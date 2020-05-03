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

#ifndef BLEND2D_THREADING_MUTEX_P_H_INCLUDED
#define BLEND2D_THREADING_MUTEX_P_H_INCLUDED

#include "../api-internal_p.h"
#include <utility>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLLockGuard]
// ============================================================================

//! Mutex guard.
//!
//! Locks the given mutex at construction time and unlocks it when destroyed.
template<typename MutexType>
class BLLockGuard {
public:
  BL_NONCOPYABLE(BLLockGuard)

  //! Pointer to the mutex in use.
  MutexType* mutex;

  //! Creates an instance of `BLLockGuard` and locks the given `mutex`.
  explicit BL_INLINE BLLockGuard(MutexType& mutex) noexcept : mutex(&mutex) { this->mutex->lock(); }
  //! Creates an instance of `BLLockGuard` and locks the given `mutex`.
  explicit BL_INLINE BLLockGuard(MutexType* mutex) noexcept : mutex(mutex) { this->mutex->lock(); }

  //! Destroys `BLSharedLockGuard` and unlocks the `mutex`.
  BL_INLINE ~BLLockGuard() noexcept { this->mutex->unlock(); }
};

// ============================================================================
// [BLSharedLockGuard]
// ============================================================================

template<typename MutexType>
class BLSharedLockGuard {
public:
  BL_NONCOPYABLE(BLSharedLockGuard)

  //! Pointer to the mutex in use.
  MutexType* mutex;

  //! Creates an instance of `BLSharedLockGuard` and locks the given `mutex`.
  explicit BL_INLINE BLSharedLockGuard(MutexType& mutex) noexcept : mutex(&mutex) { this->mutex->lockShared(); }
  //! Creates an instance of `BLSharedLockGuard` and locks the given `mutex`.
  explicit BL_INLINE BLSharedLockGuard(MutexType* mutex) noexcept : mutex(mutex) { this->mutex->lockShared(); }

  //! Destroys `BLSharedLockGuard` and unlocks the `mutex`.
  BL_INLINE ~BLSharedLockGuard() noexcept { this->mutex->unlockShared(); }
};

// ============================================================================
// [BLMutex]
// ============================================================================

//! Mutex - a synchronization primitive that can be used to protect shared data
//! from being simultaneously accessed by multiple threads.
//!
//! Implementations:
//!   - Posix implementation uses `pthread_mutex_t`, it's non-recursive by design.
//!   - Windows implementation uses `SRWLOCK` instead of `CRITICAL_SECTION`. It
//!     means that the synchronization would most likely be less fair, but should
//!     be also faster as `SRWLOCK` has less overhead than `CRITICAL_SECTION` and
//!     is not recursive by design, which is what we expect from mutex in Blend2D.
//!
//! \note The API should be similar to `std::mutex`, however, member functions
//! never throw an exception.
class BLMutex {
public:
  BL_NONCOPYABLE(BLMutex)

#ifdef _WIN32
  SRWLOCK handle;

  BL_INLINE BLMutex() noexcept : handle(SRWLOCK_INIT) {}

  BL_INLINE void lock() noexcept { AcquireSRWLockExclusive(&handle); }
  BL_INLINE bool tryLock() noexcept { return TryAcquireSRWLockExclusive(&handle) != 0; }
  BL_INLINE void unlock() noexcept { ReleaseSRWLockExclusive(&handle); }
#else
  pthread_mutex_t handle;

  #ifdef PTHREAD_MUTEX_INITIALIZER
  BL_INLINE BLMutex() noexcept : handle(PTHREAD_MUTEX_INITIALIZER) {}
  #else
  BL_INLINE BLMutex() noexcept { pthread_mutex_init(&handle, nullptr); }
  #endif
  BL_INLINE ~BLMutex() noexcept { pthread_mutex_destroy(&handle); }

  BL_INLINE void lock() noexcept { pthread_mutex_lock(&handle); }
  BL_INLINE bool tryLock() noexcept { return pthread_mutex_trylock(&handle) == 0; }
  BL_INLINE void unlock() noexcept { pthread_mutex_unlock(&handle); }
#endif

  //! Protects the execution of the given function with `BLLockGuard` making
  //! the execution exclusive.
  template<typename Fn>
  BL_INLINE decltype(std::declval<Fn>()()) protect(Fn&& fn) noexcept {
    BLLockGuard<BLMutex> guard(this);
    return std::forward<Fn>(fn)();
  }
};

// ============================================================================
// [BLSharedMutex]
// ============================================================================

//! Similar to `BLMutex`, but extends the functionality by allowing shared and
//! exclusive access levels.
class BLSharedMutex {
public:
  BL_NONCOPYABLE(BLSharedMutex)

#ifdef _WIN32
  SRWLOCK handle;

  BL_INLINE BLSharedMutex() noexcept : handle(SRWLOCK_INIT) {}

  BL_INLINE void lock() noexcept { AcquireSRWLockExclusive(&handle); }
  BL_INLINE void tryLock() noexcept { TryAcquireSRWLockExclusive(&handle); }
  BL_INLINE void unlock() noexcept { ReleaseSRWLockExclusive(&handle); }

  BL_INLINE void lockShared() noexcept { AcquireSRWLockShared(&handle); }
  BL_INLINE void tryLockShared() noexcept { TryAcquireSRWLockShared(&handle); }
  BL_INLINE void unlockShared() noexcept { ReleaseSRWLockShared(&handle); }
#else
  pthread_rwlock_t handle;

  #ifdef PTHREAD_RWLOCK_INITIALIZER
  BL_INLINE BLSharedMutex() noexcept : handle(PTHREAD_RWLOCK_INITIALIZER) {}
  #else
  BL_INLINE BLSharedMutex() noexcept { pthread_rwlock_init(&handle, nullptr); }
  #endif
  BL_INLINE ~BLSharedMutex() noexcept { pthread_rwlock_destroy(&handle); }

  BL_INLINE void lock() noexcept { pthread_rwlock_wrlock(&handle); }
  BL_INLINE bool tryLock() noexcept { return pthread_rwlock_trywrlock(&handle) == 0; }
  BL_INLINE void unlock() noexcept { pthread_rwlock_unlock(&handle); }

  BL_INLINE void lockShared() noexcept { pthread_rwlock_rdlock(&handle); }
  BL_INLINE bool tryLockShared() noexcept { return pthread_rwlock_tryrdlock(&handle) == 0; }
  BL_INLINE void unlockShared() noexcept { pthread_rwlock_unlock(&handle); }
#endif

  //! Protects the execution of the given function with `BLLockGuard` making
  //! the execution exclusive.
  template<typename Fn>
  BL_INLINE decltype(std::declval<Fn>()()) protect(Fn&& fn) noexcept {
    BLLockGuard<BLSharedMutex> guard(this);
    return std::forward<Fn>(fn)();
  }

  //! Protects the execution of the given function with `BLSharedLockGuard`
  //! making the execution shared.
  template<typename Fn>
  BL_INLINE decltype(std::declval<Fn>()()) protectShared(Fn&& fn) noexcept {
    BLSharedLockGuard<BLSharedMutex> guard(this);
    return std::forward<Fn>(fn)();
  }
};

//! \}
//! \endcond

#endif // BLEND2D_THREADING_MUTEX_P_H_INCLUDED
