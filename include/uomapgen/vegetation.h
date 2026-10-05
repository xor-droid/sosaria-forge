/*
 * vegetation.h - deterministic per-cell static placement by biome.
 *
 * Produces up to VEG_MAX_PER_CELL static records for a cell, chosen from the
 * biome's tree/rock/plant sets using a splitmix64 hash of (seed, x, y) so the
 * result is identical for a given seed and never depends on iteration order.
 */
#ifndef UOMAPGEN_VEGETATION_H
#define UOMAPGEN_VEGETATION_H

#include <stdint.h>
#include "uomapgen/config.h"

#define VEG_MAX_PER_CELL 2

/* One static item, in the on-disk field layout (x,y are cell-within-block 0-7). */
typedef struct {
    uint16_t id;
    uint8_t  x;
    uint8_t  y;
    int8_t   z;
    int16_t  hue;
} static_rec;

/* Fill out[] (capacity VEG_MAX_PER_CELL) with statics for one cell.
 *   cat  : terrain category / biome (TCAT_*)
 *   cx,cy: cell position within its 8x8 block (0..7) -> written to the record
 *   z    : ground z at the cell (statics sit on the ground)
 *   gx,gy: global tile coords (used only for the deterministic hash)
 * Returns the number of records written (0..VEG_MAX_PER_CELL). */
int vegetation_place(const mapgen_config *cfg, int cat, int cx, int cy,
                     int z, int gx, int gy, static_rec *out);

#endif /* UOMAPGEN_VEGETATION_H */
