// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_test_p.h"
#if defined(BL_TEST)

#include "array_p.h"
#include "fontfeaturesettings_p.h"
#include "fonttagdata_p.h"
#include "object_p.h"

// bl::FontFeatureSettings - Tests
// ===============================

namespace bl {
namespace Tests {

static void verifyFontFeatureSettings(const BLFontFeatureSettings& ffs) noexcept {
  BLFontFeatureSettingsView view;
  ffs.getView(&view);

  if (view.size == 0)
    return;

  uint32_t prevTag = view.data[0].tag;
  for (size_t i = 1; i < view.size; i++) {
    EXPECT_LT(prevTag, view.data[i].tag)
      .message("BLFontFeatureSettings is corrupted - previous tag 0x%08X is not less than current tag 0x%08X at [%zu]", prevTag, view.data[i].tag, i);
    prevTag = view.data[i].tag;
  }
}

UNIT(fontfeaturesettings_allocation_strategy, BL_TEST_GROUP_TEXT_CONTAINERS) {
  BLFontFeatureSettings ffs;
  size_t capacity = ffs.capacity();

  constexpr uint32_t kCharRange = FontTagData::kCharRangeInTag;
  constexpr uint32_t kNumItems = FontTagData::kUniqueTagCount / 100;

  for (uint32_t i = 0; i < kNumItems; i++) {
    BLTag tag = BL_MAKE_TAG(
      uint32_t(' ') + (i / (kCharRange * kCharRange * kCharRange)),
      uint32_t(' ') + (i / (kCharRange * kCharRange)) % kCharRange,
      uint32_t(' ') + (i / (kCharRange)) % kCharRange,
      uint32_t(' ') + (i % kCharRange));

    ffs.setValue(tag, i & 0xFFFFu);
    if (capacity != ffs.capacity()) {
      size_t implSize = FontFeatureSettingsInternal::implSizeFromCapacity(ffs.capacity()).value();
      INFO("Capacity increased from %zu to %zu [ImplSize=%zu]\n", capacity, ffs.capacity(), implSize);
      capacity = ffs.capacity();
    }
  }

  verifyFontFeatureSettings(ffs);
}

UNIT(fontfeaturesettings, BL_TEST_GROUP_TEXT_CONTAINERS) {
  // These are not sorted on purpose to test whether BLFontFeatureSettings would sort them during insertion.
  static const uint32_t fatTags[] = {
    BL_MAKE_TAG('r', 'a', 'n', 'd'),
    BL_MAKE_TAG('a', 'a', 'l', 't'),
    BL_MAKE_TAG('s', 's', '0', '9'),
    BL_MAKE_TAG('s', 's', '0', '4')
  };

  INFO("SSO initial state");
  {
    BLFontFeatureSettings ffs;

    EXPECT_TRUE(ffs._d.sso());
    EXPECT_TRUE(ffs.empty());
    EXPECT_EQ(ffs.size(), 0u);
    EXPECT_EQ(ffs.capacity(), BLFontFeatureSettings::kSSOCapacity);

    // SSO mode should present all available features as invalid (unassigned).
    for (uint32_t featureId = 0; featureId < FontTagData::kFeatureIdCount; featureId++) {
      BLTag featureTag = FontTagData::featureIdToTagTable[featureId];
      EXPECT_EQ(ffs.getValue(featureTag), BL_FONT_FEATURE_INVALID_VALUE);
    }

    // Trying to get an unknown tag should fail.
    EXPECT_EQ(ffs.getValue(BL_MAKE_TAG('-', '-', '-', '-')), BL_FONT_FEATURE_INVALID_VALUE);
    EXPECT_EQ(ffs.getValue(BL_MAKE_TAG('a', 'a', 'a', 'a')), BL_FONT_FEATURE_INVALID_VALUE);
    EXPECT_EQ(ffs.getValue(BL_MAKE_TAG('z', 'z', 'z', 'z')), BL_FONT_FEATURE_INVALID_VALUE);
  }

  INFO("SSO bit tag/value storage");
  {
    BLFontFeatureSettings ffs;

    // SSO storage must allow to store ALL font features that have bit mapping.
    uint32_t numTags = 0;
    for (uint32_t featureId = 0; featureId < FontTagData::kFeatureIdCount; featureId++) {
      if (FontTagData::featureInfoTable[featureId].hasBitId()) {
        numTags++;
        BLTag featureTag = FontTagData::featureIdToTagTable[featureId];

        EXPECT_SUCCESS(ffs.setValue(featureTag, 1u));
        EXPECT_EQ(ffs.getValue(featureTag), 1u);
        EXPECT_EQ(ffs.size(), numTags);
        EXPECT_TRUE(ffs._d.sso());

        verifyFontFeatureSettings(ffs);
      }
    }

    // Set all features to zero (disabled, but still present in the mapping).
    for (uint32_t featureId = 0; featureId < FontTagData::kFeatureIdCount; featureId++) {
      if (FontTagData::featureInfoTable[featureId].hasBitId()) {
        BLTag featureTag = FontTagData::featureIdToTagTable[featureId];

        EXPECT_SUCCESS(ffs.setValue(featureTag, 0u));
        EXPECT_EQ(ffs.getValue(featureTag), 0u);
        EXPECT_EQ(ffs.size(), numTags);
        EXPECT_TRUE(ffs._d.sso());
        verifyFontFeatureSettings(ffs);
      }
    }

    // Remove all features.
    for (uint32_t featureId = 0; featureId < FontTagData::kFeatureIdCount; featureId++) {
      if (FontTagData::featureInfoTable[featureId].hasBitId()) {
        numTags--;
        BLTag featureTag = FontTagData::featureIdToTagTable[featureId];

        EXPECT_SUCCESS(ffs.removeValue(featureTag));
        EXPECT_EQ(ffs.getValue(featureTag), BL_FONT_FEATURE_INVALID_VALUE);
        EXPECT_EQ(ffs.size(), numTags);
        EXPECT_TRUE(ffs._d.sso());

        verifyFontFeatureSettings(ffs);
      }
    }

    EXPECT_TRUE(ffs.empty());
    EXPECT_EQ(ffs, BLFontFeatureSettings());
  }

  INFO("SSO bit tag/value storage limitations");
  {
    BLFontFeatureSettings ffs;

    // Trying to set any other value than 0-1 with bit tags fails.
    for (uint32_t featureId = 0; featureId < FontTagData::kFeatureIdCount; featureId++) {
      if (FontTagData::featureInfoTable[featureId].hasBitId()) {
        BLTag featureTag = FontTagData::featureIdToTagTable[featureId];
        EXPECT_EQ(ffs.setValue(featureTag, 2u), BL_ERROR_INVALID_VALUE);
      }
    }

    EXPECT_TRUE(ffs.empty());
  }

  INFO("SSO bit tag/value storage + fat tag/value storage");
  {
    BLFontFeatureSettings ffs;
    uint32_t numTags = 0;

    // Add fat tag/value data.
    for (uint32_t i = 0; i < BL_ARRAY_SIZE(fatTags); i++) {
      numTags++;

      EXPECT_SUCCESS(ffs.setValue(fatTags[i], 15u));
      EXPECT_EQ(ffs.getValue(fatTags[i]), 15u);
      EXPECT_EQ(ffs.size(), numTags);
      EXPECT_TRUE(ffs._d.sso());

      verifyFontFeatureSettings(ffs);

      // Verify that changing a fat tag's value is working properly (it's bit twiddling).
      EXPECT_SUCCESS(ffs.setValue(fatTags[i], 1u));
      EXPECT_EQ(ffs.getValue(fatTags[i]), 1u);
      EXPECT_EQ(ffs.size(), numTags);
      EXPECT_TRUE(ffs._d.sso());

      verifyFontFeatureSettings(ffs);
    }

    // Add bit tag/value data.
    for (uint32_t featureId = 0; featureId < FontTagData::kFeatureIdCount; featureId++) {
      if (FontTagData::featureInfoTable[featureId].hasBitId()) {
        numTags++;
        BLTag featureTag = FontTagData::featureIdToTagTable[featureId];

        EXPECT_SUCCESS(ffs.setValue(featureTag, 1u));
        EXPECT_EQ(ffs.getValue(featureTag), 1u);
        EXPECT_EQ(ffs.size(), numTags);
        EXPECT_TRUE(ffs._d.sso());

        verifyFontFeatureSettings(ffs);
      }
    }

    // Remove fat tag/value data.
    for (uint32_t i = 0; i < BL_ARRAY_SIZE(fatTags); i++) {
      numTags--;

      EXPECT_SUCCESS(ffs.removeValue(fatTags[i]));
      EXPECT_EQ(ffs.size(), numTags);
      EXPECT_TRUE(ffs._d.sso());

      verifyFontFeatureSettings(ffs);
    }

    // Remove bit tag/value data.
    for (uint32_t featureId = 0; featureId < FontTagData::kFeatureIdCount; featureId++) {
      if (FontTagData::featureInfoTable[featureId].hasBitId()) {
        numTags--;
        BLTag featureTag = FontTagData::featureIdToTagTable[featureId];

        EXPECT_SUCCESS(ffs.removeValue(featureTag));
        EXPECT_EQ(ffs.size(), numTags);
        EXPECT_TRUE(ffs._d.sso());

        verifyFontFeatureSettings(ffs);
      }
    }

    EXPECT_TRUE(ffs.empty());
    EXPECT_EQ(ffs, BLFontFeatureSettings());
  }

  INFO("SSO tag/value equality");
  {
    BLFontFeatureSettings ffsA;
    BLFontFeatureSettings ffsB;

    // Assign bit tag/value data.
    for (uint32_t i = 0; i < FontTagData::kFeatureIdCount; i++) {
      uint32_t featureId = i;
      if (FontTagData::featureInfoTable[featureId].hasBitId()) {
        BLTag featureTag = FontTagData::featureIdToTagTable[featureId];
        EXPECT_SUCCESS(ffsA.setValue(featureTag, 1u));
        verifyFontFeatureSettings(ffsA);
      }
    }

    for (uint32_t i = 0; i < FontTagData::kFeatureIdCount; i++) {
      uint32_t featureId = FontTagData::kFeatureIdCount - 1 - i;
      if (FontTagData::featureInfoTable[featureId].hasBitId()) {
        BLTag featureTag = FontTagData::featureIdToTagTable[featureId];
        EXPECT_SUCCESS(ffsB.setValue(featureTag, 1u));
        verifyFontFeatureSettings(ffsB);
      }
    }

    EXPECT_EQ(ffsA, ffsB);

    // Assign fat tag/value data.
    for (uint32_t i = 0; i < BL_ARRAY_SIZE(fatTags); i++) {
      EXPECT_SUCCESS(ffsA.setValue(fatTags[i], i));
      verifyFontFeatureSettings(ffsA);
    }

    for (uint32_t iRev = 0; iRev < BL_ARRAY_SIZE(fatTags); iRev++) {
      uint32_t i = BL_ARRAY_SIZE(fatTags) - 1 - iRev;
      EXPECT_SUCCESS(ffsB.setValue(fatTags[i], i));
      verifyFontFeatureSettings(ffsB);
    }

    EXPECT_EQ(ffsA, ffsB);

    // Remove fat tag/value data.
    for (uint32_t i = 0; i < BL_ARRAY_SIZE(fatTags); i++) {
      EXPECT_SUCCESS(ffsA.removeValue(fatTags[i]));
      verifyFontFeatureSettings(ffsA);
    }

    for (uint32_t iRev = 0; iRev < BL_ARRAY_SIZE(fatTags); iRev++) {
      uint32_t i = BL_ARRAY_SIZE(fatTags) - 1 - iRev;
      EXPECT_SUCCESS(ffsB.removeValue(fatTags[i]));
      verifyFontFeatureSettings(ffsB);
    }

    EXPECT_EQ(ffsA, ffsB);
  }

  INFO("Dynamic representation");
  {
    BLFontFeatureSettings ffs;

    for (uint32_t i = 0; i < FontTagData::kFeatureIdCount; i++) {
      uint32_t featureId = FontTagData::kFeatureIdCount - 1 - i;
      BLTag featureTag = FontTagData::featureIdToTagTable[featureId];
      EXPECT_SUCCESS(ffs.setValue(featureTag, 1u));
      EXPECT_EQ(ffs.getValue(featureTag), 1u);
      EXPECT_EQ(ffs.size(), i + 1u);
      verifyFontFeatureSettings(ffs);
    }

    EXPECT_FALSE(ffs._d.sso());

    for (uint32_t i = 0; i < FontTagData::kFeatureIdCount; i++) {
      uint32_t featureId = FontTagData::kFeatureIdCount - 1 - i;
      BLTag featureTag = FontTagData::featureIdToTagTable[featureId];

      if (!FontTagData::featureInfoTable[featureId].hasBitId()) {
        EXPECT_SUCCESS(ffs.setValue(featureTag, 65535u));
        EXPECT_EQ(ffs.getValue(featureTag), 65535u);
      }
      else {
        EXPECT_SUCCESS(ffs.setValue(featureTag, 0u));
        EXPECT_EQ(ffs.getValue(featureTag), 0u);
      }

      verifyFontFeatureSettings(ffs);
    }

    EXPECT_FALSE(ffs._d.sso());

    for (uint32_t i = 0; i < FontTagData::kFeatureIdCount; i++) {
      uint32_t featureId = i;
      BLTag featureTag = FontTagData::featureIdToTagTable[featureId];

      EXPECT_SUCCESS(ffs.removeValue(featureTag));
      EXPECT_EQ(ffs.getValue(featureTag), BL_FONT_FEATURE_INVALID_VALUE);

      verifyFontFeatureSettings(ffs);
    }

    EXPECT_TRUE(ffs.empty());
    EXPECT_EQ(ffs.size(), 0u);
    EXPECT_FALSE(ffs._d.sso());

  }

  INFO("Dynamic tag/value equality");
  {
    BLFontFeatureSettings ffs1;
    BLFontFeatureSettings ffs2;

    for (uint32_t i = 0; i < FontTagData::kFeatureIdCount; i++) {
      EXPECT_SUCCESS(ffs1.setValue(FontTagData::featureIdToTagTable[i], 1u));
      EXPECT_SUCCESS(ffs2.setValue(FontTagData::featureIdToTagTable[FontTagData::kFeatureIdCount - 1u - i], 1u));

      verifyFontFeatureSettings(ffs1);
      verifyFontFeatureSettings(ffs2);
    }

    EXPECT_EQ(ffs1, ffs2);
  }

  INFO("Dynamic tag/value vs SSO tag/value equality");
  {
    BLFontFeatureSettings ffs1;
    BLFontFeatureSettings ffs2;

    for (uint32_t i = 0; i < FontTagData::kFeatureIdCount; i++) {
      if (FontTagData::featureInfoTable[i].hasBitId()) {
        EXPECT_SUCCESS(ffs1.setValue(FontTagData::featureIdToTagTable[i], 1u));
        EXPECT_SUCCESS(ffs2.setValue(FontTagData::featureIdToTagTable[i], 1u));

        verifyFontFeatureSettings(ffs1);
        verifyFontFeatureSettings(ffs2);
      }
    }

    EXPECT_EQ(ffs1, ffs2);

    // Make ffs1 go out of SSO mode.
    EXPECT_SUCCESS(ffs1.setValue(BL_MAKE_TAG('a', 'a', 'a', 'a'), 1000));
    EXPECT_SUCCESS(ffs1.removeValue(BL_MAKE_TAG('a', 'a', 'a', 'a')));
    EXPECT_EQ(ffs1, ffs2);
    EXPECT_EQ(ffs2, ffs1);

    // Make ffs2 go out of SSO mode.
    EXPECT_SUCCESS(ffs2.setValue(BL_MAKE_TAG('a', 'a', 'a', 'a'), 1000));
    EXPECT_SUCCESS(ffs2.removeValue(BL_MAKE_TAG('a', 'a', 'a', 'a')));
    EXPECT_EQ(ffs1, ffs2);
    EXPECT_EQ(ffs2, ffs1);
  }
}

} // {Tests}
} // {bl}

#endif // BL_TEST
