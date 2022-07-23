// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_OPENTYPE_OTLAYOUT_P_H_INCLUDED
#define BLEND2D_OPENTYPE_OTLAYOUT_P_H_INCLUDED

#include "../array.h"
#include "../glyphbuffer_p.h"
#include "../opentype/otcore_p.h"
#include "../support/intops_p.h"
#include "../support/ptrops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_opentype_impl
//! \{

namespace BLOpenType {

//! A context used for OpenType substitution.
//!
//! It has two buffers - input and output. However, intput and output buffers can be the same (see `inPlace()`), in
//! that case the substitution happens in-place. However, even in-place substitution can have less glyphs on output
//! than on input. Use `isSameIndex()` to effectively check whether the input and output indexes are the same.
//!
//! When the context is used the implementation first tries to make in in-place, and when it's not possible (multiple
//! substitution) the output buffer is allocated.
struct GSubContext {
  struct WorkBuffer {
    uint32_t* glyphData;
    BLGlyphInfo* infoData;
    size_t index;
    size_t end;
  };

  BLGlyphBufferPrivateImpl* gbd;
  WorkBuffer in, out;

  BL_INLINE void init(BLGlyphBufferPrivateImpl* gbd_) noexcept {
    gbd = gbd_;

    in.glyphData = gbd->content;
    in.infoData = gbd->infoData;
    in.index = 0;
    in.end = gbd->size;

    out.glyphData = gbd->content;
    out.infoData = gbd->infoData;
    out.index = 0;
    out.end = gbd->capacity[0];
  }

  BL_INLINE void done() noexcept {
    if (!inPlace()) {
      gbd->flip();
      gbd->getGlyphDataPtrs(0, &gbd->content, &gbd->infoData);
    }
    gbd->size = out.index;
  }

  //! Tests whether the `in` data is the same as `out` data.
  BL_INLINE bool inPlace() const noexcept { return in.glyphData == out.glyphData; }
  //! Tests whether the index in `in` data is the same as index in `out` data.
  BL_INLINE bool isSameIndex() const noexcept { return in.index == out.index; }

  //! Returns the number of glyphs to be processed on input.
  BL_INLINE size_t inRemaining() const noexcept { return in.end - in.index; }
  //! Returns the number of glyphs reserved on output.
  BL_INLINE size_t outRemaining() const noexcept { return out.end - out.index; }

  BL_INLINE BLResult advance(size_t n) noexcept {
    BL_ASSERT(in.end - in.index >= n);
    if (!(inPlace() && isSameIndex())) {
      BL_PROPAGATE(prepareOut(n));
      blCopyGlyphData(out.glyphData, out.infoData, in.glyphData, in.infoData, n);
    }

    in.index += n;
    out.index += n;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult advanceUncheckedWithCopy(size_t n) noexcept {
    BL_ASSERT(in.end - in.index >= n);
    BL_ASSERT(out.end - out.index >= n);

    blCopyGlyphData(out.glyphData, out.infoData, in.glyphData, in.infoData, n);
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
      gbd->getGlyphDataPtrs(1, &out.glyphData, &out.infoData);

      blCopyGlyphData(out.glyphData, out.infoData, in.glyphData, in.infoData, index);
      return BL_SUCCESS;
    }
    else {
      if (out.end - out.index >= n)
        return BL_SUCCESS;

      BLOverflowFlag of = 0;
      size_t minCapacity = BLIntOps::addOverflow(out.index, n, &of);

      if (BL_UNLIKELY(minCapacity >= SIZE_MAX - BL_GLYPH_BUFFER_AGGRESIVE_GROWTH * 2u || of))
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

      size_t newCapacity;
      if (minCapacity < BL_GLYPH_BUFFER_AGGRESIVE_GROWTH)
        newCapacity = BLIntOps::alignUpPowerOf2(minCapacity + (BL_GLYPH_BUFFER_AGGRESIVE_GROWTH / 2));
      else
        newCapacity = BLIntOps::alignUp(minCapacity + BL_GLYPH_BUFFER_AGGRESIVE_GROWTH / 2, BL_GLYPH_BUFFER_AGGRESIVE_GROWTH);

      BL_PROPAGATE(gbd->ensureBuffer(1, out.index, newCapacity));
      gbd->getGlyphDataPtrs(1, &out.glyphData, &out.infoData);

      return BL_SUCCESS;
    }
  }
};

struct GPosContext {
  BLGlyphBufferPrivateImpl* gbd;
  uint32_t* glyphData;
  BLGlyphInfo* infoData;
  BLGlyphPlacement* placementData;
  size_t index;
  size_t end;

  BL_INLINE void init(BLGlyphBufferPrivateImpl* gbd_) noexcept {
    gbd = gbd_;
    glyphData = gbd->content;
    infoData = gbd->infoData;
    placementData = gbd->placementData;
    index = 0;
    end = gbd->size;
  }

  BL_INLINE void done() noexcept {
  }
};

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

  BL_INLINE const Format1* format1() const noexcept { return BLPtrOps::offset<const Format1>(this, 0); }
  BL_INLINE const Format2* format2() const noexcept { return BLPtrOps::offset<const Format2>(this, 0); }
};

class CoverageIterator {
public:
  typedef CoverageTable::Range Range;

  const void* _array;
  size_t _size;

  BL_INLINE uint32_t init(const BLFontTable& table) noexcept {
    const void* array = nullptr;
    uint32_t size = 0;
    uint32_t format = 0;

    if (BL_LIKELY(table.size >= CoverageTable::kMinSize)) {
      format = table.dataAs<CoverageTable>()->format();
      size = table.dataAs<CoverageTable>()->array.count();

      uint32_t entrySize = format == 1 ? uint32_t(2u) : uint32_t(sizeof(CoverageTable::Range));
      if (format > 2 || !size || table.size < CoverageTable::kMinSize + size * entrySize)
        format = 0;

      array = table.dataAs<CoverageTable>()->array.array();
    }

    _array = array;
    _size = size;

    return format;
  }

  template<typename T>
  BL_INLINE const T& at(size_t index) const noexcept { return static_cast<const T*>(_array)[index]; }

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
  BL_INLINE bool find(uint32_t glyphId, uint32_t& coverageIndex) const noexcept {
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
      return glyphId >= base->firstGlyph() && glyphId <= base->lastGlyph();
    }
  }
};

//! OpenType class-definition table.
struct ClassDefTable {
  // Let's assume that Format2 table would contain at least one record.
  enum : uint32_t { kMinSize = 6 };

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

  BL_INLINE const Format1* format1() const noexcept { return BLPtrOps::offset<const Format1>(this, 0); }
  BL_INLINE const Format2* format2() const noexcept { return BLPtrOps::offset<const Format2>(this, 0); }
};

class ClassDefIterator {
public:
  typedef ClassDefTable::Range Range;

  const void* _array;
  size_t _size;
  uint32_t _firstGlyph;

  BL_INLINE uint32_t init(const BLFontTable& table) noexcept {
    const void* array = nullptr;
    uint32_t size = 0;
    uint32_t format = 0;
    uint32_t firstGlyph = 0;

    if (BL_LIKELY(table.size >= ClassDefTable::kMinSize)) {
      size_t requiredTableSize = 0;
      format = table.dataAs<ClassDefTable>()->format();

      switch (format) {
        case 1: {
          const ClassDefTable::Format1* fmt1 = table.dataAs<ClassDefTable::Format1>();

          size = fmt1->classValues.count();
          array = fmt1->classValues.array();
          firstGlyph = fmt1->firstGlyph();
          requiredTableSize = sizeof(ClassDefTable::Format1) + size * 2u;
          break;
        }

        case 2: {
          const ClassDefTable::Format2* fmt2 = table.dataAs<ClassDefTable::Format2>();

          size = fmt2->ranges.count();
          array = fmt2->ranges.array();
          // Minimum table size that we check is 6, so we are still fine here...
          firstGlyph = fmt2->ranges.array()[0].firstGlyph();
          requiredTableSize = sizeof(ClassDefTable::Format2) + size * sizeof(ClassDefTable::Range);
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
  BL_INLINE const T& at(size_t index) const noexcept { return static_cast<const T*>(_array)[index]; }

  template<uint32_t Format>
  BL_INLINE uint32_t minGlyphId() const noexcept {
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
  BL_INLINE bool find(uint32_t glyphId, uint32_t& classValue) const noexcept {
    if (Format == 1) {
      size_t index = glyphId - _firstGlyph;
      size_t fixedIndex = blMin(index, _size - 1);

      classValue = at<UInt16>(fixedIndex).value();
      return index == fixedIndex;
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

      classValue = base->classValue();
      return glyphId >= base->firstGlyph() && glyphId <= base->lastGlyph();
    }
  }
};

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

  BL_INLINE const Format1* format1() const noexcept { return BLPtrOps::offset<const Format1>(this, 0); }
};

//! OpenType 'GDEF' table.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/gdef
struct GDefTable {
  enum : uint32_t { kMinSize = 12 };

  struct HeaderV1_0 {
    enum : uint32_t { kMinSize = 12 };

    F16x16 version;
    Offset16 glyphClassDefOffset;
    Offset16 attachListOffset;
    Offset16 ligCaretListOffset;
    Offset16 markAttachClassDefOffset;
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
  BL_INLINE const HeaderV1_2* v1_2() const noexcept { return BLPtrOps::offset<const HeaderV1_2>(this, 0); }
  BL_INLINE const HeaderV1_3* v1_3() const noexcept { return BLPtrOps::offset<const HeaderV1_3>(this, 0); }
};

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
    Offset16 scriptListOffset;
    Offset16 featureListOffset;
    Offset16 lookupListOffset;
  };

  struct HeaderV1_1 : public HeaderV1_0 {
    enum : uint32_t { kMinSize = 14 };

    Offset32 featureVariationsOffset;
  };

  struct LookupHeader {
    enum : uint32_t { kMinSize = 4 };

    UInt16 format;
  };

  struct LookupHeaderWithCoverage : public LookupHeader {
    Offset16 coverageOffset;
  };

  struct ExtensionLookup : public LookupHeader {
    UInt16 lookupType;
    Offset32 offset;
  };

  typedef TagRef16 LangSysRecord;
  struct LangSysTable {
    enum : uint32_t { kMinSize = 6 };

    Offset16 lookupOrderOffset;
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

    Offset16 featureParamsOffset;
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
    Array16<Offset16> lookupOffsets;
    /*
    UInt16 markFilteringSet;
    */
  };

  HeaderV1_0 header;

  BL_INLINE const HeaderV1_0* v1_0() const noexcept { return &header; }
  BL_INLINE const HeaderV1_1* v1_1() const noexcept { return BLPtrOps::offset<const HeaderV1_1>(this, 0); }
};

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
    Offset16 glyphSequenceIndex;
    Offset16 lookupListIndex;
  };

  // Lookup Type 1 - SingleSubst
  // ---------------------------

  struct SingleSubst1 : public LookupHeaderWithCoverage {
    Int16 deltaGlyphId;
  };

  struct SingleSubst2 : public LookupHeaderWithCoverage {
    Array16<UInt16> glyphs;
  };

  // Lookup Type 2 - MultipleSubst
  // -----------------------------

  typedef Array16<UInt16> Sequence;

  struct MultipleSubst1 : public LookupHeaderWithCoverage {
    Array16<Offset16> sequenceOffsets;
  };

  // Lookup Type 3 - AlternateSubst
  // ------------------------------

  typedef Array16<UInt16> AlternateSet;

  struct AlternateSubst1 : public LookupHeaderWithCoverage {
    Array16<Offset16> altSetOffsets;
  };

  // Lookup Type 4 - LigatureSubst
  // -----------------------------

  struct Ligature {
    UInt16 ligatureGlyphId;
    Array16<UInt16> glyphs;
  };

  typedef Array16<UInt16> LigatureSet;

  struct LigatureSubst1 : public LookupHeaderWithCoverage {
    Array16<Offset16> ligSetOffsets;
  };

  // Lookup Type 5 - ContextSubst
  // ----------------------------

  struct SubRule {
    UInt16 glyphCount;
    UInt16 substCount;
    /*
    UInt16 glyphArray[glyphCount - 1];
    SubstLookupRecord substArray[substCount];
    */

    BL_INLINE const UInt16* glyphArray() const noexcept { return BLPtrOps::offset<const UInt16>(this, 4); }
    BL_INLINE const SubstLookupRecord* substArray(size_t glyphCount_) const noexcept { return BLPtrOps::offset<const SubstLookupRecord>(this, 4u + glyphCount_ * 2u - 2u); }
  };
  typedef SubRule SubClassRule;

  typedef Array16<UInt16> SubRuleSet;
  typedef Array16<UInt16> SubClassSet;

  struct ContextSubst1 : public LookupHeaderWithCoverage {
    Array16<Offset16> subRuleSetOffsets;
  };

  struct ContextSubst2 : public LookupHeaderWithCoverage {
    Offset16 classDefOffset;
    Array16<Offset16> subRuleSetOffsets;
  };

  struct ContextSubst3 : public LookupHeader {
    UInt16 glyphCount;
    UInt16 substCount;
    /*
    Offset16 coverageOffsetArray[glyphCount];
    SubstLookupRecord substArray[substCount];
    */

    BL_INLINE const UInt16* coverageOffsetArray() const noexcept { return BLPtrOps::offset<const UInt16>(this, 6); }
    BL_INLINE const SubstLookupRecord* substArray(size_t glyphCount_) const noexcept { return BLPtrOps::offset<const SubstLookupRecord>(this, 6u + glyphCount_ * 2u); }
  };

  // Lookup Type 6 - ChainContextSubst
  // ---------------------------------

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

    BL_INLINE const UInt16* backtrackSequence() const noexcept { return BLPtrOps::offset<const UInt16>(this, 2); }
  };
  typedef ChainSubRule ChainSubClassRule;

  typedef Array16<UInt16> ChainSubRuleSet;
  typedef Array16<UInt16> ChainSubClassRuleSet;

  struct ChainContextSubst1 : public LookupHeaderWithCoverage {
    Array16<Offset16> offsets;
  };

  struct ChainContextSubst2 : public LookupHeaderWithCoverage {
    Offset16 backtrackClassDefOffset;
    Offset16 inputClassDefOffset;
    Offset16 lookaheadClassDefOffset;
    Array16<Offset16> chainSubClassSets;
  };

  struct ChainContextSubst3 : public LookupHeader {
    UInt16 backtrackGlyphCount;
    /*
    Offset16 backtrackCoverageOffsets[backtrackGlyphCount];
    UInt16 inputGlyphCount;
    Offset16 inputCoverageOffsets[inputGlyphCount - 1];
    UInt16 lookaheadGlyphCount;
    Offset16 lookaheadCoverageOffsets[lookaheadGlyphCount];
    UInt16 substCount;
    SubstLookupRecord substArray[substCount];
    */

    BL_INLINE const UInt16* backtrackCoverageOffsets() const noexcept { return BLPtrOps::offset<const UInt16>(this, 4); }
  };

  // Lookup Type 7 - Extension
  // -------------------------

  // Use `ExtensionLookup` to handle this lookup type.

  // Lookup Type 8 - ReverseChainSingleSubst
  // ---------------------------------------

  struct ReverseChainSingleSubst1 : public LookupHeaderWithCoverage {
    UInt16 backtrackGlyphCount;
    /*
    Offset16 backtrackCoverageOffsets[backtrackGlyphCount];
    UInt16 lookaheadGlyphCount;
    Offset16 lookaheadCoverageOffsets[lookaheadGlyphCount];
    UInt16 substGlyphCount;
    UInt16 substGlyphArray[substGlyphCount];
    */
  };
};

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

  // Anchor Table
  // ------------

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

  // Mark
  // ----

  struct Mark {
    UInt16 markClass;
    UInt16 markAnchorOffset;
  };

  // Lookup Type 1 - Single Adjustment
  // ---------------------------------

  struct SingleAdjustment1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kMinSize = 6 };

    UInt16 valueFormat;

    BL_INLINE const UInt16* valueRecords() const noexcept { return BLPtrOps::offset<const UInt16>(this, 6u); }
  };

  struct SingleAdjustment2 : public LookupHeaderWithCoverage {
    enum : uint32_t { kMinSize = 8 };

    UInt16 valueFormat;
    UInt16 valueCount;

    BL_INLINE const UInt16* valueRecords() const noexcept { return BLPtrOps::offset<const UInt16>(this, 8u); }
  };

  // Lookup Type 2 - Pair Adjustment
  // -------------------------------

  struct PairValueRecord {
    UInt16 secondGlyph;

    BL_INLINE const UInt16* valueRecords() const noexcept { return BLPtrOps::offset<const UInt16>(this, 2); }
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
    Offset16 entryAnchorOffset;
    Offset16 exitAnchorOffset;
  };

  struct CursiveAttachment1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kMinSize = 8 };

    Array16<EntryExit> entryExits;
  };

  // Lookup Type 4 - MarkToBase Attachment
  // -------------------------------------

  struct MarkToBaseAttachment1 : public LookupHeader {
    enum : uint32_t { kMinSize = 12 };

    Offset16 markCoverageOffset;
    Offset16 baseCoverageOffset;
    UInt16 markClassCount;
    Offset16 markArrayOffset;
    Offset16 baseArrayOffset;
  };

  // Lookup Type 5 - MarkToLigature Attachment
  // -----------------------------------------

  struct MarkToLigatureAttachment1 : public LookupHeader {
    enum : uint32_t { kMinSize = 12 };

    Offset16 markCoverageOffset;
    Offset16 ligatureCoverageOffset;
    UInt16 markClassCount;
    Offset16 markArrayOffset;
    Offset16 ligatureArrayOffset;
  };

  // Lookup Type 6 - MarkToMark Attachment
  // -------------------------------------

  struct MarkToMarkAttachment1 : public LookupHeader {
    enum : uint32_t { kMinSize = 12 };

    Offset16 mark1CoverageOffset;
    Offset16 mark2CoverageOffset;
    UInt16 markClassCount;
    Offset16 mark1ArrayOffset;
    Offset16 mark2ArrayOffset;
  };

  // Lookup Type 7 - Context Positioning
  // -----------------------------------

  struct ContextPositioning1 : public LookupHeaderWithCoverage {
    enum : uint32_t { kMinSize = 6 };

    UInt16 posRuleSetCount;
  };

  struct ContextPositioning2 : public LookupHeaderWithCoverage {
    enum : uint32_t { kMinSize = 8 };

    Offset16 classDefOffset;
    Array16<UInt16> posClassSets;
  };

  struct ContextPositioning3 : public LookupHeader {
    enum : uint32_t { kMinSize = 6 };

    UInt16 glyphCount;
    UInt16 posCount;
  };

  // Lookup Type 8 - Chained Contextual Positioning
  // ----------------------------------------------

  // TODO: [OPENTYPE GPOS]

  // Lookup Type 9 - Extension
  // -------------------------

  // Use `ExtensionLookup` to handle this lookup type.
};

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

//! Data stored in `OTFaceImpl` related to OpenType advanced layout features.
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

    BL_INLINE void reset(uint32_t format_, uint32_t offset_) noexcept {
      this->format = format_ & 0xFu;
      this->offset = offset_ & 0x0FFFFFFFu;
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

namespace LayoutImpl {
BLResult init(OTFaceImpl* faceI, const BLFontData* fontData) noexcept;
} // {LayoutImpl}

} // {BLOpenType}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_OTLAYOUT_P_H_INCLUDED
