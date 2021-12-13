// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../trace_p.h"
#include "../opentype/otcore_p.h"
#include "../opentype/otcmap_p.h"
#include "../opentype/otface_p.h"

namespace BLOpenType {
namespace CoreImpl {

// OpenType::CoreImpl - Trace
// ==========================

#if defined(BL_TRACE_OT_ALL) || defined(BL_TRACE_OT_CORE)
#define Trace BLDebugTrace
#else
#define Trace BLDummyTrace
#endif

// OpenType::CoreImpl - Utilities
// ==============================

static BL_INLINE const char* stringFromBool(bool value) noexcept {
  static const char str[] = "False\0\0\0True";
  return str + (size_t(value) * 8u);
}

static BL_INLINE const char* sizeCheckMessage(size_t size) noexcept {
  return size ? "Table is truncated" : "Table not found";
}

// OpenType::CoreImpl - Init
// =========================

static BLResult initHead(OTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  BLFontTableT<HeadTable> head;
  fontData->queryTable(faceI->faceInfo.faceIndex, &head, BL_MAKE_TAG('h', 'e', 'a', 'd'));

  Trace trace;
  trace.info("BLOpenType::OTFaceImpl::InitHead [Size=%zu]\n", head.size);
  trace.indent();

  if (!blFontTableFitsT<HeadTable>(head)) {
    trace.fail("%s\n", sizeCheckMessage(head.size));
    return blTraceError(head.size ? BL_ERROR_INVALID_DATA : BL_ERROR_FONT_MISSING_IMPORTANT_TABLE);
  }
  else {
    constexpr uint16_t kMinUnitsPerEm = 16;
    constexpr uint16_t kMaxUnitsPerEm = 16384;

    uint32_t revision = head->revision();
    uint32_t headFlags = head->flags();
    uint16_t unitsPerEm = head->unitsPerEm();
    uint16_t lowestPPEM = head->lowestRecPPEM();

    BLBoxI bbox(head->xMin(), -head->yMax(), head->xMax(), -head->yMin());
    if (bbox.x0 > bbox.x1 || bbox.y0 > bbox.y1)
      bbox.reset();

    if (headFlags & HeadTable::kFlagLastResortFont)
      faceI->faceInfo.faceFlags |= BL_FONT_FACE_FLAG_LAST_RESORT_FONT;

    if (headFlags & HeadTable::kFlagBaselineYEquals0)
      faceI->faceInfo.faceFlags |= BL_FONT_FACE_FLAG_BASELINE_Y_EQUALS_0;

    if (headFlags & HeadTable::kFlagLSBPointXEquals0)
      faceI->faceInfo.faceFlags |= BL_FONT_FACE_FLAG_LSB_POINT_X_EQUALS_0;

    trace.info("Revision: %u.%u\n", revision >> 16, revision & 0xFFFFu);
    trace.info("UnitsPerEm: %u\n", unitsPerEm);
    trace.info("LowestPPEM: %u\n", lowestPPEM);
    trace.info("LastResortFont: %s\n", stringFromBool((faceI->faceInfo.faceFlags & BL_FONT_FACE_FLAG_LAST_RESORT_FONT) != 0));
    trace.info("BaselineYEquals0: %s\n", stringFromBool((faceI->faceInfo.faceFlags & BL_FONT_FACE_FLAG_BASELINE_Y_EQUALS_0) != 0));
    trace.info("LSBPointXEquals0: %s\n", stringFromBool((faceI->faceInfo.faceFlags & BL_FONT_FACE_FLAG_LSB_POINT_X_EQUALS_0) != 0));
    trace.info("BoundingBox: [%d %d %d %d]\n", bbox.x0, bbox.y0, bbox.x1, bbox.y1);

    if (BL_UNLIKELY(unitsPerEm < kMinUnitsPerEm || unitsPerEm > kMaxUnitsPerEm)) {
      trace.fail("Invalid UnitsPerEm [%u], must be within [%u:%u] range\n", unitsPerEm, kMinUnitsPerEm, kMaxUnitsPerEm);
      return blTraceError(BL_ERROR_INVALID_DATA);
    }

    uint32_t glyphDataFormat = head->glyphDataFormat();
    uint32_t indexToLocFormat = head->indexToLocFormat();

    if (glyphDataFormat != 0) {
      trace.fail("Invalid GlyphDataFormat [%u], expected 0\n", glyphDataFormat);
      return blTraceError(BL_ERROR_INVALID_DATA);
    }

    if (indexToLocFormat > 1) {
      trace.fail("Invalid IndexToLocFormat [%u], expected [0:1]\n", indexToLocFormat);
      return blTraceError(BL_ERROR_INVALID_DATA);
    }

    faceI->faceInfo.revision = revision;
    faceI->designMetrics.unitsPerEm = unitsPerEm;
    faceI->designMetrics.lowestPPEM = lowestPPEM;
    faceI->designMetrics.glyphBoundingBox = bbox;
    faceI->otFlags |= indexToLocFormat == 0 ? OTFaceFlags::kLocaOffset16 : OTFaceFlags::kLocaOffset32;
  }

  return BL_SUCCESS;
}

static BLResult initMaxP(OTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  BLFontTableT<MaxPTable> maxp;
  fontData->queryTable(faceI->faceInfo.faceIndex, &maxp, BL_MAKE_TAG('m', 'a', 'x', 'p'));

  Trace trace;
  trace.info("BLOpenType::OTFaceImpl::InitMaxP [Size=%zu]\n", maxp.size);
  trace.indent();

  if (!blFontTableFitsT<MaxPTable>(maxp)) {
    trace.fail("%s\n", sizeCheckMessage(maxp.size));
    return blTraceError(maxp.size ? BL_ERROR_INVALID_DATA : BL_ERROR_FONT_MISSING_IMPORTANT_TABLE);
  }
  else {
    // We don't know yet if the font is TrueType or OpenType, so only use v0.5 header.
    uint32_t glyphCount = maxp->v0_5()->glyphCount();
    trace.info("GlyphCount: %u\n", glyphCount);

    if (glyphCount == 0) {
      trace.fail("Invalid GlyphCount [%u]\n", glyphCount);
      return blTraceError(BL_ERROR_INVALID_DATA);
    }

    faceI->faceInfo.glyphCount = uint16_t(glyphCount);
  }

  return BL_SUCCESS;
}

static BLResult initOS_2(OTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  BLFontTableT<OS2Table> os2;
  fontData->queryTable(faceI->faceInfo.faceIndex, &os2, BL_MAKE_TAG('O', 'S', '/', '2'));

  Trace trace;
  trace.info("BLOpenType::OTFaceImpl::InitOS/2 [Size=%zu]\n", os2.size);
  trace.indent();

  if (!blFontTableFitsT<OS2Table>(os2)) {
    if (os2.size)
      trace.fail("%s\n", sizeCheckMessage(os2.size));
  }
  else {
    // Read weight and stretch (width in OS/2 table).
    uint32_t weight = os2->v0a()->weightClass();
    uint32_t stretch = os2->v0a()->widthClass();

    // Fix design weight from 1..9 to 100..900 (reported by ~8% of fonts).
    if (weight >= 1 && weight <= 9) weight *= 100;

    // Use defaults if not provided.
    if (!weight) weight = BL_FONT_WEIGHT_NORMAL;
    if (!stretch) stretch = BL_FONT_STRETCH_NORMAL;

    faceI->weight = uint16_t(blClamp<uint32_t>(weight, 1, 999));
    faceI->stretch = uint8_t(blClamp<uint32_t>(stretch, 1, 9));

    trace.info("Weight: %u\n", faceI->weight);
    trace.info("Stretch: %u\n", faceI->stretch);

    // Read PANOSE classification.
    memcpy(&faceI->panose, os2->v0a()->panose, sizeof(BLFontPanose));
    if (!faceI->panose.empty())
      faceI->faceInfo.faceFlags |= BL_FONT_FACE_FLAG_PANOSE_DATA;

    // Read unicode coverage.
    faceI->unicodeCoverage.data[0] = os2->v0a()->unicodeCoverage[0].value();
    faceI->unicodeCoverage.data[1] = os2->v0a()->unicodeCoverage[1].value();
    faceI->unicodeCoverage.data[2] = os2->v0a()->unicodeCoverage[2].value();
    faceI->unicodeCoverage.data[3] = os2->v0a()->unicodeCoverage[3].value();
    if (!faceI->unicodeCoverage.empty())
      faceI->faceInfo.faceFlags |= BL_FONT_FACE_FLAG_UNICODE_COVERAGE;

    // Read strikethrough info.
    int strikeoutThickness = os2->v0a()->yStrikeoutSize();
    int strikeoutPosition = -(os2->v0a()->yStrikeoutPosition() + strikeoutThickness);
    faceI->designMetrics.strikethroughPosition = strikeoutPosition;
    faceI->designMetrics.strikethroughThickness = strikeoutThickness;

    trace.info("StrikethroughPosition: %d\n", strikeoutPosition);
    trace.info("StrikethroughThickness: %d\n", strikeoutThickness);

    // Read additional fields provided by newer versions.
    uint32_t version = os2->v0a()->version();
    if (blFontTableFitsT<OS2Table::V0B>(os2)) {
      uint32_t selectionFlags = os2->v0a()->selectionFlags();

      if (selectionFlags & OS2Table::kSelectionItalic)
        faceI->style = BL_FONT_STYLE_ITALIC;
      else if (selectionFlags & OS2Table::kSelectionOblique)
        faceI->style = BL_FONT_STYLE_OBLIQUE;

      if ((selectionFlags & OS2Table::kSelectionUseTypoMetrics) != 0)
        faceI->faceInfo.faceFlags |= BL_FONT_FACE_FLAG_TYPOGRAPHIC_METRICS;
      trace.info("HasTypographicMetrics: %s\n", stringFromBool((faceI->faceInfo.faceFlags & BL_FONT_FACE_FLAG_TYPOGRAPHIC_METRICS) != 0));

      int ascent  = os2->v0b()->typoAscender();
      int descent = os2->v0b()->typoDescender();
      int lineGap = os2->v0b()->typoLineGap();

      descent = blAbs(descent);
      faceI->designMetrics.ascent = ascent;
      faceI->designMetrics.descent = descent;
      faceI->designMetrics.lineGap = lineGap;

      trace.info("Ascent: %d\n", faceI->designMetrics.ascent);
      trace.info("Descent: %d\n", faceI->designMetrics.descent);
      trace.info("LineGap: %d\n", faceI->designMetrics.lineGap);

      if (blFontTableFitsT<OS2Table::V2>(os2) && version >= 2) {
        faceI->designMetrics.xHeight = os2->v2()->xHeight();
        faceI->designMetrics.capHeight = os2->v2()->capHeight();

        trace.info("X-Height: %d\n", faceI->designMetrics.xHeight);
        trace.info("Cap-Height: %d\n", faceI->designMetrics.capHeight);
      }
    }
  }

  return BL_SUCCESS;
}

static BLResult initPost(OTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  BLFontTableT<PostTable> post;
  fontData->queryTable(faceI->faceInfo.faceIndex, &post, BL_MAKE_TAG('p', 'o', 's', 't'));

  Trace trace;
  trace.info("BLOpenType::OTFaceImpl::InitPost [Size=%zu]\n", post.size);
  trace.indent();

  if (!blFontTableFitsT<PostTable>(post)) {
    if (post.size)
      trace.fail("%s\n", sizeCheckMessage(post.size));
  }
  else {
    int underlineThickness = post->underlineThickness();
    int underlinePosition = -(post->underlinePosition() + underlineThickness);

    trace.info("UnderlinePosition: %d\n", underlinePosition);
    trace.info("UnderlineThickness: %d\n", underlineThickness);

    faceI->designMetrics.underlinePosition = underlinePosition;
    faceI->designMetrics.underlineThickness = underlineThickness;
  }

  return BL_SUCCESS;
}

BLResult init(OTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  BL_PROPAGATE(initHead(faceI, fontData));
  BL_PROPAGATE(initMaxP(faceI, fontData));
  BL_PROPAGATE(initOS_2(faceI, fontData));
  BL_PROPAGATE(initPost(faceI, fontData));

  return BL_SUCCESS;
}

} // {CoreImpl}
} // {BLOpenType}
