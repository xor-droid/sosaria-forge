/*
 * statics.h - writes staidxN.mul + staticsN.mul.
 *
 * Statics are produced by the vegetation module per cell (trees, rocks, plants)
 * and grouped per map block. When vegetation is disabled (or a map has no
 * statics in a block) the block's index entry is the empty marker (-1,-1,-1)
 * and nothing is written for it, which stays a valid, loadable set.
 */
#ifndef UOMAPGEN_STATICS_H
#define UOMAPGEN_STATICS_H

#include "uomapgen/terrain.h"
#include "uomapgen/config.h"

/* Write staidx<N>.mul + statics<N>.mul for the grid into out_dir. Returns 0/-1. */
int statics_write(const terrain_grid *g, const mapgen_config *cfg,
                  const char *out_dir, int map_index);

#endif /* UOMAPGEN_STATICS_H */
