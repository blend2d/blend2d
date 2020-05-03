// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_RASTER_RASTERWORKERMANAGER_P_H_INCLUDED
#define BLEND2D_RASTER_RASTERWORKERMANAGER_P_H_INCLUDED

#include "../geometry_p.h"
#include "../image.h"
#include "../path.h"
#include "../region.h"
#include "../zeroallocator_p.h"
#include "../zoneallocator_p.h"
#include "../zonelist_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/rasterworkbatch_p.h"
#include "../raster/rasterworkdata_p.h"
#include "../raster/rasterworkqueue_p.h"
#include "../raster/rasterworksynchronization_p.h"
#include "../threading/atomic_p.h"
#include "../threading/conditionvariable_p.h"
#include "../threading/mutex_p.h"
#include "../threading/thread_p.h"
#include "../threading/threadpool_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

class BLRasterContextImpl;

// ============================================================================
// [BLRasterWorkerManager]
// ============================================================================

class BLRasterWorkerManager {
public:
  BL_NONCOPYABLE(BLRasterWorkerManager)

  //! Zone allocator used to allocate commands and jobs.
  BLZoneAllocator _allocator;

  //! The current batch.
  BLRasterWorkBatch* _currentBatch;
  //! Job queue appender.
  BLRasterJobQueueAppender _jobQueueAppender;
  //! Command queue appender.
  BLRasterCommandQueueAppender _commandQueueAppender;
  //! Fetch queue appender.
  BLRasterFetchQueueAppender _fetchQueueAppender;

  //! Thread-pool that owns worker threads.
  BLThreadPool* _threadPool;
  //! Worker threads acquired from `_threadPool`.
  BLThread** _workerThreads;
  //! Work data for each worker thread.
  BLRasterWorkData** _workDataStorage;

  //! Work synchronization
  BLRasterWorkSynchronization _synchronization;

  //! Indicates that a worker manager is active.
  uint32_t _isActive;
  //! Number of worker threads.
  uint32_t _workerCount;
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

  BL_INLINE BLRasterWorkerManager() noexcept
    : _allocator(65536 - BLZoneAllocator::kBlockOverhead, 8),
      _currentBatch(nullptr),
      _jobQueueAppender(),
      _commandQueueAppender(),
      _fetchQueueAppender(),
      _threadPool(nullptr),
      _workerThreads(nullptr),
      _workDataStorage(nullptr),
      _synchronization(),
      _isActive(0),
      _workerCount(0),
      _bandCount(0),
      _batchId(1),
      _commandQueueCount(0),
      _commandQueueLimit(0),
      _stateSlotCount(0) {}

  BL_INLINE ~BLRasterWorkerManager() noexcept {
    // Cannot be initialized upon destruction!
    BL_ASSERT(!isActive());
  }

  //! Returns `true` when the worker manager is active.
  BL_INLINE bool isActive() const noexcept { return _isActive != 0; }

  //! Initializes the worker manager with the specified number of threads.
  BLResult init(BLRasterContextImpl* ctxI, const BLContextCreateInfo* createInfo) noexcept;

  BL_INLINE void initFirstBatch() noexcept {
    BLRasterWorkBatch* batch = _allocator.newT<BLRasterWorkBatch>();
    BL_ASSERT(batch != nullptr); // We have preallocated enough, cannot happen.

    batch->_jobQueueList.reset(newJobQueue());
    batch->_fetchQueueList.reset(newFetchQueue());
    batch->_commandQueueList.reset(newCommandQueue());

    _currentBatch = batch;
    _jobQueueAppender.reset(*batch->_jobQueueList.first());
    _fetchQueueAppender.reset(*batch->_fetchQueueList.first());
    _commandQueueAppender.reset(*batch->_commandQueueList.first());

    _commandQueueCount = 0;
    _stateSlotCount = 0;
  }

  //! Releases all acquired threads and destroys all work contexts.
  //!
  //! \note It's only safe to call `reset()` after all threads have finalized
  //! their work. It would be disaster to call `reset()` when one or more thread
  //! is still running as reset destroys all work contexts, so the threads would
  //! be using freed memory.
  void reset() noexcept;

  //! \name Accessors
  //! \{

  BL_INLINE uint32_t workerCount() const noexcept { return _workerCount; }

  //! \}

  //! \name Job Data
  //! \{

  BL_INLINE BLRasterJobQueue* newJobQueue() noexcept {
    void* p = _allocator.allocT<BLRasterJobQueue>(BLRasterJobQueue::sizeOf());
    return p ? new (p) BLRasterJobQueue() : nullptr;
  }

  BL_INLINE BLResult ensureJobQueue() noexcept {
    if (!_jobQueueAppender.full())
      return BL_SUCCESS;
    else
      return _growJobQueue();
  }

  BL_INLINE BLResult _growJobQueue() noexcept {
    BL_ASSERT(_jobQueueAppender.full());

    BLRasterJobQueue* jobQueue = currentBatch()->_jobQueueList.last();
    _jobQueueAppender.done(*jobQueue);
    currentBatch()->_jobCount += uint32_t(jobQueue->size());

    jobQueue = newJobQueue();
    if (BL_UNLIKELY(!jobQueue))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    currentBatch()->_jobQueueList.append(jobQueue);
    _jobQueueAppender.reset(*jobQueue);

    return BL_SUCCESS;
  }

  BL_INLINE void addJob(BLRasterJobData* jobData) noexcept {
    BL_ASSERT(!_jobQueueAppender.full());
    _jobQueueAppender.append(jobData);
  }

  //! \}

  //! \name Fetch Data
  //! \{

  BL_INLINE BLRasterFetchQueue* newFetchQueue() noexcept {
    void* p = _allocator.allocT<BLRasterFetchQueue>(BLRasterFetchQueue::sizeOf());
    return p ? new (p) BLRasterFetchQueue() : nullptr;
  }

  BL_INLINE BLResult ensureFetchQueue() noexcept {
    if (!_fetchQueueAppender.full())
      return BL_SUCCESS;
    else
      return _growFetchQueue();
  }

  BL_NOINLINE BLResult _growFetchQueue() noexcept {
    BL_ASSERT(_fetchQueueAppender.full());

    BLRasterFetchQueue* fetchQueue = currentBatch()->_fetchQueueList.last();
    _fetchQueueAppender.done(*fetchQueue);

    fetchQueue = newFetchQueue();
    if (BL_UNLIKELY(!fetchQueue))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    currentBatch()->_fetchQueueList.append(fetchQueue);
    _fetchQueueAppender.reset(*fetchQueue);

    return BL_SUCCESS;
  }

  //! \}

  //! \name Command Data
  //! \{

  BL_INLINE bool hasPendingCommands() const noexcept {
    BLRasterCommandQueue* first = _currentBatch->_commandQueueList.first();
    BLRasterCommandQueue* last = _currentBatch->_commandQueueList.last();

    return first != last || _commandQueueAppender.index(*last) != 0;
  }

  BL_INLINE BLRasterCommandQueue* newCommandQueue() noexcept {
    void* p = _allocator.allocT<BLRasterCommandQueue>(BLRasterCommandQueue::sizeOf());
    return p ? new (p) BLRasterCommandQueue() : nullptr;
  }

  BL_INLINE BLResult ensureCommandQueue() noexcept {
    if (!_commandQueueAppender.full())
      return BL_SUCCESS;
    else
      return _growCommandQueue();
  }

  BL_NOINLINE BLResult _growCommandQueue() noexcept {
    BL_ASSERT(_commandQueueAppender.full());

    BLRasterCommandQueue* commandQueue = currentBatch()->_commandQueueList.last();
    _commandQueueAppender.done(*commandQueue);
    currentBatch()->_commandCount += uint32_t(commandQueue->size());

    commandQueue = newCommandQueue();
    if (BL_UNLIKELY(!commandQueue))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    currentBatch()->_commandQueueList.append(commandQueue);
    _commandQueueAppender.reset(*commandQueue);

    return BL_SUCCESS;
  }

  BL_INLINE BLRasterCommand* currentCommandData() noexcept {
    BL_ASSERT(!_commandQueueAppender.full());
    return _commandQueueAppender._ptr;
  }

  BL_INLINE uint32_t nextStateSlotIndex() noexcept {
    return _stateSlotCount++;
  }

  //! \}

  //! \name Work Batch
  //! \{

  BL_INLINE BLRasterWorkBatch* currentBatch() const noexcept { return _currentBatch; }

  BL_INLINE uint32_t currentBatchId() const noexcept { return _batchId; }

  BL_INLINE void finalizeBatch() noexcept {
    BLRasterJobQueue* lastJobQueue = _currentBatch->_jobQueueList.last();
    BLRasterFetchQueue* lastFetchQueue = _currentBatch->_fetchQueueList.last();
    BLRasterCommandQueue* lastCommandQueue = _currentBatch->_commandQueueList.last();

    _jobQueueAppender.done(*lastJobQueue);
    _fetchQueueAppender.done(*lastFetchQueue);
    _commandQueueAppender.done(*lastCommandQueue);

    _currentBatch->_jobCount += uint32_t(lastJobQueue->size());
    _currentBatch->_commandCount += uint32_t(lastCommandQueue->size());
    _currentBatch->_stateSlotCount = _stateSlotCount;
    _currentBatch->_bandCount = _bandCount;
    _currentBatch->_pastBlock = _allocator.pastBlock();

    if (++_batchId == 0)
      _batchId = 1;

    _commandQueueCount = 0;
    _stateSlotCount = 0;
  }

  //! \}
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERWORKERMANAGER_P_H_INCLUDED
