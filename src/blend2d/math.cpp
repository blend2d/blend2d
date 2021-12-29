// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "math_p.h"
#include "support/algorithm_p.h"
#include "support/intops_p.h"
#include "support/traits_p.h"

// BLMath - Cubic Roots
// ====================

// Ax^3 + Bx^2 + Cx + D = 0.
//
// Roots3And4.c: Graphics Gems, original author Jochen Schwarze (schwarze@isa.de). See also the wiki article
// at <http://en.wikipedia.org/wiki/Cubic_function> for other equations.
size_t blCubicRoots(double* dst, const double* poly, double tMin, double tMax) noexcept {
  constexpr double k1Div3 = 1.0 / 3.0;
  constexpr double k1Div6 = 1.0 / 6.0;
  constexpr double k1Div9 = 1.0 / 9.0;
  constexpr double k1Div27 = 1.0 / 27.0;

  size_t nRoots = 0;
  double norm = poly[0];
  double a = poly[1];
  double b = poly[2];
  double c = poly[3];

  if (norm == 0.0)
    return blQuadRoots(dst, a, b, c, tMin, tMax);

  // Convert to a normalized form `x^3 + Ax^2 + Bx + C == 0`.
  a /= norm;
  b /= norm;
  c /= norm;

  // Substitute x = y - A/3 to eliminate quadric term `x^3 + px + q = 0`.
  double sa = a * a;
  double p = -k1Div9  * sa + k1Div3 * b;
  double q = (k1Div27 * sa - k1Div6 * b) * a + 0.5 * c;

  // Use Cardano's formula.
  double p3 = p * p * p;
  double d  = q * q + p3;

  // Resubstitution constant.
  double sub = -k1Div3 * a;

  if (isNearZero(d)) {
    // One triple solution.
    if (isNearZero(q)) {
      dst[0] = sub;
      return size_t(sub >= tMin && sub <= tMax);
    }

    // One single and one double solution.
    double u = blCbrt(-q);
    nRoots = 2;

    dst[0] = sub + 2.0 * u;
    dst[1] = sub - u;

    // Sort.
    if (dst[0] > dst[1])
      std::swap(dst[0], dst[1]);
  }
  else if (d < 0.0) {
    // Three real solutions.
    double phi = k1Div3 * blAcos(-q / blSqrt(-p3));
    double t = 2.0 * blSqrt(-p);

    nRoots = 3;
    dst[0] = sub + t * blCos(phi);
    dst[1] = sub - t * blCos(phi + BL_M_PI_DIV_3);
    dst[2] = sub - t * blCos(phi - BL_M_PI_DIV_3);

    // Sort.
    if (dst[0] > dst[1]) std::swap(dst[0], dst[1]);
    if (dst[1] > dst[2]) std::swap(dst[1], dst[2]);
    if (dst[0] > dst[1]) std::swap(dst[0], dst[1]);
  }
  else {
    // One real solution.
    double sqrt_d = blSqrt(d);
    double u =  blCbrt(sqrt_d - q);
    double v = -blCbrt(sqrt_d + q);

    nRoots = 1;
    dst[0] = sub + u + v;
  }

  size_t n = 0;
  for (size_t i = 0; i < nRoots; i++)
    if (dst[i] >= tMin && dst[i] <= tMax)
      dst[n++] = dst[i];
  return n;
}

// BLMath - Tests
// ==============

#ifdef BL_TEST
UNIT(math, -9) {
  INFO("blFloor()");
  {
    EXPECT_EQ(blFloor(-1.5f),-2.0f);
    EXPECT_EQ(blFloor(-1.5 ),-2.0 );
    EXPECT_EQ(blFloor(-0.9f),-1.0f);
    EXPECT_EQ(blFloor(-0.9 ),-1.0 );
    EXPECT_EQ(blFloor(-0.5f),-1.0f);
    EXPECT_EQ(blFloor(-0.5 ),-1.0 );
    EXPECT_EQ(blFloor(-0.1f),-1.0f);
    EXPECT_EQ(blFloor(-0.1 ),-1.0 );
    EXPECT_EQ(blFloor( 0.0f), 0.0f);
    EXPECT_EQ(blFloor( 0.0 ), 0.0 );
    EXPECT_EQ(blFloor( 0.1f), 0.0f);
    EXPECT_EQ(blFloor( 0.1 ), 0.0 );
    EXPECT_EQ(blFloor( 0.5f), 0.0f);
    EXPECT_EQ(blFloor( 0.5 ), 0.0 );
    EXPECT_EQ(blFloor( 0.9f), 0.0f);
    EXPECT_EQ(blFloor( 0.9 ), 0.0 );
    EXPECT_EQ(blFloor( 1.5f), 1.0f);
    EXPECT_EQ(blFloor( 1.5 ), 1.0 );
    EXPECT_EQ(blFloor(-4503599627370496.0), -4503599627370496.0);
    EXPECT_EQ(blFloor( 4503599627370496.0),  4503599627370496.0);
  }

  INFO("blCeil()");
  {
    EXPECT_EQ(blCeil(-1.5f),-1.0f);
    EXPECT_EQ(blCeil(-1.5 ),-1.0 );
    EXPECT_EQ(blCeil(-0.9f), 0.0f);
    EXPECT_EQ(blCeil(-0.9 ), 0.0 );
    EXPECT_EQ(blCeil(-0.5f), 0.0f);
    EXPECT_EQ(blCeil(-0.5 ), 0.0 );
    EXPECT_EQ(blCeil(-0.1f), 0.0f);
    EXPECT_EQ(blCeil(-0.1 ), 0.0 );
    EXPECT_EQ(blCeil( 0.0f), 0.0f);
    EXPECT_EQ(blCeil( 0.0 ), 0.0 );
    EXPECT_EQ(blCeil( 0.1f), 1.0f);
    EXPECT_EQ(blCeil( 0.1 ), 1.0 );
    EXPECT_EQ(blCeil( 0.5f), 1.0f);
    EXPECT_EQ(blCeil( 0.5 ), 1.0 );
    EXPECT_EQ(blCeil( 0.9f), 1.0f);
    EXPECT_EQ(blCeil( 0.9 ), 1.0 );
    EXPECT_EQ(blCeil( 1.5f), 2.0f);
    EXPECT_EQ(blCeil( 1.5 ), 2.0 );
    EXPECT_EQ(blCeil(-4503599627370496.0), -4503599627370496.0);
    EXPECT_EQ(blCeil( 4503599627370496.0),  4503599627370496.0);
  }

  INFO("blTrunc()");
  {
    EXPECT_EQ(blTrunc(-1.5f),-1.0f);
    EXPECT_EQ(blTrunc(-1.5 ),-1.0 );
    EXPECT_EQ(blTrunc(-0.9f), 0.0f);
    EXPECT_EQ(blTrunc(-0.9 ), 0.0 );
    EXPECT_EQ(blTrunc(-0.5f), 0.0f);
    EXPECT_EQ(blTrunc(-0.5 ), 0.0 );
    EXPECT_EQ(blTrunc(-0.1f), 0.0f);
    EXPECT_EQ(blTrunc(-0.1 ), 0.0 );
    EXPECT_EQ(blTrunc( 0.0f), 0.0f);
    EXPECT_EQ(blTrunc( 0.0 ), 0.0 );
    EXPECT_EQ(blTrunc( 0.1f), 0.0f);
    EXPECT_EQ(blTrunc( 0.1 ), 0.0 );
    EXPECT_EQ(blTrunc( 0.5f), 0.0f);
    EXPECT_EQ(blTrunc( 0.5 ), 0.0 );
    EXPECT_EQ(blTrunc( 0.9f), 0.0f);
    EXPECT_EQ(blTrunc( 0.9 ), 0.0 );
    EXPECT_EQ(blTrunc( 1.5f), 1.0f);
    EXPECT_EQ(blTrunc( 1.5 ), 1.0 );
    EXPECT_EQ(blTrunc(-4503599627370496.0), -4503599627370496.0);
    EXPECT_EQ(blTrunc( 4503599627370496.0),  4503599627370496.0);
  }

  INFO("blRound()");
  {
    EXPECT_EQ(blRound(-1.5f),-1.0f);
    EXPECT_EQ(blRound(-1.5 ),-1.0 );
    EXPECT_EQ(blRound(-0.9f),-1.0f);
    EXPECT_EQ(blRound(-0.9 ),-1.0 );
    EXPECT_EQ(blRound(-0.5f), 0.0f);
    EXPECT_EQ(blRound(-0.5 ), 0.0 );
    EXPECT_EQ(blRound(-0.1f), 0.0f);
    EXPECT_EQ(blRound(-0.1 ), 0.0 );
    EXPECT_EQ(blRound( 0.0f), 0.0f);
    EXPECT_EQ(blRound( 0.0 ), 0.0 );
    EXPECT_EQ(blRound( 0.1f), 0.0f);
    EXPECT_EQ(blRound( 0.1 ), 0.0 );
    EXPECT_EQ(blRound( 0.5f), 1.0f);
    EXPECT_EQ(blRound( 0.5 ), 1.0 );
    EXPECT_EQ(blRound( 0.9f), 1.0f);
    EXPECT_EQ(blRound( 0.9 ), 1.0 );
    EXPECT_EQ(blRound( 1.5f), 2.0f);
    EXPECT_EQ(blRound( 1.5 ), 2.0 );
    EXPECT_EQ(blRound(-4503599627370496.0), -4503599627370496.0);
    EXPECT_EQ(blRound( 4503599627370496.0),  4503599627370496.0);
  }

  INFO("blFloorToInt()");
  {
    EXPECT_EQ(blFloorToInt(-1.5f),-2);
    EXPECT_EQ(blFloorToInt(-1.5 ),-2);
    EXPECT_EQ(blFloorToInt(-0.9f),-1);
    EXPECT_EQ(blFloorToInt(-0.9 ),-1);
    EXPECT_EQ(blFloorToInt(-0.5f),-1);
    EXPECT_EQ(blFloorToInt(-0.5 ),-1);
    EXPECT_EQ(blFloorToInt(-0.1f),-1);
    EXPECT_EQ(blFloorToInt(-0.1 ),-1);
    EXPECT_EQ(blFloorToInt( 0.0f), 0);
    EXPECT_EQ(blFloorToInt( 0.0 ), 0);
    EXPECT_EQ(blFloorToInt( 0.1f), 0);
    EXPECT_EQ(blFloorToInt( 0.1 ), 0);
    EXPECT_EQ(blFloorToInt( 0.5f), 0);
    EXPECT_EQ(blFloorToInt( 0.5 ), 0);
    EXPECT_EQ(blFloorToInt( 0.9f), 0);
    EXPECT_EQ(blFloorToInt( 0.9 ), 0);
    EXPECT_EQ(blFloorToInt( 1.5f), 1);
    EXPECT_EQ(blFloorToInt( 1.5 ), 1);
  }

  INFO("blCeilToInt()");
  {
    EXPECT_EQ(blCeilToInt(-1.5f),-1);
    EXPECT_EQ(blCeilToInt(-1.5 ),-1);
    EXPECT_EQ(blCeilToInt(-0.9f), 0);
    EXPECT_EQ(blCeilToInt(-0.9 ), 0);
    EXPECT_EQ(blCeilToInt(-0.5f), 0);
    EXPECT_EQ(blCeilToInt(-0.5 ), 0);
    EXPECT_EQ(blCeilToInt(-0.1f), 0);
    EXPECT_EQ(blCeilToInt(-0.1 ), 0);
    EXPECT_EQ(blCeilToInt( 0.0f), 0);
    EXPECT_EQ(blCeilToInt( 0.0 ), 0);
    EXPECT_EQ(blCeilToInt( 0.1f), 1);
    EXPECT_EQ(blCeilToInt( 0.1 ), 1);
    EXPECT_EQ(blCeilToInt( 0.5f), 1);
    EXPECT_EQ(blCeilToInt( 0.5 ), 1);
    EXPECT_EQ(blCeilToInt( 0.9f), 1);
    EXPECT_EQ(blCeilToInt( 0.9 ), 1);
    EXPECT_EQ(blCeilToInt( 1.5f), 2);
    EXPECT_EQ(blCeilToInt( 1.5 ), 2);
  }

  INFO("blTruncToInt()");
  {
    EXPECT_EQ(blTruncToInt(-1.5f),-1);
    EXPECT_EQ(blTruncToInt(-1.5 ),-1);
    EXPECT_EQ(blTruncToInt(-0.9f), 0);
    EXPECT_EQ(blTruncToInt(-0.9 ), 0);
    EXPECT_EQ(blTruncToInt(-0.5f), 0);
    EXPECT_EQ(blTruncToInt(-0.5 ), 0);
    EXPECT_EQ(blTruncToInt(-0.1f), 0);
    EXPECT_EQ(blTruncToInt(-0.1 ), 0);
    EXPECT_EQ(blTruncToInt( 0.0f), 0);
    EXPECT_EQ(blTruncToInt( 0.0 ), 0);
    EXPECT_EQ(blTruncToInt( 0.1f), 0);
    EXPECT_EQ(blTruncToInt( 0.1 ), 0);
    EXPECT_EQ(blTruncToInt( 0.5f), 0);
    EXPECT_EQ(blTruncToInt( 0.5 ), 0);
    EXPECT_EQ(blTruncToInt( 0.9f), 0);
    EXPECT_EQ(blTruncToInt( 0.9 ), 0);
    EXPECT_EQ(blTruncToInt( 1.5f), 1);
    EXPECT_EQ(blTruncToInt( 1.5 ), 1);
  }

  INFO("blRoundToInt()");
  {
    EXPECT_EQ(blRoundToInt(-1.5f),-1);
    EXPECT_EQ(blRoundToInt(-1.5 ),-1);
    EXPECT_EQ(blRoundToInt(-0.9f),-1);
    EXPECT_EQ(blRoundToInt(-0.9 ),-1);
    EXPECT_EQ(blRoundToInt(-0.5f), 0);
    EXPECT_EQ(blRoundToInt(-0.5 ), 0);
    EXPECT_EQ(blRoundToInt(-0.1f), 0);
    EXPECT_EQ(blRoundToInt(-0.1 ), 0);
    EXPECT_EQ(blRoundToInt( 0.0f), 0);
    EXPECT_EQ(blRoundToInt( 0.0 ), 0);
    EXPECT_EQ(blRoundToInt( 0.1f), 0);
    EXPECT_EQ(blRoundToInt( 0.1 ), 0);
    EXPECT_EQ(blRoundToInt( 0.5f), 1);
    EXPECT_EQ(blRoundToInt( 0.5 ), 1);
    EXPECT_EQ(blRoundToInt( 0.9f), 1);
    EXPECT_EQ(blRoundToInt( 0.9 ), 1);
    EXPECT_EQ(blRoundToInt( 1.5f), 2);
    EXPECT_EQ(blRoundToInt( 1.5 ), 2);
  }

  INFO("blFrac()");
  {
    EXPECT_EQ(blFrac( 0.00f), 0.00f);
    EXPECT_EQ(blFrac( 0.00 ), 0.00 );
    EXPECT_EQ(blFrac( 1.00f), 0.00f);
    EXPECT_EQ(blFrac( 1.00 ), 0.00 );
    EXPECT_EQ(blFrac( 1.25f), 0.25f);
    EXPECT_EQ(blFrac( 1.25 ), 0.25 );
    EXPECT_EQ(blFrac( 1.75f), 0.75f);
    EXPECT_EQ(blFrac( 1.75 ), 0.75 );
    EXPECT_EQ(blFrac(-1.00f), 0.00f);
    EXPECT_EQ(blFrac(-1.00 ), 0.00 );
    EXPECT_EQ(blFrac(-1.25f), 0.75f);
    EXPECT_EQ(blFrac(-1.25 ), 0.75 );
    EXPECT_EQ(blFrac(-1.75f), 0.25f);
    EXPECT_EQ(blFrac(-1.75 ), 0.25 );
  }

  INFO("blIsBetween0And1()");
  {
    EXPECT_TRUE(blIsBetween0And1( 0.0f  ));
    EXPECT_TRUE(blIsBetween0And1( 0.0   ));
    EXPECT_TRUE(blIsBetween0And1( 0.5f  ));
    EXPECT_TRUE(blIsBetween0And1( 0.5   ));
    EXPECT_TRUE(blIsBetween0And1( 1.0f  ));
    EXPECT_TRUE(blIsBetween0And1( 1.0   ));
    EXPECT_TRUE(blIsBetween0And1(-0.0f  ));
    EXPECT_TRUE(blIsBetween0And1(-0.0   ));
    EXPECT_FALSE(blIsBetween0And1(-1.0f  ));
    EXPECT_FALSE(blIsBetween0And1(-1.0   ));
    EXPECT_FALSE(blIsBetween0And1( 1.001f));
    EXPECT_FALSE(blIsBetween0And1( 1.001 ));
  }

  INFO("blQuadRoots");
  {
    size_t count;
    double roots[2];

    // x^2 + 4x + 4 == 0
    count = blQuadRoots(roots, 1.0, 4.0, 4.0, BLTraits::minValue<double>(), BLTraits::maxValue<double>());

    EXPECT_EQ(count, 1u);
    EXPECT_EQ(roots[0], -2.0);

    // -4x^2 + 8x + 12 == 0
    count = blQuadRoots(roots, -4.0, 8.0, 12.0, BLTraits::minValue<double>(), BLTraits::maxValue<double>());

    EXPECT_EQ(count, 2u);
    EXPECT_EQ(roots[0], -1.0);
    EXPECT_EQ(roots[1],  3.0);
  }

  INFO("blCubicRoots");
  {
    size_t count;
    double roots[2];

    // 64x^3 - 64 == 1
    count = blCubicRoots(roots, 64, 0.0, 0.0, -64, BLTraits::minValue<double>(), BLTraits::maxValue<double>());

    EXPECT_EQ(count, 1u);
    EXPECT_EQ(roots[0], 1.0);
  }
}
#endif
