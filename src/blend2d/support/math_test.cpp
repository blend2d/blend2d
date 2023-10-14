// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_test_p.h"
#if defined(BL_TEST)

#include "../support/algorithm_p.h"
#include "../support/intops_p.h"
#include "../support/math_p.h"
#include "../support/traits_p.h"

// bl::Math - Tests
// ================

namespace bl {
namespace Tests {

UNIT(math, BL_TEST_GROUP_SUPPORT_UTILITIES) {
  INFO("bl::Math::floor()");
  {
    EXPECT_EQ(Math::floor(-1.5f),-2.0f);
    EXPECT_EQ(Math::floor(-1.5 ),-2.0 );
    EXPECT_EQ(Math::floor(-0.9f),-1.0f);
    EXPECT_EQ(Math::floor(-0.9 ),-1.0 );
    EXPECT_EQ(Math::floor(-0.5f),-1.0f);
    EXPECT_EQ(Math::floor(-0.5 ),-1.0 );
    EXPECT_EQ(Math::floor(-0.1f),-1.0f);
    EXPECT_EQ(Math::floor(-0.1 ),-1.0 );
    EXPECT_EQ(Math::floor( 0.0f), 0.0f);
    EXPECT_EQ(Math::floor( 0.0 ), 0.0 );
    EXPECT_EQ(Math::floor( 0.1f), 0.0f);
    EXPECT_EQ(Math::floor( 0.1 ), 0.0 );
    EXPECT_EQ(Math::floor( 0.5f), 0.0f);
    EXPECT_EQ(Math::floor( 0.5 ), 0.0 );
    EXPECT_EQ(Math::floor( 0.9f), 0.0f);
    EXPECT_EQ(Math::floor( 0.9 ), 0.0 );
    EXPECT_EQ(Math::floor( 1.5f), 1.0f);
    EXPECT_EQ(Math::floor( 1.5 ), 1.0 );
    EXPECT_EQ(Math::floor(-4503599627370496.0), -4503599627370496.0);
    EXPECT_EQ(Math::floor( 4503599627370496.0),  4503599627370496.0);
  }

  INFO("bl::Math::ceil()");
  {
    EXPECT_EQ(Math::ceil(-1.5f),-1.0f);
    EXPECT_EQ(Math::ceil(-1.5 ),-1.0 );
    EXPECT_EQ(Math::ceil(-0.9f), 0.0f);
    EXPECT_EQ(Math::ceil(-0.9 ), 0.0 );
    EXPECT_EQ(Math::ceil(-0.5f), 0.0f);
    EXPECT_EQ(Math::ceil(-0.5 ), 0.0 );
    EXPECT_EQ(Math::ceil(-0.1f), 0.0f);
    EXPECT_EQ(Math::ceil(-0.1 ), 0.0 );
    EXPECT_EQ(Math::ceil( 0.0f), 0.0f);
    EXPECT_EQ(Math::ceil( 0.0 ), 0.0 );
    EXPECT_EQ(Math::ceil( 0.1f), 1.0f);
    EXPECT_EQ(Math::ceil( 0.1 ), 1.0 );
    EXPECT_EQ(Math::ceil( 0.5f), 1.0f);
    EXPECT_EQ(Math::ceil( 0.5 ), 1.0 );
    EXPECT_EQ(Math::ceil( 0.9f), 1.0f);
    EXPECT_EQ(Math::ceil( 0.9 ), 1.0 );
    EXPECT_EQ(Math::ceil( 1.5f), 2.0f);
    EXPECT_EQ(Math::ceil( 1.5 ), 2.0 );
    EXPECT_EQ(Math::ceil(-4503599627370496.0), -4503599627370496.0);
    EXPECT_EQ(Math::ceil( 4503599627370496.0),  4503599627370496.0);
  }

  INFO("bl::Math::trunc()");
  {
    EXPECT_EQ(Math::trunc(-1.5f),-1.0f);
    EXPECT_EQ(Math::trunc(-1.5 ),-1.0 );
    EXPECT_EQ(Math::trunc(-0.9f), 0.0f);
    EXPECT_EQ(Math::trunc(-0.9 ), 0.0 );
    EXPECT_EQ(Math::trunc(-0.5f), 0.0f);
    EXPECT_EQ(Math::trunc(-0.5 ), 0.0 );
    EXPECT_EQ(Math::trunc(-0.1f), 0.0f);
    EXPECT_EQ(Math::trunc(-0.1 ), 0.0 );
    EXPECT_EQ(Math::trunc( 0.0f), 0.0f);
    EXPECT_EQ(Math::trunc( 0.0 ), 0.0 );
    EXPECT_EQ(Math::trunc( 0.1f), 0.0f);
    EXPECT_EQ(Math::trunc( 0.1 ), 0.0 );
    EXPECT_EQ(Math::trunc( 0.5f), 0.0f);
    EXPECT_EQ(Math::trunc( 0.5 ), 0.0 );
    EXPECT_EQ(Math::trunc( 0.9f), 0.0f);
    EXPECT_EQ(Math::trunc( 0.9 ), 0.0 );
    EXPECT_EQ(Math::trunc( 1.5f), 1.0f);
    EXPECT_EQ(Math::trunc( 1.5 ), 1.0 );
    EXPECT_EQ(Math::trunc(-4503599627370496.0), -4503599627370496.0);
    EXPECT_EQ(Math::trunc( 4503599627370496.0),  4503599627370496.0);
  }

  INFO("bl::Math::round()");
  {
    EXPECT_EQ(Math::round(-1.5f),-1.0f);
    EXPECT_EQ(Math::round(-1.5 ),-1.0 );
    EXPECT_EQ(Math::round(-0.9f),-1.0f);
    EXPECT_EQ(Math::round(-0.9 ),-1.0 );
    EXPECT_EQ(Math::round(-0.5f), 0.0f);
    EXPECT_EQ(Math::round(-0.5 ), 0.0 );
    EXPECT_EQ(Math::round(-0.1f), 0.0f);
    EXPECT_EQ(Math::round(-0.1 ), 0.0 );
    EXPECT_EQ(Math::round( 0.0f), 0.0f);
    EXPECT_EQ(Math::round( 0.0 ), 0.0 );
    EXPECT_EQ(Math::round( 0.1f), 0.0f);
    EXPECT_EQ(Math::round( 0.1 ), 0.0 );
    EXPECT_EQ(Math::round( 0.5f), 1.0f);
    EXPECT_EQ(Math::round( 0.5 ), 1.0 );
    EXPECT_EQ(Math::round( 0.9f), 1.0f);
    EXPECT_EQ(Math::round( 0.9 ), 1.0 );
    EXPECT_EQ(Math::round( 1.5f), 2.0f);
    EXPECT_EQ(Math::round( 1.5 ), 2.0 );
    EXPECT_EQ(Math::round(-4503599627370496.0), -4503599627370496.0);
    EXPECT_EQ(Math::round( 4503599627370496.0),  4503599627370496.0);
  }

  INFO("bl::Math::floorToInt()");
  {
    EXPECT_EQ(Math::floorToInt(-1.5f),-2);
    EXPECT_EQ(Math::floorToInt(-1.5 ),-2);
    EXPECT_EQ(Math::floorToInt(-0.9f),-1);
    EXPECT_EQ(Math::floorToInt(-0.9 ),-1);
    EXPECT_EQ(Math::floorToInt(-0.5f),-1);
    EXPECT_EQ(Math::floorToInt(-0.5 ),-1);
    EXPECT_EQ(Math::floorToInt(-0.1f),-1);
    EXPECT_EQ(Math::floorToInt(-0.1 ),-1);
    EXPECT_EQ(Math::floorToInt( 0.0f), 0);
    EXPECT_EQ(Math::floorToInt( 0.0 ), 0);
    EXPECT_EQ(Math::floorToInt( 0.1f), 0);
    EXPECT_EQ(Math::floorToInt( 0.1 ), 0);
    EXPECT_EQ(Math::floorToInt( 0.5f), 0);
    EXPECT_EQ(Math::floorToInt( 0.5 ), 0);
    EXPECT_EQ(Math::floorToInt( 0.9f), 0);
    EXPECT_EQ(Math::floorToInt( 0.9 ), 0);
    EXPECT_EQ(Math::floorToInt( 1.5f), 1);
    EXPECT_EQ(Math::floorToInt( 1.5 ), 1);
  }

  INFO("bl::Math::ceilToInt()");
  {
    EXPECT_EQ(Math::ceilToInt(-1.5f),-1);
    EXPECT_EQ(Math::ceilToInt(-1.5 ),-1);
    EXPECT_EQ(Math::ceilToInt(-0.9f), 0);
    EXPECT_EQ(Math::ceilToInt(-0.9 ), 0);
    EXPECT_EQ(Math::ceilToInt(-0.5f), 0);
    EXPECT_EQ(Math::ceilToInt(-0.5 ), 0);
    EXPECT_EQ(Math::ceilToInt(-0.1f), 0);
    EXPECT_EQ(Math::ceilToInt(-0.1 ), 0);
    EXPECT_EQ(Math::ceilToInt( 0.0f), 0);
    EXPECT_EQ(Math::ceilToInt( 0.0 ), 0);
    EXPECT_EQ(Math::ceilToInt( 0.1f), 1);
    EXPECT_EQ(Math::ceilToInt( 0.1 ), 1);
    EXPECT_EQ(Math::ceilToInt( 0.5f), 1);
    EXPECT_EQ(Math::ceilToInt( 0.5 ), 1);
    EXPECT_EQ(Math::ceilToInt( 0.9f), 1);
    EXPECT_EQ(Math::ceilToInt( 0.9 ), 1);
    EXPECT_EQ(Math::ceilToInt( 1.5f), 2);
    EXPECT_EQ(Math::ceilToInt( 1.5 ), 2);
  }

  INFO("bl::Math::truncToInt()");
  {
    EXPECT_EQ(Math::truncToInt(-1.5f),-1);
    EXPECT_EQ(Math::truncToInt(-1.5 ),-1);
    EXPECT_EQ(Math::truncToInt(-0.9f), 0);
    EXPECT_EQ(Math::truncToInt(-0.9 ), 0);
    EXPECT_EQ(Math::truncToInt(-0.5f), 0);
    EXPECT_EQ(Math::truncToInt(-0.5 ), 0);
    EXPECT_EQ(Math::truncToInt(-0.1f), 0);
    EXPECT_EQ(Math::truncToInt(-0.1 ), 0);
    EXPECT_EQ(Math::truncToInt( 0.0f), 0);
    EXPECT_EQ(Math::truncToInt( 0.0 ), 0);
    EXPECT_EQ(Math::truncToInt( 0.1f), 0);
    EXPECT_EQ(Math::truncToInt( 0.1 ), 0);
    EXPECT_EQ(Math::truncToInt( 0.5f), 0);
    EXPECT_EQ(Math::truncToInt( 0.5 ), 0);
    EXPECT_EQ(Math::truncToInt( 0.9f), 0);
    EXPECT_EQ(Math::truncToInt( 0.9 ), 0);
    EXPECT_EQ(Math::truncToInt( 1.5f), 1);
    EXPECT_EQ(Math::truncToInt( 1.5 ), 1);
  }

  INFO("bl::Math::roundToInt()");
  {
    EXPECT_EQ(Math::roundToInt(-1.5f),-1);
    EXPECT_EQ(Math::roundToInt(-1.5 ),-1);
    EXPECT_EQ(Math::roundToInt(-0.9f),-1);
    EXPECT_EQ(Math::roundToInt(-0.9 ),-1);
    EXPECT_EQ(Math::roundToInt(-0.5f), 0);
    EXPECT_EQ(Math::roundToInt(-0.5 ), 0);
    EXPECT_EQ(Math::roundToInt(-0.1f), 0);
    EXPECT_EQ(Math::roundToInt(-0.1 ), 0);
    EXPECT_EQ(Math::roundToInt( 0.0f), 0);
    EXPECT_EQ(Math::roundToInt( 0.0 ), 0);
    EXPECT_EQ(Math::roundToInt( 0.1f), 0);
    EXPECT_EQ(Math::roundToInt( 0.1 ), 0);
    EXPECT_EQ(Math::roundToInt( 0.5f), 1);
    EXPECT_EQ(Math::roundToInt( 0.5 ), 1);
    EXPECT_EQ(Math::roundToInt( 0.9f), 1);
    EXPECT_EQ(Math::roundToInt( 0.9 ), 1);
    EXPECT_EQ(Math::roundToInt( 1.5f), 2);
    EXPECT_EQ(Math::roundToInt( 1.5 ), 2);
  }

  INFO("bl::Math::frac()");
  {
    EXPECT_EQ(Math::frac( 0.00f), 0.00f);
    EXPECT_EQ(Math::frac( 0.00 ), 0.00 );
    EXPECT_EQ(Math::frac( 1.00f), 0.00f);
    EXPECT_EQ(Math::frac( 1.00 ), 0.00 );
    EXPECT_EQ(Math::frac( 1.25f), 0.25f);
    EXPECT_EQ(Math::frac( 1.25 ), 0.25 );
    EXPECT_EQ(Math::frac( 1.75f), 0.75f);
    EXPECT_EQ(Math::frac( 1.75 ), 0.75 );
    EXPECT_EQ(Math::frac(-1.00f), 0.00f);
    EXPECT_EQ(Math::frac(-1.00 ), 0.00 );
    EXPECT_EQ(Math::frac(-1.25f), 0.75f);
    EXPECT_EQ(Math::frac(-1.25 ), 0.75 );
    EXPECT_EQ(Math::frac(-1.75f), 0.25f);
    EXPECT_EQ(Math::frac(-1.75 ), 0.25 );
  }

  INFO("bl::Math::isBetween0And1()");
  {
    EXPECT_TRUE(Math::isBetween0And1( 0.0f  ));
    EXPECT_TRUE(Math::isBetween0And1( 0.0   ));
    EXPECT_TRUE(Math::isBetween0And1( 0.5f  ));
    EXPECT_TRUE(Math::isBetween0And1( 0.5   ));
    EXPECT_TRUE(Math::isBetween0And1( 1.0f  ));
    EXPECT_TRUE(Math::isBetween0And1( 1.0   ));
    EXPECT_TRUE(Math::isBetween0And1(-0.0f  ));
    EXPECT_TRUE(Math::isBetween0And1(-0.0   ));
    EXPECT_FALSE(Math::isBetween0And1(-1.0f  ));
    EXPECT_FALSE(Math::isBetween0And1(-1.0   ));
    EXPECT_FALSE(Math::isBetween0And1( 1.001f));
    EXPECT_FALSE(Math::isBetween0And1( 1.001 ));
  }

  INFO("bl::Math::quadRoots");
  {
    size_t count;
    double roots[2];

    // x^2 + 4x + 4 == 0
    count = Math::quadRoots(roots, 1.0, 4.0, 4.0, Traits::minValue<double>(), Traits::maxValue<double>());

    EXPECT_EQ(count, 1u);
    EXPECT_EQ(roots[0], -2.0);

    // -4x^2 + 8x + 12 == 0
    count = Math::quadRoots(roots, -4.0, 8.0, 12.0, Traits::minValue<double>(), Traits::maxValue<double>());

    EXPECT_EQ(count, 2u);
    EXPECT_EQ(roots[0], -1.0);
    EXPECT_EQ(roots[1],  3.0);
  }

  INFO("bl::Math::cubicRoots");
  {
    size_t count;
    double roots[2];

    // 64x^3 - 64 == 1
    count = Math::cubicRoots(roots, 64, 0.0, 0.0, -64, Traits::minValue<double>(), Traits::maxValue<double>());

    EXPECT_EQ(count, 1u);
    EXPECT_EQ(roots[0], 1.0);
  }
}

} // {Tests}
} // {bl}

#endif // BL_TEST
