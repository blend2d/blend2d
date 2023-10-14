// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_test_p.h"
#if defined(BL_TEST)

#include "../support/arenaallocator_p.h"
#include "../support/arenatree_p.h"

// bl::ArenaTree - Tests
// =====================

namespace bl {
namespace Tests {

template<typename NodeT>
struct TreeTestUtils {
  typedef ArenaTree<NodeT> Tree;

  static void verifyTree(Tree& tree) noexcept {
    EXPECT_GT(checkHeight(static_cast<NodeT*>(tree._root)), 0);
  }

  // Check whether the Red-Black tree is valid.
  static int checkHeight(NodeT* node) noexcept {
    if (!node) return 1;

    NodeT* ln = node->left();
    NodeT* rn = node->right();

    // Invalid tree.
    EXPECT_TRUE(ln == nullptr || *ln < *node);
    EXPECT_TRUE(rn == nullptr || *rn > *node);

    // Red violation.
    EXPECT_TRUE(!node->isRed() || (!ArenaTreeNodeBase::_isValidRed(ln) && !ArenaTreeNodeBase::_isValidRed(rn)));

    // Black violation.
    int lh = checkHeight(ln);
    int rh = checkHeight(rn);
    EXPECT_TRUE(!lh || !rh || lh == rh);

    // Only count black links.
    return (lh && rh) ? lh + !node->isRed() : 0;
  }
};

class MyTreeNode : public ArenaTreeNode<MyTreeNode> {
public:
  BL_NONCOPYABLE(MyTreeNode)

  BL_INLINE explicit MyTreeNode(uint32_t key) noexcept
    : _key(key) {}

  BL_INLINE bool operator<(const MyTreeNode& other) const noexcept { return _key < other._key; }
  BL_INLINE bool operator>(const MyTreeNode& other) const noexcept { return _key > other._key; }

  BL_INLINE bool operator<(uint32_t queryKey) const noexcept { return _key < queryKey; }
  BL_INLINE bool operator>(uint32_t queryKey) const noexcept { return _key > queryKey; }

  uint32_t _key;
};

UNIT(arena_tree, BL_TEST_GROUP_SUPPORT_CONTAINERS) {
  constexpr uint32_t kCount = 2000;

  ArenaAllocator zone(4096);
  ArenaTree<MyTreeNode> rbTree;

  uint32_t key;
  INFO("Inserting %u elements to the tree and validating each operation", unsigned(kCount));
  for (key = 0; key < kCount; key++) {
    rbTree.insert(zone.newT<MyTreeNode>(key));
    TreeTestUtils<MyTreeNode>::verifyTree(rbTree);
  }

  uint32_t count = kCount;
  INFO("Removing %u elements from the tree and validating each operation", unsigned(kCount));
  do {
    MyTreeNode* node;

    for (key = 0; key < count; key++) {
      node = rbTree.get(key);
      EXPECT_NE(node, nullptr);
      EXPECT_EQ(node->_key, key);
    }

    node = rbTree.get(--count);
    rbTree.remove(node);
    TreeTestUtils<MyTreeNode>::verifyTree(rbTree);
  } while (count);

  EXPECT_TRUE(rbTree.empty());
}

} // {Tests}
} // {bl}

#endif // BL_TEST
