// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_RASTERWORKQUEUE_P_H
#define BLEND2D_RASTER_RASTERWORKQUEUE_P_H

#include "../zonelist_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct BLRasterCommand;
struct BLRasterJobData;

// ============================================================================
// [Constants]
// ============================================================================

enum : uint32_t { BL_RASTER_QUEUE_BLOCK_CAPACITY = 256 };

// ============================================================================
// [BLRasterWorkQueue]
// ============================================================================

//! A queue used to store rendering context jobs or commands.
//!
//! \note `BLRasterWorkQueueAppender` is used to add items to the queue.
template<typename T>
class BLRasterWorkQueue : public BLZoneListNode<BLRasterWorkQueue<T>> {
public:
  BL_NONCOPYABLE(BLRasterWorkQueue)

  //! Number of items in the queue.
  size_t _size;

  BL_INLINE BLRasterWorkQueue() noexcept
    : _size(0) {}

  BL_INLINE void reset(size_t size = 0) noexcept { _size = size; }

  BL_INLINE bool empty() const noexcept { return _size == 0; }
  BL_INLINE size_t size() const noexcept { return _size; }
  BL_INLINE size_t capacity() const noexcept { return BL_RASTER_QUEUE_BLOCK_CAPACITY; }

  BL_INLINE T* data() noexcept { return (T*)(this + 1); }
  BL_INLINE const T* data() const noexcept { return (T*)(this + 1); }

  BL_INLINE T* begin() noexcept { return (T*)(this + 1); }
  BL_INLINE const T* begin() const noexcept { return (T*)(this + 1); }

  BL_INLINE T* end() noexcept { return data() + _size; }
  BL_INLINE const T* end() const noexcept { return data() + _size; }

  BL_INLINE T& at(size_t index) noexcept {
    BL_ASSERT(index < _size);
    return data()[index];
  }

  BL_INLINE const T& at(size_t index) const noexcept {
    BL_ASSERT(index < _size);
    return data()[index];
  }

  static BL_INLINE size_t sizeOf() noexcept {
    return sizeof(BLRasterWorkQueue<T>) + sizeof(T) * BL_RASTER_QUEUE_BLOCK_CAPACITY;
  }
};

typedef BLRasterWorkQueue<BLRasterCommand> BLRasterCommandQueue;
typedef BLRasterWorkQueue<BLRasterJobData*> BLRasterJobQueue;
typedef BLRasterWorkQueue<BLRasterFetchData*> BLRasterFetchQueue;

// ============================================================================
// [BLRasterWorkQueueAppender]
// ============================================================================

//! A queue appender - appends items to `BLRasterWorkQueue`.
template<typename T>
class BLRasterWorkQueueAppender {
public:
  BL_NONCOPYABLE(BLRasterWorkQueueAppender)

  //! Current position in the queue (next item will be added exactly here).
  T* _ptr;
  //! End of the queue.
  T* _end;

  BL_INLINE BLRasterWorkQueueAppender() noexcept
    : _ptr(nullptr),
      _end(nullptr) {}

  BL_INLINE bool full() const noexcept { return _ptr == _end; }

  BL_INLINE void reset() noexcept { reset(nullptr, nullptr); }
  BL_INLINE void reset(BLRasterWorkQueue<T>& queue) noexcept {
    _ptr = queue.data();
    _end = queue.data() + queue.capacity();
  }

  BL_INLINE size_t index(const BLRasterWorkQueue<T>& queue) const noexcept {
    return (size_t)(_ptr - queue.data());
  }

  BL_INLINE void done(BLRasterWorkQueue<T>& queue) noexcept {
    queue._size = index(queue);
  }

  BL_INLINE void append(const T& item) noexcept {
    BL_ASSERT(!full());
    *_ptr++ = item;
  }

  //! Used when the data of the next command were already assigned to just
  //! advance the pointer. This should be only used by command queue, other
  //! queues should use `append()`.
  BL_INLINE void advance() noexcept {
    BL_ASSERT(!full());
    _ptr++;
  }
};

typedef BLRasterWorkQueueAppender<BLRasterCommand> BLRasterCommandQueueAppender;
typedef BLRasterWorkQueueAppender<BLRasterJobData*> BLRasterJobQueueAppender;
typedef BLRasterWorkQueueAppender<BLRasterFetchData*> BLRasterFetchQueueAppender;

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERWORKQUEUE_P_H
