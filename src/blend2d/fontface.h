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

//! \addtogroup blend2d_api_text
//! \{

//! \name BLFontFace - C API
//! \{

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
BL_API BLResult BL_CDECL blFontFaceGetFaceInfo(const BLFontFaceCore* self, BLFontFaceInfo* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetScriptTags(const BLFontFaceCore* self, BLArrayCore* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetFeatureTags(const BLFontFaceCore* self, BLArrayCore* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetDesignMetrics(const BLFontFaceCore* self, BLFontDesignMetrics* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetUnicodeCoverage(const BLFontFaceCore* self, BLFontUnicodeCoverage* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetCharacterCoverage(const BLFontFaceCore* self, BLBitSetCore* out) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! Font face [C API].
struct BLFontFaceCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
  BL_DEFINE_OBJECT_DCAST(BLFontFace)
};

//! \}

//! \cond INTERNAL
//! \name BLFontFace - Internals
//! \{

//! Font face [Virtual Function Table].
struct BLFontFaceVirt BL_CLASS_INHERITS(BLObjectVirt) {
  BL_DEFINE_VIRT_BASE
};

//! Font face [Impl].
struct BLFontFaceImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Virtual function table.
  const BLFontFaceVirt* virt;

  //! Font-face default weight (1..1000) [0 if font-face is not initialized].
  uint16_t weight;
  //! Font-face default stretch (1..9) [0 if font-face is not initialized].
  uint8_t stretch;
  //! Font-face default style.
  uint8_t style;

  //! Font-face information.
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

  //! Script tags list (OpenType).
  BLArrayCore scriptTags;
  //! Feature tags list (OpenType).
  BLArrayCore featureTags;

  //! Font-face metrics in design units.
  BLFontDesignMetrics designMetrics;
  //! Font-face unicode coverage (specified in OS/2 header).
  BLFontUnicodeCoverage unicodeCoverage;
  //! Font-face panose classification.
  BLFontPanose panose;
};

//! \}
//! \endcond

//! \name BLFontFace - C++ API
//! \{

#ifdef __cplusplus
//! Font face [C++ API].
class BLFontFace : public BLFontFaceCore {
public:
  //! \cond INTERNAL
  //! \name Internals
  //! \{

  BL_INLINE BLFontFaceImpl* _impl() const noexcept { return static_cast<BLFontFaceImpl*>(_d.impl); }

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLFontFace() noexcept { blFontFaceInit(this); }
  BL_INLINE BLFontFace(BLFontFace&& other) noexcept { blFontFaceInitMove(this, &other); }
  BL_INLINE BLFontFace(const BLFontFace& other) noexcept { blFontFaceInitWeak(this, &other); }
  BL_INLINE ~BLFontFace() noexcept { blFontFaceDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return isValid(); }

  BL_INLINE BLFontFace& operator=(BLFontFace&& other) noexcept { blFontFaceAssignMove(this, &other); return *this; }
  BL_INLINE BLFontFace& operator=(const BLFontFace& other) noexcept { blFontFaceAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLFontFace& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLFontFace& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blFontFaceReset(this); }
  BL_INLINE void swap(BLFontFace& other) noexcept { _d.swap(other._d); }

  BL_INLINE BLResult assign(BLFontFace&& other) noexcept { return blFontFaceAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLFontFace& other) noexcept { return blFontFaceAssignWeak(this, &other); }

  //! Tests whether the font-face is valid.
  BL_INLINE bool isValid() const noexcept { return _impl()->faceInfo.faceType != BL_FONT_FACE_TYPE_NONE; }
  //! Tests whether the font-face is empty, which the same as `!isValid()`.
  BL_INLINE bool empty() const noexcept { return !isValid(); }

  BL_INLINE bool equals(const BLFontFace& other) const noexcept { return blFontFaceEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  //! Creates a new `BLFontFace` from a file specified by `fileName`.
  //!
  //! This is a utility function that first creates a `BLFontData` and then calls `createFromData(fontData, 0)`. See
  //! `BLFontData::createFromFile()` for more details, especially the use of `readFlags` is important for system fonts.
  //!
  //! \note This function offers a simplified creation of `BLFontFace` directly from a file, but doesn't provide as
  //! much flexibility as `createFromData()` as it allows to specify a `faceIndex`, which can be used to load multiple
  //! font-faces from a TrueType/OpenType collection. The use of `createFromData()` is recommended for any serious font
  //! handling.
  BL_INLINE BLResult createFromFile(const char* fileName, BLFileReadFlags readFlags = BL_FILE_READ_NO_FLAGS) noexcept {
    return blFontFaceCreateFromFile(this, fileName, readFlags);
  }

  //! Creates a new `BLFontFace` from `BLFontData` at the given `faceIndex`.
  //!
  //! On success the existing `BLFontFace` is completely replaced by a new one, on failure a `BLResult` is returned
  //! and the existing `BLFontFace` is kept as is. In other words, it either succeeds and replaces the `BLFontFaceImpl`
  //! or returns an error without touching the existing one.
  BL_INLINE BLResult createFromData(const BLFontDataCore& fontData, uint32_t faceIndex) noexcept {
    return blFontFaceCreateFromData(this, &fontData, faceIndex);
  }

  //! \}

  //! \name Properties
  //! \{

  //! Returns font weight (returns default weight in case this is a variable font).
  BL_INLINE uint32_t weight() const noexcept { return _impl()->weight; }
  //! Returns font stretch (returns default weight in case this is a variable font).
  BL_INLINE uint32_t stretch() const noexcept { return _impl()->stretch; }
  //! Returns font style.
  BL_INLINE uint32_t style() const noexcept { return _impl()->style; }

  //! Returns font-face information as `BLFontFaceInfo`.
  BL_INLINE const BLFontFaceInfo& faceInfo() const noexcept { return _impl()->faceInfo; }

  //! Returns the font-face type, see `BLFontFaceType`.
  BL_INLINE BLFontFaceType faceType() const noexcept { return (BLFontFaceType)_impl()->faceInfo.faceType; }
  //! Returns the font-face type, see `BLFontOutlineType`.
  BL_INLINE BLFontOutlineType outlineType() const noexcept { return (BLFontOutlineType)_impl()->faceInfo.outlineType; }
  //! Returns the number of glyphs of this font-face.
  BL_INLINE uint32_t glyphCount() const noexcept { return _impl()->faceInfo.glyphCount; }

  //! Returns a zero-based index of this font-face.
  //!
  //! \note Face index does only make sense if this face is part of a TrueType or OpenType font collection. In that
  //! case the returned value would be the index of this face in that collection. If the face is not part of a
  //! collection then the returned value would always be zero.
  BL_INLINE uint32_t faceIndex() const noexcept { return _impl()->faceInfo.faceIndex; }
  //! Returns font-face flags, see `BLFontFaceFlags`.
  BL_INLINE BLFontFaceFlags faceFlags() const noexcept { return (BLFontFaceFlags)_impl()->faceInfo.faceFlags; }

  //! Tests whether the font-face has a given `flag` set.
  BL_INLINE bool hasFaceFlag(BLFontFaceFlags flag) const noexcept { return (_impl()->faceInfo.faceFlags & flag) != 0; }

  //! Tests whether the font-face uses typographic family and subfamily names.
  BL_INLINE bool hasTypographicNames() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_TYPOGRAPHIC_NAMES); }
  //! Tests whether the font-face uses typographic metrics.
  BL_INLINE bool hasTypographicMetrics() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_TYPOGRAPHIC_METRICS); }
  //! Tests whether the font-face provides character to glyph mapping.
  BL_INLINE bool hasCharToGlyphMapping() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_CHAR_TO_GLYPH_MAPPING); }
  //! Tests whether the font-face has horizontal glyph metrics (advances, side bearings).
  BL_INLINE bool hasHorizontalMetrics() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_HORIZONTAL_METIRCS); }
  //! Tests whether the font-face has vertical glyph metrics (advances, side bearings).
  BL_INLINE bool hasVerticalMetrics() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_VERTICAL_METRICS); }
  //! Tests whether the font-face has a legacy horizontal kerning feature ('kern' table with horizontal kerning data).
  BL_INLINE bool hasHorizontalKerning() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_HORIZONTAL_KERNING); }
  //! Tests whether the font-face has a legacy vertical kerning feature ('kern' table with vertical kerning data).
  BL_INLINE bool hasVerticalKerning() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_VERTICAL_KERNING); }
  //! Tests whether the font-face has OpenType features (GDEF, GPOS, GSUB).
  BL_INLINE bool hasOpenTypeFeatures() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_OPENTYPE_FEATURES); }
  //! Tests whether the font-face has panose classification.
  BL_INLINE bool hasPanoseData() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_PANOSE_DATA); }
  //! Tests whether the font-face has unicode coverage information.
  BL_INLINE bool hasUnicodeCoverage() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_UNICODE_COVERAGE); }
  //! Tests whether the font-face's baseline equals 0.
  BL_INLINE bool hasBaselineYAt0() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_BASELINE_Y_EQUALS_0); }
  //! Tests whether the font-face's left sidebearing point at `x` equals 0.
  BL_INLINE bool hasLSBPointXAt0() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_LSB_POINT_X_EQUALS_0); }
  //! Tests whether the font-face has unicode variation sequences feature.
  BL_INLINE bool hasVariationSequences() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_VARIATION_SEQUENCES); }
  //! Tests whether the font-face has OpenType Font Variations feature.
  BL_INLINE bool hasOpenTypeVariations() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_OPENTYPE_VARIATIONS); }

  //! This is a symbol font.
  BL_INLINE bool isSymbolFont() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_SYMBOL_FONT); }
  //! This is a last resort font.
  BL_INLINE bool isLastResortFont() const noexcept { return hasFaceFlag(BL_FONT_FACE_FLAG_LAST_RESORT_FONT); }

  //! Returns font-face diagnostics flags, see `BLFontFaceDiagFlags`.
  BL_INLINE BLFontFaceDiagFlags diagFlags() const noexcept { return (BLFontFaceDiagFlags)_impl()->faceInfo.diagFlags; }

  //! Returns a unique identifier describing this `BLFontFace`.
  BL_INLINE BLUniqueId uniqueId() const noexcept { return _impl()->uniqueId; }

  //! Returns `BLFontData` associated with this font-face.
  BL_INLINE const BLFontData& data() const noexcept { return _impl()->data.dcast(); }

  //! Returns a full of the font.
  BL_INLINE const BLString& fullName() const noexcept { return _impl()->fullName.dcast(); }
  //! Returns a family name of the font.
  BL_INLINE const BLString& familyName() const noexcept { return _impl()->familyName.dcast(); }
  //! Returns a subfamily name of the font.
  BL_INLINE const BLString& subfamilyName() const noexcept { return _impl()->subfamilyName.dcast(); }
  //! Returns a PostScript name of the font.
  BL_INLINE const BLString& postScriptName() const noexcept { return _impl()->postScriptName.dcast(); }

  //! Returns script tags provided by this `BLFontFace` (OpenType).
  //!
  //! Each script tag is represented by 4 characters encoded in `BLTag`.
  BL_INLINE const BLArray<BLTag>& scriptTags() const noexcept { return _impl()->scriptTags.dcast<BLArray<BLTag>>(); }

  //! Returns feature tags provided by this `BLFontFace` (OpenType).
  //!
  //! Each feature tag is represented by 4 characters encoded in `BLTag`.
  //!
  //! Feature registry:
  //!   - Microsoft <https://docs.microsoft.com/en-us/typography/opentype/spec/featurelist>
  BL_INLINE const BLArray<BLTag>& featureTags() const noexcept { return _impl()->featureTags.dcast<BLArray<BLTag>>(); }

  //! Returns design metrics of this `BLFontFace`.
  BL_INLINE const BLFontDesignMetrics& designMetrics() const noexcept { return _impl()->designMetrics; }
  //! Returns units per em, which are part of font's design metrics.
  BL_INLINE int unitsPerEm() const noexcept { return _impl()->designMetrics.unitsPerEm; }

  //! Returns PANOSE classification of this `BLFontFace`.
  BL_INLINE const BLFontPanose& panose() const noexcept { return _impl()->panose; }

  //! Returns unicode coverage of this `BLFontFace`.
  //!
  //! \note The returned unicode-coverage is not calculated by Blend2D so in general the value doesn't have to be
  //! correct. Consider `getCharacterCoverage()` to get a coverage calculated by Blend2D at character granularity.
  BL_INLINE const BLFontUnicodeCoverage& unicodeCoverage() const noexcept { return _impl()->unicodeCoverage; }

  //! Calculates the character coverage of this `BLFontFace`.
  //!
  //! Each unicode character is represented by a single bit in the given BitSet.
  BL_INLINE BLResult getCharacterCoverage(BLBitSetCore* out) const noexcept { return blFontFaceGetCharacterCoverage(this, out); }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_FONTFACE_H_INCLUDED
