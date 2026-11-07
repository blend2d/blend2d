// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#if !defined(BL_BUILD_NO_CODECS)

#include <blend2d/core/image_decoder_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/runtime_p.h>

// ImageDecoder - Null
// ===================

namespace bl {

static constinit ObjectLayer::EternalVirtImpl<BLImageDecoderImpl, BLImageDecoderVirt> null_decoder{};

static BLResult BL_CDECL null_decoder_destroy(
  [[maybe_unused]] BLObjectCore* self
) noexcept {
  return BL_SUCCESS;
}

static BLResult BL_CDECL null_decoder_restart(
  [[maybe_unused]] BLImageDecoderCore* self
) noexcept {
  return bl_make_error(BL_ERROR_INVALID_STATE);
}

static BLResult BL_CDECL null_decoder_read_info(
  [[maybe_unused]] BLImageDecoderCore* self,
  [[maybe_unused]] BLImageInfo* info_out,
  [[maybe_unused]] const uint8_t* data,
  [[maybe_unused]] size_t size
) noexcept {
  return bl_make_error(BL_ERROR_INVALID_STATE);
}

static BLResult BL_CDECL null_decoder_read_frame(
  [[maybe_unused]] BLImageDecoderCore* self,
  [[maybe_unused]] BLImageCore* image_out,
  [[maybe_unused]] const uint8_t* data,
  [[maybe_unused]] size_t size
) noexcept {
  return bl_make_error(BL_ERROR_INVALID_STATE);
}

} // {bl}

// ImageDecoder - API - Init & Destroy
// ===================================

BL_API_IMPL BLResult bl_image_decoder_init(BLImageDecoderCore* self) noexcept {
  self->_d = bl_object_defaults[BL_OBJECT_TYPE_IMAGE_DECODER]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_image_decoder_init_move(BLImageDecoderCore* self, BLImageDecoderCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_image_decoder_init_weak(BLImageDecoderCore* self, const BLImageDecoderCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object<ObjectLayer::ObjectMode::kDyn>(self, other);
}

BL_API_IMPL BLResult bl_image_decoder_destroy(BLImageDecoderCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object(self);
}

// ImageDecoder - API - Reset
// ==========================

BL_API_IMPL BLResult bl_image_decoder_reset(BLImageDecoderCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

// ImageDecoder - API - Assign
// ===========================

BL_API_IMPL BLResult bl_image_decoder_assign_move(BLImageDecoderCore* self, BLImageDecoderCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_image_decoder_assign_weak(BLImageDecoderCore* self, const BLImageDecoderCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

// ImageDecoder - API - Interface
// ==============================

BL_API_IMPL BLResult bl_image_decoder_restart(BLImageDecoderCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_d.virt<BLImageDecoderVirt>()->restart(self);
}

BL_API_IMPL BLResult bl_image_decoder_read_info(BLImageDecoderCore* self, BLImageInfo* info_out, const uint8_t* data, size_t size) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_d.virt<BLImageDecoderVirt>()->read_info(self, info_out, data, size);
}

BL_API_IMPL BLResult bl_image_decoder_read_frame(BLImageDecoderCore* self, BLImageCore* image_out, const uint8_t* data, size_t size) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_d.virt<BLImageDecoderVirt>()->read_frame(self, image_out, data, size);
}

// ImageDecoder - Runtime Registration
// ===================================

void bl_image_decoder_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  using namespace bl;
  static constexpr BLObjectType object_type = BLImageDecoder::Info::kType;

  // Initialize a null implementation of BLImageDecoder.
  null_decoder.virt.base.destroy = null_decoder_destroy;
  null_decoder.virt.base.get_property = bl_object_impl_get_property;
  null_decoder.virt.base.set_property = bl_object_impl_set_property;
  null_decoder.virt.restart = null_decoder_restart;
  null_decoder.virt.read_info = null_decoder_read_info;
  null_decoder.virt.read_frame = null_decoder_read_frame;
  null_decoder.impl->ctor(static_cast<BLImageCodecCore*>(&bl_object_defaults[BL_OBJECT_TYPE_IMAGE_CODEC]));
  null_decoder.impl->last_result = BL_ERROR_NOT_INITIALIZED;

  ObjectLayer::init_virt(&bl_object_defaults[object_type], object_type, 0u, &null_decoder.impl, &null_decoder.virt);
}

#endif // !defined(BL_BUILD_NO_CODECS)
