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

#ifndef BLEND2D_FONT_H_INCLUDED
#define BLEND2D_FONT_H_INCLUDED

#include "./array.h"
#include "./fontdefs.h"
#include "./geometry.h"
#include "./glyphbuffer.h"
#include "./path.h"
#include "./string.h"
#include "./variant.h"

//! \addtogroup blend2d_api_text
//! \{

// ============================================================================
// [BLFontData - Core]
// ============================================================================

//! Font data [C Interface - Virtual Function Table].
struct BLFontDataVirt {
  BLResult (BL_CDECL* destroy)(BLFontDataImpl* impl) BL_NOEXCEPT;
  BLResult (BL_CDECL* listTags)(const BLFontDataImpl* impl, uint32_t faceIndex, BLArrayCore* out) BL_NOEXCEPT;
  size_t (BL_CDECL* queryTables)(const BLFontDataImpl* impl, uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t n) BL_NOEXCEPT;
};

//! Font data [C Interface - Impl].
struct BLFontDataImpl {
  //! Virtual function table.
  const BLFontDataVirt* virt;

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;
  //! Type of the face that would be created with this font-data.
  uint8_t faceType;
  //! Reserved for future use, must be zero.
  uint8_t reserved[3];

  //! Number of font-faces stored in this font-data instance.
  uint32_t faceCount;
  //! Font-data flags.
  uint32_t flags;
};

//! Font data [C Interface - Core].
struct BLFontDataCore {
  BLFontDataImpl* impl;
};

// ============================================================================
// [BLFontData - C++]
// ============================================================================

#ifdef __cplusplus
//! Font data [C++ API].
class BLFontData : public BLFontDataCore {
public:
  //! \cond INTERNAL
  static constexpr const uint32_t kImplType = BL_IMPL_TYPE_FONT_DATA;
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLFontData() noexcept { this->impl = none().impl; }
  BL_INLINE BLFontData(BLFontData&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLFontData(const BLFontData& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLFontData(BLFontDataImpl* impl) noexcept { this->impl = impl; }
  BL_INLINE ~BLFontData() noexcept { blFontDataDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return !isNone(); }

  BL_INLINE BLFontData& operator=(BLFontData&& other) noexcept { blFontDataAssignMove(this, &other); return *this; }
  BL_INLINE BLFontData& operator=(const BLFontData& other) noexcept { blFontDataAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLFontData& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLFontData& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blFontDataReset(this); }
  BL_INLINE void swap(BLFontData& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult assign(BLFontData&& other) noexcept { return blFontDataAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLFontData& other) noexcept { return blFontDataAssignWeak(this, &other); }

  //! Tests whether the font-data is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }
  //! Tests whether the font-data is empty (which the same as `isNone()` in this case).
  BL_INLINE bool empty() const noexcept { return isNone(); }

  BL_INLINE bool equals(const BLFontData& other) const noexcept { return blFontDataEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  //! Creates a `BLFontData` from a file specified by the given `fileName`.
  //!
  //! \remarks The `readFlags` argument allows to specify flags that will be passed
  //! to `BLFileSystem::readFile()` to read the content of the file. It's possible to
  //! use memory mapping to get its content, which is the recommended way for reading
  //! system fonts. The best combination is to use `BL_FILE_READ_MMAP_ENABLED` flag
  //! combined with `BL_FILE_READ_MMAP_AVOID_SMALL`. This combination means to try to
  //! use memory mapping only when the size of the font is greater than a minimum value
  //! (determined by Blend2D), and would fallback to a regular open/read in case the
  //! memory mapping is not possible or failed for some other reason. Please note that
  //! not all files can be memory mapped so `BL_FILE_READ_MMAP_NO_FALLBACK` flag is not
  //! recommended.
  BL_INLINE BLResult createFromFile(const char* fileName, uint32_t readFlags = 0) noexcept {
    return blFontDataCreateFromFile(this, fileName, readFlags);
  }

  //! Creates a `BLFontData` from the given `data` stored in `BLArray<uint8_t>`
  //!
  //! The given `data` would be weak copied on success so the given array can be
  //! safely destroyed after the function returns.
  //!
  //! \remarks The weak copy of the passed `data` is internal and there is no API
  //! to access it after the function returns. The reason for making it internal
  //! is that multiple implementations of `BLFontData` may exist and some can only
  //! store data at table level, so Blend2D doesn't expose the detail about how the
  //! data is stored.
  BL_INLINE BLResult createFromData(const BLArray<uint8_t>& data) noexcept {
    return blFontDataCreateFromDataArray(this, &data);
  }

  //! Creates ` BLFontData` from the given `data` of the given `size`.
  //!
  //! \note Optionally a `destroyFunc` can be used as a notifier that will be
  //! called when the data is no longer needed and `destroyData` acts as a user
  //! data passed to `destroyFunc()`.
  BL_INLINE BLResult createFromData(const void* data, size_t dataSize, BLDestroyImplFunc destroyFunc = nullptr, void* destroyData = nullptr) noexcept {
    return blFontDataCreateFromData(this, data, dataSize, destroyFunc, destroyData);
  }

  //! \}

  //! \name Query Functionality
  //! \{

  BL_INLINE BLResult listTags(uint32_t faceIndex, BLArray<BLTag>& dst) const noexcept {
    // The same as blFontDataListTags() [C-API].
    return impl->virt->listTags(impl, faceIndex, &dst);
  }

  BL_INLINE size_t queryTable(uint32_t faceIndex, BLFontTable* dst, BLTag tag) const noexcept {
    // The same as blFontDataQueryTables() [C-API].
    return impl->virt->queryTables(impl, faceIndex, dst, &tag, 1);
  }

  BL_INLINE size_t queryTables(uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t count) const noexcept {
    // The same as blFontDataQueryTables() [C-API].
    return impl->virt->queryTables(impl, faceIndex, dst, tags, count);
  }

  //! \}

  //! \name Accessors
  //! \{

  //! Type of font-face that this data describes.
  //!
  //! It doesn't matter if the content is a single font or a collection. In
  //! any case the `faceType()` would always return the type of the font-face
  //! that will be created by `BLFontFace::createFromData()`.
  BL_INLINE uint32_t faceType() const noexcept { return impl->faceType; }

  //! Returns the number of faces of this font-data.
  //!
  //! If the data is not initialized the result would be always zero. If the
  //! data is initialized to a single font it would be 1, and if the data is
  //! initialized to a font collection then the return would correspond to
  //! the number of font-faces within that collection.
  //!
  //! \note You should not use `faceCount()` to check whether the font is a
  //! collection as it's possible to have a font-collection with just a single
  //! font. Using `isCollection()` is more reliable and would always return the
  //! right value.
  BL_INLINE uint32_t faceCount() const noexcept { return impl->faceCount; }

  //! Returns font-data flags, see `BLFontDataFlags`.
  BL_INLINE uint32_t flags() const noexcept {
    return impl->flags;
  }

  //! Tests whether this font-data is a font-collection.
  BL_INLINE bool isCollection() const noexcept {
    return (impl->flags & BL_FONT_DATA_FLAG_COLLECTION) != 0;
  }

  //! \}

  static BL_INLINE const BLFontData& none() noexcept { return reinterpret_cast<const BLFontData*>(blNone)[kImplType]; }
};
#endif

// ============================================================================
// [BLFontFace - Core]
// ============================================================================

//! Font face [C Interface - Virtual Function Table].
struct BLFontFaceVirt {
  BLResult (BL_CDECL* destroy)(BLFontFaceImpl* impl) BL_NOEXCEPT;
};

//! Font face [C Interface - Impl].
struct BLFontFaceImpl {
  //! Virtual function table.
  const BLFontFaceVirt* virt;

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;

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
  BL_TYPED_MEMBER(BLFontDataCore, BLFontData, data);
  //! Full name.
  BL_TYPED_MEMBER(BLStringCore, BLString, fullName);
  //! Family name.
  BL_TYPED_MEMBER(BLStringCore, BLString, familyName);
  //! Subfamily name.
  BL_TYPED_MEMBER(BLStringCore, BLString, subfamilyName);
  //! PostScript name.
  BL_TYPED_MEMBER(BLStringCore, BLString, postScriptName);

  //! Font-face metrics in design units.
  BLFontDesignMetrics designMetrics;
  //! Font-face unicode coverage (specified in OS/2 header).
  BLFontUnicodeCoverage unicodeCoverage;
  //! Font-face panose classification.
  BLFontPanose panose;

  BL_HAS_TYPED_MEMBERS(BLFontFaceImpl)
};

//! Font face [C Interface - Core].
struct BLFontFaceCore {
  BLFontFaceImpl* impl;
};

// ============================================================================
// [BLFontFace - C++]
// ============================================================================

#ifdef __cplusplus
//! Font face [C++ API].
class BLFontFace : public BLFontFaceCore {
public:
  //! \cond INTERNAL
  static constexpr const uint32_t kImplType = BL_IMPL_TYPE_FONT_FACE;
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLFontFace() noexcept { this->impl = none().impl; }
  BL_INLINE BLFontFace(BLFontFace&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLFontFace(const BLFontFace& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLFontFace(BLFontFaceImpl* impl) noexcept { this->impl = impl; }
  BL_INLINE ~BLFontFace() noexcept { blFontFaceDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return !isNone(); }

  BL_INLINE BLFontFace& operator=(BLFontFace&& other) noexcept { blFontFaceAssignMove(this, &other); return *this; }
  BL_INLINE BLFontFace& operator=(const BLFontFace& other) noexcept { blFontFaceAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLFontFace& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLFontFace& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blFontFaceReset(this); }
  BL_INLINE void swap(BLFontFace& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult assign(BLFontFace&& other) noexcept { return blFontFaceAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLFontFace& other) noexcept { return blFontFaceAssignWeak(this, &other); }

  //! Tests whether the font-face is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }
  //! Tests whether the font-face is empty (which the same as `isNone()` in this case).
  BL_INLINE bool empty() const noexcept { return isNone(); }

  BL_INLINE bool equals(const BLFontFace& other) const noexcept { return blFontFaceEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  //! Creates a new `BLFontFace` from a file specified by `fileName`.
  //!
  //! This is a utility function that first creates a `BLFontData` and then
  //! calls `createFromData(fontData, 0)`. See `BLFontData::createFromFile()`
  //! for more details, especially the use of `readFlags` is important for
  //! system fonts.
  //!
  //! \note This function offers a simplified creation of `BLFontFace` directly
  //! from a file, but doesn't provide as much flexibility as `createFromData()`
  //! as it allows to specify a `faceIndex`, which can be used to load multiple
  //! font-faces from a TrueType/OpenType collection. The use of `createFromData()`
  //! is recommended for any serious font handling.
  BL_INLINE BLResult createFromFile(const char* fileName, uint32_t readFlags = 0) noexcept {
    return blFontFaceCreateFromFile(this, fileName, readFlags);
  }

  //! Creates a new `BLFontFace` from `BLFontData` at the given `faceIndex`.
  //!
  //! On success the existing `BLFontFace` is completely replaced by a new one,
  //! on failure a `BLResult` is returned and the existing `BLFontFace` is kept
  //! as is. In other words, it either succeeds and replaces the `BLFontFaceImpl`
  //! or returns an error without touching the existing one.
  BL_INLINE BLResult createFromData(const BLFontData& fontData, uint32_t faceIndex) noexcept {
    return blFontFaceCreateFromData(this, &fontData, faceIndex);
  }

  //! \}

  //! \name Properties
  //! \{

  //! Returns font weight (returns default weight in case this is a variable font).
  BL_INLINE uint32_t weight() const noexcept { return impl->weight; }
  //! Returns font stretch (returns default weight in case this is a variable font).
  BL_INLINE uint32_t stretch() const noexcept { return impl->stretch; }
  //! Returns font style.
  BL_INLINE uint32_t style() const noexcept { return impl->style; }

  //! Returns font-face information as `BLFontFaceInfo`.
  BL_INLINE const BLFontFaceInfo& faceInfo() const noexcept { return impl->faceInfo; }

  //! Returns the font-face type, see `BLFontFaceType`.
  BL_INLINE uint32_t faceType() const noexcept { return impl->faceInfo.faceType; }
  //! Returns the font-face type, see `BLFontOutlineType`.
  BL_INLINE uint32_t outlineType() const noexcept { return impl->faceInfo.outlineType; }
  //! Returns the number of glyphs of this font-face.
  BL_INLINE uint32_t glyphCount() const noexcept { return impl->faceInfo.glyphCount; }

  //! Returns a zero-based index of this font-face.
  //!
  //! \note Face index does only make sense if this face is part of a TrueType
  //! or OpenType font collection. In that case the returned value would be
  //! the index of this face in that collection. If the face is not part of a
  //! collection then the returned value would always be zero.
  BL_INLINE uint32_t faceIndex() const noexcept { return impl->faceInfo.faceIndex; }
  //! Returns font-face flags, see `BLFontFaceFlags`.
  BL_INLINE uint32_t faceFlags() const noexcept { return impl->faceInfo.faceFlags; }

  //! Tests whether the font-face has a given `flag` set.
  BL_INLINE bool hasFaceFlag(uint32_t flag) const noexcept { return (impl->faceInfo.faceFlags & flag) != 0; }

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
  BL_INLINE uint32_t diagFlags() const noexcept { return impl->faceInfo.diagFlags; }

  //! Returns a unique identifier describing this `BLFontFace`.
  BL_INLINE BLUniqueId uniqueId() const noexcept { return impl->uniqueId; }

  //! Returns `BLFontData` associated with this font-face.
  BL_INLINE const BLFontData& data() const noexcept { return impl->data; }

  //! Returns the font full name as UTF-8 null-terminated string.
  BL_INLINE const char* fullName() const noexcept { return impl->fullName.data(); }
  //! Returns the size of the string returned by `fullName()`.
  BL_INLINE size_t fullNameSize() const noexcept { return impl->fullName.size(); }
  //! Returns the font full name as a UTF-8 string view.
  BL_INLINE const BLStringView& fullNameView() const noexcept { return impl->fullName.view(); }

  //! Returns the family name as UTF-8 null-terminated string.
  BL_INLINE const char* familyName() const noexcept { return impl->familyName.data(); }
  //! Returns the size of the string returned by `familyName()`.
  BL_INLINE size_t familyNameSize() const noexcept { return impl->familyName.size(); }
  //! Returns the family-name as a UTF-8 string view.
  BL_INLINE const BLStringView& familyNameView() const noexcept { return impl->familyName.view(); }

  //! Returns the font subfamily name as UTF-8 null-terminated string.
  BL_INLINE const char* subfamilyName() const noexcept { return impl->subfamilyName.data(); }
  //! Returns the size of the string returned by `subfamilyName()`.
  BL_INLINE size_t subfamilyNameSize() const noexcept { return impl->subfamilyName.size(); }
  //! Returns the font subfamily-name as a UTF-8 string view.
  BL_INLINE const BLStringView& subfamilyNameView() const noexcept { return impl->subfamilyName.view(); }

  //! Returns the font PostScript name as UTF-8 null-terminated string.
  BL_INLINE const char* postScriptName() const noexcept { return impl->postScriptName.data(); }
  //! Returns the size of the string returned by `postScriptName()`.
  BL_INLINE size_t postScriptNameSize() const noexcept { return impl->postScriptName.size(); }
  //! Returns the font PostScript name as a UTF-8 string view.
  BL_INLINE const BLStringView& postScriptNameView() const noexcept { return impl->postScriptName.view(); }

  // TODO:
  // Returns feature-set of this `BLFontFace`.
  // BL_INLINE const BLFontFeatureSet& featureSet() const noexcept { return impl->featureSet; }

  //! Returns design metrics of this `BLFontFace`.
  BL_INLINE const BLFontDesignMetrics& designMetrics() const noexcept { return impl->designMetrics; }
  //! Returns units per em, which are part of font's design metrics.
  BL_INLINE int unitsPerEm() const noexcept { return impl->designMetrics.unitsPerEm; }

  //! Returns PANOSE classification of this `BLFontFace`.
  BL_INLINE const BLFontPanose& panose() const noexcept { return impl->panose; }

  //! Returns unicode coverage of this `BLFontFace`.
  //!
  //! \note The returned unicode-coverage is not calculated by Blend2D so in
  //! general the value doesn't have to be correct. Use `getCharacterCoverage()`
  //! to get a coverage calculated by Blend2D at character granularity.
  BL_INLINE const BLFontUnicodeCoverage& unicodeCoverage() const noexcept { return impl->unicodeCoverage; }

  //! \}

  static BL_INLINE const BLFontFace& none() noexcept { return reinterpret_cast<const BLFontFace*>(blNone)[kImplType]; }
};
#endif

// ============================================================================
// [BLFont - Core]
// ============================================================================

//! Font [C Interface - Impl].
struct BLFontImpl {
  //! Font-face used by this font.
  BL_TYPED_MEMBER(BLFontFaceCore, BLFontFace, face);

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;

  //! Font width (1..1000) [0 if the font is not initialized].
  uint16_t weight;
  //! Font stretch (1..9) [0 if the font is not initialized].
  uint8_t stretch;
  //! Font style.
  uint8_t style;

  //! Font features.
  BL_TYPED_MEMBER(BLArrayCore, BLArray<BLFontFeature>, features);
  //! Font variations.
  BL_TYPED_MEMBER(BLArrayCore, BLArray<BLFontVariation>, variations);
  //! Font metrics.
  BLFontMetrics metrics;
  //! Font matrix.
  BLFontMatrix matrix;

  BL_HAS_TYPED_MEMBERS(BLFontImpl)
};

//! Font [C Interface - Core].
struct BLFontCore {
  BLFontImpl* impl;
};

// ============================================================================
// [BLFont - C++]
// ============================================================================

#ifdef __cplusplus
//! Font [C++ API].
class BLFont : public BLFontCore {
public:
  //! \cond INTERNAL
  static constexpr const uint32_t kImplType = BL_IMPL_TYPE_FONT;
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLFont() noexcept { this->impl = none().impl; }
  BL_INLINE BLFont(BLFont&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLFont(const BLFont& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLFont(BLFontImpl* impl) noexcept { this->impl = impl; }
  BL_INLINE ~BLFont() noexcept { blFontDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return !isNone(); }

  BL_INLINE BLFont& operator=(BLFont&& other) noexcept { blFontAssignMove(this, &other); return *this; }
  BL_INLINE BLFont& operator=(const BLFont& other) noexcept { blFontAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLFont& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLFont& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blFontReset(this); }
  BL_INLINE void swap(BLFont& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult assign(BLFont&& other) noexcept { return blFontAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLFont& other) noexcept { return blFontAssignWeak(this, &other); }

  //! Tests whether the font is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }
  //! Tests whether the font is empty (which the same as `isNone()` in this case).
  BL_INLINE bool empty() const noexcept { return isNone(); }

  BL_INLINE bool equals(const BLFont& other) const noexcept { return blFontEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  BL_INLINE BLResult createFromFace(const BLFontFace& face, float size) noexcept {
    return blFontCreateFromFace(this, &face, size);
  }

  //! \}

  //! \name Properties
  //! \{

  //! Returns the type of the font's associated font-face, see `BLFontFaceType`.
  BL_INLINE uint32_t faceType() const noexcept { return impl->face.faceType(); }
  //! Returns the flags of the font, see `BLFontFaceFlags`.
  BL_INLINE uint32_t faceFlags() const noexcept { return impl->face.faceFlags(); }
  //! Returns the size of the font (as float).
  BL_INLINE float size() const noexcept { return impl->metrics.size; }
  //! Returns the "units per em" (UPEM) of the font's associated font-face.
  BL_INLINE int unitsPerEm() const noexcept { return face().unitsPerEm(); }

  //! Returns the font's associated font-face.
  //!
  //! Returns the same font-face, which was passed to `createFromFace()`.
  BL_INLINE const BLFontFace& face() const noexcept { return impl->face; }

  //! Returns the features associated with the font.
  BL_INLINE const BLArray<BLFontFeature>& features() const noexcept { return impl->features; }
  //! Returns the variations associated with the font.
  BL_INLINE const BLArray<BLFontVariation>& variations() const noexcept { return impl->variations; }

  //! Returns the weight of the font.
  BL_INLINE uint32_t weight() const noexcept { return impl->weight; }
  //! Returns the stretch of the font.
  BL_INLINE uint32_t stretch() const noexcept { return impl->stretch; }
  //! Returns the style of the font.
  BL_INLINE uint32_t style() const noexcept { return impl->style; }

  //! Returns a 2x2 matrix of the font.
  //!
  //! The returned `BLFontMatrix` is used to scale fonts from design units
  //! into user units. The matrix usually has a negative `m11` member as
  //! fonts use a different coordinate system than Blend2D.
  BL_INLINE const BLFontMatrix& matrix() const noexcept { return impl->matrix; }

  //! Returns the scaled metrics of the font.
  //!
  //! The returned metrics is a scale of design metrics that match the font size and its options.
  BL_INLINE const BLFontMetrics& metrics() const noexcept { return impl->metrics; }

  //! Returns the design metrics of the font.
  //!
  //! The returned metrics is compatible with the metrics of `BLFontFace` associated with this font.
  BL_INLINE const BLFontDesignMetrics& designMetrics() const noexcept { return face().designMetrics(); }

  //! \}

  //! \name Glyphs & Text
  //! \{

  BL_INLINE BLResult shape(BLGlyphBuffer& gb) const noexcept {
    return blFontShape(this, &gb);
  }

  BL_INLINE BLResult mapTextToGlyphs(BLGlyphBuffer& gb) const noexcept {
    return blFontMapTextToGlyphs(this, &gb, nullptr);
  }

  BL_INLINE BLResult mapTextToGlyphs(BLGlyphBuffer& gb, BLGlyphMappingState& stateOut) const noexcept {
    return blFontMapTextToGlyphs(this, &gb, &stateOut);
  }

  BL_INLINE BLResult positionGlyphs(BLGlyphBuffer& gb, uint32_t positioningFlags = 0xFFFFFFFFu) const noexcept {
    return blFontPositionGlyphs(this, &gb, positioningFlags);
  }

  BL_INLINE BLResult applyKerning(BLGlyphBuffer& gb) const noexcept {
    return blFontApplyKerning(this, &gb);
  }

  BL_INLINE BLResult applyGSub(BLGlyphBuffer& gb, size_t index, BLBitWord lookups) const noexcept {
    return blFontApplyGSub(this, &gb, index, lookups);
  }

  BL_INLINE BLResult applyGPos(BLGlyphBuffer& gb, size_t index, BLBitWord lookups) const noexcept {
    return blFontApplyGPos(this, &gb, index, lookups);
  }

  BL_INLINE BLResult getTextMetrics(BLGlyphBuffer& gb, BLTextMetrics& out) const noexcept {
    return blFontGetTextMetrics(this, &gb, &out);
  }

  BL_INLINE BLResult getGlyphBounds(const uint32_t* glyphData, intptr_t glyphAdvance, BLBoxI* out, size_t count) const noexcept {
    return blFontGetGlyphBounds(this, glyphData, glyphAdvance, out, count);
  }

  BL_INLINE BLResult getGlyphAdvances(const uint32_t* glyphData, intptr_t glyphAdvance, BLGlyphPlacement* out, size_t count) const noexcept {
    return blFontGetGlyphAdvances(this, glyphData, glyphAdvance, out, count);
  }

  BL_INLINE BLResult getGlyphOutlines(uint32_t glyphId, BLPath& out, BLPathSinkFunc sink = nullptr, void* closure = nullptr) const noexcept {
    return blFontGetGlyphOutlines(this, glyphId, nullptr, &out, sink, closure);
  }

  BL_INLINE BLResult getGlyphOutlines(uint32_t glyphId, const BLMatrix2D& userMatrix, BLPath& out, BLPathSinkFunc sink = nullptr, void* closure = nullptr) const noexcept {
    return blFontGetGlyphOutlines(this, glyphId, &userMatrix, &out, sink, closure);
  }

  BL_INLINE BLResult getGlyphRunOutlines(const BLGlyphRun& glyphRun, BLPath& out, BLPathSinkFunc sink = nullptr, void* closure = nullptr) const noexcept {
    return blFontGetGlyphRunOutlines(this, &glyphRun, nullptr, &out, sink, closure);
  }

  BL_INLINE BLResult getGlyphRunOutlines(const BLGlyphRun& glyphRun, const BLMatrix2D& userMatrix, BLPath& out, BLPathSinkFunc sink = nullptr, void* closure = nullptr) const noexcept {
    return blFontGetGlyphRunOutlines(this, &glyphRun, &userMatrix, &out, sink, closure);
  }

  //! \}

  static BL_INLINE const BLFont& none() noexcept { return reinterpret_cast<const BLFont*>(blNone)[kImplType]; }
};
#endif

//! \}

#endif // BLEND2D_FONT_H_INCLUDED
