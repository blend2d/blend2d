// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTMANAGER_H_INCLUDED
#define BLEND2D_FONTMANAGER_H_INCLUDED

#include "font.h"
#include "object.h"
#include "string.h"

//! \addtogroup bl_c_api
//! \{

//! \name BLFontManager - C API
//! \{

//! Font manager [C API].
struct BLFontManagerCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
  BL_DEFINE_OBJECT_DCAST(BLFontManager)
};

//! \cond INTERNAL
//! Font manager [C API Virtual Function Table].
struct BLFontManagerVirt BL_CLASS_INHERITS(BLObjectVirt) {
  BL_DEFINE_VIRT_BASE
};

//! Font manager [C API Impl].
struct BLFontManagerImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Virtual function table.
  const BLFontManagerVirt* virt;
};
//! \endcond

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blFontManagerInit(BLFontManagerCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontManagerInitMove(BLFontManagerCore* self, BLFontManagerCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontManagerInitWeak(BLFontManagerCore* self, const BLFontManagerCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontManagerInitNew(BLFontManagerCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontManagerDestroy(BLFontManagerCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontManagerReset(BLFontManagerCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontManagerAssignMove(BLFontManagerCore* self, BLFontManagerCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontManagerAssignWeak(BLFontManagerCore* self, const BLFontManagerCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontManagerCreate(BLFontManagerCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL blFontManagerGetFaceCount(const BLFontManagerCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL blFontManagerGetFamilyCount(const BLFontManagerCore* self) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blFontManagerHasFace(const BLFontManagerCore* self, const BLFontFaceCore* face) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontManagerAddFace(BLFontManagerCore* self, const BLFontFaceCore* face) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontManagerQueryFace(const BLFontManagerCore* self, const char* name, size_t nameSize, const BLFontQueryProperties* properties, BLFontFaceCore* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontManagerQueryFacesByFamilyName(const BLFontManagerCore* self, const char* name, size_t nameSize, BLArrayCore* out) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blFontManagerEquals(const BLFontManagerCore* a, const BLFontManagerCore* b) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! \}
//! \}

//! \addtogroup bl_text
//! \{

//! \name BLFontManager - Structs
//! \{

//! Properties that can be used to query \ref BLFont and \ref BLFontFace.
//!
//! \sa BLFontManager.
struct BLFontQueryProperties {
  //! \name Members
  //! \{

  //! Font style.
  uint32_t style;
  //! Font weight.
  uint32_t weight;
  //! Font stretch.
  uint32_t stretch;

  //! \}

#ifdef __cplusplus
  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG void reset() noexcept { *this = BLFontQueryProperties{}; }

  //! \}
#endif
};

//! \}

//! \name BLFontManager - C++ API
//! \{

#ifdef __cplusplus

//! Font manager [C++ API].
class BLFontManager final : public BLFontManagerCore {
public:
  //! \cond INTERNAL
  [[nodiscard]]
  BL_INLINE_NODEBUG BLFontManagerImpl* _impl() const noexcept { return static_cast<BLFontManagerImpl*>(_d.impl); }
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLFontManager() noexcept { blFontManagerInit(this); }
  BL_INLINE_NODEBUG BLFontManager(BLFontManager&& other) noexcept { blFontManagerInitMove(this, &other); }
  BL_INLINE_NODEBUG BLFontManager(const BLFontManager& other) noexcept { blFontManagerInitWeak(this, &other); }
  BL_INLINE_NODEBUG ~BLFontManager() noexcept { blFontManagerDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG BLFontManager& operator=(BLFontManager&& other) noexcept { blFontManagerAssignMove(this, &other); return *this; }
  BL_INLINE_NODEBUG BLFontManager& operator=(const BLFontManager& other) noexcept { blFontManagerAssignWeak(this, &other); return *this; }

  BL_INLINE_NODEBUG bool operator==(const BLFontManager& other) const noexcept { return  equals(other); }
  BL_INLINE_NODEBUG bool operator!=(const BLFontManager& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult reset() noexcept { return blFontManagerReset(this); }
  BL_INLINE_NODEBUG void swap(BLFontManager& other) noexcept { _d.swap(other._d); }

  BL_INLINE_NODEBUG BLResult assign(BLFontManager&& other) noexcept { return blFontManagerAssignMove(this, &other); }
  BL_INLINE_NODEBUG BLResult assign(const BLFontManager& other) noexcept { return blFontManagerAssignWeak(this, &other); }

  //! Tests whether the font-manager is a valid FontManager and not a built-in default instance.
  [[nodiscard]]
  BL_INLINE_NODEBUG bool isValid() const noexcept { return _d.aField() == 0; }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool equals(const BLFontManager& other) const noexcept { return blFontManagerEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult create() noexcept { return blFontManagerCreate(this); }

  //! \}

  //! Returns the number of BLFontFace instances the font manager holds.
  [[nodiscard]]
  BL_INLINE_NODEBUG size_t faceCount() const noexcept { return blFontManagerGetFaceCount(this); }

  //! Returns the number of unique font families the font manager holds.
  [[nodiscard]]
  BL_INLINE_NODEBUG size_t familyCount() const noexcept { return blFontManagerGetFamilyCount(this); }

  //! \name Face Management
  //! \{

  //! Tests whether the font manager contains the given font `face`.
  [[nodiscard]]
  BL_INLINE_NODEBUG bool hasFace(const BLFontFaceCore& face) const noexcept {
    return blFontManagerHasFace(this, &face);
  }

  //! Adds a font `face` to the font manager.
  //!
  //! Important result conditions:
  //!   - \ref BL_SUCCESS is returned if the `face` was successfully added to font manager or if font manager already
  //!     held it.
  //!   - \ref BL_ERROR_FONT_NOT_INITIALIZED is returned if the font `face` is invalid.
  //!   - \ref BL_ERROR_OUT_OF_MEMORY is returned if memory allocation failed.
  BL_INLINE_NODEBUG BLResult addFace(const BLFontFaceCore& face) noexcept {
    return blFontManagerAddFace(this, &face);
  }

  //! Queries a font face by family `name` and stores the result to `out`.
  BL_INLINE_NODEBUG BLResult queryFace(const char* name, BLFontFaceCore& out) const noexcept {
    return blFontManagerQueryFace(this, name, SIZE_MAX, nullptr, &out);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult queryFace(BLStringView name, BLFontFaceCore& out) const noexcept {
    return blFontManagerQueryFace(this, name.data, name.size, nullptr, &out);
  }

  //! Queries a font face by family `name` and stores the result to `out`.
  //!
  //! A `properties` parameter contains query properties that the query engine will consider when doing the match.
  //! The best candidate will be selected based on the following rules:
  //!
  //!   - Style has the highest priority.
  //!   - Weight has the lowest priority.
  BL_INLINE_NODEBUG BLResult queryFace(const char* name, const BLFontQueryProperties& properties, BLFontFaceCore& out) const noexcept {
    return blFontManagerQueryFace(this, name, SIZE_MAX, &properties, &out);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult queryFace(BLStringView name, const BLFontQueryProperties& properties, BLFontFaceCore& out) const noexcept {
    return blFontManagerQueryFace(this, name.data, name.size, &properties, &out);
  }

  //! Queries all font faces by family `name` and stores the result to `out`.
  BL_INLINE_NODEBUG BLResult queryFacesByFamilyName(const char* name, BLArray<BLFontFace>& out) const noexcept {
    return blFontManagerQueryFacesByFamilyName(this, name, SIZE_MAX, &out);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult queryFacesByFamilyName(BLStringView name, BLArray<BLFontFace>& out) const noexcept {
    return blFontManagerQueryFacesByFamilyName(this, name.data, name.size, &out);
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_FONTMANAGER_H_INCLUDED
