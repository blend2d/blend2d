# A8 JIT `fill_path()` reproduction

`bl_repro_a8_jit_fill_path` recreates an A8 raster artifact found while
Minimap evaluated Blend2D as a glyph raster backend. It does not use a font.
The input is an integer-aligned 30 by 1 pixel rectangle stored as a `BLPath`.

The image wraps a 16-byte-aligned buffer with a 33-byte stride, so its second
row begins one byte past an aligned address. On AArch64, the JIT pipeline
renders only 29 of the 30 expected pixels on that row: pixel `(30, 1)` is left
clear. Rendering the same path on the aligned first row, or changing the
stride to 32 bytes, makes the artifact disappear. The fixed pipeline renders
all 30 pixels in every case, and calling `fill_rect()` instead of
`fill_path()` also renders all 30 pixels.

Build Blend2D with JIT support and run:

```sh
cmake -S . -B build -DBLEND2D_TEST=ON -DASMJIT_DIR=/path/to/asmjit
cmake --build build --target bl_repro_a8_jit_fill_path
cd build
./bl_repro_a8_jit_fill_path
```

The executable prints `BUG REPRODUCED` and exits with status 0 when any pixel
differs from the expected rectangle. It exits with status 2 when the artifact
does not occur.
