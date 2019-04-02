// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLRANDOM_P_H
#define BLEND2D_BLRANDOM_P_H

#include "./blrandom.h"
#include "./blsimd_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! Shift constants used by `BLRandom` implementation.
enum BLInternalRandomShifts : uint32_t {
  // Constants suggested as `23/18/5`.
  BL_RANDOM_STEP1_SHL = 23,
  BL_RANDOM_STEP2_SHR = 18,
  BL_RANDOM_STEP3_SHR = 5,

  // Number of bits needed to shift right to extract mantissa.
  BL_RANDOM_MANTISSA_SHIFT = 64 - 52
};

// ============================================================================
// [BLRandom - Inline]
// ============================================================================

namespace {

BL_INLINE void blRandomResetInline(BLRandom* self, uint64_t seed) noexcept {
  // The number is arbitrary, it means nothing.
  const uint64_t kZeroSeed = 0x1F0A2BE71D163FA0u;

  // Generate the state data by using splitmix64.
  for (uint32_t i = 0; i < 2; i++) {
    seed += 0x9E3779B97F4A7C15u;
    uint64_t x = seed;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9u;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBu;
    x = (x ^ (x >> 31));
    self->data[i] = x != 0 ? x : kZeroSeed;
  }
}

BL_INLINE uint64_t blRandomNextUInt64Inline(BLRandom* self) noexcept {
  uint64_t x = self->data[0];
  uint64_t y = self->data[1];

  x ^= x << BL_RANDOM_STEP1_SHL;
  y ^= y >> BL_RANDOM_STEP3_SHR;
  x ^= x >> BL_RANDOM_STEP2_SHR;
  x ^= y;

  self->data[0] = y;
  self->data[1] = x;

  return x + y;
}

BL_INLINE uint32_t blRandomNextUInt32Inline(BLRandom* self) noexcept {
  return uint32_t(blRandomNextUInt64Inline(self) >> 32);
}

#ifdef BL_TARGET_OPT_SSE2
//! High-performance SIMD implementation. Better utilizes CPU in 32-bit mode
//! and it's a better candidate for `blRandomNextDouble()` in general on X86 as
//! it returns a SIMD register, which is easier to convert to `double` than GP.
BL_INLINE __m128i blRandomNextUInt64AsI128Inline(BLRandom* self) noexcept {
  using namespace SIMD;

  I128 x = vloadi128_64(&self->data[0]);
  I128 y = vloadi128_64(&self->data[1]);

  x = vxor(x, vslli64<BL_RANDOM_STEP1_SHL>(x));
  y = vxor(y, vsrli64<BL_RANDOM_STEP3_SHR>(y));
  x = vxor(x, vsrli64<BL_RANDOM_STEP2_SHR>(x));
  x = vxor(x, y);
  vstorei64(&self->data[0], y);
  vstorei64(&self->data[1], x);

  return vaddi64(x, y);
}

BL_INLINE double blRandomNextDoubleInline(BLRandom* self) noexcept {
  using namespace SIMD;

  I128 kExpMsk128 = _mm_set_epi32(0x3FF00000, 0, 0x3FF00000, 0);
  I128 x = blRandomNextUInt64AsI128Inline(self);
  I128 y = vsrli64<BL_RANDOM_MANTISSA_SHIFT>(x);
  I128 z = vor(y, kExpMsk128);
  return vcvtd128d64(vcast<D128>(z)) - 1.0;
}
#else
BL_INLINE double blRandomNextDoubleInline(BLRandom* self) noexcept {
  uint64_t kExpMsk = 0x3FF0000000000000u;
  uint64_t u = (blRandomNextUInt64Inline(self) >> BL_RANDOM_MANTISSA_SHIFT) | kExpMsk;
  return blBitCast<double>(u) - 1.0;
}
#endif

} // {anonymous}

//! \}
//! \endcond

#endif // BLEND2D_BLRANDOM_P_H
