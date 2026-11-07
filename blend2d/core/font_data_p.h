// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_FONT_DATA_P_H_INCLUDED
#define BLEND2D_CORE_FONT_DATA_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/font_data.h>
#include <blend2d/core/object_layer_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

struct FontDataPrivateImpl : public BLFontDataImpl {
  BLArray<BLFontFaceImpl*> face_cache;
};

static BL_INLINE void font_data_init_impl(FontDataPrivateImpl* impl) noexcept {
  impl->face_count = 0;
  impl->face_type = BL_FONT_FACE_TYPE_NONE;
  impl->flags = 0;
  bl_call_ctor(impl->face_cache);
}

template<>
struct ObjectDispatchInfo<BLFontDataCore> {
  using ImplType = FontDataPrivateImpl;

  static constexpr bool kHasInitSSO = false;

  static BL_INLINE BLResult free_object(BLFontDataCore* self, ImplType* impl, ObjectLayer::MetaByte meta_byte) noexcept {
    axl::maybe_unused(impl, meta_byte);
    return self->_virt()->base.destroy(self);
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_FONT_DATA_P_H_INCLUDED
