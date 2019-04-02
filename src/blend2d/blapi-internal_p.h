// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLAPI_INTERNAL_P_H
#define BLEND2D_BLAPI_INTERNAL_P_H

// ============================================================================
// [Dependencies]
// ============================================================================

#include "./blapi.h"
#include "./blapi-impl.h"
#include "./blvariant.h"

// C Headers
// ---------

// NOTE: Some headers are already included by <blapi.h>. This should be useful
// for creating an overview of what Blend2D really needs globally to be included.
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// C++ Headers
// -----------

// We are just fine with <math.h>, however, there are some useful overloads in
// C++'s <cmath> that are nicer to use than those in <math.h>. Mostly low level
// functionality like blIsFinite() relies on <cmath> instead of <math.h>.
#include <cmath>
#include <limits>
#include <new>

// Platform Specific Headers
// -------------------------

#ifdef _WIN32
  //! \cond NEVER
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  //! \endcond

  // <windows.h> is required to build Blend2D on Windows platform.
  #include <windows.h>
  #include <synchapi.h>
#else
  // <pthread.h> is required to build Blend2D on POSIX compliant platforms.
  #include <pthread.h>
#endif

// Some intrinsics defined by MSVC compiler are useful. Most of them should be
// used by "blsupport_p.h" that provides a lot of support functions used across
// the library.
#ifdef _MSC_VER
  #include <intrin.h>
#endif

// ============================================================================
// [Build Architecture & Optimizations]
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

#if BL_TARGET_ARCH_ARM && defined(__ARM_NEON__)
  #define BL_TARGET_OPT_NEON
#endif

// ============================================================================
// [Compiler Macros]
// ============================================================================

// Some compilers won't optimize our stuff if we won't tell them. However, we
// have to be careful as Blend2D doesn't use fast-math by default. So when
// applicable we turn some optimizations on and off locally, but not globally.
//
// This has also a lot of downsides. For example all wrappers in "std" namespace
// are compiled with default flags so even when we force the compiler to follow
// certain behavior it would only work if we use the "original" functions and
// not those wrapped in `std` namespace (so use floor and not std::floor, etc).
#if defined(_MSC_VER)
  #define BL_PRAGMA_FAST_MATH_PUSH __pragma(float_control(precise, off, push))
  #define BL_PRAGMA_FAST_MATH_POP __pragma(float_control(pop))
#else
  #define BL_PRAGMA_FAST_MATH_PUSH
  #define BL_PRAGMA_FAST_MATH_POP
#endif

// Diagnostic warnings can be turned on/off by using pragmas, however, this is
// a compiler specific stuff we have to maintain for each compiler. Ideally we
// should have a clean code that would compile without any warnings with all of
// them enabled by default, but since there is a lot of nitpicks we just disable
// some locally when needed (like unused parameter in null-impl functions, etc).
#if defined(__INTEL_COMPILER)
  // Not regularly tested.
#elif defined(_MSC_VER)
  #define BL_DIAGNOSTIC_PUSH(...) __pragma(warning(push)) __VA_ARGS__
  #define BL_DIAGNOSTIC_POP __pragma(warning(pop))
  #define BL_DIAGNOSTIC_NO_INVALID_OFFSETOF
  #define BL_DIAGNOSTIC_NO_UNUSED_FUNCTIONS
  #define BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS __pragma(warning(disable: 4100))
#elif defined(__clang__)
  #define BL_DIAGNOSTIC_PUSH(...) _Pragma("clang diagnostic push") __VA_ARGS__
  #define BL_DIAGNOSTIC_POP _Pragma("clang diagnostic pop")
  #define BL_DIAGNOSTIC_NO_INVALID_OFFSETOF _Pragma("clang diagnostic ignored \"-Winvalid-offsetof\"")
  #define BL_DIAGNOSTIC_NO_UNUSED_FUNCTIONS _Pragma("clang diagnostic ignored \"-Wunused-function\"")
  #define BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS _Pragma("clang diagnostic ignored \"-Wunused-parameter\"")
#elif defined(__GNUC__)
  #define BL_DIAGNOSTIC_PUSH(...) _Pragma("GCC diagnostic push") __VA_ARGS__
  #define BL_DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")
  #define BL_DIAGNOSTIC_NO_INVALID_OFFSETOF _Pragma("GCC diagnostic ignored \"-Winvalid-offsetof\"")
  #define BL_DIAGNOSTIC_NO_UNUSED_FUNCTIONS _Pragma("GCC diagnostic ignored \"-Wunused-function\"")
  #define BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")
#endif

#if !defined(BL_DIAGNOSTIC_PUSH)
  #define BL_DIAGNOSTIC_PUSH(...)
  #define BL_DIAGNOSTIC_POP
  #define BL_DIAGNOSTIC_NO_INVALID_OFFSETOF
  #define BL_DIAGNOSTIC_NO_UNUSED_FUNCTIONS
  #define BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS
#endif

#if defined(__clang__) || defined(__has_attribute__)
  #define BL_CC_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
  #define BL_CC_HAS_ATTRIBUTE(x) 0
#endif

#if __cplusplus >= 201703L
  #define BL_FALLTHROUGH [[fallthrough]];
#else
  #define BL_FALLTHROUGH /* fallthrough */
#endif

// ============================================================================
// [Internal Macros]
// ============================================================================

//! \def BL_HIDDEN
//!
//! Decorates a function that is used across more than one source file, but
//! should never be exported. Expands to a compiler-specific code that affects
//! the visibility.
#if defined(__GNUC__) && !defined(__MINGW32__)
  #define BL_HIDDEN __attribute__((__visibility__("hidden")))
#else
  #define BL_HIDDEN
#endif

//! \def BL_OPTIMIZE
//!
//! Decorates a function that should be highly optimized by C++ compiler. In
//! general Blend2D uses "-O2" optimization level on GCC and Clang, this macro
//! would change the optimization level to "-O3" for the decorated function.
#if !defined(BL_BUILD_DEBUG) && (BL_CC_HAS_ATTRIBUTE(__optimize__) || (!defined(__clang__) && defined(__GNUC__)))
  #define BL_OPTIMIZE __attribute__((__optimize__("O3")))
#else
  #define BL_OPTIMIZE
#endif

//! \def BL_NOINLINE
//!
//! Decorates a function that should never be inlined. Sometimes used by Blend2D
//! to decorate functions that are either called rarely or that are called from
//! other code in corner cases - like buffer reallocation, etc...
#if defined(__GNUC__)
  #define BL_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
  #define BL_NOINLINE __declspec(noinline)
#else
  #define BL_NOINLINE
#endif

#define BL_NONCOPYABLE(...)                                                   \
  __VA_ARGS__(const __VA_ARGS__& other) = delete;                             \
  __VA_ARGS__& operator=(const __VA_ARGS__& other) = delete;

#define BL_UNUSED(X) (void)(X)

#define BL_ARRAY_SIZE(X) uint32_t(sizeof(X) / sizeof(X[0]))
#define BL_OFFSET_OF(STRUCT, MEMBER) ((int)(offsetof(STRUCT, MEMBER)))

//! \def BL_NOT_REACHED()
//!
//! Run-time assertion used in code that should never be reached.
#ifdef BL_BUILD_DEBUG
  #define BL_NOT_REACHED()                                                    \
    do {                                                                      \
      blRuntimeAssertionFailure(__FILE__, __LINE__,                           \
                                "Unreachable code-path reached");             \
    } while (0)
#else
  #define BL_NOT_REACHED() BL_ASSUME(0)
#endif

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct BLRuntimeContext;

// ============================================================================
// [Internal Constants]
// ============================================================================

//! Internal constants and limits used across the library.
enum BLInternalConsts : uint32_t {
  // --------------------------------------------------------------------------
  // System allocator properties and some limits used by Blend2D.
  // --------------------------------------------------------------------------

  //! Host memory allocator overhead (estimated).
  BL_ALLOC_OVERHEAD = uint32_t(sizeof(void*)) * 4,
  //! Host memory allocator alignment (must match!).
  BL_ALLOC_ALIGNMENT = 8,

  //! Limits a doubling of a container size after the limit size [in bytes] is
  //! reached [8MB]. After the size is reached the container will grow in [8MB]
  //! chunks.
  BL_ALLOC_GROW_LIMIT = 1 << 23,

  // --------------------------------------------------------------------------
  // Alloc hints are specified in bytes. Each container will be allocated to
  // `BL_ALLOC_HINT_...` bytes initially when a first item is added
  // to it.
  // --------------------------------------------------------------------------

  //! Initial size of BLStringImpl of a newly allocated string [in bytes].
  BL_ALLOC_HINT_STRING = 64,
  //! Initial size of BLArrayImpl of a newly allocated array [in bytes].
  BL_ALLOC_HINT_ARRAY = 128,
  //! Initial size of BLRegionImpl of a newly allocated region [in bytes].
  BL_ALLOC_HINT_REGION = 256,
  //! Initial size of BLPathImpl of a newly allocated path [in bytes].
  BL_ALLOC_HINT_PATH2D = 512,
  //! Initial size of BLGradientImpl of a newly allocated gradient [in bytes].
  BL_ALLOC_HINT_GRADIENT = 256,

  //! To make checks for APPEND operation easier.
  BL_MODIFY_OP_APPEND_START = 2,
  //! Mask that can be used to check whether `BLModifyOp` has a grow hint.
  BL_MODIFY_OP_GROW_MASK = 0x1,

  //! Minimum vertices to amortize the check of a matrix type.
  BL_MATRIX_TYPE_MINIMUM_SIZE = 16,

  //! Maximum number of faces per a single font collection.
  BL_FONT_LOADER_MAX_FACE_COUNT = 256

};

//! Analysis result that describes whether an unknown input is conforming.
enum BLDataAnalysis : uint32_t {
  //! The input data is conforming (stored exactly as expected).
  BL_DATA_ANALYSIS_CONFORMING = 0, // Must be 0
  //! The input data is valid, but non-conforming (must be processed).
  BL_DATA_ANALYSIS_NON_CONFORMING = 1, // Must be 1
  //! The input data contains an invalid value.
  BL_DATA_ANALYSIS_INVALID_VALUE = 2
};

// ============================================================================
// [Internal Functions]
// ============================================================================

template<typename T>
struct BLInternalCastImpl { T Type; };

//! Casts a public `T` type into an internal implementation/data of that type.
//! For example `BLPathImpl` would be casted to `BLInternalPathImpl`. Used
//! by Blend2D as a shortcut to prevent using more verbose `static_cast<>` in
//! code that requires a lot of such casts (in most cases public API which is
//! then casting public interfaces into an internal ones).
template<typename T>
constexpr typename BLInternalCastImpl<T>::Type* blInternalCast(T* something) noexcept {
  return static_cast<typename BLInternalCastImpl<T>::Type*>(something);
}

//! \overload
template<typename T>
constexpr const typename BLInternalCastImpl<T>::Type* blInternalCast(const T* something) noexcept {
  return static_cast<const typename BLInternalCastImpl<T>::Type*>(something);
}

//! Assigns a built-in none implementation `impl` to `blNone` - a built-in
//! array that contains all null instances provided by Blend2D. Any code that
//! assigns to `blNone` array must use this function as it's then easy to find.
template<typename T>
static BL_INLINE void blAssignBuiltInNull(T* impl) noexcept {
  *reinterpret_cast<T**>((void **)blNone + impl->implType) = impl;
}

template<typename T>
static BL_INLINE void blCallCtor(T& t) noexcept {
  // Only needed by MSVC as otherwise it could generate null-pointer check
  // before calling the constructor (as null pointer is allowed). GCC and
  // clang can emit a "-Wtautological-undefined-compare" warning and that's
  // the main reason it's only enabled for MSVC.
  #ifdef _MSC_VER
  BL_ASSUME(&t != nullptr);
  #endif

  new (&t) T();
}

template<typename T>
static BL_INLINE void blCallDtor(T& t) noexcept {
  t.~T();
}

//! \}
//! \endcond

#endif // BLEND2D_BLAPI_INTERNAL_P_H
