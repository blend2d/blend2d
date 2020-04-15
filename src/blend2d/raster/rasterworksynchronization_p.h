// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_RASTERWORKSYNCHRONIZATION_P_H
#define BLEND2D_RASTER_RASTERWORKSYNCHRONIZATION_P_H

#include "../threading/atomic_p.h"
#include "../threading/conditionvariable_p.h"
#include "../threading/mutex_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [BLRasterWorkSynchronization]
// ============================================================================

class alignas(BL_CACHE_LINE_SIZE) BLRasterWorkSynchronization {
public:
  BLMutex mutex;
  BLConditionVariable jobsCondition;
  BLConditionVariable doneCondition;

  volatile uint32_t jobsRunningCount;
  volatile uint32_t threadsRunningCount;
  volatile uint32_t waitingForCompletion;

  BLRasterWorkSynchronization() noexcept;
  ~BLRasterWorkSynchronization() noexcept;

  void threadDone() noexcept;

  void waitForJobsToFinish() noexcept;
  void waitForThreadsToFinish() noexcept;
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERWORKSYNCHRONIZATION_P_H
