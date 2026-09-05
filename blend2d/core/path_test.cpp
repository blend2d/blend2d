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

static void expect_point_near(const BLPoint& actual, const BLPoint& expected, double tolerance = 1e-9) {
  EXPECT_TRUE(bl_abs(actual.x - expected.x) <= tolerance);
  EXPECT_TRUE(bl_abs(actual.y - expected.y) <= tolerance);
}

static void expect_path_near(const BLPath& actual, const BLPath& expected, double tolerance = 1e-9) {
  EXPECT_EQ(actual.size(), expected.size());
  EXPECT_EQ(actual.conic_weight_size(), expected.conic_weight_size());

  const uint8_t* actual_cmd = actual.command_data();
  const uint8_t* expected_cmd = expected.command_data();
  const BLPoint* actual_vtx = actual.vertex_data();
  const BLPoint* expected_vtx = expected.vertex_data();

  for (size_t i = 0; i < actual.size(); i++) {
    EXPECT_EQ(actual_cmd[i], expected_cmd[i]);
    if (actual_cmd[i] != BL_PATH_CMD_CLOSE)
      expect_point_near(actual_vtx[i], expected_vtx[i], tolerance);
  }

  const double* actual_weights = actual.conic_weight_data();
  const double* expected_weights = expected.conic_weight_data();
  for (size_t i = 0; i < actual.conic_weight_size(); i++)
    EXPECT_TRUE(bl_abs(actual_weights[i] - expected_weights[i]) <= tolerance);
}

struct StrokeSinkCapture {
  BLPath a;
  BLPath b;
  BLPath c;
  size_t call_count = 0;
};

static BLResult BL_CDECL capture_stroke_sink(BLPathCore* a, BLPathCore* b, BLPathCore* c, size_t input_start, size_t input_end, void* user_data) noexcept {
  bl_unused(input_start, input_end);

  StrokeSinkCapture* capture = static_cast<StrokeSinkCapture*>(user_data);
  capture->a = a->dcast();
  capture->b = b->dcast();
  capture->c = c->dcast();
  capture->call_count++;
  return BL_SUCCESS;
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

UNIT(path_stroke_of_zero_length_line_with_round_caps_emits_conics, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath src;
  BLPath dst;
  BLStrokeOptions options;

  options.width = 4.0;
  options.set_caps(BL_STROKE_CAP_ROUND);

  EXPECT_SUCCESS(src.move_to(3.0, 4.0));
  EXPECT_SUCCESS(src.line_to(3.0, 4.0));
  EXPECT_SUCCESS(dst.add_stroked_path(src, options, bl_default_approximation_options));

  EXPECT_FALSE(dst.is_empty());
  EXPECT_EQ(dst.conic_weight_size(), size_t(4));
  EXPECT_EQ(count_command(dst, BL_PATH_CMD_CONIC), size_t(4));
}

UNIT(path_stroke_of_zero_length_closed_figure_with_round_caps_emits_conics, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath src;
  BLPath dst;
  BLStrokeOptions options;

  options.width = 4.0;
  options.set_caps(BL_STROKE_CAP_ROUND);

  EXPECT_SUCCESS(src.move_to(3.0, 4.0));
  EXPECT_SUCCESS(src.close());
  EXPECT_SUCCESS(dst.add_stroked_path(src, options, bl_default_approximation_options));

  EXPECT_FALSE(dst.is_empty());
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

UNIT(path_stroke_sink_uses_convex_side_for_round_join, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath src;
  BLPath a;
  BLPath b;
  BLPath c;
  BLStrokeOptions options;
  StrokeSinkCapture capture;

  options.width = 4.0;
  options.join = BL_STROKE_JOIN_ROUND;
  options.set_caps(BL_STROKE_CAP_BUTT);

  EXPECT_SUCCESS(src.move_to(0.0, 0.0));
  EXPECT_SUCCESS(src.line_to(10.0, 0.0));
  EXPECT_SUCCESS(src.line_to(10.0, 10.0));
  EXPECT_SUCCESS(bl_path_stroke_to_sink(&src, nullptr, &options, &bl_default_approximation_options, &a, &b, &c, capture_stroke_sink, &capture));

  EXPECT_EQ(capture.call_count, size_t(1));
  EXPECT_EQ(count_command(capture.a, BL_PATH_CMD_CONIC), size_t(0));
  EXPECT_EQ(count_command(capture.b, BL_PATH_CMD_CONIC), size_t(1));

  expect_point_near(capture.a.vertex_data()[0], BLPoint(0.0, 2.0));
  expect_point_near(capture.a.vertex_data()[1], BLPoint(10.0, 2.0));
  expect_point_near(capture.a.vertex_data()[2], BLPoint(10.0, 0.0));
  expect_point_near(capture.a.vertex_data()[3], BLPoint(8.0, 0.0));
  expect_point_near(capture.a.vertex_data()[4], BLPoint(8.0, 10.0));

  expect_point_near(capture.b.vertex_data()[0], BLPoint(0.0, -2.0));
  expect_point_near(capture.b.vertex_data()[1], BLPoint(10.0, -2.0));
  expect_point_near(capture.b.vertex_data()[2], BLPoint(12.0, -2.0));
  expect_point_near(capture.b.vertex_data()[3], BLPoint(12.0, 0.0));
  expect_point_near(capture.b.vertex_data()[4], BLPoint(12.0, 10.0));
}

UNIT(path_stroke_sink_round_join_does_not_flip_inside_out, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath src;
  BLPath a;
  BLPath b;
  BLPath c;
  BLStrokeOptions options;
  StrokeSinkCapture capture;

  options.width = 4.0;
  options.join = BL_STROKE_JOIN_ROUND;
  options.set_caps(BL_STROKE_CAP_BUTT);

  EXPECT_SUCCESS(src.move_to(998.121, 202.38));
  EXPECT_SUCCESS(src.line_to(479.277, 467.463));
  EXPECT_SUCCESS(src.line_to(692.554, 562.961));
  EXPECT_SUCCESS(src.line_to(205.334, 471.029));
  EXPECT_SUCCESS(bl_path_stroke_to_sink(&src, nullptr, &options, &bl_default_approximation_options, &a, &b, &c, capture_stroke_sink, &capture));

  EXPECT_EQ(capture.call_count, size_t(1));
  EXPECT_EQ(count_command(capture.a, BL_PATH_CMD_CONIC), size_t(2));
  EXPECT_EQ(count_command(capture.b, BL_PATH_CMD_CONIC), size_t(2));

  const BLPoint* b_vtx = capture.b.vertex_data();
  EXPECT_TRUE(b_vtx[5].x > b_vtx[4].x);
  EXPECT_TRUE(b_vtx[7].y > b_vtx[6].y);
}

UNIT(path_stroke_sink_miter_clip_uses_contour_direction, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath src;
  BLPath a;
  BLPath b;
  BLPath c;
  BLStrokeOptions options;
  StrokeSinkCapture capture;

  options.width = 4.0;
  options.join = BL_STROKE_JOIN_MITER_CLIP;
  options.set_caps(BL_STROKE_CAP_BUTT);

  EXPECT_SUCCESS(src.move_to(1014.58, 363.059));
  EXPECT_SUCCESS(src.line_to(653.661, 405.843));
  EXPECT_SUCCESS(src.line_to(1112.74, 384.599));
  EXPECT_SUCCESS(src.line_to(716.64, 479.365));
  EXPECT_SUCCESS(bl_path_stroke_to_sink(&src, nullptr, &options, &bl_default_approximation_options, &a, &b, &c, capture_stroke_sink, &capture));

  EXPECT_EQ(capture.call_count, size_t(1));

  const BLPoint* b_vtx = capture.b.vertex_data();

  expect_point_near(b_vtx[6], BLPoint(1113.2053621143584, 386.54410619312154));
  EXPECT_TRUE(b_vtx[4].x > b_vtx[6].x);
  EXPECT_TRUE(b_vtx[5].x > b_vtx[6].x);
}

UNIT(path_stroke_of_low_limit_miter_bevel_matches_bevel, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath src;
  BLPath miter_bevel_dst;
  BLPath bevel_dst;
  BLStrokeOptions miter_bevel_options;
  BLStrokeOptions bevel_options;

  miter_bevel_options.width = 4.0;
  miter_bevel_options.join = BL_STROKE_JOIN_MITER_BEVEL;
  miter_bevel_options.miter_limit = 0.5;
  miter_bevel_options.set_caps(BL_STROKE_CAP_BUTT);

  bevel_options = miter_bevel_options;
  bevel_options.join = BL_STROKE_JOIN_BEVEL;

  EXPECT_SUCCESS(src.move_to(960.277, 72.7924));
  EXPECT_SUCCESS(src.line_to(434.98, 51.1125));
  EXPECT_SUCCESS(src.line_to(493.827, 130.201));
  EXPECT_SUCCESS(src.line_to(683.779, 88.4622));
  EXPECT_SUCCESS(miter_bevel_dst.add_stroked_path(src, miter_bevel_options, bl_default_approximation_options));
  EXPECT_SUCCESS(bevel_dst.add_stroked_path(src, bevel_options, bl_default_approximation_options));

  expect_path_near(miter_bevel_dst, bevel_dst);
}

UNIT(path_stroke_of_low_limit_miter_round_matches_round, BL_TEST_GROUP_GEOMETRY_CONTAINERS) {
  BLPath src;
  BLPath miter_round_dst;
  BLPath round_dst;
  BLStrokeOptions miter_round_options;
  BLStrokeOptions round_options;

  miter_round_options.width = 4.0;
  miter_round_options.join = BL_STROKE_JOIN_MITER_ROUND;
  miter_round_options.miter_limit = 0.5;
  miter_round_options.set_caps(BL_STROKE_CAP_BUTT);

  round_options = miter_round_options;
  round_options.join = BL_STROKE_JOIN_ROUND;

  EXPECT_SUCCESS(src.move_to(960.277, 72.7924));
  EXPECT_SUCCESS(src.line_to(434.98, 51.1125));
  EXPECT_SUCCESS(src.line_to(493.827, 130.201));
  EXPECT_SUCCESS(src.line_to(683.779, 88.4622));
  EXPECT_SUCCESS(miter_round_dst.add_stroked_path(src, miter_round_options, bl_default_approximation_options));
  EXPECT_SUCCESS(round_dst.add_stroked_path(src, round_options, bl_default_approximation_options));

  expect_path_near(miter_round_dst, round_dst);
}

} // {Tests}
} // {bl}

#endif // BL_TEST
