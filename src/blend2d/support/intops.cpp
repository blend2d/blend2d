// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../support/bitops_p.h"

// Support - BitOps - Tests
// ========================

#ifdef BL_TEST

static void testAlignment() noexcept {
  INFO("BLIntOps::isAligned()");
  {
    EXPECT_FALSE(BLIntOps::isAligned<size_t>(0xFFFF,  4));
    EXPECT_TRUE(BLIntOps::isAligned<size_t>(0xFFF4,  4));
    EXPECT_TRUE(BLIntOps::isAligned<size_t>(0xFFF8,  8));
    EXPECT_TRUE(BLIntOps::isAligned<size_t>(0xFFF0, 16));
  }

  INFO("BLIntOps::isPowerOf2()");
  {
    for (uint32_t i = 0; i < 64; i++) {
      EXPECT_TRUE(BLIntOps::isPowerOf2(uint64_t(1) << i));
      EXPECT_FALSE(BLIntOps::isPowerOf2((uint64_t(1) << i) ^ 0x001101));
    }
  }

  INFO("BLIntOps::alignUp()");
  {
    EXPECT_EQ(BLIntOps::alignUp<size_t>(0xFFFF,  4), 0x10000u);
    EXPECT_EQ(BLIntOps::alignUp<size_t>(0xFFF4,  4), 0x0FFF4u);
    EXPECT_EQ(BLIntOps::alignUp<size_t>(0xFFF8,  8), 0x0FFF8u);
    EXPECT_EQ(BLIntOps::alignUp<size_t>(0xFFF0, 16), 0x0FFF0u);
    EXPECT_EQ(BLIntOps::alignUp<size_t>(0xFFF0, 32), 0x10000u);
  }

  INFO("BLIntOps::alignUpDiff()");
  {
    EXPECT_EQ(BLIntOps::alignUpDiff<size_t>(0xFFFF,  4), 1u);
    EXPECT_EQ(BLIntOps::alignUpDiff<size_t>(0xFFF4,  4), 0u);
    EXPECT_EQ(BLIntOps::alignUpDiff<size_t>(0xFFF8,  8), 0u);
    EXPECT_EQ(BLIntOps::alignUpDiff<size_t>(0xFFF0, 16), 0u);
    EXPECT_EQ(BLIntOps::alignUpDiff<size_t>(0xFFF0, 32), 16u);
  }

  INFO("BLIntOps::alignUpPowerOf2()");
  {
    EXPECT_EQ(BLIntOps::alignUpPowerOf2<size_t>(0x0000), 0x00000u);
    EXPECT_EQ(BLIntOps::alignUpPowerOf2<size_t>(0xFFFF), 0x10000u);
    EXPECT_EQ(BLIntOps::alignUpPowerOf2<size_t>(0xF123), 0x10000u);
    EXPECT_EQ(BLIntOps::alignUpPowerOf2<size_t>(0x0F00), 0x01000u);
    EXPECT_EQ(BLIntOps::alignUpPowerOf2<size_t>(0x0100), 0x00100u);
    EXPECT_EQ(BLIntOps::alignUpPowerOf2<size_t>(0x1001), 0x02000u);
  }
}

static void testArithmetic() noexcept {
  INFO("BLIntOps::addOverflow()");
  {
    BLOverflowFlag of = 0;

    EXPECT_TRUE(BLIntOps::addOverflow<int32_t>(0, 0, &of) == 0 && !of);
    EXPECT_TRUE(BLIntOps::addOverflow<int32_t>(0, 1, &of) == 1 && !of);
    EXPECT_TRUE(BLIntOps::addOverflow<int32_t>(1, 0, &of) == 1 && !of);

    EXPECT_TRUE(BLIntOps::addOverflow<int32_t>(2147483647, 0, &of) == 2147483647 && !of);
    EXPECT_TRUE(BLIntOps::addOverflow<int32_t>(0, 2147483647, &of) == 2147483647 && !of);
    EXPECT_TRUE(BLIntOps::addOverflow<int32_t>(2147483647, -1, &of) == 2147483646 && !of);
    EXPECT_TRUE(BLIntOps::addOverflow<int32_t>(-1, 2147483647, &of) == 2147483646 && !of);

    EXPECT_TRUE(BLIntOps::addOverflow<int32_t>(-2147483647, 0, &of) == -2147483647 && !of);
    EXPECT_TRUE(BLIntOps::addOverflow<int32_t>(0, -2147483647, &of) == -2147483647 && !of);
    EXPECT_TRUE(BLIntOps::addOverflow<int32_t>(-2147483647, -1, &of) == -2147483647 - 1 && !of);
    EXPECT_TRUE(BLIntOps::addOverflow<int32_t>(-1, -2147483647, &of) == -2147483647 - 1 && !of);

    (void)BLIntOps::addOverflow<int32_t>(2147483647, 1, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::addOverflow<int32_t>(1, 2147483647, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::addOverflow<int32_t>(-2147483647, -2, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::addOverflow<int32_t>(-2, -2147483647, &of); EXPECT_NE(of, 0); of = 0;

    EXPECT_TRUE(BLIntOps::addOverflow<uint32_t>(0u, 0u, &of) == 0 && !of);
    EXPECT_TRUE(BLIntOps::addOverflow<uint32_t>(0u, 1u, &of) == 1 && !of);
    EXPECT_TRUE(BLIntOps::addOverflow<uint32_t>(1u, 0u, &of) == 1 && !of);

    EXPECT_TRUE(BLIntOps::addOverflow<uint32_t>(2147483647u, 1u, &of) == 2147483648u && !of);
    EXPECT_TRUE(BLIntOps::addOverflow<uint32_t>(1u, 2147483647u, &of) == 2147483648u && !of);
    EXPECT_TRUE(BLIntOps::addOverflow<uint32_t>(0xFFFFFFFFu, 0u, &of) == 0xFFFFFFFFu && !of);
    EXPECT_TRUE(BLIntOps::addOverflow<uint32_t>(0u, 0xFFFFFFFFu, &of) == 0xFFFFFFFFu && !of);

    (void)BLIntOps::addOverflow<uint32_t>(0xFFFFFFFFu, 1u, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::addOverflow<uint32_t>(1u, 0xFFFFFFFFu, &of); EXPECT_NE(of, 0); of = 0;

    (void)BLIntOps::addOverflow<uint32_t>(0x80000000u, 0xFFFFFFFFu, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::addOverflow<uint32_t>(0xFFFFFFFFu, 0x80000000u, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::addOverflow<uint32_t>(0xFFFFFFFFu, 0xFFFFFFFFu, &of); EXPECT_NE(of, 0); of = 0;
  }

  INFO("BLIntOps::subOverflow()");
  {
    BLOverflowFlag of = 0;

    EXPECT_TRUE(BLIntOps::subOverflow<int32_t>(0, 0, &of) ==  0 && !of);
    EXPECT_TRUE(BLIntOps::subOverflow<int32_t>(0, 1, &of) == -1 && !of);
    EXPECT_TRUE(BLIntOps::subOverflow<int32_t>(1, 0, &of) ==  1 && !of);
    EXPECT_TRUE(BLIntOps::subOverflow<int32_t>(0, -1, &of) ==  1 && !of);
    EXPECT_TRUE(BLIntOps::subOverflow<int32_t>(-1, 0, &of) == -1 && !of);

    EXPECT_TRUE(BLIntOps::subOverflow<int32_t>(2147483647, 1, &of) == 2147483646 && !of);
    EXPECT_TRUE(BLIntOps::subOverflow<int32_t>(2147483647, 2147483647, &of) == 0 && !of);
    EXPECT_TRUE(BLIntOps::subOverflow<int32_t>(-2147483647, 1, &of) == -2147483647 - 1 && !of);
    EXPECT_TRUE(BLIntOps::subOverflow<int32_t>(-2147483647, -1, &of) == -2147483646 && !of);
    EXPECT_TRUE(BLIntOps::subOverflow<int32_t>(-2147483647 - 0, -2147483647 - 0, &of) == 0 && !of);
    EXPECT_TRUE(BLIntOps::subOverflow<int32_t>(-2147483647 - 1, -2147483647 - 1, &of) == 0 && !of);

    (void)BLIntOps::subOverflow<int32_t>(-2, 2147483647, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::subOverflow<int32_t>(-2147483647, 2, &of); EXPECT_NE(of, 0); of = 0;

    (void)BLIntOps::subOverflow<int32_t>(-2147483647    , 2147483647, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::subOverflow<int32_t>(-2147483647 - 1, 2147483647, &of); EXPECT_NE(of, 0); of = 0;

    (void)BLIntOps::subOverflow<int32_t>(2147483647, -2147483647 - 0, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::subOverflow<int32_t>(2147483647, -2147483647 - 1, &of); EXPECT_NE(of, 0); of = 0;

    EXPECT_TRUE(BLIntOps::subOverflow<uint32_t>(0, 0, &of) ==  0 && !of);
    EXPECT_TRUE(BLIntOps::subOverflow<uint32_t>(1, 0, &of) ==  1 && !of);

    EXPECT_TRUE(BLIntOps::subOverflow<uint32_t>(0xFFFFFFFFu, 0u, &of) == 0xFFFFFFFFu && !of);
    EXPECT_TRUE(BLIntOps::subOverflow<uint32_t>(0xFFFFFFFFu, 0xFFFFFFFFu, &of) == 0u && !of);

    (void)BLIntOps::subOverflow<uint32_t>(0u, 1u, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::subOverflow<uint32_t>(1u, 2u, &of); EXPECT_NE(of, 0); of = 0;

    (void)BLIntOps::subOverflow<uint32_t>(0u, 0xFFFFFFFFu, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::subOverflow<uint32_t>(1u, 0xFFFFFFFFu, &of); EXPECT_NE(of, 0); of = 0;

    (void)BLIntOps::subOverflow<uint32_t>(0u, 0x7FFFFFFFu, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::subOverflow<uint32_t>(1u, 0x7FFFFFFFu, &of); EXPECT_NE(of, 0); of = 0;

    (void)BLIntOps::subOverflow<uint32_t>(0x7FFFFFFEu, 0x7FFFFFFFu, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::subOverflow<uint32_t>(0xFFFFFFFEu, 0xFFFFFFFFu, &of); EXPECT_NE(of, 0); of = 0;
  }

  INFO("BLIntOps::mulOverflow()");
  {
    BLOverflowFlag of = 0;

    EXPECT_TRUE(BLIntOps::mulOverflow<int32_t>(0, 0, &of) == 0 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int32_t>(0, 1, &of) == 0 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int32_t>(1, 0, &of) == 0 && !of);

    EXPECT_TRUE(BLIntOps::mulOverflow<int32_t>( 1,  1, &of) ==  1 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int32_t>( 1, -1, &of) == -1 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int32_t>(-1,  1, &of) == -1 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int32_t>(-1, -1, &of) ==  1 && !of);

    EXPECT_TRUE(BLIntOps::mulOverflow<int32_t>( 32768,  65535, &of) ==  2147450880 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int32_t>( 32768, -65535, &of) == -2147450880 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int32_t>(-32768,  65535, &of) == -2147450880 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int32_t>(-32768, -65535, &of) ==  2147450880 && !of);

    EXPECT_TRUE(BLIntOps::mulOverflow<int32_t>(2147483647, 1, &of) == 2147483647 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int32_t>(1, 2147483647, &of) == 2147483647 && !of);

    EXPECT_TRUE(BLIntOps::mulOverflow<int32_t>(-2147483647 - 1, 1, &of) == -2147483647 - 1 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int32_t>(1, -2147483647 - 1, &of) == -2147483647 - 1 && !of);

    (void)BLIntOps::mulOverflow<int32_t>( 65535,  65535, &of); EXPECT_TRUE(of); of = 0;
    (void)BLIntOps::mulOverflow<int32_t>( 65535, -65535, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::mulOverflow<int32_t>(-65535,  65535, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::mulOverflow<int32_t>(-65535, -65535, &of); EXPECT_NE(of, 0); of = 0;

    (void)BLIntOps::mulOverflow<int32_t>( 2147483647    ,  2147483647    , &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::mulOverflow<int32_t>( 2147483647    , -2147483647 - 1, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::mulOverflow<int32_t>(-2147483647 - 1,  2147483647    , &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::mulOverflow<int32_t>(-2147483647 - 1, -2147483647 - 1, &of); EXPECT_NE(of, 0); of = 0;

    EXPECT_TRUE(BLIntOps::mulOverflow<uint32_t>(0, 0, &of) == 0 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<uint32_t>(0, 1, &of) == 0 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<uint32_t>(1, 0, &of) == 0 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<uint32_t>(1, 1, &of) == 1 && !of);

    EXPECT_TRUE(BLIntOps::mulOverflow<uint32_t>(0x10000000u, 15, &of) == 0xF0000000u && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<uint32_t>(15, 0x10000000u, &of) == 0xF0000000u && !of);

    EXPECT_TRUE(BLIntOps::mulOverflow<uint32_t>(0xFFFFFFFFu, 1, &of) == 0xFFFFFFFFu && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<uint32_t>(1, 0xFFFFFFFFu, &of) == 0xFFFFFFFFu && !of);

    (void)BLIntOps::mulOverflow<uint32_t>(0xFFFFFFFFu, 2, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::mulOverflow<uint32_t>(2, 0xFFFFFFFFu, &of); EXPECT_NE(of, 0); of = 0;

    (void)BLIntOps::mulOverflow<uint32_t>(0x80000000u, 2, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::mulOverflow<uint32_t>(2, 0x80000000u, &of); EXPECT_NE(of, 0); of = 0;

    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(0, 0, &of) == 0 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(0, 1, &of) == 0 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(1, 0, &of) == 0 && !of);

    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>( 1,  1, &of) ==  1 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>( 1, -1, &of) == -1 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(-1,  1, &of) == -1 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(-1, -1, &of) ==  1 && !of);

    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>( 32768,  65535, &of) ==  2147450880 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>( 32768, -65535, &of) == -2147450880 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(-32768,  65535, &of) == -2147450880 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(-32768, -65535, &of) ==  2147450880 && !of);

    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(2147483647, 1, &of) == 2147483647 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(1, 2147483647, &of) == 2147483647 && !of);

    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(-2147483647 - 1, 1, &of) == -2147483647 - 1 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(1, -2147483647 - 1, &of) == -2147483647 - 1 && !of);

    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>( 65535,  65535, &of) ==  int64_t(4294836225) && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>( 65535, -65535, &of) == -int64_t(4294836225) && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(-65535,  65535, &of) == -int64_t(4294836225) && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(-65535, -65535, &of) ==  int64_t(4294836225) && !of);

    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>( 2147483647    ,  2147483647    , &of) ==  int64_t(4611686014132420609) && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>( 2147483647    , -2147483647 - 1, &of) == -int64_t(4611686016279904256) && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(-2147483647 - 1,  2147483647    , &of) == -int64_t(4611686016279904256) && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(-2147483647 - 1, -2147483647 - 1, &of) ==  int64_t(4611686018427387904) && !of);

    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(int64_t(0x7FFFFFFFFFFFFFFF), int64_t(1), &of) == int64_t(0x7FFFFFFFFFFFFFFF) && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<int64_t>(int64_t(1), int64_t(0x7FFFFFFFFFFFFFFF), &of) == int64_t(0x7FFFFFFFFFFFFFFF) && !of);

    (void)BLIntOps::mulOverflow<int64_t>(int64_t(0x7FFFFFFFFFFFFFFF), int64_t(2), &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::mulOverflow<int64_t>(int64_t(2), int64_t(0x7FFFFFFFFFFFFFFF), &of); EXPECT_NE(of, 0); of = 0;

    (void)BLIntOps::mulOverflow<int64_t>(int64_t( 0x7FFFFFFFFFFFFFFF), int64_t( 0x7FFFFFFFFFFFFFFF), &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::mulOverflow<int64_t>(int64_t( 0x7FFFFFFFFFFFFFFF), int64_t(-0x7FFFFFFFFFFFFFFF), &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::mulOverflow<int64_t>(int64_t(-0x7FFFFFFFFFFFFFFF), int64_t( 0x7FFFFFFFFFFFFFFF), &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::mulOverflow<int64_t>(int64_t(-0x7FFFFFFFFFFFFFFF), int64_t(-0x7FFFFFFFFFFFFFFF), &of); EXPECT_NE(of, 0); of = 0;

    EXPECT_TRUE(BLIntOps::mulOverflow<uint64_t>(0, 0, &of) == 0 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<uint64_t>(0, 1, &of) == 0 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<uint64_t>(1, 0, &of) == 0 && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<uint64_t>(1, 1, &of) == 1 && !of);

    EXPECT_TRUE(BLIntOps::mulOverflow<uint64_t>(0x1000000000000000u, 15, &of) == 0xF000000000000000u && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<uint64_t>(15, 0x1000000000000000u, &of) == 0xF000000000000000u && !of);

    EXPECT_TRUE(BLIntOps::mulOverflow<uint64_t>(0xFFFFFFFFFFFFFFFFu, 1, &of) == 0xFFFFFFFFFFFFFFFFu && !of);
    EXPECT_TRUE(BLIntOps::mulOverflow<uint64_t>(1, 0xFFFFFFFFFFFFFFFFu, &of) == 0xFFFFFFFFFFFFFFFFu && !of);

    (void)BLIntOps::mulOverflow<uint64_t>(0xFFFFFFFFFFFFFFFFu, 2, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::mulOverflow<uint64_t>(2, 0xFFFFFFFFFFFFFFFFu, &of); EXPECT_NE(of, 0); of = 0;

    (void)BLIntOps::mulOverflow<uint64_t>(0x8000000000000000u, 2, &of); EXPECT_NE(of, 0); of = 0;
    (void)BLIntOps::mulOverflow<uint64_t>(2, 0x8000000000000000u, &of); EXPECT_NE(of, 0); of = 0;
  }
}

template<typename T>
BL_INLINE bool checkConsecutiveBitMask(T x) {
  if (x == 0)
    return false;

  T m = x;
  while ((m & 0x1) == 0)
    m >>= 1;
  return ((m + 1) & m) == 0;
}

static void testBitUtils() noexcept {
  uint32_t i;

  INFO("BLIntOps::shl() / BLIntOps::shr()");
  EXPECT_EQ(BLIntOps::shl<int32_t >(0x00001111 , 16), 0x11110000 );
  EXPECT_EQ(BLIntOps::shl<uint32_t>(0x00001111 , 16), 0x11110000u );
  EXPECT_EQ(BLIntOps::shr<int32_t >(0x11110000u, 16), 0x00001111 );
  EXPECT_EQ(BLIntOps::shr<uint32_t>(0x11110000u, 16), 0x00001111u);
  EXPECT_EQ(BLIntOps::sar<uint32_t>(0xFFFF0000u, 16), 0xFFFFFFFFu);

  INFO("BLIntOps::rol() / BLIntOps::ror()");
  EXPECT_EQ(BLIntOps::rol<int32_t >(0x00100000 , 16), 0x00000010 );
  EXPECT_EQ(BLIntOps::rol<uint32_t>(0x00100000u, 16), 0x00000010u);
  EXPECT_EQ(BLIntOps::ror<int32_t >(0x00001000 , 16), 0x10000000 );
  EXPECT_EQ(BLIntOps::ror<uint32_t>(0x00001000u, 16), 0x10000000u);

  INFO("BLIntOps::clz()");
  EXPECT_EQ(BLIntOps::clz<uint32_t>(1u), 31u);
  EXPECT_EQ(BLIntOps::clz<uint32_t>(2u), 30u);
  EXPECT_EQ(BLIntOps::clz<uint32_t>(3u), 30u);
  EXPECT_EQ(BLIntOps::clz<uint32_t>(0x80000000u), 0u);
  EXPECT_EQ(BLIntOps::clz<uint32_t>(0x88888888u), 0u);
  EXPECT_EQ(BLIntOps::clz<uint32_t>(0x11111111u), 3u);
  EXPECT_EQ(BLIntOps::clz<uint32_t>(0x12345678u), 3u);
  EXPECT_EQ(BLIntOps::clzStatic<uint32_t>(1u), 31u);
  EXPECT_EQ(BLIntOps::clzStatic<uint32_t>(2u), 30u);
  EXPECT_EQ(BLIntOps::clzStatic<uint32_t>(3u), 30u);
  EXPECT_EQ(BLIntOps::clzStatic<uint32_t>(0x80000000u), 0u);
  EXPECT_EQ(BLIntOps::clzStatic<uint32_t>(0x88888888u), 0u);
  EXPECT_EQ(BLIntOps::clzStatic<uint32_t>(0x11111111u), 3u);
  EXPECT_EQ(BLIntOps::clzStatic<uint32_t>(0x12345678u), 3u);

  for (i = 0; i < 32; i++) {
    EXPECT_EQ(BLIntOps::clz(uint32_t(1) << i), 31 - i);
    EXPECT_EQ(BLIntOps::clz(uint32_t(0xFFFFFFFFu) >> i), i);
  }

  INFO("BLIntOps::ctz()");
  EXPECT_EQ(BLIntOps::ctz<uint32_t>(1u), 0u);
  EXPECT_EQ(BLIntOps::ctz<uint32_t>(2u), 1u);
  EXPECT_EQ(BLIntOps::ctz<uint32_t>(3u), 0u);
  EXPECT_EQ(BLIntOps::ctz<uint32_t>(0x80000000u), 31u);
  EXPECT_EQ(BLIntOps::ctz<uint32_t>(0x88888888u), 3u);
  EXPECT_EQ(BLIntOps::ctz<uint32_t>(0x11111111u), 0u);
  EXPECT_EQ(BLIntOps::ctz<uint32_t>(0x12345678u), 3u);
  EXPECT_EQ(BLIntOps::ctzStatic<uint32_t>(1u), 0u);
  EXPECT_EQ(BLIntOps::ctzStatic<uint32_t>(2u), 1u);
  EXPECT_EQ(BLIntOps::ctzStatic<uint32_t>(3u), 0u);
  EXPECT_EQ(BLIntOps::ctzStatic<uint32_t>(0x80000000u), 31u);
  EXPECT_EQ(BLIntOps::ctzStatic<uint32_t>(0x88888888u), 3u);
  EXPECT_EQ(BLIntOps::ctzStatic<uint32_t>(0x11111111u), 0u);
  EXPECT_EQ(BLIntOps::ctzStatic<uint32_t>(0x12345678u), 3u);

  for (i = 0; i < 32; i++) {
    EXPECT_EQ(BLIntOps::ctz(uint32_t(1) << i), i);
    EXPECT_EQ(BLIntOps::ctz(uint32_t(0xFFFFFFFFu) << i), i);
  }

  INFO("BLIntOps::isBitMaskConsecutive()");
  i = 0;
  for (;;) {
    bool result = BLIntOps::isBitMaskConsecutive(i);
    bool expect = checkConsecutiveBitMask(i);
    EXPECT_EQ(result, expect);

    if (i == 0xFFFFu)
      break;
    i++;
  }
}

static void testByteSwap() noexcept {
  INFO("BLIntOps::byteSwap()");
  EXPECT_EQ(BLIntOps::byteSwap16(int16_t(0x0102)), int16_t(0x0201));
  EXPECT_EQ(BLIntOps::byteSwap16(uint16_t(0x0102)), uint16_t(0x0201));
  EXPECT_EQ(BLIntOps::byteSwap24(int32_t(0x00010203)), int32_t(0x00030201));
  EXPECT_EQ(BLIntOps::byteSwap24(uint32_t(0x00010203)), uint32_t(0x00030201));
  EXPECT_EQ(BLIntOps::byteSwap32(int32_t(0x01020304)), int32_t(0x04030201));
  EXPECT_EQ(BLIntOps::byteSwap32(uint32_t(0x01020304)), uint32_t(0x04030201));
  EXPECT_EQ(BLIntOps::byteSwap64(uint64_t(0x0102030405060708)), uint64_t(0x0807060504030201));
}

static void testClamp() noexcept {
  INFO("BLIntOps::clamp()");
  {
    EXPECT_EQ(blClamp<int>(-1, 100, 1000), 100);
    EXPECT_EQ(blClamp<int>(99, 100, 1000), 100);
    EXPECT_EQ(blClamp<int>(1044, 100, 1000), 1000);
    EXPECT_EQ(blClamp<double>(-1.0, 100.0, 1000.0), 100.0);
    EXPECT_EQ(blClamp<double>(99.0, 100.0, 1000.0), 100.0);
    EXPECT_EQ(blClamp<double>(1044.0, 100.0, 1000.0), 1000.0);
  }

  INFO("BLIntOps::clampToByte()");
  {
    EXPECT_EQ(BLIntOps::clampToByte(-1), 0u);
    EXPECT_EQ(BLIntOps::clampToByte(42), 42u);
    EXPECT_EQ(BLIntOps::clampToByte(255), 0xFFu);
    EXPECT_EQ(BLIntOps::clampToByte(256), 0xFFu);
    EXPECT_EQ(BLIntOps::clampToByte(0x7FFFFFFF), 0xFFu);
    EXPECT_EQ(BLIntOps::clampToByte(0x7FFFFFFFu), 0xFFu);
    EXPECT_EQ(BLIntOps::clampToByte(0xFFFFFFFFu), 0xFFu);
  }

  INFO("BLIntOps::clampToWord()");
  {
    EXPECT_EQ(BLIntOps::clampToWord(-1), 0u);
    EXPECT_EQ(BLIntOps::clampToWord(42), 42u);
    EXPECT_EQ(BLIntOps::clampToWord(0xFFFF), 0xFFFFu);
    EXPECT_EQ(BLIntOps::clampToWord(0x10000), 0xFFFFu);
    EXPECT_EQ(BLIntOps::clampToWord(0x10000u), 0xFFFFu);
    EXPECT_EQ(BLIntOps::clampToWord(0x7FFFFFFF), 0xFFFFu);
    EXPECT_EQ(BLIntOps::clampToWord(0x7FFFFFFFu), 0xFFFFu);
    EXPECT_EQ(BLIntOps::clampToWord(0xFFFFFFFFu), 0xFFFFu);
  }
}

static void testPopCount() {
  INFO("BLIntOps::PopCounter<uint32_t>");

  static const uint32_t bitWordData[] = {
    0xFFFFFFFF, 0x11881111, 0x10000000, 0x08000000,
    0x00000001, 0x00000008, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFEFEFEFE, 0xCCCCCCCC, 0xBACFE1D9, 0x11100111,
    0x12DFEAAA, 0xFE1290AA, 0xF1018021, 0x00000000,
    0x23467111, 0x11111111, 0x137F137F
  };

  for (uint32_t i = 1; i < BL_ARRAY_SIZE(bitWordData); i++) {
    BLIntOps::PopCounterSimple<uint32_t> simple;
    BLIntOps::PopCounterHarleySeal<uint32_t> harleySeal;

    simple.addArray(bitWordData, i);
    harleySeal.addArray(bitWordData, i);

    EXPECT_EQ(simple.get(), harleySeal.get());
  }
}

UNIT(support_intops, -10) {
  testAlignment();
  testArithmetic();
  testBitUtils();
  testByteSwap();
  testClamp();
  testPopCount();
}
#endif
