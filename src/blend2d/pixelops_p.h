// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_PIXELOPS_P_H
#define BLEND2D_PIXELOPS_P_H

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

  I128 p0 = vcvtu32i128(val32);
  I128 a0 = vcvtu32i128(_a | 0x00FF0000u);

  p0 = vmovli64u8u16(p0);
  a0 = vswizli16<1, 0, 0, 0>(a0);
  p0 = vdiv255u16(vmuli16(p0, a0));
  p0 = vpackzzwb(p0);
  return vcvti128u32(p0);
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

  I128 p0 = vmovli64u8u16(vcvtu32i128(val32));
  I128 a0 = vswizli16<3, 3, 3, 3>(p0);

  p0 = vor(p0, v_const_as<I128>(blCommonTable.i128_00FF000000000000));
  p0 = vdiv255u16(vmuli16(p0, a0));
  p0 = vpackzzwb(p0);
  return vcvti128u32(p0);
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

#endif // BLEND2D_PIXELOPS_P_H
