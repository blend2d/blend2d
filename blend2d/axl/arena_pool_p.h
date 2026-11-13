// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_AXL_ARENA_POOL_P_H_INCLUDED
#define BLEND2D_AXL_ARENA_POOL_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/axl/arena_allocator_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_support
//! \{

namespace bl {

//! Helper class for implementing pooling of arena-allocated objects.
template<typename T, size_t SizeOfT = sizeof(T)>
class ArenaPool {
public:
  BL_NONCOPYABLE(ArenaPool)

  struct Link { Link* next; };
  Link* _pool;

  BL_INLINE ArenaPool() noexcept
    : _pool(nullptr) {}

  //! Resets the arena pool.
  //!
  //! Reset must be called after the associated `ArenaAllocator` has been reset, otherwise the existing pool will
  //! collide with possible allocations made on the `ArenaAllocator` object after the reset.
  BL_INLINE void reset() noexcept { _pool = nullptr; }

  //! Ensures that there is at least one object in the pool.
  [[nodiscard]]
  BL_INLINE bool ensure(ArenaAllocator& arena) noexcept {
    if (_pool) return true;

    Link* p = static_cast<Link*>(arena.alloc(SizeOfT));
    if (p == nullptr) return false;

    p->next = nullptr;
    _pool = p;
    return true;
  }

  //! Allocates a memory (or reuses the existing allocation) of `SizeOfT` (in bytes).
  [[nodiscard]]
  BL_INLINE T* alloc(ArenaAllocator& arena) noexcept {
    Link* p = _pool;
    if (BL_UNLIKELY(p == nullptr))
      return static_cast<T*>(arena.alloc(SizeOfT));
    _pool = p->next;
    return static_cast<T*>(static_cast<void*>(p));
  }

  //! Like `alloc()`, but can be only called after `ensure()` returned `true`.
  [[nodiscard]]
  BL_INLINE T* alloc_ensured() noexcept {
    Link* p = _pool;
    BL_ASSERT(p != nullptr);

    _pool = p->next;
    return static_cast<T*>(static_cast<void*>(p));
  }

  //! Pools the previously allocated memory.
  BL_INLINE void free(T* _p) noexcept {
    BL_ASSERT(_p != nullptr);
    Link* p = reinterpret_cast<Link*>(_p);

    p->next = _pool;
    _pool = p;
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_AXL_ARENA_POOL_P_H_INCLUDED
