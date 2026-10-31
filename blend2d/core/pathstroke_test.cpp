// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api-build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/core/path_p.h>
#include <blend2d/support/math_p.h>

// bl::PathStroker - Tests
// =======================

namespace bl {
namespace Tests {

static BLBox stroked_bounding_box(const BLPath& path) noexcept {
  BLStrokeOptions options;
  options.width = 1.0;
  options.start_cap = BL_STROKE_CAP_ROUND;
  options.end_cap = BL_STROKE_CAP_ROUND;
  options.join = BL_STROKE_JOIN_ROUND;

  BLPath stroked;
  EXPECT_SUCCESS(stroked.add_stroked_path(path, options, bl_default_approximation_options));
  EXPECT_GT(stroked.size(), 0u);

  BLBox box;
  EXPECT_SUCCESS(stroked.get_bounding_box(&box));
  return box;
}

UNIT(path_stroke_flat_cubic_linear_cusp, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  // The cusps of a flat cubic are the roots of a quadratic whose leading coefficient `3 r1 - 3 r2 + r3` is zero when
  // the equation is linear. Computing them with the textbook formula divided by that zero and lost the cusp (#272).
  INFO("Testing whether a flat cubic with a linear cusp equation is stroked to its full extent");

  // P -> Q -> Q -> P reaches 3/4 of the way to Q (x = 17.5) at t = 0.5 and comes back. It used to be stroked as a
  // line from P to P, which is nothing at all.
  {
    BLPath path;
    path.move_to(10, 10);
    path.cubic_to(20, 10, 20, 10, 10, 10);

    BLBox box = stroked_bounding_box(path);
    EXPECT_LT(bl_abs(box.x0 - 9.5), 1e-6);
    EXPECT_LT(bl_abs(box.x1 - 18.0), 1e-6);
  }

  // A cusp at t = 0.75 where the curve reaches x = 13.375, past its end point at x = 13. It used to be stroked as a
  // line to the end point.
  {
    BLPath path;
    path.move_to(10, 16);
    path.cubic_to(13, 16, 14, 16, 13, 16);

    BLBox box = stroked_bounding_box(path);
    EXPECT_LT(bl_abs(box.x0 - 9.5), 1e-6);
    EXPECT_LT(bl_abs(box.x1 - 13.875), 1e-6);
  }
}

} // {Tests}
} // {bl}

#endif // BL_TEST
