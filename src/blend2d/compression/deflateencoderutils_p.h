// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_COMPRESSION_DEFLATEENCODERUTILS_P_H_INCLUDED
#define BLEND2D_COMPRESSION_DEFLATEENCODERUTILS_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../compression/deflatedefs_p.h"
#include "../support/intops_p.h"
#include "../support/memops_p.h"
#include "../support/ptrops_p.h"

//! \cond INTERNAL

namespace bl {
namespace Compression {
namespace Deflate {

// We want to write machine work quantities, so the minimum padding is a machine word.
static constexpr uint32_t kMinOutputBufferPadding = sizeof(BLBitWord);

namespace {

static constexpr bool canBufferN(size_t n) noexcept { return n + 7 < IntOps::bitSizeOf<BLBitWord>(); }

struct OutputBuffer {
  //! Pointer to the beginning of the output buffer.
  uint8_t* begin;
  //! Current pointer in the output buffer (points to the position where a next byte will be written).
  uint8_t* ptr;
  //! End of the output buffer (pointer to a position which is past the last byte).
  uint8_t* end;

  BL_INLINE_NODEBUG void init(uint8_t* output, size_t size) noexcept {
    BL_ASSERT(size >= kMinOutputBufferPadding);
    begin = output;
    ptr = output;
    end = output + size - kMinOutputBufferPadding;
  }

  BL_INLINE_NODEBUG void reset() noexcept { *this = OutputBuffer{}; }
  BL_INLINE_NODEBUG bool canWrite() const noexcept { return ptr < end; }

  BL_INLINE_NODEBUG size_t byteOffset() const noexcept { return PtrOps::byteOffset(begin, ptr); }
  BL_INLINE_NODEBUG size_t remainingBytes() const noexcept { return PtrOps::bytesUntil(ptr, end); }
};

//! Bit-buffer used by the output stream.
//!
//! Bit buffer can hold at most 31 bits on a 32-bit target and 63 bits on a 64-bit target. The reason is that when
//! shifting more bits is undefined behavior (and most ISA implementations would shift nothing if the number of the
//! bits to shift is the same as the machine word size), so we avoid it by making sure the bit-buffer is never 100%
//! full.
struct OutputBits {
  //! Bits to flush.
  BLBitWord bitWord;
  //! Number of bits in `bitWord`, cannot exceed `sizeof(bitWord) * 8 - 1`.
  size_t bitLength;

  BL_INLINE_NODEBUG void reset() noexcept { *this = OutputBits{}; }

  BL_INLINE_NODEBUG BLBitWord all() const noexcept { return bitWord; }
  BL_INLINE_NODEBUG size_t length() const noexcept { return bitLength; }

  BL_INLINE_NODEBUG bool empty() const noexcept { return bitLength == 0; }
  BL_INLINE_NODEBUG bool wasProperlyFlushed() const noexcept { return bitLength <= 7 && (bitWord >> bitLength) == 0; }
  BL_INLINE_NODEBUG size_t remainingBits() const noexcept { return (IntOps::bitSizeOf<BLBitWord>() - 1u) - bitLength; }

  template<typename T>
  BL_INLINE_NODEBUG void add(const T& bits, size_t count) noexcept {
    BL_ASSERT(bitLength + count < IntOps::bitSizeOf<BLBitWord>());

    bitWord |= size_t(bits) << bitLength;
    bitLength += count;
  }

  BL_INLINE_NODEBUG void alignToBytes() noexcept {
    bitLength = (bitLength + 7u) & ~size_t(7);
  }

  BL_INLINE_NODEBUG void flush(OutputBuffer& buffer) noexcept {
    size_t n = bitLength / 8u;

    BL_ASSERT(n != IntOps::bitSizeOf<BLBitWord>());
    BL_ASSERT(buffer.canWrite());

    if BL_CONSTEXPR (MemOps::kUnalignedMemIO) {
      MemOps::storeu_le(buffer.ptr, bitWord);
      buffer.ptr += n;

      bitWord >>= n * 8u;
      bitLength &= 7;
    }
    else {
      // Flush a byte at a time.
      while (n) {
        buffer.ptr[0] = uint8_t(bitWord & 0xFFu);
        buffer.ptr++;
        bitWord >>= 8;
        n--;
      }
      bitLength &= 7;
    }
  }

  template<size_t kN>
  BL_INLINE_NODEBUG void flushIfCannotBufferN(OutputBuffer& buffer) noexcept {
    if BL_CONSTEXPR (!canBufferN(kN)) {
      flush(buffer);
    }
  }

  BL_INLINE_NODEBUG void flushFinalByte(OutputBuffer& buffer) noexcept {
    if (!empty()) {
      BL_ASSERT(length() <= 7u);
      buffer.ptr[0] = uint8_t(bitWord & 0xFFu);
      buffer.ptr++;

      bitWord = 0;
      bitLength = 0;
    }
  }
};

//! Output stream combines `OutputBits` and `OutputBuffer` - so it offers both bit & byte granularity.
struct OutputStream {
  //! Output bit-buffer contains N bits up to `sizeof(BLBitWord) * 8 - 1`, which flushed to output `buffer`.
  OutputBits bits;
  //! Output buffer and a position in it (ptr).
  OutputBuffer buffer;
};

} // {anonymous}

} // {Deflate}
} // {Compression}
} // {bl}

//! \endcond

#endif // BLEND2D_COMPRESSION_DEFLATEENCODERUTILS_P_H_INCLUDED
