// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../font_p.h"
#include "../unicode_p.h"
#include "../opentype/otcmap_p.h"
#include "../opentype/otface_p.h"
#include "../opentype/otplatform_p.h"
#include "../support/memops_p.h"
#include "../support/ptrops_p.h"

namespace BLOpenType {
namespace CMapImpl {

// OpenType::CMapImpl - None
// =========================

static BLResult BL_CDECL mapTextToGlyphsNone(const BLFontFaceImpl* faceI_, uint32_t* content, size_t count, BLGlyphMappingState* state) noexcept {
  blUnused(faceI_, content, count);
  state->reset();
  return blTraceError(BL_ERROR_FONT_NO_CHARACTER_MAPPING);
}

// OpenType::CMapImpl - Format0
// ============================

static BLResult BL_CDECL mapTextToGlyphsFormat0(const BLFontFaceImpl* faceI_, uint32_t* content, size_t count, BLGlyphMappingState* state) noexcept {
  const OTFaceImpl* faceI = static_cast<const OTFaceImpl*>(faceI_);
  const CMapTable::Format0* subTable = BLPtrOps::offset<CMapTable::Format0>(faceI->cmap.cmapTable.data, faceI->cmap.encoding.offset);
  const UInt8* glyphIdArray = subTable->glyphIdArray;

  uint32_t* ptr = content;
  uint32_t* end = content + count;

  size_t undefinedCount = 0;
  state->undefinedFirst = SIZE_MAX;

  while (ptr != end) {
    uint32_t uc = ptr[0];
    uint32_t glyphId = uc < 256 ? uint32_t(glyphIdArray[uc].value()) : uint32_t(0);

    ptr[0] = glyphId;
    if (BL_UNLIKELY(glyphId == 0)) {
      if (!undefinedCount)
        state->undefinedFirst = (size_t)(ptr - content);
      undefinedCount++;
    }

    ptr++;
  }

  state->glyphCount = (size_t)(ptr - content);
  state->undefinedCount = undefinedCount;

  return BL_SUCCESS;
}

// OpenType::CMapImpl - Format4
// ============================

static BL_INLINE const UInt16* findSegmentFormat4(
  uint32_t uc,
  const UInt16* lastCharArray,
  size_t numSeg,
  size_t numSearchableSeg,
  uint32_t& ucFirst, uint32_t& ucLast) noexcept {

  for (size_t i = numSearchableSeg; i != 0; i >>= 1) {
    const UInt16* endCountPtr = BLPtrOps::offset(lastCharArray, (i & ~size_t(1)));

    ucLast = endCountPtr[0].value();
    if (ucLast < uc) {
      lastCharArray = endCountPtr + 1;
      i--;
      continue;
    }

    ucFirst = BLMemOps::readU16uBE(BLPtrOps::offset(endCountPtr, 2u + numSeg * 2u));
    if (ucFirst <= uc)
      return endCountPtr;
  }

  return nullptr;
}

static BLResult BL_CDECL mapTextToGlyphsFormat4(const BLFontFaceImpl* faceI_, uint32_t* content, size_t count, BLGlyphMappingState* state) noexcept {
  const OTFaceImpl* faceI = static_cast<const OTFaceImpl*>(faceI_);
  const CMapTable::Format4* subTable = BLPtrOps::offset<CMapTable::Format4>(faceI->cmap.cmapTable.data, faceI->cmap.encoding.offset);

  uint32_t* ptr = content;
  uint32_t* end = content + count;

  size_t undefinedCount = 0;
  state->undefinedFirst = SIZE_MAX;

  size_t numSeg = size_t(subTable->numSegX2()) >> 1;
  const UInt16* lastCharArray = subTable->lastCharArray();

  // We want to read 2 bytes from the glyphIdsArray, that's why we decrement by one here.
  const uint8_t* dataEnd = reinterpret_cast<const uint8_t*>(subTable) + (faceI->cmap.cmapTable.size - faceI->cmap.encoding.offset - 1);

  size_t numSearchableSeg = faceI->cmap.encoding.entryCount;
  size_t idDeltaArrayOffset = 2u + numSeg * 4u;
  size_t idOffsetArrayOffset = 2u + numSeg * 6u;

  uint32_t uc;
  uint32_t ucFirst;
  uint32_t ucLast;

  while (ptr != end) {
    uc = ptr[0];

NewMatch:
    const UInt16* match = findSegmentFormat4(uc, lastCharArray, numSeg, numSearchableSeg, ucFirst, ucLast);
    if (match) {
      // `match` points to `endChar[]`, based on CMAP-Format4:
      //   - match[0             ] == lastCharArray [Segment]
      //   - match[2 + numSeg * 2] == firstCharArray[Segment]
      //   - match[2 + numSeg * 4] == idDeltaArray  [Segment]
      //   - match[2 + numSeg * 6] == idOffsetArray [Segment]
      uint32_t offset = BLPtrOps::offset(match, idOffsetArrayOffset)->value();
      for (;;) {
        // If the `offset` is not zero then we have to get the GlyphId from the array.
        if (offset != 0) {
          size_t rawRemain = (size_t)(dataEnd - reinterpret_cast<const uint8_t*>(match));
          size_t rawOffset = idOffsetArrayOffset + (uc - ucFirst) * 2u + offset;

          // This shouldn't happen if the sub-table was properly validated.
          if (BL_UNLIKELY(rawOffset >= rawRemain))
            goto UndefinedGlyph;

          uc = BLPtrOps::offset(match, rawOffset)->value();
        }

        uc += BLPtrOps::offset(match, idDeltaArrayOffset)->value();
        uc &= 0xFFFFu;

        if (BL_UNLIKELY(uc == 0))
          goto UndefinedGlyph;

        ptr[0] = uc;
        if (++ptr == end)
          goto Done;

        uc = ptr[0];
        if (uc < ucFirst || uc > ucLast)
          goto NewMatch;
      }
    }
    else {
UndefinedGlyph:
      if (!undefinedCount)
        state->undefinedFirst = (size_t)(ptr - content);

      *ptr++ = 0;
      undefinedCount++;
    }
  }

Done:
  state->glyphCount = (size_t)(ptr - content);
  state->undefinedCount = undefinedCount;

  return BL_SUCCESS;
}

// OpenType::CMapImpl - Format6
// ============================

static BLResult BL_CDECL mapTextToGlyphsFormat6(const BLFontFaceImpl* faceI_, uint32_t* content, size_t count, BLGlyphMappingState* state) noexcept {
  const OTFaceImpl* faceI = static_cast<const OTFaceImpl*>(faceI_);
  const CMapTable::Format6* subTable = BLPtrOps::offset<CMapTable::Format6>(faceI->cmap.cmapTable.data, faceI->cmap.encoding.offset);

  uint32_t* ptr = content;
  uint32_t* end = content + count;

  size_t undefinedCount = 0;
  state->undefinedFirst = SIZE_MAX;

  uint32_t ucFirst = subTable->first();
  uint32_t ucCount = subTable->count();
  const UInt16* glyphIdArray = subTable->glyphIdArray();

  while (ptr != end) {
    uint32_t uc = ptr[0] - ucFirst;
    uint32_t glyphId = uc < ucCount ? uint32_t(glyphIdArray[uc].value()) : uint32_t(0);

    *ptr++ = glyphId;
    if (BL_UNLIKELY(glyphId == 0)) {
      if (!undefinedCount)
        state->undefinedFirst = (size_t)(ptr - content) - 1;
      undefinedCount++;
    }
  }

  state->glyphCount = (size_t)(ptr - content);
  state->undefinedCount = undefinedCount;

  return BL_SUCCESS;
}

// OpenType::CMapImpl - Format10
// =============================

static BLResult BL_CDECL mapTextToGlyphsFormat10(const BLFontFaceImpl* faceI_, uint32_t* content, size_t count, BLGlyphMappingState* state) noexcept {
  const OTFaceImpl* faceI = static_cast<const OTFaceImpl*>(faceI_);
  const CMapTable::Format10* subTable = BLPtrOps::offset<CMapTable::Format10>(faceI->cmap.cmapTable.data, faceI->cmap.encoding.offset);

  uint32_t* ptr = content;
  uint32_t* end = content + count;

  size_t undefinedCount = 0;
  state->undefinedFirst = SIZE_MAX;

  uint32_t ucFirst = subTable->first();
  uint32_t ucCount = subTable->glyphIds.count();

  const UInt16* glyphIdArray = subTable->glyphIds.array();
  while (ptr != end) {
    uint32_t uc = ptr[0] - ucFirst;
    uint32_t glyphId = uc < ucCount ? uint32_t(glyphIdArray[uc].value()) : uint32_t(0);

    *ptr++ = glyphId;

    if (BL_UNLIKELY(glyphId == 0)) {
      if (!undefinedCount)
        state->undefinedFirst = (size_t)(ptr - content) - 1;
      undefinedCount++;
    }
  }

  state->glyphCount = (size_t)(ptr - content);
  state->undefinedCount = undefinedCount;

  return BL_SUCCESS;
}

// OpenType::CMapImpl - Format12 & Format13
// ========================================

static BL_INLINE bool findGroupFormat12_13(
  uint32_t uc,
  const CMapTable::Group* start,
  size_t count,
  uint32_t& ucFirst, uint32_t& ucLast, uint32_t& firstGlyphId) noexcept {

  for (size_t i = count; i != 0; i >>= 1) {
    const CMapTable::Group* group = start + (i >> 1);

    ucLast = group->last();
    if (ucLast < uc) {
      start = group + 1;
      i--;
      continue;
    }

    ucFirst = group->first();
    if (ucFirst > uc)
      continue;

    firstGlyphId = group->glyphId();
    return true;
  }

  return false;
}

template<uint32_t FormatId>
static BLResult mapTextToGlyphsFormat12_13(const BLFontFaceImpl* faceI_, uint32_t* content, size_t count, BLGlyphMappingState* state) noexcept {
  const OTFaceImpl* faceI = static_cast<const OTFaceImpl*>(faceI_);
  const CMapTable::Format12_13* subTable = BLPtrOps::offset<CMapTable::Format12_13>(faceI->cmap.cmapTable.data, faceI->cmap.encoding.offset);

  uint32_t* ptr = content;
  uint32_t* end = content + count;

  size_t undefinedCount = 0;
  state->undefinedFirst = SIZE_MAX;

  const CMapTable::Group* groupArray = subTable->groups.array();
  size_t groupCount = faceI->cmap.encoding.entryCount;

  while (ptr != end) {
    uint32_t uc;
    uint32_t ucFirst;
    uint32_t ucLast;
    uint32_t startGlyphId;

    uc = ptr[0];

NewMatch:
    if (findGroupFormat12_13(uc, groupArray, groupCount, ucFirst, ucLast, startGlyphId)) {
      for (;;) {
        uint32_t glyphId = (FormatId == 12) ? uint32_t((startGlyphId + uc - ucFirst) & 0xFFFFu)
                                            : uint32_t((startGlyphId               ) & 0xFFFFu);
        if (!glyphId)
          goto UndefinedGlyph;

        *ptr = glyphId;
        if (++ptr == end)
          goto Done;

        uc = ptr[0];
        if (uc < ucFirst || uc > ucLast)
          goto NewMatch;
      }
    }
    else {
UndefinedGlyph:
      if (!undefinedCount)
        state->undefinedFirst = (size_t)(ptr - content);

      *ptr++ = 0;
      undefinedCount++;
    }
  }

Done:
  state->glyphCount = (size_t)(ptr - content);
  state->undefinedCount = undefinedCount;

  return BL_SUCCESS;
}

// OpenType::CMapImpl - Validate
// =============================

BLResult validateSubTable(BLFontTable cmapTable, uint32_t subTableOffset, uint32_t& formatOut, CMapEncoding& encodingOut) noexcept {
  if (cmapTable.size < 4u || subTableOffset > cmapTable.size - 4u)
    return blTraceError(BL_ERROR_INVALID_DATA);

  uint32_t format = BLPtrOps::offset<const UInt16>(cmapTable.data, subTableOffset)->value();
  switch (format) {
    // Format 0 - Byte Encoding Table
    // ------------------------------

    case 0: {
      BLFontTableT<CMapTable::Format0> subTable = blFontSubTable(cmapTable, subTableOffset);
      if (!blFontTableFitsT<CMapTable::Format0>(subTable))
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint32_t length = subTable->length();
      if (length < CMapTable::Format0::kMinSize || length > subTable.size)
        return blTraceError(BL_ERROR_INVALID_DATA);

      formatOut = format;
      encodingOut.offset = subTableOffset;
      encodingOut.entryCount = 256;
      return BL_SUCCESS;
    }

    // Format 2 - High-Byte Mapping Through Table
    // ------------------------------------------

    case 2: {
      return blTraceError(BL_ERROR_NOT_IMPLEMENTED);
    }

    // Format 4 - Segment Mapping to Delta Values
    // ------------------------------------------

    case 4: {
      BLFontTableT<CMapTable::Format4> subTable = blFontSubTable(cmapTable, subTableOffset);
      if (!blFontTableFitsT<CMapTable::Format4>(subTable))
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint32_t length = subTable->length();
      if (length < CMapTable::Format4::kMinSize || length > subTable.size)
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint32_t numSegX2 = subTable->numSegX2();
      if (!numSegX2 || (numSegX2 & 1) != 0)
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint32_t numSeg = numSegX2 / 2;
      if (length < 16 + numSeg * 8)
        return blTraceError(BL_ERROR_INVALID_DATA);

      const UInt16* lastCharArray = subTable->lastCharArray();
      const UInt16* firstCharArray = subTable->firstCharArray(numSeg);
      const UInt16* idOffsetArray = subTable->idOffsetArray(numSeg);

      uint32_t previousEnd = 0;
      uint32_t numSegAfterCheck = numSeg;

      for (uint32_t i = 0; i < numSeg; i++) {
        uint32_t last = lastCharArray[i].value();
        uint32_t first = firstCharArray[i].value();
        uint32_t idOffset = idOffsetArray[i].value();

        if (first == 0xFFFF && last == 0xFFFF) {
          // We prefer number of segments without the ending mark(s). This
          // handles also the case of data with multiple ending marks.
          numSegAfterCheck = blMin(numSegAfterCheck, i);
        }
        else {
          if (first < previousEnd || first > last)
            return blTraceError(BL_ERROR_INVALID_DATA);

          if (i != 0 && first == previousEnd)
            return blTraceError(BL_ERROR_INVALID_DATA);

          if (idOffset != 0) {
            // Offset to 16-bit data must be even.
            if (idOffset & 1)
              return blTraceError(BL_ERROR_INVALID_DATA);

            // This just validates whether the table doesn't want us to jump
            // somewhere outside, it doesn't validate whether GlyphIds are not
            // outside the limit.
            uint32_t indexInTable = 16 + numSeg * 6u + idOffset + (last - first) * 2u;
            if (indexInTable >= length)
              return blTraceError(BL_ERROR_INVALID_DATA);
          }
        }

        previousEnd = last;
      }

      if (!numSegAfterCheck)
        return blTraceError(BL_ERROR_INVALID_DATA);

      formatOut = format;
      encodingOut.offset = subTableOffset;
      encodingOut.entryCount = numSegAfterCheck;
      return BL_SUCCESS;
    }

    // Format 6 - Trimmed Table Mapping
    // --------------------------------

    case 6: {
      BLFontTableT<CMapTable::Format6> subTable = blFontSubTable(cmapTable, subTableOffset);
      if (!blFontTableFitsT<CMapTable::Format6>(subTable))
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint32_t length = subTable->length();
      if (length < CMapTable::Format6::kMinSize || length > subTable.size)
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint32_t first = subTable->first();
      uint32_t count = subTable->count();

      if (!count || first + count > 0xFFFFu)
        return blTraceError(BL_ERROR_INVALID_DATA);

      if (length < sizeof(CMapTable::Format10) + count * 2u)
        return blTraceError(BL_ERROR_INVALID_DATA);

      formatOut = format;
      encodingOut.offset = subTableOffset;
      encodingOut.entryCount = count;
      return BL_SUCCESS;
    }

    // Format 8 - Mixed 16-Bit and 32-Bit Coverage
    // -------------------------------------------

    case 8: {
      return blTraceError(BL_ERROR_NOT_IMPLEMENTED);
    };

    // Format 10 - Trimmed Array
    // -------------------------

    case 10: {
      BLFontTableT<CMapTable::Format10> subTable = blFontSubTable(cmapTable, subTableOffset);
      if (!blFontTableFitsT<CMapTable::Format10>(subTable))
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint32_t length = subTable->length();
      if (length < CMapTable::Format10::kMinSize || length > subTable.size)
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint32_t first = subTable->first();
      uint32_t count = subTable->glyphIds.count();

      if (first >= BL_CHAR_MAX || !count || count > BL_CHAR_MAX || first + count > BL_CHAR_MAX)
        return blTraceError(BL_ERROR_INVALID_DATA);

      if (length < sizeof(CMapTable::Format10) + count * 2u)
        return blTraceError(BL_ERROR_INVALID_DATA);

      formatOut = format;
      encodingOut.offset = subTableOffset;
      encodingOut.entryCount = count;
      return BL_SUCCESS;
    }

    // Format 12 & 13 - Segmented Coverage / Many-To-One Range Mappings
    // ----------------------------------------------------------------

    case 12:
    case 13: {
      BLFontTableT<CMapTable::Format12_13> subTable = blFontSubTable(cmapTable, subTableOffset);
      if (!blFontTableFitsT<CMapTable::Format12_13>(subTable))
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint32_t length = subTable->length();
      if (length < CMapTable::Format12_13::kMinSize || length > subTable.size)
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint32_t count = subTable->groups.count();
      if (count > BL_CHAR_MAX || length < sizeof(CMapTable::Format12_13) + count * sizeof(CMapTable::Group))
        return blTraceError(BL_ERROR_INVALID_DATA);

      const CMapTable::Group* groupArray = subTable->groups.array();
      uint32_t first = groupArray[0].first();
      uint32_t last = groupArray[0].last();

      if (first > last || last > BL_CHAR_MAX)
        return blTraceError(BL_ERROR_INVALID_DATA);

      for (uint32_t i = 1; i < count; i++) {
        first = groupArray[i].first();
        if (first <= last)
          return blTraceError(BL_ERROR_INVALID_DATA);

        last = groupArray[i].last();
        if (first > last || last > BL_CHAR_MAX)
          return blTraceError(BL_ERROR_INVALID_DATA);
      }

      formatOut = format;
      encodingOut.offset = subTableOffset;
      encodingOut.entryCount = count;
      return BL_SUCCESS;
    }

    // Format 14 - Unicode Variation Sequences
    // ---------------------------------------

    case 14: {
      BLFontTableT<CMapTable::Format14> subTable = blFontSubTable(cmapTable, subTableOffset);
      if (!blFontTableFitsT<CMapTable::Format14>(subTable))
        return blTraceError(BL_ERROR_INVALID_DATA);

      // TODO: [OPENTYPE] CMAP Format14 not implemented.
      return blTraceError(BL_ERROR_NOT_IMPLEMENTED);
    }

    // Invalid / Unknown
    // -----------------

    default: {
      return blTraceError(BL_ERROR_INVALID_DATA);
    }
  }
}

// OpenType::CMapImpl - Populate Character Coverage
// ================================================

BLResult populateCharacterCoverage(const OTFaceImpl* faceI, BLBitSet* out) noexcept {
  out->clear();
  BLBitSetBuilderT<1024> set(out);

  switch (faceI->cmapFormat) {
    // Format 0 - Byte Encoding Table
    // ------------------------------

    case 0: {
      const CMapTable::Format0* subTable = BLPtrOps::offset<CMapTable::Format0>(faceI->cmap.cmapTable.data, faceI->cmap.encoding.offset);
      const UInt8* glyphIdArray = subTable->glyphIdArray;

      uint32_t bitArray[256 / BLIntOps::bitSizeOf<uint32_t>()] {};
      for (uint32_t i = 0; i < 256; i++) {
        uint32_t glyphId = glyphIdArray[i].value();
        if (glyphId != 0)
          BLBitSetOps::bitArraySetBit(bitArray, i);
      }

      return out->assignWords(0, bitArray, BL_ARRAY_SIZE(bitArray));
    }

    // Format 4 - Segment Mapping to Delta Values
    // ------------------------------------------

    case 4: {
      const CMapTable::Format4* subTable = BLPtrOps::offset<CMapTable::Format4>(faceI->cmap.cmapTable.data, faceI->cmap.encoding.offset);

      uint32_t numSegX2 = subTable->numSegX2();
      uint32_t numSeg = numSegX2 / 2;
      uint32_t entryCount = faceI->cmap.encoding.entryCount;

      // Not sure, we shouldn't proceed if these are zero.
      if (!numSeg || !entryCount)
        return BL_SUCCESS;

      const UInt16* lastCharArray = subTable->lastCharArray();
      const UInt16* firstCharArray = subTable->firstCharArray(numSeg);

      uint32_t rangeStart = 0;
      uint32_t rangeEnd = 0;

      uint32_t i = 0;
      while (i < entryCount) {
        uint32_t segmentStart = uint32_t(firstCharArray[i].value());
        uint32_t segmentEnd = uint32_t(lastCharArray[i].value()) + 1;

        i++;

        if (segmentStart == rangeEnd) {
          rangeEnd = segmentEnd;
          continue;
        }

        if (rangeStart < rangeEnd)
          BL_PROPAGATE(set.addRange(rangeStart, rangeEnd));

        rangeStart = segmentStart;
        rangeEnd = segmentEnd;
      }

      if (rangeStart < rangeEnd)
        BL_PROPAGATE(set.addRange(rangeStart, rangeEnd));

      return set.commit();
    }

    // Format 6 - Trimmed Table Mapping
    // --------------------------------

    case 6: {
      const CMapTable::Format6* subTable = BLPtrOps::offset<CMapTable::Format6>(faceI->cmap.cmapTable.data, faceI->cmap.encoding.offset);
      const UInt16* glyphIdArray = subTable->glyphIdArray();

      uint32_t firstChar = subTable->first();
      uint32_t entryCount = faceI->cmap.encoding.entryCount;

      for (uint32_t i = 0; i < entryCount; i++) {
        uint32_t glyphId = glyphIdArray[i].value();
        if (glyphId != 0)
          BL_PROPAGATE(set.addBit(firstChar + i));
      }

      return set.commit();
    }

    // Format 10 - Trimmed Array
    // -------------------------

    case 10: {
      const CMapTable::Format10* subTable = BLPtrOps::offset<CMapTable::Format10>(faceI->cmap.cmapTable.data, faceI->cmap.encoding.offset);
      const UInt16* glyphIdArray = subTable->glyphIds.array();

      uint32_t firstChar = subTable->first();
      uint32_t entryCount = faceI->cmap.encoding.entryCount;

      for (uint32_t i = 0; i < entryCount; i++) {
        uint32_t glyphId = glyphIdArray[i].value();
        if (glyphId != 0)
          BL_PROPAGATE(set.addBit(firstChar + i));
      }

      return set.commit();
    }

    // Format 12 / 13 - Segmented Coverage / Many-To-One Range Mappings
    // ----------------------------------------------------------------

    case 12:
    case 13: {
      const CMapTable::Format12_13* subTable = BLPtrOps::offset<CMapTable::Format12_13>(faceI->cmap.cmapTable.data, faceI->cmap.encoding.offset);
      const CMapTable::Group* groupArray = subTable->groups.array();

      uint32_t entryCount = faceI->cmap.encoding.entryCount;
      if (!entryCount)
        return BL_SUCCESS;

      uint32_t rangeStart = 0;
      uint32_t rangeEnd = 0;

      uint32_t i = 0;
      while (i < entryCount) {
        uint32_t segmentStart = groupArray[i].first();
        uint32_t segmentEnd = groupArray[i].last();

        i++;

        if (segmentEnd != 0xFFFFFFFFu)
          segmentEnd++;

        if (segmentStart == rangeEnd) {
          rangeEnd = segmentEnd;
          continue;
        }

        if (rangeStart < rangeEnd)
          BL_PROPAGATE(set.addRange(rangeStart, rangeEnd));

        rangeStart = segmentStart;
        rangeEnd = segmentEnd;
      }

      if (rangeStart < rangeEnd)
        BL_PROPAGATE(set.addRange(rangeStart, rangeEnd));

      return set.commit();
    }

    default:
      return blTraceError(BL_ERROR_FONT_NO_CHARACTER_MAPPING);
  }
}


// OpenType::CMapImpl - Init
// =========================

static bool isSupportedCMapFormat(uint32_t format) noexcept {
  switch (format) {
    case  0:
    case  4:
    case  6:
    case 10:
    case 12:
    case 13:
      return true;
    default:
      return false;
  }
}

static BLResult initCMapFuncs(OTFaceImpl* faceI) noexcept {
  switch (faceI->cmapFormat) {
    case  0: faceI->funcs.mapTextToGlyphs = mapTextToGlyphsFormat0; break;
    case  4: faceI->funcs.mapTextToGlyphs = mapTextToGlyphsFormat4; break;
    case  6: faceI->funcs.mapTextToGlyphs = mapTextToGlyphsFormat6; break;
    case 10: faceI->funcs.mapTextToGlyphs = mapTextToGlyphsFormat10; break;
    case 12: faceI->funcs.mapTextToGlyphs = mapTextToGlyphsFormat12_13<12>; break;
    case 13: faceI->funcs.mapTextToGlyphs = mapTextToGlyphsFormat12_13<13>; break;
    default: faceI->funcs.mapTextToGlyphs = mapTextToGlyphsNone; break;
  }
  return BL_SUCCESS;
}

BLResult init(OTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  BLFontTableT<CMapTable> cmap;
  if (!fontData->queryTable(faceI->faceInfo.faceIndex, &cmap, BL_MAKE_TAG('c', 'm', 'a', 'p')))
    return BL_SUCCESS;

  if (!blFontTableFitsT<CMapTable>(cmap)) {
    faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_CMAP_DATA;
    return BL_SUCCESS;
  }

  uint32_t encodingCount = cmap->encodings.count();
  if (cmap.size < sizeof(CMapTable) + encodingCount * sizeof(CMapTable::Encoding)) {
    faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_CMAP_DATA;
    return BL_SUCCESS;
  }

  enum Score : uint32_t {
    kScoreNothing    = 0x00000u,
    kScoreMacRoman   = 0x00001u, // Not sure this would ever be used, but OpenType sanitizer passes this one.
    kScoreSymbolFont = 0x00002u,
    kScoreAnyUnicode = 0x10000u,
    kScoreWinUnicode = 0x20000u  // Prefer Windows-Unicode CMAP over Unicode.
  };

  uint32_t matchedScore = kScoreNothing;
  uint32_t matchedFormat = 0xFFFFFFFFu;
  CMapEncoding matchedEncoding {};

  for (uint32_t i = 0; i < encodingCount; i++) {
    const CMapTable::Encoding& encoding = cmap->encodings.array()[i];
    uint32_t offset = encoding.offset();

    if (offset >= cmap.size - 4)
      continue;

    uint32_t platformId = encoding.platformId();
    uint32_t encodingId = encoding.encodingId();
    uint32_t format = BLPtrOps::offset(cmap.dataAs<UInt16>(), offset)->value();

    if (!isSupportedCMapFormat(format))
      continue;

    uint32_t thisScore = kScoreNothing;
    switch (platformId) {
      case Platform::kPlatformUnicode:
        thisScore = kScoreAnyUnicode + encodingId;
        break;

      case Platform::kPlatformWindows:
        if (encodingId == Platform::kWindowsEncodingSymbol) {
          thisScore = kScoreSymbolFont;
          faceI->faceInfo.faceFlags |= BL_FONT_FACE_FLAG_SYMBOL_FONT;
        }
        else if (encodingId == Platform::kWindowsEncodingUCS2 || encodingId == Platform::kWindowsEncodingUCS4) {
          thisScore = kScoreWinUnicode + encodingId;
        }
        break;

      case Platform::kPlatformMac:
        if (encodingId == Platform::kMacEncodingRoman && format == 0)
          thisScore = kScoreMacRoman;
        break;
    }

    if (thisScore > matchedScore) {
      uint32_t thisFormat;
      CMapEncoding thisEncoding {};

      BLResult result = validateSubTable(cmap, offset, thisFormat, thisEncoding);
      if (result != BL_SUCCESS) {
        if (result == BL_ERROR_NOT_IMPLEMENTED)
          faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_CMAP_FORMAT;
        else
          faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_CMAP_DATA;

        // If we had a match before then the match from before can still be used.
        continue;
      }

      matchedScore = thisScore;
      matchedFormat = thisFormat;
      matchedEncoding = thisEncoding;
    }
  }

  if (matchedScore) {
    faceI->faceInfo.faceFlags |= BL_FONT_FACE_FLAG_CHAR_TO_GLYPH_MAPPING;
    faceI->cmapFormat = uint8_t(matchedFormat);
    faceI->cmap.cmapTable = cmap;
    faceI->cmap.encoding = matchedEncoding;
    return initCMapFuncs(faceI);
  }
  else {
    // No cmap support, diagnostics was already set.
    faceI->funcs.mapTextToGlyphs = mapTextToGlyphsNone;
    return BL_SUCCESS;
  }
}

} // {CMapImpl}
} // {BLOpenType}
