// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PATH_FLATTEN_P_H_INCLUDED
#define BLEND2D_PATH_FLATTEN_P_H_INCLUDED

#include "bezier_p.h"
#include "path-options_p.h"
#include "path-simplify_p.h"
#include "path2_p.h"

struct BLPathFlatten2
{
  double tolerance;
  double simplifyTolerance;

  BLPathFlatten2(BLPathQualityOptions options) noexcept
      : tolerance(options.flattenTolerance),
        simplifyTolerance(options.simplifyTolerance) {}

  void process(BLPath2 &input, BLPath2 &output, bool forceClose)
  {
    // Check if path is empty or invalid
    if (!input.isValid())
    {
      return;
    }

    BLArray<BLPathCmd> commands = input.getCommands();
    BLArray<BLPoint2> points = input.getPoints();
    BLArray<double> weights = input.getWeights();

    size_t cIdx = 0;
    size_t pIdx = 0;
    size_t wIdx = 0;

    BLPoint2 ps = BLPoint2::zero();
    BLPoint2 p0 = BLPoint2::zero();

    while (cIdx < commands.size())
    {
      BLPathCmd command = commands[cIdx++];
      switch (command)
      {
      case BL_PATH_CMD_MOVE:
      {
        if (forceClose && p0 != ps)
        {
          output.lineTo(ps);
        }
        ps = points[pIdx++];
        output.moveTo(ps);
        p0 = ps;
        break;
      }
      case BL_PATH_CMD_ON:
      {
        BLPoint2 p1 = points[pIdx++];
        output.lineTo(p1);
        p0 = p1;
        break;
      }
      case BL_PATH_CMD_QUAD:
      {
        BLPoint2 p1 = points[pIdx++];
        BLPoint2 p2 = points[pIdx++];
        BLBezier2Curve2 c = BLBezier2Curve2(p0, p1, p2);
        flattenQuadratic(c, output);
        p0 = c.p2;
        break;
      }
      case BL_PATH_CMD_CUBIC:
      {
        BLPoint2 p1 = points[pIdx++];
        BLPoint2 p2 = points[pIdx++];
        BLPoint2 p3 = points[pIdx++];
        BLBezier3Curve2 c = BLBezier3Curve2(p0, p1, p2, p3);
        flattenCubic(c, output);
        p0 = c.p3;
        break;
      }
      case BL_PATH_CMD_CONIC:
      {
        BLPoint2 p1 = points[pIdx++];
        BLPoint2 p2 = points[pIdx++];
        double w = weights[wIdx++];
        BLBezierRCurve2 c = BLBezierRCurve2(p0, p1, p2, w);
        flattenConic(c, output);
        p0 = c.p2;
        break;
      }
      case BL_PATH_CMD_CLOSE:
      {
        if (p0 != ps)
        {
          output.lineTo(ps);
        }
        output.close();
        break;
      }
      }
    }

    if (forceClose && p0 != ps)
    {
      output.lineTo(ps);
      output.close();
    }
  }

  void flattenConic(BLBezierRCurve2 c0, BLPath2 &output)
  {
    double t = simplifyParameterStepConic(c0, 4.0, simplifyTolerance);
    BLBezierRCurve2 c = c0;

    while (t < 1.0)
    {
      BLBezierRCurve2 c1, c2;
      c.splitAt(t, c1, c2);

      flattenQuadratic(simplifyConic(c1), output);

      t = t / (1.0 - t);
      c = c2;
    }

    flattenQuadratic(simplifyConic(c), output);
  }

  void flattenCubic(BLBezier3Curve2 c0, BLPath2 &output)
  {
    double t = simplifyParameterStepCubic(c0, 54.0, simplifyTolerance);
    BLBezier3Curve2 c = c0;

    while (t < 1.0)
    {
      BLBezier3Curve2 c1, c2;
      c.splitAt(t, c1, c2);

      flattenQuadratic(simplifyCubicMidpoint(c1), output);

      t = t / (1.0 - t);
      c = c2;
    }

    flattenQuadratic(simplifyCubicMidpoint(c), output);
  }

  void flattenQuadratic(BLBezier2Curve2 c0, BLPath2 &output)
  {
    BLVector2 qa, qb;
    BLPoint2 qc;
    c0.getCoefficients(qa, qb, qc);

    // Smallest parameter step to satisfy tolerance condition
    double step = sqrt((4.0 * tolerance) / qa.length());

    for (double t = step; t < 1.0; t += step)
    {
      // Evaluate points (Horner's method)
      BLPoint2 p = qc + t * (qb + (t * qa));
      output.lineTo(p);
    }

    output.lineTo(c0.p2);
  }
};

#endif // BLEND2D_PATH_FLATTEN_P_H_INCLUDED
