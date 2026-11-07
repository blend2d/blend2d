// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/algorithm_p.h>
#include <blend2d/axl/commons.h>
#include <blend2d/axl/bitops_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/font_feature_settings_p.h>
#include <blend2d/core/font_tag_data_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/core/string_p.h>

namespace bl::FontFeatureSettingsDetail {

using FontTagData::FeatureInfo;
using FatBitOps = ParametrizedBitOps<BitOrder::kLSB, uint32_t>;

// FontFeatureSettings - SSO Utilities
// ===================================

static BL_INLINE void font_features_set_sso_bit_feature(BLFontFeatureSettings::SSO& sso, size_t index, uint32_t value) noexcept {
  uint64_t bit = uint64_t(1u) << index;
  uint64_t value_bit = uint64_t(value) << index;
  size_t non_existing = size_t((sso.bit_ids & bit) == 0u);

  sso.bit_ids |= bit;
  sso.bit_values = (sso.bit_values & ~bit) | value_bit;
  sso.size = uint8_t(sso.size + non_existing);
}

static BL_INLINE bool font_features_unset_sso_bit_feature(BLFontFeatureSettings::SSO& sso, size_t index) noexcept {
  uint64_t bit = axl::bit_mask<uint64_t>(index);
  bool existing = (sso.bit_ids & bit) != 0u;

  sso.bit_ids &= ~bit;
  sso.bit_values &= ~bit;
  sso.size = uint8_t(sso.size - static_cast<uint32_t>(existing));
  return existing;
}

static BL_INLINE bool font_features_set_sso_fat_feature(BLFontFeatureSettings::SSO& sso, uint32_t feature_id, uint32_t feature_value) noexcept {
  BL_ASSERT(feature_value <= BLFontFeatureSettings::SSO::kFatValueMask);

  size_t i = 0;
  size_t fat_count = sso.fat_count;
  uint32_t existing_id = 0xFFFFFFFFu;

  BL_NOUNROLL
  while (i < fat_count) {
    existing_id = sso.fat_features[i] >> BLFontFeatureSettings::SSO::kFatValueBits;
    if (existing_id >= feature_id) {
      break;
    }
    i++;
  }

  // Update existing fat feature when matched.
  uint32_t id_and_value = (feature_id << BLFontFeatureSettings::SSO::kFatValueBits) | feature_value;
  if (existing_id == feature_id) {
    sso.fat_features[i] = uint16_t(id_and_value);
    return true;
  }

  // Insert a new fat feature at `i`. We have to handle only one possible failure case when `fat_count == kFatCapacity`.
  if (fat_count == BLFontFeatureSettings::SSO::kFatCapacity) {
    return false;
  }

  BL_NOUNROLL
  while (i < fat_count) {
    uint32_t tmp = sso.fat_features[i];
    sso.fat_features[i] = uint16_t(id_and_value);
    id_and_value = tmp;
    i++;
  }

  sso.fat_features[fat_count] = uint16_t(id_and_value);
  sso.fat_count = uint8_t(fat_count + 1u);
  sso.size = uint8_t(sso.size + 1u);

  return true;
}

static BL_INLINE void font_features_remove_sso_fat_feature(BLFontFeatureSettings::SSO& sso, size_t index) noexcept {
  BL_ASSERT(index < sso.fat_count);
  BL_ASSERT(index < BLFontFeatureSettings::SSO::kFatCapacity);

  uint32_t fat_feature = 0u;
  size_t i = sso.fat_count;

  sso.fat_count = uint8_t(i - 1u);
  sso.size = uint8_t(sso.size - 1u);

  BL_NOUNROLL
  while (i > index) {
    i--;
    uint32_t tmp = sso.fat_features[i];
    sso.fat_features[i] = uint16_t(fat_feature);
    fat_feature = tmp;
  }
}

static bool font_features_to_sso(BLFontFeatureSettings::SSO& sso, const BLFontFeatureItem* items, size_t size) noexcept {
  BL_ASSERT(size <= BLFontFeatureSettings::Info::kSSOCapacity);

  uint64_t bit_ids = 0;
  uint64_t bit_values = 0;
  size_t fat_count = 0;

  BL_NOUNROLL
  for (size_t i = 0; i < BLFontFeatureSettings::SSO::kFatCapacity; i++) {
    sso.fat_features[i] = 0;
  }

  for (size_t i = 0; i < size; i++) {
    uint32_t id = FontTagData::feature_tag_to_id(items[i].tag);
    uint32_t value = items[i].value;

    if (id == FontTagData::kInvalidId) {
      return false;
    }

    FeatureInfo feature_info = FontTagData::feature_info_table[id];
    if (feature_info.has_bit_id()) {
      // `value > 1` should never happen as we restrict bit-tag features at API boundaries.
      BL_ASSERT(value <= 1u);

      size_t bit_id = feature_info.bit_id;
      bit_ids |= uint64_t(1) << bit_id;
      bit_values |= uint64_t(value) << bit_id;
    }
    else {
      if (value > BLFontFeatureSettings::SSO::kFatValueMask || fat_count == BLFontFeatureSettings::SSO::kFatCapacity) {
        return false;
      }

      sso.fat_features[fat_count++] = uint16_t((id << BLFontFeatureSettings::SSO::kFatValueBits) | value);
    }
  }

  sso.bit_ids = bit_ids;
  sso.bit_values = bit_values;
  sso.size = uint8_t(size);
  sso.fat_count = uint8_t(fat_count);
  sso.type_byte = uint8_t(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS);
  sso.meta_byte = uint8_t(0);

  return true;
}

static void font_features_decompose_sso(const BLFontFeatureSettings::SSO& sso, BLFontFeatureItem* item_data) noexcept {
  constexpr uint32_t null_id = 0xFFFFFFFFu;

  BLFontFeatureItem* item_ptr = item_data;
  uint64_t bit_ids = sso.bit_ids;
  uint64_t bit_values = sso.bit_values;
  uint32_t fat_feature_id = null_id;
  uint32_t fat_feature_value = BL_FONT_FEATURE_INVALID_VALUE;

  size_t fat_index = 0;
  size_t fat_count = sso.fat_count;

  if (fat_count) {
    fat_feature_id = uint32_t(sso.fat_features[fat_index]) >> BLFontFeatureSettings::SSO::kFatValueBits;
    fat_feature_value = uint32_t(sso.fat_features[fat_index]) & BLFontFeatureSettings::SSO::kFatValueMask;
  }

  ParametrizedBitOps<BitOrder::kLSB, uint64_t>::BitIterator bit_iterator(bit_ids);
  while (bit_iterator.has_next()) {
    size_t bit_index = bit_iterator.next();
    uint32_t bit_feature_id = uint32_t(FontTagData::feature_bit_id_to_feature_id(bit_index));

    while (bit_feature_id > fat_feature_id) {
      *item_ptr++ = {FontTagData::feature_id_to_tag_table[fat_feature_id], fat_feature_value};
      fat_feature_id = null_id;
      fat_feature_value = BL_FONT_FEATURE_INVALID_VALUE;

      if (++fat_index < fat_count) {
        fat_feature_id = uint32_t(sso.fat_features[fat_index]) >> BLFontFeatureSettings::SSO::kFatValueBits;
        fat_feature_value = uint32_t(sso.fat_features[fat_index]) & BLFontFeatureSettings::SSO::kFatValueMask;
      }
    }

    *item_ptr++ = {FontTagData::feature_id_to_tag_table[bit_feature_id], uint32_t((bit_values >> bit_index) & 0x1u)};
  }

  while (fat_index < fat_count) {
    fat_feature_id = uint32_t(sso.fat_features[fat_index]) >> BLFontFeatureSettings::SSO::kFatValueBits;
    fat_feature_value = uint32_t(sso.fat_features[fat_index]) & BLFontFeatureSettings::SSO::kFatValueMask;

    *item_ptr++ = {FontTagData::feature_id_to_tag_table[fat_feature_id], fat_feature_value};
    fat_index++;
  }

  BL_ASSERT(item_data + sso.size == item_ptr);
}

// FontFeatureSettings - Impl Utilities
// ====================================

static BL_INLINE BLResult font_features_init_dynamic(BLFontFeatureSettingsCore* self, ObjectImplSize impl_size, size_t size = 0u) noexcept {
  return ObjectLayer::alloc_container_impl(self, impl_size, {
    .object_type = BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS,
    .data_offset = ObjectLayer::data_offset_of<BLFontFeatureSettingsCore>,
    .size = size,
    .capacity = capacity_from_impl_size(impl_size)
  });
}

static BL_INLINE BLResult font_features_init_dynamic_with_data(BLFontFeatureSettingsCore* self, ObjectImplSize impl_size, size_t size, const BLFontFeatureItem* items, size_t item_count) noexcept {
  return ObjectLayer::alloc_container_impl_with_data(self, impl_size, {
    .object_type = BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS,
    .data_offset = ObjectLayer::data_offset_of<BLFontFeatureSettingsCore>,
    .size = size,
    .capacity = capacity_from_impl_size(impl_size),
    .src_data = items,
    .src_data_size = item_count * sizeof(BLFontFeatureItem)
  });
}

static BL_NOINLINE BLResult font_features_init_dynamic_from_sso(BLFontFeatureSettingsCore* self, ObjectImplSize impl_size, const BLFontFeatureSettings::SSO& sso) noexcept {
  BL_PROPAGATE(font_features_init_dynamic(self, impl_size, sso.size));
  font_features_decompose_sso(sso, ObjectLayer::dyn_data<BLFontFeatureItem>(self));
  return BL_SUCCESS;
}

} // {bl::FontFeatureSettingsDetail}

// FontFeatureSettings - API - Init & Destroy
// ==========================================

BL_API_IMPL BLResult bl_font_feature_settings_init(BLFontFeatureSettingsCore* self) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);
  return ObjectLayer::init_object(self);
}

BL_API_IMPL BLResult bl_font_feature_settings_init_move(BLFontFeatureSettingsCore* self, BLFontFeatureSettingsCore* other) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_font_feature_settings_init_weak(BLFontFeatureSettingsCore* self, const BLFontFeatureSettingsCore* other) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object(self, other);
}

BL_API_IMPL BLResult bl_font_feature_settings_destroy(BLFontFeatureSettingsCore* self) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object<ObjectLayer::ObjectMode::kAny>(self);
}

// FontFeatureSettings - API - Reset & Clear
// =========================================

BL_API_IMPL BLResult bl_font_feature_settings_reset(BLFontFeatureSettingsCore* self) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

BL_API_IMPL BLResult bl_font_feature_settings_clear(BLFontFeatureSettingsCore* self) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_ref_counted()) {
    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kRC>(self)) {
      ObjectLayer::set_dyn_size(self, 0u);
      return BL_SUCCESS;
    }
    ObjectLayer::release_object<ObjectLayer::ObjectMode::kRC>(self);
  }

  return ObjectLayer::init_sso(self);
}

// FontFeatureSettings - API - Shrink
// ==================================

BL_API_IMPL BLResult bl_font_feature_settings_shrink(BLFontFeatureSettingsCore* self) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (!self->_d.is_ref_counted()) {
    return BL_SUCCESS;
  }

  ObjectLayer::DataSpan<const BLFontFeatureItem> data = ObjectLayer::object_dyn_span<const BLFontFeatureItem>(self);

  BLFontFeatureSettingsCore tmp;
  if (data.size <= BLFontFeatureSettings::Info::kSSOCapacity && font_features_to_sso(tmp._d.layout<BLFontFeatureSettings::SSO>(), data.data, data.size)) {
    return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kRC>(self, &tmp);
  }

  ObjectImplSize fitting_impl_size = impl_size_from_capacity(data.size);
  ObjectImplSize current_impl_size = impl_size_from_capacity(ObjectLayer::dyn_capacity(self));

  if (fitting_impl_size == current_impl_size) {
    return BL_SUCCESS;
  }

  BL_PROPAGATE(font_features_init_dynamic_with_data(&tmp, fitting_impl_size, data.size, data.data, data.size));
  return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kRC>(self, &tmp);
}

// FontFeatureSettings - API - Assign
// ==================================

BL_API_IMPL BLResult bl_font_feature_settings_assign_move(BLFontFeatureSettingsCore* self, BLFontFeatureSettingsCore* other) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_font_feature_settings_assign_weak(BLFontFeatureSettingsCore* self, const BLFontFeatureSettingsCore* other) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

// FontFeatureSettings - API - Accessors
// =====================================

BL_API_IMPL size_t bl_font_feature_settings_get_size(const BLFontFeatureSettingsCore* self) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_size(self);
}

BL_API_IMPL size_t bl_font_feature_settings_get_capacity(const BLFontFeatureSettingsCore* self) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_capacity(self);
}

BL_API_IMPL BLResult bl_font_feature_settings_get_view(const BLFontFeatureSettingsCore* self, BLFontFeatureSettingsView* out) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_sso()) {
    BLFontFeatureItem* items = out->_decomposed_sso_data;
    size_t size = ObjectLayer::sso_size(self);

    out->data = items;
    out->size = size;

    if (!size) {
      return BL_SUCCESS;
    }

    font_features_decompose_sso(self->_d.layout<BLFontFeatureSettings::SSO>(), items);
    return BL_SUCCESS;
  }
  else {
    out->data = ObjectLayer::dyn_data<BLFontFeatureItem>(self);
    out->size = ObjectLayer::dyn_size(self);
    return BL_SUCCESS;
  }
}

BL_API_IMPL bool bl_font_feature_settings_has_value(const BLFontFeatureSettingsCore* self, BLTag feature_tag) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_sso()) {
    uint32_t feature_id = FontTagData::feature_tag_to_id(feature_tag);
    if (feature_id == FontTagData::kInvalidId) {
      return false;
    }

    FontTagData::FeatureInfo feature_info = FontTagData::feature_info_table[feature_id];
    if (feature_info.has_bit_id()) {
      return has_sso_bit_tag(self->_d.layout<BLFontFeatureSettings::SSO>(), feature_info.bit_id);
    }
    else {
      size_t dummy_index;
      return find_sso_fat_feature(self->_d.layout<BLFontFeatureSettings::SSO>(), feature_id, &dummy_index);
    }
  }
  else {
    const BLFontFeatureItem* data = ObjectLayer::dyn_data<BLFontFeatureItem>(self);
    size_t size = ObjectLayer::dyn_size(self);

    size_t index = lower_bound(data, size, feature_tag, [](const BLFontFeatureItem& item, uint32_t tag) noexcept { return item.tag < tag; });
    return index < size && data[index].tag == feature_tag;
  }
}

BL_API_IMPL uint32_t bl_font_feature_settings_get_value(const BLFontFeatureSettingsCore* self, BLTag feature_tag) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_sso())
    return get_tag_value<true>(self, feature_tag);
  else
    return get_tag_value<false>(self, feature_tag);
}

BL_API_IMPL BLResult bl_font_feature_settings_set_value(BLFontFeatureSettingsCore* self, BLTag feature_tag, uint32_t value) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  uint32_t feature_id = FontTagData::feature_tag_to_id(feature_tag);
  uint32_t max_value = 0xFFFFu;

  size_t size;
  if (self->_d.is_sso()) {
    BLFontFeatureSettings::SSO& sso = self->_d.layout<BLFontFeatureSettings::SSO>();
    size = sso.size;

    if (feature_id != FontTagData::kInvalidId) {
      FontTagData::FeatureInfo feature_info = FontTagData::feature_info_table[feature_id];
      if (feature_info.has_bit_id()) {
        if (value > 1u) {
          return bl_make_error(BL_ERROR_INVALID_VALUE);
        }

        size_t feature_bit_id = feature_info.bit_id;
        font_features_set_sso_bit_feature(sso, feature_bit_id, value);
        return BL_SUCCESS;
      }

      if (value <= BLFontFeatureSettings::SSO::kFatValueMask) {
        if (font_features_set_sso_fat_feature(sso, feature_id, value)) {
          return BL_SUCCESS;
        }
      }
    }
    else {
      if (BL_UNLIKELY(!FontTagData::is_valid_tag(feature_tag))) {
        return bl_make_error(BL_ERROR_INVALID_VALUE);
      }
    }

    if (BL_UNLIKELY(value > max_value)) {
      return bl_make_error(BL_ERROR_INVALID_VALUE);
    }

    // Turn the SSO settings to dynamic settings, because some (or multiple) cases below are true:
    //   a) The `feature_tag` doesn't have a corresponding feature id, thus it cannot be used in SSO mode.
    //   b) The `value` is not either 0 or 1.
    //   c) There is no room in SSO storage to insert another tag/value pair.
    ObjectImplSize impl_size = impl_size_from_capacity(bl_max<size_t>(size + 1u, 16u));
    BLFontFeatureSettingsCore tmp;

    // NOTE: This will turn the SSO settings into dynamic settings - it's guaranteed that all further operations will succeed.
    BL_PROPAGATE(font_features_init_dynamic_from_sso(&tmp, impl_size, sso));
    self->_d = tmp._d;
  }
  else {
    size = ObjectLayer::dyn_size(self);

    if (feature_id != FontTagData::kInvalidId) {
      FontTagData::FeatureInfo feature_info = FontTagData::feature_info_table[feature_id];
      max_value = feature_info.has_bit_id() ? 0x1u : 0xFFFFu;
    }

    if (BL_UNLIKELY(axl::bool_or(!FontTagData::is_valid_tag(feature_tag), value > max_value))) {
      return bl_make_error(BL_ERROR_INVALID_VALUE);
    }
  }

  BLFontFeatureItem* items = ObjectLayer::dyn_data<BLFontFeatureItem>(self);
  size_t index = lower_bound(items, size, feature_tag, [](const BLFontFeatureItem& item, uint32_t tag) noexcept { return item.tag < tag; });
  size_t non_existing = index >= size || items[index].tag != feature_tag;

  if (ObjectLayer::check_dynamic_capacity_and_mutability(self, non_existing, size)) {
    if (!non_existing) {
      items[index].value = value;
      return BL_SUCCESS;
    }
    else {
      ObjectLayer::set_dyn_size(self, size + 1u);
      axl::copy_reverse(items + index + 1u, items + index, size - index);
      items[index] = {feature_tag, value};
      return BL_SUCCESS;
    }
  }

  size_t result_size = size + non_existing;
  ObjectImplSize impl_size = ObjectLayer::expand_impl_size(impl_size_from_capacity(result_size));

  BLFontFeatureSettingsCore tmp;
  BL_PROPAGATE(font_features_init_dynamic_with_data(&tmp, impl_size, result_size, items, index));

  BLFontFeatureItem* dst = ObjectLayer::dyn_data<BLFontFeatureItem>(&tmp);
  bl::axl::copy_forward(dst + index + non_existing, items + index, size - index);
  dst[index] = {feature_tag, value};

  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_font_feature_settings_remove_value(BLFontFeatureSettingsCore* self, BLTag feature_tag) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_sso()) {
    BLFontFeatureSettings::SSO& sso = self->_d.layout<BLFontFeatureSettings::SSO>();
    uint32_t feature_id = FontTagData::feature_tag_to_id(feature_tag);

    if (feature_id == FontTagData::kInvalidId) {
      return bl_make_error(BL_ERROR_NO_ENTRY);
    }

    FontTagData::FeatureInfo feature_info = FontTagData::feature_info_table[feature_id];
    if (feature_info.has_bit_id()) {
      size_t feature_bit_id = feature_info.bit_id;
      return font_features_unset_sso_bit_feature(sso, feature_bit_id) ? BL_SUCCESS : bl_make_error(BL_ERROR_NO_ENTRY);
    }
    else {
      size_t index;
      if (!find_sso_fat_feature(sso, feature_id, &index)) {
        return bl_make_error(BL_ERROR_NO_ENTRY);
      }

      font_features_remove_sso_fat_feature(sso, index);
      return BL_SUCCESS;
    }
  }
  else {
    BLFontFeatureItem* items = ObjectLayer::dyn_data<BLFontFeatureItem>(self);
    size_t size = ObjectLayer::dyn_size(self);
    size_t index = lower_bound(items, size, feature_tag, [](const BLFontFeatureItem& item, uint32_t tag) noexcept { return item.tag < tag; });

    if (index >= size || items[index].tag != feature_tag) {
      return bl_make_error(BL_ERROR_NO_ENTRY);
    }

    if (!ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self)) {
      BLFontFeatureSettingsCore tmp;
      BL_PROPAGATE(font_features_init_dynamic_with_data(&tmp, impl_size_from_capacity(size), size, items, size));

      items = ObjectLayer::dyn_data<BLFontFeatureItem>(&tmp);
      ObjectLayer::replace_object(self, &tmp);
    }

    ObjectLayer::set_dyn_size(self, size - 1u);
    axl::copy_forward(items + index, items + index + 1u, size - index - 1u);
    return BL_SUCCESS;
  }
}

// FontFeatureSettings - API - Equals
// ==================================

BL_API_IMPL bool bl_font_feature_settings_equals(const BLFontFeatureSettingsCore* a, const BLFontFeatureSettingsCore* b) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);

  BL_ASSERT(a->_d.is_font_feature_settings());
  BL_ASSERT(b->_d.is_font_feature_settings());

  if (a->_d == b->_d) {
    return true;
  }

  if (a->_d.is_sso() == b->_d.is_sso()) {
    // Both are SSO: They must be binary equal, if not, they are not equal.
    if (a->_d.is_sso()) {
      return false;
    }

    // Both are dynamic.
    ObjectLayer::DataSpan<const BLFontFeatureItem> a_features = ObjectLayer::object_dyn_span<const BLFontFeatureItem>(a);
    ObjectLayer::DataSpan<const BLFontFeatureItem> b_features = ObjectLayer::object_dyn_span<const BLFontFeatureItem>(b);

    return a_features.size == b_features.size && memcmp(a_features.data, b_features.data, a_features.size * sizeof(BLFontFeatureItem)) == 0;
  }
  else {
    // One is SSO and one is dynamic, make `a` the SSO one.
    if (b->_d.is_sso()) {
      axl::swap(a, b);
    }

    ObjectLayer::DataSpan<const BLFontFeatureItem> b_features = ObjectLayer::object_dyn_span<const BLFontFeatureItem>(b);
    size_t size = ObjectLayer::sso_size(a);

    if (size != b_features.size) {
      return false;
    }

    // NOTE: Since SSO representation is not that trivial, just try to convert B impl to SSO representation
    // and then try binary equality of two SSO instances. If B is not convertible, then A and B are not equal.
    BLFontFeatureSettingsCore b_object;

    BL_ASSERT(size <= BLFontFeatureSettings::Info::kSSOCapacity);
    if (!font_features_to_sso(b_object._d.layout<BLFontFeatureSettings::SSO>(), b_features.data, size)) {
      return false;
    }

    return a->_d == b_object._d;
  }
}

// FontFeatureSettings - Runtime Registration
// ==========================================

void bl_font_feature_settings_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  BL_USING_DETAIL(bl::FontFeatureSettingsDetail);
  static constexpr BLObjectType object_type = BLFontFeatureSettings::Info::kType;

  ObjectLayer::init_sso(static_cast<BLFontFeatureSettingsCore*>(&bl_object_defaults[object_type]));
}
