// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#ifdef BL_TARGET_OPT_SSE2

#include "./blgradient_p.h"
#include "./blmath_p.h"
#include "./blsimd_p.h"
#include "./blsupport_p.h"

// ============================================================================
// [BLGradientOps - Interpolate32@SSE2]
// ============================================================================

void BL_CDECL blGradientInterpolate32_SSE2(uint32_t* dPtr, uint32_t dSize, const BLGradientStop* sPtr, size_t sSize) noexcept {
  using namespace SIMD;

  BL_ASSERT(dPtr != nullptr);
  BL_ASSERT(dSize > 0);

  BL_ASSERT(sPtr != nullptr);
  BL_ASSERT(sSize > 0);

  uint32_t* dSpanPtr = dPtr;
  uint32_t i = dSize;

  I128 c0 = vloadi128_64(&sPtr[0].rgba);
  I128 c1;

  I128 half = vseti128i32(1 << (23 - 1));
  I128 argb64_a255 = vseti128i64(int64_t(0x00FF000000000000));

  uint32_t p0 = 0;
  uint32_t p1;

  size_t sIndex = size_t(sPtr[0].offset == 0.0 && sSize > 1);
  double fWidth = double(int32_t(--dSize) << 8);

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
      cPix = vor(cPix, argb64_a255);
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
      BL_SIMD_LOOP_32x4_INIT()

      I128 cD;

      // Scale `cD` by taking advantage of SSE2-FP division.
      {
        D128 scale = vdivsd(vcvtd64d128(1 << 23), vcvti32d128(i));

        c0 = vunpackli8(c0, c0);
        cD = vunpackli8(c1, c1);

        c0 = vsrli32<24>(c0);
        cD = vsrli32<24>(cD);
        cD = vsubi32(cD, c0);
        c0 = vslli32<23>(c0);

        D128 lo = vcvti128d128(cD);
        cD = vswapi64(cD);
        scale = vdupld64(scale);

        D128 hi = vcvti128d128(cD);
        lo = vmulpd(lo, scale);
        hi = vmulpd(hi, scale);

        cD = vcvttd128i128(lo);
        cD = vunpackli64(cD, vcvttd128i128(hi));
      }

      c0 = vaddi32(c0, half);
      i++;

      BL_SIMD_LOOP_32x4_MINI_BEGIN(Loop, dSpanPtr, i)
        I128 cPix, cA;

        cPix = vsrli32<23>(c0);
        c0 = vaddi32(c0, cD);

        cPix = vpacki32i16(cPix, cPix);
        cA = vswizi16<3, 3, 3, 3>(cPix);
        cPix = vor(cPix, argb64_a255);
        cPix = vdiv255u16(vmuli16(cPix, cA));
        cPix = vpacki16u8(cPix);
        vstorei32(dSpanPtr, cPix);

        dSpanPtr++;
      BL_SIMD_LOOP_32x4_MINI_END(Loop)

      BL_SIMD_LOOP_32x4_MAIN_BEGIN(Loop)
        I128 cPix0, cA0;
        I128 cPix1, cA1;

        cPix0 = vsrli32<23>(c0);
        c0 = vaddi32(c0, cD);

        cPix1 = vsrli32<23>(c0);
        c0 = vaddi32(c0, cD);
        cPix0 = vpacki32i16(cPix0, cPix1);

        cPix1 = vsrli32<23>(c0);
        c0 = vaddi32(c0, cD);

        cA0 = vsrli32<23>(c0);
        c0 = vaddi32(c0, cD);
        cPix1 = vpacki32i16(cPix1, cA0);

        cA0 = vswizi16<3, 3, 3, 3>(cPix0);
        cA1 = vswizi16<3, 3, 3, 3>(cPix1);

        cPix0 = vor(cPix0, argb64_a255);
        cPix1 = vor(cPix1, argb64_a255);

        cPix0 = vdiv255u16(vmuli16(cPix0, cA0));
        cPix1 = vdiv255u16(vmuli16(cPix1, cA1));

        cPix0 = vpacki16u8(cPix0, cPix1);
        vstorei128a(dSpanPtr, cPix0);

        dSpanPtr += 4;
      BL_SIMD_LOOP_32x4_MAIN_END(Loop)

      c0 = c1;
    }
  } while (++sIndex < sSize);

  // The last stop doesn't have to end at 1.0, in such case the remaining space
  // is filled by the last color stop (premultiplied).
  {
    I128 cA;
    i = uint32_t((size_t)((dPtr + dSize + 1) - dSpanPtr));

    c0 = vloadi128_h64(c0, &sPtr[0].rgba);
    c0 = vsrli16<8>(c0);

    cA = vswizi16<3, 3, 3, 3>(c0);
    c0 = vor(c0, argb64_a255);
    c0 = vdiv255u16(vmuli16(c0, cA));
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
