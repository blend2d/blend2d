// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_AXL_STRING_UTILS_P_H_INCLUDED
#define BLEND2D_AXL_STRING_UTILS_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/axl/commons.h>

namespace bl::axl {
namespace {

//! \cond INTERNAL
//! \addtogroup blend2d_support
//! \{

// Support - ASCII Utilities
// =========================

template<typename T>
[[nodiscard]]
BL_INLINE_CONSTEXPR bool is_ascii_between(const T& c, const T& a, const T& b) noexcept { return uint32_t(c) - uint32_t(a) <= uint32_t(b) - uint32_t(a); }

template<typename T>
[[nodiscard]]
BL_INLINE_CONSTEXPR bool is_ascii_space(const T& c) noexcept { return bool_or(c == T(' '), c == T('\t'), c == T('\r'), c == T('\n')); }

template<typename T>
[[nodiscard]]
BL_INLINE_CONSTEXPR bool is_ascii_alpha(const T& c) noexcept { return is_ascii_between(T(uint32_t(c) | 0x20u), T('a'), T('z')); }

template<typename T>
[[nodiscard]]
BL_INLINE_CONSTEXPR bool is_ascii_digit(const T& c) noexcept { return is_ascii_between(c, T('0'), T('9')); }

template<typename T>
[[nodiscard]]
BL_INLINE_CONSTEXPR bool is_ascii_alnum(const T& c) noexcept { return bool_or(is_ascii_alpha(c), is_ascii_digit(c)); }

template<typename T, typename... Args>
[[nodiscard]]
BL_INLINE_CONSTEXPR bool is_ascii_alnum_or_(const T& c, Args&&... additional_chars) noexcept {
  return is_ascii_alnum(c) || (... || (c == T(additional_chars)));
}

template<typename T>
[[nodiscard]]
BL_INLINE_CONSTEXPR T ascii_to_lower(const T& c) noexcept { return T(c ^ T(T(c >= T('A') && c <= T('Z')) << 5)); }

template<typename T>
[[nodiscard]]
BL_INLINE_CONSTEXPR T ascii_to_upper(const T& c) noexcept { return T(c ^ T(T(c >= T('a') && c <= T('z')) << 5)); }

// Support - String Utilities
// ==========================

template<typename T>
BL_INLINE_NODEBUG size_t str_len(const T* data) noexcept {
  if (sizeof(T) == 1) {
    return strlen(reinterpret_cast<const char*>(data));
  }
  else {
    const T* p = data;
    while (p[0]) {
      p++;
    }
    return (size_t)(p - data);
  }
}

template<typename T>
[[nodiscard]]
BL_INLINE_NODEBUG size_t str_nlen(const T* s, size_t max_size) noexcept {
  size_t i = 0;
  while (i < max_size && s[i] != T(0)) {
    i++;
  }
  return i;
}

static size_t length_with_encoding(const void* text, BLTextEncoding encoding) noexcept {
  switch (encoding) {
    case BL_TEXT_ENCODING_LATIN1:
    case BL_TEXT_ENCODING_UTF8:
      return axl::str_len(static_cast<const uint8_t*>(text));

    case BL_TEXT_ENCODING_UTF16:
      return axl::str_len(static_cast<const uint16_t*>(text));

    case BL_TEXT_ENCODING_UTF32:
      return axl::str_len(static_cast<const uint32_t*>(text));

    default:
      return 0;
  }
}

static BL_INLINE bool memeq_ci(const char* a, const char* b, size_t size) noexcept {
  size_t i;
  for (i = 0; i < size; i++) {
    if (ascii_to_lower(a[i]) != ascii_to_lower(b[i])) {
      return false;
    }
  }
  return true;
}

// Support - String Hashing
// ========================

template<typename T>
BL_INLINE_CONSTEXPR uint32_t hash_char(uint32_t hash, const T& c) noexcept {
  return hash * 65599u + uint32_t(c);
}

template<typename T>
BL_INLINE_CONSTEXPR uint32_t hash_char_ci(uint32_t hash, const T& c) noexcept {
  return hash * 65599u + ascii_to_lower(uint32_t(c));
}

// Gets a hash of the given string `data` of size `size`. Size must be valid
// as this function doesn't check for a null terminator and allows it in the
// middle of the string.
BL_INLINE_CONSTEXPR uint32_t hash_string(const char* data, size_t size) noexcept {
  uint32_t hash_code = 0;
  for (uint32_t i = 0; i < size; i++) {
    hash_code = hash_char(hash_code, uint8_t(data[i]));
  }
  return hash_code;
}

BL_INLINE_CONSTEXPR uint32_t hash_string(BLSpan<const char> s) noexcept {
  return hash_string(s.data(), s.size());
}

// Gets a hash of the given string `data` of size `size`. Size must be valid
// as this function doesn't check for a null terminator and allows it in the
// middle of the string.
BL_INLINE_CONSTEXPR uint32_t hash_string_ci(const char* data, size_t size) noexcept {
  uint32_t hash_code = 0;
  for (uint32_t i = 0; i < size; i++) {
    hash_code = hash_char_ci(hash_code, uint8_t(data[i]));
  }
  return hash_code;
}

BL_INLINE_CONSTEXPR uint32_t hash_string_ci(BLSpan<const char> s) noexcept {
  return hash_string_ci(s.data(), s.size());
}

//! \}
//! \endcond

} // {anonymous}
} // {bl::axl}

#endif // BLEND2D_AXL_STRING_UTILS_P_H_INCLUDED
