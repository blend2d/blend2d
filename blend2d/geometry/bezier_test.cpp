// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/geometry/bezier_p.h>

#include <initializer_list>

// Geometry - Bezier - Tests
// =========================

namespace bl::Geometry::Tests {

TEST_CASE(geometry_quad_parameter_at_angle) {
  // For B'(t) = (1, +/-4*t), tan(angle(B'(0), B'(t))) = 4*t.
  for (double sign : {-1.0, 1.0}) {
    Quad curve(0.0, 0.0, 0.5, 0.0, 1.0, sign * 2.0);
    for (double m : {0.05, 0.2, 0.75, 2.0})
      EXPECT_LE(bl_abs(quad_parameter_at_angle(curve, m) - m * 0.25), 1e-14);
  }

  static const BLPoint curves[][3] = {
    { BLPoint(0.0, 0.0), BLPoint(50.0, 100.0), BLPoint(100.0, 0.0) },
    { BLPoint(0.0, 0.0), BLPoint(1000.0, 1000.0), BLPoint(1000.0, 0.0) },
    { BLPoint(0.0, 0.0), BLPoint(0.0, 1000.0), BLPoint(1.0, 0.0) },
    { BLPoint(3.0, 7.0), BLPoint(-40.0, 60.0), BLPoint(12.0, 19.0) },
    { BLPoint(0.0, 0.0), BLPoint(100.0, 1.0), BLPoint(200.0, 0.0) }
  };

  for (const auto& points : curves) {
    for (double m : {0.05, 0.2, 0.75, 2.0}) {
      Quad curve(points);
      size_t count = 0;
      for (;;) {
        double t = quad_parameter_at_angle(curve, m);
        EXPECT_GT(t, 0.0);
        EXPECT_LE(t, 1.0);

        // Check the actual tangent angle independently of the parameter-step formula.
        QuadDerivativeCoefficients dc = quad_derivative_coefficients(curve);
        for (uint32_t i = 1; i <= 16; i++) {
          BLPoint tangent = dc.b + dc.a * (t * double(i) / 16.0);
          double angle = bl_abs(Math::atan2(cross(dc.b, tangent), dot(dc.b, tangent)));
          EXPECT_LE(angle, Math::atan(m) + 1e-12);
          if (i == 16 && t < 1.0)
            EXPECT_LE(bl_abs(angle - Math::atan(m)), 1e-12);
        }

        EXPECT_LT(++count, 128u);
        if (t == 1.0 || count >= 128)
          break;
        quad_split_after(curve, curve, t);
      }
    }
  }

  // A straight curve can reverse at a zero derivative, so it must be split at that cusp.
  Quad reversal(0.0, 0.0, 100.0, 0.0, -100.0, 0.0);
  EXPECT_LE(bl_abs(quad_parameter_at_angle(reversal, 0.2) - 1.0 / 3.0), 1e-14);

  // A zero endpoint derivative has the direction of the other edge and requires no split.
  for (const auto& points : {
      Quad(0.0, 0.0, 0.0, 0.0, 100.0, 0.0),
      Quad(0.0, 0.0, 100.0, 0.0, 100.0, 0.0),
      Quad(3.0, 7.0, 3.0, 7.0, 3.0, 7.0)})
    EXPECT_EQ(quad_parameter_at_angle(points, 0.2), 1.0);
}

TEST_CASE(geometry_quad_parameter_at_length) {
  // Arc-length quadrature and inversion operate on curves whose tangent rotation is already small.
  static const BLPoint curves[][3] = {
    { BLPoint(0.0, 0.0), BLPoint(100.0, 5.0), BLPoint(200.0, 0.0) },
    { BLPoint(0.0, 0.0), BLPoint(1.0, 0.01), BLPoint(1000.0, 0.0) },
    { BLPoint(0.0, 0.0), BLPoint(0.0, 1.0), BLPoint(0.01, 1000.0) },
    { BLPoint(3.0, 7.0), BLPoint(40.0, 8.0), BLPoint(120.0, 9.0) }
  };

  for (const auto& points : curves) {
    QuadRef curve = quad_ref(points);
    double total = quad_length(curve);
    double previous_t = 0.0;

    EXPECT_EQ(quad_parameter_at_length(curve, -1.0), 0.0);
    EXPECT_EQ(quad_parameter_at_length(curve, 0.0), 0.0);
    EXPECT_EQ(quad_parameter_at_length(curve, total), 1.0);
    EXPECT_EQ(quad_parameter_at_length(curve, total * 2.0), 1.0);

    for (uint32_t i = 1; i < 20; i++) {
      double target = total * double(i) / 20.0;
      double t = quad_parameter_at_length(curve, target);
      EXPECT_GT(t, previous_t);
      EXPECT_LT(t, 1.0);

      Quad prefix;
      quad_split_before(curve, prefix, t);
      EXPECT_LE(bl_abs(quad_length(prefix) - target), total * 1e-10);
      previous_t = t;
    }
  }

  // A small dash must be refined relative to its own length, rather than the whole curve's length.
  for (size_t i = 0; i < 3; i++) {
    QuadRef curve = quad_ref(curves[i]);
    for (double fraction : {1e-6, 1e-9, 1e-12}) {
      double target = quad_length(curve) * fraction;
      double t = quad_parameter_at_length(curve, target);
      EXPECT_GT(t, 0.0);
      Quad prefix;
      quad_split_before(curve, prefix, t);
      EXPECT_LE(bl_abs(quad_length(prefix) - target), target * 1e-10);
    }
  }

  // Coincident start/control points give B(t) = (100*t^2, 0), so the inverse is known exactly.
  Quad curve(0.0, 0.0, 0.0, 0.0, 100.0, 0.0);
  for (uint32_t i = 1; i < 10; i++) {
    double fraction = double(i) / 10.0;
    EXPECT_LE(bl_abs(quad_parameter_at_length(curve, 100.0 * fraction) - Math::sqrt(fraction)), 1e-10);
  }
}

TEST_CASE(geometry_quad_length_degenerate) {
  Quad zero(3.0, 7.0, 3.0, 7.0, 3.0, 7.0);
  EXPECT_EQ(quad_length(zero), 0.0);
  EXPECT_EQ(quad_parameter_at_length(zero, 0.0), 0.0);
  EXPECT_EQ(quad_parameter_at_length(zero, 1.0), 1.0);

  // Split reversals at the zero derivative before measuring length or inverting it.
  Quad reversal(0.0, 0.0, 100.0, 0.0, 0.0, 0.0);
  Quad before, after;
  quad_split(reversal, before, after, quad_parameter_at_angle(reversal, 0.2));
  EXPECT_LE(bl_abs(quad_length(before) + quad_length(after) - 100.0), 1e-12);
  EXPECT_LE(bl_abs(quad_parameter_at_length(before, 25.0) - (1.0 - Math::sqrt(0.5))), 1e-10);
  EXPECT_LE(bl_abs(quad_parameter_at_length(after, 25.0) - Math::sqrt(0.5)), 1e-10);

  // An asymmetric reversal reaches x = 100/3 at t = 1/3, then travels another 400/3 units back.
  Quad asymmetric(0.0, 0.0, 100.0, 0.0, -100.0, 0.0);
  quad_split(asymmetric, before, after, quad_parameter_at_angle(asymmetric, 0.2));
  EXPECT_LE(bl_abs(quad_length(before) + quad_length(after) - 500.0 / 3.0), 1e-12);
}

// Splitting a conic at `t = 0.5` replaces its weight `w` with `sqrt((1 + w) / 2)`, which is always
// strictly closer to `1` than `w`. Repeated halving therefore drives a conic of any positive weight
// towards a parabola (`w == 1`), regardless of whether it started as a hyperbola (`w > 1`) or an
// ellipse (`w < 1`). This test verifies that convergence as well as the geometric invariants of the split.
TEST_CASE(geometry_conic_split) {
  // Starting weights spanning the hyperbola (`w > 1`) and ellipse (`w < 1`) regimes. `0.70710...` is the
  // weight of a 90-degree circular arc (`cos(45 deg)`).
  static const double initial_weights[] = {
    1000.0, 100.0, 8.0, 2.0, 0.7071067811865476, 0.1, 0.001
  };

  for (double w0 : initial_weights) {
    BLPoint pts[3] = { BLPoint(0.0, 0.0), BLPoint(1.0, 2.0), BLPoint(3.0, 0.0) };
    double w = w0;
    double prev_dist = bl_abs(w - 1.0);

    // Keep the left half after each split - the weight must converge monotonically towards 1. The loop
    // stops once the fixed point `w == 1` is reached exactly (`prev_dist == 0`), as further splits are
    // no-ops and the distance can no longer strictly decrease.
    for (uint32_t i = 0; i < 64 && prev_dist > 0.0; i++) {
      BLPoint a_pts[3];
      BLPoint b_pts[3];
      double aw;
      double bw;

      conic_split(conic_ref(pts, w), conic_out(a_pts, aw), conic_out(b_pts, bw));

      // Both halves share the standard-form weight and the split point; outer endpoints are preserved.
      EXPECT_EQ(aw, bw);
      EXPECT_EQ(a_pts[0], pts[0]);
      EXPECT_EQ(b_pts[2], pts[2]);
      EXPECT_EQ(a_pts[2], b_pts[0]);

      double expected = Math::sqrt((1.0 + w) * 0.5);
      EXPECT_LE(bl_abs(aw - expected), 1e-12);

      // The new weight is strictly closer to 1 than the previous one.
      double dist = bl_abs(aw - 1.0);
      EXPECT_LT(dist, prev_dist);
      prev_dist = dist;

      pts[0] = a_pts[0];
      pts[1] = a_pts[1];
      pts[2] = a_pts[2];
      w = aw;
    }

    // After enough splits the conic is numerically indistinguishable from a parabola.
    EXPECT_LE(bl_abs(w - 1.0), 1e-9)
      .message("Conic weight failed to converge to 1 from w0=%g (got w=%.17g)", w0, w);
  }
}

// Squared distance of point `p` from the line segment `[a, b]` (clamped to the segment endpoints).
static double conic_test_dist_sq_point_seg(BLPoint p, BLPoint a, BLPoint b) noexcept {
  BLPoint ab = b - a;
  double len_sq = magnitude_squared(ab);
  double t = len_sq > 0.0 ? bl_clamp(dot(p - a, ab) / len_sq, 0.0, 1.0) : 0.0;
  return magnitude_squared(p - (a + ab * t));
}

// `approximate_conic_with_quads` splits a conic and drops each sufficiently-flat sub-conic's weight, using its
// control polygon directly as a quad. This verifies the emitted spline is continuous, reproduces the conic's
// endpoints exactly, collapses a `w == 1` conic (already a quad) to a single quad, and stays within tolerance
// of the true conic across the ellipse/parabola/hyperbola weight regimes.
TEST_CASE(geometry_approximate_conic_with_quads) {
  // `0.70710...` is the weight of a 90-degree circular arc (`cos(45 deg)`).
  static const double weights[] = { 0.25, 0.5, 0.7071067811865476, 1.0, 2.0, 5.0 };

  BLPoint p0(0.0, 0.0);
  BLPoint cp(50.0, 80.0);
  BLPoint p2(100.0, 0.0);

  double simplify_tolerance = 0.05;

  for (double w : weights) {
    BLPoint conic[3] = { p0, cp, p2 };

    // Collect the emitted quad spline.
    BLPoint quads[256][3];
    size_t quad_count = 0;

    BLResult result = approximate_conic_with_quads(conic_ref(conic, w), simplify_tolerance,
      [&](QuadRef q) noexcept -> BLResult {
        EXPECT_LT(quad_count, 256u);
        quads[quad_count][0] = q[0];
        quads[quad_count][1] = q[1];
        quads[quad_count][2] = q[2];
        quad_count++;
        return BL_SUCCESS;
      });

    EXPECT_EQ(result, BL_SUCCESS);
    EXPECT_GT(quad_count, 0u);

    // Endpoints are reproduced exactly and the spline is continuous (each quad starts where the last ended).
    EXPECT_EQ(quads[0][0], p0);
    EXPECT_EQ(quads[quad_count - 1u][2], p2);
    for (size_t i = 1; i < quad_count; i++) {
      EXPECT_EQ(quads[i][0], quads[i - 1u][2]);
    }

    // A conic with `w == 1` is already a quad, so it is emitted verbatim as a single quad.
    if (w == 1.0) {
      EXPECT_EQ(quad_count, 1u);
      EXPECT_EQ(quads[0][1], cp);
    }

    // Sample the conic and measure the max distance to the quad spline (each quad flattened to a polyline).
    double max_dev = 0.0;
    constexpr int kConicSamples = 512;
    constexpr int kQuadSegments = 64;

    for (int s = 0; s <= kConicSamples; s++) {
      double t = double(s) / double(kConicSamples);
      BLPoint pt = eval_conic_precise(conic, w, BLPoint(t, t));

      double min_dist_sq = 1e308;
      for (size_t i = 0; i < quad_count; i++) {
        BLPoint prev = quads[i][0];
        for (int qs = 1; qs <= kQuadSegments; qs++) {
          BLPoint curr = quad_evaluate_precise(quad_ref(quads[i]), double(qs) / double(kQuadSegments));
          min_dist_sq = bl_min(min_dist_sq, conic_test_dist_sq_point_seg(pt, prev, curr));
          prev = curr;
        }
      }
      max_dev = bl_max(max_dev, Math::sqrt(min_dist_sq));
    }

    // The parameter-step estimator is a first-order heuristic (the same one used by stroking), so the actual
    // deviation can slightly exceed the nominal tolerance - allow a small margin. The point is that the spline
    // tracks the conic tightly (a fraction of a unit on a ~100-unit arc) and scales with `simplify_tolerance`.
    TEST_LOG("w=%g -> %zu quads, max deviation = %g", w, quad_count, max_dev);
    EXPECT_LE(max_dev, simplify_tolerance * 2.0)
      .message("Conic->quad approximation exceeded tolerance for w=%g (max_dev=%g)", w, max_dev);
  }
}

} // {bl::Geometry::Tests}

#endif // BL_TEST
