// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../bltrace_p.h"
#include "../opentype/blotcore_p.h"
#include "../opentype/blotcmap_p.h"
#include "../opentype/blotface_p.h"

namespace BLOpenType {
namespace CoreImpl {

// ============================================================================
// [BLOpenType::CoreImpl - Trace]
// ============================================================================

#if defined(BL_TRACE_OT_ALL) || defined(BL_TRACE_OT_CORE)
#define Trace BLDebugTrace
#else
#define Trace BLDummyTrace
#endif

// ============================================================================
// [BLOpenType::CoreImpl - Utilities]
// ============================================================================

static BL_INLINE const char* stringFromBool(bool value) noexcept {
  static const char str[] = "False\0\0\0True";
  return str + (size_t(value) * 8u);
}

static BL_INLINE const char* sizeCheckMessage(size_t size) noexcept {
  return size ? "Table is truncated" : "Table not found";
}

// ============================================================================
// [BLOpenType::CoreImpl - Init]
// ============================================================================

static BLResult initHead(BLOTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  BLFontTableT<HeadTable> head;
  fontData->queryTable(&head, BL_MAKE_TAG('h', 'e', 'a', 'd'));

  Trace trace;
  trace.info("BLOTFaceImpl::InitHead [Size=%zu]\n", head.size);
  trace.indent();

  if (!blFontTableFitsT<HeadTable>(head)) {
    trace.fail("%s\n", sizeCheckMessage(head.size));
    return blTraceError(head.size ? BL_ERROR_INVALID_DATA : BL_ERROR_FONT_MISSING_IMPORTANT_TABLE);
  }
  else {
    constexpr uint16_t kMinUnitsPerEm = 16;
    constexpr uint16_t kMaxUnitsPerEm = 16384;

    uint32_t headFlags = head->flags();
    uint16_t unitsPerEm = head->unitsPerEm();

    if (headFlags & HeadTable::kFlagLastResortFont)
      faceI->faceFlags |= BL_FONT_FACE_FLAG_LAST_RESORT_FONT;

    if (headFlags & HeadTable::kFlagBaselineYEquals0)
      faceI->otFlags |= BL_OT_FACE_FLAG_BASELINE_Y_EQUALS_0;

    if (headFlags & HeadTable::kFlagLSBPointXEquals0)
      faceI->otFlags |= BL_OT_FACE_FLAG_LSB_POINT_X_EQUALS_0;

    trace.info("UnitsPerEm: %u\n", unitsPerEm);
    trace.info("LastResortFont: %s\n", stringFromBool((faceI->faceFlags & BL_FONT_FACE_FLAG_LAST_RESORT_FONT) != 0));
    trace.info("BaselineYEquals0: %s\n", stringFromBool((faceI->otFlags & BL_OT_FACE_FLAG_BASELINE_Y_EQUALS_0) != 0));
    trace.info("LSBPointXEquals0: %s\n", stringFromBool((faceI->otFlags & BL_OT_FACE_FLAG_LSB_POINT_X_EQUALS_0) != 0));

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

    faceI->designMetrics.unitsPerEm = unitsPerEm;
    faceI->otFlags |= indexToLocFormat == 0 ? BL_OT_FACE_FLAG_LOCA_OFFSET_16
                                            : BL_OT_FACE_FLAG_LOCA_OFFSET_32;
  }

  return BL_SUCCESS;
}

static BLResult initMaxP(BLOTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  BLFontTableT<MaxPTable> maxp;
  fontData->queryTable(&maxp, BL_MAKE_TAG('m', 'a', 'x', 'p'));

  Trace trace;
  trace.info("BLOTFaceImpl::InitMaxP [Size=%zu]\n", maxp.size);
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

    faceI->glyphCount = uint16_t(glyphCount);
  }

  return BL_SUCCESS;
}

static BLResult initOS_2(BLOTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  BLFontTableT<OS2Table> os2;
  fontData->queryTable(&os2, BL_MAKE_TAG('O', 'S', '/', '2'));

  Trace trace;
  trace.info("BLOTFaceImpl::InitOS/2 [Size=%zu]\n", os2.size);
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
      faceI->faceFlags |= BL_FONT_FACE_FLAG_PANOSE_DATA;

    // Read unicode coverage.
    faceI->unicodeCoverage.data[0] = os2->v0a()->unicodeCoverage[0].value();
    faceI->unicodeCoverage.data[1] = os2->v0a()->unicodeCoverage[1].value();
    faceI->unicodeCoverage.data[2] = os2->v0a()->unicodeCoverage[2].value();
    faceI->unicodeCoverage.data[3] = os2->v0a()->unicodeCoverage[3].value();
    if (!faceI->unicodeCoverage.empty())
      faceI->faceFlags |= BL_FONT_FACE_FLAG_UNICODE_COVERAGE;

    // Read strikethrough info.
    faceI->designMetrics.strikethroughPosition = os2->v0a()->yStrikeoutPosition();
    faceI->designMetrics.strikethroughThickness = os2->v0a()->yStrikeoutSize();

    trace.info("StrikethroughPosition: %d\n", faceI->designMetrics.strikethroughPosition);
    trace.info("StrikethroughThickness: %d\n", faceI->designMetrics.strikethroughThickness);

    // Read additional fields provided by newer versions.
    uint32_t version = os2->v0a()->version();
    if (blFontTableFitsT<OS2Table::V0B>(os2)) {
      uint32_t selectionFlags = os2->v0a()->selectionFlags();

      if (selectionFlags & OS2Table::kSelectionItalic)
        faceI->style = BL_FONT_STYLE_ITALIC;
      else if (selectionFlags & OS2Table::kSelectionOblique)
        faceI->style = BL_FONT_STYLE_OBLIQUE;

      if ((selectionFlags & OS2Table::kSelectionUseTypoMetrics) != 0)
        faceI->faceFlags |= BL_FONT_FACE_FLAG_TYPOGRAPHIC_METRICS;
      trace.info("HasTypographicMetrics: %s\n", stringFromBool((faceI->faceFlags & BL_FONT_FACE_FLAG_TYPOGRAPHIC_METRICS) != 0));

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

static BLResult initPost(BLOTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  BLFontTableT<PostTable> post;
  fontData->queryTable(&post, BL_MAKE_TAG('p', 'o', 's', 't'));

  Trace trace;
  trace.info("BLOTFaceImpl::InitPost [Size=%zu]\n", post.size);
  trace.indent();

  if (!blFontTableFitsT<PostTable>(post)) {
    if (post.size)
      trace.fail("%s\n", sizeCheckMessage(post.size));
  }
  else {
    int underlinePosition = post->underlinePosition();
    int underlineThickness = post->underlineThickness();

    trace.info("UnderlinePosition: %d\n", underlinePosition);
    trace.info("UnderlineThickness: %d\n", underlineThickness);

    faceI->designMetrics.underlinePosition = underlinePosition;
    faceI->designMetrics.underlineThickness = underlineThickness;
  }

  return BL_SUCCESS;
}

BLResult init(BLOTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  BL_PROPAGATE(initHead(faceI, fontData));
  BL_PROPAGATE(initMaxP(faceI, fontData));
  BL_PROPAGATE(initOS_2(faceI, fontData));
  BL_PROPAGATE(initPost(faceI, fontData));

  return BL_SUCCESS;
}

} // {CoreImpl}
} // {BLOpenType}
