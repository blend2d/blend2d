// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_ops.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/core/stroke_options_p.h>

namespace bl::StrokeOptionsDetail {

// StrokeOptions - Internals - Alloc & Free Impl
// =============================================

static BL_INLINE_CONSTEXPR ObjectImplSize impl_size_from_capacity(size_t n) noexcept {
  return ObjectImplSize(ObjectLayer::data_offset_of<BLStrokeOptionsCore> + n * sizeof(float));
}

static BL_INLINE_CONSTEXPR size_t capacity_from_impl_size(ObjectImplSize impl_size) noexcept {
  return (size_t(impl_size) - ObjectLayer::data_offset_of<BLStrokeOptionsCore>) / sizeof(float);
}

static BL_NOINLINE BLResult init_dynamic_with_data(
  BLStrokeOptionsCore* self,
  ObjectImplSize impl_size,
  const BLStrokeOptions::Layout& src,
  BLSpan<const float> data
) noexcept {
  BL_PROPAGATE(ObjectLayer::alloc_impl(self, BL_OBJECT_TYPE_STROKE_OPTIONS, impl_size));

  BLStrokeOptions::Layout& layout = self->_d.layout<BLStrokeOptions::Layout>();
  layout.dash_array.dyn.size = uint32_t(data.size());
  layout.dash_array.dyn.capacity = uint32_t(capacity_from_impl_size(impl_size));
  layout.width = src.width;
  layout.miter_limit = src.miter_limit;
  layout.dash_offset = src.dash_offset;
  layout.payload = src.payload;
  axl::copy_forward(layout.dash_array.dyn.data.ptr, data.data(), data.size());

  return BL_SUCCESS;
}

} // {bl}

// StrokeOptions - API - Init & Destroy
// ====================================

BL_API_IMPL BLResult bl_stroke_options_init(BLStrokeOptionsCore* self) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);

  BLStrokeOptions::_init_default(self->_d);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_stroke_options_init_move(BLStrokeOptionsCore* self, BLStrokeOptionsCore* other) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  self->_d = other->_d;
  BLStrokeOptions::_init_default(other->_d);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_stroke_options_init_weak(BLStrokeOptionsCore* self, const BLStrokeOptionsCore* other) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object(self, other);
}

BL_API_IMPL BLResult bl_stroke_options_destroy(BLStrokeOptionsCore* self) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object(self);
}

// StrokeOptions - API - Reset
// ===========================

BL_API_IMPL BLResult bl_stroke_options_reset(BLStrokeOptionsCore* self) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  ObjectLayer::release_object(self);
  BLStrokeOptions::_init_default(self->_d);

  return BL_SUCCESS;
}

// StrokeOptions - API - Assign
// ============================

BL_API_IMPL BLResult bl_stroke_options_assign_move(BLStrokeOptionsCore* self, BLStrokeOptionsCore* other) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_stroke_options_assign_weak(BLStrokeOptionsCore* self, const BLStrokeOptionsCore* other) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

// StrokeOptions - API - Accessors
// ===============================

BL_API_IMPL BLStrokeCap bl_stroke_options_get_start_cap(const BLStrokeOptionsCore* self) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_payload_field<BLStrokeOptions::StartCapField>(self);
}

BL_API_IMPL BLStrokeCap bl_stroke_options_get_end_cap(const BLStrokeOptionsCore* self) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_payload_field<BLStrokeOptions::EndCapField>(self);
}

BL_API_IMPL BLStrokeJoin bl_stroke_options_get_join(const BLStrokeOptionsCore* self) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_payload_field<BLStrokeOptions::JoinField>(self);
}

BL_API_IMPL BLStrokeTransformOrder bl_stroke_options_get_transform_order(const BLStrokeOptionsCore* self) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_payload_field<BLStrokeOptions::TransformField>(self);
}

BL_API_IMPL float bl_stroke_options_get_width(const BLStrokeOptionsCore* self) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_d.layout<BLStrokeOptions::Layout>().width;
}

BL_API_IMPL float bl_stroke_options_get_miter_limit(const BLStrokeOptionsCore* self) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_d.layout<BLStrokeOptions::Layout>().miter_limit;
}

BL_API_IMPL float bl_stroke_options_get_dash_offset(const BLStrokeOptionsCore* self) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_d.layout<BLStrokeOptions::Layout>().dash_offset;
}

BL_API_IMPL const float* bl_stroke_options_get_dash_array(const BLStrokeOptionsCore* self) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_data<float>(self);
}

BL_API_IMPL size_t bl_stroke_options_get_dash_count(const BLStrokeOptionsCore* self) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_size(self);
}

BL_API_IMPL BLResult bl_stroke_options_set_start_cap(BLStrokeOptionsCore* self, BLStrokeCap start_cap) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::set_payload_field<BLStrokeOptions::StartCapField>(self, start_cap);
}

BL_API_IMPL BLResult bl_stroke_options_set_end_cap(BLStrokeOptionsCore* self, BLStrokeCap end_cap) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::set_payload_field<BLStrokeOptions::EndCapField>(self, end_cap);
}

BL_API_IMPL BLResult bl_stroke_options_set_join(BLStrokeOptionsCore* self, BLStrokeJoin join) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::set_payload_field<BLStrokeOptions::JoinField>(self, join);
}

BL_API_IMPL BLResult bl_stroke_options_set_transform_order(BLStrokeOptionsCore* self, BLStrokeTransformOrder order) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::set_payload_field<BLStrokeOptions::TransformField>(self, order);
}

BL_API_IMPL BLResult bl_stroke_options_set_width(BLStrokeOptionsCore* self, float width) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  self->_d.layout<BLStrokeOptions::Layout>().width = width;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_stroke_options_set_miter_limit(BLStrokeOptionsCore* self, float miter_limit) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  self->_d.layout<BLStrokeOptions::Layout>().miter_limit = miter_limit;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_stroke_options_set_dash_offset(BLStrokeOptionsCore* self, float dash_offset) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  self->_d.layout<BLStrokeOptions::Layout>().dash_offset = dash_offset;
  return BL_SUCCESS;
}

// StrokeOptions - API - Dash Array
// ================================

BL_API_IMPL BLResult bl_stroke_options_set_dash_array_object(BLStrokeOptionsCore* self, const BLArrayCore* array) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(array->_d.type() != BL_OBJECT_TYPE_ARRAY_FLOAT32)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  const BLArray<float>& dashes = array->dcast<BLArray<float>>();
  BLSpan<const float> data = dashes.span();

  if (self->_d.is_ref_counted()) {
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, data.size())) {
      float* dst = ObjectLayer::dyn_data<float>(self);
      memcpy(dst, data.data(), data.size() * sizeof(float));
      ObjectLayer::set_dyn_size(self, data.size());
      return BL_SUCCESS;
    }
  }
  else if (data.size() <= BLStrokeOptions::Info::kSSOCapacity) {
    ObjectLayer::set_sso_size(self, data.size());
    return ObjectLayer::init_sso_buffer<BLStrokeOptions::Info::kSSOCapacity>(ObjectLayer::sso_data<float>(self), data.data(), data.size());
  }

  if (array->_d.is_ref_counted() && !ObjectLayer::get_impl_header(array->_d.impl())->is_external()) {
    size_t array_capacity = ObjectLayer::dyn_capacity(array);
    if (ObjectLayer::size_fits_32_bits(data.size() | array_capacity)) {
      // TRANSMUTE: BLArray data into BLStrokeOptions data.
      ObjectLayer::retain_impl(array->_d.impl<BLArrayImpl>());
      ObjectLayer::release_object(self);
      self->_d.set_meta_byte(array->_d.meta_byte());
      self->_d.dyn.impl = array->_d.dyn.impl;
      self->_d.dyn.size_u32 = uint32_t(data.size());
      self->_d.dyn.capacity_u32 = uint32_t(array_capacity);
      return BL_SUCCESS;
    }
  }

  return bl_stroke_options_set_dash_array_data(self, data.data(), data.size());
}

BL_API_IMPL BLResult bl_stroke_options_set_dash_array_data(BLStrokeOptionsCore* self, const float* data, size_t size) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStrokeOptions::Layout& layout = self->_d.layout<BLStrokeOptions::Layout>();

  if (self->_d.is_sso()) {
    if (size <= BLStrokeOptions::Info::kSSOCapacity) {
      layout.meta = uint8_t(size);
      return ObjectLayer::init_sso_buffer<BLStrokeOptions::Info::kSSOCapacity>(layout.dash_array.sso.data, data, size);
    }
  }
  else {
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, size)) {
      layout.dash_array.dyn.size = uint32_t(size);
      axl::copy_forward(layout.dash_array.dyn.data.ptr, data, size);
      return BL_SUCCESS;
    }
  }

  if (!ObjectLayer::size_fits_32_bits(size)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  BLStrokeOptionsCore tmp;
  ObjectImplSize impl_size = impl_size_from_capacity(size);

  BL_PROPAGATE(init_dynamic_with_data(&tmp, impl_size, layout, {data, size}));
  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_stroke_options_reset_dash_array(BLStrokeOptionsCore* self) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_ref_counted()) {
    BLStrokeOptionsImpl* impl = self->_d.impl<BLStrokeOptionsImpl>();
    if (ObjectLayer::deref_and_test_impl(impl)) {
      ObjectLayer::free_object(self, impl);
    }
  }

  BLStrokeOptions::Layout& layout = self->_d.layout<BLStrokeOptions::Layout>();
  memset(layout.dash_array.sso.data, 0, sizeof(layout.dash_array.sso.data));

  // This makes the object become SSO - resetting size clears DR bits.
  ObjectLayer::set_sso_size(self, 0u);
  return BL_SUCCESS;
}

// StrokeOptions - API - Equality & Comparison
// ===========================================

BL_API_IMPL bool bl_stroke_options_equals(const BLStrokeOptionsCore* a, const BLStrokeOptionsCore* b) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  const BLStrokeOptions::Layout& a_layout = a->_d.layout<BLStrokeOptions::Layout>();
  const BLStrokeOptions::Layout& b_layout = b->_d.layout<BLStrokeOptions::Layout>();

  if (!axl::bool_and(bl_equals(a_layout.width      , b_layout.width      ),
                     bl_equals(a_layout.miter_limit, b_layout.miter_limit),
                     bl_equals(a_layout.dash_offset, b_layout.dash_offset),
                     bl_equals(a_layout.payload    , b_layout.payload    ))) {
    return false;
  }

  BLSpan<const float> a_dash_array = a->dcast().dash_array();
  BLSpan<const float> b_dash_array = b->dcast().dash_array();

  return a_dash_array.size() == b_dash_array.size() && memcmp(a_dash_array.data(), b_dash_array.data(), a_dash_array.size() * sizeof(float)) == 0;
}

// Path - Runtime Registration
// ===========================

void bl_stroke_options_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  BL_USING_DETAIL(bl::StrokeOptionsDetail);

  BLStrokeOptions::_init_default(bl_object_defaults[BL_OBJECT_TYPE_STROKE_OPTIONS]._d);
}
