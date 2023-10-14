// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#if defined(BL_TARGET_OPT_SSE2)

#include "../gradient_p.h"
#include "../simd/simd_p.h"
#include "../support/math_p.h"

namespace bl {
namespace PixelOps {
namespace Interpolation {

void BL_CDECL interpolate_prgb32_sse2(uint32_t* dPtr, uint32_t dSize, const BLGradientStop* sPtr, size_t sSize) noexcept {
  using namespace SIMD;

  BL_ASSERT(dPtr != nullptr);
  BL_ASSERT(dSize > 0);

  BL_ASSERT(sPtr != nullptr);
  BL_ASSERT(sSize > 0);

  uint32_t* dSpanPtr = dPtr;
  uint32_t i = dSize;

  Vec8xU16 c0 = loada_64<Vec8xU16>(&sPtr[0].rgba);
  Vec8xU16 c1;

  Vec4xI32 half = make128_i32(1 << (23 - 1));
  Vec8xU16 argb64_a255 = make128_u64<Vec8xU16>(0x00FF000000000000u);

  uint32_t p0 = 0;
  uint32_t p1;

  size_t sIndex = size_t(sPtr[0].offset == 0.0 && sSize > 1);
  double fWidth = double(int32_t(--dSize) << 8);

  do {
    c1 = loada_64<Vec8xU16>(&sPtr[sIndex].rgba);
    p1 = uint32_t(Math::roundToInt(sPtr[sIndex].offset * fWidth));

    dSpanPtr = dPtr + (p0 >> 8);
    i = ((p1 >> 8) - (p0 >> 8));
    p0 = p1;

    if (i <= 1) {
      Vec8xU16 cPix = interleave_lo_u64(c0, c1);
      c0 = c1;
      cPix = srli_u16<8>(cPix);

      Vec8xU16 cA = swizzle_u16<3, 3, 3, 3>(cPix);
      cPix = div255_u16((cPix | argb64_a255) * cA);
      cPix = packs_128_i16_u8(cPix);
      storea_32(dSpanPtr, cPix);
      dSpanPtr++;

      if (i == 0)
        continue;

      cPix = swizzle_u32<1, 1, 1, 1>(cPix);
      storea_32(dSpanPtr, cPix);
      dSpanPtr++;
    }
    else {
      BL_SIMD_LOOP_32x4_INIT()

      Vec4xI32 cI;
      Vec4xI32 cD;

      // Scale `cD` by taking advantage of SSE2-FP division.
      {
        Vec2xF64 scale = div_1xf64(make128_f64(1 << 23), cvt_f64_from_scalar_i32(int(i)));

        cI = vec_i32(interleave_lo_u8(c0, c0));
        cD = vec_i32(interleave_lo_u8(c1, c1));

        cI = srli_u32<24>(cI);
        cD = srli_u32<24>(cD);
        cD = sub_i32(cD, cI);
        cI = slli_i32<23>(cI);

        Vec2xF64 lo = cvt_2xi32_f64(vec_i32(cD));
        cD = swap_u64(cD);
        scale = dup_lo_f64(scale);

        Vec2xF64 hi = cvt_2xi32_f64(vec_i32(cD));
        lo = lo * scale;
        hi = hi * scale;
        cD = interleave_lo_u64(cvtt_f64_i32(lo), cvtt_f64_i32(hi));
      }

      cI += half;
      i++;

      BL_SIMD_LOOP_32x4_MINI_BEGIN(Loop, dSpanPtr, i)
        Vec8xU16 cPix = vec_u16(packs_128_i32_i16(srli_u32<23>(cI)));
        Vec8xU16 cA = swizzle_u16<3, 3, 3, 3>(cPix);
        cPix = div255_u16((cPix | argb64_a255) * cA);
        cPix = packs_128_i16_u8(cPix);
        storea_32(dSpanPtr, cPix);

        cI += cD;
        dSpanPtr++;
      BL_SIMD_LOOP_32x4_MINI_END(Loop)

      BL_SIMD_LOOP_32x4_MAIN_BEGIN(Loop)
        Vec8xU16 cPix0, cA0;
        Vec8xU16 cPix1, cA1;

        cPix0 = vec_u16(srli_u32<23>(cI));
        cI = add_i32(cI, cD);
        cA0 = vec_u16(srli_u32<23>(cI));
        cI = add_i32(cI, cD);

        cPix1 = vec_u16(srli_u32<23>(cI));
        cI = add_i32(cI, cD);
        cA1 = vec_u16(srli_u32<23>(cI));
        cI = add_i32(cI, cD);

        cPix0 = packs_128_i32_i16(cPix0, cA0);
        cPix1 = packs_128_i32_i16(cPix1, cA1);
        cA0 = swizzle_u16<3, 3, 3, 3>(cPix0);
        cA1 = swizzle_u16<3, 3, 3, 3>(cPix1);

        cPix0 = div255_u16((cPix0 | argb64_a255) * cA0);
        cPix1 = div255_u16((cPix1 | argb64_a255) * cA1);

        cPix0 = packs_128_i16_u8(cPix0, cPix1);
        storea(dSpanPtr, cPix0);

        dSpanPtr += 4;
      BL_SIMD_LOOP_32x4_MAIN_END(Loop)

      c0 = c1;
    }
  } while (++sIndex < sSize);

  // The last stop doesn't have to end at 1.0, in such case the remaining space
  // is filled by the last color stop (premultiplied).
  {
    Vec8xU16 cA;
    i = uint32_t((size_t)((dPtr + dSize + 1) - dSpanPtr));

    c0 = loadh_64(c0, &sPtr[0].rgba);
    c0 = srli_u16<8>(c0);

    cA = swizzle_u16<3, 3, 3, 3>(c0);
    c0 = div255_u16((c0 | argb64_a255) * cA);
    c0 = packs_128_i16_u8(c0);
    c1 = c0;
  }

  if (i != 0) {
    do {
      storea_32(dSpanPtr, c0);
      dSpanPtr++;
    } while (--i);
  }

  // The first pixel has to be always set to the first stop's color. The main loop always honors the last color value
  // of the stop colliding with the previous offset index - for example if multiple stops have the same offset [0.0]
  // the first pixel will be the last stop's color. This is easier to fix here as we don't need extra conditions in
  // the main loop.
  storea_32(dPtr, swizzle_u32<1, 1, 1, 1>(c1));
}

} // {Interpolation}
} // {PixelOps}
} // {bl}

#endif
