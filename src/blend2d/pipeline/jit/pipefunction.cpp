// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../pipeline/jit/pipefunction_p.h"
#include "../../pipeline/jit/pipepart_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {

// bl::Pipeline::PipeFunction - Construction & Destruction
// =======================================================

PipeFunction::PipeFunction() noexcept {}

// bl::Pipeline::PipeFunction - Interface
// ======================================

void PipeFunction::prepare(PipeCompiler& pc, PipePart* root) noexcept {
  // Initialize SIMD width and everything that relies on it.
  VecWidth vw = pc.maxVecWidthFromCpuFeatures();

  // NOTE 1: It depends on parts which SIMD width will be used by the pipeline. We set the maximum SIMD width
  // available for this host CPU, but if any part doesn't support such width it will end up lower. For example
  // it's possible that the pipeline would use only 128-bit SIMD width even when the CPU has support for AVX-512.

  root->forEachPart([&](PipePart* part) {
    vw = VecWidth(blMin<uint32_t>(uint32_t(vw), uint32_t(part->maxVecWidthSupported())));
  });

  pc.initVecWidth(vw);

  // Prepare all parts (the flag marks all visited parts).
  root->forEachPartAndMark(PipePartFlags::kPrepareDone, [&](PipePart* part) {
    part->preparePart();
  });
}

void PipeFunction::beginFunction(PipeCompiler& pc) noexcept {
  AsmCompiler* cc = pc.cc;
  asmjit::FuncNode* funcNode = cc->newFunc(asmjit::FuncSignature::build<void, ContextData*, const void*, const void*>(asmjit::CallConvId::kCDecl));

  pc.initFunction(funcNode);

  _ctxData = pc.newGpPtr("ctxData");
  _fillData = pc.newGpPtr("fillData");
  _fetchData = pc.newGpPtr("fetchData");

  funcNode->setArg(0, _ctxData);
  funcNode->setArg(1, _fillData);
  funcNode->setArg(2, _fetchData);
}

void PipeFunction::endFunction(PipeCompiler& pc) noexcept {
  AsmCompiler* cc = pc.cc;

  // Finalize the pipeline function.
  cc->endFunc();
}

} // {JIT}
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT