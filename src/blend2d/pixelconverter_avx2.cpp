// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#ifdef BL_BUILD_OPT_AVX2

#include "pixelconverter_p.h"
#include "simd_p.h"

using namespace SIMD;

// PixelConverter - Utilities (AVX2)
// =================================

static BL_INLINE Vec128I vpshufb_or(const Vec128I& x, const Vec128I& predicate, const Vec128I& or_mask) noexcept {
  return v_or(v_shuffle_i8(x, predicate), or_mask);
}

static BL_INLINE Vec256I vpshufb_or(const Vec256I& x, const Vec256I& predicate, const Vec256I& or_mask) noexcept {
  return v_or(v_shuffle_i8(x, predicate), or_mask);
}

// PixelConverter - Copy (AVX2)
// ============================

BLResult bl_convert_copy_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  const size_t bytesPerPixel = blPixelConverterGetData(self)->memCopyData.bytesPerPixel;
  const size_t byteWidth = size_t(w) * bytesPerPixel;

  // Use a generic copy if `byteWidth` is small as we would not be able to
  // utilize SIMD properly - in general we want to use at least 16-byte RW.
  if (byteWidth < 16)
    return bl_convert_copy(self, dstData, dstStride, srcData, srcStride, w, h, options);

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= intptr_t(byteWidth + gap);
  srcStride -= intptr_t(byteWidth);

  for (uint32_t y = h; y != 0; y--) {
    size_t i = byteWidth;

    BL_NOUNROLL
    while (i >= 64) {
      Vec256I p0 = v_loadu_i256(srcData +  0);
      Vec256I p1 = v_loadu_i256(srcData + 32);

      v_storeu_i256(dstData +  0, p0);
      v_storeu_i256(dstData + 32, p1);

      dstData += 64;
      srcData += 64;
      i -= 64;
    }

    BL_NOUNROLL
    while (i >= 16) {
      v_storeu_i128(dstData, v_loadu_i128(srcData));

      dstData += 16;
      srcData += 16;
      i -= 16;
    }

    if (i) {
      dstData += i;
      srcData += i;
      v_storeu_i128(dstData - 16, v_loadu_i128(srcData - 16));
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// PixelConverter - Copy|Or (AVX2)
// ===============================

BLResult bl_convert_copy_or_8888_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * 4;

  Vec256I fillMask = v_fill_i256_u32(blPixelConverterGetData(self)->memCopyData.fillMask);
  Vec256I loadStoreMask = v_load_i64_i8_i32(&blCommonTable.loadstore16_lo8_msk8()[w & 7]);

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 32) {
      Vec256I p0 = v_loadu_i256(srcData +  0);
      Vec256I p1 = v_loadu_i256(srcData + 32);
      Vec256I p2 = v_loadu_i256(srcData + 64);
      Vec256I p3 = v_loadu_i256(srcData + 96);

      v_storeu_i256(dstData +  0, v_or(p0, fillMask));
      v_storeu_i256(dstData + 32, v_or(p1, fillMask));
      v_storeu_i256(dstData + 64, v_or(p2, fillMask));
      v_storeu_i256(dstData + 96, v_or(p3, fillMask));

      dstData += 128;
      srcData += 128;
      i -= 32;
    }

    BL_NOUNROLL
    while (i >= 8) {
      Vec256I p0 = v_loadu_i256(srcData);
      v_storeu_i256(dstData, v_or(p0, fillMask));

      dstData += 32;
      srcData += 32;
      i -= 8;
    }

    if (i) {
      Vec256I p0 = v_loadu_i256_mask32(srcData, loadStoreMask);
      v_storeu_i256_mask32(dstData, v_or(p0, fillMask), loadStoreMask);

      dstData += i * 4;
      srcData += i * 4;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// PixelConverter - Copy|Shufb (AVX2)
// ==================================

BLResult bl_convert_copy_shufb_8888_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * 4;

  const BLPixelConverterData::ShufbData& d = blPixelConverterGetData(self)->shufbData;

  Vec256I fillMask = v_fill_i256_u32(d.fillMask);
  Vec256I predicate = v_dupl_i128(v_loadu_i128(d.shufbPredicate));
  Vec256I loadStoreMask = v_load_i64_i8_i32(&blCommonTable.loadstore16_lo8_msk8()[w & 7]);

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 32) {
      Vec256I p0 = v_loadu_i256(srcData +  0);
      Vec256I p1 = v_loadu_i256(srcData + 32);
      Vec256I p2 = v_loadu_i256(srcData + 64);
      Vec256I p3 = v_loadu_i256(srcData + 96);

      p0 = vpshufb_or(p0, predicate, fillMask);
      p1 = vpshufb_or(p1, predicate, fillMask);
      p2 = vpshufb_or(p2, predicate, fillMask);
      p3 = vpshufb_or(p3, predicate, fillMask);

      v_storeu_i256(dstData +  0, p0);
      v_storeu_i256(dstData + 32, p1);
      v_storeu_i256(dstData + 64, p2);
      v_storeu_i256(dstData + 96, p3);

      dstData += 128;
      srcData += 128;
      i -= 32;
    }

    BL_NOUNROLL
    while (i >= 8) {
      Vec256I p0 = v_loadu_i256(srcData);

      p0 = v_shuffle_i8(p0, predicate);
      p0 = v_or(p0, fillMask);
      v_storeu_i256(dstData, p0);

      dstData += 32;
      srcData += 32;
      i -= 8;
    }

    if (i) {
      Vec256I p0 = v_loadu_i256_mask32(srcData, loadStoreMask);

      p0 = v_shuffle_i8(p0, predicate);
      p0 = v_or(p0, fillMask);
      v_storeu_i256_mask32(dstData, p0, loadStoreMask);

      dstData += i * 4;
      srcData += i * 4;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// PixelConverter - RGB32 <- RGB24 (AVX2)
// ======================================

BLResult bl_convert_rgb32_from_rgb24_shufb_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * 3;

  const BLPixelConverterData::ShufbData& d = blPixelConverterGetData(self)->shufbData;
  Vec128I loadStoreMask = v_load_i32_i8_i32(&blCommonTable.loadstore16_lo8_msk8()[w & 3]);

  Vec256I fillMask = v_fill_i256_u32(d.fillMask);
  Vec256I predicate = v_dupl_i128(v_loadu_i128(d.shufbPredicate));

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 32) {
      Vec256I p0, p1, p2, p3;
      Vec256I q0, q1, q2, q3;

      p0 = v_loadu_i256_128(srcData +  0);             // [x5|z4 y4 x4|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]
      p1 = v_loadu_i256_128(srcData + 16);             // [yA|xA|z9 y9|x9 z8 y8 x8|z7 y7 x7 z6|y6 x6 z5 y5]
      p3 = v_loadu_i256_128(srcData + 32);             // [zF yF xF zE|yE xE zD yD|xD zC yC xC|zB yB xB zA]

      p2 = v_alignr_i8<8>(p3, p1);                     // [-- -- -- --|zB yB xB zA|yA|xA|z9 y9|x9 z8 y8 x8]
      p1 = v_alignr_i8<12>(p1, p0);                    // [-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5|x5|z4 y4 x4]
      p3 = v_srlb_i128<4>(p3);                         // [-- -- -- --|zF yF xF zE|yE xE zD yD|xD zC yC xC]

      p0 = v_interleave_lo_i128(p0, p1);
      p2 = v_interleave_lo_i128(p2, p3);

      q0 = v_loadu_i256_128(srcData + 48);             // [x5|z4 y4 x4|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]
      q1 = v_loadu_i256_128(srcData + 64);             // [yA|xA|z9 y9|x9 z8 y8 x8|z7 y7 x7 z6|y6 x6 z5 y5]
      q3 = v_loadu_i256_128(srcData + 80);             // [zF yF xF zE|yE xE zD yD|xD zC yC xC|zB yB xB zA]

      q2 = v_alignr_i8<8>(q3, q1);                     // [-- -- -- --|zB yB xB zA|yA|xA|z9 y9|x9 z8 y8 x8]
      q1 = v_alignr_i8<12>(q1, q0);                    // [-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5|x5|z4 y4 x4]
      q3 = v_srlb_i128<4>(q3);                         // [-- -- -- --|zF yF xF zE|yE xE zD yD|xD zC yC xC]

      q0 = v_interleave_lo_i128(q0, q1);
      q2 = v_interleave_lo_i128(q2, q3);

      p0 = vpshufb_or(p0, predicate, fillMask);
      p2 = vpshufb_or(p2, predicate, fillMask);
      q0 = vpshufb_or(q0, predicate, fillMask);
      q2 = vpshufb_or(q2, predicate, fillMask);

      v_storeu_i256(dstData +  0, p0);
      v_storeu_i256(dstData + 32, p2);
      v_storeu_i256(dstData + 64, q0);
      v_storeu_i256(dstData + 96, q2);

      dstData += 128;
      srcData += 96;
      i -= 32;
    }

    BL_NOUNROLL
    while (i >= 8) {
      Vec128I p0, p1;

      p0 = v_loadu_i128(srcData);                      // [x5|z4 y4 x4|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]
      p1 = v_load_i64(srcData + 16);                   // [-- -- -- --|-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5]
      p1 = v_alignr_i8<12>(p1, p0);                    // [-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5|x5|z4 y4 x4]

      p0 = vpshufb_or(p0, v_cast<Vec128I>(predicate), v_cast<Vec128I>(fillMask));
      p1 = vpshufb_or(p1, v_cast<Vec128I>(predicate), v_cast<Vec128I>(fillMask));

      v_storeu_i128(dstData +  0, p0);
      v_storeu_i128(dstData + 16, p1);

      dstData += 32;
      srcData += 24;
      i -= 8;
    }

    if (i >= 4) {
      Vec128I p0;

      p0 = v_load_i64(srcData);                        // [-- -- -- --|-- -- -- --|y2 x2 z1 y1|x1 z0 y0 x0]
      p0 = v_insertm_u32<2>(p0, srcData + 8);          // [-- -- -- --|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]

      p0 = vpshufb_or(p0, v_cast<Vec128I>(predicate), v_cast<Vec128I>(fillMask));
      v_storeu_i128(dstData, p0);

      dstData += 16;
      srcData += 12;
      i -= 4;
    }

    if (i) {
      Vec128I p0 = v_zero_i128();
      p0 = v_insertm_u24<0>(p0, srcData + 0);          // [-- -- -- --|-- -- -- --|-- -- -- --|-- z0 y0 x0]
      if (i >= 2) {
        p0 = v_insertm_u24<3>(p0, srcData + 3);        // [-- -- -- --|-- -- -- --|-- -- z1 y1|x1 z0 y0 x0]
        if (i >= 3) {
          p0 = v_insertm_u24<6>(p0, srcData + 6);      // [-- -- -- --|-- -- -- z2|y2 x2 z1 y1|x1 z0 y0 x0]
        }
      }

      p0 = vpshufb_or(p0, v_cast<Vec128I>(predicate), v_cast<Vec128I>(fillMask));
      v_storeu_i128_mask32(dstData, p0, loadStoreMask);

      dstData += i * 4;
      srcData += i * 3;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// PixelConverter - Premultiply (AVX2)
// ===================================

template<uint32_t A_Shift, bool UseShufB>
static BL_INLINE BLResult bl_convert_premultiply_8888_template_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * 4;

  const BLPixelConverterData::PremultiplyData& d = blPixelConverterGetData(self)->premultiplyData;

  Vec256I zero = v_zero_i256();
  Vec256I a255 = v_fill_i256_u64(uint64_t(0xFFu) << (A_Shift * 2));

  Vec256I fillMask = v_fill_i256_u32(d.fillMask);
  Vec256I predicate;

  if (UseShufB)
    predicate = v_dupl_i128(v_loadu_i128(d.shufbPredicate));

  Vec256I loadStoreMaskLo = v_load_i64_i8_i32(&blCommonTable.loadstore16_lo8_msk8()[w & 15]);
  Vec256I loadStoreMaskHi = v_load_i64_i8_i32(&blCommonTable.loadstore16_hi8_msk8()[w & 15]);

  // Alpha byte-index that can be used by instructions that perform shuffling.
  constexpr uint32_t AI = A_Shift / 8u;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 16) {
      Vec256I p0, p1, p2, p3;

      p0 = v_loadu_i256(srcData +  0);
      p2 = v_loadu_i256(srcData + 32);

      if (UseShufB) p0 = v_shuffle_i8(p0, predicate);
      if (UseShufB) p2 = v_shuffle_i8(p2, predicate);

      p1 = v_interleave_hi_i8(p0, zero);
      p0 = v_interleave_lo_i8(p0, zero);
      p3 = v_interleave_hi_i8(p2, zero);
      p2 = v_interleave_lo_i8(p2, zero);

      p0 = v_mul_i16(v_or(p0, a255), v_swizzle_i16<AI, AI, AI, AI>(p0));
      p1 = v_mul_i16(v_or(p1, a255), v_swizzle_i16<AI, AI, AI, AI>(p1));
      p2 = v_mul_i16(v_or(p2, a255), v_swizzle_i16<AI, AI, AI, AI>(p2));
      p3 = v_mul_i16(v_or(p3, a255), v_swizzle_i16<AI, AI, AI, AI>(p3));

      p0 = v_div255_u16(p0);
      p1 = v_div255_u16(p1);
      p2 = v_div255_u16(p2);
      p3 = v_div255_u16(p3);

      p0 = v_packs_i16_u8(p0, p1);
      p2 = v_packs_i16_u8(p2, p3);

      p0 = v_or(p0, fillMask);
      p2 = v_or(p2, fillMask);

      v_storeu_i256(dstData +  0, p0);
      v_storeu_i256(dstData + 32, p2);

      dstData += 64;
      srcData += 64;
      i -= 16;
    }

    if (i) {
      Vec256I p0, p1, p2, p3;

      p0 = v_loadu_i256_mask32(srcData +  0, loadStoreMaskLo);
      p2 = v_loadu_i256_mask32(srcData + 32, loadStoreMaskHi);

      if (UseShufB) p0 = v_shuffle_i8(p0, predicate);
      if (UseShufB) p2 = v_shuffle_i8(p2, predicate);

      p1 = v_interleave_hi_i8(p0, zero);
      p0 = v_interleave_lo_i8(p0, zero);
      p3 = v_interleave_hi_i8(p2, zero);
      p2 = v_interleave_lo_i8(p2, zero);

      p0 = v_mul_i16(v_or(p0, a255), v_swizzle_i16<AI, AI, AI, AI>(p0));
      p1 = v_mul_i16(v_or(p1, a255), v_swizzle_i16<AI, AI, AI, AI>(p1));
      p2 = v_mul_i16(v_or(p2, a255), v_swizzle_i16<AI, AI, AI, AI>(p2));
      p3 = v_mul_i16(v_or(p3, a255), v_swizzle_i16<AI, AI, AI, AI>(p3));

      p0 = v_div255_u16(p0);
      p1 = v_div255_u16(p1);
      p2 = v_div255_u16(p2);
      p3 = v_div255_u16(p3);

      p0 = v_packs_i16_u8(p0, p1);
      p2 = v_packs_i16_u8(p2, p3);

      p0 = v_or(p0, fillMask);
      p2 = v_or(p2, fillMask);

      v_storeu_i256_mask32(dstData +  0, p0, loadStoreMaskLo);
      v_storeu_i256_mask32(dstData + 32, p2, loadStoreMaskHi);

      dstData += i * 4;
      srcData += i * 4;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

BLResult bl_convert_premultiply_8888_leading_alpha_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return bl_convert_premultiply_8888_template_avx2<24, false>(self, dstData, dstStride, srcData, srcStride, w, h, options);
}

BLResult bl_convert_premultiply_8888_trailing_alpha_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return bl_convert_premultiply_8888_template_avx2<0, false>(self, dstData, dstStride, srcData, srcStride, w, h, options);
}

BLResult bl_convert_premultiply_8888_leading_alpha_shufb_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return bl_convert_premultiply_8888_template_avx2<24, true>(self, dstData, dstStride, srcData, srcStride, w, h, options);
}

BLResult bl_convert_premultiply_8888_trailing_alpha_shufb_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return bl_convert_premultiply_8888_template_avx2<0, true>(self, dstData, dstStride, srcData, srcStride, w, h, options);
}

// PixelConverter - Unpremultiply (PMULLD) (AVX2)
// ==============================================

template<uint32_t A_Shift>
static BL_INLINE BLResult bl_convert_unpremultiply_8888_pmulld_template_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  blUnused(self);

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4u + gap;
  srcStride -= uintptr_t(w) * 4u;

  const uint32_t* rcpTable = blCommonTable.unpremultiplyRcp;

  Vec256I alphaMask = v_fill_i256_u32(0xFFu << A_Shift);
  Vec256I componentMask = v_fill_i256_u32(0xFFu);
  Vec256I loadStoreMask = v_load_i64_i8_i32(&blCommonTable.loadstore16_lo8_msk8()[w & 7]);

  Vec256I rnd = v_fill_i256_u32(0x8000u);

  // Alpha byte-index that can be used by instructions that perform shuffling.
  constexpr uint32_t AI = A_Shift / 8u;
  constexpr uint32_t RI = (AI + 1) % 4;
  constexpr uint32_t GI = (AI + 2) % 4;
  constexpr uint32_t BI = (AI + 3) % 4;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 8) {
      Vec256I pix = v_loadu_i256(srcData);
      Vec128I rcpLo = v_load_i32(rcpTable + srcData[0 * 4 + AI]);
      Vec128I rcpHi = v_load_i32(rcpTable + srcData[4 * 4 + AI]);

      rcpLo = v_insertm_u32<1>(rcpLo, rcpTable + srcData[1 * 4 + AI]);
      rcpHi = v_insertm_u32<1>(rcpHi, rcpTable + srcData[5 * 4 + AI]);

      rcpLo = v_insertm_u32<2>(rcpLo, rcpTable + srcData[2 * 4 + AI]);
      rcpHi = v_insertm_u32<2>(rcpHi, rcpTable + srcData[6 * 4 + AI]);

      rcpLo = v_insertm_u32<3>(rcpLo, rcpTable + srcData[3 * 4 + AI]);
      rcpHi = v_insertm_u32<3>(rcpHi, rcpTable + srcData[7 * 4 + AI]);

      Vec256I rcp = v_interleave_lo_i128(rcpLo, rcpHi);

      Vec256I pr = v_srl_i32<RI * 8>(pix);
      Vec256I pg = v_srl_i32<GI * 8>(pix);
      Vec256I pb = v_srl_i32<BI * 8>(pix);

      if (RI != 3) pr = v_and(pr, componentMask);
      if (GI != 3) pg = v_and(pg, componentMask);
      if (BI != 3) pb = v_and(pb, componentMask);

      pr = v_mul_u32(pr, rcp);
      pg = v_mul_u32(pg, rcp);
      pb = v_mul_u32(pb, rcp);

      pix = v_and(pix, alphaMask);
      pr = v_sll_i32<RI * 8>(v_srl_i32<16>(v_add_i32(pr, rnd)));
      pg = v_sll_i32<GI * 8>(v_srl_i32<16>(v_add_i32(pg, rnd)));
      pb = v_sll_i32<BI * 8>(v_srl_i32<16>(v_add_i32(pb, rnd)));

      pix = v_or(pix, pr);
      pix = v_or(pix, pg);
      pix = v_or(pix, pb);
      v_storeu_i256(dstData, pix);

      dstData += 32;
      srcData += 32;
      i -= 8;
    }

    if (i) {
      Vec256I pix = v_loadu_i256_mask32(srcData, loadStoreMask);
      Vec256I pixHi = v_permute_i128<1, 1>(pix);

      size_t idx0 = v_extract_u8<0 * 4 + AI>(v_cast<Vec128I>(pix));
      size_t idx4 = v_extract_u8<0 * 4 + AI>(v_cast<Vec128I>(pixHi));

      Vec128I rcpLo = v_load_i32(rcpTable + idx0);
      Vec128I rcpHi = v_load_i32(rcpTable + idx4);

      size_t idx1 = v_extract_u8<1 * 4 + AI>(v_cast<Vec128I>(pix));
      size_t idx5 = v_extract_u8<1 * 4 + AI>(v_cast<Vec128I>(pixHi));

      rcpLo = v_insertm_u32<1>(rcpLo, rcpTable + idx1);
      rcpHi = v_insertm_u32<1>(rcpHi, rcpTable + idx5);

      size_t idx2 = v_extract_u8<2 * 4 + AI>(v_cast<Vec128I>(pix));
      size_t idx6 = v_extract_u8<2 * 4 + AI>(v_cast<Vec128I>(pixHi));

      rcpLo = v_insertm_u32<2>(rcpLo, rcpTable + idx2);
      rcpHi = v_insertm_u32<2>(rcpHi, rcpTable + idx6);

      size_t idx3 = v_extract_u8<3 * 4 + AI>(v_cast<Vec128I>(pix));
      size_t idx7 = v_extract_u8<3 * 4 + AI>(v_cast<Vec128I>(pixHi));

      rcpLo = v_insertm_u32<3>(rcpLo, rcpTable + idx3);
      rcpHi = v_insertm_u32<3>(rcpHi, rcpTable + idx7);

      Vec256I rcp = v_interleave_lo_i128(rcpLo, rcpHi);

      Vec256I pr = v_srl_i32<RI * 8>(pix);
      Vec256I pg = v_srl_i32<GI * 8>(pix);
      Vec256I pb = v_srl_i32<BI * 8>(pix);

      if (RI != 3) pr = v_and(pr, componentMask);
      if (GI != 3) pg = v_and(pg, componentMask);
      if (BI != 3) pb = v_and(pb, componentMask);

      pr = v_mul_u32(pr, rcp);
      pg = v_mul_u32(pg, rcp);
      pb = v_mul_u32(pb, rcp);

      pix = v_and(pix, alphaMask);
      pr = v_sll_i32<RI * 8>(v_srl_i32<16>(v_add_i32(pr, rnd)));
      pg = v_sll_i32<GI * 8>(v_srl_i32<16>(v_add_i32(pg, rnd)));
      pb = v_sll_i32<BI * 8>(v_srl_i32<16>(v_add_i32(pb, rnd)));

      pix = v_or(pix, pr);
      pix = v_or(pix, pg);
      pix = v_or(pix, pb);
      v_storeu_i256_mask32(dstData, pix, loadStoreMask);

      dstData += i * 4;
      srcData += i * 4;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

BLResult bl_convert_unpremultiply_8888_leading_alpha_pmulld_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return bl_convert_unpremultiply_8888_pmulld_template_avx2<24>(self, dstData, dstStride, srcData, srcStride, w, h, options);
}

BLResult bl_convert_unpremultiply_8888_trailing_alpha_pmulld_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return bl_convert_unpremultiply_8888_pmulld_template_avx2<0>(self, dstData, dstStride, srcData, srcStride, w, h, options);
}

// PixelConverter - Unpremultiply (FLOAT) (AVX2)
// =============================================

template<uint32_t A_Shift>
static BL_INLINE BLResult bl_convert_unpremultiply_8888_float_template_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  blUnused(self);

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4u + gap;
  srcStride -= uintptr_t(w) * 4u;

  Vec256I alphaMask = v_fill_i256_u32(0xFFu << A_Shift);
  Vec256I componentMask = v_fill_i256_u32(0xFFu);
  Vec256I loadStoreMask = v_load_i64_i8_i32(&blCommonTable.loadstore16_lo8_msk8()[w & 7]);

  Vec256F const255 = v_fill_f256(255.0001f);
  Vec256F lessThanOne = v_fill_f256(0.1f);

  // Alpha byte-index that can be used by instructions that perform shuffling.
  constexpr uint32_t AI = A_Shift / 8u;
  constexpr uint32_t RI = (AI + 1) % 4;
  constexpr uint32_t GI = (AI + 2) % 4;
  constexpr uint32_t BI = (AI + 3) % 4;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 8) {
      Vec256I pix = v_loadu_i256(srcData);
      Vec256I pa = v_srl_i32<AI * 8>(pix);
      Vec256I pr = v_srl_i32<RI * 8>(pix);

      if (AI != 3) pa = v_and(pa, componentMask);
      if (RI != 3) pr = v_and(pr, componentMask);

      Vec256F fa = v_cvt_i32_f32(pa);
      Vec256F fr = v_cvt_i32_f32(pr);

      fa = v_div_f32(const255, v_max_f32(fa, lessThanOne));

      Vec256I pg = v_srl_i32<GI * 8>(pix);
      Vec256I pb = v_srl_i32<BI * 8>(pix);

      if (GI != 3) pg = v_and(pg, componentMask);
      if (BI != 3) pb = v_and(pb, componentMask);

      Vec256F fg = v_cvt_i32_f32(pg);
      Vec256F fb = v_cvt_i32_f32(pb);

      fr = v_mul_f32(fr, fa);
      fg = v_mul_f32(fg, fa);
      fb = v_mul_f32(fb, fa);

      pix = v_and(pix, alphaMask);
      pr = v_cvt_f32_i32(fr);
      pg = v_cvt_f32_i32(fg);
      pb = v_cvt_f32_i32(fb);

      pr = v_sll_i32<RI * 8>(pr);
      pg = v_sll_i32<GI * 8>(pg);
      pb = v_sll_i32<BI * 8>(pb);

      pix = v_or(pix, pr);
      pix = v_or(pix, pg);
      pix = v_or(pix, pb);
      v_storeu_i256(dstData, pix);

      dstData += 32;
      srcData += 32;
      i -= 8;
    }

    if (i) {
      Vec256I pix = v_loadu_i256_mask32(srcData, loadStoreMask);

      Vec256I pa = v_srl_i32<AI * 8>(pix);
      Vec256I pr = v_srl_i32<RI * 8>(pix);

      if (AI != 3) pa = v_and(pa, componentMask);
      if (RI != 3) pr = v_and(pr, componentMask);

      Vec256F fa = v_cvt_i32_f32(pa);
      Vec256F fr = v_cvt_i32_f32(pr);

      fa = v_div_f32(const255, v_max_f32(fa, lessThanOne));

      Vec256I pg = v_srl_i32<GI * 8>(pix);
      Vec256I pb = v_srl_i32<BI * 8>(pix);

      if (GI != 3) pg = v_and(pg, componentMask);
      if (BI != 3) pb = v_and(pb, componentMask);

      Vec256F fg = v_cvt_i32_f32(pg);
      Vec256F fb = v_cvt_i32_f32(pb);

      fr = v_mul_f32(fr, fa);
      fg = v_mul_f32(fg, fa);
      fb = v_mul_f32(fb, fa);

      pix = v_and(pix, alphaMask);
      pr = v_cvt_f32_i32(fr);
      pg = v_cvt_f32_i32(fg);
      pb = v_cvt_f32_i32(fb);

      pr = v_sll_i32<RI * 8>(pr);
      pg = v_sll_i32<GI * 8>(pg);
      pb = v_sll_i32<BI * 8>(pb);

      pix = v_or(pix, pr);
      pix = v_or(pix, pg);
      pix = v_or(pix, pb);
      v_storeu_i256_mask32(dstData, pix, loadStoreMask);

      dstData += i * 4;
      srcData += i * 4;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

BLResult bl_convert_unpremultiply_8888_leading_alpha_float_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return bl_convert_unpremultiply_8888_float_template_avx2<24>(self, dstData, dstStride, srcData, srcStride, w, h, options);
}

BLResult bl_convert_unpremultiply_8888_trailing_alpha_float_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return bl_convert_unpremultiply_8888_float_template_avx2<0>(self, dstData, dstStride, srcData, srcStride, w, h, options);
}

#endif
