// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "matrix_p.h"
#include "runtime_p.h"
#include "simd/simd_p.h"
#include "support/math_p.h"

namespace bl {
namespace TransformInternal {

// bl::Transform - Private - Globals
// =================================

BLMapPointDArrayFunc mapPointDArrayFuncs[BL_TRANSFORM_TYPE_MAX_VALUE + 1];

const BLMatrix2D identityTransform { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 };

// bl::Transform - Private - MapPointDArray
// ========================================

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_FUNCTIONS)

static BLResult BL_CDECL blMatrix2DMapPointDArrayIdentity(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  blUnused(self);
  if (dst == src)
    return BL_SUCCESS;

  for (size_t i = 0; i < size; i++)
    dst[i] = src[i];

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArrayTranslate(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  double m20 = self->m20;
  double m21 = self->m21;

  for (size_t i = 0; i < size; i++)
    dst[i].reset(src[i].x + m20,
                 src[i].y + m21);

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArrayScale(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  double m00 = self->m00;
  double m11 = self->m11;
  double m20 = self->m20;
  double m21 = self->m21;

  for (size_t i = 0; i < size; i++)
    dst[i].reset(src[i].x * m00 + m20,
                 src[i].y * m11 + m21);

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArraySwap(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  double m10 = self->m10;
  double m01 = self->m01;
  double m20 = self->m20;
  double m21 = self->m21;

  for (size_t i = 0; i < size; i++)
    dst[i].reset(src[i].y * m10 + m20,
                 src[i].x * m01 + m21);

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArrayAffine(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  double m00 = self->m00;
  double m01 = self->m01;
  double m10 = self->m10;
  double m11 = self->m11;
  double m20 = self->m20;
  double m21 = self->m21;

  for (size_t i = 0; i < size; i++)
    dst[i].reset(src[i].x * m00 + src[i].y * m10 + m20,
                 src[i].x * m01 + src[i].y * m11 + m21);

  return BL_SUCCESS;
}

BL_DIAGNOSTIC_POP

} // {TransformInternal}
} // {bl}

// bl::Transform - API - Reset
// ===========================

BL_API_IMPL BLResult blMatrix2DSetIdentity(BLMatrix2D* self) noexcept {
  self->reset(1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blMatrix2DSetTranslation(BLMatrix2D* self, double x, double y) noexcept {
  self->reset(1.0, 0.0, 0.0, 1.0, x, y);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blMatrix2DSetScaling(BLMatrix2D* self, double x, double y) noexcept {
  self->reset(x, 0.0, 0.0, y, 0.0, 0.0);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blMatrix2DSetSkewing(BLMatrix2D* self, double x, double y) noexcept {
  double xTan = bl::Math::tan(x);
  double yTan = bl::Math::tan(y);

  self->reset(1.0, yTan, xTan, 1.0, 0.0, 0.0);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blMatrix2DSetRotation(BLMatrix2D* self, double angle, double x, double y) noexcept {
  double as = bl::Math::sin(angle);
  double ac = bl::Math::cos(angle);

  self->reset(ac, as, -as, ac, x, y);
  return BL_SUCCESS;
}

// bl::Transform - API - Accessors
// ===============================

BL_API_IMPL BLTransformType blMatrix2DGetType(const BLMatrix2D* self) noexcept {
  double m00 = self->m00;
  double m01 = self->m01;
  double m10 = self->m10;
  double m11 = self->m11;
  double m20 = self->m20;
  double m21 = self->m21;

  const uint32_t kBit00 = 1u << 3;
  const uint32_t kBit01 = 1u << 2;
  const uint32_t kBit10 = 1u << 1;
  const uint32_t kBit11 = 1u << 0;

#if defined(BL_TARGET_OPT_SSE2)
  // NOTE: Ideally this should be somewhere else, but for simplicity and easier dispatch it was placed here.
  // We do this as C++ compilers still cannot figure out how to do this and this is a much better solution
  // compared to scalar versions compilers produce.
  using namespace SIMD;
  uint32_t valueMsk = uint32_t(extract_sign_bits_i64(cmp_ne_f64(make128_f64(m00, m01), make_zero<Vec2xF64>())) << 2) |
                      uint32_t(extract_sign_bits_i64(cmp_ne_f64(make128_f64(m10, m11), make_zero<Vec2xF64>())) << 0) ;
#else
  uint32_t valueMsk = (uint32_t(m00 != 0.0) << 3) | (uint32_t(m01 != 0.0) << 2) |
                      (uint32_t(m10 != 0.0) << 1) | (uint32_t(m11 != 0.0) << 0) ;
#endif

  // Bit-table that contains ones for `valueMsk` combinations that are considered valid.
  uint32_t validTab = (0u << (0      | 0      | 0      | 0     )) | // [m00==0 m01==0 m10==0 m11==0]
                      (0u << (0      | 0      | 0      | kBit11)) | // [m00==0 m01==0 m10==0 m11!=0]
                      (0u << (0      | 0      | kBit10 | 0     )) | // [m00==0 m01==0 m10!=0 m11==0]
                      (1u << (0      | 0      | kBit10 | kBit11)) | // [m00==0 m01==0 m10!=0 m11!=0]
                      (0u << (0      | kBit01 | 0      | 0     )) | // [m00==0 m01!=0 m10==0 m11==0]
                      (0u << (0      | kBit01 | 0      | kBit11)) | // [m00==0 m01!=0 m10==0 m11!=0]
                      (1u << (0      | kBit01 | kBit10 | 0     )) | // [m00==0 m01!=0 m10!=0 m11==0] [SWAP]
                      (1u << (0      | kBit01 | kBit10 | kBit11)) | // [m00==0 m01!=0 m10!=0 m11!=0]
                      (0u << (kBit00 | 0      | 0      | 0     )) | // [m00!=0 m01==0 m10==0 m11==0]
                      (1u << (kBit00 | 0      | 0      | kBit11)) | // [m00!=0 m01==0 m10==0 m11!=0] [SCALE]
                      (0u << (kBit00 | 0      | kBit10 | 0     )) | // [m00!=0 m01==0 m10!=0 m11==0]
                      (1u << (kBit00 | 0      | kBit10 | kBit11)) | // [m00!=0 m01==0 m10!=0 m11!=0] [AFFINE]
                      (1u << (kBit00 | kBit01 | 0      | 0     )) | // [m00!=0 m01!=0 m10==0 m11==0]
                      (1u << (kBit00 | kBit01 | 0      | kBit11)) | // [m00!=0 m01!=0 m10==0 m11!=0] [AFFINE]
                      (1u << (kBit00 | kBit01 | kBit10 | 0     )) | // [m00!=0 m01!=0 m10!=0 m11==0] [AFFINE]
                      (1u << (kBit00 | kBit01 | kBit10 | kBit11)) ; // [m00!=0 m01!=0 m10!=0 m11!=0] [AFFINE]

  double d = m00 * m11 - m01 * m10;
  if (!((1u << valueMsk) & validTab) ||
      !bl::Math::isFinite(d) ||
      !bl::Math::isFinite(m20) ||
      !bl::Math::isFinite(m21)) {
    return BL_TRANSFORM_TYPE_INVALID;
  }

  // Transformation matrix is not swap/affine if:
  //   [. 0]
  //   [0 .]
  //   [. .]
  if (valueMsk != (kBit00 | kBit11)) {
    return (valueMsk == (kBit01 | kBit10))
      ? BL_TRANSFORM_TYPE_SWAP
      : BL_TRANSFORM_TYPE_AFFINE;
  }

  // Transformation matrix is not scaling if:
  //   [1 .]
  //   [. 1]
  //   [. .]
  if (!((m00 == 1.0) & (m11 == 1.0))) {
    return BL_TRANSFORM_TYPE_SCALE;
  }

  // Transformation matrix is not translation if:
  //   [. .]
  //   [. .]
  //   [0 0]
  if (!((m20 == 0.0) & (m21 == 0.0))) {
    return BL_TRANSFORM_TYPE_TRANSLATE;
  }

  return BL_TRANSFORM_TYPE_IDENTITY;
}

// bl::Transform - API - Operations
// ================================

BL_API_IMPL BLResult blMatrix2DApplyOp(BLMatrix2D* self, BLTransformOp opType, const void* opData) noexcept {
  BLMatrix2D* a = self;
  const double* data = static_cast<const double*>(opData);

  switch (opType) {
    //      |1 0|
    // A' = |0 1|
    //      |0 0|
    case BL_TRANSFORM_OP_RESET:
      a->reset();
      return BL_SUCCESS;

    //
    // A' = B
    //
    case BL_TRANSFORM_OP_ASSIGN:
      a->reset(*static_cast<const BLMatrix2D*>(opData));
      return BL_SUCCESS;

    //      [1 0]
    // A' = [0 1] * A
    //      [X Y]
    case BL_TRANSFORM_OP_TRANSLATE: {
      double x = data[0];
      double y = data[1];

      a->m20 += x * a->m00 + y * a->m10;
      a->m21 += x * a->m01 + y * a->m11;

      return BL_SUCCESS;
    }

    //      [X 0]
    // A' = [0 Y] * A
    //      [0 0]
    case BL_TRANSFORM_OP_SCALE: {
      double x = data[0];
      double y = data[1];

      a->m00 *= x;
      a->m01 *= x;
      a->m10 *= y;
      a->m11 *= y;

      return BL_SUCCESS;
    }

    //      [  1    tan(y)]
    // A' = [tan(x)   1   ] * A
    //      [  0      0   ]
    case BL_TRANSFORM_OP_SKEW: {
      double x = data[0];
      double y = data[1];
      double xTan = bl::Math::tan(x);
      double yTan = bl::Math::tan(y);

      double t00 = yTan * a->m10;
      double t01 = yTan * a->m11;

      a->m10 += xTan * a->m00;
      a->m11 += xTan * a->m01;

      a->m00 += t00;
      a->m01 += t01;

      return BL_SUCCESS;
    }

    // Tx and Ty are zero unless rotating about a point:
    //
    //   Tx = Px - cos(a) * Px + sin(a) * Py
    //   Ty = Py - sin(a) * Px - cos(a) * Py
    //
    //      [ cos(a) sin(a)]
    // A' = [-sin(a) cos(a)] * A
    //      [   Tx     Ty  ]
    case BL_TRANSFORM_OP_ROTATE:
    case BL_TRANSFORM_OP_ROTATE_PT: {
      double angle = data[0];
      double as = bl::Math::sin(angle);
      double ac = bl::Math::cos(angle);

      double t00 = as * a->m10 + ac * a->m00;
      double t01 = as * a->m11 + ac * a->m01;
      double t10 = ac * a->m10 - as * a->m00;
      double t11 = ac * a->m11 - as * a->m01;

      if (opType == BL_TRANSFORM_OP_ROTATE_PT) {
        double px = data[1];
        double py = data[2];

        double tx = px - ac * px + as * py;
        double ty = py - as * px - ac * py;

        double t20 = tx * a->m00 + ty * a->m10 + a->m20;
        double t21 = tx * a->m01 + ty * a->m11 + a->m21;

        a->m20 = t20;
        a->m21 = t21;
      }

      a->m00 = t00;
      a->m01 = t01;

      a->m10 = t10;
      a->m11 = t11;

      return BL_SUCCESS;
    }

    // A' = B * A
    case BL_TRANSFORM_OP_TRANSFORM: {
      const BLMatrix2D* b = static_cast<const BLMatrix2D*>(opData);

      a->reset(b->m00 * a->m00 + b->m01 * a->m10,
               b->m00 * a->m01 + b->m01 * a->m11,
               b->m10 * a->m00 + b->m11 * a->m10,
               b->m10 * a->m01 + b->m11 * a->m11,
               b->m20 * a->m00 + b->m21 * a->m10 + a->m20,
               b->m20 * a->m01 + b->m21 * a->m11 + a->m21);

      return BL_SUCCESS;
    }

    //          [1 0]
    // A' = A * [0 1]
    //          [X Y]
    case BL_TRANSFORM_OP_POST_TRANSLATE: {
      double x = data[0];
      double y = data[1];

      a->m20 += x;
      a->m21 += y;

      return BL_SUCCESS;
    }

    //          [X 0]
    // A' = A * [0 Y]
    //          [0 0]
    case BL_TRANSFORM_OP_POST_SCALE: {
      double x = data[0];
      double y = data[1];

      a->m00 *= x;
      a->m01 *= y;
      a->m10 *= x;
      a->m11 *= y;
      a->m20 *= x;
      a->m21 *= y;

      return BL_SUCCESS;
    }

    //          [  1    tan(y)]
    // A' = A * [tan(x)   1   ]
    //          [  0      0   ]
    case BL_TRANSFORM_OP_POST_SKEW:{
      double x = data[0];
      double y = data[1];
      double xTan = bl::Math::tan(x);
      double yTan = bl::Math::tan(y);

      double t00 = a->m01 * xTan;
      double t10 = a->m11 * xTan;
      double t20 = a->m21 * xTan;

      a->m01 += a->m00 * yTan;
      a->m11 += a->m10 * yTan;
      a->m21 += a->m20 * yTan;

      a->m00 += t00;
      a->m10 += t10;
      a->m20 += t20;

      return BL_SUCCESS;
    }

    //          [ cos(a) sin(a)]
    // A' = A * [-sin(a) cos(a)]
    //          [   x'     y'  ]
    case BL_TRANSFORM_OP_POST_ROTATE:
    case BL_TRANSFORM_OP_POST_ROTATE_PT: {
      double angle = data[0];
      double as = bl::Math::sin(angle);
      double ac = bl::Math::cos(angle);

      double t00 = a->m00 * ac - a->m01 * as;
      double t01 = a->m00 * as + a->m01 * ac;
      double t10 = a->m10 * ac - a->m11 * as;
      double t11 = a->m10 * as + a->m11 * ac;
      double t20 = a->m20 * ac - a->m21 * as;
      double t21 = a->m20 * as + a->m21 * ac;

      a->reset(t00, t01, t10, t11, t20, t21);
      if (opType != BL_TRANSFORM_OP_POST_ROTATE_PT)
        return BL_SUCCESS;

      double px = data[1];
      double py = data[2];

      a->m20 = t20 + px - ac * px + as * py;
      a->m21 = t21 + py - as * px - ac * py;

      return BL_SUCCESS;
    }

    // A' = A * B
    case BL_TRANSFORM_OP_POST_TRANSFORM: {
      const BLMatrix2D* b = static_cast<const BLMatrix2D*>(opData);

      a->reset(a->m00 * b->m00 + a->m01 * b->m10,
               a->m00 * b->m01 + a->m01 * b->m11,
               a->m10 * b->m00 + a->m11 * b->m10,
               a->m10 * b->m01 + a->m11 * b->m11,
               a->m20 * b->m00 + a->m21 * b->m10 + b->m20,
               a->m20 * b->m01 + a->m21 * b->m11 + b->m21);

      return BL_SUCCESS;
    }

    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }
}

BL_API_IMPL BLResult blMatrix2DInvert(BLMatrix2D* dst, const BLMatrix2D* src) noexcept {
  double d = src->m00 * src->m11 - src->m01 * src->m10;

  if (d == 0.0 || !bl::Math::isFinite(d))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  double t00 =  src->m11;
  double t01 = -src->m01;
  double t10 = -src->m10;
  double t11 =  src->m00;

  t00 /= d;
  t01 /= d;
  t10 /= d;
  t11 /= d;

  double t20 = -(src->m20 * t00 + src->m21 * t10);
  double t21 = -(src->m20 * t01 + src->m21 * t11);

  dst->reset(t00, t01, t10, t11, t20, t21);
  return BL_SUCCESS;
}

// bl::Transform - API - Map
// =========================

BL_API_IMPL BLResult blMatrix2DMapPointDArray(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t count) noexcept {
  BLTransformType transformType = BL_TRANSFORM_TYPE_AFFINE;

  if (count >= BL_MATRIX_TYPE_MINIMUM_SIZE)
    transformType = self->type();

  return bl::TransformInternal::mapPointDArrayFuncs[transformType](self, dst, src, count);
}

// bl::Transform - Runtime Registration
// ====================================

namespace bl {
namespace TransformInternal {

#ifdef BL_BUILD_OPT_SSE2
BL_HIDDEN void blTransformRtInit_SSE2(BLRuntimeContext* rt) noexcept;
#endif

#ifdef BL_BUILD_OPT_AVX
BL_HIDDEN void blTransformRtInit_AVX(BLRuntimeContext* rt) noexcept;
#endif

} // {TransformInternal}
} // {bl}

void blTransformRtInit(BLRuntimeContext* rt) noexcept {
  // Maybe unused.
  blUnused(rt);

#if !defined(BL_TARGET_OPT_SSE2)
  blUnused(rt);
  BLMapPointDArrayFunc* funcs = bl::TransformInternal::mapPointDArrayFuncs;

  blAssignFunc(&funcs[BL_TRANSFORM_TYPE_IDENTITY ], bl::TransformInternal::blMatrix2DMapPointDArrayIdentity);
  blAssignFunc(&funcs[BL_TRANSFORM_TYPE_TRANSLATE], bl::TransformInternal::blMatrix2DMapPointDArrayTranslate);
  blAssignFunc(&funcs[BL_TRANSFORM_TYPE_SCALE    ], bl::TransformInternal::blMatrix2DMapPointDArrayScale);
  blAssignFunc(&funcs[BL_TRANSFORM_TYPE_SWAP     ], bl::TransformInternal::blMatrix2DMapPointDArraySwap);
  blAssignFunc(&funcs[BL_TRANSFORM_TYPE_AFFINE   ], bl::TransformInternal::blMatrix2DMapPointDArrayAffine);
  blAssignFunc(&funcs[BL_TRANSFORM_TYPE_INVALID  ], bl::TransformInternal::blMatrix2DMapPointDArrayAffine);
#endif

#ifdef BL_BUILD_OPT_SSE2
  if (blRuntimeHasSSE2(rt))
    bl::TransformInternal::blTransformRtInit_SSE2(rt);
#endif

#ifdef BL_BUILD_OPT_AVX
  if (blRuntimeHasAVX(rt))
    bl::TransformInternal::blTransformRtInit_AVX(rt);
#endif
}
