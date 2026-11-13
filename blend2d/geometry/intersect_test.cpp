// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/geometry/intersect_p.h>

// Geometry - Intersect - Tests
// ============================

namespace bl::Geometry::Tests {

// Oracle helper: flattens both quads into `N` segments and brute-forces all segment/segment intersections,
// clustering hits within `cluster_tol` into reference points. Returns the number of reference points.
static size_t intersect_oracle(bl::Geometry::QuadRef a, bl::Geometry::QuadRef b, BLPoint out[16],
                               double cluster_tol = 1e-6) noexcept {
  constexpr uint32_t kN = 512;
  size_t out_count = 0;

  BLPoint a_prev = quad_evaluate_precise(a, 0.0);
  for (uint32_t i = 1; i <= kN; i++) {
    BLPoint a_cur = quad_evaluate_precise(a, double(i) / double(kN));
    BLPoint ad = a_cur - a_prev;

    BLPoint b_prev = quad_evaluate_precise(b, 0.0);
    for (uint32_t j = 1; j <= kN; j++) {
      BLPoint b_cur = quad_evaluate_precise(b, double(j) / double(kN));
      BLPoint bd = b_cur - b_prev;

      // Solve a_prev + t*ad == b_prev + u*bd for t, u in [0, 1].
      double denom = cross(ad, bd);
      if (bl_abs(denom) > 1e-18) {
        BLPoint rhs = b_prev - a_prev;
        double t = cross(rhs, bd) / denom;
        double u = cross(rhs, ad) / denom;

        if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0) {
          BLPoint hit = a_prev + ad * t;

          bool merged = false;
          for (size_t k = 0; k < out_count; k++) {
            if (magnitude_squared(hit - out[k]) <= cluster_tol * cluster_tol) {
              merged = true;
              break;
            }
          }

          if (!merged && out_count < 16) {
            out[out_count++] = hit;
          }
        }
      }

      b_prev = b_cur;
    }

    a_prev = a_cur;
  }

  return out_count;
}

// Quad/Line - Two Crossings
// -------------------------
//
// A symmetric upward parabola crossed by the horizontal line `y = 0.5`. By symmetry the crossings are at the
// roots of the quad's Y-coordinate equation, which for these control points are known closed-form values.
TEST_CASE(geometry_intersect_quad_line_two_crossings) {
  // Quad: p0=(0,0), p1=(1,2), p2=(2,0). Y(t) = 4t(1 - t), which equals 0.5 at 8t^2 - 8t + 1 = 0,
  // i.e. t = (2 ± sqrt(2)) / 4 ~= {0.1464, 0.8536}.
  BLPoint pts[3] = { BLPoint(0.0, 0.0), BLPoint(1.0, 2.0), BLPoint(2.0, 0.0) };

  CurveIntersectionResult r;
  quad_line_intersect(quad_ref(pts), BLPoint(-1.0, 0.5), BLPoint(3.0, 0.5), false, r);

  EXPECT_EQ(r.overlap_count, 0u);
  EXPECT_EQ(r.hits.size(), size_t(2));

  double t_lo = (2.0 - Math::sqrt(2.0)) / 4.0;
  double t_hi = (2.0 + Math::sqrt(2.0)) / 4.0;

  // Hits are sorted ascending in `t` (roots come from quad_roots sorted).
  EXPECT_LE(bl_abs(r.hits[0].t0 - t_lo), 1e-9);
  EXPECT_LE(bl_abs(r.hits[1].t0 - t_hi), 1e-9);

  // Both hits lie on the line `y = 0.5`.
  EXPECT_LE(bl_abs(r.hits[0].point.y - 0.5), 1e-9);
  EXPECT_LE(bl_abs(r.hits[1].point.y - 0.5), 1e-9);

  EXPECT_EQ(r.hits[0].kind, IntersectionKind::kCross);
  EXPECT_EQ(r.hits[1].kind, IntersectionKind::kCross);
}

// Quad/Line - Apex Tangent
// ------------------------
//
// The line `y = 1` is tangent to the apex of the parabola whose maximum is exactly `y = 1` at `t = 0.5`.
TEST_CASE(geometry_intersect_quad_line_apex_tangent) {
  // Quad with apex at (1, 1): p0=(0,0), p1=(1,2), p2=(2,0) -> Y(0.5) = 1, Y'(0.5) = 0.
  BLPoint pts[3] = { BLPoint(0.0, 0.0), BLPoint(1.0, 2.0), BLPoint(2.0, 0.0) };
  QuadRef quad = quad_ref(pts);

  CurveIntersectionResult r;
  quad_line_intersect(quad, BLPoint(-1.0, 1.0), BLPoint(3.0, 1.0), false, r);

  // The double root collapses to a single hit at the apex.
  EXPECT_EQ(r.hits.size(), size_t(1));
  EXPECT_LE(bl_abs(r.hits[0].t0 - 0.5), 1e-7);
  EXPECT_LE(bl_abs(r.hits[0].point.x - 1.0), 1e-7);
  EXPECT_LE(bl_abs(r.hits[0].point.y - 1.0), 1e-9);
  EXPECT_EQ(r.hits[0].kind, IntersectionKind::kTangent);
  EXPECT_EQ(r.hits[0].cross_sign, 0);
}

// Quad/Line - No Intersection
// ---------------------------
TEST_CASE(geometry_intersect_quad_line_none) {
  BLPoint pts[3] = { BLPoint(0.0, 0.0), BLPoint(1.0, 2.0), BLPoint(2.0, 0.0) };
  QuadRef quad = quad_ref(pts);

  // Line well above the parabola's bounding box.
  CurveIntersectionResult r;
  quad_line_intersect(quad, BLPoint(-1.0, 5.0), BLPoint(3.0, 5.0), false, r);

  EXPECT_EQ(r.overlap_count, 0u);
  EXPECT_EQ(r.hits.size(), size_t(0));
}

// Quad/Line - Segment vs Infinite
// -------------------------------
//
// The infinite line crosses the quad at two points, but a short segment only covers one of them. With
// `segment=false` both are reported; with `segment=true` only the one whose `s` falls within [0, 1].
TEST_CASE(geometry_intersect_quad_line_segment_vs_infinite) {
  BLPoint pts[3] = { BLPoint(0.0, 0.0), BLPoint(1.0, 2.0), BLPoint(2.0, 0.0) };
  QuadRef quad = quad_ref(pts);

  // The infinite line `y = 0.5` crosses the quad at x = 1 - sqrt(2)/2 ~= 0.2929 (left) and
  // x = 1 + sqrt(2)/2 ~= 1.7071 (right). The SEGMENT from (0, 0.5) to (1, 0.5) has `s = p.x`, so only the
  // left crossing (s ~= 0.2929 in [0, 1]) survives `segment=true`; the right (s ~= 1.7071) is excluded.
  BLPoint l0(0.0, 0.5);
  BLPoint l1(1.0, 0.5);

  CurveIntersectionResult r_inf;
  quad_line_intersect(quad, l0, l1, false, r_inf);
  EXPECT_EQ(r_inf.hits.size(), size_t(2));

  CurveIntersectionResult r_seg;
  quad_line_intersect(quad, l0, l1, true, r_seg);
  EXPECT_EQ(r_seg.hits.size(), size_t(1));

  // The single segment hit must be the left crossing (x ~= 0.2929).
  EXPECT_LE(bl_abs(r_seg.hits[0].point.x - (1.0 - Math::sqrt(2.0) / 2.0)), 1e-9);
}

// Quad/Line - Endpoint Contact
// ----------------------------
//
// A line passing exactly through the quad's start point classifies as kEndpoint.
TEST_CASE(geometry_intersect_quad_line_endpoint) {
  BLPoint pts[3] = { BLPoint(0.0, 0.0), BLPoint(1.0, 2.0), BLPoint(2.0, 0.0) };
  QuadRef quad = quad_ref(pts);

  // Vertical line x = 0 touches the quad only at its start point (0, 0) -> t == 0.
  CurveIntersectionResult r;
  quad_line_intersect(quad, BLPoint(0.0, -1.0), BLPoint(0.0, 1.0), false, r);

  EXPECT_EQ(r.hits.size(), size_t(1));
  EXPECT_LE(bl_abs(r.hits[0].t0 - 0.0), 1e-9);
  EXPECT_LE(bl_abs(r.hits[0].point.x - 0.0), 1e-9);
  EXPECT_LE(bl_abs(r.hits[0].point.y - 0.0), 1e-9);
  EXPECT_EQ(r.hits[0].kind, IntersectionKind::kEndpoint);
}

// Quad/Line - Closed-Form Exactness
// ---------------------------------
//
// Pick a target point P on the quad at a known `t*`, build a line through P with a generic direction, and
// assert one returned `t` matches `t*` to closed-form precision.
TEST_CASE(geometry_intersect_quad_line_exactness) {
  BLPoint pts[3] = { BLPoint(-3.0, 1.0), BLPoint(2.0, 7.0), BLPoint(5.0, -2.0) };
  QuadRef quad = quad_ref(pts);

  double t_star = 0.37;
  BLPoint p = quad_evaluate_precise(quad, t_star);

  // Line through P built from a single slanted direction so P is exactly collinear with l0 and l1.
  BLPoint dir(1.0, 0.4);
  BLPoint l0 = p - dir * 2.0;
  BLPoint l1 = p + dir * 3.0;

  CurveIntersectionResult r;
  quad_line_intersect(quad, l0, l1, false, r);

  EXPECT_GE(r.hits.size(), size_t(1));

  // At least one returned t equals t_star.
  bool found = false;
  for (size_t i = 0; i < r.hits.size(); i++) {
    if (bl_abs(r.hits[i].t0 - t_star) <= 1e-9) {
      found = true;
      EXPECT_LE(bl_abs(r.hits[i].point.x - p.x), 1e-9);
      EXPECT_LE(bl_abs(r.hits[i].point.y - p.y), 1e-9);
    }
  }
  EXPECT_TRUE(found);
}

// Quad/Line - Collinear Overlap
// -----------------------------
//
// A degenerate quad whose control points are collinear with the line produces an overlap, not isolated hits.
TEST_CASE(geometry_intersect_quad_line_overlap) {
  // Collinear control points on the x-axis.
  BLPoint pts[3] = { BLPoint(0.0, 0.0), BLPoint(1.0, 0.0), BLPoint(2.0, 0.0) };
  QuadRef quad = quad_ref(pts);

  CurveIntersectionResult r;
  quad_line_intersect(quad, BLPoint(-1.0, 0.0), BLPoint(3.0, 0.0), false, r);

  EXPECT_EQ(r.overlap_count, 1u);
  EXPECT_EQ(r.hits.size(), size_t(0));
  EXPECT_LE(bl_abs(r.overlap.t0_min - 0.0), 1e-9);
  EXPECT_LE(bl_abs(r.overlap.t0_max - 1.0), 1e-9);
  EXPECT_FALSE(r.overlap.opposite);
}

// Quad/Quad - Two Crossings
// -------------------------
//
// Two parabolas opening in opposite directions that cross at two points.
TEST_CASE(geometry_intersect_quad_quad_two_crossings) {
  // a: opens downward, apex at (1, 2).
  BLPoint a_pts[3] = { BLPoint(0.0, 0.0), BLPoint(1.0, 4.0), BLPoint(2.0, 0.0) };
  // b: opens upward, apex at (1, -1) shifted up; crosses `a` on both flanks.
  BLPoint b_pts[3] = { BLPoint(0.0, 2.0), BLPoint(1.0, -2.0), BLPoint(2.0, 2.0) };

  QuadRef a = quad_ref(a_pts);
  QuadRef b = quad_ref(b_pts);

  CurveIntersectionResult r;
  quad_quad_intersect(a, b, r);

  EXPECT_EQ(r.overlap_count, 0u);
  EXPECT_EQ(r.hits.size(), size_t(2));

  for (size_t i = 0; i < r.hits.size(); i++) {
    BLPoint pa = quad_evaluate_precise(a, r.hits[i].t0);
    BLPoint pb = quad_evaluate_precise(b, r.hits[i].t1);
    EXPECT_LE(magnitude(pa - pb), 1e-7);
  }
}

// Quad/Quad - Tangent Contact
// ---------------------------
//
// Two parabolas that just touch at a single point with parallel tangents.
TEST_CASE(geometry_intersect_quad_quad_tangent) {
  // a: apex up at (1, 1). Y_a(t) = 4t(1 - t) -> max 1 at t = 0.5, point (1, 1), tangent horizontal.
  BLPoint a_pts[3] = { BLPoint(0.0, 0.0), BLPoint(1.0, 2.0), BLPoint(2.0, 0.0) };
  // b: apex down at (1, 1) from above. Y_b(t) = 2 - 4t(1 - t) -> min 1 at t = 0.5, point (1, 1), horizontal.
  BLPoint b_pts[3] = { BLPoint(0.0, 2.0), BLPoint(1.0, 0.0), BLPoint(2.0, 2.0) };

  QuadRef a = quad_ref(a_pts);
  QuadRef b = quad_ref(b_pts);

  CurveIntersectionResult r;
  quad_quad_intersect(a, b, r);

  EXPECT_EQ(r.overlap_count, 0u);
  EXPECT_EQ(r.hits.size(), size_t(1));
  EXPECT_LE(bl_abs(r.hits[0].point.x - 1.0), 1e-5);
  EXPECT_LE(bl_abs(r.hits[0].point.y - 1.0), 1e-5);
  EXPECT_EQ(r.hits[0].kind, IntersectionKind::kTangent);
  EXPECT_EQ(r.hits[0].cross_sign, 0);
}

// Quad/Quad - Disjoint
// --------------------
TEST_CASE(geometry_intersect_quad_quad_disjoint) {
  BLPoint a_pts[3] = { BLPoint(0.0, 0.0), BLPoint(1.0, 2.0), BLPoint(2.0, 0.0) };
  BLPoint b_pts[3] = { BLPoint(0.0, 10.0), BLPoint(1.0, 12.0), BLPoint(2.0, 10.0) };

  QuadRef a = quad_ref(a_pts);
  QuadRef b = quad_ref(b_pts);

  CurveIntersectionResult r;
  quad_quad_intersect(a, b, r);

  EXPECT_EQ(r.overlap_count, 0u);
  EXPECT_EQ(r.hits.size(), size_t(0));
}

// Quad/Quad - Co-Parabolic Overlap
// --------------------------------
//
// Two sub-arcs of the SAME parabola (carved out with quad_split_between) overlap on a shared parameter span.
// The intersector must report an overlap, not isolated hits, and the shared `t0` range must match the overlap
// of the two arcs' master ranges.
TEST_CASE(geometry_intersect_quad_quad_overlap) {
  BLPoint full[3] = { BLPoint(0.0, 0.0), BLPoint(2.0, 4.0), BLPoint(4.0, 0.0) };
  QuadRef fr = quad_ref(full);

  // Arc `a` spans master [0.1, 0.7]; arc `b` spans master [0.4, 0.95]. Shared master range is [0.4, 0.7].
  BLPoint a_pts[3];
  BLPoint b_pts[3];
  quad_split_between(fr, quad_out(a_pts), 0.1, 0.7);
  quad_split_between(fr, quad_out(b_pts), 0.4, 0.95);

  QuadRef a = quad_ref(a_pts);
  QuadRef b = quad_ref(b_pts);

  CurveIntersectionResult r;
  quad_quad_intersect(a, b, r);

  EXPECT_EQ(r.overlap_count, 1u);
  EXPECT_EQ(r.hits.size(), size_t(0));

  // master 0.4 -> a-local (0.4 - 0.1) / 0.6 = 0.5; master 0.7 -> 1.0.
  EXPECT_LE(bl_abs(r.overlap.t0_min - 0.5), 1e-6);
  EXPECT_LE(bl_abs(r.overlap.t0_max - 1.0), 1e-6);
  EXPECT_FALSE(r.overlap.opposite);
}

// Quad/Quad - Oracle Cross-Check
// ------------------------------
//
// For several fixed (deterministic) quad pairs, assert that the subdivision intersector returns the same
// number of points as a brute-force flattening oracle and that every returned point matches an oracle point.
TEST_CASE(geometry_intersect_quad_quad_oracle) {
  struct Case {
    BLPoint a[3];
    BLPoint b[3];
  };

  static const Case cases[] = {
    // Two opposing parabolas crossing at two points.
    {
      { BLPoint(0.0, 0.0), BLPoint(2.0, 5.0), BLPoint(4.0, 0.0) },
      { BLPoint(0.0, 3.0), BLPoint(2.0, -2.0), BLPoint(4.0, 3.0) }
    },
    // Slanted quads that cross twice.
    {
      { BLPoint(-2.0, -1.0), BLPoint(0.0, 4.0), BLPoint(3.0, -1.0) },
      { BLPoint(-2.0, 2.0), BLPoint(1.0, -3.0), BLPoint(3.0, 2.0) }
    },
    // A small arc crossing a wide arc once.
    {
      { BLPoint(0.0, 0.0), BLPoint(3.0, 6.0), BLPoint(6.0, 0.0) },
      { BLPoint(2.0, -1.0), BLPoint(3.0, 3.0), BLPoint(4.0, -1.0) }
    },
    // Two arcs crossing at two points, rotated layout.
    {
      { BLPoint(1.0, -2.0), BLPoint(5.0, 1.0), BLPoint(1.0, 4.0) },
      { BLPoint(4.0, -2.0), BLPoint(0.0, 1.0), BLPoint(4.0, 4.0) }
    }
  };

  for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
    QuadRef a = quad_ref(cases[c].a);
    QuadRef b = quad_ref(cases[c].b);

    BLPoint oracle[16];
    size_t oracle_count = intersect_oracle(a, b, oracle);

    CurveIntersectionResult r;
    quad_quad_intersect(a, b, r);

    EXPECT_EQ(r.hits.size(), oracle_count)
      .message("case %zu: intersector found %zu hits, oracle found %zu", c, r.hits.size(), oracle_count);

    // Every returned point must be close to an oracle point.
    for (size_t i = 0; i < r.hits.size(); i++) {
      double best = 1e9;
      for (size_t k = 0; k < oracle_count; k++) {
        best = bl_min(best, magnitude(r.hits[i].point - oracle[k]));
      }
      EXPECT_LE(best, 1e-5)
        .message("case %zu: hit %zu at (%g, %g) has no matching oracle point (dist=%g)",
                 c, i, r.hits[i].point.x, r.hits[i].point.y, best);
    }

    // And the param pairs must agree geometrically.
    for (size_t i = 0; i < r.hits.size(); i++) {
      BLPoint pa = quad_evaluate_precise(a, r.hits[i].t0);
      BLPoint pb = quad_evaluate_precise(b, r.hits[i].t1);
      EXPECT_LE(magnitude(pa - pb), 1e-7);
    }
  }
}

} // {bl::Geometry::Tests}

#endif // BL_TEST
