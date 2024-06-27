// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../bitarray_p.h"
#include "../font_p.h"
#include "../fonttagdata_p.h"
#include "../fontfeaturesettings_p.h"
#include "../glyphbuffer_p.h"
#include "../trace_p.h"
#include "../opentype/otface_p.h"
#include "../opentype/otlayout_p.h"
#include "../opentype/otlayoutcontext_p.h"
#include "../opentype/otlayouttables_p.h"
#include "../support/algorithm_p.h"
#include "../support/bitops_p.h"
#include "../support/fixedbitarray_p.h"
#include "../support/lookuptable_p.h"
#include "../support/memops_p.h"
#include "../support/ptrops_p.h"

// TODO: [OpenType] This is not complete so we had to disable some warnings here...
BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

namespace bl {
namespace OpenType {
namespace LayoutImpl {

// bl::OpenType::LayoutImpl - Tracing
// ==================================

#if defined(BL_TRACE_OT_ALL) || defined(BL_TRACE_OT_LAYOUT)
  #define Trace BLDebugTrace
#else
  #define Trace BLDummyTrace
#endif

class ValidationContext {
public:
  //! \name Members
  //! \{

  const OTFaceImpl* _faceI;
  LookupKind _lookupKind;
  Trace _trace;

  //! \}

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG ValidationContext(const OTFaceImpl* faceI, LookupKind lookupKind) noexcept
    : _faceI(faceI),
      _lookupKind(lookupKind) {}

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG const OTFaceImpl* faceImpl() const noexcept { return _faceI; }
  BL_INLINE_NODEBUG LookupKind lookupKind() const noexcept { return _lookupKind; }

  //! \}

  //! \name Tracing & Error Handling
  //! \{

  BL_INLINE void indent() noexcept { _trace.indent(); }
  BL_INLINE void deindent() noexcept { _trace.deindent(); }

  template<typename... Args>
  BL_INLINE void out(Args&&... args) noexcept { _trace.out(BLInternal::forward<Args>(args)...); }

  template<typename... Args>
  BL_INLINE void info(Args&&... args) noexcept { _trace.info(BLInternal::forward<Args>(args)...); }

  template<typename... Args>
  BL_INLINE bool warn(Args&&... args) noexcept { return _trace.warn(BLInternal::forward<Args>(args)...); }

  template<typename... Args>
  BL_INLINE bool fail(Args&&... args) noexcept { return _trace.fail(BLInternal::forward<Args>(args)...); }

  BL_NOINLINE bool tableEmpty(const char* tableName) noexcept {
    return fail("%s cannot be empty", tableName);
  }

  BL_NOINLINE bool invalidTableSize(const char* tableName, uint32_t tableSize, uint32_t requiredSize) noexcept {
    return fail("%s is truncated (size=%u, required=%u)", tableName, tableSize, requiredSize);
  }

  BL_NOINLINE bool invalidTableFormat(const char* tableName, uint32_t format) noexcept {
    return fail("%s has invalid format (%u)", tableName, format);
  }

  BL_NOINLINE bool invalidFieldValue(const char* tableName, const char* field, uint32_t value) noexcept {
    return fail("%s has invalid %s (%u)", tableName, value);
  }

  BL_NOINLINE bool invalidFieldOffset(const char* tableName, const char* field, uint32_t offset, OffsetRange range) noexcept {
    return fail("%s.%s has invalid offset (%u), valid range=[%u:%u]", tableName, field, offset, range.start, range.end);
  }

  BL_NOINLINE bool invalidOffsetArray(const char* tableName, uint32_t i, uint32_t offset, OffsetRange range) noexcept {
    return fail("%s has invalid offset at #%u: Offset=%u, ValidRange=[%u:%u]", tableName, i, offset, range.start, range.end);
  }

  BL_NOINLINE bool invalidOffsetEntry(const char* tableName, const char* field, uint32_t i, uint32_t offset, OffsetRange range) noexcept {
    return fail("%s has invalid offset of %s at #%u: Offset=%u, ValidRange=[%u:%u]", tableName, field, i, offset, range.start, range.end);
  }

  //! \}
};

// bl::OpenType::LayoutImpl - GDEF - Init
// ======================================

static BL_NOINLINE BLResult initGDef(OTFaceImpl* faceI, Table<GDefTable> gdef) noexcept {
  if (!gdef.fits())
    return BL_SUCCESS;

  uint32_t version = gdef->v1_0()->version();
  uint32_t headerSize = GDefTable::HeaderV1_0::kBaseSize;

  if (version >= 0x00010002u)
    headerSize = GDefTable::HeaderV1_2::kBaseSize;

  if (version >= 0x00010003u)
    headerSize = GDefTable::HeaderV1_3::kBaseSize;

  if (BL_UNLIKELY(version < 0x00010000u || version > 0x00010003u))
    return BL_SUCCESS;

  if (BL_UNLIKELY(gdef.size < headerSize))
    return BL_SUCCESS;

  uint32_t glyphClassDefOffset = gdef->v1_0()->glyphClassDefOffset();
  uint32_t attachListOffset = gdef->v1_0()->attachListOffset();
  uint32_t ligCaretListOffset = gdef->v1_0()->ligCaretListOffset();
  uint32_t markAttachClassDefOffset = gdef->v1_0()->markAttachClassDefOffset();
  uint32_t markGlyphSetsDefOffset = version >= 0x00010002u ? uint32_t(gdef->v1_2()->markGlyphSetsDefOffset()) : uint32_t(0);
  uint32_t itemVarStoreOffset = version >= 0x00010003u ? uint32_t(gdef->v1_3()->itemVarStoreOffset()) : uint32_t(0);

  // TODO: [OpenType] Unfinished.
  blUnused(attachListOffset, ligCaretListOffset, markGlyphSetsDefOffset, itemVarStoreOffset);

  // Some fonts have incorrect value of `GlyphClassDefOffset` set to 10. This collides with the header which is
  // 12 bytes. It's probably a result of some broken tool used to write such fonts in the past. We simply fix
  // this issue by changing the `headerSize` to 10 and ignoring `markAttachClassDefOffset`.
  if (glyphClassDefOffset == 10 && version == 0x00010000u) {
    headerSize = 10;
    markAttachClassDefOffset = 0;
  }

  if (glyphClassDefOffset) {
    if (glyphClassDefOffset >= headerSize && glyphClassDefOffset < gdef.size) {
      faceI->otFlags |= OTFaceFlags::kGlyphClassDef;
    }
  }

  if (markAttachClassDefOffset) {
    if (markAttachClassDefOffset >= headerSize && markAttachClassDefOffset < gdef.size) {
      faceI->otFlags |= OTFaceFlags::kMarkAttachClassDef;
    }
  }

  faceI->layout.tables[2] = gdef;
  return BL_SUCCESS;
}

// bl::OpenType::LayoutImpl - GSUB & GPOS - Constants
// ==================================================

// Artificial format bits that describe either two/three/four Coverage/ClassDef tables having formats in [1-2] range.
enum class FormatBits2X : uint32_t {
  k11 = 0x0,
  k12 = 0x1,
  k21 = 0x2,
  k22 = 0x3
};

enum class FormatBits3X : uint32_t {
  k111 = 0x0,
  k112 = 0x1,
  k121 = 0x2,
  k122 = 0x3,
  k211 = 0x4,
  k212 = 0x5,
  k221 = 0x6,
  k222 = 0x7
};

enum class FormatBits4X : uint32_t {
  k1111 = 0x0,
  k1112 = 0x1,
  k1121 = 0x2,
  k1122 = 0x3,
  k1211 = 0x4,
  k1212 = 0x5,
  k1221 = 0x6,
  k1222 = 0x7,
  k2111 = 0x8,
  k2112 = 0x9,
  k2121 = 0xA,
  k2122 = 0xB,
  k2211 = 0xC,
  k2212 = 0xD,
  k2221 = 0xE,
  k2222 = 0xF
};

// bl::OpenType::LayoutImpl - GSUB & GPOS - Metadata
// =================================================

static BL_NOINLINE const char* gsubLookupName(uint32_t lookupType) noexcept {
  static const char lookupNames[] = {
    "<INVALID>\0"
    "SingleSubst\0"
    "MultipleSubst\0"
    "AlternateSubst\0"
    "LigatureSubst\0"
    "ContextSubst\0"
    "ChainedContextSubst\0"
    "Extension\0"
    "ReverseChainedContextSubst\0"
  };

  static const uint8_t lookupIndex[] = {
    0,   // "<INVALID>"
    10,  // "SingleSubst"
    22,  // "MultipleSubst"
    36,  // "AlternateSubst"
    51,  // "LigatureSubst"
    65,  // "ContextSubst"
    78,  // "ChainedContextSubst"
    98,  // "Extension"
    108  // "ReverseChainedContextSubst"
  };

  if (lookupType >= BL_ARRAY_SIZE(lookupIndex))
    lookupType = 0;

  return lookupNames + lookupIndex[lookupType];
}

static BL_NOINLINE const char* gposLookupName(uint32_t lookupType) noexcept {
  static const char lookupNames[] = {
    "<INVALID>\0"
    "SingleAdjustment\0"
    "PairAdjustment\0"
    "CursiveAttachment\0"
    "MarkToBaseAttachment\0"
    "MarkToLigatureAttachment\0"
    "MarkToMarkAttachment\0"
    "ContextPositioning\0"
    "ChainedContextPositioning\0"
    "Extension\0"
  };

  static const uint8_t lookupIndex[] = {
    0,   // "<INVALID>"
    10,  // "SingleAdjustment"
    27,  // "PairAdjustment"
    42,  // "CursiveAttachment"
    60,  // "MarkToBaseAttachment"
    81,  // "MarkToLigatureAttachment"
    106, // "MarkToMarkAttachment"
    127, // "ContextPositioning"
    146, // "ChainedContextPositioning"
    172  // "Extension"
  };

  if (lookupType >= BL_ARRAY_SIZE(lookupIndex))
    lookupType = 0;

  return lookupNames + lookupIndex[lookupType];
}

static const GSubGPosLookupInfo gsubLookupInfoTable = {
  // Lookup type maximum value:
  GSubTable::kLookupMaxValue,

  // Lookup extension type:
  GSubTable::kLookupExtension,

  // Lookup type to lookup type & format mapping:
  {
    { 0, uint8_t(GSubLookupAndFormat::kNone)         }, // Invalid.
    { 2, uint8_t(GSubLookupAndFormat::kType1Format1) }, // Lookup Type 1 - Single Substitution.
    { 1, uint8_t(GSubLookupAndFormat::kType2Format1) }, // Lookup Type 2 - Multiple Substitution.
    { 1, uint8_t(GSubLookupAndFormat::kType3Format1) }, // Lookup Type 3 - Alternate Substitution.
    { 1, uint8_t(GSubLookupAndFormat::kType4Format1) }, // Lookup Type 4 - Ligature Substitution.
    { 3, uint8_t(GSubLookupAndFormat::kType5Format1) }, // Lookup Type 5 - Context Substitution.
    { 3, uint8_t(GSubLookupAndFormat::kType6Format1) }, // Lookup Type 6 - Chained Context Substitution.
    { 1, uint8_t(GSubLookupAndFormat::kNone)         }, // Lookup Type 7 - Extension.
    { 1, uint8_t(GSubLookupAndFormat::kType8Format1) }  // Lookup Type 8 - Reverse Chained Context Substitution.
  },

  // Lookup type, format, headerSize.
  {
    { 0, 0, uint8_t(0)                                                }, // Lookup Type 0 - Invalid.
    { 1, 1, uint8_t(GSubTable::SingleSubst1::kBaseSize)               }, // Lookup Type 1 - Format 1.
    { 1, 2, uint8_t(GSubTable::SingleSubst2::kBaseSize)               }, // Lookup Type 1 - Format 2.
    { 2, 1, uint8_t(GSubTable::MultipleSubst1::kBaseSize)             }, // Lookup Type 2 - Format 1.
    { 3, 1, uint8_t(GSubTable::AlternateSubst1::kBaseSize)            }, // Lookup Type 3 - Format 1.
    { 4, 1, uint8_t(GSubTable::LigatureSubst1::kBaseSize)             }, // Lookup Type 4 - Format 1.
    { 5, 1, uint8_t(GSubTable::SequenceContext1::kBaseSize)           }, // Lookup Type 5 - Format 1.
    { 5, 2, uint8_t(GSubTable::SequenceContext2::kBaseSize)           }, // Lookup Type 5 - Format 2.
    { 5, 3, uint8_t(GSubTable::SequenceContext3::kBaseSize)           }, // Lookup Type 5 - Format 3.
    { 6, 1, uint8_t(GSubTable::ChainedSequenceContext1::kBaseSize)    }, // Lookup Type 6 - Format 1.
    { 6, 2, uint8_t(GSubTable::ChainedSequenceContext2::kBaseSize)    }, // Lookup Type 6 - Format 2.
    { 6, 3, uint8_t(GSubTable::ChainedSequenceContext3::kBaseSize)    }, // Lookup Type 6 - Format 3.
    { 8, 1, uint8_t(GSubTable::ReverseChainedSingleSubst1::kBaseSize) }  // Lookup Type 8 - Format 1.
  }
};

static const GSubGPosLookupInfo gposLookupInfoTable = {
  // Lookup type maximum value:
  GPosTable::kLookupMaxValue,

  // Lookup extension type:
  GPosTable::kLookupExtension,

  // Lookup type to lookup type & format mapping:
  {
    { 0, uint8_t(GPosLookupAndFormat::kNone)         }, // Lookup Type 0 - Invalid.
    { 2, uint8_t(GPosLookupAndFormat::kType1Format1) }, // Lookup Type 1 - Single Adjustment.
    { 2, uint8_t(GPosLookupAndFormat::kType2Format1) }, // Lookup Type 2 - Pair Adjustment.
    { 1, uint8_t(GPosLookupAndFormat::kType3Format1) }, // Lookup Type 3 - Cursive Attachment.
    { 1, uint8_t(GPosLookupAndFormat::kType4Format1) }, // Lookup Type 4 - MarkToBase Attachment.
    { 1, uint8_t(GPosLookupAndFormat::kType5Format1) }, // Lookup Type 5 - MarkToLigature Attachment.
    { 1, uint8_t(GPosLookupAndFormat::kType6Format1) }, // Lookup Type 6 - MarkToMark Attachment.
    { 3, uint8_t(GPosLookupAndFormat::kType7Format1) }, // Lookup Type 7 - Context Positioning.
    { 3, uint8_t(GPosLookupAndFormat::kType8Format1) }, // Lookup Type 8 - Chained Context Positioning.
    { 1, uint8_t(GPosLookupAndFormat::kNone)         }  // Lookup Type 9 - Extension.
  },

  // Lookup type, format, headerSize.
  {
    { 0, 0, uint8_t(0)                                                }, // Lookup Type 0 - Invalid.
    { 1, 1, uint8_t(GPosTable::SingleAdjustment1::kBaseSize)          }, // Lookup Type 1 - Format 1.
    { 1, 2, uint8_t(GPosTable::SingleAdjustment2::kBaseSize)          }, // Lookup Type 1 - Format 2.
    { 2, 1, uint8_t(GPosTable::PairAdjustment1::kBaseSize)            }, // Lookup Type 2 - Format 1.
    { 2, 2, uint8_t(GPosTable::PairAdjustment2::kBaseSize)            }, // Lookup Type 2 - Format 2.
    { 3, 1, uint8_t(GPosTable::CursiveAttachment1::kBaseSize)         }, // Lookup Type 3 - Format 1.
    { 4, 1, uint8_t(GPosTable::MarkToBaseAttachment1::kBaseSize)      }, // Lookup Type 4 - Format 1.
    { 5, 1, uint8_t(GPosTable::MarkToLigatureAttachment1::kBaseSize)  }, // Lookup Type 5 - Format 1.
    { 6, 1, uint8_t(GPosTable::MarkToMarkAttachment1::kBaseSize)      }, // Lookup Type 6 - Format 1.
    { 7, 1, uint8_t(GPosTable::SequenceContext1::kBaseSize)           }, // Lookup Type 7 - Format 1.
    { 7, 2, uint8_t(GPosTable::SequenceContext2::kBaseSize)           }, // Lookup Type 7 - Format 2.
    { 7, 3, uint8_t(GPosTable::SequenceContext3::kBaseSize)           }, // Lookup Type 7 - Format 3.
    { 8, 1, uint8_t(GPosTable::ChainedSequenceContext1::kBaseSize)    }, // Lookup Type 8 - Format 1.
    { 8, 2, uint8_t(GPosTable::ChainedSequenceContext2::kBaseSize)    }, // Lookup Type 8 - Format 2.
    { 8, 3, uint8_t(GPosTable::ChainedSequenceContext3::kBaseSize)    }  // Lookup Type 8 - Format 3.
  }
};

// bl::OpenType::LayoutImpl - GSUB & GPOS - Validation Helpers
// ===========================================================

// TODO: [OpenType] REMOVE?
/*
static bool validateRawOffsetArray(ValidationContext& validator, RawTable data, const char* tableName) noexcept {
  if (data.size < 2u)
    return validator.invalidTableSize(tableName, data.size, 2u);

  uint32_t count = data.dataAs<Array16<UInt16>>()->count();
  uint32_t headerSize = 2u + count * 2u;

  if (data.size < headerSize)
    return validator.invalidTableSize(tableName, data.size, headerSize);

  const UInt16* array = data.dataAs<Array16<Offset16>>()->array();
  OffsetRange range{headerSize, uint32_t(data.size)};

  for (uint32_t i = 0; i < count; i++) {
    uint32_t offset = array[i].value();
    if (!range.contains(offset))
      return validator.invalidOffsetArray(tableName, i, offset, range);
  }

  return true;
}

static bool validateTagRef16Array(ValidationContext& validator, RawTable data, const char* tableName) noexcept {
  if (data.size < 2u)
    return validator.invalidTableSize(tableName, data.size, 2u);

  uint32_t count = data.dataAs<Array16<UInt16>>()->count();
  uint32_t headerSize = 2u + count * uint32_t(sizeof(TagRef16));

  if (data.size < headerSize)
    return validator.invalidTableSize(tableName, data.size, headerSize);

  const TagRef16* array = data.dataAs<Array16<TagRef16>>()->array();
  OffsetRange range{headerSize, uint32_t(data.size)};

  for (uint32_t i = 0; i < count; i++) {
    uint32_t offset = array[i].offset.value();
    if (!range.contains(offset))
      return validator.invalidOffsetArray(tableName, i, offset, range);
  }

  return true;
}
*/

// bl::OpenType::LayoutImpl - GSUB & GPOS - Apply Scope
// ====================================================

//! A single index to be applied when processing a lookup.
struct ApplyIndex {
  size_t _index;

  BL_INLINE_NODEBUG constexpr bool isRange() const noexcept { return false; }
  BL_INLINE_NODEBUG size_t index() const noexcept { return _index; }
  BL_INLINE_NODEBUG size_t end() const noexcept { return _index + 1; }
  BL_INLINE_NODEBUG size_t size() const noexcept { return 1; }
};

//! A range to be applied when processing a lookup.
//!
//! Typically a root lookup would apply the whole range of the work buffer, however, nested lookups only
//! apply a single index, which can still match multiple glyphs, but the match must start at that index.
struct ApplyRange {
  size_t _index;
  size_t _end;

  BL_INLINE_NODEBUG constexpr bool isRange() const noexcept { return true; }
  BL_INLINE_NODEBUG size_t index() const noexcept { return _index; }
  BL_INLINE_NODEBUG size_t end() const noexcept { return _end; }
  BL_INLINE_NODEBUG size_t size() const noexcept { return _end - _index; }

  BL_INLINE void intersect(size_t index, size_t end) noexcept {
    _index = blMax(_index, index);
    _end = blMin(_end, end);
  }
};

// bl::OpenType::LayoutImpl - GSUB & GPOS - ClassDef Validation
// ============================================================

static BL_NOINLINE bool validateClassDefTable(ValidationContext& validator, Table<ClassDefTable> table, const char* tableName) noexcept {
  // Ignore if it doesn't fit.
  if (!table.fits())
    return validator.invalidTableSize(tableName, table.size, ClassDefTable::kBaseSize);

  uint32_t format = table->format();
  switch (format) {
    case 1: {
      uint32_t headerSize = ClassDefTable::Format1::kBaseSize;
      if (!table.fits(headerSize))
        return validator.invalidTableSize(tableName, table.size, headerSize);

      const ClassDefTable::Format1* f = table->format1();
      uint32_t count = f->classValues.count();

      headerSize += count * 2u;
      if (!table.fits(headerSize))
        return validator.invalidTableSize(tableName, table.size, headerSize);

      // We won't fail, but we won't consider we have a ClassDef either.
      // If the ClassDef is required by other tables then we will fail later.
      if (!count)
        return validator.warn("No glyph ids specified, ignoring...");

      return true;
    }

    case 2: {
      uint32_t headerSize = ClassDefTable::Format2::kBaseSize;
      if (!table.fits(headerSize))
        return validator.invalidTableSize(tableName, table.size, headerSize);

      const ClassDefTable::Format2* f = table->format2();
      uint32_t count = f->ranges.count();

      // We won't fail, but we won't consider we have a class definition either.
      if (!count)
        return validator.warn("No range specified, ignoring...");

      headerSize = ClassDefTable::Format2::kBaseSize + count * uint32_t(sizeof(ClassDefTable::Range));
      if (!table.fits(headerSize))
        return validator.invalidTableSize(tableName, table.size, headerSize);

      const ClassDefTable::Range* rangeArray = f->ranges.array();
      uint32_t lastGlyph = rangeArray[0].lastGlyph();

      if (rangeArray[0].firstGlyph() > lastGlyph)
        return validator.fail("%s Range[%u] firstGlyph (%u) greater than lastGlyph (%u)", tableName, 0, rangeArray[0].firstGlyph(), lastGlyph);

      for (uint32_t i = 1; i < count; i++) {
        const ClassDefTable::Range& range = rangeArray[i];
        uint32_t firstGlyph = range.firstGlyph();

        if (firstGlyph <= lastGlyph)
          return validator.fail("%s Range[%u] firstGlyph (%u) not greater than previous lastFlyph (%u)", tableName, i, firstGlyph, lastGlyph);

        lastGlyph = range.lastGlyph();
        if (firstGlyph > lastGlyph)
          return validator.fail("%s Range[%u] firstGlyph (%u) greater than lastGlyph (%u)", tableName, i, firstGlyph, lastGlyph);
      }

      return true;
    }

    default:
      return validator.invalidTableFormat(tableName, format);
  }
}

// bl::OpenType::LayoutImpl - GSUB & GPOS - Coverage Validation
// ============================================================

static BL_NOINLINE bool validateCoverageTable(ValidationContext& validator, Table<CoverageTable> coverageTable, uint32_t& coverageCount) noexcept {
  const char* tableName = "CoverageTable";

  coverageCount = 0;
  if (!coverageTable.fits())
    return validator.invalidTableSize(tableName, coverageTable.size, CoverageTable::kBaseSize);

  uint32_t format = coverageTable->format();
  switch (format) {
    case 1: {
      const CoverageTable::Format1* format1 = coverageTable->format1();

      uint32_t glyphCount = format1->glyphs.count();
      uint32_t headerSize = CoverageTable::Format1::kBaseSize + glyphCount * 2u;

      if (!coverageTable.fits(headerSize))
        return validator.invalidTableSize(tableName, coverageTable.size, headerSize);

      if (!glyphCount)
        return validator.tableEmpty(tableName);

      coverageCount = glyphCount;
      return true;
    }

    case 2: {
      const CoverageTable::Format2* format2 = coverageTable->format2();

      uint32_t rangeCount = format2->ranges.count();
      uint32_t headerSize = CoverageTable::Format2::kBaseSize + rangeCount * uint32_t(sizeof(CoverageTable::Range));

      if (!coverageTable.fits(headerSize))
        return validator.invalidTableSize(tableName, coverageTable.size, headerSize);

      if (!rangeCount)
        return validator.tableEmpty(tableName);

      const CoverageTable::Range* rangeArray = format2->ranges.array();

      uint32_t firstGlyph = rangeArray[0].firstGlyph();
      uint32_t lastGlyph = rangeArray[0].lastGlyph();
      uint32_t currentCoverageIndex = rangeArray[0].startCoverageIndex();

      if (firstGlyph > lastGlyph)
        return validator.fail("Range[%u]: firstGlyph (%u) is greater than lastGlyph (%u)", 0, firstGlyph, lastGlyph);

      if (currentCoverageIndex)
        return validator.fail("Range[%u]: initial startCoverageIndex %u must be zero", 0, currentCoverageIndex);

      currentCoverageIndex += lastGlyph - firstGlyph + 1u;
      for (uint32_t i = 1; i < rangeCount; i++) {
        const CoverageTable::Range& range = rangeArray[i];

        firstGlyph = range.firstGlyph();
        if (firstGlyph <= lastGlyph)
          return validator.fail("Range[%u]: firstGlyph (%u) is not greater than previous lastGlyph (%u)", i, firstGlyph, lastGlyph);

        lastGlyph = range.lastGlyph();
        if (firstGlyph > lastGlyph)
          return validator.fail("Range[%u]: firstGlyph (%u) is greater than lastGlyph (%u)", i, firstGlyph, lastGlyph);

        uint32_t startCoverageIndex = range.startCoverageIndex();
        if (startCoverageIndex != currentCoverageIndex)
          return validator.fail("Range[%u]: startCoverageIndex (%u) doesnt' match currentCoverageIndex (%u)", i, startCoverageIndex, currentCoverageIndex);

        currentCoverageIndex += lastGlyph - firstGlyph + 1u;
      }

      coverageCount = currentCoverageIndex;
      return true;
    }

    default:
      return validator.invalidTableFormat(tableName, format);
  }
}

static BL_NOINLINE bool validateCoverageTables(
  ValidationContext& validator,
  RawTable table,
  const char* tableName,
  const char* coverageName,
  const UInt16* offsets, uint32_t count, OffsetRange offsetRange) noexcept {

  for (uint32_t i = 0; i < count; i++) {
    uint32_t offset = offsets[i].value();
    if (!offset)
      continue;

    if (!offsetRange.contains(offset))
      return validator.fail("%s.%s[%u] offset (%u) is out of range [%u:%u]", tableName, coverageName, i, offset, offsetRange.start, offsetRange.end);

    uint32_t unusedCoverageCount;
    if (!validateCoverageTable(validator, table.subTable<CoverageTable>(offset), unusedCoverageCount))
      return false;
  }

  return true;
}

// bl::OpenType::LayoutImpl - GSUB & GPOS - Lookup Table Validation
// ================================================================

static BL_NOINLINE bool validateLookupWithCoverage(ValidationContext& validator, RawTable data, const char* tableName, uint32_t headerSize, uint32_t& coverageCount) noexcept {
  if (!data.fits(headerSize))
    return validator.invalidTableSize(tableName, data.size, headerSize);

  uint32_t coverageOffset = data.dataAs<GSubGPosTable::LookupHeaderWithCoverage>()->coverageOffset();
  if (coverageOffset < headerSize || coverageOffset >= data.size)
    return validator.fail("%s.coverage offset (%u) is out of range [%u:%u]", tableName, coverageOffset, headerSize, data.size);

  return validateCoverageTable(validator, data.subTable(coverageOffset), coverageCount);
}

// bl::OpenType::LayoutImpl - GSUB & GPOS - Sequence Context Validation
// ====================================================================

static BL_NOINLINE bool validateSequenceLookupRecordArray(ValidationContext& validator, const GSubGPosTable::SequenceLookupRecord* lookupRecordArray, uint32_t lookupRecordCount) noexcept {
  const LayoutData& layoutData = validator.faceImpl()->layout;
  uint32_t lookupCount = layoutData.byKind(validator.lookupKind()).lookupCount;

  for (uint32_t i = 0; i < lookupRecordCount; i++) {
    const GSubGPosTable::SequenceLookupRecord& lookupRecord = lookupRecordArray[i];
    uint32_t lookupIndex = lookupRecord.lookupIndex();

    if (lookupIndex >= lookupCount)
      return validator.fail("SequenceLookupRecord[%u] has invalid lookupIndex (%u) (lookupCount=%u)", i, lookupIndex, lookupCount);
  }

  return true;
}

template<typename SequenceLookupTable>
static BL_NOINLINE bool validateContextFormat1_2(ValidationContext& validator, Table<SequenceLookupTable> table, const char* tableName) noexcept {
  typedef GSubGPosTable::SequenceRule SequenceRule;
  typedef GSubGPosTable::SequenceRuleSet SequenceRuleSet;

  uint32_t coverageCount;
  if (!validateLookupWithCoverage(validator, table, tableName, SequenceLookupTable::kBaseSize, coverageCount))
    return false;

  uint32_t ruleSetCount = table->ruleSetOffsets.count();
  uint32_t headerSize = SequenceLookupTable::kBaseSize + ruleSetCount * 2u;

  if (!table.fits(headerSize))
    return validator.invalidTableSize(tableName, table.size, headerSize);

  const Offset16* ruleSetOffsetArray = table->ruleSetOffsets.array();
  OffsetRange ruleSetOffsetRange{headerSize, table.size - 4u};

  for (uint32_t i = 0; i < ruleSetCount; i++) {
    uint32_t ruleSetOffset = ruleSetOffsetArray[i].value();

    // Offsets are allowed to be null - this means that that SequenceRuleSet must be ignored.
    if (!ruleSetOffset)
      continue;

    if (!ruleSetOffsetRange.contains(ruleSetOffset))
      return validator.invalidOffsetEntry(tableName, "sequenceRuleSetOffset", i, ruleSetOffset, ruleSetOffsetRange);

    Table<SequenceRuleSet> ruleSet(table.subTable(ruleSetOffset));
    uint32_t ruleCount = ruleSet->count();

    if (!ruleCount)
      return validator.fail("%s.ruleSet[%u] cannot be empty", tableName, i);

    uint32_t ruleSetHeaderSize = 2u + ruleCount * 2u;
    if (!ruleSet.fits(ruleSetHeaderSize))
      return validator.fail("%s.ruleSet[%u] is truncated (size=%u, required=%u)", tableName, i, ruleSet.size, ruleSetHeaderSize);

    const Offset16* ruleOffsetArray = ruleSet->array();
    OffsetRange ruleOffsetRange{ruleSetHeaderSize, ruleSet.size - SequenceRule::kBaseSize};

    for (uint32_t ruleIndex = 0; ruleIndex < ruleCount; ruleIndex++) {
      uint32_t ruleOffset = ruleOffsetArray[ruleIndex].value();
      if (!ruleOffsetRange.contains(ruleOffset))
        return validator.fail("%s.ruleSet[%u].rule[%u] offset (%u) is out of range [%u:%u]", tableName, i, ruleIndex, ruleOffset, ruleOffsetRange.start, ruleOffsetRange.end);

      Table<SequenceRule> rule = ruleSet.subTable(ruleOffset);
      uint32_t glyphCount = rule->glyphCount();
      uint32_t lookupRecordCount = rule->lookupRecordCount();
      uint32_t ruleTableSize = 4u + (lookupRecordCount + glyphCount - 1) * 2u;

      if (!rule.fits(ruleTableSize))
        return validator.fail("%s.ruleSet[%u].rule[%u] is truncated (size=%u, required=%u)", tableName, i, ruleIndex, rule.size, ruleTableSize);

      if (glyphCount < 2)
        return validator.fail("%s.ruleSet[%u].rule[%u] has invalid glyphCount (%u)", tableName, i, ruleIndex, glyphCount);

      if (!lookupRecordCount)
        return validator.fail("%s.ruleSet[%u].rule[%u] has invalid lookupRecordCount (%u)", tableName, i, ruleIndex, lookupRecordCount);

      if (!validateSequenceLookupRecordArray(validator, rule->lookupRecordArray(glyphCount), lookupRecordCount))
        return false;
    }
  }

  return true;
}

static BL_INLINE bool validateContextFormat1(ValidationContext& validator, Table<GSubGPosTable::SequenceContext1> table, const char* tableName) noexcept {
  return validateContextFormat1_2<GSubGPosTable::SequenceContext1>(validator, table, tableName);
}

static BL_NOINLINE bool validateContextFormat2(ValidationContext& validator, Table<GSubGPosTable::SequenceContext2> table, const char* tableName) noexcept {
  if (!table.fits())
    return validator.invalidTableSize(tableName, table.size, GSubGPosTable::SequenceContext2::kBaseSize);

  uint32_t ruleSetCount = table->ruleSetOffsets.count();
  uint32_t headerSize = GSubGPosTable::SequenceContext2::kBaseSize + ruleSetCount * 2u;

  if (!table.fits(headerSize))
    return validator.invalidTableSize(tableName, table.size, headerSize);

  uint32_t classDefOffset = table.dataAs<GSubGPosTable::SequenceContext2>()->classDefOffset();
  OffsetRange offsetRange{headerSize, table.size};

  if (!offsetRange.contains(classDefOffset))
    return validator.invalidFieldOffset(tableName, "classDefOffset", classDefOffset, offsetRange);

  if (!validateClassDefTable(validator, table.subTableUnchecked(classDefOffset), "ClassDef"))
    return false;

  return validateContextFormat1_2<GSubGPosTable::SequenceContext2>(validator, table, tableName);
}

static BL_NOINLINE bool validateContextFormat3(ValidationContext& validator, Table<GSubGPosTable::SequenceContext3> table, const char* tableName) noexcept {
  if (!table.fits())
    return validator.invalidTableSize(tableName, table.size, GSubGPosTable::SequenceContext3::kBaseSize);

  uint32_t glyphCount = table->glyphCount();
  uint32_t lookupRecordCount = table->lookupRecordCount();
  uint32_t headerSize = GSubGPosTable::SequenceContext3::kBaseSize + glyphCount * 2u + lookupRecordCount * GPosTable::SequenceLookupRecord::kBaseSize;

  if (!table.fits(headerSize))
    return validator.invalidTableSize(tableName, table.size, headerSize);

  if (!glyphCount)
    return validator.invalidFieldValue(tableName, "glyphCount", glyphCount);

  if (!lookupRecordCount)
    return validator.invalidFieldValue(tableName, "lookupRecordCount", lookupRecordCount);

  OffsetRange subTableOffsetRange{headerSize, table.size};
  const UInt16* coverageOffsetArray = table->coverageOffsetArray();

  for (uint32_t coverageTableIndex = 0; coverageTableIndex < glyphCount; coverageTableIndex++) {
    uint32_t coverageTableOffset = coverageOffsetArray[coverageTableIndex].value();
    if (!subTableOffsetRange.contains(coverageTableOffset))
      return validator.invalidOffsetEntry(tableName, "coverageOffset", coverageTableIndex, coverageTableOffset, subTableOffsetRange);

    uint32_t coverageCount;
    if (!validateCoverageTable(validator, table.subTable<CoverageTable>(coverageTableOffset), coverageCount))
      return false;
  }

  return validateSequenceLookupRecordArray(validator, table->lookupRecordArray(glyphCount), lookupRecordCount);
}

// bl::OpenType::LayoutImpl - GSUB & GPOS - Sequence Context Utilities
// ===================================================================

struct SequenceMatch {
  uint32_t glyphCount;
  uint32_t lookupRecordCount;
  const GSubGPosTable::SequenceLookupRecord* lookupRecords;
};

static BL_INLINE bool matchSequenceRuleFormat1(Table<Array16<Offset16>> ruleOffsets, uint32_t ruleCount, const BLGlyphId* glyphData, size_t maxGlyphCount, SequenceMatch* matchOut) noexcept {
  size_t maxGlyphCountMinus1 = maxGlyphCount - 1u;
  for (uint32_t ruleIndex = 0; ruleIndex < ruleCount; ruleIndex++) {
    uint32_t ruleOffset = ruleOffsets->array()[ruleIndex].value();
    BL_ASSERT_VALIDATED(ruleOffset <= ruleOffsets.size - 4u);

    const GSubGPosTable::SequenceRule* rule = PtrOps::offset<const GSubGPosTable::SequenceRule>(ruleOffsets.data, ruleOffset);
    uint32_t glyphCount = rule->glyphCount();
    uint32_t glyphCountMinus1 = glyphCount - 1u;

    if (glyphCountMinus1 > maxGlyphCountMinus1)
      continue;

    // This is safe - a single SequenceRule is 4 bytes that is followed by `GlyphId[glyphCount - 1]` and then
    // by `SequenceLookupRecord[sequenceLookupCount]`. Since we don't know whether we have a match or not we will
    // only check bounds required by matching and postponing `sequenceLookupCount` until we have an actual match.
    BL_ASSERT_VALIDATED(ruleOffset + glyphCountMinus1 * 2u <= ruleOffsets.size - 4u);

    uint32_t glyphIndex = 0;
    for (;;) {
      BLGlyphId glyphA = rule->inputSequence()[glyphIndex].value();
      BLGlyphId glyphB = glyphData[++glyphIndex];

      if (glyphA != glyphB)
        break;

      if (glyphIndex < glyphCountMinus1)
        continue;

      BL_ASSERT_VALIDATED(rule->lookupRecordCount() > 0);
      BL_ASSERT_VALIDATED(ruleOffset + glyphCountMinus1 * 2u + rule->lookupRecordCount() * 4u <= ruleOffsets.size - 4u);

      *matchOut = SequenceMatch{glyphCount, rule->lookupRecordCount(), rule->lookupRecordArray(glyphCount)};
      return true;
    }
  }

  return false;
}

template<uint32_t kCDFmt>
static BL_INLINE bool matchSequenceRuleFormat2(Table<Array16<Offset16>> ruleOffsets, uint32_t ruleCount, const BLGlyphId* glyphData, size_t maxGlyphCount, const ClassDefTableIterator& cdIt, SequenceMatch* matchOut) noexcept {
  size_t maxGlyphCountMinus1 = maxGlyphCount - 1u;
  for (uint32_t ruleIndex = 0; ruleIndex < ruleCount; ruleIndex++) {
    uint32_t ruleOffset = ruleOffsets->array()[ruleIndex].value();
    BL_ASSERT_VALIDATED(ruleOffset <= ruleOffsets.size - 4u);

    const GSubGPosTable::SequenceRule* rule = PtrOps::offset<const GSubGPosTable::SequenceRule>(ruleOffsets.data, ruleOffset);
    uint32_t glyphCount = rule->glyphCount();
    uint32_t glyphCountMinus1 = glyphCount - 1u;

    if (glyphCountMinus1 > maxGlyphCountMinus1)
      continue;

    // This is safe - a single ClassSequenceRule is 4 bytes that is followed by `GlyphId[glyphCount - 1]` and then
    // by `SequenceLookupRecord[sequenceLookupCount]`. Since we don't know whether we have a match or not we will
    // only check bounds required by matching and postponing `sequenceLookupCount` until we have an actual match.
    BL_ASSERT_VALIDATED(ruleOffset + glyphCountMinus1 * 2u <= ruleOffsets.size - 4u);

    uint32_t glyphIndex = 0;
    for (;;) {
      uint32_t classValue = rule->inputSequence()[glyphIndex].value();
      BLGlyphId glyphId = glyphData[++glyphIndex];

      if (!cdIt.matchGlyphClass<kCDFmt>(glyphId, classValue))
        break;

      if (glyphIndex < glyphCountMinus1)
        continue;

      BL_ASSERT_VALIDATED(rule->lookupRecordCount() > 0u);
      BL_ASSERT_VALIDATED(ruleOffset + glyphCountMinus1 * 2u + rule->lookupRecordCount() * 4u <= ruleOffsets.size - 4u);

      *matchOut = SequenceMatch{glyphCount, rule->lookupRecordCount(), rule->lookupRecordArray(glyphCount)};
      return true;
    }
  }

  return false;
}

template<uint32_t kCovFmt>
static BL_INLINE bool matchSequenceFormat1(Table<GSubGPosTable::SequenceContext1> table, uint32_t ruleSetCount, GlyphRange firstGlyphRange, const CoverageTableIterator& covIt, const BLGlyphId* glyphPtr, size_t maxGlyphCount, SequenceMatch* matchOut) noexcept {
  BLGlyphId glyphId = glyphPtr[0];
  if (!firstGlyphRange.contains(glyphId))
    return false;

  uint32_t coverageIndex;
  if (!covIt.find<kCovFmt>(glyphId, coverageIndex) || coverageIndex >= ruleSetCount)
    return false;

  uint32_t ruleSetOffset = table->ruleSetOffsets.array()[coverageIndex].value();
  BL_ASSERT_VALIDATED(ruleSetOffset <= table.size - 2u);

  Table<Array16<Offset16>> ruleOffsets(table.subTableUnchecked(ruleSetOffset));
  uint32_t ruleCount = ruleOffsets->count();
  BL_ASSERT_VALIDATED(ruleCount && ruleSetOffset + ruleCount * 2u <= table.size - 2u);

  return matchSequenceRuleFormat1(ruleOffsets, ruleCount, glyphPtr, maxGlyphCount, matchOut);
}

template<uint32_t kCovFmt, uint32_t kCDFmt>
static BL_INLINE bool matchSequenceFormat2(
  Table<GSubGPosTable::SequenceContext2> table,
  uint32_t ruleSetCount,
  GlyphRange firstGlyphRange,
  const CoverageTableIterator& covIt,
  const ClassDefTableIterator& cdIt,
  const BLGlyphId* glyphPtr,
  size_t maxGlyphCount,
  SequenceMatch* matchOut) noexcept {

  BLGlyphId glyphId = glyphPtr[0];
  if (!firstGlyphRange.contains(glyphId))
    return false;

  uint32_t unusedCoverageIndex;
  if (!covIt.find<kCovFmt>(glyphId, unusedCoverageIndex))
    return false;

  uint32_t classIndex = cdIt.classOfGlyph<kCDFmt>(glyphId);
  if (classIndex >= ruleSetCount)
    return false;

  uint32_t ruleSetOffset = table->ruleSetOffsets.array()[classIndex].value();
  BL_ASSERT_VALIDATED(ruleSetOffset <= table.size - 2u);

  Table<Array16<Offset16>> ruleOffsets(table.subTableUnchecked(ruleSetOffset));
  uint32_t ruleCount = ruleOffsets->count();
  BL_ASSERT_VALIDATED(ruleCount && ruleSetOffset + ruleCount * 2u <= table.size - 2u);

  return matchSequenceRuleFormat2<kCDFmt>(ruleOffsets, ruleCount, glyphPtr, maxGlyphCount, cdIt, matchOut);
}

static BL_INLINE bool matchSequenceFormat3(
  Table<GSubGPosTable::SequenceContext3> table,
  const UInt16* coverageOffsetArray,
  GlyphRange firstGlyphRange,
  const CoverageTableIterator& cov0It,
  uint32_t cov0Fmt,
  const BLGlyphId* glyphPtr,
  size_t glyphCount) noexcept {

  BLGlyphId glyphId = glyphPtr[0];
  if (!firstGlyphRange.contains(glyphId))
    return false;

  uint32_t unusedCoverageIndex0;
  if (!cov0It.findWithFormat(cov0Fmt, glyphId, unusedCoverageIndex0))
    return false;

  for (size_t i = 1; i < glyphCount; i++) {
    CoverageTableIterator covItN;
    uint32_t covFmtN = covItN.init(table.subTableUnchecked(coverageOffsetArray[i].value()));
    GlyphRange glyphRangeN = covItN.glyphRangeWithFormat(covFmtN);

    uint32_t glyphIdN = glyphPtr[i];
    uint32_t unusedCoverageIndexN;

    if (!glyphRangeN.contains(glyphIdN) || !covItN.findWithFormat(covFmtN, glyphIdN, unusedCoverageIndexN))
      return false;
  }

  return true;
}

// bl::OpenType::LayoutImpl - GSUB & GPOS - Chained Sequence Context Validation
// ============================================================================

template<typename ChainedSequenceLookupTable>
static BL_NOINLINE bool validateChainedContextFormat1_2(ValidationContext& validator, Table<ChainedSequenceLookupTable> table, const char* tableName) noexcept {
  typedef GSubGPosTable::ChainedSequenceRule ChainedSequenceRule;
  typedef GSubGPosTable::ChainedSequenceRuleSet ChainedSequenceRuleSet;

  uint32_t coverageCount;
  if (!validateLookupWithCoverage(validator, table, tableName, ChainedSequenceLookupTable::kBaseSize, coverageCount))
    return false;

  uint32_t ruleSetCount = table->ruleSetOffsets.count();
  uint32_t headerSize = ChainedSequenceLookupTable::kBaseSize + ruleSetCount * 2u;

  if (!table.fits(headerSize))
    return validator.invalidTableSize(tableName, table.size, headerSize);

  const Offset16* ruleSetOffsetArray = table->ruleSetOffsets.array();
  OffsetRange ruleSetOffsetRange{headerSize, table.size - 4u};

  for (uint32_t i = 0; i < ruleSetCount; i++) {
    uint32_t ruleSetOffset = ruleSetOffsetArray[i].value();

    // Offsets are allowed to be null - this means that that ChainedSequenceRuleSet must be ignored.
    if (!ruleSetOffset)
      continue;

    if (!ruleSetOffsetRange.contains(ruleSetOffset))
      return validator.invalidOffsetEntry(tableName, "ruleSetOffset", i, ruleSetOffset, ruleSetOffsetRange);

    Table<ChainedSequenceRuleSet> ruleSet(table.subTable(ruleSetOffset));
    uint32_t ruleCount = ruleSet->count();

    if (!ruleCount)
      return validator.fail("%s.ruleSet[%u] cannot be empty", tableName, i);

    uint32_t ruleSetHeaderSize = 2u + ruleCount * 2u;
    if (!ruleSet.fits(ruleSetHeaderSize))
      return validator.fail("%s.ruleSet[%u] is truncated (size=%u, required=%u)", tableName, i, ruleSet.size, ruleSetHeaderSize);

    const Offset16* ruleOffsetArray = ruleSet->array();
    OffsetRange ruleOffsetRange{ruleSetHeaderSize, ruleSet.size - ChainedSequenceRule::kBaseSize};

    for (uint32_t ruleIndex = 0; ruleIndex < ruleCount; ruleIndex++) {
      uint32_t ruleOffset = ruleOffsetArray[ruleIndex].value();
      if (!ruleOffsetRange.contains(ruleOffset))
        return validator.fail("%s.ruleSet[%u].rule[%u] offset (%u) is out of range [%u:%u]", tableName, i, ruleIndex, ruleOffset, ruleOffsetRange.start, ruleOffsetRange.end);

      Table<ChainedSequenceRule> rule = ruleSet.subTable(ruleOffset);
      uint32_t backtrackGlyphCount = rule->backtrackGlyphCount();

      // Verify there is a room for `backgrackGlyphCount + backtrackSequence + inputGlyphCount`.
      uint32_t inputGlyphOffset = 2u + backtrackGlyphCount * 2u;
      if (!rule.fits(inputGlyphOffset + 2u))
        return validator.fail("%s.ruleSet[%u].rule[%u] is truncated (size=%u, required=%u)", tableName, i, ruleIndex, rule.size, inputGlyphOffset + 2u);

      uint32_t inputGlyphCount = rule.readU16(inputGlyphOffset);
      if (!inputGlyphCount)
        return validator.fail("%s.ruleSet[%u].rule[%u] has invalid inputGlyphCount (%u)", tableName, i, ruleIndex, inputGlyphCount);

      // Verify there is a room for `inputGlyphCount + inputSequence + lookaheadGlyphCount`.
      uint32_t lookaheadOffset = inputGlyphOffset + 2u + (inputGlyphCount - 1u) * 2u;
      if (!rule.fits(lookaheadOffset + 2u))
        return validator.fail("%s.ruleSet[%u].rule[%u] is truncated (size=%u, required=%u)", tableName, i, ruleIndex, rule.size, lookaheadOffset + 2u);

      // Verify there is a room for `lookaheadSequence + lookupRecordCount`.
      uint32_t lookaheadGlyphCount = rule.readU16(lookaheadOffset);
      uint32_t lookupRecordOffset = lookaheadOffset + lookaheadGlyphCount * 2u;
      if (!rule.fits(lookupRecordOffset + 2u))
        return validator.fail("%s.ruleSet[%u].rule[%u] is truncated (size=%u, required=%u)", tableName, i, ruleIndex, rule.size, lookupRecordOffset + 2u);

      uint32_t lookupRecordCount = rule.readU16(lookupRecordOffset);
      if (!lookupRecordCount)
        return validator.fail("%s.ruleSet[%u].rule[%u] has invalid lookupRecordCount (%u)", tableName, i, ruleIndex, lookupRecordCount);

      const GSubGPosTable::SequenceLookupRecord* lookupRecordArray = PtrOps::offset<const GSubGPosTable::SequenceLookupRecord>(rule.data, lookupRecordOffset + 2u);
      if (!validateSequenceLookupRecordArray(validator, lookupRecordArray, lookupRecordCount))
        return false;
    }
  }

  return true;
}

static BL_INLINE bool validateChainedContextFormat1(ValidationContext& validator, Table<GSubGPosTable::ChainedSequenceContext1> table, const char* tableName) noexcept {
  return validateChainedContextFormat1_2(validator, table, tableName);
}

static BL_NOINLINE bool validateChainedContextFormat2(ValidationContext& validator, Table<GSubGPosTable::ChainedSequenceContext2> table, const char* tableName) noexcept {
  if (!table.fits())
    return validator.invalidTableSize(tableName, table.size, GSubGPosTable::SequenceContext2::kBaseSize);

  uint32_t ruleSetCount = table->ruleSetOffsets.count();
  uint32_t headerSize = GSubGPosTable::SequenceContext2::kBaseSize + ruleSetCount * 2u;
  OffsetRange offsetRange{headerSize, table.size};

  if (!table.fits(headerSize))
    return validator.invalidTableSize(tableName, table.size, headerSize);

  uint32_t backtrackClassDefOffset = table->backtrackClassDefOffset();
  uint32_t inputClassDefOffset = table->inputClassDefOffset();
  uint32_t lookaheadClassDefOffset = table->lookaheadClassDefOffset();

  if (!offsetRange.contains(backtrackClassDefOffset))
    return validator.invalidFieldOffset(tableName, "backtrackClassDefOffset", backtrackClassDefOffset, offsetRange);

  if (!offsetRange.contains(inputClassDefOffset))
    return validator.invalidFieldOffset(tableName, "inputClassDefOffset", inputClassDefOffset, offsetRange);

  if (!offsetRange.contains(lookaheadClassDefOffset))
    return validator.invalidFieldOffset(tableName, "lookaheadClassDefOffset", lookaheadClassDefOffset, offsetRange);

  if (!validateClassDefTable(validator, table.subTableUnchecked(backtrackClassDefOffset), "backtrackClassDef"))
    return false;

  if (!validateClassDefTable(validator, table.subTableUnchecked(inputClassDefOffset), "inputClassDef"))
    return false;

  if (!validateClassDefTable(validator, table.subTableUnchecked(lookaheadClassDefOffset), "lookaheadClassDef"))
    return false;

  return validateChainedContextFormat1_2(validator, table, tableName);
}

static BL_NOINLINE bool validateChainedContextFormat3(ValidationContext& validator, Table<GSubGPosTable::ChainedSequenceContext3> table, const char* tableName) noexcept {
  if (!table.fits())
    return validator.invalidTableSize(tableName, table.size, GSubGPosTable::ChainedSequenceContext3::kBaseSize);

  uint32_t backtrackGlyphCount = table->backtrackGlyphCount();
  uint32_t inputGlyphCountOffset = 4u + backtrackGlyphCount * 2u;

  if (!table.fits(inputGlyphCountOffset + 2u))
    return validator.invalidTableSize(tableName, table.size, inputGlyphCountOffset + 2u);

  uint32_t inputGlyphCount = table.readU16(inputGlyphCountOffset);
  uint32_t lookaheadGlyphCountOffset = inputGlyphCountOffset + 2u + inputGlyphCount * 2u;

  if (!table.fits(lookaheadGlyphCountOffset + 2u))
    return validator.invalidTableSize(tableName, table.size, lookaheadGlyphCountOffset + 2u);

  uint32_t lookaheadGlyphCount = table.readU16(lookaheadGlyphCountOffset);
  uint32_t lookupRecordCountOffset = lookaheadGlyphCountOffset + 2u + lookaheadGlyphCount * 2u;

  if (!table.fits(lookupRecordCountOffset + 2u))
    return validator.invalidTableSize(tableName, table.size, lookupRecordCountOffset + 2u);

  uint32_t lookupRecordCount = table.readU16(lookupRecordCountOffset);
  uint32_t headerSize = lookupRecordCountOffset + 2u + lookupRecordCount * GSubGPosTable::SequenceLookupRecord::kBaseSize;

  if (!lookupRecordCount)
    return validator.fail("%s has no lookup records", tableName);

  if (!table.fits(headerSize))
    return validator.invalidTableSize(tableName, table.size, headerSize);

  OffsetRange offsetRange{headerSize, table.size - 2u};

  const UInt16* backtrackCoverageOffsets = table->backtrackCoverageOffsets();
  const UInt16* inputGlyphCoverageOffsets = PtrOps::offset<const UInt16>(table.data, inputGlyphCountOffset + 2u);
  const UInt16* lookaheadCoverageOffsets = PtrOps::offset<const UInt16>(table.data, lookaheadGlyphCountOffset + 2u);

  if (!validateCoverageTables(validator, table, tableName, "backtrack", backtrackCoverageOffsets, backtrackGlyphCount, offsetRange))
    return false;

  if (!validateCoverageTables(validator, table, tableName, "input", inputGlyphCoverageOffsets, inputGlyphCount, offsetRange))
    return false;

  if (!validateCoverageTables(validator, table, tableName, "lookahead", lookaheadCoverageOffsets, lookaheadGlyphCount, offsetRange))
    return false;

  const GSubGPosTable::SequenceLookupRecord* lookupRecordArray = PtrOps::offset<const GSubGPosTable::SequenceLookupRecord>(table.data, lookupRecordCountOffset + 2u);
  return validateSequenceLookupRecordArray(validator, lookupRecordArray, lookupRecordCount);
}

// bl::OpenType::LayoutImpl - GSUB & GPOS - Chained Sequence Context Lookup
// ========================================================================

struct ChainedMatchContext {
  RawTable table;
  GlyphRange firstGlyphRange;

  BLGlyphId* backGlyphPtr;
  BLGlyphId* aheadGlyphPtr;

  size_t backGlyphCount;
  size_t aheadGlyphCount;
};

static BL_INLINE bool matchBackGlyphsFormat1(const BLGlyphId* glyphPtr, const UInt16* matchSequence, size_t count) noexcept {
  const BLGlyphId* glyphStart = glyphPtr - count;

  while (glyphPtr != glyphStart) {
    if (glyphPtr[-1] != matchSequence[0].value())
      return false;
    glyphPtr--;
    matchSequence++;
  }

  return true;
}

template<uint32_t kCDFmt>
static BL_INLINE bool matchBackGlyphsFormat2(const BLGlyphId* glyphPtr, const UInt16* matchSequence, size_t count, const ClassDefTableIterator& cdIt) noexcept {
  const BLGlyphId* glyphStart = glyphPtr - count;

  while (glyphPtr != glyphStart) {
    BLGlyphId glyphId = glyphPtr[-1];
    uint32_t classValue = matchSequence[0].value();

    if (!cdIt.matchGlyphClass<kCDFmt>(glyphId, classValue))
      return false;

    glyphPtr--;
    matchSequence++;
  }

  return true;
}

static BL_INLINE bool matchBackGlyphsFormat3(RawTable mainTable, const BLGlyphId* glyphPtr, const Offset16* backtrackCoverageOffsetArray, size_t count) noexcept {
  for (size_t i = 0; i < count; i++, glyphPtr--) {
    CoverageTableIterator covIt;
    uint32_t covFmt = covIt.init(mainTable.subTableUnchecked(backtrackCoverageOffsetArray[i].value()));
    BLGlyphId glyphId = glyphPtr[0];

    if (covFmt == 1) {
      uint32_t unusedCoverageIndex;
      if (!covIt.glyphRange<1>().contains(glyphId) || !covIt.find<1>(glyphId, unusedCoverageIndex))
        return false;
    }
    else {
      uint32_t unusedCoverageIndex;
      if (!covIt.glyphRange<2>().contains(glyphId) || !covIt.find<2>(glyphId, unusedCoverageIndex))
        return false;
    }
  }

  return true;
}

static BL_INLINE bool matchAheadGlyphsFormat1(const BLGlyphId* glyphPtr, const UInt16* matchSequence, size_t count) noexcept {
  for (size_t i = 0; i < count; i++)
    if (glyphPtr[i] != matchSequence[i].value())
      return false;
  return true;
}

template<uint32_t kCDFmt>
static BL_INLINE bool matchAheadGlyphsFormat2(const BLGlyphId* glyphPtr, const UInt16* matchSequence, size_t count, const ClassDefTableIterator& cdIt) noexcept {
  for (size_t i = 0; i < count; i++) {
    BLGlyphId glyphId = glyphPtr[i];
    uint32_t classValue = matchSequence[i].value();

    if (!cdIt.matchGlyphClass<kCDFmt>(glyphId, classValue))
      return false;
  }
  return true;
}

static BL_INLINE bool matchAheadGlyphsFormat3(RawTable mainTable, const BLGlyphId* glyphPtr, const Offset16* lookaheadCoverageOffsetArray, size_t count) noexcept {
  for (size_t i = 0; i < count; i++) {
    CoverageTableIterator covIt;
    uint32_t covFmt = covIt.init(mainTable.subTableUnchecked(lookaheadCoverageOffsetArray[i].value()));
    BLGlyphId glyphId = glyphPtr[i];

    if (covFmt == 1) {
      uint32_t unusedCoverageIndex;
      if (!covIt.glyphRange<1>().contains(glyphId) || !covIt.find<1>(glyphId, unusedCoverageIndex))
        return false;
    }
    else {
      uint32_t unusedCoverageIndex;
      if (!covIt.glyphRange<2>().contains(glyphId) || !covIt.find<2>(glyphId, unusedCoverageIndex))
        return false;
    }
  }

  return true;
}

static BL_INLINE bool matchChainedSequenceRuleFormat1(
  ChainedMatchContext& mCtx,
  Table<Array16<Offset16>> ruleOffsets, uint32_t ruleCount,
  SequenceMatch* matchOut) noexcept {

  for (uint32_t ruleIndex = 0; ruleIndex < ruleCount; ruleIndex++) {
    uint32_t ruleOffset = ruleOffsets->array()[ruleIndex].value();
    BL_ASSERT_VALIDATED(ruleOffset <= ruleOffsets.size - GSubGPosTable::ChainedSequenceRule::kBaseSize);

    Table<GSubGPosTable::ChainedSequenceRule> rule = ruleOffsets.subTableUnchecked(ruleOffset);
    uint32_t backtrackGlyphCount = rule->backtrackGlyphCount();

    uint32_t inputGlyphOffset = 2u + backtrackGlyphCount * 2u;
    BL_ASSERT_VALIDATED(rule.fits(inputGlyphOffset + 2u));

    uint32_t inputGlyphCount = rule.readU16(inputGlyphOffset);
    BL_ASSERT_VALIDATED(inputGlyphCount != 0u);

    uint32_t lookaheadOffset = inputGlyphOffset + 2u + inputGlyphCount * 2u - 2u;
    BL_ASSERT_VALIDATED(rule.fits(lookaheadOffset + 2u));

    // Multiple conditions merged into a single one that results in a branch.
    uint32_t lookaheadGlyphCount = rule.readU16(lookaheadOffset);
    if (unsigned(mCtx.backGlyphCount < backtrackGlyphCount) | unsigned(mCtx.aheadGlyphCount < inputGlyphCount + lookaheadGlyphCount))
      continue;

    // Match backtrack glyphs, which are stored in reverse order in backtrack array, index 0 describes the last glyph).
    if (!matchBackGlyphsFormat1(mCtx.backGlyphPtr + mCtx.backGlyphCount, rule->backtrackSequence(), backtrackGlyphCount))
      continue;

    // Match input and lookahead glyphs.
    if (!matchAheadGlyphsFormat1(mCtx.aheadGlyphPtr, rule.dataAs<UInt16>(inputGlyphOffset + 2u), inputGlyphCount - 1u))
      continue;

    if (!matchAheadGlyphsFormat1(mCtx.aheadGlyphPtr + inputGlyphCount - 1u, rule.dataAs<UInt16>(lookaheadOffset + 2u), lookaheadGlyphCount))
      continue;

    uint32_t lookupRecordOffset = lookaheadOffset + lookaheadGlyphCount * 2u;
    BL_ASSERT_VALIDATED(rule.fits(lookupRecordOffset + 2u));

    uint32_t lookupRecordCount = rule.readU16(lookupRecordOffset);
    BL_ASSERT_VALIDATED(rule.fits(lookupRecordOffset + 2u + lookupRecordCount * GSubGPosTable::SequenceLookupRecord::kBaseSize));

    *matchOut = SequenceMatch{inputGlyphCount, lookupRecordCount, rule.dataAs<GSubGPosTable::SequenceLookupRecord>(lookupRecordOffset + 2u)};
    return true;
  }

  return false;
}

template<uint32_t kCD1Fmt, uint32_t kCD2Fmt, uint32_t kCD3Fmt>
static BL_INLINE bool matchChainedSequenceRuleFormat2(
  ChainedMatchContext& mCtx,
  Table<Array16<Offset16>> ruleOffsets, uint32_t ruleCount,
  const ClassDefTableIterator& cd1It, const ClassDefTableIterator& cd2It, const ClassDefTableIterator& cd3It,
  SequenceMatch* matchOut) noexcept {

  for (uint32_t ruleIndex = 0; ruleIndex < ruleCount; ruleIndex++) {
    uint32_t ruleOffset = ruleOffsets->array()[ruleIndex].value();
    BL_ASSERT_VALIDATED(ruleOffset <= ruleOffsets.size - GSubGPosTable::ChainedSequenceRule::kBaseSize);

    Table<GSubGPosTable::ChainedSequenceRule> rule = ruleOffsets.subTableUnchecked(ruleOffset);
    uint32_t backtrackGlyphCount = rule->backtrackGlyphCount();

    uint32_t inputGlyphOffset = 2u + backtrackGlyphCount * 2u;
    BL_ASSERT_VALIDATED(rule.fits(inputGlyphOffset + 2u));

    uint32_t inputGlyphCount = rule.readU16(inputGlyphOffset);
    BL_ASSERT_VALIDATED(inputGlyphCount != 0u);

    uint32_t lookaheadOffset = inputGlyphOffset + 2u + inputGlyphCount * 2u - 2u;
    BL_ASSERT_VALIDATED(rule.fits(lookaheadOffset + 2u));

    // Multiple conditions merged into a single one that results in a branch.
    uint32_t lookaheadGlyphCount = rule.readU16(lookaheadOffset);
    if (unsigned(mCtx.backGlyphCount < backtrackGlyphCount) | unsigned(mCtx.aheadGlyphCount < inputGlyphCount + lookaheadGlyphCount))
      continue;

    // Match backtrack glyphs, which are stored in reverse order in backtrack array, index 0 describes the last glyph).
    if (!matchBackGlyphsFormat2<kCD1Fmt>(mCtx.backGlyphPtr + mCtx.backGlyphCount, rule->backtrackSequence(), backtrackGlyphCount, cd1It))
      continue;

    // Match input and lookahead glyphs.
    if (!matchAheadGlyphsFormat2<kCD2Fmt>(mCtx.aheadGlyphPtr, rule.dataAs<UInt16>(inputGlyphOffset + 2u), inputGlyphCount - 1u, cd2It))
      continue;

    if (!matchAheadGlyphsFormat2<kCD3Fmt>(mCtx.aheadGlyphPtr + inputGlyphCount - 1u, rule.dataAs<UInt16>(lookaheadOffset + 2u), lookaheadGlyphCount, cd3It))
      continue;

    uint32_t lookupRecordOffset = lookaheadOffset + lookaheadGlyphCount * 2u;
    BL_ASSERT_VALIDATED(rule.fits(lookupRecordOffset + 2u));

    uint32_t lookupRecordCount = rule.readU16(lookupRecordOffset);
    BL_ASSERT_VALIDATED(rule.fits(lookupRecordOffset + 2u + lookupRecordCount * GSubGPosTable::SequenceLookupRecord::kBaseSize));

    *matchOut = SequenceMatch{inputGlyphCount, lookupRecordCount, rule.dataAs<GSubGPosTable::SequenceLookupRecord>(lookupRecordOffset + 2u)};
    return true;
  }

  return false;
}

template<uint32_t kCovFmt>
static BL_INLINE bool matchChainedSequenceFormat1(
  ChainedMatchContext& mCtx,
  const Offset16* ruleSetOffsets, uint32_t ruleSetCount,
  const CoverageTableIterator& covIt,
  SequenceMatch* matchOut) noexcept {

  BLGlyphId glyphId = mCtx.aheadGlyphPtr[0];
  if (!mCtx.firstGlyphRange.contains(glyphId))
    return false;

  uint32_t coverageIndex;
  if (!covIt.find<kCovFmt>(glyphId, coverageIndex) || coverageIndex >= ruleSetCount)
    return false;

  uint32_t ruleSetOffset = ruleSetOffsets[coverageIndex].value();
  BL_ASSERT_VALIDATED(ruleSetOffset <= mCtx.table.size - 2u);

  Table<Array16<Offset16>> ruleOffsets(mCtx.table.subTableUnchecked(ruleSetOffset));
  uint32_t ruleCount = ruleOffsets->count();
  BL_ASSERT_VALIDATED(ruleCount && ruleSetOffset + ruleCount * 2u <= mCtx.table.size - 2u);

  return matchChainedSequenceRuleFormat1(mCtx, ruleOffsets, ruleCount, matchOut);
}

template<uint32_t kCovFmt, uint32_t kCD1Fmt, uint32_t kCD2Fmt, uint32_t kCD3Fmt>
static BL_INLINE bool matchChainedSequenceFormat2(
  ChainedMatchContext& mCtx,
  const Offset16* ruleSetOffsets, uint32_t ruleSetCount,
  const CoverageTableIterator& covIt, const ClassDefTableIterator& cd1It, const ClassDefTableIterator& cd2It, const ClassDefTableIterator& cd3It,
  SequenceMatch* matchOut) noexcept {

  BLGlyphId glyphId = mCtx.aheadGlyphPtr[0];
  if (!mCtx.firstGlyphRange.contains(glyphId))
    return false;

  uint32_t coverageIndex;
  if (!covIt.find<kCovFmt>(glyphId, coverageIndex) || coverageIndex >= ruleSetCount)
    return false;

  uint32_t ruleSetOffset = ruleSetOffsets[coverageIndex].value();
  BL_ASSERT_VALIDATED(ruleSetOffset <= mCtx.table.size - 2u);

  Table<Array16<Offset16>> ruleOffsets(mCtx.table.subTableUnchecked(ruleSetOffset));
  uint32_t ruleCount = ruleOffsets->count();
  BL_ASSERT_VALIDATED(ruleCount && ruleSetOffset + ruleCount * 2u <= mCtx.table.size - 2u);

  return matchChainedSequenceRuleFormat2<kCD1Fmt, kCD2Fmt, kCD3Fmt>(mCtx, ruleOffsets, ruleCount, cd1It, cd2It, cd3It, matchOut);
}

static BL_INLINE bool matchChainedSequenceFormat3(
  ChainedMatchContext& mCtx,
  const UInt16* backtrackCoverageOffsetArray, uint32_t backtrackGlyphCount,
  const UInt16* inputCoverageOffsetArray, uint32_t inputGlyphCount,
  const UInt16* lookaheadCoverageOffsetArray, uint32_t lookaheadGlyphCount,
  GlyphRange firstGlyphRange,
  CoverageTableIterator& cov0It, uint32_t cov0Fmt) noexcept {

  BL_ASSERT(mCtx.backGlyphCount >= backtrackGlyphCount);
  BL_ASSERT(mCtx.aheadGlyphCount >= inputGlyphCount + lookaheadGlyphCount);

  BLGlyphId glyphId = mCtx.aheadGlyphPtr[0];
  if (!firstGlyphRange.contains(glyphId))
    return false;

  uint32_t unusedCoverageIndex0;
  if (!cov0It.findWithFormat(cov0Fmt, glyphId, unusedCoverageIndex0))
    return false;

  for (uint32_t i = 1; i < inputGlyphCount; i++) {
    CoverageTableIterator covItN;
    uint32_t covFmtN = covItN.init(mCtx.table.subTableUnchecked(inputCoverageOffsetArray[i].value()));
    GlyphRange glyphRangeN = covItN.glyphRangeWithFormat(covFmtN);

    uint32_t glyphIdN = mCtx.aheadGlyphPtr[i];
    uint32_t unusedCoverageIndexN;

    if (!glyphRangeN.contains(glyphIdN) || !covItN.findWithFormat(covFmtN, glyphIdN, unusedCoverageIndexN))
      return false;
  }

  return matchBackGlyphsFormat3(mCtx.table, mCtx.backGlyphPtr + mCtx.backGlyphCount - 1u, backtrackCoverageOffsetArray, backtrackGlyphCount) &&
         matchAheadGlyphsFormat3(mCtx.table, mCtx.aheadGlyphPtr + inputGlyphCount, lookaheadCoverageOffsetArray, lookaheadGlyphCount);
}

// bl::OpenType::LayoutImpl - GSUB - Lookup Type #1 - Single Substitution Validation
// =================================================================================

static BL_INLINE_IF_NOT_DEBUG bool validateGSubLookupType1Format1(ValidationContext& validator, Table<GSubTable::SingleSubst1> table) noexcept {
  const char* tableName = "SingleSubst1";
  uint32_t unusedCoverageCount;
  return validateLookupWithCoverage(validator, table, tableName, GSubTable::SingleSubst1::kBaseSize, unusedCoverageCount);
}

static BL_INLINE_IF_NOT_DEBUG bool validateGSubLookupType1Format2(ValidationContext& validator, Table<GSubTable::SingleSubst2> table) noexcept {
  const char* tableName = "SingleSubst2";

  uint32_t coverageCount;
  if (!validateLookupWithCoverage(validator, table, tableName, GSubTable::SingleSubst2::kBaseSize, coverageCount))
    return false;

  const GSubTable::SingleSubst2* lookup = table.dataAs<GSubTable::SingleSubst2>();
  uint32_t glyphCount = lookup->glyphs.count();
  uint32_t headerSize = GSubTable::SingleSubst2::kBaseSize + glyphCount * 2u;

  if (!table.fits(headerSize))
    return validator.invalidTableSize(tableName, table.size, headerSize);

  if (glyphCount < coverageCount)
    validator.warn("%s has less glyphs (%u) than coverage entries (%u)", tableName, glyphCount, coverageCount);

  return true;
}

// bl::OpenType::LayoutImpl - GSUB - Lookup Type #1 - Single Substitution Lookup
// =============================================================================

template<uint32_t kCovFmt, typename ApplyScope>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGSubLookupType1Format1(GSubContext& ctx, Table<GSubTable::SingleSubst1> table, ApplyScope scope, LookupFlags flags, const CoverageTableIterator& covIt) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  BLGlyphId* glyphPtr = ctx.glyphData() + scope.index();
  BLGlyphId* glyphEnd = ctx.glyphData() + scope.end();

  // Cannot apply a lookup if there is no data to be processed.
  BL_ASSERT(glyphPtr != glyphEnd);

  uint32_t glyphDelta = uint16_t(table->deltaGlyphId());
  GlyphRange glyphRange = covIt.glyphRange<kCovFmt>();

  do {
    BLGlyphId glyphId = glyphPtr[0];
    if (glyphRange.contains(glyphId)) {
      uint32_t unusedCoverageIndex;
      if (covIt.find<kCovFmt>(glyphId, unusedCoverageIndex))
        glyphPtr[0] = (glyphId + glyphDelta) & 0xFFFFu;
    }
  } while (scope.isRange() && ++glyphPtr != glyphEnd);

  return BL_SUCCESS;
}

template<uint32_t kCovFmt, typename ApplyScope>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGSubLookupType1Format2(GSubContext& ctx, Table<GSubTable::SingleSubst2> table, ApplyScope scope, LookupFlags flags, const CoverageTableIterator& covIt) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  BLGlyphId* glyphPtr = ctx.glyphData() + scope.index();
  BLGlyphId* glyphEnd = ctx.glyphData() + scope.end();

  // Cannot apply a lookup if there is no data to be processed.
  BL_ASSERT(glyphPtr != glyphEnd);

  uint32_t substCount = table->glyphs.count();
  GlyphRange glyphRange = covIt.glyphRange<kCovFmt>();
  BL_ASSERT_VALIDATED(table.fits(GSubTable::SingleSubst2::kBaseSize + substCount * 2u));

  do {
    BLGlyphId glyphId = glyphPtr[0];
    if (glyphRange.contains(glyphId)) {
      uint32_t coverageIndex;
      if (covIt.find<kCovFmt>(glyphId, coverageIndex) && coverageIndex < substCount) {
        glyphPtr[0] = table->glyphs.array()[coverageIndex].value();
      }
    }
  } while (scope.isRange() && ++glyphPtr != glyphEnd);

  return BL_SUCCESS;
}

// bl::OpenType::LayoutImpl - GSUB - Lookup Type #2 - Multiple Substitution Validation
// ===================================================================================

static BL_INLINE_IF_NOT_DEBUG bool validateGSubLookupType2Format1(ValidationContext& validator, Table<GSubTable::MultipleSubst1> table) noexcept {
  const char* tableName = "MultipleSubst1";

  uint32_t coverageCount;
  if (!validateLookupWithCoverage(validator, table, tableName, GSubTable::MultipleSubst1::kBaseSize, coverageCount))
    return false;

  uint32_t sequenceSetCount = table->sequenceOffsets.count();
  uint32_t headerSize = GSubTable::MultipleSubst1::kBaseSize + sequenceSetCount * 2u;

  if (!table.fits(headerSize))
    return validator.invalidTableSize(tableName, table.size, headerSize);

  if (sequenceSetCount < coverageCount)
    validator.warn("%s has less sequence sets (%u) than coverage entries (%u)", tableName, sequenceSetCount, coverageCount);

  // Offsets to glyph sequences.
  const Offset16* offsetArray = table->sequenceOffsets.array();
  OffsetRange offsetRange{headerSize, table.size - 4u};

  for (uint32_t i = 0; i < sequenceSetCount; i++) {
    uint32_t sequenceOffset = offsetArray[i].value();
    if (!offsetRange.contains(sequenceOffset))
      return validator.invalidOffsetEntry(tableName, "sequenceOffsets", i, sequenceOffset, offsetRange);

    Table<Array16<UInt16>> sequence(table.subTable(sequenceOffset));

    // NOTE: The OpenType specification explicitly forbids empty sequences (aka removing glyphs), however,
    // this is actually used in practice (actually by fonts from MS), so we just allow it as others do...
    uint32_t sequenceLength = sequence->count();
    uint32_t sequenceTableSize = 2u + sequenceLength * 2u;

    if (sequence.fits(sequenceTableSize))
      return validator.fail("%s.sequence[%u] is truncated (size=%u, required=%u)", tableName, i, sequence.size, sequenceTableSize);
  }

  return true;
}

// bl::OpenType::LayoutImpl - GSUB - Lookup Type #2 - Multiple Substitution Lookup
// ===============================================================================

// TODO: [OpenType] [SECURITY] What if the glyph contains kSeqMask???
template<uint32_t kCovFmt, typename ApplyScope>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGSubLookupType2Format1(GSubContext& ctx, Table<GSubTable::MultipleSubst1> table, ApplyScope scope, LookupFlags flags, const CoverageTableIterator& covIt) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  constexpr BLGlyphId kSequenceMarker = 0x80000000u;

  BLGlyphId* glyphInPtr = ctx.glyphData() + scope.index();
  BLGlyphId* glyphInEnd = ctx.glyphData() + scope.end();

  // Cannot apply a lookup if there is no data to be processed.
  BL_ASSERT(glyphInPtr != glyphInEnd);

  uint32_t sequenceSetCount = table->sequenceOffsets.count();
  BL_ASSERT_VALIDATED(table.fits(GSubTable::MultipleSubst1::kBaseSize + sequenceSetCount * 2u));

  size_t replacedGlyphCount = 0;
  size_t replacedSequenceSize = 0;
  GlyphRange glyphRange = covIt.glyphRange<kCovFmt>();

  do {
    BLGlyphId glyphId = glyphInPtr[0];
    if (glyphRange.contains(glyphId)) {
      uint32_t coverageIndex;
      if (covIt.find<kCovFmt>(glyphId, coverageIndex) && coverageIndex < sequenceSetCount) {
        uint32_t sequenceOffset = table->sequenceOffsets.array()[coverageIndex].value();
        BL_ASSERT_VALIDATED(sequenceOffset <= table.size - 2u);

        uint32_t sequenceLength = MemOps::readU16uBE(table.data + sequenceOffset);
        BL_ASSERT_VALIDATED(sequenceOffset + sequenceLength * 2u <= table.size - 2u);

        glyphInPtr[0] = sequenceOffset | kSequenceMarker;
        replacedGlyphCount++;
        replacedSequenceSize += sequenceLength;
      }
    }
  } while (scope.isRange() && ++glyphInPtr != glyphInEnd);

  // Not a single match if zero.
  if (!replacedGlyphCount)
    return BL_SUCCESS;

  // We could be only processing a range withing the work buffer, thus it's not safe to reuse `glyphInPtr`.
  glyphInPtr = ctx.glyphData() + ctx.size();

  BLGlyphId* glyphInStart = ctx.glyphData();
  BLGlyphInfo* infoInPtr = ctx.infoData() + ctx.size();

  size_t sizeAfter = ctx.size() - replacedGlyphCount + replacedSequenceSize;
  BL_PROPAGATE(ctx.ensureWorkBuffer(sizeAfter));

  BLGlyphId* glyphOutPtr = ctx.glyphData() + sizeAfter;
  BLGlyphInfo* infoOutPtr = ctx.infoData() + sizeAfter;

  // Second loop applies all matches that were found and marked.
  do {
    BLGlyphId glyphId = *--glyphInPtr;
    *--glyphOutPtr = glyphId;
    *--infoOutPtr = *--infoInPtr;

    if (glyphId & kSequenceMarker) {
      size_t sequenceOffset = glyphId & ~kSequenceMarker;
      size_t sequenceLength = MemOps::readU16uBE(table.data + sequenceOffset);
      const UInt16* sequenceData = table.dataAs<UInt16>(sequenceOffset + 2u);

      glyphOutPtr -= sequenceLength;
      infoOutPtr -= sequenceLength;

      while (sequenceLength) {
        sequenceLength--;
        glyphOutPtr[sequenceLength] = sequenceData[sequenceLength].value();
        infoOutPtr[sequenceLength] = *infoInPtr;
      }
    }
  } while (glyphInPtr != glyphInStart);

  BL_ASSERT(glyphOutPtr == ctx.glyphData());
  return BL_SUCCESS;
}

// bl::OpenType::LayoutImpl - GSUB - Lookup Type #3 - Alternate Substitution Validation
// ====================================================================================

static BL_INLINE_IF_NOT_DEBUG bool validateGSubLookupType3Format1(ValidationContext& validator, RawTable table) noexcept {
  const char* tableName = "AlternateSubst1";

  uint32_t coverageCount;
  if (!validateLookupWithCoverage(validator, table, tableName, GSubTable::AlternateSubst1::kBaseSize, coverageCount))
    return false;

  const GSubTable::AlternateSubst1* lookup = table.dataAs<GSubTable::AlternateSubst1>();
  uint32_t alternateSetCount = lookup->alternateSetOffsets.count();
  uint32_t headerSize = GSubTable::AlternateSubst1::kBaseSize + alternateSetCount * 2u;

  if (!table.fits(headerSize))
    return validator.invalidTableSize(tableName, table.size, headerSize);

  // Offsets to AlternateSet tables.
  const Offset16* offsetArray = lookup->alternateSetOffsets.array();
  OffsetRange offsetRange{headerSize, table.size - 4u};

  if (alternateSetCount < coverageCount)
    validator.warn("%s has less AlternateSet records (%u) than coverage entries (%u)", tableName, alternateSetCount, coverageCount);

  for (uint32_t i = 0; i < alternateSetCount; i++) {
    uint32_t offset = offsetArray[i].value();
    if (!offsetRange.contains(offset))
      return validator.invalidOffsetEntry(tableName, "alternateSetOffsets", i, offset, offsetRange);

    const Array16<UInt16>* alternateSet = PtrOps::offset<const Array16<UInt16>>(table.data, offset);
    uint32_t alternateSetLength = alternateSet->count();

    // Specification forbids an empty AlternateSet.
    if (!alternateSetLength)
      return validator.fail("%s.alternateSet[%u] cannot be empty", tableName, i);

    uint32_t alternateSetTableEnd = offset + 2u + alternateSetLength * 2u;
    if (alternateSetTableEnd > table.size)
      return validator.fail("%s.alternateSet[%u] overflows table size by %u bytes", tableName, i, table.size - alternateSetTableEnd);
  }

  return true;
}

// bl::OpenType::LayoutImpl - GSUB - Lookup Type #3 - Alternate Substitution Lookup
// ================================================================================

template<uint32_t kCovFmt, typename ApplyScope>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGSubLookupType3Format1(GSubContext& ctx, Table<GSubTable::AlternateSubst1> table, ApplyScope scope, LookupFlags flags, const CoverageTableIterator& covIt) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  BLGlyphId* glyphPtr = ctx.glyphData() + scope.index();
  BLGlyphId* glyphEnd = ctx.glyphData() + scope.end();

  // Cannot apply a lookup if there is no data to be processed.
  BL_ASSERT(glyphPtr != glyphEnd);

  uint32_t alternateSetCount = table->alternateSetOffsets.count();
  GlyphRange glyphRange = covIt.glyphRange<kCovFmt>();
  BL_ASSERT_VALIDATED(table.fits(GSubTable::AlternateSubst1::kBaseSize + alternateSetCount * 2u));

  // TODO: [OpenType] Not sure how the index should be selected (AlternateSubst1).
  uint32_t selectedIndex = 0u;

  do {
    BLGlyphId glyphId = glyphPtr[0];
    if (glyphRange.contains(glyphId)) {
      uint32_t coverageIndex;
      if (covIt.find<kCovFmt>(glyphId, coverageIndex) && coverageIndex < alternateSetCount) {
        uint32_t alternateSetOffset = table->alternateSetOffsets.array()[coverageIndex].value();
        BL_ASSERT_VALIDATED(alternateSetOffset <= table.size - 2u);

        const UInt16* alts = reinterpret_cast<const UInt16*>(table.data + alternateSetOffset + 2u);
        uint32_t altGlyphCount = alts[-1].value();
        BL_ASSERT_VALIDATED(altGlyphCount != 0u && alternateSetOffset + altGlyphCount * 2u <= table.size - 2u);

        uint32_t altGlyphIndex = (selectedIndex % altGlyphCount);
        glyphPtr[0] = alts[altGlyphIndex].value();
      }
    }
  } while (scope.isRange() && ++glyphPtr != glyphEnd);

  return BL_SUCCESS;
}

// bl::OpenType::LayoutImpl - GSUB - Lookup Type #4 - Ligature Substitution Validation
// ===================================================================================

static BL_INLINE_IF_NOT_DEBUG bool validateGSubLookupType4Format1(ValidationContext& validator, Table<GSubTable::LigatureSubst1> table) noexcept {
  const char* tableName = "LigatureSubst1";

  uint32_t coverageCount;
  if (!validateLookupWithCoverage(validator, table, tableName, GSubTable::LigatureSubst1::kBaseSize, coverageCount))
    return false;

  const GSubTable::LigatureSubst1* lookup = table.dataAs<GSubTable::LigatureSubst1>();
  uint32_t ligatureSetCount = lookup->ligatureSetOffsets.count();
  uint32_t headerSize = GSubTable::LigatureSubst1::kBaseSize + ligatureSetCount * 2u;

  if (!table.fits(headerSize))
    return validator.invalidTableSize(tableName, table.size, headerSize);

  if (ligatureSetCount < coverageCount)
    validator.warn("%s has less LigatureSet records (%u) than coverage entries (%u)", tableName, ligatureSetCount, coverageCount);

  // Offsets to LigatureSet tables.
  const Offset16* ligatureSetOffsetArray = lookup->ligatureSetOffsets.array();
  OffsetRange ligatureSetOffsetRange{headerSize, table.size-4u};

  for (uint32_t i = 0; i < ligatureSetCount; i++) {
    uint32_t ligatureSetOffset = ligatureSetOffsetArray[i].value();
    if (!ligatureSetOffsetRange.contains(ligatureSetOffset))
      return validator.invalidOffsetEntry(tableName, "ligatureSetOffsets", i, ligatureSetOffset, ligatureSetOffsetRange);

    Table<Array16<UInt16>> ligatureSet(table.subTable(ligatureSetOffset));

    uint32_t ligatureCount = ligatureSet->count();
    if (!ligatureCount)
      return validator.fail("%s.ligatureSet[%u] cannot be empty", tableName, i);

    uint32_t ligatureSetHeaderSize = 2u + ligatureCount * 2u;
    if (!ligatureSet.fits(ligatureSetHeaderSize))
      return validator.fail("%s.ligatureSet[%u] overflows the table size by [%u] bytes", tableName, i, ligatureSetHeaderSize - ligatureSet.size);

    const Offset16* ligatureOffsetArray = ligatureSet->array();
    OffsetRange ligatureOffsetRange{ligatureSetHeaderSize, ligatureSet.size - 6u};

    for (uint32_t ligatureIndex = 0; ligatureIndex < ligatureCount; ligatureIndex++) {
      uint32_t ligatureOffset = ligatureOffsetArray[ligatureIndex].value();
      if (!ligatureOffsetRange.contains(ligatureOffset))
        return validator.fail("%s.ligatureSet[%u] ligature[%u] offset (%u) is out of range [%u:%u]", tableName, i, ligatureIndex, ligatureOffset, headerSize, table.size);

      Table<GSubTable::Ligature> ligature = ligatureSet.subTable(ligatureOffset);
      uint32_t componentCount = ligature->glyphs.count();
      if (componentCount < 2u)
        return validator.fail("%s.ligatureSet[%u].ligature[%u] must have at least 2 glyphs, not %u", tableName, i, ligatureIndex, componentCount);

      uint32_t ligatureTableSize = 2u + componentCount * 2u;
      if (!ligature.fits(ligatureTableSize))
        return validator.fail("%s.ligatureSet[%u].ligature[%u] is truncated (size=%u, required=%u)", tableName, i, ligatureIndex, ligature.size, ligatureTableSize);
    }
  }

  return true;
}

// bl::OpenType::LayoutImpl - GSUB - Lookup Type #4 - Ligature Substitution Lookup
// ===============================================================================

static BL_INLINE bool matchLigature(
  Table<Array16<Offset16>> ligatureOffsets,
  uint32_t ligatureCount,
  const BLGlyphId* inGlyphData,
  size_t maxGlyphCount,
  uint32_t& ligatureGlyphIdOut,
  uint32_t& ligatureGlyphCount) noexcept {

  // Ligatures are ordered by preference. This means we have to go one by one.
  for (uint32_t i = 0; i < ligatureCount; i++) {
    uint32_t ligatureOffset = ligatureOffsets->array()[i].value();
    BL_ASSERT_VALIDATED(ligatureOffset <= ligatureOffsets.size - 4u);

    const GSubTable::Ligature* ligature = PtrOps::offset<const GSubTable::Ligature>(ligatureOffsets.data, ligatureOffset);
    ligatureGlyphCount = uint32_t(ligature->glyphs.count());
    if (ligatureGlyphCount > maxGlyphCount)
      continue;

    // This is safe - a single Ligature is 4 bytes + GlyphId[ligatureGlyphCount - 1]. MaxLigOffset is 4 bytes less than
    // the end to include the header, so we only have to include `ligatureGlyphCount * 2u` to verify we won't read beyond.
    BL_ASSERT_VALIDATED(ligatureOffset + ligatureGlyphCount * 2u <= ligatureOffsets.size - 4u);

    uint32_t glyphIndex = 1;
    for (;;) {
      BLGlyphId glyphA = ligature->glyphs.array()[glyphIndex-1].value();
      BLGlyphId glyphB = inGlyphData[glyphIndex];

      if (glyphA != glyphB)
        break;

      if (++glyphIndex < ligatureGlyphCount)
        continue;

      ligatureGlyphIdOut = ligature->ligatureGlyphId();
      return true;
    }
  }

  return false;
}

template<uint32_t kCovFmt, typename ApplyScope>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGSubLookupType4Format1(GSubContext& ctx, Table<GSubTable::LigatureSubst1> table, ApplyScope scope, LookupFlags flags, const CoverageTableIterator& covIt) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  BLGlyphId* glyphInPtr = ctx.glyphData() + scope.index();
  BLGlyphId* glyphInEnd = ctx.glyphData() + ctx.size();
  BLGlyphId* glyphInEndScope = ctx.glyphData() + scope.end();

  // Cannot apply a lookup if there is no data to be processed.
  BL_ASSERT(glyphInPtr != glyphInEndScope);

  uint32_t ligatureSetCount = table->ligatureSetOffsets.count();
  BL_ASSERT_VALIDATED(table.fits(GSubTable::LigatureSubst1::kBaseSize + ligatureSetCount * 2u));

  // Find the first ligature - if no ligature is matched, no buffer operation will be performed.
  BLGlyphId* glyphOutPtr = nullptr;
  GlyphRange glyphRange = covIt.glyphRange<kCovFmt>();

  for (;;) {
    BLGlyphId glyphId = glyphInPtr[0];
    if (glyphRange.contains(glyphId)) {
      uint32_t coverageIndex;
      if (covIt.find<kCovFmt>(glyphId, coverageIndex) && coverageIndex < ligatureSetCount) {
        uint32_t ligatureSetOffset = table->ligatureSetOffsets.array()[coverageIndex].value();
        BL_ASSERT_VALIDATED(ligatureSetOffset <= table.size - 2u);

        Table<Array16<Offset16>> ligatureOffsets(table.subTableUnchecked(ligatureSetOffset));
        uint32_t ligatureCount = ligatureOffsets->count();
        BL_ASSERT_VALIDATED(ligatureCount && ligatureSetOffset + ligatureCount * 2u <= table.size - 2u);

        BLGlyphId ligatureGlyphId;
        uint32_t ligatureGlyphCount;

        if (matchLigature(ligatureOffsets, ligatureCount, glyphInPtr, (size_t)(glyphInEnd - glyphInPtr), ligatureGlyphId, ligatureGlyphCount)) {
          *glyphInPtr = ligatureGlyphId;
          glyphOutPtr = glyphInPtr + 1u;

          glyphInPtr += ligatureGlyphCount;
          break;
        }
      }
    }

    if (++glyphInPtr == glyphInEndScope)
      return BL_SUCCESS;
  }

  // Secondary loop - applies the replacement in-place - the buffer will end up having less glyphs.
  size_t inIndex = size_t(glyphInPtr - ctx.glyphData());
  size_t outIndex = size_t(glyphOutPtr - ctx.glyphData());

  BLGlyphInfo* infoInPtr = ctx.infoData() + inIndex;
  BLGlyphInfo* infoOutPtr = ctx.infoData() + outIndex;

  // These is only a single possible match if the scope is only a single index (nested lookups).
  if (scope.isRange()) {
    while (glyphInPtr != glyphInEndScope) {
      BLGlyphId glyphId = glyphInPtr[0];
      if (glyphRange.contains(glyphId)) {
        uint32_t coverageIndex;
        if (covIt.find<kCovFmt>(glyphId, coverageIndex) && coverageIndex < ligatureSetCount) {
          uint32_t ligatureSetOffset = table->ligatureSetOffsets.array()[coverageIndex].value();
          BL_ASSERT_VALIDATED(ligatureSetOffset <= table.size - 2u);

          Table<Array16<Offset16>> ligatureOffsets(table.subTableUnchecked(ligatureSetOffset));
          uint32_t ligatureCount = ligatureOffsets->count();
          BL_ASSERT_VALIDATED(ligatureCount && ligatureSetOffset + ligatureCount * 2u <= table.size - 2u);

          BLGlyphId ligatureGlyphId;
          uint32_t ligatureGlyphCount;

          if (matchLigature(ligatureOffsets, ligatureCount, glyphInPtr, (size_t)(glyphInEnd - glyphInPtr), ligatureGlyphId, ligatureGlyphCount)) {
            *glyphOutPtr++ = ligatureGlyphId;
            *infoOutPtr++ = *infoInPtr;

            glyphInPtr += ligatureGlyphCount;
            infoInPtr += ligatureGlyphCount;
            continue;
          }
        }
      }

      *glyphOutPtr++ = glyphId;
      *infoOutPtr++ = *infoInPtr;

      glyphInPtr++;
      infoInPtr++;
    }
  }

  while (glyphInPtr != glyphInEnd) {
    *glyphOutPtr++ = *glyphInPtr++;
    *infoOutPtr++ = *infoInPtr++;
  }

  ctx.truncate(size_t(glyphOutPtr - ctx._workBuffer.glyphData));
  return BL_SUCCESS;
}

// bl::OpenType::LayoutImpl - GSUB - Nested Lookups
// ================================================

static void applyGSubNestedLookup(GSubContext& ctx) noexcept {
  // TODO: [OpenType] GSUB nested lookups
}

// bl::OpenType::LayoutImpl - GSUB - Lookup Type #5 - Context Substitution Validation
// ==================================================================================

static BL_INLINE bool validateGSubLookupType5Format1(ValidationContext& validator, Table<GSubTable::SequenceContext1> table) noexcept {
  return validateContextFormat1(validator, table, "ContextSubst1");
}

static BL_INLINE bool validateGSubLookupType5Format2(ValidationContext& validator, Table<GSubTable::SequenceContext2> table) noexcept {
  return validateContextFormat2(validator, table, "ContextSubst2");
}

static BL_INLINE bool validateGSubLookupType5Format3(ValidationContext& validator, Table<GSubTable::SequenceContext3> table) noexcept {
  return validateContextFormat3(validator, table, "ContextSubst3");
}

// bl::OpenType::LayoutImpl - GSUB - Lookup Type #5 - Context Substitution Lookup
// ==============================================================================

template<uint32_t kCovFmt>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGSubLookupType5Format1(GSubContext& ctx, Table<GSubTable::SequenceContext1> table, ApplyRange scope, LookupFlags flags, const CoverageTableIterator& covIt) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  uint32_t ruleSetCount = table->ruleSetOffsets.count();
  BL_ASSERT_VALIDATED(table.fits(GSubTable::SequenceContext1::kBaseSize + ruleSetCount * 2u));

  BLGlyphId* glyphInPtr = ctx.glyphData() + scope.index();
  BLGlyphId* glyphInEnd = ctx.glyphData() + scope.end();
  GlyphRange glyphRange = covIt.glyphRange<kCovFmt>();

  while (glyphInPtr != glyphInEnd) {
    SequenceMatch match;
    if (matchSequenceFormat1<kCovFmt>(table, ruleSetCount, glyphRange, covIt, glyphInPtr, size_t(glyphInEnd - glyphInPtr), &match)) {
      // TODO: [OpenType] Context MATCH
    }

    glyphInPtr++;
  }

  return BL_SUCCESS;
}

template<uint32_t kCovFmt, uint32_t kCDFmt>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGSubLookupType5Format2(GSubContext& ctx, Table<GSubTable::SequenceContext2> table, ApplyRange scope, LookupFlags flags, const CoverageTableIterator& covIt, const ClassDefTableIterator& cdIt) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  uint32_t ruleSetCount = table->ruleSetOffsets.count();
  BL_ASSERT_VALIDATED(table.fits(GSubTable::SequenceContext2::kBaseSize + ruleSetCount * 2u));

  BLGlyphId* glyphInPtr = ctx.glyphData() + scope.index();
  BLGlyphId* glyphInEnd = ctx.glyphData() + scope.end();
  GlyphRange glyphRange = covIt.glyphRange<kCovFmt>();

  while (glyphInPtr != glyphInEnd) {
    SequenceMatch match;
    if (matchSequenceFormat2<kCovFmt, kCDFmt>(table, ruleSetCount, glyphRange, covIt, cdIt, glyphInPtr, size_t(glyphInEnd - glyphInPtr), &match)) {
      // TODO: [OpenType] Context MATCH
    }

    glyphInEnd++;
  }

  return BL_SUCCESS;
}

static BL_INLINE_IF_NOT_DEBUG BLResult applyGSubLookupType5Format3(GSubContext& ctx, Table<GSubTable::SequenceContext3> table, ApplyRange scope, LookupFlags flags) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  uint32_t glyphCount = table->glyphCount();
  if (glyphCount < scope.size())
    return BL_SUCCESS;

  uint32_t lookupRecordCount = table->lookupRecordCount();
  const UInt16* coverageOffsetArray = table->coverageOffsetArray();

  BL_ASSERT_VALIDATED(glyphCount > 0);
  BL_ASSERT_VALIDATED(lookupRecordCount > 0);
  BL_ASSERT_VALIDATED(table.fits(GSubTable::SequenceContext3::kBaseSize + glyphCount * 2u + lookupRecordCount * GPosTable::SequenceLookupRecord::kBaseSize));

  CoverageTableIterator cov0It;
  uint32_t cov0Fmt = cov0It.init(table.subTableUnchecked(coverageOffsetArray[0].value()));
  GlyphRange glyphRange = cov0It.glyphRangeWithFormat(cov0Fmt);
  const GSubTable::SequenceLookupRecord* lookupRecordArray = table->lookupRecordArray(glyphCount);

  BLGlyphId* glyphInPtr = ctx.glyphData() + scope.index();
  BLGlyphId* glyphInEnd = ctx.glyphData() + scope.end();
  BLGlyphId* glyphInEndMinusN = glyphInEnd - glyphCount;

  do {
    if (matchSequenceFormat3(table, coverageOffsetArray, glyphRange, cov0It, cov0Fmt, glyphInPtr, glyphCount)) {
      // TODO: [OpenType] Context MATCH
      blUnused(lookupRecordArray, lookupRecordCount);
    }
    glyphInPtr++;
  } while (glyphInPtr != glyphInEndMinusN);

  return BL_SUCCESS;
}

// bl::OpenType::LayoutImpl - GSUB - Lookup Type #6 - Chained Context Substitution Validation
// ==========================================================================================

static BL_INLINE bool validateGSubLookupType6Format1(ValidationContext& validator, Table<GSubTable::ChainedSequenceContext1> table) noexcept {
  return validateChainedContextFormat1(validator, table, "ChainedContextSubst1");
}

static BL_INLINE bool validateGSubLookupType6Format2(ValidationContext& validator, Table<GSubTable::ChainedSequenceContext2> table) noexcept {
  return validateChainedContextFormat2(validator, table, "ChainedContextSubst2");
}

static BL_INLINE bool validateGSubLookupType6Format3(ValidationContext& validator, Table<GSubTable::ChainedSequenceContext3> table) noexcept {
  return validateChainedContextFormat3(validator, table, "ChainedContextSubst3");
}

// bl::OpenType::LayoutImpl - GSUB - Lookup Type #6 - Chained Context Substitution Lookup
// ======================================================================================

template<uint32_t kCovFmt>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGSubLookupType6Format1(GSubContext& ctx, Table<GSubTable::ChainedSequenceContext1> table, ApplyRange scope, LookupFlags flags, const CoverageTableIterator& covIt) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT(scope.index() < scope.end());
  BL_ASSERT_VALIDATED(table.fits());

  uint32_t ruleSetCount = table->ruleSetOffsets.count();
  BL_ASSERT_VALIDATED(table.fits(GSubTable::ChainedSequenceContext1::kBaseSize + ruleSetCount * 2u));

  ChainedMatchContext mCtx;
  mCtx.table = table;
  mCtx.firstGlyphRange = covIt.glyphRange<kCovFmt>();
  mCtx.backGlyphPtr = ctx.glyphData();
  mCtx.aheadGlyphPtr = ctx.glyphData() + scope.index();
  mCtx.backGlyphCount = scope.index();
  mCtx.aheadGlyphCount = scope.size();

  const Offset16* ruleSetOffsets = table->ruleSetOffsets.array();
  do {
    SequenceMatch match;
    if (matchChainedSequenceFormat1<kCovFmt>(mCtx, ruleSetOffsets, ruleSetCount, covIt, &match)) {
      // TODO: [OpenType] Context MATCH
    }
    mCtx.aheadGlyphPtr++;
    mCtx.backGlyphCount++;
  } while (--mCtx.aheadGlyphCount != 0);

  return BL_SUCCESS;
}

template<uint32_t kCovFmt, uint32_t kCD1Fmt, uint32_t kCD2Fmt, uint32_t kCD3Fmt>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGSubLookupType6Format2(
  GSubContext& ctx,
  Table<GSubTable::ChainedSequenceContext2> table,
  ApplyRange scope,
  LookupFlags flags,
  const CoverageTableIterator& covIt, const ClassDefTableIterator& cd1It, const ClassDefTableIterator& cd2It, const ClassDefTableIterator& cd3It) noexcept {

  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT(scope.index() < scope.end());
  BL_ASSERT_VALIDATED(table.fits());

  uint32_t ruleSetCount = table->ruleSetOffsets.count();
  BL_ASSERT_VALIDATED(table.fits(GSubTable::ChainedSequenceContext2::kBaseSize + ruleSetCount * 2u));

  ChainedMatchContext mCtx;
  mCtx.table = table;
  mCtx.firstGlyphRange = covIt.glyphRange<kCovFmt>();
  mCtx.backGlyphPtr = ctx.glyphData();
  mCtx.aheadGlyphPtr = ctx.glyphData() + scope.index();
  mCtx.backGlyphCount = scope.index();
  mCtx.aheadGlyphCount = scope.size();

  const Offset16* ruleSetOffsets = table->ruleSetOffsets.array();
  do {
    SequenceMatch match;
    if (matchChainedSequenceFormat2<kCovFmt, kCD1Fmt, kCD2Fmt, kCD3Fmt>(mCtx, ruleSetOffsets, ruleSetCount, covIt, cd1It, cd2It, cd3It, &match)) {
      // TODO: [OpenType] Context MATCH
    }
    mCtx.aheadGlyphPtr++;
    mCtx.backGlyphCount++;
  } while (--mCtx.aheadGlyphCount != 0);

  return BL_SUCCESS;
}

static BL_INLINE_IF_NOT_DEBUG BLResult applyGSubLookupType6Format3(GSubContext& ctx, Table<GSubTable::ChainedSequenceContext3> table, ApplyRange scope, LookupFlags flags) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  uint32_t backtrackGlyphCount = table->backtrackGlyphCount();
  uint32_t inputOffset = 4u + backtrackGlyphCount * 2u;
  BL_ASSERT_VALIDATED(table.fits(inputOffset + 2u));

  uint32_t inputGlyphCount = table.readU16(inputOffset);
  uint32_t lookaheadOffset = inputOffset + 2u + inputGlyphCount * 2u;
  BL_ASSERT_VALIDATED(inputGlyphCount > 0);
  BL_ASSERT_VALIDATED(table.fits(lookaheadOffset + 2u));

  uint32_t lookaheadGlyphCount = table.readU16(lookaheadOffset);
  uint32_t lookupOffset = lookaheadOffset + 2u + lookaheadGlyphCount * 2u;
  BL_ASSERT_VALIDATED(table.fits(lookupOffset + 2u));

  uint32_t lookupRecordCount = table.readU16(lookupOffset);
  BL_ASSERT_VALIDATED(lookupRecordCount > 0);
  BL_ASSERT_VALIDATED(table.fits(lookupOffset + 2u + lookupRecordCount * GSubGPosTable::SequenceLookupRecord::kBaseSize));

  // Restrict scope in a way so we would never underflow/overflow glyph buffer when matching backtrack/lookahead glyphs.
  uint32_t inputAndLookaheadGlyphCount = inputGlyphCount + lookaheadGlyphCount;
  scope.intersect(backtrackGlyphCount, ctx.size() - inputAndLookaheadGlyphCount);

  // Bail if the buffer or the scope is too small for this chained context substitution.
  if (scope.size() < inputAndLookaheadGlyphCount || scope.index() >= scope.end())
    return BL_SUCCESS;

  const Offset16* backtrackCoverageOffsets = table->backtrackCoverageOffsets();
  const Offset16* inputCoverageOffsets = table.dataAs<Offset16>(inputOffset + 2u);
  const Offset16* lookaheadCoverageOffsets = table.dataAs<Offset16>(lookaheadOffset + 2u);

  CoverageTableIterator cov0It;
  uint32_t cov0Fmt = cov0It.init(table.subTableUnchecked(inputCoverageOffsets[0].value()));
  GlyphRange firstGlyphRange = cov0It.glyphRangeWithFormat(cov0Fmt);

  ChainedMatchContext mCtx;
  mCtx.table = table;
  mCtx.firstGlyphRange = cov0It.glyphRangeWithFormat(cov0Fmt);
  mCtx.backGlyphPtr = ctx.glyphData();
  mCtx.aheadGlyphPtr = ctx.glyphData() + scope.index();
  mCtx.backGlyphCount = scope.index();
  mCtx.aheadGlyphCount = scope.size();

  do {
    if (matchChainedSequenceFormat3(mCtx,
        backtrackCoverageOffsets, backtrackGlyphCount,
        inputCoverageOffsets, inputGlyphCount,
        lookaheadCoverageOffsets, lookaheadGlyphCount,
        firstGlyphRange,
        cov0It, cov0Fmt)) {
      const GSubTable::SequenceLookupRecord* lookupRecordArray = table.dataAs<GSubTable::SequenceLookupRecord>(lookupOffset + 2u);
      // TODO: [OpenType] Context MATCH
      blUnused(lookupRecordArray, lookupRecordCount);
    }
    mCtx.backGlyphCount++;
    mCtx.aheadGlyphPtr++;
  } while (--mCtx.aheadGlyphCount >= inputAndLookaheadGlyphCount);

  return BL_SUCCESS;
}

// bl::OpenType::LayoutImpl - GSUB - Lookup Type #8 - Reverse Chained Context Validation
// =====================================================================================

static BL_INLINE_IF_NOT_DEBUG bool validateGSubLookupType8Format1(ValidationContext& validator, Table<GSubTable::ReverseChainedSingleSubst1> table) noexcept {
  const char* tableName = "ReverseChainedSingleSubst1";

  if (!table.fits())
    return validator.invalidTableSize(tableName, table.size, GSubTable::ReverseChainedSingleSubst1::kBaseSize);

  uint32_t backtrackGlyphCount = table->backtrackGlyphCount();
  uint32_t lookaheadOffset = 6u + backtrackGlyphCount * 2u;

  if (!table.fits(lookaheadOffset + 2u))
    return validator.invalidTableSize(tableName, table.size, lookaheadOffset + 2u);

  uint32_t lookaheadGlyphCount = table.readU16(lookaheadOffset);
  uint32_t substOffset = lookaheadOffset + 2u + lookaheadGlyphCount * 2u;

  if (!table.fits(substOffset + 2u))
    return validator.invalidTableSize(tableName, table.size, substOffset + 2u);

  uint32_t substGlyphCount = table.readU16(substOffset);
  uint32_t headerSize = substOffset + 2u + substGlyphCount * 2u;

  if (!table.fits(headerSize))
    return validator.invalidTableSize(tableName, table.size, headerSize);

  OffsetRange subTableOffsetRange{headerSize, table.size};
  uint32_t coverageOffset = table->coverageOffset();

  if (!subTableOffsetRange.contains(coverageOffset))
    return validator.invalidFieldOffset(tableName, "coverageTable", coverageOffset, subTableOffsetRange);

  uint32_t coverageCount;
  if (!validateCoverageTable(validator, table.subTable(coverageOffset), coverageCount))
    return false;

  if (coverageCount != substGlyphCount)
    return validator.fail("%s must have coverageCount (%u) equal to substGlyphCount (%u)", tableName, coverageCount, substGlyphCount);

  if (!validateCoverageTables(validator, table, tableName, "backtrackCoverages", table->backtrackCoverageOffsets(), backtrackGlyphCount, subTableOffsetRange))
    return false;

  if (!validateCoverageTables(validator, table, tableName, "lookaheadCoverages", table.dataAs<Offset16>(lookaheadOffset + 2u), lookaheadGlyphCount, subTableOffsetRange))
    return false;

  return true;
}

// bl::OpenType::LayoutImpl - GSUB - Lookup Type #8 - Reverse Chained Context Lookup
// =================================================================================

static BL_INLINE_IF_NOT_DEBUG BLResult applyGSubLookupType8Format1(GSubContext& ctx, Table<GSubTable::ReverseChainedSingleSubst1> table, ApplyRange scope, LookupFlags flags) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  uint32_t backtrackGlyphCount = table->backtrackGlyphCount();
  uint32_t lookaheadOffset = 6u + backtrackGlyphCount * 2u;
  BL_ASSERT_VALIDATED(table.fits(lookaheadOffset + 2u));

  uint32_t lookaheadGlyphCount = table.readU16(lookaheadOffset);
  uint32_t substOffset = lookaheadOffset + 2u + lookaheadGlyphCount * 2u;
  BL_ASSERT_VALIDATED(table.fits(substOffset + 2u));

  // Restrict scope in a way so we would never underflow/overflow glyph buffer when matching backtrack/lookahead glyphs.
  scope.intersect(backtrackGlyphCount, ctx.size() - lookaheadGlyphCount - 1u);

  // Bail if the buffer or the scope is too small for this chained context substitution.
  if (ctx.size() < lookaheadGlyphCount || scope.index() >= scope.end())
    return BL_SUCCESS;

  uint32_t substGlyphCount = table.readU16(substOffset);
  BL_ASSERT_VALIDATED(table.fits(substOffset + 2u + substGlyphCount * 2u));

  const Offset16* backtrackCoverageOffsets = table->backtrackCoverageOffsets();
  const Offset16* lookaheadCoverageOffsets = table.dataAs<Offset16>(lookaheadOffset + 2u);
  const UInt16* substGlyphIds = table.dataAs<UInt16>(substOffset + 2u);

  CoverageTableIterator covIt;
  uint32_t covFmt = covIt.init(table.subTableUnchecked(table->coverageOffset()));
  GlyphRange glyphRange = covIt.glyphRangeWithFormat(covFmt);

  BLGlyphId* glyphData = ctx.glyphData();
  size_t i = scope.end();
  size_t scopeBegin = scope.index();

  do {
    BLGlyphId glyphId = glyphData[--i];
    uint32_t coverageIndex;

    if (!glyphRange.contains(glyphId) || !covIt.findWithFormat(covFmt, glyphId, coverageIndex) || coverageIndex >= substGlyphCount)
      continue;

    if (!matchBackGlyphsFormat3(table, glyphData + i - 1u, backtrackCoverageOffsets, backtrackGlyphCount))
      continue;

    if (!matchAheadGlyphsFormat3(table, glyphData + i + 1u, lookaheadCoverageOffsets, lookaheadGlyphCount))
      continue;

    glyphData[i] = substGlyphIds[coverageIndex].value();
  } while (i != scopeBegin);

  return BL_SUCCESS;
}

// bl::OpenType::LayoutImpl - GSUB - Dispatch
// ==========================================

static BL_NOINLINE bool validateGSubLookup(ValidationContext& validator, RawTable table, GSubLookupAndFormat typeAndFormat) noexcept {
  switch (typeAndFormat) {
    case GSubLookupAndFormat::kType1Format1: return validateGSubLookupType1Format1(validator, table);
    case GSubLookupAndFormat::kType1Format2: return validateGSubLookupType1Format2(validator, table);
    case GSubLookupAndFormat::kType2Format1: return validateGSubLookupType2Format1(validator, table);
    case GSubLookupAndFormat::kType3Format1: return validateGSubLookupType3Format1(validator, table);
    case GSubLookupAndFormat::kType4Format1: return validateGSubLookupType4Format1(validator, table);
    case GSubLookupAndFormat::kType5Format1: return validateGSubLookupType5Format1(validator, table);
    case GSubLookupAndFormat::kType5Format2: return validateGSubLookupType5Format2(validator, table);
    case GSubLookupAndFormat::kType5Format3: return validateGSubLookupType5Format3(validator, table);
    case GSubLookupAndFormat::kType6Format1: return validateGSubLookupType6Format1(validator, table);
    case GSubLookupAndFormat::kType6Format2: return validateGSubLookupType6Format2(validator, table);
    case GSubLookupAndFormat::kType6Format3: return validateGSubLookupType6Format3(validator, table);
    case GSubLookupAndFormat::kType8Format1: return validateGSubLookupType8Format1(validator, table);
    default:
      return validator.fail("Unknown lookup type+format (%u)", uint32_t(typeAndFormat));
  }
}

static BL_INLINE_IF_NOT_DEBUG BLResult applyGSubLookup(GSubContext& ctx, RawTable table, GSubLookupAndFormat typeAndFormat, ApplyRange scope, LookupFlags flags) noexcept {
  BL_ASSERT_VALIDATED(table.fits(gsubLookupInfoTable.lookupInfo[size_t(typeAndFormat)].headerSize));

  #define BL_APPLY_WITH_COVERAGE(FN, TABLE)                                        \
    CoverageTableIterator covIt;                                                   \
    if (covIt.init(table.subTable(table.dataAs<TABLE>()->coverageOffset())) == 1u) \
      result = FN<1>(ctx, table, scope, flags, covIt);                             \
    else                                                                           \
      result = FN<2>(ctx, table, scope, flags, covIt);

  BLResult result = BL_SUCCESS;

  switch (typeAndFormat) {
    case GSubLookupAndFormat::kType1Format1: {
      BL_APPLY_WITH_COVERAGE(applyGSubLookupType1Format1, GSubTable::SingleSubst1)
      break;
    }

    case GSubLookupAndFormat::kType1Format2: {
      BL_APPLY_WITH_COVERAGE(applyGSubLookupType1Format2, GSubTable::MultipleSubst1)
      break;
    }

    case GSubLookupAndFormat::kType2Format1: {
      BL_APPLY_WITH_COVERAGE(applyGSubLookupType2Format1, GSubTable::MultipleSubst1)
      break;
    }

    case GSubLookupAndFormat::kType3Format1: {
      BL_APPLY_WITH_COVERAGE(applyGSubLookupType3Format1, GSubTable::AlternateSubst1)
      break;
    }

    case GSubLookupAndFormat::kType4Format1: {
      BL_APPLY_WITH_COVERAGE(applyGSubLookupType4Format1, GSubTable::LigatureSubst1)
      break;
    }

    case GSubLookupAndFormat::kType5Format1: {
      BL_APPLY_WITH_COVERAGE(applyGSubLookupType5Format1, GSubTable::SequenceContext1)
      break;
    }

    case GSubLookupAndFormat::kType5Format2: {
      CoverageTableIterator covIt;
      ClassDefTableIterator cdIt;

      FormatBits2X fmtBits = FormatBits2X(
        ((covIt.init(table.subTable(table.dataAs<GSubTable::SequenceContext2>()->coverageOffset())) - 1u) << 1) |
        ((cdIt.init(table.subTable(table.dataAs<GSubTable::SequenceContext2>()->classDefOffset()))  - 1u) << 0));

      switch (fmtBits) {
        case FormatBits2X::k11: return applyGSubLookupType5Format2<1, 1>(ctx, table, scope, flags, covIt, cdIt);
        case FormatBits2X::k12: return applyGSubLookupType5Format2<1, 2>(ctx, table, scope, flags, covIt, cdIt);
        case FormatBits2X::k21: return applyGSubLookupType5Format2<2, 1>(ctx, table, scope, flags, covIt, cdIt);
        case FormatBits2X::k22: return applyGSubLookupType5Format2<2, 2>(ctx, table, scope, flags, covIt, cdIt);
      }
      break;
    }

    case GSubLookupAndFormat::kType5Format3: {
      result = applyGSubLookupType5Format3(ctx, table, scope, flags);
      break;
    }

    case GSubLookupAndFormat::kType6Format1: {
      BL_APPLY_WITH_COVERAGE(applyGSubLookupType6Format1, GSubTable::ChainedSequenceContext1)
      break;
    }

    case GSubLookupAndFormat::kType6Format2: {
      CoverageTableIterator covIt;
      ClassDefTableIterator cd1It;
      ClassDefTableIterator cd2It;
      ClassDefTableIterator cd3It;

      FormatBits4X fmtBits = FormatBits4X(
        ((covIt.init(table.subTable(table.dataAs<GPosTable::ChainedSequenceContext2>()->coverageOffset()         )) - 1u) << 3) |
        ((cd1It.init(table.subTable(table.dataAs<GPosTable::ChainedSequenceContext2>()->backtrackClassDefOffset())) - 1u) << 2) |
        ((cd2It.init(table.subTable(table.dataAs<GPosTable::ChainedSequenceContext2>()->inputClassDefOffset()    )) - 1u) << 1) |
        ((cd3It.init(table.subTable(table.dataAs<GPosTable::ChainedSequenceContext2>()->lookaheadClassDefOffset())) - 1u) << 0));

      switch (fmtBits) {
        case FormatBits4X::k1111: result = applyGSubLookupType6Format2<1, 1, 1, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k1112: result = applyGSubLookupType6Format2<1, 1, 1, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k1121: result = applyGSubLookupType6Format2<1, 1, 2, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k1122: result = applyGSubLookupType6Format2<1, 1, 2, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k1211: result = applyGSubLookupType6Format2<1, 2, 1, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k1212: result = applyGSubLookupType6Format2<1, 2, 1, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k1221: result = applyGSubLookupType6Format2<1, 2, 2, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k1222: result = applyGSubLookupType6Format2<1, 2, 2, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2111: result = applyGSubLookupType6Format2<2, 1, 1, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2112: result = applyGSubLookupType6Format2<2, 1, 1, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2121: result = applyGSubLookupType6Format2<2, 1, 2, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2122: result = applyGSubLookupType6Format2<2, 1, 2, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2211: result = applyGSubLookupType6Format2<2, 2, 1, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2212: result = applyGSubLookupType6Format2<2, 2, 1, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2221: result = applyGSubLookupType6Format2<2, 2, 2, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2222: result = applyGSubLookupType6Format2<2, 2, 2, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
      }
      break;
    }

    case GSubLookupAndFormat::kType6Format3: {
      result = applyGSubLookupType6Format3(ctx, table, scope, flags);
      break;
    }

    case GSubLookupAndFormat::kType8Format1: {
      result = applyGSubLookupType8Format1(ctx, table, scope, flags);
      break;
    }

    default:
      break;
  }

  #undef BL_APPLY_WITH_COVERAGE

  return result;
}

// bl::OpenType::LayoutImpl - GPOS - Utilities
// ===========================================

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
  return uint32_t(bitCountByteTable[valueFormat & 0xFFu]) * 2u;
}

template<typename T>
static BL_INLINE const uint8_t* binarySearchGlyphIdInVarStruct(const uint8_t* array, size_t itemSize, size_t arraySize, BLGlyphId glyphId, size_t offset = 0) noexcept {
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

// bl::OpenType::LayoutImpl - GPOS - Lookup Type #1 - Single Adjustment Validation
// ===============================================================================

static BL_INLINE_IF_NOT_DEBUG bool validateGPosLookupType1Format1(ValidationContext& validator, Table<GPosTable::SingleAdjustment1> table) noexcept {
  const char* tableName = "SingleAdjustment1";

  uint32_t coverageCount;
  if (!validateLookupWithCoverage(validator, table, tableName, GPosTable::SingleAdjustment1::kBaseSize, coverageCount))
    return false;

  uint32_t valueFormat = table->valueFormat();
  if (!valueFormat)
    return validator.invalidFieldValue(tableName, "valueFormat", valueFormat);

  uint32_t recordSize = sizeOfValueRecordByFormat(valueFormat);
  uint32_t headerSize = GPosTable::SingleAdjustment1::kBaseSize + recordSize;

  if (!table.fits(headerSize))
    return validator.invalidTableSize(tableName, table.size, headerSize);

  return true;
}

static BL_INLINE_IF_NOT_DEBUG bool validateGPosLookupType1Format2(ValidationContext& validator, Table<GPosTable::SingleAdjustment2> table) noexcept {
  const char* tableName = "SingleAdjustment2";

  uint32_t coverageCount;
  if (!validateLookupWithCoverage(validator, table, tableName, GPosTable::SingleAdjustment2::kBaseSize, coverageCount))
    return false;

  uint32_t valueFormat = table->valueFormat();
  if (!valueFormat)
    return validator.invalidFieldValue(tableName, "valueFormat", valueFormat);

  uint32_t valueCount = table->valueCount();
  if (!valueCount)
    return validator.invalidFieldValue(tableName, "valueCount", valueCount);

  uint32_t recordSize = sizeOfValueRecordByFormat(valueFormat);
  uint32_t headerSize = GPosTable::SingleAdjustment2::kBaseSize + recordSize * valueCount;

  if (!table.fits(headerSize))
    return validator.invalidTableSize(tableName, table.size, headerSize);

  return true;
}

// bl::OpenType::LayoutImpl - GPOS - Lookup Type #1 - Single Adjustment Lookup
// ===========================================================================

template<uint32_t kCovFmt, typename ApplyScope>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGPosLookupType1Format1(GPosContext& ctx, Table<GPosTable::SingleAdjustment1> table, ApplyScope scope, LookupFlags flags, const CoverageTableIterator& covIt) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  uint32_t valueFormat = table->valueFormat();
  BL_ASSERT_VALIDATED(valueFormat != 0);
  BL_ASSERT_VALIDATED(table.fits(GPosTable::SingleAdjustment1::kBaseSize + sizeOfValueRecordByFormat(valueFormat)));

  size_t i = scope.index();
  size_t end = scope.end();

  BLGlyphId* glyphData = ctx.glyphData();
  BLGlyphPlacement* placementData = ctx.placementData();
  GlyphRange glyphRange = covIt.glyphRange<kCovFmt>();

  do {
    BLGlyphId glyphId = glyphData[i];
    if (glyphRange.contains(glyphId)) {
      uint32_t unusedCoverageIndex;
      if (covIt.find<kCovFmt>(glyphId, unusedCoverageIndex)) {
        const Int16* p = reinterpret_cast<const Int16*>(table.data + GPosTable::SingleAdjustment1::kBaseSize);
        applyGPosValue(p, valueFormat, &placementData[i]);
      }
    }
  } while (++i < end);

  return BL_SUCCESS;
}

template<uint32_t kCovFmt, typename ApplyScope>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGPosLookupType1Format2(GPosContext& ctx, Table<GPosTable::SingleAdjustment2> table, ApplyScope scope, LookupFlags flags, const CoverageTableIterator& covIt) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  uint32_t valueFormat = table->valueFormat();
  uint32_t valueCount = table->valueCount();
  uint32_t recordSize = sizeOfValueRecordByFormat(valueFormat);

  BL_ASSERT_VALIDATED(valueFormat != 0);
  BL_ASSERT_VALIDATED(table.fits(GPosTable::SingleAdjustment2::kBaseSize + valueCount * recordSize));

  size_t i = scope.index();
  size_t end = scope.end();

  BLGlyphId* glyphData = ctx.glyphData();
  BLGlyphPlacement* placementData = ctx.placementData();
  GlyphRange glyphRange = covIt.glyphRange<kCovFmt>();

  do {
    BLGlyphId glyphId = glyphData[i];
    if (glyphRange.contains(glyphId)) {
      uint32_t coverageIndex;
      if (covIt.find<kCovFmt>(glyphId, coverageIndex) && coverageIndex < valueCount) {
        const Int16* p = reinterpret_cast<const Int16*>(table.data + GPosTable::SingleAdjustment2::kBaseSize + coverageIndex * recordSize);
        applyGPosValue(p, valueFormat, &placementData[i]);
      }
    }
  } while (++i < end);

  return BL_SUCCESS;
}

// bl::OpenType::LayoutImpl - GPOS - Lookup Type #2 - Pair Adjustment Validation
// =============================================================================

static BL_INLINE_IF_NOT_DEBUG bool validateGPosLookupType2Format1(ValidationContext& validator, Table<GPosTable::PairAdjustment1> table) noexcept {
  const char* tableName = "PairAdjustment1";

  uint32_t coverageCount;
  if (!validateLookupWithCoverage(validator, table, tableName, GPosTable::PairAdjustment1::kBaseSize, coverageCount))
    return false;

  uint32_t pairSetCount = table->pairSetOffsets.count();
  uint32_t valueRecordSize = 2u + sizeOfValueRecordByFormat(table->valueFormat1()) +
                                  sizeOfValueRecordByFormat(table->valueFormat2()) ;

  uint32_t headerSize = GPosTable::PairAdjustment1::kBaseSize + pairSetCount * 2u;
  if (!table.fits(headerSize))
    return validator.invalidTableSize(tableName, table.size, headerSize);

  const Offset16* offsetArray = table->pairSetOffsets.array();
  OffsetRange pairSetOffsetRange{headerSize, table.size - 2u};

  for (uint32_t i = 0; i < pairSetCount; i++) {
    uint32_t pairSetOffset = offsetArray[i].value();
    if (!pairSetOffsetRange.contains(pairSetOffset))
      return validator.invalidOffsetEntry(tableName, "pairSetOffset", i, pairSetOffset, pairSetOffsetRange);

    Table<GPosTable::PairSet> pairSet(table.subTable(pairSetOffset));

    uint32_t pairValueCount = pairSet->pairValueCount();
    uint32_t pairSetSize = pairValueCount * valueRecordSize;

    if (!pairSet.fits(pairSetSize))
      return validator.invalidTableSize("PairSet", pairSet.size, pairSetSize);
  }

  return true;
}

static BL_INLINE_IF_NOT_DEBUG bool validateGPosLookupType2Format2(ValidationContext& validator, Table<GPosTable::PairAdjustment2> table) noexcept {
  const char* tableName = "PairAdjustment2";

  uint32_t coverageCount;
  if (!validateLookupWithCoverage(validator, table, tableName, GPosTable::PairAdjustment2::kBaseSize, coverageCount))
    return false;

  uint32_t class1Count = table->class1Count();
  uint32_t class2Count = table->class2Count();
  uint32_t valueRecordCount = class1Count * class2Count;

  uint32_t value1Format = table->value1Format();
  uint32_t value2Format = table->value2Format();
  uint32_t valueRecordSize = sizeOfValueRecordByFormat(value1Format) + sizeOfValueRecordByFormat(value2Format);

  uint64_t calculatedTableSize = uint64_t(valueRecordCount) * uint64_t(valueRecordSize);
  if (calculatedTableSize > table.size - GPosTable::PairAdjustment2::kBaseSize)
    calculatedTableSize = 0xFFFFFFFFu;

  if (!table.fits(calculatedTableSize))
    return validator.invalidTableSize(tableName, table.size, uint32_t(calculatedTableSize));

  return true;
}

// bl::OpenType::LayoutImpl - GPOS - Lookup Type #2 - Pair Adjustment Lookup
// =========================================================================

template<uint32_t kCovFmt, typename ApplyScope>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGPosLookupType2Format1(GPosContext& ctx, Table<GPosTable::PairAdjustment1> table, ApplyScope scope, LookupFlags flags, const CoverageTableIterator& covIt) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  size_t i = scope.index();
  size_t end = scope.isRange() ? scope.end() : ctx.size();

  // We always want to access the current and next glyphs, so bail if there is no next glyph...
  if (scope.isRange()) {
    if (i >= --end)
      return BL_SUCCESS;
  }
  else {
    if (i + 1u > ctx.size())
      return BL_SUCCESS;
  }

  uint32_t valueFormat1 = table->valueFormat1();
  uint32_t valueFormat2 = table->valueFormat2();
  uint32_t pairSetOffsetsCount = table->pairSetOffsets.count();
  BL_ASSERT_VALIDATED(table.fits(GPosTable::PairAdjustment1::kBaseSize + pairSetOffsetsCount * 2u));

  uint32_t valueRecordSize = 2u + sizeOfValueRecordByFormat(valueFormat1) +
                                  sizeOfValueRecordByFormat(valueFormat2) ;

  BLGlyphId* glyphData = ctx.glyphData();
  BLGlyphPlacement* placementData = ctx.placementData();

  BLGlyphId leftGlyphId = glyphData[i];
  BLGlyphId rightGlyphId = 0;
  GlyphRange glyphRange = covIt.glyphRange<kCovFmt>();

  do {
    rightGlyphId = glyphData[i + 1];
    if (glyphRange.contains(leftGlyphId)) {
      uint32_t coverageIndex;
      if (covIt.find<kCovFmt>(leftGlyphId, coverageIndex) && coverageIndex < pairSetOffsetsCount) {
        uint32_t pairSetOffset = table->pairSetOffsets.array()[coverageIndex].value();
        BL_ASSERT_VALIDATED(pairSetOffset <= table.size - 2u);

        const GPosTable::PairSet* pairSet = PtrOps::offset<const GPosTable::PairSet>(table.data, pairSetOffset);
        uint32_t pairSetCount = pairSet->pairValueCount();
        BL_ASSERT_VALIDATED(pairSetCount * valueRecordSize <= table.size - pairSetOffset);

        const Int16* p = reinterpret_cast<const Int16*>(
          binarySearchGlyphIdInVarStruct<UInt16>(
            reinterpret_cast<const uint8_t*>(pairSet->pairValueRecords()), valueRecordSize, pairSetCount, rightGlyphId));

        if (p) {
          p++;
          if (valueFormat1) p = applyGPosValue(p, valueFormat1, &placementData[i + 0]);
          if (valueFormat2) p = applyGPosValue(p, valueFormat2, &placementData[i + 1]);
        }
      }
    }

    leftGlyphId = rightGlyphId;
  } while (scope.isRange() && ++i < end);

  return BL_SUCCESS;
}

template<uint32_t kCovFmt, uint32_t kCD1Fmt, uint32_t kCD2Fmt, typename ApplyScope>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGPosLookupType2Format2(GPosContext& ctx, Table<GPosTable::PairAdjustment2> table, ApplyScope scope, LookupFlags flags, const CoverageTableIterator& covIt, const ClassDefTableIterator& cd1It, ClassDefTableIterator& cd2It) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  size_t i = scope.index();
  size_t end = scope.isRange() ? scope.end() : ctx.size();

  // We always want to access the current and next glyphs, so bail if there is no next glyph...
  if (scope.isRange()) {
    if (i >= --end)
      return BL_SUCCESS;
  }
  else {
    if (i + 1u > ctx.size())
      return BL_SUCCESS;
  }

  uint32_t value1Format = table->value1Format();
  uint32_t value2Format = table->value2Format();
  uint32_t valueRecordSize = sizeOfValueRecordByFormat(value1Format) + sizeOfValueRecordByFormat(value2Format);

  uint32_t class1Count = table->class1Count();
  uint32_t class2Count = table->class2Count();
  uint32_t valueRecordCount = class1Count * class2Count;
  BL_ASSERT_VALIDATED(table.fits(GPosTable::PairAdjustment2::kBaseSize + uint64_t(valueRecordCount) * uint64_t(valueRecordSize)));

  const uint8_t* valueBasePtr = table.data + GPosTable::PairAdjustment2::kBaseSize;

  BLGlyphId* glyphData = ctx.glyphData();
  BLGlyphPlacement* placementData = ctx.placementData();

  BLGlyphId leftGlyphId = glyphData[i];
  BLGlyphId rightGlyphId = 0;
  GlyphRange glyphRange = covIt.glyphRange<kCovFmt>();

  do {
    rightGlyphId = glyphData[i + 1];
    if (glyphRange.contains(leftGlyphId)) {
      uint32_t coverageIndex;
      if (covIt.find<kCovFmt>(leftGlyphId, coverageIndex)) {
        uint32_t c1 = cd1It.classOfGlyph<kCD1Fmt>(leftGlyphId);
        uint32_t c2 = cd2It.classOfGlyph<kCD2Fmt>(rightGlyphId);
        uint32_t cIndex = c1 * class2Count + c2;

        if (cIndex < valueRecordCount) {
          const Int16* p = PtrOps::offset<const Int16>(valueBasePtr, cIndex * valueRecordSize);
          if (value1Format) p = applyGPosValue(p, value1Format, &placementData[i + 0]);
          if (value2Format) p = applyGPosValue(p, value2Format, &placementData[i + 1]);
        }
      }
    }

    leftGlyphId = rightGlyphId;
  } while (scope.isRange() && ++i < end);

  return BL_SUCCESS;
}

// bl::OpenType::LayoutImpl - GPOS - Lookup Type #3 - Cursive Attachment Validation
// ================================================================================

static BL_INLINE_IF_NOT_DEBUG bool validateGPosLookupType3Format1(ValidationContext& validator, Table<GPosTable::CursiveAttachment1> table) noexcept {
  const char* tableName = "CursiveAttachment1";

  uint32_t coverageCount;
  if (!validateLookupWithCoverage(validator, table, tableName, GPosTable::CursiveAttachment1::kBaseSize, coverageCount))
    return false;

  uint32_t entryExitCount = table->entryExits.count();
  uint32_t headerSize = GPosTable::CursiveAttachment1::kBaseSize + entryExitCount * GPosTable::EntryExit::kBaseSize;

  if (!table.fits(headerSize))
    return validator.invalidTableSize(tableName, table.size, headerSize);

  // TODO: [OpenType] GPOS Cursive attachment validation.
  return false;
}

// bl::OpenType::LayoutImpl - GPOS - Lookup Type #4 - MarkToBase Attachment Validation
// ===================================================================================

// TODO: [OpenType] GPOS MarkToBase attachment

// bl::OpenType::LayoutImpl - GPOS - Lookup Type #5 - MarkToLigature Attachment Validation
// =======================================================================================

// TODO: [OpenType] GPOS MarkToLigature attachment

// bl::OpenType::LayoutImpl - GPOS - Lookup Type #6 - MarkToMark Attachment Validation
// ===================================================================================

// TODO: [OpenType] GPOS MarkToMark attachment

// bl::OpenType::LayoutImpl - GPOS - Nested Lookups
// ================================================

static BL_NOINLINE BLResult applyGPosNestedLookups(GPosContext& ctx, size_t index, const SequenceMatch& match) noexcept {
  // TODO: [OpenType] GPOS nested lookups
  return BL_SUCCESS;
}

// bl::OpenType::LayoutImpl - GPOS - Lookup Type #7 - Contextual Positioning Validation
// ====================================================================================

static BL_INLINE bool validateGPosLookupType7Format1(ValidationContext& validator, Table<GSubTable::SequenceContext1> table) noexcept {
  return validateContextFormat1(validator, table, "ContextPositioning1");
}

static BL_INLINE bool validateGPosLookupType7Format2(ValidationContext& validator, Table<GSubTable::SequenceContext2> table) noexcept {
  return validateContextFormat2(validator, table, "ContextPositioning2");
}

static BL_INLINE bool validateGPosLookupType7Format3(ValidationContext& validator, Table<GSubTable::SequenceContext3> table) noexcept {
  return validateContextFormat3(validator, table, "ContextPositioning3");
}

// bl::OpenType::LayoutImpl - GPOS - Lookup Type #7 - Contextual Positioning Lookup
// ================================================================================

template<uint32_t kCovFmt>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGPosLookupType7Format1(GPosContext& ctx, Table<GPosTable::SequenceContext1> table, ApplyRange scope, LookupFlags flags, const CoverageTableIterator& covIt) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  uint32_t ruleSetCount = table->ruleSetOffsets.count();
  BL_ASSERT_VALIDATED(table.fits(GPosTable::SequenceContext1::kBaseSize + ruleSetCount * 2u));

  size_t index = scope.index();
  size_t end = scope.end();
  BL_ASSERT(index < end);

  const BLGlyphId* glyphPtr = ctx.glyphData();
  GlyphRange glyphRange = covIt.glyphRange<kCovFmt>();

  do {
    SequenceMatch match;
    if (matchSequenceFormat1<kCovFmt>(table, ruleSetCount, glyphRange, covIt, glyphPtr, end - index, &match)) {
      BL_PROPAGATE(applyGPosNestedLookups(ctx, index, match));
    }
  } while (++index != end);

  return BL_SUCCESS;
}

template<uint32_t kCovFmt, uint32_t kCDFmt>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGPosLookupType7Format2(GPosContext& ctx, Table<GPosTable::SequenceContext2> table, ApplyRange scope, LookupFlags flags, const CoverageTableIterator& covIt, const ClassDefTableIterator& cdIt) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  uint32_t ruleSetCount = table->ruleSetOffsets.count();
  BL_ASSERT_VALIDATED(table.fits(GPosTable::SequenceContext2::kBaseSize + ruleSetCount * 2u));

  size_t index = scope.index();
  size_t end = scope.end();
  BL_ASSERT(index < end);

  const BLGlyphId* glyphPtr = ctx.glyphData() + scope.index();
  GlyphRange glyphRange = covIt.glyphRange<kCovFmt>();

  do {
    SequenceMatch match;
    if (matchSequenceFormat2<kCovFmt, kCDFmt>(table, ruleSetCount, glyphRange, covIt, cdIt, glyphPtr, end - index, &match)) {
      BL_PROPAGATE(applyGPosNestedLookups(ctx, index, match));
    }
  } while (++index != end);

  return BL_SUCCESS;
}

static BL_INLINE_IF_NOT_DEBUG BLResult applyGPosLookupType7Format3(GPosContext& ctx, Table<GPosTable::SequenceContext3> table, ApplyRange scope, LookupFlags flags) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  uint32_t glyphCount = table->glyphCount();
  uint32_t lookupRecordCount = table->lookupRecordCount();

  if (scope.size() < glyphCount)
    return BL_SUCCESS;

  BL_ASSERT_VALIDATED(glyphCount > 0);
  BL_ASSERT_VALIDATED(lookupRecordCount > 0);
  BL_ASSERT_VALIDATED(table.fits(GPosTable::SequenceContext3::kBaseSize + glyphCount * 2u + lookupRecordCount * GPosTable::SequenceLookupRecord::kBaseSize));

  const UInt16* coverageOffsetArray = table->coverageOffsetArray();
  const GPosTable::SequenceLookupRecord* lookupRecordArray = table->lookupRecordArray(glyphCount);

  CoverageTableIterator cov0It;
  uint32_t cov0Fmt = cov0It.init(table.subTableUnchecked(coverageOffsetArray[0].value()));

  size_t index = scope.index();
  size_t end = scope.end();
  BL_ASSERT(index < end);

  const BLGlyphId* glyphPtr = ctx.glyphData() + scope.index();
  GlyphRange glyphRange = cov0It.glyphRangeWithFormat(cov0Fmt);
  SequenceMatch match{glyphCount, lookupRecordCount, lookupRecordArray};

  size_t endMinusGlyphCount = end - glyphCount;
  do {
    if (matchSequenceFormat3(table, coverageOffsetArray, glyphRange, cov0It, cov0Fmt, glyphPtr, glyphCount)) {
      BL_PROPAGATE(applyGPosNestedLookups(ctx, index, match));
    }
  } while (++index != endMinusGlyphCount);

  return BL_SUCCESS;
}

// bl::OpenType::LayoutImpl - GPOS - Lookup Type #8 - Chained Context Positioning Validation
// =========================================================================================

static BL_INLINE bool validateGPosLookupType8Format1(ValidationContext& validator, Table<GSubTable::ChainedSequenceContext1> table) noexcept {
  return validateChainedContextFormat1(validator, table, "ChainedContextPositioning1");
}

static BL_INLINE bool validateGPosLookupType8Format2(ValidationContext& validator, Table<GSubTable::ChainedSequenceContext2> table) noexcept {
  return validateChainedContextFormat2(validator, table, "ChainedContextPositioning2");
}

static BL_INLINE bool validateGPosLookupType8Format3(ValidationContext& validator, Table<GSubTable::ChainedSequenceContext3> table) noexcept {
  return validateChainedContextFormat3(validator, table, "ChainedContextPositioning3");
}

// bl::OpenType::LayoutImpl - GPOS - Lookup Type #8 - Chained Context Positioning Lookup
// =====================================================================================

template<uint32_t kCovFmt>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGPosLookupType8Format1(GPosContext& ctx, Table<GPosTable::ChainedSequenceContext1> table, ApplyRange scope, LookupFlags flags, const CoverageTableIterator& covIt) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT(scope.index() < scope.end());
  BL_ASSERT_VALIDATED(table.fits());

  uint32_t ruleSetCount = table->ruleSetOffsets.count();
  BL_ASSERT_VALIDATED(table.fits(GPosTable::ChainedSequenceContext1::kBaseSize + ruleSetCount * 2u));

  ChainedMatchContext mCtx;
  mCtx.table = table;
  mCtx.firstGlyphRange = covIt.glyphRange<kCovFmt>();
  mCtx.backGlyphPtr = ctx.glyphData();
  mCtx.aheadGlyphPtr = ctx.glyphData() + scope.index();
  mCtx.backGlyphCount = scope.index();
  mCtx.aheadGlyphCount = scope.size();

  const Offset16* ruleSetOffsets = table->ruleSetOffsets.array();
  do {
    SequenceMatch match;
    if (matchChainedSequenceFormat1<kCovFmt>(mCtx, ruleSetOffsets, ruleSetCount, covIt, &match)) {
      BL_PROPAGATE(applyGPosNestedLookups(ctx, size_t(mCtx.aheadGlyphPtr - ctx.glyphData()), match));
    }
    mCtx.aheadGlyphPtr++;
    mCtx.backGlyphCount++;
  } while (--mCtx.aheadGlyphCount != 0);

  return BL_SUCCESS;
}

template<uint32_t kCovFmt, uint32_t kCD1Fmt, uint32_t kCD2Fmt, uint32_t kCD3Fmt>
static BL_INLINE_IF_NOT_DEBUG BLResult applyGPosLookupType8Format2(
  GPosContext& ctx,
  Table<GPosTable::ChainedSequenceContext2> table,
  ApplyRange scope,
  LookupFlags flags,
  const CoverageTableIterator& covIt, const ClassDefTableIterator& cd1It, const ClassDefTableIterator& cd2It, const ClassDefTableIterator& cd3It) noexcept {

  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT(scope.index() < scope.end());
  BL_ASSERT_VALIDATED(table.fits());

  uint32_t ruleSetCount = table->ruleSetOffsets.count();
  BL_ASSERT_VALIDATED(table.fits(GPosTable::ChainedSequenceContext2::kBaseSize + ruleSetCount * 2u));

  ChainedMatchContext mCtx;
  mCtx.table = table;
  mCtx.firstGlyphRange = covIt.glyphRange<kCovFmt>();
  mCtx.backGlyphPtr = ctx.glyphData();
  mCtx.aheadGlyphPtr = ctx.glyphData() + scope.index();
  mCtx.backGlyphCount = scope.index();
  mCtx.aheadGlyphCount = scope.size();

  const Offset16* ruleSetOffsets = table->ruleSetOffsets.array();
  do {
    SequenceMatch match;
    if (matchChainedSequenceFormat2<kCovFmt, kCD1Fmt, kCD2Fmt, kCD3Fmt>(mCtx, ruleSetOffsets, ruleSetCount, covIt, cd1It, cd2It, cd3It, &match)) {
      BL_PROPAGATE(applyGPosNestedLookups(ctx, size_t(mCtx.aheadGlyphPtr - ctx.glyphData()), match));
    }
    mCtx.aheadGlyphPtr++;
    mCtx.backGlyphCount++;
  } while (--mCtx.aheadGlyphCount != 0);

  return BL_SUCCESS;
}

static BL_INLINE_IF_NOT_DEBUG BLResult applyGPosLookupType8Format3(GPosContext& ctx, Table<GPosTable::ChainedSequenceContext3> table, ApplyRange scope, LookupFlags flags) noexcept {
  BL_ASSERT(scope.end() <= ctx.size());
  BL_ASSERT_VALIDATED(table.fits());

  uint32_t backtrackGlyphCount = table->backtrackGlyphCount();
  uint32_t inputOffset = 4u + backtrackGlyphCount * 2u;
  BL_ASSERT_VALIDATED(table.fits(inputOffset + 2u));

  uint32_t inputGlyphCount = table.readU16(inputOffset);
  uint32_t lookaheadOffset = inputOffset + 2u + inputGlyphCount * 2u;
  BL_ASSERT_VALIDATED(inputGlyphCount > 0);
  BL_ASSERT_VALIDATED(table.fits(lookaheadOffset + 2u));

  uint32_t lookaheadGlyphCount = table.readU16(lookaheadOffset);
  uint32_t lookupOffset = lookaheadOffset + 2u + lookaheadGlyphCount * 2u;
  BL_ASSERT_VALIDATED(table.fits(lookupOffset + 2u));

  uint32_t lookupRecordCount = table.readU16(lookupOffset);
  BL_ASSERT_VALIDATED(lookupRecordCount > 0);
  BL_ASSERT_VALIDATED(table.fits(lookupOffset + 2u + lookupRecordCount * GSubGPosTable::SequenceLookupRecord::kBaseSize));

  // Restrict scope in a way so we would never underflow/overflow glyph buffer when matching backtrack/lookahead glyphs.
  uint32_t inputAndLookaheadGlyphCount = inputGlyphCount + lookaheadGlyphCount;
  scope.intersect(backtrackGlyphCount, ctx.size() - inputAndLookaheadGlyphCount);

  // Bail if the buffer or the scope is too small for this chained context substitution.
  if (ctx.size() < inputAndLookaheadGlyphCount || scope.index() >= scope.end())
    return BL_SUCCESS;

  const Offset16* backtrackCoverageOffsets = table->backtrackCoverageOffsets();
  const Offset16* inputCoverageOffsets = table.dataAs<Offset16>(inputOffset + 2u);
  const Offset16* lookaheadCoverageOffsets = table.dataAs<Offset16>(lookaheadOffset + 2u);

  CoverageTableIterator cov0It;
  uint32_t cov0Fmt = cov0It.init(table.subTableUnchecked(inputCoverageOffsets[0].value()));
  GlyphRange firstGlyphRange = cov0It.glyphRangeWithFormat(cov0Fmt);

  ChainedMatchContext mCtx;
  mCtx.table = table;
  mCtx.firstGlyphRange = cov0It.glyphRangeWithFormat(cov0Fmt);
  mCtx.backGlyphPtr = ctx.glyphData();
  mCtx.aheadGlyphPtr = ctx.glyphData() + scope.index();
  mCtx.backGlyphCount = scope.index();
  mCtx.aheadGlyphCount = scope.size();

  SequenceMatch match;
  match.glyphCount = inputGlyphCount;
  match.lookupRecords = table.dataAs<GSubTable::SequenceLookupRecord>(lookupOffset + 2u);
  match.lookupRecordCount = lookupRecordCount;

  do {
    if (matchChainedSequenceFormat3(mCtx,
        backtrackCoverageOffsets, backtrackGlyphCount,
        inputCoverageOffsets, inputGlyphCount,
        lookaheadCoverageOffsets, lookaheadGlyphCount,
        firstGlyphRange,
        cov0It, cov0Fmt)) {
      BL_PROPAGATE(applyGPosNestedLookups(ctx, size_t(mCtx.aheadGlyphPtr - ctx.glyphData()), match));
    }
    mCtx.backGlyphCount++;
    mCtx.aheadGlyphPtr++;
  } while (--mCtx.aheadGlyphCount >= inputAndLookaheadGlyphCount);

  return BL_SUCCESS;
}

// bl::OpenType::LayoutImpl - GPOS - Lookup Dispatch
// =================================================

static BL_INLINE_IF_NOT_DEBUG bool validateGPosLookup(ValidationContext& validator, RawTable table, GPosLookupAndFormat typeAndFormat) noexcept {
  switch (typeAndFormat) {
    case GPosLookupAndFormat::kType1Format1: return validateGPosLookupType1Format1(validator, table);
    case GPosLookupAndFormat::kType1Format2: return validateGPosLookupType1Format2(validator, table);
    case GPosLookupAndFormat::kType2Format1: return validateGPosLookupType2Format1(validator, table);
    case GPosLookupAndFormat::kType2Format2: return validateGPosLookupType2Format2(validator, table);
    case GPosLookupAndFormat::kType3Format1: return validateGPosLookupType3Format1(validator, table);
    /*
    case GPosLookupAndFormat::kType4Format1: return validateGPosLookupType4Format1(validator, table);
    case GPosLookupAndFormat::kType5Format1: return validateGPosLookupType5Format1(validator, table);
    case GPosLookupAndFormat::kType6Format1: return validateGPosLookupType6Format1(validator, table);
    */
    case GPosLookupAndFormat::kType7Format1: return validateGPosLookupType7Format1(validator, table);
    case GPosLookupAndFormat::kType7Format2: return validateGPosLookupType7Format2(validator, table);
    case GPosLookupAndFormat::kType7Format3: return validateGPosLookupType7Format3(validator, table);
    case GPosLookupAndFormat::kType8Format1: return validateGPosLookupType8Format1(validator, table);
    case GPosLookupAndFormat::kType8Format2: return validateGPosLookupType8Format2(validator, table);
    case GPosLookupAndFormat::kType8Format3: return validateGPosLookupType8Format3(validator, table);
    default:
      return false;
  }
}

static BLResult applyGPosLookup(GPosContext& ctx, RawTable table, GPosLookupAndFormat typeAndFormat, ApplyRange scope, LookupFlags flags) noexcept {
  BL_ASSERT_VALIDATED(table.fits(gposLookupInfoTable.lookupInfo[size_t(typeAndFormat)].headerSize));

  #define BL_APPLY_WITH_COVERAGE(FN, TABLE)                                        \
    CoverageTableIterator covIt;                                                   \
    if (covIt.init(table.subTable(table.dataAs<TABLE>()->coverageOffset())) == 1u) \
      result = FN<1>(ctx, table, scope, flags, covIt);                             \
    else                                                                           \
      result = FN<2>(ctx, table, scope, flags, covIt);

  BLResult result = BL_SUCCESS;
  switch (typeAndFormat) {
    case GPosLookupAndFormat::kType1Format1: {
      BL_APPLY_WITH_COVERAGE(applyGPosLookupType1Format1, GPosTable::SingleAdjustment1)
      break;
    }

    case GPosLookupAndFormat::kType1Format2: {
      BL_APPLY_WITH_COVERAGE(applyGPosLookupType1Format2, GPosTable::SingleAdjustment2)
      break;
    }

    case GPosLookupAndFormat::kType2Format1: {
      BL_APPLY_WITH_COVERAGE(applyGPosLookupType2Format1, GPosTable::PairAdjustment1)
      break;
    }

    case GPosLookupAndFormat::kType2Format2: {
      CoverageTableIterator covIt;
      ClassDefTableIterator cd1It;
      ClassDefTableIterator cd2It;

      FormatBits3X fmtBits = FormatBits3X(
        ((covIt.init(table.subTable(table.dataAs<GPosTable::PairAdjustment2>()->coverageOffset()))  - 1u) << 2) |
        ((cd1It.init(table.subTable(table.dataAs<GPosTable::PairAdjustment2>()->classDef1Offset())) - 1u) << 1) |
        ((cd2It.init(table.subTable(table.dataAs<GPosTable::PairAdjustment2>()->classDef2Offset())) - 1u) << 0));

      switch (fmtBits) {
        case FormatBits3X::k111: result = applyGPosLookupType2Format2<1, 1, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It); break;
        case FormatBits3X::k112: result = applyGPosLookupType2Format2<1, 1, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It); break;
        case FormatBits3X::k121: result = applyGPosLookupType2Format2<1, 2, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It); break;
        case FormatBits3X::k122: result = applyGPosLookupType2Format2<1, 2, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It); break;
        case FormatBits3X::k211: result = applyGPosLookupType2Format2<2, 1, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It); break;
        case FormatBits3X::k212: result = applyGPosLookupType2Format2<2, 1, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It); break;
        case FormatBits3X::k221: result = applyGPosLookupType2Format2<2, 2, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It); break;
        case FormatBits3X::k222: result = applyGPosLookupType2Format2<2, 2, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It); break;
      }

      break;
    }

    case GPosLookupAndFormat::kType3Format1:
    case GPosLookupAndFormat::kType4Format1:
    case GPosLookupAndFormat::kType5Format1:
    case GPosLookupAndFormat::kType6Format1:
      // TODO: [OpenType] GPOS missing lookups.
      break;

    case GPosLookupAndFormat::kType7Format1: {
      BL_APPLY_WITH_COVERAGE(applyGPosLookupType7Format1, GPosTable::LookupHeaderWithCoverage)
      break;
    }

    case GPosLookupAndFormat::kType7Format2: {
      CoverageTableIterator covIt;
      ClassDefTableIterator cdIt;

      FormatBits2X fmtBits = FormatBits2X(
        ((covIt.init(table.subTable(table.dataAs<GPosTable::SequenceContext2>()->coverageOffset())) - 1u) << 1) |
        ((cdIt.init(table.subTable(table.dataAs<GPosTable::SequenceContext2>()->classDefOffset()))  - 1u) << 0));

      switch (fmtBits) {
        case FormatBits2X::k11: result = applyGPosLookupType7Format2<1, 1>(ctx, table, scope, flags, covIt, cdIt); break;
        case FormatBits2X::k12: result = applyGPosLookupType7Format2<1, 2>(ctx, table, scope, flags, covIt, cdIt); break;
        case FormatBits2X::k21: result = applyGPosLookupType7Format2<2, 1>(ctx, table, scope, flags, covIt, cdIt); break;
        case FormatBits2X::k22: result = applyGPosLookupType7Format2<2, 2>(ctx, table, scope, flags, covIt, cdIt); break;
      }

      break;
    }

    case GPosLookupAndFormat::kType7Format3: {
      result = applyGPosLookupType7Format3(ctx, table, scope, flags);
      break;
    }

    case GPosLookupAndFormat::kType8Format1: {
      BL_APPLY_WITH_COVERAGE(applyGPosLookupType8Format1, GPosTable::LookupHeaderWithCoverage)
      break;
    }

    case GPosLookupAndFormat::kType8Format2: {
      CoverageTableIterator covIt;
      ClassDefTableIterator cd1It;
      ClassDefTableIterator cd2It;
      ClassDefTableIterator cd3It;

      FormatBits4X fmtBits = FormatBits4X(
        ((covIt.init(table.subTable(table.dataAs<GPosTable::ChainedSequenceContext2>()->coverageOffset()         )) - 1u) << 3) |
        ((cd1It.init(table.subTable(table.dataAs<GPosTable::ChainedSequenceContext2>()->backtrackClassDefOffset())) - 1u) << 2) |
        ((cd2It.init(table.subTable(table.dataAs<GPosTable::ChainedSequenceContext2>()->inputClassDefOffset()    )) - 1u) << 1) |
        ((cd3It.init(table.subTable(table.dataAs<GPosTable::ChainedSequenceContext2>()->lookaheadClassDefOffset())) - 1u) << 0));

      switch (fmtBits) {
        case FormatBits4X::k1111: result = applyGPosLookupType8Format2<1, 1, 1, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k1112: result = applyGPosLookupType8Format2<1, 1, 1, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k1121: result = applyGPosLookupType8Format2<1, 1, 2, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k1122: result = applyGPosLookupType8Format2<1, 1, 2, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k1211: result = applyGPosLookupType8Format2<1, 2, 1, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k1212: result = applyGPosLookupType8Format2<1, 2, 1, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k1221: result = applyGPosLookupType8Format2<1, 2, 2, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k1222: result = applyGPosLookupType8Format2<1, 2, 2, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2111: result = applyGPosLookupType8Format2<2, 1, 1, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2112: result = applyGPosLookupType8Format2<2, 1, 1, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2121: result = applyGPosLookupType8Format2<2, 1, 2, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2122: result = applyGPosLookupType8Format2<2, 1, 2, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2211: result = applyGPosLookupType8Format2<2, 2, 1, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2212: result = applyGPosLookupType8Format2<2, 2, 1, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2221: result = applyGPosLookupType8Format2<2, 2, 2, 1>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
        case FormatBits4X::k2222: result = applyGPosLookupType8Format2<2, 2, 2, 2>(ctx, table, scope, flags, covIt, cd1It, cd2It, cd3It); break;
      }
      break;
    }

    case GPosLookupAndFormat::kType8Format3: {
      result = applyGPosLookupType8Format3(ctx, table, scope, flags);
      break;
    }

    default:
      break;
  }

  #undef BL_APPLY_WITH_COVERAGE

  return result;
}

// bl::OpenType::LayoutImpl - GSUB & GPOS - Validate
// =================================================

static bool validateLookup(ValidationContext& validator, Table<GSubGPosTable> table, uint32_t lookupIndex) noexcept {
  const char* tableName = "LookupList";
  const OTFaceImpl* faceI = validator.faceImpl();

  if (!table.fits())
    return validator.invalidTableSize(tableName, table.size, GSubGPosTable::kBaseSize);

  LookupKind lookupKind = validator.lookupKind();
  bool isGSub = lookupKind == LookupKind::kGSUB;

  const GSubGPosLookupInfo& lookupInfo = isGSub ? gsubLookupInfoTable : gposLookupInfoTable;
  const LayoutData::GSubGPos& layoutData = faceI->layout.kinds[size_t(lookupKind)];
  Table<Array16<UInt16>> lookupList(table.subTable(layoutData.lookupListOffset));

  uint32_t lookupCount = layoutData.lookupCount;
  if (lookupIndex >= lookupCount)
    return validator.fail("%s[%u] doesn't exist (lookupCount=%u)", tableName, lookupIndex, lookupCount);

  uint32_t lookupListSize = 2u + lookupCount * 2u;
  if (!lookupList.fits(lookupListSize))
    return validator.invalidTableSize(tableName, lookupList.size, lookupListSize);

  uint32_t lookupOffset = lookupList->array()[lookupIndex].value();
  OffsetRange lookupOffsetRange{lookupListSize, lookupList.size - GSubGPosTable::LookupTable::kBaseSize};

  if (!lookupOffsetRange.contains(lookupOffset))
    return validator.fail("%s[%u] has invalid offset (%u), valid range=[%u:%u]",
      tableName, lookupIndex, lookupOffset, lookupOffsetRange.start, lookupOffsetRange.end);

  Table<GSubGPosTable::LookupTable> lookupTable(lookupList.subTable(lookupOffset));
  uint32_t lookupType = lookupTable->lookupType();

  // Reject unknown lookup Type+Format combinations (lookupType 0 and values above lookupMaxValue are invalid).
  if (lookupType - 1u > uint32_t(lookupInfo.lookupMaxValue))
    return validator.fail("%s[%u] invalid lookup type (%u)", tableName, lookupIndex, lookupType);

  uint32_t subTableCount = lookupTable->subTableOffsets.count();
  const Offset16* subTableOffsets = lookupTable->subTableOffsets.array();

  uint32_t lookupTableSize = GSubGPosTable::LookupTable::kBaseSize + subTableCount * 2u;
  if (!lookupTable.fits(lookupTableSize))
    return validator.fail("%s[%u] truncated (size=%u, required=%u)", tableName, lookupIndex, lookupTable.size, lookupTableSize);

  uint32_t subTableMinSize = lookupType == lookupInfo.extensionType ? 8u : 6u;
  OffsetRange subTableOffsetRange{lookupTableSize, lookupTable.size - subTableMinSize};

  uint32_t extPreviousLookupType = 0u;

  for (uint32_t subTableIndex = 0; subTableIndex < subTableCount; subTableIndex++) {
    uint32_t subTableOffset = subTableOffsets[subTableIndex].value();
    if (!subTableOffsetRange.contains(subTableOffset))
      return validator.fail("%s[%u].subTable[%u] has invalid offset (%u), valid range=[%u:%u]",
        tableName, lookupIndex, subTableIndex, subTableOffset, subTableOffsetRange.start, subTableOffsetRange.end);

    Table<GSubGPosTable::LookupHeader> subTable(lookupTable.subTable(subTableOffset));
    uint32_t lookupFormat = subTable->format();

    uint32_t lookupTypeAndFormat = lookupInfo.typeInfo[lookupType].typeAndFormat + lookupFormat - 1u;
    if (lookupType == lookupInfo.extensionType) {
      Table<GSubGPosTable::ExtensionLookup> extSubTable(subTable);

      lookupType = extSubTable->lookupType();
      uint32_t extLookupFormat = extSubTable->format();
      uint32_t extSubTableOffset = extSubTable->offset();

      if (!extPreviousLookupType && lookupType != extPreviousLookupType)
        return validator.fail("%s[%u].subTable[%u] has a different type (%u) than a previous extension (%u)",
          tableName, lookupIndex, subTableIndex, lookupType, extPreviousLookupType);

      extPreviousLookupType = lookupType;
      bool validType = lookupType != lookupInfo.extensionType && lookupType - 1u < uint32_t(lookupInfo.lookupMaxValue);

      if (!validType || extLookupFormat != 1)
        return validator.fail("%s[%u].subTable[%u] has invalid extension type (%u) & format (%u) combination",
          tableName, lookupIndex, subTableIndex, lookupType, extLookupFormat);

      subTable = extSubTable.subTable(extSubTableOffset);
      if (!subTable.fits())
        return validator.fail("%s[%u].subTable[%u] of extension type points to a truncated table (size=%u required=%u)",
          tableName, lookupIndex, subTableIndex);

      lookupFormat = subTable->format();
      lookupTypeAndFormat = lookupInfo.typeInfo[lookupType].typeAndFormat + lookupFormat - 1u;
    }

    if (lookupFormat - 1u >= lookupInfo.typeInfo[lookupType].formatCount)
      return validator.fail("%s[%u].subTable[%u] has invalid type (%u) & format (%u) combination",
        tableName, lookupIndex, subTableIndex, lookupType, lookupFormat);

    bool valid = isGSub ? validateGSubLookup(validator, subTable, GSubLookupAndFormat(lookupTypeAndFormat))
                        : validateGPosLookup(validator, subTable, GPosLookupAndFormat(lookupTypeAndFormat));
    if (!valid)
      return false;
  }

  return true;
}

static BL_NOINLINE LayoutData::LookupStatusBits validateLookups(const OTFaceImpl* faceI, LookupKind lookupKind, uint32_t wordIndex, uint32_t lookupBits) noexcept {
  Table<GSubGPosTable> table(faceI->layout.tables[size_t(lookupKind)]);
  const LayoutData::GSubGPos& layoutData = faceI->layout.kinds[size_t(lookupKind)];

  uint32_t baseIndex = wordIndex * 32u;
  uint32_t lookupCount = layoutData.lookupCount;

  ValidationContext validator(faceI, lookupKind);
  uint32_t analyzedBits = lookupBits;
  uint32_t validBits = 0;

  BitSetOps::BitIterator it(analyzedBits);
  while (it.hasNext()) {
    uint32_t bitIndex = it.next();
    uint32_t lookupIndex = baseIndex + bitIndex;

    if (lookupIndex >= lookupCount)
      break;

    if (validateLookup(validator, table, lookupIndex))
      validBits |= BitSetOps::indexAsMask(bitIndex);
  }

  return faceI->layout.commitLookupStatusBits(lookupKind, wordIndex, LayoutData::LookupStatusBits::make(analyzedBits, validBits));
}

// bl::OpenType::LayoutImpl - Apply
// ================================

static BL_INLINE BLResult applyLookup(GSubContext& ctx, RawTable table, uint32_t typeAndFormat, ApplyRange scope, LookupFlags flags) noexcept {
  return applyGSubLookup(ctx, table, GSubLookupAndFormat(typeAndFormat), scope, flags);
}

static BL_INLINE BLResult applyLookup(GPosContext& ctx, RawTable table, uint32_t typeAndFormat, ApplyRange scope, LookupFlags flags) noexcept {
  return applyGPosLookup(ctx, table, GPosLookupAndFormat(typeAndFormat), scope, flags);
}

static void debugGlyphAndClustersToMessageSink(DebugSink debugSink, const BLGlyphId* glyphData, const BLGlyphInfo* infoData, size_t size) noexcept {
  BLString s;

  s.append('[');
  for (size_t i = 0; i < size; i++) {
    s.appendFormat("%u@%u", glyphData[i], infoData[i].cluster);
    if (i != size - 1)
      s.appendFormat(", ");
  }
  s.append(']');

  debugSink.message(s);
}

static void debugContextToMessageSink(GSubContext& ctx) noexcept {
  debugGlyphAndClustersToMessageSink(ctx._debugSink, ctx.glyphData(), ctx.infoData(), ctx.size());
}

static void debugContextToMessageSink(GPosContext& ctx) noexcept {
}

template<LookupKind kLookupKind, typename Context>
static BLResult BL_CDECL applyLookups(const BLFontFaceImpl* faceI_, BLGlyphBuffer* gb, const uint32_t* bitWords, size_t bitWordCount) noexcept {
  constexpr uint32_t kLookupExtension = kLookupKind == LookupKind::kGSUB ? uint32_t(GSubTable::kLookupExtension) : uint32_t(GPosTable::kLookupExtension);

  const OTFaceImpl* faceI = static_cast<const OTFaceImpl*>(faceI_);
  const GSubGPosLookupInfo& lookupInfo = kLookupKind == LookupKind::kGSUB ? gsubLookupInfoTable : gposLookupInfoTable;

  Context ctx;
  ctx.init(blGlyphBufferGetImpl(gb));

  if (ctx.empty())
    return BL_SUCCESS;

  RawTable table(faceI->layout.tables[size_t(kLookupKind)]);
  const LayoutData::GSubGPos& layoutData = faceI->layout.kinds[size_t(kLookupKind)];
  Table<Array16<UInt16>> lookupListTable(table.subTableUnchecked(layoutData.lookupListOffset));

  bool didProcessLookup = false;
  uint32_t wordCount = uint32_t(blMin<size_t>(bitWordCount, layoutData.lookupStatusDataSize));

  for (uint32_t wordIndex = 0; wordIndex < wordCount; wordIndex++) {
    uint32_t lookupBits = bitWords[wordIndex];
    if (!lookupBits)
      continue;

    // In order to process a GSUB/GPOS lookup we first validate them so the processing pipeline does not have
    // to perform validation every time it processes a lookup. Validation is lazy so only lookups that need to
    // be processed are validated.
    //
    // We have to first check whether the lookups represented by `bits` were already analyzed. If so, then we
    // can just flip bits describing lookups where validation failed and only process lookups that are valid.
    LayoutData::LookupStatusBits statusBits = faceI->layout.getLookupStatusBits(kLookupKind, wordIndex);
    uint32_t nonAnalyzedBits = lookupBits & ~statusBits.analyzed;

    if (BL_UNLIKELY(nonAnalyzedBits))
      statusBits = validateLookups(faceI, kLookupKind, wordIndex, nonAnalyzedBits);

    // Remove lookups from bits that are invalid, and thus won't be processed. Note that conforming fonts will
    // never have invalid lookups, but it's possible that a font is corrupted or malformed on purpose.
    lookupBits &= statusBits.valid;

    uint32_t bitOffset = wordIndex * 32u;
    BitSetOps::BitIterator it(lookupBits);

    while (it.hasNext()) {
      uint32_t lookupTableIndex = it.next() + bitOffset;
      BL_ASSERT_VALIDATED(lookupTableIndex < layoutData.lookupCount);

      uint32_t lookupTableOffset = lookupListTable->array()[lookupTableIndex].value();
      BL_ASSERT_VALIDATED(lookupTableOffset <= lookupListTable.size - 6u);

      Table<GSubGPosTable::LookupTable> lookupTable(lookupListTable.subTableUnchecked(lookupTableOffset));
      uint32_t lookupType = lookupTable->lookupType();
      uint32_t lookupFlags = lookupTable->lookupFlags();
      BL_ASSERT_VALIDATED(lookupType - 1u < uint32_t(lookupInfo.lookupMaxValue));

      uint32_t lookupEntryCount = lookupTable->subTableOffsets.count();
      const Offset16* lookupEntryOffsets = lookupTable->subTableOffsets.array();

      const GSubGPosLookupInfo::TypeInfo& lookupTypeInfo = lookupInfo.typeInfo[lookupType];
      uint32_t lookupTableMinSize = lookupType == kLookupExtension ? 8u : 6u;
      BL_ASSERT_VALIDATED(lookupTable.fits(lookupTableMinSize + lookupEntryCount * 2u));

      // Only used in Debug builds when `BL_ASSERT_VALIDATED` is enabled.
      blUnused(lookupTableMinSize);

      for (uint32_t j = 0; j < lookupEntryCount; j++) {
        uint32_t lookupOffset = lookupEntryOffsets[j].value();
        BL_ASSERT_VALIDATED(lookupOffset <= lookupTable.size - lookupTableMinSize);

        Table<GSubGPosTable::LookupHeader> lookupHeader(lookupTable.subTableUnchecked(lookupOffset));
        uint32_t lookupFormat = lookupHeader->format();
        BL_ASSERT_VALIDATED(lookupFormat - 1u < lookupTypeInfo.formatCount);

        uint32_t lookupTypeAndFormat = lookupTypeInfo.typeAndFormat + lookupFormat - 1u;
        if (lookupType == kLookupExtension) {
          Table<GSubGPosTable::ExtensionLookup> extensionTable(lookupTable.subTableUnchecked(lookupOffset));

          uint32_t extensionLookupType = extensionTable->lookupType();
          uint32_t extensionOffset = extensionTable->offset();

          BL_ASSERT_VALIDATED(extensionLookupType - 1u < lookupInfo.lookupMaxValue);
          BL_ASSERT_VALIDATED(extensionOffset <= extensionTable.size - 6u);

          lookupHeader = extensionTable.subTableUnchecked(extensionOffset);
          lookupFormat = lookupHeader->format();

          const GSubGPosLookupInfo::TypeInfo& extensionLookupTypeInfo = lookupInfo.typeInfo[extensionLookupType];
          BL_ASSERT_VALIDATED(lookupFormat - 1u < extensionLookupTypeInfo.formatCount);

          lookupTypeAndFormat = extensionLookupTypeInfo.typeAndFormat + lookupFormat - 1u;
        }

        if (ctx._debugSink.enabled()) {
          debugContextToMessageSink(ctx);
          BLString s;
          if (kLookupKind == LookupKind::kGSUB)
            s.assignFormat("Applying GSUB Lookup[%u].%s%u[%u]", lookupTableIndex, gsubLookupName(lookupType), lookupFormat, j);
          else
            s.assignFormat("Applying GPOS Lookup[%u].%s%u[%u]", lookupTableIndex, gposLookupName(lookupType), lookupFormat, j);

          ctx._debugSink.message(s);
          didProcessLookup = true;
        }

        BL_PROPAGATE(applyLookup(ctx, lookupHeader, lookupTypeAndFormat, ApplyRange{0, ctx.size()}, LookupFlags(lookupFlags)));

        if (ctx.empty())
          goto done;
      }
    }
  }

done:
  if (ctx._debugSink.enabled()) {
    if (didProcessLookup)
      debugContextToMessageSink(ctx);
  }

  ctx.done();

  return BL_SUCCESS;
}

// bl::OpenType::LayoutImpl - GSUB & GPOS - Init
// =============================================

static BLResult initGSubGPos(OTFaceImpl* faceI, Table<GSubGPosTable> table, LookupKind lookupKind) noexcept {
  if (BL_UNLIKELY(!table.fits()))
    return BL_SUCCESS;

  uint32_t version = table->v1_0()->version();
  if (BL_UNLIKELY(version < 0x00010000u || version > 0x00010001u))
    return BL_SUCCESS;

  uint32_t headerSize = GPosTable::HeaderV1_0::kBaseSize;
  if (version >= 0x00010001u)
    headerSize = GPosTable::HeaderV1_1::kBaseSize;

  if (BL_UNLIKELY(!table.fits(headerSize)))
    return BL_SUCCESS;

  uint32_t lookupListOffset = table->v1_0()->lookupListOffset();
  uint32_t featureListOffset = table->v1_0()->featureListOffset();
  uint32_t scriptListOffset = table->v1_0()->scriptListOffset();

  // Some fonts have these set to a table size to indicate that there are no lookups, fix this...
  if (lookupListOffset == table.size)
    lookupListOffset = 0;

  if (featureListOffset == table.size)
    featureListOffset = 0;

  if (scriptListOffset == table.size)
    scriptListOffset = 0;

  OffsetRange offsetRange{headerSize, table.size - 2u};

  // First validate core offsets - if a core offset is wrong we won't use GSUB/GPOS at all.
  if (lookupListOffset) {
    if (BL_UNLIKELY(!offsetRange.contains(lookupListOffset)))
      return BL_SUCCESS;
  }

  if (featureListOffset) {
    if (BL_UNLIKELY(!offsetRange.contains(featureListOffset)))
      return BL_SUCCESS;
  }

  if (scriptListOffset) {
    if (BL_UNLIKELY(!offsetRange.contains(scriptListOffset)))
      return BL_SUCCESS;
  }

  LayoutData::GSubGPos& d = faceI->layout.kinds[size_t(lookupKind)];

  if (lookupListOffset) {
    Table<Array16<Offset16>> lookupListOffsets(table.subTableUnchecked(lookupListOffset));
    uint32_t count = lookupListOffsets->count();

    if (count) {
      d.lookupListOffset = uint16_t(lookupListOffset);
      d.lookupCount = uint16_t(count);
    }
  }

  if (featureListOffset) {
    Table<Array16<TagRef16>> featureListOffsets(table.subTableUnchecked(featureListOffset));
    uint32_t count = featureListOffsets->count();

    if (count) {
      const TagRef16* array = featureListOffsets->array();
      for (uint32_t i = 0; i < count; i++) {
        BLTag featureTag = array[i].tag();
        BL_PROPAGATE(faceI->featureTagSet.addTag(featureTag));
      }

      d.featureCount = uint16_t(count);
      d.featureListOffset = uint16_t(featureListOffset);
    }
  }

  if (scriptListOffset) {
    Table<Array16<TagRef16>> scriptListOffsets(table.subTableUnchecked(scriptListOffset));
    uint32_t count = scriptListOffsets->count();

    if (count) {
      const TagRef16* array = scriptListOffsets->array();
      for (uint32_t i = 0; i < count; i++) {
        BLTag scriptTag = array[i].tag();
        BL_PROPAGATE(faceI->scriptTagSet.addTag(scriptTag));
      }

      d.scriptListOffset = uint16_t(scriptListOffset);
    }
  }

  if (d.lookupCount) {
    if (lookupKind == LookupKind::kGSUB)
      faceI->funcs.applyGSub = applyLookups<LookupKind::kGSUB, GSubContextPrimary>;
    else
      faceI->funcs.applyGPos = applyLookups<LookupKind::kGPOS, GPosContext>;
    faceI->layout.tables[size_t(lookupKind)] = table;
  }

  return BL_SUCCESS;
}

// bl::OpenType::LayoutImpl - Plan
// ===============================

static Table<GSubGPosTable::ScriptTable> findScriptInScriptList(Table<Array16<TagRef16>> scriptListOffsets, BLTag scriptTag, BLTag defaultScriptTag) noexcept {
  const TagRef16* scriptListArray = scriptListOffsets->array();
  uint32_t scriptCount = scriptListOffsets->count();

  Table<GSubGPosTable::ScriptTable> tableOut{};

  if (scriptListOffsets.size >= 2u + scriptCount * uint32_t(sizeof(TagRef16))) {
    for (uint32_t i = 0; i < scriptCount; i++) {
      BLTag recordTag = scriptListArray[i].tag();
      if (recordTag == scriptTag || recordTag == defaultScriptTag) {
        tableOut = scriptListOffsets.subTableUnchecked(scriptListArray[i].offset());
        if (recordTag == scriptTag)
          break;
      }
    }
  }

  return tableOut;
}

template<bool kSSO>
static BL_INLINE void populateGSubGPosLookupBits(
  Table<GSubGPosTable::LangSysTable> langSysTable,
  Table<Array16<TagRef16>> featureListOffsets,
  uint32_t featureIndexCount,
  uint32_t featureCount,
  uint32_t lookupCount,
  const BLFontFeatureSettings& settings,
  uint32_t* planBits) noexcept {

  BL_ASSERT(settings._d.sso() == kSSO);

  // We need to process requiredFeatureIndex as well as all the features from the list. To not duplicate the
  // code inside, we setup the `requiredFeatureIndex` here and just continue using the list after it's processed.
  uint32_t i = uint32_t(0) - 1u;
  uint32_t featureIndex = langSysTable->requiredFeatureIndex();

  for (;;) {
    if (BL_LIKELY(featureIndex < featureCount)) {
      BLTag featureTag = featureListOffsets->array()[featureIndex].tag();
      if (FontFeatureSettingsInternal::isFeatureEnabledForPlan<kSSO>(&settings, featureTag)) {
        uint32_t featureOffset = featureListOffsets->array()[featureIndex].offset();
        Table<GSubGPosTable::FeatureTable> featureTable(featureListOffsets.subTableUnchecked(featureOffset));

        // Don't use a feature if its offset is out of bounds.
        if (BL_LIKELY(blFontTableFitsT<GSubGPosTable::FeatureTable>(featureTable))) {
          // Don't use a lookup if its offset is out of bounds.
          uint32_t lookupIndexCount = featureTable->lookupListIndexes.count();
          if (BL_LIKELY(featureTable.size >= GSubGPosTable::FeatureTable::kBaseSize + lookupIndexCount * 2u)) {
            for (uint32_t j = 0; j < lookupIndexCount; j++) {
              uint32_t lookupIndex = featureTable->lookupListIndexes.array()[j].value();
              if (BL_LIKELY(lookupIndex < lookupCount))
                BitArrayOps::bitArraySetBit(planBits, lookupIndex);
            }
          }
        }
      }
    }

    if (++i >= featureIndexCount)
      break;

    featureIndex = langSysTable->featureIndexes.array()[i].value();
  }
}

static BLResult calculateGSubGPosPlan(const OTFaceImpl* faceI, const BLFontFeatureSettings& settings, LookupKind lookupKind, BLBitArrayCore* plan) noexcept {
  BLTag scriptTag = BL_MAKE_TAG('l', 'a', 't', 'n');
  BLTag dfltScriptTag = BL_MAKE_TAG('D', 'F', 'L', 'T');

  const LayoutData::GSubGPos& d = faceI->layout.kinds[size_t(lookupKind)];
  Table<GSubGPosTable> table = faceI->layout.tables[size_t(lookupKind)];

  if (!table)
    return BL_SUCCESS;

  Table<Array16<TagRef16>> scriptListOffsets(table.subTableUnchecked(d.scriptListOffset));
  Table<Array16<TagRef16>> featureListOffsets(table.subTableUnchecked(d.featureListOffset));
  Table<GSubGPosTable::ScriptTable> scriptTable(findScriptInScriptList(scriptListOffsets, scriptTag, dfltScriptTag));

  if (scriptTable.empty())
    return BL_SUCCESS;

  if (BL_UNLIKELY(!blFontTableFitsT<GSubGPosTable::ScriptTable>(scriptTable)))
    return BL_SUCCESS;

  uint32_t langSysOffset = scriptTable->langSysDefault();

  /*
  {
    uint32_t langSysCount = scriptTable->langSysOffsets.count();
    for (uint32_t i = 0; i < langSysCount; i++) {
      uint32_t tag = scriptTable->langSysOffsets.array()[i].tag();
      if (tag == BL_MAKE_TAG('D', 'F', 'L', 'T')) {
        langSysOffset = scriptTable->langSysOffsets.array()[i].offset();
      }
    }
  }
  */

  if (langSysOffset == 0)
    return BL_SUCCESS;

  Table<GSubGPosTable::LangSysTable> langSysTable(scriptTable.subTableUnchecked(langSysOffset));
  if (BL_UNLIKELY(!blFontTableFitsT<GSubGPosTable::LangSysTable>(langSysTable)))
    return BL_SUCCESS;

  uint32_t featureIndexCount = langSysTable->featureIndexes.count();
  uint32_t requiredLangSysTableSize = (GSubGPosTable::LangSysTable::kBaseSize) + featureIndexCount * 2u;

  if (langSysTable.size < requiredLangSysTableSize)
    return BL_SUCCESS;

  uint32_t featureCount = featureListOffsets->count();
  if (featureListOffsets.size < 2u + featureCount * 2u)
    return BL_SUCCESS;

  uint32_t lookupCount = faceI->layout.byKind(lookupKind).lookupCount;

  uint32_t* planBits;
  BL_PROPAGATE(blBitArrayReplaceOp(plan, lookupCount, &planBits));

  if (settings._d.sso())
    populateGSubGPosLookupBits<true>(langSysTable, featureListOffsets, featureIndexCount, featureCount, lookupCount, settings, planBits);
  else
    populateGSubGPosLookupBits<false>(langSysTable, featureListOffsets, featureIndexCount, featureCount, lookupCount, settings, planBits);

  return BL_SUCCESS;
}

BLResult calculateGSubPlan(const OTFaceImpl* faceI, const BLFontFeatureSettings& settings, BLBitArrayCore* plan) noexcept {
  return calculateGSubGPosPlan(faceI, settings, LookupKind::kGSUB, plan);
}

BLResult calculateGPosPlan(const OTFaceImpl* faceI, const BLFontFeatureSettings& settings, BLBitArrayCore* plan) noexcept {
  return calculateGSubGPosPlan(faceI, settings, LookupKind::kGPOS, plan);
}

// bl::OpenType::LayoutImpl - Init
// ===============================

BLResult init(OTFaceImpl* faceI, OTFaceTables& tables) noexcept {
  if (tables.gdef)
    BL_PROPAGATE(initGDef(faceI, tables.gdef));

  if (tables.gsub)
    BL_PROPAGATE(initGSubGPos(faceI, tables.gsub, LookupKind::kGSUB));

  if (tables.gpos)
    BL_PROPAGATE(initGSubGPos(faceI, tables.gpos, LookupKind::kGPOS));

  BL_PROPAGATE(faceI->layout.allocateLookupStatusBits());

  return BL_SUCCESS;
}

} // {LayoutImpl}
} // {OpenType}
} // {bl}

BL_DIAGNOSTIC_POP
