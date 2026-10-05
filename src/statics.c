#include "uomapgen/statics.h"
#include "uomapgen/io.h"
#include "uomapgen/config.h"

#include <stdio.h>

int statics_write_empty(const char *out_dir, int map_index,
                        int block_width, int block_height) {
    char name[32], path[UOMG_PATH_MAX];
    int rc = 0;

    /* staidx<N>.mul: one 12-byte record per block, all (-1,-1,-1). */
    snprintf(name, sizeof(name), "staidx%d.mul", map_index);
    if (io_join_path(path, sizeof(path), out_dir, name) != 0)
        return -1;
    FILE *fi = fopen(path, "wb");
    if (!fi) {
        fprintf(stderr, "error: cannot open %s for writing\n", path);
        return -1;
    }
    long blocks = (long)block_width * (long)block_height;
    for (long b = 0; b < blocks && rc == 0; ++b) {
        if (io_write_i32le(fi, -1) != 0 ||   /* lookup */
            io_write_i32le(fi, -1) != 0 ||   /* length */
            io_write_i32le(fi, -1) != 0)     /* extra  */
            rc = -1;
    }
    if (fclose(fi) != 0)
        rc = -1;
    if (rc != 0) {
        fprintf(stderr, "error: write failed for %s\n", path);
        return -1;
    }

    /* statics<N>.mul: empty (0 bytes). */
    snprintf(name, sizeof(name), "statics%d.mul", map_index);
    if (io_join_path(path, sizeof(path), out_dir, name) != 0)
        return -1;
    FILE *fs = fopen(path, "wb");
    if (!fs) {
        fprintf(stderr, "error: cannot open %s for writing\n", path);
        return -1;
    }
    if (fclose(fs) != 0) {
        fprintf(stderr, "error: write failed for %s\n", path);
        return -1;
    }
    return 0;
}
