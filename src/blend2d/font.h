// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONT_H_INCLUDED
#define BLEND2D_FONT_H_INCLUDED

#include "array.h"
#include "bitset.h"
#include "filesystem.h"
#include "fontdefs.h"
#include "geometry.h"
#include "glyphbuffer.h"
#include "object.h"
#include "path.h"
#include "string.h"

//! \addtogroup blend2d_api_text
//! \{

//! \name BLFont - C API
//! \{

//! Font data [C API].
struct BLFontDataCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
};

//! Font face [C API].
struct BLFontFaceCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
};

//! Font [C API].
struct BLFontCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
};

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blFontInit(BLFontCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontInitMove(BLFontCore* self, BLFontCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontInitWeak(BLFontCore* self, const BLFontCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontDestroy(BLFontCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontReset(BLFontCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontAssignMove(BLFontCore* self, BLFontCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontAssignWeak(BLFontCore* self, const BLFontCore* other) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blFontEquals(const BLFontCore* a, const BLFontCore* b) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontCreateFromFace(BLFontCore* self, const BLFontFaceCore* face, float size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontShape(const BLFontCore* self, BLGlyphBufferCore* gb) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontMapTextToGlyphs(const BLFontCore* self, BLGlyphBufferCore* gb, BLGlyphMappingState* stateOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontPositionGlyphs(const BLFontCore* self, BLGlyphBufferCore* gb, uint32_t positioningFlags) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontApplyKerning(const BLFontCore* self, BLGlyphBufferCore* gb) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontApplyGSub(const BLFontCore* self, BLGlyphBufferCore* gb, const BLBitSetCore* lookups) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontApplyGPos(const BLFontCore* self, BLGlyphBufferCore* gb, const BLBitSetCore* lookups) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetMatrix(const BLFontCore* self, BLFontMatrix* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetMetrics(const BLFontCore* self, BLFontMetrics* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetDesignMetrics(const BLFontCore* self, BLFontDesignMetrics* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetTextMetrics(const BLFontCore* self, BLGlyphBufferCore* gb, BLTextMetrics* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetGlyphBounds(const BLFontCore* self, const uint32_t* glyphData, intptr_t glyphAdvance, BLBoxI* out, size_t count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetGlyphAdvances(const BLFontCore* self, const uint32_t* glyphData, intptr_t glyphAdvance, BLGlyphPlacement* out, size_t count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetGlyphOutlines(const BLFontCore* self, uint32_t glyphId, const BLMatrix2D* userMatrix, BLPathCore* out, BLPathSinkFunc sink, void* closure) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontGetGlyphRunOutlines(const BLFontCore* self, const BLGlyphRun* glyphRun, const BLMatrix2D* userMatrix, BLPathCore* out, BLPathSinkFunc sink, void* closure) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blFontDataInit(BLFontDataCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontDataInitMove(BLFontDataCore* self, BLFontDataCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontDataInitWeak(BLFontDataCore* self, const BLFontDataCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontDataDestroy(BLFontDataCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontDataReset(BLFontDataCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontDataAssignMove(BLFontDataCore* self, BLFontDataCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontDataAssignWeak(BLFontDataCore* self, const BLFontDataCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontDataCreateFromFile(BLFontDataCore* self, const char* fileName, BLFileReadFlags readFlags) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontDataCreateFromDataArray(BLFontDataCore* self, const BLArrayCore* dataArray) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontDataCreateFromData(BLFontDataCore* self, const void* data, size_t dataSize, BLDestroyExternalDataFunc destroyFunc, void* userData) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blFontDataEquals(const BLFontDataCore* a, const BLFontDataCore* b) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontDataListTags(const BLFontDataCore* self, uint32_t faceIndex, BLArrayCore* dst) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL blFontDataQueryTables(const BLFontDataCore* self, uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t count) BL_NOEXCEPT_C;

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
BL_API BLResult BL_CDECL blFontFaceGetDesignMetrics(const BLFontFaceCore* self, BLFontDesignMetrics* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetUnicodeCoverage(const BLFontFaceCore* self, BLFontUnicodeCoverage* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFaceGetCharacterCoverage(const BLFontFaceCore* self, BLBitSetCore* out) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! \}

//! \cond INTERNAL
//! \name BLFont - Internals
//! \{

//! Font data [Virtual Function Table].
struct BLFontDataVirt BL_CLASS_INHERITS(BLObjectVirt) {
  BL_DEFINE_VIRT_BASE
  BLResult (BL_CDECL* listTags)(const BLFontDataImpl* impl, uint32_t faceIndex, BLArrayCore* out) BL_NOEXCEPT;
  size_t (BL_CDECL* queryTables)(const BLFontDataImpl* impl, uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t n) BL_NOEXCEPT;
};

//! Font data [Impl].
struct BLFontDataImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Virtual function table.
  const BLFontDataVirt* virt;

  //! Type of the face that would be created with this font-data.
  uint8_t faceType;

  //! Number of font-faces stored in this font-data instance.
  uint32_t faceCount;
  //! Font-data flags.
  uint32_t flags;
};

//! \}
//! \endcond

//! \name BLFont - C++ API
//! \{

#ifdef __cplusplus

//! Font data [C++ API].
class BLFontData : public BLFontDataCore {
public:
  //! \cond INTERNAL
  BL_INLINE BLFontDataImpl* _impl() const noexcept { return static_cast<BLFontDataImpl*>(_d.impl); }
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLFontData() noexcept { blFontDataInit(this); }
  BL_INLINE BLFontData(BLFontData&& other) noexcept { blFontDataInitMove(this, &other); }
  BL_INLINE BLFontData(const BLFontData& other) noexcept { blFontDataInitWeak(this, &other); }
  BL_INLINE ~BLFontData() noexcept { blFontDataDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return isValid(); }

  BL_INLINE BLFontData& operator=(BLFontData&& other) noexcept { blFontDataAssignMove(this, &other); return *this; }
  BL_INLINE BLFontData& operator=(const BLFontData& other) noexcept { blFontDataAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLFontData& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLFontData& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blFontDataReset(this); }
  BL_INLINE void swap(BLFontData& other) noexcept { _d.swap(other._d); }

  BL_INLINE BLResult assign(BLFontData&& other) noexcept { return blFontDataAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLFontData& other) noexcept { return blFontDataAssignWeak(this, &other); }

  //! Tests whether the font-data is a built-in null instance.
  BL_INLINE bool isValid() const noexcept { return _impl()->faceCount != 0; }
  //! Tests whether the font-data is empty, which the same as `!isValid()`.
  BL_INLINE bool empty() const noexcept { return !isValid(); }

  BL_INLINE bool equals(const BLFontData& other) const noexcept { return blFontDataEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  //! Creates a `BLFontData` from a file specified by the given `fileName`.
  //!
  //! \remarks The `readFlags` argument allows to specify flags that will be passed to `BLFileSystem::readFile()` to
  //! read the content of the file. It's possible to use memory mapping to get its content, which is the recommended
  //! way for reading system fonts. The best combination is to use `BL_FILE_READ_MMAP_ENABLED` flag combined with
  //! `BL_FILE_READ_MMAP_AVOID_SMALL`. This combination means to try to use memory mapping only when the size of the
  //! font is greater than a minimum value (determined by Blend2D), and would fallback to a regular open/read in case
  //! the memory mapping is not possible or failed for some other reason. Please note that not all files can be memory
  //! mapped so `BL_FILE_READ_MMAP_NO_FALLBACK` flag is not recommended.
  BL_INLINE BLResult createFromFile(const char* fileName, BLFileReadFlags readFlags = BL_FILE_READ_NO_FLAGS) noexcept {
    return blFontDataCreateFromFile(this, fileName, readFlags);
  }

  //! Creates a `BLFontData` from the given `data` stored in `BLArray<uint8_t>`
  //!
  //! The given `data` would be weak copied on success so the given array can be safely destroyed after the function
  //! returns.
  //!
  //! \remarks The weak copy of the passed `data` is internal and there is no API to access it after the function
  //! returns. The reason for making it internal is that multiple implementations of `BLFontData` may exist and some
  //! can only store data at table level, so Blend2D doesn't expose the detail about how the data is stored.
  BL_INLINE BLResult createFromData(const BLArray<uint8_t>& data) noexcept {
    return blFontDataCreateFromDataArray(this, &data);
  }

  //! Creates ` BLFontData` from the given `data` of the given `size`.
  //!
  //! \note Optionally a `destroyFunc` can be used as a notifier that will be called when the data is no longer needed.
  //! Destroy func will be called with `userData`.
  BL_INLINE BLResult createFromData(const void* data, size_t dataSize, BLDestroyExternalDataFunc destroyFunc = nullptr, void* userData = nullptr) noexcept {
    return blFontDataCreateFromData(this, data, dataSize, destroyFunc, userData);
  }

  //! \}

  //! \name Accessors
  //! \{

  //! Type of font-face that this data describes.
  //!
  //! It doesn't matter if the content is a single font or a collection. In any case the `faceType()` would always
  //! return the type of the font-face that will be created by `BLFontFace::createFromData()`.
  BL_INLINE uint32_t faceType() const noexcept { return _impl()->faceType; }

  //! Returns the number of faces of this font-data.
  //!
  //! If the data is not initialized the result would be always zero. If the data is initialized to a single font it
  //! would be 1, and if the data is initialized to a font collection then the return would correspond to the number
  //! of font-faces within that collection.
  //!
  //! \note You should not use `faceCount()` to check whether the font is a collection as it's possible to have a
  //! font-collection with just a single font. Using `isCollection()` is more reliable and would always return the
  //! right value.
  BL_INLINE uint32_t faceCount() const noexcept { return _impl()->faceCount; }

  //! Returns font-data flags, see `BLFontDataFlags`.
  BL_INLINE uint32_t flags() const noexcept { return _impl()->flags; }

  //! Tests whether this font-data is a font-collection.
  BL_INLINE bool isCollection() const noexcept { return (_impl()->flags & BL_FONT_DATA_FLAG_COLLECTION) != 0; }

  //! \}

  //! \name Query Functionality
  //! \{

  BL_INLINE BLResult listTags(uint32_t faceIndex, BLArray<BLTag>& dst) const noexcept {
    // The same as blFontDataListTags() [C API].
    return _impl()->virt->listTags(_impl(), faceIndex, &dst);
  }

  BL_INLINE size_t queryTable(uint32_t faceIndex, BLFontTable* dst, BLTag tag) const noexcept {
    // The same as blFontDataQueryTables() [C API].
    return _impl()->virt->queryTables(_impl(), faceIndex, dst, &tag, 1);
  }

  BL_INLINE size_t queryTables(uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t count) const noexcept {
    // The same as blFontDataQueryTables() [C API].
    return _impl()->virt->queryTables(_impl(), faceIndex, dst, tags, count);
  }

  //! \}
};

#endif
//! \}

//! \cond INTERNAL
//! \name BLFont - Internals
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

//! \}
//! \endcond

//! \name BLFont - C++ API
//! \{

#ifdef __cplusplus
//! Font face [C++ API].
class BLFontFace : public BLFontFaceCore {
public:
  //! \cond INTERNAL
  BL_INLINE BLFontFaceImpl* _impl() const noexcept { return static_cast<BLFontFaceImpl*>(_d.impl); }
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
  BL_INLINE const BLFontData& data() const noexcept { return _impl()->data; }

  //! Returns a full of the font.
  BL_INLINE const BLString& fullName() const noexcept { return _impl()->fullName; }
  //! Returns a family name of the font.
  BL_INLINE const BLString& familyName() const noexcept { return _impl()->familyName; }
  //! Returns a subfamily name of the font.
  BL_INLINE const BLString& subfamilyName() const noexcept { return _impl()->subfamilyName; }
  //! Returns a PostScript name of the font.
  BL_INLINE const BLString& postScriptName() const noexcept { return _impl()->postScriptName; }

  //! Returns design metrics of this `BLFontFace`.
  BL_INLINE const BLFontDesignMetrics& designMetrics() const noexcept { return _impl()->designMetrics; }
  //! Returns units per em, which are part of font's design metrics.
  BL_INLINE int unitsPerEm() const noexcept { return _impl()->designMetrics.unitsPerEm; }

  //! Returns PANOSE classification of this `BLFontFace`.
  BL_INLINE const BLFontPanose& panose() const noexcept { return _impl()->panose; }

  //! Returns unicode coverage of this `BLFontFace`.
  //!
  //! \note The returned unicode-coverage is not calculated by Blend2D so in
  //! general the value doesn't have to be correct. Use `getCharacterCoverage()`
  //! to get a coverage calculated by Blend2D at character granularity.
  BL_INLINE const BLFontUnicodeCoverage& unicodeCoverage() const noexcept { return _impl()->unicodeCoverage; }

  //! Calculates the character coverage of this `BLFontFace`.
  //!
  //! Each unicode character is represented by a single bit in the given BitSet.
  BL_INLINE BLResult getCharacterCoverage(BLBitSetCore* out) const noexcept { return blFontFaceGetCharacterCoverage(this, out); }

  //! \}
};

#endif
//! \}

//! \cond INTERNAL
//! \name BLFont - Internals
//! \{

//! Font [Impl].
struct BLFontImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Font-face used by this font.
  BL_TYPED_MEMBER(BLFontFaceCore, BLFontFace, face);

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

//! \}
//! \endcond

//! \name BLFont - C++ API
//! \{

#ifdef __cplusplus

//! Font [C++ API].
class BLFont : public BLFontCore {
public:
  //! \cond INTERNAL
  BL_INLINE BLFontImpl* _impl() const noexcept { return static_cast<BLFontImpl*>(_d.impl); }
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLFont() noexcept { blFontInit(this); }
  BL_INLINE BLFont(BLFont&& other) noexcept { blFontInitMove(this, &other); }
  BL_INLINE BLFont(const BLFont& other) noexcept { blFontInitWeak(this, &other); }
  BL_INLINE ~BLFont() noexcept { blFontDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return isValid(); }

  BL_INLINE BLFont& operator=(BLFont&& other) noexcept { blFontAssignMove(this, &other); return *this; }
  BL_INLINE BLFont& operator=(const BLFont& other) noexcept { blFontAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLFont& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLFont& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blFontReset(this); }
  BL_INLINE void swap(BLFont& other) noexcept { _d.swap(other._d); }

  BL_INLINE BLResult assign(BLFont&& other) noexcept { return blFontAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLFont& other) noexcept { return blFontAssignWeak(this, &other); }

  //! Tests whether the font is a valid instance.
  BL_INLINE bool isValid() const noexcept { return _impl()->face.isValid(); }
  //! Tests whether the font is empty, which the same as `!isValid()`.
  BL_INLINE bool empty() const noexcept { return !isValid(); }

  BL_INLINE bool equals(const BLFontCore& other) const noexcept { return blFontEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  BL_INLINE BLResult createFromFace(const BLFontFaceCore& face, float size) noexcept {
    return blFontCreateFromFace(this, &face, size);
  }

  //! \}

  //! \name Properties
  //! \{

  //! Returns the type of the font's associated font-face, see `BLFontFaceType`.
  BL_INLINE uint32_t faceType() const noexcept { return _impl()->face.faceType(); }
  //! Returns the flags of the font, see `BLFontFaceFlags`.
  BL_INLINE uint32_t faceFlags() const noexcept { return _impl()->face.faceFlags(); }
  //! Returns the size of the font (as float).
  BL_INLINE float size() const noexcept { return _impl()->metrics.size; }
  //! Returns the "units per em" (UPEM) of the font's associated font-face.
  BL_INLINE int unitsPerEm() const noexcept { return face().unitsPerEm(); }

  //! Returns the font's associated font-face.
  //!
  //! Returns the same font-face, which was passed to `createFromFace()`.
  BL_INLINE const BLFontFace& face() const noexcept { return _impl()->face; }

  //! Returns the features associated with the font.
  BL_INLINE const BLArray<BLFontFeature>& features() const noexcept { return _impl()->features; }
  //! Returns the variations associated with the font.
  BL_INLINE const BLArray<BLFontVariation>& variations() const noexcept { return _impl()->variations; }

  //! Returns the weight of the font.
  BL_INLINE uint32_t weight() const noexcept { return _impl()->weight; }
  //! Returns the stretch of the font.
  BL_INLINE uint32_t stretch() const noexcept { return _impl()->stretch; }
  //! Returns the style of the font.
  BL_INLINE uint32_t style() const noexcept { return _impl()->style; }

  //! Returns a 2x2 matrix of the font.
  //!
  //! The returned `BLFontMatrix` is used to scale fonts from design units
  //! into user units. The matrix usually has a negative `m11` member as
  //! fonts use a different coordinate system than Blend2D.
  BL_INLINE const BLFontMatrix& matrix() const noexcept { return _impl()->matrix; }

  //! Returns the scaled metrics of the font.
  //!
  //! The returned metrics is a scale of design metrics that match the font size and its options.
  BL_INLINE const BLFontMetrics& metrics() const noexcept { return _impl()->metrics; }

  //! Returns the design metrics of the font.
  //!
  //! The returned metrics is compatible with the metrics of `BLFontFace` associated with this font.
  BL_INLINE const BLFontDesignMetrics& designMetrics() const noexcept { return face().designMetrics(); }

  //! \}

  //! \name Glyphs & Text
  //! \{

  BL_INLINE BLResult shape(BLGlyphBufferCore& gb) const noexcept {
    return blFontShape(this, &gb);
  }

  BL_INLINE BLResult mapTextToGlyphs(BLGlyphBufferCore& gb) const noexcept {
    return blFontMapTextToGlyphs(this, &gb, nullptr);
  }

  BL_INLINE BLResult mapTextToGlyphs(BLGlyphBufferCore& gb, BLGlyphMappingState& stateOut) const noexcept {
    return blFontMapTextToGlyphs(this, &gb, &stateOut);
  }

  BL_INLINE BLResult positionGlyphs(BLGlyphBufferCore& gb, uint32_t positioningFlags = 0xFFFFFFFFu) const noexcept {
    return blFontPositionGlyphs(this, &gb, positioningFlags);
  }

  BL_INLINE BLResult applyKerning(BLGlyphBufferCore& gb) const noexcept {
    return blFontApplyKerning(this, &gb);
  }

  BL_INLINE BLResult applyGSub(BLGlyphBufferCore& gb, const BLBitSetCore& lookups) const noexcept {
    return blFontApplyGSub(this, &gb, &lookups);
  }

  BL_INLINE BLResult applyGPos(BLGlyphBufferCore& gb, const BLBitSetCore& lookups) const noexcept {
    return blFontApplyGPos(this, &gb, &lookups);
  }

  BL_INLINE BLResult getTextMetrics(BLGlyphBufferCore& gb, BLTextMetrics& out) const noexcept {
    return blFontGetTextMetrics(this, &gb, &out);
  }

  BL_INLINE BLResult getGlyphBounds(const uint32_t* glyphData, intptr_t glyphAdvance, BLBoxI* out, size_t count) const noexcept {
    return blFontGetGlyphBounds(this, glyphData, glyphAdvance, out, count);
  }

  BL_INLINE BLResult getGlyphAdvances(const uint32_t* glyphData, intptr_t glyphAdvance, BLGlyphPlacement* out, size_t count) const noexcept {
    return blFontGetGlyphAdvances(this, glyphData, glyphAdvance, out, count);
  }

  BL_INLINE BLResult getGlyphOutlines(uint32_t glyphId, BLPathCore& out, BLPathSinkFunc sink = nullptr, void* closure = nullptr) const noexcept {
    return blFontGetGlyphOutlines(this, glyphId, nullptr, &out, sink, closure);
  }

  BL_INLINE BLResult getGlyphOutlines(uint32_t glyphId, const BLMatrix2D& userMatrix, BLPathCore& out, BLPathSinkFunc sink = nullptr, void* closure = nullptr) const noexcept {
    return blFontGetGlyphOutlines(this, glyphId, &userMatrix, &out, sink, closure);
  }

  BL_INLINE BLResult getGlyphRunOutlines(const BLGlyphRun& glyphRun, BLPathCore& out, BLPathSinkFunc sink = nullptr, void* closure = nullptr) const noexcept {
    return blFontGetGlyphRunOutlines(this, &glyphRun, nullptr, &out, sink, closure);
  }

  BL_INLINE BLResult getGlyphRunOutlines(const BLGlyphRun& glyphRun, const BLMatrix2D& userMatrix, BLPathCore& out, BLPathSinkFunc sink = nullptr, void* closure = nullptr) const noexcept {
    return blFontGetGlyphRunOutlines(this, &glyphRun, &userMatrix, &out, sink, closure);
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_FONT_H_INCLUDED
