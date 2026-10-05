#include "uomapgen/io.h"
#include "uomapgen/config.h"  /* UOMG_PATH_MAX */

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

int io_write_u8(FILE *f, uint8_t v) {
    return fputc((int)v, f) == EOF ? -1 : 0;
}

int io_write_i8(FILE *f, int8_t v) {
    return io_write_u8(f, (uint8_t)v);
}

int io_write_u16le(FILE *f, uint16_t v) {
    uint8_t b[2] = { (uint8_t)(v & 0xFF), (uint8_t)((v >> 8) & 0xFF) };
    return fwrite(b, 1, 2, f) == 2 ? 0 : -1;
}

int io_write_i16le(FILE *f, int16_t v) {
    return io_write_u16le(f, (uint16_t)v);
}

int io_write_u32le(FILE *f, uint32_t v) {
    uint8_t b[4] = {
        (uint8_t)(v & 0xFF), (uint8_t)((v >> 8) & 0xFF),
        (uint8_t)((v >> 16) & 0xFF), (uint8_t)((v >> 24) & 0xFF)
    };
    return fwrite(b, 1, 4, f) == 4 ? 0 : -1;
}

int io_write_i32le(FILE *f, int32_t v) {
    return io_write_u32le(f, (uint32_t)v);
}

int io_join_path(char *out, size_t out_sz, const char *dir, const char *name) {
    int n;
    size_t len = strlen(dir);
    if (len > 0 && (dir[len - 1] == '/'))
        n = snprintf(out, out_sz, "%s%s", dir, name);
    else
        n = snprintf(out, out_sz, "%s/%s", dir, name);
    return (n < 0 || (size_t)n >= out_sz) ? -1 : 0;
}

int io_ensure_dir(const char *path) {
    char tmp[UOMG_PATH_MAX];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(tmp))
        return -1;
    memcpy(tmp, path, len + 1);
    if (len > 1 && tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}
