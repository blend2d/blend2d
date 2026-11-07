// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_AXL_BIT_SET_OPS_H_INCLUDED
#define BLEND2D_AXL_BIT_SET_OPS_H_INCLUDED

#include <blend2d/axl/commons.h>

namespace bl::axl {
namespace {

//! \addtogroup blend2d_axl
//! \{

// BitCount <-> WordCount Conversion
// =================================

template<typename T>
[[nodiscard]]
BL_INLINE_CONSTEXPR size_t word_count_from_bit_count(size_t bit_count) noexcept {
  return (bit_count + bit_size_of<T> - 1) / bit_size_of<T>;
}

template<typename T>
[[nodiscard]]
BL_INLINE_CONSTEXPR size_t bit_count_from_word_count(size_t word_count) noexcept {
  return word_count * bit_size_of<T>;
}

// TODO: Missing iterators.

//! Iterates over each bit in a number which is set to 1.
//!
//! Example of use:
//!
//! ```
//! uint32_t bits_to_iterate = 0x110F;
//! BLBitWordIterator<uint32_t> it(bits_to_iterate);
//!
//! while (it.has_next()) {
//!   uint32_t bit_index = it.next();
//!   printf("Bit at %u is set\n", unsigned(bit_index));
//! }
//! ```
template<typename T>
class BitWordIterator {
public:
  BL_INLINE explicit BitWordIterator(T bit_word) noexcept
    : _bit_word(bit_word) {}

  BL_INLINE_NODEBUG void init(T bit_word) noexcept { _bit_word = bit_word; }
  BL_INLINE_NODEBUG bool has_next() const noexcept { return _bit_word != 0; }
  BL_INLINE_NODEBUG T bit_word() const noexcept { return _bit_word; }

  BL_INLINE uint32_t next() noexcept {
    BL_ASSERT(_bit_word != 0);
    uint32_t index = axl::ctz(_bit_word);
    _bit_word ^= T(1u) << index;
    return index;
  }

  T _bit_word;
};

//! \}

} // {anonymous}
} // {bl::axl}

#endif // BLEND2D_AXL_BIT_SET_OPS_H_INCLUDED
