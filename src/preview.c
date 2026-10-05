#include "uomapgen/preview.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8  /* harmless on POSIX; keeps paths sane */
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct { unsigned char r, g, b; } rgb;

static const rgb CAT_COLOR[TCAT_COUNT] = {
    [TCAT_WATER_DEEP]    = {  30,  60, 140 },
    [TCAT_WATER_SHALLOW] = {  60, 110, 180 },
    [TCAT_SAND]          = { 210, 200, 140 },
    [TCAT_GRASS]         = {  70, 150,  70 },
    [TCAT_FOREST]        = {  40, 100,  40 },
    [TCAT_HILL]          = { 120, 100,  80 },
    [TCAT_MOUNTAIN]      = { 115, 110, 105 },
    [TCAT_RIVER]         = {  70, 130, 210 },
};

static unsigned char clampu8(int v) {
    return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

int preview_write_png(const terrain_grid *g, const char *path) {
    const int W = g->width, H = g->height;
    const size_t n = (size_t)W * (size_t)H;
    unsigned char *img = (unsigned char *)malloc(n * 3);
    if (!img) {
        fprintf(stderr, "error: out of memory for preview\n");
        return -1;
    }

    for (size_t i = 0; i < n; ++i) {
        int cat = g->cat[i];
        if (cat < 0 || cat >= TCAT_COUNT)
            cat = TCAT_GRASS;
        rgb c = CAT_COLOR[cat];
        /* Shade land by height for relief; leave water/rivers flat. */
        if (cat != TCAT_WATER_DEEP && cat != TCAT_WATER_SHALLOW && cat != TCAT_RIVER) {
            int shade = g->z[i]; /* -128..127, land is >= 0 here */
            img[i * 3 + 0] = clampu8((int)c.r + shade);
            img[i * 3 + 1] = clampu8((int)c.g + shade);
            img[i * 3 + 2] = clampu8((int)c.b + shade);
        } else {
            img[i * 3 + 0] = c.r;
            img[i * 3 + 1] = c.g;
            img[i * 3 + 2] = c.b;
        }
    }

    int ok = stbi_write_png(path, W, H, 3, img, W * 3);
    free(img);
    if (!ok) {
        fprintf(stderr, "error: failed to write preview %s\n", path);
        return -1;
    }
    return 0;
}
