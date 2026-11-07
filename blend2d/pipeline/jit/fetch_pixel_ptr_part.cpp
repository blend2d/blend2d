// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#if !defined(BL_BUILD_NO_JIT)

#include <blend2d/pipeline/jit/fetch_pixel_ptr_part_p.h>
#include <blend2d/pipeline/jit/fetch_utils_pixel_access_p.h>
#include <blend2d/pipeline/jit/pipe_compiler_p.h>

namespace bl::Pipeline::JIT {

// Pipeline::JIT::FetchPixelPtrPart - Construction & Destruction
// =============================================================

FetchPixelPtrPart::FetchPixelPtrPart(PipeCompiler* pc, FetchType fetch_type, FormatExt format) noexcept
  : FetchPart(pc, fetch_type, format) {

  _part_flags |= PipePartFlags::kMaskedAccess | PipePartFlags::kAdvanceXIsSimple;
  _max_vec_width_supported = kMaxPlatformWidth;
  _max_pixels = kUnlimitedMaxPixels;
}

// Pipeline::JIT::FetchPixelPtrPart - Fetch
// ========================================

void FetchPixelPtrPart::fetch(Pixel& p, PixelCount n, PixelFlags flags, PixelPredicate& predicate) noexcept {
  FetchUtils::fetch_pixels(pc, p, n, flags, fetch_info(), _ptr, _alignment, AdvanceMode::kNoAdvance, predicate);
}

} // {bl::Pipeline::JIT}

#endif // !BL_BUILD_NO_JIT
