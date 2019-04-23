// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blruntime_p.h"
#include "./blsupport_p.h"
#include "./blthreading_p.h"

#ifdef _WIN32
  #include <process.h>
#endif

// ============================================================================
// [Globals]
// ============================================================================

static BLThreadVirt blThreadVirt;

// ============================================================================
// [BLThreadEvent - Windows]
// ============================================================================

#ifdef _WIN32
BLResult blThreadEventCreate(BLThreadEvent* self, bool manualReset, bool signaled) noexcept {
  HANDLE h = CreateEventW(nullptr, manualReset, signaled, nullptr);
  if (BL_UNLIKELY(!h)) {
    self->handle = -1;
    return blTraceError(blResultFromWinError(GetLastError()));
  }

  self->handle = (intptr_t)h;
  return BL_SUCCESS;
}

BLResult blThreadEventDestroy(BLThreadEvent* self) noexcept {
  if (BL_UNLIKELY(self->handle == -1))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  CloseHandle((HANDLE)self->handle);
  self->handle = -1;
  return BL_SUCCESS;
}

bool blThreadEventIsSignaled(const BLThreadEvent* self) noexcept {
  if (BL_UNLIKELY(self->handle == -1))
    return false;
  return WaitForSingleObject((HANDLE)self->handle, 0) == WAIT_OBJECT_0;
}

BLResult blThreadEventSignal(BLThreadEvent* self) noexcept {
  if (BL_UNLIKELY(self->handle == -1))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  SetEvent((HANDLE)self->handle);
  return BL_SUCCESS;
}

BLResult blThreadEventReset(BLThreadEvent* self) noexcept {
  if (BL_UNLIKELY(self->handle == -1))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  ResetEvent((HANDLE)self->handle);
  return BL_SUCCESS;
}

BLResult blThreadEventWaitInternal(BLThreadEvent* self, DWORD dwMilliseconds) noexcept {
  if (BL_UNLIKELY(self->handle == -1))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  DWORD result = WaitForSingleObject((HANDLE)self->handle, dwMilliseconds);
  switch (result) {
    case WAIT_OBJECT_0:
      return BL_SUCCESS;

    case WAIT_FAILED:
      return blTraceError(blResultFromWinError(GetLastError()));

    case WAIT_TIMEOUT:
      return blTraceError(BL_ERROR_TIMED_OUT);

    default:
      return blTraceError(BL_ERROR_INVALID_STATE);
  };
}

BLResult blThreadEventWait(BLThreadEvent* self) noexcept {
  return blThreadEventWaitInternal(self, INFINITE);
}

BLResult blThreadEventTimedWait(BLThreadEvent* self, uint64_t microseconds) noexcept {
  uint32_t milliseconds = uint32_t(blMin<uint64_t>(microseconds / 1000u, INFINITE));
  return blThreadEventWaitInternal(self, milliseconds);
}
#endif

// ============================================================================
// [BLThreadEvent - Posix]
// ============================================================================

#ifndef _WIN32
struct BLThreadEventPosixImpl {
  BLConditionVariable cond;
  BLMutex mutex;
  uint32_t manualReset;
  uint32_t signaled;

  BL_INLINE BLThreadEventPosixImpl(bool manualReset, bool signaled) noexcept
    : cond(),
      mutex(),
      manualReset(manualReset),
      signaled(signaled) {}
};

BLResult blThreadEventCreate(BLThreadEvent* self, bool manualReset, bool signaled) noexcept {
  self->handle = -1;
  void* p = malloc(sizeof(BLThreadEventPosixImpl));

  if (!p)
    return BL_ERROR_OUT_OF_MEMORY;

  BLThreadEventPosixImpl* impl = new(p) BLThreadEventPosixImpl(manualReset, signaled);
  self->handle = (intptr_t)impl;
  return BL_SUCCESS;
}

BLResult blThreadEventDestroy(BLThreadEvent* self) noexcept {
  if (self->handle == -1)
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  BLThreadEventPosixImpl* impl = (BLThreadEventPosixImpl*)self->handle;
  impl->~BLThreadEventPosixImpl();
  free(impl);

  self->handle = -1;
  return BL_SUCCESS;
}

bool blThreadEventIsSignaled(const BLThreadEvent* self) noexcept {
  if (self->handle == -1)
    return false;

  BLThreadEventPosixImpl* impl = (BLThreadEventPosixImpl*)self->handle;
  BLMutexGuard guard(impl->mutex);

  return impl->signaled != 0;
}

BLResult blThreadEventSignal(BLThreadEvent* self) noexcept {
  if (self->handle == -1)
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  BLThreadEventPosixImpl* impl = (BLThreadEventPosixImpl*)self->handle;
  BLMutexGuard guard(impl->mutex);

  if (!impl->signaled) {
    impl->signaled = true;
    if (impl->manualReset)
      impl->cond.broadcast();
    else
      impl->cond.signal();
  }

  return BL_SUCCESS;
}

BLResult blThreadEventReset(BLThreadEvent* self) noexcept {
  if (self->handle == -1)
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  BLThreadEventPosixImpl* impl = (BLThreadEventPosixImpl*)self->handle;
  BLMutexGuard guard(impl->mutex);

  impl->signaled = false;
  return BL_SUCCESS;
}

BLResult blThreadEventWait(BLThreadEvent* self) noexcept {
  if (self->handle == -1)
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  BLThreadEventPosixImpl* impl = (BLThreadEventPosixImpl*)self->handle;
  BLMutexGuard guard(impl->mutex);

  while (!impl->signaled)
    impl->cond.wait(impl->mutex);

  if (!impl->manualReset)
    impl->signaled = false;

  return BL_SUCCESS;
}

BLResult blThreadEventTimedWait(BLThreadEvent* self, uint64_t microseconds) noexcept {
  if (self->handle == -1)
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  struct timespec absTime;
  blGetAbsTimeForWaitCondition(absTime, microseconds);

  BLThreadEventPosixImpl* impl = (BLThreadEventPosixImpl*)self->handle;
  BLMutexGuard guard(impl->mutex);

  while (!impl->signaled)
    if (impl->cond.timedWait(impl->mutex, &absTime) == BL_ERROR_TIMED_OUT)
      return BL_ERROR_TIMED_OUT;

  if (!impl->manualReset)
    impl->signaled = false;

  return BL_SUCCESS;
}
#endif

// ============================================================================
// [BLThread - Internal]
// ============================================================================

class BLInternalThread : public BLThread {
public:
#ifdef _WIN32
  intptr_t handle;
#else
  pthread_t handle;
#endif

  BLThreadEvent event;
  volatile uint32_t internalStatus;
  volatile uint32_t reserved;

  BLThreadFunc workFunc;
  BLThreadFunc doneFunc;
  void* workData;

  BLThreadFunc exitFunc;
  void* exitData;

  BL_INLINE BLInternalThread(BLThreadFunc exitFunc, void* exitData) noexcept
    : BLThread { &blThreadVirt },
      handle {},
      event(true, false),
      internalStatus(BL_THREAD_STATUS_IDLE),
      reserved(0),
      workFunc(nullptr),
      doneFunc(nullptr),
      workData(nullptr),
      exitFunc(exitFunc),
      exitData(exitData) {}

  BL_INLINE ~BLInternalThread() noexcept {
#if _WIN32
    // The handle MUST be closed.
    if (handle != 0)
      CloseHandle((HANDLE)handle);
#endif
  }
};

static BLInternalThread* blThreadNew(BLThreadFunc exitFunc, void* exitData) noexcept {
  BLInternalThread* self = static_cast<BLInternalThread*>(malloc(sizeof(BLInternalThread)));
  if (BL_UNLIKELY(!self))
    return nullptr;
  return new(self) BLInternalThread(exitFunc, exitData);
}

static BLResult BL_CDECL blThreadDestroy(BLThread* self_) noexcept {
  BLInternalThread* self = static_cast<BLInternalThread*>(self_);
  BL_ASSERT(self != nullptr);

  self->~BLInternalThread();
  free(self);

  return BL_SUCCESS;
}

static BL_INLINE void blThreadEntryPoint(BLInternalThread* thread) noexcept {
  for (;;) {
    // Wait for some work to do.
    thread->event.wait();
    blAtomicThreadFence(std::memory_order_acquire);

    BLThreadFunc workFunc = thread->workFunc;
    BLThreadFunc doneFunc = thread->doneFunc;
    void* workData        = thread->workData;

    thread->workFunc = nullptr;
    thread->doneFunc = nullptr;
    thread->workData = nullptr;

    // If the compare-exchange fails and the function was not provided it means that this thread is quitting.
    uint32_t value = BL_THREAD_STATUS_IDLE;
    if (!std::atomic_compare_exchange_strong((std::atomic<uint32_t>*)&thread->internalStatus, &value, uint32_t(BL_THREAD_STATUS_RUNNING)) && !workFunc)
      break;

    // Reset the event - more work can be queued from now...
    thread->event.reset();

    // Run the task.
    workFunc(thread, workData);

    // Again, if the compare-exchange fails it means we are quitting.
    value = BL_THREAD_STATUS_RUNNING;
    bool res = !std::atomic_compare_exchange_strong((std::atomic<uint32_t>*)&thread->internalStatus, &value, uint32_t(BL_THREAD_STATUS_IDLE));

    if (doneFunc)
      doneFunc(thread, workData);

    if (!res && value == BL_THREAD_STATUS_QUITTING)
      break;
  }

  thread->exitFunc(thread, thread->exitData);
}

static uint32_t BL_CDECL blThreadStatus(const BLThread* self_) noexcept {
  const BLInternalThread* self = static_cast<const BLInternalThread*>(self_);
  return blAtomicFetch(&self->internalStatus);
}

static BLResult BL_CDECL blThreadRun(BLThread* self_, BLThreadFunc workFunc, BLThreadFunc doneFunc, void* data) noexcept {
  BLInternalThread* self = static_cast<BLInternalThread*>(self_);
  if (self->event.isSignaled())
    return blTraceError(BL_ERROR_BUSY);

  blAtomicThreadFence(std::memory_order_release);
  self->workFunc = workFunc;
  self->doneFunc = doneFunc;
  self->workData = data;
  self->event.signal();

  return BL_SUCCESS;
}

static BLResult BL_CDECL blThreadQuit(BLThread* self_) noexcept {
  BLInternalThread* self = static_cast<BLInternalThread*>(self_);

  std::atomic_store((std::atomic<uint32_t>*)&self->internalStatus, uint32_t(BL_THREAD_STATUS_QUITTING));
  self->event.signal();

  return BL_SUCCESS;
}

// ============================================================================
// [BLThread - Windows]
// ============================================================================

#ifdef _WIN32
static unsigned BL_STDCALL blThreadEntryPointWrapper(void* arg) {
  BLInternalThread* thread = static_cast<BLInternalThread*>(arg);
  blThreadEntryPoint(thread);
  return 0;
}

BLResult BL_CDECL blThreadCreate(BLThread** threadOut, const BLThreadAttributes* attributes, BLThreadFunc exitFunc, void* exitData) noexcept {
  BLInternalThread* thread = blThreadNew(exitFunc, exitData);
  if (BL_UNLIKELY(!thread))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLResult result = BL_SUCCESS;
  if (!thread->event.isInitialized()) {
    result = blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }
  else {
    uint32_t flags = 0;
    uint32_t stackSize = attributes->stackSize;

    if (stackSize > 0)
      flags = STACK_SIZE_PARAM_IS_A_RESERVATION;

    HANDLE handle = (HANDLE)_beginthreadex(nullptr, stackSize, blThreadEntryPointWrapper, thread, flags, nullptr);
    if (handle == (HANDLE)-1)
      result = BL_ERROR_BUSY;
    else
      thread->handle = (intptr_t)handle;
  }

  if (result == BL_SUCCESS) {
    *threadOut = thread;
    return BL_SUCCESS;
  }
  else {
    thread->~BLInternalThread();
    free(thread);

    *threadOut = nullptr;
    return result;
  }
}
#endif

// ============================================================================
// [BLThread - Posix]
// ============================================================================

#ifndef _WIN32
static void* blThreadEntryPointWrapper(void* arg) {
  blThreadEntryPoint(static_cast<BLInternalThread*>(arg));
  return nullptr;
}

BLResult BL_CDECL blThreadCreate(BLThread** threadOut, const BLThreadAttributes* attributes, BLThreadFunc exitFunc, void* exitData) noexcept {
  pthread_attr_t ptAttr;
  int err = pthread_attr_init(&ptAttr);

  if (err)
    return blResultFromPosixError(err);

  BLResult result = blThreadSetPtAttributes(&ptAttr, attributes);
  if (result == BL_SUCCESS)
    result = blThreadCreatePt(threadOut, &ptAttr, exitFunc, exitData);

  err = pthread_attr_destroy(&ptAttr);
  BL_ASSERT(err == 0);
  BL_UNUSED(err);

  return result;
}

BLResult blThreadCreatePt(BLThread** threadOut, const pthread_attr_t* ptAttr, BLThreadFunc exitFunc, void* exitData) noexcept {
  BLInternalThread* thread = blThreadNew(exitFunc, exitData);
  if (BL_UNLIKELY(!thread))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  int err;
  if (!thread->event.isInitialized())
    err = ENOMEM;
  else
    err = pthread_create(&thread->handle, ptAttr, blThreadEntryPointWrapper, thread);

  if (!err) {
    *threadOut = thread;
    return BL_SUCCESS;
  }
  else {
    thread->~BLInternalThread();
    free(thread);

    *threadOut = nullptr;
    return blResultFromPosixError(err);
  }
}

BLResult blThreadSetPtAttributes(pthread_attr_t* ptAttr, const BLThreadAttributes* src) noexcept {
  pthread_attr_setdetachstate(ptAttr, PTHREAD_CREATE_DETACHED);
  if (src->stackSize) {
    int err = pthread_attr_setstacksize(ptAttr, src->stackSize);
    if (err)
      return blResultFromPosixError(err);
  }
  return BL_SUCCESS;
}
#endif

// ============================================================================
// [BLThreading - RuntimeInit]
// ============================================================================

void blThreadingRtInit(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);

  // BLThread virtual table.
  blThreadVirt.destroy = blThreadDestroy;
  blThreadVirt.status = blThreadStatus;
  blThreadVirt.run = blThreadRun;
  blThreadVirt.quit = blThreadQuit;
}
