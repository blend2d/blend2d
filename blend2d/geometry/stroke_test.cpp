// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/axl/mathconst_p.h>
#include <blend2d/core/path_p.h>
#include <blend2d/geometry/bezier_p.h>
#include <blend2d/geometry/stroke_p.h>

// Geometry - Bezier - Tests
// =========================

namespace bl::Geometry::Tests {

TEST_CASE(geometry_stroke_pathological) {
  BLPath path;
  path.move_to(111.600000, 80.400000);
  path.line_to(111.900000, 80.100000);
  path.cubic_to(111.700000,80.000000,111.300000,79.700000,111.100000,79.700000);
  path.line_to(111.600000, 80.400000);

  BLStrokeOptions stroke_options;
  stroke_options.set_width(0.1f);

  BLPath stroked;
  EXPECT_OK(stroked.add_stroked_path(path, stroke_options, bl_default_approximation_options));
  EXPECT_LE(stroked.vertex_count(), 40u);
}

} // {bl::Geometry::Tests}

#endif // BL_TEST
