// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_GEOMETRY_BEZIER_P_H_INCLUDED
#define BLEND2D_GEOMETRY_BEZIER_P_H_INCLUDED

#include <blend2d/axl/fixedarray_p.h>
#include <blend2d/axl/span_p.h>
#include <blend2d/geometry/commons_p.h>
#include <blend2d/geometry/tolerance_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_geometry
//! \{

namespace bl::Geometry {

//! \name Quadratic Bezier Curve Operations
//!
//! Quadratic Bezier Curve Formulas
//! -------------------------------
//!
//! Quad Coefficients:
//!
//! ```
//! A =    p0 + 2*p1 + p2
//! B = -2*p0 + 2*p1
//! C =    p0
//! ```
//!
//! Quad Evaluation at `t`:
//!
//! ```
//! V = A*t^2 + B*t + C => t(A*t + B) + C => FMA(FMA(A, t, B), t, C)
//! ```
//!
//! \{

template<typename T = BLPoint>
struct Quad {
  //! Number of vertices including both start point, control point(s), and end point.
  static constexpr inline uint32_t kVertexCount = 3u;

  //! Vertex type.
  using Vertex = T;

  //! Curve data as array (not pointing to vertices elsewhere).
  //!
  //! \note Not initialized by default to make it possible to use Quad in temporary arrays.
  Vertex vtx[kVertexCount];

  BL_INLINE_NODEBUG Quad() noexcept = default;

  template<typename U>
  BL_INLINE_NODEBUG Quad(const Quad<U>& other) noexcept
    : vtx{other.vtx[0], other.vtx[1], other.vtx[2]} {}

  BL_INLINE_NODEBUG explicit Quad(const Vertex* arr) noexcept
    : vtx{arr[0], arr[1], arr[2]} {}

  BL_INLINE_NODEBUG Quad(const Vertex& p0, const Vertex& p1, const Vertex& p2) noexcept
    : vtx{p0, p1, p2} {}

  BL_INLINE_NODEBUG Quad(double x0, double y0, double x1, double y1, double x2, double y2) noexcept
    : vtx{BLPoint(x0, y0), BLPoint(x1, y1), BLPoint(x2, y2)} {}

  template<typename U>
  BL_INLINE_NODEBUG Quad& operator=(const Quad<U>& other) noexcept {
    assign_curve(other);
    return *this;
  }

  BL_INLINE_NODEBUG Vertex& operator[](size_t i) noexcept {
    BL_ASSERT(i < kVertexCount);
    return vtx[i];
  }

  BL_INLINE_NODEBUG const Vertex& operator[](size_t i) const noexcept {
    BL_ASSERT(i < kVertexCount);
    return vtx[i];
  }

  BL_INLINE_NODEBUG void assign_vertex(size_t i, Vertex value) noexcept {
    BL_ASSERT(i < kVertexCount);
    vtx[i] = value;
  }

  template<typename U>
  BL_INLINE_NODEBUG void assign_curve(const Quad<U>& other) noexcept {
    vtx[0] = other.vtx[0];
    vtx[1] = other.vtx[1];
    vtx[2] = other.vtx[2];
  }

  BL_INLINE_NODEBUG void assign_curve(Vertex p0, Vertex p1, Vertex p2) noexcept {
    vtx[0] = p0;
    vtx[1] = p1;
    vtx[2] = p2;
  }
};

template<typename T>
struct Quad<T*> {
  //! Number of vertices including both start point, control point(s), and end point.
  static constexpr inline uint32_t kVertexCount = 3u;

  //! Vertex type.
  using Vertex = T;

  //! Pointer to data (note it's always zero initialized to make sure it's used safely).
  Vertex* vtx {};

  BL_INLINE_NODEBUG Quad() noexcept = default;

  BL_INLINE_NODEBUG explicit Quad(Vertex* storage) noexcept
    : vtx(storage) {}

  // Not explicit - designed to be created implicitly for in/out arguments.
  template<typename U>
  BL_INLINE_NODEBUG Quad(Quad<U>& other) noexcept
    : vtx(other.vtx) {}

  // Not explicit - designed to be created implicitly for in/out arguments.
  template<typename U>
  BL_INLINE_NODEBUG Quad(const Quad<U>& other) noexcept
    : vtx(other.vtx) {}

  BL_INLINE_NODEBUG Vertex& operator[](size_t i) noexcept {
    BL_ASSERT(i < kVertexCount);
    return vtx[i];
  }

  BL_INLINE_NODEBUG const Vertex& operator[](size_t i) const noexcept {
    BL_ASSERT(i < kVertexCount);
    return vtx[i];
  }

  BL_INLINE_NODEBUG void assign_storage(Vertex* vtx_ptr) noexcept { vtx = vtx_ptr; }

  BL_INLINE_NODEBUG void assign_vertex(size_t i, Vertex value) noexcept {
    BL_ASSERT(i < kVertexCount);
    vtx[i] = value;
  }

  template<typename U>
  BL_INLINE_NODEBUG void assign_curve(const Quad<U>& other) noexcept {
    vtx[0] = other.vtx[0];
    vtx[1] = other.vtx[1];
    vtx[2] = other.vtx[2];
  }

  BL_INLINE_NODEBUG void assign_curve(Vertex p0, Vertex p1, Vertex p2) noexcept {
    vtx[0] = p0;
    vtx[1] = p1;
    vtx[2] = p2;
  }
};

using QuadRef = Quad<const BLPoint*>;
using QuadRefMut = Quad<BLPoint*>;

BL_INLINE_NODEBUG QuadRef quad_ref(const BLPoint* vtx_ptr) noexcept { return QuadRef(vtx_ptr); }
BL_INLINE_NODEBUG QuadRef quad_ref(QuadRef other) noexcept { return QuadRef(other.vtx); }
BL_INLINE_NODEBUG QuadRef quad_ref(QuadRefMut other) noexcept { return QuadRef(other.vtx); }
BL_INLINE_NODEBUG QuadRef quad_ref(const Quad<BLPoint>& other) noexcept { return QuadRef(other.vtx); }

BL_INLINE_NODEBUG QuadRefMut quad_out(BLPoint* vtx_ptr) noexcept { return QuadRefMut(vtx_ptr); }
BL_INLINE_NODEBUG QuadRefMut quad_out(Quad<BLPoint*> other) noexcept { return QuadRefMut(other.vtx); }
BL_INLINE_NODEBUG QuadRefMut quad_out(Quad<BLPoint>& other) noexcept { return QuadRefMut(other.vtx); }

//! Coefficients of a quadratic curve that can be used to evaluate quad curve at `t`.
struct QuadCoefficients {
  BLPoint a, b, c;
};

//! Derivative coefficients of a quadratic curve.
struct QuadDerivativeCoefficients {
  BLPoint a, b;
};

//! Static options that can be used to split a quad curve.
enum class QuadSplitOptions : uint32_t {
  kNone = 0x0,
  //! Split at X extrema.
  kExtremaX = 0x1,
  //! Split at Y extrema.
  kExtremaY = 0x2,

  //! Split at X and Y extrema - combines `kExtremaX` and `kExtremaY`.
  kExtremaXY = kExtremaX | kExtremaY
};
BL_DEFINE_ENUM_FLAGS(QuadSplitOptions)

static BL_INLINE QuadCoefficients quad_coefficients(QuadRef q) noexcept {
  BLPoint v1 = q[1] - q[0];
  BLPoint v2 = q[2] - q[1];
  return {v2 - v1, v1 + v1, q[0]};
};

static BL_INLINE QuadDerivativeCoefficients quad_derivative_coefficients(QuadRef q) noexcept {
  BLPoint v1 = q[1] - q[0];
  BLPoint v2 = q[2] - q[1];

  BLPoint two(2.0, 2.0);
  BLPoint v1_2x = v1 * two;
  return {Math::msub(two, v2, v1_2x), v1_2x};
}

template<typename T>
static BL_INLINE bool is_quad_degenerate(QuadRef q) noexcept {
  BLPoint v1 = q[1] - q[0];
  BLPoint v2 = q[2] - q[1];

  // Check if angle is too sharp.
  return dot(v1, v2) < Math::kCosAcute * Math::sqrt(magnitude_squared(v1) * magnitude_squared(v2));
}

static BL_INLINE BLPoint quad_evaluate(const QuadCoefficients& coef, const BLPoint& t) noexcept {
  return Math::madd(Math::madd(coef.a, t, coef.b), t, coef.c);
}

static BL_INLINE BLPoint quad_evaluate(const QuadCoefficients& coef, double t) noexcept {
  return quad_evaluate(coef, BLPoint(t, t));
}

static BL_INLINE BLPoint quad_evaluate(QuadRef curve, double t) noexcept {
  return quad_evaluate(quad_coefficients(curve), t);
}

static BL_INLINE BLPoint quad_evaluate(QuadRef curve, const BLPoint& t) noexcept {
  return quad_evaluate(quad_coefficients(curve), t);
}

static BL_INLINE BLPoint quad_evaluate_precise(QuadRef curve, double t) noexcept {
  return lerp(lerp(curve[0], curve[1], t), lerp(curve[1], curve[2], t), t);
}

static BL_INLINE BLPoint quad_evaluate_precise(QuadRef curve, const BLPoint& t) noexcept {
  return lerp(lerp(curve[0], curve[1], t), lerp(curve[1], curve[2], t), t);
}

static BL_INLINE BLPoint quad_extrema_point(QuadRef curve) noexcept {
  BLPoint v0 = curve[0] - curve[1];
  BLPoint t = bl_clamp(v0 / (v0 - curve[1] + curve[2]), 0.0, 1.0);
  return quad_evaluate_precise(curve, t);
}

//! Returns the first split parameter limiting tangent rotation to `atan(m)`, where `m > 0`.
//! Returns 1 if the whole curve fits. A collinear reversal is split at its stationary point.
static BL_INLINE double quad_parameter_at_angle(QuadRef curve, double m) noexcept {
  BLPoint v0 = curve[1] - curve[0];
  BLPoint v1 = curve[2] - curve[1];
  double v0_dot_v1 = dot(v0, v1);
  double v0_cross_v1 = bl_abs(cross(v0, v1));

  // The derivative traces the segment between 2*v0 and 2*v1, so its direction turns monotonically.
  // This cone test also covers constant tangents and a zero tangent at either endpoint.
  if (v0_dot_v1 >= 0.0 && v0_cross_v1 <= m * v0_dot_v1) {
    return 1.0;
  }

  // For B'(t) = a*t + b, tan(M) = t*|a x b| / (|b|^2 + t*(a . b)). Solving for t gives
  // m*|b|^2 / (|a x b| - m*(a . b)). Substitute a = 2*(v1-v0), b = 2*v0 and cancel 4.
  double v0_sq = magnitude_squared(v0);
  return bl_min(1.0, m * v0_sq / (v0_cross_v1 + m * (v0_sq - v0_dot_v1)));
}

static BL_INLINE double quad_curvature_metric(QuadRef curve) noexcept {
  return cross(curve[2] - curve[1], curve[1] - curve[0]);
}

static BL_INLINE size_t quad_offset_cusp_ts(QuadRef curve, double d, double t_out[2], double t_min, double t_max) {
  QuadDerivativeCoefficients dc = quad_derivative_coefficients(curve);

  double bxa = cross(dc.b, dc.a);
  double boa = dot(dc.b, dc.a);

  if (bxa == 0.0) {
    return 0;
  }

  double alen2 = magnitude_squared(dc.a);
  double blen2 = magnitude_squared(dc.b);

  double fac = -1.0 / alen2;
  double sqrt_ = Math::sqrt(boa * boa - alen2 * (blen2 - Math::cbrt((d * d) * (bxa * bxa))));

  double t0 = fac * (boa + sqrt_);
  double t1 = fac * (boa - sqrt_);

  // We are only interested in (t_min, t_max) range.
  t0 = bl_max(t0, t_min);

  size_t n = size_t(t0 >= t_min && t0 <= t_max);
  t_out[0] = t0;
  t_out[n] = t1;
  return n + size_t(t1 > t0 && t1 <= t_max);
}

static BL_INLINE BLPoint quad_start_tangent(QuadRef curve) noexcept {
  BLPoint out = curve.vtx[1] - curve.vtx[0];
  BLPoint t20 = curve.vtx[2] - curve.vtx[0];

  if (is_zero(out)) out = t20;
  return out;
}

static BL_INLINE BLPoint quad_end_tangent(QuadRef curve) noexcept {
  BLPoint out = curve.vtx[2] - curve.vtx[1];
  BLPoint t20 = curve.vtx[2] - curve.vtx[0];

  if (is_zero(out)) out = t20;
  return out;
}

static BL_INLINE void quad_split(QuadRef curve, QuadRefMut a_out, QuadRefMut b_out) noexcept {
  BLPoint cp0(curve[0]);
  BLPoint cp1(curve[1]);
  BLPoint cp2(curve[2]);

  BLPoint p01(lerp(cp0, cp1));
  BLPoint p12(lerp(cp1, cp2));
  BLPoint p01_p12(lerp(p01, p12));

  a_out.assign_curve(cp0, p01, p01_p12);
  b_out.assign_curve(p01_p12, p12, cp2);
}

static BL_INLINE void quad_split(QuadRef curve, QuadRefMut a_out, QuadRefMut b_out, double t) noexcept {
  BLPoint cp0(curve[0]);
  BLPoint cp1(curve[1]);
  BLPoint cp2(curve[2]);

  BLPoint p01(lerp(cp0, cp1, t));
  BLPoint p12(lerp(cp1, cp2, t));
  BLPoint p01_p12(lerp(p01, p12, t));

  a_out.assign_curve(cp0, p01, p01_p12);
  b_out.assign_curve(p01_p12, p12, cp2);
}

static BL_INLINE void quad_split_before(QuadRef curve, QuadRefMut out, double t) noexcept {
  BLPoint p01(lerp(curve[0], curve[1], t));
  BLPoint p12(lerp(curve[1], curve[2], t));

  out.assign_curve(curve[0], p01, lerp(p01, p12, t));
}

static BL_INLINE void quad_split_after(QuadRef curve, QuadRefMut out, double t) noexcept {
  BLPoint p01(lerp(curve[0], curve[1], t));
  BLPoint p12(lerp(curve[1], curve[2], t));

  out.assign_curve(lerp(p01, p12, t), p12, curve[2]);
}

static BL_INLINE void quad_split_between(QuadRef curve, QuadRefMut out, double t0, double t1) noexcept {
  BLPoint t0p01 = lerp(curve[0], curve[1], t0);
  BLPoint t0p12 = lerp(curve[1], curve[2], t0);
  BLPoint t1p01 = lerp(curve[0], curve[1], t1);
  BLPoint t1p12 = lerp(curve[1], curve[2], t1);

  out.assign_curve(lerp(t0p01, t0p12, t0), lerp(t0p01, t0p12, t1), lerp(t1p01, t1p12, t1));
}

static BL_INLINE double gauss_legendre_quad_speed(double wz, double xz, BLPoint dca, BLPoint dcb) noexcept {
  BLPoint xzv(xz, xz);
  return magnitude(Math::madd(dca, xzv, dcb) * wz);
}

//! Approximates the arc length of an angle-bounded quadratic using a 4-node Gauss-Legendre quadrature.
//! Split sharp curves and stationary reversals before calling this function.
static BL_INLINE double quad_length(QuadRef curve) noexcept {
  // https://pomax.github.io/bezierinfo/legendre-gauss.html
  // Let `wz = (z / 2) * w` and `xz = (z / 2) * x + (z / 2)` with `z = 1`,
  // so that `sum += wz * (B + xz * A).length` is the arc length.
  QuadDerivativeCoefficients dc = quad_derivative_coefficients(curve);

  // Weights and abscissae for `n = 4`.
  double sum1 = gauss_legendre_quad_speed(0.1739274225687269, 0.06943184420297371, dc.a, dc.b) +
                gauss_legendre_quad_speed(0.3260725774312731, 0.33000947820757190, dc.a, dc.b);
  double sum2 = gauss_legendre_quad_speed(0.3260725774312731, 0.66999052179242810, dc.a, dc.b) +
                gauss_legendre_quad_speed(0.1739274225687269, 0.93056815579702630, dc.a, dc.b);
  return sum1 + sum2;
}

static BL_INLINE double quad_interpolate_at_length(QuadRef curve, double d) noexcept {
  double d1 = length(curve[1], curve[0]);
  double d2 = length(curve[2], curve[1]);
  double total = d1 + d2;

  // Arc length is monotonic in `t`, so a target `d` outside `[0, total]` has no interior solution - clamp
  // it to the matching endpoint. This also covers the degenerate zero-length control polygon (`total == 0`)
  // and the near-zero `d` produced by an arc-length refinement residual, both of which would otherwise blow
  // the result up via a division by ~0. `!(d > 0)` additionally rejects a NaN target.
  if (!(d > 0.0))
    return 0.0;
  if (d >= total)
    return 1.0;

  // Solve `(d2 - d1) * t^2 + 2 * d1 * t = d` for `t` (`t = 1` -> `d1 + d2 = d`). A root in `[0, 1]` exists
  // because `0 < d < total`; the chord fraction `d / total` is a safe in-range fallback for rounding edges.
  double roots[2];
  return Math::quad_roots(roots, d2 - d1, d1 * 2.0, -d, 0.0, 1.0) ? roots[0] : d / total;
}

//! Returns the parameter `t` at which the quadratic `curve` reaches arc length `length` from its start.
//! The curve must satisfy the same angle bound as required by `quad_length()`.
static BL_INLINE double quad_parameter_at_length(QuadRef curve, double length) noexcept {
  if (!(length > 0.0))
    return 0.0;

  double total_length = quad_length(curve);
  if (length >= total_length)
    return 1.0;

  double t = quad_interpolate_at_length(curve, length);
  if (!(t > 0.0 && t < 1.0))
    t = length / total_length;
  double t_min = 0.0;
  double t_max = 1.0;
  QuadDerivativeCoefficients dc = quad_derivative_coefficients(curve);

  // Refine the control-polygon estimate in either direction. Keep a bracket so a zero or very small
  // derivative (at a cusp, for example) cannot take a Newton step outside the curve.
  for (uint32_t i = 0; i < 32; i++) {
    Quad prefix;
    quad_split_before(curve, prefix, t);

    double error = quad_length(prefix) - length;
    if (bl_abs(error) <= length * 1e-12)
      break;

    if (error < 0.0)
      t_min = t;
    else
      t_max = t;

    double speed = magnitude(Math::madd(dc.a, BLPoint(t, t), dc.b));
    double next_t = t - error / speed;
    if (!(next_t > t_min && next_t < t_max))
      next_t = (t_min + t_max) * 0.5;

    if (next_t == t)
      break;
    t = next_t;
  }

  return t;
}

static BL_INLINE BLPoint* quad_split_with_ts(QuadRef curve, BLPoint* out, Span<const double> ts) noexcept {
  size_t i = 0;
  double t_cut = 0.0;

  out[0] = curve[0];
  BLPoint last = curve[2];
  QuadCoefficients qc = quad_coefficients(curve);

  do {
    double t_val = ts[i];
    BL_ASSERT(t_val >  0.0);
    BL_ASSERT(t_val <= 1.0);

    double dt = (t_val - t_cut) * 0.5;

    // Derivative: 2a*t + b.
    BLPoint tv = BLPoint(t_val, t_val);
    BLPoint cp = Math::madd(qc.a, tv * 2.0, qc.b) * dt;
    BLPoint tp = Math::madd(Math::madd(qc.a, tv, qc.b), tv, qc.c);

    // The last point must be exact.
    if (++i == ts.size()) {
      tp = last;
    }

    out[1].reset(tp - cp);
    out[2].reset(tp);
    out += 2;

    t_cut = t_val;
  } while (i != ts.size());

  return out;
}

template<QuadSplitOptions Opt>
static BL_INLINE BLPoint* quad_split_with_options(QuadRef curve, BLPoint* out) noexcept {
  static_assert(uint32_t(Opt) != 0, "Split options cannot be empty");

  // 2 extrema and 1 terminating `1.0` value.
  constexpr uint32_t kMaxTCount = 3;
  FixedArray<double, kMaxTCount> ts;

  // Find extrema.
  if constexpr ((Opt & QuadSplitOptions::kExtremaXY) == QuadSplitOptions::kExtremaXY) {
    BLPoint extrema_xy = (curve[0] - curve[1]) / (curve[0] - curve[1] * 2.0 + curve[2]);
    double extrema_t0 = bl_min(extrema_xy.x, extrema_xy.y);
    double extrema_t1 = bl_max(extrema_xy.x, extrema_xy.y);

    ts.append_if(extrema_t0, axl::bool_and(extrema_t0 > 0.0, extrema_t0 < 1.0));
    ts.append_if(extrema_t1, axl::bool_and(extrema_t1 > bl_max(extrema_t0, 0.0), extrema_t1 < 1.0));
  }
  else if constexpr ((Opt & QuadSplitOptions::kExtremaX) != QuadSplitOptions::kNone) {
    double extrema_tx = (curve[0].x - curve[1].x) / (curve[0].x - curve[1].x * 2.0 + curve[2].x);
    ts.append_if(extrema_tx, (extrema_tx > 0.0) & (extrema_tx < 1.0));
  }
  else if constexpr ((Opt & QuadSplitOptions::kExtremaY) != QuadSplitOptions::kNone) {
    double extrema_ty = (curve[0].y - curve[1].y) / (curve[0].y - curve[1].y * 2.0 + curve[2].y);
    ts.append_if(extrema_ty, (extrema_ty > 0.0) & (extrema_ty < 1.0));
  }

  // Split the curve into a spline, if necessary.
  if (!ts.is_empty()) {
    // The last T we want is `1.0`.
    ts.append(1.0);
    out = quad_split_with_ts(curve, out, ts.span());
  }

  return out;
}

class QuadCurveTsIter {
public:
  const double* ts;
  const double* ts_end;

  Quad<BLPoint> input;
  Quad<BLPoint> part;
  BLPoint p_tmp_01;
  BLPoint p_tmp_12;

  BL_INLINE QuadCurveTsIter() noexcept
    : ts(nullptr),
      ts_end(nullptr) {}

  BL_INLINE QuadCurveTsIter(QuadRef curve, Span<const double> ts_arr) noexcept {
    reset(curve, ts_arr);
  }

  BL_INLINE void reset(QuadRef curve, Span<const double> ts_arr) noexcept {
    // There must be always at least one T.
    BL_ASSERT(!ts_arr.is_empty());

    input.assign_curve(curve);
    ts = ts_arr.begin();
    ts_end = ts_arr.end();

    // The first iterated curve is the same as if we split left side at `t`. This behaves identically to
    // `split_quad_before()`, however, we keep `p_tmp_01`  and `p_tmp_12` for reuse in `next()` to make
    // the iteration faster.
    double t = *ts++;
    p_tmp_01 = lerp(input[0], input[1], t);
    p_tmp_12 = lerp(input[1], input[2], t);
    part.assign_curve(input[0], p_tmp_01, lerp(p_tmp_01, p_tmp_12, t));
  }

  BL_INLINE bool next() noexcept {
    if (ts >= ts_end) {
      return false;
    }

    double t = *ts++;
    part[0] = part[2];
    part[1] = lerp(p_tmp_01, p_tmp_12, t);

    p_tmp_01 = lerp(input[0], input[1], t);
    p_tmp_12 = lerp(input[1], input[2], t);
    part[2] = lerp(p_tmp_01, p_tmp_12, t);
    return true;
  }
};

//! \name Cubic Bezier Curve Operations
//!
//! Cubic Bezier Curve Formulas
//! ---------------------------
//!
//! Cubic Coefficients:
//!
//! ```
//! A =   -p0 + 3*p1 - 3*p2 + p3 => 3*(p1 - p2) + p3 - p0
//! B =  3*p0 - 6*p1 + 3*p2      => 3*(p0 - 2*p2 + p3)
//! C = -3*p0 + 3*p1             => 3*(p1 - p0)
//! D =    p0                    => p0
//! ```
//!
//! Cubic Evaluation at `t`:
//!
//! ```
//! V = At^3 + Bt^2 + Ct + D     => t(t(At + B) + C) + D => FMA(FMA(FMA(A, t, B), t, C), t, D)
//! ```
//!
//! \{

template<typename T = BLPoint>
struct Cubic {
  //! Number of vertices including both start point, control point(s), and end point.
  static constexpr inline uint32_t kVertexCount = 4u;

  //! Vertex type.
  using Vertex = T;

  //! Curve data as array (not pointing to vertices elsewhere).
  //!
  //! \note Not initialized by default to make it possible to use Cubic in temporary arrays.
  Vertex vtx[kVertexCount];

  BL_INLINE_NODEBUG Cubic() noexcept = default;

  template<typename U>
  BL_INLINE_NODEBUG Cubic(const Cubic<U>& other) noexcept
    : vtx{other.vtx[0], other.vtx[1], other.vtx[2]} {}

  BL_INLINE_NODEBUG explicit Cubic(const Vertex* arr) noexcept
    : vtx{arr[0], arr[1], arr[2]} {}

  BL_INLINE_NODEBUG Cubic(const Vertex& p0, const Vertex& p1, const Vertex& p2, const Vertex& p3) noexcept
    : vtx{p0, p1, p2, p3} {}

  BL_INLINE_NODEBUG Cubic(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3) noexcept
    : vtx{BLPoint(x0, y0), BLPoint(x1, y1), BLPoint(x2, y2), BLPoint(x3, y3)} {}

  template<typename U>
  BL_INLINE_NODEBUG Cubic& operator=(const Cubic<U>& other) noexcept {
    assign_curve(other);
    return *this;
  }

  BL_INLINE_NODEBUG Vertex& operator[](size_t i) noexcept {
    BL_ASSERT(i < kVertexCount);
    return vtx[i];
  }

  BL_INLINE_NODEBUG const Vertex& operator[](size_t i) const noexcept {
    BL_ASSERT(i < kVertexCount);
    return vtx[i];
  }

  BL_INLINE_NODEBUG void assign_vertex(size_t i, Vertex value) noexcept {
    BL_ASSERT(i < kVertexCount);
    vtx[i] = value;
  }

  template<typename U>
  BL_INLINE_NODEBUG void assign_curve(const Cubic<U>& other) noexcept {
    vtx[0] = other.vtx[0];
    vtx[1] = other.vtx[1];
    vtx[2] = other.vtx[2];
    vtx[3] = other.vtx[3];
  }

  BL_INLINE_NODEBUG void assign_curve(Vertex p0, Vertex p1, Vertex p2, Vertex p3) noexcept {
    vtx[0] = p0;
    vtx[1] = p1;
    vtx[2] = p2;
    vtx[3] = p3;
  }
};

template<typename T>
struct Cubic<T*> {
  //! Number of vertices including both start point, control point(s), and end point.
  static constexpr inline uint32_t kVertexCount = 4u;

  //! Vertex type.
  using Vertex = T;

  //! Pointer to data (note it's always zero initialized to make sure it's used safely).
  Vertex* vtx {};

  BL_INLINE_NODEBUG Cubic() noexcept = default;

  BL_INLINE_NODEBUG explicit Cubic(Vertex* storage) noexcept
    : vtx(storage) {}

  // Not explicit - designed to be created implicitly for in/out arguments.
  template<typename U>
  BL_INLINE_NODEBUG Cubic(Cubic<U>& other) noexcept
    : vtx(other.vtx) {}

  // Not explicit - designed to be created implicitly for in/out arguments.
  template<typename U>
  BL_INLINE_NODEBUG Cubic(const Cubic<U>& other) noexcept
    : vtx(other.vtx) {}

  BL_INLINE_NODEBUG Vertex& operator[](size_t i) noexcept {
    BL_ASSERT(i < kVertexCount);
    return vtx[i];
  }

  BL_INLINE_NODEBUG const Vertex& operator[](size_t i) const noexcept {
    BL_ASSERT(i < kVertexCount);
    return vtx[i];
  }

  BL_INLINE_NODEBUG void assign_storage(Vertex* storage) noexcept { vtx = storage; }

  BL_INLINE_NODEBUG void assign_vertex(size_t i, Vertex value) noexcept {
    BL_ASSERT(i < kVertexCount);
    vtx[i] = value;
  }

  template<typename U>
  BL_INLINE_NODEBUG void assign_curve(const Cubic<U>& other) noexcept {
    vtx[0] = other.vtx[0];
    vtx[1] = other.vtx[1];
    vtx[2] = other.vtx[2];
  }

  BL_INLINE_NODEBUG void assign_curve(Vertex p0, Vertex p1, Vertex p2, Vertex p3) noexcept {
    vtx[0] = p0;
    vtx[1] = p1;
    vtx[2] = p2;
    vtx[3] = p3;
  }
};

using CubicRef = Cubic<const BLPoint*>;
using CubicRefMut = Cubic<BLPoint*>;

BL_INLINE_NODEBUG CubicRef cubic_ref(const BLPoint* storage) noexcept { return CubicRef(storage); }
BL_INLINE_NODEBUG CubicRef cubic_ref(CubicRef other) noexcept { return CubicRef(other.vtx); }
BL_INLINE_NODEBUG CubicRef cubic_ref(CubicRefMut other) noexcept { return CubicRef(other.vtx); }
BL_INLINE_NODEBUG CubicRef cubic_ref(const Cubic<BLPoint>& other) noexcept { return CubicRef(other.vtx); }

BL_INLINE_NODEBUG CubicRefMut cubic_out(BLPoint* storage) noexcept { return CubicRefMut(storage); }
BL_INLINE_NODEBUG CubicRefMut cubic_out(Cubic<BLPoint*> other) noexcept { return CubicRefMut(other.vtx); }
BL_INLINE_NODEBUG CubicRefMut cubic_out(Cubic<BLPoint>& other) noexcept { return CubicRefMut(other.vtx); }

//! Coefficients of a cubic curve that can be used to evaluate quad curve at `t`.
struct CubicCoefficients {
  BLPoint a, b, c, d;
};

//! Derivative coefficients of a cubic curve.
struct CubicDerivativeCoefficients {
  BLPoint a, b, c;
};

static BL_INLINE CubicCoefficients cubic_coefficients(CubicRef curve) noexcept {
  BLPoint v1 = curve[1] - curve[0];
  BLPoint v2 = curve[2] - curve[1];
  BLPoint v3 = curve[3] - curve[2];

  return {
    v3 - v2 - v2 + v1,
    3.0 * (v2 - v1),
    3.0 * v1,
    curve[0]
  };
}

static BL_INLINE CubicDerivativeCoefficients cubic_derivative_coefficients(CubicRef curve) noexcept {
  BLPoint v1 = curve.vtx[1] - curve.vtx[0];
  BLPoint v2 = curve.vtx[2] - curve.vtx[1];
  BLPoint v3 = curve.vtx[3] - curve.vtx[2];

  return {
    3.0 * (v3 - v2 - v2 + v1),
    6.0 * (v2 - v1),
    3.0 * (v1)
  };
}

static BL_INLINE BLPoint cubic_evaluate(const CubicCoefficients& coef, const BLPoint& t) noexcept {
  return Math::madd(Math::madd(Math::madd(coef.a, t, coef.b), t, coef.c), t, coef.d);
}

static BL_INLINE BLPoint cubic_evaluate(const CubicCoefficients& coef, double t) noexcept {
  return cubic_evaluate(coef, BLPoint(t, t));
}

static BL_INLINE BLPoint cubic_evaluate(CubicRef curve, double t) noexcept {
  return cubic_evaluate(cubic_coefficients(curve), t);
}

static BL_INLINE BLPoint cubic_evaluate(CubicRef curve, const BLPoint& t) noexcept {
  return cubic_evaluate(cubic_coefficients(curve), t);
}

static BL_INLINE BLPoint cubic_evaluate_precise(CubicRef curve, double t) noexcept {
  BLPoint p01(lerp(curve.vtx[0], curve.vtx[1], t));
  BLPoint p12(lerp(curve.vtx[1], curve.vtx[2], t));
  BLPoint p23(lerp(curve.vtx[2], curve.vtx[3], t));

  return lerp(lerp(p01, p12, t), lerp(p12, p23, t), t);
}

static BL_INLINE BLPoint cubic_evaluate_precise(CubicRef curve, const BLPoint& t) noexcept {
  BLPoint p01(lerp(curve.vtx[0], curve.vtx[1], t));
  BLPoint p12(lerp(curve.vtx[1], curve.vtx[2], t));
  BLPoint p23(lerp(curve.vtx[2], curve.vtx[3], t));
  return lerp(lerp(p01, p12, t), lerp(p12, p23, t), t);
}

static BL_INLINE BLPoint cubic_derivative_at(CubicRef curve, double t) noexcept {
  BLPoint p01 = lerp(curve.vtx[0], curve.vtx[1], t);
  BLPoint p12 = lerp(curve.vtx[1], curve.vtx[2], t);
  BLPoint p23 = lerp(curve.vtx[2], curve.vtx[3], t);

  return 3.0 * (lerp(p12, p23, t) - lerp(p01, p12, t));
}

static BL_INLINE void cubic_extrema_points(CubicRef curve, BLPoint out[2]) noexcept {
  CubicDerivativeCoefficients dc = cubic_derivative_coefficients(curve);

  BLPoint t[2];
  Math::simplified_quad_roots(t, dc.a, dc.b, dc.c);

  t[0] = bl_clamp(t[0], 0.0, 1.0);
  t[1] = bl_clamp(t[1], 0.0, 1.0);

  out[0] = cubic_evaluate_precise(curve, t[0]);
  out[1] = cubic_evaluate_precise(curve, t[1]);
}

static BL_INLINE BLPoint cubic_mid_point(CubicRef curve) noexcept {
  return (curve[0] + curve[3]) * 0.125 + (curve[1] + curve[2]) * 0.375;
}

static BL_INLINE BLPoint cubic_identity(CubicRef curve) noexcept {
  BLPoint v1 = curve.vtx[1] - curve.vtx[0];
  BLPoint v2 = curve.vtx[2] - curve.vtx[1];
  BLPoint v3 = curve.vtx[3] - curve.vtx[2];

  return v3 - v2 - v2 + v1;
}

static BL_INLINE bool is_cubic_flat(CubicRef curve, double f) {
  if (curve.vtx[3] == curve.vtx[0]) {
    BLPoint v = curve.vtx[2] - curve.vtx[1];
    double a = cross(v, curve.vtx[1] - curve.vtx[0]);
    return 0.5625 * a * a <= f * f * magnitude_squared(v);
  }
  else {
    BLPoint v = curve.vtx[3] - curve.vtx[0];
    double a1 = cross(v, curve.vtx[1] - curve.vtx[0]);
    double a2 = cross(v, curve.vtx[2] - curve.vtx[0]);
    return 0.5625 * bl_max(a1 * a1, a2 * a2) <= f * f * magnitude_squared(v);
  }
}

static BL_INLINE void get_cubic_inflection_parameter(CubicRef curve, double& tc, double& tl) noexcept {
  CubicDerivativeCoefficients dc = cubic_derivative_coefficients(curve);

  // To get the inflections C'(t) cross C''(t) = at^2 + bt + c = 0 needs to be solved for 't'.
  // The first coefficient of the quadratic formula is also the denominator.
  double den = cross(dc.b, dc.a);

  if (den != 0) {
    // Two roots might exist, solve with quadratic formula ('tl' is real).
    tc = cross(dc.a, dc.c) / den;
    tl = tc * tc + cross(dc.b, dc.c) / den;

    // If 'tl < 0' there are two complex roots (no need to solve).
    // If 'tl == 0' there is a real double root at tc (cusp case).
    // If 'tl > 0' two real roots exist at 'tc - Sqrt(tl)' and 'tc + Sqrt(tl)'.
    if (tl > 0) {
      tl = Math::sqrt(tl);
    }
  }
  else {
    // One real root might exist, solve linear case ('tl' is NaN).
    tc = -0.5 * cross(dc.c, dc.b) / cross(dc.c, dc.a);
    tl = Math::nan<double>();
  }
}

static BL_INLINE BLPoint cubic_start_tangent(CubicRef curve) noexcept {
  BLPoint out = curve.vtx[1] - curve.vtx[0];
  BLPoint t20 = curve.vtx[2] - curve.vtx[0];
  BLPoint t30 = curve.vtx[3] - curve.vtx[0];

  if (is_zero(out)) out = t20;
  if (is_zero(out)) out = t30;

  return out;
}

static BL_INLINE BLPoint cubic_end_tangent(CubicRef curve) noexcept {
  BLPoint out = curve.vtx[3] - curve.vtx[2];
  BLPoint t31 = curve.vtx[3] - curve.vtx[1];
  BLPoint t30 = curve.vtx[3] - curve.vtx[0];

  if (is_zero(out)) out = t31;
  if (is_zero(out)) out = t30;

  return out;
}

static BL_INLINE void cubic_split(CubicRef curve, CubicRefMut a, CubicRefMut b) noexcept {
  BLPoint p01(lerp(curve.vtx[0], curve.vtx[1]));
  BLPoint p12(lerp(curve.vtx[1], curve.vtx[2]));
  BLPoint p23(lerp(curve.vtx[2], curve.vtx[3]));

  a.vtx[0] = curve.vtx[0];
  a.vtx[1] = p01;
  b.vtx[2] = p23;
  b.vtx[3] = curve.vtx[3];

  a.vtx[2] = lerp(p01, p12);
  b.vtx[1] = lerp(p12, p23);
  a.vtx[3] = lerp(a.vtx[2], b.vtx[1]);
  b.vtx[0] = a.vtx[3];
}

static BL_INLINE void cubic_split(CubicRef curve, CubicRefMut a, CubicRefMut b, double t) noexcept {
  BLPoint p01(lerp(curve.vtx[0], curve.vtx[1], t));
  BLPoint p12(lerp(curve.vtx[1], curve.vtx[2], t));
  BLPoint p23(lerp(curve.vtx[2], curve.vtx[3], t));

  a.vtx[0] = curve.vtx[0];
  a.vtx[1] = p01;
  b.vtx[2] = p23;
  b.vtx[3] = curve.vtx[3];

  a.vtx[2] = lerp(p01, p12, t);
  b.vtx[1] = lerp(p12, p23, t);
  a.vtx[3] = lerp(a.vtx[2], b.vtx[1], t);
  b.vtx[0] = a.vtx[3];
}

static BL_INLINE void cubic_split_before(CubicRef curve, CubicRefMut a, double t) noexcept {
  BLPoint p01(lerp(curve.vtx[0], curve.vtx[1], t));
  BLPoint p12(lerp(curve.vtx[1], curve.vtx[2], t));
  BLPoint p23(lerp(curve.vtx[2], curve.vtx[3], t));

  a.vtx[0] = curve.vtx[0];
  a.vtx[1] = p01;
  a.vtx[2] = lerp(p01, p12, t);
  a.vtx[3] = lerp(a.vtx[2], lerp(p12, p23, t), t);
}

static BL_INLINE void cubic_split_after(CubicRef curve, CubicRefMut b, double t) noexcept {
  BLPoint p01(lerp(curve.vtx[0], curve.vtx[1], t));
  BLPoint p12(lerp(curve.vtx[1], curve.vtx[2], t));
  BLPoint p23(lerp(curve.vtx[2], curve.vtx[3], t));

  b.vtx[3] = curve.vtx[3];
  b.vtx[2] = p23;
  b.vtx[1] = lerp(p12, p23, t);
  b.vtx[0] = lerp(lerp(p01, p12, t), b.vtx[1], t);
}

enum class CubicSplitOptions : uint32_t {
  kExtremaX = 0x1,
  kExtremaY = 0x2,
  kInflections = 0x4,
  kCusp = 0x8,

  kExtremaXY = kExtremaX | kExtremaY,
  kExtremaXYInflectionsCusp = kExtremaXY | kInflections | kCusp
};
BL_DEFINE_ENUM_FLAGS(CubicSplitOptions)

template<CubicSplitOptions Options>
static BL_INLINE BLPoint* cubic_split_with_options(CubicRef curve, BLPoint* out) noexcept {
  static_assert(uint32_t(Options) != 0, "Split options cannot be empty");

  // 4 extrema, 2 inflections, 1 cusp, and 1 terminating `1.0` value.
  constexpr uint32_t kMaxTCount = 4 + 2 + 1 + 1;
  FixedArray<double, kMaxTCount> ts;

  CubicCoefficients cc = cubic_coefficients(curve);

  // Find cusp and/or inflections.
  if constexpr (bl_test_flag(Options, CubicSplitOptions::kCusp | CubicSplitOptions::kInflections)) {
    double q0 = cross(cc.b, cc.a);
    double q1 = cross(cc.c, cc.a);
    double q2 = cross(cc.c, cc.b);

    // Find cusp.
    if constexpr  (bl_test_flag(Options, CubicSplitOptions::kCusp)) {
      double t_cusp = (q1 / q0) * -0.5;
      ts.append_if(t_cusp, (t_cusp > 0.0) & (t_cusp < 1.0));
    }

    // Find inflections.
    if constexpr (bl_test_flag(Options, CubicSplitOptions::kInflections)) {
      ts._increment_size(Math::quad_roots(ts.end(), q0 * 6.0, q1 * 6.0, q2 * 2.0, Math::kAfter0, Math::kBefore1));
    }
  }

  // Find extrema.
  if constexpr (bl_test_flag(Options, CubicSplitOptions::kExtremaX | CubicSplitOptions::kExtremaY)) {
    CubicDerivativeCoefficients dc = cubic_derivative_coefficients(curve);

    if constexpr (bl_test_flag(Options, CubicSplitOptions::kExtremaX)) {
      ts._increment_size(Math::quad_roots(ts.end(), dc.a.x, dc.b.x, dc.c.x, Math::kAfter0, Math::kBefore1));
    }

    if constexpr (bl_test_flag(Options, CubicSplitOptions::kExtremaY)) {
      ts._increment_size(Math::quad_roots(ts.end(), dc.a.y, dc.b.y, dc.c.y, Math::kAfter0, Math::kBefore1));
    }
  }

  // Split the curve into a spline, if necessary.
  if (!ts.is_empty()) {
    // If 2 or more flags were specified, sort Ts, otherwise we have them sorted already.
    if constexpr (!axl::is_power_of_2(uint32_t(Options))) {
      insertion_sort(ts.data(), ts.size());
    }

    // The last T we want is at 1.0.
    ts.append(1.0);

    out[0] = curve.vtx[0];
    BLPoint last = curve.vtx[3];

    size_t i = 0;
    double t_cut = 0.0;

    do {
      double t_val = ts[i++];
      BL_ASSERT(t_val >  0.0);
      BL_ASSERT(t_val <= 1.0);

      // Ignore all Ts which are the same as the previous one (border case).
      if (t_val == t_cut) {
        continue;
      }

      constexpr double k1Div3 = 1.0 / 3.0;
      double dt = (t_val - t_cut) * k1Div3;

      BLPoint tv = BLPoint(t_val, t_val);
      BLPoint tp = Math::madd(Math::madd(Math::madd(cc.a, tv, cc.b), tv, cc.c), tv, cc.d);

      // The last point must be exact.
      if (i == ts.size()) {
        tp = last;
      }

      // Derivative: 3At^2 + 2Bt + c
      //             (3At + 2B)t + c
      BLPoint tc = BLPoint(t_cut, t_cut);
      BLPoint cp1 = Math::madd(Math::madd(cc.a, (tc * 3.0), cc.b * 2.0), tc, cc.c) * dt;
      BLPoint cp2 = Math::madd(Math::madd(cc.a, (tv * 3.0), cc.b * 2.0), tv, cc.c) * dt;

      out[1].reset(out[0] + cp1);
      out[2].reset(tp - cp2);
      out[3].reset(tp);
      out += 3;

      t_cut = t_val;
    } while (i != ts.size());
  }

  return out;
}

static BL_INLINE void approximate_cubic_with_two_quads(CubicRef curve, BLPoint quads[7]) noexcept {
  BLPoint c1 = lerp(curve.vtx[0], curve.vtx[1], 0.75);
  BLPoint c2 = lerp(curve.vtx[3], curve.vtx[2], 0.75);
  BLPoint pm = lerp(c1, c2);

  if (c1 == curve.vtx[0]) {
    c1 = line_vector_intersection_safe(curve.vtx[0], cubic_start_tangent(curve), pm, cubic_derivative_at(curve, 0.5), lerp(curve.vtx[0], pm));
  }

  if (c2 == curve.vtx[3]) {
    c2 = line_vector_intersection_safe(curve.vtx[3], cubic_end_tangent(curve), pm, cubic_derivative_at(curve, 0.5), lerp(curve.vtx[3], pm));
  }

  quads[0] = curve.vtx[0];
  quads[1] = c1;
  quads[2] = pm;
  quads[3] = c2;
  quads[4] = curve.vtx[3];
}

template<typename Callback>
static BL_INLINE BLResult approximate_cubic_with_quads(CubicRef curve, double simplify_tolerance, const Callback& callback) noexcept {
  // Tolerance consists of a prefactor (27/4 * 2^3) combined with `simplify_tolerance`.
  double tolerance_sq = Math::square(54.0 * simplify_tolerance);

  // Smallest parameter step to satisfy tolerance condition.
  double t = Math::pow(tolerance_sq / magnitude_squared(cubic_identity(curve)), 1.0 / 6.0);

  BLPoint cubic[7];
  cubic[3] = curve.vtx[0];
  cubic[4] = curve.vtx[1];
  cubic[5] = curve.vtx[2];
  cubic[6] = curve.vtx[3];

  for (;;) {
    BLPoint quads[5];
    t = bl_min(1.0, t);

    if (t >= 0.999) {
      t = 1.0;
    }

    // Split the cubic:
    //   - cubic[0..3] contains the part before `t`.
    //   - cubic[3..6] contains the part after `t`.
    cubic_split(cubic_ref(cubic + 3), cubic_out(cubic), cubic_out(cubic + 3), t);
    approximate_cubic_with_two_quads(cubic_ref(cubic), quads);

    for (size_t i = 0; i <= 2; i += 2) {
      BL_PROPAGATE(callback(quad_ref(quads + i)));
    }

    if (t >= 1.0) {
      return BL_SUCCESS;
    }

    // Recalculate the parameter.
    double old_t = t;
    t = t / (1.0 - t);

    if (old_t - t < 1e-3) {
      t += 0.01;
    }
  }
}

static BL_INLINE double gauss_legendre_cubic(double wz, double xz, BLPoint dca, BLPoint dcb, BLPoint dcc) noexcept {
  // A cubic's derivative is a quadratic `dca * t^2 + dcb * t + dcc`, so one extra `madd` is needed
  // compared to `gauss_legendre_quadratic()` to evaluate `wz * |B'(xz)|`.
  BLPoint xzv(xz, xz);
  return magnitude(Math::madd(Math::madd(dca, xzv, dcb), xzv, dcc) * wz);
}

static BL_INLINE double cubic_length(CubicRef cubic) noexcept {
  // https://pomax.github.io/bezierinfo/legendre-gauss.html
  // Same Gauss-Legendre quadrature as `quad_length()`. A cubic's derivative is a quadratic (rather
  // than a line), so `|B'(t)|` is the square root of a quartic and needs more nodes for comparable
  // accuracy - `n = 8` is used here.
  CubicDerivativeCoefficients dc = cubic_derivative_coefficients(cubic);

  // Weights and abscissae for `n = 8` (mapped from `[-1, 1]` to `[0, 1]`, weights halved).
  double sum1 = gauss_legendre_cubic(0.05061426814518815, 0.01985507175123185, dc.a, dc.b, dc.c) +
                gauss_legendre_cubic(0.11119051722668725, 0.10166676129318665, dc.a, dc.b, dc.c);
  double sum2 = gauss_legendre_cubic(0.15685332293894365, 0.23723379504183550, dc.a, dc.b, dc.c) +
                gauss_legendre_cubic(0.18134189168918100, 0.40828267875217510, dc.a, dc.b, dc.c);
  double sum3 = gauss_legendre_cubic(0.18134189168918100, 0.59171732124782490, dc.a, dc.b, dc.c) +
                gauss_legendre_cubic(0.15685332293894365, 0.76276620495816450, dc.a, dc.b, dc.c);
  double sum4 = gauss_legendre_cubic(0.11119051722668725, 0.89833323870681335, dc.a, dc.b, dc.c) +
                gauss_legendre_cubic(0.05061426814518815, 0.98014492824876815, dc.a, dc.b, dc.c);
  return (sum1 + sum2) + (sum3 + sum4);
}

static BL_INLINE double cubic_interpolate(CubicRef cubic, double d) noexcept {
  double d1 = length(cubic[1], cubic[0]);
  double d2 = length(cubic[2], cubic[1]);
  double d3 = length(cubic[3], cubic[2]);
  double fallback = (d1 + d2 + d3) / d;

  // Approximate the cumulative length by integrating the speed modeled as a quadratic profile through
  // the control-polygon edge lengths (d1, d2, d3), then solve `f(t) = d` for `t`:
  //   f(t) = (d1 - 2*d2 + d3) * t^3 + 3 * (d2 - d1) * t^2 + 3 * d1 * t   (`t = 1` -> d1 + d2 + d3 = d).
  double roots[3];
  return Math::cubic_roots(roots, d1 - 2.0 * d2 + d3, 3.0 * (d2 - d1), 3.0 * d1, -d, 0.0, 1.0) ? roots[0] : fallback;
}

static BL_INLINE double cubic_parameter_at_length(CubicRef cubic, double length) noexcept {
  double t = cubic_interpolate(cubic, length);

  if (t < 1.0) {
    Cubic c1, c2;
    cubic_split(cubic, c1, c2, t);

    // Refine solution one more time.
    double d = length - cubic_length(c1);
    t += (1.0 - t) * cubic_interpolate(c2, d);
  }

  return t;
}

//! \}

//! \name Conic Bezier Curve Operations
//!
//! Conic Bezier Curve Math
//! -----------------------
//!
//! \{

template<typename T = BLPoint, typename Weight = double>
struct Conic {
  //! Number of vertices including both start point, control point(s), and end point.
  static constexpr inline uint32_t kVertexCount = 3u;

  //! Vertex type.
  using Vertex = T;

  //! Curve data as array (not pointing to vertices elsewhere).
  //!
  //! \note Not initialized by default to make it possible to use Quad in temporary arrays.
  Vertex vtx[kVertexCount];
  Weight w;

  BL_INLINE_NODEBUG Conic() noexcept = default;

  template<typename U>
  BL_INLINE_NODEBUG Conic(const Conic<U>& other, double weight) noexcept
    : vtx{other.vtx[0], other.vtx[1], other.vtx[2]},
      w(weight) {}

  BL_INLINE_NODEBUG explicit Conic(const Vertex* arr, double weight) noexcept
    : vtx{arr[0], arr[1], arr[2]},
      w(weight) {}

  BL_INLINE_NODEBUG Conic(const Vertex& p0, const Vertex& p1, const Vertex& p2, Weight weight) noexcept
    : vtx{p0, p1, p2},
      w(weight) {}

  BL_INLINE_NODEBUG Conic(double x0, double y0, double x1, double y1, double x2, double y2, Weight weight) noexcept
    : vtx{BLPoint(x0, y0), BLPoint(x1, y1), BLPoint(x2, y2)},
      w(weight) {}

  template<typename U>
  BL_INLINE_NODEBUG Conic& operator=(const Conic<U>& other) noexcept {
    assign_curve(other);
    return *this;
  }

  BL_INLINE_NODEBUG Vertex& operator[](size_t i) noexcept {
    BL_ASSERT(i < kVertexCount);
    return vtx[i];
  }

  BL_INLINE_NODEBUG const Vertex& operator[](size_t i) const noexcept {
    BL_ASSERT(i < kVertexCount);
    return vtx[i];
  }

  BL_INLINE_NODEBUG void assign_vertex(size_t i, Vertex value) noexcept {
    BL_ASSERT(i < kVertexCount);
    vtx[i] = value;
  }

  BL_INLINE_NODEBUG void assign_weight(double weight) noexcept {
    w = weight;
  }

  template<typename U>
  BL_INLINE_NODEBUG void assign_curve(const Conic<U>& other) noexcept {
    vtx[0] = other.vtx[0];
    vtx[1] = other.vtx[1];
    vtx[2] = other.vtx[2];
    w = other.w;
  }

  BL_INLINE_NODEBUG void assign_curve(Vertex p0, Vertex p1, Vertex p2, double weight) noexcept {
    vtx[0] = p0;
    vtx[1] = p1;
    vtx[2] = p2;
    w = weight;
  }
};

template<typename T, typename Weight>
struct Conic<T*, Weight> {
  //! Number of vertices including both start point, control point(s), and end point.
  static constexpr inline uint32_t kVertexCount = 3u;

  //! Vertex type.
  using Vertex = T;

  //! Pointer to data (note it's always zero initialized to make sure it's used safely).
  Vertex* vtx {};
  Weight w;

  BL_INLINE_NODEBUG Conic() noexcept = default;

  BL_INLINE_NODEBUG explicit Conic(Vertex* storage, Weight w) noexcept
    : vtx(storage),
      w(w) {}

  // Not explicit - designed to be created implicitly for in/out arguments.
  template<typename UT, typename UW>
  BL_INLINE_NODEBUG Conic(Conic<UT, UW>& other) noexcept
    : vtx(other.vtx),
      w(other.w) {}

  BL_INLINE_NODEBUG Vertex& operator[](size_t i) noexcept {
    BL_ASSERT(i < kVertexCount);
    return vtx[i];
  }

  BL_INLINE_NODEBUG const Vertex& operator[](size_t i) const noexcept {
    BL_ASSERT(i < kVertexCount);
    return vtx[i];
  }

  BL_INLINE_NODEBUG void assign_storage(Vertex* vtx_ptr) noexcept { vtx = vtx_ptr; }

  BL_INLINE_NODEBUG void assign_vertex(size_t i, Vertex value) noexcept {
    BL_ASSERT(i < kVertexCount);
    vtx[i] = value;
  }

  BL_INLINE_NODEBUG void assign_weight(double weight) noexcept {
    w = weight;
  }

  template<typename UT, typename UW>
  BL_INLINE_NODEBUG void assign_curve(const Conic<UT, UW>& other) noexcept {
    vtx[0] = other.vtx[0];
    vtx[1] = other.vtx[1];
    vtx[2] = other.vtx[2];
    w = other.w;
  }

  BL_INLINE_NODEBUG void assign_curve(Vertex p0, Vertex p1, Vertex p2, double weight) noexcept {
    vtx[0] = p0;
    vtx[1] = p1;
    vtx[2] = p2;
    w = weight;
  }
};

using ConicRef = Conic<const BLPoint*, double>;
using ConicRefMut = Conic<BLPoint*, double&>;

BL_INLINE_NODEBUG ConicRef conic_ref(const BLPoint* storage, double w) noexcept { return ConicRef(storage, w); }
BL_INLINE_NODEBUG ConicRef conic_ref(ConicRef other) noexcept { return ConicRef(other.vtx, other.w); }
BL_INLINE_NODEBUG ConicRef conic_ref(ConicRefMut other) noexcept { return ConicRef(other.vtx, other.w); }
BL_INLINE_NODEBUG ConicRef conic_ref(const Conic<BLPoint>& other) noexcept { return ConicRef(other.vtx, other.w); }

BL_INLINE_NODEBUG ConicRefMut conic_out(BLPoint* storage, double& weight) noexcept { return ConicRefMut(storage, weight); }
BL_INLINE_NODEBUG ConicRefMut conic_out(Conic<BLPoint*, double&> other) noexcept { return ConicRefMut(other.vtx, other.w); }
BL_INLINE_NODEBUG ConicRefMut conic_out(Conic<BLPoint, double>& other) noexcept { return ConicRefMut(other.vtx, other.w); }

static BL_INLINE double conic_weight_from_vectors(const BLPoint& p0, const BLPoint& p1, const BLPoint& p2) noexcept {
  BLPoint v1 = p1 - p0;
  BLPoint v2 = p2 - p0;
  return dot(v1, v2) / Math::sqrt(magnitude_squared(v1) * magnitude_squared(v2));
}

//! Static options that can be used to split a conic curve.
enum class ConicSplitOptions : uint32_t {
  kNone = 0x0,
  //! Split at X extrema.
  kExtremaX = 0x1,
  //! Split at Y extrema.
  kExtremaY = 0x2,

  //! Split at X and Y extrema - combines `kExtremaX` and `kExtremaY`.
  kExtremaXY = kExtremaX | kExtremaY
};
BL_DEFINE_ENUM_FLAGS(ConicSplitOptions)

//! Derivative coefficients of a conic curve.
struct ConicDerivativeCoefficients {
  BLPoint a, b, c;
};

static BL_INLINE ConicDerivativeCoefficients conic_derivative_coefficients(const BLPoint p[3], double w) noexcept {
  BLPoint p0 = p[0];
  BLPoint p1 = p[1];
  BLPoint p2 = p[2];

  // Note: These coefficients are missing magnitude (of the denominator)
  BLPoint v1 = p1 - p0;
  BLPoint v2 = p2 - p0;

  return {
    2.0 * (w - 1.0) * v2,
    -4.0 * w * v1 + 2.0 * v2,
    2.0 * w * v1
  };
}

static BL_INLINE void get_projective_points(const BLPoint p[3], double w, BLPoint out[6]) noexcept {
  BLPoint p0 = p[0];
  BLPoint p1 = p[1];
  BLPoint p2 = p[2];

  out[0] = BLPoint(p0.x, 1);
  out[1] = BLPoint(w * p1.x, w);
  out[2] = BLPoint(p2.x, 1);

  out[3] = BLPoint(p0.y, 1);
  out[4] = BLPoint(w * p1.y, w);
  out[5] = BLPoint(p2.y, 1);
}

static BL_INLINE BLPoint eval_conic_precise(const BLPoint p[3], double w, const BLPoint& t) noexcept {
  BLPoint pp[6];
  get_projective_points(p, w, pp);

  BLPoint ppx01(lerp(pp[0], pp[1], t.x));
  BLPoint ppy01(lerp(pp[3], pp[4], t.y));

  BLPoint ppx12(lerp(pp[1], pp[2], t.x));
  BLPoint ppy12(lerp(pp[4], pp[5], t.y));

  BLPoint ppx012(lerp(ppx01, ppx12, t.x));
  BLPoint ppy012(lerp(ppy01, ppy12, t.y));

  return BLPoint(ppx012.x / ppx012.y, ppy012.x / ppy012.y);
}

static BL_INLINE BLPoint conic_extrema_point(const BLPoint p[3], double w) noexcept {
  ConicDerivativeCoefficients dc = conic_derivative_coefficients(p, w);

  BLPoint t[2];
  Math::simplified_quad_roots(t, dc.a, dc.b, dc.c);

  double tx = axl::bool_and(t[0].x > 0.0, t[0].x < 1.0) ? t[0].x : t[1].x;
  double ty = axl::bool_and(t[0].y > 0.0, t[0].y < 1.0) ? t[0].y : t[1].y;
  return eval_conic_precise(p, w, bl_clamp(BLPoint(tx, ty), 0.0, 1.0));
}

static BL_INLINE BLPoint conic_start_tangent(ConicRef curve) noexcept {
  BLPoint out = curve.vtx[1] - curve.vtx[0];
  BLPoint t20 = curve.vtx[2] - curve.vtx[0];

  if (is_zero(out)) {
    out = t20;
  }

  return out;
}

static BL_INLINE BLPoint conic_end_tangent(ConicRef curve) noexcept {
  BLPoint out = curve.vtx[2] - curve.vtx[1];
  BLPoint t20 = curve.vtx[2] - curve.vtx[0];

  if (is_zero(out)) {
    out = t20;
  }

  return out;
}

// Splits a conic curve into two sub-conics at `t = 0.5`.
//
// The split is performed in projective space, where the conic is an ordinary quadratic with control points
// `P0 = (p0, 1)`, `P1 = (w*p1, w)`, `P2 = (p2, 1)`. The De Casteljau midpoints are projected back to affine
// space, which yields both control points and the shared split point.
//
// Both halves have the same standard-form weight `sqrt((1 + w) / 2)`, which is always closer to `1` than
// `w` - repeated halving thus drives a conic of any weight towards a parabola (`w == 1`).
//
// NOTE: Conic weights are positive, so `1 + w` is never zero.
static BL_INLINE void conic_split(ConicRef curve, ConicRefMut a_out, ConicRefMut b_out) noexcept {
  BLPoint p0 = curve[0];
  BLPoint p2 = curve[2];
  double w = curve.w;

  // Projective control points at `t = 0.5` projected back to affine space (`z = (1 + w) / 2` for both).
  double iw = 1.0 / (1.0 + w);
  BLPoint wp1 = curve[1] * w;

  BLPoint a_cp = (p0 + wp1) * iw;       // Left control point.
  BLPoint b_cp = (wp1 + p2) * iw;       // Right control point.
  BLPoint split = (a_cp + b_cp) * 0.5;  // Shared split point (== `(p0 + 2*w*p1 + p2) / (2*(1 + w))`).

  double sub_w = Math::sqrt((1.0 + w) * 0.5);

  a_out.assign_curve(p0, a_cp, split, sub_w);
  b_out.assign_curve(split, b_cp, p2, sub_w);
}

// Splits a conic curve into two sub-conics at parameter `t`.
//
// Like the `t = 0.5` overload, this works in projective space (`P0 = (p0, 1)`, `P1 = (w*p1, w)`,
// `P2 = (p2, 1)`): the homogeneous control points are interpolated with De Casteljau and projected back to
// affine space. Each sub-conic gets its own standard-form weight `z1 / sqrt(z0 * z2)`. Projective points are
// kept as a numerator (`xy`) and a homogeneous weight (`z`) pair instead of using a dedicated 3D vector.
static BL_INLINE void conic_split(ConicRef curve, ConicRefMut a_out, ConicRefMut b_out, double t) noexcept {
  BLPoint p0 = curve[0];
  BLPoint p2 = curve[2];
  double w = curve.w;

  BLPoint wp1 = curve[1] * w;

  BLPoint q01_xy = lerp(p0, wp1, t);
  double q01_z = lerp(1.0, w, t);

  BLPoint q12_xy = lerp(wp1, p2, t);
  double q12_z = lerp(w, 1.0, t);

  BLPoint q012_xy = lerp(q01_xy, q12_xy, t);
  double q012_z = lerp(q01_z, q12_z, t);

  // Project back to affine space - the split point is shared by both halves.
  BLPoint a_cp = q01_xy / q01_z;
  BLPoint b_cp = q12_xy / q12_z;
  BLPoint split = q012_xy / q012_z;

  a_out.assign_curve(p0, a_cp, split, conic_normalized_weight(1.0, q01_z, q012_z));
  b_out.assign_curve(split, b_cp, p2, conic_normalized_weight(q012_z, q12_z, 1.0));
}

// Splits a conic curve `[p0, p1, p2]` with weight `w` into a spline of conics at the curve's axis extrema.
//
// Unlike a non-rational quadratic, the position of a conic's extrema as well as the geometry of each
// sub-curve depend on the weight `w`, so both the split parameters and the sub-curve control points are
// computed in projective (homogeneous) space.
//
// The split is performed by blossoming the projective curve. Each sub-curve over the parameter interval
// `[a, b]` is reconstructed from three blossom values `B[a,a]`, `B[a,b]`, and `B[b,b]`, which are then
// projected back to affine space. Because splitting a conic yields sub-conics with their own weights, the
// weight of each emitted segment is written to `w_out` (one weight per segment). The returned pointer is
// the end of the written points (`out` is advanced by 2 per segment, sharing endpoints); `w_out` is
// advanced by 1 per segment. When the curve has no interior extrema nothing is written and `out` is
// returned unchanged - the caller is expected to use the original curve and weight in that case.
template<ConicSplitOptions Options>
static BL_INLINE BLPoint* split_conic_to_spline(const BLPoint p[3], double w, BLPoint* out, double* w_out) noexcept {
  static_assert(uint32_t(Options) != 0, "Split options cannot be empty");

  // 2 extrema and 1 terminating `1.0` value.
  constexpr uint32_t kMaxTCount = 3;
  FixedArray<double, kMaxTCount> ts;

  // Find extrema by solving the conic's (rational) derivative numerator `a*t^2 + b*t + c` per axis. A
  // well-formed conic arc has at most one extremum per axis in `(0, 1)`, so we pick the in-range root.
  ConicDerivativeCoefficients dc = conic_derivative_coefficients(p, w);
  BLPoint roots[2];
  Math::simplified_quad_roots(roots, dc.a, dc.b, dc.c);

  if constexpr ((Options & ConicSplitOptions::kExtremaXY) == ConicSplitOptions::kExtremaXY) {
    double tx = axl::bool_and(roots[0].x > 0.0, roots[0].x < 1.0) ? roots[0].x : roots[1].x;
    double ty = axl::bool_and(roots[0].y > 0.0, roots[0].y < 1.0) ? roots[0].y : roots[1].y;

    double t0 = bl_min(tx, ty);
    double t1 = bl_max(tx, ty);

    ts.append_if(t0, axl::bool_and(t0 > 0.0, t0 < 1.0));
    ts.append_if(t1, axl::bool_and(t1 > bl_max(t0, 0.0), t1 < 1.0));
  }
  else if constexpr (bl_test_flag(Options, ConicSplitOptions::kExtremaX)) {
    double tx = axl::bool_and(roots[0].x > 0.0, roots[0].x < 1.0) ? roots[0].x : roots[1].x;
    ts.append_if(tx, axl::bool_and(tx > 0.0, tx < 1.0));
  }
  else if constexpr (bl_test_flag(Options, ConicSplitOptions::kExtremaY)) {
    double ty = axl::bool_and(roots[0].y > 0.0, roots[0].y < 1.0) ? roots[0].y : roots[1].y;
    ts.append_if(ty, axl::bool_and(ty > 0.0, ty < 1.0));
  }

  // Split the curve into a spline, if necessary.
  if (!ts.is_empty()) {
    // The last T we want is at `1.0`.
    ts.append(1.0);

    // Projective control points: `P0 = (p0, 1)`, `P1 = (w*p1, w)`, `P2 = (p2, 1)`. We keep the affine
    // numerators `p0`, `w*p1`, `p2` and handle the homogeneous weights `1`, `w`, `1` separately.
    BLPoint p0 = p[0];
    BLPoint wp1 = p[1] * w;
    BLPoint p2 = p[2];

    out[0] = p0;

    double a = 0.0;
    double bw_aa = 1.0; // Projective weight of `B[a, a]` (== 1 for the start point).
    size_t i = 0;
    size_t n = ts.size();

    do {
      double b = ts[i];

      // Blossom `B[a, b]` (the new control point) and `B[b, b]` (the new end point) in projective space.
      double c0_ab = (1.0 - a) * (1.0 - b);
      double c1_ab = a * (1.0 - b) + b * (1.0 - a);
      double c2_ab = a * b;

      double c0_bb = (1.0 - b) * (1.0 - b);
      double c1_bb = 2.0 * b * (1.0 - b);
      double c2_bb = b * b;

      BLPoint cp_xy = p0 * c0_ab + wp1 * c1_ab + p2 * c2_ab;
      double  cp_w  =      c0_ab + w   * c1_ab +      c2_ab;

      BLPoint tp_xy = p0 * c0_bb + wp1 * c1_bb + p2 * c2_bb;
      double  tp_w  =      c0_bb + w   * c1_bb +      c2_bb;

      // Standard-form weight of the sub-conic: `B[a,b].w / sqrt(B[a,a].w * B[b,b].w)`.
      *w_out++ = cp_w / Math::sqrt(bw_aa * tp_w);

      // The last point must be exact.
      BLPoint tp = (++i == n) ? p2 : tp_xy / tp_w;

      out[1].reset(cp_xy / cp_w);
      out[2].reset(tp);
      out += 2;

      a = b;
      bw_aa = tp_w;
    } while (i != n);
  }

  return out;
}

//! Approximates a conic `curve` by a quad spline, invoking `callback` for each emitted quad.
//!
//! The conic is split repeatedly and the weight of each sufficiently-flat sub-conic is dropped, so its
//! control polygon is used directly as the quad. The parameter step is chosen so that dropping the weight
//! stays within `simplify_tolerance`. Unlike elevating the conic to a cubic first and then approximating
//! that cubic with quads (which stacks two approximations), this operates on the conic directly and so
//! avoids the extra error of the intermediate cubic. A conic with `w == 1` is already a quad and is emitted
//! as a single quad.
template<typename Callback>
static BL_INLINE BLResult approximate_conic_with_quads(ConicRef curve, double simplify_tolerance, const Callback& callback) noexcept {
  double w = curve.w;
  BLPoint v = (curve[2] - curve[1]) - (curve[1] - curve[0]);

  // Smallest parameter step so that dropping a sub-conic's weight satisfies the tolerance condition. When
  // `w == 1` (or the control polygon is degenerate) the denominator is zero and `t` is infinite, so the loop
  // is skipped and the whole curve is emitted as a single quad below.
  double t = Math::pow((4.0 * simplify_tolerance * (w + 1.0)) / (bl_abs(w - 1.0) * magnitude(v)), 1.0 / 4.0);

  Conic<BLPoint> c0(curve[0], curve[1], curve[2], w);

  // Peel off `[0, t]` pieces as quads while a single quad doesn't already cover the whole remaining curve.
  while (t > 0.0 && t < 1.0) {
    Conic<BLPoint> c1, c2;
    conic_split(conic_ref(c0), conic_out(c1), conic_out(c2), t);

    BLPoint quad[3] = { c1[0], c1[1], c1[2] };
    BL_PROPAGATE(callback(quad_ref(quad)));

    // Reparametrize so the constant absolute step maps onto the remaining `[t, 1]` sub-conic.
    t = t / (1.0 - t);
    c0 = c2;
  }

  // The remaining sub-conic is flat enough - emit it as a single quad (weight dropped).
  BLPoint quad[3] = { c0[0], c0[1], c0[2] };
  return callback(quad_ref(quad));
}

// \}

//! \name Degree Elevation
//!
//! Helpers that raise a lower-degree segment to a higher degree while preserving its geometry. Degree
//! elevation between polynomial Bezier curves (line -> quad -> cubic) is *exact*: the elevated curve is
//! the same curve, only expressed with more control points, so the start and end points are reproduced
//! bit-for-bit. The conic -> cubic conversion is an *approximation* (see its note).
//!
//! These are used by the path morph effect to unify two structurally-aligned segments of differing
//! degree into a common representation before interpolating them.
//!
//! \{

//! Elevates a line `[p0, p1]` to a quadratic Bezier, writing the single control point to `cp_out`.
//!
//! Exact: the control point is the segment midpoint, `cp = p0 + (1/2)(p1 - p0)`. The endpoints are
//! unchanged.
static BL_INLINE void line_to_quad(const BLPoint& p0, const BLPoint& p1, BLPoint& cp_out) noexcept {
  cp_out = lerp(p0, p1);
}

//! Elevates a line `[p0, p1]` to a cubic Bezier, writing the two control points to `cp1_out`, `cp2_out`.
//!
//! Exact: `cp1 = p0 + (1/3)(p1 - p0)`, `cp2 = p0 + (2/3)(p1 - p0)`. The endpoints are unchanged.
static BL_INLINE void line_to_cubic(const BLPoint& p0, const BLPoint& p1, BLPoint& cp1_out, BLPoint& cp2_out) noexcept {
  BLPoint d = p1 - p0;
  cp1_out = p0 + d * (1.0 / 3.0);
  cp2_out = p0 + d * (2.0 / 3.0);
}

//! Elevates a quadratic `[p0, c, p1]` to a cubic Bezier, writing the two control points to `cp1_out`,
//! `cp2_out`.
//!
//! Exact: `cp1 = p0 + (2/3)(c - p0)`, `cp2 = p1 + (2/3)(c - p1)`. The endpoints are unchanged.
static BL_INLINE void quad_to_cubic(const BLPoint& p0, const BLPoint& c, const BLPoint& p1, BLPoint& cp1_out, BLPoint& cp2_out) noexcept {
  constexpr double k = 2.0 / 3.0;
  cp1_out = p0 + (c - p0) * k;
  cp2_out = p1 + (c - p1) * k;
}

//! Approximates a conic `[p0, c, p1]` of weight `w` by a *single* cubic Bezier, writing the two control
//! points to `cp1_out`, `cp2_out`.
//!
//! \note This is an APPROXIMATION - a rational quadratic cannot in general be represented exactly by a
//! polynomial cubic. The construction `cp1 = p0 + k(c - p0)`, `cp2 = p1 + k(c - p1)` with
//! `k = (4/3) * w / (1 + w)` matches the conic *exactly* at three points - the two endpoints (`t=0`,
//! `t=1`, including their tangent directions) and the curve midpoint (`t=0.5`) - and is otherwise close
//! for shallow arcs. When `w == 1` the conic is a plain quadratic and `k` reduces to `2/3`, so this
//! coincides with the exact \ref quad_to_cubic elevation.
static BL_INLINE void conic_to_cubic(const BLPoint& p0, const BLPoint& c, const BLPoint& p1, double w, BLPoint& cp1_out, BLPoint& cp2_out) noexcept {
  double k = (4.0 / 3.0) * w / (1.0 + w);
  cp1_out = p0 + (c - p0) * k;
  cp2_out = p1 + (c - p1) * k;
}

//! \}

} // {bl::Geometry}

//! \}
//! \endcond

#endif // BLEND2D_GEOMETRY_BEZIER_P_H_INCLUDED
