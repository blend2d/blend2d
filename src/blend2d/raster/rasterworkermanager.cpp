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

#include "../api-build_p.h"
#include "../raster/rastercontext_p.h"
#include "../raster/rasterworkdata_p.h"
#include "../raster/rasterworkermanager_p.h"

// ============================================================================
// [BLRasterWorkerManager - Init / Reset]
// ============================================================================

BLResult BLRasterWorkerManager::init(BLRasterContextImpl* ctxI, const BLContextCreateInfo* createInfo) noexcept {
  uint32_t initFlags = createInfo->flags;
  uint32_t threadCount = createInfo->threadCount;
  uint32_t commandQueueLimit = createInfo->commandQueueLimit;

  BL_ASSERT(!isActive());
  BL_ASSERT(threadCount > 0);

  BLZoneAllocator& zone = ctxI->baseZone;
  BLZoneAllocator::StatePtr zoneState = zone.saveState();

  // We must enforce some hard limit here...
  if (threadCount > BL_RUNTIME_MAX_THREAD_COUNT)
    threadCount = BL_RUNTIME_MAX_THREAD_COUNT;

  // We count the user thread as a worker thread as well. In this case this one
  // doens't need a separate workData as it can use the 'syncWorkData' owned by
  // the rendering context.
  uint32_t workerCount = threadCount - 1;

  // Fallback to synchronous rendering immediately if this combination was selected.
  if (!workerCount && (initFlags & BL_CONTEXT_CREATE_FLAG_FALLBACK_TO_SYNC))
    return BL_SUCCESS;

  // Forces the zone-allocator to preallocate the first block of memory, if
  // not allocated yet.
  size_t batchContextSize = sizeof(BLRasterWorkBatch) +
                            BLRasterJobQueue::sizeOf() +
                            BLRasterFetchQueue::sizeOf() +
                            BLRasterCommandQueue::sizeOf();
  BL_PROPAGATE(_allocator.ensure(batchContextSize));

  // Allocate space for worker threads data.
  if (workerCount) {
    BLThread** workerThreads = zone.allocT<BLThread*>(blAlignUp(workerCount * sizeof(void*), 8));
    BLRasterWorkData** workDataStorage = zone.allocT<BLRasterWorkData*>(blAlignUp(workerCount * sizeof(void*), 8));

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
      threadPool = blThreadPoolGlobal();
    }

    // Acquire threads passed to thread-pool.
    BLResult reason = BL_SUCCESS;
    uint32_t acquireThreadFlags = 0;
    uint32_t n = threadPool->acquireThreads(workerThreads, workerCount, acquireThreadFlags, &reason);

    if (reason != BL_SUCCESS)
      ctxI->syncWorkData.accumulateError(reason);

    for (uint32_t i = 0; i < n; i++) {
      // NOTE: We really want work data to be aligned to the cache line as each
      // instance will be used from a different thread. This means that they should
      // not interfere with each other as that could slow down things significantly.
      BLRasterWorkData* workData = zone.allocT<BLRasterWorkData>(blAlignUp(sizeof(BLRasterWorkData), BL_CACHE_LINE_SIZE), BL_CACHE_LINE_SIZE);
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

      // Fallback to synchronous rendering - nothing else to clean up as we haven't
      // initialized anything.
      if (initFlags & BL_CONTEXT_CREATE_FLAG_FALLBACK_TO_SYNC)
        return BL_SUCCESS;
    }
    else {
      // Initialize worker contexts.
      for (uint32_t i = 0; i < n; i++) {
        workDataStorage[i] = new(workDataStorage[i]) BLRasterWorkData(ctxI, i);
        workDataStorage[i]->initBandData(ctxI->bandHeight(), ctxI->bandCount());
      }
    }

    _threadPool = threadPool;
    _workerThreads = workerThreads;
    _workDataStorage = workDataStorage;
    _workerCount = n;
  }
  else {
    // In this case we use the worker manager, but we don't really manage any threads...
    _workerCount = 0;
  }

  _isActive = true;
  _bandCount = ctxI->bandCount();
  _commandQueueLimit = commandQueueLimit;

  initFirstBatch();
  return BL_SUCCESS;
}

void BLRasterWorkerManager::reset() noexcept {
  if (!isActive())
    return;

  _isActive = false;

  if (_workerCount) {
    for (uint32_t i = 0; i < _workerCount; i++)
      _workDataStorage[i]->~BLRasterWorkData();

    _threadPool->releaseThreads(_workerThreads, _workerCount);
    _workerCount = 0;
    _workerThreads = nullptr;
    _workDataStorage = nullptr;
  }

  if (_threadPool) {
    _threadPool->release();
    _threadPool = nullptr;
  }

  _commandQueueCount = 0;
  _commandQueueLimit = 0;
  _stateSlotCount = 0;
}
