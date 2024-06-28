// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../runtime_p.h"
#include "../support/bitops_p.h"
#include "../support/fixedbitarray_p.h"
#include "../support/intops_p.h"
#include "../support/wrap_p.h"
#include "../threading/atomic_p.h"
#include "../threading/conditionvariable_p.h"
#include "../threading/mutex_p.h"
#include "../threading/threadpool_p.h"

#ifdef _WIN32
  #include <process.h>
#endif

// ThreadPool - Globals
// ====================

static BLThreadPoolVirt blThreadPoolVirt;

// ThreadPool - Internal
// =====================

class BLInternalThreadPool : public BLThreadPool {
public:
  enum : uint32_t { kMaxThreadCount = 64 };
  typedef bl::FixedBitArray<BLBitWord, kMaxThreadCount> PooledThreadBitArray;

  //! \name Members
  //! \{

  //! Counts the number of references to the thread pool from outside (not counting threads).
  size_t refCount {};
  //! Counts one reference from outside and each thread the thread pool manages.
  size_t internalRefCount {};

  uint32_t stackSize {};
  uint32_t maxThreadCount {};
  uint32_t createdThreadCount {};
  uint32_t pooledThreadCount {};
  uint32_t acquiredThreadCount {};
  uint32_t destroyWaitTimeInMS {};
  uint32_t waitingOnDestroy {};

  BLMutex mutex;
  BLConditionVariable destroyCondition;
  BLThreadAttributes threadAttributes {};

  PooledThreadBitArray pooledThreadBits {};
  BLThread* threads[kMaxThreadCount] {};

  //! \}

  //! \name Construction & Destruction
  //! \{

  BL_NOINLINE explicit BLInternalThreadPool() noexcept
    : BLThreadPool { &blThreadPoolVirt },
      refCount(1),
      internalRefCount(1),
      maxThreadCount(kMaxThreadCount),
      destroyWaitTimeInMS(100),
      mutex(),
      destroyCondition() {}

  BL_NOINLINE ~BLInternalThreadPool() noexcept {}

  //! \}

  BL_NOINLINE void performExitCleanup() noexcept {
    uint32_t numTries = 5;
    uint64_t waitTime = (uint64_t(destroyWaitTimeInMS) * 1000u) / numTries;

    do {
      cleanup(BL_THREAD_QUIT_ON_EXIT);

      BLLockGuard<BLMutex> guard(mutex);
      if (blAtomicFetchStrong(&createdThreadCount) != 0) {
        waitingOnDestroy = 1;
        if (destroyCondition.waitFor(mutex, waitTime) == BL_SUCCESS)
          break;
      }
    } while (--numTries);
  }
};

static bl::Wrap<BLInternalThreadPool> blGlobalThreadPool;

// ThreadPool - Create & Destroy
// =============================

BLThreadPool* blThreadPoolCreate() noexcept {
  void* p = malloc(sizeof(BLInternalThreadPool));
  if (!p)
    return nullptr;
  return new(BLInternal::PlacementNew{p}) BLInternalThreadPool();
}

static void blThreadPoolReleaseInternal(BLInternalThreadPool* self) noexcept {
  if (blAtomicFetchSubStrong(&self->internalRefCount) == 1) {
    self->~BLInternalThreadPool();
    if (self != &blGlobalThreadPool)
      free(self);
  }
}

// ThreadPool - AddRef & Release
// =============================

static BLThreadPool* BL_CDECL blThreadPoolAddRef(BLThreadPool* self_) noexcept {
  BLInternalThreadPool* self = static_cast<BLInternalThreadPool*>(self_);
  blAtomicFetchAddRelaxed(&self->refCount);
  return self;
}

static BLResult BL_CDECL blThreadPoolRelease(BLThreadPool* self_) noexcept {
  BLInternalThreadPool* self = static_cast<BLInternalThreadPool*>(self_);

  // Dereference the number of outside references. If that hits zero it means to destroy the thread pool.
  // However, we have to first shut down all the threads, and then we can actually destroy the pool itself.
  if (blAtomicFetchSubStrong(&self->refCount) == 1) {
    // First try to destroy all threads - this could possibly fail.
    if (blAtomicFetchStrong(&self->createdThreadCount) != 0)
      self->performExitCleanup();

    blThreadPoolReleaseInternal(self);
  }

  return BL_SUCCESS;
}

// ThreadPool - Accessors
// ======================

static uint32_t BL_CDECL blThreadPoolMaxThreadCount(const BLThreadPool* self_) noexcept {
  const BLInternalThreadPool* self = static_cast<const BLInternalThreadPool*>(self_);
  return self->maxThreadCount;
}

static uint32_t BL_CDECL blThreadPoolPooledThreadCount(const BLThreadPool* self_) noexcept {
  const BLInternalThreadPool* self = static_cast<const BLInternalThreadPool*>(self_);
  return self->pooledThreadCount;
}

static BLResult BL_CDECL blThreadPoolSetThreadAttributes(BLThreadPool* self_, const BLThreadAttributes* attributes) noexcept {
  BLInternalThreadPool* self = static_cast<BLInternalThreadPool*>(self_);
  BLLockGuard<BLMutex> guard(self->mutex);

  // Verify that the provided `stackSize` is okay.
  //   - POSIX   - Minimum stack size is `PTHREAD_STACK_MIN`, some implementations enforce alignment to a page-size.
  //   - WINDOWS - Minimum stack size is `SYSTEM_INFO::dwAllocationGranularity`, alignment should follow the
  //               granularity as well, however, WinAPI would align stack size if it's not properly aligned.
  uint32_t stackSize = attributes->stackSize;
  if (stackSize) {
    const BLRuntimeSystemInfo& si = blRuntimeContext.systemInfo;
    if (stackSize < si.threadStackSize || !bl::IntOps::isAligned(stackSize, si.allocationGranularity))
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  self->threadAttributes = *attributes;
  return BL_SUCCESS;
}

// ThreadPool - Cleanup
// ====================

static void blThreadPoolThreadExitFunc(BLThread* thread, void* data) noexcept {
  BLInternalThreadPool* threadPool = static_cast<BLInternalThreadPool*>(data);
  thread->destroy();

  if (blAtomicFetchSubStrong(&threadPool->createdThreadCount) == 1) {
    threadPool->mutex.protect([&]() {
      if (threadPool->waitingOnDestroy)
        threadPool->destroyCondition.signal();
    });
  }

  blThreadPoolReleaseInternal(threadPool);
}

static uint32_t BL_CDECL blThreadPoolCleanup(BLThreadPool* self_, uint32_t threadQuitFlags) noexcept {
  BLInternalThreadPool* self = static_cast<BLInternalThreadPool*>(self_);
  BLLockGuard<BLMutex> guard(self->mutex);

  uint32_t n = 0;
  uint32_t bwIndex = 0;
  uint32_t pooledThreadCount = self->pooledThreadCount;

  if (!pooledThreadCount)
    return 0;

  do {
    BLBitWord mask = self->pooledThreadBits.data[bwIndex];
    bl::BitWordIterator<BLBitWord> it(mask);

    while (it.hasNext()) {
      uint32_t threadIndex = bwIndex * bl::IntOps::bitSizeOf<BLBitWord>() + it.next();
      BLThread* thread = self->threads[threadIndex];

      self->threads[threadIndex] = nullptr;
      thread->quit(threadQuitFlags);

      n++;
    }
    self->pooledThreadBits.data[bwIndex] = 0;
  } while (++bwIndex < BLInternalThreadPool::PooledThreadBitArray::kFixedArraySize);

  self->pooledThreadCount = pooledThreadCount - n;
  return n;
}

// ThreadPool - Acquire & Release
// ==============================

static void blThreadPoolReleaseThreadsInternal(BLInternalThreadPool* self, BLThread** threads, uint32_t n) noexcept {
  uint32_t i = 0;
  uint32_t bwIndex = 0;

  do {
    BLBitWord mask = self->pooledThreadBits.data[bwIndex] ^ bl::IntOps::allOnes<BLBitWord>();
    bl::BitWordIterator<BLBitWord> it(mask);

    while (it.hasNext()) {
      uint32_t bit = it.next();
      mask ^= bl::IntOps::lsbBitAt<BLBitWord>(bit);

      uint32_t threadIndex = bwIndex * bl::IntOps::bitSizeOf<BLBitWord>() + bit;
      BL_ASSERT(self->threads[threadIndex] == nullptr);

      BLThread* thread = threads[i];
      self->threads[threadIndex] = thread;

      if (++i >= n)
        break;
    }

    self->pooledThreadBits.data[bwIndex] = mask ^ bl::IntOps::allOnes<BLBitWord>();
  } while (i < n && ++bwIndex < BLInternalThreadPool::PooledThreadBitArray::kFixedArraySize);

  // This shouldn't happen. What is acquired must be released. If more threads are released than acquired it means
  // the API was used wrongly. Not sure we want to recover.
  BL_ASSERT(i == n);

  self->pooledThreadCount += n;
  self->acquiredThreadCount -= n;
}

static uint32_t blThreadPoolAcquireThreadsInternal(BLInternalThreadPool* self, BLThread** threads, uint32_t n, uint32_t flags, BLResult* reasonOut) noexcept {
  BLResult reason = BL_SUCCESS;
  uint32_t nAcquired = 0;

  uint32_t pooledThreadCount = self->pooledThreadCount;
  uint32_t acquiredThreadCount = self->acquiredThreadCount;

  if (n > pooledThreadCount) {
    uint32_t createThreadCount = n - pooledThreadCount;
    uint32_t remainingThreadCount = self->maxThreadCount - (acquiredThreadCount + pooledThreadCount);

    if (createThreadCount > remainingThreadCount) {
      if (flags & BL_THREAD_POOL_ACQUIRE_FLAG_ALL_OR_NOTHING) {
        *reasonOut = BL_ERROR_THREAD_POOL_EXHAUSTED;
        return 0;
      }
      createThreadCount = remainingThreadCount;
    }

    while (nAcquired < createThreadCount) {
      // We must increase the reference here as it must be accounted if it's going to start.
      blAtomicFetchAddRelaxed(&self->internalRefCount);

      reason = blThreadCreate(&threads[nAcquired], &self->threadAttributes, blThreadPoolThreadExitFunc, self);

      if (reason != BL_SUCCESS) {
        size_t prev = blAtomicFetchSubStrong(&self->internalRefCount);
        if (BL_UNLIKELY(prev == 0)) {
          blRuntimeFailure("The thread pool has been dereferenced during acquiring threads\n");
        }

        if (flags & BL_THREAD_POOL_ACQUIRE_FLAG_ALL_OR_NOTHING) {
          self->acquiredThreadCount += nAcquired;
          blAtomicFetchAddStrong(&self->createdThreadCount, nAcquired);

          blThreadPoolReleaseThreadsInternal(self, threads, nAcquired);
          *reasonOut = reason;
          return 0;
        }

        // Don't try again... The `reason` will be propagated to the caller.
        break;
      }

      nAcquired++;
    }

    blAtomicFetchAddStrong(&self->createdThreadCount, nAcquired);
  }

  uint32_t bwIndex = 0;
  uint32_t nAcqPrev = nAcquired;

  while (nAcquired < n) {
    BLBitWord mask = self->pooledThreadBits.data[bwIndex];
    bl::BitWordIterator<BLBitWord> it(mask);

    while (it.hasNext()) {
      uint32_t bit = it.next();
      mask ^= bl::IntOps::lsbBitAt<BLBitWord>(bit);

      uint32_t threadIndex = bwIndex * bl::IntOps::bitSizeOf<BLBitWord>() + bit;
      BLThread* thread = self->threads[threadIndex];

      BL_ASSERT(thread != nullptr);
      self->threads[threadIndex] = nullptr;

      threads[nAcquired] = thread;
      if (++nAcquired == n)
        break;
    }

    self->pooledThreadBits.data[bwIndex] = mask;
    if (++bwIndex >= BLInternalThreadPool::PooledThreadBitArray::kFixedArraySize)
      break;
  }

  self->pooledThreadCount -= nAcquired - nAcqPrev;
  self->acquiredThreadCount += nAcquired;

  *reasonOut = reason;
  return nAcquired;
}

static uint32_t BL_CDECL blThreadPoolAcquireThreads(BLThreadPool* self_, BLThread** threads, uint32_t n, uint32_t flags, BLResult* reason) noexcept {
  BLInternalThreadPool* self = static_cast<BLInternalThreadPool*>(self_);
  BLLockGuard<BLMutex> guard(self->mutex);

  return blThreadPoolAcquireThreadsInternal(self, threads, n, flags, reason);
}

static void BL_CDECL blThreadPoolReleaseThreads(BLThreadPool* self_, BLThread** threads, uint32_t n) noexcept {
  BLInternalThreadPool* self = static_cast<BLInternalThreadPool*>(self_);
  BLLockGuard<BLMutex> guard(self->mutex);

  return blThreadPoolReleaseThreadsInternal(self, threads, n);
}

// ThreadPool - Global
// ===================

BLThreadPool* blThreadPoolGlobal() noexcept { return &blGlobalThreadPool; }

// ThreadPool - Runtime Registration
// =================================

static void BL_CDECL blThreadPoolOnShutdown(BLRuntimeContext* rt) noexcept {
  blUnused(rt);
  blThreadPoolRelease(&blGlobalThreadPool);
}

static void BL_CDECL blThreadPoolRtCleanup(BLRuntimeContext* rt, BLRuntimeCleanupFlags cleanupFlags) noexcept {
  blUnused(rt);
  if (cleanupFlags & BL_RUNTIME_CLEANUP_THREAD_POOL)
    blGlobalThreadPool->cleanup();
}

void blThreadPoolRtInit(BLRuntimeContext* rt) noexcept {
  // ThreadPool virtual table.
  blThreadPoolVirt.addRef = blThreadPoolAddRef;
  blThreadPoolVirt.release = blThreadPoolRelease;
  blThreadPoolVirt.maxThreadCount = blThreadPoolMaxThreadCount;
  blThreadPoolVirt.pooledThreadCount = blThreadPoolPooledThreadCount;
  blThreadPoolVirt.setThreadAttributes = blThreadPoolSetThreadAttributes;
  blThreadPoolVirt.cleanup = blThreadPoolCleanup;
  blThreadPoolVirt.acquireThreads = blThreadPoolAcquireThreads;
  blThreadPoolVirt.releaseThreads = blThreadPoolReleaseThreads;

  // ThreadPool built-in global instance.
  BLThreadAttributes attrs {};
  attrs.stackSize = rt->systemInfo.threadStackSize;

  blGlobalThreadPool.init();
  blGlobalThreadPool->setThreadAttributes(attrs);

  rt->shutdownHandlers.add(blThreadPoolOnShutdown);
  rt->cleanupHandlers.add(blThreadPoolRtCleanup);
}
