// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RGBA_P_H_INCLUDED
#define BLEND2D_RGBA_P_H_INCLUDED

#include "api-internal_p.h"
#include "math_p.h"
#include "rgba.h"

#if defined(BL_BUILD_OPT_SSE2)
  #include "simd_p.h"
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLRgbaPrivate {

static BL_INLINE bool isValid(const BLRgba& rgba) noexcept {
  return !blIsNaN(rgba.r, rgba.g, rgba.b, rgba.a);
}

static BL_INLINE bool isRgba32FullyOpaque(uint32_t rgba32) noexcept {
  return rgba32 >= 0xFF000000u;
}

static BL_INLINE bool isRgba64FullyOpaque(uint64_t rgba64) noexcept {
  return (rgba64 & 0xFFFF000000000000u) != 0;
}

static BL_INLINE uint32_t packRgba32(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFu) noexcept {
  BL_ASSUME(r <= 0xFFu);
  BL_ASSUME(g <= 0xFFu);
  BL_ASSUME(b <= 0xFFu);
  BL_ASSUME(a <= 0xFFu);

  return (a << 24) | (r << 16) | (g << 8) | (b);
}

static BL_INLINE uint64_t packRgba64(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFFFu) noexcept {
  BL_ASSUME(r <= 0xFFFFu);
  BL_ASSUME(g <= 0xFFFFu);
  BL_ASSUME(b <= 0xFFFFu);
  BL_ASSUME(a <= 0xFFFFu);

  uint32_t ar = (a << 16) | r;
  uint32_t gb = (g << 16) | b;

  return (uint64_t(ar) << 32) | gb;
}

static BL_INLINE uint64_t rgba64FromRgba32(uint32_t src) noexcept {
#if defined(BL_BUILD_OPT_SSE2)
  using namespace SIMD;
  Vec128I src128 = v_i128_from_u32(src);
  return v_get_u64(v_interleave_lo_i8(src128, src128));
#else
  return BLRgba64(BLRgba32(src)).value;
#endif
}

static BL_INLINE uint32_t rgba32FromRgba64(uint64_t src) noexcept {
#if defined(BL_BUILD_OPT_SSE2)
  using namespace SIMD;
  return v_get_u32(v_packs_i16_u8(v_srl_i16<8>(v_i128_from_u64(src))));
#else
  return BLRgba32(BLRgba64(src)).value;
#endif
}

} // {BLRgbaPrivate}

//! \}
//! \endcond

#endif // BLEND2D_RGBA_P_H_INCLUDED
