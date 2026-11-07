// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/axl/math_p.h>
#include <blend2d/core/gradient_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/random.h>
#include <blend2d/core/rgba_p.h>

#include <initializer_list>

namespace bl::GradientTests {

// Gradient - Test Tables
// ======================

static const BLMatrix2D transforms[] = {
  BLMatrix2D::make_identity(),
  BLMatrix2D::make_translation(5.0, 6.0),
  BLMatrix2D::make_scaling(2.0, 3.0),
  BLMatrix2D(0.0, 2.0, 3.0, 0.0, 5.0, 6.0),
  BLMatrix2D(1.0, 2.0, 3.0, 4.0, 5.0, 6.0),
  BLMatrix2D(0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
};

static const BLGradientStop sorted_stops[] = {
  BLGradientStop(0.00, BLRgba32(0xFF000000u)),
  BLGradientStop(0.05, BLRgba32(0xFF00007Fu)),
  BLGradientStop(0.10, BLRgba32(0xFF0000FFu)),
  BLGradientStop(0.15, BLRgba32(0xFF007FFFu)),
  BLGradientStop(0.20, BLRgba32(0xFF00FF00u)),
  BLGradientStop(0.25, BLRgba32(0xFF00FF7Fu)),
  BLGradientStop(0.30, BLRgba32(0xFFFF0000u)),
  BLGradientStop(0.35, BLRgba32(0xFFFF7F00u)),
  BLGradientStop(0.40, BLRgba32(0xFFFFFF00u)),
  BLGradientStop(0.45, BLRgba32(0xFFFFFF6Fu)),
  BLGradientStop(0.50, BLRgba32(0xFFFFFFAFu)),
  BLGradientStop(0.55, BLRgba32(0xFFFFFFFFu)),
  BLGradientStop(0.60, BLRgba32(0xFF7FFFFFu)),
  BLGradientStop(0.65, BLRgba32(0xFF00FFFFu)),
  BLGradientStop(0.70, BLRgba32(0xFFFF7FFFu)),
  BLGradientStop(0.75, BLRgba32(0xFFFF00FFu)),
  BLGradientStop(0.80, BLRgba32(0xFFFFFFFFu)),
  BLGradientStop(0.85, BLRgba32(0xFFFFAFFFu)),
  BLGradientStop(0.90, BLRgba32(0xFFAFAFAFu)),
  BLGradientStop(0.95, BLRgba32(0xFF7F7F7Fu)),
  BLGradientStop(1.00, BLRgba32(0xFF3F3F3Fu))
};

static const BLLinearGradientValues default_values(0.0, 0.0, 0.0, 0.0);
static const BLLinearGradientValues sample_linear_values(1.0, 2.0, 100.0, 150.0);
static const BLRadialGradientValues sample_radial_values(100.0, 200.0, 123.0, 344.0, 50.0, 60.0);
static const BLConicGradientValues sample_conic_values(100.0, 200.0, 1.0);

static constexpr size_t kSortedStopsCount = BL_ARRAY_SIZE(sorted_stops);

// Gradient - Test Utilities
// =========================

static void expect_attributes(const BLGradient& g, BLLinearGradientValues values, BLExtendMode extend_mode) noexcept {
  EXPECT_EQ(g.type(), BL_GRADIENT_TYPE_LINEAR);
  EXPECT_EQ(g.extend_mode(), extend_mode);
  EXPECT_EQ(g.x0(), values.x0);
  EXPECT_EQ(g.y0(), values.y0);
  EXPECT_EQ(g.x1(), values.x1);
  EXPECT_EQ(g.y1(), values.y1);
}

static void expect_attributes(const BLGradient& g, BLRadialGradientValues values, BLExtendMode extend_mode) noexcept {
  EXPECT_EQ(g.type(), BL_GRADIENT_TYPE_RADIAL);
  EXPECT_EQ(g.extend_mode(), extend_mode);
  EXPECT_EQ(g.x0(), values.x0);
  EXPECT_EQ(g.y0(), values.y0);
  EXPECT_EQ(g.x1(), values.x1);
  EXPECT_EQ(g.y1(), values.y1);
  EXPECT_EQ(g.r0(), values.r0);
  EXPECT_EQ(g.r1(), values.r1);
}

static void expect_attributes(const BLGradient& g, BLConicGradientValues values, BLExtendMode extend_mode) noexcept {
  EXPECT_EQ(g.type(), BL_GRADIENT_TYPE_CONIC);
  EXPECT_EQ(g.extend_mode(), extend_mode);
  EXPECT_EQ(g.x0(), values.x0);
  EXPECT_EQ(g.y0(), values.y0);
  EXPECT_EQ(g.conic_angle(), values.angle);
  EXPECT_EQ(g.conic_repeat(), values.repeat);
}

static void expect_transform(const BLGradient& g, const BLMatrix2D& transform) noexcept {
  EXPECT_EQ(g.transform(), transform);
  EXPECT_EQ(g.transform_type(), transform.type());
}

static void expect_stops(const BLGradient& g, const BLGradientStop* stops, size_t stop_count) noexcept {
  EXPECT_EQ(g.size(), stop_count);

  for (size_t i = 0; i < stop_count; i++) {
    BLGradientStop stop = g.stop_at(i);
    BLGradientStop expected = stops[i];
    EXPECT_EQ(stop, expected)
      .message("Stops mismatch at [%zu] stop{%f, 0x%016X} != expected{%f, 0x%016X}",
        i,
        stop.offset, stop.rgba.value,
        expected.offset, expected.rgba.value);
  }
}

template<typename T>
static void expect_all(const BLGradient& g, T values, BLExtendMode extend_mode, const BLGradientStop* stops, size_t stop_count, const BLMatrix2D& tr) noexcept {
  expect_attributes(g, values, extend_mode);
  expect_transform(g, tr);
  expect_stops(g, stops, stop_count);
}

static void expect_default(const BLGradient& g) noexcept {
  expect_all(g, default_values, BL_EXTEND_MODE_PAD, nullptr, 0u, BLMatrix2D::make_identity());
}

// Gradient - Tests
// ================

TEST_CASE(core_gradient_constructors) {
  BLGradient empty;
  expect_default(empty);

  EXPECT_OK(empty.reset());
  expect_default(empty);

  for (BLMatrix2D tr : transforms) {
    for (uint32_t stop_count = 0; stop_count < kSortedStopsCount; stop_count++) {
      const BLGradientStop* stops = stop_count ? sorted_stops : nullptr;

      // g0 is the only one without transform - we want to verify it could be nullptr.
      BLGradient g0(sample_linear_values, BL_EXTEND_MODE_REFLECT, stops, stop_count);
      expect_all(g0, sample_linear_values, BL_EXTEND_MODE_REFLECT, stops, stop_count, BLMatrix2D::make_identity());

      BLGradient g1(sample_linear_values, BL_EXTEND_MODE_REFLECT, stops, stop_count, tr);
      expect_all(g1, sample_linear_values, BL_EXTEND_MODE_REFLECT, stops, stop_count, tr);

      EXPECT_OK(g1.set_x0(0.15));
      EXPECT_OK(g1.set_y0(0.85));
      EXPECT_OK(g1.set_x1(0.75));
      EXPECT_OK(g1.set_y1(0.25));
      expect_all(g1, BLLinearGradientValues(0.15, 0.85, 0.75, 0.25), BL_EXTEND_MODE_REFLECT, stops, stop_count, tr);

      BLGradient g2(sample_radial_values, BL_EXTEND_MODE_REPEAT, stops, stop_count, tr);
      expect_all(g2, sample_radial_values, BL_EXTEND_MODE_REPEAT, stops, stop_count, tr);

      EXPECT_OK(g2.set_x0(0.15));
      EXPECT_OK(g2.set_y0(0.85));
      EXPECT_OK(g2.set_x1(100.15));
      EXPECT_OK(g2.set_y1(100.85));
      EXPECT_OK(g2.set_r0(150.0));
      EXPECT_OK(g2.set_r1(50.0));
      expect_all(g2, BLRadialGradientValues(0.15, 0.85, 100.15, 100.85, 150.0, 50.0), BL_EXTEND_MODE_REPEAT, stops, stop_count, tr);

      BLGradient g3(sample_conic_values, BL_EXTEND_MODE_PAD, stops, stop_count, tr);
      expect_all(g3, sample_conic_values, BL_EXTEND_MODE_PAD, stops, stop_count, tr);

      EXPECT_OK(g3.set_x0(0.15));
      EXPECT_OK(g3.set_y0(0.85));
      EXPECT_OK(g3.set_conic_angle(2.0));
      EXPECT_OK(g3.set_conic_repeat(3.0));
      expect_all(g3, BLConicGradientValues(0.15, 0.85, 2.0, 3.0), BL_EXTEND_MODE_PAD, stops, stop_count, tr);

      BLGradient moved(Internal::move(g1));
      expect_default(g1);
      expect_all(moved, BLLinearGradientValues(0.15, 0.85, 0.75, 0.25), BL_EXTEND_MODE_REFLECT, stops, stop_count, tr);

      moved = Internal::move(g2);
      expect_default(g2);
      expect_all(moved, BLRadialGradientValues(0.15, 0.85, 100.15, 100.85, 150.0, 50.0), BL_EXTEND_MODE_REPEAT, stops, stop_count, tr);

      moved.assign(Internal::move(g3));
      expect_default(g3);
      expect_all(moved, BLConicGradientValues(0.15, 0.85, 2.0, 3.0), BL_EXTEND_MODE_PAD, stops, stop_count, tr);

      EXPECT_OK(moved.reset());
      expect_default(moved);
    }
  }
}

TEST_CASE(core_gradient_swap) {
  BLMatrix2D tr = BLMatrix2D::make_scaling(10.0, 20.0);

  BLGradient g0(sample_linear_values, BL_EXTEND_MODE_REFLECT, sorted_stops, kSortedStopsCount);
  BLGradient g1(sample_radial_values, BL_EXTEND_MODE_REPEAT, sorted_stops, kSortedStopsCount, tr);
  BLGradient g2;

  expect_all(g0, sample_linear_values, BL_EXTEND_MODE_REFLECT, sorted_stops, kSortedStopsCount, BLMatrix2D::make_identity());
  expect_all(g1, sample_radial_values, BL_EXTEND_MODE_REPEAT, sorted_stops, kSortedStopsCount, tr);

  g0.swap(g1);

  expect_all(g0, sample_radial_values, BL_EXTEND_MODE_REPEAT, sorted_stops, kSortedStopsCount, tr);
  expect_all(g1, sample_linear_values, BL_EXTEND_MODE_REFLECT, sorted_stops, kSortedStopsCount, BLMatrix2D::make_identity());

  g0.swap(g2);

  expect_all(g2, sample_radial_values, BL_EXTEND_MODE_REPEAT, sorted_stops, kSortedStopsCount, tr);
  expect_all(g0, default_values, BL_EXTEND_MODE_PAD, nullptr, 0u, BLMatrix2D::make_identity());
}

TEST_CASE(core_gradient_values) {
  for (BLMatrix2D tr : transforms) {
    for (uint32_t stop_count = 0; stop_count < kSortedStopsCount; stop_count++) {
      const BLGradientStop* stops = stop_count ? sorted_stops : nullptr;

      BLGradient g(sample_linear_values, BL_EXTEND_MODE_REFLECT, stops, stop_count, tr);
      expect_all(g, sample_linear_values, BL_EXTEND_MODE_REFLECT, stops, stop_count, tr);

      EXPECT_OK(g.set_x0(0.15));
      EXPECT_OK(g.set_y0(0.85));
      EXPECT_OK(g.set_x1(0.75));
      EXPECT_OK(g.set_y1(0.25));
      expect_all(g, BLLinearGradientValues(0.15, 0.85, 0.75, 0.25), BL_EXTEND_MODE_REFLECT, stops, stop_count, tr);

      EXPECT_OK(g.set_type(BL_GRADIENT_TYPE_RADIAL));
      EXPECT_OK(g.set_values(sample_radial_values));
      EXPECT_OK(g.set_extend_mode(BL_EXTEND_MODE_REPEAT));
      expect_all(g, sample_radial_values, BL_EXTEND_MODE_REPEAT, stops, stop_count, tr);

      BLGradient copy(g);

      EXPECT_OK(g.set_type(BL_GRADIENT_TYPE_CONIC));
      EXPECT_OK(g.set_values(sample_conic_values));
      EXPECT_OK(g.set_extend_mode(BL_EXTEND_MODE_PAD));
      expect_all(g, sample_conic_values, BL_EXTEND_MODE_PAD, stops, stop_count, tr);

      EXPECT_EQ(g.set_type(static_cast<BLGradientType>(BL_GRADIENT_TYPE_MAX_VALUE + 1u)), BL_ERROR_INVALID_VALUE);
      EXPECT_EQ(g.set_type(static_cast<BLGradientType>(0xFFFFFFFFu)), BL_ERROR_INVALID_VALUE);
      EXPECT_EQ(g.set_extend_mode(BL_EXTEND_MODE_PAD_X_REPEAT_Y), BL_ERROR_INVALID_VALUE);
      EXPECT_EQ(g.set_extend_mode(static_cast<BLExtendMode>(0xFFFFFFFFu)), BL_ERROR_INVALID_VALUE);
    }
  }
}

TEST_CASE(core_gradient_color_stops_assign_multiple) {
  BLMatrix2D identity = BLMatrix2D::make_identity();

  for (BLMatrix2D tr : transforms) {
    for (size_t stop_count = 0; stop_count < kSortedStopsCount; stop_count++) {
      BLGradientStop unsorted_stops[kSortedStopsCount];
      for (size_t i = 0; i < stop_count; i++) {
        unsorted_stops[i] = sorted_stops[stop_count - 1u - i];
      }

      BLGradient g1;
      EXPECT_OK(g1.reserve(10)); // To reserve something to execute more branches in `bl_gradient_create()`
      EXPECT_OK(g1.create(sample_linear_values, BL_EXTEND_MODE_REPEAT, sorted_stops, stop_count, tr));
      expect_all(g1, sample_linear_values, BL_EXTEND_MODE_REPEAT, sorted_stops, stop_count, tr);

      BLGradient g2;
      EXPECT_OK(g2.create(sample_linear_values, BL_EXTEND_MODE_REPEAT, unsorted_stops, stop_count, tr));
      EXPECT_EQ(g1, g2);
      expect_all(g2, sample_linear_values, BL_EXTEND_MODE_REPEAT, sorted_stops, stop_count, tr);

      BLGradient g3;
      EXPECT_OK(g3.reserve(10));
      EXPECT_OK(g3.assign_stops(sorted_stops, stop_count));
      expect_all(g3, default_values, BL_EXTEND_MODE_PAD, sorted_stops, stop_count, identity);

      BLGradient g4;
      EXPECT_OK(g4.assign_stops(unsorted_stops, stop_count));
      EXPECT_EQ(g3, g4);
      expect_all(g4, default_values, BL_EXTEND_MODE_PAD, sorted_stops, stop_count, identity);

      BLGradient g5;
      EXPECT_OK(g5.reserve(128));
      EXPECT_OK(g5.assign_stops(unsorted_stops, stop_count));
      EXPECT_EQ(g3, g5);
      expect_all(g5, default_values, BL_EXTEND_MODE_PAD, sorted_stops, stop_count, identity);

      size_t capacity_before_shrink = g5.capacity();
      EXPECT_OK(g5.shrink());
      EXPECT_LT(g5.capacity(), capacity_before_shrink);
      EXPECT_EQ(g3, g5);
      expect_all(g5, default_values, BL_EXTEND_MODE_PAD, sorted_stops, stop_count, identity);

      if (stop_count > 1u) {
        BLGradient copy(g1);

        EXPECT_OK(copy.remove_stop(1u));
        expect_attributes(copy, sample_linear_values, BL_EXTEND_MODE_REPEAT);
        expect_transform(copy, tr);

        EXPECT_EQ(copy.size(), stop_count - 1u);
        EXPECT_EQ(g1.size(), stop_count);
        EXPECT_NE(copy, g1);
        EXPECT_EQ(g1, g2);

        EXPECT_OK(g1.remove_stop(1u));
        EXPECT_EQ(g1.size(), stop_count - 1u);
        EXPECT_EQ(copy, g1);

        for (size_t i = 1; i < stop_count - 1; i++) {
          EXPECT_EQ(g1.stop_at(i), sorted_stops[i + 1]);
          EXPECT_EQ(copy.stop_at(i), sorted_stops[i + 1]);
        }
      }

      if (stop_count > 2) {
        BLGradient copy(g3);

        EXPECT_OK(copy.remove_stop_by_offset(sorted_stops[2].offset));
        expect_attributes(copy, default_values, BL_EXTEND_MODE_PAD);
        EXPECT_EQ(copy.size(), stop_count - 1u);
        EXPECT_EQ(g3.size(), stop_count);

        EXPECT_NE(copy, g3);
        EXPECT_EQ(g3, g4);

        EXPECT_OK(g3.remove_stop_by_offset(sorted_stops[2].offset));
        EXPECT_EQ(g3.size(), stop_count - 1u);
        EXPECT_EQ(copy, g3);

        for (size_t i = 2; i < stop_count - 1; i++) {
          EXPECT_EQ(g3.stop_at(i), sorted_stops[i + 1]);
          EXPECT_EQ(copy.stop_at(i), sorted_stops[i + 1]);
        }
      }
    }
  }
}

TEST_CASE(core_gradient_color_stops_add_remove) {
  for (BLMatrix2D tr : transforms) {
    for (uint32_t variation : {0u, 1u, 2u, 3u}) {
      BLGradient g(sample_linear_values, BL_EXTEND_MODE_REPEAT);
      BLGradient copy;

      auto handle_variation = [&]() {
        switch (variation) {
          case 0:
            break;
          case 1:
            copy = g;
            break;
          case 2:
            EXPECT_OK(g.shrink());
            break;
          case 3:
            EXPECT_OK(g.shrink());
            EXPECT_OK(g.reserve(10));
            break;
        }
      };

      // Insert stops in reverse order and expect the implementation to present them in a sorted order.
      EXPECT_OK(g.set_transform(tr));

      for (size_t i = 0; i < kSortedStopsCount; i++) {
        handle_variation();
        EXPECT_OK(g.add_stop(sorted_stops[kSortedStopsCount - 1u - i]));
      }

      expect_all(g, sample_linear_values, BL_EXTEND_MODE_REPEAT, sorted_stops, kSortedStopsCount, tr);

      // Remove and re-add each stop by index, and expect the full array after each pair of ops.
      for (size_t i = 0; i < kSortedStopsCount; i++) {
        handle_variation();
        EXPECT_OK(g.remove_stop(i));
        handle_variation();
        EXPECT_OK(g.add_stop(sorted_stops[i]));
        expect_all(g, sample_linear_values, BL_EXTEND_MODE_REPEAT, sorted_stops, kSortedStopsCount, tr);
      }

      // Remove and re-add each stop by offset, and expect the full array after each pair of ops.
      for (size_t i = 0; i < kSortedStopsCount; i++) {
        handle_variation();
        EXPECT_OK(g.remove_stop_by_offset(sorted_stops[i].offset));
        handle_variation();
        EXPECT_OK(g.add_stop(sorted_stops[i]));
        expect_all(g, sample_linear_values, BL_EXTEND_MODE_REPEAT, sorted_stops, kSortedStopsCount, tr);
      }

      EXPECT_EQ(g.remove_stop(g.size()), BL_ERROR_NO_ENTRY);
      EXPECT_EQ(g.remove_stop(1000000u), BL_ERROR_NO_ENTRY);
      EXPECT_EQ(g.remove_stop(SIZE_MAX), BL_ERROR_NO_ENTRY);
      EXPECT_EQ(g.remove_stops(BLRange{g.size(), SIZE_MAX}), BL_ERROR_NO_ENTRY);
    }
  }
}

TEST_CASE(core_gradient_color_stops_add_remove_sharp) {
  BLGradientStop expected_1[] = {
    BLGradientStop(0.0, BLRgba32(0x00000000u)),
  };

  BLGradientStop expected_2[] = {
    BLGradientStop(0.0, BLRgba32(0x00000000u)),
    BLGradientStop(1.0, BLRgba32(0xFF000000u)),
  };

  BLGradientStop expected_3[] = {
    BLGradientStop(0.0, BLRgba32(0x00000000u)),
    BLGradientStop(0.5, BLRgba32(0xFFFF0000u)),
    BLGradientStop(1.0, BLRgba32(0xFF000000u)),
  };

  BLGradientStop expected_4[] = {
    BLGradientStop(0.0, BLRgba32(0x00000000u)),
    BLGradientStop(0.5, BLRgba32(0xFFFF0000u)),
    BLGradientStop(0.5, BLRgba32(0xFFFFFF00u)),
    BLGradientStop(1.0, BLRgba32(0xFF000000u)),
  };

  for (BLMatrix2D tr : transforms) {
    for (uint32_t variation : {0u, 1u, 2u, 3u}) {
      BLGradient g;
      BLGradient copy;

      auto handle_variation = [&]() {
        switch (variation) {
          case 0:
            break;
          case 1:
            copy = g;
            break;
          case 2:
            EXPECT_OK(g.shrink());
            break;
          case 3:
            EXPECT_OK(g.shrink());
            EXPECT_OK(g.reserve(10));
            break;
        }
      };

      EXPECT_OK(g.set_transform(tr));
      EXPECT_OK(g.add_stop(0.0, BLRgba32(0x00000000u)));
      expect_all(g, default_values, BL_EXTEND_MODE_PAD, expected_1, BL_ARRAY_SIZE(expected_1), tr);

      handle_variation();
      EXPECT_OK(g.add_stop(1.0, BLRgba32(0xFF000000u)));
      expect_all(g, default_values, BL_EXTEND_MODE_PAD, expected_2, BL_ARRAY_SIZE(expected_2), tr);

      handle_variation();
      EXPECT_OK(g.add_stop(0.5, BLRgba32(0xFFFF0000u)));
      expect_all(g, default_values, BL_EXTEND_MODE_PAD, expected_3, BL_ARRAY_SIZE(expected_3), tr);

      handle_variation();
      EXPECT_OK(g.add_stop(0.5, BLRgba32(0xFFFFFF00u)));
      expect_all(g, default_values, BL_EXTEND_MODE_PAD, expected_4, BL_ARRAY_SIZE(expected_4), tr);

      handle_variation();
      EXPECT_OK(g.remove_stop_by_offset(0.5, BL_GRADIENT_STOP_REMOVE_POLICY_ALL));
      expect_all(g, default_values, BL_EXTEND_MODE_PAD, expected_2, BL_ARRAY_SIZE(expected_2), tr);

      handle_variation();
      EXPECT_OK(g.add_stop(0.5, BLRgba32(0xFFFF0000u)));
      expect_all(g, default_values, BL_EXTEND_MODE_PAD, expected_3, BL_ARRAY_SIZE(expected_3), tr);

      handle_variation();
      EXPECT_OK(g.add_stop(0.5, BLRgba32(0xFFFFFF00u)));
      expect_all(g, default_values, BL_EXTEND_MODE_PAD, expected_4, BL_ARRAY_SIZE(expected_4), tr);

      handle_variation();
      BLGradient g_tmp;

      EXPECT_OK(g_tmp.assign_stops(expected_4, 4u));
      EXPECT_OK(g_tmp.set_transform(tr));
      expect_all(g_tmp, default_values, BL_EXTEND_MODE_PAD, expected_4, BL_ARRAY_SIZE(expected_4), tr);
      EXPECT_EQ(g, g_tmp);

      EXPECT_OK(g.remove_stops(BLRange{1, 3}));
      expect_all(g, default_values, BL_EXTEND_MODE_PAD, expected_2, BL_ARRAY_SIZE(expected_2), tr);

      handle_variation();
      EXPECT_OK(g.reset_stops());
      expect_all(g, default_values, BL_EXTEND_MODE_PAD, nullptr, 0u, tr);
    }
  }
}

TEST_CASE(core_gradient_color_stops_add_remove_colliding) {
  const BLGradientStop test_stops[] = {
    BLGradientStop(0.00, BLRgba32(0xFF000000u)),
    BLGradientStop(0.00, BLRgba32(0xFF000010u)),
    BLGradientStop(0.00, BLRgba32(0xFF000020u)),
    BLGradientStop(0.10, BLRgba32(0xFF0000FFu)),
    BLGradientStop(0.10, BLRgba32(0xFF0010FFu)),
    BLGradientStop(0.10, BLRgba32(0xFF0020FFu)),
    BLGradientStop(0.20, BLRgba32(0xFF00FF00u)),
    BLGradientStop(0.20, BLRgba32(0xFF00FF10u)),
    BLGradientStop(0.20, BLRgba32(0xFF00FF20u)),
    BLGradientStop(0.30, BLRgba32(0xFFFF0000u)),
    BLGradientStop(0.30, BLRgba32(0xFFFF0010u)),
    BLGradientStop(0.30, BLRgba32(0xFFFF0020u)),
    BLGradientStop(0.40, BLRgba32(0xFFFFFF00u)),
    BLGradientStop(0.40, BLRgba32(0xFFFFFF10u)),
    BLGradientStop(0.40, BLRgba32(0xFFFFFF20u)),
    BLGradientStop(0.50, BLRgba32(0xFFFF00AFu)),
    BLGradientStop(0.50, BLRgba32(0xFFFF10AFu)),
    BLGradientStop(0.50, BLRgba32(0xFFFF20AFu)),
    BLGradientStop(0.60, BLRgba32(0xFF7FFF00u)),
    BLGradientStop(0.60, BLRgba32(0xFF7FFF10u)),
    BLGradientStop(0.60, BLRgba32(0xFF7FFF20u)),
    BLGradientStop(0.70, BLRgba32(0x00FF7FFFu)),
    BLGradientStop(0.70, BLRgba32(0x10FF7FFFu)),
    BLGradientStop(0.70, BLRgba32(0x20FF7FFFu)),
    BLGradientStop(0.80, BLRgba32(0xFFFF00FFu)),
    BLGradientStop(0.80, BLRgba32(0xFFFF10FFu)),
    BLGradientStop(0.80, BLRgba32(0xFFFF20FFu)),
    BLGradientStop(0.90, BLRgba32(0xFFAFAF00u)),
    BLGradientStop(0.90, BLRgba32(0xFFAFAF10u)),
    BLGradientStop(0.90, BLRgba32(0xFFAFAF20u)),
    BLGradientStop(1.00, BLRgba32(0xFF003F3Fu)),
    BLGradientStop(1.00, BLRgba32(0xFF103F3Fu)),
    BLGradientStop(1.00, BLRgba32(0xFF203F3Fu))
  };

  const BLGradientStop expected_stops[] = {
    BLGradientStop(0.00, BLRgba32(0xFF000000u)),
    BLGradientStop(0.00, BLRgba32(0xFF000020u)),
    BLGradientStop(0.10, BLRgba32(0xFF0000FFu)),
    BLGradientStop(0.10, BLRgba32(0xFF0020FFu)),
    BLGradientStop(0.20, BLRgba32(0xFF00FF00u)),
    BLGradientStop(0.20, BLRgba32(0xFF00FF20u)),
    BLGradientStop(0.30, BLRgba32(0xFFFF0000u)),
    BLGradientStop(0.30, BLRgba32(0xFFFF0020u)),
    BLGradientStop(0.40, BLRgba32(0xFFFFFF00u)),
    BLGradientStop(0.40, BLRgba32(0xFFFFFF20u)),
    BLGradientStop(0.50, BLRgba32(0xFFFF00AFu)),
    BLGradientStop(0.50, BLRgba32(0xFFFF20AFu)),
    BLGradientStop(0.60, BLRgba32(0xFF7FFF00u)),
    BLGradientStop(0.60, BLRgba32(0xFF7FFF20u)),
    BLGradientStop(0.70, BLRgba32(0x00FF7FFFu)),
    BLGradientStop(0.70, BLRgba32(0x20FF7FFFu)),
    BLGradientStop(0.80, BLRgba32(0xFFFF00FFu)),
    BLGradientStop(0.80, BLRgba32(0xFFFF20FFu)),
    BLGradientStop(0.90, BLRgba32(0xFFAFAF00u)),
    BLGradientStop(0.90, BLRgba32(0xFFAFAF20u)),
    BLGradientStop(1.00, BLRgba32(0xFF003F3Fu)),
    BLGradientStop(1.00, BLRgba32(0xFF203F3Fu))
  };

  for (BLMatrix2D tr : transforms) {
    for (uint32_t variation : {0u, 1u, 2u, 3u}) {
      BLGradient g1;
      BLGradient g2;
      BLGradient copy;

      auto handle_variation = [&]() {
        switch (variation) {
          case 0:
            break;
          case 1:
            copy = g1;
            break;
          case 2:
            EXPECT_OK(g1.shrink());
            break;
          case 3:
            EXPECT_OK(g1.shrink());
            EXPECT_OK(g1.reserve(10));
            break;
        }
      };

      EXPECT_OK(g1.set_type(BL_GRADIENT_TYPE_RADIAL));
      EXPECT_OK(g1.set_values(sample_radial_values));
      EXPECT_OK(g1.set_extend_mode(BL_EXTEND_MODE_REFLECT));
      EXPECT_OK(g1.set_transform(tr));
      EXPECT_OK(g2.create(sample_radial_values, BL_EXTEND_MODE_REFLECT, nullptr, 0u, tr));

      expect_all(g1, sample_radial_values, BL_EXTEND_MODE_REFLECT, nullptr, 0u, tr);
      expect_all(g2, sample_radial_values, BL_EXTEND_MODE_REFLECT, nullptr, 0u, tr);
      EXPECT_EQ(g1, g2);

      for (size_t i = 0; i < BL_ARRAY_SIZE(test_stops); i++) {
        handle_variation();
        EXPECT_OK(g1.add_stop(test_stops[i]));
      }

      EXPECT_OK(g2.assign_stops(test_stops, BL_ARRAY_SIZE(test_stops)));

      expect_all(g1, sample_radial_values, BL_EXTEND_MODE_REFLECT, expected_stops, BL_ARRAY_SIZE(expected_stops), tr);
      expect_all(g2, sample_radial_values, BL_EXTEND_MODE_REFLECT, expected_stops, BL_ARRAY_SIZE(expected_stops), tr);

      constexpr size_t kExpectedStopsHalf = BL_ARRAY_SIZE(expected_stops) / 2u;
      BLGradientStop unique_expected_stops_even[kExpectedStopsHalf];
      BLGradientStop unique_expected_stops_odd[kExpectedStopsHalf];

      for (size_t i = 0; i < kExpectedStopsHalf; i++) {
        unique_expected_stops_even[i] = expected_stops[i * 2u];
        unique_expected_stops_odd[i] = expected_stops[i * 2u + 1u];
      }

      BLGradient g3(g1);
      BLGradient g4(g2);

      for (size_t i = 0; i < kExpectedStopsHalf; i++) {
        double offset = unique_expected_stops_even[i].offset;
        EXPECT_OK(g1.remove_stop_by_offset(offset, BL_GRADIENT_STOP_REMOVE_POLICY_FIRST));
        EXPECT_OK(g2.remove_stop_by_offset(offset, BL_GRADIENT_STOP_REMOVE_POLICY_SECOND));
        EXPECT_OK(g3.remove_stop_by_offset(offset, BL_GRADIENT_STOP_REMOVE_POLICY_ALL));
        EXPECT_OK(g4.remove_stops_by_offset(offset, offset));
      }

      // g1 & g2 should have now unique_expected_[even|odd]_stops.
      expect_all(g1, sample_radial_values, BL_EXTEND_MODE_REFLECT, unique_expected_stops_odd, kExpectedStopsHalf, tr);
      expect_all(g2, sample_radial_values, BL_EXTEND_MODE_REFLECT, unique_expected_stops_even, kExpectedStopsHalf, tr);

      // g3 & g4 should have no stops at all as all were removed by `remove_stop[s]_by_offset()`.
      expect_all(g3, sample_radial_values, BL_EXTEND_MODE_REFLECT, nullptr, 0u, tr);
      expect_all(g4, sample_radial_values, BL_EXTEND_MODE_REFLECT, nullptr, 0u, tr);

      // Remove API also supports `BL_GRADIENT_STOP_REMOVE_POLICY_SECOND`, which preserves the first stop, if there
      // is no second stop.
      for (size_t i = 0; i < kExpectedStopsHalf; i++) {
        double offset = unique_expected_stops_even[i].offset;
        EXPECT_EQ(g1.remove_stop_by_offset(offset, BL_GRADIENT_STOP_REMOVE_POLICY_SECOND), BL_ERROR_NO_ENTRY);
        EXPECT_OK(g2.remove_stop_by_offset(offset, BL_GRADIENT_STOP_REMOVE_POLICY_FIRST));
      }

      expect_all(g1, sample_radial_values, BL_EXTEND_MODE_REFLECT, unique_expected_stops_odd, kExpectedStopsHalf, tr);
      expect_all(g2, sample_radial_values, BL_EXTEND_MODE_REFLECT, nullptr, 0u, tr);
    }
  }
}

TEST_CASE(core_gradient_color_stops_replace) {
  BLRandom rnd(0x1234u);

  for (BLMatrix2D tr : transforms) {
    for (size_t stop_count = 1; stop_count < kSortedStopsCount; stop_count++) {
      for (uint32_t shared : {0u, 1u}) {
        BLGradient g;
        BLGradient copy;

        EXPECT_OK(g.create(sample_linear_values, BL_EXTEND_MODE_REFLECT, sorted_stops, stop_count, tr));
        expect_all(g, sample_linear_values, BL_EXTEND_MODE_REFLECT, sorted_stops, stop_count, tr);

        BLGradientStop expected_stops[kSortedStopsCount];
        memcpy(expected_stops, sorted_stops, stop_count * sizeof(BLGradientStop));

        for (size_t i = 0; i < stop_count; i++) {
          BLRgba32 new_color(rnd.next_uint32());
          if (!shared) {
            EXPECT_OK(g.replace_stop(i, sorted_stops[i].offset, new_color));
          }
          else {
            copy = g;
            EXPECT_OK(g.replace_stop(i, sorted_stops[i].offset, new_color));
            expect_all(copy, sample_linear_values, BL_EXTEND_MODE_REFLECT, expected_stops, stop_count, tr);
          }

          expected_stops[i].rgba.reset(new_color);
          expect_all(g, sample_linear_values, BL_EXTEND_MODE_REFLECT, expected_stops, stop_count, tr);
        }

        for (size_t i = 0; i < stop_count; i++) {
          BLRgba32 new_color(rnd.next_uint32());
          double new_offset = axl::min(sorted_stops[i].offset + 0.01, 1.0);

          if (!shared) {
            EXPECT_OK(g.replace_stop(i, new_offset, new_color));
          }
          else {
            copy = g;
            EXPECT_OK(g.replace_stop(i, new_offset, new_color));
            expect_all(copy, sample_linear_values, BL_EXTEND_MODE_REFLECT, expected_stops, stop_count, tr);
          }

          expected_stops[i].offset = new_offset;
          expected_stops[i].rgba.reset(new_color);
          expect_all(g, sample_linear_values, BL_EXTEND_MODE_REFLECT, expected_stops, stop_count, tr);
        }
      }
    }
  }
}

TEST_CASE(core_gradient_color_stops_non_existent) {
  BLGradient g;
  BLMatrix2D tr = BLMatrix2D::make_rotation(0.1);

  EXPECT_OK(g.create(sample_radial_values, BL_EXTEND_MODE_REFLECT, sorted_stops, kSortedStopsCount, tr));
  expect_all(g, sample_radial_values, BL_EXTEND_MODE_REFLECT, sorted_stops, kSortedStopsCount, tr);

  EXPECT_EQ(g.remove_stop(SIZE_MAX), BL_ERROR_NO_ENTRY);
  EXPECT_EQ(g.remove_stop_by_offset(0.0004), BL_ERROR_NO_ENTRY);

  EXPECT_EQ(g.remove_stops(BLRange{1000u, 1010u}), BL_ERROR_NO_ENTRY);
  EXPECT_EQ(g.remove_stops_by_offset(0.0004, 0.0005), BL_ERROR_NO_ENTRY);
}

TEST_CASE(core_gradient_color_stops_index_of) {
  for (size_t stop_count = 1; stop_count < kSortedStopsCount; stop_count++) {
    BLGradient g;
    BLGradient copy;

    EXPECT_OK(g.create(sample_linear_values, BL_EXTEND_MODE_REFLECT, sorted_stops, stop_count));
    expect_all(g, sample_linear_values, BL_EXTEND_MODE_REFLECT, sorted_stops, stop_count, BLMatrix2D::make_identity());

    for (size_t i = 0; i < stop_count; i++) {
      size_t index = g.index_of_stop(sorted_stops[i].offset);
      EXPECT_EQ(index, i)
        .message("Stop with offset %f returned wrong index %zu (expected %zu)", sorted_stops[i].offset, index, i);
    }

    EXPECT_OK(g.add_stop(sorted_stops[0].offset, BLRgba32(0xFFAABBCCu)));
    EXPECT_EQ(g.index_of_stop(sorted_stops[0].offset), 0u);
    EXPECT_EQ(g.index_of_stop(sorted_stops[1].offset), stop_count > 1 ? size_t(2) : SIZE_MAX);
    EXPECT_EQ(g.stop_at(1u), BLGradientStop(sorted_stops[0].offset, BLRgba32(0xFFAABBCCu)));

    // Additional add replaces stop at [1] (the secondary stop sharing the same offset).
    EXPECT_OK(g.add_stop(sorted_stops[0].offset, BLRgba32(0xFFCCBBAAu)));
    EXPECT_EQ(g.index_of_stop(sorted_stops[0].offset), 0u);
    EXPECT_EQ(g.index_of_stop(sorted_stops[1].offset), stop_count > 1 ? size_t(2) : SIZE_MAX);
    EXPECT_EQ(g.stop_at(1u), BLGradientStop(sorted_stops[0].offset, BLRgba32(0xFFCCBBAAu)));

    EXPECT_OK(g.remove_stop(1u));
    EXPECT_EQ(g.index_of_stop(sorted_stops[0].offset), 0u);
    EXPECT_EQ(g.index_of_stop(sorted_stops[1].offset), stop_count > 1 ? size_t(1) : SIZE_MAX);

    if (stop_count > 1u) {
      EXPECT_EQ(g.stop_at(1u), sorted_stops[1]);
    }
  }
}

TEST_CASE(core_gradient_transform) {
  for (BLMatrix2D tr_initial : transforms) {
    for (BLMatrix2D tr_second : transforms) {
      for (uint32_t variation : {0u, 1u}) {
        BLGradient g;
        BLGradient copy;

        auto handle_variation = [&]() {
          switch (variation) {
            case 0:
              break;
            case 1:
              copy = g;
              break;
          }
        };

        EXPECT_OK(g.create(sample_linear_values, BL_EXTEND_MODE_REFLECT, sorted_stops, kSortedStopsCount));
        expect_all(g, sample_linear_values, BL_EXTEND_MODE_REFLECT, sorted_stops, kSortedStopsCount, BLMatrix2D::make_identity());

        BLMatrix2D tr_expected = tr_initial;
        tr_expected.transform(tr_second);

        handle_variation();

        EXPECT_OK(g.set_transform(tr_initial));
        expect_all(g, sample_linear_values, BL_EXTEND_MODE_REFLECT, sorted_stops, kSortedStopsCount, tr_initial);

        handle_variation();

        EXPECT_OK(g.apply_transform(tr_second));
        expect_all(g, sample_linear_values, BL_EXTEND_MODE_REFLECT, sorted_stops, kSortedStopsCount, tr_expected);

        EXPECT_OK(g.reset_transform());
        expect_all(g, sample_linear_values, BL_EXTEND_MODE_REFLECT, sorted_stops, kSortedStopsCount, BLMatrix2D::make_identity());
      }
    }
  }
}

TEST_CASE(core_gradient_transform_invalid) {
  BLGradient g;
  EXPECT_EQ(g._apply_transform_op(static_cast<BLTransformOp>(0xFFFFu), nullptr), BL_ERROR_INVALID_VALUE);
}

TEST_CASE(core_gradient_equals) {
  BLGradient g1;
  BLGradient g2;

  EXPECT_EQ(g1, g2);
  EXPECT_TRUE(g1.equals(g2));

  EXPECT_OK(g1.create(sample_linear_values, BL_EXTEND_MODE_REPEAT));
  EXPECT_OK(g2.create(sample_radial_values, BL_EXTEND_MODE_REPEAT));

  EXPECT_NE(g1, g2);
  EXPECT_FALSE(g1.equals(g2));

  EXPECT_OK(g2.create(sample_linear_values, BL_EXTEND_MODE_REFLECT));
  EXPECT_OK(g1.set_extend_mode(BL_EXTEND_MODE_REFLECT));

  EXPECT_EQ(g1, g2);
  EXPECT_TRUE(g1.equals(g2));

  EXPECT_OK(g1.add_stop(0.0, BLRgba32(0xFFFFFFFFu)));
  EXPECT_NE(g1, g2);

  EXPECT_OK(g2.add_stop(0.0, BLRgba32(0xFFFFFFFFu)));
  EXPECT_EQ(g1, g2);

  EXPECT_OK(g1.add_stop(1.0, BLRgba32(0xFF000000u)));
  EXPECT_NE(g1, g2);

  EXPECT_OK(g2.add_stop(0.5, BLRgba32(0xFFAAAAAAu)));
  EXPECT_NE(g1, g2);

  EXPECT_OK(g2.replace_stop(1, 1.0, BLRgba32(0xFF000000u)));
  EXPECT_EQ(g1, g2);

  EXPECT_OK(g1.set_transform(BLMatrix2D::make_translation(1.0, 2.0)));
  EXPECT_NE(g1, g2);

  EXPECT_OK(g2.set_transform(BLMatrix2D::make_scaling(1.0, 2.0)));
  EXPECT_NE(g1, g2);

  EXPECT_OK(g1.set_transform(BLMatrix2D::make_scaling(2.0, 3.0)));
  EXPECT_NE(g1, g2);

  EXPECT_OK(g1.set_transform(BLMatrix2D::make_scaling(1.0, 2.0)));
  EXPECT_EQ(g1, g2);

  BLGradient copy(g1);
  EXPECT_EQ(g1, copy);
  EXPECT_TRUE(g1.equals(copy));

  EXPECT_EQ(g2, g2);
  EXPECT_TRUE(g2.equals(g2));
}

TEST_CASE(core_gradient_c_api_vs_cxx_api) {
  // C-API getters are inlined in C++ API, so these have to be tested explicitly.
  BLGradient g;

  EXPECT_EQ(bl_gradient_get_size(&g), 0u);
  EXPECT_EQ(bl_gradient_get_capacity(&g), 0u);
  EXPECT_EQ(bl_gradient_get_stops(&g), g.stops().data());
  EXPECT_EQ(bl_gradient_get_type(&g), BL_GRADIENT_TYPE_LINEAR);
  EXPECT_EQ(bl_gradient_get_extend_mode(&g), BL_EXTEND_MODE_PAD);
  EXPECT_EQ(bl_gradient_get_transform_type(&g), BL_TRANSFORM_TYPE_IDENTITY);

  EXPECT_OK(bl_gradient_create(&g, BL_GRADIENT_TYPE_CONIC, nullptr, BL_EXTEND_MODE_REFLECT, nullptr, 0, nullptr));
  expect_all(g, BLConicGradientValues(0.0, 0.0, 0.0, 0.0), BL_EXTEND_MODE_REFLECT, nullptr, 0, BLMatrix2D::make_identity());

  EXPECT_OK(g.set_value(0, 1.0));
  EXPECT_OK(g.set_value(1, 2.0));
  EXPECT_OK(g.set_value(2, 3.0));
  EXPECT_OK(g.set_value(3, 4.0));
  EXPECT_EQ(g.set_value(static_cast<size_t>(BL_GRADIENT_VALUE_MAX_VALUE) + 1u, 0.0), BL_ERROR_INVALID_VALUE);

  EXPECT_EQ(bl_gradient_get_value(&g, 0), 1.0);
  EXPECT_EQ(bl_gradient_get_value(&g, 1), 2.0);
  EXPECT_EQ(bl_gradient_get_value(&g, 2), 3.0);
  EXPECT_EQ(bl_gradient_get_value(&g, 3), 4.0);
  EXPECT_TRUE(Math::is_nan(bl_gradient_get_value(&g, static_cast<size_t>(BL_GRADIENT_VALUE_MAX_VALUE) + 1u)));
  EXPECT_TRUE(Math::is_nan(bl_gradient_get_value(&g, SIZE_MAX)));

  EXPECT_OK(g.add_stop(0.1, BLRgba32(0xFFFFFFFF)));
  EXPECT_EQ(bl_gradient_get_size(&g), 1u);
  EXPECT_GE(bl_gradient_get_capacity(&g), 1u);
  EXPECT_EQ(bl_gradient_get_stops(&g), g.stops().data());

  EXPECT_OK(g.set_type(BL_GRADIENT_TYPE_RADIAL));
  EXPECT_EQ(bl_gradient_get_type(&g), BL_GRADIENT_TYPE_RADIAL);

  EXPECT_OK(g.set_extend_mode(BL_EXTEND_MODE_REFLECT));
  EXPECT_EQ(bl_gradient_get_extend_mode(&g), BL_EXTEND_MODE_REFLECT);

  EXPECT_OK(g.set_value(4, 11.0));
  EXPECT_EQ(bl_gradient_get_value(&g, 4), 11.0);

  BLMatrix2D tr(BLMatrix2D::make_scaling(10.0, 20.0));
  EXPECT_OK(g.set_transform(tr));
  EXPECT_EQ(bl_gradient_get_transform_type(&g), BL_TRANSFORM_TYPE_SCALE);

  BLMatrix2D tr_retrieved;
  EXPECT_OK(bl_gradient_get_transform(&g, &tr_retrieved));
  EXPECT_EQ(tr, tr_retrieved);
}

TEST_CASE(core_gradient_alloc_overflow) {
  constexpr size_t invalid_sizes[] = {
    SIZE_MAX,
    SIZE_MAX - 1,
    SIZE_MAX / sizeof(BLGradientStop),
    size_t(0xFFFFFFFFu)
  };

  BLLinearGradientValues values(0, 0, 1, 1);
  BLMatrix2D identity = BLMatrix2D::make_identity();

  for (size_t size : invalid_sizes) {
    BLGradient g1(values, BL_EXTEND_MODE_REPEAT, nullptr, size);
    expect_all(g1, default_values, BL_EXTEND_MODE_PAD, nullptr, 0u, identity);

    BLGradient g2;
    EXPECT_EQ(g2.reserve(size), BL_ERROR_OUT_OF_MEMORY);
    expect_all(g2, default_values, BL_EXTEND_MODE_PAD, nullptr, 0u, identity);

    EXPECT_EQ(g2.create(BLLinearGradientValues{0, 0, 1, 1}, BL_EXTEND_MODE_PAD, nullptr, size), BL_ERROR_OUT_OF_MEMORY);
    expect_all(g2, default_values, BL_EXTEND_MODE_PAD, nullptr, 0u, identity);

    EXPECT_EQ(g2.assign_stops(nullptr, size), BL_ERROR_OUT_OF_MEMORY);
    expect_all(g2, default_values, BL_EXTEND_MODE_PAD, nullptr, 0u, identity);
  }
}

} // {bl::GradientTests}

#endif // BL_TEST
