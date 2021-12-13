// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../raster/workersynchronization_p.h"
#include "../threading/futex_p.h"

namespace BLRasterEngine {

WorkerSynchronization::WorkerSynchronization() noexcept
  : _jobsRunningCount(0),
    _threadsRunningCount(0),
    _waitingForCompletion(false) {

  if (BL_FUTEX_ENABLED)
    blCallCtor(_futexData);
  else
    blCallCtor(_portableData);
}

WorkerSynchronization::~WorkerSynchronization() noexcept {
  if (BL_FUTEX_ENABLED)
    blCallDtor(_futexData);
  else
    blCallDtor(_portableData);
}

void WorkerSynchronization::waitForJobsToFinish() noexcept {
  if (BL_FUTEX_ENABLED) {
    if (blAtomicFetchSub(&_jobsRunningCount, 1) == 1) {
      blAtomicStore(&_futexData.jobsFinished, 1, std::memory_order_release);
      BLFutex::wakeAll(&_futexData.jobsFinished);
    }
    else {
      do {
        BLFutex::wait(&_futexData.jobsFinished, 0);
      } while (blAtomicFetch(&_futexData.jobsFinished, std::memory_order_acquire) != 1);
    }
  }
  else {
    BLLockGuard<BLMutex> guard(_portableData.mutex);
    if (--_jobsRunningCount == 0) {
      guard.release();
      _portableData.jobsCondition.broadcast();
    }
    else {
      while (_jobsRunningCount)
        _portableData.jobsCondition.wait(_portableData.mutex);
    }
  }
}

void WorkerSynchronization::threadDone() noexcept {
  if (blAtomicFetchSub(&_threadsRunningCount) != 1)
    return;

  if (BL_FUTEX_ENABLED) {
    blAtomicStore(&_futexData.bandsFinished, 1, std::memory_order_release);
    BLFutex::wakeOne(&_futexData.bandsFinished);
  }
  else {
    if (_portableData.mutex.protect([&]() { return _waitingForCompletion; }))
      _portableData.doneCondition.signal();
  }
}

void WorkerSynchronization::waitForThreadsToFinish() noexcept {
  if (BL_FUTEX_ENABLED) {
    for (;;) {
      uint32_t finished = blAtomicFetch(&_futexData.bandsFinished, std::memory_order_acquire);
      if (finished)
        break;
      BLFutex::wait(&_futexData.bandsFinished, 0);
    }

    blAtomicStore(&_futexData.bandsFinished, 0, std::memory_order_relaxed);
  }
  else {
    BLLockGuard<BLMutex> guard(_portableData.mutex);
    if (blAtomicFetch(&_threadsRunningCount) > 0) {
      _waitingForCompletion = true;
      while (blAtomicFetch(&_threadsRunningCount) > 0)
        _portableData.doneCondition.wait(_portableData.mutex);
      _waitingForCompletion = false;
    }
  }
}

} // {BLRasterEngine}
