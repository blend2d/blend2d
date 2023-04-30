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
    case FillType::kBoxU: return Reference::FillBoxU_Base<CompOp>::fillFunc;
    case FillType::kAnalytic: return Reference::FillAnalytic_Base<CompOp>::fillFunc;
    default:
      return nullptr;
  }
}

static BLResult BL_CDECL blPipeGenRuntimeGet(PipeRuntime* self_, uint32_t signature, DispatchData* dispatchData, PipeLookupCache* cache) noexcept {
  blUnused(self_);

  Signature s(signature);
  FillFunc fillFunc = nullptr;
  FetchFunc fetchFunc = nullptr;

  switch (s.fetchType()) {
    case FetchType::kSolid:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_Solid>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_Solid>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_Solid   >(s.fillType()); break;
      }
      break;
    case FetchType::kGradientLinearPad:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_LinearPad>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_LinearPad>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_LinearPad   >(s.fillType()); break;
      }
      break;
    case FetchType::kGradientLinearRoR:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_LinearRoR>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_LinearRoR>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_LinearRoR   >(s.fillType()); break;
      }
      break;
    case FetchType::kGradientRadialPad:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_RadialPad>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_RadialPad>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_RadialPad   >(s.fillType()); break;
      }
      break;
    case FetchType::kGradientRadialRoR:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_RadialRoR>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_RadialRoR>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_RadialRoR   >(s.fillType()); break;
      }
      break;
    case FetchType::kGradientConical:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_Conical>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_Conical>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_Conical   >(s.fillType()); break;
      }
      break;
    case FetchType::kPatternAlignedBlit:
      switch (s.compOp()) {
        case BL_COMP_OP_SRC_COPY: fillFunc = getFillFunc<Reference::CompOp_SrcCopy_PRGB32_BlitAA_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_SRC_OVER: fillFunc = getFillFunc<Reference::CompOp_SrcOver_PRGB32_BlitAA_PRGB32>(s.fillType()); break;
        case BL_COMP_OP_PLUS    : fillFunc = getFillFunc<Reference::CompOp_Plus_PRGB32_BlitAA_PRGB32   >(s.fillType()); break;
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
