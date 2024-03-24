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

  _partFlags |= PipePartFlags::kAdvanceXIsSimple;
  _maxSimdWidthSupported = SimdWidth::kMaxPlatformWidth;
  _maxPixels = kUnlimitedMaxPixels;

  if (pc->hasMaskedAccessOf(bpp()))
    _partFlags |= PipePartFlags::kMaskedAccess;
}

// bl::Pipeline::JIT::FetchPixelPtrPart - Fetch
// ============================================

void FetchPixelPtrPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  FetchUtils::x_fetch_pixel(pc, p, n, flags, format(), mem_ptr(_ptr), _alignment, predicate);
}

} // {JIT}
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT
