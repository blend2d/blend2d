// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/axl/arena_allocator_p.h>
#include <blend2d/axl/arena_hash_map_p.h>

// bl::ArenaHashMap - Tests
// ========================

namespace bl::Tests {

struct MyHashMapNode : public ArenaHashMapNode {
  inline MyHashMapNode(uint32_t key) noexcept
    : ArenaHashMapNode(key),
      _key(key) {}

  uint32_t _key;
};

struct MyKeyMatcher {
  inline MyKeyMatcher(uint32_t key) noexcept
    : _key(key) {}

  inline uint32_t hash_code() const noexcept { return _key; }
  inline bool matches(const MyHashMapNode* node) const noexcept { return node->_key == _key; }

  uint32_t _key;
};

TEST_CASE(support_arena_hash_map) {
  uint32_t kCount = ZeroTest::test_runner()->cmd_line().has_arg("--quick") ? 1000 : 10000;

  ArenaAllocator allocator(4096);
  ArenaHashMap<MyHashMapNode> hash_table(&allocator);
  uint32_t key;

  TEST_LOG("Inserting %u elements to HashTable", unsigned(kCount));
  for (key = 0; key < kCount; key++) {
    hash_table.insert(allocator.new_t<MyHashMapNode>(key));
  }

  TEST_LOG("Removing %u elements from HashTable and validating each operation", unsigned(kCount));
  uint32_t count = kCount;
  do {
    MyHashMapNode* node;

    for (key = 0; key < count; key++) {
      node = hash_table.get(MyKeyMatcher(key));
      EXPECT_NE(node, nullptr);
      EXPECT_EQ(node->_key, key);
    }

    {
      count--;
      node = hash_table.get(MyKeyMatcher(count));
      hash_table.remove(node);

      node = hash_table.get(MyKeyMatcher(count));
      EXPECT_EQ(node, nullptr);
    }
  } while (count);

  EXPECT_TRUE(hash_table.is_empty());
}

} // {bl::Tests}

#endif // BL_TEST
