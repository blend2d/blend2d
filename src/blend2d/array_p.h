// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_ARRAY_P_H_INCLUDED
#define BLEND2D_ARRAY_P_H_INCLUDED

#include "./api-internal_p.h"
#include "./array.h"
#include "./support_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLArray - Internal]
// ============================================================================

BL_HIDDEN BLResult blArrayImplDelete(BLArrayImpl* impl) noexcept;

static BL_INLINE BLResult blArrayImplRelease(BLArrayImpl* impl) noexcept {
  if (blImplDecRefAndTest(impl))
    return blArrayImplDelete(impl);
  return BL_SUCCESS;
}

// ============================================================================
// [BLArray - Utilities]
// ============================================================================

namespace {

BL_NODISCARD
constexpr size_t blContainerSizeOf(size_t baseSize, size_t itemSize, size_t n) noexcept {
  return baseSize + n * itemSize;
}

BL_NODISCARD
constexpr size_t blContainerCapacityOf(size_t baseSize, size_t itemSize, size_t implSize) noexcept {
  return (implSize - baseSize) / itemSize;
}

//! Calculates the maximum theoretical capacity of items a container can hold.
//! This is really a theoretical capacity that will never be reached in practice
//! is it would mean that all addressable memory will be used by the data and
//! mapped into a single continuous region, which is impossible.
BL_NODISCARD
constexpr size_t blContainerMaximumCapacity(size_t baseSize, size_t itemSize) noexcept {
  return blContainerCapacityOf(baseSize, itemSize, SIZE_MAX);
}

BL_NODISCARD
BL_INLINE size_t blContainerFittingCapacity(size_t baseSize, size_t itemSize, size_t n) noexcept {
  size_t nInBytes  = blAlignUp(baseSize + n * itemSize, 32);
  size_t capacity = (nInBytes - baseSize) / itemSize;

  BL_ASSERT(capacity >= n);
  return capacity;
}

BL_NODISCARD
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

#endif // BLEND2D_ARRAY_P_H_INCLUDED
