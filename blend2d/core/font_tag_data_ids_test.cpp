// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/core/font_tag_data_p.h>

// FontTagData - Tests
// ===================

namespace bl::FontTagTests {

typedef uint32_t (*TagToIdFunc)(BLTag tag);

static void test_tags_order(ZeroTest::TestRunner& test_runner, const char* category, const BLTag* tags, uint32_t count) noexcept {
  if (test_runner.test_variation(category)) {
    BLTag prev_tag = tags[0];
    for (uint32_t i = 1; i < count; i++) {
      BLTag tag = tags[i];
      EXPECT_LT(prev_tag, tag)
        .message("expected %s tag '%c%c%c%c' to be lexicographically after '%c%c%c%c'",
          category,
          (prev_tag >> 24) & 0xFF,
          (prev_tag >> 16) & 0xFF,
          (prev_tag >>  8) & 0xFF,
          (prev_tag >>  0) & 0xFF,
          (tag >> 24) & 0xFF,
          (tag >> 16) & 0xFF,
          (tag >>  8) & 0xFF,
          (tag >>  0) & 0xFF);
      prev_tag = tags[i];
    }
  }
}

static void test_tags_mapping(ZeroTest::TestRunner& test_runner, const char* category, const BLTag* tags, uint32_t count, TagToIdFunc tag_to_id_func) noexcept {
  if (test_runner.test_variation(category)) {
    for (uint32_t i = 0; i < count; i++) {
      uint32_t id = tag_to_id_func(tags[i]);
      EXPECT_EQ(id, i)
        .message("%s tag to id translation is incorrect: tag id #%u translated back to #%u", category, i, id);
    }
  }
}

TEST_CASE(core_font_tags_order) {
  test_tags_order(test_runner, "table"    , FontTagData::table_id_to_tag_table    , FontTagData::kTableIdCount    );
  test_tags_order(test_runner, "script"   , FontTagData::script_id_to_tag_table   , FontTagData::kScriptIdCount   );
  test_tags_order(test_runner, "language" , FontTagData::language_id_to_tag_table , FontTagData::kLanguageIdCount );
  test_tags_order(test_runner, "feature"  , FontTagData::feature_id_to_tag_table  , FontTagData::kFeatureIdCount  );
  test_tags_order(test_runner, "baseline" , FontTagData::baseline_id_to_tag_table , FontTagData::kBaselineIdCount );
  test_tags_order(test_runner, "variation", FontTagData::variation_id_to_tag_table, FontTagData::kVariationIdCount);
}

TEST_CASE(core_font_tags_mapping) {
  test_tags_mapping(test_runner, "table"    , FontTagData::table_id_to_tag_table    , FontTagData::kTableIdCount    , FontTagData::table_tag_to_id    );
  test_tags_mapping(test_runner, "script"   , FontTagData::script_id_to_tag_table   , FontTagData::kScriptIdCount   , FontTagData::script_tag_to_id   );
  test_tags_mapping(test_runner, "language" , FontTagData::language_id_to_tag_table , FontTagData::kLanguageIdCount , FontTagData::language_tag_to_id );
  test_tags_mapping(test_runner, "feature"  , FontTagData::feature_id_to_tag_table  , FontTagData::kFeatureIdCount  , FontTagData::feature_tag_to_id  );
  test_tags_mapping(test_runner, "baseline" , FontTagData::baseline_id_to_tag_table , FontTagData::kBaselineIdCount , FontTagData::baseline_tag_to_id );
  test_tags_mapping(test_runner, "variation", FontTagData::variation_id_to_tag_table, FontTagData::kVariationIdCount, FontTagData::variation_tag_to_id);
}

} // {bl::FontTagTests}

#endif // BL_TEST
