// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_alloc.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/core/runtime_scope.h>
#include <blend2d/threading/atomic_p.h>
#include <blend2d/threading/condition_variable_p.h>
#include <blend2d/threading/futex_p.h>
#include <blend2d/threading/thread_p.h>

#ifdef _WIN32
  #include <process.h>
#else
  #include <pthread.h>
#endif

namespace bl {

// Thread - Globals
// ================

static WorkerThreadVirt futex_worker_thread_virt;
static WorkerThreadVirt portable_worker_thread_virt;

// Thread - InternalWorkerThread
// =============================

// Internal, implements the worker entry point, which then calls work items.
typedef void (BL_CDECL* ThreadEntryFunc)(Thread* self) noexcept;

//! Worker thread status flags.
//!
//! By default the thread is running, if it's not running then it's either idling or quitting.
enum BLWorkerThreadFlags : uint32_t {
  //! Thread has no work and is sleeping.
  BL_WORKER_THREAD_FLAG_SLEEPING = 0x00000001u,
  //! Thread is quitting (may still have work, but won't accept more work).
  BL_WORKER_THREAD_FLAG_QUITTING = 0x00000002u,
  //! A work item is currently being enqueued.
  BL_WORKER_THREAD_FLAG_ENQUEUING_WORK = 0x00000004u,
  //! A work item has been enqueued.
  BL_WORKER_THREAD_FLAG_ENQUEUED_WORK = 0x00000008u
};

class alignas(BL_CACHE_LINE_SIZE) InternalWorkerThread : public Thread {
public:
  struct WorkItem {
    ThreadFunc func;
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

  ThreadEntryFunc _entry_func;
  ThreadFunc _exit_func;
  void* _exit_data;
  void* _allocated_ptr;
  WorkItem _work_item;
  StatusInfo _status_data;

  BL_INLINE InternalWorkerThread(const WorkerThreadVirt* virt, ThreadEntryFunc entry_func, ThreadFunc exit_func, void* exit_data, void* allocated_ptr) noexcept
    : Thread{virt},
      _handle{},
      _entry_func(entry_func),
      _exit_func(exit_func),
      _exit_data(exit_data),
      _allocated_ptr(allocated_ptr),
      _work_item{},
      _status_data{} {}

  BL_INLINE ~InternalWorkerThread() noexcept {
#if _WIN32
    // The handle MUST be closed.
    if (_handle != 0)
      CloseHandle((HANDLE)_handle);
#endif
  }

  // Windows specific - it could happen that after returning from main() all threads are terminated even before
  // calling static destructors or DllMain(). This means that the thread could be already terminated and if we
  // have executed the regular code-path we would get stuck forever during the cleanup.
  BL_INLINE bool was_thread_terminated() const noexcept {
#if _WIN32
    DWORD result = WaitForSingleObject((HANDLE)_handle, 0);
    return result == WAIT_OBJECT_0;
#else
    return false;
#endif
  }
};

static uint32_t BL_CDECL bl_internal_worker_thread_status(const Thread* self) noexcept {
  const InternalWorkerThread* thread = static_cast<const InternalWorkerThread*>(self);
  uint32_t flags = bl::Atomic::fetch_relaxed(thread->_status_data.flags);

  if (flags & BL_WORKER_THREAD_FLAG_QUITTING) {
    return BL_THREAD_STATUS_QUITTING;
  }

  if (!(flags & BL_WORKER_THREAD_FLAG_SLEEPING)) {
    return BL_THREAD_STATUS_RUNNING;
  }

  return BL_THREAD_STATUS_IDLE;
}

// Thread - PortableWorkerThread
// =============================

static void BL_CDECL bl_portable_worker_thread_entry_point(Thread* self) noexcept;

class PortableWorkerThread : public InternalWorkerThread {
public:
  mutable Mutex _mutex;
  ConditionVariable _condition;

  BL_INLINE PortableWorkerThread (ThreadFunc exit_func, void* exit_data, void* allocated_ptr) noexcept
    : InternalWorkerThread(&portable_worker_thread_virt, bl_portable_worker_thread_entry_point, exit_func, exit_data, allocated_ptr),
      _mutex(),
      _condition() {}
};

static BLResult BL_CDECL bl_portable_worker_thread_destroy(Thread* self) noexcept {
  PortableWorkerThread* thread = static_cast<PortableWorkerThread*>(self);
  BL_ASSERT(thread != nullptr);

  void* allocated_ptr = thread->_allocated_ptr;
  thread->~PortableWorkerThread();

  axl::mem_free(allocated_ptr);
  return BL_SUCCESS;
}

static BLResult BL_CDECL bl_portable_worker_thread_run(Thread* self, ThreadFunc func, void* data) noexcept {
  PortableWorkerThread* thread = static_cast<PortableWorkerThread*>(self);

  LockGuard<Mutex> guard(thread->_mutex);
  uint32_t flags = thread->_status_data.flags;
  uint32_t kBusyFlags = BL_WORKER_THREAD_FLAG_ENQUEUING_WORK | BL_WORKER_THREAD_FLAG_ENQUEUED_WORK | BL_WORKER_THREAD_FLAG_QUITTING;

  if (flags & kBusyFlags)
    return BL_ERROR_BUSY;

  thread->_work_item.func = func;
  thread->_work_item.data = data;
  thread->_status_data.flags = flags | BL_WORKER_THREAD_FLAG_ENQUEUED_WORK;

  if (flags & BL_WORKER_THREAD_FLAG_SLEEPING) {
    thread->_condition.signal();
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL bl_portable_worker_thread_quit(Thread* self, uint32_t quit_flags) noexcept {
  PortableWorkerThread* thread = static_cast<PortableWorkerThread*>(self);

  if ((quit_flags & BL_THREAD_QUIT_ON_EXIT) && thread->was_thread_terminated()) {
    // The thread was already terminated by the runtime. Call '_exit_func'
    // manually so the object can be released properly from our side.
    thread->_exit_func(thread, thread->_exit_data);
    return BL_SUCCESS;
  }

  LockGuard<Mutex> guard(thread->_mutex);
  uint32_t flags = thread->_status_data.flags;

  thread->_status_data.flags = flags | BL_WORKER_THREAD_FLAG_QUITTING;

  if (flags & BL_WORKER_THREAD_FLAG_SLEEPING) {
    thread->_condition.signal();
  }

  return BL_SUCCESS;
}

static void BL_CDECL bl_portable_worker_thread_entry_point(Thread* self) noexcept {
  constexpr uint32_t kHasWorkOrQuittingFlags =
    BL_WORKER_THREAD_FLAG_QUITTING |
    BL_WORKER_THREAD_FLAG_ENQUEUED_WORK;

  BLRuntimeScopeCore rt_scope;
  bl_runtime_scope_begin(&rt_scope);

  PortableWorkerThread* thread = static_cast<PortableWorkerThread*>(self);

  for (;;) {
    LockGuard<Mutex> guard(thread->_mutex);

    // flags is used also as an accumulator - changes are accumulated first, and then stored before releasing the mutex.
    uint32_t flags = thread->_status_data.flags;

    for (;;) {
      if (flags & kHasWorkOrQuittingFlags)
        break;

      thread->_status_data.flags = flags | BL_WORKER_THREAD_FLAG_SLEEPING;
      thread->_condition.wait(thread->_mutex);

      flags = thread->_status_data.flags & ~BL_WORKER_THREAD_FLAG_SLEEPING;
    }

    bool has_enqueued_work = (flags & BL_WORKER_THREAD_FLAG_ENQUEUED_WORK) != 0u;
    InternalWorkerThread::WorkItem work_item = thread->_work_item;

    // Update flags now, before we release the mutex.
    flags &= ~BL_WORKER_THREAD_FLAG_ENQUEUED_WORK;
    thread->_status_data.flags = flags;

    // Doesn't matter if we are quitting or not, we have to execute the enqueued work.
    if (has_enqueued_work) {
      guard.release();
      work_item.func(thread, work_item.data);
    }

    if (flags & BL_WORKER_THREAD_FLAG_QUITTING)
      break;
  }

  bl_runtime_scope_end(&rt_scope);
  thread->_exit_func(thread, thread->_exit_data);
}

// Thread - FutexWorkerThread
// ==========================

static void BL_CDECL bl_futex_worker_thread_entry_point(Thread* self) noexcept;

class BLFutexWorkerThread : public InternalWorkerThread {
public:
  BL_INLINE BLFutexWorkerThread(ThreadFunc exit_func, void* exit_data, void* allocated_ptr) noexcept
    : InternalWorkerThread(&futex_worker_thread_virt, bl_futex_worker_thread_entry_point, exit_func, exit_data, allocated_ptr) {}
};

static BLResult BL_CDECL bl_futex_worker_thread_destroy(Thread* self) noexcept {
  BLFutexWorkerThread* thread = static_cast<BLFutexWorkerThread*>(self);
  BL_ASSERT(thread != nullptr);

  void* allocated_ptr = thread->_allocated_ptr;
  thread->~BLFutexWorkerThread();

  axl::mem_free(allocated_ptr);
  return BL_SUCCESS;
}

static BL_INLINE void bl_futex_worker_thread_wake_up(BLFutexWorkerThread* thread) noexcept {
  bl::Futex::wake_one(&thread->_status_data.flags);
}

static BLResult BL_CDECL bl_futex_worker_thread_run(Thread* self, ThreadFunc work_func, void* data) noexcept {
  BLFutexWorkerThread* thread = static_cast<BLFutexWorkerThread*>(self);

  // We want to enqueue work atomically here. For that purpose we have two status flags:
  //
  //   - BL_WORKER_THREAD_FLAG_ENQUEUING_WORK - work is being enqueued.
  //   - BL_WORKER_THREAD_FLAG_ENQUEUED_WORK - work has been enqueued.
  //
  // We just want to OR `ENQUEUING` flag here and if we encounter that another thread was faster enqueuing we just
  // return `BL_ERROR_BUSY`. It does no harm when both `ENQUEUING` and `ENQUEUED` flags are set as when the work is
  // picked both flags would be cleared.

  uint32_t prev_flags = bl::Atomic::fetch_or_acq_rel(thread->_status_data.flags, uint32_t(BL_WORKER_THREAD_FLAG_ENQUEUING_WORK));
  uint32_t kBusyFlags = BL_WORKER_THREAD_FLAG_ENQUEUING_WORK | BL_WORKER_THREAD_FLAG_ENQUEUED_WORK | BL_WORKER_THREAD_FLAG_QUITTING;

  if (prev_flags & kBusyFlags) {
    return BL_ERROR_BUSY;
  }

  bl::Atomic::store_relaxed(thread->_work_item.func, work_func);
  bl::Atomic::store_relaxed(thread->_work_item.data, data);

  // Finally, this would make the work item available for pick up.
  prev_flags = bl::Atomic::fetch_or_seq_cst(thread->_status_data.flags, uint32_t(BL_WORKER_THREAD_FLAG_ENQUEUED_WORK));

  // Wake up the thread if it is waiting.
  if (prev_flags & BL_WORKER_THREAD_FLAG_SLEEPING) {
    bl_futex_worker_thread_wake_up(thread);
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL bl_futex_worker_thread_quit(Thread* self, uint32_t quit_flags) noexcept {
  BLFutexWorkerThread* thread = static_cast<BLFutexWorkerThread*>(self);

  if ((quit_flags & BL_THREAD_QUIT_ON_EXIT) && thread->was_thread_terminated()) {
    // The thread was already terminated by the runtime. Call '_exit_func'
    // manually so the object can be released properly from our side.
    thread->_exit_func(thread, thread->_exit_data);
    return BL_SUCCESS;
  }

  uint32_t prev_flags = bl::Atomic::fetch_or_acq_rel(thread->_status_data.flags, uint32_t(BL_WORKER_THREAD_FLAG_QUITTING));

  // If already quitting it makes no sense to even wake it up as it already knows.
  if (prev_flags & BL_WORKER_THREAD_FLAG_QUITTING) {
    return BL_SUCCESS;
  }

  // Wake up the thread if it is waiting.
  if (prev_flags & BL_WORKER_THREAD_FLAG_SLEEPING) {
    bl_futex_worker_thread_wake_up(thread);
  }

  return BL_SUCCESS;
}

static void BL_CDECL bl_futex_worker_thread_entry_point(Thread* self) noexcept {
  BLRuntimeScopeCore rt_scope;
  bl_runtime_scope_begin(&rt_scope);

  BLFutexWorkerThread* thread = static_cast<BLFutexWorkerThread*>(self);

  uint32_t spin_count = 0;
  uint32_t kSpinLimit = 32;

  for (;;) {
    uint32_t flags = bl::Atomic::fetch_and_seq_cst(thread->_status_data.flags, uint32_t(~BL_WORKER_THREAD_FLAG_SLEEPING));

    if (flags & BL_WORKER_THREAD_FLAG_ENQUEUED_WORK) {
      ThreadFunc work_func = bl::Atomic::fetch_relaxed(thread->_work_item.func);
      void* work_data = bl::Atomic::fetch_relaxed(thread->_work_item.data);

      constexpr uint32_t kEnqueuingOrEnqueued = BL_WORKER_THREAD_FLAG_ENQUEUING_WORK | BL_WORKER_THREAD_FLAG_ENQUEUED_WORK;
      bl::Atomic::fetch_and_seq_cst(thread->_status_data.flags, ~kEnqueuingOrEnqueued);

      spin_count = 0;
      work_func(thread, work_data);
      continue;
    }

    if (flags & BL_WORKER_THREAD_FLAG_QUITTING) {
      break;
    }

    // If another thread is enqueuing work at the moment, spin for a little
    // time to either pick it up immediately or before going to wait.
    if (flags & BL_WORKER_THREAD_FLAG_ENQUEUING_WORK && ++spin_count < kSpinLimit) {
      continue;
    }

    // Let's wait for more work or a quit signal.
    spin_count = 0;
    flags = bl::Atomic::fetch_or_acq_rel(thread->_status_data.flags, uint32_t(BL_WORKER_THREAD_FLAG_SLEEPING));

    // Last attempt to avoid waiting...
    if (flags & (BL_WORKER_THREAD_FLAG_ENQUEUED_WORK | BL_WORKER_THREAD_FLAG_QUITTING)) {
      continue;
    }

    bl::Futex::wait(&thread->_status_data.flags, flags | BL_WORKER_THREAD_FLAG_SLEEPING);
  }

  bl_runtime_scope_end(&rt_scope);
  thread->_exit_func(thread, thread->_exit_data);
}

// Thread - WorkerThread - API
// ===========================

static InternalWorkerThread* bl_thread_new(ThreadFunc exit_func, void* exit_data) noexcept {
  uint32_t alignment = BL_CACHE_LINE_SIZE;
  uint32_t futex_enabled = BL_FUTEX_ENABLED;

  size_t impl_size = futex_enabled ? sizeof(BLFutexWorkerThread)
                                   : sizeof(PortableWorkerThread);

  void* allocated_ptr = axl::mem_alloc(impl_size + alignment);
  if (BL_UNLIKELY(!allocated_ptr)) {
    return nullptr;
  }

  void* aligned_ptr = bl::axl::align_up(allocated_ptr, alignment);
  if (futex_enabled)
    return new(bl::Internal::PlacementNew{aligned_ptr}) BLFutexWorkerThread(exit_func, exit_data, allocated_ptr);
  else
    return new(bl::Internal::PlacementNew{aligned_ptr}) PortableWorkerThread(exit_func, exit_data, allocated_ptr);
}

#ifdef _WIN32

// Thread - Windows Implementation
// ===============================

static unsigned BL_STDCALL bl_thread_entry_point(void* arg) noexcept {
  InternalWorkerThread* thread = static_cast<InternalWorkerThread*>(arg);
  thread->_entry_func(thread);
  return 0;
}

BLResult BL_CDECL bl_thread_create(Thread** thread_out, const ThreadAttributes* attributes, ThreadFunc exit_func, void* exit_data) noexcept {
  InternalWorkerThread* thread = bl_thread_new(exit_func, exit_data);
  if (BL_UNLIKELY(!thread))
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);

  BLResult result = BL_SUCCESS;
  uint32_t flags = 0;
  uint32_t stack_size = attributes->stack_size;

  if (stack_size > 0)
    flags = STACK_SIZE_PARAM_IS_A_RESERVATION;

  HANDLE handle = (HANDLE)_beginthreadex(nullptr, stack_size, bl_thread_entry_point, thread, flags, nullptr);
  if (handle == (HANDLE)-1)
    result = BL_ERROR_BUSY;
  else
    thread->_handle = (intptr_t)handle;

  if (result == BL_SUCCESS) {
    *thread_out = thread;
    return BL_SUCCESS;
  }
  else {
    thread->~InternalWorkerThread();
    axl::mem_free(thread);

    *thread_out = nullptr;
    return result;
  }
}

#else

// Thread - POSIX Implementation
// =============================

static std::atomic<size_t> bl_thread_minimum_probed_stack_size;

static void* bl_thread_entry_point(void* arg) noexcept {
  InternalWorkerThread* thread = static_cast<InternalWorkerThread*>(arg);
  thread->_entry_func(thread);
  return nullptr;
}

BLResult BL_CDECL bl_thread_create(Thread** thread_out, const ThreadAttributes* attributes, ThreadFunc exit_func, void* exit_data) noexcept {
  size_t default_stack_size = 0;
  size_t current_stack_size = attributes->stack_size;
  size_t minimum_probed_stack_size = bl_thread_minimum_probed_stack_size.load(std::memory_order::relaxed);

  if (current_stack_size)
    current_stack_size = bl_max<size_t>(current_stack_size, minimum_probed_stack_size);

  pthread_attr_t pt_attr;
  int err = pthread_attr_init(&pt_attr);
  if (err)
    return bl_result_from_posix_error(err);

  // We bail to the default stack-size if we are not able to probe a small workable stack-size. 8MB is a safe guess.
  err = pthread_attr_getstacksize(&pt_attr, &default_stack_size);
  if (err)
    default_stack_size = 1024u * 1024u * 8u;

  // This should never fail, but...
  err = pthread_attr_setdetachstate(&pt_attr, PTHREAD_CREATE_DETACHED);
  if (err) {
    err = pthread_attr_destroy(&pt_attr);
    return bl_result_from_posix_error(err);
  }

  InternalWorkerThread* thread = bl_thread_new(exit_func, exit_data);
  if (BL_UNLIKELY(!thread)) {
    pthread_attr_destroy(&pt_attr);
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  // Probe loop - Since some implementations fail to create a thread with small stack-size, we would probe a safe value
  // in this case and use it the next time we want to create a thread as a minimum so we don't have to probe it again.
  uint32_t probe_count = 0;
  for (;;) {
    if (current_stack_size)
      pthread_attr_setstacksize(&pt_attr, current_stack_size);

    err = pthread_create(&thread->_handle, &pt_attr, bl_thread_entry_point, thread);
    bool done = !err || !current_stack_size || current_stack_size >= default_stack_size;

    if (done) {
      pthread_attr_destroy(&pt_attr);

      if (!err) {
        if (probe_count) {
          bl_thread_minimum_probed_stack_size.store(current_stack_size, std::memory_order::relaxed);
        }

        *thread_out = thread;
        return BL_SUCCESS;
      }
      else {
        thread->destroy();
        *thread_out = nullptr;
        return bl_result_from_posix_error(err);
      }
    }

    current_stack_size <<= 1;
    probe_count++;
  }
}
#endif

} // {bl}

// Thread - Runtime Registration
// =============================

void bl_thread_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  using namespace bl;

  // BLFutexWorkerThread virtual table.
  futex_worker_thread_virt.destroy = bl_futex_worker_thread_destroy;
  futex_worker_thread_virt.status = bl_internal_worker_thread_status;
  futex_worker_thread_virt.run = bl_futex_worker_thread_run;
  futex_worker_thread_virt.quit = bl_futex_worker_thread_quit;

  // PortableWorkerThread virtual table.
  portable_worker_thread_virt.destroy = bl_portable_worker_thread_destroy;
  portable_worker_thread_virt.status = bl_internal_worker_thread_status;
  portable_worker_thread_virt.run = bl_portable_worker_thread_run;
  portable_worker_thread_virt.quit = bl_portable_worker_thread_quit;
}
