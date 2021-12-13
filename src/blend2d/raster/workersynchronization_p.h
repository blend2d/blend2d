// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_WORKERSYNCHRONIZATION_P_H_INCLUDED
#define BLEND2D_RASTER_WORKERSYNCHRONIZATION_P_H_INCLUDED

#include "../threading/atomic_p.h"
#include "../threading/conditionvariable_p.h"
#include "../threading/mutex_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace BLRasterEngine {

class alignas(BL_CACHE_LINE_SIZE) WorkerSynchronization {
public:
  BL_NONCOPYABLE(WorkerSynchronization)

  volatile uint32_t _jobsRunningCount;
  volatile uint32_t _threadsRunningCount;
  volatile uint32_t _waitingForCompletion;

  struct FutexData {
    uint32_t jobsFinished;
    uint32_t bandsFinished;

    BL_INLINE FutexData() noexcept
      : jobsFinished(0),
        bandsFinished(0) {}
  };

  struct PortableData {
    BL_INLINE PortableData() noexcept {}
    BL_INLINE ~PortableData() noexcept {}

    BLMutex mutex;
    BLConditionVariable jobsCondition;
    BLConditionVariable doneCondition;
  };

  union {
    FutexData _futexData;
    PortableData _portableData;
  };

  WorkerSynchronization() noexcept;
  ~WorkerSynchronization() noexcept;

  BL_INLINE void beforeStart(uint32_t threadCount) noexcept {
    blAtomicStore(&_jobsRunningCount, threadCount + 1);
    blAtomicStore(&_threadsRunningCount, threadCount);
  }

  void waitForJobsToFinish() noexcept;
  void threadDone() noexcept;
  void waitForThreadsToFinish() noexcept;
};

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_WORKERSYNCHRONIZATION_P_H_INCLUDED
