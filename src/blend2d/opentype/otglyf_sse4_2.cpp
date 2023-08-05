// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"

// NOTE: In general this implementation would work when SSSE3 is available, however, we really want to use POPCNT,
// which is available from SSE4.2 (Intel) and SSE4a (AMD). So we require SSE4.2 to make sure both SSSE3 and POPCNT
// are present.
#ifdef BL_BUILD_OPT_SSE4_2

#include "../api-build_p.h"
#include "../font_p.h"
#include "../geometry_p.h"
#include "../matrix_p.h"
#include "../path_p.h"
#include "../opentype/otface_p.h"
#include "../opentype/otglyf_p.h"
#include "../support/intops_p.h"
#include "../support/memops_p.h"
#include "../support/scopedbuffer_p.h"
#include "../tables/tables_p.h"

namespace BLOpenType {
namespace GlyfImpl {

// BLOpenType::GlyfImpl - GetGlyphOutlines (SSE4.2 | AVX2)
// =======================================================

//! Flags that are used by the vectorized outline decoder implementation.
//!
//! Most of the flags are the same as flags used in TT outlines, however, the following modifications were made in
//! order to make the implementation faster:
//!
//!   1. XByte|YByte|XSame|YSame flags were moved to [3:0] bits so they can be used as a predicate with VPSHUFB
//!      instruction. These 4 bits are the only important bits to decode X/Y vertices.
//!   2. XSameOrPositive and YSameOrPositive flags were negated. After negation when all [3:0] bits are zero, the
//!      vertex is zero as well. This is required when processing multiple flags at once at the end. Extra flags
//!      in a loop that processes 8 or 16 flags at a time are zero, thus they don't contribute to X/Y data lenghts.
//!   3. OnCurve flag and its complement flag (OffCurve) are stored next to each other. When these flags are shifted
//!      to [1:0] bits they represent either `BL_PATH_CMD_ON` or `BL_PATH_CMD_QUAD` commands, which is handy in the
//!      last loop that appends vertices.
//!   4. Additional OffSpline flag is a combination of OffCurve flag with previous OffCurve flag. If both were set
//!      then this flag would have OffSpline set as well. This is important for counting how many off-curve splines
//!      are in the data, and later in the last loop to check whether we are in off curve spline or not.
//!   5. Repeat flag is last so we can use VPMOVMSKB instruction to quickly check for repeated flags.
enum VecFlags : uint8_t {
  kVecFlagXByte = 0x01,
  kVecFlagYByte = 0x02,
  kVecFlagXNotSameOrPositive = 0x04,
  kVecFlagYNotSameOrPositive = 0x08,
  kVecFlagOffSpline = 0x10,
  kVecFlagOnCurve = 0x20,
  kVecFlagOffCurve = 0x40,
  kVecFlagRepeat = 0x80
};

static constexpr uint32_t kVecFlagOnCurveShift = BLIntOps::ctzStatic(kVecFlagOnCurve);

namespace {

using namespace SIMD;

alignas(16) static const uint8_t convertFlagsPredicate[64] = {
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

alignas(8) static const uint8_t overflowFlagsPredicate[32] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
};

// Vertex size for each ySame|xSame|yByte|xByte combination.
alignas(16) static const uint8_t sizesPerXYPredicate[16] = {
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

// These tables provide PSHUFB predicate (and additional payload) for decoding X/Y coordinates based on flags.
// The trick is to add 0x41 to each even byte to create a predicate for both LO and HI byte based on a single
// table. We add values to the LO byte as TT words are stored in big endian, so this trick makes byteswapping
// of the input words for free.
//
// NOTEs:
//   PSHUFB only uses [7] and [3:0] bits, other bits are ignored
//   0x20 means negation of both LO and HI bytes (single byte decode having X/YSameOrPositive == 0).
//   hi = (val     ) & 0x8F
//   lo = (val+0x41) & 0x8F
static constexpr const uint8_t kDecodeImmOpZero    = 0x80; // hi={0x8x} lo={0x8x}
static constexpr const uint8_t kDecodeImmOpWord    = 0x00; // hi={0x00} lo={0x01}
static constexpr const uint8_t kDecodeImmOpBytePos = 0xCF; // hi={0x8x} lo={0x00}
static constexpr const uint8_t kDecodeImmOpByteNeg = 0xEF; // hi={0xAx} lo={0x20}

alignas(16) static const uint8_t decodeOpXTable[16] = {
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

alignas(16) static const uint8_t decodeOpYTable[16] = {
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

// Converts TrueType glyph flags:
//
//   [0|0|YSame|XSame|Repeat|YByte|XByte|OnCurve]
//
// To an internal representation used by SIMD code:
//
//   [Repeat|!OnCurve|OnCurve|0|!YSame|!XSame|YByte|XByte]
static BL_INLINE Vec16xU8 convertFlags(const Vec16xU8& vf, const Vec16xU8& vConvertFlagsPredicate, const Vec16xU8& v0x3030) noexcept {
  Vec16xU8 a = swizzlev_u8(vConvertFlagsPredicate, vf);
  Vec16xU8 b = srli_u16<2>((vf & v0x3030));
  return a ^ b;
}

static BL_INLINE VecPair<Vec16xU8> aggregateVertexSizes(const Vec16xU8& vf, const Vec16xU8& vSizesPerXYPredicate, const Vec16xU8& v0x0F0F) noexcept {
  Vec16xU8 yxSizes = swizzlev_u8(vSizesPerXYPredicate, vf); // [H   G   F   E   D   C   B   A]

  yxSizes = yxSizes + slli_i64<8>(yxSizes);                 // [H:G G:F F:E E:D D:C C:B B:A A]
  yxSizes = yxSizes + slli_i64<16>(yxSizes);                // [H:E G:D F:C E:B D:A C:A B:A A]

  Vec16xU8 ySizes = srli_u64<4>(yxSizes) & v0x0F0F;         // Y sizes separated from YX sizes.
  Vec16xU8 xSizes = yxSizes & v0x0F0F;                      // X sizes separated from YX sizes.

  ySizes = ySizes + slli_i64<32>(ySizes);                   // [H:A G:A F:A E:A D:A C:A B:A A]
  xSizes = xSizes + slli_i64<32>(xSizes);                   // [H:A G:A F:A E:A D:A C:A B:A A]

  return VecPair<Vec16xU8>{xSizes, ySizes};
}

static BL_INLINE Vec4xU32 sumsFromAggregatedSizesOf8Bytes(const VecPair<Vec16xU8>& sizes) noexcept {
  return vec_u32(srli_u64<56>(shuffle_u32<1, 3, 1, 3>(sizes[0], sizes[1])));
}

static BL_INLINE Vec4xU32 sumsFromAggregatedSizesOf16Bytes(const VecPair<Vec16xU8>& sizes) noexcept {
  return vec_u32(srli_u32<24>(shuffle_u32<1, 3, 1, 3>(sizes[0], sizes[1])));
}

struct DecodedVertex {
  int16_t x;
  int16_t y;
};

static BL_INLINE Vec2xF64 transformDecodedVertex(const DecodedVertex* decodedVertex, const Vec2xF64& m00_m11, const Vec2xF64& m10_m01) noexcept {
  Vec4xI32 xy_i32 = vec_i32(unpack_lo64_i16_i32(loada_32<Vec8xI16>(decodedVertex)));

  Vec2xF64 xy_f64 = cvt_2xi32_f64(xy_i32);
  Vec2xF64 yx_f64 = swap_f64(xy_f64);

  return xy_f64 * m00_m11 + yx_f64 * m10_m01;
}

static BL_INLINE void storeVertex(BLPathAppender& appender, uint8_t cmd, const Vec2xF64& vtx) noexcept {
  appender.cmd[0].value = cmd;
  storeu(appender.vtx, vtx);
}

static BL_INLINE void appendVertex(BLPathAppender& appender, uint8_t cmd, const Vec2xF64& vtx) noexcept {
  storeVertex(appender, cmd, vtx);
  appender._advance(1);
}

static BL_INLINE void appendVertex2x(BLPathAppender& appender, uint8_t cmd0, const Vec2xF64& vtx0, uint8_t cmd1, const Vec2xF64& vtx1) noexcept {
  appender.cmd[0].value = cmd0;
  appender.cmd[1].value = cmd1;
  storeu(appender.vtx + 0, vtx0);
  storeu(appender.vtx + 1, vtx1);
  appender._advance(2);
}

} // {anonymous}

#ifndef getGlyphOutlines_SIMD
  #define getGlyphOutlines_SIMD getGlyphOutlines_SSE4_2
#endif

BLResult BL_CDECL getGlyphOutlines_SIMD(
  const BLFontFaceImpl* faceI_,
  BLGlyphId glyphId,
  const BLMatrix2D* transform,
  BLPath* out,
  size_t* contourCountOut,
  BLScopedBuffer* tmpBuffer) noexcept {

  using namespace SIMD;

  const OTFaceImpl* faceI = static_cast<const OTFaceImpl*>(faceI_);

  typedef GlyfTable::Simple Simple;
  typedef GlyfTable::Compound Compound;

  if (BL_UNLIKELY(glyphId >= faceI->faceInfo.glyphCount))
    return blTraceError(BL_ERROR_INVALID_GLYPH);

  RawTable glyfTable = faceI->glyf.glyfTable;
  RawTable locaTable = faceI->glyf.locaTable;
  uint32_t locaOffsetSize = faceI->locaOffsetSize();

  const uint8_t* gPtr = nullptr;
  size_t remainingSize = 0;
  size_t compoundLevel = 0;

  // Only matrix and compoundFlags are important in the root entry.
  CompoundEntry compoundData[CompoundEntry::kMaxLevel];
  compoundData[0].gPtr = nullptr;
  compoundData[0].remainingSize = 0;
  compoundData[0].compoundFlags = Compound::kArgsAreXYValues;
  compoundData[0].transform = *transform;

  BLPathAppender appender;
  size_t contourCountTotal = 0;

  for (;;) {
    size_t offset;
    size_t endOff;
    size_t remainingSizeAfterGlyphData;

    // NOTE: Maximum glyphId is 65535, so we are always safe here regarding multiplying the `glyphId` by 2 or 4
    // to calculate the correct index.
    if (locaOffsetSize == 2) {
      size_t index = size_t(glyphId) * 2u;
      if (BL_UNLIKELY(index + sizeof(UInt16) * 2u > locaTable.size))
        goto InvalidData;
      offset = uint32_t(reinterpret_cast<const UInt16*>(locaTable.data + index + 0)->value()) * 2u;
      endOff = uint32_t(reinterpret_cast<const UInt16*>(locaTable.data + index + 2)->value()) * 2u;
    }
    else {
      size_t index = size_t(glyphId) * 4u;
      if (BL_UNLIKELY(index + sizeof(UInt32) * 2u > locaTable.size))
        goto InvalidData;
      offset = reinterpret_cast<const UInt32*>(locaTable.data + index + 0)->value();
      endOff = reinterpret_cast<const UInt32*>(locaTable.data + index + 4)->value();
    }

    remainingSizeAfterGlyphData = glyfTable.size - endOff;

    // Simple or Empty Glyph
    // ---------------------

    if (BL_UNLIKELY(offset >= endOff || endOff > glyfTable.size)) {
      // Only ALLOWED when `offset == endOff`.
      if (BL_UNLIKELY(offset != endOff || endOff > glyfTable.size))
        goto InvalidData;
    }
    else {
      gPtr = glyfTable.data + offset;
      remainingSize = endOff - offset;

      if (BL_UNLIKELY(remainingSize < sizeof(GlyfTable::GlyphData)))
        goto InvalidData;

      int contourCountSigned = reinterpret_cast<const GlyfTable::GlyphData*>(gPtr)->numberOfContours();
      if (contourCountSigned > 0) {
        size_t contourCount = size_t(unsigned(contourCountSigned));
        BLOverflowFlag of = 0;

        // Minimum data size is:
        //   10                     [GlyphData header]
        //   (numberOfContours * 2) [endPtsOfContours]
        //   2                      [instructionLength]
        gPtr += sizeof(GlyfTable::GlyphData);
        remainingSize = BLIntOps::subOverflow(remainingSize, sizeof(GlyfTable::GlyphData) + contourCount * 2u + 2u, &of);
        if (BL_UNLIKELY(of))
          goto InvalidData;

        const UInt16* contourArray = reinterpret_cast<const UInt16*>(gPtr);
        gPtr += contourCount * 2u;
        contourCountTotal += contourCount;

        // We don't use hinting instructions, so skip them.
        size_t instructionCount = BLMemOps::readU16uBE(gPtr);
        remainingSize = BLIntOps::subOverflow(remainingSize, instructionCount, &of);
        if (BL_UNLIKELY(of))
          goto InvalidData;

        gPtr += 2u + instructionCount;
        const uint8_t* gEnd = gPtr + remainingSize;

        // Number of vertices in TrueType sense (could be less than a number of points required by BLPath
        // representation, especially if TT outline contains consecutive off-curve points).
        size_t ttVertexCount = size_t(contourArray[contourCount - 1].value()) + 1u;

        // Only try to decode vertices if there is more than 1.
        if (ttVertexCount > 1u) {
          // Read TrueType Flags Data
          // ------------------------

          // We need 3 temporary buffers:
          //
          //  - fDataPtr - Converted flags data. These flags represent the same flags as used by TrueType, however,
          //               the bits representing each value are different so they can be used in VPSHUFB.
          //  - xPredPtr - Buffer that is used to calculate predicates for X coordinates.
          //  - yPredPtr - Buffer that is used to calculate predicates for X coordinates.
          //
          // The `xPredPtr` and `yPredPtr` buffers contain data grouped for 8 flags. Each byte contains the side of
          // the coordinate (either 0, 1, or 2 bytes are used in TrueType data) aggregated in the following way:
          //
          // Input cordinate sizes      = [A B C D E F G H]
          // Aggregated in [x|y]PredPtr = [A A+B A+B+C A+B+C+D A+B+C+D+E A+B+C+D+E+F A+B+C+D+E+F+G A+B+C+D+E+F+G+H]
          //
          // The aggregated sizes are very useful, because they describe where each vertex starts in decode buffer.

#ifdef BL_TARGET_OPT_AVX2
          static constexpr uint32_t kDataAlignment = 32;
#else
          static constexpr uint32_t kDataAlignment = 16;
#endif

          uint8_t* fDataPtr = static_cast<uint8_t*>(tmpBuffer->alloc(ttVertexCount * 3 + kDataAlignment * 6));
          if (BL_UNLIKELY(!fDataPtr))
            return blTraceError(BL_ERROR_OUT_OF_MEMORY);

          fDataPtr = BLIntOps::alignUp(fDataPtr, kDataAlignment);
          uint8_t* xPredPtr = fDataPtr + BLIntOps::alignUp(ttVertexCount, kDataAlignment) + kDataAlignment;
          uint8_t* yPredPtr = xPredPtr + BLIntOps::alignUp(ttVertexCount, kDataAlignment) + kDataAlignment;

          // Sizes of xCoordinates[] and yCoordinates[] arrays in TrueType data.
          size_t xCoordinatesSize;
          size_t yCoordinatesSize;

          size_t offCurveSplineCount = 0;

          {
            Vec16xU8 v0x3030 = blCommonTable.i_3030303030303030.as<Vec16xU8>();
            Vec16xU8 v0x0F0F = blCommonTable.i_0F0F0F0F0F0F0F0F.as<Vec16xU8>();
            Vec16xU8 v0x8080 = blCommonTable.i_8080808080808080.as<Vec16xU8>();
            Vec16xU8 vSizesPerXYPredicate = loada<Vec16xU8>(sizesPerXYPredicate);
            Vec16xU8 vConvertFlagsPredicate = loada<Vec16xU8>(convertFlagsPredicate);

            Vec4xU32 vSumXY = make_zero<Vec4xU32>();
            Vec16xU8 vPrevFlags = make_zero<Vec16xU8>();

            size_t i = 0;

            // We want to read 16 bytes in main loop. This means that in the worst case we will read more than 15 bytes
            // than necessary (if reading a last flag via a 16-byte load). We must make sure that there are such bytes.
            // Instead of doing such checks in a loop, we check it here and go to the slow loop if we are at the end of
            // glyph table and 16-byte loads would read beyond. It's very unlikely, but we have to make sure it won't
            // happen.
            size_t slowFlagsDecodeFinishedCheck = BLIntOps::allOnes<size_t>();
            if (remainingSize + remainingSizeAfterGlyphData < ttVertexCount + 15)
              goto SlowFlagsDecode;

            // There is some space ahead, so try to leave slow flags decode loop after an 8-flag chunk has been decoded.
            slowFlagsDecodeFinishedCheck = 0;

            do {
              {
                size_t n = blMin<size_t>(ttVertexCount - i, 16);

                Vec16xU8 vp = loadu<Vec16xU8>(overflowFlagsPredicate + 16 - n);
                Vec16xU8 vf = swizzlev_u8(convertFlags(loadu<Vec16xU8>(gPtr - 16 + n), vConvertFlagsPredicate, v0x3030), vp);

                uint32_t repeatMask = extract_sign_bits_i8(vf);
                Vec16xU8 quadSplines = (alignr_u128<15>(vf, vPrevFlags) + vf) & v0x8080;
                VecPair<Vec16xU8> vertexSizes = aggregateVertexSizes(vf, vSizesPerXYPredicate, v0x0F0F);

                // Lucky if there are no repeats in 16 flags.
                if (repeatMask == 0) {
                  offCurveSplineCount += unsigned(_mm_popcnt_u32(extract_sign_bits_i8(quadSplines)));
                  vPrevFlags = vf;
                  vf |= srli_u16<3>(quadSplines);

                  storeu(fDataPtr + i, vf);
                  storeu(xPredPtr + i, vertexSizes[0]);
                  storeu(yPredPtr + i, vertexSizes[1]);

                  i += n;
                  gPtr += n;
                  vSumXY += sumsFromAggregatedSizesOf16Bytes(vertexSizes);
                  continue;
                }

                // Still a bit lucky if there are no repeats in the first 8 flags.
                if ((repeatMask & 0xFFu) == 0) {
                  // NOTE: Must be greater than 8 as all flags that overflow the flag count are non repeating.
                  BL_ASSERT(n >= 8);

                  offCurveSplineCount += unsigned(_mm_popcnt_u32(extract_sign_bits_i8(quadSplines)) & 0xFFu);
                  vPrevFlags = sllb_u128<8>(vf);
                  vf |= srli_u16<3>(quadSplines);

                  storeu_64(fDataPtr + i, vf);
                  storeu_64(xPredPtr + i, vertexSizes[0]);
                  storeu_64(yPredPtr + i, vertexSizes[1]);

                  i += 8;
                  gPtr += 8;
                  vSumXY += sumsFromAggregatedSizesOf8Bytes(vertexSizes);
                }
              }

              // Slow loop, processes repeating flags in 8-flag chunks. The first chunk that is non-repeating goes back
              // to the fast loop. This loop can be slow as it's not common to have many repeating flags. Some glyphs
              // have no repeating flags at all, and some have less than 2. It's very unlikely to hit this loop often.
SlowFlagsDecode:
              {
                size_t slowIndex = i;

                // First expand all repeated flags to fDataPtr[] array - X/Y data will be calculated once we have flags expanded.
                do {
                  if (BL_UNLIKELY(gPtr == gEnd))
                    goto InvalidData;

                  // Repeated flag?
                  uint32_t f = convertFlagsPredicate[*gPtr++ & Simple::kImportantFlagsMask];

                  if (f & kVecFlagRepeat) {
                    if (BL_UNLIKELY(gPtr == gEnd))
                      goto InvalidData;

                    size_t n = *gPtr++;
                    f ^= kVecFlagRepeat;

                    if (BL_UNLIKELY(n >= ttVertexCount - i))
                      goto InvalidData;

                    BLMemOps::fillSmall(fDataPtr + i, uint8_t(f), n);
                    i += n;
                  }

                  fDataPtr[i++] = uint8_t(f);
                } while ((i & 0x7u) != slowFlagsDecodeFinishedCheck && i != ttVertexCount);

                // We want to process 16 flags at a time in the next loop, however, we cannot have garbage in fDataPtr[]
                // as each byte contributes to vertex sizes we calculate out of flags. So explicitly zero the next 16
                // bytes to make sure there is no garbage.
                storeu(fDataPtr + i, make_zero<Vec16xU8>());

                // Calculate vertex sizes and off-curve spline bits of all expanded flags.
                do {
                  Vec16xU8 vf = loadu<Vec16xU8>(fDataPtr + slowIndex);
                  Vec16xU8 quadSplines = (alignr_u128<15>(vf, vPrevFlags) + vf) & v0x8080;
                  offCurveSplineCount += unsigned(_mm_popcnt_u32(extract_sign_bits_i8(quadSplines)));

                  vPrevFlags = vf;
                  vf |= srli_u16<3>(quadSplines);

                  VecPair<Vec16xU8> vertexSizes = aggregateVertexSizes(vf, vSizesPerXYPredicate, v0x0F0F);
                  storeu(fDataPtr + slowIndex, vf);
                  storeu(xPredPtr + slowIndex, vertexSizes[0]);
                  storeu(yPredPtr + slowIndex, vertexSizes[1]);

                  slowIndex += 16;
                  vSumXY += sumsFromAggregatedSizesOf16Bytes(vertexSizes);
                } while (slowIndex < i);

                // Processed more flags than necessary? Correct vPrevFlags to make off-curve calculations correct.
                if (slowIndex > i)
                  vPrevFlags = sllb_u128<8>(vPrevFlags);
              }
            } while (i < ttVertexCount);

            // Finally, calculate the size of xCoordinates[] and yCoordinates[] arrays.
            vSumXY += srli_u64<32>(vSumXY);
            xCoordinatesSize = extract_u16<0>(vSumXY);
            yCoordinatesSize = extract_u16<4>(vSumXY);
          }

          remainingSize = BLIntOps::subOverflow((size_t)(gEnd - gPtr), xCoordinatesSize + yCoordinatesSize, &of);
          if (BL_UNLIKELY(of))
            goto InvalidData;

          // Read TrueType Vertex Data
          // -------------------------

          // Vertex data in `glyf` table doesn't map 1:1 to how BLPath stores its data. Multiple off-point curves in
          // TrueType data are decomposed into a quad spline, which is one vertex larger (BLPath doesn't offer multiple
          // off-point quads). This means that the number of vertices required by BLPath can be greater than the number
          // of vertices stored in TrueType 'glyf' data. However, we should know exactly how many vertices we have to
          // add to `ttVertexCount` as we calculated `offCurveSplineCount` during flags decoding.
          //
          // The number of resulting vertices is thus:
          //   - `ttVertexCount` - base number of vertices stored in TrueType data.
          //   - `offCurveSplineCount` - the number of additional vertices we will need to add for each off-curve spline
          //     used in TrueType data.
          //   - `contourCount` - Number of contours, we multiply this by 3 as we want to include one 'MoveTo', 'Close',
          //     and one additional off-curve spline point per each contour in case it starts - ends with an off-curve
          //     point.
          //   - 16 extra vertices for SIMD stores and to prevent `decodedVertexArray` overlapping BLPath data.
          size_t maxVertexCount = ttVertexCount + offCurveSplineCount + contourCount * 3 + 16;

          // Increase maxVertexCount if the path was not allocated yet - this avoids a possible realloc of compound glyphs.
          if (out->capacity() == 0 && compoundLevel > 0)
            maxVertexCount += 128;

          BL_PROPAGATE(appender.beginAppend(out, maxVertexCount));

          // Temporary data where 16-bit coordinates (per X and Y) are stored before they are converted to double precision.
          DecodedVertex* decodedVertexArray = BLIntOps::alignUp(
            reinterpret_cast<DecodedVertex*>(appender.vtx + maxVertexCount) - BLIntOps::alignUp(ttVertexCount, 16) - 4, 16);

          {
            // Since we know exactly how many bytes both vertex arrays consume we can decode both X and Y coordinates at
            // the same time. This gives us also the opportunity to start appending to BLPath immediately.
            const uint8_t* yPtr = gPtr + xCoordinatesSize;

            // LO+HI predicate is added to interleaved predicates.
            Vec16xU8 vLoHiPredInc = make128_u16<Vec16xU8>(uint16_t(0x0041u));

            // These are predicates we need to combine with xPred and yPred to get the final predicate for VPSHUFB.
            Vec16xU8 vDecodeOpXImm = loada<Vec16xU8>(decodeOpXTable);
            Vec16xU8 vDecodeOpYImm = loada<Vec16xU8>(decodeOpYTable);

            // NOTE: It's super unlikely that there won't be 16 bytes available after the end of x/y coordinates. Basically
            // only last glyph could be affected. However, we still need to check whether the bytes are there as we cannot just
            // read outside of the glyph table.
            if (BL_LIKELY(remainingSizeAfterGlyphData >= 16)) {
              // Common case - uses at most 16-byte reads ahead, processes 16 vertices at a time.
#ifdef BL_TARGET_OPT_AVX2
              Vec32xU8 vLoHiPredInc256 = broadcast_i128<Vec32xU8>(vLoHiPredInc);
              size_t i = 0;

              // Process 32 vertices at a time.
              if (ttVertexCount > 16) {
                Vec32xU8 vDecodeOpXImm256 = broadcast_i128<Vec32xU8>(vDecodeOpXImm);
                Vec32xU8 vDecodeOpYImm256 = broadcast_i128<Vec32xU8>(vDecodeOpYImm);

                do {
                  Vec16xU8 xVerticesInitial0 = loadu<Vec16xU8>(gPtr);
                  Vec16xU8 yVerticesInitial0 = loadu<Vec16xU8>(yPtr);

                  gPtr += xPredPtr[i + 7];
                  yPtr += yPredPtr[i + 7];

                  Vec32xU8 fData = loada<Vec32xU8>(fDataPtr + i);
                  Vec32xU8 xPred = slli_i64<8>(loada<Vec32xU8>(xPredPtr + i));
                  Vec32xU8 yPred = slli_i64<8>(loada<Vec32xU8>(yPredPtr + i));

                  xPred += swizzlev_u8(vDecodeOpXImm256, fData);
                  yPred += swizzlev_u8(vDecodeOpYImm256, fData);

                  Vec16xU8 xVerticesInitial1 = loadu<Vec16xU8>(gPtr);
                  Vec16xU8 yVerticesInitial1 = loadu<Vec16xU8>(yPtr);

                  gPtr += xPredPtr[i + 15];
                  yPtr += yPredPtr[i + 15];

                  Vec32xU8 xPred0 = interleave_lo_u8(xPred, xPred);
                  Vec32xU8 xPred1 = interleave_hi_u8(xPred, xPred);
                  Vec32xU8 yPred0 = interleave_lo_u8(yPred, yPred);
                  Vec32xU8 yPred1 = interleave_hi_u8(yPred, yPred);

                  Vec16xI16 xVertices0 = make256_128<Vec16xI16>(loadu<Vec16xU8>(gPtr), xVerticesInitial0);
                  Vec16xI16 yVertices0 = make256_128<Vec16xI16>(loadu<Vec16xU8>(yPtr), yVerticesInitial0);

                  gPtr += xPredPtr[i + 23];
                  yPtr += yPredPtr[i + 23];

                  xPred0 += vLoHiPredInc256;
                  xPred1 += vLoHiPredInc256;
                  yPred0 += vLoHiPredInc256;
                  yPred1 += vLoHiPredInc256;

                  Vec16xI16 xVertices1 = make256_128<Vec16xI16>(loadu<Vec16xU8>(gPtr), xVerticesInitial1);
                  Vec16xI16 yVertices1 = make256_128<Vec16xI16>(loadu<Vec16xU8>(yPtr), yVerticesInitial1);

                  gPtr += xPredPtr[i + 31];
                  yPtr += yPredPtr[i + 31];

                  xVertices0 = swizzlev_u8(xVertices0, xPred0);
                  yVertices0 = swizzlev_u8(yVertices0, yPred0);
                  xVertices1 = swizzlev_u8(xVertices1, xPred1);
                  yVertices1 = swizzlev_u8(yVertices1, yPred1);

                  xPred0 = srai_i16<15>(slli_i16<2>(xPred0));
                  yPred0 = srai_i16<15>(slli_i16<2>(yPred0));
                  xPred1 = srai_i16<15>(slli_i16<2>(xPred1));
                  yPred1 = srai_i16<15>(slli_i16<2>(yPred1));

                  xVertices0 = (xVertices0 ^ vec_i16(xPred0)) - vec_i16(xPred0);
                  yVertices0 = (yVertices0 ^ vec_i16(yPred0)) - vec_i16(yPred0);
                  xVertices1 = (xVertices1 ^ vec_i16(xPred1)) - vec_i16(xPred1);
                  yVertices1 = (yVertices1 ^ vec_i16(yPred1)) - vec_i16(yPred1);

                  Vec16xI16 xyInterleavedLo0 = interleave_lo_u16(xVertices0, yVertices0);
                  Vec16xI16 xyInterleavedHi0 = interleave_hi_u16(xVertices0, yVertices0);
                  Vec16xI16 xyInterleavedLo1 = interleave_lo_u16(xVertices1, yVertices1);
                  Vec16xI16 xyInterleavedHi1 = interleave_hi_u16(xVertices1, yVertices1);

                  storea_128(decodedVertexArray + i +  0, xyInterleavedLo0);
                  storea_128(decodedVertexArray + i +  4, xyInterleavedHi0);
                  storea_128(decodedVertexArray + i +  8, xyInterleavedLo1);
                  storea_128(decodedVertexArray + i + 12, xyInterleavedHi1);
                  storea_128(decodedVertexArray + i + 16, extract_i128<1>(xyInterleavedLo0));
                  storea_128(decodedVertexArray + i + 20, extract_i128<1>(xyInterleavedHi0));
                  storea_128(decodedVertexArray + i + 24, extract_i128<1>(xyInterleavedLo1));
                  storea_128(decodedVertexArray + i + 28, extract_i128<1>(xyInterleavedHi1));

                  i += 32;
                } while (i < ttVertexCount - 16);
              }

              // Process remaining 16 vertices.
              if (i < ttVertexCount) {
                Vec16xU8 fData = loada<Vec16xU8>(fDataPtr + i);
                Vec16xU8 xPred = slli_i64<8>(loada<Vec16xU8>(xPredPtr + i));
                Vec16xU8 yPred = slli_i64<8>(loada<Vec16xU8>(yPredPtr + i));

                xPred += swizzlev_u8(vDecodeOpXImm, fData);
                yPred += swizzlev_u8(vDecodeOpYImm, fData);

                Vec32xU8 xPred256 = permute_i64<1, 1, 0, 0>(vec_cast<Vec32xU8>(xPred));
                Vec32xU8 yPred256 = permute_i64<1, 1, 0, 0>(vec_cast<Vec32xU8>(yPred));

                xPred256 = interleave_lo_u8(xPred256, xPred256);
                yPred256 = interleave_lo_u8(yPred256, yPred256);

                Vec16xU8 xVerticesInitial = loadu<Vec16xU8>(gPtr);
                Vec16xU8 yVerticesInitial = loadu<Vec16xU8>(yPtr);

                gPtr += xPredPtr[i + 7];
                yPtr += yPredPtr[i + 7];

                xPred256 += vLoHiPredInc256;
                yPred256 += vLoHiPredInc256;

                Vec16xI16 xVertices = make256_128<Vec16xI16>(loadu<Vec16xU8>(gPtr), xVerticesInitial);
                Vec16xI16 yVertices = make256_128<Vec16xI16>(loadu<Vec16xU8>(yPtr), yVerticesInitial);

                gPtr += xPredPtr[i + 15];
                yPtr += yPredPtr[i + 15];

                xVertices = swizzlev_u8(xVertices, xPred256);
                yVertices = swizzlev_u8(yVertices, yPred256);

                xPred256 = srai_i16<15>(slli_i16<2>(xPred256));
                yPred256 = srai_i16<15>(slli_i16<2>(yPred256));

                xVertices = (xVertices ^ vec_i16(xPred256)) - vec_i16(xPred256);
                yVertices = (yVertices ^ vec_i16(yPred256)) - vec_i16(yPred256);

                Vec16xI16 xyInterleavedLo = interleave_lo_u16(xVertices, yVertices);
                Vec16xI16 xyInterleavedHi = interleave_hi_u16(xVertices, yVertices);

                storea_128(decodedVertexArray + i +  0, xyInterleavedLo);
                storea_128(decodedVertexArray + i +  4, xyInterleavedHi);
                storea_128(decodedVertexArray + i +  8, extract_i128<1>(xyInterleavedLo));
                storea_128(decodedVertexArray + i + 12, extract_i128<1>(xyInterleavedHi));
              }
#else
              for (size_t i = 0; i < ttVertexCount; i += 16) {
                Vec16xU8 fData = loada<Vec16xU8>(fDataPtr + i);
                Vec16xU8 xPred = slli_i64<8>(loada<Vec16xU8>(xPredPtr + i));
                Vec16xU8 yPred = slli_i64<8>(loada<Vec16xU8>(yPredPtr + i));

                xPred += swizzlev_u8(vDecodeOpXImm, fData);
                yPred += swizzlev_u8(vDecodeOpYImm, fData);

                Vec16xU8 xPred0 = interleave_lo_u8(xPred, xPred);
                Vec16xU8 xPred1 = interleave_hi_u8(xPred, xPred);
                Vec16xU8 yPred0 = interleave_lo_u8(yPred, yPred);
                Vec16xU8 yPred1 = interleave_hi_u8(yPred, yPred);

                xPred0 += vLoHiPredInc;
                xPred1 += vLoHiPredInc;
                yPred0 += vLoHiPredInc;
                yPred1 += vLoHiPredInc;

                // Process low 8 vertices.
                Vec8xI16 xVertices0 = vec_i16(swizzlev_u8(loadu<Vec16xU8>(gPtr), xPred0));
                Vec8xI16 yVertices0 = vec_i16(swizzlev_u8(loadu<Vec16xU8>(yPtr), yPred0));

                gPtr += xPredPtr[i + 7];
                yPtr += yPredPtr[i + 7];

                xPred0 = srai_i16<15>(slli_i16<2>(xPred0));
                yPred0 = srai_i16<15>(slli_i16<2>(yPred0));

                xVertices0 = (xVertices0 ^ vec_i16(xPred0)) - vec_i16(xPred0);
                yVertices0 = (yVertices0 ^ vec_i16(yPred0)) - vec_i16(yPred0);

                storea(decodedVertexArray + i + 0, interleave_lo_u16(xVertices0, yVertices0));
                storea(decodedVertexArray + i + 4, interleave_hi_u16(xVertices0, yVertices0));

                // Process high 8 vertices.
                Vec8xI16 xVertices1 = vec_i16(swizzlev_u8(loadu<Vec16xU8>(gPtr), xPred1));
                Vec8xI16 yVertices1 = vec_i16(swizzlev_u8(loadu<Vec16xU8>(yPtr), yPred1));

                gPtr += xPredPtr[i + 15];
                yPtr += yPredPtr[i + 15];

                xPred1 = srai_i16<15>(slli_i16<2>(xPred1));
                yPred1 = srai_i16<15>(slli_i16<2>(yPred1));

                xVertices1 = (xVertices1 ^ vec_i16(xPred1)) - vec_i16(xPred1);
                yVertices1 = (yVertices1 ^ vec_i16(yPred1)) - vec_i16(yPred1);

                storea(decodedVertexArray + i +  8, interleave_lo_u16(xVertices1, yVertices1));
                storea(decodedVertexArray + i + 12, interleave_hi_u16(xVertices1, yVertices1));
              }
#endif
            }
            else {
              // Restricted case - uses at most 16-byte reads below, we know that there 16 bytes below, because:
              //   - Glyph header       [10 bytes]
              //   - NumberOfContours   [ 2 bytes]
              //   - InstructionLength  [ 2 bytes]
              //   - At least two flags [ 2 bytes] (one flag glyphs are refused as is not enough for a contour)
              for (size_t i = 0; i < ttVertexCount; i += 8) {
                Vec16xU8 fData = loadu_64<Vec16xU8>(fDataPtr + i);
                Vec16xU8 xPred = slli_i64<8>(loadu_64<Vec16xU8>(xPredPtr + i));
                Vec16xU8 yPred = slli_i64<8>(loadu_64<Vec16xU8>(yPredPtr + i));

                size_t xBytesUsed = xPredPtr[i + 7];
                size_t yBytesUsed = yPredPtr[i + 7];

                gPtr += xBytesUsed;
                yPtr += yBytesUsed;

                xPred += swizzlev_u8(vDecodeOpXImm, fData);
                yPred += swizzlev_u8(vDecodeOpYImm, fData);

                xPred += make128_u8<Vec16xU8>(uint8_t(16u - uint32_t(xBytesUsed)));
                yPred += make128_u8<Vec16xU8>(uint8_t(16u - uint32_t(yBytesUsed)));

                xPred = interleave_lo_u8(xPred, xPred);
                yPred = interleave_lo_u8(yPred, yPred);

                xPred += vLoHiPredInc;
                yPred += vLoHiPredInc;

                Vec8xI16 xVertices0 = vec_i16(swizzlev_u8(loadu<Vec16xU8>(gPtr - 16), xPred));
                Vec8xI16 yVertices0 = vec_i16(swizzlev_u8(loadu<Vec16xU8>(yPtr - 16), yPred));

                xPred = srai_i16<15>(slli_i16<2>(xPred));
                yPred = srai_i16<15>(slli_i16<2>(yPred));

                xVertices0 = (xVertices0 ^ vec_i16(xPred)) - vec_i16(xPred);
                yVertices0 = (yVertices0 ^ vec_i16(yPred)) - vec_i16(yPred);

                storea(decodedVertexArray + i + 0, interleave_lo_u16(xVertices0, yVertices0));
                storea(decodedVertexArray + i + 4, interleave_hi_u16(xVertices0, yVertices0));
              }
            }
          }

          // Affine transform applied to each vertex.
          //
          // NOTE: Compilers are not able to vectorize the computations efficiently, so we do it instead.
          Vec2xF64 m00_m11 = make128_f64(compoundData[compoundLevel].transform.m11, compoundData[compoundLevel].transform.m00);
          Vec2xF64 m10_m01 = make128_f64(compoundData[compoundLevel].transform.m01, compoundData[compoundLevel].transform.m10);

          // Vertices are stored relative to each other, this is the current point.
          Vec2xF64 currentPt = make128_f64(compoundData[compoundLevel].transform.m21, compoundData[compoundLevel].transform.m20);

          // SIMD constants.
          Vec2xF64 half = make128_f64(0.5);

          // Current vertex index in TT sense, advanced until `ttVertexCount`, which must be end index of the last contour.
          size_t i = 0;

          for (size_t contourIndex = 0; contourIndex < contourCount; contourIndex++) {
            size_t iEnd = size_t(contourArray[contourIndex].value()) + 1;
            if (BL_UNLIKELY(iEnd <= i || iEnd > ttVertexCount))
              goto InvalidData;

            // We do the first vertex here as we want to emit 'MoveTo' and we want to remember it for a possible off-curve
            // start. Currently this means there is some code duplicated for move-to and for other commands, unfortunately.
            uint32_t f = fDataPtr[i];
            currentPt += transformDecodedVertex(decodedVertexArray + i, m00_m11, m10_m01);

            if (++i >= iEnd)
              continue;

            // Initial 'MoveTo' coordinates.
            Vec2xF64 initialPt = currentPt;

            // We need to be able to handle a case in which the contour data starts off-curve.
            size_t startsOnCurve = size_t((f >> kVecFlagOnCurveShift) & 0x1u);
            size_t initialVertexIndex = appender.currentIndex(*out);

            // Only emit MoveTo here if we don't start off curve, which requres a special care.
            storeVertex(appender, BL_PATH_CMD_MOVE, initialPt);
            appender._advance(startsOnCurve);

            size_t iEndMinus3 = BLIntOps::usubSaturate<size_t>(iEnd, 3);

            static constexpr uint32_t kPathCmdFromFlagsShift0 = kVecFlagOnCurveShift;
            static constexpr uint32_t kPathCmdFromFlagsShift1 = kVecFlagOnCurveShift + 8;
            static constexpr uint32_t kPathCmdFromFlagsShift2 = kVecFlagOnCurveShift + 8 + 8;
            static constexpr uint32_t kPathCmdFromFlagsShift3 = kVecFlagOnCurveShift + 8 + 8 + 8;

            static constexpr uint32_t kVecFlagOffSpline0 = uint32_t(kVecFlagOffSpline) << 0;
            static constexpr uint32_t kVecFlagOffSpline1 = uint32_t(kVecFlagOffSpline) << 8;
            static constexpr uint32_t kVecFlagOffSpline2 = uint32_t(kVecFlagOffSpline) << 16;
            static constexpr uint32_t kVecFlagOffSpline3 = uint32_t(kVecFlagOffSpline) << 24;

            // NOTE: This is actually the slowest loop. The 'OffSpline' flag is not easily predictable as it heavily
            // depends on a font face. It's not a rare flag though. If a glyph contains curves there is a high chance
            // that there will be multiple off-curve splines and it's not uncommon to have multiple off-curve splines
            // having more than 3 consecutive off points.
            while (i < iEndMinus3) {
              f = BLMemOps::readU32u(fDataPtr + i);

              Vec2xF64 d0 = transformDecodedVertex(decodedVertexArray + i + 0, m00_m11, m10_m01);
              Vec2xF64 d1 = transformDecodedVertex(decodedVertexArray + i + 1, m00_m11, m10_m01);
              Vec2xF64 d2 = transformDecodedVertex(decodedVertexArray + i + 2, m00_m11, m10_m01);
              Vec2xF64 d3 = transformDecodedVertex(decodedVertexArray + i + 3, m00_m11, m10_m01);
              Vec2xF64 onPt;

              i += 4;
              currentPt += d0;

              uint32_t pathCmds = (f >> kPathCmdFromFlagsShift0) & 0x03030303u;
              BLMemOps::writeU32u(appender.cmd, pathCmds);

              if (f & kVecFlagOffSpline0)
                goto EmitSpline0Advance;

              storeu(appender.vtx + 0, currentPt);
              currentPt += d1;
              if (f & kVecFlagOffSpline1)
                goto EmitSpline1Advance;

              storeu(appender.vtx + 1, currentPt);
              currentPt += d2;
              if (f & kVecFlagOffSpline2)
                goto EmitSpline2Advance;

              storeu(appender.vtx + 2, currentPt);
              currentPt += d3;
              if (f & kVecFlagOffSpline3)
                goto EmitSpline3Advance;

              storeu(appender.vtx + 3, currentPt);
              appender._advance(4);
              continue;

EmitSpline0Advance:
              onPt = currentPt - d0 * half;
              appendVertex2x(appender, BL_PATH_CMD_ON, onPt, BL_PATH_CMD_QUAD, currentPt);
              currentPt += d1;
              if (f & kVecFlagOffSpline1)
                goto EmitSpline1Continue;

              appendVertex(appender, uint8_t((f >> kPathCmdFromFlagsShift1) & 0x3u), currentPt);
              currentPt += d2;
              if (f & kVecFlagOffSpline2)
                goto EmitSpline2Continue;

              appendVertex(appender, uint8_t((f >> kPathCmdFromFlagsShift2) & 0x3u), currentPt);
              currentPt += d3;
              if (f & kVecFlagOffSpline3)
                goto EmitSpline3Continue;

              appendVertex(appender, uint8_t((f >> kPathCmdFromFlagsShift3) & 0x3u), currentPt);
              continue;

EmitSpline1Advance:
              appender._advance(1);

EmitSpline1Continue:
              onPt = currentPt - d1 * half;
              appendVertex2x(appender, BL_PATH_CMD_ON, onPt, BL_PATH_CMD_QUAD, currentPt);
              currentPt += d2;

              if (f & kVecFlagOffSpline2)
                goto EmitSpline2Continue;

              appendVertex(appender, uint8_t((f >> kPathCmdFromFlagsShift2) & 0x3u), currentPt);
              currentPt += d3;

              if (f & kVecFlagOffSpline3)
                goto EmitSpline3Continue;

              appendVertex(appender, uint8_t((f >> kPathCmdFromFlagsShift3) & 0x3u), currentPt);
              continue;

EmitSpline2Advance:
              appender._advance(2);

EmitSpline2Continue:
              onPt = currentPt - d2 * half;
              appendVertex2x(appender, BL_PATH_CMD_ON, onPt, BL_PATH_CMD_QUAD, currentPt);
              currentPt += d3;

              if (f & kVecFlagOffSpline3)
                goto EmitSpline3Continue;

              appendVertex(appender, uint8_t((f >> kPathCmdFromFlagsShift3) & 0x3u), currentPt);
              continue;

EmitSpline3Advance:
              appender._advance(3);

EmitSpline3Continue:
              onPt = currentPt - d3 * half;
              appendVertex2x(appender, BL_PATH_CMD_ON, onPt, BL_PATH_CMD_QUAD, currentPt);
            }

            while (i < iEnd) {
              f = fDataPtr[i];
              Vec2xF64 delta = transformDecodedVertex(decodedVertexArray + i, m00_m11, m10_m01);
              currentPt += delta;
              i++;

              if ((f & kVecFlagOffSpline) == 0) {
                appendVertex(appender, uint8_t(f >> 5), currentPt);
              }
              else {
                Vec2xF64 onPt = currentPt - delta * half;
                appendVertex2x(appender, BL_PATH_CMD_ON, onPt, BL_PATH_CMD_QUAD, currentPt);
              }
            }

            f = fDataPtr[i - 1];
            if (!startsOnCurve) {
              BLPathImpl* outI = BLPathPrivate::getImpl(out);
              Vec2xF64 finalPt = loadu<Vec2xF64>(outI->vertexData + initialVertexIndex);

              outI->commandData[initialVertexIndex] = BL_PATH_CMD_MOVE;

              if (f & kVecFlagOffCurve) {
                Vec2xF64 onPt = (currentPt + initialPt) * half;
                appendVertex(appender, BL_PATH_CMD_ON, onPt);
                finalPt = (initialPt + finalPt) * half;
              }

              appendVertex2x(appender, BL_PATH_CMD_QUAD, initialPt, BL_PATH_CMD_ON, finalPt);
            }
            else if (f & kVecFlagOffCurve) {
              appendVertex(appender, BL_PATH_CMD_ON, initialPt);
            }

            appender.close();
          }
          appender.done(out);
        }
      }
      else if (contourCountSigned == -1) {
        gPtr += sizeof(GlyfTable::GlyphData);
        remainingSize -= sizeof(GlyfTable::GlyphData);

        if (BL_UNLIKELY(++compoundLevel >= CompoundEntry::kMaxLevel))
          goto InvalidData;

        goto ContinueCompound;
      }
      else {
        // Cannot be less than -1, only -1 specifies compound glyph, lesser value is invalid according to the
        // specification.
        if (BL_UNLIKELY(contourCountSigned < -1))
          goto InvalidData;

        // Otherwise the glyph has no contours.
      }
    }

    // Compound Glyph
    // --------------

    if (compoundLevel) {
      while (!(compoundData[compoundLevel].compoundFlags & Compound::kMoreComponents))
        if (--compoundLevel == 0)
          break;

      if (compoundLevel) {
        gPtr = compoundData[compoundLevel].gPtr;
        remainingSize = compoundData[compoundLevel].remainingSize;

        // The structure that we are going to read is as follows:
        //
        //   [Header]
        //     uint16_t flags;
        //     uint16_t glyphId;
        //
        //   [Translation]
        //     a) int8_t arg1/arg2;
        //     b) int16_t arg1/arg2;
        //
        //   [Scale/Affine]
        //     a) <None>
        //     b) int16_t scale;
        //     c) int16_t scaleX, scaleY;
        //     d) int16_t m00, m01, m10, m11;

ContinueCompound:
        {
          uint32_t flags;
          int arg1, arg2;
          BLOverflowFlag of = 0;

          remainingSize = BLIntOps::subOverflow<size_t>(remainingSize, 6, &of);
          if (BL_UNLIKELY(of))
            goto InvalidData;

          flags = BLMemOps::readU16uBE(gPtr);
          glyphId = BLMemOps::readU16uBE(gPtr + 2);
          if (BL_UNLIKELY(glyphId >= faceI->faceInfo.glyphCount))
            goto InvalidData;

          arg1 = BLMemOps::readI8(gPtr + 4);
          arg2 = BLMemOps::readI8(gPtr + 5);
          gPtr += 6;

          if (flags & Compound::kArgsAreWords) {
            remainingSize = BLIntOps::subOverflow<size_t>(remainingSize, 2, &of);
            if (BL_UNLIKELY(of))
              goto InvalidData;

            arg1 = BLIntOps::shl(arg1, 8) | (arg2 & 0xFF);
            arg2 = BLMemOps::readI16uBE(gPtr);
            gPtr += 2;
          }

          if (!(flags & Compound::kArgsAreXYValues)) {
            // This makes them unsigned.
            arg1 &= 0xFFFFu;
            arg2 &= 0xFFFFu;

            // TODO: [OPENTYPE GLYF] ArgsAreXYValues not implemented. I don't know how atm.
          }

          constexpr double kScaleF2x14 = 1.0 / 16384.0;

          BLMatrix2D& cm = compoundData[compoundLevel].transform;
          cm.reset(1.0, 0.0, 0.0, 1.0, double(arg1), double(arg2));

          if (flags & Compound::kAnyCompoundScale) {
            if (flags & Compound::kWeHaveScale) {
              // Simple scaling:
              //   [Sc, 0]
              //   [0, Sc]
              remainingSize = BLIntOps::subOverflow<size_t>(remainingSize, 2, &of);
              if (BL_UNLIKELY(of))
                goto InvalidData;

              double scale = double(BLMemOps::readI16uBE(gPtr)) * kScaleF2x14;
              cm.m00 = scale;
              cm.m11 = scale;
              gPtr += 2;
            }
            else if (flags & Compound::kWeHaveScaleXY) {
              // Simple scaling:
              //   [Sx, 0]
              //   [0, Sy]
              remainingSize = BLIntOps::subOverflow<size_t>(remainingSize, 4, &of);
              if (BL_UNLIKELY(of))
                goto InvalidData;

              cm.m00 = double(BLMemOps::readI16uBE(gPtr + 0)) * kScaleF2x14;
              cm.m11 = double(BLMemOps::readI16uBE(gPtr + 2)) * kScaleF2x14;
              gPtr += 4;
            }
            else {
              // Affine case:
              //   [A, B]
              //   [C, D]
              remainingSize = BLIntOps::subOverflow<size_t>(remainingSize, 8, &of);
              if (BL_UNLIKELY(of))
                goto InvalidData;

              cm.m00 = double(BLMemOps::readI16uBE(gPtr + 0)) * kScaleF2x14;
              cm.m01 = double(BLMemOps::readI16uBE(gPtr + 2)) * kScaleF2x14;
              cm.m10 = double(BLMemOps::readI16uBE(gPtr + 4)) * kScaleF2x14;
              cm.m11 = double(BLMemOps::readI16uBE(gPtr + 6)) * kScaleF2x14;
              gPtr += 8;
            }

            // Translation scale should only happen when `kArgsAreXYValues` is set. The default behavior according to
            // the specification is `kUnscaledComponentOffset`, which can be overridden by `kScaledComponentOffset`.
            // However, if both or neither are set then the behavior is the same as `kUnscaledComponentOffset`.
            if ((flags & (Compound::kArgsAreXYValues | Compound::kAnyCompoundOffset    )) ==
                         (Compound::kArgsAreXYValues | Compound::kScaledComponentOffset)) {
              // This is what FreeType does and what's not 100% according to the specification. However, according to
              // FreeType this would produce much better offsets so we will match FreeType instead of following the
              // specification.
              cm.m20 *= BLGeometry::length(BLPoint(cm.m00, cm.m01));
              cm.m21 *= BLGeometry::length(BLPoint(cm.m10, cm.m11));
            }
          }

          compoundData[compoundLevel].gPtr = gPtr;
          compoundData[compoundLevel].remainingSize = remainingSize;
          compoundData[compoundLevel].compoundFlags = flags;
          BLTransformPrivate::multiply(cm, cm, compoundData[compoundLevel - 1].transform);
          continue;
        }
      }
    }

    break;
  }

  *contourCountOut = contourCountTotal;
  return BL_SUCCESS;

InvalidData:
  *contourCountOut = 0;
  return blTraceError(BL_ERROR_INVALID_DATA);
}

#undef getGlyphOutlines_SIMD

} // {GlyfImpl}
} // {BLOpenType}

#endif
