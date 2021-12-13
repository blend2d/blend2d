// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_THREADING_CONDITIONVARIABLE_P_H_INCLUDED
#define BLEND2D_THREADING_CONDITIONVARIABLE_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../threading/mutex_p.h"
#include "../threading/threadingutils_p.h"

#ifndef _WIN32
  #include <sys/time.h>
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name Condition Variable
//! \{

class BLConditionVariable {
public:
  BL_NONCOPYABLE(BLConditionVariable)

#ifdef _WIN32
  CONDITION_VARIABLE handle;

  BL_INLINE BLConditionVariable() noexcept : handle(CONDITION_VARIABLE_INIT) {}
  BL_INLINE ~BLConditionVariable() noexcept {}

  BL_INLINE void signal() noexcept { WakeConditionVariable(&handle); }
  BL_INLINE void broadcast() noexcept { WakeAllConditionVariable(&handle); }

  BL_INLINE BLResult wait(BLMutex& mutex) noexcept {
    BOOL ret = SleepConditionVariableSRW(&handle, &mutex.handle, INFINITE, 0);
    return ret ? BL_SUCCESS : blTraceError(BL_ERROR_INVALID_STATE);
  }

  BL_INLINE BLResult waitFor(BLMutex& mutex, uint64_t microseconds) noexcept {
    uint32_t milliseconds = uint32_t(blMin<uint64_t>(microseconds / 1000u, INFINITE));
    BOOL ret = SleepConditionVariableSRW(&handle, &mutex.handle, milliseconds, 0);

    if (ret)
      return BL_SUCCESS;

    // We don't trace `BL_ERROR_TIMED_OUT` as it's not unexpected.
    return BL_ERROR_TIMED_OUT;
  }
#else
  pthread_cond_t handle;

  BL_INLINE BLConditionVariable() noexcept : handle(PTHREAD_COND_INITIALIZER) {}
  BL_INLINE ~BLConditionVariable() noexcept { pthread_cond_destroy(&handle); }

  BL_INLINE void signal() noexcept {
    int ret = pthread_cond_signal(&handle);
    BL_ASSERT(ret == 0);
    blUnused(ret);
  }

  BL_INLINE void broadcast() noexcept {
    int ret = pthread_cond_broadcast(&handle);
    BL_ASSERT(ret == 0);
    blUnused(ret);
  }

  BL_INLINE BLResult wait(BLMutex& mutex) noexcept {
    int ret = pthread_cond_wait(&handle, &mutex.handle);
    return ret == 0 ? BL_SUCCESS : blTraceError(BL_ERROR_INVALID_STATE);
  }

  BL_INLINE BLResult waitFor(BLMutex& mutex, uint64_t microseconds) noexcept {
    struct timespec absTime;
    BLThreadingUtils::getAbsTimeForWaitCondition(absTime, microseconds);
    return waitUntil(mutex, &absTime);
  }

  BL_INLINE BLResult waitUntil(BLMutex& mutex, const struct timespec* absTime) noexcept {
    int ret = pthread_cond_timedwait(&handle, &mutex.handle, absTime);
    if (ret == 0)
      return BL_SUCCESS;

    // We don't trace `BL_ERROR_TIMED_OUT` as it's not unexpected.
    return BL_ERROR_TIMED_OUT;
  }
#endif
};

//! \}

//! \}
//! \endcond

#endif // BLEND2D_THREADING_CONDITIONVARIABLE_P_H_INCLUDED
