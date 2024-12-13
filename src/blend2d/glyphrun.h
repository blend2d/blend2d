// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_GLYPHRUN_H_INCLUDED
#define BLEND2D_GLYPHRUN_H_INCLUDED

#include "fontdefs.h"

//! \addtogroup bl_text
//! \{

//! \name BLGlyphRun - Constants
//! \{

//! Flags used by \ref BLGlyphRun.
BL_DEFINE_ENUM(BLGlyphRunFlags) {
  //! No flags.
  BL_GLYPH_RUN_NO_FLAGS = 0u,
  //! Glyph-run contains UCS-4 string and not glyphs (glyph-buffer only).
  BL_GLYPH_RUN_FLAG_UCS4_CONTENT = 0x10000000u,
  //! Glyph-run was created from text that was not a valid unicode.
  BL_GLYPH_RUN_FLAG_INVALID_TEXT = 0x20000000u,
  //! Not the whole text was mapped to glyphs (contains undefined glyphs).
  BL_GLYPH_RUN_FLAG_UNDEFINED_GLYPHS = 0x40000000u,
  //! Encountered invalid font data during text / glyph processing.
  BL_GLYPH_RUN_FLAG_INVALID_FONT_DATA = 0x80000000u

  BL_FORCE_ENUM_UINT32(BL_GLYPH_RUN_FLAG)
};

//! Placement of glyphs stored in a \ref BLGlyphRun.
BL_DEFINE_ENUM(BLGlyphPlacementType) {
  //! No placement (custom handling by \ref BLPathSinkFunc).
  BL_GLYPH_PLACEMENT_TYPE_NONE = 0,
  //! Each glyph has a BLGlyphPlacement (advance + offset).
  BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET = 1,
  //! Each glyph has a BLPoint offset in design-space units.
  BL_GLYPH_PLACEMENT_TYPE_DESIGN_UNITS = 2,
  //! Each glyph has a BLPoint offset in user-space units.
  BL_GLYPH_PLACEMENT_TYPE_USER_UNITS = 3,
  //! Each glyph has a BLPoint offset in absolute units.
  BL_GLYPH_PLACEMENT_TYPE_ABSOLUTE_UNITS = 4,

  //! Maximum value of `BLGlyphPlacementType`.
  BL_GLYPH_PLACEMENT_TYPE_MAX_VALUE = 4

  BL_FORCE_ENUM_UINT32(BL_GLYPH_PLACEMENT_TYPE)
};

//! \}

//! \name BLGlyphRun - Structs
//! \{

//! BLGlyphRun describes a set of consecutive glyphs and their placements.
//!
//! BLGlyphRun should only be used to pass glyph IDs and their placements to the rendering context. The purpose of
//! BLGlyphRun is to allow rendering glyphs, which could be shaped by various shaping engines (Blend2D, Harfbuzz, etc).
//!
//! BLGlyphRun allows to render glyphs that are stored as uint32_t[] array or part of a bigger structure (for example
//! `hb_glyph_info_t` used by HarfBuzz). Glyph placements at the moment use Blend2D's \ref BLGlyphPlacement or \ref
//! BLPoint, but it's possible to extend the data type in the future.
//!
//! See `BLGlyphRunPlacement` for placement modes provided by Blend2D.
struct BLGlyphRun {
  //! \name Members
  //! \{

  //! Glyph id data (abstract, incremented by `glyphAdvance`).
  void* glyphData;
  //! Glyph placement data (abstract, incremented by `placementAdvance`).
  void* placementData;
  //! Size of the glyph-run in glyph units.
  size_t size;
  //! Reserved for future use, muse be zero.
  uint8_t reserved;
  //! Type of placement, see \ref BLGlyphPlacementType.
  uint8_t placementType;
  //! Advance of `glyphData` array.
  int8_t glyphAdvance;
  //! Advance of `placementData` array.
  int8_t placementAdvance;
  //! Glyph-run flags.
  uint32_t flags;

  //! \}

#ifdef __cplusplus
  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG void reset() noexcept { *this = BLGlyphRun{}; }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG bool empty() const noexcept { return size == 0; }

  template<typename T>
  BL_INLINE_NODEBUG T* glyphDataAs() const noexcept { return static_cast<T*>(glyphData); }

  template<typename T>
  BL_INLINE_NODEBUG T* placementDataAs() const noexcept { return static_cast<T*>(placementData); }

  BL_INLINE_NODEBUG void setGlyphData(const uint32_t* glyphData) noexcept { setGlyphData(glyphData, intptr_t(sizeof(uint32_t))); }

  BL_INLINE_NODEBUG void setGlyphData(const void* data, intptr_t advance) noexcept {
    this->glyphData = const_cast<void*>(data);
    this->glyphAdvance = int8_t(advance);
  }

  BL_INLINE_NODEBUG void resetGlyphIdData() noexcept {
    this->glyphData = nullptr;
    this->glyphAdvance = 0;
  }

  template<typename T>
  BL_INLINE_NODEBUG void setPlacementData(const T* data) noexcept {
    setPlacementData(data, sizeof(T));
  }

  BL_INLINE_NODEBUG void setPlacementData(const void* data, intptr_t advance) noexcept {
    this->placementData = const_cast<void*>(data);
    this->placementAdvance = int8_t(advance);
  }

  BL_INLINE_NODEBUG void resetPlacementData() noexcept {
    this->placementData = nullptr;
    this->placementAdvance = 0;
  }

  //! \}
#endif
};

//! \}

//! \name BLGlyphRun - C++ API
//! \{

#ifdef __cplusplus
//! A helper to iterate over a `BLGlyphRun`.
//!
//! Takes into consideration glyph-id advance and glyph-offset advance.
//!
//! Example:
//!
//! ```
//! void inspectGlyphRun(const BLGlyphRun& glyphRun) noexcept {
//!   BLGlyphRunIterator it(glyphRun);
//!   if (it.hasOffsets()) {
//!     while (!it.atEnd()) {
//!       BLGlyphId glyphId = it.glyphId();
//!       BLPoint offset = it.placement();
//!
//!       // Do something with `glyphId` and `offset`.
//!
//!       it.advance();
//!     }
//!   }
//!   else {
//!     while (!it.atEnd()) {
//!       BLGlyphId glyphId = it.glyphId();
//!
//!       // Do something with `glyphId`.
//!
//!       it.advance();
//!     }
//!   }
//! }
//! ```
class BLGlyphRunIterator {
public:
  //! \name Members
  //! \{

  size_t index;
  size_t size;
  void* glyphData;
  void* placementData;
  intptr_t glyphAdvance;
  intptr_t placementAdvance;

  //! \}

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLGlyphRunIterator() noexcept { reset(); }
  BL_INLINE explicit BLGlyphRunIterator(const BLGlyphRun& glyphRun) noexcept { reset(glyphRun); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE void reset() noexcept {
    index = 0;
    size = 0;
    glyphData = nullptr;
    placementData = nullptr;
    glyphAdvance = 0;
    placementAdvance = 0;
  }

  BL_INLINE void reset(const BLGlyphRun& glyphRun) noexcept {
    index = 0;
    size = glyphRun.size;
    glyphData = glyphRun.glyphData;
    placementData = glyphRun.placementData;
    glyphAdvance = glyphRun.glyphAdvance;
    placementAdvance = glyphRun.placementAdvance;
  }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG bool empty() const noexcept { return size == 0; }
  BL_INLINE_NODEBUG bool atEnd() const noexcept { return index == size; }
  BL_INLINE_NODEBUG bool hasPlacement() const noexcept { return placementData != nullptr; }

  BL_INLINE_NODEBUG BLGlyphId glyphId() const noexcept { return *static_cast<const BLGlyphId*>(glyphData); }

  template<typename T>
  BL_INLINE_NODEBUG const T& placement() const noexcept { return *static_cast<const T*>(placementData); }

  //! \}

  //! \name Iterator Interface
  //! \{

  BL_INLINE void advance() noexcept {
    BL_ASSERT(!atEnd());

    index++;
    glyphData = static_cast<uint8_t*>(glyphData) + glyphAdvance;
    placementData = static_cast<uint8_t*>(placementData) + placementAdvance;
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_GLYPHRUN_H_INCLUDED
