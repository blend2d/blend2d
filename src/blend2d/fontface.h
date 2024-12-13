// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTFACE_H_INCLUDED
#define BLEND2D_FONTFACE_H_INCLUDED

#include "array.h"
#include "bitset.h"
#include "filesystem.h"
#include "fontdata.h"
#include "fontdefs.h"
#include "geometry.h"
#include "glyphbuffer.h"
#include "object.h"
#include "path.h"
#include "string.h"

//! \addtogroup bl_text
//! \{

//! \name BLFontFace - Constants
//! \{

//! Flags used by \ref BLFontFace (or \ref BLFontFaceCore)
BL_DEFINE_ENUM(BLFontFaceFlags) {
  //! No flags.
  BL_FONT_FACE_NO_FLAGS = 0u,
  //! Font uses typographic family and subfamily names.
  BL_FONT_FACE_FLAG_TYPOGRAPHIC_NAMES = 0x00000001u,
  //! Font uses typographic metrics.
  BL_FONT_FACE_FLAG_TYPOGRAPHIC_METRICS = 0x00000002u,
  //! Character to glyph mapping is available.
  BL_FONT_FACE_FLAG_CHAR_TO_GLYPH_MAPPING = 0x00000004u,
  //! Horizontal glyph metrics (advances, side bearings) is available.
  BL_FONT_FACE_FLAG_HORIZONTAL_METIRCS = 0x00000010u,
  //! Vertical glyph metrics (advances, side bearings) is available.
  BL_FONT_FACE_FLAG_VERTICAL_METRICS = 0x00000020u,
  //! Legacy horizontal kerning feature ('kern' table with horizontal kerning data).
  BL_FONT_FACE_FLAG_HORIZONTAL_KERNING = 0x00000040u,
  //! Legacy vertical kerning feature ('kern' table with vertical kerning data).
  BL_FONT_FACE_FLAG_VERTICAL_KERNING = 0x00000080u,
  //! OpenType features (GDEF, GPOS, GSUB) are available.
  BL_FONT_FACE_FLAG_OPENTYPE_FEATURES = 0x00000100u,
  //! Panose classification is available.
  BL_FONT_FACE_FLAG_PANOSE_DATA = 0x00000200u,
  //! Unicode coverage information is available.
  BL_FONT_FACE_FLAG_UNICODE_COVERAGE = 0x00000400u,
  //! Baseline for font at `y` equals 0.
  BL_FONT_FACE_FLAG_BASELINE_Y_EQUALS_0 = 0x00001000u,
  //! Left sidebearing point at `x == 0` (TT only).
  BL_FONT_FACE_FLAG_LSB_POINT_X_EQUALS_0 = 0x00002000u,
  //! Unicode variation sequences feature is available.
  BL_FONT_FACE_FLAG_VARIATION_SEQUENCES = 0x10000000u,
  //! OpenType Font Variations feature is available.
  BL_FONT_FACE_FLAG_OPENTYPE_VARIATIONS = 0x20000000u,
  //! This is a symbol font.
  BL_FONT_FACE_FLAG_SYMBOL_FONT = 0x40000000u,
  //! This is a last resort font.
  BL_FONT_FACE_FLAG_LAST_RESORT_FONT = 0x80000000u

  BL_FORCE_ENUM_UINT32(BL_FONT_FACE_FLAG)
};

//! Diagnostic flags offered by \ref BLFontFace (or \ref BLFontFaceCore).
BL_DEFINE_ENUM(BLFontFaceDiagFlags) {
  //! No flags.
  BL_FONT_FACE_DIAG_NO_FLAGS = 0u,
  //! Wrong data in 'name' table.
  BL_FONT_FACE_DIAG_WRONG_NAME_DATA = 0x00000001u,
  //! Fixed data read from 'name' table and possibly fixed font family/subfamily name.
  BL_FONT_FACE_DIAG_FIXED_NAME_DATA = 0x00000002u,

  //! Wrong data in 'kern' table [kerning disabled].
  BL_FONT_FACE_DIAG_WRONG_KERN_DATA = 0x00000004u,
  //! Fixed data read from 'kern' table so it can be used.
  BL_FONT_FACE_DIAG_FIXED_KERN_DATA = 0x00000008u,

  //! Wrong data in 'cmap' table.
  BL_FONT_FACE_DIAG_WRONG_CMAP_DATA = 0x00000010u,
  //! Wrong format in 'cmap' (sub)table.
  BL_FONT_FACE_DIAG_WRONG_CMAP_FORMAT = 0x00000020u

  BL_FORCE_ENUM_UINT32(BL_FONT_FACE_DIAG)
};

//! Format of an outline stored in a font.
BL_DEFINE_ENUM(BLFontOutlineType) {
  //! None.
  BL_FONT_OUTLINE_TYPE_NONE = 0,
  //! Truetype outlines.
  BL_FONT_OUTLINE_TYPE_TRUETYPE = 1,
  //! OpenType (CFF) outlines.
  BL_FONT_OUTLINE_TYPE_CFF = 2,
  //! OpenType (CFF2) outlines with font variations support.
  BL_FONT_OUTLINE_TYPE_CFF2 = 3,

  //! Maximum value of `BLFontOutlineType`.
  BL_FONT_OUTLINE_TYPE_MAX_VALUE = 3

  BL_FORCE_ENUM_UINT32(BL_FONT_OUTLINE_TYPE)
};

//! \}

//! \name BLFontFace - Structs
//! \{

//! Information of \ref BLFontFace.
struct BLFontFaceInfo {
  //! \name Members
  //! \{

  //! Font face type, see \ref BLFontFaceType.
  uint8_t faceType;
  //! Type of outlines used by the font face, see \ref BLFontOutlineType.
  uint8_t outlineType;
  //! Reserved fields.
  uint8_t reserved8[2];
  //! Number of glyphs provided by this font face.
  uint32_t glyphCount;

  //! Revision (read from 'head' table, represented as 16.16 fixed point).
  uint32_t revision;

  //! Face face index in a TTF/OTF collection or zero if not part of a collection.
  uint32_t faceIndex;
  //! Font face flags, see \ref BLFontFaceFlags
  uint32_t faceFlags;

  //! Font face diagnostic flags, see \ref BLFontFaceDiagFlags.
  uint32_t diagFlags;

  //! Reserved for future use, set to zero.
  uint32_t reserved[2];

  //! \}

#ifdef __cplusplus
  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG void reset() noexcept { *this = BLFontFaceInfo{}; }

  //! \}
#endif
};

//! \}
//! \}

//! \addtogroup bl_c_api
//! \{

//! \name BLFontFace - C API
//! \{

//! Font face [C API].
struct BLFontFaceCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
  BL_DEFINE_OBJECT_DCAST(BLFontFace)
};

//! \cond INTERNAL
//! Font face [C API Virtual Function Table].
struct BLFontFaceVirt BL_CLASS_INHERITS(BLObjectVirt) {
  BL_DEFINE_VIRT_BASE
};

//! Font face [C API Impl].
struct BLFontFaceImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Virtual function table.
  const BLFontFaceVirt* virt;

  //! Font face default weight (1..1000) [0 if font face is not initialized].
  uint16_t weight;
  //! Font face default stretch (1..9) [0 if font face is not initialized].
  uint8_t stretch;
  //! Font face default style.
  uint8_t style;

  //! Font face information.
  BLFontFaceInfo faceInfo;
  //! Unique identifier assigned by Blend2D that can be used for caching.
  BLUniqueId uniqueId;

  //! Font data.
  BLFontDataCore data;
  //! Full name.
  BLStringCore fullName;
  //! Family name.
  BLStringCore familyName;
  //! Subfamily name.
  BLStringCore subfamilyName;
  //! PostScript name.
  BLStringCore postScriptName;

  //! Font face metrics in design units.
  BLFontDesignMetrics designMetrics;
  //! Font face unicode coverage (specified in OS/2 header).
  BLFontUnicodeCoverage unicodeCoverage;
  //! Font face panose classification.
  BLFontPanose panose;
};
//! \endcond

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blFontFaceInit(BLFontFaceCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceInitMove(BLFontFaceCore* self, BLFontFaceCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceInitWeak(BLFontFaceCore* self, const BLFontFaceCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceDestroy(BLFontFaceCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceReset(BLFontFaceCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceAssignMove(BLFontFaceCore* self, BLFontFaceCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceAssignWeak(BLFontFaceCore* self, const BLFontFaceCore* other) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blFontFaceEquals(const BLFontFaceCore* a, const BLFontFaceCore* b) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceCreateFromFile(BLFontFaceCore* self, const char* fileName, BLFileReadFlags readFlags) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceCreateFromData(BLFontFaceCore* self, const BLFontDataCore* fontData, uint32_t faceIndex) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetFullName(const BLFontFaceCore* self, BLStringCore* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetFamilyName(const BLFontFaceCore* self, BLStringCore* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetSubfamilyName(const BLFontFaceCore* self, BLStringCore* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetPostScriptName(const BLFontFaceCore* self, BLStringCore* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetFaceInfo(const BLFontFaceCore* self, BLFontFaceInfo* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetDesignMetrics(const BLFontFaceCore* self, BLFontDesignMetrics* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetUnicodeCoverage(const BLFontFaceCore* self, BLFontUnicodeCoverage* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetCharacterCoverage(const BLFontFaceCore* self, BLBitSetCore* out) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blFontFaceHasScriptTag(const BLFontFaceCore* self, BLTag scriptTag) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blFontFaceHasFeatureTag(const BLFontFaceCore* self, BLTag featureTag) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blFontFaceHasVariationTag(const BLFontFaceCore* self, BLTag variationTag) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetScriptTags(const BLFontFaceCore* self, BLArrayCore* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetFeatureTags(const BLFontFaceCore* self, BLArrayCore* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetVariationTags(const BLFontFaceCore* self, BLArrayCore* out) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! \}
//! \}

//! \addtogroup bl_text
//! \{

//! \name BLFontFace - C++ API
//! \{

#ifdef __cplusplus
//! Font face [C++ API].
class BLFontFace final : public BLFontFaceCore {
public:
  //! \cond INTERNAL
  //! \name Internals
  //! \{

  BL_INLINE_NODEBUG BLFontFaceImpl* _impl() const noexcept { return static_cast<BLFontFaceImpl*>(_d.impl); }

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLFontFace() noexcept { blFontFaceInit(this); }
  BL_INLINE_NODEBUG BLFontFace(BLFontFace&& other) noexcept { blFontFaceInitMove(this, &other); }
  BL_INLINE_NODEBUG BLFontFace(const BLFontFace& other) noexcept { blFontFaceInitWeak(this, &other); }

  BL_INLINE_NODEBUG ~BLFontFace() noexcept {
    if (BLInternal::objectNeedsCleanup(_d.info.bits))
      blFontFaceDestroy(this);
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return isValid(); }

  BL_INLINE_NODEBUG BLFontFace& operator=(BLFontFace&& other) noexcept { blFontFaceAssignMove(this, &other); return *this; }
  BL_INLINE_NODEBUG BLFontFace& operator=(const BLFontFace& other) noexcept { blFontFaceAssignWeak(this, &other); return *this; }

  BL_INLINE_NODEBUG bool operator==(const BLFontFace& other) const noexcept { return  equals(other); }
  BL_INLINE_NODEBUG bool operator!=(const BLFontFace& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult reset() noexcept { return blFontFaceReset(this); }
  BL_INLINE_NODEBUG void swap(BLFontFace& other) noexcept { _d.swap(other._d); }

  BL_INLINE_NODEBUG BLResult assign(BLFontFace&& other) noexcept { return blFontFaceAssignMove(this, &other); }
  BL_INLINE_NODEBUG BLResult assign(const BLFontFace& other) noexcept { return blFontFaceAssignWeak(this, &other); }

  //! Tests whether the font face is valid.
  BL_INLINE_NODEBUG bool isValid() const noexcept { return _impl()->faceInfo.faceType != BL_FONT_FACE_TYPE_NONE; }
  //! Tests whether the font face is empty, which is the same as `!isValid()`.
  BL_INLINE_NODEBUG bool empty() const noexcept { return !isValid(); }

  BL_INLINE_NODEBUG bool equals(const BLFontFace& other) const noexcept { return blFontFaceEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  //! Creates a new \ref BLFontFace from a file specified by `fileName`.
  //!
  //! This is a utility function that first creates a \ref BLFontData` and then calls `createFromData(fontData, 0)`.
  //! See \ref BLFontData::createFromFile() for more details, especially the use of `readFlags` is important for
  //! system fonts.
  //!
  //! \note This function offers a simplified creation of \ref BLFontFace directly from a file, but doesn't provide
  //! as much flexibility as \ref createFromData() as it allows to specify a `faceIndex`, which can be used to load
  //! multiple font faces from a TrueType/OpenType collection. The use of \ref createFromData() is recommended for
  //! any serious font handling.
  BL_INLINE_NODEBUG BLResult createFromFile(const char* fileName, BLFileReadFlags readFlags = BL_FILE_READ_NO_FLAGS) noexcept {
    return blFontFaceCreateFromFile(this, fileName, readFlags);
  }

  //! Creates a new \ref BLFontFace from \ref BLFontData at the given `faceIndex`.
  //!
  //! On success the existing \ref BLFontFace is completely replaced by a new one, on failure an error is returned
  //! in \ref BLResult and the existing \ref BLFontFace is kept as is.
  BL_INLINE_NODEBUG BLResult createFromData(const BLFontDataCore& fontData, uint32_t faceIndex) noexcept {
    return blFontFaceCreateFromData(this, &fontData, faceIndex);
  }

  //! \}

  //! \name Properties
  //! \{

  //! Returns font weight (returns default weight in case this is a variable font).
  BL_INLINE_NODEBUG uint32_t weight() const noexcept { return _impl()->weight; }
  //! Returns font stretch (returns default weight in case this is a variable font).
  BL_INLINE_NODEBUG uint32_t stretch() const noexcept { return _impl()->stretch; }
  //! Returns font style.
  BL_INLINE_NODEBUG uint32_t style() const noexcept { return _impl()->style; }

  //! Returns font face information as \ref BLFontFaceInfo.
  BL_INLINE_NODEBUG const BLFontFaceInfo& faceInfo() const noexcept { return _impl()->faceInfo; }

  //! Returns the font face type.
  BL_INLINE_NODEBUG BLFontFaceType faceType() const noexcept { return (BLFontFaceType)_impl()->faceInfo.faceType; }
  //! Returns the font face type.
  BL_INLINE_NODEBUG BLFontOutlineType outlineType() const noexcept { return (BLFontOutlineType)_impl()->faceInfo.outlineType; }
  //! Returns the number of glyphs this font face provides.
  BL_INLINE_NODEBUG uint32_t glyphCount() const noexcept { return _impl()->faceInfo.glyphCount; }

  //! Returns a zero-based index of this font face.
  //!
  //! \note Face index does only make sense if this face is part of a TrueType or OpenType font collection. In that
  //! case the returned value would be the index of this face in that collection. If the face is not part of a
  //! collection then the returned value would always be zero.
  BL_INLINE_NODEBUG uint32_t faceIndex() const noexcept { return _impl()->faceInfo.faceIndex; }
  //! Returns font face flags.
  BL_INLINE_NODEBUG BLFontFaceFlags faceFlags() const noexcept { return (BLFontFaceFlags)_impl()->faceInfo.faceFlags; }

  //! Tests whether the font face has a given `flag` set.
  BL_INLINE_NODEBUG bool hasFaceFlag(BLFontFaceFlags flag) const noexcept { return (_impl()->faceInfo.faceFlags & flag) != 0; }

  //! Tests whether the font face uses typographic family and subfamily names.
  BL_INLINE_NODEBUG bool hasTypographicNames() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_TYPOGRAPHIC_NAMES); }
  //! Tests whether the font face uses typographic metrics.
  BL_INLINE_NODEBUG bool hasTypographicMetrics() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_TYPOGRAPHIC_METRICS); }
  //! Tests whether the font face provides character to glyph mapping.
  BL_INLINE_NODEBUG bool hasCharToGlyphMapping() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_CHAR_TO_GLYPH_MAPPING); }
  //! Tests whether the font face has horizontal glyph metrics (advances, side bearings).
  BL_INLINE_NODEBUG bool hasHorizontalMetrics() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_HORIZONTAL_METIRCS); }
  //! Tests whether the font face has vertical glyph metrics (advances, side bearings).
  BL_INLINE_NODEBUG bool hasVerticalMetrics() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_VERTICAL_METRICS); }
  //! Tests whether the font face has a legacy horizontal kerning feature ('kern' table with horizontal kerning data).
  BL_INLINE_NODEBUG bool hasHorizontalKerning() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_HORIZONTAL_KERNING); }
  //! Tests whether the font face has a legacy vertical kerning feature ('kern' table with vertical kerning data).
  BL_INLINE_NODEBUG bool hasVerticalKerning() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_VERTICAL_KERNING); }
  //! Tests whether the font face has OpenType features (GDEF, GPOS, GSUB).
  BL_INLINE_NODEBUG bool hasOpenTypeFeatures() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_OPENTYPE_FEATURES); }
  //! Tests whether the font face has panose classification.
  BL_INLINE_NODEBUG bool hasPanoseData() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_PANOSE_DATA); }
  //! Tests whether the font face has unicode coverage information.
  BL_INLINE_NODEBUG bool hasUnicodeCoverage() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_UNICODE_COVERAGE); }
  //! Tests whether the font face's baseline equals 0.
  BL_INLINE_NODEBUG bool hasBaselineYAt0() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_BASELINE_Y_EQUALS_0); }
  //! Tests whether the font face's left sidebearing point at `x` equals 0.
  BL_INLINE_NODEBUG bool hasLSBPointXAt0() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_LSB_POINT_X_EQUALS_0); }
  //! Tests whether the font face has unicode variation sequences feature.
  BL_INLINE_NODEBUG bool hasVariationSequences() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_VARIATION_SEQUENCES); }
  //! Tests whether the font face has OpenType Font Variations feature.
  BL_INLINE_NODEBUG bool hasOpenTypeVariations() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_OPENTYPE_VARIATIONS); }

  //! This is a symbol font.
  BL_INLINE_NODEBUG bool isSymbolFont() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_SYMBOL_FONT); }
  //! This is a last resort font.
  BL_INLINE_NODEBUG bool isLastResortFont() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_LAST_RESORT_FONT); }

  //! Returns font face diagnostics flags.
  BL_INLINE_NODEBUG BLFontFaceDiagFlags diagFlags() const noexcept { return BLFontFaceDiagFlags(_impl()->faceInfo.diagFlags); }

  //! Returns a unique identifier describing this \ref BLFontFace.
  BL_INLINE_NODEBUG BLUniqueId uniqueId() const noexcept { return _impl()->uniqueId; }

  //! Returns \ref BLFontData associated with this font face.
  BL_INLINE_NODEBUG const BLFontData& data() const noexcept { return _impl()->data.dcast(); }

  //! Returns a full of the font.
  BL_INLINE_NODEBUG const BLString& fullName() const noexcept { return _impl()->fullName.dcast(); }
  //! Returns a family name of the font.
  BL_INLINE_NODEBUG const BLString& familyName() const noexcept { return _impl()->familyName.dcast(); }
  //! Returns a subfamily name of the font.
  BL_INLINE_NODEBUG const BLString& subfamilyName() const noexcept { return _impl()->subfamilyName.dcast(); }
  //! Returns a PostScript name of the font.
  BL_INLINE_NODEBUG const BLString& postScriptName() const noexcept { return _impl()->postScriptName.dcast(); }

  //! Returns design metrics of this \ref BLFontFace.
  BL_INLINE_NODEBUG const BLFontDesignMetrics& designMetrics() const noexcept { return _impl()->designMetrics; }
  //! Returns units per em, which are part of font's design metrics.
  BL_INLINE_NODEBUG int unitsPerEm() const noexcept { return _impl()->designMetrics.unitsPerEm; }

  //! Returns PANOSE classification of this \ref BLFontFace`.
  BL_INLINE_NODEBUG const BLFontPanose& panose() const noexcept { return _impl()->panose; }

  //! Returns unicode coverage of this \ref BLFontFace.
  //!
  //! \note The returned unicode-coverage is not calculated by Blend2D so in general the value doesn't have to be
  //! correct. Consider `getCharacterCoverage()` to get a coverage calculated by Blend2D at character granularity.
  BL_INLINE_NODEBUG const BLFontUnicodeCoverage& unicodeCoverage() const noexcept { return _impl()->unicodeCoverage; }

  //! Calculates the character coverage of this \ref BLFontFace.
  //!
  //! Each unicode character is represented by a single bit in the given BitSet.
  BL_INLINE_NODEBUG BLResult getCharacterCoverage(BLBitSetCore* out) const noexcept { return blFontFaceGetCharacterCoverage(this, out); }

  //! Tests whether the font face provides the given OpenType `scriptTag`.
  BL_INLINE_NODEBUG bool hasScriptTag(BLTag scriptTag) const noexcept { return blFontFaceHasScriptTag(this, scriptTag); }

  //! Tests whether the font face provides the given OpenType `featureTag`.
  BL_INLINE_NODEBUG bool hasFeatureTag(BLTag featureTag) const noexcept { return blFontFaceHasFeatureTag(this, featureTag); }

  //! Tests whether the font face provides the given OpenType `variationTag`.
  BL_INLINE_NODEBUG bool hasVariationTag(BLTag variationTag) const noexcept { return blFontFaceHasVariationTag(this, variationTag); }

  //! Retrieves OpenType script tags provided by this \ref BLFontFace.
  //!
  //! Each script tag is represented by 4 characters encoded in \ref BLTag.
  BL_INLINE_NODEBUG BLResult getScriptTags(BLArray<BLTag>* out) const noexcept { return blFontFaceGetScriptTags(this, out); }

  //! Retrieves OpenType feature tags provided by this \ref BLFontFace.
  //!
  //! Each feature tag is represented by 4 characters encoded in \ref BLTag.
  //!
  //! Feature tag registry:
  //!   - Microsoft <https://docs.microsoft.com/en-us/typography/opentype/spec/featurelist>
  BL_INLINE_NODEBUG BLResult getFeatureTags(BLArray<BLTag>* out) const noexcept { return blFontFaceGetFeatureTags(this, out); }

  //! Retrieves OpenType variation tags provided by this \ref BLFontFace.
  //!
  //! Each variation tag is represented by 4 characters encoded in \ref BLTag.
  //!
  //! Variation tag registry:
  //!   - Microsoft <https://docs.microsoft.com/en-us/typography/opentype/spec/dvaraxisreg>
  BL_INLINE_NODEBUG BLResult getVariationTags(BLArray<BLTag>* out) const noexcept { return blFontFaceGetVariationTags(this, out); }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_FONTFACE_H_INCLUDED
