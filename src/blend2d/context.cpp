// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "context_p.h"
#include "gradient_p.h"
#include "image_p.h"
#include "pattern_p.h"
#include "runtime_p.h"
#include "raster/rastercontext_p.h"

namespace bl {
namespace ContextInternal {

// bl::Context - Globals
// =====================

static Wrap<BLContextState> nullState;
static BLObjectEternalVirtualImpl<BLContextImpl, BLContextVirt> defaultContext;
static const constexpr BLContextCreateInfo noCreateInfo {};

// bl::Context - Null Context
// ==========================

namespace NullContext {

// NullContext implementation does nothing. These functions consistently return `BL_ERROR_INVALID_STATE` to inform the
// caller that the context is not usable. We don't want to mark every unused parameter by `blUnused()` in this case so
// the warning is temporarily turned off by BL_DIAGNOSTIC_PUSH/POP.
BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

static BLResult BL_CDECL destroyImpl(BLObjectImpl* impl) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL flushImpl(BLContextImpl* impl, BLContextFlushFlags flags) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL noArgsImpl(BLContextImpl* impl) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setDoubleImpl(BLContextImpl* impl, double) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setCompOpImpl(BLContextImpl* impl, BLCompOp) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setFillRuleImpl(BLContextImpl* impl, BLFillRule) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL saveImpl(BLContextImpl* impl, BLContextCookie*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL restoreImpl(BLContextImpl* impl, const BLContextCookie*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL getStyleImpl(const BLContextImpl* impl, BLContextStyleSlot, bool, BLVarCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setStyleImpl(BLContextImpl* impl, BLContextStyleSlot, const BLObjectCore*, BLContextStyleTransformMode) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL disableStyleImpl(BLContextImpl* impl, BLContextStyleSlot) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setStyleRgbaImpl(BLContextImpl* impl, BLContextStyleSlot, const BLRgba*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setStyleRgba32Impl(BLContextImpl* impl, BLContextStyleSlot, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setStyleRgba64Impl(BLContextImpl* impl, BLContextStyleSlot, uint64_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setStyleAlphaImpl(BLContextImpl* impl, BLContextStyleSlot, double) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL swapStylesImpl(BLContextImpl* impl, BLContextStyleSwapMode mode) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL setHintImpl(BLContextImpl* impl, BLContextHint, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setHintsImpl(BLContextImpl* impl, const BLContextHints*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setFlattenModeImpl(BLContextImpl* impl, BLFlattenMode) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setApproximationOptionsImpl(BLContextImpl* impl, const BLApproximationOptions*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setStrokeTransformOrderImpl(BLContextImpl* impl, BLStrokeTransformOrder) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setStrokeDashArrayImpl(BLContextImpl* impl, const BLArrayCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setStrokeCapImpl(BLContextImpl* impl, BLStrokeCapPosition, BLStrokeCap) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setStrokeCapsImpl(BLContextImpl* impl, BLStrokeCap) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setStrokeJoinImpl(BLContextImpl* impl, BLStrokeJoin) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL setStrokeOptionsImpl(BLContextImpl* impl, const BLStrokeOptionsCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL applyTransformOpImpl(BLContextImpl* impl, BLTransformOp, const void*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL fillAllImpl(BLContextImpl* impl) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL fillAllRgba32Impl(BLContextImpl* impl, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL fillAllExtImpl(BLContextImpl* impl, const BLObjectCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL doRectIImpl(BLContextImpl* impl, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doRectIRgba32Impl(BLContextImpl* impl, const BLRectI*, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doRectIExtImpl(BLContextImpl* impl, const BLRectI*, const BLObjectCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL doRectDImpl(BLContextImpl* impl, const BLRect*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doRectDRgba32Impl(BLContextImpl* impl, const BLRect*, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doRectDExtImpl(BLContextImpl* impl, const BLRect*, const BLObjectCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL doPathDImpl(BLContextImpl* impl, const BLPoint*, const BLPathCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doPathDRgba32Impl(BLContextImpl* impl, const BLPoint*, const BLPathCore*, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doPathDExtImpl(BLContextImpl* impl, const BLPoint*, const BLPathCore*, const BLObjectCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL doGeometryImpl(BLContextImpl* impl, BLGeometryType, const void*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doGeometryRgba32Impl(BLContextImpl* impl, BLGeometryType, const void*, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doGeometryExtImpl(BLContextImpl* impl, BLGeometryType, const void*, const BLObjectCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL doTextOpIImpl(BLContextImpl* impl, const BLPointI*, const BLFontCore*, BLContextRenderTextOp, const void*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doTextOpIRgba32Impl(BLContextImpl* impl, const BLPointI*, const BLFontCore*, BLContextRenderTextOp, const void*, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doTextOpIExtImpl(BLContextImpl* impl, const BLPointI*, const BLFontCore*, BLContextRenderTextOp, const void*, const BLObjectCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL doTextOpDImpl(BLContextImpl* impl, const BLPoint*, const BLFontCore*, BLContextRenderTextOp, const void*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doTextOpDRgba32Impl(BLContextImpl* impl, const BLPoint*, const BLFontCore*, BLContextRenderTextOp, const void*, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doTextOpDExtImpl(BLContextImpl* impl, const BLPoint*, const BLFontCore*, BLContextRenderTextOp, const void*, const BLObjectCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL doMaskIImpl(BLContextImpl* impl, const BLPointI*, const BLImageCore*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doMaskDRgba32Impl(BLContextImpl* impl, const BLPointI*, const BLImageCore*, const BLRectI*, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doMaskDExtImpl(BLContextImpl* impl, const BLPointI*, const BLImageCore*, const BLRectI*, const BLObjectCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL doMaskDImpl(BLContextImpl* impl, const BLPoint*, const BLImageCore*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doMaskDRgba32Impl(BLContextImpl* impl, const BLPoint*, const BLImageCore*, const BLRectI*, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL doMaskDExtImpl(BLContextImpl* impl, const BLPoint*, const BLImageCore*, const BLRectI*, const BLObjectCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL blitImageIImpl(BLContextImpl* impl, const BLPointI*, const BLImageCore*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blitImageDImpl(BLContextImpl* impl, const BLPoint*, const BLImageCore*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blitScaledImageIImpl(BLContextImpl* impl, const BLRectI*, const BLImageCore*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blitScaledImageDImpl(BLContextImpl* impl, const BLRect*, const BLImageCore*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

BL_DIAGNOSTIC_POP

} // {NullContext}

static void initNullContextVirt(BLContextVirt* virt) noexcept {
  virt->base.destroy             = NullContext::destroyImpl;
  virt->base.getProperty         = blObjectImplGetProperty;
  virt->base.setProperty         = blObjectImplSetProperty;
  virt->flush                    = NullContext::flushImpl;

  virt->save                     = NullContext::saveImpl;
  virt->restore                  = NullContext::restoreImpl;

  virt->userToMeta               = NullContext::noArgsImpl;
  virt->applyTransformOp         = NullContext::applyTransformOpImpl;

  virt->setHint                  = NullContext::setHintImpl;
  virt->setHints                 = NullContext::setHintsImpl;

  virt->setFlattenMode           = NullContext::setFlattenModeImpl;
  virt->setFlattenTolerance      = NullContext::setDoubleImpl;
  virt->setApproximationOptions  = NullContext::setApproximationOptionsImpl;

  virt->getStyle                 = NullContext::getStyleImpl;
  virt->setStyle                 = NullContext::setStyleImpl;
  virt->disableStyle             = NullContext::disableStyleImpl;
  virt->setStyleRgba             = NullContext::setStyleRgbaImpl;
  virt->setStyleRgba32           = NullContext::setStyleRgba32Impl;
  virt->setStyleRgba64           = NullContext::setStyleRgba64Impl;
  virt->setStyleAlpha            = NullContext::setStyleAlphaImpl;
  virt->swapStyles               = NullContext::swapStylesImpl;

  virt->setGlobalAlpha           = NullContext::setDoubleImpl;
  virt->setCompOp                = NullContext::setCompOpImpl;

  virt->setFillRule              = NullContext::setFillRuleImpl;

  virt->setStrokeWidth           = NullContext::setDoubleImpl;
  virt->setStrokeMiterLimit      = NullContext::setDoubleImpl;
  virt->setStrokeCap             = NullContext::setStrokeCapImpl;
  virt->setStrokeCaps            = NullContext::setStrokeCapsImpl;
  virt->setStrokeJoin            = NullContext::setStrokeJoinImpl;
  virt->setStrokeTransformOrder  = NullContext::setStrokeTransformOrderImpl;
  virt->setStrokeDashOffset      = NullContext::setDoubleImpl;
  virt->setStrokeDashArray       = NullContext::setStrokeDashArrayImpl;
  virt->setStrokeOptions         = NullContext::setStrokeOptionsImpl;

  virt->clipToRectI              = NullContext::doRectIImpl;
  virt->clipToRectD              = NullContext::doRectDImpl;
  virt->restoreClipping          = NullContext::noArgsImpl;

  virt->clearAll                 = NullContext::noArgsImpl;
  virt->clearRectI               = NullContext::doRectIImpl;
  virt->clearRectD               = NullContext::doRectDImpl;

  virt->fillAll                  = NullContext::fillAllImpl;
  virt->fillAllRgba32            = NullContext::fillAllRgba32Impl;
  virt->fillAllExt               = NullContext::fillAllExtImpl;

  virt->fillRectI                = NullContext::doRectIImpl;
  virt->fillRectIRgba32          = NullContext::doRectIRgba32Impl;
  virt->fillRectIExt             = NullContext::doRectIExtImpl;

  virt->fillRectD                = NullContext::doRectDImpl;
  virt->fillRectDRgba32          = NullContext::doRectDRgba32Impl;
  virt->fillRectDExt             = NullContext::doRectDExtImpl;

  virt->fillPathD                = NullContext::doPathDImpl;
  virt->fillPathDRgba32          = NullContext::doPathDRgba32Impl;
  virt->fillPathDExt             = NullContext::doPathDExtImpl;

  virt->fillGeometry             = NullContext::doGeometryImpl;
  virt->fillGeometryRgba32       = NullContext::doGeometryRgba32Impl;
  virt->fillGeometryExt          = NullContext::doGeometryExtImpl;

  virt->fillTextOpI              = NullContext::doTextOpIImpl;
  virt->fillTextOpIRgba32        = NullContext::doTextOpIRgba32Impl;
  virt->fillTextOpIExt           = NullContext::doTextOpIExtImpl;

  virt->fillTextOpD              = NullContext::doTextOpDImpl;
  virt->fillTextOpDRgba32        = NullContext::doTextOpDRgba32Impl;
  virt->fillTextOpDExt           = NullContext::doTextOpDExtImpl;

  virt->fillMaskI                = NullContext::doMaskIImpl;
  virt->fillMaskIRgba32          = NullContext::doMaskDRgba32Impl;
  virt->fillMaskIExt             = NullContext::doMaskDExtImpl;

  virt->fillMaskD                = NullContext::doMaskDImpl;
  virt->fillMaskDRgba32          = NullContext::doMaskDRgba32Impl;
  virt->fillMaskDExt             = NullContext::doMaskDExtImpl;

  virt->strokePathD              = NullContext::doPathDImpl;
  virt->strokePathDRgba32        = NullContext::doPathDRgba32Impl;
  virt->strokePathDExt           = NullContext::doPathDExtImpl;

  virt->strokeGeometry           = NullContext::doGeometryImpl;
  virt->strokeGeometryRgba32     = NullContext::doGeometryRgba32Impl;
  virt->strokeGeometryExt        = NullContext::doGeometryExtImpl;

  virt->strokeTextOpI            = NullContext::doTextOpIImpl;
  virt->strokeTextOpIRgba32      = NullContext::doTextOpIRgba32Impl;
  virt->strokeTextOpIExt         = NullContext::doTextOpIExtImpl;

  virt->strokeTextOpD            = NullContext::doTextOpDImpl;
  virt->strokeTextOpDRgba32      = NullContext::doTextOpDRgba32Impl;
  virt->strokeTextOpDExt         = NullContext::doTextOpDExtImpl;

  virt->blitImageI               = NullContext::blitImageIImpl;
  virt->blitImageD               = NullContext::blitImageDImpl;

  virt->blitScaledImageI         = NullContext::blitScaledImageIImpl;
  virt->blitScaledImageD         = NullContext::blitScaledImageDImpl;
}

} // {ContextInternal}
} // {bl}

// bl::Context - API - Init & Destroy
// ==================================

BL_API_IMPL BLResult blContextInit(BLContextCore* self) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_CONTEXT]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blContextInitMove(BLContextCore* self, BLContextCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isContext());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_CONTEXT]._d;

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blContextInitWeak(BLContextCore* self, const BLContextCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isContext());

  self->_d = other->_d;
  return bl::ObjectInternal::retainInstance(self);
}

BL_API_IMPL BLResult blContextInitAs(BLContextCore* self, BLImageCore* image, const BLContextCreateInfo* cci) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_CONTEXT]._d;
  return blContextBegin(self, image, cci);
}

BL_API_IMPL BLResult blContextDestroy(BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());

  return bl::ObjectInternal::releaseVirtualInstance(self);
}

// bl::Context - API - Reset
// =========================

BL_API_IMPL BLResult blContextReset(BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());

  return bl::ObjectInternal::replaceVirtualInstance(self, static_cast<BLContextCore*>(&blObjectDefaults[BL_OBJECT_TYPE_CONTEXT]));
}

// bl::Context - API - Assign
// ==========================

BL_API_IMPL BLResult blContextAssignMove(BLContextCore* self, BLContextCore* other) noexcept {
  BL_ASSERT(self->_d.isContext());
  BL_ASSERT(other->_d.isContext());

  BLContextCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_CONTEXT]._d;
  return bl::ObjectInternal::replaceVirtualInstance(self, &tmp);
}

BL_API_IMPL BLResult blContextAssignWeak(BLContextCore* self, const BLContextCore* other) noexcept {
  BL_ASSERT(self->_d.isContext());
  BL_ASSERT(other->_d.isContext());

  return bl::ObjectInternal::assignVirtualInstance(self, other);
}

// bl::Context - API - Accessors
// =============================

BL_API_IMPL BLContextType blContextGetType(const BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return BLContextType(impl->contextType);
}

BL_API_IMPL BLResult blContextGetTargetSize(const BLContextCore* self, BLSize* targetSizeOut) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  *targetSizeOut = impl->state->targetSize;
  return BL_SUCCESS;
}

BL_API_IMPL BLImageCore* blContextGetTargetImage(const BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->state->targetImage;
}

// bl::Context - API - Begin & End
// ===============================

BL_API_IMPL BLResult blContextBegin(BLContextCore* self, BLImageCore* image, const BLContextCreateInfo* cci) noexcept {
  // Reject empty images.
  if (image->dcast().empty())
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (!cci)
    cci = &bl::ContextInternal::noCreateInfo;

  BLContextCore newO;
  BL_PROPAGATE(blRasterContextInitImpl(&newO, image, cci));

  return bl::ObjectInternal::replaceVirtualInstance(self, &newO);
}

BL_API_IMPL BLResult blContextEnd(BLContextCore* self) noexcept {
  // Currently mapped to `BLContext::reset()`.
  return blContextReset(self);
}

// bl::Context - API - Flush
// =========================

BL_API_IMPL BLResult blContextFlush(BLContextCore* self, BLContextFlushFlags flags) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->flush(impl, flags);
}

// bl::Context - API - Save & Restore
// ==================================

BL_API_IMPL BLResult blContextSave(BLContextCore* self, BLContextCookie* cookie) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->save(impl, cookie);
}

BL_API_IMPL BLResult blContextRestore(BLContextCore* self, const BLContextCookie* cookie) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->restore(impl, cookie);
}

// bl::Context - API - Transformations
// ===================================

BL_API_IMPL BLResult blContextGetMetaTransform(const BLContextCore* self, BLMatrix2D* transformOut) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  *transformOut = impl->state->metaTransform;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blContextGetUserTransform(const BLContextCore* self, BLMatrix2D* transformOut) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  *transformOut = impl->state->userTransform;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blContextGetFinalTransform(const BLContextCore* self, BLMatrix2D* transformOut) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  *transformOut = impl->state->finalTransform;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blContextUserToMeta(BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->userToMeta(impl);
}

BL_API_IMPL BLResult blContextApplyTransformOp(BLContextCore* self, BLTransformOp opType, const void* opData) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->applyTransformOp(impl, opType, opData);
}

// bl::Context - API - Rendering Hints
// ===================================

BL_API_IMPL uint32_t blContextGetHint(const BLContextCore* self, BLContextHint hintType) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  if (BL_UNLIKELY(uint32_t(hintType) > uint32_t(BL_CONTEXT_HINT_MAX_VALUE)))
    return 0;

  return impl->state->hints.hints[hintType];
}

BL_API_IMPL BLResult blContextSetHint(BLContextCore* self, BLContextHint hintType, uint32_t value) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setHint(impl, hintType, value);
}

BL_API_IMPL BLResult blContextGetHints(const BLContextCore* self, BLContextHints* hintsOut) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  *hintsOut = impl->state->hints;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blContextSetHints(BLContextCore* self, const BLContextHints* hints) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setHints(impl, hints);
}

// bl::Context - API - Approximation Options
// =========================================

BL_API_IMPL BLResult blContextSetFlattenMode(BLContextCore* self, BLFlattenMode mode) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setFlattenMode(impl, mode);
}

BL_API_IMPL BLResult blContextSetFlattenTolerance(BLContextCore* self, double tolerance) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setFlattenTolerance(impl, tolerance);
}

BL_API_IMPL BLResult blContextSetApproximationOptions(BLContextCore* self, const BLApproximationOptions* options) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setApproximationOptions(impl, options);
}

// bl::Context - API - Fill Style & Alpha
// ======================================

BL_API_IMPL BLResult blContextGetFillStyle(const BLContextCore* self, BLVarCore* styleOut) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->getStyle(impl, BL_CONTEXT_STYLE_SLOT_FILL, false, styleOut);
}

BL_API_IMPL BLResult blContextGetTransformedFillStyle(const BLContextCore* self, BLVarCore* styleOut) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->getStyle(impl, BL_CONTEXT_STYLE_SLOT_FILL, true, styleOut);
}

BL_API_IMPL BLResult blContextSetFillStyle(BLContextCore* self, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyle(impl, BL_CONTEXT_STYLE_SLOT_FILL, static_cast<const BLObjectCore*>(style), BL_CONTEXT_STYLE_TRANSFORM_MODE_USER);
}

BL_API_IMPL BLResult blContextSetFillStyleWithMode(BLContextCore* self, const BLUnknown* style, BLContextStyleTransformMode transformMode) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyle(impl, BL_CONTEXT_STYLE_SLOT_FILL, static_cast<const BLObjectCore*>(style), transformMode);
}

BL_API_IMPL BLResult blContextSetFillStyleRgba(BLContextCore* self, const BLRgba* rgba) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleRgba(impl, BL_CONTEXT_STYLE_SLOT_FILL, rgba);
}

BL_API_IMPL BLResult blContextSetFillStyleRgba32(BLContextCore* self, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleRgba32(impl, BL_CONTEXT_STYLE_SLOT_FILL, rgba32);
}

BL_API_IMPL BLResult blContextSetFillStyleRgba64(BLContextCore* self, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleRgba64(impl, BL_CONTEXT_STYLE_SLOT_FILL, rgba64);
}

BL_API_IMPL BLResult blContextDisableFillStyle(BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->disableStyle(impl, BL_CONTEXT_STYLE_SLOT_FILL);
}

BL_API_IMPL double blContextGetFillAlpha(const BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->state->styleAlpha[BL_CONTEXT_STYLE_SLOT_FILL];
}

BL_API_IMPL BLResult blContextSetFillAlpha(BLContextCore* self, double alpha) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleAlpha(impl, BL_CONTEXT_STYLE_SLOT_FILL, alpha);
}

// bl::Context - API - Stroke Style & Alpha
// ========================================

BL_API_IMPL BLResult blContextGetStrokeStyle(const BLContextCore* self, BLVarCore* styleOut) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->getStyle(impl, BL_CONTEXT_STYLE_SLOT_STROKE, false, styleOut);
}

BL_API_IMPL BLResult blContextGetTransformedStrokeStyle(const BLContextCore* self, BLVarCore* styleOut) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->getStyle(impl, BL_CONTEXT_STYLE_SLOT_STROKE, true, styleOut);
}

BL_API_IMPL BLResult blContextSetStrokeStyle(BLContextCore* self, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyle(impl, BL_CONTEXT_STYLE_SLOT_STROKE, static_cast<const BLObjectCore*>(style), BL_CONTEXT_STYLE_TRANSFORM_MODE_USER);
}

BL_API_IMPL BLResult blContextSetStrokeStyleWithMode(BLContextCore* self, const BLUnknown* style, BLContextStyleTransformMode transformMode) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyle(impl, BL_CONTEXT_STYLE_SLOT_STROKE, static_cast<const BLObjectCore*>(style), transformMode);
}

BL_API_IMPL BLResult blContextSetStrokeStyleRgba(BLContextCore* self, const BLRgba* rgba) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleRgba(impl, BL_CONTEXT_STYLE_SLOT_STROKE, rgba);
}

BL_API_IMPL BLResult blContextSetStrokeStyleRgba32(BLContextCore* self, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleRgba32(impl, BL_CONTEXT_STYLE_SLOT_STROKE, rgba32);
}

BL_API_IMPL BLResult blContextSetStrokeStyleRgba64(BLContextCore* self, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleRgba64(impl, BL_CONTEXT_STYLE_SLOT_STROKE, rgba64);
}

BL_API_IMPL BLResult blContextDisableStrokeStyle(BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->disableStyle(impl, BL_CONTEXT_STYLE_SLOT_STROKE);
}

BL_API_IMPL double blContextGetStrokeAlpha(const BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->state->styleAlpha[BL_CONTEXT_STYLE_SLOT_STROKE];
}

BL_API_IMPL BLResult blContextSetStrokeAlpha(BLContextCore* self, double alpha) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleAlpha(impl, BL_CONTEXT_STYLE_SLOT_STROKE, alpha);
}

BL_API_IMPL BLResult blContextSwapStyles(BLContextCore* self, BLContextStyleSwapMode mode) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->swapStyles(impl, mode);
}

// bl::Context - API - Composition Options
// =======================================

BL_API_IMPL double blContextGetGlobalAlpha(const BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->state->globalAlpha;
}

BL_API_IMPL BLResult blContextSetGlobalAlpha(BLContextCore* self, double alpha) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setGlobalAlpha(impl, alpha);
}

BL_API_IMPL BLCompOp blContextGetCompOp(const BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return (BLCompOp)impl->state->compOp;
}

BL_API_IMPL BLResult blContextSetCompOp(BLContextCore* self, BLCompOp compOp) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setCompOp(impl, compOp);
}

// bl::Context - API - Fill Options
// ================================

BL_API_IMPL BLFillRule blContextGetFillRule(const BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return (BLFillRule)impl->state->fillRule;
}

BL_API_IMPL BLResult blContextSetFillRule(BLContextCore* self, BLFillRule fillRule) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setFillRule(impl, fillRule);
}

// bl::Context - API - Stroke Options
// ==================================

BL_API_IMPL double blContextGetStrokeWidth(const BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->state->strokeOptions.width;
}

BL_API_IMPL BLResult blContextSetStrokeWidth(BLContextCore* self, double width) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStrokeWidth(impl, width);
}

BL_API_IMPL double blContextGetStrokeMiterLimit(const BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->state->strokeOptions.miterLimit;
}

BL_API_IMPL BLResult blContextSetStrokeMiterLimit(BLContextCore* self, double miterLimit) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStrokeMiterLimit(impl, miterLimit);
}

BL_API_IMPL BLStrokeCap blContextGetStrokeCap(const BLContextCore* self, BLStrokeCapPosition position) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  if (BL_UNLIKELY(uint32_t(position) > uint32_t(BL_STROKE_CAP_POSITION_MAX_VALUE)))
    return (BLStrokeCap)0;

  return (BLStrokeCap)impl->state->strokeOptions.caps[position];
}

BL_API_IMPL BLResult blContextSetStrokeCap(BLContextCore* self, BLStrokeCapPosition position, BLStrokeCap strokeCap) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStrokeCap(impl, position, strokeCap);
}

BL_API_IMPL BLResult blContextSetStrokeCaps(BLContextCore* self, BLStrokeCap strokeCap) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStrokeCaps(impl, strokeCap);
}

BL_API_IMPL BLStrokeJoin blContextGetStrokeJoin(const BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return (BLStrokeJoin)impl->state->strokeOptions.join;
}

BL_API_IMPL BLResult blContextSetStrokeJoin(BLContextCore* self, BLStrokeJoin strokeJoin) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStrokeJoin(impl, strokeJoin);
}

BL_API_IMPL BLStrokeTransformOrder blContextGetStrokeTransformOrder(const BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return (BLStrokeTransformOrder)impl->state->strokeOptions.transformOrder;
}

BL_API_IMPL BLResult blContextSetStrokeTransformOrder(BLContextCore* self, BLStrokeTransformOrder transformOrder) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStrokeTransformOrder(impl, transformOrder);
}

BL_API_IMPL double blContextGetStrokeDashOffset(const BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->state->strokeOptions.dashOffset;
}

BL_API_IMPL BLResult blContextSetStrokeDashOffset(BLContextCore* self, double dashOffset) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStrokeDashOffset(impl, dashOffset);
}

BL_API_IMPL BLResult blContextGetStrokeDashArray(const BLContextCore* self, BLArrayCore* dashArrayOut) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return blArrayAssignWeak(dashArrayOut, &impl->state->strokeOptions.dashArray);
}

BL_API_IMPL BLResult blContextSetStrokeDashArray(BLContextCore* self, const BLArrayCore* dashArray) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStrokeDashArray(impl, dashArray);
}

BL_API_IMPL BLResult blContextGetStrokeOptions(const BLContextCore* self, BLStrokeOptionsCore* options) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return blStrokeOptionsAssignWeak(options, &impl->state->strokeOptions);
}

BL_API_IMPL BLResult blContextSetStrokeOptions(BLContextCore* self, const BLStrokeOptionsCore* options) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStrokeOptions(impl, options);
}

// bl::Context - API - Clip Operations
// ===================================

BL_API_IMPL BLResult blContextClipToRectI(BLContextCore* self, const BLRectI* rect) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->clipToRectI(impl, rect);
}

BL_API_IMPL BLResult blContextClipToRectD(BLContextCore* self, const BLRect* rect) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->clipToRectD(impl, rect);
}

BL_API_IMPL BLResult blContextRestoreClipping(BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->restoreClipping(impl);
}

// bl::Context - API - Clear Geometry Operations
// =============================================

BL_API_IMPL BLResult blContextClearAll(BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->clearAll(impl);
}

BL_API_IMPL BLResult blContextClearRectI(BLContextCore* self, const BLRectI* rect) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->clearRectI(impl, rect);
}

BL_API_IMPL BLResult blContextClearRectD(BLContextCore* self, const BLRect* rect) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->clearRectD(impl, rect);
}

// bl::Context - API - Fill All Operations
// =======================================

BL_API_IMPL BLResult blContextFillAll(BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillAll(impl);
}

BL_API_IMPL BLResult blContextFillAllRgba32(BLContextCore* self, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillAllRgba32(impl, rgba32);
}

BL_API_IMPL BLResult blContextFillAllRgba64(BLContextCore* self, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  return impl->virt->fillAllExt(impl, &style);
}

BL_API_IMPL BLResult blContextFillAllExt(BLContextCore* self, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillAllExt(impl, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Fill Rect Operations
// ========================================

BL_API_IMPL BLResult blContextFillRectI(BLContextCore* self, const BLRectI* rect) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillRectI(impl, rect);
}

BL_API_IMPL BLResult blContextFillRectIRgba32(BLContextCore* self, const BLRectI* rect, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillRectIRgba32(impl, rect, rgba32);
}

BL_API_IMPL BLResult blContextFillRectIRgba64(BLContextCore* self, const BLRectI* rect, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  return impl->virt->fillRectIExt(impl, rect, &style);
}

BL_API_IMPL BLResult blContextFillRectIExt(BLContextCore* self, const BLRectI* rect, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillRectIExt(impl, rect, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult blContextFillRectD(BLContextCore* self, const BLRect* rect) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillRectD(impl, rect);
}

BL_API_IMPL BLResult blContextFillRectDRgba32(BLContextCore* self, const BLRect* rect, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillRectDRgba32(impl, rect, rgba32);
}

BL_API_IMPL BLResult blContextFillRectDRgba64(BLContextCore* self, const BLRect* rect, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  return impl->virt->fillRectDExt(impl, rect, &style);
}

BL_API_IMPL BLResult blContextFillRectDExt(BLContextCore* self, const BLRect* rect, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillRectDExt(impl, rect, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Fill Path Operations
// ========================================

BL_API_IMPL BLResult blContextFillPathD(BLContextCore* self, const BLPoint* origin, const BLPathCore* path) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillPathD(impl, origin, path);
}

BL_API_IMPL BLResult blContextFillPathDRgba32(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillPathDRgba32(impl, origin, path, rgba32);
}

BL_API_IMPL BLResult blContextFillPathDRgba64(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  return impl->virt->fillPathDExt(impl, origin, path, &style);
}

BL_API_IMPL BLResult blContextFillPathDExt(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillPathDExt(impl, origin, path, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Fill Geometry Operations
// ============================================

BL_API_IMPL BLResult blContextFillGeometry(BLContextCore* self, BLGeometryType type, const void* data) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillGeometry(impl, type, data);
}

BL_API_IMPL BLResult blContextFillGeometryRgba32(BLContextCore* self, BLGeometryType type, const void* data, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillGeometryRgba32(impl, type, data, rgba32);
}

BL_API_IMPL BLResult blContextFillGeometryRgba64(BLContextCore* self, BLGeometryType type, const void* data, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  return impl->virt->fillGeometryExt(impl, type, data, &style);
}

BL_API_IMPL BLResult blContextFillGeometryExt(BLContextCore* self, BLGeometryType type, const void* data, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillGeometryExt(impl, type, data, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Fill UTF-8 Text Operations
// ==============================================

BL_API_IMPL BLResult blContextFillUtf8TextI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLStringView view{text, size};
  return impl->virt->fillTextOpI(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view);
}

BL_API_IMPL BLResult blContextFillUtf8TextIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLStringView view{text, size};
  return impl->virt->fillTextOpIRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, rgba32);
}

BL_API_IMPL BLResult blContextFillUtf8TextIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  BLStringView view{text, size};
  return impl->virt->fillTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, &style);
}

BL_API_IMPL BLResult blContextFillUtf8TextIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLStringView view{text, size};
  return impl->virt->fillTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult blContextFillUtf8TextD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLStringView view{text, size};
  return impl->virt->fillTextOpD(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view);
}

BL_API_IMPL BLResult blContextFillUtf8TextDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLStringView view{text, size};
  return impl->virt->fillTextOpDRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, rgba32);
}

BL_API_IMPL BLResult blContextFillUtf8TextDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  BLStringView view{text, size};
  return impl->virt->fillTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, &style);
}

BL_API_IMPL BLResult blContextFillUtf8TextDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLStringView view{text, size};
  return impl->virt->fillTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Fill UTF-16 Text Operations
// ===============================================

BL_API_IMPL BLResult blContextFillUtf16TextI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint16_t> view{text, size};
  return impl->virt->fillTextOpI(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view);
}

BL_API_IMPL BLResult blContextFillUtf16TextIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint16_t> view{text, size};
  return impl->virt->fillTextOpIRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, rgba32);
}

BL_API_IMPL BLResult blContextFillUtf16TextIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  BLArrayView<uint16_t> view{text, size};
  return impl->virt->fillTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, &style);
}

BL_API_IMPL BLResult blContextFillUtf16TextIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint16_t> view{text, size};
  return impl->virt->fillTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult blContextFillUtf16TextD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint16_t> view{text, size};
  return impl->virt->fillTextOpD(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view);
}

BL_API_IMPL BLResult blContextFillUtf16TextDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint16_t> view{text, size};
  return impl->virt->fillTextOpDRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, rgba32);
}

BL_API_IMPL BLResult blContextFillUtf16TextDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  BLArrayView<uint16_t> view{text, size};
  return impl->virt->fillTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, &style);
}

BL_API_IMPL BLResult blContextFillUtf16TextDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint16_t> view{text, size};
  return impl->virt->fillTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Fill UTF-32 Text Operations
// ===============================================

BL_API_IMPL BLResult blContextFillUtf32TextI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint32_t> view{text, size};
  return impl->virt->fillTextOpI(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view);
}

BL_API_IMPL BLResult blContextFillUtf32TextIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint32_t> view{text, size};
  return impl->virt->fillTextOpIRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, rgba32);
}

BL_API_IMPL BLResult blContextFillUtf32TextIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  BLArrayView<uint32_t> view{text, size};
  return impl->virt->fillTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, &style);
}

BL_API_IMPL BLResult blContextFillUtf32TextIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint32_t> view{text, size};
  return impl->virt->fillTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult blContextFillUtf32TextD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint32_t> view{text, size};
  return impl->virt->fillTextOpD(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view);
}

BL_API_IMPL BLResult blContextFillUtf32TextDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint32_t> view{text, size};
  return impl->virt->fillTextOpDRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, rgba32);
}

BL_API_IMPL BLResult blContextFillUtf32TextDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  BLArrayView<uint32_t> view{text, size};
  return impl->virt->fillTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, &style);
}

BL_API_IMPL BLResult blContextFillUtf32TextDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint32_t> view{text, size};
  return impl->virt->fillTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Fill Glyph Run Operations
// =============================================

BL_API_IMPL BLResult blContextFillGlyphRunI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillTextOpI(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun);
}

BL_API_IMPL BLResult blContextFillGlyphRunIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillTextOpIRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun, rgba32);
}

BL_API_IMPL BLResult blContextFillGlyphRunIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  return impl->virt->fillTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun, &style);
}

BL_API_IMPL BLResult blContextFillGlyphRunIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult blContextFillGlyphRunD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillTextOpD(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun);
}

BL_API_IMPL BLResult blContextFillGlyphRunDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillTextOpDRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun, rgba32);
}

BL_API_IMPL BLResult blContextFillGlyphRunDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  return impl->virt->fillTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun, &style);
}

BL_API_IMPL BLResult blContextFillGlyphRunDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Fill Mask Operations
// ========================================

BL_API_IMPL BLResult blContextFillMaskI(BLContextCore* self, const BLPointI* origin, const BLImageCore* mask, const BLRectI* maskArea) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillMaskI(impl, origin, mask, maskArea);
}

BL_API_IMPL BLResult blContextFillMaskIRgba32(BLContextCore* self, const BLPointI* origin, const BLImageCore* mask, const BLRectI* maskArea, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillMaskIRgba32(impl, origin, mask, maskArea, rgba32);
}

BL_API_IMPL BLResult blContextFillMaskIRgba64(BLContextCore* self, const BLPointI* origin, const BLImageCore* mask, const BLRectI* maskArea, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  return impl->virt->fillMaskIExt(impl, origin, mask, maskArea, &style);
}

BL_API_IMPL BLResult blContextFillMaskIExt(BLContextCore* self, const BLPointI* origin, const BLImageCore* mask, const BLRectI* maskArea, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillMaskIExt(impl, origin, mask, maskArea, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult blContextFillMaskD(BLContextCore* self, const BLPoint* origin, const BLImageCore* mask, const BLRectI* maskArea) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillMaskD(impl, origin, mask, maskArea);
}

BL_API_IMPL BLResult blContextFillMaskDRgba32(BLContextCore* self, const BLPoint* origin, const BLImageCore* mask, const BLRectI* maskArea, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillMaskDRgba32(impl, origin, mask, maskArea, rgba32);
}

BL_API_IMPL BLResult blContextFillMaskDRgba64(BLContextCore* self, const BLPoint* origin, const BLImageCore* mask, const BLRectI* maskArea, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  return impl->virt->fillMaskDExt(impl, origin, mask, maskArea, &style);
}

BL_API_IMPL BLResult blContextFillMaskDExt(BLContextCore* self, const BLPoint* origin, const BLImageCore* mask, const BLRectI* maskArea, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillMaskDExt(impl, origin, mask, maskArea, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Stroke Rect Operations
// ==========================================

BL_API_IMPL BLResult blContextStrokeRectI(BLContextCore* self, const BLRectI* rect) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeGeometry(impl, BL_GEOMETRY_TYPE_RECTI, rect);
}

BL_API_IMPL BLResult blContextStrokeRectIRgba32(BLContextCore* self, const BLRectI* rect, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeGeometryRgba32(impl, BL_GEOMETRY_TYPE_RECTI, rect, rgba32);
}

BL_API_IMPL BLResult blContextStrokeRectIRgba64(BLContextCore* self, const BLRectI* rect, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  return impl->virt->strokeGeometryExt(impl, BL_GEOMETRY_TYPE_RECTI, rect, &style);
}

BL_API_IMPL BLResult blContextStrokeRectIExt(BLContextCore* self, const BLRectI* rect, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeGeometryExt(impl, BL_GEOMETRY_TYPE_RECTI, rect, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult blContextStrokeRectD(BLContextCore* self, const BLRect* rect) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeGeometry(impl, BL_GEOMETRY_TYPE_RECTD, rect);
}

BL_API_IMPL BLResult blContextStrokeRectDRgba32(BLContextCore* self, const BLRect* rect, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeGeometryRgba32(impl, BL_GEOMETRY_TYPE_RECTD, rect, rgba32);
}

BL_API_IMPL BLResult blContextStrokeRectDRgba64(BLContextCore* self, const BLRect* rect, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  return impl->virt->strokeGeometryExt(impl, BL_GEOMETRY_TYPE_RECTD, rect, &style);
}

BL_API_IMPL BLResult blContextStrokeRectDExt(BLContextCore* self, const BLRect* rect, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeGeometryExt(impl, BL_GEOMETRY_TYPE_RECTD, rect, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Stroke Path Operations
// ==========================================

BL_API_IMPL BLResult blContextStrokePathD(BLContextCore* self, const BLPoint* origin, const BLPathCore* path) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokePathD(impl, origin, path);
}

BL_API_IMPL BLResult blContextStrokePathDRgba32(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokePathDRgba32(impl, origin, path, rgba32);
}

BL_API_IMPL BLResult blContextStrokePathDRgba64(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  return impl->virt->strokePathDExt(impl, origin, path, &style);
}

BL_API_IMPL BLResult blContextStrokePathDExt(BLContextCore* self, const BLPoint* origin, const BLPathCore* path, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokePathDExt(impl, origin, path, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Stroke Geometry Operations
// ==============================================

BL_API_IMPL BLResult blContextStrokeGeometry(BLContextCore* self, BLGeometryType type, const void* data) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeGeometry(impl, type, data);
}

BL_API_IMPL BLResult blContextStrokeGeometryRgba32(BLContextCore* self, BLGeometryType type, const void* data, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeGeometryRgba32(impl, type, data, rgba32);
}

BL_API_IMPL BLResult blContextStrokeGeometryRgba64(BLContextCore* self, BLGeometryType type, const void* data, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  return impl->virt->strokeGeometryExt(impl, type, data, &style);
}

BL_API_IMPL BLResult blContextStrokeGeometryExt(BLContextCore* self, BLGeometryType type, const void* data, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeGeometryExt(impl, type, data, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Stroke UTF-8 Text Operations
// ================================================

BL_API_IMPL BLResult blContextStrokeUtf8TextI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLStringView view{text, size};
  return impl->virt->strokeTextOpI(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view);
}

BL_API_IMPL BLResult blContextStrokeUtf8TextIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLStringView view{text, size};
  return impl->virt->strokeTextOpIRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, rgba32);
}

BL_API_IMPL BLResult blContextStrokeUtf8TextIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  BLStringView view{text, size};
  return impl->virt->strokeTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, &style);
}

BL_API_IMPL BLResult blContextStrokeUtf8TextIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const char* text, size_t size, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLStringView view{text, size};
  return impl->virt->strokeTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult blContextStrokeUtf8TextD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLStringView view{text, size};
  return impl->virt->strokeTextOpD(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view);
}

BL_API_IMPL BLResult blContextStrokeUtf8TextDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLStringView view{text, size};
  return impl->virt->strokeTextOpDRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, rgba32);
}

BL_API_IMPL BLResult blContextStrokeUtf8TextDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  BLStringView view{text, size};
  return impl->virt->strokeTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, &style);
}

BL_API_IMPL BLResult blContextStrokeUtf8TextDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const char* text, size_t size, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLStringView view{text, size};
  return impl->virt->strokeTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF8, &view, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Stroke UTF-16 Text Operations
// =================================================

BL_API_IMPL BLResult blContextStrokeUtf16TextI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint16_t> view{text, size};
  return impl->virt->strokeTextOpI(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view);
}

BL_API_IMPL BLResult blContextStrokeUtf16TextIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint16_t> view{text, size};
  return impl->virt->strokeTextOpIRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, rgba32);
}

BL_API_IMPL BLResult blContextStrokeUtf16TextIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  BLArrayView<uint16_t> view{text, size};
  return impl->virt->strokeTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, &style);
}

BL_API_IMPL BLResult blContextStrokeUtf16TextIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint16_t* text, size_t size, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint16_t> view{text, size};
  return impl->virt->strokeTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult blContextStrokeUtf16TextD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint16_t> view{text, size};
  return impl->virt->strokeTextOpD(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view);
}

BL_API_IMPL BLResult blContextStrokeUtf16TextDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint16_t> view{text, size};
  return impl->virt->strokeTextOpDRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, rgba32);
}

BL_API_IMPL BLResult blContextStrokeUtf16TextDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  BLArrayView<uint16_t> view{text, size};
  return impl->virt->strokeTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, &style);
}

BL_API_IMPL BLResult blContextStrokeUtf16TextDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint16_t* text, size_t size, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint16_t> view{text, size};
  return impl->virt->strokeTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF16, &view, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Stroke UTF-32 Text Operations
// =================================================

BL_API_IMPL BLResult blContextStrokeUtf32TextI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint32_t> view{text, size};
  return impl->virt->strokeTextOpI(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view);
}

BL_API_IMPL BLResult blContextStrokeUtf32TextIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint32_t> view{text, size};
  return impl->virt->strokeTextOpIRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, rgba32);
}

BL_API_IMPL BLResult blContextStrokeUtf32TextIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  BLArrayView<uint32_t> view{text, size};
  return impl->virt->strokeTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, &style);
}

BL_API_IMPL BLResult blContextStrokeUtf32TextIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const uint32_t* text, size_t size, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint32_t> view{text, size};
  return impl->virt->strokeTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult blContextStrokeUtf32TextD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint32_t> view{text, size};
  return impl->virt->strokeTextOpD(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view);
}

BL_API_IMPL BLResult blContextStrokeUtf32TextDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint32_t> view{text, size};
  return impl->virt->strokeTextOpDRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, rgba32);
}

BL_API_IMPL BLResult blContextStrokeUtf32TextDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  BLArrayView<uint32_t> view{text, size};
  return impl->virt->strokeTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, &style);
}

BL_API_IMPL BLResult blContextStrokeUtf32TextDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const uint32_t* text, size_t size, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLArrayView<uint32_t> view{text, size};
  return impl->virt->strokeTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_UTF32, &view, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Stroke Glyph Run Operations
// ===============================================

BL_API_IMPL BLResult blContextStrokeGlyphRunI(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeTextOpI(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun);
}

BL_API_IMPL BLResult blContextStrokeGlyphRunIRgba32(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeTextOpIRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun, rgba32);
}

BL_API_IMPL BLResult blContextStrokeGlyphRunIRgba64(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  return impl->virt->strokeTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun, &style);
}

BL_API_IMPL BLResult blContextStrokeGlyphRunIExt(BLContextCore* self, const BLPointI* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeTextOpIExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun, static_cast<const BLObjectCore*>(style));
}

BL_API_IMPL BLResult blContextStrokeGlyphRunD(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeTextOpD(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun);
}

BL_API_IMPL BLResult blContextStrokeGlyphRunDRgba32(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeTextOpDRgba32(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun, rgba32);
}

BL_API_IMPL BLResult blContextStrokeGlyphRunDRgba64(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  BLVarCore style = BLInternal::makeInlineStyle(BLRgba64(rgba64));
  return impl->virt->strokeTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun, &style);
}

BL_API_IMPL BLResult blContextStrokeGlyphRunDExt(BLContextCore* self, const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun, const BLUnknown* style) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeTextOpDExt(impl, origin, font, BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN, glyphRun, static_cast<const BLObjectCore*>(style));
}

// bl::Context - API - Blit Operations
// ===================================

BL_API_IMPL BLResult blContextBlitImageI(BLContextCore* self, const BLPointI* pt, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->blitImageI(impl, pt, img, imgArea);
}

BL_API_IMPL BLResult blContextBlitImageD(BLContextCore* self, const BLPoint* pt, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->blitImageD(impl, pt, img, imgArea);
}

BL_API_IMPL BLResult blContextBlitScaledImageI(BLContextCore* self, const BLRectI* rect, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->blitScaledImageI(impl, rect, img, imgArea);
}

BL_API_IMPL BLResult blContextBlitScaledImageD(BLContextCore* self, const BLRect* rect, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->blitScaledImageD(impl, rect, img, imgArea);
}

// bl::Context - Runtime Registration
// ==================================

void blContextRtInit(BLRuntimeContext* rt) noexcept {
  auto& defaultContext = bl::ContextInternal::defaultContext;

  // Initialize a NullContextImpl.
  bl::ContextInternal::initState(&bl::ContextInternal::nullState);
  bl::ContextInternal::initNullContextVirt(&defaultContext.virt);

  // Initialize a default context object (that points to NullContextImpl).
  defaultContext.impl->virt = &defaultContext.virt;
  defaultContext.impl->state = &bl::ContextInternal::nullState;
  blObjectDefaults[BL_OBJECT_TYPE_CONTEXT]._d.initDynamic(BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_CONTEXT), &defaultContext.impl);

  // Initialize built-in rendering context implementations.
  blRasterContextOnInit(rt);
}
