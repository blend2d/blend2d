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

#ifndef BLEND2D_OPENTYPE_OTGLYF_P_H_INCLUDED
#define BLEND2D_OPENTYPE_OTGLYF_P_H_INCLUDED

#include "../font_p.h"
#include "../matrix_p.h"
#include "../path_p.h"
#include "../opentype/otdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_opentype
//! \{

namespace BLOpenType {

// ============================================================================
// [BLOpenType::LocaTable]
// ============================================================================

//! OpenType 'loca' table.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/loca
//!   - https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6loca.html
struct LocaTable {
  // Minimum size would be 2 records (4 bytes) if the font has only 1 glyph and uses 16-bit LOCA.
  enum : uint32_t { kMinSize = 4 };

  /*
  union {
    Offset16 offsetArray16[...];
    Offset32 offsetArray32[...];
  };
  */

  BL_INLINE const Offset16* offsetArray16() const noexcept { return blOffsetPtr<const Offset16>(this, 0); }
  BL_INLINE const Offset32* offsetArray32() const noexcept { return blOffsetPtr<const Offset32>(this, 0); }
};

// ============================================================================
// [BLOpenType::GlyfTable]
// ============================================================================

//! OpenType 'glyf' table.
//!
//! External Resources:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/glyf
//!   - https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6glyf.html
struct GlyfTable {
  enum : uint32_t { kMinSize = 10 };

  struct Simple {
    enum Flags : uint8_t {
      kOnCurvePoint             = 0x01u,
      kXIsByte                  = 0x02u,
      kYIsByte                  = 0x04u,
      kRepeatFlag               = 0x08u,
      kXIsSameOrXByteIsPositive = 0x10u,
      kYIsSameOrYByteIsPositive = 0x20u,

      // We internally only keep flags within this mask.
      kImportantFlagsMask       = 0x3Fu
    };

    /*
    UInt16 endPtsOfContours[numberOfContours];
    UInt16 instructionLength;
    UInt8 instructions[instructionLength];
    UInt8 flags[...];
    UInt8/UInt16 xCoordinates[...];
    UInt8/UInt16 yCoordinates[...];
    */
  };

  struct Compound {
    enum Flags : uint16_t {
      kArgsAreWords             = 0x0001u,
      kArgsAreXYValues          = 0x0002u,
      kRoundXYToGrid            = 0x0004u,
      kWeHaveScale              = 0x0008u,
      kMoreComponents           = 0x0020u,
      kWeHaveScaleXY            = 0x0040u,
      kWeHave2x2                = 0x0080u,
      kWeHaveInstructions       = 0x0100u,
      kUseMyMetrics             = 0x0200u,
      kOverlappedCompound       = 0x0400u,
      kScaledComponentOffset    = 0x0800u,
      kUnscaledComponentOffset  = 0x1000u,

      kAnyCompoundScale         = kWeHaveScale
                                | kWeHaveScaleXY
                                | kWeHave2x2,

      kAnyCompoundOffset        = kScaledComponentOffset
                                | kUnscaledComponentOffset
    };

    UInt16 flags;
    UInt16 glyphId;
    /*
    Var arguments[...];
    Var transformations[...];
    */
  };

  struct GlyphData {
    Int16 numberOfContours;
    FWord xMin;
    FWord yMin;
    FWord xMax;
    FWord yMax;

    const Simple* simple() const noexcept { return blOffsetPtr<const Simple>(this, sizeof(GlyphData)); }
    const Compound* compound() const noexcept { return blOffsetPtr<const Compound>(this, sizeof(GlyphData)); }
  };

  /*
  GlyphData glyphData[...] // Indexed by LOCA.
  */
};

// ============================================================================
// [BLOpenType::GlyfData]
// ============================================================================

struct GlyfData {
  //! Content of 'glyf' table.
  BLFontTable glyfTable;
  //! Content of 'loca' table.
  BLFontTable locaTable;
};

// ============================================================================
// [BLOpenType::GlyfImpl]
// ============================================================================

namespace GlyfImpl {
  BLResult init(BLOTFaceImpl* faceI, BLFontTable glyfTable, BLFontTable locaTable) noexcept;
} // {GlyfImpl}

} // {BLOpenType}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_OTGLYF_P_H_INCLUDED
