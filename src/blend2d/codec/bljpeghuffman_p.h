// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_CODEC_BLJPEGHUFFMAN_P_H
#define BLEND2D_CODEC_BLJPEGHUFFMAN_P_H

#include "../blapi-internal_p.h"
#include "../blsupport_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_codecs
//! \{

// ============================================================================
// [BLJpegCodec - Constants]
// ============================================================================

//! Acceleration constants for huffman decoding. Accel bits should be either
//! 8 or 9. More bits consume more memory, but allow easier decoding of a code
//! that fits within the number of bits. Libjpeg uses 8 bits with a comment that
//! 8 bits is sufficient for decoding approximately 95% of codes so we use the
//! same.
enum BLJpegHuffmanAccelConstants : uint32_t {
  BL_JPEG_DECODER_HUFFMAN_ACCEL_BITS = 8,
  BL_JPEG_DECODER_HUFFMAN_ACCEL_SIZE = 1 << BL_JPEG_DECODER_HUFFMAN_ACCEL_BITS,
  BL_JPEG_DECODER_HUFFMAN_ACCEL_MASK = BL_JPEG_DECODER_HUFFMAN_ACCEL_SIZE - 1
};

// ============================================================================
// [BLJpegDecoder - HuffmanTable]
// ============================================================================

//! JPEG Huffman decompression table.
struct BLJpegDecoderHuffmanTable {
  //! Largest code of length k (-1 if none).
  uint32_t maxCode[18];
  //! Value offsets (deltas) for codes of length k.
  int32_t delta[17];
  uint16_t code[256];
  uint8_t size[257];
  //! Huffman symbols, in order of increasing code length (part of DHT marker).
  uint8_t values[256];
  //! Acceleration table for decoding huffman codes up to `BL_JPEG_DECODER_HUFFMAN_ACCEL_BITS`.
  uint8_t accel[BL_JPEG_DECODER_HUFFMAN_ACCEL_SIZE];
};

struct BLJpegDecoderHuffmanACTable : public BLJpegDecoderHuffmanTable {
  //! Additional table that decodes both magnitude and value of small ACs in one go.
  int16_t acAccel[BL_JPEG_DECODER_HUFFMAN_ACCEL_SIZE];
};

struct BLJpegDecoderHuffmanDCTable : public BLJpegDecoderHuffmanTable {
  // No additional fields.
};

// ============================================================================
// [BLJpegDecoder - BitStream]
// ============================================================================

//! JPEG decoder's bit-stream.
//!
//! Holds the current decoder position in a bit-stream, but it's not used to
//! fetch bits from it. Use `BLJpegDecoderBitReader` to actually read from the
//! bit-stream.
struct BLJpegDecoderBitStream {
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

  BL_INLINE void reset() noexcept { reset(nullptr, nullptr); }
  BL_INLINE void reset(const uint8_t* ptr_, const uint8_t* end_) noexcept {
    this->ptr = ptr_;
    this->end = end_;
    this->bitData = 0;
    this->bitCount = 0;
    this->eobRun = 0;
  }
};

// ============================================================================
// [BLJpegDecoder - BitReader]
// ============================================================================

//! JPEG decoder's bit-reader.
//!
//! Class that is used to read data from `BLJpegDecoderBitStream`.
struct BLJpegDecoderBitReader {
  //! Data pointer (points to the byte to be processed).
  const uint8_t* ptr;
  //! End of input (points to the first invalid byte).
  const uint8_t* end;
  //! Machine word that contains available bits.
  BLBitWord bitData;
  //! Number of valid bits in `bitData`.
  size_t bitCount;

  BL_INLINE explicit BLJpegDecoderBitReader(BLJpegDecoderBitStream& stream) noexcept
    : ptr(stream.ptr),
      end(stream.end),
      bitData(stream.bitData),
      bitCount(stream.bitCount) {}

  BL_INLINE void done(BLJpegDecoderBitStream& stream) noexcept {
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
    return T(U(bitData >> (blBitSizeOf<BLBitWord>() - n)));
  }

  BL_INLINE void refill() noexcept {
    while (bitCount <= blBitSizeOf<BLBitWord>() - 8 && ptr != end) {
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

      bitData += BLBitWord(tmpByte) << ((blBitSizeOf<BLBitWord>() - 8) - bitCount);
      bitCount += 8;
    }
  }

  BL_INLINE void refillIf32Bit() noexcept {
    if (blBitSizeOf<BLBitWord>() <= 32)
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

    int32_t tmpMask = blBitShl(int32_t(-1), n) + 1;
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
  BL_INLINE BLResult readCode(T& dst, const BLJpegDecoderHuffmanTable* table) noexcept {
    uint32_t code = table->accel[peek(BL_JPEG_DECODER_HUFFMAN_ACCEL_BITS)];
    uint32_t codeSize;

    if (code < 255) {
      // FAST: Look at the top bits and determine the symbol id.
      codeSize = table->size[code];
      if (BL_UNLIKELY(codeSize > bitCount))
        return blTraceError(BL_ERROR_DECOMPRESSION_FAILED);
    }
    else {
      // SLOW: Naive test is to shift the `bitData` down so `s` bits are valid,
      // then test against `maxCode`. To speed this up, we've preshifted maxCode
      // left so that it has `16-s` 0s at the end; in other words, regardless of
      // the number of bits, it wants to be compared against something shifted to
      // have 16; that way we don't need to shift inside the loop.
      code = peek<uint32_t>(16);
      codeSize = BL_JPEG_DECODER_HUFFMAN_ACCEL_BITS + 1;

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

// ============================================================================
// [BLJpegDecoder - BuildHuffman]
// ============================================================================

BL_HIDDEN BLResult blJpegDecoderBuildHuffmanAC(BLJpegDecoderHuffmanACTable* table, const uint8_t* data, size_t dataSize, size_t* bytesConsumed) noexcept;
BL_HIDDEN BLResult blJpegDecoderBuildHuffmanDC(BLJpegDecoderHuffmanDCTable* table, const uint8_t* data, size_t dataSize, size_t* bytesConsumed) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_CODEC_BLJPEGHUFFMAN_P_H
