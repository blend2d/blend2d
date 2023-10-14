// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#ifdef BL_BUILD_OPT_SSE2

#include "pixelconverter_p.h"
#include "simd/simd_p.h"
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

    storeu(dstData, loadu<Vec16xU8>(srcData));

    i -= alignment;
    dstData += alignment;
    srcData += alignment;

    BL_NOUNROLL
    while (i >= 64) {
      Vec16xU8 p0 = loadu<Vec16xU8>(srcData +  0);
      Vec16xU8 p1 = loadu<Vec16xU8>(srcData + 16);
      storea(dstData +  0, p0);
      storea(dstData + 16, p1);

      Vec16xU8 p2 = loadu<Vec16xU8>(srcData + 32);
      Vec16xU8 p3 = loadu<Vec16xU8>(srcData + 48);
      storea(dstData + 32, p2);
      storea(dstData + 48, p3);

      dstData += 64;
      srcData += 64;
      i -= 64;
    }

    BL_NOUNROLL
    while (i >= 16) {
      storea(dstData, loadu<Vec16xU8>(srcData));

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

// PixelConverter - Copy|Or (SSE2)
// ===============================

BLResult bl_convert_copy_or_8888_sse2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  Vec16xU8 fillMask = make128_u32<Vec16xU8>(blPixelConverterGetData(self)->memCopyData.fillMask);

  dstStride -= uintptr_t(w) * 4u + gap;
  srcStride -= uintptr_t(w) * 4u;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 16) {
      Vec16xU8 p0 = loadu<Vec16xU8>(srcData +  0);
      Vec16xU8 p1 = loadu<Vec16xU8>(srcData + 16);
      storeu(dstData +  0, p0 | fillMask);
      storeu(dstData + 16, p1 | fillMask);

      Vec16xU8 p2 = loadu<Vec16xU8>(srcData + 32);
      Vec16xU8 p3 = loadu<Vec16xU8>(srcData + 48);
      storeu(dstData + 32, p2 | fillMask);
      storeu(dstData + 48, p3 | fillMask);

      dstData += 64;
      srcData += 64;
      i -= 16;
    }

    BL_NOUNROLL
    while (i >= 4) {
      Vec16xU8 p0 = loadu<Vec16xU8>(srcData);
      storeu(dstData, p0 | fillMask);

      dstData += 16;
      srcData += 16;
      i -= 4;
    }

    BL_NOUNROLL
    while (i) {
      Vec16xU8 p0 = loadu_32<Vec16xU8>(srcData);
      storeu_32(dstData, p0 | fillMask);

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
  Vec16xU8 fillMask = make128_u32<Vec16xU8>(d.fillMask);
  Vec8xU16 alphaMask = make128_u64<Vec8xU16>(uint64_t(0xFFu) << (A_Shift * 2));

  // Alpha byte-index that can be used by instructions that perform shuffling.
  constexpr uint32_t AI = A_Shift / 8u;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 4) {
      Vec16xU8 packed = loadu<Vec16xU8>(srcData);
      Vec8xU16 p1 = vec_u16(unpack_hi64_u8_u16(packed));
      Vec8xU16 p0 = vec_u16(unpack_lo64_u8_u16(packed));

      p1 = div255_u16((p1 | alphaMask) * swizzle_u16<AI, AI, AI, AI>(p1));
      p0 = div255_u16((p0 | alphaMask) * swizzle_u16<AI, AI, AI, AI>(p0));
      storeu(dstData, vec_u8(packs_128_i16_u8(p0, p1)) | fillMask);

      dstData += 16;
      srcData += 16;
      i -= 4;
    }

    BL_NOUNROLL
    while (i) {
      Vec16xU8 packed = loadu_32<Vec16xU8>(srcData);
      Vec8xU16 p0 = vec_u16(unpack_lo64_u8_u16(packed));

      p0 = div255_u16((p0 | alphaMask) * swizzle_u16<AI, AI, AI, AI>(p0));
      storeu_32(dstData, vec_u8(packs_128_i16_u8(p0)) | fillMask);

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

  const uint32_t* rcpTable = bl::commonTable.unpremultiplyPmaddwdRcp;
  const uint32_t* rndTable = bl::commonTable.unpremultiplyPmaddwdRnd;

  Vec16xU8 alphaMask = make128_u32<Vec16xU8>(0xFFu << A_Shift);
  Vec4xU32 componentMask = make128_u32<Vec4xU32>(0xFFu);

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
      size_t idx0 = srcData[0 * 4 + AI];
      size_t idx1 = srcData[1 * 4 + AI];
      Vec16xU8 pix = loadu<Vec16xU8>(srcData);

      Vec4xU32 rcp0 = loada_32<Vec4xU32>(rcpTable + idx0);
      Vec4xU32 rcp1 = loada_32<Vec4xU32>(rcpTable + idx1);
      Vec4xU32 rnd0 = loada_32<Vec4xU32>(rndTable + idx0);
      Vec4xU32 rnd1 = loada_32<Vec4xU32>(rndTable + idx1);

      size_t idx2 = srcData[2 * 4 + AI];
      size_t idx3 = srcData[3 * 4 + AI];
      rcp0 = interleave_lo_u32(rcp0, rcp1);
      rnd0 = interleave_lo_u32(rnd0, rnd1);

      Vec4xU32 rcp2 = loada_32<Vec4xU32>(rcpTable + idx2);
      Vec4xU32 rcp3 = loada_32<Vec4xU32>(rcpTable + idx3);
      Vec4xU32 rnd2 = loada_32<Vec4xU32>(rndTable + idx2);
      Vec4xU32 rnd3 = loada_32<Vec4xU32>(rndTable + idx3);

      rcp2 = interleave_lo_u32(rcp2, rcp3);
      rnd2 = interleave_lo_u32(rnd2, rnd3);
      rcp0 = interleave_lo_u64(rcp0, rcp2);
      rnd0 = interleave_lo_u64(rnd0, rnd2);

      Vec4xU32 pr = vec_u32(srli_u32<RI * 8>(pix));
      Vec4xU32 pg = vec_u32(srli_u32<GI * 8>(pix));
      Vec4xU32 pb = vec_u32(srli_u32<BI * 8>(pix));

      if (RI != 3) pr &= componentMask;
      if (GI != 3) pg &= componentMask;
      if (BI != 3) pb &= componentMask;

      pr = maddw_i16_i32(pr | slli_i32<16 + 6>(pr), rcp0);
      pg = maddw_i16_i32(pg | slli_i32<16 + 6>(pg), rcp0);
      pb = maddw_i16_i32(pb | slli_i32<16 + 6>(pb), rcp0);
      pix = pix & alphaMask;

      pr = slli_i32<RI * 8>(srli_u32<13>(pr + rnd0));
      pg = slli_i32<GI * 8>(srli_u32<13>(pg + rnd0));
      pb = slli_i32<BI * 8>(srli_u32<13>(pb + rnd0));
      storeu(dstData, pix | vec_u8(pr) | vec_u8(pg) | vec_u8(pb));

      dstData += 16;
      srcData += 16;
      i -= 4;
    }

    BL_NOUNROLL
    while (i) {
      size_t idx0 = srcData[AI];
      Vec16xU8 pix = loadu_32<Vec16xU8>(srcData);

      Vec4xU32 p0 = vec_u32(unpack_lo32_u8_u32(pix));
      Vec4xU32 rcp0 = swizzle_u32<D, C, B, A>(loada_32<Vec4xU32>(rcpTable + idx0));
      Vec4xU32 rnd0 = swizzle_u32<D, C, B, A>(loada_32<Vec4xU32>(rndTable + idx0));

      p0 = p0 | slli_i32<16 + 6>(p0);
      pix = pix & alphaMask;

      p0 = maddw_i16_i32(p0, rcp0);
      p0 = srli_u32<13>(p0 + rnd0);
      storeu_32(dstData, vec_u8(packs_128_i32_u8(p0)) | pix);

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

// bl::PixelConverter - RGB32 From A8/L8 (SSE2)
// ============================================

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

  Vec16xU8 fillMask = make128_u32<Vec16xU8>(fillMask32);
  Vec16xU8 zeroMask = make128_u32<Vec16xU8>(zeroMask32);

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    BL_NOUNROLL
    while (i >= 16) {
      Vec16xU8 p0, p1, p2, p3;

      p0 = loadu<Vec16xU8>(srcData);
      p2 = interleave_hi_u8(p0, p0);
      p0 = interleave_lo_u8(p0, p0);

      p1 = interleave_hi_u16(p0, p0);
      p0 = interleave_lo_u16(p0, p0);
      p3 = interleave_hi_u16(p2, p2);
      p2 = interleave_lo_u16(p2, p2);

      storeu(dstData +  0, (p0 & zeroMask) | fillMask);
      storeu(dstData + 16, (p1 & zeroMask) | fillMask);
      storeu(dstData + 32, (p2 & zeroMask) | fillMask);
      storeu(dstData + 48, (p3 & zeroMask) | fillMask);

      dstData += 64;
      srcData += 16;
      i -= 16;
    }

    BL_NOUNROLL
    while (i >= 4) {
      Vec16xU8 p0 = loadu_32<Vec16xU8>(srcData);
      p0 = interleave_lo_u8(p0, p0);
      p0 = interleave_lo_u16(p0, p0);
      storeu(dstData, (p0 & zeroMask) | fillMask);

      dstData += 16;
      srcData += 4;
      i -= 4;
    }

    BL_NOUNROLL
    while (i) {
      bl::MemOps::writeU32u(dstData, ((uint32_t(srcData[0]) * 0x01010101u) & zeroMask32) | fillMask32);
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
