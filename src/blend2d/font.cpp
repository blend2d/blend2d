// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "bitarray_p.h"
#include "glyphbuffer_p.h"
#include "font_p.h"
#include "fontface_p.h"
#include "fontfeaturesettings_p.h"
#include "matrix.h"
#include "object_p.h"
#include "path.h"
#include "runtime_p.h"
#include "support/intops_p.h"
#include "support/ptrops_p.h"
#include "support/scopedbuffer_p.h"

#include "opentype/otface_p.h"
#include "opentype/otlayout_p.h"

namespace bl {
namespace FontInternal {

// bl::Font - Globals
// ==================

static BLObjectEternalImpl<BLFontPrivateImpl> defaultFont;

// bl::Font - Internal Utilities
// =============================

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

// bl::Font - Internals - Alloc & Free Impl
// ========================================

static BL_INLINE BLResult allocImpl(BLFontCore* self, const BLFontFaceCore* face, float size) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLFontPrivateImpl>(self, info));

  BLFontPrivateImpl* impl = getImpl(self);
  blCallCtor(impl->face.dcast(), face->dcast());
  blCallCtor(impl->featureSettings.dcast());
  blCallCtor(impl->variationSettings.dcast());
  impl->weight = 0;
  impl->stretch = 0;
  impl->style = 0;
  blFontCalcProperties(impl, FontFaceInternal::getImpl(face), size);
  return BL_SUCCESS;
}

BLResult freeImpl(BLFontPrivateImpl* impl) noexcept {
  blCallDtor(impl->variationSettings.dcast());
  blCallDtor(impl->featureSettings.dcast());
  blCallDtor(impl->face.dcast());

  return ObjectInternal::freeImpl(impl);
}

// bl::Font - Internals - Make Mutable
// ===================================

static BL_NOINLINE BLResult makeMutableInternal(BLFontCore* self) noexcept {
  BLFontPrivateImpl* selfI = getImpl(self);

  BLFontCore newO;
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLFontPrivateImpl>(&newO, info));

  BLFontPrivateImpl* newI = getImpl(&newO);
  blCallCtor(newI->face.dcast(), selfI->face.dcast());
  newI->weight = selfI->weight;
  newI->stretch = selfI->stretch;
  newI->style = selfI->style;
  newI->reserved = 0;
  newI->metrics = selfI->metrics;
  newI->matrix = selfI->matrix;
  blCallCtor(newI->featureSettings.dcast(), selfI->featureSettings.dcast());
  blCallCtor(newI->variationSettings.dcast(), selfI->variationSettings.dcast());

  return replaceInstance(self, &newO);
}

static BL_INLINE BLResult makeMutable(BLFontCore* self) noexcept {
  if (isInstanceMutable(self))
    return BL_SUCCESS;

  return makeMutableInternal(self);
}

} // {FontInternal}
} // {bl}

// bl::Font - Init & Destroy
// =========================

BL_API_IMPL BLResult blFontInit(BLFontCore* self) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFontInitMove(BLFontCore* self, BLFontCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFont());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT]._d;

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFontInitWeak(BLFontCore* self, const BLFontCore* other) noexcept {
  using namespace bl::FontInternal;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFont());

  self->_d = other->_d;
  return retainInstance(self);
}

BL_API_IMPL BLResult blFontDestroy(BLFontCore* self) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  return releaseInstance(self);
}

// bl::Font - Reset
// ================

BL_API_IMPL BLResult blFontReset(BLFontCore* self) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  return replaceInstance(self, static_cast<BLFontCore*>(&blObjectDefaults[BL_OBJECT_TYPE_FONT]));
}

// bl::Font - Assign
// =================

BL_API_IMPL BLResult blFontAssignMove(BLFontCore* self, BLFontCore* other) noexcept {
  using namespace bl::FontInternal;

  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(other->_d.isFont());

  BLFontCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT]._d;
  return replaceInstance(self, &tmp);
}

BL_API_IMPL BLResult blFontAssignWeak(BLFontCore* self, const BLFontCore* other) noexcept {
  using namespace bl::FontInternal;

  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(other->_d.isFont());

  retainInstance(other);
  return replaceInstance(self, other);
}

// bl::Font - Equality & Comparison
// ================================

BL_API_IMPL bool blFontEquals(const BLFontCore* a, const BLFontCore* b) noexcept {
  BL_ASSERT(a->_d.isFont());
  BL_ASSERT(b->_d.isFont());

  return a->_d.impl == b->_d.impl;
}

// bl::Font - Create
// =================

BL_API_IMPL BLResult blFontCreateFromFace(BLFontCore* self, const BLFontFaceCore* face, float size) noexcept {
  using namespace bl::FontInternal;

  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(face->_d.isFontFace());

  if (!face->dcast().isValid())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  BLFontPrivateImpl* selfI = getImpl(self);
  if (isImplMutable(selfI)) {
    BLFontFacePrivateImpl* faceI = bl::FontFaceInternal::getImpl(face);

    selfI->featureSettings.dcast().clear();
    selfI->variationSettings.dcast().clear();
    selfI->weight = 0;
    selfI->stretch = 0;
    selfI->style = 0;
    blFontCalcProperties(selfI, faceI, size);

    return bl::ObjectInternal::assignVirtualInstance(&selfI->face, face);
  }
  else {
    BLFontCore newO;
    BL_PROPAGATE(allocImpl(&newO, face, size));
    return replaceInstance(self, &newO);
  }
}

BL_API_IMPL BLResult blFontCreateFromFaceWithSettings(BLFontCore* self, const BLFontFaceCore* face, float size, const BLFontFeatureSettingsCore* featureSettings, const BLFontVariationSettingsCore* variationSettings) noexcept {
  using namespace bl::FontInternal;

  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(face->_d.isFontFace());

  if (featureSettings == nullptr)
    featureSettings = static_cast<BLFontFeatureSettings*>(&blObjectDefaults[BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS]);

  if (variationSettings == nullptr)
    variationSettings = static_cast<BLFontVariationSettings*>(&blObjectDefaults[BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS]);

  BL_ASSERT(featureSettings->_d.isFontFeatureSettings());
  BL_ASSERT(variationSettings->_d.isFontVariationSettings());

  if (!face->dcast().isValid())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  BLFontPrivateImpl* selfI = getImpl(self);
  if (isImplMutable(selfI)) {
    BLFontFacePrivateImpl* faceI = bl::FontFaceInternal::getImpl(face);

    selfI->featureSettings.dcast().assign(featureSettings->dcast());
    selfI->variationSettings.dcast().assign(variationSettings->dcast());
    selfI->weight = 0;
    selfI->stretch = 0;
    selfI->style = 0;
    blFontCalcProperties(selfI, faceI, size);

    return bl::ObjectInternal::assignVirtualInstance(&selfI->face, face);
  }
  else {
    BLFontCore newO;
    BL_PROPAGATE(allocImpl(&newO, face, size));

    BLFontPrivateImpl* newI = getImpl(&newO);
    newI->featureSettings.dcast().assign(featureSettings->dcast());
    newI->variationSettings.dcast().assign(variationSettings->dcast());
    return replaceInstance(self, &newO);
  }
}

// bl::Font - Accessors
// ====================

BL_API_IMPL BLResult blFontGetFace(const BLFontCore* self, BLFontFaceCore* out) noexcept {
  using namespace bl::FontInternal;

  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(out->_d.isFontFace());

  BLFontPrivateImpl* selfI = getImpl(self);
  return blFontFaceAssignWeak(out, &selfI->face);
}

BL_API_IMPL float blFontGetSize(const BLFontCore* self) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = getImpl(self);
  return selfI->metrics.size;
}

BL_API_IMPL BLResult blFontSetSize(BLFontCore* self, float size) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  if (getImpl(self)->face.dcast().empty())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  BL_PROPAGATE(makeMutable(self));
  BLFontPrivateImpl* selfI = getImpl(self);

  blFontCalcProperties(selfI, bl::FontFaceInternal::getImpl(&selfI->face), size);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFontGetMetrics(const BLFontCore* self, BLFontMetrics* out) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = getImpl(self);
  *out = selfI->metrics;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFontGetMatrix(const BLFontCore* self, BLFontMatrix* out) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = getImpl(self);
  *out = selfI->matrix;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFontGetDesignMetrics(const BLFontCore* self, BLFontDesignMetrics* out) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = getImpl(self);
  BLFontFacePrivateImpl* faceI = bl::FontFaceInternal::getImpl(&selfI->face);

  *out = faceI->designMetrics;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFontGetFeatureSettings(const BLFontCore* self, BLFontFeatureSettingsCore* out) noexcept {
  using namespace bl::FontInternal;

  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(out->_d.isFontFeatureSettings());

  BLFontPrivateImpl* selfI = getImpl(self);
  return blFontFeatureSettingsAssignWeak(out, &selfI->featureSettings);
}

BL_API_IMPL BLResult blFontSetFeatureSettings(BLFontCore* self, const BLFontFeatureSettingsCore* featureSettings) noexcept {
  using namespace bl::FontInternal;

  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(featureSettings->_d.isFontFeatureSettings());

  if (getImpl(self)->face.dcast().empty())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  BL_PROPAGATE(makeMutable(self));
  BLFontPrivateImpl* selfI = getImpl(self);
  return blFontFeatureSettingsAssignWeak(&selfI->featureSettings, featureSettings);
}

BL_API_IMPL BLResult blFontResetFeatureSettings(BLFontCore* self) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  // Don't make the font mutable if there are no feature settings set.
  if (getImpl(self)->featureSettings.dcast().empty())
    return BL_SUCCESS;

  BL_PROPAGATE(makeMutable(self));
  BLFontPrivateImpl* selfI = getImpl(self);
  return blFontFeatureSettingsReset(&selfI->featureSettings);
}

BL_API_IMPL BLResult blFontGetVariationSettings(const BLFontCore* self, BLFontVariationSettingsCore* out) noexcept {
  using namespace bl::FontInternal;

  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(out->_d.isFontVariationSettings());

  BLFontPrivateImpl* selfI = getImpl(self);
  return blFontVariationSettingsAssignWeak(out, &selfI->variationSettings);
}

BL_API_IMPL BLResult blFontSetVariationSettings(BLFontCore* self, const BLFontVariationSettingsCore* variationSettings) noexcept {
  using namespace bl::FontInternal;

  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(variationSettings->_d.isFontVariationSettings());

  if (getImpl(self)->face.dcast().empty())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  BL_PROPAGATE(makeMutable(self));
  BLFontPrivateImpl* selfI = getImpl(self);
  return blFontVariationSettingsAssignWeak(&selfI->variationSettings, variationSettings);
}

BL_API_IMPL BLResult blFontResetVariationSettings(BLFontCore* self) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  // Don't make the font mutable if there are no variation settings set.
  if (getImpl(self)->variationSettings.dcast().empty())
    return BL_SUCCESS;

  BL_PROPAGATE(makeMutable(self));
  BLFontPrivateImpl* selfI = getImpl(self);
  return blFontVariationSettingsReset(&selfI->variationSettings);
}

// bl::Font - Shaping
// ==================

BL_API_IMPL BLResult blFontShape(const BLFontCore* self, BLGlyphBufferCore* gb) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  BL_PROPAGATE(blFontMapTextToGlyphs(self, gb, nullptr));

  bl::OpenType::OTFaceImpl* faceI = bl::FontFaceInternal::getImpl<bl::OpenType::OTFaceImpl>(&self->dcast().face());
  if (faceI->layout.gsub().lookupCount) {
    BLBitArray plan;
    BL_PROPAGATE(bl::OpenType::LayoutImpl::calculateGSubPlan(faceI, self->dcast().featureSettings(), &plan));
    BL_PROPAGATE(blFontApplyGSub(self, gb, &plan));
  }

  return blFontPositionGlyphs(self, gb);
}

BL_API_IMPL BLResult blFontMapTextToGlyphs(const BLFontCore* self, BLGlyphBufferCore* gb, BLGlyphMappingState* stateOut) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = getImpl(self);
  BLFontFacePrivateImpl* faceI = bl::FontFaceInternal::getImpl(&selfI->face);
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

BL_API_IMPL BLResult blFontPositionGlyphs(const BLFontCore* self, BLGlyphBufferCore* gb) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = getImpl(self);
  BLFontFacePrivateImpl* faceI = bl::FontFaceInternal::getImpl(&selfI->face);
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

  bl::OpenType::OTFaceImpl* otFaceI = bl::FontFaceInternal::getImpl<bl::OpenType::OTFaceImpl>(&self->dcast().face());
  if (otFaceI->layout.gpos().lookupCount) {
    BLBitArray plan;
    BL_PROPAGATE(bl::OpenType::LayoutImpl::calculateGPosPlan(otFaceI, self->dcast().featureSettings(), &plan));
    return blFontApplyGPos(self, gb, &plan);
  }
  else if (!otFaceI->kern.table.empty()) {
    if (selfI->featureSettings.dcast().getValue(BL_MAKE_TAG('k', 'e', 'r', 'n')) != 0u)
      return faceI->funcs.applyKern(faceI, gbI->content, gbI->placementData, gbI->size);
  }

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFontApplyKerning(const BLFontCore* self, BLGlyphBufferCore* gb) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = getImpl(self);
  BLFontFacePrivateImpl* faceI = bl::FontFaceInternal::getImpl(&selfI->face);
  BLGlyphBufferPrivateImpl* gbI = blGlyphBufferGetImpl(gb);

  if (!gbI->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(!(gbI->placementData)))
    return blTraceError(BL_ERROR_INVALID_STATE);

  return faceI->funcs.applyKern(faceI, gbI->content, gbI->placementData, gbI->size);
}

BL_API_IMPL BLResult blFontApplyGSub(const BLFontCore* self, BLGlyphBufferCore* gb, const BLBitArrayCore* lookups) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = getImpl(self);
  BLFontFacePrivateImpl* faceI = bl::FontFaceInternal::getImpl(&selfI->face);

  return faceI->funcs.applyGSub(faceI, static_cast<BLGlyphBuffer*>(gb), lookups->dcast().data(), lookups->dcast().wordCount());
}

BL_API_IMPL BLResult blFontApplyGPos(const BLFontCore* self, BLGlyphBufferCore* gb, const BLBitArrayCore* lookups) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = getImpl(self);
  BLFontFacePrivateImpl* faceI = bl::FontFaceInternal::getImpl(&selfI->face);
  BLGlyphBufferPrivateImpl* gbI = blGlyphBufferGetImpl(gb);

  if (!gbI->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(!(gbI->placementData)))
    return blTraceError(BL_ERROR_INVALID_STATE);

  return faceI->funcs.applyGPos(faceI, static_cast<BLGlyphBuffer*>(gb), lookups->dcast().data(), lookups->dcast().wordCount());
}

BL_API_IMPL BLResult blFontGetTextMetrics(const BLFontCore* self, BLGlyphBufferCore* gb, BLTextMetrics* out) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = getImpl(self);
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

// bl::Font - Low-Level API
// ========================

BL_API_IMPL BLResult blFontGetGlyphBounds(const BLFontCore* self, const uint32_t* glyphData, intptr_t glyphAdvance, BLBoxI* out, size_t count) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = getImpl(self);
  BLFontFacePrivateImpl* faceI = bl::FontFaceInternal::getImpl(&selfI->face);

  return faceI->funcs.getGlyphBounds(faceI, glyphData, glyphAdvance, out, count);
}

BL_API_IMPL BLResult blFontGetGlyphAdvances(const BLFontCore* self, const uint32_t* glyphData, intptr_t glyphAdvance, BLGlyphPlacement* out, size_t count) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = getImpl(self);
  BLFontFacePrivateImpl* faceI = bl::FontFaceInternal::getImpl(&selfI->face);

  return faceI->funcs.getGlyphAdvances(faceI, glyphData, glyphAdvance, out, count);
}

// bl::Font - Glyph Outlines
// =========================

static BLResult BL_CDECL blFontDummyPathSink(BLPathCore* path, const void* info, void* userData) noexcept {
  blUnused(path, info, userData);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFontGetGlyphOutlines(const BLFontCore* self, BLGlyphId glyphId, const BLMatrix2D* userTransform, BLPathCore* out, BLPathSinkFunc sink, void* userData) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = getImpl(self);
  BLFontFacePrivateImpl* faceI = bl::FontFaceInternal::getImpl(&selfI->face);

  BLMatrix2D finalTransform;
  const BLFontMatrix& fMat = selfI->matrix;

  if (userTransform)
    blFontMatrixMultiply(&finalTransform, &fMat, userTransform);
  else
    finalTransform.reset(fMat.m00, fMat.m01, fMat.m10, fMat.m11, 0.0, 0.0);

  bl::ScopedBufferTmp<BL_FONT_GET_GLYPH_OUTLINE_BUFFER_SIZE> tmpBuffer;
  BLGlyphOutlineSinkInfo sinkInfo;
  BL_PROPAGATE(faceI->funcs.getGlyphOutlines(faceI, glyphId, &finalTransform, static_cast<BLPath*>(out), &sinkInfo.contourCount, &tmpBuffer));

  if (!sink)
    return BL_SUCCESS;

  sinkInfo.glyphIndex = 0;
  return sink(out, &sinkInfo, userData);
}

BL_API_IMPL BLResult blFontGetGlyphRunOutlines(const BLFontCore* self, const BLGlyphRun* glyphRun, const BLMatrix2D* userTransform, BLPathCore* out, BLPathSinkFunc sink, void* userData) noexcept {
  using namespace bl::FontInternal;
  BL_ASSERT(self->_d.isFont());

  BLFontPrivateImpl* selfI = getImpl(self);
  BLFontFacePrivateImpl* faceI = bl::FontFaceInternal::getImpl(&selfI->face);

  if (!glyphRun->size)
    return BL_SUCCESS;

  BLMatrix2D finalTransform;
  const BLFontMatrix& fMat = selfI->matrix;

  if (userTransform) {
    blFontMatrixMultiply(&finalTransform, &fMat, userTransform);
  }
  else {
    userTransform = &bl::TransformInternal::identityTransform;
    finalTransform.reset(fMat.m00, fMat.m01, fMat.m10, fMat.m11, 0.0, 0.0);
  }

  if (!sink)
    sink = blFontDummyPathSink;

  bl::ScopedBufferTmp<BL_FONT_GET_GLYPH_OUTLINE_BUFFER_SIZE> tmpBuffer;
  BLGlyphOutlineSinkInfo sinkInfo;

  uint32_t placementType = glyphRun->placementType;
  BLGlyphRunIterator it(*glyphRun);
  auto getGlyphOutlinesFunc = faceI->funcs.getGlyphOutlines;

  if (it.hasPlacement() && placementType != BL_GLYPH_PLACEMENT_TYPE_NONE) {
    BLMatrix2D offsetTransform(1.0, 0.0, 0.0, 1.0, finalTransform.m20, finalTransform.m21);

    switch (placementType) {
      case BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET:
      case BL_GLYPH_PLACEMENT_TYPE_DESIGN_UNITS:
        offsetTransform.m00 = finalTransform.m00;
        offsetTransform.m01 = finalTransform.m01;
        offsetTransform.m10 = finalTransform.m10;
        offsetTransform.m11 = finalTransform.m11;
        break;

      case BL_GLYPH_PLACEMENT_TYPE_USER_UNITS:
        offsetTransform.m00 = userTransform->m00;
        offsetTransform.m01 = userTransform->m01;
        offsetTransform.m10 = userTransform->m10;
        offsetTransform.m11 = userTransform->m11;
        break;
    }

    if (placementType == BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET) {
      double ox = finalTransform.m20;
      double oy = finalTransform.m21;
      double px;
      double py;

      while (!it.atEnd()) {
        const BLGlyphPlacement& pos = it.placement<BLGlyphPlacement>();

        px = pos.placement.x;
        py = pos.placement.y;
        finalTransform.m20 = px * offsetTransform.m00 + py * offsetTransform.m10 + ox;
        finalTransform.m21 = px * offsetTransform.m01 + py * offsetTransform.m11 + oy;

        sinkInfo.glyphIndex = it.index;
        BL_PROPAGATE(getGlyphOutlinesFunc(faceI, it.glyphId(), &finalTransform, static_cast<BLPath*>(out), &sinkInfo.contourCount, &tmpBuffer));
        BL_PROPAGATE(sink(out, &sinkInfo, userData));
        it.advance();

        px = pos.advance.x;
        py = pos.advance.y;
        ox += px * offsetTransform.m00 + py * offsetTransform.m10;
        oy += px * offsetTransform.m01 + py * offsetTransform.m11;
      }
    }
    else {
      while (!it.atEnd()) {
        const BLPoint& placement = it.placement<BLPoint>();
        finalTransform.m20 = placement.x * offsetTransform.m00 + placement.y * offsetTransform.m10 + offsetTransform.m20;
        finalTransform.m21 = placement.x * offsetTransform.m01 + placement.y * offsetTransform.m11 + offsetTransform.m21;

        sinkInfo.glyphIndex = it.index;
        BL_PROPAGATE(getGlyphOutlinesFunc(faceI, it.glyphId(), &finalTransform, static_cast<BLPath*>(out), &sinkInfo.contourCount, &tmpBuffer));
        BL_PROPAGATE(sink(out, &sinkInfo, userData));
        it.advance();
      }
    }
  }
  else {
    while (!it.atEnd()) {
      sinkInfo.glyphIndex = it.index;
      BL_PROPAGATE(getGlyphOutlinesFunc(faceI, it.glyphId(), &finalTransform, static_cast<BLPath*>(out), &sinkInfo.contourCount, &tmpBuffer));
      BL_PROPAGATE(sink(out, &sinkInfo, userData));
      it.advance();
    }
  }

  return BL_SUCCESS;
}

// bl::Font - Runtime Registration
// ===============================

void blFontRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  // Initialize BLFont built-ins.
  blFontImplCtor(&bl::FontInternal::defaultFont.impl);
  blObjectDefaults[BL_OBJECT_TYPE_FONT]._d.initDynamic(BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT), &bl::FontInternal::defaultFont.impl);
}
