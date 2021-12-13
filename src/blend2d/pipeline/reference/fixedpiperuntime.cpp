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

static BLResult BL_CDECL blPipeGenRuntimeGet(PipeRuntime* self_, uint32_t signature, DispatchData* dispatchData, PipeLookupCache* cache) noexcept {
  blUnused(self_);

  Signature s(signature);
  FillFunc fillFunc = nullptr;
  FetchFunc fetchFunc = nullptr;

  if (!s.isSolid()) {
    switch (s.fillType()) {
      case FillType::kBoxA:
        if (s.compOp() == BL_COMP_OP_SRC_COPY)
          fillFunc = Reference::FillBoxA_Base<Reference::CompOp_SrcCopy_PRGB32_Linear>::fillFunc;
        else if (s.compOp() == BL_COMP_OP_PLUS)
          fillFunc = Reference::FillBoxA_Base<Reference::CompOp_Plus_PRGB32_Linear>::fillFunc;
        else
          fillFunc = Reference::FillBoxA_Base<Reference::CompOp_SrcOver_PRGB32_Linear>::fillFunc;
        break;

      case FillType::kBoxU:
        if (s.compOp() == BL_COMP_OP_SRC_COPY)
          fillFunc = Reference::FillBoxU_Base<Reference::CompOp_SrcCopy_PRGB32_Linear>::fillFunc;
        else if (s.compOp() == BL_COMP_OP_PLUS)
          fillFunc = Reference::FillBoxU_Base<Reference::CompOp_Plus_PRGB32_Linear>::fillFunc;
        else
          fillFunc = Reference::FillBoxU_Base<Reference::CompOp_SrcOver_PRGB32_Linear>::fillFunc;
        break;

      case FillType::kAnalytic:
        if (s.compOp() == BL_COMP_OP_SRC_COPY)
          fillFunc = Reference::FillAnalytic_Base<Reference::CompOp_SrcCopy_PRGB32_Linear>::fillFunc;
        else if (s.compOp() == BL_COMP_OP_PLUS)
          fillFunc = Reference::FillAnalytic_Base<Reference::CompOp_Plus_PRGB32_Linear>::fillFunc;
        else
          fillFunc = Reference::FillAnalytic_Base<Reference::CompOp_SrcOver_PRGB32_Linear>::fillFunc;
        break;

      default:
        return blTraceError(BL_ERROR_NOT_IMPLEMENTED);
    }
  }
  else {
    switch (s.fillType()) {
      case FillType::kBoxA:
        if (s.compOp() == BL_COMP_OP_SRC_COPY)
          fillFunc = Reference::FillBoxA_Base<Reference::CompOp_SrcCopy_PRGB32_Solid>::fillFunc;
        else if (s.compOp() == BL_COMP_OP_PLUS)
          fillFunc = Reference::FillBoxA_Base<Reference::CompOp_Plus_PRGB32_Solid>::fillFunc;
        else
          fillFunc = Reference::FillBoxA_Base<Reference::CompOp_SrcOver_PRGB32_Solid>::fillFunc;
        break;

      case FillType::kBoxU:
        if (s.compOp() == BL_COMP_OP_SRC_COPY)
          fillFunc = Reference::FillBoxU_Base<Reference::CompOp_SrcCopy_PRGB32_Solid>::fillFunc;
        else if (s.compOp() == BL_COMP_OP_PLUS)
          fillFunc = Reference::FillBoxU_Base<Reference::CompOp_Plus_PRGB32_Solid>::fillFunc;
        else
          fillFunc = Reference::FillBoxU_Base<Reference::CompOp_SrcOver_PRGB32_Solid>::fillFunc;
        break;

      case FillType::kAnalytic:
        if (s.compOp() == BL_COMP_OP_SRC_COPY)
          fillFunc = Reference::FillAnalytic_Base<Reference::CompOp_SrcCopy_PRGB32_Solid>::fillFunc;
        else if (s.compOp() == BL_COMP_OP_PLUS)
          fillFunc = Reference::FillAnalytic_Base<Reference::CompOp_Plus_PRGB32_Solid>::fillFunc;
        else
          fillFunc = Reference::FillAnalytic_Base<Reference::CompOp_SrcOver_PRGB32_Solid>::fillFunc;
        break;

      default:
        return blTraceError(BL_ERROR_NOT_IMPLEMENTED);
    }
  }

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
