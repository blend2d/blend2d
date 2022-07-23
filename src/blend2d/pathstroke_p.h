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

namespace BLPathPrivate {

//! This is a sink that is used by path offsetting. This sink consumes both `a` and `b` offsets of the path. The sink
//! will be called for each figure and is responsible for joining these paths. If the paths are not closed then the
//! sink must insert start cap, then join `b`, and then insert end cap.
//!
//! The sink must also clean up the paths as this is not done by the offseter. The reason is that in case the `a` path
//! is the output path you can just keep it and insert `b` path into it (clearing only `b` path after each call).
typedef BLResult (BL_CDECL* StrokeSinkFunc)(BLPath* a, BLPath* b, BLPath* c, void* closure) BL_NOEXCEPT;

BL_HIDDEN BLResult strokePath(
  const BLPathView& input,
  const BLStrokeOptions& options,
  const BLApproximationOptions& approx,
  BLPath& aPath,
  BLPath& bPath,
  BLPath& cPath,
  StrokeSinkFunc sink, void* closure) noexcept;

} // {BLPathPrivate}

//! \}
//! \endcond

#endif // BLEND2D_PATHSTROKE_P_H_INCLUDED
