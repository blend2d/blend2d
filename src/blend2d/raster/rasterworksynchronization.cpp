// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../api-build_p.h"
#include "../raster/rasterworksynchronization_p.h"

// ============================================================================
// [BLRasterWorkSynchronization]
// ============================================================================

BLRasterWorkSynchronization::BLRasterWorkSynchronization() noexcept
  : jobsRunningCount(0),
    threadsRunningCount(0),
    waitingForCompletion(false) {}

BLRasterWorkSynchronization::~BLRasterWorkSynchronization() noexcept {}

void BLRasterWorkSynchronization::threadDone() noexcept {
  if (blAtomicFetchSub(&threadsRunningCount) == 1) {
    if (mutex.protect([&]() { return waitingForCompletion; }))
      doneCondition.signal();
  }
}

void BLRasterWorkSynchronization::waitForJobsToFinish() noexcept {
  mutex.lock();
  if (--jobsRunningCount == 0) {
    mutex.unlock();
    jobsCondition.broadcast();
  }
  else {
    while (jobsRunningCount)
      jobsCondition.wait(mutex);
    mutex.unlock();
  }
}

void BLRasterWorkSynchronization::waitForThreadsToFinish() noexcept {
  BLLockGuard<BLMutex> guard(mutex);
  if (blAtomicFetch(&threadsRunningCount) > 0) {
    waitingForCompletion = true;
    while (blAtomicFetch(&threadsRunningCount) > 0)
      doneCondition.wait(mutex);
    waitingForCompletion = false;
  }
}
