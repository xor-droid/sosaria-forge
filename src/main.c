/*
 * uomapgen - procedural Ultima Online map generator for ModernUO.
 *
 * Pipeline: config (defaults -> file -> CLI) -> noise -> terrain grid ->
 * mapN.mul (+ empty staidxN/staticsN) [+ preview PNG] [+ mapdef snippet].
 *
 * No environment variables are consulted anywhere by design.
 */
#include "uomapgen/config.h"
#include "uomapgen/io.h"
#include "uomapgen/tiledata.h"
#include "uomapgen/terrain.h"
#include "uomapgen/mapwriter.h"
#include "uomapgen/statics.h"
#include "uomapgen/preview.h"
#include "uomapgen/mapdef.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#ifndef UOMG_VERSION
#define UOMG_VERSION "0.1.0"
#endif

/* Long-only option codes (no short equivalents). */
enum {
    OPT_CONT_RADIUS = 1000,
    OPT_CONT_STRENGTH,
    OPT_CONT_POWER,
    OPT_CONT_SCALE,
    OPT_CONT_COUNT,
    OPT_MOUNTAINS,
    OPT_MTN_LEVEL,
    OPT_MTN_Z,
    OPT_MTN_SCALE,
    OPT_FLAT,
    OPT_FLAT_Z,
    OPT_RIVERS,
    OPT_RIVER_DENSITY
};

static void print_version(void) {
    printf("uomapgen %s\n", UOMG_VERSION);
}

static void print_help(const char *argv0) {
    printf(
"uomapgen %s - procedural UO terrain generator for ModernUO\n"
"\n"
"Usage: %s [options]\n"
"\n"
"Output is byte-identical for a given build + seed/config.\n"
"Writes map<N>.mul and (unless --terrain-only) empty staidx<N>.mul and\n"
"statics<N>.mul into the output directory.\n"
"\n"
"Options:\n"
"  --seed <uint64>       Master seed (default 0). Same seed => same bytes.\n"
"  --config <file>       Read an INI-like config file first; CLI flags override it.\n"
"  --map <N>             Facet / file index -> map<N>.mul triplet (default 0).\n"
"  --width <tiles>       Map width, multiple of 8 (default 1024).\n"
"  --height <tiles>      Map height, multiple of 8 (default 1024).\n"
"  --preset <name>       'test' (1024x1024) or 'felucca' (7168x4096).\n"
"  --out <dir>           Output directory (default ./out).\n"
"  --sea-level <float>   Water threshold on elevation noise, [-1,1] (default 0.0).\n"
"  --frequency <float>   Base noise frequency (default 0.004).\n"
"  --octaves <int>       fBm octaves (default 5).\n"
"  --max-slope <int>     Max z step between adjacent land tiles (default 4).\n"
"  --land-z-max <int>    Highest land z from elevation, 0..127 (default 45).\n"
"  --water-z <int>       Flat z for water cells (default -5).\n"
"  --continent           Radial falloff: one large central landmass ringed by ocean.\n"
"  --continent-radius <f>    Solid-land core radius, [0,1) (default 0.55; implies --continent).\n"
"  --continent-strength <f>  How hard edges fall to ocean (default 2.0; implies --continent).\n"
"  --continent-power <f>     Falloff curvature (default 2.0; implies --continent).\n"
"  --continents          Multiple continents (placed centers), ocean between them.\n"
"  --continent-count <n>     Number of continents (default 3; implies --continents).\n"
"  --continent-scale <f>     Coastline-warp frequency (default 0.00045).\n"
"  --flat                Level non-mountain ground to one z (mountains keep their height).\n"
"  --flat-z <int>            The z for flat ground; mountains rise above it (default 0; implies --flat).\n"
"  --mountains           Add ridged mountain ranges (impassable rock peaks).\n"
"  --mountain-level <f>      Ridge threshold [0,1); higher = fewer/sparser ranges (default 0.70).\n"
"  --mountain-z <int>        Extra z at peaks, 0..127 (default 70).\n"
"  --mountain-scale <f>      Ridge frequency; smaller = bigger/broader ranges\n"
"                            (default: auto = frequency*0.5).\n"
"  --rivers              Carve downhill rivers from high ground to the sea.\n"
"  --river-density <n>       Number of river sources (default: auto from map size).\n"
"  --tiledata <path>     tiledata.mul used to sanity-check tile flags\n"
"                        (default ./ref/UONewDawn/tiledata.mul; optional).\n"
"  --preview <file.png>  Also render a top-down preview image.\n"
"  --emit-mapdef         Also write map-definitions.snippet.json for ModernUO.\n"
"  --terrain-only        Write only map<N>.mul (skip staidx/statics).\n"
"  --help                Show this help and exit.\n"
"  --version             Show version and exit.\n"
"\n"
"Config file (no environment variables are ever used):\n"
"  INI-like 'key = value', '#' or ';' comments. Keys mirror the long options\n"
"  with '-' or '_' (e.g. sea_level = -0.1). 'preset = felucca' is allowed.\n"
"\n"
"Examples:\n"
"  %s --seed 42 --preset test --out ./out --preview out.png --emit-mapdef\n"
"  %s --config config/example.cfg --seed 7\n"
"  %s --seed 1 --preset felucca --out /mnt/c/.../client --map 0\n"
"\n"
"ModernUO note: the generated map's width/height MUST match the entry in\n"
"Data/map-definitions.json for that map. Use --emit-mapdef to get a matching\n"
"snippet, or --preset felucca for a drop-in map0 replacement.\n",
        UOMG_VERSION, argv0, argv0, argv0, argv0);
}

/* Pre-scan argv for --config so the file loads before CLI overrides apply. */
static const char *find_config_arg(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--config") && i + 1 < argc)
            return argv[i + 1];
        if (!strncmp(argv[i], "--config=", 9))
            return argv[i] + 9;
        if (!strcmp(argv[i], "-c") && i + 1 < argc)
            return argv[i + 1];
    }
    return NULL;
}

int main(int argc, char **argv) {
    mapgen_config cfg;
    config_defaults(&cfg);

    /* Phase 1: config file (if any) before CLI, so CLI always wins. */
    const char *cfg_path = find_config_arg(argc, argv);
    if (cfg_path) {
        snprintf(cfg.config_path, sizeof(cfg.config_path), "%s", cfg_path);
        if (config_load_file(&cfg, cfg_path) != 0)
            return 2;
    }

    /* Phase 2: CLI overrides. */
    static const struct option longopts[] = {
        { "seed",        required_argument, 0, 's' },
        { "config",      required_argument, 0, 'c' },
        { "map",         required_argument, 0, 'm' },
        { "width",       required_argument, 0, 'W' },
        { "height",      required_argument, 0, 'H' },
        { "preset",      required_argument, 0, 'p' },
        { "out",         required_argument, 0, 'o' },
        { "sea-level",   required_argument, 0, 'L' },
        { "frequency",   required_argument, 0, 'f' },
        { "octaves",     required_argument, 0, 'O' },
        { "max-slope",   required_argument, 0, 'S' },
        { "land-z-max",  required_argument, 0, 'Z' },
        { "water-z",     required_argument, 0, 'w' },
        { "continent",   no_argument,       0, 'C' },
        { "continent-radius",   required_argument, 0, OPT_CONT_RADIUS },
        { "continent-strength", required_argument, 0, OPT_CONT_STRENGTH },
        { "continent-power",    required_argument, 0, OPT_CONT_POWER },
        { "continents",  no_argument,       0, 'A' },
        { "continent-scale",    required_argument, 0, OPT_CONT_SCALE },
        { "continent-count",    required_argument, 0, OPT_CONT_COUNT },
        { "mountains",   no_argument,       0, OPT_MOUNTAINS },
        { "mountain-level",     required_argument, 0, OPT_MTN_LEVEL },
        { "mountain-z",         required_argument, 0, OPT_MTN_Z },
        { "mountain-scale",     required_argument, 0, OPT_MTN_SCALE },
        { "flat",        no_argument,       0, OPT_FLAT },
        { "flat-z",      required_argument, 0, OPT_FLAT_Z },
        { "rivers",      no_argument,       0, OPT_RIVERS },
        { "river-density",      required_argument, 0, OPT_RIVER_DENSITY },
        { "tiledata",    required_argument, 0, 'T' },
        { "preview",     required_argument, 0, 'P' },
        { "emit-mapdef", no_argument,       0, 'M' },
        { "terrain-only",no_argument,       0, 't' },
        { "help",        no_argument,       0, 'h' },
        { "version",     no_argument,       0, 'v' },
        { 0, 0, 0, 0 }
    };
    const char *optstr = "s:c:m:W:H:p:o:L:f:O:S:Z:w:CAT:P:Mthv";

    int c, idx;
    while ((c = getopt_long(argc, argv, optstr, longopts, &idx)) != -1) {
        switch (c) {
            case 's': cfg.seed = strtoull(optarg, NULL, 0); break;
            case 'c': break; /* already handled in phase 1 */
            case 'm': cfg.map_index = (int)strtol(optarg, NULL, 0); break;
            case 'W': cfg.width = (int)strtol(optarg, NULL, 0); break;
            case 'H': cfg.height = (int)strtol(optarg, NULL, 0); break;
            case 'p': if (config_apply_preset(&cfg, optarg) != 0) return 2; break;
            case 'o': snprintf(cfg.out_dir, sizeof(cfg.out_dir), "%s", optarg); break;
            case 'L': cfg.sea_level = strtod(optarg, NULL); break;
            case 'f': cfg.frequency = strtod(optarg, NULL); break;
            case 'O': cfg.octaves = (int)strtol(optarg, NULL, 0); break;
            case 'S': cfg.max_slope = (int)strtol(optarg, NULL, 0); break;
            case 'Z': cfg.land_z_max = (int)strtol(optarg, NULL, 0); break;
            case 'w': cfg.water_z = (int)strtol(optarg, NULL, 0); break;
            case 'C': cfg.continent = 1; break;
            case OPT_CONT_RADIUS:   cfg.continent = 1; cfg.continent_radius = strtod(optarg, NULL); break;
            case OPT_CONT_STRENGTH: cfg.continent = 1; cfg.continent_strength = strtod(optarg, NULL); break;
            case OPT_CONT_POWER:    cfg.continent = 1; cfg.continent_power = strtod(optarg, NULL); break;
            case 'A': cfg.continents = 1; break;
            case OPT_CONT_SCALE:    cfg.continents = 1; cfg.continent_scale = strtod(optarg, NULL); break;
            case OPT_CONT_COUNT:    cfg.continents = 1; cfg.continent_count = (int)strtol(optarg, NULL, 0); break;
            case OPT_MOUNTAINS:     cfg.mountains = 1; break;
            case OPT_MTN_LEVEL:     cfg.mountains = 1; cfg.mountain_level = strtod(optarg, NULL); break;
            case OPT_MTN_Z:         cfg.mountains = 1; cfg.mountain_z = (int)strtol(optarg, NULL, 0); break;
            case OPT_MTN_SCALE:     cfg.mountains = 1; cfg.mountain_scale = strtod(optarg, NULL); break;
            case OPT_FLAT:          cfg.flat = 1; break;
            case OPT_FLAT_Z:        cfg.flat = 1; cfg.flat_z = (int)strtol(optarg, NULL, 0); break;
            case OPT_RIVERS:        cfg.rivers = 1; break;
            case OPT_RIVER_DENSITY: cfg.rivers = 1; cfg.river_density = (int)strtol(optarg, NULL, 0); break;
            case 'T': snprintf(cfg.tiledata_path, sizeof(cfg.tiledata_path), "%s", optarg); break;
            case 'P': snprintf(cfg.preview_path, sizeof(cfg.preview_path), "%s", optarg); break;
            case 'M': cfg.emit_mapdef = 1; break;
            case 't': cfg.terrain_only = 1; break;
            case 'h': print_help(argv[0]); return 0;
            case 'v': print_version(); return 0;
            default:  fprintf(stderr, "Try '%s --help'.\n", argv[0]); return 2;
        }
    }

    if (config_validate(&cfg) != 0)
        return 2;

    if (io_ensure_dir(cfg.out_dir) != 0) {
        fprintf(stderr, "error: cannot create output directory '%s'\n", cfg.out_dir);
        return 1;
    }

    const int BW = cfg.width >> 3, BH = cfg.height >> 3;
    printf("uomapgen %s: seed=%llu map=%d size=%dx%d (%dx%d blocks) out=%s\n",
           UOMG_VERSION, (unsigned long long)cfg.seed, cfg.map_index,
           cfg.width, cfg.height, BW, BH, cfg.out_dir);

    /* Optional tile-flag reference (never fails hard). */
    tiledata_land td;
    tiledata_load(&td, cfg.tiledata_path);
    if (!td.loaded)
        fprintf(stderr, "note: tiledata not loaded from '%s' (palette flags unchecked)\n",
                cfg.tiledata_path);

    terrain_grid grid;
    if (terrain_generate(&grid, &cfg, &td) != 0) {
        fprintf(stderr, "error: terrain generation failed (out of memory?)\n");
        return 1;
    }

    int rc = 0;
    if (mapwriter_write(&grid, cfg.out_dir, cfg.map_index) != 0) rc = 1;

    if (rc == 0 && !cfg.terrain_only)
        if (statics_write_empty(cfg.out_dir, cfg.map_index, BW, BH) != 0) rc = 1;

    if (rc == 0 && cfg.preview_path[0])
        if (preview_write_png(&grid, cfg.preview_path) != 0) rc = 1;

    if (rc == 0 && cfg.emit_mapdef)
        if (mapdef_write_snippet(cfg.out_dir, cfg.map_index, cfg.width, cfg.height) != 0) rc = 1;

    terrain_free(&grid);

    if (rc == 0) {
        long long map_bytes = (long long)BW * BH * 196;
        printf("done: map%d.mul = %lld bytes%s%s\n",
               cfg.map_index, map_bytes,
               cfg.terrain_only ? "" : ", staidx/statics written",
               cfg.preview_path[0] ? ", preview written" : "");
    }
    return rc;
}
