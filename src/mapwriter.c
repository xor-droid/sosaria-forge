#include "uomapgen/mapwriter.h"
#include "uomapgen/io.h"
#include "uomapgen/config.h"

#include <stdio.h>

int mapwriter_write(const terrain_grid *g, const char *out_dir, int map_index) {
    const int W = g->width, H = g->height;
    const int BW = W >> 3;   /* block columns */
    const int BH = H >> 3;   /* block rows */

    char name[32];
    snprintf(name, sizeof(name), "map%d.mul", map_index);
    char path[UOMG_PATH_MAX];
    if (io_join_path(path, sizeof(path), out_dir, name) != 0)
        return -1;

    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "error: cannot open %s for writing\n", path);
        return -1;
    }

    int rc = 0;
    /* Column-major: block index = bx*BH + by, so bx is the outer loop. */
    for (int bx = 0; bx < BW && rc == 0; ++bx) {
        for (int by = 0; by < BH && rc == 0; ++by) {
            /* 4-byte block header (server ignores; write 0). */
            if (io_write_u32le(f, 0) != 0) { rc = -1; break; }

            /* Cells: cellIndex = (cy<<3)+cx -> cy outer, cx inner. */
            for (int cy = 0; cy < 8 && rc == 0; ++cy) {
                for (int cx = 0; cx < 8; ++cx) {
                    int x = (bx << 3) + cx;
                    int y = (by << 3) + cy;
                    size_t i = (size_t)x + (size_t)y * (size_t)W;
                    if (io_write_i16le(f, (int16_t)g->id[i]) != 0 ||
                        io_write_i8(f, g->z[i]) != 0) {
                        rc = -1;
                        break;
                    }
                }
            }
        }
    }

    if (rc != 0)
        fprintf(stderr, "error: write failed for %s\n", path);
    if (fclose(f) != 0)
        rc = -1;
    return rc;
}
