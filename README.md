# uomapgen

Procedural **Ultima Online** terrain generator for **ModernUO**, in POSIX C.

It writes classic `.mul` map data (`mapN.mul` + `staidxN.mul` + `staticsN.mul`)
using [FastNoiseLite](https://github.com/Auburn/FastNoiseLite). Output is
**byte-identical for a given build + seed**, so a map is fully described by its
seed and config. No environment variables are used — ever.

Currently generates **terrain** (oceans, waterways, navigable land). Towns and
areas of interest are planned and slot into the statics pipeline.

## Build

Single-core build is a project requirement:

```sh
cmake -S . -B build
cmake --build build -j1
```

## Usage

```sh
./build/uomapgen --help
```

Generate a small test map with a preview image and a ModernUO definition
snippet:

```sh
./build/uomapgen --seed 42 --preset test --out ./out \
    --preview ./out/preview.png --emit-mapdef
```

Full Felucca-sized drop-in `map0` (7168×4096, ~90 MB):

```sh
./build/uomapgen --seed 1 --preset felucca --out ./out --map 0
```

Drive everything from a config file (CLI flags still override it):

```sh
./build/uomapgen --config config/example.cfg --seed 7
```

### Key options

| Option | Meaning |
| --- | --- |
| `--seed <uint64>` | Master seed; same seed ⇒ same bytes. |
| `--preset test\|felucca` | 1024×1024 or 7168×4096. |
| `--width/--height` | Custom size in tiles (multiple of 8). |
| `--sea-level`, `--frequency`, `--octaves` | Terrain shaping. |
| `--max-slope` | Caps z steps between adjacent tiles (navigability). |
| `--preview <png>` | Top-down preview render. |
| `--emit-mapdef` | Write `map-definitions.snippet.json`. |
| `--map <N>` | Target facet / file index. |

See `./build/uomapgen --help` for the complete list and more examples.

## ModernUO

Copy `mapN.mul` / `staidxN.mul` / `staticsN.mul` into a directory listed in the
server's `dataDirectories`. The map's `width`/`height` **must match**
`Data/map-definitions.json` — use `--emit-mapdef` to get a matching entry, or
`--preset felucca` for a drop-in `map0`.

## Docs

- `docs/FORMAT.md` — exact `.mul` byte layouts.
- `docs/DETERMINISM.md` — seeding and reproducibility rules.
- `CLAUDE.md` — architecture and conventions.

## Third-party

Vendored single-header libraries under `third_party/` (see `THIRD-PARTY.md`):
FastNoiseLite (MIT) and stb_image_write (public domain / MIT).
