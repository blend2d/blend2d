// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/core/path_p.h>
#include <blend2d/core/stroke_options.h>
#include <blend2d/geometry/dasher_p.h>

#include <initializer_list>

namespace bl::Geometry::Tests {

struct DashTestFigure {
  BLPoint start;
  BLPoint end;
  double length;
  size_t line_count;
  size_t quad_count;
  bool closed;
};

// Measure quadratics independently of the quadrature used by the dasher.
static double dash_test_quad_length(BLPoint p0, BLPoint p1, BLPoint p2) noexcept {
  double result = 0.0;
  BLPoint previous = p0;
  constexpr size_t kSteps = 1024;

  for (size_t i = 1; i <= kSteps; i++) {
    double t = double(i) / double(kSteps);
    double u = 1.0 - t;
    BLPoint point = p0 * (u * u) + p1 * (2.0 * u * t) + p2 * (t * t);
    result += magnitude(point - previous);
    previous = point;
  }

  return result;
}

static size_t dash_test_figures(const BLPath& path, DashTestFigure* figures, size_t capacity) noexcept {
  size_t count = 0;
  PathIterator it(path.view());

  while (it.has_command()) {
    BLPathCmd cmd = it.command();
    if (cmd == BL_PATH_CMD_MOVE) {
      EXPECT_LT(count, capacity);
      figures[count++] = DashTestFigure{it.vertex(0), it.vertex(0), 0.0, 0, 0, false};
    }
    else {
      EXPECT_GT(count, 0u);
      DashTestFigure& figure = figures[count - 1u];

      if (cmd == BL_PATH_CMD_LINE) {
        figure.length += magnitude(it.vertex(0) - figure.end);
        figure.end = it.vertex(0);
        figure.line_count++;
      }
      else if (cmd == BL_PATH_CMD_QUAD) {
        figure.length += dash_test_quad_length(figure.end, it.vertex(0), it.vertex(1));
        figure.end = it.vertex(1);
        figure.quad_count++;
      }
      else {
        EXPECT_EQ(cmd, BL_PATH_CMD_CLOSE);
        figure.length += magnitude(figure.start - figure.end);
        figure.end = figure.start;
        figure.closed = true;
      }
    }

    it.advance_non_conic(cmd);
  }

  return count;
}

static void dash_test_near(BLPoint a, BLPoint b, double epsilon = 1e-9) noexcept {
  EXPECT_LE(magnitude(a - b), epsilon);
}

static void dash_test_quad_angles(const BLPath& path, double m) noexcept {
  PathIterator it(path.view());
  BLPoint start;

  while (it.has_command()) {
    BLPathCmd cmd = it.command();
    if (cmd == BL_PATH_CMD_MOVE || cmd == BL_PATH_CMD_LINE) {
      start = it.vertex(0);
    }
    else if (cmd == BL_PATH_CMD_QUAD) {
      BLPoint first = it.vertex(0) - start;
      BLPoint last = it.vertex(1) - it.vertex(0);
      BLPoint chord = it.vertex(1) - start;
      if (first == BLPoint(0.0, 0.0)) first = chord;
      if (last == BLPoint(0.0, 0.0)) last = chord;
      double angle = bl_abs(Math::atan2(cross(first, last), dot(first, last)));
      EXPECT_LE(angle, Math::atan(m) + 1e-9)
        .message("angle=%.17g limit=%.17g, (%g,%g) (%g,%g) (%g,%g)",
          angle, Math::atan(m), start.x, start.y, it.vertex(0).x, it.vertex(0).y, it.vertex(1).x, it.vertex(1).y);
      start = it.vertex(1);
    }
    else {
      EXPECT_EQ(cmd, BL_PATH_CMD_CLOSE);
    }
    it.advance_non_conic(cmd);
  }
}

static void dash_test_line_runs(const BLPath& path, const double* runs, size_t count) noexcept {
  DashTestFigure figures[64];
  EXPECT_EQ(dash_test_figures(path, figures, BL_ARRAY_SIZE(figures)), count);

  for (size_t i = 0; i < count; i++) {
    dash_test_near(figures[i].start, BLPoint(runs[i * 2u], 0.0));
    dash_test_near(figures[i].end, BLPoint(runs[i * 2u + 1u], 0.0));
    EXPECT_EQ(figures[i].quad_count, 0u);
    EXPECT_FALSE(figures[i].closed);
  }
}

TEST_CASE(geometry_dasher_lines) {
  BLPath input;
  EXPECT_OK(input.move_to(0.0, 0.0));
  EXPECT_OK(input.line_to(20.0, 0.0));

  const float dashes[] = {3.0f, 2.0f};
  BLStrokeOptions options;
  EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));

  const float offsets[] = {0.0f, 1.0f, -1.0f, 11.0f};
  const double expected[][10] = {
    {0.0, 3.0, 5.0, 8.0, 10.0, 13.0, 15.0, 18.0},
    {0.0, 2.0, 4.0, 7.0, 9.0, 12.0, 14.0, 17.0, 19.0, 20.0},
    {1.0, 4.0, 6.0, 9.0, 11.0, 14.0, 16.0, 19.0},
    {0.0, 2.0, 4.0, 7.0, 9.0, 12.0, 14.0, 17.0, 19.0, 20.0}
  };
  const size_t counts[] = {4, 5, 4, 5};

  for (size_t i = 0; i < BL_ARRAY_SIZE(offsets); i++) {
    BLPath output;
    EXPECT_OK(options.set_dash_offset(offsets[i]));
    EXPECT_OK(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr));
    dash_test_line_runs(output, expected[i], counts[i]);
  }
}

TEST_CASE(geometry_dasher_odd_array) {
  BLPath input;
  EXPECT_OK(input.move_to(0.0, 0.0));
  EXPECT_OK(input.line_to(15.0, 0.0));

  const float dashes[] = {2.0f, 1.0f, 3.0f};
  const double expected[] = {0.0, 2.0, 3.0, 6.0, 8.0, 9.0, 12.0, 14.0};
  BLStrokeOptions options;
  EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));

  BLPath output;
  EXPECT_OK(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr));
  dash_test_line_runs(output, expected, 4);

  // Advancing one array length changes the phase; two array lengths restore it.
  EXPECT_OK(output.clear());
  EXPECT_OK(options.set_dash_offset(12.0f));
  EXPECT_OK(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr));
  dash_test_line_runs(output, expected, 4);

  EXPECT_OK(output.clear());
  EXPECT_OK(options.set_dash_offset(6.0f));
  EXPECT_OK(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr));
  const double shifted[] = {2.0, 3.0, 6.0, 8.0, 9.0, 12.0, 14.0, 15.0};
  dash_test_line_runs(output, shifted, 4);
}

TEST_CASE(geometry_dasher_segment_continuity) {
  BLPath input;
  EXPECT_OK(input.move_to(0.0, 0.0));
  EXPECT_OK(input.line_to(2.0, 0.0));
  EXPECT_OK(input.line_to(10.0, 0.0));
  EXPECT_OK(input.move_to(0.0, 0.0));
  EXPECT_OK(input.line_to(4.0, 0.0));

  const float dashes[] = {3.0f, 2.0f};
  BLStrokeOptions options;
  EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));

  BLPath output;
  EXPECT_OK(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr));
  const double expected[] = {0.0, 3.0, 5.0, 8.0, 0.0, 3.0};
  dash_test_line_runs(output, expected, 3);

  // A dash crossing an input vertex retains that vertex in the same output figure.
  PathIterator it(output.view());
  EXPECT_EQ(it.command(), BL_PATH_CMD_MOVE);
  it.advance_cmd<BL_PATH_CMD_MOVE>();
  EXPECT_EQ(it.command(), BL_PATH_CMD_LINE);
  EXPECT_EQ(it.vertex(0), BLPoint(2.0, 0.0));
  it.advance_cmd<BL_PATH_CMD_LINE>();
  EXPECT_EQ(it.command(), BL_PATH_CMD_LINE);
  dash_test_near(it.vertex(0), BLPoint(3.0, 0.0));
}

TEST_CASE(geometry_dasher_quadratic_length) {
  BLPath input;
  EXPECT_OK(input.move_to(0.0, 0.0));
  EXPECT_OK(input.quad_to(50.0, 100.0, 100.0, 0.0));

  const float dashes[] = {20.0f, 10.0f};
  BLStrokeOptions options;
  EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));

  BLPath output;
  EXPECT_OK(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr));

  DashTestFigure figures[16];
  EXPECT_EQ(dash_test_figures(output, figures, BL_ARRAY_SIZE(figures)), 5u);
  for (size_t i = 0; i < 5; i++) {
    EXPECT_EQ(figures[i].line_count, 0u);
    EXPECT_GT(figures[i].quad_count, 0u);
    EXPECT_LE(bl_abs(figures[i].length - 20.0), 1e-4);
  }
  dash_test_quad_angles(output, double(bl_default_approximation_options.offset_parameter));

  // Every emitted quad must be an exact subcurve, including its control point.
  PathIterator it(output.view());
  BLPoint start;
  while (it.has_command()) {
    if (it.is_move()) {
      start = it.vertex(0);
      it.advance_cmd<BL_PATH_CMD_MOVE>();
    }
    else {
      EXPECT_EQ(it.command(), BL_PATH_CMD_QUAD);
      BLPoint cp = it.vertex(0);
      BLPoint end = it.vertex(1);
      double t0 = start.x / 100.0;
      double t1 = end.x / 100.0;
      dash_test_near(start, BLPoint(100.0 * t0, 200.0 * t0 * (1.0 - t0)));
      dash_test_near(end, BLPoint(100.0 * t1, 200.0 * t1 * (1.0 - t1)));
      dash_test_near(cp, BLPoint(50.0 * (t0 + t1), 100.0 * (t0 + t1 - 2.0 * t0 * t1)));
      start = end;
      it.advance_cmd<BL_PATH_CMD_QUAD>();
    }
  }
}

TEST_CASE(geometry_dasher_quadratic_simplify) {
  BLStrokeOptions options;
  const float dashes[] = {10000.0f, 1.0f};
  EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));
  BLApproximationOptions approx = bl_default_approximation_options;
  approx.offset_parameter = 0.2f;

  // A gentle curve already meets the angular bound and keeps its original control point.
  BLPath gentle;
  EXPECT_OK(gentle.move_to(0.0, 0.0));
  EXPECT_OK(gentle.quad_to(50.0, 1.0, 100.0, 0.0));
  BLPath output;
  EXPECT_OK(dash_path(gentle.view(), options, approx, output, nullptr, nullptr));
  EXPECT_EQ(output, gentle);

  // A sharp curve is split before length measurement, even without a dash boundary in the curve.
  BLPath sharp;
  EXPECT_OK(sharp.move_to(0.0, 0.0));
  EXPECT_OK(sharp.quad_to(0.0, 100.0, 1.0, 0.0));
  EXPECT_OK(output.clear());
  EXPECT_OK(dash_path(sharp.view(), options, approx, output, nullptr, nullptr));
  DashTestFigure figures[16];
  EXPECT_EQ(dash_test_figures(output, figures, BL_ARRAY_SIZE(figures)), 1u);
  EXPECT_GT(figures[0].quad_count, 1u);
  dash_test_quad_angles(output, double(approx.offset_parameter));

  // The nearly reversing quad still consumes the same dash lengths across its simplification cuts.
  const float short_dashes[] = {7.0f, 3.0f};
  EXPECT_OK(options.set_dash_array(short_dashes, BL_ARRAY_SIZE(short_dashes)));
  EXPECT_OK(output.clear());
  EXPECT_OK(dash_path(sharp.view(), options, approx, output, nullptr, nullptr));
  size_t count = dash_test_figures(output, figures, BL_ARRAY_SIZE(figures));
  EXPECT_GT(count, 4u);
  for (size_t i = 0; i + 1u < count; i++)
    EXPECT_LE(bl_abs(figures[i].length - 7.0), 1e-4);
  dash_test_quad_angles(output, double(approx.offset_parameter));

  // Collinear reversals must split at the cusp despite their zero cross product.
  BLPath reversal;
  EXPECT_OK(reversal.move_to(0.0, 0.0));
  EXPECT_OK(reversal.quad_to(100.0, 0.0, -100.0, 0.0));
  const float reversal_dashes[] = {20.0f, 10.0f};
  EXPECT_OK(options.set_dash_array(reversal_dashes, BL_ARRAY_SIZE(reversal_dashes)));
  EXPECT_OK(output.clear());
  EXPECT_OK(dash_path(reversal.view(), options, approx, output, nullptr, nullptr));
  EXPECT_EQ(dash_test_figures(output, figures, BL_ARRAY_SIZE(figures)), 6u);
  for (size_t i = 0; i < 5; i++)
    EXPECT_LE(bl_abs(figures[i].length - 20.0), 1e-9);
  EXPECT_LE(bl_abs(figures[5].length - 50.0 / 3.0), 1e-9);
  EXPECT_GT(figures[1].quad_count, 1u);
  dash_test_near(figures[0].end, BLPoint(20.0, 0.0));
  dash_test_near(figures[1].start, BLPoint(30.0, 0.0));
  dash_test_near(figures[1].end, BLPoint(50.0 / 3.0, 0.0));
  dash_test_near(figures[5].end, BLPoint(-100.0, 0.0));
  dash_test_quad_angles(output, double(approx.offset_parameter));

  // An inexact cusp parameter must not leave a second microscopic reversal after splitting.
  EXPECT_OK(reversal.clear());
  EXPECT_OK(reversal.move_to(0.0, 0.0));
  EXPECT_OK(reversal.quad_to(1.0, 0.0, -1.0, 0.0));
  EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));
  EXPECT_OK(output.clear());
  EXPECT_OK(dash_path(reversal.view(), options, approx, output, nullptr, nullptr));
  EXPECT_EQ(dash_test_figures(output, figures, BL_ARRAY_SIZE(figures)), 1u);
  EXPECT_EQ(figures[0].quad_count, 2u);
  EXPECT_LE(bl_abs(figures[0].length - 5.0 / 3.0), 1e-12);
  dash_test_quad_angles(output, double(approx.offset_parameter));
}

TEST_CASE(geometry_dasher_curve_conversion) {
  BLStrokeOptions options;
  const float dashes[] = {1000.0f, 1.0f};
  EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));
  BLApproximationOptions approx = bl_default_approximation_options;
  approx.simplify_tolerance = 10.0f;
  approx.offset_parameter = 0.1f;

  for (uint32_t kind = 0; kind < 2; kind++) {
    BLPath input;
    EXPECT_OK(input.move_to(0.0, 0.0));
    if (kind == 0)
      EXPECT_OK(input.cubic_to(20.0, 80.0, 80.0, -50.0, 100.0, 0.0));
    else
      EXPECT_OK(input.conic_to(50.0, 80.0, 100.0, 0.0, 0.7071067811865476));

    BLPath output;
    EXPECT_OK(dash_path(input.view(), options, approx, output, nullptr, nullptr));
    DashTestFigure figures[4];
    EXPECT_EQ(dash_test_figures(output, figures, BL_ARRAY_SIZE(figures)), 1u);
    EXPECT_EQ(figures[0].start, BLPoint(0.0, 0.0));
    EXPECT_EQ(figures[0].end, BLPoint(100.0, 0.0));
    EXPECT_EQ(figures[0].line_count, 0u);
    EXPECT_GT(figures[0].quad_count, 0u);
    dash_test_quad_angles(output, double(approx.offset_parameter));

    // Converted pieces share the dash state instead of restarting the pattern.
    const float short_dashes[] = {7.0f, 3.0f};
    EXPECT_OK(options.set_dash_array(short_dashes, BL_ARRAY_SIZE(short_dashes)));
    EXPECT_OK(output.clear());
    EXPECT_OK(dash_path(input.view(), options, approx, output, nullptr, nullptr));
    DashTestFigure runs[64];
    size_t count = dash_test_figures(output, runs, BL_ARRAY_SIZE(runs));
    EXPECT_GT(count, 4u);
    for (size_t i = 0; i + 1u < count; i++)
      EXPECT_LE(bl_abs(runs[i].length - 7.0), 1e-4);
    dash_test_quad_angles(output, double(approx.offset_parameter));

    EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));
  }
}

TEST_CASE(geometry_dasher_closed_figure) {
  BLPath input;
  EXPECT_OK(input.move_to(0.0, 0.0));
  EXPECT_OK(input.line_to(10.0, 0.0));
  EXPECT_OK(input.line_to(10.0, 10.0));
  EXPECT_OK(input.line_to(0.0, 10.0));
  EXPECT_OK(input.close());

  const float dashes[] = {15.0f, 10.0f};
  BLStrokeOptions options;
  EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));
  BLPath output;
  EXPECT_OK(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr));

  DashTestFigure figures[4];
  EXPECT_EQ(dash_test_figures(output, figures, BL_ARRAY_SIZE(figures)), 1u);
  dash_test_near(figures[0].start, BLPoint(5.0, 10.0));
  dash_test_near(figures[0].end, BLPoint(10.0, 5.0));
  EXPECT_LE(bl_abs(figures[0].length - 30.0), 1e-9);
  EXPECT_FALSE(figures[0].closed);

  const float solid_dashes[] = {100.0f, 1.0f};
  EXPECT_OK(options.set_dash_array(solid_dashes, BL_ARRAY_SIZE(solid_dashes)));
  EXPECT_OK(output.clear());
  EXPECT_OK(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr));
  EXPECT_EQ(dash_test_figures(output, figures, BL_ARRAY_SIZE(figures)), 1u);
  EXPECT_TRUE(figures[0].closed);
  EXPECT_LE(bl_abs(figures[0].length - 40.0), 1e-9);

  // An offset at a zero-length gap starts the next on interval at the seam.
  const float zero_gap_dashes[] = {10.0f, 0.0f, 100.0f, 10.0f};
  EXPECT_OK(options.set_dash_array(zero_gap_dashes, BL_ARRAY_SIZE(zero_gap_dashes)));
  EXPECT_OK(options.set_dash_offset(10.0f));
  EXPECT_OK(output.clear());
  EXPECT_OK(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr));
  EXPECT_EQ(dash_test_figures(output, figures, BL_ARRAY_SIZE(figures)), 1u);
  EXPECT_TRUE(figures[0].closed);
  EXPECT_LE(bl_abs(figures[0].length - 40.0), 1e-9);
}

struct DashTestSinkContext {
  size_t calls = 0;
  size_t figures = 0;
  BLResult result = BL_SUCCESS;
};

static BLResult BL_CDECL dash_test_sink(BLPathCore* path, void* user_data) noexcept {
  DashTestSinkContext& context = *static_cast<DashTestSinkContext*>(user_data);
  DashTestFigure figures[16];
  context.calls++;
  context.figures += dash_test_figures(path->dcast(), figures, BL_ARRAY_SIZE(figures));
  BL_PROPAGATE(path->dcast().clear());
  return context.result;
}

TEST_CASE(geometry_dasher_sink_and_solid) {
  BLPath input;
  EXPECT_OK(input.move_to(0.0, 0.0));
  EXPECT_OK(input.line_to(10.0, 0.0));
  EXPECT_OK(input.move_to(0.0, 1.0));
  EXPECT_OK(input.line_to(10.0, 1.0));

  BLStrokeOptions options;
  BLPath output;
  EXPECT_OK(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr));
  EXPECT_EQ(output, input);

  const float dashes[] = {3.0f, 2.0f};
  EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));
  EXPECT_OK(output.clear());
  DashTestSinkContext context;
  EXPECT_OK(dash_path(input.view(), options, bl_default_approximation_options, output, dash_test_sink, &context));
  EXPECT_EQ(context.calls, 2u);
  EXPECT_EQ(context.figures, 4u);
  EXPECT_TRUE(output.is_empty());

  context = DashTestSinkContext{};
  context.result = BL_ERROR_NO_MORE_DATA;
  EXPECT_EQ(dash_path(input.view(), options, bl_default_approximation_options, output, dash_test_sink, &context),
    BL_ERROR_NO_MORE_DATA);
  EXPECT_EQ(context.calls, 1u);
}

TEST_CASE(geometry_dasher_degenerate) {
  BLPath input;
  EXPECT_OK(input.move_to(0.0, 0.0));
  EXPECT_OK(input.line_to(0.0, 0.0));
  EXPECT_OK(input.quad_to(0.0, 0.0, 0.0, 0.0));
  EXPECT_OK(input.line_to(10.0, 0.0));

  BLStrokeOptions options;
  const float dashes[] = {3.0f, 2.0f};
  EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));
  BLPath output;
  EXPECT_OK(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr));
  DashTestFigure figures[8];
  EXPECT_EQ(dash_test_figures(output, figures, BL_ARRAY_SIZE(figures)), 2u);
  dash_test_near(figures[0].start, BLPoint(0.0, 0.0));
  dash_test_near(figures[0].end, BLPoint(3.0, 0.0));
  dash_test_near(figures[1].start, BLPoint(5.0, 0.0));
  dash_test_near(figures[1].end, BLPoint(8.0, 0.0));

  // Zero-length on intervals are represented as points for stroke caps.
  const float point_dashes[] = {0.0f, 5.0f};
  EXPECT_OK(options.set_dash_array(point_dashes, BL_ARRAY_SIZE(point_dashes)));
  EXPECT_OK(output.clear());
  BLPath line;
  EXPECT_OK(line.move_to(0.0, 0.0));
  EXPECT_OK(line.line_to(10.0, 0.0));
  EXPECT_OK(dash_path(line.view(), options, bl_default_approximation_options, output, nullptr, nullptr));
  DashTestFigure points[8];
  size_t count = dash_test_figures(output, points, BL_ARRAY_SIZE(points));
  EXPECT_EQ(count, 3u);
  for (size_t i = 0; i < count; i++) {
    EXPECT_EQ(points[i].start, BLPoint(double(i) * 5.0, 0.0));
    EXPECT_EQ(points[i].start, points[i].end);
    EXPECT_EQ(points[i].length, 0.0);
    EXPECT_GT(points[i].line_count, 0u);
  }

  // A zero-length segment consumes no phase, and a point inside a gap stays hidden.
  BLPath point;
  EXPECT_OK(point.move_to(2.0, 3.0));
  EXPECT_OK(point.line_to(2.0, 3.0));
  EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));
  EXPECT_OK(options.set_dash_offset(3.5f));
  EXPECT_OK(output.clear());
  EXPECT_OK(dash_path(point.view(), options, bl_default_approximation_options, output, nullptr, nullptr));
  EXPECT_TRUE(output.is_empty());
}

TEST_CASE(geometry_dasher_invalid_options) {
  BLPath input;
  EXPECT_OK(input.move_to(0.0, 0.0));
  EXPECT_OK(input.line_to(10.0, 0.0));
  BLPath output;
  BLStrokeOptions options;

  const float patterns[][2] = {
    {-1.0f, 2.0f},
    {Math::nan<float>(), 2.0f},
    {1.0f, Math::inf<float>()},
    {0.0f, 0.0f}
  };
  for (const auto& pattern : patterns) {
    EXPECT_OK(options.set_dash_array(pattern, BL_ARRAY_SIZE(pattern)));
    EXPECT_EQ(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr),
      BL_ERROR_INVALID_VALUE);
  }

  const float dashes[] = {3.0f, 2.0f};
  EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));
  const float invalid_values[] = {Math::nan<float>(), Math::inf<float>(), -Math::inf<float>()};
  for (float value : invalid_values) {
    EXPECT_OK(options.set_dash_offset(value));
    EXPECT_EQ(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr),
      BL_ERROR_INVALID_VALUE);
    EXPECT_OK(options.set_dash_offset(0.0f));

    BLApproximationOptions approx = bl_default_approximation_options;
    approx.simplify_tolerance = value;
    EXPECT_EQ(dash_path(input.view(), options, approx, output, nullptr, nullptr), BL_ERROR_INVALID_VALUE);

    approx = bl_default_approximation_options;
    approx.offset_parameter = value;
    EXPECT_EQ(dash_path(input.view(), options, approx, output, nullptr, nullptr), BL_ERROR_INVALID_VALUE);
  }

  for (float value : {0.0f, -0.2f}) {
    BLApproximationOptions approx = bl_default_approximation_options;
    approx.offset_parameter = value;
    EXPECT_EQ(dash_path(input.view(), options, approx, output, nullptr, nullptr), BL_ERROR_INVALID_VALUE);
  }

  // Reject an interval that cannot advance the current coordinate instead of generating unbounded dots.
  EXPECT_OK(input.clear());
  EXPECT_OK(input.move_to(1.0, 0.0));
  EXPECT_OK(input.line_to(2.0, 0.0));
  const float tiny_dashes[] = {1e-30f, 1e-30f};
  EXPECT_OK(options.set_dash_array(tiny_dashes, BL_ARRAY_SIZE(tiny_dashes)));
  EXPECT_EQ(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr),
    BL_ERROR_INVALID_GEOMETRY);

  // Even when the split point moves near the origin, the remaining curve length must make progress.
  EXPECT_OK(input.clear());
  EXPECT_OK(input.move_to(0.0, 0.0));
  EXPECT_OK(input.quad_to(1.0, 1.0, 2.0, 0.0));
  EXPECT_EQ(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr),
    BL_ERROR_INVALID_GEOMETRY);
}

TEST_CASE(geometry_dasher_alias) {
  BLStrokeOptions options;
  const float dashes[] = {3.0f, 2.0f};
  EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));

  BLPath path;
  EXPECT_OK(path.move_to(0.0, 0.0));
  EXPECT_OK(path.line_to(10.0, 0.0));
  EXPECT_OK(dash_path(path.view(), options, bl_default_approximation_options, path, nullptr, nullptr));
  const double expected[] = {0.0, 10.0, 0.0, 3.0, 5.0, 8.0};
  dash_test_line_runs(path, expected, 3);

  // Clearing the output in a callback must not invalidate the remaining input figures.
  EXPECT_OK(path.clear());
  EXPECT_OK(path.move_to(0.0, 0.0));
  EXPECT_OK(path.line_to(10.0, 0.0));
  EXPECT_OK(path.move_to(0.0, 1.0));
  EXPECT_OK(path.line_to(10.0, 1.0));
  DashTestSinkContext context;
  EXPECT_OK(dash_path(path.view(), options, bl_default_approximation_options, path, dash_test_sink, &context));
  EXPECT_EQ(context.calls, 2u);
  // The first callback also sees the two original figures already present in the output.
  EXPECT_EQ(context.figures, 6u);
  EXPECT_TRUE(path.is_empty());
}

TEST_CASE(geometry_dasher_path_validation) {
  BLStrokeOptions options;
  const float dashes[] = {3.0f, 2.0f};
  EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));
  BLPath output;
  BLPath input;
  EXPECT_OK(input.move_to(0.0, 0.0));
  EXPECT_OK(input.line_to(10.0, 0.0));

  BLPathView view = input.view();
  view.vertex_count = 1;
  EXPECT_EQ(dash_path(view, options, bl_default_approximation_options, output, nullptr, nullptr),
    BL_ERROR_CORRUPTED_PATH_DATA);

  view = input.view();
  view.control_data_size = 0;
  EXPECT_EQ(dash_path(view, options, bl_default_approximation_options, output, nullptr, nullptr),
    BL_ERROR_CORRUPTED_PATH_DATA);

  // A complete command still cannot start a figure without MOVE.
  view = input.view();
  view.vertex_data++;
  view.vertex_count--;
  view.control_data_rev--;
  view.control_data_size--;
  EXPECT_EQ(dash_path(view, options, bl_default_approximation_options, output, nullptr, nullptr),
    BL_ERROR_CORRUPTED_PATH_DATA);

  EXPECT_OK(input.clear());
  EXPECT_OK(input.move_to(0.0, 0.0));
  EXPECT_OK(input.conic_to(5.0, 5.0, 10.0, 0.0, 0.5));
  view = input.view();
  view.control_data_size--;
  EXPECT_EQ(dash_path(view, options, bl_default_approximation_options, output, nullptr, nullptr),
    BL_ERROR_CORRUPTED_PATH_DATA);

  view = input.view();
  view.vertex_count--;
  EXPECT_EQ(dash_path(view, options, bl_default_approximation_options, output, nullptr, nullptr),
    BL_ERROR_CORRUPTED_PATH_DATA);

  // A conic's repeated command byte is part of its control-data representation.
  uint8_t control[1u + PathAdvance<BL_PATH_CMD_CONIC>::kControlSize];
  view = input.view();
  memcpy(control, view.control_data_rev - view.control_data_size, sizeof(control));
  control[0] = uint8_t(BL_PATH_CMD_LINE);
  view.control_data_rev = control + sizeof(control);
  EXPECT_EQ(dash_path(view, options, bl_default_approximation_options, output, nullptr, nullptr),
    BL_ERROR_CORRUPTED_PATH_DATA);
}

TEST_CASE(geometry_dasher_exact_boundaries) {
  BLPath input;
  EXPECT_OK(input.move_to(0.0, 0.0));
  EXPECT_OK(input.line_to(3.0, 0.0));
  EXPECT_OK(input.line_to(5.0, 0.0));
  EXPECT_OK(input.line_to(8.0, 0.0));
  EXPECT_OK(input.move_to(0.0, 0.0));
  EXPECT_OK(input.line_to(5.0, 0.0));

  BLStrokeOptions options;
  const float dashes[] = {3.0f, 2.0f};
  EXPECT_OK(options.set_dash_array(dashes, BL_ARRAY_SIZE(dashes)));
  BLPath output;
  EXPECT_OK(dash_path(input.view(), options, bl_default_approximation_options, output, nullptr, nullptr));
  const double expected[] = {0.0, 3.0, 5.0, 8.0, 0.0, 3.0};
  dash_test_line_runs(output, expected, 3);
}

} // {bl::Geometry::Tests}

#endif // BL_TEST
