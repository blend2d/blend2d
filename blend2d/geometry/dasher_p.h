// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_GEOMETRY_DASHER_P_H_INCLUDED
#define BLEND2D_GEOMETRY_DASHER_P_H_INCLUDED

#include <blend2d/core/geometry_p.h>
#include <blend2d/core/path_p.h>
#include <blend2d/core/stroke_options.h>

//! \cond INTERNAL
//! \addtogroup blend2d_geometry
//! \{

namespace bl::Geometry {

//! Appends dashed figures to `output_path`, optionally calling `sink` after each completed input figure.
//! An empty dash array produces solid figures. The sink can clear the output after consuming it.
//! Dash phase restarts for each figure. Cubics and conics are approximated with quadratic splines using
//! `approx.simplify_tolerance` before dashing. All quadratics are split to limit their tangent rotation
//! to `atan(approx.offset_parameter)` before measuring their length.
BL_HIDDEN BLResult dash_path(
  const BLPathView& input_view,
  const BLStrokeOptions& options,
  const BLApproximationOptions& approx,
  BLPath& output_path,
  BLPathDashSinkFunc sink, void* user_data) noexcept;

} // {bl::Geometry}

//! \}
//! \endcond

#endif // BLEND2D_GEOMETRY_DASHER_P_H_INCLUDED
