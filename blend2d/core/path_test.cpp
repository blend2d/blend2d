// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mathconst_p.h>
#include <blend2d/core/matrix.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/path_p.h>
#include <blend2d/core/path_test_p.h>
#include <blend2d/geometry/commons_p.h>

#include <initializer_list>

// Path - Tests
// ============

namespace bl::PathTests {

using MoveTo = BLPath::MoveTo;
using LineTo = BLPath::LineTo;
using QuadTo = BLPath::QuadTo;
using ConicTo = BLPath::ConicTo;
using CubicTo = BLPath::CubicTo;
using Close = BLPath::Close;

static constexpr uint32_t kPathMaxUlpDiff = 4;

static constexpr BLPointI test_polygon_i[] = {
  {-1,  1}, { 2,  3}, { 4,  5}, { 6,  7}, { 8,  9},
  {34, 19}, { 1,  6}, {94, 13}, {16, 67}, {58, 39},
  {14, 95}, {31, 56}, {74, 18}, {76, 64}, {28, 59}
};

static constexpr BLPoint test_polygon_d[] = {
  {-1,  1}, { 2,  3}, { 4,  5}, { 6,  7}, { 8,  9},
  {34, 10}, { 1,  6}, {94, 13}, {16, 67}, {58, 39},
  {14, 95}, {31, 56}, {74, 18}, {76, 64}, {28, 59}
};

TEST_CASE(core_path_constructors) {
  BLPath p1;

  EXPECT_TRUE(p1.is_empty());
  EXPECT_EQ(p1.byte_size(), 0u);
  EXPECT_EQ(p1.vertex_count(), 0u);
  EXPECT_EQ(p1.vertex_data_size(), 0u);
  EXPECT_EQ(p1.control_data_size(), 0u);

  BLPath p2 = BLPath::make_from_segments(MoveTo{10.0, 12.0}, LineTo{20.0, 30.0});
  BLPath p3(p2);

  EXPECT_TRUE(path_view_check_equality(p2.view(), p3.view(), "Path::Path"));

  EXPECT_FALSE(p1.equals(p2));
  EXPECT_TRUE(p2.equals(p3));

  BLPath moved(axl::move(p2));
  EXPECT_TRUE(p2.is_empty());

  EXPECT_OK(p3.reset());
  EXPECT_TRUE(p3.is_empty());

  EXPECT_OK(p3.assign(axl::move(moved)));
  EXPECT_TRUE(moved.is_empty());
}

TEST_CASE(core_path_building) {
  for (uint32_t variation : {0u, 1u, 2u, 3u}) {
    for (size_t shape = 0; shape < 13; shape++) {
      BLPath path;
      BLPath path_copy;

      // apply-variation `variation` to the test case.
      auto av = [&]() {
        switch (variation) {
          case 1: path_copy = path; break;
          case 2: path.shrink(); break;
          case 3: path.reserve_for_append(1); break;
        }
      };

      av(); if (shape >=  1) EXPECT_OK(path.move_to(1, 2));
      av(); if (shape >=  2) EXPECT_OK(path.line_to(3, 4));
      av(); if (shape >=  3) EXPECT_OK(path.quad_to(5, 6, 7, 8));
      av(); if (shape >=  4) EXPECT_OK(path.cubic_to(9, 10, 11, 12, 13, 14));
      av(); if (shape >=  5) EXPECT_OK(path.conic_to(15, 16, 17, 18, 0.3));
      av(); if (shape >=  6) EXPECT_OK(path.arc_quadrant_to(24, 24, 40, 40));
      av(); if (shape >=  7) EXPECT_OK(path.poly_to(test_polygon_d, BL_ARRAY_SIZE(test_polygon_d)));
      av(); if (shape >=  8) EXPECT_OK(path.close());
      av(); if (shape >=  9) EXPECT_OK(path.add_box(10.0, 20.0, 100.0, 200.0, BL_GEOMETRY_DIRECTION_CW));
      av(); if (shape >= 10) EXPECT_OK(path.add_box(20.0, 30.0, 110.0, 210.0, BL_GEOMETRY_DIRECTION_CCW));
      av(); if (shape >= 11) EXPECT_OK(path.add_rect(11.0, 21.0, 101.0, 201.0, BL_GEOMETRY_DIRECTION_CW));
      av(); if (shape >= 12) EXPECT_OK(path.add_rect(21.0, 31.0, 111.0, 211.0, BL_GEOMETRY_DIRECTION_CCW));

      BLPath expected;
      PathAppender appender;
      EXPECT_OK(appender.begin_append(&expected, path.byte_size()));

      if (shape >=  1) appender.move_to(1, 2);
      if (shape >=  2) appender.line_to(3, 4);
      if (shape >=  3) appender.quad_to(5, 6, 7, 8);
      if (shape >=  4) appender.cubic_to(9, 10, 11, 12, 13, 14);
      if (shape >=  5) appender.conic_to(15, 16, 17, 18, 0.3);
      if (shape >=  6) appender.arc_quadrant_to(24, 24, 40, 40);
      if (shape >=  7) appender.poly_to(test_polygon_d, BL_ARRAY_SIZE(test_polygon_d));
      if (shape >=  8) appender.close();
      if (shape >=  9) appender.add_box_cw(10.0, 20.0, 100.0, 200.0);
      if (shape >= 10) appender.add_box_ccw(20.0, 30.0, 110.0, 210.0);
      if (shape >= 11) appender.add_rect_cw(11.0, 21.0, 101.0, 201.0);
      if (shape >= 12) appender.add_rect_ccw(21.0, 31.0, 111.0, 211.0);

      appender.done(&expected);

      EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(),
        "Path::{move_to|line_to|quad_to|cubic_to|conic_to|poly_to}()"));
    }
  }
}

TEST_CASE(core_path_add_uninitialized) {
  BLPath path;
  BLPoint* vertex_data;
  uint8_t* control_data_rev;
  EXPECT_OK(path.add_uninitialized(10, 7, &vertex_data, &control_data_rev));

  memcpy(vertex_data, test_polygon_d, sizeof(BLPoint) * 10);
  control_data_rev[-1] = BL_PATH_CMD_MOVE;
  control_data_rev[-2] = BL_PATH_CMD_LINE;
  control_data_rev[-3] = BL_PATH_CMD_QUAD;
  control_data_rev[-4] = BL_PATH_CMD_QUAD;
  control_data_rev[-5] = BL_PATH_CMD_QUAD;
  control_data_rev[-6] = BL_PATH_CMD_QUAD;
  control_data_rev[-7] = BL_PATH_CMD_CLOSE;

  BLPath expected;
  EXPECT_OK(expected.move_to(test_polygon_d[0]));
  EXPECT_OK(expected.line_to(test_polygon_d[1]));
  EXPECT_OK(expected.quad_to(test_polygon_d[2], test_polygon_d[3]));
  EXPECT_OK(expected.quad_to(test_polygon_d[4], test_polygon_d[5]));
  EXPECT_OK(expected.quad_to(test_polygon_d[6], test_polygon_d[7]));
  EXPECT_OK(expected.quad_to(test_polygon_d[8], test_polygon_d[9]));
  EXPECT_OK(expected.close());

  EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(),
    "Path::add_uninitialized()"));
}

TEST_CASE(core_path_add_segments) {
  const MoveTo move{1, 2};
  LineTo line{3, 4};
  const ConicTo conic{9, 10, 11, 12, 0.5};

  for (uint32_t variation : {0u, 1u, 2u, 3u}) {
    BLPath path;
    BLPath original;
    if (variation) {
      EXPECT_OK(original.move_to(-1, -2));
      EXPECT_OK(original.conic_to(-3, -4, -5, -6, 0.25));
      EXPECT_OK(original.close());
      EXPECT_OK(path.assign_deep(original));
    }
    if (variation == 1) EXPECT_OK(path.shrink());
    if (variation == 2) EXPECT_OK(path.reserve_for_append(512));
    BLPath owner;
    if (variation == 3) owner = path;

    EXPECT_OK(path.add_segments());
    EXPECT_TRUE(path_view_check_equality(path.view(), original.view(), "add_segments: empty pack"));
    // Empty-pack allocation can detach a shared path; share again to exercise a nonempty append.
    if (variation == 3) owner = path;
    BLPath expected;
    EXPECT_OK(expected.assign_deep(original));
    for (uint32_t repeat = 0; repeat < 2; repeat++) {
      EXPECT_OK(path.add_segments(
        move, line, QuadTo{5, 6, 7, 8}, conic,
        ConicTo{13, 14, 15, 16, 0.25}, CubicTo{17, 18, 19, 20, 21, 22}, Close{}));
      EXPECT_OK(expected.move_to(1, 2));
      EXPECT_OK(expected.line_to(3, 4));
      EXPECT_OK(expected.quad_to(5, 6, 7, 8));
      EXPECT_OK(expected.conic_to(9, 10, 11, 12, 0.5));
      EXPECT_OK(expected.conic_to(13, 14, 15, 16, 0.25));
      EXPECT_OK(expected.cubic_to(17, 18, 19, 20, 21, 22));
      EXPECT_OK(expected.close());
      EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(), "add_segments: mixed segments and repeated appends"));
    }
    if (variation == 3) {
      EXPECT_TRUE(path_view_check_equality(owner.view(), original.view(), "add_segments: owner unchanged"));
    }
  }
}

TEST_CASE(core_path_poly_to) {
  BLPoint initial(100.0, 200.0);

  for (uint32_t variation : {0u, 1u, 2u, 3u}) {
    for (size_t size = 0; size <= BL_ARRAY_SIZE(test_polygon_d); size++) {
      BLPath path;
      BLPath path_copy;

      // apply-variation `variation` to the test case.
      auto av = [&]() {
        switch (variation) {
          case 1: path_copy = path; break;
          case 2: path.shrink(); break;
          case 3: path.reserve_for_append(1); break;
        }
      };

      av(); EXPECT_OK(path.move_to(initial));
      av(); EXPECT_OK(path.poly_to(test_polygon_d, size));
      av(); EXPECT_OK(path.close());

      BLPath expected;
      EXPECT_OK(expected.move_to(initial));
      for (size_t i = 0; i < size; i++) {
        EXPECT_OK(expected.line_to(test_polygon_d[i]));
      }

      EXPECT_OK(expected.close());
      EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(), "Path::poly_to()"));
    }
  }
}

TEST_CASE(core_path_smooth_quad_to) {
  for (const BLPath& original : {
    BLPath(),
    BLPath::make_from_segments(MoveTo{1, 2}, LineTo{3, 4}, Close{})}
  ) {
    BLPath path(original);
    EXPECT_EQ(path.smooth_quad_to(5, 6), BL_ERROR_NO_MATCHING_VERTEX);
    EXPECT_TRUE(path_view_check_equality(path.view(), original.view(), "smooth_quad_to: invalid endpoint"));
  }

  struct Fixture {
    BLPath prefix;
    BLPoint control;
    bool reflects = false;
  };

  const Fixture fixtures[] = {
    {BLPath::make_from_segments(MoveTo{0, 0}), {0, 0}},
    {BLPath::make_from_segments(MoveTo{0, 0}, LineTo{3.3, 4.4}), {3.3, 4.4}},
    {BLPath::make_from_segments(MoveTo{0, 0}, CubicTo{1.1, 2.2, 3.3, 4.4, 5.5, 6.6}), {5.5, 6.6}},
    {BLPath::make_from_segments(MoveTo{0, 0}, ConicTo{1.1, 2.2, 3.3, 4.4, 0.5}), {3.3, 4.4}},
    {BLPath::make_from_segments(MoveTo{0.5, 0.5}, QuadTo{-2.3, 12.2, 1.7, -19.9}), {5.7, -52}, true}
  };

  // Only QUAD reflects its control point; MOVE, LINE, CUBIC, and CONIC use the endpoint.
  for (const Fixture& fixture : fixtures) {
    BLPath expected(fixture.prefix);
    EXPECT_OK(expected.add_segments(QuadTo{fixture.control.x, fixture.control.y, 9.9, 8.8}));
    for (uint32_t variation : {0u, 1u, 2u}) {
      BLPath path;
      EXPECT_OK(path.assign_deep(fixture.prefix));

      BLPath owner;
      if (variation == 0) EXPECT_OK(path.shrink());
      if (variation == 1) EXPECT_OK(path.reserve_for_append(QuadTo::kByteSize));
      if (variation == 2) owner = path;
      EXPECT_OK(path.smooth_quad_to(9.9, 8.8));

      const char* description = "smooth_quad_to: previous command, growth, and copy on write";
      EXPECT_TRUE(fixture.reflects
        ? path_view_check_equality_ulp(path.view(), expected.view(), kPathMaxUlpDiff, description)
        : path_view_check_equality(path.view(), expected.view(), description));

      if (variation == 2) {
        EXPECT_TRUE(path_view_check_equality(owner.view(), fixture.prefix.view(), "smooth_quad_to: owner unchanged"));
      }
    }
  }

  BLPath path = BLPath::make_from_segments(MoveTo{-5.5, 2.5}, QuadTo{-2.3, 12.2, 1.7, -19.9});
  EXPECT_OK(path.smooth_quad_to(10.5, 3.25));
  EXPECT_OK(path.smooth_quad_to(-7.5, 8.5));
  BLPath expected = BLPath::make_from_segments(
    MoveTo{-5.5, 2.5}, QuadTo{-2.3, 12.2, 1.7, -19.9},
    QuadTo{5.7, -52, 10.5, 3.25}, QuadTo{15.3, 58.5, -7.5, 8.5});
  EXPECT_TRUE(path_view_check_equality_ulp(path.view(), expected.view(), kPathMaxUlpDiff, "smooth_quad_to: chained reflection"));
}

TEST_CASE(core_path_smooth_cubic_to) {
  for (const BLPath& original : {
    BLPath(),
    BLPath::make_from_segments(MoveTo{1, 2}, LineTo{3, 4}, Close{})}
  ) {
    BLPath path(original);
    EXPECT_EQ(path.smooth_cubic_to(5, 6, 7, 8), BL_ERROR_NO_MATCHING_VERTEX);
    EXPECT_TRUE(path_view_check_equality(path.view(), original.view(), "smooth_cubic_to: invalid endpoint"));
  }

  struct Fixture {
    BLPath prefix;
    BLPoint control;
    bool reflects = false;
  };
  const Fixture fixtures[] = {
    {BLPath::make_from_segments(MoveTo{0, 0}), {0, 0}},
    {BLPath::make_from_segments(MoveTo{0, 0}, LineTo{3.3, 4.4}), {3.3, 4.4}},
    {BLPath::make_from_segments(MoveTo{0, 0}, QuadTo{1.1, 2.2, 3.3, 4.4}), {3.3, 4.4}},
    {BLPath::make_from_segments(MoveTo{0, 0}, ConicTo{1.1, 2.2, 3.3, 4.4, 0.5}), {3.3, 4.4}},
    {BLPath::make_from_segments(MoveTo{0.5, 0.5}, CubicTo{4.4, 5.5, -2.3, 12.3, 1.7, -19.9}), {5.7, -52.1}, true}
  };

  // Only CUBIC reflects its second control point; other commands use the endpoint.
  for (const Fixture& fixture : fixtures) {
    BLPath expected(fixture.prefix);
    EXPECT_OK(expected.add_segments(CubicTo{fixture.control.x, fixture.control.y, 8.8, 1.1, 10.5, 3.25}));
    for (uint32_t variation : {0u, 1u, 2u}) {
      BLPath path;
      EXPECT_OK(path.assign_deep(fixture.prefix));

      BLPath owner;
      if (variation == 0) EXPECT_OK(path.shrink());
      if (variation == 1) EXPECT_OK(path.reserve_for_append(CubicTo::kByteSize));
      if (variation == 2) owner = path;

      EXPECT_OK(path.smooth_cubic_to(8.8, 1.1, 10.5, 3.25));

      const char* description = "smooth_cubic_to: previous command, growth, and copy on write";
      EXPECT_TRUE(fixture.reflects
        ? path_view_check_equality_ulp(path.view(), expected.view(), kPathMaxUlpDiff, description)
        : path_view_check_equality(path.view(), expected.view(), description));

      if (variation == 2) {
        EXPECT_TRUE(path_view_check_equality(owner.view(), fixture.prefix.view(), "smooth_cubic_to: owner unchanged"));
      }
    }
  }

  BLPath path = BLPath::make_from_segments(MoveTo{-5.5, 2.5}, CubicTo{4.4, 5.5, -2.3, 12.3, 1.7, -19.9});
  EXPECT_OK(path.smooth_cubic_to(8.8, 1.1, 10.5, 3.25));
  EXPECT_OK(path.smooth_cubic_to(-4.4, 7.7, -7.5, 8.5));

  BLPath expected = BLPath::make_from_segments(
    MoveTo{-5.5, 2.5},
    CubicTo{4.4, 5.5, -2.3, 12.3, 1.7, -19.9},
    CubicTo{5.7, -52.1, 8.8, 1.1, 10.5, 3.25},
    CubicTo{12.2, 5.4, -4.4, 7.7, -7.5, 8.5});
  EXPECT_TRUE(path_view_check_equality_ulp(path.view(), expected.view(), kPathMaxUlpDiff, "smooth_cubic_to: chained reflection"));
}

TEST_CASE(core_path_arc_to) {
  // Quadrants of the ellipse centered at (10, 20), with radii (5, 6).
  // Each pair is a conic control point followed by its end point.
  const BLPoint quadrants[][8] = {
    {{15, 26}, {10, 26}, { 5, 26}, { 5, 20}, { 5, 14}, {10, 14}, {15, 14}, {15, 20}},
    {{15, 14}, {10, 14}, { 5, 14}, { 5, 20}, { 5, 26}, {10, 26}, {15, 26}, {15, 20}}
  };

  struct Fixture {
    double sweep;
    size_t direction;
    size_t conic_count;
  };

  const Fixture fixtures[] = {
    { Math::kPI_DIV_2    , 0, 1},
    {-Math::kPI_DIV_2    , 1, 1},
    { Math::kPI          , 0, 2},
    {-Math::kPI          , 1, 2},
    { Math::kPI_MUL_2    , 0, 4},
    {-Math::kPI_MUL_2    , 1, 4},
    { Math::kPI_MUL_2 * 2, 0, 4},
    {-Math::kPI_MUL_2 * 2, 1, 4}
  };

  for (const Fixture& fixture : fixtures) {
    BLPath path;
    EXPECT_OK(path.arc_to(10, 20, 5, 6, 0, fixture.sweep));

    BLPath expected = BLPath::make_from_segments(MoveTo{15, 20});
    for (size_t i = 0; i < fixture.conic_count; i++) {
      // Intermediate quadrants use the stored constant; the final segment rounds sqrt(0.5).
      double weight = i + 1 == fixture.conic_count ? 0.7071067811865476 : 0.7071067811865475;
      EXPECT_OK(expected.conic_to(quadrants[fixture.direction][i * 2],
                                 quadrants[fixture.direction][i * 2 + 1], weight));
    }

    EXPECT_TRUE(path_view_check_equality_ulp(path.view(), expected.view(), kPathMaxUlpDiff,
      "BLPath::arc_to(): quarter, half, full, and clamped sweeps"));
  }

  // A nonzero start angle rotates the same quadrant to the bottom-left of the ellipse.
  {
    BLPath path;
    EXPECT_OK(path.arc_to(BLPoint(10, 20), BLPoint(5, 6), Math::kPI_DIV_2, Math::kPI_DIV_2));

    BLPath expected = BLPath::make_from_segments(MoveTo{10, 26}, ConicTo{5, 26, 5, 20, 0.7071067811865476});
    EXPECT_TRUE(path_view_check_equality_ulp(path.view(), expected.view(), kPathMaxUlpDiff,
      "BLPath::arc_to(): nonzero start angle"));
  }
}

TEST_CASE(core_path_arc_to_append) {
  enum InitialCommand { kNoStartCommand, kMoveTo, kLineTo };

  struct Fixture {
    uint32_t prefix;
    bool force_move_to;
    InitialCommand initial_cmd;
  };

  const Fixture fixtures[] = {
    {0, false, kMoveTo        }, // Empty path.
    {1, false, kNoStartCommand}, // Matching MOVE.
    {2, false, kNoStartCommand}, // Matching LINE.
    {3, false, kLineTo        }, // Nonmatching endpoint.
    {4, false, kMoveTo        }, // Closed figure.
    {2, true , kMoveTo        }, // Force a new figure at a matching endpoint.
    {3, true , kMoveTo        }  // Force a new figure at a nonmatching endpoint.
  };

  for (const Fixture& fixture : fixtures) {
    BLPath path;
    switch (fixture.prefix) {
      case 1:
        EXPECT_OK(path.move_to(15, 20));
        break;

      case 2:
        EXPECT_OK(path.move_to(1, 2));
        EXPECT_OK(path.line_to(15, 20));
        break;

      case 3:
      case 4:
        EXPECT_OK(path.move_to(1, 2));
        EXPECT_OK(path.line_to(3, 4));
        if (fixture.prefix == 4) {
          EXPECT_OK(path.close());
        }
        break;
    }

    BLPath original;
    EXPECT_OK(original.assign_deep(path));

    BLPath expected;
    EXPECT_OK(expected.assign_deep(path));

    BLPath shared(path);
    EXPECT_OK(path.arc_to(10, 20, 5, 6, 0, Math::kPI_DIV_2, fixture.force_move_to));

    if (fixture.initial_cmd == kMoveTo) {
      EXPECT_OK(expected.move_to(15, 20));
    }
    else if (fixture.initial_cmd == kLineTo) {
      EXPECT_OK(expected.line_to(15, 20));
    }

    EXPECT_OK(expected.conic_to(15, 26, 10, 26, 0.7071067811865476));
    EXPECT_TRUE(path_view_check_equality_ulp(path.view(), expected.view(), kPathMaxUlpDiff,
      "BLPath::arc_to(): destination state and force_move_to"));
    EXPECT_TRUE(path_view_check_equality(shared.view(), original.view(),
      "BLPath::arc_to(): shared owner unchanged"));
  }
}

TEST_CASE(core_path_elliptic_arc_to) {
  struct Fixture {
    bool large_arc;
    bool sweep;
    size_t conic_count;
    BLPoint points[6];
  };

  // Fixed quarter-ellipse and three-quarter-ellipse paths, all starting at (16, 20)
  // and ending at (12, 28), with radii (4, 8). Each pair is a control/end point.
  const Fixture fixtures[] = {
    {false, true , 1, {{16, 28}, {12, 28}}},
    {false, false, 1, {{12, 20}, {12, 28}}},
    {true , true , 3, {{20, 20}, {20, 28}, {20, 36}, {16, 36}, {12, 36}, {12, 28}}},
    {true , false, 3, {{16, 12}, {12, 12}, { 8, 12}, { 8, 20}, { 8, 28}, {12, 28}}}
  };

  for (const Fixture& fixture : fixtures) {
    BLPath path = BLPath::make_from_segments(MoveTo{16, 20});
    BLPath shared(path);
    EXPECT_OK(path.elliptic_arc_to(4, 8, 0, fixture.large_arc, fixture.sweep, 12, 28));

    BLPath expected = BLPath::make_from_segments(MoveTo{16, 20});
    for (size_t i = 0; i < fixture.conic_count; i++) {
      double weight = i + 1 == fixture.conic_count ? 0.7071067811865476 : 0.7071067811865475;
      EXPECT_OK(expected.conic_to(fixture.points[i * 2], fixture.points[i * 2 + 1], weight));
    }

    EXPECT_TRUE(path_view_check_equality_ulp(path.view(), expected.view(), kPathMaxUlpDiff,
      "BLPath::elliptic_arc_to(): large-arc and sweep flags"));

    BLPath original = BLPath::make_from_segments(MoveTo{16, 20});
    EXPECT_TRUE(path_view_check_equality(shared.view(), original.view(),
      "BLPath::elliptic_arc_to(): shared owner unchanged"));
  }

  // Negative radii describe the same quarter ellipse.
  {
    BLPath path = BLPath::make_from_segments(MoveTo{16, 20});
    EXPECT_OK(path.elliptic_arc_to(-4, -8, 0, false, true, 12, 28));

    BLPath expected = BLPath::make_from_segments(MoveTo{16, 20}, ConicTo{16, 28, 12, 28, 0.7071067811865476});

    EXPECT_TRUE(path_view_check_equality_ulp(path.view(), expected.view(), kPathMaxUlpDiff,
      "BLPath::elliptic_arc_to(): negative radii"));
  }

  // Rotating an ellipse by 90 degrees swaps its horizontal and vertical radii.
  {
    BLPath path = BLPath::make_from_segments(MoveTo{12, 24});
    EXPECT_OK(path.elliptic_arc_to(4, 8, Math::kPI_DIV_2, false, true, 4, 20));

    BLPath expected = BLPath::make_from_segments(MoveTo{12, 24}, ConicTo{4, 24, 4, 20, 0.7071067811865476});

    EXPECT_TRUE(path_view_check_equality_ulp(path.view(), expected.view(), kPathMaxUlpDiff,
      "BLPath::elliptic_arc_to(): 90 degree axis rotation"));
  }
}

TEST_CASE(core_path_elliptic_arc_to_radius_correction) {
  // Radius 1 cannot span (0, 0) to (4, 0), so the arc expands to a semicircle of radius 2.
  const BLPoint quadrants[][4] = {
    {{0,  2}, {2,  2}, {4,  2}, {4, 0}},
    {{0, -2}, {2, -2}, {4, -2}, {4, 0}}
  };

  for (uint32_t sweep : {0u, 1u}) {
    BLPath path = BLPath::make_from_segments(MoveTo{0, 0});
    EXPECT_OK(path.elliptic_arc_to(1, 1, 0, false, sweep != 0, 4, 0));

    BLPath expected = BLPath::make_from_segments(MoveTo{0, 0});
    EXPECT_OK(expected.conic_to(quadrants[sweep][0], quadrants[sweep][1], 0.7071067811865475));
    EXPECT_OK(expected.conic_to(quadrants[sweep][2], quadrants[sweep][3], 0.7071067811865476));

    EXPECT_TRUE(path_view_check_equality_ulp(path.view(), expected.view(), kPathMaxUlpDiff,
      "BLPath::elliptic_arc_to(): radii expand to span the endpoints"));
  }
}

TEST_CASE(core_path_elliptic_arc_to_invalid) {
  {
    BLPath path;
    EXPECT_EQ(path.elliptic_arc_to(4, 8, 0, false, true, 12, 28), BL_ERROR_NO_MATCHING_VERTEX);
    EXPECT_TRUE(path.is_empty());
  }

  {
    BLPath path = BLPath::make_from_segments(MoveTo{16, 20}, LineTo{12, 28}, Close{});
    EXPECT_EQ(path.elliptic_arc_to(4, 8, 0, false, true, 8, 20), BL_ERROR_NO_MATCHING_VERTEX);

    BLPath expected = BLPath::make_from_segments(MoveTo{16, 20}, LineTo{12, 28}, Close{});

    EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(),
      "BLPath::elliptic_arc_to(): closed figure unchanged on error"));
  }
}

TEST_CASE(core_path_elliptic_arc_to_degenerate) {
  // Coinciding endpoints leave the path unchanged, even with zero radii.
  for (const BLPoint& radii : {BLPoint(4, 8), BLPoint(0, 0)}) {
    BLPath path = BLPath::make_from_segments(MoveTo{1, 2}, LineTo{16, 20});
    EXPECT_OK(path.elliptic_arc_to(radii, 0, false, true, BLPoint(16, 20)));

    BLPath expected = BLPath::make_from_segments(MoveTo{1, 2}, LineTo{16, 20});

    EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(),
      "BLPath::elliptic_arc_to(): coinciding endpoints"));
  }

  // Either zero radius reduces the arc to a line.
  for (const BLPoint& radii : {BLPoint(0, 8), BLPoint(4, 0), BLPoint(0, 0)}) {
    BLPath path = BLPath::make_from_segments(MoveTo{16, 20});
    EXPECT_OK(path.elliptic_arc_to(radii, 0, true, false, BLPoint(12, 28)));

    BLPath expected = BLPath::make_from_segments(MoveTo{16, 20}, LineTo{12, 28});

    EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(),
      "BLPath::elliptic_arc_to(): zero radius emits a line"));
  }
}

TEST_CASE(core_path_add_geometry) {
  using D = double;

  const BLPoint initial_move(1, 2);
  const BLPoint initial_line(3, 4);

  BLBoxI src_box_i(0, 1, 2, 3);
  BLBox src_box_d(0.0, 1.0, 2.0, 3.0);

  BLRectI src_rect_i(0, 1, 2, 3);
  BLRect src_rect_d(0.0, 1.0, 2.0, 3.0);

  BLCircle src_circle_d(10.0, 20.0, 30.0);
  BLEllipse src_ellipse_d(10.0, 20.0, 30.0, 40.0);

  BLRoundRect src_round_rect_d(10.0, 20.0, 30.0, 40.0, 5.0, 6.0);
  BLArc src_arc_d(10.0, 20.0, 5.0, 6.0, Math::kPI_DIV_2, Math::kPI);

  // Fixed expectations for src_arc_d; update these fixtures if the input arc changes.
  // Each row contains the start, first control/end, and second control/end (CW, CCW).
  const BLPoint arc_center(10.0, 20.0);
  const BLPoint arc_vertices[][5] = {
    {{10.0, 26.0}, { 5.0, 26.0}, { 5.0, 20.0}, { 5.0, 14.0}, {10.0, 14.0}},
    {{10.0, 26.0}, {15.0, 26.0}, {15.0, 20.0}, {15.0, 14.0}, {10.0, 14.0}}
  };
  // The final weight differs by one ULP due to the PI sweep's trigonometric rounding.
  const double arc_weights[] = {0.7071067811865475, 0.7071067811865476};

  BLLine src_line_d(30.0, 40.0, 50.0, 100.0);
  BLTriangle src_triangle_d(30.0, 40.0, 100.0, 50.0, 25.0, 166.0);

  BLSpan<const BLPointI> src_polygon_i{test_polygon_i, BL_ARRAY_SIZE(test_polygon_i)};
  BLSpan<const BLPoint> src_polygon_d{test_polygon_d, BL_ARRAY_SIZE(test_polygon_d)};

  const BLBoxI box_i_data[] = {
    {  0,   1, 200, 300},
    {400,   1, 666, 22},
    {443,  55, 502, 112},
    {123, 144, 150, 190}
  };

  const BLBox box_d_data[] = {
    {  0,   1, 200, 300},
    {400,   1, 666, 22},
    {443,  55, 502, 112},
    {123, 144, 150, 190}
  };

  const BLRectI rect_i_data[] = {
    {  0,   1,  20,   3},
    {400,   1,  66, 222},
    {443,  55, 102, 412},
    {123, 144, 850, 490}
  };

  const BLRect rect_d_data[] = {
    {  0,   1,  20,   3},
    {400,   1,  66, 222},
    {443,  55, 102, 412},
    {123, 144, 850, 490}
  };

  BLSpan<const BLBoxI> src_box_i_span(box_i_data, BL_ARRAY_SIZE(box_i_data));
  BLSpan<const BLBox> src_box_d_span(box_d_data, BL_ARRAY_SIZE(box_d_data));
  BLSpan<const BLRectI> src_rect_i_span(rect_i_data, BL_ARRAY_SIZE(rect_i_data));
  BLSpan<const BLRect> src_rect_d_span(rect_d_data, BL_ARRAY_SIZE(rect_d_data));

  BLPath src_path = BLPath::make_from_segments(
    MoveTo{123.0, 443.0}, LineTo{223.0, 443.0}, QuadTo{123.0, 243.0, 150.0, 22.0},
    ConicTo{50.0, 75.0, 100.0, 125.0, 0.5}, Close{});

  const BLMatrix2D transforms[] = {
    BLMatrix2D::make_identity(),
    BLMatrix2D::make_translation(1.0, 2.0),
    BLMatrix2D(2, 0, 0, -3, 100, -200),
    BLMatrix2D(0, 2, -3, 0, 100, -200),
    BLMatrix2D(2, 1, -1, 3, 100, -200)
  };

  for (uint32_t dirty_path : {0u, 1u}) {
    for (uint32_t variation : {0u, 1u, 2u, 3u}) {
      for (uint32_t geometry_type = 0; geometry_type <= BL_GEOMETRY_TYPE_MAX_VALUE; geometry_type++) {
        for (uint32_t geometry_direction = 0; geometry_direction <= BL_GEOMETRY_DIRECTION_MAX_VALUE; geometry_direction++) {
          for (size_t transform = 0; transform <= BL_ARRAY_SIZE(transforms); transform++) {
            BLPath path;
            BLPath path_copy;

            // apply-variation `variation` to the test case.
            auto av = [&]() {
              switch (variation) {
                case 1: path_copy = path; break;
                case 2: path.shrink(); break;
                case 3: path.reserve_for_append(1); break;
              }
            };

            const BLMatrix2D* t = transform ? &transforms[transform - 1] : nullptr;
            BLGeometryDirection d = static_cast<BLGeometryDirection>(geometry_direction);

            if (dirty_path) {
              EXPECT_OK(path.move_to(initial_move));
              EXPECT_OK(path.line_to(initial_line));
            }

            av();

            switch (geometry_type) {
              case BL_GEOMETRY_TYPE_NONE        : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_NONE        , nullptr          , t, d)); break;
              case BL_GEOMETRY_TYPE_BOX_I       : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_BOX_I       , &src_box_i       , t, d)); break;
              case BL_GEOMETRY_TYPE_BOX_D       : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_BOX_D       , &src_box_d       , t, d)); break;
              case BL_GEOMETRY_TYPE_RECT_I      : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_RECT_I      , &src_rect_i      , t, d)); break;
              case BL_GEOMETRY_TYPE_RECT_D      : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_RECT_D      , &src_rect_d      , t, d)); break;
              case BL_GEOMETRY_TYPE_CIRCLE_D    : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_CIRCLE_D    , &src_circle_d    , t, d)); break;
              case BL_GEOMETRY_TYPE_ELLIPSE_D   : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_ELLIPSE_D   , &src_ellipse_d   , t, d)); break;
              case BL_GEOMETRY_TYPE_ROUND_RECT_D: EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_ROUND_RECT_D, &src_round_rect_d, t, d)); break;
              case BL_GEOMETRY_TYPE_ARC_D       : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_ARC_D       , &src_arc_d       , t, d)); break;
              case BL_GEOMETRY_TYPE_CHORD_D     : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_CHORD_D     , &src_arc_d       , t, d)); break;
              case BL_GEOMETRY_TYPE_PIE_D       : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_PIE_D       , &src_arc_d       , t, d)); break;
              case BL_GEOMETRY_TYPE_LINE_D      : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_LINE_D      , &src_line_d      , t, d)); break;
              case BL_GEOMETRY_TYPE_TRIANGLE_D  : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_TRIANGLE_D  , &src_triangle_d  , t, d)); break;
              case BL_GEOMETRY_TYPE_POLYLINE_I  : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_POLYLINE_I  , &src_polygon_i   , t, d)); break;
              case BL_GEOMETRY_TYPE_POLYLINE_D  : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_POLYLINE_D  , &src_polygon_d   , t, d)); break;
              case BL_GEOMETRY_TYPE_POLYGON_I   : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_POLYGON_I   , &src_polygon_i   , t, d)); break;
              case BL_GEOMETRY_TYPE_POLYGON_D   : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_POLYGON_D   , &src_polygon_d   , t, d)); break;
              case BL_GEOMETRY_TYPE_SPAN_BOX_I  : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_SPAN_BOX_I  , &src_box_i_span  , t, d)); break;
              case BL_GEOMETRY_TYPE_SPAN_BOX_D  : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_SPAN_BOX_D  , &src_box_d_span  , t, d)); break;
              case BL_GEOMETRY_TYPE_SPAN_RECT_I : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_SPAN_RECT_I , &src_rect_i_span , t, d)); break;
              case BL_GEOMETRY_TYPE_SPAN_RECT_D : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_SPAN_RECT_D , &src_rect_d_span , t, d)); break;
              case BL_GEOMETRY_TYPE_PATH_D      : EXPECT_OK(path.add_geometry(BL_GEOMETRY_TYPE_PATH_D      , &src_path        , t, d)); break;
            }

            BLPath expected_geometry;
            PathAppender appender;
            EXPECT_OK(appender.begin_append(&expected_geometry, path.byte_size()));

            bool cw = d == BL_GEOMETRY_DIRECTION_CW;

            auto append_polygon = [&](const auto& src, bool closed) {
              size_t size = src.size();
              if (!size) {
                return;
              }

              appender.move_to(src.data()[cw ? 0 : size - 1]);
              for (size_t i = 1; i < size; i++) {
                appender.line_to(src.data()[cw ? i : size - 1 - i]);
              }
              if (closed) {
                appender.close();
              }
            };

            auto append_boxes = [&](const auto& src) {
              for (size_t i = 0; i < src.size(); i++) {
                const auto& box = src.data()[cw ? i : src.size() - 1 - i];
                if (Geometry::is_valid(box)) {
                  appender.add_box(D(box.x0), D(box.y0), D(box.x1), D(box.y1), d);
                }
              }
            };

            auto append_rects = [&](const auto& src) {
              for (size_t i = 0; i < src.size(); i++) {
                const auto& rect = src.data()[cw ? i : src.size() - 1 - i];
                if (Geometry::is_valid(rect)) {
                  appender.add_rect(D(rect.x), D(rect.y), D(rect.w), D(rect.h), d);
                }
              }
            };

            switch (geometry_type) {
              case BL_GEOMETRY_TYPE_NONE: break;

              case BL_GEOMETRY_TYPE_BOX_I: appender.add_box(D(src_box_i.x0), D(src_box_i.y0), D(src_box_i.x1), D(src_box_i.y1), d); break;
              case BL_GEOMETRY_TYPE_BOX_D: appender.add_box(D(src_box_d.x0), D(src_box_d.y0), D(src_box_d.x1), D(src_box_d.y1), d); break;
              case BL_GEOMETRY_TYPE_RECT_I: appender.add_rect(D(src_rect_i.x), D(src_rect_i.y), D(src_rect_i.w), D(src_rect_i.h), d); break;
              case BL_GEOMETRY_TYPE_RECT_D: appender.add_rect(D(src_rect_d.x), D(src_rect_d.y), D(src_rect_d.w), D(src_rect_d.h), d); break;

              case BL_GEOMETRY_TYPE_CIRCLE_D:
              case BL_GEOMETRY_TYPE_ELLIPSE_D: {
                BLEllipse ellipse = geometry_type == BL_GEOMETRY_TYPE_CIRCLE_D
                  ? BLEllipse(src_circle_d.cx, src_circle_d.cy, src_circle_d.r, bl_abs(src_circle_d.r))
                  : src_ellipse_d;

                double cx = ellipse.cx;
                double cy = ellipse.cy;
                double ry = ellipse.ry;
                if (!cw) {
                  ry = -ry;
                }

                double x0 = cx - ellipse.rx;
                double y0 = cy - ry;
                double x1 = cx + ellipse.rx;
                double y1 = cy + ry;

                appender.move_to(x1, cy);
                appender.arc_quadrant_to(x1, y1, cx, y1);
                appender.arc_quadrant_to(x0, y1, x0, cy);
                appender.arc_quadrant_to(x0, y0, cx, y0);
                appender.arc_quadrant_to(x1, y0, x1, cy);
                appender.close();
                break;
              }

              case BL_GEOMETRY_TYPE_ROUND_RECT_D: {
                double x0 = src_round_rect_d.x;
                double y0 = src_round_rect_d.y;
                double x1 = x0 + src_round_rect_d.w;
                double y1 = y0 + src_round_rect_d.h;
                double rx = bl_min(bl_abs(src_round_rect_d.rx), src_round_rect_d.w * 0.5);
                double ry = bl_min(bl_abs(src_round_rect_d.ry), src_round_rect_d.h * 0.5);

                if (!(rx > Math::epsilon<double>() && ry > Math::epsilon<double>())) {
                  appender.add_box(x0, y0, x1, y1, d);
                  break;
                }

                appender.move_to(x0 + rx, y0);
                if (cw) {
                  appender.line_to(x1 - rx, y0);
                  appender.arc_quadrant_to(x1, y0, x1, y0 + ry);
                  appender.line_to(x1, y1 - ry);
                  appender.arc_quadrant_to(x1, y1, x1 - rx, y1);
                  appender.line_to(x0 + rx, y1);
                  appender.arc_quadrant_to(x0, y1, x0, y1 - ry);
                  appender.line_to(x0, y0 + ry);
                  appender.arc_quadrant_to(x0, y0, x0 + rx, y0);
                }
                else {
                  appender.arc_quadrant_to(x0, y0, x0, y0 + ry);
                  appender.line_to(x0, y1 - ry);
                  appender.arc_quadrant_to(x0, y1, x0 + rx, y1);
                  appender.line_to(x1 - rx, y1);
                  appender.arc_quadrant_to(x1, y1, x1, y1 - ry);
                  appender.line_to(x1, y0 + ry);
                  appender.arc_quadrant_to(x1, y0, x1 - rx, y0);
                }
                appender.close();
                break;
              }

              case BL_GEOMETRY_TYPE_ARC_D:
              case BL_GEOMETRY_TYPE_CHORD_D:
              case BL_GEOMETRY_TYPE_PIE_D: {
                const BLPoint* vertices = arc_vertices[cw ? 0 : 1];
                if (geometry_type == BL_GEOMETRY_TYPE_PIE_D) {
                  appender.move_to(arc_center);
                  appender.line_to(vertices[0]);
                }
                else {
                  appender.move_to(vertices[0]);
                }

                appender.conic_to(vertices[1], vertices[2], arc_weights[0]);
                appender.conic_to(vertices[3], vertices[4], arc_weights[1]);
                if (geometry_type != BL_GEOMETRY_TYPE_ARC_D) {
                  appender.close();
                }
                break;
              }

              case BL_GEOMETRY_TYPE_LINE_D: {
                BLPoint p0(src_line_d.x0, src_line_d.y0);
                BLPoint p1(src_line_d.x1, src_line_d.y1);
                appender.move_to(cw ? p0 : p1);
                appender.line_to(cw ? p1 : p0);
                break;
              }

              case BL_GEOMETRY_TYPE_TRIANGLE_D: {
                BLPoint p0(src_triangle_d.x0, src_triangle_d.y0);
                BLPoint p2(src_triangle_d.x2, src_triangle_d.y2);
                appender.move_to(cw ? p0 : p2);
                appender.line_to(src_triangle_d.x1, src_triangle_d.y1);
                appender.line_to(cw ? p2 : p0);
                appender.close();
                break;
              }

              case BL_GEOMETRY_TYPE_POLYLINE_I: append_polygon(src_polygon_i, false); break;
              case BL_GEOMETRY_TYPE_POLYLINE_D: append_polygon(src_polygon_d, false); break;
              case BL_GEOMETRY_TYPE_POLYGON_I: append_polygon(src_polygon_i, true); break;
              case BL_GEOMETRY_TYPE_POLYGON_D: append_polygon(src_polygon_d, true); break;
              case BL_GEOMETRY_TYPE_SPAN_BOX_I: append_boxes(src_box_i_span); break;
              case BL_GEOMETRY_TYPE_SPAN_BOX_D: append_boxes(src_box_d_span); break;
              case BL_GEOMETRY_TYPE_SPAN_RECT_I: append_rects(src_rect_i_span); break;
              case BL_GEOMETRY_TYPE_SPAN_RECT_D: append_rects(src_rect_d_span); break;

              case BL_GEOMETRY_TYPE_PATH_D: {
                if (cw) {
                  appender.append_path(src_path.view());
                }
                else {
                  EXPECT_OK(appender.append_reversed_path(src_path.view()));
                }
                break;
              }
            }

            appender.done(&expected_geometry);

            if (t) {
              EXPECT_OK(expected_geometry.transform(*t));
            }

            // Only the appended geometry is transformed; existing vertices stay unchanged.
            BLPath expected;
            EXPECT_OK(appender.begin_append(&expected, path.byte_size()));
            if (dirty_path) {
              appender.move_to(initial_move);
              appender.line_to(initial_line);
            }
            appender.append_path(expected_geometry.view());
            appender.done(&expected);

            // Different code-paths; arc conversion can round the ideal quadrant coordinates by a few ULPs,
            // so don't compare exactly otherwise some tests would fail.
            EXPECT_TRUE(path_view_check_equality_ulp(path.view(), expected.view(), kPathMaxUlpDiff,
              "Path::add_geometry()"));
          }
        }
      }
    }
  }
}

TEST_CASE(core_path_add_geometry_invalid) {
  BLPath original = BLPath::make_from_segments(MoveTo{1, 2}, LineTo{3, 4}, Close{});
  BLPath path(original);
  EXPECT_EQ(path.add_geometry(static_cast<BLGeometryType>(0xFFFF), nullptr, nullptr), BL_ERROR_INVALID_VALUE);

  auto test_span = [&](BLGeometryType type, auto span) {
    // Empty spans are valid; oversized spans must fail before reading their data.
    for (size_t count : {size_t(0), SIZE_MAX, SIZE_MAX / sizeof(*span.data())}) {
      decltype(span) input{nullptr, count};
      EXPECT_EQ(path.add_geometry(type, &input, nullptr), count ? BL_ERROR_OUT_OF_MEMORY : BL_SUCCESS);
      EXPECT_TRUE(path_view_check_equality(path.view(), original.view(), "add_geometry: empty and invalid spans preserve destination"));
    }
  };
  test_span(BL_GEOMETRY_TYPE_SPAN_BOX_I, BLSpan<const BLBoxI>());
  test_span(BL_GEOMETRY_TYPE_SPAN_BOX_D, BLSpan<const BLBox>());
  test_span(BL_GEOMETRY_TYPE_SPAN_RECT_I, BLSpan<const BLRectI>());
  test_span(BL_GEOMETRY_TYPE_SPAN_RECT_D, BLSpan<const BLRect>());
}

TEST_CASE(core_path_close) {
  BLPath path;

  // It should be invalid to close an empty path as a path cannot start with Close.
  EXPECT_EQ(path.close(), BL_ERROR_INVALID_GEOMETRY);

  // It should be invalid to close path that is already closed.
  EXPECT_OK(path.move_to(1.0, 2.0));
  EXPECT_OK(path.line_to(3.0, 4.0));
  EXPECT_OK(path.close());
  EXPECT_EQ(path.close(), BL_ERROR_INVALID_GEOMETRY);
}

TEST_CASE(core_path_info) {
  struct Fixture {
    BLPath path;
    uint32_t flags;
    BLBox vertex_box;
    BLBox ink_box;
  };

  const Fixture fixtures[] = {
    {BLPath(), BL_PATH_FLAG_EMPTY, {}, {}},
    {BLPath::make_from_segments(Close{}), BL_PATH_FLAG_EMPTY, {}, {}},
    {BLPath::make_from_segments(MoveTo{2, 3}), 0, {2, 3, 2, 3}, {2, 3, 2, 3}},
    {BLPath::make_from_segments(MoveTo{0, 0}, LineTo{8, 0}, Close{}), 0, {0, 0, 8, 0}, {0, 0, 8, 0}},
    {BLPath::make_from_segments(MoveTo{0, 0}, QuadTo{4, 8, 8, 0}), BL_PATH_FLAG_QUADS, {0, 0, 8, 8}, {0, 0, 8, 4}},
    {BLPath::make_from_segments(MoveTo{0, 0}, ConicTo{3, 6, 6, 0, 0.5}), BL_PATH_FLAG_CONICS, {0, 0, 6, 6}, {0, 0, 6, 2}},
    {BLPath::make_from_segments(MoveTo{0, 0}, ConicTo{3, -6, 6, 0, 2}), BL_PATH_FLAG_CONICS, {0, -6, 6, 0}, {0, -4, 6, 0}},
    {BLPath::make_from_segments(MoveTo{0, 0}, CubicTo{0, 8, 8, 8, 8, 0}), BL_PATH_FLAG_CUBICS, {0, 0, 8, 8}, {0, 0, 8, 6}},
    {BLPath::make_from_segments(MoveTo{0, 0}, QuadTo{4, 8, 8, 0}, ConicTo{12, 6, 16, 0, 0.5},
               CubicTo{16, 8, 24, 8, 24, 0}, Close{}, MoveTo{30, 10}),
     BL_PATH_FLAG_MULTIPLE | BL_PATH_FLAG_QUADS | BL_PATH_FLAG_CONICS | BL_PATH_FLAG_CUBICS,
     {0, 0, 30, 10}, {0, 0, 30, 10}}
  };

  for (const Fixture& fixture : fixtures) {
    // Query twice to exercise both the initial calculation and cached information.
    for (uint32_t repeat = 0; repeat < 2; repeat++) {
      uint32_t flags = 0;
      BLBox box;

      EXPECT_OK(fixture.path.get_info_flags(&flags));
      EXPECT_EQ(flags, fixture.flags);

      EXPECT_OK(fixture.path.get_vertex_bounding_box(&box));
      EXPECT_EQ(box, fixture.vertex_box);

      EXPECT_OK(fixture.path.get_ink_bounding_box(&box));
      EXPECT_EQ(box, fixture.ink_box);
    }
  }

  for (const BLPath& path : {
    BLPath::make_from_segments(LineTo{1, 2}),
    BLPath::make_from_segments(QuadTo{1, 2, 3, 4}),
    BLPath::make_from_segments(ConicTo{1, 2, 3, 4, 0.5}),
    BLPath::make_from_segments(CubicTo{1, 2, 3, 4, 5, 6}),
    BLPath::make_from_segments(MoveTo{0, 0}, Close{}, LineTo{1, 2})
  }) {
    uint32_t flags = 0;
    BLBox box;

    EXPECT_EQ(path.get_info_flags(&flags), BL_ERROR_INVALID_GEOMETRY);
    EXPECT_EQ(flags, uint32_t(BL_PATH_FLAG_INVALID));
    EXPECT_EQ(path.get_vertex_bounding_box(&box), BL_ERROR_INVALID_GEOMETRY);
    EXPECT_EQ(box, BLBox());
    EXPECT_EQ(path.get_ink_bounding_box(&box), BL_ERROR_INVALID_GEOMETRY);
    EXPECT_EQ(box, BLBox());
  }
}

TEST_CASE(core_path_set_vertex_at) {
  const BLPoint vertices[] = {{1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}, {11, 12}, {13, 14}, {15, 16}, {17, 18}};
  auto build_path = [](const BLPoint* p) {
    return BLPath::make_from_segments(
      MoveTo{p[0].x, p[0].y}, LineTo{p[1].x, p[1].y}, QuadTo{p[2].x, p[2].y, p[3].x, p[3].y},
      CubicTo{p[4].x, p[4].y, p[5].x, p[5].y, p[6].x, p[6].y}, ConicTo{p[7].x, p[7].y, p[8].x, p[8].y, 0.5});
  };
  BLPath original = build_path(vertices);

  // Exercise every endpoint and control point through both overloads, with unique and shared storage.
  for (bool shared : {false, true}) {
    for (size_t index = 0; index < BL_ARRAY_SIZE(vertices); index++) {
      BLPath path = shared ? original : build_path(vertices);
      const BLPoint* vertex_data = path.vertex_data();
      EXPECT_EQ(vertex_data == original.vertex_data(), shared);

      BLBox box;
      EXPECT_OK(path.get_vertex_bounding_box(&box));

      BLPoint expected_vertices[BL_ARRAY_SIZE(vertices)];
      memcpy(expected_vertices, vertices, sizeof(vertices));
      expected_vertices[index] = BLPoint(-100.5, 200.5);
      if (index % 2) {
        EXPECT_OK(path.set_vertex_at(index, -100.5, 200.5));
      }
      else {
        EXPECT_OK(path.set_vertex_at(index, expected_vertices[index]));
      }
      BLPath expected = build_path(expected_vertices);
      EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(), "BLPath::set_vertex_at(): only the selected vertex changes"));
      EXPECT_TRUE(path_view_equals(original.view(), build_path(vertices).view()));
      EXPECT_EQ(path.vertex_data() == vertex_data, !shared);

      BLBox expected_box;
      EXPECT_OK(path.get_vertex_bounding_box(&box));
      EXPECT_OK(expected.get_vertex_bounding_box(&expected_box));
      EXPECT_EQ(box, expected_box);
    }
  }

  // Invalid indices preserve both unique and shared paths without detaching storage.
  for (bool shared : {false, true}) {
    BLPath path = shared ? original : build_path(vertices);
    const BLPoint* vertex_data = path.vertex_data();

    EXPECT_EQ(path.set_vertex_at(path.vertex_count(), 1, 2), BL_ERROR_INVALID_VALUE);
    EXPECT_EQ(path.set_vertex_at(SIZE_MAX, 1, 2), BL_ERROR_INVALID_VALUE);
    EXPECT_EQ(path.vertex_data(), vertex_data);
    EXPECT_TRUE(path_view_equals(path.view(), original.view()));
  }

  BLPath empty;
  EXPECT_EQ(empty.set_vertex_at(0, 1, 2), BL_ERROR_INVALID_VALUE);
  EXPECT_TRUE(empty.is_empty());
}

TEST_CASE(core_path_set_weight_at) {
  auto build_path = [](const double* w) {
    return BLPath::make_from_segments(
      MoveTo{0, 0}, ConicTo{10, 10, 20, 0, w[0]}, LineTo{25, 5}, QuadTo{26, 10, 28, 0},
      ConicTo{30, 10, 40, 0, w[1]}, CubicTo{41, 5, 42, 5, 43, 0}, ConicTo{50, 10, 60, 0, w[2]}
    );
  };
  const double weights[] = {0.3, 0.7, 0.5};
  // Control offsets are measured from the start of the reversed command stream.
  const size_t conic_offsets[] = {
    MoveTo::kControlSize,
    MoveTo::kControlSize + ConicTo::kControlSize + LineTo::kControlSize + QuadTo::kControlSize,
    MoveTo::kControlSize + ConicTo::kControlSize * 2 + LineTo::kControlSize + QuadTo::kControlSize + CubicTo::kControlSize
  };
  BLPath original = build_path(weights);

  // First, middle, and trailing conic weights must be changed selectively, including after COW.
  for (bool shared : {false, true}) {
    for (size_t index = 0; index < BL_ARRAY_SIZE(conic_offsets); index++) {
      BLPath path = shared ? original : build_path(weights);
      const BLPoint* vertex_data = path.vertex_data();
      EXPECT_EQ(vertex_data == original.vertex_data(), shared);

      BLBox box;
      EXPECT_OK(path.get_ink_bounding_box(&box));
      double expected_weights[] = {weights[0], weights[1], weights[2]};
      expected_weights[index] = 2.5;

      EXPECT_OK(path.set_weight_at(conic_offsets[index], expected_weights[index]));
      BLPath expected = build_path(expected_weights);
      EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(), "BLPath::set_weight_at(): only the selected conic weight changes"));

      BLBox expected_box;
      EXPECT_OK(path.get_ink_bounding_box(&box));
      EXPECT_OK(expected.get_ink_bounding_box(&expected_box));
      EXPECT_EQ(box, expected_box);
      EXPECT_TRUE(path_view_equals(original.view(), build_path(weights).view()));
      EXPECT_EQ(path.vertex_data() == vertex_data, !shared);
    }

    // Check every non-conic boundary, every byte within weights, and out-of-range offsets.
    BLPath path = shared ? original : build_path(weights);
    const BLPoint* vertex_data = path.vertex_data();
    for (size_t index = 0; index <= path.control_data_size(); index++) {
      if (index != conic_offsets[0] && index != conic_offsets[1] && index != conic_offsets[2]) {
        EXPECT_EQ(path.set_weight_at(index, 1.5), BL_ERROR_INVALID_VALUE);
      }
    }

    EXPECT_EQ(path.set_weight_at(SIZE_MAX, 1.5), BL_ERROR_INVALID_VALUE);
    EXPECT_EQ(path.set_weight_at(1000, 1.5), BL_ERROR_INVALID_VALUE);
    EXPECT_EQ(path.vertex_data(), vertex_data);
    EXPECT_TRUE(path_view_equals(path.view(), original.view()));
  }

  BLPath line_ended = BLPath::make_from_segments(MoveTo{0, 0}, ConicTo{10, 10, 20, 0, 0.3}, LineTo{30, 30});
  EXPECT_EQ(line_ended.set_weight_at(line_ended.control_data_size() - ConicTo::kControlSize, 1.5), BL_ERROR_INVALID_VALUE);
  EXPECT_TRUE(path_view_equals(line_ended.view(), BLPath::make_from_segments(MoveTo{0, 0}, ConicTo{10, 10, 20, 0, 0.3}, LineTo{30, 30}).view()));

  BLPath empty;
  EXPECT_EQ(empty.set_weight_at(0, 1.5), BL_ERROR_INVALID_VALUE);
  EXPECT_TRUE(empty.is_empty());
}

TEST_CASE(core_path_assign_deep) {
  auto build_path = []() {
    BLPath path = BLPath::make_from_segments(MoveTo{1, 2}, LineTo{3, 4}, QuadTo{5, 6, 7, 8}, CubicTo{9, 10, 11, 12, 13, 14});
    // Enough control data to exceed the allocation's alignment padding.
    for (size_t i = 0; i < 32; i++) {
      EXPECT_OK(path.add_segments(ConicTo{15, 16, 17, 18, 0.3}));
    }
    EXPECT_OK(path.close());
    return path;
  };

  BLPath expected = build_path();

  // Self-assignment of unique storage preserves its allocation and contents.
  {
    BLPath path = build_path();
    const BLPoint* vertex_data = path.vertex_data();
    EXPECT_OK(path.assign_deep(path));
    EXPECT_EQ(path.vertex_data(), vertex_data);
    EXPECT_TRUE(ObjectLayer::is_object_mutable(&path));
    EXPECT_TRUE(path_view_equals(path.view(), expected.view()));
  }

  // Both self-assignment and assignment from another owner must detach shared storage.
  for (bool self_assign : {false, true}) {
    BLPath path(expected);
    EXPECT_EQ(path.vertex_data(), expected.vertex_data());
    EXPECT_OK(path.assign_deep(self_assign ? path : expected));
    EXPECT_NE(path.vertex_data(), expected.vertex_data());
    EXPECT_TRUE(ObjectLayer::is_object_mutable(&path));
    EXPECT_GE(path.byte_capacity(), PathDetail::get_vertex_data_size(&path) + path.control_data_size());
    EXPECT_TRUE(path_view_equals(path.view(), expected.view()));

    EXPECT_OK(path.set_vertex_at(0, 99, 100));
    EXPECT_EQ(expected.vertex_data()[0], BLPoint(1, 2));
  }

  // Copy into a fresh destination, or reuse an existing mutable allocation and invalidate its cache.
  for (bool reuse : {false, true}) {
    BLPath path;
    const BLPoint* vertex_data = nullptr;
    if (reuse) {
      EXPECT_OK(path.reserve(4096));
      EXPECT_OK(path.add_box(-100, -200, -10, -20));
      BLBox box;
      EXPECT_OK(path.get_vertex_bounding_box(&box));
      vertex_data = path.vertex_data();
    }

    EXPECT_OK(path.assign_deep(expected));
    EXPECT_TRUE(ObjectLayer::is_object_mutable(&path));
    EXPECT_NE(path.vertex_data(), expected.vertex_data());
    EXPECT_TRUE(path_view_equals(path.view(), expected.view()));
    if (reuse) {
      EXPECT_EQ(path.vertex_data(), vertex_data);
    }

    BLBox box;
    EXPECT_OK(path.get_vertex_bounding_box(&box));
    EXPECT_EQ(box, BLBox(1, 2, 17, 18));
  }

  // Replacing a larger shared destination must preserve its other owner, even with an empty source.
  for (bool empty : {false, true}) {
    BLPath path(expected);
    BLPath source = empty ? BLPath() : BLPath::make_from_segments(MoveTo{42, 43});

    EXPECT_OK(path.assign_deep(source));
    EXPECT_TRUE(ObjectLayer::is_object_mutable(&path));
    EXPECT_TRUE(path_view_equals(path.view(), source.view()));
    EXPECT_TRUE(path_view_equals(expected.view(), build_path().view()));
  }

  // Empty self-assignment must also produce mutable storage without modifying the eternal empty impl.
  {
    BLPath path;
    BLPath empty;
    EXPECT_OK(path.assign_deep(path));
    EXPECT_TRUE(path.is_empty());
    EXPECT_TRUE(ObjectLayer::is_object_mutable(&path));
    EXPECT_NE(path.vertex_data(), empty.vertex_data());
    EXPECT_EQ(ObjectLayer::get_impl(&empty)->flags, uint32_t(BL_PATH_FLAG_EMPTY));
  }
}

// Use exactly representable coordinates so transformed paths can be compared byte-for-byte.
static void append_building_test_path(BLPath& path, const BLMatrix2D& m = BLMatrix2D::make_identity()) {
  auto p = [&](double x, double y) -> BLPoint {
    return BLPoint(x * m.m00 + y * m.m10 + m.m20, x * m.m01 + y * m.m11 + m.m21);
  };

  EXPECT_OK(path.move_to(p(1, 2)));
  EXPECT_OK(path.line_to(p(3, 4)));
  EXPECT_OK(path.quad_to(p(5, 6), p(7, 8)));
  EXPECT_OK(path.cubic_to(p(9, 10), p(11, 12), p(13, 14)));
  EXPECT_OK(path.conic_to(p(15, 16), p(17, 18), 0.25));
  EXPECT_OK(path.conic_to(p(19, 20), p(21, 22), 1.5));
  EXPECT_OK(path.close());
  EXPECT_OK(path.move_to(p(-1, -2)));

  // Enough consecutive simple commands to exercise the optimized reverse-copy loop and its remainder.
  for (int i = 0; i < 12; i++) {
    EXPECT_OK(path.line_to(p(30 + i, 40 + i)));
  }
}

static void append_reversed_building_test_path(BLPath& path, const BLMatrix2D& m = BLMatrix2D::make_identity()) {
  auto p = [&](double x, double y) -> BLPoint {
    return BLPoint(x * m.m00 + y * m.m10 + m.m20, x * m.m01 + y * m.m11 + m.m21);
  };

  EXPECT_OK(path.move_to(p(41, 51)));
  for (int i = 10; i >= 0; i--) {
    EXPECT_OK(path.line_to(p(30 + i, 40 + i)));
  }
  EXPECT_OK(path.line_to(p(-1, -2)));
  EXPECT_OK(path.move_to(p(21, 22)));
  EXPECT_OK(path.conic_to(p(19, 20), p(17, 18), 1.5));
  EXPECT_OK(path.conic_to(p(15, 16), p(13, 14), 0.25));
  EXPECT_OK(path.cubic_to(p(11, 12), p(9, 10), p(7, 8)));
  EXPECT_OK(path.quad_to(p(5, 6), p(3, 4)));
  EXPECT_OK(path.line_to(p(1, 2)));
  EXPECT_OK(path.close());
}

static void append_building_test_prefix(BLPath& path) {
  EXPECT_OK(path.add_segments(MoveTo{-100, -200}, ConicTo{-90, -80, -70, -60, 0.75}, Close{}));
}

template<typename AppendFn, typename ExpectedFn>
static void test_path_append(AppendFn append, ExpectedFn append_expected, const char* description) {
  BLPath src;
  append_building_test_path(src);
  BLPath source_original;
  append_building_test_path(source_original);

  for (bool grow : {false, true}) {
    // An independent source, the destination itself, or a second owner of the destination's storage.
    for (uint32_t alias_mode : {0u, 1u, 2u}) {
      BLPath path;
      if (alias_mode == 0) {
        append_building_test_prefix(path);
      }
      else {
        append_building_test_path(path);
      }
      if (grow) {
        EXPECT_OK(path.shrink());
        EXPECT_LT(path.byte_capacity() - path.byte_size(), src.byte_size());
      }
      else {
        EXPECT_OK(path.reserve(path.byte_size() + src.byte_size()));
      }

      BLBox box;
      EXPECT_OK(path.get_vertex_bounding_box(&box));
      BLPath original;
      EXPECT_OK(original.assign_deep(path));
      BLPath expected;
      EXPECT_OK(expected.assign_deep(path));
      append_expected(expected);

      BLPath shared;
      if (alias_mode == 2) {
        shared = path;
      }
      const BLPoint* vertex_data = path.vertex_data();
      const BLPath& source = alias_mode == 0 ? src : alias_mode == 1 ? path : shared;
      EXPECT_OK(append(path, source));
      EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(), description));
      EXPECT_TRUE(path_view_check_equality(src.view(), source_original.view(), "Appending a path preserves the source"));
      if (alias_mode == 2) {
        EXPECT_TRUE(path_view_check_equality(shared.view(), original.view(), "Appending a path preserves the shared owner"));
      }
      EXPECT_EQ(path.vertex_data() == vertex_data, !grow && alias_mode != 2);

      BLBox expected_box;
      EXPECT_OK(path.get_vertex_bounding_box(&box));
      EXPECT_OK(expected.get_vertex_bounding_box(&expected_box));
      EXPECT_EQ(box, expected_box);
    }
  }
}

TEST_CASE(core_path_add_path) {
  test_path_append(
    [](BLPath& dst, const BLPath& src) { return dst.add_path(src); },
    [](BLPath& expected) { append_building_test_path(expected); },
    "BLPath::add_path(): growth and aliasing");
}

TEST_CASE(core_path_add_transformed_path) {
  const BLMatrix2D transforms[] = {
    BLMatrix2D::make_identity(),
    BLMatrix2D::make_translation(100, -200),
    BLMatrix2D(2, 0, 0, -3, 100, -200),
    BLMatrix2D(0, 2, -3, 0, 100, -200),
    BLMatrix2D(2, 1, -1, 3, 100, -200)
  };

  for (const BLMatrix2D& m : transforms) {
    test_path_append(
      [&](BLPath& dst, const BLPath& src) { return dst.add_path(src, m); },
      [&](BLPath& expected) { append_building_test_path(expected, m); },
      "BLPath::add_path(matrix): transform only appended vertices");

    // Small inputs take the general affine mapping path even for simpler transform types.
    BLPath src = BLPath::make_from_segments(MoveTo{1, 2}, ConicTo{3, 4, 5, 6, 0.5});
    BLPath path;
    EXPECT_OK(path.add_path(src, m));
    BLPath expected = BLPath::make_from_segments(
      MoveTo{1 * m.m00 + 2 * m.m10 + m.m20, 1 * m.m01 + 2 * m.m11 + m.m21},
      ConicTo{3 * m.m00 + 4 * m.m10 + m.m20, 3 * m.m01 + 4 * m.m11 + m.m21,
              5 * m.m00 + 6 * m.m10 + m.m20, 5 * m.m01 + 6 * m.m11 + m.m21, 0.5});
    EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(), "BLPath::add_path(matrix): small source"));
  }
}

TEST_CASE(core_path_add_translated_path) {
  const BLPoint translation(100, -200);
  test_path_append(
    [&](BLPath& dst, const BLPath& src) { return dst.add_path(src, translation); },
    [&](BLPath& expected) { append_building_test_path(expected, BLMatrix2D::make_translation(translation)); },
    "BLPath::add_path(point): translated append and aliasing");
}

TEST_CASE(core_path_add_reversed_path) {
  const BLMatrix2D m(2, 1, -1, 3, 100, -200);
  test_path_append(
    [](BLPath& dst, const BLPath& src) { return dst.add_reversed_path(src); },
    [](BLPath& expected) { append_reversed_building_test_path(expected); },
    "BLPath::add_reversed_path(): mixed figures, weights, and aliasing");
  test_path_append(
    [&](BLPath& dst, const BLPath& src) { return dst.add_reversed_path(src, m); },
    [&](BLPath& expected) { append_reversed_building_test_path(expected, m); },
    "BLPath::add_reversed_path(matrix): mixed figures, weights, and aliasing");
}

TEST_CASE(core_path_add_reversed_path_small_figures) {
  for (bool closed : {false, true}) {
    for (bool transformed : {false, true}) {
      const BLMatrix2D m = transformed ? BLMatrix2D(2, 1, -1, 3, 100, -200) : BLMatrix2D::make_identity();
      auto p = [&](double x, double y) -> BLPoint {
        return BLPoint(x * m.m00 + y * m.m10 + m.m20, x * m.m01 + y * m.m11 + m.m21);
      };

      BLPath src = BLPath::make_from_segments(MoveTo{1, 2}, ConicTo{3, 4, 5, 6, 0.25});
      if (closed) {
        EXPECT_OK(src.close());
      }
      EXPECT_OK(src.move_to(10, 20));
      if (closed) {
        EXPECT_OK(src.close());
      }

      BLPath path;
      append_building_test_prefix(path);
      if (transformed) {
        EXPECT_OK(path.add_reversed_path(src, m));
      }
      else {
        EXPECT_OK(path.add_reversed_path(src));
      }

      BLPath expected;
      append_building_test_prefix(expected);
      EXPECT_OK(expected.move_to(p(10, 20)));
      if (closed) {
        EXPECT_OK(expected.close());
      }
      EXPECT_OK(expected.move_to(p(5, 6)));
      EXPECT_OK(expected.conic_to(p(3, 4), p(1, 2), 0.25));
      if (closed) {
        EXPECT_OK(expected.close());
      }
      EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(), "BLPath::add_reversed_path(): small source and move-only figure"));
    }
  }
}

TEST_CASE(core_path_add_empty_path) {
  auto test_append = [](auto append_fn, const BLPath& expected) {
    BLPath empty;
    EXPECT_OK(append_fn(empty, empty));
    EXPECT_TRUE(empty.is_empty());

    BLPath original;
    append_building_test_path(original);
    BLPath path(original);
    EXPECT_OK(append_fn(path, empty));
    BLPath unchanged;
    append_building_test_path(unchanged);
    EXPECT_TRUE(path_view_check_equality(path.view(), unchanged.view(), "Appending an empty path preserves the destination"));
    EXPECT_TRUE(path_view_check_equality(original.view(), unchanged.view(), "Appending an empty path preserves the shared owner"));

    BLPath fresh;
    EXPECT_OK(append_fn(fresh, original));
    EXPECT_TRUE(path_view_check_equality(fresh.view(), expected.view(), "Appending to an empty destination"));
  };

  BLPath expected;
  append_building_test_path(expected);
  test_append([](BLPath& dst, const BLPath& src) { return dst.add_path(src); }, expected);

  EXPECT_OK(expected.clear());
  append_building_test_path(expected, BLMatrix2D::make_translation(100, -200));
  test_append([](BLPath& dst, const BLPath& src) { return dst.add_path(src, BLPoint(100, -200)); }, expected);

  const BLMatrix2D m(2, 1, -1, 3, 100, -200);
  EXPECT_OK(expected.clear());
  append_building_test_path(expected, m);
  test_append([&](BLPath& dst, const BLPath& src) { return dst.add_path(src, m); }, expected);

  EXPECT_OK(expected.clear());
  append_reversed_building_test_path(expected);
  test_append([](BLPath& dst, const BLPath& src) { return dst.add_reversed_path(src); }, expected);

  EXPECT_OK(expected.clear());
  append_reversed_building_test_path(expected, m);
  test_append([&](BLPath& dst, const BLPath& src) { return dst.add_reversed_path(src, m); }, expected);
}

TEST_CASE(core_path_append_path) {
  BLPath expected = BLPath::make_from_segments(
    MoveTo{1, 2}, LineTo{3, 4}, MoveTo{1, 2}, QuadTo{5, 6, 7, 8}, CubicTo{9, 10, 11, 12, 13, 14},
    ConicTo{15, 16, 17, 18, 0.3}, Close{});
  EXPECT_OK(expected.add_box(10.0, 20.0, 100.0, 200.0));

  BLPath src = BLPath::make_from_segments(MoveTo{1, 2}, QuadTo{5, 6, 7, 8}, CubicTo{9, 10, 11, 12, 13, 14}, ConicTo{15, 16, 17, 18, 0.3}, Close{});
  EXPECT_OK(src.add_box(10.0, 20.0, 100.0, 200.0));

  BLPath path;
  PathAppender appender;
  EXPECT_OK(appender.begin_append(&path, expected.byte_size()));
  appender.move_to(1, 2);
  appender.line_to(3, 4);
  appender.append_path(src.view());
  appender.done(&path);
  EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(), "PathAppender::append_path()"));
}

TEST_CASE(core_path_appender_existing_path) {
  BLPath expected = BLPath::make_from_segments(MoveTo{1, 2}, ConicTo{3, 4, 5, 6, 0.25});

  for (bool shared : {false, true}) {
    BLPath path = BLPath::make_from_segments(MoveTo{1, 2}, ConicTo{3, 4, 5, 6, 0.25});
    EXPECT_OK(path.reserve(1024));

    BLBox box;
    EXPECT_OK(path.get_vertex_bounding_box(&box));
    EXPECT_EQ(box, BLBox(1, 2, 5, 6));

    BLPath shared_copy;
    if (shared) {
      shared_copy = path;
    }

    const BLPoint* vertex_data = path.vertex_data();
    PathAppender appender;
    EXPECT_TRUE(appender.is_empty());
    EXPECT_OK(appender.begin_append(&path, LineTo::kByteSize));
    EXPECT_FALSE(appender.is_empty());
    EXPECT_EQ(appender.prev_command(), BL_PATH_CMD_CONIC);
    EXPECT_EQ(appender.prev_vertex(), BLPoint(5, 6));
    appender.line_to(100, 200);
    appender.done(&path);
    EXPECT_TRUE(appender.is_empty());

    // Reuse the appender after done(), with a different command and control-data size.
    EXPECT_OK(appender.begin_append(&path, ConicTo::kByteSize + Close::kByteSize));
    appender.conic_to(50, 60, -10, -20, 0.75);
    appender.close();
    appender.done(&path);
    EXPECT_TRUE(appender.is_empty());

    BLPath expected_result = BLPath::make_from_segments(
      MoveTo{1, 2}, ConicTo{3, 4, 5, 6, 0.25}, LineTo{100, 200}, ConicTo{50, 60, -10, -20, 0.75}, Close{});
    EXPECT_TRUE(path_view_check_equality(path.view(), expected_result.view(),
      "PathAppender::begin_append(): preserve existing data and reuse after done"));

    EXPECT_OK(path.get_vertex_bounding_box(&box));
    EXPECT_EQ(box, BLBox(-10, -20, 100, 200));
    if (shared) {
      EXPECT_NE(path.vertex_data(), vertex_data);
      EXPECT_TRUE(path_view_check_equality(shared_copy.view(), expected.view(),
        "PathAppender::begin_append(): shared owner remains unchanged"));
      EXPECT_OK(shared_copy.get_vertex_bounding_box(&box));
      EXPECT_EQ(box, BLBox(1, 2, 5, 6));
    }
    else {
      EXPECT_EQ(path.vertex_data(), vertex_data);
    }
  }
}

TEST_CASE(core_path_appender_ensure) {
  BLPath path = BLPath::make_from_segments(MoveTo{1, 2}, LineTo{3, 4});

  PathAppender appender;
  EXPECT_OK(appender.begin_append(&path, ConicTo::kByteSize + Close::kByteSize));
  appender.conic_to(5, 6, 7, 8, 0.125);
  appender.close();

  // A request that fits must preserve the pending commands and current allocation.
  size_t capacity = path.byte_capacity();
  EXPECT_OK(appender.ensure(&path, appender.remaining_byte_size()));
  EXPECT_EQ(path.byte_capacity(), capacity);
  EXPECT_EQ(appender.prev_command(), BL_PATH_CMD_CLOSE);

  // Force growth after a weighted command and a command with no vertex.
  size_t grow_request = appender.remaining_byte_size() + MoveTo::kByteSize + CubicTo::kByteSize;
  EXPECT_OK(appender.ensure(&path, grow_request));
  EXPECT_GT(path.byte_capacity(), capacity);
  EXPECT_GE(appender.remaining_byte_size(), grow_request);
  EXPECT_EQ(appender.prev_command(), BL_PATH_CMD_CLOSE);
  EXPECT_EQ(appender.prev_vertex(), BLPoint(7, 8));

  appender.move_to(9, 10);
  appender.cubic_to(11, 12, 13, 14, 15, 16);
  appender.done(&path);
  EXPECT_TRUE(appender.is_empty());

  BLPath expected = BLPath::make_from_segments(
    MoveTo{1, 2}, LineTo{3, 4}, ConicTo{5, 6, 7, 8, 0.125}, Close{},
    MoveTo{9, 10}, CubicTo{11, 12, 13, 14, 15, 16});
  EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(),
    "PathAppender::ensure(): preserve pending vertices, controls, and conic weight during growth"));
}

TEST_CASE(core_path_join_path_boundaries) {
  for (bool closed : {false, true}) {
    for (bool coinciding : {false, true}) {
      for (bool single_move : {false, true}) {
        double x = coinciding ? 10.0 : 11.0;
        BLPath src = BLPath::make_from_segments(MoveTo{x, 20});
        if (!single_move) {
          EXPECT_OK(src.add_segments(ConicTo{30, 40, 50, 60, 0.75}, Close{}, MoveTo{70, 80}, LineTo{90, 100}));
        }

        BLPath path = BLPath::make_from_segments(MoveTo{0, 0}, LineTo{10, 20});
        if (closed) {
          EXPECT_OK(path.close());
        }

        PathAppender appender;
        EXPECT_OK(appender.begin_append(&path, src.byte_size()));
        appender.join_path(src.view());
        appender.done(&path);

        BLPath expected = BLPath::make_from_segments(MoveTo{0, 0}, LineTo{10, 20});
        if (closed) {
          EXPECT_OK(expected.add_segments(Close{}, MoveTo{x, 20}));
        }
        else if (!coinciding) {
          EXPECT_OK(expected.line_to(x, 20));
        }
        if (!single_move) {
          EXPECT_OK(expected.add_segments(ConicTo{30, 40, 50, 60, 0.75}, Close{}, MoveTo{70, 80}, LineTo{90, 100}));
        }

        EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(),
          "PathAppender::join_path(): coinciding endpoints, closed destination, and single-move source"));
      }
    }
  }
}

TEST_CASE(core_path_join_path_empty) {
  BLPath expected = BLPath::make_from_segments(MoveTo{1, 2}, LineTo{3, 4});

  BLPath path;
  PathAppender appender;
  EXPECT_OK(appender.begin_append(&path, expected.byte_size() + MoveTo::kByteSize));

  BLPath empty;
  appender.move_to(1, 2);
  appender.line_to(3, 4);
  appender.join_path(empty.view());
  appender.done(&path);

  EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(),
    "PathAppender::join_path(): empty path"));
}

TEST_CASE(core_path_join_path_open) {
  BLPath expected = BLPath::make_from_segments(
    MoveTo{1, 2}, LineTo{3, 4}, LineTo{100, 200}, QuadTo{5, 6, 7, 8},
    CubicTo{9, 10, 11, 12, 13, 14}, ConicTo{15, 16, 17, 18, 0.3}, Close{}
  );
  EXPECT_OK(expected.add_box(10.0, 20.0, 100.0, 200.0));

  BLPath src = BLPath::make_from_segments(
    MoveTo{100, 200}, QuadTo{5, 6, 7, 8}, CubicTo{9, 10, 11, 12, 13, 14},
    ConicTo{15, 16, 17, 18, 0.3}, Close{}
  );
  EXPECT_OK(src.add_box(10.0, 20.0, 100.0, 200.0));

  BLPath path;
  PathAppender appender;

  EXPECT_OK(appender.begin_append(&path, expected.byte_size()));
  appender.move_to(1, 2);
  appender.line_to(3, 4);
  appender.join_path(src.view());
  appender.done(&path);

  EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(),
    "PathAppender::join_path(): open destination and multiple mixed-segment figures"));
}

TEST_CASE(core_path_join_reversed_path_empty) {
  BLPath src;
  BLPath expected = BLPath::make_from_segments(MoveTo{0, 1}, LineTo{2, 3});
  BLPath dst;

  PathAppender appender;
  EXPECT_OK(appender.begin_append(&dst, expected.byte_size() + MoveTo::kByteSize));

  appender.move_to(0, 1);
  appender.line_to(2, 3);
  EXPECT_OK(appender.join_reversed_path(src.view()));
  appender.done(&dst);

  EXPECT_TRUE(path_view_check_equality(dst.view(), expected.view(),
    "PathAppender::join_reversed_path(): empty path should not append anything"));
}

TEST_CASE(core_path_join_reversed_path_open) {
  for (bool coinciding : {false, true}) {
    for (bool single_move : {false, true}) {
      BLPath src = single_move ? BLPath::make_from_segments(MoveTo{80, 0}) : BLPath::make_from_segments(
        MoveTo{0, 0}, LineTo{10, 0}, QuadTo{20, 10, 30, 0},
        CubicTo{40, 10, 50, 10, 60, 0}, ConicTo{70, 10, 80, 0, 0.5});
      double x = coinciding ? 80.0 : 90.0;
      double y = coinciding ? 0.0 : 90.0;
      BLPath expected = BLPath::make_from_segments(MoveTo{100, 100}, LineTo{x, y});
      if (!coinciding) {
        EXPECT_OK(expected.add_segments(LineTo{80, 0}));
      }
      if (!single_move) {
        EXPECT_OK(expected.add_segments(
          ConicTo{70, 10, 60, 0, 0.5}, CubicTo{50, 10, 40, 10, 30, 0},
          QuadTo{20, 10, 10, 0}, LineTo{0, 0}));
      }

      BLPath dst;
      PathAppender appender;
      EXPECT_OK(appender.begin_append(&dst,
        MoveTo::kByteSize + LineTo::kByteSize + src.byte_size() + MoveTo::kByteSize));
      appender.move_to(100, 100);
      appender.line_to(x, y);
      EXPECT_OK(appender.join_reversed_path(src.view()));
      appender.done(&dst);

      EXPECT_TRUE(path_view_check_equality(dst.view(), expected.view(),
        "PathAppender::join_reversed_path(): open destination, coinciding endpoints, and single-move source"));
    }
  }
}

TEST_CASE(core_path_join_reversed_path_closed) {
  for (bool dst_closed : {false, true}) {
    for (bool src_closed : {false, true}) {
      BLPath src = src_closed ? BLPath::make_from_segments(MoveTo{10, 0}, LineTo{20, 0}, CubicTo{30, 10, 40, 10, 50, 0}, Close{})
                              : BLPath::make_from_segments(MoveTo{10, 0}, LineTo{20, 0}, LineTo{20, 10});
      BLPoint first = src_closed ? BLPoint(50, 0) : BLPoint(20, 10);
      BLPath expected = BLPath::make_from_segments(MoveTo{0, 0}, LineTo{5, 5});

      if (dst_closed) {
        EXPECT_OK(expected.add_segments(Close{}, MoveTo{first.x, first.y}));
      }
      else {
        EXPECT_OK(expected.add_segments(LineTo{first.x, first.y}));
      }
      if (src_closed) {
        EXPECT_OK(expected.add_segments(CubicTo{40, 10, 30, 10, 20, 0}, LineTo{10, 0}, Close{}));
      }
      else {
        EXPECT_OK(expected.add_segments(LineTo{20, 0}, LineTo{10, 0}));
      }

      BLPath dst;
      PathAppender appender;

      EXPECT_OK(appender.begin_append(&dst,
        MoveTo::kByteSize + LineTo::kByteSize + Close::kByteSize + src.byte_size() + MoveTo::kByteSize));
      appender.move_to(0, 0);
      appender.line_to(5, 5);
      if (dst_closed) {
        appender.close();
      }
      EXPECT_OK(appender.join_reversed_path(src.view()));
      appender.done(&dst);

      EXPECT_TRUE(path_view_check_equality(dst.view(), expected.view(),
        "PathAppender::join_reversed_path(): open and closed source and destination combinations"));
    }
  }
}

TEST_CASE(core_path_add_vertex_outline_of_path) {
  auto build_source = [](BLPath& path) {
    append_building_test_path(path);
    // Adjacent moves include an open figure and a move-only figure before the final figure.
    EXPECT_OK(path.add_segments(MoveTo{50, 60}, MoveTo{70, 80}, LineTo{90, 100}));
  };

  BLPath src;
  BLPath original;

  build_source(src);
  build_source(original);

  // Every control point and endpoint becomes a line vertex, preserving moves and closes.
  const BLPoint first_figure[] = {
    {1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}, {11, 12},
    {13, 14}, {15, 16}, {17, 18}, {19, 20}, {21, 22}
  };

  BLPath outline;
  EXPECT_OK(outline.add_polyline(first_figure, BL_ARRAY_SIZE(first_figure)));
  EXPECT_OK(outline.close());
  EXPECT_OK(outline.move_to(-1, -2));

  for (int i = 0; i < 12; i++) {
    EXPECT_OK(outline.line_to(30 + i, 40 + i));
  }
  EXPECT_OK(outline.add_segments(MoveTo{50, 60}, MoveTo{70, 80}, LineTo{90, 100}));

  for (bool grow : {false, true}) {
    // Empty destination, an existing prefix, self-aliasing, or a second owner of the same storage.
    for (uint32_t mode : {0u, 1u, 2u, 3u}) {
      BLPath path;
      BLPath expected;

      if (mode == 1) {
        append_building_test_prefix(path);
        append_building_test_prefix(expected);
      }
      else if (mode >= 2) {
        build_source(path);
        build_source(expected);
      }
      EXPECT_OK(expected.add_path(outline));

      if (grow) {
        EXPECT_OK(path.shrink());
        EXPECT_LT(path.byte_capacity() - path.byte_size(), outline.byte_size());
      }
      else {
        EXPECT_OK(path.reserve(4096));
      }

      BLBox box;
      EXPECT_OK(path.get_vertex_bounding_box(&box));

      BLPath shared;
      if (mode == 3) {
        shared = path;
      }

      const BLPoint* vertex_data = path.vertex_data();
      if (mode == 2) {
        EXPECT_OK(path.add_vertex_outline_of_path(path));
      }
      else {
        EXPECT_OK(path.add_vertex_outline_of_path(mode == 3 ? shared : src));
      }

      EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(),
        "BLPath::add_vertex_outline_of_path(): mixed figures, growth, and aliasing"));
      EXPECT_TRUE(path_view_check_equality(src.view(), original.view(),
        "BLPath::add_vertex_outline_of_path(): source unchanged"));

      if (mode == 3) {
        EXPECT_TRUE(path_view_check_equality(shared.view(), original.view(),
          "BLPath::add_vertex_outline_of_path(): shared owner unchanged"));
      }

      if (grow || mode == 3) {
        EXPECT_NE(path.vertex_data(), vertex_data);
      }
      else {
        EXPECT_EQ(path.vertex_data(), vertex_data);
      }

      BLBox expected_box;
      EXPECT_OK(expected.get_vertex_bounding_box(&expected_box));
      EXPECT_OK(path.get_vertex_bounding_box(&box));
      EXPECT_EQ(box, expected_box);
    }
  }
}

TEST_CASE(core_path_add_vertex_outline_of_path_empty_and_invalid) {
  BLPath empty;
  EXPECT_OK(empty.add_vertex_outline_of_path(empty));
  EXPECT_TRUE(empty.is_empty());

  BLPath path;
  BLPath expected;
  append_building_test_prefix(path);
  append_building_test_prefix(expected);
  EXPECT_OK(path.add_vertex_outline_of_path(empty));
  EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(),
    "BLPath::add_vertex_outline_of_path(): empty source preserves the destination"));

  BLPath invalid = BLPath::make_from_segments(LineTo{1, 2});
  EXPECT_EQ(path.add_vertex_outline_of_path(invalid), BL_ERROR_CORRUPTED_PATH_DATA);
  EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(),
    "BLPath::add_vertex_outline_of_path(): missing initial move leaves the destination unchanged"));
}

TEST_CASE(core_path_add_aliased_data) {
  enum class Operation { kPolyTo, kPolyline, kPolygon };
  const BLPoint points[] = {{1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}, {11, 12}, {13, 14}};

  BLPath original = BLPath::make_from_segments(
    MoveTo{1, 2}, LineTo{3, 4}, LineTo{5, 6}, LineTo{7, 8}, LineTo{9, 10}, LineTo{11, 12}, LineTo{13, 14});

  for (Operation operation : {Operation::kPolyTo, Operation::kPolyline, Operation::kPolygon}) {
    for (bool grow : {false, true}) {
      for (bool shared : {false, true}) {
        for (size_t offset : {size_t(0), size_t(1), size_t(6)}) {
          BLPath path;
          EXPECT_OK(path.add_polyline(points, BL_ARRAY_SIZE(points)));
          if (grow) {
            EXPECT_OK(path.shrink());
          }
          else {
            EXPECT_OK(path.reserve(1024));
          }

          BLPath shared_copy;
          if (shared) {
            shared_copy = path;
          }

          size_t count = BL_ARRAY_SIZE(points) - offset;
          const BLPoint* data = path.vertex_data() + offset;

          switch (operation) {
            case Operation::kPolyTo: EXPECT_OK(path.poly_to(data, count)); break;
            case Operation::kPolyline: EXPECT_OK(path.add_polyline(data, count)); break;
            case Operation::kPolygon: EXPECT_OK(path.add_polygon(data, count)); break;
          }

          BLPath expected = original;
          size_t first_line = offset;

          if (operation != Operation::kPolyTo) {
            EXPECT_OK(expected.move_to(points[first_line++]));
          }

          for (size_t i = first_line; i < BL_ARRAY_SIZE(points); i++) {
            EXPECT_OK(expected.line_to(points[i]));
          }

          if (operation == Operation::kPolygon) {
            EXPECT_OK(expected.close());
          }

          EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(), "Aliased vertex data"));
          if (shared) {
            EXPECT_TRUE(path_view_check_equality(shared_copy.view(), original.view(), "Aliased vertex data: shared owner unchanged"));
          }
        }
      }
    }
  }
}

TEST_CASE(core_path_translate) {
  BLPath original = BLPath::make_from_segments(
    MoveTo{1, 2}, LineTo{3, 4}, QuadTo{5, 6, 7, 8}, ConicTo{9, 10, 11, 12, 0.25},
    CubicTo{13, 14, 15, 16, 17, 18}, Close{}, MoveTo{-2, -3}, LineTo{-4, -5}
  );
  // The outer rectangle gives exact ink bounds while the inner figures cover every segment kind.
  EXPECT_OK(original.add_box(-20, -30, 40, 50));

  BLPath translated = BLPath::make_from_segments(
    MoveTo{101, -198}, LineTo{103, -196}, QuadTo{105, -194, 107, -192}, ConicTo{109, -190, 111, -188, 0.25},
    CubicTo{113, -186, 115, -184, 117, -182}, Close{}, MoveTo{98, -203}, LineTo{96, -205}
  );
  EXPECT_OK(translated.add_box(80, -230, 140, -150));

  for (bool zero_translation : {false, true}) {
    for (bool shared : {false, true}) {
      BLPath path;
      EXPECT_OK(path.assign_deep(original));

      BLBox vertex_box;
      BLBox ink_box;

      EXPECT_OK(path.get_vertex_bounding_box(&vertex_box));
      EXPECT_OK(path.get_ink_bounding_box(&ink_box));
      EXPECT_EQ(vertex_box, BLBox(-20, -30, 40, 50));
      EXPECT_EQ(ink_box, BLBox(-20, -30, 40, 50));

      BLPath shared_owner;
      if (shared) {
        shared_owner = path;
      }

      const BLPoint* vertex_data = path.vertex_data();
      EXPECT_OK(path.translate(zero_translation ? BLPoint(0, 0) : BLPoint(100, -200)));
      EXPECT_TRUE(path_view_check_equality(path.view(), zero_translation ? original.view() : translated.view(),
        "BLPath::translate(): fixed coordinates and unchanged commands and conic weight"));

      EXPECT_OK(path.get_vertex_bounding_box(&vertex_box));
      EXPECT_OK(path.get_ink_bounding_box(&ink_box));
      BLBox expected_box = zero_translation ? BLBox(-20, -30, 40, 50) : BLBox(80, -230, 140, -150);
      EXPECT_EQ(vertex_box, expected_box);
      EXPECT_EQ(ink_box, expected_box);

      if (shared) {
        EXPECT_TRUE(path_view_check_equality(shared_owner.view(), original.view(),
          "BLPath::translate(): shared owner remains unchanged"));
        EXPECT_OK(shared_owner.get_vertex_bounding_box(&vertex_box));
        EXPECT_OK(shared_owner.get_ink_bounding_box(&ink_box));
        EXPECT_EQ(vertex_box, BLBox(-20, -30, 40, 50));
        EXPECT_EQ(ink_box, BLBox(-20, -30, 40, 50));
        if (!zero_translation) {
          EXPECT_NE(path.vertex_data(), vertex_data);
        }
      }
      else {
        EXPECT_EQ(path.vertex_data(), vertex_data);
      }
    }
  }
}

TEST_CASE(core_path_transform_empty) {
  auto test_empty = [](auto transform_fn) {
    for (uint32_t storage : {0u, 1u, 2u, 3u}) {
      BLPath path;
      BLPath shared_owner;

      if (storage == 1) {
        EXPECT_OK(path.reserve(256));
      }
      else if (storage >= 2) {
        EXPECT_OK(path.add_box(10, 20, 30, 40));
        if (storage == 2) {
          // Keep an allocation shared by two empty paths.
          EXPECT_OK(path.clear());
          shared_owner = path;
        }
        else {
          // Clear one owner of populated storage while the other still owns its shape.
          shared_owner = path;
          EXPECT_OK(path.clear());
        }
      }

      BLBox box;
      EXPECT_OK(path.get_vertex_bounding_box(&box));
      EXPECT_EQ(box, BLBox(0, 0, 0, 0));
      EXPECT_OK(path.get_ink_bounding_box(&box));
      EXPECT_EQ(box, BLBox(0, 0, 0, 0));

      const BLPoint* vertex_data = path.vertex_data();
      size_t capacity = path.byte_capacity();

      EXPECT_OK(transform_fn(path));
      EXPECT_TRUE(path.is_empty());
      EXPECT_EQ(path.vertex_count(), size_t(0));
      EXPECT_EQ(path.control_data_size(), size_t(0));
      EXPECT_EQ(path.byte_capacity(), capacity);
      EXPECT_EQ(path.vertex_data(), vertex_data);
      EXPECT_OK(path.get_vertex_bounding_box(&box));
      EXPECT_EQ(box, BLBox(0, 0, 0, 0));
      EXPECT_OK(path.get_ink_bounding_box(&box));
      EXPECT_EQ(box, BLBox(0, 0, 0, 0));

      BLPath expected_owner;
      if (storage == 3) {
        EXPECT_OK(expected_owner.add_box(10, 20, 30, 40));
      }

      EXPECT_TRUE(path_view_check_equality(shared_owner.view(), expected_owner.view(),
        "Transforming an empty path preserves its other owner"));
    }
  };

  test_empty([](BLPath& path) { return path.translate(BLPoint(100, -200)); });
  test_empty([](BLPath& path) { return path.transform(BLMatrix2D(2, 1, -1, 3, 100, -200)); });
}

TEST_CASE(core_path_transform) {
  struct Fixture {
    BLMatrix2D matrix;
    BLPoint vertices[17];
  };

  // Fixed coordinates for identity, translation, reflection/scaling, a quarter turn, and an affine transform.
  const Fixture fixtures[] = {
    {BLMatrix2D::make_identity(), {
      {  1,   2}, {  3,   5}, { -2,   4}, {  7,  -1},
      {  9,   3}, {  4,   8}, { -3,  -5}, {  6,  -2},
      {  8,   7}, { 10,  12}, { 11,  14}, { 13,  16},
      { 15,  18}, { 17,  20}, { 19,  22}, { 21,  24},
      { 23,  26}
    }},
    {BLMatrix2D::make_translation(10, -20), {
      { 11, -18}, { 13, -15}, {  8, -16}, { 17, -21},
      { 19, -17}, { 14, -12}, {  7, -25}, { 16, -22},
      { 18, -13}, { 20,  -8}, { 21,  -6}, { 23,  -4},
      { 25,  -2}, { 27,   0}, { 29,   2}, { 31,   4},
      { 33,   6}
    }},
    {BLMatrix2D(-2, 0, 0, 3, 4, -6), {
      {  2,   0}, { -2,   9}, {  8,   6}, {-10,  -9},
      {-14,   3}, { -4,  18}, { 10, -21}, { -8, -12},
      {-12,  15}, {-16,  30}, {-18,  36}, {-22,  42},
      {-26,  48}, {-30,  54}, {-34,  60}, {-38,  66},
      {-42,  72}
    }},
    {BLMatrix2D(0, 1, -1, 0, 10, -20), {
      {  8, -19}, {  5, -17}, {  6, -22}, { 11, -13},
      {  7, -11}, {  2, -16}, { 15, -23}, { 12, -14},
      {  3, -12}, { -2, -10}, { -4,  -9}, { -6,  -7},
      { -8,  -5}, {-10,  -3}, {-12,  -1}, {-14,   1},
      {-16,   3}
    }},
    {BLMatrix2D(2, 1, -1, 3, 10, -20), {
      { 10, -13}, { 11,  -2}, {  2, -10}, { 25, -16},
      { 25,  -2}, { 10,   8}, {  9, -38}, { 24, -20},
      { 19,   9}, { 18,  26}, { 18,  33}, { 20,  41},
      { 22,  49}, { 24,  57}, { 26,  65}, { 28,  73},
      { 30,  81}
    }}
  };

  auto make_fixture_path = [](const BLPoint* vertices, size_t count) {
    BLPath path;
    EXPECT_OK(path.move_to(vertices[0]));
    EXPECT_OK(path.line_to(vertices[1]));
    EXPECT_OK(path.quad_to(vertices[2], vertices[3]));
    EXPECT_OK(path.conic_to(vertices[4], vertices[5], 0.25));
    EXPECT_OK(path.cubic_to(vertices[6], vertices[7], vertices[8]));
    EXPECT_OK(path.close());

    if (count > 9) {
      EXPECT_OK(path.move_to(vertices[9]));
      EXPECT_OK(path.poly_to(vertices + 10, count - 10));
    }

    return path;
  };

  // Cover small inputs and both sides of the transform-type dispatch threshold.
  for (size_t count : {size_t(9), size_t(16), size_t(17)}) {
    BLPath original = make_fixture_path(fixtures[0].vertices, count);
    BLBox original_vertex_box;
    BLBox original_ink_box;
    EXPECT_OK(original.get_vertex_bounding_box(&original_vertex_box));
    EXPECT_OK(original.get_ink_bounding_box(&original_ink_box));

    for (const Fixture& fixture : fixtures) {
      BLPath expected = make_fixture_path(fixture.vertices, count);
      BLBox expected_vertex_box;
      BLBox expected_ink_box;
      EXPECT_OK(expected.get_vertex_bounding_box(&expected_vertex_box));
      EXPECT_OK(expected.get_ink_bounding_box(&expected_ink_box));

      for (bool shared : {false, true}) {
        BLPath path = make_fixture_path(fixtures[0].vertices, count);
        BLBox box;
        EXPECT_OK(path.get_vertex_bounding_box(&box));
        EXPECT_OK(path.get_ink_bounding_box(&box));

        BLPath owner;
        if (shared) {
          owner = path;
        }

        const BLPoint* vertex_data = path.vertex_data();
        EXPECT_OK(path.transform(fixture.matrix));

        // Full equality also verifies command order and the unchanged conic weight.
        EXPECT_TRUE(path_view_check_equality(path.view(), expected.view(),
          "BLPath::transform(): fixed coordinates for all segment types"));
        EXPECT_OK(path.get_vertex_bounding_box(&box));
        EXPECT_EQ(box, expected_vertex_box);
        EXPECT_OK(path.get_ink_bounding_box(&box));
        EXPECT_EQ(box, expected_ink_box);

        if (shared) {
          EXPECT_TRUE(path_view_check_equality(owner.view(), original.view(),
            "BLPath::transform(): shared owner unchanged"));
          EXPECT_OK(owner.get_vertex_bounding_box(&box));
          EXPECT_EQ(box, original_vertex_box);
          EXPECT_OK(owner.get_ink_bounding_box(&box));
          EXPECT_EQ(box, original_ink_box);
        }
        else {
          EXPECT_EQ(path.vertex_data(), vertex_data);
        }
      }
    }
  }
}

TEST_CASE(core_path_equals) {
  auto check_equality = [](const BLPath& a, const BLPath& b, bool expected) {
    EXPECT_EQ(a.equals(b), expected);
    EXPECT_EQ(b.equals(a), expected);
    EXPECT_EQ(a == b, expected);
    EXPECT_EQ(a != b, !expected);
  };

  // Equal contents in independently built paths, regardless of capacity or cached information.
  {
    BLPath a;
    BLPath b;
    check_equality(a, b, true);

    EXPECT_OK(b.reserve(4096));
    check_equality(a, b, true);

    append_building_test_path(a);
    check_equality(a, b, false);

    append_building_test_path(b);
    EXPECT_NE(a.vertex_data(), b.vertex_data());
    EXPECT_NE(a.byte_capacity(), b.byte_capacity());
    check_equality(a, b, true);
    check_equality(a, a, true);
    BLPath shared(a);
    check_equality(a, shared, true);

    BLBox box;
    EXPECT_OK(a.get_vertex_bounding_box(&box));
    EXPECT_OK(a.get_ink_bounding_box(&box));
    check_equality(a, b, true);

    EXPECT_OK(a.clear());
    EXPECT_OK(b.clear());
    check_equality(a, b, true);
  }

  // A vertex difference at either end must be detected without changing either stream's size.
  {
    BLPath a;
    BLPath b;

    append_building_test_path(a);
    append_building_test_path(b);

    for (size_t index : {size_t(0), a.vertex_count() - 1}) {
      EXPECT_OK(b.set_vertex_at(index, 99, 100));
      check_equality(a, b, false);
      EXPECT_OK(b.set_vertex_at(index, a.vertex_data()[index]));
      check_equality(a, b, true);
    }

    // First differ only in control size, then only in vertex size.
    EXPECT_OK(b.close());
    check_equality(a, b, false);
    EXPECT_OK(a.line_to(100, 200));
    check_equality(a, b, false);
  }

  // Identical vertices and stream sizes, but a different command.
  {
    BLPath a;
    BLPath b;

    append_building_test_path(a);
    append_building_test_path(b);
    EXPECT_OK(a.move_to(100, 200));
    EXPECT_OK(b.line_to(100, 200));
    check_equality(a, b, false);
  }

  // Conic weights are part of equality, even when all commands and vertices match.
  {
    BLPath a;
    BLPath b;

    append_building_test_prefix(a);
    append_building_test_prefix(b);
    EXPECT_OK(b.set_weight_at(BLPath::MoveTo::kControlSize, 0.5));
    check_equality(a, b, false);
  }

  // Equality is byte-exact, including the sign of zero.
  {
    BLPath a = BLPath::make_from_segments(MoveTo{0.0, 1.0});
    BLPath b = BLPath::make_from_segments(MoveTo{-0.0, 1.0});
    check_equality(a, b, false);
  }
}

TEST_CASE(core_path_get_last_vertex) {
  auto check_last = [](const BLPath& path, const BLPoint& expected) {
    BLPoint last(99, 100);
    EXPECT_OK(path.get_last_vertex(&last));
    EXPECT_EQ(last, expected);
  };

  struct Fixture {
    BLPath path;
    BLPoint last;
  };
  const Fixture fixtures[] = {
    {BLPath::make_from_segments(MoveTo{1, 2}), BLPoint(1, 2)},
    {BLPath::make_from_segments(MoveTo{1, 2}, LineTo{3, 4}), BLPoint(3, 4)},
    {BLPath::make_from_segments(MoveTo{1, 2}, QuadTo{5, 6, 7, 8}), BLPoint(7, 8)},
    {BLPath::make_from_segments(MoveTo{1, 2}, ConicTo{9, 10, 11, 12, 0.5}), BLPoint(11, 12)},
    {BLPath::make_from_segments(MoveTo{1, 2}, LineTo{3, 4}, QuadTo{5, 6, 7, 8}, ConicTo{9, 10, 11, 12, 0.5},
               CubicTo{13, 14, 15, 16, 17, 18}), BLPoint(17, 18)}
  };

  // Every segment kind returns its endpoint when open and the figure's start when closed.
  for (const Fixture& fixture : fixtures) {
    BLPath path(fixture.path);
    check_last(path, fixture.last);
    EXPECT_OK(path.close());
    check_last(path, BLPoint(1, 2));
    EXPECT_OK(path.move_to(23, 24));
    check_last(path, BLPoint(23, 24));
    EXPECT_OK(path.close());
    check_last(path, BLPoint(23, 24));
  }

  // Multiple figures: only the final figure determines the returned vertex.
  BLPath built;
  append_building_test_path(built);
  check_last(built, BLPoint(41, 51));
  EXPECT_OK(built.close());
  check_last(built, BLPoint(-1, -2));
  EXPECT_OK(built.move_to(50, 60));
  check_last(built, BLPoint(50, 60));
}

TEST_CASE(core_path_get_last_vertex_invalid) {
  // Missing MOVE commands and consecutive CLOSE commands cannot identify a figure's start.
  for (const BLPath& path : {
    BLPath{},
    BLPath::make_from_segments(Close{}),
    BLPath::make_from_segments(LineTo{1, 2}, Close{}),
    BLPath::make_from_segments(ConicTo{1, 2, 3, 4, 0.5}, Close{}),
    BLPath::make_from_segments(MoveTo{1, 2}, Close{}, Close{})
  }) {
    BLPoint last(99, 100);
    EXPECT_EQ(path.get_last_vertex(&last), BL_ERROR_NO_MATCHING_VERTEX);
    EXPECT_EQ(last, BLPoint(0, 0));
  }
}

TEST_CASE(core_path_get_closest_vertex) {
  BLPath path = BLPath::make_from_segments(
    MoveTo{0, 0}, LineTo{10, 0}, QuadTo{20, 10, 30, 0}, ConicTo{40, 10, 50, 0, 0.5}, CubicTo{60, 10, 70, 10, 80, 0}, Close{},
    MoveTo{-10, -20}, LineTo{-30, -40}, Close{}
  );

  const BLPoint vertices[] = {
    {0, 0}, {10, 0}, {20, 10}, {30, 0}, {40, 10}, {50, 0},
    {60, 10}, {70, 10}, {80, 0}, {-10, -20}, {-30, -40}
  };
  EXPECT_EQ(path.vertex_count(), BL_ARRAY_SIZE(vertices));
  for (size_t i = 0; i < BL_ARRAY_SIZE(vertices); i++) {
    size_t index = SIZE_MAX;
    double distance = -1.0;
    EXPECT_OK(path.get_closest_vertex(vertices[i], 0.0, &index, &distance));
    EXPECT_EQ(index, i);
    EXPECT_EQ(distance, 0.0);
  }

  // Vertex indices include curve controls, with no entries for close commands or conic weights.
  size_t index = SIZE_MAX;
  EXPECT_OK(path.get_closest_vertex(BLPoint(70, 10), 0.0, &index));
  EXPECT_EQ(index, size_t(7));
}

TEST_CASE(core_path_get_closest_vertex_distance) {
  BLPath path = BLPath::make_from_segments(MoveTo{0, 0}, LineTo{6, 8}, LineTo{100, 0});
  struct Fixture {
    BLPoint point;
    double max_distance;
    BLResult result;
    size_t index;
  };
  const Fixture fixtures[] = {
    // Nonpositive and infinite limits are unbounded; the first vertex wins equal-distance ties.
    {BLPoint(3, 4), 0.0, BL_SUCCESS, 0},
    {BLPoint(3, 4), -1.0, BL_SUCCESS, 0},
    {BLPoint(3, 4), Math::inf<double>(), BL_SUCCESS, 0},
    {BLPoint(3, 4), 6.0, BL_SUCCESS, 0},
    {BLPoint(97, 4), 6.0, BL_SUCCESS, 2},
    // The limit is exclusive. A scan miss succeeds; bounds rejection reports no matching vertex.
    {BLPoint(3, 4), 4.0, BL_SUCCESS, SIZE_MAX},
    {BLPoint(3, 4), 5.0, BL_SUCCESS, SIZE_MAX},
    {BLPoint(110, 0), 5.0, BL_ERROR_NO_MATCHING_VERTEX, SIZE_MAX}
  };
  for (const Fixture& fixture : fixtures) {
    size_t index = 123;
    double distance = -1.0;
    EXPECT_EQ(path.get_closest_vertex(fixture.point, fixture.max_distance, &index, &distance), fixture.result);
    EXPECT_EQ(index, fixture.index);
    if (fixture.index == SIZE_MAX) {
      EXPECT_TRUE(Math::is_nan(distance));
    }
    else {
      EXPECT_EQ(distance, 5.0);
    }
  }
}

TEST_CASE(core_path_get_closest_vertex_empty) {
  for (bool cleared : {false, true}) {
    BLPath path;
    if (cleared) {
      path = BLPath::make_from_segments(MoveTo{1, 2}, LineTo{3, 4});
      EXPECT_OK(path.clear());
    }

    size_t index = 123;
    double distance = -1.0;
    EXPECT_EQ(path.get_closest_vertex(BLPoint(3, 4), 5.0, &index, &distance), BL_ERROR_NO_MATCHING_VERTEX);
    EXPECT_EQ(index, SIZE_MAX);
    EXPECT_TRUE(Math::is_nan(distance));

    index = 123;
    EXPECT_EQ(path.get_closest_vertex(BLPoint(3, 4), 0.0, &index), BL_ERROR_NO_MATCHING_VERTEX);
    EXPECT_EQ(index, SIZE_MAX);
  }
}

TEST_CASE(core_path_hit_test_empty) {
  for (const BLPath& path : {
    BLPath{},
    BLPath::make_from_segments(MoveTo{100, 100}),
    BLPath::make_from_segments(MoveTo{100, 100}, Close{})}
  ) {
    for (BLFillRule rule : {BL_FILL_RULE_NON_ZERO, BL_FILL_RULE_EVEN_ODD}) {
      EXPECT_EQ(path.hit_test(BLPoint(100, 100), rule), BL_HIT_TEST_OUT);
    }
  }
}

TEST_CASE(core_path_hit_test_invalid) {
  for (const BLPath& path : {
    BLPath::make_from_segments(LineTo{1, 2}),
    BLPath::make_from_segments(QuadTo{1, 2, 3, 4}),
    BLPath::make_from_segments(ConicTo{1, 2, 3, 4, 0.5}),
    BLPath::make_from_segments(CubicTo{1, 2, 3, 4, 5, 6}),
    BLPath::make_from_segments(MoveTo{1, 2}, Close{}, LineTo{3, 4})
  }) {
    for (BLFillRule rule : {BL_FILL_RULE_NON_ZERO, BL_FILL_RULE_EVEN_ODD}) {
      EXPECT_EQ(path.hit_test(BLPoint(0, 0), rule), BL_HIT_TEST_INVALID);
    }
  }
}

TEST_CASE(core_path_hit_test_single) {
  BLPath path = BLPath::make_from_segments(MoveTo{50, 50}, QuadTo{150, 250, 250, 50},
                          ConicTo{400, 250, 500, 50, 1.9}, CubicTo{500, 300, 400, 500, 50, 500});

  EXPECT_EQ(path.hit_test(BLPoint(150, 150), BL_FILL_RULE_EVEN_ODD), BL_HIT_TEST_IN);
}

TEST_CASE(core_path_hit_test_multiple) {
  BLPath path = BLPath::make_from_segments(MoveTo{50, 50}, QuadTo{150, 250, 250, 50},
                          ConicTo{400, 250, 500, 50, 1.9}, CubicTo{500, 300, 400, 500, 50, 500});
  EXPECT_OK(path.add_segments(MoveTo{200, 250}, LineTo{300, 250}, LineTo{250, 350}));

  EXPECT_EQ(path.hit_test(BLPoint( 56,  74), BL_FILL_RULE_EVEN_ODD), BL_HIT_TEST_IN);
  EXPECT_EQ(path.hit_test(BLPoint( 64, 488), BL_FILL_RULE_EVEN_ODD), BL_HIT_TEST_IN);
  EXPECT_EQ(path.hit_test(BLPoint(144, 230), BL_FILL_RULE_EVEN_ODD), BL_HIT_TEST_IN);
  EXPECT_EQ(path.hit_test(BLPoint(300, 123), BL_FILL_RULE_EVEN_ODD), BL_HIT_TEST_IN);
  EXPECT_EQ(path.hit_test(BLPoint(370, 304), BL_FILL_RULE_EVEN_ODD), BL_HIT_TEST_IN);
  EXPECT_EQ(path.hit_test(BLPoint(478, 124), BL_FILL_RULE_EVEN_ODD), BL_HIT_TEST_IN);

  EXPECT_EQ(path.hit_test(BLPoint( 25, 322), BL_FILL_RULE_EVEN_ODD), BL_HIT_TEST_OUT);
  EXPECT_EQ(path.hit_test(BLPoint(147, 129), BL_FILL_RULE_EVEN_ODD), BL_HIT_TEST_OUT);
  EXPECT_EQ(path.hit_test(BLPoint(242, 482), BL_FILL_RULE_EVEN_ODD), BL_HIT_TEST_OUT);
  EXPECT_EQ(path.hit_test(BLPoint(244, 296), BL_FILL_RULE_EVEN_ODD), BL_HIT_TEST_OUT);
  EXPECT_EQ(path.hit_test(BLPoint(350, 431), BL_FILL_RULE_EVEN_ODD), BL_HIT_TEST_OUT);
  EXPECT_EQ(path.hit_test(BLPoint(382, 142), BL_FILL_RULE_EVEN_ODD), BL_HIT_TEST_OUT);
  EXPECT_EQ(path.hit_test(BLPoint(500, 322), BL_FILL_RULE_EVEN_ODD), BL_HIT_TEST_OUT);

  EXPECT_EQ(path.hit_test(BLPoint(247, 294), BL_FILL_RULE_NON_ZERO), BL_HIT_TEST_IN);
}

TEST_CASE(core_path_hit_test_fill_rules) {
  for (bool closed : {false, true}) {
    for (bool reversed : {false, true}) {
      BLPath path = BLPath::make_from_segments(MoveTo{0, 0}, LineTo{10, 0}, LineTo{10, 10}, LineTo{0, 10});
      if (closed) {
        EXPECT_OK(path.close());
      }
      EXPECT_OK(path.add_segments(MoveTo{2, 2}, LineTo{reversed ? 2.0 : 8.0, reversed ? 8.0 : 2.0},
                                  LineTo{8, 8}, LineTo{reversed ? 8.0 : 2.0, reversed ? 2.0 : 8.0}));
      if (closed) {
        EXPECT_OK(path.close());
      }

      // Same-direction nested figures fill under nonzero; opposite directions produce a hole under both rules.
      for (BLFillRule rule : {BL_FILL_RULE_NON_ZERO, BL_FILL_RULE_EVEN_ODD}) {
        EXPECT_EQ(path.hit_test(BLPoint(1, 1), rule), BL_HIT_TEST_IN);
        EXPECT_EQ(path.hit_test(BLPoint(-1, 5), rule), BL_HIT_TEST_OUT);
        EXPECT_EQ(path.hit_test(BLPoint(5, 5), rule),
                  rule == BL_FILL_RULE_NON_ZERO && !reversed ? BL_HIT_TEST_IN : BL_HIT_TEST_OUT);
      }
    }
  }
}

TEST_CASE(core_path_hit_test_conic) {
  // A 90-degree circular arc is an exact conic (rational quadratic) with the
  // control point placed at the intersection of the endpoint tangents and a
  // weight of cos(45deg).
  constexpr double r = 100.0;

  // CCW circle of radius `r` centered at (cx, cy), built from 4 conic arcs.
  constexpr double cx = 33.0;
  constexpr double cy = -17.0;

  BLPath circle = BLPath::make_from_segments(
    MoveTo{cx + r, cy},
    ConicTo{cx + r, cy + r, cx,     cy + r, Math::kSQRT_0p5},
    ConicTo{cx - r, cy + r, cx - r, cy,     Math::kSQRT_0p5},
    ConicTo{cx - r, cy - r, cx,     cy - r, Math::kSQRT_0p5},
    ConicTo{cx + r, cy - r, cx + r, cy,     Math::kSQRT_0p5}, Close{});

  // Sample a grid and compare against analytic disk membership. A small band around the radius is skipped to avoid
  // boundary precision ambiguity (both the conic flattening and the float comparison are exact only away from it).
  constexpr double kBand = 0.25;

  for (int iy = -130; iy <= 130; iy += 5) {
    for (int ix = -130; ix <= 130; ix += 5) {
      BLPoint pt(cx + double(ix), cy + double(iy));
      double dist = Math::hypot(double(ix), double(iy));

      if (bl_abs(dist - r) < kBand) {
        continue;
      }

      BLHitTest expected = dist < r ? BL_HIT_TEST_IN : BL_HIT_TEST_OUT;

      EXPECT_EQ(circle.hit_test(pt, BL_FILL_RULE_NON_ZERO), expected)
        .message("NonZero hit_test failed at [%g, %g] (dist=%g, r=%g)", pt.x, pt.y, dist, r);

      // A single non-self-intersecting loop yields the same result for both rules.
      EXPECT_EQ(circle.hit_test(pt, BL_FILL_RULE_EVEN_ODD), expected)
        .message("EvenOdd hit_test failed at [%g, %g] (dist=%g, r=%g)", pt.x, pt.y, dist, r);
    }
  }

  // A heavy conic (large weight) bulges close to its control point - verify the rational denominator is handled
  // (a non-rational quad would mis-locate the crossing). Triangle-ish wedge closed back to the start.
  BLPath wedge = BLPath::make_from_segments(MoveTo{0, 0}, ConicTo{100, 0, 100, 100, 8.0}, Close{});

  // Deep inside the bulge, near the control corner.
  EXPECT_EQ(wedge.hit_test(BLPoint(90, 10), BL_FILL_RULE_NON_ZERO), BL_HIT_TEST_IN);

  // Outside the bulge but inside the control triangle - excluded by the curve.
  EXPECT_EQ(wedge.hit_test(BLPoint(20, 60), BL_FILL_RULE_NON_ZERO), BL_HIT_TEST_OUT);

  // Clearly outside.
  EXPECT_EQ(wedge.hit_test(BLPoint(-5, 50), BL_FILL_RULE_NON_ZERO), BL_HIT_TEST_OUT);
}

TEST_CASE(core_path_c_api_vs_cxx_api) {
  // C-API getters are inlined in C++ API, so these have to be tested explicitly.
  BLPath p1;

  EXPECT_EQ(bl_path_get_byte_size(&p1), 0u);
  EXPECT_EQ(bl_path_get_vertex_count(&p1), 0u);
  EXPECT_EQ(bl_path_get_vertex_data_size(&p1), 0u);
  EXPECT_EQ(bl_path_get_control_data_size(&p1), 0u);

  EXPECT_OK(p1.add_segments(MoveTo{1, 2}, LineTo{1, 2}, ConicTo{3, 4, 5, 6, 0.5}, Close{}));
  EXPECT_EQ(bl_path_get_byte_size(&p1), p1.byte_size());
  EXPECT_EQ(bl_path_get_byte_capacity(&p1), p1.byte_capacity());
  EXPECT_EQ(bl_path_get_vertex_count(&p1), p1.vertex_count());
  EXPECT_EQ(bl_path_get_vertex_data_size(&p1), p1.vertex_data_size());
  EXPECT_EQ(bl_path_get_control_data_size(&p1), p1.control_data_size());
  EXPECT_EQ(bl_path_get_vertex_data(&p1), p1.vertex_data());
  EXPECT_EQ(bl_path_get_control_data_rev(&p1), p1.control_data_rev());
}

TEST_CASE(core_path_alloc_overflow) {
  BLPath path = BLPath::make_from_segments(MoveTo{1, 2});
  BLPath original(path);

  BLPoint* vertex_data = nullptr;
  uint8_t* control_data = nullptr;

  // Reject counts before multiplying vertices or adding control bytes.
  EXPECT_EQ(path.add_uninitialized(SIZE_MAX / sizeof(BLPoint) + 1, 0, &vertex_data, &control_data), BL_ERROR_OUT_OF_MEMORY);
  EXPECT_EQ(path.add_uninitialized(1, SIZE_MAX - sizeof(BLPoint) + 1, &vertex_data, &control_data), BL_ERROR_OUT_OF_MEMORY);
  EXPECT_EQ(path.add_uninitialized(0, SIZE_MAX, &vertex_data, &control_data), BL_ERROR_OUT_OF_MEMORY);

  const BLPoint point(3, 4);
  const BLPointI point_i(3, 4);
  const size_t polyline_count = SIZE_MAX / BLPath::LineTo::kByteSize + 1;
  const size_t polygon_count = SIZE_MAX / BLPath::LineTo::kByteSize;

  // Oversized requests must be rejected before accessing the input points.
  EXPECT_OK(path.poly_to(&point, 0));
  EXPECT_EQ(path.poly_to(&point, PathDetail::kMaxDataSize / BLPath::LineTo::kByteSize + 1), BL_ERROR_OUT_OF_MEMORY);
  EXPECT_EQ(path.poly_to(&point, polyline_count), BL_ERROR_OUT_OF_MEMORY);
  EXPECT_EQ(path.add_polyline(&point, polyline_count), BL_ERROR_OUT_OF_MEMORY);
  EXPECT_EQ(path.add_polyline(&point_i, polyline_count), BL_ERROR_OUT_OF_MEMORY);
  EXPECT_EQ(path.add_polygon(&point, polygon_count), BL_ERROR_OUT_OF_MEMORY);
  EXPECT_EQ(path.add_polygon(&point_i, polygon_count), BL_ERROR_OUT_OF_MEMORY);

  EXPECT_TRUE(path_view_equals(path.view(), original.view()));
}

TEST_CASE(core_path_reserve_overflow) {
  for (bool populated : {false, true}) {
    BLPath path;
    if (populated) {
      path = BLPath::make_from_segments(MoveTo{0, 0}, LineTo{1, 1});
    }
    BLPath original(path);

    // The implementation header must also fit within the allocation limit.
    for (size_t capacity : {SIZE_MAX, size_t(0xFFFFFFFFu), PathDetail::kMaxDataSize + 1}) {
      EXPECT_EQ(path.reserve(capacity), BL_ERROR_OUT_OF_MEMORY);
      EXPECT_EQ(path.reserve_for_append(capacity), BL_ERROR_OUT_OF_MEMORY);
    }
    EXPECT_EQ(path.reserve_for_append(PathDetail::kMaxDataSize - path.byte_size() + 1), BL_ERROR_OUT_OF_MEMORY);
    EXPECT_TRUE(path_view_equals(path.view(), original.view()));
  }
}

} // {bl::PathTests}

#endif // BL_TEST
