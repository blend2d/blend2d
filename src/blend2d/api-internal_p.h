// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_API_INTERNAL_P_H
#define BLEND2D_API_INTERNAL_P_H

// ============================================================================
// [Dependencies]
// ============================================================================

#include "./api.h"
#include "./api-impl.h"
#include "./variant.h"

// C Headers
// ---------

// NOTE: Some headers are already included by <api.h>. This should be useful
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
#include <new>
#include <cmath>
#include <limits>

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

  #include <windows.h>   // Required to build Blend2D on Windows platform.
  #include <synchapi.h>  // Synchronization primitivess.
#else
  #include <errno.h>     // Need to access it in some cases.
  #include <pthread.h>   // Required to build Blend2D on POSIX compliant platforms.
  #include <unistd.h>    // Filesystem, sysconf, etc...
#endif

// Some intrinsics defined by MSVC compiler are useful. Most of them should be
// used by "blsupport_p.h" that provides a lot of support functions used across
// the library.
#ifdef _MSC_VER
  #include <intrin.h>
#endif

// ============================================================================
// [Compiler Macros]
// ============================================================================

#if defined(__clang__)
  #define BL_CLANG_AT_LEAST(MAJOR, MINOR) ((MAJOR > __clang_major__) || (MAJOR == __clang_major__ && MINOR >= __clang_minor__))
#else
  #define BL_CLANG_AT_LEAST(MAJOR, MINOR) 0
#endif

// Some compilers won't optimize our stuff if we won't tell them. However, we
// have to be careful as Blend2D doesn't use fast-math by default. So when
// applicable we turn some optimizations on and off locally, but not globally.
//
// This has also a lot of downsides. For example all wrappers in "std" namespace
// are compiled with default flags so even when we force the compiler to follow
// certain behavior it would only work if we use the "original" functions and
// not those wrapped in `std` namespace (so use floor and not std::floor, etc).
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

// PROBLEM: On Linux the default C++ standard library is called `libstdc++` and
// comes with GCC. Clang can also use this standard library and in many cases
// it is configured to do so, however, the standard library can be older (and
// thus provide less features) than the C++ version reported by the compiler via
// `__cplusplus` macro. This means that the `__cplusplus` version doesn't
// correspond to the C++ standard library version!
//
// The problem is that `libstdc++` doesn't provide any version information, but
// a timestamp, which is unreliable if you use other compiler than GCC. Since we
// only use C++14 and higher optionally we don't have a problem to detect such
// case and to conditionally disable C++14 and higher features of the standard
// C++ library.
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

#if defined(__INTEL_COMPILER)
  #define BL_NOUNROLL __pragma(nounroll)
#elif defined(__clang__) && BL_CLANG_AT_LEAST(3, 6)
  #define BL_NOUNROLL _Pragma("nounroll")
#elif defined(__GNUC__) && (__GNUC__ >= 8)
  #define BL_NOUNROLL _Pragma("GCC unroll 1")
#else
  #define BL_NOUNROLL
#endif

#define for_nounroll BL_NOUNROLL for
#define while_nounroll BL_NOUNROLL while

//! Decorates a base class that has virtual functions.
#define BL_OVERRIDE_NEW_DELETE(TYPE)                                          \
  BL_INLINE void* operator new(size_t n) noexcept { return malloc(n); }       \
  BL_INLINE void  operator delete(void* p) noexcept { if (p) free(p); }       \
                                                                              \
  BL_INLINE void* operator new(size_t, void* p) noexcept { return p; }        \
  BL_INLINE void  operator delete(void*, void*) noexcept {}

//! Like BL_PROPAGATE, but propagates everything except `BL_RESULT_NOTHING`.
#define BL_PROPAGATE_IF_NOT_NOTHING(...)                                      \
  do {                                                                        \
    BLResult resultToPropagate = (__VA_ARGS__);                               \
    if (resultToPropagate != BL_RESULT_NOTHING)                               \
      return resultToPropagate;                                               \
  } while (0)

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct BLRuntimeContext;

// ============================================================================
// [Internal Constants]
// ============================================================================

//! Internal constants and limits used across the library.
enum BLInternalConsts : uint32_t {
  //! BLResult value that is used internally to signalize that the function
  //! didn't succeed, but also didn't fail. This is not an error state.
  //!
  //! At the moment this is only used by `BLPixelConverter` when setting up
  //! optimized conversion functions.
  //!
  //! \note This result code can be never propagated to the user code!
  BL_RESULT_NOTHING = 0xFFFFFFFFu,

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

  //! Size of a CPU cache-line or a minimum size if multiple CPUs are used.
  //!
  //! Mostly depends on architecture, we use 64 bytes by default.
  BL_CACHE_LINE_SIZE = 64,

  //! To make checks for APPEND operation easier.
  BL_MODIFY_OP_APPEND_START = 2,
  //! Mask that can be used to check whether `BLModifyOp` has a grow hint.
  BL_MODIFY_OP_GROW_MASK = 0x1,

  //! Minimum vertices to amortize the check of a matrix type.
  BL_MATRIX_TYPE_MINIMUM_SIZE = 16,

  //! Maximum number of faces per a single font collection.
  BL_FONT_DATA_MAX_FACE_COUNT = 256

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

//! Checks whether `dataAccessFlags` is valid.
static BL_INLINE bool blDataAccessFlagsIsValid(uint32_t dataAccessFlags) noexcept {
  return dataAccessFlags == BL_DATA_ACCESS_READ ||
         dataAccessFlags == BL_DATA_ACCESS_RW;
}

//! Initializes the built-in 'none' implementation.
template<typename T>
static BL_INLINE void blInitBuiltInNull(T* impl, uint32_t implType, uint32_t implTraits) noexcept {
  impl->refCount = SIZE_MAX;
  impl->implType = uint8_t(implType);
  impl->implTraits = uint8_t(implTraits | BL_IMPL_TRAIT_NULL);
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
  #if defined(_MSC_VER) && !defined(__clang__)
  BL_ASSUME(&t != nullptr);
  #endif

  new(&t) T();
}

template<typename T, typename... Args>
static BL_INLINE void blCallCtor(T& t, Args&&... args) noexcept {
  // Only needed by MSVC as otherwise it could generate null-pointer check
  // before calling the constructor (as null pointer is allowed). GCC and
  // clang can emit a "-Wtautological-undefined-compare" warning and that's
  // the main reason it's only enabled for MSVC.
  #if defined(_MSC_VER) && !defined(__clang__)
  BL_ASSUME(&t != nullptr);
  #endif

  new(&t) T(std::forward<Args>(args)...);
}

template<typename T>
static BL_INLINE void blCallDtor(T& t) noexcept {
  t.~T();
}

//! \}
//! \endcond

#endif // BLEND2D_API_INTERNAL_P_H
