// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_OPENTYPE_OTLAYOUTTABLES_P_H_INCLUDED
#define BLEND2D_OPENTYPE_OTLAYOUTTABLES_P_H_INCLUDED

#include "../opentype/otcore_p.h"
#include "../support/ptrops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_opentype_impl
//! \{

namespace bl {
namespace OpenType {

enum class LookupFlags : uint32_t {
  //! Relates only to the correct processing of the cursive attachment lookup type (GPOS lookup type 3).
  kRightToLeft = 0x0001u,
  //! Skips over base glyphs.
  kIgnoreBaseGlyphs = 0x0002u,
  //! Skips over ligatures.
  kIgnoreLigatures = 0x0004u,
  //! Skips over all combining marks.
  kIgnoreMarks = 0x0008u,
  //! Indicates that the lookup table structure is followed by a `markFilteringSet` field.
  kUseMarkFilteringSet = 0x0010u,
  //! Must be zero.
  kReserved = 0x00E0u,
  //! If non-zero, skips over all marks of attachment type different from specified.
  kMarkAttachmentType = 0xFF00u
};

BL_DEFINE_ENUM_FLAGS(LookupFlags)

//! OpenType coverage table.
struct CoverageTable {
  enum : uint32_t { kBaseSize = 4 };

  struct Range {
    enum : uint32_t { kBaseSize = 6 };

    UInt16 firstGlyph;
    UInt16 lastGlyph;
    UInt16 startCoverageIndex;
  };

  struct Format1 {
    enum : uint32_t { kBaseSize = 4 };

    UInt16 format;
    Array16<UInt16> glyphs;
  };

  struct Format2 {
    enum : uint32_t { kBaseSize = 4 };

    UInt16 format;
    Array16<Range> ranges;
  };

  UInt16 format;
  Array16<void> array;

  BL_INLINE_NODEBUG const Format1* format1() const noexcept { return PtrOps::offset<const Format1>(this, 0); }
  BL_INLINE_NODEBUG const Format2* format2() const noexcept { return PtrOps::offset<const Format2>(this, 0); }

  // Format 1 has 2 byte entries, format 2 has 6 byte entries - other formats don't exist.
  static BL_INLINE_NODEBUG uint32_t entrySizeByFormat(uint32_t format) noexcept { return format * 4u - 2u; }
};

class CoverageTableIterator {
public:
  typedef CoverageTable::Range Range;

  const void* _array;
  size_t _size;

  BL_INLINE_IF_NOT_DEBUG uint32_t init(RawTable table) noexcept {
    BL_ASSERT_VALIDATED(table.fits(CoverageTable::kBaseSize));

    uint32_t format = table.dataAs<CoverageTable>()->format();
    BL_ASSERT_VALIDATED(format == 1 || format == 2);

    uint32_t size = table.dataAs<CoverageTable>()->array.count();
    BL_ASSERT_VALIDATED(table.fits(CoverageTable::kBaseSize + size * CoverageTable::entrySizeByFormat(format)));

    _array = table.dataAs<CoverageTable>()->array.array();
    _size = size;
    return format;
  }

  template<typename T>
  BL_INLINE_NODEBUG const T& at(size_t index) const noexcept {
    return static_cast<const T*>(_array)[index];
  }

  template<uint32_t Format>
  BL_INLINE uint32_t minGlyphId() const noexcept {
    if (Format == 1)
      return at<UInt16>(0).value();
    else
      return at<Range>(0).firstGlyph();
  }

  template<uint32_t Format>
  BL_INLINE uint32_t maxGlyphId() const noexcept {
    if (Format == 1)
      return at<UInt16>(_size - 1).value();
    else
      return at<Range>(_size - 1).lastGlyph();
  }

  template<uint32_t Format>
  BL_INLINE_NODEBUG GlyphRange glyphRange() const noexcept {
    return GlyphRange{minGlyphId<Format>(), maxGlyphId<Format>()};
  }

  // Like `glyphRange()`, but used when the coverage table format cannot be templatized.
  BL_INLINE GlyphRange glyphRangeWithFormat(uint32_t format) const noexcept {
    if (format == 1)
      return GlyphRange{minGlyphId<1>(), maxGlyphId<1>()};
    else
      return GlyphRange{minGlyphId<2>(), maxGlyphId<2>()};
  }

  template<uint32_t Format>
  BL_INLINE_IF_NOT_DEBUG bool find(BLGlyphId glyphId, uint32_t& coverageIndex) const noexcept {
    if (Format == 1) {
      const UInt16* base = static_cast<const UInt16*>(_array);
      size_t size = _size;

      while (size_t half = size / 2u) {
        const UInt16* middle = base + half;
        size -= half;
        if (glyphId >= middle->value())
          base = middle;
      }

      coverageIndex = uint32_t(size_t(base - static_cast<const UInt16*>(_array)));
      return base->value() == glyphId;
    }
    else {
      const Range* base = static_cast<const Range*>(_array);
      size_t size = _size;

      while (size_t half = size / 2u) {
        const Range* middle = base + half;
        size -= half;
        if (glyphId >= middle->firstGlyph())
          base = middle;
      }

      coverageIndex = uint32_t(base->startCoverageIndex()) + glyphId - base->firstGlyph();
      uint32_t firstGlyph = base->firstGlyph();
      uint32_t lastGlyph = base->lastGlyph();
      return glyphId >= firstGlyph && glyphId <= lastGlyph;
    }
  }

  // Like `find()`, but used when the coverage table format cannot be templatized.
  BL_INLINE bool findWithFormat(uint32_t format, BLGlyphId glyphId, uint32_t& coverageIndex) const noexcept {
    if (format == 1)
      return find<1>(glyphId, coverageIndex);
    else
      return find<2>(glyphId, coverageIndex);
  }
};

//! OpenType class-definition table.
struct ClassDefTable {
  // Let's assume that Format2 table would contain at least one record.
  enum : uint32_t { kBaseSize = 6 };

  struct Range {
    UInt16 firstGlyph;
    UInt16 lastGlyph;
    UInt16 classValue;
  };

  struct Format1 {
    enum : uint32_t { kBaseSize = 6 };

    UInt16 format;
    UInt16 firstGlyph;
    Array16<UInt16> classValues;
  };

  struct Format2 {
    enum : uint32_t { kBaseSize = 4 };

    UInt16 format;
    Array16<Range> ranges;
  };

  UInt16 format;

  BL_INLINE_NODEBUG const Format1* format1() const noexcept { return PtrOps::offset<const Format1>(this, 0); }
  BL_INLINE_NODEBUG const Format2* format2() const noexcept { return PtrOps::offset<const Format2>(this, 0); }
};

class ClassDefTableIterator {
public:
  typedef ClassDefTable::Range Range;

  const void* _array;
  uint32_t _size;
  uint32_t _firstGlyph;

  BL_INLINE_IF_NOT_DEBUG uint32_t init(RawTable table) noexcept {
    const void* array = nullptr;
    uint32_t size = 0;
    uint32_t format = 0;
    uint32_t firstGlyph = 0;

    if (BL_LIKELY(table.size >= ClassDefTable::kBaseSize)) {
      uint32_t requiredTableSize = 0;
      format = table.dataAs<ClassDefTable>()->format();

      switch (format) {
        case 1: {
          const ClassDefTable::Format1* fmt1 = table.dataAs<ClassDefTable::Format1>();

          size = fmt1->classValues.count();
          array = fmt1->classValues.array();
          firstGlyph = fmt1->firstGlyph();
          requiredTableSize = uint32_t(sizeof(ClassDefTable::Format1)) + size * 2u;
          break;
        }

        case 2: {
          const ClassDefTable::Format2* fmt2 = table.dataAs<ClassDefTable::Format2>();

          size = fmt2->ranges.count();
          array = fmt2->ranges.array();
          // Minimum table size that we check is 6, so we are still fine here...
          firstGlyph = fmt2->ranges.array()[0].firstGlyph();
          requiredTableSize = uint32_t(sizeof(ClassDefTable::Format2)) + size * uint32_t(sizeof(ClassDefTable::Range));
          break;
        }

        default:
          format = 0;
          break;
      }

      if (!size || requiredTableSize > table.size)
        format = 0;
    }

    _array = array;
    _size = size;
    _firstGlyph = firstGlyph;

    return format;
  }

  template<typename T>
  BL_INLINE_NODEBUG const T& at(size_t index) const noexcept { return static_cast<const T*>(_array)[index]; }

  template<uint32_t Format>
  BL_INLINE_NODEBUG uint32_t minGlyphId() const noexcept {
    return _firstGlyph;
  }

  template<uint32_t Format>
  BL_INLINE uint32_t maxGlyphId() const noexcept {
    if (Format == 1)
      return _firstGlyph + uint32_t(_size) - 1;
    else
      return at<Range>(_size - 1).lastGlyph();
  }

  template<uint32_t Format>
  BL_INLINE_IF_NOT_DEBUG uint32_t classOfGlyph(BLGlyphId glyphId) const noexcept {
    if (Format == 1) {
      uint32_t index = glyphId - _firstGlyph;
      uint32_t fixedIndex = blMin(index, _size - 1);

      uint32_t classValue = at<UInt16>(fixedIndex).value();
      if (index >= _size)
        classValue = 0;
      return classValue;
    }
    else {
      const Range* base = static_cast<const Range*>(_array);
      uint32_t size = _size;

      while (uint32_t half = size / 2u) {
        const Range* middle = base + half;
        size -= half;
        if (glyphId >= middle->firstGlyph())
          base = middle;
      }

      uint32_t classValue = base->classValue();
      if (glyphId < base->firstGlyph() || glyphId > base->lastGlyph())
        classValue = 0;
      return classValue;
    }
  }

  template<uint32_t Format>
  BL_INLINE uint32_t matchGlyphClass(BLGlyphId glyphId, uint32_t classId) const noexcept {
    if (Format == 1) {
      uint32_t index = glyphId - _firstGlyph;
      return index == classId && classId < _size;
    }
    else {
      return classOfGlyph<2>(glyphId) == classId;
    }
  }
};

//! OpenType condition table.
struct ConditionTable {
  enum : uint32_t { kBaseSize = 2 };

  struct Format1 {
    enum : uint32_t { kBaseSize = 8 };

    UInt16 format;
    UInt16 axisIndex;
    F2x14 filterRangeMinValue;
    F2x14 filterRangeMaxValue;
  };

  UInt16 format;

  BL_INLINE_NODEBUG const Format1* format1() const noexcept { return PtrOps::offset<const Format1>(this, 0); }
};

//! OpenType 'GDEF' table.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/gdef
struct GDefTable {
  enum : uint32_t { kBaseSize = 12 };

  struct HeaderV1_0 {
    enum : uint32_t { kBaseSize = 12 };

    F16x16 version;
    Offset16 glyphClassDefOffset;
    Offset16 attachListOffset;
    Offset16 ligCaretListOffset;
    Offset16 markAttachClassDefOffset;
  };

  struct HeaderV1_2 : public HeaderV1_0 {
    enum : uint32_t { kBaseSize = 14 };

    UInt16 markGlyphSetsDefOffset;
  };

  struct HeaderV1_3 : public HeaderV1_2 {
    enum : uint32_t { kBaseSize = 18 };

    UInt32 itemVarStoreOffset;
  };

  HeaderV1_0 header;

  BL_INLINE_NODEBUG const HeaderV1_0* v1_0() const noexcept { return &header; }
  BL_INLINE_NODEBUG const HeaderV1_2* v1_2() const noexcept { return PtrOps::offset<const HeaderV1_2>(this, 0); }
  BL_INLINE_NODEBUG const HeaderV1_3* v1_3() const noexcept { return PtrOps::offset<const HeaderV1_3>(this, 0); }
};

//! Base class for 'GSUB' and 'GPOS' tables.
struct GSubGPosTable {
  enum : uint32_t { kBaseSize = 10 };

  //! No feature required, possibly stored in `LangSysTable::requiredFeatureIndex`.
  static constexpr uint16_t kFeatureNotRequired = 0xFFFFu;

  //! \name GPOS & GSUB - Core Tables
  //! \{

  struct HeaderV1_0 {
    enum : uint32_t { kBaseSize = 10 };

    F16x16 version;
    Offset16 scriptListOffset;
    Offset16 featureListOffset;
    Offset16 lookupListOffset;
  };

  struct HeaderV1_1 : public HeaderV1_0 {
    enum : uint32_t { kBaseSize = 14 };

    Offset32 featureVariationsOffset;
  };

  typedef TagRef16 LangSysRecord;
  struct LangSysTable {
    enum : uint32_t { kBaseSize = 6 };

    Offset16 lookupOrderOffset;
    UInt16 requiredFeatureIndex;
    Array16<UInt16> featureIndexes;
  };

  struct ScriptTable {
    enum : uint32_t { kBaseSize = 4 };

    UInt16 langSysDefault;
    Array16<TagRef16> langSysOffsets;
  };

  struct FeatureTable {
    enum : uint32_t { kBaseSize = 4 };

    typedef TagRef16 Record;
    typedef Array16<Record> List;

    Offset16 featureParamsOffset;
    Array16<UInt16> lookupListIndexes;
  };

  struct LookupTable {
    enum : uint32_t { kBaseSize = 6 };

    UInt16 lookupType;
    UInt16 lookupFlags;
    Array16<Offset16> subTableOffsets;
    /*
    UInt16 markFilteringSet;
    */
  };

  //! \]

  //! \name GSUB & GPOS - Lookup Headers
  //! \{

  struct LookupHeader {
    enum : uint32_t { kBaseSize = 2 };

    UInt16 format;
  };

  struct LookupHeaderWithCoverage : public LookupHeader {
    enum : uint32_t { kBaseSize = LookupHeader::kBaseSize + 2 };

    Offset16 coverageOffset;
  };

  struct ExtensionLookup : public LookupHeader {
    enum : uint32_t { kBaseSize = LookupHeader::kBaseSize + 6 };

    UInt16 lookupType;
    Offset32 offset;
  };

  //! \}

  //! \name GSUB & GPOS - Sequence Context Tables
  //! \{

  struct SequenceLookupRecord {
    enum : uint32_t { kBaseSize = 4 };

    UInt16 sequenceIndex;
    UInt16 lookupIndex;
  };

  typedef Array16<UInt16> SequenceRuleSet;

  struct SequenceRule {
    enum : uint32_t { kBaseSize = 4 };

    UInt16 glyphCount;
    UInt16 lookupRecordCount;
    /*
    UInt16 inputSequence[glyphCount - 1];
    SequenceLookupRecord lookupRecords[lookupCount];
    */

    BL_INLINE_NODEBUG const UInt16* inputSequence() const noexcept {
      return PtrOps::offset<const UInt16>(this, 4);
    }

    BL_INLINE_NODEBUG const SequenceLookupRecord* lookupRecordArray(uint32_t glyphCount_) const noexcept {
      return PtrOps::offset<const SequenceLookupRecord>(this, kBaseSize + glyphCount_ * 2u - 2u);
    }
  };

  struct SequenceContext1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kBaseSize = LookupHeaderWithCoverage::kBaseSize + 2 };

    Array16<Offset16> ruleSetOffsets;
  };

  struct SequenceContext2 : public LookupHeaderWithCoverage {
    enum : uint32_t { kBaseSize = LookupHeaderWithCoverage::kBaseSize + 4 };

    Offset16 classDefOffset;
    Array16<Offset16> ruleSetOffsets;
  };

  struct SequenceContext3 : public LookupHeader {
    enum : uint32_t { kBaseSize = LookupHeader::kBaseSize + 4 };

    UInt16 glyphCount;
    UInt16 lookupRecordCount;
    /*
    Offset16 coverageOffsetArray[glyphCount];
    SequenceLookupRecord lookupRecords[lookupRecordCount];
    */

    BL_INLINE_NODEBUG const UInt16* coverageOffsetArray() const noexcept { return PtrOps::offset<const UInt16>(this, kBaseSize); }
    BL_INLINE_NODEBUG const SequenceLookupRecord* lookupRecordArray(size_t glyphCount_) const noexcept { return PtrOps::offset<const SequenceLookupRecord>(this, kBaseSize + glyphCount_ * 2u); }
  };

  //! \}

  //! \name GSUB & GPOS - Chained Sequence Context Tables
  //! \{

  struct ChainedSequenceRule {
    enum : uint32_t { kBaseSize = 8 };

    UInt16 backtrackGlyphCount;
    /*
    UInt16 backtrackSequence[backtrackGlyphCount];
    UInt16 inputGlyphCount;
    UInt16 inputSequence[inputGlyphCount - 1];
    UInt16 lookaheadGlyphCount;
    UInt16 lookaheadSequence[lookaheadGlyphCount];
    UInt16 lookupRecordCount;
    SequenceLookupRecord lookupRecords[lookupRecordCount];
    */

    BL_INLINE const UInt16* backtrackSequence() const noexcept { return PtrOps::offset<const UInt16>(this, 2); }
  };

  typedef Array16<UInt16> ChainedSequenceRuleSet;

  struct ChainedSequenceContext1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kBaseSize = LookupHeaderWithCoverage::kBaseSize + 2 };

    Array16<Offset16> ruleSetOffsets;
  };

  struct ChainedSequenceContext2 : public LookupHeaderWithCoverage {
    enum : uint32_t { kBaseSize = LookupHeaderWithCoverage::kBaseSize + 8 };

    Offset16 backtrackClassDefOffset;
    Offset16 inputClassDefOffset;
    Offset16 lookaheadClassDefOffset;
    Array16<Offset16> ruleSetOffsets;
  };

  struct ChainedSequenceContext3 : public LookupHeader {
    enum : uint32_t { kBaseSize = LookupHeader::kBaseSize + 8 };

    UInt16 backtrackGlyphCount;
    /*
    Offset16 backtrackCoverageOffsets[backtrackGlyphCount];
    UInt16 inputGlyphCount;
    Offset16 inputCoverageOffsets[inputGlyphCount];
    UInt16 lookaheadGlyphCount;
    Offset16 lookaheadCoverageOffsets[lookaheadGlyphCount];
    UInt16 lookupRecordCount;
    SequenceLookupRecord lookupRecords[substCount];
    */

    BL_INLINE_NODEBUG const UInt16* backtrackCoverageOffsets() const noexcept { return PtrOps::offset<const UInt16>(this, 4); }
  };

  //! \}

  //! \name Members
  //! \{

  HeaderV1_0 header;

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG const HeaderV1_0* v1_0() const noexcept { return &header; }
  BL_INLINE_NODEBUG const HeaderV1_1* v1_1() const noexcept { return PtrOps::offset<const HeaderV1_1>(this, 0); }

  //! \}
};

//! Glyph Substitution Table 'GSUB'.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/gsub
//!   - https://fontforge.github.io/gposgsub.html
struct GSubTable : public GSubGPosTable {
  enum LookupType : uint8_t {
    kLookupSingle = 1,
    kLookupMultiple = 2,
    kLookupAlternate = 3,
    kLookupLigature = 4,
    kLookupContext = 5,
    kLookupChainedContext = 6,
    kLookupExtension = 7,
    //! Extension - access to lookup tables beyond a 16-bit offset.
    kLookupReverseChainedContext = 8,
    //! Maximum value of LookupType.
    kLookupMaxValue = 8
  };

  // Lookup Type 1 - SingleSubst
  // ---------------------------

  struct SingleSubst1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kBaseSize = LookupHeaderWithCoverage::kBaseSize + 2 };

    Int16 deltaGlyphId;
  };

  struct SingleSubst2 : public LookupHeaderWithCoverage {
    enum : uint32_t { kBaseSize = LookupHeaderWithCoverage::kBaseSize + 2 };

    Array16<UInt16> glyphs;
  };

  // Lookup Type 2 - MultipleSubst
  // -----------------------------

  typedef Array16<UInt16> Sequence;

  struct MultipleSubst1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kBaseSize = LookupHeaderWithCoverage::kBaseSize + 2 };

    Array16<Offset16> sequenceOffsets;
  };

  // Lookup Type 3 - AlternateSubst
  // ------------------------------

  typedef Array16<UInt16> AlternateSet;

  struct AlternateSubst1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kBaseSize = LookupHeaderWithCoverage::kBaseSize + 2 };

    Array16<Offset16> alternateSetOffsets;
  };

  // Lookup Type 4 - LigatureSubst
  // -----------------------------

  struct Ligature {
    UInt16 ligatureGlyphId;
    Array16<UInt16> glyphs;
  };

  typedef Array16<UInt16> LigatureSet;

  struct LigatureSubst1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kBaseSize = LookupHeaderWithCoverage::kBaseSize + 2 };

    Array16<Offset16> ligatureSetOffsets;
  };

  // Lookup Type 5 - ContextSubst
  // ----------------------------

  // Uses SequenceContext[1|2|3]

  // Lookup Type 6 - ChainedContextSubst
  // -----------------------------------

  // Uses ChainedSequenceContext[1|2|3]

  // Lookup Type 7 - Extension
  // -------------------------

  // Use `ExtensionLookup` to handle this lookup type.

  // Lookup Type 8 - ReverseChainedSingleSubst
  // -----------------------------------------

  struct ReverseChainedSingleSubst1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kBaseSize = LookupHeaderWithCoverage::kBaseSize + 2 };

    UInt16 backtrackGlyphCount;
    /*
    Offset16 backtrackCoverageOffsets[backtrackGlyphCount];
    UInt16 lookaheadGlyphCount;
    Offset16 lookaheadCoverageOffsets[lookaheadGlyphCount];
    UInt16 substGlyphCount;
    UInt16 substGlyphArray[substGlyphCount];
    */

    BL_INLINE_NODEBUG const UInt16* backtrackCoverageOffsets() const noexcept { return PtrOps::offset<const UInt16>(this, 6); }
  };
};

//! OpenType 'GPOS' table.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/gpos
//!   - https://fontforge.github.io/gposgsub.html
struct GPosTable : public GSubGPosTable {
  enum LookupType : uint8_t {
    //! Adjust position of a single glyph.
    kLookupSingle = 1,
    //! Adjust position of a pair of glyphs.
    kLookupPair = 2,
    //! Attach cursive glyphs.
    kLookupCursive = 3,
    //! Attach a combining mark to a base glyph.
    kLookupMarkToBase = 4,
    //! Attach a combining mark to a ligature.
    kLookupMarkToLigature = 5,
    //! Attach a combining mark to another mark.
    kLookupMarkToMark = 6,
    //! Position one or more glyphs in context.
    kLookupContext = 7,
    //! Position one or more glyphs in chained context.
    kLookupChainedContext = 8,
    //! Extension - access to lookup tables beyond a 16-bit offset.
    kLookupExtension = 9,
    //! Maximum value of LookupType.
    kLookupMaxValue = 9
  };

  enum ValueFlags : uint16_t {
    kValueXPlacement = 0x0001u,
    kValueYPlacement = 0x0002u,
    kValueXAdvance = 0x0004u,
    kValueYAdvance = 0x0008u,
    kValueXPlacementDevice = 0x0010u,
    kValueYPlacementDevice = 0x0020u,
    kValueXAdvanceDevice = 0x0040u,
    kValueYAdvanceDevice = 0x0080u,
    kValurReservedFlags = 0xFF00u
  };

  // Anchor Table
  // ------------

  struct Anchor1 {
    enum : uint32_t { kBaseSize = 6 };

    UInt16 anchorFormat;
    Int16 xCoordinate;
    Int16 yCoordinate;
  };

  struct Anchor2 {
    enum : uint32_t { kBaseSize = 8 };

    UInt16 anchorFormat;
    Int16 xCoordinate;
    Int16 yCoordinate;
    UInt16 anchorPoint;
  };

  struct Anchor3 {
    enum : uint32_t { kBaseSize = 10 };

    UInt16 anchorFormat;
    Int16 xCoordinate;
    Int16 yCoordinate;
    UInt16 xDeviceOffset;
    UInt16 yDeviceOffset;
  };

  // Mark
  // ----

  struct Mark {
    UInt16 markClass;
    UInt16 markAnchorOffset;
  };

  // Lookup Type 1 - Single Adjustment
  // ---------------------------------

  struct SingleAdjustment1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kBaseSize = LookupHeaderWithCoverage::kBaseSize + 2 };

    UInt16 valueFormat;

    BL_INLINE_NODEBUG const UInt16* valueRecords() const noexcept { return PtrOps::offset<const UInt16>(this, 6u); }
  };

  struct SingleAdjustment2 : public LookupHeaderWithCoverage {
    enum : uint32_t { kBaseSize = LookupHeaderWithCoverage::kBaseSize + 4 };

    UInt16 valueFormat;
    UInt16 valueCount;

    BL_INLINE_NODEBUG const UInt16* valueRecords() const noexcept { return PtrOps::offset<const UInt16>(this, 8u); }
  };

  // Lookup Type 2 - Pair Adjustment
  // -------------------------------

  struct PairSet {
    UInt16 pairValueCount;

    BL_INLINE_NODEBUG const UInt16* pairValueRecords() const noexcept { return PtrOps::offset<const UInt16>(this, 2); }
  };

  struct PairValueRecord {
    UInt16 secondGlyph;

    BL_INLINE_NODEBUG const UInt16* valueRecords() const noexcept { return PtrOps::offset<const UInt16>(this, 2); }
  };

  struct PairAdjustment1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kBaseSize = LookupHeaderWithCoverage::kBaseSize + 6 };

    UInt16 valueFormat1;
    UInt16 valueFormat2;
    Array16<UInt16> pairSetOffsets;
  };

  struct PairAdjustment2 : public LookupHeaderWithCoverage {
    enum : uint32_t { kBaseSize = LookupHeaderWithCoverage::kBaseSize + 12 };

    UInt16 value1Format;
    UInt16 value2Format;
    Offset16 classDef1Offset;
    Offset16 classDef2Offset;
    UInt16 class1Count;
    UInt16 class2Count;
    /*
    struct ClassRecord {
      ValueRecord value1;
      ValueRecord value2;
    };
    ClassRecord classRecords[class1Count * class2Count];
    */
  };

  // Lookup Type 3 - Cursive Attachment
  // ----------------------------------

  struct EntryExit {
    enum : uint32_t { kBaseSize = 4 };

    Offset16 entryAnchorOffset;
    Offset16 exitAnchorOffset;
  };

  struct CursiveAttachment1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kBaseSize = LookupHeaderWithCoverage::kBaseSize + 2 };

    Array16<EntryExit> entryExits;
  };

  // Lookup Type 4 - MarkToBase Attachment
  // -------------------------------------

  struct MarkToBaseAttachment1 : public LookupHeader {
    enum : uint32_t { kBaseSize = LookupHeader::kBaseSize + 10 };

    Offset16 markCoverageOffset;
    Offset16 baseCoverageOffset;
    UInt16 markClassCount;
    Offset16 markArrayOffset;
    Offset16 baseArrayOffset;
  };

  // Lookup Type 5 - MarkToLigature Attachment
  // -----------------------------------------

  struct MarkToLigatureAttachment1 : public LookupHeader {
    enum : uint32_t { kBaseSize = LookupHeader::kBaseSize + 10 };

    Offset16 markCoverageOffset;
    Offset16 ligatureCoverageOffset;
    UInt16 markClassCount;
    Offset16 markArrayOffset;
    Offset16 ligatureArrayOffset;
  };

  // Lookup Type 6 - MarkToMark Attachment
  // -------------------------------------

  struct MarkToMarkAttachment1 : public LookupHeader {
    enum : uint32_t { kBaseSize = LookupHeader::kBaseSize + 10 };

    Offset16 mark1CoverageOffset;
    Offset16 mark2CoverageOffset;
    UInt16 markClassCount;
    Offset16 mark1ArrayOffset;
    Offset16 mark2ArrayOffset;
  };

  // Lookup Type 7 - Context Positioning
  // -----------------------------------

  // Uses SequenceContext[1|2|3]

  // Lookup Type 8 - Chained Contextual Positioning
  // ----------------------------------------------

  // Uses ChainedSequenceContext[1|2|3]

  // Lookup Type 9 - Extension
  // -------------------------

  // Use `ExtensionLookup` to handle this lookup type.
};

} // {OpenType}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_OTLAYOUTTABLES_P_H_INCLUDED
