// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_FONT_FEATURE_SETTINGS_H_INCLUDED
#define BLEND2D_CORE_FONT_FEATURE_SETTINGS_H_INCLUDED

#include <blend2d/core/array.h>
#include <blend2d/core/file_system.h>
#include <blend2d/core/font_defs.h>
#include <blend2d/core/geometry.h>
#include <blend2d/core/glyph_buffer.h>
#include <blend2d/core/object_layer.h>
#include <blend2d/core/path.h>
#include <blend2d/core/string.h>

//! \addtogroup bl_c_api
//! \{

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
  //! \note If the container is in SSO mode the `data` member will point to `_decomposed_sso_data`.
  const BLFontFeatureItem* data;
  //! Count of items in `data.
  size_t size;

  //! Decomposed SSO items into array of \ref BLFontFeatureItem items.
  //!
  //! \note This member won't be initialized or zeroed in case \ref BLFontFeatureSettings is not in SSO mode. And if the
  //! container is in SSO mode only the number of items used will be overwritten by \ref BLFontFeatureSettings::get_view().
  BLFontFeatureItem _decomposed_sso_data[80];

  //! \}

#ifdef __cplusplus
  //! \name C++ Iterator Compatibility
  //! \{

  //! Tests whether the view is empty.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_empty() const noexcept { return size == 0u; }

   //! Returns a const pointer to \ref BLFontFeatureSettingsView data (iterator compatibility).
  [[nodiscard]] BL_INLINE_NODEBUG const BLFontFeatureItem* begin() const noexcept { return data; }
  //! Returns a const pointer to the end of \ref BLFontFeatureSettingsView data (iterator compatibility).
  [[nodiscard]] BL_INLINE_NODEBUG const BLFontFeatureItem* end() const noexcept { return data + size; }

  //! \}
#endif
};

//! \}

//! \name BLFontFeatureSettings - C API
//! \{

//! \cond INTERNAL

#if defined(__cplusplus)
//! Font feature settings [C API Impl].
struct BLFontFeatureSettingsImpl {};
#endif // __cplusplus

//! \endcond

//! Font feature settings [C API].
struct BLFontFeatureSettingsCore BL_INHERITS_FROM(BLObjectCore) {
  BL_DEFINE_OBJECT(BLFontFeatureSettings, bl::ObjectLayer::ContainerInfo<
    BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS,
    bl::ObjectLayer::ObjectMode::kAny,
    bl::ObjectLayer::ObjectLayout::kNative,
    64u + 6u, 0u, BLObjectDetail::kPL0ByteIndex>
  )

#ifdef __cplusplus
  static_assert(sizeof(BLFontFeatureSettingsView::_decomposed_sso_data) >= sizeof(BLFontFeatureItem) * Info::kSSOCapacity,
                "'BLFontFeatureSettingsView' must be able to hold a copy of all items that 'BLFontFeatureSettingsCore' can store in SSO mode");
#endif
};

BL_EXTERN_C_BEGIN

BL_API BLResult BL_CDECL bl_font_feature_settings_init(BLFontFeatureSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_feature_settings_init_move(BLFontFeatureSettingsCore* self, BLFontFeatureSettingsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_feature_settings_init_weak(BLFontFeatureSettingsCore* self, const BLFontFeatureSettingsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_feature_settings_destroy(BLFontFeatureSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_feature_settings_reset(BLFontFeatureSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_feature_settings_clear(BLFontFeatureSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_feature_settings_shrink(BLFontFeatureSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_feature_settings_assign_move(BLFontFeatureSettingsCore* self, BLFontFeatureSettingsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_feature_settings_assign_weak(BLFontFeatureSettingsCore* self, const BLFontFeatureSettingsCore* other) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL bl_font_feature_settings_get_size(const BLFontFeatureSettingsCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL bl_font_feature_settings_get_capacity(const BLFontFeatureSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_feature_settings_get_view(const BLFontFeatureSettingsCore* self, BLFontFeatureSettingsView* out) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_font_feature_settings_has_value(const BLFontFeatureSettingsCore* self, BLTag feature_tag) BL_NOEXCEPT_C;
BL_API uint32_t BL_CDECL bl_font_feature_settings_get_value(const BLFontFeatureSettingsCore* self, BLTag feature_tag) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_feature_settings_set_value(BLFontFeatureSettingsCore* self, BLTag feature_tag, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_feature_settings_remove_value(BLFontFeatureSettingsCore* self, BLTag feature_tag) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_font_feature_settings_equals(const BLFontFeatureSettingsCore* a, const BLFontFeatureSettingsCore* b) BL_NOEXCEPT_C;

BL_EXTERN_C_END

//! \}
//! \}

//! \addtogroup bl_text
//! \{

//! \name BLFontFeatureSettings - Constants
//! \{

//! A constant representing an invalid font feature value in font feature tag/value pair.
BL_DEFINE_CONST uint32_t BL_FONT_FEATURE_INVALID_VALUE = 0xFFFFFFFFu;

//! \}

//! \name BLFontFeatureSettings - C++ API
//! \{

#ifdef __cplusplus

//! Font feature settings [C++ API].
class BLFontFeatureSettings final : public BLFontFeatureSettingsCore {
public:
  //! \cond INTERNAL
  //! Font features layout used in SSO mode (internal).
  struct alignas(BLObjectDetail) SSO {
    static constexpr size_t kFatCapacity = 6;

    static constexpr uint32_t kFatValueBits = 8u;
    static constexpr uint32_t kFatValueMask = (uint32_t(1) << kFatValueBits) - 1u;

    //! 64 pre-assigned tag ids (0=unassigned | 1=assigned).
    uint64_t bit_ids;
    //! 64 pre-assigned tag values (0=off 1=on).
    uint64_t bit_values;
    //! Fat tag ids and values.
    uint16_t fat_features[kFatCapacity];
    //! The number of features set in SSO mode (regardless of their associated value).
    uint8_t size;
    //! The number of "fat" features stored in `fat_features` array.
    uint8_t fat_count;
    //! Aliases with BLObjectDetail::type_byte.
    uint8_t type_byte;
    //! Aliases with BLObjectDetail::meta_byte.
    uint8_t meta_byte;
  };

  static_assert(sizeof(SSO) == sizeof(BLObjectDetail));
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLFontFeatureSettings() noexcept {
    bl::ObjectLayer::init_sso(this);
  }

  BL_INLINE_NODEBUG BLFontFeatureSettings(BLFontFeatureSettings&& other) noexcept {
    _d = other._d;
    bl::ObjectLayer::init_sso(&other);
  }

  BL_INLINE_NODEBUG BLFontFeatureSettings(const BLFontFeatureSettings& other) noexcept {
    bl_font_feature_settings_init_weak(this, &other);
  }

  BL_INLINE_NODEBUG ~BLFontFeatureSettings() noexcept {
    if (bl::ObjectLayer::requires_destroy(this)) {
      bl_font_feature_settings_destroy(this);
    }
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG BLFontFeatureSettings& operator=(BLFontFeatureSettings&& other) noexcept { bl_font_feature_settings_assign_move(this, &other); return *this; }
  BL_INLINE_NODEBUG BLFontFeatureSettings& operator=(const BLFontFeatureSettings& other) noexcept { bl_font_feature_settings_assign_weak(this, &other); return *this; }

  BL_INLINE_NODEBUG bool operator==(const BLFontFeatureSettings& other) const noexcept { return equals(other); }
  BL_INLINE_NODEBUG bool operator!=(const BLFontFeatureSettings& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult reset() noexcept {
    BLResult result = bl_font_feature_settings_reset(this);

    // Reset operation always succeeds.
    BL_ASSUME(result == BL_SUCCESS);
    // Assume that a default constructed BLFontFeatureSettings has the required flags set/unset.
    BL_ASSUME(_d._sso_data[BLObjectDetail::kMetaByteIndex] == 0u);

    return result;
  }

  BL_INLINE_NODEBUG void swap(BLFontFeatureSettings& other) noexcept { _d.swap(other._d); }

  //! \}

  //! \name Accessors
  //! \{

  //! Tests whether the container is empty, which means that no tag/value pairs are stored in it.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_empty() const noexcept { return bl::ObjectLayer::get_size(this) == 0u; }

  //! Returns the number of feature tag/value pairs stored in the container.
  [[nodiscard]] BL_INLINE_NODEBUG size_t size() const noexcept { return bl::ObjectLayer::get_size(this); }

  //! Returns the container capacity
  //!
  //! \note If the container is in SSO mode, it would return the SSO capacity, however, such capacity can only be used
  //! for simple feature tag/value pairs. Some tags from these can only hold a boolean value (0 or 1) and ther others
  //! can hold a value from 0 to 255. So, if any tag/value pair requires a greater value than 255 it would never be able
  //! to use SSO representation.
  [[nodiscard]] BL_INLINE_NODEBUG size_t capacity() const noexcept { return bl::ObjectLayer::get_capacity(this); }

  //! Returns a normalized view of tag/value pairs as an iterable \ref BLFontFeatureItem array in the output view `out`.
  //!
  //! \note If the container is in SSO mode then all \ref BLFontFeatureItem values will be created from the underlying
  //! SSO representation and \ref BLFontFeatureSettingsView::data will point to \ref BLFontFeatureSettingsView::_decomposed_sso_data.
  //! If the container is dynamic, \ref BLFontFeatureSettingsView::_decomposed_sso_data won't be initialized and
  //! \ref BLFontFeatureSettingsView::data will point to the container's data. This means that the view cannot outlive
  //! the container, and also during iteration the view the container cannot be modified as that could invalidate the
  //! entire view.
  BL_INLINE_NODEBUG BLResult get_view(BLFontFeatureSettingsView* out) const noexcept {
    return bl_font_feature_settings_get_view(this, out);
  }

  //! \}

  //! \name Data Manipulation
  //! \{

  //! Clears the container so it doesn't contain any items.
  //!
  //! If the container holds dynamically allocated data, it won't be freed by `clear()`, use `reset()` if releasing
  //! memory is intended.
  BL_INLINE_NODEBUG BLResult clear() noexcept { return bl_font_feature_settings_clear(this); }

  //! Shrinks the data allocated by the container so its smallest representation.
  //!
  //! \remarks If the container is in SSO mode it's already using the smallest representation. However, if the container
  //! uses dynamically allocated data there are generally two outcomes if the container is not optimal - moving back to
  //! SSO representation in case SSO can represent the data, or reallocating the data to optimal capacity.
  BL_INLINE_NODEBUG BLResult shrink() noexcept { return bl_font_feature_settings_shrink(this); }

  //! Moves `other` feature settings to this container, making `other` default-constructed after the move.
  BL_INLINE_NODEBUG BLResult assign(BLFontFeatureSettingsCore&& other) noexcept { return bl_font_feature_settings_assign_move(this, &other); }
  //! Assigns `other` feature settings to this container, making a weak-copy if `other` uses dynamically allocated memory.
  BL_INLINE_NODEBUG BLResult assign(const BLFontFeatureSettingsCore& other) noexcept { return bl_font_feature_settings_assign_weak(this, &other); }

  //! Tests whether the settings contains the given `feature_tag`.
  [[nodiscard]] BL_INLINE_NODEBUG bool has_value(BLTag feature_tag) const noexcept {
    return bl_font_feature_settings_has_value(this, feature_tag);
  }

  //! Returns the value associated with the given `feature_tag`.
  //!
  //! If the `feature_tag` doesn't exist \ref BL_FONT_FEATURE_INVALID_VALUE is returned.
  [[nodiscard]] BL_INLINE_NODEBUG uint32_t get_value(BLTag feature_tag) const noexcept {
    return bl_font_feature_settings_get_value(this, feature_tag);
  }

  //! Sets or inserts the given `feature_tag` to the settings, associating the `feature_tag` with `value`.
  //!
  //! The `feature_tag` must be valid, which means that it must contain 4 characters within ' ' to '~'
  //! range - [32, 126] in ASCII. If the given `feature_tag` is not valid or `value` is out of range
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
  BL_INLINE_NODEBUG BLResult set_value(BLTag feature_tag, uint32_t value) noexcept {
    return bl_font_feature_settings_set_value(this, feature_tag, value);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult set_value(BLFontFeatureItem feature_item) noexcept {
    return bl_font_feature_settings_set_value(this, feature_item.tag, feature_item.value);
  }

  //! Removes the given `feature_tag` and its associated value from the settings.
  //!
  //! Nothing happens if the `feature_tag` is not in the settings (\ref BL_SUCCESS is returned).
  BL_INLINE_NODEBUG BLResult remove_value(BLTag feature_tag) noexcept {
    return bl_font_feature_settings_remove_value(this, feature_tag);
  }

  //! \}

  //! \name Equality & Comparison
  //! \{

  //! Tests whether this font feature settings is equal to `other` - equality means that it has the same tag/value pairs.
  [[nodiscard]] BL_INLINE_NODEBUG bool equals(const BLFontFeatureSettingsCore& other) const noexcept {
    return bl_font_feature_settings_equals(this, &other);
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_CORE_FONT_FEATURE_SETTINGS_H_INCLUDED
