// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#ifdef BL_TARGET_OPT_AVX2

#include "../gradient_p.h"
#include "../simd/simd_p.h"
#include "../support/math_p.h"

namespace bl {
namespace PixelOps {
namespace Interpolation {

void BL_CDECL interpolate_prgb32_avx2(uint32_t* dPtr, uint32_t dSize, const BLGradientStop* sPtr, size_t sSize) noexcept {
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
  Vec16xU16 argb64_a255 = make256_u64<Vec16xU16>(0x00FF000000000000u);

  uint32_t u0 = 0;
  uint32_t u1;

  size_t sIndex = size_t(sPtr[0].offset == 0.0 && sSize > 1);
  double fWidth = double(int32_t(--dSize) << 8);

  do {
    c1 = loada_64<Vec8xU16>(&sPtr[sIndex].rgba);
    u1 = uint32_t(Math::roundToInt(sPtr[sIndex].offset * fWidth));

    dSpanPtr = dPtr + (u0 >> 8);
    i = ((u1 >> 8) - (u0 >> 8));
    u0 = u1;

    if (i <= 1) {
      Vec8xU16 cPix = interleave_lo_u64(c0, c1);
      c0 = c1;
      cPix = srli_u16<8>(cPix);

      Vec8xU16 cA = swizzle_u16<3, 3, 3, 3>(cPix);
      cPix = div255_u16(cPix | vec_cast<Vec8xU16>(argb64_a255) * cA);
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
      uint32_t n = i + 1;

      // Scale `dx` by taking advantage of DP-FP division.
      Vec2xF64 scale = div_1xf64(cast_from_f64<Vec2xF64>(1 << 23), cvt_f64_from_scalar_i32(int(i)));
      Vec4xI32 c32 = vec_i32(interleave_lo_u8(c0, c0));
      Vec4xI32 d32 = vec_i32(interleave_lo_u8(c1, c1));

      c32 = srli_u32<24>(c32);
      d32 = srli_u32<24>(d32) - c32;
      c32 = slli_i32<23>(c32);

      Vec8xI32 dx = broadcast_i128<Vec8xI32>(cvtt_f64_i32(cvt_4xi32_f64(d32) * broadcast_f64<Vec4xF64>(scale)));
      Vec8xI32 dx4 = slli_i32<2>(dx);
      Vec8xI32 cx = broadcast_i128<Vec8xI32>(c32 + half) + permute_i128<0, 0x8>(dx4);
      Vec8xI32 dx5 = dx + dx4;

      BL_NOUNROLL
      while (n >= 16u) {
        Vec8xI32 p40 = srli_u32<23>(cx); cx += dx;
        Vec8xI32 p51 = srli_u32<23>(cx); cx += dx;
        Vec16xU16 p5410 = vec_u16(packs_128_i32_i16(p40, p51));

        Vec8xI32 p62 = srli_u32<23>(cx); cx += dx;
        Vec8xI32 p73 = srli_u32<23>(cx); cx += dx5;
        Vec16xU16 p7632 = vec_u16(packs_128_i32_i16(p62, p73));

        Vec8xI32 q40 = srli_u32<23>(cx); cx += dx;
        Vec8xI32 q51 = srli_u32<23>(cx); cx += dx;
        Vec16xU16 q5410 = vec_u16(packs_128_i32_i16(q40, q51));

        Vec8xI32 q62 = srli_u32<23>(cx); cx += dx;
        Vec8xI32 q73 = srli_u32<23>(cx); cx += dx5;
        Vec16xU16 q7632 = vec_u16(packs_128_i32_i16(q62, q73));

        p5410 = div255_u16((p5410 | argb64_a255) * swizzle_u16<3, 3, 3, 3>(p5410));
        p7632 = div255_u16((p7632 | argb64_a255) * swizzle_u16<3, 3, 3, 3>(p7632));
        q5410 = div255_u16((q5410 | argb64_a255) * swizzle_u16<3, 3, 3, 3>(q5410));
        q7632 = div255_u16((q7632 | argb64_a255) * swizzle_u16<3, 3, 3, 3>(q7632));

        storeu(dSpanPtr + 0, packs_128_i16_u8(p5410, p7632));
        storeu(dSpanPtr + 8, packs_128_i16_u8(q5410, q7632));

        dSpanPtr += 16;
        n -= 16;
      }

      Vec32xU8 p76543210;
      BL_NOUNROLL
      while (n) {
        Vec8xI32 p40 = srli_u32<23>(cx); cx += dx;
        Vec8xI32 p51 = srli_u32<23>(cx); cx += dx;
        Vec16xU16 p5410 = vec_u16(packs_128_i32_i16(p40, p51));

        Vec8xI32 p62 = srli_u32<23>(cx); cx += dx;
        Vec8xI32 p73 = srli_u32<23>(cx); cx += dx5;
        Vec16xU16 p7632 = vec_u16(packs_128_i32_i16(p62, p73));

        p5410 = div255_u16((p5410 | argb64_a255) * swizzle_u16<3, 3, 3, 3>(p5410));
        p7632 = div255_u16((p7632 | argb64_a255) * swizzle_u16<3, 3, 3, 3>(p7632));
        p76543210 = vec_u8(packs_128_i16_u8(p5410, p7632));

        if (n <= 8u) {
          Vec8xI32 msk = loada_64_i8_i32<Vec8xI32>(commonTable.loadstore16_lo8_msk8() + n);
          storeu_256_mask32(dSpanPtr, p76543210, msk);
          dSpanPtr += n;
          break;
        }
        else {
          storeu(dSpanPtr, p76543210);
          dSpanPtr += 8;
          n -= 8;
        }
      }

      c0 = c1;
    }
  } while (++sIndex < sSize);

  // The last stop doesn't have to end at 1.0, in such case the remaining space
  // is filled by the last color stop (premultiplied).
  {
    i = uint32_t((size_t)((dPtr + dSize + 1) - dSpanPtr));

    c0 = loadh_64(c0, &sPtr[0].rgba);
    c0 = srli_u16<8>(c0);

    c0 = div255_u16((c0 | vec_128(argb64_a255)) * swizzle_u16<3, 3, 3, 3>(c0));
    c0 = packs_128_i16_u8(c0);
    c1 = c0;
  }

  if (i != 0) {
    do {
      storea_32(dSpanPtr, c0);
      dSpanPtr++;
    } while (--i);
  }

  // The first pixel has to be always set to the first stop's color. The main
  // loop always honors the last color value of the stop colliding with the
  // previous offset index - for example if multiple stops have the same offset
  // [0.0] the first pixel will be the last stop's color. This is easier to fix
  // here as we don't need extra conditions in the main loop.
  storea_32(dPtr, swizzle_u32<1, 1, 1, 1>(c1));
}

} // {Interpolation}
} // {PixelOps}
} // {bl}

#endif
