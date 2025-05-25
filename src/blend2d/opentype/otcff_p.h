// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_OPENTYPE_OTCFF_P_H_INCLUDED
#define BLEND2D_OPENTYPE_OTCFF_P_H_INCLUDED

#include "../font_p.h"
#include "../opentype/otdefs_p.h"
#include "../support/ptrops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_opentype_impl
//! \{

namespace bl::OpenType {

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
//! NOTE 1: The term `VarOffset` that is used inside CFF code means that the offset size is variable and must
//!  be previously specified by an `offsetSize` field.
//!
//! NOTE 2: Many enums inside this structure are just for reference purposes. They would be useful if we want
//! to implement support for RAW PostScript fonts (CFF) that are not part of OpenType.
struct CFFTable {
  enum : uint32_t { kBaseSize = 4 };
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
    //! \note An empty Index is represented by a `count` field with a 0 value and no additional fields, thus,
    //! the total size of bytes required by a zero index is 2.
    enum : uint32_t { kBaseSize = 2 };

    UInt16 count;
    UInt8 offsetSize;
    /*
    Offset offsetArray[count + 1];
    UInt8 data[...];
    */

    BL_INLINE const uint8_t* offsetArray() const noexcept { return PtrOps::offset<const uint8_t>(this, 3); }
  };

  //! Index table (v2).
  struct IndexV2 {
    //! \note An empty Index is represented by a `count` field with a 0 value and no additional fields, thus,
    //! the total size of bytes required by a zero index is 4.
    enum : uint32_t { kBaseSize = 4 };

    UInt32 count;
    UInt8 offsetSize;
    /*
    Offset offsetArray[count + 1];
    UInt8 data[...];
    */

    BL_INLINE const uint8_t* offsetArray() const noexcept { return PtrOps::offset<const uint8_t>(this, 5); }
  };

  Header header;

  BL_INLINE const HeaderV1* headerV1() const noexcept { return PtrOps::offset<const HeaderV1>(this, 0); }
  BL_INLINE const HeaderV2* headerV2() const noexcept { return PtrOps::offset<const HeaderV2>(this, 0); }
};

//! CFF data stored in \ref OTFaceImpl.
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

    BL_INLINE void reset(const DataRange& dataRange_, uint32_t headerSize_, uint32_t offsetSize_, uint32_t entryCount_, uint16_t bias_) noexcept {
      this->dataRange = dataRange_;
      this->entryCount = entryCount_;
      this->headerSize = uint8_t(headerSize_);
      this->offsetSize = uint8_t(offsetSize_);
      this->bias = bias_;
    }

    //! Returns the offset to the offsets data (array of offsets).
    BL_INLINE uint32_t offsetsOffset() const noexcept { return headerSize; }
    //! Returns the size of offset data (array of offsets) in bytes.
    BL_INLINE uint32_t offsetsSize() const noexcept { return (entryCount + 1) * offsetSize; }

    //! Returns the offset to the payload data.
    BL_INLINE uint32_t payloadOffset() const noexcept { return offsetsOffset() + offsetsSize(); }
    //! Returns the payload size in bytes.
    BL_INLINE uint32_t payloadSize() const noexcept { return dataRange.size - payloadOffset(); }
  };

  //! Content of 'CFF ' or 'CFF2' table.
  Table<CFFTable> table;
  //! GSubR, LSubR, and CharString indexes.
  IndexData index[kIndexCount];
  //! Associates an FD (font dict) with a glyph by specifying an FD index for that glyph.
  uint32_t fdSelectOffset;
  //! Format of FDSelect data (0 or 3).
  uint8_t fdSelectFormat;
  uint8_t reserved[3];
};

namespace CFFImpl {

//! Reads a CFF floating point value as specified by the CFF specification. The format is binary, but it's just
//! a simplified text representation in the end.
//!
//! Each byte is divided into 2 nibbles (4 bits), which are accessed separately. Each nibble contains either a
//! decimal value (0..9), decimal point, or other instructions which meaning is described by `NibbleAbove9` enum.
BL_HIDDEN BLResult readFloat(const uint8_t* p, const uint8_t* pEnd, double& valueOut, size_t& valueSizeInBytes) noexcept;

// bl::OpenType::CFFImpl - DictEntry
// =================================

//! CFF dictionary entry.
struct DictEntry {
  enum : uint32_t { kValueCapacity = 48 };

  uint32_t op;
  uint32_t count;
  uint64_t fpMask;
  double values[kValueCapacity];

  BL_INLINE bool isFpValue(uint32_t index) const noexcept {
    return (fpMask & (uint64_t(1) << index)) != 0;
  }
};

//! CFF dictionary iterator.
class DictIterator {
public:
  const uint8_t* _dataPtr;
  const uint8_t* _dataEnd;

  BL_INLINE DictIterator() noexcept
    : _dataPtr(nullptr),
      _dataEnd(nullptr) {}

  BL_INLINE DictIterator(const uint8_t* data, size_t size) noexcept
    : _dataPtr(data),
      _dataEnd(data + size) {}

  BL_INLINE void reset(const uint8_t* data, size_t size) noexcept {
    _dataPtr = data;
    _dataEnd = data + size;
  }

  BL_INLINE bool hasNext() const noexcept {
    return _dataPtr != _dataEnd;
  }

  BLResult next(DictEntry& entry) noexcept;
};

BL_HIDDEN BLResult init(OTFaceImpl* faceI, OTFaceTables& tables, uint32_t cffVersion) noexcept;

} // {CFFImpl}
} // {bl::OpenType}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_OTCFF_P_H_INCLUDED
