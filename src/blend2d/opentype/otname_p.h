// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_OPENTYPE_OTNAME_P_H_INCLUDED
#define BLEND2D_OPENTYPE_OTNAME_P_H_INCLUDED

#include "../opentype/otdefs_p.h"
#include "../support/ptrops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_opentype_impl
//! \{

namespace BLOpenType {

//! OpenType 'name' table.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/name
//!   - https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6name.html
struct NameTable {
  enum : uint32_t { kMinSize = 6 };

  struct NameRecord {
    UInt16 platformId;
    UInt16 specificId;
    UInt16 languageId;
    UInt16 nameId;
    UInt16 length;
    Offset16 offset;
  };

  struct LangTagRecord {
    UInt16 length;
    Offset16 offset;
  };

  UInt16 format;
  UInt16 recordCount;
  Offset16 stringOffset;
  /*
  NameRecord nameRecords[count];
  UInt16 langTagCount;
  LangTagRecord langTagRecords[langTagCount];
  */

  BL_INLINE bool hasLangTags() const noexcept { return format() >= 1; }

  //! The name records where count is the number of records.
  BL_INLINE const NameRecord* nameRecords() const noexcept {
    return BLPtrOps::offset<const NameRecord>(this, 6);
  }

  BL_INLINE uint16_t langTagCount(size_t recordCount_) const noexcept {
    return BLPtrOps::offset<const UInt16>(this, 6 + recordCount_ * sizeof(NameRecord))->value();
  }

  BL_INLINE const LangTagRecord* langTagRecords(size_t recordCount_) const noexcept {
    return BLPtrOps::offset<const LangTagRecord>(this, 6 + recordCount_ * sizeof(NameRecord) + 2);
  }
};

namespace NameImpl {
BL_HIDDEN BLResult init(OTFaceImpl* faceI, const BLFontData* fontData) noexcept;
} // {NameImpl}

} // {BLOpenType}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_OTNAME_P_H_INCLUDED
