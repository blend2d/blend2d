// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#ifdef BL_TARGET_OPT_AVX2

#include "./blgradient_p.h"
#include "./blmath_p.h"
#include "./blsimd_p.h"
#include "./blsupport_p.h"

// ============================================================================
// [BLGradientOps - InterpolateLut32 [AVX2]]
// ============================================================================

void BL_CDECL blGradientInterpolate32_AVX2(uint32_t* dPtr, uint32_t dWidth, const BLGradientStop* sPtr, size_t sSize) noexcept {
  using namespace SIMD;

  BL_ASSERT(dPtr != nullptr);
  BL_ASSERT(dWidth > 0);

  BL_ASSERT(sPtr != nullptr);
  BL_ASSERT(sSize > 0);

  uint32_t* dSpanPtr = dPtr;
  uint32_t i = dWidth;

  I128 c0 = vloadi128_64(&sPtr[0].rgba);
  I128 c1;

  I128 half = vseti128i32(1 << (23 - 1));
  I256 argb64_a255 = vseti256i64(int64_t(0x00FF000000000000));

  uint32_t p0 = 0;
  uint32_t p1;

  size_t sIndex = size_t(sPtr[0].offset == 0.0 && sSize > 1);
  double fWidth = double(int32_t(--dWidth) << 8);

  do {
    c1 = vloadi128_64(&sPtr[sIndex].rgba);
    p1 = uint32_t(blRoundToInt(sPtr[sIndex].offset * fWidth));

    dSpanPtr = dPtr + (p0 >> 8);
    i = ((p1 >> 8) - (p0 >> 8));
    p0 = p1;

    if (i <= 1) {
      I128 cPix = vunpackli64(c0, c1);
      c0 = c1;
      cPix = vsrli16<8>(cPix);

      I128 cA = vswizi16<3, 3, 3, 3>(cPix);
      cPix = vor(cPix, vcast<I128>(argb64_a255));
      cPix = vdiv255u16(vmuli16(cPix, cA));
      cPix = vpacki16u8(cPix);
      vstorei32(dSpanPtr, cPix);
      dSpanPtr++;

      if (i == 0)
        continue;

      cPix = vswizi32<1, 1, 1, 1>(cPix);
      vstorei32(dSpanPtr, cPix);
      dSpanPtr++;
    }
    else {
      I256 dx;

      // Scale `dx` by taking advantage of DP-FP division.
      {
        I128 cx;
        D128 scale = vdivsd(vcvtd64d128(1 << 23), vcvti32d128(i));

        c0 = vunpackli8(c0, c0);
        cx = vunpackli8(c1, c1);

        c0 = vsrli32<24>(c0);
        cx = vsrli32<24>(cx);
        cx = vsubi32(cx, c0);
        c0 = vslli32<23>(c0);

        dx = vdupli128(vcvttd256i128(vmulpd(vcvti128d256(cx), vsplatd64d256(scale))));
      }

      c0 = vaddi32(c0, half);
      uint32_t n = i + 1;

      if (n >= 8) {
        I256 cx = vaddi32(vdupli128(c0), vpermi128<0, -1>(vcast<I256>(vslli32<2>(dx))));
        I256 dx5 = vaddi32(vslli32<2>(dx), dx);

        while (n >= 16) {
          I256 p40 = vsrli32<23>(cx); cx = vaddi32(cx, dx);
          I256 p51 = vsrli32<23>(cx); cx = vaddi32(cx, dx);
          I256 p5410 = vpacki32i16(p40, p51);

          I256 p62 = vsrli32<23>(cx); cx = vaddi32(cx, dx);
          I256 p73 = vsrli32<23>(cx); cx = vaddi32(cx, dx5);
          I256 p7632 = vpacki32i16(p62, p73);

          I256 q40 = vsrli32<23>(cx); cx = vaddi32(cx, dx);
          I256 q51 = vsrli32<23>(cx); cx = vaddi32(cx, dx);
          I256 q5410 = vpacki32i16(q40, q51);

          I256 q62 = vsrli32<23>(cx); cx = vaddi32(cx, dx);
          I256 q73 = vsrli32<23>(cx); cx = vaddi32(cx, dx5);
          I256 q7632 = vpacki32i16(q62, q73);

          p5410 = vmulu16(vor(p5410, argb64_a255), vswizi16<3, 3, 3, 3>(p5410));
          p7632 = vmulu16(vor(p7632, argb64_a255), vswizi16<3, 3, 3, 3>(p7632));
          q5410 = vmulu16(vor(q5410, argb64_a255), vswizi16<3, 3, 3, 3>(q5410));
          q7632 = vmulu16(vor(q7632, argb64_a255), vswizi16<3, 3, 3, 3>(q7632));

          p5410 = vdiv255u16(p5410);
          p7632 = vdiv255u16(p7632);
          q5410 = vdiv255u16(q5410);
          q7632 = vdiv255u16(q7632);

          I256 pp = vpacki16u8(p5410, p7632);
          I256 qp = vpacki16u8(q5410, q7632);

          vstorei256u(dSpanPtr + 0, pp);
          vstorei256u(dSpanPtr + 8, qp);

          n -= 16;
          dSpanPtr += 16;
        }

        while (n >= 8) {
          I256 p40 = vsrli32<23>(cx); cx = vaddi32(cx, dx);
          I256 p51 = vsrli32<23>(cx); cx = vaddi32(cx, dx);
          I256 p5410 = vpacki32i16(p40, p51);

          I256 p62 = vsrli32<23>(cx); cx = vaddi32(cx, dx);
          I256 p73 = vsrli32<23>(cx); cx = vaddi32(cx, dx5);
          I256 p7632 = vpacki32i16(p62, p73);

          p5410 = vmulu16(vor(p5410, argb64_a255), vswizi16<3, 3, 3, 3>(p5410));
          p7632 = vmulu16(vor(p7632, argb64_a255), vswizi16<3, 3, 3, 3>(p7632));

          p5410 = vdiv255u16(p5410);
          p7632 = vdiv255u16(p7632);

          I256 pp = vpacki16u8(p5410, p7632);
          vstorei256u(dSpanPtr, pp);

          n -= 8;
          dSpanPtr += 8;
        }

        c0 = vcast<I128>(cx);
      }

      while (n >= 2) {
        I128 p0 = vsrli32<23>(c0); c0 = vaddi32(c0, vcast<I128>(dx));
        I128 p1 = vsrli32<23>(c0); c0 = vaddi32(c0, vcast<I128>(dx));

        p0 = vpacki32i16(p0, p1);
        p0 = vdiv255u16(vmuli16(vor(p0, vcast<I128>(argb64_a255)), vswizi16<3, 3, 3, 3>(p0)));

        p0 = vpacki16u8(p0);
        vstorei64(dSpanPtr, p0);

        n -= 2;
        dSpanPtr += 2;
      }

      if (n) {
        I128 p0 = vsrli32<23>(c0);
        c0 = vaddi32(c0, vcast<I128>(dx));

        p0 = vpacki32i16(p0, p0);
        p0 = vdiv255u16(vmuli16(vor(p0, vcast<I128>(argb64_a255)), vswizi16<3, 3, 3, 3>(p0)));

        p0 = vpacki16u8(p0);
        vstorei32(dSpanPtr, p0);

        dSpanPtr++;
      }

      c0 = c1;
    }
  } while (++sIndex < sSize);

  // The last stop doesn't have to end at 1.0, in such case the remaining space
  // is filled by the last color stop (premultiplied).
  {
    i = uint32_t((size_t)((dPtr + dWidth + 1) - dSpanPtr));

    c0 = vloadi128_h64(c0, &sPtr[0].rgba);
    c0 = vsrli16<8>(c0);

    c0 = vdiv255u16(vmuli16(vor(c0, vcast<I128>(argb64_a255)), vswizi16<3, 3, 3, 3>(c0)));
    c0 = vpacki16u8(c0);
    c1 = c0;
  }

  if (i != 0) {
    do {
      vstorei32(dSpanPtr, c0);
      dSpanPtr++;
    } while (--i);
  }

  // The first pixel has to be always set to the first stop's color. The main
  // loop always honors the last color value of the stop colliding with the
  // previous offset index - for example if multiple stops have the same offset
  // [0.0] the first pixel will be the last stop's color. This is easier to fix
  // here as we don't need extra conditions in the main loop.
  vstorei32(dPtr, vswizi32<1, 1, 1, 1>(c1));
}

#endif
