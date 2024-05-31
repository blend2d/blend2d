// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RENDERCOMMANDPROCSYNC_P_H_INCLUDED
#define BLEND2D_RASTER_RENDERCOMMANDPROCSYNC_P_H_INCLUDED

#include "../context_p.h"
#include "../geometry_p.h"
#include "../pipeline/pipedefs_p.h"
#include "../raster/analyticrasterizer_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rendercommand_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/workdata_p.h"
#include "../support/arenaallocator_p.h"
#include "../support/intops_p.h"
#include "../support/zeroallocator_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace bl {
namespace RasterEngine {
namespace CommandProcSync {

static BL_INLINE BLResult fillBoxA(WorkData& workData, const Pipeline::DispatchData& dispatchData, uint32_t alpha, const BLBoxI& boxA, const void* fetchData) noexcept {
  Pipeline::FillData fillData;
  fillData.initBoxA8bpc(alpha, boxA.x0, boxA.y0, boxA.x1, boxA.y1);

  Pipeline::FillFunc fillFunc = dispatchData.fillFunc;
  Pipeline::FetchFunc fetchFunc = dispatchData.fetchFunc;

  if (fetchFunc == nullptr) {
    fillFunc(&workData.ctxData, &fillData, fetchData);
  }
  else {
    // TODO:
  }

  return BL_SUCCESS;
}

static BL_INLINE BLResult fillBoxU(WorkData& workData, const Pipeline::DispatchData& dispatchData, uint32_t alpha, const BLBoxI& boxU, const void* fetchData) noexcept {
  Pipeline::FillData fillData;
  Pipeline::BoxUToMaskData boxUToMaskData;

  if (!fillData.initBoxU8bpc24x8(alpha, boxU.x0, boxU.y0, boxU.x1, boxU.y1, boxUToMaskData))
    return BL_SUCCESS;

  Pipeline::FillFunc fillFunc = dispatchData.fillFunc;
  Pipeline::FetchFunc fetchFunc = dispatchData.fetchFunc;

  if (fetchFunc == nullptr) {
    fillFunc(&workData.ctxData, &fillData, fetchData);
  }
  else {
    // TODO:
  }

  return BL_SUCCESS;
}

static BL_INLINE BLResult fillBoxMaskedA(WorkData& workData, const Pipeline::DispatchData& dispatchData, uint32_t alpha, const RenderCommand::FillBoxMaskA& payload, const void* fetchData) noexcept {
  const BLImageImpl* maskI = payload.maskImageI.ptr;
  const BLPointI& maskOffset = payload.maskOffsetI;
  const uint8_t* maskData = static_cast<const uint8_t*>(maskI->pixelData) + maskI->stride * maskOffset.y + maskOffset.x * (maskI->depth / 8u);

  const BLBoxI& boxI = payload.boxI;

  Pipeline::MaskCommand maskCommands[2];
  Pipeline::MaskCommandType vMaskCmd = alpha >= 255 ? Pipeline::MaskCommandType::kVMaskA8WithGA : Pipeline::MaskCommandType::kVMaskA8WithoutGA;

  maskCommands[0].initVMask(vMaskCmd, uint32_t(boxI.x0), uint32_t(boxI.x1), maskData, maskI->stride);
  maskCommands[1].initRepeat();

  Pipeline::FillData fillData;
  fillData.initMaskA(alpha, boxI.x0, boxI.y0, boxI.x1, boxI.y1, maskCommands);

  Pipeline::FillFunc fillFunc = dispatchData.fillFunc;
  fillFunc(&workData.ctxData, &fillData, fetchData);

  return BL_SUCCESS;
}

static BL_NOINLINE BLResult fillAnalytic(WorkData& workData, const Pipeline::DispatchData& dispatchData, uint32_t alpha, const EdgeStorage<int>* edgeStorage, BLFillRule fillRule, const void* fetchData) noexcept {
  // Rasterizer options to use - do not change unless you are improving the existing rasterizers.
  constexpr uint32_t kRasterizerOptions = AnalyticRasterizer::kOptionBandOffset | AnalyticRasterizer::kOptionRecordMinXMaxX;

  // Can only be called if there is something to fill.
  BL_ASSERT(edgeStorage != nullptr);
  // Should have been verified by the caller.
  BL_ASSERT(edgeStorage->boundingBox().y0 < edgeStorage->boundingBox().y1);

  uint32_t bandHeight = edgeStorage->bandHeight();
  uint32_t bandHeightMask = bandHeight - 1;

  const uint32_t yStart = (uint32_t(edgeStorage->boundingBox().y0)                          ) >> Pipeline::A8Info::kShift;
  const uint32_t yEnd   = (uint32_t(edgeStorage->boundingBox().y1) + Pipeline::A8Info::kMask) >> Pipeline::A8Info::kShift;

  size_t requiredWidth = IntOps::alignUp(uint32_t(workData.dstSize().w) + 1u + BL_PIPE_PIXELS_PER_ONE_BIT, BL_PIPE_PIXELS_PER_ONE_BIT);
  size_t requiredHeight = bandHeight;
  size_t cellAlignment = 16;

  size_t bitStride = IntOps::wordCountFromBitCount<BLBitWord>(requiredWidth / BL_PIPE_PIXELS_PER_ONE_BIT) * sizeof(BLBitWord);
  size_t cellStride = requiredWidth * sizeof(uint32_t);

  size_t bitsStart = 0;
  size_t bitsSize = requiredHeight * bitStride;

  size_t cellsStart = IntOps::alignUp(bitsStart + bitsSize, cellAlignment);
  BL_ASSERT(workData.zeroBuffer.size >= cellsStart + requiredHeight * cellStride);

  AnalyticCellStorage cellStorage;
  cellStorage.init(
    reinterpret_cast<BLBitWord*>(workData.zeroBuffer.data + bitsStart), bitStride,
    IntOps::alignUp(reinterpret_cast<uint32_t*>(workData.zeroBuffer.data + cellsStart), cellAlignment), cellStride);

  AnalyticActiveEdge<int>* active = nullptr;
  AnalyticActiveEdge<int>* pooled = nullptr;

  EdgeList<int>* bandEdges = edgeStorage->bandEdges();
  uint32_t bandId = edgeStorage->bandStartFromBBox();
  uint32_t bandEnd = edgeStorage->bandEndFromBBox();

  uint32_t dstWidth = uint32_t(workData.dstSize().w);

  Pipeline::FillFunc fillFunc = dispatchData.fillFunc;
  Pipeline::FillData fillData;

  fillData.initAnalytic(alpha,
                        uint32_t(fillRule),
                        cellStorage.bitPtrTop, cellStorage.bitStride,
                        cellStorage.cellPtrTop, cellStorage.cellStride);

  AnalyticRasterizer ras;
  ras.init(cellStorage.bitPtrTop, cellStorage.bitStride,
           cellStorage.cellPtrTop, cellStorage.cellStride,
           bandId * bandHeight, bandHeight);
  ras._bandOffset = yStart;

  ArenaAllocator* workZone = &workData.workZone;
  do {
    EdgeVector<int>* edges = bandEdges[bandId].first();
    bandEdges[bandId].reset();

    AnalyticActiveEdge<int>** pPrev = &active;
    AnalyticActiveEdge<int>* current = *pPrev;

    ras.resetBounds();
    ras._bandEnd = blMin((bandId + 1) * bandHeight, yEnd) - 1;

    while (current) {
      ras.restore(current->state);
      ras.setSignMaskFromBit(current->signBit);

      for (;;) {
Rasterize:
        if (ras.template rasterize<kRasterizerOptions | AnalyticRasterizer::kOptionBandingMode>()) {
          // The edge is fully rasterized.
          const EdgePoint<int>* pts = current->cur;
          while (pts != current->end) {
            pts++;
            if (!ras.prepare(pts[-2], pts[-1]))
              continue;

            current->cur = pts;
            if (uint32_t(ras._ey0) <= ras._bandEnd)
              goto Rasterize;
            else
              goto SaveState;
          }

          AnalyticActiveEdge<int>* old = current;
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
        pooled = static_cast<AnalyticActiveEdge<int>*>(workZone->alloc(sizeof(AnalyticActiveEdge<int>)));
        if (BL_UNLIKELY(!pooled))
          return blTraceError(BL_ERROR_OUT_OF_MEMORY);
        pooled->next = nullptr;
      }

      do {
        const EdgePoint<int>* pts = edges->pts + 1;
        const EdgePoint<int>* end = edges->pts + edges->count;

        uint32_t signBit = edges->signBit;
        ras.setSignMaskFromBit(signBit);

        edges = edges->next;
        do {
          pts++;
          if (!ras.prepare(pts[-2], pts[-1]))
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

    // Makes `active` or the last `AnalyticActiveEdge->next` null. It's important, because we don't unlink during
    // edge pooling as it's just faster to do it here.
    *pPrev = nullptr;

    if (ras.hasBounds()) {
      fillData.analytic.box.x0 = int(ras._cellMinX);
      fillData.analytic.box.x1 = int(blMin(dstWidth, IntOps::alignUp(ras._cellMaxX + 1, BL_PIPE_PIXELS_PER_ONE_BIT)));
      fillData.analytic.box.y0 = int(ras._bandOffset);
      fillData.analytic.box.y1 = int(ras._bandEnd) + 1;

      fillFunc(&workData.ctxData, &fillData, fetchData);
    }

    ras._bandOffset = (ras._bandOffset + bandHeight) & ~bandHeightMask;
  } while (++bandId < bandEnd);

  workZone->clear();
  return BL_SUCCESS;
}

} // {CommandProcSync}
} // {RasterEngine}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RENDERCOMMANDPROCSYNC_P_H_INCLUDED
