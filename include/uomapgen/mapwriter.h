/*
 * mapwriter.h - writes mapN.mul in the client's column-major block layout.
 *
 * Block order in file: blockIndex = bx*BlockHeight + by (column-major).
 * Each block: 4-byte header (written as 0) + 64 cells.
 * Cell order within block: cellIndex = ((cy&7)<<3) + (cx&7) -> cy outer, cx inner.
 * Each cell: int16 tileID (LE) + int8 z. Total 196 bytes/block.
 */
#ifndef UOMAPGEN_MAPWRITER_H
#define UOMAPGEN_MAPWRITER_H

#include "uomapgen/terrain.h"

/* Write the terrain grid to <out_dir>/map<N>.mul. Returns 0/-1. */
int mapwriter_write(const terrain_grid *g, const char *out_dir, int map_index);

#endif /* UOMAPGEN_MAPWRITER_H */
