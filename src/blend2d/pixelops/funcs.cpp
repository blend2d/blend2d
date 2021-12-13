// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../runtime_p.h"
#include "../pixelops/funcs_p.h"

namespace BLPixelOps {

// BLPixelOps - Globals
// ====================

Funcs funcs;

// BLPixelOps - Interpolation Functions
// ====================================

namespace Interpolation {

BL_HIDDEN void BL_CDECL interpolate_prgb32(uint32_t* dPtr, uint32_t dSize, const BLGradientStop* sPtr, size_t sSize) noexcept;

#ifdef BL_BUILD_OPT_SSE2
BL_HIDDEN void BL_CDECL interpolate_prgb32_sse2(uint32_t* dPtr, uint32_t dSize, const BLGradientStop* sPtr, size_t sSize) noexcept;
#endif

#ifdef BL_BUILD_OPT_AVX2
BL_HIDDEN void BL_CDECL interpolate_prgb32_avx2(uint32_t* dPtr, uint32_t dWidth, const BLGradientStop* sPtr, size_t sSize) noexcept;
#endif

} // {Interpolation}

} // {BLPixelOps}

// BLPixelOps - Runtime Registration
// =================================

void blPixelOpsRtInit(BLRuntimeContext* rt) noexcept {
  // Maybe unused, if no architecture dependent optimizations are available.
  blUnused(rt);

  BLPixelOps::Funcs& funcs = BLPixelOps::funcs;

  // Initialize gradient ops.
  funcs.interpolate_prgb32 = BLPixelOps::Interpolation::interpolate_prgb32;

#ifdef BL_BUILD_OPT_SSE2
  if (blRuntimeHasSSE2(rt)) {
    funcs.interpolate_prgb32 = BLPixelOps::Interpolation::interpolate_prgb32_sse2;
  }
#endif

#ifdef BL_BUILD_OPT_AVX2
  if (blRuntimeHasAVX2(rt)) {
    funcs.interpolate_prgb32 = BLPixelOps::Interpolation::interpolate_prgb32_avx2;
  }
#endif
}
