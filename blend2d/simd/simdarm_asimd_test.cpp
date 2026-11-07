// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST) && defined(BL_TARGET_OPT_ASIMD)

#include <blend2d/simd/simd_test_p.h>

// SIMD - Tests [ARM - ASIMD]
// ==========================

namespace bl::SIMDTests {

BL_HIDDEN void simd_test_arm_asimd(ZeroTest::TestRunner& test_runner) noexcept {
  const char ext[] = "NEON";

  if (test_runner.test_variation("neon")) {
    test_integer<16>(ext);
  }
}

} // {bl::SIMDTests}

#endif // BL_TEST && BL_TARGET_OPT_ASIMD
