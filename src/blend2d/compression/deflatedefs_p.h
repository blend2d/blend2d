// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_COMPRESSION_DEFLATEDEFS_P_H_INCLUDED
#define BLEND2D_COMPRESSION_DEFLATEDEFS_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../array.h"

//! \cond INTERNAL

namespace BLCompression {
namespace Deflate {

enum FormatType : uint32_t {
  kFormatRaw,
  kFormatZlib
};

//! Block type.
enum BlockType : uint32_t {
  kBlockTypeUncompressed = 0,
  kBlockTypeStaticHuffman = 1,
  kBlockTypeDynamicHuffman = 2
};

//! Limits
enum Limits : uint32_t {
  //! Minimum supported match length (in bytes).
  kMinMatchLen = 3,
  //! Maximum supported match length (in bytes).
  kMaxMatchLen = 258,

  //! Minimum supported match offset (in bytes).
  kMinMatchOffset = 1,
  //! Maximum supported match offset (in bytes).
  kMaxMatchOffset = 32768,

  //! Maximum window size.
  kMaxWindowSize = 32768,

  // Number of symbols in each Huffman code.
  //
  // NOTE for the literal/length and offset codes, these are actually the
  // maximum values; a given block might use fewer symbols.
  kNumPrecodeSymbols = 19,
  kNumLitLenSymbols = 288,
  kNumOffsetSymbols = 32,

  //! The maximum number of symbols across all codes.
  kMaxSymbolCount = 288,

  // Division of symbols in the literal/length code
  kNumLiterals = 256,
  kEndOfBlock = 256,
  kNumLengthSymbols = 31,

  // Maximum codeword length, in bits, within each Huffman code
  kMaxPreCodeWordLen = 7,
  kMaxLitLenCodeWordLen = 15,
  kMaxOffsetCodeWordLen = 15,

  //! The maximum codeword length across all codes.
  kMaxCodeWordLen = 15,

  //! Maximum possible overrun when decoding codeword lengths .
  kMaxLensOverrun = 137,

  // Maximum number of extra bits that may be required to represent a match length.
  kMaxExtraLengthBits = 5,
  // Maximum number of extra bits that may be required to represent a match offset.
  kMaxExtraOffsetBits = 14,

  // The maximum number of bits in which a match can be represented. This
  // is the absolute worst case, which assumes the longest possible Huffman
  // codewords and the maximum numbers of extra bits.
  kMaxMatchBits = kMaxLitLenCodeWordLen + kMaxExtraLengthBits + kMaxOffsetCodeWordLen + kMaxExtraOffsetBits
};

#define DIV_ROUND_UP(n, d)  (((n) + (d) - 1) / (d))

static BL_INLINE uint32_t loaded_u32_to_u24(uint32_t v) noexcept
{
  return BL_BYTE_ORDER == 1234 ? v & 0xFFFFFFu : v >> 8;
}

} // {Deflate}
} // {BLCompression}

//! \endcond

#endif // BLEND2D_COMPRESSION_DEFLATEDEFS_P_H_INCLUDED
