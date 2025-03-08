// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SUPPORT_PTROPS_P_H_INCLUDED
#define BLEND2D_SUPPORT_PTROPS_P_H_INCLUDED

#include "../api-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {
namespace PtrOps {
namespace {

//! \name Pointer Arithmetic
//! \{

template<typename T, typename Offset>
BL_NODISCARD
static BL_INLINE_NODEBUG T* offset(T* ptr, Offset offset) noexcept { return (T*)((uintptr_t)(ptr) + (uintptr_t)(intptr_t)offset); }

template<typename T, typename P, typename Offset>
BL_NODISCARD
static BL_INLINE_NODEBUG T* offset(P* ptr, Offset offset) noexcept { return (T*)((uintptr_t)(ptr) + (uintptr_t)(intptr_t)offset); }

template<typename T, typename Offset>
BL_NODISCARD
static BL_INLINE_NODEBUG T* deoffset(T* ptr, Offset offset) noexcept { return (T*)((uintptr_t)(ptr) - (uintptr_t)(intptr_t)offset); }

template<typename T, typename P, typename Offset>
BL_NODISCARD
static BL_INLINE_NODEBUG T* deoffset(P* ptr, Offset offset) noexcept { return (T*)((uintptr_t)(ptr) - (uintptr_t)(intptr_t)offset); }

template<typename T, typename U>
static BL_INLINE_NODEBUG bool bothAligned(const T* ptr1, const U* ptr2, size_t alignment) noexcept {
  return (((uintptr_t)(ptr1) | (uintptr_t)(ptr2)) % uintptr_t(alignment)) == 0;
}

template<typename T, typename U>
static BL_INLINE_NODEBUG bool haveEqualAlignment(const T* ptr1, const U* ptr2, size_t alignment) noexcept {
  return (((uintptr_t)(ptr1) ^ (uintptr_t)(ptr2)) % uintptr_t(alignment)) == 0;
}

static BL_INLINE_NODEBUG size_t byteOffset(const void* base, const void* ptr) noexcept {
  // The `byteOffset` function expects `ptr` to always be greater than base - the result must be
  // zero/positive as it's represented by an unsigned type.
  BL_ASSERT(static_cast<const uint8_t*>(ptr) >= static_cast<const uint8_t*>(base));

  return (size_t)(static_cast<const uint8_t*>(ptr) - static_cast<const uint8_t*>(base));
}

static BL_INLINE_NODEBUG size_t bytesUntil(const void* ptr, const void* end) noexcept {
  // The `bytesUntil` function requires `end` to always be greater than or equal to `ptr` as it
  // describes the end of a buffer where data is stored or read from.
  BL_ASSERT(static_cast<const uint8_t*>(ptr) <= static_cast<const uint8_t*>(end));

  return (size_t)(static_cast<const uint8_t*>(end) - static_cast<const uint8_t*>(ptr));
}

//! \}

} // {anonymous}
} // {PtrOps}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_SUPPORT_PTROPS_P_H_INCLUDED
