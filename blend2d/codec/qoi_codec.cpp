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
#include <blend2d/pixelops/scalar_p.h>

#if BL_TARGET_ARCH_BITS >= 64
  #define BL_QOI_USE_64_BIT_ARITHMETIC
#endif

namespace bl::Qoi {

// Codecs - Qoi - Globals
// ======================

static constinit ObjectLayer::EternalVirtImpl<QoiCodecImpl, BLImageCodecVirt> qoi_codec{};

static BLImageCodecCore qoi_codec_object;
static BLImageDecoderVirt qoi_decoder_virt;
static BLImageEncoderVirt qoi_encoder_virt;

// Codecs - Qoi - Constants
// ========================

static constexpr size_t qoi_header_size = 14;
static constexpr size_t qoi_magic_size = 4;
static constexpr size_t qoi_end_marker_size = 8;

static constexpr uint8_t qoi_op_index = 0x00; // 00xxxxxx
static constexpr uint8_t qoi_op_diff  = 0x40; // 01xxxxxx
static constexpr uint8_t qoi_op_luma  = 0x80; // 10xxxxxx
static constexpr uint8_t qoi_op_run   = 0xC0; // 11xxxxxx
static constexpr uint8_t qoi_op_rgb   = 0xFE; // 11111110
static constexpr uint8_t qoi_op_rgba  = 0xFF; // 11111111

static constexpr uint32_t qoi_hash_r = 3u;
static constexpr uint32_t qoi_hash_g = 5u;
static constexpr uint32_t qoi_hash_b = 7u;
static constexpr uint32_t qoi_hash_a = 11u;
static constexpr uint32_t qoi_hash_mask = 0x3Fu;

static constexpr uint8_t qoi_magic[qoi_magic_size] = { 'q', 'o', 'i', 'f' };
static constexpr uint8_t qoi_end_marker[qoi_end_marker_size] = { 0, 0, 0, 0, 0, 0, 0, 1 };

// Lookup table generator that generates delta values for QOI_OP_DIFF and the first byte of QOI_OP_LUMA.
// Additionally, it provides values for a RLE run of a single pixel for a possible experimentation.
struct QoiIndexDiffLumaTableGen {
  static consteval uint32_t rgb(uint32_t r, uint32_t g, uint32_t b, uint32_t luma_mask) noexcept {
    return ((r & 0xFFu) << 24) |
           ((g & 0xFFu) << 16) |
           ((b & 0xFFu) <<  8) |
           ((luma_mask ) <<  0) ;
  }

  static consteval uint32_t diff(uint32_t b0) noexcept {
    return rgb(((b0 >> 4) & 0x3u) - 2u, ((b0 >> 2) & 0x3u) - 2u, ((b0 >> 0) & 0x3u) - 2u, 0x00u);
  }

  static consteval uint32_t luma(uint32_t b0) noexcept {
    return rgb(b0 - 40u, b0 - 32u, b0 - 40u, 0xFF);
  }

  static consteval uint32_t value(size_t idx) noexcept {
    return idx < 64u  ? diff(uint32_t(idx      )) :
           idx < 128u ? luma(uint32_t(idx - 64u)) : 0u;
  }
};

static constexpr LookupTable<uint32_t, 129> qoi_index_diff_luma_lut = make_lookup_table<uint32_t, 129, QoiIndexDiffLumaTableGen>();

// Codecs - Qoi - Hashing
// ======================

static BL_INLINE uint32_t qoi_hash_pixel_ag_rb_x64(uint64_t ag_rb) noexcept {
  ag_rb *= (uint64_t(qoi_hash_a) << ( 8 + 2)) + (uint64_t(qoi_hash_g) << (24 + 2)) +
           (uint64_t(qoi_hash_r) << (40 + 2)) + (uint64_t(qoi_hash_b) << (56 + 2)) ;
  return uint32_t(ag_rb >> 58);
}

static BL_INLINE uint32_t qoi_hash_pixel_ag_rb_x32(uint32_t ag, uint32_t rb) noexcept {
  ag *= ((qoi_hash_a << (0 + 2)) + (qoi_hash_g << (16 + 2)));
  rb *= ((qoi_hash_r << (8 + 2)) + (qoi_hash_b << (24 + 2)));

  return (ag + rb) >> 26;
}

static BL_INLINE uint32_t qoi_hash_pixel_rgba32(uint32_t pixel) noexcept {
#if defined(BL_QOI_USE_64_BIT_ARITHMETIC)
  return qoi_hash_pixel_ag_rb_x64(((uint64_t(pixel) << 24) | pixel) & 0x00FF00FF00FF00FFu);
#else
  return qoi_hash_pixel_ag_rb_x32(pixel & 0xFF00FF00u, pixel & 0x00FF00FFu);
#endif
}

static BL_INLINE uint32_t qoi_hash_pixel_a8(uint32_t a) noexcept {
  return (0xFFu * (qoi_hash_r + qoi_hash_g + qoi_hash_b) + a * qoi_hash_a) & qoi_hash_mask;
}

// Codecs - Qoi - Unpacked Pixel
// =============================

#if defined(BL_QOI_USE_64_BIT_ARITHMETIC)
struct QoiUnpackedPixel {
  uint64_t ag_rb; // Represents 0x00AA00GG00RR00BB.

  static BL_INLINE QoiUnpackedPixel unpack(uint32_t packed) noexcept {
    return QoiUnpackedPixel{((uint64_t(packed) << 24) | packed) & 0x00FF00FF00FF00FFu};
  }

  static BL_INLINE QoiUnpackedPixel unpack_rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a) noexcept {
    return QoiUnpackedPixel{(uint64_t(a) << 48) | (uint64_t(g) << 32) | (uint64_t(r) << 16) | (uint64_t(b) << 0)};
  }

  template<bool kHasAlpha>
  BL_INLINE uint32_t pack() const noexcept {
    uint32_t rgba32 = uint32_t(ag_rb >> 24) | uint32_t(ag_rb & 0xFFFFFFFFu);
    if constexpr (kHasAlpha)
      return PixelOps::Scalar::cvt_prgb32_8888_from_argb32_8888(rgba32);
    else
      return rgba32 | 0xFF000000u;
  }

  BL_INLINE uint32_t hash() const noexcept { return qoi_hash_pixel_ag_rb_x64(ag_rb); }

  BL_INLINE void add(const QoiUnpackedPixel& other) noexcept { ag_rb += other.ag_rb; }
  BL_INLINE void add_rb(uint32_t value) noexcept { ag_rb += value; }
  BL_INLINE void mask() noexcept { ag_rb &= 0x00FF00FF00FF00FFu; }

  BL_INLINE void op_rgbx(uint32_t hbyte0, const QoiUnpackedPixel& other) noexcept {
    uint64_t msk = uint64_t(hbyte0 + 1) << 48;
    ag_rb = (ag_rb & msk) | (other.ag_rb & ~msk);
  }
};
#else
struct QoiUnpackedPixel {
  uint32_t ag; // Represents 0xAA00GG00.
  uint32_t rb; // Represents 0x00RR00BB.

  static BL_INLINE QoiUnpackedPixel unpack(uint32_t packed) noexcept {
    return QoiUnpackedPixel{packed & 0xFF00FF00u, packed & 0x00FF00FFu };
  }

  static BL_INLINE QoiUnpackedPixel unpack_rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a) noexcept {
    return QoiUnpackedPixel{
      (uint32_t(a) << 24) | (uint32_t(g) << 8), // AG
      (uint32_t(r) << 16) | (uint32_t(b) << 0)  // RB
    };
  }

  template<bool kHasAlpha>
  BL_INLINE uint32_t pack() const noexcept {
    uint32_t rgba32 = ag | rb;

    if (kHasAlpha)
      return PixelOps::Scalar::cvt_prgb32_8888_from_argb32_8888(rgba32);
    else
      return rgba32 | 0xFF000000u;
  }

  BL_INLINE uint32_t hash() const noexcept { return qoi_hash_pixel_ag_rb_x32(ag, rb); }

  BL_INLINE void add(const QoiUnpackedPixel& other) noexcept { ag += other.ag; rb += other.rb; }
  BL_INLINE void add_rb(uint32_t value) noexcept { rb += value; }
  BL_INLINE void mask() noexcept { ag &= 0xFF00FF00u; rb &= 0x00FF00FFu; }

  BL_INLINE void op_rgbx(uint32_t hbyte0, const QoiUnpackedPixel& other) noexcept {
    uint32_t msk = uint32_t(hbyte0 + 1) << 24;
    ag = (ag & msk) | (other.ag & ~msk);
    rb = other.rb;
  }
};
#endif

// Codecs - Qoi - Utilities
// ========================

static BL_INLINE uint32_t* qoi_fill_rgba32(uint32_t* dst, uint32_t value, size_t count) noexcept {
  for (size_t i = 0; i < count; i++)
    dst[i] = value;
  return dst + count;
}

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
  if (size < qoi_header_size) {
    return bl_make_error(BL_ERROR_DATA_TRUNCATED);
  }

  if (memcmp(qoi_magic, data, qoi_magic_size) != 0) {
    return bl_make_error(BL_ERROR_INVALID_SIGNATURE);
  }

  uint32_t w = bl::axl::loadu_u32_be(data + 4);
  uint32_t h = bl::axl::loadu_u32_be(data + 8);

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

template<bool kHasAlpha>
static BL_INLINE BLResult qoi_decode_data(
  uint8_t* dst_row,
  intptr_t dst_stride,
  uint32_t w,
  uint32_t h,
  uint32_t packed_table[64],
  QoiUnpackedPixel unpacked_table[64],
  const uint8_t* src,
  const uint8_t* end) noexcept {

  constexpr size_t kMinRemainingBytesOfNextChunk = qoi_end_marker_size + 1u;

  uint32_t* dst_ptr = reinterpret_cast<uint32_t*>(dst_row);
  uint32_t* dst_end = dst_ptr + w;

  uint32_t packed_pixel = 0xFF000000;
  QoiUnpackedPixel unpacked_pixel = QoiUnpackedPixel::unpack(packed_pixel);

  // Edge case: If the image starts with QOI_OP_RUN, the repeated pixel must be
  // added to the pixel table, otherwise the decoder may produce incorrect result.
  {
    uint32_t hbyte0 = src[0];

    if (hbyte0 >= qoi_op_run && hbyte0 < qoi_op_run + 62u) {
      uint32_t hash = unpacked_pixel.hash();
      packed_table[hash] = packed_pixel;
      unpacked_table[hash] = unpacked_pixel;
    }
  }

  for (;;) {
    size_t remaining = axl::ptr_byte_diff(src, end);
    if (BL_UNLIKELY(remaining < kMinRemainingBytesOfNextChunk)) {
      return bl_make_error(BL_ERROR_DATA_TRUNCATED);
    }

    uint32_t hbyte0 = src[0];
    uint32_t hbyte1 = src[1];
    src++;

    if (hbyte0 < qoi_op_run) {
      // QOI_OP_INDEX + QOI_OP_DIFF + QOI_OP_LUMA
      // =========================================

      if (hbyte0 < 64u) {
        // Handle QOI_OP_INDEX - 6-bit index to a pixel table (hbyte0 = 0b00xxxxxx).
        packed_pixel = packed_table[hbyte0];
        unpacked_pixel = unpacked_table[hbyte0];

        *dst_ptr = packed_pixel;
        if (BL_LIKELY(++dst_ptr != dst_end)) {
          if (!(hbyte1 < 64u)) {
            continue;
          }

          packed_pixel = packed_table[hbyte1];
          unpacked_pixel = unpacked_table[hbyte1];
          src++;

          *dst_ptr = packed_pixel;
          if (++dst_ptr != dst_end) {
            continue;
          }
        }
        hbyte0 = 0;
      }
      else {
        // Handle QOI_OP_DIFF and QOI_OP_LUMA chunks.
        {
          src += hbyte0 >> 7;

          uint32_t packed_delta = qoi_index_diff_luma_lut[hbyte0 - 64u];
          hbyte1 &= packed_delta;
          packed_delta >>= 8;

          unpacked_pixel.add_rb((hbyte1 | (hbyte1 << 12)) & 0x000F000Fu);
          unpacked_pixel.add(QoiUnpackedPixel::unpack(packed_delta));
          unpacked_pixel.mask();
        }

store_pixel:
        hbyte0 = unpacked_pixel.hash();

        packed_pixel = unpacked_pixel.pack<kHasAlpha>();
        unpacked_table[hbyte0] = unpacked_pixel;

        *dst_ptr = packed_pixel;
        packed_table[hbyte0] = packed_pixel;

        if (++dst_ptr != dst_end)
          continue;

        hbyte0 = 0;
      }
    }
    else {
      // QOI_OP_RUN + QOI_OP_RGB + QOI_OP_RGBA
      // ======================================

      if (hbyte0 >= qoi_op_rgb) {
        // Handle both QOI_OP_RGB and QOI_OP_RGBA at the same time.
        unpacked_pixel.op_rgbx(hbyte0, QoiUnpackedPixel::unpack_rgba(hbyte1, src[1], src[2], src[3]));

        // Advance by either 3 (RGB) or 4 (RGBA) bytes.
        src += hbyte0 - 251u;
        goto store_pixel;
      }
      else {
        // Run-length encoding repeats the previous pixel by `(hbyte0 & 0x3F) + 1` times (N stored with a bias of -1).
        hbyte0 = (hbyte0 & 0x3Fu) + 1u;

store_rle:
        {
          size_t limit = (size_t)(dst_end - dst_ptr);
          size_t fill = bl_min<size_t>(hbyte0, limit);

          hbyte0 -= uint32_t(fill);
          dst_ptr = qoi_fill_rgba32(dst_ptr, packed_pixel, fill);

          if (dst_ptr != dst_end) {
            continue;
          }
        }
      }
    }

    if (BL_UNLIKELY(--h == 0)) {
      return BL_SUCCESS;
    }

    dst_row += dst_stride;
    dst_ptr = reinterpret_cast<uint32_t*>(dst_row);
    dst_end = dst_ptr + w;

    // True if we are inside an unfinished QOI_OP_RUN that spans across two or more rows.
    if (hbyte0 != 0) {
      goto store_rle;
    }
  }
}

static BLResult qoi_decoder_read_frame_internal(QoiDecoderImpl* impl, BLImage* image_out, const uint8_t* data, size_t size) noexcept {
  if (size < qoi_header_size)
    return bl_make_error(BL_ERROR_DATA_TRUNCATED);

  const uint8_t* start = data;
  const uint8_t* end = data + size;

  uint32_t w = uint32_t(impl->image_info.size.w);
  uint32_t h = uint32_t(impl->image_info.size.h);

  uint32_t depth = impl->image_info.depth;
  BLFormat format = depth == 32 ? BL_FORMAT_PRGB32 : BL_FORMAT_XRGB32;

  data += qoi_header_size;
  if (data >= end)
    return bl_make_error(BL_ERROR_DATA_TRUNCATED);

  BLImageData image_data;
  BL_PROPAGATE(image_out->create(int(w), int(h), format));
  BL_PROPAGATE(image_out->make_mutable(&image_data));

  uint8_t* dst_row = static_cast<uint8_t*>(image_data.pixel_data);
  intptr_t dst_stride = image_data.stride;

  uint32_t packed_table[64];
  qoi_fill_rgba32(packed_table, depth == 32 ? 0u : 0xFF000000u, 64);

  QoiUnpackedPixel unpacked_table[64] {};

  if (depth == 32)
    BL_PROPAGATE(qoi_decode_data<true>(dst_row, dst_stride, w, h, packed_table, unpacked_table, data, end));
  else
    BL_PROPAGATE(qoi_decode_data<false>(dst_row, dst_stride, w, h, packed_table, unpacked_table, data, end));

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
  return ObjectLayer::dealloc_impl(impl, {self->_d.meta_byte()});
}

// Codecs - Qoi - Encoder - Interface
// ==================================

// QOI isn't good for compressing alpha-only images - we can optimize the encoder's performance, but not the final size.
static uint8_t* qoi_encode_data_a8(uint8_t* dst_data, uint32_t w, uint32_t h, const uint8_t* src_data, intptr_t src_stride) noexcept {
  // NOTE: Use an initial value which is not representable, because the encoder/decoder starts with RGB==0,
  // which would decode badly into RGBA formats (the components would be zero and thus it would not be the
  // same as when used by Blend2D, which defaults to having RGB components the same as 0xFF premultiplied).
  uint32_t pixel = 0xFFFFFFFFu;
  uint16_t pixel_table[64];

  for (size_t i = 0; i < 64; i++) {
    pixel_table[i] = 0xFFFFu;
  }

  src_stride -= intptr_t(w);
  uint32_t x = w;

  for (;;) {
    uint32_t p = *src_data++;

    // Run length encoding.
    if (p == pixel) {
      size_t n = 1;
      x--;

      for (;;) {
        uint32_t prev_x = x;

        while (x) {
          p = *src_data++;
          if (p != pixel)
            break;
          x--;
        }

        n += size_t(prev_x - x);

        if (x == 0 && --h != 0) {
          src_data += src_stride;
          x = w;
        }
        else {
          break;
        }
      }

      do {
        size_t run = bl_min<size_t>(n, 62u);
        *dst_data++ = uint8_t(run + (qoi_op_run - 1u));
        n -= run;
      } while (n);

      if (!x) {
        return dst_data;
      }
    }

    uint32_t hash = qoi_hash_pixel_a8(p);

    if (pixel_table[hash] == p) {
      *dst_data++ = uint8_t(qoi_op_index | hash);
    }
    else {
      pixel_table[hash] = uint16_t(p);

      dst_data[0] = qoi_op_rgba;
      dst_data[1] = uint8_t(0xFFu);
      dst_data[2] = uint8_t(0xFFu);
      dst_data[3] = uint8_t(0xFFu);
      dst_data[4] = uint8_t(p);
      dst_data += 5;
    }

    pixel = p;

    if (--x != 0u)
      continue;

    if (--h == 0u)
      return dst_data;

    src_data += src_stride;
    x = w;
  }
}

static uint8_t* qoi_encode_data_xrgb32(uint8_t* dst_data, uint32_t w, uint32_t h, const uint8_t* src_data, intptr_t src_stride) noexcept {
  BLRgba32 pixel = BLRgba32(0xFF000000u);
  uint32_t pixel_table[64] {};

  uint32_t x = w;
  src_stride -= intptr_t(w) * 4;

  for (;;) {
    BLRgba32 p = BLRgba32(axl::loada_u32(src_data) | 0xFF000000u);
    src_data += 4;

    // Run length encoding.
    if (p == pixel) {
      size_t n = 1;
      x--;

      for (;;) {
        uint32_t prev_x = x;

        while (x) {
          p = BLRgba32(axl::loada_u32(src_data) | 0xFF000000u);
          src_data += 4;
          if (p != pixel)
            break;
          x--;
        }

        n += size_t(prev_x - x);

        if (x == 0 && --h != 0) {
          src_data += src_stride;
          x = w;
        }
        else {
          break;
        }
      }

      do {
        size_t run = bl_min<size_t>(n, 62u);
        *dst_data++ = uint8_t(run + (qoi_op_run - 1u));
        n -= run;
      } while (n);

      if (!x) {
        return dst_data;
      }
    }

    uint32_t hash = qoi_hash_pixel_rgba32(p.value);

    if (pixel_table[hash] == p.value) {
      *dst_data++ = uint8_t(qoi_op_index | hash);
    }
    else {
      pixel_table[hash] = p.value;

      uint32_t dr = uint32_t(p.r()) - uint32_t(pixel.r());
      uint32_t dg = uint32_t(p.g()) - uint32_t(pixel.g());
      uint32_t db = uint32_t(p.b()) - uint32_t(pixel.b());

      uint32_t xr = uint32_t(dr + 2u) & 0xFFu;
      uint32_t xg = uint32_t(dg + 2u) & 0xFFu;
      uint32_t xb = uint32_t(db + 2u) & 0xFFu;

      if ((xr | xg | xb) <= 0x3u) {
        *dst_data++ = uint8_t(qoi_op_diff | (xr << 4) | (xg << 2) | xb);
      }
      else {
        uint32_t dg_r = dr - dg;
        uint32_t dg_b = db - dg;

        xr = (dg_r + 8u) & 0xFFu;
        xg = (dg  + 32u) & 0xFFu;
        xb = (dg_b + 8u) & 0xFFu;

        if ((xr | xb) <= 0xFu && xg <= 0x3F) {
          dst_data[0] = uint8_t(qoi_op_luma | xg);
          dst_data[1] = uint8_t((xr << 4) | xb);
          dst_data += 2;
        }
        else {
          dst_data[0] = qoi_op_rgb;
          dst_data[1] = uint8_t(p.r());
          dst_data[2] = uint8_t(p.g());
          dst_data[3] = uint8_t(p.b());
          dst_data += 4;
        }
      }
    }

    pixel = p;

    if (--x != 0u)
      continue;

    if (--h == 0u)
      return dst_data;

    src_data += src_stride;
    x = w;
  }
}

static uint8_t* qoi_encode_data_prgb32(uint8_t* dst_data, uint32_t w, uint32_t h, const uint8_t* src_data, intptr_t src_stride) noexcept {
  BLRgba32 pixel_pm = BLRgba32(0xFF000000u);
  BLRgba32 pixel_np = BLRgba32(0xFF000000u);
  uint32_t pixel_table[64] {};

  uint32_t x = w;
  src_stride -= intptr_t(w) * 4;

  for (;;) {
    BLRgba32 pm = BLRgba32(axl::loada_u32(src_data));
    src_data += 4;

    // Run length encoding.
    if (pm == pixel_pm) {
      size_t n = 1;
      x--;

      for (;;) {
        uint32_t prev_x = x;

        while (x) {
          pm = BLRgba32(axl::loada_u32(src_data));
          src_data += 4;
          if (pm != pixel_pm)
            break;
          x--;
        }

        n += size_t(prev_x - x);

        if (x == 0 && --h != 0) {
          src_data += src_stride;
          x = w;
        }
        else {
          break;
        }
      }

      do {
        size_t run = bl_min<size_t>(n, 62u);
        *dst_data++ = uint8_t(run + (qoi_op_run - 1u));
        n -= run;
      } while (n);

      if (!x) {
        return dst_data;
      }
    }

    BLRgba32 np = BLRgba32(PixelOps::Scalar::cvt_argb32_8888_from_prgb32_8888(pm.value));
    uint32_t hash = qoi_hash_pixel_rgba32(np.value);

    if (pixel_table[hash] == np.value) {
      *dst_data++ = uint8_t(qoi_op_index | hash);
    }
    else {
      pixel_table[hash] = np.value;

      // To use delta, the previous pixel needs to have the same alpha value unfortunately.
      if (pixel_np.a() == np.a()) {
        uint32_t dr = uint32_t(np.r()) - uint32_t(pixel_np.r());
        uint32_t dg = uint32_t(np.g()) - uint32_t(pixel_np.g());
        uint32_t db = uint32_t(np.b()) - uint32_t(pixel_np.b());

        uint32_t xr = (dr + 2u) & 0xFFu;
        uint32_t xg = (dg + 2u) & 0xFFu;
        uint32_t xb = (db + 2u) & 0xFFu;

        if ((xr | xg | xb) <= 0x3u) {
          *dst_data++ = uint8_t(qoi_op_diff | (xr << 4) | (xg << 2) | xb);
        }
        else {
          uint32_t dg_r = dr - dg;
          uint32_t dg_b = db - dg;

          xr = (dg_r + 8) & 0xFFu;
          xg = (dg  + 32) & 0xFFu;
          xb = (dg_b + 8) & 0xFFu;

          if ((xr | xb) <= 0xFu && xg <= 0x3Fu) {
            dst_data[0] = uint8_t(qoi_op_luma | xg);
            dst_data[1] = uint8_t((xr << 4) | xb);
            dst_data += 2;
          }
          else {
            dst_data[0] = qoi_op_rgb;
            dst_data[1] = uint8_t(np.r());
            dst_data[2] = uint8_t(np.g());
            dst_data[3] = uint8_t(np.b());
            dst_data += 4;
          }
        }
      }
      else {
        dst_data[0] = qoi_op_rgba;
        dst_data[1] = uint8_t(np.r());
        dst_data[2] = uint8_t(np.g());
        dst_data[3] = uint8_t(np.b());
        dst_data[4] = uint8_t(np.a());
        dst_data += 5;
      }
    }

    pixel_pm = pm;
    pixel_np = np;

    if (--x != 0u)
      continue;

    if (--h == 0u)
      return dst_data;

    src_data += src_stride;
    x = w;
  }
}

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

  if (img.is_empty())
    return bl_make_error(BL_ERROR_INVALID_VALUE);

  BLImageData image_data;
  BL_PROPAGATE(img.get_data(&image_data));

  uint32_t w = uint32_t(image_data.size.w);
  uint32_t h = uint32_t(image_data.size.h);
  uint32_t format = image_data.format;

  uint32_t channels = format == BL_FORMAT_XRGB32 ? 3u : 4u;
  uint32_t max_bytes_per_encoded_pixel = channels + 1u;

  // NOTE: This should never overflow.
  uint64_t max_size = uint64_t(w) * uint64_t(h) * uint64_t(max_bytes_per_encoded_pixel) + qoi_header_size + qoi_end_marker_size;

  if (BL_UNLIKELY(max_size >= uint64_t(SIZE_MAX)))
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);

  uint8_t* dst_data;
  BL_PROPAGATE(buf.append_uninitialized(size_t(max_size), &dst_data));

  uint8_t* dst_ptr = dst_data;
  memcpy(dst_ptr, qoi_magic, qoi_magic_size);
  axl::storeu_u32_be(dst_ptr + 4, w);
  axl::storeu_u32_be(dst_ptr + 8, h);
  dst_ptr[12] = uint8_t(channels);
  dst_ptr[13] = 0;
  dst_ptr += 14;

  const uint8_t* src_line = static_cast<const uint8_t*>(image_data.pixel_data);

  switch (format) {
    case BL_FORMAT_A8    : dst_ptr = qoi_encode_data_a8(dst_ptr, w, h, src_line, image_data.stride); break;
    case BL_FORMAT_XRGB32: dst_ptr = qoi_encode_data_xrgb32(dst_ptr, w, h, src_line, image_data.stride); break;
    case BL_FORMAT_PRGB32: dst_ptr = qoi_encode_data_prgb32(dst_ptr, w, h, src_line, image_data.stride); break;

    default:
      buf._set_size_unsafe(0);
      return bl_make_error(BL_ERROR_INVALID_STATE);
  }

  memcpy(dst_ptr, qoi_end_marker, qoi_end_marker_size);
  dst_ptr += qoi_end_marker_size;

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
  return ObjectLayer::dealloc_impl(impl, {self->_d.meta_byte()});
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

  if (size == 0)
    return 0;

  size_t magic_size = bl_min<size_t>(size, qoi_magic_size);
  if (memcmp(qoi_magic, data, magic_size) != 0)
    return 0;

  if (size < 12)
    return uint32_t(magic_size);

  uint32_t w = bl::axl::loadu_u32_be(data + 4);
  uint32_t h = bl::axl::loadu_u32_be(data + 8);

  if (w == 0 || h == 0)
    return 0;

  if (size < 14)
    return uint32_t(magic_size + 1u);

  uint8_t channels = data[12];
  uint8_t colorspace = data[13];

  if (channels != 3u && channels != 4u)
    return 0;

  if (colorspace > 1u)
    return 0;

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
  using namespace bl::Qoi;

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
