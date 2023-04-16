// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../runtime_p.h"
#include "../support/intops_p.h"
#include "../threading/atomic_p.h"
#include "../threading/conditionvariable_p.h"
#include "../threading/futex_p.h"
#include "../threading/thread_p.h"

#ifdef _WIN32
  #include <process.h>
#endif

// Thread - Globals
// ================

static BLWorkerThreadVirt blFutexWorkerThreadVirt;
static BLWorkerThreadVirt blPortableWorkerThreadVirt;

// Thread - InternalWorkerThread
// =============================

// Internal, implements the worker entry point, which then calls work items.
typedef void (BL_CDECL* BLThreadEntryFunc)(BLThread* self) BL_NOEXCEPT;

//! Worker thread status flags.
//!
//! By default the thread is running, if it's not running then it's either idling or quitting.
enum BLWorkerThreadFlags : uint32_t {
  //! Thread has no work and is spleeping.
  BL_WORKER_THREAD_FLAG_SLEEPING = 0x00000001u,
  //! Thread is quitting (may still have work, but won't accept more work).
  BL_WORKER_THREAD_FLAG_QUITTING = 0x00000002u,
  //! A work item is currently being enqueued.
  BL_WORKER_THREAD_FLAG_ENQUEING_WORK = 0x00000004u,
  //! A work item has been enqueued.
  BL_WORKER_THREAD_FLAG_ENQUEUED_WORK = 0x00000008u
};

class alignas(BL_CACHE_LINE_SIZE) BLInternalWorkerThread : public BLThread {
public:
  struct WorkItem {
    BLThreadFunc func;
    void* data;
  };

  struct alignas(BL_CACHE_LINE_SIZE) StatusInfo {
    uint32_t flags;
  };

#ifdef _WIN32
  intptr_t _handle;
#else
  pthread_t _handle;
#endif

  BLThreadEntryFunc _entryFunc;
  BLThreadFunc _exitFunc;
  void* _exitData;
  void* _allocatedPtr;
  WorkItem _workItem;
  StatusInfo _statusData;

  BL_INLINE BLInternalWorkerThread(const BLWorkerThreadVirt* virt, BLThreadEntryFunc entryFunc, BLThreadFunc exitFunc, void* exitData, void* allocatedPtr) noexcept
    : BLThread{virt},
      _handle{},
      _entryFunc(entryFunc),
      _exitFunc(exitFunc),
      _exitData(exitData),
      _allocatedPtr(allocatedPtr),
      _workItem{},
      _statusData{} {}

  BL_INLINE ~BLInternalWorkerThread() noexcept {
#if _WIN32
    // The handle MUST be closed.
    if (_handle != 0)
      CloseHandle((HANDLE)_handle);
#endif
  }

  // Windows specific - it could happen that after returning from main() all threads are terminated even before
  // calling static destructors or DllMain(). This means that the thread could be already terminated and if we
  // have executed the regular code-path we would get stuck forever during the cleanup.
  BL_INLINE bool wasThreadTerminated() const noexcept {
#if _WIN32
    DWORD result = WaitForSingleObject((HANDLE)_handle, 0);
    return result == WAIT_OBJECT_0;
#else
    return false;
#endif
  }
};

static uint32_t BL_CDECL blInternalWorkerThreadStatus(const BLThread* self) noexcept {
  const BLInternalWorkerThread* thread = static_cast<const BLInternalWorkerThread*>(self);
  uint32_t flags = blAtomicFetchRelaxed(&thread->_statusData.flags);

  if (flags & BL_WORKER_THREAD_FLAG_QUITTING)
    return BL_THREAD_STATUS_QUITTING;

  if (!(flags & BL_WORKER_THREAD_FLAG_SLEEPING))
    return BL_THREAD_STATUS_RUNNING;

  return BL_THREAD_STATUS_IDLE;
}

// Thread - PortableWorkerThread
// =============================

static void BL_CDECL blPortableWorkerThreadEntryPoint(BLThread* self) noexcept;

class BLPortableWorkerThread : public BLInternalWorkerThread {
public:
  mutable BLMutex _mutex;
  BLConditionVariable _condition;

  BL_INLINE BLPortableWorkerThread (BLThreadFunc exitFunc, void* exitData, void* allocatedPtr) noexcept
    : BLInternalWorkerThread(&blPortableWorkerThreadVirt, blPortableWorkerThreadEntryPoint, exitFunc, exitData, allocatedPtr),
      _mutex(),
      _condition() {}
};

static BLResult BL_CDECL blPortableWorkerThreadDestroy(BLThread* self) noexcept {
  BLPortableWorkerThread* thread = static_cast<BLPortableWorkerThread*>(self);
  BL_ASSERT(thread != nullptr);

  void* allocatedPtr = thread->_allocatedPtr;
  thread->~BLPortableWorkerThread();

  free(allocatedPtr);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blPortableWorkerThreadRun(BLThread* self, BLThreadFunc func, void* data) noexcept {
  BLPortableWorkerThread* thread = static_cast<BLPortableWorkerThread*>(self);

  BLLockGuard<BLMutex> guard(thread->_mutex);
  uint32_t flags = thread->_statusData.flags;
  uint32_t kBusyFlags = BL_WORKER_THREAD_FLAG_ENQUEING_WORK | BL_WORKER_THREAD_FLAG_ENQUEUED_WORK | BL_WORKER_THREAD_FLAG_QUITTING;

  if (flags & kBusyFlags)
    return BL_ERROR_BUSY;

  thread->_workItem.func = func;
  thread->_workItem.data = data;
  thread->_statusData.flags = flags | BL_WORKER_THREAD_FLAG_ENQUEUED_WORK;

  if (flags & BL_WORKER_THREAD_FLAG_SLEEPING) {
    guard.release();
    thread->_condition.signal();
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blPortableWorkerThreadQuit(BLThread* self, uint32_t quitFlags) noexcept {
  BLPortableWorkerThread* thread = static_cast<BLPortableWorkerThread*>(self);

  if ((quitFlags & BL_THREAD_QUIT_ON_EXIT) && thread->wasThreadTerminated()) {
    // The thread was already terminated by the runtime. Call '_exitFunc'
    // manually so the object can be released properly from our side.
    thread->_exitFunc(thread, thread->_exitData);
    return BL_SUCCESS;
  }

  BLLockGuard<BLMutex> guard(thread->_mutex);
  uint32_t flags = thread->_statusData.flags;

  thread->_statusData.flags = flags | BL_WORKER_THREAD_FLAG_QUITTING;

  if (flags & BL_WORKER_THREAD_FLAG_SLEEPING) {
    guard.release();
    thread->_condition.signal();
  }

  return BL_SUCCESS;
}

static void BL_CDECL blPortableWorkerThreadEntryPoint(BLThread* self) noexcept {
  BLPortableWorkerThread* thread = static_cast<BLPortableWorkerThread*>(self);

  constexpr uint32_t kHasWorkOrQuittingFlags = BL_WORKER_THREAD_FLAG_QUITTING | BL_WORKER_THREAD_FLAG_ENQUEUED_WORK;

  for (;;) {
    BLLockGuard<BLMutex> guard(thread->_mutex);

    // flags is used also as an accumulator - changes are accumulated first, and then stored before releasing the mutex.
    uint32_t flags = thread->_statusData.flags;

    for (;;) {
      if (flags & kHasWorkOrQuittingFlags)
        break;

      thread->_statusData.flags = flags | BL_WORKER_THREAD_FLAG_SLEEPING;
      thread->_condition.wait(thread->_mutex);

      flags = thread->_statusData.flags & ~BL_WORKER_THREAD_FLAG_SLEEPING;
    }

    bool hasEnqueuedWork = (flags & BL_WORKER_THREAD_FLAG_ENQUEUED_WORK) != 0u;
    BLInternalWorkerThread::WorkItem workItem = thread->_workItem;

    // Update flags now, before we release the mutex.
    flags &= ~BL_WORKER_THREAD_FLAG_ENQUEUED_WORK;
    thread->_statusData.flags = flags;

    // Doesn't matter if we are quitting or not, we have to execute the enqueued work.
    if (hasEnqueuedWork) {
      guard.release();
      workItem.func(thread, workItem.data);
    }

    if (flags & BL_WORKER_THREAD_FLAG_QUITTING)
      break;
  }

  thread->_exitFunc(thread, thread->_exitData);
}

// Thread - FutexWorkerThread
// ==========================

static void BL_CDECL blFutexWorkerThreadEntryPoint(BLThread* self) noexcept;

class BLFutexWorkerThread : public BLInternalWorkerThread {
public:
  BL_INLINE BLFutexWorkerThread(BLThreadFunc exitFunc, void* exitData, void* allocatedPtr) noexcept
    : BLInternalWorkerThread(&blFutexWorkerThreadVirt, blFutexWorkerThreadEntryPoint, exitFunc, exitData, allocatedPtr) {}
};

static BLResult BL_CDECL blFutexWorkerThreadDestroy(BLThread* self) noexcept {
  BLFutexWorkerThread* thread = static_cast<BLFutexWorkerThread*>(self);
  BL_ASSERT(thread != nullptr);

  void* allocatedPtr = thread->_allocatedPtr;
  thread->~BLFutexWorkerThread();

  free(allocatedPtr);
  return BL_SUCCESS;
}

static BL_INLINE void blFutexWorkerThreadWakeUp(BLFutexWorkerThread* thread) noexcept {
  BLFutex::wakeOne(&thread->_statusData.flags);
}

static BLResult BL_CDECL blFutexWorkerThreadRun(BLThread* self, BLThreadFunc workFunc, void* data) noexcept {
  BLFutexWorkerThread* thread = static_cast<BLFutexWorkerThread*>(self);

  // We want to enqueue work atomically here. For that purpose we have two status flags:
  //
  //   - BL_WORKER_THREAD_FLAG_ENQUEING_WORK - work is being enqueued.
  //   - BL_WORKER_THREAD_FLAG_ENQUEUED_WORK - work has been enqueued.
  //
  // We just want to OR `ENQUEING` flag here and if we encounter that another thread was faster enqueuing we just
  // return `BL_ERROR_BUSY`. It does no harm when both `ENQUEING` and `ENQUEUED` flags are set as when the work is
  // picked both flags would be cleared.

  uint32_t prevFlags = blAtomicFetchOrStrong(&thread->_statusData.flags, uint32_t(BL_WORKER_THREAD_FLAG_ENQUEING_WORK));
  uint32_t kBusyFlags = BL_WORKER_THREAD_FLAG_ENQUEING_WORK | BL_WORKER_THREAD_FLAG_ENQUEUED_WORK | BL_WORKER_THREAD_FLAG_QUITTING;

  if (prevFlags & kBusyFlags)
    return BL_ERROR_BUSY;

  blAtomicStoreRelaxed(&thread->_workItem.func, workFunc);
  blAtomicStoreRelaxed(&thread->_workItem.data, data);

  // Finally, this would make the work item available for pick up.
  prevFlags = blAtomicFetchOrSeqCst(&thread->_statusData.flags, uint32_t(BL_WORKER_THREAD_FLAG_ENQUEUED_WORK));

  // Wake up the thread if it is waiting.
  if (prevFlags & BL_WORKER_THREAD_FLAG_SLEEPING)
    blFutexWorkerThreadWakeUp(thread);

  return BL_SUCCESS;
}

static BLResult BL_CDECL blFutexWorkerThreadQuit(BLThread* self, uint32_t quitFlags) noexcept {
  BLFutexWorkerThread* thread = static_cast<BLFutexWorkerThread*>(self);

  if ((quitFlags & BL_THREAD_QUIT_ON_EXIT) && thread->wasThreadTerminated()) {
    // The thread was already terminated by the runtime. Call '_exitFunc'
    // manually so the object can be released properly from our side.
    thread->_exitFunc(thread, thread->_exitData);
    return BL_SUCCESS;
  }

  uint32_t prevFlags = blAtomicFetchOrStrong(&thread->_statusData.flags, uint32_t(BL_WORKER_THREAD_FLAG_QUITTING));

  // If already quitting it makes no sense to even wake it up as it already knows.
  if (prevFlags & BL_WORKER_THREAD_FLAG_QUITTING)
    return BL_SUCCESS;

  // Wake up the thread if it is waiting.
  if (prevFlags & BL_WORKER_THREAD_FLAG_SLEEPING)
    blFutexWorkerThreadWakeUp(thread);

  return BL_SUCCESS;
}

static void BL_CDECL blFutexWorkerThreadEntryPoint(BLThread* self) noexcept {
  BLFutexWorkerThread* thread = static_cast<BLFutexWorkerThread*>(self);

  uint32_t spinCount = 0;
  uint32_t kSpinLimit = 32;

  for (;;) {
    uint32_t flags = blAtomicFetchAndSeqCst(&thread->_statusData.flags, uint32_t(~BL_WORKER_THREAD_FLAG_SLEEPING));

    if (flags & BL_WORKER_THREAD_FLAG_ENQUEUED_WORK) {
      BLThreadFunc workFunc = blAtomicFetchRelaxed(&thread->_workItem.func);
      void* workData = blAtomicFetchRelaxed(&thread->_workItem.data);

      constexpr uint32_t kEnqueingOrEnqueued = BL_WORKER_THREAD_FLAG_ENQUEING_WORK | BL_WORKER_THREAD_FLAG_ENQUEUED_WORK;
      blAtomicFetchAndSeqCst(&thread->_statusData.flags, ~kEnqueingOrEnqueued);

      spinCount = 0;
      workFunc(thread, workData);
      continue;
    }

    if (flags & BL_WORKER_THREAD_FLAG_QUITTING) {
      thread->_exitFunc(thread, thread->_exitData);
      return;
    }

    // If another thread is enqueing work at the moment, spin for a little
    // time to either pick it up immediately or before going to wait.
    if (flags & BL_WORKER_THREAD_FLAG_ENQUEING_WORK && ++spinCount < kSpinLimit)
      continue;

    // Let's wait for more work or a quit signal.
    spinCount = 0;
    flags = blAtomicFetchOrStrong(&thread->_statusData.flags, uint32_t(BL_WORKER_THREAD_FLAG_SLEEPING));

    // Last attempt to avoid waiting...
    if (flags & (BL_WORKER_THREAD_FLAG_ENQUEUED_WORK | BL_WORKER_THREAD_FLAG_QUITTING))
      continue;

    BLFutex::wait(&thread->_statusData.flags, flags | BL_WORKER_THREAD_FLAG_SLEEPING);
  }
}

// Thread - WorkerThread - API
// ===========================

static BLInternalWorkerThread* blThreadNew(BLThreadFunc exitFunc, void* exitData) noexcept {
  uint32_t alignment = BL_CACHE_LINE_SIZE;
  uint32_t futexEnabled = BL_FUTEX_ENABLED;

  size_t implSize = futexEnabled ? sizeof(BLFutexWorkerThread)
                                 : sizeof(BLPortableWorkerThread);

  void* allocatedPtr = malloc(implSize + alignment);
  if (BL_UNLIKELY(!allocatedPtr))
    return nullptr;

  void* alignedPtr = BLIntOps::alignUp(allocatedPtr, alignment);
  if (futexEnabled)
    return new(BLInternal::PlacementNew{alignedPtr}) BLFutexWorkerThread(exitFunc, exitData, allocatedPtr);
  else
    return new(BLInternal::PlacementNew{alignedPtr}) BLPortableWorkerThread(exitFunc, exitData, allocatedPtr);
}

#ifdef _WIN32

// Thread - Windows Implementation
// ===============================

static unsigned BL_STDCALL blThreadEntryPoint(void* arg) noexcept {
  BLInternalWorkerThread* thread = static_cast<BLInternalWorkerThread*>(arg);
  thread->_entryFunc(thread);
  return 0;
}

BLResult BL_CDECL blThreadCreate(BLThread** threadOut, const BLThreadAttributes* attributes, BLThreadFunc exitFunc, void* exitData) noexcept {
  BLInternalWorkerThread* thread = blThreadNew(exitFunc, exitData);
  if (BL_UNLIKELY(!thread))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLResult result = BL_SUCCESS;
  uint32_t flags = 0;
  uint32_t stackSize = attributes->stackSize;

  if (stackSize > 0)
    flags = STACK_SIZE_PARAM_IS_A_RESERVATION;

  HANDLE handle = (HANDLE)_beginthreadex(nullptr, stackSize, blThreadEntryPoint, thread, flags, nullptr);
  if (handle == (HANDLE)-1)
    result = BL_ERROR_BUSY;
  else
    thread->_handle = (intptr_t)handle;

  if (result == BL_SUCCESS) {
    *threadOut = thread;
    return BL_SUCCESS;
  }
  else {
    thread->~BLInternalWorkerThread();
    free(thread);

    *threadOut = nullptr;
    return result;
  }
}

#else

// Thread - POSIX Implementation
// =============================

static std::atomic<size_t> blThreadMinimumProbedStackSize;

static void* blThreadEntryPoint(void* arg) noexcept {
  BLInternalWorkerThread* thread = static_cast<BLInternalWorkerThread*>(arg);
  thread->_entryFunc(thread);
  return nullptr;
}

BLResult BL_CDECL blThreadCreate(BLThread** threadOut, const BLThreadAttributes* attributes, BLThreadFunc exitFunc, void* exitData) noexcept {
  size_t defaultStackSize = 0;
  size_t currentStackSize = attributes->stackSize;
  size_t minimumProbedStackSize = blThreadMinimumProbedStackSize.load(std::memory_order_relaxed);

  if (currentStackSize)
    currentStackSize = blMax<size_t>(currentStackSize, minimumProbedStackSize);

  pthread_attr_t ptAttr;
  int err = pthread_attr_init(&ptAttr);
  if (err)
    return blResultFromPosixError(err);

  // We bail to the default stack-size if we are not able to probe a small workable stack-size. 8MB is a safe guess.
  err = pthread_attr_getstacksize(&ptAttr, &defaultStackSize);
  if (err)
    defaultStackSize = 1024u * 1024u * 8u;

  // This should never fail, but...
  err = pthread_attr_setdetachstate(&ptAttr, PTHREAD_CREATE_DETACHED);
  if (err) {
    err = pthread_attr_destroy(&ptAttr);
    return blResultFromPosixError(err);
  }

  BLInternalWorkerThread* thread = blThreadNew(exitFunc, exitData);
  if (BL_UNLIKELY(!thread)) {
    err = pthread_attr_destroy(&ptAttr);
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  // Probe loop - Since some implementations fail to create a thread with small stack-size, we would probe a safe value
  // in this case and use it the next time we want to create a thread as a minimum so we don't have to probe it again.
  uint32_t probeCount = 0;
  for (;;) {
    if (currentStackSize)
      pthread_attr_setstacksize(&ptAttr, currentStackSize);

    err = pthread_create(&thread->_handle, &ptAttr, blThreadEntryPoint, thread);
    bool done = !err || !currentStackSize || currentStackSize >= defaultStackSize;

    if (done) {
      pthread_attr_destroy(&ptAttr);

      if (!err) {
        if (probeCount) {
          blThreadMinimumProbedStackSize.store(currentStackSize, std::memory_order_relaxed);
        }

        *threadOut = thread;
        return BL_SUCCESS;
      }
      else {
        thread->destroy();
        *threadOut = nullptr;
        return blResultFromPosixError(err);
      }
    }

    currentStackSize <<= 1;
    probeCount++;
  }
}
#endif

// Thread - Runtime Registration
// =============================

void blThreadRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  // BLFutexWorkerThread virtual table.
  blFutexWorkerThreadVirt.destroy = blFutexWorkerThreadDestroy;
  blFutexWorkerThreadVirt.status = blInternalWorkerThreadStatus;
  blFutexWorkerThreadVirt.run = blFutexWorkerThreadRun;
  blFutexWorkerThreadVirt.quit = blFutexWorkerThreadQuit;

  // BLPortableWorkerThread virtual table.
  blPortableWorkerThreadVirt.destroy = blPortableWorkerThreadDestroy;
  blPortableWorkerThreadVirt.status = blInternalWorkerThreadStatus;
  blPortableWorkerThreadVirt.run = blPortableWorkerThreadRun;
  blPortableWorkerThreadVirt.quit = blPortableWorkerThreadQuit;
}
