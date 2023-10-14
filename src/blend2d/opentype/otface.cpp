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

namespace bl {
namespace OpenType {

// bl::OpenType - OTFaceImpl - Tracing
// ===================================

#if defined(BL_TRACE_OT_ALL) || defined(BL_TRACE_OT_CORE)
#define Trace BLDebugTrace
#else
#define Trace BLDummyTrace
#endif

// bl::OpenType - OTFaceImpl - Globals
// ===================================

static BLFontFaceVirt blOTFaceVirt;

// bl::OpenType - OTFaceImpl - Init & Destroy
// ==========================================

static BLResult initOpenTypeFace(OTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  OTFaceTables tables;
  tables.init(faceI, fontData);

  BL_PROPAGATE(CoreImpl::init(faceI, tables));
  BL_PROPAGATE(NameImpl::init(faceI, tables));
  BL_PROPAGATE(CMapImpl::init(faceI, tables));

  // Glyph outlines require either 'CFF2', 'CFF ', or 'glyf/loca' tables. Based on these
  // tables we can initialize `outlineType` and select either CFF or GLYF implementation.
  if (tables.cff || tables.cff2) {
    BL_STATIC_ASSERT(CFFData::kVersion1 == 0);
    BL_STATIC_ASSERT(CFFData::kVersion2 == 1);

    uint32_t cffVersion = tables.cff2 ? CFFData::kVersion2 : CFFData::kVersion1;
    BL_PROPAGATE(CFFImpl::init(faceI, tables, cffVersion));
  }
  else if (tables.glyf && tables.loca) {
    BL_PROPAGATE(GlyfImpl::init(faceI, tables));
  }
  else {
    // The font has no outlines that we can use.
    return blTraceError(BL_ERROR_FONT_MISSING_IMPORTANT_TABLE);
  }

  BL_PROPAGATE(MetricsImpl::init(faceI, tables));
  BL_PROPAGATE(LayoutImpl::init(faceI, tables));

  // Only setup legacy kerning if we don't have GlyphPositioning 'GPOS' table.
  if (!blTestFlag(faceI->otFlags, OTFaceFlags::kGPosLookupList))
    BL_PROPAGATE(KernImpl::init(faceI, tables));

  BL_PROPAGATE(faceI->scriptTagSet.finalize());
  BL_PROPAGATE(faceI->featureTagSet.finalize());
  BL_PROPAGATE(faceI->variationTagSet.finalize());

  return BL_SUCCESS;
}

static BLResult BL_CDECL destroyOpenTypeFace(BLObjectImpl* impl) noexcept {
  OTFaceImpl* faceI = static_cast<OTFaceImpl*>(impl);

  blCallDtor(faceI->kern);
  blCallDtor(faceI->layout);
  blCallDtor(faceI->cffFDSubrIndexes);
  blFontFaceImplDtor(faceI);

  return blObjectFreeImpl(faceI);
}

BLResult createOpenTypeFace(BLFontFaceCore* self, const BLFontData* fontData, uint32_t faceIndex) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT_FACE);
  BL_PROPAGATE(ObjectInternal::allocImplT<OTFaceImpl>(self, info));

  // Zero everything so we don't have to initialize features not provided by the font.
  OTFaceImpl* faceI = static_cast<OTFaceImpl*>(self->_d.impl);
  memset(static_cast<void*>(faceI), 0, sizeof(OTFaceImpl));

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
    destroyOpenTypeFace(faceI);
    return result;
  }

  return BL_SUCCESS;
}

} // {OpenType}
} // {bl}

// bl::OpenType - Runtime Registration
// ===================================

void blOpenTypeRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  bl::OpenType::blOTFaceVirt.base.destroy = bl::OpenType::destroyOpenTypeFace;
  bl::OpenType::blOTFaceVirt.base.getProperty = blObjectImplGetProperty;
  bl::OpenType::blOTFaceVirt.base.setProperty = blObjectImplSetProperty;
}
