// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#include "./api-build_p.h"
#include "./support_p.h"
#include "./zoneallocator_p.h"
#include "./zonetree_p.h"

// ============================================================================
// [BLZoneTree - Unit Tests]
// ============================================================================

#ifdef BL_TEST
template<typename NodeT>
struct ZoneTreeUnit {
  typedef BLZoneTree<NodeT> Tree;

  static void verifyTree(Tree& tree) noexcept {
    EXPECT(checkHeight(static_cast<NodeT*>(tree._root)) > 0);
  }

  // Check whether the Red-Black tree is valid.
  static int checkHeight(NodeT* node) noexcept {
    if (!node) return 1;

    NodeT* ln = node->left();
    NodeT* rn = node->right();

    // Invalid tree.
    EXPECT(ln == nullptr || *ln < *node);
    EXPECT(rn == nullptr || *rn > *node);

    // Red violation.
    EXPECT(!node->isRed() ||
          (!BLZoneTreeNodeBase::_isValidRed(ln) && !BLZoneTreeNodeBase::_isValidRed(rn)));

    // Black violation.
    int lh = checkHeight(ln);
    int rh = checkHeight(rn);
    EXPECT(!lh || !rh || lh == rh);

    // Only count black links.
    return (lh && rh) ? lh + !node->isRed() : 0;
  }
};

class MyTreeNode : public BLZoneTreeNode<MyTreeNode> {
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

UNIT(zone_tree, -5) {
  constexpr uint32_t kCount = 2000;

  BLZoneAllocator zone(4096);
  BLZoneTree<MyTreeNode> rbTree;

  uint32_t key;
  INFO("Inserting %u elements to the tree and validating each operation", unsigned(kCount));
  for (key = 0; key < kCount; key++) {
    rbTree.insert(zone.newT<MyTreeNode>(key));
    ZoneTreeUnit<MyTreeNode>::verifyTree(rbTree);
  }

  uint32_t count = kCount;
  INFO("Removing %u elements from the tree and validating each operation", unsigned(kCount));
  do {
    MyTreeNode* node;

    for (key = 0; key < count; key++) {
      node = rbTree.get(key);
      EXPECT(node != nullptr);
      EXPECT(node->_key == key);
    }

    node = rbTree.get(--count);
    rbTree.remove(node);
    ZoneTreeUnit<MyTreeNode>::verifyTree(rbTree);
  } while (count);

  EXPECT(rbTree.empty());
}
#endif
