// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

// This header implements support functions used across Blend2D library. These
// functions are always inline and implement simple concepts like min/max, bit
// scanning, byte swapping, unaligned memory io, etc...

#ifndef BLEND2D_BLTHREADING_P_H
#define BLEND2D_BLTHREADING_P_H

#include "./blapi-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLMutex]
// ============================================================================

//! Mutex abstraction over Windows or POSIX threads.
class BLMutex {
public:
  BL_NONCOPYABLE(BLMutex)

#ifdef _WIN32
  typedef SRWLOCK Handle;
  Handle handle;

  BL_INLINE BLMutex() noexcept : handle(SRWLOCK_INIT) {}

  BL_INLINE void lock() noexcept { AcquireSRWLockExclusive(&handle); }
  BL_INLINE bool tryLock() noexcept { return TryAcquireSRWLockExclusive(&handle) != 0; }
  BL_INLINE void unlock() noexcept { ReleaseSRWLockExclusive(&handle); }
#else
  typedef pthread_mutex_t Handle;
  Handle handle;

  #ifdef PTHREAD_MUTEX_INITIALIZER
  BL_INLINE BLMutex() noexcept : handle(PTHREAD_MUTEX_INITIALIZER) {}
  BL_INLINE ~BLMutex() noexcept { pthread_mutex_destroy(&handle); }
  #else
  BL_INLINE BLMutex() noexcept { pthread_mutex_init(&handle, nullptr); }
  BL_INLINE ~BLMutex() noexcept { pthread_mutex_destroy(&handle); }
  #endif

  BL_INLINE void lock() noexcept { pthread_mutex_lock(&handle); }
  BL_INLINE bool tryLock() noexcept { return pthread_mutex_trylock(&handle) == 0; }
  BL_INLINE void unlock() noexcept { pthread_mutex_unlock(&handle); }
#endif
};

// ============================================================================
// [BLMutexGuard]
// ============================================================================

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

  std::atomic<uint32_t> _hi;
  std::atomic<uint32_t> _lo;

  #else

  BL_INLINE void reset() noexcept { _counter = 0; }
  BL_INLINE uint64_t next() noexcept { return ++_counter; }

  std::atomic<uint64_t> _counter;

  #endif
};

//! \}
//! \endcond

#endif // BLEND2D_BLTHREADING_P_H
