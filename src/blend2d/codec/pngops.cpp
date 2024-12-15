// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../runtime_p.h"
#include "../codec/pngops_p.h"

namespace bl {
namespace Png {
namespace Ops {

FunctionTable funcTable;

// bl::Png::Ops - Inverse Filter
// ==============================

static BLResult BL_CDECL inverseFilterImpl(uint8_t* p, uint32_t bpp, uint32_t bpl, uint32_t h) noexcept {
  BL_ASSERT(bpp > 0u);
  BL_ASSERT(bpl > 1u);
  BL_ASSERT(h   > 0u);

  uint32_t y = h;
  uint8_t* u = nullptr;

  // Subtract one BYTE that is used to store the `filter` ID - it's always processed and not part of pixel data.
  bpl--;

  // First row uses a special filter that doesn't access the previous row,
  // which is assumed to contain all zeros.
  uint32_t filterType = *p++;

  if (filterType >= kFilterTypeCount)
    filterType = kFilterTypeNone;

  filterType = simplifyFilterOfFirstRow(filterType);

  for (;;) {
    uint32_t i;

    switch (filterType) {
      case kFilterTypeSub: {
        for (i = bpl - bpp; i != 0; i--, p++)
          p[bpp] = applySumFilter(p[bpp], p[0]);

        p += bpp;
        break;
      }

      case kFilterTypeUp: {
        BL_ASSERT(u != nullptr);
        for (i = bpl; i != 0; i--, p++, u++)
          p[0] = applySumFilter(p[0], u[0]);
        break;
      }

      case kFilterTypeAvg: {
        BL_ASSERT(u != nullptr);
        for (i = 0; i < bpp; i++)
          p[i] = applySumFilter(p[i], u[i] >> 1);

        u += bpp;
        for (i = bpl - bpp; i != 0; i--, p++, u++)
          p[bpp] = applySumFilter(p[bpp], applyAvgFilter(p[0], u[0]));

        p += bpp;
        break;
      }

      case kFilterTypePaeth: {
        BL_ASSERT(u != nullptr);
        for (i = 0; i < bpp; i++)
          p[i] = applySumFilter(p[i], u[i]);

        for (i = bpl - bpp; i != 0; i--, p++, u++)
          p[bpp] = applySumFilter(p[bpp], applyPaethFilter(p[0], u[bpp], u[0]));

        p += bpp;
        break;
      }

      case kFilterTypeAvg0: {
        for (i = bpl - bpp; i != 0; i--, p++)
          p[bpp] = applySumFilter(p[bpp], p[0] >> 1);

        p += bpp;
        break;
      }

      case kFilterTypeNone:
      default:
        p += bpl;
        break;
    }

    if (--y == 0)
      break;

    u = p - bpl;
    filterType = *p++;

    if (filterType >= kFilterTypeCount)
      filterType = kFilterTypeNone;
  }

  return BL_SUCCESS;
}

void initFuncTable_Ref(FunctionTable& ft) noexcept {
  ft.inverseFilter[1] = inverseFilterImpl;
  ft.inverseFilter[2] = inverseFilterImpl;
  ft.inverseFilter[3] = inverseFilterImpl;
  ft.inverseFilter[4] = inverseFilterImpl;
  ft.inverseFilter[6] = inverseFilterImpl;
  ft.inverseFilter[8] = inverseFilterImpl;
}

void initFuncTable(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  // Initialize optimized PNG functions.
  FunctionTable& ft = funcTable;

#if !defined(BL_BUILD_OPT_SSE2) && !defined(BL_BUILD_OPT_ASIMD)
  initFuncTable_Ref(ft);
#endif

#if defined(BL_BUILD_OPT_SSE2)
  if (blRuntimeHasSSE2(rt)) {
    initFuncTable_SSE2(ft);
  }
  else {
    initFuncTable_Ref(ft);
  }
#endif // BL_BUILD_OPT_SSE2

#if defined(BL_BUILD_OPT_AVX)
  if (blRuntimeHasAVX(rt)) {
    initFuncTable_AVX(ft);
  }
#endif // BL_BUILD_OPT_AVX

#if defined(BL_BUILD_OPT_ASIMD)
  if (blRuntimeHasASIMD(rt)) {
    initFuncTable_ASIMD(ft);
  }
  else {
    initFuncTable_Ref(ft);
  }
#endif // BL_BUILD_OPT_ASIMD
}

} // {Ops}
} // {Png}
} // {bl}
