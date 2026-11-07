// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/core/image_p.h>
#include <blend2d/core/matrix_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/pattern_p.h>
#include <blend2d/core/runtime_p.h>

namespace bl::PatternDetail {

// Pattern - Null
// ==============

static ObjectLayer::EternalImpl<PatternPrivateImpl> null_pattern;

// Pattern - Internals
// ===================

static BL_INLINE BLResult alloc_impl(BLPatternCore* self, const BLImageCore* image, const BLRectI& area, const BLMatrix2D& transform, uint32_t payload) noexcept {
  BL_PROPAGATE(ObjectLayer::alloc_impl_t<PatternPrivateImpl>(self, BL_OBJECT_TYPE_PATTERN));
  ObjectLayer::set_payload_u16(self, payload);

  PatternPrivateImpl* impl = ObjectLayer::get_impl(self);
  bl_call_ctor(impl->image.dcast(), image->dcast());
  impl->transform = transform;
  impl->area = area;

  return BL_SUCCESS;
}

BLResult free_object(BLPatternCore* self, PatternPrivateImpl* impl, ObjectLayer::MetaByte meta_byte) noexcept {
  axl::maybe_unused(self);
  bl_image_destroy(&impl->image);
  return ObjectLayer::dealloc_impl(impl, meta_byte);
}

static BL_NOINLINE BLResult make_mutable_copy_of(BLPatternCore* self, const BLPatternCore* other) noexcept {
  BLPatternCore tmp;
  PatternPrivateImpl* other_impl = ObjectLayer::get_impl(other);

  BL_PROPAGATE(alloc_impl(&tmp, &other_impl->image, other_impl->area, other_impl->transform, ObjectLayer::get_payload_u16(other)));
  return ObjectLayer::replace_object(self, &tmp);
}

static BL_INLINE BLResult make_mutable(BLPatternCore* self) noexcept {
  if (!ObjectLayer::is_object_mutable(self)) {
    return make_mutable_copy_of(self, self);
  }
  else {
    return BL_SUCCESS;
  }
}

} // {bl::PatternDetail}

// Pattern - API - Init & Destroy
// ==============================

BL_API_IMPL BLResult bl_pattern_init(BLPatternCore* self) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);

  return ObjectLayer::init_object(self);
}

BL_API_IMPL BLResult bl_pattern_init_move(BLPatternCore* self, BLPatternCore* other) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_pattern_init_weak(BLPatternCore* self, const BLPatternCore* other) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object(self, other);
}

BL_API_IMPL BLResult bl_pattern_init_as(BLPatternCore* self, const BLImageCore* image, const BLRectI* area, BLExtendMode extend_mode, const BLMatrix2D* transform) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);

  ObjectLayer::init_object(self);
  if (!image) {
    image = static_cast<const BLImageCore*>(&bl_object_defaults[BL_OBJECT_TYPE_IMAGE]);
  }

  BLImageImpl* image_impl = ObjectLayer::get_impl(image);
  BLRectI image_area(0, 0, image_impl->size.w, image_impl->size.h);

  if (BL_UNLIKELY(extend_mode > BL_EXTEND_MODE_COMPLEX_MAX_VALUE))
    return bl_make_error(BL_ERROR_INVALID_VALUE);

  if (!area) {
    area = &image_area;
  }
  else if (*area != image_area && !is_area_valid(*area, image_impl->size)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  uint32_t payload = uint32_t(extend_mode) << BLPattern::ExtendModeField::kShift;
  if (!transform) {
    transform = &bl::TransformDetail::identity_transform;
  }
  else {
    payload |= uint32_t(transform->type()) << BLPattern::TransformTypeField::kShift;
  }

  return alloc_impl(self, image, *area, *transform, payload);
}

BL_API_IMPL BLResult bl_pattern_destroy(BLPatternCore* self) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object(self);
}

// Pattern - API - Reset
// =====================

BL_API_IMPL BLResult bl_pattern_reset(BLPatternCore* self) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

// Pattern - API - Assign
// ======================

BL_API_IMPL BLResult bl_pattern_assign_move(BLPatternCore* self, BLPatternCore* other) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_pattern_assign_weak(BLPatternCore* self, const BLPatternCore* other) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

BL_API_IMPL BLResult bl_pattern_assign_deep(BLPatternCore* self, const BLPatternCore* other) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  if (!ObjectLayer::is_object_mutable(self)) {
    return make_mutable_copy_of(self, other);
  }

  PatternPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  PatternPrivateImpl* other_impl = ObjectLayer::get_impl(other);

  self->_d.set_pl0_byte(other->_d.pl0_byte());
  self->_d.set_pl1_byte(other->_d.pl1_byte());

  self_impl->transform = other_impl->transform;
  self_impl->area = other_impl->area;
  return bl_image_assign_weak(&self_impl->image, &other_impl->image);
}

// Pattern - API - Create
// ======================

BL_API_IMPL BLResult bl_pattern_create(BLPatternCore* self, const BLImageCore* image, const BLRectI* area, BLExtendMode extend_mode, const BLMatrix2D* transform) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (!image) {
    image = static_cast<BLImageCore*>(&bl_object_defaults[BL_OBJECT_TYPE_IMAGE]);
  }

  BLImageImpl* image_impl = ObjectLayer::get_impl(image);
  BLRectI image_area(0, 0, image_impl->size.w, image_impl->size.h);

  if (BL_UNLIKELY(extend_mode > BL_EXTEND_MODE_COMPLEX_MAX_VALUE)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  uint32_t payload = uint32_t(extend_mode) << BLPattern::ExtendModeField::kShift;
  if (!area) {
    area = &image_area;
  }
  else if (*area != image_area && !is_area_valid(*area, image_impl->size)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  if (!transform) {
    transform = &bl::TransformDetail::identity_transform;
  }
  else {
    payload |= uint32_t(transform->type()) << BLPattern::TransformTypeField::kShift;
  }

  if (!ObjectLayer::is_object_mutable(self)) {
    BLPatternCore new_o;
    BL_PROPAGATE(alloc_impl(&new_o, image, *area, *transform, payload));
    return ObjectLayer::replace_object(self, &new_o);
  }
  else {
    ObjectLayer::set_payload_u16(self, payload);
    PatternPrivateImpl* self_impl = ObjectLayer::get_impl(self);
    self_impl->area = *area;
    self_impl->transform = *transform;
    return bl_image_assign_weak(&self_impl->image, image);
  }
}

// Pattern - API - Image & Area
// ============================

BL_API_IMPL BLResult bl_pattern_get_image(const BLPatternCore* self, BLImageCore* image) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  PatternPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return bl_image_assign_weak(image, &self_impl->image);
}

BL_API_IMPL BLResult bl_pattern_set_image(BLPatternCore* self, const BLImageCore* image, const BLRectI* area) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (!image) {
    image = static_cast<BLImageCore*>(&bl_object_defaults[BL_OBJECT_TYPE_IMAGE]);
  }

  BLImageImpl* image_impl = ObjectLayer::get_impl(image);
  BLRectI image_area(0, 0, image_impl->size.w, image_impl->size.h);

  if (!area) {
    area = &image_area;
  }
  else if (*area != image_area && !is_area_valid(*area, image->dcast().size())) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  BL_PROPAGATE(make_mutable(self));
  PatternPrivateImpl* self_impl = ObjectLayer::get_impl(self);

  self_impl->area = *area;
  return bl_image_assign_weak(&self_impl->image, image);
}

BL_API_IMPL BLResult bl_pattern_reset_image(BLPatternCore* self) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return bl_pattern_set_image(self, nullptr, nullptr);
}

BL_API_IMPL BLResult bl_pattern_get_area(const BLPatternCore* self, BLRectI* area_out) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  PatternPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  *area_out = self_impl->area;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_pattern_set_area(BLPatternCore* self, const BLRectI* area) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  PatternPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  BLImageImpl* image_impl = ObjectLayer::get_impl(&self_impl->image);

  if (BL_UNLIKELY(!is_area_valid(*area, image_impl->size))) {
    // Support empty `area` - this is necessary so `pattern.set_area(pattern.area())` works as expected.
    if ((area->x | area->y | area->w | area->h) == 0) {
      return bl_pattern_reset_area(self);
    }

    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  BL_PROPAGATE(make_mutable(self));
  self_impl = ObjectLayer::get_impl(self);
  self_impl->area = *area;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_pattern_reset_area(BLPatternCore* self) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  PatternPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  BLSizeI size = ObjectLayer::get_impl(&self_impl->image)->size;

  if (self_impl->area == BLRectI(0, 0, size.w, size.h)) {
    return BL_SUCCESS;
  }

  BL_PROPAGATE(make_mutable(self));
  self_impl = ObjectLayer::get_impl(self);
  self_impl->area.reset(0, 0, size.w, size.h);
  return BL_SUCCESS;
}

// Pattern - API - Extend Mode
// ===========================

BL_API_IMPL BLExtendMode bl_pattern_get_extend_mode(const BLPatternCore* self) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_payload_field<BLPattern::ExtendModeField>(self);
}

BL_API_IMPL BLResult bl_pattern_set_extend_mode(BLPatternCore* self, BLExtendMode extend_mode) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(extend_mode > BL_EXTEND_MODE_COMPLEX_MAX_VALUE)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  return ObjectLayer::set_payload_field<BLPattern::ExtendModeField>(self, extend_mode);
}

// Pattern - API - Transform
// =========================

BL_API_IMPL BLResult bl_pattern_get_transform(const BLPatternCore* self, BLMatrix2D* transform_out) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (ObjectLayer::get_payload_field<BLPattern::TransformTypeField>(self) == BL_TRANSFORM_TYPE_IDENTITY) {
    transform_out->reset();
  }
  else {
    PatternPrivateImpl* self_impl = ObjectLayer::get_impl(self);
    *transform_out = self_impl->transform;
  }

  return BL_SUCCESS;
}

BL_API_IMPL BLTransformType bl_pattern_get_transform_type(const BLPatternCore* self) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_payload_field<BLPattern::TransformTypeField>(self);
}

BL_API_IMPL BLResult bl_pattern_set_transform(BLPatternCore* self, const BLMatrix2D* transform) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  PatternPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  if (!ObjectLayer::is_object_mutable(self)) {
    if (ObjectLayer::get_impl(self)->transform == *transform) {
      return BL_SUCCESS;
    }

    BL_PROPAGATE(make_mutable_copy_of(self, self));
    self_impl = ObjectLayer::get_impl(self);
  }

  self_impl->transform = *transform;
  return ObjectLayer::set_payload_field<BLPattern::TransformTypeField>(self, self_impl->transform.type());
}

BL_API_IMPL BLResult bl_pattern_reset_transform(BLPatternCore* self) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  PatternPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  if (!ObjectLayer::is_object_mutable(self)) {
    if (ObjectLayer::get_payload_field<BLPattern::TransformTypeField>(self) == BL_TRANSFORM_TYPE_IDENTITY) {
      return BL_SUCCESS;
    }

    BL_PROPAGATE(make_mutable_copy_of(self, self));
    self_impl = ObjectLayer::get_impl(self);
  }

  self_impl->transform.reset();
  return ObjectLayer::set_payload_field<BLPattern::TransformTypeField>(self, BL_TRANSFORM_TYPE_IDENTITY);
}

BL_API_IMPL BLResult bl_pattern_apply_transform_op(BLPatternCore* self, BLTransformOp op_type, const void* op_data) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (op_type <= BL_TRANSFORM_OP_ASSIGN) {
    if (op_type == BL_TRANSFORM_OP_RESET) {
      return bl_pattern_reset_transform(self);
    }
    else {
      return bl_pattern_set_transform(self, static_cast<const BLMatrix2D*>(op_data));
    }
  }

  PatternPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  if (!ObjectLayer::is_object_mutable(self)) {
    BLMatrix2D transform(self_impl->transform);

    BL_PROPAGATE(bl_matrix2d_apply_op(&transform, op_type, op_data));
    BL_PROPAGATE(make_mutable_copy_of(self, self));

    self_impl = ObjectLayer::get_impl(self);
    self_impl->transform = transform;
  }
  else {
    BL_PROPAGATE(bl_matrix2d_apply_op(&self_impl->transform, op_type, op_data));
  }

  return ObjectLayer::set_payload_field<BLPattern::TransformTypeField>(self, self_impl->transform.type());
}

// Pattern - API - Equality & Comparison
// =====================================

BL_API_IMPL bool bl_pattern_equals(const BLPatternCore* a, const BLPatternCore* b) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  if (!axl::bool_and(a->_d.pl0_byte() == b->_d.pl0_byte(), a->_d.pl1_byte() == b->_d.pl1_byte())) {
    return false;
  }

  const PatternPrivateImpl* a_impl = ObjectLayer::get_impl(a);
  const PatternPrivateImpl* b_impl = ObjectLayer::get_impl(b);

  if (a_impl == b_impl) {
    return true;
  }

  if (!axl::bool_and(a_impl->transform == b_impl->transform, a_impl->area == b_impl->area)) {
    return false;
  }

  return a_impl->image.dcast() == b_impl->image.dcast();
}

// Pattern - Runtime Registration
// ==============================

void bl_pattern_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  BL_USING_DETAIL(bl::PatternDetail);
  static constexpr BLObjectType object_type = BLPattern::Info::kType;

  null_pattern.impl.image._d = bl_object_defaults[BL_OBJECT_TYPE_IMAGE]._d;
  null_pattern.impl.transform.reset();

  ObjectLayer::init_dyn(&bl_object_defaults[object_type], object_type, BLPattern::kDefaultMetaFlags, &null_pattern.impl, nullptr);
  ObjectLayer::set_payload_field<BLPattern::ExtendModeField>(&bl_object_defaults[object_type], BL_EXTEND_MODE_REPEAT);
}
