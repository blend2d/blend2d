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

//! \addtogroup blend2d_api_text
//! \{

//! \name BLFontVariationSettings - Structs
//! \{

//! A view unifying the representation of an internal storage used by \ref BLFontVariationSettings.
struct BLFontVariationSettingsView {
  //! Pointer to font variation items, where each item describes a key (tag) and its value.
  //!
  //! \note If the container is in SSO mode the `data` member will point to `ssoData`.
  const BLFontVariationItem* data;
  //! Count of items in `data.
  size_t size;
  //! Unpacked SSO items into `BLFontVariationItem` array.
  //!
  //! \note This member won't be initialized or zeroed in case `BLFontVariationSettings` is not in SSO mode. And if the
  //! container is in SSO mode only the number of items used will be overwritten by \ref blFontVariationSettingsGetView().
  BLFontVariationItem ssoData[3];
};

//! \}

//! \name BLFontVariationSettings - C API
//! \{

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
BL_API bool BL_CDECL blFontVariationSettingsHasKey(const BLFontVariationSettingsCore* self, BLTag key) BL_NOEXCEPT_C;
BL_API float BL_CDECL blFontVariationSettingsGetKey(const BLFontVariationSettingsCore* self, BLTag key) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontVariationSettingsSetKey(BLFontVariationSettingsCore* self, BLTag key, float value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontVariationSettingsRemoveKey(BLFontVariationSettingsCore* self, BLTag key) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blFontVariationSettingsEquals(const BLFontVariationSettingsCore* a, const BLFontVariationSettingsCore* b) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! Font variation settings [C API].
struct BLFontVariationSettingsCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
  BL_DEFINE_OBJECT_DCAST(BLFontVariationSettings)
};

//! \}

//! \cond INTERNAL
//! \name BLFontVariationSettings - Internals
//! \{

//! Font variation settings [Impl].
//!
//! \note This Impl is fully compatible with `BLArrayImpl`.
struct BLFontVariationSettingsImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Pointer to variation items.
  BLFontVariationItem* data;
  //! Numbef of variation items in `data`.
  size_t size;
  //! Capacity of `data`.
  size_t capacity;
};

//! \}
//! \endcond

//! \name BLFontVariationSettings - C++ API
//! \{
#ifdef __cplusplus

//! Font variation settings [C++ API].
class BLFontVariationSettings : public BLFontVariationSettingsCore {
public:
  //! \cond INTERNAL
  //! \name Internals
  //! \{

  enum : uint32_t {
    //! SSO capacity of \ref BLFontVariationSettings container.
    kSSOCapacity = 3
  };

  BL_INLINE BLFontVariationSettingsImpl* _impl() const noexcept { return static_cast<BLFontVariationSettingsImpl*>(_d.impl); }

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLFontVariationSettings() noexcept {
    _d.initStatic(BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS);
  }

  BL_INLINE BLFontVariationSettings(BLFontVariationSettings&& other) noexcept {
    _d = other._d;
    other._d.initStatic(BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS);
  }

  BL_INLINE BLFontVariationSettings(const BLFontVariationSettings& other) noexcept { blFontVariationSettingsInitWeak(this, &other); }
  BL_INLINE ~BLFontVariationSettings() noexcept { blFontVariationSettingsDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE BLFontVariationSettings& operator=(BLFontVariationSettings&& other) noexcept { blFontVariationSettingsAssignMove(this, &other); return *this; }
  BL_INLINE BLFontVariationSettings& operator=(const BLFontVariationSettings& other) noexcept { blFontVariationSettingsAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLFontVariationSettings& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLFontVariationSettings& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blFontVariationSettingsReset(this); }
  BL_INLINE BLResult clear() noexcept { return blFontVariationSettingsClear(this); }
  BL_INLINE void swap(BLFontVariationSettings& other) noexcept { _d.swap(other._d); }

  BL_INLINE BLResult assign(BLFontVariationSettings&& other) noexcept { return blFontVariationSettingsAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLFontVariationSettings& other) noexcept { return blFontVariationSettingsAssignWeak(this, &other); }

  //! \}

  //! \name Accessors
  //! \{

  //! Tests whether the container is empty, which means that no key/value pairs are stored in it.
  BL_INLINE bool empty() const noexcept { return size() == 0; }

  //! Returns the number of key/value pairs stored in the container.
  BL_INLINE size_t size() const noexcept { return _d.sso() ? size_t(_d.info.aField()) : _impl()->size; }

  //! Returns the container capacity
  //!
  //! \note If the container is in SSO mode, it would return the SSO capacity, however, such capacity can only be used
  //! for simple key/value pairs (where the key is known and value is either 0 or 1).
  BL_INLINE size_t capacity() const noexcept { return _d.sso() ? size_t(kSSOCapacity) : _impl()->capacity; }

  //! Returns a normalized view of key/value pairs as an iterable `BLFontVariationItem` array in the output view.
  //!
  //! \note If the container is in SSO mode then all `BLFontVariationItem` values will be created from the underlying SSO
  //! representation and `BLFontVariationSettingsView::data` will point to `BLFontVariationSettingsView::ssoData`. If the
  //! container is dynamic, `BLFontVariationSettingsView::ssoData` won't be initialized and `BLFontVariationSettingsView::data`
  //! will point to the container's data. This means that the view cannot outlive the container, and also during iteration the
  //! view the container cannot be modified as that could invalidate the entire view.
  BL_INLINE BLResult getView(BLFontVariationSettingsView* out) const noexcept { return blFontVariationSettingsGetView(this, out); }

  //! Tests whether the settings contains the given `key`.
  BL_INLINE bool hasKey(BLTag key) const noexcept { return blFontVariationSettingsHasKey(this, key); }

  //! Returns the value associated with the given `key`.
  //!
  //! If the `key` doesn't exist or is invalid `NaN` is returned.
  BL_INLINE float getKey(BLTag key) const noexcept { return blFontVariationSettingsGetKey(this, key); }

  //! Sets or inserts the given `key` to the settings, associating the `key` with `value`.
  BL_INLINE BLResult setKey(BLTag key, float value) noexcept { return blFontVariationSettingsSetKey(this, key, value); }

  //! Removes the given `key` from the settings.
  //!
  //! Nothing happens if the `key` is not in the settings (\ref BL_SUCCESS is returned).
  BL_INLINE BLResult removeKey(BLTag key) noexcept { return blFontVariationSettingsRemoveKey(this, key); }

  //! \}

  //! \name Equality & Comparison
  //! \{

  //! Tests whether this font variation settings is equal to `other` - equality means that it has the same key/value pairs.
  BL_INLINE bool equals(const BLFontVariationSettings& other) const noexcept { return blFontVariationSettingsEquals(this, &other); }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_FONTVARIATIONSETTINGS_H_INCLUDED
