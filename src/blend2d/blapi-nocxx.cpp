// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blruntime_p.h"

// This file provides support for building Blend2D without C++ standard library.
// Blend2D doesn't really use any C++ features, but there are little things that
// require some care.
#ifdef BL_BUILD_NO_STDCXX

extern "C" {
  // `__cxa_pure_virtual` replaces all abstract virtual functions (that have no
  // implementation). We provide a replacement with `BL_HIDDEN` attribute to
  // make it local to Blend2D library.
  BL_HIDDEN void __cxa_pure_virtual() noexcept {
    blRuntimeFailure("[Blend2D] __cxa_pure_virtual(): Pure virtual function called");
  }
}

#endif // BL_BUILD_NO_STDCXX
