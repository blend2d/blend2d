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
