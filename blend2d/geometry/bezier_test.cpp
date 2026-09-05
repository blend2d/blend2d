// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api-build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/geometry/bezier_p.h>

// bl::BitArray - Tests
// ====================

namespace bl::Tests {

struct CountingQuadSink {
  mutable size_t count;

  BL_INLINE BLResult operator()(BLPoint[3]) const noexcept {
    count++;
    return BL_SUCCESS;
  }
};

UNIT(geometry_bezier, BL_TEST_GROUP_GEOMETRY_UTILITIES) {
  {
    constexpr double w = 0.5;
    BLPoint conic[4] {
      BLPoint(0.0, 0.0),
      BLPoint(1.0, 3.0),
      BLPoint(w, Math::nan<double>()),
      BLPoint(2.0, 0.0)
    };

    BLPoint spline[30];
    BLPoint* end = Geometry::split_conic_to_spline<Geometry::QuadSplitOptions::kExtremaY>(conic, spline);

    EXPECT_EQ(size_t(end - spline), size_t(12));
    EXPECT_EQ(spline[0], BLPoint(0.0, 0.0));
    EXPECT_EQ(spline[1].x, 1.0);
    EXPECT_EQ(spline[6], spline[4]);
    EXPECT_EQ(spline[7], spline[5]);
    EXPECT_EQ(spline[10], BLPoint(2.0, 0.0));
    EXPECT_EQ(spline[11].x, 1.0);
    EXPECT_GT(spline[4].y / spline[5].x, 0.0);
  }

  {
    BLPoint cubic[4] {
      BLPoint(0.0, 0.0),
      BLPoint(0.0, 100.0),
      BLPoint(100.0, 100.0),
      BLPoint(100.0, 0.0)
    };

    CountingQuadSink sink0 { 0 };
    EXPECT_SUCCESS(Geometry::approximate_cubic_with_quads(Geometry::cubic_ref(cubic), 1e-12, 0, sink0));
    EXPECT_EQ(sink0.count, size_t(2));

    CountingQuadSink sink1 { 0 };
    EXPECT_SUCCESS(Geometry::approximate_cubic_with_quads(Geometry::cubic_ref(cubic), 1e-12, 1, sink1));
    EXPECT_EQ(sink1.count, size_t(4));
  }
}

} // {bl::Tests}

#endif // BL_TEST
