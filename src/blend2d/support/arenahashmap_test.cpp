// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_test_p.h"
#if defined(BL_TEST)

#include "../support/arenaallocator_p.h"
#include "../support/arenahashmap_p.h"

// bl::ArenaHashMap - Tests
// ========================

namespace bl {
namespace Tests {

struct MyHashMapNode : public ArenaHashMapNode {
  inline MyHashMapNode(uint32_t key) noexcept
    : ArenaHashMapNode(key),
      _key(key) {}

  uint32_t _key;
};

struct MyKeyMatcher {
  inline MyKeyMatcher(uint32_t key) noexcept
    : _key(key) {}

  inline uint32_t hashCode() const noexcept { return _key; }
  inline bool matches(const MyHashMapNode* node) const noexcept { return node->_key == _key; }

  uint32_t _key;
};

UNIT(arena_hashmap, BL_TEST_GROUP_SUPPORT_CONTAINERS) {
  uint32_t kCount = BrokenAPI::hasArg("--quick") ? 1000 : 10000;

  ArenaAllocator allocator(4096);
  ArenaHashMap<MyHashMapNode> hashTable(&allocator);
  uint32_t key;

  INFO("Inserting %u elements to HashTable", unsigned(kCount));
  for (key = 0; key < kCount; key++) {
    hashTable.insert(allocator.newT<MyHashMapNode>(key));
  }

  INFO("Removing %u elements from HashTable and validating each operation", unsigned(kCount));
  uint32_t count = kCount;
  do {
    MyHashMapNode* node;

    for (key = 0; key < count; key++) {
      node = hashTable.get(MyKeyMatcher(key));
      EXPECT_NE(node, nullptr);
      EXPECT_EQ(node->_key, key);
    }

    {
      count--;
      node = hashTable.get(MyKeyMatcher(count));
      hashTable.remove(node);

      node = hashTable.get(MyKeyMatcher(count));
      EXPECT_EQ(node, nullptr);
    }
  } while (count);

  EXPECT_TRUE(hashTable.empty());
}

} // {Tests}
} // {bl}

#endif // BL_TEST
