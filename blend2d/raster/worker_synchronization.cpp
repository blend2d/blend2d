// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#include <blend2d/raster/worker_synchronization_p.h>
#include <blend2d/threading/futex_p.h>

namespace bl::RasterEngine {

WorkerSynchronization::WorkerSynchronization() noexcept
  : _header{},
    _status{} {

  _header.use_futex = BL_FUTEX_ENABLED;

  if (!use_futex()) {
    bl_call_ctor(_portable_data);
  }
}

WorkerSynchronization::~WorkerSynchronization() noexcept {
  if (!use_futex()) {
    bl_call_dtor(_portable_data);
  }
}

void WorkerSynchronization::wait_for_jobs_to_finish() noexcept {
  if (use_futex()) {
    if (Atomic::fetch_sub_acq_rel(_status.jobs_running_count) == 1) {
      Atomic::fetch_add_acq_rel(_status.futex_jobs_finished);
      Futex::wake_all(&_status.futex_jobs_finished);
    }
    else {
      do {
        Futex::wait(&_status.futex_jobs_finished, 0u);
      } while (Atomic::fetch_strong(_status.futex_jobs_finished) != 1);
    }
  }
  else {
    LockGuard<Mutex> guard(_portable_data.mutex);
    if (--_status.jobs_running_count == 0) {
      guard.release();
      _portable_data.jobs_condition.broadcast();
    }
    else {
      while (_status.jobs_running_count) {
        _portable_data.jobs_condition.wait(_portable_data.mutex);
      }
    }
  }
}

void WorkerSynchronization::thread_done() noexcept {
  uint32_t remaining_plus_one = Atomic::fetch_sub_acq_rel(_status.threads_running_count);

  if (remaining_plus_one != 1)
    return;

  if (use_futex()) {
    Atomic::fetch_add_acq_rel(_status.futex_bands_finished);
    Futex::wake_one(&_status.futex_bands_finished);
  }
  else {
    LockGuard<Mutex> guard(_portable_data.mutex);
    if (_status.waiting_for_completion) {
      _portable_data.done_condition.signal();
    }
  }
}

void WorkerSynchronization::wait_for_threads_to_finish() noexcept {
  if (use_futex()) {
    for (;;) {
      uint32_t finished = Atomic::fetch_strong(_status.futex_bands_finished);
      if (finished)
        break;
      Futex::wait(&_status.futex_bands_finished, 0);
    }
    Atomic::store_relaxed(_status.futex_bands_finished, 0u);
  }
  else {
    LockGuard<Mutex> guard(_portable_data.mutex);
    if (Atomic::fetch_strong(_status.threads_running_count) > 0) {
      _status.waiting_for_completion = true;
      while (Atomic::fetch_strong(_status.threads_running_count) > 0) {
        _portable_data.done_condition.wait(_portable_data.mutex);
      }
      _status.waiting_for_completion = false;
    }
  }
}

} // {bl::RasterEngine}
