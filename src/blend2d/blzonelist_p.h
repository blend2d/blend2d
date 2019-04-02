// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLZONELIST_P_H
#define BLEND2D_BLZONELIST_P_H

#include "./blapi-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLZoneListNode]
// ============================================================================

//! Zone-allocated double-linked list node.
template<typename NodeT>
class BLZoneListNode {
public:
  BL_NONCOPYABLE(BLZoneListNode)

  union {
    NodeT* _listNodes[2];
    struct {
      NodeT* _listPrev;
      NodeT* _listNext;
    };
  };

  BL_INLINE BLZoneListNode() noexcept
    : _listNodes { nullptr, nullptr } {}

  BL_INLINE BLZoneListNode(BLZoneListNode&& other) noexcept
    : _listNodes { other._listNodes[0], other._listNodes[1] } {}

  BL_INLINE bool hasPrev() const noexcept { return _listPrev != nullptr; }
  BL_INLINE bool hasNext() const noexcept { return _listNext != nullptr; }

  BL_INLINE NodeT* prev() const noexcept { return _listPrev; }
  BL_INLINE NodeT* next() const noexcept { return _listNext; }
};

// ============================================================================
// [BLZoneList<T>]
// ============================================================================

//! Zone-allocated double-linked list container.
template <typename NodeT>
class BLZoneList {
public:
  BL_NONCOPYABLE(BLZoneList)

  NodeT* _nodes[2];

  BL_INLINE BLZoneList() noexcept
    : _nodes { nullptr, nullptr } {}

  BL_INLINE BLZoneList(BLZoneList&& other) noexcept
    : _nodes { other._nodes[0], other._nodes[1] } {}

  BL_INLINE void swap(BLZoneList& other) noexcept {
    std::swap(_nodes[0], other._nodes[0]);
    std::swap(_nodes[1], other._nodes[1]);
  }

  BL_INLINE void reset() noexcept {
    _nodes[0] = nullptr;
    _nodes[1] = nullptr;
  }

  BL_INLINE bool empty() const noexcept { return _nodes[0] == nullptr; }
  BL_INLINE NodeT* first() const noexcept { return _nodes[0]; }
  BL_INLINE NodeT* last() const noexcept { return _nodes[1]; }

  // Can be used to both prepend and append.
  BL_INLINE void _addNode(NodeT* node, size_t dir) noexcept {
    NodeT* prev = _nodes[dir];

    node->_listNodes[!dir] = prev;
    _nodes[dir] = node;
    if (prev)
      prev->_listNodes[dir] = node;
    else
      _nodes[!dir] = node;
  }

  // Can be used to both prepend and append.
  BL_INLINE void _insertNode(NodeT* ref, NodeT* node, size_t dir) noexcept {
    BL_ASSERT(ref != nullptr);

    NodeT* prev = ref;
    NodeT* next = ref->_listNodes[dir];

    prev->_listNodes[dir] = node;
    if (next)
      next->_listNodes[!dir] = node;
    else
      _nodes[dir] = node;

    node->_listNodes[!dir] = prev;
    node->_listNodes[ dir] = next;
  }

  BL_INLINE void append(NodeT* node) noexcept { _addNode(node, 1); }
  BL_INLINE void prepend(NodeT* node) noexcept { _addNode(node, 0); }

  BL_INLINE void insertAfter(NodeT* ref, NodeT* node) noexcept { _insertNode(ref, node, 1); }
  BL_INLINE void insertBefore(NodeT* ref, NodeT* node) noexcept { _insertNode(ref, node, 0); }

  BL_INLINE NodeT* unlink(NodeT* node) noexcept {
    NodeT* prev = node->prev();
    NodeT* next = node->next();

    if (prev) { prev->_listNext = next; node->_listPrev = nullptr; } else { _nodes[0] = next; }
    if (next) { next->_listPrev = prev; node->_listNext = nullptr; } else { _nodes[1] = prev; }

    node->_listPrev = nullptr;
    node->_listNext = nullptr;

    return node;
  }

  BL_INLINE NodeT* popFirst() noexcept {
    NodeT* node = _nodes[0];
    BL_ASSERT(node != nullptr);

    NodeT* next = node->next();
    _nodes[0] = next;

    if (next) {
      next->_listPrev = nullptr;
      node->_listNext = nullptr;
    }
    else {
      _nodes[1] = nullptr;
    }

    return node;
  }

  BL_INLINE NodeT* pop() noexcept {
    NodeT* node = _nodes[1];
    BL_ASSERT(node != nullptr);

    NodeT* prev = node->prev();
    _nodes[1] = prev;

    if (prev) {
      prev->_listNext = nullptr;
      node->_listPrev = nullptr;
    }
    else {
      _nodes[0] = nullptr;
    }

    return node;
  }
};

//! \}
//! \endcond

#endif // BLEND2D_BLZONELIST_P_H
