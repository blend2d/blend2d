// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_WORKERMANAGER_P_H_INCLUDED
#define BLEND2D_RASTER_WORKERMANAGER_P_H_INCLUDED

#include "../geometry_p.h"
#include "../image.h"
#include "../path.h"
#include "../zeroallocator_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/renderbatch_p.h"
#include "../raster/renderqueue_p.h"
#include "../raster/workdata_p.h"
#include "../raster/workersynchronization_p.h"
#include "../support/arenaallocator_p.h"
#include "../support/arenalist_p.h"
#include "../support/intops_p.h"
#include "../threading/atomic_p.h"
#include "../threading/conditionvariable_p.h"
#include "../threading/mutex_p.h"
#include "../threading/thread_p.h"
#include "../threading/threadpool_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace BLRasterEngine {

class WorkerManager {
public:
  BL_NONCOPYABLE(WorkerManager)

  enum : uint32_t { kAllocatorAlignment = 8 };

  //! Zone allocator used to allocate commands and jobs.
  BLArenaAllocator _allocator;

  //! Current batch where objects are appended to.
  RenderBatch* _currentBatch;
  //! Job appender.
  RenderJobAppender _jobAppender;
  //! Fetch data appender.
  RenderFetchDataAppender _fetchDataAppender;
  //! Object appender.
  RenderImageAppender _imageAppender;
  //! Command appender.
  RenderCommandAppender _commandAppender;

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

  BL_INLINE WorkerManager() noexcept
    : _allocator(131072 - BLArenaAllocator::kBlockOverhead, kAllocatorAlignment),
      _currentBatch(nullptr),
      _jobAppender(),
      _fetchDataAppender(),
      _imageAppender(),
      _commandAppender(),
      _threadPool(nullptr),
      _workerThreads(nullptr),
      _workDataStorage(nullptr),
      _synchronization(),
      _isActive(0),
      _threadCount(0),
      _bandCount(0),
      _batchId(1),
      _commandQueueCount(0),
      _commandQueueLimit(0),
      _stateSlotCount(0) {}

  BL_INLINE ~WorkerManager() noexcept {
    // Cannot be initialized upon destruction!
    BL_ASSERT(!isActive());
  }

  //! Returns `true` when the worker manager is active.
  BL_INLINE bool isActive() const noexcept { return _isActive != 0; }

  //! Initializes the worker manager with the specified number of threads.
  BLResult init(BLRasterContextImpl* ctxI, const BLContextCreateInfo* createInfo) noexcept;

  BL_INLINE void initFirstBatch() noexcept {
    RenderBatch* batch = _allocator.newT<RenderBatch>();
    BL_ASSERT(batch != nullptr); // We have preallocated enough, cannot happen.

    batch->_jobList.reset(newJobQueue());
    batch->_fetchList.reset(newFetchQueue());
    batch->_imageList.reset(newImageQueue());
    batch->_commandList.reset(newCommandQueue());

    _currentBatch = batch;
    _jobAppender.reset(*batch->_jobList.first());
    _fetchDataAppender.reset(*batch->_fetchList.first());
    _imageAppender.reset(*batch->_imageList.first());
    _commandAppender.reset(*batch->_commandList.first());

    _commandQueueCount = 0;
    _stateSlotCount = 0;
  }

  //! Releases all acquired threads and destroys all work contexts.
  //!
  //! \note It's only safe to call `reset()` after all threads have finalized their work. It would be disaster to
  //! call `reset()` when one or more thread is still running as reset destroys all work contexts, so the threads
  //! would be using freed memory.
  void reset() noexcept;

  //! \name Accessors
  //! \{

  BL_INLINE uint32_t threadCount() const noexcept { return _threadCount; }

  //! \}

  //! \name Job Data
  //! \{

  BL_INLINE RenderJobQueue* newJobQueue() noexcept {
    void* p = _allocator.allocNoAlignT<RenderJobQueue>(BLIntOps::alignUp(RenderJobQueue::sizeOf(), kAllocatorAlignment));
    return p ? new(BLInternal::PlacementNew{p}) RenderJobQueue() : nullptr;
  }

  BL_INLINE BLResult ensureJobQueue() noexcept {
    if (!_jobAppender.full())
      return BL_SUCCESS;
    else
      return _growJobQueue();
  }

  BL_INLINE BLResult _growJobQueue() noexcept {
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
    BL_ASSERT(!_jobAppender.full());
    _jobAppender.append(job);
  }

  //! \}

  //! \name Fetch Data
  //! \{

  BL_INLINE RenderFetchQueue* newFetchQueue() noexcept {
    void* p = _allocator.allocNoAlignT<RenderFetchQueue>(BLIntOps::alignUp(RenderFetchQueue::sizeOf(), kAllocatorAlignment));
    return p ? new(BLInternal::PlacementNew{p}) RenderFetchQueue() : nullptr;
  }

  BL_INLINE BLResult ensureFetchQueue() noexcept {
    if (!_fetchDataAppender.full())
      return BL_SUCCESS;
    else
      return _growFetchQueue();
  }

  BL_NOINLINE BLResult _growFetchQueue() noexcept {
    BL_ASSERT(_fetchDataAppender.full());

    RenderFetchQueue* fetchQueue = currentBatch()->_fetchList.last();
    _fetchDataAppender.done(*fetchQueue);

    fetchQueue = newFetchQueue();
    if (BL_UNLIKELY(!fetchQueue))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    currentBatch()->_fetchList.append(fetchQueue);
    _fetchDataAppender.reset(*fetchQueue);

    return BL_SUCCESS;
  }

  //! \}

  //! \name Image Data
  //! \{

  BL_INLINE RenderImageQueue* newImageQueue() noexcept {
    void* p = _allocator.allocNoAlignT<RenderImageQueue>(BLIntOps::alignUp(RenderImageQueue::sizeOf(), kAllocatorAlignment));
    return p ? new(BLInternal::PlacementNew{p}) RenderImageQueue() : nullptr;
  }

  BL_INLINE BLResult ensureImageQueue() noexcept {
    if (!_imageAppender.full())
      return BL_SUCCESS;
    else
      return _growObjectQueue();
  }

  BL_NOINLINE BLResult _growObjectQueue() noexcept {
    BL_ASSERT(_imageAppender.full());

    RenderImageQueue* imageQueue = currentBatch()->_imageList.last();
    _imageAppender.done(*imageQueue);
    currentBatch()->_commandCount += uint32_t(imageQueue->size());

    imageQueue = newImageQueue();
    if (BL_UNLIKELY(!imageQueue))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    currentBatch()->_imageList.append(imageQueue);
    _imageAppender.reset(*imageQueue);

    return BL_SUCCESS;
  }

  //! \}

  //! \name Command Data
  //! \{

  BL_INLINE bool hasPendingCommands() const noexcept {
    RenderCommandQueue* first = _currentBatch->_commandList.first();
    RenderCommandQueue* last = _currentBatch->_commandList.last();

    return first != last || _commandAppender.index(*last) != 0;
  }

  BL_INLINE RenderCommandQueue* newCommandQueue() noexcept {
    void* p = _allocator.allocNoAlignT<RenderCommandQueue>(BLIntOps::alignUp(RenderCommandQueue::sizeOf(), kAllocatorAlignment));
    return p ? new(BLInternal::PlacementNew{p}) RenderCommandQueue() : nullptr;
  }

  BL_INLINE BLResult ensureCommandQueue() noexcept {
    if (!_commandAppender.full())
      return BL_SUCCESS;
    else
      return _growCommandQueue();
  }

  BL_NOINLINE BLResult _growCommandQueue() noexcept {
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

  BL_INLINE RenderCommand* currentCommandData() noexcept {
    return _commandAppender._ptr;
  }

  BL_INLINE uint32_t nextStateSlotIndex() noexcept {
    return _stateSlotCount++;
  }

  //! \}

  //! \name Work Batch
  //! \{

  BL_INLINE RenderBatch* currentBatch() const noexcept { return _currentBatch; }

  BL_INLINE uint32_t currentBatchId() const noexcept { return _batchId; }

  BL_INLINE void finalizeBatch() noexcept {
    RenderJobQueue* lastJobQueue = _currentBatch->_jobList.last();
    RenderFetchQueue* lastFetchQueue = _currentBatch->_fetchList.last();
    RenderImageQueue* lastImageQueue = _currentBatch->_imageList.last();
    RenderCommandQueue* lastCommandQueue = _currentBatch->_commandList.last();

    _jobAppender.done(*lastJobQueue);
    _fetchDataAppender.done(*lastFetchQueue);
    _imageAppender.done(*lastImageQueue);
    _commandAppender.done(*lastCommandQueue);

    _currentBatch->_workerCount = _threadCount + 1;
    _currentBatch->_jobCount += uint32_t(lastJobQueue->size());
    _currentBatch->_commandCount += uint32_t(lastCommandQueue->size());
    _currentBatch->_stateSlotCount = _stateSlotCount;
    _currentBatch->_bandCount = _bandCount;
    // TODO: Not used. the idea is that after the batch is processed we can reuse the blocks of the allocator (basically move it after the current block).
    // _currentBatch->_pastBlock = _allocator.pastBlock();

    if (++_batchId == 0)
      _batchId = 1;

    _commandQueueCount = 0;
    _stateSlotCount = 0;
  }

  //! \}
};

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_WORKERMANAGER_P_H_INCLUDED
