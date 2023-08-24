// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_VECTOR_P_H_INCLUDED
#define BLEND2D_VECTOR_P_H_INCLUDED

#include <math.h>

struct BLVector2
{
  double x;
  double y;

  BLVector2() noexcept = default;

  BLVector2(double x, double y) noexcept
      : x(x), y(y) {}

  static BLVector2 zero()
  {
    return BLVector2(0, 0);
  }

  static BLVector2 unitX()
  {
    return BLVector2(1, 0);
  }

  static BLVector2 unitY()
  {
    return BLVector2(0, 1);
  }

  double length()
  {
    return sqrt(lengthSq());
  }

  double lengthSq()
  {
    return x * x + y * y;
  }

  bool isZero()
  {
    return x == 0 & y == 0;
  }

  double cross(BLVector2 v)
  {
    return x * v.y - y * v.x;
  }

  double dot(BLVector2 v)
  {
    return x * v.x + y * v.y;
  }

  BLVector2 normal()
  {
    return BLVector2(y, -x);
  }

  BLVector2 unit()
  {
    double len = length();
    return BLVector2(x / len, y / len);
  }
};

static BLVector2 operator+(BLVector2 a, BLVector2 b) noexcept { return BLVector2(a.x + b.x, a.y + b.y); }
static BLVector2 operator-(BLVector2 a, BLVector2 b) noexcept { return BLVector2(a.x - b.x, a.y - b.y); }
static BLVector2 operator-(BLVector2 a) noexcept { return BLVector2(-a.x, -a.y); }
static BLVector2 operator*(double a, BLVector2 b) noexcept { return BLVector2(a * b.x, a * b.y); }
static BLVector2 operator/(BLVector2 a, double b) noexcept { return BLVector2(a.x / b, a.y / b); }

struct BLVector3
{
  double x;
  double y;
  double z;

  BLVector3() noexcept = default;

  BLVector3(double x, double y, double z) noexcept
      : x(x), y(y), z(z) {}
};

static BLVector3 operator+(BLVector3 a, BLVector3 b) noexcept { return BLVector3(a.x + b.x, a.y + b.y, a.y + b.y); }
static BLVector3 operator-(BLVector3 a, BLVector3 b) noexcept { return BLVector3(a.x - b.x, a.y - b.y, a.y - b.y); }
static BLVector3 operator*(double a, BLVector3 b) noexcept { return BLVector3(a * b.x, a * b.y, a * b.z); }
static BLVector3 operator/(BLVector3 a, double b) noexcept { return BLVector3(a.x / b, a.y / b, a.z / b); }

#endif // BLEND2D_VECTOR_P_H_INCLUDED
