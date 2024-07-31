// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../compopinfo_p.h"
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
#include "../pipeline/piperuntime_p.h"
#include "../pixelops/scalar_p.h"
#include "../pipeline/reference/fixedpiperuntime_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rastercontext_p.h"
#include "../raster/rastercontextops_p.h"
#include "../raster/rendercommand_p.h"
#include "../raster/rendercommandprocsync_p.h"
#include "../raster/rendertargetinfo_p.h"
#include "../raster/workerproc_p.h"
#include "../support/bitops_p.h"
#include "../support/intops_p.h"
#include "../support/stringops_p.h"
#include "../support/traits_p.h"
#include "../support/zeroallocator_p.h"

#ifndef BL_BUILD_NO_JIT
  #include "../pipeline/jit/pipegenruntime_p.h"
#endif

namespace bl {
namespace RasterEngine {

static constexpr bool kNoBail = false;

static constexpr RenderingMode kSync = RenderingMode::kSync;
static constexpr RenderingMode kAsync = RenderingMode::kAsync;

// bl::RasterEngine - ContextImpl - Globals
// ========================================

static BLContextVirt rasterImplVirtSync;
static BLContextVirt rasterImplVirtAsync;

// bl::RasterEngine - ContextImpl - Tables
// =======================================

struct SolidDataWrapperU8 {
  Pipeline::Signature signature;
  uint32_t dummy1;
  uint64_t dummy2;
  uint32_t prgb32;
  uint32_t padding;
};

struct SolidDataWrapperU16 {
  Pipeline::Signature signature;
  uint32_t dummy1;
  uint64_t dummy2;
  uint64_t prgb64;
};

static const constexpr SolidDataWrapperU8 solidOverrideFillU8[] = {
  { {0u}, 0u, 0u, 0x00000000u, 0u }, // kNotModified.
  { {0u}, 0u, 0u, 0x00000000u, 0u }, // kTransparent.
  { {0u}, 0u, 0u, 0xFF000000u, 0u }, // kOpaqueBlack.
  { {0u}, 0u, 0u, 0xFFFFFFFFu, 0u }, // kOpaqueWhite.
  { {0u}, 0u, 0u, 0x00000000u, 0u }  // kAlwaysNop.
};

static const constexpr SolidDataWrapperU16 solidOverrideFillU16[] = {
  { {0u}, 0u, 0u, 0x0000000000000000u }, // kNotModified.
  { {0u}, 0u, 0u, 0x0000000000000000u }, // kTransparent.
  { {0u}, 0u, 0u, 0xFFFF000000000000u }, // kOpaqueBlack.
  { {0u}, 0u, 0u, 0xFFFFFFFFFFFFFFFFu }, // kOpaqueWhite.
  { {0u}, 0u, 0u, 0x0000000000000000u }  // kAlwaysNop.
};

static const uint8_t textByteSizeShiftByEncoding[] = { 0, 1, 2, 0 };

// bl::RasterEngine - ContextImpl - Internals - Uncategorized Yet
// ==============================================================

static BL_INLINE FormatExt formatFromRgba32(uint32_t rgba32) noexcept {
  return rgba32 == 0x00000000u ? FormatExt::kZERO32 :
         rgba32 >= 0xFF000000u ? FormatExt::kFRGB32 : FormatExt::kPRGB32;
}

// bl::RasterEngine - ContextImpl - Internals - Dispatch Info / Style
// ==================================================================

// We want to pass some data from the frontend down during the dispatching. Ideally, we just want to pass this data
// as value in registers. To minimize the registers required to pass some values as parameters, we can use 64-bit
// type on 64-bit target, which would save us one register to propagate.
union DispatchInfo {
  //! \name Members
  //! \{

  uint64_t packed;
  struct {
#if BL_BYTE_ORDER == 1234
    Pipeline::Signature signature;
    uint32_t alpha;
#else
    uint32_t alpha;
    Pipeline::Signature signature;
#endif
  };

  //! \}

  //! \name Interface
  //! \{

  BL_INLINE_NODEBUG void init(Pipeline::Signature signatureValue, uint32_t alphaValue) noexcept {
    alpha = alphaValue;
    signature = signatureValue;
  }

  BL_INLINE_NODEBUG void addSignature(Pipeline::Signature sgn) noexcept {
#if BL_TARGET_ARCH_BITS >= 64
    packed |= sgn.value;
#else
    signature |= sgn;
#endif
  }

  BL_INLINE_NODEBUG void addFillType(Pipeline::FillType fillType) noexcept {
    addSignature(Pipeline::Signature::fromFillType(fillType));
  }

  //! \}
};

//! Another data that is passed by value during a render call dispatching.
struct DispatchStyle {
  //! \name Members
  //! \{

  RenderFetchDataHeader* fetchData;

  //! \}
};

// bl::RasterEngine - ContextImpl - Internals - DirectStateAccessor
// ================================================================

class DirectStateAccessor {
public:
  const BLRasterContextImpl* ctxI;

  BL_INLINE_NODEBUG explicit DirectStateAccessor(const BLRasterContextImpl* ctxI) noexcept : ctxI(ctxI) {}

  BL_INLINE_NODEBUG const BLBox& finalClipBoxD() const noexcept { return ctxI->finalClipBoxD(); }
  BL_INLINE_NODEBUG const BLBox& finalClipBoxFixedD() const noexcept { return ctxI->finalClipBoxFixedD(); }

  BL_INLINE_NODEBUG const BLStrokeOptions& strokeOptions() const noexcept { return ctxI->strokeOptions(); }
  BL_INLINE_NODEBUG const BLApproximationOptions& approximationOptions() const noexcept { return ctxI->approximationOptions(); }

  BL_INLINE_NODEBUG BLTransformType metaTransformFixedType() const noexcept { return ctxI->metaTransformFixedType(); }
  BL_INLINE_NODEBUG BLTransformType finalTransformFixedType() const noexcept { return ctxI->finalTransformFixedType(); }

  BL_INLINE BLMatrix2D userTransform() const noexcept {
    const BLMatrix2D& t = ctxI->userTransform();
    return BLMatrix2D(t.m00, t.m01, t.m10, t.m11, 0.0, 0.0);
  }

  BL_INLINE BLMatrix2D finalTransformFixed(const BLPoint& originFixed) const noexcept {
    const BLMatrix2D& t = ctxI->finalTransformFixed();
    return BLMatrix2D(t.m00, t.m01, t.m10, t.m11, originFixed.x, originFixed.y);
  }

  BL_INLINE BLMatrix2D metaTransformFixed(const BLPoint& originFixed) const noexcept {
    const BLMatrix2D& t = ctxI->metaTransformFixed();
    return BLMatrix2D(t.m00, t.m01, t.m10, t.m11, originFixed.x, originFixed.y);
  }
};

// bl::RasterEngine - ContextImpl - Internals - SyncWorkState
// ==========================================================

//! State that is used by the synchronous rendering context when using `syncWorkData` to execute the work
//! in user thread. Some properties of `WorkData` are used as states, and those have to be saved/restored.
class SyncWorkState {
public:
  BLBox _clipBoxD;

  BL_INLINE_NODEBUG void save(const WorkData& workData) noexcept { _clipBoxD = workData.edgeBuilder._clipBoxD; }
  BL_INLINE_NODEBUG void restore(WorkData& workData) const noexcept { workData.edgeBuilder._clipBoxD = _clipBoxD; }
};

// bl::RasterEngine - ContextImpl - Internals - Core State
// =======================================================

static BL_INLINE void onBeforeConfigChange(BLRasterContextImpl* ctxI) noexcept {
  if (blTestFlag(ctxI->contextFlags, ContextFlags::kWeakStateConfig)) {
    SavedState* state = ctxI->savedState;
    state->approximationOptions = ctxI->approximationOptions();
  }
}

static BL_INLINE void onAfterFlattenToleranceChanged(BLRasterContextImpl* ctxI) noexcept {
  ctxI->internalState.toleranceFixedD = ctxI->approximationOptions().flattenTolerance * ctxI->renderTargetInfo.fpScaleD;
  ctxI->syncWorkData.edgeBuilder.setFlattenToleranceSq(Math::square(ctxI->internalState.toleranceFixedD));
}

static BL_INLINE void onAfterOffsetParameterChanged(BLRasterContextImpl* ctxI) noexcept {
  blUnused(ctxI);
}

static BL_INLINE void onAfterCompOpChanged(BLRasterContextImpl* ctxI) noexcept {
  ctxI->compOpSimplifyInfo = compOpSimplifyInfoArrayOf(CompOpExt(ctxI->compOp()), ctxI->format());
}

// bl::RasterEngine - ContextImpl - Internals - Style State
// ========================================================

static BL_INLINE void initStyleToDefault(BLRasterContextImpl* ctxI, BLContextStyleSlot slot) noexcept {
  ctxI->internalState.styleType[slot] = uint8_t(BL_OBJECT_TYPE_RGBA32);

  StyleData& style = ctxI->internalState.style[slot];
  style = StyleData{};
  style.solid.initHeader(0, FormatExt(ctxI->solidFormatTable[BL_RASTER_CONTEXT_SOLID_FORMAT_FRGB]));
  style.solid.pipelineData = ctxI->solidOverrideFillTable[size_t(CompOpSolidId::kOpaqueBlack)].pipelineData;
  style.solid.original.rgba32.value = 0xFF000000u;
  style.makeFetchDataImplicit();
}

static BL_INLINE void destroyValidStyle(BLRasterContextImpl* ctxI, StyleData* style) noexcept {
  RenderFetchData* fetchData = static_cast<RenderFetchData*>(style->fetchData);
  fetchData->release(ctxI);
}

static BL_INLINE void onBeforeStyleChange(BLRasterContextImpl* ctxI, BLContextStyleSlot slot, StyleData& style, ContextFlags contextFlags) noexcept {
  if (blTestFlag(contextFlags, ContextFlags::kFetchDataBase << slot)) {
    if (!blTestFlag(contextFlags, ContextFlags::kWeakStateBaseStyle << slot)) {
      RenderFetchData* fetchData = style.getRenderFetchData();
      fetchData->release(ctxI);
      return;
    }
  }
  else {
    BL_ASSERT(blTestFlag(contextFlags, ContextFlags::kWeakStateBaseStyle << slot));
  }

  BL_ASSERT(ctxI->savedState != nullptr);
  ctxI->savedState->style[slot].copyFrom(style);
}

// bl::RasterEngine - ContextImpl - Internals - Fetch Data Initialization
// ======================================================================

// Recycle means that the FetchData was allocated by the rendering context 'setStyle()' function and it's pooled.
static void BL_CDECL recycleFetchDataImage(BLRasterContextImpl* ctxI, RenderFetchData* fetchData) noexcept {
  ImageInternal::releaseInstance(static_cast<BLImageCore*>(&fetchData->style));
  ctxI->freeFetchData(fetchData);
}

static void BL_CDECL recycleFetchDataPattern(BLRasterContextImpl* ctxI, RenderFetchData* fetchData) noexcept {
  PatternInternal::releaseInstance(static_cast<BLPatternCore*>(&fetchData->style));
  ctxI->freeFetchData(fetchData);
}

static void BL_CDECL recycleFetchDataGradient(BLRasterContextImpl* ctxI, RenderFetchData* fetchData) noexcept {
  GradientInternal::releaseInstance(static_cast<BLGradientCore*>(&fetchData->style));
  ctxI->freeFetchData(fetchData);
}

// Destroy is used exclusively by the multi-threaded rendering context implementation. This FetchData was allocated
// during a render call dispatch in which a Style has been passed explicitly to the call. This kind of FetchData is
// one-shot: only one reference to it exists.
static void BL_CDECL destroyFetchDataImage(BLRasterContextImpl* ctxI, RenderFetchData* fetchData) noexcept {
  blUnused(ctxI);
  ImageInternal::releaseInstance(static_cast<BLImageCore*>(&fetchData->style));
}

static void BL_CDECL destroyFetchDataGradient(BLRasterContextImpl* ctxI, RenderFetchData* fetchData) noexcept {
  blUnused(ctxI);
  GradientInternal::releaseInstance(static_cast<BLGradientCore*>(&fetchData->style));
}

// Creating FetchData
// ------------------

// There are in general two ways FetchData can be created:
//
//   - using 'BLContext::setStyle()'
//   - passing Style explicitly to the frontend function suffixed by 'Ext'
//   - passing an Image to a 'blitImage' frontend function
//
// When FetchData is created by setStyle() it will become part of the rendering context State, which means that
// such FetchData can be saved, restored, reused, etc... The rendering context uses a reference count to keep
// track of such FetchData and must maintain additional properties to make getStyle() working.
//
// On the other hand, when FetchData is created from an explicitly passed style / image to a render call, it's
// only used once and doesn't need anything to be able to get that style in the future, which means that it's
// much easier to manage.
//
// Applier is used with initNonSolidFetchData function to unify both concepts and to share code.

class NonSolidFetchStateApplier {
public:
  static constexpr bool kIsExplicit = false;

  ContextFlags _contextFlags;
  ContextFlags _styleFlags;
  BLContextStyleSlot _slot;

  BL_INLINE_NODEBUG NonSolidFetchStateApplier(ContextFlags contextFlags, BLContextStyleSlot slot) noexcept
    : _contextFlags(contextFlags),
      _styleFlags(ContextFlags::kFetchDataBase),
      _slot(slot) {}

  BL_INLINE void initStyleType(BLRasterContextImpl* ctxI, BLObjectType styleType) noexcept {
    ctxI->internalState.styleType[_slot] = uint8_t(styleType);
  }

  BL_INLINE void initComputedTransform(BLRasterContextImpl* ctxI, const BLMatrix2D& transform, BLTransformType transformType) noexcept {
    if (transformType >= BL_TRANSFORM_TYPE_INVALID)
      markAsNop();
    ctxI->internalState.style[_slot].nonSolid.adjustedTransform = transform;
  }

  BL_INLINE void markAsNop() noexcept {
    _styleFlags |= ContextFlags::kNoBaseStyle;
  }

  BL_INLINE bool finalize(BLRasterContextImpl* ctxI) noexcept {
    ctxI->contextFlags = _contextFlags | (_styleFlags << uint32_t(_slot));
    return true;
  }
};

class NonSolidFetchExplicitApplier {
public:
  static constexpr bool kIsExplicit = true;

  BL_INLINE_NODEBUG NonSolidFetchExplicitApplier() noexcept {}

  BL_INLINE void initStyleType(BLRasterContextImpl* ctxI, BLObjectType styleType) noexcept {
    blUnused(ctxI, styleType);
  }

  BL_INLINE void initComputedTransform(BLRasterContextImpl* ctxI, const BLMatrix2D& transform, BLTransformType transformType) noexcept {
    blUnused(ctxI, transform, transformType);
  }

  BL_INLINE void markAsNop() noexcept {
  }

  BL_INLINE bool finalize(BLRasterContextImpl* ctxI) noexcept {
    blUnused(ctxI);
    return true;
  }
};

template<typename Applier>
static BL_INLINE bool initNonSolidFetchData(
    BLRasterContextImpl* ctxI,
    RenderFetchData* fetchData,
    const BLObjectCore* style, BLObjectType styleType, BLContextStyleTransformMode transformMode, Applier& applier) noexcept {

  const BLMatrix2D* transform = ctxI->transformPtrs[transformMode];
  BLTransformType transformType = BLTransformType(ctxI->internalState.transformTypes[transformMode]);
  BLMatrix2D transformStorage;

  applier.initStyleType(ctxI, styleType);
  Pipeline::Signature pendingBit{0};

  switch (styleType) {
    case BL_OBJECT_TYPE_PATTERN: {
      const BLPattern* pattern = static_cast<const BLPattern*>(style);
      BLPatternImpl* patternI = PatternInternal::getImpl(pattern);
      BLImageCore* image = &patternI->image;

      if BL_CONSTEXPR (Applier::kIsExplicit) {
        // Reinitialize this style to use the image instead of the pattern if this is an explicit operation.
        // The reason is that we don't need the BLPattern data once the FetchData is initialized, so if the
        // user reinitializes the pattern for multiple calls we would save one memory allocation each time
        // the pattern is reinitialized.
        fetchData->initStyleObject(image);
        fetchData->initDestroyFunc(destroyFetchDataImage);
      }
      else {
        fetchData->initDestroyFunc(recycleFetchDataPattern);
      }

      // NOTE: The area comes from pattern, it means that it's the pattern's responsibility to make sure that it's valid.
      BLRectI area = patternI->area;

      if (!area.w || !area.h) {
        applier.markAsNop();
        if BL_CONSTEXPR (Applier::kIsExplicit)
          return false;
      }

      BLTransformType styleTransformType = pattern->transformType();
      if (styleTransformType != BL_TRANSFORM_TYPE_IDENTITY) {
        TransformInternal::multiply(transformStorage, patternI->transform, *transform);
        transform = &transformStorage;
        styleTransformType = transformStorage.type();
      }
      applier.initComputedTransform(ctxI, *transform, transformType);

      BLPatternQuality quality = BLPatternQuality(ctxI->hints().patternQuality);
      BLExtendMode extendMode = PatternInternal::getExtendMode(pattern);
      BLImageImpl* imageI = ImageInternal::getImpl(image);

      fetchData->extra.format = uint8_t(imageI->format);
      fetchData->initImageSource(imageI, area);

      fetchData->signature = Pipeline::FetchUtils::initPatternAffine(
        fetchData->pipelineData.pattern, extendMode, quality, uint32_t(imageI->depth) / 8u, *transform);
      break;
    }

    case BL_OBJECT_TYPE_GRADIENT: {
      const BLGradient* gradient = static_cast<const BLGradient*>(style);
      BLGradientPrivateImpl* gradientI = GradientInternal::getImpl(gradient);

      fetchData->initStyleObject(gradient);
      if BL_CONSTEXPR (Applier::kIsExplicit)
        fetchData->initDestroyFunc(destroyFetchDataGradient);
      else
        fetchData->initDestroyFunc(recycleFetchDataGradient);

      BLTransformType styleTransformType = gradient->transformType();
      if (styleTransformType != BL_TRANSFORM_TYPE_IDENTITY) {
        TransformInternal::multiply(transformStorage, gradientI->transform, *transform);
        transform = &transformStorage;
        styleTransformType = transformStorage.type();
      }
      applier.initComputedTransform(ctxI, *transform, transformType);

      BLGradientInfo gradientInfo = GradientInternal::ensureInfo(gradientI);
      fetchData->extra.format = uint8_t(gradientInfo.format);

      if (gradientInfo.empty()) {
        applier.markAsNop();
        if BL_CONSTEXPR (Applier::kIsExplicit)
          return false;
      }
      else if (gradientInfo.solid) {
        // Using last color according to the SVG specification.
        uint32_t rgba32 = PixelOps::Scalar::cvt_prgb32_8888_from_argb32_8888(RgbaInternal::rgba32FromRgba64(gradientI->stops[gradientI->size - 1].rgba.value));
        fetchData->pipelineData.solid.prgb32 = rgba32;
      }
      else {
        BLGradientType type = GradientInternal::getGradientType(gradient);
        BLGradientQuality quality = BLGradientQuality(ctxI->hints().gradientQuality);
        BLExtendMode extendMode = GradientInternal::getExtendMode(gradient);

        // Do not dither gradients when rendering into A8 targets.
        if (ctxI->syncWorkData.ctxData.dst.format == BL_FORMAT_A8)
          quality = BL_GRADIENT_QUALITY_NEAREST;

        const void* lutData = nullptr;
        uint32_t lutSize = gradientInfo.lutSize(quality >= BL_GRADIENT_QUALITY_DITHER);

        BLGradientLUT* lut = gradientI->lut[size_t(quality >= BL_GRADIENT_QUALITY_DITHER)];
        if (lut)
          lutData = lut->data();

        // We have to store the quality somewhere as if this FetchData would be lazily materialized we have to
        // cache the desired quality and the size of the LUT calculated (to avoid going over GradientInfo again).
        fetchData->extra.custom[0] = uint8_t(quality);
        pendingBit = Pipeline::Signature::fromPendingFlag(!lut);

        fetchData->signature = Pipeline::FetchUtils::initGradient(
          fetchData->pipelineData.gradient, type, extendMode, quality, gradientI->values, lutData, lutSize, *transform);
      }
      break;
    }

    default:
      // The caller must ensure that this is not a solid case and the style type is valid.
      BL_NOT_REACHED();
  }

  if (fetchData->signature.hasPendingFlag()) {
    applier.markAsNop();
    if BL_CONSTEXPR (Applier::kIsExplicit)
      return false;
  }

  fetchData->signature |= pendingBit;
  return applier.finalize(ctxI);
}

// bl::RasterEngine - ContextImpl - Frontend - Fill & Stroke Style
// ===============================================================

static BL_INLINE_NODEBUG uint32_t restrictedIndexFromSlot(BLContextStyleSlot slot) noexcept {
  return blMin<uint32_t>(slot, uint32_t(BL_CONTEXT_STYLE_SLOT_MAX_VALUE) + 1);
}

static BLResult BL_CDECL getStyleImpl(const BLContextImpl* baseImpl, BLContextStyleSlot slot, bool transformed, BLVarCore* varOut) noexcept {
  const BLRasterContextImpl* ctxI = static_cast<const BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(slot > BL_CONTEXT_STYLE_SLOT_MAX_VALUE)) {
    blVarAssignNull(varOut);
    return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  BLObjectType styleType = BLObjectType(ctxI->internalState.styleType[slot]);
  const StyleData* style = &ctxI->internalState.style[slot];

  if (styleType <= BL_OBJECT_TYPE_NULL) {
    if (styleType == BL_OBJECT_TYPE_RGBA32)
      return blVarAssignRgba32(varOut, style->solid.original.rgba32.value);

    if (styleType == BL_OBJECT_TYPE_RGBA64)
      return blVarAssignRgba64(varOut, style->solid.original.rgba64.value);

    if (styleType == BL_OBJECT_TYPE_RGBA)
      return blVarAssignRgba(varOut, &style->solid.original.rgba);

    return blVarAssignNull(varOut);
  }

  const RenderFetchData* fetchData = style->getRenderFetchData();
  blVarAssignWeak(varOut, &fetchData->styleAs<BLVarCore>());

  if (!transformed)
    return BL_SUCCESS;

  switch (styleType) {
    case BL_OBJECT_TYPE_PATTERN:
      return varOut->dcast().as<BLPattern>().setTransform(style->nonSolid.adjustedTransform);

    case BL_OBJECT_TYPE_GRADIENT:
      return varOut->dcast().as<BLGradient>().setTransform(style->nonSolid.adjustedTransform);

    default:
      return blTraceError(BL_ERROR_INVALID_STATE);
  }
}

static BLResult BL_CDECL disableStyleImpl(BLContextImpl* baseImpl, BLContextStyleSlot slot) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  ContextFlags contextFlags = ctxI->contextFlags;
  ContextFlags styleFlags = (ContextFlags::kWeakStateBaseStyle | ContextFlags::kFetchDataBase) << restrictedIndexFromSlot(slot);

  if (blTestFlag(contextFlags, styleFlags)) {
    if (BL_UNLIKELY(slot > BL_CONTEXT_STYLE_SLOT_MAX_VALUE))
      return blTraceError(BL_ERROR_INVALID_VALUE);
    onBeforeStyleChange(ctxI, slot, ctxI->internalState.style[slot], contextFlags);
  }

  ctxI->contextFlags = (contextFlags & ~styleFlags) | ContextFlags::kNoBaseStyle << slot;
  ctxI->internalState.styleType[slot] = uint8_t(BL_OBJECT_TYPE_NULL);
  return BL_SUCCESS;
}

static BLResult BL_CDECL setStyleRgba32Impl(BLContextImpl* baseImpl, BLContextStyleSlot slot, uint32_t rgba32) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  ContextFlags contextFlags = ctxI->contextFlags;
  ContextFlags styleFlags = (ContextFlags::kWeakStateBaseStyle | ContextFlags::kFetchDataBase) << restrictedIndexFromSlot(slot);

  if (blTestFlag(contextFlags, styleFlags)) {
    if (BL_UNLIKELY(slot > BL_CONTEXT_STYLE_SLOT_MAX_VALUE))
      return blTraceError(BL_ERROR_INVALID_VALUE);
    onBeforeStyleChange(ctxI, slot, ctxI->internalState.style[slot], contextFlags);
  }

  StyleData& style = ctxI->internalState.style[slot];
  style.solid.original.rgba32.value = rgba32;

  uint32_t premultiplied = PixelOps::Scalar::cvt_prgb32_8888_from_argb32_8888(rgba32);
  FormatExt format = formatFromRgba32(rgba32);

  ctxI->contextFlags = contextFlags & ~(styleFlags | (ContextFlags::kNoBaseStyle << slot));
  ctxI->internalState.styleType[slot] = uint8_t(BL_OBJECT_TYPE_RGBA32);

  style.solid.initHeader(0, format);
  style.solid.pipelineData.prgb32 = premultiplied;
  style.makeFetchDataImplicit();

  return BL_SUCCESS;
}

static BLResult BL_CDECL setStyleRgba64Impl(BLContextImpl* baseImpl, BLContextStyleSlot slot, uint64_t rgba64) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  ContextFlags contextFlags = ctxI->contextFlags;
  ContextFlags styleFlags = (ContextFlags::kWeakStateBaseStyle | ContextFlags::kFetchDataBase) << restrictedIndexFromSlot(slot);

  if (blTestFlag(contextFlags, styleFlags)) {
    if (BL_UNLIKELY(slot > BL_CONTEXT_STYLE_SLOT_MAX_VALUE))
      return blTraceError(BL_ERROR_INVALID_VALUE);
    onBeforeStyleChange(ctxI, slot, ctxI->internalState.style[slot], contextFlags);
  }

  StyleData& style = ctxI->internalState.style[slot];
  style.solid.original.rgba64.value = rgba64;

  uint32_t rgba32 = RgbaInternal::rgba32FromRgba64(rgba64);
  uint32_t premultiplied = PixelOps::Scalar::cvt_prgb32_8888_from_argb32_8888(rgba32);
  FormatExt format = formatFromRgba32(rgba32);

  ctxI->contextFlags = contextFlags & ~(styleFlags | (ContextFlags::kNoBaseStyle << slot));
  ctxI->internalState.styleType[slot] = uint8_t(BL_OBJECT_TYPE_RGBA64);

  style.solid.initHeader(0, format);
  style.solid.pipelineData.prgb32 = premultiplied;
  style.makeFetchDataImplicit();

  return BL_SUCCESS;
}

static BLResult BL_CDECL setStyleRgbaImpl(BLContextImpl* baseImpl, BLContextStyleSlot slot, const BLRgba* rgba) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  ContextFlags contextFlags = ctxI->contextFlags;
  ContextFlags styleFlags = (ContextFlags::kWeakStateBaseStyle | ContextFlags::kFetchDataBase) << restrictedIndexFromSlot(slot);

  BLRgba norm = blClamp(*rgba, BLRgba(0.0f, 0.0f, 0.0f, 0.0f), BLRgba(1.0f, 1.0f, 1.0f, 1.0f));
  if (!RgbaInternal::isValid(*rgba))
    return disableStyleImpl(baseImpl, slot);

  if (blTestFlag(contextFlags, styleFlags)) {
    if (BL_UNLIKELY(slot > BL_CONTEXT_STYLE_SLOT_MAX_VALUE))
      return blTraceError(BL_ERROR_INVALID_VALUE);
    onBeforeStyleChange(ctxI, slot, ctxI->internalState.style[slot], contextFlags);
  }

  StyleData& style = ctxI->internalState.style[slot];
  style.solid.original.rgba = norm;

  // Premultiply and convert to RGBA32.
  float aScale = norm.a * 255.0f;
  uint32_t r = uint32_t(Math::roundToInt(norm.r * aScale));
  uint32_t g = uint32_t(Math::roundToInt(norm.g * aScale));
  uint32_t b = uint32_t(Math::roundToInt(norm.b * aScale));
  uint32_t a = uint32_t(Math::roundToInt(aScale));
  uint32_t premultiplied = BLRgba32(r, g, b, a).value;
  FormatExt format = formatFromRgba32(premultiplied);

  ctxI->contextFlags = contextFlags & ~(styleFlags | (ContextFlags::kNoBaseStyle << slot));
  ctxI->internalState.styleType[slot] = uint8_t(BL_OBJECT_TYPE_RGBA);

  style.solid.initHeader(0, format);
  style.solid.pipelineData.prgb32 = premultiplied;
  style.makeFetchDataImplicit();

  return BL_SUCCESS;
}

static BLResult BL_CDECL setStyleImpl(BLContextImpl* baseImpl, BLContextStyleSlot slot, const BLObjectCore* style, BLContextStyleTransformMode transformMode) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLObjectType styleType = style->_d.getType();

  if (styleType <= BL_OBJECT_TYPE_NULL) {
    if (styleType == BL_OBJECT_TYPE_RGBA32)
      return setStyleRgba32Impl(baseImpl, slot, style->_d.rgba32.value);

    if (styleType == BL_OBJECT_TYPE_RGBA64)
      return setStyleRgba64Impl(baseImpl, slot, style->_d.rgba64.value);

    if (styleType == BL_OBJECT_TYPE_RGBA)
      return setStyleRgbaImpl(baseImpl, slot, &style->_d.rgba);

    return disableStyleImpl(ctxI, slot);
  }

  if (BL_UNLIKELY(slot > BL_CONTEXT_STYLE_SLOT_MAX_VALUE || styleType > BL_OBJECT_TYPE_MAX_STYLE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  ContextFlags contextFlags = ctxI->contextFlags;
  ContextFlags styleFlags = (ContextFlags::kFetchDataBase | ContextFlags::kWeakStateBaseStyle) << slot;
  StyleData& styleState = ctxI->internalState.style[slot];

  RenderFetchData* fetchData = ctxI->allocFetchData();
  if (BL_UNLIKELY(!fetchData))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  if (blTestFlag(contextFlags, styleFlags))
    onBeforeStyleChange(ctxI, slot, styleState, contextFlags);

  fetchData->initHeader(1);
  fetchData->initStyleObject(style);
  ObjectInternal::retainInstance(style);

  styleState.fetchData = fetchData;
  contextFlags &= ~(styleFlags | (ContextFlags::kNoBaseStyle << slot));

  NonSolidFetchStateApplier applier(contextFlags, slot);
  initNonSolidFetchData(ctxI, fetchData, style, styleType, transformMode, applier);

  return BL_SUCCESS;
}

// bl::RasterEngine - ContextImpl - Internals - Stroke State
// =========================================================

static BL_INLINE void onBeforeStrokeChange(BLRasterContextImpl* ctxI) noexcept {
  if (blTestFlag(ctxI->contextFlags, ContextFlags::kWeakStateStrokeOptions)) {
    SavedState* state = ctxI->savedState;
    state->strokeOptions._copyFrom(ctxI->strokeOptions());
    ArrayInternal::retainInstance(&state->strokeOptions.dashArray);
  }
}

static BL_INLINE void onBeforeStrokeChangeAndDestroyDashArray(BLRasterContextImpl* ctxI) noexcept {
  if (blTestFlag(ctxI->contextFlags, ContextFlags::kWeakStateStrokeOptions)) {
    SavedState* state = ctxI->savedState;
    state->strokeOptions._copyFrom(ctxI->strokeOptions());
  }
  else {
    ArrayInternal::releaseInstance(&ctxI->internalState.strokeOptions.dashArray);
  }
}

// bl::RasterEngine - ContextImpl - Internals - Transform State
// ============================================================

// Called before `userTransform` is changed.
//
// This function is responsible for saving the current userTransform in case that the
// `ContextFlags::kWeakStateUserTransform` flag is set, which means that the userTransform
// must be saved before any modification.
static BL_INLINE void onBeforeUserTransformChange(BLRasterContextImpl* ctxI, Matrix2x2& before2x2) noexcept {
  before2x2.m[0] = ctxI->finalTransform().m00;
  before2x2.m[1] = ctxI->finalTransform().m01;
  before2x2.m[2] = ctxI->finalTransform().m10;
  before2x2.m[3] = ctxI->finalTransform().m11;

  if (blTestFlag(ctxI->contextFlags, ContextFlags::kWeakStateUserTransform)) {
    // Weak MetaTransform state must be set together with weak UserTransform.
    BL_ASSERT(blTestFlag(ctxI->contextFlags, ContextFlags::kWeakStateMetaTransform));

    SavedState* state = ctxI->savedState;
    state->altTransform = ctxI->finalTransform();
    state->userTransform = ctxI->userTransform();
  }
}

static BL_INLINE void updateFinalTransform(BLRasterContextImpl* ctxI) noexcept {
  TransformInternal::multiply(ctxI->internalState.finalTransform, ctxI->userTransform(), ctxI->metaTransform());
}

static BL_INLINE void updateMetaTransformFixed(BLRasterContextImpl* ctxI) noexcept {
  ctxI->internalState.metaTransformFixed = ctxI->metaTransform();
  ctxI->internalState.metaTransformFixed.postScale(ctxI->renderTargetInfo.fpScaleD);
}

static BL_INLINE void updateFinalTransformFixed(BLRasterContextImpl* ctxI) noexcept {
  ctxI->internalState.finalTransformFixed = ctxI->finalTransform();
  ctxI->internalState.finalTransformFixed.postScale(ctxI->renderTargetInfo.fpScaleD);
}

// Called after `userTransform` has been modified.
//
// Responsible for updating `finalTransform` and other matrix information.
static BL_INLINE void onAfterUserTransformChanged(BLRasterContextImpl* ctxI, const Matrix2x2& before2x2) noexcept {
  ContextFlags contextFlags = ctxI->contextFlags;

  contextFlags &= ~(ContextFlags::kNoUserTransform         |
                    ContextFlags::kInfoIntegralTranslation |
                    ContextFlags::kWeakStateUserTransform  );

  updateFinalTransform(ctxI);
  updateFinalTransformFixed(ctxI);

  const BLMatrix2D& ft = ctxI->finalTransformFixed();
  BLTransformType finalTransformType = ctxI->finalTransform().type();

  ctxI->internalState.finalTransformType = uint8_t(finalTransformType);
  ctxI->internalState.finalTransformFixedType = uint8_t(blMax<uint32_t>(finalTransformType, BL_TRANSFORM_TYPE_SCALE));

  if (finalTransformType <= BL_TRANSFORM_TYPE_TRANSLATE) {
    // No scaling - input coordinates have pixel granularity. Check if the translation has pixel granularity as well
    // and setup the `translationI` data for that case.
    if (ft.m20 >= ctxI->fpMinSafeCoordD && ft.m20 <= ctxI->fpMaxSafeCoordD &&
        ft.m21 >= ctxI->fpMinSafeCoordD && ft.m21 <= ctxI->fpMaxSafeCoordD) {
      // We need 64-bit ints here as we are already scaled. We also need a `floor` function as we have to handle
      // negative translations which cannot be truncated (the default conversion).
      int64_t tx64 = Math::floorToInt64(ft.m20);
      int64_t ty64 = Math::floorToInt64(ft.m21);

      // Pixel to pixel translation is only possible when both fixed points `tx64` and `ty64` have all zeros in
      // their fraction parts.
      if (((tx64 | ty64) & ctxI->renderTargetInfo.fpMaskI) == 0) {
        int tx = int(tx64 >> ctxI->renderTargetInfo.fpShiftI);
        int ty = int(ty64 >> ctxI->renderTargetInfo.fpShiftI);

        ctxI->setTranslationI(BLPointI(tx, ty));
        contextFlags |= ContextFlags::kInfoIntegralTranslation;
      }
    }
  }

  // Shared states are not invalidated when the transformation is just translated.
  uint32_t invalidateSharedState = uint32_t(before2x2.m[0] != ctxI->finalTransform().m00) |
                                   uint32_t(before2x2.m[1] != ctxI->finalTransform().m01) |
                                   uint32_t(before2x2.m[2] != ctxI->finalTransform().m10) |
                                   uint32_t(before2x2.m[3] != ctxI->finalTransform().m11);

  // Mark NoUserTransform in case that the transformation matrix is invalid.
  if (finalTransformType >= BL_TRANSFORM_TYPE_INVALID)
    contextFlags |= ContextFlags::kNoUserTransform;

  // Clear shared state flags if the shared state has been invalidated by the transformation.
  if (invalidateSharedState)
    contextFlags &= ~(ContextFlags::kSharedStateFill | ContextFlags::kSharedStateStrokeExt);

  ctxI->contextFlags = contextFlags;
}

// bl::RasterEngine - ContextImpl - Internals - Clip State
// =======================================================

static BL_INLINE void onBeforeClipBoxChange(BLRasterContextImpl* ctxI) noexcept {
  if (blTestFlag(ctxI->contextFlags, ContextFlags::kWeakStateClip)) {
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
    Math::truncToInt(ctxI->finalClipBoxD().x0),
    Math::truncToInt(ctxI->finalClipBoxD().y0),
    Math::ceilToInt(ctxI->finalClipBoxD().x1),
    Math::ceilToInt(ctxI->finalClipBoxD().y1));

  double fpScale = ctxI->renderTargetInfo.fpScaleD;
  ctxI->setFinalClipBoxFixedD(BLBox(
    ctxI->finalClipBoxD().x0 * fpScale,
    ctxI->finalClipBoxD().y0 * fpScale,
    ctxI->finalClipBoxD().x1 * fpScale,
    ctxI->finalClipBoxD().y1 * fpScale));
}

// bl::RasterEngine - ContextImpl - Internals - Clip Utilities
// ===========================================================

#if 0
// TODO: [Rendering Context] Experiment, not ready yet.
static BL_INLINE bool translateAndClipRectToFillI(const BLRasterContextImpl* ctxI, const BLRectI* srcRect, BLBoxI* dstBoxOut) noexcept {
  __m128d x0y0 = _mm_cvtepi32_pd(_mm_loadu_si64(&srcRect->x));
  __m128d wh = _mm_cvtepi32_pd(_mm_loadu_si64(&srcRect->w));

  x0y0 = _mm_add_pd(x0y0, *(__m128d*)&ctxI->internalState.finalTransform.m20);
  __m128d x1y1 = _mm_add_pd(x0y0, wh);

  x0y0 = _mm_max_pd(x0y0, *(__m128d*)&ctxI->internalState.finalClipBoxD.x0);
  x1y1 = _mm_min_pd(x1y1, *(__m128d*)&ctxI->internalState.finalClipBoxD.x1);

  __m128i a = _mm_cvtpd_epi32(x0y0);
  __m128i b = _mm_cvtpd_epi32(x1y1);
  __m128d msk = _mm_cmpge_pd(x0y0, x1y1);
  _mm_storeu_si128((__m128i*)dstBoxOut, _mm_unpacklo_epi64(a, b));

  return _mm_movemask_pd(msk) == 0;
}
#else
static BL_INLINE bool translateAndClipRectToFillI(const BLRasterContextImpl* ctxI, const BLRectI* srcRect, BLBoxI* dstBoxOut) noexcept {
  int rx = srcRect->x;
  int ry = srcRect->y;
  int rw = srcRect->w;
  int rh = srcRect->h;

  if (BL_TARGET_ARCH_BITS < 64) {
    OverflowFlag of{};

    int x0 = IntOps::addOverflow(rx, ctxI->translationI().x, &of);
    int y0 = IntOps::addOverflow(ry, ctxI->translationI().y, &of);
    int x1 = IntOps::addOverflow(rw, x0, &of);
    int y1 = IntOps::addOverflow(rh, y0, &of);

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
#endif

static BL_INLINE bool translateAndClipRectToBlitI(const BLRasterContextImpl* ctxI, const BLPointI* origin, const BLRectI* area, const BLSizeI* sz, BLResult* resultOut, BLBoxI* dstBoxOut, BLPointI* srcOffsetOut) noexcept {
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
    OverflowFlag of{};

    int dx = IntOps::addOverflow(origin->x, ctxI->translationI().x, &of);
    int dy = IntOps::addOverflow(origin->y, ctxI->translationI().y, &of);

    int x0 = dx;
    int y0 = dy;
    int x1 = IntOps::addOverflow(x0, size.w, &of);
    int y1 = IntOps::addOverflow(y0, size.h, &of);

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
    int64_t dx = int64_t(origin->x) + ctxI->translationI().x;
    int64_t dy = int64_t(origin->y) + ctxI->translationI().y;

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

// bl::RasterEngine - ContextImpl - Internals - Async - Render Batch
// =================================================================

static BL_INLINE void releaseBatchFetchData(BLRasterContextImpl* ctxI, RenderCommandQueue* queue) noexcept {
  while (queue) {
    RenderCommand* commandData = queue->_data;
    for (size_t i = 0; i < queue->_fetchDataMarks.sizeInWords(); i++) {
      BLBitWord bits = queue->_fetchDataMarks.data[i];
      ParametrizedBitOps<BitOrder::kLSB, BLBitWord>::BitIterator it(bits);

      while (it.hasNext()) {
        size_t bitIndex = it.next();
        RenderCommand& command = commandData[bitIndex];

        if (command.retainsStyleFetchData())
          command._source.fetchData->release(ctxI);

        if (command.retainsMaskImageData())
          ImageInternal::releaseImpl<RCMode::kMaybe>(command._payload.boxMaskA.maskImageI.ptr);
      }
      commandData += IntOps::bitSizeOf<BLBitWord>();
    }
    queue = queue->next();
  }
}

static BL_NOINLINE BLResult flushRenderBatch(BLRasterContextImpl* ctxI) noexcept {
  WorkerManager& mgr = ctxI->workerMgr();
  if (mgr.hasPendingCommands()) {
    mgr.finalizeBatch();

    WorkerSynchronization* synchronization = &mgr._synchronization;
    RenderBatch* batch = mgr.currentBatch();
    uint32_t threadCount = mgr.threadCount();

    for (uint32_t i = 0; i < threadCount; i++) {
      WorkData* workData = mgr._workDataStorage[i];
      workData->initBatch(batch);
      workData->initContextData(ctxI->dstData, ctxI->syncWorkData.ctxData.pixelOrigin);
    }

    // Just to make sure that all the changes are visible to the threads.
    synchronization->beforeStart(threadCount, batch->jobCount() > 0);

    for (uint32_t i = 0; i < threadCount; i++) {
      mgr._workerThreads[i]->run(WorkerProc::workerThreadEntry, mgr._workDataStorage[i]);
    }

    // User thread acts as a worker too.
    {
      synchronization->threadStarted();

      WorkData* workData = &ctxI->syncWorkData;
      SyncWorkState workState;

      workState.save(*workData);
      WorkerProc::processWorkData(workData, batch);
      workState.restore(*workData);
    }

    if (threadCount) {
      synchronization->waitForThreadsToFinish();
      ctxI->syncWorkData._accumulatedErrorFlags |= blAtomicFetchRelaxed(&batch->_accumulatedErrorFlags);
    }

    releaseBatchFetchData(ctxI, batch->_commandList.first());

    mgr._allocator.clear();
    mgr.initFirstBatch();

    ctxI->syncWorkData.startOver();
    ctxI->contextFlags &= ~ContextFlags::kSharedStateAllFlags;
    ctxI->sharedFillState = nullptr;
    ctxI->sharedStrokeState = nullptr;
  }

  return BL_SUCCESS;
}

// bl::RasterEngine - ContextImpl - Internals - Render Call - Data Allocation
// ==========================================================================

static BL_INLINE void markQueueFullOrExhausted(BLRasterContextImpl* ctxI, bool flag) noexcept {
  constexpr uint32_t kMTFullOrExhaustedShift = IntOps::bitShiftOf(ContextFlags::kMTFullOrExhausted);
  ctxI->contextFlags |= ContextFlags(uint32_t(flag) << kMTFullOrExhaustedShift);
}

template<RenderingMode kRM>
struct RenderFetchDataStorage;

template<>
struct RenderFetchDataStorage<kSync> {
  RenderFetchData _fetchData;

  BL_INLINE_NODEBUG RenderFetchDataStorage() noexcept {}
  BL_INLINE_NODEBUG RenderFetchDataStorage(BLRasterContextImpl* ctxI) noexcept { init(ctxI); }

  BL_INLINE_NODEBUG void init(BLRasterContextImpl* ctxI) noexcept { blUnused(ctxI); }

  BL_INLINE_NODEBUG RenderFetchData* ptr() noexcept { return &_fetchData; }
  BL_INLINE_NODEBUG RenderFetchData* operator->() noexcept { return &_fetchData; }
};

template<>
struct RenderFetchDataStorage<kAsync> {
  RenderFetchData* _fetchData;

  BL_INLINE_NODEBUG RenderFetchDataStorage() noexcept {}
  BL_INLINE_NODEBUG RenderFetchDataStorage(BLRasterContextImpl* ctxI) noexcept {
    init(ctxI);
  }

  BL_INLINE_NODEBUG void init(BLRasterContextImpl* ctxI) noexcept {
    _fetchData = ctxI->workerMgr()._fetchDataPool.ptr;
    _fetchData->initHeader(0);
  }

  BL_INLINE_NODEBUG RenderFetchData* ptr() noexcept { return _fetchData; }
  BL_INLINE_NODEBUG RenderFetchData* operator->() noexcept { return _fetchData; }
};

static BL_INLINE void advanceFetchPtr(BLRasterContextImpl* ctxI) noexcept {
  ctxI->workerMgr()._fetchDataPool.advance();
  markQueueFullOrExhausted(ctxI, ctxI->workerMgr()._fetchDataPool.exhausted());
}

// bl::RasterEngine - ContextImpl - Internals - Render Call - Fetch And Dispatch Data
// ==================================================================================

// Slow path - if the pipeline is not in cache there is also a chance that FetchData has not been setup yet.
// In that case it would have PendingFlag set to 1, which would indicate the pending setup.
static BL_NOINLINE BLResult ensureFetchAndDispatchDataSlow(
    BLRasterContextImpl* ctxI,
    Pipeline::Signature signature, RenderFetchDataHeader* fetchData, Pipeline::DispatchData* out) noexcept {

  if (signature.hasPendingFlag()) {
    BL_PROPAGATE(computePendingFetchData(static_cast<RenderFetchData*>(fetchData)));

    signature.clearPendingBit();
    auto m = Pipeline::cacheLookup(ctxI->pipeLookupCache, signature.value);

    if (m.matched()) {
      *out = ctxI->pipeLookupCache.dispatchData(m.index());
      return BL_SUCCESS;
    }
  }

  return ctxI->pipeProvider.get(signature.value, out, &ctxI->pipeLookupCache);
}

// Fast path - if the signature is in the cache, which means that we have the dispatch data and that FetchData
// doesn't need initialization (it's either SOLID or it has been already initialized in case that it was used
// multiple times or this render call is a blit).
static BL_INLINE BLResult ensureFetchAndDispatchData(
    BLRasterContextImpl* ctxI,
    Pipeline::Signature signature, RenderFetchDataHeader* fetchData, Pipeline::DispatchData* out) noexcept {

  // Must be inlined for greater performance.
  auto m = Pipeline::cacheLookup(ctxI->pipeLookupCache, signature.value);

  // Likely if there is not a lot of diverse render commands.
  if (BL_LIKELY(m.matched())) {
    *out = ctxI->pipeLookupCache.dispatchData(m.index());
    return BL_SUCCESS;
  }

  return ensureFetchAndDispatchDataSlow(ctxI, signature, fetchData, out);
}

// bl::RasterEngine - ContextImpl - Internals - Render Call - Queues and Pools
// ===========================================================================

// This function is called when a command/job queue is full or when pool(s) get exhausted.
//
// The purpose of this function is to make sure that ALL queues are not full and that no pools
// are exhausted, because the dispatching relies on the availability of these resources.
static BL_NOINLINE BLResult handleQueuesFullOrPoolsExhausted(BLRasterContextImpl* ctxI) noexcept {
  // Should only be called when we know at that least one queue / buffer needs refill.
  BL_ASSERT(blTestFlag(ctxI->contextFlags, ContextFlags::kMTFullOrExhausted));

  WorkerManager& mgr = ctxI->workerMgr();

  if (mgr.isCommandQueueFull()) {
    mgr.beforeGrowCommandQueue();
    if (mgr.isBatchFull()) {
      BL_PROPAGATE(flushRenderBatch(ctxI));
      // NOTE: After a successful flush the queues and pools should already be allocated.
      ctxI->contextFlags &= ~ContextFlags::kMTFullOrExhausted;
      return BL_SUCCESS;
    }

    BL_PROPAGATE(mgr._growCommandQueue());
  }

  if (mgr.isJobQueueFull()) {
    BL_PROPAGATE(mgr._growJobQueue());
  }

  if (mgr.isFetchDataPoolExhausted()) {
    BL_PROPAGATE(mgr._preallocateFetchDataPool());
  }

  if (mgr.isSharedDataPoolExhausted()) {
    BL_PROPAGATE(mgr._preallocateSharedDataPool());
  }

  ctxI->contextFlags &= ~ContextFlags::kMTFullOrExhausted;
  return BL_SUCCESS;
}

// bl::RasterEngine - ContextImpl - Internals - Render Call - Resolve
// ==================================================================

// These functions are intended to be used by the entry function (frontend). The purpose is to calculate the optimal
// pipeline signature and to perform the necessary initialization of the render command. Sync mode is pretty trivial
// as nothing survives the call, so nothing really needs any accounting. However, async mode is a bit more tricky as
// it's required to allocate the render command, and to make sure we can hold everything it uses.

struct RenderCallResolvedOp {
  //! \name Members
  //! \{

  Pipeline::Signature signature;
  ContextFlags flags;

  //! \}

  //! \name Interface
  //! \{

  BL_INLINE_NODEBUG bool unmodified() const noexcept { return flags == ContextFlags::kNoFlagsSet; }

  //! \}
};

// Resolves a clear operation - clear operation is always solid and always forces SRC_COPY operator on the input.
template<RenderingMode kRM>
static BL_INLINE RenderCallResolvedOp resolveClearOp(BLRasterContextImpl* ctxI, ContextFlags nopFlags) noexcept {
  constexpr ContextFlags kNopExtra = kRM == kSync ? ContextFlags::kNoFlagsSet : ContextFlags::kMTFullOrExhausted;

  CompOpSimplifyInfo simplifyInfo = compOpSimplifyInfo(CompOpExt::kSrcCopy, ctxI->format(), FormatExt::kPRGB32);
  CompOpSolidId solidId = simplifyInfo.solidId();

  ContextFlags combinedFlags = ctxI->contextFlags | ContextFlags(solidId);
  ContextFlags resolvedFlags = combinedFlags & (nopFlags | kNopExtra);

  return RenderCallResolvedOp{simplifyInfo.signature(), resolvedFlags};
}

// Resolves a fill operation that uses the default fill style (or stroke style if this fill implements a stroke operation).
template<RenderingMode kRM>
static BL_INLINE RenderCallResolvedOp resolveImplicitStyleOp(BLRasterContextImpl* ctxI, ContextFlags nopFlags, const RenderFetchDataHeader* fetchData, bool bail) noexcept {
  constexpr ContextFlags kNopExtra = kRM == kSync ? ContextFlags::kNoFlagsSet : ContextFlags::kMTFullOrExhausted;

  CompOpSimplifyInfo simplifyInfo = ctxI->compOpSimplifyInfo[fetchData->extra.format];
  CompOpSolidId solidId = simplifyInfo.solidId();

  ContextFlags bailFlag = ContextFlags(uint32_t(bail) << IntOps::bitShiftOf(uint32_t(ContextFlags::kNoOperation)));
  ContextFlags resolvedFlags = (ctxI->contextFlags | ContextFlags(solidId) | bailFlag) & (nopFlags | kNopExtra);

  return RenderCallResolvedOp{simplifyInfo.signature(), resolvedFlags};
}

// Resolves a solid operation, which uses a custom Rgba32 color passed by the user to the frontend.
template<RenderingMode kRM>
static BL_INLINE RenderCallResolvedOp resolveExplicitSolidOp(BLRasterContextImpl* ctxI, ContextFlags nopFlags, uint32_t rgba32, RenderFetchDataSolid& solid, bool bail) noexcept {
  constexpr ContextFlags kNopExtra = kRM == kSync ? ContextFlags::kNoFlagsSet : ContextFlags::kMTFullOrExhausted;

  FormatExt fmt = formatFromRgba32(rgba32);
  CompOpSimplifyInfo simplifyInfo = ctxI->compOpSimplifyInfo[size_t(fmt)];
  CompOpSolidId solidId = simplifyInfo.solidId();

  ContextFlags bailFlag = ContextFlags(uint32_t(bail) << IntOps::bitShiftOf(uint32_t(ContextFlags::kNoOperation)));
  ContextFlags resolvedFlags = (ctxI->contextFlags | ContextFlags(solidId) | bailFlag) & (nopFlags | kNopExtra);

  solid.signature.reset();
  solid.pipelineData.prgb32 = PixelOps::Scalar::cvt_prgb32_8888_from_argb32_8888(rgba32);
  solid.pipelineData.reserved32 = 0;

  return RenderCallResolvedOp{simplifyInfo.signature(), resolvedFlags};
}

template<RenderingMode kRM>
static BL_NOINLINE BLResultT<RenderCallResolvedOp> resolveExplicitStyleOp(BLRasterContextImpl* ctxI, ContextFlags nopFlags, const BLObjectCore* style, RenderFetchDataStorage<kRM>& fetchDataStorage, bool bail) noexcept {
  constexpr RenderCallResolvedOp kNop = {{0u}, ContextFlags::kNoOperation};
  constexpr BLContextStyleTransformMode kTransformMode = BL_CONTEXT_STYLE_TRANSFORM_MODE_USER;

  if BL_CONSTEXPR (kRM == kAsync) {
    if (blTestFlag(ctxI->contextFlags, ContextFlags::kMTFullOrExhausted)) {
      BLResult result = handleQueuesFullOrPoolsExhausted(ctxI);
      if (BL_UNLIKELY(result != BL_SUCCESS))
        return BLResultT<RenderCallResolvedOp>{result, kNop};
    }
  }

  fetchDataStorage.init(ctxI);
  RenderFetchData* fetchData = fetchDataStorage.ptr();

  FormatExt format = FormatExt::kNone;
  BLObjectType styleType = style->_d.getType();
  fetchData->initHeader(0);

  if (styleType <= BL_OBJECT_TYPE_NULL) {
    BLRgba32 rgba32;

    if (styleType == BL_OBJECT_TYPE_RGBA32)
      rgba32.reset(style->_d.rgba32);
    else if (styleType == BL_OBJECT_TYPE_RGBA64)
      rgba32.reset(style->_d.rgba64);
    else if (styleType == BL_OBJECT_TYPE_RGBA)
      rgba32 = style->_d.rgba.toRgba32();
    else
      return BLResultT<RenderCallResolvedOp>{BL_SUCCESS, kNop};

    format = formatFromRgba32(rgba32.value);
    fetchData->pipelineData.solid.prgb32 = PixelOps::Scalar::cvt_prgb32_8888_from_argb32_8888(rgba32.value);
  }
  else {
    if (BL_UNLIKELY(styleType > BL_OBJECT_TYPE_MAX_STYLE))
      return BLResultT<RenderCallResolvedOp>{BL_ERROR_INVALID_VALUE, kNop};

    NonSolidFetchExplicitApplier applier;
    if (!initNonSolidFetchData(ctxI, fetchData, style, styleType, kTransformMode, applier))
      return BLResultT<RenderCallResolvedOp>{BL_SUCCESS, kNop};
    format = FormatExt(fetchData->extra.format);
  }

  CompOpSimplifyInfo simplifyInfo = ctxI->compOpSimplifyInfo[size_t(format)];
  CompOpSolidId solidId = simplifyInfo.solidId();
  ContextFlags bailFlag = ContextFlags(uint32_t(bail) << IntOps::bitShiftOf(uint32_t(ContextFlags::kNoOperation)));

  ContextFlags resolvedFlags = (ctxI->contextFlags | ContextFlags(solidId) | bailFlag) & nopFlags;
  return BLResultT<RenderCallResolvedOp>{BL_SUCCESS, {simplifyInfo.signature(), resolvedFlags}};
}

// Resolves a blit operation.
template<RenderingMode kRM>
static BL_INLINE RenderCallResolvedOp resolveBlitOp(BLRasterContextImpl* ctxI, ContextFlags nopFlags, uint32_t format, bool bail) noexcept {
  constexpr ContextFlags kNopExtra = kRM == kSync ? ContextFlags::kNoFlagsSet : ContextFlags::kMTFullOrExhausted;

  CompOpSimplifyInfo simplifyInfo = ctxI->compOpSimplifyInfo[format];
  CompOpSolidId solidId = simplifyInfo.solidId();

  ContextFlags bailFlag = ContextFlags(uint32_t(bail) << IntOps::bitShiftOf(uint32_t(ContextFlags::kNoOperation)));
  ContextFlags resolvedFlags = (ctxI->contextFlags | ContextFlags(solidId) | bailFlag) & (nopFlags | kNopExtra);

  return RenderCallResolvedOp{simplifyInfo.signature(), resolvedFlags};
}

// Prepare means to prepare an already resolved and initialized render call. We don't have to worry about memory
// allocations here, just to setup the render call object in the way it can be consumed by all the layers below.

static BL_INLINE void prepareOverriddenFetch(BLRasterContextImpl* ctxI, DispatchInfo& di, DispatchStyle& ds, CompOpSolidId solidId) noexcept {
  blUnused(di);
  ds.fetchData = ctxI->solidFetchDataOverrideTable[size_t(solidId)];
}

static BL_INLINE void prepareNonSolidFetch(BLRasterContextImpl* ctxI, DispatchInfo& di, DispatchStyle& ds, RenderFetchDataHeader* fetchData) noexcept {
  blUnused(ctxI);
  di.addSignature(fetchData->signature);
  ds.fetchData = fetchData;
}

// Used by other macros to share code - in general this is the main resolving mechanism that
// can be used for anything except explicit style API, which requires a bit different logic.
#define BL_CONTEXT_RESOLVE_GENERIC_OP(...)                                       \
  RenderCallResolvedOp resolved = __VA_ARGS__;                                   \
                                                                                 \
  if BL_CONSTEXPR (kRM == kAsync) {                                              \
    /* ASYNC MODE: more flags are used, so make sure our queue is not full */    \
    /* and our pools are not exhausted before rejecting the render call.   */    \
    if (BL_UNLIKELY(resolved.flags >= ContextFlags::kNoOperation)) {             \
      if (!blTestFlag(resolved.flags, ContextFlags::kMTFullOrExhausted))         \
        return bailResult;                                                       \
                                                                                 \
      BL_PROPAGATE(handleQueuesFullOrPoolsExhausted(ctxI));                      \
      resolved.flags &= ~ContextFlags::kMTFullOrExhausted;                       \
                                                                                 \
      /* The same as in SYNC mode - bail if the resolved operation is NOP. */    \
      if (resolved.flags >= ContextFlags::kNoOperation)                          \
        return bailResult;                                                       \
    }                                                                            \
  }                                                                              \
  else {                                                                         \
    /* SYNC MODE: Just bail if the resolved operation is NOP. */                 \
    if (resolved.flags >= ContextFlags::kNoOperation)                            \
      return bailResult;                                                         \
  }

// Resolves a clear operation (always solid).
#define BL_CONTEXT_RESOLVE_CLEAR_OP(NopFlags)                                    \
  BL_CONTEXT_RESOLVE_GENERIC_OP(                                                 \
    resolveClearOp<kRM>(ctxI, NopFlags))                                         \
                                                                                 \
  DispatchInfo di;                                                               \
  di.init(resolved.signature, ctxI->renderTargetInfo.fullAlphaI);                \
  DispatchStyle ds{&ctxI->solidOverrideFillTable[size_t(resolved.flags)]}

// Resolves an operation that uses implicit style (fill or stroke).
#define BL_CONTEXT_RESOLVE_IMPLICIT_STYLE_OP(NopFlags, Slot, Bail)               \
  RenderFetchDataHeader* fetchData = ctxI->internalState.style[Slot].fetchData;  \
                                                                                 \
  BL_CONTEXT_RESOLVE_GENERIC_OP(                                                 \
    resolveImplicitStyleOp<kRM>(ctxI, NopFlags, fetchData, Bail))                \
                                                                                 \
  RenderFetchDataHeader* overriddenFetchData =                                   \
    ctxI->solidFetchDataOverrideTable[size_t(resolved.flags)];                   \
                                                                                 \
  if (resolved.flags != ContextFlags::kNoFlagsSet)                               \
    fetchData = overriddenFetchData;                                             \
                                                                                 \
  DispatchInfo di;                                                               \
  di.init(resolved.signature, ctxI->internalState.styleAlphaI[Slot]);            \
  di.addSignature(fetchData->signature);                                         \
  DispatchStyle ds{fetchData}

// Resolves an operation that uses explicit color (fill or stroke).
#define BL_CONTEXT_RESOLVE_EXPLICIT_SOLID_OP(NopFlags, Slot, Color, Bail)        \
  RenderFetchDataSolid solid;                                                    \
                                                                                 \
  BL_CONTEXT_RESOLVE_GENERIC_OP(                                                 \
    resolveExplicitSolidOp<kRM>(ctxI, NopFlags, Color, solid, Bail))             \
                                                                                 \
  DispatchInfo di;                                                               \
  di.init(resolved.signature, ctxI->internalState.styleAlphaI[Slot]);            \
  DispatchStyle ds{&solid}

// Resolves an operation that uses explicit style (fill or stroke).
#define BL_CONTEXT_RESOLVE_EXPLICIT_STYLE_OP(NopFlags, Slot, Style, Bail)        \
  RenderFetchDataStorage<kRM> fetchData;                                         \
                                                                                 \
  BLResultT<RenderCallResolvedOp> resolved =                                     \
    resolveExplicitStyleOp(ctxI, NopFlags, Style, fetchData, Bail);              \
                                                                                 \
  if (BL_UNLIKELY(resolved.value.flags >= ContextFlags::kNoOperation))           \
    return resolved.code ? resolved.code : bailResult;                           \
                                                                                 \
  DispatchInfo di;                                                               \
  di.init(resolved.value.signature, ctxI->internalState.styleAlphaI[Slot]);      \
                                                                                 \
  RenderFetchDataHeader* overriddenFetchData =                                   \
    ctxI->solidFetchDataOverrideTable[size_t(resolved.value.flags)];             \
                                                                                 \
  DispatchStyle ds{fetchData.ptr()};                                             \
  if (resolved.value.flags != ContextFlags::kNoFlagsSet)                         \
    ds.fetchData = overriddenFetchData;                                          \
                                                                                 \
  di.addSignature(ds.fetchData->signature)

// Resolves a blit operation that uses explicitly passed image.
#define BL_CONTEXT_RESOLVE_BLIT_OP(NopFlags, Format, Bail)                       \
  BL_CONTEXT_RESOLVE_GENERIC_OP(                                                 \
    resolveBlitOp<kRM>(ctxI, NopFlags, Format, Bail))                            \
                                                                                 \
  RenderFetchDataStorage<kRM> fetchData(ctxI);                                   \
                                                                                 \
  DispatchInfo di;                                                               \
  DispatchStyle ds;                                                              \
  di.init(resolved.signature, ctxI->globalAlphaI())

// bl::RasterEngine - ContextImpl - Internals - Render Call - Finalize
// ===================================================================

template<RenderingMode kRM>
BL_INLINE_NODEBUG BLResult finalizeExplicitOp(BLRasterContextImpl* ctxI, RenderFetchData* fetchData, BLResult result) noexcept;

template<>
BL_INLINE_NODEBUG BLResult finalizeExplicitOp<kSync>(BLRasterContextImpl* ctxI, RenderFetchData* fetchData, BLResult result) noexcept {
  blUnused(ctxI, fetchData);
  return result;
}

template<>
BL_INLINE_NODEBUG BLResult finalizeExplicitOp<kAsync>(BLRasterContextImpl* ctxI, RenderFetchData* fetchData, BLResult result) noexcept {
  // The reference count of FetchData is always incremented when a command using it is enqueued. Initially it's zero, so check for one.
  if (fetchData->refCount == 1u) {
    ObjectInternal::retainInstance(&fetchData->style);
    advanceFetchPtr(ctxI);
  }
  return result;
}

// bl::RasterEngine - ContextImpl - Frontend - Flush
// =================================================

static BLResult BL_CDECL flushImpl(BLContextImpl* baseImpl, BLContextFlushFlags flags) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  // Nothing to flush if the rendering context is synchronous.
  if (ctxI->isSync())
    return BL_SUCCESS;

  if (flags & BL_CONTEXT_FLUSH_SYNC) {
    BL_PROPAGATE(flushRenderBatch(ctxI));
  }

  return BL_SUCCESS;
}

// bl::RasterEngine - ContextImpl - Frontend - Properties
// ======================================================

static BLResult BL_CDECL getPropertyImpl(const BLObjectImpl* impl, const char* name, size_t nameSize, BLVarCore* valueOut) noexcept {
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

static BLResult BL_CDECL setPropertyImpl(BLObjectImpl* impl, const char* name, size_t nameSize, const BLVarCore* value) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(impl);
  return blObjectImplSetProperty(ctxI, name, nameSize, value);
}

// bl::RasterEngine - ContextImpl - Save & Restore
// ===============================================

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
static BL_INLINE void saveCoreState(BLRasterContextImpl* ctxI, SavedState* state) noexcept {
  state->hints = ctxI->hints();
  state->compOp = ctxI->compOp();
  state->fillRule = uint8_t(ctxI->fillRule());
  state->styleType[0] = ctxI->internalState.styleType[0];
  state->styleType[1] = ctxI->internalState.styleType[1];

  state->clipMode = ctxI->clipMode();
  state->prevContextFlags = ctxI->contextFlags & ~(ContextFlags::kPreservedFlags);

  state->transformTypesPacked = ctxI->internalState.transformTypesPacked;
  state->globalAlphaI = ctxI->globalAlphaI();
  state->styleAlphaI[0] = ctxI->internalState.styleAlphaI[0];
  state->styleAlphaI[1] = ctxI->internalState.styleAlphaI[1];

  state->globalAlpha = ctxI->globalAlphaD();
  state->styleAlpha[0] = ctxI->internalState.styleAlpha[0];
  state->styleAlpha[1] = ctxI->internalState.styleAlpha[1];

  state->translationI = ctxI->translationI();
}

static BL_INLINE void restoreCoreState(BLRasterContextImpl* ctxI, SavedState* state) noexcept {
  ctxI->internalState.hints = state->hints;
  ctxI->internalState.compOp = state->compOp;
  ctxI->internalState.fillRule = state->fillRule;
  ctxI->internalState.styleType[0] = state->styleType[0];
  ctxI->internalState.styleType[1] = state->styleType[1];
  ctxI->syncWorkData.clipMode = state->clipMode;
  ctxI->contextFlags = state->prevContextFlags;

  ctxI->internalState.transformTypesPacked = state->transformTypesPacked;
  ctxI->internalState.globalAlphaI = state->globalAlphaI;
  ctxI->internalState.styleAlphaI[0] = state->styleAlphaI[0];
  ctxI->internalState.styleAlphaI[1] = state->styleAlphaI[1];

  ctxI->internalState.globalAlpha = state->globalAlpha;
  ctxI->internalState.styleAlpha[0] = state->styleAlpha[0];
  ctxI->internalState.styleAlpha[1] = state->styleAlpha[1];

  ctxI->internalState.translationI = state->translationI;

  onAfterCompOpChanged(ctxI);
}

static void discardStates(BLRasterContextImpl* ctxI, SavedState* topState) noexcept {
  SavedState* savedState = ctxI->savedState;
  if (savedState == topState)
    return;

  // NOTE: No need to handle parts of states that don't use dynamically allocated memory.
  ContextFlags contextFlags = ctxI->contextFlags;
  do {
    if ((contextFlags & (ContextFlags::kFetchDataFill | ContextFlags::kWeakStateFillStyle)) == ContextFlags::kFetchDataFill) {
      constexpr uint32_t kSlot = BL_CONTEXT_STYLE_SLOT_FILL;
      if (savedState->style[kSlot].hasFetchData()) {
        RenderFetchData* fetchData = savedState->style[kSlot].getRenderFetchData();
        fetchData->release(ctxI);
      }
    }

    if ((contextFlags & (ContextFlags::kFetchDataStroke | ContextFlags::kWeakStateStrokeStyle)) == ContextFlags::kFetchDataStroke) {
      constexpr uint32_t kSlot = BL_CONTEXT_STYLE_SLOT_STROKE;
      if (savedState->style[kSlot].hasFetchData()) {
        RenderFetchData* fetchData = savedState->style[kSlot].getRenderFetchData();
        fetchData->release(ctxI);
      }
    }

    if (!blTestFlag(contextFlags, ContextFlags::kWeakStateStrokeOptions)) {
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

static BLResult BL_CDECL saveImpl(BLContextImpl* baseImpl, BLContextCookie* cookie) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(ctxI->internalState.savedStateCount >= ctxI->savedStateLimit))
    return blTraceError(BL_ERROR_TOO_MANY_SAVED_STATES);

  SavedState* newState = ctxI->allocSavedState();
  if (BL_UNLIKELY(!newState))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  newState->prevState = ctxI->savedState;
  newState->stateId = Traits::maxValue<uint64_t>();

  ctxI->savedState = newState;
  ctxI->internalState.savedStateCount++;

  saveCoreState(ctxI, newState);
  ctxI->contextFlags |= ContextFlags::kWeakStateAllFlags;

  if (!cookie)
    return BL_SUCCESS;

  // Setup the given `cookie` and make the state cookie dependent.
  uint64_t stateId = ++ctxI->stateIdCounter;
  newState->stateId = stateId;

  cookie->reset(ctxI->contextOriginId, stateId);
  return BL_SUCCESS;
}

static BLResult BL_CDECL restoreImpl(BLContextImpl* baseImpl, const BLContextCookie* cookie) noexcept {
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
    if (savedState->stateId != Traits::maxValue<uint64_t>())
      return blTraceError(BL_ERROR_NO_MATCHING_COOKIE);
  }

  ContextFlags kPreservedFlags = ContextFlags::kPreservedFlags | ContextFlags::kSharedStateAllFlags;
  ContextFlags contextFlagsToKeep = ctxI->contextFlags & kPreservedFlags;
  ctxI->internalState.savedStateCount -= n;

  for (;;) {
    ContextFlags currentFlags = ctxI->contextFlags;
    restoreCoreState(ctxI, savedState);

    if (!blTestFlag(currentFlags, ContextFlags::kWeakStateConfig)) {
      ctxI->internalState.approximationOptions = savedState->approximationOptions;
      onAfterFlattenToleranceChanged(ctxI);
      onAfterOffsetParameterChanged(ctxI);

      contextFlagsToKeep &= ~ContextFlags::kSharedStateFill;
    }

    if (!blTestFlag(currentFlags, ContextFlags::kWeakStateClip)) {
      restoreClippingFromState(ctxI, savedState);
      contextFlagsToKeep &= ~ContextFlags::kSharedStateFill;
    }

    if (!blTestFlag(currentFlags, ContextFlags::kWeakStateFillStyle)) {
      StyleData* dst = &ctxI->internalState.style[BL_CONTEXT_STYLE_SLOT_FILL];
      StyleData* src = &savedState->style[BL_CONTEXT_STYLE_SLOT_FILL];

      if (blTestFlag(currentFlags, ContextFlags::kFetchDataFill))
        destroyValidStyle(ctxI, dst);

      dst->copyFrom(*src);
    }

    if (!blTestFlag(currentFlags, ContextFlags::kWeakStateStrokeStyle)) {
      StyleData* dst = &ctxI->internalState.style[BL_CONTEXT_STYLE_SLOT_STROKE];
      StyleData* src = &savedState->style[BL_CONTEXT_STYLE_SLOT_STROKE];

      if (blTestFlag(currentFlags, ContextFlags::kFetchDataStroke))
        destroyValidStyle(ctxI, dst);

      dst->copyFrom(*src);
    }

    if (!blTestFlag(currentFlags, ContextFlags::kWeakStateStrokeOptions)) {
      // NOTE: This code is unsafe, but since we know that `BLStrokeOptions` is movable it's just fine. We
      // destroy `BLStrokeOptions` first and then move it into that destroyed instance from the saved state.
      ArrayInternal::releaseInstance(&ctxI->internalState.strokeOptions.dashArray);
      ctxI->internalState.strokeOptions._copyFrom(savedState->strokeOptions);
      contextFlagsToKeep &= ~(ContextFlags::kSharedStateStrokeBase | ContextFlags::kSharedStateStrokeExt);
    }

    // UserTransform state is unset when MetaTransform and/or UserTransform were saved.
    if (!blTestFlag(currentFlags, ContextFlags::kWeakStateUserTransform)) {
      ctxI->internalState.userTransform = savedState->userTransform;

      if (!blTestFlag(currentFlags, ContextFlags::kWeakStateMetaTransform)) {
        ctxI->internalState.metaTransform = savedState->altTransform;
        updateFinalTransform(ctxI);
        updateMetaTransformFixed(ctxI);
        updateFinalTransformFixed(ctxI);
      }
      else {
        ctxI->internalState.finalTransform = savedState->altTransform;
        updateFinalTransformFixed(ctxI);
      }

      contextFlagsToKeep &= ~(ContextFlags::kSharedStateFill | ContextFlags::kSharedStateStrokeBase | ContextFlags::kSharedStateStrokeExt);
    }

    SavedState* finishedSavedState = savedState;
    savedState = savedState->prevState;

    ctxI->savedState = savedState;
    ctxI->freeSavedState(finishedSavedState);

    if (--n == 0)
      break;
  }

  ctxI->contextFlags = (ctxI->contextFlags & ~kPreservedFlags) | contextFlagsToKeep;
  return BL_SUCCESS;
}

// bl::RasterEngine - ContextImpl - Frontend - Transformations
// ===========================================================

static BLResult BL_CDECL applyTransformOpImpl(BLContextImpl* baseImpl, BLTransformOp opType, const void* opData) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  Matrix2x2 before2x2;
  onBeforeUserTransformChange(ctxI, before2x2);
  BL_PROPAGATE(blMatrix2DApplyOp(&ctxI->internalState.userTransform, opType, opData));

  onAfterUserTransformChanged(ctxI, before2x2);
  return BL_SUCCESS;
}

static BLResult BL_CDECL userToMetaImpl(BLContextImpl* baseImpl) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  constexpr ContextFlags kUserAndMetaFlags = ContextFlags::kWeakStateMetaTransform | ContextFlags::kWeakStateUserTransform;

  if (blTestFlag(ctxI->contextFlags, kUserAndMetaFlags)) {
    SavedState* state = ctxI->savedState;

    // Always save both `metaTransform` and `userTransform` in case we have to save the current state before we
    // change the transform. In this case the `altTransform` of the state would store the current `metaTransform`
    // and on state restore the final transform would be recalculated in-place.
    state->altTransform = ctxI->metaTransform();

    // Don't copy it if it was already saved, we would have copied an altered `userTransform`.
    if (blTestFlag(ctxI->contextFlags, ContextFlags::kWeakStateUserTransform))
      state->userTransform = ctxI->userTransform();
  }

  ctxI->contextFlags &= ~(kUserAndMetaFlags | ContextFlags::kSharedStateStrokeExt);
  ctxI->internalState.userTransform.reset();
  ctxI->internalState.metaTransform = ctxI->finalTransform();
  ctxI->internalState.metaTransformFixed = ctxI->finalTransformFixed();
  ctxI->internalState.metaTransformType = uint8_t(ctxI->finalTransformType());
  ctxI->internalState.metaTransformFixedType = uint8_t(ctxI->finalTransformFixedType());

  return BL_SUCCESS;
}

// bl::RasterEngine - ContextImpl - Frontend - Rendering Hints
// ===========================================================

static BLResult BL_CDECL setHintImpl(BLContextImpl* baseImpl, BLContextHint hintType, uint32_t value) noexcept {
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

static BLResult BL_CDECL setHintsImpl(BLContextImpl* baseImpl, const BLContextHints* hints) noexcept {
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

// bl::RasterEngine - ContextImpl - Frontend - Approximation Options
// =================================================================

static BLResult BL_CDECL setFlattenModeImpl(BLContextImpl* baseImpl, BLFlattenMode mode) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(uint32_t(mode) > BL_FLATTEN_MODE_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeConfigChange(ctxI);
  ctxI->contextFlags &= ~ContextFlags::kWeakStateConfig;

  ctxI->internalState.approximationOptions.flattenMode = uint8_t(mode);
  return BL_SUCCESS;
}

static BLResult BL_CDECL setFlattenToleranceImpl(BLContextImpl* baseImpl, double tolerance) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(Math::isNaN(tolerance)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeConfigChange(ctxI);
  ctxI->contextFlags &= ~(ContextFlags::kWeakStateConfig | ContextFlags::kSharedStateFill);

  tolerance = blClamp(tolerance, ContextInternal::kMinimumTolerance, ContextInternal::kMaximumTolerance);
  BL_ASSERT(Math::isFinite(tolerance));

  ctxI->internalState.approximationOptions.flattenTolerance = tolerance;
  onAfterFlattenToleranceChanged(ctxI);

  return BL_SUCCESS;
}

static BLResult BL_CDECL setApproximationOptionsImpl(BLContextImpl* baseImpl, const BLApproximationOptions* options) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  uint32_t flattenMode = options->flattenMode;
  uint32_t offsetMode = options->offsetMode;

  double flattenTolerance = options->flattenTolerance;
  double offsetParameter = options->offsetParameter;

  if (BL_UNLIKELY(flattenMode > BL_FLATTEN_MODE_MAX_VALUE ||
                  offsetMode > BL_OFFSET_MODE_MAX_VALUE ||
                  Math::isNaN(flattenTolerance) ||
                  Math::isNaN(offsetParameter)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeConfigChange(ctxI);
  ctxI->contextFlags &= ~(ContextFlags::kWeakStateConfig | ContextFlags::kSharedStateFill);

  BLApproximationOptions& dst = ctxI->internalState.approximationOptions;
  dst.flattenMode = uint8_t(flattenMode);
  dst.offsetMode = uint8_t(offsetMode);
  dst.flattenTolerance = blClamp(flattenTolerance, ContextInternal::kMinimumTolerance, ContextInternal::kMaximumTolerance);
  dst.offsetParameter = offsetParameter;

  onAfterFlattenToleranceChanged(ctxI);
  onAfterOffsetParameterChanged(ctxI);

  return BL_SUCCESS;
}

// bl::RasterEngine - ContextImpl - Frontend - Style Alpha
// =======================================================

static BLResult BL_CDECL setStyleAlphaImpl(BLContextImpl* baseImpl, BLContextStyleSlot slot, double alpha) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(slot > BL_CONTEXT_STYLE_SLOT_MAX_VALUE || Math::isNaN(alpha)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  ContextFlags noAlpha = ContextFlags::kNoBaseAlpha << slot;
  ContextFlags contextFlags = ctxI->contextFlags & ~noAlpha;

  alpha = blClamp(alpha, 0.0, 1.0);
  uint32_t alphaI = uint32_t(Math::roundToInt(ctxI->globalAlphaD() * ctxI->fullAlphaD() * alpha));

  if (alphaI)
    noAlpha = ContextFlags::kNoFlagsSet;

  ctxI->internalState.styleAlpha[slot] = alpha;
  ctxI->internalState.styleAlphaI[slot] = alphaI;
  ctxI->contextFlags = contextFlags | noAlpha;
  return BL_SUCCESS;
}

// bl::RasterEngine - ContextImpl - Frontend - Swap Styles
// =======================================================

static BLResult BL_CDECL swapStylesImpl(BLContextImpl* baseImpl, BLContextStyleSwapMode mode) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  ContextFlags contextFlags = ctxI->contextFlags;

  if (BL_UNLIKELY(mode > BL_CONTEXT_STYLE_SWAP_MODE_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  RasterContextState& state = ctxI->internalState;

  constexpr BLContextStyleSlot kFillSlot = BL_CONTEXT_STYLE_SLOT_FILL;
  constexpr BLContextStyleSlot kStrokeSlot = BL_CONTEXT_STYLE_SLOT_STROKE;
  constexpr ContextFlags kWeakFillAndStrokeStyle = ContextFlags::kWeakStateFillStyle | ContextFlags::kWeakStateStrokeStyle;

  if (blTestFlag(contextFlags, kWeakFillAndStrokeStyle)) {
    BL_ASSERT(ctxI->savedState != nullptr);

    if (blTestFlag(contextFlags, ContextFlags::kWeakStateFillStyle)) {
      ctxI->savedState->style[kFillSlot].copyFrom(state.style[kFillSlot]);
      if (blTestFlag(contextFlags, ContextFlags::kFetchDataFill))
        state.style[kFillSlot].getRenderFetchData()->refCount++;
    }

    if (blTestFlag(contextFlags, ContextFlags::kWeakStateStrokeStyle)) {
      ctxI->savedState->style[kStrokeSlot].copyFrom(state.style[kStrokeSlot]);
      if (blTestFlag(contextFlags, ContextFlags::kFetchDataFill))
        state.style[kStrokeSlot].getRenderFetchData()->refCount++;
    }

    contextFlags &= ~kWeakFillAndStrokeStyle;
  }

  // Swap fill and stroke styles.
  {
    state.style[kFillSlot].swap(state.style[kStrokeSlot]);
    BLInternal::swap(state.styleType[kFillSlot], state.styleType[kStrokeSlot]);

    constexpr ContextFlags kSwapFlags = ContextFlags::kNoFillAndStrokeStyle | ContextFlags::kFetchDataFillAndStroke;
    contextFlags = (contextFlags & ~kSwapFlags) | ((contextFlags >> 1) & kSwapFlags) | ((contextFlags << 1) & kSwapFlags);
  }

  // Swap fill and stroke alphas.
  if (mode == BL_CONTEXT_STYLE_SWAP_MODE_STYLES_WITH_ALPHA) {
    BLInternal::swap(state.styleAlpha[kFillSlot], state.styleAlpha[kStrokeSlot]);
    BLInternal::swap(state.styleAlphaI[kFillSlot], state.styleAlphaI[kStrokeSlot]);

    constexpr ContextFlags kSwapFlags = ContextFlags::kNoFillAndStrokeAlpha;
    contextFlags = (contextFlags & ~kSwapFlags) | ((contextFlags >> 1) & kSwapFlags) | ((contextFlags << 1) & kSwapFlags);
  }

  ctxI->contextFlags = contextFlags;
  return BL_SUCCESS;
}

// bl::RasterEngine - ContextImpl - Frontend - Composition Options
// ===============================================================

static BLResult BL_CDECL setGlobalAlphaImpl(BLContextImpl* baseImpl, double alpha) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(Math::isNaN(alpha)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  alpha = blClamp(alpha, 0.0, 1.0);

  double intAlphaD = alpha * ctxI->fullAlphaD();
  double fillAlphaD = intAlphaD * ctxI->internalState.styleAlpha[BL_CONTEXT_STYLE_SLOT_FILL];
  double strokeAlphaD = intAlphaD * ctxI->internalState.styleAlpha[BL_CONTEXT_STYLE_SLOT_STROKE];

  uint32_t globalAlphaI = uint32_t(Math::roundToInt(intAlphaD));
  uint32_t styleAlphaI[2] = { uint32_t(Math::roundToInt(fillAlphaD)), uint32_t(Math::roundToInt(strokeAlphaD)) };

  ctxI->internalState.globalAlpha = alpha;
  ctxI->internalState.globalAlphaI = globalAlphaI;
  ctxI->internalState.styleAlphaI[0] = styleAlphaI[0];
  ctxI->internalState.styleAlphaI[1] = styleAlphaI[1];

  ContextFlags contextFlags = ctxI->contextFlags;
  contextFlags &= ~(ContextFlags::kNoGlobalAlpha | ContextFlags::kNoFillAlpha | ContextFlags::kNoStrokeAlpha);

  if (!globalAlphaI) contextFlags |= ContextFlags::kNoGlobalAlpha;
  if (!styleAlphaI[0]) contextFlags |= ContextFlags::kNoFillAlpha;
  if (!styleAlphaI[1]) contextFlags |= ContextFlags::kNoStrokeAlpha;

  ctxI->contextFlags = contextFlags;
  return BL_SUCCESS;
}

static BLResult BL_CDECL setCompOpImpl(BLContextImpl* baseImpl, BLCompOp compOp) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(uint32_t(compOp) > BL_COMP_OP_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  ctxI->internalState.compOp = uint8_t(compOp);
  onAfterCompOpChanged(ctxI);

  return BL_SUCCESS;
}

// bl::RasterEngine - ContextImpl - Frontend - Fill Options
// ========================================================

static BLResult BL_CDECL setFillRuleImpl(BLContextImpl* baseImpl, BLFillRule fillRule) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(uint32_t(fillRule) > BL_FILL_RULE_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  ctxI->internalState.fillRule = uint8_t(fillRule);
  return BL_SUCCESS;
}

// bl::RasterEngine - ContextImpl - Frontend - Stroke Options
// ==========================================================

static BLResult BL_CDECL setStrokeWidthImpl(BLContextImpl* baseImpl, double width) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(ContextFlags::kNoStrokeOptions | ContextFlags::kWeakStateStrokeOptions | ContextFlags::kSharedStateStrokeBase);
  ctxI->internalState.strokeOptions.width = width;
  return BL_SUCCESS;
}

static BLResult BL_CDECL setStrokeMiterLimitImpl(BLContextImpl* baseImpl, double miterLimit) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(ContextFlags::kNoStrokeOptions | ContextFlags::kWeakStateStrokeOptions | ContextFlags::kSharedStateStrokeBase);
  ctxI->internalState.strokeOptions.miterLimit = miterLimit;
  return BL_SUCCESS;
}

static BLResult BL_CDECL setStrokeCapImpl(BLContextImpl* baseImpl, BLStrokeCapPosition position, BLStrokeCap strokeCap) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(uint32_t(position) > BL_STROKE_CAP_POSITION_MAX_VALUE ||
                  uint32_t(strokeCap) > BL_STROKE_CAP_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~ContextFlags::kSharedStateStrokeBase;

  ctxI->internalState.strokeOptions.caps[position] = uint8_t(strokeCap);
  return BL_SUCCESS;
}

static BLResult BL_CDECL setStrokeCapsImpl(BLContextImpl* baseImpl, BLStrokeCap strokeCap) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(uint32_t(strokeCap) > BL_STROKE_CAP_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~ContextFlags::kSharedStateStrokeBase;

  for (uint32_t i = 0; i <= BL_STROKE_CAP_POSITION_MAX_VALUE; i++)
    ctxI->internalState.strokeOptions.caps[i] = uint8_t(strokeCap);
  return BL_SUCCESS;
}

static BLResult BL_CDECL setStrokeJoinImpl(BLContextImpl* baseImpl, BLStrokeJoin strokeJoin) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(uint32_t(strokeJoin) > BL_STROKE_JOIN_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~ContextFlags::kSharedStateStrokeBase;

  ctxI->internalState.strokeOptions.join = uint8_t(strokeJoin);
  return BL_SUCCESS;
}

static BLResult BL_CDECL setStrokeDashOffsetImpl(BLContextImpl* baseImpl, double dashOffset) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(ContextFlags::kNoStrokeOptions | ContextFlags::kWeakStateStrokeOptions | ContextFlags::kSharedStateStrokeBase);

  ctxI->internalState.strokeOptions.dashOffset = dashOffset;
  return BL_SUCCESS;
}

static BLResult BL_CDECL setStrokeDashArrayImpl(BLContextImpl* baseImpl, const BLArrayCore* dashArray) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(dashArray->_d.rawType() != BL_OBJECT_TYPE_ARRAY_FLOAT64))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeStrokeChangeAndDestroyDashArray(ctxI);
  ctxI->contextFlags &= ~(ContextFlags::kNoStrokeOptions | ContextFlags::kWeakStateStrokeOptions | ContextFlags::kSharedStateStrokeBase);

  ctxI->internalState.strokeOptions.dashArray._d = dashArray->_d;
  return ArrayInternal::retainInstance(&ctxI->internalState.strokeOptions.dashArray);
}

static BLResult BL_CDECL setStrokeTransformOrderImpl(BLContextImpl* baseImpl, BLStrokeTransformOrder transformOrder) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(uint32_t(transformOrder) > BL_STROKE_TRANSFORM_ORDER_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(ContextFlags::kWeakStateStrokeOptions | ContextFlags::kSharedStateStrokeBase);

  ctxI->internalState.strokeOptions.transformOrder = uint8_t(transformOrder);
  return BL_SUCCESS;
}

static BLResult BL_CDECL setStrokeOptionsImpl(BLContextImpl* baseImpl, const BLStrokeOptionsCore* options) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  if (BL_UNLIKELY(options->startCap > BL_STROKE_CAP_MAX_VALUE ||
                  options->endCap > BL_STROKE_CAP_MAX_VALUE ||
                  options->join > BL_STROKE_JOIN_MAX_VALUE ||
                  options->transformOrder > BL_STROKE_TRANSFORM_ORDER_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  onBeforeStrokeChange(ctxI);
  ctxI->contextFlags &= ~(ContextFlags::kNoStrokeOptions | ContextFlags::kWeakStateStrokeOptions | ContextFlags::kSharedStateStrokeBase);
  return blStrokeOptionsAssignWeak(&ctxI->internalState.strokeOptions, options);
}

// bl::RasterEngine - ContextImpl - Frontend - Clip Operations
// ===========================================================

static BLResult clipToFinalBox(BLRasterContextImpl* ctxI, const BLBox& inputBox) noexcept {
  BLBox b;
  onBeforeClipBoxChange(ctxI);

  if (Geometry::intersect(b, ctxI->finalClipBoxD(), inputBox)) {
    int fpMaskI = ctxI->renderTargetInfo.fpMaskI;
    int fpShiftI = ctxI->renderTargetInfo.fpShiftI;

    ctxI->setFinalClipBoxFixedD(b * ctxI->fpScaleD());
    const BLBoxI& clipBoxFixedI = ctxI->finalClipBoxFixedI();

    ctxI->internalState.finalClipBoxD = b;
    ctxI->internalState.finalClipBoxI.reset((clipBoxFixedI.x0 >> fpShiftI),
                                            (clipBoxFixedI.y0 >> fpShiftI),
                                            (clipBoxFixedI.x1 + fpMaskI) >> fpShiftI,
                                            (clipBoxFixedI.y1 + fpMaskI) >> fpShiftI);

    uint32_t bits = clipBoxFixedI.x0 | clipBoxFixedI.y0 | clipBoxFixedI.x1 | clipBoxFixedI.y1;

    if ((bits & fpMaskI) == 0)
      ctxI->syncWorkData.clipMode = BL_CLIP_MODE_ALIGNED_RECT;
    else
      ctxI->syncWorkData.clipMode = BL_CLIP_MODE_UNALIGNED_RECT;
  }
  else {
    ctxI->internalState.finalClipBoxD.reset();
    ctxI->internalState.finalClipBoxI.reset();
    ctxI->setFinalClipBoxFixedD(BLBox(0, 0, 0, 0));
    ctxI->contextFlags |= ContextFlags::kNoClipRect;
    ctxI->syncWorkData.clipMode = BL_CLIP_MODE_ALIGNED_RECT;
  }

  ctxI->contextFlags &= ~(ContextFlags::kWeakStateClip | ContextFlags::kSharedStateFill);
  return BL_SUCCESS;
}

static BLResult BL_CDECL clipToRectDImpl(BLContextImpl* baseImpl, const BLRect* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  // TODO: [Rendering Context] Path-based clipping.
  BLBox inputBox = BLBox(rect->x, rect->y, rect->x + rect->w, rect->y + rect->h);
  return clipToFinalBox(ctxI, TransformInternal::mapBox(ctxI->finalTransform(), inputBox));
}

static BLResult BL_CDECL clipToRectIImpl(BLContextImpl* baseImpl, const BLRectI* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  // Don't bother if the current ClipBox is not aligned or the translation is not integral.
  if (ctxI->syncWorkData.clipMode != BL_CLIP_MODE_ALIGNED_RECT || !blTestFlag(ctxI->contextFlags, ContextFlags::kInfoIntegralTranslation)) {
    BLRect rectD;
    rectD.x = double(rect->x);
    rectD.y = double(rect->y);
    rectD.w = double(rect->w);
    rectD.h = double(rect->h);
    return clipToRectDImpl(ctxI, &rectD);
  }

  BLBoxI b;
  onBeforeClipBoxChange(ctxI);

  int tx = ctxI->translationI().x;
  int ty = ctxI->translationI().y;

  if (BL_TARGET_ARCH_BITS < 64) {
    OverflowFlag of{};

    int x0 = IntOps::addOverflow(tx, rect->x, &of);
    int y0 = IntOps::addOverflow(ty, rect->y, &of);
    int x1 = IntOps::addOverflow(x0, rect->w, &of);
    int y1 = IntOps::addOverflow(y0, rect->h, &of);

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
    ctxI->contextFlags |= ContextFlags::kNoClipRect;
  }

  ctxI->contextFlags &= ~(ContextFlags::kWeakStateClip | ContextFlags::kSharedStateFill);
  return BL_SUCCESS;
}

static BLResult BL_CDECL restoreClippingImpl(BLContextImpl* baseImpl) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  SavedState* state = ctxI->savedState;

  if (!blTestFlag(ctxI->contextFlags, ContextFlags::kWeakStateClip)) {
    if (state) {
      restoreClippingFromState(ctxI, state);
      ctxI->syncWorkData.clipMode = state->clipMode;
      ctxI->contextFlags &= ~(ContextFlags::kNoClipRect | ContextFlags::kWeakStateClip | ContextFlags::kSharedStateFill);
      ctxI->contextFlags |= (state->prevContextFlags & ContextFlags::kNoClipRect);
    }
    else {
      // If there is no state saved it means that we have to restore clipping to
      // the initial state, which is accessible through `metaClipBoxI` member.
      ctxI->contextFlags &= ~(ContextFlags::kNoClipRect | ContextFlags::kSharedStateFill);
      resetClippingToMetaClipBox(ctxI);
    }
  }

  return BL_SUCCESS;
}

// bl::RasterEngine - ContextImpl - Mask & Blit Utilities
// ======================================================

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

// bl::RasterEngine - ContextImpl - Internals - Asynchronous Rendering - Shared State
// ==================================================================================

static constexpr ContextFlags sharedStrokeStateFlagsTable[BL_STROKE_TRANSFORM_ORDER_MAX_VALUE + 1] = {
  ContextFlags::kSharedStateStrokeBase,
  ContextFlags::kSharedStateStrokeBase | ContextFlags::kSharedStateStrokeExt
};

static constexpr uint32_t sharedStrokeStateSizeTable[BL_STROKE_TRANSFORM_ORDER_MAX_VALUE + 1] = {
  uint32_t(sizeof(SharedBaseStrokeState)),
  uint32_t(sizeof(SharedExtendedStrokeState))
};

// NOTE: These functions are named 'getXXX()', because they are not intended to fail. They allocate data from
// shared data pool, which is ALWAYS available once the frontend function checks ContextFlags and refills the
// pool when necessary. There is ALWAYS enough space in the pool to allocate BOTH shared states, so we don't
// have to do any checks in case that shared fill or stroke states were not created yet.
static BL_INLINE SharedFillState* getSharedFillState(BLRasterContextImpl* ctxI) noexcept {
  SharedFillState* sharedFillState = ctxI->sharedFillState;

  if (!blTestFlag(ctxI->contextFlags, ContextFlags::kSharedStateFill)) {
    sharedFillState = ctxI->workerMgr().allocateFromSharedDataPool<SharedFillState>();

    const BLMatrix2D& ft = ctxI->finalTransformFixed();
    sharedFillState->finalClipBoxFixedD = ctxI->finalClipBoxFixedD();
    sharedFillState->finalTransformFixed = Matrix2x2{ft.m00, ft.m01, ft.m10, ft.m11};
    sharedFillState->toleranceFixedD = ctxI->internalState.toleranceFixedD;

    ctxI->sharedFillState = sharedFillState;
    ctxI->contextFlags |= ContextFlags::kSharedStateFill;
    markQueueFullOrExhausted(ctxI, ctxI->workerMgr().isSharedDataPoolExhausted());
  }

  return sharedFillState;
}

static BL_INLINE SharedBaseStrokeState* getSharedStrokeState(BLRasterContextImpl* ctxI) noexcept {
  SharedBaseStrokeState* sharedStrokeState = ctxI->sharedStrokeState;

  BLStrokeTransformOrder transformOrder = BLStrokeTransformOrder(ctxI->strokeOptions().transformOrder);
  ContextFlags sharedFlags = sharedStrokeStateFlagsTable[transformOrder];

  if ((ctxI->contextFlags & sharedFlags) != sharedFlags) {
    size_t stateSize = sharedStrokeStateSizeTable[transformOrder];
    sharedStrokeState = ctxI->workerMgr().allocateFromSharedDataPool<SharedBaseStrokeState>(stateSize);

    blCallCtor(*sharedStrokeState, ctxI->strokeOptions(), ctxI->approximationOptions());

    if (transformOrder != BL_STROKE_TRANSFORM_ORDER_AFTER) {
      const BLMatrix2D& ut = ctxI->userTransform();
      const BLMatrix2D& mt = ctxI->metaTransformFixed();
      static_cast<SharedExtendedStrokeState*>(sharedStrokeState)->userTransform = Matrix2x2{ut.m00, ut.m01, ut.m10, ut.m11};
      static_cast<SharedExtendedStrokeState*>(sharedStrokeState)->metaTransformFixed = Matrix2x2{mt.m00, mt.m01, mt.m10, mt.m11};
    }

    ctxI->sharedStrokeState = sharedStrokeState;
    ctxI->contextFlags |= sharedFlags;
    markQueueFullOrExhausted(ctxI, ctxI->workerMgr().isSharedDataPoolExhausted());
  }

  return sharedStrokeState;
}

// bl::RasterEngine - ContextImpl - Internals - Asynchronous Rendering - Jobs
// ==========================================================================

template<typename JobType>
static BL_INLINE BLResult newFillJob(BLRasterContextImpl* ctxI, size_t jobDataSize, JobType** out) noexcept {
  JobType* job = ctxI->workerMgr()._allocator.template allocNoAlignT<JobType>(jobDataSize);
  if (BL_UNLIKELY(!job))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  job->initStates(getSharedFillState(ctxI));
  *out = job;
  return BL_SUCCESS;
}

template<typename JobType>
static BL_INLINE BLResult newStrokeJob(BLRasterContextImpl* ctxI, size_t jobDataSize, JobType** out) noexcept {
  JobType* job = ctxI->workerMgr()._allocator.template allocNoAlignT<JobType>(jobDataSize);
  if (BL_UNLIKELY(!job))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  job->initStates(getSharedFillState(ctxI), getSharedStrokeState(ctxI));
  *out = job;
  return BL_SUCCESS;
}

// bl::RasterEngine - ContextImpl - Internals - Asynchronous Rendering - Enqueue
// =============================================================================

template<typename CommandFinalizer>
static BL_INLINE BLResult enqueueCommand(
    BLRasterContextImpl* ctxI,
    RenderCommand* command,
    uint8_t qy0,
    RenderFetchDataHeader* fetchData,
    const CommandFinalizer& commandFinalizer) noexcept {

  WorkerManager& mgr = ctxI->workerMgr();
  constexpr uint32_t kRetainsStyleFetchDataShift = IntOps::bitShiftOf(uint32_t(RenderCommandFlags::kRetainsStyleFetchData));

  if (fetchData->isSolid()) {
    command->_source.solid = static_cast<RenderFetchDataSolid*>(fetchData)->pipelineData;
  }
  else {
    uint32_t batchId = mgr.currentBatchId();
    uint32_t fdRetain = uint32_t(fetchData->batchId != batchId);

    fetchData->batchId = mgr.currentBatchId();
    fetchData->retain(fdRetain);

    RenderCommandFlags flags = RenderCommandFlags(fdRetain << kRetainsStyleFetchDataShift) | RenderCommandFlags::kHasStyleFetchData;
    command->addFlags(flags);
    command->_source.fetchData = static_cast<RenderFetchData*>(fetchData);

    mgr._commandAppender.markFetchData(fdRetain);
  }

  commandFinalizer(command);
  mgr.commandAppender().initQuantizedY0(qy0);
  mgr.commandAppender().advance();
  markQueueFullOrExhausted(ctxI, mgr._commandAppender.full());

  return BL_SUCCESS;
}

template<typename JobType, typename JobFinalizer>
static BL_INLINE BLResult enqueueCommandWithFillJob(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    size_t jobSize, const BLPoint& originFixed, const JobFinalizer& jobFinalizer) noexcept {

  constexpr uint8_t kNoCoord = kInvalidQuantizedCoordinate;

  RenderCommand* command = ctxI->workerMgr->currentCommand();
  JobType* job;

  // TODO: [Rendering Context] FetchData calculation offloading not ready yet - needs more testing:
  // bool wasPending = di.signature.hasPendingFlag();
  // di.signature.clearPendingBit();

  BL_PROPAGATE(ensureFetchAndDispatchData(ctxI, di.signature, ds.fetchData, command->pipeDispatchData()));
  BL_PROPAGATE(newFillJob(ctxI, jobSize, &job));

  return enqueueCommand(ctxI, command, kNoCoord, ds.fetchData, [&](RenderCommand* command) noexcept {
    command->_payload.analytic.stateSlotIndex = ctxI->workerMgr().nextStateSlotIndex();

    WorkerManager& mgr = ctxI->workerMgr();
    job->initFillJob(mgr._commandAppender.queue(), mgr._commandAppender.index());

    // TODO: [Rendering Context] FetchData calculation offloading not ready yet - needs more testing:
    // if (wasPending && command->hasFlag(RenderCommandFlags::kRetainsStyleFetchData))
    //   job->addJobFlags(RenderJobFlags::kComputePendingFetchData);

    job->setOriginFixed(originFixed);
    job->setMetaTransformFixedType(ctxI->metaTransformFixedType());
    job->setFinalTransformFixedType(ctxI->finalTransformFixedType());
    jobFinalizer(job);
    mgr.addJob(job);
    markQueueFullOrExhausted(ctxI, mgr._jobAppender.full());
  });
}

template<typename JobType, typename JobFinalizer>
static BL_INLINE BLResult enqueueCommandWithStrokeJob(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    size_t jobSize, const BLPoint& originFixed, const JobFinalizer& jobFinalizer) noexcept {

  constexpr uint8_t kNoCoord = kInvalidQuantizedCoordinate;

  RenderCommand* command = ctxI->workerMgr->currentCommand();
  JobType* job;

  BL_PROPAGATE(ensureFetchAndDispatchData(ctxI, di.signature, ds.fetchData, command->pipeDispatchData()));
  BL_PROPAGATE(newStrokeJob(ctxI, jobSize, &job));

  return enqueueCommand(ctxI, command, kNoCoord, ds.fetchData, [&](RenderCommand* command) noexcept {
    command->_payload.analytic.stateSlotIndex = ctxI->workerMgr().nextStateSlotIndex();

    WorkerManager& mgr = ctxI->workerMgr();
    job->initStrokeJob(mgr._commandAppender.queue(), mgr._commandAppender.index());

    job->setOriginFixed(originFixed);
    job->setMetaTransformFixedType(ctxI->metaTransformFixedType());
    job->setFinalTransformFixedType(ctxI->finalTransformFixedType());
    jobFinalizer(job);

    mgr.addJob(job);
    markQueueFullOrExhausted(ctxI, mgr._jobAppender.full());
  });
}

template<uint32_t OpType, typename JobType, typename JobFinalizer>
static BL_INLINE BLResult enqueueCommandWithFillOrStrokeJob(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    size_t jobSize, const BLPoint& originFixed, const JobFinalizer& jobFinalizer) noexcept {

  if (OpType == BL_CONTEXT_STYLE_SLOT_FILL)
    return enqueueCommandWithFillJob<JobType, JobFinalizer>(ctxI, di, ds, jobSize, originFixed, jobFinalizer);
  else
    return enqueueCommandWithStrokeJob<JobType, JobFinalizer>(ctxI, di, ds, jobSize, originFixed, jobFinalizer);
}

// bl::RasterEngine - ContextImpl - Asynchronous Rendering - Enqueue GlyphRun & TextData
// =====================================================================================

struct BLGlyphPlacementRawData {
  uint64_t data[2];
};

BL_STATIC_ASSERT(sizeof(BLGlyphPlacementRawData) == sizeof(BLPoint));
BL_STATIC_ASSERT(sizeof(BLGlyphPlacementRawData) == sizeof(BLGlyphPlacement));

template<uint32_t OpType>
static BL_INLINE BLResult enqueueFillOrStrokeGlyphRun(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    const BLPoint* origin, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  size_t size = glyphRun->size;
  size_t glyphDataSize = IntOps::alignUp(size * sizeof(uint32_t), WorkerManager::kAllocatorAlignment);
  size_t placementDataSize = IntOps::alignUp(size * sizeof(BLGlyphPlacementRawData), WorkerManager::kAllocatorAlignment);

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

  BLPoint originFixed = ctxI->finalTransformFixed().mapPoint(*origin);
  di.addFillType(Pipeline::FillType::kAnalytic);

  RenderCommand* command = ctxI->workerMgr->currentCommand();
  command->initCommand(di.alpha);
  command->initFillAnalytic(nullptr, 0, BL_FILL_RULE_NON_ZERO);

  return enqueueCommandWithFillOrStrokeJob<OpType, RenderJob_TextOp>(
    ctxI, di, ds,
    IntOps::alignUp(sizeof(RenderJob_TextOp), WorkerManager::kAllocatorAlignment), originFixed,
    [&](RenderJob_TextOp* job) {
      job->initFont(*font);
      job->initGlyphRun(glyphData, placementData, size, glyphRun->placementType, glyphRun->flags);
    });
}

template<uint32_t OpType>
static BL_INLINE BLResult enqueueFillOrStrokeText(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    const BLPoint* origin, const BLFontCore* font, const void* text, size_t size, BLTextEncoding encoding) noexcept {

  if (size == SIZE_MAX)
    size = StringOps::lengthWithEncoding(text, encoding);

  if (!size)
    return BL_SUCCESS;

  BLResult result = BL_SUCCESS;
  Wrap<BLGlyphBuffer> gb;

  void* serializedTextData = nullptr;
  size_t serializedTextSize = size << textByteSizeShiftByEncoding[encoding];

  if (serializedTextSize > BL_RASTER_CONTEXT_MAXIMUM_EMBEDDED_TEXT_SIZE) {
    gb.init();
    result = gb->setText(text, size, encoding);
  }
  else {
    serializedTextData = ctxI->workerMgr->_allocator.alloc(IntOps::alignUp(serializedTextSize, 8));
    if (!serializedTextData)
      result = BL_ERROR_OUT_OF_MEMORY;
    else
      memcpy(serializedTextData, text, serializedTextSize);
  }

  if (result == BL_SUCCESS) {
    BLPoint originFixed = ctxI->finalTransformFixed().mapPoint(*origin);
    di.addFillType(Pipeline::FillType::kAnalytic);

    RenderCommand* command = ctxI->workerMgr->currentCommand();
    command->initCommand(di.alpha);
    command->initFillAnalytic(nullptr, 0, BL_FILL_RULE_NON_ZERO);

    result = enqueueCommandWithFillOrStrokeJob<OpType, RenderJob_TextOp>(
      ctxI, di, ds,
      IntOps::alignUp(sizeof(RenderJob_TextOp), WorkerManager::kAllocatorAlignment), originFixed,
      [&](RenderJob_TextOp* job) {
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

// bl::RasterEngine - ContextImpl - Internals - Fill Clipped Box
// =============================================================

template<RenderingMode kRM>
static BL_INLINE BLResult fillClippedBoxA(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLBoxI& boxA) noexcept;

template<>
BL_INLINE BLResult fillClippedBoxA<kSync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLBoxI& boxA) noexcept {
  Pipeline::DispatchData dispatchData;
  di.addFillType(Pipeline::FillType::kBoxA);
  BL_PROPAGATE(ensureFetchAndDispatchData(ctxI, di.signature, ds.fetchData, &dispatchData));

  return CommandProcSync::fillBoxA(ctxI->syncWorkData, dispatchData, di.alpha, boxA, ds.fetchData->getPipelineData());
}

template<>
BL_INLINE BLResult fillClippedBoxA<kAsync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLBoxI& boxA) noexcept {
  RenderCommand* command = ctxI->workerMgr->currentCommand();

  di.addFillType(Pipeline::FillType::kBoxA);
  BL_PROPAGATE(ensureFetchAndDispatchData(ctxI, di.signature, ds.fetchData, command->pipeDispatchData()));

  command->initCommand(di.alpha);
  command->initFillBoxA(boxA);

  uint8_t qy0 = uint8_t((boxA.y0) >> ctxI->commandQuantizationShiftAA());
  return enqueueCommand(ctxI, command, qy0, ds.fetchData, [&](RenderCommand*) noexcept {});
}

template<RenderingMode kRM>
static BL_INLINE BLResult fillClippedBoxU(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLBoxI& boxU) noexcept;

template<>
BL_INLINE BLResult fillClippedBoxU<kSync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLBoxI& boxU) noexcept {
  Pipeline::DispatchData dispatchData;
  di.addFillType(Pipeline::FillType::kMask);
  BL_PROPAGATE(ensureFetchAndDispatchData(ctxI, di.signature, ds.fetchData, &dispatchData));

  return CommandProcSync::fillBoxU(ctxI->syncWorkData, dispatchData, di.alpha, boxU, ds.fetchData->getPipelineData());
}

template<>
BL_INLINE BLResult fillClippedBoxU<kAsync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLBoxI& boxU) noexcept {
  RenderCommand* command = ctxI->workerMgr->currentCommand();

  di.addFillType(Pipeline::FillType::kMask);
  BL_PROPAGATE(ensureFetchAndDispatchData(ctxI, di.signature, ds.fetchData, command->pipeDispatchData()));

  command->initCommand(di.alpha);
  command->initFillBoxU(boxU);

  uint8_t qy0 = uint8_t((boxU.y0) >> ctxI->commandQuantizationShiftFp());
  return enqueueCommand(ctxI, command, qy0, ds.fetchData, [&](RenderCommand*) noexcept {});
}

template<RenderingMode kRM>
static BL_INLINE BLResult fillClippedBoxF(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLBoxI& boxU) noexcept;

template<>
BL_INLINE BLResult fillClippedBoxF<kSync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLBoxI& boxU) noexcept {
  if (isBoxAligned24x8(boxU))
    return fillClippedBoxA<kSync>(ctxI, di, ds, BLBoxI(boxU.x0 >> 8, boxU.y0 >> 8, boxU.x1 >> 8, boxU.y1 >> 8));
  else
    return fillClippedBoxU<kSync>(ctxI, di, ds, boxU);
}

template<>
BL_INLINE BLResult fillClippedBoxF<kAsync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLBoxI& boxU) noexcept {
  RenderCommand* command = ctxI->workerMgr->currentCommand();
  command->initCommand(di.alpha);

  uint8_t qy0 = uint8_t(boxU.y0 >> ctxI->commandQuantizationShiftFp());

  if (isBoxAligned24x8(boxU)) {
    di.addFillType(Pipeline::FillType::kBoxA);
    command->initFillBoxA(BLBoxI(boxU.x0 >> 8, boxU.y0 >> 8, boxU.x1 >> 8, boxU.y1 >> 8));
  }
  else {
    di.addFillType(Pipeline::FillType::kMask);
    command->initFillBoxU(boxU);
  }

  BL_PROPAGATE(ensureFetchAndDispatchData(ctxI, di.signature, ds.fetchData, command->pipeDispatchData()));
  return enqueueCommand(ctxI, command, qy0, ds.fetchData, [&](RenderCommand*) noexcept {});
}

// bl::RasterEngine - ContextImpl - Internals - Fill All
// =====================================================

template<RenderingMode kRM>
static BL_NOINLINE BLResult fillAll(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds) noexcept {
  return ctxI->clipMode() == BL_CLIP_MODE_ALIGNED_RECT
    ? fillClippedBoxA<kRM>(ctxI, di, ds, ctxI->finalClipBoxI())
    : fillClippedBoxU<kRM>(ctxI, di, ds, ctxI->finalClipBoxFixedI());
}

// bl::RasterEngine - ContextImpl - Internals - Fill Clipped Edges
// ===============================================================

template<RenderingMode kRM>
static BLResult fillClippedEdges(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, BLFillRule fillRule) noexcept;

template<>
BL_NOINLINE BLResult fillClippedEdges<kSync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, BLFillRule fillRule) noexcept {
  WorkData& workData = ctxI->syncWorkData;
  EdgeStorage<int>& edgeStorage = workData.edgeStorage;

  // NOTE: This doesn't happen often, but it's possible if for example the data in bands is all horizontal lines or no data at all.
  if (edgeStorage.empty() || edgeStorage.boundingBox().y0 >= edgeStorage.boundingBox().y1)
    return BL_SUCCESS;

  Pipeline::DispatchData dispatchData;
  di.addFillType(Pipeline::FillType::kAnalytic);
  BLResult result = ensureFetchAndDispatchData(ctxI, di.signature, ds.fetchData, &dispatchData);
  if (BL_UNLIKELY(result != BL_SUCCESS)) {
    // Must revert the edge builder if we have failed here as we cannot execute the render call.
    workData.revertEdgeBuilder();
    return result;
  }

  return CommandProcSync::fillAnalytic(workData, dispatchData, di.alpha, &edgeStorage, fillRule, ds.fetchData->getPipelineData());
}

template<>
BL_NOINLINE BLResult fillClippedEdges<kAsync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, BLFillRule fillRule) noexcept {
  RenderCommand* command = ctxI->workerMgr->currentCommand();

  WorkData& workData = ctxI->syncWorkData;
  EdgeStorage<int>& edgeStorage = workData.edgeStorage;

  // NOTE: This doesn't happen often, but it's possible if for example the data in bands is all horizontal lines or no data at all.
  if (edgeStorage.empty() || edgeStorage.boundingBox().y0 >= edgeStorage.boundingBox().y1)
    return BL_SUCCESS;

  uint8_t qy0 = uint8_t(edgeStorage.boundingBox().y0 >> ctxI->commandQuantizationShiftFp());

  di.addFillType(Pipeline::FillType::kAnalytic);
  command->initCommand(di.alpha);
  command->initFillAnalytic(edgeStorage.flattenEdgeLinks(), edgeStorage.boundingBox().y0, fillRule);
  edgeStorage.resetBoundingBox();

  BLResult result = ensureFetchAndDispatchData(ctxI, di.signature, ds.fetchData, command->pipeDispatchData());
  if (BL_UNLIKELY(result != BL_SUCCESS)) {
    // Must revert the edge builder if we have failed here as we cannot execute the render call.
    workData.revertEdgeBuilder();
    return result;
  }

  return enqueueCommand(ctxI, command, qy0, ds.fetchData, [&](RenderCommand* command) noexcept {
    command->_payload.analytic.stateSlotIndex = ctxI->workerMgr().nextStateSlotIndex();
  });
}

// bl::RasterEngine - ContextImpl - Internals - Fill Unclipped Path
// ================================================================

template<RenderingMode kRM>
static BL_INLINE BLResult fillUnclippedPath(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    const BLPath& path, BLFillRule fillRule, const BLMatrix2D& transform, BLTransformType transformType) noexcept {

  if BL_CONSTEXPR (kRM == kAsync)
    ctxI->syncWorkData.saveState();

  BL_PROPAGATE(addFilledPathEdges(&ctxI->syncWorkData, path.view(), transform, transformType));
  return fillClippedEdges<kRM>(ctxI, di, ds, fillRule);
}

template<RenderingMode kRM>
static BL_INLINE BLResult fillUnclippedPath(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    const BLPath& path, BLFillRule fillRule) noexcept {

  return fillUnclippedPath<kRM>(ctxI, di, ds, path, fillRule, ctxI->finalTransformFixed(), ctxI->finalTransformFixedType());
}

template<RenderingMode kRM>
static BL_INLINE BLResult fillUnclippedPathWithOrigin(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    const BLPoint& originFixed, const BLPath& path, BLFillRule fillRule) noexcept;

template<>
BL_INLINE BLResult fillUnclippedPathWithOrigin<kSync>(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    const BLPoint& originFixed, const BLPath& path, BLFillRule fillRule) noexcept {
  const BLMatrix2D& ft = ctxI->finalTransformFixed();
  BLMatrix2D transform(ft.m00, ft.m01, ft.m10, ft.m11, originFixed.x, originFixed.y);

  BLTransformType transformType = blMax<BLTransformType>(ctxI->finalTransformFixedType(), BL_TRANSFORM_TYPE_TRANSLATE);
  return fillUnclippedPath<kSync>(ctxI, di, ds, path, fillRule, transform, transformType);
}

template<>
BL_INLINE BLResult fillUnclippedPathWithOrigin<kAsync>(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    const BLPoint& originFixed, const BLPath& path, BLFillRule fillRule) noexcept {

  if (path.size() <= BL_RASTER_CONTEXT_MINIMUM_ASYNC_PATH_SIZE) {
    const BLMatrix2D& ft = ctxI->finalTransformFixed();
    BLMatrix2D transform(ft.m00, ft.m01, ft.m10, ft.m11, originFixed.x, originFixed.y);

    BLTransformType transformType = blMax<BLTransformType>(ctxI->finalTransformFixedType(), BL_TRANSFORM_TYPE_TRANSLATE);
    return fillUnclippedPath<kAsync>(ctxI, di, ds, path, fillRule, transform, transformType);
  }

  size_t jobSize = sizeof(RenderJob_GeometryOp) + sizeof(BLPathCore);
  di.addFillType(Pipeline::FillType::kAnalytic);

  RenderCommand* command = ctxI->workerMgr->currentCommand();
  command->initCommand(di.alpha);
  command->initFillAnalytic(nullptr, 0, fillRule);
  return enqueueCommandWithFillJob<RenderJob_GeometryOp>(ctxI, di, ds, jobSize, originFixed, [&](RenderJob_GeometryOp* job) noexcept { job->setGeometryWithPath(&path); });
}

// bl::RasterEngine - ContextImpl - Internals - Fill Unclipped Polygon
// ===================================================================

template<RenderingMode kRM, typename PointType>
static BL_INLINE BLResult fillUnclippedPolygonT(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    const PointType* pts, size_t size, BLFillRule fillRule, const BLMatrix2D& transform, BLTransformType transformType) noexcept {

  if BL_CONSTEXPR (kRM == kAsync)
    ctxI->syncWorkData.saveState();

  BL_PROPAGATE(addFilledPolygonEdges(&ctxI->syncWorkData, pts, size, transform, transformType));
  return fillClippedEdges<kRM>(ctxI, di, ds, fillRule);
}

template<RenderingMode kRM, typename PointType>
static BL_INLINE BLResult fillUnclippedPolygonT(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    const PointType* pts, size_t size, BLFillRule fillRule) noexcept {

  return fillUnclippedPolygonT<kRM>(ctxI, di, ds, pts, size, fillRule, ctxI->finalTransformFixed(), ctxI->finalTransformFixedType());
}

// bl::RasterEngine - ContextImpl - Internals - Fill Unclipped Box & Rect
// ======================================================================

template<RenderingMode kRM>
static BL_INLINE BLResult fillUnclippedBoxD(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    const BLBox& boxD, const BLMatrix2D& transform, BLTransformType transformType) noexcept {

  if (transformType <= BL_TRANSFORM_TYPE_SWAP) {
    BLBox finalBoxD;
    if (!Geometry::intersect(finalBoxD, TransformInternal::mapBoxScaledSwapped(transform, boxD), ctxI->finalClipBoxFixedD()))
      return BL_SUCCESS;

    BLBoxI boxU = Math::truncToInt(finalBoxD);
    if (boxU.x0 >= boxU.x1 || boxU.y0 >= boxU.y1)
      return BL_SUCCESS;

    return fillClippedBoxF<kRM>(ctxI, di, ds, boxU);
  }
  else {
    BLPoint polyD[] = {BLPoint(boxD.x0, boxD.y0), BLPoint(boxD.x1, boxD.y0), BLPoint(boxD.x1, boxD.y1), BLPoint(boxD.x0, boxD.y1)};
    return fillUnclippedPolygonT<kRM>(ctxI, di, ds, polyD, BL_ARRAY_SIZE(polyD), BL_RASTER_CONTEXT_PREFERRED_FILL_RULE, transform, transformType);
  }
}

template<RenderingMode kRM>
static BL_INLINE BLResult fillUnclippedBoxD(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLBox& boxD) noexcept {
  return fillUnclippedBoxD<kRM>(ctxI, di, ds, boxD, ctxI->finalTransformFixed(), ctxI->finalTransformFixedType());
}

template<RenderingMode kRM>
static BL_INLINE BLResult fillUnclippedRectI(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLRectI& rectI) noexcept {
  int rw = rectI.w;
  int rh = rectI.h;

  if (!blTestFlag(ctxI->contextFlags, ContextFlags::kInfoIntegralTranslation)) {
    // Clipped out.
    if ((rw <= 0) | (rh <= 0))
      return BL_SUCCESS;

    BLBox boxD(double(rectI.x), double(rectI.y), double(rectI.x) + double(rectI.w), double(rectI.y) + double(rectI.h));
    return fillUnclippedBoxD<kRM>(ctxI, di, ds, boxD);
  }

  BLBoxI dstBoxI;
  if (!translateAndClipRectToFillI(ctxI, &rectI, &dstBoxI))
    return BL_SUCCESS;

  return fillClippedBoxA<kRM>(ctxI, di, ds, dstBoxI);
}

// bl::RasterEngine - ContextImpl - Internals - Fill Unclipped Geometry
// ====================================================================

template<RenderingMode kRM>
static BLResult fillUnclippedGeometry(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, BLGeometryType type, const void* data) noexcept;

template<>
BLResult BL_NOINLINE fillUnclippedGeometry<kSync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, BLGeometryType type, const void* data) noexcept {
  // The most common primary geometry operation would be rendering rectangles - so check these first.
  if (type <= BL_GEOMETRY_TYPE_RECTD) {
    BLBox temporaryBox;

    if (type == BL_GEOMETRY_TYPE_RECTI)
      return fillUnclippedRectI<kSync>(ctxI, di, ds, *static_cast<const BLRectI*>(data));

    if (type == BL_GEOMETRY_TYPE_RECTD) {
      const BLRect* r = static_cast<const BLRect*>(data);
      temporaryBox.reset(r->x, r->y, r->x + r->w, r->y + r->h);
      data = &temporaryBox;
    }
    else if (type == BL_GEOMETRY_TYPE_BOXI) {
      const BLBoxI* boxI = static_cast<const BLBoxI*>(data);
      temporaryBox.reset(double(boxI->x0), double(boxI->y0), double(boxI->x1), double(boxI->y1));
      data = &temporaryBox;
    }
    else if (type == BL_GEOMETRY_TYPE_NONE) {
      return BL_SUCCESS;
    }

    return fillUnclippedBoxD<kSync>(ctxI, di, ds, *static_cast<const BLBox*>(data));
  }

  // The most common second geometry operation would be rendering paths.
  if (type != BL_GEOMETRY_TYPE_PATH) {
    if (type == BL_GEOMETRY_TYPE_POLYGONI || type == BL_GEOMETRY_TYPE_POLYLINEI) {
      const BLArrayView<BLPointI>* array = static_cast<const BLArrayView<BLPointI>*>(data);
      if (BL_UNLIKELY(array->size < 3))
        return BL_SUCCESS;

      return fillUnclippedPolygonT<kSync>(ctxI, di, ds, array->data, array->size, ctxI->fillRule());
    }

    if (type == BL_GEOMETRY_TYPE_POLYGOND || type == BL_GEOMETRY_TYPE_POLYLINED) {
      const BLArrayView<BLPoint>* array = static_cast<const BLArrayView<BLPoint>*>(data);
      if (BL_UNLIKELY(array->size < 3))
        return BL_SUCCESS;

      return fillUnclippedPolygonT<kSync>(ctxI, di, ds, array->data, array->size, ctxI->fillRule());
    }

    BLPath* temporaryPath = &ctxI->syncWorkData.tmpPath[3];
    temporaryPath->clear();
    BL_PROPAGATE(temporaryPath->addGeometry(type, data));
    data = temporaryPath;
  }

  const BLPath* path = static_cast<const BLPath*>(data);
  if (BL_UNLIKELY(path->empty()))
    return BL_SUCCESS;

  return fillUnclippedPath<kSync>(ctxI, di, ds, *path, ctxI->fillRule());
}

template<>
BLResult BL_NOINLINE fillUnclippedGeometry<kAsync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, BLGeometryType type, const void* data) noexcept {
  if (type <= BL_GEOMETRY_TYPE_RECTD) {
    BLBox temporaryBox;

    if (type == BL_GEOMETRY_TYPE_RECTI)
      return fillUnclippedRectI<kAsync>(ctxI, di, ds, *static_cast<const BLRectI*>(data));

    if (type == BL_GEOMETRY_TYPE_RECTD) {
      const BLRect* r = static_cast<const BLRect*>(data);
      temporaryBox.reset(r->x, r->y, r->x + r->w, r->y + r->h);
      data = &temporaryBox;
    }
    else if (type == BL_GEOMETRY_TYPE_BOXI) {
      const BLBoxI* boxI = static_cast<const BLBoxI*>(data);
      temporaryBox.reset(double(boxI->x0), double(boxI->y0), double(boxI->x1), double(boxI->y1));
      data = &temporaryBox;
    }
    else if (type == BL_GEOMETRY_TYPE_NONE) {
      return BL_SUCCESS;
    }

    return fillUnclippedBoxD<kAsync>(ctxI, di, ds, *static_cast<const BLBox*>(data));
  }

  BLFillRule fillRule = ctxI->fillRule();

  switch (type) {
    case BL_GEOMETRY_TYPE_POLYGONI:
    case BL_GEOMETRY_TYPE_POLYLINEI: {
      const BLArrayView<BLPointI>* array = static_cast<const BLArrayView<BLPointI>*>(data);
      if (BL_UNLIKELY(array->size < 3))
        return BL_SUCCESS;
      return fillUnclippedPolygonT<kAsync>(ctxI, di, ds, array->data, array->size, fillRule);
    }

    case BL_GEOMETRY_TYPE_POLYGOND:
    case BL_GEOMETRY_TYPE_POLYLINED: {
      const BLArrayView<BLPoint>* array = static_cast<const BLArrayView<BLPoint>*>(data);
      if (BL_UNLIKELY(array->size < 3))
        return BL_SUCCESS;
      return fillUnclippedPolygonT<kAsync>(ctxI, di, ds, array->data, array->size, fillRule);
    }

    case BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTI:
    case BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTD:
    case BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXI:
    case BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXD: {
      BLPath* temporaryPath = &ctxI->syncWorkData.tmpPath[3];
      temporaryPath->clear();
      BL_PROPAGATE(temporaryPath->addGeometry(type, data));

      type = BL_GEOMETRY_TYPE_PATH;
      data = temporaryPath;

      BL_FALLTHROUGH
    }

    case BL_GEOMETRY_TYPE_PATH: {
      const BLPath* path = static_cast<const BLPath*>(data);
      if (path->size() <= BL_RASTER_CONTEXT_MINIMUM_ASYNC_PATH_SIZE)
        return fillUnclippedPath<kAsync>(ctxI, di, ds, *path, fillRule);

      size_t jobSize = sizeof(RenderJob_GeometryOp) + sizeof(BLPathCore);
      BLPoint originFixed(ctxI->finalTransformFixed().m20, ctxI->finalTransformFixed().m21);

      di.addFillType(Pipeline::FillType::kAnalytic);

      RenderCommand* command = ctxI->workerMgr->currentCommand();
      command->initCommand(di.alpha);
      command->initFillAnalytic(nullptr, 0, fillRule);
      return enqueueCommandWithFillJob<RenderJob_GeometryOp>(ctxI, di, ds, jobSize, originFixed, [&](RenderJob_GeometryOp* job) noexcept { job->setGeometryWithPath(path); });
    }

    default: {
      if (!Geometry::isSimpleGeometryType(type))
        return blTraceError(BL_ERROR_INVALID_VALUE);

      size_t geometrySize = Geometry::geometryTypeSizeTable[type];
      size_t jobSize = sizeof(RenderJob_GeometryOp) + geometrySize;
      BLPoint originFixed(ctxI->finalTransformFixed().m20, ctxI->finalTransformFixed().m21);

      di.addFillType(Pipeline::FillType::kAnalytic);

      RenderCommand* command = ctxI->workerMgr->currentCommand();
      command->initCommand(di.alpha);
      command->initFillAnalytic(nullptr, 0, fillRule);

      return enqueueCommandWithFillJob<RenderJob_GeometryOp>(ctxI, di, ds, jobSize, originFixed, [&](RenderJob_GeometryOp* job) noexcept { job->setGeometryWithShape(type, data, geometrySize); });
    }
  }
}

// bl::RasterEngine - ContextImpl - Internals - Fill Unclipped Text
// ================================================================

template<RenderingMode kRM>
static BLResult fillUnclippedText(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* data) noexcept;

template<>
BL_NOINLINE BLResult fillUnclippedText<kSync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* data) noexcept {
  const BLGlyphRun* glyphRun = nullptr;

  if (opType <= BLContextRenderTextOp(BL_TEXT_ENCODING_MAX_VALUE)) {
    BLTextEncoding encoding = static_cast<BLTextEncoding>(opType);
    const BLDataView* view = static_cast<const BLDataView*>(data);

    BLGlyphBuffer& gb = ctxI->syncWorkData.glyphBuffer;
    BL_PROPAGATE(gb.setText(view->data, view->size, encoding));
    BL_PROPAGATE(font->dcast().shape(gb));
    glyphRun = &gb.glyphRun();
  }
  else if (opType == BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN) {
    glyphRun = static_cast<const BLGlyphRun*>(data);
  }
  else {
    return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  if (glyphRun->empty())
    return BL_SUCCESS;

  BLPoint originFixed = ctxI->finalTransformFixed().mapPoint(*origin);
  WorkData* workData = &ctxI->syncWorkData;

  BL_PROPAGATE(addFilledGlyphRunEdges(workData, DirectStateAccessor(ctxI), originFixed, font, glyphRun));
  return fillClippedEdges<kSync>(ctxI, di, ds, BL_FILL_RULE_NON_ZERO);
}

template<>
BL_NOINLINE BLResult fillUnclippedText<kAsync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* data) noexcept {
  if (opType <= BLContextRenderTextOp(BL_TEXT_ENCODING_MAX_VALUE)) {
    const BLDataView* view = static_cast<const BLDataView*>(data);
    BLTextEncoding encoding = static_cast<BLTextEncoding>(opType);

    if (view->size == 0)
      return BL_SUCCESS;

    return enqueueFillOrStrokeText<BL_CONTEXT_STYLE_SLOT_FILL>(ctxI, di, ds, origin, font, view->data, view->size, encoding);
  }
  else if (opType == BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN) {
    const BLGlyphRun* glyphRun = static_cast<const BLGlyphRun*>(data);
    return enqueueFillOrStrokeGlyphRun<BL_CONTEXT_STYLE_SLOT_FILL>(ctxI, di, ds, origin, font, glyphRun);
  }
  else {
    return blTraceError(BL_ERROR_INVALID_VALUE);
  }
}

// bl::RasterEngine - ContextImpl - Internals - Fill Mask
// ======================================================

template<RenderingMode kRM>
static BLResult fillClippedBoxMaskedA(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    const BLBoxI& boxA, const BLImageCore* mask, const BLPointI& maskOffsetI) noexcept;

template<>
BL_NOINLINE BLResult fillClippedBoxMaskedA<kSync>(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    const BLBoxI& boxA, const BLImageCore* mask, const BLPointI& maskOffsetI) noexcept {

  Pipeline::DispatchData dispatchData;

  di.addFillType(Pipeline::FillType::kMask);
  BL_PROPAGATE(ensureFetchAndDispatchData(ctxI, di.signature, ds.fetchData, &dispatchData));

  RenderCommand::FillBoxMaskA payload;
  payload.maskImageI.ptr = ImageInternal::getImpl(mask);
  payload.maskOffsetI = maskOffsetI;
  payload.boxI = boxA;
  return CommandProcSync::fillBoxMaskedA(ctxI->syncWorkData, dispatchData, di.alpha, payload, ds.fetchData->getPipelineData());
}

template<>
BL_NOINLINE BLResult fillClippedBoxMaskedA<kAsync>(
    BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds,
    const BLBoxI& boxA, const BLImageCore* mask, const BLPointI& maskOffsetI) noexcept {

  RenderCommand* command = ctxI->workerMgr->currentCommand();

  di.addFillType(Pipeline::FillType::kMask);
  BL_PROPAGATE(ensureFetchAndDispatchData(ctxI, di.signature, ds.fetchData, command->pipeDispatchData()));

  command->initCommand(di.alpha);
  command->initFillBoxMaskA(boxA, mask, maskOffsetI);

  uint8_t qy0 = uint8_t(boxA.y0 >> ctxI->commandQuantizationShiftAA());

  return enqueueCommand(ctxI, command, qy0, ds.fetchData, [&](RenderCommand* command) noexcept {
    ObjectInternal::retainImpl<RCMode::kMaybe>(command->_payload.boxMaskA.maskImageI.ptr);
  });
}

// bl::RasterEngine - ContextImpl - Internals - Stroke Unclipped Path
// ==================================================================

template<RenderingMode kRM>
static BLResult strokeUnclippedPath(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLPoint& originFixed, const BLPath& path) noexcept;

template<>
BL_NOINLINE BLResult strokeUnclippedPath<kSync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLPoint& originFixed, const BLPath& path) noexcept {
  WorkData* workData = &ctxI->syncWorkData;
  BL_PROPAGATE(addStrokedPathEdges(workData, DirectStateAccessor(ctxI), originFixed, &path));

  return fillClippedEdges<kSync>(ctxI, di, ds, BL_FILL_RULE_NON_ZERO);
}

template<>
BL_NOINLINE BLResult strokeUnclippedPath<kAsync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLPoint& originFixed, const BLPath& path) noexcept {
  size_t jobSize = sizeof(RenderJob_GeometryOp) + sizeof(BLPathCore);
  di.addFillType(Pipeline::FillType::kAnalytic);

  RenderCommand* command = ctxI->workerMgr->currentCommand();
  command->initCommand(di.alpha);
  command->initFillAnalytic(nullptr, 0, BL_FILL_RULE_NON_ZERO);

  return enqueueCommandWithStrokeJob<RenderJob_GeometryOp>(ctxI, di, ds, jobSize, originFixed, [&](RenderJob_GeometryOp* job) noexcept {
    job->setGeometryWithPath(&path);
  });
}

// bl::RasterEngine - ContextImpl - Internals - Stroke Unclipped Geometry
// ======================================================================

template<RenderingMode kRM>
static BLResult strokeUnclippedGeometry(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, BLGeometryType type, const void* data) noexcept;

template<>
BL_NOINLINE BLResult strokeUnclippedGeometry<kSync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, BLGeometryType type, const void* data) noexcept {
  WorkData* workData = &ctxI->syncWorkData;
  BLPath* path = const_cast<BLPath*>(static_cast<const BLPath*>(data));

  if (type != BL_GEOMETRY_TYPE_PATH) {
    path = &workData->tmpPath[3];
    path->clear();
    BL_PROPAGATE(path->addGeometry(type, data));
  }

  BLPoint originFixed(ctxI->finalTransformFixed().m20, ctxI->finalTransformFixed().m21);
  BL_PROPAGATE(addStrokedPathEdges(workData, DirectStateAccessor(ctxI), originFixed, path));

  return fillClippedEdges<kSync>(ctxI, di, ds, BL_FILL_RULE_NON_ZERO);
}

template<>
BL_NOINLINE BLResult strokeUnclippedGeometry<kAsync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, BLGeometryType type, const void* data) noexcept {
  size_t geometrySize = sizeof(BLPathCore);
  if (Geometry::isSimpleGeometryType(type)) {
    geometrySize = Geometry::geometryTypeSizeTable[type];
  }
  else if (type != BL_GEOMETRY_TYPE_PATH) {
    BLPath* temporaryPath = &ctxI->syncWorkData.tmpPath[3];

    temporaryPath->clear();
    BL_PROPAGATE(temporaryPath->addGeometry(type, data));

    type = BL_GEOMETRY_TYPE_PATH;
    data = temporaryPath;
  }

  size_t jobSize = sizeof(RenderJob_GeometryOp) + geometrySize;
  BLPoint originFixed(ctxI->finalTransformFixed().m20, ctxI->finalTransformFixed().m21);

  di.addFillType(Pipeline::FillType::kAnalytic);

  RenderCommand* command = ctxI->workerMgr->currentCommand();
  command->initCommand(di.alpha);
  command->initFillAnalytic(nullptr, 0, BL_FILL_RULE_NON_ZERO);

  return enqueueCommandWithStrokeJob<RenderJob_GeometryOp>(ctxI, di, ds, jobSize, originFixed, [&](RenderJob_GeometryOp* job) noexcept {
    job->setGeometry(type, data, geometrySize);
  });
}

// bl::RasterEngine - ContextImpl - Internals - Stroke Unclipped Text
// ==================================================================

template<RenderingMode kRM>
static BLResult strokeUnclippedText(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* data) noexcept;

template<>
BL_NOINLINE BLResult strokeUnclippedText<kSync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* data) noexcept {
  const BLGlyphRun* glyphRun = nullptr;

  if (opType <= BLContextRenderTextOp(BL_TEXT_ENCODING_MAX_VALUE)) {
    BLTextEncoding encoding = static_cast<BLTextEncoding>(opType);
    const BLDataView* view = static_cast<const BLDataView*>(data);

    BLGlyphBuffer& gb = ctxI->syncWorkData.glyphBuffer;
    BL_PROPAGATE(gb.setText(view->data, view->size, encoding));
    BL_PROPAGATE(font->dcast().shape(gb));
    glyphRun = &gb.glyphRun();
  }
  else if (opType == BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN) {
    glyphRun = static_cast<const BLGlyphRun*>(data);
  }
  else {
    return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  if (glyphRun->empty())
    return BL_SUCCESS;

  BLPoint originFixed = ctxI->finalTransformFixed().mapPoint(*origin);
  WorkData* workData = &ctxI->syncWorkData;

  BL_PROPAGATE(addStrokedGlyphRunEdges(workData, DirectStateAccessor(ctxI), originFixed, font, glyphRun));
  return fillClippedEdges<kSync>(ctxI, di, ds, BL_FILL_RULE_NON_ZERO);
}

template<>
BL_NOINLINE BLResult strokeUnclippedText<kAsync>(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* data) noexcept {
  if (opType <= BLContextRenderTextOp(BL_TEXT_ENCODING_MAX_VALUE)) {
    const BLDataView* view = static_cast<const BLDataView*>(data);
    BLTextEncoding encoding = static_cast<BLTextEncoding>(opType);

    if (view->size == 0)
      return BL_SUCCESS;

    return enqueueFillOrStrokeText<BL_CONTEXT_STYLE_SLOT_STROKE>(ctxI, di, ds, origin, font, view->data, view->size, encoding);
  }
  else if (opType == BL_CONTEXT_RENDER_TEXT_OP_GLYPH_RUN) {
    const BLGlyphRun* glyphRun = static_cast<const BLGlyphRun*>(data);
    return enqueueFillOrStrokeGlyphRun<BL_CONTEXT_STYLE_SLOT_STROKE>(ctxI, di, ds, origin, font, glyphRun);
  }
  else {
    return blTraceError(BL_ERROR_INVALID_VALUE);
  }
}

// bl::RasterEngine - ContextImpl - Frontend - Clear All
// =====================================================

template<RenderingMode kRM>
static BLResult BL_CDECL clearAllImpl(BLContextImpl* baseImpl) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_CLEAR_OP(ContextFlags::kNoClearOpAll);
  return fillAll<kRM>(ctxI, di, ds);
}

// bl::RasterEngine - ContextImpl - Frontend - Clear Rect
// ======================================================

template<RenderingMode kRM>
static BLResult BL_CDECL clearRectIImpl(BLContextImpl* baseImpl, const BLRectI* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_CLEAR_OP(ContextFlags::kNoClearOp);
  return fillUnclippedRectI<kRM>(ctxI, di, ds, *rect);
}

template<RenderingMode kRM>
static BLResult BL_CDECL clearRectDImpl(BLContextImpl* baseImpl, const BLRect* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_CLEAR_OP(ContextFlags::kNoClearOp);
  BLBox boxD(rect->x, rect->y, rect->x + rect->w, rect->y + rect->h);
  return fillUnclippedBoxD<kRM>(ctxI, di, ds, boxD);
}

// bl::RasterEngine - ContextImpl - Frontend - Fill All
// ====================================================

template<RenderingMode kRM>
static BLResult BL_CDECL fillAllImpl(BLContextImpl* baseImpl) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_IMPLICIT_STYLE_OP(ContextFlags::kNoFillOpAllImplicit, BL_CONTEXT_STYLE_SLOT_FILL, kNoBail);
  return fillAll<kRM>(ctxI, di, ds);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillAllRgba32Impl(BLContextImpl* baseImpl, uint32_t rgba32) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_SOLID_OP(ContextFlags::kNoFillOpAllExplicit, BL_CONTEXT_STYLE_SLOT_FILL, rgba32, kNoBail);
  return fillAll<kRM>(ctxI, di, ds);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillAllExtImpl(BLContextImpl* baseImpl, const BLObjectCore* style) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_STYLE_OP(ContextFlags::kNoFillOpAllExplicit, BL_CONTEXT_STYLE_SLOT_FILL, style, kNoBail);
  BLResult result = fillAll<kRM>(ctxI, di, ds);

  return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), result);
}

// bl::RasterEngine - ContextImpl - Frontend - Fill Rect
// =====================================================

template<RenderingMode kRM>
static BLResult BL_CDECL fillRectIImpl(BLContextImpl* baseImpl, const BLRectI* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_IMPLICIT_STYLE_OP(ContextFlags::kNoFillOpImplicit, BL_CONTEXT_STYLE_SLOT_FILL, kNoBail);
  return fillUnclippedRectI<kRM>(ctxI, di, ds, *rect);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillRectIRgba32Impl(BLContextImpl* baseImpl, const BLRectI* rect, uint32_t rgba32) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_SOLID_OP(ContextFlags::kNoFillOpExplicit, BL_CONTEXT_STYLE_SLOT_FILL, rgba32, kNoBail);
  return fillUnclippedRectI<kRM>(ctxI, di, ds, *rect);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillRectIExtImpl(BLContextImpl* baseImpl, const BLRectI* rect, const BLObjectCore* style) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_STYLE_OP(ContextFlags::kNoFillOpExplicit, BL_CONTEXT_STYLE_SLOT_FILL, style, kNoBail);
  BLResult result = fillUnclippedRectI<kRM>(ctxI, di, ds, *rect);

  return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), result);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillRectDImpl(BLContextImpl* baseImpl, const BLRect* rect) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_IMPLICIT_STYLE_OP(ContextFlags::kNoFillOpImplicit, BL_CONTEXT_STYLE_SLOT_FILL, kNoBail);
  BLBox boxD(rect->x, rect->y, rect->x + rect->w, rect->y + rect->h);
  return fillUnclippedBoxD<kRM>(ctxI, di, ds, boxD);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillRectDRgba32Impl(BLContextImpl* baseImpl, const BLRect* rect, uint32_t rgba32) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_SOLID_OP(ContextFlags::kNoFillOpExplicit, BL_CONTEXT_STYLE_SLOT_FILL, rgba32, kNoBail);
  BLBox boxD(rect->x, rect->y, rect->x + rect->w, rect->y + rect->h);
  return fillUnclippedBoxD<kRM>(ctxI, di, ds, boxD);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillRectDExtImpl(BLContextImpl* baseImpl, const BLRect* rect, const BLObjectCore* style) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_STYLE_OP(ContextFlags::kNoFillOpExplicit, BL_CONTEXT_STYLE_SLOT_FILL, style, kNoBail);
  BLBox boxD(rect->x, rect->y, rect->x + rect->w, rect->y + rect->h);
  BLResult result = fillUnclippedBoxD<kRM>(ctxI, di, ds, boxD);

  return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), result);
}

// bl::RasterEngine - ContextImpl - Frontend - Fill Path
// =====================================================

template<RenderingMode kRM>
static BLResult BL_CDECL fillPathDImpl(BLContextImpl* baseImpl, const BLPoint* origin, const BLPathCore* path) noexcept {
  BL_ASSERT(path->_d.isPath());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  bool bail = path->dcast().empty();
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_IMPLICIT_STYLE_OP(ContextFlags::kNoFillOpImplicit, BL_CONTEXT_STYLE_SLOT_FILL, bail);
  BLPoint originFixed = ctxI->finalTransformFixed().mapPoint(*origin);
  return fillUnclippedPathWithOrigin<kRM>(ctxI, di, ds, originFixed, path->dcast(), ctxI->fillRule());
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillPathDRgba32Impl(BLContextImpl* baseImpl, const BLPoint* origin, const BLPathCore* path, uint32_t rgba32) noexcept {
  BL_ASSERT(path->_d.isPath());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  bool bail = path->dcast().empty();
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_SOLID_OP(ContextFlags::kNoFillOpExplicit, BL_CONTEXT_STYLE_SLOT_FILL, rgba32, bail);
  BLPoint originFixed = ctxI->finalTransformFixed().mapPoint(*origin);
  return fillUnclippedPathWithOrigin<kRM>(ctxI, di, ds, originFixed, path->dcast(), ctxI->fillRule());
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillPathDExtImpl(BLContextImpl* baseImpl, const BLPoint* origin, const BLPathCore* path, const BLObjectCore* style) noexcept {
  BL_ASSERT(path->_d.isPath());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  bool bail = path->dcast().empty();
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_STYLE_OP(ContextFlags::kNoFillOpExplicit, BL_CONTEXT_STYLE_SLOT_FILL, style, bail);
  BLPoint originFixed = ctxI->finalTransformFixed().mapPoint(*origin);
  BLResult result = fillUnclippedPathWithOrigin<kRM>(ctxI, di, ds, originFixed, path->dcast(), ctxI->fillRule());

  return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), result);
}

// bl::RasterEngine - ContextImpl - Frontend - Fill Geometry
// =========================================================

template<RenderingMode kRM>
static BLResult BL_CDECL fillGeometryImpl(BLContextImpl* baseImpl, BLGeometryType type, const void* data) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_IMPLICIT_STYLE_OP(ContextFlags::kNoFillOpImplicit, BL_CONTEXT_STYLE_SLOT_FILL, kNoBail);
  return fillUnclippedGeometry<kRM>(ctxI, di, ds, type, data);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillGeometryRgba32Impl(BLContextImpl* baseImpl, BLGeometryType type, const void* data, uint32_t rgba32) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_SOLID_OP(ContextFlags::kNoFillOpExplicit, BL_CONTEXT_STYLE_SLOT_FILL, rgba32, kNoBail);
  return fillUnclippedGeometry<kRM>(ctxI, di, ds, type, data);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillGeometryExtImpl(BLContextImpl* baseImpl, BLGeometryType type, const void* data, const BLObjectCore* style) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_STYLE_OP(ContextFlags::kNoFillOpExplicit, BL_CONTEXT_STYLE_SLOT_FILL, style, kNoBail);
  BLResult result = fillUnclippedGeometry<kRM>(ctxI, di, ds, type, data);

  return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), result);
}

// bl::RasterEngine - ContextImpl - Frontend - Fill Unclipped Text
// ===============================================================

template<RenderingMode kRM>
static BLResult BL_CDECL fillTextOpDImpl(BLContextImpl* baseImpl, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* opData) noexcept {
  BL_ASSERT(font->_d.isFont());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  bool bail = !font->dcast().isValid();
  BLResult bailResult = bail ? blTraceError(BL_ERROR_FONT_NOT_INITIALIZED) : BLResult(BL_SUCCESS);

  BL_CONTEXT_RESOLVE_IMPLICIT_STYLE_OP(ContextFlags::kNoFillOpImplicit, BL_CONTEXT_STYLE_SLOT_FILL, bail);
  return fillUnclippedText<kRM>(ctxI, di, ds, origin, font, opType, opData);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillTextOpIImpl(BLContextImpl* baseImpl, const BLPointI* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* opData) noexcept {
  BL_ASSERT(font->_d.isFont());

  BLPoint originD(*origin);
  return fillTextOpDImpl<kRM>(baseImpl, &originD, font, opType, opData);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillTextOpDRgba32Impl(BLContextImpl* baseImpl, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* opData, uint32_t rgba32) noexcept {
  BL_ASSERT(font->_d.isFont());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  bool bail = !font->dcast().isValid();
  BLResult bailResult = bail ? blTraceError(BL_ERROR_FONT_NOT_INITIALIZED) : BLResult(BL_SUCCESS);

  BL_CONTEXT_RESOLVE_EXPLICIT_SOLID_OP(ContextFlags::kNoFillOpExplicit, BL_CONTEXT_STYLE_SLOT_FILL, rgba32, bail);
  return fillUnclippedText<kRM>(ctxI, di, ds, origin, font, opType, opData);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillTextOpDExtImpl(BLContextImpl* baseImpl, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* opData, const BLObjectCore* style) noexcept {
  BL_ASSERT(font->_d.isFont());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  bool bail = !font->dcast().isValid();
  BLResult bailResult = bail ? blTraceError(BL_ERROR_FONT_NOT_INITIALIZED) : BLResult(BL_SUCCESS);

  BL_CONTEXT_RESOLVE_EXPLICIT_STYLE_OP(ContextFlags::kNoFillOpExplicit, BL_CONTEXT_STYLE_SLOT_FILL, style, bail);
  BLResult result = fillUnclippedText<kRM>(ctxI, di, ds, origin, font, opType, opData);

  return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), result);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillTextOpIRgba32Impl(BLContextImpl* baseImpl, const BLPointI* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* opData, uint32_t rgba32) noexcept {
  BL_ASSERT(font->_d.isFont());

  BLPoint originD(*origin);
  return fillTextOpDRgba32Impl<kRM>(baseImpl, &originD, font, opType, opData, rgba32);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillTextOpIExtImpl(BLContextImpl* baseImpl, const BLPointI* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* opData, const BLObjectCore* style) noexcept {
  BL_ASSERT(font->_d.isFont());

  BLPoint originD(*origin);
  return fillTextOpDExtImpl<kRM>(baseImpl, &originD, font, opType, opData, style);
}

// bl::RasterEngine - ContextImpl - Frontend - Fill Mask
// =====================================================

template<RenderingMode kRM>
static BL_INLINE BLResult fillUnclippedMaskD(BLRasterContextImpl* ctxI, DispatchInfo di, DispatchStyle ds, BLPoint dst, const BLImageCore* mask, BLRectI maskRect) noexcept {
  if (ctxI->finalTransformType() <= BL_TRANSFORM_TYPE_TRANSLATE) {
    double startX = dst.x * ctxI->finalTransformFixed().m00 + ctxI->finalTransformFixed().m20;
    double startY = dst.y * ctxI->finalTransformFixed().m11 + ctxI->finalTransformFixed().m21;

    BLBox dstBoxD(blMax(startX, ctxI->finalClipBoxFixedD().x0),
                  blMax(startY, ctxI->finalClipBoxFixedD().y0),
                  blMin(startX + double(maskRect.w) * ctxI->finalTransformFixed().m00, ctxI->finalClipBoxFixedD().x1),
                  blMin(startY + double(maskRect.h) * ctxI->finalTransformFixed().m11, ctxI->finalClipBoxFixedD().y1));

    // Clipped out, invalid coordinates, or empty `maskArea`.
    if (!((dstBoxD.x0 < dstBoxD.x1) & (dstBoxD.y0 < dstBoxD.y1)))
      return BL_SUCCESS;

    int64_t startFx = Math::floorToInt64(startX);
    int64_t startFy = Math::floorToInt64(startY);

    BLBoxI dstBoxU = Math::truncToInt(dstBoxD);

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
      if (isBoxAligned24x8(dstBoxU)) {
        return fillClippedBoxMaskedA<kRM>(ctxI, di, ds, BLBoxI(x0, y0, x1, y1), mask, BLPointI(maskRect.x, maskRect.y));
      }

      // TODO: [Rendering Context] Masking support.
      /*
      BL_PROPAGATE(serializer.initFetchDataForMask(ctxI));
      serializer.maskFetchData()->initImageSource(mask, maskRect);
      if (!serializer.maskFetchData()->setupPatternBlit(x0, y0))
        return BL_SUCCESS;
      */
    }
    else {
      // TODO: [Rendering Context] Masking support.
      /*
      BL_PROPAGATE(serializer.initFetchDataForMask(ctxI));
      serializer.maskFetchData()->initImageSource(mask, maskRect);
      if (!serializer.maskFetchData()->setupPatternFxFy(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, ctxI->hints().patternQuality, startFx, startFy))
        return BL_SUCCESS;
      */
    }

    /*
    return fillClippedBoxU<RenderCommandSerializerFlags::kMask>(ctxI, serializer, dstBoxU);
    */
  }

  return blTraceError(BL_ERROR_NOT_IMPLEMENTED);

  // TODO: [Rendering Context] Masking support.
  /*
  else {
    BLMatrix2D m(ctxI->finalTransform());
    m.translate(dst.x, dst.y);

    BL_PROPAGATE(serializer.initFetchDataForMask(ctxI));
    serializer.maskFetchData()->initImageSource(mask, maskRect);
    if (!serializer.maskFetchData()->setupPatternAffine(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, ctxI->hints().patternQuality, m)) {
      serializer.rollbackFetchData(ctxI);
      return BL_SUCCESS;
    }
  }

  BLBox finalBox(dst.x, dst.y, dst.x + double(maskRect.w), dst.y + double(maskRect.h));
  return blRasterContextImplFinalizeBlit(ctxI, serializer,
         fillUnclippedBox<RenderCommandSerializerFlags::kMask>(ctxI, serializer, finalBox));
  */
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillMaskDImpl(BLContextImpl* baseImpl, const BLPoint* origin, const BLImageCore* mask, const BLRectI* maskArea) noexcept {
  BL_ASSERT(mask->_d.isImage());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BLRectI maskRect;
  BLResult bailResult = checkImageArea(maskRect, ImageInternal::getImpl(mask), maskArea);
  bool bail = bailResult != BL_SUCCESS;

  BL_CONTEXT_RESOLVE_IMPLICIT_STYLE_OP(ContextFlags::kNoFillOpImplicit, BL_CONTEXT_STYLE_SLOT_FILL, bail);
  return fillUnclippedMaskD<kRM>(ctxI, di, ds, *origin, mask, maskRect);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillMaskDRgba32Impl(BLContextImpl* baseImpl, const BLPoint* origin, const BLImageCore* mask, const BLRectI* maskArea, uint32_t rgba32) noexcept {
  BL_ASSERT(mask->_d.isImage());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BLRectI maskRect;
  BLResult bailResult = checkImageArea(maskRect, ImageInternal::getImpl(mask), maskArea);
  bool bail = bailResult != BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_SOLID_OP(ContextFlags::kNoFillOpImplicit, BL_CONTEXT_STYLE_SLOT_FILL, rgba32, bail);
  return fillUnclippedMaskD<kRM>(ctxI, di, ds, *origin, mask, maskRect);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillMaskDExtImpl(BLContextImpl* baseImpl, const BLPoint* origin, const BLImageCore* mask, const BLRectI* maskArea, const BLObjectCore* style) noexcept {
  BL_ASSERT(mask->_d.isImage());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BLRectI maskRect;
  BLResult bailResult = checkImageArea(maskRect, ImageInternal::getImpl(mask), maskArea);
  bool bail = bailResult != BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_STYLE_OP(ContextFlags::kNoFillOpImplicit, BL_CONTEXT_STYLE_SLOT_FILL, style, bail);
  BLResult result = fillUnclippedMaskD<kRM>(ctxI, di, ds, *origin, mask, maskRect);

  return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), result);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillMaskIImpl(BLContextImpl* baseImpl, const BLPointI* origin, const BLImageCore* mask, const BLRectI* maskArea) noexcept {
  BL_ASSERT(mask->_d.isImage());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  if (!blTestFlag(ctxI->contextFlags, ContextFlags::kInfoIntegralTranslation)) {
    BLPoint originD(*origin);
    return fillMaskDImpl<kRM>(ctxI, &originD, mask, maskArea);
  }

  BLImageImpl* maskI = ImageInternal::getImpl(mask);

  BLBoxI dstBox;
  BLPointI srcOffset;

  BLResult bailResult;
  bool bail = !translateAndClipRectToBlitI(ctxI, origin, maskArea, &maskI->size, &bailResult, &dstBox, &srcOffset);

  BL_CONTEXT_RESOLVE_IMPLICIT_STYLE_OP(ContextFlags::kNoFillOpImplicit, BL_CONTEXT_STYLE_SLOT_FILL, bail);
  return fillClippedBoxMaskedA<kRM>(ctxI, di, ds, dstBox, mask, srcOffset);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillMaskIRgba32Impl(BLContextImpl* baseImpl, const BLPointI* origin, const BLImageCore* mask, const BLRectI* maskArea, uint32_t rgba32) noexcept {
  BL_ASSERT(mask->_d.isImage());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  if (!blTestFlag(ctxI->contextFlags, ContextFlags::kInfoIntegralTranslation)) {
    BLPoint originD(*origin);
    return fillMaskDRgba32Impl<kRM>(ctxI, &originD, mask, maskArea, rgba32);
  }

  BLImageImpl* maskI = ImageInternal::getImpl(mask);

  BLBoxI dstBox;
  BLPointI srcOffset;

  BLResult bailResult;
  bool bail = !translateAndClipRectToBlitI(ctxI, origin, maskArea, &maskI->size, &bailResult, &dstBox, &srcOffset);

  BL_CONTEXT_RESOLVE_EXPLICIT_SOLID_OP(ContextFlags::kNoFillOpImplicit, BL_CONTEXT_STYLE_SLOT_FILL, rgba32, bail);
  return fillClippedBoxMaskedA<kRM>(ctxI, di, ds, dstBox, mask, srcOffset);
}

template<RenderingMode kRM>
static BLResult BL_CDECL fillMaskIExtImpl(BLContextImpl* baseImpl, const BLPointI* origin, const BLImageCore* mask, const BLRectI* maskArea, const BLObjectCore* style) noexcept {
  BL_ASSERT(mask->_d.isImage());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  if (!blTestFlag(ctxI->contextFlags, ContextFlags::kInfoIntegralTranslation)) {
    BLPoint originD(*origin);
    return fillMaskDExtImpl<kRM>(ctxI, &originD, mask, maskArea, style);
  }

  BLImageImpl* maskI = ImageInternal::getImpl(mask);

  BLBoxI dstBox;
  BLPointI srcOffset;

  BLResult bailResult;
  bool bail = !translateAndClipRectToBlitI(ctxI, origin, maskArea, &maskI->size, &bailResult, &dstBox, &srcOffset);

  BL_CONTEXT_RESOLVE_EXPLICIT_STYLE_OP(ContextFlags::kNoFillOpImplicit, BL_CONTEXT_STYLE_SLOT_FILL, style, bail);
  BLResult result = fillClippedBoxMaskedA<kRM>(ctxI, di, ds, dstBox, mask, srcOffset);

  return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), result);
}

// bl::RasterEngine - ContextImpl - Frontend - Stroke Geometry
// ===========================================================

template<RenderingMode kRM>
static BLResult BL_CDECL strokeGeometryImpl(BLContextImpl* baseImpl, BLGeometryType type, const void* data) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_IMPLICIT_STYLE_OP(ContextFlags::kNoStrokeOpImplicit, BL_CONTEXT_STYLE_SLOT_STROKE, kNoBail);
  return strokeUnclippedGeometry<kRM>(ctxI, di, ds, type, data);
}

template<RenderingMode kRM>
static BLResult BL_CDECL strokeGeometryRgba32Impl(BLContextImpl* baseImpl, BLGeometryType type, const void* data, uint32_t rgba32) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_SOLID_OP(ContextFlags::kNoStrokeOpExplicit, BL_CONTEXT_STYLE_SLOT_STROKE, rgba32, kNoBail);
  return strokeUnclippedGeometry<kRM>(ctxI, di, ds, type, data);
}

template<RenderingMode kRM>
static BLResult BL_CDECL strokeGeometryExtImpl(BLContextImpl* baseImpl, BLGeometryType type, const void* data, const BLObjectCore* style) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_STYLE_OP(ContextFlags::kNoStrokeOpExplicit, BL_CONTEXT_STYLE_SLOT_STROKE, style, kNoBail);
  BLResult result = strokeUnclippedGeometry<kRM>(ctxI, di, ds, type, data);

  return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), result);
}

// bl::RasterEngine - ContextImpl - Frontend - Stroke Path
// =======================================================

template<RenderingMode kRM>
static BLResult BL_CDECL strokePathDImpl(BLContextImpl* baseImpl, const BLPoint* origin, const BLPathCore* path) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BL_ASSERT(path->_d.isPath());

  bool bail = path->dcast().empty();
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_IMPLICIT_STYLE_OP(ContextFlags::kNoStrokeOpImplicit, BL_CONTEXT_STYLE_SLOT_STROKE, bail);
  BLPoint originFixed = ctxI->finalTransformFixed().mapPoint(*origin);
  return strokeUnclippedPath<kRM>(ctxI, di, ds, originFixed, path->dcast());
}

template<RenderingMode kRM>
static BLResult BL_CDECL strokePathDRgba32Impl(BLContextImpl* baseImpl, const BLPoint* origin, const BLPathCore* path, uint32_t rgba32) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BL_ASSERT(path->_d.isPath());

  bool bail = path->dcast().empty();
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_SOLID_OP(ContextFlags::kNoStrokeOpExplicit, BL_CONTEXT_STYLE_SLOT_STROKE, rgba32, bail);
  BLPoint originFixed = ctxI->finalTransformFixed().mapPoint(*origin);
  return strokeUnclippedPath<kRM>(ctxI, di, ds, originFixed, path->dcast());
}

template<RenderingMode kRM>
static BLResult BL_CDECL strokePathDExtImpl(BLContextImpl* baseImpl, const BLPoint* origin, const BLPathCore* path, const BLObjectCore* style) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  BL_ASSERT(path->_d.isPath());

  bool bail = path->dcast().empty();
  BLResult bailResult = BL_SUCCESS;

  BL_CONTEXT_RESOLVE_EXPLICIT_STYLE_OP(ContextFlags::kNoStrokeOpExplicit, BL_CONTEXT_STYLE_SLOT_STROKE, style, bail);
  BLPoint originFixed = ctxI->finalTransformFixed().mapPoint(*origin);
  BLResult result = strokeUnclippedPath<kRM>(ctxI, di, ds, originFixed, path->dcast());

  return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), result);
}

// bl::RasterEngine - ContextImpl - Frontend - Stroke Text
// =======================================================

template<RenderingMode kRM>
static BLResult BL_CDECL strokeTextOpDImpl(BLContextImpl* baseImpl, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* opData) noexcept {
  BL_ASSERT(font->_d.isFont());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  bool bail = !font->dcast().isValid();
  BLResult bailResult = bail ? blTraceError(BL_ERROR_FONT_NOT_INITIALIZED) : BLResult(BL_SUCCESS);

  BL_CONTEXT_RESOLVE_IMPLICIT_STYLE_OP(ContextFlags::kNoStrokeOpImplicit, BL_CONTEXT_STYLE_SLOT_STROKE, bail);
  return strokeUnclippedText<kRM>(ctxI, di, ds, origin, font, opType, opData);
}

template<RenderingMode kRM>
static BLResult BL_CDECL strokeTextOpIImpl(BLContextImpl* baseImpl, const BLPointI* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* opData) noexcept {
  BL_ASSERT(font->_d.isFont());

  BLPoint originD(*origin);
  return strokeTextOpDImpl<kRM>(baseImpl, &originD, font, opType, opData);
}

template<RenderingMode kRM>
static BLResult BL_CDECL strokeTextOpDRgba32Impl(BLContextImpl* baseImpl, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* opData, uint32_t rgba32) noexcept {
  BL_ASSERT(font->_d.isFont());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  bool bail = !font->dcast().isValid();
  BLResult bailResult = bail ? blTraceError(BL_ERROR_FONT_NOT_INITIALIZED) : BLResult(BL_SUCCESS);

  BL_CONTEXT_RESOLVE_EXPLICIT_SOLID_OP(ContextFlags::kNoStrokeOpExplicit, BL_CONTEXT_STYLE_SLOT_STROKE, rgba32, bail);
  return strokeUnclippedText<kRM>(ctxI, di, ds, origin, font, opType, opData);
}

template<RenderingMode kRM>
static BLResult BL_CDECL strokeTextOpDExtImpl(BLContextImpl* baseImpl, const BLPoint* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* opData, const BLObjectCore* style) noexcept {
  BL_ASSERT(font->_d.isFont());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);

  bool bail = !font->dcast().isValid();
  BLResult bailResult = bail ? blTraceError(BL_ERROR_FONT_NOT_INITIALIZED) : BLResult(BL_SUCCESS);

  BL_CONTEXT_RESOLVE_EXPLICIT_STYLE_OP(ContextFlags::kNoStrokeOpExplicit, BL_CONTEXT_STYLE_SLOT_STROKE, style, bail);
  BLResult result = strokeUnclippedText<kRM>(ctxI, di, ds, origin, font, opType, opData);

  return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), result);
}

template<RenderingMode kRM>
static BLResult BL_CDECL strokeTextOpIRgba32Impl(BLContextImpl* baseImpl, const BLPointI* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* opData, uint32_t rgba32) noexcept {
  BL_ASSERT(font->_d.isFont());

  BLPoint originD(*origin);
  return strokeTextOpDRgba32Impl<kRM>(baseImpl, &originD, font, opType, opData, rgba32);
}

template<RenderingMode kRM>
static BLResult BL_CDECL strokeTextOpIExtImpl(BLContextImpl* baseImpl, const BLPointI* origin, const BLFontCore* font, BLContextRenderTextOp opType, const void* opData, const BLObjectCore* style) noexcept {
  BL_ASSERT(font->_d.isFont());

  BLPoint originD(*origin);
  return strokeTextOpDExtImpl<kRM>(baseImpl, &originD, font, opType, opData, style);
}

// bl::RasterEngine - ContextImpl - Frontend - Blit Image
// ======================================================

template<RenderingMode kRM>
static BLResult BL_CDECL blitImageDImpl(BLContextImpl* baseImpl, const BLPoint* origin, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BL_ASSERT(img->_d.isImage());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLImageImpl* imgI = ImageInternal::getImpl(img);

  BLPoint dst(*origin);
  BLRectI srcRect;

  BLResult bailResult = checkImageArea(srcRect, imgI, imgArea);
  bool bail = bailResult != BL_SUCCESS;
  BL_CONTEXT_RESOLVE_BLIT_OP(ContextFlags::kNoBlitFlags, imgI->format, bail);

  BLBox finalBox;

  if (BL_LIKELY(resolved.unmodified())) {
    uint32_t imgBytesPerPixel = imgI->depth / 8u;

    if BL_CONSTEXPR (kRM == RenderingMode::kAsync)
      fetchData->initStyleObjectAndDestroyFunc(img, destroyFetchDataImage);

    if (ctxI->finalTransformType() <= BL_TRANSFORM_TYPE_TRANSLATE) {
      double startX = dst.x * ctxI->finalTransformFixed().m00 + ctxI->finalTransformFixed().m20;
      double startY = dst.y * ctxI->finalTransformFixed().m11 + ctxI->finalTransformFixed().m21;

      double dx0 = blMax(startX, ctxI->finalClipBoxFixedD().x0);
      double dy0 = blMax(startY, ctxI->finalClipBoxFixedD().y0);
      double dx1 = blMin(startX + double(srcRect.w) * ctxI->finalTransformFixed().m00, ctxI->finalClipBoxFixedD().x1);
      double dy1 = blMin(startY + double(srcRect.h) * ctxI->finalTransformFixed().m11, ctxI->finalClipBoxFixedD().y1);

      // Clipped out, invalid coordinates, or empty `imgArea`.
      if (!(unsigned(dx0 < dx1) & unsigned(dy0 < dy1)))
        return BL_SUCCESS;

      int ix0 = Math::truncToInt(dx0);
      int iy0 = Math::truncToInt(dy0);
      int ix1 = Math::truncToInt(dx1);
      int iy1 = Math::truncToInt(dy1);

      // Clipped out - this is required as the difference between x0 & x1 and y0 & y1 could be smaller than our fixed point.
      if (!(unsigned(ix0 < ix1) & unsigned(iy0 < iy1)))
        return BL_SUCCESS;

      int64_t startFx = Math::floorToInt64(startX);
      int64_t startFy = Math::floorToInt64(startY);

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

        fetchData->initImageSource(imgI, srcRect);
        fetchData->setupPatternBlit(x0, y0);
      }
      else {
        fetchData->initImageSource(imgI, srcRect);
        fetchData->setupPatternFxFy(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, BLPatternQuality(ctxI->hints().patternQuality), imgBytesPerPixel, startFx, startFy);
      }

      prepareNonSolidFetch(ctxI, di, ds, fetchData.ptr());
      return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), fillClippedBoxF<kRM>(ctxI, di, ds, BLBoxI(ix0, iy0, ix1, iy1)));
    }

    BLMatrix2D ft(ctxI->finalTransform());
    ft.translate(dst.x, dst.y);

    fetchData->initImageSource(imgI, srcRect);
    if (!fetchData->setupPatternAffine(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, BLPatternQuality(ctxI->hints().patternQuality), imgBytesPerPixel, ft))
      return BL_SUCCESS;

    prepareNonSolidFetch(ctxI, di, ds, fetchData.ptr());
    finalBox = BLBox(dst.x, dst.y, dst.x + double(srcRect.w), dst.y + double(srcRect.h));
  }
  else {
    prepareOverriddenFetch(ctxI, di, ds, CompOpSolidId(resolved.flags));
    finalBox = BLBox(dst.x, dst.y, dst.x + double(srcRect.w), dst.y + double(srcRect.h));
  }

  return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), fillUnclippedBoxD<kRM>(ctxI, di, ds, finalBox));
}

template<RenderingMode kRM>
static BLResult BL_CDECL blitImageIImpl(BLContextImpl* baseImpl, const BLPointI* origin, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BL_ASSERT(img->_d.isImage());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLImageImpl* imgI = ImageInternal::getImpl(img);

  if (!blTestFlag(ctxI->contextFlags, ContextFlags::kInfoIntegralTranslation)) {
    BLPoint originD(*origin);
    return blitImageDImpl<kRM>(ctxI, &originD, img, imgArea);
  }

  BLBoxI dstBox;
  BLPointI srcOffset;

  BLResult bailResult;
  bool bail = !translateAndClipRectToBlitI(ctxI, origin, imgArea, &imgI->size, &bailResult, &dstBox, &srcOffset);

  BL_CONTEXT_RESOLVE_BLIT_OP(ContextFlags::kNoBlitFlags, imgI->format, bail);

  if (BL_LIKELY(resolved.unmodified())) {
    if BL_CONSTEXPR (kRM == RenderingMode::kAsync)
      fetchData->initStyleObjectAndDestroyFunc(img, destroyFetchDataImage);

    fetchData->initImageSource(imgI, BLRectI(srcOffset.x, srcOffset.y, dstBox.x1 - dstBox.x0, dstBox.y1 - dstBox.y0));
    fetchData->setupPatternBlit(dstBox.x0, dstBox.y0);

    prepareNonSolidFetch(ctxI, di, ds, fetchData.ptr());
  }
  else {
    prepareOverriddenFetch(ctxI, di, ds, CompOpSolidId(resolved.flags));
  }

  return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), fillClippedBoxA<kRM>(ctxI, di, ds, dstBox));
}

// bl::RasterEngine - ContextImpl - Frontend - Blit Scaled Image
// =============================================================

template<RenderingMode kRM>
static BLResult BL_CDECL blitScaledImageDImpl(BLContextImpl* baseImpl, const BLRect* rect, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BL_ASSERT(img->_d.isImage());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLImageImpl* imgI = ImageInternal::getImpl(img);

  BLRectI srcRect;
  BL_PROPAGATE(checkImageArea(srcRect, imgI, imgArea));

  // OPTIMIZATION: Don't go over all the transformations if the destination and source rects have the same size.
  if (unsigned(rect->w == double(srcRect.w)) & unsigned(rect->h == double(srcRect.h)))
    return blitImageDImpl<kRM>(ctxI, reinterpret_cast<const BLPoint*>(rect), img, imgArea);

  BLResult bailResult = BL_SUCCESS;
  BL_CONTEXT_RESOLVE_BLIT_OP(ContextFlags::kNoBlitFlags, imgI->format, kNoBail);

  BLBox finalBox(rect->x, rect->y, rect->x + rect->w, rect->y + rect->h);
  if (BL_LIKELY(resolved.unmodified())) {
    if BL_CONSTEXPR (kRM == RenderingMode::kAsync)
      fetchData->initStyleObjectAndDestroyFunc(img, destroyFetchDataImage);

    BLMatrix2D ft(rect->w / double(srcRect.w), 0.0, 0.0, rect->h / double(srcRect.h), rect->x, rect->y);
    TransformInternal::multiply(ft, ft, ctxI->finalTransform());

    uint32_t imgBytesPerPixel = imgI->depth / 8u;
    fetchData->initImageSource(imgI, srcRect);

    if (!fetchData->setupPatternAffine(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, BLPatternQuality(ctxI->hints().patternQuality), imgBytesPerPixel, ft))
      return BL_SUCCESS;

    prepareNonSolidFetch(ctxI, di, ds, fetchData.ptr());
  }
  else {
    prepareOverriddenFetch(ctxI, di, ds, CompOpSolidId(resolved.flags));
  }

  return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), fillUnclippedBoxD<kRM>(ctxI, di, ds, finalBox));
}

template<RenderingMode kRM>
static BLResult BL_CDECL blitScaledImageIImpl(BLContextImpl* baseImpl, const BLRectI* rect, const BLImageCore* img, const BLRectI* imgArea) noexcept {
  BL_ASSERT(img->_d.isImage());

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(baseImpl);
  BLImageImpl* imgI = ImageInternal::getImpl(img);

  BLRectI srcRect;
  BL_PROPAGATE(checkImageArea(srcRect, imgI, imgArea));

  // OPTIMIZATION: Don't go over all the transformations if the destination and source rects have the same size.
  if (rect->w == srcRect.w && rect->h == srcRect.h)
    return blitImageIImpl<kRM>(ctxI, reinterpret_cast<const BLPointI*>(rect), img, imgArea);

  BLResult bailResult = BL_SUCCESS;
  BL_CONTEXT_RESOLVE_BLIT_OP(ContextFlags::kNoBlitFlags, imgI->format, kNoBail);

  BLBox finalBox(double(rect->x), double(rect->y), double(rect->x) + double(rect->w), double(rect->y) + double(rect->h));
  if (BL_LIKELY(resolved.unmodified())) {
    if BL_CONSTEXPR (kRM == RenderingMode::kAsync)
      fetchData->initStyleObjectAndDestroyFunc(img, destroyFetchDataImage);

    BLMatrix2D transform(double(rect->w) / double(srcRect.w), 0.0, 0.0, double(rect->h) / double(srcRect.h), double(rect->x), double(rect->y));
    TransformInternal::multiply(transform, transform, ctxI->finalTransform());

    uint32_t imgBytesPerPixel = imgI->depth / 8u;
    fetchData->initImageSource(imgI, srcRect);
    if (!fetchData->setupPatternAffine(BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND, BLPatternQuality(ctxI->hints().patternQuality), imgBytesPerPixel, transform))
      return BL_SUCCESS;

    prepareNonSolidFetch(ctxI, di, ds, fetchData.ptr());
  }
  else {
    prepareOverriddenFetch(ctxI, di, ds, CompOpSolidId(resolved.flags));
  }

  return finalizeExplicitOp<kRM>(ctxI, fetchData.ptr(), fillUnclippedBoxD<kRM>(ctxI, di, ds, finalBox));
}

// bl::RasterEngine - ContextImpl - Attach & Detach
// ================================================

static BL_INLINE uint32_t calculateBandHeight(uint32_t format, const BLSizeI& size, const BLContextCreateInfo* options) noexcept {
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
    uint32_t bandHeightShift = IntOps::ctz(bandHeight);
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

static BL_INLINE uint32_t calculateCommandQuantizationShift(uint32_t bandHeight, uint32_t bandCount) noexcept {
  uint32_t bandQuantization = IntOps::ctz(bandHeight);
  uint32_t coordinateQuantization = blMax<uint32_t>(32 - IntOps::clz(bandHeight * bandCount), 8) - 8u;

  // We should never quantize to less than a band height.
  return blMax(bandQuantization, coordinateQuantization);
}

static BL_INLINE size_t calculateZeroedMemorySize(uint32_t width, uint32_t height) noexcept {
  size_t alignedWidth = IntOps::alignUp(size_t(width) + 1u + BL_PIPE_PIXELS_PER_ONE_BIT, 16);

  size_t bitStride = IntOps::wordCountFromBitCount<BLBitWord>(alignedWidth / BL_PIPE_PIXELS_PER_ONE_BIT) * sizeof(BLBitWord);
  size_t cellStride = alignedWidth * sizeof(uint32_t);

  size_t minimumSize = bitStride * size_t(height) + cellStride * size_t(height);
  return IntOps::alignUp(minimumSize + sizeof(BLBitWord) * 16, BL_CACHE_LINE_SIZE);
}

static BLResult attach(BLRasterContextImpl* ctxI, BLImageCore* image, const BLContextCreateInfo* options) noexcept {
  BL_ASSERT(image != nullptr);
  BL_ASSERT(options != nullptr);

  uint32_t format = ImageInternal::getImpl(image)->format;
  BLSizeI size = ImageInternal::getImpl(image)->size;

  // TODO: [Rendering Context] Hardcoded for 8bpc.
  uint32_t targetComponentType = RenderTargetInfo::kPixelComponentUInt8;

  uint32_t bandHeight = calculateBandHeight(format, size, options);
  uint32_t bandCount = (uint32_t(size.h) + bandHeight - 1) >> IntOps::ctz(bandHeight);
  uint32_t commandQuantizationShift = calculateCommandQuantizationShift(bandHeight, bandCount);

  size_t zeroedMemorySize = calculateZeroedMemorySize(uint32_t(size.w), bandHeight);

  // Initialization.
  BLResult result = BL_SUCCESS;
  Pipeline::PipeRuntime* pipeRuntime = nullptr;

  // If anything fails we would restore the zone state to match this point.
  ArenaAllocator& baseZone = ctxI->baseZone;
  ArenaAllocator::StatePtr zoneState = baseZone.saveState();

  // Not a real loop, just a scope we can escape early via 'break'.
  do {
    // Step 1: Initialize edge storage of the sync worker.
    result = ctxI->syncWorkData.initBandData(bandHeight, bandCount, commandQuantizationShift);
    if (result != BL_SUCCESS)
      break;

    // Step 2: Initialize the thread manager if multi-threaded rendering is enabled.
    if (options->threadCount) {
      ctxI->ensureWorkerMgr();
      result = ctxI->workerMgr->init(ctxI, options);

      if (result != BL_SUCCESS)
        break;

      if (ctxI->workerMgr->isActive())
        ctxI->renderingMode = uint8_t(RenderingMode::kAsync);
    }

    // Step 3: Initialize pipeline runtime (JIT or fixed).
#if !defined(BL_BUILD_NO_JIT)
    if (!(options->flags & BL_CONTEXT_CREATE_FLAG_DISABLE_JIT)) {
      pipeRuntime = &Pipeline::JIT::PipeDynamicRuntime::_global;

      if (options->flags & BL_CONTEXT_CREATE_FLAG_ISOLATED_JIT_RUNTIME) {
        // Create an isolated `BLPipeGenRuntime` if specified. It will be used to store all functions
        // generated during the rendering and will be destroyed together with the context.
        Pipeline::JIT::PipeDynamicRuntime* isolatedRT =
          baseZone.newT<Pipeline::JIT::PipeDynamicRuntime>(Pipeline::PipeRuntimeFlags::kIsolated);

        // This should not really happen as the first block is allocated with the impl.
        if (BL_UNLIKELY(!isolatedRT)) {
          result = blTraceError(BL_ERROR_OUT_OF_MEMORY);
          break;
        }

        // Enable logger if required.
        if (options->flags & BL_CONTEXT_CREATE_FLAG_ISOLATED_JIT_LOGGING) {
          isolatedRT->setLoggerEnabled(true);
        }

        // Feature restrictions are related to JIT compiler - it allows us to test the code generated by JIT
        // with less features than the current CPU has, to make sure that we support older hardware or to
        // compare between implementations.
        if (options->flags & BL_CONTEXT_CREATE_FLAG_OVERRIDE_CPU_FEATURES) {
          isolatedRT->_restrictFeatures(options->cpuFeatures);
        }

        pipeRuntime = isolatedRT;
        baseZone.align(baseZone.blockAlignment());
      }
    }
#endif

    if (!pipeRuntime) {
      pipeRuntime = &Pipeline::PipeStaticRuntime::_global;
    }

    // Step 4: Allocate zeroed memory for the user thread and all worker threads.
    result = ctxI->syncWorkData.zeroBuffer.ensure(zeroedMemorySize);
    if (result != BL_SUCCESS)
      break;

    if (!ctxI->isSync()) {
      result = ctxI->workerMgr->initWorkMemory(zeroedMemorySize);
      if (result != BL_SUCCESS)
        break;
    }

    // Step 5: Make the destination image mutable.
    result = blImageMakeMutable(image, &ctxI->dstData);
    if (result != BL_SUCCESS)
      break;
  } while (0);

  // Handle a possible initialization failure.
  if (result != BL_SUCCESS) {
    // Switch back to a synchronous rendering mode if asynchronous rendering was already setup.
    // We have already acquired worker threads that must be released now.
    if (ctxI->renderingMode == uint8_t(RenderingMode::kAsync)) {
      ctxI->workerMgr->reset();
      ctxI->renderingMode = uint8_t(RenderingMode::kSync);
    }

    // If we failed we don't want the pipeline runtime associated with the
    // context so we simply destroy it and pretend like nothing happened.
    if (pipeRuntime) {
      if (blTestFlag(pipeRuntime->runtimeFlags(), Pipeline::PipeRuntimeFlags::kIsolated))
        pipeRuntime->destroy();
    }

    baseZone.restoreState(zoneState);
    return result;
  }

  ctxI->contextFlags = ContextFlags::kInfoIntegralTranslation;

  if (!ctxI->isSync()) {
    ctxI->virt = &rasterImplVirtAsync;
    ctxI->syncWorkData.synchronization = &ctxI->workerMgr->_synchronization;
  }

  // Increase `writerCount` of the image, will be decreased by `detach()`.
  BLImagePrivateImpl* imageI = ImageInternal::getImpl(image);
  blAtomicFetchAddRelaxed(&imageI->writerCount);
  ctxI->dstImage._d = image->_d;

  // Initialize the pipeline runtime and pipeline lookup cache.
  ctxI->pipeProvider.init(pipeRuntime);
  ctxI->pipeLookupCache.reset();

  // Initialize the sync work data.
  ctxI->syncWorkData.initContextData(ctxI->dstData, options->pixelOrigin);

  // Initialize destination image information available in a public rendering context state.
  ctxI->internalState.targetSize.reset(size.w, size.h);
  ctxI->internalState.targetImage = &ctxI->dstImage;

  // Initialize members that are related to target precision.
  ctxI->renderTargetInfo = renderTargetInfoByComponentType[targetComponentType];
  ctxI->fpMinSafeCoordD = Math::floor(double(Traits::minValue<int32_t>() + 1) * ctxI->fpScaleD());
  ctxI->fpMaxSafeCoordD = Math::floor(double(Traits::maxValue<int32_t>() - 1 - blMax(size.w, size.h)) * ctxI->fpScaleD());

  // Initialize members that are related to alpha blending and composition.
  ctxI->solidFormatTable[BL_RASTER_CONTEXT_SOLID_FORMAT_ARGB] = uint8_t(FormatExt::kPRGB32);
  ctxI->solidFormatTable[BL_RASTER_CONTEXT_SOLID_FORMAT_FRGB] = uint8_t(FormatExt::kFRGB32);
  ctxI->solidFormatTable[BL_RASTER_CONTEXT_SOLID_FORMAT_ZERO] = uint8_t(FormatExt::kZERO32);

  // Const-casted, because this would replace fetchData, which is non-const, but guaranteed to not modify solid styles.
  RenderFetchDataSolid* solidOverrideFillTable =
    targetComponentType == RenderTargetInfo::kPixelComponentUInt8
      ? (RenderFetchDataSolid*)solidOverrideFillU8
      : (RenderFetchDataSolid*)solidOverrideFillU16;

  ctxI->solidOverrideFillTable = solidOverrideFillTable;
  ctxI->solidFetchDataOverrideTable[size_t(CompOpSolidId::kNone       )] = nullptr;
  ctxI->solidFetchDataOverrideTable[size_t(CompOpSolidId::kTransparent)] = &solidOverrideFillTable[size_t(CompOpSolidId::kTransparent)];
  ctxI->solidFetchDataOverrideTable[size_t(CompOpSolidId::kOpaqueBlack)] = &solidOverrideFillTable[size_t(CompOpSolidId::kOpaqueBlack)];
  ctxI->solidFetchDataOverrideTable[size_t(CompOpSolidId::kOpaqueWhite)] = &solidOverrideFillTable[size_t(CompOpSolidId::kOpaqueWhite)];
  ctxI->solidFetchDataOverrideTable[size_t(CompOpSolidId::kAlwaysNop  )] = &solidOverrideFillTable[size_t(CompOpSolidId::kAlwaysNop  )];

  // Initialize the rendering state to defaults.
  ctxI->stateIdCounter = 0;
  ctxI->savedState = nullptr;
  ctxI->sharedFillState = nullptr;
  ctxI->sharedStrokeState = nullptr;

  // Initialize public state.
  ctxI->internalState.hints.reset();
  ctxI->internalState.hints.patternQuality = BL_PATTERN_QUALITY_BILINEAR;
  ctxI->internalState.compOp = uint8_t(BL_COMP_OP_SRC_OVER);
  ctxI->internalState.fillRule = uint8_t(BL_FILL_RULE_NON_ZERO);
  ctxI->internalState.styleType[BL_CONTEXT_STYLE_SLOT_FILL] = uint8_t(BL_OBJECT_TYPE_RGBA);
  ctxI->internalState.styleType[BL_CONTEXT_STYLE_SLOT_STROKE] = uint8_t(BL_OBJECT_TYPE_RGBA);
  ctxI->internalState.savedStateCount = 0;
  ctxI->internalState.approximationOptions = PathInternal::makeDefaultApproximationOptions();
  ctxI->internalState.globalAlpha = 1.0;
  ctxI->internalState.styleAlpha[0] = 1.0;
  ctxI->internalState.styleAlpha[1] = 1.0;
  ctxI->internalState.styleAlphaI[0] = uint32_t(ctxI->renderTargetInfo.fullAlphaI);
  ctxI->internalState.styleAlphaI[1] = uint32_t(ctxI->renderTargetInfo.fullAlphaI);
  blCallCtor(ctxI->internalState.strokeOptions.dcast());
  ctxI->internalState.metaTransform.reset();
  ctxI->internalState.userTransform.reset();

  // Initialize private state.
  ctxI->internalState.finalTransformFixedType = BL_TRANSFORM_TYPE_SCALE;
  ctxI->internalState.metaTransformFixedType = BL_TRANSFORM_TYPE_SCALE;
  ctxI->internalState.metaTransformType = BL_TRANSFORM_TYPE_TRANSLATE;
  ctxI->internalState.finalTransformType = BL_TRANSFORM_TYPE_TRANSLATE;
  ctxI->internalState.identityTransformType = BL_TRANSFORM_TYPE_IDENTITY;
  ctxI->internalState.globalAlphaI = uint32_t(ctxI->renderTargetInfo.fullAlphaI);

  ctxI->internalState.finalTransform.reset();
  ctxI->internalState.metaTransformFixed.resetToScaling(ctxI->renderTargetInfo.fpScaleD);
  ctxI->internalState.finalTransformFixed.resetToScaling(ctxI->renderTargetInfo.fpScaleD);
  ctxI->internalState.translationI.reset(0, 0);

  ctxI->internalState.metaClipBoxI.reset(0, 0, size.w, size.h);
  // `finalClipBoxI` and `finalClipBoxD` are initialized by `resetClippingToMetaClipBox()`.

  if (options->savedStateLimit)
    ctxI->savedStateLimit = options->savedStateLimit;
  else
    ctxI->savedStateLimit = BL_RASTER_CONTEXT_DEFAULT_SAVED_STATE_LIMIT;

  // Make sure the state is initialized properly.
  onAfterCompOpChanged(ctxI);
  onAfterFlattenToleranceChanged(ctxI);
  onAfterOffsetParameterChanged(ctxI);
  resetClippingToMetaClipBox(ctxI);

  // Initialize styles.
  initStyleToDefault(ctxI, BL_CONTEXT_STYLE_SLOT_FILL);
  initStyleToDefault(ctxI, BL_CONTEXT_STYLE_SLOT_STROKE);

  return BL_SUCCESS;
}

static BLResult detach(BLRasterContextImpl* ctxI) noexcept {
  // Release the ImageImpl.
  BLImagePrivateImpl* imageI = ImageInternal::getImpl(&ctxI->dstImage);
  BL_ASSERT(imageI != nullptr);

  flushImpl(ctxI, BL_CONTEXT_FLUSH_SYNC);

  // Release Threads/WorkerContexts used by asynchronous rendering.
  if (ctxI->workerMgrInitialized)
    ctxI->workerMgr->reset();

  // Release PipeRuntime.
  if (blTestFlag(ctxI->pipeProvider.runtime()->runtimeFlags(), Pipeline::PipeRuntimeFlags::kIsolated))
    ctxI->pipeProvider.runtime()->destroy();
  ctxI->pipeProvider.reset();

  // Release all states.
  //
  // Important as the user doesn't have to restore all states, in that case we basically need to iterate
  // over all of them and release resources they hold.
  discardStates(ctxI, nullptr);
  blCallDtor(ctxI->internalState.strokeOptions);

  ContextFlags contextFlags = ctxI->contextFlags;
  if (blTestFlag(contextFlags, ContextFlags::kFetchDataFill))
    destroyValidStyle(ctxI, &ctxI->internalState.style[BL_CONTEXT_STYLE_SLOT_FILL]);

  if (blTestFlag(contextFlags, ContextFlags::kFetchDataStroke))
    destroyValidStyle(ctxI, &ctxI->internalState.style[BL_CONTEXT_STYLE_SLOT_STROKE]);

  // Clear other important members. We don't have to clear everything as if we re-attach an image again
  // all members will be overwritten anyway.
  ctxI->contextFlags = ContextFlags::kNoFlagsSet;

  ctxI->baseZone.clear();
  ctxI->fetchDataPool.reset();
  ctxI->savedStatePool.reset();
  ctxI->syncWorkData.ctxData.reset();
  ctxI->syncWorkData.workZone.clear();

  // If the image was dereferenced during rendering it's our responsibility to destroy it. This is not useful
  // from the consumer's perspective as the resulting image can never be used again, but it can happen in some
  // cases (for example when an asynchronous rendering is terminated and the target image released with it).
  if (blAtomicFetchSubStrong(&imageI->writerCount) == 1)
    if (ObjectInternal::getImplRefCount(imageI) == 0)
      ImageInternal::freeImpl(imageI);

  ctxI->dstImage._d.impl = nullptr;
  ctxI->dstData.reset();

  return BL_SUCCESS;
}

// bl::RasterEngine - ContextImpl - Destroy
// ========================================

static BLResult BL_CDECL destroyImpl(BLObjectImpl* impl) noexcept {
  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(impl);

  if (ctxI->dstImage._d.impl)
    detach(ctxI);

  ctxI->~BLRasterContextImpl();
  return blObjectFreeImpl(ctxI);
}

// bl::RasterEngine - ContextImpl - Virtual Function Table
// =======================================================

template<RenderingMode kRM>
static void initVirt(BLContextVirt* virt) noexcept {
  virt->base.destroy             = destroyImpl;
  virt->base.getProperty         = getPropertyImpl;
  virt->base.setProperty         = setPropertyImpl;
  virt->flush                    = flushImpl;

  virt->save                     = saveImpl;
  virt->restore                  = restoreImpl;

  virt->applyTransformOp         = applyTransformOpImpl;
  virt->userToMeta               = userToMetaImpl;

  virt->setHint                  = setHintImpl;
  virt->setHints                 = setHintsImpl;

  virt->setFlattenMode           = setFlattenModeImpl;
  virt->setFlattenTolerance      = setFlattenToleranceImpl;
  virt->setApproximationOptions  = setApproximationOptionsImpl;

  virt->getStyle                 = getStyleImpl;
  virt->setStyle                 = setStyleImpl;
  virt->disableStyle             = disableStyleImpl;
  virt->setStyleRgba             = setStyleRgbaImpl;
  virt->setStyleRgba32           = setStyleRgba32Impl;
  virt->setStyleRgba64           = setStyleRgba64Impl;
  virt->setStyleAlpha            = setStyleAlphaImpl;
  virt->swapStyles               = swapStylesImpl;

  virt->setGlobalAlpha           = setGlobalAlphaImpl;
  virt->setCompOp                = setCompOpImpl;

  virt->setFillRule              = setFillRuleImpl;
  virt->setStrokeWidth           = setStrokeWidthImpl;
  virt->setStrokeMiterLimit      = setStrokeMiterLimitImpl;
  virt->setStrokeCap             = setStrokeCapImpl;
  virt->setStrokeCaps            = setStrokeCapsImpl;
  virt->setStrokeJoin            = setStrokeJoinImpl;
  virt->setStrokeTransformOrder  = setStrokeTransformOrderImpl;
  virt->setStrokeDashOffset      = setStrokeDashOffsetImpl;
  virt->setStrokeDashArray       = setStrokeDashArrayImpl;
  virt->setStrokeOptions         = setStrokeOptionsImpl;

  virt->clipToRectI              = clipToRectIImpl;
  virt->clipToRectD              = clipToRectDImpl;
  virt->restoreClipping          = restoreClippingImpl;

  virt->clearAll                 = clearAllImpl<kRM>;
  virt->clearRectI               = clearRectIImpl<kRM>;
  virt->clearRectD               = clearRectDImpl<kRM>;

  virt->fillAll                  = fillAllImpl<kRM>;
  virt->fillAllRgba32            = fillAllRgba32Impl<kRM>;
  virt->fillAllExt               = fillAllExtImpl<kRM>;

  virt->fillRectI                = fillRectIImpl<kRM>;
  virt->fillRectIRgba32          = fillRectIRgba32Impl<kRM>;
  virt->fillRectIExt             = fillRectIExtImpl<kRM>;

  virt->fillRectD                = fillRectDImpl<kRM>;
  virt->fillRectDRgba32          = fillRectDRgba32Impl<kRM>;
  virt->fillRectDExt             = fillRectDExtImpl<kRM>;

  virt->fillPathD                = fillPathDImpl<kRM>;
  virt->fillPathDRgba32          = fillPathDRgba32Impl<kRM>;
  virt->fillPathDExt             = fillPathDExtImpl<kRM>;

  virt->fillGeometry             = fillGeometryImpl<kRM>;
  virt->fillGeometryRgba32       = fillGeometryRgba32Impl<kRM>;
  virt->fillGeometryExt          = fillGeometryExtImpl<kRM>;

  virt->fillTextOpI              = fillTextOpIImpl<kRM>;
  virt->fillTextOpIRgba32        = fillTextOpIRgba32Impl<kRM>;
  virt->fillTextOpIExt           = fillTextOpIExtImpl<kRM>;

  virt->fillTextOpD              = fillTextOpDImpl<kRM>;
  virt->fillTextOpDRgba32        = fillTextOpDRgba32Impl<kRM>;
  virt->fillTextOpDExt           = fillTextOpDExtImpl<kRM>;

  virt->fillMaskI                = fillMaskIImpl<kRM>;
  virt->fillMaskIRgba32          = fillMaskIRgba32Impl<kRM>;
  virt->fillMaskIExt             = fillMaskIExtImpl<kRM>;

  virt->fillMaskD                = fillMaskDImpl<kRM>;
  virt->fillMaskDRgba32          = fillMaskDRgba32Impl<kRM>;
  virt->fillMaskDExt             = fillMaskDExtImpl<kRM>;

  virt->strokePathD              = strokePathDImpl<kRM>;
  virt->strokePathDRgba32        = strokePathDRgba32Impl<kRM>;
  virt->strokePathDExt           = strokePathDExtImpl<kRM>;

  virt->strokeGeometry           = strokeGeometryImpl<kRM>;
  virt->strokeGeometryRgba32     = strokeGeometryRgba32Impl<kRM>;
  virt->strokeGeometryExt        = strokeGeometryExtImpl<kRM>;

  virt->strokeTextOpI            = strokeTextOpIImpl<kRM>;
  virt->strokeTextOpIRgba32      = strokeTextOpIRgba32Impl<kRM>;
  virt->strokeTextOpIExt         = strokeTextOpIExtImpl<kRM>;

  virt->strokeTextOpD            = strokeTextOpDImpl<kRM>;
  virt->strokeTextOpDRgba32      = strokeTextOpDRgba32Impl<kRM>;
  virt->strokeTextOpDExt         = strokeTextOpDExtImpl<kRM>;

  virt->blitImageI               = blitImageIImpl<kRM>;
  virt->blitImageD               = blitImageDImpl<kRM>;

  virt->blitScaledImageI         = blitScaledImageIImpl<kRM>;
  virt->blitScaledImageD         = blitScaledImageDImpl<kRM>;
}

} // {RasterEngine}
} // {bl}

// bl::RasterEngine - ContextImpl - Runtime Registration
// =====================================================

BLResult blRasterContextInitImpl(BLContextCore* self, BLImageCore* image, const BLContextCreateInfo* options) noexcept {
  // NOTE: Initially static data was part of `BLRasterContextImpl`, however, that doesn't work with MSAN
  // as it would consider it destroyed when `bl::ArenaAllocator` iterates that block during destruction.
  constexpr size_t kStaticDataSize = 2048;
  constexpr size_t kContextImplSize = sizeof(BLRasterContextImpl) + kStaticDataSize;

  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_CONTEXT);
  BL_PROPAGATE(bl::ObjectInternal::allocImplAlignedT<BLRasterContextImpl>(self, info, BLObjectImplSize{kContextImplSize}, 64));

  BLRasterContextImpl* ctxI = static_cast<BLRasterContextImpl*>(self->_d.impl);
  void* staticData = static_cast<void*>(reinterpret_cast<uint8_t*>(self->_d.impl) + sizeof(BLRasterContextImpl));

  blCallCtor(*ctxI, &bl::RasterEngine::rasterImplVirtSync, staticData, kStaticDataSize);
  BLResult result = bl::RasterEngine::attach(ctxI, image, options);

  if (result != BL_SUCCESS)
    ctxI->virt->base.destroy(ctxI);

  return result;
}

void blRasterContextOnInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  bl::RasterEngine::initVirt<bl::RasterEngine::RenderingMode::kSync>(&bl::RasterEngine::rasterImplVirtSync);
  bl::RasterEngine::initVirt<bl::RasterEngine::RenderingMode::kAsync>(&bl::RasterEngine::rasterImplVirtAsync);
}
