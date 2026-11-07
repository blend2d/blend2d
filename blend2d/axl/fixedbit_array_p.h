// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_AXL_FIXEDBIT_ARRAY_P_H_INCLUDED
#define BLEND2D_AXL_FIXEDBIT_ARRAY_P_H_INCLUDED

#include <blend2d/axl/commons.h>

//! \cond INTERNAL
//! \addtogroup blend2d_support
//! \{

namespace bl {

//! A fixed bit-array that cannot grow.
template<typename T, size_t N>
class FixedBitArray {
public:
  static constexpr size_t kSizeOfTInBits = axl::bit_size_of<T>;
  static constexpr size_t kFixedArraySize = (N + kSizeOfTInBits - 1) / kSizeOfTInBits;

  T data[kFixedArraySize];

  BL_INLINE constexpr size_t size_in_words() const noexcept { return kFixedArraySize; }

  BL_INLINE bool bit_at(size_t index) const noexcept {
    BL_ASSERT(index < N);
    return bool((data[index / kSizeOfTInBits] >> (index % kSizeOfTInBits)) & 0x1);
  }

  BL_INLINE void set_at(size_t index) noexcept {
    BL_ASSERT(index < N);
    data[index / kSizeOfTInBits] |= T(1) << (index % kSizeOfTInBits);
  }

  BL_INLINE void set_at(size_t index, T value) noexcept {
    BL_ASSERT(index < N);

    T clr_mask = T(1    ) << (index % kSizeOfTInBits);
    T set_mask = T(value) << (index % kSizeOfTInBits);
    data[index / kSizeOfTInBits] = (data[index / kSizeOfTInBits] & ~clr_mask) | set_mask;
  }

  BL_INLINE void fill_at(size_t index, T value) noexcept {
    BL_ASSERT(index < N);
    data[index / kSizeOfTInBits] |= T(value) << (index % kSizeOfTInBits);
  }

  BL_INLINE void clear_at(size_t index) noexcept {
    BL_ASSERT(index < N);
    data[index / kSizeOfTInBits] &= ~(T(1) << (index % kSizeOfTInBits));
  }

  BL_INLINE void clear_all() noexcept {
    for (size_t i = 0; i < kFixedArraySize; i++) {
      data[i] = 0;
    }
  }

  BL_INLINE void set_all() noexcept {
    for (size_t i = 0; i < kFixedArraySize; i++) {
      data[i] = axl::bit_ones<T>;
    }
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_AXL_FIXEDBIT_ARRAY_P_H_INCLUDED
