// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_THREADING_THREAD_POOL_P_H_INCLUDED
#define BLEND2D_THREADING_THREAD_POOL_P_H_INCLUDED

#include <blend2d/axl/fixedbit_array_p.h>
#include <blend2d/core/api_internal_p.h>
#include <blend2d/threading/thread_p.h>

namespace bl {

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

struct ThreadPool;
struct ThreadPoolVirt;

enum ThreadPoolAcquireFlags : uint32_t {
  //! Try to acquire `n` threads, and if it's not possible then don't acquire any threads and return 0 with
  //! `BL_ERROR_THREAD_POOL_EXHAUSTED` reason.
  BL_THREAD_POOL_ACQUIRE_FLAG_ALL_OR_NOTHING = 0x00000001u
};

struct ThreadPoolVirt {
  ThreadPool* (BL_CDECL* add_ref)(ThreadPool* self) noexcept;
  BLResult (BL_CDECL* release)(ThreadPool* self) noexcept;
  uint32_t (BL_CDECL* max_thread_count)(const ThreadPool* self) noexcept;
  uint32_t (BL_CDECL* pooled_thread_count)(const ThreadPool* self) noexcept;
  BLResult (BL_CDECL* set_thread_attributes)(ThreadPool* self, const ThreadAttributes* attributes) noexcept;
  uint32_t (BL_CDECL* cleanup)(ThreadPool* self, uint32_t thread_quit_flags) noexcept;
  uint32_t (BL_CDECL* acquire_threads)(ThreadPool* self, Thread** threads, uint32_t n, uint32_t flags, BLResult* reason) noexcept;
  void     (BL_CDECL* release_threads)(ThreadPool* self, Thread** threads, uint32_t n) noexcept;
};

struct ThreadPool {
  const ThreadPoolVirt* virt;

#ifdef __cplusplus
  BL_INLINE ThreadPool* add_ref() noexcept { return virt->add_ref(this); }
  BL_INLINE BLResult release() noexcept { return virt->release(this); }

  //! Returns the number of threads that are pooled at the moment.
  //!
  //! \note This is mostly informative as it's not guaranteed that successive calls to `pooled_thread_count()`
  //! would return the same result as some  threads may be acquired during the request by another thread.
  //!
  //! \note This function is thread-safe.
  BL_INLINE uint32_t pooled_thread_count() const noexcept {
    return virt->pooled_thread_count(this);
  }

  //! Returns the maximum number of threads that would be allocated by the thread-pool.
  //!
  //! \note This function is thread-safe.
  BL_INLINE uint32_t max_thread_count() const noexcept {
    return virt->max_thread_count(this);
  }

  //! Sets attributes that will affect only new threads created by thread-pool. It's only recommended to set
  //! attributes immediately after the thread-pool has been created as having threads with various attributes
  //! in a single thread-pool could lead into unpredictable behavior and hard to find bugs.
  BL_INLINE BLResult set_thread_attributes(const ThreadAttributes& attributes) noexcept {
    return virt->set_thread_attributes(this, &attributes);
  }

  //! Cleans up all pooled threads at the moment.
  //!
  //! Cleaning up means to release all pooled threads to the operating system to free all associated resources
  //! with such threads. This operation should not be called often, it's ideal to call it when application was
  //! minimized, for example, or when the application knows that it completed an expensive task, etc...
  BL_INLINE uint32_t cleanup(uint32_t thread_quit_flags = 0) noexcept {
    return virt->cleanup(this, thread_quit_flags);
  }

  //! Acquire `n` threads and store `Thread*` to the given `threads` array.
  //!
  //! If `exact` is `true` then only the exact number of threads will be acquired, and if it's not possible then
  //! no threads will be acquired. If `exact` is `false` then the number of acquired threads can be less than `n`
  //! in case that acquiring `n` threads is not possible due to reaching `max_thread_count()`.
  BL_INLINE uint32_t acquire_threads(Thread** threads, uint32_t n, uint32_t flags, BLResult* reason) noexcept {
    return virt->acquire_threads(this, threads, n, flags, reason);
  }

  //! Release `n` threads that were previously acquired by `acquire_threads()`.
  BL_INLINE void release_threads(Thread** threads, uint32_t n) noexcept {
    return virt->release_threads(this, threads, n);
  }
#endif
};

} // {bl}

BL_HIDDEN bl::ThreadPool* bl_thread_pool_global() noexcept;
BL_HIDDEN bl::ThreadPool* bl_thread_pool_create() noexcept;

//! \}
//! \endcond

#endif // BLEND2D_THREADING_THREAD_POOL_P_H_INCLUDED
