// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_THREADING_THREAD_P_H
#define BLEND2D_THREADING_THREAD_P_H

#include "../api-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

class BLThreadEvent;

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
// [Utilities]
// ============================================================================

#ifdef _WIN32
static BL_INLINE void blThreadYield() noexcept { Sleep(0); }
#else
static BL_INLINE void blThreadYield() noexcept { sched_yield(); }
#endif

// ============================================================================
// [BLThreadEvent]
// ============================================================================

BL_HIDDEN BLResult BL_CDECL blThreadEventCreate(BLThreadEvent* self, bool manualReset, bool signaled) noexcept;
BL_HIDDEN BLResult BL_CDECL blThreadEventDestroy(BLThreadEvent* self) noexcept;
BL_HIDDEN bool     BL_CDECL blThreadEventIsSignaled(const BLThreadEvent* self) noexcept;
BL_HIDDEN BLResult BL_CDECL blThreadEventSignal(BLThreadEvent* self) noexcept;
BL_HIDDEN BLResult BL_CDECL blThreadEventReset(BLThreadEvent* self) noexcept;
BL_HIDDEN BLResult BL_CDECL blThreadEventWait(BLThreadEvent* self) noexcept;
BL_HIDDEN BLResult BL_CDECL blThreadEventTimedWait(BLThreadEvent* self, uint64_t microseconds) noexcept;

class BLThreadEvent {
public:
  BL_NONCOPYABLE(BLThreadEvent)

  intptr_t handle;

  BL_INLINE explicit BLThreadEvent(bool manualReset = false, bool signaled = false) noexcept {
    blThreadEventCreate(this, manualReset, signaled);
  }
  BL_INLINE ~BLThreadEvent() noexcept { blThreadEventDestroy(this); }

  BL_INLINE bool isInitialized() const noexcept { return handle != -1; }
  BL_INLINE bool isSignaled() const noexcept { return blThreadEventIsSignaled(this); }

  BL_INLINE BLResult signal() noexcept { return blThreadEventSignal(this); }
  BL_INLINE BLResult reset() noexcept { return blThreadEventReset(this); }
  BL_INLINE BLResult wait() noexcept { return blThreadEventWait(this); }
  BL_INLINE BLResult waitFor(uint64_t microseconds) noexcept { return blThreadEventTimedWait(this, microseconds); }
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

#ifndef _WIN32
BL_HIDDEN BLResult blThreadCreatePt(BLThread** threadOut, const pthread_attr_t* ptAttr, BLThreadFunc exitFunc, void* exitData) noexcept;
BL_HIDDEN BLResult blThreadSetPtAttributes(pthread_attr_t* ptAttr, const BLThreadAttributes* src) noexcept;
#endif

//! \}
//! \endcond

#endif // BLEND2D_THREADING_THREAD_P_H
