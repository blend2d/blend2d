// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_RASTERWORKERMANAGER_P_H
#define BLEND2D_RASTER_RASTERWORKERMANAGER_P_H

#include "../geometry_p.h"
#include "../image.h"
#include "../path.h"
#include "../region.h"
#include "../zeroallocator_p.h"
#include "../zoneallocator_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/rasterworkercontext_p.h"
#include "../threading/thread_p.h"
#include "../threading/threadpool_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

class BLRasterContextImpl;

// ============================================================================
// [BLRasterWorkerManager]
// ============================================================================

class BLRasterWorkerManager {
  BL_NONCOPYABLE(BLRasterWorkerManager)

public:
  BLThreadPool* _threadPool;
  BLThread** _workerThreads;
  BLRasterWorkerContext** _workerContexts;
  uint32_t _threadCount;

  BL_INLINE BLRasterWorkerManager() noexcept
    : _threadPool(nullptr),
      _workerThreads(nullptr),
      _workerContexts(nullptr),
      _threadCount(0) {}

  BL_INLINE ~BLRasterWorkerManager() noexcept {
    // Cannot be initialized upon destruction!
    BL_ASSERT(!initialized());
  }

  //! Returns `true` when the thread manager is active (has threads acquired).
  BL_INLINE bool initialized() const noexcept { return _threadCount != 0; }

  //! Initializes ther thread manager with the specified number of threads.
  BLResult init(BLRasterContextImpl* ctxI, uint32_t initFlags, uint32_t threadCount) noexcept;

  //! Releases all acquired threads and destroys all work context.
  //!
  //! \note It's only safe to call `reset()` after all threads have finalized
  //! their work. It would be disaster to call `reset()` when one or more thread
  //! is still running as reset destroys all work contexts, so the threads would
  //! be using freed memory.
  void reset() noexcept;
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERWORKERMANAGER_P_H
