// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../api-build_p.h"
#include "../raster/rastercontext_p.h"
#include "../raster/rasterworkercontext_p.h"
#include "../raster/rasterworkermanager_p.h"

// ============================================================================
// [BLRasterWorkerManager - Init / Reset]
// ============================================================================

BLResult BLRasterWorkerManager::init(BLRasterContextImpl* ctxI, uint32_t initFlags, uint32_t threadCount) noexcept {
  // Do not use thread-pool for synchronous rendering.
  if (!threadCount)
    return BL_SUCCESS;

  BLZoneAllocator& zone = ctxI->baseZone;
  BLZoneAllocator::State zoneState;

  zone.saveState(&zoneState);

  // We must enforce some hard limit here...
  if (threadCount > BL_RUNTIME_MAX_THREAD_COUNT)
    threadCount = BL_RUNTIME_MAX_THREAD_COUNT;

  // Allocate space for threads.
  BLThread** workerThreads = zone.allocT<BLThread*>(threadCount * sizeof(void*));
  BLRasterWorkerContext** workerContexts = zone.allocT<BLRasterWorkerContext*>(threadCount * sizeof(void*));

  if (!workerThreads || !workerContexts) {
    zone.restoreState(&zoneState);
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  // Get global thread-pool or create an isolated one.
  BLThreadPool* threadPool = nullptr;
  if (initFlags & BL_CONTEXT_CREATE_FLAG_ISOLATED_THREADS) {
    threadPool = blThreadPoolCreate();
    if (!threadPool)
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }
  else {
    threadPool = blThreadPoolGlobal();
  }

  // Acquire threads from thread-pool.
  uint32_t acquireFlags = 0;
  if (initFlags & BL_CONTEXT_CREATE_FLAG_FORCE_THREADS)
    acquireFlags = BL_THREAD_POOL_ACQUIRE_FLAG_FORCE_ALL;
  else if (!(initFlags & BL_CONTEXT_CREATE_FLAG_FALLBACK_TO_SYNC))
    acquireFlags = BL_THREAD_POOL_ACQUIRE_FLAG_FORCE_ONE;

  BLResult result = BL_SUCCESS;
  uint32_t n = threadPool->acquireThreads(workerThreads, threadCount, acquireFlags);

  if (!n) {
    if (!(initFlags & BL_CONTEXT_CREATE_FLAG_FALLBACK_TO_SYNC))
      result = blTraceError(BL_ERROR_TOO_MANY_THREADS);
  }
  else {
    for (uint32_t i = 0; i < n; i++) {
      // NOTE: We really want worker contexts aligned to cache line as each will
      // be used from a different thread. This means that they should not interfere
      // with each other as that could slow down things a significantly.
      BLRasterWorkerContext* workerContext = zone.allocT<BLRasterWorkerContext>(blAlignUp(sizeof(BLRasterWorkerContext), BL_CACHE_LINE_SIZE), BL_CACHE_LINE_SIZE);
      workerContexts[i] = workerContext;

      if (!workerContext) {
        result = blTraceError(BL_ERROR_OUT_OF_MEMORY);
        break;
      }
    }
  }

  if (result != BL_SUCCESS || !n) {
    threadPool->release();
    zone.restoreState(&zoneState);
    return result;
  }
  else {
    // Now it's time to safely initialize worker contexts.
    for (uint32_t i = 0; i < n; i++)
      new(workerContexts[i]) BLRasterWorkerContext(ctxI);

    _threadPool = threadPool;
    _workerThreads = workerThreads;
    _workerContexts = workerContexts;
    _threadCount = n;
    return BL_SUCCESS;
  }
}

void BLRasterWorkerManager::reset() noexcept {
  if (!initialized())
    return;

  BLRasterWorkerContext** workerContexts = _workerContexts;
  for (uint32_t i = 0; i < _threadCount; i++)
    workerContexts[i]->~BLRasterWorkerContext();

  _threadPool->releaseThreads(_workerThreads, _threadCount);
  _threadPool->release();

  _threadPool = nullptr;
  _workerThreads = nullptr;
  _workerContexts = nullptr;
  _threadCount = 0;
}
