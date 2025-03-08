// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_COMPRESSION_DEFLATEDECODERUTILS_P_H_INCLUDED
#define BLEND2D_COMPRESSION_DEFLATEDECODERUTILS_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../compression/deflatedecoder_p.h"
#include "../support/intops_p.h"
#include "../support/memops_p.h"

#if defined(BL_TARGET_OPT_BMI2)
  #include <x86intrin.h>
#endif

#if defined(BL_TARGET_OPT_SSSE3) || (BL_TARGET_ARCH_ARM == 64)
  #include "../simd/simd_p.h"
  #define BL_DECODER_USE_SWIZZLEV
#endif

//! \cond INTERNAL

namespace bl {
namespace Compression {
namespace Deflate {

namespace DecoderUtils {
namespace {

#if defined(BL_TARGET_OPT_BMI2)
template<typename T>
BL_INLINE_NODEBUG uint32_t mask32(const T& n) noexcept { return uint32_t(_bzhi_u32(uint32_t(0xFFFFFFFFu), uint32_t(n))); }

BL_INLINE_NODEBUG uint32_t extract_n(BLBitWord src, size_t n) noexcept { return uint32_t(_bzhi_u32(uint32_t(src), uint32_t(n))); }

BL_INLINE_NODEBUG uint32_t extract_entry(BLBitWord src, DecodeEntry entry) noexcept { return uint32_t(_bzhi_u32(uint32_t(src), entry.value)); }
#else
template<typename T>
BL_INLINE_NODEBUG uint32_t mask32(const T& n) noexcept { return (1u << n) - 1u; }

BL_INLINE_NODEBUG uint32_t extract_n(BLBitWord src, size_t n) noexcept { return (uint32_t(src) & mask32(uint32_t(n))); }

BL_INLINE_NODEBUG uint32_t extract_entry(BLBitWord src, DecodeEntry entry) noexcept { return uint32_t(src) & ((1u << (entry.value & uint32_t(IntOps::bitSizeOf<BLBitWord>() - 1))) - 1u); }
#endif

BL_INLINE_NODEBUG bool isLiteral(DecodeEntry e) noexcept { return (e.value & DecodeEntry::kLiteralFlag) != 0u; }
BL_INLINE_NODEBUG bool isOffOrLen(DecodeEntry e) noexcept { return (e.value & DecodeEntry::kOffOrLenFlag) != 0u; }
BL_INLINE_NODEBUG bool isOffAndLen(DecodeEntry e) noexcept { return (e.value & DecodeEntry::kOffAndLenFlag) != 0u; }
BL_INLINE_NODEBUG bool isEndOfBlock(DecodeEntry e) noexcept { return (e.value & (DecodeEntry::kEndOfBlockFlag)) != 0u; }
BL_INLINE_NODEBUG bool isEndOfBlockInvalid(DecodeEntry e) noexcept { return (e.value & DecodeEntry::kEndOfBlockInvalidFlag) != 0u; }

// Extracts a field from DecodeEntry.
template<uint32_t kOffset, uint32_t kNBits>
BL_INLINE_NODEBUG uint32_t extractField(DecodeEntry e) noexcept { return (e.value >> kOffset) & mask32(kNBits); }

BL_INLINE_NODEBUG uint32_t baseLength(DecodeEntry e) noexcept { return extractField<DecodeEntry::kBaseLengthOffset, DecodeEntry::kBaseLengthNBits>(e); }
BL_INLINE_NODEBUG uint32_t fullLength(DecodeEntry e) noexcept { return extractField<DecodeEntry::kFullLengthOffset, DecodeEntry::kFullLengthNBits>(e); }

BL_INLINE_NODEBUG uint32_t rawPayload(DecodeEntry e) noexcept { return (e.value >> DecodeEntry::kPayloadOffset) & 0xFFFFu; }
BL_INLINE_NODEBUG uint32_t payloadField(DecodeEntry e) noexcept { return extractField<DecodeEntry::kPayloadOffset, DecodeEntry::kPayloadNBits>(e); }

BL_INLINE_NODEBUG uint32_t precodeValue(DecodeEntry e) noexcept { return extractField<DecodeEntry::kPrecodeValueOffset, DecodeEntry::kPrecodeValueNBits>(e); }
BL_INLINE_NODEBUG uint32_t precodeRepeat(DecodeEntry e) noexcept { return extractField<DecodeEntry::kPrecodeRepeatOffset, DecodeEntry::kPrecodeRepeatNBits>(e); }

BL_INLINE_NODEBUG uint32_t extractExtra(BLBitWord src, DecodeEntry e) noexcept { return extract_entry(src, e) >> baseLength(e); }

} // {anonymous}
} // {DecoderUtils}

// bl::Compression::Deflate - Decoder Bits
// =======================================

namespace {

struct DecoderTableMask {
  uint32_t _mask;

  BL_INLINE_NODEBUG explicit DecoderTableMask(uint32_t bitlen) noexcept
    : _mask(DecoderUtils::mask32(bitlen)) {}

  BL_INLINE_NODEBUG uint32_t extractIndex(BLBitWord bits) const noexcept {
    return uint32_t(bits & _mask);
  }
};

struct DecoderBits {
  BLBitWord bitWord {};
  size_t bitLength {};

  BL_INLINE_NODEBUG void reset() noexcept {
    bitWord = 0;
    bitLength = 0;
  }

  BL_INLINE_NODEBUG void loadState(Decoder* ctx) noexcept {
    bitWord = ctx->_bitWord;
    bitLength = ctx->_bitLength;
  }

  BL_INLINE_NODEBUG void storeState(Decoder* ctx) noexcept {
    ctx->_bitWord = bitWord;
    ctx->_bitLength = bitLength;
  }

  BL_INLINE_NODEBUG BLBitWord all() const noexcept { return bitWord; }
  BL_INLINE_NODEBUG size_t length() const noexcept { return bitLength; }

  BL_INLINE_NODEBUG bool empty() const noexcept { return bitLength == 0; }
  BL_INLINE_NODEBUG bool overflown() const noexcept { return bitLength > IntOps::bitSizeOf<BLBitWord>(); }

  BL_INLINE_NODEBUG bool canRefillByte() const noexcept {
    if BL_CONSTEXPR (sizeof(BLBitWord) >= 8)
      return bitLength < (IntOps::bitSizeOf<BLBitWord>() - 8u);
    else
      return bitLength <= (IntOps::bitSizeOf<BLBitWord>() - 8u);
  }

  BL_INLINE_NODEBUG void refillByte(uint8_t b) noexcept {
    BL_ASSERT(canRefillByte());

    bitWord |= BLBitWord(b) << bitLength;
    bitLength += 8;
  }

  BL_INLINE_NODEBUG size_t calculateBitWordRefillCount() const noexcept {
    constexpr size_t kFullMinusOne = sizeof(BLBitWord) - 1;
    return kFullMinusOne - ((bitLength >> 3) & 0x7u);
  }

  BL_INLINE_NODEBUG size_t refillBitWord(BLBitWord b) noexcept {
    bitWord |= b << (bitLength & (IntOps::bitSizeOf<BLBitWord>() - 1));
    size_t refillSize = (~bitLength >> 3) & (sizeof(BLBitWord) - 1);

    bitLength |= IntOps::bitSizeOf<BLBitWord>() - 8u;
    return refillSize;
  }

  template<size_t Index = 0>
  BL_INLINE_NODEBUG uint32_t extract(size_t n) const noexcept {
    return DecoderUtils::extract_n(bitWord >> Index, n);
  }

  BL_INLINE_NODEBUG uint32_t extract(DecoderTableMask msk) const noexcept {
    return msk.extractIndex(bitWord);
  }

  BL_INLINE_NODEBUG uint32_t extract(DecodeEntry entry) const noexcept {
    return DecoderUtils::extract_n(bitWord, entry.value);
  }

  BL_INLINE_NODEBUG uint32_t extractExtra(DecodeEntry entry) noexcept {
    return DecoderUtils::extractExtra(bitWord, entry);
  }

  BL_INLINE_NODEBUG uint32_t and_(uint32_t mask) const noexcept {
    return uint32_t(bitWord & mask);
  }

  BL_INLINE_NODEBUG uint32_t operator&(uint32_t mask) const noexcept {
    return uint32_t(bitWord & mask);
  }

  BL_INLINE_NODEBUG void consumed(size_t n) noexcept {
    bitWord >>= n;
    bitLength -= n;
  }

  BL_INLINE_NODEBUG void consumedUnsafe(uint32_t n) noexcept {
    bitWord >>= n & (IntOps::bitSizeOf<BLBitWord>() - 1u);
    bitLength -= n;
  }

  BL_INLINE_NODEBUG void consumed(DecodeEntry n) noexcept {
    consumedUnsafe(n.value);
  }

  BL_INLINE_NODEBUG bool isByteAligned() const noexcept { return (bitLength & 0x7u) == 0u; }
  BL_INLINE_NODEBUG void makeByteAligned() noexcept { consumed(bitLength & 0x7u); }

  BL_INLINE_NODEBUG void fixLengthAfterFastLoop() noexcept {
    if BL_CONSTEXPR (sizeof(BLBitWord) >= 8) {
      bitLength &= (IntOps::bitSizeOf<BLBitWord>() - 1u);
    }
  }
};

// Copy at least BLBitWord quantities.
template<size_t N>
BL_INLINE void copy_bitwords(uint8_t* dst, const uint8_t* src) noexcept {
  BL_STATIC_ASSERT((N % sizeof(BLBitWord)) == 0);

  for (size_t i = 0; i < N; i += sizeof(BLBitWord)) {
    BLBitWord v = MemOps::loadu<BLBitWord>(src + i);
    MemOps::storeu(dst + i, v);
  }
}

template<size_t N>
BL_INLINE void fill_bitwords(uint8_t* dst, BLBitWord src) noexcept {
  BL_STATIC_ASSERT((N % sizeof(BLBitWord)) == 0);

  for (size_t i = 0; i < N; i += sizeof(BLBitWord)) {
    MemOps::storeu(dst + i, src);
  }
}

} // {anonymous}

extern const BLBitWord kScalarRepeatMultiply[sizeof(BLBitWord)];
extern const uint8_t kScalarRotatePredicateL[sizeof(BLBitWord)];
extern const uint8_t kScalarRotatePredicateR[sizeof(BLBitWord)];

struct BL_ALIGN_TYPE(SimdRepeatTable16, 16) { uint8_t data[16]; };
extern const SimdRepeatTable16 kSimdRepeatTable16[16];
extern const SimdRepeatTable16 kSimdRotateTable16[16];

namespace {

class ScalarCopyContext {
public:
  using Register = BLBitWord;

  Register repeatMask;
  Register repeatPred;
  Register rotateLeft;
  Register rotateRight;

  BL_INLINE void initRepeat(size_t offset) noexcept {
    BL_ASSERT(offset < sizeof(Register));

    repeatMask = IntOps::allOnes<BLBitWord>() >> (64u - (offset * 8u));
    repeatPred = BLBitWord(kScalarRepeatMultiply[offset]);
  }

  BL_INLINE void initRotate(size_t offset) noexcept {
    BL_ASSERT(offset < sizeof(Register));

    rotateLeft = kScalarRotatePredicateL[offset];
    rotateRight = kScalarRotatePredicateR[offset];
  }

  static BL_INLINE Register load(const uint8_t* src) noexcept { return MemOps::loadu_le<BLBitWord>(src); }
  static BL_INLINE Register load_raw(const uint8_t* src) noexcept { return MemOps::loadu<BLBitWord>(src); }

  static BL_INLINE void store(uint8_t* dst, Register r) noexcept { MemOps::storeu_le(dst, r); }
  static BL_INLINE void store_raw(uint8_t* dst, Register r) noexcept { MemOps::storeu(dst, r); }

  BL_INLINE Register repeat(Register r) const noexcept { return (r & repeatMask) * repeatPred; }
  BL_INLINE Register rotate(Register r) noexcept { return (r >> rotateRight) | (r << rotateLeft); }
};

#if defined(BL_DECODER_USE_SWIZZLEV)
class SimdCopyContext {
public:
  using Register = SIMD::Vec16xU8;

  Register repeatPredicate;
  Register rotatePredicate;

  BL_INLINE void initRepeat(size_t offset) noexcept {
    BL_ASSERT(offset < sizeof(Register));

    repeatPredicate = SIMD::loada_128<SIMD::Vec16xU8>(&kSimdRepeatTable16[offset]);
  }

  BL_INLINE void initRotate(size_t offset) noexcept {
    BL_ASSERT(offset < sizeof(Register));

    rotatePredicate = SIMD::loada_128<SIMD::Vec16xU8>(&kSimdRotateTable16[offset]);
  }

  BL_INLINE Register repeat(Register r) const noexcept { return SIMD::swizzlev_u8(r, repeatPredicate); }
  BL_INLINE Register rotate(Register r) const noexcept { return SIMD::swizzlev_u8(r, rotatePredicate); }

  static BL_INLINE Register load(const uint8_t* src) noexcept { return SIMD::loadu_128<Register>(src); }
  static BL_INLINE Register load_raw(const uint8_t* src) noexcept { return SIMD::loadu_128<Register>(src); }

  static BL_INLINE void store(uint8_t* dst, Register r) noexcept { SIMD::storeu_128(dst, r); }
  static BL_INLINE void store_raw(uint8_t* dst, Register r) noexcept { SIMD::storeu_128(dst, r); }
};

using CopyContext = SimdCopyContext;
#else
using CopyContext = ScalarCopyContext;
#endif

} // {anonymous}

} // {Deflate}
} // {Compression}
} // {bl}

//! \endcond

#endif // BLEND2D_COMPRESSION_DEFLATEDECODERUTILS_P_H_INCLUDED
