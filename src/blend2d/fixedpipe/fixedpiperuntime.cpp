// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../api-build_p.h"
#if !defined(BL_BUILD_NO_FIXED_PIPE)

#include "./fixedpiperuntime_p.h"

// IMPORTANT: This is a work-in-progress fixed pipeline implementation that can
// be used on devices where JIT is not available. It can also be used as a
// reference implementation to see what JIT compiled pipelines are doing. Please
// note that these pipelines were written to match JIT compiled pipelines so we
// are not afraid to use 'goto'.

// ============================================================================
// [Globals]
// ============================================================================

BLWrap<BLFixedPipeRuntime> BLFixedPipeRuntime::_global;

// ============================================================================
// [BLFixedPipe - Fill]
// ============================================================================

template<typename Impl>
struct BLFixedPipe_FillBoxAA_Base {
  static BLResult BL_CDECL pipeline(void* ctxData_, void* fillData_, const void* fetchData_) noexcept {
    BLPipeContextData* ctxData = static_cast<BLPipeContextData*>(ctxData_);
    BLPipeFillData::BoxAA* fillData = static_cast<BLPipeFillData::BoxAA*>(fillData_);

    intptr_t dstStride = ctxData->dst.stride;
    uint8_t* dstPtr = static_cast<uint8_t*>(ctxData->dst.pixelData);

    dstPtr += size_t(uint32_t(fillData->box.x0)) * Impl::DST_BPP;
    dstPtr += intptr_t(uint32_t(fillData->box.y0)) * dstStride;

    uint32_t w = fillData->box.x1 - fillData->box.x0;
    uint32_t h = fillData->box.y1 - fillData->box.y0;

    dstStride -= intptr_t(size_t(w) * Impl::DST_BPP);
    uint32_t msk = fillData->alpha.u;

    Impl impl(fetchData_);
    BL_ASSUME(h > 0);

    if (msk == 255) {
      while (h) {
        dstPtr = impl.compositeSpanOpaque(dstPtr, w);
        dstPtr += dstStride;
        h--;
      }
    }
    else {
      while (h) {
        dstPtr = impl.compositeSpanCMask(dstPtr, w, msk);
        dstPtr += dstStride;
        h--;
      }
    }

    return BL_SUCCESS;
  }
};

template<typename Impl>
struct BLFixedPipe_FillAnalytic_Base {
  enum : uint32_t {
    DST_BPP = Impl::DST_BPP,
    PIXELS_PER_ONE_BIT = 4,
    PIXELS_PER_BIT_WORD = PIXELS_PER_ONE_BIT * blBitSizeOf<BLBitWord>()
  };

  static BLResult BL_CDECL pipeline(void* ctxData_, void* fillData_, const void* fetchData_) noexcept {
    BLPipeContextData* ctxData = static_cast<BLPipeContextData*>(ctxData_);
    BLPipeFillData::Analytic* fillData = static_cast<BLPipeFillData::Analytic*>(fillData_);

    uint32_t y = uint32_t(fillData->box.y0);
    intptr_t dstStride = ctxData->dst.stride;
    uint8_t* dstPtr = static_cast<uint8_t*>(ctxData->dst.pixelData) + intptr_t(y) * dstStride;

    BLBitWord* bitPtr = fillData->bitTopPtr;
    BLBitWord* bitPtrEnd = nullptr;
    uint32_t* cellPtr = fillData->cellTopPtr;

    size_t bitStride = fillData->bitStride;
    size_t cellStride = fillData->cellStride;

    uint32_t globalAlpha = fillData->alpha.u << 7;
    uint32_t fillRuleMask = fillData->fillRuleMask;

    y = uint32_t(fillData->box.y1) - y;

    size_t x0;
    size_t xEnd = fillData->box.x1;
    size_t xOff;

    uint32_t i;
    uint32_t cov;
    uint32_t msk;

    BLBitWord bitWord;
    BLBitWord bitWordTmp;

    Impl impl(fetchData_);
    goto L_Scanline_Init;

    // ------------------------------------------------------------------------
    // [BitScan]
    // ------------------------------------------------------------------------

    // Called by Scanline iterator on the first non-zero BitWord it matches. The
    // responsibility of BitScan is to find the first bit in the passed BitWord
    // followed by matching the bit that ends this match. This would essentially
    // produce the first [x0, x1) span that has to be composited as 'VMask' loop.
L_BitScan_Init:
    x0 = blBitCtz(bitWord);
    bitPtr[-1] = 0;
    bitWordTmp = blBitOnes<BLBitWord>() << x0;
    x0 = x0 * PIXELS_PER_ONE_BIT + xOff;

    // Load the given cells to `m0` and clear the BitWord and all cells it represents
    // in memory. This is important as the compositor has to clear the memory during
    // composition. If this is a rare case where `x0` points at the end of the raster
    // there is still one cell that is non-zero. This makes sure it's cleared.
    dstPtr += x0 * DST_BPP;
    cellPtr += x0;

    // Rare case - line rasterized at the end of the raster boundary. In 99% cases
    // this is a clipped line that was rasterized as vertical-only line at the end
    // of the render box. This is a completely valid case that produces nothing.
    if (x0 >= xEnd)
      goto L_Scanline_Done0;

    // Setup compositor and source/destination parts.
    cov = 256 << (BL_PIPE_A8_SHIFT + 1);
    msk = 0;

    // If `bitWord ^ bitWordTmp` results in non-zero value it means that the current
    // span ends within the same BitWord, otherwise the span crosses multiple BitWords.
    bitWord ^= bitWordTmp;
    if (bitWord)
      goto L_BitScan_Match;

    // Okay, so the span crosses multiple BitWords. Firstly we have to make sure this was
    // not the last one. If that's the case we must terminate the scannling immediately.
    i = blBitSizeOf<BLBitWord>();
    if (bitPtr == bitPtrEnd)
      goto L_BitScan_End;

    // A BitScan loop - iterates over all consecutive BitWords and finds those that don't
    // have all bits set to 1.
L_BitScan_Next:
    for (;;) {
      bitWord = blBitOnes<BLBitWord>() ^ bitPtr[0];
      *bitPtr++ = 0;
      xOff += PIXELS_PER_BIT_WORD;

      if (bitWord)
        goto L_BitScan_Match;

      if (bitPtr == bitPtrEnd)
        goto L_BitScan_End;
    }

L_BitScan_Match:
    i = blBitCtz(bitWord);

L_BitScan_End:
    bitWordTmp = blBitOnes<BLBitWord>() << i;
    i *= PIXELS_PER_ONE_BIT;
    bitWord ^= bitWordTmp;
    i += xOff;

    // In cases where the raster width is not a multiply of `pixelsPerOneBit` we
    // must make sure we won't overflow it.
    if (i > xEnd)
      i = xEnd;

    // `i` is now the number of pixels (and cells) to composite by using `vMask`.
    i -= x0;
    x0 += i;

    // ------------------------------------------------------------------------
    // [VLoop]
    // ------------------------------------------------------------------------

    goto VLoop_CalcMsk;
    for (;;) {
      i--;
      cellPtr++;
      dstPtr = impl.compositePixelMasked(dstPtr, msk);

VLoop_CalcMsk:
      cov += cellPtr[0];
      *cellPtr = 0;

      msk = calcMask(cov, fillRuleMask, globalAlpha);
      if (!i)
        break;
    }

    if (x0 >= xEnd)
      goto L_Scanline_Done1;

    // ------------------------------------------------------------------------
    // [BitGap]
    // ------------------------------------------------------------------------

    // If we are here we are at the end of `vMask` loop. There are two possibilities:
    //
    //   1. There is a gap between bits in a single or multiple BitWords. This
    //      means that there is a possibility for a `cMask` loop which could be
    //      solid, masked, or have zero-mask (a real gap).
    //
    //   2. This was the last span and there are no more bits in consecutive BitWords.
    //      We will not consider this as a special case and just process the remaining
    //      BitWords in a normal way (scanning until the end of the current scanline).
    while (!bitWord) {
      xOff += PIXELS_PER_BIT_WORD;
      if (bitPtr == bitPtrEnd)
        goto L_Scanline_Done1;
      bitWord |= *bitPtr++;
    }

    i = blBitCtz(bitWord);
    bitWord ^= blBitOnes<BLBitWord>() << i;
    bitPtr[-1] = 0;

    i = i * PIXELS_PER_ONE_BIT + xOff - x0;
    x0 += i;
    cellPtr += i;
    BL_ASSERT(x0 <= xEnd);

    if (!msk) {
      dstPtr += i * DST_BPP;
    }
    else if (msk == 256) {
      while (i) {
        dstPtr = impl.compositePixelOpaque(dstPtr);
        i--;
      }
    }
    else {
      while (i) {
        dstPtr = impl.compositePixelMasked(dstPtr, msk);
        i--;
      }
    }

    if (bitWord)
      goto L_BitScan_Match;
    else
      goto L_BitScan_Next;

    // --------------------------------------------------------------------------
    // [Scanline Iterator]
    // --------------------------------------------------------------------------

    // This loop is used to quickly test bitWords in `bitPtr`. In some cases the
    // whole scanline could be empty, so this loop makes sure we won't enter more
    // complicated loops if this happens. It's also used to quickly find the first
    // bit, which is non-zero - in that case it jumps directly to BitMatch section.
L_Scanline_Done0:
    cellPtr[0] = 0;

L_Scanline_Done1:
    dstPtr -= x0 * DST_BPP;
    cellPtr -= x0;

    if (--y == 0)
      goto L_End;

    bitPtr = bitPtrEnd;
    do {
      dstPtr += dstStride;
      cellPtr = blOffsetPtr(cellPtr, cellStride);

L_Scanline_Init:
      xOff = 0;
      bitWord = 0;
      bitPtrEnd = blOffsetPtr(bitPtr, bitStride);

      do {
        bitWord |= *bitPtr++;
        if (bitWord)
          goto L_BitScan_Init;

        xOff += PIXELS_PER_BIT_WORD;
      } while (bitPtr != bitPtrEnd);
    } while (--y);

L_End:
    return BL_SUCCESS;
  }

  static BL_INLINE uint32_t calcMask(uint32_t cov, uint32_t fillRuleMask, uint32_t globalAlpha) noexcept {
    uint32_t c = BL_PIPE_A8_SCALE << 1;
    uint32_t m = (blBitSar(cov, BL_PIPE_A8_SHIFT) & fillRuleMask) - c;
    m = blMin<uint32_t>(uint32_t(blAbs(int32_t(m))), c);

    return (m * globalAlpha) >> 16;
  }
};

// ============================================================================
// [BLFixedPipe - Composite]
// ============================================================================

struct BLFixedPipe_Composite_PRGB32_Src_Solid {
  enum : uint32_t { DST_BPP = 4 };

  uint32_t src;

  BL_INLINE BLFixedPipe_Composite_PRGB32_Src_Solid(const void* fetchData_) noexcept {
    src = static_cast<const BLPipeFetchData::Solid*>(fetchData_)->prgb32;
  }

  BL_INLINE uint8_t* compositePixelOpaque(uint8_t* dstPtr) noexcept {
    blMemWriteU32a(dstPtr, src);
    return dstPtr + DST_BPP;
  }

  BL_INLINE uint8_t* compositePixelMasked(uint8_t* dstPtr, uint32_t m) noexcept {
    uint32_t d = blMemReadU32a(dstPtr);
    uint32_t s = src;

    uint32_t rb = ((s     ) & 0x00FF00FFu) * m;
    uint32_t ag = ((s >> 8) & 0x00FF00FFu) * m;

    m = 255 - m;
    rb = (rb + ((d     ) & 0x00FF00FFu) * m) & 0xFF00FF00u;
    ag = (ag + ((d >> 8) & 0x00FF00FFu) * m) & 0xFF00FF00u;

    blMemWriteU32a(dstPtr, ag | (rb >> 8));
    return dstPtr + DST_BPP;
  }

  BL_INLINE uint8_t* compositeSpanOpaque(uint8_t* dstPtr, uint32_t w) noexcept {
    uint32_t i = w;
    do {
      dstPtr = compositePixelOpaque(dstPtr);
    } while(--i);
    return dstPtr;
  }

  BL_INLINE uint8_t* compositeSpanCMask(uint8_t* dstPtr, uint32_t w, uint32_t m) noexcept {
    uint32_t i = w;
    do {
      dstPtr = compositePixelMasked(dstPtr, m);
    } while(--i);
    return dstPtr;
  }
};

// ============================================================================
// [BLFixedPipeRuntime]
// ============================================================================

static BLPipeFillFunc BL_CDECL blPipeGenRuntimeGet(BLPipeRuntime* self_, uint32_t signature, BLPipeLookupCache* cache) noexcept {
  BLFixedPipeRuntime* self = static_cast<BLFixedPipeRuntime*>(self_);
  BLPipeFillFunc func = nullptr;

  BLPipeSignature s(signature);
  switch (s.fillType()) {
    case BL_PIPE_FILL_TYPE_BOX_AA:
      func = BLFixedPipe_FillBoxAA_Base<BLFixedPipe_Composite_PRGB32_Src_Solid>::pipeline;
      break;

    case BL_PIPE_FILL_TYPE_ANALYTIC:
      func = BLFixedPipe_FillAnalytic_Base<BLFixedPipe_Composite_PRGB32_Src_Solid>::pipeline;
      break;

    default:
      return nullptr;
  }

  if (cache)
    cache->store<BLPipeFillFunc>(signature, func);
  return func;
}

BLFixedPipeRuntime::BLFixedPipeRuntime() noexcept {
  // Setup the `BLPipeRuntime` base.
  _runtimeType = uint8_t(BL_PIPE_RUNTIME_TYPE_FIXED);
  _reserved = 0;
  _runtimeSize = uint16_t(sizeof(BLFixedPipeRuntime));
  _runtimeFlags = 0;

  // BLFixedPipeRuntime destructor - never called.
  _destroy = nullptr;

  // BLFixedPipeRuntime interface - used by the rendering context and `BLPipeProvider`.
  _funcs.get = blPipeGenRuntimeGet;
  _funcs.test = blPipeGenRuntimeGet;
}
BLFixedPipeRuntime::~BLFixedPipeRuntime() noexcept {}

// ============================================================================
// [BLPipeGen::PipeRuntime - Runtime Init]
// ============================================================================

void blFixedPipeRtInit(BLRuntimeContext* rt) noexcept {
  BLFixedPipeRuntime::_global.init();
}

#endif
