// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/axl/commons.h>
#include <blend2d/axl/math_p.h>
#include <blend2d/core/pattern_p.h>

namespace bl::PatternTests {

// Pattern - Test Utilities
// ========================

static BLImage make_test_image(int w = 8, int h = 8, uint8_t value = 0xFFu) noexcept {
  BLImage image;
  EXPECT_OK(image.create(w, h, BL_FORMAT_PRGB32));

  BLImageData data;
  EXPECT_OK(image.make_mutable(&data));
  for (int y = 0; y < h; y++) {
    memset(static_cast<uint8_t*>(data.pixel_data) + intptr_t(y) * data.stride, value, size_t(w) * 4u);
  }
  return image;
}

static BLPattern make_test_pattern(const BLImage& image) {
  return BLPattern(image, BLRectI(1, 2, 3, 4), BL_EXTEND_MODE_REFLECT, BLMatrix2D::make_translation(5.0, 6.0));
}

static void expect_pattern(
  const BLPattern& pattern,
  const BLImage& image,
  const BLRectI& area,
  BLExtendMode extend_mode,
  const BLMatrix2D& transform
) noexcept {
  EXPECT_EQ(pattern.get_image(), image);
  EXPECT_EQ(pattern.area(), area);
  EXPECT_EQ(pattern.extend_mode(), extend_mode);
  EXPECT_EQ(pattern.transform(), transform);
  EXPECT_EQ(pattern.transform_type(), transform.type());
  EXPECT_EQ(pattern.has_transform(), transform.type() != BL_TRANSFORM_TYPE_IDENTITY);
}

static void expect_default(const BLPattern& pattern) noexcept {
  expect_pattern(pattern, BLImage(), BLRectI(0, 0, 0, 0), BL_EXTEND_MODE_REPEAT, BLMatrix2D::make_identity());
}

static void test_transform_op(
  const BLImage& image,
  const BLMatrix2D& initial,
  const BLMatrix2D& expected,
  auto op_fn
) noexcept {
  // Exercise both the mutable implementation and the copy-on-write path.
  for (uint32_t shared = 0; shared < 2; shared++) {
    BLPattern pattern(image, BLRectI(1, 2, 3, 4), BL_EXTEND_MODE_PAD_X_REFLECT_Y, initial);
    BLPattern copy;

    if (shared) {
      EXPECT_OK(copy.assign(pattern));
    }

    EXPECT_OK(op_fn(pattern));
    BLMatrix2D actual = pattern.transform();

    for (size_t i = 0; i < 6; i++) {
      EXPECT_NEAR_EPS(actual.m[i], expected.m[i], 1e-10);
    }

    EXPECT_EQ(pattern.transform_type(), expected.type());
    EXPECT_EQ(pattern.has_transform(), expected.type() != BL_TRANSFORM_TYPE_IDENTITY);
    EXPECT_EQ(pattern.get_image(), image);
    EXPECT_EQ(pattern.area(), BLRectI(1, 2, 3, 4));
    EXPECT_EQ(pattern.extend_mode(), BL_EXTEND_MODE_PAD_X_REFLECT_Y);

    if (shared) {
      expect_pattern(copy, image, BLRectI(1, 2, 3, 4), BL_EXTEND_MODE_PAD_X_REFLECT_Y, initial);
    }
  }
}

template<typename Op>
static void test_transform_op(const BLImage& image, const BLMatrix2D& initial, const Op& op) {
  BLMatrix2D expected = initial;
  EXPECT_OK(op(expected));
  test_transform_op(image, initial, expected, op);
}

// Pattern - Tests
// ===============

TEST_CASE(core_pattern_constructors) {
  BLImage image = make_test_image();
  BLRectI full_area(0, 0, 8, 8);
  BLRectI area(1, 2, 3, 4);
  BLMatrix2D identity = BLMatrix2D::make_identity();
  BLMatrix2D transform = BLMatrix2D::make_translation(5.0, 6.0);

  expect_default(BLPattern());
  expect_pattern(BLPattern(image), image, full_area, BL_EXTEND_MODE_REPEAT, identity);
  expect_pattern(BLPattern(image, area), image, area, BL_EXTEND_MODE_REPEAT, identity);

  for (uint32_t i = 0; i <= BL_EXTEND_MODE_COMPLEX_MAX_VALUE; i++) {
    BLExtendMode mode = BLExtendMode(i);
    expect_pattern(BLPattern(image, mode), image, full_area, mode, identity);
    expect_pattern(BLPattern(image, mode, transform), image, full_area, mode, transform);
    expect_pattern(BLPattern(image, area, mode), image, area, mode, identity);
    expect_pattern(BLPattern(image, area, mode, transform), image, area, mode, transform);
  }

  BLPattern original = make_test_pattern(image);
  BLPattern copy(original);
  EXPECT_EQ(copy, original);

  BLPattern moved(axl::move(copy));
  EXPECT_EQ(moved, original);
  expect_default(copy);
  EXPECT_OK(copy.create(image));

  EXPECT_OK(moved.reset());
  expect_default(moved);
  EXPECT_OK(moved.reset());
  EXPECT_EQ(original, make_test_pattern(image));

  BLImage empty;
  expect_default(BLPattern(empty));
  expect_default(BLPattern(empty, BLRectI(0, 0, 0, 0)));

  BLPattern default_pattern;
  BLPattern default_copy(default_pattern);
  BLPattern default_moved(axl::move(default_copy));

  expect_default(default_pattern);
  expect_default(default_copy);
  expect_default(default_moved);
}

TEST_CASE(core_pattern_assign) {
  BLImage image = make_test_image();
  BLPattern original = make_test_pattern(image);

  for (uint32_t state = 0; state < 3; state++) {
    BLPattern target = state ? BLPattern(image) : BLPattern();
    BLPattern copy;
    if (state == 2) {
      EXPECT_OK(copy.assign(target));
    }

    EXPECT_OK(target.assign(original));
    EXPECT_EQ(target, original);
    EXPECT_OK(target.set_area(BLRectI(2, 1, 4, 3)));
    EXPECT_EQ(original, make_test_pattern(image));
    EXPECT_EQ(&(target = original), &target);
    EXPECT_EQ(target, original);

    BLPattern source(original);
    EXPECT_OK(target.assign(axl::move(source)));
    EXPECT_EQ(target, original);
    expect_default(source);

    EXPECT_OK(source.assign(original));
    EXPECT_EQ(&(target = axl::move(source)), &target);
    EXPECT_EQ(target, original);
    expect_default(source);

    EXPECT_OK(target.assign(axl::move(target)));
    EXPECT_EQ(target, original);
    BLPattern& alias = target;
    EXPECT_EQ(&(target = axl::move(alias)), &target);
    EXPECT_EQ(target, original);

    if (state == 2) {
      expect_pattern(copy, image, BLRectI(0, 0, 8, 8), BL_EXTEND_MODE_REPEAT, BLMatrix2D::make_identity());
    }
  }

  BLPattern a(original);
  BLPattern b;
  a.swap(b);
  expect_default(a);
  EXPECT_EQ(b, original);
  a.swap(b);
  EXPECT_EQ(a, original);
  expect_default(b);
  a.swap(a);
  EXPECT_EQ(a, original);

  EXPECT_OK(a.assign(BLPattern()));
  expect_default(a);
  EXPECT_OK(a.assign(original));
  EXPECT_OK(a.assign(b));
  expect_default(a);
  EXPECT_EQ(original, make_test_pattern(image));
}

TEST_CASE(core_pattern_assign_self) {
  BLImage image = make_test_image();
  for (uint32_t state = 0; state < 3; state++) {
    BLPattern pattern  = state != 0 ? make_test_pattern(image) : BLPattern();
    BLPattern expected = state != 0 ? make_test_pattern(image) : BLPattern();

    BLPattern copy;
    if (state == 2) {
      EXPECT_OK(copy.assign(pattern));
    }

    EXPECT_OK(pattern.assign(pattern));
    EXPECT_EQ(pattern, expected);

    if (state == 2) {
      EXPECT_EQ(copy, expected);
    }
  }
}

TEST_CASE(core_pattern_create) {
  BLImage image = make_test_image();
  BLImage other = make_test_image(16, 32, 0x80u);
  BLRectI area(2, 3, 4, 5);
  BLMatrix2D transform = BLMatrix2D::make_scaling(2.0, 3.0);

  for (uint32_t state = 0; state < 3; state++) {
    for (uint32_t overload = 0; overload < 4; overload++) {
      BLPattern pattern = state ? make_test_pattern(image) : BLPattern();
      BLPattern copy;
      if (state == 2) {
        EXPECT_OK(copy.assign(pattern));
      }

      switch (overload) {
        case 0: EXPECT_OK(pattern.create(other)); break;
        case 1: EXPECT_OK(pattern.create(other, BL_EXTEND_MODE_PAD, transform)); break;
        case 2: EXPECT_OK(pattern.create(other, area)); break;
        case 3: EXPECT_OK(pattern.create(other, area, BL_EXTEND_MODE_REFLECT, transform)); break;
      }

      expect_pattern(pattern, other, overload < 2 ? BLRectI(0, 0, 16, 32) : area,
                     overload == 1 ? BL_EXTEND_MODE_PAD : overload == 3 ? BL_EXTEND_MODE_REFLECT : BL_EXTEND_MODE_REPEAT,
                     overload & 1u ? transform : BLMatrix2D::make_identity());
      if (state == 2) {
        EXPECT_EQ(copy, make_test_pattern(image));
      }

      EXPECT_OK(pattern.create(BLImage(), BLRectI(0, 0, 0, 0)));
      expect_default(pattern);
    }
  }
}

TEST_CASE(core_pattern_image_and_area) {
  BLImage image = make_test_image();
  BLImage other = make_test_image(16, 32, 0x80u);
  BLMatrix2D transform = BLMatrix2D::make_translation(5.0, 6.0);

  for (uint32_t shared = 0; shared < 2; shared++) {
    for (uint32_t op = 0; op < 6; op++) {
      BLPattern pattern = make_test_pattern(image);
      BLPattern copy;
      if (shared) {
        EXPECT_OK(copy.assign(pattern));
      }

      switch (op) {
        case 0:
          EXPECT_OK(pattern.set_image(other));
          expect_pattern(pattern, other, BLRectI(0, 0, 16, 32), BL_EXTEND_MODE_REFLECT, transform);
          break;
        case 1:
          EXPECT_OK(pattern.set_image(other, BLRectI(2, 3, 4, 5)));
          expect_pattern(pattern, other, BLRectI(2, 3, 4, 5), BL_EXTEND_MODE_REFLECT, transform);
          break;
        case 2:
          EXPECT_OK(pattern.set_area(BLRectI(2, 1, 4, 3)));
          expect_pattern(pattern, image, BLRectI(2, 1, 4, 3), BL_EXTEND_MODE_REFLECT, transform);
          break;
        case 3:
          EXPECT_OK(pattern.reset_area());
          expect_pattern(pattern, image, BLRectI(0, 0, 8, 8), BL_EXTEND_MODE_REFLECT, transform);
          EXPECT_OK(pattern.reset_area());
          break;
        case 4:
          EXPECT_OK(pattern.reset_image());
          expect_pattern(pattern, BLImage(), BLRectI(0, 0, 0, 0), BL_EXTEND_MODE_REFLECT, transform);
          EXPECT_OK(pattern.reset_image());
          break;
        case 5:
          EXPECT_OK(pattern.reset());
          expect_default(pattern);
          break;
      }

      if (shared) {
        EXPECT_EQ(copy, make_test_pattern(image));
      }
    }
  }

  BLPattern pattern;
  EXPECT_OK(pattern.set_image(image));
  expect_pattern(pattern, image, BLRectI(0, 0, 8, 8), BL_EXTEND_MODE_REPEAT, BLMatrix2D::make_identity());

  BLImage retained = pattern.get_image();
  EXPECT_OK(pattern.set_image(retained, BLRectI(1, 1, 2, 2)));
  EXPECT_OK(pattern.reset());
  EXPECT_OK(image.reset());
  EXPECT_EQ(retained.size(), BLSizeI(8, 8));

  EXPECT_OK(pattern.set_image(retained));
  EXPECT_OK(retained.create(16, 16, BL_FORMAT_PRGB32));
  EXPECT_EQ(pattern.get_image().size(), BLSizeI(8, 8));
  EXPECT_EQ(pattern.area(), BLRectI(0, 0, 8, 8));
}

TEST_CASE(core_pattern_area_validation) {
  BLImage image = make_test_image();
  const int values[] = { axl::min_value<int>(), -1, 0, 1, 7, 8, 9, axl::max_value<int>() };

  for (int x : values) {
    for (int y : values) {
      for (int w : values) {
        for (int h : values) {
          BLRectI area(x, y, w, h);
          // Use wider signed arithmetic as an independent bounds check.
          bool valid = x >= 0 && x < 8 && y >= 0 && y < 8 && w >= 0 && h >= 0 &&
                       int64_t(x) + int64_t(w) <= 8 && int64_t(y) + int64_t(h) <= 8;
          BLResult expected_result = valid ? BL_SUCCESS : BL_ERROR_INVALID_VALUE;

          BLPattern original = make_test_pattern(image);
          BLPattern pattern(original);
          EXPECT_EQ(pattern.set_area(area), expected_result).message("Area [%d, %d, %d, %d]", x, y, w, h);
          EXPECT_EQ(pattern.area(), valid ? area : original.area());
          EXPECT_EQ(pattern.create(image, area), expected_result);
          EXPECT_EQ(pattern.set_image(image, area), expected_result);
          EXPECT_EQ(original, make_test_pattern(image));

          BLPattern constructed(image, area);
          if (valid) {
            expect_pattern(constructed, image, area, BL_EXTEND_MODE_REPEAT, BLMatrix2D::make_identity());
          }
          else {
            expect_default(constructed);
          }
        }
      }
    }
  }
}

TEST_CASE(core_pattern_invalid_values) {
  BLImage image = make_test_image();
  BLRectI area(1, 2, 3, 4);
  BLRectI invalid_area(-1, 0, 1, 1);
  BLMatrix2D transform = BLMatrix2D::make_scaling(2.0, 3.0);
  const uint32_t invalid_modes[] = { BL_EXTEND_MODE_COMPLEX_MAX_VALUE + 1u, 0xFFFFFFFFu };

  for (uint32_t state = 0; state < 3; state++) {
    BLPattern pattern = state ? make_test_pattern(image) : BLPattern();
    BLPattern expected = state ? make_test_pattern(image) : BLPattern();
    BLPattern copy;
    if (state == 2) {
      EXPECT_OK(copy.assign(pattern));
    }

    for (uint32_t value : invalid_modes) {
      BLExtendMode mode = BLExtendMode(value);
      EXPECT_EQ(pattern.set_extend_mode(mode), BL_ERROR_INVALID_VALUE);
      EXPECT_EQ(pattern.create(image, mode), BL_ERROR_INVALID_VALUE);
      EXPECT_EQ(pattern.create(image, mode, transform), BL_ERROR_INVALID_VALUE);
      EXPECT_EQ(pattern.create(image, area, mode), BL_ERROR_INVALID_VALUE);
      EXPECT_EQ(pattern.create(image, area, mode, transform), BL_ERROR_INVALID_VALUE);
      EXPECT_EQ(pattern, expected);

      expect_default(BLPattern(image, mode));
      expect_default(BLPattern(image, mode, transform));
      expect_default(BLPattern(image, area, mode));
      expect_default(BLPattern(image, area, mode, transform));
    }

    EXPECT_EQ(pattern.create(image, invalid_area), BL_ERROR_INVALID_VALUE);
    EXPECT_EQ(pattern.create(image, invalid_area, BL_EXTEND_MODE_PAD, transform), BL_ERROR_INVALID_VALUE);
    EXPECT_EQ(pattern.set_image(image, invalid_area), BL_ERROR_INVALID_VALUE);
    EXPECT_EQ(pattern.set_area(invalid_area), BL_ERROR_INVALID_VALUE);
    EXPECT_EQ(pattern._apply_transform_op(BLTransformOp(BL_TRANSFORM_OP_MAX_VALUE + 1u), nullptr), BL_ERROR_INVALID_VALUE);
    EXPECT_EQ(pattern._apply_transform_op(BLTransformOp(0xFFFFFFFFu), nullptr), BL_ERROR_INVALID_VALUE);
    EXPECT_EQ(pattern, expected);
    if (state == 2) {
      EXPECT_EQ(copy, expected);
    }
  }
}

TEST_CASE(core_pattern_extend_mode) {
  BLImage image = make_test_image();
  for (uint32_t state = 0; state < 3; state++) {
    BLPattern pattern = state ? make_test_pattern(image) : BLPattern();
    BLPattern original = state ? make_test_pattern(image) : BLPattern();
    BLPattern copy;
    if (state == 2) {
      EXPECT_OK(copy.assign(pattern));
    }

    for (uint32_t i = 0; i <= BL_EXTEND_MODE_COMPLEX_MAX_VALUE; i++) {
      EXPECT_OK(pattern.set_extend_mode(BLExtendMode(i)));
      EXPECT_EQ(pattern.extend_mode(), BLExtendMode(i));
      EXPECT_EQ(pattern.get_image(), original.get_image());
      EXPECT_EQ(pattern.area(), original.area());
      EXPECT_EQ(pattern.transform(), original.transform());
      EXPECT_EQ(pattern.transform_type(), original.transform_type());
      if (state == 2) {
        EXPECT_EQ(copy, original);
      }
    }
    EXPECT_OK(pattern.reset_extend_mode());
    EXPECT_EQ(pattern.extend_mode(), BL_EXTEND_MODE_REPEAT);
  }

  // An extend mode stored on the default object must survive detachment.
  BLPattern pattern;
  EXPECT_OK(pattern.set_extend_mode(BL_EXTEND_MODE_PAD_X_REFLECT_Y));
  EXPECT_OK(pattern.translate(2.0, 3.0));
  EXPECT_EQ(pattern.extend_mode(), BL_EXTEND_MODE_PAD_X_REFLECT_Y);
  EXPECT_EQ(pattern.transform(), BLMatrix2D::make_translation(2.0, 3.0));
}

TEST_CASE(core_pattern_transform) {
  BLImage image = make_test_image();
  const BLMatrix2D initial_transforms[] = {
    BLMatrix2D::make_identity(),
    BLMatrix2D::make_translation(5.0, 6.0),
    BLMatrix2D::make_scaling(2.0, 3.0),
    BLMatrix2D(0.0, 2.0, 3.0, 0.0, 5.0, 6.0),
    BLMatrix2D(1.0, 2.0, 3.0, 4.0, 5.0, 6.0),
    BLMatrix2D(0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
  };
  BLMatrix2D operand(2.0, 3.0, 4.0, 5.0, 6.0, 7.0);

  for (const BLMatrix2D& initial : initial_transforms) {
    test_transform_op(image, initial, operand, [&](BLPattern& p) { return p.set_transform(operand); });
    test_transform_op(image, initial, BLMatrix2D::make_identity(), [](BLPattern& p) { return p.reset_transform(); });

    test_transform_op(image, initial, [](auto& p) { return p.translate(2.25, 3.5); });
    test_transform_op(image, initial, [](auto& p) { return p.translate(BLPointI(2, 3)); });
    test_transform_op(image, initial, [](auto& p) { return p.translate(BLPoint(2.25, 3.5)); });
    test_transform_op(image, initial, [](auto& p) { return p.scale(2.25); });
    test_transform_op(image, initial, [](auto& p) { return p.scale(2.25, 3.5); });
    test_transform_op(image, initial, [](auto& p) { return p.scale(BLPointI(2, 3)); });
    test_transform_op(image, initial, [](auto& p) { return p.scale(BLPoint(2.25, 3.5)); });
    test_transform_op(image, initial, [](auto& p) { return p.skew(0.2, 0.3); });
    test_transform_op(image, initial, [](auto& p) { return p.skew(BLPoint(0.2, 0.3)); });
    test_transform_op(image, initial, [](auto& p) { return p.rotate(0.5); });
    test_transform_op(image, initial, [](auto& p) { return p.rotate(0.5, 2.25, 3.5); });
    test_transform_op(image, initial, [](auto& p) { return p.rotate(0.5, BLPointI(2, 3)); });
    test_transform_op(image, initial, [](auto& p) { return p.rotate(0.5, BLPoint(2.25, 3.5)); });

    BLMatrix2D expected = initial;
    EXPECT_OK(expected.transform(operand));
    test_transform_op(image, initial, expected, [&](BLPattern& p) { return p.apply_transform(operand); });

    test_transform_op(image, initial, [](auto& p) { return p.post_translate(2.25, 3.5); });
    test_transform_op(image, initial, [](auto& p) { return p.post_translate(BLPointI(2, 3)); });
    test_transform_op(image, initial, [](auto& p) { return p.post_translate(BLPoint(2.25, 3.5)); });
    test_transform_op(image, initial, [](auto& p) { return p.post_scale(2.25); });
    test_transform_op(image, initial, [](auto& p) { return p.post_scale(2.25, 3.5); });
    test_transform_op(image, initial, [](auto& p) { return p.post_scale(BLPointI(2, 3)); });
    test_transform_op(image, initial, [](auto& p) { return p.post_scale(BLPoint(2.25, 3.5)); });
    test_transform_op(image, initial, [](auto& p) { return p.post_skew(0.2, 0.3); });
    test_transform_op(image, initial, [](auto& p) { return p.post_skew(BLPoint(0.2, 0.3)); });
    test_transform_op(image, initial, [](auto& p) { return p.post_rotate(0.5); });
    test_transform_op(image, initial, [](auto& p) { return p.post_rotate(0.5, 2.25, 3.5); });
    test_transform_op(image, initial, [](auto& p) { return p.post_rotate(0.5, BLPointI(2, 3)); });
    test_transform_op(image, initial, [](auto& p) { return p.post_rotate(0.5, BLPoint(2.25, 3.5)); });
    test_transform_op(image, initial, [&](auto& p) { return p.post_transform(operand); });
  }

  // Explicit values distinguish pre-translation from post-translation.
  BLPattern pattern;
  EXPECT_OK(pattern.reset_transform());
  expect_default(pattern);
  EXPECT_OK(pattern.scale(2.0, 3.0));
  EXPECT_OK(pattern.translate(4.0, 5.0));
  EXPECT_EQ(pattern.transform(), BLMatrix2D(2.0, 0.0, 0.0, 3.0, 8.0, 15.0));
  EXPECT_OK(pattern.post_translate(4.0, 5.0));
  EXPECT_EQ(pattern.transform(), BLMatrix2D(2.0, 0.0, 0.0, 3.0, 12.0, 20.0));
  EXPECT_OK(pattern.reset_transform());
  expect_default(pattern);
}

TEST_CASE(core_pattern_transform_non_finite) {
  BLImage image = make_test_image();
  const double values[] = { axl::quiet_nan<double>(), axl::infinity<double>() };

  for (double value : values) {
    for (size_t index = 0; index < 6; index++) {
      BLMatrix2D transform = BLMatrix2D::make_identity();
      transform.m[index] = value;
      BLPattern pattern(image, BL_EXTEND_MODE_REFLECT, transform);
      EXPECT_EQ(pattern.transform_type(), BL_TRANSFORM_TYPE_INVALID);
      EXPECT_TRUE(pattern.has_transform());

      BLPattern copy(pattern);
      EXPECT_OK(pattern.set_transform(transform));
      BLMatrix2D actual = pattern.transform();
      if (Math::is_nan(value)) {
        EXPECT_TRUE(Math::is_nan(actual.m[index]));
      }
      else {
        EXPECT_EQ(actual.m[index], value);
      }
      EXPECT_EQ(pattern.transform_type(), BL_TRANSFORM_TYPE_INVALID);
      EXPECT_EQ(pattern.extend_mode(), BL_EXTEND_MODE_REFLECT);
      EXPECT_OK(pattern.reset_transform());
      EXPECT_FALSE(pattern.has_transform());
      EXPECT_EQ(pattern.transform(), BLMatrix2D::make_identity());
      EXPECT_EQ(copy.transform_type(), BL_TRANSFORM_TYPE_INVALID);
    }
  }
}

TEST_CASE(core_pattern_equal) {
  BLImage image = make_test_image();
  BLPattern a = make_test_pattern(image);
  BLPattern b = make_test_pattern(image);
  BLPattern shared(a);

  EXPECT_TRUE(a.equals(a));
  EXPECT_TRUE(a.equals(b));
  EXPECT_TRUE(b.equals(a));
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
  EXPECT_EQ(a, shared);
  EXPECT_NE(a, BLPattern());

  EXPECT_OK(shared.set_extend_mode(BL_EXTEND_MODE_PAD));
  EXPECT_NE(a, shared);
  EXPECT_EQ(a, b);

  EXPECT_OK(b.set_area(BLRectI(2, 1, 4, 3)));
  EXPECT_FALSE(a.equals(b));
  EXPECT_FALSE(b.equals(a));
  EXPECT_OK(b.set_area(a.area()));
  EXPECT_EQ(a, b);

  EXPECT_OK(b.translate(1.0, 0.0));
  EXPECT_NE(a, b);
  EXPECT_OK(b.set_transform(a.transform()));
  EXPECT_EQ(a, b);
  EXPECT_OK(b.set_extend_mode(BL_EXTEND_MODE_PAD));
  EXPECT_NE(a, b);
  EXPECT_OK(b.set_extend_mode(a.extend_mode()));
  EXPECT_EQ(a, b);

  BLImage different = make_test_image(8, 8, 0x80u);
  EXPECT_OK(b.set_image(different, a.area()));
  EXPECT_NE(a, b);
  BLImage identical = make_test_image();
  EXPECT_OK(b.set_image(identical, a.area()));
  EXPECT_EQ(a, b);
  EXPECT_EQ(BLPattern(), BLPattern(BLImage()));
}

TEST_CASE(core_pattern_image_lifetime) {
  uint32_t pixels[64]{};
  uint32_t destroy_count = 0;
  auto destroy_func = [](BLObjectCore*, void*, void* user_data) noexcept {
    ++*static_cast<uint32_t*>(user_data);
  };

  {
    BLImage image;
    EXPECT_OK(image.create_from_data_mut(8, 8, BL_FORMAT_PRGB32, pixels, 32, destroy_func, &destroy_count));
    BLPattern pattern(image, BLRectI(1, 2, 3, 4));
    BLPattern copy(pattern);
    BLImage retained = pattern.get_image();
    EXPECT_OK(image.reset());
    EXPECT_EQ(destroy_count, 0u);

    EXPECT_OK(copy.set_area(BLRectI(2, 1, 4, 3)));
    EXPECT_OK(pattern.reset());
    EXPECT_EQ(destroy_count, 0u);
    EXPECT_OK(copy.reset_image());
    EXPECT_EQ(destroy_count, 0u);
    EXPECT_OK(retained.reset());
    EXPECT_EQ(destroy_count, 1u);
  }
  EXPECT_EQ(destroy_count, 1u);

  // Also exercise release through the pattern destructor.
  {
    BLImage image;
    EXPECT_OK(image.create_from_data_mut(8, 8, BL_FORMAT_PRGB32, pixels, 32, destroy_func, &destroy_count));
    BLPattern pattern(image);
    EXPECT_OK(image.reset());
    EXPECT_EQ(destroy_count, 1u);
  }
  EXPECT_EQ(destroy_count, 2u);
}

TEST_CASE(core_pattern_empty_area) {
  BLPattern pattern;
  EXPECT_OK(pattern.set_area(pattern.area()));
  expect_default(pattern);

  EXPECT_OK(pattern.set_image(BLImage()));
  EXPECT_OK(pattern.set_area(BLRectI(0, 0, 0, 0)));
  EXPECT_OK(pattern.reset_area());
  expect_default(pattern);
}

TEST_CASE(core_pattern_c_api_vs_cxx_api) {
  BLPattern pattern;
  expect_default(pattern);

  EXPECT_OK(bl_pattern_set_extend_mode(&pattern, BL_EXTEND_MODE_PAD_X_REFLECT_Y));
  EXPECT_EQ(bl_pattern_get_extend_mode(&pattern), BL_EXTEND_MODE_PAD_X_REFLECT_Y);

  BLMatrix2D tr1 = BLMatrix2D::make_rotation(0.2);
  EXPECT_OK(bl_pattern_set_transform(&pattern, &tr1));

  BLMatrix2D tr2;
  EXPECT_OK(bl_pattern_get_transform(&pattern, &tr2));

  EXPECT_EQ(tr1, tr2);
  EXPECT_EQ(bl_pattern_get_transform_type(&pattern), BL_TRANSFORM_TYPE_AFFINE);

  EXPECT_OK(bl_pattern_reset_transform(&pattern));
  EXPECT_EQ(bl_pattern_get_transform_type(&pattern), BL_TRANSFORM_TYPE_IDENTITY);

  EXPECT_EQ(bl_pattern_get_extend_mode(&pattern), BL_EXTEND_MODE_PAD_X_REFLECT_Y);
}

} // {bl::PatternTests}

#endif // BL_TEST
