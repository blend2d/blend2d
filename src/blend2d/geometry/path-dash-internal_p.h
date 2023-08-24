// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PATH_DASH_INTERNAL_P_H_INCLUDED
#define BLEND2D_PATH_DASH_INTERNAL_P_H_INCLUDED

#include "bezier_p.h"
#include "path2_p.h"
#include "solve_p.h"

size_t getDashIndexNext(BLArray<double> &dashArray, size_t index)
{
  return (index + 1) % dashArray.size();
}

double getDashLength(BLArray<double> &dashArray)
{
  double length = 0;

  for (size_t i = 0; i < dashArray.size(); i++)
  {
    length += dashArray[i];
  }

  return length;
}

void getDashStart(BLArray<double> &dashArray, double dashOffset, double &length, size_t &index, bool &phase)
{
  length = 2 * getDashLength(dashArray);
  size_t offset = fmod(dashOffset, length);

  if (offset < 0)
  {
    offset += length;
  }

  index = 0;
  phase = true;

  length = dashArray[index];

  while (offset >= length)
  {
    offset -= length;

    index = getDashIndexNext(dashArray, index);
    phase = !phase;

    length = dashArray[index];
  }

  length = offset;
}

double getLengthLinear(BLBezier1Curve2 c)
{
  return (c.p1 - c.p0).length();
}

double gaussLegendreQuadratic(double wz, double xz, BLVector2 qqa, BLVector2 qqb)
{
  return (wz * (qqb + xz * qqa)).length();
}

double getLengthQuadratic(BLBezier2Curve2 c)
{
  // https://pomax.github.io/bezierinfo/legendre-gauss.html
  // Let `wz = (z / 2) * w` and `xz = (z / 2) * x + (z / 2)` with `z = 1`,
  // so that `sum += wz * (B + xz * A).length` is the arc length
  double sum = 0;

  BLVector2 qqa, qqb;
  c.getDerivativeCoefficients(qqa, qqb);

  // Weights and abscissae for `n = 4`
  sum += gaussLegendreQuadratic(0.1739274225687269, 0.06943184420297371, qqa, qqb);
  sum += gaussLegendreQuadratic(0.3260725774312731, 0.3300094782075719, qqa, qqb);
  sum += gaussLegendreQuadratic(0.3260725774312731, 0.6699905217924281, qqa, qqb);
  sum += gaussLegendreQuadratic(0.1739274225687269, 0.9305681557970263, qqa, qqb);

  return sum;
}

double getParameterAtLengthLinear(BLBezier1Curve2 c, double length)
{
  return length / getLengthLinear(c);
}

double interpolateQuadratic(BLBezier2Curve2 c, double d)
{
  double d1 = (c.p1 - c.p0).length();
  double d2 = (c.p2 - c.p1).length();

  // Solve `(d2 - d1) * t^2 + d1 * t = d` for `t` (`t = 1` -> `d1 + d2 = d`)
  QuadraticSolveResult r = solveQuadratic(d2 - d1, d1, -d);

  if (r.type == kQuadraticSolveTypeTwo)
  {
    return r.x[0];
  }
  else
  {
    // Fallback
    return (d1 + d2) / d;
  }
}

double getParameterAtLengthQuadratic(BLBezier2Curve2 c, double length)
{
  double t = interpolateQuadratic(c, length);

  if (t < 1.0)
  {
    BLBezier2Curve2 c1, c2;
    c.splitAt(t, c1, c2);

    // Refine solution one more time
    double d = length - getLengthQuadratic(c1);
    t += (1.0 - t) * interpolateQuadratic(c2, d);
  }

  return t;
}

#endif // BLEND2D_PATH_DASH_INTERNAL_P_H_INCLUDED
