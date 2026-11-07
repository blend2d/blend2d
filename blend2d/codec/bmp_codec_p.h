// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CODEC_BMP_CODEC_P_H_INCLUDED
#define BLEND2D_CODEC_BMP_CODEC_P_H_INCLUDED

#include <blend2d/axl/commons.h>
#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/image_p.h>
#include <blend2d/core/image_codec.h>
#include <blend2d/core/image_decoder.h>
#include <blend2d/core/image_encoder.h>
#include <blend2d/core/pixel_converter.h>
#include <blend2d/core/runtime_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_codec_impl
//! \{

namespace bl::Bmp {

static constexpr uint32_t bmp_header_size_os2_v1 = 12;
static constexpr uint32_t bmp_header_size_win_v1 = 40;
static constexpr uint32_t bmp_header_size_win_v2 = 52;
static constexpr uint32_t bmp_header_size_win_v3 = 56;
static constexpr uint32_t bmp_header_size_win_v4 = 108;
static constexpr uint32_t bmp_header_size_win_v5 = 12;

static constexpr uint32_t bmp_compression_rgb = 0;
static constexpr uint32_t bmp_compression_rle8 = 1;
static constexpr uint32_t bmp_compression_rle4 = 2;
static constexpr uint32_t bmp_compression_bit_fields = 3;
static constexpr uint32_t bmp_compression_jpeg = 4;
static constexpr uint32_t bmp_compression_png = 5;
static constexpr uint32_t bmp_compression_alpha_bit_fields = 6;
static constexpr uint32_t bmp_compression_cmyk = 11;
static constexpr uint32_t bmp_compression_cmyk_rle8 = 12;
static constexpr uint32_t bmp_compression_cmyk_rle4 = 1;
static constexpr uint32_t bmp_compression_count = 13;

static constexpr uint32_t bmp_color_space_calibrated_rgb = 0;
static constexpr uint32_t bmp_color_space_dd_rgb = 1;
static constexpr uint32_t bmp_color_space_dd_cmyk = 2;

static constexpr uint32_t bmp_rle_cmd_line = 0;
static constexpr uint32_t bmp_rle_cmd_stop = 1;
static constexpr uint32_t bmp_rle_cmd_move = 2;
static constexpr uint32_t bmp_rle_cmd_count = 3;

// Specification describes that skipped pixels contain background color, transparent for us.
static constexpr uint32_t bmp_rle_background = 0u;

//! Bitmap file signature [2 Bytes].
struct BmpFileSignature {
  //! Bitmap signature data - "BM".
  uint8_t data[2];
};

//! Bitmap File Header [12 Bytes] (we don't count signature here, it's separate).
struct BmpFileHeader {
  //! Bitmap file size in bytes.
  uint32_t file_size;
  //! Reserved, should be zero.
  uint32_t reserved;
  //! Offset to image data (54, 124, ...).
  uint32_t image_offset;

  BL_INLINE void reset() noexcept { *this = BmpFileHeader{}; }

  BL_INLINE void byte_swap() {
    file_size    = axl::byteswap(file_size);
    image_offset = axl::byteswap(image_offset);
  }
};

//! All bitmap headers in one union.
struct BmpInfoHeader {
  //! Helper that contains XYZ (colorspace).
  struct XYZ {
    uint32_t x, y, z;
  };

  //! Bitmap OS/2 Header [12 Bytes].
  struct OS2 {
    //! Header size (40, 52).
    uint32_t header_size;
    //! Bitmap width (16-bit value).
    int16_t width;
    //! Bitmap height (16-bit value).
    int16_t height;
    //! Number of color planes (always 1).
    uint16_t planes;
    //! Bits per pixel (1, 4, 8 or 24).
    uint16_t bits_per_pixel;

    BL_INLINE void byte_swap() noexcept {
      header_size    = axl::byteswap(header_size);
      width          = axl::byteswap(width);
      height         = axl::byteswap(height);
      planes         = axl::byteswap(planes);
      bits_per_pixel = axl::byteswap(bits_per_pixel);
    }
  };

  //! Windows Info Header [40..124 Bytes].
  struct Win {
    // Version 1.

    //! Header size (40, 52, 56, 108, 124).
    uint32_t header_size;
    //! Bitmap width.
    int32_t width;
    //! Bitmap height.
    int32_t height;
    //! Count of planes, always 1.
    uint16_t planes;
    //! Bits per pixel (1, 4, 8, 16, 24 or 32).
    uint16_t bits_per_pixel;
    //! Compression methods used.
    uint32_t compression;
    //! Image data size (in bytes).
    uint32_t image_size;
    //! Horizontal resolution in pixels per meter.
    uint32_t horz_resolution;
    //! Vertical resolution in pixels per meter.
    uint32_t vert_resolution;
    //! Number of colors in the image.
    uint32_t colors_used;
    //! Minimum number of important colors.
    uint32_t colors_important;

    // Version 2 and 3.

    union {
      uint32_t masks[4];
      struct {
        //! Mask identifying bits of red component.
        uint32_t r_mask;
        //! Mask identifying bits of green component.
        uint32_t g_mask;
        //! Mask identifying bits of blue component.
        uint32_t b_mask;
        //! Mask identifying bits of alpha component [Version 3+ only].
        uint32_t a_mask;
      };
    };

    // Version 4.

    //! Color space type.
    uint32_t colorspace;
    //! Coordinates of red endpoint.
    XYZ r;
    //! Coordinates of green endpoint.
    XYZ g;
    //! Coordinates of blue endpoint.
    XYZ b;
    //! Gamma red coordinate scale value.
    uint32_t r_gamma;
    //! Gamma green coordinate scale value.
    uint32_t gGamma;
    //! Gamma blue coordinate scale value.
    uint32_t b_gamma;

    // Version 5.

    //! Rendering intent for bitmap.
    uint32_t intent;
    //! ProfileData offset (in bytes), from the beginning of `BmpInfoHeader::Win`.
    uint32_t profile_data;
    //! Size, in bytes, of embedded profile data.
    uint32_t profile_size;
    //! Reserved, should be zero.
    uint32_t reserved;

    BL_INLINE void byte_swap() noexcept {
      header_size      = axl::byteswap(header_size);
      width            = axl::byteswap(width);
      height           = axl::byteswap(height);
      planes           = axl::byteswap(planes);
      bits_per_pixel   = axl::byteswap(bits_per_pixel);
      compression      = axl::byteswap(compression);
      image_size       = axl::byteswap(image_size);
      horz_resolution  = axl::byteswap(horz_resolution);
      vert_resolution  = axl::byteswap(vert_resolution);
      colors_used      = axl::byteswap(colors_used);
      colors_important = axl::byteswap(colors_important);
      r_mask           = axl::byteswap(r_mask);
      g_mask           = axl::byteswap(g_mask);
      b_mask           = axl::byteswap(b_mask);
      a_mask           = axl::byteswap(a_mask);
      colorspace       = axl::byteswap(colorspace);
      r.x              = axl::byteswap(r.x);
      r.y              = axl::byteswap(r.y);
      r.z              = axl::byteswap(r.z);
      g.x              = axl::byteswap(g.x);
      g.y              = axl::byteswap(g.y);
      g.z              = axl::byteswap(g.z);
      b.x              = axl::byteswap(b.x);
      b.y              = axl::byteswap(b.y);
      b.z              = axl::byteswap(b.z);
      r_gamma          = axl::byteswap(r_gamma);
      gGamma           = axl::byteswap(gGamma);
      b_gamma          = axl::byteswap(b_gamma);
      intent           = axl::byteswap(intent);
      profile_data     = axl::byteswap(profile_data);
      profile_size     = axl::byteswap(profile_size);
    }
  };

  union {
    uint32_t header_size;
    OS2 os2;
    Win win;
  };

  BL_INLINE void reset() noexcept { *this = BmpInfoHeader{}; }
};

BL_HIDDEN void bmp_codec_on_init(BLRuntimeContext* rt, BLArray<BLImageCodec>* codecs) noexcept;

struct BmpCodecImpl : public BLImageCodecImpl {};

struct BmpDecoderImpl : public BLImageDecoderImpl {
  BLImageInfo image_info;
  BmpFileHeader file;
  BmpInfoHeader info;
  BLFormatInfo fmt;
  uint32_t stride;
};

struct BmpEncoderImpl : public BLImageEncoderImpl {};

} // {bl::Bmp}

//! \}
//! \endcond

#endif // BLEND2D_CODEC_BMP_CODEC_P_H_INCLUDED
