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
    reserved{},
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
    blZeroAllocatorRelease(edgeStorage.bandEdges(), edgeStorage.bandCapacity() * kEdgeListSize);
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
