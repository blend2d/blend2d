// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_MATRIX_P_H_INCLUDED
#define BLEND2D_MATRIX_P_H_INCLUDED

#include "api-internal_p.h"
#include "math_p.h"
#include "matrix.h"
#include "runtime_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLTransformPrivate {

BL_HIDDEN extern const BLMatrix2D identityTransform;

static BL_INLINE BLBox mapBox(const BLMatrix2D& m, const BLBox& src) noexcept {
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

static BL_INLINE void multiply(BLMatrix2D& dst, const BLMatrix2D& a, const BLMatrix2D& b) noexcept {
  dst.reset(a.m00 * b.m00 + a.m01 * b.m10,
            a.m00 * b.m01 + a.m01 * b.m11,
            a.m10 * b.m00 + a.m11 * b.m10,
            a.m10 * b.m01 + a.m11 * b.m11,
            a.m20 * b.m00 + a.m21 * b.m10 + b.m20,
            a.m20 * b.m01 + a.m21 * b.m11 + b.m21);
}

} // {BLTransformPrivate}

//! \}
//! \endcond

#endif // BLEND2D_MATRIX_P_H_INCLUDED
