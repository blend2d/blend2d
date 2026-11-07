// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_THREADING_MUTEX_P_H_INCLUDED
#define BLEND2D_THREADING_MUTEX_P_H_INCLUDED

#include <blend2d/axl/commons.h>
#include <blend2d/core/api_internal_p.h>

#if !defined(_WIN32)
  #include <pthread.h>
#endif

namespace bl {

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name Guards
//! \{

//! Mutex guard.
//!
//! Locks the given mutex at construction time and unlocks it when destroyed.
template<typename MutexType>
class LockGuard {
public:
  BL_NONCOPYABLE(LockGuard)

  //! Pointer to the mutex in use.
  MutexType* _mutex;

  //! Creates an instance of `LockGuard` and locks the given `mutex`.
  BL_INLINE explicit LockGuard(MutexType& mutex) noexcept : _mutex(&mutex) {
    BL_ASSUME(_mutex != nullptr);
    _mutex->lock();
  }

  //! Creates an instance of `LockGuard` and locks the given `mutex`.
  BL_INLINE explicit LockGuard(MutexType* mutex) noexcept : _mutex(mutex) {
    BL_ASSUME(_mutex != nullptr);
    _mutex->lock();
  }

  //! Destroys `SharedLockGuard` and unlocks the `mutex`.
  BL_INLINE ~LockGuard() noexcept {
    if (_mutex)
      _mutex->unlock();
  }

  BL_INLINE void release() noexcept {
    _mutex->unlock();
    _mutex = nullptr;
  }
};

template<typename MutexType>
class SharedLockGuard {
public:
  BL_NONCOPYABLE(SharedLockGuard)

  //! Pointer to the mutex in use.
  MutexType* _mutex;

  //! Creates an instance of `SharedLockGuard` and locks the given `mutex`.
  BL_INLINE explicit SharedLockGuard(MutexType& mutex) noexcept : _mutex(&mutex) {
    BL_ASSUME(_mutex != nullptr);
    _mutex->lock_shared();
  }

  //! Creates an instance of `SharedLockGuard` and locks the given `mutex`.
  BL_INLINE explicit SharedLockGuard(MutexType* mutex) noexcept : _mutex(mutex) {
    BL_ASSUME(_mutex != nullptr);
    _mutex->lock_shared();
  }

  //! Destroys `SharedLockGuard` and unlocks the `mutex`.
  BL_INLINE ~SharedLockGuard() noexcept {
    if (_mutex)
      _mutex->unlock_shared();
  }

  BL_INLINE void release() noexcept {
    _mutex->unlock_shared();
    _mutex = nullptr;
  }
};

//! \}

//! \name Mutex & SharedMutex
//! \{

//! Mutex - a synchronization primitive that can be used to protect shared data from being simultaneously accessed
//! by multiple threads.
//!
//! Implementations:
//!   - Posix implementation uses `pthread_mutex_t`, it's non-recursive by design.
//!   - Windows implementation uses `SRWLOCK` instead of `CRITICAL_SECTION`. It means that the synchronization would
//!     most likely be less fair, but should be also faster as `SRWLOCK` has less overhead than `CRITICAL_SECTION` and
//!     is not recursive by design, which is what we expect from mutex in Blend2D.
//!
//! \note The API should be similar to `std::mutex`, however, member functions never throw an exception.
class Mutex {
public:
  BL_NONCOPYABLE(Mutex)

#ifdef _WIN32
  SRWLOCK handle;

  BL_INLINE Mutex() noexcept : handle(SRWLOCK_INIT) {}

  BL_INLINE void lock() noexcept { AcquireSRWLockExclusive(&handle); }
  BL_INLINE bool try_lock() noexcept { return TryAcquireSRWLockExclusive(&handle) != 0; }
  BL_INLINE void unlock() noexcept { ReleaseSRWLockExclusive(&handle); }
#else
  pthread_mutex_t handle;

  #ifdef PTHREAD_MUTEX_INITIALIZER
  BL_INLINE Mutex() noexcept : handle(PTHREAD_MUTEX_INITIALIZER) {}
  #else
  BL_INLINE Mutex() noexcept { pthread_mutex_init(&handle, nullptr); }
  #endif
  BL_INLINE ~Mutex() noexcept { pthread_mutex_destroy(&handle); }

  BL_INLINE void lock() noexcept { pthread_mutex_lock(&handle); }
  BL_INLINE bool try_lock() noexcept { return pthread_mutex_trylock(&handle) == 0; }
  BL_INLINE void unlock() noexcept { pthread_mutex_unlock(&handle); }
#endif

  //! Protects the execution of the given function with `LockGuard` making the execution exclusive.
  template<typename Fn>
  BL_INLINE auto protect(Fn&& fn) noexcept {
    LockGuard<Mutex> guard(this);
    return axl::forward<Fn>(fn)();
  }
};

//! Similar to `Mutex`, but extends the functionality by allowing shared and exclusive access levels.
class SharedMutex {
public:
  BL_NONCOPYABLE(SharedMutex)

#ifdef _WIN32
  SRWLOCK handle;

  BL_INLINE SharedMutex() noexcept : handle(SRWLOCK_INIT) {}

  BL_INLINE void lock() noexcept { AcquireSRWLockExclusive(&handle); }
  BL_INLINE void try_lock() noexcept { TryAcquireSRWLockExclusive(&handle); }
  BL_INLINE void unlock() noexcept { ReleaseSRWLockExclusive(&handle); }

  BL_INLINE void lock_shared() noexcept { AcquireSRWLockShared(&handle); }
  BL_INLINE void try_lock_shared() noexcept { TryAcquireSRWLockShared(&handle); }
  BL_INLINE void unlock_shared() noexcept { ReleaseSRWLockShared(&handle); }
#else
  pthread_rwlock_t handle;

  #ifdef PTHREAD_RWLOCK_INITIALIZER
  BL_INLINE SharedMutex() noexcept : handle(PTHREAD_RWLOCK_INITIALIZER) {}
  #else
  BL_INLINE SharedMutex() noexcept { pthread_rwlock_init(&handle, nullptr); }
  #endif
  BL_INLINE ~SharedMutex() noexcept { pthread_rwlock_destroy(&handle); }

  BL_INLINE void lock() noexcept { pthread_rwlock_wrlock(&handle); }
  BL_INLINE bool try_lock() noexcept { return pthread_rwlock_trywrlock(&handle) == 0; }
  BL_INLINE void unlock() noexcept { pthread_rwlock_unlock(&handle); }

  BL_INLINE void lock_shared() noexcept { pthread_rwlock_rdlock(&handle); }
  BL_INLINE bool try_lock_shared() noexcept { return pthread_rwlock_tryrdlock(&handle) == 0; }
  BL_INLINE void unlock_shared() noexcept { pthread_rwlock_unlock(&handle); }
#endif

  //! Protects the execution of the given function with `LockGuard` making the execution exclusive.
  template<typename Fn>
  BL_INLINE auto protect(Fn&& fn) noexcept {
    LockGuard<SharedMutex> guard(this);
    return axl::forward<Fn>(fn)();
  }

  //! Protects the execution of the given function with `SharedLockGuard` making the execution shared.
  template<typename Fn>
  BL_INLINE auto protect_shared(Fn&& fn) noexcept {
    SharedLockGuard<SharedMutex> guard(this);
    return axl::forward<Fn>(fn)();
  }
};

//! \}

//! \}
//! \endcond

} // {bl}

#endif // BLEND2D_THREADING_MUTEX_P_H_INCLUDED
