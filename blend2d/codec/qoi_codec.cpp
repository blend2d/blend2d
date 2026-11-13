// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#if !defined(BL_BUILD_NO_CODECS)

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_ops.h>
#include <blend2d/axl/lookuptable_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/format_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/rgba.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/codec/qoi_codec_p.h>
#include <blend2d/codec/qoi_ops_p.h>
#include <blend2d/pixelops/scalar_p.h>

namespace bl::Qoi {

// Codecs - Qoi - Globals
// ======================

static constinit ObjectLayer::EternalVirtImpl<QoiCodecImpl, BLImageCodecVirt> qoi_codec{};

static BLImageCodecCore qoi_codec_object;
static BLImageDecoderVirt qoi_decoder_virt;
static BLImageEncoderVirt qoi_encoder_virt;

// Codecs - Qoi - Constants
// ========================

static constexpr uint8_t qoi_magic[kQoiMagicSize] = { 'q', 'o', 'i', 'f' };
static constexpr uint8_t qoi_end_marker[kQoiEndMarkerSize] = { 0, 0, 0, 0, 0, 0, 0, 1 };

// Codecs - Qoi - Decoder - Read Info (Internal)
// =============================================

// struct qoi_header {
//   char magic[4];      // magic bytes "qoif"
//   uint32_t width;     // image width in pixels (BE)
//   uint32_t height;    // image height in pixels (BE)
//   uint8_t channels;   // 3 = RGB, 4 = RGBA
//   uint8_t colorspace; // 0 = sRGB with linear alpha, 1 = all channels linear
// };

static BLResult qoi_decoder_read_info_internal(QoiDecoderImpl* impl, const uint8_t* data, size_t size) noexcept {
  if (size < kQoiHeaderSize) {
    return bl_make_error(BL_ERROR_DATA_TRUNCATED);
  }

  if (memcmp(qoi_magic, data, kQoiMagicSize) != 0) {
    return bl_make_error(BL_ERROR_INVALID_SIGNATURE);
  }

  uint32_t w = axl::loadu_u32_be(data + 4);
  uint32_t h = axl::loadu_u32_be(data + 8);

  if (w == 0 || h == 0) {
    return bl_make_error(BL_ERROR_INVALID_DATA);
  }

  uint8_t channels = data[12];
  uint8_t colorspace = data[13];

  if (channels != 3u && channels != 4u) {
    return bl_make_error(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
  }

  if (colorspace > 1u) {
    return bl_make_error(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
  }

  if (w > BL_RUNTIME_MAX_IMAGE_SIZE || h > BL_RUNTIME_MAX_IMAGE_SIZE) {
    return bl_make_error(BL_ERROR_IMAGE_TOO_LARGE);
  }

  impl->buffer_index = 14u;
  impl->image_info.reset();
  impl->image_info.size.reset(int32_t(w), int32_t(h));
  impl->image_info.depth = uint16_t(channels * 8);
  impl->image_info.plane_count = 1;
  impl->image_info.frame_count = 1;

  memcpy(impl->image_info.format, "QOI", 4);
  memcpy(impl->image_info.compression, "RLE", 4);

  return BL_SUCCESS;
}

static BLResult qoi_decoder_read_frame_internal(QoiDecoderImpl* impl, BLImage* image_out, const uint8_t* data, size_t size) noexcept {
  if (size < kQoiHeaderSize) {
    return bl_make_error(BL_ERROR_DATA_TRUNCATED);
  }

  const uint8_t* start = data;
  const uint8_t* end = data + size;

  uint32_t w = uint32_t(impl->image_info.size.w);
  uint32_t h = uint32_t(impl->image_info.size.h);

  uint32_t depth = impl->image_info.depth;
  BLFormat format = depth == 32 ? BL_FORMAT_PRGB32 : BL_FORMAT_XRGB32;

  data += kQoiHeaderSize;
  if (data >= end) {
    return bl_make_error(BL_ERROR_DATA_TRUNCATED);
  }

  BLImageData image_data;
  BL_PROPAGATE(image_out->create(int(w), int(h), format));
  BL_PROPAGATE(image_out->make_mutable(&image_data));

  uint8_t* dst_row = static_cast<uint8_t*>(image_data.pixel_data);
  intptr_t dst_stride = image_data.stride;

  BL_PROPAGATE(qoi_decode_data(dst_row, dst_stride, w, h, data, end, depth == 32));

  impl->buffer_index = axl::ptr_byte_diff(start, data);
  impl->frame_index++;

  return BL_SUCCESS;
}

// Codecs - Qoi - Decoder - Interface
// ==================================

static BLResult BL_CDECL qoi_decoder_restart(BLImageDecoderCore* self) noexcept {
  QoiDecoderImpl* impl = self->_impl<QoiDecoderImpl>();
  impl->last_result = BL_SUCCESS;
  impl->frame_index = 0;
  impl->buffer_index = 0;
  impl->image_info.reset();
  return BL_SUCCESS;
}

static BLResult BL_CDECL qoi_decoder_read_info(BLImageDecoderCore* self, BLImageInfo* info_out, const uint8_t* data, size_t size) noexcept {
  QoiDecoderImpl* impl = self->_impl<QoiDecoderImpl>();
  BLResult result = impl->last_result;

  if (impl->buffer_index == 0 && result == BL_SUCCESS) {
    result = qoi_decoder_read_info_internal(impl, data, size);
    if (result != BL_SUCCESS) {
      impl->last_result = result;
    }
  }

  if (info_out) {
    memcpy(info_out, &impl->image_info, sizeof(BLImageInfo));
  }

  return result;
}

static BLResult BL_CDECL qoi_decoder_read_frame(BLImageDecoderCore* self, BLImageCore* image_out, const uint8_t* data, size_t size) noexcept {
  BL_PROPAGATE(qoi_decoder_read_info(self, nullptr, data, size));

  QoiDecoderImpl* impl = self->_impl<QoiDecoderImpl>();
  if (impl->frame_index) {
    return bl_make_error(BL_ERROR_NO_MORE_DATA);
  }

  BLResult result = qoi_decoder_read_frame_internal(impl, static_cast<BLImage*>(image_out), data, size);
  if (result != BL_SUCCESS) {
    impl->last_result = result;
  }
  return result;
}

static BLResult BL_CDECL qoi_decoder_create(BLImageDecoderCore* self) noexcept {
  BL_PROPAGATE(ObjectLayer::alloc_virt_impl_t<QoiDecoderImpl>(self, &qoi_decoder_virt));
  self->_impl<QoiDecoderImpl>()->ctor(&qoi_codec_object);
  return qoi_decoder_restart(self);
}

static BLResult BL_CDECL qoi_decoder_destroy(BLObjectCore* self) noexcept {
  QoiDecoderImpl* impl = self->_d.impl<QoiDecoderImpl>();
  impl->dtor();
  return ObjectLayer::dealloc_impl(impl);
}

// Codecs - Qoi - Encoder - Interface
// ==================================

static BLResult BL_CDECL qoi_encoder_restart(BLImageEncoderCore* self) noexcept {
  QoiEncoderImpl* impl = self->_impl<QoiEncoderImpl>();
  impl->last_result = BL_SUCCESS;
  impl->frame_index = 0;
  impl->buffer_index = 0;
  return BL_SUCCESS;
}

static BLResult BL_CDECL qoi_encoder_write_frame(BLImageEncoderCore* self, BLArrayCore* dst, const BLImageCore* image) noexcept {
  QoiEncoderImpl* impl = self->_impl<QoiEncoderImpl>();
  BL_PROPAGATE(impl->last_result);

  BLArray<uint8_t>& buf = *static_cast<BLArray<uint8_t>*>(dst);
  const BLImage& img = *static_cast<const BLImage*>(image);

  if (img.is_empty()) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  BLImageData image_data;
  BL_PROPAGATE(img.get_data(&image_data));

  uint32_t w = uint32_t(image_data.size.w);
  uint32_t h = uint32_t(image_data.size.h);
  uint32_t format = image_data.format;

  uint32_t channels = format == BL_FORMAT_XRGB32 ? 3u : 4u;
  uint32_t max_bytes_per_encoded_pixel = channels + 1u;

  // NOTE: This should never overflow.
  uint64_t max_size = uint64_t(w) * uint64_t(h) * uint64_t(max_bytes_per_encoded_pixel) + kQoiHeaderSize + kQoiEndMarkerSize;

  if (BL_UNLIKELY(max_size >= uint64_t(SIZE_MAX))) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  uint8_t* dst_data;
  BL_PROPAGATE(buf.append_uninitialized(size_t(max_size), &dst_data));

  uint8_t* dst_ptr = dst_data;
  memcpy(dst_ptr, qoi_magic, kQoiMagicSize);
  axl::storeu_u32_be(dst_ptr + 4, w);
  axl::storeu_u32_be(dst_ptr + 8, h);
  dst_ptr[12] = uint8_t(channels);
  dst_ptr[13] = 0;
  dst_ptr += 14;

  const uint8_t* src_line = static_cast<const uint8_t*>(image_data.pixel_data);
  intptr_t src_stride = image_data.stride;

  switch (format) {
    case BL_FORMAT_A8:
      dst_ptr = qoi_encode_data_a8(dst_ptr, w, h, src_line, image_data.stride);
      break;

    case BL_FORMAT_XRGB32:
      // The whole QOI is a 1D pixel buffer, so make bulked loads easier if stride matches BPP perfectly.
      if (static_cast<uintptr_t>(src_stride) == static_cast<uintptr_t>(w) * 4u) {
        w *= h;
        h = 1u;
      }

      dst_ptr = qoi_encode_data_xrgb32(dst_ptr, w, h, src_line, image_data.stride);
      break;

    case BL_FORMAT_PRGB32:
      // The whole QOI is a 1D pixel buffer, so make bulked loads easier if stride matches BPP perfectly.
      if (static_cast<uintptr_t>(src_stride) == static_cast<uintptr_t>(w) * 4u) {
        w *= h;
        h = 1u;
      }

      dst_ptr = qoi_encode_data_prgb32(dst_ptr, w, h, src_line, image_data.stride);
      break;

    default:
      buf._set_size_unsafe(0);
      return bl_make_error(BL_ERROR_INVALID_STATE);
  }

  memcpy(dst_ptr, qoi_end_marker, kQoiEndMarkerSize);
  dst_ptr += kQoiEndMarkerSize;

  buf._set_size_from_end_ptr_unsafe(dst_ptr);
  return BL_SUCCESS;
}

static BLResult BL_CDECL qoi_encoder_create(BLImageEncoderCore* self) noexcept {
  BL_PROPAGATE(ObjectLayer::alloc_virt_impl_t<QoiEncoderImpl>(self, &qoi_encoder_virt));
  self->_impl<QoiEncoderImpl>()->ctor(&qoi_codec_object);
  return qoi_encoder_restart(self);
}

static BLResult BL_CDECL qoi_encoder_destroy(BLObjectCore* self) noexcept {
  QoiEncoderImpl* impl = self->_d.impl<QoiEncoderImpl>();
  impl->dtor();
  return ObjectLayer::dealloc_impl(impl);
}

// Codecs - Qoi - Interface
// ========================

static BLResult BL_CDECL qoi_codec_destroy(BLObjectCore* self) noexcept {
  // Built-in codecs are never destroyed.
  axl::maybe_unused(self);
  return BL_SUCCESS;
};

static uint32_t BL_CDECL qoi_codec_inspect_data(const BLImageCodecCore* self, const uint8_t* data, size_t size) noexcept {
  axl::maybe_unused(self);

  if (size == 0) {
    return 0;
  }

  size_t magic_size = bl_min<size_t>(size, kQoiMagicSize);
  if (memcmp(qoi_magic, data, magic_size) != 0) {
    return 0;
  }

  if (size < 12) {
    return uint32_t(magic_size);
  }

  uint32_t w = axl::loadu_u32_be(data + 4);
  uint32_t h = axl::loadu_u32_be(data + 8);

  if (w == 0 || h == 0) {
    return 0;
  }

  if (size < 14) {
    return uint32_t(magic_size + 1u);
  }

  uint8_t channels = data[12];
  uint8_t colorspace = data[13];

  if (channels != 3u && channels != 4u) {
    return 0;
  }

  if (colorspace > 1u) {
    return 0;
  }

  // A valid QOI header.
  return 100;
}

static BLResult BL_CDECL qoi_codec_create_decoder(const BLImageCodecCore* self, BLImageDecoderCore* dst) noexcept {
  axl::maybe_unused(self);

  BLImageDecoderCore tmp;
  BL_PROPAGATE(qoi_decoder_create(&tmp));
  return bl_image_decoder_assign_move(dst, &tmp);
}

static BLResult BL_CDECL qoi_codec_create_encoder(const BLImageCodecCore* self, BLImageEncoderCore* dst) noexcept {
  axl::maybe_unused(self);

  BLImageEncoderCore tmp;
  BL_PROPAGATE(qoi_encoder_create(&tmp));
  return bl_image_encoder_assign_move(dst, &tmp);
}

// Codecs - Qoi - Runtime Registration
// ===================================

void qoi_codec_on_init([[maybe_unused]] BLRuntimeContext* rt, BLArray<BLImageCodec>* codecs) noexcept {
  // Initialize QOI codec.
  qoi_codec.virt.base.destroy = qoi_codec_destroy;
  qoi_codec.virt.base.get_property = bl_object_impl_get_property;
  qoi_codec.virt.base.set_property = bl_object_impl_set_property;
  qoi_codec.virt.inspect_data = qoi_codec_inspect_data;
  qoi_codec.virt.create_decoder = qoi_codec_create_decoder;
  qoi_codec.virt.create_encoder = qoi_codec_create_encoder;
  qoi_codec.impl->features = BLImageCodecFeatures(BL_IMAGE_CODEC_FEATURE_READ     |
                                                  BL_IMAGE_CODEC_FEATURE_WRITE    |
                                                  BL_IMAGE_CODEC_FEATURE_LOSSLESS);
  qoi_codec.impl->name.dcast().assign("QOI");
  qoi_codec.impl->vendor.dcast().assign("Blend2D");
  qoi_codec.impl->mime_type.dcast().assign("image/qoi");
  qoi_codec.impl->extensions.dcast().assign("qoi");

  ObjectLayer::init_virt(&qoi_codec_object, BL_OBJECT_TYPE_IMAGE_CODEC, 0u, &qoi_codec.impl, &qoi_codec.virt);

  // Initialize QOI decoder virtual functions.
  qoi_decoder_virt.base.destroy = qoi_decoder_destroy;
  qoi_decoder_virt.base.get_property = bl_object_impl_get_property;
  qoi_decoder_virt.base.set_property = bl_object_impl_set_property;
  qoi_decoder_virt.restart = qoi_decoder_restart;
  qoi_decoder_virt.read_info = qoi_decoder_read_info;
  qoi_decoder_virt.read_frame = qoi_decoder_read_frame;

  // Initialize QOI encoder virtual functions.
  qoi_encoder_virt.base.destroy = qoi_encoder_destroy;
  qoi_encoder_virt.base.get_property = bl_object_impl_get_property;
  qoi_encoder_virt.base.set_property = bl_object_impl_set_property;
  qoi_encoder_virt.restart = qoi_encoder_restart;
  qoi_encoder_virt.write_frame = qoi_encoder_write_frame;

  codecs->append(qoi_codec_object.dcast());
}

} // {bl::Qoi}

#endif // !BL_BUILD_NO_CODECS
