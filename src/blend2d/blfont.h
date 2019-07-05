// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_BLFONT_H
#define BLEND2D_BLFONT_H

#include "./blarray.h"
#include "./blfontdefs.h"
#include "./blgeometry.h"
#include "./blglyphbuffer.h"
#include "./blpath.h"
#include "./blstring.h"
#include "./blvariant.h"

//! \addtogroup blend2d_api_text
//! \{

// ============================================================================
// [BLFontData - Core]
// ============================================================================

//! Font data [C Interface - Virtual Function Table].
struct BLFontDataVirt {
  BLResult (BL_CDECL* destroy)(BLFontDataImpl* impl) BL_NOEXCEPT;
  BLResult (BL_CDECL* listTags)(const BLFontDataImpl* impl, BLArrayCore* out) BL_NOEXCEPT;
  size_t (BL_CDECL* queryTables)(const BLFontDataImpl* impl, BLFontTable* dst, const BLTag* tags, size_t n) BL_NOEXCEPT;
};

//! Font data [C Interface - Impl].
struct BLFontDataImpl {
  //! Virtual function table.
  const BLFontDataVirt* virt;
  //! Pointer to the start of font-data (null if the data is provided at a table level).
  void* data;
  //! Size of `data` [in bytes] (zero if the data is provided at a table level).
  size_t size;

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;
  //! Font-data flags.
  uint32_t flags;
  //! Offset to the font header from the beginning of `data`.
  size_t headerOffset;
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

  //! Creates `BLFontData` from loader.
  //!
  //! This function does the same as `BLFontLoader::dataByFaceIndex(faceIndex)`.
  //! If the `loader` is a font collection then `faceIndex` specifies the index
  //! of the font in the collection, otherwise it must be zero.
  BL_INLINE BLFontData(const BLFontLoader& loader, uint32_t faceIndex) noexcept;

  BL_INLINE ~BLFontData() noexcept { blFontDataReset(this); }

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

  //! Creates `BLFontData` from loader.
  //!
  //! This function does the same as `BLFontLoader::dataByFaceIndex(faceIndex)`.
  //! If the `loader` is a font collection then `faceIndex` specifies the index
  //! of the font in the collection, otherwise it must be zero.
  BL_INLINE BLResult createFromLoader(const BLFontLoader& loader, uint32_t faceIndex) noexcept;

  //! \}

  //! \name Font-Data Accessors
  //! \{

  BL_INLINE BLResult listTags(BLArray<BLTag>& dst) const noexcept {
    // The same as blFontDataListTags() [C-API].
    return impl->virt->listTags(impl, &dst);
  }

  BL_INLINE size_t queryTable(BLFontTable* dst, BLTag tag) const noexcept {
    // The same as blFontDataQueryTables() [C-API].
    return impl->virt->queryTables(impl, dst, &tag, 1);
  }

  BL_INLINE size_t queryTables(BLFontTable* dst, const BLTag* tags, size_t count) const noexcept {
    // The same as blFontDataQueryTables() [C-API].
    return impl->virt->queryTables(impl, dst, tags, count);
  }

  //! \}

  static BL_INLINE const BLFontData& none() noexcept { return reinterpret_cast<const BLFontData*>(blNone)[kImplType]; }
};
#endif

// ============================================================================
// [BLFontLoader - Core]
// ============================================================================

//! Font loader [C Interface - Virtual Function Table].
struct BLFontLoaderVirt {
  BLResult (BL_CDECL* destroy)(BLFontLoaderImpl* impl) BL_NOEXCEPT;
  BLFontDataImpl* (BL_CDECL* dataByFaceIndex)(BLFontLoaderImpl* impl, uint32_t faceIndex) BL_NOEXCEPT;
};

//! Font loader [C Interface - Impl].
struct BLFontLoaderImpl {
  //! Virtual function table.
  const BLFontLoaderVirt* virt;
  //! Pointer to the start of font-data (null if the data is provided at table level).
  void* data;
  //! Size of `data` [in bytes] (zero if the data is provided at table level).
  size_t size;

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;

  uint8_t faceType;
  uint8_t reserved[3];
  uint32_t faceCount;
  uint32_t loaderFlags;
};

//! Font loader [C Interface - Core].
struct BLFontLoaderCore {
  BLFontLoaderImpl* impl;
};

// ============================================================================
// [BLFontLoader - C++]
// ============================================================================

#ifdef __cplusplus
//! Font loader [C++ API].
class BLFontLoader : public BLFontLoaderCore {
public:
  //! \cond INTERNAL
  static constexpr const uint32_t kImplType = BL_IMPL_TYPE_FONT_LOADER;
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLFontLoader() noexcept { this->impl = none().impl; }
  BL_INLINE BLFontLoader(BLFontLoader&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLFontLoader(const BLFontLoader& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLFontLoader(BLFontLoaderImpl* impl) noexcept { this->impl = impl; }
  BL_INLINE ~BLFontLoader() noexcept { blFontLoaderReset(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return !isNone(); }

  BL_INLINE BLFontLoader& operator=(BLFontLoader&& other) noexcept { blFontLoaderAssignMove(this, &other); return *this; }
  BL_INLINE BLFontLoader& operator=(const BLFontLoader& other) noexcept { blFontLoaderAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLFontLoader& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLFontLoader& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blFontLoaderReset(this); }
  BL_INLINE void swap(BLFontLoader& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult assign(BLFontLoader&& other) noexcept { return blFontLoaderAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLFontLoader& other) noexcept { return blFontLoaderAssignWeak(this, &other); }

  //! Tests whether the font-loader is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }
  //! Tests whether the font-loader is empty (which the same as `isNone()` in this case).
  BL_INLINE bool empty() const noexcept { return isNone(); }

  BL_INLINE bool equals(const BLFontLoader& other) const noexcept { return blFontLoaderEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  //! Creates a `BLFontLoader` from file specified by `fileName`.
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
    return blFontLoaderCreateFromFile(this, fileName, readFlags);
  }

  //! Creates a `BLFontLoader` from the given `data` stored in `BLArray<uint8_t>`
  //!
  //! The loader makes a weak copy of the `data` if successful so the given array
  //! can be destroyed after the function returns.
  //!
  //! \remarks The weak copy of the passed `data` is internal and there is no API
  //! to access it after the function returns. The reason for making it internal
  //! is that the loader is only responsible for providing `BLFontData`, which
  //! doesn't have to store the data in a continuous buffer.
  BL_INLINE BLResult createFromData(const BLArray<uint8_t>& data) noexcept {
    return blFontLoaderCreateFromDataArray(this, &data);
  }

  //! Creates ` \BLFontLoader` from the given `data` of the given `size`.
  //!
  //! \note Optionally a `destroyFunc` can be used as a notifier that will be
  //! called when the data is no longer needed and `destroyData` acts as a user
  //! data passed to `destroyFunc()`. Please note that all fonts created by the
  //! loader would also reference the loader so `destroyFunc` will only be called
  //! when there are no references to the loader possibly held by fonts or other
  //! objects.
  BL_INLINE BLResult createFromData(const void* data, size_t size, BLDestroyImplFunc destroyFunc = nullptr, void* destroyData = nullptr) noexcept {
    return blFontLoaderCreateFromData(this, data, size, destroyFunc, destroyData);
  }

  //! \}

  //! \name Properties
  //! \{

  //! Type of font-face of the loader content.
  //!
  //! It doesn't matter if the content is a single font or a collection. In
  //! any case the `faceType()` would always return the type of the font-face
  //! that will be created by `BLFontFace::createFromLoader()`.
  BL_INLINE uint32_t faceType() const noexcept { return impl->faceType; }

  //! Returns the number of faces this loader provides.
  //!
  //! If the loader is not initialized the result would be always zero. If the
  //! loader is initialized to a single font it would be 1, and if the loader
  //! is initialized to a font collection then the return would correspond to
  //! the number of font-faces within that collection.
  BL_INLINE uint32_t faceCount() const noexcept { return impl->faceCount; }

  //! Returns loader flags, see `BLFontLoaderFlags`.
  BL_INLINE uint32_t loaderFlags() const noexcept { return impl->loaderFlags; }

  //! \}

  //! \name Font-Data Access
  //! \{

  //! Returns `BLFontData` instance that matches the given `faceIndex`.
  //!
  //! Please note that this function never fails. If the `faceIndex` is out
  //! of bounds then a default constructed instance of `BLFontData` will be
  //! returned.
  //!
  //! Alternatively you can use `BLFontData::createFromLoader()`, which would
  //! call virtual function `BLFontLoaderVirt::dataByFaceIndex()` behind the
  //! scenes and return the appropriate error if `faceIndex` is invalid or if
  //! the loader is not initialized.
  BL_INLINE BLFontData dataByFaceIndex(uint32_t faceIndex) const noexcept {
    return BLFontData(impl->virt->dataByFaceIndex(impl, faceIndex));
  }

  //! \}

  static BL_INLINE const BLFontLoader& none() noexcept { return reinterpret_cast<const BLFontLoader*>(blNone)[kImplType]; }
};

// BLFontData API that depends on BLFontLoader.
//! \cond
BL_INLINE BLFontData::BLFontData(const BLFontLoader& loader, uint32_t faceIndex) noexcept {
  blFontDataInitFromLoader(this, &loader, faceIndex);
}

BL_INLINE BLResult BLFontData::createFromLoader(const BLFontLoader& loader, uint32_t faceIndex) noexcept {
  return blFontDataCreateFromLoader(this, &loader, faceIndex);
}
//! \endcond
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
  //! Font data.
  BL_TYPED_MEMBER(BLFontDataCore, BLFontData, data);
  //! Font loader used to load `BLFontData`.
  BL_TYPED_MEMBER(BLFontLoaderCore, BLFontLoader, loader);

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

  //! Unique face id assigned by Blend2D for caching.
  uint64_t faceUniqueId;

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
  BL_INLINE ~BLFontFace() noexcept { blFontFaceReset(this); }

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
  //! This is a utility function that first creates a `BLFontLoader` and then
  //! calls `createFromLoader(loader, 0)`. See `BLFontLoader::createFromFile()`
  //! for more details, especially the use of `readFlags` is important for system
  //! fonts.
  //!
  //! \note This function offers a simplified creation of `BLFontFace` directly
  //! from a file, but doesn't provide as much flexibility as `createFromLoader()`
  //! as it allows to specify a `faceIndex`, which can be used to load multiple
  //! font faces from a TrueType/OpenType collection. The use of `createFromLoader()`
  //! is recommended for any serious font handling.
  BL_INLINE BLResult createFromFile(const char* fileName, uint32_t readFlags = 0) noexcept {
    return blFontFaceCreateFromFile(this, fileName, readFlags);
  }

  //! Creates a new `BLFontFace` from `BLFontLoader`.
  //!
  //! On success the existing `BLFontFace` is completely replaced by a new one,
  //! on failure a `BLResult` is returned and the existing `BLFontFace` is kept
  //! as is. In other words, it either succeeds and replaces the `BLFontFaceImpl`
  //! or returns an error without touching the existing one.
  BL_INLINE BLResult createFromLoader(const BLFontLoader& loader, uint32_t faceIndex) noexcept {
    return blFontFaceCreateFromLoader(this, &loader, faceIndex);
  }

  //! \}

  //! \name Font Data & Loader
  //! \{

  //! Returns `BLFontData` associated with this font-face.
  BL_INLINE const BLFontData& data() const noexcept { return impl->data; }
  //! Returns `BLFontLoader` associated with this font-face.
  BL_INLINE const BLFontLoader& loader() const noexcept { return impl->loader; }

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
  //! Returns font-face diagnostics flags, see `BLFontFaceDiagFlags`.
  BL_INLINE uint32_t diagFlags() const noexcept { return impl->faceInfo.diagFlags; }

  //! Returns a unique identifier describing this `BLFontFace`.
  BL_INLINE uint64_t faceUniqueId() const noexcept { return impl->faceUniqueId; }

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

  //! Returns feature-set of this `BLFontFace`.
  // BL_INLINE const FontFeatureSet& featureSet() const noexcept { return impl->featureSet; }
  //! Returns design metrics of this `BLFontFace`.
  BL_INLINE const BLFontDesignMetrics& designMetrics() const noexcept { return impl->designMetrics; }
  //! Returns units per em, which are part of font's design metrics.
  BL_INLINE int unitsPerEm() const noexcept { return impl->designMetrics.unitsPerEm; }

  //! Returns PANOSE classification of this `BLFontFace`.
  BL_INLINE const BLFontPanose& panose() const noexcept { return impl->panose; }
  //! Returns unicode coverage of this `BLFontFace`.
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
  //! Font features.
  BL_TYPED_MEMBER(BLArrayCore, BLArray<BLFontFeature>, features);
  //! Font variations.
  BL_TYPED_MEMBER(BLArrayCore, BLArray<BLFontVariation>, variations);

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

  BLFontMetrics metrics;
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
  BL_INLINE ~BLFont() noexcept { blFontReset(this); }

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

  BL_INLINE BLResult getGlyphBounds(const void* glyphIdData, intptr_t glyphIdAdvance, BLBoxI* out, size_t count) const noexcept {
    return blFontGetGlyphBounds(this, glyphIdData, glyphIdAdvance, out, count);
  }

  BL_INLINE BLResult getGlyphAdvances(const void* glyphIdData, intptr_t glyphIdAdvance, BLGlyphPlacement* out, size_t count) const noexcept {
    return blFontGetGlyphAdvances(this, glyphIdData, glyphIdAdvance, out, count);
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

#endif // BLEND2D_BLFONT_H
