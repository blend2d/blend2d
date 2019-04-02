// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_OPENTYPE_BLOTCFF_P_H
#define BLEND2D_OPENTYPE_BLOTCFF_P_H

#include "../blfont_p.h"
#include "../opentype/blotdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_opentype
//! \{

namespace BLOpenType {

// ============================================================================
// [BLOpenType::CFFTable]
// ============================================================================

//! OpenType 'CFF' or 'CFF2' table (Compact Font Format).
//!
//! The structure of CFF File looks like this:
//!   - Header
//!   - Name INDEX
//!   - TopDict INDEX
//!   - String INDEX
//!   - GSubR INDEX
//!   - Encodings
//!   - Charsets
//!   - FDSelect
//!   - CharStrings INDEX   <- [get offset from 'TopDict.CharStrings']
//!   - FontDict INDEX
//!   - PrivateDict         <- [get offset+size from 'TopDict.Private']
//!   - LSubR INDEX
//!   - Copyright and trademark notices
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/cff
//!   - http://wwwimages.adobe.com/www.adobe.com/content/dam/acom/en/devnet/font/pdfs/5176.CFF.pdf
//!   - http://wwwimages.adobe.com/www.adobe.com/content/dam/acom/en/devnet/font/pdfs/5177.Type2.pdf
//!
//! NOTE 1: The term `VarOffset` that is used inside CFF code means that the
//! offset size is variable and must be previously specified by an `offsetSize`
//! field.
//!
//! NOTE 2: Many enums inside this structure are just for reference purposes.
//! They would be useful if we want to implement support for RAW PostScript
//! fonts (CFF) that are not part of OpenType.
struct CFFTable {
  enum : uint32_t { kMinSize = 4 };
  enum : uint32_t { kOffsetAdjustment = 1 };

  enum CharsetId : uint32_t {
    kCharsetIdISOAdobe           = 0,
    kCharsetIdExpert             = 1,
    kCharsetIdExpertSubset       = 2
  };

  enum DictOp : uint32_t {
    // Escape.
    kEscapeDictOp                = 0x0C,

    // Top Dict Operator Entries.
    kDictOpTopVersion            = 0x0000,
    kDictOpTopNotice             = 0x0001,
    kDictOpTopFullName           = 0x0002,
    kDictOpTopFamilyName         = 0x0003,
    kDictOpTopWeight             = 0x0004,
    kDictOpTopFontBBox           = 0x0005,
    kDictOpTopUniqueId           = 0x000D,
    kDictOpTopXUID               = 0x000E,
    kDictOpTopCharset            = 0x000F, // Default 0.
    kDictOpTopEncoding           = 0x0010, // Default 0.
    kDictOpTopCharStrings        = 0x0011,
    kDictOpTopPrivate            = 0x0012,

    kDictOpTopCopyright          = 0x0C00,
    kDictOpTopIsFixedPitch       = 0x0C01, // Default 0 (false).
    kDictOpTopItalicAngle        = 0x0C02, // Default 0.
    kDictOpTopUnderlinePosition  = 0x0C03, // Default -100.
    kDictOpTopUnderlineThickness = 0x0C04, // Default 50.
    kDictOpTopPaintType          = 0x0C05, // Default 0.
    kDictOpTopCharstringType     = 0x0C06, // Default 2.
    kDictOpTopFontMatrix         = 0x0C07, // Default [0.001 0 0 0.001 0 0].
    kDictOpTopStrokeWidth        = 0x0C08, // Default 0.
    kDictOpTopSyntheticBase      = 0x0C14,
    kDictOpTopPostScript         = 0x0C15,
    kDictOpTopBaseFontName       = 0x0C16,
    kDictOpTopBaseFontBlend      = 0x0C17,

    // CIDFont Operator Extensions:
    kDictOpTopROS                = 0x0C1E,
    kDictOpTopCIDFontVersion     = 0x0C1F, // Default 0.
    kDictOpTopCIDFontRevision    = 0x0C20, // Default 0.
    kDictOpTopCIDFontType        = 0x0C21, // Default 0.
    kDictOpTopCIDCount           = 0x0C22, // Default 8720.
    kDictOpTopUIDBase            = 0x0C23,
    kDictOpTopFDArray            = 0x0C24,
    kDictOpTopFDSelect           = 0x0C25,
    kDictOpTopFontName           = 0x0C26,

    // Private Dict Operator Entries.
    kDictOpPrivBlueValues        = 0x0006,
    kDictOpPrivOtherBlues        = 0x0007,
    kDictOpPrivFamilyBlues       = 0x0008,
    kDictOpPrivFamilyOtherBlues  = 0x0009,
    kDictOpPrivStdHW             = 0x000A,
    kDictOpPrivStdVW             = 0x000B,
    kDictOpPrivSubrs             = 0x0013,
    kDictOpPrivDefaultWidthX     = 0x0014, // Default 0.
    kDictOpPrivNominalWidthX     = 0x0015, // Default 0.

    kDictOpPrivBlueScale         = 0x0C09, // Default 0.039625.
    kDictOpPrivBlueShift         = 0x0C0A, // Default 7.
    kDictOpPrivBlueFuzz          = 0x0C0B, // Default 1.
    kDictOpPrivStemSnapH         = 0x0C0C,
    kDictOpPrivStemSnapV         = 0x0C0D,
    kDictOpPrivForceBold         = 0x0C0E, // Default 0 (false).
    kDictOpPrivLanguageGroup     = 0x0C11, // Default 0.
    kDictOpPrivExpansionFactor   = 0x0C12, // Default 0.06.
    kDictOpPrivInitialRandomSeed = 0x0C13  // Default 0.
  };

  struct Header {
    UInt8 majorVersion;
    UInt8 minorVersion;
    UInt8 headerSize;
  };

  struct HeaderV1 : public Header { UInt8 offsetSize; };
  struct HeaderV2 : public Header { UInt16 topDictLength; };

  //! Index table (v1).
  struct IndexV1 {
    //! NOTE: An empty Index is represented by a `count` field with a 0 value
    //! and no additional fields, thus, the total size of bytes required by a
    //! zero index is 2.
    enum : uint32_t { kMinSize = 2 };

    UInt16 count;
    UInt8 offsetSize;
    /*
    Offset offsetArray[count + 1];
    UInt8 data[...];
    */

    BL_INLINE const uint8_t* offsetArray() const noexcept { return blOffsetPtr<const uint8_t>(this, 3); }
  };

  //! Index table (v2).
  struct IndexV2 {
    //! NOTE: An empty Index is represented by a `count` field with a 0 value
    //! and no additional fields, thus, the total size of bytes required by a
    //! zero index is 4.
    enum : uint32_t { kMinSize = 4 };

    UInt32 count;
    UInt8 offsetSize;
    /*
    Offset offsetArray[count + 1];
    UInt8 data[...];
    */

    BL_INLINE const uint8_t* offsetArray() const noexcept { return blOffsetPtr<const uint8_t>(this, 5); }
  };

  Header header;

  BL_INLINE const HeaderV1* headerV1() const noexcept { return blOffsetPtr<const HeaderV1>(this, 0); }
  BL_INLINE const HeaderV2* headerV2() const noexcept { return blOffsetPtr<const HeaderV2>(this, 0); }
};

// ============================================================================
// [BLOpenType::CFFData]
// ============================================================================

struct CFFData {
  //! CFF version.
  enum Version : uint32_t {
    kVersion1        = 0,
    kVersion2        = 1
  };

  //! CFF index id.
  enum IndexId : uint32_t {
    kIndexGSubR      = 0,
    kIndexLSubR      = 1,
    kIndexCharString = 2,
    kIndexCount      = 3
  };

  //! CFF index.
  struct IndexData {
    DataRange dataRange;
    uint32_t entryCount;
    uint8_t headerSize;
    uint8_t offsetSize;
    uint16_t bias;

    BL_INLINE void reset(const DataRange& dataRange, uint32_t headerSize, uint32_t offsetSize, uint32_t entryCount, uint16_t bias) noexcept {
      this->dataRange = dataRange;
      this->entryCount = entryCount;
      this->headerSize = uint8_t(headerSize);
      this->offsetSize = uint8_t(offsetSize);
      this->bias = bias;
    }

    //! Get an offset to the offsets data (array of offsets).
    BL_INLINE uint32_t offsetsOffset() const noexcept { return headerSize; }
    //! get a size of offset data (array of offsets) in bytes.
    BL_INLINE uint32_t offsetsSize() const noexcept { return (entryCount + 1) * offsetSize; }

    //! Get an offset to the payload data.
    BL_INLINE uint32_t payloadOffset() const noexcept { return offsetsOffset() + offsetsSize(); }
    //! Get a payload size in bytes.
    BL_INLINE uint32_t payloadSize() const noexcept { return dataRange.size - payloadOffset(); }
  };

  //! Content of 'CFF ' or 'CFF2' table.
  BLFontTableT<CFFTable> table;
  //! GSubR, LSubR, and CharString indexes.
  IndexData index[kIndexCount];
};

// ============================================================================
// [BLOpenType::CFFImpl]
// ============================================================================

namespace CFFImpl {
  BL_HIDDEN BLResult init(BLOTFaceImpl* faceI, BLFontTable fontTable, uint32_t cffVersion) noexcept;
} // {CFFImpl}

} // {BLOpenType}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_BLOTCFF_P_H
