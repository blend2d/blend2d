// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

// This is an internal header file that is always included first by each source
// file. This means that any macros we might need to define to build 'blend2d'
// can be defined here instead of passing them to the compiler through command
// line.

#ifndef BLEND2D_API_BUILD_P_H
#define BLEND2D_API_BUILD_P_H

// ============================================================================
// [Build - Configuration]
// ============================================================================

// ----------------------------------------------------------------------------
// Disable most of compiler intrinsics used by Blend2D. Disabling them is only
// useful for testing fallback functions as these intrinsics improve performance
// in general.
//
// #define BL_BUILD_NO_INTRINSICS
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Disable built-in statistics that is used to trace `BLFont`, `BLImage`,
// and `BLPath`, in addition to allocations that are normally available in
// `BLRuntimeMemoryInfo` and can be queried through `BLRuntime`. These statistics
// use atomic operations so there shouldn't be reason to disable them.
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
// [Build - Requirements]
// ============================================================================

//! \cond NEVER

// Turn off deprecation warnings when building 'blend2d'. Required as <blend2d.h>
// and other headers include some essential C headers that could in some cases
// warn about using functions such as `snprintf()`, which we use correctly.
#ifdef _MSC_VER
  #if !defined(_CRT_SECURE_NO_DEPRECATE)
    #define _CRT_SECURE_NO_DEPRECATE
  #endif
  #if !defined(_CRT_SECURE_NO_WARNINGS)
    #define _CRT_SECURE_NO_WARNINGS
  #endif
#endif

#if defined(_WIN32) && !defined(_WIN32_WINNT)
  #define _WIN32_WINNT 0x0600
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
// [Build - Compiler Diagnostics]
// ============================================================================

//! \cond NEVER

#if defined(__INTEL_COMPILER)
  // Not regularly tested.
#elif defined(_MSC_VER)
  #pragma warning(disable: 4102) // Unreferenced label.
  #pragma warning(disable: 4127) // Conditional expression is constant.
  #pragma warning(disable: 4201) // Nameless struct/union.
  #pragma warning(disable: 4251) // Struct needs to have dll-interface.
  #pragma warning(disable: 4275) // Non dll-interface struct ... used.
  #pragma warning(disable: 4324) // Structure was padded due to alignment specifier.
  #pragma warning(disable: 4355) // This used in base member-init list.
  #pragma warning(disable: 4458) // declaration of 'X' hides class member.
  #pragma warning(disable: 4480) // Specifying underlying type for enum.
  #pragma warning(disable: 4505) // Unreferenced local function has been removed.
  #pragma warning(disable: 4800) // Forcing value to bool true or false.
  #pragma warning(disable: 4582) // Constructor is not implicitly called.
  #pragma warning(disable: 4583) // Destructor is not implicitly called.
#elif defined(__clang__)
  #pragma clang diagnostic ignored "-Wconstant-logical-operand"
  #pragma clang diagnostic ignored "-Wunnamed-type-template-args"
  #pragma clang diagnostic ignored "-Wunused-function"
  #pragma clang diagnostic ignored "-Wswitch"
  #pragma clang diagnostic warning "-Wattributes"
#elif defined(__GNUC__)
  #pragma GCC diagnostic ignored "-Wenum-compare"
  #pragma GCC diagnostic ignored "-Wunused-function"
  #pragma GCC diagnostic ignored "-Wswitch"
  #pragma GCC diagnostic warning "-Wattributes"
  #if (__GNUC__ == 4)
    // GCC 4 has kinda broken diagnostic in this case, GCC 5+ is okay.
    #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
  #endif
  #if (__GNUC__ >= 7)
    #pragma GCC diagnostic ignored "-Wint-in-bool-context"
  #endif
  #if (__GNUC__ >= 8)
    #pragma GCC diagnostic ignored "-Wclass-memaccess"
  #endif
#endif

//! \endcond

// ============================================================================
// [Build - Target Architecture & Optimizations]
// ============================================================================

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

#if defined(_M_X64) || defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__)
  #define BL_TARGET_ARCH_X86 64
#elif defined(_M_IX86) || defined(__i386) || defined(__i386__)
  #define BL_TARGET_ARCH_X86 32
#else
  #define BL_TARGET_ARCH_X86 0
#endif

#if defined(__ARM64__) || defined(__aarch64__)
  #define BL_TARGET_ARCH_ARM 64
#elif defined(_M_ARM) || defined(_M_ARMT) || defined(__arm__) || defined(__thumb__) || defined(__thumb2__)
  #define BL_TARGET_ARCH_ARM 32
#else
  #define BL_TARGET_ARCH_ARM 0
#endif

#if defined(_MIPS_ARCH_MIPS64) || defined(__mips64)
  #define BL_TARGET_ARCH_MIPS 64
#elif defined(_MIPS_ARCH_MIPS32) || defined(_M_MRX000) || defined(__mips) || defined(__mips__)
  #define BL_TARGET_ARCH_MIPS 32
#else
  #define BL_TARGET_ARCH_MIPS 0
#endif

#define BL_TARGET_ARCH_BITS (BL_TARGET_ARCH_X86 | BL_TARGET_ARCH_ARM | BL_TARGET_ARCH_MIPS)
#if BL_TARGET_ARCH_BITS == 0
  #undef BL_TARGET_ARCH_BITS
  #if defined (__LP64__) || defined(_LP64)
    #define BL_TARGET_ARCH_BITS 64
  #else
    #define BL_TARGET_ARCH_BITS 32
  #endif
#endif

// Defined when it's safe to assume that std::atomic<uint64_t> would be non
// locking. We can always assume this on X86 architecture (even 32-bit), but
// we don't assume this on other architectures like ARM.
#if BL_TARGET_ARCH_BITS >= 64 || BL_TARGET_ARCH_X86 != 0
  #define BL_TARGET_HAS_ATOMIC_64B 1
#else
  #define BL_TARGET_HAS_ATOMIC_64B 0
#endif

// Build optimizations control compile-time optimizations to be used by Blend2D
// and C++ compiler. These optimizations are not related to the code-generator
// optimizations (JIT) that are always auto-detected at runtime.
#if defined(BL_BUILD_OPT_AVX2) && !defined(BL_BUILD_OPT_AVX)
  #define BL_BUILD_OPT_AVX
#endif
#if defined(BL_BUILD_OPT_AVX) && !defined(BL_BUILD_OPT_SSE4_2)
  #define BL_BUILD_OPT_SSE4_2
#endif
#if defined(BL_BUILD_OPT_SSE4_2) && !defined(BL_BUILD_OPT_SSE4_1)
  #define BL_BUILD_OPT_SSE4_1
#endif
#if defined(BL_BUILD_OPT_SSE4_1) && !defined(BL_BUILD_OPT_SSSE3)
  #define BL_BUILD_OPT_SSSE3
#endif
#if defined(BL_BUILD_OPT_SSSE3) && !defined(BL_BUILD_OPT_SSE3)
  #define BL_BUILD_OPT_SSE3
#endif
#if defined(BL_BUILD_OPT_SSE3) && !defined(BL_BUILD_OPT_SSE2)
  #define BL_BUILD_OPT_SSE2
#endif

#if defined(__AVX2__)
  #define BL_TARGET_OPT_AVX2
#endif
#if defined(BL_TARGET_OPT_AVX2) || defined(__AVX__)
  #define BL_TARGET_OPT_AVX
#endif
#if defined(BL_TARGET_OPT_AVX) || defined(__SSE4_2__)
  #define BL_TARGET_OPT_SSE4_2
#endif
#if defined(BL_TARGET_OPT_SSE4_2) || defined(__SSE4_1__)
  #define BL_TARGET_OPT_SSE4_1
#endif
#if defined(BL_TARGET_OPT_SSE4_1) || defined(__SSSE3__)
  #define BL_TARGET_OPT_SSSE3
#endif
#if defined(BL_TARGET_OPT_SSSE3) || defined(__SSE3__)
  #define BL_TARGET_OPT_SSE3
#endif
#if defined(BL_TARGET_OPT_SSE3) || (BL_TARGET_ARCH_X86 == 64 || defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
  #define BL_TARGET_OPT_SSE2
#endif
#if defined(BL_TARGET_OPT_SSE2) || (BL_TARGET_ARCH_X86 == 64 || defined(__SSE__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1))
  #define BL_TARGET_OPT_SSE
#endif

#if defined(BL_TARGET_OPT_SSE4_2) || defined(__POPCNT__)
  #define BL_TARGET_OPT_POPCNT
#endif

#if BL_TARGET_ARCH_ARM && defined(__ARM_NEON__)
  #define BL_TARGET_OPT_NEON
#endif

// ============================================================================
// [Build - Configuration Autodetection]
// ============================================================================

// Don't build PipeGen at all if the host architecture doesn't support
// instruction sets required.
#if !defined(BL_BUILD_NO_JIT) && BL_TARGET_ARCH_X86 == 0
  #define BL_BUILD_NO_JIT
#endif

// Make sure we build either PipeGen or FixedPipe, but not both.
#if defined(BL_BUILD_NO_JIT)
  #if defined(BL_BUILD_NO_FIXED_PIPE)
    #undef BL_BUILD_NO_FIXED_PIPE
  #endif
#else
  #if !defined(BL_BUILD_NO_FIXED_PIPE)
    #define BL_BUILD_NO_FIXED_PIPE
  #endif
#endif

// ============================================================================
// [Build - Tests]
// ============================================================================

//! \cond NEVER

// Make sure '#ifdef'ed unit tests are not disabled by IDE.
#ifndef BL_TEST
  #if defined(__INTELLISENSE__)
    #define BL_TEST
  #endif
#endif

// Include a unit testing package if this is a `blend2d_test_unit` build.
#if defined(BL_TEST)
  #include "../../test/broken.h"
#endif

//! \endcond

// ============================================================================
// [Build - Export]
// ============================================================================

//! \cond INTERNAL

//! Export mode is on when `BL_BUILD_EXPORT` is defined - this MUST be defined
//! before including any other header as "api.h" uses `BL_BUILD_EXPORT` to
//! define a proper `BL_API` decorator that is used by all exported functions
//! and variables.
#define BL_BUILD_EXPORT

//! \endcond

// ============================================================================
// [Dependencies]
// ============================================================================

#include "./api.h"
#include "./api-impl.h"
#include "./api-internal_p.h"

#endif // BLEND2D_API_BUILD_P_H
