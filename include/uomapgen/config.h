/*
 * config.h - central configuration for uomapgen.
 *
 * A single mapgen_config struct is populated from (in order of increasing
 * precedence): built-in defaults -> config file (--config) -> CLI flags.
 * There is deliberately NO environment-variable input anywhere.
 */
#ifndef UOMAPGEN_CONFIG_H
#define UOMAPGEN_CONFIG_H

#include <stdint.h>
#include <limits.h>

#ifndef UOMG_PATH_MAX
#define UOMG_PATH_MAX 4096
#endif

typedef struct {
    uint64_t seed;          /* master seed; byte-identical output per seed */
    int      map_index;     /* facet / fileIndex N -> mapN.mul etc. */
    int      width;         /* tiles, multiple of 8 */
    int      height;        /* tiles, multiple of 8 */
    double   sea_level;     /* elevation noise threshold in [-1,1] for water */
    double   frequency;     /* base noise frequency */
    int      octaves;       /* fbm octaves */
    int      max_slope;     /* max |z| step between adjacent land tiles */
    int      land_z_max;    /* highest land z produced from elevation */
    int      water_z;       /* flat z assigned to water cells */
    int      continent;     /* bool: apply radial falloff -> single central continent */
    double   continent_radius;   /* [0,1): solid-land core radius before falloff */
    double   continent_strength; /* how hard edges are pushed to ocean (>0) */
    double   continent_power;    /* falloff curvature (>0) */
    int      continents;    /* bool: multiple continents via placed centers */
    int      continent_count;    /* number of continents when continents=1 */
    double   continent_scale;    /* frequency of the coastline-warp noise */
    int      flat;          /* bool: level ground everywhere (land z = flat_z) */
    int      flat_z;             /* the single z used for all land when flat */
    int      mountains;     /* bool: add ridged mountain ranges */
    double   mountain_level;     /* ridge threshold in [0,1]; higher = less rock */
    int      mountain_z;         /* extra z added at mountain peaks */
    double   mountain_scale;     /* mountain ridge frequency (0 = auto from frequency) */
    int      rivers;        /* bool: carve downhill rivers from high ground */
    int      river_density;      /* source count (0 = auto from map size) */
    int      biomes;        /* bool: climate-band biomes (snow/desert/jungle/swamp) */
    double   temperature_bias;   /* shift climate warmer(+)/colder(-) */
    int      vegetation;    /* bool: place tree/rock/plant statics */
    double   tree_density;       /* [0,1] fraction of eligible cells with a tree */
    double   rock_density;       /* [0,1] rocks/boulders on hills/mountains */
    double   plant_density;      /* [0,1] ground cover (plants/flowers/ferns) */
    int      beaches;       /* bool: sloped sand beaches around all coasts */
    int      beach_width;        /* beach band width in tiles */
    int      lakes;         /* bool: form lakes at inland river sinks */
    int      passes;        /* bool: carve walkable passes through mountains */
    int      emit_mapdef;   /* bool: also write map-definitions.snippet.json */
    int      terrain_only;  /* bool: write only mapN.mul (skip statics) */
    char     out_dir[UOMG_PATH_MAX];
    char     tiledata_path[UOMG_PATH_MAX];
    char     preview_path[UOMG_PATH_MAX];  /* empty string = no preview */
    char     config_path[UOMG_PATH_MAX];   /* empty string = none */
} mapgen_config;

/* Fill cfg with built-in defaults. */
void config_defaults(mapgen_config *cfg);

/* Parse an INI-like "key = value" file into cfg (keys mirror CLI long opts
 * with '-' replaced by '_'). Returns 0 on success, -1 on error (message to
 * stderr). Missing file when path is empty is a no-op success. */
int config_load_file(mapgen_config *cfg, const char *path);

/* Apply a named preset ("test", "felucca"). Returns 0 or -1 if unknown. */
int config_apply_preset(mapgen_config *cfg, const char *name);

/* Apply a single key/value pair (keys mirror CLI long options, '-' or '_').
 * Used by the config-file parser. Returns 0 on success, -1 on error. */
int config_set_kv(mapgen_config *cfg, const char *key, const char *val);

/* Validate ranges (dims multiple of 8 and > 0, octaves > 0, etc.).
 * Returns 0 on success, -1 on error (message to stderr). */
int config_validate(const mapgen_config *cfg);

#endif /* UOMAPGEN_CONFIG_H */
