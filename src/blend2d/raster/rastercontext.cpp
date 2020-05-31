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

#include "../api-build_p.h"
#include "../compop_p.h"
#include "../font_p.h"
#include "../format_p.h"
#include "../geometry_p.h"
#include "../image_p.h"
#include "../path_p.h"
#include "../pathstroke_p.h"
#include "../pattern_p.h"
#include "../piperuntime_p.h"
#include "../pixelops_p.h"
#include "../runtime_p.h"
#include "../string_p.h"
#include "../style_p.h"
#include "../support_p.h"
#include "../zeroallocator_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rastercommand_p.h"
#include "../raster/rastercommandprocsync_p.h"
#include "../raster/rastercommandserializer_p.h"
#include "../raster/rastercontext_p.h"
#include "../raster/rastercontextops_p.h"
#include "../raster/rasterworkproc_p.h"

#ifndef BL_BUILD_NO_FIXED_PIPE
  #include "../fixedpipe/fixedpiperuntime_p.h"
#endif

#ifndef BL_BUILD_NO_JIT
  #include "../pipegen/pipegenruntime_p.h"
#endif

// ============================================================================
// [BLRasterContext - Globals]
// ============================================================================

static BLContextVirt blRasterContextSyncVirt;
static BLContextVirt blRasterContextAsyncVirt;

// ============================================================================
// [BLRasterContext - Tables]
// ============================================================================

struct alignas(8) UInt32x2 {
  uint32_t first;
  uint32_t second;
};

static const UInt32x2 blRasterContextSolidDataRgba32[] = {
  { 0x00000000u, 0x0u }, // BL_COMP_OP_SOLID_ID_NONE (not solid, not used).
  { 0x00000000u, 0x0u }, // BL_COMP_OP_SOLID_ID_TRANSPARENT.
  { 0xFF000000u, 0x0u }, // BL_COMP_OP_SOLID_ID_OPAQUE_BLACK.
  { 0xFFFFFFFFu, 0x0u }  // BL_COMP_OP_SOLID_ID_OPAQUE_WHITE.
};

static const uint64_t blRasterContextSolidDataRgba64[] = {
  0x0000000000000000u,   // BL_COMP_OP_SOLID_ID_NONE (not solid, not used).
  0x0000000000000000u,   // BL_COMP_OP_SOLID_ID_TRANSPARENT.
  0xFFFF000000000000u,   // BL_COMP_OP_SOLID_ID_OPAQUE_BLACK.
  0xFFFFFFFFFFFFFFFFu    // BL_COMP_OP_SOLID_ID_OPAQUE_WHITE.
};

static const BLRasterContextPrecisionInfo blRasterContextPrecisionInfo[] = {
  #define ROW(FormatPrecision, FpBits, FullAlpha) {   \
    BL_RASTER_CONTEXT_##FormatPrecision,              \
    0,                                                \
                                                      \
    uint16_t(FullAlpha),                              \
    int(FpBits),                                      \
    int(1 << FpBits),                                 \
    int((1 << FpBits) - 1),                           \
                                                      \
    double(FullAlpha),                                \
    double(1 << FpBits)                               \
  }

  ROW(FORMAT_PRECISION_8BPC, 8, 255),
  ROW(FORMAT_PRECISION_16BPC, 16, 65535),
  ROW(FORMAT_PRECISION_FLOAT, 16, 1.0),

  #undef ROW
};

static const uint8_t blTextByteSizeShift[] = { 0, 1, 2, 0 };

enum BLRasterCommandCategory : uint32_t {
  BL_RASTER_COMMAND_CATEGORY_CORE,
  BL_RASTER_COMMAND_CATEGORY_BLIT
};

// ============================================================================
// [BLRasterContext - StateAccessor]
// ============================================================================

class BLRasterContextStateAccessor {
public:
  const BLRasterContextImpl* ctxI;

  explicit BL_INLINE BLRasterContextStateAccessor(const BLRasterContextImpl* ctxI) noexcept : ctxI(ctxI) {}

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

// ============================================================================
// [BLRasterContext - Pipeline Lookup]
// ============================================================================

static BL_INLINE BLPipeFillFunc blRasterContextImplGetFillFunc(BLRasterContextImpl* ctxI, uint32_t signature) noexcept {
  BLPipeLookupCache::MatchType m = ctxI->pipeLookupCache.match(signature);
  return m.isValid() ? ctxI->pipeLookupCache.getMatch<BLPipeFillFunc>(m)
                     : ctxI->pipeProvider.get(signature, &ctxI->pipeLookupCache);
}

// ============================================================================
// [BLRasterContext - Core State Internals]
// ============================================================================

static BL_INLINE void blRasterContextImplBeforeConfigChange(BLRasterContextImpl* ctxI) noexcept {
  if (ctxI->contextFlags & BL_RASTER_CONTEXT_STATE_CONFIG) {
    BLRasterContextSavedState* state = ctxI->savedState;
    state->approximationOptions = ctxI->approximationOptions();
  }
}

static BL_INLINE void blRasterContextImplCompOpChanged(BLRasterContextImpl* ctxI) noexcept {
  ctxI->compOpSimplifyInfo = blCompOpSimplifyInfoArrayOf(ctxI->compOp(), ctxI->format());
}

static BL_INLINE void blRasterContextImplFlattenToleranceChanged(BLRasterContextImpl* ctxI) noexcept {
  ctxI->internalState.toleranceFixedD = ctxI->approximationOptions().flattenTolerance * ctxI->precisionInfo.fpScaleD;
  ctxI->syncWorkData.edgeBuilder.setFlattenToleranceSq(blSquare(ctxI->internalState.toleranceFixedD));
}

static BL_INLINE void blRasterContextImplOffsetParameterChanged(BLRasterContextImpl* ctxI) noexcept {
  blUnused(ctxI);
}

// ============================================================================
// [BLRasterContext - Style State Internals]
// ============================================================================

static BL_INLINE void blRasterContextInitStyleToDefault(BLRasterContextImpl* ctxI, BLRasterContextStyleData& style, uint32_t alphaI) noexcept {
  style.packed = 0;
  style.styleType = BL_STYLE_TYPE_SOLID;
  style.styleFormat = ctxI->solidFormatTable[BL_RASTER_CONTEXT_SOLID_FORMAT_FRGB];
  style.alphaI = alphaI;
  style.source.solid = ctxI->solidFetchDataTable[BL_COMP_OP_SOLID_ID_OPAQUE_BLACK];
  style.rgba64.value = 0xFFFF000000000000u;
  style.adjustedMatrix.reset();
}

static BL_INLINE void blRasterContextImplDestroyValidStyle(BLRasterContextImpl* ctxI, BLRasterContextStyleData* style) noexcept {
  style->source.fetchData->release(ctxI);
}

static BL_INLINE void blRasterContextBeforeStyleChange(BLRasterContextImpl* ctxI, uint32_t opType, BLRasterContextStyleData* style) noexcept {
  uint32_t contextFlags = ctxI->contextFlags;
  BLRasterFetchData* fetchData = style->source.fetchData;

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
  BLRasterContextStyleData* stateStyle = &ctxI->savedState->style[opType];

  // The content is moved to the `stateStyle`, so it doesn't matter if it
  // contains solid, gradient, or pattern as the state uses the same layout.
  stateStyle->packed = style->packed;
  // `stateStyle->alpha` has been already set by `BLRasterContextImpl::save()`.
  stateStyle->source = style->source;
  stateStyle->rgba64 = style->rgba64;
  stateStyle->adjustedMatrix.reset();
}

template<uint32_t kOpType>
static BLResult BL_CDECL blRasterContextImplGetStyle(const BLContextImpl* baseImpl, BLStyleCore* styleOut) noexcept {
  blStyleDestroyInline(styleOut);

  const BLRasterContextImpl* ctxI = static_cast<const BLRasterContextImpl*>(baseImpl);
  const BLRasterContextStyleData* style = &ctxI->internalState.style[kOpType];

  BLRasterFetchData* fetchData = style->source.fetchData;

  // NOTE: We just set the values as we get them in SetStyle to not spend much
  // time by normalizing the input into floats. So we have to check the tag to
  // actually get the right value from the style. It should be okay as we don't
  // expect GetStyle() be called more than SetStyle().
  switch (style->styleType) {
    case BL_STYLE_TYPE_SOLID: {
      uint32_t tag = style->tagging.tag;
      if (tag == blBitCast<uint32_t>(blNaN<float>()) + 0)
        blDownCast(styleOut)->rgba.reset(style->rgba32);
      else if (tag == blBitCast<uint32_t>(blNaN<float>()) + 1)
        blDownCast(styleOut)->rgba.reset(style->rgba64);
      else
        blDownCast(styleOut)->rgba.reset(style->rgba);
      return BL_SUCCESS;
    }

    case BL_STYLE_TYPE_PATTERN: {
      BLPatternCore pattern;
      BLResult result = blPatternInitAs(&pattern, &fetchData->_image, &style->imageArea, fetchData->_extendMode, &style->adjustedMatrix);

      blStyleInitObjectInline(styleOut, reinterpret_cast<BLVariantImpl*>(pattern.impl), BL_STYLE_TYPE_PATTERN);
      return result;
    }

    case BL_STYLE_TYPE_GRADIENT: {
      BLGradientCore gradient;
      blVariantInitWeak(&gradient, &fetchData->_gradient);
      BLResult result = blGradientApplyMatrixOp(&gradient, BL_MATRIX2D_OP_ASSIGN, &style->adjustedMatrix);

      blStyleInitObjectInline(styleOut, reinterpret_cast<BLVariantImpl*>(gradient.impl), BL_STYLE_TYPE_GRADIENT);
      return result;
    }

    default:
      return blStyleInitNoneInline(styleOut);
  }
}

template<uint32_t kOpType>
static BL_INLINE BLResult blRasterContextImplSetStyleNone(BLRasterContextImpl* ctxI) noexcept {
  BLRasterContextStyleData* style = &ctxI->internalState.style[kOpType];

  uint32_t contextFlags = ctxI->contextFlags;
  uint32_t styleFlags = (BL_RASTER_CONTEXT_STATE_BASE_STYLE | BL_RASTER_CONTEXT_BASE_FETCH_DATA) << kOpType;

  if (contextFlags & styleFlags)
    blRasterContextBeforeStyleChange(ctxI, kOpType, style);

  contextFlags &= ~(styleFlags << kOpType);
  contextFlags |= BL_RASTER_CONTEXT_NO_BASE_STYLE << kOpType;

  ctxI->contextFlags = contextFlags;
  ctxI->internalState.styleType[kOpType] = uint8_t(BL_STYLE_TYPE_NONE);

  style->packed = 0;
  return BL_SUCCESS;
}

template<uint32_t kOpType>
static BLResult BL_CDECL blRasterContextImplSetStyleRgba(BLContextImpl* baseImpl, const BLRgba* rgba) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLRasterContextStyleData* style = &ctxI->internalState.style[kOpType];

  BLRgba solid = *rgba;
  if (!blStyleIsValidRgba(solid))
    return blRasterContextImplSetStyleNone<kOpType>(ctxI);

  uint32_t contextFlags = ctxI->contextFlags;
  uint32_t styleFlags = (BL_RASTER_CONTEXT_STATE_BASE_STYLE | BL_RASTER_CONTEXT_BASE_FETCH_DATA) << kOpType;

  if (contextFlags & styleFlags)
    blRasterContextBeforeStyleChange(ctxI, kOpType, style);

  contextFlags &= ~(styleFlags | (BL_RASTER_CONTEXT_NO_BASE_STYLE << kOpType));
  solid = blClamp(solid, BLRgba(0.0f, 0.0f, 0.0f, 0.0f),
                         BLRgba(1.0f, 1.0f, 1.0f, 1.0f));
  style->rgba = solid;

  // Premultiply and convert to RGBA32.
  float aScale = solid.a * 255.0f;
  uint32_t r = uint32_t(blRoundToInt(solid.r * aScale));
  uint32_t g = uint32_t(blRoundToInt(solid.g * aScale));
  uint32_t b = uint32_t(blRoundToInt(solid.b * aScale));
  uint32_t a = uint32_t(blRoundToInt(aScale));
  uint32_t rgba32 = BLRgba32(r, g, b, a).value;

  uint32_t solidFormatIndex = BL_RASTER_CONTEXT_SOLID_FORMAT_FRGB;
  if (!blRgba32IsFullyOpaque(rgba32))
    solidFormatIndex = BL_RASTER_CONTEXT_SOLID_FORMAT_ARGB;
  if (!rgba)
    solidFormatIndex = BL_RASTER_CONTEXT_SOLID_FORMAT_ZERO;

  ctxI->contextFlags = contextFlags;
  ctxI->internalState.styleType[kOpType] = uint8_t(BL_STYLE_TYPE_SOLID);

  style->packed = 0;
  style->styleType = uint8_t(BL_STYLE_TYPE_SOLID);
  style->styleFormat = ctxI->solidFormatTable[solidFormatIndex];
  style->source.solid.prgb32 = rgba32;
  return BL_SUCCESS;
}

template<uint32_t kOpType>
static BLResult BL_CDECL blRasterContextImplSetStyleRgba32(BLContextImpl* baseImpl, uint32_t rgba32) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLRasterContextStyleData* style = &ctxI->internalState.style[kOpType];

  uint32_t contextFlags = ctxI->contextFlags;
  uint32_t styleFlags = (BL_RASTER_CONTEXT_STATE_BASE_STYLE | BL_RASTER_CONTEXT_BASE_FETCH_DATA) << kOpType;

  if (contextFlags & styleFlags)
    blRasterContextBeforeStyleChange(ctxI, kOpType, style);

  style->rgba32.value = rgba32;
  style->tagging.tag = blBitCast<uint32_t>(blNaN<float>()) + 0;

  uint32_t solidFormatIndex = BL_RASTER_CONTEXT_SOLID_FORMAT_FRGB;
  if (!blRgba32IsFullyOpaque(rgba32)) {
    rgba32 = BLPixelOps::prgb32_8888_from_argb32_8888(rgba32);
    solidFormatIndex = rgba32 == 0 ? BL_RASTER_CONTEXT_SOLID_FORMAT_ZERO
                                   : BL_RASTER_CONTEXT_SOLID_FORMAT_ARGB;
  }

  ctxI->contextFlags = contextFlags & ~(styleFlags | (BL_RASTER_CONTEXT_NO_BASE_STYLE << kOpType));
  ctxI->internalState.styleType[kOpType] = uint8_t(BL_STYLE_TYPE_SOLID);

  style->packed = 0;
  style->styleType = uint8_t(BL_STYLE_TYPE_SOLID);
  style->styleFormat = ctxI->solidFormatTable[solidFormatIndex];
  style->source.solid.prgb32 = rgba32;

  return BL_SUCCESS;
}

template<uint32_t kOpType>
static BLResult BL_CDECL blRasterContextImplSetStyleRgba64(BLContextImpl* baseImpl, uint64_t rgba64) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLRasterContextStyleData* style = &ctxI->internalState.style[kOpType];

  uint32_t contextFlags = ctxI->contextFlags;
  uint32_t styleFlags = (BL_RASTER_CONTEXT_STATE_BASE_STYLE | BL_RASTER_CONTEXT_BASE_FETCH_DATA) << kOpType;

  if (contextFlags & styleFlags)
    blRasterContextBeforeStyleChange(ctxI, kOpType, style);

  style->rgba64.value = rgba64;
  style->tagging.tag = blBitCast<uint32_t>(blNaN<float>()) + 1;

  uint32_t solidFormatIndex = BL_RASTER_CONTEXT_SOLID_FORMAT_FRGB;
  uint32_t rgba32 = blRgba32FromRgba64(rgba64);

  if (!blRgba32IsFullyOpaque(rgba32)) {
    rgba32 = BLPixelOps::prgb32_8888_from_argb32_8888(rgba32);
    solidFormatIndex = rgba32 == 0 ? BL_RASTER_CONTEXT_SOLID_FORMAT_ZERO
                                   : BL_RASTER_CONTEXT_SOLID_FORMAT_ARGB;
  }

  ctxI->contextFlags = contextFlags & ~(styleFlags | (BL_RASTER_CONTEXT_NO_BASE_STYLE << kOpType));
  ctxI->internalState.styleType[kOpType] = uint8_t(BL_STYLE_TYPE_SOLID);

  style->packed = 0;
  style->styleType = uint8_t(BL_STYLE_TYPE_SOLID);
  style->styleFormat = ctxI->solidFormatTable[solidFormatIndex];
  style->source.solid.prgb32 = rgba32;

  return BL_SUCCESS;
}

template<uint32_t kOpType>
static BLResult BL_CDECL blRasterContextImplSetStyleObject(BLContextImpl* baseImpl, const void* object) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLRasterContextStyleData* style = &ctxI->internalState.style[kOpType];

  uint32_t contextFlags = ctxI->contextFlags;
  uint32_t styleFlags = (BL_RASTER_CONTEXT_BASE_FETCH_DATA | BL_RASTER_CONTEXT_STATE_BASE_STYLE) << kOpType;

  BLVariantImpl* varI = static_cast<const BLVariant*>(object)->impl;
  const BLMatrix2D* srcMatrix = nullptr;
  uint32_t srcMatrixType = BL_MATRIX2D_TYPE_IDENTITY;

  switch (varI->implType) {
    case BL_IMPL_TYPE_GRADIENT: {
      BLGradientImpl* gradientI = reinterpret_cast<BLGradientImpl*>(varI);
      BLRasterFetchData* fetchData = ctxI->allocFetchData();

      if (BL_UNLIKELY(!fetchData))
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

      if (contextFlags & styleFlags)
        blRasterContextBeforeStyleChange(ctxI, kOpType, style);

      contextFlags &= ~(styleFlags | (BL_RASTER_CONTEXT_NO_BASE_STYLE << kOpType));
      styleFlags = BL_RASTER_CONTEXT_BASE_FETCH_DATA;

      fetchData->initGradientSource(blImplIncRef(gradientI));
      fetchData->_extendMode = gradientI->extendMode;

      style->packed = 0;
      style->source.reset();

      BLGradientInfo gradientInfo = blGradientImplEnsureInfo32(gradientI);
      if (gradientInfo.empty()) {
        styleFlags |= BL_RASTER_CONTEXT_NO_BASE_STYLE;
      }
      else if (gradientInfo.solid) {
        // Using last color according to the SVG specification.
        uint32_t rgba32 = BLPixelOps::prgb32_8888_from_argb32_8888(blRgba32FromRgba64(gradientI->stops[gradientI->size - 1].rgba.value));
        fetchData->_isSetup = true;
        fetchData->_fetchFormat = gradientInfo.format;
        fetchData->_data.solid.prgb32 = rgba32;
      }

      srcMatrix = &gradientI->matrix;
      srcMatrixType = gradientI->matrixType;

      ctxI->internalState.styleType[kOpType] = uint8_t(BL_STYLE_TYPE_GRADIENT);
      style->cmdFlags |= BL_RASTER_COMMAND_FLAG_FETCH_DATA;
      style->styleType = uint8_t(BL_STYLE_TYPE_GRADIENT);
      style->styleFormat = gradientInfo.format;
      style->quality = ctxI->hints().gradientQuality;
      style->source.fetchData = fetchData;
      break;
    }

    case BL_IMPL_TYPE_PATTERN: {
      BLPatternImpl* patternI = reinterpret_cast<BLPatternImpl*>(varI);
      BLImageImpl* imgI = patternI->image.impl;
      BLRasterFetchData* fetchData = ctxI->allocFetchData();

      if (BL_UNLIKELY(!fetchData))
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

      if (contextFlags & styleFlags)
        blRasterContextBeforeStyleChange(ctxI, kOpType, style);

      contextFlags &= ~(styleFlags | (BL_RASTER_CONTEXT_NO_BASE_STYLE << kOpType));
      styleFlags = BL_RASTER_CONTEXT_BASE_FETCH_DATA;

      // NOTE: The area comes from pattern, it means that it's the pattern's
      // responsibility to make sure that it's not out of bounds. One special
      // case is area having all zeros [0, 0, 0, 0], which signalizes to use
      // the whole image.
      BLRectI area = patternI->area;
      if (!blPatternIsAreaValidAndNonZero(area, imgI->size))
        area.reset(0, 0, imgI->size.w, imgI->size.h);

      fetchData->initPatternSource(blImplIncRef(imgI), area);
      fetchData->_extendMode = patternI->extendMode;

      style->packed = 0;
      style->imageArea = area;

      if (BL_UNLIKELY(!area.w)) {
        styleFlags |= BL_RASTER_CONTEXT_NO_BASE_STYLE;
      }

      srcMatrix = &patternI->matrix;
      srcMatrixType = patternI->matrixType;

      ctxI->internalState.styleType[kOpType] = uint8_t(BL_STYLE_TYPE_PATTERN);
      style->cmdFlags |= BL_RASTER_COMMAND_FLAG_FETCH_DATA;
      style->styleType = uint8_t(BL_STYLE_TYPE_PATTERN);
      style->styleFormat = uint8_t(patternI->image.format());
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
    blMatrix2DMultiply(style->adjustedMatrix, *srcMatrix, ctxI->finalMatrix());
    adjustedMatrixType = style->adjustedMatrix.type();
  }

  if (BL_UNLIKELY(adjustedMatrixType >= BL_MATRIX2D_TYPE_INVALID))
    styleFlags |= BL_RASTER_CONTEXT_NO_BASE_STYLE;

  ctxI->contextFlags = contextFlags | (styleFlags << kOpType);
  style->adjustedMatrixType = uint8_t(adjustedMatrixType);

  return BL_SUCCESS;
}

template<uint32_t kOpType>
static BLResult BL_CDECL blRasterContextImplSetStyle(BLContextImpl* baseImpl, const BLStyleCore* style) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (!blDownCast(style)->_isTagged())
    return blRasterContextImplSetStyleRgba<kOpType>(ctxI, &style->rgba);

  uint32_t styleType = style->data.type;
  switch (styleType) {
    case BL_STYLE_TYPE_NONE:
      return blRasterContextImplSetStyleNone<kOpType>(ctxI);

    case BL_STYLE_TYPE_PATTERN:
    case BL_STYLE_TYPE_GRADIENT:
      return blRasterContextImplSetStyleObject<kOpType>(ctxI, &style->variant);

    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }
}

// ============================================================================
// [BLRasterContext - Stroke State Internals]
// ============================================================================

static BL_INLINE void blRasterContextImplBeforeStrokeChange(BLRasterContextImpl* ctxI) noexcept {
  if (ctxI->contextFlags & BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS) {
    BLRasterContextSavedState* state = ctxI->savedState;
    memcpy(&state->strokeOptions, &ctxI->strokeOptions(), sizeof(BLStrokeOptionsCore));
    blImplIncRef(state->strokeOptions.dashArray.impl);
  }
}

// ============================================================================
// [BLRasterContext - Matrix State Internals]
// ============================================================================

// Called before `userMatrix` is changed.
//
// This function is responsible for saving the current userMatrix in case that
// the `BL_RASTER_CONTEXT_STATE_USER_MATRIX` flag is set, which means that the
// userMatrix must be saved before any modification.
static BL_INLINE void blRasterContextImplBeforeUserMatrixChange(BLRasterContextImpl* ctxI) noexcept {
  if ((ctxI->contextFlags & BL_RASTER_CONTEXT_STATE_USER_MATRIX) != 0) {
    // MetaMatrix change would also save UserMatrix, no way this could be unset.
    BL_ASSERT((ctxI->contextFlags & BL_RASTER_CONTEXT_STATE_META_MATRIX) != 0);

    BLRasterContextSavedState* state = ctxI->savedState;
    state->altMatrix = ctxI->finalMatrix();
    state->userMatrix = ctxI->userMatrix();
  }
}

static BL_INLINE void blRasterContextImplUpdateFinalMatrix(BLRasterContextImpl* ctxI) noexcept {
  blMatrix2DMultiply(ctxI->internalState.finalMatrix, ctxI->userMatrix(), ctxI->metaMatrix());
}

static BL_INLINE void blRasterContextImplUpdateMetaMatrixFixed(BLRasterContextImpl* ctxI) noexcept {
  ctxI->internalState.metaMatrixFixed = ctxI->metaMatrix();
  ctxI->internalState.metaMatrixFixed.postScale(ctxI->precisionInfo.fpScaleD);
}

static BL_INLINE void blRasterContextImplUpdateFinalMatrixFixed(BLRasterContextImpl* ctxI) noexcept {
  ctxI->internalState.finalMatrixFixed = ctxI->finalMatrix();
  ctxI->internalState.finalMatrixFixed.postScale(ctxI->precisionInfo.fpScaleD);
}

// Called after `userMatrix` has been modified.
//
// Responsible for updating `finalMatrix` and other matrix information.
static BL_INLINE void blRasterContextUserMatrixChanged(BLRasterContextImpl* ctxI) noexcept {
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_NO_USER_MATRIX         |
                          BL_RASTER_CONTEXT_INTEGRAL_TRANSLATION   |
                          BL_RASTER_CONTEXT_STATE_USER_MATRIX      |
                          BL_RASTER_CONTEXT_SHARED_FILL_STATE      |
                          BL_RASTER_CONTEXT_SHARED_STROKE_EXT_STATE);

  blRasterContextImplUpdateFinalMatrix(ctxI);
  blRasterContextImplUpdateFinalMatrixFixed(ctxI);

  const BLMatrix2D& fm = ctxI->finalMatrixFixed();
  uint32_t finalMatrixType = ctxI->finalMatrix().type();

  ctxI->internalState.finalMatrixType = uint8_t(finalMatrixType);
  ctxI->internalState.finalMatrixFixedType = uint8_t(blMax<uint32_t>(finalMatrixType, BL_MATRIX2D_TYPE_SCALE));

  if (finalMatrixType <= BL_MATRIX2D_TYPE_TRANSLATE) {
    // No scaling - input coordinates have pixel granularity. Check if the
    // translation has pixel granularity as well and setup the `translationI`
    // data for that case.
    if (fm.m20 >= ctxI->fpMinSafeCoordD && fm.m20 <= ctxI->fpMaxSafeCoordD &&
        fm.m21 >= ctxI->fpMinSafeCoordD && fm.m21 <= ctxI->fpMaxSafeCoordD) {
      // We need 64-bit ints here as we are already scaled. We also need a
      // `floor` function as we have to handle negative translations which
      // cannot be truncated (the default conversion).
      int64_t tx64 = blFloorToInt64(fm.m20);
      int64_t ty64 = blFloorToInt64(fm.m21);

      // Pixel to pixel translation is only possible when both fixed points
      // `tx64` and `ty64` have all zeros in their fraction parts.
      if (((tx64 | ty64) & ctxI->precisionInfo.fpMaskI) == 0) {
        int tx = int(tx64 >> ctxI->precisionInfo.fpShiftI);
        int ty = int(ty64 >> ctxI->precisionInfo.fpShiftI);

        ctxI->setTranslationI(BLPointI(tx, ty));
        ctxI->contextFlags |= BL_RASTER_CONTEXT_INTEGRAL_TRANSLATION;
      }
    }
  }
}

// ============================================================================
// [BLRasterContext - Clip State Internals]
// ============================================================================

static BL_INLINE void blRasterContextImplBeforeClipBoxChange(BLRasterContextImpl* ctxI) noexcept {
  if (ctxI->contextFlags & BL_RASTER_CONTEXT_STATE_CLIP) {
    BLRasterContextSavedState* state = ctxI->savedState;
    state->finalClipBoxD = ctxI->finalClipBoxD();
  }
}

static BL_INLINE void blRasterContextImplResetClippingToMetaClipBox(BLRasterContextImpl* ctxI) noexcept {
  const BLBoxI& meta = ctxI->metaClipBoxI();
  ctxI->internalState.finalClipBoxI.reset(meta.x0, meta.y0, meta.x1, meta.y1);
  ctxI->internalState.finalClipBoxD.reset(meta.x0, meta.y0, meta.x1, meta.y1);
  ctxI->setFinalClipBoxFixedD(ctxI->finalClipBoxD() * ctxI->precisionInfo.fpScaleD);
}

static BL_INLINE void blRasterContextImplRestoreClippingFromState(BLRasterContextImpl* ctxI, BLRasterContextSavedState* savedState) noexcept {
  // TODO: [Rendering Context] Path-based clipping.
  ctxI->internalState.finalClipBoxD = savedState->finalClipBoxD;
  ctxI->internalState.finalClipBoxI.reset(
    blTruncToInt(ctxI->finalClipBoxD().x0),
    blTruncToInt(ctxI->finalClipBoxD().y0),
    blCeilToInt(ctxI->finalClipBoxD().x1),
    blCeilToInt(ctxI->finalClipBoxD().y1));

  double fpScale = ctxI->precisionInfo.fpScaleD;
  ctxI->setFinalClipBoxFixedD(BLBox(
    ctxI->finalClipBoxD().x0 * fpScale,
    ctxI->finalClipBoxD().y0 * fpScale,
    ctxI->finalClipBoxD().x1 * fpScale,
    ctxI->finalClipBoxD().y1 * fpScale));
}

// ============================================================================
// [BLRasterContext - Fill Internals - Prepare]
// ============================================================================

template<uint32_t RenderingMode>
static BL_INLINE uint32_t blRasterContextImplPrepareClear(BLRasterContextImpl* ctxI, BLRasterCoreCommandSerializer<RenderingMode>& serializer, uint32_t nopFlags) noexcept {
  BLCompOpSimplifyInfo simplifyInfo = blCompOpSimplifyInfo(BL_COMP_OP_CLEAR, ctxI->format(), BL_FORMAT_PRGB32);
  uint32_t contextFlags = ctxI->contextFlags;

  nopFlags &= contextFlags;
  if (nopFlags != 0)
    return BL_RASTER_CONTEXT_PREPARE_STATUS_NOP;

  // The combination of a destination format, source format, and compOp results
  // in a solid fill, so initialize the command accordingly to `solidId` type.
  serializer.initPipeline(simplifyInfo.signature());
  serializer.initCommand(ctxI->precisionInfo.fullAlphaI);
  serializer.initFetchSolid(ctxI->solidFetchDataTable[simplifyInfo.solidId()]);
  return BL_RASTER_CONTEXT_PREPARE_STATUS_SOLID;
}

template<uint32_t RenderingMode>
static BL_INLINE uint32_t blRasterContextImplPrepareFill(BLRasterContextImpl* ctxI, BLRasterCoreCommandSerializer<RenderingMode>& serializer, const BLRasterContextStyleData* styleData, uint32_t nopFlags) noexcept {
  BLCompOpSimplifyInfo simplifyInfo = ctxI->compOpSimplifyInfo[styleData->styleFormat];
  uint32_t solidId = simplifyInfo.solidId();
  uint32_t contextFlags = ctxI->contextFlags | solidId;

  serializer.initPipeline(simplifyInfo.signature());
  serializer.initCommand(styleData->alphaI);
  serializer.initFetchDataFromStyle(styleData);

  // Likely case - composition flag doesn't lead to a solid fill and there are no
  // other 'NO_' flags so the rendering of this command should produce something.
  //
  // This works since we combined `contextFlags` with `srcSolidId`, which is only
  // non-zero to force either NOP or SOLID fill.
  nopFlags &= contextFlags;
  if (nopFlags == 0)
    return BL_RASTER_CONTEXT_PREPARE_STATUS_FETCH;

  // Remove reserved flags we may have added to `nopFlags` if srcSolidId was
  // non-zero and add a possible condition flag to `nopFlags` if the composition
  // is NOP (DST-COPY).
  nopFlags &= ~BL_RASTER_CONTEXT_NO_RESERVED;
  nopFlags |= uint32_t(serializer.pipeSignature() == BLCompOpSimplifyInfo::dstCopy().signature());

  // The combination of a destination format, source format, and compOp results
  // in a solid fill, so initialize the command accordingly to `solidId` type.
  serializer.initFetchSolid(ctxI->solidFetchDataTable[solidId]);

  // If there are bits in `nopFlags` it means there is nothing to render as
  // compOp, style, alpha, or something else is nop/invalid.
  if (nopFlags != 0)
    return BL_RASTER_CONTEXT_PREPARE_STATUS_NOP;
  else
    return BL_RASTER_CONTEXT_PREPARE_STATUS_SOLID;
}

// ============================================================================
// [BLRasterContext - Fill Internals - Fetch Data]
// ============================================================================

template<uint32_t Category, uint32_t RenderingMode>
static BL_INLINE bool blRasterContextImplEnsureFetchData(BLRasterContextImpl* ctxI, BLRasterCoreCommandSerializer<RenderingMode>& serializer) noexcept {
  blUnused(ctxI);

  if (serializer.command().hasFetchData()) {
    BLRasterFetchData* fetchData = serializer.command()._source.fetchData;

    if (Category == BL_RASTER_COMMAND_CATEGORY_CORE) {
      if (!fetchData->isSetup() && !blRasterFetchDataSetup(fetchData, serializer.styleData()))
        return false;
    }

    if (Category == BL_RASTER_COMMAND_CATEGORY_BLIT) {
      BL_ASSERT(fetchData->isSetup());
    }

    serializer._pipeSignature.addFetchType(fetchData->_fetchType);
  }

  return true;
}

// ============================================================================
// [BLRasterContext - Async]
// ============================================================================

static BL_INLINE void blRasterContextImplReleaseFetchQueue(BLRasterContextImpl* ctxI, BLRasterFetchQueue* fetchQueue) noexcept {
  while (fetchQueue) {
    for (BLRasterFetchData* fetchData : *fetchQueue)
      fetchData->release(ctxI);
    fetchQueue = fetchQueue->next();
  }
}

static BL_NOINLINE BLResult blRasterContextImplFlushBatch(BLRasterContextImpl* ctxI) noexcept {
  BLRasterWorkerManager& mgr = ctxI->workerMgr();
  if (mgr.hasPendingCommands()) {
    mgr.finalizeBatch();

    uint32_t workerCount = mgr.workerCount();
    BLRasterWorkBatch* batch = mgr.currentBatch();

    BLRasterWorkSynchronization& synchronization = mgr._synchronization;
    batch->_synchronization = &synchronization;

    synchronization.jobsRunningCount = workerCount + 1;
    synchronization.threadsRunningCount = workerCount;

    for (uint32_t i = 0; i < workerCount; i++) {
      BLThread* thread = mgr._workerThreads[i];
      BLRasterWorkData* workData = mgr._workDataStorage[i];

      workData->batch = batch;
      workData->initContextData(ctxI->dstData);

      thread->run(blRasterWorkThreadEntry, blRasterWorkThreadDone, workData);
    }

    // User thread acts as a worker too.
    {
      BLRasterWorkData* workData = &ctxI->syncWorkData;
      workData->batch = batch;
      blRasterWorkProc(workData);
    }

    if (workerCount) {
      mgr._synchronization.waitForThreadsToFinish();
      ctxI->syncWorkData._accumulatedErrorFlags |= blAtomicFetch(&batch->_accumulatedErrorFlags, std::memory_order_relaxed);
    }

    blRasterContextImplReleaseFetchQueue(ctxI, batch->_fetchQueueList.first());
    mgr._allocator.clear();
    mgr.initFirstBatch();

    ctxI->syncWorkData.startOver();
    ctxI->contextFlags &= ~BL_RASTER_CONTEXT_SHARED_ALL_FLAGS;
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLRasterContext - Flush]
// ============================================================================

static BLResult BL_CDECL blRasterContextImplFlush(BLContextImpl* baseImpl, uint32_t flags) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  // Nothing to flush if the rendering context is synchronous.
  if (ctxI->isSync())
    return BL_SUCCESS;

  if (flags & BL_CONTEXT_FLUSH_SYNC) {
    BL_PROPAGATE(blRasterContextImplFlushBatch(ctxI));
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLRasterContext - Query Property]
// ============================================================================

static BLResult BL_CDECL blRasterContextImplQueryProperty(const BLContextImpl* baseImpl, uint32_t propertyId, void* valueOut) noexcept {
  const BLRasterContextImpl* ctxI = static_cast<const BLRasterContextImpl*>(baseImpl);

  switch (propertyId) {
    case BL_CONTEXT_PROPERTY_THREAD_COUNT:
      if (ctxI->isSync())
        *static_cast<uint32_t*>(valueOut) = 0;
      else
        *static_cast<uint32_t*>(valueOut) = ctxI->workerMgr().workerCount() + 1;
      return BL_SUCCESS;

    case BL_CONTEXT_PROPERTY_ACCUMULATED_ERROR_FLAGS:
      *static_cast<uint32_t*>(valueOut) = ctxI->syncWorkData.accumulatedErrorFlags();
      return BL_SUCCESS;

    default:
      *static_cast<uint32_t*>(valueOut) = 0;
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }
}

// ============================================================================
// [BLRasterContext - Save / Restore]
// ============================================================================

// Returns how many states have to be restored to match the `stateId`. It would
// return zero if there is no state that matches `stateId`.
static BL_INLINE uint32_t blRasterContextImplNumStatesToRestore(BLRasterContextSavedState* savedState, uint64_t stateId) noexcept {
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

// "CoreState" consists of states that are always saved and restored to make
// the restoration simpler. All states saved/restored by CoreState should be
// cheap to copy.
static BL_INLINE void blRasterContextImplSaveCoreState(BLRasterContextImpl* ctxI, BLRasterContextSavedState* state) noexcept {
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

static BL_INLINE void blRasterContextImplRestoreCoreState(BLRasterContextImpl* ctxI, BLRasterContextSavedState* state) noexcept {
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

  blRasterContextImplCompOpChanged(ctxI);
}

static void blRasterContextImplDiscardStates(BLRasterContextImpl* ctxI, BLRasterContextSavedState* topState) noexcept {
  BLRasterContextSavedState* savedState = ctxI->savedState;
  if (savedState == topState)
    return;

  // NOTE: No need to handle states that don't use dynamically allocated memory.
  uint32_t contextFlags = ctxI->contextFlags;
  do {
    if ((contextFlags & (BL_RASTER_CONTEXT_FILL_FETCH_DATA | BL_RASTER_CONTEXT_STATE_FILL_STYLE)) == BL_RASTER_CONTEXT_FILL_FETCH_DATA) {
      constexpr uint32_t kOpType = BL_CONTEXT_OP_TYPE_FILL;
      if (savedState->style[kOpType].hasFetchData()) {
        BLRasterFetchData* fetchData = savedState->style[kOpType].source.fetchData;
        fetchData->release(ctxI);
      }
    }

    if ((contextFlags & (BL_RASTER_CONTEXT_STROKE_FETCH_DATA | BL_RASTER_CONTEXT_STATE_STROKE_STYLE)) == BL_RASTER_CONTEXT_STROKE_FETCH_DATA) {
      constexpr uint32_t kOpType = BL_CONTEXT_OP_TYPE_STROKE;
      BLRasterFetchData* fetchData = savedState->style[kOpType].source.fetchData;
      fetchData->release(ctxI);
    }

    if ((contextFlags & BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS) == 0) {
      blCallDtor(savedState->strokeOptions.dashArray);
    }

    BLRasterContextSavedState* prevState = savedState->prevState;
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
  BLRasterContextSavedState* newState = ctxI->allocSavedState();

  if (BL_UNLIKELY(!newState))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  newState->prevState = ctxI->savedState;
  newState->stateId = blMaxValue<uint64_t>();

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
  BLRasterContextSavedState* savedState = ctxI->savedState;

  if (BL_UNLIKELY(!savedState))
    return blTraceError(BL_ERROR_NO_STATES_TO_RESTORE);

  // By default there would be only one state to restore if `cookie` was not provided.
  uint32_t n = 1;

  if (cookie) {
    // Verify context origin.
    if (BL_UNLIKELY(cookie->data[0] != ctxI->contextOriginId))
      return blTraceError(BL_ERROR_NO_MATCHING_COOKIE);

    // Verify cookie payload and get the number of states we have to restore (if valid).
    n = blRasterContextImplNumStatesToRestore(savedState, cookie->data[1]);
    if (BL_UNLIKELY(n == 0))
      return blTraceError(BL_ERROR_NO_MATCHING_COOKIE);
  }
  else {
    // A state that has a `stateId` assigned cannot be restored without a matching cookie.
    if (savedState->stateId != blMaxValue<uint64_t>())
      return blTraceError(BL_ERROR_NO_MATCHING_COOKIE);
  }

  uint32_t sharedFlagsToKeep = ctxI->contextFlags & BL_RASTER_CONTEXT_SHARED_ALL_FLAGS;
  ctxI->internalState.savedStateCount -= n;

  for (;;) {
    uint32_t restoreFlags = ctxI->contextFlags;
    blRasterContextImplRestoreCoreState(ctxI, savedState);

    if ((restoreFlags & BL_RASTER_CONTEXT_STATE_CONFIG) == 0) {
      ctxI->internalState.approximationOptions = savedState->approximationOptions;
      blRasterContextImplFlattenToleranceChanged(ctxI);
      blRasterContextImplOffsetParameterChanged(ctxI);

      sharedFlagsToKeep &= ~BL_RASTER_CONTEXT_SHARED_FILL_STATE;
    }

    if ((restoreFlags & BL_RASTER_CONTEXT_STATE_CLIP) == 0) {
      blRasterContextImplRestoreClippingFromState(ctxI, savedState);
      sharedFlagsToKeep &= ~BL_RASTER_CONTEXT_SHARED_FILL_STATE;
    }

    if ((restoreFlags & BL_RASTER_CONTEXT_STATE_FILL_STYLE) == 0) {
      constexpr uint32_t kOpType = BL_CONTEXT_OP_TYPE_FILL;

      BLRasterContextStyleData* dst = &ctxI->internalState.style[kOpType];
      BLRasterContextStyleData* src = &savedState->style[kOpType];

      if (restoreFlags & BL_RASTER_CONTEXT_FILL_FETCH_DATA)
        blRasterContextImplDestroyValidStyle(ctxI, dst);

      dst->packed = src->packed;
      dst->source = src->source;
      dst->rgba64 = src->rgba64;
      dst->adjustedMatrix = src->adjustedMatrix;
      ctxI->internalState.styleType[kOpType] = src->styleType;
    }

    if ((restoreFlags & BL_RASTER_CONTEXT_STATE_STROKE_STYLE) == 0) {
      constexpr uint32_t kOpType = BL_CONTEXT_OP_TYPE_STROKE;

      BLRasterContextStyleData* dst = &ctxI->internalState.style[kOpType];
      BLRasterContextStyleData* src = &savedState->style[kOpType];

      if (restoreFlags & BL_RASTER_CONTEXT_STROKE_FETCH_DATA)
        blRasterContextImplDestroyValidStyle(ctxI, dst);

      dst->packed = src->packed;
      dst->source = src->source;
      dst->rgba64 = src->rgba64;
      dst->adjustedMatrix = src->adjustedMatrix;
      ctxI->internalState.styleType[kOpType] = src->styleType;
    }

    if ((restoreFlags & BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS) == 0) {
      // NOTE: This code is unsafe, but since we know that `BLStrokeOptions` is
      // movable it's just fine. We destroy `BLStrokeOptions` first and then move
      // it into that destroyed instance params from the state itself.
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
        blRasterContextImplUpdateFinalMatrix(ctxI);
        blRasterContextImplUpdateMetaMatrixFixed(ctxI);
        blRasterContextImplUpdateFinalMatrixFixed(ctxI);
      }
      else {
        ctxI->internalState.finalMatrix = savedState->altMatrix;
        blRasterContextImplUpdateFinalMatrixFixed(ctxI);
      }

      sharedFlagsToKeep &= ~(BL_RASTER_CONTEXT_SHARED_FILL_STATE        |
                             BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE |
                             BL_RASTER_CONTEXT_SHARED_STROKE_EXT_STATE  );
    }

    BLRasterContextSavedState* finishedSavedState = savedState;
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

// ============================================================================
// [BLRasterContext - Transformations]
// ============================================================================

static BLResult BL_CDECL blRasterContextImplMatrixOp(BLContextImpl* baseImpl, uint32_t opType, const void* opData) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  blRasterContextImplBeforeUserMatrixChange(ctxI);
  BL_PROPAGATE(blMatrix2DApplyOp(&ctxI->internalState.userMatrix, opType, opData));

  blRasterContextUserMatrixChanged(ctxI);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplUserToMeta(BLContextImpl* baseImpl) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  constexpr uint32_t kUserAndMetaFlags = BL_RASTER_CONTEXT_STATE_META_MATRIX |
                                         BL_RASTER_CONTEXT_STATE_USER_MATRIX ;

  if (ctxI->contextFlags & kUserAndMetaFlags) {
    BLRasterContextSavedState* state = ctxI->savedState;

    // Always save both `metaMatrix` and `userMatrix` in case we have to save
    // the current state before we change the matrix. In this case the `altMatrix`
    // of the state would store the current `metaMatrix` and on state restore
    // the final matrix would be recalculated in-place.
    state->altMatrix = ctxI->metaMatrix();

    // Don't copy it if it was already saved, we would have copied an altered
    // userMatrix.
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

// ============================================================================
// [BLRasterContext - Rendering Hints]
// ============================================================================

static BLResult BL_CDECL blRasterContextImplSetHint(BLContextImpl* baseImpl, uint32_t hintType, uint32_t value) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  switch (hintType) {
    case BL_CONTEXT_HINT_RENDERING_QUALITY:
      if (BL_UNLIKELY(value >= BL_RENDERING_QUALITY_COUNT))
        return blTraceError(BL_ERROR_INVALID_VALUE);

      ctxI->internalState.hints.renderingQuality = uint8_t(value);
      return BL_SUCCESS;

    case BL_CONTEXT_HINT_GRADIENT_QUALITY:
      if (BL_UNLIKELY(value >= BL_GRADIENT_QUALITY_COUNT))
        return blTraceError(BL_ERROR_INVALID_VALUE);

      ctxI->internalState.hints.gradientQuality = uint8_t(value);
      return BL_SUCCESS;

    case BL_CONTEXT_HINT_PATTERN_QUALITY:
      if (BL_UNLIKELY(value >= BL_PATTERN_QUALITY_COUNT))
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

  if (BL_UNLIKELY(renderingQuality >= BL_RENDERING_QUALITY_COUNT ||
                  patternQuality   >= BL_PATTERN_QUALITY_COUNT   ||
                  gradientQuality  >= BL_GRADIENT_QUALITY_COUNT  ))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  ctxI->internalState.hints.renderingQuality = renderingQuality;
  ctxI->internalState.hints.patternQuality = patternQuality;
  ctxI->internalState.hints.gradientQuality = gradientQuality;
  return BL_SUCCESS;
}

// ============================================================================
// [BLRasterContext - Approximation Options]
// ============================================================================

static BLResult BL_CDECL blRasterContextImplSetFlattenMode(BLContextImpl* baseImpl, uint32_t mode) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(mode >= BL_FLATTEN_MODE_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  blRasterContextImplBeforeConfigChange(ctxI);
  ctxI->contextFlags &= ~BL_RASTER_CONTEXT_STATE_CONFIG;

  ctxI->internalState.approximationOptions.flattenMode = uint8_t(mode);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetFlattenTolerance(BLContextImpl* baseImpl, double tolerance) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(blIsNaN(tolerance)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  blRasterContextImplBeforeConfigChange(ctxI);
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_STATE_CONFIG     |
                          BL_RASTER_CONTEXT_SHARED_FILL_STATE);

  tolerance = blClamp(tolerance, BL_CONTEXT_MINIMUM_TOLERANCE, BL_CONTEXT_MAXIMUM_TOLERANCE);
  BL_ASSERT(blIsFinite(tolerance));

  ctxI->internalState.approximationOptions.flattenTolerance = tolerance;
  blRasterContextImplFlattenToleranceChanged(ctxI);

  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetApproximationOptions(BLContextImpl* baseImpl, const BLApproximationOptions* options) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  uint32_t flattenMode = options->flattenMode;
  uint32_t offsetMode = options->offsetMode;

  double flattenTolerance = options->flattenTolerance;
  double offsetParameter = options->offsetParameter;

  if (BL_UNLIKELY(flattenMode >= BL_FLATTEN_MODE_COUNT ||
                  offsetMode >= BL_OFFSET_MODE_COUNT ||
                  blIsNaN(flattenTolerance) ||
                  blIsNaN(offsetParameter)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  blRasterContextImplBeforeConfigChange(ctxI);
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_STATE_CONFIG     |
                          BL_RASTER_CONTEXT_SHARED_FILL_STATE);

  BLApproximationOptions& dst = ctxI->internalState.approximationOptions;
  dst.flattenMode = uint8_t(flattenMode);
  dst.offsetMode = uint8_t(offsetMode);
  dst.flattenTolerance = blClamp(flattenTolerance, BL_CONTEXT_MINIMUM_TOLERANCE, BL_CONTEXT_MAXIMUM_TOLERANCE);
  dst.offsetParameter = offsetParameter;

  blRasterContextImplFlattenToleranceChanged(ctxI);
  blRasterContextImplOffsetParameterChanged(ctxI);

  return BL_SUCCESS;
}

// ============================================================================
// [BLRasterContext - Compositing Options]
// ============================================================================

static BLResult BL_CDECL blRasterContextImplSetCompOp(BLContextImpl* baseImpl, uint32_t compOp) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(compOp >= BL_COMP_OP_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  ctxI->internalState.compOp = uint8_t(compOp);
  blRasterContextImplCompOpChanged(ctxI);

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

// ============================================================================
// [BLRasterContext - Fill Options]
// ============================================================================

static BLResult BL_CDECL blRasterContextImplSetFillRule(BLContextImpl* baseImpl, uint32_t fillRule) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(fillRule >= BL_FILL_RULE_COUNT))
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

// ============================================================================
// [BLRasterContext - Stroke Options]
// ============================================================================

static BLResult BL_CDECL blRasterContextImplSetStrokeWidth(BLContextImpl* baseImpl, double width) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  blRasterContextImplBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_NO_STROKE_OPTIONS    |
                          BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS |
                          BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE);

  ctxI->internalState.strokeOptions.width = width;
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeMiterLimit(BLContextImpl* baseImpl, double miterLimit) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  blRasterContextImplBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_NO_STROKE_OPTIONS    |
                          BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS |
                          BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE);

  ctxI->internalState.strokeOptions.miterLimit = miterLimit;
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeCap(BLContextImpl* baseImpl, uint32_t position, uint32_t strokeCap) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(position >= BL_STROKE_CAP_POSITION_COUNT || strokeCap >= BL_STROKE_CAP_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  blRasterContextImplBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE;

  ctxI->internalState.strokeOptions.caps[position] = uint8_t(strokeCap);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeCaps(BLContextImpl* baseImpl, uint32_t strokeCap) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(strokeCap >= BL_STROKE_CAP_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  blRasterContextImplBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE;

  for (uint32_t i = 0; i < BL_STROKE_CAP_POSITION_COUNT; i++)
    ctxI->internalState.strokeOptions.caps[i] = uint8_t(strokeCap);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeJoin(BLContextImpl* baseImpl, uint32_t strokeJoin) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(strokeJoin >= BL_STROKE_JOIN_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  blRasterContextImplBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE;

  ctxI->internalState.strokeOptions.join = uint8_t(strokeJoin);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeDashOffset(BLContextImpl* baseImpl, double dashOffset) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  blRasterContextImplBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_NO_STROKE_OPTIONS    |
                          BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS |
                          BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE);

  ctxI->internalState.strokeOptions.dashOffset = dashOffset;
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeDashArray(BLContextImpl* baseImpl, const BLArrayCore* dashArray) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(dashArray->impl->implType != BL_IMPL_TYPE_ARRAY_F64))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  blRasterContextImplBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_NO_STROKE_OPTIONS    |
                          BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS |
                          BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE);

  ctxI->internalState.strokeOptions.dashArray = dashArray->dcast<BLArray<double>>();
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeTransformOrder(BLContextImpl* baseImpl, uint32_t transformOrder) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(transformOrder >= BL_STROKE_TRANSFORM_ORDER_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  blRasterContextImplBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(BL_RASTER_CONTEXT_STATE_STROKE_OPTIONS |
                          BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE);

  ctxI->internalState.strokeOptions.transformOrder = uint8_t(transformOrder);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplSetStrokeOptions(BLContextImpl* baseImpl, const BLStrokeOptionsCore* options) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(options->startCap >= BL_STROKE_CAP_COUNT ||
                  options->endCap >= BL_STROKE_CAP_COUNT ||
                  options->join >= BL_STROKE_JOIN_COUNT ||
                  options->transformOrder >= BL_STROKE_TRANSFORM_ORDER_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  blRasterContextImplBeforeStrokeChange(ctxI);
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

// ============================================================================
// [BLRasterContext - Clip Operations]
// ============================================================================

static BLResult blRasterContextImplClipToFinalBox(BLRasterContextImpl* ctxI, const BLBox& inputBox) noexcept {
  BLBox b;
  blRasterContextImplBeforeClipBoxChange(ctxI);

  if (blIntersectBoxes(b, ctxI->finalClipBoxD(), inputBox)) {
    ctxI->internalState.finalClipBoxD = b;
    ctxI->internalState.finalClipBoxI.reset(
      blTruncToInt(b.x0),
      blTruncToInt(b.y0),
      blCeilToInt(b.x1),
      blCeilToInt(b.y1));
    ctxI->setFinalClipBoxFixedD(b * ctxI->fpScaleD());

    double frac = blMax(ctxI->finalClipBoxD().x0 - ctxI->finalClipBoxI().x0,
                        ctxI->finalClipBoxD().y0 - ctxI->finalClipBoxI().y0,
                        ctxI->finalClipBoxD().x1 - ctxI->finalClipBoxI().x1,
                        ctxI->finalClipBoxD().y1 - ctxI->finalClipBoxI().y1) * ctxI->fpScaleD();

    if (blTrunc(frac) == 0)
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
  return blRasterContextImplClipToFinalBox(ctxI, blMatrix2DMapBox(ctxI->finalMatrix(), inputBox));
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
  blRasterContextImplBeforeClipBoxChange(ctxI);

  int tx = ctxI->translationI().x;
  int ty = ctxI->translationI().y;

  if (blRuntimeIs32Bit()) {
    BLOverflowFlag of = 0;

    int x0 = blAddOverflow(tx, rect->x, &of);
    int y0 = blAddOverflow(ty, rect->y, &of);
    int x1 = blAddOverflow(x0, rect->w, &of);
    int y1 = blAddOverflow(y0, rect->h, &of);

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
  BLRasterContextSavedState* state = ctxI->savedState;

  if (!(ctxI->contextFlags & BL_RASTER_CONTEXT_STATE_CLIP)) {
    if (state) {
      blRasterContextImplRestoreClippingFromState(ctxI, state);
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
      blRasterContextImplResetClippingToMetaClipBox(ctxI);
    }
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLRasterContext - Synchronous Rendering - Process Command]
// ============================================================================

// Wrappers responsible for getting the pipeline and then calling a real command
// processor with the command data held by the command serializer.

static BL_INLINE BLResult blRasterContextImplProcessFillBoxA(BLRasterContextImpl* ctxI, BLRasterCoreCommandSerializerSync& serializer) noexcept {
  BLPipeFillFunc fillFunc = blRasterContextImplGetFillFunc(ctxI, serializer.pipeSignature().value);
  if (BL_UNLIKELY(!fillFunc))
    return blTraceError(BL_ERROR_INVALID_STATE);

  serializer.initFillFunc(fillFunc);
  return blRasterCommandProcSync_FillBoxA(ctxI->syncWorkData, serializer.command());
}

static BL_INLINE BLResult blRasterContextImplProcessFillBoxU(BLRasterContextImpl* ctxI, BLRasterCoreCommandSerializerSync& serializer) noexcept {
  BLPipeFillFunc fillFunc = blRasterContextImplGetFillFunc(ctxI, serializer.pipeSignature().value);
  if (BL_UNLIKELY(!fillFunc))
    return blTraceError(BL_ERROR_INVALID_STATE);

  serializer.initFillFunc(fillFunc);
  return blRasterCommandProcSync_FillBoxU(ctxI->syncWorkData, serializer.command());
}

static BL_INLINE BLResult blRasterContextImplProcessFillAnalytic(BLRasterContextImpl* ctxI, BLRasterCoreCommandSerializerSync& serializer) noexcept {
  BLPipeFillFunc fillFunc = blRasterContextImplGetFillFunc(ctxI, serializer.pipeSignature().value);
  if (BL_UNLIKELY(!fillFunc))
    return blTraceError(BL_ERROR_INVALID_STATE);

  serializer.initFillFunc(fillFunc);
  return blRasterCommandProcSync_FillAnalytic(ctxI->syncWorkData, serializer.command());
}

// ============================================================================
// [BLRasterContext - Synchronous Rendering - Fill Clipped Geometry]
// ============================================================================

// Synchronous fills are called when a geometry has been transformed and fully
// clipped in a synchronous mode. The responsibility of these functions is to
// initialized the command data through serializer and then to call a command
// processor.

template<uint32_t Category>
static BL_INLINE BLResult blRasterContextImplFillClippedBoxA(BLRasterContextImpl* ctxI, BLRasterCoreCommandSerializerSync& serializer, const BLBoxI& boxA) noexcept {
  serializer.initFillBoxA(boxA);
  if (!blRasterContextImplEnsureFetchData<Category>(ctxI, serializer))
    return BL_SUCCESS;
  return blRasterContextImplProcessFillBoxA(ctxI, serializer);
}

template<uint32_t Category>
static BL_INLINE BLResult blRasterContextImplFillClippedBoxU(BLRasterContextImpl* ctxI, BLRasterCoreCommandSerializerSync& serializer, const BLBoxI& boxU) noexcept {
  if (!blRasterContextImplEnsureFetchData<Category>(ctxI, serializer))
    return BL_SUCCESS;

  if (blRasterIsBoxAligned24x8(boxU)) {
    serializer.initFillBoxA(BLBoxI(boxU.x0 >> 8, boxU.y0 >> 8, boxU.x1 >> 8, boxU.y1 >> 8));
    return blRasterContextImplProcessFillBoxA(ctxI, serializer);
  }
  else {
    serializer.initFillBoxU(boxU);
    return blRasterContextImplProcessFillBoxU(ctxI, serializer);
  }
}

template<uint32_t Category>
static BL_INLINE BLResult blRasterContextImplFillClippedEdges(BLRasterContextImpl* ctxI, BLRasterCoreCommandSerializerSync& serializer, uint32_t fillRule) noexcept {
  BLRasterWorkData* workData = &ctxI->syncWorkData;

  // No data or everything was clipped out (no edges at all). We really want
  // to check this now as it could be a waste of resources to try to ensure
  // fetch-data if the command doesn't renderer anything.
  if (workData->edgeStorage.empty())
    return BL_SUCCESS;

  if (!blRasterContextImplEnsureFetchData<Category>(ctxI, serializer)) {
    workData->revertEdgeBuilder();
    return BL_SUCCESS;
  }

  serializer.initFillAnalyticSync(fillRule, &workData->edgeStorage);
  return blRasterContextImplProcessFillAnalytic(ctxI, serializer);
}

// ============================================================================
// [BLRasterContext - Asynchronous Rendering - Shared State]
// ============================================================================

static BL_INLINE BLRasterSharedFillState* blRasterContextImplEnsureFillState(BLRasterContextImpl* ctxI) noexcept {
  BLRasterSharedFillState* sharedFillState = ctxI->sharedFillState;

  if (!(ctxI->contextFlags & BL_RASTER_CONTEXT_SHARED_FILL_STATE)) {
    sharedFillState = ctxI->workerMgr()._allocator.allocNoAlignT<BLRasterSharedFillState>();
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

static const uint32_t blRasterSharedStrokeStateFlags[BL_STROKE_TRANSFORM_ORDER_COUNT] = {
  BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE,
  BL_RASTER_CONTEXT_SHARED_STROKE_BASE_STATE | BL_RASTER_CONTEXT_SHARED_STROKE_EXT_STATE
};

static const uint32_t blRasterSharedStrokeStateSize[BL_STROKE_TRANSFORM_ORDER_COUNT] = {
  uint32_t(sizeof(BLRasterSharedBaseStrokeState)),
  uint32_t(sizeof(BLRasterSharedExtendedStrokeState))
};

static BL_INLINE BLRasterSharedBaseStrokeState* blRasterContextImplEnsureStrokeState(BLRasterContextImpl* ctxI) noexcept {
  BLRasterSharedBaseStrokeState* sharedStrokeState = ctxI->sharedStrokeState;

  uint32_t transformOrder = ctxI->strokeOptions().transformOrder;
  uint32_t sharedFlags = blRasterSharedStrokeStateFlags[transformOrder];

  if ((ctxI->contextFlags & sharedFlags) != sharedFlags) {
    size_t stateSize = blRasterSharedStrokeStateSize[transformOrder];
    void* stateData = ctxI->workerMgr()._allocator.allocNoAlignT<BLRasterSharedBaseStrokeState>(stateSize);

    if (BL_UNLIKELY(!stateData))
      return nullptr;

    sharedStrokeState = new(stateData) BLRasterSharedBaseStrokeState(ctxI->strokeOptions(), ctxI->approximationOptions());
    if (transformOrder != BL_STROKE_TRANSFORM_ORDER_AFTER) {
      static_cast<BLRasterSharedExtendedStrokeState*>(sharedStrokeState)->userMatrix = ctxI->userMatrix();
      static_cast<BLRasterSharedExtendedStrokeState*>(sharedStrokeState)->metaMatrixFixed = ctxI->metaMatrixFixed();
    }

    ctxI->sharedStrokeState = sharedStrokeState;
    ctxI->contextFlags |= sharedFlags;
  }

  return sharedStrokeState;
}

// ============================================================================
// [BLRasterContext - Asynchronous Rendering - Enqueue Command]
// ============================================================================

static void BL_CDECL blRasterContextImplDestroyAsyncBlitData(BLRasterContextImpl* ctxI, BLRasterFetchData* fetchData) noexcept {
  blUnused(ctxI);

  BLImageImpl* imgI = fetchData->_image->impl;
  if (blImplDecRefAndTest(imgI))
    blImageImplDelete(imgI);
}

template<uint32_t Category, typename CommandFinalizer>
static BL_INLINE BLResult blRasterContextImplEnqueueCommand(BLRasterContextImpl* ctxI, BLRasterCommand& command, const CommandFinalizer& commandFinalizer) noexcept {
  BLRasterWorkerManager& mgr = ctxI->workerMgr();

  if (command.hasFetchData()) {
    BLRasterFetchData* fetchData = command._source.fetchData;
    if (fetchData->_batchId != mgr.currentBatchId()) {
      BL_PROPAGATE(mgr.ensureFetchQueue());
      fetchData->_batchId = mgr.currentBatchId();

      // Blit fetch-data is not managed by the context's state management. This
      // is actually the main reason we propagate the category all the way here.
      if (Category == BL_RASTER_COMMAND_CATEGORY_BLIT) {
        mgr._fetchQueueAppender.append(fetchData);
        fetchData->_destroyFunc = blRasterContextImplDestroyAsyncBlitData;
        blImplIncRef(fetchData->_image->impl);
      }
      else {
        mgr._fetchQueueAppender.append(fetchData->addRef());
      }
    }
  }

  commandFinalizer(command);
  mgr._commandQueueAppender.advance();
  return BL_SUCCESS;
}

template<uint32_t Category>
static BL_INLINE BLResult blRasterContextImplEnqueueFillBox(BLRasterContextImpl* ctxI, BLRasterCoreCommandSerializerAsync& serializer) noexcept {
  BLPipeFillFunc fillFunc = blRasterContextImplGetFillFunc(ctxI, serializer.pipeSignature().value);
  if (BL_UNLIKELY(!fillFunc))
    return blTraceError(BL_ERROR_INVALID_STATE);

  serializer.initFillFunc(fillFunc);
  return blRasterContextImplEnqueueCommand<Category>(ctxI, serializer.command(), [&](BLRasterCommand&) {});
}

template<uint32_t Category>
static BL_INLINE BLResult blRasterContextImplEnqueueFillAnalytic(BLRasterContextImpl* ctxI, BLRasterCoreCommandSerializerAsync& serializer) noexcept {
  BLPipeFillFunc fillFunc = blRasterContextImplGetFillFunc(ctxI, serializer.pipeSignature().value);
  if (BL_UNLIKELY(!fillFunc))
    return blTraceError(BL_ERROR_INVALID_STATE);

  serializer.initFillFunc(fillFunc);
  return blRasterContextImplEnqueueCommand<Category>(ctxI, serializer.command(), [&](BLRasterCommand& command) {
    command._analyticAsync.stateSlotIndex = ctxI->workerMgr().nextStateSlotIndex();
  });
}

// ============================================================================
// [BLRasterContext - Asynchronous Rendering - Enqueue Command With Job]
// ============================================================================

template<typename JobType>
static BL_INLINE BLResult blRasterContextImplNewFillJob(BLRasterContextImpl* ctxI, size_t jobDataSize, JobType** out) noexcept {
  BL_PROPAGATE(ctxI->workerMgr().ensureJobQueue());

  BLRasterSharedFillState* fillState = blRasterContextImplEnsureFillState(ctxI);
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
static BL_INLINE BLResult blRasterContextImplNewStrokeJob(BLRasterContextImpl* ctxI, size_t jobDataSize, JobType** out) noexcept {
  BL_PROPAGATE(ctxI->workerMgr().ensureJobQueue());

  BLRasterSharedFillState* fillState = blRasterContextImplEnsureFillState(ctxI);
  if (BL_UNLIKELY(!fillState))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLRasterSharedBaseStrokeState* strokeState = blRasterContextImplEnsureStrokeState(ctxI);
  if (BL_UNLIKELY(!strokeState))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  JobType* job = ctxI->workerMgr()._allocator.template allocNoAlignT<JobType>(jobDataSize);
  if (BL_UNLIKELY(!job))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  job->initStates(fillState, strokeState);
  *out = job;
  return BL_SUCCESS;
}

template<uint32_t Category, typename JobType, typename JobFinalizer>
static BL_INLINE BLResult blRasterContextImplEnqueueCommandWithFillJob(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerAsync& serializer,
  size_t jobSize,
  const JobFinalizer& jobFinalizer) noexcept {

  JobType* job;
  BL_PROPAGATE(blRasterContextImplNewFillJob(ctxI, jobSize, &job));

  if (!blRasterContextImplEnsureFetchData<Category>(ctxI, serializer)) {
    // TODO: [Rendering Context] Error handling.
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  BLPipeFillFunc fillFunc = blRasterContextImplGetFillFunc(ctxI, serializer.pipeSignature().value);
  if (BL_UNLIKELY(!fillFunc))
    return blTraceError(BL_ERROR_INVALID_STATE);

  serializer.initFillFunc(fillFunc);
  return blRasterContextImplEnqueueCommand<Category>(ctxI, serializer.command(), [&](BLRasterCommand& command) {
    command._analyticAsync.stateSlotIndex = ctxI->workerMgr().nextStateSlotIndex();
    job->initFillJob(serializer._command);
    job->setMetaMatrixFixedType(ctxI->metaMatrixFixedType());
    job->setFinalMatrixFixedType(ctxI->finalMatrixFixedType());
    jobFinalizer(job);
    ctxI->workerMgr().addJob(job);
  });
}

template<uint32_t Category, typename JobType, typename JobFinalizer>
static BL_INLINE BLResult blRasterContextImplEnqueueCommandWithStrokeJob(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerAsync& serializer,
  size_t jobSize,
  const JobFinalizer& jobFinalizer) noexcept {

  JobType* job;
  BL_PROPAGATE(blRasterContextImplNewStrokeJob(ctxI, jobSize, &job));

  if (!blRasterContextImplEnsureFetchData<Category>(ctxI, serializer)) {
    // TODO: [Rendering Context] Error handling.
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  BLPipeFillFunc fillFunc = blRasterContextImplGetFillFunc(ctxI, serializer.pipeSignature().value);
  if (BL_UNLIKELY(!fillFunc))
    return blTraceError(BL_ERROR_INVALID_STATE);

  serializer.initFillFunc(fillFunc);
  return blRasterContextImplEnqueueCommand<Category>(ctxI, serializer.command(), [&](BLRasterCommand& command) {
    command._analyticAsync.stateSlotIndex = ctxI->workerMgr().nextStateSlotIndex();
    job->initStrokeJob(serializer._command);
    job->setMetaMatrixFixedType(ctxI->metaMatrixFixedType());
    job->setFinalMatrixFixedType(ctxI->finalMatrixFixedType());
    jobFinalizer(job);
    ctxI->workerMgr().addJob(job);
  });
}

template<uint32_t Category, uint32_t OpType, typename JobType, typename JobFinalizer>
static BL_INLINE BLResult blRasterContextImplEnqueueCommandWithJob(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerAsync& serializer,
  size_t jobSize,
  const JobFinalizer& jobFinalizer) noexcept {

  if (OpType == BL_CONTEXT_OP_TYPE_FILL)
    return blRasterContextImplEnqueueCommandWithFillJob<Category, JobType, JobFinalizer>(ctxI, serializer, jobSize, jobFinalizer);
  else
    return blRasterContextImplEnqueueCommandWithStrokeJob<Category, JobType, JobFinalizer>(ctxI, serializer, jobSize, jobFinalizer);
}

// ============================================================================
// [BLRasterContext - Asynchronous Rendering - Enqueue GlyphRun / TextData]
// ============================================================================

struct BLGlyphPlacementRawData {
  uint64_t data[2];
};

static_assert(sizeof(BLGlyphPlacementRawData) == sizeof(BLPoint)         , "BLGlyphPlacementRawData doesn't match BLPoint");
static_assert(sizeof(BLGlyphPlacementRawData) == sizeof(BLGlyphPlacement), "BLGlyphPlacementRawData doesn't match BLGlyphPlacement");

template<uint32_t Category, uint32_t OpType>
static BL_INLINE BLResult blRasterContextImplEnqueueFillOrStrokeGlyphRun(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerAsync& serializer,
  const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  size_t size = glyphRun->size;
  size_t glyphDataSize = size * sizeof(uint32_t);
  size_t placementDataSize = size * sizeof(BLGlyphPlacementRawData);

  uint32_t* glyphData = ctxI->workerMgr()._allocator.template allocT<uint32_t>(glyphDataSize, 8);
  BLGlyphPlacementRawData* placementData = ctxI->workerMgr()._allocator.template allocT<BLGlyphPlacementRawData>(placementDataSize, 8);

  if (BL_UNLIKELY(!glyphData || !placementData)) {
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  BLGlyphRunIterator it(*glyphRun);
  uint32_t* dstGlyphData = glyphData;
  BLGlyphPlacementRawData* dstPlacementData = placementData;

  while (!it.atEnd()) {
    *dstGlyphData++ = it.glyphId();
    *dstPlacementData++ = it.placement<BLGlyphPlacementRawData>();
    it.advance();
  }

  serializer.initFillAnalyticAsync(BL_FILL_RULE_NON_ZERO, nullptr);
  return blRasterContextImplEnqueueCommandWithJob<Category, OpType, BLRasterJobData_TextOp>(
    ctxI, serializer,
    sizeof(BLRasterJobData_TextOp),
    [&](BLRasterJobData_TextOp* job) {
      job->initCoordinates(*pt);
      job->initFont(*font);
      job->initGlyphRun(glyphData, placementData, size, glyphRun->placementType, glyphRun->flags);
    });
}

template<uint32_t Category, uint32_t OpType>
static BL_INLINE BLResult blRasterContextImplEnqueueFillOrStrokeText(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerAsync& serializer,
  const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) noexcept {

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
    serializedTextData = ctxI->workerMgr->_allocator.alloc(blAlignUp(serializedTextSize, 8));
    if (!serializedTextData)
      result = BL_ERROR_OUT_OF_MEMORY;
    else
      memcpy(serializedTextData, text, serializedTextSize);
  }

  if (result == BL_SUCCESS) {
    serializer.initFillAnalyticAsync(BL_FILL_RULE_NON_ZERO, nullptr);
    result = blRasterContextImplEnqueueCommandWithJob<Category, OpType, BLRasterJobData_TextOp>(
      ctxI, serializer,
      sizeof(BLRasterJobData_TextOp),
      [&](BLRasterJobData_TextOp* job) {
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

// ============================================================================
// [BLRasterContext - Asynchronous Rendering - Fill Clipped Geometry]
// ============================================================================

template<uint32_t Category>
static BL_INLINE BLResult blRasterContextImplFillClippedBoxA(BLRasterContextImpl* ctxI, BLRasterCoreCommandSerializerAsync& serializer, const BLBoxI& boxA) noexcept {
  serializer.initFillBoxA(boxA);
  if (!blRasterContextImplEnsureFetchData<Category>(ctxI, serializer))
    return BL_SUCCESS;
  return blRasterContextImplEnqueueFillBox<Category>(ctxI, serializer);
}

template<uint32_t Category>
static BL_INLINE BLResult blRasterContextImplFillClippedBoxU(BLRasterContextImpl* ctxI, BLRasterCoreCommandSerializerAsync& serializer, const BLBoxI& boxU) noexcept {
  if (blRasterIsBoxAligned24x8(boxU))
    serializer.initFillBoxA(BLBoxI(boxU.x0 >> 8, boxU.y0 >> 8, boxU.x1 >> 8, boxU.y1 >> 8));
  else
    serializer.initFillBoxU(boxU);

  if (!blRasterContextImplEnsureFetchData<Category>(ctxI, serializer))
    return BL_SUCCESS;
  return blRasterContextImplEnqueueFillBox<Category>(ctxI, serializer);
}

template<uint32_t Category>
static BL_INLINE BLResult blRasterContextImplFillClippedEdges(BLRasterContextImpl* ctxI, BLRasterCoreCommandSerializerAsync& serializer, uint32_t fillRule) noexcept {
  BLRasterWorkData* workData = &ctxI->syncWorkData;

  // No data or everything was clipped out (no edges at all).
  if (workData->edgeStorage.empty())
    return BL_SUCCESS;

  if (!blRasterContextImplEnsureFetchData<Category>(ctxI, serializer)) {
    workData->revertEdgeBuilder();
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  serializer.initFillAnalyticAsync(fillRule, workData->edgeStorage.flattenEdgeLinks());
  workData->edgeStorage.resetBoundingBox();
  return blRasterContextImplEnqueueFillAnalytic<Category>(ctxI, serializer);
}

// ============================================================================
// [BLRasterContext - Asynchronous Rendering - Fill / Stroke Unsafe Geometry]
// ============================================================================

template<uint32_t Category>
static BL_INLINE BLResult blRasterContextImplFillUnsafeGeometry(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerAsync& serializer,
  uint32_t fillRule,
  uint32_t geometryType, const void* geometryData) noexcept {

  size_t geometrySize = sizeof(void*);
  if (blIsSimpleGeometryType(geometryType))
    geometrySize = blSimpleGeometrySize[geometryType];
  else
    BL_ASSERT(geometryType == BL_GEOMETRY_TYPE_PATH);

  serializer.initFillAnalyticAsync(fillRule, nullptr);
  return blRasterContextImplEnqueueCommandWithFillJob<Category, BLRasterJobData_GeometryOp>(
    ctxI, serializer,
    sizeof(BLRasterJobData_GeometryOp) + geometrySize,
    [&](BLRasterJobData_GeometryOp* job) {
      job->setGeometry(geometryType, geometryData, geometrySize);
    });
}

template<uint32_t Category>
static BL_INLINE BLResult blRasterContextImplStrokeUnsafeGeometry(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerAsync& serializer,
  uint32_t geometryType, const void* geometryData) noexcept {

  serializer.initFillAnalyticAsync(BL_FILL_RULE_NON_ZERO, nullptr);

  size_t geometrySize = sizeof(void*);
  if (blIsSimpleGeometryType(geometryType)) {
    geometrySize = blSimpleGeometrySize[geometryType];
  }
  else if (geometryType != BL_GEOMETRY_TYPE_PATH) {
    BLPath* temporaryPath = &ctxI->syncWorkData.tmpPath[3];

    temporaryPath->clear();
    BL_PROPAGATE(temporaryPath->addGeometry(geometryType, geometryData));

    geometryType = BL_GEOMETRY_TYPE_PATH;
    geometryData = temporaryPath;
  }

  return blRasterContextImplEnqueueCommandWithStrokeJob<Category, BLRasterJobData_GeometryOp>(
    ctxI, serializer,
    sizeof(BLRasterJobData_GeometryOp) + geometrySize,
    [&](BLRasterJobData_GeometryOp* job) {
      job->setGeometry(geometryType, geometryData, geometrySize);
    });
}

// ============================================================================
// [BLRasterContext - Fill Internals - Fill Unsafe Poly]
// ============================================================================

template<uint32_t Category, uint32_t RenderingMode, class PointType>
static BL_INLINE BLResult blRasterContextImplFillUnsafePolygon(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializer<RenderingMode>& serializer,
  uint32_t fillRule, const PointType* pts, size_t size, const BLMatrix2D& m, uint32_t mType) noexcept {

  BL_PROPAGATE(blRasterContextBuildPolyEdges(&ctxI->syncWorkData, pts, size, m, mType));
  return blRasterContextImplFillClippedEdges<Category>(ctxI, serializer, fillRule);
}

template<uint32_t Category, uint32_t RenderingMode, class PointType>
static BL_INLINE BLResult blRasterContextImplFillUnsafePolygon(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializer<RenderingMode>& serializer,
  uint32_t fillRule, const PointType* pts, size_t size) noexcept {

  return blRasterContextImplFillUnsafePolygon<Category>(ctxI, serializer, fillRule, pts, size, ctxI->finalMatrixFixed(), ctxI->finalMatrixFixedType());
}

// ============================================================================
// [BLRasterContext - Fill Internals - Fill Unsafe Path]
// ============================================================================

template<uint32_t Category, uint32_t RenderingMode>
static BL_INLINE BLResult blRasterContextImplFillUnsafePath(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializer<RenderingMode>& serializer,
  uint32_t fillRule, const BLPath& path, const BLMatrix2D& m, uint32_t mType) noexcept {

  BL_PROPAGATE(blRasterContextBuildPathEdges(&ctxI->syncWorkData, path.impl->view, m, mType));
  return blRasterContextImplFillClippedEdges<Category>(ctxI, serializer, fillRule);
}

template<uint32_t Category, uint32_t RenderingMode>
static BL_INLINE BLResult blRasterContextImplFillUnsafePath(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializer<RenderingMode>& serializer,
  uint32_t fillRule, const BLPath& path) noexcept {

  return blRasterContextImplFillUnsafePath<Category>(ctxI, serializer, fillRule, path, ctxI->finalMatrixFixed(), ctxI->finalMatrixFixedType());
}

// ============================================================================
// [BLRasterContext - Fill Internals - Fill Unsafe Box]
// ============================================================================

template<uint32_t Category, uint32_t RenderingMode>
static BL_INLINE BLResult blRasterContextImplFillUnsafeBox(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializer<RenderingMode>& serializer,
  const BLBox& box, const BLMatrix2D& m, uint32_t mType) noexcept {

  if (mType <= BL_MATRIX2D_TYPE_SWAP) {
    BLBox finalBox = blMatrix2DMapBox(m, box);

    if (!blIntersectBoxes(finalBox, finalBox, ctxI->finalClipBoxFixedD()))
      return BL_SUCCESS;

    BLBoxI finalBoxFixed(blTruncToInt(finalBox.x0),
                         blTruncToInt(finalBox.y0),
                         blTruncToInt(finalBox.x1),
                         blTruncToInt(finalBox.y1));
    return blRasterContextImplFillClippedBoxU<Category>(ctxI, serializer, finalBoxFixed);
  }
  else {
    BLPoint polyD[] = {
      BLPoint(box.x0, box.y0),
      BLPoint(box.x1, box.y0),
      BLPoint(box.x1, box.y1),
      BLPoint(box.x0, box.y1)
    };
    return blRasterContextImplFillUnsafePolygon<Category>(ctxI, serializer, BL_RASTER_CONTEXT_PREFERRED_FILL_RULE, polyD, BL_ARRAY_SIZE(polyD), m, mType);
  }
}

template<uint32_t Category, uint32_t RenderingMode>
static BL_INLINE BLResult blRasterContextImplFillUnsafeBox(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializer<RenderingMode>& serializer,
  const BLBox& box) noexcept {

  return blRasterContextImplFillUnsafeBox<Category>(ctxI, serializer, box, ctxI->finalMatrixFixed(), ctxI->finalMatrixFixedType());
}

// ============================================================================
// [BLRasterContext - Fill Internals - Fill Unsafe Rect]
// ============================================================================

// Fully integer-based rectangle fill.
template<uint32_t Category, uint32_t RenderingMode>
static BL_INLINE BLResult blRasterContextImplFillUnsafeRectI(BLRasterContextImpl* ctxI, BLRasterCoreCommandSerializer<RenderingMode>& serializer, const BLRectI& rect) noexcept {
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
    return blRasterContextImplFillUnsafeBox<Category>(ctxI, serializer, boxD);
  }

  BLBoxI boxI;
  if (blRuntimeIs32Bit()) {
    BLOverflowFlag of = 0;

    int x0 = blAddOverflow(rect.x, ctxI->translationI().x, &of);
    int y0 = blAddOverflow(rect.y, ctxI->translationI().y, &of);
    int x1 = blAddOverflow(rw, x0, &of);
    int y1 = blAddOverflow(rh, y0, &of);

    if (BL_UNLIKELY(of))
      goto Use64Bit;

    x0 = blMax(x0, ctxI->finalClipBoxI().x0);
    y0 = blMax(y0, ctxI->finalClipBoxI().y0);
    x1 = blMin(x1, ctxI->finalClipBoxI().x1);
    y1 = blMin(y1, ctxI->finalClipBoxI().y1);

    // Clipped out or invalid rect.
    if ((x0 >= x1) | (y0 >= y1))
      return BL_SUCCESS;

    boxI.reset(int(x0), int(y0), int(x1), int(y1));
  }
  else {
Use64Bit:
    int64_t x0 = int64_t(rect.x) + int64_t(ctxI->translationI().x);
    int64_t y0 = int64_t(rect.y) + int64_t(ctxI->translationI().y);
    int64_t x1 = int64_t(rw) + x0;
    int64_t y1 = int64_t(rh) + y0;

    x0 = blMax<int64_t>(x0, ctxI->finalClipBoxI().x0);
    y0 = blMax<int64_t>(y0, ctxI->finalClipBoxI().y0);
    x1 = blMin<int64_t>(x1, ctxI->finalClipBoxI().x1);
    y1 = blMin<int64_t>(y1, ctxI->finalClipBoxI().y1);

    // Clipped out or invalid rect.
    if ((x0 >= x1) | (y0 >= y1))
      return BL_SUCCESS;

    boxI.reset(int(x0), int(y0), int(x1), int(y1));
  }

  return blRasterContextImplFillClippedBoxA<Category>(ctxI, serializer, boxI);
}


// ============================================================================
// [BLRasterContext - Clear All]
// ============================================================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplClearAll(BLContextImpl* baseImpl) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BLRasterCoreCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  uint32_t status = blRasterContextImplPrepareClear(ctxI, serializer, BL_RASTER_CONTEXT_NO_CLEAR_FLAGS_FORCE);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  if (ctxI->syncWorkData.clipMode == BL_CLIP_MODE_ALIGNED_RECT)
    return blRasterContextImplFillClippedBoxA<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, ctxI->finalClipBoxI());

  BLBoxI box(blTruncToInt(ctxI->finalClipBoxFixedD().x0),
             blTruncToInt(ctxI->finalClipBoxFixedD().y0),
             blTruncToInt(ctxI->finalClipBoxFixedD().x1),
             blTruncToInt(ctxI->finalClipBoxFixedD().y1));
  return blRasterContextImplFillClippedBoxU<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, box);
}

// ============================================================================
// [BLRasterContext - Clear Rect]
// ============================================================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplClearRectI(BLContextImpl* baseImpl, const BLRectI* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BLRasterCoreCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  uint32_t status = blRasterContextImplPrepareClear(ctxI, serializer, BL_RASTER_CONTEXT_NO_CLEAR_FLAGS);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return blRasterContextImplFillUnsafeRectI<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, *rect);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplClearRectD(BLContextImpl* baseImpl, const BLRect* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BLRasterCoreCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  uint32_t status = blRasterContextImplPrepareClear(ctxI, serializer, BL_RASTER_CONTEXT_NO_CLEAR_FLAGS);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  BLBox boxD(rect->x, rect->y, rect->x + rect->w, rect->y + rect->h);
  return blRasterContextImplFillUnsafeBox<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, boxD);
}

// ============================================================================
// [BLRasterContext - Fill All]
// ============================================================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillAll(BLContextImpl* baseImpl) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BLRasterCoreCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  uint32_t status = blRasterContextImplPrepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_FILL), BL_RASTER_CONTEXT_NO_FILL_FLAGS_FORCE);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  if (ctxI->syncWorkData.clipMode == BL_CLIP_MODE_ALIGNED_RECT)
    return blRasterContextImplFillClippedBoxA<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, ctxI->finalClipBoxI());

  BLBoxI box(blTruncToInt(ctxI->finalClipBoxFixedD().x0),
             blTruncToInt(ctxI->finalClipBoxFixedD().y0),
             blTruncToInt(ctxI->finalClipBoxFixedD().x1),
             blTruncToInt(ctxI->finalClipBoxFixedD().y1));
  return blRasterContextImplFillClippedBoxU<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, box);
}

// ============================================================================
// [BLRasterContext - Fill Rect]
// ============================================================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillRectI(BLContextImpl* baseImpl, const BLRectI* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BLRasterCoreCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  uint32_t status = blRasterContextImplPrepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_FILL), BL_RASTER_CONTEXT_NO_FILL_FLAGS);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return blRasterContextImplFillUnsafeRectI<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, *rect);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillRectD(BLContextImpl* baseImpl, const BLRect* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BLRasterCoreCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  uint32_t status = blRasterContextImplPrepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_FILL), BL_RASTER_CONTEXT_NO_FILL_FLAGS);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  BLBox boxD(rect->x, rect->y, rect->x + rect->w, rect->y + rect->h);
  return blRasterContextImplFillUnsafeBox<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, boxD);
}

// ============================================================================
// [BLRasterContext - Fill Geometry]
// ============================================================================

static BLResult BL_INLINE blRasterContextImplFillGeometryInternal(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerSync& serializer,
  uint32_t geometryType, const void* geometryData) noexcept {

  const BLBox* box;
  BLBox temporaryBox;

  // Gotos are used to limit the inline function expansion as all the FillXXX
  // functions are inlined, this makes the binary a bit smaller as each call
  // to such function is expanded only once.
  switch (geometryType) {
    case BL_GEOMETRY_TYPE_RECTI: {
      return blRasterContextImplFillUnsafeRectI<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, *static_cast<const BLRectI*>(geometryData));
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
      return blRasterContextImplFillUnsafeBox<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, *box);
    }

    case BL_GEOMETRY_TYPE_POLYGONI:
    case BL_GEOMETRY_TYPE_POLYLINEI: {
      const BLArrayView<BLPointI>* array = static_cast<const BLArrayView<BLPointI>*>(geometryData);
      if (BL_UNLIKELY(array->size < 3))
        return BL_SUCCESS;

      return blRasterContextImplFillUnsafePolygon<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, ctxI->fillRule(), array->data, array->size);
    }

    case BL_GEOMETRY_TYPE_POLYGOND:
    case BL_GEOMETRY_TYPE_POLYLINED: {
      const BLArrayView<BLPoint>* array = static_cast<const BLArrayView<BLPoint>*>(geometryData);
      if (BL_UNLIKELY(array->size < 3))
        return BL_SUCCESS;

      return blRasterContextImplFillUnsafePolygon<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, ctxI->fillRule(), array->data, array->size);
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
      return blRasterContextImplFillUnsafePath<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, ctxI->fillRule(), *path);
    }
  }
}

static BLResult BL_INLINE blRasterContextImplFillGeometryInternal(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerAsync& serializer,
  uint32_t geometryType, const void* geometryData) noexcept {

  const BLBox* box;
  BLBox temporaryBox;

  // Gotos are used to limit the inline function expansion as all the FillXXX
  // functions are inlined, this makes the binary a bit smaller as each call
  // to such function is expanded only once.
  switch (geometryType) {
    case BL_GEOMETRY_TYPE_RECTI: {
      return blRasterContextImplFillUnsafeRectI<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, *static_cast<const BLRectI*>(geometryData));
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
      return blRasterContextImplFillUnsafeBox<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, *box);
    }

    case BL_GEOMETRY_TYPE_POLYGONI:
    case BL_GEOMETRY_TYPE_POLYLINEI: {
      const BLArrayView<BLPointI>* array = static_cast<const BLArrayView<BLPointI>*>(geometryData);
      if (BL_UNLIKELY(array->size < 3))
        return BL_SUCCESS;

      return blRasterContextImplFillUnsafePolygon<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, ctxI->fillRule(), array->data, array->size);
    }

    case BL_GEOMETRY_TYPE_POLYGOND:
    case BL_GEOMETRY_TYPE_POLYLINED: {
      const BLArrayView<BLPoint>* array = static_cast<const BLArrayView<BLPoint>*>(geometryData);
      if (BL_UNLIKELY(array->size < 3))
        return BL_SUCCESS;

      return blRasterContextImplFillUnsafePolygon<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, ctxI->fillRule(), array->data, array->size);
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
        return blRasterContextImplFillUnsafePath<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, ctxI->fillRule(), *path);

      BL_FALLTHROUGH
    }

    default: {
      return blRasterContextImplFillUnsafeGeometry<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, ctxI->fillRule(), geometryType, geometryData);
    }
  }
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillGeometry(BLContextImpl* baseImpl, uint32_t geometryType, const void* geometryData) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BLRasterCoreCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  uint32_t status = blRasterContextImplPrepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_FILL), BL_RASTER_CONTEXT_NO_FILL_FLAGS);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return blRasterContextImplFillGeometryInternal(ctxI, serializer, geometryType, geometryData);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillPathD(BLContextImpl* baseImpl, const BLPathCore* path) noexcept {
  return blRasterContextImplFillGeometry<RenderingMode>(baseImpl, BL_GEOMETRY_TYPE_PATH, path);
}

// ============================================================================
// [BLRasterContext - Fill GlyphRun]
// ============================================================================

static BLResult blRasterContextImplFillGlyphRunInternal(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerSync& serializer,
  const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  BLRasterWorkData* workData = &ctxI->syncWorkData;
  BL_PROPAGATE(blRasterContextUtilFillGlyphRun(workData, BLRasterContextStateAccessor(ctxI), pt, font, glyphRun));

  return blRasterContextImplFillClippedEdges<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, BL_FILL_RULE_NON_ZERO);
}

static BLResult blRasterContextImplFillGlyphRunInternal(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerAsync& serializer,
  const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  return blRasterContextImplEnqueueFillOrStrokeGlyphRun<BL_RASTER_COMMAND_CATEGORY_CORE, BL_CONTEXT_OP_TYPE_FILL>(ctxI, serializer, pt, font, glyphRun);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillGlyphRunD(BLContextImpl* baseImpl, const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (blDownCast(font)->isNone())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  if (glyphRun->empty())
    return BL_SUCCESS;

  BLRasterCoreCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  if (blRasterContextImplPrepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_FILL), BL_RASTER_CONTEXT_NO_FILL_FLAGS) == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return blRasterContextImplFillGlyphRunInternal(ctxI, serializer, pt, font, glyphRun);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillGlyphRunI(BLContextImpl* baseImpl, const BLPointI* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BLPoint ptD(pt->x, pt->y);
  return blRasterContextImplFillGlyphRunD<RenderingMode>(baseImpl, &ptD, font, glyphRun);
}

// ============================================================================
// [BLRasterContext - Fill Text]
// ============================================================================

static BL_INLINE BLResult blRasterContextImplFillTextInternal(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerSync& serializer,
  const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) noexcept {

  BLGlyphBuffer& gb = ctxI->syncWorkData.glyphBuffer;

  BL_PROPAGATE(gb.setText(text, size, encoding));
  BL_PROPAGATE(blDownCast(font)->shape(gb));

  if (gb.empty())
    return BL_SUCCESS;

  return blRasterContextImplFillGlyphRunInternal(ctxI, serializer, pt, font, &gb.impl->glyphRun);
}

static BL_INLINE BLResult blRasterContextImplFillTextInternal(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerAsync& serializer,
  const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) noexcept {

  return blRasterContextImplEnqueueFillOrStrokeText<BL_RASTER_COMMAND_CATEGORY_CORE, BL_CONTEXT_OP_TYPE_FILL>(ctxI, serializer, pt, font, text, size, encoding);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillTextD(BLContextImpl* baseImpl, const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(blDownCast(font)->isNone()))
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  if (BL_UNLIKELY(encoding >= BL_TEXT_ENCODING_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLRasterCoreCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  if (blRasterContextImplPrepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_FILL), BL_RASTER_CONTEXT_NO_FILL_FLAGS) == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return blRasterContextImplFillTextInternal(ctxI, serializer, pt, font, text, size, encoding);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplFillTextI(BLContextImpl* baseImpl, const BLPointI* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) noexcept {
  BLPoint ptD(pt->x, pt->y);
  return blRasterContextImplFillTextD<RenderingMode>(baseImpl, &ptD, font, text, size, encoding);
}

// ============================================================================
// [BLRasterContext - Stroke Internals - Sync - Stroke Unsafe Geometry]
// ============================================================================

template<uint32_t Category>
static BL_INLINE BLResult blRasterContextImplStrokeUnsafeGeometry(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerSync& serializer,
  uint32_t geometryType, const void* geometryData) noexcept {

  BLRasterWorkData* workData = &ctxI->syncWorkData;
  BLPath* path;

  if (geometryType == BL_GEOMETRY_TYPE_PATH) {
    path = const_cast<BLPath*>(static_cast<const BLPath*>(geometryData));
  }
  else {
    path = &workData->tmpPath[3];
    path->clear();
    BL_PROPAGATE(path->addGeometry(geometryType, geometryData));
  }

  BL_PROPAGATE(blRasterContextUtilStrokeUnsafePath(workData, BLRasterContextStateAccessor(ctxI), path));
  return blRasterContextImplFillClippedEdges<Category>(ctxI, serializer, BL_FILL_RULE_NON_ZERO);
}

// ============================================================================
// [BLRasterContext - Stroke Geometry]
// ============================================================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokeGeometry(BLContextImpl* baseImpl, uint32_t geometryType, const void* geometryData) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BLRasterCoreCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  uint32_t status = blRasterContextImplPrepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_STROKE), BL_RASTER_CONTEXT_NO_STROKE_FLAGS);
  if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return blRasterContextImplStrokeUnsafeGeometry<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, geometryType, geometryData);
}

// ============================================================================
// [BLRasterContext - Stroke Path]
// ============================================================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokePathD(BLContextImpl* baseImpl, const BLPathCore* path) noexcept {
  return blRasterContextImplStrokeGeometry<RenderingMode>(baseImpl, BL_GEOMETRY_TYPE_PATH, path);
}

// ============================================================================
// [BLRasterContext - Stroke Rect]
// ============================================================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokeRectI(BLContextImpl* baseImpl, const BLRectI* rect) noexcept {
  return blRasterContextImplStrokeGeometry<RenderingMode>(baseImpl, BL_GEOMETRY_TYPE_RECTI, rect);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokeRectD(BLContextImpl* baseImpl, const BLRect* rect) noexcept {
  return blRasterContextImplStrokeGeometry<RenderingMode>(baseImpl, BL_GEOMETRY_TYPE_RECTD, rect);
}

// ============================================================================
// [BLRasterContext - Stroke GlyphRun]
// ============================================================================

static BLResult blRasterContextImplStrokeGlyphRunInternal(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerSync& serializer,
  const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  BLRasterWorkData* workData = &ctxI->syncWorkData;
  BL_PROPAGATE(blRasterContextUtilStrokeGlyphRun(workData, BLRasterContextStateAccessor(ctxI), pt, font, glyphRun));

  return blRasterContextImplFillClippedEdges<BL_RASTER_COMMAND_CATEGORY_CORE>(ctxI, serializer, BL_FILL_RULE_NON_ZERO);
}

static BLResult blRasterContextImplStrokeGlyphRunInternal(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerAsync& serializer,
  const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  return blRasterContextImplEnqueueFillOrStrokeGlyphRun<BL_RASTER_COMMAND_CATEGORY_CORE, BL_CONTEXT_OP_TYPE_STROKE>(ctxI, serializer, pt, font, glyphRun);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokeGlyphRunD(BLContextImpl* baseImpl, const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (blDownCast(font)->isNone())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  if (glyphRun->empty())
    return BL_SUCCESS;

  BLRasterCoreCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  if (blRasterContextImplPrepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_STROKE), BL_RASTER_CONTEXT_NO_STROKE_FLAGS) == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return blRasterContextImplStrokeGlyphRunInternal(ctxI, serializer, pt, font, glyphRun);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokeGlyphRunI(BLContextImpl* baseImpl, const BLPointI* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {
  BLPoint ptD(pt->x, pt->y);
  return blRasterContextImplStrokeGlyphRunD<RenderingMode>(baseImpl, &ptD, font, glyphRun);
}

// ============================================================================
// [BLRasterContext - Stroke Text]
// ============================================================================

static BL_INLINE BLResult blRasterContextImplStrokeTextInternal(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerSync& serializer,
  const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) noexcept {

  BLGlyphBuffer& gb = ctxI->syncWorkData.glyphBuffer;

  BL_PROPAGATE(gb.setText(text, size, encoding));
  BL_PROPAGATE(blDownCast(font)->shape(gb));

  if (gb.empty())
    return BL_SUCCESS;

  return blRasterContextImplStrokeGlyphRunInternal(ctxI, serializer, pt, font, &gb.impl->glyphRun);
}

static BL_INLINE BLResult blRasterContextImplStrokeTextInternal(
  BLRasterContextImpl* ctxI,
  BLRasterCoreCommandSerializerAsync& serializer,
  const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) noexcept {

  return blRasterContextImplEnqueueFillOrStrokeText<BL_RASTER_COMMAND_CATEGORY_CORE, BL_CONTEXT_OP_TYPE_STROKE>(ctxI, serializer, pt, font, text, size, encoding);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokeTextD(BLContextImpl* baseImpl, const BLPoint* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  if (blDownCast(font)->isNone())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  BLRasterCoreCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  if (blRasterContextImplPrepareFill(ctxI, serializer, ctxI->getStyle(BL_CONTEXT_OP_TYPE_STROKE), BL_RASTER_CONTEXT_NO_STROKE_FLAGS) == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
    return BL_SUCCESS;

  return blRasterContextImplStrokeTextInternal(ctxI, serializer, pt, font, text, size, encoding);
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplStrokeTextI(BLContextImpl* baseImpl, const BLPointI* pt, const BLFontCore* font, const void* text, size_t size, uint32_t encoding) noexcept {
  BLPoint ptD(pt->x, pt->y);
  return blRasterContextImplStrokeTextD<RenderingMode>(baseImpl, &ptD, font, text, size, encoding);
}

// ============================================================================
// [BLRasterContext - Blit Internals - Prepare Blit]
// ============================================================================

template<uint32_t RenderingMode>
static BL_INLINE uint32_t blRasterContextImplPrepareBlit(BLRasterContextImpl* ctxI, BLRasterBlitCommandSerializer<RenderingMode>& serializer, uint32_t alpha, uint32_t format) noexcept {
  BLCompOpSimplifyInfo simplifyInfo = ctxI->compOpSimplifyInfo[format];

  uint32_t solidId = simplifyInfo.solidId();
  uint32_t contextFlags = ctxI->contextFlags | solidId;
  uint32_t nopFlags = BL_RASTER_CONTEXT_NO_BLIT_FLAGS;

  serializer.initPipeline(simplifyInfo.signature());
  serializer.initCommand(alpha);

  // Likely case - composition flag doesn't lead to a solid fill and there are no
  // other 'NO_' flags so the rendering of this command should produce something.
  nopFlags &= contextFlags;
  if (nopFlags == 0)
    return BL_RASTER_CONTEXT_PREPARE_STATUS_FETCH;

  // Remove reserved flags we may have added to `nopFlags` if srcSolidId was
  // non-zero and add a possible condition flag to `nopFlags` if the composition
  // is NOP (DST-COPY).
  nopFlags &= ~BL_RASTER_CONTEXT_NO_RESERVED;
  nopFlags |= uint32_t(serializer.pipeSignature() == BLCompOpSimplifyInfo::dstCopy().signature());

  // The combination of a destination format, source format, and compOp results
  // in a solid fill, so initialize the command accordingly to `solidId` type.
  serializer.initFetchSolid(ctxI->solidFetchDataTable[solidId]);

  // If there are bits in `nopFlags` it means there is nothing to render as
  // compOp, style, alpha, or something else is nop/invalid.
  if (nopFlags != 0)
    return BL_RASTER_CONTEXT_PREPARE_STATUS_NOP;
  else
    return BL_RASTER_CONTEXT_PREPARE_STATUS_SOLID;
}

// ============================================================================
// [BLRasterContext - Blit Internals - Finalize Blit]
// ============================================================================

static BL_INLINE BLResult blRasterContextImplFinalizeBlit(BLRasterContextImpl* ctxI, BLRasterBlitCommandSerializerSync& serializer, BLResult result) noexcept {
  blUnused(ctxI, serializer);
  return result;
}

static BL_INLINE BLResult blRasterContextImplFinalizeBlit(BLRasterContextImpl* ctxI, BLRasterBlitCommandSerializerAsync& serializer, BLResult result) noexcept {
  if (!serializer.enqueued(ctxI))
    serializer.rollbackFetchData(ctxI);
  return result;
}

// ============================================================================
// [BLRasterContext - Blit Image]
// ============================================================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplBlitImageD(BLContextImpl* baseImpl, const BLPoint* pt, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLImageImpl* imgI = img->impl;

  int srcX = 0;
  int srcY = 0;
  int srcW = imgI->size.w;
  int srcH = imgI->size.h;
  BLPoint dst(*pt);

  if (imgArea) {
    unsigned maxW = unsigned(srcW) - unsigned(imgArea->x);
    unsigned maxH = unsigned(srcH) - unsigned(imgArea->y);

    if ((maxW > unsigned(srcW)) | (unsigned(imgArea->w) > maxW) |
        (maxH > unsigned(srcH)) | (unsigned(imgArea->h) > maxH))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    srcX = imgArea->x;
    srcY = imgArea->y;
    srcW = imgArea->w;
    srcH = imgArea->h;
  }

  BLRasterBlitCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  uint32_t status = blRasterContextImplPrepareBlit(ctxI, serializer, ctxI->globalAlphaI(), imgI->format);
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
      double dx1 = blMin(startX + double(srcW) * ctxI->finalMatrixFixed().m00, ctxI->finalClipBoxFixedD().x1);
      double dy1 = blMin(startY + double(srcH) * ctxI->finalMatrixFixed().m11, ctxI->finalClipBoxFixedD().y1);

      // Clipped out, invalid coordinates, or empty `imgArea`.
      if (!((dx0 < dx1) & (dy0 < dy1)))
        return BL_SUCCESS;

      int64_t startFx = blFloorToInt64(startX);
      int64_t startFy = blFloorToInt64(startY);

      int ix0 = blTruncToInt(dx0);
      int iy0 = blTruncToInt(dy0);
      int ix1 = blTruncToInt(dx1);
      int iy1 = blTruncToInt(dy1);

      if (!((startFx | startFy) & ctxI->precisionInfo.fpMaskI)) {
        // Pixel aligned blit. At this point we still don't know whether the area where
        // the pixels will be composited is aligned, but we know for sure that the pixels
        // of `src` image don't require any interpolation.
        int x0 = ix0 >> ctxI->precisionInfo.fpShiftI;
        int y0 = iy0 >> ctxI->precisionInfo.fpShiftI;
        int x1 = (ix1 + ctxI->precisionInfo.fpMaskI) >> ctxI->precisionInfo.fpShiftI;
        int y1 = (iy1 + ctxI->precisionInfo.fpMaskI) >> ctxI->precisionInfo.fpShiftI;

        int tx = int(startFx >> ctxI->precisionInfo.fpShiftI);
        int ty = int(startFy >> ctxI->precisionInfo.fpShiftI);

        srcX += x0 - tx;
        srcY += y0 - ty;

        serializer.fetchData()->initPatternSource(imgI, BLRectI(srcX, srcY, x1 - x0, y1 - y0));
        if (!serializer.fetchData()->setupPatternBlit(x0, y0))
          return BL_SUCCESS;
      }
      else {
        serializer.fetchData()->initPatternSource(imgI, BLRectI(srcX, srcY, srcW, srcH));
        if (!serializer.fetchData()->setupPatternFxFy(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, ctxI->hints().patternQuality, startFx, startFy))
          return BL_SUCCESS;
      }

      return blRasterContextImplFillClippedBoxU<BL_RASTER_COMMAND_CATEGORY_BLIT>(ctxI, serializer, BLBoxI(ix0, iy0, ix1, iy1));
    }
    else {
      BLMatrix2D m(ctxI->finalMatrix());
      m.translate(dst.x, dst.y);

      serializer.fetchData()->initPatternSource(imgI, BLRectI(srcX, srcY, srcW, srcH));
      if (!serializer.fetchData()->setupPatternAffine(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, ctxI->hints().patternQuality, m)) {
        serializer.rollbackFetchData(ctxI);
        return BL_SUCCESS;
      }
    }
  }

  BLBox finalBox(dst.x, dst.y, dst.x + double(srcW), dst.y + double(srcH));
  return blRasterContextImplFinalizeBlit(ctxI, serializer,
         blRasterContextImplFillUnsafeBox<BL_RASTER_COMMAND_CATEGORY_BLIT>(ctxI, serializer, finalBox));
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplBlitImageI(BLContextImpl* baseImpl, const BLPointI* pt, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLImageImpl* imgI = img->impl;

  if (!(ctxI->contextFlags & BL_RASTER_CONTEXT_INTEGRAL_TRANSLATION)) {
    BLPoint ptD(pt->x, pt->y);
    return blRasterContextImplBlitImageD<RenderingMode>(ctxI, &ptD, img, imgArea);
  }

  int srcX = 0;
  int srcY = 0;
  int srcW = imgI->size.w;
  int srcH = imgI->size.h;

  if (imgArea) {
    unsigned maxW = unsigned(srcW) - unsigned(imgArea->x);
    unsigned maxH = unsigned(srcH) - unsigned(imgArea->y);

    if ( (maxW > unsigned(srcW)) | (unsigned(imgArea->w) > maxW) |
         (maxH > unsigned(srcH)) | (unsigned(imgArea->h) > maxH) )
      return blTraceError(BL_ERROR_INVALID_VALUE);

    srcX = imgArea->x;
    srcY = imgArea->y;
    srcW = imgArea->w;
    srcH = imgArea->h;
  }

  BLBoxI dstBox;
  if (blRuntimeIs32Bit()) {
    BLOverflowFlag of = 0;

    int dx = blAddOverflow(pt->x, ctxI->translationI().x, &of);
    int dy = blAddOverflow(pt->y, ctxI->translationI().y, &of);

    int x0 = dx;
    int y0 = dy;
    int x1 = blAddOverflow(x0, srcW, &of);
    int y1 = blAddOverflow(y0, srcH, &of);

    if (BL_UNLIKELY(of))
      goto Use64Bit;

    x0 = blMax(x0, ctxI->finalClipBoxI().x0);
    y0 = blMax(y0, ctxI->finalClipBoxI().y0);
    x1 = blMin(x1, ctxI->finalClipBoxI().x1);
    y1 = blMin(y1, ctxI->finalClipBoxI().y1);

    // Clipped out.
    if ((x0 >= x1) | (y0 >= y1))
      return BL_SUCCESS;

    srcX += x0 - dx;
    srcY += y0 - dy;
    dstBox.reset(x0, y0, x1, y1);
  }
  else {
Use64Bit:
    int64_t dx = int64_t(pt->x) + ctxI->translationI().x;
    int64_t dy = int64_t(pt->y) + ctxI->translationI().y;

    int64_t x0 = dx;
    int64_t y0 = dy;
    int64_t x1 = x0 + int64_t(unsigned(srcW));
    int64_t y1 = y0 + int64_t(unsigned(srcH));

    x0 = blMax<int64_t>(x0, ctxI->finalClipBoxI().x0);
    y0 = blMax<int64_t>(y0, ctxI->finalClipBoxI().y0);
    x1 = blMin<int64_t>(x1, ctxI->finalClipBoxI().x1);
    y1 = blMin<int64_t>(y1, ctxI->finalClipBoxI().y1);

    // Clipped out.
    if ((x0 >= x1) | (y0 >= y1))
      return BL_SUCCESS;

    srcX += int(x0 - dx);
    srcY += int(y0 - dy);
    dstBox.reset(int(x0), int(y0), int(x1), int(y1));
  }

  BLRasterBlitCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  uint32_t status = blRasterContextImplPrepareBlit(ctxI, serializer, ctxI->globalAlphaI(), imgI->format);
  if (BL_UNLIKELY(status <= BL_RASTER_CONTEXT_PREPARE_STATUS_SOLID)) {
    if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
      return BL_SUCCESS;
  }
  else {
    BL_PROPAGATE(serializer.initFetchDataForBlit(ctxI));
    serializer.fetchData()->initPatternSource(imgI, BLRectI(srcX, srcY, dstBox.x1 - dstBox.x0, dstBox.y1 - dstBox.y0));
    if (!serializer.fetchData()->setupPatternBlit(dstBox.x0, dstBox.y0)) {
      serializer.rollbackFetchData(ctxI);
      return BL_SUCCESS;
    }
  }

  return blRasterContextImplFinalizeBlit(ctxI, serializer,
         blRasterContextImplFillClippedBoxA<BL_RASTER_COMMAND_CATEGORY_BLIT>(ctxI, serializer, dstBox));
}

// ============================================================================
// [BLRasterContext - Blit Scaled Image]
// ============================================================================

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplBlitScaledImageD(BLContextImpl* baseImpl, const BLRect* rect, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLImageImpl* imgI = img->impl;

  int srcX = 0;
  int srcY = 0;
  int srcW = imgI->size.w;
  int srcH = imgI->size.h;

  if (imgArea) {
    unsigned maxW = unsigned(srcW) - unsigned(imgArea->x);
    unsigned maxH = unsigned(srcH) - unsigned(imgArea->y);

    if ((maxW > unsigned(srcW)) | (unsigned(imgArea->w) > maxW) |
        (maxH > unsigned(srcH)) | (unsigned(imgArea->h) > maxH) )
      return blTraceError(BL_ERROR_INVALID_VALUE);

    srcX = imgArea->x;
    srcY = imgArea->y;
    srcW = imgArea->w;
    srcH = imgArea->h;
  }

  // Optimization: Don't go over all the transformations if the destination and source rects have the same size.
  if (rect->w == double(srcW) && rect->h == double(srcH))
    return blRasterContextImplBlitImageD<RenderingMode>(ctxI, reinterpret_cast<const BLPoint*>(rect), img, imgArea);

  BLRasterBlitCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  BLBox finalBox(rect->x, rect->y, rect->x + rect->w, rect->y + rect->h);

  uint32_t status = blRasterContextImplPrepareBlit(ctxI, serializer, ctxI->globalAlphaI(), imgI->format);
  if (BL_UNLIKELY(status <= BL_RASTER_CONTEXT_PREPARE_STATUS_SOLID)) {
    if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
      return BL_SUCCESS;
  }
  else {
    BLMatrix2D m(rect->w / double(srcW), 0.0,
                 0.0, rect->h / double(srcH),
                 rect->x, rect->y);
    blMatrix2DMultiply(m, m, ctxI->finalMatrix());

    BL_PROPAGATE(serializer.initFetchDataForBlit(ctxI));
    serializer.fetchData()->initPatternSource(imgI, BLRectI(srcX, srcY, srcW, srcH));
    if (!serializer.fetchData()->setupPatternAffine(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, ctxI->hints().patternQuality, m)) {
      serializer.rollbackFetchData(ctxI);
      return BL_SUCCESS;
    }
  }

  return blRasterContextImplFinalizeBlit(ctxI, serializer,
         blRasterContextImplFillUnsafeBox<BL_RASTER_COMMAND_CATEGORY_BLIT>(ctxI, serializer, finalBox));
}

template<uint32_t RenderingMode>
static BLResult BL_CDECL blRasterContextImplBlitScaledImageI(BLContextImpl* baseImpl, const BLRectI* rect, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLImageImpl* imgI = img->impl;

  int srcX = 0;
  int srcY = 0;
  int srcW = imgI->size.w;
  int srcH = imgI->size.h;

  if (imgArea) {
    unsigned maxW = unsigned(srcW) - unsigned(imgArea->x);
    unsigned maxH = unsigned(srcH) - unsigned(imgArea->y);

    if ( (maxW > unsigned(srcW)) | (unsigned(imgArea->w) > maxW) |
         (maxH > unsigned(srcH)) | (unsigned(imgArea->h) > maxH) )
      return blTraceError(BL_ERROR_INVALID_VALUE);

    srcX = imgArea->x;
    srcY = imgArea->y;
    srcW = imgArea->w;
    srcH = imgArea->h;
  }

  // Optimization: Don't go over all the transformations if the destination and source rects have the same size.
  if (rect->w == srcW && rect->h == srcH)
    return blRasterContextImplBlitImageI<RenderingMode>(ctxI, reinterpret_cast<const BLPointI*>(rect), img, imgArea);

  BLRasterBlitCommandSerializer<RenderingMode> serializer;
  BL_PROPAGATE(serializer.initStorage(ctxI));

  uint32_t status = blRasterContextImplPrepareBlit(ctxI, serializer, ctxI->globalAlphaI(), imgI->format);
  BLBox finalBox(double(rect->x),
                 double(rect->y),
                 double(rect->x) + double(rect->w),
                 double(rect->y) + double(rect->h));

  if (status <= BL_RASTER_CONTEXT_PREPARE_STATUS_SOLID) {
    if (status == BL_RASTER_CONTEXT_PREPARE_STATUS_NOP)
      return BL_SUCCESS;
  }
  else {
    BLMatrix2D m(double(rect->w) / double(srcW), 0.0,
                 0.0, double(rect->h) / double(srcH),
                 double(rect->x), double(rect->y));
    blMatrix2DMultiply(m, m, ctxI->finalMatrix());

    BL_PROPAGATE(serializer.initFetchDataForBlit(ctxI));
    serializer.fetchData()->initPatternSource(imgI, BLRectI(srcX, srcY, srcW, srcH));
    if (!serializer.fetchData()->setupPatternAffine(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, ctxI->hints().patternQuality, m)) {
      serializer.rollbackFetchData(ctxI);
      return BL_SUCCESS;
    }
  }

  return blRasterContextImplFinalizeBlit(ctxI, serializer,
         blRasterContextImplFillUnsafeBox<BL_RASTER_COMMAND_CATEGORY_BLIT>(ctxI, serializer, finalBox));
}

// ============================================================================
// [BLRasterContext - Attach / Detach]
// ============================================================================

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
    uint32_t bandHeightShift = blBitCtz(bandHeight);
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

  uint32_t format = image->impl->format;
  BLSizeI size = image->impl->size;

  // TODO: [Rendering Context] Hardcoded for 8bpc.
  uint32_t formatPrecision = BL_RASTER_CONTEXT_FORMAT_PRECISION_8BPC;

  uint32_t bandHeight = blRasterContextImplCalculateBandHeight(format, size, options);
  uint32_t bandCount = (uint32_t(size.h) + bandHeight - 1) >> blBitCtz(bandHeight);

  // Initialization.
  BLResult result = BL_SUCCESS;
  BLPipeRuntime* pipeRuntime = nullptr;

  // If anything fails we would restore the zone state to match this point.
  BLZoneAllocator& baseZone = ctxI->baseZone;
  BLZoneAllocator::StatePtr zoneState = baseZone.saveState();

  // Not a real loop, just a scope we can escape early through 'break'.
  do {
    // Step 1: Initialize edge storage of the sync worker.
    result = ctxI->syncWorkData.initBandData(bandHeight, bandCount);
    if (result != BL_SUCCESS) break;

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
    pipeRuntime = &BLPipeGenRuntime::_global;

    if (options->flags & BL_CONTEXT_CREATE_FLAG_ISOLATED_JIT) {
      // Create an isolated `BLPipeGenRuntime` if specified. It will be used
      // to store all functions generated during the rendering and will be
      // destroyed together with the context.
      BLPipeGenRuntime* isolatedRT = baseZone.newT<BLPipeGenRuntime>(BL_PIPE_RUNTIME_FLAG_ISOLATED);

      // This should not really happen as the first block is allocated with the impl.
      if (BL_UNLIKELY(!isolatedRT)) {
        result = blTraceError(BL_ERROR_OUT_OF_MEMORY);
        break;
      }

      // Feature restrictions are related to JIT compiler - it allows us to test the
      // code generated by JIT with less features than the current CPU has, to make
      // sure that we support older hardware or to compare between implementations.
      if (options->flags & BL_CONTEXT_CREATE_FLAG_OVERRIDE_CPU_FEATURES) {
        isolatedRT->_restrictFeatures(options->cpuFeatures);
      }

      pipeRuntime = isolatedRT;
    }
#else
    pipeRuntime = &BLFixedPipeRuntime::_global;
#endif

    // Step 4: Make the destination image mutable.
    result = blImageMakeMutable(image, &ctxI->dstData);
    if (result != BL_SUCCESS) break;
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
      if (pipeRuntime->runtimeFlags() & BL_PIPE_RUNTIME_FLAG_ISOLATED)
        pipeRuntime->destroy();
    }

    baseZone.restoreState(zoneState);
    // TODO: [Rendering Context]
    // ctxI->jobZone.clear();
    return result;
  }

  if (!ctxI->isSync())
    ctxI->virt = &blRasterContextAsyncVirt;

  // Increase `writerCount` of the image, will be decreased by `blRasterContextImplDetach()`.
  BLInternalImageImpl* imageI = blInternalCast(image->impl);
  blAtomicFetchAdd(&imageI->writerCount);
  ctxI->dstImage.impl = imageI;

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
  ctxI->precisionInfo = blRasterContextPrecisionInfo[formatPrecision];
  ctxI->fpMinSafeCoordD = blFloor(double(blMinValue<int32_t>() + 1) * ctxI->fpScaleD());
  ctxI->fpMaxSafeCoordD = blFloor(double(blMaxValue<int32_t>() - 1 - blMax(size.w, size.h)) * ctxI->fpScaleD());

  // Initialize members that are related to alpha blending and composition.
  ctxI->solidFormatTable[BL_RASTER_CONTEXT_SOLID_FORMAT_ARGB] = uint8_t(BL_FORMAT_PRGB32);
  ctxI->solidFormatTable[BL_RASTER_CONTEXT_SOLID_FORMAT_FRGB] = uint8_t(BL_FORMAT_FRGB32);
  ctxI->solidFormatTable[BL_RASTER_CONTEXT_SOLID_FORMAT_ZERO] = uint8_t(BL_FORMAT_ZERO32);
  ctxI->solidFetchDataTable = formatPrecision == BL_RASTER_CONTEXT_FORMAT_PRECISION_16BPC
    ? reinterpret_cast<const BLPipeFetchData::Solid*>(blRasterContextSolidDataRgba64)
    : reinterpret_cast<const BLPipeFetchData::Solid*>(blRasterContextSolidDataRgba32);

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
  ctxI->internalState.styleType[BL_CONTEXT_OP_TYPE_FILL] = uint8_t(BL_STYLE_TYPE_SOLID);
  ctxI->internalState.styleType[BL_CONTEXT_OP_TYPE_STROKE] = uint8_t(BL_STYLE_TYPE_SOLID);
  memset(ctxI->internalState.reserved, 0, sizeof(ctxI->internalState.reserved));
  ctxI->internalState.approximationOptions = blMakeDefaultApproximationOptions();
  ctxI->internalState.globalAlpha = 1.0;
  ctxI->internalState.styleAlpha[0] = 1.0;
  ctxI->internalState.styleAlpha[1] = 1.0;
  blCallCtor(ctxI->internalState.strokeOptions);
  ctxI->internalState.metaMatrix.reset();
  ctxI->internalState.userMatrix.reset();
  ctxI->internalState.savedStateCount = 0;

  // Initialize private state.
  ctxI->internalState.metaMatrixType = BL_MATRIX2D_TYPE_TRANSLATE;
  ctxI->internalState.finalMatrixType = BL_MATRIX2D_TYPE_TRANSLATE;
  ctxI->internalState.metaMatrixFixedType = BL_MATRIX2D_TYPE_SCALE;
  ctxI->internalState.finalMatrixFixedType = BL_MATRIX2D_TYPE_SCALE;
  ctxI->internalState.globalAlphaI = uint32_t(ctxI->precisionInfo.fullAlphaI);

  ctxI->internalState.finalMatrix.reset();
  ctxI->internalState.metaMatrixFixed.resetToScaling(ctxI->precisionInfo.fpScaleD);
  ctxI->internalState.finalMatrixFixed.resetToScaling(ctxI->precisionInfo.fpScaleD);
  ctxI->internalState.translationI.reset(0, 0);

  ctxI->internalState.metaClipBoxI.reset(0, 0, size.w, size.h);
  // `finalClipBoxI` and `finalClipBoxD` are initialized by `blRasterContextImplResetClippingToMetaClipBox()`.

  // Make sure the state is initialized properly.
  blRasterContextImplCompOpChanged(ctxI);
  blRasterContextImplFlattenToleranceChanged(ctxI);
  blRasterContextImplOffsetParameterChanged(ctxI);
  blRasterContextImplResetClippingToMetaClipBox(ctxI);

  // Initialize styles.
  blRasterContextInitStyleToDefault(ctxI, ctxI->internalState.style[0], uint32_t(ctxI->precisionInfo.fullAlphaI));
  blRasterContextInitStyleToDefault(ctxI, ctxI->internalState.style[1], uint32_t(ctxI->precisionInfo.fullAlphaI));

  return BL_SUCCESS;
}

static BLResult blRasterContextImplDetach(BLRasterContextImpl* ctxI) noexcept {
  // Release the ImageImpl.
  BLInternalImageImpl* imageI = blInternalCast(ctxI->dstImage.impl);
  BL_ASSERT(imageI != nullptr);

  blRasterContextImplFlush(ctxI, BL_CONTEXT_FLUSH_SYNC);

  // If the image was dereferenced during rendering it's our responsibility to
  // destroy it. This is not useful from the consumer's perspective as the
  // resulting image can never be used again, but it can happen in some cases
  // (for example when an asynchronous rendering is terminated and the target
  // image released with it).
  if (blAtomicFetchSub(&imageI->writerCount) == 1) {
    if (imageI->refCount == 0)
      blImageImplDelete(imageI);
  }
  ctxI->dstImage.impl = nullptr;
  ctxI->dstData.reset();

  // Release Threads/WorkerContexts used by asynchronous rendering.
  if (ctxI->workerMgrInitialized)
    ctxI->workerMgr->reset();

  // Release PipeRuntime.
  if (ctxI->pipeProvider.runtime()->runtimeFlags() & BL_PIPE_RUNTIME_FLAG_ISOLATED)
    ctxI->pipeProvider.runtime()->destroy();
  ctxI->pipeProvider.reset();

  // Release all states.
  //
  // Important as the user doesn't have to restore all states, in that
  // case we basically need to iterate over all of them and release
  // resources they hold.
  blRasterContextImplDiscardStates(ctxI, nullptr);
  blCallDtor(ctxI->internalState.strokeOptions);

  uint32_t contextFlags = ctxI->contextFlags;
  if (contextFlags & BL_RASTER_CONTEXT_FILL_FETCH_DATA)
    blRasterContextImplDestroyValidStyle(ctxI, &ctxI->internalState.style[BL_CONTEXT_OP_TYPE_FILL]);

  if (contextFlags & BL_RASTER_CONTEXT_STROKE_FETCH_DATA)
    blRasterContextImplDestroyValidStyle(ctxI, &ctxI->internalState.style[BL_CONTEXT_OP_TYPE_STROKE]);

  // Clear other important members. We don't have to clear everything as if
  // we re-attach an image again all members will be overwritten anyway.
  ctxI->contextFlags = 0;

  ctxI->baseZone.clear();
  ctxI->fetchDataPool.reset();
  ctxI->savedStatePool.reset();
  ctxI->syncWorkData.ctxData.reset();
  ctxI->syncWorkData.workZone.clear();

  return BL_SUCCESS;
}

// ============================================================================
// [BLRasterContext - Init / Destroy]
// ============================================================================

BLResult blRasterContextImplCreate(BLContextImpl** out, BLImageCore* image, const BLContextCreateInfo* options) noexcept {
  uint16_t memPoolData;

  BLRasterContextImpl* ctxI = blRuntimeAllocAlignedImplT<BLRasterContextImpl>(
    sizeof(BLRasterContextImpl), alignof(BLRasterContextImpl), &memPoolData);

  if (BL_UNLIKELY(!ctxI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  ctxI = new(ctxI) BLRasterContextImpl(&blRasterContextSyncVirt, memPoolData);
  BLResult result = blRasterContextImplAttach(ctxI, image, options);

  if (result != BL_SUCCESS) {
    ctxI->virt->destroy(ctxI);
    return result;
  }

  *out = ctxI;
  return BL_SUCCESS;
}

static BLResult BL_CDECL blRasterContextImplDestroy(BLContextImpl* baseImpl) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (ctxI->dstImage.impl)
    blRasterContextImplDetach(ctxI);

  uint32_t memPoolData = ctxI->memPoolData;
  ctxI->~BLRasterContextImpl();
  return blRuntimeFreeImpl(ctxI, sizeof(BLRasterContextImpl), memPoolData);
}

// ============================================================================
// [BLRasterContext - Runtime]
// ============================================================================

template<uint32_t RenderingMode>
static void blRasterContextVirtInit(BLContextVirt* virt) noexcept {
  constexpr uint32_t F = BL_CONTEXT_OP_TYPE_FILL;
  constexpr uint32_t S = BL_CONTEXT_OP_TYPE_STROKE;

  virt->destroy                 = blRasterContextImplDestroy;
  virt->flush                   = blRasterContextImplFlush;

  virt->queryProperty           = blRasterContextImplQueryProperty;

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
  virt->setStyleObject[F]       = blRasterContextImplSetStyleObject<F>;
  virt->setStyleObject[S]       = blRasterContextImplSetStyleObject<S>;

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

void blRasterContextOnInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);
  blRasterContextVirtInit<BL_RASTER_RENDERING_MODE_SYNC>(&blRasterContextSyncVirt);
  blRasterContextVirtInit<BL_RASTER_RENDERING_MODE_ASYNC>(&blRasterContextAsyncVirt);
}
