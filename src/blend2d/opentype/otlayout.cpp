// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../font_p.h"
#include "../fonttags_p.h"
#include "../glyphbuffer_p.h"
#include "../tables_p.h"
#include "../trace_p.h"
#include "../opentype/otface_p.h"
#include "../opentype/otlayout_p.h"
#include "../support/bitops_p.h"
#include "../support/memops_p.h"
#include "../support/ptrops_p.h"

// TODO: This is not complete so we had to disable some warnings here...
BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

namespace BLOpenType {
namespace LayoutImpl {

// OpenType::LayoutImpl - Tracing
// ==============================

#if defined(BL_TRACE_OT_ALL) || defined(BL_TRACE_OT_LAYOUT)
  #define Trace BLDebugTrace
#else
  #define Trace BLDummyTrace
#endif

// OpenType::LayoutImpl - Debugging
// ================================

#if 0
static void dumpGPosValue(const Int16* p, uint32_t index, uint32_t valueFormat) noexcept {
  int32_t v;
  if (valueFormat & GPosTable::kValueXPlacement      ) { v = p->value(); p++; printf("#%d ValueXPlacement: %d\n", index, v); }
  if (valueFormat & GPosTable::kValueYPlacement      ) { v = p->value(); p++; printf("#%d ValueYPlacement: %d\n", index, v); }
  if (valueFormat & GPosTable::kValueXAdvance        ) { v = p->value(); p++; printf("#%d ValueXAdvance: %d\n", index, v); }
  if (valueFormat & GPosTable::kValueYAdvance        ) { v = p->value(); p++; printf("#%d ValueYAdvance: %d\n", index, v); }
  if (valueFormat & GPosTable::kValueXPlacementDevice) { v = p->value(); p++; printf("#%d ValueXPlacementDevice: %d\n", index, v); }
  if (valueFormat & GPosTable::kValueYPlacementDevice) { v = p->value(); p++; printf("#%d ValueYPlacementDevice: %d\n", index, v); }
  if (valueFormat & GPosTable::kValueXAdvanceDevice  ) { v = p->value(); p++; printf("#%d ValueXAdvanceDevice: %d\n", index, v); }
  if (valueFormat & GPosTable::kValueYAdvanceDevice  ) { v = p->value(); p++; printf("#%d ValueYAdvanceDevice: %d\n", index, v); }
}
#endif

// OpenType::LayoutImpl - Validator
// ================================

class Validator {
public:
  OTFaceImpl* faceI;

  union {
    BLFontTable tables[3];
    struct {
      BLFontTable gsub;
      BLFontTable gpos;
      BLFontTable gdef;
    };
  };

  BLArray<BLTag> scriptTags;
  BLArray<BLTag> featureTags;

  BL_INLINE Validator(OTFaceImpl* faceI) noexcept
    : faceI(faceI),
      tables {},
      scriptTags(),
      featureTags() {}
};

// OpenType::LayoutImpl - LookupInfo
// =================================

static const LookupInfo gLookupInfo[2] = {
  // GSUB:
  {
    // LookupCount & ExtensionType:
    GSubTable::kLookupCount,
    GSubTable::kLookupExtension,

    // LookupTypeInfo:
    {
      { 0, LookupInfo::kGSubNone         }, // GSUB Lookup Type #0 - Invalid.
      { 2, LookupInfo::kGSubType1Format1 }, // GSUB Lookup Type #1 - Single Substitution.
      { 1, LookupInfo::kGSubType2Format1 }, // GSUB Lookup Type #2 - Multiple Substitution.
      { 1, LookupInfo::kGSubType3Format1 }, // GSUB Lookup Type #3 - Alternate Substitution.
      { 1, LookupInfo::kGSubType4Format1 }, // GSUB Lookup Type #4 - Ligature Substitution.
      { 3, LookupInfo::kGSubType5Format1 }, // GSUB Lookup Type #5 - Contextual Substitution.
      { 3, LookupInfo::kGSubType6Format1 }, // GSUB Lookup Type #6 - Chained Context.
      { 1, LookupInfo::kGSubNone         }, // GSUB Lookup Type #7 - Extension.
      { 1, LookupInfo::kGSubType8Format1 }  // GSUB Lookup Type #8 - Reverse Chained Substitution.
    },

    // LookupIdInfo:
    {
      { uint8_t(0)  },
      { uint8_t(6)  }, // Lookup Type #1 - Format #1.
      { uint8_t(6)  }, // Lookup Type #1 - Format #2.
      { uint8_t(6)  }, // Lookup Type #2 - Format #1.
      { uint8_t(6)  }, // Lookup Type #3 - Format #1.
      { uint8_t(6)  }, // Lookup Type #4 - Format #1.
      { uint8_t(6)  }, // Lookup Type #5 - Format #1.
      { uint8_t(8)  }, // Lookup Type #5 - Format #2.
      { uint8_t(6)  }, // Lookup Type #5 - Format #3.
      { uint8_t(6)  }, // Lookup Type #6 - Format #1.
      { uint8_t(12) }, // Lookup Type #6 - Format #2.
      { uint8_t(10) }, // Lookup Type #6 - Format #3.
      { uint8_t(10) }  // Lookup Type #8 - Format #1.
    }
  },

  // GPOS:
  {
    // LookupCount & ExtensionType:
    GPosTable::kLookupCount,
    GPosTable::kLookupExtension,

    // LookupTypeInfo:
    {
      { 0, LookupInfo::kGPosNone         }, // GPOS Lookup Type #0 - Invalid.
      { 2, LookupInfo::kGPosType1Format1 }, // GPOS Lookup Type #1 - Single Adjustment.
      { 2, LookupInfo::kGPosType2Format1 }, // GPOS Lookup Type #2 - Pair Adjustment.
      { 1, LookupInfo::kGPosType3Format1 }, // GPOS Lookup Type #3 - Cursive Attachment.
      { 1, LookupInfo::kGPosType4Format1 }, // GPOS Lookup Type #4 - MarkToBase Attachment.
      { 1, LookupInfo::kGPosType5Format1 }, // GPOS Lookup Type #5 - MarkToLigature Attachment.
      { 1, LookupInfo::kGPosType6Format1 }, // GPOS Lookup Type #6 - MarkToMark Attachment.
      { 3, LookupInfo::kGPosType7Format1 }, // GPOS Lookup Type #7 - Context Positioning.
      { 3, LookupInfo::kGPosType8Format1 }, // GPOS Lookup Type #8 - Chained Contextual Positioning
      { 1, LookupInfo::kGPosNone         }  // GPOS Lookup Type #9 - Extension.
    },

    // LookupIdInfo:
    {
      { uint8_t(0)  },
      { uint8_t(6)  }, // Lookup Type #1 - Format #1.
      { uint8_t(8)  }, // Lookup Type #1 - Format #2.
      { uint8_t(10) }, // Lookup Type #2 - Format #1.
      { uint8_t(16) }, // Lookup Type #2 - Format #2.
      { uint8_t(6)  }, // Lookup Type #3 - Format #1.
      { uint8_t(12) }, // Lookup Type #4 - Format #1.
      { uint8_t(12) }, // Lookup Type #5 - Format #1.
      { uint8_t(12) }, // Lookup Type #6 - Format #1.
      { uint8_t(6)  }, // Lookup Type #7 - Format #1.
      { uint8_t(8)  }, // Lookup Type #7 - Format #2.
      { uint8_t(6)  }, // Lookup Type #7 - Format #3.
      // TODO: [OPENTYPE GPOS]
      { uint8_t(2)  }, // Lookup Type #8 - Format #1.
      { uint8_t(2)  }, // Lookup Type #8 - Format #2.
      { uint8_t(2)  }  // Lookup Type #8 - Format #3.
    }
  }
};

// OpenType::LayoutImpl - ValueRecord
// ==================================

// struct ValueRecords {
//   ?[Int16 xPlacement]
//   ?[Int16 yPlacement]
//   ?[Int16 xAdvance]
//   ?[Int16 yAdvance]
//   ?[UInt16 xPlacementDeviceOffset]
//   ?[UInt16 yPlacementDeviceOffset]
//   ?[UInt16 xAdvanceDeviceOffset]
//   ?[UInt16 yAdvanceDeviceOffset]
// }
static BL_INLINE uint32_t sizeOfValueRecordByFormat(uint32_t valueFormat) noexcept {
  return uint32_t(blBitCountByteTable[valueFormat & 0xFFu]) * 2u;
}

// OpenType::LayoutImpl - Offsets
// ==============================

static bool checkRawOffsetArray(Validator* self, Trace trace, BLFontTable data, const char* tableName) noexcept {
  blUnused(self);

  if (BL_UNLIKELY(data.size < 2u))
    return trace.fail("%s: Table is too small [Size=%zu]\n", tableName, data.size);

  uint32_t count = data.dataAs<Array16<UInt16>>()->count();
  size_t headerSize = 2u + count * 2u;

  if (BL_UNLIKELY(data.size < headerSize))
    return trace.fail("%s: Table is truncated [Size=%zu RequiredSize=%zu]\n", tableName, data.size, headerSize);

  const UInt16* array = data.dataAs<Array16<Offset16>>()->array();
  for (uint32_t i = 0; i < count; i++) {
    uint32_t subOffset = array[i].value();
    if (BL_UNLIKELY(subOffset < headerSize || subOffset >= data.size))
      return trace.fail("%s: Invalid offset at #%u [%u], valid range [%zu:%zu]\n", tableName, i, subOffset, headerSize, data.size);
  }

  return true;
}

static bool checkTagRef16Array(Validator* self, Trace trace, BLFontTable data, const char* tableName) noexcept {
  blUnused(self);

  if (BL_UNLIKELY(data.size < 2u))
    return trace.fail("%s is too small [Size=%zu]\n", tableName, data.size);

  uint32_t count = data.dataAs<Array16<UInt16>>()->count();
  size_t headerSize = 2u + count * uint32_t(sizeof(TagRef16));

  if (BL_UNLIKELY(data.size < headerSize))
    return trace.fail("%s is truncated [Size=%zu RequiredSize=%zu]\n", tableName, data.size, headerSize);

  const TagRef16* array = data.dataAs<Array16<TagRef16>>()->array();
  for (uint32_t i = 0; i < count; i++) {
    uint32_t subOffset = array[i].offset.value();
    if (BL_UNLIKELY(subOffset < headerSize || subOffset >= data.size))
      return trace.fail("%s has invalid offset at #%u [%u], valid range [%zu:%zu]\n", tableName, i, subOffset, headerSize, data.size);
  }

  return true;
}

// OpenType::LayoutImpl - ClassDefTable
// ====================================

static bool checkClassDefTable(Validator* self, Trace trace, BLFontTable data, const char* tableName) noexcept {
  trace.info("%s\n", tableName);
  trace.indent();

  // Ignore if it doesn't fit.
  if (BL_UNLIKELY(!blFontTableFitsT<ClassDefTable>(data)))
    return trace.fail("Table is too small [Size=%zu]\n", data.size);

  uint32_t format = data.dataAs<ClassDefTable>()->format();
  trace.info("Format: %u\n", format);

  switch (format) {
    case 1: {
      size_t headerSize = ClassDefTable::Format1::kMinSize;
      if (BL_UNLIKELY(data.size < headerSize))
        return trace.fail("Table is truncated [Size=%zu Required=%zu]\n", data.size, headerSize);

      const ClassDefTable::Format1* f = data.dataAs<ClassDefTable>()->format1();
      uint32_t first = f->firstGlyph();
      uint32_t count = f->classValues.count();

      trace.info("FirstGlyph: %u\n", first);
      trace.info("GlyphCount: %u\n", count);

      // We won't fail, but we won't consider we have a ClassDef either.
      // If the ClassDef is required by other tables then we will fail later.
      if (BL_UNLIKELY(!count))
        return trace.warn("No glyph ids specified, ignoring...\n");

      headerSize += count * 2u;
      if (BL_UNLIKELY(data.size < headerSize))
        return trace.fail("Table is truncated [Size=%zu RequiredSize=%zu]\n", data.size, headerSize);

      return true;
    }

    case 2: {
      size_t headerSize = ClassDefTable::Format2::kMinSize;
      if (BL_UNLIKELY(data.size < headerSize))
        return trace.fail("Table is truncated [Size=%zu Required=%zu]\n", data.size, headerSize);

      const ClassDefTable::Format2* f = data.dataAs<ClassDefTable>()->format2();
      uint32_t count = f->ranges.count();

      trace.info("RangeCount: %u\n", count);

      // We won't fail, but we won't consider we have a class definition either.
      if (BL_UNLIKELY(!count))
        return trace.warn("No range specified, ignoring...\n");

      headerSize = ClassDefTable::Format2::kMinSize + count * sizeof(ClassDefTable::Range);
      if (BL_UNLIKELY(data.size < headerSize))
        return trace.fail("Table is truncated [Size=%zu RequiredSize=%zu]\n", data.size, headerSize);

      const ClassDefTable::Range* rangeArray = f->ranges.array();
      uint32_t lastGlyph = rangeArray[0].lastGlyph();

      if (BL_UNLIKELY(rangeArray[0].firstGlyph() > lastGlyph))
        return trace.fail("Table is invalid\n");

      for (uint32_t i = 1; i < count; i++) {
        const ClassDefTable::Range& range = rangeArray[i];
        uint32_t firstGlyph = range.firstGlyph();

        if (BL_UNLIKELY(firstGlyph <= lastGlyph))
          return trace.fail("Range #%u: FirstGlyph [%u] not greater than previous LastGlyph [%u] \n", i, firstGlyph, lastGlyph);

        lastGlyph = range.lastGlyph();
        if (BL_UNLIKELY(firstGlyph > lastGlyph))
          return trace.fail("Range #%u: FirstGlyph [%u] greater than LastGlyph [%u]\n", i, firstGlyph, lastGlyph);
      }

      return true;
    }

    default:
      return trace.fail("ClassDefTable format %u is invalid\n", format);
  }
}

// OpenType::LayoutImpl - CoverageTable
// ====================================

static bool checkCoverageTable(Validator* self, Trace trace, BLFontTable data, uint32_t& countCoverageEntries) noexcept {
  countCoverageEntries = 0;
  if (!blFontTableFitsT<CoverageTable>(data))
    return trace.fail("CoverageTable is too small [Size=%zu]\n", data.size);

  uint32_t format = data.dataAs<CoverageTable>()->format();
  switch (format) {
    case 1: {
      const CoverageTable::Format1* table = data.dataAs<CoverageTable::Format1>();
      uint32_t glyphCount = table->glyphs.count();

      trace.info("CoverageTable::Format1\n");
      trace.indent();

      size_t headerSize = CoverageTable::Format1::kMinSize + glyphCount * 2u;
      if (BL_UNLIKELY(data.size < headerSize))
        return trace.fail("Table is truncated [Size=%zu RequiredSize=%zu]\n", data.size, headerSize);

      if (BL_UNLIKELY(!glyphCount))
        return trace.fail("GlyphCount cannot be zero\n");

      countCoverageEntries = glyphCount;
      return true;
    }

    case 2: {
      const CoverageTable::Format2* table = data.dataAs<CoverageTable::Format2>();
      uint32_t rangeCount = table->ranges.count();

      trace.info("CoverageTable::Format2\n");
      trace.indent();

      size_t headerSize = CoverageTable::Format2::kMinSize + rangeCount * sizeof(CoverageTable::Range);
      if (BL_UNLIKELY(data.size < headerSize))
        return trace.fail("Table is truncated [Size=%zu RequiredSize=%zu]\n", data.size, headerSize);

      if (BL_UNLIKELY(!rangeCount))
        return trace.fail("RangeCount cannot be zero\n");

      const CoverageTable::Range* rangeArray = table->ranges.array();

      uint32_t firstGlyph = rangeArray[0].firstGlyph();
      uint32_t lastGlyph = rangeArray[0].lastGlyph();
      uint32_t currentCoverageIndex = rangeArray[0].startCoverageIndex();

      if (BL_UNLIKELY(firstGlyph > lastGlyph))
        return trace.fail("Range[%u]: FirstGlyph [%u] is greater than LastGlyph [%u]\n", 0, firstGlyph, lastGlyph);

      if (BL_UNLIKELY(currentCoverageIndex))
        return trace.fail("Range[%u]: Initial StartCoverageIndex [%u] must be zero\n", 0, currentCoverageIndex);
      currentCoverageIndex += lastGlyph - firstGlyph + 1u;

      for (uint32_t i = 1; i < rangeCount; i++) {
        const CoverageTable::Range& range = rangeArray[i];

        firstGlyph = range.firstGlyph();
        if (BL_UNLIKELY(firstGlyph <= lastGlyph))
          return trace.fail("Range[%u]: FirstGlyph [%u] is not greater than previous LastGlyph [%u]\n", i, firstGlyph, lastGlyph);

        lastGlyph = range.lastGlyph();
        if (BL_UNLIKELY(firstGlyph > lastGlyph))
          return trace.fail("Range[%u]: FirstGlyph [%u] is greater than LastGlyph [%u]\n", i, firstGlyph, lastGlyph);

        uint32_t startCoverageIndex = range.startCoverageIndex();
        if (BL_UNLIKELY(startCoverageIndex != currentCoverageIndex))
          return trace.fail("Range[%u]: StartCoverageIndex [%u] doesnt' match CurrentCoverageIndex [%u]\n", i, startCoverageIndex, currentCoverageIndex);

        currentCoverageIndex += lastGlyph - firstGlyph + 1u;
      }

      countCoverageEntries = currentCoverageIndex;
      return true;
    }

    default:
      return trace.fail("Invalid CoverageTable format [%u]\n", format);
  }
}

static bool checkLookupWithCoverage(Validator* self, Trace trace, BLFontTable data, size_t headerSize, uint32_t& countCoverageEntries) noexcept {
  if (BL_UNLIKELY(data.size < headerSize))
    return trace.fail("Table is truncated [Size=%zu Required=%zu]\n", data.size, headerSize);

  uint32_t coverageOffset = data.dataAs<GAnyTable::LookupHeaderWithCoverage>()->coverageOffset();
  if (BL_UNLIKELY(coverageOffset < headerSize || coverageOffset >= data.size))
    return trace.fail("Coverage offset [%u] is out of range [%zu:%zu]\n", coverageOffset, headerSize, data.size);

  return checkCoverageTable(self, trace, blFontSubTable(data, coverageOffset), countCoverageEntries);
}

// OpenType::LayoutImpl - GDEF - Init
// ==================================

static bool checkGDefTable(Validator* self, Trace trace) noexcept {
  OTFaceImpl* faceI = self->faceI;
  BLFontTableT<GDefTable> gdef = self->gdef;

  trace.info("OpenType::Init 'GDEF' [Size=%zu]\n", gdef.size);
  trace.indent();

  if (!blFontTableFitsT<GDefTable>(gdef))
    return trace.fail("Table too small [Size=%zu Required: %zu]\n", gdef.size, size_t(GDefTable::kMinSize));

  uint32_t version = gdef->v1_0()->version();
  size_t headerSize = GDefTable::HeaderV1_0::kMinSize;

  if (version >= 0x00010002u) headerSize = GDefTable::HeaderV1_2::kMinSize;
  if (version >= 0x00010003u) headerSize = GDefTable::HeaderV1_3::kMinSize;

  if (BL_UNLIKELY(version < 0x00010000u || version > 0x00010003u))
    return trace.fail("Invalid version [%u.%u]\n", version >> 16, version & 0xFFFFu);

  if (BL_UNLIKELY(gdef.size < headerSize))
    return trace.fail("Table is too small [Size=%zu Required=%zu]\n", gdef.size, headerSize);

  uint32_t glyphClassDefOffset      = gdef->v1_0()->glyphClassDefOffset();
  uint32_t attachListOffset         = gdef->v1_0()->attachListOffset();
  uint32_t ligCaretListOffset       = gdef->v1_0()->ligCaretListOffset();
  uint32_t markAttachClassDefOffset = gdef->v1_0()->markAttachClassDefOffset();
  uint32_t markGlyphSetsDefOffset   = version >= 0x00010002u ? uint32_t(gdef->v1_2()->markGlyphSetsDefOffset()) : uint32_t(0);
  uint32_t itemVarStoreOffset       = version >= 0x00010003u ? uint32_t(gdef->v1_3()->itemVarStoreOffset()    ) : uint32_t(0);

  // TODO: Unfinished.
  blUnused(attachListOffset, ligCaretListOffset, markGlyphSetsDefOffset, itemVarStoreOffset);

  // Some fonts have incorrect value of `GlyphClassDefOffset` set to 10. This collides with the header which is
  // 12 bytes. It's probably a result of some broken tool used to write such fonts in the past. We simply fix
  // this issue by changing the `headerSize` to 10.
  if (glyphClassDefOffset == 10 && version == 0x00010000u) {
    trace.warn("Fixing header size from 12 to 10 because of GlyphClassDefOffset\n");
    headerSize = 10;
    markAttachClassDefOffset = 0;
  }

  if (glyphClassDefOffset) {
    const char* name = "GlyphClassDef";
    if (glyphClassDefOffset < headerSize || glyphClassDefOffset >= gdef.size)
      return trace.fail("%s offset [%u] out of range [%zu:%zu]\n", name, glyphClassDefOffset, headerSize, gdef.size);

    if (!checkClassDefTable(self, trace, blFontSubTable(gdef, glyphClassDefOffset), name)) {
      faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_GDEF_DATA;
    }
    else {
      faceI->otFlags |= OTFaceFlags::kGlyphClassDef;
    }
  }

  if (markAttachClassDefOffset) {
    const char* name = "MatchAttachClassDef";
    if (markAttachClassDefOffset < headerSize || markAttachClassDefOffset >= gdef.size)
      return trace.fail("%s offset [%u] out of range [%zu:%zu]\n", name, markAttachClassDefOffset, headerSize, gdef.size);

    if (!checkClassDefTable(self, trace, blFontSubTable(gdef, markAttachClassDefOffset), name)) {
      faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_GDEF_DATA;
    }
    else {
      faceI->otFlags |= OTFaceFlags::kMarkAttachClassDef;
    }
  }

  return true;
}

// OpenType::LayoutImpl - GSUB - Lookup Type #1
// ============================================

// Single Substitution
// -------------------
//
// Replace a single glyph with another glyph.

static BL_INLINE bool checkGSubLookupType1Format1(Validator* self, Trace trace, BLFontTable table) noexcept {
  uint32_t countCoverageEntries;
  return checkLookupWithCoverage(self, trace, table, sizeof(GSubTable::SingleSubst1), countCoverageEntries);
}

static BL_INLINE bool checkGSubLookupType1Format2(Validator* self, Trace trace, BLFontTable table) noexcept {
  uint32_t countCoverageEntries;
  if (BL_UNLIKELY(!checkLookupWithCoverage(self, trace, table, sizeof(GSubTable::SingleSubst2), countCoverageEntries)))
    return false;

  const GSubTable::SingleSubst2* lookup = table.dataAs<GSubTable::SingleSubst2>();
  uint32_t glyphCount = lookup->glyphs.count();

  size_t headerSize = sizeof(GSubTable::SingleSubst2) + glyphCount * 2u;
  if (BL_UNLIKELY(table.size < headerSize))
    return trace.fail("Table is truncated [Size=%zu Required=%zu]\n", table.size, headerSize);

  return true;
}

template<uint32_t CoverageFormat>
static BL_INLINE BLResult applyGSubLookupType1Format1(const OTFaceImpl* faceI, GSubContext& ctx, BLFontTableT<GSubTable::SingleSubst1> table, uint32_t lookupFlags, CoverageIterator& covIt) noexcept {
  size_t itemCount = ctx.in.end - ctx.in.index;

  uint32_t minGlyphId = covIt.minGlyphId<CoverageFormat>();
  uint32_t maxGlyphId = covIt.maxGlyphId<CoverageFormat>();
  uint32_t glyphDelta = uint16_t(table->deltaGlyphId());

  if (ctx.inPlace() && ctx.isSameIndex()) {
    uint32_t* ptr = ctx.in.glyphData + ctx.in.index;
    uint32_t* end = ctx.in.glyphData + ctx.in.end;

    while (ptr != end) {
      uint32_t glyphId = ptr[0];
      if (glyphId >= minGlyphId && glyphId <= maxGlyphId) {
        uint32_t coverageIndex;
        if (covIt.find<CoverageFormat>(glyphId, coverageIndex))
          ptr[0] = (glyphId + glyphDelta) & 0xFFFFu;
      }
      ptr++;
    }
  }
  else {
    if (!ctx.inPlace())
      BL_PROPAGATE(ctx.prepareOut(itemCount));

    uint32_t* inPtr = ctx.in.glyphData + ctx.in.index;
    uint32_t* inEnd = ctx.in.glyphData + ctx.in.end;
    BLGlyphInfo* inInfo = ctx.in.infoData + ctx.in.index;

    uint32_t* outPtr  = ctx.out.glyphData + ctx.out.index;
    BLGlyphInfo* outInfo = ctx.out.infoData + ctx.out.index;

    while (inPtr != inEnd) {
      uint32_t glyphId = inPtr[0];
      if (glyphId >= minGlyphId && glyphId <= maxGlyphId) {
        uint32_t coverageIndex;
        if (covIt.find<CoverageFormat>(glyphId, coverageIndex))
          glyphId = (glyphId + glyphDelta) & 0xFFFFu;
      }

      *outPtr++ = glyphId;
      *outInfo++ = *inInfo;

      inPtr++;
      inInfo++;
    }
  }

  ctx.in.index += itemCount;
  ctx.out.index += itemCount;
  return BL_SUCCESS;
}

template<uint32_t CoverageFormat>
static BL_INLINE BLResult applyGSubLookupType1Format2(const OTFaceImpl* faceI, GSubContext& ctx, BLFontTableT<GSubTable::SingleSubst2> table, uint32_t lookupFlags, CoverageIterator& covIt) noexcept {
  size_t itemCount = ctx.in.end - ctx.in.index;

  uint32_t minGlyphId = covIt.minGlyphId<CoverageFormat>();
  uint32_t maxGlyphId = covIt.maxGlyphId<CoverageFormat>();
  uint32_t substCount = table->glyphs.count();

  if (BL_UNLIKELY(table.size < GSubTable::SingleSubst2::kMinSize + substCount * 2u))
    return ctx.advance(itemCount);

  if (ctx.inPlace() && ctx.isSameIndex()) {
    uint32_t* ptr = ctx.in.glyphData + ctx.in.index;
    uint32_t* end = ctx.in.glyphData + ctx.in.end;

    while (ptr != end) {
      uint32_t glyphId = ptr[0];
      if (glyphId >= minGlyphId && glyphId <= maxGlyphId) {
        uint32_t coverageIndex;
        if (covIt.find<CoverageFormat>(glyphId, coverageIndex) || coverageIndex >= substCount)
          ptr[0] = table->glyphs.array()[coverageIndex].value();
      }

      ptr++;
    }
  }
  else {
    if (!ctx.inPlace())
      BL_PROPAGATE(ctx.prepareOut(itemCount));

    uint32_t* inPtr = ctx.in.glyphData + ctx.in.index;
    uint32_t* inEnd = ctx.in.glyphData + ctx.in.end;
    BLGlyphInfo* inInfo = ctx.in.infoData + ctx.in.index;

    uint32_t* outPtr = ctx.out.glyphData + ctx.out.index;
    BLGlyphInfo* outInfo = ctx.out.infoData + ctx.out.index;

    while (inPtr != inEnd) {
      uint32_t glyphId = inPtr[0];
      if (glyphId >= minGlyphId && glyphId <= maxGlyphId) {
        uint32_t coverageIndex;
        if (covIt.find<CoverageFormat>(glyphId, coverageIndex) || coverageIndex >= substCount)
          glyphId = table->glyphs.array()[coverageIndex].value();
      }

      *outPtr++ = glyphId;
      *outInfo++ = *inInfo;

      inPtr++;
      inInfo++;
    }
  }

  ctx.in.index += itemCount;
  ctx.out.index += itemCount;
  return BL_SUCCESS;
}

// OpenType::LayoutImpl - GSUB - Lookup Type #2
// ============================================

// Multiple Substitution
// ---------------------
//
// Replace a single glyph with more than one glyph. The replacement sequence cannot be empty, it's explicitly
// forbidden by the specification.

static BL_INLINE bool checkGSubLookupType2Format1(Validator* self, Trace trace, BLFontTable table) noexcept {
  uint32_t countCoverageEntries;
  if (BL_UNLIKELY(!checkLookupWithCoverage(self, trace, table, sizeof(GSubTable::MultipleSubst1), countCoverageEntries)))
    return false;

  const GSubTable::MultipleSubst1* lookup = table.dataAs<GSubTable::MultipleSubst1>();
  uint32_t seqSetCount = lookup->sequenceOffsets.count();

  size_t headerSize = sizeof(GSubTable::MultipleSubst1) + seqSetCount * 2u;
  if (BL_UNLIKELY(table.size < headerSize))
    return trace.fail("Table is too small [Size=%zu Required=%zu]\n", table.size, headerSize);

  // Offsets to glyph sequences.
  const Offset16* offsetArray = lookup->sequenceOffsets.array();
  size_t endOffset = table.size - 4u;

  for (uint32_t i = 0; i < seqSetCount; i++) {
    uint32_t seqOffset = offsetArray[i].value();

    if (BL_UNLIKELY(seqOffset < headerSize || seqOffset > endOffset))
      return trace.fail("Sequence #%u [%u] is out of range [%zu:%zu]\n", i, seqOffset, headerSize, endOffset);

    const Array16<UInt16>* sequence = BLPtrOps::offset<const Array16<UInt16>>(table.data, seqOffset);
    uint32_t seqLength = sequence->count();

    // Specification forbids an empty Sequence.
    if (BL_UNLIKELY(!seqLength))
      return trace.fail("Sequence #%u [%u] is empty, which is not allowed\n", i);

    uint32_t seqEnd = seqOffset + 2u + seqLength * 2u;
    if (BL_UNLIKELY(seqEnd > table.size))
      return trace.fail("Sequence #%u [%u] length [%u] overflows the table size by [%zu] bytes\n", i, seqLength, size_t(table.size - seqEnd));
  }

  return true;
}

template<uint32_t CoverageFormat>
static BL_INLINE BLResult applyGSubLookupType2Format1(const OTFaceImpl* faceI, GSubContext& ctx, BLFontTableT<GSubTable::MultipleSubst1> table, uint32_t lookupFlags, CoverageIterator& covIt) noexcept {
  size_t itemCount = ctx.in.end - ctx.in.index;

  uint32_t minGlyphId = covIt.minGlyphId<CoverageFormat>();
  uint32_t maxGlyphId = covIt.maxGlyphId<CoverageFormat>();
  uint32_t substSeqCount = table->sequenceOffsets.count();
  uint32_t maxSeqOffset = uint32_t(table.size) - 2u;

  if (BL_UNLIKELY(table.size < GSubTable::MultipleSubst1::kMinSize + substSeqCount * 2u))
    return ctx.advance(itemCount);

  uint32_t* inPtr = ctx.in.glyphData + ctx.in.index;
  uint32_t* inEnd = ctx.in.glyphData + ctx.in.end;

  // Used to mark the first unmatched glyph that will be copied to output buffer.
  size_t unmatchedStart = ctx.in.index;

  // Required for match.
  uint32_t glyphId;
  uint32_t coverageIndex;
  uint32_t seqOffset;
  uint32_t seqLength;

  // Detects the first substitution to be done. If there is no substitution to be done then we won't force
  // the context to allocate the output buffer.
  while (inPtr != inEnd) {
    glyphId = inPtr[0];
    if (glyphId >= minGlyphId && glyphId <= maxGlyphId) {
      if (covIt.find<CoverageFormat>(glyphId, coverageIndex) || coverageIndex >= substSeqCount) {
        seqOffset = table->sequenceOffsets.array()[coverageIndex].value();
        if (BL_LIKELY(seqOffset <= maxSeqOffset)) {
          seqLength = BLMemOps::readU16uBE(table.data + seqOffset);
          if (BL_LIKELY(seqLength && seqOffset + seqLength * 2u <= maxSeqOffset)) {
            // This makes sure we have the output buffer allocated.
            BL_PROPAGATE(ctx.prepareOut(itemCount + seqLength));
            goto HaveMatch;
          }
        }
      }
    }

    inPtr++;
  }

  // No match at all.
  return ctx.advance(itemCount);

  // Second loop - only executed if there is at least one match.
  while (inPtr != inEnd) {
    glyphId = inPtr[0];
    if (glyphId >= minGlyphId && glyphId <= maxGlyphId) {
      if (covIt.find<CoverageFormat>(glyphId, coverageIndex) || coverageIndex >= substSeqCount) {
        seqOffset = table->sequenceOffsets.array()[coverageIndex].value();
        if (BL_LIKELY(seqOffset <= maxSeqOffset)) {
          seqLength = BLMemOps::readU16uBE(table.data + seqOffset);
          if (BL_LIKELY(seqLength && seqOffset + seqLength * 2u <= maxSeqOffset)) {
HaveMatch:
            size_t unmatchedSize = (size_t)(inPtr - ctx.in.glyphData) - unmatchedStart;
            size_t requiredSize = unmatchedSize + seqLength;

            if (ctx.outRemaining() < requiredSize)
              BL_PROPAGATE(ctx.prepareOut(requiredSize));

            const BLGlyphInfo* inInfo = ctx.in.infoData + unmatchedStart;
            uint32_t* outPtr = ctx.out.glyphData + ctx.out.index;
            BLGlyphInfo* outInfo = ctx.out.infoData + ctx.out.index;

            // Copy the unmatched data.
            blCopyGlyphData(outPtr, outInfo, ctx.in.glyphData + unmatchedStart, inInfo, unmatchedSize);

            inInfo += unmatchedSize;
            outPtr += unmatchedSize;
            outInfo += unmatchedSize;
            ctx.out.index += unmatchedSize;

            // Copy the substitution.
            const UInt16* seq = reinterpret_cast<const UInt16*>(table.data + seqOffset + 2u);
            for (uint32_t i = 0; i < seqLength; i++) {
              uint32_t value = seq->value();

              *outPtr++ = value;
              *outInfo++ = *inInfo;
            }

            unmatchedStart += unmatchedSize + 1;
          }
        }
      }
    }

    inPtr++;
  }

  return BL_SUCCESS;
}

// OpenType::LayoutImpl - GSUB - Lookup Type #3
// ============================================

// Alternate Substitution
// ----------------------
//
// Replace a single glyph by an alternative glyph. The 'cmap' table contains the default mapping, which is then changed
// by alternate substitution based on features selected by the user.

static BL_INLINE bool checkGSubLookupType3Format1(Validator* self, Trace trace, BLFontTable table) noexcept {
  uint32_t countCoverageEntries;
  if (BL_UNLIKELY(!checkLookupWithCoverage(self, trace, table, sizeof(GSubTable::AlternateSubst1), countCoverageEntries)))
    return false;

  const GSubTable::AlternateSubst1* lookup = table.dataAs<GSubTable::AlternateSubst1>();
  uint32_t altSetCount = lookup->altSetOffsets.count();
  size_t headerSize = sizeof(GSubTable::AlternateSubst1) + altSetCount * 2u;

  if (BL_UNLIKELY(table.size < headerSize))
    return trace.fail("Table is too small [Size=%zu Required=%zu]\n", table.size, headerSize);

  // Offsets to AlternateSet tables.
  const Offset16* offsetArray = lookup->altSetOffsets.array();
  size_t endOffset = table.size - 4u;

  for (uint32_t i = 0; i < altSetCount; i++) {
    uint32_t alternateSetOffset = offsetArray[i].value();

    if (BL_UNLIKELY(alternateSetOffset < headerSize || alternateSetOffset > endOffset))
      return trace.fail("AlternateSet #%u [%u] is out of range [%zu:%zu]\n", i, alternateSetOffset, headerSize, endOffset);

    const Array16<UInt16>* alternateSet = BLPtrOps::offset<const Array16<UInt16>>(table.data, alternateSetOffset);
    uint32_t alternateSetLength = alternateSet->count();

    // Specification forbids an empty AlternateSet.
    if (BL_UNLIKELY(!alternateSetLength))
      return trace.fail("AlternateSet #%u [%u] is empty, which is not allowed\n", i);

    uint32_t alternateSetEnd = alternateSetOffset + 2u + alternateSetLength * 2u;
    if (BL_UNLIKELY(alternateSetEnd > table.size))
      return trace.fail("AlternateSet #%u [%u] requires [%u] bytes of data, but only [%zu] bytes are available\n", i, alternateSetLength, size_t(table.size - alternateSetOffset));
  }

  return true;
}

template<uint32_t CoverageFormat>
static BL_INLINE BLResult applyGSubLookupType3Format1(const OTFaceImpl* faceI, GSubContext& ctx, BLFontTableT<GSubTable::AlternateSubst1> table, uint32_t lookupFlags, CoverageIterator& covIt) noexcept {
  size_t itemCount = ctx.in.end - ctx.in.index;

  uint32_t minGlyphId = covIt.minGlyphId<CoverageFormat>();
  uint32_t maxGlyphId = covIt.maxGlyphId<CoverageFormat>();
  uint32_t altSetCount = table->altSetOffsets.count();
  uint32_t maxAltSetOffset = uint32_t(table.size) - 2u;

  // TODO: [OPENTYPE GSUB] Not sure how the index should be selected.
  uint32_t selectedIndex = 0u;

  if (BL_UNLIKELY(table.size < GSubTable::AlternateSubst1::kMinSize + altSetCount * 2u))
    return ctx.advance(itemCount);

  if (ctx.inPlace() && ctx.isSameIndex()) {
    uint32_t* ptr = ctx.in.glyphData + ctx.in.index;
    uint32_t* end = ctx.in.glyphData + ctx.in.end;

    while (ptr != end) {
      uint32_t glyphId = ptr[0];

      if (glyphId >= minGlyphId && glyphId <= maxGlyphId) {
        uint32_t coverageIndex;
        if (covIt.find<CoverageFormat>(glyphId, coverageIndex) || coverageIndex >= altSetCount) {
          uint32_t altSetOffset = table->altSetOffsets.array()[coverageIndex].value();
          if (BL_LIKELY(altSetOffset <= maxAltSetOffset)) {
            const UInt16* alts = reinterpret_cast<const UInt16*>(table.data + altSetOffset + 2u);
            uint32_t altGlyphsCount = alts[-1].value();
            if (BL_LIKELY(altGlyphsCount && altSetOffset + altGlyphsCount * 2u <= maxAltSetOffset)) {
              uint32_t altGlyphIndex = (selectedIndex % altGlyphsCount);
              ptr[0] = alts[altGlyphIndex].value();
            }
          }
        }
      }

      ptr++;
    }
  }
  else {
    if (!ctx.inPlace())
      BL_PROPAGATE(ctx.prepareOut(itemCount));

    uint32_t* inPtr = ctx.in.glyphData + ctx.in.index;
    uint32_t* inEnd = ctx.in.glyphData + ctx.in.end;
    BLGlyphInfo* inInfo = ctx.in.infoData + ctx.in.index;

    uint32_t* outPtr = ctx.out.glyphData + ctx.out.index;
    BLGlyphInfo* outInfo = ctx.out.infoData + ctx.out.index;

    while (inPtr != inEnd) {
      uint32_t glyphId = inPtr[0];

      if (glyphId >= minGlyphId && glyphId <= maxGlyphId) {
        uint32_t coverageIndex;
        if (covIt.find<CoverageFormat>(glyphId, coverageIndex) || coverageIndex >= altSetCount) {
          uint32_t altSetOffset = table->altSetOffsets.array()[coverageIndex].value();
          if (BL_LIKELY(altSetOffset <= maxAltSetOffset)) {
            const UInt16* alts = reinterpret_cast<const UInt16*>(table.data + altSetOffset + 2u);
            uint32_t altGlyphsCount = alts[-1].value();
            if (BL_LIKELY(altGlyphsCount && altSetOffset + altGlyphsCount * 2u <= maxAltSetOffset)) {
              uint32_t altGlyphIndex = (selectedIndex % altGlyphsCount);
              glyphId = alts[altGlyphIndex].value();
            }
          }
        }
      }

      *outPtr++ = glyphId;
      *outInfo++ = *inInfo;

      inPtr++;
      inInfo++;
    }
  }

  ctx.in.index += itemCount;
  ctx.out.index += itemCount;
  return BL_SUCCESS;
}

// OpenType::LayoutImpl - GSUB - Lookup Type #4
// ============================================

// Ligature Substitution
// ---------------------
//
// Replace multiple glyphs by a single glyph.

static BL_INLINE bool checkGSubLookupType4Format1(Validator* self, Trace trace, BLFontTable table) noexcept {
  uint32_t countCoverageEntries;
  if (BL_UNLIKELY(!checkLookupWithCoverage(self, trace, table, sizeof(GSubTable::LigatureSubst1), countCoverageEntries)))
    return false;

  const GSubTable::LigatureSubst1* lookup = table.dataAs<GSubTable::LigatureSubst1>();
  uint32_t ligatureSetCount = lookup->ligSetOffsets.count();
  size_t headerSize = sizeof(GSubTable::LigatureSubst1) + ligatureSetCount * 2u;

  if (BL_UNLIKELY(table.size < headerSize))
    return trace.fail("Table is too small [Size=%zu Required=%zu]\n", table.size, headerSize);

  // Offsets to LigatureSet tables.
  const Offset16* ligatureSetOffsetArray = lookup->ligSetOffsets.array();
  size_t ligatureSetOffsetEnd = table.size - 4u;

  for (uint32_t i = 0; i < ligatureSetCount; i++) {
    uint32_t ligatureSetOffset = ligatureSetOffsetArray[i].value();

    if (BL_UNLIKELY(ligatureSetOffset < headerSize || ligatureSetOffset > ligatureSetOffsetEnd))
      return trace.fail("LigatureSet #%u [%u] is out of range [%zu:%zu]\n", i, ligatureSetOffset, headerSize, ligatureSetOffsetEnd);

    const Array16<UInt16>* ligatureSet = BLPtrOps::offset<const Array16<UInt16>>(table.data, ligatureSetOffset);
    uint32_t ligatureCount = ligatureSet->count();

    // Specification forbids an empty LigatureSet.
    if (BL_UNLIKELY(!ligatureCount))
      return trace.fail("LigatureSet #%u [%u] is empty, which is not allowed\n", i);

    uint32_t ligatureSetEnd = ligatureSetOffset + 2u + ligatureCount * 2u;
    if (BL_UNLIKELY(ligatureSetEnd > table.size))
      return trace.fail("LigatureSet #%u [%u] count of Ligatures [%u] overflows the table size by [%zu] bytes\n", i, ligatureCount, size_t(table.size - ligatureSetEnd));

    const Offset16* ligatureOffsetArray = ligatureSet->array();
    for (uint32_t ligatureIndex = 0; ligatureIndex < ligatureCount; ligatureIndex++) {
      uint32_t ligatureOffset = ligatureSetOffset + ligatureOffsetArray[ligatureIndex].value();

      if (BL_UNLIKELY(ligatureOffset < ligatureSetEnd || ligatureOffset > ligatureSetOffsetEnd))
        return trace.fail("LigatureSet #%u: Ligature #%u [%u] is out of range [%zu:%zu]\n", i, ligatureIndex, ligatureOffset, headerSize, table.size);

      const GSubTable::Ligature* ligature = BLPtrOps::offset<const GSubTable::Ligature>(table.data, ligatureOffset);
      uint32_t componentCount = ligature->glyphs.count();
      if (BL_UNLIKELY(!componentCount))
        return trace.fail("LigatureSet #%u: Ligature #%u is empty\n", i, ligatureIndex);

      uint32_t ligatureDataEnd = ligatureSetOffset + 2u + componentCount * 2u;
      if (BL_UNLIKELY(ligatureDataEnd > table.size))
        return trace.fail("LigatureSet #%u: Ligature #%u overflows the table size by [%zu] bytes\n", i, ligatureIndex, size_t(table.size - ligatureDataEnd));
    }
  }

  return true;
}

static BL_INLINE bool matchLigature(
  BLFontTableT<Array16<Offset16>> ligOffsets,
  uint32_t ligCount,
  const uint32_t* inGlyphData,
  size_t maxGlyphCount,
  uint32_t& ligGlyphIdOut,
  uint32_t& ligGlyphCount) noexcept {

  // Ligatures are ordered by preference. This means we have to go one by one.
  uint32_t maxLigOffset = uint32_t(ligOffsets.size) - 4u;
  size_t maxGlyphCountMinusOne = maxGlyphCount - 1u;

  for (uint32_t ligIndex = 0; ligIndex < ligCount; ligIndex++) {
    uint32_t ligOffset = ligOffsets->array()[ligIndex].value();
    if (BL_UNLIKELY(ligOffset > maxLigOffset))
      break;

    const GSubTable::Ligature* lig = BLPtrOps::offset<const GSubTable::Ligature>(ligOffsets.data, ligOffset);
    ligGlyphCount = uint32_t(lig->glyphs.count()) - 1u;
    if (ligGlyphCount > maxGlyphCountMinusOne)
      continue;

    // This is safe - a single Ligature is 4 bytes + GlyphId[ligGlyphCount - 1]. MaxLigOffset is 4 bytes less than
    // the end to include the header, so we only have to include `ligGlyphCount * 2u` to verify we won't read beyond.
    if (BL_UNLIKELY(ligOffset + ligGlyphCount * 2u > maxLigOffset))
      continue;

    uint32_t glyphIndex = 0;
    for (;;) {
      uint32_t glyphA = lig->glyphs.array()[glyphIndex++].value();
      uint32_t glyphB = inGlyphData[glyphIndex];

      if (glyphA != glyphB)
        break;

      if (glyphIndex < ligGlyphCount)
        continue;

      ligGlyphIdOut = lig->ligatureGlyphId();
      return true;
    }
  }

  return false;
}

template<uint32_t CoverageFormat>
static BL_INLINE BLResult applyGSubLookupType4Format1(const OTFaceImpl* faceI, GSubContext& ctx, BLFontTableT<GSubTable::LigatureSubst1> table, uint32_t lookupFlags, CoverageIterator& covIt) noexcept {
  size_t itemCount = ctx.in.end - ctx.in.index;

  uint32_t minGlyphId = covIt.minGlyphId<CoverageFormat>();
  uint32_t maxGlyphId = covIt.maxGlyphId<CoverageFormat>();
  uint32_t ligSetCount = table->ligSetOffsets.count();
  uint32_t maxLigSetOffset = uint32_t(table.size) - 2u;

  if (BL_UNLIKELY(table.size < GSubTable::LigatureSubst1::kMinSize + ligSetCount * 2u))
    return ctx.advance(itemCount);

  uint32_t* inPtr = ctx.in.glyphData + ctx.in.index;
  uint32_t* inEnd = ctx.in.glyphData + ctx.in.end;

  if (ctx.inPlace() && ctx.isSameIndex()) {
    while (inPtr != inEnd) {
      uint32_t glyphId = inPtr[0];

      if (glyphId >= minGlyphId && glyphId <= maxGlyphId) {
        uint32_t coverageIndex;
        if (covIt.find<CoverageFormat>(glyphId, coverageIndex) || coverageIndex >= ligSetCount) {
          uint32_t ligSetOffset = table->ligSetOffsets.array()[coverageIndex].value();
          if (BL_LIKELY(ligSetOffset <= maxLigSetOffset)) {
            BLFontTableT<Array16<Offset16>> ligOffsets { blFontSubTable(table, ligSetOffset) };
            uint32_t ligCount = ligOffsets->count();
            if (BL_LIKELY(ligCount && ligSetOffset + ligCount * 2u <= maxLigSetOffset)) {
              uint32_t ligGlyphId;
              uint32_t ligGlyphCount;
              if (matchLigature(ligOffsets, ligCount, inPtr, (size_t)(inEnd - inPtr), ligGlyphId, ligGlyphCount)) {
                *inPtr = ligGlyphId;
                inPtr += ligGlyphCount;
                ctx.in.index = (size_t)(inPtr - ctx.in.glyphData);
                ctx.out.index = ctx.in.index;
                goto OutPlace;
              }
            }
          }
        }
      }

      inPtr++;
    }

    ctx.in.index += itemCount;
    ctx.out.index += itemCount;
    return BL_SUCCESS;
  }
  else {
OutPlace:
    uint32_t* outPtr = ctx.out.glyphData + ctx.out.index;

    BLGlyphInfo* inInfo = ctx.in.infoData + ctx.in.index;
    BLGlyphInfo* outInfo = ctx.out.infoData + ctx.out.index;

    while (inPtr != inEnd) {
      uint32_t glyphId = inPtr[0];

      if (glyphId >= minGlyphId && glyphId <= maxGlyphId) {
        uint32_t coverageIndex;
        if (covIt.find<CoverageFormat>(glyphId, coverageIndex) || coverageIndex >= ligSetCount) {
          uint32_t ligSetOffset = table->ligSetOffsets.array()[coverageIndex].value();
          if (BL_LIKELY(ligSetOffset <= maxLigSetOffset)) {
            BLFontTableT<Array16<Offset16>> ligOffsets { blFontSubTable(table, ligSetOffset) };
            uint32_t ligCount = ligOffsets->count();
            if (BL_LIKELY(ligCount && ligSetOffset + ligCount * 2u <= maxLigSetOffset)) {
              uint32_t ligGlyphId;
              uint32_t ligGlyphCount;
              if (matchLigature(ligOffsets, ligCount, inPtr, (size_t)(inEnd - inPtr), ligGlyphId, ligGlyphCount)) {
                *outPtr++ = ligGlyphId;
                *outInfo++ = *inInfo;

                inPtr += ligGlyphCount;
                inInfo += ligGlyphCount;
              }
            }
          }
        }
      }

      *outPtr++ = glyphId;
      *outInfo++ = *inInfo;

      inPtr++;
      inInfo++;
    }

    ctx.in.index = ctx.in.end;
    ctx.out.index = (size_t)(outPtr - ctx.out.glyphData);
    return BL_SUCCESS;
  }
}

// OpenType::LayoutImpl - GSUB - Lookup Type #5
// ============================================

// Contextual Substitution
// -----------------------

template<typename SubstTable>
static BL_INLINE bool checkGSubLookupType5Format1_2(Validator* self, Trace trace, BLFontTable table) noexcept {
  typedef GSubTable::SubRule SubRule;
  typedef GSubTable::SubRuleSet SubRuleSet;

  uint32_t countCoverageEntries;
  if (BL_UNLIKELY(!checkLookupWithCoverage(self, trace, table, sizeof(SubstTable), countCoverageEntries)))
    return false;

  const SubstTable* lookup = table.dataAs<SubstTable>();
  uint32_t subRuleSetCount = lookup->subRuleSetOffsets.count();
  size_t headerSize = sizeof(SubstTable) + subRuleSetCount * 2u;

  if (BL_UNLIKELY(table.size < headerSize))
    return trace.fail("Table is too small [Size=%zu Required=%zu]\n", table.size, headerSize);

  // Offsets to SubRuleSet tables.
  const Offset16* subRuleSetOffsetArray = lookup->subRuleSetOffsets.array();
  size_t subRuleSetOffsetEnd = table.size - 4u;

  for (uint32_t i = 0; i < subRuleSetCount; i++) {
    uint32_t subRuleSetOffset = subRuleSetOffsetArray[i].value();

    if (BL_UNLIKELY(subRuleSetOffset < headerSize || subRuleSetOffset > subRuleSetOffsetEnd))
      return trace.fail("SubRuleSet #%u [%u] is out of range [%zu:%zu]\n", i, subRuleSetOffset, headerSize, subRuleSetOffsetEnd);

    const SubRuleSet* subRuleSet = BLPtrOps::offset<const Array16<UInt16>>(table.data, subRuleSetOffset);
    uint32_t subRuleCount = subRuleSet->count();

    // Specification forbids an empty SubRuleSet.
    if (BL_UNLIKELY(!subRuleCount))
      return trace.fail("SubRuleSet #%u [%u] is empty, which is not allowed\n", i);

    uint32_t subRuleSetEnd = subRuleSetOffset + 2u + subRuleCount * 2u;
    if (BL_UNLIKELY(subRuleSetOffset > table.size))
      return trace.fail("SubRuleSet #%u [%u] count of SubRules [%u] overflows the table size by [%zu] bytes\n", i, subRuleCount, size_t(table.size - subRuleSetEnd));

    const Offset16* subRuleOffsetArray = subRuleSet->array();
    for (uint32_t subRuleIndex = 0; subRuleIndex < subRuleCount; subRuleIndex++) {
      uint32_t subRuleOffset = subRuleSetOffset + subRuleOffsetArray[subRuleIndex].value();

      if (BL_UNLIKELY(subRuleOffset < subRuleSetEnd || subRuleOffset > subRuleSetOffsetEnd))
        return trace.fail("SubRuleSet #%u: SubRule #%u [%u] is out of range [%zu:%zu]\n", i, subRuleIndex, subRuleOffset, headerSize, table.size);

      const SubRule* subRule = BLPtrOps::offset<const SubRule>(table.data, subRuleOffset);
      uint32_t glyphCount = subRule->glyphCount();
      uint32_t substCount = subRule->substCount();

      if (BL_UNLIKELY(glyphCount < 2))
        return trace.fail("SubRuleSet #%u: SubRule #%u has no InputSequence\n", i, subRuleIndex);

      if (BL_UNLIKELY(substCount < 1))
        return trace.fail("SubRuleSet #%u: SubRule #%u has no LookupRecords\n", i, subRuleIndex);

      uint32_t subRuleDataEnd = subRuleSetOffset + 4u + (substCount + glyphCount - 1) * 2u;
      if (BL_UNLIKELY(subRuleDataEnd > table.size))
        return trace.fail("SubRuleSet #%u: SubRule #%u overflows the table size by [%zu] bytes\n", i, subRuleIndex, size_t(table.size - subRuleDataEnd));
    }
  }

  return true;
}

static BL_INLINE bool checkGSubLookupType5Format1(Validator* self, Trace trace, BLFontTable table) noexcept {
  return checkGSubLookupType5Format1_2<GSubTable::ContextSubst1>(self, trace, table);
}

static BL_INLINE bool checkGSubLookupType5Format2(Validator* self, Trace trace, BLFontTable table) noexcept {
  // This is essentially the same as Format1 except that it also provides `ClassDefTable`.
  size_t headerSize = sizeof(GSubTable::ContextSubst2);

  // If the size is smaller it would fail in `checkGSubLookupType5Format1_2()`.
  if (table.size >= headerSize) {
    uint32_t classDefOffset = table.dataAs<GSubTable::ContextSubst2>()->classDefOffset();
    if (classDefOffset < headerSize || classDefOffset > table.size)
      return trace.fail("ClassDefOffset [%u] out of range [%zu:%zu]\n", classDefOffset, headerSize, table.size);

    if (!checkClassDefTable(self, trace, blFontSubTable(table, classDefOffset), "ClassDef"))
      return false;
  }

  return checkGSubLookupType5Format1_2<GSubTable::ContextSubst2>(self, trace, table);
}

static BL_INLINE bool checkGSubLookupType5Format3(Validator* self, Trace trace, BLFontTable table) noexcept {
  // TODO: [OPENTYPE GSUB]
  return true;
}

static BL_INLINE bool matchSubRule(
  BLFontTableT<Array16<Offset16>> subRuleOffsets,
  uint32_t subRuleCount,
  const uint32_t* glyphData,
  uint32_t maxGlyphCount,
  const GSubTable::SubRule** out) noexcept {

  // Ligatures are ordered by preference. This means we have to go one by one.
  uint32_t maxLigOffset = uint32_t(subRuleOffsets.size) - 4u;
  uint32_t maxGlyphCountMinusOne = maxGlyphCount - 1u;

  for (uint32_t subRuleIndex = 0; subRuleIndex < subRuleCount; subRuleIndex++) {
    uint32_t subRuleOffset = subRuleOffsets->array()[subRuleIndex].value();
    if (BL_UNLIKELY(subRuleOffset > maxLigOffset))
      break;

    const GSubTable::SubRule* subRule = BLPtrOps::offset<const GSubTable::SubRule>(subRuleOffsets.data, subRuleOffset);
    uint32_t glyphCount = uint32_t(subRule->glyphCount()) - 1u;
    if (glyphCount > maxGlyphCountMinusOne)
      continue;

    // This is safe - a single SubRule is 4 bytes that is followed by `GlyphId[glyphCount - 1]` and then by
    // `SubstLookupRecord[substCount]`. Since we don't know whether we have a match or not we will only check
    // bounds required by matching postponing `substCount` until we have an actual match.
    if (BL_UNLIKELY(subRuleOffset + glyphCount * 2u > maxLigOffset))
      continue;

    uint32_t glyphIndex = 0;
    for (;;) {
      uint32_t glyphA = subRule->glyphArray()[glyphIndex++].value();
      uint32_t glyphB = glyphData[glyphIndex];

      if (glyphA != glyphB)
        break;

      if (glyphIndex < glyphCount)
        continue;

      // Now check whether the `subRule` is not out of bounds.
      uint32_t substCount = subRule->substCount();
      if (BL_UNLIKELY(!substCount || subRuleOffset + glyphCount * 2u + substCount * 4u > maxLigOffset))
        return false;

      *out = subRule;
      return true;
    }
  }

  return false;
}

template<uint32_t CoverageFormat>
static BL_INLINE BLResult applyGSubLookupType5Format1(const OTFaceImpl* faceI, GSubContext& ctx, BLFontTableT<GSubTable::ContextSubst1> table, uint32_t lookupFlags, CoverageIterator& covIt) noexcept {
  size_t itemCount = ctx.in.end - ctx.in.index;
  return ctx.advance(itemCount);

  /*
  // TODO: [OPENTYPE GSUB]
  uint32_t* glyphData = buf->glyphData;
  uint32_t size = buf->size;

  uint32_t minGlyphId = covIt.minGlyphId<CoverageFormat>();
  uint32_t maxGlyphId = covIt.maxGlyphId<CoverageFormat>();
  uint32_t subRuleSetCount = table->subRuleSetOffsets.count();

  if (BL_UNLIKELY(table.size < GSubTable::LigatureSubst1::kMinSize + subRuleSetCount * 2u))
    return BL_SUCCESS;

  uint32_t i;
  uint32_t maxSubRuleSetOffset = uint32_t(table.size) - 2u;

  for (i = 0; i < size; i++) {
    uint32_t glyphId = glyphData[i];

    if (glyphId >= minGlyphId && glyphId <= maxGlyphId) {
      uint32_t coverageIndex;
      if (covIt.find<CoverageFormat>(glyphId, coverageIndex) || coverageIndex >= subRuleSetCount) {
        uint32_t subRuleSetOffset = table->subRuleSetOffsets.array()[coverageIndex].value();
        if (BL_LIKELY(subRuleSetOffset <= maxSubRuleSetOffset)) {
          BLFontTableT<Array16<Offset16>> subRuleOffsets { blFontSubTable(table, subRuleSetOffset) };
          uint32_t subRuleCount = subRuleOffsets->count();
          if (BL_LIKELY(subRuleCount && subRuleSetOffset + subRuleCount * 2u <= maxSubRuleSetOffset)) {
            const GSubTable::SubRule* subRule;
            if (matchSubRule(subRuleOffsets, subRuleCount, glyphData + i, size - i, &subRule)) {
              continue;
            }
          }
        }
      }
    }
  }
  */
}

// OpenType::LayoutImpl - GSUB - Lookup Type #6
// ============================================

// Chained Contextual Substitution
// -------------------------------

static BL_INLINE bool checkGSubLookupType6Format1(Validator* self, Trace trace, BLFontTable table) noexcept {
  // TODO: [OPENTYPE GSUB]
  return true;
}

static BL_INLINE bool checkGSubLookupType6Format2(Validator* self, Trace trace, BLFontTable table) noexcept {
  // TODO: [OPENTYPE GSUB]
  return true;
}

static BL_INLINE bool checkGSubLookupType6Format3(Validator* self, Trace trace, BLFontTable table) noexcept {
  // TODO: [OPENTYPE GSUB]
  return true;
}

// OpenType::LayoutImpl - GSUB - Lookup Type #8
// ============================================

// Reverse Chained Substitution
// ----------------------------
//
// Similar to "Chained Contextual Substitution", but processed in reverse order.

static BL_INLINE bool checkGSubLookupType8Format1(Validator* self, Trace trace, BLFontTable table) noexcept {
  // TODO: [OPENTYPE GSUB]
  return true;
}

// OpenType::LayoutImpl - GSUB - Lookup Common
// ===========================================

static bool checkGSubLookup(Validator* self, Trace trace, BLFontTable table, uint32_t lookupId) noexcept {
  switch (lookupId) {
    case LookupInfo::kGSubType1Format1: return checkGSubLookupType1Format1(self, trace, table);
    case LookupInfo::kGSubType1Format2: return checkGSubLookupType1Format2(self, trace, table);
    case LookupInfo::kGSubType2Format1: return checkGSubLookupType2Format1(self, trace, table);
    case LookupInfo::kGSubType3Format1: return checkGSubLookupType3Format1(self, trace, table);
    case LookupInfo::kGSubType4Format1: return checkGSubLookupType4Format1(self, trace, table);
    case LookupInfo::kGSubType5Format1: return checkGSubLookupType5Format1(self, trace, table);
    case LookupInfo::kGSubType5Format2: return checkGSubLookupType5Format2(self, trace, table);
    case LookupInfo::kGSubType5Format3: return checkGSubLookupType5Format3(self, trace, table);
    case LookupInfo::kGSubType6Format1: return checkGSubLookupType6Format1(self, trace, table);
    case LookupInfo::kGSubType6Format2: return checkGSubLookupType6Format2(self, trace, table);
    case LookupInfo::kGSubType6Format3: return checkGSubLookupType6Format3(self, trace, table);
    case LookupInfo::kGSubType8Format1: return checkGSubLookupType8Format1(self, trace, table);

    default:
      // Invalid LookupType & Format combination should never pass checks that use LookupInfo.
      BL_NOT_REACHED();
  }
}

static BLResult applyGSubLookup(const OTFaceImpl* faceI, GSubContext& ctx, BLFontTable table, uint32_t lookupId, uint32_t lookupFlags) noexcept {
  #define GSUB_APPLY_COMMON(FN, TABLE) \
    CoverageIterator covIt; \
    switch (covIt.init(blFontSubTableChecked(table, table.dataAs<TABLE>()->coverageOffset()))) { \
      case 1: return FN<1>(faceI, ctx, table, lookupFlags, covIt); \
      case 2: return FN<2>(faceI, ctx, table, lookupFlags, covIt); \
    }

  if (BL_LIKELY(table.size >= gLookupInfo[LookupInfo::kKindGSub].idEntries[lookupId].headerSize)) {
    switch (lookupId) {
      case LookupInfo::kGSubType1Format1: {
        GSUB_APPLY_COMMON(applyGSubLookupType1Format1, GSubTable::LookupHeaderWithCoverage)
        break;
      }

      case LookupInfo::kGSubType1Format2: {
        GSUB_APPLY_COMMON(applyGSubLookupType1Format2, GSubTable::LookupHeaderWithCoverage)
        break;
      }

      case LookupInfo::kGSubType2Format1: {
        GSUB_APPLY_COMMON(applyGSubLookupType2Format1, GSubTable::LookupHeaderWithCoverage)
        break;
      }

      case LookupInfo::kGSubType3Format1: {
        GSUB_APPLY_COMMON(applyGSubLookupType3Format1, GSubTable::LookupHeaderWithCoverage)
        break;
      }

      case LookupInfo::kGSubType4Format1: {
        GSUB_APPLY_COMMON(applyGSubLookupType4Format1, GSubTable::LookupHeaderWithCoverage)
        break;
      }

      /*
      case LookupInfo::kGSubType5Format1: {
        GSUB_APPLY_COMMON(applyGSubLookupType5Format1, GSubTable::LookupHeaderWithCoverage)
        break;
      }
      */
    }
  }

  #undef GSUB_APPLY_COMMON

  ctx.advance(ctx.inRemaining());
  return BL_SUCCESS;
}

// OpenType::LayoutImpl - GPOS - Utilities
// =======================================

template<typename T>
static BL_INLINE const uint8_t* binarySearchGlyphIdInVarStruct(const uint8_t* array, size_t itemSize, size_t arraySize, uint32_t glyphId, size_t offset = 0) noexcept {
  if (!arraySize)
    return nullptr;

  const uint8_t* ptr = array;
  while (size_t half = arraySize / 2u) {
    const uint8_t* middlePtr = ptr + half * itemSize;
    arraySize -= half;
    if (glyphId >= reinterpret_cast<const T*>(middlePtr + offset)->value())
      ptr = middlePtr;
  }

  if (glyphId != reinterpret_cast<const T*>(ptr + offset)->value())
    ptr = nullptr;
  return ptr;
}

static BL_INLINE const Int16* applyGPosValue(const Int16* p, uint32_t valueFormat, BLGlyphPlacement* glyphPlacement) noexcept {
  int32_t v;
  if (valueFormat & GPosTable::kValueXPlacement      ) { v = p->value(); p++; glyphPlacement->placement.x += v; }
  if (valueFormat & GPosTable::kValueYPlacement      ) { v = p->value(); p++; glyphPlacement->placement.y += v; }
  if (valueFormat & GPosTable::kValueXAdvance        ) { v = p->value(); p++; glyphPlacement->advance.x += v; }
  if (valueFormat & GPosTable::kValueYAdvance        ) { v = p->value(); p++; glyphPlacement->advance.y += v; }
  if (valueFormat & GPosTable::kValueXPlacementDevice) { v = p->value(); p++; }
  if (valueFormat & GPosTable::kValueYPlacementDevice) { v = p->value(); p++; }
  if (valueFormat & GPosTable::kValueXAdvanceDevice  ) { v = p->value(); p++; }
  if (valueFormat & GPosTable::kValueYAdvanceDevice  ) { v = p->value(); p++; }
  return p;
}

// OpenType::LayoutImpl - GPOS - Lookup Type #1
// ============================================

// Single Adjustment
// -----------------

static BL_INLINE bool checkGPosLookupType1Format1(Validator* self, Trace trace, BLFontTable table) noexcept {
  uint32_t countCoverageEntries;
  if (BL_UNLIKELY(!checkLookupWithCoverage(self, trace, table, sizeof(GPosTable::SingleAdjustment1), countCoverageEntries)))
    return false;

  const GPosTable::SingleAdjustment1* lookup = table.dataAs<GPosTable::SingleAdjustment1>();
  uint32_t valueRecordSize = sizeOfValueRecordByFormat(lookup->valueFormat());

  size_t headerSize = sizeof(GPosTable::SingleAdjustment1) + valueRecordSize;
  if (BL_UNLIKELY(table.size < headerSize))
    return trace.fail("Table is truncated [Size=%zu Required=%zu]\n", table.size, headerSize);

  return true;
}

template<uint32_t CoverageFormat>
static BL_INLINE BLResult applyGPosLookupType1Format1(const OTFaceImpl* faceI, GPosContext& ctx, BLFontTableT<GPosTable::SingleAdjustment1> table, uint32_t lookupFlags, CoverageIterator& covIt) noexcept {
  size_t index = ctx.index;
  size_t end = ctx.end;

  uint32_t minGlyphId = covIt.minGlyphId<CoverageFormat>();
  uint32_t maxGlyphId = covIt.maxGlyphId<CoverageFormat>();

  uint32_t valueFormat = table->valueFormat();
  if (BL_UNLIKELY(!valueFormat))
    return BL_SUCCESS;

  uint32_t valueRecordSize = sizeOfValueRecordByFormat(valueFormat);
  if (BL_UNLIKELY(table.size < GPosTable::PairAdjustment1::kMinSize + valueRecordSize))
    return BL_SUCCESS;

  uint32_t* glyphData = ctx.glyphData;
  BLGlyphPlacement* placementData = ctx.placementData;

  for (; index < end; index++) {
    uint32_t glyphId = glyphData[index];
    if (glyphId >= minGlyphId && glyphId <= maxGlyphId) {
      uint32_t coverageIndex;
      if (covIt.find<CoverageFormat>(glyphId, coverageIndex)) {
        const Int16* p = reinterpret_cast<const Int16*>(table.data + GPosTable::PairAdjustment1::kMinSize);
        applyGPosValue(p, valueFormat, &placementData[index]);
      }
    }
  }

  return BL_SUCCESS;
}

static BL_INLINE bool checkGPosLookupType1Format2(Validator* self, Trace trace, BLFontTable table) noexcept {
  uint32_t countCoverageEntries;
  if (BL_UNLIKELY(!checkLookupWithCoverage(self, trace, table, sizeof(GPosTable::SingleAdjustment2), countCoverageEntries)))
    return false;

  const GPosTable::SingleAdjustment2* lookup = table.dataAs<GPosTable::SingleAdjustment2>();
  uint32_t valueCount = lookup->valueCount();
  uint32_t valueRecordSize = sizeOfValueRecordByFormat(lookup->valueFormat());

  size_t headerSize = sizeof(GPosTable::SingleAdjustment2) + valueRecordSize * valueCount;
  if (BL_UNLIKELY(table.size < headerSize))
    return trace.fail("Table is truncated [Size=%zu Required=%zu]\n", table.size, headerSize);

  return true;
}

template<uint32_t CoverageFormat>
static BL_INLINE BLResult applyGPosLookupType1Format2(const OTFaceImpl* faceI, GPosContext& ctx, BLFontTableT<GPosTable::SingleAdjustment2> table, uint32_t lookupFlags, CoverageIterator& covIt) noexcept {
  size_t index = ctx.index;
  size_t end = ctx.end;

  uint32_t minGlyphId = covIt.minGlyphId<CoverageFormat>();
  uint32_t maxGlyphId = covIt.maxGlyphId<CoverageFormat>();

  uint32_t valueFormat = table->valueFormat();
  if (BL_UNLIKELY(!valueFormat))
    return BL_SUCCESS;

  uint32_t valueRecordCount = table->valueCount();
  uint32_t valueRecordSize = sizeOfValueRecordByFormat(valueFormat);
  if (BL_UNLIKELY(table.size < GPosTable::PairAdjustment1::kMinSize + valueRecordCount * valueRecordSize))
    return BL_SUCCESS;

  uint32_t* glyphData = ctx.glyphData;
  BLGlyphPlacement* placementData = ctx.placementData;

  for (; index < end; index++) {
    uint32_t glyphId = glyphData[index];
    if (glyphId >= minGlyphId && glyphId <= maxGlyphId) {
      uint32_t coverageIndex;
      if (covIt.find<CoverageFormat>(glyphId, coverageIndex) && coverageIndex < valueRecordCount) {
        const Int16* p = reinterpret_cast<const Int16*>(table.data + GPosTable::PairAdjustment1::kMinSize + coverageIndex * valueRecordSize);
        applyGPosValue(p, valueFormat, &placementData[index]);
      }
    }
  }

  return BL_SUCCESS;
}

// OpenType::LayoutImpl - GPOS - Lookup Type #2
// ============================================

// Pair Adjustment
// ---------------

static BL_INLINE bool checkGPosLookupType2Format1(Validator* self, Trace trace, BLFontTable table) noexcept {
  uint32_t countCoverageEntries;
  if (BL_UNLIKELY(!checkLookupWithCoverage(self, trace, table, sizeof(GPosTable::PairAdjustment1), countCoverageEntries)))
    return false;

  const GPosTable::PairAdjustment1* lookup = table.dataAs<GPosTable::PairAdjustment1>();
  uint32_t pairSetCount = lookup->pairSetOffsets.count();
  uint32_t valueRecordSize = 2u + sizeOfValueRecordByFormat(lookup->valueFormat1()) +
                                  sizeOfValueRecordByFormat(lookup->valueFormat2()) ;

  size_t headerSize = sizeof(GPosTable::PairAdjustment1) + pairSetCount * 2u;
  if (BL_UNLIKELY(table.size < headerSize))
    return trace.fail("Table is truncated [Size=%zu Required=%zu]\n", table.size, headerSize);

  const Offset16* pairSetOffsetArray = lookup->pairSetOffsets.array();
  size_t offsetRangeEnd = table.size - 2u;

  for (uint32_t i = 0; i < pairSetCount; i++) {
    size_t pairSetOffset = pairSetOffsetArray[i].value();
    if (BL_UNLIKELY(pairSetOffset < headerSize || pairSetOffset > offsetRangeEnd))
      return trace.fail("Pair %u: Offset [%zu] is out of range [%zu:%zu]\n", i, pairSetOffset, headerSize, offsetRangeEnd);

    uint32_t valueCount = BLMemOps::readU16uBE(table.data + pairSetOffset);
    uint32_t pairSetSize = valueCount * valueRecordSize;

    if (pairSetSize > table.size - pairSetOffset)
      return trace.fail("Pair #%u of ValueCount [%u] requires [%zu] bytes of data, but only [%zu] bytes are available\n", i, valueCount, pairSetSize, table.size - pairSetOffset);
  }

  return true;
}

template<uint32_t CoverageFormat>
static BL_INLINE BLResult applyGPosLookupType2Format1(const OTFaceImpl* faceI, GPosContext& ctx, BLFontTableT<GPosTable::PairAdjustment1> table, uint32_t lookupFlags, CoverageIterator& covIt) noexcept {
  size_t index = ctx.index;
  size_t end = ctx.end;

  if (end <= 1)
    return BL_SUCCESS;
  end--;

  uint32_t minGlyphId = covIt.minGlyphId<CoverageFormat>();
  uint32_t maxGlyphId = covIt.maxGlyphId<CoverageFormat>();

  uint32_t valueFormat1 = table->valueFormat1();
  uint32_t valueFormat2 = table->valueFormat2();
  uint32_t pairSetOffsetsCount = table->pairSetOffsets.count();

  if (BL_UNLIKELY(table.size < GPosTable::PairAdjustment1::kMinSize + pairSetOffsetsCount * 2u))
    return BL_SUCCESS;

  size_t valueRecordSize = 2u + sizeOfValueRecordByFormat(valueFormat1) +
                                sizeOfValueRecordByFormat(valueFormat2) ;

  uint32_t* glyphData = ctx.glyphData;
  BLGlyphPlacement* placementData = ctx.placementData;

  uint32_t leftGlyphId = glyphData[index];
  uint32_t rightGlyphId = 0;

  for (; index < end; index++, leftGlyphId = rightGlyphId) {
    rightGlyphId = glyphData[index + 1];
    if (leftGlyphId >= minGlyphId && leftGlyphId <= maxGlyphId) {
      uint32_t coverageIndex;
      if (covIt.find<CoverageFormat>(leftGlyphId, coverageIndex) && coverageIndex < pairSetOffsetsCount) {
        uint32_t pairSetOffset = table->pairSetOffsets.array()[coverageIndex].value();
        if (pairSetOffset > table.size - 2u)
          continue;

        const uint8_t* pairSetData = table.data + pairSetOffset;
        uint32_t pairSetCount = reinterpret_cast<const UInt16*>(pairSetData)->value();

        if (pairSetOffset + pairSetCount * valueRecordSize > table.size)
          continue;

        const Int16* p = reinterpret_cast<const Int16*>(
          binarySearchGlyphIdInVarStruct<UInt16>(pairSetData, valueRecordSize, pairSetCount, rightGlyphId));

        if (p) {
          if (valueFormat1) p = applyGPosValue(p + 1, valueFormat1, &placementData[index + 0]);
          if (valueFormat2) p = applyGPosValue(p + 0, valueFormat2, &placementData[index + 1]);
        }
      }
    }
  }

  return BL_SUCCESS;
}

static BL_INLINE bool checkGPosLookupType2Format2(Validator* self, Trace trace, BLFontTableT<GPosTable::PairAdjustment2> table) noexcept {
  uint32_t countCoverageEntries;
  if (BL_UNLIKELY(!checkLookupWithCoverage(self, trace, table, sizeof(GPosTable::PairAdjustment2), countCoverageEntries)))
    return false;

  uint32_t class1Count = table->class1Count();
  uint32_t class2Count = table->class2Count();
  uint32_t valueRecordCount = class1Count * class2Count;

  uint32_t valueFormat1 = table->valueFormat1();
  uint32_t valueFormat2 = table->valueFormat2();
  uint32_t valueRecordSize = sizeOfValueRecordByFormat(valueFormat1) + sizeOfValueRecordByFormat(valueFormat2);

  BLOverflowFlag of = 0;
  size_t headerSize = BLIntOps::addOverflow(uint32_t(sizeof(GPosTable::PairAdjustment2)),
                                    BLIntOps::mulOverflow(valueRecordCount, valueRecordSize, &of), &of);

  if (BL_UNLIKELY(of))
    return trace.fail("Overflow detected when calculating header size [Class1Count=%u Class2Count=%u]\n", class1Count, class2Count);

  if (BL_UNLIKELY(table.size < headerSize))
    return trace.fail("Table is truncated [Size=%zu Required=%zu]\n", table.size, headerSize);

  return true;
}

template<uint32_t CoverageFormat, uint32_t ClassDef1Format, uint32_t ClassDef2Format>
static BL_INLINE BLResult applyGPosLookupType2Format2(const OTFaceImpl* faceI, GPosContext& ctx, BLFontTableT<GPosTable::PairAdjustment2> table, uint32_t lookupFlags, CoverageIterator& covIt, ClassDefIterator& cd1It, ClassDefIterator& cd2It) noexcept {
  size_t index = ctx.index;
  size_t end = ctx.end;

  if (end <= 1)
    return BL_SUCCESS;
  end--;

  uint32_t minGlyphId = covIt.minGlyphId<CoverageFormat>();
  uint32_t maxGlyphId = covIt.maxGlyphId<CoverageFormat>();

  uint32_t valueFormat1 = table->valueFormat1();
  uint32_t valueFormat2 = table->valueFormat2();
  uint32_t valueRecordSize = sizeOfValueRecordByFormat(valueFormat1) + sizeOfValueRecordByFormat(valueFormat2);

  uint32_t class1Count = table->class1Count();
  uint32_t class2Count = table->class2Count();
  uint32_t valueRecordCount = class1Count * class2Count;

  BLOverflowFlag of = 0;
  size_t requiredDataSize = BLIntOps::addOverflow(uint32_t(sizeof(GPosTable::PairAdjustment2)),
                                          BLIntOps::mulOverflow(valueRecordCount, valueRecordSize, &of), &of);

  if (BL_UNLIKELY(table.size < requiredDataSize || of))
    return BL_SUCCESS;

  const uint8_t* valueBasePtr = table.data + sizeof(GPosTable::PairAdjustment2);

  uint32_t* glyphData = ctx.glyphData;
  BLGlyphPlacement* placementData = ctx.placementData;

  uint32_t leftGlyphId = glyphData[index];
  uint32_t rightGlyphId = 0;

  for (; index < end; index++, leftGlyphId = rightGlyphId) {
    rightGlyphId = glyphData[index + 1];
    if (leftGlyphId >= minGlyphId && leftGlyphId <= maxGlyphId) {
      uint32_t coverageIndex;
      if (covIt.find<CoverageFormat>(leftGlyphId, coverageIndex)) {
        uint32_t c1;
        uint32_t c2;

        if (cd1It.find<ClassDef1Format>(leftGlyphId, c1) && cd2It.find<ClassDef2Format>(rightGlyphId, c2)) {
          uint32_t cIndex = c1 * class2Count + c2;
          if (cIndex < valueRecordCount) {
            const Int16* p = BLPtrOps::offset<const Int16>(valueBasePtr, cIndex * valueRecordSize);
            if (valueFormat1) p = applyGPosValue(p, valueFormat1, &placementData[index + 0]);
            if (valueFormat2) p = applyGPosValue(p, valueFormat2, &placementData[index + 1]);
          }
        }
      }
    }
  }

  return BL_SUCCESS;
}

// OpenType::LayoutImpl - GPOS - Lookup Type #3
// ============================================

// Cursive Attachment
// ------------------

static BL_INLINE bool checkGPosLookupType3Format1(Validator* self, Trace trace, BLFontTable table) noexcept {
  uint32_t countCoverageEntries;
  if (BL_UNLIKELY(!checkLookupWithCoverage(self, trace, table, sizeof(GPosTable::CursiveAttachment1), countCoverageEntries)))
    return false;

  const GPosTable::CursiveAttachment1* lookup = table.dataAs<GPosTable::CursiveAttachment1>();
  uint32_t entryExitCount = lookup->entryExits.count();

  size_t headerSize = sizeof(GPosTable::CursiveAttachment1) + entryExitCount * sizeof(GPosTable::EntryExit);
  if (BL_UNLIKELY(table.size < headerSize))
    return trace.fail("Table is truncated [Size=%zu Required=%zu]\n", table.size, headerSize);

  return true;
}

// OpenType::LayoutImpl - GPOS - Lookup Type #4
// ============================================

// MarkToBase Attachment
// ---------------------

static BL_INLINE bool checkGPosLookupType4Format1(Validator* self, Trace trace, BLFontTable table) noexcept {
  // TODO: [OPENTYPE GPOS]
  return true;
}

// OpenType::LayoutImpl - GPOS - Lookup Type #5
// ============================================

// MarkToLigature Attachment
// -------------------------

static BL_INLINE bool checkGPosLookupType5Format1(Validator* self, Trace trace, BLFontTable table) noexcept {
  // TODO: [OPENTYPE GPOS]
  return true;
}

// OpenType::LayoutImpl - GPOS - Lookup Type #6
// ============================================

// MarkToMark Attachment
// ---------------------

static BL_INLINE bool checkGPosLookupType6Format1(Validator* self, Trace trace, BLFontTable table) noexcept {
  // TODO: [OPENTYPE GPOS]
  return true;
}

// OpenType::LayoutImpl - GPOS - Lookup Type #7
// ============================================

// Contextual Positioning
// ----------------------

static BL_INLINE bool checkGPosLookupType7Format1(Validator* self, Trace trace, BLFontTable table) noexcept {
  // TODO: [OPENTYPE GPOS]
  return true;
}

static BL_INLINE bool checkGPosLookupType7Format2(Validator* self, Trace trace, BLFontTable table) noexcept {
  // TODO: [OPENTYPE GPOS]
  return true;
}

static BL_INLINE bool checkGPosLookupType7Format3(Validator* self, Trace trace, BLFontTable table) noexcept {
  // TODO: [OPENTYPE GPOS]
  return true;
}

// OpenType::LayoutImpl - GPOS - Lookup Type #8
// ============================================

// Chained Contextual Positioning
// ------------------------------

static BL_INLINE bool checkGPosLookupType8Format1(Validator* self, Trace trace, BLFontTable table) noexcept {
  // TODO: [OPENTYPE GPOS]
  return true;
}

static BL_INLINE bool checkGPosLookupType8Format2(Validator* self, Trace trace, BLFontTable table) noexcept {
  // TODO: [OPENTYPE GPOS]
  return true;
}

static BL_INLINE bool checkGPosLookupType8Format3(Validator* self, Trace trace, BLFontTable table) noexcept {
  // TODO: [OPENTYPE GPOS]
  return true;
}

// OpenType::LayoutImpl - GPOS - Lookup Common
// ===========================================

static BL_INLINE bool checkGPosLookup(Validator* self, Trace trace, BLFontTable table, uint32_t lookupId) noexcept {
  switch (lookupId) {
    case LookupInfo::kGPosType1Format1: return checkGPosLookupType1Format1(self, trace, table);
    case LookupInfo::kGPosType1Format2: return checkGPosLookupType1Format2(self, trace, table);
    case LookupInfo::kGPosType2Format1: return checkGPosLookupType2Format1(self, trace, table);
    case LookupInfo::kGPosType2Format2: return checkGPosLookupType2Format2(self, trace, table);
    case LookupInfo::kGPosType3Format1: return checkGPosLookupType3Format1(self, trace, table);
    case LookupInfo::kGPosType4Format1: return checkGPosLookupType4Format1(self, trace, table);
    case LookupInfo::kGPosType5Format1: return checkGPosLookupType5Format1(self, trace, table);
    case LookupInfo::kGPosType6Format1: return checkGPosLookupType6Format1(self, trace, table);
    case LookupInfo::kGPosType7Format1: return checkGPosLookupType7Format1(self, trace, table);
    case LookupInfo::kGPosType7Format2: return checkGPosLookupType7Format2(self, trace, table);
    case LookupInfo::kGPosType7Format3: return checkGPosLookupType7Format3(self, trace, table);
    case LookupInfo::kGPosType8Format1: return checkGPosLookupType8Format1(self, trace, table);
    case LookupInfo::kGPosType8Format2: return checkGPosLookupType8Format2(self, trace, table);
    case LookupInfo::kGPosType8Format3: return checkGPosLookupType8Format3(self, trace, table);

    default:
      // Invalid LookupType & Format combination should never pass checks that use LookupInfo.
      BL_NOT_REACHED();
  }
}

// TODO: [OPENTYPE GPOS]
static BLResult applyGPosLookup(const OTFaceImpl* faceI, GPosContext& ctx, BLFontTable table, uint32_t lookupId, uint32_t lookupFlags) noexcept {
  // Artificial format that we use here, instead of using 1-2 we use 0-1 as it's easier to write a switch in case
  // that multiple tables with different format options can be used by a lookup.
  enum Format : uint32_t {
    kFmt1 = 0,
    kFmt2 = 1
  };

  #define GPOS_APPLY_COMMON(FN, TABLE) \
    CoverageIterator covIt; \
    switch (covIt.init(blFontSubTableChecked(table, table.dataAs<TABLE>()->coverageOffset()))) { \
      case 1: return FN<1>(faceI, ctx, table, lookupFlags, covIt); \
      case 2: return FN<2>(faceI, ctx, table, lookupFlags, covIt); \
    }

  if (BL_LIKELY(table.size >= gLookupInfo[LookupInfo::kKindGPos].idEntries[lookupId].headerSize)) {
    switch (lookupId) {
      case LookupInfo::kGPosType1Format1: {
        GPOS_APPLY_COMMON(applyGPosLookupType1Format1, GPosTable::SingleAdjustment1)
        break;
      }

      case LookupInfo::kGPosType1Format2: {
        GPOS_APPLY_COMMON(applyGPosLookupType1Format2, GPosTable::SingleAdjustment2)
        break;
      }

      case LookupInfo::kGPosType2Format1: {
        GPOS_APPLY_COMMON(applyGPosLookupType2Format1, GPosTable::PairAdjustment1)
        break;
      }

      case LookupInfo::kGPosType2Format2: {
        CoverageIterator covIt;
        ClassDefIterator cd1It;
        ClassDefIterator cd2It;

        uint32_t bits = ((covIt.init(blFontSubTableChecked(table, table.dataAs<GPosTable::PairAdjustment2>()->coverageOffset()))  - 1u) << 2) |
                        ((cd1It.init(blFontSubTableChecked(table, table.dataAs<GPosTable::PairAdjustment2>()->classDef1Offset())) - 1u) << 1) |
                        ((cd2It.init(blFontSubTableChecked(table, table.dataAs<GPosTable::PairAdjustment2>()->classDef2Offset())) - 1u) << 0);

        switch (bits) {
          case ((kFmt1 << 2) | (kFmt1 << 1) | (kFmt1 << 0)): return applyGPosLookupType2Format2<1, 1, 1>(faceI, ctx, table, lookupFlags, covIt, cd1It, cd2It);
          case ((kFmt1 << 2) | (kFmt1 << 1) | (kFmt2 << 0)): return applyGPosLookupType2Format2<1, 1, 2>(faceI, ctx, table, lookupFlags, covIt, cd1It, cd2It);
          case ((kFmt1 << 2) | (kFmt2 << 1) | (kFmt1 << 0)): return applyGPosLookupType2Format2<1, 2, 1>(faceI, ctx, table, lookupFlags, covIt, cd1It, cd2It);
          case ((kFmt1 << 2) | (kFmt2 << 1) | (kFmt2 << 0)): return applyGPosLookupType2Format2<1, 2, 2>(faceI, ctx, table, lookupFlags, covIt, cd1It, cd2It);
          case ((kFmt2 << 2) | (kFmt1 << 1) | (kFmt1 << 0)): return applyGPosLookupType2Format2<2, 1, 1>(faceI, ctx, table, lookupFlags, covIt, cd1It, cd2It);
          case ((kFmt2 << 2) | (kFmt1 << 1) | (kFmt2 << 0)): return applyGPosLookupType2Format2<2, 1, 2>(faceI, ctx, table, lookupFlags, covIt, cd1It, cd2It);
          case ((kFmt2 << 2) | (kFmt2 << 1) | (kFmt1 << 0)): return applyGPosLookupType2Format2<2, 2, 1>(faceI, ctx, table, lookupFlags, covIt, cd1It, cd2It);
          case ((kFmt2 << 2) | (kFmt2 << 1) | (kFmt2 << 0)): return applyGPosLookupType2Format2<2, 2, 2>(faceI, ctx, table, lookupFlags, covIt, cd1It, cd2It);
        }
        break;
      }
    }

    // TODO: [OPENTYPE GPOS]
  }

  #undef GPOS_APPLY_COMMON

  return BL_SUCCESS;
}

// OpenType::LayoutImpl - GPOS & GSUB - Init
// =========================================

static const char* lookupTypeAsString(uint32_t kind, uint32_t lookupType) noexcept {
  if (kind == LookupInfo::kKindGPos) {
    switch (lookupType) {
      case 1: return "SingleAdjustment";
      case 2: return "PairAdjustment";
      case 3: return "CursiveAdjustment";
      case 4: return "MarkToBaseAttachment";
      case 5: return "MarkToLigatureAttachment";
      case 6: return "MarkToMarkAttachment";
      case 7: return "ContextPositioning";
      case 8: return "ChainedContextPositioning";
      case 9: return "Extension";
    }
  }
  else {
    switch (lookupType) {
      case 1: return "SingleSubstitution";
      case 2: return "MultipleSubstitution";
      case 3: return "AlternateSubstitution";
      case 4: return "LigatureSubstitution";
      case 5: return "ContextSubstitution";
      case 6: return "ChainedContextSubstitution";
      case 7: return "Extension";
      case 8: return "ReverseChainedContextSubstitution";
    }
  }
  return "Unknown";
}

static bool checkLookupTable(Validator* self, Trace trace, uint32_t kind, BLFontTableT<GAnyTable::LookupTable> table, uint32_t lookupIndex) noexcept {
  trace.info("LookupTable #%u\n", lookupIndex);
  trace.indent();

  if (BL_UNLIKELY(!blFontTableFitsT<GAnyTable::LookupTable>(table)))
    return trace.fail("Table is too small [Size=%zu]\n", table.size);

  uint32_t lookupType = table->lookupType();
  uint32_t lookupFlags = table->lookupFlags();

  uint32_t offsetCount = table->lookupOffsets.count();
  size_t headerSize = 6u + offsetCount * 2u + (lookupFlags & GAnyTable::LookupTable::kFlagUseMarkFilteringSet ? 2u : 0u);

  trace.info("LookupType: %u (%s)\n", lookupType, lookupTypeAsString(kind, lookupType));
  trace.info("LookupFlags: 0x%02X\n", lookupFlags & 0xFFu);
  trace.info("MarkAttachmentType: %u\n", lookupFlags >> 8);

  bool isExtension = (lookupType == gLookupInfo[kind].extensionType);
  if (BL_UNLIKELY(lookupType - 1u >= gLookupInfo[kind].lookupCount))
    return trace.fail("Invalid lookup type [%u]\n", lookupType);

  if (BL_UNLIKELY(table.size < headerSize))
    return trace.fail("Table is truncated [Size=%zu Required=%zu]\n", table.size, headerSize);

  const Offset16* offsetArray = table->lookupOffsets.array();
  uint32_t& lookupTypes = self->faceI->layout.kinds[kind].lookupTypes;

  // TODO: WHY?
  // BLArray<LayoutData::LookupEntry>& lookupEntries = self->faceI->layout.lookupEntries[kind];

  const LookupInfo::TypeEntry& lookupTypeInfo = gLookupInfo[kind].typeEntries[lookupType];
  size_t lookupTableEnd = table.size - 2u;

  for (uint32_t i = 0; i < offsetCount; i++) {
    uint32_t offset = offsetArray[i].value();

    trace.info("Lookup #%u [%u]\n", i, offset);
    trace.indent();

    if (BL_UNLIKELY(offset < headerSize || offset > lookupTableEnd))
      return trace.fail("Invalid offset #%u [%u], valid range [%zu:%zu]\n", i, offset, headerSize, lookupTableEnd);

    BLFontTableT<GSubTable::LookupHeader> header { blFontSubTable(table, offset) };
    uint32_t lookupFormat = header->format();

    trace.info("LookupFormat: %u\n", lookupFormat);
    if (BL_UNLIKELY(lookupFormat - 1u >= lookupTypeInfo.formatCount))
      return trace.fail("Invalid format [%u]\n", lookupFormat);

    uint32_t lookupId = lookupTypeInfo.lookupIdIndex + lookupFormat - 1u;
    if (isExtension) {
      if (BL_UNLIKELY(header.size < sizeof(GAnyTable::ExtensionLookup)))
        return trace.fail("Extension data too small [%zu]\n", header.size);

      uint32_t extensionLookupType = header.dataAs<GAnyTable::ExtensionLookup>()->lookupType();
      trace.info("ExtensionLookupType: %u (%s)\n", extensionLookupType, lookupTypeAsString(kind, extensionLookupType));

      if (BL_UNLIKELY(extensionLookupType - 1u >= gLookupInfo[kind].lookupCount))
        return trace.fail("Invalid extension LookupType [%u]\n", extensionLookupType);

      if (BL_UNLIKELY(extensionLookupType == gLookupInfo[kind].extensionType))
        return trace.fail("Extension's LookupType cannot be Extension\n");

      const LookupInfo::TypeEntry& extensionLookupTypeInfo = gLookupInfo[kind].typeEntries[extensionLookupType];
      uint32_t extensionOffset = header.dataAs<GAnyTable::ExtensionLookup>()->offset();

      if (extensionOffset > header.size - 2u)
        return trace.fail("Invalid extension offset [%u], data ends at [%zu]\n", extensionOffset, header.size);

      header = blFontSubTable(header, extensionOffset);
      lookupFormat = header->format();

      if (BL_UNLIKELY(lookupFormat - 1u >= extensionLookupTypeInfo.formatCount))
        return trace.fail("Invalid extension format [%u]\n", lookupFormat);

      lookupId = extensionLookupTypeInfo.lookupIdIndex + lookupFormat - 1u;
    }

    bool result = (kind == LookupInfo::kKindGSub) ? checkGSubLookup(self, trace, header, lookupId)
                                                  : checkGPosLookup(self, trace, header, lookupId);
    if (BL_UNLIKELY(!result))
      return false;

    lookupTypes |= 1u << lookupType;
    trace.deindent();
  }

  return true;
}

static bool checkFeatureTable(Validator* self, Trace trace, uint32_t kind, BLFontTableT<GAnyTable::FeatureTable> table, uint32_t index, uint32_t tag) noexcept {
  char tagString[5];
  BLFontTagsPrivate::tagToAscii(tagString, tag);

  trace.info("FeatureTable #%u '%s'\n", index, tagString);
  trace.indent();

  if (BL_UNLIKELY(!blFontTableFitsT<GAnyTable::FeatureTable>(table)))
    return trace.fail("Table is too small [Size=%zu]\n", table.size);

  uint32_t featureParamsOffset = table->featureParamsOffset();
  uint32_t lookupListCount = table->lookupListIndexes.count();

  // TOOD: Unfinished
  blUnused(featureParamsOffset);

  size_t headerSize = 4u + lookupListCount * 2u;
  if (BL_UNLIKELY(table.size < headerSize))
    return trace.fail("Table is truncated [Size=%zu Required=%zu]\n", table.size, headerSize);

  const UInt16* lookupListIndexes = table->lookupListIndexes.array();
  uint32_t totalLookupCount = self->faceI->layout.kinds[kind].lookupCount;

  for (uint32_t i = 0; i < lookupListCount; i++) {
    uint32_t lookupListIndex = lookupListIndexes[i].value();
    trace.info("Entry #%u -> LookupTable #%u\n", i, lookupListIndex);

    if (lookupListIndex >= totalLookupCount)
      return trace.fail("LookupTable #%u is out of bounds [Count=%u]\n", lookupListIndex, totalLookupCount);
  }

  return true;
}

static bool checkScriptTable(Validator* self, Trace trace, uint32_t kind, BLFontTableT<GAnyTable::ScriptTable> table, uint32_t index, uint32_t tag) noexcept {
  char tagString[5];
  BLFontTagsPrivate::tagToAscii(tagString, tag);

  trace.info("ScriptTable #%u '%s'\n", index, tagString);
  trace.indent();

  if (BL_UNLIKELY(!blFontTableFitsT<GAnyTable::ScriptTable>(table)))
    return trace.fail("Table is too small [Size=%zu]\n", table.size);

  uint32_t langSysCount = table->langSysOffsets.count();
  uint32_t langSysDefault = table->langSysDefault();

  // TODO: Not used.
  blUnused(langSysDefault);

  size_t headerSize = 4u + langSysCount * 2u;
  if (BL_UNLIKELY(table.size < headerSize))
    return trace.fail("Table is truncated [Size=%zu Required=%zu]\n", table.size, headerSize);

  const TagRef16* langSysOffsetArray = table->langSysOffsets.array();
  uint32_t totalFeatureCount = self->faceI->layout.kinds[kind].featureCount;

  for (uint32_t i = 0; i < langSysCount; i++) {
    uint32_t langSysTag = langSysOffsetArray[i].tag();
    uint32_t langSysOffset = langSysOffsetArray[i].offset();

    BLFontTagsPrivate::tagToAscii(tagString, langSysTag);
    trace.info("LangSys #%u '%s' [%u]\n", i, tagString, langSysOffset);
    trace.indent();

    if (langSysOffset < headerSize || langSysOffset > table.size - GAnyTable::LangSysTable::kMinSize)
      return trace.fail("Offset [%u] out of range [%zu:%zu]\n", langSysOffset, headerSize, table.size);

    BLFontTableT<GAnyTable::LangSysTable> langSys { blFontSubTable(table, langSysOffset) };

    uint32_t lookupOrderOffset = langSys->lookupOrderOffset();
    uint32_t requiredFeatureIndex = langSys->requiredFeatureIndex();
    uint32_t featureIndexCount = langSys->featureIndexes.count();

    // TODO: Not used.
    blUnused(lookupOrderOffset);

    size_t langSysTableSize = (GAnyTable::LangSysTable::kMinSize) + featureIndexCount * 2u;
    if (langSys.size < langSysTableSize)
      return trace.fail("Table is truncated [Size=%zu Required=%zu]\n", table.size, langSysTableSize);

    if (requiredFeatureIndex != GAnyTable::kFeatureNotRequired && requiredFeatureIndex >= totalFeatureCount)
      return trace.fail("Required Feature Index [%u] is out of bounds [Count=%u]\n", requiredFeatureIndex, totalFeatureCount);

    const UInt16* featureIndexArray = langSys->featureIndexes.array();
    for (uint32_t j = 0; j < featureIndexCount; j++) {
      uint32_t featureIndex = featureIndexArray[j].value();
      if (featureIndex >= totalFeatureCount)
        return trace.fail("Feature #%u index [%u] is out of bounds [Count=%u]\n", j, featureIndex, totalFeatureCount);

      trace.info("Entry #%u -> FeatureIndex #%u\n", j, featureIndex);
    }

    trace.deindent();
  }

  return true;
}

static bool checkGPosGSubTable(Validator* self, Trace trace, uint32_t kind) noexcept {
  OTFaceImpl* faceI = self->faceI;

  BLFontTableT<GAnyTable> table;
  const char* tableTypeAsString;

  if (kind == LookupInfo::kKindGPos) {
    table = self->gpos;
    tableTypeAsString = "GPOS";
  }
  else {
    table = self->gsub;
    tableTypeAsString = "GSUB";
  }

  trace.info("BLOpenType::Init '%s' [Size=%zu]\n", tableTypeAsString, table.size);
  trace.indent();

  if (BL_UNLIKELY(!blFontTableFitsT<GAnyTable>(table)))
    return trace.fail("Table too small [Size=%zu Required: %zu]\n", table.size, size_t(GAnyTable::kMinSize));

  uint32_t version = table->v1_0()->version();
  size_t headerSize = GPosTable::HeaderV1_0::kMinSize;

  if (version >= 0x00010001u)
    headerSize = GPosTable::HeaderV1_1::kMinSize;

  if (BL_UNLIKELY(version < 0x00010000u || version > 0x00010001u))
    return trace.fail("Invalid version [%u.%u]\n", version >> 16, version & 0xFFFFu);

  if (BL_UNLIKELY(table.size < headerSize))
    return trace.fail("Table is too small [Size=%zu Required=%zu]\n", table.size, headerSize);

  // Validate Offsets
  // ----------------

  uint32_t scriptListOffset  = table->v1_0()->scriptListOffset();
  uint32_t featureListOffset = table->v1_0()->featureListOffset();
  uint32_t lookupListOffset  = table->v1_0()->lookupListOffset();

  if (scriptListOffset == table.size) scriptListOffset = 0;
  if (featureListOffset == table.size) featureListOffset = 0;
  if (lookupListOffset == table.size) lookupListOffset = 0;

  if (lookupListOffset) {
    if (BL_UNLIKELY(lookupListOffset < headerSize || lookupListOffset >= table.size))
      return trace.fail("Invalid LookupList offset [%u]\n", lookupListOffset);

    if (BL_UNLIKELY(!checkRawOffsetArray(self, trace, blFontSubTable(table, lookupListOffset), "LookupList")))
      return false;
  }

  if (featureListOffset) {
    if (BL_UNLIKELY(featureListOffset < headerSize || featureListOffset >= table.size))
      return trace.fail("Invalid FeatureList offset [%u]\n", featureListOffset);

    if (BL_UNLIKELY(!checkTagRef16Array(self, trace, blFontSubTable(table, featureListOffset), "FeatureList")))
      return false;
  }

  if (scriptListOffset) {
    if (BL_UNLIKELY(scriptListOffset < headerSize || scriptListOffset >= table.size))
      return trace.fail("Invalid ScriptList offset [%u]\n", scriptListOffset);

    if (BL_UNLIKELY(!checkTagRef16Array(self, trace, blFontSubTable(table, scriptListOffset), "ScriptList")))
      return false;
  }

  // Validate Tables
  // ---------------

  if (lookupListOffset) {
    BLFontTableT<Array16<Offset16>> lookupListOffsets { blFontSubTable(table, lookupListOffset) };
    uint32_t count = lookupListOffsets->count();

    if (count) {
      const Offset16* array = lookupListOffsets->array();
      for (uint32_t i = 0; i < count; i++) {
        BLFontTableT<GAnyTable::LookupTable> lookupTable { blFontSubTable(lookupListOffsets, array[i].value()) };
        if (!checkLookupTable(self, trace, kind, lookupTable, i))
          return false;
      }

      faceI->layout.kinds[kind].lookupCount = uint16_t(count);
      faceI->layout.kinds[kind].lookupListOffset = uint16_t(lookupListOffset);
    }
  }

  if (featureListOffset) {
    BLFontTableT<Array16<TagRef16>> featureListOffsets { blFontSubTable(table, featureListOffset) };
    uint32_t count = featureListOffsets->count();

    if (count) {
      const TagRef16* array = featureListOffsets->array();
      for (uint32_t i = 0; i < count; i++) {
        uint32_t featureTag = array[i].tag();
        BLFontTableT<GAnyTable::FeatureTable> featureTable { blFontSubTable(featureListOffsets, array[i].offset()) };

        if (!checkFeatureTable(self, trace, kind, featureTable, i, featureTag))
          return false;

        if (self->featureTags.append(featureTag) != BL_SUCCESS)
          return false;
      }

      faceI->layout.kinds[kind].featureCount = uint16_t(count);
      faceI->layout.kinds[kind].featureListOffset = uint16_t(featureListOffset);
    }
  }

  if (scriptListOffset) {
    BLFontTableT<Array16<TagRef16>> scriptListOffsets { blFontSubTable(table, scriptListOffset) };
    uint32_t count = scriptListOffsets->count();

    if (count) {
      const TagRef16* array = scriptListOffsets->array();
      for (uint32_t i = 0; i < count; i++) {
        uint32_t scriptTag = array[i].tag();
        BLFontTableT<GAnyTable::ScriptTable> scriptTable { blFontSubTable(scriptListOffsets, array[i].offset()) };

        if (!checkScriptTable(self, trace, kind, scriptTable, i, scriptTag))
          return false;

        if (self->scriptTags.append(scriptTag) != BL_SUCCESS)
          return false;
      }

      faceI->layout.kinds[kind].scriptListOffset = uint16_t(scriptListOffset);
    }
  }

  return true;
}

// OpenType::LayoutImpl - Apply
// ============================

static BL_INLINE BLResult applyLookup(const OTFaceImpl* faceI, GSubContext& ctx, BLFontTable table, uint32_t lookupId, uint32_t lookupFlags) noexcept {
  return applyGSubLookup(faceI, ctx, table, lookupId, lookupFlags);
}

static BL_INLINE BLResult applyLookup(const OTFaceImpl* faceI, GPosContext& ctx, BLFontTable table, uint32_t lookupId, uint32_t lookupFlags) noexcept {
  return applyGPosLookup(faceI, ctx, table, lookupId, lookupFlags);
}

template<uint32_t Kind, typename Context>
static BLResult BL_CDECL applyLookups(const BLFontFaceImpl* faceI_, BLGlyphBuffer* gb, const BLBitSetCore* lookups) noexcept {
  constexpr bool kIsGSub = (Kind == LookupInfo::kKindGSub);

  constexpr uint32_t kLookupCount     = kIsGSub ? uint32_t(GSubTable::kLookupCount    ) : uint32_t(GPosTable::kLookupCount    );
  constexpr uint32_t kLookupExtension = kIsGSub ? uint32_t(GSubTable::kLookupExtension) : uint32_t(GPosTable::kLookupExtension);

  const OTFaceImpl* faceI = static_cast<const OTFaceImpl*>(faceI_);
  BLFontTable table = faceI->layout.tables[Kind];
  size_t lookupListOffset = faceI->layout.kinds[Kind].lookupListOffset;

  BLFontTableT<Array16<UInt16>> lookupListTable { blFontSubTable(table, lookupListOffset) };
  size_t lookupListEndMinus6 = lookupListTable.size - 6u;
  size_t lookupListTableCount = faceI->layout.kinds[Kind].lookupCount;

  Context ctx;
  ctx.init(blGlyphBufferGetImpl(gb));

  BLBitSetWordIterator wordIterator(*lookups);
  while (uint32_t word = wordIterator.nextWord()) {
    BLBitSetOps::BitIterator it(word);
    while (it.hasNext()) {
      size_t lookupTableIndex = it.next() + wordIterator.bitIndex();
      if (BL_UNLIKELY(lookupTableIndex >= lookupListTableCount))
        return blTraceError(BL_ERROR_INVALID_VALUE);

      size_t lookupTableOffset = lookupListTable->array()[lookupTableIndex].value();
      if (BL_UNLIKELY(lookupTableOffset > lookupListEndMinus6))
        continue;

      BLFontTableT<GAnyTable::LookupTable> lookupTable { blFontSubTable(lookupListTable, lookupTableOffset) };
      uint32_t lookupType = lookupTable->lookupType();
      uint32_t lookupFlags = lookupTable->lookupFlags();

      if (BL_UNLIKELY(lookupType - 1u >= kLookupCount))
        continue;

      uint32_t lookupEntryCount = lookupTable->lookupOffsets.count();
      const Offset16* lookupEntryOffsets = lookupTable->lookupOffsets.array();

      const LookupInfo::TypeEntry& lookupTypeInfo = gLookupInfo[Kind].typeEntries[lookupType];
      size_t lookupTableMinSize = lookupType == kLookupExtension ? 8u : 6u;
      size_t lookupTableEnd = lookupTable.size - lookupTableMinSize;

      // If this doesn't pass it means that the index is out of range.
      if (BL_UNLIKELY(lookupTable.size < lookupTableMinSize + lookupEntryCount * 2u))
        continue;

      for (uint32_t j = 0; j < lookupEntryCount; j++) {
        uint32_t lookupOffset = lookupEntryOffsets[j].value();
        if (BL_UNLIKELY(lookupOffset > lookupTableEnd))
          continue;

        BLFontTableT<GAnyTable::LookupHeader> lookupHeader { blFontSubTable(lookupTable, lookupOffset) };
        uint32_t lookupFormat = lookupHeader->format();

        if (BL_UNLIKELY(lookupFormat - 1u >= lookupTypeInfo.formatCount))
          continue;

        uint32_t lookupId = lookupTypeInfo.lookupIdIndex + lookupFormat - 1u;
        if (lookupType == kLookupExtension) {
          BLFontTableT<GAnyTable::ExtensionLookup> extensionTable { blFontSubTable(lookupTable, lookupOffset) };

          uint32_t extensionLookupType = extensionTable->lookupType();
          uint32_t extensionOffset = extensionTable->offset();

          if (BL_UNLIKELY(extensionLookupType - 1u >= kLookupCount || extensionOffset > extensionTable.size - 6u))
            continue;

          lookupHeader = blFontSubTable(extensionTable, extensionOffset);
          lookupFormat = lookupHeader->format();
          const LookupInfo::TypeEntry& extensionLookupTypeInfo = gLookupInfo[Kind].typeEntries[extensionLookupType];

          if (BL_UNLIKELY(lookupFormat - 1u >= extensionLookupTypeInfo.formatCount))
            continue;

          lookupId = extensionLookupTypeInfo.lookupIdIndex + lookupFormat - 1u;
        }

        BL_PROPAGATE(applyLookup(faceI, ctx, lookupHeader, lookupId, lookupFlags));
      }
    }
  }

  ctx.done();

  return BL_SUCCESS;
}

// OpenType::LayoutImpl - Init
// ===========================

BLResult init(OTFaceImpl* faceI, const BLFontData* fontData) noexcept {
  Trace trace;
  Validator validator(faceI);

  static const BLTag tableTags[3] = {
    BL_MAKE_TAG('G', 'S', 'U', 'B'),
    BL_MAKE_TAG('G', 'P', 'O', 'S'),
    BL_MAKE_TAG('G', 'D', 'E', 'F')
  };

  if (!fontData->queryTables(faceI->faceInfo.faceIndex, validator.tables, tableTags, 3))
    return BL_SUCCESS;

  if (validator.gdef.data) {
    if (BL_UNLIKELY(!checkGDefTable(&validator, trace))) {
      faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_GDEF_DATA;
      return BL_SUCCESS;
    }
    faceI->layout.tables[2] = validator.tables[2];
  }

  if (validator.gsub.data) {
    if (BL_UNLIKELY(!checkGPosGSubTable(&validator, trace, LookupInfo::kKindGSub))) {
      faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_GSUB_DATA;
      return BL_SUCCESS;
    }

    if (faceI->layout.gsub.lookupCount)
      faceI->funcs.applyGSub = applyLookups<LookupInfo::kKindGSub, GSubContext>;
    faceI->layout.tables[0] = validator.tables[0];
  }

  if (validator.gpos.data) {
    if (BL_UNLIKELY(!checkGPosGSubTable(&validator, trace, LookupInfo::kKindGPos))) {
      faceI->faceInfo.diagFlags |= BL_FONT_FACE_DIAG_WRONG_GPOS_DATA;
      return BL_SUCCESS;
    }

    if (faceI->layout.gpos.lookupCount)
      faceI->funcs.applyGPos = applyLookups<LookupInfo::kKindGPos, GPosContext>;
    faceI->layout.tables[1] = validator.tables[1];
  }

  faceI->scriptTags.dcast<BLArray<BLTag>>().assign(std::move(validator.scriptTags));
  faceI->featureTags.dcast<BLArray<BLTag>>().assign(std::move(validator.featureTags));

  return BL_SUCCESS;
}

} // {LayoutImpl}
} // {BLOpenType}

BL_DIAGNOSTIC_POP
