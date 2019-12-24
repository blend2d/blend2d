// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../api-build_p.h"
#include "../raster/rastercontext_p.h"
#include "../raster/rasterworkercontext_p.h"

// ============================================================================
// [BLRasterWorkerContext - Init / Reset]
// ============================================================================

BLRasterWorkerContext::BLRasterWorkerContext(BLRasterContextImpl* ctxI) noexcept
  : ctxI(ctxI),
    ctxData(),
    workZone(65536 - BLZoneAllocator::kBlockOverhead, 8),
    zeroBuffer(),
    edgeStorage(),
    edgeBuilder(&workZone, &edgeStorage) {

  clipMode = BL_CLIP_MODE_ALIGNED_RECT;
  reserved[0] = 0;
  reserved[1] = 0;
  reserved[2] = 0;

  edgeStorage.setBandHeight(32);

  fullAlpha = 0x100;
  dstData.reset();
}

BLRasterWorkerContext::~BLRasterWorkerContext() noexcept {
  if (edgeStorage.bandEdges())
    blZeroAllocatorRelease(edgeStorage.bandEdges(), edgeStorage.bandCapacity() * sizeof(void*));
}

// ============================================================================
// [BLRasterWorkerContext - Interface]
// ============================================================================

BLResult BLRasterWorkerContext::initEdgeStorage(uint32_t height) noexcept {
  uint32_t bandHeight = edgeStorage.bandHeight();
  uint32_t bandCount = (height + bandHeight - 1) >> blBitCtz(bandHeight);

  if (bandCount <= edgeStorage.bandCapacity())
    return BL_SUCCESS;

  size_t allocatedSize = 0;
  edgeStorage._bandEdges = static_cast<BLEdgeVector<int>**>(
    blZeroAllocatorResize(
      edgeStorage.bandEdges(),
      edgeStorage.bandCapacity() * sizeof(void*),
      bandCount * sizeof(void*),
      &allocatedSize));

  uint32_t bandCapacity = uint32_t(allocatedSize / sizeof(void*));
  edgeStorage._bandCount = blMin(bandCount, bandCapacity);
  edgeStorage._bandCapacity = bandCapacity;

  if (!edgeStorage._bandEdges)
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  return BL_SUCCESS;
}
