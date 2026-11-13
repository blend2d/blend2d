// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_IMAGE_ENCODER_P_H_INCLUDED
#define BLEND2D_CORE_IMAGE_ENCODER_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/image_encoder.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

template<>
struct ObjectDispatchInfo<BLImageEncoderCore> {
  using ImplType = BLImageEncoderImpl;

  static constexpr bool kHasInitSSO = false;

  static BL_INLINE BLResult free_object(BLImageEncoderCore* self, ImplType* impl) noexcept {
    axl::maybe_unused(impl);
    return self->_virt()->base.destroy(self);
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_IMAGE_ENCODER_P_H_INCLUDED
