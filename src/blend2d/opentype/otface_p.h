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

#ifndef BLEND2D_OPENTYPE_OTFACE_P_H_INCLUDED
#define BLEND2D_OPENTYPE_OTFACE_P_H_INCLUDED

#include "../opentype/otcff_p.h"
#include "../opentype/otcmap_p.h"
#include "../opentype/otdefs_p.h"
#include "../opentype/otglyf_p.h"
#include "../opentype/otkern_p.h"
#include "../opentype/otlayout_p.h"
#include "../opentype/otmetrics_p.h"
#include "../opentype/otname_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_opentype
//! \{

// ============================================================================
// [Constants]
// ============================================================================

enum BLOTFaceFlags : uint32_t {
  // Flags related to 'loca' table.
  BL_OT_FACE_FLAG_LOCA_OFFSET_16        = 0x00000002u, //!< Glyph offsets in 'loca' table use 16-bit offsets [must be 0x2].
  BL_OT_FACE_FLAG_LOCA_OFFSET_32        = 0x00000004u, //!< Glyph offsets in 'loca' table use 32-bit offsets [must be 0x4].

  // Flags related to 'GDEF' table.
  BL_OT_FACE_FLAG_GLYPH_CLASS_DEF       = 0x00000100u,
  BL_OT_FACE_FLAG_ATTACH_LIST           = 0x00000200u,
  BL_OT_FACE_FLAG_LIT_CARET_LIST        = 0x00000400u,
  BL_OT_FACE_FLAG_MARK_ATTACH_CLASS_DEF = 0x00000800u,
  BL_OT_FACE_FLAG_MARK_GLYPH_SETS_DEF   = 0x00001000u,
  BL_OT_FACE_FLAG_ITEM_VAR_STORE        = 0x00002000u,

  // Flags related to 'GSUB' table.
  BL_OT_FACE_FLAG_GSUB_SCRIPT_LIST      = 0x00010000u,
  BL_OT_FACE_FLAG_GSUB_FEATURE_LIST     = 0x00020000u,
  BL_OT_FACE_FLAG_GSUB_LOOKUP_LIST      = 0x00040000u,
  BL_OT_FACE_FLAG_GSUB_FVAR             = 0x00080000u,

  // Flags related to 'GPOS' table.
  BL_OT_FACE_FLAG_GPOS_SCRIPT_LIST      = 0x00100000u,
  BL_OT_FACE_FLAG_GPOS_FEATURE_LIST     = 0x00200000u,
  BL_OT_FACE_FLAG_GPOS_LOOKUP_LIST      = 0x00400000u,
  BL_OT_FACE_FLAG_GPOS_FVAR             = 0x00800000u
};

// ============================================================================
// [BLOTFaceImpl]
// ============================================================================

//! TrueType or OpenType font face.
//!
//! This class provides extra data required by TrueType / OpenType implementation.
//! It's currently the only implementation of `BLFontFaceImpl` available in Blend2D
//! and there will probably not be any other implementation as OpenType provides
//! a lot of features required to render text in general.
struct BLOTFaceImpl : public BLInternalFontFaceImpl {
  //! OpenType flags, see OTFlags.
  uint32_t otFlags;

  //! Character mapping format (stored here so we won't misalign `CMapData`.
  uint8_t cmapFormat;

  //! Reserved for future use.
  uint8_t reservedOpenType[3];

  //! Character to glyph mapping data.
  BLOpenType::CMapData cmap;
  //! Metrics data.
  BLOpenType::MetricsData metrics;

  //! Legacy kerning data - 'kern' table and related data.
  BLOpenType::KernData kern;
  //! OpenType layout data - 'GDEF', 'GSUB', and 'GPOS' tables.
  BLOpenType::LayoutData layout;

  union {
    //! OpenType font data [Compact Font Format] [CFF or CFF2].
    BLOpenType::CFFData cff;
    //! TrueType font data [glyf/loca].
    BLOpenType::GlyfData glyf;
  };
  //! Array of LSubR indexes used by CID fonts (CFF/CFF2).
  BLArray<BLOpenType::CFFData::IndexData> cffFDSubrIndexes;

  //! Script tags.
  BLArray<BLTag> scriptTags;
  //! Feature tags.
  BLArray<BLTag> featureTags;

  BL_INLINE uint32_t locaOffsetSize() const noexcept {
    return (otFlags & (BL_OT_FACE_FLAG_LOCA_OFFSET_16 | BL_OT_FACE_FLAG_LOCA_OFFSET_32));
  }
};

BL_HIDDEN BLResult blOTFaceImplNew(BLOTFaceImpl** dst, const BLFontData* fontData, uint32_t faceIndex) noexcept;
BL_HIDDEN void blOTFaceImplOnInit(BLRuntimeContext* rt) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_OTFACE_P_H_INCLUDED
