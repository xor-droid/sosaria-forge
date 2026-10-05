/*
 * statics.h - writes staidxN.mul + staticsN.mul.
 *
 * Baseline emits a valid EMPTY statics set: every block index entry is
 * (-1, -1, -1) and staticsN.mul is zero bytes. This is the extension point for
 * towns and areas of interest: future code will group 7-byte static records
 * (uint16 id, uint8 x, uint8 y, int8 z, int16 hue) per block, write them to
 * staticsN.mul, and point each block's index entry at (lookup, length, 0).
 */
#ifndef UOMAPGEN_STATICS_H
#define UOMAPGEN_STATICS_H

/* Write empty staidx<N>.mul (block_width*block_height entries of -1,-1,-1) and
 * an empty statics<N>.mul into out_dir. Returns 0/-1. */
int statics_write_empty(const char *out_dir, int map_index,
                        int block_width, int block_height);

#endif /* UOMAPGEN_STATICS_H */
