// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#include "../../pipeline/reference/compopgeneric_p.h"
#include "../../pipeline/reference/fillgeneric_p.h"
#include "../../pipeline/reference/fixedpiperuntime_p.h"
#include "../../support/wrap_p.h"

namespace bl {
namespace Pipeline {

// FixedPipelineRuntime - Globals
// ==============================

Wrap<PipeStaticRuntime> PipeStaticRuntime::_global;

// FixedPipelineRuntime - Get
// ==========================

template<typename CompOp>
static BL_INLINE FillFunc getFillFunc(FillType fillType) noexcept {
  switch (fillType) {
    case FillType::kBoxA: return Reference::FillBoxA_Base<CompOp>::fillFunc;
    case FillType::kMask: return Reference::FillMask_Base<CompOp>::fillFunc;
    case FillType::kAnalytic: return Reference::FillAnalytic_Base<CompOp>::fillFunc;
    default:
      return nullptr;
  }
}

template<typename CompOp, uint32_t kDstBPP>
static BL_INLINE FillFunc getFillFuncEx(Signature s) noexcept {
  using PixelType = typename CompOp::PixelType;

  FillType fillType = s.fillType();
  FormatExt srcFormat = s.srcFormat();

  switch (s.fetchType()) {
    case FetchType::kSolid:
      return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchSolid<PixelType>, kDstBPP>>(fillType);

    case FetchType::kPatternAlignedBlit:
      switch (srcFormat) {
        case FormatExt::kPRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAlignedBlit<PixelType, FormatExt::kPRGB32>, kDstBPP>>(fillType);
        case FormatExt::kXRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAlignedBlit<PixelType, FormatExt::kXRGB32>, kDstBPP>>(fillType);
        case FormatExt::kA8    : return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAlignedBlit<PixelType, FormatExt::kA8    >, kDstBPP>>(fillType);
        default: return nullptr;
      }

    case FetchType::kPatternAlignedPad:
      switch (srcFormat) {
        case FormatExt::kPRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAlignedPad<PixelType, FormatExt::kPRGB32>, kDstBPP>>(fillType);
        case FormatExt::kXRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAlignedPad<PixelType, FormatExt::kXRGB32>, kDstBPP>>(fillType);
        case FormatExt::kA8    : return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAlignedPad<PixelType, FormatExt::kA8    >, kDstBPP>>(fillType);
        default: return nullptr;
      }

    case FetchType::kPatternAlignedRepeat:
      switch (srcFormat) {
        case FormatExt::kPRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAlignedRepeat<PixelType, FormatExt::kPRGB32>, kDstBPP>>(fillType);
        case FormatExt::kXRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAlignedRepeat<PixelType, FormatExt::kXRGB32>, kDstBPP>>(fillType);
        case FormatExt::kA8    : return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAlignedRepeat<PixelType, FormatExt::kA8    >, kDstBPP>>(fillType);
        default: return nullptr;
      }

    case FetchType::kPatternAlignedRoR:
      switch (srcFormat) {
        case FormatExt::kPRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAlignedRoR<PixelType, FormatExt::kPRGB32>, kDstBPP>>(fillType);
        case FormatExt::kXRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAlignedRoR<PixelType, FormatExt::kXRGB32>, kDstBPP>>(fillType);
        case FormatExt::kA8    : return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAlignedRoR<PixelType, FormatExt::kA8    >, kDstBPP>>(fillType);
        default: return nullptr;
      }

    case FetchType::kPatternFxPad:
    case FetchType::kPatternFyPad:
    case FetchType::kPatternFxFyPad:
      switch (srcFormat) {
        case FormatExt::kPRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternFxFyPad<PixelType, FormatExt::kPRGB32>, kDstBPP>>(fillType);
        case FormatExt::kXRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternFxFyPad<PixelType, FormatExt::kXRGB32>, kDstBPP>>(fillType);
        case FormatExt::kA8    : return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternFxFyPad<PixelType, FormatExt::kA8    >, kDstBPP>>(fillType);
        default: return nullptr;
      }

    case FetchType::kPatternFxRoR:
    case FetchType::kPatternFyRoR:
    case FetchType::kPatternFxFyRoR:
      switch (srcFormat) {
        case FormatExt::kPRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternFxFyRoR<PixelType, FormatExt::kPRGB32>, kDstBPP>>(fillType);
        case FormatExt::kXRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternFxFyRoR<PixelType, FormatExt::kXRGB32>, kDstBPP>>(fillType);
        case FormatExt::kA8    : return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternFxFyRoR<PixelType, FormatExt::kA8    >, kDstBPP>>(fillType);
        default: return nullptr;
      }

    case FetchType::kPatternAffineNNAny:
    case FetchType::kPatternAffineNNOpt:
      switch (srcFormat) {
        case FormatExt::kPRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAffineNNAny<PixelType, FormatExt::kPRGB32>, kDstBPP>>(fillType);
        case FormatExt::kXRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAffineNNAny<PixelType, FormatExt::kXRGB32>, kDstBPP>>(fillType);
        case FormatExt::kA8    : return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAffineNNAny<PixelType, FormatExt::kA8    >, kDstBPP>>(fillType);
        default: return nullptr;
      }

    case FetchType::kPatternAffineBIAny:
    case FetchType::kPatternAffineBIOpt:
      switch (srcFormat) {
        case FormatExt::kPRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAffineBIAny<PixelType, FormatExt::kPRGB32>, kDstBPP>>(fillType);
        case FormatExt::kXRGB32: return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAffineBIAny<PixelType, FormatExt::kXRGB32>, kDstBPP>>(fillType);
        case FormatExt::kA8    : return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchPatternAffineBIAny<PixelType, FormatExt::kA8    >, kDstBPP>>(fillType);
        default: return nullptr;
      }

    case FetchType::kGradientLinearNNPad:
      return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchLinearGradient<PixelType, BL_GRADIENT_QUALITY_NEAREST, true >, kDstBPP>>(fillType);

    case FetchType::kGradientLinearNNRoR:
      return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchLinearGradient<PixelType, BL_GRADIENT_QUALITY_NEAREST, false>, kDstBPP>>(fillType);

    case FetchType::kGradientLinearDitherPad:
      return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchLinearGradient<PixelType, BL_GRADIENT_QUALITY_DITHER , true >, kDstBPP>>(fillType);

    case FetchType::kGradientLinearDitherRoR:
      return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchLinearGradient<PixelType, BL_GRADIENT_QUALITY_DITHER , false>, kDstBPP>>(fillType);

    case FetchType::kGradientRadialNNPad:
      return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchRadialGradient<PixelType, BL_GRADIENT_QUALITY_NEAREST, true >, kDstBPP>>(fillType);

    case FetchType::kGradientRadialNNRoR:
      return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchRadialGradient<PixelType, BL_GRADIENT_QUALITY_NEAREST, false>, kDstBPP>>(fillType);

    case FetchType::kGradientRadialDitherPad:
      return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchRadialGradient<PixelType, BL_GRADIENT_QUALITY_DITHER , true >, kDstBPP>>(fillType);

    case FetchType::kGradientRadialDitherRoR:
      return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchRadialGradient<PixelType, BL_GRADIENT_QUALITY_DITHER , false>, kDstBPP>>(fillType);

    case FetchType::kGradientConicNN:
      return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchConicGradient<PixelType, BL_GRADIENT_QUALITY_NEAREST>, kDstBPP>>(fillType);

    case FetchType::kGradientConicDither:
      return getFillFunc<Reference::CompOp_Base_PRGB32<CompOp, PixelType, Reference::FetchConicGradient<PixelType, BL_GRADIENT_QUALITY_DITHER>, kDstBPP>>(fillType);

    default:
      return nullptr;
  }
}

static BLResult BL_CDECL blPipeGenRuntimeGet(PipeRuntime* self_, uint32_t signature, DispatchData* dispatchData, PipeLookupCache* cache) noexcept {
  blUnused(self_);

  Signature s{signature};
  FillFunc fillFunc = nullptr;
  FetchFunc fetchFunc = nullptr;

  FormatExt dstFormat = s.dstFormat();
  if (dstFormat == FormatExt::kA8) {
    switch (s.compOp()) {
      case CompOpExt::kSrcCopy: fillFunc = getFillFuncEx<Reference::CompOp_SrcCopy_Op<Reference::Pixel::P8_Alpha>, 1>(s); break;
      case CompOpExt::kSrcOver: fillFunc = getFillFuncEx<Reference::CompOp_SrcOver_Op<Reference::Pixel::P8_Alpha>, 1>(s); break;
      case CompOpExt::kPlus   : fillFunc = getFillFuncEx<Reference::CompOp_Plus_Op   <Reference::Pixel::P8_Alpha>, 1>(s); break;

      default:
        break;
    }
  }
  else if (dstFormat == FormatExt::kPRGB32 || dstFormat == FormatExt::kXRGB32) {
    switch (s.compOp()) {
      case CompOpExt::kSrcCopy: fillFunc = getFillFuncEx<Reference::CompOp_SrcCopy_Op<Reference::Pixel::P32_A8R8G8B8>, 4>(s); break;
      case CompOpExt::kSrcOver: fillFunc = getFillFuncEx<Reference::CompOp_SrcOver_Op<Reference::Pixel::P32_A8R8G8B8>, 4>(s); break;
      case CompOpExt::kPlus   : fillFunc = getFillFuncEx<Reference::CompOp_Plus_Op   <Reference::Pixel::P32_A8R8G8B8>, 4>(s); break;

      default:
        break;
    }
  }

  if (!fillFunc)
    return blTraceError(BL_ERROR_NOT_IMPLEMENTED);

  dispatchData->init(fillFunc, fetchFunc);

  if (cache)
    cache->store(signature, dispatchData);

  return BL_SUCCESS;
}

PipeStaticRuntime::PipeStaticRuntime() noexcept {
  // Setup the `PipeRuntime` base.
  _runtimeType = PipeRuntimeType::kStatic;
  _runtimeFlags = PipeRuntimeFlags::kNone;
  _runtimeSize = uint16_t(sizeof(PipeStaticRuntime));

  // PipeStaticRuntime destructor - never called.
  _destroy = nullptr;

  // PipeStaticRuntime interface - used by the rendering context and `PipeProvider`.
  _funcs.test = blPipeGenRuntimeGet;
  _funcs.get = blPipeGenRuntimeGet;
}

PipeStaticRuntime::~PipeStaticRuntime() noexcept {}

} // {Pipeline}
} // {bl}

// FixedPipelineRuntime - Runtime Registration
// ===========================================

void blStaticPipelineRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  bl::Pipeline::PipeStaticRuntime::_global.init();
}
