// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLMATRIX_P_H
#define BLEND2D_BLMATRIX_P_H

#include "./blapi-internal_p.h"
#include "./blmath_p.h"
#include "./blmatrix.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLMatrix2D - Globals]
// ============================================================================

BL_HIDDEN extern const BLMatrix2D blMatrix2DIdentity;

// ============================================================================
// [BLMatrix2D - Utilities]
// ============================================================================

//! Get a matrix rotation angle.
static BL_INLINE double blMatrix2DRotationAngle(const BLMatrix2D& m) noexcept {
  return blAtan2(m.m00, m.m01);
}

//! Get an average scaling (by X and Y).
//!
//! Basically used to calculate the approximation scale when decomposing
//! curves into line segments.
static BL_INLINE double blMatrix2DAverageScaling(const BLMatrix2D& m) noexcept {
  double x = m.m00 + m.m10;
  double y = m.m01 + m.m11;
  return blSqrt((blSquare(x) + blSquare(y)) * 0.5);
}

//! Get an absolute scaling.
static BL_INLINE BLPoint blMatrix2DAbsoluteScaling(const BLMatrix2D& m) noexcept {
  return BLPoint(blHypot(m.m00, m.m10), blHypot(m.m01, m.m11));
}

static BL_INLINE BLBox blMatrix2DMapBox(const BLMatrix2D& m, const BLBox& src) noexcept {
  double x0a = src.x0 * m.m00;
  double y0a = src.y0 * m.m10;
  double x1a = src.x1 * m.m00;
  double y1a = src.y1 * m.m10;

  double x0b = src.x0 * m.m01;
  double y0b = src.y0 * m.m11;
  double x1b = src.x1 * m.m01;
  double y1b = src.y1 * m.m11;

  return BLBox(blMin(x0a, x1a) + blMin(y0a, y1a) + m.m20,
               blMin(x0b, x1b) + blMin(y0b, y1b) + m.m21,
               blMax(x0a, x1a) + blMax(y0a, y1a) + m.m20,
               blMax(x0b, x1b) + blMax(y0b, y1b) + m.m21);
}

static BL_INLINE void blMatrix2DMultiply(BLMatrix2D& dst, const BLMatrix2D& a, const BLMatrix2D& b) noexcept {
  dst.reset(a.m00 * b.m00 + a.m01 * b.m10,
            a.m00 * b.m01 + a.m01 * b.m11,
            a.m10 * b.m00 + a.m11 * b.m10,
            a.m10 * b.m01 + a.m11 * b.m11,
            a.m20 * b.m00 + a.m21 * b.m10 + b.m20,
            a.m20 * b.m01 + a.m21 * b.m11 + b.m21);
}

//! \}
//! \endcond

#endif // BLEND2D_BLMATRIX_P_H
