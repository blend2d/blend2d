// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// Based on code written in 2014-2016 by Eric Biggers <ebiggers3@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along
// with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.

#include "../api-build_p.h"
#include "../compression/checksum_p.h"
#include "../compression/deflatedefs_p.h"
#include "../compression/deflateencoder_p.h"
#include "../support/intops_p.h"

/*
 * Define to 1 to maintain the full map from match offsets to offset slots.
 * This slightly speeds up translations of match offsets to offset slots, but it
 * uses 32769 bytes of memory rather than the 512 bytes used by the condensed
 * map. The speedup provided by the larger map is most helpful when the
 * near-optimal parsing algorithm is being used.
 */
#define USE_FULL_OFFSET_SLOT_FAST 1

/*
 * DEFLATE uses a 32768 byte sliding window; set the matchfinder parameters
 * appropriately.
 */
#define MATCHFINDER_WINDOW_ORDER 15

#include "matchfinder_p.h"

/*
 * The compressor always chooses a block of at least MIN_BLOCK_LENGTH bytes,
 * except if the last block has to be shorter.
 */
#define MIN_BLOCK_LENGTH 10000

/*
 * The compressor attempts to end blocks after SOFT_MAX_BLOCK_LENGTH bytes, but
 * the final length might be slightly longer due to matches extending beyond
 * this limit.
 */
#define SOFT_MAX_BLOCK_LENGTH 300000

/*
 * The number of observed matches or literals that represents sufficient data to
 * decide whether the current block should be terminated or not.
 */
#define NUM_OBSERVATIONS_PER_BLOCK_CHECK 512


/* Constants specific to the near-optimal parsing algorithm */

/*
 * The maximum number of matches the matchfinder can find at a single position.
 * Since the matchfinder never finds more than one match for the same length,
 * presuming one of each possible length is sufficient for an upper bound.
 * (This says nothing about whether it is worthwhile to consider so many
 * matches; this is just defining the worst case.)
 */
#define MAX_MATCHES_PER_POS (kMaxMatchLen - kMinMatchLen + 1)

/*
 * The number of lz_match structures in the match cache, excluding the extra
 * "overflow" entries.  This value should be high enough so that nearly the
 * time, all matches found in a given block can fit in the match cache.
 * However, fallback behavior (immediately terminating the block) on cache
 * overflow is still required.
 */
#define CACHE_LENGTH (SOFT_MAX_BLOCK_LENGTH * 5)

/*
 * These are the compressor-side limits on the codeword lengths for each Huffman
 * code.  To make outputting bits slightly faster, some of these limits are
 * lower than the limits defined by the DEFLATE format.  This does not
 * significantly affect the compression ratio, at least for the block lengths we
 * use.
 */
#define MAX_LITLEN_CODEWORD_LEN 14

/*
 * The NOSTAT_BITS value for a given alphabet is the number of bits assumed to
 * be needed to output a symbol that was unused in the previous optimization
 * pass. Assigning a default cost allows the symbol to be used in the next
 * optimization pass. However, the cost should be relatively high because the
 * symbol probably won't be used very many times (if at all).
 */
#define LITERAL_NOSTAT_BITS  13
#define LENGTH_NOSTAT_BITS  13
#define OFFSET_NOSTAT_BITS  10

namespace BLCompression {
namespace Deflate {

static const uint8_t minOutputSizeExtras[] = {
  0,    // RAW  - no extra size.
  2 + 4 // ZLIB - 2 bytes header and 4 bytes ADLER32 checksum.
};

// Length slot => length slot base value.
static const uint32_t deflate_length_slot_base[] = {
  3   , 4   , 5   , 6   , 7   , 8   , 9   , 10  ,
  11  , 13  , 15  , 17  , 19  , 23  , 27  , 31  ,
  35  , 43  , 51  , 59  , 67  , 83  , 99  , 115 ,
  131 , 163 , 195 , 227 , 258 ,
};

// Length slot => number of extra length bits.
static const uint8_t deflate_extra_length_bits[] = {
  0   , 0   , 0   , 0   , 0   , 0   , 0   , 0 ,
  1   , 1   , 1   , 1   , 2   , 2   , 2   , 2 ,
  3   , 3   , 3   , 3   , 4   , 4   , 4   , 4 ,
  5   , 5   , 5   , 5   , 0   ,
};

// Offset slot => offset slot base value.
static const uint32_t deflate_offset_slot_base[] = {
  1    , 2    , 3    , 4     , 5     , 7     , 9     , 13    ,
  17   , 25   , 33   , 49    , 65    , 97    , 129   , 193   ,
  257  , 385  , 513  , 769   , 1025  , 1537  , 2049  , 3073  ,
  4097 , 6145 , 8193 , 12289 , 16385 , 24577 ,
};

// Offset slot => number of extra offset bits.
static const uint8_t deflate_extra_offset_bits[] = {
  0    , 0    , 0    , 0     , 1     , 1     , 2     , 2     ,
  3    , 3    , 4    , 4     , 5     , 5     , 6     , 6     ,
  7    , 7    , 8    , 8     , 9     , 9     , 10    , 10    ,
  11   , 11   , 12   , 12    , 13    , 13    ,
};

// Length => length slot.
static const uint8_t deflate_length_slot[kMaxMatchLen + 1] = {
  0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 12,
  12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 16,
  16, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18,
  18, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20,
  20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
  22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
  23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
  24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25,
  25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
  25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26, 26,
  26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
  26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
  27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
  27, 27, 28,
};

// The order in which precode codeword lengths are stored.
static const uint8_t deflate_precode_lens_permutation[kNumPrecodeSymbols] = {
  16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

//! Codewords for the DEFLATE Huffman codes.
struct deflate_codewords {
  uint32_t litlen[kNumLitLenSymbols];
  uint32_t offset[kNumOffsetSymbols];
};

//! Codeword lengths (in bits) for the DEFLATE Huffman codes.
//!
//! A zero length means the corresponding symbol had zero frequency.
struct deflate_lens {
  uint8_t litlen[kNumLitLenSymbols];
  uint8_t offset[kNumOffsetSymbols];
};

//! Codewords and lengths for the DEFLATE Huffman codes.
struct deflate_codes {
  struct deflate_codewords codewords;
  struct deflate_lens lens;
};

//! Symbol frequency counters for the DEFLATE Huffman codes.
struct deflate_freqs {
  uint32_t litlen[kNumLitLenSymbols];
  uint32_t offset[kNumOffsetSymbols];
};

//! Costs for the near-optimal parsing algorithm.
struct deflate_costs {
  //! The cost to output each possible literal.
  uint32_t literal[kNumLiterals];
  //! The cost to output each possible match length.
  uint32_t length[kMaxMatchLen + 1];
  //! The cost to output a match offset of each possible offset slot.
  uint32_t offset_slot[kNumOffsetSymbols];
};

//! Represents a run of literals followed by a match or end-of-block.  This
//! struct is needed to temporarily store items chosen by the parser, since items
//! cannot be written until all items for the block have been chosen and the
//! block's Huffman codes have been computed.
struct deflate_sequence {
  //! The number of literals in the run.
  //!
  //! This may be 0. The literals are not stored explicitly in this structure;
  //! instead, they are read directly from the uncompressed data.
  uint16_t litrunlen;

  //! If 'length' doesn't indicate end-of-block, then this is the offset symbol
  //! of the match which follows the literals.
  uint8_t offset_symbol;

  //! If 'length' doesn't indicate end-of-block, then this is the length slot of
  //! the match which follows the literals.
  uint8_t length_slot;

  //! The length of the match which follows the literals, or 0 if this this
  //! sequence's literal run was the last literal run in the block, so there is
  //! no match that follows it.
  uint16_t length;

  //! If 'length' doesn't indicate end-of-block, then this is the offset of the
  //! match which follows the literals.
  uint16_t offset;
};

//! Represents a byte position in the input data and a node in the graph of possible
//! match/literal choices for the current block.
//!
//! Logically, each incoming edge to this node is labeled with a literal or a
//! match that can be taken to reach this position from an earlier position; and
//! each outgoing edge from this node is labeled with a literal or a match that
//! can be taken to advance from this position to a later position.
//!
//! But these "edges" are actually stored elsewhere (in 'match_cache').
struct deflate_optimum_node {
#define OPTIMUM_OFFSET_SHIFT 9
#define OPTIMUM_LEN_MASK (((uint32_t)1 << OPTIMUM_OFFSET_SHIFT) - 1)

  //! The minimum cost to reach the end of the block from this position.
  uint32_t cost_to_end;

  //! Represents the literal or match that must be chosen from here to reach the
  //! end of the block with the minimum cost. Equivalently, this can be
  //! interpreted as the label of the outgoing edge on the minimum-cost path to
  //! the "end of block" node from this node.
  //!
  //! Notes on the match/literal representation used here:
  //!
  //!   - The low bits of 'item' are the length: 1 if this is a literal, or the
  //!     match length if this is a match.
  //!
  //!   - The high bits of 'item' are the actual literal byte if this is a literal,
  //!     or the match offset if this is a match.
  uint32_t item;
};

/* Block split statistics.  See "Block splitting algorithm" below. */
#define NUM_LITERAL_OBSERVATION_TYPES 8
#define NUM_MATCH_OBSERVATION_TYPES 2
#define NUM_OBSERVATION_TYPES (NUM_LITERAL_OBSERVATION_TYPES + NUM_MATCH_OBSERVATION_TYPES)

struct block_split_stats {
  uint32_t new_observations[NUM_OBSERVATION_TYPES];
  uint32_t observations[NUM_OBSERVATION_TYPES];
  uint32_t num_new_observations;
  uint32_t num_observations;
};

// Deflate encoder implementation.
struct EncoderImpl {
  typedef size_t (*CompressFunc)(EncoderImpl* impl, const uint8_t *, size_t, uint8_t *, size_t) BL_NOEXCEPT;

  void* allocated_ptr;

  // Pointer to the compress() implementation.
  CompressFunc compressFunc;

  // Frequency counters for the current block.
  deflate_freqs freqs;
  // Dynamic Huffman codes for the current block.
  deflate_codes codes;
  // Static Huffman codes.
  deflate_codes static_codes;
  // Block split statistics for the currently pending block.
  block_split_stats split_stats;

  // A table for fast lookups of offset slot by match offset.
  //
  // If the full table is being used, it is a direct mapping from offset
  // to offset slot.
  //
  // If the condensed table is being used, the first 256 entries map
  // directly to the offset slots of offsets 1 through 256. The next 256
  // entries map to the offset slots for the remaining offsets, stepping
  // through the offsets with a stride of 128. This relies on the fact
  // that each of the remaining offset slots contains at least 128 offsets
  // and has an offset base that is a multiple of 128.
#if USE_FULL_OFFSET_SLOT_FAST
  uint8_t offset_slot_fast[kMaxMatchOffset + 1];
#else
  uint8_t offset_slot_fast[512];
#endif

  // The "nice" match length: if a match of this length is found, choose
  // it immediately without further consideration.  */
  uint32_t nice_match_length;

  // The maximum search depth: consider at most this many potential
  // matches at each position.
  uint32_t max_search_depth;

  uint32_t num_optim_passes;

  // Format type.
  uint8_t format;
  // The compression level with which this compressor was created.
  uint8_t compression_level;

  // Temporary space for Huffman code output.
  uint32_t precode_freqs[kNumPrecodeSymbols];
  uint8_t precode_lens[kNumPrecodeSymbols];
  uint32_t precode_codewords[kNumPrecodeSymbols];
  uint32_t precode_items[kNumLitLenSymbols + kNumOffsetSymbols];
  uint32_t num_litlen_syms;
  uint32_t num_offset_syms;
  uint32_t num_explicit_lens;
  uint32_t num_precode_items;
};

struct GreedyEncoderImpl : public EncoderImpl {
  // Hash chain matchfinder.
  hc_matchfinder hc_mf;

  // The matches and literals that the parser has chosen for the current
  // block. The required length of this array is limited by the maximum
  // number of matches that can ever be chosen for a single block, plus one
  // for the special entry at the end.
  deflate_sequence sequences[DIV_ROUND_UP(SOFT_MAX_BLOCK_LENGTH, kMinMatchLen) + 1];
};

struct NearOptimalEncoderImpl : public EncoderImpl {
  // Binary tree matchfinder.
  bt_matchfinder bt_mf;

  // Cached matches for the current block. This array contains the matches that
  // were found at each position in the block. Specifically, for each position,
  // there is a list of matches found at that position, if any, sorted by strictly
  // increasing length. In addition, following the matches for each position, there
  // is a special 'lz_match' whose 'length' member contains the number of matches
  // found at that position, and whose 'offset' member contains the literal at that
  // position.
  //
  // Note: in rare cases, there will be a very high number of matches in the block
  // and this array will overflow. If this happens, we force the end of the current
  // block. CACHE_LENGTH is the length at which we actually check for overflow. The
  // extra slots beyond this are enough to absorb the worst case overflow, which
  // occurs if starting at &match_cache[CACHE_LENGTH - 1], we write MAX_MATCHES_PER_POS
  // matches and a match count header, then skip searching for matches at 'kMaxMatchLen - 1'
  // positions and write the match count header for each.
  lz_match match_cache[CACHE_LENGTH + MAX_MATCHES_PER_POS + kMaxMatchLen - 1];

  // Array of nodes, one per position, for running the  minimum-cost path algorithm.
  //
  // This array must be large enough to accommodate the worst-case number of nodes, which
  // occurs if we find a match of length kMaxMatchLen at position SOFT_MAX_BLOCK_LENGTH - 1,
  // producing a block of length SOFT_MAX_BLOCK_LENGTH - 1 + kMaxMatchLen.  Add one for the
  // end-of-block node.
  deflate_optimum_node optimum_nodes[SOFT_MAX_BLOCK_LENGTH - 1 + kMaxMatchLen + 1];

  // The current cost model being used.
  deflate_costs costs;
};

// Can the specified number of bits always be added to 'bitbuf' after any pending bytes have been flushed?
#define CAN_BUFFER(n)  ((n) <= BLIntOps::bitSizeOf<BLBitWord>() - 7)

// Structure to keep track of the current state of sending bits to the compressed output buffer.
struct deflate_output_bitstream {
  // Bits that haven't yet been written to the output buffer.
  BLBitWord bitbuf;
  // Number of bits currently held in `bitbuf`.
  uint32_t bitcount;

  // Pointer to the beginning of the output buffer.
  uint8_t *begin;
  // Pointer to the position in the output buffer at which the next byte should be written.
  uint8_t *next;
  // Pointer just past the end of the output buffer.
  uint8_t *end;
};

#define MIN_OUTPUT_SIZE  (BLMemOps::kUnalignedMem32 ? sizeof(BLBitWord) : 1)

// Initialize the output bitstream.
static void deflate_init_output(deflate_output_bitstream* os, void* buffer, size_t size) noexcept {
  BL_ASSERT(size >= MIN_OUTPUT_SIZE);
  os->bitbuf = 0;
  os->bitcount = 0;
  os->begin = static_cast<uint8_t*>(buffer);
  os->next = os->begin;
  os->end = os->begin + size - MIN_OUTPUT_SIZE;
}

// Add some bits to the bitbuffer variable of the output bitstream.
//
// The caller must make sure there is enough room.  */
static BL_INLINE void deflate_add_bits(deflate_output_bitstream *os, BLBitWord bits, uint32_t num_bits) noexcept {
  os->bitbuf |= bits << os->bitcount;
  os->bitcount += num_bits;
}

template<typename T>
static BL_INLINE void blWriteT_LE(void* dst, const T& value) noexcept {
  if (sizeof(T) == 1)
    BLMemOps::writeU8(dst, reinterpret_cast<const uint8_t&>(value));
  else if (sizeof(T) == 2)
    BLMemOps::writeU16uLE(dst, reinterpret_cast<const uint16_t&>(value));
  else if (sizeof(T) == 4)
    BLMemOps::writeU32uLE(dst, reinterpret_cast<const uint32_t&>(value));
  else if (sizeof(T) == 8)
    BLMemOps::writeU64uLE(dst, reinterpret_cast<const uint64_t&>(value));
}

// Flush bits from the bitbuffer variable to the output buffer.
static BL_INLINE void deflate_flush_bits(deflate_output_bitstream* os) noexcept {
  if (BLMemOps::kUnalignedMem) {
    // Flush a whole word (branchlessly).
    blWriteT_LE(os->next, os->bitbuf);
    os->bitbuf >>= os->bitcount & ~7;
    os->next += blMin<size_t>((size_t)(os->end - os->next), os->bitcount >> 3);
    os->bitcount &= 7;
  }
  else {
    // Flush a byte at a time.
    while (os->bitcount >= 8) {
      *os->next = uint8_t(os->bitbuf & 0xFFu);
      if (os->next != os->end)
        os->next++;
      os->bitcount -= 8;
      os->bitbuf >>= 8;
    }
  }
}

// Align the bitstream on a byte boundary.
static BL_INLINE void deflate_align_bitstream(deflate_output_bitstream* os) noexcept {
  os->bitcount += BLIntOps::negate(os->bitcount) & 0x7u;
  deflate_flush_bits(os);
}

// Flush any remaining bits to the output buffer if needed.  Return the total
// number of bytes written to the output buffer, or 0 if an overflow occurred.
static uint32_t deflate_flush_output(deflate_output_bitstream* os) noexcept {
  // Overflow?
  if (os->next == os->end)
    return 0;

  while ((int)os->bitcount > 0) {
    *os->next++ = uint8_t(os->bitbuf & 0xFFu);
    os->bitcount -= 8;
    os->bitbuf >>= 8;
  }

  return uint32_t(os->next - os->begin);
}

// Given the binary tree node A[subtree_idx] whose children already satisfy the
// maxheap property, swap the node with its greater child until it is greater
// than both its children, so that the maxheap property is satisfied in the
// subtree rooted at A[subtree_idx].
static void heapify_subtree(uint32_t* A, uint32_t length, uint32_t subtree_idx) {
  uint32_t v = A[subtree_idx];
  uint32_t parent_idx = subtree_idx;
  uint32_t child_idx;

  while ((child_idx = parent_idx * 2) <= length) {
    if (child_idx < length && A[child_idx + 1] > A[child_idx])
      child_idx++;
    if (v >= A[child_idx])
      break;
    A[parent_idx] = A[child_idx];
    parent_idx = child_idx;
  }
  A[parent_idx] = v;
}

// Rearrange the array 'A' so that it satisfies the maxheap property. 'A' uses
// 1-based indices, so the children of A[i] are A[i*2] and A[i*2 + 1].
static void heapify_array(uint32_t* A, uint32_t length) {
  for (uint32_t subtree_idx = length / 2; subtree_idx >= 1; subtree_idx--)
    heapify_subtree(A, length, subtree_idx);
}

// Sort the array 'A', which contains 'length' uint32_t 32-bit integers.
//
// Note: name this function heap_sort() instead of heapsort() to avoid colliding
// with heapsort() from stdlib.h on BSD-derived systems --- though this isn't
// necessary when compiling with -D_ANSI_SOURCE, which is the better solution.
static void heap_sort(uint32_t* A, uint32_t length) {
  A--; /* Use 1-based indices  */

  heapify_array(A, length);

  while (length >= 2) {
    uint32_t tmp = A[length];
    A[length] = A[1];
    A[1] = tmp;
    length--;
    heapify_subtree(A, length, 1);
  }
}

#define NUM_SYMBOL_BITS 10
#define SYMBOL_MASK ((1 << NUM_SYMBOL_BITS) - 1)
#define GET_NUM_COUNTERS(num_syms)  ((((num_syms) + 3 / 4) + 3) & ~3)

/*
 * Sort the symbols primarily by frequency and secondarily by symbol
 * value.  Discard symbols with zero frequency and fill in an array with
 * the remaining symbols, along with their frequencies.  The low
 * NUM_SYMBOL_BITS bits of each array entry will contain the symbol
 * value, and the remaining bits will contain the frequency.
 *
 * @num_syms
 *  Number of symbols in the alphabet.
 *  Can't be greater than (1 << NUM_SYMBOL_BITS).
 *
 * @freqs[num_syms]
 *  The frequency of each symbol.
 *
 * @lens[num_syms]
 *  An array that eventually will hold the length of each codeword.
 *  This function only fills in the codeword lengths for symbols that
 *  have zero frequency, which are not well defined per se but will
 *  be set to 0.
 *
 * @symout[num_syms]
 *  The output array, described above.
 *
 * Returns the number of entries in 'symout' that were filled.  This is
 * the number of symbols that have nonzero frequency.
 */
static uint32_t sort_symbols(uint32_t num_syms, const uint32_t* BL_RESTRICT freqs, uint8_t* BL_RESTRICT lens, uint32_t* BL_RESTRICT symout) noexcept {
  // We rely on heapsort, but with an added optimization. Since it's common for
  // most symbol frequencies to be low, we first do a count sort using a limited
  // number of counters. High frequencies will be counted in the last counter,
  // and only they will be sorted with heapsort.
  //
  // Note: with more symbols, it is generally beneficial to have more counters.
  // About 1 counter per 4 symbols seems fast.
  //
  // Note: I also tested radix sort, but even for large symbol counts (> 255)
  // and frequencies bounded at 16 bits (enabling radix sort by just two base-256
  // digits), it didn't seem any faster than the method implemented here.
  //
  // Note: I tested the optimized quicksort implementation from glibc (with
  // indirection overhead removed), but it was only marginally faster than the
  // simple heapsort implemented here.
  //
  // Tests were done with building the codes for LZX.
  // Results may vary for different compression algorithms...!

  uint32_t sym;

  uint32_t num_counters = GET_NUM_COUNTERS(num_syms);
  uint32_t counters[GET_NUM_COUNTERS(kMaxSymbolCount)] {};

  /* Count the frequencies.  */
  for (sym = 0; sym < num_syms; sym++)
    counters[blMin(freqs[sym], num_counters - 1)]++;

  /* Make the counters cumulative, ignoring the zero-th, which
   * counted symbols with zero frequency.  As a side effect, this
   * calculates the number of symbols with nonzero frequency.  */
  uint32_t num_used_syms = 0;
  for (uint32_t i = 1; i < num_counters; i++) {
    uint32_t count = counters[i];
    counters[i] = num_used_syms;
    num_used_syms += count;
  }

  /* Sort nonzero-frequency symbols using the counters.  At the
   * same time, set the codeword lengths of zero-frequency symbols
   * to 0.  */
  for (sym = 0; sym < num_syms; sym++) {
    uint32_t freq = freqs[sym];
    if (freq != 0)
      symout[counters[blMin(freq, num_counters - 1)]++] = sym | (freq << NUM_SYMBOL_BITS);
    else
      lens[sym] = 0;
  }

  /* Sort the symbols counted in the last counter.  */
  heap_sort(symout + counters[num_counters - 2],
      counters[num_counters - 1] - counters[num_counters - 2]);

  return num_used_syms;
}

/*
 * Build the Huffman tree.
 *
 * This is an optimized implementation that
 *  (a) takes advantage of the frequencies being already sorted;
 *  (b) only generates non-leaf nodes, since the non-leaf nodes of a
 *      Huffman tree are sufficient to generate a canonical code;
 *  (c) Only stores parent pointers, not child pointers;
 *  (d) Produces the nodes in the same memory used for input
 *      frequency information.
 *
 * Array 'A', which contains 'sym_count' entries, is used for both input
 * and output.  For this function, 'sym_count' must be at least 2.
 *
 * For input, the array must contain the frequencies of the symbols,
 * sorted in increasing order.  Specifically, each entry must contain a
 * frequency left shifted by NUM_SYMBOL_BITS bits.  Any data in the low
 * NUM_SYMBOL_BITS bits of the entries will be ignored by this function.
 * Although these bits will, in fact, contain the symbols that correspond
 * to the frequencies, this function is concerned with frequencies only
 * and keeps the symbols as-is.
 *
 * For output, this function will produce the non-leaf nodes of the
 * Huffman tree.  These nodes will be stored in the first (sym_count - 1)
 * entries of the array.  Entry A[sym_count - 2] will represent the root
 * node.  Each other node will contain the zero-based index of its parent
 * node in 'A', left shifted by NUM_SYMBOL_BITS bits.  The low
 * NUM_SYMBOL_BITS bits of each entry in A will be kept as-is.  Again,
 * note that although these low bits will, in fact, contain a symbol
 * value, this symbol will have *no relationship* with the Huffman tree
 * node that happens to occupy the same slot.  This is because this
 * implementation only generates the non-leaf nodes of the tree.
 */
static void build_tree(uint32_t* A, uint32_t sym_count) noexcept {
  // Index, in 'A', of next lowest frequency symbol that has not yet been processed.
  uint32_t i = 0;

  // Index, in 'A', of next lowest frequency parentless non-leaf node; or, if equal
  // to 'e', then no such node exists yet.
  uint32_t b = 0;

  // Index, in 'A', of next node to allocate as a non-leaf.
  uint32_t e = 0;

  do {
    uint32_t m, n;

    // Choose the two next lowest frequency entries.
    if (i != sym_count && (b == e || (A[i] >> NUM_SYMBOL_BITS) <= (A[b] >> NUM_SYMBOL_BITS)))
      m = i++;
    else
      m = b++;

    if (i != sym_count && (b == e || (A[i] >> NUM_SYMBOL_BITS) <= (A[b] >> NUM_SYMBOL_BITS)))
      n = i++;
    else
      n = b++;

    // Allocate a non-leaf node and link the entries to it.
    //
    // If we link an entry that we're visiting for the first  time (via index 'i'),
    // then we're actually linking a leaf node and it will have no effect, since the
    // leaf will be overwritten with a non-leaf when index 'e' catches up to it. But
    // it's not any slower to unconditionally set the parent index.
    //
    // We also compute the frequency of the non-leaf node as the sum of its two
    // children's frequencies.
    uint32_t freq_shifted = (A[m] & ~SYMBOL_MASK) + (A[n] & ~SYMBOL_MASK);
    A[m] = (A[m] & SYMBOL_MASK) | (e << NUM_SYMBOL_BITS);
    A[n] = (A[n] & SYMBOL_MASK) | (e << NUM_SYMBOL_BITS);
    A[e] = (A[e] & SYMBOL_MASK) | freq_shifted;
    e++;
  } while (sym_count - e > 1);
  // When just one entry remains, it is a "leaf" that was linked to some other node.
  // We ignore it, since the rest of the array contains the non-leaves which we need.
  // (Note that we're assuming the cases with 0 or 1 symbols were handled separately)
}

/*
 * Given the stripped-down Huffman tree constructed by build_tree(),
 * determine the number of codewords that should be assigned each
 * possible length, taking into account the length-limited constraint.
 *
 * @A
 *  The array produced by build_tree(), containing parent index
 *  information for the non-leaf nodes of the Huffman tree.  Each
 *  entry in this array is a node; a node's parent always has a
 *  greater index than that node itself.  This function will
 *  overwrite the parent index information in this array, so
 *  essentially it will destroy the tree.  However, the data in the
 *  low NUM_SYMBOL_BITS of each entry will be preserved.
 *
 * @root_idx
 *  The 0-based index of the root node in 'A', and consequently one
 *  less than the number of tree node entries in 'A'.  (Or, really 2
 *  less than the actual length of 'A'.)
 *
 * @len_counts
 *  An array of length ('max_codeword_len' + 1) in which the number of
 *  codewords having each length <= max_codeword_len will be
 *  returned.
 *
 * @max_codeword_len
 *  The maximum permissible codeword length.
 */
static void compute_length_counts(uint32_t* BL_RESTRICT A, uint32_t root_idx, unsigned* BL_RESTRICT len_counts, uint32_t max_codeword_len) noexcept {
  /* The key observations are:
   *
   * (1) We can traverse the non-leaf nodes of the tree, always
   * visiting a parent before its children, by simply iterating
   * through the array in reverse order.  Consequently, we can
   * compute the depth of each node in one pass, overwriting the
   * parent indices with depths.
   *
   * (2) We can initially assume that in the real Huffman tree,
   * both children of the root are leaves.  This corresponds to two
   * codewords of length 1.  Then, whenever we visit a (non-leaf)
   * node during the traversal, we modify this assumption to
   * account for the current node *not* being a leaf, but rather
   * its two children being leaves.  This causes the loss of one
   * codeword for the current depth and the addition of two
   * codewords for the current depth plus one.
   *
   * (3) We can handle the length-limited constraint fairly easily
   * by simply using the largest length available when a depth
   * exceeds max_codeword_len.
   */

  for (uint32_t len = 0; len <= max_codeword_len; len++)
    len_counts[len] = 0;
  len_counts[1] = 2;

  /* Set the root node's depth to 0.  */
  A[root_idx] &= SYMBOL_MASK;

  for (int node = root_idx - 1; node >= 0; node--) {
    /* Calculate the depth of this node.  */
    uint32_t parent = A[node] >> NUM_SYMBOL_BITS;
    uint32_t parent_depth = A[parent] >> NUM_SYMBOL_BITS;
    uint32_t depth = parent_depth + 1;
    uint32_t len = depth;

    /* Set the depth of this node so that it is available
     * when its children (if any) are processed.  */

    A[node] = (A[node] & SYMBOL_MASK) | (depth << NUM_SYMBOL_BITS);

    /* If needed, decrease the length to meet the
     * length-limited constraint.  This is not the optimal
     * method for generating length-limited Huffman codes!
     * But it should be good enough.  */
    if (len >= max_codeword_len) {
      len = max_codeword_len;
      do {
        len--;
      } while (len_counts[len] == 0);
    }

    /* Account for the fact that we have a non-leaf node at
     * the current depth.  */
    len_counts[len]--;
    len_counts[len + 1] += 2;
  }
}

/*
 * Generate the codewords for a canonical Huffman code.
 *
 * @A
 *  The output array for codewords.  In addition, initially this
 *  array must contain the symbols, sorted primarily by frequency and
 *  secondarily by symbol value, in the low NUM_SYMBOL_BITS bits of
 *  each entry.
 *
 * @len
 *  Output array for codeword lengths.
 *
 * @len_counts
 *  An array that provides the number of codewords that will have
 *  each possible length <= max_codeword_len.
 *
 * @max_codeword_len
 *  Maximum length, in bits, of each codeword.
 *
 * @num_syms
 *  Number of symbols in the alphabet, including symbols with zero
 *  frequency.  This is the length of the 'A' and 'len' arrays.
 */
static void gen_codewords(uint32_t* BL_RESTRICT A, uint8_t* BL_RESTRICT lens, const unsigned* BL_RESTRICT len_counts, uint32_t max_codeword_len, uint32_t num_syms) noexcept {
  // Given the number of codewords that will have each length, assign codeword
  // lengths to symbols. We do this by assigning the lengths in decreasing order
  // to the symbols sorted primarily by increasing frequency and secondarily by
  // increasing symbol value.
  for (uint32_t i = 0, len = max_codeword_len; len >= 1; len--) {
    uint32_t count = len_counts[len];
    while (count--)
      lens[A[i++] & SYMBOL_MASK] = uint8_t(len);
  }

  // Generate the codewords themselves.  We initialize the 'next_codewords'
  // array to provide the lexicographically first codeword of each length,
  // then assign codewords in symbol order. This produces a canonical code.
  uint32_t next_codewords[kMaxCodeWordLen + 1];
  next_codewords[0] = 0;
  next_codewords[1] = 0;
  for (uint32_t len = 2; len <= max_codeword_len; len++)
    next_codewords[len] = (next_codewords[len - 1] + len_counts[len - 1]) << 1;

  for (uint32_t sym = 0; sym < num_syms; sym++)
    A[sym] = next_codewords[lens[sym]]++;
}

/*
 * ---------------------------------------------------------------------
 *      make_canonical_huffman_code()
 * ---------------------------------------------------------------------
 *
 * Given an alphabet and the frequency of each symbol in it, construct a
 * length-limited canonical Huffman code.
 *
 * @num_syms
 *  The number of symbols in the alphabet.  The symbols are the
 *  integers in the range [0, num_syms - 1].  This parameter must be
 *  at least 2 and can't be greater than (1 << NUM_SYMBOL_BITS).
 *
 * @max_codeword_len
 *  The maximum permissible codeword length.
 *
 * @freqs
 *  An array of @num_syms entries, each of which specifies the
 *  frequency of the corresponding symbol.  It is valid for some,
 *  none, or all of the frequencies to be 0.
 *
 * @lens
 *  An array of @num_syms entries in which this function will return
 *  the length, in bits, of the codeword assigned to each symbol.
 *  Symbols with 0 frequency will not have codewords per se, but
 *  their entries in this array will be set to 0.  No lengths greater
 *  than @max_codeword_len will be assigned.
 *
 * @codewords
 *  An array of @num_syms entries in which this function will return
 *  the codeword for each symbol, right-justified and padded on the
 *  left with zeroes.  Codewords for symbols with 0 frequency will be
 *  undefined.
 *
 * ---------------------------------------------------------------------
 *
 * This function builds a length-limited canonical Huffman code.
 *
 * A length-limited Huffman code contains no codewords longer than some
 * specified length, and has exactly (with some algorithms) or
 * approximately (with the algorithm used here) the minimum weighted path
 * length from the root, given this constraint.
 *
 * A canonical Huffman code satisfies the properties that a longer
 * codeword never lexicographically precedes a shorter codeword, and the
 * lexicographic ordering of codewords of the same length is the same as
 * the lexicographic ordering of the corresponding symbols.  A canonical
 * Huffman code, or more generally a canonical prefix code, can be
 * reconstructed from only a list containing the codeword length of each
 * symbol.
 *
 * The classic algorithm to generate a Huffman code creates a node for
 * each symbol, then inserts these nodes into a min-heap keyed by symbol
 * frequency.  Then, repeatedly, the two lowest-frequency nodes are
 * removed from the min-heap and added as the children of a new node
 * having frequency equal to the sum of its two children, which is then
 * inserted into the min-heap.  When only a single node remains in the
 * min-heap, it is the root of the Huffman tree.  The codeword for each
 * symbol is determined by the path needed to reach the corresponding
 * node from the root.  Descending to the left child appends a 0 bit,
 * whereas descending to the right child appends a 1 bit.
 *
 * The classic algorithm is relatively easy to understand, but it is
 * subject to a number of inefficiencies.  In practice, it is fastest to
 * first sort the symbols by frequency.  (This itself can be subject to
 * an optimization based on the fact that most frequencies tend to be
 * low.)  At the same time, we sort secondarily by symbol value, which
 * aids the process of generating a canonical code.  Then, during tree
 * construction, no heap is necessary because both the leaf nodes and the
 * unparented non-leaf nodes can be easily maintained in sorted order.
 * Consequently, there can never be more than two possibilities for the
 * next-lowest-frequency node.
 *
 * In addition, because we're generating a canonical code, we actually
 * don't need the leaf nodes of the tree at all, only the non-leaf nodes.
 * This is because for canonical code generation we don't need to know
 * where the symbols are in the tree.  Rather, we only need to know how
 * many leaf nodes have each depth (codeword length).  And this
 * information can, in fact, be quickly generated from the tree of
 * non-leaves only.
 *
 * Furthermore, we can build this stripped-down Huffman tree directly in
 * the array in which the codewords are to be generated, provided that
 * these array slots are large enough to hold a symbol and frequency
 * value.
 *
 * Still furthermore, we don't even need to maintain explicit child
 * pointers.  We only need the parent pointers, and even those can be
 * overwritten in-place with depth information as part of the process of
 * extracting codeword lengths from the tree.  So in summary, we do NOT
 * need a big structure like:
 *
 *  struct huffman_tree_node {
 *    uint32_t int symbol;
 *    uint32_t int frequency;
 *    uint32_t int depth;
 *    struct huffman_tree_node *left_child;
 *    struct huffman_tree_node *right_child;
 *  };
 *
 *
 *   ... which often gets used in "naive" implementations of Huffman code
 *   generation.
 *
 * Many of these optimizations are based on the implementation in 7-Zip
 * (source file: C/HuffEnc.c), which has been placed in the public domain
 * by Igor Pavlov.
 */
static void make_canonical_huffman_code(uint32_t num_syms, uint32_t max_codeword_len, const uint32_t* BL_RESTRICT freqs, uint8_t* BL_RESTRICT lens, uint32_t* BL_RESTRICT codewords) noexcept {
  BL_STATIC_ASSERT(kMaxSymbolCount <= 1 << NUM_SYMBOL_BITS);

  // We begin by sorting the symbols primarily by frequency and secondarily by
  // symbol value. As an optimization, the array used for this purpose ('A')
  // shares storage with the space in which we will eventually return the codewords.
  uint32_t* A = codewords;
  uint32_t num_used_syms = sort_symbols(num_syms, freqs, lens, A);

  // 'num_used_syms' is the number of symbols with nonzero frequency. This may be
  // less than @num_syms. `num_used_syms` is also the number of entries in 'A' that
  // are valid. Each entry consists of a distinct symbol and a nonzero frequency
  // packed into a 32-bit integer.

  // Handle special cases where only 0 or 1 symbols were used (had nonzero frequency).
  if (BL_UNLIKELY(num_used_syms == 0)) {
    // Code is empty. `sort_symbols()` already set all lengths to 0, so there is
    // nothing more to do.
    return;
  }

  if (BL_UNLIKELY(num_used_syms == 1)) {
    // Only one symbol was used, so we only need one codeword. But two codewords
    // are needed to form the smallest complete Huffman code, which uses codewords
    // 0 and 1. Therefore, we choose another symbol to which to assign a codeword.
    // We use 0 (if the used symbol is not 0) or 1 (if the used symbol is 0). In
    // either case, the lesser-valued symbol must be assigned codeword 0 so that
    // the resulting code is canonical.
    uint32_t sym = A[0] & SYMBOL_MASK;
    uint32_t nonzero_idx = sym ? sym : 1;

    codewords[0] = 0;
    lens[0] = 1;
    codewords[nonzero_idx] = 1;
    lens[nonzero_idx] = 1;
    return;
  }

  // Build a stripped-down version of the Huffman tree, sharing the array 'A' with
  // the symbol values. Then extract length counts from the tree and use them to
  // generate the final codewords.
  build_tree(A, num_used_syms);

  {
    uint32_t len_counts[kMaxCodeWordLen + 1];
    compute_length_counts(A, num_used_syms - 2, len_counts, max_codeword_len);
    gen_codewords(A, lens, len_counts, max_codeword_len, num_syms);
  }
}

// Clear the Huffman symbol frequency counters.
//
// This must be called when starting a new DEFLATE block.
static BL_INLINE void deflate_reset_symbol_frequencies(EncoderImpl* impl) noexcept {
  memset(&impl->freqs, 0, sizeof(impl->freqs));
}

// Reverse the Huffman codeword 'codeword', which is 'len' bits in length.
static uint32_t deflate_reverse_codeword(uint32_t codeword, uint8_t len) noexcept {
  // The following branchless algorithm is faster than going bit by bit.
  //
  // NOTE: since no codewords are longer than 16 bits, we only need to
  // reverse the low 16 bits of the 'uint32_t'.  */
  BL_STATIC_ASSERT(kMaxCodeWordLen <= 16);

  codeword = ((codeword & 0x5555) << 1) | ((codeword & 0xAAAA) >> 1);
  codeword = ((codeword & 0x3333) << 2) | ((codeword & 0xCCCC) >> 2);
  codeword = ((codeword & 0x0F0F) << 4) | ((codeword & 0xF0F0) >> 4);
  codeword = ((codeword & 0x00FF) << 8) | ((codeword & 0xFF00) >> 8);

  // Return the high `len` bits of the bit-reversed 16-bit value.
  return codeword >> (16 - len);
}

/* Make a canonical Huffman code with bit-reversed codewords.  */
static void deflate_make_huffman_code(uint32_t num_syms, uint32_t max_codeword_len, const uint32_t freqs[], uint8_t lens[], uint32_t codewords[])
{
  make_canonical_huffman_code(num_syms, max_codeword_len, freqs, lens, codewords);
  for (uint32_t sym = 0; sym < num_syms; sym++)
    codewords[sym] = deflate_reverse_codeword(codewords[sym], lens[sym]);
}

// Build the literal/length and offset Huffman codes for a DEFLATE block.
//
// This takes as input the frequency tables for each code and produces as output
// a set of tables that map symbols to codewords and codeword lengths.
static void deflate_make_huffman_codes(const deflate_freqs *freqs, deflate_codes *codes) noexcept {
  BL_STATIC_ASSERT(MAX_LITLEN_CODEWORD_LEN <= kMaxLitLenCodeWordLen);
  BL_STATIC_ASSERT(kMaxOffsetCodeWordLen <= kMaxOffsetCodeWordLen);

  deflate_make_huffman_code(kNumLitLenSymbols, MAX_LITLEN_CODEWORD_LEN, freqs->litlen, codes->lens.litlen, codes->codewords.litlen);
  deflate_make_huffman_code(kNumOffsetSymbols, kMaxOffsetCodeWordLen, freqs->offset, codes->lens.offset, codes->codewords.offset);
}

// Initialize impl->static_codes.
static void deflate_init_static_codes(EncoderImpl* impl) noexcept {
  uint32_t i;

  for (i = 0; i < 144; i++)
    impl->freqs.litlen[i] = 1 << (9 - 8);
  for (; i < 256; i++)
    impl->freqs.litlen[i] = 1 << (9 - 9);
  for (; i < 280; i++)
    impl->freqs.litlen[i] = 1 << (9 - 7);
  for (; i < 288; i++)
    impl->freqs.litlen[i] = 1 << (9 - 8);

  for (i = 0; i < 32; i++)
    impl->freqs.offset[i] = 1 << (5 - 5);

  deflate_make_huffman_codes(&impl->freqs, &impl->static_codes);
}

// Return the offset slot for the specified match offset.
static BL_INLINE uint32_t deflate_get_offset_slot(EncoderImpl* impl, uint32_t offset) noexcept {
#if USE_FULL_OFFSET_SLOT_FAST
  return impl->offset_slot_fast[offset];
#else
  if (offset <= 256)
    return impl->offset_slot_fast[offset - 1];
  else
    return impl->offset_slot_fast[256 + ((offset - 1) >> 7)];
#endif
}

// Write the header fields common to all DEFLATE block types.
static void deflate_write_block_header(deflate_output_bitstream *os, bool is_final_block, uint32_t block_type) noexcept {
  deflate_add_bits(os, is_final_block, 1);
  deflate_add_bits(os, block_type, 2);
  deflate_flush_bits(os);
}

static uint32_t deflate_compute_precode_items(const uint8_t* BL_RESTRICT lens, const uint32_t num_lens, uint32_t* BL_RESTRICT precode_freqs, unsigned* BL_RESTRICT precode_items) noexcept {
  memset(precode_freqs, 0, kNumPrecodeSymbols * sizeof(precode_freqs[0]));

  unsigned* itemptr = precode_items;
  uint32_t run_start = 0;
  do {
    /* Find the next run of codeword lengths.  */

    /* len = the length being repeated  */
    uint8_t len = lens[run_start];

    /* Extend the run.  */
    uint32_t run_end = run_start;
    do {
      run_end++;
    } while (run_end != num_lens && len == lens[run_end]);

    if (len == 0) {
      /* Run of zeroes.  */

      /* Symbol 18: RLE 11 to 138 zeroes at a time.  */
      while ((run_end - run_start) >= 11) {
        uint32_t extra_bits = blMin<uint32_t>((run_end - run_start) - 11, 0x7F);
        precode_freqs[18]++;
        *itemptr++ = 18 | (extra_bits << 5);
        run_start += 11 + extra_bits;
      }

      /* Symbol 17: RLE 3 to 10 zeroes at a time.  */
      if ((run_end - run_start) >= 3) {
        uint32_t extra_bits = blMin<uint32_t>((run_end - run_start) - 3, 0x7);
        precode_freqs[17]++;
        *itemptr++ = 17 | (extra_bits << 5);
        run_start += 3 + extra_bits;
      }
    }
    else {
      /* A run of nonzero lengths. */

      /* Symbol 16: RLE 3 to 6 of the previous length.  */
      if ((run_end - run_start) >= 4) {
        precode_freqs[len]++;
        *itemptr++ = len;
        run_start++;
        do {
          uint32_t extra_bits = blMin<uint32_t>((run_end - run_start) - 3, 0x3);
          precode_freqs[16]++;
          *itemptr++ = 16 | (extra_bits << 5);
          run_start += 3 + extra_bits;
        } while ((run_end - run_start) >= 3);
      }
    }

    /* Output any remaining lengths without RLE.  */
    while (run_start != run_end) {
      precode_freqs[len]++;
      *itemptr++ = len;
      run_start++;
    }
  } while (run_start != num_lens);

  return uint32_t(itemptr - precode_items);
}

/*
 * Huffman codeword lengths for dynamic Huffman blocks are compressed using a
 * separate Huffman code, the "precode", which contains a symbol for each
 * possible codeword length in the larger code as well as several special
 * symbols to represent repeated codeword lengths (a form of run-length
 * encoding). The precode is itself constructed in canonical form, and its
 * codeword lengths are represented literally in 19 3-bit fields that
 * immediately precede the compressed codeword lengths of the larger code.
 */

// Precompute the information needed to output Huffman codes.
static void deflate_precompute_huffman_header(EncoderImpl* impl) noexcept {
  // Compute how many litlen and offset symbols are needed.
  for (impl->num_litlen_syms = kNumLitLenSymbols; impl->num_litlen_syms > 257; impl->num_litlen_syms--)
    if (impl->codes.lens.litlen[impl->num_litlen_syms - 1] != 0)
      break;

  for (impl->num_offset_syms = kNumOffsetSymbols; impl->num_offset_syms > 1; impl->num_offset_syms--)
    if (impl->codes.lens.offset[impl->num_offset_syms - 1] != 0)
      break;

  // If we're not using the full set of literal/length codeword lengths, then
  // temporarily move the offset codeword lengths over so that the literal/length
  // and offset codeword lengths are contiguous.
  BL_STATIC_ASSERT(offsetof(deflate_lens, offset) == kNumLitLenSymbols);

  if (impl->num_litlen_syms != kNumLitLenSymbols)
    memmove((uint8_t *)&impl->codes.lens + impl->num_litlen_syms, (uint8_t *)&impl->codes.lens + kNumLitLenSymbols, impl->num_offset_syms);

  // Compute the "items" (RLE / literal tokens and extra bits) with which
  // the codeword lengths in the larger code will be output.
  impl->num_precode_items = deflate_compute_precode_items((uint8_t *)&impl->codes.lens, impl->num_litlen_syms + impl->num_offset_syms, impl->precode_freqs, impl->precode_items);

  // Build the precode.
  BL_STATIC_ASSERT(kMaxPreCodeWordLen <= kMaxPreCodeWordLen);
  deflate_make_huffman_code(kNumPrecodeSymbols, kMaxPreCodeWordLen, impl->precode_freqs, impl->precode_lens, impl->precode_codewords);

  // Count how many precode lengths we actually need to output.
  for (impl->num_explicit_lens = kNumPrecodeSymbols; impl->num_explicit_lens > 4; impl->num_explicit_lens--)
    if (impl->precode_lens[deflate_precode_lens_permutation[impl->num_explicit_lens - 1]] != 0)
      break;

  // Restore the offset codeword lengths if needed.
  if (impl->num_litlen_syms != kNumLitLenSymbols)
    memmove((uint8_t *)&impl->codes.lens + kNumLitLenSymbols, (uint8_t *)&impl->codes.lens + impl->num_litlen_syms, impl->num_offset_syms);
}

/* Output the Huffman codes. */
static void deflate_write_huffman_header(EncoderImpl* impl, deflate_output_bitstream *os) noexcept {
  uint32_t i;

  deflate_add_bits(os, impl->num_litlen_syms - 257, 5);
  deflate_add_bits(os, impl->num_offset_syms - 1, 5);
  deflate_add_bits(os, impl->num_explicit_lens - 4, 4);
  deflate_flush_bits(os);

  /* Output the lengths of the codewords in the precode.  */
  for (i = 0; i < impl->num_explicit_lens; i++) {
    deflate_add_bits(os, impl->precode_lens[deflate_precode_lens_permutation[i]], 3);
    deflate_flush_bits(os);
  }

  /* Output the encoded lengths of the codewords in the larger code.  */
  for (i = 0; i < impl->num_precode_items; i++) {
    uint32_t precode_item = impl->precode_items[i];
    uint32_t precode_sym = precode_item & 0x1F;

    deflate_add_bits(os, impl->precode_codewords[precode_sym], impl->precode_lens[precode_sym]);

    if (precode_sym >= 16) {
      if (precode_sym == 16)
        deflate_add_bits(os, precode_item >> 5, 2);
      else if (precode_sym == 17)
        deflate_add_bits(os, precode_item >> 5, 3);
      else
        deflate_add_bits(os, precode_item >> 5, 7);
    }

    BL_STATIC_ASSERT(CAN_BUFFER(kMaxPreCodeWordLen + 7));
    deflate_flush_bits(os);
  }
}

static void deflate_write_sequences(deflate_output_bitstream* BL_RESTRICT os,
    const deflate_codes* BL_RESTRICT codes,
    const deflate_sequence* BL_RESTRICT sequences,
    const uint8_t * BL_RESTRICT in_next) noexcept {

  const deflate_sequence *seq = sequences;
  for (;;) {
    uint32_t litrunlen = seq->litrunlen;
    uint32_t length;
    uint32_t length_slot;
    uint32_t litlen_symbol;
    uint32_t offset_symbol;

    if (litrunlen) {
#if 1
      while (litrunlen >= 4) {
        uint32_t lit0 = in_next[0];
        uint32_t lit1 = in_next[1];
        uint32_t lit2 = in_next[2];
        uint32_t lit3 = in_next[3];

        deflate_add_bits(os, codes->codewords.litlen[lit0], codes->lens.litlen[lit0]);
        if (!CAN_BUFFER(2 * MAX_LITLEN_CODEWORD_LEN))
          deflate_flush_bits(os);

        deflate_add_bits(os, codes->codewords.litlen[lit1], codes->lens.litlen[lit1]);
        if (!CAN_BUFFER(4 * MAX_LITLEN_CODEWORD_LEN))
          deflate_flush_bits(os);

        deflate_add_bits(os, codes->codewords.litlen[lit2], codes->lens.litlen[lit2]);
        if (!CAN_BUFFER(2 * MAX_LITLEN_CODEWORD_LEN))
          deflate_flush_bits(os);

        deflate_add_bits(os, codes->codewords.litlen[lit3], codes->lens.litlen[lit3]);
        deflate_flush_bits(os);
        in_next += 4;
        litrunlen -= 4;
      }

      if (litrunlen-- != 0) {
        deflate_add_bits(os, codes->codewords.litlen[*in_next], codes->lens.litlen[*in_next]);
        if (!CAN_BUFFER(3 * MAX_LITLEN_CODEWORD_LEN))
          deflate_flush_bits(os);
        in_next++;
        if (litrunlen-- != 0) {
          deflate_add_bits(os, codes->codewords.litlen[*in_next], codes->lens.litlen[*in_next]);
          if (!CAN_BUFFER(3 * MAX_LITLEN_CODEWORD_LEN))
            deflate_flush_bits(os);
          in_next++;
          if (litrunlen-- != 0) {
            deflate_add_bits(os, codes->codewords.litlen[*in_next], codes->lens.litlen[*in_next]);
            if (!CAN_BUFFER(3 * MAX_LITLEN_CODEWORD_LEN))
              deflate_flush_bits(os);
            in_next++;
          }
        }
        if (CAN_BUFFER(3 * MAX_LITLEN_CODEWORD_LEN))
          deflate_flush_bits(os);
      }
#else
      do {
        uint32_t lit = *in_next++;
        deflate_add_bits(os, codes->codewords.litlen[lit], codes->lens.litlen[lit]);
        deflate_flush_bits(os);
      } while (--litrunlen);
#endif
    }

    length = seq->length;
    if (length == 0)
      return;

    in_next += length;
    length_slot = seq->length_slot;
    litlen_symbol = 257 + length_slot;

    // Litlen symbol.
    deflate_add_bits(os, codes->codewords.litlen[litlen_symbol], codes->lens.litlen[litlen_symbol]);

    // Extra length bits.
    BL_STATIC_ASSERT(CAN_BUFFER(MAX_LITLEN_CODEWORD_LEN + kMaxExtraLengthBits));
    deflate_add_bits(os, length - deflate_length_slot_base[length_slot], deflate_extra_length_bits[length_slot]);

    if (!CAN_BUFFER(MAX_LITLEN_CODEWORD_LEN + kMaxExtraLengthBits + kMaxOffsetCodeWordLen + kMaxExtraOffsetBits))
      deflate_flush_bits(os);

    // Offset symbol.
    offset_symbol = seq->offset_symbol;
    deflate_add_bits(os, codes->codewords.offset[offset_symbol], codes->lens.offset[offset_symbol]);

    if (!CAN_BUFFER(kMaxOffsetCodeWordLen + kMaxExtraOffsetBits))
      deflate_flush_bits(os);

    // Extra offset bits.
    deflate_add_bits(os, seq->offset - deflate_offset_slot_base[offset_symbol], deflate_extra_offset_bits[offset_symbol]);
    deflate_flush_bits(os);

    seq++;
  }
}

// Follow the minimum-cost path in the graph of possible match/literal choices
// for the current block and write out the matches/literals using the specified
// Huffman codes.
//
// NOTE: this is slightly duplicated with deflate_write_sequences(), the reason
// being that we don't want to waste time translating between intermediate
// match/literal representations.
static void deflate_write_item_list(deflate_output_bitstream *os, const deflate_codes *codes, NearOptimalEncoderImpl* impl, uint32_t block_length) noexcept {
  deflate_optimum_node* cur_node = &impl->optimum_nodes[0];
  deflate_optimum_node* end_node = &impl->optimum_nodes[block_length];

  do {
    uint32_t length = cur_node->item & OPTIMUM_LEN_MASK;
    uint32_t offset = cur_node->item >> OPTIMUM_OFFSET_SHIFT;
    uint32_t litlen_symbol;
    uint32_t length_slot;
    uint32_t offset_slot;

    if (length == 1) {
      /* Literal  */
      litlen_symbol = offset;
      deflate_add_bits(os, codes->codewords.litlen[litlen_symbol], codes->lens.litlen[litlen_symbol]);
      deflate_flush_bits(os);
    }
    else {
      /* Match length  */
      length_slot = deflate_length_slot[length];
      litlen_symbol = 257 + length_slot;
      deflate_add_bits(os, codes->codewords.litlen[litlen_symbol], codes->lens.litlen[litlen_symbol]);
      deflate_add_bits(os, length - deflate_length_slot_base[length_slot], deflate_extra_length_bits[length_slot]);

      if (!CAN_BUFFER(MAX_LITLEN_CODEWORD_LEN + kMaxExtraLengthBits + kMaxOffsetCodeWordLen + kMaxExtraOffsetBits))
        deflate_flush_bits(os);

      /* Match offset  */
      offset_slot = deflate_get_offset_slot(impl, offset);
      deflate_add_bits(os, codes->codewords.offset[offset_slot], codes->lens.offset[offset_slot]);

      if (!CAN_BUFFER(kMaxOffsetCodeWordLen + kMaxExtraOffsetBits))
        deflate_flush_bits(os);

      deflate_add_bits(os, offset - deflate_offset_slot_base[offset_slot], deflate_extra_offset_bits[offset_slot]);
      deflate_flush_bits(os);
    }
    cur_node += length;
  } while (cur_node != end_node);
}

// Output the end-of-block symbol.
static void deflate_write_end_of_block(deflate_output_bitstream *os, const deflate_codes *codes) noexcept {
  deflate_add_bits(os, codes->codewords.litlen[kEndOfBlock], codes->lens.litlen[kEndOfBlock]);
  deflate_flush_bits(os);
}

static void deflate_write_uncompressed_block(deflate_output_bitstream *os, const uint8_t *data, uint32_t len, bool is_final_block) noexcept {
  deflate_write_block_header(os, is_final_block, kBlockTypeUncompressed);
  deflate_align_bitstream(os);

  if (len + 4u >= size_t(os->end - os->next)) {
    os->next = os->end;
    return;
  }

  BLMemOps::writeU16uLE(os->next, len);
  os->next += 2;
  BLMemOps::writeU16uLE(os->next, ~len);
  os->next += 2;
  memcpy(os->next, data, len);
  os->next += len;
}

static void deflate_write_uncompressed_blocks(deflate_output_bitstream *os, const uint8_t *data, uint32_t data_length, bool is_final_block) noexcept {
  do {
    uint32_t len = blMin<uint32_t>(data_length, 0xFFFFu);
    deflate_write_uncompressed_block(os, data, len, is_final_block && len == data_length);
    data += len;
    data_length -= len;
  } while (data_length != 0);
}

// Choose the best type of block to use (dynamic Huffman, static Huffman, or uncompressed), then output it.
static void deflate_flush_block(EncoderImpl* impl, deflate_output_bitstream* BL_RESTRICT os, const uint8_t* BL_RESTRICT block_begin, uint32_t block_length, bool is_final_block, bool use_item_list) noexcept {
  static const uint8_t deflate_extra_precode_bits[kNumPrecodeSymbols] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 7, };

  uint32_t sym;

  // Costs are measured in bits.
  uint32_t dynamic_cost = 0;
  uint32_t static_cost = 0;
  uint32_t uncompressed_cost = 0;

  // Tally the end-of-block symbol.
  impl->freqs.litlen[kEndOfBlock]++;

  // Build dynamic Huffman codes.
  deflate_codes *codes = nullptr;
  deflate_make_huffman_codes(&impl->freqs, &impl->codes);

  // Account for the cost of sending dynamic Huffman codes.
  deflate_precompute_huffman_header(impl);
  dynamic_cost += 5 + 5 + 4 + (3 * impl->num_explicit_lens);
  for (sym = 0; sym < kNumPrecodeSymbols; sym++) {
    uint32_t extra = deflate_extra_precode_bits[sym];
    dynamic_cost += impl->precode_freqs[sym] *
        (extra + impl->precode_lens[sym]);
  }

  // Account for the cost of encoding literals.
  for (sym = 0; sym < 256; sym++)
    dynamic_cost += impl->freqs.litlen[sym] * impl->codes.lens.litlen[sym];

  for (sym = 0; sym < 144; sym++)
    static_cost += impl->freqs.litlen[sym] * 8;

  for (; sym < 256; sym++)
    static_cost += impl->freqs.litlen[sym] * 9;

  // Account for the cost of encoding the end-of-block symbol.
  dynamic_cost += impl->codes.lens.litlen[256];
  static_cost += 7;

  // Account for the cost of encoding lengths.
  for (sym = 257; sym < 257 + BL_ARRAY_SIZE(deflate_extra_length_bits); sym++) {
    uint32_t extra = deflate_extra_length_bits[sym - 257];
    dynamic_cost += impl->freqs.litlen[sym] * (extra + impl->codes.lens.litlen[sym]);
    static_cost += impl->freqs.litlen[sym] * (extra + impl->static_codes.lens.litlen[sym]);
  }

  // Account for the cost of encoding offsets.
  for (sym = 0; sym < BL_ARRAY_SIZE(deflate_extra_offset_bits); sym++) {
    uint32_t extra = deflate_extra_offset_bits[sym];
    dynamic_cost += impl->freqs.offset[sym] * (extra + impl->codes.lens.offset[sym]);
    static_cost += impl->freqs.offset[sym] * (extra + 5);
  }

  // Compute the cost of using uncompressed blocks.
  uncompressed_cost += (BLIntOps::negate(os->bitcount + 3u) & 7u) + 32u + (40 * (DIV_ROUND_UP(block_length, UINT16_MAX) - 1)) + (8 * block_length);

  // Choose the cheapest block type.
  uint32_t block_type;
  if (dynamic_cost < blMin(static_cost, uncompressed_cost)) {
    block_type = kBlockTypeDynamicHuffman;
    codes = &impl->codes;
  }
  else if (static_cost < uncompressed_cost) {
    block_type = kBlockTypeStaticHuffman;
    codes = &impl->static_codes;
  }
  else {
    block_type = kBlockTypeUncompressed;
  }

  // Now actually output the block.
  if (block_type == kBlockTypeUncompressed) {
    // NOTE: the length being flushed may exceed the maximum length of an uncompressed
    // block (65535 bytes). Therefore, more than one uncompressed block might be needed.
    deflate_write_uncompressed_blocks(os, block_begin, block_length, is_final_block);
  }
  else {
    // Output the block header.
    deflate_write_block_header(os, is_final_block, block_type);

    // Output the Huffman codes (dynamic Huffman blocks only).
    if (block_type == kBlockTypeDynamicHuffman)
      deflate_write_huffman_header(impl, os);

    // Output the literals, matches, and end-of-block symbol.
    if (!use_item_list)
      deflate_write_sequences(os, codes, static_cast<GreedyEncoderImpl*>(impl)->sequences, block_begin);
    else
      deflate_write_item_list(os, codes, static_cast<NearOptimalEncoderImpl*>(impl), block_length);
    deflate_write_end_of_block(os, codes);
  }
}

static BL_INLINE void deflate_choose_literal(EncoderImpl* impl, uint32_t literal, uint32_t *litrunlen_p) noexcept {
  impl->freqs.litlen[literal]++;
  ++*litrunlen_p;
}

static BL_INLINE void deflate_choose_match(EncoderImpl* impl, uint32_t length, uint32_t offset, uint32_t *litrunlen_p, deflate_sequence **next_seq_p) noexcept {
  uint32_t litrunlen = *litrunlen_p;
  deflate_sequence *seq = *next_seq_p;
  uint32_t length_slot = deflate_length_slot[length];
  uint32_t offset_slot = deflate_get_offset_slot(impl, offset);

  impl->freqs.litlen[257 + length_slot]++;
  impl->freqs.offset[offset_slot]++;

  seq->litrunlen = uint16_t(litrunlen);
  seq->length = uint16_t(length);
  seq->offset = uint16_t(offset);
  seq->length_slot = uint8_t(length_slot);
  seq->offset_symbol = uint8_t(offset_slot);

  *litrunlen_p = 0;
  *next_seq_p = seq + 1;
}

static BL_INLINE void deflate_finish_sequence(deflate_sequence *seq, uint32_t litrunlen) noexcept {
  seq->litrunlen = uint16_t(litrunlen);
  seq->length = 0;
}

/******************************************************************************/

// Block splitting algorithm. The problem is to decide when it is worthwhile to start a new block
// with new Huffman codesThere is a theoretically optimal solution: recursively consider every
// possible block split, considering the exact cost of each block, and choose the minimum cost
// approach. But this is far too slow. Instead, as an approximation, we can count symbols and after
// every N symbols, compare the expected distribution of symbols based on the previous data with
// the actual distribution. If they differ "by enough", then start a new block.
//
// As an optimization and heuristic, we don't distinguish between every symbol but rather we
// combine many symbols into a single "observation type". For literals we only look at the high
// bits and low bits, and for matches we only look at whether the match is long or not. The
// assumption is that for typical "real" data, places that are good block boundaries will tend to
// be noticable based only on changes in these aggregate frequencies, without looking for subtle
// differences in individual symbols. For example, a change from ASCII bytes to non-ASCII bytes,
// or from few matches (generally less compressible) to many matches (generally more compressible),
// would be easily noticed based on the aggregates.
//
// For determining whether the frequency distributions are "different enough" to start a new block,
// the simply heuristic of splitting when the sum of absolute differences exceeds a constant seems
// to be good enough. We also add a number proportional to the block length so that the algorithm
// is more likely to end long blocks than short blocks. This reflects the general expectation that
// it will become increasingly beneficial to start a new block as the current block grows longer.
//
// Finally, for an approximation, it is not strictly necessary that the exact symbols being used are
// considered. With "near-optimal parsing", for example, the actual symbols that will be used are
// unknown until after the block boundary is chosen and the block has been optimized. Since the final
// choices cannot be used, we can use preliminary "greedy" choices instead.

// Initialize the block split statistics when starting a new block.
static void init_block_split_stats(block_split_stats* stats) noexcept {
  uint32_t i;

  for (i = 0; i < NUM_OBSERVATION_TYPES; i++) {
    stats->new_observations[i] = 0;
    stats->observations[i] = 0;
  }
  stats->num_new_observations = 0;
  stats->num_observations = 0;
}

// Literal observation.
//
// Heuristic: use the top 2 bits and low 1 bits of the literal, for 8 possible literal observation types.
static BL_INLINE void observe_literal(block_split_stats* stats, uint8_t lit) noexcept {
  stats->new_observations[((lit >> 5) & 0x6) | (lit & 1)]++;
  stats->num_new_observations++;
}

// Match observation.
//
// Heuristic: use one observation type for "short match" and one observation type for "long match".
static BL_INLINE void observe_match(block_split_stats* stats, uint32_t length) noexcept {
  stats->new_observations[NUM_LITERAL_OBSERVATION_TYPES + (length >= 9)]++;
  stats->num_new_observations++;
}

static bool do_end_block_check(block_split_stats* stats, uint32_t block_length) noexcept {
  uint32_t i;

  if (stats->num_observations > 0) {
    // To avoid slow divisions, we do not divide by `num_observations`, but rather do all math
    // with the numbers multiplied by `num_observations`.
    uint32_t total_delta = 0;

    for (i = 0; i < NUM_OBSERVATION_TYPES; i++) {
      uint32_t expected = stats->observations[i] * stats->num_new_observations;
      uint32_t actual = stats->new_observations[i] * stats->num_observations;
      uint32_t delta = (actual > expected) ? actual - expected : expected - actual;

      total_delta += delta;
    }

    // Ready to end the block?
    if (total_delta + (block_length / 4096) * stats->num_observations >= NUM_OBSERVATIONS_PER_BLOCK_CHECK * 200 / 512 * stats->num_observations)
      return true;
  }

  for (i = 0; i < NUM_OBSERVATION_TYPES; i++) {
    stats->num_observations += stats->new_observations[i];
    stats->observations[i] += stats->new_observations[i];
    stats->new_observations[i] = 0;
  }

  stats->num_new_observations = 0;
  return false;
}

static BL_INLINE bool should_end_block(block_split_stats* stats, const uint8_t* in_block_begin, const uint8_t* in_next, const uint8_t* in_end) noexcept {
  // Ready to check block split statistics?
  if (stats->num_new_observations < NUM_OBSERVATIONS_PER_BLOCK_CHECK ||
      in_next - in_block_begin < MIN_BLOCK_LENGTH ||
      in_end - in_next < MIN_BLOCK_LENGTH)
    return false;

  return do_end_block_check(stats, uint32_t(in_next - in_block_begin));
}

// Compression - Deflate - Greedy Implementation
// =============================================

// This is the "greedy" DEFLATE compressor. It always chooses the longest match.
static size_t deflate_compress_greedy(EncoderImpl* impl_, const uint8_t* BL_RESTRICT in, size_t in_nbytes, uint8_t* BL_RESTRICT out, size_t out_nbytes_avail) noexcept {
  GreedyEncoderImpl* impl = static_cast<GreedyEncoderImpl*>(impl_);
  deflate_output_bitstream os;

  const uint8_t* in_next = in;
  const uint8_t* in_end = in_next + in_nbytes;
  const uint8_t* in_cur_base = in_next;

  uint32_t max_len = kMaxMatchLen;
  uint32_t nice_len = blMin(impl->nice_match_length, max_len);
  uint32_t next_hashes[2] = {0, 0};

  deflate_init_output(&os, out, out_nbytes_avail);
  hc_matchfinder_init(&impl->hc_mf);

  do {
    // Starting a new DEFLATE block.
    const uint8_t* in_block_begin = in_next;
    const uint8_t* in_max_block_end = in_next + blMin<size_t>((size_t)(in_end - in_next), SOFT_MAX_BLOCK_LENGTH);

    uint32_t litrunlen = 0;
    deflate_sequence *next_seq = impl->sequences;

    init_block_split_stats(&impl->split_stats);
    deflate_reset_symbol_frequencies(impl);

    do {
      uint32_t length;
      uint32_t offset;

      // Decrease the maximum and nice match lengths if we're approaching the end of the input buffer.
      if (BL_UNLIKELY(max_len > size_t(in_end - in_next))) {
        max_len = uint32_t(in_end - in_next);
        nice_len = blMin(nice_len, max_len);
      }

      length = hc_matchfinder_longest_match(&impl->hc_mf, &in_cur_base, in_next, kMinMatchLen - 1, max_len, nice_len, impl->max_search_depth, next_hashes, &offset);
      if (length >= kMinMatchLen) {
        // Match found.
        deflate_choose_match(impl, length, offset, &litrunlen, &next_seq);
        observe_match(&impl->split_stats, length);
        in_next = hc_matchfinder_skip_positions(&impl->hc_mf, &in_cur_base, in_next + 1, in_end, length - 1, next_hashes);
      }
      else {
        // No match found.
        deflate_choose_literal(impl, *in_next, &litrunlen);
        observe_literal(&impl->split_stats, *in_next);
        in_next++;
      }

      // Check if it's time to output another block.
    } while (in_next < in_max_block_end && !should_end_block(&impl->split_stats, in_block_begin, in_next, in_end));

    deflate_finish_sequence(next_seq, litrunlen);
    deflate_flush_block(impl, &os, in_block_begin, uint32_t(in_next - in_block_begin), in_next == in_end, false);
  } while (in_next != in_end);

  return deflate_flush_output(&os);
}

// Compression - Deflate - Lazy Implementation
// ===========================================

// This is the "lazy" DEFLATE compressor. Before choosing a match, it checks to see if there's a
// longer match at the next position.  If yes, it outputs a literal and continues to the next
// position. If no, it outputs the match.
static size_t deflate_compress_lazy(EncoderImpl* impl_, const uint8_t* BL_RESTRICT in, size_t in_nbytes, uint8_t* BL_RESTRICT out, size_t out_nbytes_avail) noexcept {
  GreedyEncoderImpl* impl = static_cast<GreedyEncoderImpl*>(impl_);
  deflate_output_bitstream os;

  const uint8_t *in_next = in;
  const uint8_t *in_end = in_next + in_nbytes;
  const uint8_t *in_cur_base = in_next;
  uint32_t max_len = kMaxMatchLen;
  uint32_t nice_len = blMin(impl->nice_match_length, max_len);
  uint32_t next_hashes[2] = {0, 0};

  deflate_init_output(&os, out, out_nbytes_avail);
  hc_matchfinder_init(&impl->hc_mf);

  do {
    // Starting a new DEFLATE block.
    const uint8_t * const in_block_begin = in_next;
    const uint8_t * const in_max_block_end = in_next + blMin<size_t>((size_t)(in_end - in_next), SOFT_MAX_BLOCK_LENGTH);
    uint32_t litrunlen = 0;
    deflate_sequence *next_seq = impl->sequences;

    init_block_split_stats(&impl->split_stats);
    deflate_reset_symbol_frequencies(impl);

    do {
      uint32_t cur_offset;
      uint32_t next_len;
      uint32_t next_offset;

      if (BL_UNLIKELY(size_t(in_end - in_next) < kMaxMatchLen)) {
        max_len = uint32_t(in_end - in_next);
        nice_len = blMin(nice_len, max_len);
      }

      // Find the longest match at the current position.
      uint32_t cur_len = hc_matchfinder_longest_match(&impl->hc_mf, &in_cur_base, in_next, kMinMatchLen - 1, max_len, nice_len, impl->max_search_depth, next_hashes, &cur_offset);
      in_next += 1;

      if (cur_len < kMinMatchLen) {
        // No match found. Choose a literal.
        deflate_choose_literal(impl, *(in_next - 1), &litrunlen);
        observe_literal(&impl->split_stats, *(in_next - 1));
        continue;
      }

have_cur_match:
      // We have a match at the current position.
      observe_match(&impl->split_stats, cur_len);

      // If the current match is very long, choose it immediately.
      if (cur_len >= nice_len) {
        deflate_choose_match(impl, cur_len, cur_offset, &litrunlen, &next_seq);
        in_next = hc_matchfinder_skip_positions(&impl->hc_mf, &in_cur_base, in_next, in_end, cur_len - 1, next_hashes);
        continue;
      }

      // Try to find a match at the next position.
      //
      // NOTE: since we already have a match at the *current* position, we use only half the
      // `max_search_depth` when checking the *next* position. This is a useful trade-off
      // because it's more worthwhile to use a greater search depth on the initial match.
      //
      // NOTE: it's possible to structure the code such that there's only one call to
      // `longest_match()`, which handles both the "find the initial match" and "try to find
      // a longer match" cases. However, it is faster to have two call sites, with
      // `longest_match()` inlined at each.
      if (BL_UNLIKELY(size_t(in_end - in_next) < kMaxMatchLen)) {
        max_len = uint32_t(in_end - in_next);
        nice_len = blMin(nice_len, max_len);
      }

      next_len = hc_matchfinder_longest_match(&impl->hc_mf, &in_cur_base, in_next, cur_len, max_len, nice_len, impl->max_search_depth / 2, next_hashes, &next_offset);
      in_next += 1;

      if (next_len > cur_len) {
        // Found a longer match at the next position. Output a literal. Then the next match becomes
        // the current match.
        deflate_choose_literal(impl, *(in_next - 2), &litrunlen);
        cur_len = next_len;
        cur_offset = next_offset;
        goto have_cur_match;
      }

      // No longer match at the next position. Output the current match.
      deflate_choose_match(impl, cur_len, cur_offset, &litrunlen, &next_seq);
      in_next = hc_matchfinder_skip_positions(&impl->hc_mf, &in_cur_base, in_next, in_end, cur_len - 2, next_hashes);

      // Check if it's time to output another block.
    } while (in_next < in_max_block_end && !should_end_block(&impl->split_stats, in_block_begin, in_next, in_end));

    deflate_finish_sequence(next_seq, litrunlen);
    deflate_flush_block(impl, &os, in_block_begin, uint32_t(in_next - in_block_begin), in_next == in_end, false);
  } while (in_next != in_end);

  return deflate_flush_output(&os);
}

// BLCompression - Deflate - Near-Optimal Implementation
// =====================================================

// Follow the minimum-cost path in the graph of possible match/literal choices for the current block
// and compute the frequencies of the Huffman symbols that would be needed to output those matches
// and literals.
static void deflate_tally_item_list(NearOptimalEncoderImpl* impl, uint32_t block_length) noexcept {
  deflate_optimum_node *cur_node = &impl->optimum_nodes[0];
  deflate_optimum_node *end_node = &impl->optimum_nodes[block_length];

  do {
    uint32_t length = cur_node->item & OPTIMUM_LEN_MASK;
    uint32_t offset = cur_node->item >> OPTIMUM_OFFSET_SHIFT;

    if (length == 1) {
      // Literal.
      impl->freqs.litlen[offset]++;
    }
    else {
      // Match.
      impl->freqs.litlen[257 + deflate_length_slot[length]]++;
      impl->freqs.offset[deflate_get_offset_slot(impl, offset)]++;
    }
    cur_node += length;
  } while (cur_node != end_node);
}

// A scaling factor that makes it possible to consider fractional bit costs. A token requiring 'n'
// bits to represent has cost n << kCostShift.
//
// NOTE: This is only useful as a statistical trick for when the true costs are unknown. In reality,
// each token in DEFLATE requires a whole number of bits t output.
static constexpr uint32_t kCostShift = 3;

static constexpr uint32_t kLiteralCost = 66;    // 8.25 bits/symbol.
static constexpr uint32_t kLengthSlotCost = 60; // 7.5 bits/symbol.
static constexpr uint32_t kOffsetSlotCost = 39; // 4.875 bits/symbol.

static BL_INLINE uint32_t default_literal_cost(uint32_t literal) noexcept {
  blUnused(literal);
  return kLiteralCost;
}

static BL_INLINE uint32_t default_length_slot_cost(uint32_t length_slot) noexcept {
  return kLengthSlotCost + ((uint32_t)deflate_extra_length_bits[length_slot] << kCostShift);
}

static BL_INLINE uint32_t default_offset_slot_cost(uint32_t offset_slot) noexcept {
  return kOffsetSlotCost + ((uint32_t)deflate_extra_offset_bits[offset_slot] << kCostShift);
}

// Set default symbol costs for the first block's first optimization pass.
//
// It works well to assume that each symbol is equally probable.  This results
// in each symbol being assigned a cost of (-log2(1.0/num_syms) * (1 << kCostShift))
// where 'num_syms' is the number of symbols in the corresponding alphabet. However,
// we intentionally bias the parse towards matches rather than literals by using a
// slightly lower default cost for length symbols than for literals. This often
// improves the compression ratio slightly.
static void deflate_set_default_costs(NearOptimalEncoderImpl* impl) noexcept {
  uint32_t i;

  // Literals.
  for (i = 0; i < kNumLiterals; i++)
    impl->costs.literal[i] = default_literal_cost(i);

  // Lengths.
  for (i = kMinMatchLen; i <= kMaxMatchLen; i++)
    impl->costs.length[i] = default_length_slot_cost(deflate_length_slot[i]);

  // Offset slots.
  for (i = 0; i < BL_ARRAY_SIZE(deflate_offset_slot_base); i++)
    impl->costs.offset_slot[i] = default_offset_slot_cost(i);
}

static BL_INLINE void deflate_adjust_cost(uint32_t *cost_p, uint32_t default_cost) noexcept {
  *cost_p += ((int32_t)default_cost - (int32_t)*cost_p) >> 1;
}

// Adjust the costs when beginning a new block.
//
// Since the current costs have been optimized for the data, it's undesirable to throw them away
// and start over with the default costs. At the same time, we don't want to bias the parse by
// assuming that the next block will be similar to the current block. As a compromise, make the
// costs closer to the defaults, but don't simply set them to the defaults.
static void deflate_adjust_costs(NearOptimalEncoderImpl* impl) noexcept {
  uint32_t i;

  // Literals.
  for (i = 0; i < kNumLiterals; i++)
    deflate_adjust_cost(&impl->costs.literal[i], default_literal_cost(i));

  // Lengths.
  for (i = kMinMatchLen; i <= kMaxMatchLen; i++)
    deflate_adjust_cost(&impl->costs.length[i], default_length_slot_cost( deflate_length_slot[i]));

  // Offset slots.
  for (i = 0; i < BL_ARRAY_SIZE(deflate_offset_slot_base); i++)
    deflate_adjust_cost(&impl->costs.offset_slot[i], default_offset_slot_cost(i));
}

// Find the minimum-cost path through the graph of possible match/literal choices for this block.
//
// We find the minimum cost path from `impl->optimum_nodes[0]`, which represents the node at the
// beginning of the block, to `impl->optimum_nodes[block_length]`, which represents the node at
// the end of the block. Edge costs are evaluated using the cost model `impl->costs`.
//
// The algorithm works backwards, starting at the end node and proceeding backwards one node at a
// time.  At each node, the minimum cost to reach the end node is computed and the match/literal
// choice that begins that path is saved.
static void deflate_find_min_cost_path(NearOptimalEncoderImpl* impl, const uint32_t block_length, const lz_match* cache_ptr) noexcept {
  deflate_optimum_node *end_node = &impl->optimum_nodes[block_length];
  deflate_optimum_node *cur_node = end_node;

  cur_node->cost_to_end = 0;
  do {
    cur_node--;
    cache_ptr--;

    uint32_t num_matches = cache_ptr->length;
    uint32_t literal = cache_ptr->offset;

    // It's always possible to choose a literal.
    uint32_t best_cost_to_end = impl->costs.literal[literal] + (cur_node + 1)->cost_to_end;
    cur_node->item = ((uint32_t)literal << OPTIMUM_OFFSET_SHIFT) | 1;

    // Also consider matches if there are any.
    if (num_matches) {
      // Consider each length from the minimum (kMinMatchLen) to the length of the longest match
      // found at this position. For each length, we consider only the smallest offset for which
      // that length is available. Although this is not guaranteed to be optimal due to the
      // possibility of a larger offset costing less than a smaller offset to code, this is a very
      // useful heuristic.
      const lz_match* match = cache_ptr - num_matches;
      uint32_t len = kMinMatchLen;
      do {
        uint32_t offset = match->offset;
        uint32_t offset_slot = deflate_get_offset_slot(impl, offset);
        uint32_t offset_cost = impl->costs.offset_slot[offset_slot];
        do {
          uint32_t cost_to_end = offset_cost + impl->costs.length[len] + (cur_node + len)->cost_to_end;
          if (cost_to_end < best_cost_to_end) {
            best_cost_to_end = cost_to_end;
            cur_node->item = ((uint32_t)offset << OPTIMUM_OFFSET_SHIFT) | len;
          }
        } while (++len <= match->length);
      } while (++match != cache_ptr);
      cache_ptr -= num_matches;
    }

    cur_node->cost_to_end = best_cost_to_end;
  } while (cur_node != &impl->optimum_nodes[0]);
}

// Set the current cost model from the codeword lengths specified in `lens`.
static void deflate_set_costs_from_codes(NearOptimalEncoderImpl* impl, const deflate_lens* lens) noexcept {
  uint32_t i;

  // Literals.
  for (i = 0; i < kNumLiterals; i++) {
    uint32_t bits = (lens->litlen[i] ? lens->litlen[i] : LITERAL_NOSTAT_BITS);
    impl->costs.literal[i] = bits << kCostShift;
  }

  // Lengths.
  for (i = kMinMatchLen; i <= kMaxMatchLen; i++) {
    uint32_t length_slot = deflate_length_slot[i];
    uint32_t litlen_sym = 257 + length_slot;
    uint32_t bits = (lens->litlen[litlen_sym] ? lens->litlen[litlen_sym] : LENGTH_NOSTAT_BITS);
    bits += deflate_extra_length_bits[length_slot];
    impl->costs.length[i] = bits << kCostShift;
  }

  // Offset slots.
  for (i = 0; i < BL_ARRAY_SIZE(deflate_offset_slot_base); i++) {
    uint32_t bits = (lens->offset[i] ? lens->offset[i] : OFFSET_NOSTAT_BITS);
    bits += deflate_extra_offset_bits[i];
    impl->costs.offset_slot[i] = bits << kCostShift;
  }
}

// Choose the literal/match sequence to use for the current block. The basic algorithm finds a
// minimum-cost path through the block's graph of literal/match choices, given a cost model.
// However, the cost of each symbol is unknown until the Huffman codes have been built, but at
// the same time the Huffman codes depend on the frequencies of chosen symbols. Consequently,
// multiple passes must be used to try to approximate an optimal solution.  The first pass uses
// default costs, mixed with the costs from the previous block if any. Later passes use the Huffman
// codeword lengths from the previous pass as the costs.
static void deflate_optimize_block(NearOptimalEncoderImpl* impl, uint32_t block_length, const lz_match* cache_ptr, bool is_first_block)
{
  // Force the block to really end at the desired length, even if some matches extend beyond it.
  uint32_t num_passes_remaining = impl->num_optim_passes;
  for (uint32_t i = block_length; i <= blMin(block_length - 1 + kMaxMatchLen, BL_ARRAY_SIZE(impl->optimum_nodes) - 1); i++)
    impl->optimum_nodes[i].cost_to_end = 0x80000000;

  // Set the initial costs.
  if (is_first_block)
    deflate_set_default_costs(impl);
  else
    deflate_adjust_costs(impl);

  for (;;) {
    // Find the minimum cost path for this pass.
    deflate_find_min_cost_path(impl, block_length, cache_ptr);

    // Compute frequencies of the chosen symbols.
    deflate_reset_symbol_frequencies(impl);
    deflate_tally_item_list(impl, block_length);

    if (--num_passes_remaining == 0)
      break;

    // At least one optimization pass remains; update the costs.
    deflate_make_huffman_codes(&impl->freqs, &impl->codes);
    deflate_set_costs_from_codes(impl, &impl->codes.lens);
  }
}

// This is the "near-optimal" DEFLATE compressor. It computes the optimal representation of each
// DEFLATE block using a minimum-cost path search over the graph of possible match/literal choices
// for that block, assuming a certain cost for each Huffman symbol. For several reasons, the end
// result is not guaranteed to be optimal:
//
//   - Nonoptimal choice of blocks
//   - Heuristic limitations on which matches are actually considered
//   - Symbol costs are unknown until the symbols have already been chosen
//     (so iterative optimization must be used)
static size_t deflate_compress_near_optimal(EncoderImpl* impl_, const uint8_t* BL_RESTRICT in, size_t in_nbytes, uint8_t* BL_RESTRICT out, size_t out_nbytes_avail) noexcept {
  NearOptimalEncoderImpl* impl = static_cast<NearOptimalEncoderImpl*>(impl_);
  deflate_output_bitstream os;

  const uint8_t *in_next = in;
  const uint8_t *in_end = in_next + in_nbytes;
  const uint8_t *in_cur_base = in_next;
  const uint8_t *in_next_slide = in_next + blMin<size_t>((size_t)(in_end - in_next), MATCHFINDER_WINDOW_SIZE);

  uint32_t max_len = kMaxMatchLen;
  uint32_t nice_len = blMin(impl->nice_match_length, max_len);
  uint32_t next_hashes[2] = {0, 0};

  deflate_init_output(&os, out, out_nbytes_avail);
  bt_matchfinder_init(&impl->bt_mf);

  do {
    // Starting a new DEFLATE block.
    lz_match* cache_ptr = impl->match_cache;
    const uint8_t* in_block_begin = in_next;
    const uint8_t* in_max_block_end = in_next + blMin<size_t>((size_t)(in_end - in_next), SOFT_MAX_BLOCK_LENGTH);
    const uint8_t* next_observation = in_next;

    init_block_split_stats(&impl->split_stats);

    // Find matches until we decide to end the block. We end the block if any of the following is true:
    //   1. Maximum block length has been reached.
    //   2. Match catch may overflow.
    //   3. Block split heuristic says to split now.
    do {
      // Slide the window forward if needed.
      if (in_next == in_next_slide) {
        bt_matchfinder_slide_window(&impl->bt_mf);
        in_cur_base = in_next;
        in_next_slide = in_next + blMin<size_t>((size_t)(in_end - in_next), MATCHFINDER_WINDOW_SIZE);
      }

      // Decrease the maximum and nice match lengths if we're approaching the end of the input buffer.
      if (BL_UNLIKELY(max_len > size_t(in_end - in_next))) {
        max_len = uint32_t(in_end - in_next);
        nice_len = blMin(nice_len, max_len);
      }

      // Find matches with the current position using the  binary tree matchfinder and save them in
      // `match_cache`.
      //
      // NOTE: the binary tree matchfinder is more suited for optimal parsing than the hash chain
      // matchfinder. The reasons for this include:
      //
      //   - The binary tree matchfinder can find more matches in the same number of steps.
      //   - One of the major advantages of hash chains is that skipping positions (not
      //     searching for matches at them) is faster; however, with optimal parsing we search
      //     for matches at almost all positions, so this advantage of hash chains is negated.
      lz_match* matches = cache_ptr;
      uint32_t best_len = 0;

      if (BL_LIKELY(max_len >= BT_MATCHFINDER_REQUIRED_NBYTES))
        cache_ptr = bt_matchfinder_get_matches(&impl->bt_mf, in_cur_base, in_next - in_cur_base, max_len, nice_len, impl->max_search_depth, next_hashes, &best_len, matches);

      if (in_next >= next_observation) {
        if (best_len >= 4) {
          observe_match(&impl->split_stats, best_len);
          next_observation = in_next + best_len;
        }
        else {
          observe_literal(&impl->split_stats, *in_next);
          next_observation = in_next + 1;
        }
      }

      cache_ptr->length = uint16_t(cache_ptr - matches);
      cache_ptr->offset = *in_next;
      in_next++;
      cache_ptr++;

      // If there was a very long match found, don't cache any matches for the bytes covered
      // by that match. This avoids degenerate behavior when compressing highly redundant data,
      // where the number of matches can be very large.
      //
      // This heuristic doesn't actually hurt the compression ratio very much.  If there's a long
      // match, then the data must be highly compressible, so it doesn't matter much what we do.
      if (best_len >= kMinMatchLen && best_len >= nice_len) {
        --best_len;
        do {
          if (in_next == in_next_slide) {
            bt_matchfinder_slide_window(&impl->bt_mf);
            in_cur_base = in_next;
            in_next_slide = in_next + blMin<size_t>((size_t)(in_end - in_next), MATCHFINDER_WINDOW_SIZE);
          }
          if (BL_UNLIKELY(max_len > size_t(in_end - in_next))) {
            max_len = uint32_t(in_end - in_next);
            nice_len = blMin(nice_len, max_len);
          }
          if (max_len >= BT_MATCHFINDER_REQUIRED_NBYTES) {
            bt_matchfinder_skip_position(&impl->bt_mf, in_cur_base, in_next - in_cur_base, nice_len, impl->max_search_depth, next_hashes);
          }
          cache_ptr->length = 0;
          cache_ptr->offset = *in_next;
          in_next++;
          cache_ptr++;
        } while (--best_len);
      }
    } while (in_next < in_max_block_end && cache_ptr < &impl->match_cache[CACHE_LENGTH] && !should_end_block(&impl->split_stats, in_block_begin, in_next, in_end));

    // All the matches for this block have been cached. Now choose the sequence of items to output
    // and flush the block.
    deflate_optimize_block(impl, uint32_t(in_next - in_block_begin), cache_ptr, in_block_begin == in);
    deflate_flush_block(impl, &os, in_block_begin, uint32_t(in_next - in_block_begin), in_next == in_end, true);
  } while (in_next != in_end);

  return deflate_flush_output(&os);
}

// Initialize impl->offset_slot_fast.
static void deflate_init_offset_slot_fast(EncoderImpl* impl) noexcept {
  uint32_t offset_slot;
  uint32_t offset;
  uint32_t offset_end;

  for (offset_slot = 0; offset_slot < BL_ARRAY_SIZE(deflate_offset_slot_base); offset_slot++) {
    offset = deflate_offset_slot_base[offset_slot];
#if USE_FULL_OFFSET_SLOT_FAST
    offset_end = offset + (1 << deflate_extra_offset_bits[offset_slot]);
    do {
      impl->offset_slot_fast[offset] = uint8_t(offset_slot);
    } while (++offset != offset_end);
#else
    if (offset <= 256) {
      offset_end = offset + (1 << deflate_extra_offset_bits[offset_slot]);
      do {
        impl->offset_slot_fast[offset - 1] = offset_slot;
      } while (++offset != offset_end);
    }
    else {
      offset_end = offset + (1 << deflate_extra_offset_bits[offset_slot]);
      do {
        impl->offset_slot_fast[256 + ((offset - 1) >> 7)] = offset_slot;
      } while ((offset += (1 << 7)) != offset_end);
    }
#endif
  }
}

static void initGreedy(EncoderImpl* impl, uint32_t maxSearchDepth, uint32_t niceMatchLength) noexcept {
  impl->compressFunc = deflate_compress_greedy;
  impl->max_search_depth = maxSearchDepth;
  impl->nice_match_length = niceMatchLength;
  impl->num_optim_passes = 0;
}

static void initLazy(EncoderImpl* impl, uint32_t maxSearchDepth, uint32_t niceMatchLength) noexcept {
  impl->compressFunc = deflate_compress_lazy;
  impl->max_search_depth = maxSearchDepth;
  impl->nice_match_length = niceMatchLength;
  impl->num_optim_passes = 0;
}

static void initNearOptimal(EncoderImpl* impl, uint32_t maxSearchDepth, uint32_t niceMatchLength, uint32_t numOptimPasses) noexcept {
  impl->compressFunc = deflate_compress_near_optimal;
  impl->max_search_depth = maxSearchDepth;
  impl->nice_match_length = niceMatchLength;
  impl->num_optim_passes = numOptimPasses;
}

BLResult Encoder::init(uint32_t format, uint32_t compressionLevel) noexcept {
  if (!(compressionLevel >= 1 && compressionLevel <= 12))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  size_t implAlignment = 64;
  size_t implSize = (compressionLevel < 8) ? sizeof(GreedyEncoderImpl) : sizeof(NearOptimalEncoderImpl);

  void* allocatedPtr = malloc(implSize + implAlignment);
  if (BL_UNLIKELY(!allocatedPtr))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  EncoderImpl* newImpl = static_cast<EncoderImpl*>(BLIntOps::alignUp(allocatedPtr, implAlignment));
  newImpl->allocated_ptr = allocatedPtr;

  switch (compressionLevel) {
    case 1: initGreedy(newImpl, 2, 8); break;
    case 2: initGreedy(newImpl, 6, 10); break;
    case 3: initGreedy(newImpl, 12, 14); break;
    case 4: initGreedy(newImpl, 24, 24); break;
    case 5: initLazy(newImpl, 20, 30); break;
    case 6: initLazy(newImpl, 40, 65); break;
    case 7: initLazy(newImpl, 100, 130); break;
    case 8: initNearOptimal(newImpl, 12, 20, 1); break;
    case 9: initNearOptimal(newImpl, 16, 26, 2); break;
    case 10: initNearOptimal(newImpl, 30, 50, 2); break;
    case 11: initNearOptimal(newImpl, 60, 80, 3); break;
    case 12: initNearOptimal(newImpl, 100, 133, 4); break;
  }

  newImpl->format = uint8_t(format);
  newImpl->compression_level = uint8_t(compressionLevel);
  deflate_init_offset_slot_fast(newImpl);
  deflate_init_static_codes(newImpl);

  reset();

  impl = newImpl;
  return BL_SUCCESS;
}

void Encoder::reset() noexcept {
  if (impl) {
    free(impl->allocated_ptr);
    impl = nullptr;
  }
}

size_t Encoder::minimumOutputBufferSize(size_t inputSize) const noexcept {
  // The worst case is all uncompressed blocks where one block has length <= MIN_BLOCK_LENGTH and
  // the others have length MIN_BLOCK_LENGTH. Each uncompressed block has 5 bytes of overhead: 1
  // for BFINAL, BTYPE, and alignment to a byte boundary; 2 for LEN; and 2 for NLEN.
  size_t maxBlockCount = blMax<size_t>(DIV_ROUND_UP(inputSize, MIN_BLOCK_LENGTH), 1);
  return size_t(MIN_OUTPUT_SIZE) + minOutputSizeExtras[impl->format] + (maxBlockCount * 5u) + inputSize + 1;
}

static size_t compress_deflate(EncoderImpl* impl, void* output, size_t outputSize, const void* input, size_t inputSize) noexcept {
  // For extremely small inputs just use a single uncompressed block.
  if (BL_UNLIKELY(inputSize < 16)) {
    deflate_output_bitstream os;
    deflate_init_output(&os, output, outputSize);
    deflate_write_uncompressed_block(&os, static_cast<const uint8_t*>(input), uint32_t(inputSize), true);
    return deflate_flush_output(&os);
  }

  return impl->compressFunc(impl, static_cast<const uint8_t*>(input), inputSize, static_cast<uint8_t*>(output), outputSize);
}

#define ZLIB_CM_DEFLATE    8

#define ZLIB_CINFO_32K_WINDOW  7

#define ZLIB_FASTEST_COMPRESSION  0
#define ZLIB_FAST_COMPRESSION    1
#define ZLIB_DEFAULT_COMPRESSION  2
#define ZLIB_SLOWEST_COMPRESSION  3

size_t Encoder::compress(void* output, size_t outputSize, const void* input, size_t inputSize) noexcept {
  if (BL_UNLIKELY(outputSize < MIN_OUTPUT_SIZE + minOutputSizeExtras[impl->format]))
    return 0;

  switch (impl->format) {
    case kFormatRaw: {
      return compress_deflate(impl, output, outputSize, input, inputSize);
    }

    case kFormatZlib: {
      size_t compressedSize = compress_deflate(impl, static_cast<uint8_t*>(output) + 2, outputSize - 6, input, inputSize);
      if (compressedSize == 0)
        return 0;

      // 2 byte header: CMF and FLG
      uint32_t compression_level = impl->compression_level;
      uint32_t hdr = (ZLIB_CM_DEFLATE << 8) | (ZLIB_CINFO_32K_WINDOW << 12);
      uint32_t compression_level_hint;

      if (compression_level < 2)
        compression_level_hint = ZLIB_FASTEST_COMPRESSION;
      else if (compression_level < 6)
        compression_level_hint = ZLIB_FAST_COMPRESSION;
      else if (compression_level < 8)
        compression_level_hint = ZLIB_DEFAULT_COMPRESSION;
      else
        compression_level_hint = ZLIB_SLOWEST_COMPRESSION;

      hdr |= compression_level_hint << 6;
      hdr |= 31 - (hdr % 31);

      BLMemOps::writeU16uBE(output, hdr);

      // ADLER32.
      uint32_t checksum = adler32(static_cast<const uint8_t*>(input), inputSize);
      BLMemOps::writeU32uBE(static_cast<uint8_t*>(output) + 2 + compressedSize, checksum);

      return compressedSize + 6;
    }

    default:
      return 0;
  }

// TODO: Do we need a compress/decompress like API?
/*
LIBDEFLATEAPI size_t
libdeflate_zlib_compress(struct libdeflate_compressor *c,
       const void *in, size_t in_size,
       void *out, size_t out_nbytes_avail)
{
  u8 *out_next = out;
  u16 hdr;
  unsigned compression_level;
  unsigned level_hint;
  size_t deflate_size;

  if (out_nbytes_avail <= ZLIB_MIN_OVERHEAD)
    return 0;

  // 2 byte header: CMF and FLG
  hdr = (ZLIB_CM_DEFLATE << 8) | (ZLIB_CINFO_32K_WINDOW << 12);
  compression_level = deflate_get_compression_level(c);
  if (compression_level < 2)
    level_hint = ZLIB_FASTEST_COMPRESSION;
  else if (compression_level < 6)
    level_hint = ZLIB_FAST_COMPRESSION;
  else if (compression_level < 8)
    level_hint = ZLIB_DEFAULT_COMPRESSION;
  else
    level_hint = ZLIB_SLOWEST_COMPRESSION;
  hdr |= level_hint << 6;
  hdr |= 31 - (hdr % 31);

  put_unaligned_be16(hdr, out_next);
  out_next += 2;

  // Compressed data.
  deflate_size = libdeflate_deflate_compress(c, in, in_size, out_next,
          out_nbytes_avail - ZLIB_MIN_OVERHEAD);
  if (deflate_size == 0)
    return 0;
  out_next += deflate_size;

  // ADLER32.
  put_unaligned_be32(adler32(in, in_size), out_next);
  out_next += 4;

  return out_next - (u8 *)out;
}
*/

}

} // {Deflate}
} // {BLCompression}
