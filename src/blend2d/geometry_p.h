// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_GEOMETRY_P_H_INCLUDED
#define BLEND2D_GEOMETRY_P_H_INCLUDED

#include "geometry.h"
#include "math_p.h"
#include "support/fixedarray_p.h"
#include "support/intops_p.h"
#include "support/lookuptable_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLGeometry {

//! \name Geometry Type Size
//! \{

static BL_INLINE bool isSimpleGeometryType(uint32_t geometryType) noexcept {
  return geometryType <= BL_GEOMETRY_TYPE_SIMPLE_LAST;
}

BL_HIDDEN extern const BLLookupTable<uint8_t, BL_GEOMETRY_TYPE_SIMPLE_LAST + 1> blGeometryTypeSizeTable;

//! \}

//! \name Validity Checks
//! \{

static BL_INLINE bool isValid(const BLSizeI& size) noexcept { return (size.w > 0) & (size.h > 0); }
static BL_INLINE bool isValid(const BLSize& size) noexcept { return (size.w > 0) & (size.h > 0); }

static BL_INLINE bool isValid(const BLBoxI& box) noexcept { return (box.x0 < box.x1) & (box.y0 < box.y1); }
static BL_INLINE bool isValid(const BLBox& box) noexcept { return (box.x0 < box.x1) & (box.y0 < box.y1); }

static BL_INLINE bool isValid(const BLRectI& rect) noexcept {
  BLOverflowFlag of = 0;
  int x1 = BLIntOps::addOverflow(rect.x, rect.w, &of);
  int y1 = BLIntOps::addOverflow(rect.y, rect.h, &of);
  return (rect.x < x1) & (rect.y < y1) & (!of);
}

static BL_INLINE bool isValid(const BLRect& rect) noexcept {
  double x1 = rect.x + rect.w;
  double y1 = rect.y + rect.h;
  return (rect.x < x1) & (rect.y < y1);
}

//! \}

//! \name Vector Operations
//! \{

static BL_INLINE double lengthSq(const BLPoint& v) noexcept { return v.x * v.x + v.y * v.y; }
static BL_INLINE double lengthSq(const BLPoint& a, const BLPoint& b) noexcept { return lengthSq(b - a); }

static BL_INLINE double length(const BLPoint& v) noexcept { return blSqrt(lengthSq(v)); }
static BL_INLINE double length(const BLPoint& a, const BLPoint& b) noexcept { return blSqrt(lengthSq(a, b)); }

static BL_INLINE BLPoint normal(const BLPoint& v) noexcept { return BLPoint(-v.y, v.x); }
static BL_INLINE BLPoint unitVector(const BLPoint& v) noexcept { return v / length(v); }

static BL_INLINE double dot(const BLPoint& a, const BLPoint& b) noexcept { return a.x * b.x + a.y * b.y; }
static BL_INLINE double cross(const BLPoint& a, const BLPoint& b) noexcept { return a.x * b.y - a.y * b.x; }

static BL_INLINE BLPoint lineVectorIntersection(const BLPoint& p0, const BLPoint& v0, const BLPoint& p1, const BLPoint& v1) noexcept {
  return p0 + cross(p1 - p0, v1) / cross(v0, v1) * v0;
}

//! \}

//! \name Box/Rect Operations
//! \{

static BL_INLINE void bound(BLBox& box, const BLPoint& p) noexcept {
  box.reset(blMin(box.x0, p.x), blMin(box.y0, p.y),
            blMax(box.x1, p.x), blMax(box.y1, p.y));
}

static BL_INLINE void bound(BLBox& box, const BLBox& other) noexcept {
  box.reset(blMin(box.x0, other.x0), blMin(box.y0, other.y0),
            blMax(box.x1, other.x1), blMax(box.y1, other.y1));
}

static BL_INLINE void bound(BLBoxI& box, const BLBoxI& other) noexcept {
  box.reset(blMin(box.x0, other.x0), blMin(box.y0, other.y0),
            blMax(box.x1, other.x1), blMax(box.y1, other.y1));
}

static BL_INLINE bool intersect(BLBoxI& dst, const BLBoxI& a, const BLBoxI& b) noexcept {
  dst.reset(blMax(a.x0, b.x0), blMax(a.y0, b.y0),
            blMin(a.x1, b.x1), blMin(a.y1, b.y1));
  return (dst.x0 < dst.x1) & (dst.y0 < dst.y1);
}

static BL_INLINE bool intersect(BLBox& dst, const BLBox& a, const BLBox& b) noexcept {
  dst.reset(blMax(a.x0, b.x0), blMax(a.y0, b.y0),
            blMin(a.x1, b.x1), blMin(a.y1, b.y1));
  return (dst.x0 < dst.x1) & (dst.y0 < dst.y1);
}

static BL_INLINE bool subsumes(const BLBoxI& a, const BLBoxI& b) noexcept { return (a.x0 <= b.x0) & (a.y0 <= b.y0) & (a.x1 >= b.x1) & (a.y1 >= b.y1); }
static BL_INLINE bool subsumes(const BLBox& a, const BLBox& b) noexcept { return (a.x0 <= b.x0) & (a.y0 <= b.y0) & (a.x1 >= b.x1) & (a.y1 >= b.y1); }

static BL_INLINE bool overlaps(const BLBoxI& a, const BLBoxI& b) noexcept { return (a.x1 > b.x0) & (a.y1 > b.y0) & (a.x0 < b.x1) & (a.y0 < b.y1); }
static BL_INLINE bool overlaps(const BLBox& a, const BLBox& b) noexcept { return (a.x1 > b.x0) & (a.y1 > b.y0) & (a.x0 < b.x1) & (a.y0 < b.y1); }

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

static BL_INLINE void getQuadCoefficients(const BLPoint p[3], BLPoint& a, BLPoint& b, BLPoint& c) noexcept {
  BLPoint v1 = p[1] - p[0];
  BLPoint v2 = p[2] - p[1];

  a = v2 - v1;
  b = v1 + v1;
  c = p[0];
}

static BL_INLINE void getQuadDerivativeCoefficients(const BLPoint p[3], BLPoint& a, BLPoint& b) noexcept {
  BLPoint v1 = p[1] - p[0];
  BLPoint v2 = p[2] - p[1];

  a = 2.0 * v2 - 2.0 * v1;
  b = 2.0 * v1;
}

static BL_INLINE BLPoint evalQuad(const BLPoint p[3], double t) noexcept {
  BLPoint a, b, c;
  getQuadCoefficients(p, a, b, c);
  return (a * t + b) * t + c;
}

static BL_INLINE BLPoint evalQuad(const BLPoint p[3], const BLPoint& t) noexcept {
  BLPoint a, b, c;
  getQuadCoefficients(p, a, b, c);
  return (a * t + b) * t + c;
}

static BL_INLINE BLPoint evalQuadPrecise(const BLPoint p[3], double t) noexcept {
  return blLerp(blLerp(p[0], p[1], t), blLerp(p[1], p[2], t), t);
}

static BL_INLINE BLPoint evalQuadPrecise(const BLPoint p[3], const BLPoint& t) noexcept {
  return blLerp(blLerp(p[0], p[1], t), blLerp(p[1], p[2], t), t);
}

static BL_INLINE BLPoint quadExtremaPoint(const BLPoint p[3]) noexcept {
  BLPoint t = blClamp((p[0] - p[1]) / (p[0] - p[1] * 2.0 + p[2]), 0.0, 1.0);
  return evalQuadPrecise(p, t);
}

static BL_INLINE double quadParameterAtAngle(const BLPoint p[3], double m) noexcept {
  BLPoint qa, qb;
  getQuadDerivativeCoefficients(p, qa, qb);

  double aob = dot(qa, qb);
  double axb = cross(qa, qb);

  if (aob == 0.0)
    return 1.0;

  // m * (bx * bx + by * by) / (|ax * by - ay * bx| - m * (ax * bx + ay * by));
  return m * lengthSq(qb) / (blAbs(axb) - m * aob);
}

static BL_INLINE double quadCurvatureMetric(const BLPoint p[3]) noexcept {
  return cross(p[2] - p[1], p[1] - p[0]);
}

static BL_INLINE size_t getQuadOffsetCuspTs(const BLPoint bez[3], double d, double tOut[2]) {
  BLPoint qqa, qqb;
  getQuadDerivativeCoefficients(bez, qqa, qqb);

  double bxa = cross(qqb, qqa);
  double boa = dot(qqb, qqa);

  if (bxa == 0)
    return 0;

  double alen2 = lengthSq(qqa);
  double blen2 = lengthSq(qqb);

  double fac = -1.0 / alen2;
  double sqrt_ = blSqrt(boa * boa - alen2 * (blen2 - blCbrt(d * d * bxa * bxa)));

  double t0 = fac * (boa + sqrt_);
  double t1 = fac * (boa - sqrt_);

  // We are only interested in (0, 1) range.
  t0 = blMax(t0, 0.0);

  size_t n = size_t(t0 > 0.0 && t0 < 1.0);
  tOut[0] = t0;
  tOut[n] = t1;
  return n + size_t(t1 > t0 && t1 < 1.0);
}

static BL_INLINE void splitQuad(const BLPoint p[3], BLPoint aOut[3], BLPoint bOut[3]) noexcept {
  BLPoint p01(blLerp(p[0], p[1]));
  BLPoint p12(blLerp(p[1], p[2]));

  aOut[0] = p[0];
  aOut[1] = p01;
  bOut[1] = p12;
  bOut[2] = p[2];
  aOut[2] = blLerp(p01, p12);
  bOut[0] = aOut[2];
}

static BL_INLINE void splitQuad(const BLPoint p[3], BLPoint aOut[3], BLPoint bOut[3], double t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));

  aOut[0] = p[0];
  aOut[1] = p01;
  bOut[1] = p12;
  bOut[2] = p[2];
  aOut[2] = blLerp(p01, p12, t);
  bOut[0] = aOut[2];
}

static BL_INLINE void splitQuadBefore(const BLPoint p[3], BLPoint out[3], double t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));

  out[0] = p[0];
  out[1] = p01;
  out[2] = blLerp(p01, p12, t);
}

static BL_INLINE void splitQuadAfter(const BLPoint p[3], BLPoint out[3], double t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));

  out[0] = blLerp(p01, p12, t);
  out[1] = p12;
  out[2] = p[2];
}

static BL_INLINE void splitQuadBetween(const BLPoint p[3], BLPoint out[3], double t0, double t1) noexcept {
  BLPoint t0p01 = blLerp(p[0], p[1], t0);
  BLPoint t0p12 = blLerp(p[1], p[2], t0);

  BLPoint t1p01 = blLerp(p[0], p[1], t1);
  BLPoint t1p12 = blLerp(p[1], p[2], t1);

  out[0] = blLerp(t0p01, t0p12, t0);
  out[1] = blLerp(t0p01, t0p12, t1);
  out[2] = blLerp(t1p01, t1p12, t1);
}

enum class SplitQuadOptions : uint32_t {
  kXExtrema = 0x1,
  kYExtrema = 0x2,

  kExtremas = kXExtrema | kYExtrema
};
BL_DEFINE_ENUM_FLAGS(SplitQuadOptions)


template<SplitQuadOptions Options>
static BL_INLINE BLPoint* splitQuadToSpline(const BLPoint p[3], BLPoint* out) noexcept {
  static_assert(uint32_t(Options) != 0, "Split options cannot be empty");

  // 2 extremas and 1 terminating `1.0` value.
  constexpr uint32_t kMaxTCount = 3;
  BLFixedArray<double, kMaxTCount> ts;

  BLPoint Pa, Pb, Pc;
  getQuadCoefficients(p, Pa, Pb, Pc);

  // Find extremas.
  if ((Options & SplitQuadOptions::kExtremas) == SplitQuadOptions::kExtremas) {
    BLPoint extremaTs = (p[0] - p[1]) / (p[0] - p[1] * 2.0 + p[2]);
    double extremaT0 = blMin(extremaTs.x, extremaTs.y);
    double extremaT1 = blMax(extremaTs.x, extremaTs.y);

    ts.appendIf(extremaT0, (extremaT0 > 0.0) & (extremaT0 < 1.0));
    ts.appendIf(extremaT1, (extremaT1 > blMax(extremaT0, 0.0)) & (extremaT1 < 1.0));
  }
  else if (blTestFlag(Options, SplitQuadOptions::kXExtrema)) {
    double extremaTx = (p[0].x - p[1].x) / (p[0].x - p[1].x * 2.0 + p[2].x);
    ts.appendIf(extremaTx, (extremaTx > 0.0) & (extremaTx < 1.0));
  }
  else if (blTestFlag(Options, SplitQuadOptions::kYExtrema)) {
    double extremaTy = (p[0].y - p[1].y) / (p[0].y - p[1].y * 2.0 + p[2].y);
    ts.appendIf(extremaTy, (extremaTy > 0.0) & (extremaTy < 1.0));
  }

  // Split the curve into a spline, if necessary.
  if (!ts.empty()) {
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

//! Coverts quadratic curve to cubic curve.
//!
//! \code
//! cubic[0] = q0
//! cubic[1] = q0 + 2/3 * (q1 - q0)
//! cubic[2] = q2 + 2/3 * (q1 - q2)
//! cubic[3] = q2
//! \endcode
static BL_INLINE void quadToCubic(const BLPoint p[3], BLPoint cubicOut[4]) noexcept {
  constexpr double k1Div3 = 1.0 / 3.0;
  constexpr double k2Div3 = 2.0 / 3.0;

  BLPoint tmp = p[1] * k2Div3;
  cubicOut[0] = p[0];
  cubicOut[3] = p[2];
  cubicOut[1].reset(cubicOut[0] * k1Div3 + tmp);
  cubicOut[2].reset(cubicOut[2] * k1Div3 + tmp);
}

class QuadCurveTsIter {
public:
  const double* ts;
  const double* tsEnd;

  BLPoint input[3];
  BLPoint part[3];
  BLPoint pTmp01;
  BLPoint pTmp12;

  BL_INLINE QuadCurveTsIter() noexcept
    : ts(nullptr),
      tsEnd(nullptr) {}

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
    tsEnd = ts + count_;

    // The first iterated curve is the same as if we split left side at `t`.
    // This behaves identically to `splitQuadBefore()`, however, we cache
    // `pTmp01`  and `pTmp12` for reuse in `next()`.
    double t = *ts++;
    pTmp01 = blLerp(input[0], input[1], t);
    pTmp12 = blLerp(input[1], input[2], t);

    part[0] = input[0];
    part[1] = pTmp01;
    part[2] = blLerp(part[1], pTmp12, t);
  }

  BL_INLINE bool next() noexcept {
    if (ts >= tsEnd)
      return false;

    double t = *ts++;
    part[0] = part[2];
    part[1] = blLerp(pTmp01, pTmp12, t);

    pTmp01 = blLerp(input[0], input[1], t);
    pTmp12 = blLerp(input[1], input[2], t);
    part[2] = blLerp(pTmp01, pTmp12, t);
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


static BL_INLINE void getCubicCoefficients(const BLPoint p[4], BLPoint& a, BLPoint& b, BLPoint& c, BLPoint& d) noexcept {
  BLPoint v1 = p[1] - p[0];
  BLPoint v2 = p[2] - p[1];
  BLPoint v3 = p[3] - p[2];

  a = v3 - v2 - v2 + v1;
  b = 3.0 * (v2 - v1);
  c = 3.0 * v1;
  d = p[0];
}

static BL_INLINE void getCubicDerivativeCoefficients(const BLPoint p[4], BLPoint& a, BLPoint& b, BLPoint& c) noexcept {
  BLPoint v1 = p[1] - p[0];
  BLPoint v2 = p[2] - p[1];
  BLPoint v3 = p[3] - p[2];

  a = 3.0 * (v3 - v2 - v2 + v1);
  b = 6.0 * (v2 - v1);
  c = 3.0 * v1;
}

static BL_INLINE BLPoint evalCubic(const BLPoint p[4], double t) noexcept {
  BLPoint a, b, c, d;
  getCubicCoefficients(p, a, b, c, d);
  return ((a * t + b) * t + c) * t + d;
}

static BL_INLINE BLPoint evalCubic(const BLPoint p[4], const BLPoint& t) noexcept {
  BLPoint a, b, c, d;
  getCubicCoefficients(p, a, b, c, d);
  return ((a * t + b) * t + c) * t + d;
}

static BL_INLINE BLPoint evalCubicPrecise(const BLPoint p[4], double t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));
  BLPoint p23(blLerp(p[2], p[3], t));

  return blLerp(blLerp(p01, p12, t), blLerp(p12, p23, t), t);
}

static BL_INLINE BLPoint evalCubicPrecise(const BLPoint p[4], const BLPoint& t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));
  BLPoint p23(blLerp(p[2], p[3], t));
  return blLerp(blLerp(p01, p12, t), blLerp(p12, p23, t), t);
}

static BL_INLINE BLPoint cubicDerivativeAt(const BLPoint p[4], double t) noexcept {
  BLPoint p01 = blLerp(p[0], p[1], t);
  BLPoint p12 = blLerp(p[1], p[2], t);
  BLPoint p23 = blLerp(p[2], p[3], t);

  return 3.0 * (blLerp(p12, p23, t) - blLerp(p01, p12, t));
}

static BL_INLINE void getCubicExtremaPoints(const BLPoint p[4], BLPoint out[2]) noexcept {
  BLPoint a, b, c;
  getCubicDerivativeCoefficients(p, a, b, c);

  BLPoint t[2];
  blSimplifiedQuadRoots(t, a, b, c);

  t[0] = blClamp(t[0], 0.0, 1.0);
  t[1] = blClamp(t[1], 0.0, 1.0);

  out[0] = evalCubicPrecise(p, t[0]);
  out[1] = evalCubicPrecise(p, t[1]);
}

static BL_INLINE BLPoint cubicMidPoint(const BLPoint p[4]) noexcept {
  return (p[0] + p[3]) * 0.125 + (p[1] + p[2]) * 0.375;
}

static BL_INLINE BLPoint cubicIdentity(const BLPoint p[4]) noexcept {
  BLPoint v1 = p[1] - p[0];
  BLPoint v2 = p[2] - p[1];
  BLPoint v3 = p[3] - p[2];

  return v3 - v2 - v2 + v1;
}

static BL_INLINE bool isCubicFlat(const BLPoint p[4], double f) {
  if (p[3] == p[0]) {
    BLPoint v = p[2] - p[1];
    double a = cross(v, p[1] - p[0]);
    return 0.5625 * a * a <= f * f * lengthSq(v);
  }
  else {
    BLPoint v = p[3] - p[0];
    double a1 = cross(v, p[1] - p[0]);
    double a2 = cross(v, p[2] - p[0]);
    return 0.5625 * blMax(a1 * a1, a2 * a2) <= f * f * lengthSq(v);
  }
}

static BL_INLINE void getCubicInflectionParameter(const BLPoint p[4], double& tc, double& tl) noexcept {
  BLPoint a, b, c;
  getCubicDerivativeCoefficients(p, a, b, c);

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
      tl = blSqrt(tl);
  }
  else {
    // One real root might exist, solve linear case ('tl' is NaN).
    tc = -0.5 * cross(c, b) / cross(c, a);
    tl = blNaN<double>();
  }
}

static BL_INLINE BLPoint cubicStartTangent(const BLPoint p[4]) noexcept {
  BLPoint out = p[1] - p[0];
  BLPoint t20 = p[2] - p[0];
  BLPoint t30 = p[3] - p[0];

  if (blIsZero(out)) out = t20;
  if (blIsZero(out)) out = t30;

  return out;
}

static BL_INLINE BLPoint cubicEndTangent(const BLPoint p[4]) noexcept {
  BLPoint out = p[3] - p[2];
  BLPoint t31 = p[3] - p[1];
  BLPoint t30 = p[3] - p[0];

  if (blIsZero(out)) out = t31;
  if (blIsZero(out)) out = t30;

  return out;
}

static BL_INLINE void splitCubic(const BLPoint p[4], BLPoint a[4], BLPoint b[4]) noexcept {
  BLPoint p01(blLerp(p[0], p[1]));
  BLPoint p12(blLerp(p[1], p[2]));
  BLPoint p23(blLerp(p[2], p[3]));

  a[0] = p[0];
  a[1] = p01;
  b[2] = p23;
  b[3] = p[3];

  a[2] = blLerp(p01, p12);
  b[1] = blLerp(p12, p23);
  a[3] = blLerp(a[2], b[1]);
  b[0] = a[3];
}

static BL_INLINE void splitCubic(const BLPoint p[4], BLPoint a[4], BLPoint b[4], double t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));
  BLPoint p23(blLerp(p[2], p[3], t));

  a[0] = p[0];
  a[1] = p01;
  b[2] = p23;
  b[3] = p[3];

  a[2] = blLerp(p01, p12, t);
  b[1] = blLerp(p12, p23, t);
  a[3] = blLerp(a[2], b[1], t);
  b[0] = a[3];
}

static BL_INLINE void splitCubicBefore(const BLPoint p[4], BLPoint a[4], double t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));
  BLPoint p23(blLerp(p[2], p[3], t));

  a[0] = p[0];
  a[1] = p01;
  a[2] = blLerp(p01, p12, t);
  a[3] = blLerp(a[2], blLerp(p12, p23, t), t);
}

static BL_INLINE void splitCubicAfter(const BLPoint p[4], BLPoint b[4], double t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));
  BLPoint p23(blLerp(p[2], p[3], t));

  b[3] = p[3];
  b[2] = p23;
  b[1] = blLerp(p12, p23, t);
  b[0] = blLerp(blLerp(p01, p12, t), b[1], t);
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
static BL_INLINE BLPoint* splitCubicToSpline(const BLPoint p[4], BLPoint* out) noexcept {
  static_assert(uint32_t(Options) != 0, "Split options cannot be empty");

  // 4 extremas, 2 inflections, 1 cusp, and 1 terminating `1.0` value.
  constexpr uint32_t kMaxTCount = 4 + 2 + 1 + 1;
  BLFixedArray<double, kMaxTCount> ts;

  BLPoint Pa, Pb, Pc, Pd;
  getCubicCoefficients(p, Pa, Pb, Pc, Pd);

  // Find cusp and/or inflections.
  if (blTestFlag(Options, SplitCubicOptions::kCusp | SplitCubicOptions::kInflections)) {
    double q0 = cross(Pb, Pa);
    double q1 = cross(Pc, Pa);
    double q2 = cross(Pc, Pb);

    // Find cusp.
    if (blTestFlag(Options, SplitCubicOptions::kCusp)) {
      double tCusp = (q1 / q0) * -0.5;
      ts.appendIf(tCusp, (tCusp > 0.0) & (tCusp < 1.0));
    }

    // Find inflections.
    if (blTestFlag(Options, SplitCubicOptions::kInflections))
      ts._incrementSize(blQuadRoots(ts.end(), q0 * 6.0, q1 * 6.0, q2 * 2.0, BL_M_AFTER_0, BL_M_BEFORE_1));
  }

  // Find extremas.
  if (blTestFlag(Options, SplitCubicOptions::kXExtremas | SplitCubicOptions::kYExtremas)) {
    BLPoint Da, Db, Dc;
    getCubicDerivativeCoefficients(p, Da, Db, Dc);

    if (blTestFlag(Options, SplitCubicOptions::kXExtremas))
      ts._incrementSize(blQuadRoots(ts.end(), Da.x, Db.x, Dc.x, BL_M_AFTER_0, BL_M_BEFORE_1));

    if (blTestFlag(Options, SplitCubicOptions::kYExtremas))
      ts._incrementSize(blQuadRoots(ts.end(), Da.y, Db.y, Dc.y, BL_M_AFTER_0, BL_M_BEFORE_1));
  }

  // Split the curve into a spline, if necessary.
  if (!ts.empty()) {
    // If 2 or more flags were specified, sort Ts, otherwise we have them sorted already.
    if (!BLIntOps::isPowerOf2(uint32_t(Options)))
      BLAlgorithm::insertionSort(ts.data(), ts.size());

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

static BL_INLINE void approximateCubicWithTwoQuads(const BLPoint p[4], BLPoint quads[7]) noexcept {
  BLPoint c1 = blLerp(p[0], p[1], 0.75);
  BLPoint c2 = blLerp(p[3], p[2], 0.75);
  BLPoint pm = blLerp(c1, c2);

  if (c1 == p[0])
    c1 = lineVectorIntersection(p[0], cubicStartTangent(p), pm, cubicDerivativeAt(p, 0.5));

  if (c2 == p[3])
    c2 = lineVectorIntersection(p[3], cubicEndTangent(p), pm, cubicDerivativeAt(p, 0.5));

  quads[0] = p[0];
  quads[1] = c1;
  quads[2] = pm;
  quads[3] = c2;
  quads[4] = p[3];
}

template<typename Callback>
static BL_INLINE BLResult approximateCubicWithQuads(const BLPoint p[4], double simplifyTolerance, const Callback& callback) noexcept {
  // Tolerance consists of a prefactor (27/4 * 2^3) combined with `simplifyTolerance`.
  double toleranceSq = blSquare(54.0 * simplifyTolerance);

  // Smallest parameter step to satisfy tolerance condition.
  double t = blPow(toleranceSq / lengthSq(cubicIdentity(p)), 1.0 / 6.0);

  BLPoint cubic[7];
  cubic[3] = p[0];
  cubic[4] = p[1];
  cubic[5] = p[2];
  cubic[6] = p[3];

  for (;;) {
    BLPoint quads[5];
    t = blMin(1.0, t);

    if (t >= 0.999)
      t = 1.0;

    // Split the cubic:
    //   - cubic[0:3] contains the part before `t`.
    //   - cubic[3:7] contains the part after `t`.
    splitCubic(cubic + 3, cubic, cubic + 3, t);
    approximateCubicWithTwoQuads(cubic, quads);

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

} // {BLGeometry}

//! \}
//! \endcond

#endif // BLEND2D_GEOMETRY_P_H_INCLUDED
