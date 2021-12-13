// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_THREADING_THREADPOOL_P_H_INCLUDED
#define BLEND2D_THREADING_THREADPOOL_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../support/fixedbitarray_p.h"
#include "../threading/thread_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

struct BLThreadPool;
struct BLThreadPoolVirt;

enum BLThreadPoolAcquireFlags : uint32_t {
  //! Try to acquire `n` threads, and if it's not possible then don't acquire any threads and return 0 with
  //! `BL_ERROR_THREAD_POOL_EXHAUSTED` reason.
  BL_THREAD_POOL_ACQUIRE_FLAG_ALL_OR_NOTHING = 0x00000001u
};

struct BLThreadPoolVirt {
  BLThreadPool* (BL_CDECL* addRef)(BLThreadPool* self) BL_NOEXCEPT;
  BLResult (BL_CDECL* release)(BLThreadPool* self) BL_NOEXCEPT;
  uint32_t (BL_CDECL* maxThreadCount)(const BLThreadPool* self) BL_NOEXCEPT;
  uint32_t (BL_CDECL* pooledThreadCount)(const BLThreadPool* self) BL_NOEXCEPT;
  BLResult (BL_CDECL* setThreadAttributes)(BLThreadPool* self, const BLThreadAttributes* attributes) BL_NOEXCEPT;
  uint32_t (BL_CDECL* cleanup)(BLThreadPool* self, uint32_t threadQuitFlags) BL_NOEXCEPT;
  uint32_t (BL_CDECL* acquireThreads)(BLThreadPool* self, BLThread** threads, uint32_t n, uint32_t flags, BLResult* reason) BL_NOEXCEPT;
  void     (BL_CDECL* releaseThreads)(BLThreadPool* self, BLThread** threads, uint32_t n) BL_NOEXCEPT;
};

struct BLThreadPool {
  const BLThreadPoolVirt* virt;

#ifdef __cplusplus
  BL_INLINE BLThreadPool* addRef() noexcept { return virt->addRef(this); }
  BL_INLINE BLResult release() noexcept { return virt->release(this); }

  //! Returns the number of threads that are pooled at the moment.
  //!
  //! \note This is mostly informative as it's not guaranteed that successive calls to `pooledThreadCount()`
  //! would return the same result as some  threads may be acquired during the request by another thread.
  //!
  //! \threadsafe
  BL_INLINE uint32_t pooledThreadCount() const noexcept {
    return virt->pooledThreadCount(this);
  }

  //! Returns the maximum number of threads that would be allocated by the thread-pool.
  //!
  //! \threadsafe
  BL_INLINE uint32_t maxThreadCount() const noexcept {
    return virt->maxThreadCount(this);
  }

  //! Sets attributes that will affect only new threads created by thread-pool. It's only recommended to set
  //! attributes immediately after the thread-pool has been created as having threads with various attributes
  //! in a single thread-pool could lead into unpredictable behavior and hard to find bugs.
  BL_INLINE BLResult setThreadAttributes(const BLThreadAttributes& attributes) noexcept {
    return virt->setThreadAttributes(this, &attributes);
  }

  //! Cleans up all pooled threads at the moment.
  //!
  //! Cleaning up means to release all pooled threads to the operating system to free all associated resources
  //! with such threads. This operation should not be called often, it's ideal to call it when application was
  //! minimized, for example, or when the application knows that it completed an expensive task, etc...
  BL_INLINE uint32_t cleanup(uint32_t threadQuitFlags = 0) noexcept {
    return virt->cleanup(this, threadQuitFlags);
  }

  //! Acquire `n` threads and store `BLThread*` to the given `threads` array.
  //!
  //! If `exact` is `true` then only the exact number of threads will be acquired, and if it's not possible then
  //! no threads will be acquired. If `exact` is `false` then the number of acquired threads can be less than `n`
  //! in case that acquiring `n` threads is not possible due to reaching `maxThreadCount()`.
  BL_INLINE uint32_t acquireThreads(BLThread** threads, uint32_t n, uint32_t flags, BLResult* reason) noexcept {
    return virt->acquireThreads(this, threads, n, flags, reason);
  }

  //! Release `n` threads that were previously acquired by `acquireThreads()`.
  BL_INLINE void releaseThreads(BLThread** threads, uint32_t n) noexcept {
    return virt->releaseThreads(this, threads, n);
  }
#endif
};

BL_HIDDEN BLThreadPool* blThreadPoolGlobal() noexcept;
BL_HIDDEN BLThreadPool* blThreadPoolCreate() noexcept;

//! \}
//! \endcond

#endif // BLEND2D_THREADING_THREADPOOL_P_H_INCLUDED
