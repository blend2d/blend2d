// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_API_INTERNAL_P_H_INCLUDED
#define BLEND2D_CORE_API_INTERNAL_P_H_INCLUDED

#include <blend2d/axl/mem_alloc.h>
#include <blend2d/core/api.h>
#include <blend2d/core/api_impl.h>
#include <blend2d/core/object_layer.h>

// C Headers
// =========

// NOTE: Some headers are already included by <api.h>. This should be useful for creating an overview of what
// Blend2D really needs globally to be included.
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// C++ Headers
// ===========

// We are just fine with <math.h>, however, there are some useful overloads in C++'s <cmath> that are nicer to use
// than those in <math.h>. Mostly low-level functionality like Math::is_finite() relies on <cmath> instead of <math.h>.
#include <cmath>
#include <limits>
#include <type_traits>

// Platform Specific Headers
// =========================

#if defined(_WIN32)
  //! \cond NEVER
  #if !defined(WIN32_LEAN_AND_MEAN)
    #define WIN32_LEAN_AND_MEAN
  #endif
  #if !defined(NOMINMAX)
    #define NOMINMAX
  #endif
  //! \endcond

  #include <windows.h>   // Required to build Blend2D on Windows platform.
  #include <synchapi.h>  // Synchronization primitivess.
#endif

// Some intrinsics defined by MSVC compiler are useful and used across the library.
#ifdef _MSC_VER
  #include <intrin.h>
#endif

//! \cond INTERNAL
//! \addtogroup bl_globals
//! \{

// Build - Target Architecture & Optimizations
// ===========================================

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

#if defined(__wasm64__)
  #define BL_TARGET_ARCH_WASM 64
#elif defined(__wasm32__) || defined(__wasm__)
  #define BL_TARGET_ARCH_WASM 32
#else
  #define BL_TARGET_ARCH_WASM 0
#endif

#if defined(_M_X64) || defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__)
  #define BL_TARGET_ARCH_X86 64
#elif defined(_M_IX86) || defined(__i386) || defined(__i386__)
  #define BL_TARGET_ARCH_X86 32
#else
  #define BL_TARGET_ARCH_X86 0
#endif

#if defined(_M_ARM64) || defined(__ARM64__) || defined(__aarch64__)
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

// Defined when it's safe to assume that std::atomic<uint64_t> would be non locking. We can always assume this on
// X86 architecture even 32-bit as it provides CMPXCHG8B, but we don't assume this on other architectures like ARM.
#if BL_TARGET_ARCH_BITS >= 64 || BL_TARGET_ARCH_X86 != 0
  #define BL_TARGET_HAS_ATOMIC_64B 1
#else
  #define BL_TARGET_HAS_ATOMIC_64B 0
#endif

#if !defined(BL_BUILD_NO_JIT)
  #if BL_TARGET_ARCH_X86 != 0
    #define BL_JIT_ARCH_X86
  #elif BL_TARGET_ARCH_ARM == 64
    #define BL_JIT_ARCH_A64
  #endif
#endif // !BL_BUILD_NO_JIT

// Build optimizations control compile-time optimizations to be used by Blend2D and C++ compiler. These optimizations
// are not related to the code-generator optimizations (JIT) that are always auto-detected at runtime.
#if BL_TARGET_ARCH_X86
  #if defined(BL_BUILD_OPT_AVX512_V2) && !defined(BL_BUILD_OPT_AVX512)
    #define BL_BUILD_OPT_AVX512
  #endif
  #if defined(BL_BUILD_OPT_AVX512) && !defined(BL_BUILD_OPT_AVX2)
    #define BL_BUILD_OPT_AVX2
  #endif
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

  #if !defined(BL_TARGET_OPT_AVX512_V2) && (defined(__AVX512BITALG__)  && defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512VBMI2__) && defined(__AVX512VNNI__) && defined(__GFNI__))
    #define BL_TARGET_OPT_AVX512_V2
  #endif
  #if !defined(BL_TARGET_OPT_AVX512) && (defined(__AVX512F__)  && defined(__AVX512BW__) && defined(__AVX512DQ__) && defined(__AVX512CD__) && defined(__AVX512VL__))
    #define BL_TARGET_OPT_AVX512
  #endif
  #if !defined(BL_TARGET_OPT_AVX2) && (defined(BL_TARGET_OPT_AVX512) || defined(__AVX2__))
    #define BL_TARGET_OPT_AVX2
  #endif
  #if !defined(BL_TARGET_OPT_AVX) && (defined(BL_TARGET_OPT_AVX2) || defined(__AVX__))
    #define BL_TARGET_OPT_AVX
  #endif
  #if !defined(BL_TARGET_OPT_SSE4_2) && (defined(BL_TARGET_OPT_AVX) || defined(__SSE4_2__))
    #define BL_TARGET_OPT_SSE4_2
  #endif
  #if !defined(BL_TARGET_OPT_SSE4_1) && (defined(BL_TARGET_OPT_SSE4_2) || defined(__SSE4_1__))
    #define BL_TARGET_OPT_SSE4_1
  #endif
  #if !defined(BL_TARGET_OPT_SSSE3) && (defined(BL_TARGET_OPT_SSE4_1) || defined(__SSSE3__))
    #define BL_TARGET_OPT_SSSE3
  #endif
  #if !defined(BL_TARGET_OPT_SSE3) && (defined(BL_TARGET_OPT_SSSE3) || defined(__SSE3__))
    #define BL_TARGET_OPT_SSE3
  #endif
  #if !defined(BL_TARGET_OPT_SSE2) && (defined(BL_TARGET_OPT_SSE3) || (BL_TARGET_ARCH_X86 == 64 || defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
    #define BL_TARGET_OPT_SSE2
  #endif
  #if defined(BL_TARGET_OPT_SSE2) || (BL_TARGET_ARCH_X86 == 64 || defined(__SSE__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1))
    #define BL_TARGET_OPT_SSE
  #endif

  #if !defined(BL_TARGET_OPT_BMI2) && defined(__BMI2__)
    #define BL_TARGET_OPT_BMI2
  #endif

  #if !defined(BL_TARGET_OPT_POPCNT) && (defined(__POPCNT__) || defined(BL_TARGET_OPT_SSE4_2))
    #define BL_TARGET_OPT_POPCNT
  #endif
#endif

#if BL_TARGET_ARCH_ARM && (BL_TARGET_ARCH_ARM == 64 || defined(__ARM_NEON__))
  #ifndef BL_TARGET_OPT_ASIMD
    #define BL_TARGET_OPT_ASIMD
  #endif // BL_TARGET_OPT_ASIMD
  #ifndef BL_BUILD_OPT_ASIMD
    #define BL_BUILD_OPT_ASIMD
  #endif // BL_BUILD_OPT_ASIMD
#endif

//! \}
//! \endcond

// C++ Compiler Support
// ====================

// Some compilers won't optimize our stuff if we won't tell them. However, we have to be careful as Blend2D doesn't
// use fast-math by default. So when applicable we turn some optimizations on and off locally, but not globally.
//
// This has also a lot of downsides. For example all wrappers in "std" namespace are compiled with default flags so
// even when we force the compiler to follow certain behavior it would only work if we use the "original" functions
// and not those wrapped in `std` namespace (so use floor and not std::floor, etc).
#if defined(_MSC_VER) && !defined(__clang__)
  #define BL_PRAGMA_FAST_MATH_PUSH __pragma(float_control(precise, off, push))
  #define BL_PRAGMA_FAST_MATH_POP __pragma(float_control(pop))
#else
  #define BL_PRAGMA_FAST_MATH_PUSH
  #define BL_PRAGMA_FAST_MATH_POP
#endif

#if defined(__clang__) || defined(__has_attribute__)
  #define BL_CC_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
  #define BL_CC_HAS_ATTRIBUTE(x) 0
#endif

//! \def BL_HIDDEN
//!
//! Decorates a function that is used across more than one source file, but should never be exported. Expands to
//! a compiler-specific code that affects the visibility.
#if defined(__GNUC__) && !defined(__MINGW32__)
  #define BL_HIDDEN __attribute__((__visibility__("hidden")))
#else
  #define BL_HIDDEN
#endif

//! \def BL_OPTIMIZE
//!
//! Decorates a function that should be highly optimized by C++ compiler. In general Blend2D uses "-O2" optimization
//! level on GCC and Clang, this macro would change the optimization level to "-O3" for the decorated function.
#if !defined(BL_BUILD_DEBUG) && (BL_CC_HAS_ATTRIBUTE(__optimize__) || (!defined(__clang__) && defined(__GNUC__)))
  #define BL_OPTIMIZE __attribute__((__optimize__("O3")))
#else
  #define BL_OPTIMIZE
#endif

//! \def BL_NOINLINE
//!
//! Decorates a function that should never be inlined. Sometimes used by Blend2D to decorate functions that are
//! either called rarely or that are called from other code in corner cases - like buffer reallocation, etc...
#if defined(__GNUC__)
  #define BL_NOINLINE __attribute__((__noinline__))
#elif defined(_MSC_VER)
  #define BL_NOINLINE __declspec(noinline)
#else
  #define BL_NOINLINE
#endif

//! \def BL_FLATTEN
//!
//! Either `__attribute__((flatten))` or nothing, if not supported.
#if defined(__GNUC__)
  #define BL_FLATTEN __attribute__((__flatten__))
#else
  #define BL_FLATTEN
#endif

//! \def BL_STDCALL
//!
//! Calling convention used by Windows.
#if defined(__GNUC__) && defined(__i386__) && !defined(__x86_64__)
  #define BL_STDCALL __attribute__((__stdcall__))
#elif defined(_MSC_VER)
  #define BL_STDCALL __stdcall
#else
  #define BL_STDCALL
#endif

//! \def BL_OPTIMIZED_CALL
//!
//! Optimized calling convention used internally in some places.
#define BL_OPTIMIZED_CALL

//! \def BL_UNALIGNED_TYPE(TYPE, ALIGNMENT)
//!
//! Defines a type, that is aligned to less than its native alignment. In general this is not supported by the
//! C++ alignas() keyword, which only allows to increase the alignment. Compilers that support unaligned types
//! can generate nicer code when such type is accessed and UBSAN would not complain about such accesses, if
//! properly annotated.
#if defined(__GNUC__)
  #define BL_UNALIGNED_TYPE(TYPE, ALIGNMENT) __attribute__((__aligned__(ALIGNMENT))) __attribute__((__may_alias__)) TYPE
#elif defined(_MSC_VER)
  #define BL_UNALIGNED_TYPE(TYPE, ALIGNMENT) __declspec(align(ALIGNMENT)) TYPE
#else
  #define BL_UNALIGNED_TYPE(TYPE, ALIGNMENT) TYPE
#endif

//! \def BL_MAY_ALIAS
//!
//! Expands to `__attribute__((__may_alias__))` if supported.
#if defined(__GNUC__)
  #define BL_MAY_ALIAS __attribute__((__may_alias__))
#else
  #define BL_MAY_ALIAS
#endif

//! \def BL_NOUNROLL
//!
//! Compiler-specific macro that annotates a do/for/while loop to not get unrolled.
#if defined(__clang__)
  #define BL_NOUNROLL _Pragma("nounroll")
#elif defined(__GNUC__) && (__GNUC__ >= 8)
  // NOTE: We could use `_Pragma("GCC unroll 1")`, however, this doesn't apply to all loops and GCC emits a lot of
  // warnings such as "ignoring loop annotation", which cannot be turned off globally nor locally. So we disable
  // loop annotations when compiling with GCC. This comment is here so we can re-enable in the future.
  #define BL_NOUNROLL
#else
  #define BL_NOUNROLL
#endif

//! \def BL_RUNTIME_INITIALIZER
//!
//! Static initialization in C++ is full of surpsises and basically nothing is guaranteed. When Blend2D is compiled
//! as a shared library everything is fine, however, when it's compiled as a static library and user uses statically
//! allocated Blend2D objects then the object may initialize even before the library - this tries to solve that issue.
#if defined(__clang__) || (defined(__GNUC__) && !defined(__APPLE__))
  #define BL_RUNTIME_INITIALIZER __attribute__((init_priority(102)))
#else
  #define BL_RUNTIME_INITIALIZER
#endif

#if defined(BL_BUILD_DEBUG) && !defined(__OPTIMIZE__)
  #define BL_INLINE_IF_NOT_DEBUG
#else
  #define BL_INLINE_IF_NOT_DEBUG BL_INLINE
#endif

//! \def BL_API_IMPL
//!
//! Decorator used to mark all functions and variables that are exported - it expands to "extern C", which ensures
//! that an exported function or variable can be implemented within a private namespace and it would still be exported
//! as C-API.
#define BL_API_IMPL extern "C" BL_API

#define BL_USING_DETAIL(detail_namespace) \
  using namespace bl; \
  using namespace detail_namespace

#define BL_STRINGIFY_WRAP(N) #N
#define BL_STRINGIFY(N) BL_STRINGIFY_WRAP(N)

// Internal C++ Macros
// ===================

//! \def BL_NONCOPYABLE
//!
//! Makes a class noncopyable by making its copy constructor and copy assignment operator deleted.
#define BL_NONCOPYABLE(...)                                                   \
  __VA_ARGS__(const __VA_ARGS__& other) = delete;                             \
  __VA_ARGS__& operator=(const __VA_ARGS__& other) = delete;

//! \def BL_NOT_REACHED()
//!
//! Run-time assertion used in code that should never be reached.
#ifdef BL_BUILD_DEBUG
  #define BL_NOT_REACHED() bl_runtime_assertion_failure(__FILE__, __LINE__, "BL_NOT_REACHED()")
#elif defined(__GNUC__)
  #define BL_NOT_REACHED() __builtin_unreachable()
#else
  #define BL_NOT_REACHED() BL_ASSUME(0)
#endif

#if defined(_MSC_VER)
  #define BL_RESTRICT __restrict
#elif defined(__GNUC__)
  #define BL_RESTRICT __restrict__
#else
  #define BL_RESTRICT
#endif

#define BL_ARRAY_SIZE(X) uint32_t(sizeof(X) / sizeof(X[0]))
#define BL_OFFSET_OF(STRUCT, MEMBER) ((int)(offsetof(STRUCT, MEMBER)))

#define BL_PROPAGATE_(exp, cleanup)                                           \
  do {                                                                        \
    BLResult _result_to_propagate = (exp);                                    \
    if (BL_UNLIKELY(_result_to_propagate != BL_SUCCESS)) {                    \
      cleanup                                                                 \
      return _result_to_propagate;                                            \
    }                                                                         \
  } while (0)

//! Like BL_PROPAGATE, but propagates everything except `BL_RESULT_NOTHING`.
#define BL_PROPAGATE_IF_NOT_NOTHING(...)                                      \
  do {                                                                        \
    BLResult result_to_propagate = (__VA_ARGS__);                             \
    if (result_to_propagate != BL_RESULT_NOTHING) {                           \
      return result_to_propagate;                                             \
    }                                                                         \
  } while (0)

//! Decorates a base class that has virtual functions.
#define BL_OVERRIDE_NEW_DELETE(TYPE)                                             \
  BL_INLINE void* operator new(size_t n) noexcept { return axl::mem_alloc(n); }  \
  BL_INLINE void  operator delete(void* p) noexcept { if (p) axl::mem_free(p); } \
                                                                                 \
  BL_INLINE void* operator new(size_t, const Internal::PlacementNew& p) noexcept { return p.ptr; } \
  BL_INLINE void  operator delete(void*, void*) noexcept {}

//! \def BL_DEFINE_ENUM_FLAGS(T)
//!
//! Defines operations for enumeration flags.
#ifdef _DOXYGEN
  #define BL_DEFINE_ENUM_FLAGS(T)
#else
  #define BL_DEFINE_ENUM_FLAGS(T)                                             \
    static BL_INLINE_CONSTEXPR T operator~(T a) noexcept {                    \
      return T(~std::underlying_type_t<T>(a));                                \
    }                                                                         \
                                                                              \
    static BL_INLINE_CONSTEXPR T operator|(T a, T b) noexcept {               \
      return T(std::underlying_type_t<T>(a) | std::underlying_type_t<T>(b));  \
    }                                                                         \
    static BL_INLINE_CONSTEXPR T operator&(T a, T b) noexcept {               \
      return T(std::underlying_type_t<T>(a) & std::underlying_type_t<T>(b));  \
    }                                                                         \
    static BL_INLINE_CONSTEXPR T operator^(T a, T b) noexcept {               \
      return T(std::underlying_type_t<T>(a) ^ std::underlying_type_t<T>(b));  \
    }                                                                         \
                                                                              \
    static BL_INLINE_CONSTEXPR T& operator|=(T& a, T b) noexcept {            \
      a = T(std::underlying_type_t<T>(a) | std::underlying_type_t<T>(b));     \
      return a;                                                               \
    }                                                                         \
    static BL_INLINE_CONSTEXPR T& operator&=(T& a, T b) noexcept {            \
      a = T(std::underlying_type_t<T>(a) & std::underlying_type_t<T>(b));     \
      return a;                                                               \
    }                                                                         \
    static BL_INLINE_CONSTEXPR T& operator^=(T& a, T b) noexcept {            \
      a = T(std::underlying_type_t<T>(a) ^ std::underlying_type_t<T>(b));     \
      return a;                                                               \
    }
#endif

// Internal Macros
// ===============

#define BL_RETURN_ERROR_IF_NULL(ptr)                \
  do {                                              \
    if (BL_UNLIKELY(!(ptr))) {                      \
      return bl_make_error(BL_ERROR_OUT_OF_MEMORY); \
    }                                               \
  } while (0)

#define BL_RETURN_ERROR_IF_NULL_(ptr, ...)          \
  do {                                              \
    if (BL_UNLIKELY(!(ptr))) {                      \
      __VA_ARGS__                                   \
      return bl_make_error(BL_ERROR_OUT_OF_MEMORY); \
    }                                               \
  } while (0)

// Internal Constants
// ==================

// Target CPU Properties Known at Compile Time
// -------------------------------------------

//! Size of a CPU cache-line or a minimum size if multiple CPUs are used.
//!
//! Mostly depends on architecture, we use 64 bytes by default.
static constexpr uint32_t BL_CACHE_LINE_SIZE = 64;

// Blend2D Limits and System Allocator Properties
// ----------------------------------------------

//! Host memory allocator overhead (estimated).
static constexpr uint32_t BL_ALLOC_OVERHEAD = uint32_t(sizeof(void*)) * 4u;
//! Host memory allocator alignment (can be lower than reality, but cannot be higher).
static constexpr uint32_t BL_ALLOC_ALIGNMENT = 8u;

//! Small number that is always added to a growing allocation. Its purpose is to optimize growing small containers
//! to prevent often reallocations at the beginning and to minimize the risk that something is initially zero and
//! stays zero.
static constexpr uint32_t BL_ALLOC_GROWTH_ADDEND = 64;

//! 50% growth rate of containers after its size in bytes reached 128 KiB.
static constexpr uint32_t BL_ALLOC_GROWTH_LIMIT_50 = 1u << 17;
//! 25% growth rate of containers after its size in bytes reached 4 MiB.
static constexpr uint32_t BL_ALLOC_GROWTH_LIMIT_25 = 1u << 22;

// Alloc Hints Used by Blend2D Containers
// --------------------------------------

//! Minimum vertices to amortize the check of a matrix type.
static constexpr uint32_t BL_MATRIX_TYPE_MINIMUM_SIZE = 16u;

//! Maximum number of faces per a single font collection.
static constexpr uint32_t BL_FONT_DATA_MAX_FACE_COUNT = 256u;

//! BLResult value that is used internally to signalize that the function didn't succeed, but also didn't fail.
//! This is not an error state. At the moment this is only used by \ref BLPixelConverter when setting up optimized
//! conversion functions.
//!
//! \note This result code can be never propagated to the user code!
static constexpr BLResult BL_RESULT_NOTHING = BLResult(0xFFFFFFFFu);

// Internal C++ Structs
// ====================

template<typename T>
struct BLResultOr {
  BLResult _result;
  T _value;

  BL_INLINE_NODEBUG BLResultOr(BLResult result) noexcept
    : _result(result) {}

  BL_INLINE_NODEBUG BLResultOr(BLResult result, T value) noexcept
    : _result(result),
      _value(value) {}

  BL_INLINE_NODEBUG BLResultOr(T value) noexcept
    : _result(BL_SUCCESS),
      _value(value) {}

  BL_INLINE_NODEBUG bool ok() const noexcept { return _result == BL_SUCCESS; }
  BL_INLINE_NODEBUG BLResult result() const noexcept { return _result; }

  BL_INLINE_NODEBUG T& value() noexcept { return _value; }
  BL_INLINE_NODEBUG const T& value() const noexcept { return _value; }

  BL_INLINE_NODEBUG T& operator()() noexcept { return _value; }
  BL_INLINE_NODEBUG const T& operator()() const noexcept { return _value; }
};

// Internal C++ Functions
// ======================

template<typename T>
static BL_INLINE_CONSTEXPR bool bl_test_flag(const T& x, const T& y) noexcept {
  return (std::underlying_type_t<T>(x) & std::underlying_type_t<T>(y)) != std::underlying_type_t<T>(0);
}

// TODO: Remove.
template<typename T, typename F>
static BL_INLINE_NODEBUG void bl_assign_func(T** dst, F f) noexcept { *(void**)dst = (void*)f; }

static BL_INLINE_NODEBUG BLResult bl_combine_result(BLResult r1, BLResult r2) noexcept {
  return BLResult(uint32_t(r1) | uint32_t(r2));
}

// Miscellaneous Internals
// =======================

static BL_INLINE_NODEBUG void bl_prefetch_w(const void* p) { (void)p; }

namespace bl::axl {

BL_INLINE_NODEBUG void* operator_new(size_t n) noexcept { return axl::mem_alloc(n); }
BL_INLINE_NODEBUG void operator_delete(void* p) noexcept { if (p) axl::mem_free(p); }

} // {bl::axl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_API_INTERNAL_P_H_INCLUDED
