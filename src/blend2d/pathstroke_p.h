// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PATHSTROKE_P_H_INCLUDED
#define BLEND2D_PATHSTROKE_P_H_INCLUDED

#include "api-internal_p.h"
#include "geometry_p.h"
#include "path_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {
namespace PathInternal {

BL_HIDDEN BLResult strokePath(
  const BLPathView& input,
  const BLStrokeOptions& options,
  const BLApproximationOptions& approx,
  BLPath& aPath,
  BLPath& bPath,
  BLPath& cPath,
  BLPathStrokeSinkFunc sink, void* userData) noexcept;

} // {PathInternal}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PATHSTROKE_P_H_INCLUDED
