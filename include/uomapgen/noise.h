/*
 * noise.h - deterministic seeding and layered noise sampling.
 *
 * Determinism rules:
 *   - A single uint64 master seed drives everything.
 *   - Each noise "layer" (elevation, moisture, ...) gets its own 32-bit seed
 *     derived from the master via splitmix64 with a fixed per-layer salt.
 *   - No rand(), no time(), no threads, no environment input.
 * Same build + same master seed => identical samples, every run.
 */
#ifndef UOMAPGEN_NOISE_H
#define UOMAPGEN_NOISE_H

#include <stdint.h>

/* Fixed per-layer salts. Changing these changes output for a given seed, so
 * they are frozen as part of the determinism contract. */
enum {
    NOISE_LAYER_ELEVATION = 0x01,
    NOISE_LAYER_MOISTURE  = 0x02,
    NOISE_LAYER_DETAIL    = 0x03,
    NOISE_LAYER_CONTINENT = 0x04,
    NOISE_LAYER_MEANDER   = 0x05
};

/* splitmix64: fixed, portable 64-bit mixer used for seed derivation. */
uint64_t noise_splitmix64(uint64_t *state);

/* Derive the 32-bit int seed FastNoiseLite wants for a given layer salt. */
int noise_derive_seed(uint64_t master, uint32_t salt);

/* Opaque layered sampler. */
typedef struct noise_layer noise_layer;

/* Create an fBm OpenSimplex2 layer. frequency/octaves come from config.
 * Returns NULL on allocation failure. Free with noise_layer_free(). */
noise_layer *noise_layer_create(uint64_t master, uint32_t salt,
                                double frequency, int octaves);

/* Create a RIDGED OpenSimplex2 layer (sharp ridge lines -> mountain ranges). */
noise_layer *noise_layer_create_ridged(uint64_t master, uint32_t salt,
                                        double frequency, int octaves);
void noise_layer_free(noise_layer *l);

/* Sample at tile (x,y). Returns a value in roughly [-1, 1]. */
double noise_layer_sample(const noise_layer *l, int x, int y);

#endif /* UOMAPGEN_NOISE_H */
