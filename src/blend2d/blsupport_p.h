// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLSUPPORT_P_H
#define BLEND2D_BLSUPPORT_P_H

#include "./blapi-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [StdInt]
// ============================================================================

template<typename T>
static constexpr bool blIsUnsigned() noexcept { return std::is_unsigned<T>::value; }

//! Cast an integer `x` to a fixed-width type as defined by <stdint.h>
//!
//! This can help when specializing some functions for a particular type. Since
//! some C/C++ types may overlap (like `long` vs `long long`) it's easier to
//! just cast to a type as defined by <stdint.h> and specialize for it.
template<typename T>
static constexpr typename BLInternal::StdInt<sizeof(T), blIsUnsigned<T>()>::Type blAsStdInt(T x) noexcept {
  return (typename BLInternal::StdInt<sizeof(T), blIsUnsigned<T>()>::Type)x;
}

//! Cast an integer `x` to a fixed-width unsigned type as defined by <stdint.h>
template<typename T>
static constexpr typename BLInternal::StdInt<sizeof(T), 1>::Type blAsStdUInt(T x) noexcept {
  return (typename BLInternal::StdInt<sizeof(T), 1>::Type)x;
}

//! Cast an integer `x` to either `int32_t`, uint32_t`, `int64_t`, or `uint64_t`.
//!
//! Used to keep a signedness of `T`, but to promote it to at least 32-bit type.
template<typename T>
static constexpr typename BLInternal::StdInt<blMax<size_t>(sizeof(T), 4), blIsUnsigned<T>()>::Type blAsIntLeast32(T x) noexcept {
  typedef typename BLInternal::StdInt<blMax<size_t>(sizeof(T), 4), blIsUnsigned<T>()>::Type Result;
  return Result(x);
}

//! Cast an integer `x` to either `uint32_t` or `uint64_t`.
template<typename T>
static constexpr typename BLInternal::StdInt<blMax<size_t>(sizeof(T), 4), 1>::Type blAsUIntLeast32(T x) noexcept {
  typedef typename BLInternal::StdInt<blMax<size_t>(sizeof(T), 4), 1>::Type Result;
  typedef typename std::make_unsigned<T>::type U;
  return Result(U(x));
}

// ============================================================================
// [MisalignedInt]
// ============================================================================

static const constexpr bool BL_UNALIGNED_IO_16 = BL_TARGET_ARCH_X86 != 0;
static const constexpr bool BL_UNALIGNED_IO_32 = BL_TARGET_ARCH_X86 != 0;
static const constexpr bool BL_UNALIGNED_IO_64 = BL_TARGET_ARCH_X86 != 0;

// Type alignment (not allowed by C++11 'alignas' keyword).
#if defined(_MSC_VER)
  #define BL_MISALIGN_TYPE(TYPE, N) __declspec(align(N)) TYPE
#elif defined(__GNUC__)
  #define BL_MISALIGN_TYPE(TYPE, N) __attribute__((__aligned__(N))) TYPE
#else
  #define BL_MISALIGN_TYPE(TYPE, N) TYPE
#endif

//! An integer type that has possibly less alignment than its size.
template<typename T, size_t Alignment>
struct BLMisalignedUInt {};

template<> struct BLMisalignedUInt<uint8_t, 1> { typedef uint8_t T; };
template<> struct BLMisalignedUInt<uint16_t, 1> { typedef uint16_t BL_MISALIGN_TYPE(T, 1); };
template<> struct BLMisalignedUInt<uint16_t, 2> { typedef uint16_t T; };
template<> struct BLMisalignedUInt<uint32_t, 1> { typedef uint32_t BL_MISALIGN_TYPE(T, 1); };
template<> struct BLMisalignedUInt<uint32_t, 2> { typedef uint32_t BL_MISALIGN_TYPE(T, 2); };
template<> struct BLMisalignedUInt<uint32_t, 4> { typedef uint32_t T; };
template<> struct BLMisalignedUInt<uint64_t, 1> { typedef uint64_t BL_MISALIGN_TYPE(T, 1); };
template<> struct BLMisalignedUInt<uint64_t, 2> { typedef uint64_t BL_MISALIGN_TYPE(T, 2); };
template<> struct BLMisalignedUInt<uint64_t, 4> { typedef uint64_t BL_MISALIGN_TYPE(T, 4); };
template<> struct BLMisalignedUInt<uint64_t, 8> { typedef uint64_t T; };

#undef BL_MISALIGN_TYPE

// ============================================================================
// [Numeric Limits]
// ============================================================================

template<typename T> static constexpr T blInf() noexcept { return std::numeric_limits<T>::infinity(); }
template<typename T> static constexpr T blNaN() noexcept { return std::numeric_limits<T>::quiet_NaN(); }
template<typename T> static constexpr T blMinValue() noexcept { return std::numeric_limits<T>::lowest(); }
template<typename T> static constexpr T blMaxValue() noexcept { return std::numeric_limits<T>::max(); }

// ============================================================================
// [Bit Utilities]
// ============================================================================

namespace {

//! Returns `0 - x` in a safe way (no undefined behavior), works for both signed and unsigned numbers.
template<typename T>
constexpr T blNegate(const T& x) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return T(U(0) - U(x));
}

template<typename T>
constexpr uint32_t blBitSizeOf() noexcept { return uint32_t(sizeof(T) * 8u); }

template<typename T>
constexpr size_t blBitWordCountFromBitCount(size_t nBits) noexcept {
  return (nBits + blBitSizeOf<T>() - 1) / blBitSizeOf<T>();
}

//! Bit-cast `x` of `In` type to the given `Out` type.
//!
//! Useful to bit-cast between integers and floating points. The size of `Out`
//! and `In` must be the same otherwise the compilation would fail. Bit casting
//! is used by `blEquals` to implement bit equality for floating point types.
template<typename Out, typename In>
BL_INLINE Out blBitCast(const In& x) noexcept {
  static_assert(sizeof(Out) == sizeof(In), "The size of In and Out must match");
  union { In in; Out out; } u;
  u.in = x;
  return u.out;
}

template<typename T>
constexpr T blBitOnes() noexcept { return T(~T(0)); }

//! Returns `x << y` (shift left logical) by explicitly casting `x` to an unsigned type and back.
template<typename X, typename Y>
constexpr X blBitShl(const X& x, const Y& y) noexcept {
  typedef typename std::make_unsigned<X>::type U;
  return X(U(x) << y);
}

//! Returns `x >> y` (shift right logical) by explicitly casting `x` to an unsigned type and back.
template<typename X, typename Y>
constexpr X blBitShr(const X& x, const Y& y) noexcept {
  typedef typename std::make_unsigned<X>::type U;
  return X(U(x) >> y);
}

//! Returns `x >> y` (shift right arithmetic) by explicitly casting `x` to a signed type and back.
template<typename X, typename Y>
constexpr X blBitSar(const X& x, const Y& y) noexcept {
  typedef typename std::make_signed<X>::type S;
  return X(S(x) >> y);
}

template<typename T>
BL_INLINE T blBitRolImpl(const T& x, unsigned n) noexcept {
  return blBitShl(x, n % unsigned(sizeof(T) * 8u)) | blBitShr(x, (0u - n) % unsigned(sizeof(T) * 8u)) ;
}

template<typename T>
BL_INLINE T blBitRorImpl(const T& x, unsigned n) noexcept {
  return blBitShr(x, n % unsigned(sizeof(T) * 8u)) | blBitShl(x, (0u - n) % unsigned(sizeof(T) * 8u)) ;
}

// MSVC is unable to emit `rol|ror` instruction when `n` is not constant so we
// have to help it a bit. This, however, prevents us from using `constexpr`.
#if defined(_MSC_VER)
template<> BL_INLINE uint8_t blBitRolImpl(const uint8_t& x, unsigned n) noexcept { return uint8_t(_rotl8(x, n)); }
template<> BL_INLINE uint8_t blBitRorImpl(const uint8_t& x, unsigned n) noexcept { return uint8_t(_rotr8(x, n)); }
template<> BL_INLINE uint16_t blBitRolImpl(const uint16_t& x, unsigned n) noexcept { return uint16_t(_rotl16(x, n)); }
template<> BL_INLINE uint16_t blBitRorImpl(const uint16_t& x, unsigned n) noexcept { return uint16_t(_rotr16(x, n)); }
template<> BL_INLINE uint32_t blBitRolImpl(const uint32_t& x, unsigned n) noexcept { return uint32_t(_rotl(x, n)); }
template<> BL_INLINE uint32_t blBitRorImpl(const uint32_t& x, unsigned n) noexcept { return uint32_t(_rotr(x, n)); }
template<> BL_INLINE uint64_t blBitRolImpl(const uint64_t& x, unsigned n) noexcept { return uint64_t(_rotl64(x, n)); }
template<> BL_INLINE uint64_t blBitRorImpl(const uint64_t& x, unsigned n) noexcept { return uint64_t(_rotr64(x, n)); }
#endif

template<typename X, typename Y>
BL_INLINE X blBitRol(const X& x, const Y& n) noexcept { return X(blBitRolImpl(blAsUIntLeast32(x), unsigned(n))); }

template<typename X, typename Y>
BL_INLINE X blBitRor(const X& x, const Y& n) noexcept { return X(blBitRorImpl(blAsUIntLeast32(x), unsigned(n))); }

//! Returns `x | (x >> y)` - helper used by some bit manipulation helpers.
template<typename X, typename Y>
constexpr X blBitShrOr(const X& x, const Y& y) noexcept { return X(x | blBitShr(x, y)); }

template<typename X, typename Y, typename... Args>
constexpr X blBitShrOr(const X& x, const Y& y, Args... args) noexcept { return blBitShrOr(blBitShrOr(x, y), args...); }

//! Fill all trailing bits right from the first most significant bit set.
template<typename T>
constexpr T blFillTrailingBits(const T& x) noexcept {
  typedef typename BLInternal::StdInt<sizeof(T), 1>::Type U;
  return T(blFillTrailingBits(U(x)));
}

template<> constexpr uint8_t  blFillTrailingBits(const uint8_t& x) noexcept { return blBitShrOr(x, 1, 2, 4); }
template<> constexpr uint16_t blFillTrailingBits(const uint16_t& x) noexcept { return blBitShrOr(x, 1, 2, 4, 8); }
template<> constexpr uint32_t blFillTrailingBits(const uint32_t& x) noexcept { return blBitShrOr(x, 1, 2, 4, 8, 16); }
template<> constexpr uint64_t blFillTrailingBits(const uint64_t& x) noexcept { return blBitShrOr(x, 1, 2, 4, 8, 16, 32); }

//! Return a bit-mask that has `x` bit set.
template<typename T, typename Arg>
constexpr T blBitMask(Arg x) noexcept { return T(1u) << x; }

//! Return a bit-mask that has `x` bit set (multiple arguments).
template<typename T, typename Arg, typename... Args>
constexpr T blBitMask(Arg x, Args... args) noexcept { return T(blBitMask<T>(x) | blBitMask<T>(args...)); }

//! Returns a bit-mask where all bits are set if the given value `x` is 1, or
//! zero otherwise. Please note that `x` must be either 0 or 1, all other
//! values will produce invalid output.
template<typename T, typename B>
constexpr T blBitMaskFromBool(const B& x) noexcept { return blNegate(T(x)); }

//! Get whether `x` has `n`th bit set.
template<typename T, typename I>
constexpr bool blBitTest(const T& x, const I& i) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return (U(x) & (U(1) << i)) != 0;
}

//! Get whether bits specified by `y` are all set in `x`.
template<typename X, typename Y>
constexpr bool blBitMatch(const X& x, const Y& y) noexcept { return (x & y) == y; }

constexpr uint32_t blBitCtzFallback(uint32_t xAndNegX) noexcept {
  return 31 - ((xAndNegX & 0x0000FFFFu) ? 16 : 0)
            - ((xAndNegX & 0x00FF00FFu) ?  8 : 0)
            - ((xAndNegX & 0x0F0F0F0Fu) ?  4 : 0)
            - ((xAndNegX & 0x33333333u) ?  2 : 0)
            - ((xAndNegX & 0x55555555u) ?  1 : 0);
}

constexpr uint32_t blBitCtzFallback(uint64_t xAndNegX) noexcept {
  return 63 - ((xAndNegX & 0x00000000FFFFFFFFu) ? 32 : 0)
            - ((xAndNegX & 0x0000FFFF0000FFFFu) ? 16 : 0)
            - ((xAndNegX & 0x00FF00FF00FF00FFu) ?  8 : 0)
            - ((xAndNegX & 0x0F0F0F0F0F0F0F0Fu) ?  4 : 0)
            - ((xAndNegX & 0x3333333333333333u) ?  2 : 0)
            - ((xAndNegX & 0x5555555555555555u) ?  1 : 0);
}

template<typename T>
constexpr uint32_t blBitCtzStatic(const T& x) noexcept { return blBitCtzFallback(blAsUIntLeast32(x) & blNegate(blAsUIntLeast32(x))); }

template<typename T>
BL_INLINE uint32_t blBitCtzImpl(const T& x) noexcept { return blBitCtzStatic(x); }

#if !defined(BL_BUILD_NO_INTRINSICS)
# if defined(__GNUC__)
template<> BL_INLINE uint32_t blBitCtzImpl(const uint32_t& x) noexcept { return uint32_t(__builtin_ctz(x)); }
template<> BL_INLINE uint32_t blBitCtzImpl(const uint64_t& x) noexcept { return uint32_t(__builtin_ctzll(x)); }
# elif defined(_MSC_VER)
template<> BL_INLINE uint32_t blBitCtzImpl(const uint32_t& x) noexcept { unsigned long i; _BitScanForward(&i, x); return uint32_t(i); }
#  if BL_TARGET_ARCH_X86 == 64 || BL_TARGET_ARCH_ARM == 64
template<> BL_INLINE uint32_t blBitCtzImpl(const uint64_t& x) noexcept { unsigned long i; _BitScanForward64(&i, x); return uint32_t(i); }
#  endif
# endif
#endif

//! Count trailing zeros in `x` (returns a position of a first bit set in `x`).
//!
//! NOTE: The input MUST NOT be zero, otherwise the result is undefined.
template<typename T>
static BL_INLINE uint32_t blBitCtz(T x) noexcept { return blBitCtzImpl(blAsUIntLeast32(x)); }

//! Generate a trailing bit-mask that has `n` least significant (trailing) bits set.
template<typename T, typename N>
constexpr T blTrailingBitMask(const N& n) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return (sizeof(U) < sizeof(uintptr_t))
    ? T(U((uintptr_t(1) << n) - uintptr_t(1)))
    // Shifting more bits than the type provides is UNDEFINED BEHAVIOR.
    // In such case we trash the result by ORing it with a mask that has
    // all bits set and discards the UNDEFINED RESULT of the shift.
    : T(((U(1) << n) - U(1u)) | blNegate(U(n >= N(blBitSizeOf<T>()))));
}

template<typename T>
constexpr bool blIsBitMaskConsecutive(const T& x) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return x != 0 && (U(x) ^ (U(x) + (U(x) & (~U(x) + 1u)))) >= U(x);
}

template<typename T>
constexpr uint32_t blBitShiftOf(const T& x) noexcept { return blBitCtzStatic(x); }

} // {anonymous}

// ============================================================================
// [ByteSwap]
// ============================================================================

namespace {

template<typename T>
static BL_INLINE T blByteSwap32(const T& x) noexcept {
#if defined(__GNUC__) && !defined(BL_BUILD_NO_INTRINSICS)
  return T(uint32_t(__builtin_bswap32(uint32_t(x))));
#elif defined(_MSC_VER) && !defined(BL_BUILD_NO_INTRINSICS)
  return T(uint32_t(_byteswap_ulong(uint32_t(x))));
#else
  return T((uint32_t(x) << 24) | (uint32_t(x) >> 24) | ((uint32_t(x) << 8) & 0x00FF0000u) | ((uint32_t(x) >> 8) & 0x0000FF00));
#endif
}

template<typename T>
static BL_INLINE T blByteSwap24(const T& x) noexcept {
  // This produces always much better code than trying to do a real 24-bit byteswap.
  return T(blByteSwap32(uint32_t(x)) >> 8);
}

template<typename T>
static BL_INLINE T blByteSwap16(const T& x) noexcept {
#if defined(_MSC_VER) && !defined(BL_BUILD_NO_INTRINSICS)
  return T(uint16_t(_byteswap_ushort(uint16_t(x))));
#else
  return T((uint16_t(x) << 8) | (uint16_t(x) >> 8));
#endif
}

template<typename T>
static BL_INLINE T blByteSwap64(const T& x) noexcept {
#if defined(__GNUC__) && !defined(BL_BUILD_NO_INTRINSICS)
  return T(uint64_t(__builtin_bswap64(uint64_t(x))));
#elif defined(_MSC_VER) && !defined(BL_BUILD_NO_INTRINSICS)
  return T(uint64_t(_byteswap_uint64(uint64_t(x))));
#else
  return T( (uint64_t(blByteSwap32(uint32_t(uint64_t(x) >> 32        )))      ) |
            (uint64_t(blByteSwap32(uint32_t(uint64_t(x) & 0xFFFFFFFFu))) << 32) );
#endif
}

// TODO: [GLOBAL] REMOVE?
template<typename T> static BL_INLINE T blByteSwap16LE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_LE ? T(x) : T(blByteSwap16(int16_t(x))); }
template<typename T> static BL_INLINE T blByteSwap24LE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_LE ? T(x) : T(blByteSwap24(uint32_t(x))); }
template<typename T> static BL_INLINE T blByteSwap32LE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_LE ? T(x) : T(blByteSwap32(uint32_t(x))); }
template<typename T> static BL_INLINE T blByteSwap64LE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_LE ? T(x) : T(blByteSwap64(uint64_t(x))); }

template<typename T> static BL_INLINE T blByteSwap16BE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_BE ? T(x) : T(blByteSwap16(uint16_t(x))); }
template<typename T> static BL_INLINE T blByteSwap24BE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_BE ? T(x) : T(blByteSwap24(uint32_t(x))); }
template<typename T> static BL_INLINE T blByteSwap32BE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_BE ? T(x) : T(blByteSwap32(uint32_t(x))); }
template<typename T> static BL_INLINE T blByteSwap64BE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_BE ? T(x) : T(blByteSwap64(uint64_t(x))); }

} // {anonymous}

// ============================================================================
// [Alignment]
// ============================================================================

template<typename X, typename Y>
static constexpr bool blIsAligned(const X& base, const Y& alignment) noexcept {
  typedef typename BLInternal::StdInt<sizeof(X), 1>::Type U;
  return ((U)base % (U)alignment) == 0;
}

//! Get whether the `x` is a power of two (only one bit is set).
template<typename T>
static constexpr bool blIsPowerOf2(const T& x) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return x && !(U(x) & (U(x) - U(1)));
}

template<typename X, typename Y>
static constexpr X blAlignUp(const X& x, const Y& alignment) noexcept {
  typedef typename BLInternal::StdInt<sizeof(X), 1>::Type U;
  return (X)( ((U)x + ((U)(alignment) - 1u)) & ~((U)(alignment) - 1u) );
}

template<typename T>
static constexpr T blAlignUpPowerOf2(const T& x) noexcept {
  typedef typename BLInternal::StdInt<sizeof(T), 1>::Type U;
  return (T)(blFillTrailingBits(U(x) - 1u) + 1u);
}

//! Get zero or a positive difference between `base` and `base` aligned to `alignment`.
template<typename X, typename Y>
static constexpr X blAlignUpDiff(const X& base, const Y& alignment) noexcept {
  typedef typename BLInternal::StdInt<sizeof(X), 1>::Type U;
  return blAlignUp(U(base), alignment) - U(base);
}

template<typename X, typename Y>
static constexpr X blAlignDown(const X& x, const Y& alignment) noexcept {
  typedef typename BLInternal::StdInt<sizeof(X), 1>::Type U;
  return (X)( (U)x & ~((U)(alignment) - 1u) );
}

// ============================================================================
// [Pointer Utilities]
// ============================================================================

template<typename T, typename Offset>
static constexpr T* blOffsetPtr(T* ptr, Offset offset) noexcept { return (T*)((uintptr_t)(ptr) + (uintptr_t)(intptr_t)offset); }

template<typename T, typename P, typename Offset>
static constexpr T* blOffsetPtr(P* ptr, Offset offset) noexcept { return (T*)((uintptr_t)(ptr) + (uintptr_t)(intptr_t)offset); }

// ============================================================================
// [ClampTo]
// ============================================================================

template<typename SrcT, typename DstT>
static constexpr DstT blClampToImpl(const SrcT& x, const DstT& y) noexcept {
  typedef typename std::make_unsigned<SrcT>::type U;
  return U(x) <= U(y)         ? DstT(x) :
         blIsUnsigned<SrcT>() ? DstT(y) : DstT(SrcT(y) & SrcT(blBitSar(blNegate(x), sizeof(SrcT) * 8 - 1)));
}

//! Clamp a value `x` to a byte (unsigned 8-bit type).
template<typename T>
static constexpr uint8_t blClampToByte(const T& x) noexcept {
  return blClampToImpl<T, uint8_t>(x, uint8_t(0xFFu));
}

//! Clamp a value `x` to a word (unsigned 16-bit type).
template<typename T>
static constexpr uint16_t blClampToWord(const T& x) noexcept {
  return blClampToImpl<T, uint16_t>(x, uint16_t(0xFFFFu));
}

// ============================================================================
// [Arithmetic]
// ============================================================================

typedef unsigned char BLOverflowFlag;

namespace BLInternal {
  template<typename T>
  BL_INLINE T addOverflowFallback(T x, T y, BLOverflowFlag* of) noexcept {
    typedef typename std::make_unsigned<T>::type U;

    U result = U(x) + U(y);
    *of |= BLOverflowFlag(blIsUnsigned<T>() ? result < U(x) : T((U(x) ^ ~U(y)) & (U(x) ^ result)) < 0);
    return T(result);
  }

  template<typename T>
  BL_INLINE T subOverflowFallback(T x, T y, BLOverflowFlag* of) noexcept {
    typedef typename std::make_unsigned<T>::type U;

    U result = U(x) - U(y);
    *of |= BLOverflowFlag(blIsUnsigned<T>() ? result > U(x) : T((U(x) ^ U(y)) & (U(x) ^ result)) < 0);
    return T(result);
  }

  template<typename T>
  BL_INLINE T mulOverflowFallback(T x, T y, BLOverflowFlag* of) noexcept {
    typedef typename BLInternal::StdInt<sizeof(T) * 2, blIsUnsigned<T>()>::Type I;
    typedef typename std::make_unsigned<I>::type U;

    U mask = U(blMaxValue<typename std::make_unsigned<T>::type>());
    if (std::is_signed<T>::value) {
      U prod = U(I(x)) * U(I(y));
      *of |= BLOverflowFlag(I(prod) < I(blMinValue<T>()) || I(prod) > I(blMaxValue<T>()));
      return T(I(prod & mask));
    }
    else {
      U prod = U(x) * U(y);
      *of |= BLOverflowFlag((prod & ~mask) != 0);
      return T(prod & mask);
    }
  }

  template<>
  BL_INLINE int64_t mulOverflowFallback(int64_t x, int64_t y, BLOverflowFlag* of) noexcept {
    int64_t result = int64_t(uint64_t(x) * uint64_t(y));
    *of |= BLOverflowFlag(x && (result / x != y));
    return result;
  }

  template<>
  BL_INLINE uint64_t mulOverflowFallback(uint64_t x, uint64_t y, BLOverflowFlag* of) noexcept {
    uint64_t result = x * y;
    *of |= BLOverflowFlag(y != 0 && blMaxValue<uint64_t>() / y < x);
    return result;
  }

  // These can be specialized.
  template<typename T> BL_INLINE T addOverflowImpl(const T& x, const T& y, BLOverflowFlag* of) noexcept { return addOverflowFallback(x, y, of); }
  template<typename T> BL_INLINE T subOverflowImpl(const T& x, const T& y, BLOverflowFlag* of) noexcept { return subOverflowFallback(x, y, of); }
  template<typename T> BL_INLINE T mulOverflowImpl(const T& x, const T& y, BLOverflowFlag* of) noexcept { return mulOverflowFallback(x, y, of); }

  #if defined(__GNUC__) && !defined(BL_BUILD_NO_INTRINSICS)
  #if defined(__clang__) || __GNUC__ >= 5
  #define BL_ARITH_OVERFLOW_SPECIALIZE(FUNC, T, RESULT_T, BUILTIN)            \
    template<>                                                                \
    BL_INLINE T FUNC(const T& x, const T& y, BLOverflowFlag* of) noexcept {   \
      RESULT_T result;                                                        \
      *of |= BLOverflowFlag(BUILTIN((RESULT_T)x, (RESULT_T)y, &result));      \
      return T(result);                                                       \
    }
  BL_ARITH_OVERFLOW_SPECIALIZE(addOverflowImpl, int32_t , int               , __builtin_sadd_overflow  )
  BL_ARITH_OVERFLOW_SPECIALIZE(addOverflowImpl, uint32_t, unsigned int      , __builtin_uadd_overflow  )
  BL_ARITH_OVERFLOW_SPECIALIZE(addOverflowImpl, int64_t , long long         , __builtin_saddll_overflow)
  BL_ARITH_OVERFLOW_SPECIALIZE(addOverflowImpl, uint64_t, unsigned long long, __builtin_uaddll_overflow)
  BL_ARITH_OVERFLOW_SPECIALIZE(subOverflowImpl, int32_t , int               , __builtin_ssub_overflow  )
  BL_ARITH_OVERFLOW_SPECIALIZE(subOverflowImpl, uint32_t, unsigned int      , __builtin_usub_overflow  )
  BL_ARITH_OVERFLOW_SPECIALIZE(subOverflowImpl, int64_t , long long         , __builtin_ssubll_overflow)
  BL_ARITH_OVERFLOW_SPECIALIZE(subOverflowImpl, uint64_t, unsigned long long, __builtin_usubll_overflow)
  BL_ARITH_OVERFLOW_SPECIALIZE(mulOverflowImpl, int32_t , int               , __builtin_smul_overflow  )
  BL_ARITH_OVERFLOW_SPECIALIZE(mulOverflowImpl, uint32_t, unsigned int      , __builtin_umul_overflow  )
  BL_ARITH_OVERFLOW_SPECIALIZE(mulOverflowImpl, int64_t , long long         , __builtin_smulll_overflow)
  BL_ARITH_OVERFLOW_SPECIALIZE(mulOverflowImpl, uint64_t, unsigned long long, __builtin_umulll_overflow)
  #undef BL_ARITH_OVERFLOW_SPECIALIZE
  #endif
  #endif

  // There is a bug in MSVC that makes these specializations unusable, maybe in the future...
  #if defined(_MSC_VER) && 0
  #define BL_ARITH_OVERFLOW_SPECIALIZE(FUNC, T, ALT_T, BUILTIN)               \
    template<>                                                                \
    BL_INLINE T FUNC(T x, T y, BLOverflowFlag* of) noexcept {                 \
      ALT_T result;                                                           \
      *of |= BLOverflowFlag(BUILTIN(0, (ALT_T)x, (ALT_T)y, &result));         \
      return T(result);                                                       \
    }
  BL_ARITH_OVERFLOW_SPECIALIZE(addOverflowImpl, uint32_t, unsigned int      , _addcarry_u32 )
  BL_ARITH_OVERFLOW_SPECIALIZE(subOverflowImpl, uint32_t, unsigned int      , _subborrow_u32)
  #if ARCH_BITS >= 64
  BL_ARITH_OVERFLOW_SPECIALIZE(addOverflowImpl, uint64_t, unsigned __int64  , _addcarry_u64 )
  BL_ARITH_OVERFLOW_SPECIALIZE(subOverflowImpl, uint64_t, unsigned __int64  , _subborrow_u64)
  #endif
  #undef BL_ARITH_OVERFLOW_SPECIALIZE
  #endif
} // {BLInternal}

template<typename T>
static BL_INLINE T blAddOverflow(const T& x, const T& y, BLOverflowFlag* of) noexcept { return T(BLInternal::addOverflowImpl(blAsStdInt(x), blAsStdInt(y), of)); }

template<typename T>
static BL_INLINE T blSubOverflow(const T& x, const T& y, BLOverflowFlag* of) noexcept { return T(BLInternal::subOverflowImpl(blAsStdInt(x), blAsStdInt(y), of)); }

template<typename T>
static BL_INLINE T blMulOverflow(const T& x, const T& y, BLOverflowFlag* of) noexcept { return T(BLInternal::mulOverflowImpl(blAsStdInt(x), blAsStdInt(y), of)); }

template<typename T>
static BL_INLINE T blUAddSaturate(const T& x, const T& y) noexcept {
  BLOverflowFlag of = 0;
  T result = blAddOverflow(x, y, &of);
  return T(result | blBitMaskFromBool<T>(of));
}

template<typename T>
static BL_INLINE T blUSubSaturate(const T& x, const T& y) noexcept {
  BLOverflowFlag of = 0;
  T result = blSubOverflow(x, y, &of);
  return T(result & blBitMaskFromBool<T>(!of));
}

template<typename T>
static BL_INLINE T blUMulSaturate(const T& x, const T& y) noexcept {
  BLOverflowFlag of = 0;
  T result = blMulOverflow(x, y, &of);
  return T(result | blBitMaskFromBool<T>(of));
}

// ============================================================================
// [Udiv255]
// ============================================================================

//! Integer division by 255, compatible with `(x + (x >> 8)) >> 8` used by SIMD.
static constexpr uint32_t blUdiv255(uint32_t x) noexcept { return ((x + 128) * 257) >> 16; }

// ============================================================================
// [blMemRead]
// ============================================================================

namespace {

BL_INLINE uint32_t blMemReadU8(const void* p) noexcept { return uint32_t(static_cast<const uint8_t*>(p)[0]); }
BL_INLINE int32_t blMemReadI8(const void* p) noexcept { return int32_t(static_cast<const int8_t*>(p)[0]); }

template<uint32_t ByteOrder, size_t Alignment>
BL_INLINE uint32_t blMemReadU16(const void* p) noexcept {
  if (ByteOrder == BL_BYTE_ORDER_NATIVE && (BL_UNALIGNED_IO_16 || Alignment >= 2)) {
    typedef typename BLMisalignedUInt<uint16_t, Alignment>::T U16AlignedToN;
    return uint32_t(static_cast<const U16AlignedToN*>(p)[0]);
  }
  else {
    uint32_t hi = blMemReadU8(static_cast<const uint8_t*>(p) + (ByteOrder == BL_BYTE_ORDER_LE ? 1 : 0));
    uint32_t lo = blMemReadU8(static_cast<const uint8_t*>(p) + (ByteOrder == BL_BYTE_ORDER_LE ? 0 : 1));
    return blBitShl(hi, 8) | lo;
  }
}

template<uint32_t ByteOrder, size_t Alignment>
BL_INLINE int32_t blMemReadI16(const void* p) noexcept {
  if (ByteOrder == BL_BYTE_ORDER_NATIVE && (BL_UNALIGNED_IO_16 || Alignment >= 2)) {
    typedef typename BLMisalignedUInt<uint16_t, Alignment>::T U16AlignedToN;
    return int32_t(int16_t(static_cast<const U16AlignedToN*>(p)[0]));
  }
  else {
    int32_t hi = blMemReadI8(static_cast<const uint8_t*>(p) + (ByteOrder == BL_BYTE_ORDER_LE ? 1 : 0));
    int32_t lo = blMemReadU8(static_cast<const uint8_t*>(p) + (ByteOrder == BL_BYTE_ORDER_LE ? 0 : 1));
    return blBitShl(hi, 8) | lo;
  }
}

template<uint32_t ByteOrder = BL_BYTE_ORDER_NATIVE>
BL_INLINE uint32_t blMemReadU24u(const void* p) noexcept {
  uint32_t b0 = blMemReadU8(static_cast<const uint8_t*>(p) + (ByteOrder == BL_BYTE_ORDER_LE ? 2 : 0));
  uint32_t b1 = blMemReadU8(static_cast<const uint8_t*>(p) + (ByteOrder == BL_BYTE_ORDER_LE ? 1 : 1));
  uint32_t b2 = blMemReadU8(static_cast<const uint8_t*>(p) + (ByteOrder == BL_BYTE_ORDER_LE ? 0 : 2));
  return blBitShl(b0, 16) | blBitShl(b1, 8) | b2;
}

template<uint32_t ByteOrder, size_t Alignment>
BL_INLINE uint32_t blMemReadU32(const void* p) noexcept {
  if (BL_UNALIGNED_IO_32 || Alignment >= 4) {
    typedef typename BLMisalignedUInt<uint32_t, Alignment>::T U32AlignedToN;
    uint32_t x = static_cast<const U32AlignedToN*>(p)[0];
    return ByteOrder == BL_BYTE_ORDER_NATIVE ? x : blByteSwap32(x);
  }
  else {
    uint32_t hi = blMemReadU16<ByteOrder, Alignment >= 2 ? size_t(2) : Alignment>(static_cast<const uint8_t*>(p) + (ByteOrder == BL_BYTE_ORDER_LE ? 2 : 0));
    uint32_t lo = blMemReadU16<ByteOrder, Alignment >= 2 ? size_t(2) : Alignment>(static_cast<const uint8_t*>(p) + (ByteOrder == BL_BYTE_ORDER_LE ? 0 : 2));
    return blBitShl(hi, 16) | lo;
  }
}

template<uint32_t ByteOrder, size_t Alignment>
BL_INLINE uint64_t blMemReadU64(const void* p) noexcept {
  if (ByteOrder == BL_BYTE_ORDER_NATIVE && (BL_UNALIGNED_IO_64 || Alignment >= 8)) {
    typedef typename BLMisalignedUInt<uint64_t, Alignment>::T U64AlignedToN;
    return static_cast<const U64AlignedToN*>(p)[0];
  }
  else {
    uint32_t hi = blMemReadU32<ByteOrder, Alignment >= 4 ? size_t(4) : Alignment>(static_cast<const uint8_t*>(p) + (ByteOrder == BL_BYTE_ORDER_LE ? 4 : 0));
    uint32_t lo = blMemReadU32<ByteOrder, Alignment >= 4 ? size_t(4) : Alignment>(static_cast<const uint8_t*>(p) + (ByteOrder == BL_BYTE_ORDER_LE ? 0 : 4));
    return blBitShl(uint64_t(hi), 32) | lo;
  }
}

template<uint32_t ByteOrder, size_t Alignment>
BL_INLINE int32_t blMemReadI32(const void* p) noexcept { return int32_t(blMemReadU32<ByteOrder, Alignment>(p)); }

template<uint32_t ByteOrder, size_t Alignment>
BL_INLINE int64_t blMemReadI64(const void* p) noexcept { return int64_t(blMemReadU64<ByteOrder, Alignment>(p)); }

BL_INLINE int32_t blMemReadI16a(const void* p) noexcept { return blMemReadI16<BL_BYTE_ORDER_NATIVE, 2>(p); }
BL_INLINE int32_t blMemReadI16u(const void* p) noexcept { return blMemReadI16<BL_BYTE_ORDER_NATIVE, 1>(p); }
BL_INLINE uint32_t blMemReadU16a(const void* p) noexcept { return blMemReadU16<BL_BYTE_ORDER_NATIVE, 2>(p); }
BL_INLINE uint32_t blMemReadU16u(const void* p) noexcept { return blMemReadU16<BL_BYTE_ORDER_NATIVE, 1>(p); }

BL_INLINE int32_t blMemReadI16aLE(const void* p) noexcept { return blMemReadI16<BL_BYTE_ORDER_LE, 2>(p); }
BL_INLINE int32_t blMemReadI16uLE(const void* p) noexcept { return blMemReadI16<BL_BYTE_ORDER_LE, 1>(p); }
BL_INLINE uint32_t blMemReadU16aLE(const void* p) noexcept { return blMemReadU16<BL_BYTE_ORDER_LE, 2>(p); }
BL_INLINE uint32_t blMemReadU16uLE(const void* p) noexcept { return blMemReadU16<BL_BYTE_ORDER_LE, 1>(p); }

BL_INLINE int32_t blMemReadI16aBE(const void* p) noexcept { return blMemReadI16<BL_BYTE_ORDER_BE, 2>(p); }
BL_INLINE int32_t blMemReadI16uBE(const void* p) noexcept { return blMemReadI16<BL_BYTE_ORDER_BE, 1>(p); }
BL_INLINE uint32_t blMemReadU16aBE(const void* p) noexcept { return blMemReadU16<BL_BYTE_ORDER_BE, 2>(p); }
BL_INLINE uint32_t blMemReadU16uBE(const void* p) noexcept { return blMemReadU16<BL_BYTE_ORDER_BE, 1>(p); }

BL_INLINE uint32_t blMemReadU24uLE(const void* p) noexcept { return blMemReadU24u<BL_BYTE_ORDER_LE>(p); }
BL_INLINE uint32_t blMemReadU24uBE(const void* p) noexcept { return blMemReadU24u<BL_BYTE_ORDER_BE>(p); }

BL_INLINE int32_t blMemReadI32a(const void* p) noexcept { return blMemReadI32<BL_BYTE_ORDER_NATIVE, 4>(p); }
BL_INLINE int32_t blMemReadI32u(const void* p) noexcept { return blMemReadI32<BL_BYTE_ORDER_NATIVE, 1>(p); }
BL_INLINE uint32_t blMemReadU32a(const void* p) noexcept { return blMemReadU32<BL_BYTE_ORDER_NATIVE, 4>(p); }
BL_INLINE uint32_t blMemReadU32u(const void* p) noexcept { return blMemReadU32<BL_BYTE_ORDER_NATIVE, 1>(p); }

BL_INLINE int32_t blMemReadI32aLE(const void* p) noexcept { return blMemReadI32<BL_BYTE_ORDER_LE, 4>(p); }
BL_INLINE int32_t blMemReadI32uLE(const void* p) noexcept { return blMemReadI32<BL_BYTE_ORDER_LE, 1>(p); }
BL_INLINE uint32_t blMemReadU32aLE(const void* p) noexcept { return blMemReadU32<BL_BYTE_ORDER_LE, 4>(p); }
BL_INLINE uint32_t blMemReadU32uLE(const void* p) noexcept { return blMemReadU32<BL_BYTE_ORDER_LE, 1>(p); }

BL_INLINE int32_t blMemReadI32aBE(const void* p) noexcept { return blMemReadI32<BL_BYTE_ORDER_BE, 4>(p); }
BL_INLINE int32_t blMemReadI32uBE(const void* p) noexcept { return blMemReadI32<BL_BYTE_ORDER_BE, 1>(p); }
BL_INLINE uint32_t blMemReadU32aBE(const void* p) noexcept { return blMemReadU32<BL_BYTE_ORDER_BE, 4>(p); }
BL_INLINE uint32_t blMemReadU32uBE(const void* p) noexcept { return blMemReadU32<BL_BYTE_ORDER_BE, 1>(p); }

BL_INLINE int64_t blMemReadI64a(const void* p) noexcept { return blMemReadI64<BL_BYTE_ORDER_NATIVE, 8>(p); }
BL_INLINE int64_t blMemReadI64u(const void* p) noexcept { return blMemReadI64<BL_BYTE_ORDER_NATIVE, 1>(p); }
BL_INLINE uint64_t blMemReadU64a(const void* p) noexcept { return blMemReadU64<BL_BYTE_ORDER_NATIVE, 8>(p); }
BL_INLINE uint64_t blMemReadU64u(const void* p) noexcept { return blMemReadU64<BL_BYTE_ORDER_NATIVE, 1>(p); }

BL_INLINE int64_t blMemReadI64aLE(const void* p) noexcept { return blMemReadI64<BL_BYTE_ORDER_LE, 8>(p); }
BL_INLINE int64_t blMemReadI64uLE(const void* p) noexcept { return blMemReadI64<BL_BYTE_ORDER_LE, 1>(p); }
BL_INLINE uint64_t blMemReadU64aLE(const void* p) noexcept { return blMemReadU64<BL_BYTE_ORDER_LE, 8>(p); }
BL_INLINE uint64_t blMemReadU64uLE(const void* p) noexcept { return blMemReadU64<BL_BYTE_ORDER_LE, 1>(p); }

BL_INLINE int64_t blMemReadI64aBE(const void* p) noexcept { return blMemReadI64<BL_BYTE_ORDER_BE, 8>(p); }
BL_INLINE int64_t blMemReadI64uBE(const void* p) noexcept { return blMemReadI64<BL_BYTE_ORDER_BE, 1>(p); }
BL_INLINE uint64_t blMemReadU64aBE(const void* p) noexcept { return blMemReadU64<BL_BYTE_ORDER_BE, 8>(p); }
BL_INLINE uint64_t blMemReadU64uBE(const void* p) noexcept { return blMemReadU64<BL_BYTE_ORDER_BE, 1>(p); }

} // {anonymous}

// ============================================================================
// [blMemWrite]
// ============================================================================

namespace {

BL_INLINE void blMemWriteU8(void* p, uint32_t x) noexcept { static_cast<uint8_t*>(p)[0] = uint8_t(x & 0xFFu); }
BL_INLINE void blMemWriteI8(void* p, int32_t x) noexcept { static_cast<uint8_t*>(p)[0] = uint8_t(x & 0xFF); }

template<uint32_t ByteOrder, size_t Alignment>
BL_INLINE void blMemWriteU16(void* p, uint32_t x) noexcept {
  if (ByteOrder == BL_BYTE_ORDER_NATIVE && (BL_UNALIGNED_IO_16 || Alignment >= 2)) {
    typedef typename BLMisalignedUInt<uint16_t, Alignment>::T U16AlignedToN;
    static_cast<U16AlignedToN*>(p)[0] = uint16_t(x & 0xFFFFu);
  }
  else {
    static_cast<uint8_t*>(p)[0] = uint8_t((x >> (ByteOrder == BL_BYTE_ORDER_LE ? 0 : 8)) & 0xFFu);
    static_cast<uint8_t*>(p)[1] = uint8_t((x >> (ByteOrder == BL_BYTE_ORDER_LE ? 8 : 0)) & 0xFFu);
  }
}

template<uint32_t ByteOrder = BL_BYTE_ORDER_NATIVE>
BL_INLINE void blMemWriteU24u(void* p, uint32_t v) noexcept {
  static_cast<uint8_t*>(p)[0] = uint8_t((v >> (ByteOrder == BL_BYTE_ORDER_LE ?  0 : 16)) & 0xFFu);
  static_cast<uint8_t*>(p)[1] = uint8_t((v >> (ByteOrder == BL_BYTE_ORDER_LE ?  8 :  8)) & 0xFFu);
  static_cast<uint8_t*>(p)[2] = uint8_t((v >> (ByteOrder == BL_BYTE_ORDER_LE ? 16 :  0)) & 0xFFu);
}

template<uint32_t ByteOrder, size_t Alignment>
BL_INLINE void blMemWriteU32(void* p, uint32_t x) noexcept {
  if (BL_UNALIGNED_IO_32 || Alignment >= 4) {
    typedef typename BLMisalignedUInt<uint32_t, Alignment>::T U32AlignedToN;
    static_cast<U32AlignedToN*>(p)[0] = (ByteOrder == BL_BYTE_ORDER_NATIVE) ? x : blByteSwap32(x);
  }
  else {
    blMemWriteU16<ByteOrder, Alignment >= 2 ? size_t(2) : Alignment>(static_cast<uint8_t*>(p) + 0, x >> (ByteOrder == BL_BYTE_ORDER_LE ?  0 : 16));
    blMemWriteU16<ByteOrder, Alignment >= 2 ? size_t(2) : Alignment>(static_cast<uint8_t*>(p) + 2, x >> (ByteOrder == BL_BYTE_ORDER_LE ? 16 :  0));
  }
}

template<uint32_t ByteOrder, size_t Alignment>
BL_INLINE void blMemWriteU64(void* p, uint64_t x) noexcept {
  if (ByteOrder == BL_BYTE_ORDER_NATIVE && (BL_UNALIGNED_IO_64 || Alignment >= 8)) {
    typedef typename BLMisalignedUInt<uint64_t, Alignment>::T U64AlignedToN;
    static_cast<U64AlignedToN*>(p)[0] = x;
  }
  else {
    blMemWriteU32<ByteOrder, Alignment >= 4 ? size_t(4) : Alignment>(static_cast<uint8_t*>(p) + 0, uint32_t((x >> (ByteOrder == BL_BYTE_ORDER_LE ?  0 : 32)) & 0xFFFFFFFFu));
    blMemWriteU32<ByteOrder, Alignment >= 4 ? size_t(4) : Alignment>(static_cast<uint8_t*>(p) + 4, uint32_t((x >> (ByteOrder == BL_BYTE_ORDER_LE ? 32 :  0)) & 0xFFFFFFFFu));
  }
}

template<uint32_t ByteOrder, size_t Alignment> BL_INLINE void blMemWriteI16(void* p, int32_t x) noexcept { blMemWriteU16<ByteOrder, Alignment>(p, uint32_t(x)); }
template<uint32_t ByteOrder, size_t Alignment> BL_INLINE void blMemWriteI32(void* p, int32_t x) noexcept { blMemWriteU32<ByteOrder, Alignment>(p, uint32_t(x)); }
template<uint32_t ByteOrder, size_t Alignment> BL_INLINE void blMemWriteI64(void* p, int64_t x) noexcept { blMemWriteU64<ByteOrder, Alignment>(p, uint64_t(x)); }

BL_INLINE void blMemWriteI16a(void* p, int32_t x) noexcept { blMemWriteI16<BL_BYTE_ORDER_NATIVE, 2>(p, x); }
BL_INLINE void blMemWriteI16u(void* p, int32_t x) noexcept { blMemWriteI16<BL_BYTE_ORDER_NATIVE, 1>(p, x); }
BL_INLINE void blMemWriteU16a(void* p, uint32_t x) noexcept { blMemWriteU16<BL_BYTE_ORDER_NATIVE, 2>(p, x); }
BL_INLINE void blMemWriteU16u(void* p, uint32_t x) noexcept { blMemWriteU16<BL_BYTE_ORDER_NATIVE, 1>(p, x); }

BL_INLINE void blMemWriteI16aLE(void* p, int32_t x) noexcept { blMemWriteI16<BL_BYTE_ORDER_LE, 2>(p, x); }
BL_INLINE void blMemWriteI16uLE(void* p, int32_t x) noexcept { blMemWriteI16<BL_BYTE_ORDER_LE, 1>(p, x); }
BL_INLINE void blMemWriteU16aLE(void* p, uint32_t x) noexcept { blMemWriteU16<BL_BYTE_ORDER_LE, 2>(p, x); }
BL_INLINE void blMemWriteU16uLE(void* p, uint32_t x) noexcept { blMemWriteU16<BL_BYTE_ORDER_LE, 1>(p, x); }

BL_INLINE void blMemWriteI16aBE(void* p, int32_t x) noexcept { blMemWriteI16<BL_BYTE_ORDER_BE, 2>(p, x); }
BL_INLINE void blMemWriteI16uBE(void* p, int32_t x) noexcept { blMemWriteI16<BL_BYTE_ORDER_BE, 1>(p, x); }
BL_INLINE void blMemWriteU16aBE(void* p, uint32_t x) noexcept { blMemWriteU16<BL_BYTE_ORDER_BE, 2>(p, x); }
BL_INLINE void blMemWriteU16uBE(void* p, uint32_t x) noexcept { blMemWriteU16<BL_BYTE_ORDER_BE, 1>(p, x); }

BL_INLINE void blMemWriteU24uLE(void* p, uint32_t v) noexcept { blMemWriteU24u<BL_BYTE_ORDER_LE>(p, v); }
BL_INLINE void blMemWriteU24uBE(void* p, uint32_t v) noexcept { blMemWriteU24u<BL_BYTE_ORDER_BE>(p, v); }

BL_INLINE void blMemWriteI32a(void* p, int32_t x) noexcept { blMemWriteI32<BL_BYTE_ORDER_NATIVE, 4>(p, x); }
BL_INLINE void blMemWriteI32u(void* p, int32_t x) noexcept { blMemWriteI32<BL_BYTE_ORDER_NATIVE, 1>(p, x); }
BL_INLINE void blMemWriteU32a(void* p, uint32_t x) noexcept { blMemWriteU32<BL_BYTE_ORDER_NATIVE, 4>(p, x); }
BL_INLINE void blMemWriteU32u(void* p, uint32_t x) noexcept { blMemWriteU32<BL_BYTE_ORDER_NATIVE, 1>(p, x); }

BL_INLINE void blMemWriteI32aLE(void* p, int32_t x) noexcept { blMemWriteI32<BL_BYTE_ORDER_LE, 4>(p, x); }
BL_INLINE void blMemWriteI32uLE(void* p, int32_t x) noexcept { blMemWriteI32<BL_BYTE_ORDER_LE, 1>(p, x); }
BL_INLINE void blMemWriteU32aLE(void* p, uint32_t x) noexcept { blMemWriteU32<BL_BYTE_ORDER_LE, 4>(p, x); }
BL_INLINE void blMemWriteU32uLE(void* p, uint32_t x) noexcept { blMemWriteU32<BL_BYTE_ORDER_LE, 1>(p, x); }

BL_INLINE void blMemWriteI32aBE(void* p, int32_t x) noexcept { blMemWriteI32<BL_BYTE_ORDER_BE, 4>(p, x); }
BL_INLINE void blMemWriteI32uBE(void* p, int32_t x) noexcept { blMemWriteI32<BL_BYTE_ORDER_BE, 1>(p, x); }
BL_INLINE void blMemWriteU32aBE(void* p, uint32_t x) noexcept { blMemWriteU32<BL_BYTE_ORDER_BE, 4>(p, x); }
BL_INLINE void blMemWriteU32uBE(void* p, uint32_t x) noexcept { blMemWriteU32<BL_BYTE_ORDER_BE, 1>(p, x); }

BL_INLINE void blMemWriteI64a(void* p, int64_t x) noexcept { blMemWriteI64<BL_BYTE_ORDER_NATIVE, 8>(p, x); }
BL_INLINE void blMemWriteI64u(void* p, int64_t x) noexcept { blMemWriteI64<BL_BYTE_ORDER_NATIVE, 1>(p, x); }
BL_INLINE void blMemWriteU64a(void* p, uint64_t x) noexcept { blMemWriteU64<BL_BYTE_ORDER_NATIVE, 8>(p, x); }
BL_INLINE void blMemWriteU64u(void* p, uint64_t x) noexcept { blMemWriteU64<BL_BYTE_ORDER_NATIVE, 1>(p, x); }

BL_INLINE void blMemWriteI64aLE(void* p, int64_t x) noexcept { blMemWriteI64<BL_BYTE_ORDER_LE, 8>(p, x); }
BL_INLINE void blMemWriteI64uLE(void* p, int64_t x) noexcept { blMemWriteI64<BL_BYTE_ORDER_LE, 1>(p, x); }
BL_INLINE void blMemWriteU64aLE(void* p, uint64_t x) noexcept { blMemWriteU64<BL_BYTE_ORDER_LE, 8>(p, x); }
BL_INLINE void blMemWriteU64uLE(void* p, uint64_t x) noexcept { blMemWriteU64<BL_BYTE_ORDER_LE, 1>(p, x); }

BL_INLINE void blMemWriteI64aBE(void* p, int64_t x) noexcept { blMemWriteI64<BL_BYTE_ORDER_BE, 8>(p, x); }
BL_INLINE void blMemWriteI64uBE(void* p, int64_t x) noexcept { blMemWriteI64<BL_BYTE_ORDER_BE, 1>(p, x); }
BL_INLINE void blMemWriteU64aBE(void* p, uint64_t x) noexcept { blMemWriteU64<BL_BYTE_ORDER_BE, 8>(p, x); }
BL_INLINE void blMemWriteU64uBE(void* p, uint64_t x) noexcept { blMemWriteU64<BL_BYTE_ORDER_BE, 1>(p, x); }

} // {anonymous}

// ============================================================================
// [blMemCopySmall]
// ============================================================================

namespace {

template<typename T>
static BL_INLINE void blMemCopyInlineT(T* dst, const T* src, size_t count) noexcept {
  for (size_t i = 0; i < count; i++)
    dst[i] = src[i];
}

//! Copies `n` bytes from `src` to `dst` optimized for small buffers.
static BL_INLINE void blMemCopyInline(void* dst, const void* src, size_t n) noexcept {
#if defined(__GNUC__) && BL_TARGET_ARCH_X86
  size_t unused;
  __asm__ __volatile__(
    "rep movsb" : "=&D"(dst), "=&S"(src), "=&c"(unused)
                : "0"(dst), "1"(src), "2"(n)
                : "memory"
  );
#elif defined(_MSC_VER) && BL_TARGET_ARCH_X86
  __movsb(static_cast<unsigned char *>(dst), static_cast<const unsigned char *>(src), n);
#else
  blMemCopyInlineT<uint8_t>(static_cast<uint8_t*>(dst), static_cast<const uint8_t*>(src), n);
#endif
}

} // {anonymous}

// ============================================================================
// [BLWrap<T>]
// ============================================================================

//! Wrapper to control construction & destruction of `T`.
template<typename T>
struct alignas(alignof(T)) BLWrap {
  // --------------------------------------------------------------------------
  // [Init / Destroy]
  // --------------------------------------------------------------------------

  //! Placement new constructor.
  BL_INLINE T* init() noexcept {
    #ifdef _MSC_VER
    BL_ASSUME(_data != nullptr);
    #endif
    return new(static_cast<void*>(_data)) T;
  }

  //! Placement new constructor with arguments.
  template<typename... Args>
  BL_INLINE T* init(Args&&... args) noexcept {
    #ifdef _MSC_VER
    BL_ASSUME(_data != nullptr);
    #endif
    return new(static_cast<void*>(_data)) T(std::forward<Args>(args)...);
  }

  //! Placement delete destructor.
  BL_INLINE void destroy() noexcept {
    #ifdef _MSC_VER
    BL_ASSUME(_data != nullptr);
    #endif
    static_cast<T*>(static_cast<void*>(_data))->~T();
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  BL_INLINE T* p() noexcept { return static_cast<T*>(static_cast<void*>(_data)); }
  BL_INLINE const T* p() const noexcept { return static_cast<const T*>(static_cast<const void*>(_data)); }

  BL_INLINE operator T&() noexcept { return *p(); }
  BL_INLINE operator const T&() const noexcept { return *p(); }

  BL_INLINE T& operator()() noexcept { return *p(); }
  BL_INLINE const T& operator()() const noexcept { return *p(); }

  BL_INLINE T* operator&() noexcept { return p(); }
  BL_INLINE const T* operator&() const noexcept { return p(); }

  BL_INLINE T* operator->() noexcept { return p(); }
  BL_INLINE T const* operator->() const noexcept { return p(); }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Storage required to instantiate `T`.
  char _data[sizeof(T)];
};

// ============================================================================
// [BLScopedAllocator]
// ============================================================================

//! A simple allocator that can be used to remember allocated memory so it can
//! be then freed in one go. Typically used in areas where some heap allocation
//! is required and at the end of the work it will all be freed.
class BLScopedAllocator {
public:
  BL_NONCOPYABLE(BLScopedAllocator)
  struct Link { Link* next; };

  Link* links;
  uint8_t* poolPtr;
  uint8_t* poolMem;
  uint8_t* poolEnd;

  BL_INLINE BLScopedAllocator() noexcept
    : links(nullptr),
      poolPtr(nullptr),
      poolMem(nullptr),
      poolEnd(nullptr) {}

  BL_INLINE BLScopedAllocator(void* poolMem, size_t poolSize) noexcept
    : links(nullptr),
      poolPtr(static_cast<uint8_t*>(poolMem)),
      poolMem(static_cast<uint8_t*>(poolMem)),
      poolEnd(static_cast<uint8_t*>(poolMem) + poolSize) {}

  BL_INLINE ~BLScopedAllocator() noexcept { reset(); }

  void* alloc(size_t size, size_t alignment = 1) noexcept;
  void reset() noexcept;
};

// ============================================================================
// [BLMemBuffer]
// ============================================================================

//! Memory buffer.
//!
//! Memory buffer is a helper class which holds pointer to an allocated memory
//! block, which will be released automatically by `BLMemBuffer` destructor or by
//! `reset()` call.
class BLMemBuffer {
public:
  BL_NONCOPYABLE(BLMemBuffer)

  void* _mem;
  void* _buf;
  size_t _capacity;

  BL_INLINE BLMemBuffer() noexcept
    : _mem(nullptr),
      _buf(nullptr),
      _capacity(0) {}

  BL_INLINE ~BLMemBuffer() noexcept {
    _reset();
  }

protected:
  BL_INLINE BLMemBuffer(void* mem, void* buf, size_t capacity) noexcept
    : _mem(mem),
      _buf(buf),
      _capacity(capacity) {}

public:
  BL_INLINE void* get() const noexcept { return _mem; }
  BL_INLINE size_t capacity() const noexcept { return _capacity; }

  BL_INLINE void* alloc(size_t size) noexcept {
    if (size <= _capacity)
      return _mem;

    if (_mem != _buf)
      free(_mem);

    _mem = malloc(size);
    _capacity = size;

    return _mem;
  }

  BL_INLINE void _reset() noexcept {
    if (_mem != _buf)
      free(_mem);
  }

  BL_INLINE void reset() noexcept {
    _reset();

    _mem = nullptr;
    _capacity = 0;
  }
};

// ============================================================================
// [BLMemBufferTmp<>]
// ============================================================================

//! Memory buffer (temporary).
//!
//! This template is for fast routines that need to use memory  allocated on
//! the stack, but the memory requirement is not known at compile time. The
//! number of bytes allocated on the stack is described by `N` parameter.
template<size_t N>
class BLMemBufferTmp : public BLMemBuffer {
public:
  BL_NONCOPYABLE(BLMemBufferTmp<N>)

  uint8_t _storage[N];

  BL_INLINE BLMemBufferTmp() noexcept
    : BLMemBuffer(_storage, _storage, N) {}

  BL_INLINE ~BLMemBufferTmp() noexcept {}

  using BLMemBuffer::alloc;

  BL_INLINE void reset() noexcept {
    _reset();
    _mem = _buf;
    _capacity = N;
  }
};

//! \}
//! \endcond

#endif // BLEND2D_BLSUPPORT_P_H
