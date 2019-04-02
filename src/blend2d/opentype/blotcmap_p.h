// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_OPENTYPE_BLOTCMAP_P_H
#define BLEND2D_OPENTYPE_BLOTCMAP_P_H

#include "../opentype/blotdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_opentype
//! \{

namespace BLOpenType {

// ============================================================================
// [BLOpenType::CMapTable]
// ============================================================================

//! OpenType 'cmap' table.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/cmap
//!   - https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6cmap.html
//!
//! Some names inside this table do not match 1:1 specifications of Apple and
//! MS as they diverge as well. In general the naming was normalized to be
//! consistent in the following:
//!   - `first` - First character or glyph included in the set.
//!   - `last`  - Last character or glyph included in the set.
//!   - `end`   - First character or glyph excluded from the set.
//!   - `count` - Count of something, specifies a range of [first, first + count).
struct CMapTable {
  // Header and one encoding record (just to read the header).
  enum : uint32_t { kMinSize = 4 + 8 };

  struct Encoding {
    UInt16 platformId;
    UInt16 encodingId;
    UInt32 offset;
  };

  struct Group {
    UInt32 first;
    UInt32 last;
    UInt32 glyphId;
  };

  struct Format0 {
    enum : uint32_t { kMinSize = 262 };

    UInt16 format;
    UInt16 length;
    UInt16 language;
    UInt8 glyphIdArray[256];
  };

  struct Format2 {
    enum : uint32_t { kMinSize = 518 };

    struct SubHeader {
      UInt16 firstCode;
      UInt16 entryCount;
      Int16 idDelta;
      UInt16 idRangeOffset;
    };

    UInt16 format;
    UInt16 length;
    UInt16 language;
    UInt16 glyphIndexArray[256];
    /*
    SubHeader subHeaderArray[numSub];
    UInt16 glyphIdArray[];
    */

    BL_INLINE const SubHeader* subHeaderArray() const noexcept { return blOffsetPtr<const SubHeader>(this, sizeof(Format2)); }
    BL_INLINE const UInt16* glyphIdArray(size_t nSub) const noexcept { return blOffsetPtr<const UInt16>(this, sizeof(Format2) + nSub * sizeof(SubHeader)); }
  };

  struct Format4 {
    enum : uint32_t { kMinSize = 24 };

    UInt16 format;
    UInt16 length;
    UInt16 macLanguageCode;
    UInt16 numSegX2;
    UInt16 searchRange;
    UInt16 entrySelector;
    UInt16 rangeShift;
    /*
    UInt16 lastCharArray[numSegs];
    UInt16 pad;
    UInt16 firstCharArray[numSegs];
    Int16 idDeltaArray[numSegs];
    UInt16 idOffsetArray[numSegs];
    UInt16 glyphIdArray[];
    */

    BL_INLINE const UInt16* lastCharArray() const noexcept { return blOffsetPtr<const UInt16>(this, sizeof(*this)); }
    BL_INLINE const UInt16* firstCharArray(size_t numSeg) const noexcept { return blOffsetPtr<const UInt16>(this, sizeof(Format4) + 2u + numSeg * 2u); }
    BL_INLINE const UInt16* idDeltaArray(size_t numSeg) const noexcept { return blOffsetPtr<const UInt16>(this, sizeof(Format4) + 2u + numSeg * 4u); }
    BL_INLINE const UInt16* idOffsetArray(size_t numSeg) const noexcept { return blOffsetPtr<const UInt16>(this, sizeof(Format4) + 2u + numSeg * 6u); }
    BL_INLINE const UInt16* glyphIdArray(size_t numSeg) const noexcept { return blOffsetPtr<const UInt16>(this, sizeof(Format4) + 2u + numSeg * 8u); }
  };

  struct Format6 {
    enum : uint32_t { kMinSize = 12 };

    UInt16 format;
    UInt16 length;
    UInt16 language;
    UInt16 first;
    UInt16 count;
    /*
    UInt16 glyphIdArray[count];
    */

    BL_INLINE const UInt16* glyphIdArray() const noexcept { return blOffsetPtr<const UInt16>(this, sizeof(Format6)); }
  };

  //! This format is dead and it's not supported by Blend2D [only defined for reference].
  struct Format8 {
    enum : uint32_t { kMinSize = 16 + 8192 };

    UInt16 format;
    UInt16 reserved;
    UInt32 length;
    UInt32 language;
    UInt8 is32[8192];
    Array32<Group> groups;
  };

  struct Format10 {
    enum : uint32_t { kMinSize = 20 };

    UInt16 format;
    UInt16 reserved;
    UInt32 length;
    UInt32 language;
    UInt32 first;
    Array32<UInt16> glyphIds;
  };

  struct Format12_13 {
    enum : uint32_t { kMinSize = 16 };

    UInt16 format;
    UInt16 reserved;
    UInt32 length;
    UInt32 language;
    Array32<Group> groups;
  };

  struct Format14 {
    enum : uint32_t { kMinSize = 10 };

    struct VarSelector {
      UInt24 varSelector;
      UInt32 defaultUVSOffset;
      UInt32 nonDefaultUVSOffset;
    };

    struct UnicodeRange {
      UInt24 startUnicodeValue;
      UInt8 additionalCount;
    };

    struct UVSMapping {
      UInt24 unicodeValue;
      UInt16 glyphId;
    };

    struct DefaultUVS {
      Array32<UnicodeRange> ranges;
    };

    struct NonDefaultUVS {
      Array32<UVSMapping> mappings;
    };

    UInt16 format;
    UInt32 length;
    Array32<VarSelector> varSelectors;
  };

  UInt16 version;
  Array16<Encoding> encodings;
};

// ============================================================================
// [BLOpenType::CMapEncoding]
// ============================================================================

struct CMapEncoding {
  //! Offset to get the sub-table of this encoding.
  uint32_t offset;
  //! Count of entries in that sub-table (possibly corrected).
  uint32_t entryCount;

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
};

// ============================================================================
// [BLOpenType::CMapData]
// ============================================================================

//! Character to glyph mapping data for making it easier to use `CMapTable`.
struct CMapData {
  //! CMap table.
  BLFontTable cmapTable;
  //! CMap encoding [selected].
  CMapEncoding encoding;

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
};

// ============================================================================
// [BLOpenType::CMapImpl]
// ============================================================================

namespace CMapImpl {

//! Validate a CMapTable::Encoding subtable of any format at `subTableOffset`.
//! On success a valid `CMapEncoding` is written to `encodingOut`, otherwise
//! an error is returned and `encodingOut` is kept unchanged.
BL_HIDDEN BLResult validateSubTable(BLFontTable cmapTable, uint32_t subTableOffset, uint32_t& formatOut, CMapEncoding& encodingOut) noexcept;

//! Find the best encoding in the provided 'cmap' and store this information
//! into the given `faceI` instance. The function will return `BL_SUCCESS` even
//! if there is no encoding to be used, however, in such case the character to
//! glyph mapping feature will not be available to the users of this font-face.
BL_HIDDEN BLResult init(BLOTFaceImpl* faceI, const BLFontData* fontData) noexcept;

} // {CMapImpl}

} // {BLOpenType}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_BLOTCMAP_P_H
