// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_FONT_VARIATION_SETTINGS_H_INCLUDED
#define BLEND2D_CORE_FONT_VARIATION_SETTINGS_H_INCLUDED

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
  //! \note If the container is in SSO mode the `data` member will point to `_decomposed_sso_data`.
  const BLFontVariationItem* data;
  //! Count of items in `data.
  size_t size;

  //! Unpacked SSO items into \ref BLFontVariationItem array.
  //!
  //! \note This member won't be initialized or zeroed in case \ref BLFontVariationSettings is not in
  //! SSO mode. And if the container is in SSO mode only the number of items used will be overwritten
  //! by \ref BLFontVariationSettings::get_view().
  BLFontVariationItem _decomposed_sso_data[6];

#ifdef __cplusplus
  //! \name C++ Iterator Compatibility
  //! \{

  //! Tests whether the view is empty.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_empty() const noexcept { return size == 0; }
   //! Returns a const pointer to \ref BLFontVariationSettingsView data (iterator compatibility).
  [[nodiscard]] BL_INLINE_NODEBUG const BLFontVariationItem* begin() const noexcept { return data; }
  //! Returns a const pointer to the end of \ref BLFontVariationSettingsView data (iterator compatibility).
  [[nodiscard]] BL_INLINE_NODEBUG const BLFontVariationItem* end() const noexcept { return data + size; }

  //! \}
#endif
};

//! \}

//! \name BLFontVariationSettings - C API
//! \{

//! Font variation settings [C API].
struct BLFontVariationSettingsCore BL_INHERITS_FROM(BLObjectCore) {
  BL_DEFINE_OBJECT(BLFontVariationSettings, bl::ObjectLayer::ContainerInfo<
    BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS,
    bl::ObjectLayer::ObjectMode::kAny,
    bl::ObjectLayer::ObjectLayout::kNative,
    6u, 0u, BLObjectDetail::kMetaByteIndex>
  )

#ifdef __cplusplus
  static_assert(sizeof(BLFontVariationSettingsView::_decomposed_sso_data) >= sizeof(BLFontVariationItem) * Info::kSSOCapacity,
                "'BLFontVariationSettingsView' must be able to hold a copy of all items that 'BLFontVariationSettingsCore' can store in SSO mode");
#endif
};

//! \cond INTERNAL

#if defined(__cplusplus)
//! Font variation settings [C API Impl].
//!
//! \note This Impl's layout is fully compatible with \ref BLArrayImpl.
struct BLFontVariationSettingsImpl {};
#endif // __cplusplus

//! \endcond

BL_EXTERN_C_BEGIN

BL_API BLResult BL_CDECL bl_font_variation_settings_init(BLFontVariationSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_variation_settings_init_move(BLFontVariationSettingsCore* self, BLFontVariationSettingsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_variation_settings_init_weak(BLFontVariationSettingsCore* self, const BLFontVariationSettingsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_variation_settings_destroy(BLFontVariationSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_variation_settings_reset(BLFontVariationSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_variation_settings_clear(BLFontVariationSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_variation_settings_shrink(BLFontVariationSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_variation_settings_assign_move(BLFontVariationSettingsCore* self, BLFontVariationSettingsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_variation_settings_assign_weak(BLFontVariationSettingsCore* self, const BLFontVariationSettingsCore* other) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL bl_font_variation_settings_get_size(const BLFontVariationSettingsCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL bl_font_variation_settings_get_capacity(const BLFontVariationSettingsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_variation_settings_get_view(const BLFontVariationSettingsCore* self, BLFontVariationSettingsView* out) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_font_variation_settings_has_value(const BLFontVariationSettingsCore* self, BLTag variation_tag) BL_NOEXCEPT_C;
BL_API float BL_CDECL bl_font_variation_settings_get_value(const BLFontVariationSettingsCore* self, BLTag variation_tag) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_variation_settings_set_value(BLFontVariationSettingsCore* self, BLTag variation_tag, float variation_value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_font_variation_settings_remove_value(BLFontVariationSettingsCore* self, BLTag variation_tag) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_font_variation_settings_equals(const BLFontVariationSettingsCore* a, const BLFontVariationSettingsCore* b) BL_NOEXCEPT_C;

BL_EXTERN_C_END

//! \}
//! \}

//! \addtogroup bl_text
//! \{

//! \name BLFontVariationSettings - C++ API
//! \{
#ifdef __cplusplus

//! Font variation settings [C++ API].
class BLFontVariationSettings final : public BLFontVariationSettingsCore {
public:
  //! \cond INTERNAL
  //! SSO representation used by \ref BLFontVariationSettings.
  struct alignas(BLObjectDetail) SSO {
    //! Variation values.
    float values[6];
    //! Variation feature ids.
    uint8_t ids[6];
    //! Aliases with BLObjectDetail::type_byte.
    uint8_t type_byte;
    //! Aliases with BLObjectDetail::meta_byte.
    uint8_t size;
  };

  static_assert(sizeof(SSO) == sizeof(BLObjectDetail));
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLFontVariationSettings() noexcept {
    bl::ObjectLayer::init_sso(this);
  }

  BL_INLINE_NODEBUG BLFontVariationSettings(BLFontVariationSettings&& other) noexcept {
    _d = other._d;
    bl::ObjectLayer::init_sso(&other);
  }

  BL_INLINE_NODEBUG BLFontVariationSettings(const BLFontVariationSettings& other) noexcept {
    bl_font_variation_settings_init_weak(this, &other);
  }

  BL_INLINE_NODEBUG ~BLFontVariationSettings() noexcept {
    if (bl::ObjectLayer::requires_destroy(this)) {
      bl_font_variation_settings_destroy(this);
    }
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG BLFontVariationSettings& operator=(BLFontVariationSettings&& other) noexcept { bl_font_variation_settings_assign_move(this, &other); return *this; }
  BL_INLINE_NODEBUG BLFontVariationSettings& operator=(const BLFontVariationSettings& other) noexcept { bl_font_variation_settings_assign_weak(this, &other); return *this; }

  BL_INLINE_NODEBUG bool operator==(const BLFontVariationSettings& other) const noexcept { return  equals(other); }
  BL_INLINE_NODEBUG bool operator!=(const BLFontVariationSettings& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Resets this font variation settings into its default constructed state - this means clearing the container
  //! and possibly releasing all resources allocated with it.
  BL_INLINE_NODEBUG BLResult reset() noexcept {
    BLResult result = bl_font_variation_settings_reset(this);

    // Reset operation always succeeds.
    BL_ASSUME(result == BL_SUCCESS);
    // Assume that a default constructed BLFontVariationSettings has the required flags set/unset.
    BL_ASSUME(_d._sso_data[BLObjectDetail::kMetaByteIndex] == 0u);

    return result;
  }

  BL_INLINE_NODEBUG void swap(BLFontVariationSettings& other) noexcept { _d.swap(other._d); }

  //! \}

  //! \name Accessors
  //! \{

  //! Tests whether the container is empty, which means that no tag/value pairs are stored in it.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_empty() const noexcept { return size() == 0; }

  //! Returns the number of tag/value pairs stored in the container.
  [[nodiscard]] BL_INLINE_NODEBUG size_t size() const noexcept { return bl::ObjectLayer::get_size(this); }

  //! Returns the container capacity.
  //!
  //! \note If the container is in SSO mode, it would return the SSO capacity, however, such capacity can only be used
  //! for simple tag/value pairs (where the tag is known by Blend2D and has associated an internal ID that represents it).
  [[nodiscard]] BL_INLINE_NODEBUG size_t capacity() const noexcept { return bl::ObjectLayer::get_capacity(this); }

  //! Returns a normalized view of tag/value pairs as an iterable \ref BLFontVariationItem array in the output view.
  //!
  //! \note If the container is in SSO mode then all \ref BLFontVariationItem values will be created from the underlying SSO
  //! representation and \ref BLFontVariationSettingsView::data will point to \ref BLFontVariationSettingsView::_decomposed_sso_data.
  //! If the container is dynamic, \ref BLFontVariationSettingsView::_decomposed_sso_data won't be initialized and
  //! \ref BLFontVariationSettingsView::data will point to the container's data. This means that the view cannot outlive the
  //! container, and also during iteration the view the container cannot be modified
  //! as that could invalidate the entire view.
  BL_INLINE_NODEBUG BLResult get_view(BLFontVariationSettingsView* out) const noexcept {
    return bl_font_variation_settings_get_view(this, out);
  }

  //! \}

  //! \name Data Manipulation
  //! \{

  //! Clears the container so it doesn't contain any items.
  //!
  //! If the container holds dynamically allocated data, it won't be freed by `clear()`, use `reset()` if releasing
  //! memory is intended.
  BL_INLINE_NODEBUG BLResult clear() noexcept { return bl_font_variation_settings_clear(this); }

  //! Shrinks the data allocated by the container so its smallest representation.
  //!
  //! \remarks If the container is in SSO mode it's already using the smallest representation. However, if the container
  //! uses dynamically allocated data there are generally two outcomes if the container is not optimal - moving back to
  //! SSO representation in case SSO can represent the data, or reallocating the data to optimal capacity.
  BL_INLINE_NODEBUG BLResult shrink() noexcept { return bl_font_variation_settings_shrink(this); }

  //! Moves `other` variation settings to this container, making `other` default-constructed after the move.
  BL_INLINE_NODEBUG BLResult assign(BLFontVariationSettings&& other) noexcept { return bl_font_variation_settings_assign_move(this, &other); }
  //! Assigns `other` variation settings to this container, making a weak-copy if `other` uses dynamically allocated memory.
  BL_INLINE_NODEBUG BLResult assign(const BLFontVariationSettings& other) noexcept { return bl_font_variation_settings_assign_weak(this, &other); }


  //! Tests whether the settings contains the given `variation_tag`.
  [[nodiscard]] BL_INLINE_NODEBUG bool has_value(BLTag variation_tag) const noexcept {
    return bl_font_variation_settings_has_value(this, variation_tag);
  }

  //! Returns the value associated with the given `variation_tag`.
  //!
  //! If the `variation_tag` doesn't exist or is invalid `NaN` is returned.
  [[nodiscard]] BL_INLINE_NODEBUG float get_value(BLTag variation_tag) const noexcept {
    return bl_font_variation_settings_get_value(this, variation_tag);
  }

  //! Sets or inserts the given `variation_tag` to the settings and associates it with the given `value`.
  BL_INLINE_NODEBUG BLResult set_value(BLTag variation_tag, float variation_value) noexcept {
    return bl_font_variation_settings_set_value(this, variation_tag, variation_value);
  }

  //! Sets or inserts the given `variation_item` to the settings.
  BL_INLINE_NODEBUG BLResult set_value(const BLFontVariationItem& variation_item) noexcept {
    return bl_font_variation_settings_set_value(this, variation_item.tag, variation_item.value);
  }

  //! Removes the given `variation_tag` and its value from the settings.
  //!
  //! Nothing happens if the `variation_tag` is not in the settings (\ref BL_SUCCESS is returned).
  BL_INLINE_NODEBUG BLResult remove_value(BLTag variation_tag) noexcept {
    return bl_font_variation_settings_remove_value(this, variation_tag);
  }

  //! \}

  //! \name Equality & Comparison
  //! \{

  //! Tests whether this font variation settings is equal to `other` - equality means that it has the same tag/value pairs.
  [[nodiscard]] BL_INLINE_NODEBUG bool equals(const BLFontVariationSettingsCore& other) const noexcept {
    return bl_font_variation_settings_equals(this, &other);
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_CORE_FONT_VARIATION_SETTINGS_H_INCLUDED
