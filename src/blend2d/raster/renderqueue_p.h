// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RENDERQUEUE_P_H_INCLUDED
#define BLEND2D_RASTER_RENDERQUEUE_P_H_INCLUDED

#include "../support/arenalist_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace BLRasterEngine {

struct RenderCommand;
struct RenderFetchData;
struct RenderJob;

//! A queue used to store rendering context jobs or commands.
//!
//! \note `RenderQueueAppender` is used to add items to the queue.
template<typename T>
class RenderQueue : public BLArenaListNode<RenderQueue<T>> {
public:
  BL_NONCOPYABLE(RenderQueue)

  enum : uint32_t { kQueueBlockCapacity = 256 };

  //! Number of items in the queue.
  size_t _size;

  BL_INLINE RenderQueue() noexcept
    : _size(0) {}

  BL_INLINE void reset(size_t size = 0) noexcept { _size = size; }

  BL_INLINE bool empty() const noexcept { return _size == 0; }
  BL_INLINE size_t size() const noexcept { return _size; }
  BL_INLINE size_t capacity() const noexcept { return kQueueBlockCapacity; }

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
    return sizeof(RenderQueue<T>) + sizeof(T) * kQueueBlockCapacity;
  }
};

typedef RenderQueue<RenderJob*> RenderJobQueue;
typedef RenderQueue<RenderFetchData*> RenderFetchQueue;
typedef RenderQueue<BLImageCore> RenderImageQueue;
typedef RenderQueue<RenderCommand> RenderCommandQueue;

//! A queue appender - appends items to `RenderQueue`.
template<typename T>
class RenderQueueAppender {
public:
  BL_NONCOPYABLE(RenderQueueAppender)

  //! Current position in the queue (next item will be added exactly here).
  T* _ptr;
  //! End of the queue.
  T* _end;

  BL_INLINE RenderQueueAppender() noexcept
    : _ptr(nullptr),
      _end(nullptr) {}

  BL_INLINE bool full() const noexcept { return _ptr == _end; }

  BL_INLINE void reset() noexcept { reset(nullptr, nullptr); }
  BL_INLINE void reset(RenderQueue<T>& queue) noexcept {
    _ptr = queue.data();
    _end = queue.data() + queue.capacity();
  }

  BL_INLINE size_t index(const RenderQueue<T>& queue) const noexcept {
    return (size_t)(_ptr - queue.data());
  }

  BL_INLINE void done(RenderQueue<T>& queue) noexcept {
    queue._size = index(queue);
  }

  BL_INLINE void append(const T& item) noexcept {
    BL_ASSERT(!full());
    *_ptr++ = item;
  }

  //! Used when the data of the next command were already assigned to just advance the pointer. This should be
  //! only used by command queue, other queues should use `append()`.
  BL_INLINE void advance() noexcept {
    BL_ASSERT(!full());
    _ptr++;
  }
};

typedef RenderQueueAppender<RenderCommand> RenderCommandAppender;
typedef RenderQueueAppender<RenderJob*> RenderJobAppender;
typedef RenderQueueAppender<RenderFetchData*> RenderFetchDataAppender;
typedef RenderQueueAppender<BLImageCore> RenderImageAppender;

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RENDERQUEUE_P_H_INCLUDED
