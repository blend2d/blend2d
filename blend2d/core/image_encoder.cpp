// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#if !defined(BL_BUILD_NO_CODECS)

#include <blend2d/core/image_encoder_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/runtime_p.h>

// ImageEncoder - Null
// ===================

namespace bl {

static constinit ObjectLayer::EternalVirtImpl<BLImageEncoderImpl, BLImageEncoderVirt> null_encoder{};

static BLResult BL_CDECL null_encoder_destroy(
  [[maybe_unused]] BLObjectCore* self
) noexcept {
  return BL_SUCCESS;
}

static BLResult BL_CDECL null_encoder_restart(
  [[maybe_unused]] BLImageEncoderCore* self
) noexcept {
  return bl_make_error(BL_ERROR_INVALID_STATE);
}

static BLResult BL_CDECL null_encoder_write_frame(
  [[maybe_unused]] BLImageEncoderCore* self,
  [[maybe_unused]] BLArrayCore* dst,
  [[maybe_unused]] const BLImageCore* image
) noexcept {
  return bl_make_error(BL_ERROR_INVALID_STATE);
}

} // {bl}

// ImageEncoder - API - Init & Destroy
// ===================================

BL_API_IMPL BLResult bl_image_encoder_init(BLImageEncoderCore* self) noexcept {
  self->_d = bl_object_defaults[BL_OBJECT_TYPE_IMAGE_ENCODER]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_image_encoder_init_move(BLImageEncoderCore* self, BLImageEncoderCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_image_encoder_init_weak(BLImageEncoderCore* self, const BLImageEncoderCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object<ObjectLayer::ObjectMode::kDyn>(self, other);
}

BL_API_IMPL BLResult bl_image_encoder_destroy(BLImageEncoderCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return bl::ObjectLayer::release_object(self);
}

// ImageEncoder - API - Reset
// ==========================

BL_API_IMPL BLResult bl_image_encoder_reset(BLImageEncoderCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

// ImageEncoder - API - Assign
// ===========================

BL_API_IMPL BLResult bl_image_encoder_assign_move(BLImageEncoderCore* self, BLImageEncoderCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_image_encoder_assign_weak(BLImageEncoderCore* self, const BLImageEncoderCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

// ImageEncoder - API - Interface
// ==============================

BL_API_IMPL BLResult bl_image_encoder_restart(BLImageEncoderCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->restart(self);
}

BL_API_IMPL BLResult bl_image_encoder_write_frame(BLImageEncoderCore* self, BLArrayCore* dst, const BLImageCore* src) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->write_frame(self, dst, src);
}

// ImageEncoder - Runtime Registration
// ===================================

void bl_image_encoder_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  using namespace bl;
  static constexpr BLObjectType object_type = BLImageEncoder::Info::kType;

  // Initialize default BLImageEncoder.
  null_encoder.impl->ctor(static_cast<BLImageCodecCore*>(&bl_object_defaults[BL_OBJECT_TYPE_IMAGE_CODEC]));
  null_encoder.impl->last_result = BL_ERROR_NOT_INITIALIZED;
  null_encoder.virt.base.destroy = null_encoder_destroy;
  null_encoder.virt.base.get_property = bl_object_impl_get_property;
  null_encoder.virt.base.set_property = bl_object_impl_set_property;
  null_encoder.virt.restart = null_encoder_restart;
  null_encoder.virt.write_frame = null_encoder_write_frame;

  ObjectLayer::init_virt(&bl_object_defaults[object_type], object_type, 0u, &null_encoder.impl, &null_encoder.virt);
}

#endif // !defined(BL_BUILD_NO_CODECS)
