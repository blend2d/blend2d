// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib


#include <blend2d/core/api_build_p.h>
#if !defined(BL_BUILD_NO_CODECS)

#include <blend2d/axl/scopedbuffer_p.h>
#include <blend2d/codec/png_encoder_fast_p.h>
#include <blend2d/compression/checksum_p.h>

namespace bl::Png::FastEncoder {
namespace {

static constexpr uint8_t kBitReverseNibble[16] = {
  0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15
};

static constexpr uint8_t kLengthExtraBits[29] = {
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
  2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};

static constexpr uint16_t kLengthBase[29] = {
  3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
  31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};

static BL_INLINE uint16_t reverse_bits(uint32_t count, uint32_t bits) noexcept {
  uint32_t reversed = (uint32_t(kBitReverseNibble[(bits & 15u)      ]) << 12u) |
                      (uint32_t(kBitReverseNibble[(bits >> 4u) & 15u]) << 8u) |
                      (uint32_t(kBitReverseNibble[(bits >> 8u) & 15u]) << 4u) |
                      (uint32_t(kBitReverseNibble[(bits >> 12u)     ]));
  return uint16_t(reversed >> (16u - count));
}

// Solve the small length-limited Huffman problem on a fixed Kraft grid.
// `scratch` is reused for both the 48-symbol prefix and 29-symbol suffix trees.
static void compute_code_lengths(uint64_t* scratch, const uint64_t* frequencies,
                                 uint32_t count, uint8_t* min_lengths,
                                 const uint8_t* max_lengths, uint8_t* lengths) noexcept {
  uint32_t precision = 0;
  uint64_t frequency_sum = 0;
  for (uint32_t i = 0; i < count; i++) {
    BL_ASSERT(frequencies[i] != 0u);
    frequency_sum += frequencies[i];
    min_lengths[i] = bl_max<uint8_t>(min_lengths[i], 1u);
    precision = bl_max<uint32_t>(precision, max_lengths[i]);
  }

  uint32_t capacity = 1u << precision;
  uint32_t stride = capacity + 1u;
  uint64_t infinity = frequency_sum * precision;
  size_t cells = size_t(count + 1u) * stride;

  for (size_t i = 0; i < cells; i++) {
    scratch[i] = infinity;
  }

  scratch[0] = 0u;

  for (uint32_t symbol = 0; symbol < count; symbol++) {
    const uint64_t* previous = scratch + size_t(symbol) * stride;
    uint64_t* next = scratch + size_t(symbol + 1u) * stride;

    for (uint32_t bits = min_lengths[symbol]; bits <= max_lengths[symbol]; bits++) {
      uint32_t delta = 1u << (precision - bits);
      uint64_t cost = frequencies[symbol] * bits;
      for (uint32_t offset = 0; offset <= capacity - delta; offset++) {
        next[offset + delta] = bl_min(next[offset + delta], previous[offset] + cost);
      }
    }
  }

  uint32_t offset = capacity;
  for (uint32_t symbol = count; symbol;) {
    symbol--;
    const uint64_t* previous = scratch + size_t(symbol) * stride;
    const uint64_t* next = previous + stride;

    for (uint32_t bits = min_lengths[symbol]; bits <= max_lengths[symbol]; bits++) {
      uint32_t delta = 1u << (precision - bits);
      if (delta <= offset && next[offset] == previous[offset - delta] + frequencies[symbol] * bits) {
        lengths[symbol] = uint8_t(bits);
        offset -= delta;
        break;
      }
    }
  }

  BL_ASSERT(offset == 0u);
}

static BLResult make_huffman_table(HuffmanTable& table, const uint8_t* src, size_t bpl, uint32_t h, const uint64_t* masks) noexcept {
  for (size_t i = 0; i < 29u; i++) {
    uint32_t extra_count = 1u << kLengthExtraBits[i];
    for (size_t extra = 0; extra < extra_count; extra++) {
      table.lz77_length_sym[kLengthBase[i] + extra] = uint16_t(257u + i);
    }
  }

  uint64_t frequencies[286]{};
  uint32_t y0 = uint32_t(uint64_t(h) * 126u / 256u);
  uint32_t y1 = bl_max(uint32_t(uint64_t(h) * 130u / 256u), 1u);

  // fpnge originally uses the first sampled row to warm up its predictor. Retaining
  // that omission here matches its sampled tree for an explicitly selected filter.
  if (y0) {
    y0++;
  }

  for (uint32_t y = y0; y < y1; y++) {
    const uint64_t* row_masks = masks ? masks + size_t(y) * repeat_mask_stride(bpl) : nullptr;
    process_row_repeats(src + size_t(y) * bpl + 1u, bpl - 1u, row_masks,
      [&](const uint8_t* data, size_t count) noexcept {
        for (size_t i = 0; i < count; i++) {
          frequencies[data[i]]++;
        }
      },
      [&](const uint8_t* data, size_t length) noexcept {
        frequencies[data[0]]++;
        for_each_zero_match(length, [&](size_t match_length, size_t count) noexcept {
          frequencies[table.lz77_length_sym[match_length]] += count;
        });
      }
    );
  }

  // A small prior keeps every literal available even when sampling misses it.
  static constexpr uint8_t kLowPrior[16] = { 113, 54, 28, 18, 12, 9, 7, 6, 5, 4, 3, 3, 2, 2, 2, 2 };
  static constexpr uint8_t kHighPrior[16] = { 2, 2, 2, 2, 2, 3, 3, 4, 5, 6, 7, 9, 12, 18, 29, 54 };

  for (uint32_t i = 0; i < 286u; i++) {
    frequencies[i] += i < 16u ? kLowPrior[i] : i >= 240u && i < 256u ? kHighPrior[i - 240u] : 1u;
  }

  uint64_t prefix_frequencies[48]{};
  uint8_t prefix_min[48]{};
  uint8_t prefix_max[48];
  uint8_t prefix_lengths[48]{};
  memset(prefix_max, 8, sizeof(prefix_max));

  for (uint32_t i = 0; i < 16u; i++) {
    prefix_frequencies[i] = frequencies[i];
    prefix_frequencies[30u + i] = frequencies[240u + i];
  }

  // Fourteen equally sized literal groups plus one length-code subtree occupy
  // complete eight-bit prefixes. Their suffixes are handled separately.
  for (uint32_t i = 16u; i < 30u; i++) {
    prefix_frequencies[i] = 1u;
    prefix_min[i] = 8u;
  }

  prefix_frequencies[46] = 1u;
  prefix_min[46] = 8u;
  prefix_frequencies[47] = frequencies[285];

  ScopedBuffer scratch_buffer;
  constexpr size_t kScratchCells = 49u * 257u;
  uint64_t* scratch = static_cast<uint64_t*>(scratch_buffer.alloc(kScratchCells * sizeof(uint64_t)));

  if (BL_UNLIKELY(!scratch)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  compute_code_lengths(scratch, prefix_frequencies, 48u, prefix_min, prefix_max, prefix_lengths);

  uint8_t suffix_min[29];
  uint8_t suffix_max[29];
  uint8_t suffix_lengths[29]{};

  memset(suffix_min, 4, sizeof(suffix_min));
  memset(suffix_max, 7, sizeof(suffix_max));
  compute_code_lengths(scratch, frequencies + 256u, 29u, suffix_min, suffix_max, suffix_lengths);

  for (uint32_t i = 0; i < 16u; i++) {
    table.nbits[i] = prefix_lengths[i];
    table.nbits[240u + i] = prefix_lengths[30u + i];
  }

  for (uint32_t i = 16u; i < 240u; i++) {
    table.nbits[i] = uint8_t(prefix_lengths[16u + (i - 16u) / 16u] + 4u);
  }

  for (uint32_t i = 0; i < 29u; i++) {
    table.nbits[256u + i] = uint8_t(prefix_lengths[46] + suffix_lengths[i]);
  }

  table.nbits[285] = prefix_lengths[47];

  uint16_t length_counts[16]{};
  uint16_t next_code[16]{};

  for (uint32_t i = 0; i < 286u; i++) {
    BL_ASSERT(table.nbits[i] >= 1u && table.nbits[i] <= 15u);
    length_counts[table.nbits[i]]++;
  }

  uint32_t code = 0;
  for (uint32_t i = 1; i < 16u; i++) {
    code = (code + length_counts[i - 1u]) << 1u;
    next_code[i] = uint16_t(code);
  }

  for (uint32_t i = 0; i < 286u; i++) {
    table.bits[i] = reverse_bits(table.nbits[i], next_code[table.nbits[i]]++);
  }

  table.mid_nbits = table.nbits[16];
  BL_ASSERT(table.mid_nbits == 12u);
  memset(table.mid_lowbits, 0, sizeof(table.mid_lowbits));

  for (uint32_t i = 0; i < 16u; i++) {
    table.first16_nbits[i] = table.nbits[i];
    table.first16_bits[i] = uint8_t(table.bits[i]);
    table.last16_nbits[i] = table.nbits[240u + i];
    table.last16_bits[i] = uint8_t(table.bits[240u + i]);
  }

  for (uint32_t i = 16u; i < 240u; i += 16u) {
    table.mid_lowbits[i / 16u] = uint8_t(table.bits[i] & 255u);
  }

  for (uint32_t i = 0; i < 29u; i++) {
    uint32_t extra_count = 1u << kLengthExtraBits[i];
    for (uint32_t extra = 0; extra < extra_count; extra++) {
      uint32_t length = kLengthBase[i] + extra;
      table.lz77_length_nbits[length] = table.nbits[257u + i] + kLengthExtraBits[i];
      table.lz77_length_bits[length] = table.bits[257u + i] | (extra << table.nbits[257u + i]);
    }
  }

  return BL_SUCCESS;
}

static void write_huffman_header(FastBitWriter& writer, const HuffmanTable& table) noexcept {
  static constexpr uint8_t kCodeLengthOrder[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

  writer.write(14u, 29u | (15u << 10u)); // All literals/lengths, one distance code.

  uint64_t precode = 0;
  for (uint32_t i = 0; i < 19u; i++) {
    precode |= uint64_t(kCodeLengthOrder[i] < 16u ? 4u : 0u) << (i * 3u);
  }

  writer.write(57u, precode);
  uint32_t i = 0;

  while (i < 286u) {
    uint32_t count = bl_min<uint32_t>(286u - i, 16u);
    uint64_t bits = 0;

    for (uint32_t j = 0; j < count; j++) {
      bits |= uint64_t(kBitReverseNibble[table.nbits[i + j]]) << (j * 4u);
    }

    if (count == 16u) {
      writer.write_full_word(bits);
    }
    else {
      writer.write(count * 4u, bits);
    }

    i += count;
  }
  writer.write(4u, 8u); // The single distance symbol has a one-bit code.
}

static size_t stored_size(size_t input_size) noexcept {
  size_t blocks = input_size / 65535u + size_t(input_size % 65535u != 0u);
  return input_size + blocks * 5u + 6u;
}

static size_t encode_stored(uint8_t* dst, const uint8_t* src, size_t input_size, uint32_t adler32) noexcept {
  uint8_t* p = dst;
  *p++ = 8u;
  *p++ = 29u;

  size_t remain = input_size;
  while (remain) {
    uint32_t size = uint32_t(bl_min<size_t>(remain, 65535u));
    *p++ = uint8_t(remain == size);
    axl::storeu_u16_le(p, uint16_t(size));
    axl::storeu_u16_le(p + 2u, uint16_t(size ^ 0xFFFFu));
    p += 4u;
    memcpy(p, src, size);
    p += size;
    src += size;
    remain -= size;

  }
  axl::storeu_u32_be(p, adler32);
  return size_t(p + 4u - dst);
}

} // {anonymous}

void BL_CDECL write_header_ref(FastBitWriter& writer, const HuffmanTable& table) noexcept {
  write_huffman_header(writer, table);
}

void build_repeat_masks(const uint8_t* src, size_t bpl, uint32_t h, uint64_t* masks) noexcept {
  size_t stride = repeat_mask_stride(bpl);
  size_t row_size = bpl - 1u;

  for (uint32_t y = 0; y < h; y++) {
    const uint8_t* row = src + size_t(y) * bpl + 1u;

    for (size_t word = 0; word < stride; word++) {
      size_t x = word * 64u;
      uint32_t count = uint32_t(bl_min<size_t>(row_size - x, 64u));
      uint64_t bits = 0;
      for (uint32_t i = 0; i < count && x + i + 1u < row_size; i++) {
        bits |= uint64_t(row[x + i] == row[x + i + 1u]) << i;
      }
      masks[size_t(y) * stride + word] = bits;
    }
  }
}

void BL_CDECL encode_rows_ref(
  FastBitWriter& writer, const HuffmanTable& table, const uint8_t* data, size_t bpl, uint32_t h, const uint64_t* masks
) noexcept {
  size_t stride = repeat_mask_stride(bpl);

  for (uint32_t y = 0; y < h; y++, data += bpl) {
    writer.write(table.nbits[data[0]], table.bits[data[0]]);
    const uint8_t* row = data + 1u;
    const uint8_t* pending = row;
    const uint64_t* row_masks = masks ? masks + size_t(y) * stride : nullptr;

    process_row_repeats(row, bpl - 1u, row_masks,
      [&](const uint8_t* data, size_t count) noexcept { axl::maybe_unused(data, count); },
      [&](const uint8_t* run, size_t length) noexcept {
        if (!repeat_is_profitable(table, run[0], length)) {
          return;
        }
        write_literals_ref(writer, table, pending, uint32_t(run - pending));
        write_repeat_run(writer, table, run[0], length);
        pending = run + length;
      });
    write_literals_ref(writer, table, pending, uint32_t(row + bpl - 1u - pending));
  }
}

size_t minimum_output_buffer_size(size_t input_size) noexcept {
  return input_size <= (SIZE_MAX - 1024u) / 2u ? input_size * 2u + 1024u : SIZE_MAX;
}

BLResult encode(uint8_t* dst, size_t capacity, size_t* written, const uint8_t* src,
                size_t bpl, uint32_t h, EncodeRowsFunc encode_rows, const uint64_t* masks,
                WriteHeaderFunc write_header
) noexcept {
  if (written) {
    *written = 0u;
  }

  if (BL_UNLIKELY(!dst || !src || !written || !encode_rows || bpl <= 1u || h == 0u || size_t(h) > SIZE_MAX / bpl)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  size_t input_size = bpl * h;
  size_t minimum_size = minimum_output_buffer_size(input_size);

  if (BL_UNLIKELY(minimum_size == SIZE_MAX || capacity < minimum_size)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  uint32_t adler32 = Compression::Checksum::adler32(src, input_size);
  // Even an empty dynamic tree occupies about 152 bytes. Tiny images are both
  // quicker and smaller with stored blocks, without allocating Huffman scratch.
  if (input_size <= 128u) {
    *written = encode_stored(dst, src, input_size, adler32);
    return BL_SUCCESS;
  }

  HuffmanTable table{};
  BL_PROPAGATE(make_huffman_table(table, src, bpl, h, masks));

  FastBitWriter writer(dst, capacity);
  writer.write(19u, 8u | (29u << 8u) | (5u << 16u)); // Zlib and one final dynamic block.

  (write_header ? write_header : write_header_ref)(writer, table);

  encode_rows(writer, table, src, bpl, h, masks);
  writer.write(table.nbits[256], table.bits[256]);
  writer.pad_to_byte();
  writer.write(32u, axl::byteswap32(adler32));

  if (BL_UNLIKELY(writer.overflow)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  *written = writer.bytes_written;
  if (*written > stored_size(input_size)) {
    *written = encode_stored(dst, src, input_size, adler32);
  }

  return BL_SUCCESS;
}

} // {bl::Png::FastEncoder}

#endif // !BL_BUILD_NO_CODECS
