// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_BIT_ARRAY_P_H_INCLUDED
#define BLEND2D_CORE_BIT_ARRAY_P_H_INCLUDED

#include <blend2d/axl/algorithm_p.h>
#include <blend2d/axl/bitops_p.h>
#include <blend2d/axl/commons.h>
#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/bit_array.h>
#include <blend2d/core/object_layer_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

template<>
struct ObjectDispatchInfo<BLBitArrayCore> {
  using ImplType = BLBitArrayImpl;

  static constexpr bool kHasInitSSO = true;
  static constexpr size_t kImplDataOffset = 0;

  static BL_INLINE void init_sso(BLBitArrayCore* self) noexcept { ObjectLayer::init_sso(self); }

  static BL_INLINE BLResult free_object(BLBitArrayCore* self, ImplType* impl, ObjectLayer::MetaByte meta_byte) noexcept {
    axl::maybe_unused(self);
    return dealloc_impl(impl, meta_byte);
  }
};

using BitOps = ParametrizedBitOps<BitOrder::kLSB, BLBitWord>;

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_BIT_ARRAY_P_H_INCLUDED
