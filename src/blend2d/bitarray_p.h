// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_BITARRAY_P_H
#define BLEND2D_BITARRAY_P_H

#include "./bitarray.h"
#include "./bitops_p.h"
#include "./support_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLFixedBitArray]
// ============================================================================

template<typename T, size_t N>
class BLFixedBitArray {
public:
  enum : size_t {
    kSizeOfTInBits = blBitSizeOf<T>(),
    kFixedArraySize = (N + kSizeOfTInBits - 1) / kSizeOfTInBits
  };

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
      data[i] = blBitOnes<T>();
  }

  T data[kFixedArraySize];
};

//! \}
//! \endcond

#endif // BLEND2D_BITARRAY_P_H
