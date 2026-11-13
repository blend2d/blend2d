Blend2D
-------

2D Vector Graphics Powered by a JIT Compiler.

  * [Official Home Page (blend2d.com)](https://blend2d.com)
  * [Official Repository (blend2d/blend2d)](https://github.com/blend2d/blend2d)
  * [Public Chat Channel](https://app.element.io/#/room/#blend2d:matrix.org)
  * [Zlib License](./LICENSE.md)

See [blend2d.com](https://blend2d.com) page for more details.

Project Organization
--------------------

  * **`/`**                - Project root, contains this README.md, other markdown files, CMakeLists.txt, and scripts for developers
    * **src**              - Source code (always point include path in here)
      * **blend2d**        - Blend2D source code and headers, all core classes and public headers are here
        * **axl**          - Support classes and utilities (arena allocator & containers, bit operations, string operations, ...)
        * **codec**        - Image codecs and codec utilities (BMP, JPEG, PNG, and QOI)
        * **compression**  - Implementation of compression and decompression algorithms (DEFLATE)
        * **opentype**     - TrueType and OpenType font support
        * **pipeline**     - 2D rendering pipeline definitions and implementations (reference and JIT)
        * **pixelops**     - Pixel operations helpers
        * **raster**       - 2D rendering context implementation
        * **simd**         - SIMD abstraction library Blend2D uses
        * **tables**       - Lookup tables and SIMD constants
        * **threading**    - Multithreading abstraction (POSIX threads or Windows threads)
        * **unicode**      - Support for unicode and conversion between various unicode encodings
    * **testing**          - Testing, benchmarking, samples, and interactive demos (don't embed in your project)
    * **3rdparty**         - Place where to clone [asmjit](https://github.com/asmjit/asmjit) to enable JIT code generation or where to add dependencies for testing and benchmarking purposes

Roadmap
-------

  * See [Roadmap](https://blend2d.com/roadmap.html) page for more details

Documentation
-------------

  * [Documentation Index](https://blend2d.com/doc/index.html)

Contributing Guidelines
-----------------------

  * [CONTRIBUTING.md](./CONTRIBUTING.md)

Building
--------

Blend2D is almost self-containing library. The only dependency is AsmJit, which is only required by when JIT is enabled.

To fetch both Blend2D and AsmJit, type:

```bash
$ git clone https://github.com/blend2d/blend2d
$ git clone https://github.com/asmjit/asmjit blend2d/3rdparty/asmjit
```

Then you can simply configure and build blend2d with CMake:

```bash
$ cmake -S blend2d -B blend2d/build/Release -DCMAKE_BUILD_TYPE=Release -DBLEND2D_TEST=ON
$ cmake --build blend2d/build/Release --config Release
```

And run samples:

```
$ blend2d/build/Release/bl_sample_1
$ blend2d/build/Release/bl_sample_2
...
```

To build Blend2D with interactive demos and with benchmark targeting Qt and other libraries, use `BLEND2D_DEMOS=ON`

```bash
$ cmake -S blend2d -B blend2d/build/Release -DCMAKE_BUILD_TYPE=Release -DBLEND2D_TEST=ON -DBLEND2D_DEMOS=ON
$ cmake --build blend2d/build/Release --config Release
```

Please note that to build interactive demos and benchmarking backends, you need to have Qt installed, in addition to all rendering libraries you would like to benchmark.

For more information, visit [https://blend2d.com/doc/build-instructions.html](Build Instructions)

### Cross-compiling for Windows on Linux

The `debug-mingw-x64` and `release-mingw-x64` presets build Blend2D tests and samples for 64-bit Windows using MinGW. On Debian/Ubuntu, install the cross-compiler and Wine:

```bash
sudo apt-get install g++-mingw-w64-x86-64-posix wine wine64
```

And then run these commands from the Blend2D directory:

```bash
cmake --preset release-mingw-x64
cmake --build --preset release-mingw-x64 --parallel
```

Replace `debug-mingw-x64` with `release-mingw-x64` for an optimized build.

The presets build a static Blend2D library and link executable runtime dependencies statically, so tests and samples do not need MinGW runtime DLLs alongside them. This build won't enable Qt demos or any other tests/benchmarks that would need third party libraries installed.

To run a sample or a selected unit test directly:

```bash
wine build/debug-mingw-x64/bl_test_runner.exe
```

Bindings
--------

  * [Download Page](https://blend2d.com/download.html#Bindings) provides a list of Blend2D bindings

License
-------

  * [LICENSE.md](./LICENSE.md)
