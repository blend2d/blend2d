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

#ifndef BLEND2D_THREADING_THREAD_P_H_INCLUDED
#define BLEND2D_THREADING_THREAD_P_H_INCLUDED

#include "../api-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

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

//! \}
//! \endcond

#endif // BLEND2D_THREADING_THREAD_P_H_INCLUDED
