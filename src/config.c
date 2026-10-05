#include "uomapgen/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  /* strcasecmp */
#include <ctype.h>
#include <errno.h>

void config_defaults(mapgen_config *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->seed        = 0;
    cfg->map_index   = 0;
    cfg->width       = 1024;
    cfg->height      = 1024;
    cfg->sea_level   = 0.0;
    cfg->frequency   = 0.004;
    cfg->octaves     = 5;
    cfg->max_slope   = 4;
    cfg->land_z_max  = 45;
    cfg->water_z     = -5;
    cfg->emit_mapdef = 0;
    cfg->terrain_only = 0;
    snprintf(cfg->out_dir, sizeof(cfg->out_dir), "%s", "./out");
    snprintf(cfg->tiledata_path, sizeof(cfg->tiledata_path), "%s",
             "./ref/UONewDawn/tiledata.mul");
    cfg->preview_path[0] = '\0';
    cfg->config_path[0]  = '\0';
}

static int parse_bool(const char *v, int *out) {
    if (!strcmp(v, "1") || !strcasecmp(v, "true") || !strcasecmp(v, "yes") ||
        !strcasecmp(v, "on")) { *out = 1; return 0; }
    if (!strcmp(v, "0") || !strcasecmp(v, "false") || !strcasecmp(v, "no") ||
        !strcasecmp(v, "off")) { *out = 0; return 0; }
    return -1;
}

/* Normalize a key: lowercase and treat '-' and '_' the same (-> '_'). */
static void normalize_key(char *k) {
    for (; *k; ++k) {
        if (*k == '-') *k = '_';
        else *k = (char)tolower((unsigned char)*k);
    }
}

/* Apply one key/value pair (shared by config file and, if desired, CLI).
 * Returns 0 on success, -1 on unknown key or bad value. */
int config_set_kv(mapgen_config *cfg, const char *key_in, const char *val) {
    char key[64];
    snprintf(key, sizeof(key), "%s", key_in);
    normalize_key(key);

    errno = 0;
    if (!strcmp(key, "seed")) {
        cfg->seed = strtoull(val, NULL, 0);
    } else if (!strcmp(key, "map")) {
        cfg->map_index = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "width")) {
        cfg->width = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "height")) {
        cfg->height = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "sea_level")) {
        cfg->sea_level = strtod(val, NULL);
    } else if (!strcmp(key, "frequency")) {
        cfg->frequency = strtod(val, NULL);
    } else if (!strcmp(key, "octaves")) {
        cfg->octaves = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "max_slope")) {
        cfg->max_slope = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "land_z_max")) {
        cfg->land_z_max = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "water_z")) {
        cfg->water_z = (int)strtol(val, NULL, 0);
    } else if (!strcmp(key, "out")) {
        snprintf(cfg->out_dir, sizeof(cfg->out_dir), "%s", val);
    } else if (!strcmp(key, "tiledata")) {
        snprintf(cfg->tiledata_path, sizeof(cfg->tiledata_path), "%s", val);
    } else if (!strcmp(key, "preview")) {
        snprintf(cfg->preview_path, sizeof(cfg->preview_path), "%s", val);
    } else if (!strcmp(key, "emit_mapdef")) {
        if (parse_bool(val, &cfg->emit_mapdef) != 0) return -1;
    } else if (!strcmp(key, "terrain_only")) {
        if (parse_bool(val, &cfg->terrain_only) != 0) return -1;
    } else if (!strcmp(key, "preset")) {
        return config_apply_preset(cfg, val);
    } else {
        fprintf(stderr, "error: unknown config key '%s'\n", key_in);
        return -1;
    }
    return 0;
}

int config_load_file(mapgen_config *cfg, const char *path) {
    if (!path || path[0] == '\0')
        return 0;
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "error: cannot open config file '%s'\n", path);
        return -1;
    }

    char line[1024];
    int lineno = 0, rc = 0;
    while (fgets(line, sizeof(line), f)) {
        ++lineno;
        char *s = line;
        while (*s && isspace((unsigned char)*s)) ++s;       /* ltrim */
        if (*s == '\0' || *s == '#' || *s == ';')            /* comment/blank */
            continue;

        char *eq = strchr(s, '=');
        if (!eq) {
            fprintf(stderr, "error: %s:%d: expected key = value\n", path, lineno);
            rc = -1; break;
        }
        *eq = '\0';
        char *key = s;
        char *val = eq + 1;

        /* rtrim key */
        char *ke = key + strlen(key);
        while (ke > key && isspace((unsigned char)ke[-1])) *--ke = '\0';
        /* ltrim val */
        while (*val && isspace((unsigned char)*val)) ++val;
        /* strip an inline comment (whitespace-preceded # or ;), but not when
         * the value is quoted (a quoted path may legitimately contain them). */
        if (*val != '"' && *val != '\'') {
            for (char *p = val; *p; ++p) {
                if ((*p == '#' || *p == ';') &&
                    (p == val || isspace((unsigned char)p[-1]))) {
                    *p = '\0';
                    break;
                }
            }
        }
        /* rtrim val */
        char *ve = val + strlen(val);
        while (ve > val && isspace((unsigned char)ve[-1])) *--ve = '\0';
        /* strip surrounding quotes on value */
        size_t vlen = strlen(val);
        if (vlen >= 2 && ((val[0] == '"' && val[vlen-1] == '"') ||
                          (val[0] == '\'' && val[vlen-1] == '\''))) {
            val[vlen-1] = '\0';
            ++val;
        }

        if (config_set_kv(cfg, key, val) != 0) {
            fprintf(stderr, "       (in %s:%d)\n", path, lineno);
            rc = -1; break;
        }
    }
    fclose(f);
    return rc;
}

int config_apply_preset(mapgen_config *cfg, const char *name) {
    if (!strcasecmp(name, "test")) {
        cfg->width = 1024; cfg->height = 1024;
        return 0;
    }
    if (!strcasecmp(name, "felucca")) {
        cfg->width = 7168; cfg->height = 4096;
        return 0;
    }
    fprintf(stderr, "error: unknown preset '%s' (use 'test' or 'felucca')\n", name);
    return -1;
}

int config_validate(const mapgen_config *cfg) {
    int ok = 1;
    if (cfg->width <= 0 || cfg->width % 8 != 0) {
        fprintf(stderr, "error: width (%d) must be > 0 and a multiple of 8\n", cfg->width); ok = 0;
    }
    if (cfg->height <= 0 || cfg->height % 8 != 0) {
        fprintf(stderr, "error: height (%d) must be > 0 and a multiple of 8\n", cfg->height); ok = 0;
    }
    if (cfg->octaves <= 0) {
        fprintf(stderr, "error: octaves (%d) must be > 0\n", cfg->octaves); ok = 0;
    }
    if (cfg->map_index < 0 || cfg->map_index > 255) {
        fprintf(stderr, "error: map (%d) must be in 0..255\n", cfg->map_index); ok = 0;
    }
    if (cfg->sea_level < -1.0 || cfg->sea_level > 1.0) {
        fprintf(stderr, "error: sea-level (%g) must be in [-1,1]\n", cfg->sea_level); ok = 0;
    }
    if (cfg->land_z_max < 0 || cfg->land_z_max > 127) {
        fprintf(stderr, "error: land-z-max (%d) must be in 0..127\n", cfg->land_z_max); ok = 0;
    }
    if (cfg->water_z < -128 || cfg->water_z > 127) {
        fprintf(stderr, "error: water-z (%d) must be in -128..127\n", cfg->water_z); ok = 0;
    }
    return ok ? 0 : -1;
}
