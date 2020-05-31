// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#include "./api-build_p.h"
#include "./context_p.h"
#include "./image_p.h"
#include "./runtime_p.h"
#include "./raster/rastercontext_p.h"

// ============================================================================
// [BLContext - Globals]
// ============================================================================

BLWrap<BLContextState> blNullContextState;

static BLContextVirt blNullContextVirt;
static BLWrap<BLContextImpl> blNullContextImpl;

// ============================================================================
// [BLContext - Init / Destroy]
// ============================================================================

BLResult blContextInit(BLContextCore* self) noexcept {
  self->impl = BLContext::none().impl;
  return BL_SUCCESS;
}

BLResult blContextInitAs(BLContextCore* self, BLImageCore* image, const BLContextCreateInfo* options) noexcept {
  self->impl = BLContext::none().impl;
  return blContextBegin(self, image, options);
}

BLResult blContextDestroy(BLContextCore* self) noexcept {
  BLContextImpl* selfI = self->impl;
  self->impl = nullptr;
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLContext - Reset]
// ============================================================================

BLResult blContextReset(BLContextCore* self) noexcept {
  BLContextImpl* selfI = self->impl;
  self->impl = &blNullContextImpl;
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLContext - Assign]
// ============================================================================

BLResult blContextAssignMove(BLContextCore* self, BLContextCore* other) noexcept {
  BLContextImpl* selfI = self->impl;
  BLContextImpl* otherI = other->impl;

  self->impl = otherI;
  other->impl = &blNullContextImpl;

  return blImplReleaseVirt(selfI);
}

BLResult blContextAssignWeak(BLContextCore* self, const BLContextCore* other) noexcept {
  BLContextImpl* selfI = self->impl;
  BLContextImpl* otherI = other->impl;

  self->impl = blImplIncRef(otherI);
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLContext - Properties]
// ============================================================================

uint32_t blContextGetType(const BLContextCore* self) noexcept {
  return self->impl->contextType;
}

BLResult blContextGetTargetSize(const BLContextCore* self, BLSize* targetSizeOut) noexcept {
  *targetSizeOut = self->impl->state->targetSize;
  return BL_SUCCESS;
}

BLImageCore* blContextGetTargetImage(const BLContextCore* self) noexcept {
  return self->impl->state->targetImage;
}

// ============================================================================
// [BLContext - Begin / End]
// ============================================================================

BLResult blContextBegin(BLContextCore* self, BLImageCore* image, const BLContextCreateInfo* options) noexcept {
  // Reject empty images.
  if (blDownCast(image)->empty())
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLContextCreateInfo noOptions {};
  if (!options)
    options = &noOptions;

  BLContextImpl* newI;
  BL_PROPAGATE(blRasterContextImplCreate(&newI, image, options));

  BLContextImpl* oldI = self->impl;
  self->impl = newI;
  return blImplReleaseVirt(oldI);
}

BLResult blContextEnd(BLContextCore* self) noexcept {
  // Currently mapped to `BLContext::reset()`.
  return blContextReset(self);
}

// ============================================================================
// [BLContext - Flush]
// ============================================================================

BLResult blContextFlush(BLContextCore* self, uint32_t flags) noexcept {
  return self->impl->virt->flush(self->impl, flags);
}

// ============================================================================
// [BLContext - Query Property]
// ============================================================================

BLResult blContextQueryProperty(const BLContextCore* self, uint32_t propertyId, void* valueOut) noexcept {
  return self->impl->virt->queryProperty(self->impl, propertyId, valueOut);
}

// ============================================================================
// [BLContext - Save / Restore]
// ============================================================================

BLResult blContextSave(BLContextCore* self, BLContextCookie* cookie) noexcept {
  return self->impl->virt->save(self->impl, cookie);
}

BLResult blContextRestore(BLContextCore* self, const BLContextCookie* cookie) noexcept {
  return self->impl->virt->restore(self->impl, cookie);
}

// ============================================================================
// [BLContext - Transformations]
// ============================================================================

BLResult blContextGetMetaMatrix(const BLContextCore* self, BLMatrix2D* m) noexcept {
  *m = self->impl->state->metaMatrix;
  return BL_SUCCESS;
}

BLResult blContextGetUserMatrix(const BLContextCore* self, BLMatrix2D* m) noexcept {
  *m = self->impl->state->userMatrix;
  return BL_SUCCESS;
}

BLResult blContextUserToMeta(BLContextCore* self) noexcept {
  return self->impl->virt->userToMeta(self->impl);
}

BLResult blContextMatrixOp(BLContextCore* self, uint32_t opType, const void* opData) noexcept {
  return self->impl->virt->matrixOp(self->impl, opType, opData);
}

// ============================================================================
// [BLContext - Rendering Hints]
// ============================================================================

BLResult blContextSetHint(BLContextCore* self, uint32_t hintType, uint32_t value) noexcept {
  return self->impl->virt->setHint(self->impl, hintType, value);
}

BLResult blContextSetHints(BLContextCore* self, const BLContextHints* hints) noexcept {
  return self->impl->virt->setHints(self->impl, hints);
}

// ============================================================================
// [BLContext - Approximation Options]
// ============================================================================

BLResult blContextSetFlattenMode(BLContextCore* self, uint32_t mode) noexcept {
  return self->impl->virt->setFlattenMode(self->impl, mode);
}

BLResult blContextSetFlattenTolerance(BLContextCore* self, double tolerance) noexcept {
  return self->impl->virt->setFlattenTolerance(self->impl, tolerance);
}

BLResult blContextSetApproximationOptions(BLContextCore* self, const BLApproximationOptions* options) noexcept {
  return self->impl->virt->setApproximationOptions(self->impl, options);
}

// ============================================================================
// [BLContext - Composition Options]
// ============================================================================

BLResult blContextSetCompOp(BLContextCore* self, uint32_t compOp) noexcept {
  return self->impl->virt->setCompOp(self->impl, compOp);
}

BLResult blContextSetGlobalAlpha(BLContextCore* self, double alpha) noexcept {
  return self->impl->virt->setGlobalAlpha(self->impl, alpha);
}

// ============================================================================
// [BLContext - Fill Options]
// ============================================================================

BLResult blContextSetFillAlpha(BLContextCore* self, double alpha) noexcept {
  return self->impl->virt->setStyleAlpha[BL_CONTEXT_OP_TYPE_FILL](self->impl, alpha);
}

BLResult blContextGetFillStyle(const BLContextCore* self, BLStyleCore* styleOut) noexcept {
  return self->impl->virt->getStyle[BL_CONTEXT_OP_TYPE_FILL](self->impl, styleOut);
}

BLResult blContextSetFillStyle(BLContextCore* self, const BLStyleCore* style) noexcept {
  return self->impl->virt->setStyle[BL_CONTEXT_OP_TYPE_FILL](self->impl, style);
}

BLResult blContextSetFillStyleRgba(BLContextCore* self, const BLRgba* rgba) noexcept {
  return self->impl->virt->setStyleRgba[BL_CONTEXT_OP_TYPE_FILL](self->impl, rgba);
}

BLResult blContextSetFillStyleRgba32(BLContextCore* self, uint32_t rgba32) noexcept {
  return self->impl->virt->setStyleRgba32[BL_CONTEXT_OP_TYPE_FILL](self->impl, rgba32);
}

BLResult blContextSetFillStyleRgba64(BLContextCore* self, uint64_t rgba64) noexcept {
  return self->impl->virt->setStyleRgba64[BL_CONTEXT_OP_TYPE_FILL](self->impl, rgba64);
}

BLResult blContextSetFillStyleObject(BLContextCore* self, const void* object) noexcept {
  return self->impl->virt->setStyleObject[BL_CONTEXT_OP_TYPE_FILL](self->impl, object);
}

BLResult blContextSetFillRule(BLContextCore* self, uint32_t fillRule) noexcept {
  return self->impl->virt->setFillRule(self->impl, fillRule);
}

// ============================================================================
// [BLContext - Stroke Options]
// ============================================================================

BLResult blContextSetStrokeAlpha(BLContextCore* self, double alpha) noexcept {
  return self->impl->virt->setStyleAlpha[BL_CONTEXT_OP_TYPE_STROKE](self->impl, alpha);
}

BLResult blContextGetStrokeStyle(const BLContextCore* self, BLStyleCore* styleOut) noexcept {
  return self->impl->virt->getStyle[BL_CONTEXT_OP_TYPE_STROKE](self->impl, styleOut);
}

BLResult blContextSetStrokeStyle(BLContextCore* self, const BLStyleCore* style) noexcept {
  return self->impl->virt->setStyle[BL_CONTEXT_OP_TYPE_STROKE](self->impl, style);
}

BLResult blContextSetStrokeStyleRgba(BLContextCore* self, const BLRgba* rgba) noexcept {
  return self->impl->virt->setStyleRgba[BL_CONTEXT_OP_TYPE_STROKE](self->impl, rgba);
}

BLResult blContextSetStrokeStyleRgba32(BLContextCore* self, uint32_t rgba32) noexcept {
  return self->impl->virt->setStyleRgba32[BL_CONTEXT_OP_TYPE_STROKE](self->impl, rgba32);
}

BLResult blContextSetStrokeStyleRgba64(BLContextCore* self, uint64_t rgba64) noexcept {
  return self->impl->virt->setStyleRgba64[BL_CONTEXT_OP_TYPE_STROKE](self->impl, rgba64);
}

BLResult blContextSetStrokeStyleObject(BLContextCore* self, const void* object) noexcept {
  return self->impl->virt->setStyleObject[BL_CONTEXT_OP_TYPE_STROKE](self->impl, object);
}

BLResult blContextSetStrokeWidth(BLContextCore* self, double width) noexcept {
  return self->impl->virt->setStrokeWidth(self->impl, width);
}

BLResult blContextSetStrokeMiterLimit(BLContextCore* self, double miterLimit) noexcept {
  return self->impl->virt->setStrokeMiterLimit(self->impl, miterLimit);
}

BLResult blContextSetStrokeCap(BLContextCore* self, uint32_t position, uint32_t strokeCap) noexcept {
  return self->impl->virt->setStrokeCap(self->impl, position, strokeCap);
}

BLResult blContextSetStrokeCaps(BLContextCore* self, uint32_t strokeCap) noexcept {
  return self->impl->virt->setStrokeCaps(self->impl, strokeCap);
}

BLResult blContextSetStrokeJoin(BLContextCore* self, uint32_t strokeJoin) noexcept {
  return self->impl->virt->setStrokeJoin(self->impl, strokeJoin);
}

BLResult blContextSetStrokeDashOffset(BLContextCore* self, double dashOffset) noexcept {
  return self->impl->virt->setStrokeDashOffset(self->impl, dashOffset);
}

BLResult blContextSetStrokeDashArray(BLContextCore* self, const BLArrayCore* dashArray) noexcept {
  return self->impl->virt->setStrokeDashArray(self->impl, dashArray);
}

BLResult blContextSetStrokeTransformOrder(BLContextCore* self, uint32_t transformOrder) noexcept {
  return self->impl->virt->setStrokeTransformOrder(self->impl, transformOrder);
}

BLResult blContextGetStrokeOptions(const BLContextCore* self, BLStrokeOptionsCore* options) noexcept {
  return blStrokeOptionsAssignWeak(options, &self->impl->state->strokeOptions);
}

BLResult blContextSetStrokeOptions(BLContextCore* self, const BLStrokeOptionsCore* options) noexcept {
  return self->impl->virt->setStrokeOptions(self->impl, options);
}

// ============================================================================
// [BLContext - Clip Operations]
// ============================================================================

BLResult blContextClipToRectI(BLContextCore* self, const BLRectI* rect) noexcept {
  return self->impl->virt->clipToRectI(self->impl, rect);
}

BLResult blContextClipToRectD(BLContextCore* self, const BLRect* rect) noexcept {
  return self->impl->virt->clipToRectD(self->impl, rect);
}

BLResult blContextRestoreClipping(BLContextCore* self) noexcept {
  return self->impl->virt->restoreClipping(self->impl);
}

// ============================================================================
// [BLContext - Clear Operations]
// ============================================================================

BLResult blContextClearAll(BLContextCore* self) noexcept {
  return self->impl->virt->clearAll(self->impl);
}

BLResult blContextClearRectI(BLContextCore* self, const BLRectI* rect) noexcept {
  return self->impl->virt->clearRectI(self->impl, rect);
}

BLResult blContextClearRectD(BLContextCore* self, const BLRect* rect) noexcept {
  return self->impl->virt->clearRectD(self->impl, rect);
}

// ============================================================================
// [BLContext - Fill Operations]
// ============================================================================

BLResult blContextFillAll(BLContextCore* self) noexcept {
  return self->impl->virt->fillAll(self->impl);
}

BLResult blContextFillRectI(BLContextCore* self, const BLRectI* rect) noexcept {
  return self->impl->virt->fillRectI(self->impl, rect);
}

BLResult blContextFillRectD(BLContextCore* self, const BLRect* rect) noexcept {
  return self->impl->virt->fillRectD(self->impl, rect);
}

BLResult blContextFillPathD(BLContextCore* self, const BLPathCore* path) noexcept {
  return self->impl->virt->fillPathD(self->impl, path);
}

BLResult blContextFillGeometry(BLContextCore* self, uint32_t geometryType, const void* geometryData) noexcept {
  return self->impl->virt->fillGeometry(self->impl, geometryType, geometryData);
}

BLResult blContextFillTextI(BLContextCore* self, const BLPointI* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) noexcept {
  return self->impl->virt->fillTextI(self->impl, pt, font, text, size, encoding);
}

BLResult blContextFillTextD(BLContextCore* self, const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) noexcept {
  return self->impl->virt->fillTextD(self->impl, pt, font, text, size, encoding);
}

BLResult blContextFillGlyphRunI(BLContextCore* self, const BLPointI* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  return self->impl->virt->fillGlyphRunI(self->impl, pt, font, glyphRun);
}

BLResult blContextFillGlyphRunD(BLContextCore* self, const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  return self->impl->virt->fillGlyphRunD(self->impl, pt, font, glyphRun);
}

// ============================================================================
// [BLContext - Stroke Operations]
// ============================================================================

BLResult blContextStrokeRectI(BLContextCore* self, const BLRectI* rect) noexcept {
  return self->impl->virt->strokeRectI(self->impl, rect);
}

BLResult blContextStrokeRectD(BLContextCore* self, const BLRect* rect) noexcept {
  return self->impl->virt->strokeRectD(self->impl, rect);
}

BLResult blContextStrokePathD(BLContextCore* self, const BLPathCore* path) noexcept {
  return self->impl->virt->strokePathD(self->impl, path);
}

BLResult blContextStrokeGeometry(BLContextCore* self, uint32_t geometryType, const void* geometryData) noexcept {
  return self->impl->virt->strokeGeometry(self->impl, geometryType, geometryData);
}

BLResult blContextStrokeTextI(BLContextCore* self, const BLPointI* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) noexcept {
  return self->impl->virt->strokeTextI(self->impl, pt, font, text, size, encoding);
}

BLResult blContextStrokeTextD(BLContextCore* self, const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) noexcept {
  return self->impl->virt->strokeTextD(self->impl, pt, font, text, size, encoding);
}

BLResult blContextStrokeGlyphRunI(BLContextCore* self, const BLPointI* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  return self->impl->virt->strokeGlyphRunI(self->impl, pt, font, glyphRun);
}

BLResult blContextStrokeGlyphRunD(BLContextCore* self, const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  return self->impl->virt->strokeGlyphRunD(self->impl, pt, font, glyphRun);
}

// ============================================================================
// [BLContext - Blit Operations]
// ============================================================================

BLResult blContextBlitImageI(BLContextCore* self, const BLPointI* pt, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  return self->impl->virt->blitImageI(self->impl, pt, img, imgArea);
}

BLResult blContextBlitImageD(BLContextCore* self, const BLPoint* pt, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  return self->impl->virt->blitImageD(self->impl, pt, img, imgArea);
}

BLResult blContextBlitScaledImageI(BLContextCore* self, const BLRectI* rect, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  return self->impl->virt->blitScaledImageI(self->impl, rect, img, imgArea);
}

BLResult blContextBlitScaledImageD(BLContextCore* self, const BLRect* rect, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  return self->impl->virt->blitScaledImageD(self->impl, rect, img, imgArea);
}

// ============================================================================
// [BLNullContext - Impl]
// ============================================================================

// NullContext implementation does nothing. These functions consistently return
// `BL_ERROR_INVALID_STATE` to inform the caller that the context is not usable.
BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

static BLResult BL_CDECL blNullContextImplNoArgs(BLContextImpl*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplSetUInt32(BLContextImpl*, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplSetUInt64(BLContextImpl*, uint64_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplSetDouble(BLContextImpl*, double) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplSetVoidPtr(BLContextImpl*, const void*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplSet2xUInt32(BLContextImpl*, uint32_t, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL blNullContextImplQueryProperty(const BLContextImpl*, uint32_t propertyId, void* valueOut) noexcept {
  switch (propertyId) {
    case BL_CONTEXT_PROPERTY_THREAD_COUNT:
      *static_cast<uint32_t*>(valueOut) = 0;
      return BL_SUCCESS;

    case BL_CONTEXT_PROPERTY_ACCUMULATED_ERROR_FLAGS:
      *static_cast<uint32_t*>(valueOut) = 0;
      return BL_SUCCESS;

    default:
      *static_cast<uint32_t*>(valueOut) = 0;
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }
}

static BLResult BL_CDECL blNullContextImplSave(BLContextImpl*, BLContextCookie*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplRestore(BLContextImpl*, const BLContextCookie*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL blNullContextImplGetStyle(const BLContextImpl*, BLStyleCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplSetStyle(BLContextImpl*, const BLStyleCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplSetRgba(BLContextImpl*, const BLRgba*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL blNullContextImplSetHints(BLContextImpl*, const BLContextHints*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplSetApproximationOptions(BLContextImpl*, const BLApproximationOptions*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplSetStrokeDashArray(BLContextImpl*, const BLArrayCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplSetStrokeOptions(BLContextImpl*, const BLStrokeOptionsCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL blNullContextImplDoRectI(BLContextImpl*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplDoRectD(BLContextImpl*, const BLRect*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplDoPathD(BLContextImpl*, const BLPathCore*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplDoGeometry(BLContextImpl*, uint32_t, const void*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplDoTextI(BLContextImpl*, const BLPointI*, const BLFontCore*, const void*, size_t, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplDoTextD(BLContextImpl*, const BLPoint*, const BLFontCore*, const void*, size_t, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplDoGlyphRunI(BLContextImpl*, const BLPointI*, const BLFontCore*, const BLGlyphRun*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplDoGlyphRunD(BLContextImpl*, const BLPoint*, const BLFontCore*, const BLGlyphRun*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

static BLResult BL_CDECL blNullContextImplBlitImageI(BLContextImpl*, const BLPointI*, const BLImageCore*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplBlitImageD(BLContextImpl*, const BLPoint*, const BLImageCore*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplBlitScaledImageI(BLContextImpl*, const BLRectI*, const BLImageCore*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplBlitScaledImageD(BLContextImpl*, const BLRect*, const BLImageCore*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

BL_DIAGNOSTIC_POP

static void blNullContextVirtInit(BLContextVirt* virt) noexcept {
  constexpr uint32_t F = BL_CONTEXT_OP_TYPE_FILL;
  constexpr uint32_t S = BL_CONTEXT_OP_TYPE_STROKE;

  virt->destroy                 = blNullContextImplNoArgs;
  virt->flush                   = blNullContextImplSetUInt32;

  virt->queryProperty           = blNullContextImplQueryProperty;

  virt->save                    = blNullContextImplSave;
  virt->restore                 = blNullContextImplRestore;

  virt->userToMeta              = blNullContextImplNoArgs;
  virt->matrixOp                = blNullContextImplDoGeometry;

  virt->setHint                 = blNullContextImplSet2xUInt32;
  virt->setHints                = blNullContextImplSetHints;

  virt->setFlattenMode          = blNullContextImplSetUInt32;
  virt->setFlattenTolerance     = blNullContextImplSetDouble;
  virt->setApproximationOptions = blNullContextImplSetApproximationOptions;

  virt->setCompOp               = blNullContextImplSetUInt32;
  virt->setGlobalAlpha          = blNullContextImplSetDouble;

  virt->setStyleAlpha[F]        = blNullContextImplSetDouble;
  virt->setStyleAlpha[S]        = blNullContextImplSetDouble;
  virt->getStyle[F]             = blNullContextImplGetStyle;
  virt->getStyle[S]             = blNullContextImplGetStyle;
  virt->setStyle[F]             = blNullContextImplSetStyle;
  virt->setStyle[S]             = blNullContextImplSetStyle;
  virt->setStyleRgba[F]         = blNullContextImplSetRgba;
  virt->setStyleRgba[S]         = blNullContextImplSetRgba;
  virt->setStyleRgba32[F]       = blNullContextImplSetUInt32;
  virt->setStyleRgba32[S]       = blNullContextImplSetUInt32;
  virt->setStyleRgba64[F]       = blNullContextImplSetUInt64;
  virt->setStyleRgba64[S]       = blNullContextImplSetUInt64;
  virt->setStyleObject[F]       = blNullContextImplSetVoidPtr;
  virt->setStyleObject[S]       = blNullContextImplSetVoidPtr;

  virt->setFillRule             = blNullContextImplSetUInt32;

  virt->setStrokeWidth          = blNullContextImplSetDouble;
  virt->setStrokeMiterLimit     = blNullContextImplSetDouble;
  virt->setStrokeCap            = blNullContextImplSet2xUInt32;
  virt->setStrokeCaps           = blNullContextImplSetUInt32;
  virt->setStrokeJoin           = blNullContextImplSetUInt32;
  virt->setStrokeTransformOrder = blNullContextImplSetUInt32;
  virt->setStrokeDashOffset     = blNullContextImplSetDouble;
  virt->setStrokeDashArray      = blNullContextImplSetStrokeDashArray;
  virt->setStrokeOptions        = blNullContextImplSetStrokeOptions;

  virt->clipToRectI             = blNullContextImplDoRectI;
  virt->clipToRectD             = blNullContextImplDoRectD;
  virt->restoreClipping         = blNullContextImplNoArgs;

  virt->clearAll                = blNullContextImplNoArgs;
  virt->clearRectI              = blNullContextImplDoRectI;
  virt->clearRectD              = blNullContextImplDoRectD;

  virt->fillAll                 = blNullContextImplNoArgs;
  virt->fillRectI               = blNullContextImplDoRectI;
  virt->fillRectD               = blNullContextImplDoRectD;
  virt->fillPathD               = blNullContextImplDoPathD;
  virt->fillGeometry            = blNullContextImplDoGeometry;
  virt->fillTextI               = blNullContextImplDoTextI;
  virt->fillTextD               = blNullContextImplDoTextD;
  virt->fillGlyphRunI           = blNullContextImplDoGlyphRunI;
  virt->fillGlyphRunD           = blNullContextImplDoGlyphRunD;

  virt->strokeRectI             = blNullContextImplDoRectI;
  virt->strokeRectD             = blNullContextImplDoRectD;
  virt->strokePathD             = blNullContextImplDoPathD;
  virt->strokeGeometry          = blNullContextImplDoGeometry;
  virt->strokeTextI             = blNullContextImplDoTextI;
  virt->strokeTextD             = blNullContextImplDoTextD;
  virt->strokeGlyphRunI         = blNullContextImplDoGlyphRunI;
  virt->strokeGlyphRunD         = blNullContextImplDoGlyphRunD;

  virt->blitImageI              = blNullContextImplBlitImageI;
  virt->blitImageD              = blNullContextImplBlitImageD;
  virt->blitScaledImageI        = blNullContextImplBlitScaledImageI;
  virt->blitScaledImageD        = blNullContextImplBlitScaledImageD;
}

// ============================================================================
// [BLContext - Runtime]
// ============================================================================

void blContextOnInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  // Initialize null context state.
  blContextStateInit(&blNullContextState);

  // Initialize null context virtual functions.
  BLContextVirt* virt = &blNullContextVirt;
  blNullContextVirtInit(virt);

  // Initialize null context built-in instance.
  BLContextImpl* contextI = &blNullContextImpl;
  blInitBuiltInNull(contextI, BL_IMPL_TYPE_CONTEXT, BL_IMPL_TRAIT_VIRT);
  contextI->virt = &blNullContextVirt;
  contextI->state = &blNullContextState;
  blAssignBuiltInNull(contextI);

  // Initialize other context implementations.
  blRasterContextOnInit(rt);
}
