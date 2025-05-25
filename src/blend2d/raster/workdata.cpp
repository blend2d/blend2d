// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../raster/rastercontext_p.h"
#include "../raster/workdata_p.h"

namespace bl::RasterEngine {

// bl::RasterEngine::WorkData - Construction & Destruction
// =======================================================

WorkData::WorkData(BLRasterContextImpl* ctxI, WorkerSynchronization* synchronization, uint32_t workerId) noexcept
  : ctxI(ctxI),
    synchronization(synchronization),
    _batch(nullptr),
    ctxData(),
    clipMode(BL_CLIP_MODE_ALIGNED_RECT),
    _commandQuantizationShiftAA(0),
    _commandQuantizationShiftFp(0),
    reserved{},
    _workerId(workerId),
    _bandHeight(0),
    _accumulatedErrorFlags(0),
    workZone(65536, 8),
    workState{},
    zeroBuffer(),
    edgeStorage(),
    edgeBuilder(&workZone, &edgeStorage) {}

WorkData::~WorkData() noexcept {
  if (edgeStorage.bandEdges())
    blZeroAllocatorRelease(edgeStorage.bandEdges(), edgeStorage.bandCapacity() * kEdgeListSize);
}

// bl::RasterEngine::WorkData - Initialization
// ===========================================

BLResult WorkData::initBandData(uint32_t bandHeight, uint32_t bandCount, uint32_t commandQuantizationShift) noexcept {
  // Can only happen if the storage was already allocated.
  if (bandCount <= edgeStorage.bandCapacity()) {
    _bandHeight = bandHeight;
    edgeStorage.initData(edgeStorage.bandEdges(), bandCount, edgeStorage.bandCapacity(), bandHeight);
  }
  else {
    size_t allocatedSize = 0;
    EdgeList<int>* edges = static_cast<EdgeList<int>*>(
      blZeroAllocatorResize(
        edgeStorage.bandEdges(),
        edgeStorage.bandCapacity() * kEdgeListSize,
        bandCount * kEdgeListSize,
        &allocatedSize));

    if (BL_UNLIKELY(!edges)) {
      edgeStorage.reset();
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
    }

    uint32_t bandCapacity = uint32_t(allocatedSize / kEdgeListSize);
    _bandHeight = bandHeight;
    edgeStorage.initData(edges, bandCount, bandCapacity, bandHeight);
  }

  _commandQuantizationShiftAA = uint8_t(commandQuantizationShift);
  _commandQuantizationShiftFp = uint8_t(commandQuantizationShift + 8);

  return BL_SUCCESS;
}

// bl::RasterEngine::WorkData - Error Accumulation
// ===============================================

BLResult WorkData::accumulateError(BLResult error) noexcept {
  switch (error) {
    // Should not happen.
    case BL_SUCCESS: break;

    case BL_ERROR_INVALID_VALUE        : _accumulatedErrorFlags |= BL_CONTEXT_ERROR_FLAG_INVALID_VALUE        ; break;
    case BL_ERROR_INVALID_GEOMETRY     : _accumulatedErrorFlags |= BL_CONTEXT_ERROR_FLAG_INVALID_GEOMETRY     ; break;
    case BL_ERROR_INVALID_GLYPH        : _accumulatedErrorFlags |= BL_CONTEXT_ERROR_FLAG_INVALID_GLYPH        ; break;
    case BL_ERROR_FONT_NOT_INITIALIZED : _accumulatedErrorFlags |= BL_CONTEXT_ERROR_FLAG_INVALID_FONT         ; break;
    case BL_ERROR_THREAD_POOL_EXHAUSTED: _accumulatedErrorFlags |= BL_CONTEXT_ERROR_FLAG_THREAD_POOL_EXHAUSTED; break;
    case BL_ERROR_OUT_OF_MEMORY        : _accumulatedErrorFlags |= BL_CONTEXT_ERROR_FLAG_OUT_OF_MEMORY        ; break;
    default                            : _accumulatedErrorFlags |= BL_CONTEXT_ERROR_FLAG_UNKNOWN_ERROR        ; break;
  }
  return error;
}

} // {bl::RasterEngine}
