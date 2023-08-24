// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SCALAR_P_H_INCLUDED
#define BLEND2D_SCALAR_P_H_INCLUDED

double lerp(double x0, double x1, double t)
{
  return x0 + t * (x1 - x0);
}

#endif // BLEND2D_SCALAR_P_H_INCLUDED
