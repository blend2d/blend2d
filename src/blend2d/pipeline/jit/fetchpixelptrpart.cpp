// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if BL_TARGET_ARCH_X86 && !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/fetchpixelptrpart_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"

namespace BLPipeline {
namespace JIT {

// BLPipeline::JIT::FetchPixelPtrPart - Construction & Destruction
// ===============================================================

FetchPixelPtrPart::FetchPixelPtrPart(PipeCompiler* pc, FetchType fetchType, uint32_t format) noexcept
  : FetchPart(pc, fetchType, format) {

  /*
  _maxSimdWidthSupported = SimdWidth::k256;
  */
  _maxPixels = kUnlimitedMaxPixels;
}

// BLPipeline::JIT::FetchPixelPtrPart - Fetch
// ==========================================

void FetchPixelPtrPart::fetch1(Pixel& p, PixelFlags flags) noexcept {
  pc->xFetchPixel_1x(p, flags, format(), x86::ptr(_ptr), _ptrAlignment);
}

void FetchPixelPtrPart::fetch4(Pixel& p, PixelFlags flags) noexcept {
  pc->xFetchPixel_4x(p, flags, format(), x86::ptr(_ptr), _ptrAlignment);
}

void FetchPixelPtrPart::fetch8(Pixel& p, PixelFlags flags) noexcept {
  pc->xFetchPixel_8x(p, flags, format(), x86::ptr(_ptr), _ptrAlignment);
}

} // {JIT}
} // {BLPipeline}

#endif
