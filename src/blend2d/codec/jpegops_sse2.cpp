// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// The JPEG codec is based on stb_image <https://github.com/nothings/stb>
// released into PUBLIC DOMAIN. Blend2D's JPEG codec can be distributed
// under Blend2D's ZLIB license or under STB's PUBLIC DOMAIN as well.

#include "../api-build_p.h"
#ifdef BL_TARGET_OPT_SSE2

#include "../rgba_p.h"
#include "../simd_p.h"
#include "../codec/jpegops_p.h"
#include "../support/intops_p.h"
#include "../support/memops_p.h"

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

#define BL_JPEG_CONST_XMM(C) (*(const Vec128I*)(blJpegSSE2Constants.C))

#define BL_JPEG_IDCT_INTERLEAVE8_XMM(a, b) { Vec128I t = a; a = _mm_unpacklo_epi8(a, b); b = _mm_unpackhi_epi8(t, b); }
#define BL_JPEG_IDCT_INTERLEAVE16_XMM(a, b) { Vec128I t = a; a = _mm_unpacklo_epi16(a, b); b = _mm_unpackhi_epi16(t, b); }

// out(0) = c0[even]*x + c0[odd]*y (in 16-bit, out 32-bit).
// out(1) = c1[even]*x + c1[odd]*y (in 16-bit, out 32-bit).
#define BL_JPEG_IDCT_ROTATE_XMM(dst0, dst1, x, y, c0, c1) \
  Vec128I c0##_l = _mm_unpacklo_epi16(x, y); \
  Vec128I c0##_h = _mm_unpackhi_epi16(x, y); \
  \
  Vec128I dst0##_l = v_madd_i16_i32(c0##_l, BL_JPEG_CONST_XMM(c0)); \
  Vec128I dst0##_h = v_madd_i16_i32(c0##_h, BL_JPEG_CONST_XMM(c0)); \
  Vec128I dst1##_l = v_madd_i16_i32(c0##_l, BL_JPEG_CONST_XMM(c1)); \
  Vec128I dst1##_h = v_madd_i16_i32(c0##_h, BL_JPEG_CONST_XMM(c1));

// out = in << 12 (in 16-bit, out 32-bit)
#define BL_JPEG_IDCT_WIDEN_XMM(dst, in) \
  Vec128I dst##_l = v_sra_i32<4>(_mm_unpacklo_epi16(v_zero_i128(), in)); \
  Vec128I dst##_h = v_sra_i32<4>(_mm_unpackhi_epi16(v_zero_i128(), in));

// Wide add (32-bit).
#define BL_JPEG_IDCT_WADD_XMM(dst, a, b) \
  Vec128I dst##_l = v_add_i32(a##_l, b##_l); \
  Vec128I dst##_h = v_add_i32(a##_h, b##_h);

// Wide sub (32-bit).
#define BL_JPEG_IDCT_WSUB_XMM(dst, a, b) \
  Vec128I dst##_l = v_sub_i32(a##_l, b##_l); \
  Vec128I dst##_h = v_sub_i32(a##_h, b##_h);

// Butterfly a/b, add bias, then shift by `norm` and pack to 16-bit.
#define BL_JPEG_IDCT_BFLY_XMM(dst0, dst1, a, b, bias, norm) { \
  Vec128I abiased_l = v_add_i32(a##_l, bias); \
  Vec128I abiased_h = v_add_i32(a##_h, bias); \
  \
  BL_JPEG_IDCT_WADD_XMM(sum, abiased, b) \
  BL_JPEG_IDCT_WSUB_XMM(diff, abiased, b) \
  \
  dst0 = v_packs_i32_i16(v_sra_i32<norm>(sum_l), v_sra_i32<norm>(sum_h)); \
  dst1 = v_packs_i32_i16(v_sra_i32<norm>(diff_l), v_sra_i32<norm>(diff_h)); \
}

#define BL_JPEG_IDCT_IDCT_PASS_XMM(bias, norm) { \
  /* Even part. */ \
  BL_JPEG_IDCT_ROTATE_XMM(t2e, t3e, row2, row6, idct_rot0a, idct_rot0b) \
  \
  Vec128I sum04 = _mm_add_epi16(row0, row4); \
  Vec128I dif04 = _mm_sub_epi16(row0, row4); \
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
  Vec128I sum17 = _mm_add_epi16(row1, row7); \
  Vec128I sum35 = _mm_add_epi16(row3, row5); \
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

  // Load and dequantize (`src` is aligned to 16 bytes, `qTable` doesn't have to be).
  Vec128I row0 = v_mul_i16(v_loadu_i128(qTable +  0), *(const Vec128I*)(src +  0));
  Vec128I row1 = v_mul_i16(v_loadu_i128(qTable +  8), *(const Vec128I*)(src +  8));
  Vec128I row2 = v_mul_i16(v_loadu_i128(qTable + 16), *(const Vec128I*)(src + 16));
  Vec128I row3 = v_mul_i16(v_loadu_i128(qTable + 24), *(const Vec128I*)(src + 24));
  Vec128I row4 = v_mul_i16(v_loadu_i128(qTable + 32), *(const Vec128I*)(src + 32));
  Vec128I row5 = v_mul_i16(v_loadu_i128(qTable + 40), *(const Vec128I*)(src + 40));
  Vec128I row6 = v_mul_i16(v_loadu_i128(qTable + 48), *(const Vec128I*)(src + 48));
  Vec128I row7 = v_mul_i16(v_loadu_i128(qTable + 56), *(const Vec128I*)(src + 56));

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
  row0 = v_packs_i16_u8(row0, row1);        // [a0a1a2a3|a4a5a6a7|b0b1b2b3|b4b5b6b7]
  row2 = v_packs_i16_u8(row2, row3);        // [c0c1c2c3|c4c5c6c7|d0d1d2d3|d4d5d6d7]
  row4 = v_packs_i16_u8(row4, row5);        // [e0e1e2e3|e4e5e6e7|f0f1f2f3|f4f5f6f7]
  row6 = v_packs_i16_u8(row6, row7);        // [g0g1g2g3|g4g5g6g7|h0h1h2h3|h4h5h6h7]

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

  v_storel_i64(dst0, row0); dst0 += dstStride2;
  v_storeh_i64(dst1, row0); dst1 += dstStride2;

  v_storel_i64(dst0, row4); dst0 += dstStride2;
  v_storeh_i64(dst1, row4); dst1 += dstStride2;

  v_storel_i64(dst0, row2); dst0 += dstStride2;
  v_storeh_i64(dst1, row2); dst1 += dstStride2;

  v_storel_i64(dst0, row6);
  v_storeh_i64(dst1, row6);
}

// ============================================================================
// [BLJpegOps - RGB32FromYCbCr8@SSE2]
// ============================================================================

void BL_CDECL blJpegRGB32FromYCbCr8_SSE2(uint8_t* dst, const uint8_t* pY, const uint8_t* pCb, const uint8_t* pCr, uint32_t count) noexcept {
  using namespace SIMD;
  uint32_t i = count;

  while (i >= 8) {
    Vec128I yy = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pY));
    Vec128I cb = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pCb));
    Vec128I cr = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pCr));

    yy = v_unpack_lo_u8_u16(yy);
    cb = v_add_i16(v_unpack_lo_u8_u16(cb), BL_JPEG_CONST_XMM(ycbcr_tosigned));
    cr = v_add_i16(v_unpack_lo_u8_u16(cr), BL_JPEG_CONST_XMM(ycbcr_tosigned));

    Vec128I r_l = v_madd_i16_i32(_mm_unpacklo_epi16(yy, cr), BL_JPEG_CONST_XMM(ycbcr_yycrMul));
    Vec128I r_h = v_madd_i16_i32(_mm_unpackhi_epi16(yy, cr), BL_JPEG_CONST_XMM(ycbcr_yycrMul));

    Vec128I b_l = v_madd_i16_i32(_mm_unpacklo_epi16(yy, cb), BL_JPEG_CONST_XMM(ycbcr_yycbMul));
    Vec128I b_h = v_madd_i16_i32(_mm_unpackhi_epi16(yy, cb), BL_JPEG_CONST_XMM(ycbcr_yycbMul));

    Vec128I g_l = v_madd_i16_i32(_mm_unpacklo_epi16(cb, cr), BL_JPEG_CONST_XMM(ycbcr_cbcrMul));
    Vec128I g_h = v_madd_i16_i32(_mm_unpackhi_epi16(cb, cr), BL_JPEG_CONST_XMM(ycbcr_cbcrMul));

    g_l = v_add_i32(g_l, v_sll_i32<BL_JPEG_YCBCR_PREC>(v_unpack_lo_u16_u32(yy)));
    g_h = v_add_i32(g_h, v_sll_i32<BL_JPEG_YCBCR_PREC>(v_unpack_hi_u16_u32(yy)));

    r_l = v_add_i32(r_l, BL_JPEG_CONST_XMM(ycbcr_round));
    r_h = v_add_i32(r_h, BL_JPEG_CONST_XMM(ycbcr_round));

    b_l = v_add_i32(b_l, BL_JPEG_CONST_XMM(ycbcr_round));
    b_h = v_add_i32(b_h, BL_JPEG_CONST_XMM(ycbcr_round));

    g_l = v_add_i32(g_l, BL_JPEG_CONST_XMM(ycbcr_round));
    g_h = v_add_i32(g_h, BL_JPEG_CONST_XMM(ycbcr_round));

    r_l = v_sra_i32<BL_JPEG_YCBCR_PREC>(r_l);
    r_h = v_sra_i32<BL_JPEG_YCBCR_PREC>(r_h);

    b_l = v_sra_i32<BL_JPEG_YCBCR_PREC>(b_l);
    b_h = v_sra_i32<BL_JPEG_YCBCR_PREC>(b_h);

    g_l = v_sra_i32<BL_JPEG_YCBCR_PREC>(g_l);
    g_h = v_sra_i32<BL_JPEG_YCBCR_PREC>(g_h);

    __m128i r = v_packz_u32_u8(r_l, r_h);
    __m128i g = v_packz_u32_u8(g_l, g_h);
    __m128i b = v_packz_u32_u8(b_l, b_h);

    __m128i ra = v_interleave_lo_i8(r, BL_JPEG_CONST_XMM(ycbcr_allones));
    __m128i bg = v_interleave_lo_i8(b, g);

    __m128i bgra0 = v_interleave_lo_i16(bg, ra);
    __m128i bgra1 = v_interleave_hi_i16(bg, ra);

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

    uint32_t rgba32 = BLRgbaPrivate::packRgba32(
      BLIntOps::clampToByte(r >> BL_JPEG_YCBCR_PREC),
      BLIntOps::clampToByte(g >> BL_JPEG_YCBCR_PREC),
      BLIntOps::clampToByte(b >> BL_JPEG_YCBCR_PREC));
    BLMemOps::writeU32a(dst, rgba32);

    dst += 4;
    pY  += 1;
    pCb += 1;
    pCr += 1;
    i   -= 1;
  }
}

#endif
