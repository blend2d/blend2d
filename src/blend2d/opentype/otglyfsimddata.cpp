// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#if defined(BL_BUILD_OPT_SSE4_2) || defined(BL_BUILD_OPT_ASIMD)

#include "../opentype/otglyfsimddata_p.h"

namespace bl {
namespace OpenType {
namespace GlyfImpl {

alignas(16) const uint8_t convertFlagsPredicate[64] = {
  // The first 16 bytes are used as a predicate for PSHUFB.

  0x4C, // [0|0|    ?|    ?|     0|    0|    0|      0] -> [0     |Off| 0|0|1|1|    0|    0]
  0x2C, // [0|0|    ?|    ?|     0|    0|    0|OnCurve] -> [0     |  0|On|0|1|1|    0|    0]
  0x4D, // [0|0|    ?|    ?|     0|    0|xByte|      0] -> [0     |Off| 0|0|1|1|    0|xByte]
  0x2D, // [0|0|    ?|    ?|     0|    0|xByte|OnCurve] -> [0     |  0|On|0|1|1|    0|xByte]
  0x4E, // [0|0|    ?|    ?|     0|yByte|    0|      0] -> [0     |Off| 0|0|1|1|yByte|    0]
  0x2E, // [0|0|    ?|    ?|     0|yByte|    0|OnCurve] -> [0     |  0|On|0|1|1|yByte|    0]
  0x4F, // [0|0|    ?|    ?|     0|yByte|xByte|      0] -> [0     |Off| 0|0|1|1|yByte|xByte]
  0x2F, // [0|0|    ?|    ?|     0|yByte|xByte|OnCurve] -> [0     |  0|On|0|1|1|yByte|xByte]
  0xCC, // [0|0|    ?|    ?|Repeat|    0|    0|      0] -> [Repeat|Off| 0|0|1|1|    0|    0]
  0xAC, // [0|0|    ?|    ?|Repeat|    0|    0|OnCurve] -> [Repeat|  0|On|0|1|1|    0|    0]
  0xCD, // [0|0|    ?|    ?|Repeat|    0|xByte|      0] -> [Repeat|Off| 0|0|1|1|    0|xByte]
  0xAD, // [0|0|    ?|    ?|Repeat|    0|xByte|OnCurve] -> [Repeat|  0|On|0|1|1|    0|xByte]
  0xCE, // [0|0|    ?|    ?|Repeat|yByte|    0|      0] -> [Repeat|Off| 0|0|1|1|yByte|    0]
  0xAE, // [0|0|    ?|    ?|Repeat|yByte|    0|OnCurve] -> [Repeat|  0|On|0|1|1|yByte|    0]
  0xCF, // [0|0|    ?|    ?|Repeat|yByte|xByte|      0] -> [Repeat|Off| 0|0|1|1|yByte|xByte]
  0xAF, // [0|0|    ?|    ?|Repeat|yByte|xByte|OnCurve] -> [Repeat|  0|On|0|1|1|yByte|xByte]

  // The last 48 bytes are only used by a slow flags decoding loop when some flag in 8-flag chunk repeats.

  0x48, // [0|0|    0|xSame|     0|    0|    0|      0] -> [0     |Off| 0|0|1|0|    0|    0]
  0x28, // [0|0|    0|xSame|     0|    0|    0|OnCurve] -> [0     |  0|On|0|1|0|    0|    0]
  0x49, // [0|0|    0|xSame|     0|    0|xByte|      0] -> [0     |Off| 0|0|1|0|    0|xByte]
  0x29, // [0|0|    0|xSame|     0|    0|xByte|OnCurve] -> [0     |  0|On|0|1|0|    0|xByte]
  0x4A, // [0|0|    0|xSame|     0|yByte|    0|      0] -> [0     |Off| 0|0|1|0|yByte|    0]
  0x2A, // [0|0|    0|xSame|     0|yByte|    0|OnCurve] -> [0     |  0|On|0|1|0|yByte|    0]
  0x4B, // [0|0|    0|xSame|     0|yByte|xByte|      0] -> [0     |Off| 0|0|1|0|yByte|xByte]
  0x2B, // [0|0|    0|xSame|     0|yByte|xByte|OnCurve] -> [0     |  0|On|0|1|0|yByte|xByte]
  0xC8, // [0|0|    0|xSame|Repeat|    0|    0|      0] -> [Repeat|Off| 0|0|1|0|    0|    0]
  0xA8, // [0|0|    0|xSame|Repeat|    0|    0|OnCurve] -> [Repeat|  0|On|0|1|0|    0|    0]
  0xC9, // [0|0|    0|xSame|Repeat|    0|xByte|      0] -> [Repeat|Off| 0|0|1|0|    0|xByte]
  0xA9, // [0|0|    0|xSame|Repeat|    0|xByte|OnCurve] -> [Repeat|  0|On|0|1|0|    0|xByte]
  0xCA, // [0|0|    0|xSame|Repeat|yByte|    0|      0] -> [Repeat|Off| 0|0|1|0|yByte|    0]
  0xAA, // [0|0|    0|xSame|Repeat|yByte|    0|OnCurve] -> [Repeat|  0|On|0|1|0|yByte|    0]
  0xCB, // [0|0|    0|xSame|Repeat|yByte|xByte|      0] -> [Repeat|Off| 0|0|1|0|yByte|xByte]
  0xAB, // [0|0|    0|xSame|Repeat|yByte|xByte|OnCurve] -> [Repeat|  0|On|0|1|0|yByte|xByte]

  0x44, // [0|0|ySame|    0|     0|    0|    0|      0] -> [0     |Off| 0|0|0|1|    0|    0]
  0x24, // [0|0|ySame|    0|     0|    0|    0|OnCurve] -> [0     |  0|On|0|0|1|    0|    0]
  0x45, // [0|0|ySame|    0|     0|    0|xByte|      0] -> [0     |Off| 0|0|0|1|    0|xByte]
  0x25, // [0|0|ySame|    0|     0|    0|xByte|OnCurve] -> [0     |  0|On|0|0|1|    0|xByte]
  0x46, // [0|0|ySame|    0|     0|yByte|    0|      0] -> [0     |Off| 0|0|0|1|yByte|    0]
  0x26, // [0|0|ySame|    0|     0|yByte|    0|OnCurve] -> [0     |  0|On|0|0|1|yByte|    0]
  0x47, // [0|0|ySame|    0|     0|yByte|xByte|      0] -> [0     |Off| 0|0|0|1|yByte|xByte]
  0x27, // [0|0|ySame|    0|     0|yByte|xByte|OnCurve] -> [0     |  0|On|0|0|1|yByte|xByte]
  0xC4, // [0|0|ySame|    0|Repeat|    0|    0|      0] -> [Repeat|Off| 0|0|0|1|    0|    0]
  0xA4, // [0|0|ySame|    0|Repeat|    0|    0|OnCurve] -> [Repeat|  0|On|0|0|1|    0|    0]
  0xC5, // [0|0|ySame|    0|Repeat|    0|xByte|      0] -> [Repeat|Off| 0|0|0|1|    0|xByte]
  0xA5, // [0|0|ySame|    0|Repeat|    0|xByte|OnCurve] -> [Repeat|  0|On|0|0|1|    0|xByte]
  0xC6, // [0|0|ySame|    0|Repeat|yByte|    0|      0] -> [Repeat|Off| 0|0|0|1|yByte|    0]
  0xA6, // [0|0|ySame|    0|Repeat|yByte|    0|OnCurve] -> [Repeat|  0|On|0|0|1|yByte|    0]
  0xC7, // [0|0|ySame|    0|Repeat|yByte|xByte|      0] -> [Repeat|Off| 0|0|0|1|yByte|xByte]
  0xA7, // [0|0|ySame|    0|Repeat|yByte|xByte|OnCurve] -> [Repeat|  0|On|0|0|1|yByte|xByte]

  0x40, // [0|0|ySame|xSame|     0|    0|    0|      0] -> [0     |Off| 0|0|0|0|    0|    0]
  0x20, // [0|0|ySame|xSame|     0|    0|    0|OnCurve] -> [0     |  0|On|0|0|0|    0|    0]
  0x41, // [0|0|ySame|xSame|     0|    0|xByte|      0] -> [0     |Off| 0|0|0|0|    0|xByte]
  0x21, // [0|0|ySame|xSame|     0|    0|xByte|OnCurve] -> [0     |  0|On|0|0|0|    0|xByte]
  0x42, // [0|0|ySame|xSame|     0|yByte|    0|      0] -> [0     |Off| 0|0|0|0|yByte|    0]
  0x22, // [0|0|ySame|xSame|     0|yByte|    0|OnCurve] -> [0     |  0|On|0|0|0|yByte|    0]
  0x43, // [0|0|ySame|xSame|     0|yByte|xByte|      0] -> [0     |Off| 0|0|0|0|yByte|xByte]
  0x23, // [0|0|ySame|xSame|     0|yByte|xByte|OnCurve] -> [0     |  0|On|0|0|0|yByte|xByte]
  0xC0, // [0|0|ySame|xSame|Repeat|    0|    0|      0] -> [Repeat|Off| 0|0|0|0|    0|    0]
  0xA0, // [0|0|ySame|xSame|Repeat|    0|    0|OnCurve] -> [Repeat|  0|On|0|0|0|    0|    0]
  0xC1, // [0|0|ySame|xSame|Repeat|    0|xByte|      0] -> [Repeat|Off| 0|0|0|0|    0|xByte]
  0xA1, // [0|0|ySame|xSame|Repeat|    0|xByte|OnCurve] -> [Repeat|  0|On|0|0|0|    0|xByte]
  0xC2, // [0|0|ySame|xSame|Repeat|yByte|    0|      0] -> [Repeat|Off| 0|0|0|0|yByte|    0]
  0xA2, // [0|0|ySame|xSame|Repeat|yByte|    0|OnCurve] -> [Repeat|  0|On|0|0|0|yByte|    0]
  0xC3, // [0|0|ySame|xSame|Repeat|yByte|xByte|      0] -> [Repeat|Off| 0|0|0|0|yByte|xByte]
  0xA3  // [0|0|ySame|xSame|Repeat|yByte|xByte|OnCurve] -> [Repeat|  0|On|0|0|0|yByte|xByte]
};

alignas(8) const uint8_t overflowFlagsPredicate[32] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
};

alignas(16) const uint8_t sizesPerXYPredicate[16] = {
  0x00, // [0|0|0|0| ySame| xSame|    0|    0] -> {y=0, x=0}
  0x01, // [0|0|0|0| ySame| xSame|    0|xByte] -> {y=0, x=1}
  0x10, // [0|0|0|0| ySame| xSame|yByte|    0] -> {y=1, x=0}
  0x11, // [0|0|0|0| ySame| xSame|yByte|xByte] -> {y=1, x=1}
  0x02, // [0|0|0|0| ySame|!xSame|    0|    0] -> {y=0, x=2}
  0x01, // [0|0|0|0| ySame|!xSame|    0|xByte] -> {y=0, x=1}
  0x12, // [0|0|0|0| ySame|!xSame|yByte|    0] -> {y=1, x=2}
  0x11, // [0|0|0|0| ySame|!xSame|yByte|xByte] -> {y=1, x=1}
  0x20, // [0|0|0|0|!ySame| xSame|    0|    0] -> {y=2, x=0}
  0x21, // [0|0|0|0|!ySame| xSame|    0|xByte] -> {y=2, x=1}
  0x10, // [0|0|0|0|!ySame| xSame|yByte|    0] -> {y=1, x=0}
  0x11, // [0|0|0|0|!ySame| xSame|yByte|xByte] -> {y=1, x=1}
  0x22, // [0|0|0|0|!ySame|!xSame|    0|    0] -> {y=2, x=2}
  0x21, // [0|0|0|0|!ySame|!xSame|    0|xByte] -> {y=2, x=1}
  0x12, // [0|0|0|0|!ySame|!xSame|yByte|    0] -> {y=1, x=2}
  0x11  // [0|0|0|0|!ySame|!xSame|yByte|xByte] -> {y=1, x=1}
};

static constexpr uint8_t kDecodeImmOpZero    = 0x80; // hi={0x8x} lo={0x8x}
static constexpr uint8_t kDecodeImmOpWord    = 0x00; // hi={0x00} lo={0x01}
static constexpr uint8_t kDecodeImmOpBytePos = 0xCF; // hi={0x8x} lo={0x00}
static constexpr uint8_t kDecodeImmOpByteNeg = 0xEF; // hi={0xAx} lo={0x20}

alignas(16) const uint8_t decodeOpXTable[16] = {
  kDecodeImmOpZero,    // [0|?|?|?|?| xSame|?|    0]
  kDecodeImmOpBytePos, // [0|?|?|?|?| xSame|?|xByte]
  kDecodeImmOpZero,    // [0|?|?|?|?| xSame|?|    0]
  kDecodeImmOpBytePos, // [0|?|?|?|?| xSame|?|xByte]
  kDecodeImmOpWord,    // [0|?|?|?|?|!xSame|?|    0]
  kDecodeImmOpByteNeg, // [0|?|?|?|?|!xSame|?|xByte]
  kDecodeImmOpWord,    // [0|?|?|?|?|!xSame|?|    0]
  kDecodeImmOpByteNeg, // [0|?|?|?|?|!xSame|?|xByte]
  kDecodeImmOpZero,    // [0|?|?|?|?| xSame|?|    0]
  kDecodeImmOpBytePos, // [0|?|?|?|?| xSame|?|xByte]
  kDecodeImmOpZero,    // [0|?|?|?|?| xSame|?|    0]
  kDecodeImmOpBytePos, // [0|?|?|?|?| xSame|?|xByte]
  kDecodeImmOpWord,    // [0|?|?|?|?|!xSame|?|    0]
  kDecodeImmOpByteNeg, // [0|?|?|?|?|!xSame|?|xByte]
  kDecodeImmOpWord,    // [0|?|?|?|?|!xSame|?|    0]
  kDecodeImmOpByteNeg  // [0|?|?|?|?|!xSame|?|xByte]
};

alignas(16) const uint8_t decodeOpYTable[16] = {
  kDecodeImmOpZero,    // [0|?|?|?| ySame|?|    0|?]
  kDecodeImmOpZero,    // [0|?|?|?| ySame|?|    0|?]
  kDecodeImmOpBytePos, // [0|?|?|?| ySame|?|yByte|?]
  kDecodeImmOpBytePos, // [0|?|?|?| ySame|?|yByte|?]
  kDecodeImmOpZero,    // [0|?|?|?| ySame|?|    0|?]
  kDecodeImmOpZero,    // [0|?|?|?| ySame|?|    0|?]
  kDecodeImmOpBytePos, // [0|?|?|?| ySame|?|yByte|?]
  kDecodeImmOpBytePos, // [0|?|?|?| ySame|?|yByte|?]
  kDecodeImmOpWord,    // [0|?|?|?|!ySame|?|    0|?]
  kDecodeImmOpWord,    // [0|?|?|?|!ySame|?|    0|?]
  kDecodeImmOpByteNeg, // [0|?|?|?|!ySame|?|yByte|?]
  kDecodeImmOpByteNeg, // [0|?|?|?|!ySame|?|yByte|?]
  kDecodeImmOpWord,    // [0|?|?|?|!ySame|?|    0|?]
  kDecodeImmOpWord,    // [0|?|?|?|!ySame|?|    0|?]
  kDecodeImmOpByteNeg, // [0|?|?|?|!ySame|?|yByte|?]
  kDecodeImmOpByteNeg  // [0|?|?|?|!ySame|?|yByte|?]
};

} // {GlyfImpl}
} // {OpenType}
} // {bl}

#endif // BL_BUILD_OPT_SSE4_2 || BL_BUILD_OPT_ASIMD
