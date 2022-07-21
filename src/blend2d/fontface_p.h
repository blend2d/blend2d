// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTFACE_P_H_INCLUDED
#define BLEND2D_FONTFACE_P_H_INCLUDED

#include "api-internal_p.h"
#include "array_p.h"
#include "bitset_p.h"
#include "font.h"
#include "matrix_p.h"
#include "object_p.h"
#include "support/scopedbuffer_p.h"

//! \cond INTERNAL

namespace BLOpenType { struct OTFaceImpl; }

//! \addtogroup blend2d_internal
//! \{

//! \name BLFontFace - Internal Memory Management
//! \{

struct BLInternalFontFaceFuncs {
  BLResult (BL_CDECL* mapTextToGlyphs)(
    const BLFontFaceImpl* impl,
    uint32_t* content,
    size_t count,
    BLGlyphMappingState* state) BL_NOEXCEPT;

  BLResult (BL_CDECL* getGlyphBounds)(
    const BLFontFaceImpl* impl,
    const uint32_t* glyphData,
    intptr_t glyphAdvance,
    BLBoxI* boxes,
    size_t count) BL_NOEXCEPT;

  BLResult (BL_CDECL* getGlyphAdvances)(
    const BLFontFaceImpl* impl,
    const uint32_t* glyphData,
    intptr_t glyphAdvance,
    BLGlyphPlacement* placementData,
    size_t count) BL_NOEXCEPT;

  BLResult (BL_CDECL* getGlyphOutlines)(
    const BLFontFaceImpl* impl,
    uint32_t glyphId,
    const BLMatrix2D* userMatrix,
    BLPath* out,
    size_t* contourCountOut,
    BLScopedBuffer* tmpBuffer) BL_NOEXCEPT;

  BLResult (BL_CDECL* applyKern)(
    const BLFontFaceImpl* faceI,
    uint32_t* glyphData,
    BLGlyphPlacement* placementData,
    size_t count) BL_NOEXCEPT;

  BLResult (BL_CDECL* applyGSub)(
    const BLFontFaceImpl* impl,
    BLGlyphBuffer* gb,
    const BLBitSetCore* lookups) BL_NOEXCEPT;

  BLResult (BL_CDECL* applyGPos)(
    const BLFontFaceImpl* impl,
    BLGlyphBuffer* gb,
    const BLBitSetCore* lookups) BL_NOEXCEPT;

  BLResult (BL_CDECL* positionGlyphs)(
    const BLFontFaceImpl* impl,
    uint32_t* glyphData,
    BLGlyphPlacement* placementData,
    size_t count) BL_NOEXCEPT;
};

BL_HIDDEN extern BLInternalFontFaceFuncs blNullFontFaceFuncs;

struct BLInternalFontFaceImpl : public BLFontFaceImpl {
  BLBitSetCore characterCoverage;
  BLInternalFontFaceFuncs funcs;
};

template<typename T = BLInternalFontFaceImpl>
static BL_INLINE T* blFontFaceGetImpl(const BLFontFaceCore* self) noexcept {
  return static_cast<T*>(static_cast<BLInternalFontFaceImpl*>(self->_d.impl));
}

static BL_INLINE void blFontFaceImplCtor(BLInternalFontFaceImpl* impl, BLFontFaceVirt* virt, BLInternalFontFaceFuncs& funcs) noexcept {
  impl->virt = virt;
  blCallCtor(impl->data);
  blCallCtor(impl->fullName);
  blCallCtor(impl->familyName);
  blCallCtor(impl->subfamilyName);
  blCallCtor(impl->postScriptName);
  blCallCtor(impl->scriptTags);
  blCallCtor(impl->featureTags);
  blObjectAtomicContentInit(&impl->characterCoverage);
  impl->funcs = funcs;
}

static BL_INLINE void blFontFaceImplDtor(BLInternalFontFaceImpl* impl) noexcept {
  if (blObjectAtomicContentTest(&impl->characterCoverage))
    blCallDtor(impl->characterCoverage);

  blCallDtor(impl->featureTags);
  blCallDtor(impl->scriptTags);
  blCallDtor(impl->postScriptName);
  blCallDtor(impl->subfamilyName);
  blCallDtor(impl->familyName);
  blCallDtor(impl->fullName);
  blCallDtor(impl->data);
}

//! \}

//! \}
//! \endcond

#endif // BLEND2D_FONTFACE_P_H_INCLUDED
