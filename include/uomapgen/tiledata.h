/*
 * tiledata.h - reader for the land section of tiledata.mul (High Seas format).
 *
 * We use this only to validate/inspect the land-tile flags of the tile IDs our
 * palette picks (e.g. water tiles must carry the Wet flag; land tiles must not
 * be Impassable). If the file is missing, the generator still works using the
 * built-in palette (flags treated as unknown).
 *
 * HS land section layout (confirmed against ref/UONewDawn/tiledata.mul,
 * 3,188,736 bytes): 512 groups, each = 4-byte header + 32 entries; each land
 * entry = 8-byte flags + int16 textureID + 20-byte name = 30 bytes.
 * => 0x4000 (16384) land tile IDs total.
 */
#ifndef UOMAPGEN_TILEDATA_H
#define UOMAPGEN_TILEDATA_H

#include <stdint.h>

#define TILEDATA_LAND_COUNT 0x4000

/* Subset of CalcTileFlags bits we care about. */
#define TDF_IMPASSABLE 0x00000040ULL
#define TDF_WET        0x00000080ULL

typedef struct {
    int      loaded;                        /* 0 if file absent/unreadable */
    uint64_t flags[TILEDATA_LAND_COUNT];    /* per land-tile-id flags */
} tiledata_land;

/* Load the land section. Always returns a usable struct; on failure, loaded=0
 * and all flags are 0. Never fails hard. */
void tiledata_load(tiledata_land *td, const char *path);

/* Convenience predicates (safe when !loaded -> return 0). */
int tiledata_is_wet(const tiledata_land *td, uint16_t id);
int tiledata_is_impassable(const tiledata_land *td, uint16_t id);

#endif /* UOMAPGEN_TILEDATA_H */
