#include "uomapgen/statics.h"
#include "uomapgen/vegetation.h"
#include "uomapgen/io.h"

#include <stdio.h>
#include <stdlib.h>

/* Deterministic ordering within a block: by y, then x, then z, then id. */
static int rec_less(const static_rec *a, const static_rec *b) {
    if (a->y != b->y) return a->y < b->y;
    if (a->x != b->x) return a->x < b->x;
    if (a->z != b->z) return a->z < b->z;
    return a->id < b->id;
}

static int write_rec(FILE *f, const static_rec *r) {
    return (io_write_u16le(f, r->id) != 0 ||
            io_write_u8(f, r->x) != 0 ||
            io_write_u8(f, r->y) != 0 ||
            io_write_i8(f, r->z) != 0 ||
            io_write_i16le(f, (uint16_t)r->hue) != 0) ? -1 : 0;
}

int statics_write(const terrain_grid *g, const mapgen_config *cfg,
                  const char *out_dir, int map_index) {
    const int W = g->width, H = g->height;
    const int BW = W >> 3, BH = H >> 3;

    char name[32], path[UOMG_PATH_MAX];
    snprintf(name, sizeof(name), "staidx%d.mul", map_index);
    if (io_join_path(path, sizeof(path), out_dir, name) != 0) return -1;
    FILE *fi = fopen(path, "wb");
    if (!fi) { fprintf(stderr, "error: cannot open %s\n", path); return -1; }

    snprintf(name, sizeof(name), "statics%d.mul", map_index);
    if (io_join_path(path, sizeof(path), out_dir, name) != 0) { fclose(fi); return -1; }
    FILE *fs = fopen(path, "wb");
    if (!fs) { fprintf(stderr, "error: cannot open %s\n", path); fclose(fi); return -1; }

    /* Up to 64 cells * VEG_MAX_PER_CELL records per block. */
    static_rec buf[64 * VEG_MAX_PER_CELL];
    int32_t offset = 0;   /* running byte offset into statics file */
    int rc = 0;
    long total = 0;

    /* Column-major block order: blockIndex = bx*BH + by (matches map/staidx). */
    for (int bx = 0; bx < BW && rc == 0; ++bx) {
        for (int by = 0; by < BH && rc == 0; ++by) {
            int nb = 0;
            for (int cy = 0; cy < 8; ++cy)
                for (int cx = 0; cx < 8; ++cx) {
                    int x = (bx << 3) + cx, y = (by << 3) + cy;
                    size_t idx = (size_t)x + (size_t)y * (size_t)W;
                    int k = vegetation_place(cfg, g->cat[idx], cx, cy, g->z[idx],
                                             x, y, &buf[nb]);
                    nb += k;
                }

            if (nb == 0) {
                /* empty block */
                if (io_write_i32le(fi, -1) || io_write_i32le(fi, -1) || io_write_i32le(fi, -1))
                    rc = -1;
                continue;
            }

            /* deterministic insertion sort (nb is small) */
            for (int a = 1; a < nb; ++a) {
                static_rec key = buf[a]; int b = a - 1;
                while (b >= 0 && rec_less(&key, &buf[b])) { buf[b + 1] = buf[b]; --b; }
                buf[b + 1] = key;
            }

            for (int r = 0; r < nb; ++r)
                if (write_rec(fs, &buf[r]) != 0) { rc = -1; break; }

            int32_t len = (int32_t)(nb * 7);
            if (rc == 0 &&
                (io_write_i32le(fi, offset) || io_write_i32le(fi, len) || io_write_i32le(fi, 0)))
                rc = -1;
            offset += len;
            total += nb;
        }
    }

    if (fclose(fs) != 0) rc = -1;
    if (fclose(fi) != 0) rc = -1;
    if (rc != 0) fprintf(stderr, "error: failed writing statics for map %d\n", map_index);
    else fprintf(stderr, "statics: %ld records, %d bytes\n", total, offset);
    return rc;
}
