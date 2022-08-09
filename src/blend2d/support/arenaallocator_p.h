// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SUPPORT_ARENAALLOCATOR_P_H_INCLUDED
#define BLEND2D_SUPPORT_ARENAALLOCATOR_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../support/intops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! Arena memory allocator.
//!
//! Arena allocator is an incremental memory allocator that allocates memory by simply incrementing a pointer.
//! It allocates blocks of memory by using standard C library `malloc/free`, but divides these blocks into
//! smaller chunks requested by calling `BLArenaAllocator::alloc()` and friends.
//!
//! Arena allocators are designed to either allocate memory for data that has a short lifetime or data in containers
//! where it's expected that many small chunks will be allocated.
//!
//! \note It's not recommended to use `BLArenaAllocator` to allocate larger data structures than the initial `blockSize`
//! passed to its constructor. The block size should be always greater than the maximum `size` passed to `alloc()`.
//! Arena allocator is designed to handle such cases, but it may allocate new block for each call to `alloc()` that
//! exceeds the default block size.
class BLArenaAllocator {
public:
  BL_NONCOPYABLE(BLArenaAllocator)

  //! A single block of memory managed by `BLArenaAllocator`.
  struct Block {
    //! Link to the previous block.
    Block* prev;
    //! Link to the next block.
    Block* next;
    //! Size of the block.
    size_t size;

    BL_INLINE uint8_t* data() const noexcept {
      return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(this) + sizeof(*this));
    }

    BL_INLINE uint8_t* end() const noexcept {
      return data() + size;
    }
  };

  //! Zero block, used by a default constructed `BLArenaAllocator`, which doesn't hold any allocated block. This block
  //! must be properly aligned so when arena allocator aligns its current pointer to check for aligned allocation it
  //! would not overflow past the end of the block - which is the same as the beginning of the block as it has no size.
  struct alignas(64) ZeroBlock {
    uint8_t padding[64 - sizeof(Block)];
    Block block;
  };

  typedef uint8_t* StatePtr;

  enum Limits : size_t {
    kMinBlockSize = 1024, // Safe bet - it must be greater than `kMaxAlignment`.
    kMaxBlockSize = size_t(1) << (sizeof(size_t) * 8 - 4 - 1),
    kMinAlignment = 1,
    kMaxAlignment = 64,

    kBlockSize = sizeof(Block),
    kBlockOverhead = sizeof(Block) + kMaxAlignment + BL_ALLOC_OVERHEAD,
  };

  //! Pointer in the current block.
  uint8_t* _ptr;
  //! End of the current block.
  uint8_t* _end;
  //! Current block.
  Block* _block;

  union {
    struct {
      //! Default block size.
      size_t _blockSize : BLIntOps::bitSizeOf<size_t>() - 4;
      //! First block is temporary (BLArenaAllocatorTmp).
      size_t _hasStaticBlock : 1;
      //! Block alignment (1 << alignment).
      size_t _blockAlignmentShift : 3;
    };
    size_t _packedData;
  };

  static BL_HIDDEN const ZeroBlock _zeroBlock;

  //! \name Construction / Destruction
  //! \{

  //! Create a new `BLArenaAllocator`.
  //!
  //! The `blockSize` parameter describes the default size of the block. If the `size` parameter passed to
  //! `alloc()` is greater than the default size `BLArenaAllocator` will allocate and use a larger block, but
  //! it will not change the default `blockSize`.
  //!
  //! It's not required, but it's good practice to set `blockSize` to a reasonable value that depends on the
  //! usage of `BLArenaAllocator`. Greater block sizes are generally safer and perform better than unreasonably
  //! low block sizes.
  BL_INLINE explicit BLArenaAllocator(size_t blockSize, size_t blockAlignment = 1) noexcept {
    _init(blockSize, blockAlignment, nullptr, 0);
  }

  BL_INLINE BLArenaAllocator(size_t blockSize, size_t blockAlignment, void* staticData, size_t staticSize) noexcept {
    _init(blockSize, blockAlignment, staticData, staticSize);
  }

  //! Destroy the `BLArenaAllocator` instance.
  //!
  //! This will destroy the `BLArenaAllocator` instance and release all blocks of memory allocated by it. It
  //! performs implicit `reset()`.
  BL_INLINE ~BLArenaAllocator() noexcept { reset(); }

  BL_HIDDEN void _init(size_t blockSize, size_t blockAlignment, void* staticData, size_t staticSize) noexcept;

  //! Resets the `BLArenaAllocator` and invalidates all blocks it has allocated.
  BL_HIDDEN void reset() noexcept;

  //! \}

  //! \name Basic Operations
  //! \{

  //! Invalidates all allocations and moves the current block pointer to the first block. It's similar to
  //! `reset()`, however, it doesn't free blocks of memory it holds.
  BL_INLINE void clear() noexcept {
    Block* cur = _block;
    while (cur->prev)
      cur = cur->prev;
    _assignBlock(cur);
  }

  BL_INLINE void swap(BLArenaAllocator& other) noexcept {
    // This could lead to a disaster.
    BL_ASSERT(!this->hasStaticBlock());
    BL_ASSERT(!other.hasStaticBlock());

    std::swap(_ptr, other._ptr);
    std::swap(_end, other._end);
    std::swap(_block, other._block);
    std::swap(_packedData, other._packedData);
  }

  //! \}

  //! \name Accessors
  //! \{

  //! Tests whether this `BLArenaAllocator` is actually a `BLArenaAllocatorTmp` that uses temporary memory.
  BL_NODISCARD
  BL_INLINE bool hasStaticBlock() const noexcept { return _hasStaticBlock != 0; }

  //! Returns the default block size.
  BL_NODISCARD
  BL_INLINE size_t blockSize() const noexcept { return _blockSize; }

  //! Returns the default block alignment.
  BL_NODISCARD
  BL_INLINE size_t blockAlignment() const noexcept { return size_t(1) << _blockAlignmentShift; }

  //! Returns the remaining size of the current block.
  BL_NODISCARD
  BL_INLINE size_t remainingSize() const noexcept { return (size_t)(_end - _ptr); }

  //! Returns the current arena allocator cursor (dangerous).
  //!
  //! This is a function that can be used to get exclusive access to the current block's memory buffer.
  template<typename T = uint8_t>
  BL_NODISCARD
  BL_INLINE T* ptr() noexcept { return reinterpret_cast<T*>(_ptr); }

  //! Returns the end of the current arena allocator block, only useful if you use `ptr()`.
  template<typename T = uint8_t>
  BL_NODISCARD
  BL_INLINE T* end() noexcept { return reinterpret_cast<T*>(_end); }

  // NOTE: The following two functions `setPtr()` and `setEnd()` can be used to perform manual memory allocation
  // in case that an incremental allocation is needed - for example you build some data structure without knowing
  // the final size. This is used for example by AnalyticRasterizer to build list of edges.

  //! Sets the current arena allocator pointer to `ptr` (must be within the current block).
  template<typename T>
  BL_INLINE void setPtr(T* ptr) noexcept {
    uint8_t* p = reinterpret_cast<uint8_t*>(ptr);
    BL_ASSERT(p >= _ptr && p <= _end);
    _ptr = p;
  }

  //! Sets the end arena allocator pointer to `end` (must be within the current block).
  template<typename T>
  BL_INLINE void setEnd(T* end) noexcept {
    uint8_t* p = reinterpret_cast<uint8_t*>(end);
    BL_ASSERT(p >= _ptr && p <= _end);
    _end = p;
  }

  //! Align the current pointer to `alignment`.
  BL_INLINE void align(size_t alignment) noexcept {
    _ptr = blMin(BLIntOps::alignUp(_ptr, alignment), _end);
  }

  //! Ensures the remaining size is at least equal or greater than `size`.
  //!
  //! \note This function doesn't respect any alignment. If you need to ensure there is enough room for an aligned
  //! allocation you need to call `align()` before calling `ensure()`.
  BL_NODISCARD
  BL_INLINE BLResult ensure(size_t size) noexcept {
    if (size <= remainingSize())
      return BL_SUCCESS;
    else
      return _alloc(0, 1) ? BL_SUCCESS : blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  BL_INLINE void _assignBlock(Block* block) noexcept {
    size_t alignment = blockAlignment();
    _ptr = BLIntOps::alignUp(block->data(), alignment);
    _end = block->data() + block->size;
    _block = block;
  }

  BL_INLINE void _assignZeroBlock() noexcept {
    Block* block = const_cast<Block*>(&_zeroBlock.block);
    _ptr = block->data();
    _end = block->data();
    _block = block;
  }

  //! \}

  //! \name Allocation
  //! \{

  //! Internal alloc function.
  BL_NODISCARD
  BL_HIDDEN void* _alloc(size_t size, size_t alignment) noexcept;

  //! Allocates the requested memory specified by `size`.
  //!
  //! Pointer returned is valid until the `BLArenaAllocator` instance is destroyed or reset by calling `reset()`.
  //! If you plan to make an instance of C++ from the given pointer use placement `new` and `delete` operators:
  //!
  //! ```
  //! class Object { ... };
  //!
  //! // Create a new arena with default block size of approximately 65536 bytes.
  //! BLArenaAllocator arena(65536 - BLArenaAllocator::kBlockOverhead);
  //!
  //! // Create your objects using arena object allocating, for example:
  //! Object* obj = static_cast<Object*>(arena.alloc(sizeof(Object)));
  //
  //! if (!obj) {
  //!   // Handle out of memory error.
  //! }
  //!
  //! // Placement `new` and `delete` operators can be used to instantiate it.
  //! new(obj) Object();
  //!
  //! // ... lifetime of your objects ...
  //!
  //! // To destroy the instance (if required).
  //! obj->~Object();
  //!
  //! // Reset or destroy `BLArenaAllocator`.
  //! arena.reset();
  //! ```
  BL_NODISCARD
  BL_INLINE void* alloc(size_t size) noexcept {
    if (BL_UNLIKELY(size > remainingSize()))
      return _alloc(size, 1);

    uint8_t* ptr = _ptr;
    _ptr += size;
    return static_cast<void*>(ptr);
  }

  //! Allocates the requested memory specified by `size` and `alignment`.
  //!
  //! Performs the same operation as `BLArenaAllocator::alloc(size)` with `alignment` applied.
  BL_NODISCARD
  BL_INLINE void* alloc(size_t size, size_t alignment) noexcept {
    BL_ASSERT(BLIntOps::isPowerOf2(alignment));
    uint8_t* ptr = BLIntOps::alignUp(_ptr, alignment);

    if (size > (size_t)(_end - ptr))
      return _alloc(size, alignment);

    _ptr = ptr + size;
    return static_cast<void*>(ptr);
  }

  //! Allocates the requested memory specified by `size` without doing any checks.
  //!
  //! Can only be called if `remainingSize()` returns size at least equal to `size`.
  BL_NODISCARD
  BL_INLINE void* allocNoCheck(size_t size) noexcept {
    BL_ASSERT(remainingSize() >= size);

    uint8_t* ptr = _ptr;
    _ptr += size;
    return static_cast<void*>(ptr);
  }

  //! Allocates the requested memory specified by `size` and `alignment` without doing any checks.
  //!
  //! Performs the same operation as `BLArenaAllocator::allocNoCheck(size)` with `alignment` applied.
  BL_NODISCARD
  BL_INLINE void* allocNoCheck(size_t size, size_t alignment) noexcept {
    BL_ASSERT(BLIntOps::isPowerOf2(alignment));

    uint8_t* ptr = BLIntOps::alignUp(_ptr, alignment);
    BL_ASSERT(size <= (size_t)(_end - ptr));

    _ptr = ptr + size;
    return static_cast<void*>(ptr);
  }

  //! Allocates the requested memory specified by `size` and `alignment` and clears it before returning its pointer.
  //!
  //! See `alloc()` for more details.
  BL_NODISCARD
  BL_HIDDEN void* allocZeroed(size_t size, size_t alignment = 1) noexcept;

  //! Like `alloc()`, but the return pointer is casted to `T*`.
  template<typename T>
  BL_NODISCARD
  BL_INLINE T* allocT(size_t size = sizeof(T), size_t alignment = alignof(T)) noexcept {
    return static_cast<T*>(alloc(size, alignment));
  }

  template<typename T>
  BL_NODISCARD
  BL_INLINE T* allocNoAlignT(size_t size = sizeof(T)) noexcept {
    T* ptr = static_cast<T*>(alloc(size));
    BL_ASSERT(BLIntOps::isAligned(ptr, alignof(T)));
    return ptr;
  }

  //! Like `allocNoCheck()`, but the return pointer is casted to `T*`.
  template<typename T>
  BL_NODISCARD
  BL_INLINE T* allocNoCheckT(size_t size = sizeof(T), size_t alignment = alignof(T)) noexcept {
    return static_cast<T*>(allocNoCheck(size, alignment));
  }

  //! Like `allocZeroed()`, but the return pointer is casted to `T*`.
  template<typename T>
  BL_NODISCARD
  BL_INLINE T* allocZeroedT(size_t size = sizeof(T), size_t alignment = alignof(T)) noexcept {
    return static_cast<T*>(allocZeroed(size, alignment));
  }

  //! Like `new(std::nothrow) T(...)`, but allocated by `BLArenaAllocator`.
  template<typename T>
  BL_NODISCARD
  BL_INLINE T* newT() noexcept {
    void* p = alloc(sizeof(T), alignof(T));
    if (BL_UNLIKELY(!p))
      return nullptr;
    return new(BLInternal::PlacementNew{p}) T();
  }

  //! Like `new(std::nothrow) T(...)`, but allocated by `BLArenaAllocator`.
  template<typename T, typename... Args>
  BL_NODISCARD
  BL_INLINE T* newT(Args&&... args) noexcept {
    void* p = alloc(sizeof(T), alignof(T));
    if (BL_UNLIKELY(!p))
      return nullptr;
    return new(BLInternal::PlacementNew{p}) T(std::forward<Args>(args)...);
  }

  BL_INLINE void release(void* ptr, size_t size) noexcept {
    // TODO: This should work by creating an invisible block.
    blUnused(ptr, size);
  }

  //! \}

  //! \name State Management
  //! \{

  //! Stores the current state to `state`.
  BL_NODISCARD
  BL_INLINE StatePtr saveState() noexcept {
    return _ptr;
  }

  //! Restores the state of `BLArenaAllocator` from the previously saved `state`.
  BL_INLINE void restoreState(StatePtr p) noexcept {
    Block* block = _block;
    size_t alignment = blockAlignment();

    while (p < block->data() || p >= block->end()) {
      if (!block->prev) {
        // Special case - can happen in case that the allocator didn't have allocated any block when `saveState()`
        // was called. In that case we won't restore to the shared null block, instead we restore to the first block
        // the allocator has.
        p = BLIntOps::alignUp(block->data(), alignment);
        break;
      }
      block = block->prev;
    }

    _block = block;
    _ptr = p;
    _end = block->end();
  }

  //! \}

  //! \name Block Management
  //! \{

  //! Returns a past block - a block used before the current one, or null if this is the first block. Use together
  //! with `reusePastBlock()`.
  BL_NODISCARD
  BL_INLINE Block* pastBlock() const noexcept { return _block->prev; }

  //! Moves the passed block after the current block and makes the block after the given `block` first.
  BL_INLINE void reusePastBlock(Block* pastLast) noexcept {
    BL_ASSERT(pastLast != nullptr); // Cannot be null, check for null block before.
    BL_ASSERT(pastLast != _block);  // Cannot be the current block, must be past that.

    Block* pastFirst = pastLast;
    while (pastFirst->prev)
      pastFirst = pastFirst->prev;

    // Makes `pastNext` the first block.
    Block* pastNext = pastLast->next;
    pastNext->prev = nullptr;

    // Link [pastFirst:pastLast] between `_block` and next.
    Block* next = _block->next;

    _block->next = pastFirst;
    pastFirst->prev = _block;

    next->prev = pastLast;
    pastLast->next = next;
  }

  //! \}
};

//! A temporary `BLArenaAllocator`.
template<size_t N>
class BLArenaAllocatorTmp : public BLArenaAllocator {
public:
  BL_NONCOPYABLE(BLArenaAllocatorTmp)

  BL_INLINE explicit BLArenaAllocatorTmp(size_t blockSize, size_t blockAlignment = 1) noexcept
    : BLArenaAllocator(blockSize, blockAlignment, _storage.data, N) {}

  struct Storage {
    char data[N];
  } _storage;
};

//! Helper class for implementing pooling of arena-allocated objects.
template<typename T, size_t SizeOfT = sizeof(T)>
class BLArenaPool {
public:
  BL_NONCOPYABLE(BLArenaPool)

  struct Link { Link* next; };
  Link* _pool;

  BL_INLINE BLArenaPool() noexcept
    : _pool(nullptr) {}

  //! Resets the arena pool.
  //!
  //! Reset must be called after the associated `BLArenaAllocator` has been reset, otherwise the existing pool will
  //! collide with possible allocations made on the `BLArenaAllocator` object after the reset.
  BL_INLINE void reset() noexcept { _pool = nullptr; }

  //! Ensures that there is at least one object in the pool.
  BL_NODISCARD
  BL_INLINE bool ensure(BLArenaAllocator& arena) noexcept {
    if (_pool) return true;

    Link* p = static_cast<Link*>(arena.alloc(SizeOfT));
    if (p == nullptr) return false;

    p->next = nullptr;
    _pool = p;
    return true;
  }

  //! Allocates a memory (or reuses the existing allocation) of `SizeOfT` (in bytes).
  BL_NODISCARD
  BL_INLINE T* alloc(BLArenaAllocator& arena) noexcept {
    Link* p = _pool;
    if (BL_UNLIKELY(p == nullptr))
      return static_cast<T*>(arena.alloc(SizeOfT));
    _pool = p->next;
    return static_cast<T*>(static_cast<void*>(p));
  }

  //! Like `alloc()`, but can be only called after `ensure()` returned `true`.
  BL_NODISCARD
  BL_INLINE T* allocEnsured() noexcept {
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

//! \}
//! \endcond

#endif // BLEND2D_SUPPORT_ARENAALLOCATOR_P_H_INCLUDED
