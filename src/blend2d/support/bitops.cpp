// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../support/bitops_p.h"

// BLSupport - BitOps - Tests
// ==========================

#ifdef BL_TEST

template<typename T>
using LSBBitOps = BLParametrizedBitOps<BLBitOrder::kLSB, T>;

template<typename T>
using MSBBitOps = BLParametrizedBitOps<BLBitOrder::kMSB, T>;

static void testBitArrayOps() {
  uint32_t bits[3];

  INFO("BLParametrizedBitOps<BLBitOrder::kLSB>::bitArrayFill");
  memset(bits, 0, sizeof(bits));
  LSBBitOps<uint32_t>::bitArrayFill(bits, 1, 94);
  EXPECT_EQ(bits[0], 0xFFFFFFFEu);
  EXPECT_EQ(bits[1], 0xFFFFFFFFu);
  EXPECT_EQ(bits[2], 0x7FFFFFFFu);

  INFO("BLParametrizedBitOps<BLBitOrder::kMSB>::bitArrayFill");
  memset(bits, 0, sizeof(bits));
  MSBBitOps<uint32_t>::bitArrayFill(bits, 1, 94);
  EXPECT_EQ(bits[0], 0x7FFFFFFFu);
  EXPECT_EQ(bits[1], 0xFFFFFFFFu);
  EXPECT_EQ(bits[2], 0xFFFFFFFEu);
}

static void testBitIterator() {
  INFO("BLParametrizedBitOps<BLBitOrder::kLSB>::BitIterator<uint32_t>");
  LSBBitOps<uint32_t>::BitIterator lsbIt(0x40000010u);

  EXPECT_TRUE(lsbIt.hasNext());
  EXPECT_EQ(lsbIt.next(), 4u);
  EXPECT_TRUE(lsbIt.hasNext());
  EXPECT_EQ(lsbIt.next(), 30u);
  EXPECT_TRUE(!lsbIt.hasNext());

  INFO("BLParametrizedBitOps<BLBitOrder::kMSB>::BitIterator<uint32_t>");
  MSBBitOps<uint32_t>::BitIterator msbIt(0x40000010u);

  EXPECT_TRUE(msbIt.hasNext());
  EXPECT_EQ(msbIt.next(), 1u);
  EXPECT_TRUE(msbIt.hasNext());
  EXPECT_EQ(msbIt.next(), 27u);
  EXPECT_TRUE(!msbIt.hasNext());
}

static void testBitVectorIterator() {
  static const uint32_t lsbBits[] = { 0x00000001u, 0x80000000u };
  static const uint32_t msbBits[] = { 0x00000001u, 0x80000000u };

  INFO("BLParametrizedBitOps<BLBitOrder::kLSB>::BitVectorIterator<uint32_t>");
  LSBBitOps<uint32_t>::BitVectorIterator lsbIt(lsbBits, BL_ARRAY_SIZE(lsbBits));

  EXPECT_TRUE(lsbIt.hasNext());
  EXPECT_EQ(lsbIt.next(), 0u);
  EXPECT_TRUE(lsbIt.hasNext());
  EXPECT_EQ(lsbIt.next(), 63u);
  EXPECT_TRUE(!lsbIt.hasNext());

  INFO("BLParametrizedBitOps<BLBitOrder::kMSB>::BitVectorIterator<uint32_t>");
  MSBBitOps<uint32_t>::BitVectorIterator msbIt(msbBits, BL_ARRAY_SIZE(msbBits));

  EXPECT_TRUE(msbIt.hasNext());
  EXPECT_EQ(msbIt.next(), 31u);
  EXPECT_TRUE(msbIt.hasNext());
  EXPECT_EQ(msbIt.next(), 32u);
  EXPECT_TRUE(!msbIt.hasNext());
}

static void testBitVectorFlipIterator() {
  static const uint32_t lsbBits[] = { 0xFFFFFFF0u, 0x00FFFFFFu };
  static const uint32_t msbBits[] = { 0x0FFFFFFFu, 0xFFFFFF00u };

  INFO("BLParametrizedBitOps<BLBitOrder::kLSB>::BitVectorFlipIterator<uint32_t>");
  LSBBitOps<uint32_t>::BitVectorFlipIterator lsbIt(lsbBits, BL_ARRAY_SIZE(lsbBits));
  EXPECT_TRUE(lsbIt.hasNext());
  EXPECT_EQ(lsbIt.peekNext(), 4u);
  EXPECT_EQ(lsbIt.nextAndFlip(), 4u);
  EXPECT_TRUE(lsbIt.hasNext());
  EXPECT_EQ(lsbIt.peekNext(), 56u);
  EXPECT_EQ(lsbIt.nextAndFlip(), 56u);
  EXPECT_TRUE(!lsbIt.hasNext());

  INFO("BLParametrizedBitOps<BLBitOrder::kMSB>::BitVectorFlipIterator<uint32_t>");
  MSBBitOps<uint32_t>::BitVectorFlipIterator msbIt(msbBits, BL_ARRAY_SIZE(msbBits));
  EXPECT_TRUE(msbIt.hasNext());
  EXPECT_EQ(msbIt.peekNext(), 4u);
  EXPECT_EQ(msbIt.nextAndFlip(), 4u);
  EXPECT_TRUE(msbIt.hasNext());
  EXPECT_EQ(msbIt.peekNext(), 56u);
  EXPECT_EQ(msbIt.nextAndFlip(), 56u);
  EXPECT_TRUE(!msbIt.hasNext());
}

UNIT(support_bitops, -10) {
  testBitArrayOps();
  testBitIterator();
  testBitVectorIterator();
  testBitVectorFlipIterator();
}
#endif
