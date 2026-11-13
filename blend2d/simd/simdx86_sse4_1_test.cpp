// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST) && defined(BL_TARGET_OPT_SSE4_1)

#include <blend2d/simd/simd_test_p.h>

// SIMD - Tests - X86 - SSE4.1
// ===========================

namespace bl::SIMDTests {

BL_HIDDEN void simd_test_x86_sse4_1(ZeroTest::TestRunner& test_runner) noexcept {
  const char ext[] = "SSE4.1";

  if (test_runner.test_variation("sse4.1")) {
    test_integer<16>(ext);
  }
}

} // {bl::SIMDTests}

#endif // BL_TEST && BL_TARGET_OPT_SSE4_1
