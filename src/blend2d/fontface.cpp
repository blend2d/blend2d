// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "glyphbuffer_p.h"
#include "filesystem.h"
#include "fontface_p.h"
#include "matrix.h"
#include "object_p.h"
#include "path.h"
#include "runtime_p.h"
#include "string_p.h"
#include "unicode_p.h"
#include "opentype/otcore_p.h"
#include "opentype/otface_p.h"
#include "support/intops_p.h"
#include "support/ptrops_p.h"
#include "support/scopedbuffer_p.h"
#include "threading/uniqueidgenerator_p.h"

// BLFontFace - Globals
// ====================

BLFontFacePrivateFuncs blNullFontFaceFuncs;
static BLObjectEthernalVirtualImpl<BLFontFacePrivateImpl, BLFontFaceVirt> blFontFaceDefaultImpl;

// BLFontFace - Default Impl
// =========================

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

static BLResult BL_CDECL blNullFontFaceImplDestroy(BLObjectImpl* impl, uint32_t info) noexcept {
  return BL_SUCCESS;
}

static BLResult BL_CDECL blNullFontFaceMapTextToGlyphs(
  const BLFontFaceImpl* impl,
  uint32_t* content,
  size_t count,
  BLGlyphMappingState* state) noexcept {

  state->reset();
  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceGetGlyphBounds(
  const BLFontFaceImpl* impl,
  const uint32_t* glyphData,
  intptr_t glyphAdvance,
  BLBoxI* boxes,
  size_t count) noexcept {

  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceGetGlyphAdvances(
  const BLFontFaceImpl* impl,
  const uint32_t* glyphData,
  intptr_t glyphAdvance,
  BLGlyphPlacement* placementData,
  size_t count) noexcept {

  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceGetGlyphOutlines(
  const BLFontFaceImpl* impl,
  uint32_t glyphId,
  const BLMatrix2D* userMatrix,
  BLPath* out,
  size_t* contourCountOut,
  BLScopedBuffer* tmpBuffer) noexcept {

  *contourCountOut = 0;
  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceApplyKern(
  const BLFontFaceImpl* faceI,
  uint32_t* glyphData,
  BLGlyphPlacement* placementData,
  size_t count) noexcept {

  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceApplyGSub(
  const BLFontFaceImpl* impl,
  BLGlyphBuffer* gb,
  const BLBitSetCore* lookups) noexcept {

  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceApplyGPos(
  const BLFontFaceImpl* impl,
  BLGlyphBuffer* gb,
  const BLBitSetCore* lookups) noexcept {

  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFacePositionGlyphs(
  const BLFontFaceImpl* impl,
  uint32_t* glyphData,
  BLGlyphPlacement* placementData,
  size_t count) noexcept {

  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

BL_DIAGNOSTIC_POP

// BLFontFace - Init & Destroy
// ===========================

BLResult blFontFaceInit(BLFontFaceCore* self) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_FACE]._d;
  return BL_SUCCESS;
}

BLResult blFontFaceInitMove(BLFontFaceCore* self, BLFontFaceCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontFace());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_FACE]._d;

  return BL_SUCCESS;
}

BLResult blFontFaceInitWeak(BLFontFaceCore* self, const BLFontFaceCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontFace());

  return blObjectPrivateInitWeakTagged(self, other);
}

BLResult blFontFaceDestroy(BLFontFaceCore* self) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  return blObjectPrivateReleaseVirtual(self);
}

// BLFontFace - Reset
// ==================

BLResult blFontFaceReset(BLFontFaceCore* self) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  return blObjectPrivateReplaceVirtual(self, static_cast<BLFontFaceCore*>(&blObjectDefaults[BL_OBJECT_TYPE_FONT_FACE]));
}

// BLFontFace - Assign
// ===================

BLResult blFontFaceAssignMove(BLFontFaceCore* self, BLFontFaceCore* other) noexcept {
  BL_ASSERT(self->_d.isFontFace());
  BL_ASSERT(other->_d.isFontFace());

  BLFontFaceCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_FACE]._d;
  return blObjectPrivateReplaceVirtual(self, &tmp);
}

BLResult blFontFaceAssignWeak(BLFontFaceCore* self, const BLFontFaceCore* other) noexcept {
  BL_ASSERT(self->_d.isFontFace());
  BL_ASSERT(other->_d.isFontFace());

  return blObjectPrivateAssignWeakVirtual(self, other);
}

// BLFontFace - Equality & Comparison
// ==================================

bool blFontFaceEquals(const BLFontFaceCore* a, const BLFontFaceCore* b) noexcept {
  BL_ASSERT(a->_d.isFontFace());
  BL_ASSERT(b->_d.isFontFace());

  return a->_d.impl == b->_d.impl;
}

// BLFontFace - Create
// ===================

BLResult blFontFaceCreateFromFile(BLFontFaceCore* self, const char* fileName, BLFileReadFlags readFlags) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  BLFontData fontData;
  BL_PROPAGATE(fontData.createFromFile(fileName, readFlags));
  return blFontFaceCreateFromData(self, &fontData, 0);
}

BLResult blFontFaceCreateFromData(BLFontFaceCore* self, const BLFontDataCore* fontData, uint32_t faceIndex) noexcept {
  BL_ASSERT(self->_d.isFontFace());
  BL_ASSERT(fontData->_d.isFontData());

  if (BL_UNLIKELY(!fontData->dcast().isValid()))
    return blTraceError(BL_ERROR_NOT_INITIALIZED);

  if (faceIndex >= fontData->dcast().faceCount())
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLFontFaceCore newO;
  BL_PROPAGATE(BLOpenType::createOpenTypeFace(&newO, static_cast<const BLFontData*>(fontData), faceIndex));

  // TODO: Move to OTFace?
  blFontFaceGetImpl<BLOpenType::OTFaceImpl>(&newO)->uniqueId = BLUniqueIdGenerator::generateId(BLUniqueIdGenerator::Domain::kAny);

  return blObjectPrivateReplaceVirtual(self, &newO);
}

// BLFontFace - Accessors
// ======================

BLResult blFontFaceGetFaceInfo(const BLFontFaceCore* self, BLFontFaceInfo* out) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  *out = self->dcast().faceInfo();
  return BL_SUCCESS;
}

BLResult blFontFaceGetDesignMetrics(const BLFontFaceCore* self, BLFontDesignMetrics* out) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  *out = self->dcast().designMetrics();
  return BL_SUCCESS;
}

BLResult blFontFaceGetUnicodeCoverage(const BLFontFaceCore* self, BLFontUnicodeCoverage* out) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  *out = self->dcast().unicodeCoverage();
  return BL_SUCCESS;
}

BLResult blFontFaceGetCharacterCoverage(const BLFontFaceCore* self, BLBitSetCore* out) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  // Don't calculate the `characterCoverage` again if it was already calculated. We don't need atomics here as it
  // is set only once, atomics will be used only if it hasn't been calculated yet or if there is a race (already
  // calculated by another thread, but nullptr at this exact moment here).
  BLFontFacePrivateImpl* selfI = blFontFaceGetImpl(self);
  if (!blObjectAtomicContentTest(&selfI->characterCoverage)) {
    if (selfI->faceInfo.faceType != BL_FONT_FACE_TYPE_OPENTYPE)
      return blTraceError(BL_ERROR_NOT_IMPLEMENTED);

    BLBitSetCore tmpBitSet;
    BL_PROPAGATE(BLOpenType::CMapImpl::populateCharacterCoverage(static_cast<BLOpenType::OTFaceImpl*>(selfI), &tmpBitSet.dcast()));

    blBitSetShrink(&tmpBitSet);
    if (!blObjectAtomicContentMove(&selfI->characterCoverage, &tmpBitSet))
      return blBitSetAssignMove(out, &tmpBitSet);
  }

  return blBitSetAssignWeak(out, &selfI->characterCoverage);
}

BLResult blFontFaceGetScriptTags(const BLFontFaceCore* self, BLArrayCore* out) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  const BLFontFaceImpl* selfI = blFontFaceGetImpl(self);
  return blArrayAssignWeak(out, &selfI->scriptTags);
}

BLResult blFontFaceGetFeatureTags(const BLFontFaceCore* self, BLArrayCore* out) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  const BLFontFaceImpl* selfI = blFontFaceGetImpl(self);
  return blArrayAssignWeak(out, &selfI->featureTags);
}

// BLFontFace - Runtime Registration
// =================================

void blFontFaceRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  // Initialize BLFontFace built-ins.
  blNullFontFaceFuncs.mapTextToGlyphs = blNullFontFaceMapTextToGlyphs;
  blNullFontFaceFuncs.getGlyphBounds = blNullFontFaceGetGlyphBounds;
  blNullFontFaceFuncs.getGlyphAdvances = blNullFontFaceGetGlyphAdvances;
  blNullFontFaceFuncs.getGlyphOutlines = blNullFontFaceGetGlyphOutlines;
  blNullFontFaceFuncs.applyKern = blNullFontFaceApplyKern;
  blNullFontFaceFuncs.applyGSub = blNullFontFaceApplyGSub;
  blNullFontFaceFuncs.applyGPos = blNullFontFaceApplyGPos;
  blNullFontFaceFuncs.positionGlyphs = blNullFontFacePositionGlyphs;

  blFontFaceDefaultImpl.virt.base.destroy = blNullFontFaceImplDestroy;
  blFontFaceDefaultImpl.virt.base.getProperty = blObjectImplGetProperty;
  blFontFaceDefaultImpl.virt.base.setProperty = blObjectImplSetProperty;
  blFontFaceImplCtor(&blFontFaceDefaultImpl.impl, &blFontFaceDefaultImpl.virt, blNullFontFaceFuncs);

  blObjectDefaults[BL_OBJECT_TYPE_FONT_FACE]._d.initDynamic(
    BL_OBJECT_TYPE_FONT_FACE,
    BLObjectInfo{BL_OBJECT_INFO_IMMUTABLE_FLAG},
    &blFontFaceDefaultImpl.impl);
}
