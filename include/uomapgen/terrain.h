/*
 * terrain.h - procedural terrain grid: noise -> (land tile id, z).
 *
 * The grid is stored row-major (index = x + y*width) in generation space.
 * The .mul writer is responsible for translating this into the client's
 * column-major block layout, so this module stays layout-agnostic.
 */
#ifndef UOMAPGEN_TERRAIN_H
#define UOMAPGEN_TERRAIN_H

#include <stdint.h>
#include "uomapgen/config.h"
#include "uomapgen/tiledata.h"

/* Coarse terrain categories, used by the preview renderer. */
enum {
    TCAT_WATER_DEEP = 0,
    TCAT_WATER_SHALLOW,
    TCAT_SAND,
    TCAT_GRASS,
    TCAT_FOREST,
    TCAT_HILL,
    TCAT_COUNT
};

typedef struct {
    int       width;
    int       height;
    uint16_t *id;   /* land tile id per cell */
    int8_t   *z;    /* signed z per cell */
    uint8_t  *cat;  /* TCAT_* per cell (preview only) */
} terrain_grid;

/* Generate the full grid from cfg. td may be unloaded (flags optional); when
 * loaded, palette choices are validated and a warning is printed on mismatch.
 * Returns 0 on success, -1 on allocation failure. Caller frees with
 * terrain_free(). */
int terrain_generate(terrain_grid *g, const mapgen_config *cfg,
                     const tiledata_land *td);

void terrain_free(terrain_grid *g);

#endif /* UOMAPGEN_TERRAIN_H */
