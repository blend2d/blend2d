// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PATH_STROKE_P_H_INCLUDED
#define BLEND2D_PATH_STROKE_P_H_INCLUDED

#include "bezier_p.h"
#include "path-options_p.h"
#include "path-simplify_p.h"
#include "path-stroke-internal_p.h"
#include "path2_p.h"

struct BLPathStroke2
{
  double simplifyTolerance;
  double tanOffsetTolerance;
  BLStrokeState state;

  BLPathStroke2(BLPathQualityOptions options) noexcept
      : simplifyTolerance(options.simplifyTolerance),
        tanOffsetTolerance(tan(options.simplifyTolerance)),
        state(BLStrokeState()) {}

  void process(BLPath2 &input, BLPath2 &output, BLPathStrokeOptions options)
  {
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

    BLPathCmd ct0 = BL_PATH_CMD_MOVE;

    BLPoint2 ps = BLPoint2::zero();
    BLPoint2 p0 = BLPoint2::zero();
    BLVector2 m0 = BLVector2::zero();

    state.initialize(output, options);

    while (cIdx < commands.size())
    {
      BLPathCmd cmd = commands[cIdx++];
      BLPathCmd ct1 = cmd;

      switch (ct1)
      {
      case BL_PATH_CMD_MOVE:
      {
        if (!m0.isZero())
        {
          state.finalizeOpen();
        }
        else if (ct0 != BL_PATH_CMD_MOVE && ct0 != BL_PATH_CMD_CLOSE)
        {
          state.finalizePoint(ps);
        }

        ps = points[pIdx++];
        p0 = ps;
        m0 = BLVector2::zero();
        break;
      }
      case BL_PATH_CMD_ON:
      {
        BLPoint2 p1 = points[pIdx++];
        BLBezier1Curve2 c = BLBezier1Curve2(p0, p1);
        BLVector2 m = c.getDerivative();

        if (!m.isZero())
        {
          state.strokeFirstOrJoin(p0, m0, m);
          state.strokeLinear(c, m);

          p0 = c.p1;
          m0 = m;
        }

        break;
      }
      case BL_PATH_CMD_QUAD:
      {
        BLPoint2 p1 = points[pIdx++];
        BLPoint2 p2 = points[pIdx++];
        BLBezier2Curve2 c = BLBezier2Curve2(p0, p1, p2);
        BLVector2 m = c.getTangentStart();

        if (!m.isZero())
        {
          state.strokeFirstOrJoin(p0, m0, m);
          strokeQuadratic(c);

          p0 = c.p2;
          m0 = c.getTangentEnd();
        }

        break;
      }
      case BL_PATH_CMD_CUBIC:
      {
        BLPoint2 p1 = points[pIdx++];
        BLPoint2 p2 = points[pIdx++];
        BLPoint2 p3 = points[pIdx++];
        BLBezier3Curve2 c = BLBezier3Curve2(p0, p1, p2, p3);
        BLVector2 m = c.getTangentStart();

        if (!m.isZero())
        {
          state.strokeFirstOrJoin(p0, m0, m);
          strokeCubic(c);

          p0 = c.p3;
          m0 = c.getTangentEnd();
        }

        break;
      }
      case BL_PATH_CMD_CONIC:
      {
        BLPoint2 p1 = points[pIdx++];
        BLPoint2 p2 = points[pIdx++];
        double w = weights[wIdx++];
        BLBezierRCurve2 c = BLBezierRCurve2(p0, p1, p2, w);
        BLVector2 m = c.getTangentStart();

        if (!m.isZero())
        {
          state.strokeFirstOrJoin(p0, m0, m);
          strokeConic(c);

          p0 = c.p2;
          m0 = c.getTangentEnd();
        }

        break;
      }
      case BL_PATH_CMD_CLOSE:
      {
        BLBezier1Curve2 c = BLBezier1Curve2(p0, ps);
        BLVector2 m = c.getDerivative();

        if (!m.isZero())
        {
          state.strokeFirstOrJoin(p0, m0, m);
          state.strokeLinear(c, m);

          m0 = m;
        }

        if (!m0.isZero())
        {
          state.strokeFirstOrJoin(ps, m0, state.ms);
          state.finalizeClosed();
        }
        else if (ct0 != BL_PATH_CMD_CLOSE)
        {
          state.finalizePoint(ps);
        }

        p0 = ps;
        m0 = BLVector2::zero();
        break;
      }
      }

      ct0 = ct1;
    }

    // Finalize last shape
    if (!m0.isZero())
    {
      state.finalizeOpen();
    }
    else if (ct0 != BL_PATH_CMD_MOVE && ct0 != BL_PATH_CMD_CLOSE)
    {
      state.finalizePoint(ps);
    }
  }

  void strokeConic(BLBezierRCurve2 c0)
  {
    double t = simplifyParameterStepConic(c0, 4.0, simplifyTolerance);
    BLBezierRCurve2 c = c0;

    while (t > 0.0 && t < 1.0)
    {
      BLBezierRCurve2 c1, c2;
      c.splitAt(t, c1, c2);

      BLBezier2Curve2 cc1 = simplifyConic(c1);
      strokeQuadratic(cc1);

      t = t / (1.0 - t);
      c = c2;
    }

    BLBezier2Curve2 cc = simplifyConic(c);
    strokeQuadratic(cc);
  }

  void strokeCubic(BLBezier3Curve2 c0)
  {
    double t = simplifyParameterStepCubic(c0, 54.0, simplifyTolerance);
    BLBezier3Curve2 c = c0;

    while (t < 1.0)
    {
      BLBezier3Curve2 c1, c2;
      c.splitAt(t, c1, c2);

      BLBezier2Curve2 cc1, cc2;
      simplifyCubicContinious(c1, cc1, cc2);
      strokeQuadratic(cc1);
      strokeQuadratic(cc2);

      t = t / (1.0 - t);
      c = c2;
    }

    BLBezier2Curve2 cc1, cc2;
    simplifyCubicContinious(c, cc1, cc2);
    strokeQuadratic(cc1);
    strokeQuadratic(cc2);
  }

  void strokeQuadratic(BLBezier2Curve2 c0)
  {
    double tc, td;
    c0.getOffsetCuspParameter(state.distance, tc, td);

    double t1 = tc - td;
    double t2 = tc + td;

    // Considers `NaN` parameters to be outside
    if (t1 < 1.0 && t2 > 0.0)
    {
      if (isDegenerateQuad(c0))
      {
        // Degenerate case
        state.strokeQuadraticDegenerate(c0.p0, c0.getValueAt(tc), c0.p2);
      }
      else
      {
        // Generic case
        double t0 = 0.0;

        // Start curve
        if (t1 > t0 && t1 < MAX_PARAMETER)
        {
          strokeQuadraticSimplify(c0.splitBefore(t1));

          t0 = t1;
        }

        // Middle curve
        if (t2 > t0 && t2 < MAX_PARAMETER)
        {
          strokeQuadraticSimplify(c0.splitBetween(t0, t2));

          t0 = t2;
        }

        // End curve
        if (t0 > 0.0)
        {
          strokeQuadraticSimplify(c0.splitAfter(t0));
        }
        else
        {
          strokeQuadraticSimplify(c0);
        }
      }
    }
    else
    {
      // Default case (parameters outside of curve)
      strokeQuadraticSimplify(c0);
    }
  }

  void strokeQuadraticSimplify(BLBezier2Curve2 c0)
  {
    double t = simplifyParameterStepQuad(c0, tanOffsetTolerance);
    BLBezier2Curve2 c = c0;

    while (t > 0.0 && t < MAX_PARAMETER)
    {
      BLBezier2Curve2 c1, c2;
      c.splitAt(t, c1, c2);

      state.strokeQuadraticSimple(c1);

      t = simplifyParameterStepQuad(c2, tanOffsetTolerance);
      c = c2;
    }

    state.strokeQuadraticSimple(c);
  }
};

#endif // BLEND2D_PATH_STROKE_P_H_INCLUDED
