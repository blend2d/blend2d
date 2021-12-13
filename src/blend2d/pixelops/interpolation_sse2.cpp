// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#ifdef BL_TARGET_OPT_SSE2

#include "../gradient_p.h"
#include "../math_p.h"
#include "../simd_p.h"

namespace BLPixelOps {
namespace Interpolation {

void BL_CDECL interpolate_prgb32_sse2(uint32_t* dPtr, uint32_t dSize, const BLGradientStop* sPtr, size_t sSize) noexcept {
  using namespace SIMD;

  BL_ASSERT(dPtr != nullptr);
  BL_ASSERT(dSize > 0);

  BL_ASSERT(sPtr != nullptr);
  BL_ASSERT(sSize > 0);

  uint32_t* dSpanPtr = dPtr;
  uint32_t i = dSize;

  Vec128I c0 = v_load_i64(&sPtr[0].rgba);
  Vec128I c1;

  Vec128I half = v_fill_i128_i32(1 << (23 - 1));
  Vec128I argb64_a255 = v_fill_i128_u64(0x00FF000000000000u);

  uint32_t p0 = 0;
  uint32_t p1;

  size_t sIndex = size_t(sPtr[0].offset == 0.0 && sSize > 1);
  double fWidth = double(int32_t(--dSize) << 8);

  do {
    c1 = v_load_i64(&sPtr[sIndex].rgba);
    p1 = uint32_t(blRoundToInt(sPtr[sIndex].offset * fWidth));

    dSpanPtr = dPtr + (p0 >> 8);
    i = ((p1 >> 8) - (p0 >> 8));
    p0 = p1;

    if (i <= 1) {
      Vec128I cPix = v_interleave_lo_i64(c0, c1);
      c0 = c1;
      cPix = v_srl_i16<8>(cPix);

      Vec128I cA = v_swizzle_i16<3, 3, 3, 3>(cPix);
      cPix = v_or(cPix, argb64_a255);
      cPix = v_div255_u16(v_mul_i16(cPix, cA));
      cPix = v_packs_i16_u8(cPix);
      v_store_i32(dSpanPtr, cPix);
      dSpanPtr++;

      if (i == 0)
        continue;

      cPix = v_swizzle_i32<1, 1, 1, 1>(cPix);
      v_store_i32(dSpanPtr, cPix);
      dSpanPtr++;
    }
    else {
      BL_SIMD_LOOP_32x4_INIT()

      Vec128I cD;

      // Scale `cD` by taking advantage of SSE2-FP division.
      {
        Vec128D scale = s_div_f64(v_d128_from_f64(1 << 23), s_cvt_i32_f64(int(i)));

        c0 = v_interleave_lo_i8(c0, c0);
        cD = v_interleave_lo_i8(c1, c1);

        c0 = v_srl_i32<24>(c0);
        cD = v_srl_i32<24>(cD);
        cD = v_sub_i32(cD, c0);
        c0 = v_sll_i32<23>(c0);

        Vec128D lo = v_cvt_2xi32_f64(cD);
        cD = v_swap_i64(cD);
        scale = v_dupl_f64(scale);

        Vec128D hi = v_cvt_2xi32_f64(cD);
        lo = v_mul_f64(lo, scale);
        hi = v_mul_f64(hi, scale);

        cD = v_cvtt_f64_i32(lo);
        cD = v_interleave_lo_i64(cD, v_cvtt_f64_i32(hi));
      }

      c0 = v_add_i32(c0, half);
      i++;

      BL_SIMD_LOOP_32x4_MINI_BEGIN(Loop, dSpanPtr, i)
        Vec128I cPix, cA;

        cPix = v_srl_i32<23>(c0);
        c0 = v_add_i32(c0, cD);

        cPix = v_packs_i32_i16(cPix, cPix);
        cA = v_swizzle_i16<3, 3, 3, 3>(cPix);
        cPix = v_or(cPix, argb64_a255);
        cPix = v_div255_u16(v_mul_i16(cPix, cA));
        cPix = v_packs_i16_u8(cPix);
        v_store_i32(dSpanPtr, cPix);

        dSpanPtr++;
      BL_SIMD_LOOP_32x4_MINI_END(Loop)

      BL_SIMD_LOOP_32x4_MAIN_BEGIN(Loop)
        Vec128I cPix0, cA0;
        Vec128I cPix1, cA1;

        cPix0 = v_srl_i32<23>(c0);
        c0 = v_add_i32(c0, cD);

        cPix1 = v_srl_i32<23>(c0);
        c0 = v_add_i32(c0, cD);
        cPix0 = v_packs_i32_i16(cPix0, cPix1);

        cPix1 = v_srl_i32<23>(c0);
        c0 = v_add_i32(c0, cD);

        cA0 = v_srl_i32<23>(c0);
        c0 = v_add_i32(c0, cD);
        cPix1 = v_packs_i32_i16(cPix1, cA0);

        cA0 = v_swizzle_i16<3, 3, 3, 3>(cPix0);
        cA1 = v_swizzle_i16<3, 3, 3, 3>(cPix1);

        cPix0 = v_or(cPix0, argb64_a255);
        cPix1 = v_or(cPix1, argb64_a255);

        cPix0 = v_div255_u16(v_mul_i16(cPix0, cA0));
        cPix1 = v_div255_u16(v_mul_i16(cPix1, cA1));

        cPix0 = v_packs_i16_u8(cPix0, cPix1);
        v_storea_i128(dSpanPtr, cPix0);

        dSpanPtr += 4;
      BL_SIMD_LOOP_32x4_MAIN_END(Loop)

      c0 = c1;
    }
  } while (++sIndex < sSize);

  // The last stop doesn't have to end at 1.0, in such case the remaining space
  // is filled by the last color stop (premultiplied).
  {
    Vec128I cA;
    i = uint32_t((size_t)((dPtr + dSize + 1) - dSpanPtr));

    c0 = v_loadh_i64(c0, &sPtr[0].rgba);
    c0 = v_srl_i16<8>(c0);

    cA = v_swizzle_i16<3, 3, 3, 3>(c0);
    c0 = v_or(c0, argb64_a255);
    c0 = v_div255_u16(v_mul_i16(c0, cA));
    c0 = v_packs_i16_u8(c0);
    c1 = c0;
  }

  if (i != 0) {
    do {
      v_store_i32(dSpanPtr, c0);
      dSpanPtr++;
    } while (--i);
  }

  // The first pixel has to be always set to the first stop's color. The main
  // loop always honors the last color value of the stop colliding with the
  // previous offset index - for example if multiple stops have the same offset
  // [0.0] the first pixel will be the last stop's color. This is easier to fix
  // here as we don't need extra conditions in the main loop.
  v_store_i32(dPtr, v_swizzle_i32<1, 1, 1, 1>(c1));
}

} // {Interpolation}
} // {BLPixelOps}

#endif
