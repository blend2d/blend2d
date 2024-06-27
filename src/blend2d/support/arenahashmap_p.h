// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SUPPORT_ARENAHASHMAP_P_H_INCLUDED
#define BLEND2D_SUPPORT_ARENAHASHMAP_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../support/arenaallocator_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

//! \name Arena Allocated Hash Map
//! \{

//! Node used by `ZoneHash<>` template.
//!
//! You must provide function `bool eq(const Key& key)` in order to make
//! `ZoneHash::get()` working.
class ArenaHashMapNode {
public:
  BL_NONCOPYABLE(ArenaHashMapNode)

  //! Next node in the chain, null if it terminates the chain.
  ArenaHashMapNode* _hashNext;
  //! Precalculated hash-code of key.
  uint32_t _hashCode;
  //! Padding, can be reused by any Node that inherits `ArenaHashMapNode`.
  union {
    uint32_t _customData;
    uint16_t _customDataU16[2];
    uint8_t _customDataU8[4];
  };

  BL_INLINE ArenaHashMapNode(uint32_t hashCode = 0, uint32_t customData = 0) noexcept
    : _hashNext(nullptr),
      _hashCode(hashCode),
      _customData(customData) {}
};

//! Base class used by `ArenaHashMap<>` template to share the common functionality.
class ArenaHashMapBase {
public:
  BL_NONCOPYABLE(ArenaHashMapBase)

  // NOTE: There must be at least 2 embedded buckets, otherwise we wouldn't be
  // able to implement division as multiplication and shift in 32-bit mode the
  // way we want. Additionally, if we know that there is always a valid buckets
  // array we won't have to perform null checks.
  enum NullConstants : uint32_t {
    kNullCount = 2,
    kNullGrow = 1,
    kNullRcpValue = 2147483648u, // 2^31
    kNullRcpShift = BL_TARGET_ARCH_BITS >= 64 ? 32 : 0
  };

  ArenaAllocator* _allocator {};
  //! Buckets data.
  ArenaHashMapNode** _data {};
  //! Count of records inserted into the hash table.
  size_t _size {};
  //! Count of hash buckets.
  uint32_t _bucketCount = kNullCount;
  //! When buckets array should grow (only checked after insertion).
  uint32_t _bucketGrow = kNullGrow;
  //! Reciprocal value of `_bucketCount`.
  uint32_t _rcpValue = kNullRcpValue;
  //! How many bits to shift right when hash is multiplied with `_rcpValue`.
  uint8_t _rcpShift = kNullRcpShift;
  //! Prime value index in internal prime array.
  uint8_t _primeIndex = 0;
  //! Padding...
  uint8_t _reserved[2] {};
  //! Embedded and initial hash data.
  ArenaHashMapNode* _embedded[kNullCount] {};

  //! \name Construction & Destruction
  //! \{

  BL_INLINE ArenaHashMapBase(ArenaAllocator* allocator) noexcept
    : _allocator(allocator),
      _data(_embedded) {}

  BL_INLINE ArenaHashMapBase(ArenaHashMapBase&& other) noexcept
    : _allocator(other._allocator),
      _data(other._data),
      _size(other._size),
      _bucketCount(other._bucketCount),
      _bucketGrow(other._bucketGrow),
      _rcpValue(other._rcpValue),
      _rcpShift(other._rcpShift),
      _primeIndex(other._primeIndex) {
    other._data = nullptr;
    other._size = 0;
    other._bucketCount = kNullCount;
    other._bucketGrow = kNullGrow;
    other._rcpValue = kNullRcpValue;
    other._rcpShift = kNullRcpShift;
    other._primeIndex = 0;

    memcpy(_embedded, other._embedded, kNullCount * sizeof(ArenaHashMapNode*));
    memset(other._embedded, 0, kNullCount * sizeof(ArenaHashMapNode*));
  }

  BL_INLINE ~ArenaHashMapBase() noexcept {
    if (_data != _embedded)
      _allocator->release(_data, _bucketCount * sizeof(ArenaHashMapNode*));
  }

  BL_INLINE void reset() noexcept {
    if (_data != _embedded)
      _allocator->release(_data, _bucketCount * sizeof(ArenaHashMapNode*));

    _data = _embedded;
    _size = 0;
    _bucketCount = kNullCount;
    _bucketGrow = kNullGrow;
    _rcpValue = kNullRcpValue;
    _rcpShift = kNullRcpShift;
    _primeIndex = 0;
    memset(_embedded, 0, kNullCount * sizeof(ArenaHashMapNode*));
  }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE bool empty() const noexcept { return _size == 0; }
  BL_INLINE size_t size() const noexcept { return _size; }

  //! \}

  //! \name Internals
  //! \{

  BL_INLINE void _swap(ArenaHashMapBase& other) noexcept {
    BLInternal::swap(_allocator, other._allocator);
    BLInternal::swap(_data, other._data);
    BLInternal::swap(_size, other._size);
    BLInternal::swap(_bucketCount, other._bucketCount);
    BLInternal::swap(_bucketGrow, other._bucketGrow);
    BLInternal::swap(_rcpValue, other._rcpValue);
    BLInternal::swap(_rcpShift, other._rcpShift);
    BLInternal::swap(_primeIndex, other._primeIndex);

    for (uint32_t i = 0; i < kNullCount; i++)
      BLInternal::swap(_embedded[i], other._embedded[i]);

    if (_data == other._embedded) _data = _embedded;
    if (other._data == _embedded) other._data = other._embedded;
  }

  BL_INLINE uint32_t _calcMod(uint32_t hash) const noexcept {
    uint32_t divided =
      BL_TARGET_ARCH_BITS >= 64
        ? uint32_t((uint64_t(hash) * _rcpValue) >> _rcpShift)
        : uint32_t((uint64_t(hash) * _rcpValue) >> 32) >> _rcpShift;

    uint32_t result = hash - divided * _bucketCount;
    BL_ASSERT(result < _bucketCount);
    return result;
  }

  void _rehash(uint32_t newCount) noexcept;
  void _insert(ArenaHashMapNode* node) noexcept;
  bool _remove(ArenaHashMapNode* node) noexcept;

  //! \}
};

//! Low-level hash table specialized for storing string keys and POD values.
//!
//! This hash table allows duplicates to be inserted (the API is so low
//! level that it's up to you if you allow it or not, as you should first
//! `get()` the node and then modify it or insert a new node by using `insert()`,
//! depending on the intention).
template<typename NodeT>
class ArenaHashMap : public ArenaHashMapBase {
public:
  BL_NONCOPYABLE(ArenaHashMap)

  typedef NodeT Node;

  //! \name Construction & Destruction
  //! \{

  BL_INLINE ArenaHashMap(ArenaAllocator* allocator) noexcept
    : ArenaHashMapBase(allocator) {}

  BL_INLINE ArenaHashMap(ArenaHashMap&& other) noexcept
    : ArenaHashMap(other) {}

  BL_INLINE ~ArenaHashMap() noexcept {
    if (!std::is_trivially_destructible<NodeT>::value)
      _destroy();
  }

  //! \}

  //! \name Utilities
  //! \{

  BL_INLINE void swap(ArenaHashMap& other) noexcept {
    ArenaHashMapBase::_swap(other);
  }

  BL_NOINLINE void _destroy() noexcept {
    for (size_t i = 0; i < _bucketCount; i++) {
      NodeT* node = static_cast<NodeT*>(_data[i]);
      if (node) {
        do {
          NodeT* next = static_cast<NodeT*>(node->_hashNext);
          blCallDtor(*node);
          node = next;
        } while (node);
        _data[i] = nullptr;
      }
    }
  }

  //! \}

  //! \name Functionality
  //! \{

  BL_INLINE NodeT* nodesByHashCode(uint32_t hashCode) const noexcept {
    uint32_t hashMod = _calcMod(hashCode);
    return static_cast<NodeT*>(_data[hashMod]);
  }

  template<typename KeyT>
  BL_INLINE NodeT* get(const KeyT& key) const noexcept {
    NodeT* node = nodesByHashCode(key.hashCode());
    while (node && !key.matches(node))
      node = static_cast<NodeT*>(node->_hashNext);
    return node;
  }

  BL_INLINE void insert(NodeT* node) noexcept { _insert(node); }
  BL_INLINE bool remove(NodeT* node) noexcept { return _remove(node); }

  template<typename Lambda>
  BL_INLINE void forEach(Lambda&& f) const noexcept {
    ArenaHashMapNode** buckets = _data;
    uint32_t bucketCount = _bucketCount;

    for (uint32_t i = 0; i < bucketCount; i++) {
      Node* node = static_cast<Node*>(buckets[i]);
      while (node) {
        Node* next = static_cast<Node*>(node->_hashNext);
        f(node);
        node = next;
      }
    }
  }

  //! \}
};

//! \}

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_SUPPORT_ARENAHASHMAP_P_H_INCLUDED
