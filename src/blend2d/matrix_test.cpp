// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_test_p.h"
#if defined(BL_TEST)

#include "matrix_p.h"
#include "runtime_p.h"
#include "simd/simd_p.h"
#include "support/math_p.h"

// BLTransform - Tests
// ===================

namespace BLTransformTests {

UNIT(matrix, BL_TEST_GROUP_GEOMETRY_UTILITIES) {
  INFO("Testing matrix types");
  {
    BLMatrix2D m;

    m = BLMatrix2D::makeIdentity();
    EXPECT_EQ(m.type(), BL_TRANSFORM_TYPE_IDENTITY);

    m = BLMatrix2D::makeTranslation(1.0, 2.0);
    EXPECT_EQ(m.type(), BL_TRANSFORM_TYPE_TRANSLATE);

    m = BLMatrix2D::makeScaling(2.0, 2.0);
    EXPECT_EQ(m.type(), BL_TRANSFORM_TYPE_SCALE);

    m.m10 = 3.0;
    EXPECT_EQ(m.type(), BL_TRANSFORM_TYPE_AFFINE);

    m.reset(0.0, 1.0, 1.0, 0.0, 0.0, 0.0);
    EXPECT_EQ(m.type(), BL_TRANSFORM_TYPE_SWAP);

    m.reset(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    EXPECT_EQ(m.type(), BL_TRANSFORM_TYPE_INVALID);
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
      // greater than our Math::epsilon<double>, so use a more relaxed value here.
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
            EXPECT_TRUE(false);
          }
        }
      }
    }
  }
}

} // {BLTransformTests}

#endif // BL_TEST
