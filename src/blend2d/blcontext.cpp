// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blcontext_p.h"
#include "./blruntime_p.h"
#include "./raster/blrastercontext_p.h"

// ============================================================================
// [BLContext - Globals]
// ============================================================================

BLWrap<BLContextState> blNullContextState;
BLAtomicUInt64Generator blContextIdGenerator;

static BLContextVirt blNullContextVirt;
static BLWrap<BLContextImpl> blNullContextImpl;

// ============================================================================
// [BLContext - Init / Reset]
// ============================================================================

BLResult blContextInit(BLContextCore* self) noexcept {
  self->impl = BLContext::none().impl;
  return BL_SUCCESS;
}

BLResult blContextInitAs(BLContextCore* self, BLImageCore* image, const BLContextCreateOptions* options) noexcept {
  self->impl = BLContext::none().impl;
  return blContextBegin(self, image, options);
}

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
// [BLContext - Begin / End]
// ============================================================================

BLResult blContextBegin(BLContextCore* self, BLImageCore* image, const BLContextCreateOptions* options) noexcept {
  // Reject empty images.
  if (blDownCast(image)->empty())
    return blTraceError(BL_ERROR_INVALID_VALUE);

  // Reject images that already have a writer.
  if (image->impl->writer != nullptr)
    return blTraceError(BL_ERROR_BUSY);

  BLContextCreateOptions noOptions {};
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

BLResult blContextUserToMeta(BLContextCore* self) noexcept {
  return self->impl->virt->userToMeta(self->impl);
}

BLResult blContextMatrixOp(BLContextCore* self, uint32_t opType, const void* opData) noexcept {
  return self->impl->virt->matrixOp(self->impl, opType, opData);
}

// ============================================================================
// [BLContext - State]
// ============================================================================

BLResult blContextSetHint(BLContextCore* self, uint32_t hintType, uint32_t value) noexcept {
  return self->impl->virt->setHint(self->impl, hintType, value);
}

BLResult blContextSetHints(BLContextCore* self, const BLContextHints* hints) noexcept {
  return self->impl->virt->setHints(self->impl, hints);
}

BLResult blContextSetFlattenMode(BLContextCore* self, uint32_t mode) noexcept {
  return self->impl->virt->setFlattenMode(self->impl, mode);
}

BLResult blContextSetFlattenTolerance(BLContextCore* self, double tolerance) noexcept {
  return self->impl->virt->setFlattenTolerance(self->impl, tolerance);
}

BLResult blContextSetApproximationOptions(BLContextCore* self, const BLApproximationOptions* options) noexcept {
  return self->impl->virt->setApproximationOptions(self->impl, options);
}

BLResult blContextSetCompOp(BLContextCore* self, uint32_t compOp) noexcept {
  return self->impl->virt->setCompOp(self->impl, compOp);
}

BLResult blContextSetGlobalAlpha(BLContextCore* self, double alpha) noexcept {
  return self->impl->virt->setGlobalAlpha(self->impl, alpha);
}

// ============================================================================
// [BLContext - Fill Options]
// ============================================================================

BLResult blContextSetFillRule(BLContextCore* self, uint32_t fillRule) noexcept {
  return self->impl->virt->setFillRule(self->impl, fillRule);
}

BLResult blContextSetFillStyle(BLContextCore* self, const void* object) noexcept {
  return self->impl->virt->setFillStyle(self->impl, object);
}

BLResult blContextSetFillStyleRgba32(BLContextCore* self, uint32_t rgba32) noexcept {
  return self->impl->virt->setFillStyleRgba32(self->impl, rgba32);
}

BLResult blContextSetFillStyleRgba64(BLContextCore* self, uint64_t rgba64) noexcept {
  return self->impl->virt->setFillStyleRgba64(self->impl, rgba64);
}

BLResult blContextSetFillAlpha(BLContextCore* self, double alpha) noexcept {
  return self->impl->virt->setFillAlpha(self->impl, alpha);
}

// ============================================================================
// [BLContext - Stroke Options]
// ============================================================================

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

BLResult blContextSetStrokeOptions(BLContextCore* self, const BLStrokeOptionsCore* options) noexcept {
  return self->impl->virt->setStrokeOptions(self->impl, options);
}

BLResult blContextSetStrokeAlpha(BLContextCore* self, double alpha) noexcept {
  return self->impl->virt->setStrokeAlpha(self->impl, alpha);
}

BLResult blContextGetFillStyle(const BLContextCore* self, void* object) noexcept {
  return self->impl->virt->getFillStyle(self->impl, object);
}

BLResult blContextGetFillStyleRgba32(const BLContextCore* self, uint32_t* rgba32) noexcept {
  return self->impl->virt->getFillStyleRgba32(self->impl, rgba32);
}

BLResult blContextGetFillStyleRgba64(const BLContextCore* self, uint64_t* rgba64) noexcept {
  return self->impl->virt->getFillStyleRgba64(self->impl, rgba64);
}

BLResult blContextGetStrokeStyle(const BLContextCore* self, void* object) noexcept {
  return self->impl->virt->getStrokeStyle(self->impl, object);
}

BLResult blContextGetStrokeStyleRgba32(const BLContextCore* self, uint32_t* rgba32) noexcept {
  return self->impl->virt->getStrokeStyleRgba32(self->impl, rgba32);
}

BLResult blContextGetStrokeStyleRgba64(const BLContextCore* self, uint64_t* rgba64) noexcept {
  return self->impl->virt->getStrokeStyleRgba64(self->impl, rgba64);
}

BLResult blContextSetStrokeStyle(BLContextCore* self, const void* object) noexcept {
  return self->impl->virt->setStrokeStyle(self->impl, object);
}

BLResult blContextSetStrokeStyleRgba32(BLContextCore* self, uint32_t rgba32) noexcept {
  return self->impl->virt->setStrokeStyleRgba32(self->impl, rgba32);
}

BLResult blContextSetStrokeStyleRgba64(BLContextCore* self, uint64_t rgba64) noexcept {
  return self->impl->virt->setStrokeStyleRgba64(self->impl, rgba64);
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

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

static BLResult BL_CDECL blNullContextImplNop(BLContextImpl*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplU32(BLContextImpl*, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplU64(BLContextImpl*, uint64_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplDbl(BLContextImpl*, double) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplPtr(BLContextImpl*, const void*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplU32Ptr(BLContextImpl*, const void*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplU32U32(BLContextImpl*, uint32_t, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplText(BLContextImpl*, const void*, const BLFontCore*, const void*, size_t, uint32_t) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplGlyphRun(BLContextImpl*, const void*, const BLFontCore*, const BLGlyphRun*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }
static BLResult BL_CDECL blNullContextImplBlit(BLContextImpl*, const void*, const BLImageCore*, const BLRectI*) noexcept { return blTraceError(BL_ERROR_INVALID_STATE); }

BL_DIAGNOSTIC_POP

// ============================================================================
// [BLContext - Runtime Init]
// ============================================================================

void blContextRtInit(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);

  // Initialize null context state.
  blContextStateInit(&blNullContextState);

  // Initialize null context virtual functions.
  BLContextVirt* virt = &blNullContextVirt;
  blAssignFunc(&virt->destroy, blNullContextImplNop);
  blAssignFunc(&virt->flush, blNullContextImplU32);

  blAssignFunc(&virt->save, blNullContextImplPtr);
  blAssignFunc(&virt->restore, blNullContextImplPtr);

  blAssignFunc(&virt->userToMeta, blNullContextImplNop);
  blAssignFunc(&virt->matrixOp, blNullContextImplU32Ptr);

  blAssignFunc(&virt->setHint, blNullContextImplU32U32);
  blAssignFunc(&virt->setHints, blNullContextImplPtr);

  blAssignFunc(&virt->setFlattenMode, blNullContextImplU32);
  blAssignFunc(&virt->setFlattenTolerance, blNullContextImplDbl);
  blAssignFunc(&virt->setApproximationOptions, blNullContextImplPtr);

  blAssignFunc(&virt->setCompOp, blNullContextImplU32);
  blAssignFunc(&virt->setGlobalAlpha, blNullContextImplDbl);

  blAssignFunc(&virt->setFillRule, blNullContextImplU32);
  blAssignFunc(&virt->setFillAlpha, blNullContextImplDbl);
  blAssignFunc(&virt->getFillStyle, blNullContextImplPtr);
  blAssignFunc(&virt->getFillStyleRgba32, blNullContextImplPtr);
  blAssignFunc(&virt->getFillStyleRgba64, blNullContextImplPtr);
  blAssignFunc(&virt->setFillStyle, blNullContextImplPtr);
  blAssignFunc(&virt->setFillStyleRgba32, blNullContextImplU32);
  blAssignFunc(&virt->setFillStyleRgba64, blNullContextImplU64);

  blAssignFunc(&virt->setStrokeWidth, blNullContextImplDbl);
  blAssignFunc(&virt->setStrokeMiterLimit, blNullContextImplDbl);
  blAssignFunc(&virt->setStrokeCap, blNullContextImplU32U32);
  blAssignFunc(&virt->setStrokeCaps, blNullContextImplU32);
  blAssignFunc(&virt->setStrokeJoin, blNullContextImplU32);
  blAssignFunc(&virt->setStrokeTransformOrder, blNullContextImplU32);
  blAssignFunc(&virt->setStrokeDashOffset, blNullContextImplDbl);
  blAssignFunc(&virt->setStrokeDashArray, blNullContextImplPtr);
  blAssignFunc(&virt->setStrokeOptions, blNullContextImplPtr);
  blAssignFunc(&virt->setStrokeAlpha, blNullContextImplDbl);
  blAssignFunc(&virt->getStrokeStyle, blNullContextImplPtr);
  blAssignFunc(&virt->getStrokeStyleRgba32, blNullContextImplPtr);
  blAssignFunc(&virt->getStrokeStyleRgba64, blNullContextImplPtr);
  blAssignFunc(&virt->setStrokeStyle, blNullContextImplPtr);
  blAssignFunc(&virt->setStrokeStyleRgba32, blNullContextImplU32);
  blAssignFunc(&virt->setStrokeStyleRgba64, blNullContextImplU64);

  blAssignFunc(&virt->clipToRectI, blNullContextImplPtr);
  blAssignFunc(&virt->clipToRectD, blNullContextImplPtr);
  blAssignFunc(&virt->restoreClipping, blNullContextImplNop);

  blAssignFunc(&virt->clearAll, blNullContextImplNop);
  blAssignFunc(&virt->clearRectI, blNullContextImplPtr);
  blAssignFunc(&virt->clearRectD, blNullContextImplPtr);

  blAssignFunc(&virt->fillAll, blNullContextImplNop);
  blAssignFunc(&virt->fillRectI, blNullContextImplPtr);
  blAssignFunc(&virt->fillRectD, blNullContextImplPtr);
  blAssignFunc(&virt->fillPathD, blNullContextImplPtr);
  blAssignFunc(&virt->fillGeometry, blNullContextImplU32Ptr);
  blAssignFunc(&virt->fillTextI, blNullContextImplText);
  blAssignFunc(&virt->fillTextD, blNullContextImplText);
  blAssignFunc(&virt->fillGlyphRunI, blNullContextImplGlyphRun);
  blAssignFunc(&virt->fillGlyphRunD, blNullContextImplGlyphRun);

  blAssignFunc(&virt->strokeRectI, blNullContextImplPtr);
  blAssignFunc(&virt->strokeRectD, blNullContextImplPtr);
  blAssignFunc(&virt->strokePathD, blNullContextImplPtr);
  blAssignFunc(&virt->strokeGeometry, blNullContextImplU32Ptr);
  blAssignFunc(&virt->strokeTextI, blNullContextImplText);
  blAssignFunc(&virt->strokeTextD, blNullContextImplText);
  blAssignFunc(&virt->strokeGlyphRunI, blNullContextImplGlyphRun);
  blAssignFunc(&virt->strokeGlyphRunD, blNullContextImplGlyphRun);

  blAssignFunc(&virt->blitImageI, blNullContextImplBlit);
  blAssignFunc(&virt->blitImageD, blNullContextImplBlit);
  blAssignFunc(&virt->blitScaledImageI, blNullContextImplBlit);
  blAssignFunc(&virt->blitScaledImageD, blNullContextImplBlit);

  // Initialize null context built-in instance.
  BLContextImpl* impl = &blNullContextImpl;
  impl->virt = &blNullContextVirt;
  impl->state = &blNullContextState;
  impl->implType = uint8_t(BL_IMPL_TYPE_CONTEXT);
  impl->implTraits = uint8_t(BL_IMPL_TRAIT_NULL | BL_IMPL_TRAIT_VIRT);
  blAssignBuiltInNull(impl);

  // Initialize other context implementations.
  blRasterContextRtInit(rt);
}
