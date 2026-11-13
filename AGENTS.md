Blend2D - high performance 2D rendering engine

  - no exceptions/rtti => don't use C++ `std` containers

Error handling:

  - failures are reported via `BLResult`
  - use `BL_PROPAGATE(xxx(...))` to immediately return propagated errors

Building:

  - use the following cmake presets: `debug`, `debug-demos` or `release-demos`
  - build directories are `build/debug`, `build/debug-demos`, and `build/release-demos`

Testing:

  - Use `build/debug/bl_test_runner --no-color --run xxx` to run `TEST_CASE(xxx)`

Compiling temporary files having `main()`:

  - first build `build/debug` so `libblend2d.a` is available
  - `$CXX <file> -o <executable> -DBL_STATIC -O2 -I<blend2d_root> -L<blend2d_root>/build/debug -lblend2d`
