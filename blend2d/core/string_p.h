// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_STRING_P_H_INCLUDED
#define BLEND2D_CORE_STRING_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/string.h>
#include <blend2d/unicode/unicode_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl::StringDetail {

static BL_INLINE_CONSTEXPR ObjectImplSize impl_size_from_capacity(size_t capacity) noexcept {
  return ObjectImplSize{axl::align_up(1u + capacity, ObjectLayer::kMinImplAlignment)};
}

static BL_INLINE_CONSTEXPR size_t capacity_from_impl_size(ObjectImplSize impl_size) noexcept {
  return size_t(impl_size) - 1u;
}

static constexpr size_t kMaxSize = capacity_from_impl_size(ObjectImplSize{ObjectLayer::kMaxImplSize}) - 1u;

} // {bl::StringDetail}

namespace bl {

template<>
struct ObjectDispatchInfo<BLStringCore> {
  using ImplType = BLStringImpl;

  static constexpr bool kHasInitSSO = true;
  static constexpr size_t kImplDataOffset = 0;

  static BL_INLINE void init_sso(BLStringCore* self) noexcept { ObjectLayer::init_sso(self); }

  static BL_INLINE BLResult free_object(BLStringCore* self, ImplType* impl, ObjectLayer::MetaByte meta_byte) noexcept {
    axl::maybe_unused(self);
    return dealloc_impl(impl, meta_byte);
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_STRING_P_H_INCLUDED
