// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blmath_p.h"
#include "./blmatrix_p.h"
#include "./blruntime_p.h"
#include "./blsimd_p.h"

// ============================================================================
// [BLMatrix2D - Global Variables]
// ============================================================================

const BLMatrix2D blMatrix2DIdentity { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0 };

BLMapPointDArrayFunc blMatrix2DMapPointDArrayFuncs[BL_MATRIX2D_TYPE_COUNT];

// ============================================================================
// [BLMatrix2D - Reset]
// ============================================================================

BLResult blMatrix2DSetIdentity(BLMatrix2D* self) noexcept {
  self->reset(1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
  return BL_SUCCESS;
}

BLResult blMatrix2DSetTranslation(BLMatrix2D* self, double x, double y) noexcept {
  self->reset(1.0, 0.0, 0.0, 1.0, x, y);
  return BL_SUCCESS;
}

BLResult blMatrix2DSetScaling(BLMatrix2D* self, double x, double y) noexcept {
  self->reset(x, 0.0, 0.0, y, 0.0, 0.0);
  return BL_SUCCESS;
}

BLResult blMatrix2DSetSkewing(BLMatrix2D* self, double x, double y) noexcept {
  double xTan = blTan(x);
  double yTan = blTan(y);

  self->reset(1.0, yTan, xTan, 1.0, 0.0, 0.0);
  return BL_SUCCESS;
}

BLResult blMatrix2DSetRotation(BLMatrix2D* self, double angle, double x, double y) noexcept {
  double as = blSin(angle);
  double ac = blCos(angle);

  self->reset(ac, as, -as, ac, x, y);
  return BL_SUCCESS;
}

// ============================================================================
// [BLMatrix2D - Ops]
// ============================================================================

BLResult blMatrix2DApplyOp(BLMatrix2D* self, uint32_t opType, const void* opData) noexcept {
  BLMatrix2D* a = self;
  const double* data = static_cast<const double*>(opData);

  switch (opType) {
    //      |1 0|
    // A' = |0 1|
    //      |0 0|
    case BL_MATRIX2D_OP_RESET:
      a->reset();
      return BL_SUCCESS;

    //
    // A' = B
    //
    case BL_MATRIX2D_OP_ASSIGN:
      a->reset(*static_cast<const BLMatrix2D*>(opData));
      return BL_SUCCESS;

    //      [1 0]
    // A' = [0 1] * A
    //      [X Y]
    case BL_MATRIX2D_OP_TRANSLATE: {
      double x = data[0];
      double y = data[1];

      a->m20 += x * a->m00 + y * a->m10;
      a->m21 += x * a->m01 + y * a->m11;

      return BL_SUCCESS;
    }

    //      [X 0]
    // A' = [0 Y] * A
    //      [0 0]
    case BL_MATRIX2D_OP_SCALE: {
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
    case BL_MATRIX2D_OP_SKEW: {
      double x = data[0];
      double y = data[1];
      double xTan = blTan(x);
      double yTan = blTan(y);

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
    case BL_MATRIX2D_OP_ROTATE:
    case BL_MATRIX2D_OP_ROTATE_PT: {
      double angle = data[0];
      double as = blSin(angle);
      double ac = blCos(angle);

      double t00 = as * a->m10 + ac * a->m00;
      double t01 = as * a->m11 + ac * a->m01;
      double t10 = ac * a->m10 - as * a->m00;
      double t11 = ac * a->m11 - as * a->m01;

      if (opType == BL_MATRIX2D_OP_ROTATE_PT) {
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
    case BL_MATRIX2D_OP_TRANSFORM: {
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
    case BL_MATRIX2D_OP_POST_TRANSLATE: {
      double x = data[0];
      double y = data[1];

      a->m20 += x;
      a->m21 += y;

      return BL_SUCCESS;
    }

    //          [X 0]
    // A' = A * [0 Y]
    //          [0 0]
    case BL_MATRIX2D_OP_POST_SCALE: {
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
    case BL_MATRIX2D_OP_POST_SKEW:{
      double x = data[0];
      double y = data[1];
      double xTan = blTan(x);
      double yTan = blTan(y);

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
    case BL_MATRIX2D_OP_POST_ROTATE:
    case BL_MATRIX2D_OP_POST_ROTATE_PT: {
      double angle = data[0];
      double as = blSin(angle);
      double ac = blCos(angle);

      double t00 = a->m00 * ac - a->m01 * as;
      double t01 = a->m00 * as + a->m01 * ac;
      double t10 = a->m10 * ac - a->m11 * as;
      double t11 = a->m10 * as + a->m11 * ac;
      double t20 = a->m20 * ac - a->m21 * as;
      double t21 = a->m20 * as + a->m21 * ac;

      a->reset(t00, t01, t10, t11, t20, t21);
      if (opType != BL_MATRIX2D_OP_POST_ROTATE_PT)
        return BL_SUCCESS;

      double px = data[1];
      double py = data[2];

      a->m20 = t20 + px - ac * px + as * py;
      a->m21 = t21 + py - as * px - ac * py;

      return BL_SUCCESS;
    }

    // A' = A * B
    case BL_MATRIX2D_OP_POST_TRANSFORM: {
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

BLResult blMatrix2DInvert(BLMatrix2D* dst, const BLMatrix2D* src) noexcept {
  double d = src->m00 * src->m11 - src->m01 * src->m10;

  if (d == 0.0)
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

// ============================================================================
// [BLMatrix2D - Type]
// ============================================================================

uint32_t blMatrix2DGetType(const BLMatrix2D* self) noexcept {
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

  // NOTE: Ideally this should be somewhere else, but for simplicity and easier
  // displatch it was placed here. We do this as C++ compilers still cannot figure
  // out how to do this and this is a much better solution compared to scalar
  // versions compilers produce.
  using namespace SIMD;
  uint32_t valueMsk = (_mm_movemask_pd(vcmpnepd(vsetd128(m00, m01), vzerod128())) << 2) |
                      (_mm_movemask_pd(vcmpnepd(vsetd128(m10, m11), vzerod128())) << 0) ;
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
  if (!((1u << valueMsk) & validTab) || !blIsFinite(d) || !blIsFinite(m20) || !blIsFinite(m21))
    return BL_MATRIX2D_TYPE_INVALID;

  // Matrix is not swap/affine if:
  //   [. 0]
  //   [0 .]
  //   [. .]
  if (valueMsk != (kBit00 | kBit11))
    return (valueMsk == (kBit01 | kBit10))
      ? BL_MATRIX2D_TYPE_SWAP
      : BL_MATRIX2D_TYPE_AFFINE;

  // Matrix is not scaling if:
  //   [1 .]
  //   [. 1]
  //   [. .]
  if (!((m00 == 1.0) & (m11 == 1.0)))
    return BL_MATRIX2D_TYPE_SCALE;

  // Matrix is not translation if:
  //   [. .]
  //   [. .]
  //   [0 0]
  if (!((m20 == 0.0) & (m21 == 0.0)))
    return BL_MATRIX2D_TYPE_TRANSLATE;

  return BL_MATRIX2D_TYPE_IDENTITY;
}

// ============================================================================
// [BLMatrix2D - Map]
// ============================================================================

BLResult blMatrix2DMapPointDArray(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t count) noexcept {
  uint32_t matrixType = BL_MATRIX2D_TYPE_AFFINE;

  if (count >= BL_MATRIX_TYPE_MINIMUM_SIZE)
    matrixType = self->type();

  return blMatrix2DMapPointDArrayFuncs[matrixType](self, dst, src, count);
}

// ============================================================================
// [BLMatrix2D - MapPointDArray]
// ============================================================================

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_FUNCTIONS)

static BLResult BL_CDECL blMatrix2DMapPointDArrayIdentity(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  BL_UNUSED(self);
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

// ============================================================================
// [BLMatrix2D - Runtime Init]
// ============================================================================

#ifdef BL_BUILD_OPT_SSE2
BL_HIDDEN void blMatrix2DRtInit_SSE2(BLRuntimeContext* rt) noexcept;
#endif

#ifdef BL_BUILD_OPT_AVX
BL_HIDDEN void blMatrix2DRtInit_AVX(BLRuntimeContext* rt) noexcept;
#endif

void blMatrix2DRtInit(BLRuntimeContext* rt) noexcept {
  #if !defined(BL_TARGET_OPT_SSE2)
  BL_UNUSED(rt);
  BLMapPointDArrayFunc* funcs = blMatrix2DMapPointDArrayFuncs;

  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_IDENTITY ], blMatrix2DMapPointDArrayIdentity);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_TRANSLATE], blMatrix2DMapPointDArrayTranslate);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_SCALE    ], blMatrix2DMapPointDArrayScale);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_SWAP     ], blMatrix2DMapPointDArraySwap);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_AFFINE   ], blMatrix2DMapPointDArrayAffine);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_INVALID  ], blMatrix2DMapPointDArrayAffine);
  #endif

  #ifdef BL_BUILD_OPT_SSE2
  if (blRuntimeHasSSE2(rt)) blMatrix2DRtInit_SSE2(rt);
  #endif

  #ifdef BL_BUILD_OPT_AVX
  if (blRuntimeHasAVX(rt)) blMatrix2DRtInit_AVX(rt);
  #endif
}

// ============================================================================
// [BLMatrix2D - Unit Tests]
// ============================================================================

#ifdef BL_BUILD_TEST
UNIT(blend2d_matrix) {
  INFO("Testing matrix types");
  {
    BLMatrix2D m;

    m = BLMatrix2D::makeIdentity();
    EXPECT(m.type() == BL_MATRIX2D_TYPE_IDENTITY);

    m = BLMatrix2D::makeTranslation(1.0, 2.0);
    EXPECT(m.type() == BL_MATRIX2D_TYPE_TRANSLATE);

    m = BLMatrix2D::makeScaling(2.0, 2.0);
    EXPECT(m.type() == BL_MATRIX2D_TYPE_SCALE);

    m.m10 = 3.0;
    EXPECT(m.type() == BL_MATRIX2D_TYPE_AFFINE);

    m.reset(0.0, 1.0, 1.0, 0.0, 0.0, 0.0);
    EXPECT(m.type() == BL_MATRIX2D_TYPE_SWAP);

    m.reset(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    EXPECT(m.type() == BL_MATRIX2D_TYPE_INVALID);
  }

  INFO("Testing whether special-case transformations match matrix multiplication");
  {
    enum BL_TEST_MATRIX : uint32_t {
      BL_TEST_MATRIX_IDENTITY,
      BL_TEST_MATRIX_TRANSLATE,
      BL_TEST_MATRIX_SCALE,
      BL_TEST_MATRIX_SKEW,
      BL_TEST_MATRIX_ROTATE,
      BL_TEST_MATRIX_COUNT
    };

    static const BLPoint ptOffset(128.0, 64.0);
    static const BLPoint ptScale(1.5, 2.0);
    static const BLPoint ptSkew(1.5, 2.0);
    static const double angle = 0.9;

    auto testMatrixName = [](uint32_t type) noexcept -> const char* {
      switch (type) {
        case BL_TEST_MATRIX_IDENTITY : return "Identity";
        case BL_TEST_MATRIX_TRANSLATE: return "Translate";
        case BL_TEST_MATRIX_SCALE    : return "Scale";
        case BL_TEST_MATRIX_SKEW     : return "Skew";
        case BL_TEST_MATRIX_ROTATE   : return "Rotate";
        default: return "Unknown";
      }
    };

    auto createTestMatrix = [](uint32_t type) noexcept -> BLMatrix2D {
      switch (type) {
        case BL_TEST_MATRIX_TRANSLATE: return BLMatrix2D::makeTranslation(ptOffset);
        case BL_TEST_MATRIX_SCALE    : return BLMatrix2D::makeScaling(ptScale);
        case BL_TEST_MATRIX_SKEW     : return BLMatrix2D::makeSkewing(ptSkew);
        case BL_TEST_MATRIX_ROTATE   : return BLMatrix2D::makeRotation(angle);

        default:
          return BLMatrix2D::makeIdentity();
      }
    };

    auto compare = [](const BLMatrix2D& a, const BLMatrix2D& b) noexcept -> bool {
      double diff = blMax(blAbs(a.m00 - b.m00),
                          blAbs(a.m01 - b.m01),
                          blAbs(a.m10 - b.m10),
                          blAbs(a.m11 - b.m11),
                          blAbs(a.m20 - b.m20),
                          blAbs(a.m21 - b.m21));
      // If Blend2D is compiled with FMA enabled there could be a difference
      // greater than our blEpsilon<double>, so use a more relaxed value here.
      return diff < 1e-8;
    };

    BLMatrix2D m, n;
    BLMatrix2D a = BLMatrix2D::makeIdentity();
    BLMatrix2D b;

    for (uint32_t aType = 0; aType < BL_TEST_MATRIX_COUNT; aType++) {
      for (uint32_t bType = 0; bType < BL_TEST_MATRIX_COUNT; bType++) {
        a = createTestMatrix(aType);
        b = createTestMatrix(bType);

        m = a;
        n = a;

        for (uint32_t post = 0; post < 2; post++) {
          if (!post)
            m.transform(b);
          else
            m.postTransform(b);

          switch (bType) {
            case BL_TEST_MATRIX_IDENTITY:
              break;

            case BL_TEST_MATRIX_TRANSLATE:
              if (!post)
                n.translate(ptOffset);
              else
                n.postTranslate(ptOffset);
              break;

            case BL_TEST_MATRIX_SCALE:
              if (!post)
                n.scale(ptScale);
              else
                n.postScale(ptScale);
              break;

            case BL_TEST_MATRIX_SKEW:
              if (!post)
                n.skew(ptSkew);
              else
                n.postSkew(ptSkew);
              break;

            case BL_TEST_MATRIX_ROTATE:
              if (!post)
                n.rotate(angle);
              else
                n.postRotate(angle);
              break;
          }

          if (!compare(m, n)) {
            INFO("Matrices don't match [%s x %s]\n", testMatrixName(aType), testMatrixName(bType));
            INFO("    [% 3.14f | % 3.14f]      [% 3.14f | % 3.14f]\n", a.m00, a.m01, b.m00, b.m01);
            INFO("  A [% 3.14f | % 3.14f]    B [% 3.14f | % 3.14f]\n", a.m10, a.m11, b.m10, b.m11);
            INFO("    [% 3.14f | % 3.14f]      [% 3.14f | % 3.14f]\n", a.m20, a.m21, b.m20, b.m21);
            INFO("\n");
            INFO("Operation: %s\n", post ? "M = A * B" : "M = B * A");
            INFO("    [% 3.14f | % 3.14f]      [% 3.14f | % 3.14f]\n", m.m00, m.m01, n.m00, n.m01);
            INFO("  M [% 3.14f | % 3.14f] != N [% 3.14f | % 3.14f]\n", m.m10, m.m11, n.m10, n.m11);
            INFO("    [% 3.14f | % 3.14f]      [% 3.14f | % 3.14f]\n", m.m20, m.m21, n.m20, n.m21);
            EXPECT(false);
          }
        }
      }
    }
  }
}
#endif
