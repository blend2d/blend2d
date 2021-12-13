// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#ifdef BL_BUILD_OPT_SSSE3

#include "pixelconverter_p.h"
#include "simd_p.h"
#include "support/memops_p.h"

using namespace SIMD;

// PixelConverter - Copy|Shufb (SSSE3)
// ===================================

BLResult bl_convert_copy_shufb_8888_ssse3(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * 4;

  const BLPixelConverterData::ShufbData& d = blPixelConverterGetData(self)->shufbData;
  Vec128I fillMask = v_fill_i128_u32(d.fillMask);
  Vec128I predicate = v_loadu_i128(d.shufbPredicate);

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 16) {
      Vec128I p0, p1, p2, p3;
      p0 = v_loadu_i128(srcData +  0);
      p1 = v_loadu_i128(srcData + 16);
      p2 = v_loadu_i128(srcData + 32);
      p3 = v_loadu_i128(srcData + 48);

      p0 = v_or(v_shuffle_i8(p0, predicate), fillMask);
      p1 = v_or(v_shuffle_i8(p1, predicate), fillMask);
      p2 = v_or(v_shuffle_i8(p2, predicate), fillMask);
      p3 = v_or(v_shuffle_i8(p3, predicate), fillMask);

      v_storeu_i128(dstData +  0, p0);
      v_storeu_i128(dstData + 16, p1);
      v_storeu_i128(dstData + 32, p2);
      v_storeu_i128(dstData + 48, p3);

      dstData += 64;
      srcData += 64;
      i -= 16;
    }

    BL_NOUNROLL
    while (i >= 4) {
      Vec128I p0 = v_loadu_i128(srcData);
      v_storeu_i128(dstData, v_or(v_shuffle_i8(p0, predicate), fillMask));

      dstData += 16;
      srcData += 16;
      i -= 4;
    }

    BL_NOUNROLL
    while (i) {
      Vec128I p0 = v_load_i32(srcData);
      v_store_i32(dstData, v_or(v_shuffle_i8(p0, predicate), fillMask));

      dstData += 4;
      srcData += 4;
      i--;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// PixelConverter - RGB32 <- RGB24 (SSSE3)
// =======================================

BLResult bl_convert_rgb32_from_rgb24_shufb_ssse3(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * 3;

  const BLPixelConverterData::ShufbData& d = blPixelConverterGetData(self)->shufbData;
  Vec128I fillMask = v_fill_i128_u32(d.fillMask);
  Vec128I predicate = v_loadu_i128(d.shufbPredicate);

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 16) {
      Vec128I p0, p1, p2, p3;
      p0 = v_loadu_i128(srcData +  0);                 // [x5|z4 y4 x4|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]
      p1 = v_loadu_i128(srcData + 16);                 // [yA|xA|z9 y9|x9 z8 y8 x8|z7 y7 x7 z6|y6 x6 z5 y5]
      p3 = v_loadu_i128(srcData + 32);                 // [zF yF xF zE|yE xE zD yD|xD zC yC xC|zB yB xB zA]

      p2 = v_alignr_i8<8>(p3, p1);                     // [-- -- -- --|zB yB xB zA|yA|xA|z9 y9|x9 z8 y8 x8]
      p1 = v_alignr_i8<12>(p1, p0);                    // [-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5|x5|z4 y4 x4]
      p3 = v_srlb_i128<4>(p3);                         // [-- -- -- --|zF yF xF zE|yE xE zD yD|xD zC yC xC]

      p0 = v_or(v_shuffle_i8(p0, predicate), fillMask);
      p1 = v_or(v_shuffle_i8(p1, predicate), fillMask);
      p2 = v_or(v_shuffle_i8(p2, predicate), fillMask);
      p3 = v_or(v_shuffle_i8(p3, predicate), fillMask);

      v_storeu_i128(dstData +  0, p0);
      v_storeu_i128(dstData + 16, p1);
      v_storeu_i128(dstData + 32, p2);
      v_storeu_i128(dstData + 48, p3);

      dstData += 64;
      srcData += 48;
      i -= 16;
    }

    if (i >= 8) {
      Vec128I p0, p1;

      p0 = v_loadu_i128  (srcData +  0);               // [x5|z4 y4 x4|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]
      p1 = v_load_i64(srcData + 16);                   // [-- -- -- --|-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5]
      p1 = v_alignr_i8<12>(p1, p0);                    // [-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5|x5|z4 y4 x4]

      p0 = v_or(v_shuffle_i8(p0, predicate), fillMask);
      p1 = v_or(v_shuffle_i8(p1, predicate), fillMask);

      v_storeu_i128(dstData +  0, p0);
      v_storeu_i128(dstData + 16, p1);

      dstData += 32;
      srcData += 24;
      i -= 8;
    }

    if (i >= 4) {
      Vec128I p0, p1;

      p0 = v_load_i64(srcData +  0);                   // [-- -- -- --|-- -- -- --|y2 x2 z1 y1|x1 z0 y0 x0]
      p1 = v_load_i32(srcData +  8);                   // [-- -- -- --|-- -- -- --|-- -- -- --|z3 y3 x3 z2]
      p0 = v_interleave_lo_i64(p0, p1);                // [-- -- -- --|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]

      v_storeu_i128(dstData, v_or(v_shuffle_i8(p0, predicate), fillMask));

      dstData += 16;
      srcData += 12;
      i -= 4;
    }

    BL_NOUNROLL
    while (i) {
      uint32_t yx = BLMemOps::readU16u(srcData + 0);
      uint32_t z  = BLMemOps::readU8(srcData + 2);

      Vec128I p0 = v_i128_from_u32((z << 16) | yx);
      v_store_i32(dstData, v_or(v_shuffle_i8(p0, predicate), fillMask));

      dstData += 4;
      srcData += 3;
      i--;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// BLPixelConverter - Premultiply (SSSE3)
// ======================================

template<uint32_t A_Shift>
static BL_INLINE BLResult bl_convert_premultiply_8888_shufb_template_ssse3(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4u + gap;
  srcStride -= uintptr_t(w) * 4u;

  const BLPixelConverterData::ShufbData& d = blPixelConverterGetData(self)->shufbData;
  Vec128I zero = v_zero_i128();
  Vec128I a255 = v_fill_i128_u64(uint64_t(0xFFu) << (A_Shift * 2));

  Vec128I fillMask = v_fill_i128_u32(d.fillMask);
  Vec128I predicate = v_loadu_i128(d.shufbPredicate);

  // Alpha byte-index that can be used by instructions that perform shuffling.
  constexpr uint32_t AI = A_Shift / 8u;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 4) {
      Vec128I p0, p1;

      p0 = v_loadu_i128(srcData);
      p0 = v_shuffle_i8(p0, predicate);

      p1 = v_interleave_hi_i8(p0, zero);
      p0 = v_interleave_lo_i8(p0, zero);

      p0 = v_mul_i16(v_or(p0, a255), v_swizzle_i16<AI, AI, AI, AI>(p0));
      p1 = v_mul_i16(v_or(p1, a255), v_swizzle_i16<AI, AI, AI, AI>(p1));

      p0 = v_div255_u16(p0);
      p1 = v_div255_u16(p1);
      p0 = v_packs_i16_u8(p0, p1);
      p0 = v_or(p0, fillMask);
      v_storeu_i128(dstData, p0);

      dstData += 16;
      srcData += 16;
      i -= 4;
    }

    BL_NOUNROLL
    while (i) {
      Vec128I p0;

      p0 = v_load_i32(srcData);
      p0 = v_shuffle_i8(p0, predicate);
      p0 = v_interleave_lo_i8(p0, zero);
      p0 = v_mul_i16(v_or(p0, a255), v_swizzle_i16<AI, AI, AI, AI>(p0));
      p0 = v_div255_u16(p0);
      p0 = v_packs_i16_u8(p0, p0);
      p0 = v_or(p0, fillMask);
      v_store_i32(dstData, p0);

      dstData += 4;
      srcData += 4;
      i--;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

BLResult bl_convert_premultiply_8888_leading_alpha_shufb_ssse3(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return bl_convert_premultiply_8888_shufb_template_ssse3<24>(self, dstData, dstStride, srcData, srcStride, w, h, options);
}

BLResult bl_convert_premultiply_8888_trailing_alpha_shufb_ssse3(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return bl_convert_premultiply_8888_shufb_template_ssse3<0>(self, dstData, dstStride, srcData, srcStride, w, h, options);
}
#endif
