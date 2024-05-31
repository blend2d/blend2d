// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_REFERENCE_FILLGENERIC_P_H_INCLUDED
#define BLEND2D_PIPELINE_REFERENCE_FILLGENERIC_P_H_INCLUDED

#include "../../pipeline/pipedefs_p.h"
#include "../../pipeline/reference/pixelbufferptr_p.h"
#include "../../support/bitops_p.h"
#include "../../support/intops_p.h"
#include "../../support/ptrops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_reference
//! \{

namespace bl {
namespace Pipeline {
namespace Reference {
namespace {

template<typename CompOp>
struct FillBoxA_Base {
  static void BL_CDECL fillFunc(ContextData* ctxData, const void* fillData_, const void* fetchData_) noexcept {
    const FillData::BoxA* fillData = static_cast<const FillData::BoxA*>(fillData_);

    intptr_t dstStride = ctxData->dst.stride;
    uint8_t* dstPtr = static_cast<uint8_t*>(ctxData->dst.pixelData);

    uint32_t y0 = uint32_t(fillData->box.y0);
    dstPtr += intptr_t(y0) * dstStride;

    uint32_t x0 = uint32_t(fillData->box.x0);
    dstPtr += size_t(x0) * CompOp::kDstBPP;

    uint32_t w = uint32_t(fillData->box.x1) - uint32_t(fillData->box.x0);
    uint32_t h = uint32_t(fillData->box.y1) - uint32_t(fillData->box.y0);

    dstStride -= intptr_t(size_t(w) * CompOp::kDstBPP);
    uint32_t msk = fillData->alpha.u;

    CompOp compOp;
    compOp.rectInitFetch(ctxData, fetchData_, x0, y0, w);

    BL_ASSUME(h > 0);
    if (CompOp::kOptimizeOpaque && msk == 255) {
      while (h) {
        compOp.rectStartX(x0);
        dstPtr = compOp.compositeCSpanOpaque(dstPtr, w);
        dstPtr += dstStride;
        compOp.advanceY();
        h--;
      }
    }
    else {
      while (h) {
        compOp.rectStartX(x0);
        dstPtr = compOp.compositeCSpanMasked(dstPtr, w, msk);
        dstPtr += dstStride;
        compOp.advanceY();
        h--;
      }
    }
  }
};

template<typename CompOp>
struct FillMask_Base {
  static void BL_CDECL fillFunc(ContextData* ctxData, const void* fillData_, const void* fetchData_) noexcept {
    const FillData::Mask* fillData = static_cast<const FillData::Mask*>(fillData_);

    uint8_t* dstPtr = static_cast<uint8_t*>(ctxData->dst.pixelData);
    intptr_t dstStride = ctxData->dst.stride;

    uint32_t y0 = uint32_t(fillData->box.y0);
    dstPtr += intptr_t(y0) * dstStride;

    CompOp compOp;
    compOp.spanInitY(ctxData, fetchData_, y0);

    uint32_t alpha = fillData->alpha.u;
    MaskCommand* cmdPtr = fillData->maskCommandData;

    uint32_t h = uint32_t(fillData->box.y1) - y0;

    for (;;) {
      uint32_t x1AndType = cmdPtr->_x1AndType;
      uint32_t x = cmdPtr->x0();

      MaskCommand* cmdBegin = cmdPtr;

      // This is not really common to not be true, however, it's possible to skip entire scanlines
      // with kEndOrRepeat command, which is zero.
      if (BL_LIKELY((x1AndType & MaskCommand::kTypeMask) != 0u)) {
        compOp.spanStartX(x);
        dstPtr += size_t(x) * CompOp::kDstBPP;

        uint32_t i = x1AndType >> MaskCommand::kTypeBits;
        MaskCommandType cmdType = MaskCommandType(x1AndType & MaskCommand::kTypeMask);

        i -= x;
        x += i;

        uintptr_t maskValue = cmdPtr->_value.data;
        cmdPtr++;

        for (;;) {
          if (cmdType == MaskCommandType::kCMask) {
            BL_ASSUME(maskValue <= 255);
            dstPtr = compOp.compositeCSpan(dstPtr, i, uint32_t(maskValue));
          }
          else {
            // Increments the advance in the mask command in case it would be repeated.
            cmdPtr[-1]._value.data = maskValue + uintptr_t(cmdPtr[-1].maskAdvance());
            if (cmdType == MaskCommandType::kVMaskA8WithoutGA) {
              dstPtr = compOp.compositeVSpanWithoutGA(dstPtr, reinterpret_cast<const uint8_t*>(maskValue), alpha, i);
            }
            else {
              dstPtr = compOp.compositeVSpanWithGA(dstPtr, reinterpret_cast<const uint8_t*>(maskValue), i);
            }
          }

          x1AndType = cmdPtr->_x1AndType;

          // Terminates this command span.
          if ((x1AndType & MaskCommand::kTypeMask) == 0u)
            break;

          uint32_t x0 = cmdPtr->x0();
          if (x != x0) {
            compOp.spanAdvanceX(x0, x0 - x);
            x = x0;
          }

          i = (x1AndType >> MaskCommand::kTypeBits) - x;
          x += i;
          cmdType = MaskCommandType(x1AndType & MaskCommand::kTypeMask);

          maskValue = cmdPtr->_value.data;
          cmdPtr++;
        }

        compOp.spanEndX(x);
        dstPtr -= size_t(x) * CompOp::kDstBPP;
      }

      uint32_t repeatCount = cmdPtr->repeatCount();
      if (--h == 0)
        break;

      cmdPtr++;
      dstPtr += dstStride;

      compOp.advanceY();
      repeatCount--;
      cmdPtr[-1].updateRepeatCount(repeatCount);

      if (repeatCount != 0)
        cmdPtr = cmdBegin;
    }
  }
};

template<typename CompOp>
struct FillAnalytic_Base {
  enum : uint32_t {
    kDstBPP = CompOp::kDstBPP,
    kPixelsPerOneBit = 4,
    kPixelsPerBitWord = kPixelsPerOneBit * IntOps::bitSizeOf<BLBitWord>()
  };

  typedef PrivateBitWordOps BitOps;

  static void BL_CDECL fillFunc(ContextData* ctxData, const void* fillData_, const void* fetchData_) noexcept {
    const FillData::Analytic* fillData = static_cast<const FillData::Analytic*>(fillData_);

    uint32_t y = uint32_t(fillData->box.y0);
    intptr_t dstStride = ctxData->dst.stride;
    uint8_t* dstPtr = static_cast<uint8_t*>(ctxData->dst.pixelData) + intptr_t(y) * dstStride;

    BLBitWord* bitPtr = fillData->bitTopPtr;
    BLBitWord* bitPtrEnd = nullptr;
    uint32_t* cellPtr = fillData->cellTopPtr;

    size_t bitStride = fillData->bitStride;
    size_t cellStride = fillData->cellStride;

    uint32_t globalAlpha = fillData->alpha.u;
    uint32_t fillRuleMask = fillData->fillRuleMask;

    CompOp compOp;
    compOp.spanInitY(ctxData, fetchData_, y);

    y = uint32_t(fillData->box.y1) - y;

    size_t x0;
    size_t xEnd = fillData->box.x1;
    size_t xOff;

    size_t i;
    uint32_t cov;
    uint32_t msk;

    BLBitWord bitWord;
    BLBitWord bitWordTmp;

    goto L_Scanline_Init;

    // BitScan
    // -------

    // Called by Scanline iterator on the first non-zero BitWord it matches. The responsibility of BitScan is to find
    // the first bit in the passed BitWord followed by matching the bit that ends this match. This would essentially
    // produce the first [x0, x1) span that has to be composited as 'VMask' loop.
L_BitScan_Init:
    x0 = BitOps::countZerosFromStart(bitWord);
    bitPtr[-1] = 0;
    bitWordTmp = BitOps::shiftToEnd(BitOps::ones(), x0);
    x0 = x0 * kPixelsPerOneBit + xOff;

    // Load the given cells to `m0` and clear the BitWord and all cells it represents in memory. This is important as
    // the compositor has to clear the memory during composition. If this is a rare case where `x0` points at the end
    // of the raster there is still one cell that is non-zero. This makes sure it's cleared.
    dstPtr += x0 * kDstBPP;
    cellPtr += x0;
    compOp.spanStartX(uint32_t(x0));

    // Rare case - line rasterized at the end of the raster boundary. In 99% cases this is a clipped line that was
    // rasterized as vertical-only line at the end of the render box. This is a completely valid case that produces
    // nothing.
    if (x0 >= xEnd)
      goto L_Scanline_Done0;

    // Setup compositor and source/destination parts.
    cov = 256 << (A8Info::kShift + 1);
    msk = 0;

    // If `bitWord ^ bitWordTmp` results in non-zero value it means that the current span ends within the same BitWord,
    // otherwise the span crosses multiple BitWords.
    bitWord ^= bitWordTmp;
    if (bitWord)
      goto L_BitScan_Match;

    // Okay, so the span crosses multiple BitWords. Firstly we have to make sure this was not the last one. If that's
    // the case we must terminate the scanning immediately.
    i = BitOps::kNumBits;
    if (bitPtr == bitPtrEnd)
      goto L_BitScan_End;

    // A BitScan loop - iterates over all consecutive BitWords and finds those that don't have all bits set to 1.
L_BitScan_Next:
    for (;;) {
      bitWord = BitOps::ones() ^ bitPtr[0];
      *bitPtr++ = 0;
      xOff += kPixelsPerBitWord;

      if (bitWord)
        goto L_BitScan_Match;

      if (bitPtr == bitPtrEnd)
        goto L_BitScan_End;
    }

L_BitScan_Match:
    i = BitOps::countZerosFromStart(bitWord);

L_BitScan_End:
    bitWordTmp = BitOps::shiftToEnd(BitOps::ones(), i);
    i *= kPixelsPerOneBit;
    bitWord ^= bitWordTmp;
    i += xOff;

    // In cases where the raster width is not a multiply of `pixelsPerOneBit` we must make sure we won't overflow it.
    if (i > xEnd)
      i = xEnd;

    // `i` is now the number of pixels (and cells) to composite by using `vMask`.
    i -= x0;
    x0 += i;

    // VLoop
    // -----

    goto VLoop_CalcMsk;
    for (;;) {
      i--;
      cellPtr++;
      dstPtr = compOp.compositePixelMasked(dstPtr, msk);

VLoop_CalcMsk:
      cov += cellPtr[0];
      *cellPtr = 0;

      msk = calcMask(cov, fillRuleMask, globalAlpha);
      if (!i)
        break;
    }

    if (x0 >= xEnd)
      goto L_Scanline_Done1;

    // BitGap
    // ------

    // If we are here we are at the end of `vMask` loop. There are two possibilities:
    //
    //   1. There is a gap between bits in a single or multiple BitWords. This means that there is a possibility for
    //      a `cMask` loop which could be solid, masked, or have zero-mask (a real gap).
    //
    //   2. This was the last span and there are no more bits in consecutive BitWords. We will not consider this as
    //      a special case and just process the remaining BitWords in a normal way (scanning until the end of the
    //      current scanline).
    while (!bitWord) {
      xOff += kPixelsPerBitWord;
      if (bitPtr == bitPtrEnd)
        goto L_Scanline_Done1;
      bitWord = *bitPtr++;
    }

    i = BitOps::countZerosFromStart(bitWord);
    bitWord ^= BitOps::shiftToEnd(BitOps::ones(), i);
    bitPtr[-1] = 0;

    i = i * kPixelsPerOneBit + xOff - x0;
    x0 += i;
    cellPtr += i;

    BL_ASSERT(x0 <= xEnd);

    if (!msk) {
      dstPtr += i * kDstBPP;
      compOp.spanAdvanceX(uint32_t(x0), uint32_t(i));
    }
    else {
      dstPtr = compOp.compositeCSpan(dstPtr, i, msk);
    }

    if (bitWord)
      goto L_BitScan_Match;
    else
      goto L_BitScan_Next;

    // Scanline Iterator
    // -----------------

    // This loop is used to quickly test bitWords in `bitPtr`. In some cases the whole scanline could be empty, so
    // this loop makes sure we won't enter more complicated loops if this happens. It's also used to quickly find
    // the first bit, which is non-zero - in that case it jumps directly to BitMatch section.
L_Scanline_Done0:
    cellPtr[0] = 0;

L_Scanline_Done1:
    dstPtr -= x0 * kDstBPP;
    cellPtr -= x0;
    compOp.spanEndX(uint32_t(x0));

    if (--y == 0)
      return;

    bitPtr = bitPtrEnd;
    do {
      dstPtr += dstStride;
      cellPtr = PtrOps::offset(cellPtr, cellStride);
      compOp.advanceY();

L_Scanline_Init:
      xOff = 0;
      bitWord = 0;
      bitPtrEnd = PtrOps::offset(bitPtr, bitStride);

      do {
        bitWord |= *bitPtr++;
        if (bitWord)
          goto L_BitScan_Init;

        xOff += kPixelsPerBitWord;
      } while (bitPtr != bitPtrEnd);
    } while (--y);
  }

  static BL_INLINE uint32_t calcMask(uint32_t cov, uint32_t fillRuleMask, uint32_t globalAlpha) noexcept {
    uint32_t c = A8Info::kScale;
    uint32_t m = (IntOps::sar(cov, A8Info::kShift + 1u) & fillRuleMask) - c;
    m = blMin<uint32_t>(uint32_t(blAbs(int32_t(m))), c);

    return (m * globalAlpha) >> 8;
  }
};

template<FillType kFillType, typename CompOp>
struct FillDispatch {};

template<typename CompOp>
struct FillDispatch<FillType::kBoxA, CompOp> { using Fill = FillBoxA_Base<CompOp>; };

template<typename CompOp>
struct FillDispatch<FillType::kMask, CompOp> { using Fill = FillMask_Base<CompOp>; };

template<typename CompOp>
struct FillDispatch<FillType::kAnalytic, CompOp> { using Fill = FillAnalytic_Base<CompOp>; };

} // {anonymous}
} // {Reference}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_REFERENCE_FILLGENERIC_P_H_INCLUDED
