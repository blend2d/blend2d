// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../compop_p.h"
#include "../font_p.h"
#include "../format_p.h"
#include "../geometry_p.h"
#include "../image_p.h"
#include "../object_p.h"
#include "../path_p.h"
#include "../pathstroke_p.h"
#include "../pattern_p.h"
#include "../runtime_p.h"
#include "../string_p.h"
#include "../var_p.h"
#include "../zeroallocator_p.h"
#include "../pipeline/piperuntime_p.h"
#include "../pixelops/scalar_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rastercontext_p.h"
#include "../raster/rastercontextops_p.h"
#include "../raster/rendercommand_p.h"
#include "../raster/rendercommandprocsync_p.h"
#include "../raster/rendercommandserializer_p.h"
#include "../raster/rendertargetinfo_p.h"
#include "../raster/workerproc_p.h"
#include "../support/intops_p.h"
#include "../support/stringops_p.h"
#include "../support/traits_p.h"

#ifndef BL_BUILD_NO_JIT
  #include "../pipeline/jit/pipegenruntime_p.h"
#endif

#ifndef BL_BUILD_NO_FIXED_PIPE
  #include "../pipeline/reference/fixedpiperuntime_p.h"
#endif

using namespace BLRasterEngine;

// BLRasterEngine - ContextImpl - Globals
// ======================================

static BLContextVirt rasterImplVirtSync;
static BLContextVirt rasterImplVirtAsync;

// BLRasterEngine - ContextImpl - Tables
// =====================================

struct alignas(8) UInt32x2 {
  uint32_t first;
  uint32_t second;
};

static const UInt32x2 blRasterContextSolidDataRgba32[] = {
  { 0x00000000u, 0x0u }, // BLCompOpSolidId::kNone (not solid, never used).
  { 0x00000000u, 0x0u }, // BLCompOpSolidId::kTransparent.
  { 0xFF000000u, 0x0u }, // BLCompOpSolidId::kOpaqueBlack.
  { 0xFFFFFFFFu, 0x0u }  // BLCompOpSolidId::kOpaqueWhite.
};

static const uint64_t blRasterContextSolidDataRgba64[] = {
  0x0000000000000000u,   // BLCompOpSolidId::kNone (not solid, never used).
  0x0000000000000000u,   // BLCompOpSolidId::kTransparent.
  0xFFFF000000000000u,   // BLCompOpSolidId::kOpaqueBlack.
  0xFFFFFFFFFFFFFFFFu    // BLCompOpSolidId::kOpaqueWhite.
};

static const uint8_t blTextByteSizeShift[] = { 0, 1, 2, 0 };

// BLRasterEngine - ContextImpl - DirectStateAccessor
// ==================================================

class DirectStateAccessor {
public:
  const BLRasterContextImpl* ctxI;

  explicit BL_INLINE DirectStateAccessor(const BLRasterContextImpl* ctxI) noexcept : ctxI(ctxI) {}

  BL_INLINE const BLApproximationOptions& approximationOptions() const noexcept { return ctxI->approximationOptions(); }
  BL_INLINE const BLStrokeOptions& strokeOptions() const noexcept { return ctxI->strokeOptions(); }

  BL_INLINE uint8_t metaMatrixFixedType() const noexcept { return ctxI->metaMatrixFixedType(); }
  BL_INLINE uint8_t finalMatrixFixedType() const noexcept { return ctxI->finalMatrixFixedType(); }

  BL_INLINE const BLMatrix2D& metaMatrixFixed() const noexcept { return ctxI->metaMatrixFixed(); }
  BL_INLINE const BLMatrix2D& userMatrix() const noexcept { return ctxI->userMatrix(); }
  BL_INLINE const BLMatrix2D& finalMatrixFixed() const noexcept { return ctxI->finalMatrixFixed(); }

  BL_INLINE const BLBox& finalClipBoxD() const noexcept { return ctxI->finalClipBoxD(); }
  BL_INLINE const BLBox& finalClipBoxFixedD() const noexcept { return ctxI->finalClipBoxFixedD(); }
};

// BLRasterEngine - ContextImpl - SyncWorkState
// ============================================

//! State that is used by the synchronous rendering context when using `syncWorkData` to execute the work
//! in user thread. Some properties of `WorkData` are used as states, and those have to be saved/restored.
class SyncWorkState {
public:
  BLBox _clipBoxD;

  BL_INLINE void save(const WorkData& workData) noexcept {
    _clipBoxD = workData.edgeBuilder._clipBoxD;
  }

  BL_INLINE void restore(WorkData& workData) const noexcept {
    workData.edgeBuilder._clipBoxD = _clipBoxD;
  }
};

// BLRasterEngine - ContextImpl - Core State Internals
// ===================================================

static BL_INLINE void onBeforeConfigChange(BLRasterContextImpl* ctxI) noexcept {
  if (ctxI->contextFlags & BL_RASTER_CONTEXT_STATE_CONFIG) {
    SavedState* state = ctxI->savedState;
    state->approximationOptions = ctxI->approximationOptions();
  }
}

static BL_INLINE void onAfterFlattenToleranceChanged(BLRasterContextImpl* ctxI) noexcept {
  ctxI->internalState.toleranceFixedD = ctxI->approximationOptions().flattenTolerance * ctxI->renderTargetInfo.fpScaleD;
  ctxI->syncWorkData.edgeBuilder.setFlattenToleranceSq(blSquare(ctxI->internalState.toleranceFixedD));
}

static BL_INLINE void onAfterOffsetParameterChanged(BLRasterContextImpl* ctxI) noexcept {
  blUnused(ctxI);
}

static BL_INLINE void onAfterCompOpChanged(BLRasterContextImpl* ctxI) noexcept {
  ctxI->compOpSimplifyInfo = blCompOpSimplifyInfoArrayOf(ctxI->compOp(), BLInternalFormat(ctxI->format()));
}

// BLRasterEngine - ContextImpl - Style State Internals
// ====================================================

static BL_INLINE void initStyleToDefault(BLRasterContextImpl* ctxI, StyleData& style, uint32_t alphaI) noexcept {
  style.packed = 0;
  style.styleType = uint8_t(BL_OBJECT_TYPE_RGBA);
  style.styleFormat = ctxI->solidFormatTable[BL_RASTER_CONTEXT_SOLID_FORMAT_FRGB];
  style.alphaI = alphaI;
  style.source.solid = ctxI->solidFetchDataTable[uint32_t(BLCompOpSolidId::kOpaqueBlack)];
  style.assignRgba64(0xFFFF000000000000u);
  style.adjustedMatrix.reset();
}

static BL_INLINE void destroyValidStyle(BLRasterContextImpl* ctxI, StyleData* style) noexcept {
  style->source.fetchData->release(ctxI);
}

static BL_INLINE void onBeforeStyleChange(BLRasterContextImpl* ctxI, uint32_t opType, StyleData* style) noexcept {
  uint32_t contextFlags = ctxI->contextFlags;
  RenderFetchData* fetchData = style->source.fetchData;

  if ((contextFlags & (BL_RASTER_CONTEXT_BASE_FETCH_DATA << opType)) != 0) {
    if ((contextFlags & (BL_RASTER_CONTEXT_STATE_BASE_STYLE << opType)) == 0) {
      fetchData->release(ctxI);
      return;
    }
  }
  else {
    BL_ASSERT((contextFlags & (BL_RASTER_CONTEXT_STATE_BASE_STYLE << opType)) != 0);
  }

  BL_ASSERT(ctxI->savedState != nullptr);
  StyleData* stateStyle = &ctxI->savedState->style[opType];

  // The content is moved to the `stateStyle`, so it doesn't matter if it
  // contains solid, gradient, or pattern as the state uses the same layout.
  stateStyle->packed = style->packed;
  // `stateStyle->alpha` has been already set by `BLRasterContextImpl::save()`.
  stateStyle->source = style->source;
  stateStyle->rgba = style->rgba;
  stateStyle->adjustedMatrix.reset();
}

template<uint32_t kOpType>
static BL_INLINE BLResult setStyleToNone(BLRasterContextImpl* ctxI) noexcept {
  StyleData* style = &ctxI->internalState.style[kOpType];

  uint32_t contextFlags = ctxI->contextFlags;
  uint32_t styleFlags = (BL_RASTER_CONTEXT_STATE_BASE_STYLE | BL_RASTER_CONTEXT_BASE_FETCH_DATA) << kOpType;

  if (contextFlags & styleFlags)
    onBeforeStyleChange(ctxI, kOpType, style);

  contextFlags &= ~(styleFlags << kOpType);
  contextFlags |= BL_RASTER_CONTEXT_NO_BASE_STYLE << kOpType;

  ctxI->contextFlags = contextFlags;
  ctxI->internalState.styleType[kOpType] = uint8_t(BL_OBJECT_TYPE_NULL);

  style->packed = 0;
  return BL_SUCCESS;
}

// BLRasterEngine - ContextImpl - Style API
// ========================================

template<uint32_t kOpType>
static BLResult BL_CDECL blRasterContextImplGetStyle(const BLContextImpl* baseImpl, BLVarCore* varOut) noexcept {
  // NOTE: We just set the values as we get them in SetStyle to not spend much time by normalizing the input into
  // floats. So we have to check the tag to actually get the right value from the style. It should be okay as we
  // don't expect GetStyle() be called more than SetStyle().
  const BLRasterContextImpl* ctxI = static_cast<const BLRasterContextImpl*>(baseImpl);
  const StyleData* style = &ctxI->internalState.style[kOpType];

  BLVarCore varTmp;
  RenderFetchData* fetchData = style->source.fetchData;

  switch (style->styleType) {
    case BL_OBJECT_TYPE_RGBA: {
      BLRgba rgba;
      if (style->isRgba32())
        rgba.reset(style->rgba32);
      else if (style->isRgba64())
        rgba.reset(style->rgba64);
      else
        rgba.reset(style->rgba);
      BL_PROPAGATE(BLVarPrivate::initRgba(&varTmp, &rgba));
      break;
    }

    case BL_OBJECT_TYPE_PATTERN: {
      const BLImageCore* image = reinterpret_cast<const BLImageCore*>(&fetchData->_style);
      BLExtendMode extendMode = BLExtendMode(fetchData->_extendMode);
      BL_PROPAGATE(blPatternInitAs(reinterpret_cast<BLPatternCore*>(&varTmp), image, &style->imageArea, extendMode, &style->adjustedMatrix));
      break;
    }

    case BL_OBJECT_TYPE_GRADIENT: {
      BL_PROPAGATE(blVarInitWeak(&varTmp, &fetchData->_style));
      BL_PROPAGATE(blGradientApplyMatrixOp(reinterpret_cast<BLGradientCore*>(&varTmp), BL_MATRIX2D_OP_ASSIGN, &style->adjustedMatrix));
      break;
    }

    default: {
      return blVarAssignNull(varOut);
    }
  }

  return blVarAssignMove(varOut, &varTmp);
}

template<uint32_t kOpType>
static BLResult BL_CDECL blRasterContextImplSetStyleRgba(BLContextImpl* baseImpl, const BLRgba* rgba) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  StyleData* style = &ctxI->internalState.style[kOpType];

  BLRgba norm = *rgba;
  if (!BLRgbaPrivate::isValid(norm))
    return setStyleToNone<kOpType>(ctxI);

  uint32_t contextFlags = ctxI->contextFlags;
  uint32_t styleFlags = (BL_RASTER_CONTEXT_STATE_BASE_STYLE | BL_RASTER_CONTEXT_BASE_FETCH_DATA) << kOpType;

  if (contextFlags & styleFlags)
    onBeforeStyleChange(ctxI, kOpType, style);

  contextFlags &= ~(styleFlags | (BL_RASTER_CONTEXT_NO_BASE_STYLE << kOpType));
  norm = blClamp(norm, BLRgba(0.0f, 0.0f, 0.0f, 0.0f),
                       BLRgba(1.0f, 1.0f, 1.0f, 1.0f));
  style->assignRgba(norm);

  // Premultiply and convert to RGBA32.
  float aScale = norm.a * 255.0f;
  uint32_t r = uint32_t(blRoundToInt(norm.r * aScale));
  uint32_t g = uint32_t(blRoundToInt(norm.g * aScale));
  uint32_t b = uint32_t(blRoundToInt(norm.b * aScale));
  uint32_t a = uint32_t(blRoundToInt(aScale));
  uint32_t rgba32 = BLRgba32(r, g, b, a).value;

  uint32_t solidFormatIndex = BL_RASTER_CONTEXT_SOLID_FORMAT_FRGB;
  if (!BLRgbaPrivate::isRgba32FullyOpaque(rgba32))
    solidFormatIndex = BL_RASTER_CONTEXT_SOLID_FORMAT_ARGB;
  if (!rgba)
    solidFormatIndex = BL_RASTER_CONTEXT_SOLID_FORMAT_ZERO;

  ctxI->contextFlags = contextFlags;
  ctxI->internalState.styleType[kOpType] = uint8_t(BL_OBJECT_TYPE_RGBA);

  style->packed = 0;
  style->styleType = uint8_t(BL_OBJECT_TYPE_RGBA);
  style->styleFormat = ctxI->solidFormatTable[solidFormatIndex];
  style->source.solid.prgb32 = rgba32;
  return BL_SUCCESS;
}

template<uint32_t kOpType>
static BLResult BL_CDECL blRasterContextImplSetStyleRgba32(BLContextImpl* baseImpl, uint32_t rgba32) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  StyleData* style = &ctxI->internalState.style[kOpType];

  uint32_t contextFlags = ctxI->contextFlags;
  uint32_t styleFlags = (BL_RASTER_CONTEXT_STATE_BASE_STYLE | BL_RASTER_CONTEXT_BASE_FETCH_DATA) << kOpType;

  if (contextFlags & styleFlags)
    onBeforeStyleChange(ctxI, kOpType, style);

  style->assignRgba32(rgba32);

  uint32_t solidFormatIndex = BL_RASTER_CONTEXT_SOLID_FORMAT_FRGB;
  if (!BLRgbaPrivate::isRgba32FullyOpaque(rgba32)) {
    rgba32 = BLPixelOps::Scalar::cvt_prgb32_8888_from_argb32_8888(rgba32);
    solidFormatIndex = rgba32 == 0 ? BL_RASTER_CONTEXT_SOLID_FORMAT_ZERO
                                   : BL_RASTER_CONTEXT_SOLID_FORMAT_ARGB;
  }

  ctxI->contextFlags = contextFlags & ~(styleFlags | (BL_RASTER_CONTEXT_NO_BASE_STYLE << kOpType));
  ctxI->internalState.styleType[kOpType] = uint8_t(BL_OBJECT_TYPE_RGBA);

  style->packed = 0;
  style->styleType = uint8_t(BL_OBJECT_TYPE_RGBA);
  style->styleFormat = ctxI->solidFormatTable[solidFormatIndex];
  style->source.solid.prgb32 = rgba32;

  return BL_SUCCESS;
}

template<uint32_t kOpType>
static BLResult BL_CDECL blRasterContextImplSetStyleRgba64(BLContextImpl* baseImpl, uint64_t rgba64) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  StyleData* style = &ctxI->internalState.style[kOpType];

  uint32_t contextFlags = ctxI->contextFlags;
  uint32_t styleFlags = (BL_RASTER_CONTEXT_STATE_BASE_STYLE | BL_RASTER_CONTEXT_BASE_FETCH_DATA) << kOpType;

  if (contextFlags & styleFlags)
    onBeforeStyleChange(ctxI, kOpType, style);

  style->assignRgba64(rgba64);

  uint32_t solidFormatIndex = BL_RASTER_CONTEXT_SOLID_FORMAT_FRGB;
  uint32_t rgba32 = BLRgbaPrivate::rgba32FromRgba64(rgba64);

  if (!BLRgbaPrivate::isRgba32FullyOpaque(rgba32)) {
    rgba32 = BLPixelOps::Scalar::cvt_prgb32_8888_from_argb32_8888(rgba32);
    solidFormatIndex = rgba32 == 0 ? BL_RASTER_CONTEXT_SOLID_FORMAT_ZERO
                                   : BL_RASTER_CONTEXT_SOLID_FORMAT_ARGB;
  }

  ctxI->contextFlags = contextFlags & ~(styleFlags | (BL_RASTER_CONTEXT_NO_BASE_STYLE << kOpType));
  ctxI->internalState.styleType[kOpType] = uint8_t(BL_OBJECT_TYPE_RGBA);

  style->packed = 0;
  style->styleType = uint8_t(BL_OBJECT_TYPE_RGBA);
  style->styleFormat = ctxI->solidFormatTable[solidFormatIndex];
  style->source.solid.prgb32 = rgba32;

  return BL_SUCCESS;
}

template<uint32_t kOpType>
static BLResult BL_CDECL blRasterContextImplSetStyle(BLContextImpl* baseImpl, const BLUnknown* variant) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  StyleData* style = &ctxI->internalState.style[kOpType];

  uint32_t contextFlags = ctxI->contextFlags;
  uint32_t styleFlags = (BL_RASTER_CONTEXT_BASE_FETCH_DATA | BL_RASTER_CONTEXT_STATE_BASE_STYLE) << kOpType;

  const BLMatrix2D* srcMatrix = nullptr;
  BLMatrix2DType srcMatrixType = BL_MATRIX2D_TYPE_IDENTITY;

  switch (static_cast<const BLObjectCore*>(variant)->_d.getType()) {
    case BL_OBJECT_TYPE_NULL: {
      return setStyleToNone<kOpType>(ctxI);
    }

    case BL_OBJECT_TYPE_RGBA: {
      return blRasterContextImplSetStyleRgba<kOpType>(ctxI, static_cast<const BLRgba*>(variant));
    }

    case BL_OBJECT_TYPE_GRADIENT: {
      const BLGradientCore* gradient = static_cast<const BLGradientCore*>(variant);

      BLGradientPrivateImpl* gradientI = BLGradientPrivate::getImpl(gradient);
      RenderFetchData* fetchData = ctxI->allocFetchData();

      if (BL_UNLIKELY(!fetchData))
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

      if (contextFlags & styleFlags)
        onBeforeStyleChange(ctxI, kOpType, style);

      contextFlags &= ~(styleFlags | (BL_RASTER_CONTEXT_NO_BASE_STYLE << kOpType));
      styleFlags = BL_RASTER_CONTEXT_BASE_FETCH_DATA;

      blObjectPrivateAddRefTagged(gradient);
      fetchData->initGradientSource(gradient);
      fetchData->_extendMode = gradientI->extendMode;

      style->packed = 0;
      style->source.reset();

      BLGradientInfo gradientInfo = BLGradientPrivate::ensureInfo32(gradientI);
      if (gradientInfo.empty()) {
        styleFlags |= BL_RASTER_CONTEXT_NO_BASE_STYLE;
      }
      else if (gradientInfo.solid) {
        // Using last color according to the SVG specification.
        uint32_t rgba32 = BLPixelOps::Scalar::cvt_prgb32_8888_from_argb32_8888(BLRgbaPrivate::rgba32FromRgba64(gradientI->stops[gradientI->size - 1].rgba.value));
        fetchData->_isSetup = true;
        fetchData->_fetchFormat = gradientInfo.format;
        fetchData->_data.solid.prgb32 = rgba32;
      }

      srcMatrix = &gradientI->matrix;
      srcMatrixType = (BLMatrix2DType)gradientI->matrixType;

      ctxI->internalState.styleType[kOpType] = uint8_t(BL_OBJECT_TYPE_GRADIENT);
      style->cmdFlags |= BL_RASTER_COMMAND_FLAG_FETCH_DATA;
      style->styleType = uint8_t(BL_OBJECT_TYPE_GRADIENT);
      style->styleFormat = gradientInfo.format;
      style->quality = ctxI->hints().gradientQuality;
      style->source.fetchData = fetchData;
      break;
    }

    case BL_OBJECT_TYPE_PATTERN: {
      const BLPatternCore* pattern = static_cast<const BLPatternCore*>(variant);
      BLPatternImpl* patternI = BLPatternPrivate::getImpl(pattern);

      BLImageCore* image = &patternI->image;
      BLImageImpl* imgI = BLImagePrivate::getImpl(image);

      RenderFetchData* fetchData = ctxI->allocFetchData();
      if (BL_UNLIKELY(!fetchData))
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

      if (contextFlags & styleFlags)
        onBeforeStyleChange(ctxI, kOpType, style);

      contextFlags &= ~(styleFlags | (BL_RASTER_CONTEXT_NO_BASE_STYLE << kOpType));
      styleFlags = BL_RASTER_CONTEXT_BASE_FETCH_DATA;

      // NOTE: The area comes from pattern, it means that it's the pattern's responsibility to make sure that it's not
      // out of bounds. One special case is area having all zeros [0, 0, 0, 0], which signalizes to use the whole image.
      BLRectI area = patternI->area;
      if (!BLPatternPrivate::isAreaValidAndNonZero(area, imgI->size))
        area.reset(0, 0, imgI->size.w, imgI->size.h);

      blObjectPrivateAddRefTagged(image);
      fetchData->initPatternSource(image, area);
      fetchData->_extendMode = uint8_t(BLPatternPrivate::getExtendMode(pattern));

      style->packed = 0;
      style->imageArea = area;

      if (BL_UNLIKELY(!area.w)) {
        styleFlags |= BL_RASTER_CONTEXT_NO_BASE_STYLE;
      }

      srcMatrix = &patternI->matrix;
      srcMatrixType = BLPatternPrivate::getMatrixType(pattern);

      ctxI->internalState.styleType[kOpType] = uint8_t(BL_OBJECT_TYPE_PATTERN);
      style->cmdFlags |= BL_RASTER_COMMAND_FLAG_FETCH_DATA;
      style->styleType = uint8_t(BL_OBJECT_TYPE_PATTERN);
      style->styleFormat = uint8_t(patternI->image.dcast().format());
      style->quality = ctxI->hints().patternQuality;
      style->source.fetchData = fetchData;
      break;
    }

    default: {
      return blTraceError(BL_ERROR_INVALID_VALUE);
    }
  }

  uint32_t adjustedMatrixType;
  if (srcMatrixType == BL_MATRIX2D_TYPE_IDENTITY) {
    style->adjustedMatrix = ctxI->finalMatrix();
    adjustedMatrixType = ctxI->finalMatrixType();
  }
  else {
    BLTransformPrivate::multiply(style->adjustedMatrix, *srcMatrix, ctxI->finalMatrix());
    adjustedMatrixType = style->adjustedMatrix.type();
  }

  if (BL_UNLIKELY(adjustedMatrixType >= BL_MATRIX2D_TYPE_INVALID))
    styleFlags |= BL_RASTER_CONTEXT_NO_BASE_STYLE;

  ctxI->contextFlags = contextFlags | (styleFlags << kOpType);
  style->adjustedMatrixType = uint8_t(adjustedMatrixType);

  return BL_SUCCESS;
}

// BLRasterEngine - ContextImpl - Stroke State Internals
// =====================================================

static BL_INLINE void onBeforeStrokeChange(BLRasterContextImpl* ctxI) noexcept {
  if (ctxI->contextFlags & BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS) {
    SavedState* state = ctxI->savedState;
    memcpy(&state->strokeOptions, &ctxI->strokeOptions(), sizeof(BLStrokeOptionsCore));
    blObjectPrivateAddRefTagged(&state->strokeOptions.dashArray);
  }
}

// BLRasterEngine - ContextImpl - Matrix State Internals
// =====================================================

// Called before `userMatrix` is changed.
//
// This function is responsible for saving the current userMatrix in case that the `BL_RASTER_CONTEXT_STATE_USER_MATRIX`
// flag is set, which means that the userMatrix must be saved before any modification.
static BL_INLINE void onBeforeUserMatrixChange(BLRasterContextImpl* ctxI) noexcept {
  if ((ctxI->contextFlags & BL_RASTER_CONTEXT_STATE_USER_MATRIX) != 0) {
    // MetaMatrix change would also save UserMatrix, no way this could be unset.
    BL_ASSERT((ctxI->contextFlags & BL_RASTER_CONTEXT_STATE_META_MATRIX) != 0);

    SavedState* state = ctxI->savedState;
    state->altMatrix = ctxI->finalMatrix();
    state->userMatrix = ctxI->userMatrix();
  }
}

static BL_INLINE void updateFinalMatrix(BLRasterContextImpl* ctxI) noexcept {
  BLTransformPrivate::multiply(ctxI->internalState.finalMatrix, ctxI->userMatrix(), ctxI->metaMatrix());
}

static BL_INLINE void updateMetaMatrixFixed(BLRasterContextImpl* ctxI) noexcept {
  ctxI->internalState.metaMatrixFixed = ctxI->metaMatrix();
  ctxI->internalState.metaMatrixFixed.postScale(ctxI->renderTargetInfo.fpScaleD);
}

static BL_INLINE void updateFinalMatrixFixed(BLRasterContextImpl* ctxI) noexcept {
  ctxI->internalState.finalMatrixFixed = ctxI->finalMatrix();
  ctxI->internalState.finalMatrixFixed.postScale(ctxI->renderTargetInfo.fpScaleD);
}

// Called after `userMatrix` has been modified.
//
// Responsible for updating `finalMatrix` and other matrix information.
static BL_INLINE void onAfterUserMatrixChanged(BLRasterContextImpl* ctxI) noexcept {
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_NO_USER_MATRIX         |
                          BL_RASTER_CONTEXT_INTEGRAL_TRANSLATION   |
                          BL_RASTER_CONTEXT_STATE_USER_MATRIX      |
                          BL_RASTER_CONTEXT_SHARED_FILL_STATE      |
                          BL_RASTER_CONTEXT_SHARED_STROKE_EXT_STATE);

  updateFinalMatrix(ctxI);
  updateFinalMatrixFixed(ctxI);

  const BLMatrix2D& fm = ctxI->finalMatrixFixed();
  uint32_t finalMatrixType = ctxI->finalMatrix().type();

  ctxI->internalState.finalMatrixType = uint8_t(finalMatrixType);
  ctxI->internalState.finalMatrixFixedType = uint8_t(blMax<uint32_t>(finalMatrixType, BL_MATRIX2D_TYPE_SCALE));

  if (finalMatrixType <= BL_MATRIX2D_TYPE_TRANSLATE) {
    // No scaling - input coordinates have pixel granularity. Check if the translation has pixel granularity as well
    // and setup the `translationI` data for that case.
    if (fm.m20 >= ctxI->fpMinSafeCoordD && fm.m20 <= ctxI->fpMaxSafeCoordD &&
        fm.m21 >= ctxI->fpMinSafeCoordD && fm.m21 <= ctxI->fpMaxSafeCoordD) {
      // We need 64-bit ints here as we are already scaled. We also need a `floor` function as we have to handle
      // negative translations which cannot be truncated (the default conversion).
      int64_t tx64 = blFloorToInt64(fm.m20);
      int64_t ty64 = blFloorToInt64(fm.m21);

      // Pixel to pixel translation is only possible when both fixed points `tx64` and `ty64` have all zeros in
      // their fraction parts.
      if (((tx64 | ty64) & ctxI->renderTargetInfo.fpMaskI) == 0) {
        int tx = int(tx64 >> ctxI->renderTargetInfo.fpShiftI);
        int ty = int(ty64 >> ctxI->renderTargetInfo.fpShiftI);

        ctxI->setTranslationI(BLPointI(tx, ty));
        ctxI->contextFlags |= BL_RASTER_CONTEXT_INTEGRAL_TRANSLATION;
      }
    }
  }
}

// BLRasterEngine - ContextImpl - Clip State Internals
// ===================================================

static BL_INLINE void onBeforeClipBoxChange(BLRasterContextImpl* ctxI) noexcept {
  if (ctxI->contextFlags & BL_RASTER_CONTEXT_STATE_CLIP) {
    SavedState* state = ctxI->savedState;
    state->finalClipBoxD = ctxI->finalClipBoxD();
  }
}

static BL_INLINE void resetClippingToMetaClipBox(BLRasterContextImpl* ctxI) noexcept {
  const BLBoxI& meta = ctxI->metaClipBoxI();
  ctxI->internalState.finalClipBoxI.reset(meta.x0, meta.y0, meta.x1, meta.y1);
  ctxI->internalState.finalClipBoxD.reset(meta.x0, meta.y0, meta.x1, meta.y1);
  ctxI->setFinalClipBoxFixedD(ctxI->finalClipBoxD() * ctxI->renderTargetInfo.fpScaleD);
}

static BL_INLINE void restoreClippingFromState(BLRasterContextImpl* ctxI, SavedState* savedState) noexcept {
  // TODO: [Rendering Context] Path-based clipping.
  ctxI->internalState.finalClipBoxD = savedState->finalClipBoxD;
  ctxI->internalState.finalClipBoxI.reset(
    blTruncToInt(ctxI->finalClipBoxD().x0),
    blTruncToInt(ctxI->finalClipBoxD().y0),
    blCeilToInt(ctxI->finalClipBoxD().x1),
    blCeilToInt(ctxI->finalClipBoxD().y1));

  double fpScale = ctxI->renderTargetInfo.fpScaleD;
  ctxI->setFinalClipBoxFixedD(BLBox(
    ctxI->finalClipBoxD().x0 * fpScale,
    ctxI->finalClipBoxD().y0 * fpScale,
    ctxI->finalClipBoxD().x1 * fpScale,
    ctxI->finalClipBoxD().y1 * fpScale));
}

// BLRasterEngine - ContextImpl - Ensure Fill Function
// ===================================================

static BL_INLINE BLResult ensureFillFunc(BLRasterContextImpl* ctxI, uint32_t signature, BLPipeline::DispatchData* out) noexcept {
  BLPipeline::PipeLookupCache::MatchType m = ctxI->pipeLookupCache.match(signature);
  if (m.isValid()) {
    *out = ctxI->pipeLookupCache.matchToData(m);
    return BL_SUCCESS;
  }
  else {
    return ctxI->pipeProvider.get(signature, out, &ctxI->pipeLookupCache);
  }
}

// BLRasterEngine - ContextImpl - Ensure Fetch Data
// ================================================

template<RenderCommandSerializerFlags Flags, uint32_t RenderingMode>
static BL_INLINE bool ensureFetchData(BLRasterContextImpl* ctxI, RenderCommandSerializerCore<RenderingMode>& serializer) noexcept {
  blUnused(ctxI);

  if (serializer.command().hasFetchData()) {
    RenderFetchData* fetchData = serializer.command()._source.fetchData;

    if (!blTestFlag(Flags, RenderCommandSerializerFlags::kBlit | RenderCommandSerializerFlags::kSolid)) {
      if (!fetchData->isSetup() && !blRasterFetchDataSetup(fetchData, serializer.styleData()))
        return false;
    }

    if (blTestFlag(Flags, RenderCommandSerializerFlags::kBlit)) {
      BL_ASSERT(fetchData->isSetup());
    }

    serializer._pipeSignature.addFetchType(fetchData->_fetchType);
  }

  return true;
}

// BLRasterEngine - ContextImpl - Fill Internals - Prepare
// =======================================================

template<uint32_t RenderingMode>
static BL_INLINE uint32_t prepareClear(BLRasterContextImpl* ctxI, RenderCommandSerializerCore<RenderingMode>& serializer, uint32_t nopFlags) noexcept {
  BLCompOpSimplifyInfo simplifyInfo = blCompOpSimplifyInfo(BL_COMP_OP_CLEAR, BLInternalFormat(ctxI->format()), BLInternalFormat::kPRGB32);
  uint32_t contextFlags = ctxI->contextFlags;

  nopFlags &= contextFlags;
  if (nopFlags != 0)
    return BL_RASTER_CONTEXT_PREPARE_STATUS_NOP;

  // The combination of a destination format, source format, and compOp results in a solid fill, so initialize the
  // command accordingly to `solidId` type.
  serializer.initPipeline(simplifyInfo.signature());
  serializer.initCommand(ctxI->renderTargetInfo.fullAlphaI);
  serializer.initFetchSolid(ctxI->solidFetchDataTable[uint32_t(simplifyInfo.solidId())]);
  return BL_RASTER_CONTEXT_PREPARE_STATUS_SOLID;
}

template<uint32_t RenderingMode>
static BL_INLINE uint32_t prepareFill(BLRasterContextImpl* ctxI, RenderCommandSerializerCore<RenderingMode>& serializer, const StyleData* styleData, uint32_t nopFlags) noexcept {
  BLCompOpSimplifyInfo simplifyInfo = ctxI->compOpSimplifyInfo[styleData->styleFormat];
  BLCompOpSolidId solidId = simplifyInfo.solidId();
  uint32_t contextFlags = ctxI->contextFlags | uint32_t(solidId);

  serializer.initPipeline(simplifyInfo.signature());
  serializer.initCommand(styleData->alphaI);
  serializer.initFetchDataFromStyle(styleData);

  // Likely case - composition flag doesn't lead to a solid fill and there are no other 'NO_' flags so the rendering
  // of this command should produce something. This works since we combined `contextFlags` with `srcSolidId`, which
  // is only non-zero to force either NOP or SOLID fill.
  nopFlags &= contextFlags;
  if (nopFlags == 0)
    return BL_RASTER_CONTEXT_PREPARE_STATUS_FETCH;

  // Remove reserved flags we may have added to `nopFlags` if srcSolidId was non-zero and add a possible condition
  // flag to `nopFlags` if the composition is NOP (DST-COPY).
  nopFlags &= ~BL_RASTER_CONTEXT_NO_RESERVED;
  nopFlags |= uint32_t(serializer.pipeSignature() == BLCompOpSimplifyInfo::dstCopy().signature());

  // The combination of a destination format, source format, and compOp results in a solid fill, so initialize the
  // command accordingly to `solidId` type.
  serializer.clearFetchFlags();
  serializer.initFetchSolid(ctxI->solidFetchDataTable[size_t(solidId)]);

  // If there are bits in `nopFlags` it means there is nothing to render as compOp, style, alpha, or something else
  // is nop/invalid.
  if (nopFlags != 0)
    return BL_RASTER_CONTEXT_PREPARE_STATUS_NOP;
  else
    return BL_RASTER_CONTEXT_PREPARE_STATUS_SOLID;
}

// BLRasterEngine - ContextImpl - Async
// ====================================

static BL_INLINE void releaseFetchQueue(BLRasterContextImpl* ctxI, RenderFetchQueue* queue) noexcept {
  while (queue) {
    for (RenderFetchData* fetchData : *queue)
      fetchData->release(ctxI);
    queue = queue->next();
  }
}

static BL_INLINE void releaseImageQueue(BLRasterContextImpl* ctxI, RenderImageQueue* queue) noexcept {
  blUnused(ctxI);

  while (queue) {
    for (BLImageCore& image : *queue)
      BLImagePrivate::releaseInstance(&image);
    queue = queue->next();
  }
}

static BL_NOINLINE BLResult flushRenderBatch(BLRasterContextImpl* ctxI) noexcept {
  WorkerManager& mgr = ctxI->workerMgr();
  if (mgr.hasPendingCommands()) {
    mgr.finalizeBatch();

    WorkerSynchronization& synchronization = mgr._synchronization;
    RenderBatch* batch = mgr.currentBatch();
    uint32_t threadCount = mgr.threadCount();

    synchronization.beforeStart(threadCount, batch->jobCount() > 0);
    batch->_synchronization = &synchronization;

    for (uint32_t i = 0; i < threadCount; i++) {
      WorkData* workData = mgr._workDataStorage[i];
      workData->batch = batch;
      workData->initContextData(ctxI->dstData);
    }

    // Just to make sure that all the changes are visible to the threads.
    blAtomicThreadFence();

    for (uint32_t i = 0; i < threadCount; i++) {
      mgr._workerThreads[i]->run(WorkerProc::workerThreadEntry, mgr._workDataStorage[i]);
    }

    // User thread acts as a worker too.
    {
      WorkData* workData = &ctxI->syncWorkData;
      SyncWorkState workState;

      workState.save(*workData);
      workData->batch = batch;
      WorkerProc::processWorkData(workData);
      workState.restore(*workData);
    }

    if (threadCount) {
      mgr._synchronization.waitForThreadsToFinish();
      ctxI->syncWorkData._accumulatedErrorFlags |= blAtomicFetchRelaxed(&batch->_accumulatedErrorFlags);
    }

    releaseFetchQueue(ctxI, batch->_fetchList.first());
    releaseImageQueue(ctxI, batch->_imageList.first());

    mgr._allocator.clear();
    mgr.initFirstBatch();

    ctxI->syncWorkData.startOver();
    ctxI->contextFlags &= ~BL_RASTER_CONTEXT_SHARED_ALL_FLAGS;
    ctxI->sharedFillState = nullptr;
    ctxI->sharedStrokeState = nullptr;
  }

  return BL_SUCCESS;
}

// BLRasterEngine - ContextImpl - Flush
// ====================================

static BLResult BL_CDECL blRasterContextImplFlush(BLContextImpl* baseImpl, BLContextFlushFlags flags) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  // Nothing to flush if the rendering context is synchronous.
  if (ctxI->isSync())
    return BL_SUCCESS;

  if (flags & BL_CONTEXT_FLUSH_SYNC) {
    BL_PROPAGATE(flushRenderBatch(ctxI));
  }

  return BL_SUCCESS;
}

// BLRasterEngine - ContextImpl - Properties
// =========================================

static BLResult BL_CDECL blRasterContextImplGetProperty(const BLObjectImpl* impl, const char* name, size_t nameSize, BLVarCore* valueOut) noexcept {
  const BLRasterContextImpl* ctxI = static_cast<const BLRasterContextImpl*>(impl);

  if (blMatchProperty(name, nameSize, "threadCount")) {
    uint32_t value = ctxI->isSync() ? uint32_t(0) : uint32_t(ctxI->workerMgr().threadCount() + 1u);
    return blVarAssignUInt64(valueOut, value);
  }

  if (blMatchProperty(name, nameSize, "accumulatedErrorFlags")) {
    uint32_t value = ctxI->syncWorkData.accumulatedErrorFlags();
    return blVarAssignUInt64(valueOut, value);
  }

  return blObjectImplGetProperty(ctxI, name, nameSize, valueOut);
}

static BLResult BL_CDECL blRasterContextImplSetProperty(BLObjectImpl* impl, const char* name, size_t nameSize, const BLVarCore* value) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(impl);
  return blObjectImplSetProperty(ctxI, name, nameSize, value);
}

// BLRasterEngine - ContextImpl - Save & Restore
// =============================================

// Returns how many states have to be restored to match the `stateId`. It would return zero if there is no state
// that matches `stateId`.
static BL_INLINE uint32_t getNumStatesToRestore(SavedState* savedState, uint64_t stateId) noexcept {
  uint32_t n = 1;
  do {
    uint64_t savedId = savedState->stateId;
    if (savedId <= stateId)
      return savedId == stateId ? n : uint32_t(0);
    n++;
    savedState = savedState->prevState;
  } while (savedState);

  return 0;
}

// "CoreState" consists of states that are always saved and restored to make the restoration simpler. All states
// saved/restored by CoreState should be cheap to copy.
static BL_INLINE void blRasterContextImplSaveCoreState(BLRasterContextImpl* ctxI, SavedState* state) noexcept {
  state->prevContextFlags = ctxI->contextFlags;

  state->hints = ctxI->hints();
  state->compOp = ctxI->compOp();
  state->fillRule = ctxI->fillRule();
  state->clipMode = ctxI->syncWorkData.clipMode;

  state->metaMatrixType = ctxI->metaMatrixType();
  state->finalMatrixType = ctxI->finalMatrixType();
  state->metaMatrixFixedType = ctxI->metaMatrixFixedType();
  state->finalMatrixFixedType = ctxI->finalMatrixFixedType();
  state->translationI = ctxI->translationI();

  state->globalAlpha = ctxI->globalAlphaD();
  state->styleAlpha[0] = ctxI->internalState.styleAlpha[0];
  state->styleAlpha[1] = ctxI->internalState.styleAlpha[1];

  state->globalAlphaI = ctxI->globalAlphaI();
  state->style[0].alphaI = ctxI->getStyle(0)->alphaI;
  state->style[1].alphaI = ctxI->getStyle(1)->alphaI;
}

static BL_INLINE void blRasterContextImplRestoreCoreState(BLRasterContextImpl* ctxI, SavedState* state) noexcept {
  ctxI->contextFlags = state->prevContextFlags;

  ctxI->internalState.hints = state->hints;
  ctxI->internalState.compOp = state->compOp;
  ctxI->internalState.fillRule = state->fillRule;
  ctxI->syncWorkData.clipMode = state->clipMode;

  ctxI->internalState.metaMatrixType = state->metaMatrixType;
  ctxI->internalState.finalMatrixType = state->finalMatrixType;
  ctxI->internalState.metaMatrixFixedType = state->metaMatrixFixedType;
  ctxI->internalState.finalMatrixFixedType = state->finalMatrixFixedType;
  ctxI->internalState.translationI = state->translationI;

  ctxI->internalState.globalAlpha = state->globalAlpha;
  ctxI->internalState.styleAlpha[0] = state->styleAlpha[0];
  ctxI->internalState.styleAlpha[1] = state->styleAlpha[1];

  ctxI->internalState.globalAlphaI = state->globalAlphaI;
  ctxI->internalState.style[0].alphaI = state->style[0].alphaI;
  ctxI->internalState.style[1].alphaI = state->style[1].alphaI;

  onAfterCompOpChanged(ctxI);
}

static void blRasterContextImplDiscardStates(BLRasterContextImpl* ctxI, SavedState* topState) noexcept {
  SavedState* savedState = ctxI->savedState;
  if (savedState == topState)
    return;

  // NOTE: No need to handle states that don't use dynamically allocated memory.
  uint32_t contextFlags = ctxI->contextFlags;
  do {
    if ((contextFlags & (BL_RASTER_CONTEXT_FILL_FETCH_DATA | BL_RASTER_CONTEXT_STATE_FILL_STYLE)) == BL_RASTER_CONTEXT_FILL_FETCH_DATA) {
      constexpr uint32_t kOpType = BL_CONTEXT_OP_TYPE_FILL;
      if (savedState->style[kOpType].hasFetchData()) {
        RenderFetchData* fetchData = savedState->style[kOpType].source.fetchData;
        fetchData->release(ctxI);
      }
    }

    if ((contextFlags & (BL_RASTER_CONTEXT_STROKE_FETCH_DATA | BL_RASTER_CONTEXT_STATE_STROKE_STYLE)) == BL_RASTER_CONTEXT_STROKE_FETCH_DATA) {
      constexpr uint32_t kOpType = BL_CONTEXT_OP_TYPE_STROKE;
      if (savedState->style[kOpType].hasFetchData()) {
        RenderFetchData* fetchData = savedState->style[kOpType].source.fetchData;
        fetchData->release(ctxI);
      }
    }

    if ((contextFlags & BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS) == 0) {
      blCallDtor(savedState->strokeOptions.dashArray);
    }

    SavedState* prevState = savedState->prevState;
    contextFlags = savedState->prevContextFlags;

    ctxI->freeSavedState(savedState);
    savedState = prevState;
  } while (savedState != topState);

  // Make 'topState' the current state.
  ctxI->savedState = topState;
  ctxI->contextFlags = contextFlags;
}

static BLResult BL_CDECL blRasterContextImplSave(BLContextImpl* baseImpl, BLContextCookie* cookie) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  SavedState* newState = ctxI->allocSavedState();

  if (BL_UNLIKELY(!newState))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  newState->prevState = ctxI->savedState;
  newState->stateId = BLTraits::maxValue<uint64_t>();

  ctxI->savedState = newState;
  ctxI->internalState.savedStateCount++;

  blRasterContextImplSaveCoreState(ctxI, newState);
  ctxI->contextFlags |= BL_RASTER_CONTEXT_STATE_ALL_FLAGS;

  if (!cookie)
    return BL_SUCCESS;

  // Setup the given `coookie` and make the state cookie-dependent.
  uint64_t stateId = ++ctxI->stateIdCounter;
  newState->stateId = stateId;

  cookie->reset(ctxI->contextOriginId, stateId);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplRestore(BLContextImpl* baseImpl, const BLContextCookie* cookie) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  SavedState* savedState = ctxI->savedState;

  if (BL_UNLIKELY(!savedState))
    return blTraceError(BL_ERROR_NO_STATES_TO_RESTORE);

  // By default there would be only one state to restore if `cookie` was not provided.
  uint32_t n = 1;

  if (cookie) {
    // Verify context origin.
    if (BL_UNLIKELY(cookie->data[0] != ctxI->contextOriginId))
      return blTraceError(BL_ERROR_NO_MATCHING_COOKIE);

    // Verify cookie payload and get the number of states we have to restore (if valid).
    n = getNumStatesToRestore(savedState, cookie->data[1]);
    if (BL_UNLIKELY(n == 0))
      return blTraceError(BL_ERROR_NO_MATCHING_COOKIE);
  }
  else {
    // A state that has a `stateId` assigned cannot be restored without a matching cookie.
    if (savedState->stateId != BLTraits::maxValue<uint64_t>())
      return blTraceError(BL_ERROR_NO_MATCHING_COOKIE);
  }

  uint32_t sharedFlagsToKeep = ctxI->contextFlags & BL_RASTER_CONTEXT_SHARED_ALL_FLAGS;
  ctxI->internalState.savedStateCount -= n;

  for (;;) {
    uint32_t restoreFlags = ctxI->contextFlags;
    blRasterContextImplRestoreCoreState(ctxI, savedState);

    if ((restoreFlags & BL_RASTER_CONTEXT_STATE_CONFIG) == 0) {
      ctxI->internalState.approximationOptions = savedState->approximationOptions;
      onAfterFlattenToleranceChanged(ctxI);
      onAfterOffsetParameterChanged(ctxI);

      sharedFlagsToKeep &= ~BL_RASTER_CONTEXT_SHARED_FILL_STATE;
    }

    if ((restoreFlags & BL_RASTER_CONTEXT_STATE_CLIP) == 0) {
      restoreClippingFromState(ctxI, savedState);
      sharedFlagsToKeep &= ~BL_RASTER_CONTEXT_SHARED_FILL_STATE;
    }

    if ((restoreFlags & BL_RASTER_CONTEXT_STATE_FILL_STYLE) == 0) {
      constexpr uint32_t kOpType = BL_CONTEXT_OP_TYPE_FILL;

      StyleData* dst = &ctxI->internalState.style[kOpType];
      StyleData* src = &savedState->style[kOpType];

      if (restoreFlags & BL_RASTER_CONTEXT_FILL_FETCH_DATA)
        destroyValidStyle(ctxI, dst);

      dst->packed = src->packed;
      dst->source = src->source;
      dst->rgba = src->rgba;
      dst->adjustedMatrix = src->adjustedMatrix;
      ctxI->internalState.styleType[kOpType] = src->styleType;
    }

    if ((restoreFlags & BL_RASTER_CONTEXT_STATE_STROKE_STYLE) == 0) {
      constexpr uint32_t kOpType = BL_CONTEXT_OP_TYPE_STROKE;

      StyleData* dst = &ctxI->internalState.style[kOpType];
      StyleData* src = &savedState->style[kOpType];

      if (restoreFlags & BL_RASTER_CONTEXT_STROKE_FETCH_DATA)
        destroyValidStyle(ctxI, dst);

      dst->packed = src->packed;
      dst->source = src->source;
      dst->rgba = src->rgba;
      dst->adjustedMatrix = src->adjustedMatrix;
      ctxI->internalState.styleType[kOpType] = src->styleType;
    }

    if ((restoreFlags & BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS) == 0) {
      // NOTE: This code is unsafe, but since we know that `BLStrokeOptions` is movable it's just fine. We destroy
      // `BLStrokeOptions` first and then move it into that destroyed instance params from the state itself.
      blArrayReset(&ctxI->internalState.strokeOptions.dashArray);
      memcpy(&ctxI->internalState.strokeOptions, &savedState->strokeOptions, sizeof(BLStrokeOptions));
      sharedFlagsToKeep &= ~(BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE |
                             BL_RASTER_CONTEXT_SHARED_STROKE_EXT_STATE  );
    }

    // UserMatrix state is unset when MetaMatrix and/or UserMatrix were saved.
    if ((restoreFlags & BL_RASTER_CONTEXT_STATE_USER_MATRIX) == 0) {
      ctxI->internalState.userMatrix = savedState->userMatrix;

      if ((restoreFlags & BL_RASTER_CONTEXT_STATE_META_MATRIX) == 0) {
        ctxI->internalState.metaMatrix = savedState->altMatrix;
        updateFinalMatrix(ctxI);
        updateMetaMatrixFixed(ctxI);
        updateFinalMatrixFixed(ctxI);
      }
      else {
        ctxI->internalState.finalMatrix = savedState->altMatrix;
        updateFinalMatrixFixed(ctxI);
      }

      sharedFlagsToKeep &= ~(BL_RASTER_CONTEXT_SHARED_FILL_STATE        |
                             BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE |
                             BL_RASTER_CONTEXT_SHARED_STROKE_EXT_STATE  );
    }

    SavedState* finishedSavedState = savedState;
    savedState = savedState->prevState;

    ctxI->savedState = savedState;
    ctxI->freeSavedState(finishedSavedState);

    if (--n == 0)
      break;
  }

  ctxI->contextFlags &= ~BL_RASTER_CONTEXT_SHARED_ALL_FLAGS;
  ctxI->contextFlags |= sharedFlagsToKeep;

  return BL_SUCCESS;
}

// BLRasterEngine - ContextImpl - Transformations
// ==============================================

static BLResult BL_CDECL blRasterContextImplMatrixOp(BLContextImpl* baseImpl, BLMatrix2DOp opType, const void* opData) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  onBeforeUserMatrixChange(ctxI);
  BL_PROPAGATE(blMatrix2DApplyOp(&ctxI->internalState.userMatrix, opType, opData));

  onAfterUserMatrixChanged(ctxI);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplUserToMeta(BLContextImpl* baseImpl) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  constexpr uint32_t kUserAndMetaFlags = BL_RASTER_CONTEXT_STATE_META_MATRIX |
                                         BL_RASTER_CONTEXT_STATE_USER_MATRIX ;

  if (ctxI->contextFlags & kUserAndMetaFlags) {
    SavedState* state = ctxI->savedState;

    // Always save both `metaMatrix` and `userMatrix` in case we have to save the current state before we change the
    // matrix. In this case the `altMatrix` of the state would store the current `metaMatrix` and on state restore
    // the final matrix would be recalculated in-place.
    state->altMatrix = ctxI->metaMatrix();

    // Don't copy it if it was already saved, we would have copied an altered userMatrix.
    if (ctxI->contextFlags & BL_RASTER_CONTEXT_STATE_USER_MATRIX)
      state->userMatrix = ctxI->userMatrix();
  }

  ctxI->contextFlags &= ~(kUserAndMetaFlags | BL_RASTER_CONTEXT_SHARED_STROKE_EXT_STATE);
  ctxI->internalState.userMatrix.reset();
  ctxI->internalState.metaMatrix = ctxI->finalMatrix();
  ctxI->internalState.metaMatrixFixed = ctxI->finalMatrixFixed();
  ctxI->internalState.metaMatrixType = ctxI->finalMatrixType();
  ctxI->internalState.metaMatrixFixedType = ctxI->finalMatrixFixedType();

  return BL_SUCCESS;
}

// BLRasterEngine - ContextImpl - Rendering Hints
// ==============================================

static BLResult BL_CDECL blRasterContextImplSetHint(BLContextImpl* baseImpl, BLContextHint hintType, uint32_t value) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  switch (hintType) {
    case BL_CONTEXT_HINT_RENDERING_QUALITY:
      if (BL_UNLIKELY(value > BL_RENDERING_QUALITY_MAX_VALUE))
        return blTraceError(BL_ERROR_INVALID_VALUE);

      ctxI->internalState.hints.renderingQuality = uint8_t(value);
      return BL_SUCCESS;

    case BL_CONTEXT_HINT_GRADIENT_QUALITY:
      if (BL_UNLIKELY(value > BL_GRADIENT_QUALITY_MAX_VALUE))
        return blTraceError(BL_ERROR_INVALID_VALUE);

      ctxI->internalState.hints.gradientQuality = uint8_t(value);
      return BL_SUCCESS;

    case BL_CONTEXT_HINT_PATTERN_QUALITY:
      if (BL_UNLIKELY(value > BL_PATTERN_QUALITY_MAX_VALUE))
        return blTraceError(BL_ERROR_INVALID_VALUE);

      ctxI->internalState.hints.patternQuality = uint8_t(value);
      return BL_SUCCESS;

    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }
}

static BLResult BL_CDECL blRasterContextImplSetHints(BLContextImpl* baseImpl, const BLContextHints* hints) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  uint8_t renderingQuality = hints->renderingQuality;
  uint8_t patternQuality = hints->patternQuality;
  uint8_t gradientQuality = hints->gradientQuality;

  if (BL_UNLIKELY(renderingQuality > BL_RENDERING_QUALITY_MAX_VALUE ||
                  patternQuality   > BL_PATTERN_QUALITY_MAX_VALUE   ||
                  gradientQuality  > BL_GRADIENT_QUALITY_MAX_VALUE  ))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  ctxI->internalState.hints.renderingQuality = renderingQuality;
  ctxI->internalState.hints.patternQuality = patternQuality;
  ctxI->internalState.hints.gradientQuality = gradientQuality;
  return BL_SUCCESS;
}

// BLRasterEngine - ContextImpl - Approximation Options
// ====================================================

static BLResult BL_CDECL blRasterContextImplSetFlattenMode(BLContextImpl* baseImpl, BLFlattenMode mode) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(uint32_t(mode) > BL_FLATTEN_MODE_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeConfigChange(ctxI);
  ctxI->contextFlags &= ~BL_RASTER_CONTEXT_STATE_CONFIG;

  ctxI->internalState.approximationOptions.flattenMode = uint8_t(mode);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetFlattenTolerance(BLContextImpl* baseImpl, double tolerance) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(blIsNaN(tolerance)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeConfigChange(ctxI);
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_STATE_CONFIG     |
                          BL_RASTER_CONTEXT_SHARED_FILL_STATE);

  tolerance = blClamp(tolerance, BLContextPrivate::kMinimumTolerance, BLContextPrivate::kMaximumTolerance);
  BL_ASSERT(blIsFinite(tolerance));

  ctxI->internalState.approximationOptions.flattenTolerance = tolerance;
  onAfterFlattenToleranceChanged(ctxI);

  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetApproximationOptions(BLContextImpl* baseImpl, const BLApproximationOptions* options) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  uint32_t flattenMode = options->flattenMode;
  uint32_t offsetMode = options->offsetMode;

  double flattenTolerance = options->flattenTolerance;
  double offsetParameter = options->offsetParameter;

  if (BL_UNLIKELY(flattenMode > BL_FLATTEN_MODE_MAX_VALUE ||
                  offsetMode > BL_OFFSET_MODE_MAX_VALUE ||
                  blIsNaN(flattenTolerance) ||
                  blIsNaN(offsetParameter)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeConfigChange(ctxI);
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_STATE_CONFIG     |
                          BL_RASTER_CONTEXT_SHARED_FILL_STATE);

  BLApproximationOptions& dst = ctxI->internalState.approximationOptions;
  dst.flattenMode = uint8_t(flattenMode);
  dst.offsetMode = uint8_t(offsetMode);
  dst.flattenTolerance = blClamp(flattenTolerance, BLContextPrivate::kMinimumTolerance, BLContextPrivate::kMaximumTolerance);
  dst.offsetParameter = offsetParameter;

  onAfterFlattenToleranceChanged(ctxI);
  onAfterOffsetParameterChanged(ctxI);

  return BL_SUCCESS;
}

// BLRasterEngine - ContextImpl - Compositing Options
// ==================================================

static BLResult BL_CDECL blRasterContextImplSetCompOp(BLContextImpl* baseImpl, BLCompOp compOp) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(uint32_t(compOp) > BL_COMP_OP_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  ctxI->internalState.compOp = uint8_t(compOp);
  onAfterCompOpChanged(ctxI);

  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetGlobalAlpha(BLContextImpl* baseImpl, double alpha) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(blIsNaN(alpha)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  alpha = blClamp(alpha, 0.0, 1.0);

  double intAlphaD = alpha * ctxI->fullAlphaD();
  double fillAlphaD = intAlphaD * ctxI->internalState.styleAlpha[BL_CONTEXT_OP_TYPE_FILL];
  double strokeAlphaD = intAlphaD * ctxI->internalState.styleAlpha[BL_CONTEXT_OP_TYPE_STROKE];

  uint32_t globalAlphaI = uint32_t(blRoundToInt(intAlphaD));
  uint32_t fillAlphaI = uint32_t(blRoundToInt(fillAlphaD));
  uint32_t strokeAlphaI = uint32_t(blRoundToInt(strokeAlphaD));

  ctxI->internalState.globalAlpha = alpha;
  ctxI->internalState.globalAlphaI = globalAlphaI;

  ctxI->internalState.style[BL_CONTEXT_OP_TYPE_FILL].alphaI = fillAlphaI;
  ctxI->internalState.style[BL_CONTEXT_OP_TYPE_STROKE].alphaI = strokeAlphaI;

  uint32_t contextFlags = ctxI->contextFlags;
  contextFlags &= ~(BL_RASTER_CONTEXT_NO_GLOBAL_ALPHA |
                    BL_RASTER_CONTEXT_NO_FILL_ALPHA   |
                    BL_RASTER_CONTEXT_NO_STROKE_ALPHA );

  if (!globalAlphaI) contextFlags |= BL_RASTER_CONTEXT_NO_GLOBAL_ALPHA;
  if (!fillAlphaI  ) contextFlags |= BL_RASTER_CONTEXT_NO_FILL_ALPHA;
  if (!strokeAlphaI) contextFlags |= BL_RASTER_CONTEXT_NO_STROKE_ALPHA;

  ctxI->contextFlags = contextFlags;
  return BL_SUCCESS;
}

// BLRasterEngine - ContextImpl - Fill Options
// ===========================================

static BLResult BL_CDECL blRasterContextImplSetFillRule(BLContextImpl* baseImpl, BLFillRule fillRule) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(uint32_t(fillRule) > BL_FILL_RULE_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  ctxI->internalState.fillRule = uint8_t(fillRule);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetFillAlpha(BLContextImpl* baseImpl, double alpha) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(blIsNaN(alpha)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  alpha = blClamp(alpha, 0.0, 1.0);

  uint32_t alphaI = uint32_t(blRoundToInt(ctxI->globalAlphaD() * ctxI->fullAlphaD() * alpha));
  ctxI->internalState.styleAlpha[BL_CONTEXT_OP_TYPE_FILL] = alpha;
  ctxI->internalState.style[BL_CONTEXT_OP_TYPE_FILL].alphaI = alphaI;

  uint32_t contextFlags = ctxI->contextFlags & ~BL_RASTER_CONTEXT_NO_FILL_ALPHA;
  if (!alphaI) contextFlags |= BL_RASTER_CONTEXT_NO_FILL_ALPHA;

  ctxI->contextFlags = contextFlags;
  return BL_SUCCESS;
}

// BLRasterEngine - ContextImpl - Stroke Options
// =============================================

static BLResult BL_CDECL blRasterContextImplSetStrokeWidth(BLContextImpl* baseImpl, double width) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_NO_STROKE_OPTIONS    |
                          BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS |
                          BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE);

  ctxI->internalState.strokeOptions.width = width;
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeMiterLimit(BLContextImpl* baseImpl, double miterLimit) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_NO_STROKE_OPTIONS    |
                          BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS |
                          BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE);

  ctxI->internalState.strokeOptions.miterLimit = miterLimit;
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeCap(BLContextImpl* baseImpl, BLStrokeCapPosition position, BLStrokeCap strokeCap) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(uint32_t(position) > BL_STROKE_CAP_POSITION_MAX_VALUE ||
                  uint32_t(strokeCap) > BL_STROKE_CAP_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE;

  ctxI->internalState.strokeOptions.caps[position] = uint8_t(strokeCap);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeCaps(BLContextImpl* baseImpl, BLStrokeCap strokeCap) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(uint32_t(strokeCap) > BL_STROKE_CAP_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE;

  for (uint32_t i = 0; i <= BL_STROKE_CAP_POSITION_MAX_VALUE; i++)
    ctxI->internalState.strokeOptions.caps[i] = uint8_t(strokeCap);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeJoin(BLContextImpl* baseImpl, BLStrokeJoin strokeJoin) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(uint32_t(strokeJoin) > BL_STROKE_JOIN_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE;

  ctxI->internalState.strokeOptions.join = uint8_t(strokeJoin);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeDashOffset(BLContextImpl* baseImpl, double dashOffset) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_NO_STROKE_OPTIONS    |
                          BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS |
                          BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE);

  ctxI->internalState.strokeOptions.dashOffset = dashOffset;
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeDashArray(BLContextImpl* baseImpl, const BLArrayCore* dashArray) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(dashArray->_d.rawType() != BL_OBJECT_TYPE_ARRAY_FLOAT64))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_NO_STROKE_OPTIONS    |
                          BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS |
                          BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE);

  ctxI->internalState.strokeOptions.dashArray = dashArray->dcast<BLArray<double>>();
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeTransformOrder(BLContextImpl* baseImpl, BLStrokeTransformOrder transformOrder) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(uint32_t(transformOrder) > BL_STROKE_TRANSFORM_ORDER_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS |
                          BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE);

  ctxI->internalState.strokeOptions.transformOrder = uint8_t(transformOrder);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeOptions(BLContextImpl* baseImpl, const BLStrokeOptionsCore* options) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(options->startCap > BL_STROKE_CAP_MAX_VALUE ||
                  options->endCap > BL_STROKE_CAP_MAX_VALUE ||
                  options->join > BL_STROKE_JOIN_MAX_VALUE ||
                  options->transformOrder > BL_STROKE_TRANSFORM_ORDER_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_NO_STROKE_OPTIONS    |
                          BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS |
                          BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE);
  return blStrokeOptionsAssignWeak(&ctxI->internalState.strokeOptions, options);
}

static BLResult BL_CDECL blRasterContextImplSetStrokeAlpha(BLContextImpl* baseImpl, double alpha) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(blIsNaN(alpha)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  alpha = blClamp(alpha, 0.0, 1.0);

  uint32_t alphaI = uint32_t(blRoundToInt(ctxI->globalAlphaD() * ctxI->fullAlphaD() * alpha));
  ctxI->internalState.styleAlpha[BL_CONTEXT_OP_TYPE_STROKE] = alpha;
  ctxI->internalState.style[BL_CONTEXT_OP_TYPE_STROKE].alphaI = alphaI;

  uint32_t contextFlags = ctxI->contextFlags & ~BL_RASTER_CONTEXT_NO_STROKE_ALPHA;
  if (!alphaI)
    contextFlags |= BL_RASTER_CONTEXT_NO_STROKE_ALPHA;

  ctxI->contextFlags = contextFlags;
  return BL_SUCCESS;
}

// BLRasterEngine - ContextImpl - Clip Operations
// ==============================================

static BLResult clipToFinalBox(BLRasterContextImpl* ctxI, const BLBox& inputBox) noexcept {
  BLBox b;
  onBeforeClipBoxChange(ctxI);

  if (BLGeometry::intersect(b, ctxI->finalClipBoxD(), inputBox)) {
    int fpMaskI = ctxI->renderTargetInfo.fpMaskI;
    int fpShiftI = ctxI->renderTargetInfo.fpShiftI;

    ctxI->setFinalClipBoxFixedD(b * ctxI->fpScaleD());
    const BLBoxI& clipBoxFixedI = ctxI->finalClipBoxFixedI();

    ctxI->internalState.finalClipBoxD = b;
    ctxI->internalState.finalClipBoxI.reset((clipBoxFixedI.x0 >> fpShiftI),
                                            (clipBoxFixedI.y0 >> fpShiftI),
                                            (clipBoxFixedI.x1 + fpMaskI) >> fpShiftI,
                                            (clipBoxFixedI.y1 + fpMaskI) >> fpShiftI);

    uint32_t bits = clipBoxFixedI.x0 |
                    clipBoxFixedI.y0 |
                    clipBoxFixedI.x1 |
                    clipBoxFixedI.y1 ;

    if ((bits & fpMaskI) == 0)
      ctxI->syncWorkData.clipMode = BL_CLIP_MODE_ALIGNED_RECT;
    else
      ctxI->syncWorkData.clipMode = BL_CLIP_MODE_UNALIGNED_RECT;
  }
  else {
    ctxI->internalState.finalClipBoxD.reset();
    ctxI->internalState.finalClipBoxI.reset();
    ctxI->setFinalClipBoxFixedD(BLBox(0, 0, 0, 0));
    ctxI->contextFlags |= BL_RASTER_CONTEXT_NO_CLIP_RECT;
    ctxI->syncWorkData.clipMode = BL_CLIP_MODE_ALIGNED_RECT;
  }

  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_STATE_CLIP | BL_RASTER_CONTEXT_SHARED_FILL_STATE);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplClipToRectD(BLContextImpl* baseImpl, const BLRect* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  // TODO: [Rendering Context] Path-based clipping.
  BLBox inputBox = BLBox(rect->x, rect->y, rect->x + rect->w, rect->y + rect->h);
  return clipToFinalBox(ctxI, BLTransformPrivate::mapBox(ctxI->finalMatrix(), inputBox));
}

static BLResult BL_CDECL blRasterContextImplClipToRectI(BLContextImpl* baseImpl, const BLRectI* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  // Don't bother if the current ClipBox is not aligned or the translation is not integral.
  if (ctxI->syncWorkData.clipMode != BL_CLIP_MODE_ALIGNED_RECT || !(ctxI->contextFlags & BL_RASTER_CONTEXT_INTEGRAL_TRANSLATION)) {
    BLRect rectD;
    rectD.x = double(rect->x);
    rectD.y = double(rect->y);
    rectD.w = double(rect->w);
    rectD.h = double(rect->h);
    return blRasterContextImplClipToRectD(ctxI, &rectD);
  }

  BLBoxI b;
  onBeforeClipBoxChange(ctxI);

  int tx = ctxI->translationI().x;
  int ty = ctxI->translationI().y;

  if (BL_TARGET_ARCH_BITS < 64) {
    BLOverflowFlag of = 0;

    int x0 = BLIntOps::addOverflow(tx, rect->x, &of);
    int y0 = BLIntOps::addOverflow(ty, rect->y, &of);
    int x1 = BLIntOps::addOverflow(x0, rect->w, &of);
    int y1 = BLIntOps::addOverflow(y0, rect->h, &of);

    if (BL_UNLIKELY(of))
      goto Use64Bit;

    b.x0 = blMax(x0, ctxI->finalClipBoxI().x0);
    b.y0 = blMax(y0, ctxI->finalClipBoxI().y0);
    b.x1 = blMin(x1, ctxI->finalClipBoxI().x1);
    b.y1 = blMin(y1, ctxI->finalClipBoxI().y1);
  }
  else {
Use64Bit:
    // We don't have to worry about overflow in 64-bit mode.
    int64_t x0 = tx + int64_t(rect->x);
    int64_t y0 = ty + int64_t(rect->y);
    int64_t x1 = x0 + int64_t(rect->w);
    int64_t y1 = y0 + int64_t(rect->h);

    b.x0 = int(blMax<int64_t>(x0, ctxI->finalClipBoxI().x0));
    b.y0 = int(blMax<int64_t>(y0, ctxI->finalClipBoxI().y0));
    b.x1 = int(blMin<int64_t>(x1, ctxI->finalClipBoxI().x1));
    b.y1 = int(blMin<int64_t>(y1, ctxI->finalClipBoxI().y1));
  }

  if (b.x0 < b.x1 && b.y0 < b.y1) {
    ctxI->internalState.finalClipBoxI = b;
    ctxI->internalState.finalClipBoxD.reset(b);
    ctxI->setFinalClipBoxFixedD(ctxI->finalClipBoxD() * ctxI->fpScaleD());
  }
  else {
    ctxI->internalState.finalClipBoxI.reset();
    ctxI->internalState.finalClipBoxD.reset(b);
    ctxI->setFinalClipBoxFixedD(BLBox(0, 0, 0, 0));
    ctxI->contextFlags |= BL_RASTER_CONTEXT_NO_CLIP_RECT;
  }

  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_STATE_CLIP | BL_RASTER_CONTEXT_SHARED_FILL_STATE);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplRestoreClipping(BLContextImpl* baseImpl) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  SavedState* state = ctxI->savedState;

  if (!(ctxI->contextFlags & BL_RASTER_CONTEXT_STATE_CLIP)) {
    if (state) {
      restoreClippingFromState(ctxI, state);
      ctxI->syncWorkData.clipMode = state->clipMode;
      ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_NO_CLIP_RECT     |
                              BL_RASTER_CONTEXT_STATE_CLIP       |
                              BL_RASTER_CONTEXT_SHARED_FILL_STATE);
      ctxI->contextFlags |= (state->prevContextFlags & BL_RASTER_CONTEXT_NO_CLIP_RECT);
    }
    else {
      // If there is no state saved it means that we have to restore clipping to
      // the initial state, which is accessible through `metaClipBoxI` member.
      ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_NO_CLIP_RECT     |
                              BL_RASTER_CONTEXT_SHARED_FILL_STATE);
      resetClippingToMetaClipBox(ctxI);
    }
  }

  return BL_SUCCESS;
}

// BLRasterEngine - ContextImpl - Clip Utilities
// =============================================

static BL_INLINE bool clipFillRectI(const BLRasterContextImpl* ctxI, const BLRectI* srcRect, BLBoxI* dstBoxOut) noexcept {
  int rx = srcRect->x;
  int ry = srcRect->y;
  int rw = srcRect->w;
  int rh = srcRect->h;

  if (BL_TARGET_ARCH_BITS < 64) {
    BLOverflowFlag of = 0;

    int x0 = BLIntOps::addOverflow(rx, ctxI->translationI().x, &of);
    int y0 = BLIntOps::addOverflow(ry, ctxI->translationI().y, &of);
    int x1 = BLIntOps::addOverflow(rw, x0, &of);
    int y1 = BLIntOps::addOverflow(rh, y0, &of);

    if (BL_UNLIKELY(of))
      goto Use64Bit;

    x0 = blMax(x0, ctxI->finalClipBoxI().x0);
    y0 = blMax(y0, ctxI->finalClipBoxI().y0);
    x1 = blMin(x1, ctxI->finalClipBoxI().x1);
    y1 = blMin(y1, ctxI->finalClipBoxI().y1);

    // Clipped out or invalid rect.
    if (BL_UNLIKELY((x0 >= x1) | (y0 >= y1)))
      return false;

    dstBoxOut->reset(int(x0), int(y0), int(x1), int(y1));
    return true;
  }
  else {
Use64Bit:
    int64_t x0 = int64_t(rx) + int64_t(ctxI->translationI().x);
    int64_t y0 = int64_t(ry) + int64_t(ctxI->translationI().y);
    int64_t x1 = int64_t(rw) + x0;
    int64_t y1 = int64_t(rh) + y0;

    x0 = blMax<int64_t>(x0, ctxI->finalClipBoxI().x0);
    y0 = blMax<int64_t>(y0, ctxI->finalClipBoxI().y0);
    x1 = blMin<int64_t>(x1, ctxI->finalClipBoxI().x1);
    y1 = blMin<int64_t>(y1, ctxI->finalClipBoxI().y1);

    // Clipped out or invalid rect.
    if (BL_UNLIKELY((x0 >= x1) | (y0 >= y1)))
      return false;

    dstBoxOut->reset(int(x0), int(y0), int(x1), int(y1));
    return true;
  }
}

static BL_INLINE bool clipBlitRectI(const BLRasterContextImpl* ctxI, const BLPointI* pt, const BLRectI* area, const BLSizeI* sz, BLResult* resultOut, BLBoxI* dstBoxOut, BLPointI* srcOffsetOut) noexcept {
  BLSizeI size(sz->w, sz->h);
  srcOffsetOut->reset();

  if (area) {
    unsigned maxW = unsigned(size.w) - unsigned(area->x);
    unsigned maxH = unsigned(size.h) - unsigned(area->y);

    if (BL_UNLIKELY((maxW > unsigned(size.w)) | (unsigned(area->w) > maxW) |
                    (maxH > unsigned(size.h)) | (unsigned(area->h) > maxH))) {
      *resultOut = blTraceError(BL_ERROR_INVALID_VALUE);
      return false;
    }

    srcOffsetOut->reset(area->x, area->y);
    size.reset(area->w, area->h);
  }

  *resultOut = BL_SUCCESS;
  if (BL_TARGET_ARCH_BITS < 64) {
    BLOverflowFlag of = 0;

    int dx = BLIntOps::addOverflow(pt->x, ctxI->translationI().x, &of);
    int dy = BLIntOps::addOverflow(pt->y, ctxI->translationI().y, &of);

    int x0 = dx;
    int y0 = dy;
    int x1 = BLIntOps::addOverflow(x0, size.w, &of);
    int y1 = BLIntOps::addOverflow(y0, size.h, &of);

    if (BL_UNLIKELY(of))
      goto Use64Bit;

    x0 = blMax(x0, ctxI->finalClipBoxI().x0);
    y0 = blMax(y0, ctxI->finalClipBoxI().y0);
    x1 = blMin(x1, ctxI->finalClipBoxI().x1);
    y1 = blMin(y1, ctxI->finalClipBoxI().y1);

    // Clipped out.
    if ((x0 >= x1) | (y0 >= y1))
      return false;

    dstBoxOut->reset(x0, y0, x1, y1);
    srcOffsetOut->x += x0 - dx;
    srcOffsetOut->y += y0 - dy;
    return true;
  }
  else {
Use64Bit:
    int64_t dx = int64_t(pt->x) + ctxI->translationI().x;
    int64_t dy = int64_t(pt->y) + ctxI->translationI().y;

    int64_t x0 = dx;
    int64_t y0 = dy;
    int64_t x1 = x0 + int64_t(unsigned(size.w));
    int64_t y1 = y0 + int64_t(unsigned(size.h));

    x0 = blMax<int64_t>(x0, ctxI->finalClipBoxI().x0);
    y0 = blMax<int64_t>(y0, ctxI->finalClipBoxI().y0);
    x1 = blMin<int64_t>(x1, ctxI->finalClipBoxI().x1);
    y1 = blMin<int64_t>(y1, ctxI->finalClipBoxI().y1);

    // Clipped out.
    if ((x0 >= x1) | (y0 >= y1))
      return false;

    dstBoxOut->reset(int(x0), int(y0), int(x1), int(y1));
    srcOffsetOut->x += int(x0 - dx);
    srcOffsetOut->y += int(y0 - dy);
    return true;
  }
}

// BLRasterEngine - ContextImpl - Mask & Blit Utilities
// ====================================================

static BL_INLINE BLResult checkImageArea(BLRectI& out, const BLImageImpl* image, const BLRectI* area) noexcept {
  out.reset(0, 0, image->size.w, image->size.h);

  if (area) {
    unsigned maxW = unsigned(out.w) - unsigned(area->x);
    unsigned maxH = unsigned(out.h) - unsigned(area->y);

    if ((maxW > unsigned(out.w)) | (unsigned(area->w) > maxW) |
        (maxH > unsigned(out.h)) | (unsigned(area->h) > maxH))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    out = *area;
  }

  return BL_SUCCESS;
}

// BLRasterEngine - ContextImpl - Synchronous Rendering - Process Command
// ======================================================================

// Wrappers responsible for getting the pipeline and then calling a real command processor with the command data
// held by the command serializer.

static BL_INLINE BLResult processFillBoxA(BLRasterContextImpl* ctxI, RenderCommandSerializerCoreSync& serializer) noexcept {
  BL_PROPAGATE(ensureFillFunc(ctxI, serializer.pipeSignature().value, serializer.command().pipeDispatchData()));
  return CommandProcSync::fillBoxA(ctxI->syncWorkData, serializer.command());
}

static BL_INLINE BLResult processFillBoxU(BLRasterContextImpl* ctxI, RenderCommandSerializerCoreSync& serializer) noexcept {
  BL_PROPAGATE(ensureFillFunc(ctxI, serializer.pipeSignature().value, serializer.command().pipeDispatchData()));
  return CommandProcSync::fillBoxU(ctxI->syncWorkData, serializer.command());
}

static BL_INLINE BLResult processFillMaskA(BLRasterContextImpl* ctxI, RenderCommandSerializerCoreSync& serializer) noexcept {
  BL_PROPAGATE(ensureFillFunc(ctxI, serializer.pipeSignature().value, serializer.command().pipeDispatchData()));
  return CommandProcSync::fillMaskRaw(ctxI->syncWorkData, serializer.command());
}

static BL_INLINE BLResult processFillAnalytic(BLRasterContextImpl* ctxI, RenderCommandSerializerCoreSync& serializer) noexcept {
  BL_PROPAGATE(ensureFillFunc(ctxI, serializer.pipeSignature().value, serializer.command().pipeDispatchData()));
  return CommandProcSync::fillAnalytic(ctxI->syncWorkData, serializer.command());
}

// BLRasterEngine - ContextImpl - Synchronous Rendering - Fill Clipped Geometry
// ============================================================================

// Synchronous fills are called when a geometry has been transformed and fully clipped in a synchronous mode.
// The responsibility of these functions is to initialized the command data through serializer and then to call
// a command processor.

template<RenderCommandSerializerFlags Flags>
static BL_INLINE BLResult fillClippedBoxA(BLRasterContextImpl* ctxI, RenderCommandSerializerCoreSync& serializer, const BLBoxI& boxA) noexcept {
  serializer.initFillBoxA(boxA);
  if (!ensureFetchData<Flags>(ctxI, serializer))
    return BL_SUCCESS;
  return processFillBoxA(ctxI, serializer);
}

template<RenderCommandSerializerFlags Flags>
static BL_INLINE BLResult fillClippedBoxU(BLRasterContextImpl* ctxI, RenderCommandSerializerCoreSync& serializer, const BLBoxI& boxU) noexcept {
  if (!ensureFetchData<Flags>(ctxI, serializer))
    return BL_SUCCESS;

  if (isBoxAligned24x8(boxU)) {
    serializer.initFillBoxA(BLBoxI(boxU.x0 >> 8, boxU.y0 >> 8, boxU.x1 >> 8, boxU.y1 >> 8));
    return processFillBoxA(ctxI, serializer);
  }
  else {
    serializer.initFillBoxU(boxU);
    return processFillBoxU(ctxI, serializer);
  }
}

template<RenderCommandSerializerFlags Flags>
static BL_INLINE BLResult fillClippedEdges(BLRasterContextImpl* ctxI, RenderCommandSerializerCoreSync& serializer, uint32_t fillRule) noexcept {
  WorkData* workData = &ctxI->syncWorkData;

  // No data or everything was clipped out (no edges at all). We really want to check this now as it could be
  // a waste of resources to try to ensure fetch-data if the command doesn't renderer anything.
  if (workData->edgeStorage.empty())
    return BL_SUCCESS;

  if (!ensureFetchData<Flags>(ctxI, serializer)) {
    workData->revertEdgeBuilder();
    return BL_SUCCESS;
  }

  serializer.initFillAnalyticSync(fillRule, &workData->edgeStorage);
  return processFillAnalytic(ctxI, serializer);
}

// BLRasterEngine - ContextImpl - Asynchronous Rendering - Shared State
// ====================================================================

static BL_INLINE SharedFillState* ensureFillState(BLRasterContextImpl* ctxI) noexcept {
  SharedFillState* sharedFillState = ctxI->sharedFillState;

  if (!(ctxI->contextFlags & BL_RASTER_CONTEXT_SHARED_FILL_STATE)) {
    sharedFillState = ctxI->workerMgr()._allocator.allocNoAlignT<SharedFillState>(
      BLIntOps::alignUp(sizeof(SharedFillState), WorkerManager::kAllocatorAlignment));

    if (BL_UNLIKELY(!sharedFillState))
      return nullptr;

    sharedFillState->finalClipBoxFixedD = ctxI->finalClipBoxFixedD();
    sharedFillState->finalMatrixFixed = ctxI->finalMatrixFixed();
    sharedFillState->toleranceFixedD = ctxI->internalState.toleranceFixedD;

    ctxI->sharedFillState = sharedFillState;
    ctxI->contextFlags |= BL_RASTER_CONTEXT_SHARED_FILL_STATE;
  }

  return sharedFillState;
}

static const uint32_t sharedStrokeStateFlagsTable[BL_STROKE_TRANSFORM_ORDER_MAX_VALUE + 1] = {
  BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE,
  BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE | BL_RASTER_CONTEXT_SHARED_STROKE_EXT_STATE
};

static const uint32_t sharedStrokeStateSizeTable[BL_STROKE_TRANSFORM_ORDER_MAX_VALUE + 1] = {
  uint32_t(BLIntOps::alignUp(sizeof(SharedBaseStrokeState), WorkerManager::kAllocatorAlignment)),
  uint32_t(BLIntOps::alignUp(sizeof(SharedExtendedStrokeState), WorkerManager::kAllocatorAlignment))
};

static BL_INLINE SharedBaseStrokeState* ensureStrokeState(BLRasterContextImpl* ctxI) noexcept {
  SharedBaseStrokeState* sharedStrokeState = ctxI->sharedStrokeState;

  BLStrokeTransformOrder transformOrder = (BLStrokeTransformOrder)ctxI->strokeOptions().transformOrder;
  uint32_t sharedFlags = sharedStrokeStateFlagsTable[transformOrder];

  if ((ctxI->contextFlags & sharedFlags) != sharedFlags) {
    size_t stateSize = sharedStrokeStateSizeTable[transformOrder];
    sharedStrokeState = ctxI->workerMgr()._allocator.allocNoAlignT<SharedBaseStrokeState>(stateSize);

    if (BL_UNLIKELY(!sharedStrokeState))
      return nullptr;

    blCallCtor(*sharedStrokeState, ctxI->strokeOptions(), ctxI->approximationOptions());

    if (transformOrder != BL_STROKE_TRANSFORM_ORDER_AFTER) {
      static_cast<SharedExtendedStrokeState*>(sharedStrokeState)->userMatrix = ctxI->userMatrix();
      static_cast<SharedExtendedStrokeState*>(sharedStrokeState)->metaMatrixFixed = ctxI->metaMatrixFixed();
    }

    ctxI->sharedStrokeState = sharedStrokeState;
    ctxI->contextFlags |= sharedFlags;
  }

  return sharedStrokeState;
}

// BLRasterEngine - ContextImpl - Asynchronous Rendering - Enqueue Command
// =======================================================================

static void BL_CDECL destroyAsyncBlitData(BLRasterContextImpl* ctxI, RenderFetchData* fetchData) noexcept {
  blUnused(ctxI);
  BLImagePrivate::releaseInstance(static_cast<BLImageCore*>(&fetchData->_style));
}

template<RenderCommandSerializerFlags Flags, typename CommandFinalizer>
static BL_INLINE BLResult enqueueCommand(BLRasterContextImpl* ctxI, RenderCommand& command, const CommandFinalizer& commandFinalizer) noexcept {
  WorkerManager& mgr = ctxI->workerMgr();

  // TODO: [Rendering Context] Masking support.

  if (command.hasFetchData()) {
    RenderFetchData* fetchData = command._source.fetchData;
    if (fetchData->_batchId != mgr.currentBatchId()) {
      BL_PROPAGATE(mgr.ensureFetchQueue());
      fetchData->_batchId = mgr.currentBatchId();

      // FetchData used by blits is not managed by the context's state management - it's allocated via allocator.
      if (blTestFlag(Flags, RenderCommandSerializerFlags::kBlit)) {
        mgr._fetchDataAppender.append(fetchData);
        fetchData->_destroyFunc = destroyAsyncBlitData;
        blObjectPrivateAddRefTagged(&fetchData->_style);
      }
      else {
        mgr._fetchDataAppender.append(fetchData->addRef());
      }
    }
  }

  commandFinalizer(command);
  mgr._commandAppender.advance();
  return BL_SUCCESS;
}

template<RenderCommandSerializerFlags Flags>
static BL_INLINE BLResult enqueueFillBox(BLRasterContextImpl* ctxI, RenderCommandSerializerCoreAsync& serializer) noexcept {
  BL_PROPAGATE(ensureFillFunc(ctxI, serializer.pipeSignature().value, serializer.command().pipeDispatchData()));
  return enqueueCommand<Flags>(ctxI, serializer.command(), [&](RenderCommand&) {});
}

template<RenderCommandSerializerFlags Flags>
static BL_INLINE BLResult enqueueFillAnalytic(BLRasterContextImpl* ctxI, RenderCommandSerializerCoreAsync& serializer) noexcept {
  BL_PROPAGATE(ensureFillFunc(ctxI, serializer.pipeSignature().value, serializer.command().pipeDispatchData()));
  return enqueueCommand<Flags>(ctxI, serializer.command(), [&](RenderCommand& command) {
    command._payload.analyticAsync.stateSlotIndex = ctxI->workerMgr().nextStateSlotIndex();
  });
}

template<RenderCommandSerializerFlags Flags>
static BL_INLINE BLResult enqueueFillMaskA(BLRasterContextImpl* ctxI, RenderCommandSerializerMaskAsync& serializer, const BLImageCore* mask) noexcept {
  BL_PROPAGATE(ensureFillFunc(ctxI, serializer.pipeSignature().value, serializer.command().pipeDispatchData()));
  return enqueueCommand<Flags>(ctxI, serializer.command(), [&](RenderCommand& command) {
    ctxI->workerMgr->_imageAppender.append(*mask);
    blObjectImplAddRef(command._payload.maskRaw.maskImageI);
  });
}

// BLRasterEngine - ContextImpl - FetchData Utilities
// ==================================================

static BL_INLINE BLResult blRasterContextImplFinalizeBlit(BLRasterContextImpl* ctxI, RenderCommandSerializerBlitSync& serializer, BLResult result) noexcept {
  blUnused(ctxI, serializer);
  return result;
}

static BL_INLINE BLResult blRasterContextImplFinalizeBlit(BLRasterContextImpl* ctxI, RenderCommandSerializerBlitAsync& serializer, BLResult result) noexcept {
  if (!serializer.enqueued(ctxI))
    serializer.rollbackFetchData(ctxI);
  return result;
}

static BL_INLINE BLResult blRasterContextImplFinalizeBlit(BLRasterContextImpl* ctxI, RenderCommandSerializerMaskSync& serializer, BLResult result) noexcept {
  blUnused(ctxI, serializer);
  return result;
}

static BL_INLINE BLResult blRasterContextImplFinalizeBlit(BLRasterContextImpl* ctxI, RenderCommandSerializerMaskAsync& serializer, BLResult result) noexcept {
  // TODO: [Rendering Context] Masking support.
  blUnused(ctxI, serializer);
  return result;
}

// BLRasterEngine - ContextImpl - Asynchronous Rendering - Enqueue Command With Job
// ================================================================================

template<typename JobType>
static BL_INLINE BLResult newFillJob(BLRasterContextImpl* ctxI, size_t jobDataSize, JobType** out) noexcept {
  BL_PROPAGATE(ctxI->workerMgr().ensureJobQueue());

  SharedFillState* fillState = ensureFillState(ctxI);
  if (BL_UNLIKELY(!fillState))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  JobType* job = ctxI->workerMgr()._allocator.template allocNoAlignT<JobType>(jobDataSize);
  if (BL_UNLIKELY(!job))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  job->initStates(fillState);
  *out = job;
  return BL_SUCCESS;
}

template<typename JobType>
static BL_INLINE BLResult newStrokeJob(BLRasterContextImpl* ctxI, size_t jobDataSize, JobType** out) noexcept {
  BL_PROPAGATE(ctxI->workerMgr().ensureJobQueue());

  SharedFillState* fillState = ensureFillState(ctxI);
  if (BL_UNLIKELY(!fillState))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  SharedBaseStrokeState* strokeState = ensureStrokeState(ctxI);
  if (BL_UNLIKELY(!strokeState))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  JobType* job = ctxI->workerMgr()._allocator.template allocNoAlignT<JobType>(jobDataSize);
  if (BL_UNLIKELY(!job))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  job->initStates(fillState, strokeState);
  *out = job;
  return BL_SUCCESS;
}

template<RenderCommandSerializerFlags Flags, typename JobType, typename JobFinalizer>
static BL_INLINE BLResult enqueueCommandWithFillJob(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreAsync& serializer,
  size_t jobSize,
  const JobFinalizer& jobFinalizer) noexcept {

  JobType* job;
  BL_PROPAGATE(newFillJob(ctxI, jobSize, &job));

  if (!ensureFetchData<Flags>(ctxI, serializer)) {
    // TODO: [Rendering Context] Error handling.
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  BL_PROPAGATE(ensureFillFunc(ctxI, serializer.pipeSignature().value, serializer.command().pipeDispatchData()));
  return enqueueCommand<Flags>(ctxI, serializer.command(), [&](RenderCommand& command) {
    command._payload.analyticAsync.stateSlotIndex = ctxI->workerMgr().nextStateSlotIndex();
    job->initFillJob(serializer._command);
    job->setMetaMatrixFixedType(ctxI->metaMatrixFixedType());
    job->setFinalMatrixFixedType(ctxI->finalMatrixFixedType());
    jobFinalizer(job);
    ctxI->workerMgr().addJob(job);
  });
}

template<RenderCommandSerializerFlags Flags, typename JobType, typename JobFinalizer>
static BL_INLINE BLResult enqueueCommandWithStrokeJob(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreAsync& serializer,
  size_t jobSize,
  const JobFinalizer& jobFinalizer) noexcept {

  JobType* job;
  BL_PROPAGATE(newStrokeJob(ctxI, jobSize, &job));

  if (!ensureFetchData<Flags>(ctxI, serializer)) {
    // TODO: [Rendering Context] Error handling.
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  BL_PROPAGATE(ensureFillFunc(ctxI, serializer.pipeSignature().value, serializer.command().pipeDispatchData()));
  return enqueueCommand<Flags>(ctxI, serializer.command(), [&](RenderCommand& command) {
    command._payload.analyticAsync.stateSlotIndex = ctxI->workerMgr().nextStateSlotIndex();
    job->initStrokeJob(serializer._command);
    job->setMetaMatrixFixedType(ctxI->metaMatrixFixedType());
    job->setFinalMatrixFixedType(ctxI->finalMatrixFixedType());
    jobFinalizer(job);
    ctxI->workerMgr().addJob(job);
  });
}

template<RenderCommandSerializerFlags Flags, uint32_t OpType, typename JobType, typename JobFinalizer>
static BL_INLINE BLResult enqueueCommandWithJob(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreAsync& serializer,
  size_t jobSize,
  const JobFinalizer& jobFinalizer) noexcept {

  if (OpType == BL_CONTEXT_OP_TYPE_FILL)
    return enqueueCommandWithFillJob<Flags, JobType, JobFinalizer>(ctxI, serializer, jobSize, jobFinalizer);
  else
    return enqueueCommandWithStrokeJob<Flags, JobType, JobFinalizer>(ctxI, serializer, jobSize, jobFinalizer);
}

// BLRasterEngine - ContextImpl - Asynchronous Rendering - Enqueue GlyphRun & TextData
// ===================================================================================

struct BLGlyphPlacementRawData {
  uint64_t data[2];
};

BL_STATIC_ASSERT(sizeof(BLGlyphPlacementRawData) == sizeof(BLPoint));
BL_STATIC_ASSERT(sizeof(BLGlyphPlacementRawData) == sizeof(BLGlyphPlacement));

template<RenderCommandSerializerFlags Flags, uint32_t OpType>
static BL_INLINE BLResult enqueueFillOrStrokeGlyphRun(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreAsync& serializer,
  const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  size_t size = glyphRun->size;
  size_t glyphDataSize = BLIntOps::alignUp(size * sizeof(uint32_t), WorkerManager::kAllocatorAlignment);
  size_t placementDataSize = BLIntOps::alignUp(size * sizeof(BLGlyphPlacementRawData), WorkerManager::kAllocatorAlignment);

  uint32_t* glyphData = ctxI->workerMgr()._allocator.template allocNoAlignT<uint32_t>(glyphDataSize);
  BLGlyphPlacementRawData* placementData = ctxI->workerMgr()._allocator.template allocNoAlignT<BLGlyphPlacementRawData>(placementDataSize);

  if (BL_UNLIKELY(!glyphData || !placementData))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLGlyphRunIterator it(*glyphRun);
  uint32_t* dstGlyphData = glyphData;
  BLGlyphPlacementRawData* dstPlacementData = placementData;

  while (!it.atEnd()) {
    *dstGlyphData++ = it.glyphId();
    *dstPlacementData++ = it.placement<BLGlyphPlacementRawData>();
    it.advance();
  }

  serializer.initFillAnalyticAsync(BL_FILL_RULE_NON_ZERO, nullptr);
  return enqueueCommandWithJob<Flags, OpType, RenderJob_TextOp>(
    ctxI, serializer,
    BLIntOps::alignUp(sizeof(RenderJob_TextOp), WorkerManager::kAllocatorAlignment),
    [&](RenderJob_TextOp* job) {
      job->initCoordinates(*pt);
      job->initFont(*font);
      job->initGlyphRun(glyphData, placementData, size, glyphRun->placementType, glyphRun->flags);
    });
}

template<RenderCommandSerializerFlags Flags, uint32_t OpType>
static BL_INLINE BLResult enqueueFillOrStrokeText(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreAsync& serializer,
  const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, BLTextEncoding encoding) noexcept {

  if (size == SIZE_MAX)
    size = blStrLenWithEncoding(text, encoding);

  if (!size)
    return BL_SUCCESS;

  BLResult result = BL_SUCCESS;
  BLWrap<BLGlyphBuffer> gb;

  void* serializedTextData = nullptr;
  size_t serializedTextSize = size << blTextByteSizeShift[encoding];

  if (serializedTextSize > BL_RASTER_CONTEXT_MAXIMUM_EMBEDDED_TEXT_SIZE) {
    gb.init();
    result = gb->setText(text, size, encoding);
  }
  else {
    serializedTextData = ctxI->workerMgr->_allocator.alloc(BLIntOps::alignUp(serializedTextSize, 8));
    if (!serializedTextData)
      result = BL_ERROR_OUT_OF_MEMORY;
    else
      memcpy(serializedTextData, text, serializedTextSize);
  }

  if (result == BL_SUCCESS) {
    serializer.initFillAnalyticAsync(BL_FILL_RULE_NON_ZERO, nullptr);
    result = enqueueCommandWithJob<Flags, OpType, RenderJob_TextOp>(
      ctxI, serializer,
      BLIntOps::alignUp(sizeof(RenderJob_TextOp), WorkerManager::kAllocatorAlignment),
      [&](RenderJob_TextOp* job) {
        job->initCoordinates(*pt);
        job->initFont(*font);
        if (serializedTextSize > BL_RASTER_CONTEXT_MAXIMUM_EMBEDDED_TEXT_SIZE)
          job->initGlyphBuffer(gb->impl);
        else
          job->initTextData(serializedTextData, size, encoding);
      });
  }

  if (result != BL_SUCCESS && serializedTextSize > BL_RASTER_CONTEXT_MAXIMUM_EMBEDDED_TEXT_SIZE)
    gb.destroy();

  return result;
}

// BLRasterEngine - ContextImpl - Asynchronous Rendering - Fill Clipped Geometry
// =============================================================================

template<RenderCommandSerializerFlags Flags>
static BL_INLINE BLResult fillClippedBoxA(BLRasterContextImpl* ctxI, RenderCommandSerializerCoreAsync& serializer, const BLBoxI& boxA) noexcept {
  serializer.initFillBoxA(boxA);
  if (!ensureFetchData<Flags>(ctxI, serializer))
    return BL_SUCCESS;
  return enqueueFillBox<Flags>(ctxI, serializer);
}

template<RenderCommandSerializerFlags Flags>
static BL_INLINE BLResult fillClippedBoxU(BLRasterContextImpl* ctxI, RenderCommandSerializerCoreAsync& serializer, const BLBoxI& boxU) noexcept {
  if (isBoxAligned24x8(boxU))
    serializer.initFillBoxA(BLBoxI(boxU.x0 >> 8, boxU.y0 >> 8, boxU.x1 >> 8, boxU.y1 >> 8));
  else
    serializer.initFillBoxU(boxU);

  if (!ensureFetchData<Flags>(ctxI, serializer))
    return BL_SUCCESS;
  return enqueueFillBox<Flags>(ctxI, serializer);
}

template<RenderCommandSerializerFlags Flags>
static BL_INLINE BLResult fillClippedEdges(BLRasterContextImpl* ctxI, RenderCommandSerializerCoreAsync& serializer, uint32_t fillRule) noexcept {
  WorkData* workData = &ctxI->syncWorkData;

  // No data or everything was clipped out (no edges at all).
  if (workData->edgeStorage.empty())
    return BL_SUCCESS;

  if (!ensureFetchData<Flags>(ctxI, serializer)) {
    workData->revertEdgeBuilder();
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  serializer.initFillAnalyticAsync(fillRule, workData->edgeStorage.flattenEdgeLinks());
  workData->edgeStorage.resetBoundingBox();
  return enqueueFillAnalytic<Flags>(ctxI, serializer);
}

// BLRasterEngine - ContextImpl - Asynchronous Rendering - Fill & Stroke Unsafe Geometry
// =====================================================================================

template<RenderCommandSerializerFlags Flags>
static BL_INLINE BLResult fillUnsafeGeometry(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreAsync& serializer,
  uint32_t fillRule,
  BLGeometryType geometryType, const void* geometryData) noexcept {

  size_t geometrySize = sizeof(BLPathCore);
  if (BLGeometry::isSimpleGeometryType(geometryType))
    geometrySize = BLGeometry::blGeometryTypeSizeTable[geometryType];
  else
    BL_ASSERT(geometryType == BL_GEOMETRY_TYPE_PATH);

  serializer.initFillAnalyticAsync(fillRule, nullptr);
  return enqueueCommandWithFillJob<Flags, RenderJob_GeometryOp>(
    ctxI, serializer,
    sizeof(RenderJob_GeometryOp) + geometrySize,
    [&](RenderJob_GeometryOp* job) {
      job->setGeometry(geometryType, geometryData, geometrySize);
    });
}

template<RenderCommandSerializerFlags Flags>
static BL_INLINE BLResult strokeUnsafeGeometry(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreAsync& serializer,
  BLGeometryType geometryType, const void* geometryData) noexcept {

  serializer.initFillAnalyticAsync(BL_FILL_RULE_NON_ZERO, nullptr);

  size_t geometrySize = sizeof(BLPathCore);
  if (BLGeometry::isSimpleGeometryType(geometryType)) {
    geometrySize = BLGeometry::blGeometryTypeSizeTable[geometryType];
  }
  else if (geometryType != BL_GEOMETRY_TYPE_PATH) {
    BLPath* temporaryPath = &ctxI->syncWorkData.tmpPath[3];

    temporaryPath->clear();
    BL_PROPAGATE(temporaryPath->addGeometry(geometryType, geometryData));

    geometryType = BL_GEOMETRY_TYPE_PATH;
    geometryData = temporaryPath;
  }

  return enqueueCommandWithStrokeJob<Flags, RenderJob_GeometryOp>(
    ctxI, serializer,
    sizeof(RenderJob_GeometryOp) + geometrySize,
    [&](RenderJob_GeometryOp* job) {
      job->setGeometry(geometryType, geometryData, geometrySize);
    });
}

// BLRasterEngine - ContextImpl - Fill Internals - Fill Unsafe Poly
// ================================================================

template<RenderCommandSerializerFlags Flags, uint32_t RenderingMode, class PointType>
static BL_INLINE BLResult fillUnsafePolygon(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCore<RenderingMode>& serializer,
  uint32_t fillRule, const PointType* pts, size_t size, const BLMatrix2D& m, uint32_t mType) noexcept {

  BL_PROPAGATE(blRasterContextBuildPolyEdges(&ctxI->syncWorkData, pts, size, m, mType));
  return fillClippedEdges<Flags>(ctxI, serializer, fillRule);
}

template<RenderCommandSerializerFlags Flags, uint32_t RenderingMode, class PointType>
static BL_INLINE BLResult fillUnsafePolygon(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCore<RenderingMode>& serializer,
  uint32_t fillRule, const PointType* pts, size_t size) noexcept {

  return fillUnsafePolygon<Flags>(ctxI, serializer, fillRule, pts, size, ctxI->finalMatrixFixed(), ctxI->finalMatrixFixedType());
}

// BLRasterEngine - ContextImpl - Fill Internals - Fill Unsafe Path
// ================================================================

template<RenderCommandSerializerFlags Flags, uint32_t RenderingMode>
static BL_INLINE BLResult fillUnsafePath(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCore<RenderingMode>& serializer,
  uint32_t fillRule, const BLPath& path, const BLMatrix2D& m, uint32_t mType) noexcept {

  BL_PROPAGATE(blRasterContextBuildPathEdges(&ctxI->syncWorkData, path.view(), m, mType));
  return fillClippedEdges<Flags>(ctxI, serializer, fillRule);
}

template<RenderCommandSerializerFlags Flags, uint32_t RenderingMode>
static BL_INLINE BLResult fillUnsafePath(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCore<RenderingMode>& serializer,
  uint32_t fillRule, const BLPath& path) noexcept {

  return fillUnsafePath<Flags>(ctxI, serializer, fillRule, path, ctxI->finalMatrixFixed(), ctxI->finalMatrixFixedType());
}

// BLRasterEngine - ContextImpl - Fill Internals - Fill Unsafe Box
// ===============================================================

template<RenderCommandSerializerFlags Flags, uint32_t RenderingMode>
static BL_INLINE BLResult fillUnsafeBox(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCore<RenderingMode>& serializer,
  const BLBox& box, const BLMatrix2D& m, uint32_t mType) noexcept {

  if (mType <= BL_MATRIX2D_TYPE_SWAP) {
    BLBox finalBox;
    if (!BLGeometry::intersect(finalBox, BLTransformPrivate::mapBox(m, box), ctxI->finalClipBoxFixedD()))
      return BL_SUCCESS;

    BLBoxI boxU = blTruncToInt(finalBox);
    if (boxU.x0 >= boxU.x1 || boxU.y0 >= boxU.y1)
      return BL_SUCCESS;

    return fillClippedBoxU<Flags>(ctxI, serializer, boxU);
  }
  else {
    BLPoint polyD[] = {
      BLPoint(box.x0, box.y0),
      BLPoint(box.x1, box.y0),
      BLPoint(box.x1, box.y1),
      BLPoint(box.x0, box.y1)
    };
    return fillUnsafePolygon<Flags>(ctxI, serializer, BL_RASTER_CONTEXT_PREFERRED_FILL_RULE, polyD, BL_ARRAY_SIZE(polyD), m, mType);
  }
}

template<RenderCommandSerializerFlags Flags, uint32_t RenderingMode>
static BL_INLINE BLResult fillUnsafeBox(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCore<RenderingMode>& serializer,
  const BLBox& box) noexcept {

  return fillUnsafeBox<Flags>(ctxI, serializer, box, ctxI->finalMatrixFixed(), ctxI->finalMatrixFixedType());
}

// BLRasterEngine - ContextImpl - Fill Internals - Fill Unsafe Rect
// ================================================================

// Fully integer-based rectangle fill.
template<RenderCommandSerializerFlags Flags, uint32_t RenderingMode>
static BL_INLINE BLResult fillUnsafeRectI(BLRasterContextImpl* ctxI, RenderCommandSerializerCore<RenderingMode>& serializer, const BLRectI& rect) noexcept {
  int rw = rect.w;
  int rh = rect.h;

  if (!(ctxI->contextFlags & BL_RASTER_CONTEXT_INTEGRAL_TRANSLATION)) {
    // Clipped out.
    if ((rw <= 0) | (rh <= 0))
      return BL_SUCCESS;

    BLBox boxD(double(rect.x),
               double(rect.y),
               double(rect.x) + double(rect.w),
               double(rect.y) + double(rect.h));
    return fillUnsafeBox<Flags>(ctxI, serializer, boxD);
  }

  BLBoxI dstBoxI;
  if (!clipFillRectI(ctxI, &rect, &dstBoxI))
    return BL_SUCCESS;

  return fillClippedBoxA<Flags>(ctxI, serializer, dstBoxI);
}

// BLRasterEngine - ContextImpl - Fill Internals - Fill Geometry
// =============================================================

static BLResult BL_INLINE fillGeometryInternal(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreSync& serializer,
  BLGeometryType geometryType, const void* geometryData) noexcept {

  const BLBox* box;
  BLBox temporaryBox;

  // Gotos are used to limit the inline function expansion as all the FillXXX functions are inlined, this
  // makes the binary a bit smaller as each call to such function is expanded only once.
  switch (geometryType) {
    case BL_GEOMETRY_TYPE_RECTI: {
      return fillUnsafeRectI<RenderCommandSerializerFlags::kNone>(ctxI, serializer, *static_cast<const BLRectI*>(geometryData));
    }

    case BL_GEOMETRY_TYPE_RECTD: {
      const BLRect* r = static_cast<const BLRect*>(geometryData);
      temporaryBox.reset(r->x, r->y, r->x + r->w, r->y + r->h);

      box = &temporaryBox;
      goto FillBoxD;
    }

    case BL_GEOMETRY_TYPE_BOXI: {
      const BLBoxI* boxI = static_cast<const BLBoxI*>(geometryData);
      temporaryBox.reset(double(boxI->x0), double(boxI->y0), double(boxI->x1), double(boxI->y1));

      box = &temporaryBox;
      goto FillBoxD;
    }

    case BL_GEOMETRY_TYPE_BOXD: {
      box = static_cast<const BLBox*>(geometryData);
FillBoxD:
      return fillUnsafeBox<RenderCommandSerializerFlags::kNone>(ctxI, serializer, *box);
    }

    case BL_GEOMETRY_TYPE_POLYGONI:
    case BL_GEOMETRY_TYPE_POLYLINEI: {
      const BLArrayView<BLPointI>* array = static_cast<const BLArrayView<BLPointI>*>(geometryData);
      if (BL_UNLIKELY(array->size < 3))
        return BL_SUCCESS;

      return fillUnsafePolygon<RenderCommandSerializerFlags::kNone>(ctxI, serializer, ctxI->fillRule(), array->data, array->size);
    }

    case BL_GEOMETRY_TYPE_POLYGOND:
    case BL_GEOMETRY_TYPE_POLYLINED: {
      const BLArrayView<BLPoint>* array = static_cast<const BLArrayView<BLPoint>*>(geometryData);
      if (BL_UNLIKELY(array->size < 3))
        return BL_SUCCESS;

      return fillUnsafePolygon<RenderCommandSerializerFlags::kNone>(ctxI, serializer, ctxI->fillRule(), array->data, array->size);
    }

    default: {
      BLPath* temporaryPath = &ctxI->syncWorkData.tmpPath[3];
      temporaryPath->clear();
      BL_PROPAGATE(temporaryPath->addGeometry(geometryType, geometryData));

      geometryData = temporaryPath;
      BL_FALLTHROUGH
    }

    case BL_GEOMETRY_TYPE_PATH: {
      const BLPath* path = static_cast<const BLPath*>(geometryData);
      if (BL_UNLIKELY(path->empty()))
        return BL_SUCCESS;
      return fillUnsafePath<RenderCommandSerializerFlags::kNone>(ctxI, serializer, ctxI->fillRule(), *path);
    }
  }
}

static BLResult BL_INLINE fillGeometryInternal(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreAsync& serializer,
  BLGeometryType geometryType, const void* geometryData) noexcept {

  const BLBox* box;
  BLBox temporaryBox;

  // Gotos are used to limit the inline function expansion as all the FillXXX functions are inlined, this makes the
  // binary a bit smaller as each call to such function is expanded only once.
  switch (geometryType) {
    case BL_GEOMETRY_TYPE_RECTI: {
      return fillUnsafeRectI<RenderCommandSerializerFlags::kNone>(ctxI, serializer, *static_cast<const BLRectI*>(geometryData));
    }

    case BL_GEOMETRY_TYPE_RECTD: {
      const BLRect* r = static_cast<const BLRect*>(geometryData);
      temporaryBox.reset(r->x, r->y, r->x + r->w, r->y + r->h);

      box = &temporaryBox;
      goto FillBoxD;
    }

    case BL_GEOMETRY_TYPE_BOXI: {
      const BLBoxI* boxI = static_cast<const BLBoxI*>(geometryData);
      temporaryBox.reset(double(boxI->x0), double(boxI->y0), double(boxI->x1), double(boxI->y1));

      box = &temporaryBox;
      goto FillBoxD;
    }

    case BL_GEOMETRY_TYPE_BOXD: {
      box = static_cast<const BLBox*>(geometryData);
FillBoxD:
      return fillUnsafeBox<RenderCommandSerializerFlags::kNone>(ctxI, serializer, *box);
    }

    case BL_GEOMETRY_TYPE_POLYGONI:
    case BL_GEOMETRY_TYPE_POLYLINEI: {
      const BLArrayView<BLPointI>* array = static_cast<const BLArrayView<BLPointI>*>(geometryData);
      if (BL_UNLIKELY(array->size < 3))
        return BL_SUCCESS;

      return fillUnsafePolygon<RenderCommandSerializerFlags::kNone>(ctxI, serializer, ctxI->fillRule(), array->data, array->size);
    }

    case BL_GEOMETRY_TYPE_POLYGOND:
    case BL_GEOMETRY_TYPE_POLYLINED: {
      const BLArrayView<BLPoint>* array = static_cast<const BLArrayView<BLPoint>*>(geometryData);
      if (BL_UNLIKELY(array->size < 3))
        return BL_SUCCESS;

      return fillUnsafePolygon<RenderCommandSerializerFlags::kNone>(ctxI, serializer, ctxI->fillRule(), array->data, array->size);
    }

    case BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTI:
    case BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTD:
    case BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXI:
    case BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXD: {
      BLPath* temporaryPath = &ctxI->syncWorkData.tmpPath[3];
      temporaryPath->clear();
      BL_PROPAGATE(temporaryPath->addGeometry(geometryType, geometryData));

      geometryData = temporaryPath;
      geometryType = BL_GEOMETRY_TYPE_PATH;

      BL_FALLTHROUGH
    }

    case BL_GEOMETRY_TYPE_PATH: {
      const BLPath* path = static_cast<const BLPath*>(geometryData);
      if (path->size() <= BL_RASTER_CONTEXT_MINIMUM_ASYNC_PATH_SIZE)
        return fillUnsafePath<RenderCommandSerializerFlags::kNone>(ctxI, serializer, ctxI->fillRule(), *path);

      BL_FALLTHROUGH
    }

    default: {
      return fillUnsafeGeometry<RenderCommandSerializerFlags::kNone>(ctxI, serializer, ctxI->fillRule(), geometryType, geometryData);
    }
  }
}

// BLRasterEngine - ContextImpl - Fill Internals - Fill GlyphRun
// =============================================================

static BLResult fillGlyphRunInternal(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreSync& serializer,
  const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  WorkData* workData = &ctxI->syncWorkData;
  BL_PROPAGATE(blRasterContextUtilFillGlyphRun(workData, DirectStateAccessor(ctxI), pt, font, glyphRun));

  return fillClippedEdges<RenderCommandSerializerFlags::kNone>(ctxI, serializer, BL_FILL_RULE_NON_ZERO);
}

static BLResult fillGlyphRunInternal(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreAsync& serializer,
  const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  return enqueueFillOrStrokeGlyphRun<RenderCommandSerializerFlags::kNone, BL_CONTEXT_OP_TYPE_FILL>(ctxI, serializer, pt, font, glyphRun);
}

// BLRasterEngine - ContextImpl - Fill Internals - Fill Text
// =========================================================

static BL_INLINE BLResult fillTextInternal(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreSync& serializer,
  const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, BLTextEncoding encoding) noexcept {

  BLGlyphBuffer& gb = ctxI->syncWorkData.glyphBuffer;

  BL_PROPAGATE(gb.setText(text, size, encoding));
  BL_PROPAGATE(font->dcast().shape(gb));

  if (gb.empty())
    return BL_SUCCESS;

  return fillGlyphRunInternal(ctxI, serializer, pt, font, &gb.impl->glyphRun);
}

static BL_INLINE BLResult fillTextInternal(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreAsync& serializer,
  const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, BLTextEncoding encoding) noexcept {

  return enqueueFillOrStrokeText<RenderCommandSerializerFlags::kNone, BL_CONTEXT_OP_TYPE_FILL>(ctxI, serializer, pt, font, text, size, encoding);
}

// BLRasterEngine - ContextImpl - Clear All
// ========================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplClearAll(BLContextImpl* baseImpl) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  RenderCommandSerializerCore<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  uint32_t status = prepareClear(ctxI, serializer, BL_RASTER_CONTEXT_NO_CLEAR_FLAGS_FORCE);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  if (ctxI->syncWorkData.clipMode == BL_CLIP_MODE_ALIGNED_RECT)
    return fillClippedBoxA<RenderCommandSerializerFlags::kNone>(ctxI, serializer, ctxI->finalClipBoxI());

  BLBoxI boxU = blTruncToInt(ctxI->finalClipBoxFixedD());
  return fillClippedBoxU<RenderCommandSerializerFlags::kNone>(ctxI, serializer, boxU);
}

// BLRasterEngine - ContextImpl - Clear Rect
// =========================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplClearRectI(BLContextImpl* baseImpl, const BLRectI* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  RenderCommandSerializerCore<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  uint32_t status = prepareClear(ctxI, serializer, BL_RASTER_CONTEXT_NO_CLEAR_FLAGS);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return fillUnsafeRectI<RenderCommandSerializerFlags::kNone>(ctxI, serializer, *rect);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplClearRectD(BLContextImpl* baseImpl, const BLRect* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  RenderCommandSerializerCore<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  uint32_t status = prepareClear(ctxI, serializer, BL_RASTER_CONTEXT_NO_CLEAR_FLAGS);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  BLBox boxD(rect->x, rect->y, rect->x + rect->w, rect->y + rect->h);
  return fillUnsafeBox<RenderCommandSerializerFlags::kNone>(ctxI, serializer, boxD);
}

// BLRasterEngine - ContextImpl - Fill All
// =======================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillAll(BLContextImpl* baseImpl) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  RenderCommandSerializerCore<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  uint32_t status = prepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_FILL), BL_RASTER_CONTEXT_NO_FILL_FLAGS_FORCE);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  if (ctxI->syncWorkData.clipMode == BL_CLIP_MODE_ALIGNED_RECT)
    return fillClippedBoxA<RenderCommandSerializerFlags::kNone>(ctxI, serializer, ctxI->finalClipBoxI());

  BLBoxI boxU = blTruncToInt(ctxI->finalClipBoxFixedD());
  return fillClippedBoxU<RenderCommandSerializerFlags::kNone>(ctxI, serializer, boxU);
}

// BLRasterEngine - ContextImpl - Fill Rect
// ========================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillRectI(BLContextImpl* baseImpl, const BLRectI* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  RenderCommandSerializerCore<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  uint32_t status = prepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_FILL), BL_RASTER_CONTEXT_NO_FILL_FLAGS);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return fillUnsafeRectI<RenderCommandSerializerFlags::kNone>(ctxI, serializer, *rect);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillRectD(BLContextImpl* baseImpl, const BLRect* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  RenderCommandSerializerCore<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  uint32_t status = prepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_FILL), BL_RASTER_CONTEXT_NO_FILL_FLAGS);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  BLBox boxD(rect->x, rect->y, rect->x + rect->w, rect->y + rect->h);
  return fillUnsafeBox<RenderCommandSerializerFlags::kNone>(ctxI, serializer, boxD);
}

// BLRasterEngine - ContextImpl - Fill Geometry
// ============================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillGeometry(BLContextImpl* baseImpl, BLGeometryType geometryType, const void* geometryData) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  RenderCommandSerializerCore<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  uint32_t status = prepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_FILL), BL_RASTER_CONTEXT_NO_FILL_FLAGS);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return fillGeometryInternal(ctxI, serializer, geometryType, geometryData);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillPathD(BLContextImpl* baseImpl, const BLPathCore* path) noexcept {
  return blRasterContextImplFillGeometry<RenderingMode>(baseImpl, BL_GEOMETRY_TYPE_PATH, path);
}

// BLRasterEngine - ContextImpl - Fill GlyphRun
// ============================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillGlyphRunD(BLContextImpl* baseImpl, const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (!font->dcast().isValid())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  if (glyphRun->empty())
    return BL_SUCCESS;

  RenderCommandSerializerCore<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  if (prepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_FILL), BL_RASTER_CONTEXT_NO_FILL_FLAGS) == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return fillGlyphRunInternal(ctxI, serializer, pt, font, glyphRun);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillGlyphRunI(BLContextImpl* baseImpl, const BLPointI* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BLPoint ptD(pt->x, pt->y);
  return blRasterContextImplFillGlyphRunD<RenderingMode>(baseImpl, &ptD, font, glyphRun);
}

// BLRasterEngine - ContextImpl - Fill Text
// ========================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillTextD(BLContextImpl* baseImpl, const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, BLTextEncoding encoding) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(!font->dcast().isValid()))
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  if (BL_UNLIKELY(uint32_t(encoding) > BL_TEXT_ENCODING_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  RenderCommandSerializerCore<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  if (prepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_FILL), BL_RASTER_CONTEXT_NO_FILL_FLAGS) == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return fillTextInternal(ctxI, serializer, pt, font, text, size, encoding);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillTextI(BLContextImpl* baseImpl, const BLPointI* pt, const BLFontCore* font, const void* text, size_t size, BLTextEncoding encoding) noexcept {
  BLPoint ptD(pt->x, pt->y);
  return blRasterContextImplFillTextD<RenderingMode>(baseImpl, &ptD, font, text, size, encoding);
}

// BLRasterEngine - ContextImpl - Fill Mask
// ========================================

template<RenderCommandSerializerFlags Flags>
static BL_INLINE BLResult fillClippedMaskRawA(BLRasterContextImpl* ctxI, RenderCommandSerializerMaskSync& serializer, const BLBoxI& boxA, const BLImageCore* mask, const BLPointI& maskOffset) noexcept {
  serializer.initFillMaskRaw(boxA, mask, maskOffset);

  if (!ensureFetchData<Flags>(ctxI, serializer))
    return BL_SUCCESS;

  return processFillMaskA(ctxI, serializer);
}

template<RenderCommandSerializerFlags Flags>
static BL_INLINE BLResult fillClippedMaskRawA(BLRasterContextImpl* ctxI, RenderCommandSerializerMaskAsync& serializer, const BLBoxI& boxA, const BLImageCore* mask, const BLPointI& maskOffset) noexcept {
  serializer.initFillMaskRaw(boxA, mask, maskOffset);

  if (!ensureFetchData<Flags>(ctxI, serializer))
    return BL_SUCCESS;

  BL_PROPAGATE(ctxI->workerMgr->ensureImageQueue());
  return enqueueFillMaskA<RenderCommandSerializerFlags::kNone>(ctxI, serializer, mask);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillMaskD(BLContextImpl* baseImpl, const BLPoint* pt, const BLImageCore* mask, const BLRectI* maskArea) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BLImageImpl* maskI = BLImagePrivate::getImpl(mask);
  BLRectI maskRect;
  BL_PROPAGATE(checkImageArea(maskRect, maskI, maskArea));

  BLPoint dst(*pt);

  RenderCommandSerializerMask<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  uint32_t status = prepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_FILL), BL_RASTER_CONTEXT_NO_FILL_FLAGS);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  if (ctxI->finalMatrixType() <= BL_MATRIX2D_TYPE_TRANSLATE) {
    double startX = dst.x * ctxI->finalMatrixFixed().m00 + ctxI->finalMatrixFixed().m20;
    double startY = dst.y * ctxI->finalMatrixFixed().m11 + ctxI->finalMatrixFixed().m21;

    BLBox dstBoxD(blMax(startX, ctxI->finalClipBoxFixedD().x0),
                  blMax(startY, ctxI->finalClipBoxFixedD().y0),
                  blMin(startX + double(maskRect.w) * ctxI->finalMatrixFixed().m00, ctxI->finalClipBoxFixedD().x1),
                  blMin(startY + double(maskRect.h) * ctxI->finalMatrixFixed().m11, ctxI->finalClipBoxFixedD().y1));

    // Clipped out, invalid coordinates, or empty `maskArea`.
    if (!((dstBoxD.x0 < dstBoxD.x1) & (dstBoxD.y0 < dstBoxD.y1)))
      return BL_SUCCESS;

    int64_t startFx = blFloorToInt64(startX);
    int64_t startFy = blFloorToInt64(startY);

    BLBoxI dstBoxU = blTruncToInt(dstBoxD);

    if (!((startFx | startFy) & ctxI->renderTargetInfo.fpMaskI)) {
      // Pixel aligned mask.
      int x0 = dstBoxU.x0 >> ctxI->renderTargetInfo.fpShiftI;
      int y0 = dstBoxU.y0 >> ctxI->renderTargetInfo.fpShiftI;
      int x1 = (dstBoxU.x1 + ctxI->renderTargetInfo.fpMaskI) >> ctxI->renderTargetInfo.fpShiftI;
      int y1 = (dstBoxU.y1 + ctxI->renderTargetInfo.fpMaskI) >> ctxI->renderTargetInfo.fpShiftI;

      int tx = int(startFx >> ctxI->renderTargetInfo.fpShiftI);
      int ty = int(startFy >> ctxI->renderTargetInfo.fpShiftI);

      maskRect.x += x0 - tx;
      maskRect.y += y0 - ty;
      maskRect.w = x1 - x0;
      maskRect.h = y1 - y0;

      // Pixel aligned fill with a pixel aligned mask.
      if (isBoxAligned24x8(dstBoxU))
        return fillClippedMaskRawA<RenderCommandSerializerFlags::kMask>(ctxI, serializer, BLBoxI(x0, y0, x1, y1), mask, BLPointI(maskRect.x, maskRect.y));

      // TODO: [Rendering Context] Masking support.
      /*
      BL_PROPAGATE(serializer.initFetchDataForMask(ctxI));
      serializer.maskFetchData()->initPatternSource(mask, maskRect);
      if (!serializer.maskFetchData()->setupPatternBlit(x0, y0))
        return BL_SUCCESS;
      */
    }
    else {
      // TODO: [Rendering Context] Masking support.
      /*
      BL_PROPAGATE(serializer.initFetchDataForMask(ctxI));
      serializer.maskFetchData()->initPatternSource(mask, maskRect);
      if (!serializer.maskFetchData()->setupPatternFxFy(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, ctxI->hints().patternQuality, startFx, startFy))
        return BL_SUCCESS;
      */
    }

    /*
    return fillClippedBoxU<RenderCommandSerializerFlags::kMask>(ctxI, serializer, dstBoxU);
    */
  }

  return blTraceError(BL_ERROR_NOT_IMPLEMENTED);

  /*
  else {
    BLMatrix2D m(ctxI->finalMatrix());
    m.translate(dst.x, dst.y);

    BL_PROPAGATE(serializer.initFetchDataForMask(ctxI));
    serializer.maskFetchData()->initPatternSource(mask, maskRect);
    if (!serializer.maskFetchData()->setupPatternAffine(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, ctxI->hints().patternQuality, m)) {
      serializer.rollbackFetchData(ctxI);
      return BL_SUCCESS;
    }
  }

  BLBox finalBox(dst.x, dst.y, dst.x + double(maskRect.w), dst.y + double(maskRect.h));
  return blRasterContextImplFinalizeBlit(ctxI, serializer,
         fillUnsafeBox<RenderCommandSerializerFlags::kMask>(ctxI, serializer, finalBox));
  */
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillMaskI(BLContextImpl* baseImpl, const BLPointI* pt, const BLImageCore* mask, const BLRectI* maskArea) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLImageImpl* maskI = BLImagePrivate::getImpl(mask);

  if (!(ctxI->contextFlags & BL_RASTER_CONTEXT_INTEGRAL_TRANSLATION)) {
    BLPoint ptD(pt->x, pt->y);
    return blRasterContextImplFillMaskD<RenderingMode>(ctxI, &ptD, mask, maskArea);
  }

  BLBoxI dstBox;
  BLPointI srcOffset;
  BLResult clippedOutResult;

  if (!clipBlitRectI(ctxI, pt, maskArea, &maskI->size, &clippedOutResult, &dstBox, &srcOffset))
    return clippedOutResult;

  RenderCommandSerializerMask<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  uint32_t status = prepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_FILL), BL_RASTER_CONTEXT_NO_FILL_FLAGS);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return fillClippedMaskRawA<RenderCommandSerializerFlags::kMask>(ctxI, serializer, dstBox, mask, srcOffset);
}

// BLRasterEngine - ContextImpl - Stroke Internals - Sync - Stroke Unsafe Geometry
// ===============================================================================

template<RenderCommandSerializerFlags Flags>
static BL_INLINE BLResult strokeUnsafeGeometry(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreSync& serializer,
  BLGeometryType geometryType, const void* geometryData) noexcept {

  WorkData* workData = &ctxI->syncWorkData;
  BLPath* path;

  if (geometryType == BL_GEOMETRY_TYPE_PATH) {
    path = const_cast<BLPath*>(static_cast<const BLPath*>(geometryData));
  }
  else {
    path = &workData->tmpPath[3];
    path->clear();
    BL_PROPAGATE(path->addGeometry(geometryType, geometryData));
  }

  BL_PROPAGATE(blRasterContextUtilStrokeUnsafePath(workData, DirectStateAccessor(ctxI), path));
  return fillClippedEdges<Flags>(ctxI, serializer, BL_FILL_RULE_NON_ZERO);
}

// BLRasterEngine - ContextImpl - Stroke Geometry
// ==============================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokeGeometry(BLContextImpl* baseImpl, BLGeometryType geometryType, const void* geometryData) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  RenderCommandSerializerCore<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  uint32_t status = prepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_STROKE), BL_RASTER_CONTEXT_NO_STROKE_FLAGS);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return strokeUnsafeGeometry<RenderCommandSerializerFlags::kNone>(ctxI, serializer, geometryType, geometryData);
}

// BLRasterEngine - ContextImpl - Stroke Path
// ==========================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokePathD(BLContextImpl* baseImpl, const BLPathCore* path) noexcept {
  return blRasterContextImplStrokeGeometry<RenderingMode>(baseImpl, BL_GEOMETRY_TYPE_PATH, path);
}

// BLRasterEngine - ContextImpl - Stroke Rect
// ==========================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokeRectI(BLContextImpl* baseImpl, const BLRectI* rect) noexcept {
  return blRasterContextImplStrokeGeometry<RenderingMode>(baseImpl, BL_GEOMETRY_TYPE_RECTI, rect);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokeRectD(BLContextImpl* baseImpl, const BLRect* rect) noexcept {
  return blRasterContextImplStrokeGeometry<RenderingMode>(baseImpl, BL_GEOMETRY_TYPE_RECTD, rect);
}

// BLRasterEngine - ContextImpl - Stroke GlyphRun
// ==============================================

static BLResult blRasterContextImplStrokeGlyphRunInternal(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreSync& serializer,
  const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  WorkData* workData = &ctxI->syncWorkData;
  BL_PROPAGATE(blRasterContextUtilStrokeGlyphRun(workData, DirectStateAccessor(ctxI), pt, font, glyphRun));

  return fillClippedEdges<RenderCommandSerializerFlags::kNone>(ctxI, serializer, BL_FILL_RULE_NON_ZERO);
}

static BLResult blRasterContextImplStrokeGlyphRunInternal(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreAsync& serializer,
  const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  return enqueueFillOrStrokeGlyphRun<RenderCommandSerializerFlags::kNone, BL_CONTEXT_OP_TYPE_STROKE>(ctxI, serializer, pt, font, glyphRun);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokeGlyphRunD(BLContextImpl* baseImpl, const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (!font->dcast().isValid())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  if (glyphRun->empty())
    return BL_SUCCESS;

  RenderCommandSerializerCore<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  if (prepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_STROKE), BL_RASTER_CONTEXT_NO_STROKE_FLAGS) == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return blRasterContextImplStrokeGlyphRunInternal(ctxI, serializer, pt, font, glyphRun);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokeGlyphRunI(BLContextImpl* baseImpl, const BLPointI* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BLPoint ptD(pt->x, pt->y);
  return blRasterContextImplStrokeGlyphRunD<RenderingMode>(baseImpl, &ptD, font, glyphRun);
}

// BLRasterEngine - ContextImpl - Stroke Text
// ==========================================

static BL_INLINE BLResult blRasterContextImplStrokeTextInternal(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreSync& serializer,
  const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, BLTextEncoding encoding) noexcept {

  BLGlyphBuffer& gb = ctxI->syncWorkData.glyphBuffer;

  BL_PROPAGATE(gb.setText(text, size, encoding));
  BL_PROPAGATE(font->dcast().shape(gb));

  if (gb.empty())
    return BL_SUCCESS;

  return blRasterContextImplStrokeGlyphRunInternal(ctxI, serializer, pt, font, &gb.impl->glyphRun);
}

static BL_INLINE BLResult blRasterContextImplStrokeTextInternal(
  BLRasterContextImpl* ctxI,
  RenderCommandSerializerCoreAsync& serializer,
  const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, BLTextEncoding encoding) noexcept {

  return enqueueFillOrStrokeText<RenderCommandSerializerFlags::kNone, BL_CONTEXT_OP_TYPE_STROKE>(ctxI, serializer, pt, font, text, size, encoding);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokeTextD(BLContextImpl* baseImpl, const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, BLTextEncoding encoding) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  if (!font->dcast().isValid())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  RenderCommandSerializerCore<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  if (prepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_STROKE), BL_RASTER_CONTEXT_NO_STROKE_FLAGS) == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return blRasterContextImplStrokeTextInternal(ctxI, serializer, pt, font, text, size, encoding);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokeTextI(BLContextImpl* baseImpl, const BLPointI* pt, const BLFontCore* font, const void* text, size_t size, BLTextEncoding encoding) noexcept {
  BLPoint ptD(pt->x, pt->y);
  return blRasterContextImplStrokeTextD<RenderingMode>(baseImpl, &ptD, font, text, size, encoding);
}

// BLRasterEngine - ContextImpl - Blit Internals - Prepare Blit
// ============================================================

template<uint32_t RenderingMode>
static BL_INLINE uint32_t prepareBlit(BLRasterContextImpl* ctxI, RenderCommandSerializerBlit<RenderingMode>& serializer, uint32_t alpha, uint32_t format) noexcept {
  BLCompOpSimplifyInfo simplifyInfo = ctxI->compOpSimplifyInfo[format];

  BLCompOpSolidId solidId = simplifyInfo.solidId();
  uint32_t contextFlags = ctxI->contextFlags | uint32_t(solidId);
  uint32_t nopFlags = BL_RASTER_CONTEXT_NO_BLIT_FLAGS;

  serializer.initPipeline(simplifyInfo.signature());
  serializer.initCommand(alpha);

  // Likely case - composition flag doesn't lead to a solid fill and there are no other 'NO_' flags so the rendering
  // of this command should produce something.
  nopFlags &= contextFlags;
  if (nopFlags == 0)
    return BL_RASTER_CONTEXT_PREPARE_STATUS_FETCH;

  // Remove reserved flags we may have added to `nopFlags` if srcSolidId was non-zero and add a possible condition
  // flag to `nopFlags` if the composition is NOP (DST-COPY).
  nopFlags &= ~BL_RASTER_CONTEXT_NO_RESERVED;
  nopFlags |= uint32_t(serializer.pipeSignature() == BLCompOpSimplifyInfo::dstCopy().signature());

  // The combination of a destination format, source format, and compOp results in a solid fill, so initialize the
  // command accordingly to `solidId` type.
  serializer.initFetchSolid(ctxI->solidFetchDataTable[uint32_t(solidId)]);

  // If there are bits in `nopFlags` it means there is nothing to render as compOp, style, alpha, or something else
  // is nop/invalid.
  if (nopFlags != 0)
    return BL_RASTER_CONTEXT_PREPARE_STATUS_NOP;
  else
    return BL_RASTER_CONTEXT_PREPARE_STATUS_SOLID;
}

// BLRasterEngine - ContextImpl - Blit Image
// =========================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplBlitImageD(BLContextImpl* baseImpl, const BLPoint* pt, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BLImageImpl* imgI = BLImagePrivate::getImpl(img);
  BLRectI srcRect;
  BL_PROPAGATE(checkImageArea(srcRect, imgI, imgArea));

  BLPoint dst(*pt);
  RenderCommandSerializerBlit<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  uint32_t status = prepareBlit(ctxI, serializer, ctxI->globalAlphaI(), imgI->format);
  if (status <= BL_RASTER_CONTEXT_PREPARE_STATUS_SOLID) {
    if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
      return BL_SUCCESS;
  }
  else {
    BL_PROPAGATE(serializer.initFetchDataForBlit(ctxI));

    if (ctxI->finalMatrixType() <= BL_MATRIX2D_TYPE_TRANSLATE) {
      double startX = dst.x * ctxI->finalMatrixFixed().m00 + ctxI->finalMatrixFixed().m20;
      double startY = dst.y * ctxI->finalMatrixFixed().m11 + ctxI->finalMatrixFixed().m21;

      double dx0 = blMax(startX, ctxI->finalClipBoxFixedD().x0);
      double dy0 = blMax(startY, ctxI->finalClipBoxFixedD().y0);
      double dx1 = blMin(startX + double(srcRect.w) * ctxI->finalMatrixFixed().m00, ctxI->finalClipBoxFixedD().x1);
      double dy1 = blMin(startY + double(srcRect.h) * ctxI->finalMatrixFixed().m11, ctxI->finalClipBoxFixedD().y1);

      // Clipped out, invalid coordinates, or empty `imgArea`.
      if (!((dx0 < dx1) & (dy0 < dy1)))
        return BL_SUCCESS;

      int64_t startFx = blFloorToInt64(startX);
      int64_t startFy = blFloorToInt64(startY);

      int ix0 = blTruncToInt(dx0);
      int iy0 = blTruncToInt(dy0);
      int ix1 = blTruncToInt(dx1);
      int iy1 = blTruncToInt(dy1);

      if (!((startFx | startFy) & ctxI->renderTargetInfo.fpMaskI)) {
        // Pixel aligned blit. At this point we still don't know whether the area where the pixels will be composited
        // is aligned, but we know for sure that the pixels of `src` image don't require any interpolation.
        int x0 = ix0 >> ctxI->renderTargetInfo.fpShiftI;
        int y0 = iy0 >> ctxI->renderTargetInfo.fpShiftI;
        int x1 = (ix1 + ctxI->renderTargetInfo.fpMaskI) >> ctxI->renderTargetInfo.fpShiftI;
        int y1 = (iy1 + ctxI->renderTargetInfo.fpMaskI) >> ctxI->renderTargetInfo.fpShiftI;

        int tx = int(startFx >> ctxI->renderTargetInfo.fpShiftI);
        int ty = int(startFy >> ctxI->renderTargetInfo.fpShiftI);

        srcRect.x += x0 - tx;
        srcRect.y += y0 - ty;
        srcRect.w = x1 - x0;
        srcRect.h = y1 - y0;

        serializer.fetchData()->initPatternSource(img, srcRect);
        if (!serializer.fetchData()->setupPatternBlit(x0, y0)) {
          serializer.rollbackFetchData(ctxI);
          return BL_SUCCESS;
        }
      }
      else {
        serializer.fetchData()->initPatternSource(img, srcRect);
        if (!serializer.fetchData()->setupPatternFxFy(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, ctxI->hints().patternQuality, startFx, startFy)) {
          serializer.rollbackFetchData(ctxI);
          return BL_SUCCESS;
        }
      }

      return fillClippedBoxU<RenderCommandSerializerFlags::kBlit>(ctxI, serializer, BLBoxI(ix0, iy0, ix1, iy1));
    }
    else {
      BLMatrix2D m(ctxI->finalMatrix());
      m.translate(dst.x, dst.y);

      serializer.fetchData()->initPatternSource(img, srcRect);
      if (!serializer.fetchData()->setupPatternAffine(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, ctxI->hints().patternQuality, m)) {
        serializer.rollbackFetchData(ctxI);
        return BL_SUCCESS;
      }
    }
  }

  BLBox finalBox(dst.x, dst.y, dst.x + double(srcRect.w), dst.y + double(srcRect.h));
  return blRasterContextImplFinalizeBlit(ctxI, serializer,
         fillUnsafeBox<RenderCommandSerializerFlags::kBlit>(ctxI, serializer, finalBox));
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplBlitImageI(BLContextImpl* baseImpl, const BLPointI* pt, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLImageImpl* imgI = BLImagePrivate::getImpl(img);

  if (!(ctxI->contextFlags & BL_RASTER_CONTEXT_INTEGRAL_TRANSLATION)) {
    BLPoint ptD(pt->x, pt->y);
    return blRasterContextImplBlitImageD<RenderingMode>(ctxI, &ptD, img, imgArea);
  }

  BLBoxI dstBox;
  BLPointI srcOffset;
  BLResult clippedOutResult;

  if (!clipBlitRectI(ctxI, pt, imgArea, &imgI->size, &clippedOutResult, &dstBox, &srcOffset))
    return clippedOutResult;

  RenderCommandSerializerBlit<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  uint32_t status = prepareBlit(ctxI, serializer, ctxI->globalAlphaI(), imgI->format);
  if (BL_UNLIKELY(status <= BL_RASTER_CONTEXT_PREPARE_STATUS_SOLID)) {
    if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
      return BL_SUCCESS;
  }
  else {
    BL_PROPAGATE(serializer.initFetchDataForBlit(ctxI));
    serializer.fetchData()->initPatternSource(img, BLRectI(srcOffset.x, srcOffset.y, dstBox.x1 - dstBox.x0, dstBox.y1 - dstBox.y0));
    if (!serializer.fetchData()->setupPatternBlit(dstBox.x0, dstBox.y0)) {
      serializer.rollbackFetchData(ctxI);
      return BL_SUCCESS;
    }
  }

  return blRasterContextImplFinalizeBlit(ctxI, serializer,
         fillClippedBoxA<RenderCommandSerializerFlags::kBlit>(ctxI, serializer, dstBox));
}

// BLRasterEngine - ContextImpl - Blit Scaled Image
// ================================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplBlitScaledImageD(BLContextImpl* baseImpl, const BLRect* rect, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BLImageImpl* imgI = BLImagePrivate::getImpl(img);
  BLRectI srcRect;
  BL_PROPAGATE(checkImageArea(srcRect, imgI, imgArea));

  // Optimization: Don't go over all the transformations if the destination and source rects have the same size.
  if ((rect->w == double(srcRect.w)) & (rect->h == double(srcRect.h)))
    return blRasterContextImplBlitImageD<RenderingMode>(ctxI, reinterpret_cast<const BLPoint*>(rect), img, imgArea);

  RenderCommandSerializerBlit<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  BLBox finalBox(rect->x, rect->y, rect->x + rect->w, rect->y + rect->h);

  uint32_t status = prepareBlit(ctxI, serializer, ctxI->globalAlphaI(), imgI->format);
  if (BL_UNLIKELY(status <= BL_RASTER_CONTEXT_PREPARE_STATUS_SOLID)) {
    if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
      return BL_SUCCESS;
  }
  else {
    BLMatrix2D m(rect->w / double(srcRect.w), 0.0, 0.0, rect->h / double(srcRect.h), rect->x, rect->y);
    BLTransformPrivate::multiply(m, m, ctxI->finalMatrix());

    BL_PROPAGATE(serializer.initFetchDataForBlit(ctxI));
    serializer.fetchData()->initPatternSource(img, srcRect);
    if (!serializer.fetchData()->setupPatternAffine(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, ctxI->hints().patternQuality, m)) {
      serializer.rollbackFetchData(ctxI);
      return BL_SUCCESS;
    }
  }

  return blRasterContextImplFinalizeBlit(ctxI, serializer,
         fillUnsafeBox<RenderCommandSerializerFlags::kBlit>(ctxI, serializer, finalBox));
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplBlitScaledImageI(BLContextImpl* baseImpl, const BLRectI* rect, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BLImageImpl* imgI = BLImagePrivate::getImpl(img);
  BLRectI srcRect;
  BL_PROPAGATE(checkImageArea(srcRect, imgI, imgArea));

  // Optimization: Don't go over all the transformations if the destination and source rects have the same size.
  if (rect->w == srcRect.w && rect->h == srcRect.h)
    return blRasterContextImplBlitImageI<RenderingMode>(ctxI, reinterpret_cast<const BLPointI*>(rect), img, imgArea);

  RenderCommandSerializerBlit<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initSerializer(ctxI));

  uint32_t status = prepareBlit(ctxI, serializer, ctxI->globalAlphaI(), imgI->format);
  BLBox finalBox(double(rect->x),
                 double(rect->y),
                 double(rect->x) + double(rect->w),
                 double(rect->y) + double(rect->h));

  if (status <= BL_RASTER_CONTEXT_PREPARE_STATUS_SOLID) {
    if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
      return BL_SUCCESS;
  }
  else {
    BLMatrix2D m(double(rect->w) / double(srcRect.w), 0.0, 0.0, double(rect->h) / double(srcRect.h), double(rect->x), double(rect->y));
    BLTransformPrivate::multiply(m, m, ctxI->finalMatrix());

    BL_PROPAGATE(serializer.initFetchDataForBlit(ctxI));
    serializer.fetchData()->initPatternSource(img, srcRect);
    if (!serializer.fetchData()->setupPatternAffine(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, ctxI->hints().patternQuality, m)) {
      serializer.rollbackFetchData(ctxI);
      return BL_SUCCESS;
    }
  }

  return blRasterContextImplFinalizeBlit(ctxI, serializer,
         fillUnsafeBox<RenderCommandSerializerFlags::kBlit>(ctxI, serializer, finalBox));
}

// BLRasterEngine - ContextImpl - Attach & Detach
// ==============================================

static uint32_t blRasterContextImplCalculateBandHeight(uint32_t format, const BLSizeI& size, const BLContextCreateInfo* options) noexcept {
  // TODO: [Rendering Context] We should use the format and calculate how many bytes are used by raster storage per band.
  blUnused(format);

  // Maximum band height we start at is 64, then decrease to 16.
  uint32_t kMinBandHeight = 8;
  uint32_t kMaxBandHeight = 64;

  uint32_t bandHeight = kMaxBandHeight;

  // TODO: [Rendering Context] We should read this number from the CPU and adjust.
  size_t cacheSizeLimit = 1024 * 256;
  size_t pixelCount = size_t(uint32_t(size.w)) * bandHeight;

  do {
    size_t cellStorage = pixelCount * sizeof(uint32_t);
    if (cellStorage <= cacheSizeLimit)
      break;

    bandHeight >>= 1;
    pixelCount >>= 1;
  } while (bandHeight > kMinBandHeight);

  uint32_t threadCount = options->threadCount;
  if (bandHeight > kMinBandHeight && threadCount > 1) {
    uint32_t bandHeightShift = BLIntOps::ctz(bandHeight);
    uint32_t minimumBandCount = threadCount;

    do {
      uint32_t bandCount = (uint32_t(size.h) + bandHeight - 1) >> bandHeightShift;
      if (bandCount >= minimumBandCount)
        break;

      bandHeight >>= 1;
      bandHeightShift--;
    } while (bandHeight > kMinBandHeight);
  }

  return bandHeight;
}

static BLResult blRasterContextImplAttach(BLRasterContextImpl* ctxI, BLImageCore* image, const BLContextCreateInfo* options) noexcept {
  BL_ASSERT(image != nullptr);
  BL_ASSERT(options != nullptr);

  uint32_t format = BLImagePrivate::getImpl(image)->format;
  BLSizeI size = BLImagePrivate::getImpl(image)->size;

  // TODO: [Rendering Context] Hardcoded for 8bpc.
  uint32_t targetComponentType = RenderTargetInfo::kPixelComponentUInt8;

  uint32_t bandHeight = blRasterContextImplCalculateBandHeight(format, size, options);
  uint32_t bandCount = (uint32_t(size.h) + bandHeight - 1) >> BLIntOps::ctz(bandHeight);

  // Initialization.
  BLResult result = BL_SUCCESS;
  BLPipeline::PipeRuntime* pipeRuntime = nullptr;

  // If anything fails we would restore the zone state to match this point.
  BLArenaAllocator& baseZone = ctxI->baseZone;
  BLArenaAllocator::StatePtr zoneState = baseZone.saveState();

  // Not a real loop, just a scope we can escape early through 'break'.
  do {
    // Step 1: Initialize edge storage of the sync worker.
    result = ctxI->syncWorkData.initBandData(bandHeight, bandCount);
    if (result != BL_SUCCESS)
      break;

    // Step 2: Initialize the thread manager if multi-threaded rendering is enabled.
    if (options->threadCount) {
      ctxI->ensureWorkerMgr();
      result = ctxI->workerMgr->init(ctxI, options);

      if (result != BL_SUCCESS)
        break;

      if (ctxI->workerMgr->isActive())
        ctxI->renderingMode = BL_RASTER_RENDERING_MODE_ASYNC;
    }

    // Step 3: Initialize pipeline runtime (JIT or fixed).
#if !defined(BL_BUILD_NO_JIT)
    if (!(options->flags & BL_CONTEXT_CREATE_FLAG_DISABLE_JIT)) {
      pipeRuntime = &BLPipeline::JIT::PipeDynamicRuntime::_global;

      if (options->flags & BL_CONTEXT_CREATE_FLAG_ISOLATED_JIT_RUNTIME) {
        // Create an isolated `BLPipeGenRuntime` if specified. It will be used
        // to store all functions generated during the rendering and will be
        // destroyed together with the context.
        BLPipeline::JIT::PipeDynamicRuntime* isolatedRT = baseZone.newT<BLPipeline::JIT::PipeDynamicRuntime>(BLPipeline::PipeRuntimeFlags::kIsolated);

        // This should not really happen as the first block is allocated with the impl.
        if (BL_UNLIKELY(!isolatedRT)) {
          result = blTraceError(BL_ERROR_OUT_OF_MEMORY);
          break;
        }

        // Enable logger if required.
        if (options->flags & BL_CONTEXT_CREATE_FLAG_ISOLATED_JIT_LOGGING) {
          isolatedRT->setLoggerEnabled(true);
        }

        // Feature restrictions are related to JIT compiler - it allows us to test the
        // code generated by JIT with less features than the current CPU has, to make
        // sure that we support older hardware or to compare between implementations.
        if (options->flags & BL_CONTEXT_CREATE_FLAG_OVERRIDE_CPU_FEATURES) {
          isolatedRT->_restrictFeatures(options->cpuFeatures);
        }

        pipeRuntime = isolatedRT;
      }
    }
#endif

#if !defined(BL_BUILD_NO_FIXED_PIPE)
    if (!pipeRuntime)
      pipeRuntime = &BLPipeline::PipeStaticRuntime::_global;
#endif

    if (BL_UNLIKELY(!pipeRuntime)) {
      result = blTraceError(BL_ERROR_INVALID_CREATE_FLAGS);
      break;
    }

    // Step 4: Make the destination image mutable.
    result = blImageMakeMutable(image, &ctxI->dstData);
    if (result != BL_SUCCESS)
      break;
  } while (0);

  // Handle a possible initialization failure.
  if (result != BL_SUCCESS) {
    // Switch back to a synchronous rendering mode if asynchronous rendering
    // was already setup. We have already acquired worker threads that must
    // be released now.
    if (ctxI->renderingMode == BL_RASTER_RENDERING_MODE_ASYNC) {
      ctxI->workerMgr->reset();
      ctxI->renderingMode = BL_RASTER_RENDERING_MODE_SYNC;
    }

    // If we failed we don't want the pipeline runtime associated with the
    // context so we simply destroy it and pretend like nothing happened.
    if (pipeRuntime) {
      if (blTestFlag(pipeRuntime->runtimeFlags(), BLPipeline::PipeRuntimeFlags::kIsolated))
        pipeRuntime->destroy();
    }

    baseZone.restoreState(zoneState);
    // TODO: [Rendering Context]
    // ctxI->jobZone.clear();
    return result;
  }

  if (!ctxI->isSync())
    ctxI->virt = &rasterImplVirtAsync;

  // Increase `writerCount` of the image, will be decreased by `blRasterContextImplDetach()`.
  BLImagePrivateImpl* imageI = BLImagePrivate::getImpl(image);
  blAtomicFetchAddRelaxed(&imageI->writerCount);
  ctxI->dstImage._d = image->_d;

  // Initialize the pipeline runtime and pipeline lookup cache.
  ctxI->pipeProvider.init(pipeRuntime);
  ctxI->pipeLookupCache.reset();

  // Initialize the sync work data.
  ctxI->syncWorkData.initContextData(ctxI->dstData);

  // Initialize destination image information available in a public rendering context state.
  ctxI->internalState.targetSize.reset(size.w, size.h);
  ctxI->internalState.targetImage = &ctxI->dstImage;

  // Initialize members that are related to target precision.
  ctxI->contextFlags = BL_RASTER_CONTEXT_INTEGRAL_TRANSLATION;
  ctxI->renderTargetInfo = renderTargetInfoByComponentType[targetComponentType];
  ctxI->fpMinSafeCoordD = blFloor(double(BLTraits::minValue<int32_t>() + 1) * ctxI->fpScaleD());
  ctxI->fpMaxSafeCoordD = blFloor(double(BLTraits::maxValue<int32_t>() - 1 - blMax(size.w, size.h)) * ctxI->fpScaleD());

  // Initialize members that are related to alpha blending and composition.
  ctxI->solidFormatTable[BL_RASTER_CONTEXT_SOLID_FORMAT_ARGB] = uint8_t(BLInternalFormat::kPRGB32);
  ctxI->solidFormatTable[BL_RASTER_CONTEXT_SOLID_FORMAT_FRGB] = uint8_t(BLInternalFormat::kFRGB32);
  ctxI->solidFormatTable[BL_RASTER_CONTEXT_SOLID_FORMAT_ZERO] = uint8_t(BLInternalFormat::kZERO32);
  ctxI->solidFetchDataTable = targetComponentType == RenderTargetInfo::kPixelComponentUInt16
    ? reinterpret_cast<const BLPipeline::FetchData::Solid*>(blRasterContextSolidDataRgba64)
    : reinterpret_cast<const BLPipeline::FetchData::Solid*>(blRasterContextSolidDataRgba32);

  // Initialize the rendering state to defaults.
  ctxI->contextFlags = BL_RASTER_CONTEXT_INTEGRAL_TRANSLATION;
  ctxI->stateIdCounter = 0;
  ctxI->savedState = nullptr;
  ctxI->sharedFillState = nullptr;
  ctxI->sharedStrokeState = nullptr;

  // Initialize public state.
  ctxI->internalState.hints.reset();
  ctxI->internalState.hints.patternQuality = BL_PATTERN_QUALITY_BILINEAR;
  ctxI->internalState.compOp = uint8_t(BL_COMP_OP_SRC_OVER);
  ctxI->internalState.fillRule = uint8_t(BL_FILL_RULE_NON_ZERO);
  ctxI->internalState.styleType[BL_CONTEXT_OP_TYPE_FILL] = uint8_t(BL_OBJECT_TYPE_RGBA);
  ctxI->internalState.styleType[BL_CONTEXT_OP_TYPE_STROKE] = uint8_t(BL_OBJECT_TYPE_RGBA);
  memset(ctxI->internalState.reserved, 0, sizeof(ctxI->internalState.reserved));
  ctxI->internalState.approximationOptions = BLPathPrivate::makeDefaultApproximationOptions();
  ctxI->internalState.globalAlpha = 1.0;
  ctxI->internalState.styleAlpha[0] = 1.0;
  ctxI->internalState.styleAlpha[1] = 1.0;
  blCallCtor(ctxI->internalState.strokeOptions.dcast());
  ctxI->internalState.metaMatrix.reset();
  ctxI->internalState.userMatrix.reset();
  ctxI->internalState.savedStateCount = 0;

  // Initialize private state.
  ctxI->internalState.metaMatrixType = BL_MATRIX2D_TYPE_TRANSLATE;
  ctxI->internalState.finalMatrixType = BL_MATRIX2D_TYPE_TRANSLATE;
  ctxI->internalState.metaMatrixFixedType = BL_MATRIX2D_TYPE_SCALE;
  ctxI->internalState.finalMatrixFixedType = BL_MATRIX2D_TYPE_SCALE;
  ctxI->internalState.globalAlphaI = uint32_t(ctxI->renderTargetInfo.fullAlphaI);

  ctxI->internalState.finalMatrix.reset();
  ctxI->internalState.metaMatrixFixed.resetToScaling(ctxI->renderTargetInfo.fpScaleD);
  ctxI->internalState.finalMatrixFixed.resetToScaling(ctxI->renderTargetInfo.fpScaleD);
  ctxI->internalState.translationI.reset(0, 0);

  ctxI->internalState.metaClipBoxI.reset(0, 0, size.w, size.h);
  // `finalClipBoxI` and `finalClipBoxD` are initialized by `resetClippingToMetaClipBox()`.

  // Make sure the state is initialized properly.
  onAfterCompOpChanged(ctxI);
  onAfterFlattenToleranceChanged(ctxI);
  onAfterOffsetParameterChanged(ctxI);
  resetClippingToMetaClipBox(ctxI);

  // Initialize styles.
  initStyleToDefault(ctxI, ctxI->internalState.style[0], uint32_t(ctxI->renderTargetInfo.fullAlphaI));
  initStyleToDefault(ctxI, ctxI->internalState.style[1], uint32_t(ctxI->renderTargetInfo.fullAlphaI));

  return BL_SUCCESS;
}

static BLResult blRasterContextImplDetach(BLRasterContextImpl* ctxI) noexcept {
  // Release the ImageImpl.
  BLImagePrivateImpl* imageI = BLImagePrivate::getImpl(&ctxI->dstImage);
  BL_ASSERT(imageI != nullptr);

  blRasterContextImplFlush(ctxI, BL_CONTEXT_FLUSH_SYNC);

  // Release Threads/WorkerContexts used by asynchronous rendering.
  if (ctxI->workerMgrInitialized)
    ctxI->workerMgr->reset();

  // Release PipeRuntime.
  if (blTestFlag(ctxI->pipeProvider.runtime()->runtimeFlags(), BLPipeline::PipeRuntimeFlags::kIsolated))
    ctxI->pipeProvider.runtime()->destroy();
  ctxI->pipeProvider.reset();

  // Release all states.
  //
  // Important as the user doesn't have to restore all states, in that case we basically need to iterate
  // over all of them and release resources they hold.
  blRasterContextImplDiscardStates(ctxI, nullptr);
  blCallDtor(ctxI->internalState.strokeOptions);

  uint32_t contextFlags = ctxI->contextFlags;
  if (contextFlags & BL_RASTER_CONTEXT_FILL_FETCH_DATA)
    destroyValidStyle(ctxI, &ctxI->internalState.style[BL_CONTEXT_OP_TYPE_FILL]);

  if (contextFlags & BL_RASTER_CONTEXT_STROKE_FETCH_DATA)
    destroyValidStyle(ctxI, &ctxI->internalState.style[BL_CONTEXT_OP_TYPE_STROKE]);

  // Clear other important members. We don't have to clear everything as if we re-attach an image again
  // all members will be overwritten anyway.
  ctxI->contextFlags = 0;

  ctxI->baseZone.clear();
  ctxI->fetchDataPool.reset();
  ctxI->savedStatePool.reset();
  ctxI->syncWorkData.ctxData.reset();
  ctxI->syncWorkData.workZone.clear();

  // If the image was dereferenced during rendering it's our responsibility to destroy it. This is not useful
  // from the consumer's perspective as the resulting image can never be used again, but it can happen in some
  // cases (for example when an asynchronous rendering is terminated and the target image released with it).
  if (blAtomicFetchSubStrong(&imageI->writerCount) == 1)
    if (blObjectImplGetRefCount(imageI) == 0)
      BLImagePrivate::freeImpl(imageI, ctxI->dstImage._d.info);

  ctxI->dstImage._d.impl = nullptr;
  ctxI->dstData.reset();

  return BL_SUCCESS;
}

// BLRasterEngine - ContextImpl - Init & Destroy
// =============================================

BLResult blRasterContextInitImpl(BLContextCore* self, BLImageCore* image, const BLContextCreateInfo* options) noexcept {
  BLRasterContextImpl* ctxI = blObjectDetailAllocImplT<BLRasterContextImpl>(self, BLObjectInfo::packType(BL_OBJECT_TYPE_CONTEXT));
  if (BL_UNLIKELY(!ctxI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  blCallCtor(*ctxI, &rasterImplVirtSync);
  BLResult result = blRasterContextImplAttach(ctxI, image, options);

  if (result != BL_SUCCESS)
    ctxI->virt->base.destroy(ctxI, self->_d.info.bits);

  return result;
}

static BLResult BL_CDECL blRasterContextImplDestroy(BLObjectImpl* impl, uint32_t info) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(impl);

  if (ctxI->dstImage._d.impl)
    blRasterContextImplDetach(ctxI);

  ctxI->~BLRasterContextImpl();

  return blObjectDetailFreeImpl(ctxI, info);
}

// BLRasterEngine - ContextImpl - Virtual Function Table
// =====================================================

template<uint32_t RenderingMode>
static void blRasterContextVirtInit(BLContextVirt* virt) noexcept {
  constexpr uint32_t F = BL_CONTEXT_OP_TYPE_FILL;
  constexpr uint32_t S = BL_CONTEXT_OP_TYPE_STROKE;

  virt->base.destroy            = blRasterContextImplDestroy;
  virt->base.getProperty        = blRasterContextImplGetProperty;
  virt->base.setProperty        = blRasterContextImplSetProperty;
  virt->flush                   = blRasterContextImplFlush;

  virt->save                    = blRasterContextImplSave;
  virt->restore                 = blRasterContextImplRestore;

  virt->matrixOp                = blRasterContextImplMatrixOp;
  virt->userToMeta              = blRasterContextImplUserToMeta;

  virt->setHint                 = blRasterContextImplSetHint;
  virt->setHints                = blRasterContextImplSetHints;

  virt->setFlattenMode          = blRasterContextImplSetFlattenMode;
  virt->setFlattenTolerance     = blRasterContextImplSetFlattenTolerance;
  virt->setApproximationOptions = blRasterContextImplSetApproximationOptions;

  virt->setCompOp               = blRasterContextImplSetCompOp;
  virt->setGlobalAlpha          = blRasterContextImplSetGlobalAlpha;

  virt->setStyleAlpha[F]        = blRasterContextImplSetFillAlpha;
  virt->setStyleAlpha[S]        = blRasterContextImplSetStrokeAlpha;
  virt->getStyle[F]             = blRasterContextImplGetStyle<F>;
  virt->getStyle[S]             = blRasterContextImplGetStyle<S>;
  virt->setStyle[F]             = blRasterContextImplSetStyle<F>;
  virt->setStyle[S]             = blRasterContextImplSetStyle<S>;
  virt->setStyleRgba[F]         = blRasterContextImplSetStyleRgba<F>;
  virt->setStyleRgba[S]         = blRasterContextImplSetStyleRgba<S>;
  virt->setStyleRgba32[F]       = blRasterContextImplSetStyleRgba32<F>;
  virt->setStyleRgba32[S]       = blRasterContextImplSetStyleRgba32<S>;
  virt->setStyleRgba64[F]       = blRasterContextImplSetStyleRgba64<F>;
  virt->setStyleRgba64[S]       = blRasterContextImplSetStyleRgba64<S>;

  virt->setFillRule             = blRasterContextImplSetFillRule;

  virt->setStrokeWidth          = blRasterContextImplSetStrokeWidth;
  virt->setStrokeMiterLimit     = blRasterContextImplSetStrokeMiterLimit;
  virt->setStrokeCap            = blRasterContextImplSetStrokeCap;
  virt->setStrokeCaps           = blRasterContextImplSetStrokeCaps;
  virt->setStrokeJoin           = blRasterContextImplSetStrokeJoin;
  virt->setStrokeTransformOrder = blRasterContextImplSetStrokeTransformOrder;
  virt->setStrokeDashOffset     = blRasterContextImplSetStrokeDashOffset;
  virt->setStrokeDashArray      = blRasterContextImplSetStrokeDashArray;
  virt->setStrokeOptions        = blRasterContextImplSetStrokeOptions;

  virt->clipToRectI             = blRasterContextImplClipToRectI;
  virt->clipToRectD             = blRasterContextImplClipToRectD;
  virt->restoreClipping         = blRasterContextImplRestoreClipping;

  virt->clearAll                = blRasterContextImplClearAll<RenderingMode>;
  virt->clearRectI              = blRasterContextImplClearRectI<RenderingMode>;
  virt->clearRectD              = blRasterContextImplClearRectD<RenderingMode>;

  virt->fillAll                 = blRasterContextImplFillAll<RenderingMode>;
  virt->fillRectI               = blRasterContextImplFillRectI<RenderingMode>;
  virt->fillRectD               = blRasterContextImplFillRectD<RenderingMode>;
  virt->fillPathD               = blRasterContextImplFillPathD<RenderingMode>;
  virt->fillGeometry            = blRasterContextImplFillGeometry<RenderingMode>;
  virt->fillTextI               = blRasterContextImplFillTextI<RenderingMode>;
  virt->fillTextD               = blRasterContextImplFillTextD<RenderingMode>;
  virt->fillGlyphRunI           = blRasterContextImplFillGlyphRunI<RenderingMode>;
  virt->fillGlyphRunD           = blRasterContextImplFillGlyphRunD<RenderingMode>;
  virt->fillMaskI               = blRasterContextImplFillMaskI<RenderingMode>;
  virt->fillMaskD               = blRasterContextImplFillMaskD<RenderingMode>;

  virt->strokeRectI             = blRasterContextImplStrokeRectI<RenderingMode>;
  virt->strokeRectD             = blRasterContextImplStrokeRectD<RenderingMode>;
  virt->strokePathD             = blRasterContextImplStrokePathD<RenderingMode>;
  virt->strokeGeometry          = blRasterContextImplStrokeGeometry<RenderingMode>;
  virt->strokeTextI             = blRasterContextImplStrokeTextI<RenderingMode>;
  virt->strokeTextD             = blRasterContextImplStrokeTextD<RenderingMode>;
  virt->strokeGlyphRunI         = blRasterContextImplStrokeGlyphRunI<RenderingMode>;
  virt->strokeGlyphRunD         = blRasterContextImplStrokeGlyphRunD<RenderingMode>;

  virt->blitImageI              = blRasterContextImplBlitImageI<RenderingMode>;
  virt->blitImageD              = blRasterContextImplBlitImageD<RenderingMode>;
  virt->blitScaledImageI        = blRasterContextImplBlitScaledImageI<RenderingMode>;
  virt->blitScaledImageD        = blRasterContextImplBlitScaledImageD<RenderingMode>;
}

// BLRasterEngine - ContextImpl - Runtime Registration
// ===================================================

void blRasterContextOnInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  blRasterContextVirtInit<BL_RASTER_RENDERING_MODE_SYNC>(&rasterImplVirtSync);
  blRasterContextVirtInit<BL_RASTER_RENDERING_MODE_ASYNC>(&rasterImplVirtAsync);
}
