// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_RASTERCOMMANDPROCASYNC_P_H
#define BLEND2D_RASTER_RASTERCOMMANDPROCASYNC_P_H

#include "../bitops_p.h"
#include "../support_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rastercommandprocsync_p.h"
#include "../raster/rasterworkbatch_p.h"
#include "../raster/rasterworkdata_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [BLRasterWorkProcAsyncState]
// ============================================================================

struct BLRasterWorkProcAsyncState {
  struct Analytic {
    const BLEdgeVector<int>* edges;
    BLActiveEdge<int>* active;
  };

  union {
    Analytic analytic;
  };
};

// ============================================================================
// [BLRasterCommandProcAsyncData]
// ============================================================================

class BLRasterCommandProcAsyncData {
public:
  typedef BLPrivateBitOps<BLBitWord> BitOps;

  BLRasterWorkData* _workData;
  BLRasterWorkBatch* _batch;

  uint32_t _bandY0;
  uint32_t _bandY1;
  uint32_t _bandFixedY0;
  uint32_t _bandFixedY1;

  BLRasterWorkProcAsyncState* _stateSlotData;
  size_t _stateSlotCount;

  BLBitWord* _pendingCommandBitSetData;
  size_t _pendingCommandBitSetSize;
  BLBitWord _pendingCommandBitSetMask;

  BLActiveEdge<int>* _pooledEdges;

  BL_INLINE BLRasterCommandProcAsyncData(BLRasterWorkData* workData) noexcept
    : _workData(workData),
      _batch(workData->batch),
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

  //! \name Initialization
  //! \{

  BL_INLINE BLResult initProcData() noexcept {
    size_t commandCount = _batch->commandCount();
    size_t stateSlotCount = _batch->stateSlotCount();

    size_t bitWordCount = blBitWordCountFromBitCount<BLBitWord>(commandCount);
    size_t remainingBits = commandCount & (blBitSizeOf<BLBitWord>() - 1);

    _stateSlotData = _workData->workZone.allocT<BLRasterWorkProcAsyncState>(stateSlotCount * sizeof(BLRasterWorkProcAsyncState));
    _pendingCommandBitSetData = _workData->workZone.allocT<BLBitWord>(bitWordCount * sizeof(BLBitWord), sizeof(BLBitWord));

    if (!_stateSlotData || !_pendingCommandBitSetData)
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    _stateSlotCount = stateSlotCount;
    _pendingCommandBitSetSize = bitWordCount;

    // Initialize the last BitWord as it can have bits that are outside of
    // the command count. We rely on these bits, they cannot be wrong...
    if (remainingBits)
      _pendingCommandBitSetData[bitWordCount - 1] = BitOps::nonZeroBitMask(remainingBits);
    else
      _pendingCommandBitSetData[bitWordCount - 1] = BitOps::ones();

    if (bitWordCount > 1)
      _pendingCommandBitSetMask = blBitOnes<BLBitWord>();
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

  BL_INLINE BLRasterWorkData* workData() const noexcept { return _workData; }
  BL_INLINE BLRasterWorkBatch* batch() const noexcept { return _batch; }

  BL_INLINE uint32_t bandY0() const noexcept { return _bandY0; }
  BL_INLINE uint32_t bandY1() const noexcept { return _bandY1; }
  BL_INLINE uint32_t bandFixedY0() const noexcept { return _bandFixedY0; }
  BL_INLINE uint32_t bandFixedY1() const noexcept { return _bandFixedY1; }

  BL_INLINE BLBitWord* pendingCommandBitSetData() const noexcept { return _pendingCommandBitSetData; }
  BL_INLINE BLBitWord* pendingCommandBitSetEnd() const noexcept { return _pendingCommandBitSetData + _pendingCommandBitSetSize; }
  BL_INLINE size_t pendingCommandBitSetSize() const noexcept { return _pendingCommandBitSetSize; }

  BL_INLINE BLBitWord pendingCommandBitSetMask() const noexcept { return _pendingCommandBitSetMask; }
  BL_INLINE void clearPendingCommandBitSetMask() noexcept { _pendingCommandBitSetMask = 0; }

  BL_INLINE BLRasterWorkProcAsyncState& stateDataAt(size_t index) noexcept {
    BL_ASSERT(index < _stateSlotCount);
    return _stateSlotData[index];
  }

  //! \}
};

// ============================================================================
// [blRasterCommandProcAsync - FillBoxA]
// ============================================================================

static BL_INLINE bool blRasterCommandProcAsync_FillBoxA(BLRasterCommandProcAsyncData& procData, const BLRasterCommand& command) noexcept {
  int y0 = blMax(command.boxI().y0, int(procData.bandY0()));
  int y1 = blMin(command.boxI().y1, int(procData.bandY1()));

  if (y0 < y1) {
    BLPipeFillData fillData;
    fillData.initBoxA8bpc(command.alpha(), command.boxI().x0, y0, command.boxI().x1, y1);

    BLPipeFillFunc fillFunc = command.fillFunc();
    const void* fetchData = command.getPipeFetchData();

    fillFunc(&procData.workData()->ctxData, &fillData, fetchData);
  }

  return command.boxI().y1 <= int(procData.bandY1());
}

// ============================================================================
// [blRasterCommandProcAsync - FillBoxU]
// ============================================================================

static BL_INLINE bool blRasterCommandProcAsync_FillBoxU(BLRasterCommandProcAsyncData& procData, const BLRasterCommand& command) noexcept {
  int y0 = blMax(command.boxI().y0, int(procData.bandFixedY0()));
  int y1 = blMin(command.boxI().y1, int(procData.bandFixedY1()));

  if (y0 < y1) {
    BLPipeFillData fillData;
    if (fillData.initBoxU8bpc24x8(command.alpha(), command.boxI().x0, y0, command.boxI().x1, y1)) {
      BLPipeFillFunc fillFunc = command.fillFunc();
      const void* fetchData = command.getPipeFetchData();

      fillFunc(&procData.workData()->ctxData, &fillData, fetchData);
    }
  }

  return command.boxI().y1 <= int(procData.bandFixedY1());
}

// ============================================================================
// [blRasterCommandProcAsync - FillAnalytic]
// ============================================================================

static bool blRasterCommandProcAsync_FillAnalytic(BLRasterCommandProcAsyncData& procData, const BLRasterCommand& command, bool isInitialBand) noexcept {
  // Rasterizer options to use - do not change unless you are improving
  // the existing rasterizers.
  constexpr uint32_t kRasterizerOptions =
    BLAnalyticRasterizer::kOptionBandOffset     |
    BLAnalyticRasterizer::kOptionRecordMinXMaxX ;

  BLRasterWorkData& workData = *procData.workData();
  BLRasterWorkProcAsyncState::Analytic& procState = procData.stateDataAt(command._analyticAsync.stateSlotIndex).analytic;

  int bandFixedY0 = procData.bandFixedY0();
  int bandFixedY1 = procData.bandFixedY1();

  const BLEdgeVector<int>* edges;
  BLActiveEdge<int>* active;

  if (isInitialBand) {
    edges = command.analyticEdgesAsync();
    active = nullptr;

    // Everything clipped out, or all lines horizontal, etc...
    if (!edges)
      return true;

    // Don't do anything if we haven't advanced enough.
    if (command._analyticAsync.fixedY0 >= bandFixedY1) {
      procState.edges = edges;
      procState.active = active;
      return false;
    }
  }
  else {
    // Don't do anything if we haven't advanced enough.
    if (command._analyticAsync.fixedY0 >= bandFixedY1)
      return false;

    edges = procState.edges;
    active = procState.active;
  }

  int bandY0 = procData.bandY0();
  int bandY1 = procData.bandY1();
  uint32_t bandHeight = workData.bandHeight();

  // TODO:
  /*
  if (BL_UNLIKELY(edgeStorage->boundingBox().y0 >= edgeStorage->boundingBox().y1))
    return BL_SUCCESS;
  */

  uint32_t dstWidth = uint32_t(workData.dstSize().w);
  size_t requiredWidth = blAlignUp(dstWidth + 1u + BL_PIPE_PIXELS_PER_ONE_BIT, BL_PIPE_PIXELS_PER_ONE_BIT);
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

  BLActiveEdge<int>* pooled = procData._pooledEdges;

  BLPipeFillFunc fillFunc = command.fillFunc();
  BLPipeFillData fillData;

  fillData.initAnalytic(command.alpha(),
                        command.analyticFillRule(),
                        cellStorage.bitPtrTop, cellStorage.bitStride,
                        cellStorage.cellPtrTop, cellStorage.cellStride);

  BLAnalyticRasterizer ras;
  ras.init(cellStorage.bitPtrTop, cellStorage.bitStride,
           cellStorage.cellPtrTop, cellStorage.cellStride,
           bandY0, bandHeight);

  BLZoneAllocator* workZone = &workData.workZone;
  const void* fetchData = command.getPipeFetchData();

  BLActiveEdge<int>** pPrev = &active;
  BLActiveEdge<int>* current = *pPrev;

  ras.resetBounds();
  ras._bandEnd = bandY1 - 1;

  while (current) {
    // Skipped.
    ras.setSignMaskFromBit(current->signBit);
    if (current->state._ey1 < bandY0)
      goto EdgeDone;
    ras.restore(current->state);

    // Important - since we only process a single band here we have to skip
    // into the correct band as it's not guaranteed that the next band would
    // be consecutive.
AdvanceY:
    ras.advanceToY(bandY0);

    for (;;) {
Rasterize:
      if (ras.template rasterize<kRasterizerOptions | BLAnalyticRasterizer::kOptionBandingMode>()) {
        // The edge is fully rasterized.
EdgeDone:
        const BLEdgePoint<int>* pts = current->cur;
        while (pts != current->end) {
          pts++;
          if (pts[-1].y <= bandFixedY0 || !ras.prepare(pts[-2], pts[-1]))
            continue;

          current->cur = pts;
          if (uint32_t(ras._ey0) > ras._bandEnd)
            goto SaveState;

          if (ras._ey0 < bandY0)
            goto AdvanceY;

          goto Rasterize;
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

      if (pts[-1].y >= bandFixedY1)
        break;

      uint32_t signBit = edges->signBit;
      ras.setSignMaskFromBit(signBit);

      edges = edges->next;
      if (end[-1].y <= bandFixedY0)
        continue;

      do {
        pts++;
        if (pts[-1].y <= bandFixedY0 || !ras.prepare(pts[-2], pts[-1]))
          continue;

        ras.advanceToY(bandY0);
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

          if (ras._ey0 < bandY0)
            goto AdvanceY;

          goto Rasterize;
        }
      } while (pts != end);
    } while (edges);
  }

  // Makes `active` or the last `BLActiveEdge->next` null. It's important,
  // because we don't unlink during edge pooling as it's just faster to do
  // it here.
  *pPrev = nullptr;

  // Pooled active edges can be reused, we cannot return them to the allocator.
  procData._pooledEdges = pooled;
  procState.edges = edges;
  procState.active = active;

  if (ras.hasBounds()) {
    fillData.analytic.box.x0 = int(ras._cellMinX);
    fillData.analytic.box.x1 = int(blMin(dstWidth, blAlignUp(ras._cellMaxX + 1, BL_PIPE_PIXELS_PER_ONE_BIT)));
    fillData.analytic.box.y0 = int(ras._bandOffset);
    fillData.analytic.box.y1 = int(ras._bandEnd) + 1;
    fillFunc(&workData.ctxData, &fillData, fetchData);
  }

  return !edges && !active;
}

// ============================================================================
// [BLRasterWorkProcessCommand]
// ============================================================================

static BL_NOINLINE bool blRasterCommandProcAsync(BLRasterCommandProcAsyncData& procData, const BLRasterCommand& command, bool isInitialBand) noexcept {
  switch (command.type()) {
    case BL_RASTER_COMMAND_TYPE_FILL_BOX_A:
      return blRasterCommandProcAsync_FillBoxA(procData, command);

    case BL_RASTER_COMMAND_TYPE_FILL_BOX_U:
      return blRasterCommandProcAsync_FillBoxU(procData, command);

    case BL_RASTER_COMMAND_TYPE_FILL_ANALYTIC_NON_ZERO:
    case BL_RASTER_COMMAND_TYPE_FILL_ANALYTIC_EVEN_ODD:
      return blRasterCommandProcAsync_FillAnalytic(procData, command, isInitialBand);

    default:
      return true;
  }
}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERCOMMANDPROCASYNC_P_H
