// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../raster/rastercontext_p.h"
#include "../raster/workdata_p.h"
#include "../raster/workermanager_p.h"
#include "../support/intops_p.h"

namespace bl {
namespace RasterEngine {

// bl::RasterEngine::WorkerManager - Init
// ======================================

BLResult WorkerManager::init(BLRasterContextImpl* ctxI, const BLContextCreateInfo* createInfo) noexcept {
  uint32_t initFlags = createInfo->flags;
  uint32_t threadCount = createInfo->threadCount;
  uint32_t commandQueueLimit = IntOps::alignUp(createInfo->commandQueueLimit, kRenderQueueCapacity);

  BL_ASSERT(!isActive());
  BL_ASSERT(threadCount > 0);

  ArenaAllocator& zone = ctxI->baseZone;
  ArenaAllocator::StatePtr zoneState = zone.saveState();

  // We must enforce some hard limit here...
  if (threadCount > BL_RUNTIME_MAX_THREAD_COUNT)
    threadCount = BL_RUNTIME_MAX_THREAD_COUNT;

  // If the command queue limit is not specified, use the default.
  if (commandQueueLimit == 0)
    commandQueueLimit = BL_RASTER_CONTEXT_DEFAULT_COMMAND_QUEUE_LIMIT;

  // We count the user thread as a worker thread as well. In this case this one doesn't need a separate workData
  // as it can use the 'syncWorkData' owned by the rendering context.
  uint32_t workerCount = threadCount - 1;

  // Fallback to synchronous rendering immediately if this combination was selected.
  if (!workerCount && (initFlags & BL_CONTEXT_CREATE_FLAG_FALLBACK_TO_SYNC))
    return BL_SUCCESS;

  // Forces the zone-allocator to preallocate the first block of memory, if not allocated yet.
  size_t batchContextSize = sizeof(RenderBatch) +
                            RenderJobQueue::sizeOf() +
                            RenderCommandQueue::sizeOf();
  BL_PROPAGATE(_allocator.ensure(batchContextSize));

  // Allocate space for worker threads data.
  if (workerCount) {
    BLThread** workerThreads = zone.allocT<BLThread*>(IntOps::alignUp(workerCount * sizeof(void*), 8));
    WorkData** workDataStorage = zone.allocT<WorkData*>(IntOps::alignUp(workerCount * sizeof(void*), 8));

    if (!workerThreads || !workDataStorage) {
      zone.restoreState(zoneState);
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
    }

    // Get global thread-pool or create an isolated one.
    BLThreadPool* threadPool = nullptr;
    if (initFlags & BL_CONTEXT_CREATE_FLAG_ISOLATED_THREAD_POOL) {
      threadPool = blThreadPoolCreate();
      if (!threadPool)
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);
    }
    else {
      threadPool = blThreadPoolGlobal()->addRef();
    }

    // Acquire threads passed to thread-pool.
    BLResult reason = BL_SUCCESS;
    uint32_t acquireThreadFlags = 0;
    uint32_t n = threadPool->acquireThreads(workerThreads, workerCount, acquireThreadFlags, &reason);

    if (reason != BL_SUCCESS)
      ctxI->syncWorkData.accumulateError(reason);

    for (uint32_t i = 0; i < n; i++) {
      // NOTE: We really want work data to be aligned to the cache line as each instance will be used from a different
      // thread. This means that they should not interfere with each other as that could slow down things significantly.
      WorkData* workData = zone.allocT<WorkData>(IntOps::alignUp(sizeof(WorkData), BL_CACHE_LINE_SIZE), BL_CACHE_LINE_SIZE);
      workDataStorage[i] = workData;

      if (!workData) {
        ctxI->syncWorkData.accumulateError(blTraceError(BL_ERROR_OUT_OF_MEMORY));
        threadPool->releaseThreads(workerThreads, n);
        n = 0;
        break;
      }
    }

    if (!n) {
      threadPool->release();
      threadPool = nullptr;
      workerThreads = nullptr;
      workDataStorage = nullptr;
      zone.restoreState(zoneState);

      // Fallback to synchronous rendering - nothing else to clean up as we haven't initialized anything.
      if (initFlags & BL_CONTEXT_CREATE_FLAG_FALLBACK_TO_SYNC)
        return BL_SUCCESS;
    }
    else {
      // Initialize worker contexts.
      WorkerSynchronization* synchronization = &_synchronization;
      for (uint32_t i = 0; i < n; i++) {
        blCallCtor(*workDataStorage[i], ctxI, synchronization, i + 1);
        workDataStorage[i]->initBandData(ctxI->bandHeight(), ctxI->bandCount(), ctxI->commandQuantizationShiftAA());
      }
    }

    _threadPool = threadPool;
    _workerThreads = workerThreads;
    _workDataStorage = workDataStorage;
    _threadCount = n;
  }
  else {
    // In this case we use the worker manager, but we don't really manage any threads...
    _threadCount = 0;
  }

  _isActive = true;
  _bandCount = ctxI->bandCount();
  _commandQueueLimit = commandQueueLimit;

  initFirstBatch();
  return BL_SUCCESS;
}

BLResult WorkerManager::initWorkMemory(size_t zeroedMemorySize) noexcept {
  uint32_t n = threadCount();
  for (uint32_t i = 0; i < n; i++) {
    BL_PROPAGATE(_workDataStorage[i]->zeroBuffer.ensure(zeroedMemorySize));
  }
  return BL_SUCCESS;
}

// bl::RasterEngine::WorkerManager - Reset
// =======================================

void WorkerManager::reset() noexcept {
  if (!isActive())
    return;

  _isActive = false;

  if (_threadPool) {
    for (uint32_t i = 0; i < _threadCount; i++)
      blCallDtor(*_workDataStorage[i]);

    _threadPool->releaseThreads(_workerThreads, _threadCount);
    _threadPool->release();

    _threadPool = nullptr;
    _workerThreads = nullptr;
    _workDataStorage = nullptr;
    _threadCount = 0;
  }

  _commandQueueCount = 0;
  _commandQueueLimit = 0;
  _stateSlotCount = 0;
}

} // {RasterEngine}
} // {bl}
