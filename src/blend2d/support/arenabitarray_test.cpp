// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_test_p.h"
#if defined(BL_TEST)

#include "../support/arenaallocator_p.h"
#include "../support/arenabitarray_p.h"

// BLArenaBitArray - Tests
// =======================

namespace BLArenaBitArrayTests {

UNIT(support_arenabitarray, BL_TEST_GROUP_SUPPORT_CONTAINERS) {
  BLArenaAllocator arena(8096 - BLArenaAllocator::kBlockOverhead);

  uint32_t i, count;
  uint32_t kMaxCount = 1000;

  BLArenaBitArray<BLBitWord> ba;
  EXPECT_TRUE(ba.empty());
  EXPECT_EQ(ba.size(), 0u);

  INFO("BLArenaBitArray::resize()");
  for (count = 1; count < kMaxCount; count++) {
    ba.clear();

    EXPECT_SUCCESS(ba.resize(&arena, count, false));
    EXPECT_EQ(ba.size(), count);

    for (i = 0; i < count; i++)
      EXPECT_FALSE(ba.bitAt(i));

    ba.clear();
    EXPECT_SUCCESS(ba.resize(&arena, count, true));
    EXPECT_EQ(ba.size(), count);

    for (i = 0; i < count; i++)
      EXPECT_TRUE(ba.bitAt(i));
  }

  INFO("BLArenaBitArray::fillBits() / clearBits()");
  for (count = 1; count < kMaxCount; count += 2) {
    ba.clear();

    EXPECT_SUCCESS(ba.resize(&arena, count));
    EXPECT_EQ(ba.size(), count);

    for (i = 0; i < (count + 1) / 2; i++) {
      bool value = bool(i & 1);
      if (value)
        ba.fillBits(i, count - i * 2);
      else
        ba.clearBits(i, count - i * 2);
    }

    for (i = 0; i < count; i++) {
      EXPECT_EQ(ba.bitAt(i), bool(i & 1));
    }
  }
}

} // {BLArenaBitArrayTests}

#endif // BL_TEST
