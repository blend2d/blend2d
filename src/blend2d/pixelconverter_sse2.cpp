// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "./api-build_p.h"
#ifdef BL_BUILD_OPT_SSE2

#include "./pixelconverter_p.h"
#include "./simd_p.h"
#include "./support_p.h"

using namespace SIMD;

// ============================================================================
// [BLPixelConverter - Copy (SSE2)]
// ============================================================================

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
  dstStride -= uintptr_t(byteWidth) + intptr_t(gap);
  srcStride -= uintptr_t(byteWidth);

  for (uint32_t y = h; y != 0; y--) {
    size_t i = byteWidth;
    size_t alignment = 16 - (uintptr_t(dstData) & 0xFu);

    vstorei128u(dstData, vloadi128u(srcData));

    i -= alignment;
    dstData += alignment;
    srcData += alignment;

    while_nounroll (i >= 64) {
      I128 p0 = vloadi128u(srcData +  0);
      I128 p1 = vloadi128u(srcData + 16);
      I128 p2 = vloadi128u(srcData + 32);
      I128 p3 = vloadi128u(srcData + 48);

      vstorei128a(dstData +  0, p0);
      vstorei128a(dstData + 16, p1);
      vstorei128a(dstData + 32, p2);
      vstorei128a(dstData + 48, p3);

      dstData += 64;
      srcData += 64;
      i -= 64;
    }

    while_nounroll (i >= 16) {
      vstorei128a(dstData, vloadi128u(srcData));

      dstData += 16;
      srcData += 16;
      i -= 16;
    }

    if (i) {
      dstData += i;
      srcData += i;
      vstorei128u(dstData - 16, vloadi128u(srcData - 16));
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLPixelConverter - Copy|Or (SSE2)]
// ============================================================================

BLResult bl_convert_copy_or_8888_sse2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  I128 fillMask = vseti128u32(blPixelConverterGetData(self)->memCopyData.fillMask);

  dstStride -= uintptr_t(w) * 4u + gap;
  srcStride -= uintptr_t(w) * 4u;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while_nounroll (i >= 16) {
      I128 p0 = vloadi128u(srcData +  0);
      I128 p1 = vloadi128u(srcData + 16);
      I128 p2 = vloadi128u(srcData + 32);
      I128 p3 = vloadi128u(srcData + 48);

      vstorei128u(dstData +  0, vor(p0, fillMask));
      vstorei128u(dstData + 16, vor(p1, fillMask));
      vstorei128u(dstData + 32, vor(p2, fillMask));
      vstorei128u(dstData + 48, vor(p3, fillMask));

      dstData += 64;
      srcData += 64;
      i -= 16;
    }

    while_nounroll (i >= 4) {
      I128 p0 = vloadi128u(srcData);
      vstorei128u(dstData, vor(p0, fillMask));

      dstData += 16;
      srcData += 16;
      i -= 4;
    }

    while_nounroll (i) {
      I128 p0 = vloadi128_32(srcData);
      vstorei32(dstData, vor(p0, fillMask));

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

// ============================================================================
// [BLPixelConverter - Premultiply (SSE2)]
// ============================================================================

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
  I128 zero = vzeroi128();
  I128 a255 = vseti128u64(uint64_t(0xFFu) << (A_Shift * 2));
  I128 fillMask = vseti128u32(d.fillMask);

  // Alpha byte-index that can be used by instructions that perform shuffling.
  constexpr uint32_t AI = A_Shift / 8u;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while_nounroll (i >= 4) {
      I128 p0, p1;

      p0 = vloadi128u(srcData);
      p1 = vunpackhi8(p0, zero);
      p0 = vunpackli8(p0, zero);

      p1 = vdiv255u16(vmuli16(vor(p1, a255), vswizi16<AI, AI, AI, AI>(p1)));
      p0 = vdiv255u16(vmuli16(vor(p0, a255), vswizi16<AI, AI, AI, AI>(p0)));
      p0 = vpacki16u8(p0, p1);
      p0 = vor(p0, fillMask);
      vstorei128u(dstData, p0);

      dstData += 16;
      srcData += 16;
      i -= 4;
    }

    while_nounroll (i) {
      I128 p0;

      p0 = vloadi128_32(srcData);
      p0 = vunpackli8(p0, zero);
      p0 = vmuli16(vor(p0, a255), vswizi16<AI, AI, AI, AI>(p0));
      p0 = vdiv255u16(p0);
      p0 = vpacki16u8(p0, p0);
      p0 = vor(p0, fillMask);
      vstorei32(dstData, p0);

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

// ============================================================================
// [BLPixelConverter - Unpremultiply (SSE2)]
// ============================================================================

template<uint32_t A_Shift>
static BL_INLINE BLResult bl_convert_unpremultiply_8888_template_sse2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  BL_UNUSED(self);

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4u + gap;
  srcStride -= uintptr_t(w) * 4u;

  const uint32_t* rcpTable = blCommonTable.unpremultiplyPmaddwdRcp;
  const uint32_t* rndTable = blCommonTable.unpremultiplyPmaddwdRnd;

  I128 alphaMask = vseti128u32(0xFFu << A_Shift);
  I128 componentMask = vseti128u32(0xFFu);

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

    while_nounroll (i >= 4) {
      I128 pix = vloadi128u(srcData);
      size_t idx0 = srcData[0 * 4 + AI];
      size_t idx1 = srcData[1 * 4 + AI];

      I128 rcp0 = vloadi128_32(rcpTable + idx0);
      I128 rcp1 = vloadi128_32(rcpTable + idx1);
      I128 rnd0 = vloadi128_32(rndTable + idx0);
      I128 rnd1 = vloadi128_32(rndTable + idx1);

      rcp0 = vunpackli32(rcp0, rcp1);
      rnd0 = vunpackli32(rnd0, rnd1);
      size_t idx2 = srcData[2 * 4 + AI];
      size_t idx3 = srcData[3 * 4 + AI];

      I128 rcp2 = vloadi128_32(rcpTable + idx2);
      I128 rcp3 = vloadi128_32(rcpTable + idx3);
      I128 rnd2 = vloadi128_32(rndTable + idx2);
      I128 rnd3 = vloadi128_32(rndTable + idx3);

      rcp2 = vunpackli32(rcp2, rcp3);
      rnd2 = vunpackli32(rnd2, rnd3);
      rcp0 = vunpackli64(rcp0, rcp2);
      rnd0 = vunpackli64(rnd0, rnd2);

      I128 pr = vsrli32<RI * 8>(pix);
      I128 pg = vsrli32<GI * 8>(pix);
      I128 pb = vsrli32<BI * 8>(pix);

      if (RI != 3) pr = vand(pr, componentMask);
      if (GI != 3) pg = vand(pg, componentMask);
      if (BI != 3) pb = vand(pb, componentMask);

      pr = vor(pr, vslli32<16 + 6>(pr));
      pg = vor(pg, vslli32<16 + 6>(pg));
      pb = vor(pb, vslli32<16 + 6>(pb));

      pr = vmaddi16i32(pr, rcp0);
      pg = vmaddi16i32(pg, rcp0);
      pb = vmaddi16i32(pb, rcp0);

      pix = vand(pix, alphaMask);
      pr = vslli32<RI * 8>(vsrli32<13>(vaddi32(pr, rnd0)));
      pg = vslli32<GI * 8>(vsrli32<13>(vaddi32(pg, rnd0)));
      pb = vslli32<BI * 8>(vsrli32<13>(vaddi32(pb, rnd0)));

      pix = vor(pix, pr);
      pix = vor(pix, pg);
      pix = vor(pix, pb);
      vstorei128u(dstData, pix);

      dstData += 16;
      srcData += 16;
      i -= 4;
    }

    I128 zero = vzeroi128();
    while_nounroll (i) {
      I128 pix = vloadi128_32(srcData);
      size_t idx0 = srcData[AI];

      I128 p0;
      p0 = vunpackli8(pix, zero);
      p0 = vunpackli16(p0, zero);
      p0 = vor(p0, vslli32<16 + 6>(p0));

      I128 rcp0 = vswizi32<D, C, B, A>(vloadi128_32(rcpTable + idx0));
      I128 rnd0 = vswizi32<D, C, B, A>(vloadi128_32(rndTable + idx0));

      pix = vand(pix, alphaMask);
      p0 = vmaddi16i32(p0, rcp0);
      p0 = vsrli32<13>(vaddi32(p0, rnd0));

      p0 = vpacki32i16(p0);
      p0 = vpacki16u8(p0);
      p0 = vor(p0, pix);
      vstorei32(dstData, p0);

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

// ============================================================================
// [BLPixelConverter - RGB32 From A8/L8 (SSE2)]
// ============================================================================

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

  I128 fillMask = vseti128u32(fillMask32);
  I128 zeroMask = vseti128u32(zeroMask32);

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while_nounroll (i >= 16) {
      I128 p0, p1, p2, p3;

      p0 = vloadi128u(srcData);

      p2 = vunpackhi8(p0, p0);
      p0 = vunpackli8(p0, p0);

      p1 = vunpackhi16(p0, p0);
      p0 = vunpackli16(p0, p0);
      p3 = vunpackhi16(p2, p2);
      p2 = vunpackli16(p2, p2);

      p0 = vand(p0, zeroMask);
      p1 = vand(p1, zeroMask);
      p2 = vand(p2, zeroMask);
      p3 = vand(p3, zeroMask);

      p0 = vor(p0, fillMask);
      p1 = vor(p1, fillMask);
      p2 = vor(p2, fillMask);
      p3 = vor(p3, fillMask);

      vstorei128u(dstData +  0, p0);
      vstorei128u(dstData + 16, p1);
      vstorei128u(dstData + 32, p2);
      vstorei128u(dstData + 48, p3);

      dstData += 64;
      srcData += 16;
      i -= 16;
    }

    while_nounroll (i >= 4) {
      I128 p0 = vloadi128_32(srcData);

      p0 = vunpackli8(p0, p0);
      p0 = vunpackli16(p0, p0);
      p0 = vand(p0, zeroMask);
      p0 = vor(p0, fillMask);

      vstorei128u(dstData, p0);

      dstData += 16;
      srcData += 4;
      i -= 4;
    }

    while_nounroll (i) {
      blMemWriteU32u(dstData, ((uint32_t(srcData[0]) * 0x01010101u) & zeroMask32) | fillMask32);
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
