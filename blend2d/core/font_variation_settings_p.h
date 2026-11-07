// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_FONT_VARIATION_SETTINGS_P_H_INCLUDED
#define BLEND2D_CORE_FONT_VARIATION_SETTINGS_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/font_variation_settings.h>
#include <blend2d/core/object_layer_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl::FontVariationSettingsDetail {

static BL_INLINE constexpr ObjectImplSize impl_size_from_capacity(size_t capacity) noexcept {
  return axl::align_up(ObjectImplSize(capacity * sizeof(BLFontVariationItem)), ObjectLayer::kMinImplAlignment);
}

static BL_INLINE constexpr size_t capacity_from_impl_size(ObjectImplSize impl_size) noexcept {
  return size_t(impl_size) / sizeof(BLFontVariationItem);
}

} // {bl::FontVariationSettingsDetail}

namespace bl {

template<>
struct ObjectDispatchInfo<BLFontVariationSettingsCore> {
  using ImplType = BLFontVariationSettingsImpl;

  static constexpr bool kHasInitSSO = true;
  static constexpr size_t kImplDataOffset = 0;

  static BL_INLINE void init_sso(BLFontVariationSettingsCore* self) noexcept { ObjectLayer::init_sso(self); }

  static BL_INLINE BLResult free_object(BLFontVariationSettingsCore* self, ImplType* impl, ObjectLayer::MetaByte meta_byte) noexcept {
    axl::maybe_unused(self);
    return dealloc_impl(impl, meta_byte);
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_FONT_VARIATION_SETTINGS_P_H_INCLUDED
