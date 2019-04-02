// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_OPENTYPE_BLOTFACE_P_H
#define BLEND2D_OPENTYPE_BLOTFACE_P_H

#include "../opentype/blotcff_p.h"
#include "../opentype/blotcmap_p.h"
#include "../opentype/blotdefs_p.h"
#include "../opentype/blotglyf_p.h"
#include "../opentype/blotkern_p.h"
#include "../opentype/blotlayout_p.h"
#include "../opentype/blotmetrics_p.h"
#include "../opentype/blotname_p.h"

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

  // Flags related to 'head' table.
  BL_OT_FACE_FLAG_BASELINE_Y_EQUALS_0   = 0x00000010u, //!< Baseline for font at `y` equals 0.
  BL_OT_FACE_FLAG_LSB_POINT_X_EQUALS_0  = 0x00000020u, //!< Left-side-bearing point at `x` equals 0 (TT only).

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

  //! Script tags.
  BLArray<BLTag> scriptTags;
  //! Feature tags.
  BLArray<BLTag> featureTags;

  BL_INLINE uint32_t locaOffsetSize() const noexcept {
    return (otFlags & (BL_OT_FACE_FLAG_LOCA_OFFSET_16 | BL_OT_FACE_FLAG_LOCA_OFFSET_32));
  }
};

BL_HIDDEN BLResult blOTFaceImplNew(BLOTFaceImpl** dst, const BLFontLoader* loader, const BLFontData* fontData, uint32_t faceIndex) noexcept;
BL_HIDDEN void blOTFaceImplRtInit(BLRuntimeContext* rt) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_BLOTFACE_P_H
