// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/core/runtime_p.h>
#include <blend2d/simd/simd_p.h>

// SIMD - Tests
// ============

namespace bl::SIMDTests {

// SIMD - Tests - ARM
// ==================

#if BL_TARGET_ARCH_ARM

#if defined(BL_BUILD_OPT_ASIMD)
BL_HIDDEN void simd_test_arm_asimd(ZeroTest::TestRunner& test_runner) noexcept;
#endif // BL_BUILD_OPT_ASIMD

static void simd_test_arm(ZeroTest::TestRunner& test_runner, [[maybe_unused]] BLRuntimeContext& rt) noexcept {
#if defined(BL_BUILD_OPT_ASIMD)
  if (bl_runtime_has_asimd(&rt)) {
    simd_test_arm_asimd(test_runner);
  }
#endif // BL_BUILD_OPT_ASIMD
}
#endif // BL_TARGET_ARCH_ARM

// SIMD - Tests - X86
// ==================

#if BL_TARGET_ARCH_X86

#if defined(BL_BUILD_OPT_SSE2)
BL_HIDDEN void simd_test_x86_sse2(ZeroTest::TestRunner& test_runner) noexcept;
#endif // BL_BUILD_OPT_SSE2

#if defined(BL_BUILD_OPT_SSSE3)
BL_HIDDEN void simd_test_x86_ssse3(ZeroTest::TestRunner& test_runner) noexcept;
#endif // BL_BUILD_OPT_SSSE3

#if defined(BL_BUILD_OPT_SSE4_1)
BL_HIDDEN void simd_test_x86_sse4_1(ZeroTest::TestRunner& test_runner) noexcept;
#endif // BL_BUILD_OPT_SSE4_1

#if defined(BL_BUILD_OPT_SSE4_2)
BL_HIDDEN void simd_test_x86_sse4_2(ZeroTest::TestRunner& test_runner) noexcept;
#endif // BL_BUILD_OPT_SSE4_2

#if defined(BL_BUILD_OPT_AVX)
BL_HIDDEN void simd_test_x86_avx(ZeroTest::TestRunner& test_runner) noexcept;
#endif // BL_BUILD_OPT_AVX

#if defined(BL_BUILD_OPT_AVX2)
BL_HIDDEN void simd_test_x86_avx2(ZeroTest::TestRunner& test_runner) noexcept;
#endif // BL_BUILD_OPT_AVX2

#if defined(BL_BUILD_OPT_AVX512)
BL_HIDDEN void simd_test_x86_avx512(ZeroTest::TestRunner& test_runner) noexcept;
#endif // BL_BUILD_OPT_AVX512

static void simd_test_x86(ZeroTest::TestRunner& test_runner, [[maybe_unused]] BLRuntimeContext& rt) noexcept {
#if defined(BL_BUILD_OPT_SSE2)
  if (bl_runtime_has_sse2(&rt)) {
    simd_test_x86_sse2(test_runner);
  }
#endif // BL_BUILD_OPT_SSE2

#if defined(BL_BUILD_OPT_SSSE3)
  if (bl_runtime_has_ssse3(&rt)) {
    simd_test_x86_ssse3(test_runner);
  }
#endif // BL_BUILD_OPT_SSSE3

#if defined(BL_BUILD_OPT_SSE4_1)
  if (bl_runtime_has_sse4_1(&rt)) {
    simd_test_x86_sse4_1(test_runner);
  }
#endif // BL_BUILD_OPT_SSE4_1

#if defined(BL_BUILD_OPT_SSE4_2)
  if (bl_runtime_has_sse4_2(&rt)) {
    simd_test_x86_sse4_2(test_runner);
  }
#endif // BL_BUILD_OPT_SSE4_2

#if defined(BL_BUILD_OPT_AVX)
  if (bl_runtime_has_avx(&rt)) {
    simd_test_x86_avx(test_runner);
  }
#endif // BL_BUILD_OPT_AVX

#if defined(BL_BUILD_OPT_AVX2)
  if (bl_runtime_has_avx2(&rt)) {
    simd_test_x86_avx2(test_runner);
  }
#endif // BL_BUILD_OPT_AVX2

#if defined(BL_BUILD_OPT_AVX512)
  if (bl_runtime_has_avx512(&rt)) {
    simd_test_x86_avx512(test_runner);
  }
#endif // BL_BUILD_OPT_AVX512
}
#endif // BL_TARGET_ARCH_X86

// SIMD - Tests - Dispatcher
// =========================

TEST_CASE(simd) {
#if BL_TARGET_ARCH_ARM
  simd_test_arm(test_runner, bl_runtime_context);
#endif

#if BL_TARGET_ARCH_X86
  simd_test_x86(test_runner, bl_runtime_context);
#endif
}

} // {bl::SIMDTests}

#endif // BL_TEST
