// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLRGBA_P_H
#define BLEND2D_BLRGBA_P_H

#include "./blapi-internal_p.h"
#include "./blrgba.h"

#if defined(BL_BUILD_OPT_SSE2)
  #include "./blsimd_p.h"
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLRgba - Utilities]
// ============================================================================

namespace {

static BL_INLINE bool blRgba32IsFullyOpaque(uint32_t rgba32) noexcept {
  return rgba32 >= 0xFF000000u;
}

static BL_INLINE bool blRgba64IsFullyOpaque(uint64_t rgba64) noexcept {
  return (rgba64 & 0xFFFF000000000000u) != 0;
}

static BL_INLINE uint32_t blRgba32Pack(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFu) noexcept {
  BL_ASSUME(r <= 0xFFu);
  BL_ASSUME(g <= 0xFFu);
  BL_ASSUME(b <= 0xFFu);
  BL_ASSUME(a <= 0xFFu);

  return (a << 24) | (r << 16) | (g << 8) | (b);
}

static BL_INLINE uint32_t blRgba64Pack(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFFFFu) noexcept {
  BL_ASSUME(r <= 0xFFFFu);
  BL_ASSUME(g <= 0xFFFFu);
  BL_ASSUME(b <= 0xFFFFu);
  BL_ASSUME(a <= 0xFFFFu);

  uint32_t ar = (a << 16) | r;
  uint32_t gb = (g << 16) | b;

  return (uint64_t(ar) << 32) | gb;
}

static BL_INLINE uint64_t blRgba64FromRgba32(uint32_t src) noexcept {
#if defined(BL_BUILD_OPT_SSE2)
  using namespace SIMD;
  I128 src128 = vcvtu32i128(src);
  return vcvti128u64(vunpackli8(src128, src128));
#else
  return BLRgba64(BLRgba32(src)).value;
#endif
}

static BL_INLINE uint32_t blRgba32FromRgba64(uint64_t src) noexcept {
#if defined(BL_BUILD_OPT_SSE2)
  using namespace SIMD;
  return vcvti128u32(vpacki16u8(vsrli16<8>(vcvtu64i128(src))));
#else
  return BLRgba32(BLRgba64(src)).value;
#endif
}

} // {anonymous}

//! \}
//! \endcond

#endif // BLEND2D_BLRGBA_P_H
