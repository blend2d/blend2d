
// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_test_p.h"
#if defined(BL_TEST)

#include "gradient_p.h"
#include "object_p.h"
#include "pattern_p.h"
#include "rgba_p.h"
#include "string_p.h"
#include "var_p.h"

// bl::Var - Tests
// ===============

namespace bl {
namespace Tests {

UNIT(var_basic_types, BL_TEST_GROUP_CORE_OBJECT) {
  INFO("Verifying null value functionality");
  {
    EXPECT_EQ(BLVar().type(), BL_OBJECT_TYPE_NULL);
    EXPECT_EQ(BLVar(), BLVar());

    // Null can be used as a style to disable drawing.
    EXPECT_TRUE(BLVar().isStyle());
  }

  INFO("Verifying bool value functionality");
  {
    EXPECT_EQ(BLVar(true).type(), BL_OBJECT_TYPE_BOOL);
    EXPECT_EQ(BLVar(true), true);
    EXPECT_EQ(BLVar(true), 1);
    EXPECT_EQ(BLVar(true), 1u);
    EXPECT_EQ(BLVar(true), 1.0);
    EXPECT_EQ(BLVar(true), BLVar(true));
    EXPECT_EQ(BLVar(true), BLVar(1));
    EXPECT_EQ(BLVar(true), BLVar(1u));
    EXPECT_EQ(BLVar(true), BLVar(1.0));

    EXPECT_EQ(BLVar(false).type(), BL_OBJECT_TYPE_BOOL);
    EXPECT_EQ(BLVar(false), false);
    EXPECT_EQ(BLVar(false), 0);
    EXPECT_EQ(BLVar(false), 0u);
    EXPECT_EQ(BLVar(false), 0.0);
    EXPECT_EQ(BLVar(false), BLVar(false));
    EXPECT_EQ(BLVar(false), BLVar(0));
    EXPECT_EQ(BLVar(false), BLVar(0u));
    EXPECT_EQ(BLVar(false), BLVar(0.0));
  }

  INFO("Verifying int/uint value functionality");
  {
    EXPECT_EQ(BLVar(0).type(), BL_OBJECT_TYPE_INT64);
    EXPECT_EQ(BLVar(1).type(), BL_OBJECT_TYPE_INT64);
    EXPECT_EQ(BLVar(0u).type(), BL_OBJECT_TYPE_UINT64);
    EXPECT_EQ(BLVar(1u).type(), BL_OBJECT_TYPE_UINT64);
    EXPECT_EQ(BLVar(INT64_MAX).type(), BL_OBJECT_TYPE_INT64);
    EXPECT_EQ(BLVar(INT64_MIN).type(), BL_OBJECT_TYPE_INT64);
    EXPECT_EQ(BLVar(UINT64_MAX).type(), BL_OBJECT_TYPE_UINT64);

    EXPECT_EQ(BLVar(0), 0);
    EXPECT_EQ(BLVar(0u), 0u);
    EXPECT_EQ(BLVar(0), false);
    EXPECT_EQ(BLVar(1), 1);
    EXPECT_EQ(BLVar(1u), 1u);
    EXPECT_EQ(BLVar(1), true);
    EXPECT_EQ(BLVar(-1), -1);
    EXPECT_EQ(BLVar(char(1)), 1);
    EXPECT_EQ(BLVar(int64_t(0)), int64_t(0));
    EXPECT_EQ(BLVar(int64_t(1)), int64_t(1));
    EXPECT_EQ(BLVar(int64_t(-1)), int64_t(-1));
    EXPECT_EQ(BLVar(INT64_MIN), INT64_MIN);
    EXPECT_EQ(BLVar(INT64_MAX), INT64_MAX);
    EXPECT_EQ(BLVar(UINT64_MAX), UINT64_MAX);
    EXPECT_EQ(BLVar(uint64_t(0)), uint64_t(0));
    EXPECT_EQ(BLVar(uint64_t(1)), uint64_t(1));

    EXPECT_EQ(BLVar(int64_t(1)), BLVar(uint64_t(1)));
    EXPECT_EQ(BLVar(uint64_t(1)), BLVar(int64_t(1)));
    EXPECT_EQ(BLVar(double(1)), BLVar(int64_t(1)));
    EXPECT_EQ(BLVar(double(1)), BLVar(uint64_t(1)));
    EXPECT_EQ(BLVar(int64_t(1)), BLVar(double(1)));
    EXPECT_EQ(BLVar(uint64_t(1)), BLVar(double(1)));
  }

  INFO("Verifying float/double value functionality");
  {
    EXPECT_EQ(BLVar(0.0f).type(), BL_OBJECT_TYPE_DOUBLE);
    EXPECT_EQ(BLVar(0.0).type(), BL_OBJECT_TYPE_DOUBLE);
    EXPECT_EQ(BLVar(Math::nan<float>()).type(), BL_OBJECT_TYPE_DOUBLE);
    EXPECT_EQ(BLVar(Math::nan<double>()).type(), BL_OBJECT_TYPE_DOUBLE);

    EXPECT_EQ(BLVar(float(0)), float(0));
    EXPECT_EQ(BLVar(float(0.5)), float(0.5));
    EXPECT_EQ(BLVar(double(0)), double(0));
    EXPECT_EQ(BLVar(double(0.5)), double(0.5));
  }

  INFO("Checking bool/int/uint/double value conversions");
  {
    bool b;
    EXPECT_TRUE(BLVar(0).toBool(&b) == BL_SUCCESS && b == false);
    EXPECT_TRUE(BLVar(1).toBool(&b) == BL_SUCCESS && b == true);
    EXPECT_TRUE(BLVar(0.0).toBool(&b) == BL_SUCCESS && b == false);
    EXPECT_TRUE(BLVar(Math::nan<double>()).toBool(&b) == BL_SUCCESS && b == false);
    EXPECT_TRUE(BLVar(1.0).toBool(&b) == BL_SUCCESS && b == true);
    EXPECT_TRUE(BLVar(123456.0).toBool(&b) == BL_SUCCESS && b == true);
    EXPECT_TRUE(BLVar(-123456.0).toBool(&b) == BL_SUCCESS && b == true);

    int64_t i64;
    EXPECT_TRUE(BLVar(0).toInt64(&i64) == BL_SUCCESS && i64 == 0);
    EXPECT_TRUE(BLVar(1).toInt64(&i64) == BL_SUCCESS && i64 == 1);
    EXPECT_TRUE(BLVar(INT64_MIN).toInt64(&i64) == BL_SUCCESS && i64 == INT64_MIN);
    EXPECT_TRUE(BLVar(INT64_MAX).toInt64(&i64) == BL_SUCCESS && i64 == INT64_MAX);
    EXPECT_TRUE(BLVar(0.0).toInt64(&i64) == BL_SUCCESS && i64 == 0);
    EXPECT_TRUE(BLVar(Math::nan<double>()).toInt64(&i64) == BL_ERROR_INVALID_CONVERSION && i64 == 0);
    EXPECT_TRUE(BLVar(1.0).toInt64(&i64) == BL_SUCCESS && i64 == 1);
    EXPECT_TRUE(BLVar(123456.0).toInt64(&i64) == BL_SUCCESS && i64 == 123456);
    EXPECT_TRUE(BLVar(-123456.0).toInt64(&i64) == BL_SUCCESS && i64 == -123456);
    EXPECT_TRUE(BLVar(123456.3).toInt64(&i64) == BL_ERROR_OVERFLOW && i64 == 123456);
    EXPECT_TRUE(BLVar(123456.9).toInt64(&i64) == BL_ERROR_OVERFLOW && i64 == 123456);

    uint64_t u64;
    EXPECT_TRUE(BLVar(0).toUInt64(&u64) == BL_SUCCESS && u64 == 0);
    EXPECT_TRUE(BLVar(1).toUInt64(&u64) == BL_SUCCESS && u64 == 1);
    EXPECT_TRUE(BLVar(INT64_MIN).toUInt64(&u64) == BL_ERROR_OVERFLOW && u64 == 0);
    EXPECT_TRUE(BLVar(INT64_MAX).toUInt64(&u64) == BL_SUCCESS && u64 == uint64_t(INT64_MAX));
    EXPECT_TRUE(BLVar(0.0).toUInt64(&u64) == BL_SUCCESS && u64 == 0);
    EXPECT_TRUE(BLVar(Math::nan<double>()).toUInt64(&u64) == BL_ERROR_INVALID_CONVERSION && u64 == 0);
    EXPECT_TRUE(BLVar(1.0).toUInt64(&u64) == BL_SUCCESS && u64 == 1);
    EXPECT_TRUE(BLVar(123456.0).toUInt64(&u64) == BL_SUCCESS && u64 == 123456);
    EXPECT_TRUE(BLVar(-123456.0).toUInt64(&u64) == BL_ERROR_OVERFLOW && u64 == 0);
    EXPECT_TRUE(BLVar(123456.3).toUInt64(&u64) == BL_ERROR_OVERFLOW && u64 == 123456);
    EXPECT_TRUE(BLVar(123456.9).toUInt64(&u64) == BL_ERROR_OVERFLOW && u64 == 123456);

    double f64;
    EXPECT_TRUE(BLVar(true).toDouble(&f64) == BL_SUCCESS && f64 == 1.0);
    EXPECT_TRUE(BLVar(false).toDouble(&f64) == BL_SUCCESS && f64 == 0.0);
    EXPECT_TRUE(BLVar(0).toDouble(&f64) == BL_SUCCESS && f64 == 0.0);
    EXPECT_TRUE(BLVar(1).toDouble(&f64) == BL_SUCCESS && f64 == 1.0);
    EXPECT_TRUE(BLVar(0u).toDouble(&f64) == BL_SUCCESS && f64 == 0.0);
    EXPECT_TRUE(BLVar(1u).toDouble(&f64) == BL_SUCCESS && f64 == 1.0);
    EXPECT_TRUE(BLVar(0.0).toDouble(&f64) == BL_SUCCESS && f64 == 0.0);
    EXPECT_TRUE(BLVar(1.0).toDouble(&f64) == BL_SUCCESS && f64 == 1.0);
    EXPECT_TRUE(BLVar(Math::nan<double>()).toDouble(&f64) == BL_SUCCESS && Math::isNaN(f64));
  }
}

UNIT(var_styles, BL_TEST_GROUP_CORE_OBJECT) {
  INFO("Verifying BLRgba[32|64] value functionality");
  {
    BLRgba32 rgba32 = BLRgba32(0xFF001122);
    EXPECT_EQ(BLVar(rgba32).type(), BL_OBJECT_TYPE_RGBA32);
    EXPECT_EQ(BLVar(rgba32), BLVar(rgba32));
    EXPECT_EQ(BLVar(rgba32).as<BLRgba32>(), rgba32);
    EXPECT_TRUE(BLVar(rgba32).isRgba32());
    EXPECT_TRUE(BLVar(rgba32).isStyle());

    BLRgba64 rgba64 = BLRgba64(0xFFFF000011112222);
    EXPECT_EQ(BLVar(rgba64).type(), BL_OBJECT_TYPE_RGBA64);
    EXPECT_EQ(BLVar(rgba64), BLVar(rgba64));
    EXPECT_EQ(BLVar(rgba64).as<BLRgba64>(), rgba64);
    EXPECT_TRUE(BLVar(rgba64).isRgba64());
    EXPECT_TRUE(BLVar(rgba64).isStyle());

    BLRgba rgba = BLRgba(0.1f, 0.2f, 0.3f, 0.5f);
    EXPECT_EQ(BLVar(rgba).type(), BL_OBJECT_TYPE_RGBA);
    EXPECT_EQ(BLVar(rgba), BLVar(rgba));
    EXPECT_EQ(BLVar(rgba).as<BLRgba>(), rgba);
    EXPECT_TRUE(BLVar(rgba).isRgba());
    EXPECT_TRUE(BLVar(rgba).isStyle());

    // Wrapped BLRgba is an exception - it doesn't form a valid BLObject signature.
    EXPECT_FALSE(BLVar(rgba)._d.hasObjectSignature());
  }

  INFO("Checking BLRgba[32|64] value conversions");
  {
    BLRgba32 rgba32;
    BLRgba64 rgba64;

    EXPECT_TRUE(BLVar(BLRgba32(0xFF008040u)).toRgba32(&rgba32) == BL_SUCCESS && rgba32 == BLRgba32(0xFF008040u));
    EXPECT_TRUE(BLVar(BLRgba32(0xFF008040u)).toRgba64(&rgba64) == BL_SUCCESS && rgba64 == BLRgba64(0xFFFF000080804040u));
    EXPECT_TRUE(BLVar(BLRgba64(0xFFEE00DD80CC40BBu)).toRgba64(&rgba64) == BL_SUCCESS && rgba64 == BLRgba64(0xFFEE00DD80CC40BBu));
    EXPECT_TRUE(BLVar(BLRgba64(0xFFEE00DD80CC40BBu)).toRgba32(&rgba32) == BL_SUCCESS && rgba32 == BLRgba32(0xFF008040u));
  }

  INFO("Checking BLGradient value functionality");
  {
    EXPECT_TRUE(BLVar(BLGradient()).isStyle());

    BLGradient g;
    g.addStop(0.0, BLRgba32(0x00000000u));
    g.addStop(1.0, BLRgba32(0xFFFFFFFFu));

    BLVar var(BLInternal::move(g));

    // The object should have been moved, so `g` should be default constructed now.
    EXPECT_EQ(g._d.getType(), BL_OBJECT_TYPE_GRADIENT);
    EXPECT_EQ(g, BLGradient());
    EXPECT_FALSE(var.equals(g));

    g = var.as<BLGradient>();
    EXPECT_TRUE(var.equals(g));
  }

  INFO("Checking BLPattern value functionality");
  {
    EXPECT_TRUE(BLVar(BLPattern()).isStyle());

    BLPattern p(BLImage(16, 16, BL_FORMAT_PRGB32));
    BLVar var(BLInternal::move(p));

    // The object should have been moved, so `p` should be default constructed now.
    EXPECT_EQ(p._d.getType(), BL_OBJECT_TYPE_PATTERN);
    EXPECT_EQ(p, BLPattern());
    EXPECT_FALSE(var.equals(p));

    p = var.as<BLPattern>();
    EXPECT_TRUE(var.equals(p));
  }
}

} // {Tests}
} // {bl}

#endif // BL_TEST
