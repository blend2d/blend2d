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

#include "../api-build_p.h"
#include "../runtime_p.h"
#include "../support_p.h"
#include "../threading/atomic_p.h"
#include "../threading/conditionvariable_p.h"
#include "../threading/thread_p.h"

#ifdef _WIN32
  #include <process.h>
#endif

// ============================================================================
// [Globals]
// ============================================================================

static BLThreadVirt blThreadVirt;

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

  volatile uint32_t internalStatus;
  volatile uint32_t reserved;

  BLThreadFunc workFunc;
  BLThreadFunc doneFunc;
  void* workData;

  BLThreadFunc exitFunc;
  void* exitData;

  BLMutex mutex;
  BLConditionVariable condition;

  BL_INLINE BLInternalThread(BLThreadFunc exitFunc, void* exitData) noexcept
    : BLThread { &blThreadVirt },
      handle {},
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
  uint32_t status = BL_THREAD_STATUS_IDLE;

  do {
    BLThreadFunc workFunc;
    BLThreadFunc doneFunc;
    void* workData;

    for (;;) {
      BLLockGuard<BLMutex> guard(thread->mutex);
      status = thread->internalStatus;

      if (status == BL_THREAD_STATUS_IDLE) {
        thread->condition.wait(thread->mutex);
        status = thread->internalStatus;
      }

      if (status != BL_THREAD_STATUS_IDLE) {
        workFunc = thread->workFunc;
        doneFunc = thread->doneFunc;
        workData = thread->workData;

        thread->workFunc = nullptr;
        thread->doneFunc = nullptr;
        thread->workData = nullptr;
        break;
      }
    }

    // Run the task.
    if (workFunc)
      workFunc(thread, workData);

    thread->mutex.protect([&]() {
      thread->internalStatus = BL_THREAD_STATUS_IDLE;
    });

    if (doneFunc)
      doneFunc(thread, workData);
  } while (status != BL_THREAD_STATUS_QUITTING);

  thread->exitFunc(thread, thread->exitData);
}

static uint32_t BL_CDECL blThreadStatus(const BLThread* self_) noexcept {
  const BLInternalThread* self = static_cast<const BLInternalThread*>(self_);
  return blAtomicFetch(&self->internalStatus);
}

static BLResult BL_CDECL blThreadRun(BLThread* self_, BLThreadFunc workFunc, BLThreadFunc doneFunc, void* data) noexcept {
  BLInternalThread* self = static_cast<BLInternalThread*>(self_);

  {
    BLLockGuard<BLMutex> guard(self->mutex);
    if (self->internalStatus != BL_THREAD_STATUS_IDLE)
      return blTraceError(BL_ERROR_BUSY);

    self->internalStatus = BL_THREAD_STATUS_RUNNING;
    self->workFunc = workFunc;
    self->doneFunc = doneFunc;
    self->workData = data;
  }

  self->condition.signal();
  return BL_SUCCESS;
}

static BLResult BL_CDECL blThreadQuit(BLThread* self_, uint32_t quitFlags) noexcept {
  BLInternalThread* self = static_cast<BLInternalThread*>(self_);

  // Update status to `BL_THREAD_STATUS_QUITTING` must be guarted as more threads
  // may want this thread to quit. Just return if the thread is already quitting.
  {
    BLLockGuard<BLMutex> guard(self->mutex);
    if (self->internalStatus == BL_THREAD_STATUS_QUITTING)
      return BL_SUCCESS;
    self->internalStatus = BL_THREAD_STATUS_QUITTING;
  }

#ifdef _WIN32
  // Windows specific - it could happen that after returning from main() all
  // threads are terminated even before calling static destructors or DllMain().
  // This means that the thread could be already terminated and we executed the
  // regular code-path we would get stuck forever during the cleanup.
  if (quitFlags & BL_THREAD_QUIT_ON_EXIT) {
    DWORD result = WaitForSingleObject((HANDLE)self->handle, 0);
    if (result == WAIT_OBJECT_0) {
      // The thread was already terminated by the runtime. Call 'exitFunc'
      // manually so the object can be released properly from our side.
      self->exitFunc(self, self->exitData);
      return BL_SUCCESS;
    }
  }
#endif

  self->condition.signal();
  return BL_SUCCESS;
}

// ============================================================================
// [BLThread - Windows]
// ============================================================================

#ifdef _WIN32
static unsigned BL_STDCALL blThreadEntryPointWrapper(void* arg) noexcept {
  BLInternalThread* thread = static_cast<BLInternalThread*>(arg);
  blThreadEntryPoint(thread);
  return 0;
}

BLResult BL_CDECL blThreadCreate(BLThread** threadOut, const BLThreadAttributes* attributes, BLThreadFunc exitFunc, void* exitData) noexcept {
  BLInternalThread* thread = blThreadNew(exitFunc, exitData);
  if (BL_UNLIKELY(!thread))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLResult result = BL_SUCCESS;
  uint32_t flags = 0;
  uint32_t stackSize = attributes->stackSize;

  if (stackSize > 0)
    flags = STACK_SIZE_PARAM_IS_A_RESERVATION;

  HANDLE handle = (HANDLE)_beginthreadex(nullptr, stackSize, blThreadEntryPointWrapper, thread, flags, nullptr);
  if (handle == (HANDLE)0)
    result = BL_ERROR_BUSY;
  else
    thread->handle = (intptr_t)handle;

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
static std::atomic<size_t> blThreadMinimumProbedStackSize;

static void* blThreadEntryPointWrapper(void* arg) noexcept {
  blThreadEntryPoint(static_cast<BLInternalThread*>(arg));
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

  // We bail to the default stack-size if we are not able to probe
  // a small workable stack-size. 8MB is a safe guess if this fails.
  err = pthread_attr_getstacksize(&ptAttr, &defaultStackSize);
  if (err)
    defaultStackSize = 1024u * 1024u * 8u;

  // This should never fail, but...
  err = pthread_attr_setdetachstate(&ptAttr, PTHREAD_CREATE_DETACHED);
  if (err) {
    err = pthread_attr_destroy(&ptAttr);
    return blResultFromPosixError(err);
  }

  BLInternalThread* thread = blThreadNew(exitFunc, exitData);
  if (BL_UNLIKELY(!thread)) {
    err = pthread_attr_destroy(&ptAttr);
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  // Probe loop - Since some implementations fail to create a thread with
  // small stack-size, we would probe a safe value in this case and use
  // it the next time we want to create a thread as a minimum so we don't
  // have to probe it again.
  uint32_t probeCount = 0;
  for (;;) {
    if (currentStackSize)
      pthread_attr_setstacksize(&ptAttr, currentStackSize);

    err = pthread_create(&thread->handle, &ptAttr, blThreadEntryPointWrapper, thread);
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
        thread->~BLInternalThread();
        free(thread);

        *threadOut = nullptr;
        return blResultFromPosixError(err);
      }
    }

    currentStackSize <<= 1;
    probeCount++;
  }
}
#endif

// ============================================================================
// [BLThread - RuntimeInit]
// ============================================================================

void blThreadOnInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  // BLThread virtual table.
  blThreadVirt.destroy = blThreadDestroy;
  blThreadVirt.status = blThreadStatus;
  blThreadVirt.run = blThreadRun;
  blThreadVirt.quit = blThreadQuit;
}
