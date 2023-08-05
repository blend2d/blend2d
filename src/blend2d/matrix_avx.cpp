// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#ifdef BL_TARGET_OPT_AVX

#include "geometry.h"
#include "matrix_p.h"
#include "runtime_p.h"
#include "simd/simd_p.h"

namespace BLTransformPrivate {

// BLTransform - MapPointDArray (AVX)
// ==================================

static BLResult BL_CDECL mapPointDArrayIdentity_AVX(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  blUnused(self);
  if (dst == src)
    return BL_SUCCESS;

  size_t i = size;
  while (i >= 8) {
    storeu(dst + 0, loadu<Vec4xF64>(src + 0));
    storeu(dst + 2, loadu<Vec4xF64>(src + 2));
    storeu(dst + 4, loadu<Vec4xF64>(src + 4));
    storeu(dst + 6, loadu<Vec4xF64>(src + 6));

    i -= 8;
    dst += 8;
    src += 8;
  }

  while (i >= 2) {
    storeu(dst, loadu<Vec4xF64>(src));

    i -= 2;
    dst += 2;
    src += 2;
  }

  if (i)
    storeu(dst, loadu<Vec2xF64>(src));

  return BL_SUCCESS;
}

static BLResult BL_CDECL mapPointDArrayTranslate_AVX(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  size_t i = size;
  Vec4xF64 m20_m21 = load_broadcast_2xf64<Vec4xF64>(&self->m20);

  while (i >= 8) {
    storeu(dst + 0, loadu<Vec4xF64>(src + 0) + m20_m21);
    storeu(dst + 2, loadu<Vec4xF64>(src + 2) + m20_m21);
    storeu(dst + 4, loadu<Vec4xF64>(src + 4) + m20_m21);
    storeu(dst + 6, loadu<Vec4xF64>(src + 6) + m20_m21);

    i -= 8;
    dst += 8;
    src += 8;
  }

  while (i >= 2) {
    storeu(dst, loadu<Vec4xF64>(src) + m20_m21);

    i -= 2;
    dst += 2;
    src += 2;
  }

  if (i)
    storeu(dst, loadu<Vec2xF64>(src) + vec_cast<Vec2xF64>(m20_m21));

  return BL_SUCCESS;
}

static BLResult BL_CDECL mapPointDArrayScale_AVX(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  size_t i = size;
  Vec4xF64 m00_m11 = make256_f64(self->m11, self->m00);
  Vec4xF64 m20_m21 = load_broadcast_2xf64<Vec4xF64>(&self->m20);

  while (i >= 8) {
    storeu(dst + 0, loadu<Vec4xF64>(src + 0) * m00_m11 + m20_m21);
    storeu(dst + 2, loadu<Vec4xF64>(src + 2) * m00_m11 + m20_m21);
    storeu(dst + 4, loadu<Vec4xF64>(src + 4) * m00_m11 + m20_m21);
    storeu(dst + 6, loadu<Vec4xF64>(src + 6) * m00_m11 + m20_m21);

    i -= 8;
    dst += 8;
    src += 8;
  }

  while (i >= 2) {
    storeu(dst, loadu<Vec4xF64>(src) * m00_m11 + m20_m21);

    i -= 2;
    dst += 2;
    src += 2;
  }

  if (i)
    storeu(dst, loadu<Vec2xF64>(src) * vec_cast<Vec2xF64>(m00_m11) + vec_cast<Vec2xF64>(m20_m21));

  return BL_SUCCESS;
}

static BLResult BL_CDECL mapPointDArraySwap_AVX(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  size_t i = size;
  Vec4xF64 m01_m10 = make256_f64(self->m01, self->m10);
  Vec4xF64 m20_m21 = load_broadcast_2xf64<Vec4xF64>(&self->m20);

  while (i >= 8) {
    storeu(dst + 0, swap_f64(loadu<Vec4xF64>(src + 0)) * m01_m10 + m20_m21);
    storeu(dst + 2, swap_f64(loadu<Vec4xF64>(src + 2)) * m01_m10 + m20_m21);
    storeu(dst + 4, swap_f64(loadu<Vec4xF64>(src + 4)) * m01_m10 + m20_m21);
    storeu(dst + 6, swap_f64(loadu<Vec4xF64>(src + 6)) * m01_m10 + m20_m21);

    i -= 8;
    dst += 8;
    src += 8;
  }

  while (i >= 2) {
    storeu(dst, swap_f64(loadu<Vec4xF64>(src)) * m01_m10 + m20_m21);

    i -= 2;
    dst += 2;
    src += 2;
  }

  if (i)
    storeu(dst, swap_f64(loadu<Vec2xF64>(src)) * vec_cast<Vec2xF64>(m01_m10) + vec_cast<Vec2xF64>(m20_m21));

  return BL_SUCCESS;
}

static BLResult BL_CDECL mapPointDArrayAffine_AVX(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  size_t i = size;
  Vec4xF64 m00_m11 = make256_f64(self->m11, self->m00);
  Vec4xF64 m10_m01 = make256_f64(self->m01, self->m10);
  Vec4xF64 m20_m21 = load_broadcast_2xf64<Vec4xF64>(&self->m20);

  while (i >= 8) {
    Vec4xF64 v0 = loadu<Vec4xF64>(src + 0);
    Vec4xF64 v1 = loadu<Vec4xF64>(src + 2);
    Vec4xF64 v2 = loadu<Vec4xF64>(src + 4);
    Vec4xF64 v3 = loadu<Vec4xF64>(src + 6);

    storeu(dst + 0, v0 * m00_m11 + swap_f64(v0) * m10_m01 + m20_m21);
    storeu(dst + 2, v1 * m00_m11 + swap_f64(v1) * m10_m01 + m20_m21);
    storeu(dst + 4, v2 * m00_m11 + swap_f64(v2) * m10_m01 + m20_m21);
    storeu(dst + 6, v3 * m00_m11 + swap_f64(v3) * m10_m01 + m20_m21);

    i -= 8;
    dst += 8;
    src += 8;
  }

  while (i >= 2) {
    Vec4xF64 v0 = loadu<Vec4xF64>(src);
    storeu(dst, v0 * m00_m11 + swap_f64(v0) * m10_m01 + m20_m21);

    i -= 2;
    dst += 2;
    src += 2;
  }

  if (i) {
    Vec2xF64 v0 = loadu<Vec2xF64>(src);
    storeu(dst, v0 * vec_cast<Vec2xF64>(m00_m11) + swap_f64(v0) * vec_cast<Vec2xF64>(m10_m01) + vec_cast<Vec2xF64>(m20_m21));
  }

  return BL_SUCCESS;
}

// BLTransform - Runtime Registration (AVX)
// ========================================

void blTransformRtInit_AVX(BLRuntimeContext* rt) noexcept {
  blUnused(rt);
  BLMapPointDArrayFunc* funcs = BLTransformPrivate::mapPointDArrayFuncs;

  blAssignFunc(&funcs[BL_TRANSFORM_TYPE_IDENTITY ], mapPointDArrayIdentity_AVX);
  blAssignFunc(&funcs[BL_TRANSFORM_TYPE_TRANSLATE], mapPointDArrayTranslate_AVX);
  blAssignFunc(&funcs[BL_TRANSFORM_TYPE_SCALE    ], mapPointDArrayScale_AVX);
  blAssignFunc(&funcs[BL_TRANSFORM_TYPE_SWAP     ], mapPointDArraySwap_AVX);
  blAssignFunc(&funcs[BL_TRANSFORM_TYPE_AFFINE   ], mapPointDArrayAffine_AVX);
  blAssignFunc(&funcs[BL_TRANSFORM_TYPE_INVALID  ], mapPointDArrayAffine_AVX);
}

} // {BLTransformPrivate}

#endif
