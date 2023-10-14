// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_test_p.h"
#if defined(BL_TEST)

#include "fonttagdata_p.h"

// bl::FontTagData - Tests
// =======================

namespace bl {
namespace Tests {

typedef uint32_t (*TagToIdFunc)(BLTag tag);

static void verifyTags(const char* category, const BLTag* tags, uint32_t count, TagToIdFunc tagToIdFunc) noexcept {
  if (count == 0)
    return;

  INFO("Verifying whether the %s tag data is sorted", category);
  BLTag prev = tags[0];
  for (uint32_t i = 1; i < count; i++) {
    EXPECT_LT(prev, tags[i]);
    prev = tags[i];
  }

  INFO("Verifying whether the %s tag to id translation is correct", category);
  for (uint32_t i = 0; i < count; i++) {
    uint32_t id = tagToIdFunc(tags[i]);
    EXPECT_EQ(id, i);
  }
}

UNIT(fonttagdata_ids, BL_TEST_GROUP_TEXT_OPENTYPE) {
  verifyTags("tableId", FontTagData::tableIdToTagTable, FontTagData::kTableIdCount, FontTagData::tableTagToId);
  verifyTags("scriptId", FontTagData::scriptIdToTagTable, FontTagData::kScriptIdCount, FontTagData::scriptTagToId);
  verifyTags("languageId", FontTagData::languageIdToTagTable, FontTagData::kLanguageIdCount, FontTagData::languageTagToId);
  verifyTags("featureId", FontTagData::featureIdToTagTable, FontTagData::kFeatureIdCount, FontTagData::featureTagToId);
  verifyTags("baselineId", FontTagData::baselineIdToTagTable, FontTagData::kBaselineIdCount, FontTagData::baselineTagToId);
  verifyTags("variationId", FontTagData::variationIdToTagTable, FontTagData::kVariationIdCount, FontTagData::variationTagToId);
}

} // {Tests}
} // {bl}

#endif // BL_TEST
