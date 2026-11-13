// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/axl/mathconst_p.h>
#include <blend2d/core/context.h>
#include <blend2d/core/path_p.h>
#include <blend2d/geometry/bezier_p.h>
#include <blend2d/geometry/stroke_p.h>

// Geometry - Stroke - Tests
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

TEST_CASE(geometry_stroke_cubic_degenerate_quad) {
  // Issue #269: the first approximating quadratic reverses along an almost straight line. The generated
  // quadratic must receive the same degeneracy handling as a quadratic path command, before offsetting.
  const BLPoint points[] = {BLPoint(0, 0), BLPoint(4, 4), BLPoint(7, 3), BLPoint(5, 4), BLPoint(7, 2)};
  const BLPoint translations[] = {BLPoint(0, 0), BLPoint(0.125, -0.25), BLPoint(10000, -10000)};

  for (uint32_t mode = 0; mode < 3; mode++) {
    double sx = mode == 2 ? 2.0 : 1191.0 / 595.32;
    double sy = mode == 2 ? 2.0 : 1684.0 / 841.92;

    for (const BLPoint& translation : translations) {
      BLPoint p[5];
      for (size_t i = 0; i < BL_ARRAY_SIZE(points); i++) {
        p[i].reset((0.095993 * points[i].x + 254.7695923 + 56.9459229) * sx + translation.x,
                  1684.0 - (-0.0959473 * points[i].y + 485.9789734) * sy + translation.y);
      }

      for (uint32_t with_line = 0; with_line < 2; with_line++) {
        BLPath path;
        EXPECT_OK(path.move_to(p[with_line ? 0 : 1]));
        if (with_line) {
          EXPECT_OK(path.line_to(p[1]));
        }
        EXPECT_OK(path.cubic_to(p[2], p[3], p[4]));

        BLStrokeOptions options;
        EXPECT_OK(options.set_width(1.0));
        if (mode != 1) {
          EXPECT_OK(options.set_caps(BL_STROKE_CAP_ROUND));
          EXPECT_OK(options.set_join(BL_STROKE_JOIN_ROUND));
        }

        BLPath outline;
        EXPECT_OK(outline.add_stroked_path(path, options, bl_default_approximation_options));
        EXPECT_GT(outline.vertex_count(), 0u);
        EXPECT_LE(outline.vertex_count(), 100u);
        for (size_t i = 0; i < outline.vertex_count(); i++) {
          const BLPoint& v = outline.vertex_data()[i];
          EXPECT_TRUE(Math::is_finite(v.x) && Math::is_finite(v.y));
          EXPECT_LE(bl_abs(v.x - p[1].x), 4.0);
          EXPECT_LE(bl_abs(v.y - p[1].y), 4.0);
        }

        // Also exercise the original rendering entry point without a translation.
        if (translation == BLPoint(0, 0) && with_line) {
          BLImage image(1191, 1684, BL_FORMAT_PRGB32);
          BLContext context(image);
          EXPECT_OK(context.set_stroke_options(options));
          EXPECT_OK(context.stroke_path(path, BLRgba32(0xFF000000u)));
          EXPECT_OK(context.end());
        }
      }
    }
  }
}

TEST_CASE(geometry_stroke_generated_quad_cusp) {
  // At this scale the cubic is approximated by these two quadratics. Compare with explicit quadratic
  // commands to verify the round cusp and outgoing tangent, as well as successful termination.
  BLPath cubic;
  EXPECT_OK(cubic.move_to(0.4, 0.4));
  EXPECT_OK(cubic.cubic_to(0.7, 0.3, 0.5, 0.4, 0.7, 0.2));

  BLPath quads;
  EXPECT_OK(quads.move_to(0.4, 0.4));
  EXPECT_OK(quads.quad_to(0.625, 0.325, 0.5875, 0.3375));
  EXPECT_OK(quads.quad_to(0.55, 0.35, 0.7, 0.2));

  BLStrokeOptions options;
  EXPECT_OK(options.set_width(1.0));
  BLPath cubic_outline;
  BLPath quad_outline;
  EXPECT_OK(cubic_outline.add_stroked_path(cubic, options, bl_default_approximation_options));
  EXPECT_OK(quad_outline.add_stroked_path(quads, options, bl_default_approximation_options));
  EXPECT_EQ(cubic_outline.vertex_count(), quad_outline.vertex_count());
  // Rounding may select either side as the outer side of the 180-degree cusp. Compare the resulting
  // coverage instead of requiring the two outlines to emit that semicircle in the same vertex order.
  BLImage images[2];
  const BLPath* outlines[] = {&cubic_outline, &quad_outline};
  for (size_t i = 0; i < 2; i++) {
    EXPECT_OK(images[i].create(64, 64, BL_FORMAT_PRGB32));
    BLContext context(images[i]);
    EXPECT_OK(context.clear_all());
    EXPECT_OK(context.scale(32.0));
    EXPECT_OK(context.translate(0.5, 0.5));
    EXPECT_OK(context.fill_path(*outlines[i], BLRgba32(0xFFFFFFFFu)));
    EXPECT_OK(context.end());
  }
  EXPECT_EQ(images[0], images[1]);
}

TEST_CASE(geometry_stroke_quad_tiny_cusp) {
  // An unrepresentable or negligible first cusp side must not discard the far endpoint.
  const double controls[] = {0.0, 1e-200, 1e-12};
  BLStrokeOptions options;
  EXPECT_OK(options.set_width(2.0));
  BLPath line;
  EXPECT_OK(line.move_to(0.0, 0.0));
  EXPECT_OK(line.line_to(-100.0, 0.0));
  BLPath expected;
  EXPECT_OK(expected.add_stroked_path(line, options, bl_default_approximation_options));

  for (double control : controls) {
    BLPath quad;
    EXPECT_OK(quad.move_to(0.0, 0.0));
    EXPECT_OK(quad.quad_to(control, 0.0, -100.0, 0.0));
    BLPath outline;
    EXPECT_OK(outline.add_stroked_path(quad, options, bl_default_approximation_options));
    EXPECT_EQ(outline, expected);
  }
}

TEST_CASE(geometry_stroke_quad_offset_progress) {
  // At a larger translation the nearly collinear generated quadratic can pass the degeneracy threshold,
  // yet a later angle split cannot advance its coordinates. Terminate instead of repeating that split.
  BLPath path;
  EXPECT_OK(path.move_to(1000000000.04, 1000000.04));
  EXPECT_OK(path.cubic_to(1000000000.07, 1000000.03,
                         1000000000.05, 1000000.04,
                         1000000000.07, 1000000.02));
  BLStrokeOptions options;
  EXPECT_OK(options.set_width(0.1f));
  EXPECT_OK(options.set_caps(BL_STROKE_CAP_ROUND));
  EXPECT_OK(options.set_join(BL_STROKE_JOIN_ROUND));
  BLPath outline;
  BLResult result = outline.add_stroked_path(path, options, bl_default_approximation_options);
  // Different floating-point implementations may make progress and finish this curve successfully.
  EXPECT_TRUE(result == BL_SUCCESS || result == BL_ERROR_INVALID_GEOMETRY);
  EXPECT_LE(outline.vertex_count(), 100u);
  for (size_t i = 0; i < outline.vertex_count(); i++) {
    const BLPoint& p = outline.vertex_data()[i];
    EXPECT_TRUE(Math::is_finite(p.x) && Math::is_finite(p.y));
  }
}

} // {bl::Geometry::Tests}

#endif // BL_TEST
