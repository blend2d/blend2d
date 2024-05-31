// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_WORKERSYNCHRONIZATION_P_H_INCLUDED
#define BLEND2D_RASTER_WORKERSYNCHRONIZATION_P_H_INCLUDED

#include "../threading/atomic_p.h"
#include "../threading/conditionvariable_p.h"
#include "../threading/mutex_p.h"
#include "../threading/tsanutils_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace bl {
namespace RasterEngine {

class alignas(BL_CACHE_LINE_SIZE) WorkerSynchronization {
public:
  BL_NONCOPYABLE(WorkerSynchronization)

  struct alignas(BL_CACHE_LINE_SIZE) Header {
    bool useFutex;
    Threading::TSanBarrier barrier;
  };

  struct alignas(BL_CACHE_LINE_SIZE) Status {
    // These are used by both portable and futex implementation.
    uint32_t jobsRunningCount;
    uint32_t threadsRunningCount;
    uint32_t waitingForCompletion;

    uint8_t padding[64 - 12];

    // These are only really used by futex implementation, however, the variables are always stored to.
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

  BL_INLINE_NODEBUG bool useFutex() const noexcept { return _header.useFutex; }

  BL_INLINE void beforeStart(uint32_t threadCount, bool hasJobs) noexcept {
    blAtomicStoreRelaxed(&_status.jobsRunningCount, hasJobs ? uint32_t(threadCount + 1) : uint32_t(0));
    blAtomicStoreRelaxed(&_status.threadsRunningCount, threadCount);
    blAtomicStoreStrong(&_status.futexJobsFinished, 0u);

    _header.barrier.release();
  }

  BL_INLINE void threadStarted() noexcept {
    _header.barrier.acquire();
  }

  // Called when there are no jobs at all to acknowledge that `waitForJobsToFinish()` would never be called.
  BL_INLINE void noJobsToWaitFor() noexcept {
    blUnused(
      blAtomicFetchStrong(&_status.futexJobsFinished)
    );
  }

  void waitForJobsToFinish() noexcept;
  void threadDone() noexcept;
  void waitForThreadsToFinish() noexcept;
};

} // {RasterEngine}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_WORKERSYNCHRONIZATION_P_H_INCLUDED
