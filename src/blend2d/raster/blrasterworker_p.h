// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_BLRASTERWORKER_P_H
#define BLEND2D_RASTER_BLRASTERWORKER_P_H

#include "../blgeometry_p.h"
#include "../blimage.h"
#include "../blpath.h"
#include "../blregion.h"
#include "../blzeroallocator_p.h"
#include "../blzoneallocator_p.h"
#include "../pipegen/blpiperuntime_p.h"
#include "../raster/bledgebuilder_p.h"
#include "../raster/blrasterdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct BLRasterContextImpl;

// ============================================================================
// [BLRasterWorker]
// ============================================================================

class BLRasterWorker {
public:
  BL_NONCOPYABLE(BLRasterWorker)

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

  //! Zone memory used by the worker.
  BLZoneAllocator workerZone;
  //! Zero memory exclusively used by analytic rasterizer.
  BLZeroBuffer zeroBuffer;
  //! Edge storage.
  BLEdgeStorage<int> edgeStorage;
  //! Edge builder.
  BLEdgeBuilder<int> edgeBuilder;

  BLRasterWorker(BLRasterContextImpl* ctxI) noexcept;
  ~BLRasterWorker() noexcept;

  BLResult initEdgeStorage(int height) noexcept;

  BL_INLINE void initFullAlpha(uint32_t val) noexcept { fullAlpha = val; }
  BL_INLINE void initContextDataByDstData() noexcept { ctxData.dst = dstData; }
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_BLRASTERWORKER_P_H
