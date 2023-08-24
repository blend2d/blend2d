// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_POINT_P_H_INCLUDED
#define BLEND2D_POINT_P_H_INCLUDED

#include "scalar_p.h"
#include "vector_p.h"

struct BLPoint2
{
  double x;
  double y;

  BLPoint2() noexcept = default;

  BLPoint2(double x, double y) noexcept
      : x(x), y(y) {}

  static BLPoint2 fromXYW(double x, double y, double w)
  {
    return BLPoint2(x / w, y / w);
  }

  static BLPoint2 zero()
  {
    return BLPoint2(0, 0);
  }

  BLPoint2 lerp(BLPoint2 p1, double t)
  {
    double x = ::lerp(this->x, p1.x, t);
    double y = ::lerp(this->y, p1.y, t);
    return BLPoint2(x, y);
  }
};

static bool operator==(BLPoint2 a, BLPoint2 b) noexcept { return (a.x == b.x) & (a.y == b.y); }
static bool operator!=(BLPoint2 a, BLPoint2 b) noexcept { return (a.x != b.x) | (a.y != b.y); }
static BLPoint2 operator+(BLPoint2 a, BLVector2 b) noexcept { return BLPoint2(a.x + b.x, a.y + b.y); }
static BLPoint2 operator-(BLPoint2 a, BLVector2 b) noexcept { return BLPoint2(a.x - b.x, a.y - b.y); }
static BLVector2 operator-(BLPoint2 a, BLPoint2 b) noexcept { return BLVector2(a.x - b.x, a.y - b.y); }

struct BLPoint3
{
  double x;
  double y;
  double z;

  BLPoint3() noexcept = default;

  BLPoint3(double x, double y, double z) noexcept
      : x(x), y(y), z(z) {}

  static BLPoint3 fromXY(double x, double y)
  {
    return BLPoint3(x, y, 1.0);
  }

  static BLPoint3 fromXYW(double x, double y, double w)
  {
    return BLPoint3(w * x, w * y, w);
  }

  BLPoint3 lerp(BLPoint3 p1, double t)
  {
    double x = ::lerp(this->x, p1.x, t);
    double y = ::lerp(this->y, p1.y, t);
    double z = ::lerp(this->z, p1.z, t);
    return BLPoint3(x, y, z);
  }
};

static bool operator==(BLPoint3 a, BLPoint3 b) noexcept { return (a.x == b.x) & (a.y == b.y) & (a.z == b.z); }
static bool operator!=(BLPoint3 a, BLPoint3 b) noexcept { return (a.x != b.x) | (a.y != b.y) | (a.z != b.z); }
static BLPoint3 operator+(BLPoint3 a, BLVector3 b) noexcept { return BLPoint3(a.x + b.x, a.y + b.y, a.y + b.y); }
static BLVector3 operator-(BLPoint3 a, BLPoint3 b) noexcept { return BLVector3(a.x - b.x, a.y - b.y, a.y - b.y); }

#endif // BLEND2D_POINT_P_H_INCLUDED
