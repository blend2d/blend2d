// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/compoppart_p.h"
#include "../../pipeline/jit/fetchpart_p.h"
#include "../../pipeline/jit/fetchgradientpart_p.h"
#include "../../pipeline/jit/fetchpixelptrpart_p.h"
#include "../../pipeline/jit/fetchsolidpart_p.h"
#include "../../pipeline/jit/fetchpatternpart_p.h"
#include "../../pipeline/jit/fillpart_p.h"
#include "../../pipeline/jit/pipecomposer_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {

// bl::Pipeline::PipeComposer - Construction & Destruction
// =======================================================

// TODO: [JIT] There should be a getter on asmjit side that will return
// the `ZoneAllocator` object that can be used for these kind of purposes.
// It doesn't make sense to create another ZoneAllocator.
PipeComposer::PipeComposer(PipeCompiler& pc) noexcept
  : _pc(pc),
    _zone(pc.cc->_codeZone) {}

// bl::Pipeline::PipeComposer - Pipeline Parts Construction
// ========================================================

FillPart* PipeComposer::newFillPart(FillType fillType, FetchPart* dstPart, CompOpPart* compOpPart) noexcept {
  if (fillType == FillType::kBoxA)
    return newPartT<FillBoxAPart>(dstPart->as<FetchPixelPtrPart>(), compOpPart);

  if (fillType == FillType::kMask)
    return newPartT<FillMaskPart>(dstPart->as<FetchPixelPtrPart>(), compOpPart);

  if (fillType == FillType::kAnalytic)
    return newPartT<FillAnalyticPart>(dstPart->as<FetchPixelPtrPart>(), compOpPart);

  return nullptr;
}

FetchPart* PipeComposer::newFetchPart(FetchType fetchType, FormatExt format) noexcept {
  if (fetchType == FetchType::kSolid)
    return newPartT<FetchSolidPart>(format);

  if (fetchType >= FetchType::kGradientLinearFirst && fetchType <= FetchType::kGradientLinearLast)
    return newPartT<FetchLinearGradientPart>(fetchType, format);

  if (fetchType >= FetchType::kGradientRadialFirst && fetchType <= FetchType::kGradientRadialLast)
    return newPartT<FetchRadialGradientPart>(fetchType, format);

  if (fetchType >= FetchType::kGradientConicFirst && fetchType <= FetchType::kGradientConicLast)
    return newPartT<FetchConicGradientPart>(fetchType, format);

  if (fetchType >= FetchType::kPatternSimpleFirst && fetchType <= FetchType::kPatternSimpleLast)
    return newPartT<FetchSimplePatternPart>(fetchType, format);

  if (fetchType >= FetchType::kPatternAffineFirst && fetchType <= FetchType::kPatternAffineLast)
    return newPartT<FetchAffinePatternPart>(fetchType, format);

  if (fetchType == FetchType::kPixelPtr)
    return newPartT<FetchPixelPtrPart>(fetchType, format);

  return nullptr;
}

CompOpPart* PipeComposer::newCompOpPart(CompOpExt compOp, FetchPart* dstPart, FetchPart* srcPart) noexcept {
  return newPartT<CompOpPart>(compOp, dstPart, srcPart);
}

} // {JIT}
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT