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

#ifndef BLEND2D_THREADING_CONDITIONVARIABLE_P_H_INCLUDED
#define BLEND2D_THREADING_CONDITIONVARIABLE_P_H_INCLUDED

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

#endif // BLEND2D_THREADING_CONDITIONVARIABLE_P_H_INCLUDED
