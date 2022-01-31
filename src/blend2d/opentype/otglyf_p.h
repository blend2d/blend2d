// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_OPENTYPE_OTGLYF_P_H_INCLUDED
#define BLEND2D_OPENTYPE_OTGLYF_P_H_INCLUDED

#include "../font_p.h"
#include "../matrix_p.h"
#include "../path_p.h"
#include "../opentype/otdefs_p.h"
#include "../support/ptrops_p.h"
#include "../support/scopedbuffer_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_opentype_impl
//! \{

namespace BLOpenType {

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

  BL_INLINE const Offset16* offsetArray16() const noexcept { return BLPtrOps::offset<const Offset16>(this, 0); }
  BL_INLINE const Offset32* offsetArray32() const noexcept { return BLPtrOps::offset<const Offset32>(this, 0); }
};

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

      // We internally only use flags within this mask.
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

    const Simple* simple() const noexcept { return BLPtrOps::offset<const Simple>(this, sizeof(GlyphData)); }
    const Compound* compound() const noexcept { return BLPtrOps::offset<const Compound>(this, sizeof(GlyphData)); }
  };

  /*
  GlyphData glyphData[...] // Indexed by LOCA.
  */
};

struct GlyfData {
  //! Content of 'glyf' table.
  BLFontTable glyfTable;
  //! Content of 'loca' table.
  BLFontTable locaTable;
};

namespace {

// Used by getGlyphOutline() implementation.
struct CompoundEntry {
  enum : uint32_t { kMaxLevel = 16 };

  const uint8_t* gPtr;
  size_t remainingSize;
  uint32_t compoundFlags;
  BLMatrix2D matrix;
};

} // {anonymous}

namespace GlyfImpl {

BL_HIDDEN extern const BLLookupTable<uint32_t, ((GlyfTable::Simple::kImportantFlagsMask + 1) >> 1)> vertexSizeTable;

#ifdef BL_BUILD_OPT_AVX2
BL_HIDDEN BLResult BL_CDECL getGlyphOutlines_AVX2(
  const BLFontFaceImpl* faceI_,
  uint32_t glyphId,
  const BLMatrix2D* matrix,
  BLPath* out,
  size_t* contourCountOut,
  BLScopedBuffer* tmpBuffer) noexcept;
#endif

#ifdef BL_BUILD_OPT_SSE4_2
BL_HIDDEN BLResult BL_CDECL getGlyphOutlines_SSE4_2(
  const BLFontFaceImpl* faceI_,
  uint32_t glyphId,
  const BLMatrix2D* matrix,
  BLPath* out,
  size_t* contourCountOut,
  BLScopedBuffer* tmpBuffer) noexcept;
#endif

BLResult init(OTFaceImpl* faceI, BLFontTable glyfTable, BLFontTable locaTable) noexcept;

} // {GlyfImpl}

} // {BLOpenType}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_OTGLYF_P_H_INCLUDED
