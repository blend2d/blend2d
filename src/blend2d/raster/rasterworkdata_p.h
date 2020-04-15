// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_RASTERWORKDATA_P_H
#define BLEND2D_RASTER_RASTERWORKDATA_P_H

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
class BLRasterWorkBatch;

// ============================================================================
// [BLRasterWorkData]
// ============================================================================

//! Provides data used by both single-threaded and multi-threaded render command
//! processing. Single-threaded rendering context uses this data synchronously
//! to process commands that are required before using pipelines. Multi-threaded
//! rendering context uses 1 + N WorkData instances, where the first one can be
//! used synchronously by the rendering context to perform synchronous tasks
//! while the remaining WorkData is used per worker thread.
class BLRasterWorkData {
public:
  BL_NONCOPYABLE(BLRasterWorkData)

  enum : uint32_t { kSyncWorkerId = 0xFFFFFFFFu };

  //! Rendering context impl.
  BLRasterContextImpl* ctxI;
  //! Batch data to process in case this data is used in a worker thread.
  BLRasterWorkBatch* batch;
  //! Context data used by pipelines (either the destination data or layer).
  BLPipeContextData ctxData;

  //! Clip mode.
  uint8_t clipMode;
  //! Reserved.
  uint8_t reserved[3];
  //! Id of the worker that uses this WorkData.
  uint32_t _workerId;
  //! Band height.
  uint32_t _bandHeight;
  //! Accumulated error flags.
  uint32_t _accumulatedErrorFlags;

  //! Temporary paths.
  BLPath tmpPath[4];
  //! Temporary glyph buffer used by high-level text rendering calls.
  BLGlyphBuffer glyphBuffer;

  //! Zone memory used by the worker context.
  BLZoneAllocator workZone;
  //! The last state of the zone to be reverted to in case of failure.
  BLZoneAllocator::StatePtr workState;
  //! Zero memory filled by rasterizers and zeroed back by pipelines.
  BLZeroBuffer zeroBuffer;
  //! Edge storage.
  BLEdgeStorage<int> edgeStorage;
  //! Edge builder.
  BLEdgeBuilder<int> edgeBuilder;

  explicit BLRasterWorkData(BLRasterContextImpl* ctxI, uint32_t workerId = kSyncWorkerId) noexcept;
  ~BLRasterWorkData() noexcept;

  // NOTE: `initContextData()` is called after `initBandData()` in `blRasterContextImplAttach()`.

  BL_INLINE void initContextData(const BLImageData& dstData) noexcept { ctxData.dst = dstData; }
  BLResult initBandData(uint32_t bandHeight, uint32_t bandCount) noexcept;

  BL_INLINE bool isSync() const noexcept { return _workerId == kSyncWorkerId; }

  BL_INLINE const BLSizeI& dstSize() const noexcept { return ctxData.dst.size; }
  BL_INLINE uint32_t workerId() const noexcept { return _workerId; }
  BL_INLINE uint32_t bandHeight() const noexcept { return _bandHeight; }
  BL_INLINE uint32_t bandCount() const noexcept { return edgeStorage.bandCount(); }

  BL_INLINE uint32_t accumulatedErrorFlags() const noexcept { return _accumulatedErrorFlags; }
  BL_INLINE void cleanAccumulatedErrorFlags() noexcept { _accumulatedErrorFlags = 0; }

  BL_INLINE void startOver() noexcept {
    workZone.clear();
  }

  BL_INLINE void saveState() noexcept {
    workState = workZone.saveState();
  }

  BL_INLINE void revertEdgeBuilder() noexcept {
    edgeBuilder.mergeBoundingBox();
    edgeStorage.clear();
    workZone.restoreState(workState);
  }

  //! Accumulates the error result into error flags of this work-data. Used by
  //! both synchronous and asynchronous rendering context to accumulate errors
  //! that may happen during the rendering.
  BLResult accumulateError(BLResult error) noexcept;
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERWORKDATA_P_H
