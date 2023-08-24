// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PATH_SIMPLIFY_P_H_INCLUDED
#define BLEND2D_PATH_SIMPLIFY_P_H_INCLUDED

#include "bezier_p.h"

double simplifyParameterStepQuad(BLBezier2Curve2 c, double m)
{
  BLVector2 qqa, qqb;
  c.getDerivativeCoefficients(qqa, qqb);

  // m * (bx * bx + by * by) / (|ax * by - ay * bx| - m * (ax * bx + ay * by));
  return (m * qqb.lengthSq()) / (fabs(qqa.cross(qqb)) - m * qqa.dot(qqb));
}

double simplifyParameterStepConic(BLBezierRCurve2 c, double k, double tolerance)
{
  BLVector2 v1 = c.p1 - c.p0;
  BLVector2 v2 = c.p2 - c.p1;

  BLVector2 v = v2 - v1;
  double tol = k * tolerance * (c.w + 1.0);

  // Smallest parameter step to satisfy tolerance condition
  return pow(tol / (fabs(c.w - 1.0) * v.length()), 1.0 / 4.0);
}

double simplifyParameterStepCubic(BLBezier3Curve2 c, double k, double tolerance)
{
  BLVector2 v1 = c.p1 - c.p0;
  BLVector2 v2 = c.p2 - c.p1;
  BLVector2 v3 = c.p3 - c.p2;

  BLVector2 v = v3 - v2 - v2 + v1;
  double tol = k * tolerance;

  // Smallest parameter step to satisfy tolerance condition
  return pow((tol * tol) / v.lengthSq(), 1.0 / 6.0);
}

void simplifyCubicContinious(BLBezier3Curve2 c, BLBezier2Curve2 &c1, BLBezier2Curve2 &c2)
{
  // Continuous at endpoints
  BLPoint2 pc1 = c.p0.lerp(c.p1, 0.75);
  BLPoint2 pc2 = c.p3.lerp(c.p2, 0.75);
  BLPoint2 pm = pc1.lerp(pc2, 0.5);

  c1 = BLBezier2Curve2(c.p0, pc1, pm);
  c2 = BLBezier2Curve2(pm, pc2, c.p3);
}

BLBezier2Curve2 simplifyConic(BLBezierRCurve2 c)
{
  return BLBezier2Curve2(c.p0, c.p1, c.p2);
}

BLBezier2Curve2 simplifyCubicMidpoint(BLBezier3Curve2 c)
{
  // Not continuous at endpoints (midpoint interpolation)
  BLPoint2 pc1 = c.p0.lerp(c.p1, 1.5);
  BLPoint2 pc2 = c.p3.lerp(c.p2, 1.5);
  BLPoint2 pc = pc1.lerp(pc2, 0.5);

  return BLBezier2Curve2(c.p0, pc, c.p3);
}

bool isDegenerateQuad(BLBezier2Curve2 c0)
{
  BLVector2 v1 = c0.p1 - c0.p0;
  BLVector2 v2 = c0.p2 - c0.p1;

  // Check if angle is too sharp
  return v1.dot(v2) < COS_ACUTE * sqrt(v1.lengthSq() * v2.lengthSq());
}

#endif // BLEND2D_PATH_SIMPLIFY_P_H_INCLUDED
