// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PATH_OPTIONS_P_H_INCLUDED
#define BLEND2D_PATH_OPTIONS_P_H_INCLUDED

#include "bezier_p.h"
#include "path2_p.h"

// Threshold for acute (179.9999 degrees) and obtuse (0.0001 degrees) angles.
const double COS_ACUTE = -0.99999999999847689;
const double COS_OBTUSE = 0.99999999999847689;

// Threshold for curve splitting (to avoid tiny tail curves).
const double MIN_PARAMETER = 5e-7;
const double MAX_PARAMETER = 1 - MIN_PARAMETER;

struct BLStrokeCaps
{
  BLStrokeCap start;
  BLStrokeCap end;
};

struct BLPathStrokeOptions
{
  BLStrokeCaps caps;
  BLArray<double> dashArray;
  BLStrokeCaps dashCaps;
  double dashOffset;
  BLStrokeJoin join;
  double miterLimit;
  double width;
};

struct BLPathQualityOptions
{
  double flattenTolerance;
  double simplifyTolerance;
  double offsetTolerance;
};

#endif // BLEND2D_PATH_OPTIONS_P_H_INCLUDED
