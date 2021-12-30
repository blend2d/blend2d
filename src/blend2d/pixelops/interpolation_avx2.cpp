// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#ifdef BL_TARGET_OPT_AVX2

#include "../gradient_p.h"
#include "../math_p.h"
#include "../simd_p.h"

namespace BLPixelOps {
namespace Interpolation {

void BL_CDECL interpolate_prgb32_avx2(uint32_t* dPtr, uint32_t dSize, const BLGradientStop* sPtr, size_t sSize) noexcept {
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
  Vec256I argb64_a255 = v_fill_i256_u64(0x00FF000000000000u);

  uint32_t u0 = 0;
  uint32_t u1;

  size_t sIndex = size_t(sPtr[0].offset == 0.0 && sSize > 1);
  double fWidth = double(int32_t(--dSize) << 8);

  do {
    c1 = v_load_i64(&sPtr[sIndex].rgba);
    u1 = uint32_t(blRoundToInt(sPtr[sIndex].offset * fWidth));

    dSpanPtr = dPtr + (u0 >> 8);
    i = ((u1 >> 8) - (u0 >> 8));
    u0 = u1;

    if (i <= 1) {
      Vec128I cPix = v_interleave_lo_i64(c0, c1);
      c0 = c1;
      cPix = v_srl_i16<8>(cPix);

      Vec128I cA = v_swizzle_i16<3, 3, 3, 3>(cPix);
      cPix = v_or(cPix, v_cast<Vec128I>(argb64_a255));
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
      Vec256I dx;

      // Scale `dx` by taking advantage of DP-FP division.
      {
        Vec128I cx;
        Vec128D scale = s_div_f64(v_d128_from_f64(1 << 23), s_cvt_i32_f64(int(i)));

        c0 = v_interleave_lo_i8(c0, c0);
        cx = v_interleave_lo_i8(c1, c1);

        c0 = v_srl_i32<24>(c0);
        cx = v_srl_i32<24>(cx);
        cx = v_sub_i32(cx, c0);
        c0 = v_sll_i32<23>(c0);

        dx = v_dupl_i128(v_cvtt_f64_i32(v_mul_f64(v_cvt_4xi32_f64(cx), v_splat256_f64(scale))));
      }

      c0 = v_add_i32(c0, half);
      uint32_t n = i + 1;

      if (n >= 8) {
        Vec256I cx = v_add_i32(v_dupl_i128(c0), v_permute_i128<0, 0x8>(v_cast<Vec256I>(v_sll_i32<2>(dx))));
        Vec256I dx5 = v_add_i32(v_sll_i32<2>(dx), dx);

        BL_NOUNROLL
        while (n >= 16) {
          Vec256I p40 = v_srl_i32<23>(cx); cx = v_add_i32(cx, dx);
          Vec256I p51 = v_srl_i32<23>(cx); cx = v_add_i32(cx, dx);
          Vec256I p5410 = v_packs_i32_i16(p40, p51);

          Vec256I p62 = v_srl_i32<23>(cx); cx = v_add_i32(cx, dx);
          Vec256I p73 = v_srl_i32<23>(cx); cx = v_add_i32(cx, dx5);
          Vec256I p7632 = v_packs_i32_i16(p62, p73);

          Vec256I q40 = v_srl_i32<23>(cx); cx = v_add_i32(cx, dx);
          Vec256I q51 = v_srl_i32<23>(cx); cx = v_add_i32(cx, dx);
          Vec256I q5410 = v_packs_i32_i16(q40, q51);

          Vec256I q62 = v_srl_i32<23>(cx); cx = v_add_i32(cx, dx);
          Vec256I q73 = v_srl_i32<23>(cx); cx = v_add_i32(cx, dx5);
          Vec256I q7632 = v_packs_i32_i16(q62, q73);

          p5410 = v_mul_u16(v_or(p5410, argb64_a255), v_swizzle_i16<3, 3, 3, 3>(p5410));
          p7632 = v_mul_u16(v_or(p7632, argb64_a255), v_swizzle_i16<3, 3, 3, 3>(p7632));
          q5410 = v_mul_u16(v_or(q5410, argb64_a255), v_swizzle_i16<3, 3, 3, 3>(q5410));
          q7632 = v_mul_u16(v_or(q7632, argb64_a255), v_swizzle_i16<3, 3, 3, 3>(q7632));

          p5410 = v_div255_u16(p5410);
          p7632 = v_div255_u16(p7632);
          q5410 = v_div255_u16(q5410);
          q7632 = v_div255_u16(q7632);

          Vec256I pp = v_packs_i16_u8(p5410, p7632);
          Vec256I qp = v_packs_i16_u8(q5410, q7632);

          v_storeu_i256(dSpanPtr + 0, pp);
          v_storeu_i256(dSpanPtr + 8, qp);

          n -= 16;
          dSpanPtr += 16;
        }

        BL_NOUNROLL
        while (n >= 8) {
          Vec256I p40 = v_srl_i32<23>(cx); cx = v_add_i32(cx, dx);
          Vec256I p51 = v_srl_i32<23>(cx); cx = v_add_i32(cx, dx);
          Vec256I p5410 = v_packs_i32_i16(p40, p51);

          Vec256I p62 = v_srl_i32<23>(cx); cx = v_add_i32(cx, dx);
          Vec256I p73 = v_srl_i32<23>(cx); cx = v_add_i32(cx, dx5);
          Vec256I p7632 = v_packs_i32_i16(p62, p73);

          p5410 = v_mul_u16(v_or(p5410, argb64_a255), v_swizzle_i16<3, 3, 3, 3>(p5410));
          p7632 = v_mul_u16(v_or(p7632, argb64_a255), v_swizzle_i16<3, 3, 3, 3>(p7632));

          p5410 = v_div255_u16(p5410);
          p7632 = v_div255_u16(p7632);

          Vec256I pp = v_packs_i16_u8(p5410, p7632);
          v_storeu_i256(dSpanPtr, pp);

          n -= 8;
          dSpanPtr += 8;
        }

        c0 = v_cast<Vec128I>(cx);
      }

      BL_NOUNROLL
      while (n >= 2) {
        Vec128I p0 = v_srl_i32<23>(c0); c0 = v_add_i32(c0, v_cast<Vec128I>(dx));
        Vec128I p1 = v_srl_i32<23>(c0); c0 = v_add_i32(c0, v_cast<Vec128I>(dx));

        p0 = v_packs_i32_i16(p0, p1);
        p0 = v_div255_u16(v_mul_i16(v_or(p0, v_cast<Vec128I>(argb64_a255)), v_swizzle_i16<3, 3, 3, 3>(p0)));

        p0 = v_packs_i16_u8(p0);
        v_store_i64(dSpanPtr, p0);

        n -= 2;
        dSpanPtr += 2;
      }

      if (n) {
        Vec128I p0 = v_srl_i32<23>(c0);
        c0 = v_add_i32(c0, v_cast<Vec128I>(dx));

        p0 = v_packs_i32_i16(p0, p0);
        p0 = v_div255_u16(v_mul_i16(v_or(p0, v_cast<Vec128I>(argb64_a255)), v_swizzle_i16<3, 3, 3, 3>(p0)));

        p0 = v_packs_i16_u8(p0);
        v_store_i32(dSpanPtr, p0);

        dSpanPtr++;
      }

      c0 = c1;
    }
  } while (++sIndex < sSize);

  // The last stop doesn't have to end at 1.0, in such case the remaining space
  // is filled by the last color stop (premultiplied).
  {
    i = uint32_t((size_t)((dPtr + dSize + 1) - dSpanPtr));

    c0 = v_loadh_i64(c0, &sPtr[0].rgba);
    c0 = v_srl_i16<8>(c0);

    c0 = v_div255_u16(v_mul_i16(v_or(c0, v_cast<Vec128I>(argb64_a255)), v_swizzle_i16<3, 3, 3, 3>(c0)));
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
