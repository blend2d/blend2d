// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#include "../api-build_p.h"
#include "../runtime_p.h"
#include "../support_p.h"
#include "../codec/pngops_p.h"

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
// [BLPngOps - Runtime]
// ============================================================================

void blPngOpsOnInit(BLRuntimeContext* rt) noexcept {
  blPngOps.inverseFilter = blPngInverseFilter;

  #ifdef BL_BUILD_OPT_SSE2
  if (blRuntimeHasSSE2(rt)) {
    blPngOps.inverseFilter = blPngInverseFilter_SSE2;
  }
  #endif
}
