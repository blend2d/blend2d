// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_PIXELOPS_P_H_INCLUDED
#define BLEND2D_PIXELOPS_P_H_INCLUDED

#include "./api-internal_p.h"
#include "./rgba_p.h"
#include "./simd_p.h"
#include "./tables_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLPixelOps {

// ============================================================================
// [PixelOps - Utilities]
// ============================================================================

static BL_INLINE void unpremultiply_rgb_8bit(uint32_t& r, uint32_t& g, uint32_t& b, uint32_t a) noexcept {
  uint32_t recip = blCommonTable.unpremultiplyRcp[a];
  r = (r * recip + 0x8000u) >> 16;
  g = (g * recip + 0x8000u) >> 16;
  b = (b * recip + 0x8000u) >> 16;
}

// ============================================================================
// [PixelOps - Conversion]
// ============================================================================

static BL_INLINE uint32_t xrgb32_0888_from_xrgb16_0555(uint32_t src) noexcept {
  uint32_t t0 = src;       // [00000000] [00000000] [XRRRRRGG] [GGGBBBBB]
  uint32_t t1;
  uint32_t t2;

  t0 = (t0 * 0x00080008u); // [RRRGGGGG] [BBBBBXRR] [RRRGGGGG] [BBBBB000]
  t0 = (t0 & 0x1F03E0F8u); // [000GGGGG] [000000RR] [RRR00000] [BBBBB000]
  t0 = (t0 | (t0 >> 5));   // [000GGGGG] [GGGGG0RR] [RRRRRRRR] [BBBBBBBB]

  t1 = t0 >> 13;           // [00000000] [00000000] [GGGGGGGG] [GG0RRRRR]
  t2 = t0 << 6;            // [GGGGGGG0] [RRRRRRRR] [RRBBBBBB] [BB000000]

  t0 &= 0x000000FFu;       // [00000000] [00000000] [00000000] [BBBBBBBB]
  t1 &= 0x0000FF00u;       // [00000000] [00000000] [GGGGGGGG] [00000000]
  t2 &= 0x00FF0000u;       // [00000000] [RRRRRRRR] [00000000] [00000000]

  return 0xFF000000u | t0 | t1 | t2;
}

static BL_INLINE uint32_t xrgb32_0888_from_xrgb16_0565(uint32_t src) noexcept {
  uint32_t t0 = src;       // [00000000] [00000000] [RRRRRGGG] [GGGBBBBB]
  uint32_t t1 = src;
  uint32_t t2;

  t0 = (t0 & 0x0000F81Fu); // [00000000] [00000000] [RRRRR000] [000BBBBB]
  t1 = (t1 & 0x000007E0u); // [00000000] [00000000] [00000GGG] [GGG00000]

  t0 = (t0 * 0x21u);       // [00000000] [000RRRRR] [RRRRR0BB] [BBBBBBBB]
  t1 = (t1 * 0x41u);       // [00000000] [0000000G] [GGGGGGGG] [GGG00000]

  t2 = (t0 << 3);          // [00000000] [RRRRRRRR] [RR0BBBBB] [BBBBB000]
  t0 = (t0 >> 2);          // [00000000] [00000RRR] [RRRRRRR0] [BBBBBBBB]
  t1 = (t1 >> 1);          // [00000000] [00000000] [GGGGGGGG] [GGGG0000]

  t0 = t0 & 0x000000FFu;   // [00000000] [00000000] [00000000] [BBBBBBBB]
  t1 = t1 & 0x0000FF00u;   // [00000000] [00000000] [GGGGGGGG] [00000000]
  t2 = t2 & 0x00FF0000u;   // [00000000] [RRRRRRRR] [00000000] [00000000]

  return 0xFF000000u | t0 | t1 | t2;
}

static BL_INLINE uint32_t argb32_8888_from_argb16_4444(uint32_t src) noexcept {
  uint32_t t0 = src;       // [00000000] [00000000] [AAAARRRR] [GGGGBBBB]
  uint32_t t1;
  uint32_t t2;

  t1 = t0 << 12;           // [0000AAAA] [RRRRGGGG] [BBBB0000] [00000000]
  t2 = t0 << 4;            // [00000000] [0000AAAA] [RRRRGGGG] [BBBB0000]

  t0 = t0 | t1;            // [0000AAAA] [RRRRGGGG] [XXXXRRRR] [GGGGBBBB]
  t1 = t2 << 4;            // [00000000] [AAAARRRR] [GGGGBBBB] [00000000]

  t0 &= 0x0F00000Fu;       // [0000AAAA] [00000000] [00000000] [0000BBBB]
  t1 &= 0x000F0000u;       // [00000000] [0000RRRR] [00000000] [00000000]
  t2 &= 0x00000F00u;       // [00000000] [00000000] [0000GGGG] [00000000]

  t0 += t1;                // [0000AAAA] [0000RRRR] [00000000] [0000BBBB]
  t0 += t2;                // [0000AAAA] [0000RRRR] [0000GGGG] [0000BBBB]

  return t0 * 0x11u;       // [AAAAAAAA] [RRRRRRRR] [GGGGGGGG] [BBBBBBBB]
}

// --------------------------------------------------------------------------
// [Premultiply / Unpremultiply]
// --------------------------------------------------------------------------

static BL_INLINE uint32_t prgb32_8888_from_argb32_8888(uint32_t val32, uint32_t _a) noexcept {
#if BL_TARGET_SIMD_I
  using namespace SIMD;

  Vec128I p0 = v_i128_from_u32(val32);
  Vec128I a0 = v_i128_from_u32(_a | 0x00FF0000u);

  p0 = v_unpack_lo_u8_u16(p0);
  a0 = v_swizzle_lo_i16<1, 0, 0, 0>(a0);
  p0 = v_div255_u16(v_mul_i16(p0, a0));
  p0 = v_packz_u16_u8(p0);
  return v_get_u32(p0);
#else
  uint32_t rb = val32;
  uint32_t ag = val32;

  ag |= 0xFF000000u;
  ag >>= 8;

  rb = (rb & 0x00FF00FFu) * _a;
  ag = (ag & 0x00FF00FFu) * _a;

  rb += 0x00800080u;
  ag += 0x00800080u;

  rb = (rb + ((rb >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;
  ag = (ag + ((ag >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;

  rb >>= 8;
  return ag | rb;
#endif
}

static BL_INLINE uint32_t prgb32_8888_from_argb32_8888(uint32_t val32) noexcept {
#if BL_TARGET_SIMD_I
  using namespace SIMD;

  Vec128I p0 = v_unpack_lo_u8_u16(v_i128_from_u32(val32));
  Vec128I a0 = v_swizzle_lo_i16<3, 3, 3, 3>(p0);

  p0 = v_or(p0, v_const_as<Vec128I>(blCommonTable.i128_00FF000000000000));
  p0 = v_div255_u16(v_mul_i16(p0, a0));
  p0 = v_packz_u16_u8(p0);
  return v_get_u32(p0);
#else
  return prgb32_8888_from_argb32_8888(val32, val32 >> 24);
#endif
}

static BL_INLINE uint32_t argb32_8888_from_prgb32_8888(uint32_t val32) noexcept {
  uint32_t r = ((val32 >> 16) & 0xFFu);
  uint32_t g = ((val32 >>  8) & 0xFFu);
  uint32_t b = ((val32      ) & 0xFFu);
  uint32_t a = ((val32 >> 24));

  BLPixelOps::unpremultiply_rgb_8bit(r, g, b, a);
  return (a << 24) | (r << 16) | (g << 8) | b;
}

static BL_INLINE uint32_t abgr32_8888_from_prgb32_8888(uint32_t val32) noexcept {
  uint32_t r = ((val32 >> 16) & 0xFFu);
  uint32_t g = ((val32 >>  8) & 0xFFu);
  uint32_t b = ((val32      ) & 0xFFu);
  uint32_t a = ((val32 >> 24));

  BLPixelOps::unpremultiply_rgb_8bit(r, g, b, a);
  return (a << 24) | (b << 16) | (g << 8) | r;
}

} // {BLPixelOps}

//! \}
//! \endcond

#endif // BLEND2D_PIXELOPS_P_H_INCLUDED
