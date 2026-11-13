// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if !defined(BL_BUILD_NO_CODECS) && defined(BL_TEST)

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_ops.h>
#include <blend2d/core/array.h>
#include <blend2d/core/image.h>
#include <blend2d/core/image_codec.h>
#include <blend2d/core/image_decoder.h>
#include <blend2d/core/image_encoder.h>
#include <blend2d/core/random.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/codec/png_encoder_fast_p.h>
#include <blend2d/codec/png_ops_p.h>
#include <blend2d/compression/deflate_decoder_p.h>

namespace bl::Png::Tests {

static constexpr size_t kFastEncoderGuardSize = 64;
static constexpr uint8_t kFastEncoderGuard = 0xA7u;

static uint8_t* fast_test_allocate(BLArray<uint8_t>& array, size_t size) noexcept {
  uint8_t* data = nullptr;
  EXPECT_OK(array.append_uninitialized(size, &data));
  return data;
}

static void fast_test_guard(const uint8_t* data, size_t size) noexcept {
  for (size_t i = 0; i < size; i++) {
    EXPECT_EQ(data[i], kFastEncoderGuard).message("Fast encoder guard overwritten at byte %zu", i);
  }
}

// Independently check the zlib trailer, since Deflate::Decoder does not verify it.
static uint32_t fast_test_adler32(const uint8_t* data, size_t size) noexcept {
  uint32_t s1 = 1;
  uint32_t s2 = 0;
  for (size_t i = 0; i < size; i++) {
    s1 = (s1 + data[i]) % 65521u;
    s2 = (s2 + s1) % 65521u;
  }
  return (s2 << 16) | s1;
}

static void fast_test_inflate(const uint8_t* encoded, size_t encoded_size, const uint8_t* expected, size_t expected_size) noexcept {
  EXPECT_GE(encoded_size, size_t(6));
  EXPECT_EQ((uint32_t(encoded[0]) * 256u + encoded[1]) % 31u, 0u);
  EXPECT_EQ(encoded[0] & 15u, 8u);
  EXPECT_EQ(encoded[1] & 32u, 0u);
  EXPECT_EQ(axl::loadu_u32_be(encoded + encoded_size - 4), fast_test_adler32(expected, expected_size));

  Compression::Deflate::Decoder decoder;
  EXPECT_OK(decoder.init(Compression::Deflate::FormatType::kZlib));
  BLArray<uint8_t> decoded;
  EXPECT_OK(decoder.decode(decoded, {encoded, encoded_size}));
  EXPECT_EQ(decoded.size(), expected_size);
  EXPECT_EQ(memcmp(decoded.data(), expected, expected_size), 0);
}

static void fast_test_stream(const uint8_t* input, size_t bpl, uint32_t h, FastEncoder::EncodeRowsFunc fn,
                             FastEncoder::WriteHeaderFunc write_header, uint32_t misalignment = 0,
                             uint32_t expected_block_type = 0xFFFFFFFFu) noexcept {
  size_t input_size = size_t(bpl) * h;
  size_t bound = FastEncoder::minimum_output_buffer_size(input_size);
  EXPECT_GE(bound, size_t(6));

  BLArray<uint8_t> ref_storage;
  BLArray<uint8_t> opt_storage;

  uint8_t* ref_base = fast_test_allocate(ref_storage, bound + kFastEncoderGuardSize * 2);
  uint8_t* opt_base = fast_test_allocate(opt_storage, bound + kFastEncoderGuardSize * 2 + misalignment);
  memset(ref_base, kFastEncoderGuard, ref_storage.size());
  memset(opt_base, kFastEncoderGuard, opt_storage.size());

  uint8_t* ref = ref_base + kFastEncoderGuardSize;
  uint8_t* opt = opt_base + kFastEncoderGuardSize + misalignment;
  size_t ref_size = 0;
  size_t opt_size = 0;

  BLArray<uint64_t> mask_storage;
  uint64_t* mask_base = nullptr;
  size_t mask_count = FastEncoder::repeat_mask_stride(bpl) * h;
  EXPECT_OK(mask_storage.append_uninitialized(mask_count + 2u, &mask_base));

  memset(mask_base, kFastEncoderGuard, mask_storage.size() * sizeof(uint64_t));
  uint64_t* masks = mask_base + 1u;
  FastEncoder::build_repeat_masks(input, bpl, h, masks);
  EXPECT_OK(FastEncoder::encode(ref, bound, &ref_size, input, bpl, h, FastEncoder::encode_rows_ref));
  EXPECT_LE(ref_size, bound);

  FastEncoder::EncodeRowsFunc functions[] = { FastEncoder::encode_rows_ref, fn };
  for (FastEncoder::EncodeRowsFunc encode_rows : functions) {
    for (uint32_t masked = 0; masked < 2; masked++) {
      memset(opt, kFastEncoderGuard, bound);

      EXPECT_OK(FastEncoder::encode(opt, bound, &opt_size, input, bpl, h, encode_rows, masked ? masks : nullptr, write_header));
      EXPECT_LE(opt_size, bound);
      EXPECT_EQ(opt_size, ref_size);
      EXPECT_EQ(memcmp(opt, ref, ref_size), 0).message(
        "Fast encoder stream mismatch: Bpl=%u H=%u Misalignment=%u Masked=%u NativeHeader=%u",
        bpl, h, misalignment, masked, uint32_t(write_header != nullptr));

      fast_test_guard(opt_base, kFastEncoderGuardSize + misalignment);
      fast_test_guard(opt + bound, kFastEncoderGuardSize);
    }
    if (fn == FastEncoder::encode_rows_ref)
      break;
  }

  fast_test_guard(ref_base, kFastEncoderGuardSize);
  fast_test_guard(ref + bound, kFastEncoderGuardSize);
  fast_test_guard(opt_base, kFastEncoderGuardSize + misalignment);
  fast_test_guard(opt + bound, kFastEncoderGuardSize);
  fast_test_guard(reinterpret_cast<const uint8_t*>(mask_base), sizeof(uint64_t));
  fast_test_guard(reinterpret_cast<const uint8_t*>(masks + mask_count), sizeof(uint64_t));
  fast_test_inflate(opt, opt_size, input, input_size);

  if (expected_block_type != 0xFFFFFFFFu) {
    EXPECT_EQ((opt[2] >> 1u) & 3u, expected_block_type);
  }
}

static void test_fast_encoder_rows(FastEncoder::EncodeRowsFunc fn, FastEncoder::WriteHeaderFunc write_header = nullptr) noexcept {
  static const uint32_t row_sizes[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 47, 48, 49, 63, 64, 65, 95, 96, 97, 127, 128, 129,
    191, 192, 193, 255, 256, 257, 513
  };
  static const uint32_t heights[] = { 1, 2, 5 };
  BLRandom rnd(0xB17F457123456789u);
  for (uint32_t row_size : row_sizes) {
    uint32_t bpl = row_size + 1u;
    for (uint32_t h : heights) {
      BLArray<uint8_t> storage;
      uint8_t* input = fast_test_allocate(storage, size_t(bpl) * h + 63u);
      for (uint32_t pattern = 0; pattern < 4; pattern++) {
        // Vary input alignment too. The final row has exactly the tested tail.
        uint32_t offset = (row_size + pattern) % 64u;
        uint8_t* p = input + offset;
        for (uint32_t y = 0; y < h; y++) {
          p[size_t(y) * bpl] = uint8_t((y + pattern) % kFilterTypeCount);
          for (uint32_t x = 0; x < row_size; x++) {
            uint8_t value;
            if (pattern == 0)
              value = 0;
            else if (pattern == 1)
              value = uint8_t(rnd.next_uint32() >> 24u);
            else if (pattern == 2)
              value = uint8_t(int32_t(rnd.next_uint32() % 32u) - 16);
            else
              value = uint8_t(16u + rnd.next_uint32() % 224u);
            p[size_t(y) * bpl + 1u + x] = value;
          }
        }
        fast_test_stream(p, bpl, h, fn, write_header, (row_size + pattern * 7u) % 64u);
      }
    }
  }

  // A sampled-table image needs enough rows to exercise central-row sampling.
  constexpr uint32_t kBpl = 131;
  constexpr uint32_t kHeight = 129;
  BLArray<uint8_t> sampled;
  uint8_t* p = fast_test_allocate(sampled, size_t(kBpl) * kHeight);
  for (uint32_t y = 0; y < kHeight; y++) {
    p[size_t(y) * kBpl] = uint8_t(y % kFilterTypeCount);
    for (uint32_t x = 1; x < kBpl; x++) {
      p[size_t(y) * kBpl + x] = y < 64u ? uint8_t(rnd.next_uint32() >> 24u) : uint8_t(x % 17u ? 0u : 255u);
    }
  }
  fast_test_stream(p, kBpl, kHeight, fn, write_header, 3);

  // Keep the stream dynamic while exercising every literal and every final
  // vector tail. Literal-only small/random cases can legitimately use stored
  // blocks, which would otherwise hide errors in SIMD Huffman packing.
  for (uint32_t tail = 0; tail < 64; tail++) {
    uint32_t bpl = 2049u + tail;
    BLArray<uint8_t> mixed;
    uint8_t* bytes = fast_test_allocate(mixed, size_t(bpl) * 3u);
    memset(bytes, 0, mixed.size());
    for (uint32_t y = 0; y < 3; y++) {
      uint8_t* row = bytes + size_t(y) * bpl;
      row[0] = uint8_t(y);
      for (uint32_t x = 256; x < 768; x++) {
        row[x + 1u] = uint8_t(x + y * 19u);
      }
      for (uint32_t x = 768; x < 1280; x++) {
        row[x + 1u] = uint8_t(int32_t((x + y) % 32u) - 16);
      }
      for (uint32_t x = 2048; x < bpl - 1u; x++) {
        row[x + 1u] = uint8_t(16u + x % 224u);
      }
    }
    fast_test_stream(bytes, bpl, 3, fn, write_header, tail, 2);
  }

  // The unsampled prior assigns both rare near-zero literals eight-bit codes.
  // Alternating them avoids runs, so eight literals fill exactly 64 bits in
  // the SIMD short packing path; zero rows keep the whole stream dynamic.
  {
    constexpr uint32_t kShortBpl = 65;
    constexpr uint32_t kShortHeight = 9;
    uint8_t bytes[kShortBpl * kShortHeight]{};
    for (uint32_t y = 0; y < kShortHeight; y++) {
      bytes[y * kShortBpl] = uint8_t(kFilterTypePaeth);
    }
    for (uint32_t x = 0; x < kShortBpl - 1u; x++) {
      bytes[x + 1u] = uint8_t(241u + (x & 1u));
    }
    fast_test_stream(bytes, kShortBpl, kShortHeight, fn, write_header, 5, 2);
  }

  // A single middle literal changes the packing width in just one half of a
  // 64-byte span. Move it through every byte and both sides of lane boundaries.
  for (uint32_t middle = 0; middle < 64u; middle++) {
    constexpr uint32_t kMixedBpl = 65;
    constexpr uint32_t kMixedHeight = 9;
    uint8_t bytes[kMixedBpl * kMixedHeight]{};
    for (uint32_t y = 0; y < kMixedHeight; y++) {
      bytes[y * kMixedBpl] = uint8_t(kFilterTypePaeth);
    }
    for (uint32_t x = 0; x < kMixedBpl - 1u; x++) {
      bytes[x + 1u] = uint8_t(241u + (x & 1u));
    }
    bytes[middle + 1u] = 127u;
    fast_test_stream(bytes, kMixedBpl, kMixedHeight, fn, write_header, middle, 2);
  }

  // Exercise length-258 splitting, remainder 1/2 repairs, and scalar zero tails.
  static const uint32_t run_lengths[] = { 16, 17, 31, 32, 33, 255, 256, 257, 258, 259, 260, 261, 262, 272, 273, 516, 517, 518, 519, 520, 1024 };
  for (uint32_t run : run_lengths) {
    for (uint32_t tail = 0; tail < 3; tail++) {
      uint32_t bpl = run + tail + 1u;
      BLArray<uint8_t> data;
      uint8_t* bytes = fast_test_allocate(data, size_t(bpl) * 3u);
      memset(bytes, 0, data.size());
      for (uint32_t y = 0; y < 3; y++) {
        bytes[size_t(y) * bpl] = uint8_t(y);
        for (uint32_t x = 0; x < tail; x++) {
          bytes[size_t(y) * bpl + 1u + run + x] = uint8_t(127u + x);
        }
      }
      fast_test_stream(bytes, bpl, 3, fn, write_header, tail);
    }
  }

  // The dynamic path must compress a long zero run, while literal-heavy input
  // should use stored blocks instead of expanding to the constrained 12-bit codes.
  {
    BLArray<uint8_t> zeros;
    uint8_t* bytes = fast_test_allocate(zeros, 4097);
    memset(bytes, 0, zeros.size());
    fast_test_stream(bytes, 4097, 1, fn, write_header, 1, 2);
  }
  {
    constexpr uint32_t kStoredBpl = 65538;
    BLArray<uint8_t> literals;
    uint8_t* bytes = fast_test_allocate(literals, size_t(kStoredBpl) * 2u);
    for (uint32_t y = 0; y < 2; y++) {
      bytes[size_t(y) * kStoredBpl] = uint8_t(y);
      for (uint32_t x = 1; x < kStoredBpl; x++) {
        bytes[size_t(y) * kStoredBpl + x] = uint8_t(16u + x % 224u);
      }
    }
    fast_test_stream(bytes, kStoredBpl, 2, fn, write_header, 7, 0);
  }
}

static void test_fast_encoder_repeats(FastEncoder::EncodeRowsFunc fn, FastEncoder::WriteHeaderFunc write_header = nullptr) noexcept {
  static const uint32_t run_lengths[] = {
    1, 2, 3, 4, 5, 14, 15, 16, 17, 18, 19, 20, 31, 32, 33,
    61, 62, 63, 64, 65, 66, 127, 128, 129,
    255, 256, 257, 258, 259, 260, 261, 262, 263,
    515, 516, 517, 518, 519, 520, 1024
  };
  static const uint32_t prefixes[] = {
    0, 1, 13, 14, 15, 16, 17, 61, 62, 63, 64, 65, 125, 126, 127, 128, 129
  };
  static const uint8_t values[] = { 1, 17, 127, 240, 255 };
  constexpr uint32_t kHeight = BL_ARRAY_SIZE(values);

  for (uint32_t run : run_lengths) {
    for (uint32_t prefix : prefixes) {
      BLArray<uint8_t> data;

      uint32_t bpl = prefix + run + 1027u;
      uint8_t* bytes = fast_test_allocate(data, size_t(bpl) * kHeight);
      memset(bytes, 0, data.size());

      for (uint32_t y = 0; y < kHeight; y++) {
        uint8_t* row = bytes + size_t(y) * bpl;
        uint8_t value = values[y];

        // Include a marker equal to the first residual: masks must start after
        // the marker, and runs must never continue into another scanline.
        row[0] = uint8_t((y + 1u) % kFilterTypeCount);
        for (uint32_t x = 0; x < prefix; x++) {
          row[x + 1u] = uint8_t(32u + x % 96u);
        }

        if (prefix) {
          row[prefix] = uint8_t(value ^ 1u);
        }

        memset(row + prefix + 1u, value, run);
        row[prefix + run + 1u] = uint8_t(value ^ 1u);
        row[prefix + run + 2u] = uint8_t(value ^ 2u);
      }
      // The final zero anchor keeps short nonzero runs on the dynamic path.
      fast_test_stream(bytes, bpl, kHeight, fn, write_header, (prefix + run) % 64u, 2);
    }
  }

  // A whole row may have the same value as both adjacent filter markers.
  // Exact word-sized rows require their final equality bit to be cleared.
  static const uint32_t whole_row_sizes[] = { 63, 64, 65, 127, 128, 129, 191, 192, 193, 1024 };
  for (uint32_t row_size : whole_row_sizes) {
    BLArray<uint8_t> data;

    uint32_t bpl = row_size + 1u;
    uint8_t* bytes = fast_test_allocate(data, size_t(bpl) * 5u);

    memset(bytes, kFilterTypePaeth, data.size());
    fast_test_stream(bytes, bpl, 5u, fn, write_header, row_size % 64u, 2);
  }
}

TEST_CASE(codec_png_fast_encoder) {
  test_fast_encoder_rows(FastEncoder::encode_rows_ref);
  test_fast_encoder_repeats(FastEncoder::encode_rows_ref);
}

TEST_CASE(codec_png_fast_repeat_masks) {
  constexpr uint32_t kHeight = 3;
  constexpr uint32_t kMaxRowSize = 193;

  uint8_t bytes[(kMaxRowSize + 1u) * kHeight];
  uint64_t mask_storage[((kMaxRowSize + 63u) / 64u) * kHeight + 2u];
  BLRandom rnd(0xE91347A5B2213u);

  for (uint32_t row_size = 1; row_size <= kMaxRowSize; row_size++) {
    size_t bpl = row_size + 1u;
    size_t stride = (size_t(row_size) + 63u) / 64u;
    EXPECT_EQ(FastEncoder::repeat_mask_stride(bpl), stride);

    for (uint32_t pattern = 0; pattern < 4; pattern++) {
      for (uint32_t y = 0; y < kHeight; y++) {
        uint8_t* row = bytes + y * bpl;
        row[0] = uint8_t(y + 1u);
        for (uint32_t x = 0; x < row_size; x++) {
          row[x + 1u] = pattern == 0 ? uint8_t(y + 1u) :
                        pattern == 1 ? uint8_t(x & 1u) :
                        pattern == 2 ? uint8_t((x / 3u + y) % 251u) : uint8_t(rnd.next_uint32() >> 24u);
        }
      }

      memset(mask_storage, kFastEncoderGuard, sizeof(mask_storage));
      uint64_t* masks = mask_storage + 1u;
      // Dirty destination bits catch producers that fail to clear row padding.
      memset(masks, 0xFF, stride * kHeight * sizeof(uint64_t));
      FastEncoder::build_repeat_masks(bytes, bpl, kHeight, masks);
      for (uint32_t y = 0; y < kHeight; y++) {
        const uint8_t* row = bytes + y * bpl + 1u;
        for (size_t word = 0; word < stride; word++) {
          uint64_t expected = 0;
          for (uint32_t bit = 0; bit < 64u; bit++) {
            size_t x = word * 64u + bit;
            if (x + 1u < row_size && row[x] == row[x + 1u])
              expected |= uint64_t(1) << bit;
          }
          EXPECT_EQ(masks[y * stride + word], expected).message(
            "Repeat mask mismatch: RowSize=%u Pattern=%u Row=%u Word=%zu", row_size, pattern, y, word);
        }
      }

      fast_test_guard(reinterpret_cast<const uint8_t*>(mask_storage), sizeof(uint64_t));
      fast_test_guard(reinterpret_cast<const uint8_t*>(masks + stride * kHeight), sizeof(uint64_t));
    }
  }
}

#if defined(BL_BUILD_OPT_AVX512_V2) || defined(BL_BUILD_OPT_ASIMD)
static void test_fast_encoder_literal_bit_offsets(FastEncoder::EncodeRowsFunc fn) noexcept {
  constexpr size_t kCapacity = 512;

  static const uint32_t row_sizes[] = { 16, 17, 31, 32, 33, 63, 64, 65, 95, 96, 97, 127, 128, 129, 191, 192, 193 };
  static const uint8_t reverse_nibble[] = { 0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15 };

  uint8_t input[194];
  uint8_t ref_storage[kCapacity + kFastEncoderGuardSize * 2u];
  uint8_t opt_storage[kCapacity + kFastEncoderGuardSize * 2u];
  uint8_t* ref = ref_storage + kFastEncoderGuardSize;
  uint8_t* opt = opt_storage + kFastEncoderGuardSize;

  // Exercise the literal writer independently of tree selection, so all bit
  // offsets, full 64-bit groups, and groups shorter than one byte are covered.
  for (uint32_t lengths = 0; lengths < 3u; lengths++) {
    FastEncoder::HuffmanTable table{};
    table.mid_nbits = 12;
    for (uint32_t i = 0; i < 16u; i++) {
      table.mid_lowbits[i] = uint8_t(i * 17u + 3u);
      for (uint32_t high = 0; high < 2u; high++) {
        uint32_t value = i + high * 240u;
        uint32_t nbits = lengths == 0 ? 1u : lengths == 1 ? 8u : (i + high * 3u) % 8u + 1u;
        uint8_t bits = uint8_t((value * 37u + 11u) & ((1u << nbits) - 1u));

        table.nbits[value] = uint8_t(nbits);
        table.bits[value] = bits;

        (high ? table.last16_nbits : table.first16_nbits)[i] = uint8_t(nbits);
        (high ? table.last16_bits : table.first16_bits)[i] = bits;
      }
    }
    for (uint32_t value = 16; value < 240u; value++) {
      table.nbits[value] = 12;
      table.bits[value] = uint16_t(table.mid_lowbits[value >> 4u] | uint32_t(reverse_nibble[value & 15u]) << 8u);
    }

    for (uint32_t row_size : row_sizes) {
      for (uint32_t pattern = 0; pattern < 3u; pattern++) {
        input[0] = uint8_t(kFilterTypePaeth);
        for (uint32_t x = 0; x < row_size; x++) {
          uint32_t near_zero = (x * 13u + x / 8u) % 32u;
          input[x + 1u] = pattern == 2 || (pattern == 1 && x % 64u == 31u)
            ? uint8_t(16u + x * 29u % 224u)
            : uint8_t(near_zero < 16u ? near_zero : near_zero + 224u);
        }

        for (uint32_t prefix = 0; prefix < 8u; prefix++) {
          memset(ref_storage, kFastEncoderGuard, sizeof(ref_storage));
          memset(opt_storage, kFastEncoderGuard, sizeof(opt_storage));

          FastEncoder::FastBitWriter ref_writer(ref, kCapacity);
          FastEncoder::FastBitWriter opt_writer(opt, kCapacity);

          uint64_t prefix_bits = 0x5Du & ((1u << prefix) - 1u);
          ref_writer.write(prefix, prefix_bits);
          opt_writer.write(prefix, prefix_bits);

          FastEncoder::encode_rows_ref(ref_writer, table, input, row_size + 1u, 1u, nullptr);
          fn(opt_writer, table, input, row_size + 1u, 1u, nullptr);
          EXPECT_EQ(opt_writer.bytes_written, ref_writer.bytes_written);
          EXPECT_EQ(opt_writer.bits_in_buffer, ref_writer.bits_in_buffer);
          EXPECT_EQ(opt_writer.buffer, ref_writer.buffer).message(
            "Literal carry mismatch: RowSize=%u Lengths=%u Pattern=%u Prefix=%u", row_size, lengths, pattern, prefix);

          // Appending another code checks that the pending partial byte remains
          // usable by the ordinary scalar writer after the SIMD batch.
          ref_writer.write(13u, 0x1A5Bu);
          opt_writer.write(13u, 0x1A5Bu);
          ref_writer.pad_to_byte();
          opt_writer.pad_to_byte();

          EXPECT_EQ(opt_writer.bytes_written, ref_writer.bytes_written);
          EXPECT_EQ(memcmp(opt, ref, ref_writer.bytes_written), 0).message(
            "Literal packing mismatch: RowSize=%u Lengths=%u Pattern=%u Prefix=%u", row_size, lengths, pattern, prefix);
          fast_test_guard(opt_storage, kFastEncoderGuardSize);
          fast_test_guard(opt + kCapacity, kFastEncoderGuardSize);
        }
      }
    }
  }
}
#endif // BL_BUILD_OPT_AVX512_V2 || BL_BUILD_OPT_ASIMD

TEST_CASE(codec_png_fast_encoder_simd) {
#ifdef BL_BUILD_OPT_SSE2
  if (bl_runtime_has_sse2(&bl_runtime_context) && test_runner.test_variation("sse2")) {
    Ops::FunctionTable optimized{};
    Ops::init_func_table_sse2(optimized);
    test_fast_encoder_rows(optimized.encode_rows_fast, optimized.write_header_fast);
    test_fast_encoder_repeats(optimized.encode_rows_fast, optimized.write_header_fast);
  }
#endif // BL_BUILD_OPT_SSE2
#ifdef BL_BUILD_OPT_SSSE3
  if (bl_runtime_has_ssse3(&bl_runtime_context) && test_runner.test_variation("ssse3")) {
    Ops::FunctionTable optimized{};
    Ops::init_func_table_ref(optimized);
    Ops::init_fast_encoder_ssse3(optimized);
    test_fast_encoder_rows(optimized.encode_rows_fast, optimized.write_header_fast);
    test_fast_encoder_repeats(optimized.encode_rows_fast, optimized.write_header_fast);
  }
#endif // BL_BUILD_OPT_SSSE3
#ifdef BL_BUILD_OPT_AVX
  if (bl_runtime_has_avx(&bl_runtime_context) && test_runner.test_variation("avx")) {
    Ops::FunctionTable optimized{};
    Ops::init_func_table_avx(optimized);
    test_fast_encoder_rows(optimized.encode_rows_fast, optimized.write_header_fast);
    test_fast_encoder_repeats(optimized.encode_rows_fast, optimized.write_header_fast);
  }
#endif // BL_BUILD_OPT_AVX
#ifdef BL_BUILD_OPT_AVX512_V2
  if (bl_runtime_has_avx512_v2(&bl_runtime_context) && test_runner.test_variation("avx512_v2")) {
    Ops::FunctionTable optimized{};
    Ops::init_func_table_ref(optimized);
    Ops::init_fast_encoder_avx512_v2(optimized);
    test_fast_encoder_rows(optimized.encode_rows_fast, optimized.write_header_fast);
    test_fast_encoder_repeats(optimized.encode_rows_fast, optimized.write_header_fast);
    test_fast_encoder_literal_bit_offsets(optimized.encode_rows_fast);
  }
#endif // BL_BUILD_OPT_AVX512_V2
#ifdef BL_BUILD_OPT_ASIMD
  if (bl_runtime_has_asimd(&bl_runtime_context) && test_runner.test_variation("neon")) {
    Ops::FunctionTable optimized{};
    Ops::init_func_table_asimd(optimized);
    test_fast_encoder_rows(optimized.encode_rows_fast, optimized.write_header_fast);
    test_fast_encoder_repeats(optimized.encode_rows_fast, optimized.write_header_fast);
    test_fast_encoder_literal_bit_offsets(optimized.encode_rows_fast);
  }
#endif // BL_BUILD_OPT_ASIMD
}

TEST_CASE(codec_png_fast_encoder_invalid) {
  uint8_t input[] = { 0u, 0u };
  size_t bound = FastEncoder::minimum_output_buffer_size(sizeof(input));
  BLArray<uint8_t> storage;
  uint8_t* data = fast_test_allocate(storage, bound + kFastEncoderGuardSize * 2u);
  memset(data, kFastEncoderGuard, storage.size());
  uint8_t* output = data + kFastEncoderGuardSize;
  size_t written = SIZE_MAX;

  EXPECT_EQ(FastEncoder::encode(output, bound - 1u, &written, input, 2, 1, FastEncoder::encode_rows_ref), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(written, size_t(0));
  EXPECT_EQ(FastEncoder::encode(output, bound, &written, nullptr, 2, 1, FastEncoder::encode_rows_ref), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(FastEncoder::encode(nullptr, bound, &written, input, 2, 1, FastEncoder::encode_rows_ref), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(FastEncoder::encode(output, bound, nullptr, input, 2, 1, FastEncoder::encode_rows_ref), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(FastEncoder::encode(output, bound, &written, input, 2, 1, nullptr), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(FastEncoder::encode(output, bound, &written, input, 1, 1, FastEncoder::encode_rows_ref), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(FastEncoder::encode(output, bound, &written, input, 2, 0, FastEncoder::encode_rows_ref), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(FastEncoder::encode(output, bound, &written, input, UINT32_MAX, UINT32_MAX, FastEncoder::encode_rows_ref), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(written, size_t(0));
  EXPECT_EQ(FastEncoder::minimum_output_buffer_size(SIZE_MAX), SIZE_MAX);
  fast_test_guard(data, storage.size());
}

TEST_CASE(codec_png_fast_bit_writer) {
  static const uint32_t counts[] = { 0, 1, 7, 8, 15, 32, 48, 56, 57, 63, 2, 63, 1, 3 };
  uint8_t storage[64u + kFastEncoderGuardSize * 2u];
  memset(storage, kFastEncoderGuard, sizeof(storage));
  uint8_t expected[64]{};
  uint8_t* output = storage + kFastEncoderGuardSize;
  FastEncoder::BitWriter writer(output, sizeof(expected));
  BLRandom rnd(0x1299384756444u);
  uint32_t bit_offset = 0;
  for (uint32_t count : counts) {
    uint64_t bits = rnd.next_uint64();
    writer.write(count, bits);
    for (uint32_t i = 0; i < count; i++) {
      expected[bit_offset / 8u] |= uint8_t(((bits >> i) & 1u) << (bit_offset % 8u));
      bit_offset++;
    }
  }
  writer.pad_to_byte();
  EXPECT_FALSE(writer.overflow);
  EXPECT_EQ(writer.bytes_written, size_t((bit_offset + 7u) / 8u));
  EXPECT_EQ(memcmp(output, expected, writer.bytes_written), 0);
  fast_test_guard(storage, kFastEncoderGuardSize);
  fast_test_guard(output + sizeof(expected), kFastEncoderGuardSize);

  // Wide stores must respect the caller's actual capacity, including the last
  // seven bytes. An overflow can modify valid output bytes but never its guards.
  for (size_t capacity = 0; capacity < 9; capacity++) {
    memset(storage, kFastEncoderGuard, sizeof(storage));
    FastEncoder::BitWriter small(output, capacity);
    small.write(7u, 0x7Fu);
    small.write(63u, UINT64_MAX);
    small.pad_to_byte();
    EXPECT_TRUE(small.overflow);
    fast_test_guard(storage, kFastEncoderGuardSize);
    fast_test_guard(output + capacity, sizeof(storage) - kFastEncoderGuardSize - capacity);
  }
}

TEST_CASE(codec_png_fast_trusted_bit_writer) {
  static const uint32_t counts[] = { 0, 1, 7, 8, 15, 32, 48, 56, 57, 63, 64, 2, 63, 1, 3 };
  constexpr size_t kCapacity = 128u + 7u;
  uint8_t storage[kCapacity + kFastEncoderGuardSize * 2u];
  BLRandom rnd(0x83B4916743A5u);

  for (uint32_t prefix = 0; prefix < 8u; prefix++) {
    memset(storage, kFastEncoderGuard, sizeof(storage));
    uint8_t expected[128]{};
    uint8_t* output = storage + kFastEncoderGuardSize;
    FastEncoder::FastBitWriter writer(output, kCapacity);
    uint32_t bit_offset = 0;
    auto append = [&](uint32_t count, uint64_t bits) noexcept {
      if (count != 64u)
        bits &= (uint64_t(1) << count) - 1u;
      if (count == 64u)
        writer.write_full_word(bits);
      else
        writer.write(count, bits);
      for (uint32_t i = 0; i < count; i++) {
        expected[bit_offset / 8u] |= uint8_t(((bits >> i) & 1u) << (bit_offset % 8u));
        bit_offset++;
      }
    };

    append(prefix, rnd.next_uint64());
    for (uint32_t count : counts)
      append(count, rnd.next_uint64());
    writer.pad_to_byte();
    EXPECT_FALSE(writer.overflow);
    EXPECT_EQ(writer.bits_in_buffer, 0u);
    EXPECT_EQ(writer.bytes_written, size_t((bit_offset + 7u) / 8u));
    EXPECT_EQ(memcmp(output, expected, writer.bytes_written), 0).message("Fast bit writer mismatch: PrefixBits=%u", prefix);
    fast_test_guard(storage, kFastEncoderGuardSize);
    fast_test_guard(output + kCapacity, kFastEncoderGuardSize);
  }
}

static void BL_CDECL overflow_fast_encoder_writer(
  FastEncoder::FastBitWriter& writer, const FastEncoder::HuffmanTable& table,
  const uint8_t* data, size_t bpl, uint32_t h, const uint64_t* masks
) noexcept {
  axl::maybe_unused(table, data, bpl, h, masks);
  writer.overflow = true;
}

TEST_CASE(codec_png_fast_encoder_rollback) {
  static const uint8_t prefix[] = {0x42u, 0x00u, 0xA7u, 0xFFu, 0x13u};
  BLImage image;
  EXPECT_OK(image.create(33, 5, BL_FORMAT_XRGB32));
  BLImageData data;
  EXPECT_OK(image.make_mutable(&data));
  for (uint32_t y = 0; y < 5; y++)
    memset(static_cast<uint8_t*>(data.pixel_data) + intptr_t(y) * data.stride, 0xFF, 33u * 4u);

  BLImageCodec codec;
  EXPECT_OK(codec.find_by_name("PNG"));
  BLImageEncoder encoder;
  EXPECT_OK(codec.create_encoder(&encoder));
  EXPECT_OK(bl_object_set_property_uint32(&encoder, "compression", SIZE_MAX, 1u));
  BLArray<uint8_t> encoded;
  EXPECT_OK(encoded.append_data(prefix, sizeof(prefix)));

  // The scanlines exceed the stored-only tiny-image threshold. Force a failure
  // after the PNG header/IDAT reservation and restore dispatch before any check
  // can abort the test, so subsequent encoders always see the production table.
  FastEncoder::EncodeRowsFunc saved = Ops::func_table.encode_rows_fast;
  Ops::func_table.encode_rows_fast = overflow_fast_encoder_writer;
  BLResult result = encoder.write_frame(encoded, image);
  Ops::func_table.encode_rows_fast = saved;

  EXPECT_EQ(result, BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(encoded.size(), sizeof(prefix));
  EXPECT_EQ(memcmp(encoded.data(), prefix, sizeof(prefix)), 0);
}

TEST_CASE(codec_png_fast_encoder_append) {
  static const uint8_t prefix[] = {0x1Du, 0x00u, 0xFFu, 0xA7u, 0x42u, 0x81u, 0x02u};
  static const uint32_t widths[] = { 1, 2, 3, 5, 7, 15, 16, 17, 31, 33 };
  static const BLFormat formats[] = { BL_FORMAT_XRGB32, BL_FORMAT_PRGB32, BL_FORMAT_A8 };
  BLImageCodec codec;
  EXPECT_OK(codec.find_by_name("PNG"));
  for (BLFormat format : formats) {
    for (uint32_t w : widths) {
      constexpr uint32_t kHeight = 3;
      BLImage image;
      EXPECT_OK(image.create(int(w), int(kHeight), format));
      BLImageData data;
      EXPECT_OK(image.make_mutable(&data));
      for (uint32_t y = 0; y < kHeight; y++) {
        uint8_t* row = static_cast<uint8_t*>(data.pixel_data) + intptr_t(y) * data.stride;
        for (uint32_t x = 0; x < w; x++) {
          if (format == BL_FORMAT_A8) {
            row[x] = uint8_t(x * 17u + y * 31u);
          }
          else {
            uint32_t a = format == BL_FORMAT_XRGB32 ? 255u : ((x + y) % 4u) * 85u;
            uint32_t r = ((x * 13u + y * 7u) & 255u) * a / 255u;
            uint32_t g = ((x * 5u + y * 19u) & 255u) * a / 255u;
            uint32_t b = ((x * 3u + y * 23u) & 255u) * a / 255u;
            axl::storeu_u32(row + x * 4u, (a << 24u) | (r << 16u) | (g << 8u) | b);
          }
        }
      }
      for (uint32_t filter = 0; filter <= kFilterTypeDefault; filter++) {
        BLImageEncoder encoder;
        EXPECT_OK(codec.create_encoder(&encoder));
        EXPECT_OK(bl_object_set_property_uint32(&encoder, "compression", SIZE_MAX, 1u));
        EXPECT_OK(bl_object_set_property_uint32(&encoder, "filter", SIZE_MAX, filter));
        BLArray<uint8_t> encoded;
        EXPECT_OK(encoded.append_data(prefix, sizeof(prefix)));
        EXPECT_OK(encoder.write_frame(encoded, image));
        EXPECT_GE(encoded.size(), sizeof(prefix) + size_t(8));
        EXPECT_EQ(memcmp(encoded.data(), prefix, sizeof(prefix)), 0);
        EXPECT_EQ(memcmp(encoded.data() + sizeof(prefix), "\x89PNG\r\n\x1a\n", 8), 0);

        BLImage decoded;
        EXPECT_OK(decoded.read_from_data(encoded.data() + sizeof(prefix), encoded.size() - sizeof(prefix)));
        EXPECT_EQ(decoded.width(), int(w));
        EXPECT_EQ(decoded.height(), int(kHeight));
        BLImageData actual;
        EXPECT_OK(decoded.get_data(&actual));
        for (uint32_t y = 0; y < kHeight; y++) {
          const uint8_t* src = static_cast<const uint8_t*>(data.pixel_data) + intptr_t(y) * data.stride;
          const uint8_t* dst = static_cast<const uint8_t*>(actual.pixel_data) + intptr_t(y) * actual.stride;
          if (format == BL_FORMAT_A8) {
            for (uint32_t x = 0; x < w; x++)
              EXPECT_EQ(axl::loadu_u32(dst + x * 4u), 0xFF000000u | uint32_t(src[x]) * 0x010101u);
          }
          else {
            EXPECT_EQ(memcmp(src, dst, size_t(w) * 4u), 0).message(
              "Fast PNG appended roundtrip mismatch: W=%u Format=%u Filter=%u Row=%u", w, uint32_t(format), filter, y);
          }
        }
      }
    }
  }
}

} // {bl::Png::Tests}

#endif // !BL_BUILD_NO_CODECS && BL_TEST
