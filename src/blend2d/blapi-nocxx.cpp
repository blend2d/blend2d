// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blruntime_p.h"

// This file provides an experimental support for building Blend2D without C++
// standard library. Blend2D doesn't really use any C++ features, but still
// there are little things that require some care.
#ifdef BL_BUILD_NO_CXX_LIB

// The following functions are required to be provided if we don't link to the
// c++ standard library.
extern "C" void __cxa_pure_virtual() noexcept {
  blRuntimeFailure("__cxa_pure_virtual(): Pure virtual function called, terminating...");
}

#if defined(_MSC_VER)
  #define BL_NEW_DELETE_CALL BL_CDECL
#else
  #define BL_NEW_DELETE_CALL
#endif

void* BL_NEW_DELETE_CALL operator new(size_t size) { return malloc(size); }
void  BL_NEW_DELETE_CALL operator delete(void* p) { if (p) free(p); }
void  BL_NEW_DELETE_CALL operator delete(void* p, size_t) {}

#endif
