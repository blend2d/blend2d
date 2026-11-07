// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/axl/arena_allocator_p.h>
#include <blend2d/axl/arena_bit_set_p.h>

// bl::ArenaBitSet - Tests
// =========================

namespace bl::Tests {

TEST_CASE(support_arena_bit_set) {
  ArenaAllocator arena(8192);

  uint32_t i, count;
  uint32_t kMaxCount = 1000;

  ArenaBitSet<BLBitWord> ba;
  EXPECT_TRUE(ba.is_empty());
  EXPECT_EQ(ba.size(), 0u);

  TEST_LOG("bl::ArenaBitSet::resize()");
  for (count = 1; count < kMaxCount; count++) {
    ba.clear();

    EXPECT_OK(ba.resize(&arena, count, false));
    EXPECT_EQ(ba.size(), count);

    for (i = 0; i < count; i++)
      EXPECT_FALSE(ba.bit_at(i));

    ba.clear();
    EXPECT_OK(ba.resize(&arena, count, true));
    EXPECT_EQ(ba.size(), count);

    for (i = 0; i < count; i++)
      EXPECT_TRUE(ba.bit_at(i));
  }

  TEST_LOG("bl::ArenaBitSet::fill_bits() / clear_bits()");
  for (count = 1; count < kMaxCount; count += 2) {
    ba.clear();

    EXPECT_OK(ba.resize(&arena, count));
    EXPECT_EQ(ba.size(), count);

    for (i = 0; i < (count + 1) / 2; i++) {
      bool value = bool(i & 1);
      if (value)
        ba.fill_bits(i, count - i * 2);
      else
        ba.clear_bits(i, count - i * 2);
    }

    for (i = 0; i < count; i++) {
      EXPECT_EQ(ba.bit_at(i), bool(i & 1));
    }
  }
}

} // {bl::Tests}

#endif // BL_TEST
