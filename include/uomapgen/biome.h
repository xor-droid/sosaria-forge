/*
 * biome.h - climate-band biome classification + per-biome land tile selection.
 *
 * Temperature is driven by latitude (poles cold, equator hot), modulated by a
 * low-frequency noise and reduced with elevation; moisture by a noise field.
 * The result is a TCAT_* category (see terrain.h) used both for the land tile
 * and for the vegetation set.
 */
#ifndef UOMAPGEN_BIOME_H
#define UOMAPGEN_BIOME_H

#include <stdint.h>

/* Classify a LAND cell (not water/mountain/hill/beach) into a biome TCAT_*.
 *   h01: normalized land height in [0,1]
 *   temperature: ~[-1,1] cold..hot (already includes latitude/elevation/bias)
 *   moisture: ~[-1,1] dry..wet
 * Returns one of TCAT_SNOW, TCAT_DESERT, TCAT_JUNGLE, TCAT_SWAMP, TCAT_FOREST,
 * TCAT_GRASS. */
int biome_classify(double h01, double temperature, double moisture);

/* Pick a land tile id for a category, lightly varied by a per-cell hash so
 * fields of the same biome aren't a single flat tile. Handles every TCAT_*
 * (water, river, lake, beach, mountain, and the biomes). */
uint16_t biome_tile(int cat, uint64_t hash);

#endif /* UOMAPGEN_BIOME_H */
