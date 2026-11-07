// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/axl/commons.h>
#include <blend2d/core/font_p.h>
#include <blend2d/core/path_p.h>
#include <blend2d/core/path_test_p.h>
#include <blend2d/opentype/ot_glyf_p.h>

#include <blend2d-testing/resources/abeezee_regular_ttf.h>

namespace bl::OpenTypeTests {

static void opentype_glyf_outline_decoder_simd_impl_test_font_face(const BLFontFace& font_face) {
  auto get_glyph_outlines_opt = font_face._impl<FontFacePrivateImpl>()->funcs.get_glyph_outlines;
  auto get_glyph_outlines_ref = OpenType::GlyfImpl::get_glyph_outlines;

  if (get_glyph_outlines_opt == get_glyph_outlines_ref) {
    TEST_LOG("Reference and optimized functions are the same, skipping the test");
    return;
  }

  ScopedBuffer ref_scoped_buffer;
  ScopedBuffer opt_scoped_buffer;

  uint32_t glyph_count = font_face.glyph_count();
  BLMatrix2D transform = BLMatrix2D::make_identity();

  for (uint32_t glyph_id = 0; glyph_id < glyph_count; glyph_id++) {
    BLString description;
    description.append_format("GlyphId=%u", glyph_id);

    BLPath ref_path;
    BLPath opt_path;

    size_t ref_contour_count;
    size_t opt_contour_count;

    BLResult ref_result = get_glyph_outlines_ref(font_face._impl<BLFontFaceImpl>(), BLGlyphId(glyph_id), &transform, &ref_path, &ref_contour_count, &ref_scoped_buffer);
    BLResult opt_result = get_glyph_outlines_opt(font_face._impl<BLFontFaceImpl>(), BLGlyphId(glyph_id), &transform, &opt_path, &opt_contour_count, &opt_scoped_buffer);

    EXPECT_EQ(ref_result, opt_result);
    if (ref_result != BL_SUCCESS) {
      continue;
    }

    EXPECT_EQ(ref_contour_count, opt_contour_count);
    EXPECT_TRUE(PathTests::path_view_check_equality(opt_path.view(), ref_path.view(), description.data()));
  }
}

TEST_CASE(opentype_glyf_outline_decoder_simd_impl) {
  BLFontData font_data;
  EXPECT_OK(font_data.create_from_data(resource_abeezee_regular_ttf, sizeof(resource_abeezee_regular_ttf)));

  BLFontFace font_face;
  EXPECT_OK(font_face.create_from_data(font_data, 0));

  opentype_glyf_outline_decoder_simd_impl_test_font_face(font_face);
}

} // {bl::OpenTypeTests}

#endif // BL_TEST
