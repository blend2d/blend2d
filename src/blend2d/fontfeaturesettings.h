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

//! \addtogroup bl_c_api
//! \{

//! \name BLFontFeatureSettings - C API
//! \{

//! Font feature settings [C API].
struct BLFontFeatureSettingsCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
  BL_DEFINE_OBJECT_DCAST(BLFontFeatureSettings)
};

//! \cond INTERNAL
//! Font feature settings [C API Impl].
//!
//! \note This Impl's layout is fully compatible with \ref BLArrayImpl.
struct BLFontFeatureSettingsImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Pointer to feature items.
  BLFontFeatureItem* data;
  //! Number of feature items in `data`.
  size_t size;
  //! Capacity of `data`.
  size_t capacity;
};
//! \endcond

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
BL_API bool BL_CDECL blFontFeatureSettingsHasValue(const BLFontFeatureSettingsCore* self, BLTag featureTag) BL_NOEXCEPT_C;
BL_API uint32_t BL_CDECL blFontFeatureSettingsGetValue(const BLFontFeatureSettingsCore* self, BLTag featureTag) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFeatureSettingsSetValue(BLFontFeatureSettingsCore* self, BLTag featureTag, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFontFeatureSettingsRemoveValue(BLFontFeatureSettingsCore* self, BLTag featureTag) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blFontFeatureSettingsEquals(const BLFontFeatureSettingsCore* a, const BLFontFeatureSettingsCore* b) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! \}
//! \}

//! \addtogroup bl_text
//! \{

//! \name BLFontFeatureSettings - Constants
//! \{

//! A constant representing an invalid font feature value in font feature tag/value pair.
BL_DEFINE_CONST uint32_t BL_FONT_FEATURE_INVALID_VALUE = 0xFFFFFFFFu;

//! \}

//! \name BLFontFeatureSettings - Structs
//! \{

//! Associates a font feature tag with a value. Tag describes the feature (as provided by the font) and `value`
//! describes its value. Some features only allow boolean values 0 and 1 and some allow values up to 65535.
//! Values above 65535 are invalid, however, only \ref BL_FONT_FEATURE_INVALID_VALUE should be used as invalid
//! value in general.
//!
//! Registered OpenType features:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/featuretags
//!   - https://helpx.adobe.com/typekit/using/open-type-syntax.html
struct BLFontFeatureItem {
  //! \name Members
  //! \{

  //! Feature tag (32-bit).
  BLTag tag;

  //! Feature value.
  //!
  //! \note values greater than 65535 are invalid.
  uint32_t value;

  //! \}

#ifdef __cplusplus
  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG void reset() noexcept { *this = BLFontFeatureItem{}; }

  //! \}
#endif
};

//! A view unifying the representation of an internal storage used by \ref BLFontFeatureSettings.
struct BLFontFeatureSettingsView {
  //! \name Members
  //! \{

  //! Pointer to font feature items, where each item describes a tag and its value.
  //!
  //! \note If the container is in SSO mode the `data` member will point to `ssoData`.
  const BLFontFeatureItem* data;
  //! Count of items in `data.
  size_t size;
  //! Unpacked SSO items into \ref BLFontFeatureItem array.
  //!
  //! \note This member won't be initialized or zeroed in case \ref BLFontFeatureSettings is not in SSO mode. And if the
  //! container is in SSO mode only the number of items used will be overwritten by \ref BLFontFeatureSettings::getView().
  BLFontFeatureItem ssoData[36];

  //! \}

#if defined(__cplusplus)
  //! \name C++ Iterator Compatibility
  //! \{

  //! Tests whether the view is empty.
  BL_NODISCARD
  BL_INLINE_NODEBUG bool empty() const noexcept { return size == 0; }

   //! Returns a const pointer to \ref BLFontFeatureSettingsView data (iterator compatibility).
  BL_NODISCARD
  BL_INLINE_NODEBUG const BLFontFeatureItem* begin() const noexcept { return data; }

  //! Returns a const pointer to the end of \ref BLFontFeatureSettingsView data (iterator compatibility).
  BL_NODISCARD
  BL_INLINE_NODEBUG const BLFontFeatureItem* end() const noexcept { return data + size; }

   //! Returns a const pointer to \ref BLFontFeatureSettingsView data (iterator compatibility).
  BL_NODISCARD
  BL_INLINE_NODEBUG const BLFontFeatureItem* cbegin() const noexcept { return data; }

  //! Returns a const pointer to the end of \ref BLFontFeatureSettingsView data (iterator compatibility).
  BL_NODISCARD
  BL_INLINE_NODEBUG const BLFontFeatureItem* cend() const noexcept { return data + size; }

  //! \}
#endif
};

//! \}

//! \name BLFontFeatureSettings - C++ API
//! \{

#ifdef __cplusplus

//! Font feature settings [C++ API].
class BLFontFeatureSettings final : public BLFontFeatureSettingsCore {
public:
  //! \cond INTERNAL
  //! \name Internals
  //! \{

  enum : uint32_t {
    //! SSO capacity of \ref BLFontFeatureSettings container.
    kSSOCapacity = 36u,

    //! Signature of an empty font feature settings.
    kSSOEmptySignature = BLObjectInfo::packTypeWithMarker(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS)
  };

  BL_INLINE_NODEBUG BLFontFeatureSettingsImpl* _impl() const noexcept { return static_cast<BLFontFeatureSettingsImpl*>(_d.impl); }

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLFontFeatureSettings() noexcept {
    _d.initStatic(BLObjectInfo{kSSOEmptySignature});
    _d.u32_data[2] = 0xFFFFFFFFu;
  }

  BL_INLINE_NODEBUG BLFontFeatureSettings(BLFontFeatureSettings&& other) noexcept {
    _d = other._d;
    other._d.initStatic(BLObjectInfo{kSSOEmptySignature});
    other._d.u32_data[2] = 0xFFFFFFFFu;
  }

  BL_INLINE_NODEBUG BLFontFeatureSettings(const BLFontFeatureSettings& other) noexcept {
    blFontFeatureSettingsInitWeak(this, &other);
  }

  BL_INLINE_NODEBUG ~BLFontFeatureSettings() noexcept {
    if (BLInternal::objectNeedsCleanup(_d.info.bits))
      blFontFeatureSettingsDestroy(this);
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG BLFontFeatureSettings& operator=(BLFontFeatureSettings&& other) noexcept { blFontFeatureSettingsAssignMove(this, &other); return *this; }
  BL_INLINE_NODEBUG BLFontFeatureSettings& operator=(const BLFontFeatureSettings& other) noexcept { blFontFeatureSettingsAssignWeak(this, &other); return *this; }

  BL_INLINE_NODEBUG bool operator==(const BLFontFeatureSettings& other) const noexcept { return  equals(other); }
  BL_INLINE_NODEBUG bool operator!=(const BLFontFeatureSettings& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult reset() noexcept { return blFontFeatureSettingsReset(this); }
  BL_INLINE_NODEBUG BLResult clear() noexcept { return blFontFeatureSettingsClear(this); }
  BL_INLINE_NODEBUG void swap(BLFontFeatureSettings& other) noexcept { _d.swap(other._d); }

  BL_INLINE_NODEBUG BLResult assign(BLFontFeatureSettings&& other) noexcept { return blFontFeatureSettingsAssignMove(this, &other); }
  BL_INLINE_NODEBUG BLResult assign(const BLFontFeatureSettings& other) noexcept { return blFontFeatureSettingsAssignWeak(this, &other); }

  //! \}

  //! \name Accessors
  //! \{

  //! Tests whether the container is empty, which means that no tag/value pairs are stored in it.
  BL_INLINE_NODEBUG bool empty() const noexcept { return size() == 0; }

  //! Returns the number of feature tag/value pairs stored in the container.
  BL_INLINE_NODEBUG size_t size() const noexcept { return _d.sso() ? size_t(_d.info.aField()) : _impl()->size; }

  //! Returns the container capacity
  //!
  //! \note If the container is in SSO mode, it would return the SSO capacity, however, such capacity can only be used
  //! for simple feature tag/value pairs. Some tags from these can only hold a boolean value (0 or 1) and ther others
  //! can hold a value from 0 to 15. So, if any tag/value pair requires a greater value than 15 it would never be able
  //! to use SSO representation.
  BL_INLINE_NODEBUG size_t capacity() const noexcept { return _d.sso() ? size_t(kSSOCapacity) : _impl()->capacity; }

  //! Returns a normalized view of tag/value pairs as an iterable \ref BLFontFeatureItem array in the output view `out`.
  //!
  //! \note If the container is in SSO mode then all \ref BLFontFeatureItem values will be created from the underlying
  //! SSO representation and \ref BLFontFeatureSettingsView::data will point to \ref BLFontFeatureSettingsView::ssoData.
  //! If the container is dynamic, \ref BLFontFeatureSettingsView::ssoData won't be initialized and
  //! \ref BLFontFeatureSettingsView::data will point to the container's data. This means that the view cannot outlive
  //! the container, and also during iteration the view the container cannot be modified as that could invalidate the
  //! entire view.
  BL_INLINE_NODEBUG BLResult getView(BLFontFeatureSettingsView* out) const noexcept { return blFontFeatureSettingsGetView(this, out); }

  //! Tests whether the settings contains the given `featureTag`.
  BL_INLINE_NODEBUG bool hasValue(BLTag featureTag) const noexcept { return blFontFeatureSettingsHasValue(this, featureTag); }

  //! Returns the value associated with the given `featureTag`.
  //!
  //! If the `featureTag` doesn't exist or is invalid \ref BL_FONT_FEATURE_INVALID_VALUE is returned.
  BL_INLINE_NODEBUG uint32_t getValue(BLTag featureTag) const noexcept { return blFontFeatureSettingsGetValue(this, featureTag); }

  //! Sets or inserts the given `featureTag` to the settings, associating the `featureTag` with `value`.
  //!
  //! The `featureTag` must be valid, which means that it must contain 4 characters within ' ' to '~'
  //! range - [32, 126] in ASCII. If the given `featureTag` is not valid or `value` is out of range
  //! (maximum value is `65535`) \ref BL_ERROR_INVALID_VALUE is returned.
  //!
  //! The following tags only support values that are either 0 (disabled) or 1 (enabled):
  //!
  //!   - 'case'
  //!   - 'clig'
  //!   - 'cpct'
  //!   - 'cpsp'
  //!   - 'dlig'
  //!   - 'dnom'
  //!   - 'expt'
  //!   - 'falt'
  //!   - 'frac'
  //!   - 'fwid'
  //!   - 'halt'
  //!   - 'hist'
  //!   - 'hwid'
  //!   - 'jalt'
  //!   - 'kern'
  //!   - 'liga'
  //!   - 'lnum'
  //!   - 'onum'
  //!   - 'ordn'
  //!   - 'palt'
  //!   - 'pcap'
  //!   - 'ruby'
  //!   - 'smcp'
  //!   - 'subs'
  //!   - 'sups'
  //!   - 'titl'
  //!   - 'tnam'
  //!   - 'tnum'
  //!   - 'unic'
  //!   - 'valt'
  //!   - 'vkrn'
  //!   - 'zero'
  //!
  //! Trying to use any other value with these tags would fail with \ref BL_ERROR_INVALID_VALUE error.
  BL_INLINE_NODEBUG BLResult setValue(BLTag featureTag, uint32_t value) noexcept { return blFontFeatureSettingsSetValue(this, featureTag, value); }

  //! Removes the given `featureTag` and its associated value from the settings.
  //!
  //! Nothing happens if the `featureTag` is not in the settings (\ref BL_SUCCESS is returned).
  BL_INLINE_NODEBUG BLResult removeValue(BLTag featureTag) noexcept { return blFontFeatureSettingsRemoveValue(this, featureTag); }

  //! \}

  //! \name Equality & Comparison
  //! \{

  //! Tests whether this font feature settings is equal to `other` - equality means that it has the same tag/value pairs.
  BL_INLINE_NODEBUG bool equals(const BLFontFeatureSettings& other) const noexcept { return blFontFeatureSettingsEquals(this, &other); }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_FONTFEATURESETTINGS_H_INCLUDED
