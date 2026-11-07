// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST) && defined(BL_TARGET_OPT_AVX2)

#include <blend2d/simd/simd_test_p.h>

// SIMD - Tests - X86 - AVX2
// =========================

namespace bl::SIMDTests {

BL_HIDDEN void simd_test_x86_avx2(ZeroTest::TestRunner& test_runner) noexcept {
  const char ext[] = "AVX2";

  if (test_runner.test_variation("avx2{128}")) {
    test_integer<16>(ext);
  }

  if (test_runner.test_variation("avx2{256}")) {
    test_integer<32>(ext);
  }
}

} // {bl::SIMDTests}

#endif // BL_TEST && BL_TARGET_OPT_AVX2
