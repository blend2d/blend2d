// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../unicode/unicode_p.h"
#include "../support/intops_p.h"
#include "../support/memops_p.h"

// BLUnicode - Data
// ================

// NOTE: Theoretically UTF-8 sequence can be extended to support sequences up
// to 6 bytes, however, since UCS-4 code-point's maximum value is 0x10FFFF it
// also limits the maximum length of a UTF-8 sequence to 4 bytes.
const uint8_t blUtf8SizeData[256] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0   - 15
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 16  - 31
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 32  - 47
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 48  - 63
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 64  - 79
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 80  - 95
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 96  - 111
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 112 - 127
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 128 - 143
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 144 - 159
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 160 - 175
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 176 - 191
  0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // 192 - 207
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // 208 - 223
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, // 224 - 239
  4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // 240 - 255
};

// BLUnicode - Validation
// ======================

// Not really anything to validate, we just want to calculate a corresponding UTf-8 size.
static BL_INLINE BLResult validateLatin1String(const char* data, size_t size, BLUnicodeValidationState& state) noexcept {
  size_t extra = 0;
  state.utf16Index = size;
  state.utf32Index = size;

  for (size_t i = 0; i < size; i++)
    extra += size_t(uint8_t(data[i])) >> 7;

  BLOverflowFlag of = 0;
  size_t utf8Size = BLIntOps::addOverflow(size, extra, &of);

  if (BL_UNLIKELY(of))
    return blTraceError(BL_ERROR_DATA_TOO_LARGE);

  state.utf8Index = utf8Size;
  return BL_SUCCESS;
}

template<typename Iterator, uint32_t Flags>
static BL_INLINE BLResult validateUnicodeString(const void* data, size_t size, BLUnicodeValidationState& state) noexcept {
  Iterator it(data, size);
  BLResult result = it.template validate<Flags | BL_UNICODE_IO_CALC_INDEX>();
  state.utf8Index = it.utf8Index(data);
  state.utf16Index = it.utf16Index(data);
  state.utf32Index = it.utf32Index(data);
  return result;
}

BLResult blValidateUnicode(const void* data, size_t sizeInBytes, BLTextEncoding encoding, BLUnicodeValidationState& state) noexcept {
  BLResult result;
  state.reset();

  switch (encoding) {
    case BL_TEXT_ENCODING_LATIN1:
      return validateLatin1String(static_cast<const char*>(data), sizeInBytes, state);

    case BL_TEXT_ENCODING_UTF8:
      return validateUnicodeString<BLUtf8Reader, BL_UNICODE_IO_STRICT>(data, sizeInBytes, state);

    case BL_TEXT_ENCODING_UTF16:
      // This will make sure we won't compile specialized code for architectures that don't penalize unaligned reads.
      if (BLMemOps::kUnalignedMem16 || !BLIntOps::isAligned(data, 2))
        result = validateUnicodeString<BLUtf16Reader, BL_UNICODE_IO_STRICT | BL_UNICODE_IO_UNALIGNED>(data, sizeInBytes, state);
      else
        result = validateUnicodeString<BLUtf16Reader, BL_UNICODE_IO_STRICT>(data, sizeInBytes, state);

      if (result == BL_SUCCESS && BL_UNLIKELY(sizeInBytes & 0x1))
        result = blTraceError(BL_ERROR_DATA_TRUNCATED);
      return result;

    case BL_TEXT_ENCODING_UTF32:
      // This will make sure we won't compile specialized code for architectures that don't penalize unaligned reads.
      if (BLMemOps::kUnalignedMem32 || !BLIntOps::isAligned(data, 4))
        result = validateUnicodeString<BLUtf32Reader, BL_UNICODE_IO_STRICT | BL_UNICODE_IO_UNALIGNED>(data, sizeInBytes, state);
      else
        result = validateUnicodeString<BLUtf32Reader, BL_UNICODE_IO_STRICT>(data, sizeInBytes, state);

      if (result == BL_SUCCESS && BL_UNLIKELY(sizeInBytes & 0x3))
        result = blTraceError(BL_ERROR_DATA_TRUNCATED);
      return result;

    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }
}

// BLUnicode - Conversion
// ======================

static BL_INLINE size_t offsetOfPtr(const void* base, const void* advanced) noexcept {
  return (size_t)(static_cast<const char*>(advanced) - static_cast<const char*>(base));
}

// A simple implementation. It iterates `src` char-by-char and writes it to the
// destination. The advantage of this implementation is that switching `Writer`
// and `Iterator` can customize strictness, endianness, etc, so we don't have
// to repeat the code for different variations of UTF16 and UTF32.
template<typename Writer, typename Iterator, uint32_t IterFlags>
static BL_INLINE BLResult blConvertUnicodeImpl(void* dst, size_t dstSizeInBytes, const void* src, size_t srcSizeInBytes, BLUnicodeConversionState& state) noexcept {
  typedef typename Writer::CharType DstChar;

  Writer writer(static_cast<DstChar*>(dst), dstSizeInBytes / sizeof(DstChar));
  Iterator iter(src, BLIntOps::alignDown(srcSizeInBytes, Iterator::kCharSize));

  BLResult result = BL_SUCCESS;
  while (iter.hasNext()) {
    uint32_t uc;
    size_t ucSizeInBytes;

    result = iter.template next<IterFlags>(uc, ucSizeInBytes);
    if (BL_UNLIKELY(result != BL_SUCCESS))
      break;

    result = writer.write(uc);
    if (BL_UNLIKELY(result != BL_SUCCESS)) {
      state.dstIndex = offsetOfPtr(dst, writer._ptr);
      state.srcIndex = offsetOfPtr(src, iter._ptr) - ucSizeInBytes;
      return result;
    }
  }

  state.dstIndex = offsetOfPtr(dst, writer._ptr);
  state.srcIndex = offsetOfPtr(src, iter._ptr);

  if (Iterator::kCharSize > 1 && result == BL_SUCCESS && !BLIntOps::isAligned(state.srcIndex, Iterator::kCharSize))
    return blTraceError(BL_ERROR_DATA_TRUNCATED);
  else
    return result;
}

BLResult blConvertUnicode(
  void* dst, size_t dstSizeInBytes, uint32_t dstEncoding,
  const void* src, size_t srcSizeInBytes, uint32_t srcEncoding,
  BLUnicodeConversionState& state) noexcept {

  constexpr bool BL_UNALIGNED_IO_Any = BLMemOps::kUnalignedMem16 && BLMemOps::kUnalignedMem32;

  BLResult result = BL_SUCCESS;
  state.reset();

  uint32_t encodingCombined = (dstEncoding << 2) | srcEncoding;
  switch (encodingCombined) {
    // MemCpy
    // ------

    case (BL_TEXT_ENCODING_LATIN1 << 2) | BL_TEXT_ENCODING_LATIN1: {
      size_t copySize = blMin(dstSizeInBytes, srcSizeInBytes);
      memcpy(dst, src, copySize);

      state.dstIndex = copySize;
      state.srcIndex = copySize;

      if (dstSizeInBytes < srcSizeInBytes)
        result = blTraceError(BL_ERROR_NO_SPACE_LEFT);
      break;
    }

    // Utf8 <- Latin1
    // --------------

    case (BL_TEXT_ENCODING_UTF8 << 2) | BL_TEXT_ENCODING_LATIN1: {
      BLUtf8Writer writer(static_cast<char*>(dst), dstSizeInBytes);
      const uint8_t* src8 = static_cast<const uint8_t*>(src);

      if (dstSizeInBytes / 2 >= srcSizeInBytes) {
        // Fast case, there is enough space in `dst` even for the worst-case scenario.
        for (size_t i = 0; i < srcSizeInBytes; i++) {
          uint32_t uc = src8[i];
          if (uc <= 0x7F)
            writer.writeByteUnsafe(uc);
          else
            writer.write2BytesUnsafe(uc);
        }

        state.srcIndex = srcSizeInBytes;
        state.dstIndex = writer.index(static_cast<char*>(dst));
      }
      else {
        for (size_t i = 0; i < srcSizeInBytes; i++) {
          uint32_t uc = src8[i];
          if (uc <= 0x7F)
            result = writer.writeByte(uc);
          else
            result = writer.write2Bytes(uc);

          if (BL_UNLIKELY(result != BL_SUCCESS)) {
            state.dstIndex = writer.index(static_cast<char*>(dst));
            state.srcIndex = i;
            break;
          }
        }
      }

      break;
    }

    // Utf8 <- Utf8
    // ------------

    case (BL_TEXT_ENCODING_UTF8 << 2) | BL_TEXT_ENCODING_UTF8: {
      size_t copySize = blMin(dstSizeInBytes, srcSizeInBytes);
      BLUnicodeValidationState validationState;

      result = blValidateUnicode(src, copySize, BL_TEXT_ENCODING_UTF8, validationState);
      size_t validatedSize = validationState.utf8Index;

      if (validatedSize)
        memcpy(dst, src, validatedSize);

      // Prevent `BL_ERROR_DATA_TRUNCATED` in case there is not enough space in destination.
      if (copySize < srcSizeInBytes && (result == BL_SUCCESS || result == BL_ERROR_DATA_TRUNCATED))
        result = blTraceError(BL_ERROR_NO_SPACE_LEFT);

      state.dstIndex = validatedSize;
      state.srcIndex = validatedSize;
      break;
    }

    // Utf8 <- Utf16
    // -------------

    case (BL_TEXT_ENCODING_UTF8 << 2) | BL_TEXT_ENCODING_UTF16:
      if (BLMemOps::kUnalignedMem16 || !BLIntOps::isAligned(src, 2))
        result = blConvertUnicodeImpl<BLUtf8Writer, BLUtf16Reader, BL_UNICODE_IO_STRICT | BL_UNICODE_IO_UNALIGNED>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = blConvertUnicodeImpl<BLUtf8Writer, BLUtf16Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;

    // Utf8 <- Utf32
    // -------------

    case (BL_TEXT_ENCODING_UTF8 << 2) | BL_TEXT_ENCODING_UTF32: {
      if (BLMemOps::kUnalignedMem32 || !BLIntOps::isAligned(src, 4))
        result = blConvertUnicodeImpl<BLUtf8Writer, BLUtf32Reader, BL_UNICODE_IO_STRICT | BL_UNICODE_IO_UNALIGNED>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = blConvertUnicodeImpl<BLUtf8Writer, BLUtf32Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // Utf16 <- Latin1
    // ---------------

    case (BL_TEXT_ENCODING_UTF16 << 2) | BL_TEXT_ENCODING_LATIN1: {
      size_t count = blMin(dstSizeInBytes / 2, srcSizeInBytes);

      if (BLMemOps::kUnalignedMem16 || BLIntOps::isAligned(dst, 2)) {
        for (size_t i = 0; i < count; i++)
          BLMemOps::writeU16aLE(static_cast<uint8_t*>(dst) + i * 2u, static_cast<const uint8_t*>(src)[i]);
      }
      else {
        for (size_t i = 0; i < count; i++)
          BLMemOps::writeU16uLE(static_cast<uint8_t*>(dst) + i * 2u, static_cast<const uint8_t*>(src)[i]);
      }

      if (count < srcSizeInBytes)
        result = blTraceError(BL_ERROR_NO_SPACE_LEFT);

      state.dstIndex = count * 2u;
      state.srcIndex = count;
      break;
    }

    // Utf16 <- Utf8
    // -------------

    case (BL_TEXT_ENCODING_UTF16 << 2) | BL_TEXT_ENCODING_UTF8: {
      if (BLMemOps::kUnalignedMem16 || !BLIntOps::isAligned(dst, 2))
        result = blConvertUnicodeImpl<BLUtf16Writer<BL_BYTE_ORDER_NATIVE, 1>, BLUtf8Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = blConvertUnicodeImpl<BLUtf16Writer<BL_BYTE_ORDER_NATIVE, 2>, BLUtf8Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // Utf16 <- Utf16
    // --------------

    case (BL_TEXT_ENCODING_UTF16 << 2) | BL_TEXT_ENCODING_UTF16: {
      size_t copySize = BLIntOps::alignDown(blMin(dstSizeInBytes, srcSizeInBytes), 2);
      BLUnicodeValidationState validationState;

      result = blValidateUnicode(src, copySize, BL_TEXT_ENCODING_UTF16, validationState);
      size_t validatedSize = validationState.utf16Index * 2;

      memmove(dst, src, validatedSize);

      // Prevent `BL_ERROR_DATA_TRUNCATED` in case there is not enough space in destination.
      if (copySize < srcSizeInBytes && (result == BL_SUCCESS || result == BL_ERROR_DATA_TRUNCATED))
        result = blTraceError(BL_ERROR_NO_SPACE_LEFT);

      // Report `BL_ERROR_DATA_TRUNCATED` is everything went right, but the
      // source size was not aligned to 2 bytes.
      if (result == BL_SUCCESS && !BLIntOps::isAligned(srcSizeInBytes, 2))
        result = blTraceError(BL_ERROR_DATA_TRUNCATED);

      state.dstIndex = validatedSize;
      state.srcIndex = validatedSize;
      break;
    }

    // Utf16 <- Utf32
    // --------------

    case (BL_TEXT_ENCODING_UTF16 << 2) | BL_TEXT_ENCODING_UTF32: {
      if (BL_UNALIGNED_IO_Any || !BLIntOps::isAligned(dst, 2) || !BLIntOps::isAligned(src, 4))
        result = blConvertUnicodeImpl<BLUtf16Writer<BL_BYTE_ORDER_NATIVE, 1>, BLUtf32Reader, BL_UNICODE_IO_STRICT | BL_UNICODE_IO_UNALIGNED>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = blConvertUnicodeImpl<BLUtf16Writer<BL_BYTE_ORDER_NATIVE, 2>, BLUtf32Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // Utf32 <- Latin1
    // ---------------

    case (BL_TEXT_ENCODING_UTF32 << 2) | BL_TEXT_ENCODING_LATIN1: {
      size_t count = blMin(dstSizeInBytes / 4, srcSizeInBytes);

      if (BLMemOps::kUnalignedMem32 || BLIntOps::isAligned(dst, 4)) {
        for (size_t i = 0; i < count; i++)
          BLMemOps::writeU32a(static_cast<uint8_t*>(dst) + i * 4u, static_cast<const uint8_t*>(src)[i]);
      }
      else {
        for (size_t i = 0; i < count; i++)
          BLMemOps::writeU32u(static_cast<uint8_t*>(dst) + i * 4u, static_cast<const uint8_t*>(src)[i]);
      }

      if (count < srcSizeInBytes)
        result = blTraceError(BL_ERROR_NO_SPACE_LEFT);

      state.dstIndex = count * 4u;
      state.srcIndex = count;
      break;
    }

    // Utf32 <- Utf8
    // -------------

    case (BL_TEXT_ENCODING_UTF32 << 2) | BL_TEXT_ENCODING_UTF8: {
      if (BLMemOps::kUnalignedMem32 || !BLIntOps::isAligned(dst, 4))
        result = blConvertUnicodeImpl<BLUtf32Writer<BL_BYTE_ORDER_NATIVE, 1>, BLUtf8Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = blConvertUnicodeImpl<BLUtf32Writer<BL_BYTE_ORDER_NATIVE, 4>, BLUtf8Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // Utf32 <- Utf16
    // --------------

    case (BL_TEXT_ENCODING_UTF32 << 2) | BL_TEXT_ENCODING_UTF16: {
      if (BL_UNALIGNED_IO_Any || !BLIntOps::isAligned(dst, 4) || !BLIntOps::isAligned(src, 2))
        result = blConvertUnicodeImpl<BLUtf32Writer<BL_BYTE_ORDER_NATIVE, 1>, BLUtf16Reader, BL_UNICODE_IO_STRICT | BL_UNICODE_IO_UNALIGNED>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = blConvertUnicodeImpl<BLUtf32Writer<BL_BYTE_ORDER_NATIVE, 4>, BLUtf16Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // Utf32 <- Utf32
    // --------------

    case (BL_TEXT_ENCODING_UTF32 << 2) | BL_TEXT_ENCODING_UTF32: {
      if (BLMemOps::kUnalignedMem32 || !BLIntOps::isAligned(dst, 4) || !BLIntOps::isAligned(src, 4))
        result = blConvertUnicodeImpl<BLUtf32Writer<BL_BYTE_ORDER_NATIVE, 1>, BLUtf32Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = blConvertUnicodeImpl<BLUtf32Writer<BL_BYTE_ORDER_NATIVE, 4>, BLUtf32Reader, BL_UNICODE_IO_STRICT | BL_UNICODE_IO_UNALIGNED>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // Invalid
    // -------

    default: {
      return blTraceError(BL_ERROR_INVALID_VALUE);
    }
  }

  return result;
}
