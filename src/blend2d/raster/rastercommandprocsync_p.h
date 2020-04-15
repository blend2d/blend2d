// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_RASTERCOMMANDPROCSYNC_P_H
#define BLEND2D_RASTER_RASTERCOMMANDPROCSYNC_P_H

#include "../context_p.h"
#include "../geometry_p.h"
#include "../pipedefs_p.h"
#include "../zeroallocator_p.h"
#include "../zoneallocator_p.h"
#include "../raster/analyticrasterizer_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rastercommand_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/rasterworkdata_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [blRasterCommandProcSync - FillBoxA]
// ============================================================================

static BL_INLINE BLResult blRasterCommandProcSync_FillBoxA(BLRasterWorkData& workData, const BLRasterCommand& command) noexcept {
  BLPipeFillFunc fillFunc = command.fillFunc();
  const void* fillData = command.getPipeFillDataOfBoxA();
  const void* fetchData = command.getPipeFetchData();

  fillFunc(&workData.ctxData, fillData, fetchData);
  return BL_SUCCESS;
}

// ============================================================================
// [blRasterCommandProcSync - FillBoxU]
// ============================================================================

static BL_INLINE BLResult blRasterCommandProcSync_FillBoxU(BLRasterWorkData& workData, const BLRasterCommand& command) noexcept {
  BLPipeFillData fillData;
  if (!fillData.initBoxU8bpc24x8(command.alpha(), command.boxI().x0, command.boxI().y0, command.boxI().x1, command.boxI().y1))
    return BL_SUCCESS;

  BLPipeFillFunc fillFunc = command.fillFunc();
  const void* fetchData = command.getPipeFetchData();

  fillFunc(&workData.ctxData, &fillData, fetchData);
  return BL_SUCCESS;
}

// ============================================================================
// [blRasterCommandProcSync - FillAnalytic]
// ============================================================================

template<typename T>
struct BLActiveEdge {
  //! Rasterizer state.
  BLAnalyticRasterizerState state;
  //! Sign bit, for making cover/area negative.
  uint32_t signBit;
  //! Start of point data (advanced during rasterization).
  const BLEdgePoint<T>* cur;
  //! End of point data.
  const BLEdgePoint<T>* end;
  //! Next active edge (single-linked list).
  BLActiveEdge<T>* next;
};

static BL_NOINLINE BLResult blRasterCommandProcSync_FillAnalytic(BLRasterWorkData& workData, const BLRasterCommand& command) noexcept {
  // Rasterizer options to use - do not change unless you are improving
  // the existing rasterizers.
  constexpr uint32_t kRasterizerOptions =
    BLAnalyticRasterizer::kOptionBandOffset     |
    BLAnalyticRasterizer::kOptionRecordMinXMaxX ;

  // Can only be called if there is something to fill.
  const BLEdgeStorage<int>* edgeStorage = command.analyticEdgesSync();
  BL_ASSERT(edgeStorage != nullptr);

  // NOTE: This doesn't happen often, but it's possible. If, for any reason,
  // the data in bands is all horizontal lines or no data at all it would
  // trigger this check.
  if (BL_UNLIKELY(edgeStorage->boundingBox().y0 >= edgeStorage->boundingBox().y1))
    return BL_SUCCESS;

  uint32_t bandHeight = edgeStorage->bandHeight();
  uint32_t bandHeightMask = bandHeight - 1;

  const uint32_t yStart = (uint32_t(edgeStorage->boundingBox().y0)                  ) >> BL_PIPE_A8_SHIFT;
  const uint32_t yEnd   = (uint32_t(edgeStorage->boundingBox().y1) + BL_PIPE_A8_MASK) >> BL_PIPE_A8_SHIFT;

  size_t requiredWidth = blAlignUp(uint32_t(workData.dstSize().w) + 1u + BL_PIPE_PIXELS_PER_ONE_BIT, BL_PIPE_PIXELS_PER_ONE_BIT);
  size_t requiredHeight = bandHeight;
  size_t cellAlignment = 16;

  size_t bitStride = blBitWordCountFromBitCount<BLBitWord>(requiredWidth / BL_PIPE_PIXELS_PER_ONE_BIT) * sizeof(BLBitWord);
  size_t cellStride = requiredWidth * sizeof(uint32_t);

  size_t bitsStart = 0;
  size_t bitsSize = requiredHeight * bitStride;

  size_t cellsStart = blAlignUp(bitsStart + bitsSize, cellAlignment);
  size_t cellsSize = requiredHeight * cellStride;

  BL_PROPAGATE(workData.zeroBuffer.ensure(cellsStart + cellsSize));

  BLAnalyticCellStorage cellStorage;
  cellStorage.init(
    reinterpret_cast<BLBitWord*>(workData.zeroBuffer.data + bitsStart), bitStride,
    blAlignUp(reinterpret_cast<uint32_t*>(workData.zeroBuffer.data + cellsStart), cellAlignment), cellStride);

  BLActiveEdge<int>* active = nullptr;
  BLActiveEdge<int>* pooled = nullptr;

  BLEdgeList<int>* bandEdges = edgeStorage->bandEdges();
  uint32_t fixedBandHeightShift = edgeStorage->fixedBandHeightShift();

  uint32_t bandId = unsigned(edgeStorage->boundingBox().y0) >> fixedBandHeightShift;
  uint32_t bandLast = unsigned(edgeStorage->boundingBox().y1 - 1) >> fixedBandHeightShift;
  uint32_t dstWidth = uint32_t(workData.dstSize().w);

  BLPipeFillFunc fillFunc = command.fillFunc();
  BLPipeFillData fillData;

  fillData.initAnalytic(command.alpha(),
                        command.analyticFillRule(),
                        cellStorage.bitPtrTop, cellStorage.bitStride,
                        cellStorage.cellPtrTop, cellStorage.cellStride);

  BLAnalyticRasterizer ras;
  ras.init(cellStorage.bitPtrTop, cellStorage.bitStride,
           cellStorage.cellPtrTop, cellStorage.cellStride,
           bandId * bandHeight, bandHeight);
  ras._bandOffset = yStart;

  BLZoneAllocator* workZone = &workData.workZone;
  const void* fetchData = command.getPipeFetchData();

  do {
    BLEdgeVector<int>* edges = bandEdges[bandId].first();
    bandEdges[bandId].reset();

    BLActiveEdge<int>** pPrev = &active;
    BLActiveEdge<int>* current = *pPrev;

    ras.resetBounds();
    ras._bandEnd = blMin((bandId + 1) * bandHeight, yEnd) - 1;

    while (current) {
      ras.restore(current->state);
      ras.setSignMaskFromBit(current->signBit);

      for (;;) {
Rasterize:
        if (ras.template rasterize<kRasterizerOptions | BLAnalyticRasterizer::kOptionBandingMode>()) {
          // The edge is fully rasterized.
          const BLEdgePoint<int>* pts = current->cur;
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

          BLActiveEdge<int>* old = current;
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
        pooled = static_cast<BLActiveEdge<int>*>(workZone->alloc(sizeof(BLActiveEdge<int>)));
        if (BL_UNLIKELY(!pooled))
          return blTraceError(BL_ERROR_OUT_OF_MEMORY);
        pooled->next = nullptr;
      }

      do {
        const BLEdgePoint<int>* pts = edges->pts + 1;
        const BLEdgePoint<int>* end = edges->pts + edges->count;

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
    // because we don't unlink during edge pooling as it's just faster to do
    // it here.
    *pPrev = nullptr;

    if (ras.hasBounds()) {
      fillData.analytic.box.x0 = int(ras._cellMinX);
      fillData.analytic.box.x1 = int(blMin(dstWidth, blAlignUp(ras._cellMaxX + 1, BL_PIPE_PIXELS_PER_ONE_BIT)));
      fillData.analytic.box.y0 = int(ras._bandOffset);
      fillData.analytic.box.y1 = int(ras._bandEnd) + 1;
      fillFunc(&workData.ctxData, &fillData, fetchData);
    }

    ras._bandOffset = (ras._bandOffset + bandHeight) & ~bandHeightMask;
  } while (++bandId <= bandLast);

  workZone->clear();
  return BL_SUCCESS;
}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERCOMMANDPROCSYNC_P_H
