// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../array_p.h"
#include "../runtime_p.h"
#include "../compression/deflatedecoder_p.h"
#include "../compression/deflatedecoderfast_p.h"
#include "../compression/deflatedecoderutils_p.h"
#include "../support/memops_p.h"
#include "../support/intops_p.h"
#include "../support/ptrops_p.h"

// Decoding Notes:
//
// Some parts of DEFLATE decoder provided by Blend2D are based on libdeflate design:
//
//   - The `buildDecodeTable()` function uses the same algorithm and implementation that libdeflate uses, with minor
//     modifications, but no further optimizations yet (I think some bit-scans can be used to remove few trivial
//     loops, but I'm not sure it would be significant).
//
//   - The `buildFastTable()` function is an addition to `buildDecodeTable()`, which adds literal pairs to literal
//     entries, which can host two literals. Ideally this should be incorporated into `buildDecodeTable()` so it's
//     always available, but I didn't want to slow it down in case literal pairs are not used (for example when fast
//     loop never enters or `buildFastTable()` is not called due to a small input buffer).
//
//   - The rest of the code uses libdeflate ideas, but it's original - even the decode entry struct uses a completely
//     different layout. The main reason is that libdeflate decompressor only works with a single contiguous input
//     chunk, whereas Blend2D needs the support for streaming so PNG images with multiple 'IDAT' chunks can be decoded
//     without additional overhead (like copying 'IDAT' content to a single buffer).
//
//   - Some optimizations that libdeflate doesn't do:
//
//       - End of table and invalid symbol are always handled via a sub-table, even when the code is smaller than the
//         table size - in that case the subtable simply links itself.
//
//       - There is no sub-table entry type - if the entry is not literal or length (or offset / symbol in non-main
//         table) it's ALWAYS a sub-table pointer, which could point to itself. This simplifies some logic in the
//         decoder.
//
//       - The fast loop precalculates the number of SAFE iterations, which is decremented by one by literal symbols
//         and decremented appropriately by offset+length symbols.
//
//   - Some comments (including the comment below) were copied from libdeflate, because they very well explain what
//     the decompressor does. I would advise anyone who wants to write a DEFLATE decoder to look into libdeflate,
//     because it's probably the top library that not just implements a very good decoder and encoder, but it's also
//     documents the code very well.
//
// The fastest way to decode Huffman-encoded data is basically to use a decode table that maps the maximum table bits
// of data to their symbol or pair of symbols in case 2 literals fit. Each entry in a decode table maps to the symbol
// whose codeword is a prefix of 'i'. A symbol with codeword length 'n' has '2**(TableBits-n)' entries in the table.
//
// Ideally, TableBits and the maximum codeword length would be the same; some compression formats are designed with
// this goal in mind. Unfortunately, in DEFLATE, the maximum litlen and offset codeword lengths are 15 bits, which
// is too large to be practical. For example a 15-bit lookup table would mean 32k entries, which would take a lot of
// time to build. Since it's not that much larger, the workaround is to use a single level of subtables: entries for
// prefixes of codewords longer than TableBits contain an index to the appropriate subtable along with the number of
// bits it is indexed with.
//
// The most efficient way to allocate subtables is to allocate them dynamically after the main table. The worst-case
// number of table entries needed, including subtables, is pre-computable; for example by `enough` tool from Zlib.
//
// A useful optimization is to store the codeword lengths in the decode table so that they don't have to be looked up
// by indexing a separate table that maps symbols to their codeword lengths. We basically do this; however, for the
// litlen and offset codes we also implement some DEFLATE-specific optimizations that build in the consideration of
// the "extra bits" and the literal/length/end-of-block division. For the exact decode table entry format we use, see
// the definitions below.

namespace bl::Compression::Deflate {

// bl::Compression::Deflate - Constants
// ====================================

// Static part of pre-code entries (the pre-code decode table never has subtables).
static constexpr DecodeEntry kPrecodeDecodeResults[] = {
  #define ENTRY(value, repeat, extra) {                       \
    (uint32_t(value ) << DecodeEntry::kPrecodeValueOffset ) | \
    (uint32_t(repeat) << DecodeEntry::kPrecodeRepeatOffset) | \
    (uint32_t(extra ) << DecodeEntry::kFullLengthOffset   )   \
  }

  ENTRY(0 , 1, 0), ENTRY(1 , 1, 0), ENTRY(2 , 1 , 0), ENTRY(3 , 1, 0),
  ENTRY(4 , 1, 0), ENTRY(5 , 1, 0), ENTRY(6 , 1 , 0), ENTRY(7 , 1, 0),
  ENTRY(8 , 1, 0), ENTRY(9 , 1, 0), ENTRY(10, 1 , 0), ENTRY(11, 1, 0),
  ENTRY(12, 1, 0), ENTRY(13, 1, 0), ENTRY(14, 1 , 0), ENTRY(15, 1, 0),
  ENTRY(16, 3, 2), ENTRY(17, 3, 3), ENTRY(18, 11, 7)

  #undef ENTRY
};

// Literals+Length decode entries.
static constexpr DecodeEntry kLitLenDecodeResults[] = {
  // Literal entries.
  #define ENTRY(value) {(uint32_t(value) << DecodeEntry::kPayloadOffset) | uint32_t(1u << DecodeEntry::kLiteralCountOffset) | DecodeEntry::kLiteralFlag}

  ENTRY(0)  , ENTRY(1)  , ENTRY(2)  , ENTRY(3)  , ENTRY(4)  , ENTRY(5)  , ENTRY(6)  , ENTRY(7)  ,
  ENTRY(8)  , ENTRY(9)  , ENTRY(10) , ENTRY(11) , ENTRY(12) , ENTRY(13) , ENTRY(14) , ENTRY(15) ,
  ENTRY(16) , ENTRY(17) , ENTRY(18) , ENTRY(19) , ENTRY(20) , ENTRY(21) , ENTRY(22) , ENTRY(23) ,
  ENTRY(24) , ENTRY(25) , ENTRY(26) , ENTRY(27) , ENTRY(28) , ENTRY(29) , ENTRY(30) , ENTRY(31) ,
  ENTRY(32) , ENTRY(33) , ENTRY(34) , ENTRY(35) , ENTRY(36) , ENTRY(37) , ENTRY(38) , ENTRY(39) ,
  ENTRY(40) , ENTRY(41) , ENTRY(42) , ENTRY(43) , ENTRY(44) , ENTRY(45) , ENTRY(46) , ENTRY(47) ,
  ENTRY(48) , ENTRY(49) , ENTRY(50) , ENTRY(51) , ENTRY(52) , ENTRY(53) , ENTRY(54) , ENTRY(55) ,
  ENTRY(56) , ENTRY(57) , ENTRY(58) , ENTRY(59) , ENTRY(60) , ENTRY(61) , ENTRY(62) , ENTRY(63) ,
  ENTRY(64) , ENTRY(65) , ENTRY(66) , ENTRY(67) , ENTRY(68) , ENTRY(69) , ENTRY(70) , ENTRY(71) ,
  ENTRY(72) , ENTRY(73) , ENTRY(74) , ENTRY(75) , ENTRY(76) , ENTRY(77) , ENTRY(78) , ENTRY(79) ,
  ENTRY(80) , ENTRY(81) , ENTRY(82) , ENTRY(83) , ENTRY(84) , ENTRY(85) , ENTRY(86) , ENTRY(87) ,
  ENTRY(88) , ENTRY(89) , ENTRY(90) , ENTRY(91) , ENTRY(92) , ENTRY(93) , ENTRY(94) , ENTRY(95) ,
  ENTRY(96) , ENTRY(97) , ENTRY(98) , ENTRY(99) , ENTRY(100), ENTRY(101), ENTRY(102), ENTRY(103),
  ENTRY(104), ENTRY(105), ENTRY(106), ENTRY(107), ENTRY(108), ENTRY(109), ENTRY(110), ENTRY(111),
  ENTRY(112), ENTRY(113), ENTRY(114), ENTRY(115), ENTRY(116), ENTRY(117), ENTRY(118), ENTRY(119),
  ENTRY(120), ENTRY(121), ENTRY(122), ENTRY(123), ENTRY(124), ENTRY(125), ENTRY(126), ENTRY(127),
  ENTRY(128), ENTRY(129), ENTRY(130), ENTRY(131), ENTRY(132), ENTRY(133), ENTRY(134), ENTRY(135),
  ENTRY(136), ENTRY(137), ENTRY(138), ENTRY(139), ENTRY(140), ENTRY(141), ENTRY(142), ENTRY(143),
  ENTRY(144), ENTRY(145), ENTRY(146), ENTRY(147), ENTRY(148), ENTRY(149), ENTRY(150), ENTRY(151),
  ENTRY(152), ENTRY(153), ENTRY(154), ENTRY(155), ENTRY(156), ENTRY(157), ENTRY(158), ENTRY(159),
  ENTRY(160), ENTRY(161), ENTRY(162), ENTRY(163), ENTRY(164), ENTRY(165), ENTRY(166), ENTRY(167),
  ENTRY(168), ENTRY(169), ENTRY(170), ENTRY(171), ENTRY(172), ENTRY(173), ENTRY(174), ENTRY(175),
  ENTRY(176), ENTRY(177), ENTRY(178), ENTRY(179), ENTRY(180), ENTRY(181), ENTRY(182), ENTRY(183),
  ENTRY(184), ENTRY(185), ENTRY(186), ENTRY(187), ENTRY(188), ENTRY(189), ENTRY(190), ENTRY(191),
  ENTRY(192), ENTRY(193), ENTRY(194), ENTRY(195), ENTRY(196), ENTRY(197), ENTRY(198), ENTRY(199),
  ENTRY(200), ENTRY(201), ENTRY(202), ENTRY(203), ENTRY(204), ENTRY(205), ENTRY(206), ENTRY(207),
  ENTRY(208), ENTRY(209), ENTRY(210), ENTRY(211), ENTRY(212), ENTRY(213), ENTRY(214), ENTRY(215),
  ENTRY(216), ENTRY(217), ENTRY(218), ENTRY(219), ENTRY(220), ENTRY(221), ENTRY(222), ENTRY(223),
  ENTRY(224), ENTRY(225), ENTRY(226), ENTRY(227), ENTRY(228), ENTRY(229), ENTRY(230), ENTRY(231),
  ENTRY(232), ENTRY(233), ENTRY(234), ENTRY(235), ENTRY(236), ENTRY(237), ENTRY(238), ENTRY(239),
  ENTRY(240), ENTRY(241), ENTRY(242), ENTRY(243), ENTRY(244), ENTRY(245), ENTRY(246), ENTRY(247),
  ENTRY(248), ENTRY(249), ENTRY(250), ENTRY(251), ENTRY(252), ENTRY(253), ENTRY(254), ENTRY(255),

  #undef ENTRY

  // End of block entry.
  {DecodeEntry::kEndOfBlockFlag},

  // Length entries.
  #define ENTRY(base, extra) {                            \
    (uint32_t(base) << DecodeEntry::kPayloadOffset)     | \
    (uint32_t(extra) << DecodeEntry::kFullLengthOffset) | \
    DecodeEntry::kOffOrLenFlag                            \
  }

  ENTRY(3  , 0) , ENTRY(4  , 0) , ENTRY(5  , 0) , ENTRY(6  , 0),
  ENTRY(7  , 0) , ENTRY(8  , 0) , ENTRY(9  , 0) , ENTRY(10 , 0),
  ENTRY(11 , 1) , ENTRY(13 , 1) , ENTRY(15 , 1) , ENTRY(17 , 1),
  ENTRY(19 , 2) , ENTRY(23 , 2) , ENTRY(27 , 2) , ENTRY(31 , 2),
  ENTRY(35 , 3) , ENTRY(43 , 3) , ENTRY(51 , 3) , ENTRY(59 , 3),
  ENTRY(67 , 4) , ENTRY(83 , 4) , ENTRY(99 , 4) , ENTRY(115, 4),
  ENTRY(131, 5) , ENTRY(163, 5) , ENTRY(195, 5) , ENTRY(227, 5),
  ENTRY(258, 0) ,

  #undef ENTRY

  // These two entries are invalid - if they appear in a bit-stream the decoder should stop and report invalid data.
  {DecodeEntry::kEndOfBlockFlag | DecodeEntry::kEndOfBlockInvalidFlag},
  {DecodeEntry::kEndOfBlockFlag | DecodeEntry::kEndOfBlockInvalidFlag}
};

static const DecodeEntry kOffsetDecodeResults[] = {
  #define ENTRY(base, extra) {                            \
    (uint32_t(base) << DecodeEntry::kPayloadOffset)     | \
    (uint32_t(extra) << DecodeEntry::kFullLengthOffset) | \
    DecodeEntry::kOffOrLenFlag                            \
  }

  ENTRY(1    , 0)  , ENTRY(2    , 0)  , ENTRY(3    , 0)  , ENTRY(4    , 0)  ,
  ENTRY(5    , 1)  , ENTRY(7    , 1)  , ENTRY(9    , 2)  , ENTRY(13   , 2)  ,
  ENTRY(17   , 3)  , ENTRY(25   , 3)  , ENTRY(33   , 4)  , ENTRY(49   , 4)  ,
  ENTRY(65   , 5)  , ENTRY(97   , 5)  , ENTRY(129  , 6)  , ENTRY(193  , 6)  ,
  ENTRY(257  , 7)  , ENTRY(385  , 7)  , ENTRY(513  , 8)  , ENTRY(769  , 8)  ,
  ENTRY(1025 , 9)  , ENTRY(1537 , 9)  , ENTRY(2049 , 10) , ENTRY(3073 , 10) ,
  ENTRY(4097 , 11) , ENTRY(6145 , 11) , ENTRY(8193 , 12) , ENTRY(12289, 12) ,
  ENTRY(16385, 13) , ENTRY(24577, 13) ,

  #undef ENTRY

  // These two entries are invalid - if they appear in a bit-stream the decoder should stop and report invalid data.
  {DecodeEntry::kEndOfBlockFlag | DecodeEntry::kEndOfBlockInvalidFlag},
  {DecodeEntry::kEndOfBlockFlag | DecodeEntry::kEndOfBlockInvalidFlag}
};

// bl::Compression::Deflate - Decode Table Building
// ================================================

namespace {

BL_INLINE_NODEBUG DecodeEntry makeTopEntry(DecodeEntry entry, uint32_t length) noexcept {
  // Base value is an entry without any flags used to build entries.
  uint32_t baseLength = length << DecodeEntry::kBaseLengthOffset;
  uint32_t fullLength = length << DecodeEntry::kFullLengthOffset;

  if (DecoderUtils::isEndOfBlock(entry)) {
    baseLength = 0;
  }

  return DecodeEntry{entry.value + (fullLength | baseLength)};
}

BL_INLINE_NODEBUG DecodeEntry makeSubLink(uint32_t startIndex, uint32_t baseLength, uint32_t fullLength) noexcept {
  return DecodeEntry{
    (baseLength << DecodeEntry::kBaseLengthOffset) |
    (fullLength << DecodeEntry::kFullLengthOffset) |
    (startIndex << DecodeEntry::kPayloadOffset   )
  };
}

BL_INLINE_NODEBUG DecodeEntry makeSubEntry(DecodeEntry entry, uint32_t length) noexcept {
  // Base value is an entry without any flags used to build entries.
  uint32_t baseLength = length << DecodeEntry::kBaseLengthOffset;
  uint32_t fullLength = length << DecodeEntry::kFullLengthOffset;

  return DecodeEntry{(entry.value & 0xFFFFFF3F) + fullLength + baseLength};
}

//! Decode table type.
enum class DecodeTableType : uint32_t {
  kPrecode,
  kLitLen,
  kOffset
};

//! Build a table for fast decoding of symbols from a Huffman code. As input, this function takes the codeword length
//! of each symbol which may be used in the code. As output, it produces a decode table for the canonical Huffman code
//! described by the codeword lengths. The decode table is built with the assumption that it will be indexed with bit
//! reversed codewords, where the low-order bit is the first bit of the codeword. This format is used for all Huffman
//! codes in DEFLATE.
//!
//! \param decode_table The array in which the decode table will be generated. This array must have sufficient length;
//!   see the definition of the ENOUGH numbers.
//!
//! \param lens An array which provides, for each symbol, the length of the corresponding codeword in bits, or 0 if the
//!   symbol is unused. This may alias `decode_table`, since nothing is written to `decode_table` until all `lens` have
//!   been consumed. All codeword lengths are assumed to be `<= max_codeword_len` but are otherwise considered untrusted.
//!   If they do not form a valid Huffman code, then the decode table is not built and `false` is returned.
//!
//! \param num_syms The number of symbols in the code, including all unused symbols.
//!
//! \param decode_results An array which gives the incomplete decode result for each symbol. The needed values in this
//!   array will be combined with codeword lengths to make the final decode table entries.
//!
//! \param table_bits The log base-2 of the number of main table entries to use. If `table_bits_ret != nullptr`, then
//!   `table_bits` is treated as a maximum value and it will be decreased if a smaller table would be sufficient.
//!
//! \param max_codeword_len The maximum allowed codeword length for this Huffman code. Must be `<= kMaxCodeWordLen`.
//!
//! \param sorted_syms A temporary array of length @num_syms.
//!
//! Returns `true` if successful; `false` if the codeword lengths do not form a valid Huffman code.
static BL_NOINLINE DecodeTableInfo buildDecodeTable(
  DecodeEntry decode_table[],
  const uint8_t lens[],
  uint32_t num_syms,
  const DecodeEntry decode_results[],
  uint32_t max_table_bits,
  uint32_t max_codeword_len,
  DecodeTableType table_type
) noexcept {
  // Count how many codewords have each length, including 0.
  uint32_t len_counts[kMaxCodeWordLen + 1] {};
  uint32_t len_mask = 0;
  uint32_t literal_mask = 0;

  for (uint32_t sym = 0; sym < num_syms; sym++) {
    uint32_t len = lens[sym];
    len_counts[len]++;

    len_mask |= 1u << len;

    if (sym < 256u) {
      literal_mask |= 1u << len;
    }
  }
  literal_mask &= ~uint32_t(1);

  // Determine the actual maximum codeword length that was used, and decrease table_bits to it if allowed.
  max_codeword_len = blMin<uint32_t>(max_codeword_len, 32u - IntOps::clz(len_mask | 1u));
  uint32_t table_bits = blMax<uint32_t>(blMin(max_table_bits, max_codeword_len), 1u);

  // Sort the symbols primarily by increasing codeword length and secondarily by increasing symbol value;
  // or equivalently by their codewords in lexicographic order, since a canonical code is assumed.
  //
  // For efficiency, also compute 'codespace_used' in the same pass over `len_counts[]` used to build
  // `offsets[]` for sorting.

  uint32_t offsets[kMaxCodeWordLen + 1];
  offsets[0] = 0;
  offsets[1] = len_counts[0];

  // Ensure that 'codespace_used' cannot overflow.
  uint32_t codespace_used = 0; // codespace used out of '2^max_codeword_len'.
  BL_STATIC_ASSERT(~uint32_t(0) / (1u << (kMaxCodeWordLen - 1)) >= kMaxSymbolCount);

  {
    uint32_t len;
    for (len = 1; len < max_codeword_len; len++) {
      offsets[len + 1] = offsets[len] + len_counts[len];
      codespace_used = (codespace_used << 1) + len_counts[len];
    }

    codespace_used = (codespace_used << 1) + len_counts[len];
  }

  uint16_t sorted_syms_data[kMaxSymbolCount];
  for (uint32_t sym = 0; sym < num_syms; sym++) {
    uint32_t len = lens[sym];
    sorted_syms_data[offsets[len]++] = uint16_t(sym);
  }

  // Skip unused symbols.
  uint16_t* sorted_syms = sorted_syms_data + offsets[0];

  // lens[] is done being used, so we can write to decode_table[] now.

  // Check whether the lengths form a complete code (exactly fills the codespace), an incomplete code (doesn't
  // fill the codespace), or an overfull code (overflows the codespace). A codeword of length 'n' uses proportion
  // '1/(2^n)' of the codespace. An overfull code is nonsensical, so is considered invalid. An incomplete code
  // is considered valid only in two specific cases; see below.

  // Overfull code?
  if (BL_UNLIKELY(codespace_used > (1u << max_codeword_len))) {
    return DecodeTableInfo{};
  }

  // Incomplete code?
  if (BL_UNLIKELY(codespace_used < (1u << max_codeword_len))) {
    // The DEFLATE RFC explicitly allows the offset code to be incomplete in two cases: a code containing just
    // 1 codeword, if that codeword has length 1; and a code containing no codewords. Note: the list of offset
    // codeword lengths is always non-empty, but lengths of 0 don't count as codewords.
    //
    // The RFC doesn't say whether the same cases are allowed for the litlen and pre-codes. It's actually
    // impossible for no symbols to be used from these codes; however, it's technically possible for only one
    // symbol to be used. Zlib allows 1 codeword for the litlen code, but not the pre-code. The RFC also doesn't
    // say whether, when there is 1 codeword, that codeword is '0' or '1'. zlib uses '0'.
    uint32_t tableSize = 1u << table_bits;
    DecodeEntry firstEntry;
    DecodeEntry invalidEntry = makeTopEntry(DecodeEntry{DecodeEntry::kEndOfBlockFlag | DecodeEntry::kEndOfBlockInvalidFlag}, 1u);

    if (codespace_used == 0u) {
      // Only allow empty code to be used with offset table, like Zlib does. Precode and LitLen tables must use
      // at least one symbol each.
      if (table_type != DecodeTableType::kOffset) {
        return DecodeTableInfo{};
      }

      firstEntry = invalidEntry;
    }
    else {
      // Allow codes with a single used symbol for litlen and offset tables, but not for the precode table.
      if (table_type == DecodeTableType::kPrecode) {
        return DecodeTableInfo{};
      }

      if (codespace_used != (1u << (max_codeword_len - 1)) || len_counts[1] != 1) {
        return DecodeTableInfo{};
      }

      firstEntry = makeTopEntry(decode_results[sorted_syms[0]], 1u);
    }

    for (uint32_t i = 0; i < tableSize; i += 2) {
      decode_table[i + 0u] = firstEntry;
      decode_table[i + 1u] = invalidEntry;
    }
  }
  else {
    // The lengths form a complete code. Now, enumerate the codewords in lexicographic order and fill the decode
    // table entries for each one.
    //
    // First, process all codewords with len <= table_bits. Each one gets '2^(table_bits-len)' direct entries in
    // the table.
    //
    // Since DEFLATE uses bit-reversed codewords, these entries aren't consecutive but rather are spaced '2^len'
    // entries apart. This makes filling them naively somewhat awkward and inefficient, since strided stores are
    // less cache-friendly and preclude the use of word or vector-at-a-time stores to fill multiple entries per
    // instruction.
    //
    // To optimize this, we incrementally double the table size. When processing codewords with length 'len', the
    // table is treated as having only '2^len' entries, so each codeword uses just one entry. Then, each time 'len'
    // is incremented, the table size is doubled and the first half is copied to the second half. This significantly
    // improves performance over naively doing strided stores.
    //
    // Note that some entries copied for each table doubling may not have been initialized yet, but it doesn't matter
    // since they're guaranteed to be initialized later (because the Huffman code is complete).

    uint32_t codeword = 0; // Current codeword, bit-reversed.
    uint32_t len = 1;      // Current codeword length in bits.
    uint32_t count;        // Num codewords remaining with this length.

    while ((count = len_counts[len]) == 0) {
      len++;
    }

    // End index of current table.
    uint32_t cur_table_end = 1u << len;

    while (len <= table_bits) {
      // Process `count` of codewords with length `len` bits.
      do {
        // Fill the first entry for the current codeword.
        DecodeEntry entry = makeTopEntry(decode_results[*sorted_syms++], len);
        decode_table[codeword] = entry;

        if (codeword == cur_table_end - 1) {
          // Last codeword (all 1's).
          for (; len < table_bits; len++) {
            memcpy(&decode_table[cur_table_end], decode_table, cur_table_end * sizeof(DecodeEntry));
            cur_table_end <<= 1;
          }
          goto Done;
        }

        // To advance to the lexicographically next codeword in the canonical code, the codeword must be
        // incremented, then 0's must be appended to the codeword as needed to match the next codeword's length.
        //
        // Since the codeword is bit-reversed, appending 0's is a no-op. However, incrementing it is nontrivial.
        // To do so efficiently, use the 'count leading zeros' instruction to find the last (highest order) zero
        // bit in the codeword, set it, and clear any later (higher order) one bits. To use count leading zeros
        // instruction the bits in codeword have to be flipped as the instruction finds the first non-zero bit.
        uint32_t bit = 1u << (31 - IntOps::clz(codeword ^ (cur_table_end - 1)));
        codeword &= bit - 1;
        codeword |= bit;
      } while (--count);

      // Advance to the next codeword length.
      do {
        if (++len <= table_bits) {
          memcpy(&decode_table[cur_table_end], decode_table, cur_table_end * sizeof(DecodeEntry));
          cur_table_end <<= 1;
        }
      } while ((count = len_counts[len]) == 0);
    }

    // Process codewords with len > table_bits - these require subtables.
    cur_table_end = 1u << table_bits;

    uint32_t subtable_start = 0;            // start index of current subtable.
    uint32_t subtable_prefix = 0xFFFFFFFFu; // codeword prefix of current subtable.

    for (;;) {
      // Start a new subtable if the first 'table_bits' bits of the codeword don't match the prefix of the current
      // subtable.
      if ((codeword & DecoderUtils::mask32(table_bits)) != subtable_prefix) {
        subtable_prefix = codeword & DecoderUtils::mask32(table_bits);
        subtable_start = cur_table_end;

        // Calculate the subtable length. If the codeword has length 'table_bits + n', then the subtable needs
        // '2^n' entries. But it may need more; if fewer than '2^n' codewords of length 'table_bits + n' remain,
        // then the length will need to be incremented to bring in longer codewords until the subtable can be
        // completely filled. Note that because the Huffman code is complete, it will always be possible to fill
        // the subtable eventually.
        uint32_t subtable_bits = len - table_bits;
        codespace_used = count;

        BL_NOUNROLL
        while (codespace_used < (1u << subtable_bits)) {
          subtable_bits++;
          codespace_used = (codespace_used << 1) + len_counts[table_bits + subtable_bits];
        }

        cur_table_end = subtable_start + (1u << subtable_bits);
        // Create the entry that points from the main table to the subtable.
        decode_table[subtable_prefix] = makeSubLink(subtable_start, table_bits, table_bits + subtable_bits);
      }

      // Fill the subtable entries for the current codeword.
      DecodeEntry entry = makeSubEntry(decode_results[*sorted_syms++], len);
      uint32_t i = subtable_start + (codeword >> table_bits);
      uint32_t stride = 1u << (len - table_bits);

      do {
        decode_table[i] = entry;
        i += stride;
      } while (i < cur_table_end);

      // Advance to the next codeword.

      // Last codeword (all 1's)?
      if (codeword == DecoderUtils::mask32(len)) {
        break;
      }

      uint32_t bit = 1u << (31 - IntOps::clz(codeword ^ DecoderUtils::mask32(len)));
      codeword &= bit - 1;
      codeword |= bit;

      count--;
      while (count == 0) {
        count = len_counts[++len];
      }
    }
  }

  // Merge multiple literals into a single DecodeEntry, when possible.
Done:
  return DecodeTableInfo{
    uint8_t(table_bits),
    uint8_t(max_codeword_len),
    uint16_t(literal_mask)
  };
}

#if BL_TARGET_ARCH_BITS >= 64
static BL_NOINLINE uint32_t buildFastTable(DecodeTableInfo tableInfo, DecodeEntry decode_table[]) noexcept {
  // Fast table bits represents the final "fast" table size in bits (8 -> 256 entries, 9 -> 512 entries, etc...).
  uint32_t fastTableBits = tableInfo.tableBits;

  // If the table has no literals, don't build a fast table!
  if (tableInfo.literalMask == 0) {
    return fastTableBits;
  }

  uint32_t minLiteralSize = IntOps::ctz(tableInfo.literalMask | (1u << fastTableBits));

  if (fastTableBits < kDecoderLitLenTableBits) {
    // If the current table bits is less than maximum table bits then don't grow it so much as we
    // could spend more time building the fast table than actually decoding the Huffman stream.
    fastTableBits = blMin<uint32_t>(blMax<uint32_t>(fastTableBits + 1, 6), kDecoderLitLenTableBits);
  }

  // This is the table mask of a current table.
  uint32_t regularTableMask = DecoderUtils::mask32(tableInfo.tableBits);
  uint32_t maxMergeableSize = fastTableBits - blMin<uint32_t>(minLiteralSize, fastTableBits);

  uint32_t fastTableSize = 1 << fastTableBits;
  uint32_t dstIndex = 0;

  do {
    uint32_t srcIndex = 0;

    while (srcIndex <= regularTableMask) {
      DecodeEntry decodeEntry = decode_table[srcIndex++];
      if (DecoderUtils::isLiteral(decodeEntry)) {
        uint32_t litLen = DecoderUtils::baseLength(decodeEntry);
        decodeEntry.value = (decodeEntry.value & 0xFF00FF00u) | (1u << DecodeEntry::kLiteralCountOffset) | litLen;

        if (litLen < maxMergeableSize) {
          DecodeEntry consecutiveEntry = decode_table[(dstIndex >> litLen) & regularTableMask];
          uint32_t consecutiveLength = DecoderUtils::baseLength(consecutiveEntry);

          if (DecoderUtils::isLiteral(consecutiveEntry) && litLen + consecutiveLength <= fastTableBits) {
            decodeEntry.value += ((consecutiveEntry.value & 0xFF00u) << 8) + (consecutiveLength) + (1u << DecodeEntry::kLiteralCountOffset);
          }
        }
      }

      decode_table[dstIndex++] = decodeEntry;
    }

    regularTableMask = dstIndex - 1;
  } while (dstIndex < fastTableSize);

  return fastTableBits;
}
#endif

} // {anonymous}

BLResult Decoder::init(FormatType format, DecoderOptions options) noexcept {
  _state = format == FormatType::kZlib ? DecoderState::kZlibHeader : DecoderState::kBlockHeader;
  _flags = DecoderFlags::kNone;
  _options = options;

  _bitWord = 0;
  _bitLength = 0;
  _copyRemaining = 0;

  _litlenSymbolCount = 0;
  _offsetSymbolCount = 0;
  _workIndex = 0;
  _workCount = 0;
  _processedBytes = 0;

  // Fast implementation is only available on 64-bit targets.
#if BL_TARGET_ARCH_BITS >= 64
  _fastDecodeFunc = Fast::decode;

#if defined(BL_BUILD_OPT_AVX2)
  if (blRuntimeHasAVX2(&blRuntimeContext)) {
    _fastDecodeFunc = Fast::decode_AVX2;
  }
#endif // BL_BUILD_OPT_AVX2
#endif // BL_TARGET_ARCH_BITS >= 64

  return BL_SUCCESS;
}

BLResult Decoder::decode(BLArray<uint8_t>& dst, BLDataView input) noexcept {
  uint8_t* dstStart;
  BL_PROPAGATE(dst.makeMutable(&dstStart));

  uint8_t* dstPtr = dstStart + dst.size();
  uint8_t* dstEnd = dstStart + dst.capacity();

  const uint8_t* srcData = input.data;
  const uint8_t* srcPtr = srcData;
  const uint8_t* srcEnd = srcData + input.size;

  DecoderBits bits;
  DecoderState state = _state;

  bits.loadState(this);

  // This is a state loop - initially we start with kZlibHeader or kBlockHeader state and then once we consume
  // input bytes the state is changed. The purpose of having states is to have a recoverable position so we can
  // support consuming multiple input chunks of data, which will happen if we consume multiple IDAT chunks in a
  // PNG image, for example.
  for (;;) {
    // Refill enough bits so we can process or refuse to process the current state. This doesn't have to be
    // optimized as this would only execute between switching states or between processing different input
    // chunks - so the idea is to refill the whole BitWord whenever possible so we don't have to refill
    // within the switch{}, if possible.
    BL_NOUNROLL
    while (srcPtr != srcEnd && bits.canRefillByte()) {
      bits.refillByte(*srcPtr++);
    }

    switch (state) {
      case DecoderState::kDone: {
        // TODO:
        break;
      }

      // Zlib Header
      // -----------

      case DecoderState::kZlibHeader: {
        if (BL_UNLIKELY(bits.length() < 16u)) {
          goto NotEnoughInputBytes;
        }

        uint32_t cmf = bits.extract<0>(8); // CMF (8 bits) - Compression method & info.
        uint32_t flg = bits.extract<8>(8); // FLG (8 bits) - Zlib flags.
        uint32_t fdict = (flg >> 5) & 0x1u;

        // `(CMF << 8) | FLG` has to be divisible by `31`.
        if (BL_UNLIKELY(((cmf << 8) + flg) % 31u != 0u))
          goto ErrorInvalidData;

        // The only allowed compression method is DEFLATE (8).
        if (BL_UNLIKELY((cmf & 0xFu) != 8u))
          goto ErrorInvalidData;

        // Preset dictionary is not supported.
        if (BL_UNLIKELY(fdict))
          goto ErrorInvalidData;

        bits.consumed(16);
        state = DecoderState::kBlockHeader;
        continue;
      }

      // Block Header
      // ------------

      case DecoderState::kBlockHeader: {
        if (BL_UNLIKELY(bits.length() < 3u)) {
          goto NotEnoughInputBytes;
        }

        uint32_t finalBlock = bits.extract<0>(1); // BFINAL (1 bit) - Final block flag.
        uint32_t blockType = bits.extract<1>(2);  // BTYPE (2 bits) - Type of the block.

        static constexpr uint8_t nextState[4] = {
          uint8_t(DecoderState::kUncompressedHeader),   // Uncompressed.
          uint8_t(DecoderState::kStaticHuffmanHeader),  // Static Huffman.
          uint8_t(DecoderState::kDynamicHuffmanHeader), // Dynamic Huffman.
          uint8_t(0)                                    // Invalid.
        };

        // The only combination, which is not allowed (blockType == 3) - this is invalid.
        if (blockType == 3) {
          goto ErrorInvalidData;
        }

        bits.consumed(3);
        state = DecoderState(nextState[blockType]);

        if (finalBlock) {
          _flags |= DecoderFlags::kFinalBlock;
        }

        if (blockType == uint32_t(BlockType::kUncompressed)) {
          // We must discard remaining bits in `bitWord` in case of uncompressed data - we have to do it here, because
          // on 32-bit targets we need all 32 bits in `kUncompressedHeader` state, which describe how many bytes to copy.
          bits.makeByteAligned();
        }

        continue;
      }

      case DecoderState::kUncompressedHeader: {
        // Must be byte aligned as we have already discarded the unnecessary bits.
        BL_ASSERT(bits.isByteAligned());

        // The bit-buffer must be byte-aligned and fully refilled - that ensures there are at least 32 bits available.
        if (BL_UNLIKELY(bits.length() < 32u)) {
          goto NotEnoughInputBytes;
        }

        // The maximum number of bytes to copy is 65535.
        uint32_t len = bits.extract<0>(16);
        uint32_t lenCheck = bits.extract<16>(16) ^ 0xFFFFu;

        // len == nlen ^ 0xFFFF;
        if (BL_UNLIKELY(len != lenCheck)) {
          goto ErrorInvalidData;
        }

        // Store how many bytes to copy in kCopyUncompressedBlock state.
        _copyRemaining = len;

        // In general we don't need to refill the BitWord at this point as the generic refill is slower than doing
        // a raw memory copy - so, when we can, don't refill and jump directly to the copy case. This is not required
        // though as the current chunk of data could end here and that's perfectly fine, so it's just an optimization.
        if constexpr (sizeof(BLBitWord) < 8) {
          // Consuming 32 bits at once never happens except here, so we have to handle this correctly on 32-bit targets
          // as shifting a 32-bit number by 32 is undefined behavior. So reset the bit buffer instead of shifting by 32.
          bits.reset();
        }
        else {
          bits.consumed(32);
        }

        // Allowed by the specification: "The uncompressed data size can range between 0 and 65535 bytes".
        if (BL_UNLIKELY(len == 0u)) {
          goto BlockDone;
        }

        state = DecoderState::kCopyUncompressedBlock;
        goto CopyBytesState;
      }

      case DecoderState::kStaticHuffmanHeader: {
        BL_DECODER_UPDATE_STATISTICS(statistics.stream.staticBlockCount++);

        // Static Huffman block: build the decode tables for the static codes. Skip doing so if the tables are already
        // set up from an earlier static block; this speeds up decompression of degenerate input of many empty or very
        // short static blocks. Afterwards, the remainder is the same as decompressing a dynamic Huffman block.
        if (blTestFlag(_flags, DecoderFlags::kStaticTableActive)) {
          state = DecoderState::kDecompressHuffmanBlock;
          continue;
        }

        _flags |= DecoderFlags::kStaticTableActive;
        _flags &=~DecoderFlags::kOptimizedTableActive;

        _litlenSymbolCount = kNumLitLenSymbols;
        _offsetSymbolCount = kNumOffsetSymbols;

        // Initialize pre-code lens table that will be used to construct static Huffman tables.
        uint32_t i;
        for (i = 0; i < 144; i++) {
          tables.lens[i] = 8;
        }

        for (; i < 256; i++) {
          tables.lens[i] = 9;
        }

        for (; i < 280; i++) {
          tables.lens[i] = 7;
        }

        for (; i < kNumLitLenSymbols; i++) {
          tables.lens[i] = 8;
        }

        for (; i < kNumLitLenSymbols + kNumOffsetSymbols; i++) {
          tables.lens[i] = 5;
        }

        goto BuildHuffmanTables;
      }

      case DecoderState::kDynamicHuffmanHeader: {
        constexpr uint32_t kHeaderPrecodeLens = sizeof(BLBitWord) < 8u ? 3u : 4u;
        constexpr uint32_t kHeaderMinLength = (5u + 5u + 4u) + (3u * kHeaderPrecodeLens);

        BL_DECODER_UPDATE_STATISTICS(statistics.stream.dynamicBlockCount++);

        if (BL_UNLIKELY(bits.length() < kHeaderMinLength)) {
          goto NotEnoughInputBytes;
        }

        // Read the codeword length counts.
        _litlenSymbolCount = bits.extract<0>(5) + 257u;
        _offsetSymbolCount = bits.extract<5>(5) + 1u;

        _flags &= ~(DecoderFlags::kStaticTableActive | DecoderFlags::kOptimizedTableActive);
        _workIndex = kHeaderPrecodeLens;
        _workCount = bits.extract<10>(4) + 4u;

        // We know the minimum explicit pre-code lens is 4 - so we can process up to 4 here.
        uint32_t plen0 = bits.extract<14>(3);
        uint32_t plen1 = bits.extract<17>(3);
        uint32_t plen2 = bits.extract<20>(3);

        memset(tables.precode_lens, 0, kNumPrecodeSymbols);
        tables.precode_lens[kPrecodeLensPermutation[0]] = uint8_t(plen0);
        tables.precode_lens[kPrecodeLensPermutation[1]] = uint8_t(plen1);
        tables.precode_lens[kPrecodeLensPermutation[2]] = uint8_t(plen2);

        // 4th is only possible on a 64-bit machine as it's not guaranteed we will have enough bits otherwise.
        if constexpr (kHeaderPrecodeLens == 4u) {
          uint32_t plen3 = bits.extract<23>(3);
          tables.precode_lens[kPrecodeLensPermutation[3]] = uint8_t(plen3);
        }

        bits.consumed(kHeaderMinLength);
        state = DecoderState::kDynamicHuffmanPreCodeLens;
        continue;
      }

      case DecoderState::kDynamicHuffmanPreCodeLens: {
        uint32_t i = _workIndex;
        uint32_t remaining = _workCount - i;

        constexpr uint32_t kMainLoopSize = 3u;
        constexpr uint32_t kMainLoopBits = 3u * 3u;

        if (remaining >= kMainLoopSize) {
          if (BL_UNLIKELY(bits.length() < kMainLoopBits)) {
            goto NotEnoughInputBytes;
          }

          do {
            uint32_t plen0 = bits.extract<0>(3);
            uint32_t plen1 = bits.extract<3>(3);
            uint32_t plen2 = bits.extract<6>(3);

            tables.precode_lens[kPrecodeLensPermutation[i + 0]] = uint8_t(plen0);
            tables.precode_lens[kPrecodeLensPermutation[i + 1]] = uint8_t(plen1);
            tables.precode_lens[kPrecodeLensPermutation[i + 2]] = uint8_t(plen2);

            i += kMainLoopSize;
            remaining -= kMainLoopSize;
            bits.consumed(kMainLoopBits);

            if (srcPtr != srcEnd) {
              bits.refillByte(*srcPtr++);
            }
          } while (remaining >= kMainLoopSize && bits.length() >= kMainLoopBits);
        }

        uint32_t requiredBits = remaining * 3u;
        if (BL_UNLIKELY(bits.length() < requiredBits)) {
          // Update the work index as we could have executed the main loop previously.
          _workIndex = i;
          goto NotEnoughInputBytes;
        }

        while (i != _workCount) {
          uint32_t plen = bits.extract<0>(3);
          bits.consumed(3);

          // Should never happen as we have checked the size of the bit-buffer before entering the loop.
          BL_ASSERT(!bits.overflown());

          tables.precode_lens[kPrecodeLensPermutation[i]] = uint8_t(plen);
          i++;
        }

        // Reset the workIndex as we will enter a new state.
        _workIndex = 0;

        // Build a decode table for the precode.
        _precodeTableInfo = buildDecodeTable(
          tables.precode_decode_table.entries,
          tables.precode_lens,
          kNumPrecodeSymbols,
          kPrecodeDecodeResults,
          kDecoderPrecodeTableBits,
          kMaxPreCodeWordLen,
          DecodeTableType::kPrecode);

        if (_precodeTableInfo.tableBits == 0) {
          goto ErrorInvalidData;
        }

        state = DecoderState::kDynamicHuffmanLitLenOffsetCodes;
        continue;
      }

      case DecoderState::kDynamicHuffmanLitLenOffsetCodes: {
        // Decode the litlen and offset codeword lengths.
        {
          uint32_t i = _workIndex;
          uint32_t count = _litlenSymbolCount + _offsetSymbolCount;
          uint32_t precodeLookupMask = DecoderUtils::mask32(_precodeTableInfo.tableBits);

          do {
            // NOTE: We refill 1 byte per iteration - this should be okay considering the maximum
            // precode size is 7 bits and then additional 7 bits can be required for length. In
            // the worst case we would have to repeat some iterations.
            if (bits.canRefillByte() && BL_LIKELY(srcPtr != srcEnd)) {
              bits.refillByte(*srcPtr++);
            }

            // The code below assumes that the pre-code decode table doesn't have any subtables.
            BL_STATIC_ASSERT(kDecoderPrecodeTableBits == kMaxPreCodeWordLen);

            // Decode the next pre-code symbol.
            DecodeEntry entry = tables.precode_decode_table[bits & precodeLookupMask];

            uint32_t presym = DecoderUtils::precodeValue(entry);
            uint32_t entryLen = DecoderUtils::fullLength(entry);

            if (BL_UNLIKELY(bits.length() < entryLen)) {
              if (BL_UNLIKELY(srcPtr == srcEnd)) {
                _workIndex = i;
                goto NotEnoughInputBytes;
              }
              continue;
            }

            // Explicit codeword length.
            if (presym < 16u) {
              tables.lens[i++] = uint8_t(presym);
              bits.consumed(entryLen);
              continue;
            }

            uint32_t n = (bits.extract(entryLen) >> DecoderUtils::baseLength(entry)) + DecoderUtils::precodeRepeat(entry);

            // We don't need to immediately verify that the repeat count doesn't overflow the number of elements,
            // since we've sized the lens array to have enough extra space to allow for the worst-case overrun
            // (138 zeroes when only 1 length was remaining). In the case of the small repeat counts (presyms
            // 16 and 17), it is fastest to always write the maximum number of entries. That gets rid of branches
            // that would otherwise be required.
            BL_STATIC_ASSERT(kMaxLensOverrun == 138 - 1);

            if (presym == 16) {
              // Repeat the previous length 3 - 6 times - this is invalid if this is the first entry.
              if (BL_UNLIKELY(i == 0)) {
                goto ErrorInvalidData;
              }

              bits.consumed(entryLen);
              uint8_t v = tables.lens[i - 1];
              memset(tables.lens + i, v, 6);
              i += n;
            }
            else if (presym == 17) {
              // Repeat zero 3 - 10 times.
              bits.consumed(entryLen);
              memset(tables.lens + i, 0, 10);
              i += n;
            }
            else {
              // Repeat zero 11 - 138 times.
              bits.consumed(entryLen);
              memset(tables.lens + i, 0, n);
              i += n;
            }

            // That would mean there is a bug in the impl as we have consumed more bits than we had.
            BL_ASSERT(!bits.overflown());
          } while (i < count);

          // This makes the decoder's behavior compatible with both zlib and libdeflate.
          if (BL_UNLIKELY(i != count)) {
            goto ErrorInvalidData;
          }
        }

BuildHuffmanTables:
        _offsetTableInfo = buildDecodeTable(
          tables.offset_decode_table.entries,
          tables.lens + _litlenSymbolCount,
          _offsetSymbolCount,
          kOffsetDecodeResults,
          kDecoderOffsetTableBits,
          kMaxOffsetCodeWordLen,
          DecodeTableType::kOffset);

        if (_offsetTableInfo.tableBits == 0) {
          goto ErrorInvalidData;
        }

        _litlenTableInfo = buildDecodeTable(
          tables.litlen_decode_table.entries,
          tables.lens,
          _litlenSymbolCount,
          kLitLenDecodeResults,
          kDecoderLitLenTableBits,
          kMaxLitLenCodeWordLen,
          DecodeTableType::kLitLen);
        _litlenFastTableBits = _litlenTableInfo.tableBits;

        if (_litlenTableInfo.tableBits == 0) {
          goto ErrorInvalidData;
        }

        state = DecoderState::kDecompressHuffmanBlock;
        continue;
      }

      // Compressed Block
      // ----------------

      case DecoderState::kDecompressHuffmanBlock: {
        // Reset some state variables that could be potentially set.
        _copyRemaining = 0;

        // Optimized Loop (Dispatch)
        // -------------------------

        // Fast loop is only implemented on 64-bit targets at the moment.
#if BL_TARGET_ARCH_BITS >= 64
        // Only call fast decode func if both source and destination buffers have sufficient size.
        if (PtrOps::bytesUntil(srcPtr, srcEnd) >= Fast::kMinimumFastSrcBuffer &&
            PtrOps::bytesUntil(dstPtr, dstEnd) >= Fast::kMinimumFastDstBuffer) {
          if (!blTestFlag(_flags, DecoderFlags::kOptimizedTableActive)) {
            _litlenFastTableBits = buildFastTable(_litlenTableInfo, tables.litlen_decode_table.entries);
            _flags |= DecoderFlags::kOptimizedTableActive;
          }

          bits.storeState(this);
          DecoderFastResult result = _fastDecodeFunc(this, dstStart, dstPtr, dstEnd, srcPtr, srcEnd);

          bits.loadState(this);
          dstPtr = result.dstPtr;
          srcPtr = result.srcPtr;

          if (BL_UNLIKELY(result.status != DecoderFastStatus::kOk)) {
            if (BL_LIKELY(result.status == DecoderFastStatus::kBlockDone)) {
              goto BlockDone;
            }
            else {
              goto ErrorInvalidData;
            }
          }
        }
#endif // BL_TARGET_ARCH_BITS >= 64

        // Decompressing a Huffman block (either dynamic or static).
        DecoderTableMask litlenTableMask(_litlenTableInfo.tableBits);
        DecoderTableMask offsetTableMask(_offsetTableInfo.tableBits);

        BL_DECODER_UPDATE_STATISTICS(statistics.tail.numRestarts++);

        // Tail Loop - Optimized
        // ---------------------

#if BL_TARGET_ARCH_BITS >= 64
        if (PtrOps::bytesUntil(srcPtr, srcEnd) >= sizeof(BLBitWord) && PtrOps::bytesUntil(dstPtr, dstEnd) >= 3) {
          uint8_t* dstEndMinus2 = dstEnd - 2;

          srcPtr += bits.refillBitWord(MemOps::loadu_le<BLBitWord>(srcPtr));
          DecodeEntry entry = tables.litlen_decode_table[bits.extract(litlenTableMask)];

          while (dstPtr < dstEndMinus2 && PtrOps::bytesUntil(srcPtr, srcEnd) >= 8) {
            BL_DECODER_UPDATE_STATISTICS(statistics.tail.numIterations++);

            BLBitWord refillData = MemOps::loadu_le<BLBitWord>(srcPtr);
            uint32_t length = DecoderUtils::payloadField(entry);

            if (DecoderUtils::isLiteral(entry)) {
              BL_DECODER_UPDATE_STATISTICS(statistics.tail.quickLiteralEntries++);
              bits.consumed(DecoderUtils::baseLength(entry));
              entry = tables.litlen_decode_table[bits.extract(litlenTableMask)];
              *dstPtr++ = uint8_t(length & 0xFFu);

              if (DecoderUtils::isLiteral(entry)) {
                BL_DECODER_UPDATE_STATISTICS(statistics.tail.quickLiteralEntries++);
                bits.consumed(DecoderUtils::baseLength(entry));
                length = DecoderUtils::payloadField(entry);

                entry = tables.litlen_decode_table[bits.extract(litlenTableMask)];
                *dstPtr++ = uint8_t(length & 0xFFu);

                if (DecoderUtils::isLiteral(entry)) {
                  BL_DECODER_UPDATE_STATISTICS(statistics.tail.quickLiteralEntries++);
                  bits.consumed(DecoderUtils::baseLength(entry));
                  length = DecoderUtils::payloadField(entry);

                  entry = tables.litlen_decode_table[bits.extract(litlenTableMask)];
                  *dstPtr++ = uint8_t(length & 0xFFu);
                }
              }

              srcPtr += bits.refillBitWord(refillData);
              continue;
            }

            DecoderBits savedBits = bits;
            length += bits.extractExtra(entry);

            if (BL_UNLIKELY(!DecoderUtils::isOffOrLen(entry))) {
              BL_DECODER_UPDATE_STATISTICS(statistics.tail.subtableOffsetEntries++);
              entry = tables.litlen_decode_table[length];
              length = DecoderUtils::payloadField(entry);
              bits.consumed(entry);

              if (DecoderUtils::isLiteral(entry)) {
                BL_DECODER_UPDATE_STATISTICS(statistics.tail.subtableLiteralEntries++);
                size_t entryIndex = bits.extract(litlenTableMask);
                *dstPtr++ = uint8_t(length & 0xFFu);

                srcPtr += bits.refillBitWord(refillData);
                entry = tables.litlen_decode_table[entryIndex];
                continue;
              }

              if (BL_UNLIKELY(DecoderUtils::isEndOfBlock(entry))) {
                if (BL_LIKELY(!DecoderUtils::isEndOfBlockInvalid(entry)))
                  goto BlockDone;
                else
                  goto ErrorInvalidData;
              }

              length += savedBits.extractExtra(entry);
              BL_DECODER_UPDATE_STATISTICS(statistics.tail.subtableLengthEntries++);
            }
            else {
              bits.consumed(entry);
            }

            BL_DECODER_UPDATE_STATISTICS(statistics.tail.matchEntries++);

            if (BL_UNLIKELY(PtrOps::bytesUntil(dstPtr, dstEnd) < length)) {
              _copyRemaining = length;
              bits = savedBits;
              goto NotEnoughOutputBytes;
            }

            DecodeEntry offsetEntry = tables.offset_decode_table[bits.extract(offsetTableMask)];
            uint32_t offset = DecoderUtils::payloadField(offsetEntry) + bits.extractExtra(offsetEntry);

            if (BL_UNLIKELY(!DecoderUtils::isOffOrLen(offsetEntry))) {
              BL_DECODER_UPDATE_STATISTICS(statistics.tail.subtableOffsetEntries++);
              offsetEntry = tables.offset_decode_table[offset];
              offset = DecoderUtils::payloadField(offsetEntry) + bits.extractExtra(offsetEntry);

              if (BL_UNLIKELY(DecoderUtils::isEndOfBlock(offsetEntry))) {
                goto ErrorInvalidData;
              }
            }

            size_t dstSize = PtrOps::byteOffset(dstStart, dstPtr);
            if (BL_UNLIKELY(offset > dstSize)) {
              goto ErrorInvalidData;
            }

            bits.consumed(offsetEntry);
            srcPtr += bits.refillBitWord(refillData);

            const uint8_t* matchPtr = dstPtr - offset;
            const uint8_t* matchEnd = matchPtr + length;

            BL_STATIC_ASSERT(kMinMatchLen == 3);
            *dstPtr++ = *matchPtr++;
            *dstPtr++ = *matchPtr++;
            entry = tables.litlen_decode_table[bits.extract(litlenTableMask)];

            do {
              *dstPtr++ = *matchPtr++;
            } while (matchPtr != matchEnd);
          }

          bits.fixLengthAfterFastLoop();
        }
#endif

        // Tail Loop - Safe
        // ----------------

        // This is a generic loop for decoding literals and matches. The purpose of this loop is to be safe when it
        // comes to both source and destination buffers - this means that it cannot read after `srcEnd` and it cannot
        // write after `dstEnd`. Typically, this loop executes only at the end of the decompression phase to handle
        // the remaining bytes that cannot be processed by the fast loop.
        for (;;) {
          BL_DECODER_UPDATE_STATISTICS(statistics.tail.numIterations++);

          while (bits.canRefillByte() && srcPtr != srcEnd) {
            bits.refillByte(*srcPtr++);
          }

          DecodeEntry entry = tables.litlen_decode_table[bits.extract(litlenTableMask)];
          DecoderBits savedBits = bits;

          uint32_t baseLen = DecoderUtils::baseLength(entry);
          uint32_t length = DecoderUtils::payloadField(entry);

          if (DecoderUtils::isLiteral(entry)) {
            if (BL_UNLIKELY(dstPtr == dstEnd)) {
              goto NotEnoughOutputBytes;
            }

            if (BL_UNLIKELY(bits.length() < baseLen)) {
              goto NotEnoughInputBytes;
            }

            BL_DECODER_UPDATE_STATISTICS(statistics.tail.quickLiteralEntries++);
            bits.consumed(baseLen);

            *dstPtr++ = uint8_t(length & 0xFFu);
            continue;
          }

          // NOTE: We can treat end-of-block as a sub-table - it has baseLen equal to fullLen, so we would
          // just repeat the same lookup. The reason why to do this is to remove branches we don't want slightly
          // penalizing end of block handling, but since it's rare compared to literals/lengths it's just fine.
          uint32_t fullLen = DecoderUtils::fullLength(entry);
          length += savedBits.extract(fullLen) >> baseLen;

          if (BL_UNLIKELY(!DecoderUtils::isOffOrLen(entry))) {
            BL_DECODER_UPDATE_STATISTICS(statistics.tail.subtableLookups++);

            entry = tables.litlen_decode_table[length];
            length = DecoderUtils::payloadField(entry);
            fullLen = DecoderUtils::fullLength(entry);

            if (bits.length() < fullLen) {
              goto NotEnoughInputBytes;
            }

            if (DecoderUtils::isLiteral(entry)) {
              BL_DECODER_UPDATE_STATISTICS(statistics.tail.subtableLiteralEntries++);

              if (BL_UNLIKELY(dstPtr == dstEnd)) {
                goto NotEnoughOutputBytes;
              }

              BL_ASSERT(bits.length() >= fullLen);
              bits.consumed(fullLen);

              *dstPtr++ = uint8_t(length & 0xFFu);
              continue;
            }

            length += savedBits.extractExtra(entry);

            if (BL_UNLIKELY(DecoderUtils::isEndOfBlock(entry))) {
              BL_ASSERT(bits.length() >= fullLen);
              bits.consumed(fullLen);

              if (BL_LIKELY(!DecoderUtils::isEndOfBlockInvalid(entry)))
                goto BlockDone;
              else
                goto ErrorInvalidData;
            }

            BL_DECODER_UPDATE_STATISTICS(statistics.tail.subtableLengthEntries++);
          }

          BL_DECODER_UPDATE_STATISTICS(statistics.tail.matchEntries++);

          bits.consumed(fullLen);
          if (BL_UNLIKELY(bits.overflown())) {
            bits = savedBits;
            goto NotEnoughInputBytes;
          }

          if (BL_UNLIKELY(PtrOps::bytesUntil(dstPtr, dstEnd) < length)) {
            _copyRemaining = length;
            bits = savedBits;
            goto NotEnoughOutputBytes;
          }

          if constexpr (sizeof(BLBitWord) < 8) {
            while (bits.canRefillByte() && srcPtr != srcEnd) {
              bits.refillByte(*srcPtr++);
            }

            // This would make the accumulator always full so we would be able to always read 28 bits from it
            // in 32-bit mode.
            if (srcPtr != srcEnd && bits.bitLength < 32) {
              bits.bitWord |= BLBitWord(srcPtr[0]) << bits.bitLength;
            }

            savedBits = bits;
          }

          entry = tables.offset_decode_table[bits.extract(offsetTableMask)];
          fullLen = DecoderUtils::fullLength(entry);
          uint32_t offset = DecoderUtils::payloadField(entry) + bits.extractExtra(entry);

          if (BL_UNLIKELY(!DecoderUtils::isOffOrLen(entry))) {
            BL_DECODER_UPDATE_STATISTICS(statistics.tail.subtableOffsetEntries++);
            entry = tables.offset_decode_table[offset];
            fullLen = DecoderUtils::fullLength(entry);
            offset = DecoderUtils::payloadField(entry) + bits.extractExtra(entry);

            if (BL_UNLIKELY(DecoderUtils::isEndOfBlock(entry))) {
              goto ErrorInvalidData;
            }
          }

          if (BL_UNLIKELY(bits.length() < fullLen)) {
            if constexpr (sizeof(BLBitWord) < 8) {
              if (srcPtr == srcEnd) {
                // This is only needed in 32-bit mode as in 64-bit the bit-accumulator is long enough
                // to hold all 48 bits that can be required to hold both offset+length match data.
                state = DecoderState::kDecompressHuffmanInterruptedMatch;

                _copyRemaining = length;
                goto NotEnoughInputBytes;
              }
              else {
                bits.consumed(8);
                bits.refillByte(*srcPtr++);
                fullLen -= 8;
              }
            }
            else {
              // In 64-bit mode this always means that there is not enough input bytes and that the input is
              // exhausted.
              bits = savedBits;
              _copyRemaining = length;
              goto NotEnoughInputBytes;
            }
          }

          BL_ASSERT(bits.length() >= fullLen);
          bits.consumed(fullLen);

          size_t dstSize = PtrOps::byteOffset(dstStart, dstPtr);
          if (BL_UNLIKELY(offset > dstSize)) {
            goto ErrorInvalidData;
          }

          const uint8_t* matchPtr = dstPtr - offset;
          const uint8_t* matchEnd = matchPtr + length;

          BL_STATIC_ASSERT(kMinMatchLen == 3);
          *dstPtr++ = *matchPtr++;
          *dstPtr++ = *matchPtr++;

          do {
            *dstPtr++ = *matchPtr++;
          } while (matchPtr != matchEnd);
        }
      }

      case DecoderState::kDecompressHuffmanInterruptedMatch: {
        // A state only designed to continue processing an interrupted match since it could need more bytes than
        // a word size. When entering this state the litlen code was already processed and the decoded length was
        // stored to `_copyRemaining` so we only need to process the remaining offset part.
        if constexpr (sizeof(BLBitWord) < 8) {
          DecoderTableMask offsetTableMask(_offsetTableInfo.tableBits);
          size_t length = _copyRemaining;

          // Since the user feeds data and is responsible for passing the destination buffer each time it feeds a
          // source buffer, we don't know whether the passed buffer has enough window DEFLATE requires. So we must
          // stay safe and we just cannot blindly copy bytes to the destination).
          if (BL_UNLIKELY(PtrOps::bytesUntil(dstPtr, dstEnd) < length)) {
            goto NotEnoughOutputBytes;
          }

          uint32_t entryIndex = bits.extract(offsetTableMask);
          DecodeEntry entry = tables.offset_decode_table[entryIndex];

          uint32_t fullLen = DecoderUtils::fullLength(entry);
          uint32_t offset = DecoderUtils::payloadField(entry) + (bits.extract(entry) >> DecoderUtils::baseLength(entry));

          if (BL_UNLIKELY(bits.length() < fullLen)) {
            goto NotEnoughInputBytes;
          }

          if (BL_UNLIKELY(!DecoderUtils::isOffOrLen(entry))) {
            BL_DECODER_UPDATE_STATISTICS(statistics.tail.subtableOffsetEntries++);
            entry = tables.offset_decode_table[offset];
            fullLen = DecoderUtils::fullLength(entry);
            offset = DecoderUtils::payloadField(entry);

            uint32_t baseLen = DecoderUtils::baseLength(entry);
            uint32_t extra = bits.extract(entry) >> baseLen;

            if (BL_UNLIKELY(DecoderUtils::isEndOfBlock(entry))) {
              goto ErrorInvalidData;
            }

            // NOTE: In 32-bit mode even a full bit-buffer could not be enough to encode offset + extra. The reason is
            // that the maximum codeword length is 15 bits and the maximum offset extra is 13 bits, which totals 28 bits.
            // This situation is only possible when the offset requires a subtable, otherwise the bit-buffer would always
            // have enough bits.
            if (BL_UNLIKELY(bits.length() < fullLen)) {
              // If the bit-buffer length is smaller than 25 bits or the source pointer is at the end it means that there
              // is not enough data and the user has to provide more. There is nothing we can do now. We have all the data
              // we need to decode this entry again from the current bit-buffer content, and we need more data to continue.
              if (bits.length() < 25u || srcPtr == srcEnd) {
                goto NotEnoughInputBytes;
              }

              // Now we know that we don't have enough data in our bit-buffer, but there is more data in the input buffer.
              // To follow how the data is usually extracted from bit-buffer we just insert a partial byte into our bit-buffer
              // (partial because it doesn't fit as a whole), extract the data the usual way and then consume 8 bits so we can
              // add the byte for real.
              uint32_t pendingByte = *srcPtr++;
              BL_ASSERT(fullLen >= 8);

              bits.bitWord |= pendingByte << bits.bitLength;
              extra = bits.extract(entry) >> baseLen;

              bits.consumed(8);
              bits.refillByte(uint8_t(pendingByte));
              fullLen -= 8;
            }

            offset += extra;
          }

          bits.consumed(fullLen);
          BL_ASSERT(!bits.overflown());

          size_t dstSize = PtrOps::byteOffset(dstStart, dstPtr);
          if (BL_UNLIKELY(offset > dstSize)) {
            goto ErrorInvalidData;
          }

          const uint8_t* matchPtr = dstPtr - offset;
          const uint8_t* matchEnd = matchPtr + length;

          BL_STATIC_ASSERT(kMinMatchLen == 3);
          *dstPtr++ = *matchPtr++;
          *dstPtr++ = *matchPtr++;

          do {
            *dstPtr++ = *matchPtr++;
          } while (matchPtr != matchEnd);

          _copyRemaining = 0;
          state = DecoderState::kDecompressHuffmanBlock;
          continue;
        }
        else {
          // This state is never reached in 64-bit mode - it's impossible to get here.
          goto ErrorInvalidData;
        }
      }

      // Uncompressed Block
      // ------------------

      case DecoderState::kCopyUncompressedBlock:
CopyBytesState: {
        // The bit-buffer must be aligned to bytes at this point - the same as in `kUncompressedHeader` state.
        BL_ASSERT(bits.isByteAligned());

        // Cannot be zero as that would mean this is an invalid state.
        BL_ASSERT(_copyRemaining != 0u);

        size_t srcRemaining = PtrOps::bytesUntil(srcPtr, srcEnd);
        size_t dstRemaining = PtrOps::bytesUntil(dstPtr, dstEnd);

        if (BL_UNLIKELY(dstRemaining < _copyRemaining)) {
          goto NotEnoughOutputBytes;
        }

        // Process the remaining part in BitWord first. Ideally this would be at most sizeof(BitWord) - 4 bytes, but
        // in case the input buffer is from multiple chunks, it could have more bytes.
        if (!bits.empty()) {
          // Calculate the number of bytes we can copy here.
          size_t n = blMin<size_t>(_copyRemaining, bits.length() >> 3u);

          if (n) {
            _copyRemaining -= n;
            bits.bitLength -= n * 8u;

            do {
              *dstPtr++ = uint8_t(bits & 0xFFu);
              bits.bitWord >>= 8;
            } while (--n);
          }

          // If there are no remaining bytes to copy then this block is done.
          if (_copyRemaining == 0) {
            goto BlockDone;
          }
        }

        size_t n = blMin<size_t>(_copyRemaining, srcRemaining);
        if (n == 0) {
          goto NotEnoughInputBytes;
        }
        else {
          memcpy(dstPtr, srcPtr, n);
          dstPtr += n;
          srcPtr += n;

          // If there are no remaining bytes to copy then this block is done.
          _copyRemaining -= n;
          if (_copyRemaining == 0) {
            goto BlockDone;
          }
        }

        continue;
      }

      // Other States
      // ------------

      case DecoderState::kInvalid: {
        return blTraceError(BL_ERROR_INVALID_DATA);
      }
    }

    // Hit when the destination is full and thus requires to grow.
NotEnoughOutputBytes:
    bits.fixLengthAfterFastLoop();

    {
      // Update the size of the destination array first so we can grow.
      size_t dstSize = PtrOps::byteOffset(dstStart, dstPtr);
      ArrayInternal::setSize(&dst, dstSize);

      // Save the current status in case of failure so the exact state could be recovered if the user recovers the
      // error.
      _state = state;
      bits.storeState(this);

      // Update the number of bytes processed - this is important as we may fail to grow the destination - in that
      // case we would just return and want the member updated.
      _processedBytes += PtrOps::byteOffset(srcData, srcPtr);
      srcData = srcPtr;

      // When decoding data where the uncompressed size is known (for example decoding PNG pixel data) it's desired
      // to fail early if the buffer decompresses to more bytes than it should. The implementation has to check the
      // size of the decompressed data anyway, but we don't want to grow above the threshold.
      if (blTestFlag(_options, DecoderOptions::kNeverReallocOutputBuffer)) {
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);
      }

      // We can calculate the number of bytes required exactly if this is a last block, which is uncompressed.
      uint64_t sizeEstimate = dstSize;
      if (state == DecoderState::kCopyUncompressedBlock && blTestFlag(_flags, DecoderFlags::kFinalBlock) && _copyRemaining) {
        sizeEstimate += _copyRemaining;
      }
      else {
        // Calculate the current compression ratio and estimated the current input chunk based on that. We don't
        // know whether the current chunk is last or not, but we definitely want to consider it in case that the
        // default estimate would be too small.
        double estimatedRatio = (double(dstSize) / double(_processedBytes)) + 0.05;

        uint64_t genericEstimate = blMax<uint64_t>(dstSize, 4096u);
        uint64_t chunkEstimate = uint64_t(double(size_t(srcEnd - srcPtr)) * estimatedRatio);

        sizeEstimate += blMax<uint64_t>(blMax<uint64_t>(genericEstimate, chunkEstimate) + 4096u, _copyRemaining);
      }

#if BL_TARGET_ARCH_BITS < 64
      if (sizeEstimate > uint64_t(SIZE_MAX)) {
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);
      }
#endif

      BL_PROPAGATE(dst.reserve(size_t(sizeEstimate)));

      // Destination pointers were invalidated by reallocating `dst`.
      dstStart = ArrayInternal::getData<uint8_t>(&dst);
      dstPtr = dstStart + dstSize;
      dstEnd = dstStart + dst.capacity();

      continue;
    }

BlockDone:
    bits.fixLengthAfterFastLoop();

    if (!blTestFlag(_flags, DecoderFlags::kFinalBlock)) {
      // Expect an additional block if this block was not the last.
      state = DecoderState::kBlockHeader;
      continue;
    }

    // The decoding is done - reset all internal states and mark the decoder done.
    _processedBytes += PtrOps::byteOffset(srcData, srcPtr);
    _processedBytes -= (_bitLength >> 3u);

    _state = DecoderState::kDone;
    _bitWord = 0;
    _bitLength = 0;

    // Update the size of the destination array as it was most likely overallocated.
    ArrayInternal::setSize(&dst, PtrOps::byteOffset(dstStart, dstPtr));
    return BL_SUCCESS;
  }

  // A label where we jump in case we need more input bytes - the input chunk must be fully consumed - the only
  // non-consumed bits can be stored in `bitWord` or in temporary buffers of the decoder (if the current state
  // is a processing of a Huffman header).
NotEnoughInputBytes:
  // The entire input buffer must be consumed.
  BL_ASSERT(srcPtr == srcEnd);

  // Update the size of the destination array.
  ArrayInternal::setSize(&dst, PtrOps::byteOffset(dstStart, dstPtr));

  // Save all states as we have to continue once another input chunk is available.
  bits.storeState(this);
  _state = state;
  _processedBytes += input.size;
  return BL_ERROR_DATA_TRUNCATED;

  // Error in a bit-stream or malformed data - the decoding should never continue if this happens.
ErrorInvalidData:
  // Update the size of the destination array so the user can see the output written so far.
  ArrayInternal::setSize(&dst, PtrOps::byteOffset(dstStart, dstPtr));

  bits.fixLengthAfterFastLoop();
  bits.storeState(this);

  _state = DecoderState::kInvalid;
  _processedBytes += PtrOps::byteOffset(srcData, srcPtr);

  return blTraceError(BL_ERROR_DECOMPRESSION_FAILED);
}

} // {bl::Compression::Deflate}
