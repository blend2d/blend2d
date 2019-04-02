// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../blruntime_p.h"
#include "../bltrace_p.h"
#include "../opentype/blotcff_p.h"
#include "../opentype/blotcmap_p.h"
#include "../opentype/blotcore_p.h"
#include "../opentype/blotdefs_p.h"
#include "../opentype/blotface_p.h"
#include "../opentype/blotglyf_p.h"
#include "../opentype/blotkern_p.h"
#include "../opentype/blotlayout_p.h"
#include "../opentype/blotmetrics_p.h"
#include "../opentype/blotname_p.h"

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

  BL_PROPAGATE(CoreImpl::init(faceI, fontData));
  BL_PROPAGATE(NameImpl::init(faceI, fontData));
  BL_PROPAGATE(CMapImpl::init(faceI, fontData));

  // Glyph outlines require either 'CFF2', 'CFF ', or 'glyf/loca' tables. Based
  // on these tables we can initialize `outlineType` and select either CFF or
  // GLYF implementation.
  BLFontTable tables[2];
  static const uint32_t cffxTags[2] = { BL_MAKE_TAG('C', 'F', 'F', ' '), BL_MAKE_TAG('C', 'F', 'F', '2') };
  static const uint32_t glyfTags[2] = { BL_MAKE_TAG('g', 'l', 'y', 'f'), BL_MAKE_TAG('l', 'o', 'c', 'a') };

  if (fontData->queryTables(tables, cffxTags, 2) != 0) {
    static_assert(CFFData::kVersion1 == 0, "CFFv1 must have value 0");
    static_assert(CFFData::kVersion2 == 1, "CFFv2 must have value 1");

    uint32_t version = !tables[1].size ? CFFData::kVersion1
                                       : CFFData::kVersion2;

    faceI->outlineType = BL_FONT_OUTLINE_TYPE_CFF + version;
    BL_PROPAGATE(CFFImpl::init(faceI, tables[version], version));
  }
  else if (fontData->queryTables(tables, glyfTags, 2) == 2) {
    faceI->outlineType = BL_FONT_OUTLINE_TYPE_TRUETYPE;
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
// [BLOTFaceImpl - New]
// ============================================================================

BLResult blOTFaceImplNew(BLOTFaceImpl** dst, const BLFontLoader* loader, const BLFontData* fontData, uint32_t faceIndex) noexcept {
  uint16_t memPoolData;
  BLOTFaceImpl* faceI = blRuntimeAllocImplT<BLOTFaceImpl>(sizeof(BLOTFaceImpl), &memPoolData);

  *dst = faceI;
  if (BL_UNLIKELY(!faceI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  // Zero everything so we don't have to initialize features not provided by the font.
  memset(faceI, 0, sizeof(BLOTFaceImpl));

  blImplInit(faceI, BL_IMPL_TYPE_FONT_FACE, BL_IMPL_TRAIT_VIRT, memPoolData);
  faceI->virt = &blOTFaceVirt;
  faceI->data.impl = blImplIncRef(fontData->impl);
  faceI->loader.impl = blImplIncRef(loader->impl);
  faceI->faceType = uint8_t(BL_FONT_FACE_TYPE_OPENTYPE);
  faceI->faceIndex = faceIndex;
  faceI->funcs = blNullFontFaceFuncs;

  blCallCtor(faceI->fullName);
  blCallCtor(faceI->familyName);
  blCallCtor(faceI->subfamilyName);
  blCallCtor(faceI->postScriptName);
  blCallCtor(faceI->kern);
  blCallCtor(faceI->layout);
  blCallCtor(faceI->scriptTags);
  blCallCtor(faceI->featureTags);

  BLResult result = blOTFaceImplInitFace(faceI, fontData);
  if (result == BL_SUCCESS)
    return result;

  *dst = nullptr;
  faceI->virt->destroy(faceI);
  return result;
}

static BLResult BL_CDECL blOTFaceImplDestroy(BLFontFaceImpl* faceI_) noexcept {
  BLOTFaceImpl* faceI = static_cast<BLOTFaceImpl*>(faceI_);

  blCallDtor(faceI->data);
  blCallDtor(faceI->loader);
  blCallDtor(faceI->fullName);
  blCallDtor(faceI->familyName);
  blCallDtor(faceI->subfamilyName);
  blCallDtor(faceI->postScriptName);
  blCallDtor(faceI->kern);
  blCallDtor(faceI->layout);
  blCallDtor(faceI->scriptTags);
  blCallDtor(faceI->featureTags);

  return blRuntimeFreeImpl(faceI, sizeof(BLOTFaceImpl), faceI->memPoolData);
}

// ============================================================================
// [BLOTFaceImpl - RtInit]
// ============================================================================

void blOTFaceImplRtInit(BLRuntimeContext* rt) noexcept {
  blOTFaceVirt.destroy =blOTFaceImplDestroy;
}
