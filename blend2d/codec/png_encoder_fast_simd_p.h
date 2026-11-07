// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CODEC_PNG_ENCODER_FAST_SIMD_P_H_INCLUDED
#define BLEND2D_CODEC_PNG_ENCODER_FAST_SIMD_P_H_INCLUDED

#include <blend2d/codec/png_encoder_fast_p.h>
#include <blend2d/simd/simd_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_codec_impl
//! \{

namespace bl::Png::FastEncoder {
namespace {

#if defined(BL_SIMD_FEATURE_SWIZZLEV_U8) && !defined(BL_TARGET_BIG_ENDIAN)

// Retain fpnge's 16-byte literal lookup structure. The vector values are copied
// once so stores through the output byte pointer cannot alias the source tables.
struct SimdLiteralTable {
  SIMD::Vec16xU8 first_nbits;
  SIMD::Vec16xU8 first_bits;
  SIMD::Vec16xU8 last_nbits;
  SIMD::Vec16xU8 last_bits;
  SIMD::Vec16xU8 mid_bits;
  SIMD::Vec16xU8 reverse_nibble;
#if !BL_TARGET_ARCH_ARM
  SIMD::Vec16xU8 power_lo;
  SIMD::Vec16xU8 power_hi;
#endif

  BL_INLINE explicit SimdLiteralTable(const HuffmanTable& table) noexcept {
    using namespace SIMD;
    alignas(16) static constexpr uint8_t kReverseNibble[16] = {
      0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15
    };
#if !BL_TARGET_ARCH_ARM
    alignas(16) static constexpr uint8_t kPowerLo[16] = {
      1, 2, 4, 8, 16, 32, 64, 128, 0, 0, 0, 0, 0, 0, 0, 0
    };
    alignas(16) static constexpr uint8_t kPowerHi[16] = {
      0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 4, 8, 16, 32, 64, 128
    };
#endif

    BL_ASSERT(table.mid_nbits == 12u);
    first_nbits = loadu<Vec16xU8>(table.first16_nbits);
    first_bits = loadu<Vec16xU8>(table.first16_bits);
    last_nbits = loadu<Vec16xU8>(table.last16_nbits);
    last_bits = loadu<Vec16xU8>(table.last16_bits);
    mid_bits = loadu<Vec16xU8>(table.mid_lowbits);
    reverse_nibble = loadu<Vec16xU8>(kReverseNibble);
#if !BL_TARGET_ARCH_ARM
    power_lo = loadu<Vec16xU8>(kPowerLo);
    power_hi = loadu<Vec16xU8>(kPowerHi);
#endif
  }
};

// Pack adjacent Huffman codes into four 32-bit lanes. AArch64 can shift each
// high code directly by the preceding code's length. On x86, a lookup of
// 2^length and integer multiplication avoids floating-point operations.
static BL_INLINE SIMD::Vec4xU32 pack_literal_pairs(
  const SIMD::Vec8xU16& bits,
  const SIMD::Vec8xU16& nbits,
#if !BL_TARGET_ARCH_ARM
  const SIMD::Vec8xU16& powers,
#endif
  SIMD::Vec4xU32& pair_nbits
) noexcept {
  using namespace SIMD;
  Vec4xU32 mask = make128_u32<Vec4xU32>(0xFFFFu);
  Vec4xU32 lengths = vec_u32(nbits);
  Vec4xU32 codes = vec_u32(bits);
  Vec4xU32 lo_lengths = and_(lengths, mask);
  Vec4xU32 lo_bits = and_(codes, mask);
  Vec4xU32 hi_bits = srli_u32<16>(codes);

  pair_nbits = add_i32(lo_lengths, srli_u32<16>(lengths));
#if BL_TARGET_ARCH_ARM
  return or_(lo_bits, Vec4xU32{vshlq_u32(hi_bits.v, vreinterpretq_s32_u32(lo_lengths.v))});
#else
  Vec4xU32 factors = and_(vec_u32(powers), mask);
  return or_(lo_bits, mul_u32(hi_bits, factors));
#endif
}

static BL_INLINE void write_literals_simd(FastBitWriter& writer, const SimdLiteralTable& table, const SIMD::Vec16xU8& bytes) noexcept {
  using namespace SIMD;

  Vec16xU8 nibble_mask = make128_u8<Vec16xU8>(15u);
  Vec16xU8 low_nibbles = and_(bytes, nibble_mask);
  Vec16xU8 high_half = cmp_ge_u8(bytes, make128_u8<Vec16xU8>(128u));
  Vec16xU8 near_zero = cmp_lt_u8(add_i8(bytes, make128_u8<Vec16xU8>(16u)), make128_u8<Vec16xU8>(32u));
  Vec16xU8 nbits = blendv_u8(swizzlev_u8(table.first_nbits, low_nibbles),
                           swizzlev_u8(table.last_nbits, low_nibbles), high_half);
  Vec16xU8 bits = blendv_u8(swizzlev_u8(table.first_bits, low_nibbles),
                          swizzlev_u8(table.last_bits, low_nibbles), high_half);
  bool short_codes = test_mask_ones_i8(near_zero);
  Vec8xU16 codes_lo;
  Vec8xU16 codes_hi;

  if (short_codes) {
    // Residuals in [-16, 15] have codes no longer than eight bits.
    codes_lo = vec_u16(unpack_lo64_u8_u16(bits));
    codes_hi = vec_u16(unpack_hi64_u8_u16(bits));
  }
  else {
    // All middle literals have twelve-bit codes. The low eight bits depend on
    // the high nibble; the high four bits are the reversed low nibble.
#if BL_TARGET_ARCH_ARM
    Vec16xU8 high_nibbles = srli_u8<4>(bytes);
#else
    Vec16xU8 high_nibbles = and_(srli_u16<4>(bytes), nibble_mask);
#endif
    Vec16xU8 middle = swizzlev_u8(table.mid_bits, high_nibbles);
    Vec16xU8 upper = andnot(near_zero, swizzlev_u8(table.reverse_nibble, low_nibbles));
    bits = blendv_u8(middle, bits, near_zero);
    nbits = blendv_u8(make128_u8<Vec16xU8>(12u), nbits, near_zero);
    codes_lo = vec_u16(or_(unpack_lo64_u8_u16(bits), slli_u16<8>(unpack_lo64_u8_u16(upper))));
    codes_hi = vec_u16(or_(unpack_hi64_u8_u16(bits), slli_u16<8>(unpack_hi64_u8_u16(upper))));
  }

  Vec4xU32 counts_lo;
  Vec4xU32 counts_hi;
#if BL_TARGET_ARCH_ARM
  Vec4xU32 pairs_lo = pack_literal_pairs(codes_lo, vec_u16(unpack_lo64_u8_u16(nbits)), counts_lo);
  Vec4xU32 pairs_hi = pack_literal_pairs(codes_hi, vec_u16(unpack_hi64_u8_u16(nbits)), counts_hi);
#else
  Vec16xU8 power_lo = swizzlev_u8(table.power_lo, nbits);
  Vec16xU8 power_hi = swizzlev_u8(table.power_hi, nbits);
  Vec8xU16 powers_lo = vec_u16(or_(unpack_lo64_u8_u16(power_lo), slli_u16<8>(unpack_lo64_u8_u16(power_hi))));
  Vec8xU16 powers_hi = vec_u16(or_(unpack_hi64_u8_u16(power_lo), slli_u16<8>(unpack_hi64_u8_u16(power_hi))));
  Vec4xU32 pairs_lo = pack_literal_pairs(codes_lo, vec_u16(unpack_lo64_u8_u16(nbits)), powers_lo, counts_lo);
  Vec4xU32 pairs_hi = pack_literal_pairs(codes_hi, vec_u16(unpack_hi64_u8_u16(nbits)), powers_hi, counts_hi);
#endif

  alignas(16) uint32_t pair_bits[8];
  alignas(16) uint32_t pair_nbits[8];
  storeu(pair_bits, pairs_lo);
  storeu(pair_bits + 4, pairs_hi);
  storeu(pair_nbits, counts_lo);
  storeu(pair_nbits + 4, counts_hi);

  if (short_codes) {
    // Eight short codes fit in at most 64 bits. Usually this emits just two
    // writer calls per vector; the full-word path handles exactly 64 bits.
    for (uint32_t i = 0; i < 8; i += 4) {
      uint32_t lo_count = pair_nbits[i] + pair_nbits[i + 1u];
      uint32_t hi_count = pair_nbits[i + 2u] + pair_nbits[i + 3u];
      uint64_t lo = uint64_t(pair_bits[i]) | (uint64_t(pair_bits[i + 1u]) << pair_nbits[i]);
      uint64_t hi = uint64_t(pair_bits[i + 2u]) | (uint64_t(pair_bits[i + 3u]) << pair_nbits[i + 2u]);
      uint32_t count = lo_count + hi_count;
      if (count < 64u) {
        writer.write(count, lo | (hi << lo_count));
      }
      else {
        BL_ASSERT(lo_count == 32u && hi_count == 32u);
        writer.write_full_word(lo | (hi << lo_count));
      }
    }
  }
  else {
    // Four long codes occupy at most 48 bits.
    for (uint32_t i = 0; i < 8; i += 2) {
      uint32_t count = pair_nbits[i];
      uint64_t packed = uint64_t(pair_bits[i]) | (uint64_t(pair_bits[i + 1u]) << count);
      writer.write(count + pair_nbits[i + 1u], packed);
    }
  }
}

#endif // BL_SIMD_FEATURE_SWIZZLEV_U8 && !BL_TARGET_BIG_ENDIAN

static void BL_CDECL write_header_simd(FastBitWriter& writer, const HuffmanTable& table) noexcept {
#if BL_TARGET_ARCH_ARM == 64 && !defined(BL_TARGET_BIG_ENDIAN)
  alignas(16) static constexpr uint8_t kReverseNibble[16] = {
    0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15
  };
  static constexpr uint8_t kCodeLengthOrder[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
  };
  static constexpr uint64_t kPrecode = []() constexpr noexcept {
    uint64_t bits = 0;
    for (uint32_t i = 0; i < 19u; i++)
      bits |= uint64_t(kCodeLengthOrder[i] < 16u ? 4u : 0u) << (i * 3u);
    return bits;
  }();

  writer.write(14u, 29u | (15u << 10u)); // All literals/lengths, one distance code.
  writer.write(57u, kPrecode);

  // Each code length is represented by a reversed four-bit precode symbol.
  // Pack sixteen lengths into one 64-bit word with one lookup and narrowing.
  uint8x16_t reverse = vld1q_u8(kReverseNibble);
  uint32_t i = 0;
  for (; i + 16u <= 286u; i += 16u) {
    uint8x16_t codes = vqtbl1q_u8(reverse, vld1q_u8(table.nbits + i));
    uint16x8_t words = vreinterpretq_u16_u8(codes);
    uint8x8_t packed = vmovn_u16(vorrq_u16(words, vshrq_n_u16(words, 4)));
    writer.write_full_word(vget_lane_u64(vreinterpret_u64_u8(packed), 0));
  }

  uint64_t tail = 0;
  uint32_t tail_nbits = 0;
  for (; i < 286u; i++, tail_nbits += 4u)
    tail |= uint64_t(kReverseNibble[table.nbits[i]]) << tail_nbits;
  writer.write(tail_nbits, tail);
  writer.write(4u, 8u); // The single distance symbol has a one-bit code.
#else
  write_header_ref(writer, table);
#endif
}

static BL_INLINE void write_literal_span_simd(
  FastBitWriter& writer,
  const HuffmanTable& table,
#if defined(BL_SIMD_FEATURE_SWIZZLEV_U8) && !defined(BL_TARGET_BIG_ENDIAN)
  const SimdLiteralTable& literal_table,
#endif
  const uint8_t* data,
  uint32_t count
) noexcept {
  using namespace SIMD;

#if defined(BL_SIMD_FEATURE_SWIZZLEV_U8) && !defined(BL_TARGET_BIG_ENDIAN)
  while (count >= 16u) {
    write_literals_simd(writer, literal_table, loadu<Vec16xU8>(data));
    data += 16u;
    count -= 16u;
  }
#endif

  // Four scalar codes fit in at most 48 bits, including middle literals.
  while (count) {
    uint32_t n = bl_min<uint32_t>(count, 4u);
    uint32_t nbits = 0;
    uint64_t bits = 0;
    for (uint32_t i = 0; i < n; i++) {
      bits |= uint64_t(table.bits[data[i]]) << nbits;
      nbits += table.nbits[data[i]];
    }
    writer.write(nbits, bits);
    data += n;
    count -= n;
  }
}

static void BL_CDECL encode_rows_simd(
  FastBitWriter& writer,
  const HuffmanTable& table,
  const uint8_t* data,
  uint32_t bpl,
  uint32_t h,
  const uint64_t* masks
) noexcept {
  BL_ASSERT(bpl > 1u);
  BL_ASSERT(h > 0u);

#if defined(BL_SIMD_FEATURE_SWIZZLEV_U8) && !defined(BL_TARGET_BIG_ENDIAN)
  SimdLiteralTable literal_table(table);
#endif

  uint32_t row_bytes = bpl - 1u;
  size_t mask_stride = repeat_mask_stride(bpl);
  for (uint32_t y = 0; y < h; y++, data += bpl) {
    // A filter marker is always a literal, separate from the residual runs.
    writer.write(table.nbits[data[0]], table.bits[data[0]]);
    const uint8_t* row = data + 1u;
    const uint8_t* pending = row;
    const uint64_t* row_masks = masks ? masks + size_t(y) * mask_stride : nullptr;
    process_row_repeats(row, row_bytes, row_masks,
      [](const uint8_t*, uint32_t) noexcept {},
      [&](const uint8_t* run, uint32_t length) noexcept {
        if (!repeat_is_profitable(table, *run, length))
          return;

        // Leave rejected runs in the pending literal span so they do not
        // interrupt SIMD packing of the surrounding literals.
        write_literal_span_simd(writer, table,
#if defined(BL_SIMD_FEATURE_SWIZZLEV_U8) && !defined(BL_TARGET_BIG_ENDIAN)
          literal_table,
#endif
          pending, uint32_t(run - pending));
        write_repeat_run(writer, table, *run, length);
        pending = run + length;
      });
    write_literal_span_simd(writer, table,
#if defined(BL_SIMD_FEATURE_SWIZZLEV_U8) && !defined(BL_TARGET_BIG_ENDIAN)
      literal_table,
#endif
      pending, uint32_t(row + row_bytes - pending));
  }
}

} // {anonymous}
} // {bl::Png::FastEncoder}

//! \}
//! \endcond

#endif // BLEND2D_CODEC_PNG_ENCODER_FAST_SIMD_P_H_INCLUDED
