# Third-party components

Vendored under `third_party/`. Their licenses are reproduced in the respective
header files.

## FastNoiseLite

- Path: `third_party/FastNoiseLite/FastNoiseLite.h`
- Source: https://github.com/Auburn/FastNoiseLite (C version)
- License: MIT

## stb_image_write

- Path: `third_party/stb/stb_image_write.h`
- Source: https://github.com/nothings/stb
- License: public domain (Unlicense) OR MIT, at your option

These headers are included as-is and are not modified. Do not reformat or edit
them; to update, re-download the upstream version and note the change (it is
part of the determinism contract — see `docs/DETERMINISM.md`).
