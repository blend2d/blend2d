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

#ifndef BLEND2D_PATHSTROKE_P_H_INCLUDED
#define BLEND2D_PATHSTROKE_P_H_INCLUDED

#include "./api-internal_p.h"
#include "./geometry_p.h"
#include "./path_p.h"

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

#endif // BLEND2D_PATHSTROKE_P_H_INCLUDED
