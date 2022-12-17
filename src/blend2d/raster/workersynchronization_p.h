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

  struct alignas(BL_CACHE_LINE_SIZE) Header {
    bool useFutex;
  };

  struct alignas(BL_CACHE_LINE_SIZE) Status {
    // These are used by both portable and futex implementation.
    uint32_t jobsRunningCount;
    uint32_t threadsRunningCount;
    uint32_t waitingForCompletion;

    uint8_t padding[64 - 12];

    // These are only used by futex implementation.
    uint32_t futexJobsFinished;
    uint32_t futexBandsFinished;
  };

  struct alignas(BL_CACHE_LINE_SIZE) PortableData {
    BL_INLINE PortableData() noexcept {}
    BL_INLINE ~PortableData() noexcept {}

    BLMutex mutex;
    BLConditionVariable jobsCondition;
    BLConditionVariable doneCondition;
  };

  Header _header;
  Status _status;
  PortableData _portableData;

  WorkerSynchronization() noexcept;
  ~WorkerSynchronization() noexcept;

  BL_INLINE bool useFutex() const noexcept { return _header.useFutex; }

  BL_INLINE void beforeStart(uint32_t threadCount, bool hasJobs) noexcept {
    blAtomicStoreRelaxed(&_status.jobsRunningCount, hasJobs ? uint32_t(threadCount + 1) : uint32_t(0));
    blAtomicStoreRelaxed(&_status.threadsRunningCount, threadCount);
    blAtomicStoreRelaxed(&_status.futexJobsFinished, 0u);
  }

  void waitForJobsToFinish() noexcept;
  void threadDone() noexcept;
  void waitForThreadsToFinish() noexcept;
};

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_WORKERSYNCHRONIZATION_P_H_INCLUDED
