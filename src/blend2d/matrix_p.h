// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_MATRIX_P_H_INCLUDED
#define BLEND2D_MATRIX_P_H_INCLUDED

#include "./api-internal_p.h"
#include "./math_p.h"
#include "./matrix.h"

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

//! Returns a matrix rotation angle.
static BL_INLINE double blMatrix2DRotationAngle(const BLMatrix2D& m) noexcept {
  return blAtan2(m.m00, m.m01);
}

//! Returns an average scaling (by X and Y).
//!
//! Basically used to calculate the approximation scale when decomposing
//! curves into line segments.
static BL_INLINE double blMatrix2DAverageScaling(const BLMatrix2D& m) noexcept {
  double x = m.m00 + m.m10;
  double y = m.m01 + m.m11;
  return blSqrt((blSquare(x) + blSquare(y)) * 0.5);
}

//! Returns absolute scaling of `m`.
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

// ============================================================================
// [BLMatrix2D - Runtime Initialization]
// ============================================================================

#ifdef BL_BUILD_OPT_SSE2
BL_HIDDEN void blMatrix2DOnInit_SSE2(BLRuntimeContext* rt) noexcept;
#endif

#ifdef BL_BUILD_OPT_AVX
BL_HIDDEN void blMatrix2DOnInit_AVX(BLRuntimeContext* rt) noexcept;
#endif

//! \}
//! \endcond

#endif // BLEND2D_MATRIX_P_H_INCLUDED
