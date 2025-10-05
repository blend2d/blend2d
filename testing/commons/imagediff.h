// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// This file provides utility classes and functions shared between some tests.

#ifndef TESTING_IMAGEDIFF_H_INCLUDED
#define TESTING_IMAGEDIFF_H_INCLUDED

#include <blend2d.h>
#include <stdint.h>

namespace ImageUtils {

struct DiffInfo {
  uint32_t max_diff;
  uint64_t cumulative_diff;
};

[[maybe_unused]]
static DiffInfo diff_info(const BLImage& a_image, const BLImage& b_image) noexcept {
  DiffInfo info {};
  BLImageData a_data;
  BLImageData b_data;

  // Used in case of error (image size/format doesn't match).
  info.max_diff = 0xFFFFFFFFu;

  if (a_image.size() != b_image.size())
    return info;

  size_t w = size_t(a_image.width());
  size_t h = size_t(a_image.height());

  if (a_image.get_data(&a_data) != BL_SUCCESS)
    return info;

  if (b_image.get_data(&b_data) != BL_SUCCESS)
    return info;

  if (a_data.format != b_data.format) {
    if ((a_data.format == BL_FORMAT_XRGB32 && b_data.format == BL_FORMAT_PRGB32) ||
        (a_data.format == BL_FORMAT_PRGB32 && b_data.format == BL_FORMAT_XRGB32)) {
      // Pass: We would convert between these two formats on the fly.
    }
    else {
      return info;
    }
  }

  intptr_t a_stride = a_data.stride;
  intptr_t b_stride = b_data.stride;

  const uint8_t* a_line = static_cast<const uint8_t*>(a_data.pixel_data);
  const uint8_t* b_line = static_cast<const uint8_t*>(b_data.pixel_data);

  info.max_diff = 0;

  switch (a_data.format) {
    case BL_FORMAT_XRGB32:
    case BL_FORMAT_PRGB32: {
      uint32_t a_mask = a_data.format == BL_FORMAT_XRGB32 ? 0xFF000000u : 0x0u;
      uint32_t b_mask = b_data.format == BL_FORMAT_XRGB32 ? 0xFF000000u : 0x0u;

      for (size_t y = 0; y < h; y++) {
        const uint32_t* a_ptr = reinterpret_cast<const uint32_t*>(a_line);
        const uint32_t* b_ptr = reinterpret_cast<const uint32_t*>(b_line);

        for (size_t x = 0; x < w; x++) {
          uint32_t a_val = a_ptr[x] | a_mask;
          uint32_t b_val = b_ptr[x] | b_mask;

          if (a_val != b_val) {
            uint32_t a_diff = uint32_t(bl_abs(int((a_val >> 24) & 0xFF) - int((b_val >> 24) & 0xFF)));
            uint32_t r_diff = uint32_t(bl_abs(int((a_val >> 16) & 0xFF) - int((b_val >> 16) & 0xFF)));
            uint32_t gDiff = uint32_t(bl_abs(int((a_val >>  8) & 0xFF) - int((b_val >>  8) & 0xFF)));
            uint32_t b_diff = uint32_t(bl_abs(int((a_val      ) & 0xFF) - int((b_val      ) & 0xFF)));
            uint32_t max_diff = bl_max(a_diff, r_diff, gDiff, b_diff);

            info.max_diff = bl_max(info.max_diff, max_diff);
            info.cumulative_diff += max_diff;
          }
        }

        a_line += a_stride;
        b_line += b_stride;
      }
      break;
    }

    case BL_FORMAT_A8: {
      for (size_t y = 0; y < h; y++) {
        const uint8_t* a_ptr = a_line;
        const uint8_t* b_ptr = b_line;

        for (size_t x = 0; x < w; x++) {
          uint32_t a_val = a_ptr[x];
          uint32_t b_val = b_ptr[x];
          uint32_t diff = uint32_t(bl_abs(int(a_val) - int(b_val)));

          info.max_diff = bl_max(info.max_diff, diff);
          info.cumulative_diff += diff;
        }

        a_line += a_stride;
        b_line += b_stride;
      }
      break;
    }

    default: {
      info.max_diff = 0xFFFFFFFFu;
      break;
    }
  }

  return info;
}

[[maybe_unused]]
static BLImage diff_image(const BLImage& a_image, const BLImage& b_image) noexcept {
  BLImage result;
  BLImageData r_data;
  BLImageData a_data;
  BLImageData b_data;

  if (a_image.size() != b_image.size())
    return result;

  size_t w = size_t(a_image.width());
  size_t h = size_t(a_image.height());

  if (a_image.get_data(&a_data) != BL_SUCCESS)
    return result;

  if (b_image.get_data(&b_data) != BL_SUCCESS)
    return result;

  if (a_data.format != b_data.format)
    return result;

  if (result.create(int(w), int(h), BL_FORMAT_XRGB32) != BL_SUCCESS)
    return result;

  if (result.get_data(&r_data) != BL_SUCCESS)
    return result;

  intptr_t d_stride = r_data.stride;
  intptr_t a_stride = a_data.stride;
  intptr_t b_stride = b_data.stride;

  uint8_t* d_line = static_cast<uint8_t*>(r_data.pixel_data);
  const uint8_t* a_line = static_cast<const uint8_t*>(a_data.pixel_data);
  const uint8_t* b_line = static_cast<const uint8_t*>(b_data.pixel_data);

  auto&& color_from_diff = [](uint32_t diff) noexcept -> uint32_t {
    static constexpr uint32_t low_diff[] = {
      0xFF000000u,
      0xFF0000A0u,
      0xFF0000C0u,
      0xFF0000FFu,
      0xFF0040A0u
    };

    if (diff <= 4u)
      return low_diff[diff];
    else if (diff <= 16u)
      return 0xFF000000u + unsigned((diff * 16u - 1u) << 8);
    else
      return 0xFF000000u + unsigned((127u + diff / 2u) << 16);
  };

  switch (a_data.format) {
    case BL_FORMAT_PRGB32:
    case BL_FORMAT_XRGB32: {
      for (size_t y = 0; y < h; y++) {
        uint32_t* d_ptr = reinterpret_cast<uint32_t*>(d_line);
        const uint32_t* a_ptr = reinterpret_cast<const uint32_t*>(a_line);
        const uint32_t* b_ptr = reinterpret_cast<const uint32_t*>(b_line);

        for (size_t x = 0; x < w; x++) {
          uint32_t a_val = a_ptr[x];
          uint32_t b_val = b_ptr[x];
          uint32_t diff = uint32_t(bl_abs(int(a_val) - int(b_val)));

          uint32_t color = color_from_diff(diff);
          d_ptr[x] = color;
        }

        d_line += d_stride;
        a_line += a_stride;
        b_line += b_stride;
      }
      break;
    }

    case BL_FORMAT_A8: {
      for (size_t y = 0; y < h; y++) {
        uint32_t* d_ptr = reinterpret_cast<uint32_t*>(d_line);
        const uint8_t* a_ptr = a_line;
        const uint8_t* b_ptr = b_line;

        for (size_t x = 0; x < w; x++) {
          uint32_t a_val = a_ptr[x];
          uint32_t b_val = b_ptr[x];
          int a_diff = bl_abs(int((a_val >> 24) & 0xFF) - int((b_val >> 24) & 0xFF));
          int r_diff = bl_abs(int((a_val >> 16) & 0xFF) - int((b_val >> 16) & 0xFF));
          int gDiff = bl_abs(int((a_val >>  8) & 0xFF) - int((b_val >>  8) & 0xFF));
          int b_diff = bl_abs(int((a_val      ) & 0xFF) - int((b_val      ) & 0xFF));

          uint32_t color = color_from_diff(uint32_t(bl_max(a_diff, r_diff, gDiff, b_diff)));
          d_ptr[x] = color;
        }

        d_line += d_stride;
        a_line += a_stride;
        b_line += b_stride;
      }
      break;
    }

    default: {
      result.reset();
      break;
    }
  }

  return result;
}

} // {ImageUtils}

#endif // TESTING_IMAGEDIFF_H_INCLUDED
