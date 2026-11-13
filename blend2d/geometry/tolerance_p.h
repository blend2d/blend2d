// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_GEOMETRY_TOLERANCE_P_H_INCLUDED
#define BLEND2D_GEOMETRY_TOLERANCE_P_H_INCLUDED

#include <blend2d/core/geometry.h>
#include <blend2d/geometry/commons_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_geometry
//! \{

namespace bl::Geometry {

struct Tolerance {
  //! Tolerance parameter.
  double tolerance;
  //! Tolerance used to simplify conic curves.
  double tolerance_mul4;
  //! Tolerance used to simplify cubic curves.
  double tolerance_mul54;
};

static BL_INLINE Tolerance make_tolerance(const BLApproximationOptions& approx) noexcept {
  double tolerance = double(approx.simplify_tolerance);
  double tolerance_mul4 = tolerance * 4.0;
  double tolerance_mul54 = tolerance * 54.0;
  return Tolerance{tolerance, tolerance_mul4, tolerance_mul54};
}

} // {bl::Geometry}

//! \}
//! \endcond

#endif // BLEND2D_GEOMETRY_TOLERANCE_P_H_INCLUDED
