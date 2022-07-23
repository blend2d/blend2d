// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

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
//! \addtogroup blend2d_opentype_impl
//! \{

namespace BLOpenType {

enum class OTFaceFlags : uint32_t {
  // Flags related to 'loca' table.
  kLocaOffset16        = 0x00000002u, //!< Glyph offsets in 'loca' table use 16-bit offsets [must be 0x2].
  kLocaOffset32        = 0x00000004u, //!< Glyph offsets in 'loca' table use 32-bit offsets [must be 0x4].

  // Flags related to 'GDEF' table.
  kGlyphClassDef       = 0x00000100u,
  kAttachList          = 0x00000200u,
  kLitCaretList        = 0x00000400u,
  kMarkAttachClassDef  = 0x00000800u,
  kMarkGlyphSetsDef    = 0x00001000u,
  kItemVarStore        = 0x00002000u,

  // Flags related to 'GSUB' table.
  kGSubScriptList      = 0x00010000u,
  kGSubFeatureList     = 0x00020000u,
  kGSubLookupList      = 0x00040000u,
  kGSubFVar            = 0x00080000u,

  // Flags related to 'GPOS' table.
  kGPosScriptList      = 0x00100000u,
  kGPosFeatureList     = 0x00200000u,
  kGPosLookupList      = 0x00400000u,
  kGPosFVar            = 0x00800000u
};
BL_DEFINE_ENUM_FLAGS(OTFaceFlags)

//! OpenType & TrueType font face.
//!
//! This class provides extra data required by TrueType / OpenType implementation. It's currently the only
//! implementation of \ref BLFontFaceImpl available in Blend2D and there will probably not be any other
//! implementation as OpenType provides enough features required to render text in general.
struct OTFaceImpl : public BLFontFacePrivateImpl {
  //! OpenType flags, see OTFlags.
  OTFaceFlags otFlags;

  //! Character mapping format (stored here so we won't misalign `CMapData`.
  uint8_t cmapFormat;

  //! Reserved for future use.
  uint8_t reservedOpenType[3];

  //! Character to glyph mapping data.
  CMapData cmap;
  //! Metrics data.
  MetricsData metrics;

  //! Legacy kerning data - 'kern' table and related data.
  KernData kern;
  //! OpenType layout data - 'GDEF', 'GSUB', and 'GPOS' tables.
  LayoutData layout;

  union {
    //! OpenType font data [Compact Font Format] [CFF or CFF2].
    CFFData cff;
    //! TrueType font data [glyf/loca].
    GlyfData glyf;
  };
  //! Array of LSubR indexes used by CID fonts (CFF/CFF2).
  BLArray<CFFData::IndexData> cffFDSubrIndexes;

  BL_INLINE uint32_t locaOffsetSize() const noexcept {
    return uint32_t(otFlags & (OTFaceFlags::kLocaOffset16 | OTFaceFlags::kLocaOffset32));
  }
};

BL_HIDDEN BLResult createOpenTypeFace(BLFontFaceCore* self, const BLFontData* fontData, uint32_t faceIndex) noexcept;

} // {BLOpenType}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_OTFACE_P_H_INCLUDED
