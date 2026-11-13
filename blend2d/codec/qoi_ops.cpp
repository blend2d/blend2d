// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#if !defined(BL_BUILD_NO_CODECS)

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_ops.h>
#include <blend2d/codec/qoi_codec_p.h>
#include <blend2d/codec/qoi_ops_p.h>
#include <blend2d/pixelops/scalar_p.h>

namespace bl::Qoi {

// Codecs - Qoi - Decoder - Constants
// ==================================

// Lookup table generator that generates delta values for QOI_OP_DIFF and the first byte of QOI_OP_LUMA.
// Additionally, it provides values for a RLE run of a single pixel for a possible experimentation.
struct QoiIndexDiffLumaTableGen {
  static consteval uint32_t rgb(uint32_t r, uint32_t g, uint32_t b, uint32_t luma_mask) noexcept {
    return ((r & 0xFFu) << 24) |
           ((g & 0xFFu) << 16) |
           ((b & 0xFFu) <<  8) |
           ((luma_mask) <<  0) ;
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

// Codecs - Qoi - Decoder - Ops
// ============================

#if BL_TARGET_ARCH_BITS >= 64
struct UnpackedPixel {
  uint64_t ag_rb; // Represents 0x00AA00GG00RR00BB.

  static BL_INLINE UnpackedPixel unpack(uint32_t packed) noexcept {
    return UnpackedPixel{((uint64_t(packed) << 24) | packed) & 0x00FF00FF00FF00FFu};
  }

  static BL_INLINE UnpackedPixel unpack_rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a) noexcept {
    return UnpackedPixel{(uint64_t(a) << 48) | (uint64_t(g) << 32) | (uint64_t(r) << 16) | (uint64_t(b) << 0)};
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

  BL_INLINE void add(const UnpackedPixel& other) noexcept { ag_rb += other.ag_rb; }
  BL_INLINE void add_rb(uint32_t value) noexcept { ag_rb += value; }
  BL_INLINE void mask() noexcept { ag_rb &= 0x00FF00FF00FF00FFu; }

  BL_INLINE void op_rgbx(uint32_t hbyte0, const UnpackedPixel& other) noexcept {
    uint64_t msk = uint64_t(hbyte0 + 1) << 48;
    ag_rb = (ag_rb & msk) | (other.ag_rb & ~msk);
  }
};
#else
struct UnpackedPixel {
  uint32_t ag; // Represents 0xAA00GG00.
  uint32_t rb; // Represents 0x00RR00BB.

  static BL_INLINE UnpackedPixel unpack(uint32_t packed) noexcept {
    return UnpackedPixel{packed & 0xFF00FF00u, packed & 0x00FF00FFu };
  }

  static BL_INLINE UnpackedPixel unpack_rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a) noexcept {
    return UnpackedPixel{
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

  BL_INLINE void add(const UnpackedPixel& other) noexcept { ag += other.ag; rb += other.rb; }
  BL_INLINE void add_rb(uint32_t value) noexcept { rb += value; }
  BL_INLINE void mask() noexcept { ag &= 0xFF00FF00u; rb &= 0x00FF00FFu; }

  BL_INLINE void op_rgbx(uint32_t hbyte0, const UnpackedPixel& other) noexcept {
    uint32_t msk = uint32_t(hbyte0 + 1) << 24;
    ag = (ag & msk) | (other.ag & ~msk);
    rb = other.rb;
  }
};
#endif
static BL_INLINE uint32_t* qoi_fill_rgba32(uint32_t* dst, uint32_t value, size_t count) noexcept {
  for (size_t i = 0; i < count; i++) {
    dst[i] = value;
  }
  return dst + count;
}

template<bool kHasAlpha>
static BL_INLINE BLResult qoi_decode_data_t(
  uint8_t* dst_row,
  intptr_t dst_stride,
  uint32_t w,
  uint32_t h,
  uint32_t packed_table[64],
  UnpackedPixel unpacked_table[64],
  const uint8_t* src,
  const uint8_t* end
) noexcept {

  constexpr size_t kMinRemainingBytesOfNextChunk = kQoiEndMarkerSize + 1u;

  uint32_t* dst_ptr = reinterpret_cast<uint32_t*>(dst_row);
  uint32_t* dst_end = dst_ptr + w;

  uint32_t packed_pixel = 0xFF000000;
  UnpackedPixel unpacked_pixel = UnpackedPixel::unpack(packed_pixel);

  // Edge case: If the image starts with QOI_OP_RUN, the repeated pixel must be
  // added to the pixel table, otherwise the decoder may produce incorrect result.
  {
    uint32_t hbyte0 = src[0];

    if (hbyte0 >= kQoiOpRun && hbyte0 < kQoiOpRun + 62u) {
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

    if (hbyte0 < kQoiOpRun) {
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
          unpacked_pixel.add(UnpackedPixel::unpack(packed_delta));
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

      if (hbyte0 >= kQoiOpRGB) {
        // Handle both QOI_OP_RGB and QOI_OP_RGBA at the same time.
        unpacked_pixel.op_rgbx(hbyte0, UnpackedPixel::unpack_rgba(hbyte1, src[1], src[2], src[3]));

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

BLResult qoi_decode_data(uint8_t* dst_row, intptr_t dst_stride, uint32_t w, uint32_t h, const uint8_t* src, const uint8_t* end, bool has_alpha) noexcept {
  uint32_t packed_table[64];
  qoi_fill_rgba32(packed_table, has_alpha ? 0u : 0xFF000000u, 64);
  UnpackedPixel unpacked_table[64] {};

  if (has_alpha) {
    return qoi_decode_data_t<true>(dst_row, dst_stride, w, h, packed_table, unpacked_table, src, end);
  }
  else {
    return qoi_decode_data_t<false>(dst_row, dst_stride, w, h, packed_table, unpacked_table, src, end);
  }
}

// Codecs - Qoi - Encoder - Ops
// ============================

// QOI isn't good for compressing alpha-only images - we can optimize the encoder's performance, but not the final size.
uint8_t* qoi_encode_data_a8(uint8_t* BL_RESTRICT dst_data, uint32_t w, uint32_t h, const uint8_t* BL_RESTRICT src_data, intptr_t src_stride) noexcept {
  // NOTE: Use an initial value which is not representable, because the encoder/decoder starts with RGB==0,
  // which would decode badly into RGBA formats (the components would be zero and thus it would not be the
  // same as when used by Blend2D, which defaults to having RGB components the same as 0xFF premultiplied).
  uint32_t pixel = 0xFFFFFFFFu;
  uint16_t buckets[64];

  for (size_t i = 0; i < 64; i++) {
    buckets[i] = 0xFFFFu;
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
        *dst_data++ = uint8_t(run + (kQoiOpRun - 1u));
        n -= run;
      } while (n);

      if (!x) {
        return dst_data;
      }
    }

    uint32_t hash = qoi_hash_pixel_a8(p);

    if (buckets[hash] == p) {
      *dst_data++ = uint8_t(kQoiOpIndex | hash);
    }
    else {
      buckets[hash] = uint16_t(p);

      dst_data[0] = kQoiOpRGBA;
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

uint8_t* qoi_encode_data_xrgb32(uint8_t* BL_RESTRICT dst_data, uint32_t w, uint32_t h, const uint8_t* BL_RESTRICT src_data, intptr_t src_stride) noexcept {
  BLRgba32 pixel = BLRgba32(0xFF000000u);
  uint32_t buckets[64] {};

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
        *dst_data++ = uint8_t(run + (kQoiOpRun - 1u));
        n -= run;
      } while (n);

      if (!x) {
        return dst_data;
      }
    }

    uint32_t hash = qoi_hash_pixel_rgba32(p.value);

    if (buckets[hash] == p.value) {
      *dst_data++ = uint8_t(kQoiOpIndex | hash);
    }
    else {
      buckets[hash] = p.value;

      uint32_t dr = uint32_t(p.r()) - uint32_t(pixel.r());
      uint32_t dg = uint32_t(p.g()) - uint32_t(pixel.g());
      uint32_t db = uint32_t(p.b()) - uint32_t(pixel.b());

      uint32_t xr = uint32_t(dr + 2u) & 0xFFu;
      uint32_t xg = uint32_t(dg + 2u) & 0xFFu;
      uint32_t xb = uint32_t(db + 2u) & 0xFFu;

      if ((xr | xg | xb) <= 0x3u) {
        *dst_data++ = uint8_t(kQoiOpDiff | (xr << 4) | (xg << 2) | xb);
      }
      else {
        uint32_t dg_r = dr - dg;
        uint32_t dg_b = db - dg;

        xr = (dg_r + 8u) & 0xFFu;
        xg = (dg  + 32u) & 0xFFu;
        xb = (dg_b + 8u) & 0xFFu;

        if ((xr | xb) <= 0xFu && xg <= 0x3F) {
          dst_data[0] = uint8_t(kQoiOpLuma | xg);
          dst_data[1] = uint8_t((xr << 4) | xb);
          dst_data += 2;
        }
        else {
          dst_data[0] = kQoiOpRGB;
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

uint8_t* qoi_encode_data_prgb32(uint8_t* BL_RESTRICT dst_data, uint32_t w, uint32_t h, const uint8_t* BL_RESTRICT src_data, intptr_t src_stride) noexcept {
  BLRgba32 pixel_pm = BLRgba32(0xFF000000u);
  BLRgba32 pixel_np = BLRgba32(0xFF000000u);
  uint32_t buckets[64] {};

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
        *dst_data++ = uint8_t(run + (kQoiOpRun - 1u));
        n -= run;
      } while (n);

      if (!x) {
        return dst_data;
      }
    }

    BLRgba32 np = BLRgba32(PixelOps::Scalar::cvt_argb32_8888_from_prgb32_8888(pm.value));
    uint32_t hash = qoi_hash_pixel_rgba32(np.value);

    if (buckets[hash] == np.value) {
      *dst_data++ = uint8_t(kQoiOpIndex | hash);
    }
    else {
      buckets[hash] = np.value;

      // To use delta, the previous pixel needs to have the same alpha value unfortunately.
      if (pixel_np.a() == np.a()) {
        uint32_t dr = uint32_t(np.r()) - uint32_t(pixel_np.r());
        uint32_t dg = uint32_t(np.g()) - uint32_t(pixel_np.g());
        uint32_t db = uint32_t(np.b()) - uint32_t(pixel_np.b());

        uint32_t xr = (dr + 2u) & 0xFFu;
        uint32_t xg = (dg + 2u) & 0xFFu;
        uint32_t xb = (db + 2u) & 0xFFu;

        if ((xr | xg | xb) <= 0x3u) {
          *dst_data++ = uint8_t(kQoiOpDiff | (xr << 4) | (xg << 2) | xb);
        }
        else {
          uint32_t dg_r = dr - dg;
          uint32_t dg_b = db - dg;

          xr = (dg_r + 8) & 0xFFu;
          xg = (dg  + 32) & 0xFFu;
          xb = (dg_b + 8) & 0xFFu;

          if ((xr | xb) <= 0xFu && xg <= 0x3Fu) {
            dst_data[0] = uint8_t(kQoiOpLuma | xg);
            dst_data[1] = uint8_t((xr << 4) | xb);
            dst_data += 2;
          }
          else {
            dst_data[0] = kQoiOpRGB;
            dst_data[1] = uint8_t(np.r());
            dst_data[2] = uint8_t(np.g());
            dst_data[3] = uint8_t(np.b());
            dst_data += 4;
          }
        }
      }
      else {
        dst_data[0] = kQoiOpRGBA;
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

} // {bl::Qoi}

#endif // !BL_BUILD_NO_CODECS
