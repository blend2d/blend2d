// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_test_p.h"
#if defined(BL_TEST)

#include "bitset.h"
#include "font.h"
#include "fontdata.h"
#include "fontface.h"
#include "matrix.h"
#include "path.h"

#include "../test/resources/abeezee_regular_ttf.h"

// bl::Font - Tests
// ================

namespace bl {
namespace Tests {

UNIT(font, BL_TEST_GROUP_TEXT_COMBINED) {
  BLFontData fontData;
  BLFontFace fontFace;

  INFO("Testing creation of BLFontData from raw buffer");
  EXPECT_SUCCESS(fontData.createFromData(resource_abeezee_regular_ttf, sizeof(resource_abeezee_regular_ttf)));
  EXPECT_SUCCESS(fontFace.createFromData(fontData, 0));

  INFO("Testing retrieving unicode character coverage from BLFontFace");
  {
    BLBitSet characterCoverage;
    EXPECT_SUCCESS(fontFace.getCharacterCoverage(&characterCoverage));

    // The font provides 252 characters - verify at least this.
    uint32_t cardinality = characterCoverage.cardinality();
    EXPECT_EQ(cardinality, 252u);
  }
}

} // {Tests}
} // {bl}

#endif // BL_TEST
