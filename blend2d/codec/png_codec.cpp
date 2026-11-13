// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#if !defined(BL_BUILD_NO_CODECS)

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_ops.h>
#include <blend2d/axl/scopedbuffer_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/format.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/core/var_p.h>
#include <blend2d/codec/png_codec_p.h>
#include <blend2d/codec/png_ops_p.h>
#include <blend2d/compression/checksum_p.h>
#include <blend2d/compression/deflate_decoder_p.h>
#include <blend2d/compression/deflate_encoder_p.h>
#include <blend2d/pixelops/scalar_p.h>

namespace bl::Png {

// Codecs - Png - Globals
// ======================

static constinit ObjectLayer::EternalVirtImpl<PngCodecImpl, BLImageCodecVirt> png_codec{};

static BLImageCodecCore png_codec_object;
static BLImageDecoderVirt png_decoder_virt;
static BLImageEncoderVirt png_encoder_virt;

// Codecs - Png - Constants
// ========================

// PNG file signature (8 bytes).
static constexpr uint8_t png_signature_bytes[8] = { 0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au };
// Allowed bits-per-sample per "ColorType"
static constexpr uint8_t png_color_type_bit_depth_table[7] = { 0x1Fu, 0u, 0x18u, 0x0Fu, 0x18u, 0u, 0x18u };
// Count of samples per "ColorType".
static constexpr uint8_t png_color_type_to_sample_count_table[7] = { 1u, 0u, 3u, 1u, 2u, 0u, 4u };

static constexpr uint32_t kPngSignatureSize = 8u;
static constexpr uint32_t kPngChunkHeaderSize = 8u;
static constexpr uint32_t kPngChunkCRCSize = 4u;
static constexpr uint32_t kPngChunkBaseSize = 12u;
static constexpr uint32_t kPngChunkDataSize_CgBI = 4u;
static constexpr uint32_t kPngChunkDataSize_IHDR = 13u;
static constexpr uint32_t kPngChunkDataSize_acTL = 8u;
static constexpr uint32_t kPngChunkDataSize_fcTL = 26;

// Codecs - Png - Utilities
// ========================

static BL_INLINE bool png_check_color_type_and_bit_depth(uint32_t color_type, uint32_t depth) noexcept {
  // TODO: [PNG] 16-BPC.
  if (depth == 16) {
    return false;
  }

  return color_type < BL_ARRAY_SIZE(png_color_type_bit_depth_table) &&
         (png_color_type_bit_depth_table[color_type] & depth) != 0 &&
         axl::is_power_of_2(depth);
}

static BL_INLINE void png_create_grayscale_palette(BLRgba32* pal, uint32_t depth) noexcept {
  static const uint32_t scale_table[9] = { 0, 0xFF, 0x55, 0, 0x11, 0, 0, 0, 0x01 };
  BL_ASSERT(depth < BL_ARRAY_SIZE(scale_table));

  uint32_t scale = uint32_t(scale_table[depth]) * 0x00010101;
  uint32_t count = 1u << depth;
  uint32_t value = 0xFF000000;

  for (uint32_t i = 0; i < count; i++, value += scale) {
    pal[i].value = value;
  }
}

// Codecs - Png - Interlace / Deinterlace
// ======================================

// A single PNG interlace/deinterlace step related to the full image size.
struct PngInterlaceStep {
  uint32_t used;
  uint32_t width;
  uint32_t height;
  uint32_t bpl;

  uint32_t offset;
  uint32_t size;
};

// PNG deinterlace table data.
struct PngInterlaceTable {
  uint8_t x_off;
  uint8_t y_off;
  uint8_t x_pow;
  uint8_t y_pow;
};

// No interlacing.
static const PngInterlaceTable png_internace_none_table[1] = {
  { 0, 0, 0, 0 }
};

// Passes start from zero to stay compatible with interlacing tables, however,
// this representation is not visually compatible with PNG spec, where passes
// are indexed from `1` (that's the only difference).
//
//        8x8 block
//   +-----------------+
//   | 0 5 3 5 1 5 3 5 |
//   | 6 6 6 6 6 6 6 6 |
//   | 4 5 4 5 4 5 4 5 |
//   | 6 6 6 6 6 6 6 6 |
//   | 2 5 3 5 2 5 3 5 |
//   | 6 6 6 6 6 6 6 6 |
//   | 4 5 4 5 4 5 4 5 |
//   | 6 6 6 6 6 6 6 6 |
//   +-----------------+
static const PngInterlaceTable png_interlace_adam7_table[7] = {
  { 0, 0, 3, 3 },
  { 4, 0, 3, 3 },
  { 0, 4, 2, 3 },
  { 2, 0, 2, 2 },
  { 0, 2, 1, 2 },
  { 1, 0, 1, 1 },
  { 0, 1, 0, 1 }
};

static uint32_t png_calculate_interlace_steps(
  PngInterlaceStep* dst, const PngInterlaceTable* table, uint32_t step_count,
  uint32_t sample_depth, uint32_t sample_count,
  uint32_t w, uint32_t h) noexcept {

  // Byte-offset of each chunk.
  uint32_t offset = 0;

  for (uint32_t i = 0; i < step_count; i++, dst++) {
    const PngInterlaceTable& tab = table[i];

    uint32_t sx = 1 << tab.x_pow;
    uint32_t sy = 1 << tab.y_pow;
    uint32_t sw = (w + sx - tab.x_off - 1) >> tab.x_pow;
    uint32_t sh = (h + sy - tab.y_off - 1) >> tab.y_pow;

    // If the reference image contains fewer than five columns or fewer than
    // five rows, some passes will be empty, decoders must handle this case.
    uint32_t used = sw != 0 && sh != 0;

    // NOTE: No need to check for overflow at this point as we have already
    // calculated the total BPL of the whole image, and since interlacing is
    // splitting it into multiple images, it can't overflow the base size.
    uint32_t bpl = ((sw * sample_depth + 7) / 8) * sample_count + 1;
    uint32_t size = used ? bpl * sh : uint32_t(0);

    dst->used = used;
    dst->width = sw;
    dst->height = sh;
    dst->bpl = bpl;

    dst->offset = offset;
    dst->size = size;

    axl::OverflowFlag of{};
    offset = axl::add_overflow(offset, size, of);

    if (BL_UNLIKELY(axl::did_overflow(of))) {
      return 0;
    }
  }

  return offset;
}

static BL_INLINE uint8_t png_combine_byte_1bpp(uint32_t b0, uint32_t b1, uint32_t b2, uint32_t b3, uint32_t b4, uint32_t b5, uint32_t b6, uint32_t b7) noexcept {
  return uint8_t(((b0) & 0x80) | ((b1) & 0x40) | ((b2) & 0x20) | ((b3) & 0x10) | ((b4) & 0x08) | ((b5) & 0x04) | ((b6) & 0x02) | ((b7) & 0x01));
}

static BL_INLINE uint8_t png_combine_byte_2bpp(uint32_t b0, uint32_t b1, uint32_t b2, uint32_t b3) noexcept {
  return uint8_t(((b0) & 0xC0) + ((b1) & 0x30) + ((b2) & 0x0C) + ((b3) & 0x03));
}

static BL_INLINE uint8_t png_combine_byte_4bpp(uint32_t b0, uint32_t b1) noexcept {
  return uint8_t(((b0) & 0xF0) + ((b1) & 0x0F));
}

// Deinterlace a PNG image that has depth less than 8 bits. This is a bit tricky as one byte describes two or more
// pixels that can be fetched from 1st to 6th progressive images. Basically each bit depth is implemented separately
// as generic case would be very inefficient. Also, the destination image is handled pixel-by-pixel fetching data
// from all possible scanlines as necessary - this is a bit different when compared with `png_deinterlace_bytes()`.
template<uint32_t N>
static void png_deinterlace_bits(
  uint8_t* dst_line, intptr_t dst_stride, const BLPixelConverter& pc,
  uint8_t* tmp_line, intptr_t tmp_stride, const uint8_t* data, const PngInterlaceStep* steps,
  uint32_t w, uint32_t h) noexcept {

  const uint8_t* d0 = data + steps[0].offset;
  const uint8_t* d1 = data + steps[1].offset;
  const uint8_t* d2 = data + steps[2].offset;
  const uint8_t* d3 = data + steps[3].offset;
  const uint8_t* d4 = data + steps[4].offset;
  const uint8_t* d5 = data + steps[5].offset;

  BL_ASSERT(h != 0);

  // We store only to odd scanlines.
  uint32_t y = (h + 1) / 2;
  uint32_t n = 0;

  for (;;) {
    uint8_t* tmp_data = tmp_line + (intptr_t(n) * tmp_stride);
    uint32_t x = w;

    // 1-BPP
    // -----

    if (N == 1) {
      switch (n) {
        // [a b b b a b b b]
        // [0 5 3 5 1 5 3 5]
        case 0: {
          uint32_t a = 0, b;

          d0 += 1;
          d1 += (x >= 5);
          d3 += (x >= 3);
          d5 += (x >= 2);

          while (x >= 32) {
            // Fetched every second iteration.
            if (!(a & 0x80000000))
              a = (uint32_t(*d0++)) + (uint32_t(*d1++) << 8) + 0x08000000u;

            b = (uint32_t(*d3++)      ) +
                (uint32_t(d5[0]) <<  8) +
                (uint32_t(d5[1]) << 16) ;
            d5 += 2;

            tmp_data[0] = png_combine_byte_1bpp(a     , b >>  9, b >> 2, b >> 10, a >> 12, b >> 11, b >> 5, b >> 12);
            tmp_data[1] = png_combine_byte_1bpp(a << 1, b >>  5, b     , b >>  6, a >> 11, b >>  7, b >> 3, b >>  8);
            tmp_data[2] = png_combine_byte_1bpp(a << 2, b >> 17, b << 2, b >> 18, a >> 10, b >> 19, b >> 1, b >> 20);
            tmp_data[3] = png_combine_byte_1bpp(a << 3, b >> 13, b << 4, b >> 14, a >>  9, b >> 15, b << 1, b >> 16);
            tmp_data += 4;

            a <<= 4;
            x -= 32;
          }

          if (!x)
            break;

          if (!(a & 0x80000000)) {
            a = uint32_t(*d0++);
            if (x >= 5)
              a += uint32_t(*d1++) << 8;
          }

          b = 0;
          if (x >=  3) b  = uint32_t(*d3++);
          if (x >=  2) b += uint32_t(*d5++) <<  8;
          if (x >= 18) b += uint32_t(*d5++) << 16;

          tmp_data[0] = png_combine_byte_1bpp(a     , b >>  9, b >> 2, b >> 10, a >> 12, b >> 11, b >> 5, b >> 12);
          if (x <= 8) break;

          tmp_data[1] = png_combine_byte_1bpp(a << 1, b >>  5, b     , b >>  6, a >> 11, b >>  7, b >> 3, b >>  8);
          if (x <= 16) break;

          tmp_data[2] = png_combine_byte_1bpp(a << 2, b >> 17, b << 2, b >> 18, a >> 10, b >> 19, b >> 1, b >> 20);
          if (x <= 24) break;

          tmp_data[3] = png_combine_byte_1bpp(a << 3, b >> 13, b << 4, b >> 14, a >>  9, b >> 15, b << 1, b >> 16);
          break;
        }

        // [a b a b a b a b]
        // [2 5 3 5 2 5 3 5]
        case 2: {
          uint32_t a, b;

          d2 += 1;
          d3 += (x >= 3);
          d5 += (x >= 2);

          while (x >= 32) {
            a = uint32_t(*d2++) + (uint32_t(*d3++) << 8);
            b = uint32_t(d5[0]) + (uint32_t(d5[1]) << 8);
            d5 += 2;

            tmp_data[0] = png_combine_byte_1bpp(a     , b >>  1, a >> 10, b >>  2, a >>  3, b >>  3, a >> 13, b >>  4);
            tmp_data[1] = png_combine_byte_1bpp(a << 2, b <<  3, a >>  8, b <<  2, a >>  1, b <<  1, a >> 11, b      );
            tmp_data[2] = png_combine_byte_1bpp(a << 4, b >>  9, a >>  6, b >> 10, a <<  1, b >> 11, a >>  9, b >> 12);
            tmp_data[3] = png_combine_byte_1bpp(a << 6, b >>  5, a >>  4, b >>  6, a <<  3, b >>  7, a >>  7, b >>  8);
            tmp_data += 4;

            x -= 32;
          }

          if (!x)
            break;

          a = uint32_t(*d2++);
          b = 0;

          if (x >=  3) a += uint32_t(*d3++) << 8;
          if (x >=  2) b  = uint32_t(*d5++);
          if (x >= 18) b += uint32_t(*d5++) << 8;

          tmp_data[0] = png_combine_byte_1bpp(a     , b >>  1, a >> 10, b >>  2, a >>  3, b >>  3, a >> 13, b >>  4);
          if (x <=  8) break;

          tmp_data[1] = png_combine_byte_1bpp(a << 2, b <<  3, a >>  8, b <<  2, a >>  1, b <<  1, a >> 11, b      );
          if (x <= 16) break;

          tmp_data[2] = png_combine_byte_1bpp(a << 4, b >>  9, a >>  6, b >> 10, a <<  1, b >> 11, a >>  9, b >> 12);
          if (x <= 24) break;

          tmp_data[3] = png_combine_byte_1bpp(a << 6, b >>  5, a >>  4, b >>  6, a <<  3, b >>  7, a >>  7, b >>  8);
          break;
        }

        // [a b a b a b a b]
        // [4 5 4 5 4 5 4 5]
        case 1:
        case 3: {
          uint32_t a, b;

          d4 += 1;
          d5 += (x >= 2);

          while (x >= 16) {
            a = uint32_t(*d4++);
            b = uint32_t(*d5++);

            tmp_data[0] = png_combine_byte_1bpp(a     , b >> 1, a >> 1, b >> 2, a >> 2, b >> 3, a >> 3, b >> 4);
            tmp_data[1] = png_combine_byte_1bpp(a << 4, b << 3, a << 3, b << 2, a << 2, b << 1, a << 1, b     );
            tmp_data += 2;

            x -= 16;
          }

          if (!x)
            break;

          a = uint32_t(*d4++);
          b = 0;

          if (x >= 2)
            b = uint32_t(*d5++);

          tmp_data[0] = png_combine_byte_1bpp(a     , b >> 1, a >> 1, b >> 2, a >> 2, b >> 3, a >> 3, b >> 4);

          if (x <= 8) break;
          tmp_data[1] = png_combine_byte_1bpp(a << 4, b << 3, a << 3, b << 2, a << 2, b << 1, a << 1, b     );
          break;
        }
      }
    }

    // 2-BPP
    // -----

    else if (N == 2) {
      switch (n) {
        // [aa bb bb bb][aa bb bb bb]
        // [00 55 33 55][11 55 33 55]
        case 0: {
          uint32_t a = 0, b;

          d0 += 1;
          d1 += (x >= 5);
          d3 += (x >= 3);
          d5 += (x >= 2);

          while (x >= 16) {
            // Fetched every second iteration.
            if (!(a & 0x80000000))
              a = (uint32_t(*d0++)     ) +
                  (uint32_t(*d1++) << 8) + 0x08000000;

            b = (uint32_t(*d3++)      ) +
                (uint32_t(d5[0]) <<  8) +
                (uint32_t(d5[1]) << 16) ;
            d5 += 2;

            tmp_data[0] = png_combine_byte_2bpp(a     , b >> 10, b >> 4, b >> 12);
            tmp_data[1] = png_combine_byte_2bpp(a >> 8, b >>  6, b >> 2, b >>  8);
            tmp_data[2] = png_combine_byte_2bpp(a << 2, b >> 18, b     , b >> 20);
            tmp_data[3] = png_combine_byte_2bpp(a >> 6, b >> 14, b << 2, b >> 16);
            tmp_data += 4;

            a <<= 4;
            x -= 16;
          }

          if (!x)
            break;

          if (!(a & 0x80000000)) {
            a = (uint32_t(*d0++));
            if (x >= 5)
              a += (uint32_t(*d1++) << 8);
          }

          b = 0;
          if (x >=  3) b  = (uint32_t(*d3++)      );
          if (x >=  2) b += (uint32_t(*d5++) <<  8);
          if (x >= 10) b += (uint32_t(*d5++) << 16);

          tmp_data[0] = png_combine_byte_2bpp(a     , b >> 10, b >> 4, b >> 12);
          if (x <=  4) break;

          tmp_data[1] = png_combine_byte_2bpp(a >> 8, b >>  6, b >> 2, b >>  8);
          if (x <=  8) break;

          tmp_data[2] = png_combine_byte_2bpp(a << 2, b >> 18, b     , b >> 20);
          if (x <= 12) break;

          tmp_data[3] = png_combine_byte_2bpp(a >> 6, b >> 14, b << 2, b >> 16);
          break;
        }

        // [aa bb aa bb][aa bb aa bb]
        // [22 55 33 55][22 55 33 55]
        case 2: {
          uint32_t a, b;

          d2 += 1;
          d3 += (x >= 3);
          d5 += (x >= 2);

          while (x >= 16) {
            a = uint32_t(*d2++) + (uint32_t(*d3++) << 8);
            b = uint32_t(*d5++);

            tmp_data[0] = png_combine_byte_2bpp(a     , b >>  2, a >> 12, b >>  4);
            tmp_data[1] = png_combine_byte_2bpp(a << 2, b <<  2, a >> 10, b      );

            b = uint32_t(*d5++);

            tmp_data[2] = png_combine_byte_2bpp(a << 4, b >>  2, a >>  8, b >>  4);
            tmp_data[3] = png_combine_byte_2bpp(a << 6, b <<  2, a >>  6, b      );
            tmp_data += 4;

            x -= 16;
          }

          if (!x)
            break;

          a = uint32_t(*d2++);
          b = 0;

          if (x >=  3) a  = (uint32_t(*d3++) << 8);
          if (x >=  2) b  = (uint32_t(*d5++)     );
          if (x >= 10) b += (uint32_t(*d5++) << 8);

          tmp_data[0] = png_combine_byte_2bpp(a     , b >>  2, a >> 12, b >>  4);
          if (x <=  4) break;

          tmp_data[1] = png_combine_byte_2bpp(a << 2, b <<  2, a >> 10, b      );
          if (x <=  8) break;

          tmp_data[2] = png_combine_byte_2bpp(a << 4, b >> 10, a >>  8, b >> 12);
          if (x <= 12) break;

          tmp_data[3] = png_combine_byte_2bpp(a << 6, b >>  6, a >>  6, b >>  8);
          break;
        }

        // [aa bb aa bb][aa bb aa bb]
        // [44 55 44 55][44 55 44 55]
        case 1:
        case 3: {
          uint32_t a, b;

          d4 += 1;
          d5 += (x >= 2);

          while (x >= 8) {
            a = uint32_t(*d4++);
            b = uint32_t(*d5++);

            tmp_data[0] = png_combine_byte_2bpp(a     , b >> 2, a >> 2, b >> 4);
            tmp_data[1] = png_combine_byte_2bpp(a << 4, b << 2, a << 2, b     );
            tmp_data += 2;

            x -= 8;
          }

          if (!x)
            break;

          a = uint32_t(*d4++);
          b = 0;

          if (x >= 2)
            b = uint32_t(*d5++);

          tmp_data[0] = png_combine_byte_2bpp(a     , b >> 10, b >> 4, b >> 12);
          if (x <=  4) break;

          tmp_data[1] = png_combine_byte_2bpp(a >> 8, b >>  6, b >> 2, b >>  8);
          break;
        }
      }
    }

    // 4-BPP
    // -----

    else if (N == 4) {
      switch (n) {
        // [aaaa bbbb][bbbb bbbb][aaaa bbbb][bbbb bbbb]
        // [0000 5555][3333 5555][1111 5555][3333 5555]
        case 0: {
          uint32_t a = 0, b;

          d0 += 1;
          d1 += (x >= 5);
          d3 += (x >= 3);
          d5 += (x >= 2);

          while (x >= 8) {
            // Fetched every second iteration.
            if (!(a & 0x80000000))
              a = (uint32_t(*d0++)      ) +
                  (uint32_t(*d1++) <<  8) + 0x08000000;

            b = (uint32_t(*d3++)      ) +
                (uint32_t(d5[0]) <<  8) +
                (uint32_t(d5[1]) << 16) ;
            d5 += 2;

            tmp_data[0] = png_combine_byte_4bpp(a     , b >> 12);
            tmp_data[1] = png_combine_byte_4bpp(b     , b >>  8);
            tmp_data[2] = png_combine_byte_4bpp(a >> 8, b >> 20);
            tmp_data[3] = png_combine_byte_4bpp(b << 4, b >> 16);
            tmp_data += 4;

            a <<= 4;
            x -= 8;
          }

          if (!x)
            break;

          if (!(a & 0x80000000)) {
            a = (uint32_t(*d0++));
            if (x >= 5)
              a += (uint32_t(*d1++) << 8);
          }

          b = 0;
          if (x >= 3) b  = (uint32_t(*d3++)      );
          if (x >= 2) b += (uint32_t(*d5++) <<  8);
          if (x >= 6) b += (uint32_t(*d5++) << 16);

          tmp_data[0] = png_combine_byte_4bpp(a, b >> 12);
          if (x <= 2) break;

          tmp_data[1] = png_combine_byte_4bpp(b, b >> 8);
          if (x <= 4) break;

          tmp_data[2] = png_combine_byte_4bpp(a >> 8, b >> 20);
          if (x <= 6) break;

          tmp_data[3] = png_combine_byte_4bpp(b << 4, b >> 16);
          break;
        }

        // [aaaa bbbb][aaaa bbbb][aaaa bbbb][aaaa bbbb]
        // [2222 5555][3333 5555][2222 5555][3333 5555]
        case 2: {
          uint32_t a, b;

          d2 += 1;
          d3 += (x >= 3);
          d5 += (x >= 2);

          while (x >= 8) {
            a = uint32_t(*d2++) + (uint32_t(*d3++) << 8);
            b = uint32_t(*d5++);
            tmp_data[0] = png_combine_byte_4bpp(a, b >> 4);
            tmp_data[1] = png_combine_byte_4bpp(a >> 8, b);

            b = uint32_t(*d5++);
            tmp_data[2] = png_combine_byte_4bpp(a << 4, b >> 4);
            tmp_data[3] = png_combine_byte_4bpp(a >> 4, b);
            tmp_data += 4;

            x -= 8;
          }

          if (!x)
            break;

          a = uint32_t(*d2++);
          b = 0;

          if (x >= 3) a += (uint32_t(*d3++) << 8);
          if (x >= 2) b  = (uint32_t(*d5++)     );

          tmp_data[0] = png_combine_byte_4bpp(a, b >> 4);
          if (x <= 2) break;

          tmp_data[1] = png_combine_byte_4bpp(a >> 8, b);
          if (x <= 4) break;

          b = uint32_t(*d5++);
          tmp_data[2] = png_combine_byte_4bpp(a << 4, b >> 4);
          if (x <= 6) break;

          tmp_data[3] = png_combine_byte_4bpp(a >> 4, b);
          break;
        }

        // [aaaa bbbb aaaa bbbb][aaaa bbbb aaaa bbbb]
        // [4444 5555 4444 5555][4444 5555 4444 5555]
        case 1:
        case 3: {
          uint32_t a, b;

          d4 += 1;
          d5 += (x >= 2);

          while (x >= 4) {
            a = uint32_t(*d4++);
            b = uint32_t(*d5++);

            tmp_data[0] = png_combine_byte_4bpp(a, b >> 4);
            tmp_data[1] = png_combine_byte_4bpp(a << 4, b);
            tmp_data += 2;

            x -= 4;
          }

          if (!x)
            break;

          a = uint32_t(*d4++);
          b = 0;

          if (x >= 2)
            b = uint32_t(*d5++);

          tmp_data[0] = png_combine_byte_4bpp(a, b >> 4);
          if (x <= 2) break;

          tmp_data[1] = png_combine_byte_4bpp(a << 4, b);
          break;
        }
      }
    }

    // Don't change to `||`, both have to be executed!
    if (uint32_t(--y == 0) | uint32_t(++n == 4)) {
      pc.convert_rect(dst_line, dst_stride * 2, tmp_line, tmp_stride, w, n);
      dst_line += dst_stride * 8;

      if (y == 0)
        break;
      n = 0;
    }
  }
}

// Copy `N` bytes from unaligned `src` into aligned `dst`. Allows us to handle
// some special cases if the CPU supports unaligned reads/writes from/to memory.
template<uint32_t N>
static BL_INLINE const uint8_t* png_copy_bytes(uint8_t* dst, const uint8_t* src) noexcept {
  if (N == 2) {
    axl::storea_u16(dst, axl::loadu_u16(src));
  }
  else if (N == 4) {
    axl::storea_u32(dst, axl::loadu_u32(src));
  }
  else if (N == 8) {
    axl::storea_u32(dst + 0, axl::loadu_u32(src + 0));
    axl::storea_u32(dst + 4, axl::loadu_u32(src + 4));
  }
  else {
    if (N >= 1) dst[0] = src[0];
    if (N >= 2) dst[1] = src[1];
    if (N >= 3) dst[2] = src[2];
    if (N >= 4) dst[3] = src[3];
    if (N >= 5) dst[4] = src[4];
    if (N >= 6) dst[5] = src[5];
    if (N >= 7) dst[6] = src[6];
    if (N >= 8) dst[7] = src[7];
  }
  return src + N;
}

template<uint32_t N>
static void png_deinterlace_bytes(
  uint8_t* dst_line, intptr_t dst_stride, const BLPixelConverter& pc,
  uint8_t* tmp_line, intptr_t tmp_stride, const uint8_t* data, const PngInterlaceStep* steps,
  uint32_t w, uint32_t h) noexcept {

  const uint8_t* d0 = data + steps[0].offset;
  const uint8_t* d1 = data + steps[1].offset;
  const uint8_t* d2 = data + steps[2].offset;
  const uint8_t* d3 = data + steps[3].offset;
  const uint8_t* d4 = data + steps[4].offset;
  const uint8_t* d5 = data + steps[5].offset;

  BL_ASSERT(h != 0);

  // We store only to odd scanlines.
  uint32_t y = (h + 1) / 2;
  uint32_t n = 0;
  uint32_t x_max = w * N;

  for (;;) {
    uint8_t* tmp_data = tmp_line + (intptr_t(n) * tmp_stride);
    uint32_t x;

    switch (n) {
      // [05351535]
      case 0: {
        d0 += 1;
        d1 += (w >= 5);
        d3 += (w >= 3);
        d5 += (w >= 2);

        for (x = 0 * N; x < x_max; x += 8 * N) d0 = png_copy_bytes<N>(tmp_data + x, d0);
        for (x = 4 * N; x < x_max; x += 8 * N) d1 = png_copy_bytes<N>(tmp_data + x, d1);
        for (x = 2 * N; x < x_max; x += 4 * N) d3 = png_copy_bytes<N>(tmp_data + x, d3);
        for (x = 1 * N; x < x_max; x += 2 * N) d5 = png_copy_bytes<N>(tmp_data + x, d5);

        break;
      }

      // [25352535]
      case 2: {
        d2 += 1;
        d3 += (w >= 3);
        d5 += (w >= 2);

        for (x = 0 * N; x < x_max; x += 4 * N) d2 = png_copy_bytes<N>(tmp_data + x, d2);
        for (x = 2 * N; x < x_max; x += 4 * N) d3 = png_copy_bytes<N>(tmp_data + x, d3);
        for (x = 1 * N; x < x_max; x += 2 * N) d5 = png_copy_bytes<N>(tmp_data + x, d5);

        break;
      }

      // [45454545]
      case 1:
      case 3: {
        d4 += 1;
        d5 += (w >= 2);

        for (x = 0 * N; x < x_max; x += 2 * N) d4 = png_copy_bytes<N>(tmp_data + x, d4);
        for (x = 1 * N; x < x_max; x += 2 * N) d5 = png_copy_bytes<N>(tmp_data + x, d5);

        break;
      }
    }

    // Don't change to `||`, both have to be executed!
    if (uint32_t(--y == 0) | uint32_t(++n == 4)) {
      pc.convert_rect(dst_line, dst_stride * 2, tmp_line, tmp_stride, w, n);
      dst_line += dst_stride * 8;

      if (y == 0)
        break;
      n = 0;
    }
  }
}

// Codecs - Png - Decoder - Chunk Reader
// =====================================

namespace {

class PngChunkReader {
public:
  const uint8_t* ptr {};
  const uint8_t* end {};

  BL_INLINE_NODEBUG PngChunkReader(const uint8_t* ptr, const uint8_t* end) noexcept
    : ptr(ptr),
      end(end) {}

  BL_INLINE_NODEBUG size_t remaining_bytes() const noexcept { return axl::ptr_byte_diff(ptr, end); }

  BL_INLINE void advance(size_t size) noexcept {
    BL_ASSERT(size <= remaining_bytes());
    ptr += size;
  }

  BL_INLINE void advance_chunk_header() noexcept {
    BL_ASSERT(remaining_bytes() >= kPngChunkHeaderSize);
    ptr += kPngChunkHeaderSize;
  }

  BL_INLINE void advance_checksum() noexcept {
    BL_ASSERT(remaining_bytes() >= kPngChunkCRCSize);
    ptr += kPngChunkCRCSize;
  }

  BL_INLINE_NODEBUG bool at_end() const noexcept { return ptr == end; }
  BL_INLINE_NODEBUG bool has_chunk() const noexcept { return remaining_bytes() >= kPngChunkBaseSize; }

  BL_INLINE_NODEBUG bool has_chunk_with_size(size_t size) const noexcept {
    // Always called after `has_chunk()` with the advertized size of the chunk, so we always have at least 12 bytes.
    BL_ASSERT(remaining_bytes() >= kPngChunkBaseSize);

    return remaining_bytes() - kPngChunkBaseSize >= size;
  }

  BL_INLINE uint32_t read_chunk_size() const noexcept {
    BL_ASSERT(has_chunk());
    return axl::loadu_u32_be(ptr + 0);
  }

  BL_INLINE uint32_t read_chunk_tag() const noexcept {
    BL_ASSERT(has_chunk());
    return axl::loadu_u32_be(ptr + 4);
  }

  BL_INLINE uint32_t read_u8(size_t offset) const noexcept {
    BL_ASSERT(offset + 1u <= remaining_bytes());
    return axl::load_u8(ptr + offset);
  }

  BL_INLINE uint32_t read_u16(size_t offset) const noexcept {
    BL_ASSERT(offset + 2u <= remaining_bytes());
    return axl::loadu_u16_be(ptr + offset);
  }

  BL_INLINE uint32_t read_u32(size_t offset) const noexcept {
    BL_ASSERT(offset + 4u <= remaining_bytes());
    return axl::loadu_u32_be(ptr + offset);
  }
};

} // {anonymous}

// Codecs - Png - Decoder - API
// ============================

static BLResult BL_CDECL png_decoder_restart(BLImageDecoderCore* self) noexcept {
  PngDecoderImpl* impl = self->_impl<PngDecoderImpl>();
  impl->last_result = BL_SUCCESS;
  impl->frame_index = 0;
  impl->buffer_index = 0;
  impl->image_info.reset();
  impl->status_flags = DecoderStatusFlags::kNone;
  impl->color_type = 0;
  impl->sample_depth = 0;
  impl->sample_count = 0;
  impl->output_format = uint8_t(BL_FORMAT_NONE);
  impl->color_key.reset();
  impl->palette_size = 0;
  impl->first_fctl_offset = 0;
  impl->prev_ctrl = FCTL{};
  impl->frame_ctrl = FCTL{};
  return BL_SUCCESS;
}

static BLResult png_decoder_read_fctl(PngDecoderImpl* impl, size_t chunk_offset, BLSpan<const uint8_t> chunk) noexcept {
  if (BL_UNLIKELY(chunk.size() < kPngChunkDataSize_fcTL)) {
    return bl_make_error(BL_ERROR_INVALID_DATA);
  }

  uint32_t n = axl::loadu_u32_be(chunk.data() + 0u);
  uint32_t w = axl::loadu_u32_be(chunk.data() + 4u);
  uint32_t h = axl::loadu_u32_be(chunk.data() + 8u);
  uint32_t x = axl::loadu_u32_be(chunk.data() + 12u);
  uint32_t y = axl::loadu_u32_be(chunk.data() + 16u);
  uint32_t delay_num = axl::loadu_u16_be(chunk.data() + 20u);
  uint32_t delay_den = axl::loadu_u16_be(chunk.data() + 22u);
  uint32_t dispose_op = axl::load_u8(chunk.data() + 24u);
  uint32_t blend_op = axl::load_u8(chunk.data() + 25u);

  if (BL_UNLIKELY(x >= uint32_t(impl->image_info.size.w) ||
                  y >= uint32_t(impl->image_info.size.h) ||
                  w > uint32_t(impl->image_info.size.w) - x ||
                  h > uint32_t(impl->image_info.size.h) - y ||
                  dispose_op > kAPNGDisposeOpMaxValue ||
                  blend_op > kAPNGBlendOpMaxValue)) {
    return bl_make_error(BL_ERROR_INVALID_DATA);
  }

  if (impl->first_fctl_offset == 0) {
    impl->first_fctl_offset = chunk_offset;
  }

  impl->prev_ctrl = impl->frame_ctrl;
  impl->frame_ctrl.sequence_number = n;
  impl->frame_ctrl.w = w;
  impl->frame_ctrl.h = h;
  impl->frame_ctrl.x = x;
  impl->frame_ctrl.y = y;
  impl->frame_ctrl.delay_num = uint16_t(delay_num);
  impl->frame_ctrl.delay_den = uint16_t(delay_den);
  impl->frame_ctrl.dispose_op = uint8_t(dispose_op);
  impl->frame_ctrl.blend_op = uint8_t(blend_op);
  impl->add_flag(DecoderStatusFlags::kRead_fcTL);

  return BL_SUCCESS;
}

static BLResult png_decoder_read_info_internal(PngDecoderImpl* impl, const uint8_t* p, size_t size) noexcept {
  constexpr size_t kMinSize_PNG = kPngSignatureSize + kPngChunkBaseSize + kPngChunkDataSize_IHDR;
  constexpr size_t kMinSize_CgBI = kMinSize_PNG + kPngChunkBaseSize + kPngChunkDataSize_CgBI;

  if (BL_UNLIKELY(size < kMinSize_PNG)) {
    return bl_make_error(BL_ERROR_DATA_TRUNCATED);
  }

  // Check PNG signature.
  if (BL_UNLIKELY(memcmp(p, png_signature_bytes, kPngSignatureSize) != 0)) {
    return bl_make_error(BL_ERROR_INVALID_SIGNATURE);
  }

  PngChunkReader chunk_reader(p + kPngSignatureSize, p + size);

  // Already verified by `kMinSize_PNG` check - so it must be true.
  BL_ASSERT(chunk_reader.has_chunk());

  // Expect 'IHDR' or 'CgBI' chunk.
  uint32_t chunk_tag = chunk_reader.read_chunk_tag();
  uint32_t chunk_size = chunk_reader.read_chunk_size();

  // Read 'CgBI' Chunk (4 Bytes)
  // ---------------------------

  // Support "CgBI" aka "CoreGraphicsBrokenImage" - a violation of the PNG Spec:
  //   1. http://www.jongware.com/pngdefry.html
  //   2. http://iphonedevwiki.net/index.php/CgBI_file_format
  if (chunk_tag == BL_MAKE_TAG('C', 'g', 'B', 'I')) {
    if (BL_UNLIKELY(chunk_size != kPngChunkDataSize_CgBI)) {
      return bl_make_error(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
    }

    if (BL_UNLIKELY(size < kMinSize_CgBI)) {
      return bl_make_error(BL_ERROR_DATA_TRUNCATED);
    }

    impl->add_flag(DecoderStatusFlags::kRead_CgBI);

    // Skip "CgBI" chunk and read the next chunk tag/size, which must be 'IHDR'.
    chunk_reader.advance(kPngChunkBaseSize + kPngChunkDataSize_CgBI);

    chunk_tag = chunk_reader.read_chunk_tag();
    chunk_size = chunk_reader.read_chunk_size();
  }

  // Read 'IHDR' Chunk (13 Bytes)
  // ----------------------------

  if (chunk_tag != BL_MAKE_TAG('I', 'H', 'D', 'R') || chunk_size != kPngChunkDataSize_IHDR) {
    return bl_make_error(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
  }

  uint32_t w            = chunk_reader.read_u32(kPngChunkHeaderSize + 0u);
  uint32_t h            = chunk_reader.read_u32(kPngChunkHeaderSize + 4u);
  uint32_t sample_depth = chunk_reader.read_u8(kPngChunkHeaderSize + 8u);
  uint32_t color_type   = chunk_reader.read_u8(kPngChunkHeaderSize + 9u);
  uint32_t compression  = chunk_reader.read_u8(kPngChunkHeaderSize + 10u);
  uint32_t filter       = chunk_reader.read_u8(kPngChunkHeaderSize + 11u);
  uint32_t progressive  = chunk_reader.read_u8(kPngChunkHeaderSize + 12u);

  chunk_reader.advance(kPngChunkBaseSize + kPngChunkDataSize_IHDR);

  // Width/Height can't be zero or greater than `2^31 - 1`.
  if (BL_UNLIKELY(w == 0 || h == 0)) {
    return bl_make_error(BL_ERROR_INVALID_DATA);
  }

  if (BL_UNLIKELY(w >= 0x80000000u || h >= 0x80000000u)) {
    return bl_make_error(BL_ERROR_IMAGE_TOO_LARGE);
  }

  if (BL_UNLIKELY(!png_check_color_type_and_bit_depth(color_type, sample_depth))) {
    return bl_make_error(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
  }

  // Compression and filter has to be zero, progressive can be [0, 1].
  if (BL_UNLIKELY(compression != 0 || filter != 0 || progressive >= 2)) {
    return bl_make_error(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
  }

  // Setup the image information.
  impl->add_flag(DecoderStatusFlags::kRead_IHDR);
  impl->color_type = uint8_t(color_type);
  impl->sample_depth = uint8_t(sample_depth);
  impl->sample_count = png_color_type_to_sample_count_table[color_type];

  impl->image_info.size.reset(int(w), int(h));
  impl->image_info.depth = uint16_t(sample_depth * uint32_t(impl->sample_count));
  impl->image_info.frame_count = 1;
  impl->image_info.flags = progressive ? BL_IMAGE_INFO_FLAG_PROGRESSIVE : BL_IMAGE_INFO_FLAG_NO_FLAGS;

  memcpy(impl->image_info.format, "PNG", 4);
  memcpy(impl->image_info.compression, "DEFLATE", 8);

  BLFormat output_format = color_type == kColorType2_RGB ? BL_FORMAT_XRGB32 : BL_FORMAT_PRGB32;

  impl->output_format = uint8_t(output_format);
  impl->buffer_index = axl::ptr_byte_diff(p, chunk_reader.ptr);

  // Read Extra Chunks to Detect APNG
  // --------------------------------

  while (chunk_reader.has_chunk()) {
    chunk_tag = chunk_reader.read_chunk_tag();
    chunk_size = chunk_reader.read_chunk_size();

    if (BL_UNLIKELY(!chunk_reader.has_chunk_with_size(chunk_size))) {
      break;
    }

    if (chunk_tag == BL_MAKE_TAG('a', 'c', 'T', 'L')) {
      // Animated PNG chunk.
      if (chunk_size != kPngChunkDataSize_acTL) {
        // Don't refuse the file, but don't mark it as APNG (we would just treat it as a regular PNG if 'acTL' is broken).
        break;
      }

      uint32_t frame_count = chunk_reader.read_u32(kPngChunkHeaderSize + 0u);
      uint32_t repeat_count = chunk_reader.read_u32(kPngChunkHeaderSize + 4u);

      if (frame_count <= 1) {
        break;
      }

      impl->image_info.frame_count = frame_count;
      impl->image_info.repeat_count = repeat_count;
      memcpy(impl->image_info.format, "APNG", 5);
      impl->add_flag(DecoderStatusFlags::kRead_acTL);
      break;
    }

    if ((chunk_tag == BL_MAKE_TAG('I', 'H', 'D', 'R')) ||
        (chunk_tag == BL_MAKE_TAG('P', 'L', 'T', 'E')) ||
        (chunk_tag == BL_MAKE_TAG('I', 'D', 'A', 'T')) ||
        (chunk_tag == BL_MAKE_TAG('I', 'E', 'N', 'D'))) {
      break;
    }

    chunk_reader.advance(size_t(kPngChunkBaseSize) + size_t(chunk_size));
  }

  return BL_SUCCESS;
}

// Reads initial chunks and stops at the beginning of pixel data ('IDAT' and 'fdAT') or 'IEND'.
static BLResult png_decoder_read_important_chunks(PngDecoderImpl* impl, const uint8_t* p, size_t size) noexcept {
  // Don't read beyond the user provided buffer.
  if (BL_UNLIKELY(size < impl->buffer_index)) {
    return bl_make_error(BL_ERROR_INVALID_STATE);
  }

  PngChunkReader chunk_reader(p + impl->buffer_index, p + size);
  for (;;) {
    if (BL_UNLIKELY(!chunk_reader.has_chunk())) {
      return bl_make_error(BL_ERROR_DATA_TRUNCATED);
    }

    uint32_t chunk_tag = chunk_reader.read_chunk_tag();
    uint32_t chunk_size = chunk_reader.read_chunk_size();

    if (BL_UNLIKELY(!chunk_reader.has_chunk_with_size(chunk_size))) {
      return bl_make_error(BL_ERROR_DATA_TRUNCATED);
    }

    if (chunk_tag == BL_MAKE_TAG('P', 'L', 'T', 'E')) {
      // Read 'PLTE' Chunk (Once)
      // ------------------------

      // 1. There must not be more than one PLTE chunk.
      // 2. It must precede the first IDAT chunk (also tRNS chunk).
      // 3. Contains 1...256 RGB palette entries.
      if (impl->has_flag(DecoderStatusFlags::kRead_PLTE | DecoderStatusFlags::kRead_tRNS)) {
        return bl_make_error(BL_ERROR_PNG_INVALID_PLTE);
      }

      if (chunk_size == 0 || chunk_size > 768 || (chunk_size % 3) != 0) {
        return bl_make_error(BL_ERROR_PNG_INVALID_PLTE);
      }

      chunk_reader.advance_chunk_header();

      uint32_t i = 0u;
      uint32_t palette_size = chunk_size / 3;

      impl->add_flag(DecoderStatusFlags::kRead_PLTE);
      impl->palette_size = palette_size;

      while (i < palette_size) {
        impl->palette_data[i++] = BLRgba32(chunk_reader.read_u8(0u),
                                              chunk_reader.read_u8(1u),
                                              chunk_reader.read_u8(2u));
        chunk_reader.advance(3);
      }

      while (i < 256u) {
        impl->palette_data[i++] = BLRgba32(0x00, 0x00, 0x00, 0xFF);
      }

      chunk_reader.advance(4u); // CRC32
    }
    else if (chunk_tag == BL_MAKE_TAG('t', 'R', 'N', 'S')) {
      // Read 'tRNS' Chunk (Once)
      // ------------------------

      uint32_t color_type = impl->color_type;

      // 1. There must not be more than one 'tRNS' chunk.
      // 2. It must precede the first 'IDAT' chunk and follow a 'PLTE' chunk, if any.
      // 3. It is prohibited for color types 4 and 6.
      if (impl->has_flag(DecoderStatusFlags::kRead_tRNS)) {
        return bl_make_error(BL_ERROR_PNG_INVALID_TRNS);
      }

      if (color_type == kColorType4_LUMA || color_type == kColorType6_RGBA) {
        return bl_make_error(BL_ERROR_PNG_INVALID_TRNS);
      }

      if (color_type == kColorType0_LUM) {
        // For color type 0 (grayscale), the tRNS chunk contains a single gray level value, stored in the format:
        //   [0..1] Gray:  2 bytes, range 0 .. (2^depth)-1
        if (chunk_size != 2u) {
          return bl_make_error(BL_ERROR_PNG_INVALID_TRNS);
        }

        uint32_t gray = chunk_reader.read_u16(kPngChunkHeaderSize);
        impl->color_key.reset(gray, gray, gray, 0u);
        impl->add_flag(DecoderStatusFlags::kHasColorKey);

        chunk_reader.advance(kPngChunkBaseSize + 2u);
      }
      else if (color_type == kColorType2_RGB) {
        // For color type 2 (truecolor), the tRNS chunk contains a single RGB color value, stored in the format:
        //   [0..1] Red:   2 bytes, range 0 .. (2^depth)-1
        //   [2..3] Green: 2 bytes, range 0 .. (2^depth)-1
        //   [4..5] Blue:  2 bytes, range 0 .. (2^depth)-1
        if (chunk_size != 6u) {
          return bl_make_error(BL_ERROR_PNG_INVALID_TRNS);
        }

        uint32_t r = chunk_reader.read_u16(kPngChunkHeaderSize + 0u);
        uint32_t g = chunk_reader.read_u16(kPngChunkHeaderSize + 2u);
        uint32_t b = chunk_reader.read_u16(kPngChunkHeaderSize + 4u);

        impl->color_key.reset(r, g, b, 0u);
        impl->add_flag(DecoderStatusFlags::kHasColorKey);

        chunk_reader.advance(kPngChunkBaseSize + 6u);
      }
      else {
        // For color type 3 (indexed color), the tRNS chunk contains a series of one-byte alpha values, corresponding
        // to entries in the PLTE chunk.
        BL_ASSERT(color_type == kColorType3_PAL);

        // 1. Has to follow PLTE if color type is 3.
        // 2. The tRNS chunk can contain 1...pal_size alpha values, but in general it can contain less than `pal_size`
        //    values, in that case the remaining alpha values are assumed to be 255.
        if (!impl->has_flag(DecoderStatusFlags::kRead_PLTE) || chunk_size == 0u || chunk_size > impl->palette_size) {
          return bl_make_error(BL_ERROR_PNG_INVALID_TRNS);
        }

        chunk_reader.advance_chunk_header();

        for (uint32_t i = 0; i < chunk_size; i++) {
          impl->palette_data[i].set_a(chunk_reader.read_u8(i));
        }

        chunk_reader.advance(chunk_size + 4u);
      }

      impl->add_flag(DecoderStatusFlags::kRead_tRNS);
    }
    else if (chunk_tag == BL_MAKE_TAG('I', 'H', 'D', 'R') ||
             chunk_tag == BL_MAKE_TAG('I', 'D', 'A', 'T') ||
             chunk_tag == BL_MAKE_TAG('I', 'E', 'N', 'D') ||
             chunk_tag == BL_MAKE_TAG('f', 'c', 'T', 'L')) {
      // Stop - these will be read by a different function.
      break;
    }
    else {
      if (chunk_tag == BL_MAKE_TAG('f', 'c', 'T', 'L') && impl->is_apng()) {
        if (impl->has_fctl()) {
          return bl_make_error(BL_ERROR_INVALID_DATA);
        }

        BL_PROPAGATE(png_decoder_read_fctl(
          impl,
          axl::ptr_byte_diff(p, chunk_reader.ptr),
          BLSpan<const uint8_t>{chunk_reader.ptr + kPngChunkHeaderSize, chunk_size}));
      }

      // Skip unknown or known, but unsupported chunks.
      chunk_reader.advance(kPngChunkBaseSize + chunk_size);
    }
  }

  // Create a pixel converter capable of converting PNG pixel data to BLImage pixel data.
  BLFormatInfo png_fmt {};
  png_fmt.depth = impl->sample_depth;

  if (axl::kIsLittleEndian) {
    png_fmt.add_flags(BL_FORMAT_FLAG_BYTE_SWAP);
  }

  if (impl->color_type == kColorType0_LUM && impl->sample_depth <= 8) {
    // Treat grayscale images up to 8bpp as indexed and create a dummy palette.
    png_create_grayscale_palette(impl->palette_data, impl->sample_depth);

    // Handle color-key properly.
    if (impl->has_color_key() && impl->color_key.r() < (1u << impl->sample_depth)) {
      impl->palette_data[impl->color_key.r()] = BLRgba32(0u);
    }

    png_fmt.add_flags(BLFormatFlags(BL_FORMAT_FLAG_RGBA | BL_FORMAT_FLAG_INDEXED));
    png_fmt.palette = impl->palette_data;
  }
  else if (impl->color_type == kColorType3_PAL) {
    png_fmt.add_flags(BLFormatFlags(BL_FORMAT_FLAG_RGBA | BL_FORMAT_FLAG_INDEXED));
    png_fmt.palette = impl->palette_data;
  }
  else {
    png_fmt.depth *= impl->sample_count;

    if (impl->color_type == kColorType0_LUM) {
      // TODO: [PNG] 16-BPC.
    }
    else if (impl->color_type == kColorType2_RGB) {
      png_fmt.add_flags(BL_FORMAT_FLAG_RGB);
      png_fmt.r_size = 8; png_fmt.r_shift = 16;
      png_fmt.g_size = 8; png_fmt.g_shift = 8;
      png_fmt.b_size = 8; png_fmt.b_shift = 0;
    }
    else if (impl->color_type == kColorType4_LUMA) {
      png_fmt.add_flags(BL_FORMAT_FLAG_LUMA);
      png_fmt.r_size = 8; png_fmt.r_shift = 8;
      png_fmt.g_size = 8; png_fmt.g_shift = 8;
      png_fmt.b_size = 8; png_fmt.b_shift = 8;
      png_fmt.a_size = 8; png_fmt.a_shift = 0;
    }
    else if (impl->color_type == kColorType6_RGBA) {
      png_fmt.add_flags(BL_FORMAT_FLAG_RGBA);
      png_fmt.r_size = 8; png_fmt.r_shift = 24;
      png_fmt.g_size = 8; png_fmt.g_shift = 16;
      png_fmt.b_size = 8; png_fmt.b_shift = 8;
      png_fmt.a_size = 8; png_fmt.a_shift = 0;
    }

    if (impl->is_cgbi()) {
      Internal::swap(png_fmt.r_shift, png_fmt.b_shift);
      if (png_fmt.has_flag(BL_FORMAT_FLAG_ALPHA)) {
        png_fmt.add_flags(BL_FORMAT_FLAG_PREMULTIPLIED);
      }
    }
  }

  BL_PROPAGATE(impl->pixel_converter.create(bl_format_info[impl->output_format], png_fmt,
    BLPixelConverterCreateFlags(
      BL_PIXEL_CONVERTER_CREATE_FLAG_DONT_COPY_PALETTE |
      BL_PIXEL_CONVERTER_CREATE_FLAG_ALTERABLE_PALETTE)));

  impl->buffer_index = axl::ptr_byte_diff(p, chunk_reader.ptr);
  return BL_SUCCESS;
}

static void png_copy_pixels(uint8_t* dst_data, intptr_t dst_stride, const uint8_t* src_data, intptr_t src_stride, size_t w, uint32_t h) noexcept {
  for (uint32_t i = 0; i < h; i++) {
    memcpy(dst_data, src_data, w);
    dst_data += dst_stride;
    src_data += src_stride;
  }
}

static void png_zero_pixels(uint8_t* dst_data, intptr_t dst_stride, size_t w, uint32_t h) noexcept {
  for (uint32_t i = 0; i < h; i++) {
    memset(dst_data, 0, w);
    dst_data += dst_stride;
  }
}

static BLResult png_decoder_read_pixel_data(PngDecoderImpl* impl, BLImage* image_out, const uint8_t* input, size_t size) noexcept {
  // Number of bytes to overallocate so the DEFLATE decoder doesn't have to run the slow loop at the end.
  constexpr uint32_t kOutputSizeScratch = 1024u;

  // Make sure we won't initialize our chunk reader out of range.
  if (BL_UNLIKELY(size < impl->buffer_index)) {
    return bl_make_error(BL_ERROR_INVALID_STATE);
  }

  PngChunkReader chunk_reader(input + impl->buffer_index, input + size);

  uint32_t x = 0u;
  uint32_t y = 0u;
  uint32_t w = uint32_t(impl->image_info.size.w);
  uint32_t h = uint32_t(impl->image_info.size.h);

  // Advance Chunks
  // --------------

  uint32_t frame_tag =
    (impl->frame_index == 0u)
      ? BL_MAKE_TAG('I', 'D', 'A', 'T')
      : BL_MAKE_TAG('f', 'd', 'A', 'T');

  // Process all preceding chunks, which are not 'IDAT' or 'fdAT'.
  for (;;) {
    if (BL_UNLIKELY(!chunk_reader.has_chunk())) {
      return bl_make_error(BL_ERROR_DATA_TRUNCATED);
    }

    uint32_t chunk_tag = chunk_reader.read_chunk_tag();
    uint32_t chunk_size = chunk_reader.read_chunk_size();

    if (BL_UNLIKELY(!chunk_reader.has_chunk_with_size(chunk_size))) {
      return bl_make_error(BL_ERROR_DATA_TRUNCATED);
    }

    if (chunk_tag == frame_tag) {
      // Found a frame chunk.
      break;
    }

    if (chunk_tag == BL_MAKE_TAG('I', 'H', 'D', 'R')) {
      return bl_make_error(BL_ERROR_PNG_MULTIPLE_IHDR);
    }

    if (chunk_tag == BL_MAKE_TAG('I', 'E', 'N', 'D')) {
      return bl_make_error(BL_ERROR_PNG_INVALID_IEND);
    }

    if (chunk_tag == BL_MAKE_TAG('f', 'c', 'T', 'L') && impl->is_apng()) {
      if (impl->has_fctl()) {
        return bl_make_error(BL_ERROR_INVALID_DATA);
      }
      BL_PROPAGATE(png_decoder_read_fctl(
        impl,
        axl::ptr_byte_diff(input, chunk_reader.ptr),
        BLSpan<const uint8_t>{chunk_reader.ptr + 8u, chunk_size}));
    }

    chunk_reader.advance(size_t(kPngChunkBaseSize) + chunk_size);
  }

  // Handle APNG Frame Window
  // ------------------------

  if (impl->frame_index != 0u) {
    if (!impl->has_fctl()) {
      return bl_make_error(BL_ERROR_INVALID_DATA);
    }

    x = impl->frame_ctrl.x;
    y = impl->frame_ctrl.y;
    w = impl->frame_ctrl.w;
    h = impl->frame_ctrl.h;
  }

  // Decode Pixel Data (DEFLATE)
  // ---------------------------

  uint32_t sample_depth = impl->sample_depth;
  uint32_t sample_count = impl->sample_count;

  uint32_t progressive = (impl->image_info.flags & BL_IMAGE_INFO_FLAG_PROGRESSIVE) != 0;
  uint32_t step_count = progressive ? 7 : 1;

  PngInterlaceStep steps[7];
  uint32_t png_pixel_data_size = png_calculate_interlace_steps(steps, progressive ? png_interlace_adam7_table : png_internace_none_table, step_count, sample_depth, sample_count, w, h);

  if (BL_UNLIKELY(png_pixel_data_size == 0)) {
    return bl_make_error(BL_ERROR_INVALID_DATA);
  }

  BL_PROPAGATE(impl->deflate_decoder.init(impl->deflate_format(), Compression::Deflate::DecoderOptions::kNeverReallocOutputBuffer));
  BL_PROPAGATE(impl->png_pixel_data.clear());
  BL_PROPAGATE(impl->png_pixel_data.reserve(size_t(png_pixel_data_size) + kOutputSizeScratch));

  // Read 'IDAT' or 'fdAT' chunks - once the first chunk is found, it's either the only chunk or there are consecutive
  // chunks of the same type. It's not allowed that the chunks are interleaved with chunks of a different chunk tag.
  {
    uint32_t chunk_size = chunk_reader.read_chunk_size();
    // Was already checked...
    BL_ASSERT(chunk_reader.has_chunk_with_size(chunk_size));

    for (;;) {
      // Zero chunks are allowed, however, they don't contain any data, thus don't call the DEFLATE decoder with these.
      const uint8_t* chunk_data = chunk_reader.ptr + kPngChunkHeaderSize;
      chunk_reader.advance(size_t(kPngChunkBaseSize) + chunk_size);

      if (frame_tag == BL_MAKE_TAG('f', 'd', 'A', 'T')) {
        // The 'fdAT' chunk starts with 4 bytes specifying the sequence.
        if (BL_UNLIKELY(chunk_size < 4u)) {
          return bl_make_error(BL_ERROR_INVALID_DATA);
        }

        chunk_data += 4u;
        chunk_size -= 4u;
      }

      if (chunk_size > 0u) {
        // When the decompression is done, verify whether the decompressed data size matches the PNG pixel data size.
        BLResult result = impl->deflate_decoder.decode(impl->png_pixel_data, BLSpan<const uint8_t>{chunk_data, chunk_size});
        if (result == BL_SUCCESS) {
          if (impl->png_pixel_data.size() != png_pixel_data_size) {
            return bl_make_error(BL_ERROR_INVALID_DATA);
          }
          break;
        }

        // The decoder returns this error (which is not traced) in case that the input data was not enough to
        // decompress the data. It's not an error if more pixel data chunks follow.
        if (result != BL_ERROR_DATA_TRUNCATED) {
          return result;
        }
      }

      // Consecutive chunks required.
      if (BL_UNLIKELY(!chunk_reader.has_chunk())) {
        return bl_make_error(BL_ERROR_DATA_TRUNCATED);
      }

      chunk_size = chunk_reader.read_chunk_size();
      if (BL_UNLIKELY(!chunk_reader.has_chunk_with_size(chunk_size))) {
        return bl_make_error(BL_ERROR_DATA_TRUNCATED);
      }

      uint32_t chunk_tag = chunk_reader.read_chunk_tag();
      if (BL_UNLIKELY(chunk_tag != frame_tag)) {
        return bl_make_error(BL_ERROR_INVALID_DATA);
      }
    }
  }

  impl->clear_flag(DecoderStatusFlags::kRead_fcTL);
  impl->buffer_index = axl::ptr_byte_diff(input, chunk_reader.ptr);

  uint8_t* png_pixel_ptr = const_cast<uint8_t*>(impl->png_pixel_data.data());
  uint32_t bytes_per_pixel = bl_max<uint32_t>((sample_depth * sample_count) / 8, 1);

  // Apply Inverse Filter
  // --------------------

  // If progressive `step_count` is 7 and `steps` array contains all windows.
  for (uint32_t i = 0; i < step_count; i++) {
    PngInterlaceStep& step = steps[i];
    if (step.used) {
      BL_PROPAGATE(Ops::func_table.inverse_filter[bytes_per_pixel](png_pixel_ptr + step.offset, bytes_per_pixel, step.bpl, step.height));
    }
  }

  // Deinterlace & Copy/Blend
  // ------------------------

  BLImageData image_data;

  if (impl->frame_index == 0u) {
    BL_PROPAGATE(image_out->create(int(w), int(h), BLFormat(impl->output_format)));
  }
  else {
    // The animation requires that the user passes an image that has the previous content, but we only want to verify
    // its size and pixel format.
    if (BL_UNLIKELY(image_out->size() != impl->image_info.size || image_out->format() != BLFormat(impl->output_format))) {
      return bl_make_error(BL_ERROR_INVALID_STATE);
    }
  }

  BL_PROPAGATE(image_out->make_mutable(&image_data));

  intptr_t dst_stride = image_data.stride;
  uint8_t* dst_pixels = static_cast<uint8_t*>(image_data.pixel_data);

  if (impl->frame_index != 0u) {
    size_t bpp = image_out->depth() / 8u;
    size_t prev_area_width_in_bytes = impl->prev_ctrl.w * bpp;

    switch (impl->prev_ctrl.dispose_op) {
      case kAPNGDisposeOpBackground: {
        png_zero_pixels(
          dst_pixels + intptr_t(impl->prev_ctrl.y) * dst_stride + intptr_t(impl->prev_ctrl.x * bpp),
          dst_stride,
          prev_area_width_in_bytes,
          impl->prev_ctrl.h);
        break;
      }

      case kAPNGDisposeOpPrevious: {
        const uint8_t* saved_pixels = static_cast<const uint8_t*>(impl->previous_pixel_buffer.get());

        png_copy_pixels(
          dst_pixels + intptr_t(impl->prev_ctrl.y) * dst_stride + intptr_t(impl->prev_ctrl.x * bpp),
          dst_stride,
          saved_pixels,
          intptr_t(prev_area_width_in_bytes),
          prev_area_width_in_bytes,
          impl->prev_ctrl.h);
        break;
      }

      default: {
        // Do nothing if the dispose op is kAPNGDisposeOpNone.
        break;
      }
    }

    dst_pixels += intptr_t(y) * dst_stride + intptr_t(x * bpp);

    if (impl->frame_ctrl.dispose_op == kAPNGDisposeOpPrevious) {
      size_t copy_area_width_in_bytes = w * bpp;
      uint8_t* saved_pixels = static_cast<uint8_t*>(impl->previous_pixel_buffer.alloc(h * copy_area_width_in_bytes));

      if (BL_UNLIKELY(!saved_pixels)) {
        return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
      }

      png_copy_pixels(saved_pixels, intptr_t(copy_area_width_in_bytes), dst_pixels, dst_stride, copy_area_width_in_bytes, h);
    }

    // TODO: [APNG] kAPNGBlendOpOver is currently not supported.
    //
    // if (impl->frame_ctrl.blend_op == kBlendOpOver) {
    // }
  }

  if (progressive) {
    // PNG interlacing requires 7 steps, where 7th handles all even scanlines (indexing from 1). This means that we
    // can, in general, reuse the buffer required by 7th step as a temporary to merge steps 1-6. To achieve this,
    // we need to:
    //
    //   1. Convert all even scanlines already ready by 7th step to `dst`. This makes the buffer ready to be reused.
    //   2. Merge pixels from steps 1-6 into that buffer.
    //   3. Convert all odd scanlines (from the reused buffer) to `dst`.
    //
    // We, in general, process 4 odd scanlines at a time, so we need the 7th buffer to have enough space to hold them
    // as well, if not, we allocate an extra buffer and use it instead. This approach is good as small images would
    // probably require the extra buffer, but larger images can reuse the 7th.
    BL_ASSERT(steps[6].width == w);
    BL_ASSERT(steps[6].height == h / 2); // Half of the rows, rounded down.

    uint32_t depth = sample_depth * sample_count;
    uint32_t tmp_height = bl_min<uint32_t>((h + 1u) / 2u, 4u);
    uint32_t tmp_bpl = steps[6].bpl;
    uint32_t tmp_size;

    if (steps[6].height) {
      impl->pixel_converter.convert_rect(dst_pixels + dst_stride, dst_stride * 2, png_pixel_ptr + 1u + steps[6].offset, intptr_t(tmp_bpl), w, steps[6].height);
    }

    // Align `tmp_bpl` so we can use aligned memory writes and reads while using it.
    tmp_bpl = axl::align_up(tmp_bpl, 16);
    tmp_size = tmp_bpl * tmp_height;

    ScopedBuffer tmp_alloc;
    uint8_t* tmp_pixel_ptr;

    // Decide whether to alloc an extra buffer or to reuse 7th.
    if (steps[6].size < tmp_size + 15) {
      tmp_pixel_ptr = static_cast<uint8_t*>(tmp_alloc.alloc(tmp_size + 15));
    }
    else {
      tmp_pixel_ptr = png_pixel_ptr + steps[6].offset;
    }

    if (!tmp_pixel_ptr) {
      return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
    }

    tmp_pixel_ptr = axl::align_up(tmp_pixel_ptr, 16);
    switch (depth) {
      case 1 : png_deinterlace_bits<1>(dst_pixels, dst_stride, impl->pixel_converter, tmp_pixel_ptr, intptr_t(tmp_bpl), png_pixel_ptr, steps, w, h); break;
      case 2 : png_deinterlace_bits<2>(dst_pixels, dst_stride, impl->pixel_converter, tmp_pixel_ptr, intptr_t(tmp_bpl), png_pixel_ptr, steps, w, h); break;
      case 4 : png_deinterlace_bits<4>(dst_pixels, dst_stride, impl->pixel_converter, tmp_pixel_ptr, intptr_t(tmp_bpl), png_pixel_ptr, steps, w, h); break;
      case 8 : png_deinterlace_bytes<1>(dst_pixels, dst_stride, impl->pixel_converter, tmp_pixel_ptr, intptr_t(tmp_bpl), png_pixel_ptr, steps, w, h); break;
      case 16: png_deinterlace_bytes<2>(dst_pixels, dst_stride, impl->pixel_converter, tmp_pixel_ptr, intptr_t(tmp_bpl), png_pixel_ptr, steps, w, h); break;
      case 24: png_deinterlace_bytes<3>(dst_pixels, dst_stride, impl->pixel_converter, tmp_pixel_ptr, intptr_t(tmp_bpl), png_pixel_ptr, steps, w, h); break;
      case 32: png_deinterlace_bytes<4>(dst_pixels, dst_stride, impl->pixel_converter, tmp_pixel_ptr, intptr_t(tmp_bpl), png_pixel_ptr, steps, w, h); break;
    }
  }
  else {
    BL_ASSERT(steps[0].width == w);
    BL_ASSERT(steps[0].height == h);

    impl->pixel_converter.convert_rect(dst_pixels, dst_stride, png_pixel_ptr + 1, intptr_t(steps[0].bpl), w, h);
  }

  impl->frame_index++;
  if (impl->is_apng() && impl->frame_index >= impl->image_info.frame_count) {
    // Restart the animation to create a loop.
    impl->frame_index = 0;
    impl->buffer_index = impl->first_fctl_offset;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL png_decoder_read_info(BLImageDecoderCore* self, BLImageInfo* info_out, const uint8_t* data, size_t size) noexcept {
  PngDecoderImpl* impl = self->_impl<PngDecoderImpl>();
  BLResult result = impl->last_result;

  if (impl->buffer_index == 0u && result == BL_SUCCESS) {
    result = png_decoder_read_info_internal(impl, data, size);
    if (result != BL_SUCCESS) {
      impl->last_result = result;
    }
  }

  if (info_out) {
    memcpy(info_out, &impl->image_info, sizeof(BLImageInfo));
  }

  return result;
}

static BLResult BL_CDECL png_decoder_read_frame(BLImageDecoderCore* self, BLImageCore* image_out, const uint8_t* data, size_t size) noexcept {
  BL_PROPAGATE(png_decoder_read_info(self, nullptr, data, size));

  PngDecoderImpl* impl = self->_impl<PngDecoderImpl>();
  if (impl->frame_index == 0u && impl->first_fctl_offset == 0u) {
    BLResult result = png_decoder_read_important_chunks(impl, data, size);
    if (result != BL_SUCCESS) {
      impl->last_result = result;
      return result;
    }
  }
  else if (!impl->is_apng()) {
    return bl_make_error(BL_ERROR_NO_MORE_DATA);
  }

  {
    BLResult result = png_decoder_read_pixel_data(impl, static_cast<BLImage*>(image_out), data, size);
    if (result != BL_SUCCESS) {
      impl->last_result = result;
      return result;
    }
    return BL_SUCCESS;
  }
}

static BLResult BL_CDECL png_decoder_create_impl(BLImageDecoderCore* self) noexcept {
  BL_PROPAGATE(ObjectLayer::alloc_virt_impl_t<PngDecoderImpl>(self, &png_decoder_virt));
  PngDecoderImpl* impl = self->_d.impl<PngDecoderImpl>();
  impl->ctor(&png_codec_object);
  return png_decoder_restart(self);
}

static BLResult BL_CDECL png_decoder_destroy(BLObjectCore* self) noexcept {
  PngDecoderImpl* impl = self->_impl<PngDecoderImpl>();
  impl->dtor();
  return ObjectLayer::dealloc_impl(impl);
}

// Codecs - Png - Encoder - Output Buffer
// ======================================

class PngOutputBuffer {
public:
  uint8_t* _data = nullptr;
  uint8_t* _ptr = nullptr;
  uint8_t* _end = nullptr;

  BL_INLINE PngOutputBuffer() noexcept {}

  BL_INLINE PngOutputBuffer(uint8_t* data, size_t size) noexcept
    : _data(data),
      _ptr(data),
      _end(data + size) {}

  BL_INLINE uint8_t* ptr() const noexcept { return _ptr; }
  BL_INLINE uint8_t* end() const noexcept { return _end; }

  BL_INLINE size_t index() const noexcept { return axl::ptr_byte_diff(_data, _ptr); }
  BL_INLINE size_t capacity() const noexcept { return axl::ptr_byte_diff(_data, _end); }
  BL_INLINE size_t remaining_size() const noexcept { return axl::ptr_byte_diff(_ptr, _end); }

  BL_INLINE void reset() noexcept {
    _data = nullptr;
    _ptr = nullptr;
    _end = nullptr;
  }

  BL_INLINE void reset(uint8_t* data, size_t size) noexcept {
    _data = data;
    _ptr = data;
    _end = data + size;
  }

  BL_INLINE void append_byte(uint8_t value) noexcept {
    BL_ASSERT(remaining_size() >= 1);

    *_ptr++ = value;
  }

  BL_INLINE void append_u16(uint16_t value) noexcept {
    BL_ASSERT(remaining_size() >= 2);

    axl::storeu_u16(_ptr, value);
    _ptr += 2;
  }

  BL_INLINE void append_u16_le(uint16_t value) noexcept {
    BL_ASSERT(remaining_size() >= 2);

    axl::storeu_u16_le(_ptr, value);
    _ptr += 2;
  }

  BL_INLINE void append_u16_be(uint16_t value) noexcept {
    BL_ASSERT(remaining_size() >= 2);

    axl::storeu_u16_be(_ptr, value);
    _ptr += 2;
  }

  BL_INLINE void append_u32(uint32_t value) noexcept {
    BL_ASSERT(remaining_size() >= 4);

    axl::storeu_u32(_ptr, value);
    _ptr += 4;
  }

  BL_INLINE void append_u32_le(uint32_t value) noexcept {
    BL_ASSERT(remaining_size() >= 4);

    axl::storeu_u32_le(_ptr, value);
    _ptr += 4;
  }

  BL_INLINE void append_u32_be(uint32_t value) noexcept {
    BL_ASSERT(remaining_size() >= 4);

    axl::storeu_u32_be(_ptr, value);
    _ptr += 4;
  }

  BL_INLINE void append_u64(uint64_t value) noexcept {
    BL_ASSERT(remaining_size() >= 4);

    axl::storeu_u64(_ptr, value);
    _ptr += 8;
  }

  BL_INLINE void append_u64_le(uint64_t value) noexcept {
    BL_ASSERT(remaining_size() >= 4);

    axl::storeu_u64_le(_ptr, value);
    _ptr += 8;
  }

  BL_INLINE void append_u64_be(uint64_t value) noexcept {
    BL_ASSERT(remaining_size() >= 4);

    axl::storeu_u64_be(_ptr, value);
    _ptr += 8;
  }

  BL_INLINE void append_data(const uint8_t* data, size_t size) noexcept {
    BL_ASSERT(remaining_size() >= size);

    memcpy(_ptr, data, size);
    _ptr += size;
  }
};

// Codecs - Png - Encoder - Chunk Writer
// =====================================

class PngChunkWriter {
public:
  uint8_t* chunk_data = nullptr;

  BL_INLINE void start(PngOutputBuffer& output, uint32_t tag) noexcept {
    chunk_data = output.ptr();
    output.append_u32_be(0);
    output.append_u32_be(tag);
  }

  BL_INLINE void done(PngOutputBuffer& output) noexcept {
    const uint8_t* start = chunk_data + 8;
    size_t chunk_length = axl::ptr_byte_diff(start, output.ptr());

    // PNG Specification: CRC is calculated on the preceding bytes in the chunk, including
    // the chunk type code and chunk data fields, but not including the length field.
    axl::storeu_u32_be(chunk_data, uint32_t(chunk_length));
    output.append_u32_be(Compression::Checksum::crc32(start - 4, chunk_length + 4));
  }
};

// Codecs - Png - Encoder - API
// ============================

static BLResult BL_CDECL png_encoder_restart(BLImageEncoderCore* self) noexcept {
  PngEncoderImpl* impl = self->_impl<PngEncoderImpl>();
  impl->last_result = BL_SUCCESS;
  impl->frame_index = 0;
  impl->buffer_index = 0;
  impl->compression_level = 6;
  impl->filter_type = static_cast<uint8_t>(kFilterTypeDefault);
  return BL_SUCCESS;
}

static BLResult BL_CDECL png_encoder_get_property(const BLObjectCore* self, const char* name, size_t name_size, BLVarCore* value_out) noexcept {
  const PngEncoderImpl* impl = self->_impl<const PngEncoderImpl>();

  if (bl_match_property_literal(name, name_size, "compression")) {
    return bl_var_assign_uint64(value_out, impl->compression_level);
  }

  if (bl_match_property_literal(name, name_size, "filter")) {
    return bl_var_assign_uint64(value_out, impl->filter_type);
  }

  return bl_object_impl_get_property(self, name, name_size, value_out);
}

static BLResult BL_CDECL png_encoder_set_property(BLObjectCore* self, const char* name, size_t name_size, const BLVarCore* value) noexcept {
  PngEncoderImpl* impl = self->_impl<PngEncoderImpl>();

  if (bl_match_property_literal(name, name_size, "compression")) {
    uint64_t v;
    BL_PROPAGATE(bl_var_to_uint64(value, &v));
    impl->compression_level = uint8_t(bl_min<uint64_t>(v, 12));
    return BL_SUCCESS;
  }

  if (bl_match_property_literal(name, name_size, "filter")) {
    uint64_t v;
    BL_PROPAGATE(bl_var_to_uint64(value, &v));

    if (v > kFilterTypeDefault) {
      return bl_make_error(BL_ERROR_INVALID_VALUE);
    }

    impl->filter_type = static_cast<uint8_t>(v);
    return BL_SUCCESS;
  }

  return bl_object_impl_set_property(self, name, name_size, value);
}

static BLResult BL_CDECL png_encoder_write_frame(BLImageEncoderCore* self, BLArrayCore* dst, const BLImageCore* image) noexcept {
  PngEncoderImpl* impl = self->_impl<PngEncoderImpl>();
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

  // Setup target PNG format and other information.
  BLFormatInfo png_format_info {};
  uint8_t png_bit_depth = 0;
  uint8_t png_color_type = 0;

  switch (format) {
    case BL_FORMAT_PRGB32:
      png_format_info.depth = 32;
      png_format_info.flags = BLFormatFlags(BL_FORMAT_FLAG_RGBA | BL_FORMAT_FLAG_BE);
      png_format_info.set_sizes(8, 8, 8, 8);
      png_format_info.set_shifts(24, 16, 8, 0);
      png_bit_depth = 8;
      png_color_type = 6;
      break;

    case BL_FORMAT_XRGB32:
      png_format_info.depth = 24;
      png_format_info.flags = BLFormatFlags(BL_FORMAT_FLAG_RGB | BL_FORMAT_FLAG_BE);
      png_format_info.set_sizes(8, 8, 8, 0);
      png_format_info.set_shifts(16, 8, 0, 0);
      png_bit_depth = 8;
      png_color_type = 2;
      break;

    case BL_FORMAT_A8:
      png_format_info.depth = 8;
      png_format_info.flags = BL_FORMAT_FLAG_ALPHA;
      png_format_info.set_sizes(0, 0, 0, 8);
      png_format_info.set_shifts(0, 0, 0, 0);
      png_bit_depth = 8;
      png_color_type = 0;
      break;
  }

  // Setup pixel converter and convert the input image to PNG representation.
  BLPixelConverter pc;
  BL_PROPAGATE(pc.create(png_format_info, bl_format_info[format]));

  size_t uncompressed_stride = size_t(w) * (png_format_info.depth / 8u) + 1u;
  if (BL_UNLIKELY(uncompressed_stride > UINT32_MAX || size_t(h) > SIZE_MAX / uncompressed_stride)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }
  size_t uncompressed_data_size = uncompressed_stride * h;

  ScopedBuffer uncompressed_buffer;
  uint8_t* uncompressed_data = static_cast<uint8_t*>(uncompressed_buffer.alloc(uncompressed_data_size));

  if (BL_UNLIKELY(!uncompressed_data)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  BL_PROPAGATE(pc.convert_rect(uncompressed_data + 1, intptr_t(uncompressed_stride), image_data.pixel_data, image_data.stride, w, h));
  uint32_t bytes_per_pixel = png_format_info.depth / 8u;
  bool use_fast_encoder = impl->compression_level == 1u;

  ScopedBuffer repeat_masks_buffer;
  uint64_t* repeat_masks = nullptr;

  if (use_fast_encoder && uncompressed_data_size > 128u) {
    size_t mask_stride = FastEncoder::repeat_mask_stride(uint32_t(uncompressed_stride));
    if (BL_UNLIKELY(mask_stride > SIZE_MAX / sizeof(uint64_t) / h)) {
      return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
    }

    repeat_masks = static_cast<uint64_t*>(repeat_masks_buffer.alloc(mask_stride * h * sizeof(uint64_t)));
    if (BL_UNLIKELY(!repeat_masks)) {
      return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
    }

    BL_PROPAGATE(Ops::func_table.filter_with_masks[bytes_per_pixel](
      uncompressed_data, bytes_per_pixel, uint32_t(uncompressed_stride), h, impl->filter_type, repeat_masks));
  }
  else {
    BL_PROPAGATE(Ops::func_table.filter[bytes_per_pixel](
      uncompressed_data, bytes_per_pixel, uint32_t(uncompressed_stride), h, impl->filter_type));
  }

  // Level one uses the residual/run encoder. Other levels retain the general
  // Deflate compressor and do not allocate or generate repeat masks.
  Compression::Deflate::Encoder deflate_encoder;
  if (!use_fast_encoder) {
    BL_PROPAGATE(deflate_encoder.init(Compression::Deflate::FormatType::kZlib, impl->compression_level));
  }

  // Create PNG file.
  size_t output_worst_case_size = use_fast_encoder
    ? FastEncoder::minimum_output_buffer_size(uncompressed_data_size)
    : deflate_encoder.minimum_output_buffer_size(uncompressed_data_size);

  size_t ihdr_size = kPngChunkBaseSize + kPngChunkDataSize_IHDR;
  size_t iend_size = kPngChunkBaseSize;

  size_t overhead_size = kPngSignatureSize + ihdr_size + kPngChunkBaseSize + iend_size;
  if (BL_UNLIKELY(output_worst_case_size > SIZE_MAX - overhead_size)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }
  size_t reserve_bytes = overhead_size + output_worst_case_size;
  size_t original_size = buf.size();
  uint8_t* output_data;
  BL_PROPAGATE(buf.append_uninitialized(reserve_bytes, &output_data));

  // Prepare output buffer and chunk writer.
  PngOutputBuffer output(output_data, reserve_bytes);
  PngChunkWriter chunk;

  // Write PNG signature.
  output.append_data(png_signature_bytes, kPngSignatureSize);

  // Write IHDR chunk.
  chunk.start(output, BL_MAKE_TAG('I', 'H', 'D', 'R'));
  output.append_u32_be(w);            // Image width.
  output.append_u32_be(h);            // Image height.
  output.append_byte(png_bit_depth);  // Bit depth (1, 2, 4, 8, 16).
  output.append_byte(png_color_type); // Color type (0, 2, 3, 4, 6).
  output.append_byte(0u);             // Compression method, must be zero.
  output.append_byte(0u);             // Filter method, must be zero.
  output.append_byte(0u);             // Interlace method (0 == no interlacing).
  chunk.done(output);

  // Write IDAT chunk.
  chunk.start(output, BL_MAKE_TAG('I', 'D', 'A', 'T'));
  if (use_fast_encoder) {
    size_t compressed_size = 0;
    BLResult result = FastEncoder::encode(
      output.ptr(), output_worst_case_size, &compressed_size, uncompressed_data,
      uint32_t(uncompressed_stride), h, Ops::func_table.encode_rows_fast, repeat_masks, Ops::func_table.write_header_fast);

    if (BL_UNLIKELY(result != BL_SUCCESS)) {
      buf.truncate(original_size);
      return result;
    }
    output._ptr += compressed_size;
  }
  else {
    output._ptr += deflate_encoder.compress_to(output.ptr(), output.remaining_size(), uncompressed_data, uncompressed_data_size);
  }
  chunk.done(output);

  // Write IEND chunk.
  chunk.start(output, BL_MAKE_TAG('I', 'E', 'N', 'D'));
  chunk.done(output);

  buf._set_size_from_end_ptr_unsafe(output.ptr());
  return BL_SUCCESS;
}

static BLResult png_encoder_create_impl(BLImageEncoderCore* self) noexcept {
  BL_PROPAGATE(ObjectLayer::alloc_virt_impl_t<PngEncoderImpl>(self, &png_encoder_virt));
  PngEncoderImpl* impl = self->_d.impl<PngEncoderImpl>();
  impl->ctor(&png_codec_object);
  return png_encoder_restart(self);
}

static BLResult BL_CDECL png_encoder_destroy(BLObjectCore* self) noexcept {
  PngEncoderImpl* impl = self->_impl<PngEncoderImpl>();
  impl->dtor();
  return ObjectLayer::dealloc_impl(impl);
}

// Codecs - Png - Codec API
// ========================

static BLResult BL_CDECL png_codec_destroy(BLObjectCore* self) noexcept {
  // Built-in codecs are never destroyed.
  axl::maybe_unused(self);
  return BL_SUCCESS;
}

static uint32_t BL_CDECL png_codec_inspect_data(const BLImageCodecCore* self, const uint8_t* data, size_t size) noexcept {
  axl::maybe_unused(self);

  // Minimum PNG size and signature.
  if (size < kPngSignatureSize || memcmp(data, png_signature_bytes, kPngSignatureSize) != 0) {
    return 0;
  }

  return 100;
}

static BLResult BL_CDECL png_codec_create_decoder(const BLImageCodecCore* self, BLImageDecoderCore* dst) noexcept {
  axl::maybe_unused(self);

  BLImageDecoderCore tmp;
  BL_PROPAGATE(png_decoder_create_impl(&tmp));
  return bl_image_decoder_assign_move(dst, &tmp);
}

static BLResult BL_CDECL png_codec_create_encoder(const BLImageCodecCore* self, BLImageEncoderCore* dst) noexcept {
  axl::maybe_unused(self);

  BLImageEncoderCore tmp;
  BL_PROPAGATE(png_encoder_create_impl(&tmp));
  return bl_image_encoder_assign_move(dst, &tmp);
}

// Codecs - Png - Runtime Registration
// ===================================

void png_codec_on_init(BLRuntimeContext* rt, BLArray<BLImageCodec>* codecs) noexcept {
  Ops::init_func_table(rt);

  // Initialize PNG codec.
  png_codec.virt.base.destroy = png_codec_destroy;
  png_codec.virt.base.get_property = bl_object_impl_get_property;
  png_codec.virt.base.set_property = bl_object_impl_set_property;
  png_codec.virt.inspect_data = png_codec_inspect_data;
  png_codec.virt.create_decoder = png_codec_create_decoder;
  png_codec.virt.create_encoder = png_codec_create_encoder;

  png_codec.impl->ctor();
  png_codec.impl->features =
    BL_IMAGE_CODEC_FEATURE_READ     |
    BL_IMAGE_CODEC_FEATURE_WRITE    |
    BL_IMAGE_CODEC_FEATURE_LOSSLESS ;
  png_codec.impl->name.dcast().assign("PNG");
  png_codec.impl->vendor.dcast().assign("Blend2D");
  png_codec.impl->mime_type.dcast().assign("image/png");
  png_codec.impl->extensions.dcast().assign("png");
  ObjectLayer::init_virt(&png_codec_object, BL_OBJECT_TYPE_IMAGE_CODEC, 0u, &png_codec.impl, &png_codec.virt);

  // Initialize PNG decoder virtual functions.
  png_decoder_virt.base.destroy = png_decoder_destroy;
  png_decoder_virt.base.get_property = bl_object_impl_get_property;
  png_decoder_virt.base.set_property = bl_object_impl_set_property;
  png_decoder_virt.restart = png_decoder_restart;
  png_decoder_virt.read_info = png_decoder_read_info;
  png_decoder_virt.read_frame = png_decoder_read_frame;

  // Initialize PNG encoder virtual functions.
  png_encoder_virt.base.destroy = png_encoder_destroy;
  png_encoder_virt.base.get_property = png_encoder_get_property;
  png_encoder_virt.base.set_property = png_encoder_set_property;
  png_encoder_virt.restart = png_encoder_restart;
  png_encoder_virt.write_frame = png_encoder_write_frame;

  codecs->append(png_codec_object.dcast());
}

} // {bl::Png}

#endif // !BL_BUILD_NO_CODECS
