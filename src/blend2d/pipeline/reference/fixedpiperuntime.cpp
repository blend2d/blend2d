// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_FIXED_PIPE)

#include "../../pipeline/reference/compopgeneric_p.h"
#include "../../pipeline/reference/fillgeneric_p.h"
#include "../../pipeline/reference/fixedpiperuntime_p.h"
#include "../../support/wrap_p.h"

namespace BLPipeline {

// FixedPipelineRuntime - Globals
// ==============================

BLWrap<PipeStaticRuntime> PipeStaticRuntime::_global;

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

static BLResult BL_CDECL blPipeGenRuntimeGet(PipeRuntime* self_, uint32_t signature, DispatchData* dispatchData, PipeLookupCache* cache) noexcept {
  blUnused(self_);

  Signature s{signature};
  FillFunc fillFunc = nullptr;
  FetchFunc fetchFunc = nullptr;

  switch (s.fetchType()) {
    case FetchType::kSolid:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_Solid>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_Solid>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_Solid>(s.fillType()); break;
      }
      break;

    case FetchType::kPatternAlignedBlit:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_PatternAlignedBlit_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_PatternAlignedBlit_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_PatternAlignedBlit_PRGB32>(s.fillType()); break;
      }
      break;

    case FetchType::kPatternAlignedPad:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_PatternAlignedPad_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_PatternAlignedPad_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_PatternAlignedPad_PRGB32>(s.fillType()); break;
      }
      break;

    case FetchType::kPatternAlignedRepeat:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_PatternAlignedRepeat_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_PatternAlignedRepeat_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_PatternAlignedRepeat_PRGB32>(s.fillType()); break;
      }
      break;

    case FetchType::kPatternAlignedRoR:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_PatternAlignedRoR_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_PatternAlignedRoR_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_PatternAlignedRoR_PRGB32>(s.fillType()); break;
      }
      break;

    case FetchType::kPatternFxPad:
    case FetchType::kPatternFyPad:
    case FetchType::kPatternFxFyPad:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_PatternFxFyPad_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_PatternFxFyPad_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_PatternFxFyPad_PRGB32>(s.fillType()); break;
      }
      break;

    case FetchType::kPatternFxRoR:
    case FetchType::kPatternFyRoR:
    case FetchType::kPatternFxFyRoR:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_PatternFxFyRoR_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_PatternFxFyRoR_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_PatternFxFyRoR_PRGB32>(s.fillType()); break;
      }
      break;

    case FetchType::kPatternAffineNNAny:
    case FetchType::kPatternAffineNNOpt:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_PatternAffineNNAny_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_PatternAffineNNAny_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_PatternAffineNNAny_PRGB32>(s.fillType()); break;
      }
      break;

    case FetchType::kPatternAffineBIAny:
    case FetchType::kPatternAffineBIOpt:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_PatternAffineBIAny_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_PatternAffineBIAny_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_PatternAffineBIAny_PRGB32>(s.fillType()); break;
      }
      break;

    case FetchType::kGradientLinearNNPad:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_LinearPad_NN>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_LinearPad_NN>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_LinearPad_NN>(s.fillType()); break;
      }
      break;

    case FetchType::kGradientLinearNNRoR:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_LinearRoR_NN>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_LinearRoR_NN>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_LinearRoR_NN>(s.fillType()); break;
      }
      break;

    case FetchType::kGradientLinearDitherPad:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_LinearPad_Dither>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_LinearPad_Dither>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_LinearPad_Dither>(s.fillType()); break;
      }
      break;

    case FetchType::kGradientLinearDitherRoR:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_LinearRoR_Dither>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_LinearRoR_Dither>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_LinearRoR_Dither>(s.fillType()); break;
      }
      break;

    case FetchType::kGradientRadialNNPad:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_RadialPad_NN>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_RadialPad_NN>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_RadialPad_NN>(s.fillType()); break;
      }
      break;

    case FetchType::kGradientRadialNNRoR:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_RadialRoR_NN>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_RadialRoR_NN>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_RadialRoR_NN>(s.fillType()); break;
      }
      break;

    case FetchType::kGradientRadialDitherPad:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_RadialPad_Dither>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_RadialPad_Dither>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_RadialPad_Dither>(s.fillType()); break;
      }
      break;

    case FetchType::kGradientRadialDitherRoR:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_RadialRoR_Dither>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_RadialRoR_Dither>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_RadialRoR_Dither>(s.fillType()); break;
      }
      break;

    case FetchType::kGradientConicNN:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_Conic_NN>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_Conic_NN>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_Conic_NN>(s.fillType()); break;
      }
      break;

    case FetchType::kGradientConicDither:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_Conic_Dither>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_Conic_Dither>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_Conic_Dither>(s.fillType()); break;
      }
      break;
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

} // {BLPipeline}

// FixedPipelineRuntime - Runtime Registration
// ===========================================

void blStaticPipelineRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  BLPipeline::PipeStaticRuntime::_global.init();
}

#endif
