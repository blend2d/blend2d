// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SUPPORT_HASHOPS_P_H_INCLUDED
#define BLEND2D_SUPPORT_HASHOPS_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../unicode_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLHashOps {
namespace {

//! \name Hash Functions
//! \{

static BL_INLINE uint32_t hashRound(uint32_t hash, uint32_t c) noexcept {
  return hash * 65599u + c;
}

static BL_INLINE uint32_t hashRoundCI(uint32_t hash, uint32_t c) noexcept {
  return hash * 65599u + blAsciiToLower(c);
}

// Gets a hash of the given string `data` of size `size`. Size must be valid
// as this function doesn't check for a null terminator and allows it in the
// middle of the string.
static BL_INLINE uint32_t hashString(const char* data, size_t size) noexcept {
  uint32_t hashCode = 0;
  for (uint32_t i = 0; i < size; i++)
    hashCode = hashRound(hashCode, uint8_t(data[i]));
  return hashCode;
}

static BL_INLINE uint32_t hashString(BLStringView view) noexcept {
  return hashString(view.data, view.size);
}

// Gets a hash of the given string `data` of size `size`. Size must be valid
// as this function doesn't check for a null terminator and allows it in the
// middle of the string.
static BL_INLINE uint32_t hashStringCI(const char* data, size_t size) noexcept {
  uint32_t hashCode = 0;
  for (uint32_t i = 0; i < size; i++)
    hashCode = hashRoundCI(hashCode, uint8_t(data[i]));
  return hashCode;
}

static BL_INLINE uint32_t hashStringCI(BLStringView view) noexcept {
  return hashStringCI(view.data, view.size);
}

//! \}

} // {anonymous}
} // {BLHashOps}

//! \}
//! \endcond

#endif // BLEND2D_SUPPORT_HASHOPS_P_H_INCLUDED
