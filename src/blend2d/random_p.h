// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RANDOM_P_H_INCLUDED
#define BLEND2D_RANDOM_P_H_INCLUDED

#include "random.h"
#include "simd_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLRandomPrivate {

//! \name Constants
//! \{

// Constants suggested as `23/18/5`.
static constexpr uint32_t kStep1Shift = 23;
static constexpr uint32_t kStep2Shift = 18;
static constexpr uint32_t kStep3Shift = 5;

//! Number of bits needed to shift right to extract mantissa.
static constexpr uint32_t kMantissaShift = 64 - 52;

//! \}

//! \name Inline API (Private)
//! \{

static BL_INLINE void resetSeed(BLRandom* self, uint64_t seed) noexcept {
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

static BL_INLINE uint64_t nextUInt64(BLRandom* self) noexcept {
  uint64_t x = self->data[0];
  uint64_t y = self->data[1];

  x ^= x << kStep1Shift;
  y ^= y >> kStep3Shift;
  x ^= x >> kStep2Shift;
  x ^= y;

  self->data[0] = y;
  self->data[1] = x;

  return x + y;
}

static BL_INLINE uint32_t nextUInt32(BLRandom* self) noexcept {
  return uint32_t(nextUInt64(self) >> 32);
}

#ifdef BL_TARGET_OPT_SSE2

//! High-performance SIMD implementation. Better utilizes CPU in 32-bit mode and it's a better candidate for
//! `blRandomNextDouble()` in general on X86 as it returns a SIMD register, which is easier to convert to `double`
//! than GP.
static BL_INLINE __m128i nextUInt64AsI128(BLRandom* self) noexcept {
  using namespace SIMD;

  Vec128I x = v_load_i64(&self->data[0]);
  Vec128I y = v_load_i64(&self->data[1]);

  x = v_xor(x, v_sll_i64<kStep1Shift>(x));
  y = v_xor(y, v_srl_i64<kStep3Shift>(y));
  x = v_xor(x, v_srl_i64<kStep2Shift>(x));
  x = v_xor(x, y);
  v_store_i64(&self->data[0], y);
  v_store_i64(&self->data[1], x);

  return v_add_i64(x, y);
}

static BL_INLINE double nextDouble(BLRandom* self) noexcept {
  using namespace SIMD;

  Vec128I kExpMsk128 = _mm_set_epi32(0x3FF00000, 0, 0x3FF00000, 0);
  Vec128I x = nextUInt64AsI128(self);
  Vec128I y = v_srl_i64<kMantissaShift>(x);
  Vec128I z = v_or(y, kExpMsk128);
  return v_get_f64(v_cast<Vec128D>(z)) - 1.0;
}

#else

static BL_INLINE double nextDouble(BLRandom* self) noexcept {
  uint64_t kExpMsk = 0x3FF0000000000000u;
  uint64_t u = (nextUInt64(self) >> kMantissaShift) | kExpMsk;
  return blBitCast<double>(u) - 1.0;
}

#endif

//! \}

} // {BLRandomPrivate}

//! \}
//! \endcond

#endif // BLEND2D_RANDOM_P_H_INCLUDED
