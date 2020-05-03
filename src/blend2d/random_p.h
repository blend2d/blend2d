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

#ifndef BLEND2D_RANDOM_P_H_INCLUDED
#define BLEND2D_RANDOM_P_H_INCLUDED

#include "./random.h"
#include "./simd_p.h"
#include "./support_p.h"

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

  Vec128I x = v_load_i64(&self->data[0]);
  Vec128I y = v_load_i64(&self->data[1]);

  x = v_xor(x, v_sll_i64<BL_RANDOM_STEP1_SHL>(x));
  y = v_xor(y, v_srl_i64<BL_RANDOM_STEP3_SHR>(y));
  x = v_xor(x, v_srl_i64<BL_RANDOM_STEP2_SHR>(x));
  x = v_xor(x, y);
  v_store_i64(&self->data[0], y);
  v_store_i64(&self->data[1], x);

  return v_add_i64(x, y);
}

BL_INLINE double blRandomNextDoubleInline(BLRandom* self) noexcept {
  using namespace SIMD;

  Vec128I kExpMsk128 = _mm_set_epi32(0x3FF00000, 0, 0x3FF00000, 0);
  Vec128I x = blRandomNextUInt64AsI128Inline(self);
  Vec128I y = v_srl_i64<BL_RANDOM_MANTISSA_SHIFT>(x);
  Vec128I z = v_or(y, kExpMsk128);
  return v_get_f64(v_cast<Vec128D>(z)) - 1.0;
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

#endif // BLEND2D_RANDOM_P_H_INCLUDED
