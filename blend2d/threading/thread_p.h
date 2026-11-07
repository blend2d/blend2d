// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_THREADING_THREAD_P_H_INCLUDED
#define BLEND2D_THREADING_THREAD_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>

#if defined(BL_TARGET_OPT_SSE2)
  #include <emmintrin.h> // for _mm_pause().
#endif

namespace bl {

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

struct Thread;
struct WorkerThreadVirt;
struct ThreadAttributes;

#if defined(BL_TARGET_OPT_SSE2)
static BL_INLINE void bl_thread_pause() noexcept { _mm_pause(); }
#else
static BL_INLINE void bl_thread_pause() noexcept {}
#endif

typedef void (BL_CDECL* ThreadFunc)(Thread* thread, void* data) noexcept;

enum ThreadStatus : uint32_t {
  BL_THREAD_STATUS_IDLE = 0,
  BL_THREAD_STATUS_RUNNING = 1,
  BL_THREAD_STATUS_QUITTING = 2
};

enum ThreadQuitFlags : uint32_t {
  BL_THREAD_QUIT_ON_EXIT = 0x00000001u
};

struct ThreadAttributes {
  uint32_t stack_size;
};

struct WorkerThreadVirt {
  BLResult (BL_CDECL* destroy)(Thread* self) noexcept;
  uint32_t (BL_CDECL* status)(const Thread* self) noexcept;
  BLResult (BL_CDECL* run)(Thread* self, ThreadFunc work_func, void* data) noexcept;
  BLResult (BL_CDECL* quit)(Thread* self, uint32_t quit_flags) noexcept;
};

struct Thread {
  const WorkerThreadVirt* virt;

#ifdef __cplusplus
  BL_INLINE BLResult destroy() noexcept {
    return virt->destroy(this);
  }

  BL_INLINE uint32_t status() const noexcept {
    return virt->status(this);
  }

  BL_INLINE BLResult run(ThreadFunc work_func, void* data) noexcept {
    return virt->run(this, work_func, data);
  }

  BL_INLINE BLResult quit(uint32_t quit_flags = 0) noexcept {
    return virt->quit(this, quit_flags);
  }
#endif
};

BL_HIDDEN BLResult BL_CDECL bl_thread_create(Thread** thread_out, const ThreadAttributes* attributes, ThreadFunc exit_func, void* exit_data) noexcept;

//! \}
//! \endcond

} // {bl}

#endif // BLEND2D_THREADING_THREAD_P_H_INCLUDED
