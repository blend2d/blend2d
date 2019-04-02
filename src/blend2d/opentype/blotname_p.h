// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_OPENTYPE_BLOTNAME_P_H
#define BLEND2D_OPENTYPE_BLOTNAME_P_H

#include "../opentype/blotdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_opentype
//! \{

namespace BLOpenType {

// ============================================================================
// [BLOpenType::NameTable]
// ============================================================================

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
    UInt16 offset;
  };

  struct LangTagRecord {
    UInt16 length;
    UInt16 offset;
  };

  UInt16 format;
  UInt16 recordCount;
  UInt16 stringOffset;
  /*
  NameRecord nameRecords[count];
  UInt16 langTagCount;
  LangTagRecord langTagRecords[langTagCount];
  */

  BL_INLINE bool hasLangTags() const noexcept { return format() >= 1; }

  //! The name records where count is the number of records.
  BL_INLINE const NameRecord* nameRecords() const noexcept {
    return blOffsetPtr<const NameRecord>(this, 6);
  }

  BL_INLINE uint16_t langTagCount(size_t recordCount) const noexcept {
    return blOffsetPtr<const UInt16>(this, 6 + recordCount * sizeof(NameRecord))->value();
  }

  BL_INLINE const LangTagRecord* langTagRecords(size_t recordCount) const noexcept {
    return blOffsetPtr<const LangTagRecord>(this, 6 + recordCount * sizeof(NameRecord) + 2);
  }
};

// ============================================================================
// [BLOpenType::NameImpl]
// ============================================================================

namespace NameImpl {
  BL_HIDDEN BLResult init(BLOTFaceImpl* faceI, const BLFontData* fontData) noexcept;
} // {NameImpl}

} // {BLOpenType}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_BLOTNAME_P_H
