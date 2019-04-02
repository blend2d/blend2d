// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_OPENTYPE_BLOTLAYOUT_P_H
#define BLEND2D_OPENTYPE_BLOTLAYOUT_P_H

#include "../blarray.h"
#include "../blglyphbuffer_p.h"
#include "../blsupport_p.h"
#include "../opentype/blotcore_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_opentype
//! \{

namespace BLOpenType {

// ============================================================================
// [BLOpenType::GSubContext]
// ============================================================================

//! A context used for OpenType substitution.
//!
//! It has two buffers - input and output. However, intput and output buffers
//! can be the same (see `inPlace()`), in that case the substitution happens
//! in-place. However, even in-place substitution can have less glyphs on output
//! than on input. Use `isSameIndex()` to effectively check whether the input
//! and output indexes are the same.
//!
//! When the context is used the implementation first tries to make in in-place,
//! and when it's not possible (multiple substitution) the output buffer is
//! allocated.
struct GSubContext {
  struct WorkBuffer {
    BLGlyphItem* itemData;
    BLGlyphInfo* infoData;
    size_t index;
    size_t end;
  };

  BLInternalGlyphBufferData* gbd;
  WorkBuffer in, out;

  BL_INLINE void init(BLInternalGlyphBufferData* gbd_) noexcept {
    gbd = gbd_;

    in.itemData = gbd->glyphItemData;
    in.infoData = gbd->glyphInfoData;
    in.index = 0;
    in.end = gbd->size;

    out.itemData = gbd->glyphItemData;
    out.infoData = gbd->glyphInfoData;
    out.index = 0;
    out.end = gbd->capacity[0];
  }

  BL_INLINE void done() noexcept {
    if (!inPlace()) {
      gbd->flip();
      gbd->getGlyphDataPtrs(0, &gbd->glyphItemData, &gbd->glyphInfoData);
    }
    gbd->size = out.index;
  }

  //! Gets whether the `in` data is the same as `out` data.
  BL_INLINE bool inPlace() const noexcept { return in.itemData == out.itemData; }
  //! Gets whether the index in `in` data is the same as index in `out` data.
  BL_INLINE bool isSameIndex() const noexcept { return in.index == out.index; }

  //! Returns the number of glyphs to be processed on input.
  BL_INLINE size_t inRemaining() const noexcept { return in.end - in.index; }
  //! Returns the number of glyphs reserved on output.
  BL_INLINE size_t outRemaining() const noexcept { return out.end - out.index; }

  BL_INLINE BLResult advance(size_t n) noexcept {
    BL_ASSERT(in.end - in.index >= n);
    if (!(inPlace() && isSameIndex())) {
      BL_PROPAGATE(prepareOut(n));
      blCopyGlyphData(out.itemData, out.infoData, in.itemData, in.infoData, n);
    }

    in.index += n;
    out.index += n;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult advanceUncheckedWithCopy(size_t n) noexcept {
    BL_ASSERT(in.end - in.index >= n);
    BL_ASSERT(out.end - out.index >= n);

    blCopyGlyphData(out.itemData, out.infoData, in.itemData, in.infoData, n);
    in.index += n;
    out.index += n;
    return BL_SUCCESS;
  }

  //! Reserves at least `n` items in the output buffer, and makes sure the
  //! output buffer is allocated.
  BL_NOINLINE BLResult prepareOut(size_t n) noexcept {
    if (inPlace()) {
      BL_PROPAGATE(gbd->ensureBuffer(1, 0, n));

      size_t index = in.index;
      out.index = index;
      out.end = gbd->capacity[1];
      gbd->getGlyphDataPtrs(1, &out.itemData, &out.infoData);

      blCopyGlyphData(out.itemData, out.infoData, in.itemData, in.infoData, index);
      return BL_SUCCESS;
    }
    else {
      if (out.end - out.index >= n)
        return BL_SUCCESS;

      BLOverflowFlag of = 0;
      size_t minCapacity = blAddOverflow(out.index, n, &of);

      if (BL_UNLIKELY(minCapacity >= SIZE_MAX - BL_GLYPH_BUFFER_AGGRESIVE_GROWTH * 2u || of))
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

      size_t newCapacity;
      if (minCapacity < BL_GLYPH_BUFFER_AGGRESIVE_GROWTH)
        newCapacity = blAlignUpPowerOf2(minCapacity + (BL_GLYPH_BUFFER_AGGRESIVE_GROWTH / 2));
      else
        newCapacity = blAlignUp(minCapacity + BL_GLYPH_BUFFER_AGGRESIVE_GROWTH / 2, BL_GLYPH_BUFFER_AGGRESIVE_GROWTH);

      BL_PROPAGATE(gbd->ensureBuffer(1, out.index, minCapacity));
      gbd->getGlyphDataPtrs(1, &out.itemData, &out.infoData);

      return BL_SUCCESS;
    }
  }
};

// ============================================================================
// [BLOpenType::GPosContext]
// ============================================================================

struct GPosContext {
  BLInternalGlyphBufferData* gbd;

  BL_INLINE void init(BLInternalGlyphBufferData* gbd_) noexcept {
    gbd = gbd_;
  }

  BL_INLINE void done() noexcept {
  }
};

// ============================================================================
// [BLOpenType::CoverageTable]
// ============================================================================

//! OpenType coverage table.
struct CoverageTable {
  enum : uint32_t { kMinSize = 4 };

  struct Range {
    UInt16 firstGlyph;
    UInt16 lastGlyph;
    UInt16 startCoverageIndex;
  };

  struct Format1 {
    enum : uint32_t { kMinSize = 4 };

    UInt16 format;
    Array16<UInt16> glyphs;
  };

  struct Format2 {
    enum : uint32_t { kMinSize = 4 };

    UInt16 format;
    Array16<Range> ranges;
  };

  UInt16 format;
  Array16<void> array;

  BL_INLINE const Format1* format1() const noexcept { return blOffsetPtr<const Format1>(this, 0); }
  BL_INLINE const Format2* format2() const noexcept { return blOffsetPtr<const Format2>(this, 0); }
};

// ============================================================================
// [BLOpenType::ClassDefTable]
// ============================================================================

//! OpenType class-definition table.
struct ClassDefTable {
  enum : uint32_t { kMinSize = 4 };

  struct Range {
    UInt16 firstGlyph;
    UInt16 lastGlyph;
    UInt16 classValue;
  };

  struct Format1 {
    enum : uint32_t { kMinSize = 6 };

    UInt16 format;
    UInt16 firstGlyph;
    Array16<UInt16> classValues;
  };

  struct Format2 {
    enum : uint32_t { kMinSize = 4 };

    UInt16 format;
    Array16<Range> ranges;
  };

  UInt16 format;

  BL_INLINE const Format1* format1() const noexcept { return blOffsetPtr<const Format1>(this, 0); }
  BL_INLINE const Format2* format2() const noexcept { return blOffsetPtr<const Format2>(this, 0); }
};

// ============================================================================
// [BLOpenType::ConditionTable]
// ============================================================================

//! OpenType condition table.
struct ConditionTable {
  enum : uint32_t { kMinSize = 2 };

  struct Format1 {
    enum : uint32_t { kMinSize = 8 };

    UInt16 format;
    UInt16 axisIndex;
    F2x14 filterRangeMinValue;
    F2x14 filterRangeMaxValue;
  };

  UInt16 format;

  BL_INLINE const Format1* format1() const noexcept { return blOffsetPtr<const Format1>(this, 0); }
};

// ============================================================================
// [BLOpenType::GDefTable]
// ============================================================================

//! OpenType 'GDEF' table.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/gdef
struct GDefTable {
  enum : uint32_t { kMinSize = 12 };

  struct HeaderV1_0 {
    enum : uint32_t { kMinSize = 12 };

    F16x16 version;
    UInt16 glyphClassDefOffset;
    UInt16 attachListOffset;
    UInt16 ligCaretListOffset;
    UInt16 markAttachClassDefOffset;
  };

  struct HeaderV1_2 : public HeaderV1_0 {
    enum : uint32_t { kMinSize = 14 };

    UInt16 markGlyphSetsDefOffset;
  };

  struct HeaderV1_3 : public HeaderV1_2 {
    enum : uint32_t { kMinSize = 18 };

    UInt32 itemVarStoreOffset;
  };

  HeaderV1_0 header;

  BL_INLINE const HeaderV1_0* v1_0() const noexcept { return &header; }
  BL_INLINE const HeaderV1_2* v1_2() const noexcept { return blOffsetPtr<const HeaderV1_2>(this, 0); }
  BL_INLINE const HeaderV1_3* v1_3() const noexcept { return blOffsetPtr<const HeaderV1_3>(this, 0); }
};

// ============================================================================
// [BLOpenType::GAnyTable]
// ============================================================================

//! Base class for 'GSUB' and 'GPOS' tables.
struct GAnyTable {
  enum : uint32_t { kMinSize = 10 };

  enum : uint16_t {
    //! No feature required, possibly stored in `LangSysTable::requiredFeatureIndex`.
    kFeatureNotRequired = 0xFFFFu
  };

  struct HeaderV1_0 {
    enum : uint32_t { kMinSize = 10 };

    F16x16 version;
    UInt16 scriptListOffset;
    UInt16 featureListOffset;
    UInt16 lookupListOffset;
  };

  struct HeaderV1_1 : public HeaderV1_0 {
    enum : uint32_t { kMinSize = 14 };

    UInt32 featureVariationsOffset;
  };

  struct LookupHeader {
    enum : uint32_t { kMinSize = 4 };

    UInt16 format;
  };

  struct LookupHeaderWithCoverage : public LookupHeader {
    UInt16 coverageOffset;
  };

  struct ExtensionLookup : public LookupHeader {
    UInt16 lookupType;
    UInt32 offset;
  };

  typedef TagRef16 LangSysRecord;
  struct LangSysTable {
    enum : uint32_t { kMinSize = 6 };

    UInt16 lookupOrderOffset;
    UInt16 requiredFeatureIndex;
    Array16<UInt16> featureIndexes;
  };

  struct ScriptTable {
    enum : uint32_t { kMinSize = 4 };

    UInt16 langSysDefault;
    Array16<TagRef16> langSysOffsets;
  };

  struct FeatureTable {
    enum : uint32_t { kMinSize = 4 };

    typedef TagRef16 Record;
    typedef Array16<Record> List;

    UInt16 featureParamsOffset;
    Array16<UInt16> lookupListIndexes;
  };

  struct LookupTable {
    enum : uint32_t { kMinSize = 6 };

    enum Flags : uint16_t {
      kFlagRightToLeft        = 0x0001u, //!< Relates only to the correct processing of the cursive attachment lookup type (GPOS lookup type 3).
      kFlagIgnoreBaseGlyphs   = 0x0002u, //!< Skips over base glyphs.
      kFlagIgnoreLigatures    = 0x0004u, //!< Skips over ligatures.
      kFlagIgnoreMarks        = 0x0008u, //!< Skips over all combining marks.
      kFlagUseMarkFilteringSet= 0x0010u, //!< Indicates that the lookup table structure is followed by a `markFilteringSet` field.
      kFlagReserved           = 0x00E0u, //!< Must be zero.
      kFlagMarkAttachmentType = 0xFF00u  //!< If non-zero, skips over all marks of attachment type different from specified.
    };

    UInt16 lookupType;
    UInt16 lookupFlags;
    Array16<UInt16> lookupOffsets;
    /*
    UInt16 markFilteringSet;
    */
  };

  HeaderV1_0 header;

  BL_INLINE const HeaderV1_0* v1_0() const noexcept { return &header; }
  BL_INLINE const HeaderV1_1* v1_1() const noexcept { return blOffsetPtr<const HeaderV1_1>(this, 0); }
};

// ============================================================================
// [BLOpenType::GSubTable]
// ============================================================================

//! Glyph Substitution Table 'GSUB'.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/gsub
//!   - https://fontforge.github.io/gposgsub.html
struct GSubTable : public GAnyTable {
  enum LookupType : uint8_t {
    kLookupSingle                  = 1,
    kLookupMultiple                = 2,
    kLookupAlternate               = 3,
    kLookupLigature                = 4,
    kLookupContext                 = 5,
    kLookupChainedContext          = 6,
    kLookupExtension               = 7,
    kLookupReverseChainedContext   = 8,
    kLookupCount                   = 9
  };

  struct SubstLookupRecord {
    UInt16 glyphSequenceIndex;
    UInt16 lookupListIndex;
  };

  // --------------------------------------------------------------------------
  // [Lookup Type 1 - SingleSubst]
  // --------------------------------------------------------------------------

  struct SingleSubst1 : public LookupHeaderWithCoverage {
    Int16 deltaGlyphId;
  };

  struct SingleSubst2 : public LookupHeaderWithCoverage {
    Array16<UInt16> glyphs;
  };

  // --------------------------------------------------------------------------
  // [Lookup Type 2 - MultipleSubst]
  // --------------------------------------------------------------------------

  typedef Array16<UInt16> Sequence;

  struct MultipleSubst1 : public LookupHeaderWithCoverage {
    Array16<UInt16> sequenceOffsets;
  };

  // --------------------------------------------------------------------------
  // [Lookup Type 3 - AlternateSubst]
  // --------------------------------------------------------------------------

  typedef Array16<UInt16> AlternateSet;

  struct AlternateSubst1 : public LookupHeaderWithCoverage {
    Array16<UInt16> altSetOffsets;
  };

  // --------------------------------------------------------------------------
  // [Lookup Type 4 - LigatureSubst]
  // --------------------------------------------------------------------------

  struct Ligature {
    UInt16 ligatureGlyphId;
    Array16<UInt16> glyphs;
  };

  typedef Array16<UInt16> LigatureSet;

  struct LigatureSubst1 : public LookupHeaderWithCoverage {
    Array16<UInt16> ligSetOffsets;
  };

  // --------------------------------------------------------------------------
  // [Lookup Type 5 - ContextSubst]
  // --------------------------------------------------------------------------

  struct SubRule {
    UInt16 glyphCount;
    UInt16 substCount;
    /*
    UInt16 glyphArray[glyphCount - 1];
    SubstLookupRecord substArray[substCount];
    */

    BL_INLINE const UInt16* glyphArray() const noexcept { return blOffsetPtr<const UInt16>(this, 4); }
    BL_INLINE const SubstLookupRecord* substArray(size_t glyphCount) const noexcept { return blOffsetPtr<const SubstLookupRecord>(this, 4u + glyphCount * 2u - 2u); }
  };
  typedef SubRule SubClassRule;

  typedef Array16<UInt16> SubRuleSet;
  typedef Array16<UInt16> SubClassSet;

  struct ContextSubst1 : public LookupHeaderWithCoverage {
    Array16<UInt16> subRuleSetOffsets;
  };

  struct ContextSubst2 : public LookupHeaderWithCoverage {
    UInt16 classDefOffset;
    Array16<UInt16> subRuleSetOffsets;
  };

  struct ContextSubst3 : public LookupHeader {
    UInt16 glyphCount;
    UInt16 substCount;
    /*
    UInt16 coverageOffsetArray[glyphCount];
    SubstLookupRecord substArray[substCount];
    */

    BL_INLINE const UInt16* coverageOffsetArray() const noexcept { return blOffsetPtr<const UInt16>(this, 6); }
    BL_INLINE const SubstLookupRecord* substArray(size_t glyphCount) const noexcept { return blOffsetPtr<const SubstLookupRecord>(this, 6u + glyphCount * 2u); }
  };

  // --------------------------------------------------------------------------
  // [Lookup Type 6 - ChainContextSubst]
  // --------------------------------------------------------------------------

  struct ChainSubRule {
    UInt16 backtrackGlyphCount;
    /*
    UInt16 backtrackSequence[backtrackGlyphCount];
    UInt16 inputGlyphCount;
    UInt16 inputSequence[inputGlyphCount - 1];
    UInt16 lookaheadGlyphCount;
    UInt16 lookAheadSequence[lookaheadGlyphCount];
    UInt16 substCount;
    SubstLookupRecord substArray[substCount];
    */

    BL_INLINE const UInt16* backtrackSequence() const noexcept { return blOffsetPtr<const UInt16>(this, 2); }
  };
  typedef ChainSubRule ChainSubClassRule;

  typedef Array16<UInt16> ChainSubRuleSet;
  typedef Array16<UInt16> ChainSubClassRuleSet;

  struct ChainContextSubst1 : public LookupHeaderWithCoverage {
    Array16<UInt16> offsets;
  };

  struct ChainContextSubst2 : public LookupHeaderWithCoverage {
    UInt16 backtrackClassDefOffset;
    UInt16 inputClassDefOffset;
    UInt16 lookaheadClassDefOffset;
    Array16<UInt16> chainSubClassSets;
  };

  struct ChainContextSubst3 : public LookupHeader {
    UInt16 backtrackGlyphCount;
    /*
    UInt16 backtrackCoverageOffsets[backtrackGlyphCount];
    UInt16 inputGlyphCount;
    UInt16 inputCoverageOffsets[inputGlyphCount - 1];
    UInt16 lookaheadGlyphCount;
    UInt16 lookaheadCoverageOffsets[lookaheadGlyphCount];
    UInt16 substCount;
    SubstLookupRecord substArray[substCount];
    */

    BL_INLINE const UInt16* backtrackCoverageOffsets() const noexcept { return blOffsetPtr<const UInt16>(this, 4); }
  };

  // --------------------------------------------------------------------------
  // [Lookup Type 7 - Extension]
  // --------------------------------------------------------------------------

  // Use `ExtensionLookup` to handle this lookup type.

  // --------------------------------------------------------------------------
  // [Lookup Type 8 - ReverseChainSingleSubst]
  // --------------------------------------------------------------------------

  struct ReverseChainSingleSubst1 : public LookupHeaderWithCoverage {
    UInt16 backtrackGlyphCount;
    /*
    UInt16 backtrackCoverageOffsets[backtrackGlyphCount];
    UInt16 lookaheadGlyphCount;
    UInt16 lookaheadCoverageOffsets[lookaheadGlyphCount];
    UInt16 substGlyphCount;
    UInt16 substGlyphArray[substGlyphCount];
    */
  };
};

// ============================================================================
// [BLOpenType::GPosTable]
// ============================================================================

//! OpenType 'GPOS' table.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/gpos
//!   - https://fontforge.github.io/gposgsub.html
struct GPosTable : public GAnyTable {
  enum LookupType : uint8_t {
    kLookupSingle             = 1,       //!< Adjust position of a single glyph.
    kLookupPair               = 2,       //!< Adjust position of a pair of glyphs.
    kLookupCursive            = 3,       //!< Attach cursive glyphs.
    kLookupMarkToBase         = 4,       //!< Attach a combining mark to a base glyph.
    kLookupMarkToLigature     = 5,       //!< Attach a combining mark to a ligature.
    kLookupMarkToMark         = 6,       //!< Attach a combining mark to another mark.
    kLookupContext            = 7,       //!< Position one or more glyphs in context.
    kLookupChainedContext     = 8,       //!< Position one or more glyphs in chained context.
    kLookupExtension          = 9,       //!< Extension mechanism for other positionings.
    kLookupCount              = 10
  };

  enum ValueFlags : uint16_t {
    kValueXPlacement          = 0x0001u,
    kValueYPlacement          = 0x0002u,
    kValueXAdvance            = 0x0004u,
    kValueYAdvance            = 0x0008u,
    kValueXPlacementDevice    = 0x0010u,
    kValueYPlacementDevice    = 0x0020u,
    kValueXAdvanceDevice      = 0x0040u,
    kValueYAdvanceDevice      = 0x0080u,
    kValurReservedFlags       = 0xFF00u
  };

  // --------------------------------------------------------------------------
  // [Anchor Table]
  // --------------------------------------------------------------------------

  struct Anchor1 {
    enum : uint32_t { kMinSize = 6 };

    UInt16 anchorFormat;
    Int16 xCoordinate;
    Int16 yCoordinate;
  };

  struct Anchor2 {
    enum : uint32_t { kMinSize = 8 };

    UInt16 anchorFormat;
    Int16 xCoordinate;
    Int16 yCoordinate;
    UInt16 anchorPoint;
  };

  struct Anchor3 {
    enum : uint32_t { kMinSize = 10 };

    UInt16 anchorFormat;
    Int16 xCoordinate;
    Int16 yCoordinate;
    UInt16 xDeviceOffset;
    UInt16 yDeviceOffset;
  };

  // --------------------------------------------------------------------------
  // [Mark]
  // --------------------------------------------------------------------------

  struct Mark {
    UInt16 markClass;
    UInt16 markAnchorOffset;
  };

  // --------------------------------------------------------------------------
  // [Lookup Type 1 - Single Adjustment]
  // --------------------------------------------------------------------------

  struct SingleAdjustment1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kMinSize = 6 };

    UInt16 valueFormat;

    BL_INLINE const UInt16* valueRecords() const noexcept { return blOffsetPtr<const UInt16>(this, 6u); }
  };

  struct SingleAdjustment2 : public LookupHeaderWithCoverage {
    enum : uint32_t { kMinSize = 8 };

    UInt16 valueFormat;
    UInt16 valueCount;

    BL_INLINE const UInt16* valueRecords() const noexcept { return blOffsetPtr<const UInt16>(this, 8u); }
  };

  // --------------------------------------------------------------------------
  // [Lookup Type 2 - Pair Adjustment]
  // --------------------------------------------------------------------------

  struct PairValueRecord {
    UInt16 secondGlyph;

    BL_INLINE const UInt16* valueRecords() const noexcept { return blOffsetPtr<const UInt16>(this, 2); }
  };

  struct PairAdjustment1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kMinSize = 10 };

    UInt16 valueFormat1;
    UInt16 valueFormat2;
    Array16<UInt16> pairSetOffsets;
  };

  struct PairAdjustment2 : public LookupHeaderWithCoverage {
    enum : uint32_t { kMinSize = 16 };

    UInt16 valueFormat1;
    UInt16 valueFormat2;
    UInt16 classDef1Offset;
    UInt16 classDef2Offset;
    UInt16 class1Count;
    UInt16 class2Count;
  };

  // --------------------------------------------------------------------------
  // [Lookup Type 3 - Cursive Attachment]
  // --------------------------------------------------------------------------

  struct EntryExit {
    UInt16 entryAnchorOffset;
    UInt16 exitAnchorOffset;
  };

  struct CursiveAttachment1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kMinSize = 6 };

    Array16<EntryExit> entryExits;
  };

  // --------------------------------------------------------------------------
  // [Lookup Type 4 - MarkToBase Attachment]
  // --------------------------------------------------------------------------

  struct MarkToBaseAttachment1 : public LookupHeader {
    enum : uint32_t { kMinSize = 12 };

    UInt16 markCoverageOffset;
    UInt16 baseCoverageOffset;
    UInt16 markClassCount;
    UInt16 markArrayOffset;
    UInt16 baseArrayOffset;
  };

  // --------------------------------------------------------------------------
  // [Lookup Type 5 - MarkToLigature Attachment]
  // --------------------------------------------------------------------------

  struct MarkToLigatureAttachment1 : public LookupHeader {
    enum : uint32_t { kMinSize = 12 };

    UInt16 markCoverageOffset;
    UInt16 ligatureCoverageOffset;
    UInt16 markClassCount;
    UInt16 markArrayOffset;
    UInt16 ligatureArrayOffset;
  };

  // --------------------------------------------------------------------------
  // [Lookup Type 6 - MarkToMark Attachment]
  // --------------------------------------------------------------------------

  struct MarkToMarkAttachment1 : public LookupHeader {
    enum : uint32_t { kMinSize = 12 };

    UInt16 mark1CoverageOffset;
    UInt16 mark2CoverageOffset;
    UInt16 markClassCount;
    UInt16 mark1ArrayOffset;
    UInt16 mark2ArrayOffset;
  };

  // --------------------------------------------------------------------------
  // [Lookup Type 7 - Context Positioning]
  // --------------------------------------------------------------------------

  struct ContextPositioning1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kMinSize = 6 };

    UInt16 posRuleSetCount;
  };

  struct ContextPositioning2 : public LookupHeaderWithCoverage {
    enum : uint32_t { kMinSize = 8 };

    UInt16 classDefOffset;
    Array16<UInt16> posClassSets;
  };

  struct ContextPositioning3 : public LookupHeader {
    enum : uint32_t { kMinSize = 6 };

    UInt16 glyphCount;
    UInt16 posCount;
  };

  // --------------------------------------------------------------------------
  // [Lookup Type 8 - Chained Contextual Positioning]
  // --------------------------------------------------------------------------

  // TODO: [OPENTYPE GPOS]

  // --------------------------------------------------------------------------
  // [Lookup Type 9 - Extension]
  // --------------------------------------------------------------------------

  // Use `ExtensionLookup` to handle this lookup type.
};

// ============================================================================
// [BLOpenType::LookupInfo]
// ============================================================================

struct LookupInfo {
  //! Kind of a lookup (either GPOS or GSUB).
  enum Kind : uint32_t {
    kKindGSub  = 0,
    kKindGPos  = 1
  };

  //! GSUB LookupType combined with Format.
  enum GSubId : uint8_t {
    kGSubNone = 0,
    kGSubType1Format1, kGSubType1Format2,
    kGSubType2Format1,
    kGSubType3Format1,
    kGSubType4Format1,
    kGSubType5Format1, kGSubType5Format2, kGSubType5Format3,
    kGSubType6Format1, kGSubType6Format2, kGSubType6Format3,
    kGSubType8Format1,
    kGSubCount
  };

  //! GPOS LookupType combined with Format.
  enum GPosId : uint8_t {
    kGPosNone = 0,
    kGPosType1Format1, kGPosType1Format2,
    kGPosType2Format1, kGPosType2Format2,
    kGPosType3Format1,
    kGPosType4Format1,
    kGPosType5Format1,
    kGPosType6Format1,
    kGPosType7Format1, kGPosType7Format2, kGPosType7Format3,
    kGPosType8Format1, kGPosType8Format2, kGPosType8Format3,
    kGPosCount
  };

  enum : uint32_t {
    kTypeCount = 10,
    kIdCount   = 20
  };

  //! Structure that describes a lookup of a specific LookupType of any format.
  struct TypeEntry {
    uint8_t formatCount;
    uint8_t lookupIdIndex;
  };

  //! Structure that describes a lookup of a specific LookupType and Format.
  struct IdEntry {
    uint8_t headerSize;
  };

  uint8_t lookupCount;
  uint8_t extensionType;
  TypeEntry typeEntries[kTypeCount];
  IdEntry idEntries[kIdCount];
};

// ============================================================================
// [BLOpenType::LayoutData]
// ============================================================================

//! Data stored in `BLOTFaceImpl` related to OpenType advanced layout features.
struct LayoutData {
  struct LookupEntry {
    uint8_t type;
    uint8_t format;
    uint16_t flags;
    uint32_t offset;
  };

  struct TableRef {
    uint32_t format : 4;
    uint32_t offset : 28;

    BL_INLINE void reset(uint32_t format, uint32_t offset) noexcept {
      this->format = format & 0xFu;
      this->offset = offset & 0x0FFFFFFFu;
    }
  };

  struct GDef {
    TableRef glyphClassDef;
    TableRef markAttachClassDef;

    uint16_t attachListOffset;
    uint16_t ligCaretListOffset;
    uint16_t markGlyphSetsDefOffset;
    uint32_t itemVarStoreOffset;
  };

  struct GAny {
    uint16_t scriptListOffset;
    uint16_t featureListOffset;
    uint16_t lookupListOffset;
    uint16_t featureCount;
    uint16_t lookupCount;
    uint32_t lookupTypes;
  };

  BLFontTable tables[3];
  GDef gdef;
  union {
    GAny kinds[2];
    struct {
      GAny gsub;
      GAny gpos;
    };
  };

  BL_INLINE LayoutData() noexcept
    : tables {},
      gdef {},
      kinds {} {}
};

// ============================================================================
// [BLOpenType::LayoutImpl]
// ============================================================================

namespace LayoutImpl {
  BLResult init(BLOTFaceImpl* faceI, const BLFontData* fontData) noexcept;
}

} // {BLOpenType}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_BLOTLAYOUT_P_H
