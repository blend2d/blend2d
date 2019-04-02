// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../blbitarray_p.h"
#include "../bltables_p.h"
#include "../blsupport_p.h"
#include "../blunicode_p.h"
#include "../bltrace_p.h"
#include "../opentype/blotface_p.h"
#include "../opentype/blotname_p.h"
#include "../opentype/blotplatform_p.h"

namespace BLOpenType {
namespace NameImpl {

// ============================================================================
// [BLOpenType::NameImpl - Tracing]
// ============================================================================

#if defined(BL_TRACE_OT_ALL) || defined(BL_TRACE_OT_NAME)
  #define Trace BLDebugTrace
#else
  #define Trace BLDummyTrace
#endif

// ============================================================================
// [BLOpenType::NameImpl - Utilities]
// ============================================================================

static uint32_t encodingFromPlatformId(uint32_t platformId) noexcept {
  // Both Unicode and Windows platform use 'UTF16-BE' encoding.
  bool isUnicode = platformId == Platform::kPlatformUnicode ||
                   platformId == Platform::kPlatformWindows ;
  return isUnicode ? BL_TEXT_ENCODING_UTF16 : BL_TEXT_ENCODING_LATIN1;
}

static BLResult convertNameStringToUtf8(BLString& dst, BLArrayView<uint8_t> src, uint32_t encoding) noexcept {
  // Name table should only have 16-bit lengths, so verify it's correct.
  BL_ASSERT(src.size < 65536);

  // We may overapproximate a bit, but it doesn't really matter as the length
  // is limited anyway.
  size_t dstSize = src.size * 2u;
  char* dstStart;
  BL_PROPAGATE(dst.modifyOp(BL_MODIFY_OP_ASSIGN_GROW, dstSize, &dstStart));

  BLUtf8Writer dstWriter(dstStart, dstSize);
  size_t nullTerminatorCount = 0;

  if (encoding == BL_TEXT_ENCODING_LATIN1) {
    const uint8_t* srcPtr = src.data;
    const uint8_t* srcEnd = src.data + src.size;

    while (srcPtr != srcEnd) {
      uint32_t uc = *srcPtr++;

      nullTerminatorCount += (uc == 0);
      if (uc <= 0x7F)
        dstWriter.writeByteUnsafe(uc);
      else
        dstWriter.write2BytesUnsafe(uc);
    }
  }
  else {
    // UTF16-BE.
    BLUtf16Reader srcReader(src.data, src.size & ~size_t(0x1));
    while (srcReader.hasNext()) {
      uint32_t uc;
      BL_PROPAGATE(srcReader.next<BL_UNICODE_IO_UNALIGNED | BL_UNICODE_IO_BYTE_ORDER_BE | BL_UNICODE_IO_STRICT>(uc));

      nullTerminatorCount += (uc == 0);
      BL_PROPAGATE(dstWriter.writeUnsafe(uc));
    }
  }

  // Remove null terminators at the end of the string. This can happen as some
  // fonts use them as padding. Also, some broken fonts encode data as UTF32-BE,
  // which would produce a lot of null terminators when decoded as UTF16-BE.
  char* dstPtr = dstWriter._ptr;
  while (dstPtr != dstStart && dstPtr[-1] == '\0') {
    dstPtr--;
    nullTerminatorCount--;
  }

  dst.truncate((size_t)(dstPtr - dstStart));
  if (nullTerminatorCount != 0)
    return blTraceError(BL_ERROR_INVALID_DATA);

  return BL_SUCCESS;
}

static void normalizeFamilyAndSubfamily(BLOTFaceImpl* faceI, Trace trace) noexcept {
  BLString& familyName = faceI->familyName;
  BLString& subfamilyName = faceI->subfamilyName;

  // Some fonts duplicate font subfamily-name in family-name, we try to match
  // such cases and truncate the sub-family in such case.
  if (familyName.size() >= subfamilyName.size() && !subfamilyName.empty()) {
    // Base size is a size of family name after the whole subfamily was removed
    // from it (if matched). It's basically the minimum length we would end up
    // when subfamily-name matches the end of family-name fully.
    size_t baseSize = familyName.size() - subfamilyName.size();
    if (memcmp(familyName.data() + baseSize, subfamilyName.data(), subfamilyName.size()) == 0) {
      trace.warn("Subfamily '%s' is redundant, removing...\n", subfamilyName.data());
      subfamilyName.reset();
      faceI->diagFlags |= BL_FONT_FACE_DIAG_FIXED_NAME_DATA;
    }
  }
}

// ============================================================================
// [BLOpenType::NameImpl - Init]
// ============================================================================

BLResult init(BLOTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  typedef NameTable::NameRecord NameRecord;

  BLFontTableT<NameTable> name;
  if (fontData->queryTable(&name, BL_MAKE_TAG('n', 'a', 'm', 'e')) == 0)
    return blTraceError(BL_ERROR_FONT_MISSING_IMPORTANT_TABLE);

  if (!blFontTableFitsT<NameTable>(name))
    return blTraceError(BL_ERROR_INVALID_DATA);

  Trace trace;
  trace.info("BLOTFaceImpl::InitName [Size=%zu]\n", name.size);
  trace.indent();

  if (BL_UNLIKELY(name.size < NameTable::kMinSize)) {
    trace.warn("Table is too small\n");
    return blTraceError(BL_ERROR_INVALID_DATA);
  }

  uint32_t format = name->format();
  uint32_t recordCount = name->recordCount();

  trace.info("Format: %u\n", format);
  trace.info("RecordCount: %u\n", recordCount);

  uint32_t stringRegionOffset = name->stringOffset();
  if (BL_UNLIKELY(stringRegionOffset >= name.size))
    return blTraceError(BL_ERROR_INVALID_DATA);

  // Only formats 0 and 1 are defined.
  if (BL_UNLIKELY(format > 1))
    return blTraceError(BL_ERROR_INVALID_DATA);

  // There must be some names otherwise this table is invalid. Also make sure
  // that the number of records doesn't overflow the size of 'name' itself.
  if (BL_UNLIKELY(recordCount == 0) || !blFontTableFitsN(name, 6 + recordCount * sizeof(NameRecord)))
    return blTraceError(BL_ERROR_INVALID_DATA);

  // Mask of name IDs which we are interested in.
  //
  // NOTE: We are not interested in WWS family and subfamily names as those may
  // include subfamilies, which we expect to be separate. We would only use WWS
  // names if there is no other choice.
  constexpr uint32_t kImportantNameIdMask = blBitMask<uint32_t>(
    BL_FONT_STRING_FAMILY_NAME,
    BL_FONT_STRING_SUBFAMILY_NAME,
    BL_FONT_STRING_FULL_NAME,
    BL_FONT_STRING_POST_SCRIPT_NAME,
    BL_FONT_STRING_TYPOGRAPHIC_FAMILY_NAME,
    BL_FONT_STRING_TYPOGRAPHIC_SUBFAMILY_NAME,
    BL_FONT_STRING_WWS_FAMILY_NAME,
    BL_FONT_STRING_WWS_SUBFAMILY_NAME);

  // Scoring is used to select the best records as the same NameId can be
  // repeated multiple times having a different `platformId`, `specificId`,
  // and `languageId`.
  uint16_t nameIdScore[BL_FONT_STRING_COMMON_COUNT] = { 0 }; // Score of each interesting NameId.
  uint32_t nameIdIndex[BL_FONT_STRING_COMMON_COUNT];         // Record index of matched NameId.
  uint32_t nameIdMask = 0;                                   // Mask of all matched NameIds.

  BLString tmpString;

  const NameRecord* nameRecords = name->nameRecords();
  uint32_t stringRegionSize = uint32_t(name.size - stringRegionOffset);

  for (uint32_t i = 0; i < recordCount; i++) {
    const NameRecord& nameRecord = nameRecords[i];

    // Don't bother with a NameId we are not interested in.
    uint32_t nameId = nameRecord.nameId();
    if (nameId >= BL_FONT_STRING_COMMON_COUNT || !(kImportantNameIdMask & (1 << nameId)))
      continue;

    uint32_t stringOffset = nameRecord.offset();
    uint32_t stringLength = nameRecord.length();

    // Offset could be anything if length is zero.
    if (stringLength == 0)
      stringOffset = 0;

    // Fonts are full of wrong data, if the offset is outside of the string data we simply skip the record.
    if (BL_UNLIKELY(stringOffset >= stringRegionSize || stringRegionSize - stringOffset < stringLength)) {
      trace.warn("Invalid Region {NameId=%u Offset=%u Length=%u}", stringOffset, stringLength);
      continue;
    }

    uint32_t platformId = nameRecord.platformId();
    uint32_t specificId = nameRecord.specificId();
    uint32_t languageId = nameRecord.languageId();

    uint32_t score = 0;
    switch (platformId) {
      case Platform::kPlatformUnicode: {
        score = 3;
        break;
      }

      case Platform::kPlatformMac: {
        // Sucks, but better than nothing...
        if (specificId == Platform::kMacEncodingRoman)
          score = 2;
        else
          continue;

        if (languageId == Platform::kMacLanguageEnglish) {
          score |= (0x01u << 8);
        }

        break;
      }

      case Platform::kPlatformWindows: {
        if (specificId == Platform::kWindowsEncodingSymbol)
          score = 1;
        else if (specificId == Platform::kWindowsEncodingUCS2)
          score = 4;
        else
          continue;

        // We use the term "locale" instead of "language" when it comes to
        // Windows platform. Locale specifies both primary language and
        // sub-language, which is usually related to a geographic location.
        uint32_t localeId = languageId;
        uint32_t primaryLangId = localeId & 0xFFu;

        // Check primary language.
        if (primaryLangId == Platform::kWindowsLanguageEnglish) {
          if (localeId == Platform::kWindowsLocaleEnglish_US)
            score |= (0x04u << 8);
          else if (localeId == Platform::kWindowsLocaleEnglish_UK)
            score |= (0x03u << 8);
          else
            score |= (0x02u << 8);
        }
        break;
      }
    }

    if (score) {
      // Make sure this string is decodable before using this entry.
      uint32_t encoding = encodingFromPlatformId(platformId);

      const uint8_t* src = name.data + stringRegionOffset + stringOffset;
      BLResult result = convertNameStringToUtf8(tmpString, BLArrayView<uint8_t> { src, stringLength }, encoding);

      if (result != BL_SUCCESS) {
        // Data contains either null terminator(s) or the data is corrupted. There are
        // some fonts that store some names in UTF32-BE encoding, we refuse these names
        // as it's not anywhere in the specification and thus broken.
        if (trace.enabled()) {
          trace.warn("Failed to decode '%s' <- [", tmpString.data());
          for (size_t j = 0; j < stringLength; j++)
            trace.out(" %02X", src[j]);
          trace.out(" ]\n");
        }

        score = 0;
        faceI->diagFlags |= BL_FONT_FACE_DIAG_WRONG_NAME_DATA;
      }
      else {
        // If this is a subfamily (NameId=2) on a MAC platform and it's empty
        // we prefer it, because many fonts have this field correctly empty on
        // MAC platform and filled incorrectly on Windows platform.
        if (platformId == Platform::kPlatformMac && nameId == BL_FONT_STRING_SUBFAMILY_NAME && tmpString.empty())
          score = 0xFFFFu;
      }

      trace.info("[%s] \"%s\" [Size=%u] {NameId=%u PlatformId=%u SpecificId=%u LanguageId=%u Score=%u}\n",
                 score > nameIdScore[nameId] ? "SELECT" : "DROP",
                 result != BL_SUCCESS ? "Failed" : tmpString.data(),
                 stringLength,
                 nameId,
                 platformId,
                 specificId,
                 languageId,
                 score);

      // Update if we have found a better candidate or this is was the first one.
      if (score > nameIdScore[nameId]) {
        nameIdScore[nameId] = uint16_t(score);
        nameIdIndex[nameId] = i;
        nameIdMask |= blBitMask<uint32_t>(nameId);
      }
    }
  }

  // Prefer TypographicFamilyName over FamilyName and WWSFamilyName.
  if (blBitTest(nameIdMask, BL_FONT_STRING_TYPOGRAPHIC_FAMILY_NAME)) {
    nameIdMask &= ~blBitMask<uint32_t>(BL_FONT_STRING_FAMILY_NAME, BL_FONT_STRING_WWS_FAMILY_NAME);
  }

  // Prefer TypographicSubfamilyName over SubfamilyName and WWSSubfamilyName.
  if (blBitTest(nameIdMask, BL_FONT_STRING_TYPOGRAPHIC_SUBFAMILY_NAME)) {
    nameIdMask &= ~blBitMask<uint32_t>(BL_FONT_STRING_SUBFAMILY_NAME, BL_FONT_STRING_WWS_SUBFAMILY_NAME);
  }

  if (blBitMatch(nameIdMask, blBitMask<uint32_t>(BL_FONT_STRING_TYPOGRAPHIC_FAMILY_NAME, BL_FONT_STRING_TYPOGRAPHIC_SUBFAMILY_NAME))) {
    trace.info("Has Typographic FamilyName and SubfamilyName\n");
    faceI->faceFlags |= BL_FONT_FACE_FLAG_TYPOGRAPHIC_NAMES;
  }

  BLBitWordIterator<uint32_t> bitWordIterator(nameIdMask);
  while (bitWordIterator.hasNext()) {
    uint32_t nameId = bitWordIterator.next();
    const NameRecord& nameRecord = nameRecords[nameIdIndex[nameId]];

    uint32_t platformId = nameRecord.platformId();
    uint32_t stringOffset = nameRecord.offset();
    uint32_t stringLength = nameRecord.length();

    // Offset could be anything if length is zero.
    if (stringLength == 0)
      stringOffset = 0;

    // This should have already been filtered out, but one is never sure...
    if (BL_UNLIKELY(stringOffset >= stringRegionSize || stringRegionSize - stringOffset < stringLength))
      return blTraceError(BL_ERROR_INVALID_DATA);

    BLString* dst = nullptr;
    switch (nameId) {
      case BL_FONT_STRING_FULL_NAME:
        dst = &faceI->fullName;
        break;

      case BL_FONT_STRING_FAMILY_NAME:
      case BL_FONT_STRING_WWS_FAMILY_NAME:
      case BL_FONT_STRING_TYPOGRAPHIC_FAMILY_NAME:
        dst = &faceI->familyName;
        break;

      case BL_FONT_STRING_SUBFAMILY_NAME:
      case BL_FONT_STRING_WWS_SUBFAMILY_NAME:
      case BL_FONT_STRING_TYPOGRAPHIC_SUBFAMILY_NAME:
        dst = &faceI->subfamilyName;
        break;

      case BL_FONT_STRING_POST_SCRIPT_NAME:
        dst = &faceI->postScriptName;
        break;
    }

    if (dst) {
      const uint8_t* src = name.data + stringRegionOffset + stringOffset;
      uint32_t encoding = encodingFromPlatformId(platformId);
      BL_PROPAGATE(convertNameStringToUtf8(*dst, BLArrayView<uint8_t> { src, stringLength }, encoding));
    }
  }

  normalizeFamilyAndSubfamily(faceI, trace);
  trace.info("Family=%s [SubFamily=%s] {PostScriptName=%s}\n",
             faceI->familyName.data(),
             faceI->subfamilyName.data(),
             faceI->postScriptName.data());
  return BL_SUCCESS;
}

} // {NameImpl}
} // {BLOpenType}
