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

struct BLFontFacePrivateFuncs {
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

BL_HIDDEN extern BLFontFacePrivateFuncs blNullFontFaceFuncs;

struct BLFontFacePrivateImpl : public BLFontFaceImpl {
  BLBitSetCore characterCoverage;
  BLFontFacePrivateFuncs funcs;
};

template<typename T = BLFontFacePrivateImpl>
static BL_INLINE T* blFontFaceGetImpl(const BLFontFaceCore* self) noexcept {
  return static_cast<T*>(static_cast<BLFontFacePrivateImpl*>(self->_d.impl));
}

static BL_INLINE void blFontFaceImplCtor(BLFontFacePrivateImpl* impl, BLFontFaceVirt* virt, BLFontFacePrivateFuncs& funcs) noexcept {
  impl->virt = virt;
  blCallCtor(impl->data.dcast());
  blCallCtor(impl->fullName.dcast());
  blCallCtor(impl->familyName.dcast());
  blCallCtor(impl->subfamilyName.dcast());
  blCallCtor(impl->postScriptName.dcast());
  blCallCtor(impl->scriptTags.dcast<BLArray<BLTag>>());
  blCallCtor(impl->featureTags.dcast<BLArray<BLTag>>());
  blObjectAtomicContentInit(&impl->characterCoverage);
  impl->funcs = funcs;
}

static BL_INLINE void blFontFaceImplDtor(BLFontFacePrivateImpl* impl) noexcept {
  if (blObjectAtomicContentTest(&impl->characterCoverage))
    blCallDtor(impl->characterCoverage.dcast());

  blCallDtor(impl->featureTags.dcast<BLArray<BLTag>>());
  blCallDtor(impl->scriptTags.dcast<BLArray<BLTag>>());
  blCallDtor(impl->postScriptName.dcast());
  blCallDtor(impl->subfamilyName.dcast());
  blCallDtor(impl->familyName.dcast());
  blCallDtor(impl->fullName.dcast());
  blCallDtor(impl->data.dcast());
}

//! \}

//! \}
//! \endcond

#endif // BLEND2D_FONTFACE_P_H_INCLUDED
