// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_BLTHREADING_P_H
#define BLEND2D_BLTHREADING_P_H

#include "./blapi-internal_p.h"

#ifndef _WIN32
  #include <sys/time.h>
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

class BLMutex;
class BLRWLock;
class BLConditionVariable;
class BLThreadEvent;

struct BLThread;
struct BLThreadVirt;
struct BLThreadAttributes;

// ============================================================================
// [Typedefs]
// ============================================================================

typedef void (BL_CDECL* BLThreadFunc)(BLThread* thread, void* data) BL_NOEXCEPT;

// ============================================================================
// [Constants]
// ============================================================================

enum BLThreadStatus : uint32_t {
  BL_THREAD_STATUS_IDLE = 0,
  BL_THREAD_STATUS_RUNNING = 1,
  BL_THREAD_STATUS_QUITTING = 2
};

// ============================================================================
// [Atomics]
// ============================================================================

static BL_INLINE void blAtomicThreadFence(std::memory_order order = std::memory_order_release) noexcept {
  std::atomic_thread_fence(order);
}

template<typename T>
static BL_INLINE typename std::remove_volatile<T>::type blAtomicFetch(const T* p, std::memory_order order = std::memory_order_relaxed) noexcept {
  typedef typename BLInternal::StdInt<sizeof(T), 0>::Type RawT;
  return (typename std::remove_volatile<T>::type)((const std::atomic<RawT>*)p)->load(order);
}

template<typename T>
static BL_INLINE void blAtomicStore(T* p, typename std::remove_volatile<T>::type value, std::memory_order order = std::memory_order_release) noexcept {
  typedef typename BLInternal::StdInt<sizeof(T), 0>::Type RawT;
  return ((std::atomic<RawT>*)p)->store((RawT)value, order);
}

// ============================================================================
// [Utilities]
// ============================================================================

#ifdef _WIN32
static BL_INLINE void blThreadYield() noexcept { Sleep(0); }
#else
static BL_INLINE void blThreadYield() noexcept { sched_yield(); }
#endif

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
// [BLMutex]
// ============================================================================

//! Mutex abstraction over Windows or POSIX threads.
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
};

//! Mutex guard.
//!
//! Automatically locks the given mutex when created and unlocks it when destroyed.
class BLMutexGuard {
public:
  BL_NONCOPYABLE(BLMutexGuard)

  BLMutex* mutex;

  //! Creates an instance of `BLMutexGuard` and locks the given `mutex`.
  BL_INLINE BLMutexGuard(BLMutex& mutex) noexcept : mutex(&mutex) { this->mutex->lock(); }
  //! Creates an instance of `BLMutexGuard` and locks the given `mutex`.
  BL_INLINE BLMutexGuard(BLMutex* mutex) noexcept : mutex(mutex) { this->mutex->lock(); }

  //! Unlocks the mutex that has been locked by the constructor.
  BL_INLINE ~BLMutexGuard() noexcept { this->mutex->unlock(); }
};

// ============================================================================
// [BLRWLock]
// ============================================================================

class BLRWLock {
public:
  BL_NONCOPYABLE(BLRWLock)

#ifdef _WIN32
  SRWLOCK handle;

  BL_INLINE BLRWLock() noexcept : handle(SRWLOCK_INIT) {}

  BL_INLINE void lockRead() noexcept { AcquireSRWLockShared(&handle); }
  BL_INLINE void lockWrite() noexcept { AcquireSRWLockExclusive(&handle); }

  BL_INLINE void tryLockRead() noexcept { TryAcquireSRWLockShared(&handle); }
  BL_INLINE void tryLockWrite() noexcept { TryAcquireSRWLockExclusive(&handle); }

  BL_INLINE void unlockRead() noexcept { ReleaseSRWLockShared(&handle); }
  BL_INLINE void unlockWrite() noexcept { ReleaseSRWLockExclusive(&handle); }
#else
  pthread_rwlock_t handle;

  #ifdef PTHREAD_RWLOCK_INITIALIZER
  BL_INLINE BLRWLock() noexcept : handle(PTHREAD_RWLOCK_INITIALIZER) {}
  #else
  BL_INLINE BLRWLock() noexcept { pthread_rwlock_init(&handle, nullptr); }
  #endif
  BL_INLINE ~BLRWLock() noexcept { pthread_rwlock_destroy(&handle); }

  BL_INLINE void lockRead() noexcept { pthread_rwlock_rdlock(&handle); }
  BL_INLINE void lockWrite() noexcept { pthread_rwlock_wrlock(&handle); }

  BL_INLINE bool tryLockRead() noexcept { return pthread_rwlock_tryrdlock(&handle) == 0; }
  BL_INLINE bool tryLockWrite() noexcept { return pthread_rwlock_trywrlock(&handle) == 0; }

  BL_INLINE void unlockRead() noexcept { pthread_rwlock_unlock(&handle); }
  BL_INLINE void unlockWrite() noexcept { pthread_rwlock_unlock(&handle); }
#endif
};

class BLRWLockReadGuard {
public:
  BL_NONCOPYABLE(BLRWLockReadGuard)

  BLRWLock* lock;

  BL_INLINE BLRWLockReadGuard(BLRWLock& lock) noexcept : lock(&lock) { this->lock->lockRead(); }
  BL_INLINE BLRWLockReadGuard(BLRWLock* lock) noexcept : lock(lock) { this->lock->lockRead(); }
  BL_INLINE ~BLRWLockReadGuard() noexcept { this->lock->unlockRead(); }
};

class BLRWLockWriteGuard {
public:
  BL_NONCOPYABLE(BLRWLockWriteGuard)

  BLRWLock* lock;

  BL_INLINE BLRWLockWriteGuard(BLRWLock& lock) noexcept : lock(&lock) { this->lock->lockWrite(); }
  BL_INLINE BLRWLockWriteGuard(BLRWLock* lock) noexcept : lock(lock) { this->lock->lockWrite(); }
  BL_INLINE ~BLRWLockWriteGuard() noexcept { this->lock->unlockWrite(); }
};

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

  BL_INLINE BLResult timedWait(BLMutex& mutex, uint64_t microseconds) noexcept {
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

  BL_INLINE BLResult timedWait(BLMutex& mutex, const struct timespec* absTime) noexcept {
    int ret = pthread_cond_timedwait(&handle, &mutex.handle, absTime);
    if (ret == 0)
      return BL_SUCCESS;

    // We don't trace `BL_ERROR_TIMED_OUT` as it's not unexpected.
    return BL_ERROR_TIMED_OUT;
  }

  BL_INLINE BLResult timedWait(BLMutex& mutex, uint64_t microseconds) noexcept {
    struct timespec absTime;
    blGetAbsTimeForWaitCondition(absTime, microseconds);
    return timedWait(mutex, &absTime);
  }
#endif
};

// ============================================================================
// [BLThreadEvent]
// ============================================================================

BL_HIDDEN BLResult BL_CDECL blThreadEventCreate(BLThreadEvent* self, bool manualReset, bool signaled) noexcept;
BL_HIDDEN BLResult BL_CDECL blThreadEventDestroy(BLThreadEvent* self) noexcept;
BL_HIDDEN bool     BL_CDECL blThreadEventIsSignaled(const BLThreadEvent* self) noexcept;
BL_HIDDEN BLResult BL_CDECL blThreadEventSignal(BLThreadEvent* self) noexcept;
BL_HIDDEN BLResult BL_CDECL blThreadEventReset(BLThreadEvent* self) noexcept;
BL_HIDDEN BLResult BL_CDECL blThreadEventWait(BLThreadEvent* self) noexcept;
BL_HIDDEN BLResult BL_CDECL blThreadEventTimedWait(BLThreadEvent* self, uint64_t microseconds) noexcept;

class BLThreadEvent {
public:
  BL_NONCOPYABLE(BLThreadEvent)

  intptr_t handle;

  explicit BL_INLINE BLThreadEvent(bool manualReset = false, bool signaled = false) noexcept {
    blThreadEventCreate(this, manualReset, signaled);
  }
  BL_INLINE ~BLThreadEvent() noexcept { blThreadEventDestroy(this); }

  BL_INLINE bool isInitialized() const noexcept { return handle != -1; }
  BL_INLINE bool isSignaled() const noexcept { return blThreadEventIsSignaled(this); }

  BL_INLINE BLResult signal() noexcept { return blThreadEventSignal(this); }
  BL_INLINE BLResult reset() noexcept { return blThreadEventReset(this); }
  BL_INLINE BLResult wait() noexcept { return blThreadEventWait(this); }
  BL_INLINE BLResult timedWait(uint64_t microseconds) noexcept { return blThreadEventTimedWait(this, microseconds); }
};

// ============================================================================
// [BLThread]
// ============================================================================

struct BLThreadAttributes {
  uint32_t stackSize;
};

struct BLThreadVirt {
  BLResult (BL_CDECL* destroy)(BLThread* self) BL_NOEXCEPT;
  uint32_t (BL_CDECL* status)(const BLThread* self) BL_NOEXCEPT;
  BLResult (BL_CDECL* run)(BLThread* self, BLThreadFunc workFunc, BLThreadFunc doneFunc, void* data) BL_NOEXCEPT;
  BLResult (BL_CDECL* quit)(BLThread* self) BL_NOEXCEPT;
};

struct BLThread {
  BLThreadVirt* virt;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus
  BL_INLINE BLResult destroy() noexcept {
    return virt->destroy(this);
  }

  BL_INLINE uint32_t status() const noexcept {
    return virt->status(this);
  }

  BL_INLINE BLResult run(BLThreadFunc workFunc, BLThreadFunc doneFunc, void* data) noexcept {
    return virt->run(this, workFunc, doneFunc, data);
  }

  BL_INLINE BLResult quit() noexcept {
    return virt->quit(this);
  }
  #endif
  // --------------------------------------------------------------------------
};

BL_HIDDEN BLResult BL_CDECL blThreadCreate(BLThread** threadOut, const BLThreadAttributes* attributes, BLThreadFunc exitFunc, void* exitData) noexcept;

#ifndef _WIN32
BL_HIDDEN BLResult blThreadCreatePt(BLThread** threadOut, const pthread_attr_t* ptAttr, BLThreadFunc exitFunc, void* exitData) noexcept;
BL_HIDDEN BLResult blThreadSetPtAttributes(pthread_attr_t* ptAttr, const BLThreadAttributes* src) noexcept;
#endif

// ============================================================================
// [BLAtomicUInt64Generator]
// ============================================================================

//! A context that can be used to generate unique 64-bit IDs in a thread-safe
//! manner. It uses atomic operations to make the generation as fast as possible
//! and provides an implementation for both 32-bit and 64-bit targets.
//!
//! The implementation choses a different startegy between 32-bit and 64-bit hosts.
//! On a 64-bit host the implementation always returns sequential IDs starting
//! from 1, on 32-bit host the implementation would always return a number which
//! is higher than the previous one, but it doesn't have to be sequential as it
//! uses the highest bit of LO value as an indicator to increment HI value.
struct BLAtomicUInt64Generator {
#if BL_TARGET_ARCH_BITS < 64
  std::atomic<uint32_t> _hi;
  std::atomic<uint32_t> _lo;

  BL_INLINE void reset() noexcept {
    _hi = 0;
    _lo = 0;
  }

  BL_INLINE uint64_t next() noexcept {
    // This implementation doesn't always return an incrementing value as it's
    // not the point. The requirement is to never return the same value, so it
    // sacrifices one bit in `_lo` counter that would tell us to increment `_hi`
    // counter and try again.
    const uint32_t kThresholdLo32 = 0x80000000u;

    for (;;) {
      uint32_t hiValue = _hi.load();
      uint32_t loValue = ++_lo;

      // This MUST support even cases when the thread executing this function
      // right now is terminated. When we reach the threshold we increment
      // `_hi`, which would contain a new HIGH value that will be used
      // immediately, then we remove the threshold mark from LOW value and try
      // to get a new LOW and HIGH values to return.
      if (BL_UNLIKELY(loValue & kThresholdLo32)) {
        _hi++;

        // If the thread is interrupted here we only incremented the HIGH value.
        // In this case another thread that might call `next()` would end up
        // right here trying to clear `kThresholdLo32` from LOW value as well,
        // which is fine.
        _lo.fetch_and(uint32_t(~kThresholdLo32));
        continue;
      }

      return (uint64_t(hiValue) << 32) | loValue;
    }
  }
#else
  std::atomic<uint64_t> _counter;

  BL_INLINE void reset() noexcept { _counter = 0; }
  BL_INLINE uint64_t next() noexcept { return ++_counter; }
#endif
};

//! \}
//! \endcond

#endif // BLEND2D_BLTHREADING_P_H
