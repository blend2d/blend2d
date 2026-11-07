// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// This code was initially inspired in fpnge (https://github.com/veluca93/fpnge), but it
// started drifting from the original ideas. For example to encode RLE we don't care, which
// byte repeats, because we want to do SIMD-RLE encoding bit-mask, which is then used by the
// encoder - the performance advantage is mixed, but the output size is almost always smaller.

#ifndef BLEND2D_CODEC_PNG_ENCODER_FAST_P_H_INCLUDED
#define BLEND2D_CODEC_PNG_ENCODER_FAST_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_ops.h>

//! \cond INTERNAL
//! \addtogroup blend2d_codec_impl
//! \{

namespace bl::Png::FastEncoder {

// The middle fourteen groups of sixteen literals share twelve-bit codes.
// The two groups nearest zero use individual lengths and 16-byte SIMD tables.
struct HuffmanTable {
  uint8_t nbits[286];
  uint16_t bits[286];

  alignas(16) uint8_t first16_nbits[16];
  alignas(16) uint8_t first16_bits[16];
  alignas(16) uint8_t last16_nbits[16];
  alignas(16) uint8_t last16_bits[16];
  alignas(16) uint8_t mid_lowbits[16];
  uint8_t mid_nbits;

  uint32_t lz77_length_nbits[259];
  uint32_t lz77_length_bits[259];
  uint16_t lz77_length_sym[259];
};

// Little-endian Deflate bit writer. Wide stores are used only within the
// caller's capacity; the exact-byte tail also supports guard-page tests.
struct BitWriter {
  uint8_t* data;
  size_t capacity;
  size_t bytes_written;
  uint64_t buffer;
  uint32_t bits_in_buffer;
  bool overflow;

  BL_INLINE BitWriter(uint8_t* dst, size_t size) noexcept
    : data(dst), capacity(size), bytes_written(0), buffer(0), bits_in_buffer(0), overflow(false) {}

  BL_INLINE void write_chunk(uint32_t count, uint64_t bits) noexcept {
    bits &= (uint64_t(1) << count) - 1u;
    buffer |= bits << bits_in_buffer;
    uint32_t total_bits = bits_in_buffer + count;
    uint32_t bytes = total_bits >> 3u;

    if (BL_UNLIKELY(bytes_written > capacity || bytes > capacity - bytes_written)) {
      overflow = true;
      return;
    }

    if (capacity - bytes_written >= 8u) {
      axl::storeu_u64_le(data + bytes_written, buffer);
    }
    else {
      for (uint32_t i = 0; i < bytes; i++) {
        data[bytes_written + i] = uint8_t(buffer >> (i * 8u));
      }
    }

    bytes_written += bytes;
    bits_in_buffer = total_bits & 7u;
    buffer >>= bytes * 8u;
  }

  BL_INLINE void write(uint32_t count, uint64_t bits) noexcept {
    BL_ASSERT(count <= 63u);
    if (!count) {
      return;
    }

    // Four literal codes fit in 48 bits. This split also makes the writer safe
    // for callers that combine up to 63 bits with an existing partial byte.
    if (count + bits_in_buffer > 63u) {
      write_chunk(32u, uint32_t(bits));
      bits >>= 32u;
      count -= 32u;
    }
    write_chunk(count, bits);
  }

  BL_INLINE void pad_to_byte() noexcept {
    if (bits_in_buffer) {
      write(8u - bits_in_buffer, 0u);
    }
  }
};

// Encoder-owned buffers have a checked 2*N+1024 bound. Literal codes are at
// most twelve bits and accepted matches cost less than their literals, leaving
// at least seven writable bytes after every possible encoded end. Keep the
// checked writer above for bounded callers and use clean, overlapping stores
// inside this validated frame allocation.
struct FastBitWriter : public BitWriter {
  BL_INLINE FastBitWriter(uint8_t* dst, size_t size) noexcept : BitWriter(dst, size) {}

  BL_INLINE void write_chunk(uint32_t count, uint64_t bits) noexcept {
    buffer |= bits << bits_in_buffer;
    uint32_t total = bits_in_buffer + count;
    uint32_t bytes = total >> 3u;
    axl::storeu_u64_le(data + bytes_written, buffer);
    bytes_written += bytes;
    bits_in_buffer = total & 7u;
    buffer >>= bytes * 8u;
  }

  BL_INLINE void write(uint32_t count, uint64_t bits) noexcept {
    BL_ASSERT(count <= 63u);

    if (!count) {
      return;
    }

    if (count + bits_in_buffer > 63u) {
      write_chunk(32u, uint32_t(bits));
      bits >>= 32u;
      count -= 32u;
    }

    write_chunk(count, bits);
  }

  BL_INLINE void write_full_word(uint64_t bits) noexcept {
    write_chunk(32u, uint32_t(bits));
    write_chunk(32u, bits >> 32u);
  }

  BL_INLINE void pad_to_byte() noexcept {
    if (bits_in_buffer) {
      write_chunk(8u - bits_in_buffer, 0u);
    }
  }
};

static BL_INLINE size_t repeat_mask_stride(uint32_t bpl) noexcept {
  return (size_t(bpl - 1u) + 63u) / 64u;
}

// Masks exclude filter markers. Bit X means residual[X] == residual[X+1];
// each row's final equality and all padding bits must be zero.
void build_repeat_masks(const uint8_t* src, uint32_t bpl, uint32_t h, uint64_t* masks) noexcept;

template<typename LiteralCallback, typename RunCallback>
static BL_INLINE void process_row_repeats(const uint8_t* row, uint32_t size, const uint64_t* masks,
                                          LiteralCallback&& literals, RunCallback&& run
) noexcept {
  if (!masks) {
    uint32_t pending = 0;
    uint32_t x = 0;

    while (x < size) {
      uint32_t begin = x++;
      while (x < size && row[x] == row[begin]) {
        x++;
      }

      if (x - begin >= 4u) {
        if (begin != pending) {
          literals(row + pending, begin - pending);
        }
        run(row + begin, x - begin);
        pending = x;
      }
    }

    if (pending < size) {
      literals(row + pending, size - pending);
    }
    return;
  }

  if (size < 4u) {
    if (size) {
      literals(row, size);
    }
    return;
  }

  size_t count = (size_t(size) + 63u) / 64u;
  size_t word = 0;

  uint64_t equality = masks[0];
  uint64_t preceding = 0;
  uint32_t pending = 0;

  for (;;) {
    auto find_heads = [&]() noexcept {
      uint64_t starts = equality & ~(equality << 1u) & ~preceding;
      uint64_t qualified = starts & (equality >> 1u) & (equality >> 2u);
      if ((starts >> 62u) && word + 1u < count) {
        uint64_t next = masks[word + 1u];
        qualified |= (starts & UINT64_C(0xC000000000000000)) &
                     ((equality >> 1u) | (next << 63u)) &
                     ((equality >> 2u) | (next << 62u));
      }
      return qualified;
    };

    uint64_t starts = find_heads();
    uint64_t zeros = ~equality;

    while (starts) {
      uint32_t begin_bit = axl::ctz(starts);
      uint32_t begin = uint32_t(word * 64u + begin_bit);
      uint64_t ending = zeros & (~uint64_t(0) << begin_bit);
      if (ending) {
        uint32_t end = uint32_t(word * 64u + axl::ctz(ending) + 1u);
        if (begin != pending) {
          literals(row + pending, begin - pending);
        }
        run(row + begin, end - begin);
        pending = end;
        starts &= starts - 1u;
      }
      else {
        // The forced final zero terminates this walk inside the row's masks.
        do {
          equality = masks[++word];
        } while (equality == ~uint64_t(0));

        uint32_t end = uint32_t(word * 64u + axl::ctz(~equality) + 1u);
        if (begin != pending) {
          literals(row + pending, begin - pending);
        }

        run(row + begin, end - begin);
        pending = end;
        preceding = 1u;
        zeros = ~equality;
        starts = find_heads();
      }
    }

    preceding = equality >> 63u;
    if (++word == count) {
      break;
    }
    equality = masks[word];
  }

  if (pending < size) {
    literals(row + pending, size - pending);
  }
}

template<typename Callback>
static BL_INLINE void for_each_zero_match(size_t length, Callback&& callback) noexcept {
  BL_ASSERT(length >= 4u);
  length--;

  if (length < 258u) {
    callback(uint32_t(length), size_t(1));
    return;
  }

  size_t runs = length / 258u;
  uint32_t remain = uint32_t(length % 258u);
  if (remain == 1u || remain == 2u) {
    remain += 255u;
    runs--;
    callback(3u, size_t(1));
  }

  if (runs) {
    callback(258u, runs);
  }

  if (remain) {
    callback(remain, size_t(1));
  }
}

static BL_INLINE void write_literals_ref(FastBitWriter& writer, const HuffmanTable& table, const uint8_t* src, uint32_t count) noexcept {
  while (count) {
    uint32_t n = bl_min<uint32_t>(count, 4u);
    uint32_t nbits = 0;
    uint64_t bits = 0;

    for (uint32_t i = 0; i < n; i++) {
      bits |= uint64_t(table.bits[src[i]]) << nbits;
      nbits += table.nbits[src[i]];
    }

    writer.write(nbits, bits);
    src += n;
    count -= n;
  }
}

static BL_INLINE bool repeat_is_profitable(const HuffmanTable& table, uint8_t value, uint32_t length) noexcept {
  // Nineteen bytes beat even the most expensive short length code when the
  // repeated literal has a one-bit code. Shorter matches need an exact check.
  return length >= 19u || table.lz77_length_nbits[length - 1u] + 1u < (length - 1u) * table.nbits[value];
}

static BL_INLINE void write_repeated_tokens(FastBitWriter& writer, uint32_t nbits, uint64_t bits, size_t count) noexcept {
  if (count < 8u || nbits > 31u) {
    for (size_t i = 0; i < count; i++) {
      writer.write(nbits, bits);
    }
    return;
  }

  uint32_t packed_nbits = nbits;
  uint32_t packed_count = 1u;
  uint64_t packed_bits = bits;

  while (packed_nbits <= 31u && size_t(packed_count) * 2u <= count) {
    packed_bits |= packed_bits << packed_nbits;
    packed_nbits *= 2u;
    packed_count *= 2u;
  }

  while (count >= packed_count) {
    writer.write(packed_nbits, packed_bits);
    count -= packed_count;
  }

  if (count) {
    uint32_t tail_nbits = uint32_t(count) * nbits;
    writer.write(tail_nbits, packed_bits & ((uint64_t(1) << tail_nbits) - 1u));
  }
}

static BL_INLINE void write_repeat_run(FastBitWriter& writer, const HuffmanTable& table, uint8_t value, uint32_t length) noexcept {
  bool first = true;

  for_each_zero_match(length, [&](uint32_t match_length, size_t count) noexcept {
    uint32_t nbits = table.lz77_length_nbits[match_length] + 1u;
    uint32_t bits = table.lz77_length_bits[match_length];

    if (first) {
      writer.write(table.nbits[value] + nbits, uint64_t(table.bits[value]) | (uint64_t(bits) << table.nbits[value]));
      first = false;
      count--;
    }

    write_repeated_tokens(writer, nbits, bits, count);
  });
}

using EncodeRowsFunc = void (BL_CDECL*)(FastBitWriter& writer, const HuffmanTable& table, const uint8_t* data, uint32_t bpl, uint32_t h, const uint64_t* masks) noexcept;
using WriteHeaderFunc = void (BL_CDECL*)(FastBitWriter& writer, const HuffmanTable& table) noexcept;

void BL_CDECL encode_rows_ref(FastBitWriter& writer, const HuffmanTable& table, const uint8_t* data, uint32_t bpl, uint32_t h, const uint64_t* masks) noexcept;
void BL_CDECL write_header_ref(FastBitWriter& writer, const HuffmanTable& table) noexcept;

size_t minimum_output_buffer_size(size_t input_size) noexcept;

// Input already contains filtered PNG scanlines, including their filter bytes.
// Source and destination buffers must not overlap.
// Returns invalid-value for invalid dimensions, null pointers, or capacity below
// minimum_output_buffer_size(). No output bytes are written on such failure.
BLResult encode(uint8_t* dst, size_t capacity, size_t* written, const uint8_t* src,
                uint32_t bpl, uint32_t h, EncodeRowsFunc encode_rows, const uint64_t* masks = nullptr,
                WriteHeaderFunc write_header = nullptr) noexcept;

} // {bl::Png::FastEncoder}

//! \}
//! \endcond

#endif // BLEND2D_CODEC_PNG_ENCODER_FAST_P_H_INCLUDED
