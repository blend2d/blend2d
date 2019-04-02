// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../blruntime_p.h"
#include "../blsupport_p.h"
#include "../codec/blpngops_p.h"

// ============================================================================
// [Global Variables]
// ============================================================================

BLPngOps blPngOps;

// ============================================================================
// [BLPngOps - InverseFilter]
// ============================================================================

BLResult BL_CDECL blPngInverseFilter(uint8_t* p, uint32_t bpp, uint32_t bpl, uint32_t h) noexcept {
  BL_ASSERT(bpp > 0);
  BL_ASSERT(bpl > 1);
  BL_ASSERT(h   > 0);

  uint32_t y = h;
  uint8_t* u = nullptr;

  // Subtract one BYTE that is used to store the `filter` ID.
  bpl--;

  // First row uses a special filter that doesn't access the previous row,
  // which is assumed to contain all zeros.
  uint32_t filterType = *p++;
  if (BL_UNLIKELY(filterType >= BL_PNG_FILTER_TYPE_COUNT))
    return blTraceError(BL_ERROR_INVALID_DATA);
  filterType = blPngFirstRowFilterReplacement(filterType);

  for (;;) {
    uint32_t i;

    switch (filterType) {
      case BL_PNG_FILTER_TYPE_NONE:
        p += bpl;
        break;

      case BL_PNG_FILTER_TYPE_SUB: {
        for (i = bpl - bpp; i != 0; i--, p++)
          p[bpp] = blPngSumFilter(p[bpp], p[0]);

        p += bpp;
        break;
      }

      case BL_PNG_FILTER_TYPE_UP: {
        for (i = bpl; i != 0; i--, p++, u++)
          p[0] = blPngSumFilter(p[0], u[0]);
        break;
      }

      case BL_PNG_FILTER_TYPE_AVG: {
        for (i = 0; i < bpp; i++)
          p[i] = blPngSumFilter(p[i], u[i] >> 1);

        u += bpp;
        for (i = bpl - bpp; i != 0; i--, p++, u++)
          p[bpp] = blPngSumFilter(p[bpp], blPngAvgFilter(p[0], u[0]));

        p += bpp;
        break;
      }

      case BL_PNG_FILTER_TYPE_PAETH: {
        for (i = 0; i < bpp; i++)
          p[i] = blPngSumFilter(p[i], u[i]);

        for (i = bpl - bpp; i != 0; i--, p++, u++)
          p[bpp] = blPngSumFilter(p[bpp], blPngPaethFilter(p[0], u[bpp], u[0]));

        p += bpp;
        break;
      }

      case BL_PNG_FILTER_TYPE_AVG0: {
        for (i = bpl - bpp; i != 0; i--, p++)
          p[bpp] = blPngSumFilter(p[bpp], p[0] >> 1);

        p += bpp;
        break;
      }
    }

    if (--y == 0)
      break;

    u = p - bpl;
    filterType = *p++;

    if (BL_UNLIKELY(filterType >= BL_PNG_FILTER_TYPE_COUNT))
      return blTraceError(BL_ERROR_INVALID_DATA);
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLPngOps - Runtime Init]
// ============================================================================

void blPngOpsRtInit(BLRuntimeContext* rt) noexcept {
  blPngOps.inverseFilter = blPngInverseFilter;

  #ifdef BL_BUILD_OPT_SSE2
  if (blRuntimeHasSSE2(rt)) {
    blPngOps.inverseFilter = blPngInverseFilter_SSE2;
  }
  #endif
}
