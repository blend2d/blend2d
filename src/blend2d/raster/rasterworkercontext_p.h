// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_RASTERWORKERCONTEXT_P_H
#define BLEND2D_RASTER_RASTERWORKERCONTEXT_P_H

#include "../geometry_p.h"
#include "../image.h"
#include "../path.h"
#include "../region.h"
#include "../zeroallocator_p.h"
#include "../zoneallocator_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rasterdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

class BLRasterContextImpl;

// ============================================================================
// [BLRasterWorkerContext]
// ============================================================================

//! Provides abstractions useful for both single-threaded and multi-threaded
//! rendering. Single-threaded rendering context has only a single worker that
//! is used synchronously to perform operations that are required before using
//! pipelines. Multi-threaded rendering context uses 1 + N workers, where the
//! first worker can be used synchronously by the rendering context and other
//! workers are used by worker threads.
class BLRasterWorkerContext {
public:
  BL_NONCOPYABLE(BLRasterWorkerContext)

  //! Rendering context impl.
  BLRasterContextImpl* ctxI;
  //! Context data.
  BLPipeContextData ctxData;

  //! Clip mode.
  uint8_t clipMode;
  //! Reserved.
  uint8_t reserved[3];
  //! Full alpha value (256 or 65536).
  uint32_t fullAlpha;

  //! Destination image data.
  BLImageData dstData;
  //! Temporary paths.
  BLPath tmpPath[4];

  //! Zone memory used by the work context.
  BLZoneAllocator workZone;
  //! Zero memory used exclusively by rasterizers.
  BLZeroBuffer zeroBuffer;
  //! Edge storage.
  BLEdgeStorage<int> edgeStorage;
  //! Edge builder.
  BLEdgeBuilder<int> edgeBuilder;

  BLRasterWorkerContext(BLRasterContextImpl* ctxI) noexcept;
  ~BLRasterWorkerContext() noexcept;

  BLResult initEdgeStorage(uint32_t height) noexcept;

  BL_INLINE void initFullAlpha(uint32_t val) noexcept { fullAlpha = val; }
  BL_INLINE void initContextDataByDstData() noexcept { ctxData.dst = dstData; }
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERWORKERCONTEXT_P_H
