// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PATH_STROKE_INTERNAL_P_H_INCLUDED
#define BLEND2D_PATH_STROKE_INTERNAL_P_H_INCLUDED

#include "bezier_p.h"
#include "path-options_p.h"
#include "path-dash-internal_p.h"
#include "path-offset-internal_p.h"
#include "path2_p.h"

void insertStrokeJoin(
    BLPath2 &left,
    BLPath2 &right,
    BLPoint2 p,
    BLVector2 m0,
    BLVector2 m1,
    double d,
    double ml,
    BLStrokeJoin join)
{
  BLVector2 n0 = m0.unit().normal();
  BLVector2 n1 = m1.unit().normal();

  // Check if join is not too flat
  if (n0.dot(n1) < COS_OBTUSE)
  {
    if (n0.cross(n1) >= 0)
    {
      insertOuterJoin(left, p, n0, n1, d, ml, join);
      insertInnerJoin(right, p, n1, -d);
    }
    else
    {
      insertOuterJoin(right, p, n0, n1, -d, ml, join);
      insertInnerJoin(left, p, n1, d);
    }
  }
}

void insertStrokeCap(BLPath2 &path, BLPoint2 p1, BLStrokeCap cap)
{
  switch (cap)
  {
  case BL_STROKE_CAP_BUTT:
  {
    path.lineTo(p1);
    break;
  }
  case BL_STROKE_CAP_SQUARE:
  {
    Option<BLPoint2> pp0 = path.getLastPoint();

    if (!pp0.hasValue)
    {
      break;
    }

    BLPoint2 p0 = pp0.value;

    BLVector2 v = 0.5 * (p1 - p0).normal();
    path.lineTo(p0 + v);
    path.lineTo(p1 + v);
    path.lineTo(p1);

    break;
  }
  case BL_STROKE_CAP_ROUND:
  {
    Option<BLPoint2> pp0 = path.getLastPoint();

    if (!pp0.hasValue)
    {
      break;
    }

    BLPoint2 p0 = pp0.value;

    BLVector2 v = 0.5 * (p1 - p0).normal();
    path.arcTo(p0 + v, p0 + (v - v.normal()));
    path.arcTo(p1 + v, p1);

    break;
  }
    // default: {
    //     const p0 = path.getLastPoint();

    //     if (p0 === undefined) {
    //         break;
    //     }

    //     cap(path, p0, p1);

    //     // Check if last point is set by the callback
    //     if (path.getLastPoint()?.eq(p1) === true) {
    //         break;
    //     }

    //     // Fallback
    //     path.lineTo(p1);
    // }
  }
}

void combineStroke(
    BLPath2 &output,
    BLPath2 &left,
    BLPath2 &right,
    BLStrokeCap startCap,
    BLStrokeCap endCap)
{
  Option<BLPoint2> pp1 = right.getLastPoint();
  Option<BLPoint2> pp2 = left.getFirstPoint();

  if (!pp1.hasValue | !pp2.hasValue)
  {
    return;
  }

  BLPoint2 p1 = pp1.value;
  BLPoint2 p2 = pp2.value;

  output.addPath(left, false);
  insertStrokeCap(output, p1, endCap);
  output.addPathReversed(right, true);
  insertStrokeCap(output, p2, startCap);
  output.close();
}

struct BLStrokeState
{
  BLStrokeCaps caps;
  size_t currentIndex;
  size_t currentLength;
  size_t currentPhase;
  BLArray<double> dashArray;
  BLStrokeCaps dashCaps;
  double distance;
  bool isDash;
  bool isFirstDash;
  BLStrokeJoin join;
  BLPath2 leftFirst;
  BLPath2 leftMain;
  double miterLimit;
  BLVector2 ms;
  BLPath2 rightFirst;
  BLPath2 rightMain;
  size_t startAdvancedLength;
  size_t startIndex;
  bool startPhase;

  BLPath2 *output;
  BLPath2 *left;
  BLPath2 *right;

  BLStrokeState() noexcept : caps(BLStrokeCaps{.start = BL_STROKE_CAP_BUTT, .end = BL_STROKE_CAP_BUTT}),
                             dashArray(BLArray<double>()),
                             dashCaps(BLStrokeCaps{.start = BL_STROKE_CAP_BUTT, .end = BL_STROKE_CAP_BUTT}),
                             distance(0.5),
                             join(BL_STROKE_JOIN_MITER_CLIP),
                             miterLimit(4),
                             currentIndex(0),
                             currentLength(0),
                             currentPhase(false),
                             isDash(false),
                             isFirstDash(true),
                             ms(BLVector2::zero()),
                             startAdvancedLength(0),
                             startIndex(0),
                             startPhase(false),
                             leftFirst(BLPath2()),
                             rightFirst(BLPath2()),
                             leftMain(BLPath2()),
                             rightMain(BLPath2()),

                             output(),
                             left(),
                             right()
  {
  }

  void finalizeClosed()
  {
    if (isDash)
    {
      finalizeClosedDashStroke();
    }
    else
    {
      finalizeClosedStroke();
    }
  }

  void finalizeOpen()
  {
    if (isDash)
    {
      finalizeOpenDashStroke();
    }
    else
    {
      finalizeOpenStroke();
    }
  }

  void finalizePoint(BLPoint2 p)
  {
    insertMoveStroke(p, BLVector2::unitX());
    finalizeOpen();
  }

  void initialize(BLPath2 &output, BLPathStrokeOptions &options)
  {
    this->output = &output;

    if (options.dashArray.size() > 0)
    {
      initializeStroke(options);
      initializeDashStroke(options);
      resetDash();

      isDash = true;
    }
    else
    {
      initializeStroke(options);
      resetStroke();

      left = &leftMain;
      right = &rightMain;
      isDash = false;
    }
  }

  void strokeFirstOrJoin(BLPoint2 p, BLVector2 m0, BLVector2 m1)
  {
    if (isDash)
    {
      insertFirstOrJoinDashStroke(p, m0, m1);
    }
    else
    {
      insertFirstOrJoinStroke(p, m0, m1);
    }
  }

  void strokeLinear(BLBezier1Curve2 c0, BLVector2 m)
  {
    if (isDash)
    {
      insertLinearDashStroke(c0);
    }
    else
    {
      insertLinearStroke(c0.p1, m);
    }
  }

  void strokeQuadraticDegenerate(BLPoint2 p0, BLPoint2 p1, BLPoint2 p2)
  {
    if (isDash)
    {
      insertQuadraticDegenerateDashStroke(p0, p1, p2);
    }
    else
    {
      insertQuadraticDegenerateStroke(p0, p1, p2);
    }
  }

  void strokeQuadraticSimple(BLBezier2Curve2 c0)
  {
    if (isDash)
    {
      insertQuadraticSimpleDashStroke(c0);
    }
    else
    {
      insertQuadraticSimpleStroke(c0);
    }
  }

  void advanceDash()
  {
    size_t index = getDashIndexNext(dashArray, currentIndex);

    if (isFirstDash)
    {
      isFirstDash = false;

      left = &leftMain;
      right = &rightMain;
    }
    else if (currentPhase)
    {
      combineStroke(*output, leftMain, rightMain, dashCaps.start, dashCaps.end);

      resetStroke();
    }

    currentLength = 0;
    currentIndex = index;
    currentPhase = !currentPhase;
  }

  void finalizeClosedDashStroke()
  {
    if (startPhase && currentPhase)
    {
      if (isFirstDash)
      {
        // First dash is closed
        leftFirst.close();
        rightFirst.close();

        output->addPath(leftFirst, false);
        output->addPathReversed(rightFirst, false);
      }
      else
      {
        // Last and first dash are connected
        leftMain.addPath(leftFirst, true);
        rightMain.addPath(rightFirst, true);

        combineStroke(*output, leftMain, rightMain, dashCaps.start, dashCaps.end);
      }
    }
    else
    {
      // Last and first dash are not connected
      combineStroke(*output, leftMain, rightMain, dashCaps.start, caps.end);
      combineStroke(*output, leftFirst, rightFirst, caps.start, dashCaps.end);
    }

    resetDash();
  }

  void finalizeClosedStroke()
  {
    left->close();
    right->close();

    output->addPath(*left, false);
    output->addPathReversed(*right, false);

    resetStroke();
  }

  void finalizeOpenDashStroke()
  {
    if (!isFirstDash)
    {
      // Last dash
      combineStroke(*output, leftMain, rightMain, dashCaps.start, caps.end);
    }

    if (startPhase)
    {
      // First dash
      combineStroke(*output, leftFirst, rightFirst, caps.start, dashCaps.end);
    }

    resetDash();
  }

  void finalizeOpenStroke()
  {
    combineStroke(*output, *left, *right, caps.start, caps.end);

    resetStroke();
  }

  double getDashLength()
  {
    return dashArray[currentIndex];
  }

  void initializeDashStroke(BLPathStrokeOptions &options)
  {
    dashArray = options.dashArray;
    dashCaps = options.dashCaps;

    double length;
    size_t index;
    bool phase;
    getDashStart(options.dashArray, options.dashOffset, length, index, phase);

    startAdvancedLength = length;
    startIndex = index;
    startPhase = phase;
  }

  void initializeStroke(BLPathStrokeOptions &options)
  {
    caps = options.caps;
    distance = 0.5 * options.width;
    join = options.join;
    miterLimit = options.miterLimit;
  }

  void insertFirstOrJoinDashStroke(BLPoint2 p, BLVector2 m0, BLVector2 m1)
  {
    if (currentPhase)
    {
      insertFirstOrJoinStroke(p, m0, m1);
    }
  }

  void insertFirstOrJoinStroke(BLPoint2 p, BLVector2 m0, BLVector2 m1)
  {
    if (m0.isZero())
    {
      insertMoveStroke(p, m1);
      ms = m1;
    }
    else
    {
      insertStrokeJoin(*left, *right, p, m0, m1, distance, miterLimit, join);
    }
  }

  void insertLinearDashStroke(BLBezier1Curve2 c0)
  {
    BLBezier1Curve2 c = c0;

    // Remaining length of the current dash and full length of the line
    double dashRemainingLength = getDashLength() - currentLength;
    double length = getLengthLinear(c);

    while (dashRemainingLength < length)
    {
      double t = getParameterAtLengthLinear(c, dashRemainingLength);

      c = c.splitAfter(t);
      length = getLengthLinear(c);

      if (currentPhase)
      {
        insertLinearStroke(c.p0, c.getDerivative());
      }
      else
      {
        insertMoveStroke(c.p0, c.getDerivative());
      }

      advanceDash();

      dashRemainingLength = getDashLength();
    }

    currentLength += length;

    if (currentPhase)
    {
      insertLinearStroke(c.p1, c.getTangentStart());
    }
  }

  void insertLinearStroke(BLPoint2 p, BLVector2 m)
  {
    BLVector2 v = distance * m.unit().normal();

    left->lineTo(p + v);
    right->lineTo(p - v);
  }

  void insertMoveStroke(BLPoint2 p0, BLVector2 m)
  {
    BLVector2 v = distance * m.unit().normal();

    left->moveTo(p0 + v);
    right->moveTo(p0 - v);
  }

  void insertQuadraticDegenerateDashStroke(BLPoint2 p0, BLPoint2 p1, BLPoint2 p2)
  {
    BLBezier2Curve2 c1 = BLBezier2Curve2(p0, p1, p1);
    BLBezier2Curve2 c2 = BLBezier2Curve2(p1, p1, p2);

    BLVector2 n0 = (p1 - p0).unit().normal();
    BLVector2 n1 = (p2 - p1).unit().normal();

    insertQuadraticSimpleStroke(c1);

    if (currentPhase)
    {
      insertOuterJoin(*left, p1, n0, n1, distance, 0, BL_STROKE_JOIN_ROUND);
      insertOuterJoin(*right, p1, -n0, -n1, distance, 0, BL_STROKE_JOIN_ROUND);
    }

    insertQuadraticSimpleStroke(c2);
  }

  void insertQuadraticDegenerateStroke(BLPoint2 p0, BLPoint2 p1, BLPoint2 p2)
  {
    BLBezier2Curve2 c1 = BLBezier2Curve2(p0, p1, p1);
    BLBezier2Curve2 c2 = BLBezier2Curve2(p1, p1, p2);

    BLVector2 n0 = (p1 - p0).unit().normal();
    BLVector2 n1 = (p2 - p1).unit().normal();

    insertQuadraticSimpleStroke(c1);

    insertOuterJoin(*left, p1, n0, n1, distance, 0, BL_STROKE_JOIN_ROUND);
    insertOuterJoin(*right, p1, -n0, -n1, distance, 0, BL_STROKE_JOIN_ROUND);

    insertQuadraticSimpleStroke(c2);
  }

  void insertQuadraticSimpleDashStroke(BLBezier2Curve2 c0)
  {
    BLBezier2Curve2 c = c0;

    // Remaining length of the current dash and full length of the curve
    double dashRemainingLength = getDashLength() - currentLength;
    double length = getLengthQuadratic(c);

    while (dashRemainingLength < length)
    {
      double t = getParameterAtLengthQuadratic(c, dashRemainingLength);

      BLBezier2Curve2 c1, c2;
      c.splitAt(t, c1, c2);
      length = getLengthQuadratic(c2);

      if (currentPhase)
      {
        insertQuadraticSimpleStroke(c1);
      }
      else
      {
        insertMoveStroke(c2.p0, c2.getTangentStart());
      }

      advanceDash();

      dashRemainingLength = getDashLength();
      c = c2;
    }

    currentLength += length;

    if (currentPhase)
    {
      insertQuadraticSimpleStroke(c);
    }
  }

  void insertQuadraticSimpleStroke(BLBezier2Curve2 c)
  {
    // Possible null vector (curve is a point)
    BLVector2 v1 = c.getTangentStart();
    BLVector2 v2 = c.getTangentEnd();

    if (!v1.isZero())
    {
      double d = distance;

      v1 = v1.unit().normal();
      v2 = v2.unit().normal();

      v1 = v1 + v2;

      v1 = 2 * d * v1 / v1.lengthSq();
      v2 = d * v2;

      left->quadTo(c.p1 + v1, c.p2 + v2);
      right->quadTo(c.p1 - v1, c.p2 - v2);
    }
  }

  void resetDash()
  {
    leftFirst.clear();
    rightFirst.clear();
    leftMain.clear();
    rightMain.clear();

    currentIndex = startIndex;
    currentLength = startAdvancedLength;

    if (startPhase)
    {
      left = &leftFirst;
      right = &rightFirst;
      currentPhase = true;
      isFirstDash = true;
    }
    else
    {
      left = &leftMain;
      right = &rightMain;
      currentPhase = false;
      isFirstDash = false;
    }
  }

  void resetStroke()
  {
    leftMain.clear();
    rightMain.clear();
  }
};

#endif // BLEND2D_PATH_STROKE_INTERNAL_P_H_INCLUDED
