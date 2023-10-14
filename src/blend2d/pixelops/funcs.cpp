// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../runtime_p.h"
#include "../pixelops/funcs_p.h"

namespace bl {
namespace PixelOps {

// bl::PixelOps - Globals
// ======================

Funcs funcs;

// bl::PixelOps - Interpolation Functions
// ======================================

namespace Interpolation {

BL_HIDDEN void BL_CDECL interpolate_prgb32(uint32_t* dPtr, uint32_t dSize, const BLGradientStop* sPtr, size_t sSize) noexcept;
BL_HIDDEN void BL_CDECL interpolate_prgb64(uint64_t* dPtr, uint32_t dSize, const BLGradientStop* sPtr, size_t sSize) noexcept;

#ifdef BL_BUILD_OPT_SSE2
BL_HIDDEN void BL_CDECL interpolate_prgb32_sse2(uint32_t* dPtr, uint32_t dSize, const BLGradientStop* sPtr, size_t sSize) noexcept;
#endif

#ifdef BL_BUILD_OPT_AVX2
BL_HIDDEN void BL_CDECL interpolate_prgb32_avx2(uint32_t* dPtr, uint32_t dWidth, const BLGradientStop* sPtr, size_t sSize) noexcept;
#endif

} // {Interpolation}
} // {PixelOps}
} // {bl}

// bl::PixelOps - Runtime Registration
// ===================================

void blPixelOpsRtInit(BLRuntimeContext* rt) noexcept {
  // Maybe unused, if no architecture dependent optimizations are available.
  blUnused(rt);

  bl::PixelOps::Funcs& funcs = bl::PixelOps::funcs;

  // Initialize gradient ops.
  funcs.interpolate_prgb32 = bl::PixelOps::Interpolation::interpolate_prgb32;
  funcs.interpolate_prgb64 = bl::PixelOps::Interpolation::interpolate_prgb64;

#ifdef BL_BUILD_OPT_SSE2
  if (blRuntimeHasSSE2(rt)) {
    funcs.interpolate_prgb32 = bl::PixelOps::Interpolation::interpolate_prgb32_sse2;
  }
#endif

#ifdef BL_BUILD_OPT_AVX2
  if (blRuntimeHasAVX2(rt)) {
    funcs.interpolate_prgb32 = bl::PixelOps::Interpolation::interpolate_prgb32_avx2;
  }
#endif
}
