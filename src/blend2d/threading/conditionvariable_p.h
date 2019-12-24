// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_THREADING_CONDITIONVARIABLE_P_H
#define BLEND2D_THREADING_CONDITIONVARIABLE_P_H

#include "../api-internal_p.h"
#include "../threading/mutex_p.h"

#ifndef _WIN32
  #include <sys/time.h>
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

#ifndef _WIN32
static void blGetAbsTimeForWaitCondition(struct timespec& out, uint64_t microseconds) noexcept {
  struct timeval now;
  gettimeofday(&now, nullptr);

  out.tv_sec = now.tv_sec + int64_t(microseconds / 1000000u);
  out.tv_nsec = (now.tv_usec + int64_t(microseconds % 1000000u)) * 1000;
  out.tv_sec += out.tv_nsec / 1000000000;
  out.tv_nsec %= 1000000000;
}
#endif

// ============================================================================
// [BLConditionalVariable]
// ============================================================================

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
    BL_UNUSED(ret);
  }

  BL_INLINE void broadcast() noexcept {
    int ret = pthread_cond_broadcast(&handle);
    BL_ASSERT(ret == 0);
    BL_UNUSED(ret);
  }

  BL_INLINE BLResult wait(BLMutex& mutex) noexcept {
    int ret = pthread_cond_wait(&handle, &mutex.handle);
    return ret == 0 ? BL_SUCCESS : blTraceError(BL_ERROR_INVALID_STATE);
  }

  BL_INLINE BLResult waitFor(BLMutex& mutex, uint64_t microseconds) noexcept {
    struct timespec absTime;
    blGetAbsTimeForWaitCondition(absTime, microseconds);
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
//! \endcond

#endif // BLEND2D_THREADING_CONDITIONVARIABLE_P_H
