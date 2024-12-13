// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTDATA_H_INCLUDED
#define BLEND2D_FONTDATA_H_INCLUDED

#include "array.h"
#include "filesystem.h"
#include "fontdefs.h"
#include "object.h"
#include "string.h"

//! \addtogroup bl_text
//! \{

//! \name BLFontData - Constants
//! \{

//! Flags used by \ref BLFontData (or \ref BLFontDataCore).
BL_DEFINE_ENUM(BLFontDataFlags) {
  //! No flags.
  BL_FONT_DATA_NO_FLAGS = 0u,
  //!< Font data references a font-collection.
  BL_FONT_DATA_FLAG_COLLECTION = 0x00000001u

  BL_FORCE_ENUM_UINT32(BL_FONT_DATA_FLAG)
};

//! \}

//! \name BLFontData - Structs
//! \{

//! A read only data that represents a font table or its sub-table.
struct BLFontTable {
  //! \name Members
  //! \{

  //! Pointer to the beginning of the data interpreted as `uint8_t*`.
  const uint8_t* data;
  //! Size of `data` in bytes.
  size_t size;

  //! \}

#ifdef __cplusplus
  //! \name Common Functionality
  //! \{

  //! Tests whether the table has a content.
  //!
  //! \note This is essentially the opposite of `empty()`.
  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return size != 0; }

  //! Tests whether the table is empty (has no content).
  BL_INLINE_NODEBUG bool empty() const noexcept { return !size; }

  BL_INLINE_NODEBUG void reset() noexcept { *this = BLFontTable{}; }

  BL_INLINE_NODEBUG void reset(const uint8_t* data_, size_t size_) noexcept {
    data = data_;
    size = size_;
  }

  //! \}

  //! \name Accessors
  //! \{

  template<typename T>
  BL_INLINE_NODEBUG const T* dataAs() const noexcept { return reinterpret_cast<const T*>(data); }

  //! \}
#endif
};

//! \}
//! \}

//! \addtogroup bl_c_api
//! \{

//! \name BLFontData - C API
//! \{

//! Font data [C API].
struct BLFontDataCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
  BL_DEFINE_OBJECT_DCAST(BLFontData)
};

//! \cond INTERNAL
//! Font data [C API Virtual Function Table].
struct BLFontDataVirt BL_CLASS_INHERITS(BLObjectVirt) {
  BL_DEFINE_VIRT_BASE

  BLResult (BL_CDECL* getTableTags)(const BLFontDataImpl* impl, uint32_t faceIndex, BLArrayCore* out) BL_NOEXCEPT;
  size_t (BL_CDECL* getTables)(const BLFontDataImpl* impl, uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t n) BL_NOEXCEPT;
};

//! Font data [C API Impl].
struct BLFontDataImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Virtual function table.
  const BLFontDataVirt* virt;

  //! Type of the face that would be created with this font data.
  uint8_t faceType;

  //! Number of font faces stored in this font data instance.
  uint32_t faceCount;
  //! Font data flags.
  uint32_t flags;
};
//! \endcond

BL_BEGIN_C_DECLS

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
BL_API uint32_t BL_CDECL blFontDataGetFaceCount(const BLFontDataCore* self) BL_NOEXCEPT_C;
BL_API BLFontFaceType BL_CDECL blFontDataGetFaceType(const BLFontDataCore* self) BL_NOEXCEPT_C;
BL_API BLFontDataFlags BL_CDECL blFontDataGetFlags(const BLFontDataCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontDataGetTableTags(const BLFontDataCore* self, uint32_t faceIndex, BLArrayCore* dst) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL blFontDataGetTables(const BLFontDataCore* self, uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t count) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! \}
//! \}

//! \addtogroup bl_text
//! \{

//! \name BLFontData - C++ API
//! \{

#ifdef __cplusplus

//! Font data [C++ API].
class BLFontData final : public BLFontDataCore {
public:
  //! \cond INTERNAL
  //! \name Internals
  //! \{

  BL_INLINE_NODEBUG BLFontDataImpl* _impl() const noexcept { return static_cast<BLFontDataImpl*>(_d.impl); }

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLFontData() noexcept { blFontDataInit(this); }
  BL_INLINE_NODEBUG BLFontData(BLFontData&& other) noexcept { blFontDataInitMove(this, &other); }
  BL_INLINE_NODEBUG BLFontData(const BLFontData& other) noexcept { blFontDataInitWeak(this, &other); }

  BL_INLINE_NODEBUG ~BLFontData() noexcept {
    if (BLInternal::objectNeedsCleanup(_d.info.bits))
      blFontDataDestroy(this);
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return isValid(); }

  BL_INLINE_NODEBUG BLFontData& operator=(BLFontData&& other) noexcept { blFontDataAssignMove(this, &other); return *this; }
  BL_INLINE_NODEBUG BLFontData& operator=(const BLFontData& other) noexcept { blFontDataAssignWeak(this, &other); return *this; }

  BL_INLINE_NODEBUG bool operator==(const BLFontData& other) const noexcept { return  equals(other); }
  BL_INLINE_NODEBUG bool operator!=(const BLFontData& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult reset() noexcept { return blFontDataReset(this); }
  BL_INLINE_NODEBUG void swap(BLFontData& other) noexcept { _d.swap(other._d); }

  BL_INLINE_NODEBUG BLResult assign(BLFontData&& other) noexcept { return blFontDataAssignMove(this, &other); }
  BL_INLINE_NODEBUG BLResult assign(const BLFontData& other) noexcept { return blFontDataAssignWeak(this, &other); }

  //! Tests whether the font data is a built-in null instance.
  BL_INLINE_NODEBUG bool isValid() const noexcept { return _impl()->faceCount != 0; }
  //! Tests whether the font data is empty, which is the same as `!isValid()`.
  BL_INLINE_NODEBUG bool empty() const noexcept { return !isValid(); }

  BL_INLINE_NODEBUG bool equals(const BLFontData& other) const noexcept { return blFontDataEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  //! Creates a \ref BLFontData from a file specified by the given `fileName`.
  //!
  //! \remarks The `readFlags` argument allows to specify flags that will be passed to \ref BLFileSystem::readFile()`
  //! to read the content of the file. It's possible to use memory mapping to get its content, which is the recommended
  //! way for reading system fonts. The best combination is to use \ref BL_FILE_READ_MMAP_ENABLED flag combined with
  //! \ref BL_FILE_READ_MMAP_AVOID_SMALL. This combination means to try to use memory mapping only when the size of the
  //! font is greater than a minimum value (determined by Blend2D), and would fallback to a regular open/read in case
  //! the memory mapping is not possible or failed for some other reason. Please note that not all files can be memory
  //! mapped so \ref BL_FILE_READ_MMAP_NO_FALLBACK flag is not recommended.
  BL_INLINE_NODEBUG BLResult createFromFile(const char* fileName, BLFileReadFlags readFlags = BL_FILE_READ_NO_FLAGS) noexcept {
    return blFontDataCreateFromFile(this, fileName, readFlags);
  }

  //! Creates a \ref BLFontData from the given `data` stored in `BLArray<uint8_t>`
  //!
  //! The given `data` would be weak copied on success so the given array can be safely destroyed after the function
  //! returns.
  //!
  //! \remarks The weak copy of the passed `data` is internal and there is no API to access it after the function
  //! returns. The reason for making it internal is that multiple implementations of \ref BLFontData may exist and some
  //! can only store data at table level, so Blend2D doesn't expose the detail about how the data is stored.
  BL_INLINE_NODEBUG BLResult createFromData(const BLArray<uint8_t>& data) noexcept {
    return blFontDataCreateFromDataArray(this, &data);
  }

  //! Creates ` BLFontData` from the given `data` of the given `size`.
  //!
  //! \note Optionally a `destroyFunc` can be used as a notifier that will be called when the data is no longer needed.
  //! Destroy func will be called with `userData`.
  BL_INLINE_NODEBUG BLResult createFromData(const void* data, size_t dataSize, BLDestroyExternalDataFunc destroyFunc = nullptr, void* userData = nullptr) noexcept {
    return blFontDataCreateFromData(this, data, dataSize, destroyFunc, userData);
  }

  //! \}

  //! \name Accessors
  //! \{

  //! Type of font face that this data describes.
  //!
  //! It doesn't matter if the content is a single font or a collection. In any case the `faceType()` would always
  //! return the type of the font face that will be created by \ref BLFontFace::createFromData().
  BL_INLINE_NODEBUG BLFontFaceType faceType() const noexcept { return BLFontFaceType(_impl()->faceType); }

  //! Returns the number of faces of this font data.
  //!
  //! If the data is not initialized the result would be always zero. If the data is initialized to a single font it
  //! would be 1, and if the data is initialized to a font collection then the return would correspond to the number
  //! of font faces within that collection.
  //!
  //! \note You should not use `faceCount()` to check whether the font is a collection as it's possible to have a
  //! font-collection with just a single font. Using `isCollection()` is more reliable and would always return the
  //! right value.
  BL_INLINE_NODEBUG uint32_t faceCount() const noexcept { return _impl()->faceCount; }

  //! Returns font data flags.
  BL_INLINE_NODEBUG BLFontDataFlags flags() const noexcept { return BLFontDataFlags(_impl()->flags); }

  //! Tests whether this font data is a font-collection.
  BL_INLINE_NODEBUG bool isCollection() const noexcept { return (_impl()->flags & BL_FONT_DATA_FLAG_COLLECTION) != 0; }

  //! Populates `dst` array with all table tags provided by font face at the given `faceIndex`.
  BL_INLINE_NODEBUG BLResult getTableTags(uint32_t faceIndex, BLArray<BLTag>& dst) const noexcept {
    // The same as blFontDataGetTableTags() [C API].
    return _impl()->virt->getTableTags(_impl(), faceIndex, &dst);
  }

  BL_INLINE_NODEBUG size_t getTable(uint32_t faceIndex, BLFontTable* dst, BLTag tag) const noexcept {
    // The same as blFontDataGetTables() [C API].
    return _impl()->virt->getTables(_impl(), faceIndex, dst, &tag, 1);
  }

  BL_INLINE_NODEBUG size_t getTables(uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t count) const noexcept {
    // The same as blFontDataGetTables() [C API].
    return _impl()->virt->getTables(_impl(), faceIndex, dst, tags, count);
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_FONTDATA_H_INCLUDED
