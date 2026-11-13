// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#if !defined(BL_BUILD_NO_CODECS)

#include <blend2d/axl/string_utils_p.h>
#include <blend2d/axl/wrap_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/image_p.h>
#include <blend2d/core/image_codec_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/core/string_p.h>
#include <blend2d/codec/bmp_codec_p.h>
#include <blend2d/codec/jpeg_codec_p.h>
#include <blend2d/codec/png_codec_p.h>
#include <blend2d/codec/qoi_codec_p.h>
#include <blend2d/threading/mutex_p.h>

namespace bl::ImageCodecDetail {

// ImageCodec - Null
// =================

static constinit ObjectLayer::EternalVirtImpl<BLImageCodecImpl, BLImageCodecVirt> null_codec{};

static BLResult BL_CDECL null_codec_destroy(
  [[maybe_unused]] BLObjectCore* self
) noexcept {
  return BL_SUCCESS;
}

static uint32_t BL_CDECL null_codec_inspect_data(
  [[maybe_unused]] const BLImageCodecCore* self,
  [[maybe_unused]] const uint8_t* data,
  [[maybe_unused]] size_t size
) noexcept {
  return 0;
}

static BLResult BL_CDECL null_codec_create_decoder(
  [[maybe_unused]] const BLImageCodecCore* self,
  [[maybe_unused]] BLImageDecoderCore* dst
) noexcept {
  return bl_make_error(BL_ERROR_IMAGE_DECODER_NOT_PROVIDED);
}

static BLResult BL_CDECL null_codec_create_encoder(
  [[maybe_unused]] const BLImageCodecCore* self,
  [[maybe_unused]] BLImageEncoderCore* dst
) noexcept {
  return bl_make_error(BL_ERROR_IMAGE_ENCODER_NOT_PROVIDED);
}

// ImageCodec - Detail - Built-In Codecs
// =====================================

static axl::Wrap<BLArray<BLImageCodec>> builtin_codecs_array;
static axl::Wrap<SharedMutex> builtin_codecs_mutex;

// ImageCodec - Detail - Utilities
// ===============================

static bool match_extension(const char* extensions, const char* match, size_t match_size) noexcept {
  while (*extensions) {
    // Match each extension in `extensions` string list delimited by '|'.
    const char* p = extensions;

    while (*p && *p != '|') {
      p++;
    }

    size_t ext_size = (size_t)(p - extensions);
    if (ext_size == match_size && axl::memeq_ci(extensions, match, match_size)) {
      return true;
    }

    if (*p == '|') {
      p++;
    }

    extensions = p;
  }
  return false;
}

// Returns possibly advanced `match` and fixes the `size`.
static const char* keep_only_extension_in_match(const char* match, size_t& size) noexcept {
  const char* end = match + size;
  const char* p = end;

  while (p != match && p[-1] != '.') {
    p--;
  }

  size = (size_t)(end - p);
  return p;
}

static BLResult find_codec_by_name(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) noexcept {
  for (const BLImageCodec& codec : codecs->dcast<BLArray<BLImageCodec>>().span()) {
    if (codec.name().size() == size && axl::memeq_ci(codec.name().data(), name, size)) {
      return bl_image_codec_assign_weak(self, &codec);
    }
  }

  return bl_make_error(BL_ERROR_IMAGE_NO_MATCHING_CODEC);
}

static BLResult find_codec_by_extension(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) noexcept {
  for (const BLImageCodec& codec : codecs->dcast<BLArray<BLImageCodec>>().span()) {
    if (match_extension(codec.extensions().data(), name, size)) {
      return bl_image_codec_assign_weak(self, &codec);
    }
  }

  return bl_make_error(BL_ERROR_IMAGE_NO_MATCHING_CODEC);
}

static BLResult find_codec_by_data(BLImageCodecCore* self, const void* data, size_t size, const BLArrayCore* codecs) noexcept {
  uint32_t best_score = 0;
  const BLImageCodec* candidate = nullptr;

  for (const BLImageCodec& codec : codecs->dcast<BLArray<BLImageCodec>>().span()) {
    uint32_t score = codec.inspect_data(data, size);
    if (best_score < score) {
      best_score = score;
      candidate = &codec;
    }
  }

  if (candidate) {
    return bl_image_codec_assign_weak(self, candidate);
  }

  return bl_make_error(BL_ERROR_IMAGE_NO_MATCHING_CODEC);
}

} // {bl::ImageCodecDetail}

// ImageCodec - API - Init & Destroy
// =================================

BL_API_IMPL BLResult bl_image_codec_init(BLImageCodecCore* self) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);

  self->_d = bl_object_defaults[BL_OBJECT_TYPE_IMAGE_CODEC]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_image_codec_init_move(BLImageCodecCore* self, BLImageCodecCore* other) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_image_codec_init_weak(BLImageCodecCore* self, const BLImageCodecCore* other) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object(self, other);
}

BL_API_IMPL BLResult bl_image_codec_init_by_name_cstr(BLImageCodecCore* self, const char* name, const BLArrayCore* codecs) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);

  self->_d = bl_object_defaults[BL_OBJECT_TYPE_IMAGE_CODEC]._d;
  return bl_image_codec_find_by_name_cstr(self, name, codecs);
}

BL_API_IMPL BLResult bl_image_codec_init_by_name_view(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);

  self->_d = bl_object_defaults[BL_OBJECT_TYPE_IMAGE_CODEC]._d;
  return bl_image_codec_find_by_name_view(self, name, size, codecs);
}

BL_API_IMPL BLResult bl_image_codec_destroy(BLImageCodecCore* self) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);

  return ObjectLayer::release_object(self);
}

// ImageCodec - API - Reset
// ========================

BL_API_IMPL BLResult bl_image_codec_reset(BLImageCodecCore* self) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

// ImageCodec - API - Assign
// =========================

BL_API_IMPL BLResult bl_image_codec_assign_move(BLImageCodecCore* self, BLImageCodecCore* other) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_image_codec_assign_weak(BLImageCodecCore* self, const BLImageCodecCore* other) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

// ImageCodec - API - Equality
// ===========================

BL_API_IMPL bool bl_image_codec_equals(const BLImageCodecCore* a, const BLImageCodecCore* b) noexcept {
  return a->_impl<BLImageCodecImpl>() == b->_impl<BLImageCodecImpl>();
}

// ImageCodec - API - Inspect Data
// ===============================

BL_API_IMPL uint32_t bl_image_codec_inspect_data(const BLImageCodecCore* self, const void* data, size_t size) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_virt()->inspect_data(self, static_cast<const uint8_t*>(data), size);
}

// ImageCodec - API - Find By Name & Extension & Data
// ==================================================

BL_API_IMPL BLResult bl_image_codec_find_by_name_cstr(BLImageCodecCore* self, const char* name, const BLArrayCore* codecs) noexcept {
  if (BL_UNLIKELY(name == nullptr)) {
    return bl_make_error(BL_ERROR_IMAGE_NO_MATCHING_CODEC);
  }

  size_t size = strlen(name);
  return bl_image_codec_find_by_name_view(self, name, size, codecs);
}

BL_API_IMPL BLResult bl_image_codec_find_by_name_view(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (codecs) {
    return find_codec_by_name(self, name, size, codecs);
  }
  else {
    return builtin_codecs_mutex->protect_shared([&] { return find_codec_by_name(self, name, size, &builtin_codecs_array); });
  }
}

BL_API_IMPL BLResult bl_image_codec_find_by_extension_cstr(BLImageCodecCore* self, const char* extension, const BLArrayCore* codecs) noexcept {
  if (BL_UNLIKELY(extension == nullptr)) {
    return bl_make_error(BL_ERROR_IMAGE_NO_MATCHING_CODEC);
  }

  size_t size = strlen(extension);
  return bl_image_codec_find_by_extension_view(self, extension, size, codecs);
}

BL_API_IMPL BLResult bl_image_codec_find_by_extension_view(BLImageCodecCore* self, const char* extension, size_t size, const BLArrayCore* codecs) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  extension = keep_only_extension_in_match(extension, size);
  if (codecs) {
    return find_codec_by_extension(self, extension, size, codecs);
  }
  else {
    return builtin_codecs_mutex->protect_shared([&] { return find_codec_by_extension(self, extension, size, &builtin_codecs_array); });
  }
}

BL_API_IMPL BLResult bl_image_codec_find_by_data(BLImageCodecCore* self, const void* data, size_t size, const BLArrayCore* codecs) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (codecs) {
    return find_codec_by_data(self, data, size, codecs);
  }
  else {
    return builtin_codecs_mutex->protect_shared([&] { return find_codec_by_data(self, data, size, &builtin_codecs_array); });
  }
}

BL_API_IMPL BLResult bl_image_codec_create_decoder(const BLImageCodecCore* self, BLImageDecoderCore* dst) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_d.virt<BLImageCodecVirt>()->create_decoder(self, dst);
}

BL_API_IMPL BLResult bl_image_codec_create_encoder(const BLImageCodecCore* self, BLImageEncoderCore* dst) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return self->_d.virt<BLImageCodecVirt>()->create_encoder(self, dst);
}

// ImageCodec - API - Built-In Codecs (Global)
// ===========================================

BL_API_IMPL BLResult bl_image_codec_array_init_built_in_codecs(BLArrayCore* self) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);

  *self = builtin_codecs_mutex->protect_shared([&] {
    BLArrayCore tmp = builtin_codecs_array();
    ObjectLayer::retain_object(&tmp);
    return tmp;
  });

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_image_codec_array_assign_built_in_codecs(BLArrayCore* self) noexcept {
  bl_array_destroy(self);
  return bl_image_codec_array_init_built_in_codecs(self);
}

BL_API_IMPL BLResult bl_image_codec_add_to_built_in(const BLImageCodecCore* codec) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(codec));

  return builtin_codecs_mutex->protect([&] {
    size_t i = builtin_codecs_array->index_of(codec->dcast());
    if (i != SIZE_MAX) {
      return bl_make_error(BL_ERROR_ALREADY_EXISTS);
    }
    return builtin_codecs_array->append(codec->dcast());
  });
}

BL_API_IMPL BLResult bl_image_codec_remove_from_built_in(const BLImageCodecCore* codec) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(codec));

  return builtin_codecs_mutex->protect([&] {
    size_t i = builtin_codecs_array->index_of(codec->dcast());
    if (i == SIZE_MAX) {
      return bl_make_error(BL_ERROR_NO_ENTRY);
    }
    return builtin_codecs_array->remove(i);
  });
}

// ImageCodec - Runtime Registration
// =================================

static void BL_CDECL bl_image_codec_rt_shutdown([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);

  builtin_codecs_array.destroy();
  builtin_codecs_mutex.destroy();
}

void bl_image_codec_rt_init(BLRuntimeContext* rt) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);
  static constexpr BLObjectType object_type = BLImageCodec::Info::kType;

  builtin_codecs_mutex.init();
  builtin_codecs_array.init();

  // Initialize default BLImageCodec.
  null_codec.virt.base.destroy = null_codec_destroy;
  null_codec.virt.base.get_property = bl_object_impl_get_property;
  null_codec.virt.base.set_property = bl_object_impl_set_property;
  null_codec.virt.inspect_data = null_codec_inspect_data;
  null_codec.virt.create_decoder = null_codec_create_decoder;
  null_codec.virt.create_encoder = null_codec_create_encoder;
  null_codec.impl->ctor();

  ObjectLayer::init_virt(&bl_object_defaults[object_type], object_type, 0u, &null_codec.impl, &null_codec.virt);

  rt->shutdown_handlers.add(bl_image_codec_rt_shutdown);
}

void bl_register_built_in_codecs(BLRuntimeContext* rt) noexcept {
  BL_USING_DETAIL(bl::ImageCodecDetail);

  BLArray<BLImageCodec>* codecs = &builtin_codecs_array;
  codecs->reserve(4);
  bl::Bmp::bmp_codec_on_init(rt, codecs);
  bl::Jpeg::jpeg_codec_on_init(rt, codecs);
  bl::Png::png_codec_on_init(rt, codecs);
  bl::Qoi::qoi_codec_on_init(rt, codecs);
}

#endif // !defined(BL_BUILD_NO_CODECS)
