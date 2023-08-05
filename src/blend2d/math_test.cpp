// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_test_p.h"
#if defined(BL_TEST)

#include "math_p.h"
#include "support/algorithm_p.h"
#include "support/intops_p.h"
#include "support/traits_p.h"

// BLMath - Tests
// ==============

namespace BLMathTests {

UNIT(math, BL_TEST_GROUP_SUPPORT_UTILITIES) {
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

} // {BLMathTests}

#endif // BL_TEST
