// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_GEOMETRY_BEZIER_P_H_INCLUDED
#define BLEND2D_GEOMETRY_BEZIER_P_H_INCLUDED

#include <blend2d/geometry/commons_p.h>
#include <blend2d/geometry/tolerance_p.h>
#include <blend2d/support/fixedarray_p.h>
#include <blend2d/support/span_p.h>

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

template<typename T>
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

BL_INLINE_NODEBUG Quad<const BLPoint*> quad_ref(const BLPoint* vtx_ptr) noexcept { return Quad<const BLPoint*>(vtx_ptr); }
BL_INLINE_NODEBUG Quad<const BLPoint*> quad_ref(Quad<const BLPoint*> other) noexcept { return Quad<const BLPoint*>(other.vtx); }
BL_INLINE_NODEBUG Quad<const BLPoint*> quad_ref(const Quad<BLPoint>& other) noexcept { return Quad<const BLPoint*>(other.vtx); }

BL_INLINE_NODEBUG Quad<BLPoint*> quad_out(BLPoint* vtx_ptr) noexcept { return Quad<BLPoint*>(vtx_ptr); }
BL_INLINE_NODEBUG Quad<BLPoint*> quad_out(Quad<BLPoint*> other) noexcept { return Quad<BLPoint*>(other.vtx); }
BL_INLINE_NODEBUG Quad<BLPoint*> quad_out(Quad<BLPoint>& other) noexcept { return Quad<BLPoint*>(other.vtx); }

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

static BL_INLINE QuadCoefficients coefficients_of(QuadRef curve) noexcept {
  BLPoint v1 = curve[1] - curve[0];
  BLPoint v2 = curve[2] - curve[1];
  return QuadCoefficients{v2 - v1, v1 + v1, curve[0]};
};

static BL_INLINE QuadDerivativeCoefficients derivative_coefficients_of(QuadRef curve) noexcept {
  BLPoint v1 = curve[1] - curve[0];
  BLPoint v2 = curve[2] - curve[1];
  return QuadDerivativeCoefficients{2.0 * v2 - 2.0 * v1, 2.0 * v1};
}

static BL_INLINE BLPoint evaluate(const QuadCoefficients& coef, double t) noexcept {
  return (coef.a * t + coef.b) * t + coef.c;
}

static BL_INLINE BLPoint evaluate(const QuadCoefficients& coef, const BLPoint& t) noexcept {
  return (coef.a * t + coef.b) * t + coef.c;
}

static BL_INLINE BLPoint evaluate(QuadRef curve, double t) noexcept {
  return evaluate(coefficients_of(curve), t);
}

static BL_INLINE BLPoint evaluate(QuadRef curve, const BLPoint& t) noexcept {
  return evaluate(coefficients_of(curve), t);
}

static BL_INLINE BLPoint evaluate_precise(QuadRef curve, double t) noexcept {
  return lerp(lerp(curve[0], curve[1], t), lerp(curve[1], curve[2], t), t);
}

static BL_INLINE BLPoint evaluate_precise(QuadRef curve, const BLPoint& t) noexcept {
  return lerp(lerp(curve[0], curve[1], t), lerp(curve[1], curve[2], t), t);
}

static BL_INLINE BLPoint quad_extrema_point(QuadRef curve) noexcept {
  BLPoint v0 = curve[0] - curve[1];
  BLPoint t = bl_clamp(v0 / (v0 - curve[1] + curve[2]), 0.0, 1.0);
  return evaluate_precise(curve, t);
}

static BL_INLINE double quad_parameter_at_angle(QuadRef curve, double m) noexcept {
  QuadDerivativeCoefficients dc = derivative_coefficients_of(curve);

  double aob = dot(dc.a, dc.b);
  double axb = cross(dc.a, dc.b);

  if (aob == 0.0) {
    return 1.0;
  }

  // m * (bx * bx + by * by) / (|ax * by - ay * bx| - m * (ax * bx + ay * by));
  return m * magnitude_squared(dc.b) / (bl_abs(axb) - m * aob);
}

static BL_INLINE double quad_curvature_metric(QuadRef curve) noexcept {
  return cross(curve[2] - curve[1], curve[1] - curve[0]);
}

static BL_INLINE size_t quad_offset_cusp_ts(QuadRef curve, double d, double t_out[2]) {
  QuadDerivativeCoefficients dc = derivative_coefficients_of(curve);

  double bxa = cross(dc.b, dc.a);
  double boa = dot(dc.b, dc.a);

  if (bxa == 0.0) {
    return 0;
  }

  double alen2 = magnitude_squared(dc.a);
  double blen2 = magnitude_squared(dc.b);

  double fac = -1.0 / alen2;
  double sqrt_ = Math::sqrt(boa * boa - alen2 * (blen2 - Math::cbrt(d * d * bxa * bxa)));

  double t0 = fac * (boa + sqrt_);
  double t1 = fac * (boa - sqrt_);

  // We are only interested in (0, 1) range.
  t0 = bl_max(t0, 0.0);

  size_t n = size_t(t0 > 0.0 && t0 < 1.0);
  t_out[0] = t0;
  t_out[n] = t1;
  return n + size_t(t1 > t0 && t1 < 1.0);
}

static BL_INLINE void split(QuadRef curve, QuadRefMut a_out, QuadRefMut b_out) noexcept {
  BLPoint cp0(curve[0]);
  BLPoint cp1(curve[1]);
  BLPoint cp2(curve[2]);

  BLPoint p01(lerp(cp0, cp1));
  BLPoint p12(lerp(cp1, cp2));
  BLPoint p01_p12(lerp(p01, p12));

  a_out.assign_curve(cp0, p01, p01_p12);
  b_out.assign_curve(p01_p12, p12, cp2);
}

static BL_INLINE void split(QuadRef curve, QuadRefMut a_out, QuadRefMut b_out, double t) noexcept {
  BLPoint cp0(curve[0]);
  BLPoint cp1(curve[1]);
  BLPoint cp2(curve[2]);

  BLPoint p01(lerp(cp0, cp1, t));
  BLPoint p12(lerp(cp1, cp2, t));
  BLPoint p01_p12(lerp(p01, p12, t));

  a_out.assign_curve(cp0, p01, p01_p12);
  b_out.assign_curve(p01_p12, p12, cp2);
}

static BL_INLINE void split_before(QuadRef curve, QuadRefMut out, double t) noexcept {
  BLPoint p01(lerp(curve[0], curve[1], t));
  BLPoint p12(lerp(curve[1], curve[2], t));

  out.assign_curve(curve[0], p01, lerp(p01, p12, t));
}

static BL_INLINE void split_after(QuadRef curve, QuadRefMut out, double t) noexcept {
  BLPoint p01(lerp(curve[0], curve[1], t));
  BLPoint p12(lerp(curve[1], curve[2], t));

  out.assign_curve(lerp(p01, p12, t), p12, curve[2]);
}

static BL_INLINE void split_between(QuadRef curve, QuadRefMut out, double t0, double t1) noexcept {
  BLPoint t0p01 = lerp(curve[0], curve[1], t0);
  BLPoint t0p12 = lerp(curve[1], curve[2], t0);
  BLPoint t1p01 = lerp(curve[0], curve[1], t1);
  BLPoint t1p12 = lerp(curve[1], curve[2], t1);

  out.assign_curve(lerp(t0p01, t0p12, t0), lerp(t0p01, t0p12, t1), lerp(t1p01, t1p12, t1));
}

static BL_INLINE BLPoint* split_with_ts(QuadRef curve, BLPoint* out, Span<const double> ts) noexcept {
  size_t i = 0;
  double t_cut = 0.0;

  out[0] = curve[0];
  BLPoint last = curve[2];
  QuadCoefficients qc = coefficients_of(curve);

  do {
    double t_val = ts[i];
    BL_ASSERT(t_val >  0.0);
    BL_ASSERT(t_val <= 1.0);

    double dt = (t_val - t_cut) * 0.5;

    // Derivative: 2a*t + b.
    BLPoint cp = (qc.a * (t_val * 2.0) + qc.b) * dt;
    BLPoint tp = (qc.a * t_val + qc.b) * t_val + qc.c;

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
static BL_INLINE BLPoint* split_with_options(QuadRef curve, BLPoint* out) noexcept {
  static_assert(uint32_t(Opt) != 0, "Split options cannot be empty");

  // 2 extrema and 1 terminating `1.0` value.
  constexpr uint32_t kMaxTCount = 3;
  FixedArray<double, kMaxTCount> ts;

  // Find extrema.
  if constexpr ((Opt & QuadSplitOptions::kExtremaXY) == QuadSplitOptions::kExtremaXY) {
    BLPoint extrema_xy = (curve[0] - curve[1]) / (curve[0] - curve[1] * 2.0 + curve[2]);
    double extrema_t0 = bl_min(extrema_xy.x, extrema_xy.y);
    double extrema_t1 = bl_max(extrema_xy.x, extrema_xy.y);

    ts.append_if(extrema_t0, bool_and(extrema_t0 > 0.0, extrema_t0 < 1.0));
    ts.append_if(extrema_t1, bool_and(extrema_t1 > bl_max(extrema_t0, 0.0), extrema_t1 < 1.0));
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
    out = split_with_ts(curve, out, ts.as_cspan());
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

template<typename T>
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

BL_INLINE_NODEBUG Cubic<const BLPoint*> cubic_ref(const BLPoint* storage) noexcept { return Cubic<const BLPoint*>(storage); }
BL_INLINE_NODEBUG Cubic<const BLPoint*> cubic_ref(Cubic<const BLPoint*> other) noexcept { return Cubic<const BLPoint*>(other.vtx); }
BL_INLINE_NODEBUG Cubic<const BLPoint*> cubic_ref(const Cubic<BLPoint>& other) noexcept { return Cubic<const BLPoint*>(other.vtx); }

BL_INLINE_NODEBUG Cubic<BLPoint*> cubic_out(BLPoint* storage) noexcept { return Cubic<BLPoint*>(storage); }
BL_INLINE_NODEBUG Cubic<BLPoint*> cubic_out(Cubic<BLPoint*> other) noexcept { return Cubic<BLPoint*>(other.vtx); }
BL_INLINE_NODEBUG Cubic<BLPoint*> cubic_out(Cubic<BLPoint>& other) noexcept { return Cubic<BLPoint*>(other.vtx); }

//! Coefficients of a cubic curve that can be used to evaluate quad curve at `t`.
struct CubicCoefficients {
  BLPoint a, b, c, d;
};

//! Derivative coefficients of a cubic curve.
struct CubicDerivativeCoefficients {
  BLPoint a, b, c;
};

static BL_INLINE CubicCoefficients coefficients_of(CubicRef curve) noexcept {
  BLPoint v1 = curve[1] - curve[0];
  BLPoint v2 = curve[2] - curve[1];
  BLPoint v3 = curve[3] - curve[2];

  return CubicCoefficients {
    v3 - v2 - v2 + v1,
    3.0 * (v2 - v1),
    3.0 * v1,
    curve[0]
  };
}

static BL_INLINE CubicDerivativeCoefficients derivative_coefficients_of(CubicRef curve) noexcept {
  BLPoint v1 = curve.vtx[1] - curve.vtx[0];
  BLPoint v2 = curve.vtx[2] - curve.vtx[1];
  BLPoint v3 = curve.vtx[3] - curve.vtx[2];

  return CubicDerivativeCoefficients {
    3.0 * (v3 - v2 - v2 + v1),
    6.0 * (v2 - v1),
    3.0 * v1
  };
}

static BL_INLINE BLPoint evaluate(const CubicCoefficients& coef, double t) noexcept {
  return ((coef.a * t + coef.b) * t + coef.c) * t + coef.d;
}

static BL_INLINE BLPoint evaluate(const CubicCoefficients& coef, const BLPoint& t) noexcept {
  return ((coef.a * t + coef.b) * t + coef.c) * t + coef.d;
}

static BL_INLINE BLPoint evaluate(CubicRef curve, double t) noexcept {
  return evaluate(coefficients_of(curve), t);
}

static BL_INLINE BLPoint evaluate(CubicRef curve, const BLPoint& t) noexcept {
  return evaluate(coefficients_of(curve), t);
}

static BL_INLINE BLPoint evaluate_precise(CubicRef curve, double t) noexcept {
  BLPoint p01(lerp(curve.vtx[0], curve.vtx[1], t));
  BLPoint p12(lerp(curve.vtx[1], curve.vtx[2], t));
  BLPoint p23(lerp(curve.vtx[2], curve.vtx[3], t));

  return lerp(lerp(p01, p12, t), lerp(p12, p23, t), t);
}

static BL_INLINE BLPoint evaluate_precise(CubicRef curve, const BLPoint& t) noexcept {
  BLPoint p01(lerp(curve.vtx[0], curve.vtx[1], t));
  BLPoint p12(lerp(curve.vtx[1], curve.vtx[2], t));
  BLPoint p23(lerp(curve.vtx[2], curve.vtx[3], t));
  return lerp(lerp(p01, p12, t), lerp(p12, p23, t), t);
}

static BL_INLINE BLPoint derivative_at(CubicRef curve, double t) noexcept {
  BLPoint p01 = lerp(curve.vtx[0], curve.vtx[1], t);
  BLPoint p12 = lerp(curve.vtx[1], curve.vtx[2], t);
  BLPoint p23 = lerp(curve.vtx[2], curve.vtx[3], t);

  return 3.0 * (lerp(p12, p23, t) - lerp(p01, p12, t));
}

static BL_INLINE void cubic_extrema_points(CubicRef curve, BLPoint out[2]) noexcept {
  CubicDerivativeCoefficients dc = derivative_coefficients_of(curve);

  BLPoint t[2];
  Math::simplified_quad_roots(t, dc.a, dc.b, dc.c);

  t[0] = bl_clamp(t[0], 0.0, 1.0);
  t[1] = bl_clamp(t[1], 0.0, 1.0);

  out[0] = evaluate_precise(curve, t[0]);
  out[1] = evaluate_precise(curve, t[1]);
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
  CubicDerivativeCoefficients dc = derivative_coefficients_of(curve);

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
    if (tl > 0)
      tl = Math::sqrt(tl);
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

static BL_INLINE void split(CubicRef curve, CubicRefMut a, CubicRefMut b) noexcept {
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

static BL_INLINE void split(CubicRef curve, CubicRefMut a, CubicRefMut b, double t) noexcept {
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

static BL_INLINE void split_before(CubicRef curve, CubicRefMut a, double t) noexcept {
  BLPoint p01(lerp(curve.vtx[0], curve.vtx[1], t));
  BLPoint p12(lerp(curve.vtx[1], curve.vtx[2], t));
  BLPoint p23(lerp(curve.vtx[2], curve.vtx[3], t));

  a.vtx[0] = curve.vtx[0];
  a.vtx[1] = p01;
  a.vtx[2] = lerp(p01, p12, t);
  a.vtx[3] = lerp(a.vtx[2], lerp(p12, p23, t), t);
}

static BL_INLINE void split_after(CubicRef curve, CubicRefMut b, double t) noexcept {
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
static BL_INLINE BLPoint* split_cubic_to_spline(CubicRef curve, BLPoint* out) noexcept {
  static_assert(uint32_t(Options) != 0, "Split options cannot be empty");

  // 4 extrema, 2 inflections, 1 cusp, and 1 terminating `1.0` value.
  constexpr uint32_t kMaxTCount = 4 + 2 + 1 + 1;
  FixedArray<double, kMaxTCount> ts;

  CubicCoefficients cc = coefficients_of(curve);

  // Find cusp and/or inflections.
  if (bl_test_flag(Options, CubicSplitOptions::kCusp | CubicSplitOptions::kInflections)) {
    double q0 = cross(cc.b, cc.a);
    double q1 = cross(cc.c, cc.a);
    double q2 = cross(cc.c, cc.b);

    // Find cusp.
    if (bl_test_flag(Options, CubicSplitOptions::kCusp)) {
      double tCusp = (q1 / q0) * -0.5;
      ts.append_if(tCusp, (tCusp > 0.0) & (tCusp < 1.0));
    }

    // Find inflections.
    if (bl_test_flag(Options, CubicSplitOptions::kInflections))
      ts._increment_size(Math::quad_roots(ts.end(), q0 * 6.0, q1 * 6.0, q2 * 2.0, Math::kAfter0, Math::kBefore1));
  }

  // Find extrema.
  if (bl_test_flag(Options, CubicSplitOptions::kExtremaX | CubicSplitOptions::kExtremaY)) {
    CubicDerivativeCoefficients dc = derivative_coefficients_of(curve);

    if (bl_test_flag(Options, CubicSplitOptions::kExtremaX))
      ts._increment_size(Math::quad_roots(ts.end(), dc.a.x, dc.b.x, dc.c.x, Math::kAfter0, Math::kBefore1));

    if (bl_test_flag(Options, CubicSplitOptions::kExtremaY))
      ts._increment_size(Math::quad_roots(ts.end(), dc.a.y, dc.b.y, dc.c.y, Math::kAfter0, Math::kBefore1));
  }

  // Split the curve into a spline, if necessary.
  if (!ts.is_empty()) {
    // If 2 or more flags were specified, sort Ts, otherwise we have them sorted already.
    if (!IntOps::is_power_of_2(uint32_t(Options)))
      insertion_sort(ts.data(), ts.size());

    // The last T we want is at 1.0.
    ts.append(1.0);

    out[0] = curve.vtx[0];
    BLPoint last = curve.vtx[3];

    size_t i = 0;
    double tCut = 0.0;

    do {
      double tVal = ts[i++];
      BL_ASSERT(tVal >  0.0);
      BL_ASSERT(tVal <= 1.0);

      // Ignore all Ts which are the same as the previous one (border case).
      if (tVal == tCut)
        continue;

      constexpr double k1Div3 = 1.0 / 3.0;
      double dt = (tVal - tCut) * k1Div3;

      BLPoint tp = ((cc.a * tVal + cc.b) * tVal + cc.c) * tVal + cc.d;

      // The last point must be exact.
      if (i == ts.size())
        tp = last;

      // Derivative: 3At^2 + 2Bt + c
      //             (3At + 2B)t + c
      BLPoint cp1 { ((cc.a * (tCut * 3.0) + cc.b * 2.0) * tCut + cc.c) * dt };
      BLPoint cp2 { ((cc.a * (tVal * 3.0) + cc.b * 2.0) * tVal + cc.c) * dt };

      out[1].reset(out[0] + cp1);
      out[2].reset(tp - cp2);
      out[3].reset(tp);
      out += 3;

      tCut = tVal;
    } while (i != ts.size());
  }

  return out;
}

static BL_INLINE void approximate_cubic_with_two_quads(CubicRef curve, BLPoint quads[7]) noexcept {
  BLPoint c1 = lerp(curve.vtx[0], curve.vtx[1], 0.75);
  BLPoint c2 = lerp(curve.vtx[3], curve.vtx[2], 0.75);
  BLPoint pm = lerp(c1, c2);

  if (c1 == curve.vtx[0]) {
    c1 = line_vector_intersection(curve.vtx[0], cubic_start_tangent(curve), pm, derivative_at(curve, 0.5));
  }

  if (c2 == curve.vtx[3]) {
    c2 = line_vector_intersection(curve.vtx[3], cubic_end_tangent(curve), pm, derivative_at(curve, 0.5));
  }

  quads[0] = curve.vtx[0];
  quads[1] = c1;
  quads[2] = pm;
  quads[3] = c2;
  quads[4] = curve.vtx[3];
}

template<typename Callback>
static BL_INLINE BLResult approximate_cubic_with_quads(CubicRef curve, double simplify_tolerance, uint32_t max_split_steps, const Callback& callback) noexcept {
  // Tolerance consists of a prefactor (27/4 * 2^3) combined with `simplify_tolerance`.
  double tolerance_sq = Math::square(54.0 * simplify_tolerance);

  // Smallest parameter step to satisfy tolerance condition.
  double t = Math::pow(tolerance_sq / magnitude_squared(cubic_identity(curve)), 1.0 / 6.0);

  BLPoint cubic[7];
  cubic[3] = curve.vtx[0];
  cubic[4] = curve.vtx[1];
  cubic[5] = curve.vtx[2];
  cubic[6] = curve.vtx[3];

  uint32_t split_steps = 0;

  for (;;) {
    BLPoint quads[5];
    if (split_steps >= max_split_steps) {
      t = 1.0;
    }
    else {
      t = bl_min(1.0, t);

      if (t >= 0.999)
        t = 1.0;
    }

    // Split the cubic:
    //   - cubic[0:3] contains the part before `t`.
    //   - cubic[3:7] contains the part after `t`.
    split(cubic_ref(cubic + 3), cubic_out(cubic), cubic_out(cubic + 3), t);
    approximate_cubic_with_two_quads(cubic_ref(cubic), quads);

    for (size_t i = 0; i <= 2; i += 2)
      BL_PROPAGATE(callback(quads + i));

    if (t >= 1.0)
      return BL_SUCCESS;

    split_steps++;

    // Recalculate the parameter.
    double oldT = t;
    t = t / (1.0 - t);

    if (oldT - t < 1e-3)
      t += 0.01;
  }
}

//! \name Conic Bezier Curve Operations
//!
//! Conic Bezier Curve Math
//! -----------------------
//!
//! \{

static BL_INLINE void get_conic_projective_points(const BLPoint p[4], BLPoint out[6]) noexcept {
  BLPoint p0 = p[0];
  BLPoint p1 = p[1];
  double w = p[2].x;
  BLPoint p2 = p[3];

  out[0] = p0;
  out[1] = BLPoint(1.0, Math::nan<double>());
  out[2] = p1 * w;
  out[3] = BLPoint(w, Math::nan<double>());
  out[4] = p2;
  out[5] = BLPoint(1.0, Math::nan<double>());
}

static BL_INLINE void split_projective_conic(const BLPoint p[6], BLPoint left[6], BLPoint right[6], double t) noexcept {
  BLPoint q01 = lerp(p[0], p[2], t);
  BLPoint w01 = lerp(p[1], p[3], t);
  BLPoint q12 = lerp(p[2], p[4], t);
  BLPoint w12 = lerp(p[3], p[5], t);
  BLPoint q012 = lerp(q01, q12, t);
  BLPoint w012 = lerp(w01, w12, t);

  left[0] = p[0];
  left[1] = p[1];
  left[2] = q01;
  left[3] = w01;
  left[4] = q012;
  left[5] = w012;

  right[0] = q012;
  right[1] = w012;
  right[2] = q12;
  right[3] = w12;
  right[4] = p[4];
  right[5] = p[5];
}

static BL_INLINE void get_conic_derivative_coefficients(const BLPoint p[4], BLPoint& a, BLPoint& b, BLPoint& c) noexcept;

template<QuadSplitOptions Options>
static BL_INLINE BLPoint* split_conic_to_spline(const BLPoint p[4], BLPoint* out) noexcept {
  static_assert(uint32_t(Options) != 0, "Split options cannot be empty");

  // Each axis can contribute up to two extrema.
  constexpr uint32_t kMaxTCount = 4;
  FixedArray<double, kMaxTCount> ts;

  auto append_t = [&](double t) noexcept {
    if (!(t > 0.0 && t < 1.0))
      return;

    size_t i = 0;
    while (i != ts.size() && ts[i] < t)
      i++;

    if (i != ts.size() && bl_abs(ts[i] - t) <= Math::epsilon<double>())
      return;

    ts.append(0.0);
    for (size_t j = ts.size() - 1; j != i; j--)
      ts[j] = ts[j - 1];
    ts[i] = t;
  };

  BLPoint a, b, c;
  get_conic_derivative_coefficients(p, a, b, c);

  auto append_roots = [&](double qa, double qb, double qc) noexcept {
    if (Math::is_near_zero(qa)) {
      if (!Math::is_near_zero(qb))
        append_t(-qc / qb);
      return;
    }

    double roots[2];
    size_t n = Math::quad_roots(roots, qa, qb, qc, Math::kAfter0, Math::kBefore1);
    for (size_t i = 0; i < n; i++)
      append_t(roots[i]);
  };

  if ((Options & QuadSplitOptions::kExtremaXY) == QuadSplitOptions::kExtremaXY || bl_test_flag(Options, QuadSplitOptions::kExtremaX)) {
    append_roots(a.x, b.x, c.x);
  }

  if ((Options & QuadSplitOptions::kExtremaXY) == QuadSplitOptions::kExtremaXY || bl_test_flag(Options, QuadSplitOptions::kExtremaY)) {
    append_roots(a.y, b.y, c.y);
  }

  // Split the curve into a spline, if necessary.
  if (!ts.is_empty()) {
    BLPoint current[6];
    get_conic_projective_points(p, current);

    size_t i = 0;
    double tCut = 0.0;

    do {
      double tVal = ts[i];
      BL_ASSERT(tVal >  0.0);
      BL_ASSERT(tVal <= 1.0);

      double dt = (tVal - tCut) / (1.0 - tCut);
      BLPoint next[6];
      split_projective_conic(current, out, next, dt);

      for (size_t j = 0; j < 6; j++)
        current[j] = next[j];

      out += 6;
      i++;

      tCut = tVal;
    } while (i < ts.size());

    for (size_t j = 0; j < 6; j++)
      out[j] = current[j];
    out += 6;
  }

  return out;
}


static BL_INLINE void get_conic_derivative_coefficients(const BLPoint p[4], BLPoint& a, BLPoint& b, BLPoint& c) noexcept {
  BLPoint p0 = p[0];
  BLPoint p1 = p[1];
  double w = p[2].x;
  BLPoint p2 = p[3];

  // Note: These coefficients are missing magnitude (of the denominator)
  BLPoint v1 = p1 - p0;
  BLPoint v2 = p2 - p0;

  a = 2 * (w - 1) * v2;
  b = -4 * w * v1 + 2 * v2;
  c = 2 * w * v1;
}

static BL_INLINE BLPoint eval_conic_precise(const BLPoint p[4], const BLPoint& t) noexcept {
  BLPoint pp[6];
  get_conic_projective_points(p, pp);

  BLPoint q01x(lerp(pp[0], pp[2], t.x));
  BLPoint w01x(lerp(pp[1], pp[3], t.x));
  BLPoint q12x(lerp(pp[2], pp[4], t.x));
  BLPoint w12x(lerp(pp[3], pp[5], t.x));
  BLPoint q012x(lerp(q01x, q12x, t.x));
  BLPoint w012x(lerp(w01x, w12x, t.x));

  BLPoint q01y(lerp(pp[0], pp[2], t.y));
  BLPoint w01y(lerp(pp[1], pp[3], t.y));
  BLPoint q12y(lerp(pp[2], pp[4], t.y));
  BLPoint w12y(lerp(pp[3], pp[5], t.y));
  BLPoint q012y(lerp(q01y, q12y, t.y));
  BLPoint w012y(lerp(w01y, w12y, t.y));

  return BLPoint(q012x.x / w012x.x, q012y.y / w012y.x);
}


static BL_INLINE void get_conic_extrema_points(const BLPoint p[4], BLPoint out[2]) noexcept {
  BLPoint a, b, c;
  get_conic_derivative_coefficients(p, a, b, c);

  BLPoint t[2];
  Math::simplified_quad_roots(t, a, b, c);

  t[0] = bl_clamp(t[0], 0.0, 1.0);
  t[1] = bl_clamp(t[1], 0.0, 1.0);

  out[0] = eval_conic_precise(p, t[0]);
  out[1] = eval_conic_precise(p, t[1]);
}

// \}

} // {bl::Geometry}

//! \}
//! \endcond

#endif // BLEND2D_GEOMETRY_BEZIER_P_H_INCLUDED
