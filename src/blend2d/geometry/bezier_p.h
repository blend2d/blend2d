// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_BEZIER_P_H_INCLUDED
#define BLEND2D_BEZIER_P_H_INCLUDED

#include "point_p.h"

struct BLBezier1Curve2
{
  BLPoint2 p0;
  BLPoint2 p1;

  BLBezier1Curve2() noexcept = default;

  BLBezier1Curve2(BLPoint2 p0, BLPoint2 p1) noexcept
      : p0(p0), p1(p1) {}

  BLVector2 getDerivative()
  {
    return p1 - p0;
  }

  BLVector2 getTangentEnd()
  {
    return getDerivative();
  }

  BLVector2 getTangentStart()
  {
    return getDerivative();
  }

  BLBezier1Curve2 splitAfter(double t)
  {
    BLPoint2 p01 = p0.lerp(p1, t);

    return BLBezier1Curve2(p01, p1);
  }

  BLBezier1Curve2 splitBefore(double t)
  {
    BLPoint2 p01 = p0.lerp(p1, t);

    return BLBezier1Curve2(p0, p01);
  }
};

struct BLBezier2Curve2
{
  BLPoint2 p0;
  BLPoint2 p1;
  BLPoint2 p2;

  BLBezier2Curve2() noexcept = default;

  BLBezier2Curve2(BLPoint2 p0, BLPoint2 p1, BLPoint2 p2) noexcept
      : p0(p0), p1(p1), p2(p2) {}

  void getCoefficients(BLVector2 &qa, BLVector2 &qb, BLPoint2 &qc)
  {
    BLVector2 v1 = p1 - p0;
    BLVector2 v2 = p2 - p1;

    qa = v2 - v1;
    qb = v1 + v1;
    qc = p0;
  }

  void getDerivativeCoefficients(BLVector2 &qqa, BLVector2 &qqb)
  {
    BLVector2 v1 = p1 - p0;
    BLVector2 v2 = p2 - p1;

    qqa = 2 * (v2 - v1);
    qqb = v1 + v1;
  }

  BLPoint2 getValueAt(double t)
  {
    BLPoint2 p01 = p0.lerp(p1, t);
    BLPoint2 p12 = p1.lerp(p2, t);

    return p01.lerp(p12, t);
  }

  void getOffsetCuspParameter(double rad, double &tc, double &td)
  {
    BLVector2 qqa, qqb;
    getDerivativeCoefficients(qqa, qqb);

    double alen2 = qqa.lengthSq();
    double blen2 = qqb.lengthSq();
    double axb = qqa.cross(qqb);
    double aob = qqa.dot(qqb);
    double fac = 1.0 / alen2;

    tc = fac * -aob;
    td = 0.0;

    if (axb != 0.0)
    {
      double cbr = cbrt(rad * rad * axb * axb);
      double sqr = sqrt(aob * aob - alen2 * (blen2 - cbr));

      td = fac * sqr;
    }
  }

  void splitAt(double t, BLBezier2Curve2 &c1, BLBezier2Curve2 &c2)
  {
    BLPoint2 p01 = p0.lerp(p1, t);
    BLPoint2 p12 = p1.lerp(p2, t);

    BLPoint2 p012 = p01.lerp(p12, t);

    c1 = BLBezier2Curve2(p0, p01, p012);
    c2 = BLBezier2Curve2(p012, p12, p2);
  }

  BLBezier2Curve2 splitAfter(double t)
  {
    BLPoint2 p01 = p0.lerp(p1, t);
    BLPoint2 p12 = p1.lerp(p2, t);

    BLPoint2 p012 = p01.lerp(p12, t);

    return BLBezier2Curve2(p012, p12, p2);
  }

  BLBezier2Curve2 splitBefore(double t)
  {
    BLPoint2 p01 = p0.lerp(p1, t);
    BLPoint2 p12 = p1.lerp(p2, t);

    BLPoint2 p012 = p01.lerp(p12, t);

    return BLBezier2Curve2(p0, p01, p012);
  }

  BLBezier2Curve2 splitBetween(double t0, double t1)
  {
    // Blossoming (Curves and Surfaces for CAGD by Gerald Farin)
    BLPoint2 t0p0 = p0.lerp(p1, t0);
    BLPoint2 t1p1 = p0.lerp(p1, t1);
    BLPoint2 t0p1 = p1.lerp(p2, t0);
    BLPoint2 t1p2 = p1.lerp(p2, t1);

    BLPoint2 tp0 = t0p0.lerp(t0p1, t0);
    BLPoint2 tp1 = t1p1.lerp(t1p2, t0);
    BLPoint2 tp2 = t1p1.lerp(t1p2, t1);

    return BLBezier2Curve2(tp0, tp1, tp2);
  }

  BLVector2 getTangentEnd()
  {
    if (p2 != p1)
    {
      return p2 - p1;
    }
    else
    {
      return p1 - p0;
    }
  }

  BLVector2 getTangentStart()
  {
    if (p1 != p0)
    {
      return p1 - p0;
    }
    else
    {
      return p2 - p1;
    }
  }
};

struct BLBezier3Curve2
{
  BLPoint2 p0;
  BLPoint2 p1;
  BLPoint2 p2;
  BLPoint2 p3;

  BLBezier3Curve2() noexcept = default;

  BLBezier3Curve2(BLPoint2 p0, BLPoint2 p1, BLPoint2 p2, BLPoint2 p3) noexcept
      : p0(p0), p1(p1), p2(p2), p3(p3) {}

  void splitAt(double t, BLBezier3Curve2 &c1, BLBezier3Curve2 &c2)
  {
    BLPoint2 p01 = p0.lerp(p1, t);
    BLPoint2 p12 = p1.lerp(p2, t);
    BLPoint2 p23 = p2.lerp(p3, t);

    BLPoint2 p012 = p01.lerp(p12, t);
    BLPoint2 p123 = p12.lerp(p23, t);

    BLPoint2 p0123 = p012.lerp(p123, t);

    c1 = BLBezier3Curve2(p0, p01, p012, p0123);
    c2 = BLBezier3Curve2(p0123, p123, p23, p3);
  }

  BLPoint2 getValueAt(double t)
  {
    BLPoint2 p01 = p0.lerp(p1, t);
    BLPoint2 p12 = p1.lerp(p2, t);
    BLPoint2 p23 = p2.lerp(p3, t);

    BLPoint2 p012 = p01.lerp(p12, t);
    BLPoint2 p123 = p12.lerp(p23, t);

    return p012.lerp(p123, t);
  }

  BLVector2 getTangentEnd()
  {
    if (p3 != p2)
    {
      return p3 - p2;
    }
    else if (p2 != p1)
    {
      return p2 - p1;
    }
    else
    {
      return p1 - p0;
    }
  }

  BLVector2 getTangentStart()
  {
    if (p1 != p0)
    {
      return p1 - p0;
    }
    else if (p2 != p1)
    {
      return p2 - p1;
    }
    else
    {
      return p3 - p2;
    }
  }
};

struct BLBezierRCurve2
{
  BLPoint2 p0;
  BLPoint2 p1;
  BLPoint2 p2;
  double w;

  BLBezierRCurve2() noexcept = default;

  BLBezierRCurve2(BLPoint2 p0, BLPoint2 p1, BLPoint2 p2, double w) noexcept
      : p0(p0), p1(p1), p2(p2), w(w) {}

  static BLBezierRCurve2 fromProjectivePoints(BLPoint3 p0, BLPoint3 p1, BLPoint3 p2)
  {
    BLPoint2 pp0 = BLPoint2::fromXYW(p0.x, p0.y, p0.z);
    BLPoint2 pp1 = BLPoint2::fromXYW(p1.x, p1.y, p1.z);
    BLPoint2 pp2 = BLPoint2::fromXYW(p2.x, p2.y, p2.z);
    double w = getNormalizedWeight(p0.z, p1.z, p2.z);
    return BLBezierRCurve2(pp0, pp1, pp2, w);
  }

  static double getNormalizedWeight(double w0, double w1, double w2)
  {
    return w1 / sqrt(w0 * w2);
  }

  static double getWeightFromVectors(BLPoint2 pc, BLPoint2 p1, BLPoint2 p2)
  {
    BLVector2 v1 = p1 - pc;
    BLVector2 v2 = p2 - pc;

    return v1.dot(v2) / sqrt(v1.lengthSq() * v2.lengthSq());
  }

  BLVector2 getTangentEnd()
  {
    if (p2 != p1)
    {
      return p2 - p1;
    }
    else
    {
      return p1 - p0;
    }
  }

  BLVector2 getTangentStart()
  {
    if (p1 != p0)
    {
      return p1 - p0;
    }
    else
    {
      return p2 - p1;
    }
  }

  void splitAt(double t, BLBezierRCurve2 &c1, BLBezierRCurve2 &c2)
  {
    BLPoint3 p0, p1, p2;
    getProjectivePoints(p0, p1, p2);

    BLPoint3 p01 = p0.lerp(p1, t);
    BLPoint3 p12 = p1.lerp(p2, t);

    BLPoint3 p012 = p01.lerp(p12, t);

    c1 = BLBezierRCurve2::fromProjectivePoints(p0, p01, p012);
    c2 = BLBezierRCurve2::fromProjectivePoints(p012, p12, p2);
  }

  void getProjectivePoints(BLPoint3 &pp0, BLPoint3 &pp1, BLPoint3 &pp2)
  {
    pp0 = BLPoint3::fromXY(p0.x, p0.y);
    pp1 = BLPoint3::fromXYW(p1.x, p1.y, w);
    pp2 = BLPoint3::fromXY(p2.x, p2.y);
  }
};

#endif // BLEND2D_BEZIER_P_H_INCLUDED
