#include "uomapgen/terrain.h"
#include "uomapgen/noise.h"
#include "uomapgen/biome.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Deterministic per-cell hash (for tile variation). */
static uint64_t cell_hash(uint64_t seed, int x, int y, uint32_t salt) {
    uint64_t s = seed
               ^ (0x100000001B3ULL * (uint64_t)(uint32_t)x)
               ^ (0xC2B2AE3D27D4EB4FULL * (uint64_t)(uint32_t)y)
               ^ ((uint64_t)salt << 48);
    return noise_splitmix64(&s);
}

/*
 * Centralized land-tile palette (classic UO land tile IDs, verified against the
 * reference client's tiledata.mul). Keep them here so they are easy to tune.
 */
#define TILE_WATER_DEEP     0x00A8  /* ocean (Wet) */
#define TILE_WATER_SHALLOW  0x00AB  /* ocean variant */
#define TILE_RIVER          0x00A8  /* rivers rendered with ocean water */
#define TILE_SAND           0x0016  /* coast sand */
#define TILE_GRASS          0x0003  /* grass */
#define TILE_FOREST         0x00C4  /* forest */
#define TILE_HILL           0x0071  /* dirt / high ground */
#define TILE_MOUNTAIN       0x00E4  /* rock (impassable -> true mountain barrier) */

static uint16_t tile_for_cat(int cat) {
    switch (cat) {
        case TCAT_WATER_DEEP:    return TILE_WATER_DEEP;
        case TCAT_WATER_SHALLOW: return TILE_WATER_SHALLOW;
        case TCAT_RIVER:         return TILE_RIVER;
        case TCAT_SAND:          return TILE_SAND;
        case TCAT_GRASS:         return TILE_GRASS;
        case TCAT_FOREST:        return TILE_FOREST;
        case TCAT_HILL:          return TILE_HILL;
        case TCAT_MOUNTAIN:      return TILE_MOUNTAIN;
        default:                 return TILE_GRASS;
    }
}

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

#define IS_WATER_CAT(c) ((c) == TCAT_WATER_DEEP || (c) == TCAT_WATER_SHALLOW || \
                         (c) == TCAT_RIVER || (c) == TCAT_LAKE)
#define IS_OCEAN_CAT(c) ((c) == TCAT_WATER_DEEP || (c) == TCAT_WATER_SHALLOW)
#define IS_FIXED_CAT(c) (IS_WATER_CAT(c) || (c) == TCAT_MOUNTAIN)

/* Slope-limiting relaxation so adjacent WALKABLE land cells never differ by
 * more than cfg->max_slope in z. Water, rivers and mountains are left as-is
 * (mountains stay steep, water/rivers stay flat). Two fixed-order passes keep
 * this fully deterministic. */
static void limit_slope(terrain_grid *g, const mapgen_config *cfg) {
    const int W = g->width, H = g->height;
    const int step = cfg->max_slope < 1 ? 1 : cfg->max_slope;

    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 0) {
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x) {
                    int i = x + y * W;
                    if (IS_FIXED_CAT(g->cat[i])) continue;
                    int zi = g->z[i];
                    if (x > 0) { int n = g->z[i - 1]; if (zi > n + step) zi = n + step; }
                    if (y > 0) { int n = g->z[i - W]; if (zi > n + step) zi = n + step; }
                    g->z[i] = (int8_t)zi;
                }
        } else {
            for (int y = H - 1; y >= 0; --y)
                for (int x = W - 1; x >= 0; --x) {
                    int i = x + y * W;
                    if (IS_FIXED_CAT(g->cat[i])) continue;
                    int zi = g->z[i];
                    if (x < W - 1) { int n = g->z[i + 1]; if (zi > n + step) zi = n + step; }
                    if (y < H - 1) { int n = g->z[i + W]; if (zi > n + step) zi = n + step; }
                    g->z[i] = (int8_t)zi;
                }
        }
    }
}

static void validate_palette(const tiledata_land *td) {
    if (!td->loaded)
        return;
    if (!tiledata_is_wet(td, TILE_WATER_DEEP))
        fprintf(stderr, "warning: water tile 0x%04X is not flagged Wet in tiledata\n", TILE_WATER_DEEP);
    /* Note: TILE_MOUNTAIN (rock) is intentionally Impassable. */
    const uint16_t land[] = { TILE_SAND, TILE_GRASS, TILE_FOREST, TILE_HILL };
    for (size_t k = 0; k < sizeof(land) / sizeof(land[0]); ++k)
        if (tiledata_is_impassable(td, land[k]))
            fprintf(stderr, "warning: land tile 0x%04X is flagged Impassable in tiledata\n", land[k]);
}

/* ------------------------------------------------------------------------- */

typedef struct { double x, y; } vec2;

/* True if any 8-neighbour of (x,y) is a mountain cell. */
static int adjacent_to_mountain(const terrain_grid *g, int x, int y) {
    const int W = g->width, H = g->height;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            if (!dx && !dy) continue;
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            if (g->cat[nx + ny * W] == TCAT_MOUNTAIN) return 1;
        }
    return 0;
}

/* Deterministically place continent_count centers on an ellipse inscribed in
 * the map (a triangle for n=3), seed-rotated. This spreads them in 2D so they
 * stay well separated with ocean between, rather than merging along one axis.
 * n==1 is placed at the center. */
static void place_centers(const mapgen_config *cfg, vec2 *c, int n) {
    uint64_t s = cfg->seed ^ 0xC0FFEE123456789ULL;
    double phase = (double)(noise_splitmix64(&s) >> 11) / 9007199254740992.0 * 6.2831853;
    double cx = cfg->width * 0.5, cy = cfg->height * 0.5;
    if (n == 1) { c[0].x = cx; c[0].y = cy; return; }
    double a = cfg->width * 0.26, b = cfg->height * 0.27;
    for (int k = 0; k < n; ++k) {
        double ang = phase + k * (6.2831853 / (double)n);
        c[k].x = cx + a * cos(ang);
        c[k].y = cy + b * sin(ang);
    }
}

/*
 * River carving: pick high-ground sources spread across a coarse grid, then
 * trace each one strictly downhill over the height field, marking a ~3-wide
 * water channel until it reaches the sea (or a local pit). Rivers merge where
 * paths meet, giving a dendritic network. Deterministic throughout.
 */
static void carve_rivers(terrain_grid *g, const mapgen_config *cfg,
                         const float *hf) {
    const int W = g->width, H = g->height;
    const int S = 96;                 /* coarse source-grid cell size */
    const int gx = (W + S - 1) / S, gy = (H + S - 1) / S;

    /* Meander steering: a smooth noise field rotates each downhill step so the
     * river wanders laterally instead of running straight down the slope. */
    noise_layer *mnd = noise_layer_create(cfg->seed, NOISE_LAYER_MEANDER, 0.012, 2);
    const double MEANDER_MAX = 1.15;  /* max angular deflection (radians) */

    /* Ford anchors: one crossing every FORD_STEP cells along each river path.
     * Each becomes a small compact land bridge (not a long sandbar). */
    const int FORD_STEP = 120;
    int *fordAnchors = NULL; int nFord = 0, capFord = 0;
    int *lakeAnchors = NULL; int nLake = 0, capLake = 0;  /* river sinks -> ponds */

    /* Collect one highest candidate per coarse cell (mountain if available). */
    typedef struct { float h; int idx; } cand;
    cand *cs = (cand *)malloc((size_t)gx * gy * sizeof(cand));
    if (!cs) { noise_layer_free(mnd); return; }
    int nc = 0;
    for (int cy = 0; cy < gy; ++cy)
        for (int cx = 0; cx < gx; ++cx) {
            /* Rivers never originate in or run through mountains. Prefer a
             * "spring" at a mountain's foot (highest non-mountain cell that
             * touches a range); otherwise fall back to the highest high-ground
             * cell in this grid cell. */
            float best = -1e30f;  int bi = -1;
            float bestF = -1e30f; int biF = -1;
            for (int y = cy * S; y < (cy + 1) * S && y < H; ++y)
                for (int x = cx * S; x < (cx + 1) * S && x < W; ++x) {
                    int i = x + y * W;
                    if (IS_WATER_CAT(g->cat[i]) || g->cat[i] == TCAT_MOUNTAIN)
                        continue;
                    if (hf[i] > best) { best = hf[i]; bi = i; }
                    if (adjacent_to_mountain(g, x, y) && hf[i] > bestF) {
                        bestF = hf[i]; biF = i;
                    }
                }
            if (biF >= 0)
                cs[nc++] = (cand){ bestF, biF };      /* mountain-fed spring */
            else if (bi >= 0 && best > 0.55f)
                cs[nc++] = (cand){ best, bi };         /* high-ground source */
        }

    /* Sort candidates by height descending (simple insertion on the modest
     * count; deterministic). */
    for (int a = 1; a < nc; ++a) {
        cand key = cs[a]; int b = a - 1;
        while (b >= 0 && cs[b].h < key.h) { cs[b + 1] = cs[b]; --b; }
        cs[b + 1] = key;
    }

    int cap = cfg->river_density > 0 ? cfg->river_density : (W + H) / 400;
    if (cap > nc) cap = nc;

    long carved = 0, reached = 0;
    for (int s = 0; s < cap; ++s) {
        int cur = cs[s].idx;
        int got_sea = 0;
        int prevx = cur % W, prevy = cur / W;
        for (int step = 0; step < W + H; ++step) {
            int cx = cur % W, cy = cur / W;
            if (IS_WATER_CAT(g->cat[cur]) && g->cat[cur] != TCAT_RIVER) {
                got_sea = 1;
                break;                     /* reached the sea */
            }
            /* carve this cell */
            if (g->cat[cur] != TCAT_RIVER) {
                g->cat[cur] = TCAT_RIVER;
                g->id[cur]  = TILE_RIVER;
                g->z[cur]   = (int8_t)clampi(g->z[cur] - 2, -128, 127);
                ++carved;
            }
            /* record a ford anchor every FORD_STEP cells along the path */
            if (step > 0 && step % FORD_STEP == 0) {
                if (nFord == capFord) {
                    int nc2 = capFord ? capFord * 2 : 256;
                    int *tmp = (int *)realloc(fordAnchors, (size_t)nc2 * sizeof(int));
                    if (tmp) { fordAnchors = tmp; capFord = nc2; }
                }
                if (nFord < capFord) fordAnchors[nFord++] = cur;
            }
            /* find the lowest neighbor (8-dir) by routing height: this gives the
             * natural flow direction and detects pits. */
            int best = -1; float bh = 1e30f;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy) continue;
                    int nx = cx + dx, ny = cy + dy;
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    int ni = nx + ny * W;
                    if (g->cat[ni] == TCAT_MOUNTAIN) continue; /* never flow into a range */
                    if (hf[ni] < bh) { bh = hf[ni]; best = ni; }
                }
            if (best < 0) break;
            /* If the lowest reachable neighbor is not lower (beyond a small
             * tolerance for coastline-warp dips), the river ends in a sink: it
             * stops here and (if lakes are on) this spot becomes a pond. */
            if (bh > hf[cur] + 0.01f) {
                if (cfg->lakes) {
                    if (nLake == capLake) {
                        int nc2 = capLake ? capLake * 2 : 64;
                        int *tmp = (int *)realloc(lakeAnchors, (size_t)nc2 * sizeof(int));
                        if (tmp) { lakeAnchors = tmp; capLake = nc2; }
                    }
                    if (nLake < capLake) lakeAnchors[nLake++] = cur;
                }
                break;
            }

            /* Meander: rotate the steepest-descent direction by a smooth
             * noise-driven angle, then flow to the downhill neighbour best
             * aligned with that heading. Every step still descends (within
             * tolerance), so rivers keep reaching the sea while winding. */
            double baseAng = atan2((double)(best / W - cy), (double)(best % W - cx));
            double ang = baseAng + MEANDER_MAX * noise_layer_sample(mnd, cx, cy);
            double dxw = cos(ang), dyw = sin(ang);
            int chosen = best; double bestScore = -1e30;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy) continue;
                    int nx2 = cx + dx, ny2 = cy + dy;
                    if (nx2 < 0 || ny2 < 0 || nx2 >= W || ny2 >= H) continue;
                    int ni = nx2 + ny2 * W;
                    if (g->cat[ni] == TCAT_MOUNTAIN) continue;
                    if (hf[ni] > hf[cur] + 0.01f) continue;   /* keep descending */
                    double len = (dx && dy) ? 0.70710678 : 1.0;
                    double score = ((double)dx * dxw + (double)dy * dyw) * len;
                    if (score > bestScore) { bestScore = score; chosen = ni; }
                }

            if (g->cat[chosen] == TCAT_RIVER) break;  /* merged into another river */
            /* widen: mark the two cells perpendicular to flow as river banks */
            int nx = chosen % W, ny = chosen / W;
            int ddx = nx - cx, ddy = ny - cy;
            int px = -ddy, py = ddx;       /* perpendicular */
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                int wx = cx + px * sgn, wy = cy + py * sgn;
                if (wx < 0 || wy < 0 || wx >= W || wy >= H) continue;
                int wi = wx + wy * W;
                if (!IS_WATER_CAT(g->cat[wi]) && g->cat[wi] != TCAT_MOUNTAIN) {
                    g->cat[wi] = TCAT_RIVER;
                    g->id[wi]  = TILE_RIVER;
                    g->z[wi]   = (int8_t)clampi(g->z[wi] - 2, -128, 127);
                }
            }
            prevx = cx; prevy = cy; (void)prevx; (void)prevy;
            cur = chosen;
        }
        if (got_sea) ++reached;
    }

    /* Widen rivers by one cell (4-neighbour dilation) so channels read clearly
     * and look like real rivers. Uses a sentinel so it stays a single ring and
     * does not cascade within the pass. */
    const uint8_t PEND = 250;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            int i = x + y * W;
            if (IS_WATER_CAT(g->cat[i]) || g->cat[i] == TCAT_MOUNTAIN) continue;
            int near = 0;
            if (x > 0     && g->cat[i - 1] == TCAT_RIVER) near = 1;
            else if (x < W-1 && g->cat[i + 1] == TCAT_RIVER) near = 1;
            else if (y > 0     && g->cat[i - W] == TCAT_RIVER) near = 1;
            else if (y < H-1 && g->cat[i + W] == TCAT_RIVER) near = 1;
            if (near) g->cat[i] = PEND;
        }
    for (size_t i = 0; i < (size_t)W * H; ++i)
        if (g->cat[i] == PEND) {
            g->cat[i] = TCAT_RIVER;
            g->id[i]  = TILE_RIVER;
            g->z[i]   = (int8_t)clampi(g->z[i] - 1, -128, 127);
        }

    /* Fords: a compact crossing at each anchor so no area is landlocked by a
     * river, without long sandbars or bridges to nowhere. Each ford floods the
     * connected river cells within a small radius back to passable land (so it
     * spans the channel bank-to-bank and nothing more). Anchors within a few
     * cells of the open sea are skipped, so there are no sandbars at river
     * mouths. */
    const int FORD_RADIUS = 3;
    long fords = 0, fordsPlaced = 0;
    int q[512], qd[512];
    for (int a = 0; a < nFord; ++a) {
        int start = fordAnchors[a];
        if (g->cat[start] != TCAT_RIVER) continue;   /* already forded/merged */

        /* skip anchors near open ocean (would make a sandbar at the mouth) */
        int sx = start % W, sy = start / W, nearSea = 0;
        for (int dy = -FORD_RADIUS - 1; dy <= FORD_RADIUS + 1 && !nearSea; ++dy)
            for (int dx = -FORD_RADIUS - 1; dx <= FORD_RADIUS + 1; ++dx) {
                int nx = sx + dx, ny = sy + dy;
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                int c = g->cat[nx + ny * W];
                if (c == TCAT_WATER_DEEP || c == TCAT_WATER_SHALLOW) { nearSea = 1; break; }
            }
        if (nearSea) continue;

        int qh = 0, qt = 0;
        q[qt] = start; qd[qt] = 0; qt++;
        while (qh < qt) {
            int c = q[qh]; int d = qd[qh]; qh++;
            if (g->cat[c] != TCAT_RIVER) continue;
            int cxc = c % W, cyc = c / W;
            int bz = g->z[c] + 2; int found = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy) continue;
                    int nx = cxc + dx, ny = cyc + dy;
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    int ni = nx + ny * W;
                    if (!IS_WATER_CAT(g->cat[ni]) && g->cat[ni] != TCAT_MOUNTAIN) {
                        if (!found || g->z[ni] > bz) { bz = g->z[ni]; found = 1; }
                    }
                }
            g->cat[c] = TCAT_SAND;
            g->id[c]  = tile_for_cat(TCAT_SAND);
            g->z[c]   = (int8_t)clampi(bz, -128, 127);
            ++fords;
            if (d < FORD_RADIUS) {
                int nb[4] = { c - 1, c + 1, c - W, c + W };
                for (int k = 0; k < 4; ++k) {
                    int n = nb[k];
                    if (n < 0 || n >= W * H) continue;
                    if (g->cat[n] == TCAT_RIVER && qt < 512) { q[qt] = n; qd[qt] = d + 1; qt++; }
                }
            }
        }
        ++fordsPlaced;
    }
    free(fordAnchors);

    /* Lakes: flood a small pond at each inland river sink (not near ocean), so
     * rivers that don't reach the sea feed a lake instead of just stopping. */
    long lakesPlaced = 0;
    for (int a = 0; a < nLake; ++a) {
        int start = lakeAnchors[a];
        if (IS_OCEAN_CAT(g->cat[start])) continue;
        int sx = start % W, sy = start / W, nearSea = 0;
        for (int dy = -4; dy <= 4 && !nearSea; ++dy)
            for (int dx = -4; dx <= 4; ++dx) {
                int nx = sx + dx, ny = sy + dy;
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                if (IS_OCEAN_CAT(g->cat[nx + ny * W])) { nearSea = 1; break; }
            }
        if (nearSea) continue;
        int lz = g->z[start];
        int qh = 0, qt = 0;
        q[qt] = start; qd[qt] = 0; qt++;
        while (qh < qt) {
            int c = q[qh]; int d = qd[qh]; qh++;
            if (g->cat[c] == TCAT_MOUNTAIN || IS_OCEAN_CAT(g->cat[c])) continue;
            g->cat[c] = TCAT_LAKE;
            g->id[c]  = biome_tile(TCAT_LAKE, cell_hash(cfg->seed, c % W, c / W, NOISE_LAYER_BIOME));
            g->z[c]   = (int8_t)clampi(lz, -128, 127);
            ++lakesPlaced;
            if (d < 2) {
                int nb[4] = { c - 1, c + 1, c - W, c + W };
                for (int k = 0; k < 4; ++k) {
                    int nn = nb[k];
                    if (nn < 0 || nn >= W * H) continue;
                    int nc3 = g->cat[nn];
                    if (nc3 != TCAT_MOUNTAIN && !IS_OCEAN_CAT(nc3) && nc3 != TCAT_LAKE
                        && qt < 512) { q[qt] = nn; qd[qt] = d + 1; qt++; }
                }
            }
        }
    }
    free(lakeAnchors);

    fprintf(stderr, "rivers: %d sources, %ld cells carved, %ld reached the sea, %ld fords (%ld cells), %ld lakes\n",
            cap, carved, reached, fordsPlaced, fords, lakesPlaced);
    noise_layer_free(mnd);
    free(cs);
}

/* Sloped sand beaches around every ocean coast: land within beach_width of the
 * sea becomes sand, with z ramped from the shore (water_z) up to the inland
 * height, so coasts slope instead of cliffing. Rivers/lakes/mountains excluded. */
static void beach_pass(terrain_grid *g, const mapgen_config *cfg) {
    const int W = g->width, H = g->height;
    const size_t n = (size_t)W * (size_t)H;
    const int bw = cfg->beach_width;
    if (bw < 1) return;
    uint8_t *dist = (uint8_t *)malloc(n);
    if (!dist) return;
    for (size_t i = 0; i < n; ++i) dist[i] = IS_OCEAN_CAT(g->cat[i]) ? 0 : 255;

    for (int d = 1; d <= bw; ++d)
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                int i = x + y * W;
                if (dist[i] != 255) continue;
                int c = g->cat[i];
                if (c == TCAT_MOUNTAIN || IS_WATER_CAT(c)) continue;
                int adj = (x > 0 && dist[i-1] == d-1) || (x < W-1 && dist[i+1] == d-1)
                       || (y > 0 && dist[i-W] == d-1) || (y < H-1 && dist[i+W] == d-1);
                if (!adj) continue;
                dist[i] = (uint8_t)d;
                double f = (double)d / (double)bw;      /* shore .. inland */
                int zl = g->z[i];
                int zb = cfg->water_z + (int)lround((zl - cfg->water_z) * f);
                g->cat[i] = TCAT_SAND;
                g->id[i]  = biome_tile(TCAT_SAND, cell_hash(cfg->seed, x, y, NOISE_LAYER_BIOME));
                g->z[i]   = (int8_t)clampi(zb, -128, 127);
            }
    free(dist);
}

static int is_walkable_land(const terrain_grid *g, int i) {
    int c = g->cat[i];
    return !(IS_WATER_CAT(c) || c == TCAT_MOUNTAIN);
}

/* Mountain passes: carve ~3-wide walkable corridors (dirt) through mountain
 * bands up to MAXTHICK thick at regular intervals along both axes, so interior
 * valleys aren't sealed off. Heuristic but bounded and deterministic. */
static void carve_passes(terrain_grid *g, const mapgen_config *cfg) {
    const int W = g->width, H = g->height;
    const int SPACING = 160, MAXTHICK = 48;

    for (int y = SPACING / 2; y < H; y += SPACING) {
        int x = 0;
        while (x < W) {
            int i = x + y * W;
            if (g->cat[i] == TCAT_MOUNTAIN && x > 0 && is_walkable_land(g, i - 1)) {
                int zL = g->z[i - 1], e = x;
                while (e < W && g->cat[e + y * W] == TCAT_MOUNTAIN && (e - x) < MAXTHICK) ++e;
                if (e < W && e > x && is_walkable_land(g, e + y * W)) {
                    int zR = g->z[e + y * W], len = e - x;
                    for (int p = x; p < e; ++p) {
                        double f = (double)(p - x + 1) / (double)(len + 1);
                        int zz = zL + (int)lround((zR - zL) * f);
                        for (int yy = y - 1; yy <= y + 1; ++yy) {
                            if (yy < 0 || yy >= H) continue;
                            int j = p + yy * W;
                            if (g->cat[j] == TCAT_MOUNTAIN) {
                                g->cat[j] = TCAT_HILL;
                                g->id[j]  = biome_tile(TCAT_HILL, cell_hash(cfg->seed, p, yy, NOISE_LAYER_BIOME));
                                g->z[j]   = (int8_t)clampi(zz, -128, 127);
                            }
                        }
                    }
                }
                x = e > x ? e : x + 1;
                continue;
            }
            ++x;
        }
    }
    for (int x = SPACING / 2; x < W; x += SPACING) {
        int y = 0;
        while (y < H) {
            int i = x + y * W;
            if (g->cat[i] == TCAT_MOUNTAIN && y > 0 && is_walkable_land(g, i - W)) {
                int zT = g->z[i - W], e = y;
                while (e < H && g->cat[x + e * W] == TCAT_MOUNTAIN && (e - y) < MAXTHICK) ++e;
                if (e < H && e > y && is_walkable_land(g, x + e * W)) {
                    int zB = g->z[x + e * W], len = e - y;
                    for (int p = y; p < e; ++p) {
                        double f = (double)(p - y + 1) / (double)(len + 1);
                        int zz = zT + (int)lround((zB - zT) * f);
                        for (int xx = x - 1; xx <= x + 1; ++xx) {
                            if (xx < 0 || xx >= W) continue;
                            int j = xx + p * W;
                            if (g->cat[j] == TCAT_MOUNTAIN) {
                                g->cat[j] = TCAT_HILL;
                                g->id[j]  = biome_tile(TCAT_HILL, cell_hash(cfg->seed, xx, p, NOISE_LAYER_BIOME));
                                g->z[j]   = (int8_t)clampi(zz, -128, 127);
                            }
                        }
                    }
                }
                y = e > y ? e : y + 1;
                continue;
            }
            ++y;
        }
    }
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
    float *hf = (float *)malloc(n * sizeof(float));
    if (!g->id || !g->z || !g->cat || !hf) {
        free(hf); terrain_free(g); return -1;
    }

    noise_layer *elev = noise_layer_create(cfg->seed, NOISE_LAYER_ELEVATION,
                                            cfg->frequency, cfg->octaves);
    noise_layer *moist = noise_layer_create(cfg->seed, NOISE_LAYER_MOISTURE,
                                             cfg->frequency * 2.0,
                                             cfg->octaves > 2 ? cfg->octaves - 1 : cfg->octaves);
    noise_layer *cont = NULL, *mtn = NULL, *temp = NULL;
    if (cfg->biomes)
        temp = noise_layer_create(cfg->seed, NOISE_LAYER_TEMPERATURE,
                                  cfg->frequency * 0.6, 3);
    if (cfg->continents)
        cont = noise_layer_create(cfg->seed, NOISE_LAYER_CONTINENT,
                                  cfg->continent_scale, cfg->octaves > 4 ? 4 : cfg->octaves);
    if (cfg->mountains) {
        /* Lower frequency + fewer octaves => a few broad, distinct ranges
         * rather than a fine web of ridges across the whole interior.
         * --mountain-scale overrides the frequency (smaller = bigger ranges). */
        double mfreq = cfg->mountain_scale > 0.0 ? cfg->mountain_scale
                                                 : cfg->frequency * 0.5;
        mtn = noise_layer_create_ridged(cfg->seed, NOISE_LAYER_DETAIL, mfreq,
                                        cfg->octaves > 4 ? 4 : cfg->octaves);
    }
    if (!elev || !moist || (cfg->continents && !cont) || (cfg->mountains && !mtn)
        || (cfg->biomes && !temp)) {
        noise_layer_free(elev); noise_layer_free(moist);
        noise_layer_free(cont); noise_layer_free(mtn); noise_layer_free(temp);
        free(hf); terrain_free(g); return -1;
    }

    validate_palette(td);

    const double sea = cfg->sea_level;
    const int land_zmax = cfg->land_z_max;
    const double inv_halfw = (W > 1) ? 2.0 / (double)(W - 1) : 0.0;
    const double inv_halfh = (H > 1) ? 2.0 / (double)(H - 1) : 0.0;

    /* Continent centers + base radius (for the structured multi-continent mode). */
    int ncen = cfg->continent_count;
    vec2 centers[64];
    double R = 1.0;
    if (cfg->continents) {
        if (ncen > 64) ncen = 64;
        place_centers(cfg, centers, ncen);
        /* Radius from the minimum pairwise distance so continents stay
         * separated by ocean (even at max coastline warp). */
        double minpair = (double)(W < H ? W : H);
        for (int a = 0; a < ncen; ++a)
            for (int b = a + 1; b < ncen; ++b) {
                double dx = centers[a].x - centers[b].x;
                double dy = centers[a].y - centers[b].y;
                double d = sqrt(dx * dx + dy * dy);
                if (d < minpair) minpair = d;
            }
        R = 0.40 * minpair;
    }

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            size_t i = (size_t)x + (size_t)y * (size_t)W;
            double e_detail = noise_layer_sample(elev, x, y);  /* ~[-1,1] */
            double e;       /* landness: >sea => land */
            double hbase;   /* [0,1] macro land height (interior high, coast low) */

            if (cfg->continents) {
                /* Nearest continent center's warped radial field. */
                double warp = noise_layer_sample(cont, x, y);   /* [-1,1] */
                double cf = -1e30;
                for (int k = 0; k < ncen; ++k) {
                    double dx = (double)x - centers[k].x;
                    double dy = (double)y - centers[k].y;
                    double dist = sqrt(dx * dx + dy * dy);
                    double reach = R * (1.0 + 0.28 * warp);
                    double v = (reach - dist) / R;             /* >0 inside */
                    if (v > cf) cf = v;
                }
                e = cf + 0.15 * e_detail;
                /* Ocean margin: force the outer ring of the map to sea so no
                 * continent runs off the edge and ends abruptly. */
                double nxe = (double)x * inv_halfw - 1.0;
                double nye = (double)y * inv_halfh - 1.0;
                double em = fabs(nxe) > fabs(nye) ? fabs(nxe) : fabs(nye);
                double et = (em - 0.80) / (1.0 - 0.80);
                if (et < 0.0) et = 0.0;
                e -= 3.0 * et * et;
                hbase = cf;
            } else if (cfg->continent) {
                e = e_detail;
                double nx = (double)x * inv_halfw - 1.0;
                double ny = (double)y * inv_halfh - 1.0;
                double d = sqrt(nx * nx + ny * ny);
                if (d > 1.0) d = 1.0;
                double r0 = cfg->continent_radius;
                double t = (d - r0) / (1.0 - r0);
                if (t < 0.0) t = 0.0;
                e -= cfg->continent_strength * pow(t, cfg->continent_power);
                hbase = (e - sea);
            } else {
                e = e_detail;
                hbase = (e - sea);
            }

            int cat; int z; double height;

            if (e < sea) {
                cat = (e < sea - 0.15) ? TCAT_WATER_DEEP : TCAT_WATER_SHALLOW;
                z = cfg->water_z;
                height = -1.0e9f;            /* sea = strong sink for river routing */
            } else {
                double h = hbase;             /* roughly [0,1] inland */
                if (h < 0.0) h = 0.0;
                if (h > 1.0) h = 1.0;
                /* z carries fine detail for micro-relief... */
                double hh = 0.65 * h + 0.35 * (e_detail * 0.5 + 0.5);
                if (hh < 0.0) hh = 0.0;
                if (hh > 1.0) hh = 1.0;
                z = (int)lround(hh * (double)land_zmax);

                /* ...but the river ROUTING height is the SMOOTH macro slope
                 * (monotonic toward the coast) so downhill tracing is not
                 * trapped by detail-noise pits. */
                double route = h;

                /* Mountains: ridged ridges on sufficiently inland/high ground. */
                int isMountain = 0;
                if (cfg->mountains && h > 0.30) {
                    double m = noise_layer_sample(mtn, x, y);  /* ridged, ~[0,1] peaks high */
                    double mn = (m + 1.0) * 0.5;               /* -> [0,1] */
                    if (mn > cfg->mountain_level) {
                        double f = (mn - cfg->mountain_level) / (1.0 - cfg->mountain_level);
                        /* Mountains rise above the ground in both modes: from
                         * flat_z in flat mode, or from the base height otherwise. */
                        int base = cfg->flat ? cfg->flat_z : z;
                        z = clampi(base + (int)lround(f * (double)cfg->mountain_z), -128, 127);
                        route += f;           /* ranges are river sources/high ground */
                        isMountain = 1;
                    }
                }

                /* Flat mode: non-mountain land sits at one level, while mountain
                 * ranges keep their elevation (set above). */
                if (cfg->flat && !isMountain)
                    z = cfg->flat_z;

                height = (float)route;

                double mo = noise_layer_sample(moist, x, y);
                if (isMountain) {
                    cat = TCAT_MOUNTAIN;
                } else if (hh > 0.72) {
                    cat = TCAT_HILL;
                } else if (cfg->biomes) {
                    /* Climate: temperature by latitude (poles cold, centre hot),
                     * modulated by noise and cooled with elevation. */
                    double ny2 = (H > 1) ? (double)y / (double)(H - 1) : 0.5;
                    double lat = 1.0 - 2.0 * fabs(ny2 - 0.5);       /* 0 poles .. 1 centre */
                    double temperature = (lat * 2.0 - 1.0) * 0.65
                                       + noise_layer_sample(temp, x, y) * 0.25
                                       - hh * 0.5 + cfg->temperature_bias;
                    cat = biome_classify(hh, temperature, mo);
                } else {
                    cat = (mo > 0.1) ? TCAT_FOREST : TCAT_GRASS;
                }
            }

            uint64_t hash = cell_hash(cfg->seed, x, y, NOISE_LAYER_BIOME);
            g->cat[i] = (uint8_t)cat;
            g->id[i]  = biome_tile(cat, hash);
            g->z[i]   = (int8_t)clampi(z, -128, 127);
            hf[i]     = height;
        }
    }

    if (cfg->rivers)
        carve_rivers(g, cfg, hf);

    if (cfg->beaches)
        beach_pass(g, cfg);

    if (cfg->passes && cfg->mountains)
        carve_passes(g, cfg);

    /* Flat mode is already level; slope-limiting would only pull coastal land
     * down toward the ocean, so skip it. */
    if (!cfg->flat)
        limit_slope(g, cfg);

    noise_layer_free(elev); noise_layer_free(moist);
    noise_layer_free(cont); noise_layer_free(mtn); noise_layer_free(temp);
    free(hf);
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
