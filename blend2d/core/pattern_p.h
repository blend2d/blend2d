// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_PATTERN_P_H_INCLUDED
#define BLEND2D_CORE_PATTERN_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/pattern.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{


namespace bl {

//! Private implementation that extends \ref BLPatternImpl.
struct PatternPrivateImpl : public BLPatternImpl {
  // Nothing at the moment.
};

} // {bl}

namespace bl::PatternDetail {

static BL_INLINE bool is_area_valid(const BLRectI& area, const BLSizeI& size) noexcept {
  typedef unsigned U;
  return bool((U(area.x) < U(size.w)) &
              (U(area.y) < U(size.h)) &
              (U(area.w) <= U(size.w) - U(area.x)) &
              (U(area.h) <= U(size.h) - U(area.y)));
}

static BL_INLINE bool is_area_valid_and_non_zero(const BLRectI& area, const BLSizeI& size) noexcept {
  typedef unsigned U;
  return bool((U(area.x) < U(size.w)) &
              (U(area.y) < U(size.h)) &
              (U(area.w) - U(1) < U(size.w) - U(area.x)) &
              (U(area.h) - U(1) < U(size.h) - U(area.y)));
}

BLResult free_object(BLPatternCore* self, PatternPrivateImpl* impl) noexcept;

} // {bl::PatternDetail}

namespace bl {

template<>
struct ObjectDispatchInfo<BLPatternCore> {
  using ImplType = PatternPrivateImpl;

  static constexpr bool kHasInitSSO = false;

  static BL_INLINE BLResult free_object(BLPatternCore* self, ImplType* impl) noexcept {
    return PatternDetail::free_object(self, impl);
  }
};

} // {bl}

//! \}

//! \}
//! \endcond

#endif // BLEND2D_CORE_PATTERN_P_H_INCLUDED
