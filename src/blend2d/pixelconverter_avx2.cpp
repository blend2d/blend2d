// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#ifdef BL_BUILD_OPT_AVX2

#include "pixelconverter_p.h"
#include "simd/simd_p.h"

// PixelConverter - Copy (AVX2)
// ============================

BLResult bl_convert_copy_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  using namespace SIMD;

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
      Vec32xU8 p0 = loadu<Vec32xU8>(srcData +  0);
      Vec32xU8 p1 = loadu<Vec32xU8>(srcData + 32);

      storeu(dstData +  0, p0);
      storeu(dstData + 32, p1);

      dstData += 64;
      srcData += 64;
      i -= 64;
    }

    BL_NOUNROLL
    while (i >= 16) {
      storeu(dstData, loadu<Vec16xU8>(srcData));

      dstData += 16;
      srcData += 16;
      i -= 16;
    }

    if (i) {
      dstData += i;
      srcData += i;
      storeu(dstData - 16, loadu<Vec16xU8>(srcData - 16));
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

  using namespace SIMD;

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * 4;

  Vec32xU8 fillMask = make256_u32<Vec32xU8>(blPixelConverterGetData(self)->memCopyData.fillMask);
  Vec32xU8 loadStoreMask = loada_64_i8_i32<Vec32xU8>(bl::commonTable.loadstore16_lo8_msk8() + (w & 7u));

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 32) {
      Vec32xU8 p0 = loadu<Vec32xU8>(srcData +  0);
      Vec32xU8 p1 = loadu<Vec32xU8>(srcData + 32);
      Vec32xU8 p2 = loadu<Vec32xU8>(srcData + 64);
      Vec32xU8 p3 = loadu<Vec32xU8>(srcData + 96);

      storeu(dstData +  0, p0 | fillMask);
      storeu(dstData + 32, p1 | fillMask);
      storeu(dstData + 64, p2 | fillMask);
      storeu(dstData + 96, p3 | fillMask);

      dstData += 128;
      srcData += 128;
      i -= 32;
    }

    BL_NOUNROLL
    while (i >= 8) {
      Vec32xU8 p0 = loadu<Vec32xU8>(srcData);
      storeu(dstData, p0 | fillMask);

      dstData += 32;
      srcData += 32;
      i -= 8;
    }

    if (i) {
      Vec32xU8 p0 = loadu_256_mask32<Vec32xU8>(srcData, loadStoreMask);
      storeu_256_mask32(dstData, p0 | fillMask, loadStoreMask);

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

  using namespace SIMD;

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * 4;

  const BLPixelConverterData::ShufbData& d = blPixelConverterGetData(self)->shufbData;

  Vec32xU8 fillMask = make256_u32<Vec32xU8>(blPixelConverterGetData(self)->memCopyData.fillMask);
  Vec32xU8 predicate = broadcast_i128<Vec32xU8>(loadu<Vec16xU8>(d.shufbPredicate));
  Vec32xU8 loadStoreMask = loada_64_i8_i32<Vec32xU8>(bl::commonTable.loadstore16_lo8_msk8() + (w & 7u));

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 32) {
      Vec32xU8 p0 = loadu<Vec32xU8>(srcData +  0);
      Vec32xU8 p1 = loadu<Vec32xU8>(srcData + 32);
      Vec32xU8 p2 = loadu<Vec32xU8>(srcData + 64);
      Vec32xU8 p3 = loadu<Vec32xU8>(srcData + 96);

      storeu(dstData +  0, swizzlev_u8(p0, predicate) | fillMask);
      storeu(dstData + 32, swizzlev_u8(p1, predicate) | fillMask);
      storeu(dstData + 64, swizzlev_u8(p2, predicate) | fillMask);
      storeu(dstData + 96, swizzlev_u8(p3, predicate) | fillMask);

      dstData += 128;
      srcData += 128;
      i -= 32;
    }

    BL_NOUNROLL
    while (i >= 8) {
      Vec32xU8 p0 = loadu<Vec32xU8>(srcData);
      storeu(dstData, swizzlev_u8(p0, predicate) | fillMask);

      dstData += 32;
      srcData += 32;
      i -= 8;
    }

    if (i) {
      Vec32xU8 p0 = loadu_256_mask32<Vec32xU8>(srcData, loadStoreMask);
      storeu_256_mask32(dstData, swizzlev_u8(p0, predicate) | fillMask, loadStoreMask);

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

  using namespace SIMD;

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * 3;

  const BLPixelConverterData::ShufbData& d = blPixelConverterGetData(self)->shufbData;

  Vec32xU8 fillMask = make256_u32<Vec32xU8>(blPixelConverterGetData(self)->memCopyData.fillMask);
  Vec32xU8 predicate = broadcast_i128<Vec32xU8>(loadu<Vec16xU8>(d.shufbPredicate));
  Vec16xU8 loadStoreMask = loada_32_i8_i32<Vec16xU8>(bl::commonTable.loadstore16_lo8_msk8() + (w & 3u));

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 32) {
      Vec32xU8 p0, p1, p2, p3;
      Vec32xU8 q0, q1, q2, q3;

      p0 = loadu_128<Vec32xU8>(srcData +  0);          // [x5|z4 y4 x4|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]
      p1 = loadu_128<Vec32xU8>(srcData + 16);          // [yA|xA|z9 y9|x9 z8 y8 x8|z7 y7 x7 z6|y6 x6 z5 y5]
      p3 = loadu_128<Vec32xU8>(srcData + 32);          // [zF yF xF zE|yE xE zD yD|xD zC yC xC|zB yB xB zA]

      p2 = alignr_u128<8>(p3, p1);                       // [-- -- -- --|zB yB xB zA|yA|xA|z9 y9|x9 z8 y8 x8]
      p1 = alignr_u128<12>(p1, p0);                      // [-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5|x5|z4 y4 x4]
      p3 = srlb_u128<4>(p3);                           // [-- -- -- --|zF yF xF zE|yE xE zD yD|xD zC yC xC]

      p0 = interleave_i128(p0, p1);
      p2 = interleave_i128(p2, p3);

      q0 = loadu_128<Vec32xU8>(srcData + 48);          // [x5|z4 y4 x4|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]
      q1 = loadu_128<Vec32xU8>(srcData + 64);          // [yA|xA|z9 y9|x9 z8 y8 x8|z7 y7 x7 z6|y6 x6 z5 y5]
      q3 = loadu_128<Vec32xU8>(srcData + 80);          // [zF yF xF zE|yE xE zD yD|xD zC yC xC|zB yB xB zA]

      q2 = alignr_u128<8>(q3, q1);                       // [-- -- -- --|zB yB xB zA|yA|xA|z9 y9|x9 z8 y8 x8]
      q1 = alignr_u128<12>(q1, q0);                      // [-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5|x5|z4 y4 x4]
      q3 = srlb_u128<4>(q3);                           // [-- -- -- --|zF yF xF zE|yE xE zD yD|xD zC yC xC]

      q0 = interleave_i128(q0, q1);
      q2 = interleave_i128(q2, q3);

      storeu(dstData +  0, swizzlev_u8(p0, predicate) | fillMask);
      storeu(dstData + 32, swizzlev_u8(p2, predicate) | fillMask);
      storeu(dstData + 64, swizzlev_u8(q0, predicate) | fillMask);
      storeu(dstData + 96, swizzlev_u8(q2, predicate) | fillMask);

      dstData += 128;
      srcData += 96;
      i -= 32;
    }

    BL_NOUNROLL
    while (i >= 8) {
      Vec16xU8 p0, p1;

      p0 = loadu<Vec16xU8>(srcData);                   // [x5|z4 y4 x4|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]
      p1 = loadu_64<Vec16xU8>(srcData + 16);           // [-- -- -- --|-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5]
      p1 = alignr_u128<12>(p1, p0);                      // [-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5|x5|z4 y4 x4]

      storeu(dstData +  0, swizzlev_u8(p0, vec_128(predicate)) | vec_128(fillMask));
      storeu(dstData + 16, swizzlev_u8(p1, vec_128(predicate)) | vec_128(fillMask));

      dstData += 32;
      srcData += 24;
      i -= 8;
    }

    if (i >= 4) {
      Vec16xU8 p0;

      p0 = loadu_64<Vec16xU8>(srcData);                // [-- -- -- --|-- -- -- --|y2 x2 z1 y1|x1 z0 y0 x0]
      p0 = insert_m32<2>(p0, srcData + 8);             // [-- -- -- --|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]

      storeu(dstData, swizzlev_u8(p0, vec_128(predicate)) | vec_128(fillMask));

      dstData += 16;
      srcData += 12;
      i -= 4;
    }

    if (i) {
      Vec16xU8 p0 = make_zero<Vec16xU8>();
      p0 = insert_m24<0>(p0, srcData + 0);             // [-- -- -- --|-- -- -- --|-- -- -- --|-- z0 y0 x0]
      if (i >= 2) {
        p0 = insert_m24<3>(p0, srcData + 3);           // [-- -- -- --|-- -- -- --|-- -- z1 y1|x1 z0 y0 x0]
        if (i >= 3) {
          p0 = insert_m24<6>(p0, srcData + 6);         // [-- -- -- --|-- -- -- z2|y2 x2 z1 y1|x1 z0 y0 x0]
        }
      }

      storeu_128_mask32(dstData, swizzlev_u8(p0, vec_128(predicate)) | vec_128(fillMask), loadStoreMask);

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

  using namespace SIMD;

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * 4;

  const BLPixelConverterData::PremultiplyData& d = blPixelConverterGetData(self)->premultiplyData;

  Vec32xU8 zero = make_zero<Vec32xU8>();
  Vec32xU8 fillMask = make256_u32<Vec32xU8>(d.fillMask);
  Vec16xU16 alphaMask = make256_u64<Vec16xU16>(uint64_t(0xFFu) << (A_Shift * 2));

  Vec32xU8 predicate;
  if (UseShufB)
    predicate = broadcast_i128<Vec32xU8>(loadu<Vec16xU8>(d.shufbPredicate));

  Vec32xU8 loadStoreMaskLo = loada_64_i8_i32<Vec32xU8>(&bl::commonTable.loadstore16_lo8_msk8()[w & 15]);
  Vec32xU8 loadStoreMaskHi = loada_64_i8_i32<Vec32xU8>(&bl::commonTable.loadstore16_hi8_msk8()[w & 15]);

  // Alpha byte-index that can be used by instructions that perform shuffling.
  constexpr uint32_t AI = A_Shift / 8u;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 16) {
      Vec32xU8 packed0 = loadu<Vec32xU8>(srcData +  0);
      Vec32xU8 packed1 = loadu<Vec32xU8>(srcData + 32);

      if (UseShufB) {
        packed0 = swizzlev_u8(packed0, predicate);
        packed1 = swizzlev_u8(packed1, predicate);
      }

      Vec16xU16 p1 = vec_u16(interleave_hi_u8(packed0, zero));
      Vec16xU16 p0 = vec_u16(interleave_lo_u8(packed0, zero));
      Vec16xU16 p3 = vec_u16(interleave_hi_u8(packed1, zero));
      Vec16xU16 p2 = vec_u16(interleave_lo_u8(packed1, zero));

      p0 = div255_u16((p0 | alphaMask) * swizzle_u16<AI, AI, AI, AI>(p0));
      p1 = div255_u16((p1 | alphaMask) * swizzle_u16<AI, AI, AI, AI>(p1));
      p2 = div255_u16((p2 | alphaMask) * swizzle_u16<AI, AI, AI, AI>(p2));
      p3 = div255_u16((p3 | alphaMask) * swizzle_u16<AI, AI, AI, AI>(p3));

      storeu(dstData +  0, vec_u8(packs_128_i16_u8(p0, p1)) | fillMask);
      storeu(dstData + 32, vec_u8(packs_128_i16_u8(p2, p3)) | fillMask);

      dstData += 64;
      srcData += 64;
      i -= 16;
    }

    if (i) {
      Vec32xU8 packed0 = loadu_256_mask32<Vec32xU8>(srcData +  0, loadStoreMaskLo);
      Vec32xU8 packed1 = loadu_256_mask32<Vec32xU8>(srcData + 32, loadStoreMaskHi);

      if (UseShufB) {
        packed0 = swizzlev_u8(packed0, predicate);
        packed1 = swizzlev_u8(packed1, predicate);
      }

      Vec16xU16 p1 = vec_u16(interleave_hi_u8(packed0, zero));
      Vec16xU16 p0 = vec_u16(interleave_lo_u8(packed0, zero));
      Vec16xU16 p3 = vec_u16(interleave_hi_u8(packed1, zero));
      Vec16xU16 p2 = vec_u16(interleave_lo_u8(packed1, zero));

      p0 = div255_u16((p0 | alphaMask) * swizzle_u16<AI, AI, AI, AI>(p0));
      p1 = div255_u16((p1 | alphaMask) * swizzle_u16<AI, AI, AI, AI>(p1));
      p2 = div255_u16((p2 | alphaMask) * swizzle_u16<AI, AI, AI, AI>(p2));
      p3 = div255_u16((p3 | alphaMask) * swizzle_u16<AI, AI, AI, AI>(p3));

      storeu_256_mask32(dstData +  0, vec_u8(packs_128_i16_u8(p0, p1)) | fillMask, loadStoreMaskLo);
      storeu_256_mask32(dstData + 32, vec_u8(packs_128_i16_u8(p2, p3)) | fillMask, loadStoreMaskHi);

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

  using namespace SIMD;

  blUnused(self);

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4u + gap;
  srcStride -= uintptr_t(w) * 4u;

  const uint32_t* rcpTable = bl::commonTable.unpremultiplyRcp;

  Vec8xU32 half = make256_u32(0x8000u);
  Vec32xU8 alphaMask = make256_u32<Vec32xU8>(0xFFu << A_Shift);
  Vec8xU32 componentMask = make256_u32<Vec8xU32>(0xFFu);
  Vec32xU8 loadStoreMask = loada_64_i8_i32<Vec32xU8>(bl::commonTable.loadstore16_lo8_msk8() + (w & 7));

  // Alpha byte-index that can be used by instructions that perform shuffling.
  constexpr uint32_t AI = A_Shift / 8u;
  constexpr uint32_t RI = (AI + 1) % 4;
  constexpr uint32_t GI = (AI + 2) % 4;
  constexpr uint32_t BI = (AI + 3) % 4;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 8) {
      Vec32xU8 pix = loadu<Vec32xU8>(srcData);

      Vec4xU32 rcpLo = loada_32<Vec4xU32>(rcpTable + srcData[0 * 4 + AI]);
      Vec4xU32 rcpHi = loada_32<Vec4xU32>(rcpTable + srcData[4 * 4 + AI]);

      rcpLo = insert_m32<1>(rcpLo, rcpTable + srcData[1 * 4 + AI]);
      rcpHi = insert_m32<1>(rcpHi, rcpTable + srcData[5 * 4 + AI]);

      rcpLo = insert_m32<2>(rcpLo, rcpTable + srcData[2 * 4 + AI]);
      rcpHi = insert_m32<2>(rcpHi, rcpTable + srcData[6 * 4 + AI]);

      rcpLo = insert_m32<3>(rcpLo, rcpTable + srcData[3 * 4 + AI]);
      rcpHi = insert_m32<3>(rcpHi, rcpTable + srcData[7 * 4 + AI]);

      Vec8xU32 rcp = interleave_i128<Vec8xU32>(rcpLo, rcpHi);
      Vec8xU32 pr = vec_u32(srli_u32<RI * 8>(pix));
      Vec8xU32 pg = vec_u32(srli_u32<GI * 8>(pix));
      Vec8xU32 pb = vec_u32(srli_u32<BI * 8>(pix));

      if (RI != 3) pr = pr & componentMask;
      if (GI != 3) pg = pg & componentMask;
      if (BI != 3) pb = pb & componentMask;

      pix = pix & alphaMask;
      pr = slli_i32<RI * 8>(srli_u32<16>(pr * rcp + half));
      pg = slli_i32<GI * 8>(srli_u32<16>(pg * rcp + half));
      pb = slli_i32<BI * 8>(srli_u32<16>(pb * rcp + half));
      pix = pix | vec_u8(pr) | vec_u8(pg) | vec_u8(pb);
      storeu(dstData, pix);

      dstData += 32;
      srcData += 32;
      i -= 8;
    }

    if (i) {
      Vec32xU8 pix = loadu_256_mask32<Vec32xU8>(srcData, loadStoreMask);

      Vec4xU32 rcpLo = loada_32<Vec4xU32>(rcpTable + srcData[0 * 4 + AI]);
      Vec4xU32 rcpHi = loada_32<Vec4xU32>(rcpTable + srcData[4 * 4 + AI]);

      rcpLo = insert_m32<1>(rcpLo, rcpTable + srcData[1 * 4 + AI]);
      rcpHi = insert_m32<1>(rcpHi, rcpTable + srcData[5 * 4 + AI]);

      rcpLo = insert_m32<2>(rcpLo, rcpTable + srcData[2 * 4 + AI]);
      rcpHi = insert_m32<2>(rcpHi, rcpTable + srcData[6 * 4 + AI]);

      rcpLo = insert_m32<3>(rcpLo, rcpTable + srcData[3 * 4 + AI]);
      rcpHi = insert_m32<3>(rcpHi, rcpTable + srcData[7 * 4 + AI]);

      Vec8xU32 rcp = interleave_i128<Vec8xU32>(rcpLo, rcpHi);
      Vec8xU32 pr = vec_u32(srli_u32<RI * 8>(pix));
      Vec8xU32 pg = vec_u32(srli_u32<GI * 8>(pix));
      Vec8xU32 pb = vec_u32(srli_u32<BI * 8>(pix));

      if (RI != 3) pr = pr & componentMask;
      if (GI != 3) pg = pg & componentMask;
      if (BI != 3) pb = pb & componentMask;

      pix = pix & alphaMask;
      pr = slli_i32<RI * 8>(srli_u32<16>(pr * rcp + half));
      pg = slli_i32<GI * 8>(srli_u32<16>(pg * rcp + half));
      pb = slli_i32<BI * 8>(srli_u32<16>(pb * rcp + half));
      pix = pix | vec_u8(pr) | vec_u8(pg) | vec_u8(pb);
      storeu_256_mask32(dstData, pix, loadStoreMask);

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

  using namespace SIMD;

  blUnused(self);

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4u + gap;
  srcStride -= uintptr_t(w) * 4u;

  Vec32xU8 alphaMask = make256_u32<Vec32xU8>(0xFFu << A_Shift);
  Vec8xU32 componentMask = make256_u32(0xFFu);
  Vec32xU8 loadStoreMask = loada_64_i8_i32<Vec32xU8>(bl::commonTable.loadstore16_lo8_msk8() + (w & 7u));

  Vec8xF32 f32_255 = make256_f32(255.0001f);
  Vec8xF32 f32_lessThanOne = make256_f32(0.1f);

  // Alpha byte-index that can be used by instructions that perform shuffling.
  constexpr uint32_t AI = A_Shift / 8u;
  constexpr uint32_t RI = (AI + 1) % 4;
  constexpr uint32_t GI = (AI + 2) % 4;
  constexpr uint32_t BI = (AI + 3) % 4;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 8) {
      Vec32xU8 pix = loadu<Vec32xU8>(srcData);
      Vec8xU32 pa = vec_u32(srli_u32<AI * 8>(pix));

      if (AI != 3) pa = pa & componentMask;

      Vec8xF32 fa = cvt_i32_f32(pa);
      fa = f32_255 / max(fa, f32_lessThanOne);

      Vec8xU32 pr = vec_u32(srli_u32<RI * 8>(pix));
      Vec8xU32 pg = vec_u32(srli_u32<GI * 8>(pix));
      Vec8xU32 pb = vec_u32(srli_u32<BI * 8>(pix));

      if (RI != 3) pr = pr & componentMask;
      if (GI != 3) pg = pg & componentMask;
      if (BI != 3) pb = pb & componentMask;

      pr = vec_u32(cvt_f32_i32(cvt_i32_f32(pr) * fa));
      pg = vec_u32(cvt_f32_i32(cvt_i32_f32(pg) * fa));
      pb = vec_u32(cvt_f32_i32(cvt_i32_f32(pb) * fa));
      pix = pix & alphaMask;

      pr = slli_i32<RI * 8>(pr);
      pg = slli_i32<GI * 8>(pg);
      pb = slli_i32<BI * 8>(pb);
      pix = pix | vec_u8(pr) | vec_u8(pg) | vec_u8(pb);

      storeu(dstData, pix);

      dstData += 32;
      srcData += 32;
      i -= 8;
    }

    if (i) {
      Vec32xU8 pix = loadu_256_mask32<Vec32xU8>(srcData, loadStoreMask);
      Vec8xU32 pa = vec_u32(srli_u32<AI * 8>(pix));

      if (AI != 3) pa = pa & componentMask;

      Vec8xF32 fa = cvt_i32_f32(pa);
      fa = f32_255 / max(fa, f32_lessThanOne);

      Vec8xU32 pr = vec_u32(srli_u32<RI * 8>(pix));
      Vec8xU32 pg = vec_u32(srli_u32<GI * 8>(pix));
      Vec8xU32 pb = vec_u32(srli_u32<BI * 8>(pix));

      if (RI != 3) pr = pr & componentMask;
      if (GI != 3) pg = pg & componentMask;
      if (BI != 3) pb = pb & componentMask;

      pr = vec_u32(cvt_f32_i32(cvt_i32_f32(pr) * fa));
      pg = vec_u32(cvt_f32_i32(cvt_i32_f32(pg) * fa));
      pb = vec_u32(cvt_f32_i32(cvt_i32_f32(pb) * fa));
      pix = pix & alphaMask;

      pr = slli_i32<RI * 8>(pr);
      pg = slli_i32<GI * 8>(pg);
      pb = slli_i32<BI * 8>(pb);
      pix = pix | vec_u8(pr) | vec_u8(pg) | vec_u8(pb);

      storeu_256_mask32(dstData, pix, loadStoreMask);

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
