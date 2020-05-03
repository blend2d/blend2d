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

#ifndef BLEND2D_STRING_P_H_INCLUDED
#define BLEND2D_STRING_P_H_INCLUDED

#include "./api-internal_p.h"
#include "./array_p.h"
#include "./string.h"
#include "./unicode_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLString - Internal]
// ============================================================================

BL_HIDDEN BLResult blStringImplDelete(BLStringImpl* impl) noexcept;

// ============================================================================
// [BLString - StrLen]
// ============================================================================

template<typename T>
static size_t BL_INLINE blStrLen(const T* data) noexcept {
  if (sizeof(T) == 1) {
    return strlen(reinterpret_cast<const char*>(data));
  }
  else {
    const T* p = data;
    while (p[0])
      p++;
    return (size_t)(p - data);
  }
}

static size_t blStrLenWithEncoding(const void* text, uint32_t encoding) noexcept {
  switch (encoding) {
    case BL_TEXT_ENCODING_LATIN1:
    case BL_TEXT_ENCODING_UTF8:
      return blStrLen(static_cast<const uint8_t*>(text));

    case BL_TEXT_ENCODING_UTF16:
      return blStrLen(static_cast<const uint16_t*>(text));

    case BL_TEXT_ENCODING_UTF32:
      return blStrLen(static_cast<const uint32_t*>(text));

    default:
      return 0;
  }
}

// ============================================================================
// [BLString - StrEq]
// ============================================================================

static BL_INLINE bool blStrEq(const char* a, const char* b, size_t bSize) noexcept {
  size_t i;
  for (i = 0; i < bSize; i++)
    if (a[i] == 0 || a[i] != b[i])
      return false;
  return a[i] == 0;
}

static BL_INLINE bool blStrEqI(const char* a, const char* b, size_t bSize) noexcept {
  size_t i;
  for (i = 0; i < bSize; i++)
    if (a[i] == 0 || blAsciiToLower(a[i]) != blAsciiToLower(b[i]))
      return false;
  return a[i] == 0;
}

static BL_INLINE bool blMemEqI(const char* a, const char* b, size_t size) noexcept {
  size_t i;
  for (i = 0; i < size; i++)
    if (blAsciiToLower(a[i]) != blAsciiToLower(b[i]))
      return false;
  return true;
}

//! \}
//! \endcond

#endif // BLEND2D_STRING_P_H_INCLUDED
