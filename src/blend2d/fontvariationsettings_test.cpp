// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_test_p.h"
#if defined(BL_TEST)

#include "fonttagdata_p.h"
#include "fontvariationsettings_p.h"
#include "object_p.h"
#include "support/math_p.h"

// bl::FontVariationSettings - Tests
// =================================

namespace bl {
namespace Tests {

static void verifyFontVariationSettings(const BLFontVariationSettings& ffs) noexcept {
  BLFontVariationSettingsView view;
  ffs.getView(&view);

  if (view.size == 0)
    return;

  uint32_t prevTag = view.data[0].tag;
  for (size_t i = 1; i < view.size; i++) {
    EXPECT_LT(prevTag, view.data[i].tag)
      .message("BLFontVariationSettings is corrupted - previous tag 0x%08X is not less than current tag 0x%08X at [%zu]", prevTag, view.data[i].tag, i);
  }
}

UNIT(fontvariationsettings_allocation_strategy, BL_TEST_GROUP_TEXT_CONTAINERS) {
  BLFontVariationSettings ffs;
  size_t capacity = ffs.capacity();

  constexpr uint32_t kCharRange = FontTagData::kCharRangeInTag;
  constexpr uint32_t kNumItems = FontTagData::kUniqueTagCount / 100;

  for (uint32_t i = 0; i < kNumItems; i++) {
    BLTag tag = BL_MAKE_TAG(
      uint32_t(' ') + (i / (kCharRange * kCharRange * kCharRange)),
      uint32_t(' ') + (i / (kCharRange * kCharRange)) % kCharRange,
      uint32_t(' ') + (i / (kCharRange)) % kCharRange,
      uint32_t(' ') + (i % kCharRange));

    ffs.setValue(tag, float(i & 0xFFFFu));
    if (capacity != ffs.capacity()) {
      size_t implSize = FontVariationSettingsInternal::implSizeFromCapacity(ffs.capacity()).value();
      INFO("Capacity increased from %zu to %zu [ImplSize=%zu]\n", capacity, ffs.capacity(), implSize);
      capacity = ffs.capacity();
    }
  }

  verifyFontVariationSettings(ffs);
}

UNIT(fontvariationsettings, BL_TEST_GROUP_TEXT_CONTAINERS) {
  // These are not sorted on purpose - we want BLFontVariationSettings to sort them.
  static const uint32_t ssoTags[] = {
    BL_MAKE_TAG('w', 'g', 'h', 't'),
    BL_MAKE_TAG('i', 't', 'a', 'l'),
    BL_MAKE_TAG('w', 'd', 't', 'h')
  };

  static const uint32_t dynamicTags[] = {
    BL_MAKE_TAG('w', 'g', 'h', 't'),
    BL_MAKE_TAG('i', 't', 'a', 'l'),
    BL_MAKE_TAG('w', 'd', 't', 'h'),
    BL_MAKE_TAG('s', 'l', 'n', 't'),
    BL_MAKE_TAG('o', 'p', 's', 'z')
  };

  INFO("SSO representation");
  {
    BLFontVariationSettings ffs;

    EXPECT_TRUE(ffs._d.sso());
    EXPECT_TRUE(ffs.empty());
    EXPECT_EQ(ffs.size(), 0u);
    EXPECT_EQ(ffs.capacity(), BLFontVariationSettings::kSSOCapacity);

    // Getting an unknown tag should return invalid value.
    EXPECT_TRUE(Math::isNaN(ffs.getValue(BL_MAKE_TAG('-', '-', '-', '-'))));

    for (uint32_t i = 0; i < BL_ARRAY_SIZE(ssoTags); i++) {
      EXPECT_SUCCESS(ffs.setValue(ssoTags[i], 1u));
      EXPECT_EQ(ffs.getValue(ssoTags[i]), 1u);
      EXPECT_EQ(ffs.size(), i + 1);
      EXPECT_TRUE(ffs._d.sso());
      verifyFontVariationSettings(ffs);
    }

    for (uint32_t i = 0; i < BL_ARRAY_SIZE(ssoTags); i++) {
      EXPECT_SUCCESS(ffs.setValue(ssoTags[i], 0u));
      EXPECT_EQ(ffs.getValue(ssoTags[i]), 0u);
      EXPECT_EQ(ffs.size(), BL_ARRAY_SIZE(ssoTags));
      EXPECT_TRUE(ffs._d.sso());
      verifyFontVariationSettings(ffs);
    }

    for (uint32_t i = 0; i < BL_ARRAY_SIZE(ssoTags); i++) {
      EXPECT_SUCCESS(ffs.removeValue(ssoTags[i]));
      EXPECT_TRUE(Math::isNaN(ffs.getValue(ssoTags[i])));
      EXPECT_EQ(ffs.size(), BL_ARRAY_SIZE(ssoTags) - i - 1);
      EXPECT_TRUE(ffs._d.sso());
      verifyFontVariationSettings(ffs);
    }
  }

  INFO("SSO border cases");
  {
    // First veriation ids use R/I bits, which is used for reference counted dynamic objects.
    // What we want to test here is that this bit is not checked when destroying SSO instances.
    BLFontVariationSettings settings;
    settings.setValue(FontTagData::variationIdToTagTable[0], 0.5f);
    settings.setValue(FontTagData::variationIdToTagTable[1], 0.5f);
  }

  INFO("Dynamic representation");
  {
    BLFontVariationSettings ffs;

    EXPECT_TRUE(ffs._d.sso());
    EXPECT_TRUE(ffs.empty());
    EXPECT_EQ(ffs.size(), 0u);
    EXPECT_EQ(ffs.capacity(), BLFontVariationSettings::kSSOCapacity);

    // Getting an unknown tag should return invalid value.
    EXPECT_TRUE(Math::isNaN(ffs.getValue(BL_MAKE_TAG('-', '-', '-', '-'))));

    for (uint32_t i = 0; i < BL_ARRAY_SIZE(dynamicTags); i++) {
      EXPECT_SUCCESS(ffs.setValue(dynamicTags[i], 1u));
      EXPECT_EQ(ffs.getValue(dynamicTags[i]), 1u);
      EXPECT_EQ(ffs.size(), i + 1);
      verifyFontVariationSettings(ffs);
    }

    EXPECT_FALSE(ffs._d.sso());

    for (uint32_t i = 0; i < BL_ARRAY_SIZE(dynamicTags); i++) {
      EXPECT_SUCCESS(ffs.setValue(dynamicTags[i], 0u));
      EXPECT_EQ(ffs.getValue(dynamicTags[i]), 0u);
      EXPECT_EQ(ffs.size(), BL_ARRAY_SIZE(dynamicTags));
      verifyFontVariationSettings(ffs);
    }

    EXPECT_FALSE(ffs._d.sso());

    for (uint32_t i = 0; i < BL_ARRAY_SIZE(dynamicTags); i++) {
      EXPECT_SUCCESS(ffs.removeValue(dynamicTags[i]));
      EXPECT_TRUE(Math::isNaN(ffs.getValue(dynamicTags[i])));
      EXPECT_EQ(ffs.size(), BL_ARRAY_SIZE(dynamicTags) - i - 1);
      verifyFontVariationSettings(ffs);
    }

    EXPECT_FALSE(ffs._d.sso());
  }

  INFO("Equality");
  {
    BLFontVariationSettings ffs1;
    BLFontVariationSettings ffs2;

    for (uint32_t i = 0; i < BL_ARRAY_SIZE(ssoTags); i++) {
      EXPECT_SUCCESS(ffs1.setValue(ssoTags[i], 1u));
      EXPECT_SUCCESS(ffs2.setValue(ssoTags[BL_ARRAY_SIZE(ssoTags) - i - 1], 1u));
    }

    EXPECT_TRUE(ffs1.equals(ffs2));

    // Make ffs1 go out of SSO mode.
    EXPECT_SUCCESS(ffs1.setValue(BL_MAKE_TAG('a', 'a', 'a', 'a'), 1));
    EXPECT_SUCCESS(ffs1.removeValue(BL_MAKE_TAG('a', 'a', 'a', 'a')));
    EXPECT_TRUE(ffs1.equals(ffs2));

    // Make ffs2 go out of SSO mode.
    EXPECT_SUCCESS(ffs2.setValue(BL_MAKE_TAG('a', 'a', 'a', 'a'), 1));
    EXPECT_SUCCESS(ffs2.removeValue(BL_MAKE_TAG('a', 'a', 'a', 'a')));
    EXPECT_TRUE(ffs1.equals(ffs2));
  }
}

} // {Tests}
} // {bl}

#endif // BL_TEST
