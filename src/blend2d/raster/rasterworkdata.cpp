// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../api-build_p.h"
#include "../raster/rastercontext_p.h"
#include "../raster/rasterworkdata_p.h"

// ============================================================================
// [BLRasterWorkData - Init / Reset]
// ============================================================================

BLRasterWorkData::BLRasterWorkData(BLRasterContextImpl* ctxI, uint32_t workerId) noexcept
  : ctxI(ctxI),
    batch(nullptr),
    ctxData(),
    clipMode(BL_CLIP_MODE_ALIGNED_RECT),
    reserved {},
    _workerId(workerId),
    _bandHeight(0),
    _accumulatedErrorFlags(0),
    workZone(65536 - BLZoneAllocator::kBlockOverhead, 8),
    workState{},
    zeroBuffer(),
    edgeStorage(),
    edgeBuilder(&workZone, &edgeStorage) {}

BLRasterWorkData::~BLRasterWorkData() noexcept {
  if (edgeStorage.bandEdges())
    blZeroAllocatorRelease(edgeStorage.bandEdges(), edgeStorage.bandCapacity() * sizeof(void*));
}

// ============================================================================
// [BLRasterWorkData - Interface]
// ============================================================================

BLResult BLRasterWorkData::initBandData(uint32_t bandHeight, uint32_t bandCount) noexcept {
  // Can only happen if the storage was already allocated.
  if (bandCount <= edgeStorage.bandCapacity()) {
    _bandHeight = bandHeight;
    edgeStorage.initData(edgeStorage.bandEdges(), bandCount, edgeStorage.bandCapacity(), bandHeight);
    return BL_SUCCESS;
  }

  size_t allocatedSize = 0;
  BLEdgeList<int>* edges = static_cast<BLEdgeList<int>*>(
    blZeroAllocatorResize(
      edgeStorage.bandEdges(),
      edgeStorage.bandCapacity() * sizeof(void*),
      bandCount * sizeof(BLEdgeList<int>),
      &allocatedSize));

  if (BL_UNLIKELY(!edges)) {
    edgeStorage.reset();
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  uint32_t bandCapacity = uint32_t(allocatedSize / sizeof(void*));
  _bandHeight = bandHeight;
  edgeStorage.initData(edges, bandCount, bandCapacity, bandHeight);
  return BL_SUCCESS;
}

BLResult BLRasterWorkData::accumulateError(BLResult error) noexcept {
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
