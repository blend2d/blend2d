// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

// The JPEG codec is based on stb_image <https://github.com/nothings/stb>
// released into PUBLIC DOMAIN. Blend2D's JPEG codec can be distributed
// under Blend2D's ZLIB license or under STB's PUBLIC DOMAIN as well.

#include "../blapi-build_p.h"
#ifdef BL_TARGET_OPT_SSE2

#include "../blrgba_p.h"
#include "../blsimd_p.h"
#include "../blsupport_p.h"
#include "../codec/bljpegops_p.h"

// ============================================================================
// [BLJpegOps - IDCT@SSE2]
// ============================================================================

struct alignas(16) BLJpegSSE2Constants {
  // IDCT.
  int16_t idct_rot0a[8], idct_rot0b[8];
  int16_t idct_rot1a[8], idct_rot1b[8];
  int16_t idct_rot2a[8], idct_rot2b[8];
  int16_t idct_rot3a[8], idct_rot3b[8];

  int32_t idct_col_bias[4];
  int32_t idct_row_bias[4];

  // YCbCr.
  int32_t ycbcr_allones[4];
  int16_t ycbcr_tosigned[8];
  int32_t ycbcr_round[4];
  int16_t ycbcr_yycrMul[8];
  int16_t ycbcr_yycbMul[8];
  int16_t ycbcr_cbcrMul[8];
};

#define DATA_4X(...) { __VA_ARGS__, __VA_ARGS__, __VA_ARGS__, __VA_ARGS__ }
static const BLJpegSSE2Constants blJpegSSE2Constants = {
  // IDCT.
  DATA_4X(BL_JPEG_IDCT_P_0_541196100                              ,
          BL_JPEG_IDCT_P_0_541196100 + BL_JPEG_IDCT_M_1_847759065),
  DATA_4X(BL_JPEG_IDCT_P_0_541196100 + BL_JPEG_IDCT_P_0_765366865 ,
          BL_JPEG_IDCT_P_0_541196100                             ),
  DATA_4X(BL_JPEG_IDCT_P_1_175875602 + BL_JPEG_IDCT_M_0_899976223 ,
          BL_JPEG_IDCT_P_1_175875602                             ),
  DATA_4X(BL_JPEG_IDCT_P_1_175875602                              ,
          BL_JPEG_IDCT_P_1_175875602 + BL_JPEG_IDCT_M_2_562915447),
  DATA_4X(BL_JPEG_IDCT_M_1_961570560 + BL_JPEG_IDCT_P_0_298631336 ,
          BL_JPEG_IDCT_M_1_961570560                             ),
  DATA_4X(BL_JPEG_IDCT_M_1_961570560                              ,
          BL_JPEG_IDCT_M_1_961570560 + BL_JPEG_IDCT_P_3_072711026),
  DATA_4X(BL_JPEG_IDCT_M_0_390180644 + BL_JPEG_IDCT_P_2_053119869 ,
          BL_JPEG_IDCT_M_0_390180644                             ),
  DATA_4X(BL_JPEG_IDCT_M_0_390180644                              ,
          BL_JPEG_IDCT_M_0_390180644 + BL_JPEG_IDCT_P_1_501321110),

  DATA_4X(BL_JPEG_IDCT_COL_BIAS),
  DATA_4X(BL_JPEG_IDCT_ROW_BIAS),

  // YCbCr.
  DATA_4X(-1),
  DATA_4X(-128, -128),
  DATA_4X(1 << (BL_JPEG_YCBCR_PREC - 1)),
  DATA_4X( BL_JPEG_YCBCR_FIXED(1.00000),  BL_JPEG_YCBCR_FIXED(1.40200)),
  DATA_4X( BL_JPEG_YCBCR_FIXED(1.00000),  BL_JPEG_YCBCR_FIXED(1.77200)),
  DATA_4X(-BL_JPEG_YCBCR_FIXED(0.34414), -BL_JPEG_YCBCR_FIXED(0.71414))
};
#undef DATA_4X

#define BL_JPEG_CONST_XMM(C) (*(const I128*)(blJpegSSE2Constants.C))

#define BL_JPEG_IDCT_INTERLEAVE8_XMM(a, b) { I128 t = a; a = _mm_unpacklo_epi8(a, b); b = _mm_unpackhi_epi8(t, b); }
#define BL_JPEG_IDCT_INTERLEAVE16_XMM(a, b) { I128 t = a; a = _mm_unpacklo_epi16(a, b); b = _mm_unpackhi_epi16(t, b); }

// out(0) = c0[even]*x + c0[odd]*y (in 16-bit, out 32-bit).
// out(1) = c1[even]*x + c1[odd]*y (in 16-bit, out 32-bit).
#define BL_JPEG_IDCT_ROTATE_XMM(dst0, dst1, x, y, c0, c1) \
  I128 c0##_l = _mm_unpacklo_epi16(x, y); \
  I128 c0##_h = _mm_unpackhi_epi16(x, y); \
  \
  I128 dst0##_l = _mm_madd_epi16(c0##_l, BL_JPEG_CONST_XMM(c0)); \
  I128 dst0##_h = _mm_madd_epi16(c0##_h, BL_JPEG_CONST_XMM(c0)); \
  I128 dst1##_l = _mm_madd_epi16(c0##_l, BL_JPEG_CONST_XMM(c1)); \
  I128 dst1##_h = _mm_madd_epi16(c0##_h, BL_JPEG_CONST_XMM(c1));

// out = in << 12 (in 16-bit, out 32-bit)
#define BL_JPEG_IDCT_WIDEN_XMM(dst, in) \
  I128 dst##_l = vsrai32<4>(_mm_unpacklo_epi16(vzeroi128(), in)); \
  I128 dst##_h = vsrai32<4>(_mm_unpackhi_epi16(vzeroi128(), in));

// Wide add (32-bit).
#define BL_JPEG_IDCT_WADD_XMM(dst, a, b) \
  I128 dst##_l = vaddi32(a##_l, b##_l); \
  I128 dst##_h = vaddi32(a##_h, b##_h);

// Wide sub (32-bit).
#define BL_JPEG_IDCT_WSUB_XMM(dst, a, b) \
  I128 dst##_l = vsubi32(a##_l, b##_l); \
  I128 dst##_h = vsubi32(a##_h, b##_h);

// Butterfly a/b, add bias, then shift by `norm` and pack to 16-bit.
#define BL_JPEG_IDCT_BFLY_XMM(dst0, dst1, a, b, bias, norm) { \
  I128 abiased_l = vaddi32(a##_l, bias); \
  I128 abiased_h = vaddi32(a##_h, bias); \
  \
  BL_JPEG_IDCT_WADD_XMM(sum, abiased, b) \
  BL_JPEG_IDCT_WSUB_XMM(diff, abiased, b) \
  \
  dst0 = vpacki32i16(vsrai32<norm>(sum_l), vsrai32<norm>(sum_h)); \
  dst1 = vpacki32i16(vsrai32<norm>(diff_l), vsrai32<norm>(diff_h)); \
}

#define BL_JPEG_IDCT_IDCT_PASS_XMM(bias, norm) { \
  /* Even part. */ \
  BL_JPEG_IDCT_ROTATE_XMM(t2e, t3e, row2, row6, idct_rot0a, idct_rot0b) \
  \
  I128 sum04 = _mm_add_epi16(row0, row4); \
  I128 dif04 = _mm_sub_epi16(row0, row4); \
  \
  BL_JPEG_IDCT_WIDEN_XMM(t0e, sum04) \
  BL_JPEG_IDCT_WIDEN_XMM(t1e, dif04) \
  \
  BL_JPEG_IDCT_WADD_XMM(x0, t0e, t3e) \
  BL_JPEG_IDCT_WSUB_XMM(x3, t0e, t3e) \
  BL_JPEG_IDCT_WADD_XMM(x1, t1e, t2e) \
  BL_JPEG_IDCT_WSUB_XMM(x2, t1e, t2e) \
  \
  /* Odd part */ \
  BL_JPEG_IDCT_ROTATE_XMM(y0o, y2o, row7, row3, idct_rot2a, idct_rot2b) \
  BL_JPEG_IDCT_ROTATE_XMM(y1o, y3o, row5, row1, idct_rot3a, idct_rot3b) \
  I128 sum17 = _mm_add_epi16(row1, row7); \
  I128 sum35 = _mm_add_epi16(row3, row5); \
  BL_JPEG_IDCT_ROTATE_XMM(y4o,y5o, sum17, sum35, idct_rot1a, idct_rot1b) \
  \
  BL_JPEG_IDCT_WADD_XMM(x4, y0o, y4o) \
  BL_JPEG_IDCT_WADD_XMM(x5, y1o, y5o) \
  BL_JPEG_IDCT_WADD_XMM(x6, y2o, y5o) \
  BL_JPEG_IDCT_WADD_XMM(x7, y3o, y4o) \
  \
  BL_JPEG_IDCT_BFLY_XMM(row0, row7, x0, x7, bias, norm) \
  BL_JPEG_IDCT_BFLY_XMM(row1, row6, x1, x6, bias, norm) \
  BL_JPEG_IDCT_BFLY_XMM(row2, row5, x2, x5, bias, norm) \
  BL_JPEG_IDCT_BFLY_XMM(row3, row4, x3, x4, bias, norm) \
}

void BL_CDECL blJpegIDCT8_SSE2(uint8_t* dst, intptr_t dstStride, const int16_t* src, const uint16_t* qTable) noexcept {
  using namespace SIMD;

  // Load and dequantize (the inputs are aligned to 16 bytes so this is safe).
  I128 row0 = vmuli16(*(const I128*)(src +  0), *(const I128*)(qTable +  0));
  I128 row1 = vmuli16(*(const I128*)(src +  8), *(const I128*)(qTable +  8));
  I128 row2 = vmuli16(*(const I128*)(src + 16), *(const I128*)(qTable + 16));
  I128 row3 = vmuli16(*(const I128*)(src + 24), *(const I128*)(qTable + 24));
  I128 row4 = vmuli16(*(const I128*)(src + 32), *(const I128*)(qTable + 32));
  I128 row5 = vmuli16(*(const I128*)(src + 40), *(const I128*)(qTable + 40));
  I128 row6 = vmuli16(*(const I128*)(src + 48), *(const I128*)(qTable + 48));
  I128 row7 = vmuli16(*(const I128*)(src + 56), *(const I128*)(qTable + 56));

  // IDCT columns.
  BL_JPEG_IDCT_IDCT_PASS_XMM(BL_JPEG_CONST_XMM(idct_col_bias), BL_JPEG_IDCT_COL_NORM)

  // Transpose.
  BL_JPEG_IDCT_INTERLEAVE16_XMM(row0, row4) // [a0a4|b0b4|c0c4|d0d4] | [e0e4|f0f4|g0g4|h0h4]
  BL_JPEG_IDCT_INTERLEAVE16_XMM(row2, row6) // [a2a6|b2b6|c2c6|d2d6] | [e2e6|f2f6|g2g6|h2h6]
  BL_JPEG_IDCT_INTERLEAVE16_XMM(row1, row5) // [a1a5|b1b5|c2c5|d1d5] | [e1e5|f1f5|g1g5|h1h5]
  BL_JPEG_IDCT_INTERLEAVE16_XMM(row3, row7) // [a3a7|b3b7|c3c7|d3d7] | [e3e7|f3f7|g3g7|h3h7]

  BL_JPEG_IDCT_INTERLEAVE16_XMM(row0, row2) // [a0a2|a4a6|b0b2|b4b6] | [c0c2|c4c6|d0d2|d4d6]
  BL_JPEG_IDCT_INTERLEAVE16_XMM(row1, row3) // [a1a3|a5a7|b1b3|b5b7] | [c1c3|c5c7|d1d3|d5d7]
  BL_JPEG_IDCT_INTERLEAVE16_XMM(row4, row6) // [e0e2|e4e6|f0f2|f4f6] | [g0g2|g4g6|h0h2|h4h6]
  BL_JPEG_IDCT_INTERLEAVE16_XMM(row5, row7) // [e1e3|e5e7|f1f3|f5f7] | [g1g3|g5g7|h1h3|h5h7]

  BL_JPEG_IDCT_INTERLEAVE16_XMM(row0, row1) // [a0a1|a2a3|a4a5|a6a7] | [b0b1|b2b3|b4b5|b6b7]
  BL_JPEG_IDCT_INTERLEAVE16_XMM(row2, row3) // [c0c1|c2c3|c4c5|c6c7] | [d0d1|d2d3|d4d5|d6d7]
  BL_JPEG_IDCT_INTERLEAVE16_XMM(row4, row5) // [e0e1|e2e3|e4e5|e6e7] | [f0f1|f2f3|f4f5|f6f7]
  BL_JPEG_IDCT_INTERLEAVE16_XMM(row6, row7) // [g0g1|g2g3|g4g5|g6g7] | [h0h1|h2h3|h4h5|h6h7]

  // IDCT rows.
  BL_JPEG_IDCT_IDCT_PASS_XMM(BL_JPEG_CONST_XMM(idct_row_bias), BL_JPEG_IDCT_ROW_NORM)

  // Pack to 8-bit unsigned integers with saturation.
  row0 = vpacki16u8(row0, row1);        // [a0a1a2a3|a4a5a6a7|b0b1b2b3|b4b5b6b7]
  row2 = vpacki16u8(row2, row3);        // [c0c1c2c3|c4c5c6c7|d0d1d2d3|d4d5d6d7]
  row4 = vpacki16u8(row4, row5);        // [e0e1e2e3|e4e5e6e7|f0f1f2f3|f4f5f6f7]
  row6 = vpacki16u8(row6, row7);        // [g0g1g2g3|g4g5g6g7|h0h1h2h3|h4h5h6h7]

  // Transpose.
  BL_JPEG_IDCT_INTERLEAVE8_XMM(row0, row4)  // [a0e0a1e1|a2e2a3e3|a4e4a5e5|a6e6a7e7] | [b0f0b1f1|b2f2b3f3|b4f4b5f5|b6f6b7f7]
  BL_JPEG_IDCT_INTERLEAVE8_XMM(row2, row6)  // [c0g0c1g1|c2g2c3g3|c4g4c5g5|c6g6c7g7] | [d0h0d1h1|d2h2d3h3|d4h4d5h5|d6h6d7h7]
  BL_JPEG_IDCT_INTERLEAVE8_XMM(row0, row2)  // [a0c0e0g0|a1c1e1g1|a2c2e2g2|a3c3e3g3] | [a4c4e4g4|a5c5e5g5|a6c6e6g6|a7c7e7g7]
  BL_JPEG_IDCT_INTERLEAVE8_XMM(row4, row6)  // [b0d0f0h0|b1d1f1h1|b2d2f2h2|b3d3f3h3| | [b4d4f4h4|b5d5f5h5|b6d6f6h6|b7d7f7h7]
  BL_JPEG_IDCT_INTERLEAVE8_XMM(row0, row4)  // [a0b0c0d0|e0f0g0h0|a1b1c1d1|e1f1g1h1] | [a2b2c2d2|e2f2g2h2|a3b3c3d3|e3f3g3h3]
  BL_JPEG_IDCT_INTERLEAVE8_XMM(row2, row6)  // [a4b4c4d4|e4f4g4h4|a5b5c5d5|e5f5g5h5] | [a6b6c6d6|e6f6g6h6|a7b7c7d7|e7f7g7h7]

  // Store.
  uint8_t* dst0 = dst;
  uint8_t* dst1 = dst + dstStride;
  intptr_t dstStride2 = dstStride * 2;

  vstoreli64(dst0, row0); dst0 += dstStride2;
  vstorehi64(dst1, row0); dst1 += dstStride2;

  vstoreli64(dst0, row4); dst0 += dstStride2;
  vstorehi64(dst1, row4); dst1 += dstStride2;

  vstoreli64(dst0, row2); dst0 += dstStride2;
  vstorehi64(dst1, row2); dst1 += dstStride2;

  vstoreli64(dst0, row6);
  vstorehi64(dst1, row6);
}

// ============================================================================
// [BLJpegOps - RGB32FromYCbCr8@SSE2]
// ============================================================================

void BL_CDECL blJpegRGB32FromYCbCr8_SSE2(uint8_t* dst, const uint8_t* pY, const uint8_t* pCb, const uint8_t* pCr, uint32_t count) noexcept {
  using namespace SIMD;
  uint32_t i = count;

  while (i >= 8) {
    I128 yy = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pY));
    I128 cb = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pCb));
    I128 cr = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pCr));

    yy = vmovli64u8u16(yy);
    cb = vaddi16(vmovli64u8u16(cb), BL_JPEG_CONST_XMM(ycbcr_tosigned));
    cr = vaddi16(vmovli64u8u16(cr), BL_JPEG_CONST_XMM(ycbcr_tosigned));

    I128 r_l = _mm_madd_epi16(_mm_unpacklo_epi16(yy, cr), BL_JPEG_CONST_XMM(ycbcr_yycrMul));
    I128 r_h = _mm_madd_epi16(_mm_unpackhi_epi16(yy, cr), BL_JPEG_CONST_XMM(ycbcr_yycrMul));

    I128 b_l = _mm_madd_epi16(_mm_unpacklo_epi16(yy, cb), BL_JPEG_CONST_XMM(ycbcr_yycbMul));
    I128 b_h = _mm_madd_epi16(_mm_unpackhi_epi16(yy, cb), BL_JPEG_CONST_XMM(ycbcr_yycbMul));

    I128 g_l = _mm_madd_epi16(_mm_unpacklo_epi16(cb, cr), BL_JPEG_CONST_XMM(ycbcr_cbcrMul));
    I128 g_h = _mm_madd_epi16(_mm_unpackhi_epi16(cb, cr), BL_JPEG_CONST_XMM(ycbcr_cbcrMul));

    g_l = vaddi32(g_l, vslli32<BL_JPEG_YCBCR_PREC>(vmovli64u16u32(yy)));
    g_h = vaddi32(g_h, vslli32<BL_JPEG_YCBCR_PREC>(vmovhi64u16u32(yy)));

    r_l = vaddi32(r_l, BL_JPEG_CONST_XMM(ycbcr_round));
    r_h = vaddi32(r_h, BL_JPEG_CONST_XMM(ycbcr_round));

    b_l = vaddi32(b_l, BL_JPEG_CONST_XMM(ycbcr_round));
    b_h = vaddi32(b_h, BL_JPEG_CONST_XMM(ycbcr_round));

    g_l = vaddi32(g_l, BL_JPEG_CONST_XMM(ycbcr_round));
    g_h = vaddi32(g_h, BL_JPEG_CONST_XMM(ycbcr_round));

    r_l = vsrai32<BL_JPEG_YCBCR_PREC>(r_l);
    r_h = vsrai32<BL_JPEG_YCBCR_PREC>(r_h);

    b_l = vsrai32<BL_JPEG_YCBCR_PREC>(b_l);
    b_h = vsrai32<BL_JPEG_YCBCR_PREC>(b_h);

    g_l = vsrai32<BL_JPEG_YCBCR_PREC>(g_l);
    g_h = vsrai32<BL_JPEG_YCBCR_PREC>(g_h);

    __m128i r = vpackzzdb(r_l, r_h);
    __m128i g = vpackzzdb(g_l, g_h);
    __m128i b = vpackzzdb(b_l, b_h);

    __m128i ra = vunpackli8(r, BL_JPEG_CONST_XMM(ycbcr_allones));
    __m128i bg = vunpackli8(b, g);

    __m128i bgra0 = vunpackli16(bg, ra);
    __m128i bgra1 = vunpackhi16(bg, ra);

    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst +  0), bgra0);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), bgra1);

    dst += 32;
    pY  += 8;
    pCb += 8;
    pCr += 8;
    i   -= 8;
  }

  while (i) {
    int yy = (int(pY[0]) << BL_JPEG_YCBCR_PREC) + (1 << (BL_JPEG_YCBCR_PREC - 1));
    int cr = int(pCr[0]) - 128;
    int cb = int(pCb[0]) - 128;

    int r = yy + cr * BL_JPEG_YCBCR_FIXED(1.40200);
    int g = yy - cr * BL_JPEG_YCBCR_FIXED(0.71414) - cb * BL_JPEG_YCBCR_FIXED(0.34414);
    int b = yy + cb * BL_JPEG_YCBCR_FIXED(1.77200);

    uint32_t rgba32 = blRgba32Pack(
      blClampToByte(r >> BL_JPEG_YCBCR_PREC),
      blClampToByte(g >> BL_JPEG_YCBCR_PREC),
      blClampToByte(b >> BL_JPEG_YCBCR_PREC));
    blMemWriteU32a(dst, rgba32);

    dst += 4;
    pY  += 1;
    pCb += 1;
    pCr += 1;
    i   -= 1;
  }
}

#endif
