// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_UNICODE_UNICODE_P_H_INCLUDED
#define BLEND2D_UNICODE_UNICODE_P_H_INCLUDED

#include "../support/intops_p.h"
#include "../support/memops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {
namespace Unicode {

// bl::Unicode - Constants
// =======================

//! Special unicode characters.
enum CharCode : uint32_t {
  kCharBOM                    = 0x00FEFFu,   //!< Native Byte-Order-Mark.
  kCharMax                    = 0x10FFFFu,   //!< Last code-point.

  kCharReplacement            = 0x00FFFDu,   //!< Replacement character.

  kCharFVS1                   = 0x00180Bu,   //!< First char in Mongolian 'free variation selectors' FVS1..FVS3.
  kCharFVS3                   = 0x00180Du,   //!< Last char in Mongolian 'free variation selectors' FVS1..FVS3.

  kCharVS1                    = 0x00FE00u,   //!< First char in 'variation selectors' VS1..VS16.
  kCharVS16                   = 0x00FE0Fu,   //!< Last char in 'variation selectors' VS1..VS16.

  kCharVS17                   = 0x0E0100u,   //!< First char in 'variation selectors supplement' VS17..VS256.
  kCharVS256                  = 0x0E01EFu,   //!< Last char in 'variation selectors supplement' VS17..VS256.

  kCharSurrogateFirst         = 0x00D800u,   //!< First surrogate code-point.
  kCharSurrogateLast          = 0x00DFFFu,   //!< Last surrogate code-point.

  kCharHiSurrogateFirst       = 0x00D800u,   //!< First high-surrogate code-point
  kCharHiSurrogateLast        = 0x00DBFFu,   //!< Last high-surrogate code-point

  kCharLoSurrogateFirst       = 0x00DC00u,   //!< First low-surrogate code-point
  kCharLoSurrogateLast        = 0x00DFFFu    //!< Last low-surrogate code-point
};

//! Flags that can be used to parametrize unicode I/O iterators.
enum class IOFlags : uint32_t {
  kNoFlags     = 0u,
  kUnaligned   = 0x00000001u,
  kByteSwap    = 0x00000002u,
  kStrict      = 0x00000004u,
  kCalcIndex   = 0x00000008u,

  kByteOrderLE = BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_LE ? 0 : kByteSwap,
  kByteOrderBE = BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_BE ? 0 : kByteSwap
};

BL_DEFINE_ENUM_FLAGS(IOFlags)

// bl::Unicode - Data
// ==================

BL_HIDDEN extern const uint8_t utf8SizeData[256];

// bl::Unicode - Utilities
// =======================

namespace {

template<typename T>
BL_NODISCARD
BL_INLINE uint32_t utf8CharSize(const T& c) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return utf8SizeData[U(c)];
}

template<typename T>
BL_NODISCARD
BL_INLINE bool isValidUtf8(const T& c) noexcept {
  typedef typename std::make_unsigned<T>::type U;
  return U(c) < 128 || (U(c) - U(194) < U(245 - 194));
}

template<typename T>
BL_NODISCARD
constexpr bool isAsciiAlpha(const T& x) noexcept { return T(x | 0x20) >= T('a') && T(x | 0x20) <= T('z'); }

template<typename T>
BL_NODISCARD
constexpr bool isAsciiDigit(const T& x) noexcept { return x >= T('0') && x <= T('9'); }

template<typename T>
BL_NODISCARD
constexpr bool isAsciiAlnum(const T& x) noexcept { return isAsciiAlpha(x) || (x >= T('0') && x <= T('9')); }

template<typename T>
BL_NODISCARD
constexpr T asciiToLower(const T& x) noexcept { return x >= T('A') && x <= T('Z') ? T(x |  T(0x20)) : x; }

template<typename T>
BL_NODISCARD
constexpr T asciiToUpper(const T& x) noexcept { return x >= T('a') && x <= T('z') ? T(x & ~T(0x20)) : x; }

//! Tests whether the unicode character `uc` is high or low surrogate.
template<typename T>
BL_NODISCARD
constexpr bool isSurrogate(const T& uc) noexcept { return uc >= kCharSurrogateFirst && uc <= kCharSurrogateLast; }

//! Tests whether the unicode character `uc` is a high (leading) surrogate.
template<typename T>
BL_NODISCARD
constexpr bool isHiSurrogate(const T& uc) noexcept { return uc >= kCharHiSurrogateFirst && uc <= kCharHiSurrogateLast; }

//! Tests whether the unicode character `uc` is a low (trailing) surrogate.
template<typename T>
BL_NODISCARD
constexpr bool isLoSurrogate(const T& uc) noexcept { return uc >= kCharLoSurrogateFirst && uc <= kCharLoSurrogateLast; }

//! Composes `hi` and `lo` surrogates into a unicode code-point.
template<typename T>
BL_NODISCARD
constexpr uint32_t charFromSurrogate(const T& hi, const T& lo) noexcept {
  return (uint32_t(hi) << 10) + uint32_t(lo) - uint32_t((kCharSurrogateFirst << 10) + kCharLoSurrogateFirst - 0x10000u);
}

//! Decomposes a unicode code-point into `hi` and `lo` surrogates.
template<typename T>
BL_INLINE void blCharToSurrogate(uint32_t uc, T& hi, T& lo) noexcept {
  uc -= 0x10000u;
  hi = T(kCharHiSurrogateFirst | (uc >> 10));
  lo = T(kCharLoSurrogateFirst | (uc & 0x3FFu));
}

} // {anonymous}

// bl::Unicode - Validation
// ========================

struct ValidationState {
  size_t utf8Index;
  size_t utf16Index;
  size_t utf32Index;

  BL_INLINE void reset() noexcept { *this = ValidationState{}; }
  BL_INLINE bool hasSMP() const noexcept { return utf16Index != utf32Index; }
};

BL_HIDDEN BLResult blValidateUnicode(const void* data, size_t sizeInBytes, BLTextEncoding encoding, ValidationState& state) noexcept;

static BL_INLINE BLResult blValidateUtf8(const char* data, size_t size, ValidationState& state) noexcept {
  return blValidateUnicode(data, size, BL_TEXT_ENCODING_UTF8, state);
}

static BL_INLINE BLResult blValidateUtf16(const uint16_t* data, size_t size, ValidationState& state) noexcept {
  return blValidateUnicode(data, size * 2u, BL_TEXT_ENCODING_UTF16, state);
}

static BL_INLINE BLResult blValidateUtf32(const uint32_t* data, size_t size, ValidationState& state) noexcept {
  return blValidateUnicode(data, size * 4u, BL_TEXT_ENCODING_UTF32, state);
}

// bl::Unicode - Conversion
// ========================

struct ConversionState {
  size_t dstIndex;
  size_t srcIndex;

  BL_INLINE void reset() noexcept { *this = ConversionState{}; }
};

//! Converts a string from one encoding to another.
//!
//! Convert function works at a byte level. All sizes here are including those stored
//! in a `bl::Unicode::ConversionState` are byte entities. So for example to convert
//! a single UTF-16 BMP character the source size must be 2, etc...
BL_HIDDEN BLResult convertUnicode(
  void* dst, size_t dstSizeInBytes, uint32_t dstEncoding,
  const void* src, size_t srcSizeInBytes, uint32_t srcEncoding, ConversionState& state) noexcept;

// bl::Unicode - UTF8 Reader
// =========================

//! UTF-8 reader.
class Utf8Reader {
public:
  enum : uint32_t { kCharSize = 1 };

  //! Current pointer.
  const char* _ptr;
  //! End of input.
  const char* _end;
  //! `index() - _utf32IndexSubtract` yields the current `utf32Index`.
  size_t _utf32IndexSubtract;
  //! Number of surrogates is required to calculate `utf16Index`.
  size_t _utf16SurrogateCount;

  BL_INLINE Utf8Reader(const void* data, size_t byteSize) noexcept {
    reset(data, byteSize);
  }

  BL_INLINE void reset(const void* data, size_t byteSize) noexcept {
    _ptr = static_cast<const char*>(data);
    _end = static_cast<const char*>(data) + byteSize;
    _utf32IndexSubtract = 0;
    _utf16SurrogateCount = 0;
  }

  BL_NODISCARD
  BL_INLINE bool hasNext() const noexcept { return _ptr != _end; }

  BL_NODISCARD
  BL_INLINE size_t remainingByteSize() const noexcept { return (size_t)(_end - _ptr); }

  BL_NODISCARD
  BL_INLINE size_t byteIndex(const void* start) const noexcept { return (size_t)(_ptr - static_cast<const char*>(start)); }

  BL_NODISCARD
  BL_INLINE size_t utf8Index(const void* start) const noexcept { return byteIndex(start); }

  BL_NODISCARD
  BL_INLINE size_t utf16Index(const void* start) const noexcept { return utf32Index(start) + _utf16SurrogateCount; }

  BL_NODISCARD
  BL_INLINE size_t utf32Index(const void* start) const noexcept { return byteIndex(start) - _utf32IndexSubtract; }

  BL_NODISCARD
  BL_INLINE size_t nativeIndex(const void* start) const noexcept { return utf8Index(start); }

  template<IOFlags kFlags = IOFlags::kNoFlags>
  BL_INLINE BLResult next(uint32_t& uc) noexcept {
    size_t ucSizeInBytes;
    return next<kFlags>(uc, ucSizeInBytes);
  }

  template<IOFlags kFlags = IOFlags::kNoFlags>
  BL_INLINE BLResult next(uint32_t& uc, size_t& ucSizeInBytes) noexcept {
    BL_ASSERT(hasNext());

    uc = MemOps::readU8(_ptr);
    ucSizeInBytes = 1;

    _ptr++;
    if (uc < 0x80u) {
      // 1-Byte UTF-8 Sequence -> [0x00..0x7F].
      // ...nothing to do...
    }
    else {
      // Start of MultiByte.
      const uint32_t kMultiByte = 0xC2u;

      uc -= kMultiByte;
      if (uc < 0xE0u - kMultiByte) {
        // 2-Byte UTF-8 Sequence -> [0x80-0x7FF].
        _ptr++;
        ucSizeInBytes = 2;

        // Truncated input.
        if (BL_UNLIKELY(_ptr > _end))
          goto TruncatedString;

        // All consecutive bytes must be '10xxxxxx'.
        uint32_t b1 = MemOps::readU8(_ptr - 1) ^ 0x80u;
        uc = ((uc + kMultiByte - 0xC0u) << 6) + b1;

        if (BL_UNLIKELY(b1 > 0x3Fu))
          goto InvalidString;

        // 2-Byte UTF-8 maps to one UTF-16 or UTF-32 code-point, so subtract 1.
        if (blTestFlag(kFlags, IOFlags::kCalcIndex))
          _utf32IndexSubtract += 1;
      }
      else if (uc < 0xF0u - kMultiByte) {
        // 3-Byte UTF-8 Sequence -> [0x800-0xFFFF].
        _ptr += 2;
        ucSizeInBytes = 3;

        // Truncated input.
        if (BL_UNLIKELY(_ptr > _end))
          goto TruncatedString;

        uint32_t b1 = MemOps::readU8(_ptr - 2) ^ 0x80u;
        uint32_t b2 = MemOps::readU8(_ptr - 1) ^ 0x80u;
        uc = ((uc + kMultiByte - 0xE0u) << 12) + (b1 << 6) + b2;

        // 1. All consecutive bytes must be '10xxxxxx'.
        // 2. Refuse overlong UTF-8.
        if (BL_UNLIKELY((b1 | b2) > 0x3Fu || uc < 0x800u))
          goto InvalidString;

        // 3-Byte UTF-8 maps to one UTF-16 or UTF-32 code-point, so subtract 2.
        if (blTestFlag(kFlags, IOFlags::kCalcIndex))
          _utf32IndexSubtract += 2;
      }
      else {
        // 4-Byte UTF-8 Sequence -> [0x010000-0x10FFFF].
        _ptr += 3;
        ucSizeInBytes = 4;

        // Truncated input.
        if (BL_UNLIKELY(_ptr > _end)) {
          // If this happens we want to report a correct error, bytes 0xF5
          // and above are always invalid and normally caught later.
          if (uc >= 0xF5u - kMultiByte)
            goto InvalidString;
          else
            goto TruncatedString;
        }

        uint32_t b1 = MemOps::readU8(_ptr - 3) ^ 0x80u;
        uint32_t b2 = MemOps::readU8(_ptr - 2) ^ 0x80u;
        uint32_t b3 = MemOps::readU8(_ptr - 1) ^ 0x80u;
        uc = ((uc + kMultiByte - 0xF0u) << 18) + (b1 << 12) + (b2 << 6) + b3;

        // 1. All consecutive bytes must be '10xxxxxx'.
        // 2. Refuse overlong UTF-8.
        // 3. Make sure the final character is <= U+10FFFF.
        if (BL_UNLIKELY((b1 | b2 | b3) > 0x3Fu || uc < 0x010000u || uc > kCharMax))
          goto InvalidString;

        // 4-Byte UTF-8 maps to one UTF-16 or UTF-32 code-point, so subtract 3.
        if (blTestFlag(kFlags, IOFlags::kCalcIndex)) {
          _utf32IndexSubtract += 3;
          _utf16SurrogateCount += 1;
        }
      }
    }
    return BL_SUCCESS;

InvalidString:
    _ptr -= ucSizeInBytes;
    return blTraceError(BL_ERROR_INVALID_STRING);

TruncatedString:
    _ptr -= ucSizeInBytes;
    return blTraceError(BL_ERROR_DATA_TRUNCATED);
  }

  BL_INLINE void skipOneUnit() noexcept {
    BL_ASSERT(hasNext());
    _ptr++;
  }

  template<IOFlags kFlags = IOFlags::kNoFlags>
  BL_NODISCARD
  BL_INLINE BLResult validate() noexcept {
    BLResult result = BL_SUCCESS;
    while (hasNext()) {
      uint32_t uc;
      result = next<kFlags>(uc);
      if (result)
        break;
    }
    return result;
  }
};

// bl::Unicode - UTF16 Reader
// ==========================

//! UTF-16 reader.
class Utf16Reader {
public:
  enum : uint32_t { kCharSize = 2 };

  const char* _ptr;
  const char* _end;

  size_t _utf8IndexAdd;
  size_t _utf16SurrogateCount;

  BL_INLINE Utf16Reader(const void* data, size_t byteSize) noexcept {
    reset(data, byteSize);
  }

  //! \name Reset
  //! \{

  BL_INLINE void reset(const void* data, size_t byteSize) noexcept {
    _ptr = static_cast<const char*>(data);
    _end = static_cast<const char*>(data) + IntOps::alignDown(byteSize, 2);
    _utf8IndexAdd = 0;
    _utf16SurrogateCount = 0;
  }

  //! \}

  //! \name Accessors
  //! \{

  BL_NODISCARD
  BL_INLINE bool hasNext() const noexcept { return _ptr != _end; }

  BL_NODISCARD
  BL_INLINE size_t remainingByteSize() const noexcept { return (size_t)(_end - _ptr); }

  BL_NODISCARD
  BL_INLINE size_t byteIndex(const void* start) const noexcept { return (size_t)(_ptr - static_cast<const char*>(start)); }

  BL_NODISCARD
  BL_INLINE size_t utf8Index(const void* start) const noexcept { return utf16Index(start) + _utf8IndexAdd; }

  BL_NODISCARD
  BL_INLINE size_t utf16Index(const void* start) const noexcept { return byteIndex(start) / 2u; }

  BL_NODISCARD
  BL_INLINE size_t utf32Index(const void* start) const noexcept { return utf16Index(start) - _utf16SurrogateCount; }

  BL_NODISCARD
  BL_INLINE size_t nativeIndex(const void* start) const noexcept { return utf16Index(start); }

  //! \}

  //! \name Iterator
  //! \{

  template<IOFlags kFlags = IOFlags::kNoFlags>
  BL_INLINE BLResult next(uint32_t& uc) noexcept {
    size_t ucSizeInBytes;
    return next<kFlags>(uc, ucSizeInBytes);
  }

  template<IOFlags kFlags = IOFlags::kNoFlags>
  BL_INLINE BLResult next(uint32_t& uc, size_t& ucSizeInBytes) noexcept {
    BL_ASSERT(hasNext());

    uc = readU16<kFlags>(_ptr);
    _ptr += 2;

    if (isSurrogate(uc)) {
      if (BL_LIKELY(isHiSurrogate(uc))) {
        if (BL_LIKELY(_ptr != _end)) {
          uint32_t lo = readU16<kFlags>(_ptr);
          if (BL_LIKELY(isLoSurrogate(lo))) {
            uc = charFromSurrogate(uc, lo);
            _ptr += 2;

            // Add two to `_utf8IndexAdd` as two surrogates count as 2, so we
            // have to add 2 more to have UTF-8 length of a valid surrogate.
            if (blTestFlag(kFlags, IOFlags::kCalcIndex)) {
              _utf8IndexAdd += 2;
              _utf16SurrogateCount += 1;
            }

            ucSizeInBytes = 4;
            return BL_SUCCESS;
          }
          else {
            if (blTestFlag(kFlags, IOFlags::kStrict))
              goto InvalidString;
          }
        }
        else {
          if (blTestFlag(kFlags, IOFlags::kStrict))
            goto TruncatedString;
        }
      }
      else {
        if (blTestFlag(kFlags, IOFlags::kStrict))
          goto InvalidString;
      }
    }

    // Either not surrogate or fallback in non-strict mode.
    if (blTestFlag(kFlags, IOFlags::kCalcIndex))
      _utf8IndexAdd += size_t(uc >= 0x0080u) + size_t(uc >= 0x0800u);

    ucSizeInBytes = 2;
    return BL_SUCCESS;

InvalidString:
    _ptr -= 2;
    return blTraceError(BL_ERROR_INVALID_STRING);

TruncatedString:
    _ptr -= 2;
    return blTraceError(BL_ERROR_DATA_TRUNCATED);
  }

  BL_INLINE void skipOneUnit() noexcept {
    BL_ASSERT(hasNext());
    _ptr += 2;
  }

  //! \}

  //! \name Validator
  //! \{

  template<IOFlags kFlags = IOFlags::kNoFlags>
  BL_NODISCARD
  BL_INLINE BLResult validate() noexcept {
    BLResult result = BL_SUCCESS;
    while (hasNext()) {
      uint32_t uc;
      result = next<kFlags>(uc);
      if (result)
        break;
    }
    return result;
  }

  //! \}

  //! \name Utilities
  //! \{

  template<IOFlags kFlags = IOFlags::kNoFlags>
  BL_NODISCARD
  static BL_INLINE uint32_t readU16(const char* ptr) noexcept {
    constexpr uint32_t kByteOrder = blTestFlag(kFlags, IOFlags::kByteSwap) ? BL_BYTE_ORDER_SWAPPED : BL_BYTE_ORDER_NATIVE;
    constexpr uint32_t kAlignment = blTestFlag(kFlags, IOFlags::kUnaligned) ? 1 : 2;
    return MemOps::readU16<kByteOrder, kAlignment>(ptr);
  }

  //! \}
};

// bl::Unicode - UTF32 Reader
// ==========================

//! UTF-32 reader.
class Utf32Reader {
public:
  enum : uint32_t { kCharSize = 4 };

  const char* _ptr;
  const char* _end;

  size_t _utf8IndexAdd;
  size_t _utf16SurrogateCount;

  BL_INLINE Utf32Reader(const void* data, size_t byteSize) noexcept {
    reset(data, byteSize);
  }

  BL_INLINE void reset(const void* data, size_t byteSize) noexcept {
    _ptr = static_cast<const char*>(data);
    _end = static_cast<const char*>(data) + IntOps::alignDown(byteSize, 4);
    _utf8IndexAdd = 0;
    _utf16SurrogateCount = 0;
  }

  BL_NODISCARD
  BL_INLINE bool hasNext() const noexcept { return _ptr != _end; }

  BL_NODISCARD
  BL_INLINE size_t remainingByteSize() const noexcept { return (size_t)(_end - _ptr); }

  BL_NODISCARD
  BL_INLINE size_t byteIndex(const void* start) const noexcept { return (size_t)(_ptr - static_cast<const char*>(start)); }

  BL_NODISCARD
  BL_INLINE size_t utf8Index(const void* start) const noexcept { return utf32Index(start) + _utf16SurrogateCount + _utf8IndexAdd; }

  BL_NODISCARD
  BL_INLINE size_t utf16Index(const void* start) const noexcept { return utf32Index(start) + _utf16SurrogateCount; }

  BL_NODISCARD
  BL_INLINE size_t utf32Index(const void* start) const noexcept { return byteIndex(start) / 4u; }

  BL_NODISCARD
  BL_INLINE size_t nativeIndex(const void* start) const noexcept { return utf32Index(start); }

  template<IOFlags kFlags = IOFlags::kNoFlags>
  BL_INLINE BLResult next(uint32_t& uc) noexcept {
    size_t ucSizeInBytes;
    return next<kFlags>(uc, ucSizeInBytes);
  }

  template<IOFlags kFlags = IOFlags::kNoFlags>
  BL_INLINE BLResult next(uint32_t& uc, size_t& ucSizeInBytes) noexcept {
    BL_ASSERT(hasNext());

    uc = readU32<kFlags>(_ptr);
    if (BL_UNLIKELY(uc > kCharMax))
      return blTraceError(BL_ERROR_INVALID_STRING);

    if (blTestFlag(kFlags, IOFlags::kStrict)) {
      if (BL_UNLIKELY(isSurrogate(uc)))
        return blTraceError(BL_ERROR_INVALID_STRING);
    }

    if (blTestFlag(kFlags, IOFlags::kCalcIndex)) {
      _utf8IndexAdd += size_t(uc >= 0x800u) + size_t(uc >= 0x80u);
      _utf16SurrogateCount += size_t(uc >= 0x10000u);
    }

    _ptr += 4;
    ucSizeInBytes = 4;
    return BL_SUCCESS;
  }

  BL_INLINE void skipOneUnit() noexcept {
    BL_ASSERT(hasNext());
    _ptr += 4;
  }

  template<IOFlags kFlags = IOFlags::kNoFlags>
  BL_NODISCARD
  BL_INLINE BLResult validate() noexcept {
    BLResult result = BL_SUCCESS;
    while (hasNext()) {
      uint32_t uc;
      result = next<kFlags>(uc);
      if (result)
        break;
    }
    return result;
  }

  template<IOFlags kFlags = IOFlags::kNoFlags>
  BL_NODISCARD
  static BL_INLINE uint32_t readU32(const char* ptr) noexcept {
    constexpr uint32_t kByteOrder = blTestFlag(kFlags, IOFlags::kByteSwap) ? BL_BYTE_ORDER_SWAPPED : BL_BYTE_ORDER_NATIVE;
    constexpr uint32_t kAlignment = blTestFlag(kFlags, IOFlags::kUnaligned) ? 1 : 4;
    return MemOps::readU32<kByteOrder, kAlignment>(ptr);
  }
};

// bl::Unicode - UTF8 Writer
// ===========================

//! UTF8 writer.
class Utf8Writer {
public:
  typedef char CharType;

  char* _ptr;
  char* _end;

  BL_INLINE Utf8Writer(char* dst, size_t size) noexcept {
    reset(dst, size);
  }

  BL_INLINE void reset(char* dst, size_t size) noexcept {
    _ptr = dst;
    _end = dst + size;
  }

  BL_NODISCARD
  BL_INLINE size_t index(const char* start) const noexcept { return (size_t)(_ptr - start); }

  BL_NODISCARD
  BL_INLINE bool atEnd() const noexcept { return _ptr == _end; }

  BL_NODISCARD
  BL_INLINE size_t remainingSize() const noexcept { return (size_t)(_end - _ptr); }

  BL_INLINE BLResult write(uint32_t uc) noexcept {
    if (uc <= 0x7F)
      return writeByte(uc);
    else if (uc <= 0x7FFu)
      return write2Bytes(uc);
    else if (uc <= 0xFFFFu)
      return write3Bytes(uc);
    else
      return write4Bytes(uc);
  }

  BL_INLINE BLResult writeUnsafe(uint32_t uc) noexcept {
    if (uc <= 0x7F)
      return writeByteUnsafe(uc);
    else if (uc <= 0x7FFu)
      return write2BytesUnsafe(uc);
    else if (uc <= 0xFFFFu)
      return write3BytesUnsafe(uc);
    else
      return write4BytesUnsafe(uc);
  }

  BL_INLINE BLResult writeByte(uint32_t uc) noexcept {
    BL_ASSERT(uc <= 0x7Fu);
    if (BL_UNLIKELY(atEnd()))
      return blTraceError(BL_ERROR_NO_SPACE_LEFT);

    _ptr[0] = char(uint8_t(uc));
    _ptr++;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult writeByteUnsafe(uint32_t uc) noexcept {
    BL_ASSERT(remainingSize() >= 1);
    _ptr[0] = char(uint8_t(uc));
    _ptr++;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult write2Bytes(uint32_t uc) noexcept {
    BL_ASSERT(uc >= 0x80u && uc <= 0x7FFu);

    _ptr += 2;
    if (BL_UNLIKELY(_ptr > _end)) {
      _ptr -= 2;
      return blTraceError(BL_ERROR_NO_SPACE_LEFT);
    }

    _ptr[-2] = char(uint8_t(0xC0u | (uc >> 6)));
    _ptr[-1] = char(uint8_t(0x80u | (uc & 63)));
    return BL_SUCCESS;
  }

  BL_INLINE BLResult write2BytesUnsafe(uint32_t uc) noexcept {
    BL_ASSERT(remainingSize() >= 2);
    BL_ASSERT(uc >= 0x80u && uc <= 0x7FFu);

    _ptr[0] = char(uint8_t(0xC0u | (uc >> 6)));
    _ptr[1] = char(uint8_t(0x80u | (uc & 63)));

    _ptr += 2;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult write3Bytes(uint32_t uc) noexcept {
    BL_ASSERT(uc >= 0x800u && uc <= 0xFFFFu);

    _ptr += 3;
    if (BL_UNLIKELY(_ptr > _end)) {
      _ptr -= 3;
      return blTraceError(BL_ERROR_NO_SPACE_LEFT);
    }

    _ptr[-3] = char(uint8_t(0xE0u | ((uc >> 12)     )));
    _ptr[-2] = char(uint8_t(0x80u | ((uc >>  6) & 63)));
    _ptr[-1] = char(uint8_t(0x80u | ((uc      ) & 63)));
    return BL_SUCCESS;
  }

  BL_INLINE BLResult write3BytesUnsafe(uint32_t uc) noexcept {
    BL_ASSERT(remainingSize() >= 3);
    BL_ASSERT(uc >= 0x800u && uc <= 0xFFFFu);

    _ptr[0] = char(uint8_t(0xE0u | ((uc >> 12)     )));
    _ptr[1] = char(uint8_t(0x80u | ((uc >>  6) & 63)));
    _ptr[2] = char(uint8_t(0x80u | ((uc      ) & 63)));

    _ptr += 3;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult write4Bytes(uint32_t uc) noexcept {
    BL_ASSERT(uc >= 0x10000u && uc <= 0x10FFFFu);

    _ptr += 4;
    if (BL_UNLIKELY(_ptr > _end)) {
      _ptr -= 4;
      return blTraceError(BL_ERROR_NO_SPACE_LEFT);
    }

    _ptr[-4] = char(uint8_t(0xF0u | ((uc >> 18)     )));
    _ptr[-3] = char(uint8_t(0x80u | ((uc >> 12) & 63)));
    _ptr[-2] = char(uint8_t(0x80u | ((uc >>  6) & 63)));
    _ptr[-1] = char(uint8_t(0x80u | ((uc      ) & 63)));
    return BL_SUCCESS;
  }

  BL_INLINE BLResult write4BytesUnsafe(uint32_t uc) noexcept {
    BL_ASSERT(remainingSize() >= 4);
    BL_ASSERT(uc >= 0x10000u && uc <= 0x10FFFFu);

    _ptr[0] = char(uint8_t(0xF0u | ((uc >> 18)     )));
    _ptr[1] = char(uint8_t(0x80u | ((uc >> 12) & 63)));
    _ptr[2] = char(uint8_t(0x80u | ((uc >>  6) & 63)));
    _ptr[3] = char(uint8_t(0x80u | ((uc      ) & 63)));

    _ptr += 4;
    return BL_SUCCESS;
  }
};

// bl::Unicode - UTF16 Writer
// ==========================

//! UTF16 writer that can be parametrized by `ByteOrder` and `Alignment`.
template<uint32_t ByteOrder = BL_BYTE_ORDER_NATIVE, uint32_t Alignment = 2>
class Utf16Writer {
public:
  typedef uint16_t CharType;

  uint16_t* _ptr;
  uint16_t* _end;

  BL_INLINE Utf16Writer(uint16_t* dst, size_t size) noexcept {
    reset(dst, size);
  }

  BL_INLINE void reset(uint16_t* dst, size_t size) noexcept {
    _ptr = dst;
    _end = dst + size;
  }

  BL_NODISCARD
  BL_INLINE size_t index(const uint16_t* start) const noexcept { return (size_t)(_ptr - start); }

  BL_NODISCARD
  BL_INLINE bool atEnd() const noexcept { return _ptr == _end; }

  BL_NODISCARD
  BL_INLINE size_t remainingSize() const noexcept { return (size_t)(_end - _ptr); }

  BL_INLINE BLResult write(uint32_t uc) noexcept {
    if (uc <= 0xFFFFu)
      return writeBMP(uc);
    else
      return writeSMP(uc);
  }

  BL_INLINE BLResult writeBMP(uint32_t uc) noexcept {
    BL_ASSERT(uc <= 0xFFFFu);

    if (BL_UNLIKELY(atEnd()))
      return blTraceError(BL_ERROR_NO_SPACE_LEFT);

    _writeMemU16(_ptr, uc);
    _ptr++;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult writeBMPUnsafe(uint32_t uc) noexcept {
    BL_ASSERT(remainingSize() >= 1);

    _writeMemU16(_ptr, uc);
    _ptr++;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult writeSMP(uint32_t uc) noexcept {
    BL_ASSERT(uc >= 0x10000u && uc <= 0x10FFFFu);

    _ptr += 2;
    if (BL_UNLIKELY(_ptr > _end)) {
      _ptr -= 2;
      return blTraceError(BL_ERROR_NO_SPACE_LEFT);
    }

    uint32_t hi, lo;
    blCharToSurrogate(uc, hi, lo);

    _writeMemU16(_ptr - 2, hi);
    _writeMemU16(_ptr - 1, lo);
    return BL_SUCCESS;
  }

  BL_INLINE BLResult writeSMPUnsafe(uint32_t uc) noexcept {
    BL_ASSERT(remainingSize() >= 2);
    BL_ASSERT(uc >= 0x10000u && uc <= 0x10FFFFu);

    uint32_t hi, lo;
    blCharToSurrogate(uc, hi, lo);

    _writeMemU16(_ptr + 0, hi);
    _writeMemU16(_ptr + 1, lo);

    _ptr += 2;
    return BL_SUCCESS;
  }

  //! \name Utilities
  //! \{

  static BL_INLINE void _writeMemU16(void* dst, uint32_t value) noexcept {
    MemOps::writeU16<ByteOrder, Alignment>(dst, value);
  }

  //! \}
};

// bl::Unicode - UTF32 Writer
// ==========================

//! UTF32 writer that can be parametrized by `ByteOrder` and `Alignment`.
template<uint32_t ByteOrder = BL_BYTE_ORDER_NATIVE, uint32_t Alignment = 4>
class Utf32Writer {
public:
  typedef uint32_t CharType;

  uint32_t* _ptr;
  uint32_t* _end;

  BL_INLINE Utf32Writer(uint32_t* dst, size_t size) noexcept {
    reset(dst, size);
  }

  BL_INLINE void reset(uint32_t* dst, size_t size) noexcept {
    _ptr = dst;
    _end = dst + size;
  }

  BL_NODISCARD
  BL_INLINE size_t index(const uint32_t* start) const noexcept { return (size_t)(_ptr - start); }

  BL_NODISCARD
  BL_INLINE bool atEnd() const noexcept { return _ptr == _end; }

  BL_NODISCARD
  BL_INLINE size_t remainingSize() const noexcept { return (size_t)(_end - _ptr); }

  BL_INLINE BLResult write(uint32_t uc) noexcept {
    if (BL_UNLIKELY(atEnd()))
      return blTraceError(BL_ERROR_NO_SPACE_LEFT);

    _writeMemU32(_ptr, uc);
    _ptr++;
    return BL_SUCCESS;
  }

  static BL_INLINE void _writeMemU32(void* dst, uint32_t value) noexcept {
    MemOps::writeU32<ByteOrder, Alignment>(dst, value);
  }
};

} // {Unicode}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_UNICODE_UNICODE_P_H_INCLUDED
