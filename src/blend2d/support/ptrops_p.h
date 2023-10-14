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

//! \}

} // {anonymous}
} // {PtrOps}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_SUPPORT_PTROPS_P_H_INCLUDED
