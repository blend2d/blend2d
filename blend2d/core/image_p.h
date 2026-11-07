// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_IMAGE_P_H_INCLUDED
#define BLEND2D_CORE_IMAGE_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/image.h>
#include <blend2d/core/object_layer_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

//! Private implementation that extends \ref BLImageImpl.
struct ImagePrivateImpl : public BLImageImpl {
  //! Count of writers that write to this image.
  //!
  //! Writers don't increase the reference count of the image to keep it mutable. However, we must keep
  //! a counter that would tell the BLImage destructor that a writer exists when `writer_count > 0`.
  size_t writer_count;
};

} // {bl}

namespace bl::ImageDetail {

BLResult free_object(BLImageCore* self, ImagePrivateImpl* impl, ObjectLayer::MetaByte meta_byte) noexcept;

} // {bl::ImageDetail}

namespace bl {

template<>
struct ObjectDispatchInfo<BLImageCore> {
  using ImplType = ImagePrivateImpl;

  static constexpr bool kHasInitSSO = false;
  static constexpr size_t kImplDataOffset = sizeof(ImagePrivateImpl);

  static BL_INLINE BLResult free_object(BLImageCore* self, ImplType* impl, ObjectLayer::MetaByte meta_byte) noexcept {
    return ImageDetail::free_object(self, impl, meta_byte);
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_IMAGE_P_H_INCLUDED
