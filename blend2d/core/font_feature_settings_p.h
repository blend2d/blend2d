// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_FONT_FEATURE_SETTINGS_P_H_INCLUDED
#define BLEND2D_CORE_FONT_FEATURE_SETTINGS_P_H_INCLUDED

#include <blend2d/axl/algorithm_p.h>
#include <blend2d/axl/bitops_p.h>
#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/font_feature_settings.h>
#include <blend2d/core/font_tag_data_p.h>
#include <blend2d/core/object_layer_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl::FontFeatureSettingsDetail {

struct FeatureMatcher {
  uint32_t value = BL_FONT_FEATURE_INVALID_VALUE;

  BL_INLINE bool operator()(const BLFontFeatureItem& a, BLTag b) noexcept {
    BLTag tag = a.tag;
    if (tag == b) {
      value = a.value;
    }
    return tag < b;
  }
};
static BL_INLINE constexpr ObjectImplSize impl_size_from_capacity(size_t capacity) noexcept {
  return axl::align_up(ObjectImplSize(capacity * sizeof(BLFontFeatureItem)), ObjectLayer::kMinImplAlignment);
}

static BL_INLINE constexpr size_t capacity_from_impl_size(ObjectImplSize impl_size) noexcept {
  return size_t(impl_size) / sizeof(BLFontFeatureItem);
}

static BL_INLINE_NODEBUG bool has_sso_bit_tag(const BLFontFeatureSettings::SSO& sso, size_t index) noexcept {
  return bool((sso.bit_ids >> index) & 0x1u);
}

static BL_INLINE_NODEBUG uint32_t get_sso_bit_value(const BLFontFeatureSettings::SSO& sso, size_t index) noexcept {
  return uint32_t((sso.bit_values >> index) & 0x1u);
}

static BL_INLINE_NODEBUG uint32_t get_sso_fat_value(const BLFontFeatureSettings::SSO& sso, size_t index) noexcept {
  return sso.fat_features[index] & BLFontFeatureSettings::SSO::kFatValueMask;
}

static BL_INLINE bool find_sso_fat_feature(const BLFontFeatureSettings::SSO& sso, uint32_t feature_id, size_t* index_out) noexcept {
  size_t fat_count = sso.fat_count;
  for (size_t i = 0; i < fat_count; i++) {
    uint32_t fat_feature = sso.fat_features[i];
    if ((fat_feature >> BLFontFeatureSettings::SSO::kFatValueBits) == feature_id) {
      *index_out = i;
      return true;
    }
  }
  return false;
}

static BL_INLINE uint32_t get_sso_tag_value(const BLFontFeatureSettings::SSO& sso, BLTag feature_tag, uint32_t not_found_value = BL_FONT_FEATURE_INVALID_VALUE) noexcept {
  BL_ASSERT((sso.type_byte & BLObjectDetail::kMetaFlagD) == 0u);

  uint32_t feature_id = FontTagData::feature_tag_to_id(feature_tag);
  if (feature_id == FontTagData::kInvalidId) {
    return not_found_value;
  }

  FontTagData::FeatureInfo feature_info = FontTagData::feature_info_table[feature_id];
  if (feature_info.has_bit_id()) {
    uint32_t feature_bit_id = feature_info.bit_id;
    if (!has_sso_bit_tag(sso, feature_bit_id))
      return not_found_value;

    return get_sso_bit_value(sso, feature_bit_id);
  }
  else {
    size_t index;
    if (!find_sso_fat_feature(sso, feature_id, &index))
      return not_found_value;

    return get_sso_fat_value(sso, index);
  }
}

static BL_INLINE uint32_t get_dynamic_tag_value(const BLFontFeatureSettingsCore* self, BLTag feature_tag, uint32_t not_found_value = BL_FONT_FEATURE_INVALID_VALUE) noexcept {
  BL_ASSERT(self->_d.is_dynamic());

  FeatureMatcher m{not_found_value};
  ObjectLayer::DataSpan<const BLFontFeatureItem> items = ObjectLayer::object_dyn_span<const BLFontFeatureItem>(self);

  axl::maybe_unused(lower_bound(items.data, items.size, feature_tag, m));
  return m.value;
}

template<bool is_sso>
static BL_INLINE uint32_t get_tag_value(const BLFontFeatureSettingsCore* self, BLTag feature_tag, uint32_t not_found_value = BL_FONT_FEATURE_INVALID_VALUE) noexcept {
  if constexpr (is_sso) {
    return get_sso_tag_value(self->_d.layout<BLFontFeatureSettings::SSO>(), feature_tag, not_found_value);
  }
  else {
    return get_dynamic_tag_value(self, feature_tag, not_found_value);
  }
}

template<bool is_sso>
static BL_INLINE bool is_enabled_for_plan(const BLFontFeatureSettingsCore* self, BLTag feature_tag) noexcept {
  uint32_t feature_id = FontTagData::feature_tag_to_id(feature_tag);
  uint32_t feature_info_index = bl_min<uint32_t>(feature_id, FontTagData::kFeatureIdCount);
  const FontTagData::FeatureInfo& feature_info = FontTagData::feature_info_table[feature_info_index];

  return get_tag_value<is_sso>(self, feature_tag, feature_info.enabled_by_default) > 0u;
}

} // {bl::FontFeatureSettingsDetail}

namespace bl {

template<>
struct ObjectDispatchInfo<BLFontFeatureSettingsCore> {
  using ImplType = BLFontFeatureSettingsImpl;

  static constexpr bool kHasInitSSO = true;
  static constexpr size_t kImplDataOffset = 0;

  static BL_INLINE void init_sso(BLFontFeatureSettingsCore* self) noexcept { ObjectLayer::init_sso(self); }

  static BL_INLINE BLResult free_object(BLFontFeatureSettingsCore* self, ImplType* impl, ObjectLayer::MetaByte meta_byte) noexcept {
    axl::maybe_unused(self);
    return dealloc_impl(impl, meta_byte);
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_FONT_FEATURE_SETTINGS_P_H_INCLUDED
