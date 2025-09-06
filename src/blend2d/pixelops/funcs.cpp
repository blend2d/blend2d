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

BL_HIDDEN void BL_CDECL interpolate_prgb32(uint32_t* d_ptr, uint32_t d_size, const BLGradientStop* s_ptr, size_t s_size) noexcept;
BL_HIDDEN void BL_CDECL interpolate_prgb64(uint64_t* d_ptr, uint32_t d_size, const BLGradientStop* s_ptr, size_t s_size) noexcept;

#ifdef BL_BUILD_OPT_SSE2
BL_HIDDEN void BL_CDECL interpolate_prgb32_sse2(uint32_t* d_ptr, uint32_t d_size, const BLGradientStop* s_ptr, size_t s_size) noexcept;
#endif

#ifdef BL_BUILD_OPT_AVX2
BL_HIDDEN void BL_CDECL interpolate_prgb32_avx2(uint32_t* d_ptr, uint32_t d_width, const BLGradientStop* s_ptr, size_t s_size) noexcept;
#endif

} // {Interpolation}
} // {PixelOps}
} // {bl}

// bl::PixelOps - Runtime Registration
// ===================================

void bl_pixel_ops_rt_init(BLRuntimeContext* rt) noexcept {
  // Maybe unused, if no architecture dependent optimizations are available.
  bl_unused(rt);

  bl::PixelOps::Funcs& funcs = bl::PixelOps::funcs;

  // Initialize gradient ops.
  funcs.interpolate_prgb32 = bl::PixelOps::Interpolation::interpolate_prgb32;
  funcs.interpolate_prgb64 = bl::PixelOps::Interpolation::interpolate_prgb64;

#ifdef BL_BUILD_OPT_SSE2
  if (bl_runtime_has_sse2(rt)) {
    funcs.interpolate_prgb32 = bl::PixelOps::Interpolation::interpolate_prgb32_sse2;
  }
#endif

#ifdef BL_BUILD_OPT_AVX2
  if (bl_runtime_has_avx2(rt)) {
    funcs.interpolate_prgb32 = bl::PixelOps::Interpolation::interpolate_prgb32_avx2;
  }
#endif
}
