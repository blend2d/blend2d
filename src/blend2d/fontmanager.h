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

#ifndef BLEND2D_FONTMANAGER_H_INCLUDED
#define BLEND2D_FONTMANAGER_H_INCLUDED

#include "./font.h"
#include "./string.h"

//! \addtogroup blend2d_api_text
//! \{

// ============================================================================
// [BLFontManager - Core]
// ============================================================================

//! Font manager [C Interface - Virtual Function Table].
struct BLFontManagerVirt {
  BLResult (BL_CDECL* destroy)(BLFontManagerImpl* impl) BL_NOEXCEPT;
};

//! Font manager [C Interface - Impl].
struct BLFontManagerImpl {
  //! Virtual function table.
  const BLFontManagerVirt* virt;

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;
  //! Reserved for future use, must be zero.
  uint8_t reserved[4];
};

//! Font manager [C Interface - Core].
struct BLFontManagerCore {
  BLFontManagerImpl* impl;
};

// ============================================================================
// [BLFontManager - C++]
// ============================================================================

#ifdef __cplusplus
//! Font manager [C++ API].
class BLFontManager : public BLFontManagerCore {
public:
  //! \cond INTERNAL
  static constexpr const uint32_t kImplType = BL_IMPL_TYPE_FONT_MANAGER;
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLFontManager() noexcept { this->impl = none().impl; }
  BL_INLINE BLFontManager(BLFontManager&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLFontManager(const BLFontManager& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLFontManager(BLFontManagerImpl* impl) noexcept { this->impl = impl; }
  BL_INLINE ~BLFontManager() noexcept { blFontManagerDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE BLFontManager& operator=(BLFontManager&& other) noexcept { blFontManagerAssignMove(this, &other); return *this; }
  BL_INLINE BLFontManager& operator=(const BLFontManager& other) noexcept { blFontManagerAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLFontManager& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLFontManager& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blFontManagerReset(this); }
  BL_INLINE void swap(BLFontManager& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult assign(BLFontManager&& other) noexcept { return blFontManagerAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLFontManager& other) noexcept { return blFontManagerAssignWeak(this, &other); }

  //! Tests whether the font-data is a built-in null instance.
  BL_NODISCARD
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }

  BL_NODISCARD
  BL_INLINE bool equals(const BLFontManager& other) const noexcept { return blFontManagerEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  BL_INLINE BLResult create() noexcept { return blFontManagerCreate(this); }

  //! \}

  //! Returns the number of BLFontFace instances the font manager holds.
  BL_NODISCARD
  BL_INLINE size_t faceCount() const noexcept { return blFontManagerGetFaceCount(this); }

  //! Returns the number of unique font families the font manager holds.
  BL_NODISCARD
  BL_INLINE size_t familyCount() const noexcept { return blFontManagerGetFamilyCount(this); }

  //! \name Face Management
  //! \{

  //! Tests whether the font manager contains the given font `face`.
  BL_NODISCARD
  BL_INLINE bool hasFace(const BLFontFace& face) const noexcept {
    return blFontManagerHasFace(this, &face);
  }

  //! Adds a font `face` to the font manager.
  //!
  //! Important conditions:
  //!   * `BL_SUCCESS` is returned if the `face` was successfully added to font
  //!     manager or if font manager already held it.
  //!   * `BL_ERROR_FONT_NOT_INITIALIZED` is returned if the font `face` is invalid.
  //!   * `BL_ERROR_OUT_OF_MEMORY` is returned if memory allocation failed.
  BL_INLINE BLResult addFace(const BLFontFace& face) noexcept {
    return blFontManagerAddFace(this, &face);
  }

  //! Queries a font face by family `name` and stores the result to `out`.
  BL_INLINE BLResult queryFace(const char* name, BLFontFace& out) const noexcept {
    return blFontManagerQueryFace(this, name, SIZE_MAX, nullptr, &out);
  }

  //! \overload
  BL_INLINE BLResult queryFace(const BLStringView& name, BLFontFace& out) const noexcept {
    return blFontManagerQueryFace(this, name.data, name.size, nullptr, &out);
  }

  //! Queries a font face by family `name` and stores the result to `out`.
  //!
  //! A `properties` parameter contains query properties that the query engine
  //! will consider when doing the match. The best candidate will be selected
  //! based on the following rules:
  //!
  //!   * Style has the highest priority.
  //!   * Weight has the lowest priority.
  BL_INLINE BLResult queryFace(const char* name, const BLFontQueryProperties& properties, BLFontFace& out) const noexcept {
    return blFontManagerQueryFace(this, name, SIZE_MAX, &properties, &out);
  }

  //! \overload
  BL_INLINE BLResult queryFace(const BLStringView& name, const BLFontQueryProperties& properties, BLFontFace& out) const noexcept {
    return blFontManagerQueryFace(this, name.data, name.size, &properties, &out);
  }

  //! Queries all font-faces by family `name` and stores the result to `out`.
  BL_INLINE BLResult queryFacesByFamilyName(const char* name, BLArray<BLFontFace>& out) const noexcept {
    return blFontManagerQueryFacesByFamilyName(this, name, SIZE_MAX, &out);
  }

  //! \overload
  BL_INLINE BLResult queryFacesByFamilyName(const BLStringView& name, BLArray<BLFontFace>& out) const noexcept {
    return blFontManagerQueryFacesByFamilyName(this, name.data, name.size, &out);
  }

  //! \}

  BL_NODISCARD
  static BL_INLINE const BLFontManager& none() noexcept { return reinterpret_cast<const BLFontManager*>(blNone)[kImplType]; }
};
#endif

//! \}

#endif // BLEND2D_FONTMANAGER_H_INCLUDED
