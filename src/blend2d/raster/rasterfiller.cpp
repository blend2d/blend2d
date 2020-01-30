// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../api-build_p.h"
#include "../raster/analyticrasterizer_p.h"
#include "../raster/rasterfiller_p.h"
#include "../raster/rasterworkercontext_p.h"

// ============================================================================
// [BLRasterFiller - Fill Rect]
// ============================================================================

BLResult BL_CDECL BLRasterFiller::fillRectImpl(BLRasterFiller* filler, BLRasterWorkerContext* workerCtx, const BLRasterFetchData* fetchData) noexcept {
  filler->fillFunc(&workerCtx->ctxData, &filler->fillData, fetchData);
  return BL_SUCCESS;
}

// ============================================================================
// [BLRasterFiller - Fill Analytic]
// ============================================================================

struct BLActiveEdge {
  //! Rasterizer state.
  BLAnalyticRasterizerState state;
  //! Sign bit, for making cover/area negative.
  uint32_t signBit;
  //! Start of point data (advanced during rasterization).
  BLEdgePoint<int>* cur;
  //! End of point data.
  BLEdgePoint<int>* end;
  //! Next active edge (single-linked list).
  BLActiveEdge* next;
};

static void debugEdges(BLEdgeStorage<int>* edgeStorage) noexcept {
  BLEdgeVector<int>** edges = edgeStorage->bandEdges();
  size_t count = edgeStorage->bandCount();
  uint32_t bandHeight = edgeStorage->bandHeight();

  int minX = blMaxValue<int>();
  int minY = blMaxValue<int>();
  int maxX = blMinValue<int>();
  int maxY = blMinValue<int>();

  const BLBoxI& bb = edgeStorage->boundingBox();
  printf("EDGE STORAGE [%d.%d %d.%d %d.%d %d.%d]:\n",
         bb.x0 >> 8, bb.x0 & 0xFF,
         bb.y0 >> 8, bb.y0 & 0xFF,
         bb.x1 >> 8, bb.x1 & 0xFF,
         bb.y1 >> 8, bb.y1 & 0xFF);

  for (size_t bandId = 0; bandId < count; bandId++) {
    BLEdgeVector<int>* edge = edges[bandId];
    if (edge) {
      printf("BAND #%d y={%d:%d}\n", int(bandId), int(bandId * bandHeight), int((bandId + 1) * bandHeight - 1));
      while (edge) {
        printf("  EDGES {sign=%u count=%u}", unsigned(edge->signBit), unsigned(edge->count));

        if (edge->count <= 1)
          printf("{WRONG COUNT!}");

        BLEdgePoint<int>* ptr = edge->pts;
        BLEdgePoint<int>* ptrStart = ptr;
        BLEdgePoint<int>* ptrEnd = edge->pts + edge->count;

        while (ptr != ptrEnd) {
          minX = blMin(minX, ptr[0].x);
          minY = blMin(minY, ptr[0].y);
          maxX = blMax(maxX, ptr[0].x);
          maxY = blMax(maxY, ptr[0].y);

          printf(" [%d.%d, %d.%d]", ptr[0].x >> 8, ptr[0].x & 0xFF, ptr[0].y >> 8, ptr[0].y & 0xFF);

          if (ptr != ptrStart && ptr[-1].y > ptr[0].y)
            printf(" !INVALID! ");

          ptr++;
        }

        printf("\n");
        edge = edge->next;
      }
    }
  }

  printf("EDGE STORAGE BBOX [%d.%d, %d.%d] -> [%d.%d, %d.%d]\n\n",
    minX >> 8, minX & 0xFF,
    minY >> 8, minY & 0xFF,
    maxX >> 8, maxX & 0xFF,
    maxY >> 8, maxY & 0xFF);
}

BLResult BL_CDECL BLRasterFiller::fillAnalyticImpl(BLRasterFiller* filler, BLRasterWorkerContext* workerCtx, const BLRasterFetchData* fetchData) noexcept {
  // Rasterizer options to use - do not change unless you are improving
  // the existing rasterizers.
  constexpr uint32_t kRasterizerOptions =
    BLAnalyticRasterizer::kOptionBandOffset     |
    BLAnalyticRasterizer::kOptionRecordMinXMaxX ;

  // Can only be called if there is something to fill.
  BLEdgeStorage<int>* edgeStorage = filler->edgeStorage;

  // TODO: REMOVE.
  // debugEdges(edgeStorage);
  // printf("NUM LINES: %u\n", unsigned(calcLines(edgeStorage)));

  // NOTE: This doesn't happen often, but it's possible. If, for any reason,
  // the data in bands is all horz lines or no data at all it would trigger
  // this condition.
  if (BL_UNLIKELY(edgeStorage->boundingBox().y0 >= edgeStorage->boundingBox().y1))
    return BL_SUCCESS;

  uint32_t bandHeight = edgeStorage->bandHeight();
  uint32_t bandHeightMask = bandHeight - 1;

  const uint32_t yStart = (uint32_t(edgeStorage->boundingBox().y0)                  ) >> BL_PIPE_A8_SHIFT;
  const uint32_t yEnd   = (uint32_t(edgeStorage->boundingBox().y1) + BL_PIPE_A8_MASK) >> BL_PIPE_A8_SHIFT;

  size_t requiredWidth = blAlignUp(uint32_t(workerCtx->dstData.size.w) + 1u + BL_PIPE_PIXELS_PER_ONE_BIT, BL_PIPE_PIXELS_PER_ONE_BIT);
  size_t requiredHeight = bandHeight;
  size_t cellAlignment = 16;

  size_t bitStride = blBitWordCountFromBitCount<BLBitWord>(requiredWidth / BL_PIPE_PIXELS_PER_ONE_BIT) * sizeof(BLBitWord);
  size_t cellStride = requiredWidth * sizeof(uint32_t);

  size_t bitsStart = 0;
  size_t bitsSize = requiredHeight * bitStride;

  size_t cellsStart = blAlignUp(bitsStart + bitsSize, cellAlignment);
  size_t cellsSize = requiredHeight * cellStride;

  BL_PROPAGATE(workerCtx->zeroBuffer.ensure(cellsStart + cellsSize));

  BLAnalyticCellStorage cellStorage;
  cellStorage.init(reinterpret_cast<BLBitWord*>(workerCtx->zeroBuffer.data + bitsStart), bitStride,
                   blAlignUp(reinterpret_cast<uint32_t*>(workerCtx->zeroBuffer.data + cellsStart), cellAlignment), cellStride);

  BLActiveEdge* active = nullptr;
  BLActiveEdge* pooled = nullptr;

  BLEdgeVector<int>** bandEdges = edgeStorage->_bandEdges;
  uint32_t fixedBandHeightShift = edgeStorage->fixedBandHeightShift();

  uint32_t bandId = unsigned(edgeStorage->boundingBox().y0) >> fixedBandHeightShift;
  uint32_t bandLast = unsigned(edgeStorage->boundingBox().y1 - 1) >> fixedBandHeightShift;
  uint32_t dstWidth = uint32_t(workerCtx->dstData.size.w);

  // Overwritten before calling `fillFunc`.
  filler->fillData.analytic.box.reset();

  // Same for all bands.
  filler->fillData.analytic.bitTopPtr = cellStorage.bitPtrTop;
  filler->fillData.analytic.bitStride = cellStorage.bitStride;
  filler->fillData.analytic.cellTopPtr = cellStorage.cellPtrTop;
  filler->fillData.analytic.cellStride = cellStorage.cellStride;

  BLAnalyticRasterizer ras;
  ras.init(cellStorage.bitPtrTop, cellStorage.bitStride, cellStorage.cellPtrTop, cellStorage.cellStride, bandId * bandHeight, bandHeight);
  ras._bandOffset = yStart;

  BLZoneAllocator* workZone = &workerCtx->workZone;
  do {
    BLEdgeVector<int>* edges = bandEdges[bandId];
    bandEdges[bandId] = nullptr;

    BLActiveEdge** pPrev = &active;
    BLActiveEdge* current = *pPrev;

    ras.resetBounds();
    ras._bandEnd = blMin((bandId + 1) * bandHeight, yEnd) - 1;

    while (current) {
      ras.restore(current->state);
      ras.setSignMaskFromBit(current->signBit);

      for (;;) {
Rasterize:
        if (ras.template rasterize<kRasterizerOptions | BLAnalyticRasterizer::kOptionBandingMode>()) {
          // The edge is fully rasterized.
          BLEdgePoint<int>* pts = current->cur;
          while (pts != current->end) {
            pts++;
            if (!ras.prepare(pts[-2].x, pts[-2].y, pts[-1].x, pts[-1].y))
              continue;

            current->cur = pts;
            if (uint32_t(ras._ey0) <= ras._bandEnd)
              goto Rasterize;
            else
              goto SaveState;
          }

          BLActiveEdge* old = current;
          current = current->next;

          old->next = pooled;
          pooled = old;
          break;
        }

SaveState:
        // The edge is not fully rasterized and crosses the band.
        ras.save(current->state);

        *pPrev = current;
        pPrev = &current->next;
        current = *pPrev;
        break;
      }
    }

    if (edges) {
      if (!pooled) {
        pooled = static_cast<BLActiveEdge*>(workZone->alloc(sizeof(BLActiveEdge)));
        if (BL_UNLIKELY(!pooled))
          return blTraceError(BL_ERROR_OUT_OF_MEMORY);
        pooled->next = nullptr;
      }

      do {
        BLEdgePoint<int>* pts = edges->pts + 1;
        BLEdgePoint<int>* end = edges->pts + edges->count;

        uint32_t signBit = edges->signBit;
        ras.setSignMaskFromBit(signBit);

        edges = edges->next;
        do {
          pts++;
          if (!ras.prepare(pts[-2].x, pts[-2].y, pts[-1].x, pts[-1].y))
            continue;

          if (uint32_t(ras._ey1) <= ras._bandEnd) {
            ras.template rasterize<kRasterizerOptions>();
          }
          else {
            current = pooled;
            pooled = current->next;

            current->signBit = signBit;
            current->cur = pts;
            current->end = end;
            current->next = nullptr;

            if (uint32_t(ras._ey0) <= ras._bandEnd)
              goto Rasterize;
            else
              goto SaveState;
          }
        } while (pts != end);
      } while (edges);
    }

    // Makes `active` or the last `BLActiveEdge->next` null. It's important,
    // because we don't unlink during edge pooling as we want to do it here.
    *pPrev = nullptr;

    if (ras.hasBounds()) {
      filler->fillData.analytic.box.x1 = int(blMin(dstWidth, blAlignUp(ras._cellMaxX, BL_PIPE_PIXELS_PER_ONE_BIT)));
      filler->fillData.analytic.box.x1 = int(dstWidth);
      filler->fillData.analytic.box.y0 = int(ras._bandOffset);
      filler->fillData.analytic.box.y1 = int(ras._bandEnd) + 1;
      filler->fillFunc(&workerCtx->ctxData, &filler->fillData, fetchData);
    }

    ras._bandOffset = (ras._bandOffset + bandHeight) & ~bandHeightMask;
  } while (++bandId <= bandLast);

  workZone->clear();
  return BL_SUCCESS;
}
