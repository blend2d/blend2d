// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

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
