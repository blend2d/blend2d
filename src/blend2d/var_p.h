
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
static BL_INLINE BLResult initRgba(BLObjectCore* self, const BLRgba* rgba) noexcept {
  uint32_t r = blBitCast<uint32_t>(rgba->r);
  uint32_t g = blBitCast<uint32_t>(rgba->g);
  uint32_t b = blBitCast<uint32_t>(rgba->b);
  uint32_t a = blMax<uint32_t>(blBitCast<uint32_t>(rgba->a), 0);

  self->_d.initU32x4(r, g, b, a);
  return BL_SUCCESS;
}

//! \}

} // {VarInternal}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_VAR_P_H_INCLUDED
