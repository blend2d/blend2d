// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_THREADING_THREAD_P_H_INCLUDED
#define BLEND2D_THREADING_THREAD_P_H_INCLUDED

#include "../api-internal_p.h"

#if defined(BL_TARGET_OPT_SSE2)
  #include <emmintrin.h> // for _mm_pause().
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

struct BLThread;
struct BLWorkerThreadVirt;
struct BLThreadAttributes;

#if defined(BL_TARGET_OPT_SSE2)
static BL_INLINE void blThreadPause() noexcept { _mm_pause(); }
#else
static BL_INLINE void blThreadPause() noexcept {}
#endif

typedef void (BL_CDECL* BLThreadFunc)(BLThread* thread, void* data) BL_NOEXCEPT;

enum BLThreadStatus : uint32_t {
  BL_THREAD_STATUS_IDLE = 0,
  BL_THREAD_STATUS_RUNNING = 1,
  BL_THREAD_STATUS_QUITTING = 2
};

enum BLThreadQuitFlags : uint32_t {
  BL_THREAD_QUIT_ON_EXIT = 0x00000001u
};

struct BLThreadAttributes {
  uint32_t stackSize;
};

struct BLWorkerThreadVirt {
  BLResult (BL_CDECL* destroy)(BLThread* self) BL_NOEXCEPT;
  uint32_t (BL_CDECL* status)(const BLThread* self) BL_NOEXCEPT;
  BLResult (BL_CDECL* run)(BLThread* self, BLThreadFunc workFunc, void* data) BL_NOEXCEPT;
  BLResult (BL_CDECL* quit)(BLThread* self, uint32_t quitFlags) BL_NOEXCEPT;
};

struct BLThread {
  const BLWorkerThreadVirt* virt;

#ifdef __cplusplus
  BL_INLINE BLResult destroy() noexcept {
    return virt->destroy(this);
  }

  BL_INLINE uint32_t status() const noexcept {
    return virt->status(this);
  }

  BL_INLINE BLResult run(BLThreadFunc workFunc, void* data) noexcept {
    return virt->run(this, workFunc, data);
  }

  BL_INLINE BLResult quit(uint32_t quitFlags = 0) noexcept {
    return virt->quit(this, quitFlags);
  }
#endif
};

BL_HIDDEN BLResult BL_CDECL blThreadCreate(BLThread** threadOut, const BLThreadAttributes* attributes, BLThreadFunc exitFunc, void* exitData) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_THREADING_THREAD_P_H_INCLUDED
