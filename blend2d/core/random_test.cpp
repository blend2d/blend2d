// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/axl/commons.h>
#include <blend2d/core/random_p.h>
#include <blend2d/simd/simd_p.h>

namespace bl::RandomTests {

// Random - Tests
// ==============

// Number of iterations for tests that use loop.
static constexpr uint32_t kIterCount = 1000000;

TEST_CASE(core_random_uint) {
  BLRandom a(0x12345678u);
  BLRandom b(0x12345678u);

  for (uint32_t i = 0; i < kIterCount; i++) {
    EXPECT_EQ(uint32_t(a.next_uint64() >> 32), b.next_uint32()).message("Iteration %u");
  }
}

TEST_CASE(core_random_float64) {
  BLRandom rnd(0x1); // Supply a low-entropy seed on purpose.
  uint32_t distribution[10] {};

  for (uint32_t i = 0; i < kIterCount; i++) {
    double val = rnd.next_double();
    EXPECT_GE(val, 0.0);
    EXPECT_LT(val, 1.0);

    size_t di = axl::min<size_t>(static_cast<uint32_t>(val * 10.0), BL_ARRAY_SIZE(distribution) - 1);
    distribution[di]++;
  }

  for (uint32_t di = 0; di < 10; di++) {
    double dist_from = static_cast<double>(di + 0u) / 10.0;
    double dist_to = static_cast<double>(di + 1u) / 10.0;
    double percentage = static_cast<double>(distribution[di]) / double(kIterCount) * 100.0;
    TEST_LOG("  Distribution [%3.1f..%3.1f): (%5.3f%%)", dist_from, dist_to, percentage);
    EXPECT_TRUE(percentage >= 9.9 && percentage <= 10.1)
      .message("  Distribution (%3.1f) is not within [9.9%%..10.01%%] interval", percentage);
  }
}

#if defined(BL_TARGET_OPT_SSE2)
TEST_CASE(core_random_simd) {
  BLRandom a(0);
  BLRandom b(0);

  for (uint32_t i = 0; i < kIterCount; i++) {
    uint64_t a_val = a.next_uint64();
    SIMD::Vec2xU64 b_vec = RandomDetail::next_uint64_as_v128(&b);
    uint64_t b_val = SIMD::cast_to_u64(b_vec);
    EXPECT_EQ(a_val, b_val).message("Iteration %u", i);
  }
}
#endif

} // {bl::RandomTests}

#endif // BL_TEST
