#include "uomapgen/mapdef.h"
#include "uomapgen/io.h"
#include "uomapgen/config.h"

#include <stdio.h>

int mapdef_write_snippet(const char *out_dir, int map_index,
                         int width, int height) {
    char path[UOMG_PATH_MAX];
    if (io_join_path(path, sizeof(path), out_dir, "map-definitions.snippet.json") != 0)
        return -1;

    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "error: cannot open %s for writing\n", path);
        return -1;
    }

    /* A single array element matching the shipped map-definitions.json schema.
     * Paste this into ModernUO's Data/map-definitions.json (replacing the entry
     * with the same index, or adding a new one). */
    fprintf(f,
        "[\n"
        "  {\n"
        "    \"index\": %d,\n"
        "    \"id\": %d,\n"
        "    \"fileIndex\": %d,\n"
        "    \"name\": \"Procedural%d\",\n"
        "    \"width\": %d,\n"
        "    \"height\": %d,\n"
        "    \"season\": 0,\n"
        "    \"rules\": \"TrammelRules\"\n"
        "  }\n"
        "]\n",
        map_index, map_index, map_index, map_index, width, height);

    if (fclose(f) != 0) {
        fprintf(stderr, "error: write failed for %s\n", path);
        return -1;
    }
    return 0;
}
