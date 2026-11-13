// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/core/coverage_set.h>
#include <blend2d/core/font.h>
#include <blend2d/core/font_data.h>
#include <blend2d/core/font_face.h>
#include <blend2d/core/matrix.h>
#include <blend2d/core/path.h>

#include <blend2d-testing/resources/abeezee_regular_ttf.h>

// Font - Tests
// ============

namespace bl::FontTests {

TEST_CASE(core_font_from_data) {
  BLFontData font_data;
  BLFontFace font_face;

  EXPECT_OK(font_data.create_from_data(resource_abeezee_regular_ttf, sizeof(resource_abeezee_regular_ttf)));
  EXPECT_OK(font_face.create_from_data(font_data, 0));
}

TEST_CASE(core_font_cmap_coverage) {
  BLFontData font_data;
  BLFontFace font_face;

  EXPECT_OK(font_data.create_from_data(resource_abeezee_regular_ttf, sizeof(resource_abeezee_regular_ttf)));
  EXPECT_OK(font_face.create_from_data(font_data, 0));

  BLCoverageSet cmap_coverage;
  EXPECT_OK(font_face.get_cmap_coverage(&cmap_coverage));

  // The font provides 252 characters - verify at least this.
  uint32_t cardinality = cmap_coverage.cardinality();
  EXPECT_EQ(cardinality, 252u);
}

} // {bl::FontTests}

#endif // BL_TEST
