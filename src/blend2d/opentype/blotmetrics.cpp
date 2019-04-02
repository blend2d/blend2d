// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../bltrace_p.h"
#include "../opentype/blotface_p.h"
#include "../opentype/blotmetrics_p.h"

namespace BLOpenType {
namespace MetricsImpl {

// ============================================================================
// [BLOpenType::MetricsImpl - Trace]
// ============================================================================

#if defined(BL_TRACE_OT_ALL) || defined(BL_TRACE_OT_CORE)
#define Trace BLDebugTrace
#else
#define Trace BLDummyTrace
#endif

// ============================================================================
// [BLOpenType::MetricsImpl - Utilities]
// ============================================================================

static BL_INLINE const char* stringFromBool(bool value) noexcept {
  static const char str[] = "False\0\0\0True";
  return str + (size_t(value) * 8u);
}

static BL_INLINE const char* sizeCheckMessage(size_t size) noexcept {
  return size ? "Table is truncated" : "Table not found";
}

// ============================================================================
// [BLOpenType::MetricsImpl - GetGlyphAdvances]
// ============================================================================

static BLResult BL_CDECL getGlyphAdvances(const BLFontFaceImpl* faceI_, const BLGlyphId* glyphIdData, intptr_t glyphIdAdvance, BLGlyphPlacement* placementData, size_t count) noexcept {
  const BLOTFaceImpl* faceI = static_cast<const BLOTFaceImpl*>(faceI_);
  const XMtxTable* mtxTable = faceI->metrics.xmtxTable[BL_TEXT_ORIENTATION_HORIZONTAL].dataAs<XMtxTable>();

  // Sanity check.
  uint32_t longMetricCount = faceI->metrics.longMetricCount[BL_TEXT_ORIENTATION_HORIZONTAL];
  uint32_t longMetricMax = longMetricCount - 1u;

  if (BL_UNLIKELY(!longMetricCount))
    return blTraceError(BL_ERROR_INVALID_DATA);

  for (size_t i = 0; i < count; i++) {
    uint32_t glyphId = glyphIdData[0];
    glyphIdData = blOffsetPtr(glyphIdData, glyphIdAdvance);

    uint32_t metricIndex = blMin(glyphId, longMetricMax);
    int32_t advance = mtxTable->lmArray()[metricIndex].advance.value();

    placementData[i].placement.reset(0, 0);
    placementData[i].advance.reset(advance, 0);
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLOpenType::MetricsImpl - Init]
// ============================================================================

BLResult init(BLOTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  BLFontTableT<XHeaTable> hhea;
  BLFontTableT<XHeaTable> vhea;

  BLFontDesignMetrics& dm = faceI->designMetrics;

  if (fontData->queryTable(&hhea, BL_MAKE_TAG('h', 'h', 'e', 'a'))) {
    if (!blFontTableFitsT<XHeaTable>(hhea))
      return blTraceError(BL_ERROR_INVALID_DATA);

    if (!(faceI->faceFlags & BL_FONT_FACE_FLAG_TYPOGRAPHIC_METRICS)) {
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

    BLFontTableT<XMtxTable> hmtx;
    if (fontData->queryTable(&hmtx, BL_MAKE_TAG('h', 'm', 't', 'x'))) {
      uint32_t longMetricCount = blMin<uint32_t>(hhea->longMetricCount(), faceI->glyphCount);
      uint32_t longMetricDataSize = longMetricCount * sizeof(XMtxTable::LongMetric);

      if (longMetricDataSize > hmtx.size)
        return blTraceError(BL_ERROR_INVALID_DATA);

      size_t lsbCount = blMin<size_t>((hmtx.size - longMetricDataSize) / 2u, longMetricCount - faceI->glyphCount);
      faceI->metrics.xmtxTable[BL_TEXT_ORIENTATION_HORIZONTAL] = hmtx;
      faceI->metrics.longMetricCount[BL_TEXT_ORIENTATION_HORIZONTAL] = uint16_t(longMetricCount);
      faceI->metrics.lsbArraySize[BL_TEXT_ORIENTATION_HORIZONTAL] = uint16_t(lsbCount);
    }

    faceI->funcs.getGlyphAdvances = getGlyphAdvances;
  }

  if (fontData->queryTable(&vhea, BL_MAKE_TAG('v', 'h', 'e', 'a'))) {
    if (!blFontTableFitsT<XHeaTable>(vhea))
      return blTraceError(BL_ERROR_INVALID_DATA);

    dm.vAscent = vhea->ascender();
    dm.vDescent = vhea->descender();
    dm.vMinLSB = vhea->minLeadingBearing();
    dm.vMinTSB = vhea->minTrailingBearing();
    dm.vMaxAdvance = vhea->maxAdvance();

    BLFontTableT<XMtxTable> vmtx;
    if (fontData->queryTable(&vmtx, BL_MAKE_TAG('v', 'm', 't', 'x'))) {
      uint32_t longMetricCount = blMin<uint32_t>(vhea->longMetricCount(), faceI->glyphCount);
      uint32_t longMetricDataSize = longMetricCount * sizeof(XMtxTable::LongMetric);

      if (longMetricDataSize > vmtx.size)
        return blTraceError(BL_ERROR_INVALID_DATA);

      size_t lsbCount = blMin<size_t>((vmtx.size - longMetricDataSize) / 2u, longMetricCount - faceI->glyphCount);
      faceI->metrics.xmtxTable[BL_TEXT_ORIENTATION_VERTICAL] = vmtx;
      faceI->metrics.longMetricCount[BL_TEXT_ORIENTATION_VERTICAL] = uint16_t(longMetricCount);
      faceI->metrics.lsbArraySize[BL_TEXT_ORIENTATION_VERTICAL] = uint16_t(lsbCount);
    }
  }

  return BL_SUCCESS;
}

} // {MetricsImpl}
} // {BLOpenType}
