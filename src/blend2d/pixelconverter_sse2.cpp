// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#ifdef BL_BUILD_OPT_SSE2

#include "pixelconverter_p.h"
#include "simd_p.h"
#include "support/memops_p.h"

using namespace SIMD;

// PixelConverter - Copy (SSE2)
// ============================

BLResult bl_convert_copy_sse2(
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
  dstStride -= uintptr_t(byteWidth) + uintptr_t(gap);
  srcStride -= uintptr_t(byteWidth);

  for (uint32_t y = h; y != 0; y--) {
    size_t i = byteWidth;
    size_t alignment = 16 - (uintptr_t(dstData) & 0xFu);

    v_storeu_i128(dstData, v_loadu_i128(srcData));

    i -= alignment;
    dstData += alignment;
    srcData += alignment;

    BL_NOUNROLL
    while (i >= 64) {
      Vec128I p0 = v_loadu_i128(srcData +  0);
      Vec128I p1 = v_loadu_i128(srcData + 16);
      Vec128I p2 = v_loadu_i128(srcData + 32);
      Vec128I p3 = v_loadu_i128(srcData + 48);

      v_storea_i128(dstData +  0, p0);
      v_storea_i128(dstData + 16, p1);
      v_storea_i128(dstData + 32, p2);
      v_storea_i128(dstData + 48, p3);

      dstData += 64;
      srcData += 64;
      i -= 64;
    }

    BL_NOUNROLL
    while (i >= 16) {
      v_storea_i128(dstData, v_loadu_i128(srcData));

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

// PixelConverter - Copy|Or (SSE2)
// ===============================

BLResult bl_convert_copy_or_8888_sse2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  Vec128I fillMask = v_fill_i128_u32(blPixelConverterGetData(self)->memCopyData.fillMask);

  dstStride -= uintptr_t(w) * 4u + gap;
  srcStride -= uintptr_t(w) * 4u;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 16) {
      Vec128I p0 = v_loadu_i128(srcData +  0);
      Vec128I p1 = v_loadu_i128(srcData + 16);
      Vec128I p2 = v_loadu_i128(srcData + 32);
      Vec128I p3 = v_loadu_i128(srcData + 48);

      v_storeu_i128(dstData +  0, v_or(p0, fillMask));
      v_storeu_i128(dstData + 16, v_or(p1, fillMask));
      v_storeu_i128(dstData + 32, v_or(p2, fillMask));
      v_storeu_i128(dstData + 48, v_or(p3, fillMask));

      dstData += 64;
      srcData += 64;
      i -= 16;
    }

    BL_NOUNROLL
    while (i >= 4) {
      Vec128I p0 = v_loadu_i128(srcData);
      v_storeu_i128(dstData, v_or(p0, fillMask));

      dstData += 16;
      srcData += 16;
      i -= 4;
    }

    BL_NOUNROLL
    while (i) {
      Vec128I p0 = v_load_i32(srcData);
      v_store_i32(dstData, v_or(p0, fillMask));

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

// PixelConverter - Premultiply (SSE2)
// ===================================

template<uint32_t A_Shift>
static BL_INLINE BLResult bl_convert_premultiply_8888_template_sse2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4u + gap;
  srcStride -= uintptr_t(w) * 4u;

  const BLPixelConverterData::PremultiplyData& d = blPixelConverterGetData(self)->premultiplyData;
  Vec128I zero = v_zero_i128();
  Vec128I a255 = v_fill_i128_u64(uint64_t(0xFFu) << (A_Shift * 2));
  Vec128I fillMask = v_fill_i128_u32(d.fillMask);

  // Alpha byte-index that can be used by instructions that perform shuffling.
  constexpr uint32_t AI = A_Shift / 8u;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 4) {
      Vec128I p0, p1;

      p0 = v_loadu_i128(srcData);
      p1 = v_interleave_hi_i8(p0, zero);
      p0 = v_interleave_lo_i8(p0, zero);

      p1 = v_div255_u16(v_mul_i16(v_or(p1, a255), v_swizzle_i16<AI, AI, AI, AI>(p1)));
      p0 = v_div255_u16(v_mul_i16(v_or(p0, a255), v_swizzle_i16<AI, AI, AI, AI>(p0)));
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

BLResult bl_convert_premultiply_8888_leading_alpha_sse2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return bl_convert_premultiply_8888_template_sse2<24>(self, dstData, dstStride, srcData, srcStride, w, h, options);
}

BLResult bl_convert_premultiply_8888_trailing_alpha_sse2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return bl_convert_premultiply_8888_template_sse2<0>(self, dstData, dstStride, srcData, srcStride, w, h, options);
}

// PixelConverter - Unpremultiply (SSE2)
// =====================================

template<uint32_t A_Shift>
static BL_INLINE BLResult bl_convert_unpremultiply_8888_template_sse2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  blUnused(self);

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4u + gap;
  srcStride -= uintptr_t(w) * 4u;

  const uint32_t* rcpTable = blCommonTable.unpremultiplyPmaddwdRcp;
  const uint32_t* rndTable = blCommonTable.unpremultiplyPmaddwdRnd;

  Vec128I alphaMask = v_fill_i128_u32(0xFFu << A_Shift);
  Vec128I componentMask = v_fill_i128_u32(0xFFu);

  // Alpha byte-index that can be used by instructions that perform shuffling.
  constexpr uint32_t AI = A_Shift / 8u;
  constexpr uint32_t RI = (AI + 1) % 4;
  constexpr uint32_t GI = (AI + 2) % 4;
  constexpr uint32_t BI = (AI + 3) % 4;

  constexpr uint32_t A = AI == 0 ? 3 : 0;
  constexpr uint32_t B = AI == 1 ? 3 : 0;
  constexpr uint32_t C = AI == 2 ? 3 : 0;
  constexpr uint32_t D = AI == 3 ? 3 : 0;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 4) {
      Vec128I pix = v_loadu_i128(srcData);
      size_t idx0 = srcData[0 * 4 + AI];
      size_t idx1 = srcData[1 * 4 + AI];

      Vec128I rcp0 = v_load_i32(rcpTable + idx0);
      Vec128I rcp1 = v_load_i32(rcpTable + idx1);
      Vec128I rnd0 = v_load_i32(rndTable + idx0);
      Vec128I rnd1 = v_load_i32(rndTable + idx1);

      rcp0 = v_interleave_lo_i32(rcp0, rcp1);
      rnd0 = v_interleave_lo_i32(rnd0, rnd1);
      size_t idx2 = srcData[2 * 4 + AI];
      size_t idx3 = srcData[3 * 4 + AI];

      Vec128I rcp2 = v_load_i32(rcpTable + idx2);
      Vec128I rcp3 = v_load_i32(rcpTable + idx3);
      Vec128I rnd2 = v_load_i32(rndTable + idx2);
      Vec128I rnd3 = v_load_i32(rndTable + idx3);

      rcp2 = v_interleave_lo_i32(rcp2, rcp3);
      rnd2 = v_interleave_lo_i32(rnd2, rnd3);
      rcp0 = v_interleave_lo_i64(rcp0, rcp2);
      rnd0 = v_interleave_lo_i64(rnd0, rnd2);

      Vec128I pr = v_srl_i32<RI * 8>(pix);
      Vec128I pg = v_srl_i32<GI * 8>(pix);
      Vec128I pb = v_srl_i32<BI * 8>(pix);

      if (RI != 3) pr = v_and(pr, componentMask);
      if (GI != 3) pg = v_and(pg, componentMask);
      if (BI != 3) pb = v_and(pb, componentMask);

      pr = v_or(pr, v_sll_i32<16 + 6>(pr));
      pg = v_or(pg, v_sll_i32<16 + 6>(pg));
      pb = v_or(pb, v_sll_i32<16 + 6>(pb));

      pr = v_madd_i16_i32(pr, rcp0);
      pg = v_madd_i16_i32(pg, rcp0);
      pb = v_madd_i16_i32(pb, rcp0);

      pix = v_and(pix, alphaMask);
      pr = v_sll_i32<RI * 8>(v_srl_i32<13>(v_add_i32(pr, rnd0)));
      pg = v_sll_i32<GI * 8>(v_srl_i32<13>(v_add_i32(pg, rnd0)));
      pb = v_sll_i32<BI * 8>(v_srl_i32<13>(v_add_i32(pb, rnd0)));

      pix = v_or(pix, pr);
      pix = v_or(pix, pg);
      pix = v_or(pix, pb);
      v_storeu_i128(dstData, pix);

      dstData += 16;
      srcData += 16;
      i -= 4;
    }

    Vec128I zero = v_zero_i128();

    BL_NOUNROLL
    while (i) {
      Vec128I pix = v_load_i32(srcData);
      size_t idx0 = srcData[AI];

      Vec128I p0;
      p0 = v_interleave_lo_i8(pix, zero);
      p0 = v_interleave_lo_i16(p0, zero);
      p0 = v_or(p0, v_sll_i32<16 + 6>(p0));

      Vec128I rcp0 = v_swizzle_i32<D, C, B, A>(v_load_i32(rcpTable + idx0));
      Vec128I rnd0 = v_swizzle_i32<D, C, B, A>(v_load_i32(rndTable + idx0));

      pix = v_and(pix, alphaMask);
      p0 = v_madd_i16_i32(p0, rcp0);
      p0 = v_srl_i32<13>(v_add_i32(p0, rnd0));

      p0 = v_packs_i32_i16(p0);
      p0 = v_packs_i16_u8(p0);
      p0 = v_or(p0, pix);
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

BLResult bl_convert_unpremultiply_8888_leading_alpha_sse2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return bl_convert_unpremultiply_8888_template_sse2<24>(self, dstData, dstStride, srcData, srcStride, w, h, options);
}

BLResult bl_convert_unpremultiply_8888_trailing_alpha_sse2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return bl_convert_unpremultiply_8888_template_sse2<0>(self, dstData, dstStride, srcData, srcStride, w, h, options);
}

// BLPixelConverter - RGB32 From A8/L8 (SSE2)
// ==========================================

BLResult bl_convert_8888_from_x8_sse2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w);

  const BLPixelConverterData::Rgb32FromX8Data& d = blPixelConverterGetData(self)->rgb32FromX8Data;
  uint32_t fillMask32 = d.fillMask;
  uint32_t zeroMask32 = d.zeroMask;

  Vec128I fillMask = v_fill_i128_u32(fillMask32);
  Vec128I zeroMask = v_fill_i128_u32(zeroMask32);

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 16) {
      Vec128I p0, p1, p2, p3;

      p0 = v_loadu_i128(srcData);

      p2 = v_interleave_hi_i8(p0, p0);
      p0 = v_interleave_lo_i8(p0, p0);

      p1 = v_interleave_hi_i16(p0, p0);
      p0 = v_interleave_lo_i16(p0, p0);
      p3 = v_interleave_hi_i16(p2, p2);
      p2 = v_interleave_lo_i16(p2, p2);

      p0 = v_and(p0, zeroMask);
      p1 = v_and(p1, zeroMask);
      p2 = v_and(p2, zeroMask);
      p3 = v_and(p3, zeroMask);

      p0 = v_or(p0, fillMask);
      p1 = v_or(p1, fillMask);
      p2 = v_or(p2, fillMask);
      p3 = v_or(p3, fillMask);

      v_storeu_i128(dstData +  0, p0);
      v_storeu_i128(dstData + 16, p1);
      v_storeu_i128(dstData + 32, p2);
      v_storeu_i128(dstData + 48, p3);

      dstData += 64;
      srcData += 16;
      i -= 16;
    }

    BL_NOUNROLL
    while (i >= 4) {
      Vec128I p0 = v_load_i32(srcData);

      p0 = v_interleave_lo_i8(p0, p0);
      p0 = v_interleave_lo_i16(p0, p0);
      p0 = v_and(p0, zeroMask);
      p0 = v_or(p0, fillMask);

      v_storeu_i128(dstData, p0);

      dstData += 16;
      srcData += 4;
      i -= 4;
    }

    BL_NOUNROLL
    while (i) {
      BLMemOps::writeU32u(dstData, ((uint32_t(srcData[0]) * 0x01010101u) & zeroMask32) | fillMask32);
      dstData += 4;
      srcData += 1;
      i--;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

#endif
