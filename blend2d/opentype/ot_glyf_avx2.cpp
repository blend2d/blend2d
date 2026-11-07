// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#if defined(BL_TARGET_OPT_AVX2) && defined(BL_TARGET_OPT_FMA)

#include <blend2d/opentype/ot_glyf_simd_impl_p.h>

namespace bl::OpenType::GlyfImpl {

BLResult BL_CDECL get_glyph_outlines_avx2(
  const BLFontFaceImpl* face_impl,
  BLGlyphId glyph_id,
  const BLMatrix2D* transform,
  BLPath* out,
  size_t* contour_count_out,
  ScopedBuffer* tmp_buffer) noexcept {

  return get_glyph_outlines_simd_impl(face_impl, glyph_id, transform, out, contour_count_out, tmp_buffer);
}

} // {bl::OpenType::GlyfImpl}

#endif // BL_BUILD_OPT_AVX2
