// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_WORKERMANAGER_P_H_INCLUDED
#define BLEND2D_RASTER_WORKERMANAGER_P_H_INCLUDED

#include "../geometry_p.h"
#include "../image.h"
#include "../path.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/renderbatch_p.h"
#include "../raster/renderqueue_p.h"
#include "../raster/statedata_p.h"
#include "../raster/workdata_p.h"
#include "../raster/workersynchronization_p.h"
#include "../support/arenaallocator_p.h"
#include "../support/arenalist_p.h"
#include "../support/intops_p.h"
#include "../support/zeroallocator_p.h"
#include "../threading/atomic_p.h"
#include "../threading/conditionvariable_p.h"
#include "../threading/mutex_p.h"
#include "../threading/thread_p.h"
#include "../threading/threadpool_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace bl {
namespace RasterEngine {

template<typename T>
struct PreallocatedStructPool {
  //! \name Members
  //! \{

  T* ptr {};
  T* end {};

  //! \}

  //! \name Interface
  //! \{

  BL_INLINE_NODEBUG bool exhausted() const noexcept { return ptr >= end; }

  BL_INLINE_NODEBUG void reset() noexcept {
    ptr = nullptr;
    end = nullptr;
  }

  BL_INLINE void advance(size_t n = 1) noexcept {
    BL_ASSERT(!exhausted());
    ptr += n;
  }

  BL_INLINE BLResult preallocate(ArenaAllocator& allocator, uint32_t count) noexcept {
    constexpr uint32_t kAlignment = uint32_t(alignof(T));
    constexpr uint32_t kItemSize = uint32_t(sizeof(T));

    allocator.align(kAlignment);

    size_t remaining = allocator.remainingSize();

    // If there is not enough space to allocate all the items, then reduce the number of items to be allocated.
    // This makes it possible to use memory that would otherwise be wasted (the allocation of the requested number
    // of items would require a new block).
    if (remaining >= kItemSize && remaining < count * kItemSize)
      count = uint32_t(remaining) / kItemSize;

    T* allocatedPtr = allocator.allocT<T>(count * kItemSize, kAlignment);
    if (BL_UNLIKELY(!allocatedPtr))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    ptr = allocatedPtr;
    end = allocatedPtr + count;
    return BL_SUCCESS;
  }

  //! \}
};

struct PreallocatedBytePool {
  //! \name Members
  //! \{

  uint8_t* ptr {};
  uint8_t* end {};

  //! \}

  //! \name Interface
  //! \{

  BL_INLINE_NODEBUG bool exhausted() const noexcept {
    // NOTE: Must `ptr >= end` as we might over-allocate for some specific purposes (like allocating 2 things at
    // once). In such case the over-allocation is not accounted in the `end` pointer and the code that uses the
    // pool simply allocates more - but it must guarantee that it doesn't allocate more than extra bytes reserved
    // for this use-case.
    //
    // Most often this would be used when both Fill and Stroke shared states have to be created - to simplify the
    // logic and minimize error handling in the rendering context, both states are allocated at once.
    return ptr >= end;
  }

  BL_INLINE_NODEBUG void reset() noexcept {
    ptr = nullptr;
    end = nullptr;
  }

  BL_INLINE void* alloc(size_t size) noexcept {
    void* p = ptr;
    ptr += size;
    return p;
  }

  BL_INLINE BLResult preallocate(ArenaAllocator& allocator, uint32_t minimumSize, uint32_t defaultSize, uint32_t extraSize, uint32_t alignment) noexcept {
    allocator.align(alignment);

    size_t remaining = allocator.remainingSize();
    size_t n = defaultSize;

    // Just consume everything in case the buffer is not long enough to hold the `defaultSize`, however,
    // also check whether it can hold `minimumByteCount` - if not, a new buffer has to be allocated, which would
    // be handled by `allocator.alloc()` automatically when the required allocation size exceeds the remaining
    // capacity of the current block.
    if (remaining >= minimumSize + extraSize && remaining < defaultSize + extraSize)
      n = remaining - extraSize;

    uint8_t* allocatedPtr = static_cast<uint8_t*>(allocator.alloc(n + extraSize, alignment));
    if (BL_UNLIKELY(!allocatedPtr))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    ptr = allocatedPtr;
    end = allocatedPtr + n;
    return BL_SUCCESS;
  }

  //! \}
};

class WorkerManager {
public:
  BL_NONCOPYABLE(WorkerManager)

  enum : uint32_t { kAllocatorAlignment = 8 };

  //! \name Members
  //! \{

  //! Zone allocator used to allocate commands, jobs, and related data.
  ArenaAllocator _allocator;

  //! Current batch where objects are appended to.
  RenderBatch* _currentBatch;
  //! Command appender.
  RenderCommandAppender _commandAppender;
  //! Job appender.
  RenderJobAppender _jobAppender;

  //! Preallocated fetch data - multiple FetchData structs are allocated at a time, and then used during dispatching.
  PreallocatedStructPool<RenderFetchData> _fetchDataPool;

  //! Preallocated shared data pool - used by shared fill and stroke states.
  PreallocatedBytePool _sharedDataPool;

  //! Thread-pool that owns worker threads.
  BLThreadPool* _threadPool;
  //! Worker threads acquired from `_threadPool`.
  BLThread** _workerThreads;
  //! Work data for each worker thread.
  WorkData** _workDataStorage;

  //! Work synchronization
  WorkerSynchronization _synchronization;

  //! Indicates that a worker manager is active.
  uint32_t _isActive;
  //! Number of worker threads.
  uint32_t _threadCount;
  //! Number of bands,
  uint32_t _bandCount;
  //! Batch id, an incrementing number that is assigned to FetchData.
  uint32_t _batchId;
  //! Number of commands in the queue.
  uint32_t _commandQueueCount;
  //! Maximum number of commands in a queue.
  uint32_t _commandQueueLimit;
  //! Count of data slots.
  uint32_t _stateSlotCount;

  //! \}

  //! \name Construction & Destruction
  //! \{

  BL_INLINE WorkerManager() noexcept
    : _allocator(131072 - ArenaAllocator::kBlockOverhead, kAllocatorAlignment),
      _currentBatch{},
      _commandAppender{},
      _jobAppender{},
      _fetchDataPool{},
      _sharedDataPool{},
      _threadPool{},
      _workerThreads{},
      _workDataStorage{},
      _synchronization(),
      _isActive{},
      _threadCount{},
      _bandCount{},
      _batchId{1},
      _commandQueueCount{},
      _commandQueueLimit{},
      _stateSlotCount{} {}

  BL_INLINE ~WorkerManager() noexcept {
    // Cannot be active upon destruction!
    BL_ASSERT(!isActive());
  }

  //! \}

  //! \name Explicit Initialization
  //! \{

  //! Initializes the worker manager with the specified number of threads.
  BLResult init(BLRasterContextImpl* ctxI, const BLContextCreateInfo* createInfo) noexcept;

  BLResult initWorkMemory(size_t zeroedMemorySize) noexcept;

  BL_INLINE void initFirstBatch() noexcept {
    RenderBatch* batch = _allocator.allocZeroedT<RenderBatch>();

    // We have preallocated enough, cannot happen.
    BL_ASSERT(batch != nullptr);

    batch->_commandList.reset(newCommandQueue());
    batch->_jobList.reset(newJobQueue());

    // We have preallocated enough, cannot happen.
    BL_ASSERT(batch->_commandList.first() != nullptr);
    BL_ASSERT(batch->_jobList.first() != nullptr);

    _currentBatch = batch;
    _jobAppender.reset(*batch->_jobList.first());
    _commandAppender.reset(*batch->_commandList.first());

    BLResult result = _preallocateFetchDataPool() |
                      _preallocateSharedDataPool();

    // We have preallocated enough, cannot happen.
    BL_ASSERT(result == BL_SUCCESS);
    blUnused(result);

    _commandQueueCount = 0;
    _stateSlotCount = 0;
  }

  //! Releases all acquired threads and destroys all work contexts.
  //!
  //! \note It's only safe to call `reset()` after all threads have finalized their work. It would be disaster to
  //! call `reset()` when one or more thread is still running as reset destroys all work contexts, so the threads
  //! would be using freed memory.
  void reset() noexcept;

  //! \}

  //! \name Interface
  //! \{

  //! Returns `true` when the worker manager is active.
  BL_INLINE_NODEBUG bool isActive() const noexcept { return _isActive != 0; }

  BL_INLINE_NODEBUG uint32_t threadCount() const noexcept { return _threadCount; }

  //! \}

  //! \name Command Data
  //! \{

  BL_INLINE_NODEBUG RenderCommandAppender& commandAppender() noexcept { return _commandAppender; }

  BL_INLINE_NODEBUG RenderCommand* currentCommand() noexcept { return _commandAppender.currentCommand(); }
  BL_INLINE_NODEBUG uint32_t nextStateSlotIndex() noexcept { return _stateSlotCount++; }

  BL_INLINE_NODEBUG bool isCommandQueueFull() const noexcept { return _commandAppender.full(); }

  BL_INLINE bool hasPendingCommands() const noexcept {
    RenderCommandQueue* first = _currentBatch->_commandList.first();
    RenderCommandQueue* last = _currentBatch->_commandList.last();

    return first != last || !_commandAppender.empty();
  }

  BL_INLINE RenderCommandQueue* newCommandQueue() noexcept {
    RenderCommandQueue* p = _allocator.allocNoAlignT<RenderCommandQueue>(IntOps::alignUp(RenderCommandQueue::sizeOf(), kAllocatorAlignment));
    if (BL_UNLIKELY(!p))
      return nullptr;
    return new(BLInternal::PlacementNew{p}) RenderCommandQueue();
  }

  BL_INLINE void beforeGrowCommandQueue() noexcept {
    _commandQueueCount += kRenderQueueCapacity;
  }

  BL_INLINE BLResult _growCommandQueue() noexcept {
    // Can only be called when the current command queue is full.
    BL_ASSERT(_commandAppender.full());

    RenderCommandQueue* commandQueue = currentBatch()->_commandList.last();
    _commandAppender.done(*commandQueue);
    currentBatch()->_commandCount += uint32_t(commandQueue->size());

    commandQueue = newCommandQueue();
    if (BL_UNLIKELY(!commandQueue))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    currentBatch()->_commandList.append(commandQueue);
    _commandAppender.reset(*commandQueue);

    return BL_SUCCESS;
  }

  //! \}

  //! \name Job Data
  //! \{

  BL_INLINE_NODEBUG bool isJobQueueFull() const noexcept { return _jobAppender.full(); }

  BL_INLINE RenderJobQueue* newJobQueue() noexcept {
    RenderJobQueue* p = _allocator.allocNoAlignT<RenderJobQueue>(IntOps::alignUp(RenderJobQueue::sizeOf(), kAllocatorAlignment));
    if (BL_UNLIKELY(!p))
      return nullptr;
    p->reset();
    return p;
  }

  BL_INLINE BLResult _growJobQueue() noexcept {
    // Can only be called when the current job queue is full.
    BL_ASSERT(_jobAppender.full());

    RenderJobQueue* jobQueue = currentBatch()->_jobList.last();
    _jobAppender.done(*jobQueue);
    currentBatch()->_jobCount += uint32_t(jobQueue->size());

    jobQueue = newJobQueue();
    if (BL_UNLIKELY(!jobQueue))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    currentBatch()->_jobList.append(jobQueue);
    _jobAppender.reset(*jobQueue);

    return BL_SUCCESS;
  }

  BL_INLINE void addJob(RenderJob* job) noexcept {
    BL_ASSERT(!isJobQueueFull());
    _jobAppender.append(job);
  }

  //! \}

  //! \name Fetch Data
  //! \{

  BL_INLINE_NODEBUG bool isFetchDataPoolExhausted() const noexcept {
    return _fetchDataPool.exhausted();
  }

  BL_INLINE BLResult _preallocateFetchDataPool() noexcept {
    return _fetchDataPool.preallocate(_allocator, 32);
  }

  //! \}

  //! \name Shared Data
  //! \{

  BL_INLINE_NODEBUG bool isSharedDataPoolExhausted() const noexcept {
    return _sharedDataPool.exhausted();
  }

  BL_INLINE BLResult _preallocateSharedDataPool() noexcept {
    constexpr uint32_t kCombinedStateSize = uint32_t(sizeof(SharedFillState) + sizeof(SharedExtendedStrokeState));

    constexpr uint32_t kMinimumSize = kCombinedStateSize;
    constexpr uint32_t kDefaultSize = kCombinedStateSize * 20;
    constexpr uint32_t kExtraSize = kCombinedStateSize;

    return _sharedDataPool.preallocate(_allocator, kMinimumSize, kDefaultSize, kExtraSize, 16u);
  }

  template<typename T>
  BL_INLINE T* allocateFromSharedDataPool(size_t size = sizeof(T)) noexcept {
    return static_cast<T*>(_sharedDataPool.alloc(size));
  }

  //! \}

  //! \name Work Batch
  //! \{

  BL_INLINE_NODEBUG RenderBatch* currentBatch() const noexcept { return _currentBatch; }
  BL_INLINE_NODEBUG uint32_t currentBatchId() const noexcept { return _batchId; }

  BL_INLINE_NODEBUG bool isBatchFull() const noexcept { return _commandQueueCount >= _commandQueueLimit; }

  BL_INLINE void finalizeBatch() noexcept {
    RenderJobQueue* lastJobQueue = _currentBatch->_jobList.last();
    RenderCommandQueue* lastCommandQueue = _currentBatch->_commandList.last();

    _jobAppender.done(*lastJobQueue);
    _commandAppender.done(*lastCommandQueue);

    _currentBatch->_workerCount = _threadCount + 1;
    _currentBatch->_jobCount += uint32_t(lastJobQueue->size());
    _currentBatch->_commandCount += uint32_t(lastCommandQueue->size());
    _currentBatch->_stateSlotCount = _stateSlotCount;
    _currentBatch->_bandCount = _bandCount;
    // TODO: [Rendering Context] Not used. the idea is that after the batch is processed we can reuse the blocks of the allocator (basically move it after the current block).
    // _currentBatch->_pastBlock = _allocator.pastBlock();

    if (++_batchId == 0)
      _batchId = 1;

    _commandQueueCount = 0;
    _stateSlotCount = 0;
  }

  //! \}
};

} // {RasterEngine}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_WORKERMANAGER_P_H_INCLUDED
