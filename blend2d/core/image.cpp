// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_ops.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/file_system.h>
#include <blend2d/core/format_p.h>
#include <blend2d/core/image_p.h>
#include <blend2d/core/image_codec_p.h>
#include <blend2d/core/image_decoder_p.h>
#include <blend2d/core/image_encoder_p.h>
#include <blend2d/core/image_scale_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/pixel_converter_p.h>
#include <blend2d/core/runtime_p.h>

namespace bl::ImageDetail {

// Image - Null
// ============

static consteval ImagePrivateImpl make_eternal_image_impl() noexcept {
  return ImagePrivateImpl{};
}

static constexpr ObjectLayer::EternalImpl<ImagePrivateImpl> null_image {
  .padding{},
  .header{},
  .impl = make_eternal_image_impl()
};

// Image - Constants
// =================

static constexpr uint32_t image_large_data_alignment = 64;
static constexpr uint32_t image_large_data_threshold = 1024;
static constexpr uint32_t image_max_addressable_offset = 0x7FFFFFFFu;

// Image - Utilities
// =================

static BL_INLINE uint32_t image_stride_for_width(uint32_t width, uint32_t depth) noexcept {
  return (uint32_t(width) * depth + 7u) / 8u;
}

static BL_INLINE bool image_check_size_and_format(int w, int h, BLFormat format) noexcept {
  return !(unsigned(w) - 1u >= BL_RUNTIME_MAX_IMAGE_SIZE ||
           unsigned(h) - 1u >= BL_RUNTIME_MAX_IMAGE_SIZE ||
           unsigned(format) - 1u >= BL_FORMAT_MAX_VALUE);
}

static BL_INLINE BLResultOr<intptr_t> image_stride_from_create_params(int w, int h, BLFormat format) noexcept {
  if (BL_UNLIKELY(!image_check_size_and_format(w, h, format))) {
    BLResult result =
      (w <= 0 || h <= 0 || unsigned(format - 1) >= BL_FORMAT_MAX_VALUE)
        ? BL_ERROR_INVALID_VALUE
        : BL_ERROR_IMAGE_TOO_LARGE;
    return BLResultOr<intptr_t>{result, 0};
  }
  else {
    uint32_t bytes_per_line = image_stride_for_width(uint32_t(w), bl_format_info[format].depth);
    uint64_t bytes_per_image = uint64_t(bytes_per_line) * unsigned(h);

    // NOTE: Align the stride to 16 bytes if bytes_per_line is not too small. The reason is that when multi-threaded
    // rendering is used and bytes_per_line not aligned, some bands could share a cache line, which would potentially
    // affect the performance in a very negative way.
    if (bytes_per_line > 256u)
      bytes_per_line = axl::align_up(bytes_per_line, 16);

    BLResult result = bytes_per_image <= image_max_addressable_offset ? BL_SUCCESS : BL_ERROR_IMAGE_TOO_LARGE;
    return BLResultOr<intptr_t>{result, intptr_t(bytes_per_line)};
  }
}

// Make sure that the external image won't cause any kind of overflow in rasterization and texture fetching.
static BL_INLINE BLResult image_check_create_from_data_params(int w, int h, BLFormat format, intptr_t stride) noexcept {
  uint32_t minimum_stride = image_stride_for_width(uint32_t(w), bl_format_info[format].depth);
  uintptr_t bytes_per_line = uintptr_t(bl_abs(stride));

  if (BL_UNLIKELY(!image_check_size_and_format(w, h, format) || bytes_per_line < minimum_stride)) {
    return BL_ERROR_INVALID_VALUE;
  }

  // Make sure that the image height multiplied by stride is not greater than 2^31 - this makes sure that we
  // can handle also negative strides properly and that we can guarantee that all pixels are addressable via
  // 32-bit offsets, which is required by some SIMD fetchers.
  //
  // NOTE: BytesPerImage also considers all parent images in case this image is indeed a sub-image. The reason
  // is that we have to address all pixels in this sub-image too, so basically we include the parent in the
  // computation as well. Since a sub-image may be managed by the user (outside of Blend2D) we have to check
  // this every time an external pixel data is used.
  uint64_t bytes_per_image = uint64_t(bytes_per_line) * uint64_t(uint32_t(h));
  return bytes_per_line > uintptr_t(image_max_addressable_offset) || bytes_per_image > image_max_addressable_offset ? BL_ERROR_IMAGE_TOO_LARGE : BL_SUCCESS;
}

static void image_copy_data(uint8_t* dst_data, intptr_t dst_stride, const uint8_t* src_data, intptr_t src_stride, int w, int h, BLFormat format) noexcept {
  size_t bytes_per_line = (size_t(unsigned(w)) * bl_format_info[format].depth + 7u) / 8u;

  if (intptr_t(bytes_per_line) == dst_stride && intptr_t(bytes_per_line) == src_stride) {
    // Special case that happens often - stride equals bytes-per-line (no gaps).
    memcpy(dst_data, src_data, bytes_per_line * unsigned(h));
  }
  else {
    // Generic case - there are either gaps or source/destination is a sub-image.
    size_t gap = dst_stride > 0 ? size_t(dst_stride) - bytes_per_line : size_t(0);
    for (unsigned y = unsigned(h); y; y--) {
      memcpy(dst_data, src_data, bytes_per_line);
      axl::fill_items(dst_data + bytes_per_line, gap, uint8_t(0));

      dst_data += dst_stride;
      src_data += src_stride;
    }
  }
}

// Image - Alloc & Free Impl
// =========================

static BL_INLINE BLResult image_init_impl_data(ImagePrivateImpl* impl, int w, int h, BLFormat format, void* pixel_data, intptr_t stride) noexcept {
  impl->pixel_data = pixel_data;
  impl->size.reset(w, h);
  impl->stride = stride;
  impl->format = uint8_t(format);
  impl->flags = uint8_t(0);
  impl->depth = uint16_t(bl_format_info[format].depth);
  memset(impl->reserved, 0, sizeof(impl->reserved));
  return BL_SUCCESS;
}

static BL_NOINLINE BLResult image_alloc_impl(BLImageCore* self, int w, int h, BLFormat format, intptr_t stride) noexcept {
  BL_ASSERT(w > 0);
  BL_ASSERT(h > 0);
  BL_ASSERT(format != BL_FORMAT_NONE && format <= BL_FORMAT_MAX_VALUE);
  BL_ASSERT(stride > 0);

  size_t kBaseImplSize = axl::align_up(sizeof(ImagePrivateImpl), ObjectLayer::kMinImplAlignment);
  size_t pixel_data_size = size_t(h) * size_t(stride);

  ObjectImplSize impl_size = ObjectImplSize(kBaseImplSize + pixel_data_size);
  if (pixel_data_size >= image_large_data_threshold) {
    impl_size = ObjectImplSize(size_t(impl_size) + image_large_data_alignment - ObjectLayer::kMinImplAlignment);
  }

  BL_PROPAGATE(ObjectLayer::alloc_impl_t<ImagePrivateImpl>(self, BL_OBJECT_TYPE_IMAGE, impl_size));

  ImagePrivateImpl* impl = ObjectLayer::get_impl(self);
  uint8_t* pixel_data = reinterpret_cast<uint8_t*>(impl) + kBaseImplSize;

  if (pixel_data_size >= image_large_data_threshold) {
    pixel_data = axl::align_up(pixel_data, image_large_data_alignment);
  }

  image_init_impl_data(impl, w, h, format, pixel_data, stride);
  impl->writer_count = 0;
  return BL_SUCCESS;
}

static BL_NOINLINE BLResult image_alloc_external(BLImageCore* self, int w, int h, BLFormat format, void* pixel_data, intptr_t stride, BLDataAccessMode access_mode, BLDestroyExternalDataFunc destroy_func, void* user_data) noexcept {
  BL_ASSERT(w > 0);
  BL_ASSERT(h > 0);
  BL_ASSERT(format != BL_FORMAT_NONE && format <= BL_FORMAT_MAX_VALUE);

  BL_PROPAGATE(ObjectLayer::alloc_external_impl(self, ObjectImplSize{sizeof(ImagePrivateImpl)}, {
    .object_type = BL_OBJECT_TYPE_IMAGE,
    .access_mode = access_mode,
    .external_data = pixel_data,
    .size = 0,
    .capacity = 0,
    .destroy_func = destroy_func,
    .user_data = user_data
  }));

  ImagePrivateImpl* impl = ObjectLayer::get_impl(self);
  image_init_impl_data(impl, w, h, format, pixel_data, stride);
  impl->writer_count = 0;
  return BL_SUCCESS;
}

BLResult free_impl(ImagePrivateImpl* impl) noexcept {
  // Postpone the deletion in case that the image still has writers attached. This is required as the rendering
  // context doesn't manipulate the reference count of `BLImage` (otherwise it would not be possible to attach
  // multiple rendering contexts, for example).
  if (impl->writer_count != 0) {
    return BL_SUCCESS;
  }

  if (ObjectLayer::get_impl_header(impl)->is_external()) {
    ObjectLayer::call_external_destroy_func(impl, impl->pixel_data);
  }

  return ObjectLayer::dealloc_impl(impl);
}

} // {bl::ImageDetail}

// Image - API - Init & Destroy
// ============================

BL_API_IMPL BLResult bl_image_init(BLImageCore* self) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);

  self->_d = bl_object_defaults[BL_OBJECT_TYPE_IMAGE]._d;

  // STATIC ANALYSIS: Make sure static analysis doesn't assume that a Null eternal image is reference-counted.
  BL_ASSERT(!self->_d.is_ref_counted());

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_image_init_move(BLImageCore* self, BLImageCore* other) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_image_init_weak(BLImageCore* self, const BLImageCore* other) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object(self, other);
}

BL_API_IMPL BLResult bl_image_init_as(BLImageCore* self, int w, int h, BLFormat format) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);

  self->_d = bl_object_defaults[BL_OBJECT_TYPE_IMAGE]._d;

  // STATIC ANALYSIS: Make sure static analysis doesn't assume that a Null eternal image is reference-counted.
  BL_ASSERT(!self->_d.is_ref_counted());

  return bl_image_create(self, w, h, format);
}

BL_API_IMPL BLResult bl_image_init_as_from_data(
    BLImageCore* self, int w, int h, BLFormat format,
    void* pixel_data, intptr_t stride,
    BLDataAccessMode access_mode,
    BLDestroyExternalDataFunc destroy_func, void* user_data
) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);

  self->_d = bl_object_defaults[BL_OBJECT_TYPE_IMAGE]._d;

  // STATIC ANALYSIS: Make sure static analysis doesn't assume that a Null eternal image is reference-counted.
  BL_ASSERT(!self->_d.is_ref_counted());

  return bl_image_create_from_data(self, w, h, format, pixel_data, stride, access_mode, destroy_func, user_data);
}

BL_API_IMPL BLResult bl_image_destroy(BLImageCore* self) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object(self);
}

// Image - API - Reset
// ===================

BL_API_IMPL BLResult bl_image_reset(BLImageCore* self) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

// Image - API - Assign
// ====================

BL_API_IMPL BLResult bl_image_assign_move(BLImageCore* self, BLImageCore* other) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_image_assign_weak(BLImageCore* self, const BLImageCore* other) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

BL_API_IMPL BLResult bl_image_assign_deep(BLImageCore* self, const BLImageCore* other) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  ImagePrivateImpl* self_impl = ObjectLayer::get_impl(self);
  ImagePrivateImpl* other_impl = ObjectLayer::get_impl(other);

  BLSizeI size = other_impl->size;
  BLFormat format = BLFormat(other_impl->format);

  if (format == BL_FORMAT_NONE)
    return bl_image_reset(self);

  BLImageData dummy_image_data;
  if (self_impl == other_impl)
    return bl_image_make_mutable(self, &dummy_image_data);

  BL_PROPAGATE(bl_image_create(self, size.w, size.h, format));
  self_impl = ObjectLayer::get_impl(self);

  image_copy_data(static_cast<uint8_t*>(self_impl->pixel_data), self_impl->stride,
                         static_cast<uint8_t*>(other_impl->pixel_data), other_impl->stride, size.w, size.h, format);
  return BL_SUCCESS;
}

// Image - API - Create
// ====================

BL_API_IMPL BLResult bl_image_create(BLImageCore* self, int w, int h, BLFormat format) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLResultOr<intptr_t> wrapped = image_stride_from_create_params(w, h, format);
  if (BL_UNLIKELY(!wrapped.ok())) {
    if ((w | h) == 0 && format == BL_FORMAT_NONE) {
      return bl_image_reset(self);
    }
    else {
      return wrapped.result();
    }
  }

  ImagePrivateImpl* self_impl = ObjectLayer::get_impl(self);
  if (self_impl->size == BLSizeI(w, h) && self_impl->format == format && !ObjectLayer::get_impl_header(self_impl)->is_external()) {
    if (ObjectLayer::is_impl_mutable(self_impl)) {
      return BL_SUCCESS;
    }
  }

  BLImageCore new_o;
  BL_PROPAGATE(image_alloc_impl(&new_o, w, h, format, wrapped.value()));

  return ObjectLayer::replace_object(self, &new_o);
}

BL_API_IMPL BLResult bl_image_create_from_data(
    BLImageCore* self, int w, int h, BLFormat format,
    void* pixel_data, intptr_t stride,
    BLDataAccessMode access_mode,
    BLDestroyExternalDataFunc destroy_func, void* user_data
) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLResult result = image_check_create_from_data_params(w, h, format, stride);
  if (BL_UNLIKELY(result != BL_SUCCESS)) {
    return bl_make_error(result);
  }

  // OPTIMIZATION: If the user code calls `BLImage::create_from_data()` often on the same image, reuse the
  // already allocated Impl if it's both unique and external to avoid an allocation roundtrip for each call.
  if (ObjectLayer::get_impl_header(self->_d.impl())->is_external()) {
    ImagePrivateImpl* impl = ObjectLayer::get_impl(self);
    if (ObjectLayer::is_impl_unique(impl) && impl->writer_count == 0) {
      ObjectLayer::call_external_destroy_func(impl, impl->pixel_data);
      ObjectLayer::init_external_header(impl, destroy_func, user_data);
      ObjectLayer::init_impl_ref_count(impl, ObjectLayer::ref_count_base_from_access_mode(access_mode));
      return image_init_impl_data(impl, w, h, format, pixel_data, stride);
    }
  }

  BLImageCore new_o;
  BL_PROPAGATE(image_alloc_external(&new_o, w, h, format, pixel_data, stride, access_mode, destroy_func, user_data));
  return ObjectLayer::replace_object(self, &new_o);
}

// Image - API - Accessors
// =======================

BL_API_IMPL BLResult bl_image_get_data(const BLImageCore* self, BLImageData* data_out) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  ImagePrivateImpl* self_impl = ObjectLayer::get_impl(self);
  data_out->pixel_data = self_impl->pixel_data;
  data_out->stride = self_impl->stride;
  data_out->size = self_impl->size;
  data_out->format = self_impl->format;
  data_out->flags = 0;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_image_make_mutable(BLImageCore* self, BLImageData* data_out) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  ImagePrivateImpl* self_impl = ObjectLayer::get_impl(self);
  BLSizeI size = self_impl->size;
  BLFormat format = (BLFormat)self_impl->format;

  if (format != BL_FORMAT_NONE && !ObjectLayer::is_impl_mutable(self_impl)) {
    intptr_t stride = intptr_t(image_stride_for_width(uint32_t(size.w), bl_format_info[format].depth));

    BLImageCore new_o;
    BL_PROPAGATE(image_alloc_impl(&new_o, size.w, size.h, format, stride));

    ImagePrivateImpl* new_impl = ObjectLayer::get_impl(&new_o);
    data_out->pixel_data = new_impl->pixel_data;
    data_out->stride = new_impl->stride;
    data_out->size = size;
    data_out->format = format;
    data_out->flags = 0;

    image_copy_data(static_cast<uint8_t*>(new_impl->pixel_data), new_impl->stride,
                    static_cast<uint8_t*>(self_impl->pixel_data), self_impl->stride, size.w, size.h, format);

    return ObjectLayer::replace_object(self, &new_o);
  }
  else {
    data_out->pixel_data = self_impl->pixel_data;
    data_out->stride = self_impl->stride;
    data_out->size = size;
    data_out->format = format;
    data_out->flags = 0;
    return BL_SUCCESS;
  }
}

// Image - API - Convert
// =====================

BL_API_IMPL BLResult bl_image_convert(BLImageCore* self, BLFormat format) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  ImagePrivateImpl* self_impl = ObjectLayer::get_impl(self);
  FormatExt src_format = FormatExt(self_impl->format);
  FormatExt dst_format = FormatExt(format);

  if (dst_format == src_format) {
    return BL_SUCCESS;
  }

  if (dst_format == FormatExt::kXRGB32) {
    dst_format = FormatExt::kFRGB32;
  }

  if (src_format == FormatExt::kNone) {
    return bl_make_error(BL_ERROR_NOT_INITIALIZED);
  }

  BLResult result = BL_SUCCESS;
  BLPixelConverterCore pc {};

  BLSizeI size = self_impl->size;
  const BLFormatInfo& di = bl_format_info[size_t(dst_format)];
  const BLFormatInfo& si = bl_format_info[size_t(src_format)];

  // Save some cycles by calling `pixel_converter_init_internal` as we don't need to sanitize the destination and
  // source formats in this case.
  if (pixel_converter_init_internal(&pc, di, si, BL_PIXEL_CONVERTER_CREATE_NO_FLAGS) != BL_SUCCESS) {
    // Built-in formats should always have a built-in converter, so report a different error if the initialization
    // failed. This is pretty critical.
    return bl_make_error(BL_ERROR_INVALID_STATE);
  }

  if (di.depth == si.depth && ObjectLayer::is_impl_mutable(self_impl)) {
    // Prefer in-place conversion if the depths are equal and the image mutable.
    pc.convert_func(&pc, static_cast<uint8_t*>(self_impl->pixel_data), self_impl->stride,
                        static_cast<uint8_t*>(self_impl->pixel_data), self_impl->stride, uint32_t(size.w), uint32_t(size.h), nullptr);
    self_impl->format = uint8_t(format);
  }
  else {
    BLImageCore dst_image;
    result = bl_image_init_as(&dst_image, size.w, size.h, format);

    if (result == BL_SUCCESS) {
      ImagePrivateImpl* dst_impl = ObjectLayer::get_impl(&dst_image);
      BLPixelConverterOptions opt {};

      opt.gap = uintptr_t(bl_abs(dst_impl->stride)) - uintptr_t(uint32_t(size.w)) * (dst_impl->depth / 8u);
      pc.convert_func(&pc, static_cast<uint8_t*>(dst_impl->pixel_data), dst_impl->stride,
                          static_cast<uint8_t*>(self_impl->pixel_data), self_impl->stride, uint32_t(size.w), uint32_t(size.h), &opt);

      return ObjectLayer::replace_object(self, &dst_image);
    }
  }

  bl_pixel_converter_reset(&pc);
  return result;
}

// Image - API - Equality & Comparison
// ===================================

BL_API_IMPL bool bl_image_equals(const BLImageCore* a, const BLImageCore* b) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  const ImagePrivateImpl* a_impl = ObjectLayer::get_impl(a);
  const ImagePrivateImpl* b_impl = ObjectLayer::get_impl(b);

  if (a_impl == b_impl) {
    return true;
  }

  if (a_impl->size != b_impl->size || a_impl->format != b_impl->format) {
    return false;
  }

  uint32_t w = uint32_t(a_impl->size.w);
  uint32_t h = uint32_t(a_impl->size.h);

  const uint8_t* a_data = static_cast<const uint8_t*>(a_impl->pixel_data);
  const uint8_t* b_data = static_cast<const uint8_t*>(b_impl->pixel_data);

  intptr_t a_stride = a_impl->stride;
  intptr_t b_stride = b_impl->stride;

  size_t bytes_per_line = (w * bl_format_info[a_impl->format].depth + 7u) / 8u;
  for (uint32_t y = 0; y < h; y++) {
    if (memcmp(a_data, b_data, bytes_per_line) != 0) {
      return false;
    }
    a_data += a_stride;
    b_data += b_stride;
  }

  return true;
}

// Image - API - Scale
// ===================

BL_API_IMPL BLResult bl_image_scale(BLImageCore* dst, const BLImageCore* src, const BLSizeI* size, BLImageScaleFilter filter) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(dst));
  BL_ASSERT(ObjectLayer::is_object_valid(src));

  ImagePrivateImpl* src_impl = ObjectLayer::get_impl(src);
  if (src_impl->format == BL_FORMAT_NONE) {
    return bl_image_reset(dst);
  }

  ImageScaleContext scale_ctx;
  BL_PROPAGATE(scale_ctx.create(*size, src_impl->size, filter));

  BLFormat format = BLFormat(src_impl->format);
  int tw = scale_ctx.dst_width();
  int th = scale_ctx.src_height();

  BLImage tmp;
  BLImageData buf;

  if (th == scale_ctx.dst_height() || tw == scale_ctx.src_width()) {
    // Only horizontal or vertical scale.

    // Move to `tmp` so it's not destroyed by `dst->create()`.
    if (dst == src)
      tmp = src->dcast();

    BL_PROPAGATE(bl_image_create(dst, scale_ctx.dst_width(), scale_ctx.dst_height(), format));
    BL_PROPAGATE(bl_image_make_mutable(dst, &buf));

    if (th == scale_ctx.dst_height())
      scale_ctx.process_horz_data(static_cast<uint8_t*>(buf.pixel_data), buf.stride, static_cast<const uint8_t*>(src_impl->pixel_data), src_impl->stride, format);
    else
      scale_ctx.process_vert_data(static_cast<uint8_t*>(buf.pixel_data), buf.stride, static_cast<const uint8_t*>(src_impl->pixel_data), src_impl->stride, format);
  }
  else {
    // Both horizontal and vertical scale.
    BL_PROPAGATE(tmp.create(tw, th, format));
    BL_PROPAGATE(tmp.make_mutable(&buf));
    scale_ctx.process_horz_data(static_cast<uint8_t*>(buf.pixel_data), buf.stride, static_cast<const uint8_t*>(src_impl->pixel_data), src_impl->stride, format);

    src_impl = tmp._impl<ImagePrivateImpl>();
    BL_PROPAGATE(bl_image_create(dst, scale_ctx.dst_width(), scale_ctx.dst_height(), format));
    BL_PROPAGATE(bl_image_make_mutable(dst, &buf));

    scale_ctx.process_vert_data(static_cast<uint8_t*>(buf.pixel_data), buf.stride, static_cast<const uint8_t*>(src_impl->pixel_data), src_impl->stride, format);
  }

  return BL_SUCCESS;
}

// Image - API - Read File
// =======================

#if !defined(BL_BUILD_NO_CODECS) && !defined(BL_BUILD_NO_FILESYSTEM)
BL_API_IMPL BLResult bl_image_read_from_file(BLImageCore* self, const char* file_name, const BLArrayCore* codecs) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return bl_image_read_from_file_with_info(self, file_name, codecs, nullptr);
}

BL_API_IMPL BLResult bl_image_read_from_file_with_info(BLImageCore* self, const char* file_name, const BLArrayCore* codecs, BLImageInfo* info_out) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLArray<uint8_t> buffer;
  BL_PROPAGATE(BLFileSystem::read_file(file_name, buffer));

  if (buffer.is_empty()) {
    return bl_make_error(BL_ERROR_FILE_EMPTY);
  }

  BLImageCodec codec;
  BL_PROPAGATE(bl_image_codec_find_by_data(&codec, buffer.data(), buffer.size(), codecs));

  if (BL_UNLIKELY(!(codec.features() & BL_IMAGE_CODEC_FEATURE_READ))) {
    return bl_make_error(BL_ERROR_IMAGE_DECODER_NOT_PROVIDED);
  }

  BLImageDecoder decoder;
  BL_PROPAGATE(codec.create_decoder(&decoder));

  if (info_out != nullptr) {
    BL_PROPAGATE(decoder.read_info(*info_out, buffer));
  }

  return decoder.read_frame(*self, buffer);
}
#endif // !BL_BUILD_NO_CODECS && !BL_BUILD_NO_FILESYSTEM

// Image - API - Read Data
// =======================

#if !defined(BL_BUILD_NO_CODECS)
BL_API_IMPL BLResult bl_image_read_from_data(BLImageCore* self, const void* data, size_t size, const BLArrayCore* codecs) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return bl_image_read_from_data_with_info(self, data, size, codecs, nullptr);
}

BL_API_IMPL BLResult bl_image_read_from_data_with_info(BLImageCore* self, const void* data, size_t size, const BLArrayCore* codecs, BLImageInfo* info_out) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLImageCodec codec;
  BL_PROPAGATE(bl_image_codec_find_by_data(&codec, data, size, codecs));

  if (BL_UNLIKELY(!(codec.features() & BL_IMAGE_CODEC_FEATURE_READ))) {
    return bl_make_error(BL_ERROR_IMAGE_DECODER_NOT_PROVIDED);
  }

  BLImageDecoder decoder;
  BL_PROPAGATE(codec.create_decoder(&decoder));

  if (info_out != nullptr) {
    BL_PROPAGATE(decoder.read_info(*info_out, data, size));
  }

  return decoder.read_frame(*self, data, size);
}
#endif // !BL_BUILD_NO_CODECS

// Image - API - Write File
// ========================

#if !defined(BL_BUILD_NO_CODECS) && !defined(BL_BUILD_NO_FILESYSTEM)
namespace bl::ImageDetail {

static BLResult image_write_to_file_internal(const BLImageCore* self, const char* file_name, const BLImageCodecCore* codec) noexcept {
  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(codec));

  BLArray<uint8_t> buffer;
  BL_PROPAGATE(bl_image_write_to_data(self, &buffer, codec));
  return BLFileSystem::write_file(file_name, buffer);
}

} // {bl::ImageDetail}

BL_API_IMPL BLResult bl_image_write_to_file(const BLImageCore* self, const char* file_name, const BLImageCodecCore* codec) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (!codec) {
    BLImageCodec local_codec;
    BL_PROPAGATE(local_codec.find_by_extension(file_name));
    return image_write_to_file_internal(self, file_name, &local_codec);
  }
  else {
    BL_ASSERT(ObjectLayer::is_object_valid(codec));
    return image_write_to_file_internal(self, file_name, codec);
  }
}
#endif // !BL_BUILD_NO_CODECS && !BL_BUILD_NO_FILESYSTEM

// Image - API - Write Data
// ========================

#if !defined(BL_BUILD_NO_CODECS)
BL_API_IMPL BLResult bl_image_write_to_data(const BLImageCore* self, BLArrayCore* dst, const BLImageCodecCore* codec) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(codec));

  if (BL_UNLIKELY(!(codec->dcast().features() & BL_IMAGE_CODEC_FEATURE_WRITE))) {
    return bl_make_error(BL_ERROR_IMAGE_ENCODER_NOT_PROVIDED);
  }

  BLImageEncoder encoder;
  BL_PROPAGATE(codec->dcast().create_encoder(&encoder));
  return encoder.write_frame(dst->dcast<BLArray<uint8_t>>(), self->dcast());
}
#endif // !BL_BUILD_NO_CODECS

// Image - Runtime Registration
// ============================

void bl_image_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  BL_USING_DETAIL(bl::ImageDetail);
  static constexpr BLObjectType object_type = BLImage::Info::kType;

  ObjectLayer::init_dyn(&bl_object_defaults[object_type], object_type, 0u, const_cast<ImagePrivateImpl*>(&null_image.impl), nullptr);
}
