// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/fetchpixelptrpart_p.h"
#include "../../pipeline/jit/fetchutilspixelaccess_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {

// bl::Pipeline::JIT::FetchPixelPtrPart - Construction & Destruction
// =================================================================

FetchPixelPtrPart::FetchPixelPtrPart(PipeCompiler* pc, FetchType fetchType, FormatExt format) noexcept
  : FetchPart(pc, fetchType, format) {

  _partFlags |= PipePartFlags::kMaskedAccess | PipePartFlags::kAdvanceXIsSimple;
  _maxVecWidthSupported = VecWidth::kMaxPlatformWidth;
  _maxPixels = kUnlimitedMaxPixels;
}

// bl::Pipeline::JIT::FetchPixelPtrPart - Fetch
// ============================================

void FetchPixelPtrPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  FetchUtils::fetchPixels(pc, p, n, flags, fetchInfo(), _ptr, _alignment, AdvanceMode::kNoAdvance, predicate);
}

} // {JIT}
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT
