// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../trace_p.h"
#include "../opentype/otface_p.h"
#include "../opentype/otmetrics_p.h"
#include "../support/ptrops_p.h"

namespace bl {
namespace OpenType {
namespace MetricsImpl {

// bl::OpenType::MetricsImpl - Trace
// =================================

#if defined(BL_TRACE_OT_ALL) || defined(BL_TRACE_OT_CORE)
#define Trace BLDebugTrace
#else
#define Trace BLDummyTrace
#endif

// bl::OpenType::MetricsImpl - Utilities
// =====================================

static BL_INLINE const char* stringFromBool(bool value) noexcept {
  static const char str[] = "False\0\0\0True";
  return str + (size_t(value) * 8u);
}

static BL_INLINE const char* sizeCheckMessage(size_t size) noexcept {
  return size ? "Table is truncated" : "Table not found";
}

// bl::OpenType::MetricsImpl - GetGlyphAdvances
// ============================================

static BLResult BL_CDECL getGlyphAdvances(const BLFontFaceImpl* faceI_, const uint32_t* glyphData, intptr_t glyphAdvance, BLGlyphPlacement* placementData, size_t count) noexcept {
  const OTFaceImpl* faceI = static_cast<const OTFaceImpl*>(faceI_);
  const XMtxTable* mtxTable = faceI->metrics.xmtxTable[BL_ORIENTATION_HORIZONTAL].dataAs<XMtxTable>();

  // Sanity check.
  uint32_t longMetricCount = faceI->metrics.longMetricCount[BL_ORIENTATION_HORIZONTAL];
  uint32_t longMetricMax = longMetricCount - 1u;

  if (BL_UNLIKELY(!longMetricCount))
    return blTraceError(BL_ERROR_INVALID_DATA);

  for (size_t i = 0; i < count; i++) {
    BLGlyphId glyphId = glyphData[0];
    glyphData = PtrOps::offset(glyphData, glyphAdvance);

    uint32_t metricIndex = blMin(glyphId, longMetricMax);
    int32_t advance = mtxTable->lmArray()[metricIndex].advance.value();

    placementData[i].placement.reset(0, 0);
    placementData[i].advance.reset(advance, 0);
  }

  return BL_SUCCESS;
}

// bl::OpenType::MetricsImpl - Init
// ================================

BLResult init(OTFaceImpl* faceI, OTFaceTables& tables) noexcept {
  BLFontDesignMetrics& dm = faceI->designMetrics;

  Table<XHeaTable> hhea(tables.hhea);
  Table<XMtxTable> hmtx(tables.hmtx);
  Table<XHeaTable> vhea(tables.vhea);
  Table<XMtxTable> vmtx(tables.vmtx);

  if (hhea) {
    if (!hhea.fits())
      return blTraceError(BL_ERROR_INVALID_DATA);

    if (!(faceI->faceInfo.faceFlags & BL_FONT_FACE_FLAG_TYPOGRAPHIC_METRICS)) {
      int ascent  = hhea->ascender();
      int descent = hhea->descender();
      int lineGap = hhea->lineGap();

      dm.ascent = ascent;
      dm.descent = blAbs(descent);
      dm.lineGap = lineGap;
    }

    dm.hMinLSB = hhea->minLeadingBearing();
    dm.hMinTSB = hhea->minTrailingBearing();
    dm.hMaxAdvance = hhea->maxAdvance();

    if (hmtx) {
      uint32_t longMetricCount = blMin<uint32_t>(hhea->longMetricCount(), faceI->faceInfo.glyphCount);
      uint32_t longMetricDataSize = longMetricCount * sizeof(XMtxTable::LongMetric);

      if (!hmtx.fits(longMetricDataSize))
        return blTraceError(BL_ERROR_INVALID_DATA);

      size_t lsbCount = blMin<size_t>((hmtx.size - longMetricDataSize) / 2u, longMetricCount - faceI->faceInfo.glyphCount);
      faceI->metrics.xmtxTable[BL_ORIENTATION_HORIZONTAL] = hmtx;
      faceI->metrics.longMetricCount[BL_ORIENTATION_HORIZONTAL] = uint16_t(longMetricCount);
      faceI->metrics.lsbArraySize[BL_ORIENTATION_HORIZONTAL] = uint16_t(lsbCount);
    }

    faceI->funcs.getGlyphAdvances = getGlyphAdvances;
  }

  if (vhea) {
    if (!vhea.fits())
      return blTraceError(BL_ERROR_INVALID_DATA);

    dm.vAscent = vhea->ascender();
    dm.vDescent = vhea->descender();
    dm.vMinLSB = vhea->minLeadingBearing();
    dm.vMinTSB = vhea->minTrailingBearing();
    dm.vMaxAdvance = vhea->maxAdvance();

    if (vmtx) {
      uint32_t longMetricCount = blMin<uint32_t>(vhea->longMetricCount(), faceI->faceInfo.glyphCount);
      uint32_t longMetricDataSize = longMetricCount * sizeof(XMtxTable::LongMetric);

      if (!vmtx.fits(longMetricDataSize))
        return blTraceError(BL_ERROR_INVALID_DATA);

      size_t lsbCount = blMin<size_t>((vmtx.size - longMetricDataSize) / 2u, longMetricCount - faceI->faceInfo.glyphCount);
      faceI->metrics.xmtxTable[BL_ORIENTATION_VERTICAL] = vmtx;
      faceI->metrics.longMetricCount[BL_ORIENTATION_VERTICAL] = uint16_t(longMetricCount);
      faceI->metrics.lsbArraySize[BL_ORIENTATION_VERTICAL] = uint16_t(lsbCount);
    }
  }

  return BL_SUCCESS;
}

} // {MetricsImpl}
} // {OpenType}
} // {bl}
