#include "uomapgen/biome.h"
#include "uomapgen/terrain.h"

/*
 * Land-tile sets per category (classic UO land tile IDs, verified against the
 * client's tiledata.mul). A per-cell hash selects within each set for subtle
 * variation. Keep these here so the look is easy to tune.
 */
static const uint16_t TILES_WATER_DEEP[]    = { 0x00A8, 0x00A9, 0x00AA, 0x00AB };
static const uint16_t TILES_WATER_SHALLOW[] = { 0x00AB, 0x00A8 };
static const uint16_t TILES_SAND[]          = { 0x0016, 0x0017, 0x0018, 0x0019 };
static const uint16_t TILES_GRASS[]         = { 0x0003, 0x0004, 0x0005, 0x0006 };
static const uint16_t TILES_FOREST[]        = { 0x00C4, 0x00C5, 0x00C6, 0x00C7 };
static const uint16_t TILES_JUNGLE[]        = { 0x00AC, 0x00AD, 0x00AE, 0x00AF };
static const uint16_t TILES_SNOW[]          = { 0x011A, 0x011B, 0x011C, 0x011D };
static const uint16_t TILES_DIRT[]          = { 0x0071, 0x0072, 0x0073, 0x0074 };
static const uint16_t TILES_MOUNTAIN[]      = { 0x00E4, 0x00E5, 0x00E6, 0x00E7 };

#define PICK(arr, h) ((arr)[(h) % (sizeof(arr) / sizeof((arr)[0]))])

int biome_classify(double h01, double temperature, double moisture) {
    /* Cold dominates -> snow (incl. high/cold ground). */
    if (temperature < -0.40)
        return TCAT_SNOW;
    /* Hot -> jungle (wet) or desert (dry). */
    if (temperature > 0.40)
        return moisture > 0.05 ? TCAT_JUNGLE : TCAT_DESERT;
    /* Temperate: low + very wet -> swamp; wet -> forest; else grass. */
    if (h01 < 0.14 && moisture > 0.35)
        return TCAT_SWAMP;
    if (moisture > 0.12)
        return TCAT_FOREST;
    return TCAT_GRASS;
}

uint16_t biome_tile(int cat, uint64_t hash) {
    switch (cat) {
        case TCAT_WATER_DEEP:    return PICK(TILES_WATER_DEEP, hash);
        case TCAT_WATER_SHALLOW: return PICK(TILES_WATER_SHALLOW, hash);
        case TCAT_RIVER:         return 0x00A8;
        case TCAT_LAKE:          return PICK(TILES_WATER_DEEP, hash);
        case TCAT_SAND:          return PICK(TILES_SAND, hash);
        case TCAT_DESERT:        return PICK(TILES_SAND, hash);
        case TCAT_GRASS:         return PICK(TILES_GRASS, hash);
        case TCAT_FOREST:        return PICK(TILES_FOREST, hash);
        case TCAT_JUNGLE:        return PICK(TILES_JUNGLE, hash);
        case TCAT_SNOW:          return PICK(TILES_SNOW, hash);
        case TCAT_SWAMP:         return PICK(TILES_DIRT, hash);   /* murky ground */
        case TCAT_HILL:          return PICK(TILES_DIRT, hash);
        case TCAT_MOUNTAIN:      return PICK(TILES_MOUNTAIN, hash);
        default:                 return PICK(TILES_GRASS, hash);
    }
}
