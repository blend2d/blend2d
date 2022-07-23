// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "context_p.h"
#include "image_p.h"
#include "runtime_p.h"
#include "raster/rastercontext_p.h"

namespace BLContextPrivate {

// BLContext - Globals
// ===================

static BLWrap<BLContextState> nullState;
static BLObjectEthernalVirtualImpl<BLContextImpl, BLContextVirt> defaultContext;
static const BLContextCreateInfo noCreateInfo {};

// BLContext - Null Context
// ========================

namespace NullContext {

// NullContext implementation does nothing. These functions consistently return `BL_ERROR_INVALID_STATE` to inform the
// caller that the context is not usable. We don't want to mark every unused parameter by `blUnused()` in this case.
BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

static BLResult BL_CDECL implDestroy(BLObjectImpl* impl, uint32_t info) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implFlush(BLContextImpl* impl, BLContextFlushFlags flags) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL implNoArgs(BLContextImpl* impl) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetCompOp(BLContextImpl* impl, BLCompOp) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetFillRule(BLContextImpl* impl, BLFillRule) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetUInt32(BLContextImpl* impl, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetUInt64(BLContextImpl* impl, uint64_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetDouble(BLContextImpl* impl, double) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL implSave(BLContextImpl* impl, BLContextCookie*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implRestore(BLContextImpl* impl, const BLContextCookie*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL implGetStyle(const BLContextImpl* impl, BLVarCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetStyle(BLContextImpl* impl, const BLUnknown*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetRgba(BLContextImpl* impl, const BLRgba*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL implSetHint(BLContextImpl* impl, BLContextHint, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetHints(BLContextImpl* impl, const BLContextHints*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetFlattenMode(BLContextImpl* impl, BLFlattenMode) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetApproximationOptions(BLContextImpl* impl, const BLApproximationOptions*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetStrokeTransformOrder(BLContextImpl* impl, BLStrokeTransformOrder) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetStrokeDashArray(BLContextImpl* impl, const BLArrayCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetStrokeCap(BLContextImpl* impl, BLStrokeCapPosition, BLStrokeCap) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetStrokeCaps(BLContextImpl* impl, BLStrokeCap) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetStrokeJoin(BLContextImpl* impl, BLStrokeJoin) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implSetStrokeOptions(BLContextImpl* impl, const BLStrokeOptionsCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL implMatrixOp(BLContextImpl* impl, BLMatrix2DOp, const void*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL implDoRectI(BLContextImpl* impl, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implDoRectD(BLContextImpl* impl, const BLRect*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implDoPathD(BLContextImpl* impl, const BLPathCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implDoGeometry(BLContextImpl* impl, BLGeometryType, const void*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implDoTextI(BLContextImpl* impl, const BLPointI*, const BLFontCore*, const void*, size_t, BLTextEncoding) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implDoTextD(BLContextImpl* impl, const BLPoint*, const BLFontCore*, const void*, size_t, BLTextEncoding) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implDoGlyphRunI(BLContextImpl* impl, const BLPointI*, const BLFontCore*, const BLGlyphRun*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implDoGlyphRunD(BLContextImpl* impl, const BLPoint*, const BLFontCore*, const BLGlyphRun*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL implDoImageI(BLContextImpl* impl, const BLPointI*, const BLImageCore*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implDoImageD(BLContextImpl* impl, const BLPoint*, const BLImageCore*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implBlitScaledImageI(BLContextImpl* impl, const BLRectI*, const BLImageCore*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL implBlitScaledImageD(BLContextImpl* impl, const BLRect*, const BLImageCore*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

BL_DIAGNOSTIC_POP

} // {NullContext}

static void initNullContextVirt(BLContextVirt* virt) noexcept {
  constexpr uint32_t F = BL_CONTEXT_OP_TYPE_FILL;
  constexpr uint32_t S = BL_CONTEXT_OP_TYPE_STROKE;

  virt->base.destroy            = NullContext::implDestroy;
  virt->base.getProperty        = blObjectImplGetProperty;
  virt->base.setProperty        = blObjectImplSetProperty;
  virt->flush                   = NullContext::implFlush;

  virt->save                    = NullContext::implSave;
  virt->restore                 = NullContext::implRestore;

  virt->userToMeta              = NullContext::implNoArgs;
  virt->matrixOp                = NullContext::implMatrixOp;

  virt->setHint                 = NullContext::implSetHint;
  virt->setHints                = NullContext::implSetHints;

  virt->setFlattenMode          = NullContext::implSetFlattenMode;
  virt->setFlattenTolerance     = NullContext::implSetDouble;
  virt->setApproximationOptions = NullContext::implSetApproximationOptions;

  virt->setCompOp               = NullContext::implSetCompOp;
  virt->setGlobalAlpha          = NullContext::implSetDouble;

  virt->setStyleAlpha[F]        = NullContext::implSetDouble;
  virt->setStyleAlpha[S]        = NullContext::implSetDouble;
  virt->getStyle[F]             = NullContext::implGetStyle;
  virt->getStyle[S]             = NullContext::implGetStyle;
  virt->setStyle[F]             = NullContext::implSetStyle;
  virt->setStyle[S]             = NullContext::implSetStyle;
  virt->setStyleRgba[F]         = NullContext::implSetRgba;
  virt->setStyleRgba[S]         = NullContext::implSetRgba;
  virt->setStyleRgba32[F]       = NullContext::implSetUInt32;
  virt->setStyleRgba32[S]       = NullContext::implSetUInt32;
  virt->setStyleRgba64[F]       = NullContext::implSetUInt64;
  virt->setStyleRgba64[S]       = NullContext::implSetUInt64;

  virt->setFillRule             = NullContext::implSetFillRule;

  virt->setStrokeWidth          = NullContext::implSetDouble;
  virt->setStrokeMiterLimit     = NullContext::implSetDouble;
  virt->setStrokeCap            = NullContext::implSetStrokeCap;
  virt->setStrokeCaps           = NullContext::implSetStrokeCaps;
  virt->setStrokeJoin           = NullContext::implSetStrokeJoin;
  virt->setStrokeTransformOrder = NullContext::implSetStrokeTransformOrder;
  virt->setStrokeDashOffset     = NullContext::implSetDouble;
  virt->setStrokeDashArray      = NullContext::implSetStrokeDashArray;
  virt->setStrokeOptions        = NullContext::implSetStrokeOptions;

  virt->clipToRectI             = NullContext::implDoRectI;
  virt->clipToRectD             = NullContext::implDoRectD;
  virt->restoreClipping         = NullContext::implNoArgs;

  virt->clearAll                = NullContext::implNoArgs;
  virt->clearRectI              = NullContext::implDoRectI;
  virt->clearRectD              = NullContext::implDoRectD;

  virt->fillAll                 = NullContext::implNoArgs;
  virt->fillRectI               = NullContext::implDoRectI;
  virt->fillRectD               = NullContext::implDoRectD;
  virt->fillPathD               = NullContext::implDoPathD;
  virt->fillGeometry            = NullContext::implDoGeometry;
  virt->fillTextI               = NullContext::implDoTextI;
  virt->fillTextD               = NullContext::implDoTextD;
  virt->fillGlyphRunI           = NullContext::implDoGlyphRunI;
  virt->fillGlyphRunD           = NullContext::implDoGlyphRunD;
  virt->fillMaskI               = NullContext::implDoImageI;
  virt->fillMaskD               = NullContext::implDoImageD;

  virt->strokeRectI             = NullContext::implDoRectI;
  virt->strokeRectD             = NullContext::implDoRectD;
  virt->strokePathD             = NullContext::implDoPathD;
  virt->strokeGeometry          = NullContext::implDoGeometry;
  virt->strokeTextI             = NullContext::implDoTextI;
  virt->strokeTextD             = NullContext::implDoTextD;
  virt->strokeGlyphRunI         = NullContext::implDoGlyphRunI;
  virt->strokeGlyphRunD         = NullContext::implDoGlyphRunD;

  virt->blitImageI              = NullContext::implDoImageI;
  virt->blitImageD              = NullContext::implDoImageD;
  virt->blitScaledImageI        = NullContext::implBlitScaledImageI;
  virt->blitScaledImageD        = NullContext::implBlitScaledImageD;
}

} // {BLContextPrivate}

// BLContext - API - Init & Destroy
// ================================

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

  return blObjectPrivateInitWeakTagged(self, other);
}

BL_API_IMPL BLResult blContextInitAs(BLContextCore* self, BLImageCore* image, const BLContextCreateInfo* cci) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_CONTEXT]._d;
  return blContextBegin(self, image, cci);
}

BL_API_IMPL BLResult blContextDestroy(BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());

  return blObjectPrivateReleaseVirtual(self);
}

// BLContext - API - Reset
// =======================

BL_API_IMPL BLResult blContextReset(BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());

  return blObjectPrivateReplaceVirtual(self, static_cast<BLContextCore*>(&blObjectDefaults[BL_OBJECT_TYPE_CONTEXT]));
}

// BLContext - API - Assign
// ========================

BL_API_IMPL BLResult blContextAssignMove(BLContextCore* self, BLContextCore* other) noexcept {
  BL_ASSERT(self->_d.isContext());
  BL_ASSERT(other->_d.isContext());

  BLContextCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_CONTEXT]._d;
  return blObjectPrivateReplaceVirtual(self, &tmp);
}

BL_API_IMPL BLResult blContextAssignWeak(BLContextCore* self, const BLContextCore* other) noexcept {
  BL_ASSERT(self->_d.isContext());
  BL_ASSERT(other->_d.isContext());

  return blObjectPrivateAssignWeakVirtual(self, other);
}

// BLContext - API - Accessors
// ===========================

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

// BLContext - API - Begin & End
// =============================

BL_API_IMPL BLResult blContextBegin(BLContextCore* self, BLImageCore* image, const BLContextCreateInfo* cci) noexcept {
  // Reject empty images.
  if (image->dcast().empty())
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (!cci)
    cci = &BLContextPrivate::noCreateInfo;

  BLContextCore newO;
  BL_PROPAGATE(blRasterContextInitImpl(&newO, image, cci));

  return blObjectPrivateReplaceVirtual(self, &newO);
}

BL_API_IMPL BLResult blContextEnd(BLContextCore* self) noexcept {
  // Currently mapped to `BLContext::reset()`.
  return blContextReset(self);
}

// BLContext - API - Flush
// =======================

BL_API_IMPL BLResult blContextFlush(BLContextCore* self, BLContextFlushFlags flags) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->flush(impl, flags);
}

// BLContext - API - Save & Restore
// ================================

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

// BLContext - API - Transformations
// =================================

BL_API_IMPL BLResult blContextGetMetaMatrix(const BLContextCore* self, BLMatrix2D* m) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  *m = impl->state->metaMatrix;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blContextGetUserMatrix(const BLContextCore* self, BLMatrix2D* m) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  *m = impl->state->userMatrix;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blContextUserToMeta(BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->userToMeta(impl);
}

BL_API_IMPL BLResult blContextMatrixOp(BLContextCore* self, BLMatrix2DOp opType, const void* opData) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->matrixOp(impl, opType, opData);
}

// BLContext - API - Rendering Hints
// =================================

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

// BLContext - API - Approximation Options
// =======================================

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

// BLContext - API - Composition Options
// =====================================

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

// BLContext - API - Fill Options
// ==============================

BL_API_IMPL double blContextGetFillAlpha(const BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->state->styleAlpha[BL_CONTEXT_OP_TYPE_FILL];
}

BL_API_IMPL BLResult blContextSetFillAlpha(BLContextCore* self, double alpha) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleAlpha[BL_CONTEXT_OP_TYPE_FILL](impl, alpha);
}

BL_API_IMPL BLResult blContextGetFillStyle(const BLContextCore* self, BLVarCore* varOut) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->getStyle[BL_CONTEXT_OP_TYPE_FILL](impl, varOut);
}

BL_API_IMPL BLResult blContextSetFillStyle(BLContextCore* self, const BLUnknown* var) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyle[BL_CONTEXT_OP_TYPE_FILL](impl, var);
}

BL_API_IMPL BLResult blContextSetFillStyleRgba(BLContextCore* self, const BLRgba* rgba) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleRgba[BL_CONTEXT_OP_TYPE_FILL](impl, rgba);
}

BL_API_IMPL BLResult blContextSetFillStyleRgba32(BLContextCore* self, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleRgba32[BL_CONTEXT_OP_TYPE_FILL](impl, rgba32);
}

BL_API_IMPL BLResult blContextSetFillStyleRgba64(BLContextCore* self, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleRgba64[BL_CONTEXT_OP_TYPE_FILL](impl, rgba64);
}

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

// BLContext - API - Stroke Options
// ================================

BL_API_IMPL double blContextGetStrokeAlpha(const BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->state->styleAlpha[BL_CONTEXT_OP_TYPE_STROKE];
}

BL_API_IMPL BLResult blContextSetStrokeAlpha(BLContextCore* self, double alpha) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleAlpha[BL_CONTEXT_OP_TYPE_STROKE](impl, alpha);
}

BL_API_IMPL BLResult blContextGetStrokeStyle(const BLContextCore* self, BLVarCore* varOut) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->getStyle[BL_CONTEXT_OP_TYPE_STROKE](impl, varOut);
}

BL_API_IMPL BLResult blContextSetStrokeStyle(BLContextCore* self, const BLUnknown* var) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyle[BL_CONTEXT_OP_TYPE_STROKE](impl, var);
}

BL_API_IMPL BLResult blContextSetStrokeStyleRgba(BLContextCore* self, const BLRgba* rgba) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleRgba[BL_CONTEXT_OP_TYPE_STROKE](impl, rgba);
}

BL_API_IMPL BLResult blContextSetStrokeStyleRgba32(BLContextCore* self, uint32_t rgba32) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleRgba32[BL_CONTEXT_OP_TYPE_STROKE](impl, rgba32);
}

BL_API_IMPL BLResult blContextSetStrokeStyleRgba64(BLContextCore* self, uint64_t rgba64) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->setStyleRgba64[BL_CONTEXT_OP_TYPE_STROKE](impl, rgba64);
}

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

// BLContext - API - Clip Operations
// =================================

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

// BLContext - API - Clear Operations
// ==================================

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

// BLContext - API - Fill Operations
// =================================

BL_API_IMPL BLResult blContextFillAll(BLContextCore* self) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillAll(impl);
}

BL_API_IMPL BLResult blContextFillRectI(BLContextCore* self, const BLRectI* rect) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillRectI(impl, rect);
}

BL_API_IMPL BLResult blContextFillRectD(BLContextCore* self, const BLRect* rect) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillRectD(impl, rect);
}

BL_API_IMPL BLResult blContextFillPathD(BLContextCore* self, const BLPathCore* path) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillPathD(impl, path);
}

BL_API_IMPL BLResult blContextFillGeometry(BLContextCore* self, BLGeometryType geometryType, const void* geometryData) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillGeometry(impl, geometryType, geometryData);
}

BL_API_IMPL BLResult blContextFillTextI(BLContextCore* self, const BLPointI* pt, const BLFontCore* font, const void* text, size_t size, BLTextEncoding encoding) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillTextI(impl, pt, font, text, size, encoding);
}

BL_API_IMPL BLResult blContextFillTextD(BLContextCore* self, const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, BLTextEncoding encoding) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillTextD(impl, pt, font, text, size, encoding);
}

BL_API_IMPL BLResult blContextFillGlyphRunI(BLContextCore* self, const BLPointI* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillGlyphRunI(impl, pt, font, glyphRun);
}

BL_API_IMPL BLResult blContextFillGlyphRunD(BLContextCore* self, const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillGlyphRunD(impl, pt, font, glyphRun);
}

BL_API_IMPL BLResult blContextFillMaskI(BLContextCore* self, const BLPointI* pt, const BLImageCore* mask, const BLRectI* maskArea) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillMaskI(impl, pt, mask, maskArea);
}

BL_API_IMPL BLResult blContextFillMaskD(BLContextCore* self, const BLPoint* pt, const BLImageCore* mask, const BLRectI* maskArea) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->fillMaskD(impl, pt, mask, maskArea);
}

// BLContext - API - Stroke Operations
// ===================================

BL_API_IMPL BLResult blContextStrokeRectI(BLContextCore* self, const BLRectI* rect) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeRectI(impl, rect);
}

BL_API_IMPL BLResult blContextStrokeRectD(BLContextCore* self, const BLRect* rect) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeRectD(impl, rect);
}

BL_API_IMPL BLResult blContextStrokePathD(BLContextCore* self, const BLPathCore* path) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokePathD(impl, path);
}

BL_API_IMPL BLResult blContextStrokeGeometry(BLContextCore* self, BLGeometryType geometryType, const void* geometryData) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeGeometry(impl, geometryType, geometryData);
}

BL_API_IMPL BLResult blContextStrokeTextI(BLContextCore* self, const BLPointI* pt, const BLFontCore* font, const void* text, size_t size, BLTextEncoding encoding) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeTextI(impl, pt, font, text, size, encoding);
}

BL_API_IMPL BLResult blContextStrokeTextD(BLContextCore* self, const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, BLTextEncoding encoding) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeTextD(impl, pt, font, text, size, encoding);
}

BL_API_IMPL BLResult blContextStrokeGlyphRunI(BLContextCore* self, const BLPointI* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeGlyphRunI(impl, pt, font, glyphRun);
}

BL_API_IMPL BLResult blContextStrokeGlyphRunD(BLContextCore* self, const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BL_ASSERT(self->_d.isContext());
  BLContextImpl* impl = self->_impl();

  return impl->virt->strokeGlyphRunD(impl, pt, font, glyphRun);
}

// BLContext - API - Blit Operations
// =================================

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

// BLContext - Runtime Registration
// ================================

void blContextRtInit(BLRuntimeContext* rt) noexcept {
  auto& defaultContext = BLContextPrivate::defaultContext;

  // Initialize a NullContextImpl.
  BLContextPrivate::initState(&BLContextPrivate::nullState);
  BLContextPrivate::initNullContextVirt(&defaultContext.virt);

  // Initialize a default context object (that points to NullContextImpl).
  defaultContext.impl->virt = &defaultContext.virt;
  defaultContext.impl->state = &BLContextPrivate::nullState;
  blObjectDefaults[BL_OBJECT_TYPE_CONTEXT]._d.initDynamic(
    BL_OBJECT_TYPE_CONTEXT,
    BLObjectInfo{BL_OBJECT_INFO_IMMUTABLE_FLAG},
    &defaultContext.impl);

  // Initialize built-in rendering context implementations.
  blRasterContextOnInit(rt);
}

// BLContext - Tests
// =================

#ifdef BL_TEST

static void test_context_state(BLContext& ctx) {
  INFO("Verifying state management of styles");
  {
    BLRgba fillColor(0.1f, 0.2f, 0.3f, 0.4f);
    BLRgba strokeColor(0.5f, 0.6f, 0.7f, 0.8f);

    EXPECT_SUCCESS(ctx.setFillStyle(fillColor));
    EXPECT_SUCCESS(ctx.setStrokeStyle(strokeColor));
    EXPECT_SUCCESS(ctx.setFillAlpha(0.3));
    EXPECT_SUCCESS(ctx.setStrokeAlpha(0.4));

    BLVar fillStyleVar;
    BLVar strokeStyleVar;

    EXPECT_SUCCESS(ctx.getFillStyle(fillStyleVar));
    EXPECT_SUCCESS(ctx.getStrokeStyle(strokeStyleVar));
    EXPECT_EQ(ctx.fillAlpha(), 0.3);
    EXPECT_EQ(ctx.strokeAlpha(), 0.4);

    EXPECT_TRUE(fillStyleVar.isRgba());
    EXPECT_TRUE(strokeStyleVar.isRgba());

    EXPECT_EQ(fillStyleVar.as<BLRgba>(), fillColor);
    EXPECT_EQ(strokeStyleVar.as<BLRgba>(), strokeColor);

    EXPECT_SUCCESS(ctx.save());

    BLRgba newFillColor(0.9f, 0.8f, 0.7f, 0.6f);
    BLRgba newStrokeColor(0.7f, 0.6f, 0.5f, 0.4f);

    EXPECT_SUCCESS(ctx.setFillStyle(newFillColor));
    EXPECT_SUCCESS(ctx.setStrokeStyle(newStrokeColor));
    EXPECT_SUCCESS(ctx.setFillAlpha(0.7));
    EXPECT_SUCCESS(ctx.setStrokeAlpha(0.8));

    EXPECT_SUCCESS(ctx.getFillStyle(fillStyleVar));
    EXPECT_SUCCESS(ctx.getStrokeStyle(strokeStyleVar));
    EXPECT_EQ(ctx.fillAlpha(), 0.7);
    EXPECT_EQ(ctx.strokeAlpha(), 0.8);

    EXPECT_TRUE(fillStyleVar.isRgba());
    EXPECT_TRUE(strokeStyleVar.isRgba());

    EXPECT_EQ(fillStyleVar.as<BLRgba>(), newFillColor);
    EXPECT_EQ(strokeStyleVar.as<BLRgba>(), newStrokeColor);

    EXPECT_SUCCESS(ctx.restore());

    EXPECT_SUCCESS(ctx.getFillStyle(fillStyleVar));
    EXPECT_SUCCESS(ctx.getStrokeStyle(strokeStyleVar));
    EXPECT_EQ(ctx.fillAlpha(), 0.3);
    EXPECT_EQ(ctx.strokeAlpha(), 0.4);

    EXPECT_TRUE(fillStyleVar.isRgba());
    EXPECT_TRUE(strokeStyleVar.isRgba());

    EXPECT_EQ(fillStyleVar.as<BLRgba>(), fillColor);
    EXPECT_EQ(strokeStyleVar.as<BLRgba>(), strokeColor);

    EXPECT_SUCCESS(ctx.setFillAlpha(1.0));
    EXPECT_SUCCESS(ctx.setStrokeAlpha(1.0));
  }

  INFO("Verifying state management of transformations");
  {
    BLMatrix2D m = BLMatrix2D::makeScaling(2.0);
    EXPECT_SUCCESS(ctx.transform(m));
    EXPECT_EQ(ctx.userMatrix(), m);
    EXPECT_EQ(ctx.metaMatrix(), BLMatrix2D::makeIdentity());

    EXPECT_SUCCESS(ctx.save());
    EXPECT_SUCCESS(ctx.userToMeta());
    EXPECT_EQ(ctx.metaMatrix(), m);
    EXPECT_EQ(ctx.userMatrix(), BLMatrix2D::makeIdentity());
    EXPECT_SUCCESS(ctx.restore());

    EXPECT_EQ(ctx.userMatrix(), m);
    EXPECT_EQ(ctx.metaMatrix(), BLMatrix2D::makeIdentity());

    EXPECT_SUCCESS(ctx.resetMatrix());
    EXPECT_EQ(ctx.userMatrix(), BLMatrix2D::makeIdentity());
  }

  INFO("Verifying state management of composition options");
  {
    EXPECT_SUCCESS(ctx.setCompOp(BL_COMP_OP_SRC_ATOP));
    EXPECT_SUCCESS(ctx.setGlobalAlpha(0.5));

    EXPECT_EQ(ctx.compOp(), BL_COMP_OP_SRC_ATOP);
    EXPECT_EQ(ctx.globalAlpha(), 0.5);

    EXPECT_SUCCESS(ctx.save());
    EXPECT_SUCCESS(ctx.setCompOp(BL_COMP_OP_MULTIPLY));
    EXPECT_SUCCESS(ctx.setGlobalAlpha(0.9));
    EXPECT_EQ(ctx.compOp(), BL_COMP_OP_MULTIPLY);
    EXPECT_EQ(ctx.globalAlpha(), 0.9);
    EXPECT_SUCCESS(ctx.restore());

    EXPECT_EQ(ctx.compOp(), BL_COMP_OP_SRC_ATOP);
    EXPECT_EQ(ctx.globalAlpha(), 0.5);

    EXPECT_SUCCESS(ctx.setCompOp(BL_COMP_OP_SRC_OVER));
    EXPECT_SUCCESS(ctx.setGlobalAlpha(1.0));
  }
}

UNIT(context, 1) {
  BLImage img(256, 256, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  test_context_state(ctx);
}

#endif
