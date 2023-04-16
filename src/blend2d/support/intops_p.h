// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SUPPORT_INTOPS_P_H_INCLUDED
#define BLEND2D_SUPPORT_INTOPS_P_H_INCLUDED

#include "../support/traits_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name Types
//! \{

typedef unsigned char BLOverflowFlag;

//! \}

//! Utility functions and classes simplifying integer operations.
namespace BLIntOps {

using BLInternal::StdInt;

namespace {

//! \name Integer Type Traits
//! \{

template<typename T>
BL_NODISCARD
static BL_INLINE constexpr bool isUnsigned() noexcept { return std::is_unsigned<T>::value; }

//! \}

//! \name Integer Type Conversion
//! \{

//! Cast an integer `x` to a fixed-width type as defined by <stdint.h>
//!
//! This can help when specializing some functions for a particular type. Since
//! some C/C++ types may overlap (like `long` vs `long long`) it's easier to
//! just cast to a type as defined by <stdint.h> and specialize for it.
template<typename T>
BL_NODISCARD
static BL_INLINE constexpr typename StdInt<sizeof(T), isUnsigned<T>()>::Type asStdInt(T x) noexcept {
  return (typename StdInt<sizeof(T), isUnsigned<T>()>::Type)x;
}

//! Cast an integer `x` to a fixed-width unsigned type as defined by <stdint.h>
template<typename T>
BL_NODISCARD
static BL_INLINE constexpr typename StdInt<sizeof(T), 1>::Type asStdUInt(T x) noexcept {
  return (typename StdInt<sizeof(T), 1>::Type)x;
}

//! Cast an integer `x` to either `int32_t`, uint32_t`, `int64_t`, or `uint64_t`.
//!
//! Used to keep a signedness of `T`, but to promote it to at least 32-bit type.
template<typename T>
BL_NODISCARD
static BL_INLINE constexpr typename StdInt<blMax<size_t>(sizeof(T), 4), isUnsigned<T>()>::Type asInt32AtLeast(T x) noexcept {
  typedef typename StdInt<blMax<size_t>(sizeof(T), 4), isUnsigned<T>()>::Type Result;
  return Result(x);
}

//! Cast an integer `x` to either `uint32_t` or `uint64_t`.
template<typename T>
BL_NODISCARD
static BL_INLINE constexpr typename StdInt<blMax<size_t>(sizeof(T), 4), 1>::Type asUInt32AtLeast(T x) noexcept {
  typedef typename StdInt<blMax<size_t>(sizeof(T), 4), 1>::Type Result;
  typedef typename std::make_unsigned<T>::type U;
  return Result(U(x));
}

//! \}

//! \name Byte Swap Operations
//! \{

template<typename T>
BL_NODISCARD
static BL_INLINE T byteSwap32(const T& x) noexcept {
#if defined(__GNUC__) && !defined(BL_BUILD_NO_INTRINSICS)
  return T(uint32_t(__builtin_bswap32(uint32_t(x))));
#elif defined(_MSC_VER) && !defined(BL_BUILD_NO_INTRINSICS)
  return T(uint32_t(_byteswap_ulong(uint32_t(x))));
#else
  return T((uint32_t(x) << 24) | (uint32_t(x) >> 24) | ((uint32_t(x) << 8) & 0x00FF0000u) | ((uint32_t(x) >> 8) & 0x0000FF00));
#endif
}

template<typename T>
BL_NODISCARD
static BL_INLINE T byteSwap24(const T& x) noexcept {
  // This produces always much better code than trying to do a real 24-bit byteswap.
  return T(byteSwap32(uint32_t(x)) >> 8);
}

template<typename T>
BL_NODISCARD
static BL_INLINE T byteSwap16(const T& x) noexcept {
#if defined(_MSC_VER) && !defined(BL_BUILD_NO_INTRINSICS)
  return T(uint16_t(_byteswap_ushort(uint16_t(x))));
#else
  return T((uint16_t(x) << 8) | (uint16_t(x) >> 8));
#endif
}

template<typename T>
BL_NODISCARD
static BL_INLINE T byteSwap64(const T& x) noexcept {
#if defined(__GNUC__) && !defined(BL_BUILD_NO_INTRINSICS)
  return T(uint64_t(__builtin_bswap64(uint64_t(x))));
#elif defined(_MSC_VER) && !defined(BL_BUILD_NO_INTRINSICS)
  return T(uint64_t(_byteswap_uint64(uint64_t(x))));
#else
  return T( (uint64_t(byteSwap32(uint32_t(uint64_t(x) >> 32        )))      ) |
            (uint64_t(byteSwap32(uint32_t(uint64_t(x) & 0xFFFFFFFFu))) << 32) );
#endif
}

template<typename T>
BL_NODISCARD
static BL_INLINE T byteSwap(const T& x) noexcept {
  BL_STATIC_ASSERT(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);

  if (sizeof(T) == 1)
    return x;
  else if (sizeof(T) == 2)
    return byteSwap16(x);
  else if (sizeof(T) == 4)
    return byteSwap32(x);
  else
    return byteSwap64(x);
}

// TODO: [GLOBAL] REMOVE?
template<typename T> BL_NODISCARD static BL_INLINE T byteSwap16LE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_LE ? T(x) : T(byteSwap16(uint16_t(x))); }
template<typename T> BL_NODISCARD static BL_INLINE T byteSwap24LE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_LE ? T(x) : T(byteSwap24(uint32_t(x))); }
template<typename T> BL_NODISCARD static BL_INLINE T byteSwap32LE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_LE ? T(x) : T(byteSwap32(uint32_t(x))); }
template<typename T> BL_NODISCARD static BL_INLINE T byteSwap64LE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_LE ? T(x) : T(byteSwap64(uint64_t(x))); }

template<typename T> BL_NODISCARD static BL_INLINE T byteSwap16BE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_BE ? T(x) : T(byteSwap16(uint16_t(x))); }
template<typename T> BL_NODISCARD static BL_INLINE T byteSwap24BE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_BE ? T(x) : T(byteSwap24(uint32_t(x))); }
template<typename T> BL_NODISCARD static BL_INLINE T byteSwap32BE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_BE ? T(x) : T(byteSwap32(uint32_t(x))); }
template<typename T> BL_NODISCARD static BL_INLINE T byteSwap64BE(T x) noexcept { return BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_BE ? T(x) : T(byteSwap64(uint64_t(x))); }

//! \}

//! \name Arithmetic Operations
//! \{

//! Returns `0 - x` in a safe way (no undefined behavior), works for both signed and unsigned numbers.
template<typename T>
BL_NODISCARD
static constexpr T negate(const T& x) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return T(U(0) - U(x));
}

// Look for "Carry-save adder" for more details.
template<typename T>
static BL_INLINE void csa(T& hi, T& lo, T a, T b, T c) noexcept {
  T u = a ^ b;
  hi = (a & b) | (u & c);
  lo = u ^ c;
}

//! \}

//! \name Bit Manipulation
//! \{

template<typename T>
BL_NODISCARD
BL_INLINE constexpr uint32_t bitSizeOf() noexcept { return uint32_t(sizeof(T) * 8u); }

template<typename T>
BL_NODISCARD
BL_INLINE constexpr T allOnes() noexcept { return T(~T(0)); }

template<typename T>
BL_NODISCARD
BL_INLINE constexpr size_t wordCountFromBitCount(size_t nBits) noexcept {
  return (nBits + bitSizeOf<T>() - 1) / bitSizeOf<T>();
}

//! Returns `x << y` (shift left logical) by explicitly casting `x` to an unsigned type and back.
template<typename X, typename Y>
BL_NODISCARD
BL_INLINE constexpr X shl(const X& x, const Y& y) noexcept {
  typedef typename std::make_unsigned<X>::type U;
  return X(U(x) << y);
}

//! Returns `x >> y` (shift right logical) by explicitly casting `x` to an unsigned type and back.
template<typename X, typename Y>
BL_NODISCARD
BL_INLINE constexpr X shr(const X& x, const Y& y) noexcept {
  typedef typename std::make_unsigned<X>::type U;
  return X(U(x) >> y);
}

//! Returns `x >> y` (shift right arithmetic) by explicitly casting `x` to a signed type and back.
template<typename X, typename Y>
BL_NODISCARD
BL_INLINE constexpr X sar(const X& x, const Y& y) noexcept {
  typedef typename std::make_signed<X>::type S;
  return X(S(x) >> y);
}

template<typename T>
BL_NODISCARD
BL_INLINE T rolImpl(const T& x, unsigned n) noexcept {
  return shl(x, n % unsigned(sizeof(T) * 8u)) | shr(x, (0u - n) % unsigned(sizeof(T) * 8u)) ;
}

template<typename T>
BL_NODISCARD
BL_INLINE T rorImpl(const T& x, unsigned n) noexcept {
  return shr(x, n % unsigned(sizeof(T) * 8u)) | shl(x, (0u - n) % unsigned(sizeof(T) * 8u)) ;
}

// MSVC is unable to emit `rol|ror` instruction when `n` is not a constant so we have to help it a bit.
// This  prevents us from using `constexpr`.
#if defined(_MSC_VER)
template<> BL_NODISCARD BL_INLINE uint8_t rolImpl(const uint8_t& x, unsigned n) noexcept { return uint8_t(_rotl8(x, uint8_t(n))); }
template<> BL_NODISCARD BL_INLINE uint8_t rorImpl(const uint8_t& x, unsigned n) noexcept { return uint8_t(_rotr8(x, uint8_t(n))); }
template<> BL_NODISCARD BL_INLINE uint16_t rolImpl(const uint16_t& x, unsigned n) noexcept { return uint16_t(_rotl16(x, uint8_t(n))); }
template<> BL_NODISCARD BL_INLINE uint16_t rorImpl(const uint16_t& x, unsigned n) noexcept { return uint16_t(_rotr16(x, uint8_t(n))); }
template<> BL_NODISCARD BL_INLINE uint32_t rolImpl(const uint32_t& x, unsigned n) noexcept { return uint32_t(_rotl(x, int(n))); }
template<> BL_NODISCARD BL_INLINE uint32_t rorImpl(const uint32_t& x, unsigned n) noexcept { return uint32_t(_rotr(x, int(n))); }
template<> BL_NODISCARD BL_INLINE uint64_t rolImpl(const uint64_t& x, unsigned n) noexcept { return uint64_t(_rotl64(x, int(n))); }
template<> BL_NODISCARD BL_INLINE uint64_t rorImpl(const uint64_t& x, unsigned n) noexcept { return uint64_t(_rotr64(x, int(n))); }
#endif

template<typename X, typename Y>
BL_NODISCARD
BL_INLINE X rol(const X& x, const Y& n) noexcept { return X(rolImpl(asUInt32AtLeast(x), unsigned(n))); }

template<typename X, typename Y>
BL_NODISCARD
BL_INLINE X ror(const X& x, const Y& n) noexcept { return X(rorImpl(asUInt32AtLeast(x), unsigned(n))); }

//! Returns `x | (x >> y)` - helper used by some bit manipulation helpers.
template<typename X, typename Y>
BL_NODISCARD
BL_INLINE constexpr X shrOr(const X& x, const Y& y) noexcept { return X(x | shr(x, y)); }

template<typename X, typename Y, typename... Args>
BL_NODISCARD
BL_INLINE constexpr X shrOr(const X& x, const Y& y, Args... args) noexcept { return shrOr(shrOr(x, y), args...); }

//! Fills all trailing bits right from the first most significant bit set.
template<typename T>
BL_NODISCARD
BL_INLINE constexpr T fillTrailingBits(const T& x) noexcept {
  typedef typename StdInt<sizeof(T), 1>::Type U;
  return T(fillTrailingBits(U(x)));
}

template<> BL_NODISCARD BL_INLINE constexpr uint8_t  fillTrailingBits(const uint8_t& x) noexcept { return shrOr(x, 1, 2, 4); }
template<> BL_NODISCARD BL_INLINE constexpr uint16_t fillTrailingBits(const uint16_t& x) noexcept { return shrOr(x, 1, 2, 4, 8); }
template<> BL_NODISCARD BL_INLINE constexpr uint32_t fillTrailingBits(const uint32_t& x) noexcept { return shrOr(x, 1, 2, 4, 8, 16); }
template<> BL_NODISCARD BL_INLINE constexpr uint64_t fillTrailingBits(const uint64_t& x) noexcept { return shrOr(x, 1, 2, 4, 8, 16, 32); }

template<typename T, typename N = uint32_t>
BL_NODISCARD
static BL_INLINE constexpr T nonZeroLsbMask(const N& n = 1) noexcept {
  return shr(allOnes<T>(), N(bitSizeOf<T>()) - n);
}

template<typename T, typename N = uint32_t>
BL_NODISCARD
static BL_INLINE constexpr T nonZeroMsbMask(const N& n = 1) noexcept {
  return sar(shl(T(1), bitSizeOf<T>() - 1u), n - 1u);
}

//! Returns a bit-mask that has `x` bit set.
template<typename T, typename Arg>
BL_NODISCARD
BL_INLINE constexpr T lsbBitAt(Arg x) noexcept { return T(T(1u) << x); }

//! Returns a bit-mask that has `x` bit set (multiple bits version).
template<typename T, typename Arg>
BL_NODISCARD
BL_INLINE constexpr T lsbBitsAt(Arg x) noexcept { return T(T(1u) << x); }

template<typename T, typename Arg, typename... Args>
BL_NODISCARD
BL_INLINE constexpr T lsbBitsAt(Arg x, Args... args) noexcept { return T(lsbBitsAt<T>(x) | lsbBitsAt<T>(args...)); }

//! Returns a bit-mask where all bits are set if the given value `x` is 1, or
//! zero otherwise. Please note that `x` must be either 0 or 1, all other
//! values will produce invalid output.
template<typename T, typename B>
BL_NODISCARD
BL_INLINE constexpr T bitMaskFromBool(const B& x) noexcept { return negate(T(x)); }

//! Tests whether `x` has `n`th bit set.
template<typename T, typename I>
BL_NODISCARD
BL_INLINE constexpr bool bitTest(const T& x, const I& i) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return (U(x) & (U(1) << i)) != 0;
}

//! Tests whether bits specified by `y` are all set in `x`.
template<typename X, typename Y>
BL_NODISCARD
BL_INLINE constexpr bool bitMatch(const X& x, const Y& y) noexcept { return (x & y) == y; }

template<typename T>
BL_NODISCARD
BL_INLINE constexpr bool isBitMaskConsecutive(const T& x) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return x != 0 && (U(x) ^ (U(x) + (U(x) & (~U(x) + 1u)))) >= U(x);
}

template<typename T>
BL_NODISCARD
static BL_INLINE T bitSwap(const T& x) noexcept {
  auto v = asUInt32AtLeast(x);

  auto m1 = asUInt32AtLeast(T(0x5555555555555555u & allOnes<T>()));
  auto m2 = asUInt32AtLeast(T(0x3333333333333333u & allOnes<T>()));
  auto m4 = asUInt32AtLeast(T(0x0F0F0F0F0F0F0F0Fu & allOnes<T>()));

  v = ((v >> 1) & m1) | ((v & m1) << 1);
  v = ((v >> 2) & m2) | ((v & m2) << 2);
  v = ((v >> 4) & m4) | ((v & m4) << 4);

  return byteSwap(T(v));
}

//! \}

//! \name Bit Scanning
//! \{

template<typename T>
struct BitScanResult { T x; uint32_t n; };

template<typename T, uint32_t N>
struct BitScanImpl {
  BL_NODISCARD
  static BL_INLINE constexpr BitScanResult<T> advanceLeft(const BitScanResult<T>& data, uint32_t n) noexcept {
    return BitScanResult<T> { data.x << n, data.n + n };
  }

  BL_NODISCARD
  static BL_INLINE constexpr BitScanResult<T> advanceRight(const BitScanResult<T>& data, uint32_t n) noexcept {
    return BitScanResult<T> { data.x >> n, data.n + n };
  }

  BL_NODISCARD
  static BL_INLINE constexpr BitScanResult<T> clz(const BitScanResult<T>& data) noexcept {
    return BitScanImpl<T, N / 2>::clz(advanceLeft(data, data.x & (allOnes<T>() << (bitSizeOf<T>() - N)) ? uint32_t(0) : N));
  }

  BL_NODISCARD
  static BL_INLINE constexpr BitScanResult<T> ctz(const BitScanResult<T>& data) noexcept {
    return BitScanImpl<T, N / 2>::ctz(advanceRight(data, data.x & (allOnes<T>() >> (bitSizeOf<T>() - N)) ? uint32_t(0) : N));
  }
};

template<typename T>
struct BitScanImpl<T, 0> {
  BL_NODISCARD
  static BL_INLINE constexpr BitScanResult<T> clz(const BitScanResult<T>& ctx) noexcept {
    return BitScanResult<T> { 0, ctx.n - uint32_t(ctx.x >> (bitSizeOf<T>() - 1)) };
  }

  BL_NODISCARD
  static BL_INLINE constexpr BitScanResult<T> ctz(const BitScanResult<T>& ctx) noexcept {
    return BitScanResult<T> { 0, ctx.n - uint32_t(ctx.x & 0x1) };
  }
};

template<typename T>
BL_NODISCARD
static BL_INLINE constexpr uint32_t clzFallback(const T& x) noexcept {
  return BitScanImpl<T, bitSizeOf<T>() / 2u>::clz(BitScanResult<T>{x, 1}).n;
}

template<typename T>
BL_NODISCARD
static BL_INLINE constexpr uint32_t ctzFallback(const T& x) noexcept {
  return BitScanImpl<T, bitSizeOf<T>() / 2u>::ctz(BitScanResult<T>{x, 1}).n;
}

template<typename T> BL_NODISCARD BL_INLINE constexpr uint32_t clzStatic(const T& x) noexcept { return clzFallback(asUInt32AtLeast(x)); }
template<typename T> BL_NODISCARD BL_INLINE constexpr uint32_t ctzStatic(const T& x) noexcept { return ctzFallback(asUInt32AtLeast(x)); }

template<typename T> BL_NODISCARD BL_INLINE uint32_t clzImpl(const T& x) noexcept { return clzStatic(x); }
template<typename T> BL_NODISCARD BL_INLINE uint32_t ctzImpl(const T& x) noexcept { return ctzStatic(x); }

#if !defined(BL_BUILD_NO_INTRINSICS)
# if defined(__GNUC__)
template<> BL_NODISCARD BL_INLINE uint32_t clzImpl(const uint32_t& x) noexcept { return uint32_t(__builtin_clz(x)); }
template<> BL_NODISCARD BL_INLINE uint32_t clzImpl(const uint64_t& x) noexcept { return uint32_t(__builtin_clzll(x)); }
template<> BL_NODISCARD BL_INLINE uint32_t ctzImpl(const uint32_t& x) noexcept { return uint32_t(__builtin_ctz(x)); }
template<> BL_NODISCARD BL_INLINE uint32_t ctzImpl(const uint64_t& x) noexcept { return uint32_t(__builtin_ctzll(x)); }
# elif defined(_MSC_VER)
template<> BL_NODISCARD BL_INLINE uint32_t clzImpl(const uint32_t& x) noexcept { unsigned long i; _BitScanReverse(&i, x); return uint32_t(i ^ 31); }
template<> BL_NODISCARD BL_INLINE uint32_t ctzImpl(const uint32_t& x) noexcept { unsigned long i; _BitScanForward(&i, x); return uint32_t(i); }
#  if BL_TARGET_ARCH_X86 == 64 || BL_TARGET_ARCH_ARM == 64
template<> BL_NODISCARD BL_INLINE uint32_t clzImpl(const uint64_t& x) noexcept { unsigned long i; _BitScanReverse64(&i, x); return uint32_t(i ^ 63); }
template<> BL_NODISCARD BL_INLINE uint32_t ctzImpl(const uint64_t& x) noexcept { unsigned long i; _BitScanForward64(&i, x); return uint32_t(i); }
#  endif
# endif
#endif

//! Counts leading zeros in `x`.
//!
//! \note If the input is zero the result is undefined.
template<typename T>
BL_NODISCARD
static BL_INLINE uint32_t clz(T x) noexcept { return clzImpl(asUInt32AtLeast(x)); }

//! Counts trailing zeros in `x`.
//!
//! \note If the input is zero the result is undefined.
template<typename T>
BL_NODISCARD
static BL_INLINE uint32_t ctz(T x) noexcept { return ctzImpl(asUInt32AtLeast(x)); }

template<typename T>
BL_NODISCARD
static BL_INLINE constexpr uint32_t bitShiftOf(const T& x) noexcept { return ctzStatic(x); }

//! \}

//! \name Bit Counting
//! \{

// Based on the following resource:
//   http://graphics.stanford.edu/~seander/bithacks.html
//
// Alternatively, for a very small number of bits in `x`:
//   uint32_t n = 0;
//   while (x) {
//     x &= x - 1;
//     n++;
//   }
//   return n;
template<typename T>
BL_NODISCARD
static BL_INLINE uint32_t popCountStatic(const T& x) noexcept {
  typedef typename std::make_unsigned<T>::type U;

  const U m1 = U(0x5555555555555555u & allOnes<U>());
  const U m2 = U(0x3333333333333333u & allOnes<U>());
  const U m4 = U(0x0F0F0F0F0F0F0F0Fu & allOnes<U>());
  const U mX = U(0x0101010101010101u & allOnes<U>());

  U u = U(x);
  u -= ((u >> 1) & m1);
  u  = ((u >> 2) & m2) + (u & m2);
  u  = ((u >> 4) + u) & m4;

  if (sizeof(T) > 1)
    return uint32_t((u * mX) >> (bitSizeOf<T>() - 8));
  else
    return uint32_t(u & 0xFFu);
}

BL_NODISCARD
static BL_INLINE uint32_t popCountImpl(uint32_t x) noexcept {
#if defined(__GNUC__)
  return uint32_t(__builtin_popcount(x));
#elif defined(_MSC_VER) && defined(BL_TARGET_OPT_POPCNT)
  return __popcnt(x);
#else
  return popCountStatic(x);
#endif
}

BL_NODISCARD
static BL_INLINE uint32_t popCountImpl(uint64_t x) noexcept {
#if defined(__GNUC__)
  return uint32_t(__builtin_popcountll(x));
#elif defined(_MSC_VER) && defined(BL_TARGET_OPT_POPCNT) && BL_TARGET_ARCH_BITS >= 64
  return uint32_t(__popcnt64(x));
#elif BL_TARGET_ARCH_BITS >= 64
  return popCountImpl(uint32_t(x >> 32)) + popCountImpl(uint32_t(x & 0xFFFFFFFFu));
#else
  return popCountStatic(x);
#endif
}

//! Calculates count of bits in `x`.
template<typename T>
BL_NODISCARD
static BL_INLINE uint32_t popCount(T x) noexcept { return popCountImpl(asUInt32AtLeast(x)); }

// Simple PopCount context designed to take advantage of HW PopCount support.
template<typename T>
class PopCounterSimple {
public:
  uint32_t _counter = 0;

  BL_INLINE void reset() noexcept {
    _counter = 0;
  }

  BL_INLINE uint32_t get() const noexcept {
    return _counter;
  }

  BL_INLINE void addPopulation(uint32_t v) noexcept {
    _counter += v;
  }

  BL_INLINE void addItem(const T& x) noexcept {
    _counter += popCount(x);
  }

  BL_INLINE void addArray(const T* data, size_t n) noexcept {
    while (n) {
      _counter += popCount(data[0]);
      data++;
      n--;
    }
  }
};

// Harley-Seal PopCount from Hacker's Delight, Second Edition.
//
// This is one of the best implementation if the hardware doesn't provide POPCNT instruction.
template<typename T>
class PopCounterHarleySeal {
public:
  uint32_t _counter = 0;
  T _ones = 0;
  T _twos = 0;
  T _fours = 0;

  BL_INLINE void reset() noexcept {
    _counter = 0;
    _ones = 0;
    _twos = 0;
    _fours = 0;
  }

  BL_INLINE uint32_t get() const noexcept {
    return _counter + 4 * popCount(_fours) + 2 * popCount(_twos) + popCount(_ones);
  }

  BL_INLINE void addPopulation(uint32_t v) noexcept {
    _counter += v;
  }

  BL_INLINE void addItem(const T& x) noexcept {
    _counter += popCount(x);
  }

  BL_INLINE void addArray(const T* data, size_t n) noexcept {
    uint32_t eightsCount = 0;
    while (n >= 8) {
      T twosA, twosB;
      T foursA, foursB;
      T eights;

      csa(twosA, _ones, _ones, data[0], data[1]);
      csa(twosB, _ones, _ones, data[2], data[3]);
      csa(foursA, _twos, _twos, twosA, twosB);
      csa(twosA, _ones, _ones, data[4], data[5]);
      csa(twosB, _ones, _ones, data[6], data[7]);
      csa(foursB, _twos, _twos, twosA, twosB);
      csa(eights, _fours, _fours, foursA, foursB);

      eightsCount += popCount(eights);
      data += 8;
      n -= 8;
    }

    _counter += 8 * eightsCount;
    while (n) {
      _counter += popCount(data[0]);
      data++;
      n--;
    }
  }
};

#if defined(BL_TARGET_OPT_POPCNT)
template<typename T>
using PopCounter = PopCounterSimple<T>;
#else
template<typename T>
using PopCounter = PopCounterHarleySeal<T>;
#endif

//! \}

//! \name Alignment Operations
//! \{

template<typename X, typename Y>
BL_NODISCARD
static BL_INLINE constexpr bool isAligned(const X& x, const Y& alignment) noexcept {
  typedef typename StdInt<sizeof(X), 1>::Type U;
  return ((U)x % (U)alignment) == 0;
}

//! Tests whether the `x` is a power of two (only one bit is set).
template<typename T>
BL_NODISCARD
static BL_INLINE constexpr bool isPowerOf2(const T& x) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return x && !(U(x) & (U(x) - U(1)));
}

template<typename X, typename Y>
BL_NODISCARD
static BL_INLINE constexpr X alignUp(const X& x, const Y& alignment) noexcept {
  typedef typename StdInt<sizeof(X), 1>::Type U;
  return (X)( ((U)x + ((U)(alignment) - 1u)) & ~((U)(alignment) - 1u) );
}

//! Returns zero or a positive difference between `x` and `x` aligned to `alignment`.
template<typename X, typename Y>
BL_NODISCARD
static BL_INLINE constexpr X alignUpDiff(const X& x, const Y& alignment) noexcept {
  typedef typename StdInt<sizeof(X), 1>::Type U;
  return (X)((U(0) - U(x)) & (alignment - 1));
}

template<typename T>
BL_NODISCARD
static BL_INLINE constexpr T alignUpPowerOf2(const T& x) noexcept {
  typedef typename StdInt<sizeof(T), 1>::Type U;
  return (T)(fillTrailingBits(U(x) - 1u) + 1u);
}

template<typename X, typename Y>
BL_NODISCARD
static BL_INLINE constexpr X alignDown(const X& x, const Y& alignment) noexcept {
  typedef typename StdInt<sizeof(X), 1>::Type U;
  return (X)( (U)x & ~((U)(alignment) - 1u) );
}

//! \}

//! \name Arithmetic Operations
//! \{

template<typename T>
BL_INLINE T addOverflowFallback(T x, T y, BLOverflowFlag* of) noexcept {
  typedef typename std::make_unsigned<T>::type U;

  U result = U(x) + U(y);
  *of = BLOverflowFlag(*of | BLOverflowFlag(isUnsigned<T>() ? result < U(x) : T((U(x) ^ ~U(y)) & (U(x) ^ result)) < 0));
  return T(result);
}

template<typename T>
BL_INLINE T subOverflowFallback(T x, T y, BLOverflowFlag* of) noexcept {
  typedef typename std::make_unsigned<T>::type U;

  U result = U(x) - U(y);
  *of = BLOverflowFlag(*of | BLOverflowFlag(isUnsigned<T>() ? result > U(x) : T((U(x) ^ U(y)) & (U(x) ^ result)) < 0));
  return T(result);
}

template<typename T>
BL_INLINE T mulOverflowFallback(T x, T y, BLOverflowFlag* of) noexcept {
  typedef typename StdInt<sizeof(T) * 2, isUnsigned<T>()>::Type I;
  typedef typename std::make_unsigned<I>::type U;

  U mask = U(BLTraits::maxValue<typename std::make_unsigned<T>::type>());
  if (std::is_signed<T>::value) {
    U prod = U(I(x)) * U(I(y));
    *of = BLOverflowFlag(*of | BLOverflowFlag(I(prod) < I(BLTraits::minValue<T>()) || I(prod) > BLTraits::maxValue<T>()));
    return T(I(prod & mask));
  }
  else {
    U prod = U(x) * U(y);
    *of = BLOverflowFlag(*of | BLOverflowFlag((prod & ~mask) != 0));
    return T(prod & mask);
  }
}

template<>
BL_INLINE int64_t mulOverflowFallback(int64_t x, int64_t y, BLOverflowFlag* of) noexcept {
  int64_t result = int64_t(uint64_t(x) * uint64_t(y));
  *of = BLOverflowFlag(*of | BLOverflowFlag(x && (result / x != y)));
  return result;
}

template<>
BL_INLINE uint64_t mulOverflowFallback(uint64_t x, uint64_t y, BLOverflowFlag* of) noexcept {
  uint64_t result = x * y;
  *of = BLOverflowFlag(*of | BLOverflowFlag(y != 0 && BLTraits::maxValue<uint64_t>() / y < x));
  return result;
}

// These can be specialized.
template<typename T> BL_INLINE T addOverflowImpl(const T& x, const T& y, BLOverflowFlag* of) noexcept { return addOverflowFallback(x, y, of); }
template<typename T> BL_INLINE T subOverflowImpl(const T& x, const T& y, BLOverflowFlag* of) noexcept { return subOverflowFallback(x, y, of); }
template<typename T> BL_INLINE T mulOverflowImpl(const T& x, const T& y, BLOverflowFlag* of) noexcept { return mulOverflowFallback(x, y, of); }

#if defined(__GNUC__) && !defined(BL_BUILD_NO_INTRINSICS)
#if defined(__clang__) || __GNUC__ >= 5
#define BL_ARITH_OVERFLOW_SPECIALIZE(FUNC, T, RESULT_T, BUILTIN)              \
  template<>                                                                  \
  BL_INLINE T FUNC(const T& x, const T& y, BLOverflowFlag* of) noexcept {     \
    RESULT_T result;                                                          \
    *of = BLOverflowFlag(*of | (BUILTIN((RESULT_T)x, (RESULT_T)y, &result))); \
    return T(result);                                                         \
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

// Do not use these in 32-bit mode as some compilers would instead emit a call
// into a helper function (__mulodi4, for example), which could then fail if
// the resulting binary is not linked to the compiler runtime library.
#if BL_TARGET_ARCH_BITS == 64
BL_ARITH_OVERFLOW_SPECIALIZE(mulOverflowImpl, int64_t , long long         , __builtin_smulll_overflow)
BL_ARITH_OVERFLOW_SPECIALIZE(mulOverflowImpl, uint64_t, unsigned long long, __builtin_umulll_overflow)
#endif

#undef BL_ARITH_OVERFLOW_SPECIALIZE
#endif
#endif

// There is a bug in MSVC that makes these specializations unusable, maybe in the future...
#if defined(_MSC_VER) && 0
#define BL_ARITH_OVERFLOW_SPECIALIZE(FUNC, T, ALT_T, BUILTIN)                 \
  template<>                                                                  \
  BL_INLINE T FUNC(T x, T y, BLOverflowFlag* of) noexcept {                   \
    ALT_T result;                                                             \
    *of = BLOverflowFlag(*of | (BUILTIN(0, (ALT_T)x, (ALT_T)y, &result)));    \
    return T(result);                                                         \
  }
BL_ARITH_OVERFLOW_SPECIALIZE(addOverflowImpl, uint32_t, unsigned int      , _addcarry_u32 )
BL_ARITH_OVERFLOW_SPECIALIZE(subOverflowImpl, uint32_t, unsigned int      , _subborrow_u32)
#if ARCH_BITS >= 64
BL_ARITH_OVERFLOW_SPECIALIZE(addOverflowImpl, uint64_t, unsigned __int64  , _addcarry_u64 )
BL_ARITH_OVERFLOW_SPECIALIZE(subOverflowImpl, uint64_t, unsigned __int64  , _subborrow_u64)
#endif
#undef BL_ARITH_OVERFLOW_SPECIALIZE
#endif

template<typename T>
static BL_INLINE T addOverflow(const T& x, const T& y, BLOverflowFlag* of) noexcept { return T(addOverflowImpl(asStdInt(x), asStdInt(y), of)); }

template<typename T>
static BL_INLINE T subOverflow(const T& x, const T& y, BLOverflowFlag* of) noexcept { return T(subOverflowImpl(asStdInt(x), asStdInt(y), of)); }

template<typename T>
static BL_INLINE T mulOverflow(const T& x, const T& y, BLOverflowFlag* of) noexcept { return T(mulOverflowImpl(asStdInt(x), asStdInt(y), of)); }

template<typename T>
BL_NODISCARD
static BL_INLINE T uaddSaturate(const T& x, const T& y) noexcept {
  BLOverflowFlag of = 0;
  T result = addOverflow(x, y, &of);
  return T(result | bitMaskFromBool<T>(of));
}

template<typename T>
BL_NODISCARD
static BL_INLINE T usubSaturate(const T& x, const T& y) noexcept {
  BLOverflowFlag of = 0;
  T result = subOverflow(x, y, &of);
  return T(result & bitMaskFromBool<T>(!of));
}

template<typename T>
BL_NODISCARD
static BL_INLINE T umulSaturate(const T& x, const T& y) noexcept {
  BLOverflowFlag of = 0;
  T result = mulOverflow(x, y, &of);
  return T(result | bitMaskFromBool<T>(of));
}

//! \}

//! \name Clamp
//! \{

template<typename SrcT, typename DstT>
BL_NODISCARD
static BL_INLINE constexpr DstT clampToImpl(const SrcT& x, const DstT& y) noexcept {
  typedef typename std::make_unsigned<SrcT>::type U;
  return U(x) <= U(y) ? DstT(x)
                      : isUnsigned<SrcT>() ? DstT(y) : DstT(SrcT(y) & SrcT(sar(negate(x), sizeof(SrcT) * 8 - 1)));
}

//! Clamp a value `x` to a byte (unsigned 8-bit type).
template<typename T>
BL_NODISCARD
static BL_INLINE constexpr uint8_t clampToByte(const T& x) noexcept {
  return clampToImpl<T, uint8_t>(x, uint8_t(0xFFu));
}

//! Clamp a value `x` to a word (unsigned 16-bit type).
template<typename T>
BL_NODISCARD
static BL_INLINE constexpr uint16_t clampToWord(const T& x) noexcept {
  return clampToImpl<T, uint16_t>(x, uint16_t(0xFFFFu));
}

//! \}

} // {anonymous}
} // {BLIntOps}

//! \}
//! \endcond

#endif // BLEND2D_SUPPORT_INTOPS_P_H_INCLUDED
