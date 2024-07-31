// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_WORKDATA_P_H_INCLUDED
#define BLEND2D_RASTER_WORKDATA_P_H_INCLUDED

#include "../geometry_p.h"
#include "../image.h"
#include "../path.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rasterdefs_p.h"
#include "../support/arenaallocator_p.h"
#include "../support/zeroallocator_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace bl {
namespace RasterEngine {

class RenderBatch;
class WorkerSynchronization;

//! Provides data used by both single-threaded and multi-threaded render command processing. Single-threaded rendering
//! context uses this data synchronously to process commands that are required before using pipelines. Multi-threaded
//! rendering context uses 1 + N WorkData instances, where the first one can be used synchronously by the rendering
//! context to perform synchronous tasks while the remaining WorkData is used per worker thread.
class WorkData {
public:
  BL_NONCOPYABLE(WorkData)

  enum : uint32_t { kSyncWorkerId = 0u };
  enum : size_t { kEdgeListSize = sizeof(EdgeList<int>) };

  //! Rendering context impl.
  BLRasterContextImpl* ctxI {};
  //! Worker synchronization.
  WorkerSynchronization* synchronization {};
  //! Batch data to process in case this data is used in a worker thread.
  RenderBatch* _batch {};
  //! Context data used by pipelines (either the destination data or layer).
  Pipeline::ContextData ctxData {};

  //! Clip mode.
  uint8_t clipMode {};
  //! Quantization shift of vertical coordinates - used to store quantized coordinates in command queue (aligned coordinates).
  uint8_t _commandQuantizationShiftAA;
  //! Quantization shift of vertical coordinates - used to store quantized coordinates in command queue (fractional coordinates).
  uint8_t _commandQuantizationShiftFp;
  //! Reserved.
  uint8_t reserved[2] {};
  //! Id of the worker that uses this WorkData.
  uint32_t _workerId {};
  //! Band height.
  uint32_t _bandHeight {};
  //! Accumulated error flags.
  uint32_t _accumulatedErrorFlags {};

  //! Temporary paths.
  BLPath tmpPath[4];
  //! Temporary glyph buffer used by high-level text rendering calls.
  BLGlyphBuffer glyphBuffer;

  //! Zone memory used by the worker context.
  ArenaAllocator workZone;
  //! The last state of the zone to be reverted to in case of failure.
  ArenaAllocator::StatePtr workState {};
  //! Zero memory filled by rasterizers and zeroed back by pipelines.
  ZeroBuffer zeroBuffer;
  //! Edge storage.
  EdgeStorage<int> edgeStorage;
  //! Edge builder.
  EdgeBuilder<int> edgeBuilder;

  explicit WorkData(BLRasterContextImpl* ctxI, WorkerSynchronization* synchronization, uint32_t workerId = kSyncWorkerId) noexcept;
  ~WorkData() noexcept;

  // NOTE: `initContextData()` is called after `initBandData()` in `blRasterContextImplAttach()`.

  BL_INLINE void initBatch(RenderBatch* batch) noexcept { blAtomicStoreStrong(&_batch, batch); }
  BL_INLINE void resetBatch() noexcept { initBatch(nullptr); }
  BL_INLINE RenderBatch* acquireBatch() noexcept { return blAtomicFetchStrong(&_batch); }

  BL_INLINE void initContextData(const BLImageData& dstData, const BLPointI& pixelOrigin) noexcept {
    ctxData.dst = dstData;
    ctxData.pixelOrigin = pixelOrigin;
  }

  BLResult initBandData(uint32_t bandHeight, uint32_t bandCount, uint32_t commandQuantizationShift) noexcept;

  BL_INLINE_NODEBUG bool isSync() const noexcept { return _workerId == kSyncWorkerId; }
  BL_INLINE_NODEBUG const BLSizeI& dstSize() const noexcept { return ctxData.dst.size; }
  BL_INLINE_NODEBUG uint32_t workerId() const noexcept { return _workerId; }
  BL_INLINE_NODEBUG uint32_t bandHeight() const noexcept { return _bandHeight; }
  BL_INLINE_NODEBUG uint32_t bandHeightFixed() const noexcept { return _bandHeight << 8; }
  BL_INLINE_NODEBUG uint32_t bandCount() const noexcept { return edgeStorage.bandCount(); }

  BL_INLINE_NODEBUG uint32_t commandQuantizationShiftAA() const noexcept { return _commandQuantizationShiftAA; }
  BL_INLINE_NODEBUG uint32_t commandQuantizationShiftFp() const noexcept { return _commandQuantizationShiftFp; }

  BL_INLINE_NODEBUG BLContextErrorFlags accumulatedErrorFlags() const noexcept { return BLContextErrorFlags(_accumulatedErrorFlags); }

  BL_INLINE_NODEBUG void accumulateErrorFlag(BLContextErrorFlags flag) noexcept { _accumulatedErrorFlags |= uint32_t(flag); }
  BL_INLINE_NODEBUG void cleanAccumulatedErrorFlags() noexcept { _accumulatedErrorFlags = 0; }

  BL_INLINE void avoidCacheLineSharing() noexcept {
    workZone.align(BL_CACHE_LINE_SIZE);
  }

  BL_INLINE void startOver() noexcept {
    workZone.clear();
    workState = ArenaAllocator::StatePtr{};
    edgeStorage.clear();
  }

  BL_INLINE void saveState() noexcept {
    workState = workZone.saveState();
  }

  BL_INLINE void restoreState() noexcept {
    workZone.restoreState(workState);
  }

  BL_INLINE void revertEdgeBuilder() noexcept {
    edgeBuilder.mergeBoundingBox();
    edgeStorage.clear();
    workZone.restoreState(workState);
  }

  //! Accumulates the error result into error flags of this work-data. Used by both synchronous and asynchronous
  //! rendering context to accumulate errors that may happen during the rendering.
  BLResult accumulateError(BLResult error) noexcept;
};

} // {RasterEngine}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_WORKDATA_P_H_INCLUDED
