// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/core/array_p.h>
#include <blend2d/core/font_feature_settings_p.h>
#include <blend2d/core/font_tag_data_p.h>
#include <blend2d/core/object_layer_p.h>

#include <initializer_list>

namespace bl::FontFeatureSettingsTests {

// FontFeatureSettings - Test Tables
// =================================

static const BLFontFeatureItem bit_items[] = {
  {BL_MAKE_TAG('c', 'a', 's', 'e'), 1u},
  {BL_MAKE_TAG('c', 'l', 'i', 'g'), 1u},
  {BL_MAKE_TAG('c', 'p', 'c', 't'), 1u},
  {BL_MAKE_TAG('c', 'p', 's', 'p'), 1u},
  {BL_MAKE_TAG('d', 'l', 'i', 'g'), 1u},
  {BL_MAKE_TAG('d', 'n', 'o', 'm'), 1u},
  {BL_MAKE_TAG('e', 'x', 'p', 't'), 0u},
  {BL_MAKE_TAG('f', 'a', 'l', 't'), 0u},
  {BL_MAKE_TAG('f', 'r', 'a', 'c'), 0u},
  {BL_MAKE_TAG('f', 'w', 'i', 'd'), 1u},
  {BL_MAKE_TAG('h', 'a', 'l', 't'), 0u},
  {BL_MAKE_TAG('h', 'i', 's', 't'), 1u},
  {BL_MAKE_TAG('h', 'w', 'i', 'd'), 1u},
  {BL_MAKE_TAG('j', 'a', 'l', 't'), 1u},
  {BL_MAKE_TAG('k', 'e', 'r', 'n'), 1u},
  {BL_MAKE_TAG('l', 'i', 'g', 'a'), 1u},
  {BL_MAKE_TAG('l', 'n', 'u', 'm'), 1u},
  {BL_MAKE_TAG('o', 'n', 'u', 'm'), 1u},
  {BL_MAKE_TAG('o', 'r', 'd', 'n'), 1u},
  {BL_MAKE_TAG('p', 'a', 'l', 't'), 1u},
  {BL_MAKE_TAG('p', 'c', 'a', 'p'), 1u},
  {BL_MAKE_TAG('r', 'u', 'b', 'y'), 1u},
  {BL_MAKE_TAG('s', 'm', 'c', 'p'), 1u},
  {BL_MAKE_TAG('s', 'u', 'b', 's'), 0u},
  {BL_MAKE_TAG('s', 'u', 'p', 's'), 0u},
  {BL_MAKE_TAG('t', 'i', 't', 'l'), 1u},
  {BL_MAKE_TAG('t', 'n', 'a', 'm'), 1u},
  {BL_MAKE_TAG('t', 'n', 'u', 'm'), 1u},
  {BL_MAKE_TAG('u', 'n', 'i', 'c'), 1u},
  {BL_MAKE_TAG('v', 'a', 'l', 't'), 1u},
  {BL_MAKE_TAG('v', 'k', 'r', 'n'), 0u},
  {BL_MAKE_TAG('z', 'e', 'r', 'o'), 1u}
};

static constexpr size_t kBitItemCount = BL_ARRAY_SIZE(bit_items);

static const BLFontFeatureItem fat_items[] = {
  {BL_MAKE_TAG('b', 'l', 'w', 'f'), 2u},
  {BL_MAKE_TAG('b', 'l', 'w', 'm'), 3u},
  {BL_MAKE_TAG('b', 'l', 'w', 's'), 4u},
  {BL_MAKE_TAG('c', '2', 'p', 'c'), 5u},
  {BL_MAKE_TAG('c', '2', 's', 'c'), 6u},
  {BL_MAKE_TAG('c', 'a', 'l', 't'), 7u},
  {BL_MAKE_TAG('c', 'c', 'm', 'p'), 9u},
  {BL_MAKE_TAG('c', 'f', 'a', 'r'), 1u},
  {BL_MAKE_TAG('c', 'h', 'w', 's'), 4u},
  {BL_MAKE_TAG('c', 'j', 'c', 't'), 3u},
  {BL_MAKE_TAG('c', 's', 'w', 'h'), 3u},
  {BL_MAKE_TAG('c', 'u', 'r', 's'), 4u},
  {BL_MAKE_TAG('c', 'v', '0', '1'), 5u},
  {BL_MAKE_TAG('c', 'v', '0', '2'), 2u},
  {BL_MAKE_TAG('c', 'v', '0', '3'), 6u},
  {BL_MAKE_TAG('c', 'v', '0', '4'), 8u},
  {BL_MAKE_TAG('c', 'v', '0', '5'), 6u},
  {BL_MAKE_TAG('c', 'v', '0', '6'), 5u},
  {BL_MAKE_TAG('c', 'v', '0', '7'), 4u},
  {BL_MAKE_TAG('i', 'n', 'i', 't'), 5u},
  {BL_MAKE_TAG('i', 's', 'o', 'l'), 3u},
  {BL_MAKE_TAG('i', 't', 'a', 'l'), 9u},
  {BL_MAKE_TAG('j', 'p', '0', '4'), 0u},
  {BL_MAKE_TAG('j', 'p', '7', '8'), 2u},
  {BL_MAKE_TAG('j', 'p', '8', '3'), 3u},
  {BL_MAKE_TAG('j', 'p', '9', '0'), 4u},
  {BL_MAKE_TAG('l', 'f', 'b', 'd'), 5u},
  {BL_MAKE_TAG('l', 'j', 'm', 'o'), 1u},
  {BL_MAKE_TAG('l', 'o', 'c', 'l'), 2u},
  {BL_MAKE_TAG('l', 't', 'r', 'a'), 3u},
  {BL_MAKE_TAG('l', 't', 'r', 'm'), 6u},
  {BL_MAKE_TAG('m', 'a', 'r', 'k'), 7u},
  {BL_MAKE_TAG('m', 'e', 'd', '2'), 8u},
  {BL_MAKE_TAG('m', 'e', 'd', 'i'), 9u},
  {BL_MAKE_TAG('m', 'g', 'r', 'k'), 1u},
  {BL_MAKE_TAG('m', 'k', 'm', 'k'), 9u},
  {BL_MAKE_TAG('m', 's', 'e', 't'), 2u},
  {BL_MAKE_TAG('n', 'a', 'l', 't'), 3u},
  {BL_MAKE_TAG('n', 'l', 'c', 'k'), 4u},
  {BL_MAKE_TAG('n', 'u', 'k', 't'), 5u},
  {BL_MAKE_TAG('n', 'u', 'm', 'r'), 6u},
  {BL_MAKE_TAG('o', 'p', 'b', 'd'), 7u},
  {BL_MAKE_TAG('o', 'r', 'n', 'm'), 8u}
};

static constexpr size_t kFatItemCount = BL_ARRAY_SIZE(fat_items);

static const BLFontFeatureItem unknown_items[] = {
  {BL_MAKE_TAG('a', 'a', 'z', 'z'), 2u},
  {BL_MAKE_TAG('b', 'b', 'z', 'z'), 8u},
  {BL_MAKE_TAG('c', 'c', 'z', 'z'), 1u},
  {BL_MAKE_TAG('d', '0', '0', '0'), 2u},
  {BL_MAKE_TAG('z', '8', '9', '0'), 7u},
  {BL_MAKE_TAG('z', 'z', 'z', 'z'), 9u}
};

static constexpr size_t kUnknownItemCount = BL_ARRAY_SIZE(unknown_items);

static const BLTag invalid_tags[] = {
  0x00000000u,
  0x10101010u,
  0x5F6F7F8Fu,
  0x80808080u,
  0xFFFFFFFFu
};

// FontFeatureSettings - Test Utilities
// ====================================

static void verify_content(const BLFontFeatureSettings& fvs) noexcept {
  BLFontFeatureSettingsView view;
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
    const BLFontFeatureSettings::SSO& layout = fvs._d.layout<BLFontFeatureSettings::SSO>();
    for (size_t i = layout.fat_count; i < BLFontFeatureSettings::SSO::kFatCapacity; i++) {
      EXPECT_EQ(layout.fat_features[i], 0u)
        .message("Invalid SSO: Item at [%zu] has nonzero id/value pair '0x%02X'", i, layout.fat_features[i]);
    }
  }
}

static void expect_content(const BLFontFeatureSettings& ffs, Span<const BLFontFeatureItem> items) noexcept {
  verify_content(ffs);

  BLFontFeatureSettingsView view;
  EXPECT_OK(ffs.get_view(&view));
  EXPECT_EQ(ffs.size(), items.size());
  EXPECT_EQ(view.size, items.size());

  for (size_t i = 0; i < items.size(); i++) {
    EXPECT_TRUE(view.data[i].tag == items[i].tag && view.data[i].value == items[i].value)
      .message("Items don't match at [%zu] - item{0x%08X, %u} != expected{0x%08X, %u}",
        i,
        view.data[i].tag,
        view.data[i].value,
        items[i].tag,
        items[i].value);
  }
}

static void expect_contains(const BLFontFeatureSettings& ffs, Span<const BLFontFeatureItem> items) noexcept {
  for (size_t i = 0; i < items.size(); i++) {
    EXPECT_TRUE(ffs.has_value(items[i].tag))
      .message("Items don't match - the container is missing 0x%08X tag", items[i].tag);

    uint32_t value = ffs.get_value(items[i].tag);
    EXPECT_EQ(value, items[i].value)
      .message("Items don't match - the container has {0x%08X, %u}, but the expected value is %u", items[i].tag, value, items[i].value);
  }
}

// FontFeatureSettings - Tests
// ===========================

TEST_CASE(core_font_feature_settings_constructors) {
  BLFontFeatureSettings ffs1;
  EXPECT_TRUE(ffs1._d.is_sso());
  EXPECT_TRUE(ffs1.is_empty());
  EXPECT_EQ(ffs1.size(), 0u);
  EXPECT_EQ(ffs1.capacity(), BLFontFeatureSettings::Info::kSSOCapacity);

  EXPECT_TRUE(ffs1._d.is_sso());
  expect_content(ffs1, {nullptr, 0u});

  BLFontFeatureSettings ffs2(ffs1);
  EXPECT_EQ(ffs1, ffs2);
  EXPECT_TRUE(ffs1.equals(ffs2));

  EXPECT_OK(ffs1.set_value(BL_MAKE_TAG('a', 'a', 'a', 'a'), 3u));
  BLFontFeatureSettings ffs3(ffs1);
  EXPECT_EQ(ffs1, ffs3);

  BLFontFeatureSettings ffs4(axl::move(ffs1));
  EXPECT_EQ(ffs3, ffs4);
  EXPECT_TRUE(ffs1.is_empty());
  EXPECT_EQ(ffs1, BLFontFeatureSettings());

  EXPECT_OK(ffs1.assign(axl::move(ffs3)));
  EXPECT_EQ(ffs1, ffs4);
  EXPECT_EQ(ffs3, BLFontFeatureSettings());

  EXPECT_OK(ffs1.reset());
  EXPECT_NE(ffs1, ffs4);

  EXPECT_OK(ffs4.reset());
  EXPECT_EQ(ffs1, ffs4);
}

TEST_CASE(core_font_feature_settings_value_ops) {
  BLFontFeatureSettings ffs;

  // SSO Bit Items
  // -------------

  // Getting an unknown tag should return invalid value.
  EXPECT_EQ(ffs.get_value(BL_MAKE_TAG('-', '-', '-', '-')), BL_FONT_FEATURE_INVALID_VALUE);

  for (size_t i = 0; i < kBitItemCount; i++) {
    EXPECT_OK(ffs.set_value(bit_items[i].tag, bit_items[i].value));
    EXPECT_EQ(ffs.get_value(bit_items[i].tag), bit_items[i].value);
    EXPECT_TRUE(ffs.has_value(bit_items[i].tag));
    EXPECT_TRUE(ffs._d.is_sso());
    expect_content(ffs, {bit_items, i + 1u});
  }

  BLFontFeatureItem bit_items_changed[kBitItemCount];
  memcpy(&bit_items_changed, bit_items, kBitItemCount * sizeof(BLFontFeatureItem));

  for (size_t i = 0; i < kBitItemCount; i++) {
    uint32_t value = static_cast<uint32_t>(i & 1);
    EXPECT_OK(ffs.set_value(bit_items[i].tag, value));
    EXPECT_EQ(ffs.get_value(bit_items[i].tag), value);
    EXPECT_TRUE(ffs.has_value(bit_items[i].tag));
    EXPECT_TRUE(ffs._d.is_sso());

    bit_items_changed[i].value = value;
    expect_content(ffs, {bit_items_changed, kBitItemCount});
  }

  EXPECT_EQ(ffs.has_value(BL_MAKE_TAG('a', 'a', 'a', 'a')), false);
  EXPECT_EQ(ffs.has_value(BL_MAKE_TAG('a', 'b', 'c', 'd')), false);
  EXPECT_EQ(ffs.remove_value(BL_MAKE_TAG(0, 0, 0, 0)), BL_ERROR_NO_ENTRY);
  EXPECT_EQ(ffs.remove_value(BL_MAKE_TAG('a', 'a', 'a', 'a')), BL_ERROR_NO_ENTRY);
  EXPECT_EQ(ffs.remove_value(BL_MAKE_TAG('z', 'z', 'z', 'z')), BL_ERROR_NO_ENTRY);

  BLFontFeatureSettings copy(ffs);
  EXPECT_EQ(ffs, copy);

  for (size_t i = 0; i < kBitItemCount; i++) {
    EXPECT_OK(ffs.remove_value(bit_items[i].tag));
    EXPECT_EQ(ffs.get_value(bit_items[i].tag), BL_FONT_FEATURE_INVALID_VALUE);
    EXPECT_FALSE(ffs.has_value(bit_items[i].tag));
    EXPECT_EQ(ffs.remove_value(bit_items[i].tag), BL_ERROR_NO_ENTRY);
    expect_content(ffs, {bit_items_changed + i + 1u, kBitItemCount - 1u - i});
  }
  EXPECT_TRUE(ffs._d.is_sso());

  EXPECT_OK(copy.clear());
  EXPECT_EQ(ffs, copy);
  EXPECT_EQ(ffs, BLFontFeatureSettings());

  // SSO Fat Items
  // -------------

  // Tags that occupy bits can be combined with "fat" tags, which are also known,
  // but need more storage space - and have more bits reserved for their values.

  for (size_t i = 0; i < kBitItemCount; i++) {
    EXPECT_OK(ffs.set_value(bit_items[i].tag, bit_items[i].value));
  }
  expect_content(ffs, {bit_items, kBitItemCount});

  for (size_t i = 0; i < BLFontFeatureSettings::SSO::kFatCapacity; i++) {
    EXPECT_OK(ffs.set_value(fat_items[i].tag, fat_items[i].value));
    EXPECT_EQ(ffs.get_value(fat_items[i].tag), fat_items[i].value);
    EXPECT_TRUE(ffs.has_value(fat_items[i].tag));

    EXPECT_EQ(ffs.size(), kBitItemCount + i + 1u);
    EXPECT_TRUE(ffs._d.is_sso());

    verify_content(ffs);
    expect_contains(ffs, {bit_items, kBitItemCount});
    expect_contains(ffs, {fat_items, i + 1u});
  }

  for (size_t i = 0; i < BLFontFeatureSettings::SSO::kFatCapacity; i++) {
    EXPECT_OK(ffs.remove_value(fat_items[i].tag));
    EXPECT_FALSE(ffs.has_value(fat_items[i].tag));

    EXPECT_EQ(ffs.size(), kBitItemCount + BLFontFeatureSettings::SSO::kFatCapacity - 1u - i);
    EXPECT_TRUE(ffs._d.is_sso());

    verify_content(ffs);
    expect_contains(ffs, {bit_items, kBitItemCount});
    expect_contains(ffs, {fat_items + i + 1u, BLFontFeatureSettings::SSO::kFatCapacity - 1u - i});
  }
  EXPECT_EQ(ffs.size(), kBitItemCount);

  BLFontFeatureSettings ffs_initial(ffs);
  EXPECT_OK(ffs.clear());
  expect_content(ffs, {nullptr, 0u});

  // Dynamic Items
  // -------------

  for (uint32_t variation : {0u, 1u, 2u, 3u, 4u, 5u}) {
    EXPECT_OK(ffs.assign(ffs_initial));

    auto handle_variation = [&]() {
      switch (variation) {
        case 0: // NOP
          break;
        case 1:
          EXPECT_OK(copy.assign(ffs));
          break;
        case 2:
          EXPECT_OK(ffs.shrink());
          break;
        case 3:
          EXPECT_OK(copy.assign(ffs));
          EXPECT_OK(ffs.shrink());
          break;
        case 4:
          EXPECT_OK(copy.assign(ffs));
          EXPECT_OK(copy.clear());
          break;
        case 5:
          EXPECT_OK(copy.assign(ffs));
          EXPECT_OK(copy.set_value(BL_MAKE_TAG('a', 'b', 'c', 'd'), 1u));
          EXPECT_EQ(copy.get_value(BL_MAKE_TAG('a', 'b', 'c', 'd')), 1u);
          EXPECT_OK(copy.clear());
          break;
      }
    };

    for (size_t i = 0; i < kFatItemCount; i++) {
      handle_variation();

      // Update values twice - the first creates the entry, the second overwrites it.
      EXPECT_OK(ffs.set_value(fat_items[i].tag, 11u));
      EXPECT_EQ(ffs.get_value(fat_items[i].tag), 11u);

      EXPECT_OK(ffs.set_value(fat_items[i].tag, fat_items[i].value));
      EXPECT_EQ(ffs.get_value(fat_items[i].tag), fat_items[i].value);
      EXPECT_TRUE(ffs.has_value(fat_items[i].tag));
      EXPECT_EQ(ffs._d.is_sso(), i < BLFontFeatureSettings::SSO::kFatCapacity);

      verify_content(ffs);
      expect_contains(ffs, {bit_items, kBitItemCount});
      expect_contains(ffs, {fat_items, i + 1u});
    }

    EXPECT_FALSE(ffs._d.is_sso());
    EXPECT_EQ(ffs.remove_value(BL_MAKE_TAG(0, 0, 0, 0)), BL_ERROR_NO_ENTRY);
    EXPECT_EQ(ffs.remove_value(BL_MAKE_TAG('a', 'a', 'a', 'a')), BL_ERROR_NO_ENTRY);
    EXPECT_EQ(ffs.remove_value(BL_MAKE_TAG('z', 'z', 'z', 'z')), BL_ERROR_NO_ENTRY);

    EXPECT_OK(ffs.reset());
    EXPECT_OK(ffs.assign(ffs_initial));

    for (size_t i = 0; i < kUnknownItemCount; i++) {
      handle_variation();

      EXPECT_OK(ffs.set_value(unknown_items[i].tag, unknown_items[i].value));
      EXPECT_EQ(ffs.get_value(unknown_items[i].tag), unknown_items[i].value);
      EXPECT_TRUE(ffs.has_value(unknown_items[i].tag));
      EXPECT_FALSE(ffs._d.is_sso());

      verify_content(ffs);
      expect_contains(ffs, {bit_items, kBitItemCount});
      expect_contains(ffs, {unknown_items, i + 1u});
    }

    for (size_t i = 0; i < kUnknownItemCount; i++) {
      handle_variation();

      EXPECT_TRUE(ffs.has_value(unknown_items[i].tag));
      EXPECT_OK(ffs.remove_value(unknown_items[i].tag));
      EXPECT_FALSE(ffs.has_value(unknown_items[i].tag));

      verify_content(ffs);
      expect_contains(ffs, {bit_items, kBitItemCount});
      expect_contains(ffs, {unknown_items + i + 1u, kUnknownItemCount - 1u - i});
    }

    EXPECT_OK(ffs.shrink());
    EXPECT_TRUE(ffs._d.is_sso());
  }
}

TEST_CASE(core_font_feature_settings_value_ops_special) {
  static const BLFontFeatureItem items[] = {
    {BL_MAKE_TAG('c', 'a', 's', 'e'), 1u}, // SSO
    {BL_MAKE_TAG('o', 'p', 'b', 'd'), 1u}  // Fat ID, greater than SSO.
  };

  BLFontFeatureSettings ffs;

  for (BLFontFeatureItem item : items) {
    EXPECT_OK(ffs.set_value(item));
  }

  expect_contains(ffs, {items, BL_ARRAY_SIZE(items)});
}

TEST_CASE(core_font_feature_settings_invalid_ops) {
  BLFontFeatureItem aaaa{BL_MAKE_TAG('a', 'a', 'a', 'a'), 10u};

  // SSO tag handling - invalid tags.
  BLFontFeatureSettings ffs;
  for (BLTag invalid_tag : invalid_tags) {
    EXPECT_EQ(ffs.set_value(invalid_tag, 99u), BL_ERROR_INVALID_VALUE);
    expect_content(ffs, {nullptr, 0u});
  }

  // SSO tag handling - invalid values.
  EXPECT_EQ(ffs.set_value(BL_MAKE_TAG('a', 'a', 'a', 'a'), 0x10000u), BL_ERROR_INVALID_VALUE);
  expect_content(ffs, {nullptr, 0u});

  for (size_t i = 0; i < kBitItemCount; i++) {
    EXPECT_EQ(ffs.set_value(bit_items[i].tag, 2u), BL_ERROR_INVALID_VALUE);
    EXPECT_EQ(ffs.set_value(bit_items[i].tag, 0xFFFFFFFFu), BL_ERROR_INVALID_VALUE);
    expect_content(ffs, {nullptr, 0u});
  }

  // Dynamic tag handling - invalid tags.
  EXPECT_OK(ffs.set_value(aaaa.tag, aaaa.value));
  expect_content(ffs, {&aaaa, 1u});
  for (BLTag invalid_tag : invalid_tags) {
    EXPECT_EQ(ffs.set_value(invalid_tag, 98u), BL_ERROR_INVALID_VALUE);
    expect_content(ffs, {&aaaa, 1u});
  }

  // Dynamic tag handling - invalid values.
  EXPECT_EQ(ffs.set_value(BL_MAKE_TAG('a', 'a', 'a', 'a'), 0x10000u), BL_ERROR_INVALID_VALUE);
  expect_content(ffs, {&aaaa, 1u});

  for (size_t i = 0; i < kBitItemCount; i++) {
    EXPECT_EQ(ffs.set_value(bit_items[i].tag, 2u), BL_ERROR_INVALID_VALUE);
    EXPECT_EQ(ffs.set_value(bit_items[i].tag, 0xFFFFFFFFu), BL_ERROR_INVALID_VALUE);
    expect_content(ffs, {&aaaa, 1u});
  }
}

TEST_CASE(core_font_feature_settings_equals) {
  BLFontFeatureSettings ffs1;
  BLFontFeatureSettings ffs2;

  EXPECT_EQ(ffs1, ffs2);

  for (size_t i = 0; i < kBitItemCount; i++) {
    EXPECT_OK(ffs1.set_value(bit_items[i]));
    EXPECT_NE(ffs1, ffs2);

    EXPECT_OK(ffs2.set_value(bit_items[kBitItemCount - 1u - i]));
    EXPECT_EQ(ffs1.equals(ffs2), i == kBitItemCount - 1u);
  }

  expect_content(ffs1, {bit_items, kBitItemCount});
  expect_content(ffs2, {bit_items, kBitItemCount});

  EXPECT_TRUE(ffs1.equals(ffs2));
  BLFontFeatureSettings ffs_full_bits(ffs1);

  for (size_t i = 0; i < kFatItemCount; i++) {
    EXPECT_OK(ffs1.set_value(fat_items[i]));
    EXPECT_NE(ffs1, ffs2);
    EXPECT_OK(ffs2.set_value(fat_items[kFatItemCount - 1u - i]));
    EXPECT_EQ(ffs1.equals(ffs2), i == kFatItemCount - 1u);
  }

  expect_contains(ffs1, {bit_items, kBitItemCount});
  expect_contains(ffs1, {fat_items, kFatItemCount});
  expect_contains(ffs2, {bit_items, kBitItemCount});
  expect_contains(ffs2, {fat_items, kFatItemCount});
  EXPECT_TRUE(ffs1.equals(ffs2));

  EXPECT_OK(ffs1.assign(ffs_full_bits));
  EXPECT_OK(ffs2.assign(axl::move(ffs_full_bits)));

  EXPECT_EQ(ffs1, ffs2);

  // Dynamic vs SSO.
  EXPECT_OK(ffs1.set_value(BL_MAKE_TAG('a', 'a', 'a', 'a'), 1u));
  EXPECT_OK(ffs1.remove_value(BL_MAKE_TAG('a', 'a', 'a', 'a')));

  EXPECT_TRUE(ffs1.equals(ffs2));
  EXPECT_TRUE(ffs2.equals(ffs1));

  // Dynamic vs Dynamic.
  EXPECT_OK(ffs1.set_value(BL_MAKE_TAG('a', 'a', 'a', 'a'), 1u));
  EXPECT_FALSE(ffs1.equals(ffs2));
  EXPECT_FALSE(ffs2.equals(ffs1));

  EXPECT_OK(ffs2.set_value(BL_MAKE_TAG('a', 'a', 'a', 'a'), 1u));
  EXPECT_TRUE(ffs1.equals(ffs2));
  EXPECT_TRUE(ffs2.equals(ffs1));
}

TEST_CASE(core_font_feature_settings_stress) {
  BLFontFeatureSettings ffs;

  constexpr uint32_t kCharRange = FontTagData::kCharRangeInTag;
  constexpr uint32_t threshold = FontTagData::kUniqueTagCount / 100;

  for (uint32_t i = 0; i < threshold; i++) {
    BLTag tag = BL_MAKE_TAG(uint32_t(' ') + (i / (kCharRange * kCharRange * kCharRange)),
                            uint32_t(' ') + (i / (kCharRange * kCharRange)) % kCharRange,
                            uint32_t(' ') + (i / (kCharRange)) % kCharRange,
                            uint32_t(' ') + (i % kCharRange));
    ffs.set_value(tag, i & 0xFFFFu);
  }

  for (uint32_t i = 0; i < threshold; i++) {
    BLTag tag = BL_MAKE_TAG(uint32_t(' ') + (i / (kCharRange * kCharRange * kCharRange)),
                            uint32_t(' ') + (i / (kCharRange * kCharRange)) % kCharRange,
                            uint32_t(' ') + (i / (kCharRange)) % kCharRange,
                            uint32_t(' ') + (i % kCharRange));
    EXPECT_EQ(ffs.get_value(tag), i & 0xFFFFu);
  }

  verify_content(ffs);
}

TEST_CASE(core_font_feature_settings_c_api_vs_cxx_api) {
  BLFontFeatureSettings ffx;

  BLFontFeatureSettingsCore ffa;
  bl_font_feature_settings_init(&ffa);

  EXPECT_TRUE(bl_font_feature_settings_equals(&ffa, &ffx));
  EXPECT_EQ(memcmp(&ffx, &ffx, sizeof(BLObjectCore)), 0);

  EXPECT_EQ(bl_font_feature_settings_get_size(&ffa), 0u);
  EXPECT_EQ(bl_font_feature_settings_get_capacity(&ffa), BLFontFeatureSettings::Info::kSSOCapacity);

  BLTag aaaa = BL_MAKE_TAG('a', 'a', 'a', 'a');

  EXPECT_OK(bl_font_feature_settings_set_value(&ffa, aaaa, 10u));
  EXPECT_EQ(bl_font_feature_settings_get_value(&ffa, aaaa), 10u);
  EXPECT_EQ(bl_font_feature_settings_get_size(&ffa), 1u);
  EXPECT_GT(bl_font_feature_settings_get_capacity(&ffa), 0u);

  BLFontFeatureSettingsCore ffb;
  BLFontFeatureSettingsCore ffc;

  bl_font_feature_settings_init_weak(&ffb, &ffa);
  bl_font_feature_settings_init_move(&ffc, &ffa);

  EXPECT_FALSE(bl_font_feature_settings_equals(&ffb, &ffa));
  EXPECT_TRUE(bl_font_feature_settings_equals(&ffb, &ffc));

  bl_font_feature_settings_destroy(&ffa);
  bl_font_feature_settings_destroy(&ffb);
  bl_font_feature_settings_destroy(&ffc);
}

} // {bl::FontFeatureSettingsTests}

#endif // BL_TEST
