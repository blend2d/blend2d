// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api-build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/core/object_p.h>
#include <blend2d/core/path_p.h>

// bl::Path - Tests
// ================

namespace bl {
namespace Tests {

static void expect_box_eq(const BLBox& actual, const BLBox& expected) {
  EXPECT_EQ(actual.x0, expected.x0);
  EXPECT_EQ(actual.y0, expected.y0);
  EXPECT_EQ(actual.x1, expected.x1);
  EXPECT_EQ(actual.y1, expected.y1);
}

static size_t count_command(const BLPath& path, uint8_t cmd) {
  size_t count = 0;

  for (size_t i = 0; i < path.size(); i++)
    count += size_t(path.command_data()[i] == cmd);

  return count;
}

UNIT(path_allocation_strategy, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath p;
  size_t kNumItems = 1000000;
  size_t capacity = p.capacity();

  for (size_t i = 0; i < kNumItems; i++) {
    if (i == 0)
      p.move_to(0, 0);
    else
      p.move_to(double(i), double(i));

    if (capacity != p.capacity()) {
      size_t impl_size = PathInternal::impl_size_from_capacity(p.capacity()).value();
      INFO("Capacity increased from %zu to %zu [ImplSize=%zu]\n", capacity, p.capacity(), impl_size);

      capacity = p.capacity();
    }
  }
}

UNIT(path_conic_storage, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath p;
  EXPECT_SUCCESS(p.move_to(0.0, 0.0));
  EXPECT_SUCCESS(p.conic_to(1.0, 2.0, 3.0, 4.0, 0.5));
  EXPECT_SUCCESS(p.line_to(5.0, 6.0));

  EXPECT_EQ(p.size(), size_t(4));
  EXPECT_EQ(p.conic_weight_size(), size_t(1));

  const uint8_t* cmd = p.command_data();
  const BLPoint* vtx = p.vertex_data();
  const double* w = p.conic_weight_data();

  EXPECT_EQ(cmd[0], uint8_t(BL_PATH_CMD_MOVE));
  EXPECT_EQ(cmd[1], uint8_t(BL_PATH_CMD_CONIC));
  EXPECT_EQ(cmd[2], uint8_t(BL_PATH_CMD_ON));
  EXPECT_EQ(cmd[3], uint8_t(BL_PATH_CMD_ON));

  EXPECT_EQ(vtx[0], BLPoint(0.0, 0.0));
  EXPECT_EQ(vtx[1], BLPoint(1.0, 2.0));
  EXPECT_EQ(vtx[2], BLPoint(3.0, 4.0));
  EXPECT_EQ(vtx[3], BLPoint(5.0, 6.0));
  EXPECT_EQ(w[0], 0.5);
}

UNIT(path_conic_copy_and_reverse, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath src;
  EXPECT_SUCCESS(src.move_to(0.0, 0.0));
  EXPECT_SUCCESS(src.conic_to(1.0, 2.0, 3.0, 4.0, 0.75));

  BLPath copy;
  EXPECT_SUCCESS(copy.add_path(src));
  EXPECT_TRUE(copy.equals(src));
  EXPECT_EQ(copy.conic_weight_size(), size_t(1));
  EXPECT_EQ(copy.conic_weight_data()[0], 0.75);

  BLPath reversed;
  EXPECT_SUCCESS(reversed.add_reversed_path(src, BL_PATH_REVERSE_MODE_COMPLETE));

  EXPECT_EQ(reversed.size(), size_t(3));
  EXPECT_EQ(reversed.conic_weight_size(), size_t(1));
  EXPECT_EQ(reversed.command_data()[0], uint8_t(BL_PATH_CMD_MOVE));
  EXPECT_EQ(reversed.command_data()[1], uint8_t(BL_PATH_CMD_CONIC));
  EXPECT_EQ(reversed.command_data()[2], uint8_t(BL_PATH_CMD_ON));
  EXPECT_EQ(reversed.vertex_data()[0], BLPoint(3.0, 4.0));
  EXPECT_EQ(reversed.vertex_data()[1], BLPoint(1.0, 2.0));
  EXPECT_EQ(reversed.vertex_data()[2], BLPoint(0.0, 0.0));
  EXPECT_EQ(reversed.conic_weight_data()[0], 0.75);
}

UNIT(path_bounds_of_empty_path, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath path;
  BLBox control_box;
  BLBox bounding_box;

  EXPECT_SUCCESS(path.get_control_box(&control_box));
  EXPECT_SUCCESS(path.get_bounding_box(&bounding_box));

  expect_box_eq(control_box, BLBox(0.0, 0.0, 0.0, 0.0));
  expect_box_eq(bounding_box, BLBox(0.0, 0.0, 0.0, 0.0));
}

UNIT(path_bounds_of_line_and_multiple_figures, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath path;
  BLBox control_box;
  BLBox bounding_box;

  EXPECT_SUCCESS(path.move_to(10.0, 10.0));
  EXPECT_SUCCESS(path.line_to(20.0, 10.0));
  EXPECT_SUCCESS(path.move_to(-5.0, -7.0));
  EXPECT_SUCCESS(path.line_to(-3.0, 1.0));

  EXPECT_SUCCESS(path.get_control_box(&control_box));
  EXPECT_SUCCESS(path.get_bounding_box(&bounding_box));

  expect_box_eq(control_box, BLBox(-5.0, -7.0, 20.0, 10.0));
  expect_box_eq(bounding_box, BLBox(-5.0, -7.0, 20.0, 10.0));
}

UNIT(path_bounds_of_quadratic_curve, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath path;
  BLBox control_box;
  BLBox bounding_box;

  EXPECT_SUCCESS(path.move_to(0.0, 0.0));
  EXPECT_SUCCESS(path.quad_to(2.0, 4.0, 4.0, 0.0));

  EXPECT_SUCCESS(path.get_control_box(&control_box));
  EXPECT_SUCCESS(path.get_bounding_box(&bounding_box));

  expect_box_eq(control_box, BLBox(0.0, 0.0, 4.0, 4.0));
  expect_box_eq(bounding_box, BLBox(0.0, 0.0, 4.0, 2.0));
}

UNIT(path_bounds_of_conic_curve, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath path;
  BLBox control_box;
  BLBox bounding_box;

  EXPECT_SUCCESS(path.move_to(0.0, 0.0));
  EXPECT_SUCCESS(path.conic_to(2.0, 4.0, 4.0, 0.0, 1.0));

  EXPECT_SUCCESS(path.get_control_box(&control_box));
  EXPECT_SUCCESS(path.get_bounding_box(&bounding_box));

  expect_box_eq(control_box, BLBox(0.0, 0.0, 4.0, 4.0));
  expect_box_eq(bounding_box, BLBox(0.0, 0.0, 4.0, 2.0));
}

UNIT(path_bounds_of_cubic_curve, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath path;
  BLBox control_box;
  BLBox bounding_box;

  EXPECT_SUCCESS(path.move_to(0.0, 0.0));
  EXPECT_SUCCESS(path.cubic_to(0.0, 3.0, 3.0, 3.0, 3.0, 0.0));

  EXPECT_SUCCESS(path.get_control_box(&control_box));
  EXPECT_SUCCESS(path.get_bounding_box(&bounding_box));

  expect_box_eq(control_box, BLBox(0.0, 0.0, 3.0, 3.0));
  expect_box_eq(bounding_box, BLBox(0.0, 0.0, 3.0, 2.25));
}

UNIT(path_stroke_of_line_with_round_caps_emits_conics, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath src;
  BLPath dst;
  BLStrokeOptions options;

  options.width = 4.0;
  options.set_caps(BL_STROKE_CAP_ROUND);

  EXPECT_SUCCESS(src.move_to(0.0, 0.0));
  EXPECT_SUCCESS(src.line_to(10.0, 0.0));
  EXPECT_SUCCESS(dst.add_stroked_path(src, options, bl_default_approximation_options));

  EXPECT_EQ(dst.conic_weight_size(), size_t(4));
  EXPECT_EQ(count_command(dst, BL_PATH_CMD_CONIC), size_t(4));
}

UNIT(path_stroke_of_polyline_with_round_join_emits_conic, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath src;
  BLPath dst;
  BLStrokeOptions options;

  options.width = 4.0;
  options.join = BL_STROKE_JOIN_ROUND;
  options.set_caps(BL_STROKE_CAP_BUTT);

  EXPECT_SUCCESS(src.move_to(0.0, 0.0));
  EXPECT_SUCCESS(src.line_to(10.0, 0.0));
  EXPECT_SUCCESS(src.line_to(10.0, 10.0));
  EXPECT_SUCCESS(dst.add_stroked_path(src, options, bl_default_approximation_options));

  EXPECT_EQ(dst.conic_weight_size(), size_t(1));
  EXPECT_EQ(count_command(dst, BL_PATH_CMD_CONIC), size_t(1));
}

} // {Tests}
} // {bl}

#endif // BL_TEST
