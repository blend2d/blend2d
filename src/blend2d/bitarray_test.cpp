// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_test_p.h"
#if defined(BL_TEST)

#include "bitarray_p.h"
#include "object_p.h"

// bl::BitArray - Tests
// ====================

namespace bl {
namespace Tests {

UNIT(bitarray, BL_TEST_GROUP_CORE_CONTAINERS) {
  constexpr uint32_t kSSOBitCapacity = BLBitArray::kSSOWordCount * 32u;
  BLBitArray empty;

  INFO("[SSO] Basic functionality");
  {
    BLBitArray ba;

    EXPECT_TRUE(ba._d.sso());
    EXPECT_TRUE(ba.empty());
    EXPECT_EQ(ba.size(), 0u);
    EXPECT_EQ(ba.capacity(), kSSOBitCapacity);
    EXPECT_TRUE(ba.equals(empty));

    for (uint32_t i = 0; i < kSSOBitCapacity; i++) {
      EXPECT_SUCCESS(ba.appendBit(true));
      EXPECT_EQ(ba.size(), i + 1u);
      EXPECT_TRUE(ba._d.sso());
      EXPECT_TRUE(ba.equals(ba));
      EXPECT_FALSE(ba.equals(empty));

      EXPECT_EQ(ba.cardinality(), i + 1u);
      EXPECT_EQ(ba.cardinalityInRange(0, i + 1u), i + 1u);
    }

    ba.clear();
    EXPECT_TRUE(ba._d.sso());
    EXPECT_TRUE(ba.empty());
    EXPECT_EQ(ba.size(), 0u);
    EXPECT_EQ(ba.capacity(), kSSOBitCapacity);
    EXPECT_TRUE(ba.equals(empty));

    for (uint32_t i = 0; i < kSSOBitCapacity; i++) {
      EXPECT_SUCCESS(ba.appendBit(i & 1 ? true : false));
      EXPECT_EQ(ba.size(), i + 1u);
      EXPECT_TRUE(ba._d.sso());
      EXPECT_TRUE(ba.equals(ba));
      EXPECT_FALSE(ba.equals(empty));
    }

    EXPECT_EQ(ba.cardinality(), kSSOBitCapacity / 2u);
    EXPECT_EQ(ba.cardinalityInRange(0, kSSOBitCapacity), kSSOBitCapacity / 2u);

    for (uint32_t i = 0; i < kSSOBitCapacity; i++) {
      EXPECT_SUCCESS(ba.setBit(i));
    }

    EXPECT_EQ(ba.cardinality(), kSSOBitCapacity);
    EXPECT_EQ(ba.cardinalityInRange(0, kSSOBitCapacity), kSSOBitCapacity);

    for (uint32_t i = 0; i < kSSOBitCapacity; i++) {
      EXPECT_SUCCESS(ba.clearBit(i));
    }

    EXPECT_EQ(ba.cardinality(), 0u);
    EXPECT_EQ(ba.cardinalityInRange(0, kSSOBitCapacity), 0u);
  }

  INFO("[SSO] Using word quantities");
  {
    BLBitArray ba;
    EXPECT_TRUE(ba._d.sso());

    uint32_t words[3] = { 0x80000000, 0x40000000, 0x20000000 };
    EXPECT_SUCCESS(ba.appendWords(words, 3));

    EXPECT_TRUE(ba._d.sso());
    EXPECT_EQ(ba.size(), 96u);
    EXPECT_TRUE(ba.hasBit(0));
    EXPECT_TRUE(ba.hasBit(33));
    EXPECT_TRUE(ba.hasBit(66));
    EXPECT_FALSE(ba.hasBit(1));
    EXPECT_FALSE(ba.hasBit(32));
    EXPECT_FALSE(ba.hasBit(34));
    EXPECT_FALSE(ba.hasBit(65));
    EXPECT_FALSE(ba.hasBit(67));

    ba.clear();
    EXPECT_TRUE(ba._d.sso());

    // Appends words to a BitArray that has 1 butm thus the words need to be shifted.
    ba.appendBit(false);
    EXPECT_SUCCESS(ba.appendWords(words, 2));

    EXPECT_TRUE(ba.hasBit(1));
    EXPECT_TRUE(ba.hasBit(34));
    EXPECT_FALSE(ba.hasBit(0));
    EXPECT_FALSE(ba.hasBit(2));
    EXPECT_FALSE(ba.hasBit(33));
    EXPECT_FALSE(ba.hasBit(35));
    EXPECT_FALSE(ba.hasBit(66));
    EXPECT_FALSE(ba.hasBit(68));

    EXPECT_SUCCESS(ba.clear());
  }

  INFO("[Dynamic] Basic functionality");
  {
    constexpr uint32_t kCount = uint32_t(100000 & -64);
    BLBitArray ba;

    for (uint32_t i = 0; i < kCount; i++) {
      EXPECT_SUCCESS(ba.appendBit(true));
      EXPECT_EQ(ba.size(), i + 1u);
      EXPECT_TRUE(ba.equals(ba));
    }

    EXPECT_EQ(ba.cardinality(), kCount);
    EXPECT_EQ(ba.cardinalityInRange(0, kCount), kCount);

    for (uint32_t i = 0; i < kCount; i += 2) {
      EXPECT_SUCCESS(ba.clearBit(i));
      EXPECT_EQ(ba.size(), kCount);
    }

    EXPECT_EQ(ba.cardinality(), kCount / 2u);
    EXPECT_EQ(ba.cardinalityInRange(0, kCount), kCount / 2u);

    for (uint32_t i = 0; i < kCount; i += 2) {
      EXPECT_SUCCESS(ba.replaceBit(i, true));
      EXPECT_EQ(ba.size(), kCount);
    }

    EXPECT_EQ(ba.cardinality(), kCount);
    EXPECT_EQ(ba.cardinalityInRange(0, kCount), kCount);

    uint32_t pattern = 0;
    for (uint32_t i = 0; i < kCount; i += 32) {
      EXPECT_SUCCESS(ba.replaceWord(i, pattern));
      pattern ^= 0xFFFFFFFFu;
    }

    EXPECT_EQ(ba.cardinality(), kCount / 2u);
    EXPECT_EQ(ba.cardinalityInRange(0, kCount), kCount / 2u);

    for (uint32_t i = 0; i < kCount; i += 32) {
      EXPECT_SUCCESS(ba.clearWord(i, 0x33333333));
    }

    EXPECT_EQ(ba.cardinality(), kCount / 4u);
    EXPECT_EQ(ba.cardinalityInRange(0, kCount), kCount / 4u);
  }
}

} // {Tests}
} // {bl}

#endif // BL_TEST
