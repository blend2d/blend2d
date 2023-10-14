// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_test_p.h"
#if defined(BL_TEST)

#include "bitset_p.h"
#include "random.h"
#include "simd/simd_p.h"
#include "support/intops_p.h"

// bl::BitSet - Tests
// ==================

namespace bl {
namespace Tests {

static void dumpBitSet(const BLBitSetCore* self) noexcept {
  if (self->_d.sso()) {
    if (self->_d.isBitSetRange()) {
      BitSetInternal::Range range = BitSetInternal::getSSORange(self);
      printf("BitSet<SSO_Range> {%u-%u}\n", range.start, range.end);
    }
    else {
      uint32_t wordIndex = BitSetInternal::getSSOWordIndex(self);
      printf("BitSet<SSO_Dense> {%u-%u}\n", wordIndex, wordIndex + BitSetInternal::kSSOWordCount);
      for (uint32_t i = 0; i < BitSetInternal::kSSOWordCount; i++) {
        printf("  [%u] %08X\n", i, self->_d.u32_data[i]);
      }
    }
  }
  else {
    const BLBitSetImpl* impl = BitSetInternal::getImpl(self);
    printf("BitSet<Dynamic> {Count=%u Capacity=%u}\n", impl->segmentCount, impl->segmentCapacity);

    for (uint32_t i = 0; i < impl->segmentCount; i++) {
      const BLBitSetSegment* segment = impl->segmentData() + i;
      if (segment->allOnes()) {
          printf("  [%u] {%u-%llu} [ones]\n", i, segment->startBit(), (unsigned long long)segment->lastBit() + 1);
      }
      else {
        for (uint32_t j = 0; j < BitSetInternal::kSegmentWordCount; j++) {
          uint32_t bitIndex = segment->startBit() + j * 32;
          printf("  [%u] {%u-%llu} [%08X]\n", i, bitIndex, (unsigned long long)bitIndex + 32, segment->data()[j]);
        }
      }
    }
  }
}

static void testBits(const BLBitSet& bitSet, uint32_t wordIndex, const uint32_t* wordData, uint32_t wordCount) noexcept {
  for (uint32_t i = 0; i < wordCount; i++) {
    for (uint32_t j = 0; j < 32; j++) {
      uint32_t bitIndex = (wordIndex + i) * 32 + j;
      bool bitValue = BitSetOps::hasBit(wordData[i], j);
      EXPECT_EQ(bitSet.hasBit(bitIndex), bitValue)
        .message("Failed to test bit [%u] - the bit value is not '%s'", bitIndex, bitValue ? "true" : "false");
    }
  }
}

UNIT(bitset, BL_TEST_GROUP_CORE_CONTAINERS) {
  uint32_t kNumBits = 1000000u;
  uint32_t kSSOLastWord = BitSetInternal::kSSOLastWord;

  INFO("Checking SSO BitSet basics");
  {
    BLBitSet set;
    EXPECT_TRUE(set.empty());

    EXPECT_SUCCESS(set.addBit(32));
    EXPECT_TRUE(set._d.sso());
    EXPECT_TRUE(set._d.isBitSetRange());

    EXPECT_SUCCESS(set.addBit(33));
    EXPECT_TRUE(set._d.sso());
    EXPECT_TRUE(set._d.isBitSetRange());

    EXPECT_SUCCESS(set.addBit(35));
    EXPECT_TRUE(set._d.sso());
    EXPECT_FALSE(set._d.isBitSetRange());
    EXPECT_EQ(BitSetInternal::getSSODenseInfo(&set).startBit(), 32u);
    EXPECT_EQ(set._d.u32_data[0], (BitSetOps::indexAsMask(0) | BitSetOps::indexAsMask(1) | BitSetOps::indexAsMask(3)));

    EXPECT_SUCCESS(set.clearBit(35));
    EXPECT_SUCCESS(set.clearBit(33));
    EXPECT_TRUE(set._d.sso());
    EXPECT_FALSE(set._d.isBitSetRange());
    EXPECT_EQ(BitSetInternal::getSSODenseInfo(&set).startBit(), 32u);
    EXPECT_EQ(set._d.u32_data[0], BitSetOps::indexAsMask(0));

    EXPECT_SUCCESS(set.clearBit(32));
    EXPECT_TRUE(set.empty());
    EXPECT_TRUE(set._d.sso());
    EXPECT_TRUE(set._d.isBitSetRange());

    EXPECT_SUCCESS(set.addBit(0xFFFFFFFEu));
    EXPECT_TRUE(set._d.sso());
    EXPECT_TRUE(set._d.isBitSetRange());

    // Dense SSO representation shouldn't start with a word that would overflow the data.
    EXPECT_SUCCESS(set.addBit(0xFFFFFFFAu));
    EXPECT_TRUE(set._d.sso());
    EXPECT_FALSE(set._d.isBitSetRange());
    EXPECT_EQ(BitSetInternal::getSSODenseInfo(&set).startWord(), kSSOLastWord);
    EXPECT_EQ(set._d.u32_data[0], 0u);
    EXPECT_EQ(set._d.u32_data[1], (BitSetOps::indexAsMask(26) | BitSetOps::indexAsMask(30)));

    EXPECT_SUCCESS(set.addBit(0xFFFFFFD0u));
    EXPECT_TRUE(set._d.sso());
    EXPECT_FALSE(set._d.isBitSetRange());
    EXPECT_EQ(set._d.u32_data[0], BitSetOps::indexAsMask(16));
    EXPECT_EQ(set._d.u32_data[1], (BitSetOps::indexAsMask(26) | BitSetOps::indexAsMask(30)));

    // Clearing the bit in the first word in this case won't shift the offset, as it would overflow addressable words.
    EXPECT_SUCCESS(set.clearBit(0xFFFFFFD0u));
    EXPECT_TRUE(set._d.sso());
    EXPECT_FALSE(set._d.isBitSetRange());
    EXPECT_EQ(set._d.u32_data[0], 0u);
    EXPECT_EQ(set._d.u32_data[1], (BitSetOps::indexAsMask(26) | BitSetOps::indexAsMask(30)));

    // Adding a range that fully subsumes a dense SSO data should result in SSO BitSet.
    EXPECT_SUCCESS(set.clear());
    EXPECT_SUCCESS(set.addBit(64));
    EXPECT_SUCCESS(set.addBit(90));
    EXPECT_SUCCESS(set.addBit(33));
    EXPECT_TRUE(set._d.sso());
    EXPECT_TRUE(set.hasBit(33));
    EXPECT_TRUE(set.hasBit(64));
    EXPECT_TRUE(set.hasBit(90));
    EXPECT_SUCCESS(set.addRange(4, 112));
    EXPECT_TRUE(set._d.sso());
    EXPECT_TRUE(set.hasBit(4));
    EXPECT_TRUE(set.hasBit(111));
    EXPECT_EQ(set, BLBitSet(4, 112));

    EXPECT_SUCCESS(set.chop(5, 111));
    EXPECT_TRUE(set._d.sso());
    EXPECT_FALSE(set.hasBit(4));
    EXPECT_FALSE(set.hasBit(111));
    EXPECT_EQ(set, BLBitSet(5, 111));
  }

  INFO("Checking SSO BitSet ranges");
  {
    uint32_t i;
    BLBitSet set;

    EXPECT_TRUE(set._d.isBitSet());
    EXPECT_TRUE(set._d.sso());
    EXPECT_TRUE(set.empty());

    // This index is invalid in BitSet.
    EXPECT_EQ(set.addBit(0xFFFFFFFFu), BL_ERROR_INVALID_VALUE);

    for (i = 0; i < kNumBits; i++) {
      EXPECT_SUCCESS(set.addBit(i));
      EXPECT_TRUE(set._d.sso());

      BLBitSet range(0, i + 1);
      EXPECT_EQ(set, range);
    }

    EXPECT_SUCCESS(set.clear());
    for (i = 0; i < kNumBits; i++) {
      EXPECT_SUCCESS(set.addBit(kNumBits - i - 1));
      EXPECT_TRUE(set._d.sso());

      BLBitSet range(kNumBits - i - 1, kNumBits);
      EXPECT_EQ(set, range);
    }

    EXPECT_SUCCESS(set.assignRange(0, kNumBits));
    for (i = 0; i < kNumBits; i++) {
      EXPECT_SUCCESS(set.clearBit(i));
      EXPECT_TRUE(set._d.sso());

      BLBitSet range(i + 1, kNumBits);
      EXPECT_EQ(set, range);
    }

    EXPECT_SUCCESS(set.assignRange(0, kNumBits));
    for (i = 0; i < kNumBits; i++) {
      EXPECT_SUCCESS(set.clearBit(kNumBits - i - 1));
      EXPECT_TRUE(set._d.sso());

      BLBitSet range(0, kNumBits - i - 1);
      EXPECT_EQ(set, range);
    }

    EXPECT_SUCCESS(set.clear());
    for (i = 0; i < 65536; i++) {
      uint32_t start = i * 65536;
      uint32_t end = start + 65536;

      if (end == 0)
        end = 0xFFFFFFFFu;

      EXPECT_SUCCESS(set.addRange(start, end));
      EXPECT_TRUE(set._d.sso());

      BLBitSet range(0, end);
      EXPECT_EQ(set, range);
    }

    // Tests whether addRange() handles unions properly.
    EXPECT_SUCCESS(set.clear());
    for (i = 0; i < 65536; i++) {
      uint32_t start = i * 13;
      uint32_t end = i * 65536 + 65536;

      if (end == 0)
        end = 0xFFFFFFFFu;

      EXPECT_SUCCESS(set.addRange(start, end));
      EXPECT_TRUE(set._d.sso());

      BLBitSet range(0, end);
      EXPECT_EQ(set, range);
    }

    // Tests whether addRange() handles adding ranges from the end.
    EXPECT_SUCCESS(set.clear());
    for (i = 0; i < 65536; i++) {
      uint32_t start = (65535 - i) * 65536;
      uint32_t end = start + 65536;

      if (end == 0)
        end = 0xFFFFFFFFu;

      EXPECT_SUCCESS(set.addRange(start, end));
      EXPECT_TRUE(set._d.sso());

      BLBitSet range(start, 0xFFFFFFFFu);
      EXPECT_EQ(set, range);
    }

    // Tests whether clearRange() handles clearing ranges from the end.
    EXPECT_SUCCESS(set.clear());
    EXPECT_SUCCESS(set.assignRange(0, 0xFFFFFFFFu));
    EXPECT_EQ(set.cardinality(), 0xFFFFFFFFu);

    for (i = 0; i < 65536; i++) {
      uint32_t start = (65535 - i) * 65536;
      uint32_t end = start + 65536;

      if (end == 0)
        end = 0xFFFFFFFFu;

      EXPECT_SUCCESS(set.clearRange(start, end));
      EXPECT_TRUE(set._d.sso());

      BLBitSet range(0, start);
      EXPECT_EQ(set, range);
    }
  }

  INFO("Checking SSO BitSet assignWords()");
  {
    BLBitSet set;

    {
      const uint32_t words[] = { 0x80000000u, 0x01010101u };
      EXPECT_SUCCESS(set.assignWords(0, words, BL_ARRAY_SIZE(words)));
      EXPECT_TRUE(set._d.sso());
      EXPECT_EQ(BitSetInternal::getSSOWordIndex(&set), 0u);
      EXPECT_EQ(set._d.u32_data[0], 0x80000000u);
      EXPECT_EQ(set._d.u32_data[1], 0x01010101u);
      EXPECT_EQ(set.cardinality(), 5u);
    }

    {
      const uint32_t words[] = { 0x80000000u, 0x01010101u };
      EXPECT_SUCCESS(set.assignWords(55, words, BL_ARRAY_SIZE(words)));
      EXPECT_TRUE(set._d.sso());
      EXPECT_EQ(BitSetInternal::getSSOWordIndex(&set), 55u);
      EXPECT_EQ(set._d.u32_data[0], 0x80000000u);
      EXPECT_EQ(set._d.u32_data[1], 0x01010101u);
      EXPECT_EQ(set.cardinality(), 5u);
    }

    {
      const uint32_t words[] = { 0x00000000u, 0x80000000u, 0x01010101u };
      EXPECT_SUCCESS(set.assignWords(0, words, BL_ARRAY_SIZE(words)));
      EXPECT_TRUE(set._d.sso());
      EXPECT_EQ(BitSetInternal::getSSOWordIndex(&set), 1u);
      EXPECT_EQ(set._d.u32_data[0], 0x80000000u);
      EXPECT_EQ(set._d.u32_data[1], 0x01010101u);
      EXPECT_EQ(set.cardinality(), 5u);
    }

    {
      const uint32_t words[] = { 0x00000000u, 0x80000000u, 0x01010101u, 0x00000000u };
      EXPECT_SUCCESS(set.assignWords(0, words, BL_ARRAY_SIZE(words)));
      EXPECT_TRUE(set._d.sso());
      EXPECT_EQ(BitSetInternal::getSSOWordIndex(&set), 1u);
      EXPECT_EQ(set._d.u32_data[0], 0x80000000u);
      EXPECT_EQ(set._d.u32_data[1], 0x01010101u);
      EXPECT_EQ(set.cardinality(), 5u);
    }

    {
      const uint32_t words[] = { 0x00000000u, 0x00000000u, 0x80000000u, 0x01010101u, 0x00000000u, 0x00000000u };
      EXPECT_SUCCESS(set.assignWords(0, words, BL_ARRAY_SIZE(words)));
      EXPECT_TRUE(set._d.sso());
      EXPECT_EQ(BitSetInternal::getSSOWordIndex(&set), 2u);
      EXPECT_EQ(set._d.u32_data[0], 0x80000000u);
      EXPECT_EQ(set._d.u32_data[1], 0x01010101u);
      EXPECT_EQ(set.cardinality(), 5u);
    }

    {
      const uint32_t words[] = { 0xFFFF0000u };
      EXPECT_SUCCESS(set.assignWords(BitSetInternal::kLastWord, words, BL_ARRAY_SIZE(words)));
      EXPECT_TRUE(set._d.sso());
      EXPECT_EQ(BitSetInternal::getSSOWordIndex(&set), BitSetInternal::kSSOLastWord);
      EXPECT_EQ(set._d.u32_data[0], 0x00000000u);
      EXPECT_EQ(set._d.u32_data[1], 0xFFFF0000u);
      EXPECT_EQ(set.cardinality(), 16u);
    }

    {
      const uint32_t words[] = { 0x0000FFFFu, 0xFFFF0000u };
      EXPECT_SUCCESS(set.assignWords(BitSetInternal::kLastWord - 1, words, BL_ARRAY_SIZE(words)));
      EXPECT_TRUE(set._d.sso());
      EXPECT_EQ(BitSetInternal::getSSOWordIndex(&set), BitSetInternal::kSSOLastWord);
      EXPECT_EQ(set._d.u32_data[0], 0x0000FFFFu);
      EXPECT_EQ(set._d.u32_data[1], 0xFFFF0000u);
      EXPECT_EQ(set.cardinality(), 32u);
    }

    // Last index of SSO Dense BitSet must be kSSOLastWord even when the first word would be zero.
    // The reason is that if we allowed a higher index it would be possible to address words, which
    // are outside of the addressable range, which is [0, 4294967296).
    {
      const uint32_t words[] = { 0x00000000u, 0x0000FFFFu, 0xFFFF0000u };
      EXPECT_SUCCESS(set.assignWords(BitSetInternal::kLastWord - 2, words, BL_ARRAY_SIZE(words)));
      EXPECT_TRUE(set._d.sso());
      EXPECT_EQ(BitSetInternal::getSSOWordIndex(&set), BitSetInternal::kSSOLastWord);
      EXPECT_EQ(set._d.u32_data[0], 0x0000FFFFu);
      EXPECT_EQ(set._d.u32_data[1], 0xFFFF0000u);
      EXPECT_EQ(set.cardinality(), 32u);
    }

    {
      const uint32_t words[] = { 0x00000000u, 0x00000000u, 0x0000FFFFu, 0xFFFF0000u };
      EXPECT_SUCCESS(set.assignWords(BitSetInternal::kLastWord - 3, words, BL_ARRAY_SIZE(words)));
      EXPECT_TRUE(set._d.sso());
      EXPECT_EQ(BitSetInternal::getSSOWordIndex(&set), BitSetInternal::kSSOLastWord);
      EXPECT_EQ(set._d.u32_data[0], 0x0000FFFFu);
      EXPECT_EQ(set._d.u32_data[1], 0xFFFF0000u);
      EXPECT_EQ(set.cardinality(), 32u);
    }

    // BitSet should refuse words, which are outside of the addressable range.
    {
      const uint32_t words[] = { 0x0000FFFFu, 0xFFFF0000u };
      EXPECT_EQ(set.assignWords(BitSetInternal::kLastWord, words, BL_ARRAY_SIZE(words)), BL_ERROR_INVALID_VALUE);
    }
  }

  INFO("Checking SSO BitSet chop()");
  {
    BLBitSet set;

    // Range BitSet.
    EXPECT_SUCCESS(set.addRange(0, 1000));

    EXPECT_TRUE(set._d.sso());
    EXPECT_EQ(set.cardinality(), 1000u);

    for (uint32_t i = 0; i < 1000; i++) {
      EXPECT_SUCCESS(set.chop(i, 1000));
      EXPECT_EQ(set.cardinality(), 1000u - i);

      uint32_t start, end;
      EXPECT_TRUE(set.getRange(&start, &end));
      EXPECT_EQ(start, i);
      EXPECT_EQ(end, 1000u);
    }

    EXPECT_SUCCESS(set.clear());

    // Dense BitSet.
    for (uint32_t i = 0; i < 96; i += 2) {
      EXPECT_SUCCESS(set.addBit(i));
    }

    for (uint32_t i = 0; i < 96; i++) {
      EXPECT_SUCCESS(set.chop(i, 96));
      EXPECT_EQ(set.cardinality(), 96 / 2u - ((i + 1) / 2u));
    }
  }

  INFO("Checking SSO BitSet hasBitsInRange() & cardinalityInRange()");
  {
    BLBitSet set;

    // Dense SSO range data will describe bits in range [992, 1088) - of words range [31, 34).
    EXPECT_SUCCESS(set.addRange(1000, 1022));
    EXPECT_SUCCESS(set.addRange(1029, 1044));
    EXPECT_SUCCESS(set.addBit(1055));

    EXPECT_TRUE(set._d.sso());
    EXPECT_EQ(set.cardinality(), 38u);

    EXPECT_EQ(set.cardinalityInRange(0, 50), 0u);
    EXPECT_EQ(set.cardinalityInRange(0, 992), 0u);
    EXPECT_EQ(set.cardinalityInRange(0, 1000), 0u);

    EXPECT_EQ(set.cardinalityInRange(1000, 1001), 1u);
    EXPECT_EQ(set.cardinalityInRange(1000, 1010), 10u);
    EXPECT_EQ(set.cardinalityInRange(1000, 1029), 22u);
    EXPECT_EQ(set.cardinalityInRange(1000, 1040), 33u);
    EXPECT_EQ(set.cardinalityInRange(1000, 1100), 38u);

    EXPECT_EQ(set.cardinalityInRange(1050, 2000), 1u);
  }

  INFO("Checking dynamic BitSet basics");
  {
    uint32_t i;
    BLBitSet set;

    for (i = 0; i < kNumBits; i += 2) {
      EXPECT_FALSE(set.hasBit(i));
      EXPECT_SUCCESS(set.addBit(i));
      EXPECT_TRUE(set.hasBit(i));
      EXPECT_FALSE(set.hasBit(i + 1));
    }

    for (i = 0; i < kNumBits; i += 2) {
      EXPECT_TRUE(set.hasBit(i));
      EXPECT_SUCCESS(set.clearBit(i));
      EXPECT_FALSE(set.hasBit(i));
    }

    for (i = 0; i < kNumBits; i += 2) {
      EXPECT_FALSE(set.hasBit(kNumBits - i));
      EXPECT_SUCCESS(set.addBit(kNumBits - i));
      EXPECT_TRUE(set.hasBit(kNumBits - i));
    }

    for (i = 0; i < kNumBits; i += 2) {
      EXPECT_TRUE(set.hasBit(kNumBits - i));
      EXPECT_SUCCESS(set.clearBit(kNumBits - i));
      EXPECT_FALSE(set.hasBit(kNumBits - i));
    }

    EXPECT_SUCCESS(set.reset());

    for (i = 0; i < kNumBits; i += 4) {
      EXPECT_SUCCESS(set.addRange(i, i + 3));
      EXPECT_TRUE(set.hasBit(i));
      EXPECT_TRUE(set.hasBit(i + 1));
      EXPECT_TRUE(set.hasBit(i + 2));
      EXPECT_FALSE(set.hasBit(i + 3));

      EXPECT_SUCCESS(set.clearBit(i));
      EXPECT_FALSE(set.hasBit(i));
      EXPECT_SUCCESS(set.clearRange(i, i + 2));
      EXPECT_FALSE(set.hasBit(i));
      EXPECT_FALSE(set.hasBit(i + 1));
      EXPECT_TRUE(set.hasBit(i + 2));
      EXPECT_FALSE(set.hasBit(i + 3));

      EXPECT_SUCCESS(set.addRange(i + 1, i + 4));
      EXPECT_FALSE(set.hasBit(i));
      EXPECT_TRUE(set.hasBit(i + 1));
      EXPECT_TRUE(set.hasBit(i + 2));
      EXPECT_TRUE(set.hasBit(i + 3));
    }

    for (i = 0; i < kNumBits; i += 4) {
      EXPECT_FALSE(set.hasBit(i));
      EXPECT_TRUE(set.hasBit(i + 1));
      EXPECT_TRUE(set.hasBit(i + 2));
      EXPECT_TRUE(set.hasBit(i + 3));
    }
  }

  INFO("Checking dynamic BitSet addRange() & clearRange()");
  {
    uint32_t i;
    BLBitSet set;

    // Add {0-10000} and {20000-30000} range and then add overlapping range.
    EXPECT_SUCCESS(set.addRange(0, 10000));
    for (i = 0; i < 10000; i++)
      EXPECT_TRUE(set.hasBit(i));
    EXPECT_FALSE(set.hasBit(10000));

    EXPECT_SUCCESS(set.addRange(20000, 30000));
    for (i = 0; i < 10000; i++)
      EXPECT_TRUE(set.hasBit(i));
    for (i = 20000; i < 30000; i++)
      EXPECT_TRUE(set.hasBit(i));
    EXPECT_FALSE(set.hasBit(30000));
    EXPECT_EQ(set.segmentCount(), 5u);

    EXPECT_SUCCESS(set.addRange(6001, 23999));
    for (i = 0; i < 30000; i++)
      EXPECT_TRUE(set.hasBit(i));
    EXPECT_FALSE(set.hasBit(30000));
    EXPECT_EQ(set.segmentCount(), 2u);

    // Turns dense segments into a range ending with a dense segment.
    EXPECT_SUCCESS(set.reset());
    for (i = 0; i < 10000; i += 2)
      EXPECT_SUCCESS(set.addBit(i));
    EXPECT_EQ(set.segmentCount(), 79u);
    EXPECT_SUCCESS(set.addRange(0, 10000));
    for (i = 0; i < 10000; i++)
      EXPECT_TRUE(set.hasBit(i));
    EXPECT_EQ(set.segmentCount(), 2u);

    // Sparse bits to ranges.
    EXPECT_SUCCESS(set.reset());
    for (i = 1000; i < 10000000; i += 100000)
      EXPECT_SUCCESS(set.addBit(i));
    for (i = 1000; i < 10000000; i += 100000)
      EXPECT_TRUE(set.hasBit(i));
    for (i = 1000; i < 10000000; i += 100000)
      EXPECT_SUCCESS(set.addRange(i - 500, i + 500));

    // Verify that clearRange() correctly inserts 4 segments.
    EXPECT_SUCCESS(set.reset());
    EXPECT_SUCCESS(set.addRange(0, 1024 * 1024));
    EXPECT_SUCCESS(set.clearRange(1023, 9999));
    EXPECT_EQ(set.segmentCount(), 4u);

    // Verify that clearRange() correctly inserts 3 segments.
    EXPECT_SUCCESS(set.reset());
    EXPECT_SUCCESS(set.addRange(0, 1024 * 1024));
    EXPECT_SUCCESS(set.clearRange(1024, 9999));
    EXPECT_EQ(set.segmentCount(), 3u);

    // Verify that clearRange() correctly inserts 2 segments.
    EXPECT_SUCCESS(set.reset());
    EXPECT_SUCCESS(set.addRange(0, 1024 * 1024));
    EXPECT_SUCCESS(set.clearRange(1024, 4096));
    EXPECT_EQ(set.segmentCount(), 2u);

    // Verify that clearRange() correctly inserts 1 segment.
    EXPECT_SUCCESS(set.reset());
    EXPECT_SUCCESS(set.addRange(0, 1024 * 1024));
    EXPECT_SUCCESS(set.clearRange(0, 4096));
    EXPECT_EQ(set.segmentCount(), 1u);
  }

  INFO("Checking dynamic BitSet assignWords()");
  {
    BLBitSet set;
    uint32_t startBit, endBit;

    {
      static const uint32_t words[] = {
        0x80000000u, 0x01010101u, 0x02020202u, 0x04040404u
      };

      EXPECT_SUCCESS(set.assignWords(0, words, BL_ARRAY_SIZE(words)));
      EXPECT_FALSE(set._d.sso());
      EXPECT_EQ(set.segmentCount(), 1u);
      EXPECT_EQ(set.cardinality(), 13u);

      EXPECT_TRUE(set.getRange(&startBit, &endBit));
      EXPECT_EQ(startBit, 0u);
      EXPECT_EQ(endBit, 126u);

      EXPECT_SUCCESS(set.assignWords(33311, words, BL_ARRAY_SIZE(words)));
      EXPECT_FALSE(set._d.sso());
      EXPECT_EQ(set.segmentCount(), 2u);
      EXPECT_EQ(set.cardinality(), 13u);

      EXPECT_TRUE(set.getRange(&startBit, &endBit));
      EXPECT_EQ(startBit, 1065952u);
      EXPECT_EQ(endBit, 1065952u + 126u);
    }

    // Test whether assignWords() results in a Range segment, when possible.
    {
      BLBitSet tmp;

      static const uint32_t words[] = {
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu
      };

      // NOTE: 'set' is already dynamic, assignWords() will not turn it to SSO if it's mutable.
      EXPECT_SUCCESS(set.assignWords(0, words, BL_ARRAY_SIZE(words)));
      EXPECT_FALSE(set._d.sso());
      EXPECT_EQ(set.segmentCount(), 1u);
      EXPECT_EQ(set.cardinality(), 512u);

      // NOTE: 'tmp' is SSO, if assignWords() forms a range, it will be setup as SSO range.
      EXPECT_SUCCESS(tmp.assignWords(0, words, BL_ARRAY_SIZE(words)));
      EXPECT_TRUE(tmp._d.sso());
      EXPECT_EQ(tmp.cardinality(), 512u);

      EXPECT_TRUE(set.equals(tmp));

      // Verify whether assignWords() works well with arguments not aligned to a segment boundary.
      EXPECT_SUCCESS(set.assignWords(33, words, BL_ARRAY_SIZE(words)));
      EXPECT_FALSE(set._d.sso());
      EXPECT_EQ(set.segmentCount(), 3u);
      EXPECT_EQ(set.cardinality(), 512u);

      EXPECT_SUCCESS(tmp.assignWords(33, words, BL_ARRAY_SIZE(words)));
      EXPECT_TRUE(tmp._d.sso());
      EXPECT_EQ(tmp.cardinality(), 512u);

      EXPECT_TRUE(set.equals(tmp));
    }
  }

  INFO("Checking dynamic BitSet addWords() - small BitSet");
  {
    BLBitSet set;

    {
      static const uint32_t words[] = { 0x80000000u, 0x01010101u, 0x02020202u, 0x04040404u };
      static const uint32_t range[] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, };

      static const uint32_t check1[] = { 0x80000000u, 0x81010101u, 0x03030303u, 0x06060606u, 0x04040404u };
      static const uint32_t check2[] = { 0x80000000u, 0x81010101u, 0x83030303u, 0x07070707u, 0x06060606u, 0x04040404u };

      EXPECT_SUCCESS(set.addWords(8, words, BL_ARRAY_SIZE(words)));
      EXPECT_SUCCESS(set.addWords(9, words, BL_ARRAY_SIZE(words)));

      EXPECT_FALSE(set._d.sso());
      EXPECT_EQ(set.segmentCount(), 2u);
      EXPECT_EQ(set.cardinality(), 26u);
      testBits(set, 8, check1, BL_ARRAY_SIZE(check1));

      EXPECT_SUCCESS(set.addWords(7, words, BL_ARRAY_SIZE(words)));
      EXPECT_EQ(set.segmentCount(), 3u);
      EXPECT_EQ(set.cardinality(), 39u);
      testBits(set, 7, check2, BL_ARRAY_SIZE(check2));

      // Adding a range of words that don't overlap with existing segments must create a range segment.
      EXPECT_SUCCESS(set.addWords(32, range, BL_ARRAY_SIZE(range)));
      EXPECT_EQ(set.segmentCount(), 4u);
      EXPECT_EQ(set.cardinality(), 39u + 32 * 8);
      testBits(set, 7, check2, BL_ARRAY_SIZE(check2));
      testBits(set, 32, range, BL_ARRAY_SIZE(range));
    }
  }

  INFO("Checking dynamic BitSet addWords() - large BitSet");
  {
    BLBitSet set;
    BLRandom rnd(0x1234);

    constexpr uint32_t kIterationCount = 1000;
    constexpr uint32_t kWordCount = 33;

    for (uint32_t i = 0; i < kIterationCount; i++) {
      uint32_t wordIndex = rnd.nextUInt32() & 0xFFFFu;
      uint32_t wordData[kWordCount];

      // Random pattern... But we also want 0 and all bits set.
      uint32_t pattern = rnd.nextUInt32();
      if (pattern < 0x20000000u)
        pattern = 0u;
      else if (pattern > 0xF0000000)
        pattern = 0xFFFFFFFF;

      for (uint32_t j = 0; j < kWordCount; j++) {
        wordData[j] = pattern;
      }

      set.addWords(wordIndex, wordData, kWordCount);
    }
  }

  INFO("Checking dynamic BitSet addWords() - consecutive");
  {
    BLBitSet set;
    BLRandom rnd(0x1234);

    constexpr uint32_t kIterationCount = 1000;
    constexpr uint32_t kWordCount = 33;

    uint32_t cardinality = 0;

    for (uint32_t i = 0; i < kIterationCount; i++) {
      uint32_t wordData[kWordCount];

      // Random pattern... But we also want 0 and all bits set.
      uint32_t pattern = rnd.nextUInt32();
      if (pattern < 0x20000000u)
        pattern = 0u;
      else if (pattern > 0xF0000000)
        pattern = 0xFFFFFFFF;

      for (uint32_t j = 0; j < kWordCount; j++) {
        wordData[j] = pattern;
      }

      set.addWords(i * kWordCount, wordData, kWordCount);
      cardinality += IntOps::popCount(pattern) * kWordCount;
    }

    EXPECT_EQ(set.cardinality(), cardinality);
  }

  INFO("Checking dynamic BitSet chop()");
  {
    BLBitSet set;

    for (uint32_t i = 0; i < kNumBits; i += 2) {
      EXPECT_SUCCESS(set.addBit(i));
    }
    EXPECT_FALSE(set._d.sso());
    EXPECT_EQ(set.cardinality(), kNumBits / 2u);

    for (uint32_t i = 0; i < kNumBits / 2; i += 2) {
      EXPECT_TRUE(set.hasBit(i));
      EXPECT_SUCCESS(set.chop(i + 1, kNumBits));
      EXPECT_FALSE(set.hasBit(i));
      EXPECT_TRUE(set.hasBit(i + 2));
    }

    for (uint32_t i = kNumBits - 2; i > kNumBits / 2; i -= 2) {
      EXPECT_TRUE(set.hasBit(i));
      EXPECT_SUCCESS(set.chop(0, i));
      EXPECT_FALSE(set.hasBit(i));
      EXPECT_TRUE(set.hasBit(i - 2));
    }

    // BitSet should end up having a single segment having a single bit set.
    EXPECT_TRUE(set.hasBit(kNumBits / 2u));
    EXPECT_EQ(set.segmentCount(), 1u);
    EXPECT_EQ(set.cardinality(), 1u);

    // Let's create a range segment and try to chop it.
    EXPECT_SUCCESS(set.clear());
    EXPECT_EQ(set.segmentCount(), 0u);
    EXPECT_EQ(set.cardinality(), 0u);

    EXPECT_SUCCESS(set.addRange(0, 512));
    EXPECT_SUCCESS(set.addRange(1024, 2048));
    EXPECT_SUCCESS(set.addRange(4096, 8192));
    EXPECT_EQ(set.segmentCount(), 3u);
    EXPECT_EQ(set.cardinality(), 512u + 1024u + 4096u);

    EXPECT_SUCCESS(set.chop(1025, 2047));
    EXPECT_EQ(set.segmentCount(), 3u);
    EXPECT_EQ(set.cardinality(), 1022u);
  }

  INFO("Checking dynamic BitSet hasBitsInRange() & cardinalityInRange()");
  {
    BLBitSet set;

    EXPECT_SUCCESS(set.addRange(0, 512));
    EXPECT_SUCCESS(set.addRange(1024, 2048));
    EXPECT_SUCCESS(set.addRange(4096, 8192));

    EXPECT_TRUE(set.hasBitsInRange(0, 1));
    EXPECT_TRUE(set.hasBitsInRange(0, 512));
    EXPECT_TRUE(set.hasBitsInRange(0, 8192));
    EXPECT_TRUE(set.hasBitsInRange(444, 600));
    EXPECT_TRUE(set.hasBitsInRange(500, 600));
    EXPECT_TRUE(set.hasBitsInRange(1000, 2000));

    EXPECT_FALSE(set.hasBitsInRange(512, 600));
    EXPECT_FALSE(set.hasBitsInRange(512, 1024));
    EXPECT_FALSE(set.hasBitsInRange(2048, 4096));
    EXPECT_FALSE(set.hasBitsInRange(3000, 4011));

    for (uint32_t i = 0; i < 512; i++) {
      EXPECT_EQ(set.cardinalityInRange(0, i), i);
      EXPECT_EQ(set.hasBitsInRange(0, i), i > 0);
    }

    for (uint32_t i = 0; i < 512; i++) {
      EXPECT_EQ(set.cardinalityInRange(i, 512), 512 - i);
      EXPECT_TRUE(set.hasBitsInRange(i, 512));
    }

    for (uint32_t i = 0; i < 1024; i++) {
      EXPECT_EQ(set.cardinalityInRange(1024, 1024 + i), i);
      EXPECT_EQ(set.hasBitsInRange(1024, 1024 + i), i > 0);
    }

    for (uint32_t i = 0; i < 4096; i++) {
      EXPECT_EQ(set.cardinalityInRange(4096, 4096 + i), i);
      EXPECT_EQ(set.hasBitsInRange(4096, 4096 + i), i > 0);
    }

    for (uint32_t i = 0; i < 8192; i++) {
      uint32_t expectedCardinality;
      if (i < 1024)
        expectedCardinality = 4096 + 1024 + 512 - blMin<uint32_t>(i, 512u);
      else if (i < 4096)
        expectedCardinality = 4096 + 1024 - blMin(i - 1024u, 1024u);
      else
        expectedCardinality = 8192 - i;

      EXPECT_EQ(set.cardinalityInRange(i, 8192), expectedCardinality);
      EXPECT_TRUE(set.hasBitsInRange(i, 8192));
    }
  }

  INFO("Checking functionality of shrink() & optimize()");
  {
    BLBitSet set;
    uint32_t kCount = BitSetInternal::kSegmentBitCount * 100;

    for (uint32_t i = 0; i < kCount; i += 2)
      EXPECT_SUCCESS(set.addBit(i));

    EXPECT_EQ(set.cardinality(), kCount / 2);

    for (uint32_t i = 0; i < kCount; i += 2)
      EXPECT_SUCCESS(set.addBit(i + 1));

    EXPECT_FALSE(set._d.sso());
    EXPECT_EQ(set.cardinality(), kCount);
    EXPECT_GT(set.segmentCount(), 1u);

    EXPECT_SUCCESS(set.optimize());
    EXPECT_EQ(set.segmentCount(), 1u);
    EXPECT_FALSE(set._d.sso());
    EXPECT_EQ(set.cardinality(), kCount);

    EXPECT_SUCCESS(set.shrink());
    EXPECT_TRUE(set._d.sso());
    EXPECT_EQ(set.cardinality(), kCount);
  }

  INFO("Checking functionality of subsumes() & intersects()");
  {
    BLBitSet a;
    BLBitSet b;
    BLBitSet c;
    BLBitSet empty;

    EXPECT_SUCCESS(a.assignRange(10, 100));
    EXPECT_SUCCESS(b.assignRange(10, 100));
    EXPECT_TRUE(a.subsumes(b));
    EXPECT_TRUE(b.subsumes(a));
    EXPECT_TRUE(a.intersects(b));
    EXPECT_TRUE(b.intersects(a));

    EXPECT_SUCCESS(b.assignRange(11, 100));
    EXPECT_TRUE(a.subsumes(b));
    EXPECT_FALSE(b.subsumes(a));
    EXPECT_TRUE(a.intersects(b));
    EXPECT_TRUE(b.intersects(a));

    EXPECT_SUCCESS(b.assignRange(10, 99));
    EXPECT_TRUE(a.subsumes(b));
    EXPECT_FALSE(b.subsumes(a));
    EXPECT_TRUE(a.intersects(b));
    EXPECT_TRUE(b.intersects(a));

    EXPECT_SUCCESS(a.assignRange(10, 100));
    EXPECT_SUCCESS(b.assignRange(1000, 10000));
    EXPECT_FALSE(a.subsumes(b));
    EXPECT_FALSE(b.subsumes(a));
    EXPECT_FALSE(a.intersects(b));
    EXPECT_FALSE(b.intersects(a));

    static const uint32_t aSSOWords[] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFF8 };
    static const uint32_t bSSOWords[] = { 0xFFFF0000u, 0x0000FFFFu, 0xFFFFFFF8 };

    EXPECT_SUCCESS(a.assignWords(0, aSSOWords, BL_ARRAY_SIZE(aSSOWords)));
    EXPECT_SUCCESS(b.assignWords(0, bSSOWords, BL_ARRAY_SIZE(bSSOWords)));
    EXPECT_SUCCESS(c.assignRange(16, 32));

    EXPECT_TRUE(a.subsumes(empty));
    EXPECT_TRUE(b.subsumes(empty));
    EXPECT_TRUE(c.subsumes(empty));

    EXPECT_FALSE(a.intersects(empty));
    EXPECT_FALSE(b.intersects(empty));
    EXPECT_FALSE(c.intersects(empty));

    EXPECT_TRUE(a.subsumes(b));
    EXPECT_TRUE(a.subsumes(c));
    EXPECT_FALSE(b.subsumes(a));
    EXPECT_FALSE(b.subsumes(c));

    EXPECT_TRUE(a.intersects(b));
    EXPECT_TRUE(a.intersects(c));
    EXPECT_TRUE(b.intersects(a));
    EXPECT_FALSE(b.intersects(c));

    static const uint32_t aDynamicWords[] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFF8, 0x00000000u, 0xFFFF0000, 0xFFFFFFFFu, 0xFFFFFFFFu };
    static const uint32_t bDynamicWords[] = { 0xFFFF0000u, 0x0000FFFFu, 0xFFFFFFF8, 0x00000000u, 0x00FF0000, 0xFF000000u, 0x00000000u };

    EXPECT_SUCCESS(a.assignWords(31, aDynamicWords, BL_ARRAY_SIZE(aDynamicWords)));
    EXPECT_SUCCESS(b.assignWords(31, bDynamicWords, BL_ARRAY_SIZE(bDynamicWords)));
    EXPECT_SUCCESS(c.assignRange(992, 1184));

    EXPECT_TRUE(a.subsumes(empty));
    EXPECT_TRUE(b.subsumes(empty));
    EXPECT_TRUE(c.subsumes(empty));

    EXPECT_FALSE(a.intersects(empty));
    EXPECT_FALSE(b.intersects(empty));
    EXPECT_FALSE(c.intersects(empty));

    EXPECT_TRUE(a.subsumes(b));
    EXPECT_FALSE(a.subsumes(c));
    EXPECT_FALSE(b.subsumes(a));
    EXPECT_FALSE(b.subsumes(c));

    EXPECT_FALSE(c.subsumes(a));
    EXPECT_TRUE(c.subsumes(b));
  }

  INFO("Checking functionality of BLBitSetBuilder");
  {
    BLBitSet set;

    {
      BLBitSetBuilder builder(&set);
      EXPECT_SUCCESS(builder.addBit(1024));
      EXPECT_SUCCESS(builder.addBit(1025));
      EXPECT_SUCCESS(builder.addBit(1125));
      EXPECT_SUCCESS(builder.addBit(1126));
      EXPECT_SUCCESS(builder.addRange(1080, 1126));
      EXPECT_SUCCESS(builder.commit());
    }

    EXPECT_TRUE(set.hasBit(1024));
    EXPECT_TRUE(set.hasBit(1025));
    EXPECT_TRUE(set.hasBit(1080));
    EXPECT_TRUE(set.hasBit(1126));
    EXPECT_EQ(set.cardinality(), 49u);
    EXPECT_EQ(set.cardinalityInRange(1024, 1127), 49u);

    {
      BLBitSetBuilder builder(&set);
      for (uint32_t i = 0; i < 4096; i += 2) {
        EXPECT_SUCCESS(builder.addBit(4096 + i));
      }
      EXPECT_SUCCESS(builder.commit());
    }

    EXPECT_EQ(set.cardinality(), 49u + 2048u);
    EXPECT_EQ(set.cardinalityInRange(1024, 8192), 49u + 2048u);
  }

  INFO("Checking functionality of BLBitSetWordIterator");
  {
    // SSO Range BitSet.
    {
      BLBitSet set;
      EXPECT_SUCCESS(set.addRange(130, 200));

      BLBitSetWordIterator wordIterator(set);
      EXPECT_EQ(wordIterator.nextWord(), 0x3FFFFFFFu);
      EXPECT_EQ(wordIterator.bitIndex(), 128u);
      EXPECT_EQ(wordIterator.nextWord(), 0xFFFFFFFFu);
      EXPECT_EQ(wordIterator.bitIndex(), 160u);
      EXPECT_EQ(wordIterator.nextWord(), 0xFF000000u);
      EXPECT_EQ(wordIterator.bitIndex(), 192u);
      EXPECT_EQ(wordIterator.nextWord(), 0u);
    }

    // SSO Dense BitSet.
    {
      BLBitSet set;
      EXPECT_SUCCESS(set.addRange(130, 140));
      EXPECT_SUCCESS(set.addRange(180, 200));

      BLBitSetWordIterator wordIterator(set);
      EXPECT_EQ(wordIterator.nextWord(), 0x3FF00000u);
      EXPECT_EQ(wordIterator.bitIndex(), 128u);
      EXPECT_EQ(wordIterator.nextWord(), 0x00000FFFu);
      EXPECT_EQ(wordIterator.bitIndex(), 160u);
      EXPECT_EQ(wordIterator.nextWord(), 0xFF000000u);
      EXPECT_EQ(wordIterator.bitIndex(), 192u);
      EXPECT_EQ(wordIterator.nextWord(), 0u);
    }

    // Dynamic BitSet.
    {
      BLBitSet set;
      EXPECT_SUCCESS(set.addRange(130, 140));
      EXPECT_SUCCESS(set.addRange(1024, 1025));
      EXPECT_SUCCESS(set.addRange(2050, 2060));

      BLBitSetWordIterator wordIterator(set);
      EXPECT_EQ(wordIterator.nextWord(), 0x3FF00000u);
      EXPECT_EQ(wordIterator.bitIndex(), 128u);
      EXPECT_EQ(wordIterator.nextWord(), 0x80000000u);
      EXPECT_EQ(wordIterator.bitIndex(), 1024u);
      EXPECT_EQ(wordIterator.nextWord(), 0x3FF00000u);
      EXPECT_EQ(wordIterator.bitIndex(), 2048u);
      EXPECT_EQ(wordIterator.nextWord(), 0u);
    }
  }
}

} // {Tests}
} // {bl}

#endif // BL_TEST
