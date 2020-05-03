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

#include "./api-build_p.h"
#ifdef BL_TARGET_OPT_SSE2

#include "./geometry.h"
#include "./matrix_p.h"
#include "./runtime_p.h"
#include "./simd_p.h"
#include "./support_p.h"

// ============================================================================
// [BLMatrix2D - MapPointDArray [SSE2]]
// ============================================================================

static BLResult BL_CDECL blMatrix2DMapPointDArrayIdentity_SSE2(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  BL_UNUSED(self);
  if (dst == src)
    return BL_SUCCESS;

  size_t i = size;
  if (blIsAligned(((uintptr_t)dst | (uintptr_t)src), 16)) {
    while (i >= 4) {
      Vec128D s0 = v_loada_d128(src + 0);
      Vec128D s1 = v_loada_d128(src + 1);
      Vec128D s2 = v_loada_d128(src + 2);
      Vec128D s3 = v_loada_d128(src + 3);

      v_storea_d128(dst + 0, s0);
      v_storea_d128(dst + 1, s1);
      v_storea_d128(dst + 2, s2);
      v_storea_d128(dst + 3, s3);

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      v_storea_d128(dst, v_loada_d128(src));
      i--;
      dst++;
      src++;
    }
  }
  else {
    while (i) {
      Vec128D sx = v_load_f64(&src->x);
      Vec128D sy = v_load_f64(&src->y);

      v_store_f64(&dst->x, sx);
      v_store_f64(&dst->y, sy);

      i--;
      dst++;
      src++;
    }
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArrayTranslate_SSE2(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  size_t i = size;
  Vec128D m20_m21 = v_loadu_d128(&self->m20);

  if (blIsAligned(((uintptr_t)dst | (uintptr_t)src), 16)) {
    while (i >= 4) {
      Vec128D s0 = v_loada_d128(src + 0);
      Vec128D s1 = v_loada_d128(src + 1);
      Vec128D s2 = v_loada_d128(src + 2);
      Vec128D s3 = v_loada_d128(src + 3);

      v_storea_d128(dst + 0, v_add_f64(s0, m20_m21));
      v_storea_d128(dst + 1, v_add_f64(s1, m20_m21));
      v_storea_d128(dst + 2, v_add_f64(s2, m20_m21));
      v_storea_d128(dst + 3, v_add_f64(s3, m20_m21));

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      Vec128D s0 = v_loada_d128(src);
      v_storea_d128(dst, v_add_f64(s0, m20_m21));

      i--;
      dst++;
      src++;
    }
  }
  else {
    while (i >= 4) {
      Vec128D s0 = v_loadu_d128(src + 0);
      Vec128D s1 = v_loadu_d128(src + 1);
      Vec128D s2 = v_loadu_d128(src + 2);
      Vec128D s3 = v_loadu_d128(src + 3);

      v_storeu_d128(dst + 0, v_add_f64(s0, m20_m21));
      v_storeu_d128(dst + 1, v_add_f64(s1, m20_m21));
      v_storeu_d128(dst + 2, v_add_f64(s2, m20_m21));
      v_storeu_d128(dst + 3, v_add_f64(s3, m20_m21));

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      Vec128D s0 = v_loadu_d128(src + 0);
      v_storeu_d128(dst + 0, v_add_f64(s0, m20_m21));

      i--;
      dst++;
      src++;
    }
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArrayScale_SSE2(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  size_t i = size;
  Vec128D m00_m11 = v_fill_d128(self->m11, self->m00);
  Vec128D m20_m21 = v_loadu_d128(&self->m20);

  if (blIsAligned(((uintptr_t)dst | (uintptr_t)src), 16)) {
    while (i >= 4) {
      Vec128D s0 = v_loada_d128(src + 0);
      Vec128D s1 = v_loada_d128(src + 1);
      Vec128D s2 = v_loada_d128(src + 2);
      Vec128D s3 = v_loada_d128(src + 3);

      v_storea_d128(dst + 0, v_add_f64(v_mul_f64(s0, m00_m11), m20_m21));
      v_storea_d128(dst + 1, v_add_f64(v_mul_f64(s1, m00_m11), m20_m21));
      v_storea_d128(dst + 2, v_add_f64(v_mul_f64(s2, m00_m11), m20_m21));
      v_storea_d128(dst + 3, v_add_f64(v_mul_f64(s3, m00_m11), m20_m21));

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      Vec128D s0 = v_loada_d128(src);
      v_storea_d128(dst, v_add_f64(v_mul_f64(s0, m00_m11), m20_m21));

      i--;
      dst++;
      src++;
    }
  }
  else {
    while (i >= 4) {
      Vec128D s0 = v_loadu_d128(src + 0);
      Vec128D s1 = v_loadu_d128(src + 1);
      Vec128D s2 = v_loadu_d128(src + 2);
      Vec128D s3 = v_loadu_d128(src + 3);

      v_storeu_d128(dst + 0, v_add_f64(v_mul_f64(s0, m00_m11), m20_m21));
      v_storeu_d128(dst + 1, v_add_f64(v_mul_f64(s1, m00_m11), m20_m21));
      v_storeu_d128(dst + 2, v_add_f64(v_mul_f64(s2, m00_m11), m20_m21));
      v_storeu_d128(dst + 3, v_add_f64(v_mul_f64(s3, m00_m11), m20_m21));

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      Vec128D s0 = v_loadu_d128(src);
      v_storeu_d128(dst, v_add_f64(v_mul_f64(s0, m00_m11), m20_m21));

      i--;
      dst++;
      src++;
    }
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArraySwap_SSE2(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  Vec128D m01_m10 = v_fill_d128(self->m01, self->m10);
  Vec128D m20_m21 = v_loadu_d128(&self->m20);

  size_t i = size;
  if (blIsAligned(((uintptr_t)dst | (uintptr_t)src), 16)) {
    while (i >= 4) {
      Vec128D s0 = v_loada_d128(src + 0);
      Vec128D s1 = v_loada_d128(src + 1);
      Vec128D s2 = v_loada_d128(src + 2);
      Vec128D s3 = v_loada_d128(src + 3);

      s0 = v_mul_f64(v_swap_f64(s0), m01_m10);
      s1 = v_mul_f64(v_swap_f64(s1), m01_m10);
      s2 = v_mul_f64(v_swap_f64(s2), m01_m10);
      s3 = v_mul_f64(v_swap_f64(s3), m01_m10);

      v_storea_d128(dst + 0, v_add_f64(s0, m20_m21));
      v_storea_d128(dst + 1, v_add_f64(s1, m20_m21));
      v_storea_d128(dst + 2, v_add_f64(s2, m20_m21));
      v_storea_d128(dst + 3, v_add_f64(s3, m20_m21));

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      Vec128D s0 = v_loada_d128(src);
      s0 = v_mul_f64(v_swap_f64(s0), m01_m10);
      v_storea_d128(dst, v_add_f64(s0, m20_m21));

      i--;
      dst++;
      src++;
    }
  }
  else {
    while (i >= 4) {
      Vec128D s0 = v_loadu_d128(src + 0);
      Vec128D s1 = v_loadu_d128(src + 1);
      Vec128D s2 = v_loadu_d128(src + 2);
      Vec128D s3 = v_loadu_d128(src + 3);

      s0 = v_mul_f64(v_swap_f64(s0), m01_m10);
      s1 = v_mul_f64(v_swap_f64(s1), m01_m10);
      s2 = v_mul_f64(v_swap_f64(s2), m01_m10);
      s3 = v_mul_f64(v_swap_f64(s3), m01_m10);

      v_storeu_d128(dst + 0, v_add_f64(s0, m20_m21));
      v_storeu_d128(dst + 1, v_add_f64(s1, m20_m21));
      v_storeu_d128(dst + 2, v_add_f64(s2, m20_m21));
      v_storeu_d128(dst + 3, v_add_f64(s3, m20_m21));

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      Vec128D s0 = v_loadu_d128(src);
      s0 = v_mul_f64(v_swap_f64(s0), m01_m10);
      v_storeu_d128(dst, v_add_f64(s0, m20_m21));

      i--;
      dst++;
      src++;
    }
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArrayAffine_SSE2(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  size_t i = size;
  Vec128D m00_m11 = v_fill_d128(self->m11, self->m00);
  Vec128D m10_m01 = v_fill_d128(self->m01, self->m10);
  Vec128D m20_m21 = v_loadu_d128(&self->m20);

  if (blIsAligned(((uintptr_t)dst | (uintptr_t)src), 16)) {
    while (i >= 4) {
      Vec128D s0 = v_loada_d128(src + 0);
      Vec128D s1 = v_loada_d128(src + 1);
      Vec128D s2 = v_loada_d128(src + 2);
      Vec128D s3 = v_loada_d128(src + 3);

      Vec128D r0 = v_swap_f64(s0);
      Vec128D r1 = v_swap_f64(s1);
      Vec128D r2 = v_swap_f64(s2);
      Vec128D r3 = v_swap_f64(s3);

      s0 = v_mul_f64(s0, m00_m11);
      s1 = v_mul_f64(s1, m00_m11);
      s2 = v_mul_f64(s2, m00_m11);
      s3 = v_mul_f64(s3, m00_m11);

      s0 = v_add_f64(v_add_f64(s0, m20_m21), v_mul_f64(r0, m10_m01));
      s1 = v_add_f64(v_add_f64(s1, m20_m21), v_mul_f64(r1, m10_m01));
      s2 = v_add_f64(v_add_f64(s2, m20_m21), v_mul_f64(r2, m10_m01));
      s3 = v_add_f64(v_add_f64(s3, m20_m21), v_mul_f64(r3, m10_m01));

      v_storea_d128(dst + 0, s0);
      v_storea_d128(dst + 1, s1);
      v_storea_d128(dst + 2, s2);
      v_storea_d128(dst + 3, s3);

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      Vec128D s0 = v_loada_d128(src);
      Vec128D r0 = v_swap_f64(s0);

      s0 = v_mul_f64(s0, m00_m11);
      s0 = v_add_f64(v_add_f64(s0, m20_m21), v_mul_f64(r0, m10_m01));

      v_storea_d128(dst, s0);

      i--;
      dst++;
      src++;
    }
  }
  else {
    while (i >= 4) {
      Vec128D s0 = v_loadu_d128(src + 0);
      Vec128D s1 = v_loadu_d128(src + 1);
      Vec128D s2 = v_loadu_d128(src + 2);
      Vec128D s3 = v_loadu_d128(src + 3);

      Vec128D r0 = v_swap_f64(s0);
      Vec128D r1 = v_swap_f64(s1);
      Vec128D r2 = v_swap_f64(s2);
      Vec128D r3 = v_swap_f64(s3);

      s0 = v_mul_f64(s0, m00_m11);
      s1 = v_mul_f64(s1, m00_m11);
      s2 = v_mul_f64(s2, m00_m11);
      s3 = v_mul_f64(s3, m00_m11);

      s0 = v_add_f64(v_add_f64(s0, m20_m21), v_mul_f64(r0, m10_m01));
      s1 = v_add_f64(v_add_f64(s1, m20_m21), v_mul_f64(r1, m10_m01));
      s2 = v_add_f64(v_add_f64(s2, m20_m21), v_mul_f64(r2, m10_m01));
      s3 = v_add_f64(v_add_f64(s3, m20_m21), v_mul_f64(r3, m10_m01));

      v_storeu_d128(dst + 0, s0);
      v_storeu_d128(dst + 1, s1);
      v_storeu_d128(dst + 2, s2);
      v_storeu_d128(dst + 3, s3);

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      Vec128D s0 = v_loadu_d128(src);
      Vec128D r0 = v_swap_f64(s0);

      s0 = v_mul_f64(s0, m00_m11);
      s0 = v_add_f64(v_add_f64(s0, m20_m21), v_mul_f64(r0, m10_m01));

      v_storeu_d128(dst, s0);

      i--;
      dst++;
      src++;
    }
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLMatrix2D - Runtime Init [SSE2]]
// ============================================================================

void blMatrix2DOnInit_SSE2(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);
  BLMapPointDArrayFunc* funcs = blMatrix2DMapPointDArrayFuncs;

  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_IDENTITY ], blMatrix2DMapPointDArrayIdentity_SSE2);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_TRANSLATE], blMatrix2DMapPointDArrayTranslate_SSE2);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_SCALE    ], blMatrix2DMapPointDArrayScale_SSE2);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_SWAP     ], blMatrix2DMapPointDArraySwap_SSE2);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_AFFINE   ], blMatrix2DMapPointDArrayAffine_SSE2);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_INVALID  ], blMatrix2DMapPointDArrayAffine_SSE2);
}

#endif
