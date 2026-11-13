// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#if !defined(BL_BUILD_NO_CODECS)

#include <blend2d/codec/jpeg_huffman_p.h>

namespace bl::Jpeg {

// Codecs - Jpeg - Huffman - BuildHuffmanTable
// ===========================================

static BLResult jpeg_build_huffman_table(JpegDecoderHuffmanTable* table, const uint8_t* data, size_t data_size, size_t* bytes_consumed) noexcept {
  uint32_t i;
  uint32_t k;
  uint32_t n = 0;

  if (BL_UNLIKELY(data_size < 16))
    return bl_make_error(BL_ERROR_INVALID_DATA);

  for (i = 0; i < 16; i++)
    n += uint32_t(data[i]);

  if (BL_UNLIKELY(n > 256 || n + 16 > data_size))
    return bl_make_error(BL_ERROR_INVALID_DATA);

  table->max_code[0] = 0;            // Not used.
  table->max_code[17] = 0xFFFFFFFFu; // Sentinel.
  table->delta[0] = 0;

  // Build size list for each symbol.
  i = 0;
  k = 0;
  do {
    uint32_t c = data[i++];
    for (uint32_t j = 0; j < c; j++)
      table->size[k++] = uint8_t(i);
  } while (i < 16);
  table->size[k] = 0;

  // Compute actual symbols.
  {
    uint32_t code = 0;
    for (i = 1, k = 0; i <= 16; i++, code <<= 1) {
      // Compute `delta` to add to code to compute symbol id.
      table->delta[i] = int32_t(k) - int32_t(code);

      if (table->size[k] == i) {
        while (table->size[k] == i)
          table->code[k++] = uint16_t(code++);

        if (code - 1 >= (1u << i))
          return bl_make_error(BL_ERROR_INVALID_DATA);
      }

      // Compute largest code + 1 for this size, pre-shifted as needed later.
      table->max_code[i] = code << (16u - i);
    }
  }

  // Copy values from huffman data and zero the undefined ones for sanity.
  memcpy(table->values, data + 16, n);
  memset(table->values + n, 0, 256 - n);

  // Build acceleration table; 255 is a flag for not-accelerated.
  memset(table->accel, 255, jpeg_huffman_accel_size);
  for (i = 0; i < k; i++) {
    uint32_t s = table->size[i];

    if (s <= jpeg_huffman_accel_bits) {
      s = jpeg_huffman_accel_bits - s;

      uint32_t code = uint32_t(table->code[i]) << s;
      uint32_t c_max = code + (1u << s);

      while (code < c_max)
        table->accel[code++] = uint8_t(i);
    }
  }

  *bytes_consumed = 16 + n;
  return BL_SUCCESS;
}

BLResult jpeg_build_huffman_dc(JpegDecoderHuffmanDCTable* table, const uint8_t* data, size_t data_size, size_t* bytes_consumed) noexcept {
  return jpeg_build_huffman_table(table, data, data_size, bytes_consumed);
}

BLResult jpeg_build_huffman_ac(JpegDecoderHuffmanACTable* table, const uint8_t* data, size_t data_size, size_t* bytes_consumed) noexcept {
  BL_PROPAGATE(jpeg_build_huffman_table(table, data, data_size, bytes_consumed));

  // Build an AC specific acceleration table.
  for (uint32_t i = 0; i < jpeg_huffman_accel_size; i++) {
    uint32_t accel = table->accel[i];
    int32_t ac = 0;

    if (accel < 255) {
      uint32_t val = table->values[accel];
      uint32_t size = table->size[accel];

      uint32_t run = val >> 4;
      uint32_t mag = val & 15;

      if (mag != 0 && size + mag <= jpeg_huffman_accel_bits) {
        // Magnitude code followed by receive/extend code.
        int32_t k = ((i << size) & jpeg_huffman_accel_mask) >> (jpeg_huffman_accel_bits - mag);
        int32_t m = axl::shl(1, mag - 1);

        if (k < m)
          k += axl::shl(-1, mag) + 1;

        // If the result is small enough, we can fit it in ac_accel table.
        if (k >= -128 && k <= 127)
          ac = int32_t(axl::shl(k, 8)) + int32_t(axl::shl(run, 4) + size + mag);
      }
    }

    table->ac_accel[i] = int16_t(ac);
  }

  return BL_SUCCESS;
}

} // {bl::Jpeg}

#endif // !BL_BUILD_NO_CODECS
