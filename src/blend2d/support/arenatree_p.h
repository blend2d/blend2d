// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SUPPORT_ARENATREE_P_H_INCLUDED
#define BLEND2D_SUPPORT_ARENATREE_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../support/algorithm_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name Arena Allocated Tree
//! \{

//! BLArenaTree node (base class).
//!
//! The color is stored in a least significant bit of the `left` node.
//!
//! \note Always use accessors to access left and right nodes.
class BLArenaTreeNodeBase {
public:
  BL_NONCOPYABLE(BLArenaTreeNodeBase)

  //! \name Constants
  //! \{

  static constexpr const uintptr_t kRedMask = 0x1;
  static constexpr const uintptr_t kPtrMask = ~kRedMask;

  //! \}

  //! \name Members
  //! \{

  uintptr_t _treeNodes[2];

  //! \}

  BL_INLINE BLArenaTreeNodeBase() noexcept
    : _treeNodes { 0, 0 } {}

  //! \name Accessors
  //! \{

  BL_INLINE bool hasChild(size_t i) const noexcept { return _treeNodes[i] > kRedMask; }
  BL_INLINE bool hasLeft() const noexcept { return _treeNodes[0] > kRedMask; }
  BL_INLINE bool hasRight() const noexcept { return _treeNodes[1] != 0; }

  BL_INLINE BLArenaTreeNodeBase* _getChild(size_t i) const noexcept { return (BLArenaTreeNodeBase*)(_treeNodes[i] & kPtrMask); }
  BL_INLINE BLArenaTreeNodeBase* _getLeft() const noexcept { return (BLArenaTreeNodeBase*)(_treeNodes[0] & kPtrMask); }
  BL_INLINE BLArenaTreeNodeBase* _getRight() const noexcept { return (BLArenaTreeNodeBase*)(_treeNodes[1]); }

  BL_INLINE void _setChild(size_t i, BLArenaTreeNodeBase* node) noexcept { _treeNodes[i] = (_treeNodes[i] & kRedMask) | (uintptr_t)node; }
  BL_INLINE void _setLeft(BLArenaTreeNodeBase* node) noexcept { _treeNodes[0] = (_treeNodes[0] & kRedMask) | (uintptr_t)node; }
  BL_INLINE void _setRight(BLArenaTreeNodeBase* node) noexcept { _treeNodes[1] = (uintptr_t)node; }

  template<typename T = BLArenaTreeNodeBase>
  BL_INLINE T* child(size_t i) const noexcept { return static_cast<T*>(_getChild(i)); }
  template<typename T = BLArenaTreeNodeBase>
  BL_INLINE T* left() const noexcept { return static_cast<T*>(_getLeft()); }
  template<typename T = BLArenaTreeNodeBase>
  BL_INLINE T* right() const noexcept { return static_cast<T*>(_getRight()); }

  BL_INLINE bool isRed() const noexcept { return static_cast<bool>(_treeNodes[0] & kRedMask); }
  BL_INLINE void _makeRed() noexcept { _treeNodes[0] |= kRedMask; }
  BL_INLINE void _makeBlack() noexcept { _treeNodes[0] &= kPtrMask; }

  //! \}

  //! Tests whether the node is RED (RED node must be non-null and must have RED flag set).
  static BL_INLINE bool _isValidRed(BLArenaTreeNodeBase* node) noexcept { return node && node->isRed(); }
};

//! BLArenaTree node.
template<typename NodeT>
class BLArenaTreeNode : public BLArenaTreeNodeBase {
public:
  BL_NONCOPYABLE(BLArenaTreeNode)

  BL_INLINE BLArenaTreeNode() noexcept
    : BLArenaTreeNodeBase() {}

  //! \name Accessors
  //! \{

  BL_INLINE NodeT* child(size_t i) const noexcept { return static_cast<NodeT*>(_getChild(i)); }
  BL_INLINE NodeT* left() const noexcept { return static_cast<NodeT*>(_getLeft()); }
  BL_INLINE NodeT* right() const noexcept { return static_cast<NodeT*>(_getRight()); }

  //! \}
};

//! A red-black tree that uses nodes allocated by `BLArenaAllocator`.
template<typename NodeT>
class BLArenaTree {
public:
  BL_NONCOPYABLE(BLArenaTree)

  typedef NodeT Node;
  NodeT* _root;

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLArenaTree() noexcept
    : _root(nullptr) {}

  BL_INLINE BLArenaTree(BLArenaTree&& other) noexcept
    : _root(other._root) {}

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE void swap(BLArenaTree& other) noexcept {
    std::swap(_root, other._root);
  }

  BL_INLINE void reset() noexcept { _root = nullptr; }

  //! \}

  //! \name BLArenaTree Functionality
  //! \{

  //! Insert a node into the tree.
  template<typename CompareT = BLAlgorithm::CompareOp<BLAlgorithm::SortOrder::kAscending>>
  void insert(NodeT* node, const CompareT& cmp = CompareT()) noexcept {
    // Node to insert must not contain garbage.
    BL_ASSERT(!node->hasLeft());
    BL_ASSERT(!node->hasRight());
    BL_ASSERT(!node->isRed());

    if (!_root) {
      _root = node;
      return;
    }

    BLArenaTreeNodeBase head;          // False root node,
    head._setRight(_root);           // having root on the right.

    BLArenaTreeNodeBase* g = nullptr;  // Grandparent.
    BLArenaTreeNodeBase* p = nullptr;  // Parent.
    BLArenaTreeNodeBase* t = &head;    // Iterator.
    BLArenaTreeNodeBase* q = _root;    // Query.

    size_t dir = 0;                  // Direction for accessing child nodes.
    size_t last = 0;                 // Not needed to initialize, but makes some tools happy.
    node->_makeRed();                // New nodes are always red and violations fixed appropriately.

    // Search down the tree.
    for (;;) {
      if (!q) {
        // Insert new node at the bottom.
        q = node;
        p->_setChild(dir, node);
      }
      else if (_isValidRed(q->_getLeft()) && _isValidRed(q->_getRight())) {
        // Color flip.
        q->_makeRed();
        q->_getLeft()->_makeBlack();
        q->_getRight()->_makeBlack();
      }

      // Fix red violation.
      if (_isValidRed(q) && _isValidRed(p))
        t->_setChild(t->_getRight() == g, q == p->_getChild(last) ? _singleRotate(g, !last) : _doubleRotate(g, !last));

      // Stop if found.
      if (q == node)
        break;

      last = dir;
      dir = cmp(*static_cast<NodeT*>(q), *static_cast<NodeT*>(node)) < 0;

      // Update helpers.
      if (g) t = g;

      g = p;
      p = q;
      q = q->_getChild(dir);
    }

    // Update root and make it black.
    _root = static_cast<NodeT*>(head._getRight());
    _root->_makeBlack();
  }

  //! Remove a node from the tree.
  template<typename CompareT = BLAlgorithm::CompareOp<BLAlgorithm::SortOrder::kAscending>>
  void remove(BLArenaTreeNodeBase* node, const CompareT& cmp = CompareT()) noexcept {
    BLArenaTreeNodeBase head;          // False root node,
    head._setRight(_root);           // having root on the right.

    BLArenaTreeNodeBase* g = nullptr;  // Grandparent.
    BLArenaTreeNodeBase* p = nullptr;  // Parent.
    BLArenaTreeNodeBase* q = &head;    // Query.

    BLArenaTreeNodeBase* f  = nullptr; // Found item.
    BLArenaTreeNodeBase* gf = nullptr; // Found grandparent.
    size_t dir = 1;                  // Direction (0 or 1).

    // Search and push a red down.
    while (q->hasChild(dir)) {
      size_t last = dir;

      // Update helpers.
      g = p;
      p = q;
      q = q->_getChild(dir);
      dir = cmp(*static_cast<NodeT*>(q), *static_cast<NodeT*>(node)) < 0;

      // Save found node.
      if (q == node) {
        f = q;
        gf = g;
      }

      // Push the red node down.
      if (!_isValidRed(q) && !_isValidRed(q->_getChild(dir))) {
        if (_isValidRed(q->_getChild(!dir))) {
          BLArenaTreeNodeBase* child = _singleRotate(q, dir);
          p->_setChild(last, child);
          p = child;
        }
        else if (!_isValidRed(q->_getChild(!dir)) && p->_getChild(!last)) {
          BLArenaTreeNodeBase* s = p->_getChild(!last);
          if (!_isValidRed(s->_getChild(!last)) && !_isValidRed(s->_getChild(last))) {
            // Color flip.
            p->_makeBlack();
            s->_makeRed();
            q->_makeRed();
          }
          else {
            size_t dir2 = g->_getRight() == p;
            BLArenaTreeNodeBase* child = g->_getChild(dir2);

            if (_isValidRed(s->_getChild(last))) {
              child = _doubleRotate(p, last);
              g->_setChild(dir2, child);
            }
            else if (_isValidRed(s->_getChild(!last))) {
              child = _singleRotate(p, last);
              g->_setChild(dir2, child);
            }

            // Ensure correct coloring.
            q->_makeRed();
            child->_makeRed();
            child->_getLeft()->_makeBlack();
            child->_getRight()->_makeBlack();
          }
        }
      }
    }

    // Replace and remove.
    BL_ASSERT(f != nullptr);
    BL_ASSERT(f != &head);
    BL_ASSERT(q != &head);

    p->_setChild(p->_getRight() == q, q->_getChild(q->_getLeft() == nullptr));

    // NOTE: The original algorithm used a trick to just copy 'key/value' to `f` and mark `q` for deletion. But
    // this is unacceptable here as we really want to destroy the passed `node`. So, we have to make sure that
    // we have really removed `f` and not `q`.
    if (f != q) {
      BL_ASSERT(f != &head);
      BL_ASSERT(f != gf);

      BLArenaTreeNodeBase* n = gf ? gf : &head;
      dir = (n == &head) ? 1  : cmp(*static_cast<NodeT*>(n), *static_cast<NodeT*>(node)) < 0;

      for (;;) {
        if (n->_getChild(dir) == f) {
          n->_setChild(dir, q);
          // RAW copy, including the color.
          q->_treeNodes[0] = f->_treeNodes[0];
          q->_treeNodes[1] = f->_treeNodes[1];
          break;
        }

        n = n->_getChild(dir);

        // Cannot be true as we know that it must reach `f` in few iterations.
        BL_ASSERT(n != nullptr);
        dir = cmp(*static_cast<NodeT*>(n), *static_cast<NodeT*>(node)) < 0;
      }
    }

    // Update root and make it black.
    _root = static_cast<NodeT*>(head._getRight());
    if (_root) _root->_makeBlack();
  }

  template<typename KeyT, typename CompareT = BLAlgorithm::CompareOp<BLAlgorithm::SortOrder::kAscending>>
  BL_INLINE NodeT* get(const KeyT& key, const CompareT& cmp = CompareT()) const noexcept {
    BLArenaTreeNodeBase* node = _root;
    while (node) {
      auto result = cmp(*static_cast<const NodeT*>(node), key);
      if (result == 0) break;

      // Go left or right depending on the `result`.
      node = node->_getChild(result < 0);
    }
    return static_cast<NodeT*>(node);
  }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE bool empty() const noexcept { return _root == nullptr; }
  BL_INLINE NodeT* root() const noexcept { return static_cast<NodeT*>(_root); }

  //! \}

  //! \name Internals
  //! \{

  static BL_INLINE bool _isValidRed(BLArenaTreeNodeBase* node) noexcept { return BLArenaTreeNodeBase::_isValidRed(node); }

  //! Single rotation.
  static BL_INLINE BLArenaTreeNodeBase* _singleRotate(BLArenaTreeNodeBase* root, size_t dir) noexcept {
    BLArenaTreeNodeBase* save = root->_getChild(!dir);
    root->_setChild(!dir, save->_getChild(dir));
    save->_setChild( dir, root);
    root->_makeRed();
    save->_makeBlack();
    return save;
  }

  //! Double rotation.
  static BL_INLINE BLArenaTreeNodeBase* _doubleRotate(BLArenaTreeNodeBase* root, size_t dir) noexcept {
    root->_setChild(!dir, _singleRotate(root->_getChild(!dir), !dir));
    return _singleRotate(root, dir);
  }

  //! \}
};

//! \}

//! \}
//! \endcond

#endif // BLEND2D_SUPPORT_ARENATREE_P_H_INCLUDED
