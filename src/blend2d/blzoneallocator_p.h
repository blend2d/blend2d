// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLZONEALLOCATOR_P_H
#define BLEND2D_BLZONEALLOCATOR_P_H

#include "./blapi-internal_p.h"
#include "./blsupport_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLZoneAllocator]
// ============================================================================

//! Zone memory allocator.
//!
//! Zone is an incremental memory allocator that allocates memory by simply
//! incrementing a pointer. It allocates blocks of memory by using standard
//! C library `malloc/free`, but divides these blocks into smaller chunks
//! requested by calling `BLZoneAllocator::alloc()` and friends.
//!
//! Zone memory allocators are designed to allocate data of short lifetime.
//!
//! NOTE: It's not recommended to use `BLZoneAllocator` to allocate larger data
//! structures than the initial `blockSize` passed to `BLZoneAllocator()`
//! constructor. The block size should be always greater than the maximum `size`
//! passed to `alloc()`. Zone is designed to handle such cases, but it may
//! allocate new block for each call to `alloc()` that exceeds the default block
//! size.
class BLZoneAllocator {
public:
  BL_NONCOPYABLE(BLZoneAllocator)

  //! A single block of memory managed by `BLZoneAllocator`.
  struct Block {
    BL_INLINE uint8_t* data() const noexcept {
      return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(this) + sizeof(*this));
    }

    //! Link to the previous block.
    Block* prev;
    //! Link to the next block.
    Block* next;
    //! Size of the block.
    size_t size;
  };

  //! Saved state, used by `BLZoneAllocator::saveState()` and `BLZoneAllocator::restoreState()`.
  struct State {
    //! Current pointer.
    uint8_t* ptr;
    //! End pointer.
    uint8_t* end;
    //! Current block.
    Block* block;
  };

  enum Limits : size_t {
    kBlockSize = sizeof(Block),
    kBlockOverhead = BL_ALLOC_OVERHEAD + sizeof(Block),

    kMinBlockSize = 64, // The number is ridiculously small, but still possible.
    kMaxBlockSize = size_t(1) << (sizeof(size_t) * 8 - 4 - 1),
    kMinAlignment = 1,
    kMaxAlignment = 64
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
      size_t _blockSize : blBitSizeOf<size_t>() - 4;
      //! First block is temporary (BLZoneAllocatorTmp).
      size_t _hasStaticBlock : 1;
      //! Block alignment (1 << alignment).
      size_t _blockAlignmentShift : 3;
    };
    size_t _packedData;
  };

  static BL_HIDDEN const Block _zeroBlock;

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create a new `BLZoneAllocator`.
  //!
  //! The `blockSize` parameter describes the default size of the block. If the
  //! `size` parameter passed to `alloc()` is greater than the default size
  //! `BLZoneAllocator` will allocate and use a larger block, but it will not change the
  //! default `blockSize`.
  //!
  //! It's not required, but it's good practice to set `blockSize` to a
  //! reasonable value that depends on the usage of `BLZoneAllocator`. Greater block sizes
  //! are generally safer and perform better than unreasonably low block sizes.
  BL_INLINE explicit BLZoneAllocator(size_t blockSize, size_t blockAlignment = 1) noexcept {
    _init(blockSize, blockAlignment, nullptr, 0);
  }

  BL_INLINE BLZoneAllocator(size_t blockSize, size_t blockAlignment, void* staticData, size_t staticSize) noexcept {
    _init(blockSize, blockAlignment, staticData, staticSize);
  }

  //! Destroy the `BLZoneAllocator` instance.
  //!
  //! This will destroy the `BLZoneAllocator` instance and release all blocks of memory
  //! allocated by it. It performs implicit `reset()`.
  BL_INLINE ~BLZoneAllocator() noexcept { reset(); }

  // --------------------------------------------------------------------------
  // [Init / Reset]
  // --------------------------------------------------------------------------

  BL_HIDDEN void _init(size_t blockSize, size_t blockAlignment, void* staticData, size_t staticSize) noexcept;

  //! Invalidates all allocations and moves the current block pointer to the
  //! first block. It's similar to `reset()`, however, it doesn't free blocks
  //! of memory it holds.
  BL_INLINE void clear() noexcept {
    Block* cur = _block;
    while (cur->prev)
      cur = cur->prev;
    _assignBlock(cur);
  }

  //! Reset the `BLZoneAllocator` invalidating all blocks allocated.
  BL_HIDDEN void reset() noexcept;

  // --------------------------------------------------------------------------
  // [Swap]
  // --------------------------------------------------------------------------

  BL_INLINE void swap(BLZoneAllocator& other) noexcept {
    // This could lead to a disaster.
    BL_ASSERT(!this->hasStaticBlock());
    BL_ASSERT(!other.hasStaticBlock());

    std::swap(_ptr, other._ptr);
    std::swap(_end, other._end);
    std::swap(_block, other._block);
    std::swap(_packedData, other._packedData);
  }

  //! Get whether this `BLZoneAllocator` is actually a `BLZoneAllocatorTmp` that uses temporary memory.
  BL_INLINE bool hasStaticBlock() const noexcept { return _hasStaticBlock != 0; }

  //! Get the default block size.
  BL_INLINE size_t blockSize() const noexcept { return _blockSize; }
  //! Get the default block alignment.
  BL_INLINE size_t blockAlignment() const noexcept { return size_t(1) << _blockAlignmentShift; }
  //! Get remaining size of the current block.
  BL_INLINE size_t remainingSize() const noexcept { return (size_t)(_end - _ptr); }

  //! Get the current zone cursor (dangerous).
  //!
  //! This is a function that can be used to get exclusive access to the current
  //! block's memory buffer.
  template<typename T = uint8_t>
  BL_INLINE T* ptr() noexcept { return reinterpret_cast<T*>(_ptr); }
  //! Get the end of the current zone block, only useful if you use `ptr()`.
  template<typename T = uint8_t>
  BL_INLINE T* end() noexcept { return reinterpret_cast<T*>(_end); }

  // NOTE: The following two functions `setPtr()` and `setEnd()` can be used
  // to perform manual memory allocation in case that an incremental allocation
  // is needed - for example you build some data structure without knowing the
  // final size. This is used for example by BLAnalyticRasterizer to build list
  // of edges.

  //! Set the current zone pointer to `ptr` (must be within the current block).
  template<typename T>
  BL_INLINE void setPtr(T* ptr) noexcept {
    uint8_t* p = reinterpret_cast<uint8_t*>(ptr);
    BL_ASSERT(p >= _ptr && p <= _end);
    _ptr = p;
  }

  //! Set the end zone pointer to `end` (must be within the current block).
  template<typename T>
  BL_INLINE void setEnd(T* end) noexcept {
    uint8_t* p = reinterpret_cast<uint8_t*>(end);
    BL_ASSERT(p >= _ptr && p <= _end);
    _end = p;
  }

  //! Align the current pointer to `alignment`.
  BL_INLINE void align(size_t alignment) noexcept {
    _ptr = blMin(blAlignUp(_ptr, alignment), _end);
  }

  //! Ensure the remaining size is at least equal or greater than `size`.
  //!
  //! NOTE: This function doesn't respect any alignment. If you need to ensure
  //! there is enough room for an aligned allocation you need to call `align()`
  //! before calling `ensure()`.
  BL_INLINE BLResult ensure(size_t size) noexcept {
    if (size <= remainingSize())
      return BL_SUCCESS;
    else
      return _alloc(0, 1) ? BL_SUCCESS : blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  BL_INLINE void _assignBlock(Block* block) noexcept {
    size_t alignment = blockAlignment();
    _ptr = blAlignUp(block->data(), alignment);
    _end = blAlignDown(block->data() + block->size, alignment);
    _block = block;
  }

  BL_INLINE void _assignZeroBlock() noexcept {
    Block* block = const_cast<Block*>(&_zeroBlock);
    _ptr = block->data();
    _end = block->data();
    _block = block;
  }

  //! Internal alloc function.
  BL_HIDDEN void* _alloc(size_t size, size_t alignment) noexcept;

  //! Allocate the requested memory specified by `size`.
  //!
  //! Pointer returned is valid until the `BLZoneAllocator` instance is destroyed or reset
  //! by calling `reset()`. If you plan to make an instance of C++ from the
  //! given pointer use placement `new` and `delete` operators:
  //!
  //! ~~~
  //! class Object { ... };
  //!
  //! // Create Zone with default block size of approximately 65536 bytes.
  //! BLZoneAllocator zone(65536 - BLZoneAllocator::kBlockOverhead);
  //!
  //! // Create your objects using zone object allocating, for example:
  //! Object* obj = static_cast<Object*>(zone.alloc(sizeof(Object)));
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
  //! // Reset or destroy `BLZoneAllocator`.
  //! zone.reset();
  //! ~~~
  BL_INLINE void* alloc(size_t size) noexcept {
    if (BL_UNLIKELY(size > remainingSize()))
      return _alloc(size, 1);

    uint8_t* ptr = _ptr;
    _ptr += size;
    return static_cast<void*>(ptr);
  }

  //! Allocate the requested memory specified by `size` and `alignment`.
  //!
  //! Performs the same operation as `BLZoneAllocator::alloc(size)` with `alignment` applied.
  BL_INLINE void* alloc(size_t size, size_t alignment) noexcept {
    BL_ASSERT(blIsPowerOf2(alignment));
    uint8_t* ptr = blAlignUp(_ptr, alignment);

    if (size > (size_t)(_end - ptr))
      return _alloc(size, alignment);

    _ptr = ptr + size;
    return static_cast<void*>(ptr);
  }

  //! Allocate the requested memory specified by `size` without doing any checks.
  //!
  //! Can only be called if `remainingSize()` returns size at least equal to `size`.
  BL_INLINE void* allocNoCheck(size_t size) noexcept {
    BL_ASSERT(remainingSize() >= size);

    uint8_t* ptr = _ptr;
    _ptr += size;
    return static_cast<void*>(ptr);
  }

  //! Allocate the requested memory specified by `size` and `alignment` without doing any checks.
  //!
  //! Performs the same operation as `BLZoneAllocator::allocNoCheck(size)` with `alignment` applied.
  BL_INLINE void* allocNoCheck(size_t size, size_t alignment) noexcept {
    BL_ASSERT(blIsPowerOf2(alignment));

    uint8_t* ptr = blAlignUp(_ptr, alignment);
    BL_ASSERT(size <= (size_t)(_end - ptr));

    _ptr = ptr + size;
    return static_cast<void*>(ptr);
  }

  //! Allocate the requested memory specified by `size` and `alignment` and zero
  //! it before returning its pointer.
  //!
  //! See `alloc()` for more details.
  BL_HIDDEN void* allocZeroed(size_t size, size_t alignment = 1) noexcept;

  //! Like `alloc()`, but the return pointer is casted to `T*`.
  template<typename T>
  BL_INLINE T* allocT(size_t size = sizeof(T), size_t alignment = alignof(T)) noexcept {
    return static_cast<T*>(alloc(size, alignment));
  }

  //! Like `allocNoCheck()`, but the return pointer is casted to `T*`.
  template<typename T>
  BL_INLINE T* allocNoCheckT(size_t size = sizeof(T), size_t alignment = alignof(T)) noexcept {
    return static_cast<T*>(allocNoCheck(size, alignment));
  }

  //! Like `allocZeroed()`, but the return pointer is casted to `T*`.
  template<typename T>
  BL_INLINE T* allocZeroedT(size_t size = sizeof(T), size_t alignment = alignof(T)) noexcept {
    return static_cast<T*>(allocZeroed(size, alignment));
  }

  //! Like `new(std::nothrow) T(...)`, but allocated by `BLZoneAllocator`.
  template<typename T>
  BL_INLINE T* newT() noexcept {
    void* p = alloc(sizeof(T), alignof(T));
    if (BL_UNLIKELY(!p))
      return nullptr;
    return new(p) T();
  }

  //! Like `new(std::nothrow) T(...)`, but allocated by `BLZoneAllocator`.
  template<typename T, typename... Args>
  BL_INLINE T* newT(Args&&... args) noexcept {
    void* p = alloc(sizeof(T), alignof(T));
    if (BL_UNLIKELY(!p))
      return nullptr;
    return new(p) T(std::forward<Args>(args)...);
  }

  //! Store the current state to `state`.
  BL_INLINE void saveState(State* state) noexcept {
    state->ptr = _ptr;
    state->end = _end;
    state->block = _block;
  }

  //! Restore the state of `BLZoneAllocator` to the previously saved `State`.
  BL_INLINE void restoreState(State* state) noexcept {
    Block* block = state->block;
    _ptr = state->ptr;
    _end = state->end;
    _block = block;
  }
};

// ============================================================================
// [BLZoneAllocatorTmp]
// ============================================================================

//! A temporary `BLZoneAllocator`.
template<size_t N>
class BLZoneAllocatorTmp : public BLZoneAllocator {
public:
  BL_NONCOPYABLE(BLZoneAllocatorTmp<N>)

  BL_INLINE explicit BLZoneAllocatorTmp(size_t blockSize, size_t blockAlignment = 1) noexcept
    : BLZoneAllocator(blockSize, blockAlignment, _storage.data, N) {}

  struct Storage {
    char data[N];
  } _storage;
};

// ============================================================================
// [BLZonePool]
// ============================================================================

//! Helper class for implementing pooling of zone-allocated objects.
template<typename T, size_t SizeOfT = sizeof(T)>
class BLZonePool {
public:
  BL_NONCOPYABLE(BLZonePool)

  struct Link { Link* next; };
  BLZoneAllocator* _zone;
  Link* _pool;

  BL_INLINE BLZonePool(BLZoneAllocator* zone) noexcept
    : _zone(zone),
      _pool(nullptr) {}

  //! Reset the zone pool.
  //!
  //! Reset must be called after the associated `BLZoneAllocator` has been reset, otherwise
  //! the existing pool will collide with possible allocations made on the `BLZoneAllocator`
  //! object after the reset.
  BL_INLINE void reset() noexcept { _pool = nullptr; }

  //! Ensure that there is at least one object in the pool.
  BL_INLINE bool ensure() noexcept {
    if (_pool) return true;

    Link* p = static_cast<Link*>(_zone->alloc(SizeOfT));
    if (p == nullptr) return false;

    p->next = nullptr;
    _pool = p;
    return true;
  }

  //! Alloc a memory (or reuse the existing allocation) of `size` (in byts).
  BL_INLINE T* alloc() noexcept {
    Link* p = _pool;
    if (BL_UNLIKELY(p == nullptr))
      return static_cast<T*>(_zone->alloc(SizeOfT));
    _pool = p->next;
    return static_cast<T*>(static_cast<void*>(p));
  }

  //! Like `alloc()`, but can be only called after `ensure()` returned `true`.
  BL_INLINE T* allocEnsured() noexcept {
    Link* p = _pool;
    BL_ASSERT(p != nullptr);

    _pool = p->next;
    return static_cast<T*>(static_cast<void*>(p));
  }

  //! Pool the previously allocated memory.
  BL_INLINE void free(T* _p) noexcept {
    BL_ASSERT(_p != nullptr);
    Link* p = reinterpret_cast<Link*>(_p);

    p->next = _pool;
    _pool = p;
  }
};

//! \}
//! \endcond

#endif // BLEND2D_BLZONEALLOCATOR_P_H
