// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_GEOMETRY_INTERSECT_P_H_INCLUDED
#define BLEND2D_GEOMETRY_INTERSECT_P_H_INCLUDED

#include <blend2d/axl/fixedarray_p.h>
#include <blend2d/geometry/bezier_p.h>
#include <blend2d/geometry/commons_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_geometry
//! \{

namespace bl::Geometry {

//! \name Curve Intersection Operations
//!
//! Curve Intersection Primitives
//! -----------------------------
//!
//! These primitives intersect two parametric operands and the PRIMARY output is the parameter pair on the two
//! operands (`t` on operand 0, `t`/`s` on operand 1). A curve-preserving boolean engine splits the operands at
//! those parameters (via `quad_split_between`) to produce exact sub-curves; the intersection point itself is
//! secondary and only used for snapping/reconciliation.
//!
//! \{

//! Epsilon used to classify isolated intersections (endpoint/tangency tests) and to detect degenerate overlaps.
static constexpr double kIntersectEpsilon = 1e-9;

//! How two parametric primitives meet at an isolated intersection.
enum class IntersectionKind : uint32_t {
  kCross    = 0,  //!< Transversal crossing (distinct tangents) — winding flips here.
  kTangent  = 1,  //!< Tangential contact (parallel tangents, even-multiplicity root) — winding does NOT flip.
  kEndpoint = 2   //!< Contact at a curve endpoint (t at 0 or 1 on either operand).
};

//! A single isolated intersection between two parametric primitives.
struct CurveIntersection {
  double t0;              //!< Parameter on operand 0, in [0, 1].
  double t1;              //!< Parameter on operand 1, in [0, 1] (line's `s` for quad/line).
  BLPoint point;          //!< Reconciled contact point — caller stamps this into both split halves.
  IntersectionKind kind;  //!< Contact classification.
  int32_t cross_sign;     //!< sign(cross(tangent0, tangent1)): +1/-1 for kCross, 0 for kTangent.
};

//! A coincident (overlapping) arc shared by both operands. For quad/quad this only happens when both
//! quads are sub-arcs of the SAME underlying parabola; for quad/line when the quad lies on the line.
struct CurveOverlap {
  double t0_min, t0_max;  //!< Shared sub-range on operand 0.
  double t1_min, t1_max;  //!< Shared sub-range on operand 1.
  bool opposite;          //!< Arcs traverse the shared range in opposite directions.
};

//! Aggregate result. Isolated hits and an overlap are mutually exclusive in practice.
struct CurveIntersectionResult {
  FixedArray<CurveIntersection, 4> hits;  //!< <=2 for quad/line, <=4 for quad/quad.
  uint32_t overlap_count;                 //!< 0 or 1.
  CurveOverlap overlap;                   //!< Valid iff overlap_count != 0.

  BL_INLINE void reset() noexcept { hits.clear(); overlap_count = 0; }
};

//! \name Shared Classification
//! \{

//! Returns true if parameter `t` lies on a curve endpoint (within `epsilon`).
static BL_INLINE bool is_endpoint_param(double t, double epsilon) noexcept {
  return (t <= epsilon) | (t >= 1.0 - epsilon);
}

//! Classifies an isolated intersection from the operand tangents at the hit, then appends it to `out.hits`.
//!
//! `tg0` and `tg1` are the (unnormalized) tangents of operand 0 and operand 1 at the intersection. The
//! `endpoint` flag is set by the caller when either parameter lies on a curve endpoint (t==0 or t==1); it
//! wins over the cross/tangent decision but `cross_sign` is still recorded so winding can be resolved.
static BL_INLINE void intersect_classify_and_append(CurveIntersectionResult& out, double t0, double t1,
                                                    const BLPoint& point, const BLPoint& tg0, const BLPoint& tg1,
                                                    bool endpoint, double epsilon) noexcept {
  double cr = cross(tg0, tg1);

  // Tangential when the tangents are (near-)parallel relative to their magnitudes.
  double mag = magnitude_squared(tg0) * magnitude_squared(tg1);
  bool tangent = bl_abs(cr) <= epsilon * Math::sqrt(mag);

  CurveIntersection hit;
  hit.t0 = t0;
  hit.t1 = t1;
  hit.point = point;
  hit.cross_sign = tangent ? int32_t(0) : (cr > 0.0 ? int32_t(1) : int32_t(-1));
  hit.kind = endpoint ? IntersectionKind::kEndpoint
                      : (tangent ? IntersectionKind::kTangent : IntersectionKind::kCross);

  if (out.hits.size() < out.hits.capacity()) {
    out.hits.append(hit);
  }
}

//! \}

//! \name Quad/Line Intersection
//! \{

//! Closed-form quad × line intersection kernel.
//!
//! Returns the number of intersections [0..2]. Writes the quad parameters into `t_out` and the line parameters
//! (the `s` coordinate along `l0 -> l1`, where `s==0` is `l0` and `s==1` is `l1`) into `s_out`.
//!
//! If `segment` is true the results are restricted to `s` in [0, 1] (the line is treated as a finite segment),
//! otherwise the line is treated as infinite. A degenerate zero-length line returns 0.
static BL_INLINE size_t quad_line_intersect_params(QuadRef quad, const BLPoint& l0, const BLPoint& l1,
                                                   bool segment, double t_out[2], double s_out[2]) noexcept {
  BLPoint d = l1 - l0;
  double d_len2 = magnitude_squared(d);

  // Degenerate (zero-length) line cannot define an intersection direction.
  if (d_len2 == 0.0) {
    return 0;
  }

  QuadCoefficients qc = quad_coefficients(quad);
  BLPoint n = normal(d);

  // Signed distance of the quad to the line is `dot(n, B(t) - l0) = 0`, which expands to a quadratic in `t`.
  double a = dot(n, qc.a);
  double b = dot(n, qc.b);
  double c = dot(n, qc.c - l0);

  double roots[2];
  size_t root_count = Math::quad_roots(roots, a, b, c, 0.0, 1.0);

  // `quad_roots` clamps a negative discriminant to zero, which fabricates a spurious double root when the line
  // actually misses the quad. Reject any candidate whose residual `dot(n, B(t) - l0)` isn't (near) zero.
  double residual_tol = kIntersectEpsilon * Math::sqrt(d_len2) *
                        (1.0 + magnitude(qc.a) + magnitude(qc.b) + magnitude(qc.c - l0));

  size_t count = 0;
  for (size_t i = 0; i < root_count; i++) {
    double t = roots[i];
    BLPoint p = quad_evaluate_precise(quad, t);

    if (bl_abs(dot(n, p - l0)) > residual_tol) {
      continue;
    }

    double s = dot(p - l0, d) / d_len2;
    if (segment && (s < -kIntersectEpsilon || s > 1.0 + kIntersectEpsilon)) {
      continue;
    }

    // Snap to the [0, 1] range to remove tiny numerical overshoot at the endpoints.
    t_out[count] = bl_clamp(t, 0.0, 1.0);
    s_out[count] = bl_clamp(s, 0.0, 1.0);
    count++;
  }

  return count;
}

//! Classified quad × line intersection.
//!
//! Resets `out`, then either fills `out.overlap` (when the quad lies on the infinite line) or appends classified
//! isolated hits to `out.hits`.
static BL_INLINE void quad_line_intersect(QuadRef quad, const BLPoint& l0, const BLPoint& l1, bool segment,
                                          CurveIntersectionResult& out, double epsilon = kIntersectEpsilon) noexcept {
  out.reset();

  BLPoint d = l1 - l0;
  double d_len2 = magnitude_squared(d);
  if (d_len2 == 0.0) {
    return;
  }

  QuadCoefficients qc = quad_coefficients(quad);
  BLPoint n = normal(d);

  double a = dot(n, qc.a);
  double b = dot(n, qc.b);
  double c = dot(n, qc.c - l0);

  // Overlap detection: if all three coefficients vanish the quad lies entirely on the infinite line. The
  // threshold is scaled by the line normal's magnitude and a coarse measure of the quad's extent so the
  // test is scale-aware.
  double scale = Math::sqrt(d_len2) * (1.0 + magnitude(qc.a) + magnitude(qc.b) + magnitude(qc.c - l0));
  if (bl_abs(a) <= epsilon * scale && bl_abs(b) <= epsilon * scale && bl_abs(c) <= epsilon * scale) {
    // The mapping `s(t) = dot(B(t) - l0, d) / d_len2` is the projection of the quad onto the line. On a
    // collinear quad it is monotonic, so its endpoints describe the shared span.
    double s0 = dot(quad_evaluate_precise(quad, 0.0) - l0, d) / d_len2;
    double s1 = dot(quad_evaluate_precise(quad, 1.0) - l0, d) / d_len2;

    double t0_min = 0.0;
    double t0_max = 1.0;
    double sa = s0;
    double sb = s1;

    if (segment) {
      // Restrict the quad's parameter range to where its projection falls within the segment [0, 1].
      double lo = bl_min(s0, s1);
      double hi = bl_max(s0, s1);
      double clipped_lo = bl_max(lo, 0.0);
      double clipped_hi = bl_min(hi, 1.0);

      if (clipped_lo > clipped_hi) {
        return;
      }

      // Map the clipped `s` range back to `t` via the (linear over the collinear quad) projection.
      double inv = (s1 != s0) ? 1.0 / (s1 - s0) : 0.0;
      double ta = (clipped_lo - s0) * inv;
      double tb = (clipped_hi - s0) * inv;
      t0_min = bl_clamp(bl_min(ta, tb), 0.0, 1.0);
      t0_max = bl_clamp(bl_max(ta, tb), 0.0, 1.0);
      sa = clipped_lo;
      sb = clipped_hi;
    }

    out.overlap_count = 1;
    out.overlap.t0_min = t0_min;
    out.overlap.t0_max = t0_max;
    out.overlap.t1_min = bl_clamp(bl_min(sa, sb), 0.0, 1.0);
    out.overlap.t1_max = bl_clamp(bl_max(sa, sb), 0.0, 1.0);
    out.overlap.opposite = s1 < s0;
    return;
  }

  double roots[2];
  size_t root_count = Math::quad_roots(roots, a, b, c, 0.0, 1.0);

  QuadDerivativeCoefficients qd = quad_derivative_coefficients(quad);
  for (size_t i = 0; i < root_count; i++) {
    double t = roots[i];
    BLPoint p = quad_evaluate_precise(quad, t);

    // Reject spurious roots fabricated by `quad_roots` clamping a negative discriminant to zero (see the
    // kernel for the rationale); a real root has a (near) zero residual `dot(n, B(t) - l0)`.
    if (bl_abs(dot(n, p - l0)) > epsilon * scale) {
      continue;
    }

    double s = dot(p - l0, d) / d_len2;
    if (segment && (s < -epsilon || s > 1.0 + epsilon)) {
      continue;
    }

    double t_clamped = bl_clamp(t, 0.0, 1.0);
    double s_clamped = bl_clamp(s, 0.0, 1.0);

    BLPoint tg0 = qd.a * t_clamped + qd.b;
    bool endpoint = is_endpoint_param(t_clamped, epsilon) || is_endpoint_param(s_clamped, epsilon);
    intersect_classify_and_append(out, t_clamped, s_clamped, p, tg0, d, endpoint, epsilon);
  }
}

//! \}

//! \name Quad/Quad Intersection
//! \{

namespace Intersect_ {

//! Computes the control-point bounding box of a quad.
static BL_INLINE BLBox quad_control_box(const Quad<BLPoint>& q) noexcept {
  return BLBox(
    bl_min(bl_min(q.vtx[0].x, q.vtx[1].x), q.vtx[2].x),
    bl_min(bl_min(q.vtx[0].y, q.vtx[1].y), q.vtx[2].y),
    bl_max(bl_max(q.vtx[0].x, q.vtx[1].x), q.vtx[2].x),
    bl_max(bl_max(q.vtx[0].y, q.vtx[1].y), q.vtx[2].y));
}

//! Squared length of a box diagonal.
static BL_INLINE double box_diagonal_squared(const BLBox& box) noexcept {
  double dx = box.x1 - box.x0;
  double dy = box.y1 - box.y0;
  return dx * dx + dy * dy;
}

//! Returns true if two boxes overlap when both are inflated by `margin`.
static BL_INLINE bool boxes_overlap(const BLBox& a, const BLBox& b, double margin) noexcept {
  return (a.x1 + margin >= b.x0) & (b.x1 + margin >= a.x0) &
         (a.y1 + margin >= b.y0) & (b.y1 + margin >= a.y0);
}

//! Implicit conic form of a quad: `f(x, y) = k_xx*x^2 + k_xy*x*y + k_yy*y^2 + k_x*x + k_y*y + k_1`, where
//! every point on the quad's underlying parabola satisfies `f == 0`.
struct QuadImplicit {
  double k_xx, k_xy, k_yy, k_x, k_y, k_1;
  double scale;  //!< Rough magnitude used to make the on-curve test scale-aware.

  BL_INLINE double eval(const BLPoint& p) const noexcept {
    return k_xx * p.x * p.x + k_xy * p.x * p.y + k_yy * p.y * p.y + k_x * p.x + k_y * p.y + k_1;
  }
};

//! Builds the implicit conic from a quad's coefficients `B(t) = a*t^2 + b*t + c`.
//!
//! The implicit form is the resultant of `Ax*t^2 + Bx*t + (Cx - x)` and `Ay*t^2 + By*t + (Cy - y)` w.r.t. `t`:
//!
//!   `(Ax*F - Ay*E)^2 - (Ax*By - Ay*Bx) * (Bx*F - By*E) == 0`,  with `E = Cx - x`, `F = Cy - y`.
//!
//! Rewriting `Ax*F - Ay*E = Ay*x - Ax*y + (Ax*Cy - Ay*Cx)` and `Bx*F - By*E = By*x - Bx*y + (Bx*Cy - By*Cx)`
//! and expanding gives the conic coefficients below.
static BL_INLINE QuadImplicit quad_implicit(QuadRef q) noexcept {
  QuadCoefficients qc = quad_coefficients(q);
  double Ax = qc.a.x, Ay = qc.a.y;
  double Bx = qc.b.x, By = qc.b.y;
  double Cx = qc.c.x, Cy = qc.c.y;

  double k = Math::msub(Ax, By, Ay * Bx);  // Ax*By - Ay*Bx

  double u_x = Ay;                          // 'x' coefficient inside the squared linear term.
  double u_y = -Ax;                         // 'y' coefficient.
  double u_1 = Math::msub(Ax, Cy, Ay * Cx); // Ax*Cy - Ay*Cx

  double v_x = By;
  double v_y = -Bx;
  double v_1 = Math::msub(Bx, Cy, By * Cx); // Bx*Cy - By*Cx

  QuadImplicit out;
  out.k_xx = u_x * u_x;
  out.k_xy = 2.0 * u_x * u_y;
  out.k_yy = u_y * u_y;
  out.k_x  = 2.0 * u_x * u_1 - k * v_x;
  out.k_y  = 2.0 * u_y * u_1 - k * v_y;
  out.k_1  = u_1 * u_1 - k * v_1;

  out.scale = 1.0 + bl_abs(out.k_xx) + bl_abs(out.k_xy) + bl_abs(out.k_yy) +
                    bl_abs(out.k_x) + bl_abs(out.k_y) + bl_abs(out.k_1);
  return out;
}

//! Refines an intersection candidate by Newton iteration on `F(t, u) = A(t) - B(u) = 0`.
//!
//! On a true tangency the Jacobian `[A'(t), -B'(u)]` is singular, so a plain Newton step explodes. We keep the
//! lowest-residual iterate seen and, when the Jacobian is near-singular, fall back to a damped gradient step
//! (Gauss-Newton with `J^T`) which still drives the residual down along the well-conditioned direction.
//!
//! `geo_tol` is a geometric (distance) tolerance: the candidate is accepted when the best residual magnitude is
//! within `geo_tol`. `t`/`u` are updated in place to the best iterate.
static BL_INLINE bool quad_quad_newton(QuadRef a, QuadRef b, double& t, double& u, double geo_tol) noexcept {
  QuadDerivativeCoefficients da = quad_derivative_coefficients(a);
  QuadDerivativeCoefficients db = quad_derivative_coefficients(b);

  double best_t = t;
  double best_u = u;
  double best_res = magnitude_squared(quad_evaluate_precise(a, t) - quad_evaluate_precise(b, u));

  double geo_tol_sq = geo_tol * geo_tol;
  double det_eps = bl_max(geo_tol, 1e-30);

  for (uint32_t iter = 0; iter < 24; iter++) {
    BLPoint f = quad_evaluate_precise(a, t) - quad_evaluate_precise(b, u);

    // Jacobian columns: `A'(t)` and `-B'(u)`.
    BLPoint ja = da.a * t + da.b;
    BLPoint jb = db.a * u + db.b;  // B'(u); the Jacobian's second column is `-jb`.

    // J = [ ja.x  -jb.x ]   solve `J * [dt, du]^T = -f`.
    //     [ ja.y  -jb.y ]
    double det = Math::msub(ja.x, -jb.y, ja.y * -jb.x);

    double dt, du;
    if (bl_abs(det) > det_eps) {
      double inv = 1.0 / det;
      dt = (-f.x * -jb.y - (-f.y) * -jb.x) * inv;
      du = (ja.x * -f.y - ja.y * -f.x) * inv;
    }
    else {
      // Near-singular (tangency): damped gradient step `-alpha * J^T * f` along the conditioned direction.
      double g0 = dot(ja, f);      // d(0.5|f|^2)/dt
      double g1 = -dot(jb, f);     // d(0.5|f|^2)/du
      double denom = magnitude_squared(ja) + magnitude_squared(jb);
      double alpha = denom > 1e-30 ? 1.0 / denom : 0.0;
      dt = -alpha * g0;
      du = -alpha * g1;
    }

    t = bl_clamp(t + dt, 0.0, 1.0);
    u = bl_clamp(u + du, 0.0, 1.0);

    double res = magnitude_squared(quad_evaluate_precise(a, t) - quad_evaluate_precise(b, u));
    if (res < best_res) {
      best_res = res;
      best_t = t;
      best_u = u;
    }

    if (res <= geo_tol_sq) {
      break;
    }
  }

  t = best_t;
  u = best_u;
  return best_res <= geo_tol_sq;
}

//! A sub-curve over a parameter sub-range, used by the subdivision work stack.
struct SubQuad {
  Quad<BLPoint> q;  //!< Control points of the sub-curve.
  double t0, t1;    //!< Parameter sub-range on the original curve.
};

} // {Intersect_}

//! Quad × quad intersection kernel using bbox-pruned recursive subdivision followed by Newton refinement.
//!
//! Returns the number of intersection candidates [0..4]. Writes the parameter on `a` into `ta_out` and the
//! parameter on `b` into `tb_out`.
//!
//! When the two quads are co-parabolic (sub-arcs of the same underlying parabola), this returns 0 and sets
//! `*overlap_out` (when non-null) to true — the classified wrapper handles the overlap. Otherwise `*overlap_out`
//! is set to false.
static BL_INLINE size_t quad_quad_intersect_params(QuadRef a, QuadRef b, double ta_out[4], double tb_out[4],
                                                   double epsilon,
                                                   bool& overlap_out) noexcept {
  using namespace Intersect_;

  overlap_out = false;

  // Co-parabolic overlap guard: if three points sampled ON curve `b` lie on `a`'s implicit parabola, both quads
  // are sub-arcs of the same parabola. Subdividing such a pair never separates the bounding boxes, so we must
  // detect it up-front to avoid pathological subdivision. NOTE: a quad's middle control point is OFF the curve,
  // so we test evaluated curve points `B(0)`, `B(0.5)`, `B(1)` — not the raw control net.
  QuadImplicit impl = quad_implicit(a);
  {
    double f0 = impl.eval(quad_evaluate_precise(b, 0.0));
    double f1 = impl.eval(quad_evaluate_precise(b, 0.5));
    double f2 = impl.eval(quad_evaluate_precise(b, 1.0));
    double tol = epsilon * impl.scale;

    if (bl_abs(f0) <= tol && bl_abs(f1) <= tol && bl_abs(f2) <= tol) {
      overlap_out = true;
      return 0;
    }
  }

  // Geometric convergence tolerance derived from the operands' extents.
  BLBox box_a0 = quad_control_box(Quad<BLPoint>(a.vtx[0], a.vtx[1], a.vtx[2]));
  BLBox box_b0 = quad_control_box(Quad<BLPoint>(b.vtx[0], b.vtx[1], b.vtx[2]));
  double extent = Math::sqrt(bl_max(box_diagonal_squared(box_a0), box_diagonal_squared(box_b0)));
  double converge_tol = bl_max(extent * 1e-7, 1e-12);
  double converge_tol_sq = converge_tol * converge_tol;
  double overlap_margin = converge_tol;

  // Converged candidates are grouped into geometric clusters on the fly. Each cluster accumulates the running
  // sum of its members' parameters so its centroid (the seed for Newton) is computed over ALL members — this
  // is what makes a tangency (which yields a large, symmetric spread of overlapping sub-boxes) refine to its
  // true contact instead of an arbitrary cluster edge. Clusters are keyed by the geometric point of `a` at the
  // candidate's `ta`, since distinct intersections are well separated geometrically.
  struct Cluster {
    double ta_sum, tb_sum;  //!< Running sums of member parameters.
    BLPoint point;          //!< Representative point (first member) used for cluster matching.
    uint32_t n;             //!< Member count.
  };

  constexpr size_t kMaxClusters = 8;
  Cluster clusters[kMaxClusters];
  size_t cluster_count = 0;

  double cluster_radius = bl_max(extent * 1e-3, 1e-7);
  double cluster_radius_sq = cluster_radius * cluster_radius;

  // Explicit work stack for subdivision.
  constexpr size_t kStackCapacity = 200;
  SubQuad stack_a[kStackCapacity];
  SubQuad stack_b[kStackCapacity];

  stack_a[0].q.assign_curve(a.vtx[0], a.vtx[1], a.vtx[2]);
  stack_a[0].t0 = 0.0;
  stack_a[0].t1 = 1.0;
  stack_b[0].q.assign_curve(b.vtx[0], b.vtx[1], b.vtx[2]);
  stack_b[0].t0 = 0.0;
  stack_b[0].t1 = 1.0;
  size_t sp = 1;

  uint32_t processed = 0;
  constexpr uint32_t kMaxProcessed = 200000;

  while (sp != 0 && processed < kMaxProcessed) {
    processed++;
    sp--;
    SubQuad sa = stack_a[sp];
    SubQuad sb = stack_b[sp];

    BLBox bba = quad_control_box(sa.q);
    BLBox bbb = quad_control_box(sb.q);

    if (!boxes_overlap(bba, bbb, overlap_margin)) {
      continue;
    }

    double diag_a_sq = box_diagonal_squared(bba);
    double diag_b_sq = box_diagonal_squared(bbb);

    // Converged, or out of stack space: record a candidate at the sub-curves' parameter midpoints, merging it
    // into a geometric cluster.
    if ((diag_a_sq <= converge_tol_sq && diag_b_sq <= converge_tol_sq) || sp + 2 > kStackCapacity) {
      double ta_mid = (sa.t0 + sa.t1) * 0.5;
      double tb_mid = (sb.t0 + sb.t1) * 0.5;
      BLPoint pt = quad_evaluate_precise(a, ta_mid);

      bool merged = false;
      for (size_t k = 0; k < cluster_count; k++) {
        if (magnitude_squared(pt - clusters[k].point) <= cluster_radius_sq) {
          clusters[k].ta_sum += ta_mid;
          clusters[k].tb_sum += tb_mid;
          clusters[k].n++;
          merged = true;
          break;
        }
      }

      if (!merged && cluster_count < kMaxClusters) {
        clusters[cluster_count].ta_sum = ta_mid;
        clusters[cluster_count].tb_sum = tb_mid;
        clusters[cluster_count].point = pt;
        clusters[cluster_count].n = 1;
        cluster_count++;
      }
      continue;
    }

    // Split the curve with the larger bounding box at its mid-parameter, keeping the parameter sub-ranges.
    if (diag_a_sq >= diag_b_sq) {
      double tm = (sa.t0 + sa.t1) * 0.5;
      Quad<BLPoint> left, right;
      quad_split(quad_ref(sa.q), quad_out(left), quad_out(right));

      stack_a[sp].q = left;  stack_a[sp].t0 = sa.t0; stack_a[sp].t1 = tm;
      stack_b[sp] = sb;
      sp++;
      stack_a[sp].q = right; stack_a[sp].t0 = tm; stack_a[sp].t1 = sa.t1;
      stack_b[sp] = sb;
      sp++;
    }
    else {
      double tm = (sb.t0 + sb.t1) * 0.5;
      Quad<BLPoint> left, right;
      quad_split(quad_ref(sb.q), quad_out(left), quad_out(right));

      stack_a[sp] = sa;
      stack_b[sp].q = left;  stack_b[sp].t0 = sb.t0; stack_b[sp].t1 = tm;
      sp++;
      stack_a[sp] = sa;
      stack_b[sp].q = right; stack_b[sp].t0 = tm; stack_b[sp].t1 = sb.t1;
      sp++;
    }
  }

  // Refine each cluster centroid with Newton iteration. The centroid of a (symmetric) tangency spread sits on
  // the true contact, so refinement converges even though the per-iterate Jacobian is singular there. A final
  // geometric dedup guards against two clusters collapsing onto the same refined root.
  double refine_tol = bl_max(extent * 1e-9, 1e-12);
  double merge_tol = bl_max(extent * 1e-5, 1e-9);
  double merge_tol_sq = merge_tol * merge_tol;

  size_t count = 0;
  for (size_t i = 0; i < cluster_count; i++) {
    double ta = clusters[i].ta_sum / double(clusters[i].n);
    double tb = clusters[i].tb_sum / double(clusters[i].n);

    quad_quad_newton(a, b, ta, tb, refine_tol);

    // Accept only if the refined points actually coincide (rejects spurious clusters where the curves merely
    // came close but never met).
    BLPoint pa = quad_evaluate_precise(a, ta);
    BLPoint pb = quad_evaluate_precise(b, tb);
    if (magnitude_squared(pa - pb) > merge_tol_sq) {
      continue;
    }

    bool merged = false;
    for (size_t j = 0; j < count; j++) {
      BLPoint pj = quad_evaluate_precise(a, ta_out[j]);
      if (magnitude_squared(pa - pj) <= merge_tol_sq) {
        merged = true;
        break;
      }
    }

    if (!merged && count < 4) {
      ta_out[count] = ta;
      tb_out[count] = tb;
      count++;
    }
  }

  return count;
}

//! Classified quad × quad intersection.
//!
//! Resets `out`, then either fills `out.overlap` (co-parabolic overlap) or appends classified isolated hits.
static BL_INLINE void quad_quad_intersect(QuadRef a, QuadRef b, CurveIntersectionResult& out,
                                          double epsilon = kIntersectEpsilon) noexcept {
  out.reset();

  double ta[4];
  double tb[4];
  bool overlap = false;
  size_t count = quad_quad_intersect_params(a, b, ta, tb, epsilon, overlap);

  if (overlap) {
    // Co-parabolic overlap: estimate the shared sub-range by projecting b's endpoints onto a (closest-point
    // via Newton) and reporting the spanned a-range; the b-range is its full [0, 1]. This is a best-effort
    // fill — overlaps are a rare degenerate case in a quadratic boolean engine.
    QuadDerivativeCoefficients da = quad_derivative_coefficients(a);

    // Parameter on `a` whose point is closest to `p` (Newton on `d/dt |A(t) - p|^2 = 0`, seeded by sampling).
    auto param_on_a = [&](const BLPoint& p) noexcept -> double {
      double best = 0.0;
      double best_d = magnitude_squared(quad_evaluate_precise(a, 0.0) - p);
      for (uint32_t k = 1; k <= 8; k++) {
        double tt = double(k) / 8.0;
        double d = magnitude_squared(quad_evaluate_precise(a, tt) - p);
        if (d < best_d) { best_d = d; best = tt; }
      }

      double t = best;
      for (uint32_t iter = 0; iter < 16; iter++) {
        BLPoint diff = quad_evaluate_precise(a, t) - p;
        BLPoint deriv = da.a * t + da.b;
        double g = dot(diff, deriv);
        double gp = magnitude_squared(deriv) + dot(diff, da.a);
        if (bl_abs(gp) <= 1e-30) break;
        double step = g / gp;
        t = bl_clamp(t - step, 0.0, 1.0);
        if (bl_abs(step) <= 1e-12) break;
      }
      return t;
    };

    double ta0 = param_on_a(b.vtx[0]);
    double ta1 = param_on_a(b.vtx[2]);

    out.overlap_count = 1;
    out.overlap.t0_min = bl_clamp(bl_min(ta0, ta1), 0.0, 1.0);
    out.overlap.t0_max = bl_clamp(bl_max(ta0, ta1), 0.0, 1.0);
    out.overlap.t1_min = 0.0;
    out.overlap.t1_max = 1.0;
    out.overlap.opposite = ta0 > ta1;
    return;
  }

  QuadDerivativeCoefficients da = quad_derivative_coefficients(a);
  QuadDerivativeCoefficients db = quad_derivative_coefficients(b);

  for (size_t i = 0; i < count; i++) {
    BLPoint pa = quad_evaluate_precise(a, ta[i]);
    BLPoint pb = quad_evaluate_precise(b, tb[i]);
    BLPoint point = lerp(pa, pb, 0.5);

    BLPoint tg0 = da.a * ta[i] + da.b;
    BLPoint tg1 = db.a * tb[i] + db.b;

    bool endpoint = is_endpoint_param(ta[i], epsilon) || is_endpoint_param(tb[i], epsilon);
    intersect_classify_and_append(out, ta[i], tb[i], point, tg0, tg1, endpoint, epsilon);
  }
}

//! \}

//! \}

} // {bl::Geometry}

//! \}
//! \endcond

#endif // BLEND2D_GEOMETRY_INTERSECT_P_H_INCLUDED
