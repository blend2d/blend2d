// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "./api-build_p.h"
#ifdef BL_BUILD_OPT_AVX2

#include "./pixelconverter_p.h"
#include "./simd_p.h"
#include "./support_p.h"

using namespace SIMD;

// ============================================================================
// [BLPixelConverter - Utilities (AVX2)]
// ============================================================================

static BL_INLINE I128 vpshufb_or(const I128& x, const I128& predicate, const I128& or_mask) noexcept {
  return vor(vpshufb(x, predicate), or_mask);
}

static BL_INLINE I256 vpshufb_or(const I256& x, const I256& predicate, const I256& or_mask) noexcept {
  return vor(vpshufb(x, predicate), or_mask);
}

// ============================================================================
// [BLPixelConverter - Copy (AVX2)]
// ============================================================================

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
  dstStride -= intptr_t(byteWidth) + gap;
  srcStride -= intptr_t(byteWidth);

  for (uint32_t y = h; y != 0; y--) {
    size_t i = byteWidth;

    while_nounroll (i >= 64) {
      I256 p0 = vloadi256u(srcData +  0);
      I256 p1 = vloadi256u(srcData + 32);

      vstorei256u(dstData +  0, p0);
      vstorei256u(dstData + 32, p1);

      dstData += 64;
      srcData += 64;
      i -= 64;
    }

    while_nounroll (i >= 16) {
      vstorei128u(dstData, vloadi128u(srcData));

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
// [BLPixelConverter - Copy|Or (AVX2)]
// ============================================================================

BLResult bl_convert_copy_or_8888_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * 4;

  I256 fillMask = vseti256u32(blPixelConverterGetData(self)->memCopyData.fillMask);
  const BLCommonTable::LoadStoreM32* loadStoreM32 = blCommonTable.load_store_m32;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while_nounroll (i >= 32) {
      I256 p0 = vloadi256u(srcData +  0);
      I256 p1 = vloadi256u(srcData + 32);
      I256 p2 = vloadi256u(srcData + 64);
      I256 p3 = vloadi256u(srcData + 96);

      vstorei256u(dstData +  0, vor(p0, fillMask));
      vstorei256u(dstData + 32, vor(p1, fillMask));
      vstorei256u(dstData + 64, vor(p2, fillMask));
      vstorei256u(dstData + 96, vor(p3, fillMask));

      dstData += 128;
      srcData += 128;
      i -= 32;
    }

    while_nounroll (i >= 8) {
      I256 p0 = vloadi256u(srcData);
      vstorei256u(dstData, vor(p0, fillMask));

      dstData += 32;
      srcData += 32;
      i -= 8;
    }

    if (i) {
      I256 msk = loadStoreM32[i].as<I256>();
      I256 p0 = vloadi256_mask32(srcData, msk);
      vstorei256_mask32(dstData, vor(p0, fillMask), msk);

      dstData += i * 4;
      srcData += i * 4;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}
// ============================================================================
// [BLPixelConverter - Copy|Shufb (AVX2)]
// ============================================================================

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
  const BLCommonTable::LoadStoreM32* loadStoreM32 = blCommonTable.load_store_m32;

  I256 fillMask = vseti256u32(d.fillMask);
  I256 predicate = vdupli128(vloadi128u(d.shufbPredicate));

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while_nounroll (i >= 32) {
      I256 p0 = vloadi256u(srcData +  0);
      I256 p1 = vloadi256u(srcData + 32);
      I256 p2 = vloadi256u(srcData + 64);
      I256 p3 = vloadi256u(srcData + 96);

      p0 = vpshufb_or(p0, predicate, fillMask);
      p1 = vpshufb_or(p1, predicate, fillMask);
      p2 = vpshufb_or(p2, predicate, fillMask);
      p3 = vpshufb_or(p3, predicate, fillMask);

      vstorei256u(dstData +  0, p0);
      vstorei256u(dstData + 32, p1);
      vstorei256u(dstData + 64, p2);
      vstorei256u(dstData + 96, p3);

      dstData += 128;
      srcData += 128;
      i -= 32;
    }

    while_nounroll (i >= 8) {
      I256 p0 = vloadi256u(srcData);

      p0 = vpshufb(p0, predicate);
      p0 = vor(p0, fillMask);
      vstorei256u(dstData, p0);

      dstData += 32;
      srcData += 32;
      i -= 8;
    }

    if (i) {
      I256 msk = loadStoreM32[i].as<I256>();
      I256 p0 = vloadi256_mask32(srcData, msk);

      p0 = vpshufb(p0, predicate);
      p0 = vor(p0, fillMask);
      vstorei256_mask32(dstData, p0, msk);

      dstData += i * 4;
      srcData += i * 4;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLPixelConverter - RGB32 <- RGB24 (AVX2)]
// ============================================================================

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
  const BLCommonTable::LoadStoreM32* loadStoreM32 = blCommonTable.load_store_m32;

  I256 fillMask = vseti256u32(d.fillMask);
  I256 predicate = vdupli128(vloadi128u(d.shufbPredicate));

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while_nounroll (i >= 32) {
      I256 p0, p1, p2, p3;
      I256 q0, q1, q2, q3;

      p0 = vloadi256_128u(srcData +  0);             // [x5|z4 y4 x4|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]
      p1 = vloadi256_128u(srcData + 16);             // [yA|xA|z9 y9|x9 z8 y8 x8|z7 y7 x7 z6|y6 x6 z5 y5]
      p3 = vloadi256_128u(srcData + 32);             // [zF yF xF zE|yE xE zD yD|xD zC yC xC|zB yB xB zA]

      p2 = vpalignr<8>(p3, p1);                      // [-- -- -- --|zB yB xB zA|yA|xA|z9 y9|x9 z8 y8 x8]
      p1 = vpalignr<12>(p1, p0);                     // [-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5|x5|z4 y4 x4]
      p3 = vsrli128b<4>(p3);                         // [-- -- -- --|zF yF xF zE|yE xE zD yD|xD zC yC xC]

      p0 = vunpackli128(p0, p1);
      p2 = vunpackli128(p2, p3);

      q0 = vloadi256_128u(srcData + 48);             // [x5|z4 y4 x4|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]
      q1 = vloadi256_128u(srcData + 64);             // [yA|xA|z9 y9|x9 z8 y8 x8|z7 y7 x7 z6|y6 x6 z5 y5]
      q3 = vloadi256_128u(srcData + 80);             // [zF yF xF zE|yE xE zD yD|xD zC yC xC|zB yB xB zA]

      q2 = vpalignr<8>(q3, q1);                      // [-- -- -- --|zB yB xB zA|yA|xA|z9 y9|x9 z8 y8 x8]
      q1 = vpalignr<12>(q1, q0);                     // [-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5|x5|z4 y4 x4]
      q3 = vsrli128b<4>(q3);                         // [-- -- -- --|zF yF xF zE|yE xE zD yD|xD zC yC xC]

      q0 = vunpackli128(q0, q1);
      q2 = vunpackli128(q2, q3);

      p0 = vpshufb_or(p0, predicate, fillMask);
      p2 = vpshufb_or(p2, predicate, fillMask);
      q0 = vpshufb_or(q0, predicate, fillMask);
      q2 = vpshufb_or(q2, predicate, fillMask);

      vstorei256u(dstData +  0, p0);
      vstorei256u(dstData + 32, p2);
      vstorei256u(dstData + 64, q0);
      vstorei256u(dstData + 96, q2);

      dstData += 128;
      srcData += 96;
      i -= 32;
    }

    while_nounroll (i >= 8) {
      I128 p0, p1;

      p0 = vloadi128u(srcData);                      // [x5|z4 y4 x4|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]
      p1 = vloadi128_64(srcData + 16);               // [-- -- -- --|-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5]
      p1 = vpalignr<12>(p1, p0);                     // [-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5|x5|z4 y4 x4]

      p0 = vpshufb_or(p0, vcast<I128>(predicate), vcast<I128>(fillMask));
      p1 = vpshufb_or(p1, vcast<I128>(predicate), vcast<I128>(fillMask));

      vstorei128u(dstData +  0, p0);
      vstorei128u(dstData + 16, p1);

      dstData += 32;
      srcData += 24;
      i -= 8;
    }

    if (i >= 4) {
      I128 p0;

      p0 = vloadi128_64(srcData);                    // [-- -- -- --|-- -- -- --|y2 x2 z1 y1|x1 z0 y0 x0]
      p0 = vinsertm32<2>(p0, srcData + 8);           // [-- -- -- --|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]

      p0 = vpshufb_or(p0, vcast<I128>(predicate), vcast<I128>(fillMask));
      vstorei128u(dstData, p0);

      dstData += 16;
      srcData += 12;
      i -= 4;
    }

    if (i) {
      I128 p0 = vzeroi128();
      I128 msk = loadStoreM32[i].as<I128>();

      p0 = vinsertm24<0>(p0, srcData + 0);           // [-- -- -- --|-- -- -- z2|y2 x2 z1 y1|x1 z0 y0 x0]
      if (i >= 2) {
        p0 = vinsertm24<3>(p0, srcData + 3);         // [-- -- -- --|-- -- -- --|-- -- z1 y1|x1 z0 y0 x0]
        if (i >= 3) {
          p0 = vinsertm24<6>(p0, srcData + 6);       // [-- -- -- --|-- -- -- z2|y2 x2 z1 y1|x1 z0 y0 x0]
        }
      }

      p0 = vpshufb_or(p0, vcast<I128>(predicate), vcast<I128>(fillMask));
      vstorei128_mask32(dstData, p0, msk);

      dstData += i * 4;
      srcData += i * 3;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLPixelConverter - Premultiply (AVX2)]
// ============================================================================

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
  const BLCommonTable::LoadStoreM32* loadStoreM32 = blCommonTable.load_store_m32;

  I256 zero = vzeroi256();
  I256 a255 = vseti256u64(uint64_t(0xFFu) << (A_Shift * 2));

  I256 fillMask = vseti256u32(d.fillMask);
  I256 predicate;

  if (UseShufB)
    predicate = vdupli128(vloadi128u(d.shufbPredicate));

  // Alpha byte-index that can be used by instructions that perform shuffling.
  constexpr uint32_t AI = A_Shift / 8u;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while_nounroll (i >= 16) {
      I256 p0, p1, p2, p3;

      p0 = vloadi256u(srcData +  0);
      p2 = vloadi256u(srcData + 32);
      if (UseShufB) p0 = vpshufb(p0, predicate);
      if (UseShufB) p2 = vpshufb(p2, predicate);

      p1 = vunpackhi8(p0, zero);
      p0 = vunpackli8(p0, zero);
      p3 = vunpackhi8(p2, zero);
      p2 = vunpackli8(p2, zero);

      p0 = vmuli16(vor(p0, a255), vswizi16<AI, AI, AI, AI>(p0));
      p1 = vmuli16(vor(p1, a255), vswizi16<AI, AI, AI, AI>(p1));
      p2 = vmuli16(vor(p2, a255), vswizi16<AI, AI, AI, AI>(p2));
      p3 = vmuli16(vor(p3, a255), vswizi16<AI, AI, AI, AI>(p3));

      p0 = vdiv255u16(p0);
      p1 = vdiv255u16(p1);
      p2 = vdiv255u16(p2);
      p3 = vdiv255u16(p3);

      p0 = vpacki16u8(p0, p1);
      p2 = vpacki16u8(p2, p3);

      p0 = vor(p0, fillMask);
      p2 = vor(p2, fillMask);

      vstorei256u(dstData +  0, p0);
      vstorei256u(dstData + 32, p2);

      dstData += 64;
      srcData += 64;
      i -= 16;
    }

    while_nounroll (i) {
      uint32_t n = blMin(i, uint32_t(8));

      I256 p0, p1;
      I256 msk = loadStoreM32[n].as<I256>();

      p0 = vloadi256_mask32(srcData, msk);
      if (UseShufB) p0 = vpshufb(p0, predicate);

      p1 = vunpackhi8(p0, zero);
      p0 = vunpackli8(p0, zero);

      p0 = vmuli16(vor(p0, a255), vswizi16<AI, AI, AI, AI>(p0));
      p1 = vmuli16(vor(p1, a255), vswizi16<AI, AI, AI, AI>(p1));

      p0 = vdiv255u16(p0);
      p1 = vdiv255u16(p1);

      p0 = vpacki16u8(p0, p1);
      p0 = vor(p0, fillMask);

      vstorei256_mask32(dstData, p0, msk);

      dstData += n * 4;
      srcData += n * 4;
      i -= n;
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
#endif

// ============================================================================
// [BLPixelConverter - Unpremultiply (PMULLD) (AVX2)]
// ============================================================================

template<uint32_t A_Shift>
static BL_INLINE BLResult bl_convert_unpremultiply_8888_pmulld_template_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  BL_UNUSED(self);

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4u + gap;
  srcStride -= uintptr_t(w) * 4u;

  const uint32_t* rcpTable = blCommonTable.unpremultiplyRcp;
  const BLCommonTable::LoadStoreM32* loadStoreM32 = blCommonTable.load_store_m32;

  I256 alphaMask = vseti256u32(0xFFu << A_Shift);
  I256 componentMask = vseti256u32(0xFFu);
  I256 rnd = vseti256u32(0x8000u);

  // Alpha byte-index that can be used by instructions that perform shuffling.
  constexpr uint32_t AI = A_Shift / 8u;
  constexpr uint32_t RI = (AI + 1) % 4;
  constexpr uint32_t GI = (AI + 2) % 4;
  constexpr uint32_t BI = (AI + 3) % 4;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while_nounroll (i >= 8) {
      I256 pix = vloadi256u(srcData);

      I256 rcp0 = vloadi256_32(rcpTable + srcData[0 * 4 + AI]);
      I256 rcp4 = vloadi256_32(rcpTable + srcData[4 * 4 + AI]);

      rcp0 = vinsertum32<1>(rcp0, rcpTable + srcData[1 * 4 + AI]);
      rcp4 = vinsertum32<1>(rcp4, rcpTable + srcData[5 * 4 + AI]);

      rcp0 = vinsertum32<2>(rcp0, rcpTable + srcData[2 * 4 + AI]);
      rcp4 = vinsertum32<2>(rcp4, rcpTable + srcData[6 * 4 + AI]);

      rcp0 = vinsertum32<3>(rcp0, rcpTable + srcData[3 * 4 + AI]);
      rcp4 = vinsertum32<3>(rcp4, rcpTable + srcData[7 * 4 + AI]);

      rcp0 = vunpackli128(rcp0, rcp4);

      I256 pr = vsrli32<RI * 8>(pix);
      I256 pg = vsrli32<GI * 8>(pix);
      I256 pb = vsrli32<BI * 8>(pix);

      if (RI != 3) pr = vand(pr, componentMask);
      if (GI != 3) pg = vand(pg, componentMask);
      if (BI != 3) pb = vand(pb, componentMask);

      pr = vmulu32(pr, rcp0);
      pg = vmulu32(pg, rcp0);
      pb = vmulu32(pb, rcp0);

      pix = vand(pix, alphaMask);
      pr = vslli32<RI * 8>(vsrli32<16>(vaddi32(pr, rnd)));
      pg = vslli32<GI * 8>(vsrli32<16>(vaddi32(pg, rnd)));
      pb = vslli32<BI * 8>(vsrli32<16>(vaddi32(pb, rnd)));

      pix = vor(pix, pr);
      pix = vor(pix, pg);
      pix = vor(pix, pb);
      vstorei256u(dstData, pix);

      dstData += 32;
      srcData += 32;
      i -= 8;
    }

    if (i) {
      I256 msk = loadStoreM32[i].as<I256>();
      I256 pix = vloadi256_mask32(srcData, msk);
      I256 pixHi = vpermi128<1, 1>(pix);

      size_t idx0 = vextractu8<0 * 4 + AI>(pix);
      size_t idx4 = vextractu8<0 * 4 + AI>(pixHi);

      I256 rcp0 = vloadi256_32(rcpTable + idx0);
      I256 rcp4 = vloadi256_32(rcpTable + idx4);

      size_t idx1 = vextractu8<1 * 4 + AI>(pix);
      size_t idx5 = vextractu8<1 * 4 + AI>(pixHi);

      rcp0 = vinsertum32<1>(rcp0, rcpTable + idx1);
      rcp4 = vinsertum32<1>(rcp4, rcpTable + idx5);

      size_t idx2 = vextractu8<2 * 4 + AI>(pix);
      size_t idx6 = vextractu8<2 * 4 + AI>(pixHi);

      rcp0 = vinsertum32<2>(rcp0, rcpTable + idx2);
      rcp4 = vinsertum32<2>(rcp4, rcpTable + idx6);

      size_t idx3 = vextractu8<3 * 4 + AI>(pix);
      size_t idx7 = vextractu8<3 * 4 + AI>(pixHi);

      rcp0 = vinsertum32<3>(rcp0, rcpTable + idx3);
      rcp4 = vinsertum32<3>(rcp4, rcpTable + idx7);

      rcp0 = vunpackli128(rcp0, rcp4);

      I256 pr = vsrli32<RI * 8>(pix);
      I256 pg = vsrli32<GI * 8>(pix);
      I256 pb = vsrli32<BI * 8>(pix);

      if (RI != 3) pr = vand(pr, componentMask);
      if (GI != 3) pg = vand(pg, componentMask);
      if (BI != 3) pb = vand(pb, componentMask);

      pr = vmulu32(pr, rcp0);
      pg = vmulu32(pg, rcp0);
      pb = vmulu32(pb, rcp0);

      pix = vand(pix, alphaMask);
      pr = vslli32<RI * 8>(vsrli32<16>(vaddi32(pr, rnd)));
      pg = vslli32<GI * 8>(vsrli32<16>(vaddi32(pg, rnd)));
      pb = vslli32<BI * 8>(vsrli32<16>(vaddi32(pb, rnd)));

      pix = vor(pix, pr);
      pix = vor(pix, pg);
      pix = vor(pix, pb);
      vstorei256_mask32(dstData, pix, msk);

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

// ============================================================================
// [BLPixelConverter - Unpremultiply (FLOAT) (AVX2)]
// ============================================================================

template<uint32_t A_Shift>
static BL_INLINE BLResult bl_convert_unpremultiply_8888_float_template_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  BL_UNUSED(self);

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4u + gap;
  srcStride -= uintptr_t(w) * 4u;

  const BLCommonTable::LoadStoreM32* loadStoreM32 = blCommonTable.load_store_m32;

  I256 alphaMask = vseti256u32(0xFFu << A_Shift);
  I256 componentMask = vseti256u32(0xFFu);

  F256 const255 = vsetf256(255.0001);
  F256 lessThanOne = vsetf256(0.1);

  // Alpha byte-index that can be used by instructions that perform shuffling.
  constexpr uint32_t AI = A_Shift / 8u;
  constexpr uint32_t RI = (AI + 1) % 4;
  constexpr uint32_t GI = (AI + 2) % 4;
  constexpr uint32_t BI = (AI + 3) % 4;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while_nounroll (i >= 8) {
      I256 pix = vloadi256u(srcData);
      I256 pa = vsrli32<AI * 8>(pix);
      I256 pr = vsrli32<RI * 8>(pix);

      if (AI != 3) pa = vand(pa, componentMask);
      if (RI != 3) pr = vand(pr, componentMask);

      F256 fa = vcvti256f256(pa);
      F256 fr = vcvti256f256(pr);

      fa = vdivps(const255, vmaxps(fa, lessThanOne));

      I256 pg = vsrli32<GI * 8>(pix);
      I256 pb = vsrli32<BI * 8>(pix);

      if (GI != 3) pg = vand(pg, componentMask);
      if (BI != 3) pb = vand(pb, componentMask);

      F256 fg = vcvti256f256(pg);
      F256 fb = vcvti256f256(pb);

      fr = vmulps(fr, fa);
      fg = vmulps(fg, fa);
      fb = vmulps(fb, fa);

      pix = vand(pix, alphaMask);
      pr = vcvtf256i256(fr);
      pg = vcvtf256i256(fg);
      pb = vcvtf256i256(fb);

      pr = vslli32<RI * 8>(pr);
      pg = vslli32<GI * 8>(pg);
      pb = vslli32<BI * 8>(pb);

      pix = vor(pix, pr);
      pix = vor(pix, pg);
      pix = vor(pix, pb);
      vstorei256u(dstData, pix);

      dstData += 32;
      srcData += 32;
      i -= 8;
    }

    if (i) {
      I256 msk = loadStoreM32[i].as<I256>();
      I256 pix = vloadi256_mask32(srcData, msk);

      I256 pa = vsrli32<AI * 8>(pix);
      I256 pr = vsrli32<RI * 8>(pix);

      if (AI != 3) pa = vand(pa, componentMask);
      if (RI != 3) pr = vand(pr, componentMask);

      F256 fa = vcvti256f256(pa);
      F256 fr = vcvti256f256(pr);

      fa = vdivps(const255, vmaxps(fa, lessThanOne));

      I256 pg = vsrli32<GI * 8>(pix);
      I256 pb = vsrli32<BI * 8>(pix);

      if (GI != 3) pg = vand(pg, componentMask);
      if (BI != 3) pb = vand(pb, componentMask);

      F256 fg = vcvti256f256(pg);
      F256 fb = vcvti256f256(pb);

      fr = vmulps(fr, fa);
      fg = vmulps(fg, fa);
      fb = vmulps(fb, fa);

      pix = vand(pix, alphaMask);
      pr = vcvtf256i256(fr);
      pg = vcvtf256i256(fg);
      pb = vcvtf256i256(fb);

      pr = vslli32<RI * 8>(pr);
      pg = vslli32<GI * 8>(pg);
      pb = vslli32<BI * 8>(pb);

      pix = vor(pix, pr);
      pix = vor(pix, pg);
      pix = vor(pix, pb);
      vstorei256_mask32(dstData, pix, msk);

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
