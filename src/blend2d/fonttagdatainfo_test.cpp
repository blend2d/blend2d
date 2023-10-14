// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_test_p.h"
#if defined(BL_TEST)

#include "fonttagdatainfo_p.h"

// bl::FontTagData - Tests
// =======================

namespace bl {
namespace Tests {

UNIT(fonttagdata_info, BL_TEST_GROUP_TEXT_OPENTYPE) {
  // Make sure that featureBitIdToFeatureIdTable contains a correct reverse mapping.
  INFO("Verifying whether the feature id to feature bit mapping and its reverse mapping match");
  for (uint32_t bitId = 0; bitId < 32u; bitId++) {
    uint32_t bitIdToFeatureId = FontTagData::featureBitIdToFeatureIdTable[bitId];
    uint32_t featureIdToBitId = FontTagData::featureInfoTable[bitIdToFeatureId].bitId;

    EXPECT_EQ(bitId, featureIdToBitId)
      .message("FeatureInfoTable and FeatureBitIdToFeatureIdTable mismatch - bitId(%u != %u) (featureId=%u))",
        bitId, featureIdToBitId, bitIdToFeatureId);
  }
}

} // {Tests}
} // {bl}

#endif // BL_TEST
