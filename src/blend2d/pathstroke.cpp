// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "geometry_p.h"
#include "matrix_p.h"
#include "path_p.h"
#include "pathstroke_p.h"
#include "support/lookuptable_p.h"
#include "support/math_p.h"

namespace bl {
namespace PathInternal {

// bl::Path - Stroke - Constants
// =============================

// Default minimum miter-join length that always bypasses any other join-type. The reason behind this is to
// prevent emitting very small line segments in case that normals of joining segments are almost equal.
static constexpr double kStrokeMiterMinimum = 1e-10;
static constexpr double kStrokeMiterMinimumSq = Math::square(kStrokeMiterMinimum);

// Minimum length for a line/curve the stroker will accept. If the segment is smaller than this it would be skipped.
static constexpr double kStrokeLengthEpsilon = 1e-10;
static constexpr double kStrokeLengthEpsilonSq = Math::square(kStrokeLengthEpsilon);

static constexpr double kStrokeCollinearityEpsilon = 1e-10;

static constexpr double kStrokeCuspTThreshold = 1e-10;
static constexpr double kStrokeDegenerateFlatness = 1e-6;

// Epsilon used to split quadratic bezier curves during offsetting.
static constexpr double kOffsetQuadEpsilonT = 1e-5;

// Minimum vertices that would be required for any join + additional line.
//
// Calculated from:
//   JOIN:
//     bevel: 1 vertex
//     miter: 3 vertices
//     round: 7 vertices (2 cubics at most)
//   ADDITIONAL:
//     end-point: 1 vertex
//     line-to  : 1 vertex
static constexpr size_t kStrokeMaxJoinVertices = 9;

// bl::Path - Stroke - Tables
// ==========================

struct CapVertexCountGen {
  static constexpr uint8_t value(size_t cap) noexcept {
    return cap == BL_STROKE_CAP_SQUARE       ? 3 :
           cap == BL_STROKE_CAP_ROUND        ? 6 :
           cap == BL_STROKE_CAP_ROUND_REV    ? 8 :
           cap == BL_STROKE_CAP_TRIANGLE     ? 2 :
           cap == BL_STROKE_CAP_TRIANGLE_REV ? 4 :
           cap == BL_STROKE_CAP_BUTT         ? 1 : 1; // Default if not known.
  }
};

static constexpr auto capVertexCountTable =
  makeLookupTable<uint8_t, BL_STROKE_CAP_MAX_VALUE + 1, CapVertexCountGen>();

// bl::Path - Stroke - Utilities
// =============================

enum class Side : size_t {
  kA = 0,
  kB = 1
};

static BL_INLINE Side oppositeSide(Side side) noexcept { return Side(size_t(1) - size_t(side)); }

static BL_INLINE Side sideFromNormals(const BLPoint& n0, const BLPoint& n1) noexcept {
  return Side(size_t(Geometry::cross(n0, n1) >= 0));
}

static BL_INLINE uint32_t sanityStrokeCap(uint32_t cap) noexcept {
  return cap <= BL_STROKE_CAP_MAX_VALUE ? cap : BL_STROKE_CAP_BUTT;
}

static BL_INLINE bool isMiterJoinCategory(uint32_t joinType) noexcept {
  return joinType == BL_STROKE_JOIN_MITER_CLIP  ||
         joinType == BL_STROKE_JOIN_MITER_BEVEL ||
         joinType == BL_STROKE_JOIN_MITER_ROUND ;
}

static BL_INLINE uint32_t miterJoinToSimpleJoin(uint32_t joinType) {
  if (joinType == BL_STROKE_JOIN_MITER_BEVEL)
    return BL_STROKE_JOIN_BEVEL;
  else if (joinType == BL_STROKE_JOIN_MITER_ROUND)
    return BL_STROKE_JOIN_ROUND;
  else
    return joinType;
}

static BL_INLINE bool testInnerJoinIntersecion(const BLPoint& a0, const BLPoint& a1, const BLPoint& b0, const BLPoint& b1, const BLPoint& join) noexcept {
  BLPoint min = blMax(blMin(a0, a1), blMin(b0, b1));
  BLPoint max = blMin(blMax(a0, a1), blMax(b0, b1));

  return (join.x >= min.x) & (join.y >= min.y) &
         (join.x <= max.x) & (join.y <= max.y) ;
}

static BL_INLINE void dullAngleArcTo(PathAppender& appender, const BLPoint& p0, const BLPoint& pa, const BLPoint& pb, const BLPoint& intersection) noexcept {
  BLPoint pm = (pa + pb) * 0.5;

  double w = Math::sqrt(Geometry::length(p0 - pm) / Geometry::length(p0 - intersection));
  double a = 4 * w / (3.0 * (1.0 + w));

  BLPoint c0 = pa + a * (intersection - pa);
  BLPoint c1 = pb + a * (intersection - pb);

  appender.cubicTo(c0, c1, pb);
};

// bl::Path - Stroke - Implementation
// ==================================

class PathStroker {
public:
  enum Flags : uint32_t {
    kFlagIsOpen   = 0x01,
    kFlagIsClosed = 0x02
  };

  // Stroke input.
  PathIterator _iter;

  // Stroke options.
  const BLStrokeOptions& _options;
  const BLApproximationOptions& _approx;

  struct SideData {
    BLPath* path;            // Output path (outer/inner, per side).
    size_t figureOffset;     // Start of the figure offset in output path (only A path).
    PathAppender appender; // Output path appended (outer/inner, per side).
    double d;                // Distance (StrokeWidth / 2).
    double d2;               // Distance multiplied by 2.
  };

  double _miterLimit;        // Miter limit possibly clamped to a safe range.
  double _miterLimitSq;      // Miter limit squared.
  uint32_t _joinType;        // Simplified join type.
  SideData _sideData[2];     // A and B data (outer/inner side).

  // Stroke output.
  BLPath* _cPath;            // Output C path.

  // Global state.
  BLPoint _p0;               // Current point.
  BLPoint _n0;               // Unit normal of `_p0`.
  BLPoint _pInitial;         // Initial point (MoveTo).
  BLPoint _nInitial;         // Unit normal of `_pInitial`.
  uint32_t _flags;           // Work flags.

  BL_INLINE PathStroker(const BLPathView& input, const BLStrokeOptions& options, const BLApproximationOptions& approx, BLPath* a, BLPath* b, BLPath* c) noexcept
    : _iter(input),
      _options(options),
      _approx(approx),
      _joinType(options.join),
      _cPath(c) {

    _sideData[0].path = a;
    _sideData[0].figureOffset = 0;
    _sideData[0].d = options.width * 0.5;
    _sideData[0].d2 = options.width;
    _sideData[1].path = b;
    _sideData[1].figureOffset = 0;
    _sideData[1].d = -_sideData[0].d;
    _sideData[1].d2 = -_sideData[0].d2;

    // Initialize miter calculation options. What we do here is to change `_joinType` to a value that would be easier
    // for us to use during joining. We always honor `_miterLimitSq` even when the `_joinType` is not miter to prevent
    // emitting very small line segments next to next other, which saves vertices and also prevents border cases in
    // additional processing.
    if (isMiterJoinCategory(_joinType)) {
      // Simplify miter-join type to non-miter join, if possible.
      _joinType = miterJoinToSimpleJoin(_joinType);

      // Final miter limit is `0.5 * width * miterLimit`.
      _miterLimit = d() * options.miterLimit;
      _miterLimitSq = Math::square(_miterLimit);
    }
    else {
      _miterLimit = kStrokeMiterMinimum;
      _miterLimitSq = kStrokeMiterMinimumSq;
    }
  }

  BL_INLINE bool isOpen() const noexcept { return (_flags & kFlagIsOpen) != 0; }
  BL_INLINE bool isClosed() const noexcept { return (_flags & kFlagIsClosed) != 0; }

  BL_INLINE SideData& sideData(Side side) noexcept { return _sideData[size_t(side)]; }

  BL_INLINE double d() const noexcept { return _sideData[0].d; }
  BL_INLINE double d(Side side) const noexcept { return _sideData[size_t(side)].d; }

  BL_INLINE double d2() const noexcept { return _sideData[0].d2; }
  BL_INLINE double d2(Side side) const noexcept { return _sideData[size_t(side)].d2; }

  BL_INLINE BLPath* aPath() const noexcept { return _sideData[size_t(Side::kA)].path; }
  BL_INLINE BLPath* bPath() const noexcept { return _sideData[size_t(Side::kB)].path; }
  BL_INLINE BLPath* cPath() const noexcept { return _cPath; }

  BL_INLINE BLPath* outerPath(Side side) const noexcept { return _sideData[size_t(side)].path; }
  BL_INLINE BLPath* innerPath(Side side) const noexcept { return _sideData[size_t(oppositeSide(side))].path; }

  BL_INLINE PathAppender& aOut() noexcept { return _sideData[size_t(Side::kA)].appender; }
  BL_INLINE PathAppender& bOut() noexcept { return _sideData[size_t(Side::kB)].appender; }

  BL_INLINE PathAppender& outerAppender(Side side) noexcept { return _sideData[size_t(side)].appender; }
  BL_INLINE PathAppender& innerAppender(Side side) noexcept { return _sideData[size_t(oppositeSide(side))].appender; }

  BL_INLINE BLResult ensureAppendersCapacity(size_t aRequired, size_t bRequired) noexcept {
    uint32_t ok = uint32_t(aOut().remainingSize() >= aRequired) &
                  uint32_t(bOut().remainingSize() >= bRequired) ;

    if (BL_LIKELY(ok))
      return BL_SUCCESS;

    return aOut().ensure(aPath(), aRequired) |
           bOut().ensure(bPath(), bRequired) ;
  }

  BL_INLINE_IF_NOT_DEBUG BLResult stroke(BLPathStrokeSinkFunc sink, void* userData) noexcept {
    size_t figureStartIdx = 0;
    size_t estimatedSize = _iter.remainingForward() * 2u;

    BL_PROPAGATE(aPath()->reserve(aPath()->size() + estimatedSize));

    while (!_iter.atEnd()) {
      // Start of the figure.
      const uint8_t* figureStartCmd = _iter.cmd;
      if (BL_UNLIKELY(_iter.cmd[0] != BL_PATH_CMD_MOVE)) {
        if (_iter.cmd[0] != BL_PATH_CMD_CLOSE)
          return blTraceError(BL_ERROR_INVALID_GEOMETRY);

        _iter++;
        continue;
      }

      figureStartIdx += (size_t)(_iter.cmd - figureStartCmd);
      figureStartCmd = _iter.cmd;

      sideData(Side::kA).figureOffset = sideData(Side::kA).path->size();
      BL_PROPAGATE(aOut().begin(aPath(), BL_MODIFY_OP_APPEND_GROW, _iter.remainingForward()));
      BL_PROPAGATE(bOut().begin(bPath(), BL_MODIFY_OP_ASSIGN_GROW, 48));

      BLPoint polyPts[4];
      size_t polySize;

      _p0 = *_iter.vtx;
      _pInitial = _p0;
      _flags = 0;

      // Content of the figure.
      _iter++;
      while (!_iter.atEnd()) {
        BL_PROPAGATE(ensureAppendersCapacity(kStrokeMaxJoinVertices, kStrokeMaxJoinVertices));

        uint8_t cmd = _iter.cmd[0]; // Next command.
        BLPoint p1 = _iter.vtx[0];  // Next line-to or control point.
        BLPoint v1;                 // Vector of `p1 - _p0`.
        BLPoint n1;                 // Unit normal of `v1`.

        if (cmd == BL_PATH_CMD_ON) {
          // Line command, collinear curve converted to line or close of the figure.
          _iter++;
LineTo:
          v1 = p1 - _p0;
          if (Geometry::lengthSq(v1) < kStrokeLengthEpsilonSq)
            continue;

          n1 = Geometry::normal(Geometry::unitVector(v1));
          if (!isOpen()) {
            BL_PROPAGATE(openLineTo(p1, n1));
            continue;
          }

          for (;;) {
            BL_PROPAGATE(joinLineTo(p1, n1));

            if (_iter.atEnd())
              break;

            BL_PROPAGATE(ensureAppendersCapacity(kStrokeMaxJoinVertices, kStrokeMaxJoinVertices));

            cmd = _iter.cmd[0];
            p1 = _iter.vtx[0];

            if (cmd != BL_PATH_CMD_ON)
              break;

            _iter++;
            v1 = p1 - _p0;
            if (Geometry::lengthSq(v1) < kStrokeLengthEpsilonSq)
              break;

            n1 = Geometry::normal(Geometry::unitVector(v1));
          }
          continue;

          // This is again to minimize inline expansion of `smoothPolyTo()`.
SmoothPolyTo:
          BL_PROPAGATE(smoothPolyTo(polyPts, polySize));
          continue;
        }
        else if (cmd == BL_PATH_CMD_QUAD) {
          // Quadratic curve segment.
          _iter += 2;
          if (BL_UNLIKELY(_iter.afterEnd()))
            return BL_ERROR_INVALID_GEOMETRY;

          const BLPoint* quad = _iter.vtx - 3;
          BLPoint p2 = quad[2];
          BLPoint v2 = p2 - p1;

          v1 = p1 - _p0;
          n1 = Geometry::normal(Geometry::unitVector(v1));

          double cm = Geometry::cross(v2, v1);
          if (blAbs(cm) <= kStrokeCollinearityEpsilon) {
            // All points are [almost] collinear (degenerate case).
            double dot = Geometry::dot(-v1, v2);

            // Check if control point lies outside of the start/end points.
            if (dot > 0.0) {
              // Rotate all points to x-axis.
              double r1 = Geometry::dot(p1 - _p0, v1);
              double r2 = Geometry::dot(p2 - _p0, v1);

              // Parameter of the cusp if it's within (0, 1).
              double t = r1 / (2.0 * r1 - r2);
              if (t > 0.0 && t < 1.0) {
                polyPts[0] = Geometry::evalQuad(quad, t);
                polyPts[1] = p2;
                polySize = 2;
                goto SmoothPolyTo;
              }
            }

            // Collinear without cusp => straight line.
            p1 = p2;
            goto LineTo;
          }

          // Very small curve segment => straight line.
          if (Geometry::lengthSq(v1) < kStrokeLengthEpsilonSq || Geometry::lengthSq(v2) < kStrokeLengthEpsilonSq) {
            p1 = p2;
            goto LineTo;
          }

          if (!isOpen())
            BL_PROPAGATE(openCurve(n1));
          else
            BL_PROPAGATE(joinCurve(n1));

          BL_PROPAGATE(offsetQuad(_iter.vtx - 3));
        }
        else if (cmd == BL_PATH_CMD_CUBIC) {
          // Cubic curve segment.
          _iter += 3;
          if (BL_UNLIKELY(_iter.afterEnd()))
            return BL_ERROR_INVALID_GEOMETRY;

          BLPoint p[7];

          int cusp = 0;
          double tCusp = 0;

          p[0] = _p0;
          p[1] = _iter.vtx[-3];
          p[2] = _iter.vtx[-2];
          p[3] = _iter.vtx[-1];

          // Check if the curve is flat enough to be potentially degenerate.
          if (Geometry::isCubicFlat(p, kStrokeDegenerateFlatness)) {
            double dot1 = Geometry::dot(p[0] - p[1], p[3] - p[1]);
            double dot2 = Geometry::dot(p[0] - p[2], p[3] - p[2]);

            if (!(dot1 < 0.0) || !(dot2 < 0.0)) {
              // Rotate all points to x-axis.
              const BLPoint r = Geometry::cubicStartTangent(p);

              double r1 = Geometry::dot(p[1] - p[0], r);
              double r2 = Geometry::dot(p[2] - p[0], r);
              double r3 = Geometry::dot(p[3] - p[0], r);

              double a = 1.0 / (3.0 * r1 - 3.0 * r2 + r3);
              double b = 2.0 * r1 - r2;
              double s = Math::sqrt(r2 * (r2 - r1) - r1 * (r3 - r1));

              // Parameters of the cusps.
              double t1 = a * (b - s);
              double t2 = a * (b + s);

              // Offset the first and second cusps (if they exist).
              polySize = 0;
              if (t1 > kStrokeCuspTThreshold && t1 < 1.0 - kStrokeCuspTThreshold)
                polyPts[polySize++] = Geometry::evalCubic(p, t1);

              if (t2 > kStrokeCuspTThreshold && t2 < 1.0 - kStrokeCuspTThreshold)
                polyPts[polySize++] = Geometry::evalCubic(p, t2);

              if (polySize == 0) {
                p1 = p[3];
                goto LineTo;
              }

              polyPts[polySize++] = p[3];
              goto SmoothPolyTo;
            }
            else {
              p1 = p[3];
              goto LineTo;
            }
          }
          else {
            double tl;
            Geometry::getCubicInflectionParameter(p, tCusp, tl);

            if (tl == 0.0 && tCusp > 0.0 && tCusp < 1.0) {
              Geometry::splitCubic(p, p, p + 3);
              cusp = 1;
            }
          }

          for (;;) {
            v1 = p[1] - _p0;
            if (Geometry::isZero(v1))
              v1 = p[2] - _p0;
            n1 = Geometry::normal(Geometry::unitVector(v1));

            if (!isOpen())
              BL_PROPAGATE(openCurve(n1));
            else if (cusp >= 0)
              BL_PROPAGATE(joinCurve(n1));
            else
              BL_PROPAGATE(joinCusp(n1));

            BL_PROPAGATE(offsetCubic(p));
            if (cusp <= 0)
              break;

            BL_PROPAGATE(ensureAppendersCapacity(kStrokeMaxJoinVertices, kStrokeMaxJoinVertices));

            // Second part of the cubic after the cusp. We assign `-1` to `cusp` so we can call `joinCusp()` later.
            // This is a special join that we need in this case.
            cusp = -1;
            p[0] = p[3];
            p[1] = p[4];
            p[2] = p[5];
            p[3] = p[6];
          }
        }
        else {
          // Either invalid command or close of the figure. If the figure is already closed it means that we have
          // already jumped to `LineTo` and we should terminate now. Otherwise we just encountered close or
          // something else which is not part of the current figure.
          if (isClosed())
            break;

          if (cmd != BL_PATH_CMD_CLOSE)
            break;

          // The figure is closed. We just jump to `LineTo` to minimize inlining expansion and mark the figure as
          // closed. Next time we terminate on `isClosed()` condition above.
          _flags |= kFlagIsClosed;
          p1 = _pInitial;
          goto LineTo;
        }
      }

      // Don't emit anything if the figure has no points (and thus no direction).
      _iter += size_t(isClosed());
      if (!isOpen()) {
        aOut().done(aPath());
        bOut().done(bPath());
        continue;
      }

      if (isClosed()) {
        // The figure is closed => the end result is two closed figures without caps. In this case only paths
        // A and B have a content, path C will be empty and should be thus ignored by the sink.

        // Allocate space for the end join and close command.
        BL_PROPAGATE(ensureAppendersCapacity(kStrokeMaxJoinVertices + 1, kStrokeMaxJoinVertices + 1));

        BL_PROPAGATE(joinEndPoint(_nInitial));
        aOut().close();
        bOut().close();
        cPath()->clear();
      }
      else {
        // The figure is open => the end result is a single figure with caps. The paths contain the following:
        //   A - Offset of the figure and end cap.
        //   B - Offset of the figure that MUST BE reversed.
        //   C - Start cap (not reversed).
        uint32_t startCap = sanityStrokeCap(_options.startCap);
        uint32_t endCap = sanityStrokeCap(_options.endCap);

        BL_PROPAGATE(aOut().ensure(aPath(), capVertexCountTable[endCap]));
        BL_PROPAGATE(addCap(aOut(), _p0, bOut().vtx[-1], endCap));

        PathAppender cOut;
        BL_PROPAGATE(cOut.begin(cPath(), BL_MODIFY_OP_ASSIGN_GROW, capVertexCountTable[startCap] + 1));
        cOut.moveTo(bPath()->vertexData()[0]);
        BL_PROPAGATE(addCap(cOut, _pInitial, aPath()->vertexData()[sideData(Side::kA).figureOffset], startCap));
        cOut.done(cPath());
      }

      aOut().done(aPath());
      bOut().done(bPath());

      // Call the path to the provided sink with resulting paths.
      size_t figureEndIdx = figureStartIdx + (size_t)(_iter.cmd - figureStartCmd);
      BL_PROPAGATE(sink(aPath(), bPath(), cPath(), figureStartIdx, figureEndIdx,  userData));

      figureStartIdx = figureEndIdx;
    }

    return BL_SUCCESS;
  }

  // Opens a new figure with a line segment starting from the current point and ending at `p1`. The `n1` is a
  // normal calculated from a unit vector of `p1 - _p0`.
  //
  // This function can only be called after we have at least two vertices that form the line. These vertices
  // cannot be a single point as that would mean that we cannot calculate unit vector and then normal for the
  // offset. This must be handled before calling `openLineTo()`.
  //
  // NOTE: Path cannot be open when calling this function.
  BL_INLINE_IF_NOT_DEBUG BLResult openLineTo(const BLPoint& p1, const BLPoint& n1) noexcept {
    BL_ASSERT(!isOpen());
    BLPoint w = n1 * d();

    aOut().moveTo(_p0 + w);
    bOut().moveTo(_p0 - w);

    _p0 = p1;
    _n0 = n1;
    _nInitial = n1;

    aOut().lineTo(_p0 + w);
    bOut().lineTo(_p0 - w);

    _flags |= kFlagIsOpen;
    return BL_SUCCESS;
  }

  // Joins line-to segment described by `p1` point and `n1` normal.
  BL_INLINE_IF_NOT_DEBUG BLResult joinLineTo(const BLPoint& p1, const BLPoint& n1) noexcept {
    if (_n0 == n1) {
      // Collinear case - patch the previous point(s) if they connect lines.
      aOut().back(aOut().cmd[-2].value <= BL_PATH_CMD_ON);
      bOut().back(bOut().cmd[-2].value <= BL_PATH_CMD_ON);

      BLPoint w1 = n1 * d();
      aOut().lineTo(p1 + w1);
      bOut().lineTo(p1 - w1);
    }
    else {
      Side side = sideFromNormals(_n0, n1);
      BLPoint m = _n0 + n1;
      BLPoint k = m * d2(side) / Geometry::lengthSq(m);
      BLPoint w1 = n1 * d(side);

      size_t miterFlag = 0;

      if (side == Side::kA) {
        PathAppender& outer = aOut();
        outerJoin(outer, n1, w1, k, d(side), d2(side), miterFlag);
        outer.back(miterFlag);
        outer.lineTo(p1 + w1);

        PathAppender& inner = bOut();
        innerJoinLineTo(inner, _p0 - w1, p1 - w1, _p0 - k);
        inner.lineTo(p1 - w1);
      }
      else {
        PathAppender& outer = bOut();
        outerJoin(outer, n1, w1, k, d(side), d2(side), miterFlag);
        outer.back(miterFlag);
        outer.lineTo(p1 + w1);

        PathAppender& inner = aOut();
        innerJoinLineTo(inner, _p0 - w1, p1 - w1, _p0 - k);
        inner.lineTo(p1 - w1);
      }
    }

    _p0 = p1;
    _n0 = n1;
    return BL_SUCCESS;
  }

  // Opens a new figure at the current point `_p0`. The first vertex (MOVE) is calculated by offsetting `_p0`
  // by the given unit normal `n0`.
  //
  // NOTE: Path cannot be open when calling this function.
  BL_INLINE_IF_NOT_DEBUG BLResult openCurve(const BLPoint& n0) noexcept {
    BL_ASSERT(!isOpen());
    BLPoint w = n0 * d();

    aOut().moveTo(_p0 + w);
    bOut().moveTo(_p0 - w);

    _n0 = n0;
    _nInitial = n0;

    _flags |= kFlagIsOpen;
    return BL_SUCCESS;
  }

  // Joins curve-to segment.
  BL_INLINE_IF_NOT_DEBUG BLResult joinCurve(const BLPoint& n1) noexcept {
    // Collinear case - do nothing.
    if (_n0 == n1)
      return BL_SUCCESS;

    Side side = sideFromNormals(_n0, n1);
    BLPoint m = _n0 + n1;
    BLPoint k = m * d2(side) / Geometry::lengthSq(m);
    BLPoint w1 = n1 * d(side);
    size_t dummyMiterFlag;

    outerJoin(outerAppender(side), n1, w1, k, d(side), d2(side), dummyMiterFlag);
    innerJoinCurveTo(innerAppender(side), _p0 - w1);

    _n0 = n1;
    return BL_SUCCESS;
  }

  BL_INLINE_IF_NOT_DEBUG BLResult joinCusp(const BLPoint& n1) noexcept {
    Side side = sideFromNormals(_n0, n1);
    BLPoint w1 = n1 * d(side);

    dullRoundJoin(outerAppender(side), d(side), d2(side), w1);
    innerAppender(side).lineTo(_p0 - w1);

    _n0 = n1;
    return BL_SUCCESS;
  }

  BL_INLINE_IF_NOT_DEBUG BLResult joinCuspAndLineTo(const BLPoint& n1, const BLPoint& p1) noexcept {
    Side side = sideFromNormals(_n0, n1);
    BLPoint w1 = n1 * d(side);

    PathAppender& outer = outerAppender(side);
    dullRoundJoin(outer, d(side), d2(side), w1);
    outer.lineTo(p1 + w1);

    PathAppender& inner = innerAppender(side);
    inner.lineTo(_p0 - w1);
    inner.lineTo(p1 - w1);

    _n0 = n1;
    _p0 = p1;
    return BL_SUCCESS;
  }

  BL_INLINE_IF_NOT_DEBUG BLResult smoothPolyTo(const BLPoint* poly, size_t count) noexcept {
    BL_ASSERT(count >= 2);

    BLPoint p1 = poly[0];
    BLPoint v1 = p1 - _p0;
    if (Geometry::lengthSq(v1) < kStrokeLengthEpsilonSq)
      return BL_SUCCESS;

    BLPoint n1 = Geometry::normal(Geometry::unitVector(v1));
    if (!isOpen())
      BL_PROPAGATE(openLineTo(p1, n1));
    else
      BL_PROPAGATE(joinLineTo(p1, n1));

    // We have already ensured vertices for `openLineTo()` and `joinLineTo()`,
    // however, we need more vertices for consecutive joins and line segments.
    size_t requiredCapacity = (count - 1) * kStrokeMaxJoinVertices;
    BL_PROPAGATE(ensureAppendersCapacity(requiredCapacity, requiredCapacity));

    for (size_t i = 1; i < count; i++) {
      p1 = poly[i];
      v1 = p1 - _p0;
      if (Geometry::lengthSq(v1) < kStrokeLengthEpsilonSq)
        continue;

      n1 = Geometry::normal(Geometry::unitVector(v1));
      BL_PROPAGATE(joinCuspAndLineTo(n1, p1));
    }

    return BL_SUCCESS;
  }

  // Joins end point that is only applied to closed figures.
  BL_INLINE_IF_NOT_DEBUG BLResult joinEndPoint(const BLPoint& n1) noexcept {
    if (_n0 == n1) {
      // Collinear case - patch the previous point(s) if they connect lines.
      aOut().back(aOut().cmd[-2].value <= BL_PATH_CMD_ON);
      bOut().back(bOut().cmd[-2].value <= BL_PATH_CMD_ON);
      return BL_SUCCESS;
    }

    Side side = sideFromNormals(_n0, n1);
    BLPoint m = _n0 + n1;
    BLPoint w1 = n1 * d(side);
    BLPoint k = m * d2(side) / Geometry::lengthSq(m);

    size_t miterFlag = 0;

    BLPathPrivateImpl* outerImpl = getImpl(outerPath(side));
    size_t outerStart = sideData(side).figureOffset;

    PathAppender& outer = outerAppender(side);
    outerJoin(outer, n1, w1, k, d(side), d2(side), miterFlag);

    // Shift the start point to be at the miter intersection and remove the
    // Line from the intersection to the start of the path if miter was applied.
    if (miterFlag) {
      if (outerImpl->commandData[outerStart + 1] == BL_PATH_CMD_ON) {
        outer.back();
        outerImpl->vertexData[outerStart] = outer.vtx[-1];
        outer.back(outer.cmd[-2].value <= BL_PATH_CMD_ON);
      }
    }

    BLPathPrivateImpl* innerImpl = getImpl(innerPath(side));
    size_t innerStart = sideData(oppositeSide(side)).figureOffset;

    if (innerImpl->commandData[innerStart + 1] <= BL_PATH_CMD_ON) {
      innerJoinEndPoint(innerAppender(side), innerImpl->vertexData[innerStart], innerImpl->vertexData[innerStart + 1], _p0 - k);
    }

    return BL_SUCCESS;
  }

  BL_INLINE_IF_NOT_DEBUG void innerJoinCurveTo(PathAppender& out, const BLPoint& p1) noexcept {
    out.lineTo(_p0);
    out.lineTo(p1);
  }

  BL_INLINE_IF_NOT_DEBUG void innerJoinLineTo(PathAppender& out, const BLPoint& lineP0, const BLPoint& lineP1, const BLPoint& innerPt) noexcept {
    if (out.cmd[-2].value <= BL_PATH_CMD_ON && testInnerJoinIntersecion(out.vtx[-2], out.vtx[-1], lineP0, lineP1, innerPt)) {
      out.vtx[-1] = innerPt;
    }
    else {
      out.lineTo(_p0);
      out.lineTo(lineP0);
    }
  }

  BL_INLINE_IF_NOT_DEBUG void innerJoinEndPoint(PathAppender& out, BLPoint& lineP0, const BLPoint& lineP1, const BLPoint& innerPt) noexcept {
    if (out.cmd[-2].value <= BL_PATH_CMD_ON && testInnerJoinIntersecion(out.vtx[-2], out.vtx[-1], lineP0, lineP1, innerPt)) {
      lineP0 = innerPt;
      out.back(1);
    }
    else {
      out.lineTo(_p0);
      out.lineTo(lineP0);
    }
  }

  // Calculates outer join to `pb`.
  BL_INLINE_IF_NOT_DEBUG BLResult outerJoin(PathAppender& appender, const BLPoint& n1, const BLPoint& w1, const BLPoint& k, double d, double d2, size_t& miterFlag) noexcept {
    BLPoint pb = _p0 + w1;

    if (Geometry::lengthSq(k) <= _miterLimitSq) {
      // Miter condition is met.
      appender.back(appender.cmd[-2].value <= BL_PATH_CMD_ON);
      appender.lineTo(_p0 + k);
      appender.lineTo(pb);

      miterFlag = 1;
      return BL_SUCCESS;
    }

    if (_joinType == BL_STROKE_JOIN_MITER_CLIP) {
      double b2 = blAbs(Geometry::cross(k, _n0));

      // Avoid degenerate cases and NaN.
      if (b2 > 0)
        b2 = b2 * _miterLimit / Geometry::length(k);
      else
        b2 = _miterLimit;

      appender.back(appender.cmd[-2].value <= BL_PATH_CMD_ON);
      appender.lineTo(_p0 + d * _n0 - b2 * Geometry::normal(_n0));
      appender.lineTo(_p0 + d *  n1 + b2 * Geometry::normal(n1));

      miterFlag = 1;
      appender.lineTo(pb);
      return BL_SUCCESS;
    }

    if (_joinType == BL_STROKE_JOIN_ROUND) {
      BLPoint pa = appender.vtx[-1];
      if (Geometry::dot(_p0 - pa, _p0 - pb) < 0.0) {
        // Dull angle.
        BLPoint n2 = Geometry::normal(Geometry::unitVector(pb - pa));
        BLPoint m = _n0 + n2;
        BLPoint k0 = d2 * m / Geometry::lengthSq(m);
        BLPoint q = d * n2;

        BLPoint pc1 = _p0 + k0;
        BLPoint pp1 = _p0 + q;
        BLPoint pc2 = Math::lerp(pc1, pp1, 2.0);

        dullAngleArcTo(appender, _p0, pa, pp1, pc1);
        dullAngleArcTo(appender, _p0, pp1, pb, pc2);
      }
      else {
        // Acute angle.
        BLPoint pm = Math::lerp(pa, pb);
        BLPoint pi = _p0 + k;

        double w = Math::sqrt(Geometry::length(_p0 - pm) / Geometry::length(_p0 - pi));
        double a = 4.0 * w / (3.0 * (1.0 + w));

        BLPoint c0 = pa + a * (pi - pa);
        BLPoint c1 = pb + a * (pi - pb);

        appender.cubicTo(c0, c1, pb);
      }
      return BL_SUCCESS;
    }

    // Bevel or unknown `_joinType`.
    appender.lineTo(pb);
    return BL_SUCCESS;
  }

  // Calculates round join to `pb` (dull angle), only used by offsetting cusps.
  BL_INLINE_IF_NOT_DEBUG BLResult dullRoundJoin(PathAppender& out, double d, double d2, const BLPoint& w1) noexcept {
    BLPoint pa = out.vtx[-1];
    BLPoint pb = _p0 + w1;
    BLPoint n2 = Geometry::normal(Geometry::unitVector(pb - pa));

    if (!Math::isFinite(n2.x))
      return BL_SUCCESS;

    BLPoint m = _n0 + n2;
    BLPoint k = m * d2 / Geometry::lengthSq(m);
    BLPoint q = n2 * d;

    BLPoint pc1 = _p0 + k;
    BLPoint pp1 = _p0 + q;
    BLPoint pc2 = Math::lerp(pc1, pp1, 2.0);

    dullAngleArcTo(out, _p0, pa, pp1, pc1);
    dullAngleArcTo(out, _p0, pp1, pb, pc2);
    return BL_SUCCESS;
  }

  BL_INLINE_IF_NOT_DEBUG BLResult offsetQuad(const BLPoint bez[3]) noexcept {
    double ts[3];
    size_t tn = Geometry::getQuadOffsetCuspTs(bez, d(), ts);
    ts[tn++] = 1.0;

    Geometry::QuadCurveTsIter iter(bez, ts, tn);
    double m = _approx.offsetParameter;

    do {
      for (;;) {
        BL_PROPAGATE(ensureAppendersCapacity(2, 2));

        double t = Geometry::quadParameterAtAngle(iter.part, m);
        if (!(t > kOffsetQuadEpsilonT && t < 1.0 - kOffsetQuadEpsilonT))
          t = 1.0;

        BLPoint part[3];
        Geometry::splitQuad(iter.part, part, iter.part, t);
        offsetQuadSimple(part[0], part[1], part[2]);

        if (t == 1.0)
          break;
      }
    } while (iter.next());

    return BL_SUCCESS;
  }

  BL_INLINE_IF_NOT_DEBUG void offsetQuadSimple(const BLPoint& p0, const BLPoint& p1, const BLPoint& p2) noexcept {
    if (p0 == p2)
      return;

    BLPoint v0 = p1 - p0;
    BLPoint v1 = p2 - p1;

    BLPoint m0 = Geometry::normal(Geometry::unitVector(p0 != p1 ? v0 : v1));
    BLPoint m2 = Geometry::normal(Geometry::unitVector(p1 != p2 ? v1 : v0));

    _p0 = p2;
    _n0 = m2;

    BLPoint m = m0 + m2;
    BLPoint k1 = m * d2() / Geometry::lengthSq(m);
    BLPoint k2 = m2 * d();

    aOut().quadTo(p1 + k1, p2 + k2);
    bOut().quadTo(p1 - k1, p2 - k2);
  }

  struct CubicApproximateSink {
    PathStroker& _stroker;

    BL_INLINE CubicApproximateSink(PathStroker& stroker) noexcept
      : _stroker(stroker) {}

    BL_INLINE BLResult operator()(BLPoint quad[3]) const noexcept {
      return _stroker.offsetQuad(quad);
    }
  };

  BL_INLINE_IF_NOT_DEBUG BLResult offsetCubic(const BLPoint bez[4]) noexcept {
    CubicApproximateSink sink(*this);
    return Geometry::approximateCubicWithQuads(bez, _approx.simplifyTolerance, sink);
  }

  BL_INLINE_IF_NOT_DEBUG BLResult addCap(PathAppender& out, BLPoint pivot, BLPoint p1, uint32_t capType) noexcept {
    BLPoint p0 = out.vtx[-1];
    BLPoint q = Geometry::normal(p1 - p0) * 0.5;

    switch (capType) {
      case BL_STROKE_CAP_BUTT:
      default: {
        out.lineTo(p1);
        break;
      }

      case BL_STROKE_CAP_SQUARE: {
        out.lineTo(p0 + q);
        out.lineTo(p1 + q);
        out.lineTo(p1);
        break;
      }

      case BL_STROKE_CAP_ROUND: {
        out.arcQuadrantTo(p0 + q, pivot + q);
        out.arcQuadrantTo(p1 + q, p1);
        break;
      }

      case BL_STROKE_CAP_ROUND_REV: {
        out.lineTo(p0 + q);
        out.arcQuadrantTo(p0, pivot);
        out.arcQuadrantTo(p1, p1 + q);
        out.lineTo(p1);
        break;
      }

      case BL_STROKE_CAP_TRIANGLE: {
        out.lineTo(pivot + q);
        out.lineTo(p1);
        break;
      }

      case BL_STROKE_CAP_TRIANGLE_REV: {
        out.lineTo(p0 + q);
        out.lineTo(pivot);
        out.lineTo(p1 + q);
        out.lineTo(p1);
        break;
      }
    }

    return BL_SUCCESS;
  }
};

// bl::Path - Stroke - Interface
// =============================

BLResult strokePath(
  const BLPathView& input,
  const BLStrokeOptions& options,
  const BLApproximationOptions& approx,
  BLPath& a,
  BLPath& b,
  BLPath& c,
  BLPathStrokeSinkFunc sink, void* userData) noexcept {

  return PathStroker(input, options, approx, &a, &b, &c).stroke(sink, userData);
}

} // {PathInternal}
} // {bl}
