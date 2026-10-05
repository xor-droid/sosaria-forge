# UO `.mul` map format (as consumed by ModernUO)

All integers are **little-endian**. Confirmed against the reference client
(`ref/UONewDawn/`) and ModernUO
(`Projects/Server/TileMatrix/TileMatrix.cs`, `Projects/Server/Maps/MapLoader.cs`).

## Geometry

For a map of `width` × `height` tiles (both multiples of 8):

```
BlockWidth  = width  >> 3      # blocks across
BlockHeight = height >> 3      # blocks down
```

Blocks are stored **column-major**:

```
blockIndex = blockX * BlockHeight + blockY
```

Within a block, cells are stored row-major in an 8×8 grid:

```
cellIndex = ((y & 7) << 3) + (x & 7)
```

## `mapN.mul` — terrain

One record per block, **196 bytes**:

| Field | Size | Notes |
| --- | --- | --- |
| header | 4 | Ignored by the server; we write `0`. |
| cells | 64 × 3 | Each cell: `int16 tileID` + `int8 z`. |

`tileID` indexes the land section of `tiledata.mul`. `z` is signed (−128..127).

Total file size = `BlockWidth * BlockHeight * 196` bytes.
(Felucca 7168×4096 → 896×512 blocks → 89,915,392 bytes.)

## `staidxN.mul` — static index

One **12-byte** record per block, in the same column-major order as the map:

| Field | Size | Notes |
| --- | --- | --- |
| lookup | int32 | Byte offset into `staticsN.mul`. |
| length | int32 | Byte length of this block's statics (multiple of 7). |
| extra  | int32 | Unused by the server. |

An empty block is `(lookup=-1, length=-1, extra=-1)`. The server treats
`lookup < 0` **or** `length <= 0` as "no statics".

Total file size = `BlockWidth * BlockHeight * 12` bytes.

## `staticsN.mul` — statics

Concatenated **7-byte** records, grouped per block; each block's group is
located via its `staidx` entry (`lookup`, `length`).

| Field | Size | Notes |
| --- | --- | --- |
| id   | uint16 | Static item id. |
| x    | uint8  | Cell X within block (0..7). |
| y    | uint8  | Cell Y within block (0..7). |
| z    | int8   | Signed height. |
| hue  | int16  | Hue (0 = none). |

An empty map has a 0-byte `staticsN.mul`.

## `tiledata.mul` — tile metadata (read-only input)

The reference client ships the **High Seas** format (3,188,736 bytes):

- **Land section:** 512 groups × (4-byte group header + 32 entries). Each land
  entry = **8-byte flags** + `int16 textureID` + 20-byte name = **30 bytes**.
  → 0x4000 (16384) land tile ids.
- **Static section** follows (not used by this generator).

We read only the land flags (e.g. `Wet = 0x80`, `Impassable = 0x40`) to
sanity-check palette choices.

## Format notes

- ModernUO prefers `mapN.mul`; it falls back to `mapNLegacyMUL.uop` **only** if
  `mapN.mul` is absent. There is no config toggle — presence decides. We always
  write `.mul`.
- `radarcol.mul` is **not** used by the ModernUO server.
- `map-definitions.json` (`width`/`height`) must agree with the generated file,
  because block offsets are derived from those dimensions.
