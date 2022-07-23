// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "glyphbuffer_p.h"
#include "font_p.h"
#include "fontface_p.h"
#include "matrix.h"
#include "object_p.h"
#include "path.h"
#include "runtime_p.h"
#include "support/intops_p.h"
#include "support/ptrops_p.h"
#include "support/scopedbuffer_p.h"

// BLFont - Globals
// ================

static BLObjectEthernalImpl<BLFontPrivateImpl> blFontDefaultImpl;

// BLFont - Internal Utilities
// ===========================

static void blFontCalcProperties(BLFontPrivateImpl* fontI, const BLFontFacePrivateImpl* faceI, float size) noexcept {
  const BLFontDesignMetrics& dm = faceI->designMetrics;

  double yScale = dm.unitsPerEm ? double(size) / double(dm.unitsPerEm) : 0.0;
  double xScale = yScale;

  fontI->metrics.size                   = size;
  fontI->metrics.ascent                 = float(double(dm.ascent                ) * yScale);
  fontI->metrics.descent                = float(double(dm.descent               ) * yScale);
  fontI->metrics.lineGap                = float(double(dm.lineGap               ) * yScale);
  fontI->metrics.xHeight                = float(double(dm.xHeight               ) * yScale);
  fontI->metrics.capHeight              = float(double(dm.capHeight             ) * yScale);
  fontI->metrics.vAscent                = float(double(dm.vAscent               ) * yScale);
  fontI->metrics.vDescent               = float(double(dm.vDescent              ) * yScale);
  fontI->metrics.xMin                   = float(double(dm.glyphBoundingBox.x0   ) * xScale);
  fontI->metrics.yMin                   = float(double(dm.glyphBoundingBox.y0   ) * yScale);
  fontI->metrics.xMax                   = float(double(dm.glyphBoundingBox.x1   ) * xScale);
  fontI->metrics.yMax                   = float(double(dm.glyphBoundingBox.y1   ) * yScale);
  fontI->metrics.underlinePosition      = float(double(dm.underlinePosition     ) * yScale);
  fontI->metrics.underlineThickness     = float(double(dm.underlineThickness    ) * yScale);
  fontI->metrics.strikethroughPosition  = float(double(dm.strikethroughPosition ) * yScale);
  fontI->metrics.strikethroughThickness = float(double(dm.strikethroughThickness) * yScale);
  fontI->matrix.reset(xScale, 0.0, 0.0, -yScale);
}

// BLFont - Alloc & Free Impl
// ==========================

static BL_INLINE BLFontPrivateImpl* blFontPrivateAllocImpl(BLFontCore* self, const BLFontFaceCore* face, float size) noexcept {
  BLObjectImplSize implSize(sizeof(BLFontPrivateImpl));
  BLFontPrivateImpl* impl = blObjectDetailAllocImplT<BLFontPrivateImpl>(self, BLObjectInfo::packType(BL_OBJECT_TYPE_FONT), implSize);

  if (BL_UNLIKELY(!impl))
    return impl;

  blCallCtor(impl->face.dcast(), face->dcast());
  blCallCtor(impl->featureSettings.dcast());
  blCallCtor(impl->variationSettings.dcast());
  impl->weight = 0;
  impl->stretch = 0;
  impl->style = 0;
  blFontCalcProperties(impl, blFontFaceGetImpl(face), size);

  return impl;
}

BLResult blFontImplFree(BLFontPrivateImpl* impl, BLObjectInfo info) noexcept {
  blCallDtor(impl->variationSettings.dcast());
  blCallDtor(impl->featureSettings.dcast());
  blCallDtor(impl->face.dcast());

  return blObjectImplFreeInline(impl, info);
}

static BL_INLINE BLResult blFontPrivateRelease(BLFontCore* self) noexcept {
  BLFontPrivateImpl* impl = blFontGetImpl(self);
  BLObjectInfo info = self->_d.info;

  if (info.refCountedFlag() && blObjectImplDecRefAndTest(impl, info))
    return blFontImplFree(impl, info);

  return BL_SUCCESS;
}

static BL_INLINE BLResult blFontPrivateReplace(BLFontCore* self, const BLFontCore* other) noexcept {
  BLFontPrivateImpl* impl = blFontGetImpl(self);
  BLObjectInfo info = self->_d.info;

  self->_d = other->_d;

  if (info.refCountedFlag() && blObjectImplDecRefAndTest(impl, info))
    return blFontImplFree(impl, info);

  return BL_SUCCESS;
}

static BL_NOINLINE BLResult blFontPrivateMakeMutableInternal(BLFontCore* self) noexcept {
  BLFontPrivateImpl* selfI = blFontGetImpl(self);

  BLFontCore newO;
  BLObjectImplSize implSize(sizeof(BLFontPrivateImpl));
  BLFontPrivateImpl* newI = blObjectDetailAllocImplT<BLFontPrivateImpl>(&newO, BLObjectInfo::packType(BL_OBJECT_TYPE_FONT), implSize);

  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  blCallCtor(newI->face.dcast(), selfI->face.dcast());
  newI->weight = selfI->weight;
  newI->stretch = selfI->stretch;
  newI->style = selfI->style;
  newI->reserved = 0;
  newI->metrics = selfI->metrics;
  newI->matrix = selfI->matrix;
  blCallCtor(newI->featureSettings.dcast(), selfI->featureSettings.dcast());
  blCallCtor(newI->variationSettings.dcast(), selfI->variationSettings.dcast());

  return blFontPrivateReplace(self, &newO);
}

static BL_INLINE BLResult blFontPrivateMakeMutable(BLFontCore* self) noexcept {
  if (blFontPrivateIsMutable(self))
    return BL_SUCCESS;

  return blFontPrivateMakeMutableInternal(self);
}

// BLFont - Init & Destroy
// =======================

BLResult blFontInit(BLFontCore* self) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT]._d;
  return BL_SUCCESS;
}

BLResult blFontInitMove(BLFontCore* self, BLFontCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFont());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT]._d;

  return BL_SUCCESS;
}

BLResult blFontInitWeak(BLFontCore* self, const BLFontCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFont());

  return blObjectPrivateInitWeakTagged(self, other);
}

BLResult blFontDestroy(BLFontCore* self) noexcept {
  BL_ASSERT(self->_d.isFont());

  return blFontPrivateRelease(self);
}

// BLFont - Reset
// ==============

BLResult blFontReset(BLFontCore* self) noexcept {
  BL_ASSERT(self->_d.isFont());

  return blFontPrivateReplace(self, static_cast<BLFontCore*>(&blObjectDefaults[BL_OBJECT_TYPE_FONT]));
}

// BLFont - Assign
// ===============

BLResult blFontAssignMove(BLFontCore* self, BLFontCore* other) noexcept {
  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(other->_d.isFont());

  BLFontCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT]._d;
  return blFontPrivateReplace(self, &tmp);
}

BLResult blFontAssignWeak(BLFontCore* self, const BLFontCore* other) noexcept {
  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(other->_d.isFont());

  blObjectPrivateAddRefTagged(other);
  return blFontPrivateReplace(self, other);
}

// BLFont - Equality & Comparison
// ==============================

bool blFontEquals(const BLFontCore* a, const BLFontCore* b) noexcept {
  BL_ASSERT(a->_d.isFont());
  BL_ASSERT(b->_d.isFont());

  return a->_d.impl == b->_d.impl;
}

// BLFont - Create
// ===============

BLResult blFontCreateFromFace(BLFontCore* self, const BLFontFaceCore* face, float size) noexcept {
  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(face->_d.isFontFace());

  if (!face->dcast().isValid())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  if (blFontPrivateIsMutable(self)) {
    BLFontPrivateImpl* selfI = blFontGetImpl(self);
    BLFontFacePrivateImpl* faceI = blFontFaceGetImpl(face);

    selfI->featureSettings.dcast().clear();
    selfI->variationSettings.dcast().clear();
    selfI->weight = 0;
    selfI->stretch = 0;
    selfI->style = 0;
    blFontCalcProperties(selfI, faceI, size);

    return blObjectPrivateAssignWeakVirtual(&selfI->face, face);
  }
  else {
    BLFontCore newO;
    BLFontPrivateImpl* newI = blFontPrivateAllocImpl(&newO, face, size);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    return blFontPrivateReplace(self, &newO);
  }
}

// BLFont - Accessors
// ==================

float blFontGetSize(const BLFontCore* self) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  return selfI->metrics.size;
}

BLResult blFontSetSize(BLFontCore* self, float size) noexcept {
  BL_ASSERT(self->_d.isFont());

  if (blFontGetImpl(self)->face.dcast().empty())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  BL_PROPAGATE(blFontPrivateMakeMutable(self));
  BLFontPrivateImpl* selfI = blFontGetImpl(self);

  blFontCalcProperties(selfI, blFontFaceGetImpl(&selfI->face), size);
  return BL_SUCCESS;
}

BLResult blFontGetMetrics(const BLFontCore* self, BLFontMetrics* out) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  *out = selfI->metrics;
  return BL_SUCCESS;
}

BLResult blFontGetMatrix(const BLFontCore* self, BLFontMatrix* out) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  *out = selfI->matrix;
  return BL_SUCCESS;
}

BLResult blFontGetDesignMetrics(const BLFontCore* self, BLFontDesignMetrics* out) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  BLFontFacePrivateImpl* faceI = blFontFaceGetImpl(&selfI->face);

  *out = faceI->designMetrics;
  return BL_SUCCESS;
}

BLResult blFontGetFeatureSettings(const BLFontCore* self, BLFontFeatureSettingsCore* out) noexcept {
  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(out->_d.isFontFeatureSettings());

  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  return blFontFeatureSettingsAssignWeak(out, &selfI->featureSettings);
}

BLResult blFontSetFeatureSettings(BLFontCore* self, const BLFontFeatureSettingsCore* featureSettings) noexcept {
  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(featureSettings->_d.isFontFeatureSettings());

  if (blFontGetImpl(self)->face.dcast().empty())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  BL_PROPAGATE(blFontPrivateMakeMutable(self));
  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  return blFontFeatureSettingsAssignWeak(&selfI->featureSettings, featureSettings);
}

BLResult blFontResetFeatureSettings(BLFontCore* self) noexcept {
  // Don't make the font mutable if there are no feature settings set.
  if (blFontGetImpl(self)->featureSettings.dcast().empty())
    return BL_SUCCESS;

  BL_PROPAGATE(blFontPrivateMakeMutable(self));
  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  return blFontFeatureSettingsReset(&selfI->featureSettings);
}

// BLFont - Shaping
// ================

BLResult blFontShape(const BLFontCore* self, BLGlyphBufferCore* gb) noexcept {
  BL_ASSERT(self->_d.isFont());

  BL_PROPAGATE(blFontMapTextToGlyphs(self, gb, nullptr));
  BL_PROPAGATE(blFontPositionGlyphs(self, gb, 0xFFFFFFFFu));

  return BL_SUCCESS;
}

BLResult blFontMapTextToGlyphs(const BLFontCore* self, BLGlyphBufferCore* gb, BLGlyphMappingState* stateOut) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  BLFontFacePrivateImpl* faceI = blFontFaceGetImpl(&selfI->face);
  BLGlyphBufferPrivateImpl* gbI = blGlyphBufferGetImpl(gb);

  if (!gbI->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(!(gbI->flags & BL_GLYPH_RUN_FLAG_UCS4_CONTENT)))
    return blTraceError(BL_ERROR_INVALID_STATE);

  BLGlyphMappingState state;
  if (!stateOut)
    stateOut = &state;

  BL_PROPAGATE(faceI->funcs.mapTextToGlyphs(faceI, gbI->content, gbI->size, stateOut));

  gbI->flags = gbI->flags & ~BL_GLYPH_RUN_FLAG_UCS4_CONTENT;
  if (stateOut->undefinedCount > 0)
    gbI->flags |= BL_GLYPH_RUN_FLAG_UNDEFINED_GLYPHS;

  return BL_SUCCESS;
}

BLResult blFontPositionGlyphs(const BLFontCore* self, BLGlyphBufferCore* gb, uint32_t positioningFlags) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  BLFontFacePrivateImpl* faceI = blFontFaceGetImpl(&selfI->face);
  BLGlyphBufferPrivateImpl* gbI = blGlyphBufferGetImpl(gb);

  if (!gbI->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(gbI->flags & BL_GLYPH_RUN_FLAG_UCS4_CONTENT))
    return blTraceError(BL_ERROR_INVALID_STATE);

  if (!(gbI->flags & BL_GLYPH_BUFFER_GLYPH_ADVANCES)) {
    BL_PROPAGATE(gbI->ensurePlacement());
    faceI->funcs.getGlyphAdvances(faceI, gbI->content, sizeof(uint32_t), gbI->placementData, gbI->size);
    gbI->glyphRun.placementType = uint8_t(BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET);
    gbI->flags |= BL_GLYPH_BUFFER_GLYPH_ADVANCES;
  }

  if (positioningFlags) {
    faceI->funcs.applyKern(faceI, gbI->content, gbI->placementData, gbI->size);
  }

  return BL_SUCCESS;
}

BLResult blFontApplyKerning(const BLFontCore* self, BLGlyphBufferCore* gb) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  BLFontFacePrivateImpl* faceI = blFontFaceGetImpl(&selfI->face);
  BLGlyphBufferPrivateImpl* gbI = blGlyphBufferGetImpl(gb);

  if (!gbI->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(!(gbI->placementData)))
    return blTraceError(BL_ERROR_INVALID_STATE);

  return faceI->funcs.applyKern(faceI, gbI->content, gbI->placementData, gbI->size);
}

BLResult blFontApplyGSub(const BLFontCore* self, BLGlyphBufferCore* gb, const BLBitSetCore* lookups) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  BLFontFacePrivateImpl* faceI = blFontFaceGetImpl(&selfI->face);

  return faceI->funcs.applyGSub(faceI, static_cast<BLGlyphBuffer*>(gb), lookups);
}

BLResult blFontApplyGPos(const BLFontCore* self, BLGlyphBufferCore* gb, const BLBitSetCore* lookups) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  BLFontFacePrivateImpl* faceI = blFontFaceGetImpl(&selfI->face);
  BLGlyphBufferPrivateImpl* gbI = blGlyphBufferGetImpl(gb);

  if (!gbI->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(!(gbI->placementData)))
    return blTraceError(BL_ERROR_INVALID_STATE);

  return faceI->funcs.applyGPos(faceI, static_cast<BLGlyphBuffer*>(gb), lookups);
}

BLResult blFontGetTextMetrics(const BLFontCore* self, BLGlyphBufferCore* gb, BLTextMetrics* out) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  BLGlyphBufferPrivateImpl* gbI = blGlyphBufferGetImpl(gb);

  out->reset();
  if (!(gbI->flags & BL_GLYPH_BUFFER_GLYPH_ADVANCES)) {
    BL_PROPAGATE(blFontShape(self, gb));
    gbI = blGlyphBufferGetImpl(gb);
  }

  size_t size = gbI->size;
  if (!size)
    return BL_SUCCESS;

  BLPoint advance {};

  const uint32_t* glyphData = gbI->content;
  const BLGlyphPlacement* placementData = gbI->placementData;

  for (size_t i = 0; i < size; i++) {
    advance += BLPoint(placementData[i].advance);
  }

  BLBoxI glyphBounds[2];
  uint32_t borderGlyphs[2] = { glyphData[0], glyphData[size - 1] };

  BL_PROPAGATE(blFontGetGlyphBounds(self, borderGlyphs, intptr_t(sizeof(uint32_t)), glyphBounds, 2));
  out->advance = advance;

  double lsb = glyphBounds[0].x0;
  double rsb = placementData[size - 1].advance.x - glyphBounds[1].x1;

  out->leadingBearing.reset(lsb, 0);
  out->trailingBearing.reset(rsb, 0);
  out->boundingBox.reset(glyphBounds[0].x0, 0.0, advance.x - rsb, 0.0);

  const BLFontMatrix& m = selfI->matrix;
  BLPoint scalePt = BLPoint(m.m00, m.m11);

  out->advance *= scalePt;
  out->leadingBearing *= scalePt;
  out->trailingBearing *= scalePt;
  out->boundingBox *= scalePt;

  return BL_SUCCESS;
}

// BLFont - Low-Level API
// ======================

BLResult blFontGetGlyphBounds(const BLFontCore* self, const uint32_t* glyphData, intptr_t glyphAdvance, BLBoxI* out, size_t count) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  BLFontFacePrivateImpl* faceI = blFontFaceGetImpl(&selfI->face);

  return faceI->funcs.getGlyphBounds(faceI, glyphData, glyphAdvance, out, count);
}

BLResult blFontGetGlyphAdvances(const BLFontCore* self, const uint32_t* glyphData, intptr_t glyphAdvance, BLGlyphPlacement* out, size_t count) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  BLFontFacePrivateImpl* faceI = blFontFaceGetImpl(&selfI->face);

  return faceI->funcs.getGlyphAdvances(faceI, glyphData, glyphAdvance, out, count);
}

// BLFont - Glyph Outlines
// =======================

static BLResult BL_CDECL blFontDummyPathSink(BLPathCore* path, const void* info, void* closure) noexcept {
  blUnused(path, info, closure);
  return BL_SUCCESS;
}

BLResult blFontGetGlyphOutlines(const BLFontCore* self, uint32_t glyphId, const BLMatrix2D* userMatrix, BLPathCore* out, BLPathSinkFunc sink, void* closure) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  BLFontFacePrivateImpl* faceI = blFontFaceGetImpl(&selfI->face);

  BLMatrix2D finalMatrix;
  const BLFontMatrix& fMat = selfI->matrix;

  if (userMatrix)
    blFontMatrixMultiply(&finalMatrix, &fMat, userMatrix);
  else
    finalMatrix.reset(fMat.m00, fMat.m01, fMat.m10, fMat.m11, 0.0, 0.0);

  BLScopedBufferTmp<BL_FONT_GET_GLYPH_OUTLINE_BUFFER_SIZE> tmpBuffer;
  BLGlyphOutlineSinkInfo sinkInfo;
  BL_PROPAGATE(faceI->funcs.getGlyphOutlines(faceI, glyphId, &finalMatrix, static_cast<BLPath*>(out), &sinkInfo.contourCount, &tmpBuffer));

  if (!sink)
    return BL_SUCCESS;

  sinkInfo.glyphIndex = 0;
  return sink(out, &sinkInfo, closure);
}

BLResult blFontGetGlyphRunOutlines(const BLFontCore* self, const BLGlyphRun* glyphRun, const BLMatrix2D* userMatrix, BLPathCore* out, BLPathSinkFunc sink, void* closure) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = blFontGetImpl(self);
  BLFontFacePrivateImpl* faceI = blFontFaceGetImpl(&selfI->face);

  if (!glyphRun->size)
    return BL_SUCCESS;

  BLMatrix2D finalMatrix;
  const BLFontMatrix& fMat = selfI->matrix;

  if (userMatrix) {
    blFontMatrixMultiply(&finalMatrix, &fMat, userMatrix);
  }
  else {
    userMatrix = &BLTransformPrivate::identityTransform;
    finalMatrix.reset(fMat.m00, fMat.m01, fMat.m10, fMat.m11, 0.0, 0.0);
  }

  if (!sink)
    sink = blFontDummyPathSink;

  BLScopedBufferTmp<BL_FONT_GET_GLYPH_OUTLINE_BUFFER_SIZE> tmpBuffer;
  BLGlyphOutlineSinkInfo sinkInfo;

  uint32_t placementType = glyphRun->placementType;
  BLGlyphRunIterator it(*glyphRun);
  auto getGlyphOutlinesFunc = faceI->funcs.getGlyphOutlines;

  if (it.hasPlacement() && placementType != BL_GLYPH_PLACEMENT_TYPE_NONE) {
    BLMatrix2D offsetMatrix(1.0, 0.0, 0.0, 1.0, finalMatrix.m20, finalMatrix.m21);

    switch (placementType) {
      case BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET:
      case BL_GLYPH_PLACEMENT_TYPE_DESIGN_UNITS:
        offsetMatrix.m00 = finalMatrix.m00;
        offsetMatrix.m01 = finalMatrix.m01;
        offsetMatrix.m10 = finalMatrix.m10;
        offsetMatrix.m11 = finalMatrix.m11;
        break;

      case BL_GLYPH_PLACEMENT_TYPE_USER_UNITS:
        offsetMatrix.m00 = userMatrix->m00;
        offsetMatrix.m01 = userMatrix->m01;
        offsetMatrix.m10 = userMatrix->m10;
        offsetMatrix.m11 = userMatrix->m11;
        break;
    }

    if (placementType == BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET) {
      double ox = finalMatrix.m20;
      double oy = finalMatrix.m21;
      double px;
      double py;

      while (!it.atEnd()) {
        const BLGlyphPlacement& pos = it.placement<BLGlyphPlacement>();

        px = pos.placement.x;
        py = pos.placement.y;
        finalMatrix.m20 = px * offsetMatrix.m00 + py * offsetMatrix.m10 + ox;
        finalMatrix.m21 = px * offsetMatrix.m01 + py * offsetMatrix.m11 + oy;

        sinkInfo.glyphIndex = it.index;
        BL_PROPAGATE(getGlyphOutlinesFunc(faceI, it.glyphId(), &finalMatrix, static_cast<BLPath*>(out), &sinkInfo.contourCount, &tmpBuffer));
        BL_PROPAGATE(sink(out, &sinkInfo, closure));
        it.advance();

        px = pos.advance.x;
        py = pos.advance.y;
        ox += px * offsetMatrix.m00 + py * offsetMatrix.m10;
        oy += px * offsetMatrix.m01 + py * offsetMatrix.m11;
      }
    }
    else {
      while (!it.atEnd()) {
        const BLPoint& placement = it.placement<BLPoint>();
        finalMatrix.m20 = placement.x * offsetMatrix.m00 + placement.y * offsetMatrix.m10 + offsetMatrix.m20;
        finalMatrix.m21 = placement.x * offsetMatrix.m01 + placement.y * offsetMatrix.m11 + offsetMatrix.m21;

        sinkInfo.glyphIndex = it.index;
        BL_PROPAGATE(getGlyphOutlinesFunc(faceI, it.glyphId(), &finalMatrix, static_cast<BLPath*>(out), &sinkInfo.contourCount, &tmpBuffer));
        BL_PROPAGATE(sink(out, &sinkInfo, closure));
        it.advance();
      }
    }
  }
  else {
    while (!it.atEnd()) {
      sinkInfo.glyphIndex = it.index;
      BL_PROPAGATE(getGlyphOutlinesFunc(faceI, it.glyphId(), &finalMatrix, static_cast<BLPath*>(out), &sinkInfo.contourCount, &tmpBuffer));
      BL_PROPAGATE(sink(out, &sinkInfo, closure));
      it.advance();
    }
  }

  return BL_SUCCESS;
}

// BLFont - Runtime Registration
// =============================

void blFontRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  // Initialize BLFont built-ins.
  blFontImplCtor(&blFontDefaultImpl.impl);
  blObjectDefaults[BL_OBJECT_TYPE_FONT]._d.initDynamic(
    BL_OBJECT_TYPE_FONT,
    BLObjectInfo{BL_OBJECT_INFO_IMMUTABLE_FLAG},
    &blFontDefaultImpl.impl);
}
