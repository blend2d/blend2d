// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CODEC_JPEGHUFFMAN_P_H_INCLUDED
#define BLEND2D_CODEC_JPEGHUFFMAN_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../support/intops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_codec_impl
//! \{

namespace bl {
namespace Jpeg {

// Acceleration constants for huffman decoding. Accel bits should be either 8 or 9. More bits consume more memory,
// but allow easier decoding of a code that fits within the number of bits. Libjpeg uses 8 bits with a comment that
// 8 bits is sufficient for decoding approximately 95% of codes so we use the same.
static constexpr uint32_t kHuffmanAccelBits = 8;
static constexpr uint32_t kHuffmanAccelSize = 1 << kHuffmanAccelBits;
static constexpr uint32_t kHuffmanAccelMask = kHuffmanAccelSize - 1;

//! JPEG Huffman decompression table.
struct DecoderHuffmanTable {
  //! Largest code of length k (-1 if none).
  uint32_t maxCode[18];
  //! Value offsets (deltas) for codes of length k.
  int32_t delta[17];
  uint16_t code[256];
  uint8_t size[257];
  //! Huffman symbols, in order of increasing code length (part of DHT marker).
  uint8_t values[256];
  //! Acceleration table for decoding huffman codes up to `kHuffmanAccelBits`.
  uint8_t accel[kHuffmanAccelSize];
};

struct DecoderHuffmanACTable : public DecoderHuffmanTable {
  //! Additional table that decodes both magnitude and value of small ACs in one go.
  int16_t acAccel[kHuffmanAccelSize];
};

struct DecoderHuffmanDCTable : public DecoderHuffmanTable {
  // No additional fields.
};

//! JPEG decoder's bit-stream.
//!
//! Holds the current decoder position in a bit-stream, but it's not used to fetch bits from it. Use
//! `DecoderBitReader` to actually read from the bit-stream.
struct DecoderBitStream {
  //! Data pointer (points to the byte to be processed).
  const uint8_t* ptr;
  //! End of input (points to the first invalid byte).
  const uint8_t* end;
  //! Machine word that contains available bits.
  BLBitWord bitData;
  //! Number of valid bits in `bitData`.
  size_t bitCount;
  //! TODO: [JPEG] Document EOB run.
  uint32_t eobRun;
  //! Restart counter in the current stream (reset by DRI and RST markers).
  uint32_t restartCounter;

  BL_INLINE void reset(const uint8_t* ptr_, const uint8_t* end_) noexcept {
    ptr = ptr_;
    end = end_;
    bitData = 0;
    bitCount = 0;
    eobRun = 0;
  }

  BL_INLINE void reset() noexcept { reset(nullptr, nullptr); }
};

//! JPEG decoder's bit-reader.
//!
//! Class that is used to read data from `DecoderBitStream`.
struct DecoderBitReader {
  //! Data pointer (points to the byte to be processed).
  const uint8_t* ptr;
  //! End of input (points to the first invalid byte).
  const uint8_t* end;
  //! Machine word that contains available bits.
  BLBitWord bitData;
  //! Number of valid bits in `bitData`.
  size_t bitCount;

  BL_INLINE explicit DecoderBitReader(DecoderBitStream& stream) noexcept
    : ptr(stream.ptr),
      end(stream.end),
      bitData(stream.bitData),
      bitCount(stream.bitCount) {}

  BL_INLINE void done(DecoderBitStream& stream) noexcept {
    stream.bitData = bitData;
    stream.bitCount = bitCount;
    stream.ptr = ptr;
    stream.end = end;
  }

  BL_INLINE bool atEnd() const noexcept { return ptr == end; }
  BL_INLINE bool hasBits(size_t n) const noexcept { return bitCount >= n; }

  BL_INLINE BLResult requireBits(size_t n) const noexcept {
    if (BL_UNLIKELY(!hasBits(n)))
      return blTraceError(BL_ERROR_DECOMPRESSION_FAILED);
    return BL_SUCCESS;
  }

  BL_INLINE void flush() noexcept {
    bitData = 0;
    bitCount = 0;
  }

  BL_INLINE void advance(size_t nBytes) noexcept {
    BL_ASSERT((size_t)(end - ptr) >= nBytes);
    ptr += nBytes;
  }

  BL_INLINE void drop(size_t n) noexcept {
    bitData <<= n;
    bitCount -= n;
  }

  template<typename T = BLBitWord>
  BL_INLINE T peek(size_t n) const noexcept {
    typedef typename std::make_unsigned<T>::type U;
    return T(U(bitData >> (IntOps::bitSizeOf<BLBitWord>() - n)));
  }

  BL_INLINE void refill() noexcept {
    while (bitCount <= IntOps::bitSizeOf<BLBitWord>() - 8 && ptr != end) {
      uint32_t tmpByte = *ptr++;

      // The [0xFF] byte has to be escaped by [0xFF, 0x00], so read two bytes.
      if (tmpByte == 0xFF) {
        if (BL_UNLIKELY(ptr == end))
          break;

        uint32_t tmpMarker = *ptr++;
        if (BL_UNLIKELY(tmpMarker != 0x00)) {
          ptr -= 2;
          end = ptr;
          break;
        }
      }

      bitData += BLBitWord(tmpByte) << ((IntOps::bitSizeOf<BLBitWord>() - 8) - bitCount);
      bitCount += 8;
    }
  }

  BL_INLINE void refillIf32Bit() noexcept {
    if (IntOps::bitSizeOf<BLBitWord>() <= 32)
      return refill();
  }

  // Read a single bit (0 or 1).
  template<typename T = BLBitWord>
  BL_INLINE T readBit() noexcept {
    BL_ASSERT(bitCount >= 1);

    T result = peek<T>(1);
    drop(1);
    return result;
  }

  // Read `n` bits and sign extend.
  BL_INLINE int32_t readSigned(size_t n) noexcept {
    BL_ASSERT(bitCount >= n);

    int32_t tmpMask = IntOps::shl(int32_t(-1), n) + 1;
    int32_t tmpSign = -peek<int32_t>(1);

    int32_t result = peek<int32_t>(n) + (tmpMask & ~tmpSign);
    drop(n);
    return result;
  }

  // Read `n` bits and zero extend.
  BL_INLINE uint32_t readUnsigned(size_t n) noexcept {
    BL_ASSERT(bitCount >= n);

    uint32_t result = peek<uint32_t>(n);
    drop(n);
    return result;
  }

  template<typename T>
  BL_INLINE BLResult readCode(T& dst, const DecoderHuffmanTable* table) noexcept {
    uint32_t code = table->accel[peek(kHuffmanAccelBits)];
    uint32_t codeSize;

    if (code < 255) {
      // FAST: Look at the top bits and determine the symbol id.
      codeSize = table->size[code];
      if (BL_UNLIKELY(codeSize > bitCount))
        return blTraceError(BL_ERROR_DECOMPRESSION_FAILED);
    }
    else {
      // SLOW: Naive test is to shift the `bitData` down so `s` bits are valid, then test against `maxCode`. To speed
      // this up, we've pre-shifted maxCode left so that it has `16-s` 0s at the end; in other words, regardless of the
      // number of bits, it wants to be compared against something shifted to have 16; that way we don't need to shift
      // inside the loop.
      code = peek<uint32_t>(16);
      codeSize = kHuffmanAccelBits + 1;

      while (code >= table->maxCode[codeSize])
        codeSize++;

      // Maximum code size is 16, check for 17/bitCount and fail if reached.
      if (BL_UNLIKELY(codeSize == 17 || codeSize > bitCount))
        return blTraceError(BL_ERROR_DECOMPRESSION_FAILED);

      // Convert the huffman code to the symbol ID.
      code = peek<uint32_t>(codeSize) + uint32_t(table->delta[codeSize]);
    }

    // Convert the symbol ID to the resulting BYTE.
    dst = T(table->values[code]);
    drop(codeSize);
    return BL_SUCCESS;
  }
};

BL_HIDDEN BLResult buildHuffmanAC(DecoderHuffmanACTable* table, const uint8_t* data, size_t dataSize, size_t* bytesConsumed) noexcept;
BL_HIDDEN BLResult buildHuffmanDC(DecoderHuffmanDCTable* table, const uint8_t* data, size_t dataSize, size_t* bytesConsumed) noexcept;

} // {Jpeg}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CODEC_JPEGHUFFMAN_P_H_INCLUDED
