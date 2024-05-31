// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_test_p.h"
#if defined(BL_TEST) && defined(BL_JIT_ARCH_X86) && !defined(BL_BUILD_NO_JIT)

#if defined(_MSC_VER)
  #include <intrin.h>
#else
  #include <emmintrin.h>
#endif

// bl::Pipeline::JIT - Tests - MAdd - Reference (X86)
// ==================================================

namespace bl {
namespace Pipeline {
namespace JIT {
namespace Tests {

// NOTE: We have to provide the reference implementation of these functions as it's impossible to test
// this on 32-bit X86 as the compiler would most likely use FPU registers, which uses the same precision
// for all floating point types. In that case the results from compiled functions would be different
// when compared to results that used FPU. So, use intrinsics would guarantees us the precision.

float madd_nofma_ref_f32(float a, float b, float c) noexcept {
  __m128 av = _mm_set1_ps(a);
  __m128 bv = _mm_set1_ps(b);
  __m128 cv = _mm_set1_ps(c);

  return _mm_cvtss_f32(_mm_add_ss(_mm_mul_ss(av, bv), cv));
}

double madd_nofma_ref_f64(double a, double b, double c) noexcept {
  __m128d av = _mm_set1_pd(a);
  __m128d bv = _mm_set1_pd(b);
  __m128d cv = _mm_set1_pd(c);

  return _mm_cvtsd_f64(_mm_add_sd(_mm_mul_sd(av, bv), cv));
}

} // {Tests}
} // {JIT}
} // {Pipeline}
} // {bl}

#endif // BL_TEST && !BL_BUILD_NO_JIT
