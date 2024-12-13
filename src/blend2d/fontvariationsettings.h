// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTVARIATIONSETTINGS_H_INCLUDED
#define BLEND2D_FONTVARIATIONSETTINGS_H_INCLUDED

#include "array.h"
#include "bitset.h"
#include "filesystem.h"
#include "fontdefs.h"
#include "geometry.h"
#include "glyphbuffer.h"
#include "object.h"
#include "path.h"
#include "string.h"

//! \addtogroup bl_c_api
//! \{

//! \name BLFontVariationSettings - C API
//! \{

//! Font variation settings [C API].
struct BLFontVariationSettingsCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
  BL_DEFINE_OBJECT_DCAST(BLFontVariationSettings)
};

//! \cond INTERNAL
//! Font variation settings [C API Impl].
//!
//! \note This Impl's layout is fully compatible with \ref BLArrayImpl.
struct BLFontVariationSettingsImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Pointer to variation items.
  BLFontVariationItem* data;
  //! Number of variation items in `data`.
  size_t size;
  //! Capacity of `data`.
  size_t capacity;
};
//! \endcond

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blFontVariationSettingsInit(BLFontVariationSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontVariationSettingsInitMove(BLFontVariationSettingsCore* self, BLFontVariationSettingsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontVariationSettingsInitWeak(BLFontVariationSettingsCore* self, const BLFontVariationSettingsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontVariationSettingsDestroy(BLFontVariationSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontVariationSettingsReset(BLFontVariationSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontVariationSettingsClear(BLFontVariationSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontVariationSettingsShrink(BLFontVariationSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontVariationSettingsAssignMove(BLFontVariationSettingsCore* self, BLFontVariationSettingsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontVariationSettingsAssignWeak(BLFontVariationSettingsCore* self, const BLFontVariationSettingsCore* other) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL blFontVariationSettingsGetSize(const BLFontVariationSettingsCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL blFontVariationSettingsGetCapacity(const BLFontVariationSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontVariationSettingsGetView(const BLFontVariationSettingsCore* self, BLFontVariationSettingsView* out) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blFontVariationSettingsHasValue(const BLFontVariationSettingsCore* self, BLTag variationTag) BL_NOEXCEPT_C;
BL_API float BL_CDECL blFontVariationSettingsGetValue(const BLFontVariationSettingsCore* self, BLTag variationTag) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontVariationSettingsSetValue(BLFontVariationSettingsCore* self, BLTag variationTag, float value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontVariationSettingsRemoveValue(BLFontVariationSettingsCore* self, BLTag variationTag) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blFontVariationSettingsEquals(const BLFontVariationSettingsCore* a, const BLFontVariationSettingsCore* b) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! \}
//! \}

//! \addtogroup bl_text
//! \{

//! \name BLFontVariationSettings - Structs
//! \{

//! Associates a font variation tag with a value.
struct BLFontVariationItem {
  //! \name Members
  //! \{

  //! Variation tag (32-bit).
  BLTag tag;
  //! Variation value.
  //!
  //! \note values outside of [0, 1] range are invalid.
  float value;

  //! \}

#ifdef __cplusplus
  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG void reset() noexcept { *this = BLFontVariationItem{}; }

  //! \}
#endif
};

//! A view unifying the representation of an internal storage used by \ref BLFontVariationSettings.
struct BLFontVariationSettingsView {
  //! Pointer to font variation items, where each item describes a variation tag and its value.
  //!
  //! \note If the container is in SSO mode the `data` member will point to `ssoData`.
  const BLFontVariationItem* data;
  //! Count of items in `data.
  size_t size;
  //! Unpacked SSO items into \ref BLFontVariationItem array.
  //!
  //! \note This member won't be initialized or zeroed in case \ref BLFontVariationSettings is not in
  //! SSO mode. And if the container is in SSO mode only the number of items used will be overwritten
  //! by \ref BLFontVariationSettings::getView().
  BLFontVariationItem ssoData[3];

#if defined(__cplusplus)
  //! \name C++ Iterator Compatibility
  //! \{

  //! Tests whether the view is empty.
  BL_NODISCARD
  BL_INLINE_NODEBUG bool empty() const noexcept { return size == 0; }

   //! Returns a const pointer to \ref BLFontVariationSettingsView data (iterator compatibility).
  BL_NODISCARD
  BL_INLINE_NODEBUG const BLFontVariationItem* begin() const noexcept { return data; }

  //! Returns a const pointer to the end of \ref BLFontVariationSettingsView data (iterator compatibility).
  BL_NODISCARD
  BL_INLINE_NODEBUG const BLFontVariationItem* end() const noexcept { return data + size; }

   //! Returns a const pointer to \ref BLFontVariationSettingsView data (iterator compatibility).
  BL_NODISCARD
  BL_INLINE_NODEBUG const BLFontVariationItem* cbegin() const noexcept { return data; }

  //! Returns a const pointer to the end of \ref BLFontVariationSettingsView data (iterator compatibility).
  BL_NODISCARD
  BL_INLINE_NODEBUG const BLFontVariationItem* cend() const noexcept { return data + size; }

  //! \}
#endif
};

//! \}

//! \name BLFontVariationSettings - C++ API
//! \{
#ifdef __cplusplus

//! Font variation settings [C++ API].
class BLFontVariationSettings final : public BLFontVariationSettingsCore {
public:
  //! \cond INTERNAL
  //! \name Internals
  //! \{

  enum : uint32_t {
    //! SSO capacity of \ref BLFontVariationSettings container.
    kSSOCapacity = 3,

    //! Signature of SSO representation of an empty font variation settings.
    kSSOEmptySignature = BLObjectInfo::packTypeWithMarker(BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS)
  };

  BL_INLINE_NODEBUG BLFontVariationSettingsImpl* _impl() const noexcept { return static_cast<BLFontVariationSettingsImpl*>(_d.impl); }

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLFontVariationSettings() noexcept {
    _d.initStatic(BLObjectInfo{kSSOEmptySignature});
  }

  BL_INLINE_NODEBUG BLFontVariationSettings(BLFontVariationSettings&& other) noexcept {
    _d = other._d;
    other._d.initStatic(BLObjectInfo{kSSOEmptySignature});
  }

  BL_INLINE_NODEBUG BLFontVariationSettings(const BLFontVariationSettings& other) noexcept {
    blFontVariationSettingsInitWeak(this, &other);
  }

  BL_INLINE_NODEBUG ~BLFontVariationSettings() noexcept {
    if (BLInternal::objectNeedsCleanup(_d.info.bits))
      blFontVariationSettingsDestroy(this);
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG BLFontVariationSettings& operator=(BLFontVariationSettings&& other) noexcept { blFontVariationSettingsAssignMove(this, &other); return *this; }
  BL_INLINE_NODEBUG BLFontVariationSettings& operator=(const BLFontVariationSettings& other) noexcept { blFontVariationSettingsAssignWeak(this, &other); return *this; }

  BL_INLINE_NODEBUG bool operator==(const BLFontVariationSettings& other) const noexcept { return  equals(other); }
  BL_INLINE_NODEBUG bool operator!=(const BLFontVariationSettings& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult reset() noexcept { return blFontVariationSettingsReset(this); }
  BL_INLINE_NODEBUG BLResult clear() noexcept { return blFontVariationSettingsClear(this); }
  BL_INLINE_NODEBUG void swap(BLFontVariationSettings& other) noexcept { _d.swap(other._d); }

  BL_INLINE_NODEBUG BLResult assign(BLFontVariationSettings&& other) noexcept { return blFontVariationSettingsAssignMove(this, &other); }
  BL_INLINE_NODEBUG BLResult assign(const BLFontVariationSettings& other) noexcept { return blFontVariationSettingsAssignWeak(this, &other); }

  //! \}

  //! \name Accessors
  //! \{

  //! Tests whether the container is empty, which means that no tag/value pairs are stored in it.
  BL_INLINE_NODEBUG bool empty() const noexcept { return size() == 0; }

  //! Returns the number of tag/value pairs stored in the container.
  BL_INLINE_NODEBUG size_t size() const noexcept { return _d.sso() ? size_t(_d.info.aField()) : _impl()->size; }

  //! Returns the container capacity.
  //!
  //! \note If the container is in SSO mode, it would return the SSO capacity, however, such capacity can only be used
  //! for simple tag/value pairs (where the tag is known by Blend2D and has associated an internal ID that represents it).
  BL_INLINE_NODEBUG size_t capacity() const noexcept { return _d.sso() ? size_t(kSSOCapacity) : _impl()->capacity; }

  //! Returns a normalized view of tag/value pairs as an iterable \ref BLFontVariationItem array in the output view.
  //!
  //! \note If the container is in SSO mode then all \ref BLFontVariationItem values will be created from the
  //! underlying SSO representation and \ref BLFontVariationSettingsView::data will point to \ref
  //! BLFontVariationSettingsView::ssoData. If the container is dynamic, \ref BLFontVariationSettingsView::ssoData
  //! won't be initialized and \ref BLFontVariationSettingsView::data will point to the container's data. This means
  //! that the view cannot outlive the container, and also during iteration the view the container cannot be modified
  //! as that could invalidate the entire view.
  BL_INLINE_NODEBUG BLResult getView(BLFontVariationSettingsView* out) const noexcept { return blFontVariationSettingsGetView(this, out); }

  //! Tests whether the settings contains the given `variationTag`.
  BL_INLINE_NODEBUG bool hasValue(BLTag variationTag) const noexcept { return blFontVariationSettingsHasValue(this, variationTag); }

  //! Returns the value associated with the given `variationTag`.
  //!
  //! If the `variationTag` doesn't exist or is invalid `NaN` is returned.
  BL_INLINE_NODEBUG float getValue(BLTag variationTag) const noexcept { return blFontVariationSettingsGetValue(this, variationTag); }

  //! Sets or inserts the given `variationTag` to the settings and associates it with the given `value`.
  BL_INLINE_NODEBUG BLResult setValue(BLTag variationTag, float value) noexcept { return blFontVariationSettingsSetValue(this, variationTag, value); }

  //! Removes the given `variationTag` and its value from the settings.
  //!
  //! Nothing happens if the `variationTag` is not in the settings (\ref BL_SUCCESS is returned).
  BL_INLINE_NODEBUG BLResult removeValue(BLTag variationTag) noexcept { return blFontVariationSettingsRemoveValue(this, variationTag); }

  //! \}

  //! \name Equality & Comparison
  //! \{

  //! Tests whether this font variation settings is equal to `other` - equality means that it has the same tag/value pairs.
  BL_INLINE_NODEBUG bool equals(const BLFontVariationSettings& other) const noexcept { return blFontVariationSettingsEquals(this, &other); }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_FONTVARIATIONSETTINGS_H_INCLUDED
