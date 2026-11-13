// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/axl/math_p.h>
#include <blend2d/core/font_tag_data_p.h>
#include <blend2d/core/font_variation_settings_p.h>
#include <blend2d/core/object_layer_p.h>

#include <initializer_list>

namespace bl::FontVariationSettingsTests {

// FontVariationSettings - Test Tables
// ===================================

static const BLFontVariationItem sso_items[] = {
  {BL_MAKE_TAG('i', 't', 'a', 'l'), 0.1f},
  {BL_MAKE_TAG('w', 'd', 't', 'h'), 0.2f},
  {BL_MAKE_TAG('w', 'g', 'h', 't'), 0.3f}
};

static const BLFontVariationItem sso_items_changed[] = {
  {BL_MAKE_TAG('i', 't', 'a', 'l'), 0.5f},
  {BL_MAKE_TAG('w', 'd', 't', 'h'), 0.8f},
  {BL_MAKE_TAG('w', 'g', 'h', 't'), 1.0f}
};

static const BLFontVariationItem dynamic_items[] = {
  {BL_MAKE_TAG('I', 'N', 'L', 'N'), 0.1f},
  {BL_MAKE_TAG('R', 'O', 'U', 'N'), 0.2f},
  {BL_MAKE_TAG('T', 'R', 'A', 'P'), 0.3f},
  {BL_MAKE_TAG('Y', 'T', 'D', 'E'), 0.4f},
  {BL_MAKE_TAG('i', 't', 'a', 'l'), 0.5f},
  {BL_MAKE_TAG('o', 'p', 's', 'z'), 0.6f},
  {BL_MAKE_TAG('s', 'l', 'n', 't'), 0.7f},
  {BL_MAKE_TAG('w', 'd', 't', 'h'), 0.8f},
  {BL_MAKE_TAG('w', 'g', 'h', 't'), 0.9f}
};

static const BLFontVariationItem dynamic_items_changed[] = {
  {BL_MAKE_TAG('I', 'N', 'L', 'N'), 1.0f},
  {BL_MAKE_TAG('R', 'O', 'U', 'N'), 0.7f},
  {BL_MAKE_TAG('T', 'R', 'A', 'P'), 0.1f},
  {BL_MAKE_TAG('Y', 'T', 'D', 'E'), 0.5f},
  {BL_MAKE_TAG('i', 't', 'a', 'l'), 0.3f},
  {BL_MAKE_TAG('o', 'p', 's', 'z'), 0.9f},
  {BL_MAKE_TAG('s', 'l', 'n', 't'), 0.2f},
  {BL_MAKE_TAG('w', 'd', 't', 'h'), 0.4f},
  {BL_MAKE_TAG('w', 'g', 'h', 't'), 0.0f}
};

static const BLTag invalid_tags[] = {
  0x00000000u,
  0x10101010u,
  0x5F6F7F8Fu,
  0x80808080u,
  0xFFFFFFFFu
};

// FontVariationSettings - Test Utilities
// ======================================

static void verify_content(const BLFontVariationSettings& fvs) noexcept {
  BLFontVariationSettingsView view;
  EXPECT_OK(fvs.get_view(&view));

  EXPECT_EQ(fvs.is_empty(), view.size == 0u);
  EXPECT_EQ(fvs.size(), view.size);
  EXPECT_GE(fvs.capacity(), fvs.size());

  if (view.size != 0) {
    uint32_t prev_tag = view.data[0].tag;
    for (size_t i = 1; i < view.size; i++) {
      uint32_t cur_tag = view.data[i].tag;
      EXPECT_LT(prev_tag, cur_tag)
        .message("Corrupted container: Previous tag 0x%08X is not less than current tag 0x%08X at [%zu]", prev_tag, cur_tag, i);
      prev_tag = cur_tag;
    }
  }

  // Verify SSO data is clean after the content.
  if (fvs._d.is_sso()) {
    const BLFontVariationSettings::SSO& layout = fvs._d.layout<BLFontVariationSettings::SSO>();

    for (size_t i = fvs.size(); i < BLFontVariationSettings::Info::kSSOCapacity; i++) {
      EXPECT_EQ(layout.ids[i], 0u)
        .message("Invalid SSO: Item at [%zu] has nonzero id '0x%02X'", i, layout.ids[i]);
      EXPECT_EQ(layout.values[i], 0.0f)
        .message("Invalid SSO: Item at [%zu] has nonzero value '%f'", i, static_cast<double>(layout.values[i]));
    }
  }
}

static void expect_content(const BLFontVariationSettings& fvs, Span<const BLFontVariationItem> items) noexcept {
  BLFontVariationSettingsView view;

  EXPECT_OK(fvs.get_view(&view));
  EXPECT_EQ(fvs.size(), items.size());
  EXPECT_EQ(view.size, items.size());

  for (size_t i = 0; i < items.size(); i++) {
    EXPECT_TRUE(view.data[i].tag == items[i].tag && view.data[i].value == items[i].value)
      .message("Items don't match at [%zu] - item{0x%08X, %f} != expected{0x%08X, %f}",
        i,
        view.data[i].tag,
        static_cast<double>(view.data[i].value),
        items[i].tag,
        static_cast<double>(items[i].value));
  }
}

// FontVariationSettings - Tests
// =============================

TEST_CASE(core_font_variation_settings_constructors) {
  BLFontVariationSettings fvs1;
  EXPECT_TRUE(fvs1._d.is_sso());
  EXPECT_TRUE(fvs1.is_empty());
  EXPECT_EQ(fvs1.size(), 0u);
  EXPECT_EQ(fvs1.capacity(), BLFontVariationSettings::Info::kSSOCapacity);

  EXPECT_TRUE(fvs1._d.is_sso());
  expect_content(fvs1, {nullptr, 0u});

  BLFontVariationSettings fvs2(fvs1);
  EXPECT_EQ(fvs1, fvs2);
  EXPECT_TRUE(fvs1.equals(fvs2));

  EXPECT_OK(fvs1.set_value(BL_MAKE_TAG('a', 'a', 'a', 'a'), 0.5f));
  BLFontVariationSettings fvs3(fvs1);
  EXPECT_EQ(fvs1, fvs3);

  BLFontVariationSettings fvs4(axl::move(fvs1));
  EXPECT_EQ(fvs3, fvs4);
  EXPECT_TRUE(fvs1.is_empty());
  EXPECT_EQ(fvs1, BLFontVariationSettings());

  EXPECT_OK(fvs1.assign(axl::move(fvs3)));
  EXPECT_EQ(fvs1, fvs4);
  EXPECT_EQ(fvs3, BLFontVariationSettings());

  EXPECT_OK(fvs1.reset());
  EXPECT_NE(fvs1, fvs4);

  EXPECT_OK(fvs4.reset());
  EXPECT_EQ(fvs1, fvs4);
}

TEST_CASE(core_font_variation_settings_value_ops) {
  BLFontVariationSettings fvs;

  // SSO Storage and Operations
  // --------------------------

  // Getting an unknown tag should return invalid value.
  EXPECT_TRUE(Math::is_nan(fvs.get_value(BL_MAKE_TAG('-', '-', '-', '-'))));

  for (size_t i = 0; i < BL_ARRAY_SIZE(sso_items); i++) {
    EXPECT_OK(fvs.set_value(sso_items[i].tag, sso_items[i].value));
    verify_content(fvs);
    expect_content(fvs, {sso_items, i + 1u});
  }
  EXPECT_TRUE(fvs._d.is_sso());

  for (size_t i = 0; i < BL_ARRAY_SIZE(sso_items); i++) {
    EXPECT_OK(fvs.set_value(sso_items[i].tag, sso_items_changed[i].value));
    EXPECT_EQ(fvs.get_value(sso_items[i].tag), sso_items_changed[i].value);
    EXPECT_TRUE(fvs.has_value(sso_items[i].tag));
    EXPECT_TRUE(fvs._d.is_sso());

    verify_content(fvs);
  }
  expect_content(fvs, {sso_items_changed, BL_ARRAY_SIZE(sso_items_changed)});

  EXPECT_EQ(fvs.has_value(BL_MAKE_TAG('a', 'a', 'a', 'a')), false);
  EXPECT_EQ(fvs.has_value(BL_MAKE_TAG('a', 'b', 'c', 'd')), false);
  EXPECT_EQ(fvs.remove_value(BL_MAKE_TAG(0, 0, 0, 0)), BL_ERROR_NO_ENTRY);
  EXPECT_EQ(fvs.remove_value(BL_MAKE_TAG('a', 'a', 'a', 'a')), BL_ERROR_NO_ENTRY);
  EXPECT_EQ(fvs.remove_value(BL_MAKE_TAG('z', 'z', 'z', 'z')), BL_ERROR_NO_ENTRY);

  BLFontVariationSettings copy(fvs);
  EXPECT_EQ(fvs, copy);

  for (size_t i = 0; i < BL_ARRAY_SIZE(sso_items_changed); i++) {
    EXPECT_OK(fvs.remove_value(sso_items_changed[i].tag));
    EXPECT_TRUE(Math::is_nan(fvs.get_value(sso_items_changed[i].tag)));
    EXPECT_FALSE(fvs.has_value(sso_items_changed[i].tag));
    EXPECT_EQ(fvs.remove_value(sso_items_changed[i].tag), BL_ERROR_NO_ENTRY);
    EXPECT_TRUE(fvs._d.is_sso());

    verify_content(fvs);
    expect_content(fvs, {sso_items_changed + i + 1u, BL_ARRAY_SIZE(sso_items_changed) - 1u - i});
  }

  EXPECT_OK(copy.clear());
  EXPECT_EQ(fvs, copy);
  EXPECT_EQ(fvs, BLFontVariationSettings());

  // Dynamic Storage and Operations
  // ------------------------------

  for (uint32_t variation : {0u, 1u, 2u, 3u, 4u, 5u}) {
    EXPECT_OK(fvs.reset());

    auto handle_variation = [&]() {
      switch (variation) {
        case 0: // NOP
          break;
        case 1:
          EXPECT_OK(copy.assign(fvs));
          break;
        case 2:
          EXPECT_OK(fvs.shrink());
          break;
        case 3:
          EXPECT_OK(copy.assign(fvs));
          EXPECT_OK(fvs.shrink());
          break;
        case 4:
          EXPECT_OK(copy.assign(fvs));
          EXPECT_OK(copy.clear());
          break;
        case 5:
          EXPECT_OK(copy.assign(fvs));
          EXPECT_OK(copy.set_value(BL_MAKE_TAG('a', 'b', 'c', 'd'), 0.1f));
          EXPECT_EQ(copy.get_value(BL_MAKE_TAG('a', 'b', 'c', 'd')), 0.1f);
          EXPECT_OK(copy.clear());
          break;
      }
    };

    for (size_t i = 0; i < BL_ARRAY_SIZE(dynamic_items); i++) {
      handle_variation();

      EXPECT_OK(fvs.set_value(dynamic_items[i].tag, dynamic_items[i].value));
      EXPECT_EQ(fvs.get_value(dynamic_items[i].tag), dynamic_items[i].value);
      EXPECT_TRUE(fvs.has_value(dynamic_items[i].tag));
      verify_content(fvs);
      expect_content(fvs, {dynamic_items, i + 1u});
    }

    EXPECT_FALSE(fvs._d.is_sso());
    EXPECT_EQ(fvs.remove_value(BL_MAKE_TAG(0, 0, 0, 0)), BL_ERROR_NO_ENTRY);
    EXPECT_EQ(fvs.remove_value(BL_MAKE_TAG('a', 'a', 'a', 'a')), BL_ERROR_NO_ENTRY);
    EXPECT_EQ(fvs.remove_value(BL_MAKE_TAG('z', 'z', 'z', 'z')), BL_ERROR_NO_ENTRY);

    for (size_t i = 0; i < BL_ARRAY_SIZE(dynamic_items_changed); i++) {
      handle_variation();

      EXPECT_OK(fvs.set_value(dynamic_items_changed[i].tag, dynamic_items_changed[i].value));
      EXPECT_EQ(fvs.get_value(dynamic_items_changed[i].tag), dynamic_items_changed[i].value);
      verify_content(fvs);
    }

    expect_content(fvs, {dynamic_items_changed, BL_ARRAY_SIZE(dynamic_items_changed)});
    EXPECT_FALSE(fvs._d.is_sso());

    for (size_t i = 0; i < BL_ARRAY_SIZE(dynamic_items_changed); i++) {
      handle_variation();

      EXPECT_OK(fvs.remove_value(dynamic_items_changed[i].tag));
      EXPECT_TRUE(Math::is_nan(fvs.get_value(dynamic_items_changed[i].tag)));
      EXPECT_FALSE(fvs.has_value(dynamic_items_changed[i].tag));
      verify_content(fvs);
      expect_content(fvs, {dynamic_items_changed + i + 1u, BL_ARRAY_SIZE(dynamic_items_changed) - 1u - i});
    }
  }
}

TEST_CASE(core_font_variation_settings_invalid_ops) {
  BLFontVariationItem aaaa{BL_MAKE_TAG('a', 'a', 'a', 'a'), 0.3f};

  // SSO tag handling.
  BLFontVariationSettings fvs;
  for (BLTag invalid_tag : invalid_tags) {
    EXPECT_EQ(fvs.set_value(invalid_tag, 0.5f), BL_ERROR_INVALID_VALUE);
  }
  expect_content(fvs, {nullptr, 0u});

  // Dynamic tag handling (separate code-path)
  EXPECT_OK(fvs.set_value(aaaa.tag, aaaa.value));
  expect_content(fvs, {&aaaa, 1u});
  for (BLTag invalid_tag : invalid_tags) {
    EXPECT_EQ(fvs.set_value(invalid_tag, 0.5f), BL_ERROR_INVALID_VALUE);
  }
  expect_content(fvs, {&aaaa, 1u});
}

TEST_CASE(core_font_variation_settings_equals) {
  BLFontVariationSettings fvs1;
  BLFontVariationSettings fvs2;

  EXPECT_EQ(fvs1, fvs2);

  for (size_t i = 0; i < BL_ARRAY_SIZE(sso_items); i++) {
    EXPECT_OK(fvs1.set_value(sso_items[i]));
    EXPECT_NE(fvs1, fvs2);
    EXPECT_OK(fvs2.set_value(sso_items[BL_ARRAY_SIZE(sso_items) - 1u - i]));
    EXPECT_EQ(fvs1.equals(fvs2), i == BL_ARRAY_SIZE(sso_items) - 1u);
  }

  expect_content(fvs1, {sso_items, BL_ARRAY_SIZE(sso_items)});
  expect_content(fvs2, {sso_items, BL_ARRAY_SIZE(sso_items)});
  EXPECT_TRUE(fvs1.equals(fvs2));

  for (size_t i = 0; i < BL_ARRAY_SIZE(sso_items); i++) {
    EXPECT_OK(fvs1.set_value(sso_items_changed[i]));
  }

  expect_content(fvs1, {sso_items_changed, BL_ARRAY_SIZE(sso_items_changed)});
  EXPECT_FALSE(fvs1.equals(fvs2));

  for (size_t i = 0; i < BL_ARRAY_SIZE(sso_items); i++) {
    EXPECT_OK(fvs2.set_value(sso_items_changed[i]));
  }

  expect_content(fvs2, {sso_items_changed, BL_ARRAY_SIZE(sso_items_changed)});
  EXPECT_TRUE(fvs1.equals(fvs2));

  // Dynamic vs SSO.
  EXPECT_OK(fvs1.set_value(BL_MAKE_TAG('a', 'a', 'a', 'a'), 1.0f));
  EXPECT_OK(fvs1.remove_value(BL_MAKE_TAG('a', 'a', 'a', 'a')));

  EXPECT_TRUE(fvs1.equals(fvs2));
  EXPECT_TRUE(fvs2.equals(fvs1));

  // Dynamic vs Dynamic.
  EXPECT_OK(fvs1.set_value(BL_MAKE_TAG('a', 'a', 'a', 'a'), 1.0f));
  EXPECT_FALSE(fvs1.equals(fvs2));
  EXPECT_FALSE(fvs2.equals(fvs1));

  EXPECT_OK(fvs2.set_value(BL_MAKE_TAG('a', 'a', 'a', 'a'), 1.0f));
  EXPECT_TRUE(fvs1.equals(fvs2));
  EXPECT_TRUE(fvs2.equals(fvs1));
}

TEST_CASE(core_font_variation_settings_stress) {
  BLFontVariationSettings fvs;

  constexpr uint32_t kCharRange = FontTagData::kCharRangeInTag;
  constexpr uint32_t kThreshold = FontTagData::kUniqueTagCount / 100;

  for (uint32_t i = 0; i < kThreshold; i++) {
    BLTag tag = BL_MAKE_TAG(uint32_t(' ') + (i / (kCharRange * kCharRange * kCharRange)),
                            uint32_t(' ') + (i / (kCharRange * kCharRange)) % kCharRange,
                            uint32_t(' ') + (i / (kCharRange)) % kCharRange,
                            uint32_t(' ') + (i % kCharRange));
    EXPECT_OK(fvs.set_value(tag, float(i & 0xFFFFu)));
  }

  for (uint32_t i = 0; i < kThreshold; i++) {
    BLTag tag = BL_MAKE_TAG(uint32_t(' ') + (i / (kCharRange * kCharRange * kCharRange)),
                            uint32_t(' ') + (i / (kCharRange * kCharRange)) % kCharRange,
                            uint32_t(' ') + (i / (kCharRange)) % kCharRange,
                            uint32_t(' ') + (i % kCharRange));
    EXPECT_EQ(fvs.get_value(tag), float(i & 0xFFFFu));
  }

  verify_content(fvs);
}

TEST_CASE(core_font_variation_settings_c_api_vs_cxx_api) {
  BLFontVariationSettings fvx;

  BLFontVariationSettingsCore fva;
  bl_font_variation_settings_init(&fva);

  EXPECT_TRUE(bl_font_variation_settings_equals(&fva, &fvx));
  EXPECT_EQ(memcmp(&fvx, &fvx, sizeof(BLObjectCore)), 0);

  EXPECT_EQ(bl_font_variation_settings_get_size(&fva), 0u);
  EXPECT_EQ(bl_font_variation_settings_get_capacity(&fva), BLFontVariationSettings::Info::kSSOCapacity);

  BLTag aaaa = BL_MAKE_TAG('a', 'a', 'a', 'a');

  EXPECT_OK(bl_font_variation_settings_set_value(&fva, aaaa, 10.0f));
  EXPECT_EQ(bl_font_variation_settings_get_value(&fva, aaaa), 10.0f);
  EXPECT_EQ(bl_font_variation_settings_get_size(&fva), 1u);
  EXPECT_GT(bl_font_variation_settings_get_capacity(&fva), 0u);

  BLFontVariationSettingsCore fvb;
  BLFontVariationSettingsCore fvc;

  bl_font_variation_settings_init_weak(&fvb, &fva);
  bl_font_variation_settings_init_move(&fvc, &fva);

  EXPECT_FALSE(bl_font_variation_settings_equals(&fvb, &fva));
  EXPECT_TRUE(bl_font_variation_settings_equals(&fvb, &fvc));

  bl_font_variation_settings_destroy(&fva);
  bl_font_variation_settings_destroy(&fvb);
  bl_font_variation_settings_destroy(&fvc);
}

} // {bl::FontVariationSettingsTests}

#endif // BL_TEST
