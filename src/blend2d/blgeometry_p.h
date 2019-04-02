// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLGEOMETRY_P_H
#define BLEND2D_BLGEOMETRY_P_H

#include "./blgeometry.h"
#include "./blmath_p.h"
#include "./blsupport_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [Math Extensions]
// ============================================================================

static BL_INLINE bool blIsNaN(const BLPoint& p) noexcept { return blIsNaN(p.x + p.y); }
static BL_INLINE bool blIsFinite(const BLPoint& p) noexcept { return blIsFinite(p.x, p.y); }
static BL_INLINE bool blIsFinite(const BLBox& b) noexcept { return blIsFinite(b.x0, b.y0, b.x1, b.y1); }
static BL_INLINE bool blIsFinite(const BLRect& r) noexcept { return blIsFinite(r.x, r.y, r.w, r.h); }

static BL_INLINE BLPoint blCopySign(const BLPoint& a, const BLPoint& b) noexcept {
  return BLPoint(blCopySign(a.x, b.x), blCopySign(a.y, b.y));
}

static BL_INLINE BLPoint blSqrt(const BLPoint& p) noexcept {
  return BLPoint(blSqrt(p.x), blSqrt(p.y));
}

static BL_INLINE size_t blSimplifiedQuadRoots(BLPoint dst[2], const BLPoint& a, const BLPoint& b, const BLPoint& c) noexcept {
  BLPoint d = blMax(b * b - 4.0 * a * c, 0.0);
  BLPoint s = blSqrt(d);
  BLPoint q = -0.5 * (b + blCopySign(s, b));

  dst[0] = q / a;
  dst[1] = c / q;

  return 2;
}

static BL_INLINE bool blIsZero(const BLPoint& p) noexcept {
  return (p.x == 0) & (p.y == 0);
}

// ============================================================================
// [IsValid]
// ============================================================================

static BL_INLINE bool blIsValid(const BLSizeI& size) noexcept { return (size.w > 0) & (size.h > 0); }
static BL_INLINE bool blIsValid(const BLSize& size) noexcept { return (size.w > 0) & (size.h > 0); }

static BL_INLINE bool blIsValid(const BLBoxI& box) noexcept { return (box.x0 < box.x1) & (box.y0 < box.y1); }
static BL_INLINE bool blIsValid(const BLBox& box) noexcept { return (box.x0 < box.x1) & (box.y0 < box.y1); }

static BL_INLINE bool blIsValid(const BLRectI& rect) noexcept {
  BLOverflowFlag of = 0;
  int x1 = blAddOverflow(rect.x, rect.w, &of);
  int y1 = blAddOverflow(rect.y, rect.h, &of);
  return (rect.x < x1) & (rect.y < y1) & (!of);
}

static BL_INLINE bool blIsValid(const BLRect& rect) noexcept {
  double x1 = rect.x + rect.w;
  double y1 = rect.y + rect.h;
  return (rect.x < x1) & (rect.y < y1);
}

// ============================================================================
// [Box/Rect Manipulation]
// ============================================================================

static BL_INLINE bool blIntersectBoxes(BLBoxI& dst, const BLBoxI& a, const BLBoxI& b) noexcept {
  dst.reset(blMax(a.x0, b.x0), blMax(a.y0, b.y0),
            blMin(a.x1, b.x1), blMin(a.y1, b.y1));
  return (dst.x0 < dst.x1) & (dst.y0 < dst.y1);
}

static BL_INLINE bool blIntersectBoxes(BLBox& dst, const BLBox& a, const BLBox& b) noexcept {
  dst.reset(blMax(a.x0, b.x0), blMax(a.y0, b.y0),
            blMin(a.x1, b.x1), blMin(a.y1, b.y1));
  return (dst.x0 < dst.x1) & (dst.y0 < dst.y1);
}

static BL_INLINE void blBoundBoxes(BLBox& box, const BLPoint& p) noexcept {
  box.reset(blMin(box.x0, p.x), blMin(box.y0, p.y),
            blMax(box.x1, p.x), blMax(box.y1, p.y));
}

static BL_INLINE void blBoundBoxes(BLBox& box, const BLBox& other) noexcept {
  box.reset(blMin(box.x0, other.x0), blMin(box.y0, other.y0),
            blMax(box.x1, other.x1), blMax(box.y1, other.y1));
}

static BL_INLINE void blBoundBoxes(BLBoxI& box, const BLBoxI& other) noexcept {
  box.reset(blMin(box.x0, other.x0), blMin(box.y0, other.y0),
            blMax(box.x1, other.x1), blMax(box.y1, other.y1));
}

static BL_INLINE bool blSubsumes(const BLBoxI& a, const BLBoxI& b) noexcept { return (a.x0 <= b.x0) & (a.y0 <= b.y0) & (a.x1 >= b.x1) & (a.y1 >= b.y1); }
static BL_INLINE bool blSubsumes(const BLBox& a, const BLBox& b) noexcept { return (a.x0 <= b.x0) & (a.y0 <= b.y0) & (a.x1 >= b.x1) & (a.y1 >= b.y1); }

static BL_INLINE bool blOverlaps(const BLBoxI& a, const BLBoxI& b) noexcept { return (a.x1 > b.x0) & (a.y1 > b.y0) & (a.x0 < b.x1) & (a.y0 < b.y1); }
static BL_INLINE bool blOverlaps(const BLBox& a, const BLBox& b) noexcept { return (a.x1 > b.x0) & (a.y1 > b.y0) & (a.x0 < b.x1) & (a.y0 < b.y1); }

// ============================================================================
// [Point / Vector]
// ============================================================================

static BL_INLINE double blLengthSq(const BLPoint& v) noexcept { return v.x * v.x + v.y * v.y; }
static BL_INLINE double blLengthSq(const BLPoint& a, const BLPoint& b) noexcept { return blLengthSq(b - a); }

static BL_INLINE double blLength(const BLPoint& v) noexcept { return blSqrt(blLengthSq(v)); }
static BL_INLINE double blLength(const BLPoint& a, const BLPoint& b) noexcept { return blSqrt(blLengthSq(a, b)); }

static BL_INLINE BLPoint blNormal(const BLPoint& v) noexcept { return BLPoint(-v.y, v.x); }
static BL_INLINE BLPoint blUnitVector(const BLPoint& v) noexcept { return v / blLength(v); }

static BL_INLINE double blDotProduct(const BLPoint& a, const BLPoint& b) noexcept { return a.x * b.x + a.y * b.y; }
static BL_INLINE double blCrossProduct(const BLPoint& a, const BLPoint& b) noexcept { return a.x * b.y - a.y * b.x; }

// ============================================================================
// [Line]
// ============================================================================

static BL_INLINE BLPoint blGetLineVectorIntersection(const BLPoint& p0, const BLPoint& v0, const BLPoint& p1, const BLPoint& v1) noexcept {
  return p0 + blCrossProduct(p1 - p0, v1) / blCrossProduct(v0, v1) * v0;
}

// ============================================================================
// [Quad]
// ============================================================================

// Quad Coefficients
// -----------------
//
// A =    p0 + 2*p1 + p2
// B = -2*p0 + 2*p1
// C =    p0
//
// Quad Evaluation at `t`
// ----------------------
//
// V = At^2 + Bt + C => t(At + B) + C

static BL_INLINE void blSplitQuad(const BLPoint p[3], BLPoint aOut[3], BLPoint bOut[3]) noexcept {
  BLPoint p01(blLerp(p[0], p[1]));
  BLPoint p12(blLerp(p[1], p[2]));

  aOut[0] = p[0];
  aOut[1] = p01;
  bOut[1] = p12;
  bOut[2] = p[2];
  aOut[2] = blLerp(p01, p12);
  bOut[0] = aOut[2];
}

static BL_INLINE void blSplitQuad(const BLPoint p[3], BLPoint aOut[3], BLPoint bOut[3], double t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));

  aOut[0] = p[0];
  aOut[1] = p01;
  bOut[1] = p12;
  bOut[2] = p[2];
  aOut[2] = blLerp(p01, p12, t);
  bOut[0] = aOut[2];
}

static BL_INLINE void blSplitQuadBefore(const BLPoint p[3], BLPoint out[3], double t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));

  out[0] = p[0];
  out[1] = p01;
  out[2] = blLerp(p01, p12, t);
}

static BL_INLINE void blSplitQuadAfter(const BLPoint p[3], BLPoint out[3], double t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));

  out[0] = blLerp(p01, p12, t);
  out[1] = p12;
  out[2] = p[2];
}

static BL_INLINE void blSplitQuadBetween(const BLPoint p[3], BLPoint out[3], double t0, double t1) noexcept {
  BLPoint t0p01 = blLerp(p[0], p[1], t0);
  BLPoint t0p12 = blLerp(p[1], p[2], t0);

  BLPoint t1p01 = blLerp(p[0], p[1], t1);
  BLPoint t1p12 = blLerp(p[1], p[2], t1);

  out[0] = blLerp(t0p01, t0p12, t0);
  out[1] = blLerp(t0p01, t0p12, t1);
  out[2] = blLerp(t1p01, t1p12, t1);
}

static BL_INLINE void blGetQuadCoefficients(const BLPoint p[3], BLPoint& a, BLPoint& b, BLPoint& c) noexcept {
  BLPoint v1 = p[1] - p[0];
  BLPoint v2 = p[2] - p[1];

  a = v2 - v1;
  b = v1 + v1;
  c = p[0];
}

static BL_INLINE void blGetQuadDerivativeCoefficients(const BLPoint p[3], BLPoint& a, BLPoint& b) noexcept {
  BLPoint v1 = p[1] - p[0];
  BLPoint v2 = p[2] - p[1];

  a = 2.0 * v2 - 2.0 * v1;
  b = 2.0 * v1;
}

static BL_INLINE BLPoint blGetQuadValueAt(const BLPoint p[3], double t) noexcept {
  BLPoint a, b, c;
  blGetQuadCoefficients(p, a, b, c);
  return (a * t + b) * t + c;
}

static BL_INLINE BLPoint blGetQuadValueAt(const BLPoint p[3], const BLPoint& t) noexcept {
  BLPoint a, b, c;
  blGetQuadCoefficients(p, a, b, c);
  return (a * t + b) * t + c;
}

static BL_INLINE BLPoint blGetPreciseQuadValueAt(const BLPoint p[3], double t) noexcept {
  return blLerp(blLerp(p[0], p[1], t), blLerp(p[1], p[2], t), t);
}

static BL_INLINE BLPoint blGetPreciseQuadValueAt(const BLPoint p[3], const BLPoint& t) noexcept {
  return blLerp(blLerp(p[0], p[1], t), blLerp(p[1], p[2], t), t);
}

static BL_INLINE void blGetQuadExtremaPoint(const BLPoint p[3], BLPoint& out) noexcept {
  BLPoint t = blClamp((p[0] - p[1]) / (p[0] - p[1] * 2.0 + p[2]), 0.0, 1.0);
  out = blGetPreciseQuadValueAt(p, t);
}

static BL_INLINE double blGetQuadParameterAtAngle(const BLPoint p[3], double m) noexcept {
  BLPoint qa, qb;
  blGetQuadDerivativeCoefficients(p, qa, qb);

  double aob = blDotProduct(qa, qb);
  double axb = blCrossProduct(qa, qb);

  if (aob == 0.0)
    return 1.0;

  // m * (bx * bx + by * by) / (|ax * by - ay * bx| - m * (ax * bx + ay * by));
  return m * blLengthSq(qb) / (blAbs(axb) - m * aob);
}

static BL_INLINE double blGetQuadCurvatureMetric(const BLPoint p[3]) noexcept {
  return blCrossProduct(p[2] - p[1], p[1] - p[0]);
}

static BL_INLINE size_t blGetQuadOffsetCuspTs(const BLPoint bez[3], double d, double tOut[2]) {
  BLPoint qqa, qqb;
  blGetQuadDerivativeCoefficients(bez, qqa, qqb);

  double bxa = blCrossProduct(qqb, qqa);
  double boa = blDotProduct(qqb, qqa);

  if (bxa == 0)
    return 0;

  double alen2 = blLengthSq(qqa);
  double blen2 = blLengthSq(qqb);

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

//! Coverts quadratic curve to cubic curve.
//!
//! \code
//! cubic[0] = q0
//! cubic[1] = q0 + 2/3 * (q1 - q0)
//! cubic[2] = q2 + 2/3 * (q1 - q2)
//! cubic[3] = q2
//! \endcode
static BL_INLINE void blQuadToCubic(const BLPoint p[3], BLPoint cubicOut[4]) noexcept {
  constexpr double k1Div3 = 1.0 / 3.0;
  constexpr double k2Div3 = 2.0 / 3.0;

  BLPoint tmp = p[1] * k2Div3;
  cubicOut[0] = p[0];
  cubicOut[3] = p[2];
  cubicOut[1].reset(cubicOut[0] * k1Div3 + tmp);
  cubicOut[2].reset(cubicOut[2] * k1Div3 + tmp);
}

class BLQuadCurveTsIter {
public:
  const double* ts;
  const double* tsEnd;

  BLPoint input[3];
  BLPoint part[3];
  BLPoint pTmp01;
  BLPoint pTmp12;

  BL_INLINE BLQuadCurveTsIter() noexcept
    : ts(nullptr),
      tsEnd(nullptr) {}

  BL_INLINE BLQuadCurveTsIter(const BLPoint* input_, const double* ts_, size_t count_) noexcept {
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
    // This behaves identically to `blSplitQuadBefore()`, however, we cache
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

// ============================================================================
// [Cubic]
// ============================================================================

// Cubic Coefficients
// ------------------
//
// A =   -p0 + 3*p1 - 3*p2 + p3 => 3*(p1 - p2) + p3 - p0
// B =  3*p0 - 6*p1 + 3*p2      => 3*(p0 - 2*p2 + p3)
// C = -3*p0 + 3*p1             => 3*(p1 - p0)
// D =    p0                    => p0
//
// Cubic Evaluation at `t`
// -----------------------
//
// V = At^3 + Bt^2 + Ct + D     => t(t(At + B) + C) + D

static BL_INLINE void blSplitCubic(const BLPoint p[4], BLPoint a[4], BLPoint b[4]) noexcept {
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

static BL_INLINE void blSplitCubic(const BLPoint p[4], BLPoint before[4], BLPoint after[4], double t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));
  BLPoint p23(blLerp(p[2], p[3], t));

  before[0] = p[0];
  before[1] = p01;
  after[2] = p23;
  after[3] = p[3];

  before[2] = blLerp(p01, p12, t);
  after[1] = blLerp(p12, p23, t);
  before[3] = blLerp(before[2], after[1], t);
  after[0] = before[3];
}

static BL_INLINE void blSplitCubicBefore(const BLPoint p[4], BLPoint a[4], double t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));
  BLPoint p23(blLerp(p[2], p[3], t));

  a[0] = p[0];
  a[1] = p01;
  a[2] = blLerp(p01, p12, t);
  a[3] = blLerp(a[2], blLerp(p12, p23, t), t);
}

static BL_INLINE void blSplitCubicAfter(const BLPoint p[4], BLPoint b[4], double t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));
  BLPoint p23(blLerp(p[2], p[3], t));

  b[3] = p[3];
  b[2] = p23;
  b[1] = blLerp(p12, p23, t);
  b[0] = blLerp(blLerp(p01, p12, t), b[1], t);
}

static BL_INLINE void blGetCubicCoefficients(const BLPoint p[4], BLPoint& a, BLPoint& b, BLPoint& c, BLPoint& d) noexcept {
  BLPoint v1 = p[1] - p[0];
  BLPoint v2 = p[2] - p[1];
  BLPoint v3 = p[3] - p[2];

  a = v3 - v2 - v2 + v1;
  b = 3.0 * (v2 - v1);
  c = 3.0 * v1;
  d = p[0];
}

static BL_INLINE void blGetCubicDerivativeCoefficients(const BLPoint p[4], BLPoint& a, BLPoint& b, BLPoint& c) noexcept {
  BLPoint v1 = p[1] - p[0];
  BLPoint v2 = p[2] - p[1];
  BLPoint v3 = p[3] - p[2];

  a = 3.0 * (v3 - v2 - v2 + v1);
  b = 6.0 * (v2 - v1);
  c = 3.0 * v1;
}

static BL_INLINE BLPoint blGetCubicValueAt(const BLPoint p[4], double t) noexcept {
  BLPoint a, b, c, d;
  blGetCubicCoefficients(p, a, b, c, d);
  return ((a * t + b) * t + c) * t + d;
}

static BL_INLINE BLPoint blGetCubicValueAt(const BLPoint p[4], const BLPoint& t) noexcept {
  BLPoint a, b, c, d;
  blGetCubicCoefficients(p, a, b, c, d);
  return ((a * t + b) * t + c) * t + d;
}

static BL_INLINE BLPoint blGetPreciseCubicValueAt(const BLPoint p[4], double t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));
  BLPoint p23(blLerp(p[2], p[3], t));

  return blLerp(blLerp(p01, p12, t), blLerp(p12, p23, t), t);
}

static BL_INLINE BLPoint blGetPreciseCubicValueAt(const BLPoint p[4], const BLPoint& t) noexcept {
  BLPoint p01(blLerp(p[0], p[1], t));
  BLPoint p12(blLerp(p[1], p[2], t));
  BLPoint p23(blLerp(p[2], p[3], t));
  return blLerp(blLerp(p01, p12, t), blLerp(p12, p23, t), t);
}

static BL_INLINE BLPoint blGetCubicDerivativeAt(const BLPoint p[4], double t) noexcept {
  BLPoint p01 = blLerp(p[0], p[1], t);
  BLPoint p12 = blLerp(p[1], p[2], t);
  BLPoint p23 = blLerp(p[2], p[3], t);

  return 3.0 * (blLerp(p12, p23, t) - blLerp(p01, p12, t));
}

static BL_INLINE void blGetCubicExtremaPoints(const BLPoint p[4], BLPoint out[2]) noexcept {
  BLPoint a, b, c;
  blGetCubicDerivativeCoefficients(p, a, b, c);

  BLPoint t[2];
  blSimplifiedQuadRoots(t, a, b, c);

  t[0] = blClamp(t[0], 0.0, 1.0);
  t[1] = blClamp(t[1], 0.0, 1.0);

  out[0] = blGetPreciseCubicValueAt(p, t[0]);
  out[1] = blGetPreciseCubicValueAt(p, t[1]);
}

static BL_INLINE BLPoint blCubicMidPoint(const BLPoint p[4]) noexcept {
  return (p[0] + p[3]) * 0.125 + (p[1] + p[2]) * 0.375;
}

static BL_INLINE BLPoint blGetCubicIdentity(const BLPoint p[4]) noexcept {
  BLPoint v1 = p[1] - p[0];
  BLPoint v2 = p[2] - p[1];
  BLPoint v3 = p[3] - p[2];

  return v3 - v2 - v2 + v1;
}

static BL_INLINE bool blIsCubicFlat(const BLPoint p[3], double f) {
  if (p[3] == p[0]) {
    BLPoint v = p[2] - p[1];
    double a = blCrossProduct(v, p[1] - p[0]);
    return 0.5625 * a * a <= f * f * blLengthSq(v);
  }
  else {
    BLPoint v = p[3] - p[0];
    double a1 = blCrossProduct(v, p[1] - p[0]);
    double a2 = blCrossProduct(v, p[2] - p[0]);
    return 0.5625 * blMax(a1 * a1, a2 * a2) <= f * f * blLengthSq(v);
  }
}

static BL_INLINE void blGetCubicCuspInflectionParameter(const BLPoint p[4], double& tc, double& tl) noexcept {
  BLPoint a, b, c, d;
  blGetCubicCoefficients(p, a, b, c, d);

  tc = -0.5 * blCrossProduct(c, a) / blCrossProduct(b, a);
  tl = tc * tc - blCrossProduct(c, b) / (3.0 * blCrossProduct(b, a));

  // TODO: We don't need the SQRT in stroker check.
  // If true inflections points might exist.
  if (tl > 0)
    tl = blSqrt(tl);
}

static BL_INLINE BLPoint blGetCubicStartTangent(const BLPoint p[4]) noexcept {
  BLPoint out = p[1] - p[0];
  BLPoint t20 = p[2] - p[0];
  BLPoint t30 = p[3] - p[0];

  if (blIsZero(out)) out = t20;
  if (blIsZero(out)) out = t30;

  return out;
}

static BL_INLINE BLPoint blGetCubicEndTangent(const BLPoint p[4]) noexcept {
  BLPoint out = p[3] - p[2];
  BLPoint t31 = p[3] - p[1];
  BLPoint t30 = p[3] - p[0];

  if (blIsZero(out)) out = t31;
  if (blIsZero(out)) out = t30;

  return out;
}

static BL_INLINE void blApproximateCubicWithTwoQuads(const BLPoint p[4], BLPoint quads[7]) noexcept {
  BLPoint c1 = blLerp(p[0], p[1], 0.75);
  BLPoint c2 = blLerp(p[3], p[2], 0.75);
  BLPoint pm = blLerp(c1, c2);

  if (c1 == p[0])
    c1 = blGetLineVectorIntersection(p[0], blGetCubicStartTangent(p), pm, blGetCubicDerivativeAt(p, 0.5));

  if (c2 == p[3])
    c2 = blGetLineVectorIntersection(p[3], blGetCubicEndTangent(p), pm, blGetCubicDerivativeAt(p, 0.5));

  quads[0] = p[0];
  quads[1] = c1;
  quads[2] = pm;
  quads[3] = c2;
  quads[4] = p[3];
}

template<typename Callback>
static BL_INLINE BLResult blApproximateCubicWithQuads(const BLPoint p[4], double simplifyTolerance, const Callback& callback) noexcept {
  // Tolerance consists of a prefactor (27/4 * 2^3) combined with `simplifyTolerance`.
  double toleranceSq = blSquare(54.0 * simplifyTolerance);

  // Smallest parameter step to satisfy tolerance condition.
  double t = blPow(toleranceSq / blLengthSq(blGetCubicIdentity(p)), 1.0 / 6.0);

  BLPoint cubic[7];
  cubic[3] = p[0];
  cubic[4] = p[1];
  cubic[5] = p[2];
  cubic[6] = p[3];

  for (;;) {
    BLPoint quads[5];
    t = blMin(t, 1.0);

    // Split the cubic:
    //   - cubic[0:3] contains the part before `t`.
    //   - cubic[3:7] contains the part after `t`.
    blSplitCubic(cubic + 3, cubic, cubic + 3, t);
    blApproximateCubicWithTwoQuads(cubic, quads);

    for (size_t i = 0; i <= 2; i += 2)
      BL_PROPAGATE(callback(quads + i));

    if (t >= 1.0)
      return BL_SUCCESS;

    // Recalculate the parameter.
    t = t / (1.0 - t);
  }
}

//! \}
//! \endcond

#endif // BLEND2D_BLGEOMETRY_P_H
