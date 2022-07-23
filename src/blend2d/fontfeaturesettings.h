// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTFEATURESETTINGS_H_INCLUDED
#define BLEND2D_FONTFEATURESETTINGS_H_INCLUDED

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

//! \name BLFontFeatureSettings - Constants
//! \{

//! A constant representing an invalid font feature value in font feature key/value pair.
BL_DEFINE_CONST uint32_t BL_FONT_FEATURE_INVALID_VALUE = 0xFFFFFFFFu;

//! \}

//! \name BLFontFeatureSettings - Structs
//! \{

//! A view unifying the representation of an internal storage used by \ref BLFontFeatureSettings.
struct BLFontFeatureSettingsView {
  //! Pointer to font feature items, where each item describes a key (tag) and its value.
  //!
  //! \note If the container is in SSO mode the `data` member will point to `ssoData`.
  const BLFontFeatureItem* data;
  //! Count of items in `data.
  size_t size;
  //! Unpacked SSO items into `BLFontFeatureItem` array.
  //!
  //! \note This member won't be initialized or zeroed in case `BLFontFeatureSettings` is not in SSO mode. And if the
  //! container is in SSO mode only the number of items used will be overwritten by \ref blFontFeatureSettingsGetView().
  BLFontFeatureItem ssoData[12];
};

//! \}

//! \name BLFontFeatureSettings - C API
//! \{

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blFontFeatureSettingsInit(BLFontFeatureSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFeatureSettingsInitMove(BLFontFeatureSettingsCore* self, BLFontFeatureSettingsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFeatureSettingsInitWeak(BLFontFeatureSettingsCore* self, const BLFontFeatureSettingsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFeatureSettingsDestroy(BLFontFeatureSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFeatureSettingsReset(BLFontFeatureSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFeatureSettingsClear(BLFontFeatureSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFeatureSettingsShrink(BLFontFeatureSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFeatureSettingsAssignMove(BLFontFeatureSettingsCore* self, BLFontFeatureSettingsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFeatureSettingsAssignWeak(BLFontFeatureSettingsCore* self, const BLFontFeatureSettingsCore* other) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL blFontFeatureSettingsGetSize(const BLFontFeatureSettingsCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL blFontFeatureSettingsGetCapacity(const BLFontFeatureSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFeatureSettingsGetView(const BLFontFeatureSettingsCore* self, BLFontFeatureSettingsView* out) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blFontFeatureSettingsHasKey(const BLFontFeatureSettingsCore* self, BLTag key) BL_NOEXCEPT_C;
BL_API uint32_t BL_CDECL blFontFeatureSettingsGetKey(const BLFontFeatureSettingsCore* self, BLTag key) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFeatureSettingsSetKey(BLFontFeatureSettingsCore* self, BLTag key, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFeatureSettingsRemoveKey(BLFontFeatureSettingsCore* self, BLTag key) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blFontFeatureSettingsEquals(const BLFontFeatureSettingsCore* a, const BLFontFeatureSettingsCore* b) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! Font feature settings [C API].
struct BLFontFeatureSettingsCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
  BL_DEFINE_OBJECT_DCAST(BLFontFeatureSettings)
};

//! \}

//! \cond INTERNAL
//! \name BLFontFeatureSettings - Internals
//! \{

//! Font feature settings [Impl].
//!
//! \note This Impl is fully compatible with `BLArrayImpl`.
struct BLFontFeatureSettingsImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Pointer to feature items.
  BLFontFeatureItem* data;
  //! Numbef of feature items in `data`.
  size_t size;
  //! Capacity of `data`.
  size_t capacity;
};

//! \}
//! \endcond

//! \name BLFontFeatureSettings - C++ API
//! \{

#ifdef __cplusplus

//! Font feature settings [C++ API].
class BLFontFeatureSettings : public BLFontFeatureSettingsCore {
public:
  //! \cond INTERNAL
  //! \name Internals
  //! \{

  enum : uint32_t {
    //! SSO capacity of \ref BLFontFeatureSettings container.
    kSSOCapacity = 12u
  };

  BL_INLINE BLFontFeatureSettingsImpl* _impl() const noexcept { return static_cast<BLFontFeatureSettingsImpl*>(_d.impl); }

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLFontFeatureSettings() noexcept {
    _d.initStatic(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS);
  }

  BL_INLINE BLFontFeatureSettings(BLFontFeatureSettings&& other) noexcept {
    _d = other._d;
    other._d.initStatic(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS);
  }

  BL_INLINE BLFontFeatureSettings(const BLFontFeatureSettings& other) noexcept { blFontFeatureSettingsInitWeak(this, &other); }
  BL_INLINE ~BLFontFeatureSettings() noexcept { blFontFeatureSettingsDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE BLFontFeatureSettings& operator=(BLFontFeatureSettings&& other) noexcept { blFontFeatureSettingsAssignMove(this, &other); return *this; }
  BL_INLINE BLFontFeatureSettings& operator=(const BLFontFeatureSettings& other) noexcept { blFontFeatureSettingsAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLFontFeatureSettings& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLFontFeatureSettings& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blFontFeatureSettingsReset(this); }
  BL_INLINE BLResult clear() noexcept { return blFontFeatureSettingsClear(this); }
  BL_INLINE void swap(BLFontFeatureSettings& other) noexcept { _d.swap(other._d); }

  BL_INLINE BLResult assign(BLFontFeatureSettings&& other) noexcept { return blFontFeatureSettingsAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLFontFeatureSettings& other) noexcept { return blFontFeatureSettingsAssignWeak(this, &other); }

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

  //! Returns a normalized view of key/value pairs as an iterable `BLFontFeatureItem` array in the output view.
  //!
  //! \note If the container is in SSO mode then all `BLFontFeatureItem` values will be created from the underlying SSO
  //! representation and `BLFontFeatureSettingsView::data` will point to `BLFontFeatureSettingsView::ssoData`. If the
  //! container is dynamic, `BLFontFeatureSettingsView::ssoData` won't be initialized and `BLFontFeatureSettingsView::data`
  //! will point to the container's data. This means that the view cannot outlive the container, and also during iteration
  //! the view the container cannot be modified as that coult invalidate the entire view.
  BL_INLINE BLResult getView(BLFontFeatureSettingsView* out) const noexcept { return blFontFeatureSettingsGetView(this, out); }

  //! Tests whether the settings contains the given `key`.
  BL_INLINE bool hasKey(BLTag key) const noexcept { return blFontFeatureSettingsHasKey(this, key); }

  //! Returns the value associated with the given `key`.
  //!
  //! If the `key` doesn't exist or is invalid \ref BL_FONT_FEATURE_INVALID_VALUE is returned.
  BL_INLINE uint32_t getKey(BLTag key) const noexcept { return blFontFeatureSettingsGetKey(this, key); }

  //! Sets or inserts the given `key` to the settings, associating the `key` with `value`.
  //!
  //! The `key` must be valid, which means that it must contain 4 characters within ' ' to '~' range - [32, 126] in
  //! ASCII. If the given `key` is not valid or `value` is out of range (maximum value is `65535`) \ref
  //! BL_ERROR_INVALID_VALUE is returned.
  BL_INLINE BLResult setKey(BLTag key, uint32_t value) noexcept { return blFontFeatureSettingsSetKey(this, key, value); }

  //! Removes the given `key` from the settings.
  //!
  //! Nothing happens if the `key` is not in the settings (\ref BL_SUCCESS is returned).
  BL_INLINE BLResult removeKey(BLTag key) noexcept { return blFontFeatureSettingsRemoveKey(this, key); }

  //! \}

  //! \name Equality & Comparison
  //! \{

  //! Tests whether this font feature settings is equal to `other` - equality means that it has the same key/value pairs.
  BL_INLINE bool equals(const BLFontFeatureSettings& other) const noexcept { return blFontFeatureSettingsEquals(this, &other); }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_FONTFEATURESETTINGS_H_INCLUDED
