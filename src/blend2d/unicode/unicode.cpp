// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../unicode/unicode_p.h"
#include "../support/intops_p.h"
#include "../support/memops_p.h"

namespace bl {
namespace Unicode {

// bl::Unicode - Data
// ==================

// NOTE: Theoretically UTF-8 sequence can be extended to support sequences up
// to 6 bytes, however, since UCS-4 code-point's maximum value is 0x10FFFF it
// also limits the maximum length of a UTF-8 sequence to 4 bytes.
const uint8_t utf8SizeData[256] = {
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

// bl::Unicode - Validation
// ========================

// Not really anything to validate, we just want to calculate a corresponding UTf-8 size.
static BL_INLINE BLResult validateLatin1String(const char* data, size_t size, ValidationState& state) noexcept {
  size_t extra = 0;
  state.utf16Index = size;
  state.utf32Index = size;

  for (size_t i = 0; i < size; i++)
    extra += size_t(uint8_t(data[i])) >> 7;

  bl::OverflowFlag of{};
  size_t utf8Size = bl::IntOps::addOverflow(size, extra, &of);

  if (BL_UNLIKELY(of))
    return blTraceError(BL_ERROR_DATA_TOO_LARGE);

  state.utf8Index = utf8Size;
  return BL_SUCCESS;
}

template<typename Iterator, IOFlags kFlags>
static BL_INLINE BLResult validateUnicodeString(const void* data, size_t size, ValidationState& state) noexcept {
  Iterator it(data, size);
  BLResult result = it.template validate<kFlags | IOFlags::kCalcIndex>();
  state.utf8Index = it.utf8Index(data);
  state.utf16Index = it.utf16Index(data);
  state.utf32Index = it.utf32Index(data);
  return result;
}

BLResult blValidateUnicode(const void* data, size_t sizeInBytes, BLTextEncoding encoding, ValidationState& state) noexcept {
  BLResult result;
  state.reset();

  switch (encoding) {
    case BL_TEXT_ENCODING_LATIN1:
      return validateLatin1String(static_cast<const char*>(data), sizeInBytes, state);

    case BL_TEXT_ENCODING_UTF8:
      return validateUnicodeString<Utf8Reader, IOFlags::kStrict>(data, sizeInBytes, state);

    case BL_TEXT_ENCODING_UTF16:
      // This will make sure we won't compile specialized code for architectures that don't penalize unaligned reads.
      if (MemOps::kUnalignedMem16 || !IntOps::isAligned(data, 2))
        result = validateUnicodeString<Utf16Reader, IOFlags::kStrict | IOFlags::kUnaligned>(data, sizeInBytes, state);
      else
        result = validateUnicodeString<Utf16Reader, IOFlags::kStrict>(data, sizeInBytes, state);

      if (result == BL_SUCCESS && BL_UNLIKELY(sizeInBytes & 0x1))
        result = blTraceError(BL_ERROR_DATA_TRUNCATED);
      return result;

    case BL_TEXT_ENCODING_UTF32:
      // This will make sure we won't compile specialized code for architectures that don't penalize unaligned reads.
      if (MemOps::kUnalignedMem32 || !IntOps::isAligned(data, 4))
        result = validateUnicodeString<Utf32Reader, IOFlags::kStrict | IOFlags::kUnaligned>(data, sizeInBytes, state);
      else
        result = validateUnicodeString<Utf32Reader, IOFlags::kStrict>(data, sizeInBytes, state);

      if (result == BL_SUCCESS && BL_UNLIKELY(sizeInBytes & 0x3))
        result = blTraceError(BL_ERROR_DATA_TRUNCATED);
      return result;

    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }
}

// bl::Unicode - Conversion
// ========================

static BL_INLINE size_t offsetOfPtr(const void* base, const void* advanced) noexcept {
  return (size_t)(static_cast<const char*>(advanced) - static_cast<const char*>(base));
}

// A simple implementation. It iterates `src` char-by-char and writes it to the destination. The advantage of this
// implementation is that switching `Writer` and `Iterator` can customize strictness, endianness, etc, so we don't
// have to repeat the code for different variations of UTF16 and UTF32.
template<typename Writer, typename Iterator, IOFlags kFlags>
static BL_INLINE BLResult convertUnicodeImpl(void* dst, size_t dstSizeInBytes, const void* src, size_t srcSizeInBytes, ConversionState& state) noexcept {
  typedef typename Writer::CharType DstChar;

  Writer writer(static_cast<DstChar*>(dst), dstSizeInBytes / sizeof(DstChar));
  Iterator iter(src, IntOps::alignDown(srcSizeInBytes, Iterator::kCharSize));

  BLResult result = BL_SUCCESS;
  while (iter.hasNext()) {
    uint32_t uc;
    size_t ucSizeInBytes;

    result = iter.template next<kFlags>(uc, ucSizeInBytes);
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

  if (Iterator::kCharSize > 1 && result == BL_SUCCESS && !IntOps::isAligned(state.srcIndex, Iterator::kCharSize))
    return blTraceError(BL_ERROR_DATA_TRUNCATED);
  else
    return result;
}

BLResult convertUnicode(
  void* dst, size_t dstSizeInBytes, uint32_t dstEncoding,
  const void* src, size_t srcSizeInBytes, uint32_t srcEncoding,
  ConversionState& state) noexcept {

  constexpr bool kUnalignedAny = MemOps::kUnalignedMem16 && MemOps::kUnalignedMem32;

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
      Utf8Writer writer(static_cast<char*>(dst), dstSizeInBytes);
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
      ValidationState validationState;

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
      if (MemOps::kUnalignedMem16 || !IntOps::isAligned(src, 2))
        result = convertUnicodeImpl<Utf8Writer, Utf16Reader, IOFlags::kStrict | IOFlags::kUnaligned>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = convertUnicodeImpl<Utf8Writer, Utf16Reader, IOFlags::kStrict>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;

    // Utf8 <- Utf32
    // -------------

    case (BL_TEXT_ENCODING_UTF8 << 2) | BL_TEXT_ENCODING_UTF32: {
      if (MemOps::kUnalignedMem32 || !IntOps::isAligned(src, 4))
        result = convertUnicodeImpl<Utf8Writer, Utf32Reader, IOFlags::kStrict | IOFlags::kUnaligned>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = convertUnicodeImpl<Utf8Writer, Utf32Reader, IOFlags::kStrict>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // Utf16 <- Latin1
    // ---------------

    case (BL_TEXT_ENCODING_UTF16 << 2) | BL_TEXT_ENCODING_LATIN1: {
      size_t count = blMin(dstSizeInBytes / 2, srcSizeInBytes);

      if (MemOps::kUnalignedMem16 || IntOps::isAligned(dst, 2)) {
        for (size_t i = 0; i < count; i++)
          MemOps::writeU16aLE(static_cast<uint8_t*>(dst) + i * 2u, static_cast<const uint8_t*>(src)[i]);
      }
      else {
        for (size_t i = 0; i < count; i++)
          MemOps::writeU16uLE(static_cast<uint8_t*>(dst) + i * 2u, static_cast<const uint8_t*>(src)[i]);
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
      if (MemOps::kUnalignedMem16 || !IntOps::isAligned(dst, 2))
        result = convertUnicodeImpl<Utf16Writer<BL_BYTE_ORDER_NATIVE, 1>, Utf8Reader, IOFlags::kStrict>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = convertUnicodeImpl<Utf16Writer<BL_BYTE_ORDER_NATIVE, 2>, Utf8Reader, IOFlags::kStrict>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // Utf16 <- Utf16
    // --------------

    case (BL_TEXT_ENCODING_UTF16 << 2) | BL_TEXT_ENCODING_UTF16: {
      size_t copySize = IntOps::alignDown(blMin(dstSizeInBytes, srcSizeInBytes), 2);
      ValidationState validationState;

      result = blValidateUnicode(src, copySize, BL_TEXT_ENCODING_UTF16, validationState);
      size_t validatedSize = validationState.utf16Index * 2;

      memmove(dst, src, validatedSize);

      // Prevent `BL_ERROR_DATA_TRUNCATED` in case there is not enough space in destination.
      if (copySize < srcSizeInBytes && (result == BL_SUCCESS || result == BL_ERROR_DATA_TRUNCATED))
        result = blTraceError(BL_ERROR_NO_SPACE_LEFT);

      // Report `BL_ERROR_DATA_TRUNCATED` is everything went right, but the
      // source size was not aligned to 2 bytes.
      if (result == BL_SUCCESS && !IntOps::isAligned(srcSizeInBytes, 2))
        result = blTraceError(BL_ERROR_DATA_TRUNCATED);

      state.dstIndex = validatedSize;
      state.srcIndex = validatedSize;
      break;
    }

    // Utf16 <- Utf32
    // --------------

    case (BL_TEXT_ENCODING_UTF16 << 2) | BL_TEXT_ENCODING_UTF32: {
      if (kUnalignedAny || !IntOps::isAligned(dst, 2) || !IntOps::isAligned(src, 4))
        result = convertUnicodeImpl<Utf16Writer<BL_BYTE_ORDER_NATIVE, 1>, Utf32Reader, IOFlags::kStrict | IOFlags::kUnaligned>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = convertUnicodeImpl<Utf16Writer<BL_BYTE_ORDER_NATIVE, 2>, Utf32Reader, IOFlags::kStrict>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // Utf32 <- Latin1
    // ---------------

    case (BL_TEXT_ENCODING_UTF32 << 2) | BL_TEXT_ENCODING_LATIN1: {
      size_t count = blMin(dstSizeInBytes / 4, srcSizeInBytes);

      if (MemOps::kUnalignedMem32 || IntOps::isAligned(dst, 4)) {
        for (size_t i = 0; i < count; i++)
          MemOps::writeU32a(static_cast<uint8_t*>(dst) + i * 4u, static_cast<const uint8_t*>(src)[i]);
      }
      else {
        for (size_t i = 0; i < count; i++)
          MemOps::writeU32u(static_cast<uint8_t*>(dst) + i * 4u, static_cast<const uint8_t*>(src)[i]);
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
      if (MemOps::kUnalignedMem32 || !IntOps::isAligned(dst, 4))
        result = convertUnicodeImpl<Utf32Writer<BL_BYTE_ORDER_NATIVE, 1>, Utf8Reader, IOFlags::kStrict>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = convertUnicodeImpl<Utf32Writer<BL_BYTE_ORDER_NATIVE, 4>, Utf8Reader, IOFlags::kStrict>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // Utf32 <- Utf16
    // --------------

    case (BL_TEXT_ENCODING_UTF32 << 2) | BL_TEXT_ENCODING_UTF16: {
      if (kUnalignedAny || !IntOps::isAligned(dst, 4) || !IntOps::isAligned(src, 2))
        result = convertUnicodeImpl<Utf32Writer<BL_BYTE_ORDER_NATIVE, 1>, Utf16Reader, IOFlags::kStrict | IOFlags::kUnaligned>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = convertUnicodeImpl<Utf32Writer<BL_BYTE_ORDER_NATIVE, 4>, Utf16Reader, IOFlags::kStrict>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // Utf32 <- Utf32
    // --------------

    case (BL_TEXT_ENCODING_UTF32 << 2) | BL_TEXT_ENCODING_UTF32: {
      if (MemOps::kUnalignedMem32 || !IntOps::isAligned(dst, 4) || !IntOps::isAligned(src, 4))
        result = convertUnicodeImpl<Utf32Writer<BL_BYTE_ORDER_NATIVE, 1>, Utf32Reader, IOFlags::kStrict>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = convertUnicodeImpl<Utf32Writer<BL_BYTE_ORDER_NATIVE, 4>, Utf32Reader, IOFlags::kStrict | IOFlags::kUnaligned>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
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

} // {Unicode}
} // {bl}
