// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SUPPORT_FIXEDBITARRAY_P_H_INCLUDED
#define BLEND2D_SUPPORT_FIXEDBITARRAY_P_H_INCLUDED

#include "../support/intops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! A fixed bit-array that cannot grow.
template<typename T, size_t N>
class BLFixedBitArray {
public:
  enum : size_t {
    kSizeOfTInBits = BLIntOps::bitSizeOf<T>(),
    kFixedArraySize = (N + kSizeOfTInBits - 1) / kSizeOfTInBits
  };

  T data[kFixedArraySize];

  BL_INLINE bool bitAt(size_t index) const noexcept {
    BL_ASSERT(index < N);
    return bool((data[index / kSizeOfTInBits] >> (index % kSizeOfTInBits)) & 0x1);
  }

  BL_INLINE void setAt(size_t index) noexcept {
    BL_ASSERT(index < N);
    data[index / kSizeOfTInBits] |= T(1) << (index % kSizeOfTInBits);
  }

  BL_INLINE void setAt(size_t index, bool value) noexcept {
    BL_ASSERT(index < N);

    T clrMask = T(1    ) << (index % kSizeOfTInBits);
    T setMask = T(value) << (index % kSizeOfTInBits);
    data[index / kSizeOfTInBits] = (data[index / kSizeOfTInBits] & ~clrMask) | setMask;
  }

  BL_INLINE void clearAt(size_t index) noexcept {
    BL_ASSERT(index < N);
    data[index / kSizeOfTInBits] &= ~(T(1) << (index % kSizeOfTInBits));
  }

  BL_INLINE void clearAll() noexcept {
    for (size_t i = 0; i < kFixedArraySize; i++)
      data[i] = 0;
  }

  BL_INLINE void setAll() noexcept {
    for (size_t i = 0; i < kFixedArraySize; i++)
      data[i] = BLIntOps::allOnes<T>();
  }
};

//! \}
//! \endcond

#endif // BLEND2D_SUPPORT_FIXEDBITARRAY_P_H_INCLUDED
