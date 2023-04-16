// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_API_INTERNAL_P_H_INCLUDED
#define BLEND2D_API_INTERNAL_P_H_INCLUDED

#include "api.h"
#include "api-impl.h"
#include "object.h"

// C Headers
// =========

// NOTE: Some headers are already included by <api.h>. This should be useful for creating an overview of what
// Blend2D really needs globally to be included.
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// C++ Headers
// ===========

// We are just fine with <math.h>, however, there are some useful overloads in C++'s <cmath> that are nicer to use
// than those in <math.h>. Mostly low-level functionality like blIsFinite() relies on <cmath> instead of <math.h>.
#include <cmath>
#include <limits>
#include <type_traits>

// Platform Specific Headers
// =========================

#ifdef _WIN32
  //! \cond NEVER
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  //! \endcond

  #include <windows.h>   // Required to build Blend2D on Windows platform.
  #include <synchapi.h>  // Synchronization primitivess.
#else
  #include <errno.h>     // Need to access it in some cases.
  #include <pthread.h>   // Required to build Blend2D on POSIX compliant platforms.
  #include <unistd.h>    // Filesystem, sysconf, etc...
#endif

// Some intrinsics defined by MSVC compiler are useful and used across the library.
#ifdef _MSC_VER
  #include <intrin.h>
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_api_globals
//! \{

// C++ Compiler Support
// ====================

#if defined(__clang__)
  #define BL_CLANG_AT_LEAST(MAJOR, MINOR) ((MAJOR > __clang_major__) || (MAJOR == __clang_major__ && MINOR >= __clang_minor__))
#else
  #define BL_CLANG_AT_LEAST(MAJOR, MINOR) 0
#endif

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

#if __cplusplus >= 201703L
  #define BL_FALLTHROUGH [[fallthrough]];
#elif defined(__GNUC__) && __GNUC__ >= 7
  #define BL_FALLTHROUGH __attribute__((fallthrough));
#else
  #define BL_FALLTHROUGH /* fallthrough */
#endif

//! \def BL_STDCXX_VERSION
//!
//! PROBLEM: On Linux the default C++ standard library is called `libstdc++` and comes with GCC. Clang can also use
//! this standard library and in many cases it is configured to do so, however, the standard library can be older
//! (and thus provide less features) than the C++ version reported by the compiler via `__cplusplus` macro. This means
//! that the `__cplusplus` version doesn't correspond to the C++ standard library version!
//!
//! The problem is that `libstdc++` doesn't provide any version information, but a timestamp, which is unreliable if
//! you use other compiler than GCC. Since we only use C++14 and higher optionally we don't have a problem to detect
//! such case and to conditionally disable C++14 and higher features of the standard  C++ library.
#if defined(__GLIBCXX__) && defined(__clang__)
  #if __has_include(<string_view>) && __cplusplus >= 201703L
    #define BL_STDCXX_VERSION 201703L
  #elif __has_include(<shared_mutex>) && __cplusplus >= 201402L
    #define BL_STDCXX_VERSION 201402L
  #else
    #define BL_STDCXX_VERSION 201103L
  #endif
#else
  #define BL_STDCXX_VERSION __cplusplus
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
  #define BL_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
  #define BL_NOINLINE __declspec(noinline)
#else
  #define BL_NOINLINE
#endif

//! \def BL_INLINE_NODEBUG
//!
//! The same as `BL_INLINE` combined with `__attribute__((artificial))`.
#if defined(__clang__)
  #define BL_INLINE_NODEBUG inline __attribute__((__always_inline__, __nodebug__))
#elif defined(__GNUC__)
  #define BL_INLINE_NODEBUG inline __attribute__((__always_inline__, __artificial__))
#else
  #define BL_INLINE_NODEBUG BL_INLINE
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
#if defined(__INTEL_COMPILER)
  #define BL_NOUNROLL __pragma(nounroll)
#elif defined(__clang__) && BL_CLANG_AT_LEAST(3, 6)
  #define BL_NOUNROLL _Pragma("nounroll")
#elif defined(__GNUC__) && (__GNUC__ >= 8)
  #define BL_NOUNROLL _Pragma("GCC unroll 1")
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
//! properly.
#define BL_API_IMPL extern "C" BL_API

#define BL_STRINGIFY_WRAP(N) #N
#define BL_STRINGIFY(N) BL_STRINGIFY_WRAP(N)

#define BL_STATIC_ASSERT(...) static_assert(__VA_ARGS__, "Failed BL_STATIC_ASSERT(" #__VA_ARGS__ ")")

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
  #define BL_NOT_REACHED()                                                    \
    do {                                                                      \
      blRuntimeAssertionFailure(__FILE__, __LINE__,                           \
                                "Unreachable code-path reached");             \
    } while (0)
#else
  #define BL_NOT_REACHED() BL_ASSUME(0)
#endif

#define BL_RESTRICT

#define BL_ARRAY_SIZE(X) uint32_t(sizeof(X) / sizeof(X[0]))
#define BL_OFFSET_OF(STRUCT, MEMBER) ((int)(offsetof(STRUCT, MEMBER)))

#define BL_PROPAGATE_(exp, cleanup)                                           \
  do {                                                                        \
    BLResult _resultToPropagate = (exp);                                      \
    if (BL_UNLIKELY(_resultToPropagate != BL_SUCCESS)) {                      \
      cleanup                                                                 \
      return _resultToPropagate;                                              \
    }                                                                         \
  } while (0)

//! Like BL_PROPAGATE, but propagates everything except `BL_RESULT_NOTHING`.
#define BL_PROPAGATE_IF_NOT_NOTHING(...)                                      \
  do {                                                                        \
    BLResult resultToPropagate = (__VA_ARGS__);                               \
    if (resultToPropagate != BL_RESULT_NOTHING)                               \
      return resultToPropagate;                                               \
  } while (0)

//! Decorates a base class that has virtual functions.
#define BL_OVERRIDE_NEW_DELETE(TYPE)                                          \
  BL_INLINE void* operator new(size_t n) noexcept { return malloc(n); }       \
  BL_INLINE void  operator delete(void* p) noexcept { if (p) free(p); }       \
                                                                              \
  BL_INLINE void* operator new(size_t, const BLInternal::PlacementNew& p) noexcept { return p.ptr; } \
  BL_INLINE void  operator delete(void*, void*) noexcept {}

//! \def BL_DEFINE_ENUM_FLAGS(T)
//!
//! Defines operations for enumeration flags.
#ifdef _DOXYGEN
  #define BL_DEFINE_ENUM_FLAGS(T)
#else
  #define BL_DEFINE_ENUM_FLAGS(T)                                                             \
    static BL_INLINE_NODEBUG constexpr T operator~(T a) noexcept {                            \
      return T(~(std::underlying_type<T>::type)(a));                                          \
    }                                                                                         \
                                                                                              \
    static BL_INLINE_NODEBUG constexpr T operator|(T a, T b) noexcept {                       \
      return T((std::underlying_type<T>::type)(a) |                                           \
              (std::underlying_type<T>::type)(b));                                            \
    }                                                                                         \
    static BL_INLINE_NODEBUG constexpr T operator&(T a, T b) noexcept {                       \
      return T((std::underlying_type<T>::type)(a) &                                           \
              (std::underlying_type<T>::type)(b));                                            \
    }                                                                                         \
    static BL_INLINE_NODEBUG constexpr T operator^(T a, T b) noexcept {                       \
      return T((std::underlying_type<T>::type)(a) ^                                           \
              (std::underlying_type<T>::type)(b));                                            \
    }                                                                                         \
                                                                                              \
    static BL_INLINE_NODEBUG T& operator|=(T& a, T b) noexcept {                              \
      a = T((std::underlying_type<T>::type)(a) |                                              \
            (std::underlying_type<T>::type)(b));                                              \
      return a;                                                                               \
    }                                                                                         \
    static BL_INLINE_NODEBUG T& operator&=(T& a, T b) noexcept {                              \
      a = T((std::underlying_type<T>::type)(a) &                                              \
            (std::underlying_type<T>::type)(b));                                              \
      return a;                                                                               \
    }                                                                                         \
    static BL_INLINE_NODEBUG T& operator^=(T& a, T b) noexcept {                              \
      a = T((std::underlying_type<T>::type)(a) ^                                              \
            (std::underlying_type<T>::type)(b));                                              \
      return a;                                                                               \
    }
#endif

//! \def BL_DEFINE_STRONG_TYPE(C, T)
//!
//! Defines a strong type `C` that wraps a value of `T`.
#define BL_DEFINE_STRONG_TYPE(C, T)                                                           \
struct C {                                                                                    \
  T _v;                                                                                       \
                                                                                              \
  BL_INLINE_NODEBUG C() = default;                                                            \
  BL_INLINE_NODEBUG constexpr explicit C(T x) noexcept : _v(x) {}                             \
  BL_INLINE_NODEBUG constexpr C(const C& other) noexcept = default;                           \
                                                                                              \
  BL_INLINE_NODEBUG constexpr T value() const noexcept { return _v; }                         \
                                                                                              \
  BL_INLINE_NODEBUG T* valuePtr() noexcept { return &_v; }                                    \
  BL_INLINE_NODEBUG const T* valuePtr() const noexcept { return &_v; }                        \
                                                                                              \
  BL_INLINE_NODEBUG C& operator=(T x) noexcept { _v = x; return *this; };                     \
  BL_INLINE_NODEBUG C& operator=(const C& x) noexcept = default;                              \
                                                                                              \
  BL_INLINE_NODEBUG constexpr C operator+(T x) const noexcept { return C(_v + x); }           \
  BL_INLINE_NODEBUG constexpr C operator-(T x) const noexcept { return C(_v - x); }           \
  BL_INLINE_NODEBUG constexpr C operator*(T x) const noexcept { return C(_v * x); }           \
  BL_INLINE_NODEBUG constexpr C operator/(T x) const noexcept { return C(_v / x); }           \
                                                                                              \
  BL_INLINE_NODEBUG constexpr C operator+(const C& x) const noexcept { return C(_v + x._v); } \
  BL_INLINE_NODEBUG constexpr C operator-(const C& x) const noexcept { return C(_v - x._v); } \
  BL_INLINE_NODEBUG constexpr C operator*(const C& x) const noexcept { return C(_v * x._v); } \
  BL_INLINE_NODEBUG constexpr C operator/(const C& x) const noexcept { return C(_v / x._v); } \
                                                                                              \
  BL_INLINE_NODEBUG C& operator+=(T x) noexcept { _v += x; return *this; }                    \
  BL_INLINE_NODEBUG C& operator-=(T x) noexcept { _v -= x; return *this; }                    \
  BL_INLINE_NODEBUG C& operator*=(T x) noexcept { _v *= x; return *this; }                    \
  BL_INLINE_NODEBUG C& operator/=(T x) noexcept { _v /= x; return *this; }                    \
                                                                                              \
  BL_INLINE_NODEBUG C& operator+=(const C& x) noexcept { _v += x._v; return *this; }          \
  BL_INLINE_NODEBUG C& operator-=(const C& x) noexcept { _v -= x._v; return *this; }          \
  BL_INLINE_NODEBUG C& operator*=(const C& x) noexcept { _v *= x._v; return *this; }          \
  BL_INLINE_NODEBUG C& operator/=(const C& x) noexcept { _v /= x._v; return *this; }          \
                                                                                              \
  BL_INLINE_NODEBUG bool operator==(T x) const noexcept { return _v == x; }                   \
  BL_INLINE_NODEBUG bool operator!=(T x) const noexcept { return _v != x; }                   \
  BL_INLINE_NODEBUG bool operator> (T x) const noexcept { return _v >  x; }                   \
  BL_INLINE_NODEBUG bool operator>=(T x) const noexcept { return _v >= x; }                   \
  BL_INLINE_NODEBUG bool operator< (T x) const noexcept { return _v <  x; }                   \
  BL_INLINE_NODEBUG bool operator<=(T x) const noexcept { return _v <= x; }                   \
                                                                                              \
  BL_INLINE_NODEBUG bool operator==(const C& x) const noexcept { return _v == x._v; }         \
  BL_INLINE_NODEBUG bool operator!=(const C& x) const noexcept { return _v != x._v; }         \
  BL_INLINE_NODEBUG bool operator> (const C& x) const noexcept { return _v >  x._v; }         \
  BL_INLINE_NODEBUG bool operator>=(const C& x) const noexcept { return _v >= x._v; }         \
  BL_INLINE_NODEBUG bool operator< (const C& x) const noexcept { return _v <  x._v; }         \
  BL_INLINE_NODEBUG bool operator<=(const C& x) const noexcept { return _v <= x._v; }         \
};

// Internal Macros
// ===============

#define BL_RETURN_ERROR_IF_NULL(ptr)               \
  do {                                             \
    if (!(ptr))                                    \
      return blTraceError(BL_ERROR_OUT_OF_MEMORY); \
  } while (0)

#define BL_RETURN_ERROR_IF_NULL_(ptr, ...)         \
  do {                                             \
    if (!(ptr)) {                                  \
      __VA_ARGS__                                  \
      return blTraceError(BL_ERROR_OUT_OF_MEMORY); \
    }                                              \
  } while (0)

// Internal Types
// ==============

//! A type used to store a pack of bits (typedef to `uintptr_t`).
//!
//! BitWord should be equal in size to a machine word.
typedef uintptr_t BLBitWord;

// Internal Constants
// ==================

//! To make checks for APPEND operation easier.
static constexpr BLModifyOp BL_MODIFY_OP_APPEND_START = BLModifyOp(2);
//! Mask that can be used to check whether `BLModifyOp` has a grow hint.
static constexpr BLModifyOp BL_MODIFY_OP_GROW_MASK = BLModifyOp(1);

static BL_INLINE constexpr bool blModifyOpIsAssign(BLModifyOp modifyOp) noexcept { return modifyOp < BL_MODIFY_OP_APPEND_START; }
static BL_INLINE constexpr bool blModifyOpIsAppend(BLModifyOp modifyOp) noexcept { return modifyOp >= BL_MODIFY_OP_APPEND_START; }
static BL_INLINE constexpr bool blModifyOpDoesGrow(BLModifyOp modifyOp) noexcept { return (modifyOp & BL_MODIFY_OP_GROW_MASK) != 0; }

//! Internal constants and limits used across the library.
enum : uint32_t {
  // Target CPU Properties Known at Compile Time
  // -------------------------------------------

  //! Size of a CPU cache-line or a minimum size if multiple CPUs are used.
  //!
  //! Mostly depends on architecture, we use 64 bytes by default.
  BL_CACHE_LINE_SIZE = 64,

  // Blend2D Limits and System Allocator Properties
  // ----------------------------------------------

  //! Host memory allocator overhead (estimated).
  BL_ALLOC_OVERHEAD = uint32_t(sizeof(void*)) * 4,
  //! Host memory allocator alignment (can be lower than reality, but cannot be higher).
  BL_ALLOC_ALIGNMENT = 8,

  //! Limits doubling of a container size after the limit size [in bytes] has reached 8MB. The container will
  //! use a more conservative approach after the threshold has been reached.
  BL_ALLOC_GROW_LIMIT = 1 << 23,

  // Alloc Hints Used by Blend2D Containers
  // --------------------------------------

  //! Minimum vertices to amortize the check of a matrix type.
  BL_MATRIX_TYPE_MINIMUM_SIZE = 16,

  //! Maximum number of faces per a single font collection.
  BL_FONT_DATA_MAX_FACE_COUNT = 256,

  //! BLResult value that is used internally to signalize that the function didn't succeed, but also didn't fail.
  //! This is not an error state. At the moment this is only used by `BLPixelConverter` when setting up optimized
  //! conversion functions.
  //!
  //! \note This result code can be never propagated to the user code!
  BL_RESULT_NOTHING = 0xFFFFFFFFu
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

// Internal C++ Functions
// ======================

//! Used to silence warnings about unused arguments or variables.
template<typename... Args>
static BL_INLINE void blUnused(Args&&...) noexcept {}

template<typename T>
static BL_INLINE constexpr bool blTestFlag(const T& x, const T& y) noexcept {
  return ((typename std::underlying_type<T>::type)(x & y)) != 0;
}

// TODO: Remove.
template<typename T, typename F>
static BL_INLINE void blAssignFunc(T** dst, F f) noexcept { *(void**)dst = (void*)f; }

// Miscellaneous Internals
// =======================

//! Checks whether `dataAccessFlags` is valid.
static BL_INLINE bool blDataAccessFlagsIsValid(uint32_t dataAccessFlags) noexcept {
  return dataAccessFlags == BL_DATA_ACCESS_READ ||
         dataAccessFlags == BL_DATA_ACCESS_RW;
}

static BL_INLINE void blPrefetchW(const void* p) { (void)p; }

//! \}
//! \endcond

#endif // BLEND2D_API_INTERNAL_P_H_INCLUDED
