// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_STROKE_OPTIONS_P_H_INCLUDED
#define BLEND2D_CORE_STROKE_OPTIONS_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/stroke_options.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

template<>
struct ObjectDispatchInfo<BLStrokeOptionsCore> {
  using ImplType = BLStrokeOptionsImpl;

  static constexpr bool kHasInitSSO = true;
  static constexpr size_t kImplDataOffset = 0;

  static BL_INLINE void init_sso(BLStrokeOptionsCore* self) noexcept { BLStrokeOptions::_init_default(self->_d); }

  static BL_INLINE BLResult free_object(BLStrokeOptionsCore* self, ImplType* impl) noexcept {
    axl::maybe_unused(self);
    return ObjectLayer::dealloc_impl(impl);
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_STROKE_OPTIONS_P_H_INCLUDED
