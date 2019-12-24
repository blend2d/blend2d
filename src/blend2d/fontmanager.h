// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_FONTMANAGER_H
#define BLEND2D_FONTMANAGER_H

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
//! Font data [C++ API].
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
  BL_INLINE ~BLFontManager() noexcept { blFontManagerReset(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return !isNone(); }

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
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }
  //! Tests whether the font-data is empty (which the same as `isNone()` in this case).
  BL_INLINE bool empty() const noexcept { return isNone(); }

  BL_INLINE bool equals(const BLFontManager& other) const noexcept { return blFontManagerEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  //! \}

  //! \name Query Functionality
  //! \{

  //! \}

  //! \name Accessors
  //! \{

  //! \}

  static BL_INLINE const BLFontManager& none() noexcept { return reinterpret_cast<const BLFontManager*>(blNone)[kImplType]; }
};
#endif

//! \}

#endif // BLEND2D_FONTMANAGER_H
