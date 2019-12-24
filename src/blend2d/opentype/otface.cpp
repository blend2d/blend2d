// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

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

// ============================================================================
// [BLOTFaceImpl - Trace]
// ============================================================================

#if defined(BL_TRACE_OT_ALL) || defined(BL_TRACE_OT_CORE)
#define Trace BLDebugTrace
#else
#define Trace BLDummyTrace
#endif

// ============================================================================
// [BLOTFaceImpl - Globals]
// ============================================================================

static BLFontFaceVirt blOTFaceVirt;

// ============================================================================
// [BLOTFaceImpl - Init / Destroy]
// ============================================================================

static BLResult blOTFaceImplInitFace(BLOTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  using namespace BLOpenType;

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
    static_assert(CFFData::kVersion1 == 0, "CFFv1 must have value 0");
    static_assert(CFFData::kVersion2 == 1, "CFFv2 must have value 1");

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
  if (!(faceI->otFlags & BL_OT_FACE_FLAG_GPOS_LOOKUP_LIST))
    BL_PROPAGATE(KernImpl::init(faceI, fontData));

  return BL_SUCCESS;
}

// ============================================================================
// [BLOTFaceImpl - New / Destroy]
// ============================================================================

static BLResult BL_CDECL blOTFaceImplDestroy(BLFontFaceImpl* faceI_) noexcept {
  BLOTFaceImpl* faceI = static_cast<BLOTFaceImpl*>(faceI_);

  blCallDtor(faceI->data);
  blCallDtor(faceI->fullName);
  blCallDtor(faceI->familyName);
  blCallDtor(faceI->subfamilyName);
  blCallDtor(faceI->postScriptName);
  blCallDtor(faceI->kern);
  blCallDtor(faceI->layout);
  blCallDtor(faceI->cffFDSubrIndexes);
  blCallDtor(faceI->scriptTags);
  blCallDtor(faceI->featureTags);

  return blRuntimeFreeImpl(faceI, sizeof(BLOTFaceImpl), faceI->memPoolData);
}

BLResult blOTFaceImplNew(BLOTFaceImpl** dst, const BLFontData* fontData, uint32_t faceIndex) noexcept {
  uint16_t memPoolData;
  BLOTFaceImpl* faceI = blRuntimeAllocImplT<BLOTFaceImpl>(sizeof(BLOTFaceImpl), &memPoolData);

  *dst = nullptr;
  if (BL_UNLIKELY(!faceI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  // Zero everything so we don't have to initialize features not provided by the font.
  memset(faceI, 0, sizeof(BLOTFaceImpl));

  blImplInit(faceI, BL_IMPL_TYPE_FONT_FACE, BL_IMPL_TRAIT_MUTABLE | BL_IMPL_TRAIT_VIRT, memPoolData);
  faceI->virt = &blOTFaceVirt;
  faceI->faceInfo.faceType = uint8_t(BL_FONT_FACE_TYPE_OPENTYPE);
  faceI->faceInfo.faceIndex = faceIndex;
  faceI->funcs = blNullFontFaceFuncs;

  faceI->data.impl = blImplIncRef(fontData->impl);
  blCallCtor(faceI->fullName);
  blCallCtor(faceI->familyName);
  blCallCtor(faceI->subfamilyName);
  blCallCtor(faceI->postScriptName);

  faceI->cmapFormat = uint8_t(0xFF);
  blCallCtor(faceI->kern);
  blCallCtor(faceI->layout);
  blCallCtor(faceI->cffFDSubrIndexes);
  blCallCtor(faceI->scriptTags);
  blCallCtor(faceI->featureTags);

  BLResult result = blOTFaceImplInitFace(faceI, fontData);
  if (BL_UNLIKELY(result != BL_SUCCESS)) {
    blOTFaceImplDestroy(faceI);
    return result;
  }

  *dst = faceI;
  return BL_SUCCESS;
}

// ============================================================================
// [BLOTFaceImpl - RtInit]
// ============================================================================

void blOTFaceImplRtInit(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);
  blOTFaceVirt.destroy = blOTFaceImplDestroy;
}
