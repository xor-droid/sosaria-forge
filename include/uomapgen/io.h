/*
 * io.h - explicit little-endian byte writers.
 *
 * All on-disk UO data is little-endian. We never fwrite() raw structs (that
 * would leak host endianness and struct padding). Every multi-byte field goes
 * through these helpers so output is byte-identical regardless of host arch.
 */
#ifndef UOMAPGEN_IO_H
#define UOMAPGEN_IO_H

#include <stdio.h>
#include <stdint.h>

/* Each returns 0 on success, -1 on write error. */
int io_write_u8 (FILE *f, uint8_t  v);
int io_write_i8 (FILE *f, int8_t   v);
int io_write_u16le(FILE *f, uint16_t v);
int io_write_i16le(FILE *f, int16_t  v);
int io_write_u32le(FILE *f, uint32_t v);
int io_write_i32le(FILE *f, int32_t  v);

/* Join a directory and filename into out (UOMG_PATH_MAX). Returns 0/-1. */
int io_join_path(char *out, size_t out_sz, const char *dir, const char *name);

/* mkdir -p style: ensure directory exists. Returns 0/-1. */
int io_ensure_dir(const char *path);

#endif /* UOMAPGEN_IO_H */
