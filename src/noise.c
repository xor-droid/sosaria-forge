/*
 * noise.c - FastNoiseLite wrapper. This is the single translation unit that
 * instantiates the FastNoiseLite implementation (FNL_IMPL).
 */
#include "uomapgen/noise.h"

#define FNL_IMPL
#include "FastNoiseLite.h"

#include <stdlib.h>

struct noise_layer {
    fnl_state state;
};

uint64_t noise_splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

int noise_derive_seed(uint64_t master, uint32_t salt) {
    /* Mix master with the salt, then take the low 32 bits as a signed int.
     * Deterministic and independent per salt. */
    uint64_t s = master ^ (0xD1B54A32D192ED03ULL * (uint64_t)(salt + 1));
    uint64_t mixed = noise_splitmix64(&s);
    return (int)(uint32_t)mixed;
}

noise_layer *noise_layer_create(uint64_t master, uint32_t salt,
                                double frequency, int octaves) {
    noise_layer *l = (noise_layer *)calloc(1, sizeof(*l));
    if (!l)
        return NULL;
    l->state = fnlCreateState();
    l->state.seed = noise_derive_seed(master, salt);
    l->state.noise_type = FNL_NOISE_OPENSIMPLEX2;
    l->state.fractal_type = FNL_FRACTAL_FBM;
    l->state.frequency = (float)frequency;
    l->state.octaves = octaves;
    /* Explicit defaults so the determinism contract does not depend on
     * FastNoiseLite's internal defaults changing across versions. */
    l->state.lacunarity = 2.0f;
    l->state.gain = 0.5f;
    l->state.weighted_strength = 0.0f;
    return l;
}

noise_layer *noise_layer_create_ridged(uint64_t master, uint32_t salt,
                                        double frequency, int octaves) {
    noise_layer *l = noise_layer_create(master, salt, frequency, octaves);
    if (l)
        l->state.fractal_type = FNL_FRACTAL_RIDGED;
    return l;
}

void noise_layer_free(noise_layer *l) {
    free(l);
}

double noise_layer_sample(const noise_layer *l, int x, int y) {
    return (double)fnlGetNoise2D(&l->state, (FNLfloat)x, (FNLfloat)y);
}
