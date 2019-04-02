// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

// This is an internal header file that is always included first by each source
// file. This means that any macros we might need to define to build 'blend2d'
// can be defined here instead of passing them to the compiler through command
// line.

#ifndef BLEND2D_BLAPI_BUILD_P_H
#define BLEND2D_BLAPI_BUILD_P_H

// ============================================================================
// [Build Configuration]
// ============================================================================

// ----------------------------------------------------------------------------
// Disable most of compiler intrinsics used by Blend2D. Disabling them is only
// useful for testing fallback functions as these intrinsics improve performance
// in general.
//
// #define BL_BUILD_NO_INTRINSICS
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Disable built-in statistics that are used to trace `BLFont`, `BLImage`,
// `BLPath`, allocations that are normally available in `BLRuntimeMemoryInfo`
// and can be queried through `BLRuntime`. These statistics use atomic operations
// so there shouldn't be reason to disable them.
//
// #define BL_BUILD_NO_STATISTICS
// ----------------------------------------------------------------------------

// Blend2D provides traces that can be enabled during development. Traces can
// help to understand how certain things work and can be used to track bugs.

// #define BL_TRACE_OT_ALL          // Trace OpenType features (all).
// #define BL_TRACE_OT_CFF          // Trace OpenType CFF|CFF2 ('CFF ', 'CFF2).
// #define BL_TRACE_OT_CORE         // Trace OpenType core     ('OS/2', 'head', 'maxp', 'post').
// #define BL_TRACE_OT_KERN         // Trace OpenType kerning  ('kern').
// #define BL_TRACE_OT_LAYOUT       // Trace OpenType layout   ('BASE', 'GDEF', 'GPOS', 'GSUB', 'JSTF').
// #define BL_TRACE_OT_NAME         // Trace OpenType name     ('name').

// ============================================================================
// [Build Requirements]
// ============================================================================

//! \cond NEVER

// Turn off deprecation warnings when building 'blend2d'. Required as <blend2d.h>
// other internal headers include some essential C headers which would show us
// deprecation warnings if we use some standard C function like `snprintf()`.
#ifdef _MSC_VER
  #if !defined(_CRT_SECURE_NO_DEPRECATE)
    #define _CRT_SECURE_NO_DEPRECATE
  #endif
  #if !defined(_CRT_SECURE_NO_WARNINGS)
    #define _CRT_SECURE_NO_WARNINGS
  #endif
#endif

#if defined(_WIN32) && !defined(_WIN32_WINNT)
  #define _WIN32_WINNT 0x0601
#endif

// The FileSystem API works fully with 64-bit file sizes and offsets,
// however, this feature must be enabled before including any header.
#if !defined(_WIN32) && !defined(_LARGEFILE64_SOURCE)
  #define _LARGEFILE64_SOURCE 1
#endif

// The FileSystem API supports extensions offered by Linux.
#if defined(__linux__) && !defined(_GNU_SOURCE)
  #define _GNU_SOURCE
#endif

//! \endcond

// ============================================================================
// [Build Diagnostics]
// ============================================================================

//! \cond NEVER

#if defined(__INTEL_COMPILER)
  // Not regularly tested.
#elif defined(_MSC_VER)
  #pragma warning(push)
  #pragma warning(disable: 4102) // unreferenced label
  #pragma warning(disable: 4127) // conditional expression is constant
  #pragma warning(disable: 4201) // nameless struct/union
  #pragma warning(disable: 4127) // conditional expression is constant
  #pragma warning(disable: 4251) // struct needs to have dll-interface
  #pragma warning(disable: 4275) // non dll-interface struct ... used
  #pragma warning(disable: 4355) // this used in base member-init list
  #pragma warning(disable: 4480) // specifying underlying type for enum
  #pragma warning(disable: 4800) // forcing value to bool true or false
#elif defined(__clang__)
  #pragma clang diagnostic ignored "-Wconstant-logical-operand"
  #pragma clang diagnostic ignored "-Wunnamed-type-template-args"
  #pragma clang diagnostic warning "-Wattributes"
#elif defined(__GNUC__)
  #pragma GCC diagnostic ignored "-Wenum-compare"
  #pragma GCC diagnostic warning "-Wattributes"
  #pragma GCC diagnostic warning "-Winline"
#endif

//! \endcond

// ============================================================================
// [Build Tests]
// ============================================================================

//! \cond NEVER

// Make sure '#ifdef'ed unit tests are not disabled by IDE.
#ifndef BL_BUILD_TEST
  #if defined(__INTELLISENSE__)
    #define BL_BUILD_TEST
  #endif
#endif

// Include a unit testing package if this is a `blend2d_test_unit` build.
#if defined(BL_BUILD_TEST)
  #include "../../test/broken.h"
#endif

//! \endcond

// ============================================================================
// [Export]
// ============================================================================

//! \cond INTERNAL

//! Export mode is on when `BL_BUILD_EXPORT` is defined - this MUST be defined
//! before including any other header as "blapi.h" uses `BL_BUILD_EXPORT` to
//! define a proper `BL_API` decorator that is used by all exported functions
//! and variables.
#define BL_BUILD_EXPORT

//! \endcond

// ============================================================================
// [Dependencies]
// ============================================================================

#include "./blapi.h"
#include "./blapi-impl.h"
#include "./blapi-internal_p.h"

#endif // BLEND2D_BLAPI_BUILD_P_H
