// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PATH_OFFSET_INTERNAL_P_H_INCLUDED
#define BLEND2D_PATH_OFFSET_INTERNAL_P_H_INCLUDED

#include "bezier_p.h"
#include "path-options_p.h"

void insertOuterJoin(
    BLPath2& path,
    BLPoint2 p,
    BLVector2 n0,
    BLVector2 n1,
    double d,
    double ml,
    BLStrokeJoin join)
{
  ml *= fabs(d);

  switch (join)
  {
  case BL_STROKE_JOIN_BEVEL:
  {
    path.lineTo(p + d * n1);

    break;
  }
  case BL_STROKE_JOIN_MITER_BEVEL:
  {
    BLVector2 k = n0 + n1;

    k = 2 * d * k / k.lengthSq();

    if (k.lengthSq() <= ml * ml)
    {
      path.lineTo(p + k);
    }

    path.lineTo(p + d * n1);

    break;
  }
  case BL_STROKE_JOIN_MITER_CLIP:
  {
    BLVector2 k = n0 + n1;

    k = 2 * d * k / k.lengthSq();

    BLPoint2 pp0 = p + d * n0;
    BLPoint2 pp2 = p + d * n1;

    if (k.lengthSq() <= ml * ml)
    {
      // Same as miter join
      path.lineTo(p + k);
    }
    else if (n0.dot(n1) <= COS_ACUTE)
    {
      // Join is too sharp ('k' is approaching infinity)
      path.lineTo(pp0 - ml * n0.normal());
      path.lineTo(pp2 + ml * n1.normal());
    }
    else
    {
      double kov = k.dot(p - pp0);
      double kok = k.dot(k);

      double t = (kov + ml * sqrt(kok)) / (kov + kok);

      // Fall back to bevel otherwise
      if (t > 0.0)
      {
        BLPoint2 pp1 = p + k;

        path.lineTo(pp0.lerp(pp1, t));
        path.lineTo(pp2.lerp(pp1, t));
      }
    }

    path.lineTo(pp2);

    break;
  }
  case BL_STROKE_JOIN_ROUND:
  {
    BLPoint2 pp0 = p + d * n0;
    BLPoint2 pp2 = p + d * n1;

    if (n0.dot(n1) < 0)
    {
      // Obtuse angle (2 segments)
      BLVector2 nm = (pp2 - pp0).unit().normal();

      BLVector2 k = n0 + nm;

      k = 2 * d * k / k.lengthSq();

      BLPoint2 pc1 = p + k;
      BLPoint2 pp1 = p + d * nm;
      BLPoint2 pc2 = pc1.lerp(pp1, 2);

      double w = BLBezierRCurve2::getWeightFromVectors(p, pc1, pp1);

      path.conicTo(pc1, pp1, w);
      path.conicTo(pc2, pp2, w);
    }
    else
    {
      // Acute angle (1 segment)
      BLVector2 k = n0 + n1;

      k = 2 * d * k / k.lengthSq();

      BLPoint2 pc = p + k;

      double w = BLBezierRCurve2::getWeightFromVectors(p, pc, pp2);

      path.conicTo(pc, pp2, w);
    }

    break;
  }
  }
}

void insertInnerJoin(BLPath2& path, BLPoint2 p, BLVector2 n1, double d)
{
  // Go back to the point of the base path to fix some offset artifacts (basically a hack)
  path.lineTo(p);

  // Bevel join
  path.lineTo(p + d * n1);
}

#endif // BLEND2D_PATH_OFFSET_INTERNAL_P_H_INCLUDED
