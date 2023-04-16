// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RENDERCOMMANDPROCSYNC_P_H_INCLUDED
#define BLEND2D_RASTER_RENDERCOMMANDPROCSYNC_P_H_INCLUDED

#include "../context_p.h"
#include "../geometry_p.h"
#include "../zeroallocator_p.h"
#include "../pipeline/pipedefs_p.h"
#include "../raster/analyticrasterizer_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rendercommand_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/workdata_p.h"
#include "../support/arenaallocator_p.h"
#include "../support/intops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace BLRasterEngine {
namespace CommandProcSync {

static BL_INLINE BLResult fillBoxA(WorkData& workData, const RenderCommand& command) noexcept {
  const void* fillData = command.getPipeFillDataOfBoxA();
  const void* fetchData = command.getPipeFetchData();

  BLPipeline::FillFunc fillFunc = command.pipeDispatchData()->fillFunc;
  BLPipeline::FetchFunc fetchFunc = command.pipeDispatchData()->fetchFunc;

  if (fetchFunc == nullptr) {
    fillFunc(&workData.ctxData, fillData, fetchData);
  }
  else {
    // TODO:
  }

  return BL_SUCCESS;
}

static BL_INLINE BLResult fillBoxU(WorkData& workData, const RenderCommand& command) noexcept {
  BLPipeline::FillData fillData;
  BLPipeline::BoxUToMaskData boxUToMaskData;

  if (!fillData.initBoxU8bpc24x8(command.alpha(), command.boxI().x0, command.boxI().y0, command.boxI().x1, command.boxI().y1, boxUToMaskData))
    return BL_SUCCESS;

  const void* fetchData = command.getPipeFetchData();

  BLPipeline::FillFunc fillFunc = command.pipeDispatchData()->fillFunc;
  BLPipeline::FetchFunc fetchFunc = command.pipeDispatchData()->fetchFunc;

  if (fetchFunc == nullptr) {
    fillFunc(&workData.ctxData, &fillData, fetchData);
  }
  else {
    // TODO:
  }

  return BL_SUCCESS;
}

static BL_NOINLINE BLResult fillMaskRaw(WorkData& workData, const RenderCommand& command) noexcept {
  const RenderCommand::FillMaskRaw& payload = command._payload.maskRaw;
  const BLImageImpl* maskI = payload.maskImageI;
  const BLPointI& maskOffset = payload.maskOffsetI;
  const uint8_t* maskData = static_cast<const uint8_t*>(maskI->pixelData) + maskI->stride * maskOffset.y + maskOffset.x * (maskI->depth / 8u);

  const BLBoxI& boxI = payload.boxI;

  BLPipeline::MaskCommand maskCommands[2];
  BLPipeline::MaskCommandType vMaskCmd = command.alpha() >= 255 ? BLPipeline::MaskCommandType::kVMaskA8WithGA : BLPipeline::MaskCommandType::kVMaskA8WithoutGA;

  maskCommands[0].initVMask(vMaskCmd, uint32_t(boxI.x0), uint32_t(boxI.x1), maskData, maskI->stride);
  maskCommands[1].initRepeat();

  BLPipeline::FillData fillData;
  fillData.initMaskA(command.alpha(), boxI.x0, boxI.y0, boxI.x1, boxI.y1, maskCommands);

  BLPipeline::FillFunc fillFunc = command.pipeDispatchData()->fillFunc;
  const void* fetchData = command.getPipeFetchData();

  fillFunc(&workData.ctxData, &fillData, fetchData);
  return BL_SUCCESS;
}

static BL_NOINLINE BLResult fillAnalytic(WorkData& workData, const RenderCommand& command) noexcept {
  // Rasterizer options to use - do not change unless you are improving the existing rasterizers.
  constexpr uint32_t kRasterizerOptions =
    AnalyticRasterizer::kOptionBandOffset     |
    AnalyticRasterizer::kOptionRecordMinXMaxX ;

  // Can only be called if there is something to fill.
  const EdgeStorage<int>* edgeStorage = command.analyticEdgesSync();
  BL_ASSERT(edgeStorage != nullptr);

  // NOTE: This doesn't happen often, but it's possible. If, for any reason, the data in bands is all horizontal
  // lines or no data at all it would trigger this check.
  if (BL_UNLIKELY(edgeStorage->boundingBox().y0 >= edgeStorage->boundingBox().y1))
    return BL_SUCCESS;

  uint32_t bandHeight = edgeStorage->bandHeight();
  uint32_t bandHeightMask = bandHeight - 1;

  const uint32_t yStart = (uint32_t(edgeStorage->boundingBox().y0)                            ) >> BLPipeline::A8Info::kShift;
  const uint32_t yEnd   = (uint32_t(edgeStorage->boundingBox().y1) + BLPipeline::A8Info::kMask) >> BLPipeline::A8Info::kShift;

  size_t requiredWidth = BLIntOps::alignUp(uint32_t(workData.dstSize().w) + 1u + BL_PIPE_PIXELS_PER_ONE_BIT, BL_PIPE_PIXELS_PER_ONE_BIT);
  size_t requiredHeight = bandHeight;
  size_t cellAlignment = 16;

  size_t bitStride = BLIntOps::wordCountFromBitCount<BLBitWord>(requiredWidth / BL_PIPE_PIXELS_PER_ONE_BIT) * sizeof(BLBitWord);
  size_t cellStride = requiredWidth * sizeof(uint32_t);

  size_t bitsStart = 0;
  size_t bitsSize = requiredHeight * bitStride;

  size_t cellsStart = BLIntOps::alignUp(bitsStart + bitsSize, cellAlignment);
  size_t cellsSize = requiredHeight * cellStride;

  BL_PROPAGATE(workData.zeroBuffer.ensure(cellsStart + cellsSize));

  AnalyticCellStorage cellStorage;
  cellStorage.init(
    reinterpret_cast<BLBitWord*>(workData.zeroBuffer.data + bitsStart), bitStride,
    BLIntOps::alignUp(reinterpret_cast<uint32_t*>(workData.zeroBuffer.data + cellsStart), cellAlignment), cellStride);

  AnalyticActiveEdge<int>* active = nullptr;
  AnalyticActiveEdge<int>* pooled = nullptr;

  EdgeList<int>* bandEdges = edgeStorage->bandEdges();
  uint32_t bandId = edgeStorage->bandStartFromBBox();
  uint32_t bandEnd = edgeStorage->bandEndFromBBox();

  uint32_t dstWidth = uint32_t(workData.dstSize().w);

  BLPipeline::FillFunc fillFunc = command.pipeDispatchData()->fillFunc;
  BLPipeline::FillData fillData;

  fillData.initAnalytic(command.alpha(),
                        command.analyticFillRule(),
                        cellStorage.bitPtrTop, cellStorage.bitStride,
                        cellStorage.cellPtrTop, cellStorage.cellStride);

  AnalyticRasterizer ras;
  ras.init(cellStorage.bitPtrTop, cellStorage.bitStride,
           cellStorage.cellPtrTop, cellStorage.cellStride,
           bandId * bandHeight, bandHeight);
  ras._bandOffset = yStart;

  BLArenaAllocator* workZone = &workData.workZone;
  const void* fetchData = command.getPipeFetchData();

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
      fillData.analytic.box.x1 = int(blMin(dstWidth, BLIntOps::alignUp(ras._cellMaxX + 1, BL_PIPE_PIXELS_PER_ONE_BIT)));
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
} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RENDERCOMMANDPROCSYNC_P_H_INCLUDED
