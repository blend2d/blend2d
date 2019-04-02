// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_OPENTYPE_BLOTCORE_P_H
#define BLEND2D_OPENTYPE_BLOTCORE_P_H

#include "../opentype/blotdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_opentype
//! \{

namespace BLOpenType {

// ============================================================================
// [BLOpenType::SFNTHeader]
// ============================================================================

//! OpenType 'SFNT' header.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/font-file
struct SFNTHeader {
  enum : uint32_t { kMinSize = 12 };

  enum VersionTag : uint32_t {
    kVersionTagOpenType   = BL_MAKE_TAG('O', 'T', 'T', 'O'),
    kVersionTagTrueTypeA  = BL_MAKE_TAG( 0,   1 ,  0 ,  0 ),
    kVersionTagTrueTypeB  = BL_MAKE_TAG('t', 'r', 'u', 'e'),
    kVersionTagType1      = BL_MAKE_TAG('t', 'y', 'p', '1')
  };

  struct TableRecord {
    UInt32 tag;
    CheckSum checkSum;
    UInt32 offset;
    UInt32 length;
  };

  UInt32 versionTag;
  UInt16 numTables;
  UInt16 searchRange;
  UInt16 entrySelector;
  UInt16 rangeShift;

  BL_INLINE const TableRecord* tableRecords() const noexcept { return blOffsetPtr<const TableRecord>(this, sizeof(SFNTHeader)); }
};

// ============================================================================
// [BLOpenType::TTCFHeader]
// ============================================================================

//! OpenType 'TTCF' header.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/font-file
struct TTCFHeader {
  enum : uint32_t { kMinSize = 12 };
  enum : uint32_t { kMaxFonts = 65536 };

  BL_INLINE size_t calcSize(uint32_t numFonts) const noexcept {
    uint32_t headerSize = uint32_t(sizeof(TTCFHeader));

    if (numFonts > kMaxFonts)
      return 0;

    if (version() >= 0x00020000u)
      headerSize += 12;

    return headerSize + numFonts * 4;
  }

  // Version 1.
  UInt32 ttcTag;
  F16x16 version;
  Array32<UInt32> fonts;

  /*
  // Version 2.
  UInt32 dsigTag;
  UInt32 dsigLength;
  UInt32 dsigOffset;
  */
};

// ============================================================================
// [BLOpenType::HeadTable]
// ============================================================================

//! OpenType 'head' table.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/head
//!   - https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6head.html
struct HeadTable {
  enum : uint32_t { kMinSize = 54 };

  enum : uint32_t {
    kCheckSumAdjustment      = BL_MAKE_TAG(0xB1, 0xB0, 0xAF, 0xBA),
    kMagicNumber             = BL_MAKE_TAG(0x5F, 0x0F, 0x3C, 0xF5)
  };

  enum Flags : uint16_t {
    kFlagBaselineYEquals0    = 0x0001u,
    kFlagLSBPointXEquals0    = 0x0002u,
    kFlagInstDependOnPtSize  = 0x0004u,
    kFlagForcePPEMToInteger  = 0x0008u,
    kFlagInstMayAlterAW      = 0x0010u,
    kFlagLossLessData        = 0x0800u,
    kFlagConvertedFont       = 0x1000u,
    kFlagClearTypeOptimized  = 0x2000u,
    kFlagLastResortFont      = 0x4000u
  };

  enum MacStyle : uint16_t {
    kMacStyleBold            = 0x0001u,
    kMacStyleItalic          = 0x0002u,
    kMacStyleUnderline       = 0x0004u,
    kMacStyleOutline         = 0x0008u,
    kMacStyleShadow          = 0x0010u,
    kMacStyleCondensed       = 0x0020u,
    kMacStyleExtended        = 0x0040u,
    kMacStyleReservedBits    = 0xFF70u
  };

  enum IndexToLocFormat : uint16_t {
    kIndexToLocUInt16        = 0,
    kIndexToLocUInt32        = 1
  };

  F16x16 version;
  F16x16 revision;

  UInt32 checkSumAdjustment;
  UInt32 magicNumber;
  UInt16 flags;
  UInt16 unitsPerEm;

  DateTime created;
  DateTime modified;

  Int16 xMin;
  Int16 yMin;
  Int16 xMax;
  Int16 yMax;

  UInt16 macStyle;
  UInt16 lowestRecPPEM;

  Int16 fontDirectionHint;
  Int16 indexToLocFormat;
  Int16 glyphDataFormat;
};

// ============================================================================
// [BLOpenType::MaxPTable]
// ============================================================================

//! OpenType 'maxp' table.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/maxp
//!   - https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6maxp.html
struct MaxPTable {
  enum : uint32_t { kMinSize = 6 };

  // V0.5 - Must be used with CFF Glyphs (OpenType).
  struct V0_5 {
    F16x16 version;
    UInt16 glyphCount;
  };

  // V1.0 - Must be used with TT Glyphs (TrueType).
  struct V1_0 : public V0_5 {
    UInt16 maxPoints;
    UInt16 maxContours;
    UInt16 maxComponentPoints;
    UInt16 maxComponentContours;
    UInt16 maxZones;
    UInt16 maxTwilightPoints;
    UInt16 maxStorage;
    UInt16 maxFunctionDefs;
    UInt16 maxInstructionDefs;
    UInt16 maxStackElements;
    UInt16 maxSizeOfInstructions;
    UInt16 maxComponentElements;
    UInt16 maxComponentDepth;
  };

  V0_5 header;

  BL_INLINE const V0_5* v0_5() const noexcept { return blOffsetPtr<const V0_5>(this, 0); }
  BL_INLINE const V1_0* v1_0() const noexcept { return blOffsetPtr<const V1_0>(this, 0); }
};

// ============================================================================
// [BLOpenType::OS2Table]
// ============================================================================

//! OpenType 'OS/2' table.
//!
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/os2
//!   - https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6OS2.html
struct OS2Table {
  enum : uint32_t { kMinSize = 68 };

  //! OS/2 selection flags used by `OS2::selectionFlags` field.
  enum SelectionFlags : uint32_t {
    kSelectionItalic         = 0x0001u,
    kSelectionUnderscore     = 0x0002u,
    kSelectionNegative       = 0x0004u,
    kSelectionOutlined       = 0x0008u,
    kSelectionStrikeout      = 0x0010u,
    kSelectionBold           = 0x0020u,
    kSelectionRegular        = 0x0040u,
    kSelectionUseTypoMetrics = 0x0080u,
    kSelectionWWS            = 0x0100u,
    kSelectionOblique        = 0x0200u
  };

  struct V0A {
    enum : uint32_t { kMinSize = 68 };

    UInt16 version;
    Int16 xAverateWidth;
    UInt16 weightClass;
    UInt16 widthClass;
    UInt16 embeddingFlags;
    Int16 ySubscriptXSize;
    Int16 ySubscriptYSize;
    Int16 ySubscriptXOffset;
    Int16 ySubscriptYOffset;
    Int16 ySuperscriptXSize;
    Int16 ySuperscriptYSize;
    Int16 ySuperscriptXOffset;
    Int16 ySuperscriptYOffset;
    Int16 yStrikeoutSize;
    Int16 yStrikeoutPosition;
    Int16 familyClass;
    UInt8 panose[10];
    UInt32 unicodeCoverage[4];
    UInt8 vendorId[4];
    UInt16 selectionFlags;
    UInt16 firstChar;
    UInt16 lastChar;
  };

  struct V0B : public V0A {
    enum : uint32_t { kMinSize = 78 };

    Int16 typoAscender;
    Int16 typoDescender;
    Int16 typoLineGap;
    UInt16 winAscent;
    UInt16 winDescent;
  };

  struct V1 : public V0B {
    enum : uint32_t { kMinSize = 86 };

    UInt32 codePageRange[2];
  };

  struct V2 : public V1 {
    enum : uint32_t { kMinSize = 96 };

    Int16 xHeight;
    Int16 capHeight;
    UInt16 defaultChar;
    UInt16 breakChar;
    UInt16 maxContext;
  };

  struct V5 : public V2 {
    enum : uint32_t { kMinSize = 100 };

    UInt16 lowerOpticalPointSize;
    UInt16 upperOpticalPointSize;
  };

  V0A header;

  BL_INLINE const V0A* v0a() const noexcept { return blOffsetPtr<const V0A>(this, 0); }
  BL_INLINE const V0B* v0b() const noexcept { return blOffsetPtr<const V0B>(this, 0); }
  BL_INLINE const V1* v1() const noexcept { return blOffsetPtr<const V1>(this, 0); }
  BL_INLINE const V2* v2() const noexcept { return blOffsetPtr<const V2>(this, 0); }
  BL_INLINE const V5* v5() const noexcept { return blOffsetPtr<const V5>(this, 0); }
};

// ============================================================================
// [BLOpenType::PostTable]
// ============================================================================

//! OpenType 'post' table.
//!
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/post
//!   - https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6Post.html
struct PostTable {
  enum : uint32_t { kMinSize = 32 };

  F16x16 version;
  F16x16 italicAngle;
  Int16 underlinePosition;
  Int16 underlineThickness;
  UInt32 isFixedPitch;
  UInt32 minMemType42;
  UInt32 maxMemType42;
  UInt32 minMemType1;
  UInt32 maxMemType1;
};

// ============================================================================
// [BLOpenType::CoreImpl]
// ============================================================================

namespace CoreImpl {
  BLResult init(BLOTFaceImpl* faceI, const BLFontData* fontData) noexcept;
} // {CoreImpl}

} // {BLOpenType}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_BLOTCORE_P_H
