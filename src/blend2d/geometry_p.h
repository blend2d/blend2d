// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_GEOMETRY_P_H_INCLUDED
#define BLEND2D_GEOMETRY_P_H_INCLUDED

#include "geometry.h"
#include "support/fixedarray_p.h"
#include "support/intops_p.h"
#include "support/lookuptable_p.h"
#include "support/math_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {
namespace Geometry {

using Math::lerp;

static BL_INLINE_NODEBUG bool is_zero(const BLPoint& p) noexcept { return (p.x == 0) & (p.y == 0); }


//! \name Geometry Type Size
//! \{

static BL_INLINE bool is_simple_geometry_type(uint32_t geometry_type) noexcept {
  return geometry_type <= BL_GEOMETRY_TYPE_SIMPLE_LAST;
}

BL_HIDDEN extern const LookupTable<uint8_t, BL_GEOMETRY_TYPE_SIMPLE_LAST + 1> geometry_type_size_table;

//! \}

//! \name Validity Checks
//! \{

static BL_INLINE bool is_valid(const BLSizeI& size) noexcept { return BLInternal::bool_and(size.w > 0, size.h > 0); }
static BL_INLINE bool is_valid(const BLSize& size) noexcept { return BLInternal::bool_and(size.w > 0, size.h > 0); }

static BL_INLINE bool is_valid(const BLBoxI& box) noexcept { return BLInternal::bool_and(box.x0 < box.x1, box.y0 < box.y1); }
static BL_INLINE bool is_valid(const BLBox& box) noexcept { return BLInternal::bool_and(box.x0 < box.x1, box.y0 < box.y1); }

static BL_INLINE bool is_valid(const BLRectI& rect) noexcept {
  OverflowFlag of = 0;
  int x1 = IntOps::add_overflow(rect.x, rect.w, &of);
  int y1 = IntOps::add_overflow(rect.y, rect.h, &of);
  return BLInternal::bool_and(rect.x < x1, rect.y < y1, !of);
}

static BL_INLINE bool is_valid(const BLRect& rect) noexcept {
  double x1 = rect.x + rect.w;
  double y1 = rect.y + rect.h;
  return BLInternal::bool_and(rect.x < x1, rect.y < y1);
}

//! \}

//! \name Vector Operations
//! \{

static BL_INLINE double length_sq(const BLPoint& v) noexcept { return v.x * v.x + v.y * v.y; }
static BL_INLINE double length_sq(const BLPoint& a, const BLPoint& b) noexcept { return length_sq(b - a); }

static BL_INLINE double length(const BLPoint& v) noexcept { return Math::sqrt(length_sq(v)); }
static BL_INLINE double length(const BLPoint& a, const BLPoint& b) noexcept { return Math::sqrt(length_sq(a, b)); }

static BL_INLINE BLPoint normal(const BLPoint& v) noexcept { return BLPoint(-v.y, v.x); }
static BL_INLINE BLPoint unit_vector(const BLPoint& v) noexcept { return v / length(v); }

static BL_INLINE double dot(const BLPoint& a, const BLPoint& b) noexcept { return a.x * b.x + a.y * b.y; }
static BL_INLINE double cross(const BLPoint& a, const BLPoint& b) noexcept { return a.x * b.y - a.y * b.x; }

static BL_INLINE BLPoint line_vector_intersection(const BLPoint& p0, const BLPoint& v0, const BLPoint& p1, const BLPoint& v1) noexcept {
  return p0 + cross(p1 - p0, v1) / cross(v0, v1) * v0;
}

//! \}

//! \name Box/Rect Operations
//! \{

static BL_INLINE void bound(BLBox& box, const BLPoint& p) noexcept {
  box.reset(bl_min(box.x0, p.x), bl_min(box.y0, p.y),
            bl_max(box.x1, p.x), bl_max(box.y1, p.y));
}

static BL_INLINE void bound(BLBox& box, const BLBox& other) noexcept {
  box.reset(bl_min(box.x0, other.x0), bl_min(box.y0, other.y0),
            bl_max(box.x1, other.x1), bl_max(box.y1, other.y1));
}

static BL_INLINE void bound(BLBoxI& box, const BLBoxI& other) noexcept {
  box.reset(bl_min(box.x0, other.x0), bl_min(box.y0, other.y0),
            bl_max(box.x1, other.x1), bl_max(box.y1, other.y1));
}

static BL_INLINE bool intersect(BLBoxI& dst, const BLBoxI& a, const BLBoxI& b) noexcept {
  dst.reset(bl_max(a.x0, b.x0), bl_max(a.y0, b.y0),
            bl_min(a.x1, b.x1), bl_min(a.y1, b.y1));
  return BLInternal::bool_and(dst.x0 < dst.x1,dst.y0 < dst.y1);
}

static BL_INLINE bool intersect(BLBox& dst, const BLBox& a, const BLBox& b) noexcept {
  dst.reset(bl_max(a.x0, b.x0), bl_max(a.y0, b.y0),
            bl_min(a.x1, b.x1), bl_min(a.y1, b.y1));
  return BLInternal::bool_and(dst.x0 < dst.x1, dst.y0 < dst.y1);
}

static BL_INLINE bool subsumes(const BLBoxI& a, const BLBoxI& b) noexcept {
  return BLInternal::bool_and(a.x0 <= b.x0, a.y0 <= b.y0, a.x1 >= b.x1, a.y1 >= b.y1);
}

static BL_INLINE bool subsumes(const BLBox& a, const BLBox& b) noexcept {
  return BLInternal::bool_and(a.x0 <= b.x0, a.y0 <= b.y0, a.x1 >= b.x1, a.y1 >= b.y1);
}

static BL_INLINE bool overlaps(const BLBoxI& a, const BLBoxI& b) noexcept {
  return BLInternal::bool_and(a.x1 > b.x0, a.y1 > b.y0, a.x0 < b.x1, a.y0 < b.y1);
}

static BL_INLINE bool overlaps(const BLBox& a, const BLBox& b) noexcept {
  return BLInternal::bool_and(a.x1 > b.x0, a.y1 > b.y0, a.x0 < b.x1, a.y0 < b.y1);
}

//! \}

//! \name Quadratic Bézier Curve Operations
//!
//! Quadratic Bézier Curve Formulas
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
//! V = At^2 + Bt + C => t(At + B) + C
//! ```
//!
//! \{

static BL_INLINE void get_quad_coefficients(const BLPoint p[3], BLPoint& a, BLPoint& b, BLPoint& c) noexcept {
  BLPoint v1 = p[1] - p[0];
  BLPoint v2 = p[2] - p[1];

  a = v2 - v1;
  b = v1 + v1;
  c = p[0];
}

static BL_INLINE void get_quad_derivative_coefficients(const BLPoint p[3], BLPoint& a, BLPoint& b) noexcept {
  BLPoint v1 = p[1] - p[0];
  BLPoint v2 = p[2] - p[1];

  a = 2.0 * v2 - 2.0 * v1;
  b = 2.0 * v1;
}

static BL_INLINE BLPoint eval_quad(const BLPoint p[3], double t) noexcept {
  BLPoint a, b, c;
  get_quad_coefficients(p, a, b, c);
  return (a * t + b) * t + c;
}

static BL_INLINE BLPoint eval_quad(const BLPoint p[3], const BLPoint& t) noexcept {
  BLPoint a, b, c;
  get_quad_coefficients(p, a, b, c);
  return (a * t + b) * t + c;
}

static BL_INLINE BLPoint eval_quad_precise(const BLPoint p[3], double t) noexcept {
  return lerp(lerp(p[0], p[1], t), lerp(p[1], p[2], t), t);
}

static BL_INLINE BLPoint eval_quad_precise(const BLPoint p[3], const BLPoint& t) noexcept {
  return lerp(lerp(p[0], p[1], t), lerp(p[1], p[2], t), t);
}

static BL_INLINE BLPoint quad_extrema_point(const BLPoint p[3]) noexcept {
  BLPoint t = bl_clamp((p[0] - p[1]) / (p[0] - p[1] * 2.0 + p[2]), 0.0, 1.0);
  return eval_quad_precise(p, t);
}

static BL_INLINE double quad_parameter_at_angle(const BLPoint p[3], double m) noexcept {
  BLPoint qa, qb;
  get_quad_derivative_coefficients(p, qa, qb);

  double aob = dot(qa, qb);
  double axb = cross(qa, qb);

  if (aob == 0.0)
    return 1.0;

  // m * (bx * bx + by * by) / (|ax * by - ay * bx| - m * (ax * bx + ay * by));
  return m * length_sq(qb) / (bl_abs(axb) - m * aob);
}

static BL_INLINE double quad_curvature_metric(const BLPoint p[3]) noexcept {
  return cross(p[2] - p[1], p[1] - p[0]);
}

static BL_INLINE size_t get_quad_offset_cusp_ts(const BLPoint bez[3], double d, double tOut[2]) {
  BLPoint qqa, qqb;
  get_quad_derivative_coefficients(bez, qqa, qqb);

  double bxa = cross(qqb, qqa);
  double boa = dot(qqb, qqa);

  if (bxa == 0)
    return 0;

  double alen2 = length_sq(qqa);
  double blen2 = length_sq(qqb);

  double fac = -1.0 / alen2;
  double sqrt_ = Math::sqrt(boa * boa - alen2 * (blen2 - Math::cbrt(d * d * bxa * bxa)));

  double t0 = fac * (boa + sqrt_);
  double t1 = fac * (boa - sqrt_);

  // We are only interested in (0, 1) range.
  t0 = bl_max(t0, 0.0);

  size_t n = size_t(t0 > 0.0 && t0 < 1.0);
  tOut[0] = t0;
  tOut[n] = t1;
  return n + size_t(t1 > t0 && t1 < 1.0);
}

static BL_INLINE void split_quad(const BLPoint p[3], BLPoint a_out[3], BLPoint b_out[3]) noexcept {
  BLPoint p01(lerp(p[0], p[1]));
  BLPoint p12(lerp(p[1], p[2]));

  a_out[0] = p[0];
  a_out[1] = p01;
  b_out[1] = p12;
  b_out[2] = p[2];
  a_out[2] = lerp(p01, p12);
  b_out[0] = a_out[2];
}

static BL_INLINE void split_quad(const BLPoint p[3], BLPoint a_out[3], BLPoint b_out[3], double t) noexcept {
  BLPoint p01(lerp(p[0], p[1], t));
  BLPoint p12(lerp(p[1], p[2], t));

  a_out[0] = p[0];
  a_out[1] = p01;
  b_out[1] = p12;
  b_out[2] = p[2];
  a_out[2] = lerp(p01, p12, t);
  b_out[0] = a_out[2];
}

static BL_INLINE void split_quad_before(const BLPoint p[3], BLPoint out[3], double t) noexcept {
  BLPoint p01(lerp(p[0], p[1], t));
  BLPoint p12(lerp(p[1], p[2], t));

  out[0] = p[0];
  out[1] = p01;
  out[2] = lerp(p01, p12, t);
}

static BL_INLINE void split_quad_after(const BLPoint p[3], BLPoint out[3], double t) noexcept {
  BLPoint p01(lerp(p[0], p[1], t));
  BLPoint p12(lerp(p[1], p[2], t));

  out[0] = lerp(p01, p12, t);
  out[1] = p12;
  out[2] = p[2];
}

static BL_INLINE void split_quad_between(const BLPoint p[3], BLPoint out[3], double t0, double t1) noexcept {
  BLPoint t0p01 = lerp(p[0], p[1], t0);
  BLPoint t0p12 = lerp(p[1], p[2], t0);

  BLPoint t1p01 = lerp(p[0], p[1], t1);
  BLPoint t1p12 = lerp(p[1], p[2], t1);

  out[0] = lerp(t0p01, t0p12, t0);
  out[1] = lerp(t0p01, t0p12, t1);
  out[2] = lerp(t1p01, t1p12, t1);
}

enum class SplitQuadOptions : uint32_t {
  kXExtrema = 0x1,
  kYExtrema = 0x2,

  kExtremas = kXExtrema | kYExtrema
};
BL_DEFINE_ENUM_FLAGS(SplitQuadOptions)


template<SplitQuadOptions Options>
static BL_INLINE BLPoint* split_quad_to_spline(const BLPoint p[3], BLPoint* out) noexcept {
  static_assert(uint32_t(Options) != 0, "Split options cannot be empty");

  // 2 extrema and 1 terminating `1.0` value.
  constexpr uint32_t kMaxTCount = 3;
  FixedArray<double, kMaxTCount> ts;

  BLPoint Pa, Pb, Pc;
  get_quad_coefficients(p, Pa, Pb, Pc);

  // Find extrema.
  if ((Options & SplitQuadOptions::kExtremas) == SplitQuadOptions::kExtremas) {
    BLPoint extrema_ts = (p[0] - p[1]) / (p[0] - p[1] * 2.0 + p[2]);
    double extremaT0 = bl_min(extrema_ts.x, extrema_ts.y);
    double extremaT1 = bl_max(extrema_ts.x, extrema_ts.y);

    ts.append_if(extremaT0, (extremaT0 > 0.0) & (extremaT0 < 1.0));
    ts.append_if(extremaT1, (extremaT1 > bl_max(extremaT0, 0.0)) & (extremaT1 < 1.0));
  }
  else if (bl_test_flag(Options, SplitQuadOptions::kXExtrema)) {
    double extrema_tx = (p[0].x - p[1].x) / (p[0].x - p[1].x * 2.0 + p[2].x);
    ts.append_if(extrema_tx, (extrema_tx > 0.0) & (extrema_tx < 1.0));
  }
  else if (bl_test_flag(Options, SplitQuadOptions::kYExtrema)) {
    double extrema_ty = (p[0].y - p[1].y) / (p[0].y - p[1].y * 2.0 + p[2].y);
    ts.append_if(extrema_ty, (extrema_ty > 0.0) & (extrema_ty < 1.0));
  }

  // Split the curve into a spline, if necessary.
  if (!ts.is_empty()) {
    // The last T we want is at 1.0.
    ts.append(1.0);

    out[0] = p[0];
    BLPoint last = p[2];

    size_t i = 0;
    double tCut = 0.0;

    do {
      double tVal = ts[i];
      BL_ASSERT(tVal >  0.0);
      BL_ASSERT(tVal <= 1.0);

      double dt = (tVal - tCut) * 0.5;

      // Derivative: 2a*t + b.
      BLPoint cp = (Pa * (tVal * 2.0) + Pb) * dt;
      BLPoint tp = (Pa * tVal + Pb) * tVal + Pc;

      // The last point must be exact.
      if (++i == ts.size())
        tp = last;

      out[1].reset(tp - cp);
      out[2].reset(tp);
      out += 2;

      tCut = tVal;
    } while (i != ts.size());
  }

  return out;
}

//! Converts quadratic curve to cubic curve.
//!
//! \code
//! cubic[0] = q0
//! cubic[1] = q0 + 2/3 * (q1 - q0)
//! cubic[2] = q2 + 2/3 * (q1 - q2)
//! cubic[3] = q2
//! \endcode
static BL_INLINE void quad_to_cubic(const BLPoint p[3], BLPoint cubic_out[4]) noexcept {
  constexpr double k1Div3 = 1.0 / 3.0;
  constexpr double k2Div3 = 2.0 / 3.0;

  BLPoint tmp = p[1] * k2Div3;
  cubic_out[0] = p[0];
  cubic_out[3] = p[2];
  cubic_out[1].reset(cubic_out[0] * k1Div3 + tmp);
  cubic_out[2].reset(cubic_out[2] * k1Div3 + tmp);
}

class QuadCurveTsIter {
public:
  const double* ts;
  const double* ts_end;

  BLPoint input[3];
  BLPoint part[3];
  BLPoint pTmp01;
  BLPoint pTmp12;

  BL_INLINE QuadCurveTsIter() noexcept
    : ts(nullptr),
      ts_end(nullptr) {}

  BL_INLINE QuadCurveTsIter(const BLPoint* input_, const double* ts_, size_t count_) noexcept {
    reset(input_, ts_, count_);
  }

  BL_INLINE void reset(const BLPoint* input_, const double* ts_, size_t count_) noexcept {
    // There must be always at least one T.
    BL_ASSERT(count_ > 0);

    input[0] = input_[0];
    input[1] = input_[1];
    input[2] = input_[2];
    ts = ts_;
    ts_end = ts + count_;

    // The first iterated curve is the same as if we split left side at `t`.
    // This behaves identically to `split_quad_before()`, however, we cache
    // `pTmp01`  and `pTmp12` for reuse in `next()`.
    double t = *ts++;
    pTmp01 = lerp(input[0], input[1], t);
    pTmp12 = lerp(input[1], input[2], t);

    part[0] = input[0];
    part[1] = pTmp01;
    part[2] = lerp(part[1], pTmp12, t);
  }

  BL_INLINE bool next() noexcept {
    if (ts >= ts_end)
      return false;

    double t = *ts++;
    part[0] = part[2];
    part[1] = lerp(pTmp01, pTmp12, t);

    pTmp01 = lerp(input[0], input[1], t);
    pTmp12 = lerp(input[1], input[2], t);
    part[2] = lerp(pTmp01, pTmp12, t);
    return true;
  }
};

//! \}

//! \name Cubic Bézier Curve Operations
//!
//! Cubic Bézier Curve Math
//! -----------------------
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
//! V = At^3 + Bt^2 + Ct + D     => t(t(At + B) + C) + D
//! ```
//!
//! \{

static BL_INLINE void get_cubic_coefficients(const BLPoint p[4], BLPoint& a, BLPoint& b, BLPoint& c, BLPoint& d) noexcept {
  BLPoint v1 = p[1] - p[0];
  BLPoint v2 = p[2] - p[1];
  BLPoint v3 = p[3] - p[2];

  a = v3 - v2 - v2 + v1;
  b = 3.0 * (v2 - v1);
  c = 3.0 * v1;
  d = p[0];
}

static BL_INLINE void get_cubic_derivative_coefficients(const BLPoint p[4], BLPoint& a, BLPoint& b, BLPoint& c) noexcept {
  BLPoint v1 = p[1] - p[0];
  BLPoint v2 = p[2] - p[1];
  BLPoint v3 = p[3] - p[2];

  a = 3.0 * (v3 - v2 - v2 + v1);
  b = 6.0 * (v2 - v1);
  c = 3.0 * v1;
}

static BL_INLINE BLPoint eval_cubic(const BLPoint p[4], double t) noexcept {
  BLPoint a, b, c, d;
  get_cubic_coefficients(p, a, b, c, d);
  return ((a * t + b) * t + c) * t + d;
}

static BL_INLINE BLPoint eval_cubic(const BLPoint p[4], const BLPoint& t) noexcept {
  BLPoint a, b, c, d;
  get_cubic_coefficients(p, a, b, c, d);
  return ((a * t + b) * t + c) * t + d;
}

static BL_INLINE BLPoint eval_cubic_precise(const BLPoint p[4], double t) noexcept {
  BLPoint p01(lerp(p[0], p[1], t));
  BLPoint p12(lerp(p[1], p[2], t));
  BLPoint p23(lerp(p[2], p[3], t));

  return lerp(lerp(p01, p12, t), lerp(p12, p23, t), t);
}

static BL_INLINE BLPoint eval_cubic_precise(const BLPoint p[4], const BLPoint& t) noexcept {
  BLPoint p01(lerp(p[0], p[1], t));
  BLPoint p12(lerp(p[1], p[2], t));
  BLPoint p23(lerp(p[2], p[3], t));
  return lerp(lerp(p01, p12, t), lerp(p12, p23, t), t);
}

static BL_INLINE BLPoint cubic_derivative_at(const BLPoint p[4], double t) noexcept {
  BLPoint p01 = lerp(p[0], p[1], t);
  BLPoint p12 = lerp(p[1], p[2], t);
  BLPoint p23 = lerp(p[2], p[3], t);

  return 3.0 * (lerp(p12, p23, t) - lerp(p01, p12, t));
}

static BL_INLINE void get_cubic_extrema_points(const BLPoint p[4], BLPoint out[2]) noexcept {
  BLPoint a, b, c;
  get_cubic_derivative_coefficients(p, a, b, c);

  BLPoint t[2];
  Math::simplified_quad_roots(t, a, b, c);

  t[0] = bl_clamp(t[0], 0.0, 1.0);
  t[1] = bl_clamp(t[1], 0.0, 1.0);

  out[0] = eval_cubic_precise(p, t[0]);
  out[1] = eval_cubic_precise(p, t[1]);
}

static BL_INLINE BLPoint cubic_mid_point(const BLPoint p[4]) noexcept {
  return (p[0] + p[3]) * 0.125 + (p[1] + p[2]) * 0.375;
}

static BL_INLINE BLPoint cubic_identity(const BLPoint p[4]) noexcept {
  BLPoint v1 = p[1] - p[0];
  BLPoint v2 = p[2] - p[1];
  BLPoint v3 = p[3] - p[2];

  return v3 - v2 - v2 + v1;
}

static BL_INLINE bool is_cubic_flat(const BLPoint p[4], double f) {
  if (p[3] == p[0]) {
    BLPoint v = p[2] - p[1];
    double a = cross(v, p[1] - p[0]);
    return 0.5625 * a * a <= f * f * length_sq(v);
  }
  else {
    BLPoint v = p[3] - p[0];
    double a1 = cross(v, p[1] - p[0]);
    double a2 = cross(v, p[2] - p[0]);
    return 0.5625 * bl_max(a1 * a1, a2 * a2) <= f * f * length_sq(v);
  }
}

static BL_INLINE void get_cubic_inflection_parameter(const BLPoint p[4], double& tc, double& tl) noexcept {
  BLPoint a, b, c;
  get_cubic_derivative_coefficients(p, a, b, c);

  // To get the inflections C'(t) cross C''(t) = at^2 + bt + c = 0 needs to be solved for 't'.
  // The first cooefficient of the quadratic formula is also the denominator.
  double den = cross(b, a);

  if (den != 0) {
    // Two roots might exist, solve with quadratic formula ('tl' is real).
    tc = cross(a, c) / den;
    tl = tc * tc + cross(b, c) / den;

    // If 'tl < 0' there are two complex roots (no need to solve).
    // If 'tl == 0' there is a real double root at tc (cusp case).
    // If 'tl > 0' two real roots exist at 'tc - Sqrt(tl)' and 'tc + Sqrt(tl)'.
    if (tl > 0)
      tl = Math::sqrt(tl);
  }
  else {
    // One real root might exist, solve linear case ('tl' is NaN).
    tc = -0.5 * cross(c, b) / cross(c, a);
    tl = Math::nan<double>();
  }
}

static BL_INLINE BLPoint cubic_start_tangent(const BLPoint p[4]) noexcept {
  BLPoint out = p[1] - p[0];
  BLPoint t20 = p[2] - p[0];
  BLPoint t30 = p[3] - p[0];

  if (is_zero(out)) out = t20;
  if (is_zero(out)) out = t30;

  return out;
}

static BL_INLINE BLPoint cubic_end_tangent(const BLPoint p[4]) noexcept {
  BLPoint out = p[3] - p[2];
  BLPoint t31 = p[3] - p[1];
  BLPoint t30 = p[3] - p[0];

  if (is_zero(out)) out = t31;
  if (is_zero(out)) out = t30;

  return out;
}

static BL_INLINE void split_cubic(const BLPoint p[4], BLPoint a[4], BLPoint b[4]) noexcept {
  BLPoint p01(lerp(p[0], p[1]));
  BLPoint p12(lerp(p[1], p[2]));
  BLPoint p23(lerp(p[2], p[3]));

  a[0] = p[0];
  a[1] = p01;
  b[2] = p23;
  b[3] = p[3];

  a[2] = lerp(p01, p12);
  b[1] = lerp(p12, p23);
  a[3] = lerp(a[2], b[1]);
  b[0] = a[3];
}

static BL_INLINE void split_cubic(const BLPoint p[4], BLPoint a[4], BLPoint b[4], double t) noexcept {
  BLPoint p01(lerp(p[0], p[1], t));
  BLPoint p12(lerp(p[1], p[2], t));
  BLPoint p23(lerp(p[2], p[3], t));

  a[0] = p[0];
  a[1] = p01;
  b[2] = p23;
  b[3] = p[3];

  a[2] = lerp(p01, p12, t);
  b[1] = lerp(p12, p23, t);
  a[3] = lerp(a[2], b[1], t);
  b[0] = a[3];
}

static BL_INLINE void split_cubic_before(const BLPoint p[4], BLPoint a[4], double t) noexcept {
  BLPoint p01(lerp(p[0], p[1], t));
  BLPoint p12(lerp(p[1], p[2], t));
  BLPoint p23(lerp(p[2], p[3], t));

  a[0] = p[0];
  a[1] = p01;
  a[2] = lerp(p01, p12, t);
  a[3] = lerp(a[2], lerp(p12, p23, t), t);
}

static BL_INLINE void split_cubic_after(const BLPoint p[4], BLPoint b[4], double t) noexcept {
  BLPoint p01(lerp(p[0], p[1], t));
  BLPoint p12(lerp(p[1], p[2], t));
  BLPoint p23(lerp(p[2], p[3], t));

  b[3] = p[3];
  b[2] = p23;
  b[1] = lerp(p12, p23, t);
  b[0] = lerp(lerp(p01, p12, t), b[1], t);
}

enum class SplitCubicOptions : uint32_t {
  kXExtremas = 0x1,
  kYExtremas = 0x2,
  kInflections = 0x4,
  kCusp = 0x8,

  kExtremas = kXExtremas | kYExtremas,
  kExtremasInflectionsCusp = kExtremas | kInflections | kCusp
};
BL_DEFINE_ENUM_FLAGS(SplitCubicOptions)

template<SplitCubicOptions Options>
static BL_INLINE BLPoint* split_cubic_to_spline(const BLPoint p[4], BLPoint* out) noexcept {
  static_assert(uint32_t(Options) != 0, "Split options cannot be empty");

  // 4 extrema, 2 inflections, 1 cusp, and 1 terminating `1.0` value.
  constexpr uint32_t kMaxTCount = 4 + 2 + 1 + 1;
  FixedArray<double, kMaxTCount> ts;

  BLPoint Pa, Pb, Pc, Pd;
  get_cubic_coefficients(p, Pa, Pb, Pc, Pd);

  // Find cusp and/or inflections.
  if (bl_test_flag(Options, SplitCubicOptions::kCusp | SplitCubicOptions::kInflections)) {
    double q0 = cross(Pb, Pa);
    double q1 = cross(Pc, Pa);
    double q2 = cross(Pc, Pb);

    // Find cusp.
    if (bl_test_flag(Options, SplitCubicOptions::kCusp)) {
      double tCusp = (q1 / q0) * -0.5;
      ts.append_if(tCusp, (tCusp > 0.0) & (tCusp < 1.0));
    }

    // Find inflections.
    if (bl_test_flag(Options, SplitCubicOptions::kInflections))
      ts._increment_size(Math::quad_roots(ts.end(), q0 * 6.0, q1 * 6.0, q2 * 2.0, Math::kAfter0, Math::kBefore1));
  }

  // Find extrema.
  if (bl_test_flag(Options, SplitCubicOptions::kXExtremas | SplitCubicOptions::kYExtremas)) {
    BLPoint Da, Db, Dc;
    get_cubic_derivative_coefficients(p, Da, Db, Dc);

    if (bl_test_flag(Options, SplitCubicOptions::kXExtremas))
      ts._increment_size(Math::quad_roots(ts.end(), Da.x, Db.x, Dc.x, Math::kAfter0, Math::kBefore1));

    if (bl_test_flag(Options, SplitCubicOptions::kYExtremas))
      ts._increment_size(Math::quad_roots(ts.end(), Da.y, Db.y, Dc.y, Math::kAfter0, Math::kBefore1));
  }

  // Split the curve into a spline, if necessary.
  if (!ts.is_empty()) {
    // If 2 or more flags were specified, sort Ts, otherwise we have them sorted already.
    if (!IntOps::is_power_of_2(uint32_t(Options)))
      insertion_sort(ts.data(), ts.size());

    // The last T we want is at 1.0.
    ts.append(1.0);

    out[0] = p[0];
    BLPoint last = p[3];

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

      BLPoint tp = ((Pa * tVal + Pb) * tVal + Pc) * tVal + Pd;

      // The last point must be exact.
      if (i == ts.size())
        tp = last;

      // Derivative: 3At^2 + 2Bt + c
      //             (3At + 2B)t + c
      BLPoint cp1 { ((Pa * (tCut * 3.0) + Pb * 2.0) * tCut + Pc) * dt };
      BLPoint cp2 { ((Pa * (tVal * 3.0) + Pb * 2.0) * tVal + Pc) * dt };

      out[1].reset(out[0] + cp1);
      out[2].reset(tp - cp2);
      out[3].reset(tp);
      out += 3;

      tCut = tVal;
    } while (i != ts.size());
  }

  return out;
}

static BL_INLINE void approximate_cubic_with_two_quads(const BLPoint p[4], BLPoint quads[7]) noexcept {
  BLPoint c1 = lerp(p[0], p[1], 0.75);
  BLPoint c2 = lerp(p[3], p[2], 0.75);
  BLPoint pm = lerp(c1, c2);

  if (c1 == p[0])
    c1 = line_vector_intersection(p[0], cubic_start_tangent(p), pm, cubic_derivative_at(p, 0.5));

  if (c2 == p[3])
    c2 = line_vector_intersection(p[3], cubic_end_tangent(p), pm, cubic_derivative_at(p, 0.5));

  quads[0] = p[0];
  quads[1] = c1;
  quads[2] = pm;
  quads[3] = c2;
  quads[4] = p[3];
}

template<typename Callback>
static BL_INLINE BLResult approximate_cubic_with_quads(const BLPoint p[4], double simplify_tolerance, const Callback& callback) noexcept {
  // Tolerance consists of a prefactor (27/4 * 2^3) combined with `simplify_tolerance`.
  double tolerance_sq = Math::square(54.0 * simplify_tolerance);

  // Smallest parameter step to satisfy tolerance condition.
  double t = Math::pow(tolerance_sq / length_sq(cubic_identity(p)), 1.0 / 6.0);

  BLPoint cubic[7];
  cubic[3] = p[0];
  cubic[4] = p[1];
  cubic[5] = p[2];
  cubic[6] = p[3];

  for (;;) {
    BLPoint quads[5];
    t = bl_min(1.0, t);

    if (t >= 0.999)
      t = 1.0;

    // Split the cubic:
    //   - cubic[0:3] contains the part before `t`.
    //   - cubic[3:7] contains the part after `t`.
    split_cubic(cubic + 3, cubic, cubic + 3, t);
    approximate_cubic_with_two_quads(cubic, quads);

    for (size_t i = 0; i <= 2; i += 2)
      BL_PROPAGATE(callback(quads + i));

    if (t >= 1.0)
      return BL_SUCCESS;

    // Recalculate the parameter.
    double oldT = t;
    t = t / (1.0 - t);

    if (oldT - t < 1e-3)
      t += 0.01;
  }
}

//! \}

//! \name Conic Bézier Curve Operations
//!
//! Conic Bézier Curve Math
//! -----------------------
//!
//! \{

template<SplitQuadOptions Options>
static BL_INLINE BLPoint* split_conic_to_spline(const BLPoint p[3], BLPoint* out) noexcept {
  static_assert(uint32_t(Options) != 0, "Split options cannot be empty");

  // 2 extremas and 1 terminating `1.0` value.
  constexpr uint32_t kMaxTCount = 3;
  FixedArray<double, kMaxTCount> ts;

  BLPoint Pa, Pb, Pc;
  get_quad_coefficients(p, Pa, Pb, Pc);

  // Find extremas.
  if ((Options & SplitQuadOptions::kExtremas) == SplitQuadOptions::kExtremas) {
    BLPoint extrema_ts = (p[0] - p[1]) / (p[0] - p[1] * 2.0 + p[2]);
    double extremaT0 = bl_min(extrema_ts.x, extrema_ts.y);
    double extremaT1 = bl_max(extrema_ts.x, extrema_ts.y);

    ts.append_if(extremaT0, (extremaT0 > 0.0) & (extremaT0 < 1.0));
    ts.append_if(extremaT1, (extremaT1 > bl_max(extremaT0, 0.0)) & (extremaT1 < 1.0));
  }
  else if (bl_test_flag(Options, SplitQuadOptions::kXExtrema)) {
    double extrema_tx = (p[0].x - p[1].x) / (p[0].x - p[1].x * 2.0 + p[2].x);
    ts.append_if(extrema_tx, (extrema_tx > 0.0) & (extrema_tx < 1.0));
  }
  else if (bl_test_flag(Options, SplitQuadOptions::kYExtrema)) {
    double extrema_ty = (p[0].y - p[1].y) / (p[0].y - p[1].y * 2.0 + p[2].y);
    ts.append_if(extrema_ty, (extrema_ty > 0.0) & (extrema_ty < 1.0));
  }

  // Split the curve into a spline, if necessary.
  if (!ts.is_empty()) {
    // The last T we want is at 1.0.
    ts.append(1.0);

    out[0] = p[0];
    BLPoint last = p[2];

    size_t i = 0;
    double tCut = 0.0;

    do {
      double tVal = ts[i];
      BL_ASSERT(tVal >  0.0);
      BL_ASSERT(tVal <= 1.0);

      double dt = (tVal - tCut) * 0.5;

      // Derivative: 2a*t + b.
      BLPoint cp = (Pa * (tVal * 2.0) + Pb) * dt;
      BLPoint tp = (Pa * tVal + Pb) * tVal + Pc;

      // The last point must be exact.
      if (++i == ts.size())
        tp = last;

      out[1].reset(tp - cp);
      out[2].reset(tp);
      out += 2;

      tCut = tVal;
    } while (i != ts.size());
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

static BL_INLINE void get_projective_points(const BLPoint p[4], BLPoint out[6]) noexcept {
  BLPoint p0 = p[0];
  BLPoint p1 = p[1];
  double w = p[2].x;
  BLPoint p2 = p[3];

  out[0] = BLPoint(p0.x, 1);
  out[1] = BLPoint(w * p1.x,  w);
  out[2] = BLPoint(p2.x,  1);

  out[3] = BLPoint(p0.y, 1);
  out[4] = BLPoint(w * p1.y, w);
  out[5] = BLPoint(p2.y, 1);
}

static BL_INLINE BLPoint eval_conic_precise(const BLPoint p[4], const BLPoint& t) noexcept {
  BLPoint pp[6];
  get_projective_points(p, pp);

  BLPoint ppx01(lerp(pp[0], pp[1], t.x));
  BLPoint ppy01(lerp(pp[3], pp[4], t.y));

  BLPoint ppx12(lerp(pp[1], pp[2], t.x));
  BLPoint ppy12(lerp(pp[4], pp[5], t.y));

  BLPoint ppx012(lerp(ppx01, ppx12, t.x));
  BLPoint ppy012(lerp(ppy01, ppy12, t.y));

  return BLPoint(ppx012.x / ppx012.y, ppy012.x / ppy012.y);
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

//! \}

} // {Geometry}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_GEOMETRY_P_H_INCLUDED
