#include "uomapgen/terrain.h"
#include "uomapgen/noise.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*
 * Centralized land-tile palette. These are classic UO land tile IDs; keep them
 * here so they are easy to tune. Water id 0x00A8 was confirmed as ocean in the
 * reference client's map0.mul (block 0 is open ocean at z=-5).
 */
#define TILE_WATER_DEEP     0x00A8  /* ocean (animated, Wet) */
#define TILE_WATER_SHALLOW  0x00AB  /* ocean variant */
#define TILE_SAND           0x0016  /* coast sand */
#define TILE_GRASS          0x0003  /* grass */
#define TILE_FOREST         0x0004  /* grass variant (trees come later as statics) */
#define TILE_HILL           0x0071  /* dirt / high ground */

static uint16_t tile_for_cat(int cat) {
    switch (cat) {
        case TCAT_WATER_DEEP:    return TILE_WATER_DEEP;
        case TCAT_WATER_SHALLOW: return TILE_WATER_SHALLOW;
        case TCAT_SAND:          return TILE_SAND;
        case TCAT_GRASS:         return TILE_GRASS;
        case TCAT_FOREST:        return TILE_FOREST;
        case TCAT_HILL:          return TILE_HILL;
        default:                 return TILE_GRASS;
    }
}

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Slope-limiting relaxation so adjacent land cells never differ by more than
 * cfg->max_slope in z. Water cells are pinned (not modified). Two passes
 * (forward + reverse raster) in a fixed order keep this fully deterministic. */
static void limit_slope(terrain_grid *g, const mapgen_config *cfg) {
    const int W = g->width, H = g->height;
    const int step = cfg->max_slope < 1 ? 1 : cfg->max_slope;

    #define IS_WATER(i) (g->cat[i] == TCAT_WATER_DEEP || g->cat[i] == TCAT_WATER_SHALLOW)

    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 0) {
            for (int y = 0; y < H; ++y) {
                for (int x = 0; x < W; ++x) {
                    int i = x + y * W;
                    if (IS_WATER(i)) continue;
                    int zi = g->z[i];
                    if (x > 0)       { int n = g->z[i - 1]; if (zi > n + step) zi = n + step; }
                    if (y > 0)       { int n = g->z[i - W]; if (zi > n + step) zi = n + step; }
                    g->z[i] = (int8_t)zi;
                }
            }
        } else {
            for (int y = H - 1; y >= 0; --y) {
                for (int x = W - 1; x >= 0; --x) {
                    int i = x + y * W;
                    if (IS_WATER(i)) continue;
                    int zi = g->z[i];
                    if (x < W - 1) { int n = g->z[i + 1]; if (zi > n + step) zi = n + step; }
                    if (y < H - 1) { int n = g->z[i + W]; if (zi > n + step) zi = n + step; }
                    g->z[i] = (int8_t)zi;
                }
            }
        }
    }
    #undef IS_WATER
}

static void validate_palette(const tiledata_land *td) {
    if (!td->loaded)
        return;
    if (!tiledata_is_wet(td, TILE_WATER_DEEP))
        fprintf(stderr, "warning: water tile 0x%04X is not flagged Wet in tiledata\n", TILE_WATER_DEEP);
    const uint16_t land[] = { TILE_SAND, TILE_GRASS, TILE_FOREST, TILE_HILL };
    for (size_t k = 0; k < sizeof(land) / sizeof(land[0]); ++k)
        if (tiledata_is_impassable(td, land[k]))
            fprintf(stderr, "warning: land tile 0x%04X is flagged Impassable in tiledata\n", land[k]);
}

int terrain_generate(terrain_grid *g, const mapgen_config *cfg,
                     const tiledata_land *td) {
    const int W = cfg->width, H = cfg->height;
    const size_t n = (size_t)W * (size_t)H;

    g->width = W;
    g->height = H;
    g->id  = (uint16_t *)malloc(n * sizeof(uint16_t));
    g->z   = (int8_t  *)malloc(n * sizeof(int8_t));
    g->cat = (uint8_t *)malloc(n * sizeof(uint8_t));
    if (!g->id || !g->z || !g->cat) {
        terrain_free(g);
        return -1;
    }

    noise_layer *elev = noise_layer_create(cfg->seed, NOISE_LAYER_ELEVATION,
                                            cfg->frequency, cfg->octaves);
    noise_layer *moist = noise_layer_create(cfg->seed, NOISE_LAYER_MOISTURE,
                                             cfg->frequency * 2.0, cfg->octaves > 2 ? cfg->octaves - 1 : cfg->octaves);
    noise_layer *cont = NULL;
    if (cfg->continents) {
        /* Low-frequency mask that carves several landmasses of varying size.
         * Few octaves keeps the continents coherent while still giving a mix
         * of large masses and smaller islands. */
        int moct = cfg->octaves > 4 ? 4 : cfg->octaves;
        cont = noise_layer_create(cfg->seed, NOISE_LAYER_CONTINENT,
                                  cfg->continent_scale, moct);
    }
    if (!elev || !moist || (cfg->continents && !cont)) {
        noise_layer_free(elev);
        noise_layer_free(moist);
        noise_layer_free(cont);
        terrain_free(g);
        return -1;
    }

    validate_palette(td);

    const double sea = cfg->sea_level;
    const int land_zmax = cfg->land_z_max;

    /* Normalizer so radial distance is 0 at the center and 1.0 at the nearest
     * edge midpoint (continent fills the inscribed area; corners are ocean). */
    const double inv_halfw = (W > 1) ? 2.0 / (double)(W - 1) : 0.0;
    const double inv_halfh = (H > 1) ? 2.0 / (double)(H - 1) : 0.0;

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)x + (size_t)y * (size_t)W;
            double e_detail = noise_layer_sample(elev, x, y);  /* ~[-1,1] */
            double e;
            int cat;
            int z;

            if (cfg->continents) {
                /* Multiple continents: a low-frequency mask decides land vs sea
                 * (several masses of various sizes), detail noise roughens the
                 * coastlines, and a gentle falloff keeps the map's outer ring
                 * ocean so continents don't run off the edge. */
                double mask = noise_layer_sample(cont, x, y);
                e = 0.80 * mask + 0.20 * e_detail;
                double nx = (double)x * inv_halfw - 1.0;
                double ny = (double)y * inv_halfh - 1.0;
                double d = sqrt(nx * nx + ny * ny);
                if (d > 1.0) d = 1.0;
                double t = (d - 0.80) / (1.0 - 0.80);  /* only the outer 20% */
                if (t < 0.0) t = 0.0;
                e -= 1.6 * (t * t);
            } else if (cfg->continent) {
                /* Single central continent: a solid-land core out to
                 * continent_radius, then rising ocean pressure toward the edges.
                 * d is clamped Euclidean distance (1.0 at the nearest edge), so
                 * every edge drowns, not just the corners. */
                e = e_detail;
                double nx = (double)x * inv_halfw - 1.0;  /* [-1,1] */
                double ny = (double)y * inv_halfh - 1.0;  /* [-1,1] */
                double d = sqrt(nx * nx + ny * ny);
                if (d > 1.0) d = 1.0;
                double r0 = cfg->continent_radius;
                double t = (d - r0) / (1.0 - r0);         /* 0 at core edge, 1 at map edge */
                if (t < 0.0) t = 0.0;
                e -= cfg->continent_strength * pow(t, cfg->continent_power);
            } else {
                e = e_detail;
            }

            if (e < sea) {
                /* Water: deep vs shallow band near the coast. */
                cat = (e < sea - 0.15) ? TCAT_WATER_DEEP : TCAT_WATER_SHALLOW;
                z = cfg->water_z;
            } else {
                double h = (e - sea) / (1.0 - sea);   /* [0,1] above sea */
                if (h < 0.0) h = 0.0;
                if (h > 1.0) h = 1.0;
                z = (int)lround(h * (double)land_zmax);

                if (h < 0.04) {
                    cat = TCAT_SAND;
                } else if (h > 0.75) {
                    cat = TCAT_HILL;
                } else {
                    double m = noise_layer_sample(moist, x, y);
                    cat = (m > 0.1) ? TCAT_FOREST : TCAT_GRASS;
                }
            }

            g->cat[i] = (uint8_t)cat;
            g->id[i]  = tile_for_cat(cat);
            g->z[i]   = (int8_t)clampi(z, -128, 127);
        }
    }

    limit_slope(g, cfg);

    noise_layer_free(elev);
    noise_layer_free(moist);
    noise_layer_free(cont);
    return 0;
}

void terrain_free(terrain_grid *g) {
    if (!g)
        return;
    free(g->id);  g->id = NULL;
    free(g->z);   g->z = NULL;
    free(g->cat); g->cat = NULL;
    g->width = g->height = 0;
}
