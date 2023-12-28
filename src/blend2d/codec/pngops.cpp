// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../runtime_p.h"
#include "../codec/pngops_p.h"

namespace bl {
namespace Png {

FuncOpts opts;

// bl::png::Opts - Inverse Filter
// ==============================

BLResult BL_CDECL inverseFilterImpl(uint8_t* p, uint32_t bpp, uint32_t bpl, uint32_t h) noexcept {
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
  filterType = simplifyFilterOfFirstRow(filterType);

  for (;;) {
    uint32_t i;

    switch (filterType) {
      case BL_PNG_FILTER_TYPE_NONE:
        p += bpl;
        break;

      case BL_PNG_FILTER_TYPE_SUB: {
        for (i = bpl - bpp; i != 0; i--, p++)
          p[bpp] = applySumFilter(p[bpp], p[0]);

        p += bpp;
        break;
      }

      case BL_PNG_FILTER_TYPE_UP: {
        BL_ASSERT(u != nullptr);
        for (i = bpl; i != 0; i--, p++, u++)
          p[0] = applySumFilter(p[0], u[0]);
        break;
      }

      case BL_PNG_FILTER_TYPE_AVG: {
        BL_ASSERT(u != nullptr);
        for (i = 0; i < bpp; i++)
          p[i] = applySumFilter(p[i], u[i] >> 1);

        u += bpp;
        for (i = bpl - bpp; i != 0; i--, p++, u++)
          p[bpp] = applySumFilter(p[bpp], applyAvgFilter(p[0], u[0]));

        p += bpp;
        break;
      }

      case BL_PNG_FILTER_TYPE_PAETH: {
        BL_ASSERT(u != nullptr);
        for (i = 0; i < bpp; i++)
          p[i] = applySumFilter(p[i], u[i]);

        for (i = bpl - bpp; i != 0; i--, p++, u++)
          p[bpp] = applySumFilter(p[bpp], blPngPaethFilter(p[0], u[bpp], u[0]));

        p += bpp;
        break;
      }

      case BL_PNG_FILTER_TYPE_AVG0: {
        for (i = bpl - bpp; i != 0; i--, p++)
          p[bpp] = applySumFilter(p[bpp], p[0] >> 1);

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

} // {Png}
} // {bl}
