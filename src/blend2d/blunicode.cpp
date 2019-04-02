// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blsupport_p.h"
#include "./blunicode_p.h"

// ============================================================================
// [Unicode Data]
// ============================================================================

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

// ============================================================================
// [BLUnicode - Validation]
// ============================================================================

// Not really anything to validate, we just want to calculate a corresponding UTf-8 size.
static BL_INLINE BLResult validateLatin1String(const char* data, size_t size, BLUnicodeValidationState& state) noexcept {
  size_t extra = 0;
  state.utf16Index = size;
  state.utf32Index = size;

  for (size_t i = 0; i < size; i++)
    extra += size_t(uint8_t(data[i])) >> 7;

  BLOverflowFlag of = 0;
  size_t utf8Size = blAddOverflow(size, extra, &of);

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

BLResult blValidateUnicode(const void* data, size_t sizeInBytes, uint32_t encoding, BLUnicodeValidationState& state) noexcept {
  BLResult result;
  state.reset();

  switch (encoding) {
    case BL_TEXT_ENCODING_LATIN1:
      return validateLatin1String(static_cast<const char*>(data), sizeInBytes, state);

    case BL_TEXT_ENCODING_UTF8:
      return validateUnicodeString<BLUtf8Reader, BL_UNICODE_IO_STRICT>(data, sizeInBytes, state);

    case BL_TEXT_ENCODING_UTF16:
      // This will make sure we won't compile specialized code for architectures that don't penalize unaligned reads.
      if (BL_UNALIGNED_IO_16 || !blIsAligned(data, 2))
        result = validateUnicodeString<BLUtf16Reader, BL_UNICODE_IO_STRICT | BL_UNICODE_IO_UNALIGNED>(data, sizeInBytes, state);
      else
        result = validateUnicodeString<BLUtf16Reader, BL_UNICODE_IO_STRICT>(data, sizeInBytes, state);

      if (result == BL_SUCCESS && BL_UNLIKELY(sizeInBytes & 0x1))
        result = blTraceError(BL_ERROR_DATA_TRUNCATED);
      return result;

    case BL_TEXT_ENCODING_UTF32:
      // This will make sure we won't compile specialized code for architectures that don't penalize unaligned reads.
      if (BL_UNALIGNED_IO_32 || !blIsAligned(data, 4))
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

// ============================================================================
// [BLUnicode - Conversion]
// ============================================================================

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
  Iterator iter(src, blAlignDown(srcSizeInBytes, Iterator::kCharSize));

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

  if (Iterator::kCharSize > 1 && result == BL_SUCCESS && !blIsAligned(state.srcIndex, Iterator::kCharSize))
    return blTraceError(BL_ERROR_DATA_TRUNCATED);
  else
    return result;
}

BLResult blConvertUnicode(
  void* dst, size_t dstSizeInBytes, uint32_t dstEncoding,
  const void* src, size_t srcSizeInBytes, uint32_t srcEncoding,
  BLUnicodeConversionState& state) noexcept {

  constexpr bool BL_UNALIGNED_IO_Any = BL_UNALIGNED_IO_16 && BL_UNALIGNED_IO_32;

  BLResult result = BL_SUCCESS;
  state.reset();

  uint32_t encodingCombined = (dstEncoding << 2) | srcEncoding;
  switch (encodingCombined) {
    // ------------------------------------------------------------------------
    // [MemCpy]
    // ------------------------------------------------------------------------

    case (BL_TEXT_ENCODING_LATIN1 << 2) | BL_TEXT_ENCODING_LATIN1: {
      size_t copySize = blMin(dstSizeInBytes, srcSizeInBytes);
      memcpy(dst, src, copySize);

      state.dstIndex = copySize;
      state.srcIndex = copySize;

      if (dstSizeInBytes < srcSizeInBytes)
        result = blTraceError(BL_ERROR_NO_SPACE_LEFT);
      break;
    }

    // ------------------------------------------------------------------------
    // [Utf8 <- Latin1]
    // ------------------------------------------------------------------------

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

    // ------------------------------------------------------------------------
    // [Utf8 <- Utf8]
    // ------------------------------------------------------------------------

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

    // ------------------------------------------------------------------------
    // [Utf8 <- Utf16]
    // ------------------------------------------------------------------------

    case (BL_TEXT_ENCODING_UTF8 << 2) | BL_TEXT_ENCODING_UTF16:
      if (BL_UNALIGNED_IO_16 || !blIsAligned(src, 2))
        result = blConvertUnicodeImpl<BLUtf8Writer, BLUtf16Reader, BL_UNICODE_IO_STRICT | BL_UNICODE_IO_UNALIGNED>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = blConvertUnicodeImpl<BLUtf8Writer, BLUtf16Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;

    // ------------------------------------------------------------------------
    // [Utf8 <- Utf32]
    // ------------------------------------------------------------------------

    case (BL_TEXT_ENCODING_UTF8 << 2) | BL_TEXT_ENCODING_UTF32: {
      if (BL_UNALIGNED_IO_32 || !blIsAligned(src, 4))
        result = blConvertUnicodeImpl<BLUtf8Writer, BLUtf32Reader, BL_UNICODE_IO_STRICT | BL_UNICODE_IO_UNALIGNED>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = blConvertUnicodeImpl<BLUtf8Writer, BLUtf32Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // ------------------------------------------------------------------------
    // [Utf16 <- Latin1]
    // ------------------------------------------------------------------------

    case (BL_TEXT_ENCODING_UTF16 << 2) | BL_TEXT_ENCODING_LATIN1: {
      size_t count = blMin(dstSizeInBytes / 2, srcSizeInBytes);

      if (BL_UNALIGNED_IO_16 || blIsAligned(dst, 2)) {
        for (size_t i = 0; i < count; i++)
          blMemWriteU16aLE(static_cast<uint8_t*>(dst) + i * 2u, static_cast<const uint8_t*>(src)[i]);
      }
      else {
        for (size_t i = 0; i < count; i++)
          blMemWriteU16uLE(static_cast<uint8_t*>(dst) + i * 2u, static_cast<const uint8_t*>(src)[i]);
      }

      if (count < srcSizeInBytes)
        result = blTraceError(BL_ERROR_NO_SPACE_LEFT);

      state.dstIndex = count * 2u;
      state.srcIndex = count;
      break;
    }

    // ------------------------------------------------------------------------
    // [Utf16 <- Utf8]
    // ------------------------------------------------------------------------

    case (BL_TEXT_ENCODING_UTF16 << 2) | BL_TEXT_ENCODING_UTF8: {
      if (BL_UNALIGNED_IO_16 || !blIsAligned(dst, 2))
        result = blConvertUnicodeImpl<BLUtf16Writer<BL_BYTE_ORDER_NATIVE, 1>, BLUtf8Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = blConvertUnicodeImpl<BLUtf16Writer<BL_BYTE_ORDER_NATIVE, 2>, BLUtf8Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // ------------------------------------------------------------------------
    // [Utf16 <- Utf16]
    // ------------------------------------------------------------------------

    case (BL_TEXT_ENCODING_UTF16 << 2) | BL_TEXT_ENCODING_UTF16: {
      size_t copySize = blAlignDown(blMin(dstSizeInBytes, srcSizeInBytes), 2);
      BLUnicodeValidationState validationState;

      result = blValidateUnicode(src, copySize, BL_TEXT_ENCODING_UTF16, validationState);
      size_t validatedSize = validationState.utf16Index * 2;

      memmove(dst, src, validatedSize);

      // Prevent `BL_ERROR_DATA_TRUNCATED` in case there is not enough space in destination.
      if (copySize < srcSizeInBytes && (result == BL_SUCCESS || result == BL_ERROR_DATA_TRUNCATED))
        result = blTraceError(BL_ERROR_NO_SPACE_LEFT);

      // Report `BL_ERROR_DATA_TRUNCATED` is everything went right, but the
      // source size was not aligned to 2 bytes.
      if (result == BL_SUCCESS && !blIsAligned(srcSizeInBytes, 2))
        result = blTraceError(BL_ERROR_DATA_TRUNCATED);

      state.dstIndex = validatedSize;
      state.srcIndex = validatedSize;
      break;
    }

    // ------------------------------------------------------------------------
    // [Utf16 <- Utf32]
    // ------------------------------------------------------------------------

    case (BL_TEXT_ENCODING_UTF16 << 2) | BL_TEXT_ENCODING_UTF32: {
      if (BL_UNALIGNED_IO_Any || !blIsAligned(dst, 2) || !blIsAligned(src, 4))
        result = blConvertUnicodeImpl<BLUtf16Writer<BL_BYTE_ORDER_NATIVE, 1>, BLUtf32Reader, BL_UNICODE_IO_STRICT | BL_UNICODE_IO_UNALIGNED>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = blConvertUnicodeImpl<BLUtf16Writer<BL_BYTE_ORDER_NATIVE, 2>, BLUtf32Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // ------------------------------------------------------------------------
    // [Utf32 <- Latin1]
    // ------------------------------------------------------------------------

    case (BL_TEXT_ENCODING_UTF32 << 2) | BL_TEXT_ENCODING_LATIN1: {
      size_t count = blMin(dstSizeInBytes / 4, srcSizeInBytes);

      if (BL_UNALIGNED_IO_32 || blIsAligned(dst, 4)) {
        for (size_t i = 0; i < count; i++)
          blMemWriteU32a(static_cast<uint8_t*>(dst) + i * 4u, static_cast<const uint8_t*>(src)[i]);
      }
      else {
        for (size_t i = 0; i < count; i++)
          blMemWriteU32u(static_cast<uint8_t*>(dst) + i * 4u, static_cast<const uint8_t*>(src)[i]);
      }

      if (count < srcSizeInBytes)
        result = blTraceError(BL_ERROR_NO_SPACE_LEFT);

      state.dstIndex = count * 4u;
      state.srcIndex = count;
      break;
    }

    // ------------------------------------------------------------------------
    // [Utf32 <- Utf8]
    // ------------------------------------------------------------------------

    case (BL_TEXT_ENCODING_UTF32 << 2) | BL_TEXT_ENCODING_UTF8: {
      if (BL_UNALIGNED_IO_32 || !blIsAligned(dst, 4))
        result = blConvertUnicodeImpl<BLUtf32Writer<BL_BYTE_ORDER_NATIVE, 1>, BLUtf8Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = blConvertUnicodeImpl<BLUtf32Writer<BL_BYTE_ORDER_NATIVE, 4>, BLUtf8Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // ------------------------------------------------------------------------
    // [Utf32 <- Utf16]
    // ------------------------------------------------------------------------

    case (BL_TEXT_ENCODING_UTF32 << 2) | BL_TEXT_ENCODING_UTF16: {
      if (BL_UNALIGNED_IO_Any || !blIsAligned(dst, 4) || !blIsAligned(src, 2))
        result = blConvertUnicodeImpl<BLUtf32Writer<BL_BYTE_ORDER_NATIVE, 1>, BLUtf16Reader, BL_UNICODE_IO_STRICT | BL_UNICODE_IO_UNALIGNED>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = blConvertUnicodeImpl<BLUtf32Writer<BL_BYTE_ORDER_NATIVE, 4>, BLUtf16Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // ------------------------------------------------------------------------
    // [Utf32 <- Utf32]
    // ------------------------------------------------------------------------

    case (BL_TEXT_ENCODING_UTF32 << 2) | BL_TEXT_ENCODING_UTF32: {
      if (BL_UNALIGNED_IO_32 || !blIsAligned(dst, 4) || !blIsAligned(src, 4))
        result = blConvertUnicodeImpl<BLUtf32Writer<BL_BYTE_ORDER_NATIVE, 1>, BLUtf32Reader, BL_UNICODE_IO_STRICT>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      else
        result = blConvertUnicodeImpl<BLUtf32Writer<BL_BYTE_ORDER_NATIVE, 4>, BLUtf32Reader, BL_UNICODE_IO_STRICT | BL_UNICODE_IO_UNALIGNED>(dst, dstSizeInBytes, src, srcSizeInBytes, state);
      break;
    }

    // ------------------------------------------------------------------------
    // [Invalid]
    // ------------------------------------------------------------------------

    default: {
      return blTraceError(BL_ERROR_INVALID_VALUE);
    }
  }

  return result;
}

// ============================================================================
// [BLUnicode - Test]
// ============================================================================

#ifdef BL_BUILD_TEST
UNIT(blend2d_unicode) {
  struct TestEntry {
    char dst[28];
    char src[28];
    uint8_t dstSize;
    uint8_t srcSize;
    uint8_t dstEncoding;
    uint8_t srcEncoding;
    BLResult result;
  };

  static const TestEntry testEntries[] = {
    #define ENTRY(DST, DST_ENC, SRC, SRC_ENC, ERROR_CODE) { \
      DST,                                                  \
      SRC,                                                  \
      uint8_t(sizeof(DST) - 1),                             \
      uint8_t(sizeof(SRC) - 1),                             \
      uint8_t(BL_TEXT_ENCODING_##DST_ENC),                  \
      uint8_t(BL_TEXT_ENCODING_##SRC_ENC),                  \
      ERROR_CODE                                            \
    }

    ENTRY("Test"                            , LATIN1, "Test"                            , LATIN1, BL_SUCCESS),
    ENTRY("Test"                            , UTF8  , "Test"                            , LATIN1, BL_SUCCESS),
    ENTRY("Test"                            , UTF8  , "Test"                            , UTF8  , BL_SUCCESS),
    #if BL_BUILD_BYTE_ORDER == 1234
    ENTRY("Test"                            , UTF8  , "T\0e\0s\0t\0"                    , UTF16 , BL_SUCCESS),
    ENTRY("T\0e\0s\0t\0"                    , UTF16 , "Test"                            , UTF8  , BL_SUCCESS),
    #else
    ENTRY("Test"                            , UTF8  , "\0T\0e\0s\0t"                    , UTF16 , BL_SUCCESS),
    ENTRY("\0T\0e\0s\0t"                    , UTF16  , "Test"                           , UTF8  , BL_SUCCESS),
    #endif

    // Tests a Czech word (Rain in english) with diacritic marks, at most 2 BYTEs per character.
    #if BL_BUILD_BYTE_ORDER == 1234
    ENTRY("\x44\xC3\xA9\xC5\xA1\xC5\xA5"    , UTF8  , "\x44\x00\xE9\x00\x61\x01\x65\x01", UTF16 , BL_SUCCESS),
    ENTRY("\x44\x00\xE9\x00\x61\x01\x65\x01", UTF16 , "\x44\xC3\xA9\xC5\xA1\xC5\xA5"    , UTF8  , BL_SUCCESS),
    #else
    ENTRY("\x44\xC3\xA9\xC5\xA1\xC5\xA5"    , UTF8  , "\x00\x44\x00\xE9\x01\x61\x01\x65", UTF16 , BL_SUCCESS),
    ENTRY("\x00\x44\x00\xE9\x01\x61\x01\x65", UTF16 , "\x44\xC3\xA9\xC5\xA1\xC5\xA5"    , UTF8  , BL_SUCCESS),
    #endif

    // Tests full-width digit zero (3 BYTEs per UTF-8 character).
    #if BL_BUILD_BYTE_ORDER == 1234
    ENTRY("\xEF\xBC\x90"                    , UTF8  , "\x10\xFF"                        , UTF16 , BL_SUCCESS),
    ENTRY("\x10\xFF"                        , UTF16 , "\xEF\xBC\x90"                    , UTF8  , BL_SUCCESS),
    #else
    ENTRY("\xEF\xBC\x90"                    , UTF8  , "\xFF\x10"                        , UTF16 , BL_SUCCESS),
    ENTRY("\xFF\x10"                        , UTF16 , "\xEF\xBC\x90"                    , UTF8  , BL_SUCCESS),
    #endif

    // Tests `BL_CHAR_MAX` character (4 BYTEs per UTF-8 character, the highest possible unicode code-point).
    #if BL_BUILD_BYTE_ORDER == 1234
    ENTRY("\xF4\x8F\xBF\xBF"                , UTF8  , "\xFF\xDB\xFF\xDF"                , UTF16 , BL_SUCCESS),
    ENTRY("\xFF\xDB\xFF\xDF"                , UTF16 , "\xF4\x8F\xBF\xBF"                , UTF8  , BL_SUCCESS),
    #else
    ENTRY("\xF4\x8F\xBF\xBF"                , UTF8  , "\xDB\xFF\xDF\xFF"                , UTF16 , BL_SUCCESS),
    ENTRY("\xDB\xFF\xDF\xFF"                , UTF16 , "\xF4\x8F\xBF\xBF"                , UTF8  , BL_SUCCESS),
    #endif

    #if BL_BUILD_BYTE_ORDER == 1234
    ENTRY("Test"                            , UTF8  , "T\0\0\0e\0\0\0s\0\0\0t\0\0\0"    , UTF32 , BL_SUCCESS),
    ENTRY("T\0e\0s\0t\0"                    , UTF16 , "T\0\0\0e\0\0\0s\0\0\0t\0\0\0"    , UTF32 , BL_SUCCESS),
    ENTRY("T\0\0\0e\0\0\0s\0\0\0t\0\0\0"    , UTF32 , "T\0\0\0e\0\0\0s\0\0\0t\0\0\0"    , UTF32 , BL_SUCCESS),
    ENTRY("T\0\0\0e\0\0\0s\0\0\0t\0\0\0"    , UTF32 , "T\0e\0s\0t\0"                    , UTF16 , BL_SUCCESS),
    ENTRY("T\0\0\0e\0\0\0s\0\0\0t\0\0\0"    , UTF32 , "Test"                            , LATIN1, BL_SUCCESS),
    ENTRY("T\0\0\0e\0\0\0s\0\0\0t\0\0\0"    , UTF32 , "Test"                            , UTF8  , BL_SUCCESS),
    #else
    ENTRY("Test"                            , UTF8  , "\0\0\0T\0\0\0e\0\0\0s\0\0\0t"    , UTF32 , BL_SUCCESS),
    ENTRY("\0T\0e\0s\0t"                    , UTF16 , "\0\0\0T\0\0\0e\0\0\0s\0\0\0t"    , UTF32 , BL_SUCCESS),
    ENTRY("\0\0\0T\0\0\0e\0\0\0s\0\0\0t"    , UTF32 , "\0\0\0T\0\0\0e\0\0\0s\0\0\0t"    , UTF32 , BL_SUCCESS),
    ENTRY("\0\0\0T\0\0\0e\0\0\0s\0\0\0t"    , UTF32 , "\0T\0e\0s\0t"                    , UTF16 , BL_SUCCESS)
    ENTRY("\0\0\0T\0\0\0e\0\0\0s\0\0\0t"    , UTF32 , "Test"                            , LATIN1, BL_SUCCESS),
    ENTRY("\0\0\0T\0\0\0e\0\0\0s\0\0\0t"    , UTF32 , "Test"                            , UTF8  , BL_SUCCESS),
    #endif

    // Truncated characters.
    ENTRY(""                                , UTF8  , "\xC5"                            , UTF8  , BL_ERROR_DATA_TRUNCATED),
    ENTRY(""                                , UTF8  , "\xEF"                            , UTF8  , BL_ERROR_DATA_TRUNCATED),
    ENTRY(""                                , UTF8  , "\xEF\xBC"                        , UTF8  , BL_ERROR_DATA_TRUNCATED),
    ENTRY(""                                , UTF8  , "\xF4"                            , UTF8  , BL_ERROR_DATA_TRUNCATED),
    ENTRY(""                                , UTF8  , "\xF4\x8F"                        , UTF8  , BL_ERROR_DATA_TRUNCATED),
    ENTRY(""                                , UTF8  , "\xF4\x8F\xBF"                    , UTF8  , BL_ERROR_DATA_TRUNCATED),

    // Truncated character at the end (the converter must output the content, which was correct).
    ENTRY("a"                               , UTF8  , "a\xF4\x8F\xBF"                   , UTF8  , BL_ERROR_DATA_TRUNCATED),
    ENTRY("ab"                              , UTF8  , "ab\xF4\x8F\xBF"                  , UTF8  , BL_ERROR_DATA_TRUNCATED),
    ENTRY("TestString"                      , UTF8  , "TestString\xC5"                  , UTF8  , BL_ERROR_DATA_TRUNCATED),
    #if BL_BUILD_BYTE_ORDER == 1234
    ENTRY("T\0e\0s\0t\0S\0t\0r\0i\0n\0g\0"  , UTF16 , "TestString\xC5"                  , UTF8  , BL_ERROR_DATA_TRUNCATED),
    #else
    ENTRY("\0T\0e\0s\0t\0S\0t\0r\0i\0n\0g"  , UTF16 , "TestString\xC5"                  , UTF8  , BL_ERROR_DATA_TRUNCATED),
    #endif

    // Invalid UTf-8 characters.
    ENTRY(""                                , UTF8  , "\x80"                            , UTF8  , BL_ERROR_INVALID_STRING),
    ENTRY(""                                , UTF8  , "\xC1"                            , UTF8  , BL_ERROR_INVALID_STRING),
    ENTRY(""                                , UTF8  , "\xF5\x8F\xBF\xBF"                , UTF8  , BL_ERROR_INVALID_STRING),
    ENTRY(""                                , UTF8  , "\x91\x8F\xBF\xBF"                , UTF8  , BL_ERROR_INVALID_STRING),
    ENTRY(""                                , UTF8  , "\xF6\x8F\xBF\xBF"                , UTF8  , BL_ERROR_INVALID_STRING),
    ENTRY(""                                , UTF8  , "\xF4\xFF\xBF\xBF"                , UTF8  , BL_ERROR_INVALID_STRING),

    // Overlong UTF-8 characters.
    ENTRY(""                                , UTF8  , "\xC0\xA0"                        , UTF8  , BL_ERROR_INVALID_STRING)

    #undef ENTRY
  };

  for (size_t i = 0; i < BL_ARRAY_SIZE(testEntries); i++) {
    const TestEntry& entry = testEntries[i];
    char output[32];

    BLUnicodeConversionState state;
    BLResult result = blConvertUnicode(output, 32, entry.dstEncoding, entry.src, entry.srcSize, entry.srcEncoding, state);

    bool failed = (result != entry.result) ||
                  (state.dstIndex != entry.dstSize) ||
                  (memcmp(output, entry.dst, state.dstIndex) != 0);

    if (failed) {
      size_t inputSize = entry.srcSize;
      size_t outputSize = state.dstIndex;
      size_t expectedSize = entry.dstSize;

      printf("  Failed Entry #%u\n", unsigned(i));

      printf("    Input   :");
      for (size_t j = 0; j < inputSize; j++)
        printf(" %02X", uint8_t(entry.src[j]));
      printf("%s\n", inputSize ? "" : " (Nothing)");

      printf("    Output  :");
      for (size_t j = 0; j < outputSize; j++)
        printf(" %02X", uint8_t(output[j]));
      printf("%s\n", outputSize ? "" : " (Nothing)");

      printf("    Expected:");
      for (size_t j = 0; j < expectedSize; j++)
        printf(" %02X", uint8_t(entry.dst[j]));
      printf("%s\n", expectedSize ? "" : " (Nothing)");
      printf("    ErrorCode: Actual(%u) %s Expected(%u)\n", result, (result == entry.result) ? "==" : "!=", entry.result);
    }

    EXPECT(!failed);
  }
}

UNIT(blend2d_unicode_io) {
  INFO("BLUtf8Reader");
  {
    const uint8_t data[] = {
      0xE2, 0x82, 0xAC,      // U+0020AC
      0xF0, 0x90, 0x8D, 0x88 // U+010348
    };

    BLUtf8Reader it(data, BL_ARRAY_SIZE(data));
    uint32_t uc;

    EXPECT(it.hasNext());
    EXPECT(it.next<BL_UNICODE_IO_CALC_INDEX>(uc) == BL_SUCCESS);
    EXPECT(uc == 0x0020AC);

    EXPECT(it.hasNext());
    EXPECT(it.next<BL_UNICODE_IO_CALC_INDEX>(uc) == BL_SUCCESS);
    EXPECT(uc == 0x010348);

    EXPECT(!it.hasNext());

    // Verify that sizes were calculated correctly.
    EXPECT(it.byteIndex(data) == 7);
    EXPECT(it.utf8Index(data) == 7);
    EXPECT(it.utf16Index(data) == 3); // 3 code-points (1 BMP and 1 SMP).
    EXPECT(it.utf32Index(data) == 2); // 2 code-points.

    const uint8_t invalidData[] = { 0xE2, 0x82 };
    it.reset(invalidData, BL_ARRAY_SIZE(invalidData));

    EXPECT(it.hasNext());
    EXPECT(it.next(uc) == BL_ERROR_DATA_TRUNCATED);

    // After error the iterator should not move.
    EXPECT(it.hasNext());
    EXPECT(it.byteIndex(invalidData) == 0);
    EXPECT(it.utf8Index(invalidData) == 0);
    EXPECT(it.utf16Index(invalidData) == 0);
    EXPECT(it.utf32Index(invalidData) == 0);
  }

  INFO("BLUtf16Reader");
  {
    const uint16_t data[] = {
      0x20AC,                // U+0020AC
      0xD800, 0xDF48         // U+010348
    };

    BLUtf16Reader it(data, BL_ARRAY_SIZE(data) * sizeof(uint16_t));
    uint32_t uc;

    EXPECT(it.hasNext());
    EXPECT(it.next<BL_UNICODE_IO_CALC_INDEX>(uc) == BL_SUCCESS);
    EXPECT(uc == 0x0020AC);

    EXPECT(it.hasNext());
    EXPECT(it.next<BL_UNICODE_IO_CALC_INDEX>(uc) == BL_SUCCESS);
    EXPECT(uc == 0x010348);

    EXPECT(!it.hasNext());

    // Verify that sizes were calculated correctly.
    EXPECT(it.byteIndex(data) == 6);
    EXPECT(it.utf8Index(data) == 7);
    EXPECT(it.utf16Index(data) == 3); // 3 code-points (1 BMP and 1 SMP).
    EXPECT(it.utf32Index(data) == 2); // 2 code-points.

    const uint16_t invalidData[] = { 0xD800 };
    it.reset(invalidData, BL_ARRAY_SIZE(invalidData) * sizeof(uint16_t));

    EXPECT(it.hasNext());
    EXPECT(it.next<BL_UNICODE_IO_CALC_INDEX | BL_UNICODE_IO_STRICT>(uc) == BL_ERROR_DATA_TRUNCATED);

    // After an error the iterator should not move.
    EXPECT(it.hasNext());
    EXPECT(it.byteIndex(invalidData) == 0);
    EXPECT(it.utf8Index(invalidData) == 0);
    EXPECT(it.utf16Index(invalidData) == 0);
    EXPECT(it.utf32Index(invalidData) == 0);

    // However, this should pass in non-strict mode.
    EXPECT(it.next(uc) == BL_SUCCESS);
    EXPECT(!it.hasNext());
  }

  INFO("BLUtf32Reader");
  {
    const uint32_t data[] = {
      0x0020AC,
      0x010348
    };

    BLUtf32Reader it(data, BL_ARRAY_SIZE(data) * sizeof(uint32_t));
    uint32_t uc;

    EXPECT(it.hasNext());
    EXPECT(it.next<BL_UNICODE_IO_CALC_INDEX>(uc) == BL_SUCCESS);
    EXPECT(uc == 0x0020AC);

    EXPECT(it.hasNext());
    EXPECT(it.next<BL_UNICODE_IO_CALC_INDEX>(uc) == BL_SUCCESS);
    EXPECT(uc == 0x010348);

    EXPECT(!it.hasNext());

    // Verify that sizes were calculated correctly.
    EXPECT(it.byteIndex(data) == 8);
    EXPECT(it.utf8Index(data) == 7);
    EXPECT(it.utf16Index(data) == 3); // 3 code-points (1 BMP and 1 SMP).
    EXPECT(it.utf32Index(data) == 2); // 2 code-points.

    const uint32_t invalidData[] = { 0xD800 };
    it.reset(invalidData, BL_ARRAY_SIZE(invalidData) * sizeof(uint32_t));

    EXPECT(it.hasNext());
    EXPECT(it.next<BL_UNICODE_IO_CALC_INDEX | BL_UNICODE_IO_STRICT>(uc) == BL_ERROR_INVALID_STRING);

    // After an error the iterator should not move.
    EXPECT(it.hasNext());
    EXPECT(it.byteIndex(invalidData) == 0);
    EXPECT(it.utf8Index(invalidData) == 0);
    EXPECT(it.utf16Index(invalidData) == 0);
    EXPECT(it.utf32Index(invalidData) == 0);

    // However, this should pass in non-strict mode.
    EXPECT(it.next(uc) == BL_SUCCESS);
    EXPECT(!it.hasNext());
  }

  INFO("BLUtf8Writer");
  {
    char dst[7];
    BLUtf8Writer writer(dst, BL_ARRAY_SIZE(dst));

    EXPECT(writer.write(0x20ACu) == BL_SUCCESS);
    EXPECT(uint8_t(dst[0]) == 0xE2);
    EXPECT(uint8_t(dst[1]) == 0x82);
    EXPECT(uint8_t(dst[2]) == 0xAC);

    EXPECT(writer.write(0x010348u) == BL_SUCCESS);
    EXPECT(uint8_t(dst[3]) == 0xF0);
    EXPECT(uint8_t(dst[4]) == 0x90);
    EXPECT(uint8_t(dst[5]) == 0x8D);
    EXPECT(uint8_t(dst[6]) == 0x88);
    EXPECT(writer.atEnd());

    writer.reset(dst, 1);
    EXPECT(writer.write(0x20ACu) == BL_ERROR_NO_SPACE_LEFT);
    EXPECT(writer.write(0x0080u) == BL_ERROR_NO_SPACE_LEFT);
    EXPECT(writer.write(0x00C1u) == BL_ERROR_NO_SPACE_LEFT);

    // We have only one byte left so this must pass...
    EXPECT(writer.write('a') == BL_SUCCESS);
    EXPECT(writer.atEnd());

    writer.reset(dst, 2);
    EXPECT(writer.write(0x20ACu) == BL_ERROR_NO_SPACE_LEFT);
    EXPECT(writer.write(0x00C1u) == BL_SUCCESS);
    EXPECT(dst[0] == char(0xC3));
    EXPECT(dst[1] == char(0x81));
    EXPECT(writer.atEnd());
    EXPECT(writer.write('a') == BL_ERROR_NO_SPACE_LEFT);
  }

  INFO("BLUtf16Writer");
  {
    uint16_t dst[3];
    BLUtf16Writer<> writer(dst, BL_ARRAY_SIZE(dst));

    EXPECT(writer.write(0x010348u) == BL_SUCCESS);
    EXPECT(dst[0] == 0xD800u);
    EXPECT(dst[1] == 0xDF48u);

    EXPECT(writer.write(0x010348u) == BL_ERROR_NO_SPACE_LEFT);
    EXPECT(writer.write(0x20ACu) == BL_SUCCESS);
    EXPECT(dst[2] == 0x20ACu);
    EXPECT(writer.atEnd());
  }
}
#endif
