// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLARRAY_P_H
#define BLEND2D_BLARRAY_P_H

#include "./blapi-internal_p.h"
#include "./blarray.h"
#include "./blsupport_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLArray - Internal]
// ============================================================================

BL_HIDDEN BLResult blArrayImplDelete(BLArrayImpl* impl) noexcept;

static BL_INLINE BLResult blArrayImplRelease(BLArrayImpl* impl) noexcept {
  if (blAtomicFetchDecRef(&impl->refCount) != 1)
    return BL_SUCCESS;

  return blArrayImplDelete(impl);
}

// ============================================================================
// [BLArray - Utilities]
// ============================================================================

namespace {

constexpr size_t blContainerSizeOf(size_t baseSize, size_t itemSize, size_t n) noexcept {
  return baseSize + n * itemSize;
}

constexpr size_t blContainerCapacityOf(size_t baseSize, size_t itemSize, size_t implSize) noexcept {
  return (implSize - baseSize) / itemSize;
}

//! Calculates the maximum theoretical capacity of items a container can hold.
//! This is really a theoretical capacity that will never be reached in practice
//! is it would mean that all addressable memory will be used by the data and
//! mapped into a single continuous region, which is impossible.
constexpr size_t blContainerMaximumCapacity(size_t baseSize, size_t itemSize) noexcept {
  return blContainerCapacityOf(baseSize, itemSize, SIZE_MAX);
}

BL_INLINE size_t blContainerFittingCapacity(size_t baseSize, size_t itemSize, size_t n) noexcept {
  size_t nInBytes  = blAlignUp(baseSize + n * itemSize, 32);
  size_t capacity = (nInBytes - baseSize) / itemSize;

  BL_ASSERT(capacity >= n);
  return capacity;
}

BL_INLINE size_t blContainerGrowingCapacity(size_t baseSize, size_t itemSize, size_t n, size_t minSizeInBytes) noexcept {
  size_t nInBytes  = baseSize + n * itemSize;
  size_t optInBytes;

  if (nInBytes < BL_ALLOC_GROW_LIMIT) {
    optInBytes = blMax<size_t>(minSizeInBytes, blAlignUpPowerOf2(nInBytes + (nInBytes >> 1)));
  }
  else {
    optInBytes = blMax<size_t>(nInBytes, blAlignUp(nInBytes, BL_ALLOC_GROW_LIMIT));
  }

  size_t capacity = (optInBytes - baseSize) / itemSize;
  BL_ASSERT(capacity >= n);

  return capacity;
}

} // {anonymous}

//! \}
//! \endcond

#endif // BLEND2D_BLARRAY_P_H
