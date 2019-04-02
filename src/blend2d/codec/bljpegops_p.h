// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_CODEC_BLJPEGOPS_P_H
#define BLEND2D_CODEC_BLJPEGOPS_P_H

#include "../blapi-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_codecs
//! \{

// ============================================================================
// [BLJpegOps - Macros]
// ============================================================================

// Derived from jidctint's `jpeg_idct_islow`.
#define BL_JPEG_IDCT_PREC 12
#define BL_JPEG_IDCT_HALF(precision) (1 << ((precision) - 1))

#define BL_JPEG_IDCT_SCALE(x) ((x) << BL_JPEG_IDCT_PREC)
#define BL_JPEG_IDCT_FIXED(x) int(double(x) * double(1 << BL_JPEG_IDCT_PREC) + 0.5)

#define BL_JPEG_IDCT_M_2_562915447 (-BL_JPEG_IDCT_FIXED(2.562915447))
#define BL_JPEG_IDCT_M_1_961570560 (-BL_JPEG_IDCT_FIXED(1.961570560))
#define BL_JPEG_IDCT_M_1_847759065 (-BL_JPEG_IDCT_FIXED(1.847759065))
#define BL_JPEG_IDCT_M_0_899976223 (-BL_JPEG_IDCT_FIXED(0.899976223))
#define BL_JPEG_IDCT_M_0_390180644 (-BL_JPEG_IDCT_FIXED(0.390180644))
#define BL_JPEG_IDCT_P_0_298631336 ( BL_JPEG_IDCT_FIXED(0.298631336))
#define BL_JPEG_IDCT_P_0_541196100 ( BL_JPEG_IDCT_FIXED(0.541196100))
#define BL_JPEG_IDCT_P_0_765366865 ( BL_JPEG_IDCT_FIXED(0.765366865))
#define BL_JPEG_IDCT_P_1_175875602 ( BL_JPEG_IDCT_FIXED(1.175875602))
#define BL_JPEG_IDCT_P_1_501321110 ( BL_JPEG_IDCT_FIXED(1.501321110))
#define BL_JPEG_IDCT_P_2_053119869 ( BL_JPEG_IDCT_FIXED(2.053119869))
#define BL_JPEG_IDCT_P_3_072711026 ( BL_JPEG_IDCT_FIXED(3.072711026))

// Keep 2 bits of extra precision for the intermediate results.
#define BL_JPEG_IDCT_COL_NORM (BL_JPEG_IDCT_PREC - 2)
#define BL_JPEG_IDCT_COL_BIAS BL_JPEG_IDCT_HALF(BL_JPEG_IDCT_COL_NORM)

// Consume 2 bits of an intermediate results precision and 3 bits that were
// produced by `2 * sqrt(8)`. Also normalize to from `-128..127` to `0..255`.
#define BL_JPEG_IDCT_ROW_NORM (BL_JPEG_IDCT_PREC + 2 + 3)
#define BL_JPEG_IDCT_ROW_BIAS (BL_JPEG_IDCT_HALF(BL_JPEG_IDCT_ROW_NORM) + (128 << BL_JPEG_IDCT_ROW_NORM))

#define BL_JPEG_YCBCR_PREC 12
#define BL_JPEG_YCBCR_SCALE(x) ((x) << BL_JPEG_YCBCR_PREC)
#define BL_JPEG_YCBCR_FIXED(x) int(double(x) * double(1 << BL_JPEG_YCBCR_PREC) + 0.5)

// ============================================================================
// [BLJpegOps - Globals]
// ============================================================================

//! Optimized JPEG functions.
struct BLJpegOps {
  //! Dequantize and perform IDCT and store clamped 8-bit results to `dst`.
  void (BL_CDECL* idct8)(uint8_t* dst, intptr_t dstStride, const int16_t* src, const uint16_t* qTable) BL_NOEXCEPT;

  //! No upsampling (stub).
  uint8_t* (BL_CDECL* upsample1x1)(uint8_t* dst, uint8_t* src0, uint8_t* src1, uint32_t w, uint32_t hs) BL_NOEXCEPT;
  //! Upsample row in vertical direction.
  uint8_t* (BL_CDECL* upsample1x2)(uint8_t* dst, uint8_t* src0, uint8_t* src1, uint32_t w, uint32_t hs) BL_NOEXCEPT;
  //! Upsample row in horizontal direction.
  uint8_t* (BL_CDECL* upsample2x1)(uint8_t* dst, uint8_t* src0, uint8_t* src1, uint32_t w, uint32_t hs) BL_NOEXCEPT;
  //! Upsample row in vertical and horizontal direction.
  uint8_t* (BL_CDECL* upsample2x2)(uint8_t* dst, uint8_t* src0, uint8_t* src1, uint32_t w, uint32_t hs) BL_NOEXCEPT;
  //! Upsample row any.
  uint8_t* (BL_CDECL* upsampleAny)(uint8_t* dst, uint8_t* src0, uint8_t* src1, uint32_t w, uint32_t hs) BL_NOEXCEPT;

  //! Perform planar YCbCr to RGB conversion and pack to XRGB32.
  void (BL_CDECL* convYCbCr8ToRGB32)(uint8_t* dst, const uint8_t* pY, const uint8_t* pCb, const uint8_t* pCr, uint32_t count) BL_NOEXCEPT;
};
extern BLJpegOps blJpegOps;

// ============================================================================
// [BLJpegOps - Base]
// ============================================================================

BL_HIDDEN void BL_CDECL blJpegIDCT8(uint8_t* dst, intptr_t dstStride, const int16_t* src, const uint16_t* qTable) noexcept;
BL_HIDDEN void BL_CDECL blJpegRGB32FromYCbCr8(uint8_t* dst, const uint8_t* pY, const uint8_t* pCb, const uint8_t* pCr, uint32_t count) noexcept;

BL_HIDDEN uint8_t* BL_CDECL blJpegUpsample1x1(uint8_t* dst, uint8_t* src0, uint8_t* src1, uint32_t w, uint32_t hs) noexcept;
BL_HIDDEN uint8_t* BL_CDECL blJpegUpsample1x2(uint8_t* dst, uint8_t* src0, uint8_t* src1, uint32_t w, uint32_t hs) noexcept;
BL_HIDDEN uint8_t* BL_CDECL blJpegUpsample2x1(uint8_t* dst, uint8_t* src0, uint8_t* src1, uint32_t w, uint32_t hs) noexcept;
BL_HIDDEN uint8_t* BL_CDECL blJpegUpsample2x2(uint8_t* dst, uint8_t* src0, uint8_t* src1, uint32_t w, uint32_t hs) noexcept;
BL_HIDDEN uint8_t* BL_CDECL blJpegUpsampleAny(uint8_t* dst, uint8_t* src0, uint8_t* src1, uint32_t w, uint32_t hs) noexcept;

// ============================================================================
// [BLJpegOps - SSE2]
// ============================================================================

#ifdef BL_BUILD_OPT_SSE2
BL_HIDDEN void BL_CDECL blJpegIDCT8_SSE2(uint8_t* dst, intptr_t dstStride, const int16_t* src, const uint16_t* qTable) noexcept;
BL_HIDDEN void BL_CDECL blJpegRGB32FromYCbCr8_SSE2(uint8_t* dst, const uint8_t* pY, const uint8_t* pCb, const uint8_t* pCr, uint32_t count) noexcept;
#endif

//! \}
//! \endcond

#endif // BLEND2D_CODEC_BLJPEGOPS_P_H
