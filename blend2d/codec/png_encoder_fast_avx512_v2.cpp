// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#if !defined(BL_BUILD_NO_CODECS) && defined(BL_TARGET_OPT_AVX512_V2)

#include <blend2d/codec/png_ops_simd_impl_p.h>
#include <blend2d/codec/png_encoder_fast_simd_p.h>

namespace bl::Png::FastEncoder {
namespace {

struct Avx512V2LiteralTable {
  SimdLiteralTable tail;
  __m512i nbits;
  __m512i bits;
  __m512i reverse_nibble;

  BL_INLINE explicit Avx512V2LiteralTable(const HuffmanTable& table) noexcept
    : tail(table),
      nbits(_mm512_inserti32x4(_mm512_inserti32x4(_mm512_set1_epi8(12), tail.last_nbits.v, 0), tail.first_nbits.v, 1)),
      bits(_mm512_inserti32x4(_mm512_inserti32x4(_mm512_broadcast_i32x4(tail.mid_bits.v), tail.last_bits.v, 0), tail.first_bits.v, 1)),
      reverse_nibble(_mm512_broadcast_i32x4(tail.reverse_nibble.v)) {}
};

// Each reduction concatenates adjacent codes and sums their lengths without
// changing their order. Short literals start in bytes; twelve-bit literals
// start in words. Both paths finish with eight independently packed qwords.
static BL_INLINE void pack_literal_bytes(__m512i& codes, __m512i& lengths) noexcept {
  __m512i mask = _mm512_set1_epi16(0xFF);
  __m512i lo_lengths = _mm512_and_si512(lengths, mask);
  codes = _mm512_or_si512(_mm512_and_si512(codes, mask), _mm512_sllv_epi16(_mm512_srli_epi16(codes, 8), lo_lengths));
  lengths = _mm512_add_epi16(lo_lengths, _mm512_srli_epi16(lengths, 8));
}

static BL_INLINE void pack_literal_words(__m512i& codes, __m512i& lengths) noexcept {
  __m512i word_mask = _mm512_set1_epi32(0xFFFF);
  __m512i lo_lengths = _mm512_and_si512(lengths, word_mask);
  codes = _mm512_or_si512(_mm512_and_si512(codes, word_mask), _mm512_sllv_epi32(_mm512_srli_epi32(codes, 16), lo_lengths));
  lengths = _mm512_add_epi32(lo_lengths, _mm512_srli_epi32(lengths, 16));
}

static BL_INLINE void pack_literal_dwords(__m512i& codes, __m512i& lengths) noexcept {
  __m512i dword_mask = _mm512_set1_epi64(0xFFFFFFFFu);
  __m512i lo_lengths = _mm512_and_si512(lengths, dword_mask);
  codes = _mm512_or_si512(_mm512_and_si512(codes, dword_mask), _mm512_sllv_epi64(_mm512_srli_epi64(codes, 32), lo_lengths));
  lengths = _mm512_add_epi64(lo_lengths, _mm512_srli_epi64(lengths, 32));
}

template<bool ShortCodes>
static BL_INLINE void write_literal_chunks(FastBitWriter& writer, const __m512i& codes, const __m512i& lengths) noexcept {
  uint32_t incoming_bits = writer.bits_in_buffer;
  uint64_t incoming_buffer = writer.buffer;
  size_t incoming_bytes = writer.bytes_written;
  __m512i zero = _mm512_setzero_si512();
  // All lengths fit in bytes. Broadcast the eight lengths, then sum a growing
  // prefix in each qword instead of performing three serial shift/add steps.
  __m512i prefix_mask = _mm512_setr_epi64(0xFF, 0xFFFF, 0xFFFFFF, 0xFFFFFFFFu,
                                        0xFFFFFFFFFF, 0xFFFFFFFFFFFF, 0xFFFFFFFFFFFFFF, -1);
  __m512i prefix = _mm512_broadcastq_epi64(_mm512_cvtepi64_epi8(lengths));
  prefix = _mm512_sad_epu8(_mm512_and_si512(prefix, prefix_mask), zero);

  __m512i incoming = _mm512_set1_epi64(incoming_bits);
  __m512i buffer = _mm512_set1_epi64(int64_t(incoming_buffer));
  __m512i starts = _mm512_add_epi64(_mm512_sub_epi64(prefix, lengths), incoming);
  __m512i shifts = _mm512_and_si512(starts, _mm512_set1_epi64(7));
  __m512i width = _mm512_set1_epi64(64);
  __m512i aligned = _mm512_sllv_epi64(codes, _mm512_sub_epi64(width, lengths));

  if constexpr (!ShortCodes) {
    // Four literals can occupy as few as four bits, so a partial output byte
    // may span two preceding groups. Extend each high-aligned group with the
    // preceding group's suffix before extracting the next lane's carry.
    __m512i incoming_high = _mm512_sllv_epi64(buffer, _mm512_sub_epi64(width, incoming));
    __m512i preceding_high = _mm512_alignr_epi64(aligned, incoming_high, 7);
    aligned = _mm512_shrdv_epi64(preceding_high, codes, lengths);
  }

  __m512i packed = _mm512_shldv_epi64(codes, _mm512_alignr_epi64(aligned, zero, 7), shifts);
  packed = _mm512_or_si512(packed, _mm512_maskz_set1_epi64(1, int64_t(incoming_buffer)));
  __m512i byte_counts = _mm512_srli_epi64(_mm512_add_epi64(shifts, lengths), 3);
  __m512i mask_shifts = _mm512_slli_epi64(_mm512_sub_epi64(_mm512_set1_epi64(8), byte_counts), 3);
  __mmask64 mask = _mm512_movepi8_mask(_mm512_srlv_epi64(_mm512_set1_epi64(-1), mask_shifts));
  _mm512_mask_compressstoreu_epi8(writer.data + incoming_bytes, mask, packed);

  // Keep the trailing partial byte in the scalar writer. The vector shift
  // also handles an empty tail (count 64) without a C++ shift by 64.
  uint32_t total = incoming_bits + uint32_t(_mm_extract_epi32(_mm512_extracti32x4_epi32(prefix, 3), 2));
  writer.bytes_written = incoming_bytes + (total >> 3u);
  writer.bits_in_buffer = total & 7u;
  __m128i tail = _mm_srl_epi64(_mm512_extracti32x4_epi32(aligned, 3), _mm_cvtsi32_si128(int(64u - writer.bits_in_buffer)));
  SIMD::storeh_64(&writer.buffer, SIMD::Vec2xU64{tail});
}

static BL_INLINE void write_literal_half_avx512_v2(FastBitWriter& writer, const __m256i& bits,
                                                 const __m256i& upper, const __m256i& nbits) noexcept {
  __m512i codes = _mm512_or_si512(_mm512_cvtepu8_epi16(bits), _mm512_slli_epi16(_mm512_cvtepu8_epi16(upper), 8));
  __m512i lengths = _mm512_cvtepu8_epi16(nbits);
  pack_literal_words(codes, lengths);
  pack_literal_dwords(codes, lengths);
  write_literal_chunks<false>(writer, codes, lengths);
}

static BL_INLINE void write_literals_avx512_v2(FastBitWriter& writer, const Avx512V2LiteralTable& table, const uint8_t* data) noexcept {
  __m512i bytes = _mm512_loadu_si512(data);
  // Near-zero residuals use indices 0..31; the fourteen middle groups use
  // indices 33..46. One shared index selects both code bytes and lengths.
  __m512i indices = _mm512_add_epi8(bytes, _mm512_set1_epi8(16));
  __mmask64 near_zero = _mm512_cmplt_epu8_mask(indices, _mm512_set1_epi8(32));
  if (uint64_t(near_zero) == ~uint64_t(0)) {
    __m512i codes = _mm512_permutexvar_epi8(indices, table.bits);
    __m512i lengths = _mm512_permutexvar_epi8(indices, table.nbits);
    pack_literal_bytes(codes, lengths);
    pack_literal_words(codes, lengths);
    pack_literal_dwords(codes, lengths);
    write_literal_chunks<true>(writer, codes, lengths);
    return;
  }

  __m512i nibble_mask = _mm512_set1_epi8(15);
  __m512i high_nibbles = _mm512_and_si512(_mm512_srli_epi16(bytes, 4), nibble_mask);
  indices = _mm512_mask_blend_epi8(near_zero, _mm512_add_epi8(high_nibbles, _mm512_set1_epi8(32)), indices);
  __m512i bits = _mm512_permutexvar_epi8(indices, table.bits);
  __m512i nbits = _mm512_permutexvar_epi8(indices, table.nbits);
  __m512i upper = _mm512_maskz_shuffle_epi8(~near_zero, table.reverse_nibble, _mm512_and_si512(bytes, nibble_mask));
  write_literal_half_avx512_v2(writer, _mm512_castsi512_si256(bits), _mm512_castsi512_si256(upper), _mm512_castsi512_si256(nbits));
  write_literal_half_avx512_v2(writer, _mm512_extracti64x4_epi64(bits, 1), _mm512_extracti64x4_epi64(upper, 1), _mm512_extracti64x4_epi64(nbits, 1));
}

static BL_INLINE void write_literal_span_avx512_v2(FastBitWriter& writer, const HuffmanTable& table,
                                                 const Avx512V2LiteralTable& literal_table, const uint8_t* data, uint32_t count) noexcept {
  while (count >= 64u) {
    write_literals_avx512_v2(writer, literal_table, data);
    data += 64u;
    count -= 64u;
  }
  write_literal_span_simd(writer, table, literal_table.tail, data, count);
}

static void BL_CDECL encode_rows_avx512_v2(
  FastBitWriter& writer, const HuffmanTable& table, const uint8_t* data,
  size_t bpl, uint32_t h, const uint64_t* masks
) noexcept {
  BL_ASSERT(bpl > 1u);
  BL_ASSERT(h > 0u);

  Avx512V2LiteralTable literal_table(table);
  size_t row_bytes = bpl - 1u;
  size_t mask_stride = repeat_mask_stride(bpl);

  for (uint32_t y = 0; y < h; y++, data += bpl) {
    writer.write(table.nbits[data[0]], table.bits[data[0]]);
    const uint8_t* row = data + 1u;
    const uint8_t* pending = row;
    const uint64_t* row_masks = masks ? masks + size_t(y) * mask_stride : nullptr;

    process_row_repeats(row, row_bytes, row_masks,
      [](const uint8_t* data, size_t count) noexcept { axl::maybe_unused(data, count); },
      [&](const uint8_t* run, size_t length) noexcept {
        if (!repeat_is_profitable(table, *run, length))
          return;
        write_literal_span_avx512_v2(writer, table, literal_table, pending, uint32_t(run - pending));
        write_repeat_run(writer, table, *run, length);
        pending = run + length;
      });
    write_literal_span_avx512_v2(writer, table, literal_table, pending, uint32_t(row + row_bytes - pending));
  }
}

} // {anonymous}
} // {bl::Png::FastEncoder}

namespace bl::Png::Ops {

void init_fast_encoder_avx512_v2(FunctionTable& ft) noexcept {
  ft.encode_rows_fast = FastEncoder::encode_rows_avx512_v2;
  ft.write_header_fast = FastEncoder::write_header_simd;
  ft.filter_with_masks[1] = filter_with_masks_simd_impl<1>;
  ft.filter_with_masks[2] = filter_with_masks_simd_impl<2>;
  ft.filter_with_masks[3] = filter_with_masks_simd_impl<3>;
  ft.filter_with_masks[4] = filter_with_masks_simd_impl<4>;
  ft.filter_with_masks[6] = filter_with_masks_simd_impl<6>;
  ft.filter_with_masks[8] = filter_with_masks_simd_impl<8>;
}

} // {bl::Png::Ops}

#endif // !BL_BUILD_NO_CODECS && BL_TARGET_OPT_AVX512_V2
