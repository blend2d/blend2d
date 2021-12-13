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

enum BLWorkerThreadQueueFlags : uint32_t {
  BL_WORKER_THREAD_QUEUE_NO_WORK  = 0x00000000u,
  BL_WORKER_THREAD_QUEUE_ADDING   = 0x00000001u,
  BL_WORKER_THREAD_QUEUE_ENQUEUED = 0x00000003u,
  BL_WORKER_THREAD_QUEUE_QUITTING = 0x80000000u
};

class BLInternalWorkerThread : public BLThread {
public:
  struct WorkItem {
    BLThreadFunc func;
    void* data;
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

  struct alignas(BL_CACHE_LINE_SIZE) {
    volatile uint32_t _status;
    volatile uint32_t _queueFlags;
    volatile WorkItem _workItem;
  };

  BL_INLINE BLInternalWorkerThread(const BLWorkerThreadVirt* virt, BLThreadEntryFunc entryFunc, BLThreadFunc exitFunc, void* exitData, void* allocatedPtr) noexcept
    : BLThread{virt},
      _handle{},
      _entryFunc(entryFunc),
      _exitFunc(exitFunc),
      _exitData(exitData),
      _allocatedPtr(allocatedPtr),
      _status(BL_THREAD_STATUS_RUNNING),
      _queueFlags(0),
      _workItem{} {}

  BL_INLINE ~BLInternalWorkerThread() noexcept {
#if _WIN32
    // The handle MUST be closed.
    if (_handle != 0)
      CloseHandle((HANDLE)_handle);
#endif
  }

  // Windows specific - it could happen that after returning from main() all threads are terminated even before
  // calling static destructors or DllMain(). This means that the thread could be already terminated and we executed
  // the regular code-path we would get stuck forever during the cleanup.
  BL_INLINE bool wasThreadTerminated() const noexcept {
#if _WIN32
    DWORD result = WaitForSingleObject((HANDLE)_handle, 0);
    return result == WAIT_OBJECT_0;
#else
    return false;
#endif
  }
};

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

static uint32_t BL_CDECL blPortableWorkerThreadStatus(const BLThread* self) noexcept {
  const BLPortableWorkerThread* thread = static_cast<const BLPortableWorkerThread*>(self);
  BLLockGuard<BLMutex> guard(thread->_mutex);

  uint32_t status = thread->_status;
  uint32_t queueFlags = thread->_queueFlags;

  if (queueFlags & BL_WORKER_THREAD_QUEUE_QUITTING)
    return BL_THREAD_STATUS_QUITTING;
  else
    return status;
}

static BLResult BL_CDECL blPortableWorkerThreadRun(BLThread* self, BLThreadFunc func, void* data) noexcept {
  BLPortableWorkerThread* thread = static_cast<BLPortableWorkerThread*>(self);
  BLLockGuard<BLMutex> guard(thread->_mutex);

  if (thread->_queueFlags != BL_WORKER_THREAD_QUEUE_NO_WORK)
    return BL_ERROR_BUSY;

  thread->_workItem.func = func;
  thread->_workItem.data = data;
  thread->_queueFlags = BL_WORKER_THREAD_QUEUE_ENQUEUED;

  if (thread->_status == BL_THREAD_STATUS_IDLE) {
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
  thread->_queueFlags |= BL_WORKER_THREAD_QUEUE_QUITTING;

  if (thread->_status == BL_THREAD_STATUS_IDLE) {
    guard.release();
    thread->_condition.signal();
  }

  return BL_SUCCESS;
}

static void BL_CDECL blPortableWorkerThreadEntryPoint(BLThread* self) noexcept {
  BLPortableWorkerThread* thread = static_cast<BLPortableWorkerThread*>(self);

  for (;;) {
    BLLockGuard<BLMutex> guard(thread->_mutex);
    uint32_t queueFlags = thread->_queueFlags;

    for (;;) {
      if ((queueFlags & ~BL_WORKER_THREAD_QUEUE_QUITTING) != BL_WORKER_THREAD_QUEUE_NO_WORK)
        break;

      thread->_status = BL_THREAD_STATUS_IDLE;
      thread->_condition.wait(thread->_mutex);
      thread->_status = BL_THREAD_STATUS_RUNNING;
      queueFlags = thread->_queueFlags;
    }

    if (queueFlags & BL_WORKER_THREAD_QUEUE_ENQUEUED) {
      BLInternalWorkerThread::WorkItem work;
      work.func = thread->_workItem.func;
      work.data = thread->_workItem.data;

      if (!(queueFlags & BL_WORKER_THREAD_QUEUE_QUITTING))
        thread->_queueFlags = 0;

      guard.release();
      work.func(thread, work.data);
    }

    if (queueFlags & BL_WORKER_THREAD_QUEUE_QUITTING)
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

static uint32_t BL_CDECL blFutexWorkerThreadStatus(const BLThread* self) noexcept {
  const BLFutexWorkerThread* thread = static_cast<const BLFutexWorkerThread*>(self);

  uint32_t queueFlags = blAtomicFetch(&thread->_queueFlags, std::memory_order_relaxed);
  uint32_t status = blAtomicFetch(&thread->_status, std::memory_order_seq_cst);

  if (queueFlags & BL_WORKER_THREAD_QUEUE_QUITTING)
    return BL_THREAD_STATUS_QUITTING;
  else
    return status;
}

static BLResult BL_CDECL blFutexWorkerThreadRun(BLThread* self, BLThreadFunc workFunc, void* data) noexcept {
  BLInternalWorkerThread* thread = static_cast<BLInternalWorkerThread*>(self);

  uint32_t workStatusExpected = BL_WORKER_THREAD_QUEUE_NO_WORK;
  if (!blAtomicCompareExchange(&thread->_queueFlags, &workStatusExpected, BL_WORKER_THREAD_QUEUE_ADDING)) {
    return BL_ERROR_BUSY;
  }

  blAtomicStore(&thread->_workItem.func, workFunc, std::memory_order_relaxed);
  blAtomicStore(&thread->_workItem.data, data, std::memory_order_relaxed);
  blAtomicFetchOr(&thread->_queueFlags, BL_WORKER_THREAD_QUEUE_ENQUEUED, std::memory_order_relaxed);

  blAtomicThreadFence(std::memory_order_release);

  uint32_t statusExpected = BL_THREAD_STATUS_IDLE;
  if (blAtomicCompareExchange(&thread->_status, &statusExpected, BL_THREAD_STATUS_RUNNING))
    BLFutex::wakeOne(&thread->_status);

  return BL_SUCCESS;
}

static BLResult BL_CDECL blFutexWorkerThreadQuit(BLThread* self, uint32_t quitFlags) noexcept {
  BLInternalWorkerThread* thread = static_cast<BLInternalWorkerThread*>(self);

  if ((quitFlags & BL_THREAD_QUIT_ON_EXIT) && thread->wasThreadTerminated()) {
    // The thread was already terminated by the runtime. Call '_exitFunc'
    // manually so the object can be released properly from our side.
    thread->_exitFunc(thread, thread->_exitData);
    return BL_SUCCESS;
  }

  blAtomicFetchOr(&thread->_queueFlags, BL_WORKER_THREAD_QUEUE_QUITTING, std::memory_order_relaxed);

  uint32_t statusExpected = BL_THREAD_STATUS_IDLE;
  if (blAtomicCompareExchange(&thread->_status, &statusExpected, BL_THREAD_STATUS_RUNNING))
    BLFutex::wakeOne(&thread->_status);

  return BL_SUCCESS;
}

static void BL_CDECL blFutexWorkerThreadEntryPoint(BLThread* self) noexcept {
  BLFutexWorkerThread* thread = static_cast<BLFutexWorkerThread*>(self);
  uint32_t spinCount = 0;

  for (;;) {
    uint32_t queueFlags = blAtomicFetch(&thread->_queueFlags, std::memory_order_acquire);
    switch (queueFlags) {
      // Some thread is enqueuing some work at the moment.
      case BL_WORKER_THREAD_QUEUE_ADDING:
      case BL_WORKER_THREAD_QUEUE_ADDING | BL_WORKER_THREAD_QUEUE_QUITTING:
        if (++spinCount < 32)
          break;

        BL_FALLTHROUGH

      // No work enqueued.
      case BL_WORKER_THREAD_QUEUE_NO_WORK:
        spinCount = 0;

        blAtomicStore(&thread->_status, BL_THREAD_STATUS_IDLE, std::memory_order_release);
        BLFutex::wait(&thread->_status, BL_THREAD_STATUS_IDLE);
        break;

      // We have work to do.
      case BL_WORKER_THREAD_QUEUE_ENQUEUED:
      case BL_WORKER_THREAD_QUEUE_ENQUEUED | BL_WORKER_THREAD_QUEUE_QUITTING:
        spinCount = 0;

        blAtomicThreadFence(std::memory_order_acquire);

        {
          BLThreadFunc workFunc = blAtomicFetch(&thread->_workItem.func, std::memory_order_relaxed);
          void* workData = blAtomicFetch(&thread->_workItem.data, std::memory_order_relaxed);

          blAtomicFetchAnd(&thread->_queueFlags, BL_WORKER_THREAD_QUEUE_QUITTING, std::memory_order_seq_cst);
          workFunc(thread, workData);
        }

        if (!(queueFlags & BL_WORKER_THREAD_QUEUE_QUITTING))
          break;

        BL_FALLTHROUGH

      case BL_WORKER_THREAD_QUEUE_NO_WORK | BL_WORKER_THREAD_QUEUE_QUITTING:
        thread->_exitFunc(thread, thread->_exitData);
        return;
    }
  }
}

// Thread - WorkerThread - API
// ===========================

static BLInternalWorkerThread* blThreadNew(BLThreadFunc exitFunc, void* exitData) noexcept {
  uint32_t alignment = 64;
  size_t implSize = BL_FUTEX_ENABLED ? sizeof(BLFutexWorkerThread)
                                     : sizeof(BLPortableWorkerThread);

  void* allocatedPtr = malloc(implSize + alignment);
  if (BL_UNLIKELY(!allocatedPtr))
    return nullptr;

  void* alignedPtr = BLIntOps::alignUp(allocatedPtr, alignment);
  if (BL_FUTEX_ENABLED)
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
  blFutexWorkerThreadVirt.status = blFutexWorkerThreadStatus;
  blFutexWorkerThreadVirt.run = blFutexWorkerThreadRun;
  blFutexWorkerThreadVirt.quit = blFutexWorkerThreadQuit;

  // BLPortableWorkerThread virtual table.
  blPortableWorkerThreadVirt.destroy = blPortableWorkerThreadDestroy;
  blPortableWorkerThreadVirt.status = blPortableWorkerThreadStatus;
  blPortableWorkerThreadVirt.run = blPortableWorkerThreadRun;
  blPortableWorkerThreadVirt.quit = blPortableWorkerThreadQuit;
}
