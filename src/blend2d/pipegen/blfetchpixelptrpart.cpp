// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../pipegen/blfetchpixelptrpart_p.h"
#include "../pipegen/blpipecompiler_p.h"

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::FetchPixelPtrPart - Construction / Destruction]
// ============================================================================

FetchPixelPtrPart::FetchPixelPtrPart(PipeCompiler* pc, uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept
  : FetchPart(pc, fetchType, fetchPayload, format) {

  _maxOptLevelSupported = kOptLevel_X86_AVX;
  _maxPixels = kUnlimitedMaxPixels;
  _ptrAlignment = 0;
}

// ============================================================================
// [BLPipeGen::FetchPixelPtrPart - Fetch]
// ============================================================================

void FetchPixelPtrPart::fetch1(PixelARGB& p, uint32_t flags) noexcept {
  pc->xFetchARGB32_1x(p, flags, x86::ptr(_ptr), 4);
  pc->xSatisfyARGB32_1x(p, flags);
}

void FetchPixelPtrPart::fetch4(PixelARGB& p, uint32_t flags) noexcept {
  pc->xFetchARGB32_4x(p, flags, x86::ptr(_ptr), _ptrAlignment);
  pc->xSatisfyARGB32_Nx(p, flags);
}

void FetchPixelPtrPart::fetch8(PixelARGB& p, uint32_t flags) noexcept {
  pc->xFetchARGB32_8x(p, flags, x86::ptr(_ptr), _ptrAlignment);
  pc->xSatisfyARGB32_Nx(p, flags);
}

} // {BLPipeGen}
