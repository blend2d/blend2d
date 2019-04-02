// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLPATHSTROKE_P_H
#define BLEND2D_BLPATHSTROKE_P_H

#include "./blapi-internal_p.h"
#include "./blgeometry_p.h"
#include "./blpath_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! This is a sink that is used by path offsetting. This sink consumes both
//! `a` and `b` offsets of the path. The sink will be called for each figure
//! and is responsible for joining these paths. If the paths are not closed
//! then the sink must insert start cap, then join `b`, and then insert end
//! cap.
//!
//! The sink must also clean up the paths as this is not done by the offseter.
//! The reason is that in case the `a` path is the output path you can just
//! keep it and insert `b` path into it (clearing only `b` path after each
//! call).
typedef BLResult (BL_CDECL* BLPathStrokeSinkFunc)(BLPath* a, BLPath* b, BLPath* c, void* closure) BL_NOEXCEPT;

BL_HIDDEN BLResult blPathStrokeInternal(
  const BLPathView& input,
  const BLStrokeOptions& options,
  const BLApproximationOptions& approx,
  BLPath* aPath,
  BLPath* bPath,
  BLPath* cPath,
  BLPathStrokeSinkFunc sink, void* closure) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_BLPATHSTROKE_P_H
