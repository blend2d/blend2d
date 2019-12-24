// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "./api-build_p.h"
#ifdef BL_BUILD_OPT_SSSE3

#include "./pixelconverter_p.h"
#include "./simd_p.h"
#include "./support_p.h"

using namespace SIMD;

// ============================================================================
// [BLPixelConverter - Copy|Shufb (SSSE3)]
// ============================================================================

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
  I128 fillMask = vseti128u32(d.fillMask);
  I128 predicate = vloadi128u(d.shufbPredicate);

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while_nounroll (i >= 16) {
      I128 p0, p1, p2, p3;
      p0 = vloadi128u(srcData +  0);
      p1 = vloadi128u(srcData + 16);
      p2 = vloadi128u(srcData + 32);
      p3 = vloadi128u(srcData + 48);

      p0 = vor(vpshufb(p0, predicate), fillMask);
      p1 = vor(vpshufb(p1, predicate), fillMask);
      p2 = vor(vpshufb(p2, predicate), fillMask);
      p3 = vor(vpshufb(p3, predicate), fillMask);

      vstorei128u(dstData +  0, p0);
      vstorei128u(dstData + 16, p1);
      vstorei128u(dstData + 32, p2);
      vstorei128u(dstData + 48, p3);

      dstData += 64;
      srcData += 64;
      i -= 16;
    }

    while_nounroll (i >= 4) {
      I128 p0 = vloadi128u(srcData);
      vstorei128u(dstData, vor(vpshufb(p0, predicate), fillMask));

      dstData += 16;
      srcData += 16;
      i -= 4;
    }

    while_nounroll (i) {
      I128 p0 = vloadi128_32(srcData);
      vstorei32(dstData, vor(vpshufb(p0, predicate), fillMask));

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
// [BLPixelConverter - RGB32 <- RGB24 (SSSE3)]
// ============================================================================

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
  I128 fillMask = vseti128u32(d.fillMask);
  I128 predicate = vloadi128u(d.shufbPredicate);

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while_nounroll (i >= 16) {
      I128 p0, p1, p2, p3;
      p0 = vloadi128u(srcData +  0);                 // [x5|z4 y4 x4|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]
      p1 = vloadi128u(srcData + 16);                 // [yA|xA|z9 y9|x9 z8 y8 x8|z7 y7 x7 z6|y6 x6 z5 y5]
      p3 = vloadi128u(srcData + 32);                 // [zF yF xF zE|yE xE zD yD|xD zC yC xC|zB yB xB zA]

      p2 = vpalignr<8>(p3, p1);                      // [-- -- -- --|zB yB xB zA|yA|xA|z9 y9|x9 z8 y8 x8]
      p1 = vpalignr<12>(p1, p0);                     // [-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5|x5|z4 y4 x4]
      p3 = vsrli128b<4>(p3);                         // [-- -- -- --|zF yF xF zE|yE xE zD yD|xD zC yC xC]

      p0 = vor(vpshufb(p0, predicate), fillMask);
      p1 = vor(vpshufb(p1, predicate), fillMask);
      p2 = vor(vpshufb(p2, predicate), fillMask);
      p3 = vor(vpshufb(p3, predicate), fillMask);

      vstorei128u(dstData +  0, p0);
      vstorei128u(dstData + 16, p1);
      vstorei128u(dstData + 32, p2);
      vstorei128u(dstData + 48, p3);

      dstData += 64;
      srcData += 48;
      i -= 16;
    }

    if (i >= 8) {
      I128 p0, p1;

      p0 = vloadi128u  (srcData +  0);               // [x5|z4 y4 x4|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]
      p1 = vloadi128_64(srcData + 16);               // [-- -- -- --|-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5]
      p1 = vpalignr<12>(p1, p0);                     // [-- -- -- --|z7 y7 x7 z6|y6 x6 z5 y5|x5|z4 y4 x4]

      p0 = vor(vpshufb(p0, predicate), fillMask);
      p1 = vor(vpshufb(p1, predicate), fillMask);

      vstorei128u(dstData +  0, p0);
      vstorei128u(dstData + 16, p1);

      dstData += 32;
      srcData += 24;
      i -= 8;
    }

    if (i >= 4) {
      I128 p0, p1;

      p0 = vloadi128_64(srcData +  0);               // [-- -- -- --|-- -- -- --|y2 x2 z1 y1|x1 z0 y0 x0]
      p1 = vloadi128_32(srcData +  8);               // [-- -- -- --|-- -- -- --|-- -- -- --|z3 y3 x3 z2]
      p0 = vunpackli64(p0, p1);                      // [-- -- -- --|z3 y3 x3 z2|y2 x2 z1 y1|x1 z0 y0 x0]

      vstorei128u(dstData, vor(vpshufb(p0, predicate), fillMask));

      dstData += 16;
      srcData += 12;
      i -= 4;
    }

    while_nounroll (i) {
      uint32_t yx = blMemReadU16u(srcData + 0);
      uint32_t z  = blMemReadU8(srcData + 2);

      I128 p0 = vcvtu32i128((z << 16) | yx);
      vstorei32(dstData, vor(vpshufb(p0, predicate), fillMask));

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

// ============================================================================
// [BLPixelConverter - Premultiply (SSSE3)]
// ============================================================================

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
  I128 zero = vzeroi128();
  I128 a255 = vseti128u64(uint64_t(0xFFu) << (A_Shift * 2));

  I128 fillMask = vseti128u32(d.fillMask);
  I128 predicate = vloadi128u(d.shufbPredicate);

  // Alpha byte-index that can be used by instructions that perform shuffling.
  constexpr uint32_t AI = A_Shift / 8u;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while_nounroll (i >= 4) {
      I128 p0, p1;

      p0 = vloadi128u(srcData);
      p0 = vpshufb(p0, predicate);

      p1 = vunpackhi8(p0, zero);
      p0 = vunpackli8(p0, zero);

      p0 = vmuli16(vor(p0, a255), vswizi16<AI, AI, AI, AI>(p0));
      p1 = vmuli16(vor(p1, a255), vswizi16<AI, AI, AI, AI>(p1));

      p0 = vdiv255u16(p0);
      p1 = vdiv255u16(p1);
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
      p0 = vpshufb(p0, predicate);
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
