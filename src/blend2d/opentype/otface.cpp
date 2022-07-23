// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../runtime_p.h"
#include "../trace_p.h"
#include "../opentype/otcff_p.h"
#include "../opentype/otcmap_p.h"
#include "../opentype/otcore_p.h"
#include "../opentype/otdefs_p.h"
#include "../opentype/otface_p.h"
#include "../opentype/otglyf_p.h"
#include "../opentype/otkern_p.h"
#include "../opentype/otlayout_p.h"
#include "../opentype/otmetrics_p.h"
#include "../opentype/otname_p.h"

namespace BLOpenType {

// BLOpenType - OTFaceImpl - Tracing
// =================================

#if defined(BL_TRACE_OT_ALL) || defined(BL_TRACE_OT_CORE)
#define Trace BLDebugTrace
#else
#define Trace BLDummyTrace
#endif

// BLOpenType - OTFaceImpl - Globals
// =================================

static BLFontFaceVirt blOTFaceVirt;

// BLOpenType - OTFaceImpl - Init & Destroy
// ========================================

static BLResult initOpenTypeFace(OTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  uint32_t faceIndex = faceI->faceInfo.faceIndex;

  BL_PROPAGATE(CoreImpl::init(faceI, fontData));
  BL_PROPAGATE(NameImpl::init(faceI, fontData));
  BL_PROPAGATE(CMapImpl::init(faceI, fontData));

  // Glyph outlines require either 'CFF2', 'CFF ', or 'glyf/loca' tables. Based
  // on these tables we can initialize `outlineType` and select either CFF or
  // GLYF implementation.
  BLFontTable tables[2];
  static const uint32_t cffxTags[2] = { BL_MAKE_TAG('C', 'F', 'F', ' '), BL_MAKE_TAG('C', 'F', 'F', '2') };
  static const uint32_t glyfTags[2] = { BL_MAKE_TAG('g', 'l', 'y', 'f'), BL_MAKE_TAG('l', 'o', 'c', 'a') };

  if (fontData->queryTables(faceIndex, tables, cffxTags, 2) != 0) {
    BL_STATIC_ASSERT(CFFData::kVersion1 == 0);
    BL_STATIC_ASSERT(CFFData::kVersion2 == 1);

    uint32_t version = !tables[1].size ? CFFData::kVersion1
                                       : CFFData::kVersion2;

    faceI->faceInfo.outlineType = uint8_t(BL_FONT_OUTLINE_TYPE_CFF + version);
    BL_PROPAGATE(CFFImpl::init(faceI, tables[version], version));
  }
  else if (fontData->queryTables(faceIndex, tables, glyfTags, 2) == 2) {
    faceI->faceInfo.outlineType = BL_FONT_OUTLINE_TYPE_TRUETYPE;
    BL_PROPAGATE(GlyfImpl::init(faceI, tables[0], tables[1]));
  }
  else {
    // The font has no outlines that we can use.
    return blTraceError(BL_ERROR_FONT_MISSING_IMPORTANT_TABLE);
  }

  BL_PROPAGATE(MetricsImpl::init(faceI, fontData));
  BL_PROPAGATE(LayoutImpl::init(faceI, fontData));

  // Only setup legacy kerning if we don't have GlyphPositioning 'GPOS' table.
  if (!blTestFlag(faceI->otFlags, OTFaceFlags::kGPosLookupList))
    BL_PROPAGATE(KernImpl::init(faceI, fontData));

  return BL_SUCCESS;
}

static BLResult BL_CDECL destroyOpenTypeFace(BLObjectImpl* impl, uint32_t info) noexcept {
  OTFaceImpl* faceI = static_cast<OTFaceImpl*>(impl);

  blCallDtor(faceI->kern);
  blCallDtor(faceI->layout);
  blCallDtor(faceI->cffFDSubrIndexes);
  blFontFaceImplDtor(faceI);

  return blObjectDetailFreeImpl(faceI, info);
}

BLResult createOpenTypeFace(BLFontFaceCore* self, const BLFontData* fontData, uint32_t faceIndex) noexcept {
  OTFaceImpl* faceI = blObjectDetailAllocImplT<OTFaceImpl>(self, BLObjectInfo::packType(BL_OBJECT_TYPE_FONT_FACE));
  if (BL_UNLIKELY(!faceI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  // Zero everything so we don't have to initialize features not provided by the font.
  memset(faceI, 0, sizeof(OTFaceImpl));

  blFontFaceImplCtor(faceI, &blOTFaceVirt, blNullFontFaceFuncs);

  faceI->faceInfo.faceType = uint8_t(BL_FONT_FACE_TYPE_OPENTYPE);
  faceI->faceInfo.faceIndex = faceIndex;
  faceI->data.dcast() = *fontData;
  faceI->cmapFormat = uint8_t(0xFF);

  blCallCtor(faceI->kern);
  blCallCtor(faceI->layout);
  blCallCtor(faceI->cffFDSubrIndexes);

  BLResult result = initOpenTypeFace(faceI, fontData);
  if (BL_UNLIKELY(result != BL_SUCCESS)) {
    destroyOpenTypeFace(faceI, self->_d.info.bits);
    return result;
  }

  return BL_SUCCESS;
}

} // {BLOpenType}

// BLOpenType - Runtime Registration
// =================================

void blOpenTypeRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  BLOpenType::blOTFaceVirt.base.destroy = BLOpenType::destroyOpenTypeFace;
  BLOpenType::blOTFaceVirt.base.getProperty = blObjectImplGetProperty;
  BLOpenType::blOTFaceVirt.base.setProperty = blObjectImplSetProperty;
}
