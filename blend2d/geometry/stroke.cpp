// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/lookuptable_p.h>
#include <blend2d/axl/math_p.h>
#include <blend2d/core/matrix_p.h>
#include <blend2d/core/path_p.h>
#include <blend2d/geometry/bezier_p.h>
#include <blend2d/geometry/dasher_p.h>
#include <blend2d/geometry/stroke_p.h>

// MSCV warns about potentially uninitialized variables used - but it's a false positive.
BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNINIT_VAR_USED)

namespace bl::Geometry {

// bl::Path - Stroke - Constants
// =============================

// Default minimum miter-join length that always bypasses any other join-type. The reason behind this is to
// prevent emitting very small line segments in case that normals of joining segments are almost equal.
static constexpr double kStrokeMiterMinimum = 1e-10;
static constexpr double kStrokeMiterMinimumSq = Math::square(kStrokeMiterMinimum);

static constexpr double kStrokeCuspTThreshold = 1e-10;
static constexpr double kStrokeDegenerateFlatness = 1e-6;

// Epsilon used to split quadratic bezier curves during offsetting.
static constexpr double kQuadOffsetEpsilonT = 1e-5;

// Minimum vertices that would be required for any join + additional line.
//
// Calculated from:
//   JOIN:
//     bevel: 1 vertex
//     miter: 3 vertices
//     round: 7 vertices (2 cubics at most)
//   ADDITIONAL:
//     end-point: 1 vertex
//     line-to  : 1 vertex
static constexpr size_t kStrokeMaxJoinDataSize = 9 * sizeof(BLPoint) + 9;

// bl::Path - Stroke - Tables
// ==========================

struct CapGeometryByteSizeGen {
  static constexpr uint8_t value(size_t cap) noexcept {
    return uint8_t(
      BLStrokeCap(cap) == BL_STROKE_CAP_SQUARE       ? BLPath::LineTo::kByteSize * 3u :
      BLStrokeCap(cap) == BL_STROKE_CAP_ROUND        ? BLPath::LineTo::kByteSize * 0u + BLPath::ConicTo::kByteSize * 2u :
      BLStrokeCap(cap) == BL_STROKE_CAP_ROUND_REV    ? BLPath::LineTo::kByteSize * 2u + BLPath::ConicTo::kByteSize * 2u :
      BLStrokeCap(cap) == BL_STROKE_CAP_TRIANGLE     ? BLPath::LineTo::kByteSize * 2u :
      BLStrokeCap(cap) == BL_STROKE_CAP_TRIANGLE_REV ? BLPath::LineTo::kByteSize * 4u :
      BLStrokeCap(cap) == BL_STROKE_CAP_BUTT         ? BLPath::LineTo::kByteSize * 1u : 0);
  }
};

static constexpr auto cap_geometry_byte_size_table =
  make_lookup_table<uint8_t, BL_STROKE_CAP_MAX_VALUE + 1, CapGeometryByteSizeGen>();

// bl::Path - Stroke - Utilities
// =============================

enum class Side : size_t {
  kA = 0,
  kB = 1
};

static BL_INLINE Side opposite_side(Side side) noexcept { return Side(size_t(1) - size_t(side)); }

static BL_INLINE Side side_from_normals(const BLPoint& n0, const BLPoint& n1) noexcept {
  return Side(size_t(cross(n0, n1) >= 0));
}

static BL_INLINE uint32_t sanity_stroke_cap(uint32_t cap) noexcept {
  return cap <= BL_STROKE_CAP_MAX_VALUE ? cap : BL_STROKE_CAP_BUTT;
}

static BL_INLINE bool is_miter_join_category(uint32_t join_type) noexcept {
  return join_type == BL_STROKE_JOIN_MITER_CLIP  ||
         join_type == BL_STROKE_JOIN_MITER_BEVEL ||
         join_type == BL_STROKE_JOIN_MITER_ROUND ;
}

static BL_INLINE uint32_t miter_join_to_simple_join(uint32_t join_type) {
  if (join_type == BL_STROKE_JOIN_MITER_BEVEL)
    return BL_STROKE_JOIN_BEVEL;
  else if (join_type == BL_STROKE_JOIN_MITER_ROUND)
    return BL_STROKE_JOIN_ROUND;
  else
    return join_type;
}

static BL_INLINE bool test_inner_join_intersection(const BLPoint& a0, const BLPoint& a1, const BLPoint& b0, const BLPoint& b1, const BLPoint& join) noexcept {
  BLPoint min = bl_max(bl_min(a0, a1), bl_min(b0, b1));
  BLPoint max = bl_min(bl_max(a0, a1), bl_max(b0, b1));

  return axl::bool_and(join.x >= min.x, join.y >= min.y,
                       join.x <= max.x, join.y <= max.y);
}

static BL_INLINE double conic_step_simplify_parameter(const BLPoint& v1, const BLPoint& v2, double w, double tolerance_mul_4) noexcept {
  BLPoint v = v2 - v1;
  double t = Math::madd(tolerance_mul_4, w, tolerance_mul_4);

  // Smallest parameter step to satisfy tolerance condition.
  return Math::pow(t / (bl_abs(w - 1.0) * magnitude(v)), 0.25);
}

static BL_INLINE bool test_collinear_normals(const BLPoint& n0, const BLPoint& n1) noexcept {
  BLPoint diff = (n0 - n1);
  return axl::bool_and(bl_abs(diff.x) < Geometry::kNormalCollinearityEpsilon,
                       bl_abs(diff.y) < Geometry::kNormalCollinearityEpsilon);
}

// bl::Path - Stroke - Implementation
// ==================================

class Stroker {
public:
  enum Flags : uint32_t {
    kFlagIsOpen   = 0x01,
    kFlagIsClosed = 0x02
  };

  // Stroke input.
  PathIterator _input_iter;
  size_t _input_vertex_count;
  PathIterator::Location _input_beginning;

  // Stroke options.
  const BLStrokeOptions& _options;
  const BLApproximationOptions& _approx;

  struct SideData {
    BLPath* path;              // Output path (outer/inner, per side).
    PathAppender appender;     // Output path appended (outer/inner, per side).
    BLPathPosition figure_pos; // Start of the figure in output path (only A path).
    double d;                  // Distance (StrokeWidth / 2).
    double d2;                 // Distance multiplied by 2.
  };

  double _miter_limit;         // Miter limit possibly clamped to a safe range.
  double _miter_limit_sq;      // Miter limit squared.

  uint32_t _join_type;         // Simplified join type.
  SideData _side_data[2];      // A and B data (outer/inner side).

  // Stroke output.
  BLPath* _c_path;             // Output C path.

  // Global state.
  BLPoint _p0;                 // Current point.
  BLPoint _n0;                 // Unit normal of `_p0`.
  BLPoint _p_initial;          // Initial point (MoveTo).
  BLPoint _n_initial;          // Unit normal of `_p_initial`.
  uint32_t _flags;             // Work flags.

  BL_INLINE Stroker(const BLPathView& input, const BLStrokeOptions& options, const BLApproximationOptions& approx, BLPath* a, BLPath* b, BLPath* c) noexcept
    : _input_iter(input),
      _input_vertex_count(input.vertex_count),
      _options(options),
      _approx(approx),
      _join_type(options.join()),
      _c_path(c) {

    _input_beginning = _input_iter.location();

    double w = double(options.width());
    _side_data[0].path = a;
    _side_data[0].figure_pos = {};
    _side_data[0].d = w * 0.5;
    _side_data[0].d2 = w;
    _side_data[1].path = b;
    _side_data[1].figure_pos = {};
    _side_data[1].d = -_side_data[0].d;
    _side_data[1].d2 = -_side_data[0].d2;

    // Initialize miter calculation options. What we do here is to change `_join_type` to a value that would be easier
    // for us to use during joining. We always honor `_miter_limit_sq` even when the `_join_type` is not miter to prevent
    // emitting very small line segments next to next other, which saves vertices and also prevents border cases in
    // additional processing.
    if (is_miter_join_category(_join_type)) {
      // Simplify miter-join type to non-miter join, if possible.
      _join_type = miter_join_to_simple_join(_join_type);

      // Final miter limit is `0.5 * width * miter_limit`.
      _miter_limit = d() * double(options.miter_limit());
      _miter_limit_sq = Math::square(_miter_limit);
    }
    else {
      _miter_limit = kStrokeMiterMinimum;
      _miter_limit_sq = kStrokeMiterMinimumSq;
    }
  }

  BL_INLINE_NODEBUG bool is_open() const noexcept { return (_flags & kFlagIsOpen) != 0; }
  BL_INLINE_NODEBUG bool is_closed() const noexcept { return (_flags & kFlagIsClosed) != 0; }

  BL_INLINE_NODEBUG SideData& a_side() noexcept { return _side_data[size_t(Side::kA)]; }
  BL_INLINE_NODEBUG SideData& b_side() noexcept { return _side_data[size_t(Side::kB)]; }

  BL_INLINE_NODEBUG const SideData& a_side() const noexcept { return _side_data[size_t(Side::kA)]; }
  BL_INLINE_NODEBUG const SideData& b_side() const noexcept { return _side_data[size_t(Side::kB)]; }

  BL_INLINE_NODEBUG SideData& side_data(Side side) noexcept { return _side_data[size_t(side)]; }

  BL_INLINE_NODEBUG double d() const noexcept { return _side_data[0].d; }
  BL_INLINE_NODEBUG double d(Side side) const noexcept { return _side_data[size_t(side)].d; }

  BL_INLINE_NODEBUG double d2() const noexcept { return _side_data[0].d2; }
  BL_INLINE_NODEBUG double d2(Side side) const noexcept { return _side_data[size_t(side)].d2; }

  BL_INLINE_NODEBUG BLPath* a_path() const noexcept { return a_side().path; }
  BL_INLINE_NODEBUG BLPath* b_path() const noexcept { return b_side().path; }
  BL_INLINE_NODEBUG BLPath* c_path() const noexcept { return _c_path; }

  BL_INLINE_NODEBUG BLPath* outer_path(Side side) const noexcept { return _side_data[size_t(side)].path; }
  BL_INLINE_NODEBUG BLPath* inner_path(Side side) const noexcept { return _side_data[size_t(opposite_side(side))].path; }

  BL_INLINE_NODEBUG PathAppender& a_out() noexcept { return a_side().appender; }
  BL_INLINE_NODEBUG PathAppender& b_out() noexcept { return b_side().appender; }

  BL_INLINE_NODEBUG PathAppender& outer_appender(Side side) noexcept { return _side_data[size_t(side)].appender; }
  BL_INLINE_NODEBUG PathAppender& inner_appender(Side side) noexcept { return _side_data[size_t(opposite_side(side))].appender; }

  BL_NOINLINE BLResult _ensure_would_grow(size_t a_byte_size, size_t b_byte_size) {
    return bl_combine_result(a_ensure(a_byte_size), b_ensure(b_byte_size));
  }

  BL_INLINE BLResult a_ensure(size_t a_byte_size) noexcept { return a_out().ensure(a_path(), a_byte_size); }
  BL_INLINE BLResult b_ensure(size_t b_byte_size) noexcept { return b_out().ensure(b_path(), b_byte_size); }

  BL_INLINE BLResult ensure_both(size_t a_byte_size, size_t b_byte_size) noexcept {
    if (BL_LIKELY(axl::bool_and(a_out().remaining_byte_size() >= a_byte_size,
                                b_out().remaining_byte_size() >= b_byte_size))) {
      return BL_SUCCESS;
    }
    else {
      return _ensure_would_grow(a_byte_size, b_byte_size);
    }
  }

  BL_INLINE BLResult ensure_both(size_t byte_size) noexcept {
    return ensure_both(byte_size, byte_size);
  }

  BL_NOINLINE BLResult stroke(BLPathStrokeSinkFunc sink, void* user_data) noexcept {
    size_t estimated_byte_size = (32u + _input_vertex_count * 3u) * (sizeof(BLPoint) + 1u);

    while (_input_iter.has_command()) {
      BLPathPosition figure_start = _input_iter.position_from(_input_beginning);
      if (BL_UNLIKELY(!_input_iter.is_move_to())) {
        if (!_input_iter.is_close()) {
          return bl_make_error(BL_ERROR_CORRUPTED_PATH_DATA);
        }

        // TODO: Is this necessary?
        _input_iter.advance<BL_PATH_CMD_CLOSE>();
        continue;
      }

      side_data(Side::kA).figure_pos.vertex_offset = side_data(Side::kA).path->vertex_count();
      side_data(Side::kA).figure_pos.control_offset = side_data(Side::kA).path->control_data_size();

      b_path()->clear();
      c_path()->clear();

      BL_PROPAGATE(a_out().begin_append(a_path(), estimated_byte_size));
      BL_PROPAGATE(b_out().begin_append(b_path(), estimated_byte_size));

      BLPoint poly_pts[4];
      size_t poly_size;

      _p0 = _input_iter.vertex(0);
      _input_iter.advance<BL_PATH_CMD_MOVE>();

      _p_initial = _p0;
      _flags = 0;

      // Content of the figure.
      while (_input_iter.has_command()) {
        BL_PROPAGATE(ensure_both(kStrokeMaxJoinDataSize));

        BLPoint p1; // Next line-to or control point.
        BLPoint v1; // Vector of `p1 - _p0`.
        BLPoint v2; // Vector of `p2 - _p1` (if any).
        BLPoint n1; // Unit normal of `v1`.

        Conic c0, c1;
        double t;

        if (_input_iter.is_line_to()) {
          // Line command, collinear curve converted to line or close of the figure.
          p1 = _input_iter.vertex(0);
          _input_iter.advance<BL_PATH_CMD_LINE>();
LineTo:
          v1 = p1 - _p0;
          if (magnitude_squared(v1) < kProcessingDistanceEpsilonSq) {
            continue;
          }

          n1 = normal(unit_vector(v1));
          if (!is_open()) {
            BL_PROPAGATE(open_line_to(p1, n1));
            continue;
          }

          for (;;) {
            BL_PROPAGATE(join_line_to(p1, n1));

            if (!_input_iter.has_command()) {
              break;
            }

            BL_PROPAGATE(ensure_both(kStrokeMaxJoinDataSize));
            p1 = _input_iter.vertex(0);

            if (!_input_iter.is_line_to()) {
              break;
            }

            _input_iter.advance<BL_PATH_CMD_LINE>();
            v1 = p1 - _p0;
            if (magnitude_squared(v1) < kProcessingDistanceEpsilonSq) {
              break;
            }

            n1 = normal(unit_vector(v1));
          }
          continue;
        }

        if (_input_iter.is_quad_to()) {
          // Quadratic curve segment.
          c0[0] = _p0;
          c0[1] =_input_iter.vertex(0);
          c0[2] =_input_iter.vertex(1);
          _input_iter.advance<BL_PATH_CMD_QUAD>();

          v1 = c0[1] - c0[0];
          v2 = c0[2] - c0[1];
          n1 = normal(unit_vector(v1));

          double cm = cross(v2, v1);
          if (bl_abs(cm) <= kCurveCollinearityEpsilon) {
            if (dot(v1, v2) < 0.0) {
              BL_PROPAGATE(offset_quad_degenerate(c0[0], c0[1], c0[2]));
              continue;
            }
            p1 = c0[2];
            goto LineTo;
          }

          // Very small curve segment => straight line.
          if (magnitude_squared(v1) < kProcessingDistanceEpsilonSq || magnitude_squared(v2) < kProcessingDistanceEpsilonSq) {
            p1 = c0[2];
            goto LineTo;
          }
          else {
            t = 1.0;
            goto OffsetQuad;
          }
        }

        if (_input_iter.is_conic_to()) {
          c1.assign_curve(_p0, _input_iter.vertex(0), _input_iter.vertex(1), _input_iter.weight());
          _input_iter.advance<BL_PATH_CMD_CONIC>();

          v1 = c1[1] - c1[0];
          v2 = c1[2] - c1[1];

          if (axl::bool_or(is_zero(v1), is_zero(v2))) {
            p1 = c1[2];
            goto LineTo;
          }

          n1 = normal(unit_vector(v1));
          t = conic_step_simplify_parameter(v1, v2, c1.w, double(_approx.simplify_tolerance) * 4.0);

          if (t < Math::kCurveMinT || t > Math::kCurveMaxT) {
            t = 1.0;
            c0 = c1;
          }
          else {
            conic_split(c1, c0, c1, t);
          }

OffsetQuad:
          if (!is_open()) {
            BL_PROPAGATE(open_curve(n1));
          }
          else {
            BL_PROPAGATE(join_curve(n1));
          }

          for (;;) {
            double new_t = t / (1.0 - t);
            BL_PROPAGATE(offset_quad(quad_ref(c0.vtx)));

            if (t >= 1.0) {
              break;
            }

            if (new_t < Math::kCurveMinT || new_t > Math::kCurveMaxT) {
              t = 1.0;
              c0 = c1;
            }
            else {
              t = new_t;
              conic_split(c1, c0, c1, t);
            }
          }
          continue;
        }

        if (_input_iter.is_cubic_to()) {
          // Cubic curve segment.
          BLPoint p[7];
          p[0] = _p0;
          p[1] = _input_iter.vertex(0);
          p[2] = _input_iter.vertex(1);
          p[3] = _input_iter.vertex(2);
          _input_iter.advance<BL_PATH_CMD_CUBIC>();

          int cusp = 0;

          // Check if the curve is flat enough to be potentially degenerate.
          if (is_cubic_flat(cubic_ref(p), kStrokeDegenerateFlatness)) {
            double dot1 = dot(p[0] - p[1], p[3] - p[1]);
            double dot2 = dot(p[0] - p[2], p[3] - p[2]);

            if (!(dot1 < 0.0) || !(dot2 < 0.0)) {
              // Rotate all points to x-axis.
              const BLPoint r = cubic_start_tangent(cubic_ref(p));

              double r1 = dot(p[1] - p[0], r);
              double r2 = dot(p[2] - p[0], r);
              double r3 = dot(p[3] - p[0], r);

              // Stationary points (folds/cusps) of the flattened curve are the roots of its tangential velocity: the
              // quadratic `A*t^2 + B*t + C` below. The naive closed form `(b +/- s) / A` is unstable here:
              // `A = 3*r1 - 3*r2 + r3` vanishes for symmetric flat cubics (e.g. P1 == P2 && P0 == P3), turning `1/A`
              // into infinity and the wanted root into a NaN, and `b - s` cancels for large coordinates.
              //
              // `quad_roots()` solves it in the numerically stable way (q = -0.5*(b + sign(b)*sqrt(d)); q/A & C/q).
              double ts[2];
              size_t ts_count = Math::quad_roots(ts,
                3.0 * r1 - 3.0 * r2 + r3,   // A
                2.0 * r2 - 4.0 * r1,        // B = -2 * (2*r1 - r2)
                r1,                         // C
                kStrokeCuspTThreshold, 1.0 - kStrokeCuspTThreshold);

              if (ts_count == 0) {
                p1 = p[3];
                goto LineTo;
              }

              // Offset the cusps (already sorted and clamped to the open (0, 1) interval).
              poly_size = 0;
              for (size_t ts_index = 0; ts_index < ts_count; ts_index++) {
                poly_pts[poly_size++] = cubic_evaluate(cubic_ref(p), ts[ts_index]);
              }

              poly_pts[poly_size++] = p[3];
              goto SmoothPolyTo;
            }
            else {
              p1 = p[3];
              goto LineTo;
            }
          }
          else {
            double tc, tl;
            get_cubic_inflection_parameter(cubic_ref(p), tc, tl);

            if (tl == 0.0 && tc > 0.0 && tc < 1.0) {
              cubic_split(cubic_ref(p), cubic_out(p), cubic_out(p + 3));
              cusp = 1;
            }
          }

          for (;;) {
            v1 = p[1] - _p0;
            if (is_zero(v1)) {
              v1 = p[2] - _p0;
            }
            n1 = normal(unit_vector(v1));

            if (!is_open()) {
              BL_PROPAGATE(open_curve(n1));
            }
            else if (cusp >= 0) {
              BL_PROPAGATE(join_curve(n1));
            }
            else {
              BL_PROPAGATE(join_cusp(n1));
            }

            BL_PROPAGATE(offset_cubic(p));
            if (cusp <= 0) {
              break;
            }

            BL_PROPAGATE(ensure_both(kStrokeMaxJoinDataSize));

            // Second part of the cubic after the cusp. We assign `-1` to `cusp` so we can call `join_cusp()` later.
            // This is a special join that we need in this case.
            cusp = -1;
            p[0] = p[3];
            p[1] = p[4];
            p[2] = p[5];
            p[3] = p[6];
          }
          continue;
        }

        // Either invalid command or close of the figure. If the figure is already closed it means that we have
        // already jumped to `LineTo` and we should terminate now. Otherwise we just encountered close or
        // something else which is not part of the current figure.
        if (is_closed()) {
          break;
        }

        if (!_input_iter.is_close()) {
          break;
        }

        // The figure is closed. We just jump to `LineTo` to minimize inlining expansion and
        // mark the figure as closed. Next time we terminate on `is_closed()` condition above.
        _flags |= kFlagIsClosed;
        p1 = _p_initial;
        goto LineTo;

        // Smooth POLY handler. This is a special case that happens when handling collinear curves that have
        // stationary points (points outside of their start/end point's bounding-box). Basically, a curve is
        // approximated with lines, and those lines are joined by round join, instead of any other join type.
SmoothPolyTo:
        BL_PROPAGATE(smooth_poly_to(poly_pts, poly_size));
      }

      // Don't emit anything if the figure has no points (and thus no direction).
      _input_iter.advance_vertex_and_control(0, size_t(is_closed()));
      if (!is_open()) {
        a_out().done(a_path());
        b_out().done(b_path());
        continue;
      }

      if (is_closed()) {
        // The figure is closed => the end result is two closed figures without caps. In this case only paths
        // A and B have a content, path C will be empty and should be thus ignored by the sink.

        // Allocate space for the end join and close command.
        BL_PROPAGATE(ensure_both(kStrokeMaxJoinDataSize + BLPath::Close::kByteSize));
        BL_PROPAGATE(join_end_point(_n_initial));

        a_out().close();
        b_out().close();
      }
      else {
        // The figure is open => the end result is a single figure with caps. The paths contain the following:
        //   A - Offset of the figure and end cap.
        //   B - Offset of the figure that MUST BE reversed.
        //   C - Start cap (not reversed).
        uint32_t start_cap = sanity_stroke_cap(_options.start_cap());
        uint32_t end_cap = sanity_stroke_cap(_options.end_cap());

        BL_PROPAGATE(a_ensure(cap_geometry_byte_size_table[end_cap]));
        BL_PROPAGATE(add_cap(a_out(), _p0, b_out().prev_vertex(), end_cap));

        PathAppender c_out;
        BL_PROPAGATE(c_out.begin_append(c_path(), size_t(cap_geometry_byte_size_table[start_cap]) + BLPath::LineTo::kByteSize));
        c_out.move_to(b_path()->vertex_data()[0]);
        BL_PROPAGATE(add_cap(c_out, _p_initial, a_path()->vertex_data()[side_data(Side::kA).figure_pos.vertex_offset], start_cap));
        c_out.done(c_path());
      }

      a_out().done(a_path());
      b_out().done(b_path());

      // Call the provided path-sink with resulting paths.
      BLPathRegion input_region{
        .start = figure_start,
        .end = _input_iter.position_from(_input_beginning)
      };
      BL_PROPAGATE(sink(a_path(), b_path(), c_path(), &input_region,  user_data));

      // Since we have appended vertices to the destination path already, don't over-estimate the next figure.
      // There are either no more figures to stroke or the engine would grow the existing path as needed. In
      // any case it's better if we don't grow immediately as there is a chance that the grow won't be necessary.
      estimated_byte_size = BLPath::LineTo::kByteSize * 2u;
    }

    return BL_SUCCESS;
  }

  // Opens a new figure with a line segment starting from the current point and ending at `p1`. The `n1` is a
  // normal calculated from a unit vector of `p1 - _p0`.
  //
  // This function can only be called after we have at least two vertices that form the line. These vertices
  // cannot be a single point as that would mean that we cannot calculate unit vector and then normal for the
  // offset. This must be handled before calling `open_line_to()`.
  //
  // NOTE: Path cannot be open when calling this function.
  BL_INLINE_IF_NOT_DEBUG BLResult open_line_to(const BLPoint& p1, const BLPoint& n1) noexcept {
    BL_ASSERT(!is_open());
    BLPoint w = n1 * d();

    a_out().move_to(_p0 + w);
    b_out().move_to(_p0 - w);

    _p0 = p1;
    _n0 = n1;
    _n_initial = n1;

    a_out().line_to(_p0 + w);
    b_out().line_to(_p0 - w);

    _flags |= kFlagIsOpen;
    return BL_SUCCESS;
  }

  // Joins line-to segment described by `p1` point and `n1` normal.
  BL_INLINE_IF_NOT_DEBUG BLResult join_line_to(const BLPoint& p1, const BLPoint& n1) noexcept {
    if (test_collinear_normals(_n0, n1)) {
      // Collinear case - patch the previous point(s) if they connect lines.
      a_out()._revert_if_line();
      b_out()._revert_if_line();

      BLPoint w1 = n1 * d();
      a_out().line_to(p1 + w1);
      b_out().line_to(p1 - w1);
    }
    else {
      Side side = side_from_normals(_n0, n1);
      BLPoint m = _n0 + n1;
      BLPoint k = m * d2(side) / magnitude_squared(m);
      BLPoint w1 = n1 * d(side);

      size_t miter_flag = 0;

      if (side == Side::kA) {
        PathAppender& outer = a_out();
        outer_join(outer, n1, w1, k, d(side), d2(side), miter_flag);
        outer._revert(miter_flag, miter_flag);
        outer.line_to(p1 + w1);

        PathAppender& inner = b_out();
        inner_join_line_to(inner, _p0 - w1, p1 - w1, _p0 - k);
        inner.line_to(p1 - w1);
      }
      else {
        PathAppender& outer = b_out();
        outer_join(outer, n1, w1, k, d(side), d2(side), miter_flag);
        outer._revert(miter_flag, miter_flag);
        outer.line_to(p1 + w1);

        PathAppender& inner = a_out();
        inner_join_line_to(inner, _p0 - w1, p1 - w1, _p0 - k);
        inner.line_to(p1 - w1);
      }
    }

    _p0 = p1;
    _n0 = n1;
    return BL_SUCCESS;
  }

  // Opens a new figure at the current point `_p0`. The first vertex (MOVE) is calculated by offsetting `_p0`
  // by the given unit normal `n0`.
  //
  // NOTE: Path cannot be open when calling this function.
  BL_INLINE_IF_NOT_DEBUG BLResult open_curve(const BLPoint& n0) noexcept {
    BL_ASSERT(!is_open());
    BLPoint w = n0 * d();

    a_out().move_to(_p0 + w);
    b_out().move_to(_p0 - w);

    _n0 = n0;
    _n_initial = n0;

    _flags |= kFlagIsOpen;
    return BL_SUCCESS;
  }

  // Joins curve-to segment.
  BL_INLINE_IF_NOT_DEBUG BLResult join_curve(const BLPoint& n1) noexcept {
    // Collinear case - do nothing.
    if (test_collinear_normals(_n0, n1)) {
      return BL_SUCCESS;
    }

    Side side = side_from_normals(_n0, n1);
    BLPoint m = _n0 + n1;
    BLPoint k = m * d2(side) / magnitude_squared(m);
    BLPoint w1 = n1 * d(side);
    size_t dummy_miter_flag;

    outer_join(outer_appender(side), n1, w1, k, d(side), d2(side), dummy_miter_flag);
    inner_join_curve_to(inner_appender(side), _p0 - w1);

    _n0 = n1;
    return BL_SUCCESS;
  }

  BL_INLINE_IF_NOT_DEBUG BLResult join_cusp(const BLPoint& n1) noexcept {
    Side side = side_from_normals(_n0, n1);
    BLPoint w1 = n1 * d(side);

    dull_round_join(outer_appender(side), d(side), d2(side), w1);
    inner_appender(side).line_to(_p0 - w1);

    _n0 = n1;
    return BL_SUCCESS;
  }

  BL_INLINE_IF_NOT_DEBUG BLResult join_cusp_and_line_to(const BLPoint& n1, const BLPoint& p1) noexcept {
    Side side = side_from_normals(_n0, n1);
    BLPoint w1 = n1 * d(side);

    PathAppender& outer = outer_appender(side);
    dull_round_join(outer, d(side), d2(side), w1);
    outer.line_to(p1 + w1);

    PathAppender& inner = inner_appender(side);
    inner.line_to(_p0 - w1);
    inner.line_to(p1 - w1);

    _n0 = n1;
    _p0 = p1;
    return BL_SUCCESS;
  }

  BL_INLINE_IF_NOT_DEBUG BLResult smooth_poly_to(const BLPoint* poly, size_t count) noexcept {
    BL_ASSERT(count >= 2);

    BLPoint p1 = poly[0];
    BLPoint v1 = p1 - _p0;
    if (magnitude_squared(v1) < kProcessingDistanceEpsilonSq) {
      return BL_SUCCESS;
    }

    BLPoint n1 = normal(unit_vector(v1));
    if (!is_open()) {
      BL_PROPAGATE(open_line_to(p1, n1));
    }
    else {
      BL_PROPAGATE(join_line_to(p1, n1));
    }

    // We have already ensured vertices for `open_line_to()` and `join_line_to()`,
    // however, we need more vertices for consecutive joins and line segments.
    size_t required_capacity = (count - 1u) * kStrokeMaxJoinDataSize;
    BL_PROPAGATE(ensure_both(required_capacity));

    for (size_t i = 1; i < count; i++) {
      p1 = poly[i];
      v1 = p1 - _p0;
      if (magnitude_squared(v1) < kProcessingDistanceEpsilonSq) {
        continue;
      }

      n1 = normal(unit_vector(v1));
      BL_PROPAGATE(join_cusp_and_line_to(n1, p1));
    }

    return BL_SUCCESS;
  }

  // Joins end point that is only applied to closed figures.
  BL_INLINE_IF_NOT_DEBUG BLResult join_end_point(const BLPoint& n1) noexcept {
    if (_n0 == n1) {
      // Collinear case - patch the previous point(s) if they connect lines.
      a_out()._revert_if_line();
      b_out()._revert_if_line();
      return BL_SUCCESS;
    }

    Side side = side_from_normals(_n0, n1);
    BLPoint m = _n0 + n1;
    BLPoint w1 = n1 * d(side);
    BLPoint k = m * d2(side) / magnitude_squared(m);

    size_t miter_flag = 0;

    SideData& outer_side = side_data(side);
    outer_join(outer_side.appender, n1, w1, k, d(side), d2(side), miter_flag);

    // Shift the start point to be at the miter intersection and remove the
    // Line from the intersection to the start of the path if miter was applied.
    if (miter_flag) {
      BLPoint* vertex_start = PathDetail::get_vertex_data(outer_side.path) + side_data(side).figure_pos.vertex_offset;
      uint8_t* control_start = PathDetail::get_control_data_rev(outer_side.path) - side_data(side).figure_pos.control_offset;

      if (control_start[-2] == uint8_t(BL_PATH_CMD_LINE)) {
        outer_side.appender._revert(1u, 1u);
        vertex_start[0] = outer_side.appender.prev_vertex();
        outer_side.appender._revert_if_line();
      }
    }

    SideData& inner_side = side_data(opposite_side(side));
    uint8_t* inner_control_ptr = PathDetail::get_control_data_rev(inner_side.path) - inner_side.figure_pos.control_offset - 2u;

    if (PathDetail::is_single_vertex_cmd(BLPathCmd(inner_control_ptr[0]))) {
      BLPoint* inner_data = PathDetail::get_vertex_data(inner_side.path);
      inner_join_end_point(inner_side.appender,
        inner_data[inner_side.figure_pos.vertex_offset + 0u],
        inner_data[inner_side.figure_pos.vertex_offset + 1u], _p0 - k);
    }

    return BL_SUCCESS;
  }

  BL_INLINE_IF_NOT_DEBUG void inner_join_curve_to(PathAppender& out, const BLPoint& p1) noexcept {
    out.line_to(_p0);
    out.line_to(p1);
  }

  BL_INLINE_IF_NOT_DEBUG void inner_join_line_to(PathAppender& out, const BLPoint& line_p0, const BLPoint& line_p1, const BLPoint& inner_pt) noexcept {
    if (out.prev_command() == BL_PATH_CMD_LINE && test_inner_join_intersection(out._vertex_ptr[-2], out._vertex_ptr[-1], line_p0, line_p1, inner_pt)) {
      out.set_prev_vertex(inner_pt);
    }
    else {
      out.line_to(_p0);
      out.line_to(line_p0);
    }
  }

  BL_INLINE_IF_NOT_DEBUG void inner_join_end_point(PathAppender& out, BLPoint& line_p0, const BLPoint& line_p1, const BLPoint& inner_pt) noexcept {
    if (out.prev_command() == BL_PATH_CMD_LINE && test_inner_join_intersection(out._vertex_ptr[-2], out._vertex_ptr[-1], line_p0, line_p1, inner_pt)) {
      line_p0 = inner_pt;
      out._revert(1u, 1u);
    }
    else {
      out.line_to(_p0);
      out.line_to(line_p0);
    }
  }

  // Calculates outer join to `pb`.
  BL_INLINE_IF_NOT_DEBUG BLResult outer_join(PathAppender& appender, const BLPoint& n1, const BLPoint& w1, const BLPoint& k, double d, double d2, size_t& miter_flag) noexcept {
    size_t prev_is_line = size_t(appender.prev_command() == BL_PATH_CMD_LINE);
    BLPoint pb = _p0 + w1;

    if (magnitude_squared(k) <= _miter_limit_sq) {
      // Miter condition is met.
      appender._revert(prev_is_line, prev_is_line);
      appender.line_to(_p0 + k);
      appender.line_to(pb);

      miter_flag = 1;
      return BL_SUCCESS;
    }

    if (_join_type == BL_STROKE_JOIN_MITER_CLIP) {
      double b2 = bl_abs(cross(k, _n0));

      // Avoid degenerate cases and NaN.
      if (b2 > 0) {
        b2 = b2 * _miter_limit / magnitude(k);
      }
      else {
        b2 = _miter_limit;
      }

      appender._revert(prev_is_line, prev_is_line);
      appender.line_to(Math::madd(-b2, normal(_n0), Math::madd(d, _n0, _p0)));
      appender.line_to(Math::madd( b2, normal( n1), Math::madd(d, n1, _p0)));

      miter_flag = 1;
      appender.line_to(pb);
      return BL_SUCCESS;
    }

    if (_join_type == BL_STROKE_JOIN_ROUND) {
      BLPoint pa = appender._vertex_ptr[-1];

      if (dot(_n0, n1) < 0.0) {
        // Obtuse angle (2 segments).
        BLPoint nm = normal(unit_vector(pb - pa));
        BLPoint nk = _n0 + nm;
        nk = d2 * nk / magnitude_squared(nk);

        BLPoint pc1 = _p0 + nk;
        BLPoint pp1 = Math::madd(d, nm, _p0);
        BLPoint pc2 = lerp(pc1, pp1, 2.0);
        double w = conic_weight_from_vectors(_p0, pc1, pp1);

        appender.conic_to(pc1, pp1, w);
        appender.conic_to(pc2, pb, w);
      }
      else {
        // Acute angle (1 segment).
        BLPoint nk = _n0 + n1;
        nk = d2 * nk / magnitude_squared(nk);

        BLPoint pc = _p0 + nk;
        double w = conic_weight_from_vectors(_p0, pc, pb);

        appender.conic_to(pc, pb, w);
      }

      return BL_SUCCESS;
    }

    // Bevel or unknown `_join_type`.
    appender.line_to(pb);
    return BL_SUCCESS;
  }

  // Calculates round join to `pb` (dull angle), only used by offsetting cusps.
  BL_INLINE_IF_NOT_DEBUG BLResult dull_round_join(PathAppender& out, double d, double d2, const BLPoint& w1) noexcept {
    BLPoint pa = out.prev_vertex();
    BLPoint pb = _p0 + w1;
    BLPoint n2 = normal(unit_vector(pb - pa));

    if (!Math::is_finite(n2.x)) {
      return BL_SUCCESS;
    }

    BLPoint nm = normal(unit_vector(pb - pa));
    BLPoint k = _n0 + nm;
    k = d2 * k / magnitude_squared(k);

    BLPoint pc1 = _p0 + k;
    BLPoint pp1 = Math::madd(d, nm, _p0);
    BLPoint pc2 = lerp(pc1, pp1, 2.0);
    double w = conic_weight_from_vectors(_p0, pc1, pp1);

    out.conic_to(pc1, pp1, w);
    out.conic_to(pc2, pb, w);
    return BL_SUCCESS;
  }

  // Pass points by value so the caller's shared quad/conic temporary can stay in registers.
  BL_NOINLINE BLResult offset_quad_degenerate(BLPoint p0, BLPoint p1, BLPoint p2) noexcept {
    const BLPoint vertices[3] = {p0, p1, p2};
    QuadRef quad(vertices);
    BLPoint v0 = quad[1] - quad[0];
    BLPoint v1 = quad[2] - quad[1];
    double v0_dot_v1 = dot(v0, v1);

    BL_PROPAGATE(ensure_both(kStrokeMaxJoinDataSize));

    if (v0_dot_v1 < 0.0) {
      // Collinear reversal: keep the stationary point and join its two sides with a round cusp.
      double v0_sq = magnitude_squared(v0);
      double t = v0_sq / (v0_sq - v0_dot_v1);
      if (t > 0.0 && t < 1.0) {
        BLPoint poly[2] = {quad_evaluate_precise(quad, t), quad[2]};
        if (magnitude_squared(poly[0] - _p0) >= kProcessingDistanceEpsilonSq) {
          return smooth_poly_to(poly, 2);
        }
      }
    }

    BLPoint v = quad[2] - _p0;
    if (magnitude_squared(v) < kProcessingDistanceEpsilonSq) {
      return BL_SUCCESS;
    }

    BLPoint n = normal(unit_vector(v));
    return is_open() ? join_line_to(quad[2], n) : open_line_to(quad[2], n);
  }

  template<bool kCheckDegenerate = false>
  BL_INLINE_IF_NOT_DEBUG BLResult offset_quad(QuadRef quad) noexcept {
    QuadDerivativeCoefficients dc = quad_derivative_coefficients(quad);

    // Cubic approximation can introduce collinear reversals in otherwise regular curves. Classify these
    // before solving offset cusps, reusing the derivative cross product needed by quad_offset_cusp_ts().
    // Its scale is four times the control-edge cross product used to classify quadratic path commands.
    if constexpr (kCheckDegenerate) {
      if (BL_UNLIKELY(bl_abs(cross(dc.b, dc.a)) <= 4.0 * kCurveCollinearityEpsilon)) {
        return offset_quad_degenerate(quad[0], quad[1], quad[2]);
      }
    }

    double ts[3];
    size_t tn = quad_offset_cusp_ts(dc, d(), ts, Math::kCurveMinT, Math::kCurveMaxT);
    ts[tn++] = 1.0;

    QuadCurveTsIter iter(quad, Span<const double>(ts, tn));
    double m = double(_approx.offset_parameter);

    do {
      for (;;) {
        constexpr size_t kRequiredCapacity = BLPath::QuadTo::kByteSize;
        BL_PROPAGATE(ensure_both(kRequiredCapacity));

        double t = quad_parameter_at_angle(iter.part, m);
        if (!(t > kQuadOffsetEpsilonT && t < 1.0 - kQuadOffsetEpsilonT)) {
          // A split at t=1 only copies the current curve. Offset it directly; next() only needs its endpoint.
          if (iter.part[0] != iter.part[2]) {
            offset_quad_simple(iter.part[0], iter.part[1], iter.part[2]);
          }
          break;
        }

        BLPoint part[3];
        BLPoint prev_control = iter.part[1];
        quad_split(quad_ref(iter.part), quad_out(part), quad_out(iter.part), t);
        if (part[0] != part[2]) {
          offset_quad_simple(part[0], part[1], part[2]);
        }
        else if (iter.part[1] == prev_control) {
          // A collapsed prefix has not moved the start point. If the control point has not moved either,
          // the remainder is unchanged and subdivision would repeat the same curve forever.
          return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
        }
      }
    } while (iter.next());

    return BL_SUCCESS;
  }

  BL_INLINE_IF_NOT_DEBUG void offset_quad_simple(const BLPoint& p0, const BLPoint& p1, const BLPoint& p2) noexcept {
    BLPoint v0 = p1 - p0;
    BLPoint v1 = p2 - p1;

    BLPoint m0 = normal(unit_vector(p0 != p1 ? v0 : v1));
    BLPoint m2 = normal(unit_vector(p1 != p2 ? v1 : v0));

    _p0 = p2;
    _n0 = m2;

    BLPoint m = m0 + m2;
    BLPoint k1 = m * d2() / magnitude_squared(m);
    BLPoint k2 = m2 * d();

    a_out().quad_to(p1 + k1, p2 + k2);
    b_out().quad_to(p1 - k1, p2 - k2);
  }

  struct CubicApproximateSink {
    Stroker& _stroker;

    BL_INLINE CubicApproximateSink(Stroker& stroker) noexcept
      : _stroker(stroker) {}

    BL_INLINE BLResult operator()(QuadRef quad) const noexcept {
      return _stroker.offset_quad<true>(quad);
    }
  };

  BL_INLINE_IF_NOT_DEBUG BLResult offset_cubic(const BLPoint bez[4]) noexcept {
    CubicApproximateSink sink(*this);
    return approximate_cubic_with_quads(cubic_ref(bez), double(_approx.simplify_tolerance), sink);
  }

  BL_INLINE_IF_NOT_DEBUG BLResult add_cap(PathAppender& out, BLPoint pivot, BLPoint p1, uint32_t cap_type) noexcept {
    BLPoint p0 = out.prev_vertex();
    BLPoint q = normal(p1 - p0) * 0.5;

    switch (cap_type) {
      case BL_STROKE_CAP_BUTT:
      default: {
        out.line_to(p1);
        break;
      }

      case BL_STROKE_CAP_SQUARE: {
        out.line_to(p0 + q);
        out.line_to(p1 + q);
        out.line_to(p1);
        break;
      }

      case BL_STROKE_CAP_ROUND: {
        out.arc_quadrant_to(p0 + q, pivot + q);
        out.arc_quadrant_to(p1 + q, p1);
        break;
      }

      case BL_STROKE_CAP_ROUND_REV: {
        out.line_to(p0 + q);
        out.arc_quadrant_to(p0, pivot);
        out.arc_quadrant_to(p1, p1 + q);
        out.line_to(p1);
        break;
      }

      case BL_STROKE_CAP_TRIANGLE: {
        out.line_to(pivot + q);
        out.line_to(p1);
        break;
      }

      case BL_STROKE_CAP_TRIANGLE_REV: {
        out.line_to(p0 + q);
        out.line_to(pivot);
        out.line_to(p1 + q);
        out.line_to(p1);
        break;
      }
    }

    return BL_SUCCESS;
  }
};

// bl::Path - Stroke - Interface
// =============================

BLResult stroke_path(
  const BLPathView& input,
  const BLStrokeOptions& options,
  const BLApproximationOptions& approx,
  BLPath& a,
  BLPath& b,
  BLPath& c,
  BLPathStrokeSinkFunc sink, void* user_data
) noexcept {
  BLSpan<const float> dash_array = options.dash_array();

  if (dash_array.size() >= 2u) {
    BLPath dashed;
    BL_PROPAGATE(dash_path(input, options, approx, dashed, nullptr, nullptr));
    return Stroker(dashed.view(), options, approx, &a, &b, &c).stroke(sink, user_data);
  }
  else {
    return Stroker(input, options, approx, &a, &b, &c).stroke(sink, user_data);
  }
}

} // {bl::Geometry}

BL_DIAGNOSTIC_POP
