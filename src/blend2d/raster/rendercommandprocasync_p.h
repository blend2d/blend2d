// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RENDERCOMMANDPROCASYNC_P_H_INCLUDED
#define BLEND2D_RASTER_RENDERCOMMANDPROCASYNC_P_H_INCLUDED

#include "../raster/edgebuilder_p.h"
#include "../raster/renderbatch_p.h"
#include "../raster/rendercommandprocsync_p.h"
#include "../raster/workdata_p.h"
#include "../support/bitops_p.h"
#include "../support/intops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace bl {
namespace RasterEngine {
namespace CommandProcAsync {

enum class CommandStatus : uint32_t {
  kContinue = 0,
  kDone = 1
};

struct SlotData {
  struct Analytic {
    const EdgeVector<int>* edges;
    AnalyticActiveEdge<int>* active;
  };

  union {
    Analytic analytic;
  };
};

class ProcData {
public:
  typedef PrivateBitWordOps BitOps;

  //! \name Members
  //! \{

  WorkData* _workData;
  RenderBatch* _batch;

  uint32_t _bandY0;
  uint32_t _bandY1;
  uint32_t _bandFixedY0;
  uint32_t _bandFixedY1;

  SlotData* _stateSlotData;
  size_t _stateSlotCount;

  BLBitWord* _pendingCommandBitSetData;
  size_t _pendingCommandBitSetSize;
  BLBitWord _pendingCommandBitSetMask;

  AnalyticActiveEdge<int>* _pooledEdges;

  //! \}

  //! \name Construction & Destruction
  //! \{

  BL_INLINE ProcData(WorkData* workData, RenderBatch* batch) noexcept
    : _workData(workData),
      _batch(batch),
      _bandY0(0),
      _bandY1(0),
      _bandFixedY0(0),
      _bandFixedY1(0),
      _stateSlotData(nullptr),
      _stateSlotCount(0),
      _pendingCommandBitSetData(nullptr),
      _pendingCommandBitSetSize(0),
      _pendingCommandBitSetMask(0),
      _pooledEdges(nullptr) {}

  //! \}

  //! \name Initialization
  //! \{

  BL_INLINE BLResult initProcData() noexcept {
    size_t commandCount = _batch->commandCount();
    size_t stateSlotCount = _batch->stateSlotCount();

    size_t bitWordCount = IntOps::wordCountFromBitCount<BLBitWord>(commandCount);
    size_t remainingBits = commandCount & (IntOps::bitSizeOf<BLBitWord>() - 1);

    _stateSlotData = _workData->workZone.allocT<SlotData>(stateSlotCount * sizeof(SlotData));
    _pendingCommandBitSetData = _workData->workZone.allocT<BLBitWord>(bitWordCount * sizeof(BLBitWord), sizeof(BLBitWord));

    if (!_stateSlotData || !_pendingCommandBitSetData)
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    _stateSlotCount = stateSlotCount;
    _pendingCommandBitSetSize = bitWordCount;

    // Initialize the last BitWord as it can have bits that are outside of the command count. We rely on these bits,
    // they cannot be wrong...
    if (remainingBits)
      _pendingCommandBitSetData[bitWordCount - 1] = BitOps::nonZeroStartMask(remainingBits);
    else
      _pendingCommandBitSetData[bitWordCount - 1] = BitOps::ones();

    if (bitWordCount > 1)
      _pendingCommandBitSetMask = IntOps::allOnes<BLBitWord>();
    else
      _pendingCommandBitSetMask = 0;

    return BL_SUCCESS;
  }

  BL_INLINE void initBand(uint32_t bandId, uint32_t bandHeight, uint32_t fpScale) noexcept {
    _bandY0 = bandId * bandHeight;
    _bandY1 = _bandY0 + bandHeight;
    _bandFixedY0 = _bandY0 * fpScale;
    _bandFixedY1 = _bandY1 * fpScale;
  }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE WorkData* workData() const noexcept { return _workData; }
  BL_INLINE RenderBatch* batch() const noexcept { return _batch; }

  BL_INLINE uint32_t bandY0() const noexcept { return _bandY0; }
  BL_INLINE uint32_t bandY1() const noexcept { return _bandY1; }
  BL_INLINE uint32_t bandFixedY0() const noexcept { return _bandFixedY0; }
  BL_INLINE uint32_t bandFixedY1() const noexcept { return _bandFixedY1; }

  BL_INLINE BLBitWord* pendingCommandBitSetData() const noexcept { return _pendingCommandBitSetData; }
  BL_INLINE BLBitWord* pendingCommandBitSetEnd() const noexcept { return _pendingCommandBitSetData + _pendingCommandBitSetSize; }
  BL_INLINE size_t pendingCommandBitSetSize() const noexcept { return _pendingCommandBitSetSize; }

  BL_INLINE BLBitWord pendingCommandBitSetMask() const noexcept { return _pendingCommandBitSetMask; }
  BL_INLINE void clearPendingCommandBitSetMask() noexcept { _pendingCommandBitSetMask = 0; }

  BL_INLINE SlotData& stateDataAt(size_t index) noexcept {
    BL_ASSERT(index < _stateSlotCount);
    return _stateSlotData[index];
  }

  //! \}
};

static BL_INLINE CommandStatus fillBoxA(ProcData& procData, const RenderCommand& command) noexcept {
  int y0 = blMax(command.boxI().y0, int(procData.bandY0()));
  int y1 = blMin(command.boxI().y1, int(procData.bandY1()));

  if (y0 < y1) {
    Pipeline::FillData fillData;
    fillData.initBoxA8bpc(command.alpha(), command.boxI().x0, y0, command.boxI().x1, y1);

    Pipeline::FillFunc fillFunc = command.pipeDispatchData()->fillFunc;
    Pipeline::FetchFunc fetchFunc = command.pipeDispatchData()->fetchFunc;
    const void* fetchData = command.getPipeFetchData();

    if (fetchFunc == nullptr) {
      fillFunc(&procData.workData()->ctxData, &fillData, fetchData);
    }
    else {
      // TODO:
    }
  }

  return CommandStatus(command.boxI().y1 <= int(procData.bandY1()));
}

static BL_INLINE CommandStatus fillBoxU(ProcData& procData, const RenderCommand& command) noexcept {
  int y0 = blMax(command.boxI().y0, int(procData.bandFixedY0()));
  int y1 = blMin(command.boxI().y1, int(procData.bandFixedY1()));

  if (y0 < y1) {
    Pipeline::FillData fillData;
    Pipeline::BoxUToMaskData boxUToMaskData;

    if (fillData.initBoxU8bpc24x8(command.alpha(), command.boxI().x0, y0, command.boxI().x1, y1, boxUToMaskData)) {
      Pipeline::FillFunc fillFunc = command.pipeDispatchData()->fillFunc;
      Pipeline::FetchFunc fetchFunc = command.pipeDispatchData()->fetchFunc;
      const void* fetchData = command.getPipeFetchData();

      if (fetchFunc == nullptr) {
        fillFunc(&procData.workData()->ctxData, &fillData, fetchData);
      }
      else {
        // TODO:
      }
    }
  }

  return CommandStatus(command.boxI().y1 <= int(procData.bandFixedY1()));
}

static CommandStatus fillBoxMaskA(ProcData& procData, const RenderCommand& command) noexcept {
  const RenderCommand::FillBoxMaskA& payload = command._payload.boxMaskA;
  const BLBoxI& boxI = payload.boxI;

  int y0 = blMax(boxI.y0, int(procData.bandY0()));
  int y1 = blMin(boxI.y1, int(procData.bandY1()));

  if (y0 < y1) {
    uint32_t maskX = payload.maskOffsetI.x;
    uint32_t maskY = payload.maskOffsetI.y + (y0 - boxI.y0);

    const BLImageImpl* maskI = payload.maskImageI.ptr;
    const uint8_t* maskData = static_cast<const uint8_t*>(maskI->pixelData) + maskY * maskI->stride + maskX * (maskI->depth / 8u);

    Pipeline::MaskCommand maskCommands[2];
    Pipeline::MaskCommandType vMaskCmd = command.alpha() >= 255 ? Pipeline::MaskCommandType::kVMaskA8WithoutGA : Pipeline::MaskCommandType::kVMaskA8WithGA;

    maskCommands[0].initVMask(vMaskCmd, uint32_t(boxI.x0), uint32_t(boxI.x1), maskData, maskI->stride);
    maskCommands[1].initRepeat();

    Pipeline::FillData fillData;
    fillData.initMaskA(command.alpha(), boxI.x0, y0, boxI.x1, y1, maskCommands);

    Pipeline::FillFunc fillFunc = command.pipeDispatchData()->fillFunc;
    Pipeline::FetchFunc fetchFunc = command.pipeDispatchData()->fetchFunc;
    const void* fetchData = command.getPipeFetchData();

    if (fetchFunc == nullptr) {
      fillFunc(&procData.workData()->ctxData, &fillData, fetchData);
    }
    else {
      // TODO:
    }
  }

  return CommandStatus(boxI.y1 <= int(procData.bandY1()));
}

static CommandStatus fillAnalytic(ProcData& procData, const RenderCommand& command, int32_t prevBandFy1, int32_t nextBandFy0) noexcept {
  // Rasterizer options to use - do not change unless you are improving the existing rasterizers.
  constexpr uint32_t kRasterizerOptions =
    AnalyticRasterizer::kOptionBandOffset     |
    AnalyticRasterizer::kOptionRecordMinXMaxX ;

  WorkData& workData = *procData.workData();
  SlotData::Analytic& procState = procData.stateDataAt(command._payload.analytic.stateSlotIndex).analytic;

  uint32_t bandFixedY0 = procData.bandFixedY0();
  uint32_t bandFixedY1 = procData.bandFixedY1();

  const EdgeVector<int>* edges;
  AnalyticActiveEdge<int>* active;

  // TODO:
  blUnused(nextBandFy0);

  {
    int32_t cmdFy0 = command._payload.analytic.fixedY0;
    bool isFirstBand = prevBandFy1 < cmdFy0;

    if (isFirstBand) {
      // If it's the first band we have to initialize the state. This must be done only once per command.
      edges = command.analyticEdges();
      active = nullptr;

      procState.edges = edges;
      procState.active = active;

      // Everything clipped out, or all lines horizontal, etc...
      if (!edges)
        return CommandStatus::kDone;
    }
    else {
      // If the state has been already initialized, we have to take the remaining `edges` and `active` ones from it.
      edges = procState.edges;
      active = procState.active;
    }

    // Don't do anything if we haven't advanced enough.
    if (uint32_t(cmdFy0) >= bandFixedY1) {
      return CommandStatus::kContinue;
    }
  }

  uint32_t bandY0 = procData.bandY0();
  uint32_t bandY1 = procData.bandY1();
  uint32_t bandHeight = workData.bandHeight();

  // TODO:
  /*
  if (BL_UNLIKELY(edgeStorage->boundingBox().y0 >= edgeStorage->boundingBox().y1))
    return BL_SUCCESS;
  */

  uint32_t dstWidth = uint32_t(workData.dstSize().w);
  size_t requiredWidth = IntOps::alignUp(dstWidth + 1u + BL_PIPE_PIXELS_PER_ONE_BIT, BL_PIPE_PIXELS_PER_ONE_BIT);
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

  AnalyticActiveEdge<int>* pooled = procData._pooledEdges;

  Pipeline::FillData fillData;
  fillData.initAnalytic(command.alpha(),
                        command.analyticFillRule(),
                        cellStorage.bitPtrTop, cellStorage.bitStride,
                        cellStorage.cellPtrTop, cellStorage.cellStride);

  AnalyticRasterizer ras;
  ras.init(cellStorage.bitPtrTop, cellStorage.bitStride,
           cellStorage.cellPtrTop, cellStorage.cellStride,
           bandY0, bandHeight);

  ArenaAllocator* workZone = &workData.workZone;

  AnalyticActiveEdge<int>** pPrev = &active;
  AnalyticActiveEdge<int>* current = *pPrev;

  ras.resetBounds();
  ras._bandEnd = bandY1 - 1;

  while (current) {
    // Skipped.
    ras.setSignMaskFromBit(current->signBit);
    if (current->state._ey1 < int(bandY0))
      goto EdgeDone;
    ras.restore(current->state);

    // Important - since we only process a single band here we have to skip into the correct band as it's not
    // guaranteed that the next band would be consecutive.
AdvanceY:
    ras.advanceToY(int(bandY0));

    for (;;) {
Rasterize:
      if (ras.template rasterize<kRasterizerOptions | AnalyticRasterizer::kOptionBandingMode>()) {
        // The edge is fully rasterized.
EdgeDone:
        const EdgePoint<int>* pts = current->cur;
        while (pts != current->end) {
          pts++;
          if (pts[-1].y <= int(bandFixedY0) || !ras.prepare(pts[-2], pts[-1]))
            continue;

          current->cur = pts;
          if (uint32_t(ras._ey0) > ras._bandEnd)
            goto SaveState;

          if (ras._ey0 < int(bandY0))
            goto AdvanceY;

          goto Rasterize;
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
      if (BL_UNLIKELY(!pooled)) {
        // Failed to allocate memory for the current edge.
        procData.workData()->accumulateErrorFlag(BL_CONTEXT_ERROR_FLAG_OUT_OF_MEMORY);
        return CommandStatus::kDone;
      }
      pooled->next = nullptr;
    }

    do {
      const EdgePoint<int>* pts = edges->pts + 1;
      const EdgePoint<int>* end = edges->pts + edges->count;

      if (pts[-1].y >= int(bandFixedY1))
        break;

      uint32_t signBit = edges->signBit;
      ras.setSignMaskFromBit(signBit);

      edges = edges->next;
      if (end[-1].y <= int(bandFixedY0))
        continue;

      do {
        pts++;
        if (pts[-1].y <= int(bandFixedY0) || !ras.prepare(pts[-2], pts[-1]))
          continue;

        ras.advanceToY(int(bandY0));
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

          if (uint32_t(ras._ey0) > ras._bandEnd)
            goto SaveState;

          if (ras._ey0 < int(bandY0))
            goto AdvanceY;

          goto Rasterize;
        }
      } while (pts != end);
    } while (edges);
  }

  // Makes `active` or the last `AnalyticActiveEdge->next` null. It's important, because we don't unlink during
  // edge pooling as it's just faster to do it here.
  *pPrev = nullptr;

  // Pooled active edges can be reused, we cannot return them to the allocator.
  procData._pooledEdges = pooled;
  procState.edges = edges;
  procState.active = active;

  if (ras.hasBounds()) {
    Pipeline::FillFunc fillFunc = command.pipeDispatchData()->fillFunc;
    Pipeline::FetchFunc fetchFunc = command.pipeDispatchData()->fetchFunc;
    const void* fetchData = command.getPipeFetchData();

    fillData.analytic.box.x0 = int(ras._cellMinX);
    fillData.analytic.box.x1 = int(blMin(dstWidth, IntOps::alignUp(ras._cellMaxX + 1, BL_PIPE_PIXELS_PER_ONE_BIT)));
    fillData.analytic.box.y0 = int(ras._bandOffset);
    fillData.analytic.box.y1 = int(ras._bandEnd) + 1;

    if (fetchFunc == nullptr) {
      fillFunc(&workData.ctxData, &fillData, fetchData);
    }
    else {
      // TODO:
    }
  }

  return CommandStatus(!edges && !active);
}

static CommandStatus processCommand(ProcData& procData, const RenderCommand& command, int32_t prevBandFy1, int32_t nextBandFy0) noexcept {
  switch (command.type()) {
    case RenderCommandType::kFillBoxA:
      return fillBoxA(procData, command);

    case RenderCommandType::kFillBoxU:
      return fillBoxU(procData, command);

    case RenderCommandType::kFillAnalytic:
      return fillAnalytic(procData, command, prevBandFy1, nextBandFy0);

    case RenderCommandType::kFillBoxMaskA:
      return fillBoxMaskA(procData, command);

    default:
      return CommandStatus::kDone;
  }
}

} // {CommandProcAsync}
} // {RasterEngine}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RENDERCOMMANDPROCASYNC_P_H_INCLUDED
