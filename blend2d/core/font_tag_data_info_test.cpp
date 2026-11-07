// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/core/font_tag_data_info_p.h>

// FontTagData - Tests
// ===================

namespace bl::FontTagTests {

TEST_CASE(core_font_tag_data_info) {
  // Make sure that feature_bit_id_to_feature_id_table contains a correct reverse mapping.
  TEST_LOG("Verifying whether the feature id to feature bit mapping and its reverse mapping match");
  for (uint32_t bit_id = 0; bit_id < 32u; bit_id++) {
    uint32_t bit_id_to_feature_id = FontTagData::feature_bit_id_to_feature_id_table[bit_id];
    uint32_t feature_id_to_bit_id = FontTagData::feature_info_table[bit_id_to_feature_id].bit_id;

    EXPECT_EQ(bit_id, feature_id_to_bit_id)
      .message("FeatureInfoTable and FeatureBitIdToFeatureIdTable mismatch - bit_id(%u != %u) (feature_id=%u))",
        bit_id, feature_id_to_bit_id, bit_id_to_feature_id);
  }
}

} // {bl::FontTagTests}

#endif // BL_TEST
