// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_OPENTYPE_OTNAME_P_H_INCLUDED
#define BLEND2D_OPENTYPE_OTNAME_P_H_INCLUDED

#include "../opentype/otdefs_p.h"

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
    return blOffsetPtr<const NameRecord>(this, 6);
  }

  BL_INLINE uint16_t langTagCount(size_t recordCount_) const noexcept {
    return blOffsetPtr<const UInt16>(this, 6 + recordCount_ * sizeof(NameRecord))->value();
  }

  BL_INLINE const LangTagRecord* langTagRecords(size_t recordCount_) const noexcept {
    return blOffsetPtr<const LangTagRecord>(this, 6 + recordCount_ * sizeof(NameRecord) + 2);
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

#endif // BLEND2D_OPENTYPE_OTNAME_P_H_INCLUDED
