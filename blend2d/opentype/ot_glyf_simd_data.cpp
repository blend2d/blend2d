// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#if defined(BL_BUILD_OPT_SSE4_2) || defined(BL_BUILD_OPT_ASIMD)

#include <blend2d/opentype/ot_glyf_simd_data_p.h>

namespace bl::OpenType::GlyfImpl {

alignas(16) const uint8_t convert_flags_predicate[16] = {
  uint8_t(kVFEmitMidpoint | kVFEmitCommand | kVFOffCurve | 0xCu), // [?|?|?|?|     0|    0|    0| 0] -> [EmitMid|EmitCmd|Off| 0][1|1|    0|    0]
  uint8_t(             0u |             0u | kVFOnCurve  | 0xCu), // [?|?|?|?|     0|    0|    0|On] -> [      0|      0|  0|On][1|1|    0|    0]
  uint8_t(kVFEmitMidpoint | kVFEmitCommand | kVFOffCurve | 0xDu), // [?|?|?|?|     0|    0|XByte| 0] -> [EmitMid|EmitCmd|Off| 0][1|1|    0|XByte]
  uint8_t(             0u |             0u | kVFOnCurve  | 0xDu), // [?|?|?|?|     0|    0|XByte|On] -> [      0|      0|  0|On][1|1|    0|XByte]
  uint8_t(kVFEmitMidpoint | kVFEmitCommand | kVFOffCurve | 0xEu), // [?|?|?|?|     0|YByte|    0| 0] -> [EmitMid|EmitCmd|Off| 0][1|1|YByte|    0]
  uint8_t(             0u |             0u | kVFOnCurve  | 0xEu), // [?|?|?|?|     0|YByte|    0|On] -> [      0|      0|  0|On][1|1|YByte|    0]
  uint8_t(kVFEmitMidpoint | kVFEmitCommand | kVFOffCurve | 0xFu), // [?|?|?|?|     0|YByte|XByte| 0] -> [EmitMid|EmitCmd|Off| 0][1|1|YByte|XByte]
  uint8_t(             0u |             0u | kVFOnCurve  | 0xFu), // [?|?|?|?|     0|YByte|XByte|On] -> [      0|      0|  0|On][1|1|YByte|XByte]
  uint8_t(             0u | kVFEmitCommand | kVFOffCurve | 0xCu), // [?|?|?|?|PrevOn|    0|    0| 0] -> [      0|EmitCmd|Off| 0][1|1|    0|    0]
  uint8_t(             0u | kVFEmitCommand | kVFOnCurve  | 0xCu), // [?|?|?|?|PrevOn|    0|    0|On] -> [      0|EmitCmd|  0|On][1|1|    0|    0]
  uint8_t(             0u | kVFEmitCommand | kVFOffCurve | 0xDu), // [?|?|?|?|PrevOn|    0|XByte| 0] -> [      0|EmitCmd|Off| 0][1|1|    0|XByte]
  uint8_t(             0u | kVFEmitCommand | kVFOnCurve  | 0xDu), // [?|?|?|?|PrevOn|    0|XByte|On] -> [      0|EmitCmd|  0|On][1|1|    0|XByte]
  uint8_t(             0u | kVFEmitCommand | kVFOffCurve | 0xEu), // [?|?|?|?|PrevOn|YByte|    0| 0] -> [      0|EmitCmd|Off| 0][1|1|YByte|    0]
  uint8_t(             0u | kVFEmitCommand | kVFOnCurve  | 0xEu), // [?|?|?|?|PrevOn|YByte|    0|On] -> [      0|EmitCmd|  0|On][1|1|YByte|    0]
  uint8_t(             0u | kVFEmitCommand | kVFOffCurve | 0xFu), // [?|?|?|?|PrevOn|YByte|XByte| 0] -> [      0|EmitCmd|Off| 0][1|1|YByte|XByte]
  uint8_t(             0u | kVFEmitCommand | kVFOnCurve  | 0xFu)  // [?|?|?|?|PrevOn|YByte|XByte|On] -> [      0|EmitCmd|  0|On][1|1|YByte|XByte]
};

alignas(16) const uint8_t sizes_per_xy_predicate[16] = {
  0x00, // [0|0|0|0| y_same| x_same|     0|     0] -> {y=0, x=0}
  0x01, // [0|0|0|0| y_same| x_same|     0|x_byte] -> {y=0, x=1}
  0x10, // [0|0|0|0| y_same| x_same|y_byte|     0] -> {y=1, x=0}
  0x11, // [0|0|0|0| y_same| x_same|y_byte|x_byte] -> {y=1, x=1}
  0x02, // [0|0|0|0| y_same|!x_same|     0|     0] -> {y=0, x=2}
  0x01, // [0|0|0|0| y_same|!x_same|     0|x_byte] -> {y=0, x=1}
  0x12, // [0|0|0|0| y_same|!x_same|y_byte|     0] -> {y=1, x=2}
  0x11, // [0|0|0|0| y_same|!x_same|y_byte|x_byte] -> {y=1, x=1}
  0x20, // [0|0|0|0|!y_same| x_same|     0|     0] -> {y=2, x=0}
  0x21, // [0|0|0|0|!y_same| x_same|     0|x_byte] -> {y=2, x=1}
  0x10, // [0|0|0|0|!y_same| x_same|y_byte|     0] -> {y=1, x=0}
  0x11, // [0|0|0|0|!y_same| x_same|y_byte|x_byte] -> {y=1, x=1}
  0x22, // [0|0|0|0|!y_same|!x_same|     0|     0] -> {y=2, x=2}
  0x21, // [0|0|0|0|!y_same|!x_same|     0|x_byte] -> {y=2, x=1}
  0x12, // [0|0|0|0|!y_same|!x_same|y_byte|     0] -> {y=1, x=2}
  0x11  // [0|0|0|0|!y_same|!x_same|y_byte|x_byte] -> {y=1, x=1}
};

static constexpr uint8_t kDecodeImmOpZero    = 0x80; // hi={0x8x} lo={0x8x}
static constexpr uint8_t kDecodeImmOpWord    = 0x00; // hi={0x00} lo={0x01}
static constexpr uint8_t kDecodeImmOpBytePos = 0xCF; // hi={0x8x} lo={0x00}
static constexpr uint8_t kDecodeImmOpByteNeg = 0xEF; // hi={0xAx} lo={0x20}

alignas(16) const uint8_t decode_op_x_table[16] = {
  kDecodeImmOpZero,    // [0|?|?|?|?| x_same|?|     0]
  kDecodeImmOpBytePos, // [0|?|?|?|?| x_same|?|x_byte]
  kDecodeImmOpZero,    // [0|?|?|?|?| x_same|?|     0]
  kDecodeImmOpBytePos, // [0|?|?|?|?| x_same|?|x_byte]
  kDecodeImmOpWord,    // [0|?|?|?|?|!x_same|?|     0]
  kDecodeImmOpByteNeg, // [0|?|?|?|?|!x_same|?|x_byte]
  kDecodeImmOpWord,    // [0|?|?|?|?|!x_same|?|     0]
  kDecodeImmOpByteNeg, // [0|?|?|?|?|!x_same|?|x_byte]
  kDecodeImmOpZero,    // [0|?|?|?|?| x_same|?|     0]
  kDecodeImmOpBytePos, // [0|?|?|?|?| x_same|?|x_byte]
  kDecodeImmOpZero,    // [0|?|?|?|?| x_same|?|     0]
  kDecodeImmOpBytePos, // [0|?|?|?|?| x_same|?|x_byte]
  kDecodeImmOpWord,    // [0|?|?|?|?|!x_same|?|     0]
  kDecodeImmOpByteNeg, // [0|?|?|?|?|!x_same|?|x_byte]
  kDecodeImmOpWord,    // [0|?|?|?|?|!x_same|?|     0]
  kDecodeImmOpByteNeg  // [0|?|?|?|?|!x_same|?|x_byte]
};

alignas(16) const uint8_t decode_op_y_table[16] = {
  kDecodeImmOpZero,    // [0|?|?|?| y_same|?|     0|?]
  kDecodeImmOpZero,    // [0|?|?|?| y_same|?|     0|?]
  kDecodeImmOpBytePos, // [0|?|?|?| y_same|?|y_byte|?]
  kDecodeImmOpBytePos, // [0|?|?|?| y_same|?|y_byte|?]
  kDecodeImmOpZero,    // [0|?|?|?| y_same|?|     0|?]
  kDecodeImmOpZero,    // [0|?|?|?| y_same|?|     0|?]
  kDecodeImmOpBytePos, // [0|?|?|?| y_same|?|y_byte|?]
  kDecodeImmOpBytePos, // [0|?|?|?| y_same|?|y_byte|?]
  kDecodeImmOpWord,    // [0|?|?|?|!y_same|?|     0|?]
  kDecodeImmOpWord,    // [0|?|?|?|!y_same|?|     0|?]
  kDecodeImmOpByteNeg, // [0|?|?|?|!y_same|?|y_byte|?]
  kDecodeImmOpByteNeg, // [0|?|?|?|!y_same|?|y_byte|?]
  kDecodeImmOpWord,    // [0|?|?|?|!y_same|?|     0|?]
  kDecodeImmOpWord,    // [0|?|?|?|!y_same|?|     0|?]
  kDecodeImmOpByteNeg, // [0|?|?|?|!y_same|?|y_byte|?]
  kDecodeImmOpByteNeg  // [0|?|?|?|!y_same|?|y_byte|?]
};

} // {bl::OpenType::GlyfImpl}

#endif // BL_BUILD_OPT_SSE4_2 || BL_BUILD_OPT_ASIMD
