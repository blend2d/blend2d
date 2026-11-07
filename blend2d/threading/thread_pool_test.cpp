// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/threading/atomic_p.h>
#include <blend2d/threading/condition_variable_p.h>
#include <blend2d/threading/mutex_p.h>
#include <blend2d/threading/thread_pool_p.h>

// ThreadPool - Tests
// ==================

namespace bl::ThreadingTests {

struct ThreadTestData {
  uint32_t iter;
  uint32_t counter;
  uint32_t waiting;
  Mutex mutex;
  ConditionVariable condition;

  ThreadTestData() noexcept
    : iter(0),
      counter(0),
      waiting(false) {}
};

static void BL_CDECL test_thread_entry(Thread* thread, void* data_) noexcept {
  ThreadTestData* data = static_cast<ThreadTestData*>(data_);
  uint32_t iter = Atomic::fetch_strong(data->iter);

  TEST_LOG("[#%u] Thread %p running", iter, thread);

  if (Atomic::fetch_sub_acq_rel(data->counter) == 1) {
    LockGuard<Mutex> guard(data->mutex);
    if (data->waiting) {
      TEST_LOG("[#%u] Thread %p signaling to main thread", iter, thread);
      data->condition.signal();
    }
  }
}

TEST_CASE(threading_thread_pool) {
  ThreadPool* tp = bl_thread_pool_global();
  ThreadTestData data;

  constexpr uint32_t kThreadCount = 4;
  Thread* threads[kThreadCount];

  // Allocating more threads than an internal limit for must always fail.
  TEST_LOG("Trying to allocate very high number of threads that should fail");
  {
    BLResult reason;
    uint32_t n = tp->acquire_threads(nullptr, 1000000, BL_THREAD_POOL_ACQUIRE_FLAG_ALL_OR_NOTHING, &reason);

    EXPECT_EQ(n, 0u);
    EXPECT_EQ(reason, BL_ERROR_THREAD_POOL_EXHAUSTED);
  }

  TEST_LOG("Repeatedly acquiring / releasing %u threads with a simple task", kThreadCount);
  for (uint32_t i = 0; i < 10; i++) {
    Atomic::fetch_add_acq_rel(data.iter);

    TEST_LOG("[#%u] Acquiring %u threads from thread-pool", i, kThreadCount);
    BLResult reason;
    uint32_t n = tp->acquire_threads(threads, kThreadCount, 0, &reason);

    EXPECT_OK(reason);
    EXPECT_EQ(n, kThreadCount);

    Atomic::store_relaxed(data.counter, n);
    TEST_LOG("[#%u] Running %u threads", i, n);
    for (Thread* thread : threads) {
      BLResult result = thread->run(test_thread_entry, &data);
      EXPECT_OK(result);
    }

    TEST_LOG("[#%u] Waiting and releasing", i);
    {
      LockGuard<Mutex> guard(data.mutex);
      data.waiting = true;
      while (Atomic::fetch_strong(data.counter) != 0)
        data.condition.wait(data.mutex);
    }

    tp->release_threads(threads, n);
  }


  TEST_LOG("Cleaning up");
  tp->cleanup();

  TEST_LOG("Done");
}

} // {bl::ThreadingTests}

#endif // BL_TEST
