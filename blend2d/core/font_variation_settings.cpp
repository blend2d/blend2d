// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/algorithm_p.h>
#include <blend2d/axl/commons.h>
#include <blend2d/axl/math_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/font_tag_data_p.h>
#include <blend2d/core/font_variation_settings_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/core/string_p.h>

namespace bl::FontVariationSettingsDetail {

// FontVariationSettings - SSO Utilities
// =====================================

static BL_INLINE bool font_variations_find_sso_tag(const BLFontVariationSettings::SSO& sso, uint32_t variation_id, size_t* index_out) noexcept {
  size_t i;
  size_t size = sso.size;
  uint32_t sso_id = 0xFFFFFFFFu;

  for (i = 0; i < size; i++) {
    sso_id = sso.ids[i];
    if (sso_id >= variation_id) {
      break;
    }
  }

  *index_out = i;
  return sso_id == variation_id;
}

static bool font_variations_to_sso(BLFontVariationSettings::SSO& sso, const BLFontVariationItem* items, size_t size) noexcept {
  BL_ASSERT(size <= BLFontVariationSettings::Info::kSSOCapacity);

  memset(&sso, 0, sizeof(sso));

  for (size_t i = 0; i < size; i++) {
    uint32_t variation_id = FontTagData::variation_tag_to_id(items[i].tag);
    float variation_value = items[i].value;

    if (variation_id == FontTagData::kInvalidId) {
      return false;
    }

    sso.ids[i] = uint8_t(variation_id);
    sso.values[i] = variation_value;
  }

  sso.type_byte = uint8_t(BLFontVariationSettings::Info::kType);
  sso.size = uint8_t(size);
  return true;
}

static BL_INLINE void font_variations_decompose_sso(const BLFontVariationSettings::SSO& sso, BLFontVariationItem* item_data) noexcept {
  size_t size = sso.size;
  for (size_t i = 0; i < size; i++) {
    item_data[i] = BLFontVariationItem{FontTagData::variation_id_to_tag_table[sso.ids[i]], sso.values[i]};
  }
}

static BL_INLINE bool font_variations_set_sso_variation(BLFontVariationSettings::SSO& sso, uint32_t variation_id, float variation_value) noexcept {
  size_t i = 0;
  size_t size = sso.size;
  uint32_t existing_id = 0xFFFFFFFFu;

  BL_NOUNROLL
  while (i < size) {
    existing_id = sso.ids[i];
    if (existing_id >= variation_id) {
      break;
    }
    i++;
  }

  // Update existing variation when matched.
  if (existing_id == variation_id) {
    sso.values[i] = variation_value;
    return true;
  }

  // Insert a new variation at `i`. We have to handle only one possible failure case when `i == sso_capacity`.
  if (size == BLFontVariationSettings::Info::kSSOCapacity) {
    return false;
  }

  BL_NOUNROLL
  while (i < size) {
    uint32_t tmp_id = sso.ids[i];
    float tmp_value = sso.values[i];

    sso.ids[i] = uint8_t(variation_id);
    sso.values[i] = variation_value;

    variation_id = tmp_id;
    variation_value = tmp_value;

    i++;
  }

  sso.ids[size] = uint8_t(variation_id);
  sso.values[size] = variation_value;

  sso.size = uint8_t(size + 1u);
  return true;
}

static BL_INLINE void font_variations_remove_sso_variation(BLFontVariationSettings::SSO& sso, size_t index) noexcept {
  BL_ASSERT(index < sso.size);
  BL_ASSERT(index < BLFontVariationSettings::Info::kSSOCapacity);

  uint32_t variation_id = 0u;
  float variation_value = 0.0f;

  size_t i = sso.size;
  sso.size = uint8_t(i - 1u);

  BL_NOUNROLL
  while (i > index) {
    i--;

    uint32_t tmp_id = sso.ids[i];
    float tmp_value = sso.values[i];

    sso.ids[i] = uint8_t(variation_id);
    sso.values[i] = variation_value;

    variation_id = tmp_id;
    variation_value = tmp_value;
  }
}

// FontVariationSettings - Alloc & Free
// ====================================

static BL_INLINE BLResult font_variations_init_dynamic(BLFontVariationSettingsCore* self, ObjectImplSize impl_size, size_t size = 0u) noexcept {
  return ObjectLayer::alloc_container_impl(self, impl_size, {
    .object_type = BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS,
    .data_offset = ObjectLayer::data_offset_of<BLFontVariationSettingsCore>,
    .size = size,
    .capacity = capacity_from_impl_size(impl_size)
  });
}

static BL_INLINE BLResult font_variations_init_dynamic_with_data(BLFontVariationSettingsCore* self, ObjectImplSize impl_size, size_t size, const BLFontVariationItem* items, size_t item_count) noexcept {
  return ObjectLayer::alloc_container_impl_with_data(self, impl_size, {
    .object_type = BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS,
    .data_offset = ObjectLayer::data_offset_of<BLFontVariationSettingsCore>,
    .size = size,
    .capacity = capacity_from_impl_size(impl_size),
    .src_data = items,
    .src_data_size = item_count * sizeof(BLFontVariationItem)
  });
}

static BL_NOINLINE BLResult font_variations_init_dynamic_from_sso(BLFontVariationSettingsCore* self, ObjectImplSize impl_size, const BLFontVariationSettings::SSO& sso) noexcept {
  BL_PROPAGATE(font_variations_init_dynamic(self, impl_size, sso.size));
  font_variations_decompose_sso(sso, ObjectLayer::dyn_data<BLFontVariationItem>(self));
  return BL_SUCCESS;
}

} // {bl::FontVariationSettingsDetail}

// FontVariationSettings - API - Init & Destroy
// ============================================

BL_API_IMPL BLResult bl_font_variation_settings_init(BLFontVariationSettingsCore* self) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);

  return ObjectLayer::init_object(self);
}

BL_API_IMPL BLResult bl_font_variation_settings_init_move(BLFontVariationSettingsCore* self, BLFontVariationSettingsCore* other) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_font_variation_settings_init_weak(BLFontVariationSettingsCore* self, const BLFontVariationSettingsCore* other) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object(self, other);
}

BL_API_IMPL BLResult bl_font_variation_settings_destroy(BLFontVariationSettingsCore* self) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object(self);
}

// FontVariationSettings - API - Reset & Clear
// ===========================================

BL_API_IMPL BLResult bl_font_variation_settings_reset(BLFontVariationSettingsCore* self) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

BL_API_IMPL BLResult bl_font_variation_settings_clear(BLFontVariationSettingsCore* self) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);
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

// FontVariationSettings - API - Shrink
// ====================================

BL_API_IMPL BLResult bl_font_variation_settings_shrink(BLFontVariationSettingsCore* self) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (!self->_d.is_ref_counted()) {
    return BL_SUCCESS;
  }

  ObjectLayer::DataSpan<const BLFontVariationItem> data = ObjectLayer::object_dyn_span<const BLFontVariationItem>(self);

  BLFontVariationSettingsCore tmp;
  if (data.size <= BLFontVariationSettings::Info::kSSOCapacity &&
      font_variations_to_sso(tmp._d.layout<BLFontVariationSettings::SSO>(), data.data, data.size)) {
    return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kRC>(self, &tmp);
  }

  ObjectImplSize fitting_impl_size = impl_size_from_capacity(data.size);
  ObjectImplSize current_impl_size = impl_size_from_capacity(ObjectLayer::dyn_capacity(self));

  if (fitting_impl_size == current_impl_size) {
    return BL_SUCCESS;
  }

  BL_PROPAGATE(font_variations_init_dynamic_with_data(&tmp, fitting_impl_size, data.size, data.data, data.size));
  return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kRC>(self, &tmp);
}

// FontVariationSettings - API - Assign
// ====================================

BL_API_IMPL BLResult bl_font_variation_settings_assign_move(BLFontVariationSettingsCore* self, BLFontVariationSettingsCore* other) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_font_variation_settings_assign_weak(BLFontVariationSettingsCore* self, const BLFontVariationSettingsCore* other) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

// FontVariationSettings - API - Accessors
// =======================================

BL_API_IMPL size_t bl_font_variation_settings_get_size(const BLFontVariationSettingsCore* self) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_size(self);
}

BL_API_IMPL size_t bl_font_variation_settings_get_capacity(const BLFontVariationSettingsCore* self) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_capacity(self);
}

BL_API_IMPL BLResult bl_font_variation_settings_get_view(const BLFontVariationSettingsCore* self, BLFontVariationSettingsView* out) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_sso()) {
    BLFontVariationItem* items = out->_decomposed_sso_data;
    size_t size = ObjectLayer::sso_size(self);

    out->data = items;
    out->size = size;

    font_variations_decompose_sso(self->_d.layout<BLFontVariationSettings::SSO>(), items);
    return BL_SUCCESS;
  }
  else {
    out->data = ObjectLayer::dyn_data<BLFontVariationItem>(self);
    out->size = ObjectLayer::dyn_size(self);
    return BL_SUCCESS;
  }
}

BL_API_IMPL bool bl_font_variation_settings_has_value(const BLFontVariationSettingsCore* self, BLTag variation_tag) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_sso()) {
    uint32_t variation_id = FontTagData::variation_tag_to_id(variation_tag);
    if (variation_id == FontTagData::kInvalidId) {
      return false;
    }

    size_t dummy_index;
    return font_variations_find_sso_tag(self->_d.layout<BLFontVariationSettings::SSO>(), variation_id, &dummy_index);
  }
  else {
    const BLFontVariationItem* data = ObjectLayer::dyn_data<BLFontVariationItem>(self);
    size_t size = ObjectLayer::dyn_size(self);

    size_t index = bl::lower_bound(data, size, variation_tag, [](const BLFontVariationItem& item, uint32_t tag) noexcept { return item.tag < tag; });
    return index < size && data[index].tag == variation_tag;
  }
}

BL_API_IMPL float bl_font_variation_settings_get_value(const BLFontVariationSettingsCore* self, BLTag variation_tag) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_sso()) {
    uint32_t variation_id = FontTagData::variation_tag_to_id(variation_tag);
    if (variation_id == FontTagData::kInvalidId) {
      return Math::nan<float>();
    }

    size_t index;
    if (font_variations_find_sso_tag(self->_d.layout<BLFontVariationSettings::SSO>(), variation_id, &index)) {
      return self->_d.layout<BLFontVariationSettings::SSO>().values[index];
    }
    else {
      return Math::nan<float>();
    }
  }
  else {
    const BLFontVariationItem* data = ObjectLayer::dyn_data<BLFontVariationItem>(self);
    size_t size = ObjectLayer::dyn_size(self);

    size_t index = bl::lower_bound(data, size, variation_tag, [](const BLFontVariationItem& item, uint32_t tag) noexcept { return item.tag < tag; });
    if (index < size && data[index].tag == variation_tag) {
      return data[index].value;
    }
    else {
      return bl::Math::nan<float>();
    }
  }
}

BL_API_IMPL BLResult bl_font_variation_settings_set_value(BLFontVariationSettingsCore* self, BLTag variation_tag, float variation_value) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  size_t size;
  if (self->_d.is_sso()) {
    BLFontVariationSettings::SSO& sso = self->_d.layout<BLFontVariationSettings::SSO>();
    uint32_t variation_id = FontTagData::variation_tag_to_id(variation_tag);

    size = sso.size;

    if (variation_id != FontTagData::kInvalidId) {
      if (font_variations_set_sso_variation(sso, variation_id, variation_value)) {
        return BL_SUCCESS;
      }
    }
    else {
      if (BL_UNLIKELY(!FontTagData::is_valid_tag(variation_tag))) {
        return bl_make_error(BL_ERROR_INVALID_VALUE);
      }
    }

    // Turn the SSO settings to dynamic settings, because some (or multiple) cases below are true:
    //   a) The `variation_tag` doesn't have a corresponding variation id, thus it cannot be used in SSO mode.
    //   b) There is no room in SSO storage to insert another tag/value pair.
    ObjectImplSize impl_size = impl_size_from_capacity(bl_max<size_t>(size + 1u, 16u));
    BLFontVariationSettingsCore tmp;

    // NOTE: This will turn the SSO settings into dynamic settings - it's guaranteed that all further operations will succeed.
    BL_PROPAGATE(font_variations_init_dynamic_from_sso(&tmp, impl_size, sso));
    self->_d = tmp._d;
  }
  else {
    if (BL_UNLIKELY(!FontTagData::is_valid_tag(variation_tag))) {
      return bl_make_error(BL_ERROR_INVALID_VALUE);
    }
    size = ObjectLayer::dyn_size(self);
  }

  BLFontVariationItem* items = ObjectLayer::dyn_data<BLFontVariationItem>(self);
  size_t index = bl::lower_bound(items, size, variation_tag, [](const BLFontVariationItem& item, uint32_t tag) noexcept { return item.tag < tag; });
  size_t non_existing = index >= size || items[index].tag != variation_tag;

  if (ObjectLayer::check_dynamic_capacity_and_mutability(self, non_existing, size)) {
    if (!non_existing) {
      items[index].value = variation_value;
      return BL_SUCCESS;
    }
    else {
      ObjectLayer::set_dyn_size(self, size + 1u);
      bl::axl::copy_reverse(items + index + 1u, items + index, size - index);
      items[index] = {variation_tag, variation_value};
      return BL_SUCCESS;
    }
  }

  size_t result_size = size + non_existing;
  ObjectImplSize impl_size = ObjectLayer::expand_impl_size(impl_size_from_capacity(result_size));

  BLFontVariationSettingsCore tmp;
  BL_PROPAGATE(font_variations_init_dynamic_with_data(&tmp, impl_size, result_size, items, index));

  BLFontVariationItem* dst = ObjectLayer::dyn_data<BLFontVariationItem>(&tmp);
  bl::axl::copy_forward(dst + index + non_existing, items + index, size - index);
  dst[index] = {variation_tag, variation_value};

  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_font_variation_settings_remove_value(BLFontVariationSettingsCore* self, BLTag variation_tag) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_sso()) {
    BLFontVariationSettings::SSO& sso = self->_d.layout<BLFontVariationSettings::SSO>();
    uint32_t variation_id = FontTagData::variation_tag_to_id(variation_tag);

    if (variation_id == FontTagData::kInvalidId) {
      return bl_make_error(BL_ERROR_NO_ENTRY);
    }

    size_t index;
    if (!font_variations_find_sso_tag(sso, variation_id, &index)) {
      return bl_make_error(BL_ERROR_NO_ENTRY);
    }

    font_variations_remove_sso_variation(sso, index);
    return BL_SUCCESS;
  }
  else {
    BLFontVariationItem* items = ObjectLayer::dyn_data<BLFontVariationItem>(self);
    size_t size = ObjectLayer::dyn_size(self);
    size_t index = bl::lower_bound(items, size, variation_tag, [](const BLFontVariationItem& item, uint32_t tag) noexcept { return item.tag < tag; });

    if (index >= size || items[index].tag != variation_tag) {
      return bl_make_error(BL_ERROR_NO_ENTRY);
    }

    if (!ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self)) {
      BLFontVariationSettingsCore tmp;
      BL_PROPAGATE(font_variations_init_dynamic_with_data(&tmp, impl_size_from_capacity(size), size, items, size));

      items = ObjectLayer::dyn_data<BLFontVariationItem>(&tmp);
      ObjectLayer::replace_object(self, &tmp);
    }

    ObjectLayer::set_dyn_size(self, size - 1u);
    bl::axl::copy_forward(items + index, items + index + 1u, size - index - 1u);
    return BL_SUCCESS;
  }
}

// FontVariationSettings - API - Equals
// ====================================

BL_API_IMPL bool bl_font_variation_settings_equals(const BLFontVariationSettingsCore* a, const BLFontVariationSettingsCore* b) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);

  BL_ASSERT(a->_d.is_font_variation_settings());
  BL_ASSERT(b->_d.is_font_variation_settings());

  if (a->_d == b->_d) {
    return true;
  }

  if (a->_d.is_sso() == b->_d.is_sso()) {
    // Both are SSO: They must be binary equal, if not, they are not equal.
    if (a->_d.is_sso()) {
      return false;
    }

    // Both are dynamic.
    ObjectLayer::DataSpan<const BLFontVariationItem> a_variations = ObjectLayer::object_dyn_span<const BLFontVariationItem>(a);
    ObjectLayer::DataSpan<const BLFontVariationItem> b_variations = ObjectLayer::object_dyn_span<const BLFontVariationItem>(b);

    return a_variations.size == b_variations.size && memcmp(a_variations.data, b_variations.data, a_variations.size * sizeof(BLFontVariationItem)) == 0;
  }
  else {
    // One is SSO and one is dynamic, make `a` the SSO one.
    if (b->_d.is_sso()) {
      axl::swap(a, b);
    }

    ObjectLayer::DataSpan<const BLFontVariationItem> b_variations = ObjectLayer::object_dyn_span<const BLFontVariationItem>(b);
    size_t size = ObjectLayer::sso_size(a);

    if (size != b_variations.size) {
      return false;
    }

    // NOTE: Since SSO representation is not that trivial, just try to convert B impl to SSO representation
    // and then try binary equality of two SSO instances. If B is not convertible, then A and B are not equal.
    BLFontVariationSettingsCore b_object;

    BL_ASSERT(size <= BLFontVariationSettings::Info::kSSOCapacity);
    if (!font_variations_to_sso(b_object._d.layout<BLFontVariationSettings::SSO>(), b_variations.data, size)) {
      return false;
    }

    return a->_d == b_object._d;
  }
}

// FontVariationSettings - Runtime Registration
// ============================================

void bl_font_variation_settings_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  BL_USING_DETAIL(bl::FontVariationSettingsDetail);
  static constexpr BLObjectType object_type = BLFontVariationSettings::Info::kType;

  ObjectLayer::init_sso(static_cast<BLFontVariationSettingsCore*>(&bl_object_defaults[object_type]));
}
