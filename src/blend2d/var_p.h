
// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_VAR_P_H_INCLUDED
#define BLEND2D_VAR_P_H_INCLUDED

#include "api-internal_p.h"
#include "object_p.h"
#include "rgba_p.h"
#include "var.h"
#include "support/math_p.h"

//! \cond INTERNAL
//! \addtogroup bl_globals
//! \{

namespace bl {
namespace VarInternal {

//! \name Variant - Internals - Initialization
//! \{

//! Initializes BLVar with \ref BLRgba and protects BLObjectInfo from payload that would conflict with BLObject tag.
static BL_INLINE BLResult init_rgba(BLObjectCore* self, const BLRgba* rgba) noexcept {
  uint32_t r = bl_bit_cast<uint32_t>(rgba->r);
  uint32_t g = bl_bit_cast<uint32_t>(rgba->g);
  uint32_t b = bl_bit_cast<uint32_t>(rgba->b);
  uint32_t a = bl_max<uint32_t>(bl_bit_cast<uint32_t>(rgba->a), 0);

  self->_d.init_u32x4(r, g, b, a);
  return BL_SUCCESS;
}

//! \}

} // {VarInternal}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_VAR_P_H_INCLUDED
