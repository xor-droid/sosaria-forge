# CLAUDE.md — uomapgen

Guidance for Claude Code (and humans) working in this repo.

## What this is

`uomapgen` is a procedural **Ultima Online** map generator that writes classic
`.mul` map data (terrain `mapN.mul` + `staidxN.mul` + `staticsN.mul`) for a
**ModernUO** shard. It uses
[FastNoiseLite](https://github.com/Auburn/FastNoiseLite) for randomization.
It generates **organic continents** (noise-shaped coastlines) with **climate
biomes** (snow/desert/jungle/swamp/forest/grass), **rivers** (meandering, fords,
lakes), **mountains** (ridged, with walkable passes), **beaches**, and
**vegetation statics** (trees/rocks/plants). The statics pipeline is also the
hook for **towns and areas of interest** (not built yet).

- **Language:** POSIX C (C11), Linux only.
- **Build system:** CMake.
- **Determinism:** output is **byte-identical for a given build + seed/config**.
- **No environment variables** are ever read — configuration is CLI flags and
  an optional config file only.

## Build & run

Always build single-core (`-j1`) — this is a hard project requirement:

```sh
cmake -S . -B build
cmake --build build -j1
./build/uomapgen --help
```

Example run (small test map + preview + ModernUO snippet):

```sh
./build/uomapgen --seed 42 --preset test --out ./out --preview ./out/preview.png --emit-mapdef
```

## Repository layout

```
CMakeLists.txt            top-level build (pins -j1 in docs, determinism flags)
include/uomapgen/*.h      public headers, one per module
src/*.c                   implementation (see modules below)
third_party/
  FastNoiseLite/          vendored C single-header noise lib
  stb/                    vendored stb_image_write.h (PNG preview)
config/example.cfg        annotated sample config
docs/FORMAT.md            exact .mul byte layouts
docs/DETERMINISM.md       seeding + reproducibility rules
ref/UONewDawn/            reference UO client assets (NOT committed; see below)
out/                      generated output (NOT committed)
```

### Modules (`src/` + matching `include/uomapgen/*.h`)

- `config`   — defaults, config-file parse, presets, validation. No env vars.
- `io`       — explicit little-endian byte writers + path/dir helpers. **Never
               `fwrite` raw structs** (would leak host endianness/padding).
- `noise`    — FastNoiseLite wrapper; the ONE unit that defines `FNL_IMPL`.
               splitmix64-derived per-layer seeds from the master seed (salts in
               `noise.h`, incl. elevation/moisture/continent/temperature/meander/
               biome/veg). Also `noise_layer_create_ridged` (mountains).
- `tiledata` — reads the land section of `tiledata.mul` (High Seas format) to
               sanity-check palette tile flags (Wet / Impassable). Optional.
- `biome`    — climate-band biome classification (temperature-by-latitude +
               moisture + elevation) and the per-biome **land-tile palette**
               (varied by a per-cell hash). `biome.c` is where land tiles live.
- `terrain`  — the pipeline: elevation/continent fields → land/water → biomes,
               then passes for mountains, **rivers** (meander, fords, lakes),
               **beaches** (sloped coasts), **mountain passes**, and slope-limit.
               Continent shaping = elevation noise − radial falloff (organic).
- `vegetation`— deterministic per-cell static placement by biome (trees, cacti,
               reeds, boulders, plants); the curated static-ID sets live here.
- `mapwriter`— writes `mapN.mul` in the client's column-major block layout.
- `statics`  — writes `staidxN.mul` + `staticsN.mul`: streams the vegetation
               records grouped per block with correct index offsets. Empty
               blocks → `(-1,-1,-1)`. **Towns/POI will add records here too.**
- `preview`  — optional top-down PNG via stb (per-biome colors).
- `mapdef`   — emits `map-definitions.snippet.json` for ModernUO.

## UO `.mul` byte format (what the writers must honor)

Full detail in `docs/FORMAT.md`. The essentials, confirmed against the
reference client and ModernUO's `Projects/Server/TileMatrix/TileMatrix.cs`:

- **`mapN.mul`** — blocks of 8×8 cells, **196 bytes** each:
  - 4-byte block header (server ignores it; we write `0`).
  - 64 cells, each `int16 tileID` (LE) + `int8 z`.
  - **Block order is column-major:** `blockIndex = blockX * BlockHeight + blockY`
    where `BlockWidth = width>>3`, `BlockHeight = height>>3`.
  - **Cell order in a block:** `cellIndex = ((y & 7) << 3) + (x & 7)`.
- **`staidxN.mul`** — one 12-byte record per block (same column-major order):
  `int32 lookup, int32 length, int32 extra`. Empty block = `(-1,-1,-1)`;
  `length` must be a multiple of 7.
- **`staticsN.mul`** — 7-byte records grouped per block:
  `uint16 id, uint8 x(0..7), uint8 y(0..7), int8 z, int16 hue` (all LE).
- **`tiledata.mul`** here is **High Seas format** (3,188,736 bytes): land entry
  = 8-byte flags + `int16 texID` + 20-byte name = 30 bytes; 512 groups of 32.
- ModernUO prefers `.mul`; it only falls back to `mapNLegacyMUL.uop` if
  `mapN.mul` is absent. **We always write `.mul`.** `radarcol.mul` is unused by
  the server and not generated.

## ModernUO integration

- Server source lives at
  `/mnt/c/Users/brian/bin/uoMaps/ModernUO-Main/ModernUO-main`.
- Map dimensions come from `Distribution/Data/map-definitions.json`. The
  generated map's `width`/`height` **must match** the entry for that map, or the
  server computes wrong block offsets. Use `--emit-mapdef` to get a matching
  snippet, or `--preset felucca` (7168×4096) for a drop-in `map0` replacement.
- Drop the generated `mapN.mul` / `staidxN.mul` / `staticsN.mul` into a
  directory listed in the server's `dataDirectories` config.

## Reference assets

`ref/UONewDawn/` is a full UO client (Stygian Abyss era, ClassicUO 7.0.15.1,
HS-format tiledata). It is **large and copyrighted — never commit it** (it is
git-ignored). Treat `ref/UONewDawn/map0.mul` as the layout oracle and
`tiledata.mul` as the tile-flag source.

## Conventions / gotchas

- Keep all on-disk writes going through `io_write_*le` helpers.
- Keep `FNL_IMPL` defined in exactly one `.c` file (`noise.c`).
- Anything affecting output bytes for a given seed (noise params, the per-biome
  palettes in `biome.c`, the vegetation sets/densities in `vegetation.c`,
  iteration order, the per-layer salts in `noise.h`) is part of the determinism
  contract — changing it changes everyone's maps. Call it out in commits.
- Determinism now covers `map0.mul`, `staidx0.mul` **and** `statics0.mul`
  (vegetation is hashed from `(seed,x,y)`); verify all three with `sha256sum`.
- Build must stay warning-clean with `-Wall -Wextra -Wshadow` (the FastNoiseLite
  3D-cellular warning is suppressed only for `noise.c`).

## Roadmap

1. ✅ Terrain: organic continents, oceans, navigable land → valid `.mul` triplet.
2. ✅ Enrichment: climate biomes, rivers (fords/lakes), mountains (passes),
   beaches, vegetation statics.
3. ⬜ Towns & areas of interest: place building/road/deco statics via
   `vegetation.c`/`statics.c` (same per-block record pipeline).
4. ⬜ Optional region/teleporter JSON emission for ModernUO.
5. ⬜ Polish: biome-border dithering; pixel-authentic cliff-face mountains.
