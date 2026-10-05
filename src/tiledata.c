#include "uomapgen/tiledata.h"

#include <stdio.h>
#include <string.h>

/* Read a little-endian uint64 from a byte buffer. */
static uint64_t rd_u64le(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
        v |= (uint64_t)p[i] << (8 * i);
    return v;
}

void tiledata_load(tiledata_land *td, const char *path) {
    memset(td, 0, sizeof(*td));
    if (!path || path[0] == '\0')
        return;

    FILE *f = fopen(path, "rb");
    if (!f)
        return;

    /* HS land group: 4-byte header + 32 * 30-byte entries = 964 bytes. */
    uint8_t group[964];
    int tile = 0;
    const int groups = TILEDATA_LAND_COUNT / 32; /* 512 */

    for (int g = 0; g < groups; ++g) {
        if (fread(group, 1, sizeof(group), f) != sizeof(group))
            goto done; /* truncated/old-format file: keep what we have */
        const uint8_t *p = group + 4; /* skip group header */
        for (int e = 0; e < 32; ++e, p += 30) {
            td->flags[tile++] = rd_u64le(p); /* first 8 bytes = flags */
        }
    }
    td->loaded = 1;

done:
    if (tile == TILEDATA_LAND_COUNT)
        td->loaded = 1;
    fclose(f);
}

int tiledata_is_wet(const tiledata_land *td, uint16_t id) {
    if (!td->loaded || id >= TILEDATA_LAND_COUNT)
        return 0;
    return (td->flags[id] & TDF_WET) ? 1 : 0;
}

int tiledata_is_impassable(const tiledata_land *td, uint16_t id) {
    if (!td->loaded || id >= TILEDATA_LAND_COUNT)
        return 0;
    return (td->flags[id] & TDF_IMPASSABLE) ? 1 : 0;
}
