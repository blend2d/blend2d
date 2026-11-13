// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_ARRAY_P_H_INCLUDED
#define BLEND2D_CORE_ARRAY_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/array.h>
#include <blend2d/core/object_layer_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

BLResult array_free_object(BLArrayCore* self, BLArrayImpl* impl) noexcept;
BLResult array_prepend_possibly_aliased_data_inplace(uint8_t* dst, size_t dst_size, const uint8_t* src, size_t src_size) noexcept;
BLResult array_replace_possibly_aliased_data_inplace(uint8_t* dst, size_t dst_size, const uint8_t* src, size_t src_size, size_t replace_size) noexcept;

template<>
struct ObjectDispatchInfo<BLArrayCore> {
  using ImplType = BLArrayImpl;

  static constexpr bool kHasInitSSO = false;
  static constexpr size_t kImplDataOffset = 0;

  static BL_INLINE BLResult free_object(BLArrayCore* self, ImplType* impl) noexcept {
    return array_free_object(self, impl);
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_ARRAY_P_H_INCLUDED
