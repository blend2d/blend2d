
// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_VAR_P_H_INCLUDED
#define BLEND2D_VAR_P_H_INCLUDED

#include "api-internal_p.h"
#include "math_p.h"
#include "object_p.h"
#include "rgba_p.h"
#include "var.h"

//! \cond INTERNAL
//! \addtogroup blend2d_api_globals
//! \{

namespace BLVarPrivate {

//! \name Variant - Internals - Initialization
//! \{

//! Initializes BLVar with \ref BLRgba and protects BLObjectInfo from payload that would conflict with BLObject tag.
static BL_INLINE BLResult initRgba(BLObjectCore* self, const BLRgba* rgba) noexcept {
  uint32_t r = blBitCast<uint32_t>(rgba->r);
  uint32_t g = blBitCast<uint32_t>(rgba->g);
  uint32_t b = blBitCast<uint32_t>(rgba->b);
  uint32_t a = blBitCast<uint32_t>(blMax(0.0f, rgba->a)) & 0x7FFFFFFFu;

  self->_d.initU32x4(r, g, b, a);
  return BL_SUCCESS;
}

//! Initializes BLVar with \ref BLRgba converted from `rgba32`.
static BL_INLINE BLResult initRgba32(BLObjectCore* self, BLRgba32 rgba32) noexcept {
  BLRgba rgba(rgba32);
  self->_d.initF32x4(rgba.r, rgba.g, rgba.b, rgba.a);
  return BL_SUCCESS;
}

//! Initializes BLVar with \ref BLRgba converted from `rgba64`.
static BL_INLINE BLResult initRgba64(BLObjectCore* self, BLRgba64 rgba64) noexcept {
  BLRgba rgba(rgba64);
  self->_d.initF32x4(rgba.r, rgba.g, rgba.b, rgba.a);
  return BL_SUCCESS;
}

//! \}

} // {BLVarPrivate}

//! \}
//! \endcond

#endif // BLEND2D_VAR_P_H_INCLUDED
