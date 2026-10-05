/*
 * mapdef.h - emit a map-definitions.json snippet matching the generated map.
 *
 * ModernUO derives block offsets from width/height in Data/map-definitions.json.
 * When you generate a non-standard size you MUST make that JSON agree, or the
 * server reads garbage. This writes a ready-to-paste entry.
 */
#ifndef UOMAPGEN_MAPDEF_H
#define UOMAPGEN_MAPDEF_H

/* Write <out_dir>/map-definitions.snippet.json for the given facet/size.
 * Returns 0/-1. */
int mapdef_write_snippet(const char *out_dir, int map_index,
                         int width, int height);

#endif /* UOMAPGEN_MAPDEF_H */
