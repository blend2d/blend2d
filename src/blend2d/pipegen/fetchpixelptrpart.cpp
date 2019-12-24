// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../api-build_p.h"
#if BL_TARGET_ARCH_X86 && !defined(BL_BUILD_NO_JIT)

#include "../pipegen/fetchpixelptrpart_p.h"
#include "../pipegen/pipecompiler_p.h"

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::FetchPixelPtrPart - Construction / Destruction]
// ============================================================================

FetchPixelPtrPart::FetchPixelPtrPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept
  : FetchPart(pc, fetchType, fetchPayload, format) {

  _maxPixels = kUnlimitedMaxPixels;
  _maxSimdWidthSupported = 16;
  _ptrAlignment = 0;
}

// ============================================================================
// [BLPipeGen::FetchPixelPtrPart - Fetch]
// ============================================================================

void FetchPixelPtrPart::fetch1(Pixel& p, uint32_t flags) noexcept {
  pc->xFetchPixel_1x(p, flags, format(), x86::ptr(_ptr), _ptrAlignment);
}

void FetchPixelPtrPart::fetch4(Pixel& p, uint32_t flags) noexcept {
  pc->xFetchPixel_4x(p, flags, format(), x86::ptr(_ptr), _ptrAlignment);
}

void FetchPixelPtrPart::fetch8(Pixel& p, uint32_t flags) noexcept {
  pc->xFetchPixel_8x(p, flags, format(), x86::ptr(_ptr), _ptrAlignment);
}

} // {BLPipeGen}

#endif
