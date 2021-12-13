// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../random.h"
#include "../pipeline/pipedefs_p.h"
#include "../raster/analyticrasterizer_p.h"
#include "../support/intops_p.h"

namespace BLRasterEngine {

// AnalyticRasterizer - Tests
// ==========================

#if defined(BL_TEST)
static bool checkRasterizerState(const AnalyticState& a, const AnalyticState& b) noexcept {
  int yDltMask = (a._dy >= a._dx) ? 255 : -1;

  return a._dx   == b._dx   && a._dy   == b._dy   &&
         a._ex0  == b._ex0  && a._ey0  == b._ey0  &&
         a._ex1  == b._ex1  && a._ey1  == b._ey1  &&
         a._fx0  == b._fx0  && a._fy0  == b._fy0  &&
         a._fx1  == b._fx1  && a._fy1  == b._fy1  &&
         a._xErr == b._xErr && a._yErr == b._yErr &&
         a._xDlt == b._xDlt && (a._yDlt & yDltMask) == (b._yDlt & yDltMask) &&
         a._xRem == b._xRem && a._yRem == b._yRem &&
         a._xLift == b._xLift && a._yLift == b._yLift &&
         a._savedFy1 == b._savedFy1;
}

UNIT(analytic_rasterizer, 1) {
  int w = 1000;
  int h = 1000;

  typedef BLPipeline::A8Info A8Info;

  uint32_t maxBandHeight = 64;
  uint32_t edgeCount = BrokenAPI::hasArg("--quick") ? 5000 : 100000;

  for (uint32_t bandHeightShift = 0; bandHeightShift < BLIntOps::ctz(maxBandHeight); bandHeightShift++) {
    uint32_t bandHeight = 1 << bandHeightShift;
    INFO("Testing advanceToY() correctness [bandHeight=%u]", bandHeight);

    // TODO: Wrap this logic into something, now it's duplicated 3x.
    size_t requiredWidth = BLIntOps::alignUp(uint32_t(w) + 1u + BL_PIPE_PIXELS_PER_ONE_BIT, BL_PIPE_PIXELS_PER_ONE_BIT);
    size_t requiredHeight = bandHeight;
    size_t cellAlignment = 16;

    size_t bitStride = BLIntOps::wordCountFromBitCount<BLBitWord>(requiredWidth / BL_PIPE_PIXELS_PER_ONE_BIT) * sizeof(BLBitWord);
    size_t cellStride = requiredWidth * sizeof(uint32_t);

    size_t bitsStart = 0;
    size_t bitsSize = requiredHeight * bitStride;

    size_t cellsStart = BLIntOps::alignUp(bitsStart + bitsSize, cellAlignment);
    size_t cellsSize = requiredHeight * cellStride;

    uint8_t* buffer = static_cast<uint8_t*>(calloc(bitsSize + cellsSize + cellAlignment, 1));
    EXPECT_NE(buffer, nullptr);

    AnalyticCellStorage cellStorage;
    cellStorage.init(
      reinterpret_cast<BLBitWord*>(buffer + bitsStart), bitStride,
      BLIntOps::alignUp(reinterpret_cast<uint32_t*>(buffer + cellsStart), cellAlignment), cellStride);

    BLRandom rnd(0x1234);
    for (uint32_t i = 0; i < edgeCount; i++) {
      int x0 = int(rnd.nextDouble() * double(w) * double(A8Info::kScale));
      int y0 = int(rnd.nextDouble() * double(h) * double(A8Info::kScale));
      int x1 = int(rnd.nextDouble() * double(w) * double(A8Info::kScale));
      int y1 = int(rnd.nextDouble() * double(h) * double(A8Info::kScale));

      // At least one horizontal line.
      if (i == 0)
        x1 = x0;

      if (y0 > y1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
      }

      y1 += int((bandHeight + 1) * A8Info::kScale);
      if (y1 > (h << A8Info::kShift))
        y1 = (h << A8Info::kShift);

      uint32_t bandY0 = uint32_t(y0 >> A8Info::kShift);
      uint32_t bandY1 = bandY0 + bandHeight;

      AnalyticRasterizer a;
      AnalyticRasterizer b;

      // We don't really care of the cell storage here, can be the same...
      a.init(cellStorage.bitPtrTop, cellStorage.bitStride,
             cellStorage.cellPtrTop, cellStorage.cellStride, bandY0, bandHeight);
      b.init(cellStorage.bitPtrTop, cellStorage.bitStride,
             cellStorage.cellPtrTop, cellStorage.cellStride, bandY0, bandHeight);

      bool aPrepared = a.prepareRef(EdgePoint<int>{x0, y0}, EdgePoint<int>{x1, y1});
      bool bPrepared = b.prepare(EdgePoint<int>{x0, y0}, EdgePoint<int>{x1, y1});

      bool prepareMustMatch = checkRasterizerState(a, b);
      EXPECT_TRUE(prepareMustMatch)
        .message("Rasterizer preparation failed [TestId=%u]:\n"
                 "    Line: int x0=%d, y0=%d, x1=%d, y1=%d;\n"
                 "    A: x0={%d.%d} y0={%d.%d} x1={%d.%d} y1={%d.%d} err={%d|%d} dlt={%d|%d} rem={%d|%d} lift={%d|%d} dx|dy={%d|%d}\n"
                 "    B: x0={%d.%d} y0={%d.%d} x1={%d.%d} y1={%d.%d} err={%d|%d} dlt={%d|%d} rem={%d|%d} lift={%d|%d} dx|dy={%d|%d}",
                 i,
                 x0, y0, x1, y1,
                 a._ex0, a._fx0, a._ey0, a._fy0, a._ex1, a._fx1, a._ey1, a._fy1, a._xErr, a._yErr, a._xDlt, a._yDlt, a._xRem, a._yRem, a._xLift, a._yLift, a._dx, a._dy,
                 b._ex0, b._fx0, b._ey0, b._fy0, b._ex1, b._fx1, b._ey1, b._fy1, b._xErr, b._yErr, b._xDlt, b._yDlt, b._xRem, b._yRem, b._xLift, b._yLift, b._dx, b._dy);

      EXPECT_TRUE(aPrepared);
      EXPECT_TRUE(bPrepared);

      constexpr uint32_t kRasterizerOptions =
        AnalyticRasterizer::kOptionBandOffset |
        AnalyticRasterizer::kOptionBandingMode;

      uint32_t iteration = 0;
      for (;;) {
        bool isFinished = a.template rasterize<kRasterizerOptions>();

        // We cannot advance beyond the end of the edge.
        if (isFinished)
          break;

        b.advanceToY(int(bandY1));
        bool statesMustMatch = checkRasterizerState(a, b);

        EXPECT_TRUE(statesMustMatch)
          .message("Rasterizer states are different [TestId=%u, Iteration=%u, BandY0=%u, BandY1=%u]:\n"
                   "    Line: int x0=%d, y0=%d, x1=%d, y1=%d;\n"
                   "    A: x0={%d.%d} y0={%d.%d} x1={%d.%d} y1={%d.%d} err={%d|%d} dlt={%d|%d} rem={%d|%d} lift={%d|%d} dx|dy={%d|%d}\n"
                   "    B: x0={%d.%d} y0={%d.%d} x1={%d.%d} y1={%d.%d} err={%d|%d} dlt={%d|%d} rem={%d|%d} lift={%d|%d} dx|dy={%d|%d}",
                   i, iteration, bandY0, bandY1,
                   x0, y0, x1, y1,
                   a._ex0, a._fx0, a._ey0, a._fy0, a._ex1, a._fx1, a._ey1, a._fy1, a._xErr, a._yErr, a._xDlt, a._yDlt, a._xRem, a._yRem, a._xLift, a._yLift, a._dx, a._dy,
                   b._ex0, b._fx0, b._ey0, b._fy0, b._ex1, b._fx1, b._ey1, b._fy1, b._xErr, b._yErr, b._xDlt, b._yDlt, b._xRem, b._yRem, b._xLift, b._yLift, b._dx, b._dy);

        bandY0 = bandY1;
        bandY1 += bandHeight;

        a._bandOffset = bandY0;
        a._bandEnd = blMin(bandY1 - 1, uint32_t((y1 - 1) >> A8Info::kShift));

        b._bandOffset = a._bandOffset;
        b._bandEnd = a._bandEnd;

        iteration++;
      }
    }

    free(buffer);
  }
}
#endif

} // {BLRasterEngine}
