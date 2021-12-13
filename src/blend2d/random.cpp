// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "random_p.h"
#include "simd_p.h"

// BLRandom - API - Reset
// ======================

BL_API_IMPL BLResult blRandomReset(BLRandom* self, uint64_t seed) noexcept {
  BLRandomPrivate::resetSeed(self, seed);
  return BL_SUCCESS;
}

// BLRandom - API - Next
// =====================

BL_API_IMPL double blRandomNextDouble(BLRandom* self) noexcept {
  return BLRandomPrivate::nextDouble(self);
}

BL_API_IMPL uint32_t blRandomNextUInt32(BLRandom* self) noexcept {
  return BLRandomPrivate::nextUInt32(self);
}

BL_API_IMPL uint64_t blRandomNextUInt64(BLRandom* self) noexcept {
  return BLRandomPrivate::nextUInt64(self);
}

// BLRandom - Tests
// ================

#ifdef BL_TEST
UNIT(random) {
  // Number of iterations for tests that use loop.
  enum { kCount = 1000000 };

  #if defined(BL_TARGET_OPT_SSE2)
  INFO("Testing whether the SIMD implementation matches the C++ one");
  {
    BLRandom a(0);
    BLRandom b(0);

    SIMD::Vec128I bLo = BLRandomPrivate::nextUInt64AsI128(&b);
    SIMD::Vec128I bHi = SIMD::v_swizzle_i32<2, 3, 0, 1>(bLo);

    uint64_t aVal = a.nextUInt64();
    uint64_t bVal = (uint64_t(SIMD::v_get_i32(bLo))      ) +
                    (uint64_t(SIMD::v_get_i32(bHi)) << 32) ;

    EXPECT_EQ(aVal, bVal);
  }
  #endif

  INFO("Testing whether the random 32-bit integer is the HI part of the 64-bit one");
  {
    BLRandom a(0);
    BLRandom b(0);
    EXPECT_EQ(uint32_t(a.nextUInt64() >> 32), b.nextUInt32());
  }

  INFO("Test whether returned double precision values satisfy [0..1) condition");
  {
    // Supply a low-entropy seed on purpose.
    BLRandom rnd(3);

    uint32_t below = 0;
    uint32_t above = 0;

    for (uint32_t i = 0; i < kCount; i++) {
      double x = rnd.nextDouble();
      below += x <  0.5;
      above += x >= 0.5;
      EXPECT_GE(x, 0.0);
      EXPECT_LT(x, 1.0);
    }
    INFO("  Random numbers at [0.0, 0.5): %u of %u", below, kCount);
    INFO("  Random numbers at [0.5, 1.0): %u of %u", above, kCount);
  }
}
#endif
