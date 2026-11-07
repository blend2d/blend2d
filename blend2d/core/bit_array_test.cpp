// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/axl/commons.h>
#include <blend2d/core/bit_array_p.h>
#include <blend2d/core/object_layer_p.h>

#include <initializer_list>

namespace bl::BitArrayTests {

// BitArray - Test Utilities
// =========================

constexpr size_t kBitWordBits = axl::bit_size_of<BLBitWord>;
constexpr uint32_t kSSOBitCapacity = BLBitArray::Info::kSSOCapacity;

static void expect_size_and_cardinality(const BLBitArray& ba, size_t size, size_t cardinality) noexcept {
  EXPECT_EQ(ba.is_empty(), size == 0u);
  EXPECT_EQ(ba.size(), size);
  EXPECT_GE(ba.capacity(), ba.size());
  EXPECT_EQ(ba.cardinality(), cardinality);
  EXPECT_EQ(ba.cardinality_in_range(0, size), cardinality);
}

static void expect_words(const BLBitArray& ba, size_t size, const BLBitWord* words, size_t word_count) noexcept {
  EXPECT_EQ(ba.is_empty(), size == 0u);
  EXPECT_EQ(ba.size(), size);
  EXPECT_GE(ba.capacity(), ba.size());

  uint32_t cardinality = 0u;

  size_t bit_index = 0;

  for (size_t word_index = 0; word_index < word_count; word_index++) {
    BLBitWord word = words[word_index];
    for (size_t i = 0; i < kBitWordBits && bit_index < size; i++, bit_index++) {
      BLBitWord mask = axl::bit_mask<BLBitWord>(i);
      EXPECT_EQ(ba.cardinality_in_range(0, bit_index), cardinality);
      EXPECT_EQ(ba.has_bits_in_range(0, bit_index), cardinality != 0u);

      if (word & mask) {
        EXPECT_TRUE(ba.has_bit(bit_index));
        cardinality++;
      }
      else {
        EXPECT_FALSE(ba.has_bit(bit_index));
      }
    }
  }

  EXPECT_EQ(ba.cardinality(), cardinality);
  EXPECT_EQ(ba.cardinality_in_range(0, size), cardinality);
  EXPECT_EQ(ba.has_bits_in_range(0, size), cardinality != 0u);
}

// Creates a bit-shifted words in `dst` based on `src` shifted right by `bit_shift`.
//
// The destination buffer have `word_count + 1` size.
static void bit_shift_words(BLBitWord* dst, const BLBitWord* src, size_t word_count, uint32_t bit_shift, BLBitWord init_content = 0u) noexcept {
  uint32_t opposite_shift = kBitWordBits - bit_shift;
  BLBitWord acc = init_content;
  for (size_t i = 0; i < word_count; i++) {
    BLBitWord word = src[i];
    dst[i] = acc | (word << bit_shift);
    acc = word >> opposite_shift;
  }
  dst[word_count] = acc;
}

// BitArray - Tests
// ================

TEST_CASE(core_bit_array_constructors) {
  BLBitArray ba;
  BLBitArray empty;

  EXPECT_TRUE(ba._d.is_sso());
  EXPECT_TRUE(ba.equals(empty));
  EXPECT_EQ(ba.capacity(), kSSOBitCapacity);
  expect_size_and_cardinality(ba, 0u, 0);
}

TEST_CASE(core_bit_array_bit_ops) {
  BLBitArray ba;
  BLBitArray empty;

  for (uint32_t i = 0; i < kSSOBitCapacity; i++) {
    EXPECT_OK(ba.append_bit(true));
    EXPECT_TRUE(ba._d.is_sso());
    EXPECT_TRUE(ba.equals(ba));
    EXPECT_FALSE(ba.equals(empty));
    expect_size_and_cardinality(ba, i + 1u, i + 1u);
  }

  EXPECT_FALSE(ba.has_bit(10000u));
  EXPECT_FALSE(ba.has_bit(SIZE_MAX));

  EXPECT_OK(ba.clear());
  EXPECT_TRUE(ba._d.is_sso());
  EXPECT_TRUE(ba.equals(empty));
  EXPECT_EQ(ba.capacity(), kSSOBitCapacity);
  expect_size_and_cardinality(ba, 0u, 0u);

  for (uint32_t i = 0; i < kSSOBitCapacity; i++) {
    EXPECT_OK(ba.append_bit(i & 1 ? true : false));
    EXPECT_TRUE(ba._d.is_sso());
    EXPECT_TRUE(ba.equals(ba));
    EXPECT_FALSE(ba.equals(empty));
    expect_size_and_cardinality(ba, i + 1u, (i + 1u) / 2u);
  }

  for (uint32_t i = 0; i < kSSOBitCapacity; i++) {
    EXPECT_OK(ba.fill_bit(i));
  }
  expect_size_and_cardinality(ba, kSSOBitCapacity, kSSOBitCapacity);

  for (uint32_t i = 0; i < kSSOBitCapacity; i++) {
    EXPECT_OK(ba.clear_bit(i));
  }
  expect_size_and_cardinality(ba, kSSOBitCapacity, 0);

  constexpr size_t kDynamicCount = axl::align_up(100000u, 64u * 2u);

  for (uint32_t variation : {0u, 1u, 2u, 3u, 4u}) {
    BLBitArray copy;
    auto handle_variation = [&]() {
      switch (variation) {
        case 0: break;
        case 1: copy = ba; break;
        case 2: ba.shrink(); break;
        case 3: copy = ba; ba.shrink(); break;
        case 4: ba.reserve(1024); break;
      }
    };

    EXPECT_OK(ba.reset());

    for (size_t i = 0; i < kDynamicCount; i++) {
      EXPECT_OK(ba.append_bit(true));
      EXPECT_EQ(ba.size(), i + 1u);
      EXPECT_TRUE(ba.equals(ba));

      handle_variation();
    }

    EXPECT_EQ(ba.cardinality(), kDynamicCount);
    EXPECT_EQ(ba.cardinality_in_range(0, kDynamicCount), kDynamicCount);

    for (size_t i = 0; i < kDynamicCount; i += 2) {
      EXPECT_OK(ba.clear_bit(i));
      EXPECT_EQ(ba.size(), kDynamicCount);

      handle_variation();
    }

    EXPECT_EQ(ba.cardinality(), kDynamicCount / 2u);
    EXPECT_EQ(ba.cardinality_in_range(0, kDynamicCount), kDynamicCount / 2u);

    for (size_t i = 0; i < kDynamicCount; i += 2) {
      EXPECT_OK(ba.replace_bit(i, true));
      EXPECT_EQ(ba.size(), kDynamicCount);

      handle_variation();
    }

    EXPECT_EQ(ba.cardinality(), kDynamicCount);
    EXPECT_EQ(ba.cardinality_in_range(0, kDynamicCount), kDynamicCount);
  }

  for (size_t size : {0u, 1u, 2u, 66u, 129u, 666u, 6666u}) {
    EXPECT_OK(ba.reset());

    for (size_t i = 0; i < size; i++) {
      EXPECT_OK(ba.append_bit(i & 1 ? true : false));
    }

    for (size_t capacity : {0u, 1u, 100u, 1000u, 1000000u}) {
      EXPECT_OK(ba.reserve(capacity));
      EXPECT_EQ(ba.size(), size);
      EXPECT_GE(ba.capacity(), capacity);
      EXPECT_EQ(ba.cardinality(), size / 2u);
    }
  }
}

TEST_CASE(core_bit_array_assign_zeroed) {
  const BLBitWord words[] = {
    0x80000000u, 0x40000000u, 0x20000000u, 0xFFFFFFFFu,
    0x00000000u, 0x12345678u, 0x000FFFF0u, 0xFF00FF00u,
    0xFFFF0000u, 0xFF008812u, 0x12349876u, 0x00000001u
  };

  BLBitArray ba;
  BLBitArray tmp;

  for (uint32_t variation : {0u, 1u, 2u, 3u, 4u}) {
    EXPECT_OK(ba.reset());

    auto handle_variation = [&]() {
      switch (variation) {
        case 1: tmp = ba; break;
        case 2: ba.reserve(1000u); break;
        case 3: ba.shrink(); break;
        case 4: tmp = ba; ba.shrink(); break;
      }
    };

    for (size_t word_count = 0; word_count < BL_ARRAY_SIZE(words); word_count++) {
      BLBitWord* bits;
      EXPECT_OK(ba.assign_zeroed(kBitWordBits * word_count, &bits));
      memcpy(bits, words, word_count * sizeof(BLBitWord));

      expect_words(ba, kBitWordBits * word_count, words, word_count);
      handle_variation();
    }
  }

}

TEST_CASE(core_bit_array_word_ops) {
  constexpr size_t kWordCount = 8;

  const BLBitWord words_a[] = { 0x80000000u, 0x40000000u, 0x20000000u, 0xFFFFFFFFu, 0x00000000u, 0x12345678u, 0x000FFFF0u, 0xFF00FF00u };
  const BLBitWord words_b[] = { 0x00000111u, 0x00300000u, 0x00040000u, 0x00000000u, 0xFFFFFFFFu, 0x80400001u, 0xFFF0000Fu, 0x00FF00FFu };
  const BLBitWord no_words[kWordCount] {};

  BLBitWord words_combined[kWordCount] {};
  for (size_t i = 0; i < kWordCount; i++) {
    words_combined[i] = words_a[i] | words_b[i];
  }

  for (size_t word_count = 0; word_count < kWordCount; word_count++) {
    for (uint32_t variation : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u}) {
      BLBitArray ba1;
      BLBitArray ba2;

      BLBitArray copy;

      auto handle_variation = [&]() {
        switch (variation) {
          case 0: break;
          case 1: copy = ba1; break;
          case 2: copy = ba2; break;
          case 3: ba1.shrink(); break;
          case 4: ba2.shrink(); break;
          case 5: copy = ba1; ba1.shrink(); break;
          case 6: copy = ba2; ba2.shrink(); break;
          case 7: ba1.reserve(1024); break;
          case 8: ba2.reserve(1024); break;
        }
      };

      for (size_t i = 0; i < word_count; i++) {
        EXPECT_OK(ba1.append_word(words_a[i]));
      }
      EXPECT_TRUE(ba1.subsumes(ba2));
      EXPECT_FALSE(ba1.intersects(ba2));
      EXPECT_OK(ba2.append_words(words_a, word_count));

      EXPECT_TRUE(ba1.subsumes(ba2));
      EXPECT_EQ(ba1.intersects(ba2), word_count != 0u);

      expect_words(ba1, kBitWordBits * word_count, words_a, word_count);
      expect_words(ba2, kBitWordBits * word_count, words_a, word_count);

      EXPECT_EQ(ba1, ba2);
      handle_variation();

      EXPECT_OK(ba2.append_bit(true));
      EXPECT_NE(ba1, ba2);
      EXPECT_TRUE(ba2.subsumes(ba1));
      handle_variation();

      EXPECT_OK(ba2.resize(ba1.size()));
      EXPECT_EQ(ba1, ba2);
      handle_variation();

      for (size_t i = 0; i < word_count; i++) {
        EXPECT_OK(ba1.clear_word(i * kBitWordBits, words_a[i]));
      }
      EXPECT_TRUE(ba2.subsumes(ba1));
      EXPECT_OK(ba2.clear_words(0u, words_a, word_count));
      EXPECT_EQ(ba1, ba2);

      expect_words(ba1, kBitWordBits * word_count, no_words, word_count);
      expect_words(ba2, kBitWordBits * word_count, no_words, word_count);
      handle_variation();

      for (size_t i = 0; i < word_count; i++) {
        EXPECT_OK(ba1.replace_word(i * kBitWordBits, words_b[i]));
      }
      EXPECT_OK(ba2.replace_words(0u, words_b, word_count));

      expect_words(ba1, kBitWordBits * word_count, words_b, word_count);
      expect_words(ba2, kBitWordBits * word_count, words_b, word_count);
      handle_variation();

      for (size_t i = 0; i < word_count; i++) {
        EXPECT_OK(ba1.fill_word(i * kBitWordBits, words_a[i]));
      }
      EXPECT_OK(ba2.fill_words(0u, words_a, word_count));

      expect_words(ba1, kBitWordBits * word_count, words_combined, word_count);
      expect_words(ba2, kBitWordBits * word_count, words_combined, word_count);
      handle_variation();

      EXPECT_OK(ba1.clear());
      EXPECT_OK(ba2.clear());
      EXPECT_EQ(ba1, ba2);
      expect_words(ba1, 0u, nullptr, 0u);
      expect_words(ba2, 0u, nullptr, 0u);
      handle_variation();

      EXPECT_OK(ba1.assign_words(words_a, word_count));
      EXPECT_OK(ba2.assign_words(words_b, word_count));
      EXPECT_EQ(ba1.equals(ba2), word_count == 0u);
      expect_words(ba1, kBitWordBits * word_count, words_a, word_count);
      expect_words(ba2, kBitWordBits * word_count, words_b, word_count);
      handle_variation();

      // Appends words to a BitArray that has 1 bit, thus the words need to be shifted.
      for (bool initial_bit : {false, true}) {
        EXPECT_OK(ba1.reset());
        EXPECT_TRUE(ba1._d.is_sso());

        EXPECT_OK(ba1.append_bit(initial_bit));
        EXPECT_OK(ba1.append_words(words_a, word_count));

        BLBitWord shifted_words[kWordCount + 1u] {};
        bit_shift_words(shifted_words, words_a, word_count, 1u, static_cast<BLBitWord>(initial_bit));

        expect_words(ba1, 1u + kBitWordBits * word_count, shifted_words, word_count + 1u);
      }
    }
  }
}

TEST_CASE(core_bit_array_range_queries) {
  const BLBitWord words[] = {0u, 1u, axl::bit_mask<BLBitWord>(kBitWordBits - 1u), 0u};

  for (size_t word_count = 0; word_count <= BL_ARRAY_SIZE(words); word_count++) {
    BLBitArray ba;
    EXPECT_OK(ba.assign_words(words, word_count));

    size_t size = ba.size();
    for (size_t start = 0; start <= size + 1u; start++) {
      size_t cardinality = 0;
      for (size_t end = 0; end <= size + 1u; end++) {
        if (end > start && end <= size) {
          size_t bit_index = end - 1u;
          BLBitWord mask = axl::bit_mask<BLBitWord>(bit_index % kBitWordBits);
          cardinality += size_t((words[bit_index / kBitWordBits] & mask) != 0u);
        }

        EXPECT_EQ(ba.cardinality_in_range(start, end), cardinality);
        EXPECT_EQ(ba.has_bits_in_range(start, end), cardinality != 0u);
      }

      EXPECT_EQ(ba.cardinality_in_range(start, SIZE_MAX), cardinality);
      EXPECT_EQ(ba.has_bits_in_range(start, SIZE_MAX), cardinality != 0u);
    }
  }
}

TEST_CASE(core_bit_array_range_ops) {
  for (uint32_t variation : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u}) {
    BLBitArray ba;
    BLBitArray copy;

    auto handle_variation = [&]() {
      switch (variation) {
        case 0: break;
        case 1: copy = ba; break;
        case 3: ba.shrink(); break;
        case 5: copy = ba; ba.shrink(); break;
        case 7: ba.reserve(1024); break;
      }
    };

    BLRange range {};
    EXPECT_FALSE(ba.get_range(&range));
    EXPECT_EQ(range.start, 0u);
    EXPECT_EQ(range.end, 0u);

    EXPECT_OK(ba.reset());

    for (size_t i = 0; i < 1000u; i++) {
      if (i > 0u) {
        EXPECT_OK(ba.clear_bit(i - 1));
      }

      handle_variation();
      EXPECT_OK(ba.append_bit(true));

      EXPECT_TRUE(ba.get_range(&range));
      EXPECT_EQ(range.start, i);
      EXPECT_EQ(range.end, i + 1);
    }

    EXPECT_OK(ba.reset());

    BLRange range_data[] = {
      {10u, 12u}, {100u, 111u}, {0u, 1000u}, {3u, 64u}, {1u, 1024}
    };

    for (BLRange r : range_data) {
      EXPECT_OK(ba.reset());

      EXPECT_OK(ba.resize(r.end));
      handle_variation();

      EXPECT_OK(ba.fill_range(r));
      handle_variation();

      EXPECT_EQ(ba.fill_range(1000000u, 1u), BL_ERROR_INVALID_VALUE);

      EXPECT_TRUE(ba.get_range(&range));
      EXPECT_EQ(range.start, r.start);
      EXPECT_EQ(range.end, r.end);

      size_t range_size = r.end - r.start;
      EXPECT_EQ(ba.cardinality(), range_size);
      EXPECT_EQ(ba.cardinality_in_range(r), range_size);

      EXPECT_OK(ba.clear_range(r));
      handle_variation();

      EXPECT_FALSE(ba.get_range(&range));
      EXPECT_EQ(range.start, 0u);
      EXPECT_EQ(range.end, 0u);

      EXPECT_EQ(ba.cardinality(), 0u);
      EXPECT_EQ(ba.cardinality_in_range(r), 0u);
    }
  }
}

TEST_CASE(core_bit_array_boolean_ops) {
  // Intersection, but not subsumption - bit-arrays having same sizes.
  {
    const BLBitWord a_words[] = { 0x80000000u, 0x40000000u, 0x20000000u, 0xFFFFFFFFu, 0x000FFFF0u, 0xFF00FF00u };
    const BLBitWord b_words[] = { 0x00000001u, 0x90000000u, 0x10000000u, 0x00000000u, 0xFFF0000Fu, 0x80000000u };

    BLBitArray a;
    BLBitArray b;

    EXPECT_OK(a.assign_words(a_words, BL_ARRAY_SIZE(a_words)));
    EXPECT_OK(b.assign_words(b_words, BL_ARRAY_SIZE(b_words)));

    EXPECT_NE(a, b);
    EXPECT_FALSE(a.subsumes(b));
    EXPECT_FALSE(b.subsumes(a));
    EXPECT_TRUE(a.intersects(b));
    EXPECT_TRUE(b.intersects(a));
  }

  // No intersection nor subsumption - bit-arrays having different sizes.
  {
    const BLBitWord a_words[] = { 0x80000000u, 0x40000000u, 0x20000000u, 0xFFFFFFFFu, 0x000FFFF0u, 0xFF00FF00u };
    const BLBitWord b_words[] = { 0x00000001u, 0x90000000u, 0x10000000u, 0x00000000u, 0xFFF0000Fu };

    BLBitArray a;
    BLBitArray b;

    EXPECT_OK(a.assign_words(a_words, BL_ARRAY_SIZE(a_words)));
    EXPECT_OK(b.assign_words(b_words, BL_ARRAY_SIZE(b_words)));

    EXPECT_NE(a, b);
    EXPECT_FALSE(a.subsumes(b));
    EXPECT_FALSE(b.subsumes(a));
    EXPECT_FALSE(a.intersects(b));
    EXPECT_FALSE(b.intersects(a));
  }

  // Subsumption & intersection - bit-arrays having different sizes.
  {
    const BLBitWord a_words[] = { 0x80000001u, 0xF0000000u, 0xF0000000u, 0xFFFFFFFFu, 0xFFFFFFFFu };
    const BLBitWord b_words[] = { 0x00000001u, 0x90000000u, 0x10000000u, 0x00000000u, 0xFFF0000Fu, 0x00000000u };

    BLBitArray a;
    BLBitArray b;

    EXPECT_OK(a.assign_words(a_words, BL_ARRAY_SIZE(a_words)));
    EXPECT_OK(b.assign_words(b_words, BL_ARRAY_SIZE(b_words)));

    EXPECT_NE(a, b);
    EXPECT_TRUE(a.subsumes(b));
    EXPECT_FALSE(b.subsumes(a));
    EXPECT_TRUE(a.intersects(b));
    EXPECT_TRUE(b.intersects(a));
  }
}

TEST_CASE(core_bit_array_compare) {
  BLBitArray ba1;
  BLBitArray ba2;
  BLBitArray copy;

  for (uint32_t variation : {0u, 1u, 2u, 3u, 4u, 5u, 6u}) {
    auto handle_variation = [&]() {
      switch (variation) {
        case 1: copy = ba1; break;
        case 2: copy = ba2; break;
        case 3: ba1.reserve(1000); break;
        case 4: ba2.reserve(1000); break;
        case 5: ba1.shrink(); break;
        case 6: ba2.shrink(); break;
      }
    };

    for (size_t i = 0; i < 4000u; i++) {
      handle_variation();

      EXPECT_OK(ba1.append_bit(1));
      EXPECT_OK(ba2.append_bit(1));
      EXPECT_EQ(ba1, ba2);
      EXPECT_TRUE(ba1.equals(ba2));
      EXPECT_TRUE(ba2.equals(ba1));

      size_t indexes[] = {size_t(0), i / 2u, i};
      for (size_t bit_index : indexes) {
        EXPECT_OK(ba1.replace_bit(bit_index, false));
        EXPECT_FALSE(ba1.equals(ba2));
        EXPECT_EQ(ba1.compare(ba2), -1);

        EXPECT_OK(ba2.replace_bit(bit_index, false));
        EXPECT_TRUE(ba1.equals(ba2));
        EXPECT_EQ(ba1.compare(ba2), 0);

        EXPECT_OK(ba1.replace_bit(bit_index, true));
        EXPECT_FALSE(ba1.equals(ba2));
        EXPECT_EQ(ba1.compare(ba2), 1);

        EXPECT_OK(ba2.replace_bit(bit_index, true));
        EXPECT_TRUE(ba1.equals(ba2));
        EXPECT_EQ(ba1.compare(ba2), 0);
      }
    }
  }
}

TEST_CASE(core_bit_array_c_api_vs_cxx_api) {
  BLBitArrayCore ba1;

  EXPECT_OK(bl_bit_array_init(&ba1));
  EXPECT_EQ(bl_bit_array_is_empty(&ba1), true);
  EXPECT_EQ(bl_bit_array_get_size(&ba1), 0u);
  EXPECT_EQ(bl_bit_array_get_size_in_words(&ba1), 0u);
  EXPECT_EQ(bl_bit_array_get_capacity(&ba1), BLBitArray::Info::kSSOCapacity);
  EXPECT_EQ(bl_bit_array_get_capacity_in_words(&ba1), BLBitArray::kSSOCapacityInWords);

  EXPECT_OK(bl_bit_array_append_bit(&ba1, false));
  EXPECT_EQ(bl_bit_array_is_empty(&ba1), false);
  EXPECT_EQ(bl_bit_array_get_size(&ba1), 1u);
  EXPECT_EQ(bl_bit_array_get_size_in_words(&ba1), 1u);
  EXPECT_EQ(bl_bit_array_get_capacity(&ba1), BLBitArray::Info::kSSOCapacity);
  EXPECT_EQ(bl_bit_array_get_capacity_in_words(&ba1), BLBitArray::kSSOCapacityInWords);

  EXPECT_OK(bl_bit_array_append_bit(&ba1, true));
  EXPECT_EQ(bl_bit_array_is_empty(&ba1), false);
  EXPECT_EQ(bl_bit_array_get_size(&ba1), 2u);
  EXPECT_EQ(bl_bit_array_get_size_in_words(&ba1), 1u);
  EXPECT_EQ(bl_bit_array_get_capacity(&ba1), BLBitArray::Info::kSSOCapacity);
  EXPECT_EQ(bl_bit_array_get_capacity_in_words(&ba1), BLBitArray::kSSOCapacityInWords);

  EXPECT_EQ(bl_bit_array_get_data(&ba1), ba1.dcast().data());
  for (size_t i = 2u; i < 1000u; i++) {
    EXPECT_OK(bl_bit_array_append_bit(&ba1, (i & 1) ? true : false));
  }
  EXPECT_EQ(bl_bit_array_get_size(&ba1), 1000u);
  EXPECT_GE(bl_bit_array_get_capacity(&ba1), 1000u);
  EXPECT_EQ(bl_bit_array_get_data(&ba1), ba1.dcast().data());

  BLBitArrayCore ba2;
  EXPECT_OK(bl_bit_array_init_weak(&ba2, &ba1));
  EXPECT_TRUE(bl_bit_array_equals(&ba1, &ba2));

  BLBitArrayCore ba3;
  EXPECT_OK(bl_bit_array_init_move(&ba3, &ba1));
  EXPECT_TRUE(bl_bit_array_equals(&ba2, &ba3));
  EXPECT_FALSE(bl_bit_array_equals(&ba1, &ba2));

  EXPECT_OK(bl_bit_array_assign_move(&ba1, &ba3));
  EXPECT_TRUE(bl_bit_array_equals(&ba1, &ba2));
  EXPECT_FALSE(bl_bit_array_equals(&ba1, &ba3));

  EXPECT_OK(bl_bit_array_destroy(&ba1));
  EXPECT_OK(bl_bit_array_destroy(&ba2));
  EXPECT_OK(bl_bit_array_destroy(&ba3));
}

} // {bl::BitArrayTests}

#endif // BL_TEST
