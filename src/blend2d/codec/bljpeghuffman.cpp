// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../codec/bljpeghuffman_p.h"

// ============================================================================
// [BLJpegDecoder - BuildHuffmanTable]
// ============================================================================

static BLResult blJpegDecoderBuildHuffmanTable(BLJpegDecoderHuffmanTable* table, const uint8_t* data, size_t dataSize, size_t* bytesConsumed) noexcept {
  uint32_t i;
  uint32_t k;
  uint32_t n = 0;

  if (BL_UNLIKELY(dataSize < 16))
    return blTraceError(BL_ERROR_INVALID_DATA);

  for (i = 0; i < 16; i++)
    n += uint32_t(data[i]);

  if (BL_UNLIKELY(n > 256 || n + 16 > dataSize))
    return blTraceError(BL_ERROR_INVALID_DATA);

  table->maxCode[0] = 0;            // Not used.
  table->maxCode[17] = 0xFFFFFFFFu; // Sentinel.
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
          return blTraceError(BL_ERROR_INVALID_DATA);
      }

      // Compute largest code + 1 for this size, pre-shifted as needed later.
      table->maxCode[i] = code << (16u - i);
    }
  }

  // Copy values from huffman data and zero the undefined ones for sanity.
  memcpy(table->values, data + 16, n);
  memset(table->values + n, 0, 256 - n);

  // Build acceleration table; 255 is a flag for not-accelerated.
  memset(table->accel, 255, BL_JPEG_DECODER_HUFFMAN_ACCEL_SIZE);
  for (i = 0; i < k; i++) {
    uint32_t s = table->size[i];

    if (s <= BL_JPEG_DECODER_HUFFMAN_ACCEL_BITS) {
      s = BL_JPEG_DECODER_HUFFMAN_ACCEL_BITS - s;

      uint32_t code = uint32_t(table->code[i]) << s;
      uint32_t cMax = code + (1u << s);

      while (code < cMax)
        table->accel[code++] = uint8_t(i);
    }
  }

  *bytesConsumed = 16 + n;
  return BL_SUCCESS;
}

BLResult blJpegDecoderBuildHuffmanDC(BLJpegDecoderHuffmanDCTable* table, const uint8_t* data, size_t dataSize, size_t* bytesConsumed) noexcept {
  return blJpegDecoderBuildHuffmanTable(table, data, dataSize, bytesConsumed);
}

BLResult blJpegDecoderBuildHuffmanAC(BLJpegDecoderHuffmanACTable* table, const uint8_t* data, size_t dataSize, size_t* bytesConsumed) noexcept {
  BL_PROPAGATE(blJpegDecoderBuildHuffmanTable(table, data, dataSize, bytesConsumed));

  // Build an AC specific acceleration table.
  for (uint32_t i = 0; i < BL_JPEG_DECODER_HUFFMAN_ACCEL_SIZE; i++) {
    uint32_t accel = table->accel[i];
    int32_t ac = 0;

    if (accel < 255) {
      uint32_t val = table->values[accel];
      uint32_t size = table->size[accel];

      uint32_t run = val >> 4;
      uint32_t mag = val & 15;

      if (mag != 0 && size + mag <= BL_JPEG_DECODER_HUFFMAN_ACCEL_BITS) {
        // Magnitude code followed by receive/extend code.
        int32_t k = ((i << size) & BL_JPEG_DECODER_HUFFMAN_ACCEL_MASK) >> (BL_JPEG_DECODER_HUFFMAN_ACCEL_BITS - mag);
        int32_t m = blBitShl(1, mag - 1);

        if (k < m)
          k += blBitShl(-1, mag) + 1;

        // If the result is small enough, we can fit it in acAccel table.
        if (k >= -128 && k <= 127)
          ac = int32_t(blBitShl(k, 8)) + int32_t(blBitShl(run, 4)) + size + mag;
      }
    }

    table->acAccel[i] = int16_t(ac);
  }

  return BL_SUCCESS;
}
