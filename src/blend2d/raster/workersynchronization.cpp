// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../raster/workersynchronization_p.h"
#include "../threading/futex_p.h"

namespace BLRasterEngine {

WorkerSynchronization::WorkerSynchronization() noexcept
  : _header{},
    _status{} {

  _header.useFutex = BL_FUTEX_ENABLED;
  if (!useFutex())
    blCallCtor(_portableData);
}

WorkerSynchronization::~WorkerSynchronization() noexcept {
  if (!useFutex())
    blCallDtor(_portableData);
}

void WorkerSynchronization::waitForJobsToFinish() noexcept {
  if (useFutex()) {
    if (blAtomicFetchSubStrong(&_status.jobsRunningCount) == 1) {
      blAtomicFetchAddStrong(&_status.futexJobsFinished);
      BLFutex::wakeAll(&_status.futexJobsFinished);
    }
    else {
      do {
        BLFutex::wait(&_status.futexJobsFinished, 0u);
      } while (blAtomicFetchRelaxed(&_status.futexJobsFinished) != 1);
    }
  }
  else {
    BLLockGuard<BLMutex> guard(_portableData.mutex);
    if (--_status.jobsRunningCount == 0) {
      guard.release();
      _portableData.jobsCondition.broadcast();
    }
    else {
      while (_status.jobsRunningCount)
        _portableData.jobsCondition.wait(_portableData.mutex);
    }
  }
}

void WorkerSynchronization::threadDone() noexcept {
  uint32_t remainingPlusOne = blAtomicFetchSubStrong(&_status.threadsRunningCount);

  if (remainingPlusOne != 1)
    return;

  if (useFutex()) {
    blAtomicFetchAddStrong(&_status.futexBandsFinished);
    BLFutex::wakeOne(&_status.futexBandsFinished);
  }
  else {
    if (_portableData.mutex.protect([&]() { return _status.waitingForCompletion; }))
      _portableData.doneCondition.signal();
  }
}

void WorkerSynchronization::waitForThreadsToFinish() noexcept {
  if (useFutex()) {
    for (;;) {
      uint32_t finished = blAtomicFetchStrong(&_status.futexBandsFinished);
      if (finished)
        break;
      BLFutex::wait(&_status.futexBandsFinished, 0);
    }
    blAtomicStoreRelaxed(&_status.futexBandsFinished, 0u);
  }
  else {
    BLLockGuard<BLMutex> guard(_portableData.mutex);
    if (blAtomicFetchRelaxed(&_status.threadsRunningCount) > 0) {
      _status.waitingForCompletion = true;
      while (blAtomicFetchRelaxed(&_status.threadsRunningCount) > 0)
        _portableData.doneCondition.wait(_portableData.mutex);
      _status.waitingForCompletion = false;
    }
  }
}

} // {BLRasterEngine}
