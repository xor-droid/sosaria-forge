# Determinism & reproducibility

**Goal:** the generated `.mul` files are **byte-identical for a given build and
a given seed/config**. A map is fully described by its seed and configuration.

## How it's achieved

1. **Single master seed.** `--seed` (a `uint64`) is the only entropy source.
2. **Derived per-layer seeds.** Each noise layer (elevation, moisture, …) gets a
   32-bit seed via `splitmix64` with a fixed per-layer salt (`noise.h`,
   `NOISE_LAYER_*`). The salts and mixing are frozen as part of the contract.
3. **No nondeterministic inputs.** No `rand()`, no `time()`, no threads, and
   **no environment variables**. Configuration is CLI flags + optional file only.
4. **Fixed iteration order.** Terrain generation and the slope-limiting passes
   run in a fixed raster/reverse-raster order; file writers emit blocks and
   cells in the exact on-disk order.
5. **Explicit little-endian writes.** All multi-byte fields go through
   `io_write_*le`. We never `fwrite` raw structs, so host endianness and struct
   padding cannot leak into the output.
6. **Pinned FP behavior.** The build disables FP contraction
   (`-ffp-contract=off`) so FMA availability / optimization level does not change
   results within a build target.

## Scope of the guarantee

Byte-identical output is guaranteed for the **same build target** — same
compiler, architecture, and flags. This is the intended "per-build" guarantee.

**Cross-architecture bit-exactness is out of scope.** FastNoiseLite produces
`float` results, and floating-point rounding can differ across CPUs/compilers.
If you need identical bytes across different machines, build with one pinned
toolchain (e.g. ship a container/toolchain file) and treat that as *the* build.

## Verifying

```sh
./build/uomapgen --seed 42 --preset test --out ./a
./build/uomapgen --seed 42 --preset test --out ./b
sha256sum a/map0.mul b/map0.mul      # hashes must match

./build/uomapgen --seed 99 --preset test --out ./c
sha256sum c/map0.mul                 # must differ from seed 42
```

## What changes output

Anything below alters the bytes for a given seed; changing it is a
"breaking" change for reproducibility and should be called out in commits:

- noise parameters (`frequency`, `octaves`, lacunarity/gain in `noise.c`),
- the per-layer salts in `noise.h`,
- the tile palette or category thresholds in `terrain.c`,
- slope-limiting logic / order,
- the FastNoiseLite version under `third_party/`.
