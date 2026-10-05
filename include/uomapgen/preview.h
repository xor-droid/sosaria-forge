/*
 * preview.h - optional top-down PNG render of the terrain for fast iteration.
 */
#ifndef UOMAPGEN_PREVIEW_H
#define UOMAPGEN_PREVIEW_H

#include "uomapgen/terrain.h"

/* Render the grid to a PNG at path (blue=water, green=land, grey=hills, with
 * land shaded by z). Returns 0/-1. */
int preview_write_png(const terrain_grid *g, const char *path);

#endif /* UOMAPGEN_PREVIEW_H */
