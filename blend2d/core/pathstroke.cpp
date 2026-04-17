// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api-build_p.h>
#include <blend2d/core/matrix_p.h>
#include <blend2d/core/path_p.h>
#include <blend2d/core/pathstroke_p.h>
#include <blend2d/geometry/bezier_p.h>
#include <blend2d/support/lookuptable_p.h>
#include <blend2d/support/math_p.h>

#include <limits>
#include <utility>

namespace bl {
namespace PathInternal {

// bl::Path - Stroke - Constants
// =============================

static constexpr double kStrokeLengthEpsilon = 1e-10;
static constexpr double kStrokeLengthEpsilonSq = Math::square(kStrokeLengthEpsilon);

static constexpr double kStrokeJoinEpsilon = 1e-10;
static constexpr double kStrokeOneOverSqrt2 = 0.70710678118654752440;

static constexpr uint32_t kStrokeTangentRecursiveLimit = 15u;
static constexpr uint32_t kStrokeCubicRecursiveLimit = 24u;
static constexpr uint32_t kStrokeConicRecursiveLimit = 33u;
static constexpr uint32_t kStrokeQuadRecursiveLimit = 33u;

static constexpr size_t kStrokeMaxJoinVertices = 9;
static constexpr size_t kStrokeMaxJoinConics = 2;
static constexpr size_t kStrokeCircleVertices = 10;
static constexpr size_t kStrokeCircleConics = 4;

// bl::Path - Stroke - Tables
// ==========================

struct CapVertexCountGen {
  static constexpr uint8_t value(size_t cap) noexcept {
    return BLStrokeCap(cap) == BL_STROKE_CAP_SQUARE       ? 3 :
           BLStrokeCap(cap) == BL_STROKE_CAP_ROUND        ? 4 :
           BLStrokeCap(cap) == BL_STROKE_CAP_ROUND_REV    ? 6 :
           BLStrokeCap(cap) == BL_STROKE_CAP_TRIANGLE     ? 2 :
           BLStrokeCap(cap) == BL_STROKE_CAP_TRIANGLE_REV ? 4 :
           BLStrokeCap(cap) == BL_STROKE_CAP_BUTT         ? 1 : 0;
  }
};

struct CapConicCountGen {
  static constexpr uint8_t value(size_t cap) noexcept {
    return BLStrokeCap(cap) == BL_STROKE_CAP_ROUND     ? 2 :
           BLStrokeCap(cap) == BL_STROKE_CAP_ROUND_REV ? 2 : 0;
  }
};

static constexpr auto cap_vertex_count_table =
  make_lookup_table<uint8_t, BL_STROKE_CAP_MAX_VALUE + 1, CapVertexCountGen>();

static constexpr auto cap_conic_count_table =
  make_lookup_table<uint8_t, BL_STROKE_CAP_MAX_VALUE + 1, CapConicCountGen>();

// bl::Path - Stroke - Utilities
// =============================

enum class StrokeType : int {
  kOuter = 1,
  kInner = -1
};

enum class AngleType : uint32_t {
  kNearly180,
  kSharp,
  kShallow,
  kNearlyLine
};

enum class ReductionType : uint32_t {
  kPoint,
  kLine,
  kQuad,
  kDegenerate,
  kDegenerate2,
  kDegenerate3
};

enum class ResultType : uint32_t {
  kSplit,
  kDegenerate,
  kQuad
};

enum class IntersectRayType : uint32_t {
  kCtrlPt,
  kResultType
};

struct QuadConstruct {
  BLPoint quad[3];
  BLPoint tangent_start;
  BLPoint tangent_end;
  double start_t;
  double mid_t;
  double end_t;
  bool start_set;
  bool end_set;
  bool opposite_tangents;

  BL_INLINE bool init(double start, double end) noexcept {
    start_t = start;
    mid_t = (start + end) * 0.5;
    end_t = end;
    start_set = false;
    end_set = false;
    opposite_tangents = false;
    return start_t < mid_t && mid_t < end_t;
  }

  BL_INLINE bool init_with_start(const QuadConstruct& parent) noexcept {
    if (!init(parent.start_t, parent.mid_t))
      return false;

    quad[0] = parent.quad[0];
    tangent_start = parent.tangent_start;
    start_set = true;
    return true;
  }

  BL_INLINE bool init_with_end(const QuadConstruct& parent) noexcept {
    if (!init(parent.mid_t, parent.end_t))
      return false;

    quad[2] = parent.quad[2];
    tangent_end = parent.tangent_end;
    end_set = true;
    return true;
  }
};

static BL_INLINE bool is_nearly_zero(double v) noexcept { return bl_abs(v) <= kStrokeJoinEpsilon; }

static BL_INLINE bool is_clockwise(const BLPoint& before, const BLPoint& after) noexcept {
  return before.x * after.y > before.y * after.x;
}

static BL_INLINE AngleType dot_to_angle_type(double dot) noexcept {
  if (dot >= 0.0)
    return is_nearly_zero(1.0 - dot) ? AngleType::kNearlyLine : AngleType::kShallow;
  else
    return is_nearly_zero(1.0 + dot) ? AngleType::kNearly180 : AngleType::kSharp;
}

static BL_INLINE uint32_t sanitize_stroke_cap(uint32_t cap) noexcept {
  return cap <= BL_STROKE_CAP_MAX_VALUE ? cap : BL_STROKE_CAP_BUTT;
}

static BL_INLINE uint32_t sanitize_stroke_join(uint32_t join) noexcept {
  return join <= BL_STROKE_JOIN_MAX_VALUE ? join : BL_STROKE_JOIN_MITER_CLIP;
}

static BL_INLINE bool is_round_join(uint32_t join) noexcept {
  return join == BL_STROKE_JOIN_ROUND || join == BL_STROKE_JOIN_MITER_ROUND;
}

static BL_INLINE bool is_bevel_join(uint32_t join) noexcept {
  return join == BL_STROKE_JOIN_BEVEL || join == BL_STROKE_JOIN_MITER_BEVEL;
}

static BL_INLINE BLPoint set_length(const BLPoint& v, double len) noexcept {
  double v_len = Geometry::magnitude(v);
  if (!(v_len > 0.0))
    return BLPoint(0.0, 0.0);
  return v * (len / v_len);
}

static BL_INLINE bool degenerate_vector(const BLPoint& v) noexcept {
  return Geometry::magnitude_squared(v) <= kStrokeLengthEpsilonSq;
}

static BL_INLINE bool set_normal_unitnormal(const BLPoint& before, const BLPoint& after, double scale, double radius, BLPoint* normal, BLPoint* unit_normal) noexcept {
  BLPoint tangent = (after - before) * scale;
  double tangent_len_sq = Geometry::magnitude_squared(tangent);
  if (!(tangent_len_sq > kStrokeLengthEpsilonSq))
    return false;

  BLPoint unit_tangent = tangent / Math::sqrt(tangent_len_sq);
  *unit_normal = Geometry::normal(unit_tangent);
  *normal = *unit_normal * radius;
  return true;
}

static BL_INLINE bool set_normal_unitnormal(const BLPoint& tangent, double radius, BLPoint* normal, BLPoint* unit_normal) noexcept {
  double tangent_len_sq = Geometry::magnitude_squared(tangent);
  if (!(tangent_len_sq > kStrokeLengthEpsilonSq))
    return false;

  BLPoint unit_tangent = tangent / Math::sqrt(tangent_len_sq);
  *unit_normal = Geometry::normal(unit_tangent);
  *normal = *unit_normal * radius;
  return true;
}

static BL_INLINE bool has_non_butt_caps(const BLStrokeOptions& options) noexcept {
  return sanitize_stroke_cap(options.start_cap) != BL_STROKE_CAP_BUTT ||
         sanitize_stroke_cap(options.end_cap) != BL_STROKE_CAP_BUTT;
}

static BL_INLINE bool is_zero_length_since_point(const BLPath& path, size_t start_point) noexcept {
  size_t point_count = path.size();
  if (point_count - start_point < 2)
    return true;

  const BLPoint* pts = path.vertex_data() + start_point;
  const BLPoint first = pts[0];
  for (size_t i = 1; i < point_count - start_point; i++) {
    if (pts[i] != first)
      return false;
  }
  return true;
}

static BL_INLINE double point_to_line_distance_sq(const BLPoint& pt, const BLPoint& line_start, const BLPoint& line_end) noexcept {
  BLPoint dxy = line_end - line_start;
  BLPoint ab0 = pt - line_start;

  double numer = Geometry::dot(dxy, ab0);
  double denom = Geometry::dot(dxy, dxy);
  double t = numer / denom;

  if (t >= 0.0 && t <= 1.0) {
    BLPoint hit = line_start + dxy * t;
    return Geometry::magnitude_squared(hit - pt);
  }
  else {
    return Geometry::magnitude_squared(pt - line_start);
  }
}

static BL_INLINE double point_to_tangent_line_distance_sq(const BLPoint& pt, const BLPoint& line_start, const BLPoint& tangent) noexcept {
  BLPoint ab0 = pt - line_start;

  double numer = Geometry::dot(tangent, ab0);
  double denom = Geometry::dot(tangent, tangent);
  double t = numer / denom;

  if (t >= 0.0 && t <= 1.0) {
    BLPoint hit = line_start + tangent * t;
    return Geometry::magnitude_squared(hit - pt);
  }
  else {
    return Geometry::magnitude_squared(pt - line_start);
  }
}

static BL_INLINE bool points_within_dist(const BLPoint& a, const BLPoint& b, double limit) noexcept {
  return Geometry::magnitude_squared(a - b) <= limit * limit;
}

static BL_INLINE bool quad_in_line(const BLPoint quad[3]) noexcept {
  double pt_max = -1.0;
  int outer1 = 0;
  int outer2 = 1;

  for (int i = 0; i < 2; i++) {
    for (int j = i + 1; j < 3; j++) {
      BLPoint diff = quad[j] - quad[i];
      double test_max = bl_max(bl_abs(diff.x), bl_abs(diff.y));
      if (pt_max < test_max) {
        outer1 = i;
        outer2 = j;
        pt_max = test_max;
      }
    }
  }

  int mid = outer1 ^ outer2 ^ 3;
  double line_slop = pt_max * pt_max * 0.000005;
  return point_to_line_distance_sq(quad[mid], quad[outer1], quad[outer2]) <= line_slop;
}

static BL_INLINE bool conic_in_line(const BLPoint conic[4]) noexcept {
  return quad_in_line(conic);
}

static BL_INLINE bool cubic_in_line(const BLPoint cubic[4]) noexcept {
  double pt_max = -1.0;
  int outer1 = 0;
  int outer2 = 1;

  for (int i = 0; i < 3; i++) {
    for (int j = i + 1; j < 4; j++) {
      BLPoint diff = cubic[j] - cubic[i];
      double test_max = bl_max(bl_abs(diff.x), bl_abs(diff.y));
      if (pt_max < test_max) {
        outer1 = i;
        outer2 = j;
        pt_max = test_max;
      }
    }
  }

  int mid1 = (1 + (2 >> outer2)) >> outer1;
  int mid2 = outer1 ^ outer2 ^ mid1;
  double line_slop = pt_max * pt_max * 0.00001;
  return point_to_line_distance_sq(cubic[mid1], cubic[outer1], cubic[outer2]) <= line_slop &&
         point_to_line_distance_sq(cubic[mid2], cubic[outer1], cubic[outer2]) <= line_slop;
}

static BL_INLINE double find_quad_max_curvature(const BLPoint quad[3]) noexcept {
  double ax = quad[1].x - quad[0].x;
  double ay = quad[1].y - quad[0].y;
  double bx = quad[0].x - quad[1].x - quad[1].x + quad[2].x;
  double by = quad[0].y - quad[1].y - quad[1].y + quad[2].y;

  double numer = -(ax * bx + ay * by);
  double denom = bx * bx + by * by;

  if (denom < 0.0) {
    numer = -numer;
    denom = -denom;
  }

  if (numer <= 0.0)
    return 0.0;

  if (numer >= denom)
    return 1.0;

  return numer / denom;
}

static BL_INLINE size_t find_cubic_inflections(const BLPoint cubic[4], double t_values[2]) noexcept {
  double ax = cubic[1].x - cubic[0].x;
  double ay = cubic[1].y - cubic[0].y;
  double bx = cubic[2].x - 2.0 * cubic[1].x + cubic[0].x;
  double by = cubic[2].y - 2.0 * cubic[1].y + cubic[0].y;
  double cx = cubic[3].x + 3.0 * (cubic[1].x - cubic[2].x) - cubic[0].x;
  double cy = cubic[3].y + 3.0 * (cubic[1].y - cubic[2].y) - cubic[0].y;

  return Math::quad_roots(t_values,
    bx * cy - by * cx,
    ax * cy - ay * cx,
    ax * by - ay * bx,
    Math::kAfter0,
    Math::kBefore1);
}

static BL_INLINE void formulate_f1_dot_f2(const BLPoint cubic[4], double coeff_x[4], double coeff_y[4]) noexcept {
  auto formulate = [](const double* src, double coeff[4]) noexcept {
    double a = src[2] - src[0];
    double b = src[4] - 2.0 * src[2] + src[0];
    double c = src[6] + 3.0 * (src[2] - src[4]) - src[0];

    coeff[0] = c * c;
    coeff[1] = 3.0 * b * c;
    coeff[2] = 2.0 * b * b + c * a;
    coeff[3] = a * b;
  };

  formulate(&cubic[0].x, coeff_x);
  formulate(&cubic[0].y, coeff_y);
}

static BL_INLINE size_t find_cubic_max_curvature(const BLPoint cubic[4], double t_values[3]) noexcept {
  double coeff_x[4];
  double coeff_y[4];
  formulate_f1_dot_f2(cubic, coeff_x, coeff_y);

  for (uint32_t i = 0; i < 4; i++)
    coeff_x[i] += coeff_y[i];

  return Math::cubic_roots(t_values, coeff_x, Math::kAfter0, Math::kBefore1);
}

static BL_INLINE double calc_cubic_precision(const BLPoint cubic[4]) noexcept {
  return (Geometry::magnitude_squared(cubic[1] - cubic[0]) +
          Geometry::magnitude_squared(cubic[2] - cubic[1]) +
          Geometry::magnitude_squared(cubic[3] - cubic[2])) * 1e-8;
}

static BL_INLINE bool on_same_side(const BLPoint cubic[4], int test_index, int line_index) noexcept {
  BLPoint origin = cubic[line_index];
  BLPoint line = cubic[line_index + 1] - origin;
  double crosses[2];

  for (int i = 0; i < 2; i++) {
    BLPoint test_line = cubic[test_index + i] - origin;
    crosses[i] = Geometry::cross(line, test_line);
  }

  return crosses[0] * crosses[1] >= 0.0;
}

static BL_INLINE double find_cubic_cusp(const BLPoint cubic[4]) noexcept {
  if (cubic[0] == cubic[1] || cubic[2] == cubic[3])
    return -1.0;

  if (on_same_side(cubic, 0, 2) || on_same_side(cubic, 2, 0))
    return -1.0;

  double max_curvature[3];
  size_t roots = find_cubic_max_curvature(cubic, max_curvature);
  for (size_t i = 0; i < roots; i++) {
    double t = max_curvature[i];
    if (!(t > 0.0 && t < 1.0))
      continue;

    BLPoint dpt = Geometry::derivative_at(Geometry::cubic_ref(cubic), t);
    if (Geometry::magnitude_squared(dpt) < calc_cubic_precision(cubic))
      return t;
  }

  return -1.0;
}

static BL_INLINE void angle_arc_to(PathAppender& appender, const BLPoint& pivot, const BLPoint& pa, const BLPoint& pb, const BLPoint& intersection) noexcept {
  BLPoint pm = (pa + pb) * 0.5;

  double denominator = Geometry::magnitude(pivot - intersection);
  if (!(denominator > 0.0)) {
    appender.line_to(pb);
    return;
  }

  double w = Math::sqrt(Geometry::magnitude(pivot - pm) / denominator);
  if (!(w > 0.0) || !Math::is_finite(w)) {
    appender.line_to(pb);
    return;
  }

  appender.conic_to(intersection, pb, w);
}

static BL_INLINE BLResult add_cap(PathAppender& out, BLPoint pivot, BLPoint p1, uint32_t cap_type) noexcept {
  BLPoint p0 = out.vtx[-1];
  BLPoint q = Geometry::normal(p1 - p0) * 0.5;

  switch (cap_type) {
    case BL_STROKE_CAP_BUTT:
    default:
      out.line_to(p1);
      break;

    case BL_STROKE_CAP_SQUARE:
      out.line_to(p0 + q);
      out.line_to(p1 + q);
      out.line_to(p1);
      break;

    case BL_STROKE_CAP_ROUND:
      out.arc_quadrant_to(p0 + q, pivot + q);
      out.arc_quadrant_to(p1 + q, p1);
      break;

    case BL_STROKE_CAP_ROUND_REV:
      out.line_to(p0 + q);
      out.arc_quadrant_to(p0, pivot);
      out.arc_quadrant_to(p1, p1 + q);
      out.line_to(p1);
      break;

    case BL_STROKE_CAP_TRIANGLE:
      out.line_to(pivot + q);
      out.line_to(p1);
      break;

    case BL_STROKE_CAP_TRIANGLE_REV:
      out.line_to(p0 + q);
      out.line_to(pivot);
      out.line_to(p1 + q);
      out.line_to(p1);
      break;
  }

  return BL_SUCCESS;
}

// bl::Path - Stroke - Implementation
// ==================================

class PathStroker {
public:
  PathIterator _iter;
  const BLStrokeOptions& _options;
  const BLApproximationOptions& _approx;

  BLPath* _a_path;
  BLPath* _b_path;
  BLPath* _c_path;
  BLPath _cusp_path;

  PathAppender _a_out;
  PathAppender _b_out;

  double _radius;
  double _miter_limit;
  double _inv_miter_limit;
  double _inv_res_scale;
  double _inv_res_scale_sq;

  BLPoint _first_normal;
  BLPoint _prev_normal;
  BLPoint _first_unit_normal;
  BLPoint _prev_unit_normal;
  BLPoint _first_pt;
  BLPoint _prev_pt;
  BLPoint _first_outer_pt;

  size_t _a_figure_offset;
  size_t _b_figure_offset;

  int _segment_count;
  bool _first_is_line;
  bool _prev_is_line;
  bool _join_completed;
  uint32_t _recursion_depth;
  bool _found_tangents;
  StrokeType _stroke_type;
  uint32_t _join_override;

  BL_INLINE PathStroker(const BLPathView& input, const BLStrokeOptions& options, const BLApproximationOptions& approx, BLPath* a, BLPath* b, BLPath* c) noexcept
    : _iter(input),
      _options(options),
      _approx(approx),
      _a_path(a),
      _b_path(b),
      _c_path(c),
      _radius(options.width * 0.5),
      _miter_limit(options.miter_limit),
      _inv_miter_limit(options.miter_limit > 0.0 ? 1.0 / options.miter_limit : std::numeric_limits<double>::infinity()),
      _inv_res_scale(4.0 * approx.simplify_tolerance),
      _inv_res_scale_sq(Math::square(_inv_res_scale)),
      _a_figure_offset(0),
      _b_figure_offset(0),
      _segment_count(-1),
      _first_is_line(false),
      _prev_is_line(false),
      _join_completed(false),
      _recursion_depth(0),
      _found_tangents(false),
      _stroke_type(StrokeType::kOuter),
      _join_override(UINT32_MAX) {}

  BL_INLINE bool has_only_move_to() const noexcept { return _segment_count == 0; }
  BL_INLINE BLPoint move_to_pt() const noexcept { return _first_pt; }

  BL_INLINE bool is_current_contour_empty() const noexcept {
    return is_zero_length_since_point(*_b_path, _b_figure_offset) &&
           is_zero_length_since_point(*_a_path, _a_figure_offset);
  }

  BL_INLINE BLResult ensure_appenders_capacity(size_t a_required, size_t b_required, size_t a_conic_required = 0, size_t b_conic_required = 0) noexcept {
    uint32_t ok = uint32_t(_a_out.remaining_size() >= a_required) &
                  uint32_t(_b_out.remaining_size() >= b_required) &
                  uint32_t(_a_out.remaining_conic_weight_size() >= a_conic_required) &
                  uint32_t(_b_out.remaining_conic_weight_size() >= b_conic_required);

    if (BL_LIKELY(ok))
      return BL_SUCCESS;

    return _a_out.ensure(_a_path, a_required, a_conic_required) |
           _b_out.ensure(_b_path, b_required, b_conic_required);
  }

  BL_INLINE BLResult ensure_active_side_capacity(size_t required, size_t conic_required = 0) noexcept {
    if (_stroke_type == StrokeType::kOuter)
      return ensure_appenders_capacity(required, 0, conic_required, 0);
    else
      return ensure_appenders_capacity(0, required, 0, conic_required);
  }

  BL_INLINE PathAppender& active_side() noexcept {
    return _stroke_type == StrokeType::kOuter ? _a_out : _b_out;
  }

  BL_INLINE void set_last_point(PathAppender& out, const BLPoint& p) noexcept {
    out.vtx[-1] = p;
  }

  BL_INLINE uint32_t current_join() const noexcept {
    return _join_override <= BL_STROKE_JOIN_MAX_VALUE ? _join_override : sanitize_stroke_join(_options.join);
  }

  BL_INLINE void move_to(const BLPoint& pt) noexcept {
    _segment_count = 0;
    _first_pt = pt;
    _prev_pt = pt;
    _first_is_line = false;
    _join_completed = false;
    _prev_is_line = false;
  }

  BL_INLINE bool has_valid_tangent(PathIterator iter) const noexcept {
    while (!iter.at_end()) {
      uint8_t cmd = iter.cmd[0];
      if (cmd == BL_PATH_CMD_MOVE)
        return false;

      if (cmd == BL_PATH_CMD_ON) {
        if (iter.vtx[0] != _prev_pt)
          return true;
        iter++;
        continue;
      }

      if (cmd == BL_PATH_CMD_QUAD || cmd == BL_PATH_CMD_CONIC) {
        if (iter.remaining_forward() < 2)
          return false;
        if (!(iter.vtx[0] == _prev_pt && iter.vtx[1] == _prev_pt))
          return true;
        iter += 2;
        continue;
      }

      if (cmd == BL_PATH_CMD_CUBIC) {
        if (iter.remaining_forward() < 3)
          return false;
        if (!(iter.vtx[0] == _prev_pt && iter.vtx[1] == _prev_pt && iter.vtx[2] == _prev_pt))
          return true;
        iter += 3;
        continue;
      }

      return false;
    }

    return false;
  }

  BL_INLINE bool pre_join_to(const BLPoint& curr_pt, BLPoint* normal, BLPoint* unit_normal, bool curr_is_line) noexcept {
    if (!set_normal_unitnormal(_prev_pt, curr_pt, 1.0, _radius, normal, unit_normal)) {
      if (_segment_count != 0 || !has_non_butt_caps(_options))
        return false;

      normal->reset(_radius, 0.0);
      unit_normal->reset(1.0, 0.0);
    }

    if (_segment_count == 0) {
      _first_is_line = curr_is_line;
      _first_normal = *normal;
      _first_unit_normal = *unit_normal;
      _first_outer_pt = _prev_pt + *normal;

      _a_out.move_to(_first_outer_pt);
      _b_out.move_to(_prev_pt - *normal);
    }
    else {
      join_to(_prev_unit_normal, _prev_pt, *unit_normal, _prev_is_line, curr_is_line);
    }

    _prev_is_line = curr_is_line;
    return true;
  }

  BL_INLINE void post_join_to(const BLPoint& curr_pt, const BLPoint& normal, const BLPoint& unit_normal) noexcept {
    _join_completed = true;
    _prev_pt = curr_pt;
    _prev_normal = normal;
    _prev_unit_normal = unit_normal;
    _segment_count++;
  }

  BL_INLINE void line_to_current_segment_end(const BLPoint& curr_pt, const BLPoint& normal) noexcept {
    _a_out.line_to(curr_pt + normal);
    _b_out.line_to(curr_pt - normal);
  }

  BL_INLINE void handle_inner_join(PathAppender& inner, const BLPoint& pivot, const BLPoint& after) noexcept {
    inner.line_to(pivot);
    inner.line_to(pivot - after);
  }

  struct JoinSideSelection {
    PathAppender* outer;
    PathAppender* inner;
    BLPoint before;
    BLPoint after;
    BLPoint before_tangent;
    BLPoint after_tangent;
  };

  BL_INLINE JoinSideSelection select_join_sides(PathAppender& a_ref, PathAppender& b_ref, const BLPoint& pivot, const BLPoint& before_unit, const BLPoint& after_unit) noexcept {
    JoinSideSelection selection {
      &a_ref,
      &b_ref,
      before_unit,
      after_unit,
      BLPoint(before_unit.y, -before_unit.x),
      BLPoint(after_unit.y, -after_unit.x)
    };
    double outer_sign = is_clockwise(before_unit, after_unit) ? -1.0 : 1.0;
    double a_sign = Geometry::dot(a_ref.vtx[-1] - pivot, before_unit) < 0.0 ? -1.0 : 1.0;

    if (a_sign != outer_sign) {
      using std::swap;
      swap(selection.outer, selection.inner);
    }

    selection.before *= outer_sign;
    selection.after *= outer_sign;
    return selection;
  }

  BL_INLINE void blunt_join(PathAppender& outer, PathAppender& inner, const BLPoint&, const BLPoint& pivot, const BLPoint& after_unit) noexcept {
    BLPoint after = after_unit * _radius;
    outer.line_to(pivot + after);
    handle_inner_join(inner, pivot, after);
  }

  BL_INLINE void round_join(PathAppender& outer, PathAppender& inner, const BLPoint& before_unit, const BLPoint& pivot, const BLPoint& after_unit) noexcept {
    double dot_prod = Geometry::dot(before_unit, after_unit);
    if (dot_to_angle_type(dot_prod) == AngleType::kNearlyLine)
      return;

    BLPoint pa = outer.vtx[-1];
    BLPoint pb = pivot + after_unit * _radius;

    if (Geometry::dot(pivot - pa, pivot - pb) < 0.0) {
      BLPoint n2 = Geometry::normal(Geometry::unit_vector(pb - pa));
      if (Math::is_finite(n2.x)) {
        BLPoint incoming = pa - outer.vtx[-2];
        if (!degenerate_vector(incoming) && Geometry::dot(pivot + n2 * _radius - pa, incoming) < 0.0)
          n2 = -n2;

        BLPoint m = before_unit + n2;
        BLPoint k = m * (_radius + _radius) / Geometry::magnitude_squared(m);
        BLPoint q = n2 * _radius;

        BLPoint pc1 = pivot + k;
        BLPoint pp1 = pivot + q;
        BLPoint pc2 = Math::lerp(pc1, pp1, 2.0);

        angle_arc_to(outer, pivot, pa, pp1, pc1);
        angle_arc_to(outer, pivot, pp1, pb, pc2);
      }
      else {
        outer.line_to(pb);
      }
    }
    else {
      BLPoint m = before_unit + after_unit;
      BLPoint k = m * (_radius + _radius) / Geometry::magnitude_squared(m);
      angle_arc_to(outer, pivot, pa, pb, pivot + k);
    }

    handle_inner_join(inner, pivot, after_unit * _radius);
  }

  BL_INLINE void miter_clip_join(PathAppender& outer, PathAppender& inner, const BLPoint& before_unit, const BLPoint& pivot, const BLPoint& after_unit, const BLPoint& before_tangent, const BLPoint& after_tangent, bool prev_is_line) noexcept {
    BLPoint m = before_unit + after_unit;
    BLPoint k = m * (_radius + _radius) / Geometry::magnitude_squared(m);
    BLPoint after = after_unit * _radius;

    if (Geometry::magnitude_squared(k) <= Math::square(_radius * _miter_limit)) {
      BLPoint mid = pivot + k;
      if (prev_is_line)
        set_last_point(outer, mid);
      else
        outer.line_to(mid);
      outer.line_to(pivot + after);
      handle_inner_join(inner, pivot, after);
      return;
    }

    double b2 = bl_abs(Geometry::cross(k, before_unit));
    if (b2 > 0.0)
      b2 = b2 * (_radius * _miter_limit) / Geometry::magnitude(k);
    else
      b2 = _radius * _miter_limit;

    BLPoint t0 = pivot + _radius * before_unit + b2 * before_tangent;
    BLPoint t1 = pivot + after - b2 * after_tangent;

    if (prev_is_line)
      set_last_point(outer, t0);
    else
      outer.line_to(t0);

    outer.line_to(t1);
    outer.line_to(pivot + after);
    handle_inner_join(inner, pivot, after);
  }

  BL_INLINE void miter_join(PathAppender& outer, PathAppender& inner, const BLPoint& before_unit, const BLPoint& pivot, const BLPoint& after_unit, const BLPoint& before_tangent, const BLPoint& after_tangent, bool prev_is_line, bool curr_is_line, uint32_t join_type) noexcept {
    double dot_prod = Geometry::dot(before_unit, after_unit);
    AngleType angle_type = dot_to_angle_type(dot_prod);

    if (angle_type == AngleType::kNearlyLine)
      return;

    if (join_type == BL_STROKE_JOIN_MITER_CLIP) {
      miter_clip_join(outer, inner, before_unit, pivot, after_unit, before_tangent, after_tangent, prev_is_line);
      return;
    }

    if (angle_type == AngleType::kNearly180) {
      blunt_join(outer, inner, before_unit, pivot, after_unit);
      return;
    }

    double sin_half_angle = Math::sqrt(0.5 * (1.0 + dot_prod));
    if (!(dot_prod == 0.0 && _inv_miter_limit <= kStrokeOneOverSqrt2) && sin_half_angle < _inv_miter_limit) {
      if (join_type == BL_STROKE_JOIN_MITER_ROUND)
        round_join(outer, inner, before_unit, pivot, after_unit);
      else
        blunt_join(outer, inner, before_unit, pivot, after_unit);
      return;
    }

    BLPoint mid = before_unit + after_unit;
    double mid_scale = (_radius + _radius) / Geometry::magnitude_squared(mid);
    if (!Math::is_finite(mid_scale)) {
      blunt_join(outer, inner, before_unit, pivot, after_unit);
      return;
    }

    mid *= mid_scale;

    if (prev_is_line)
      set_last_point(outer, pivot + mid);
    else
      outer.line_to(pivot + mid);

    BLPoint after_scaled = after_unit * _radius;
    if (!curr_is_line)
      outer.line_to(pivot + after_scaled);

    handle_inner_join(inner, pivot, after_scaled);
  }

  BL_INLINE void emit_join(const JoinSideSelection& selection, const BLPoint& pivot, bool prev_is_line, bool curr_is_line) noexcept {
    uint32_t join = current_join();

    if (join == BL_STROKE_JOIN_ROUND || join == BL_STROKE_JOIN_BEVEL)
      prev_is_line = false;

    if (join == BL_STROKE_JOIN_ROUND)
      round_join(*selection.outer, *selection.inner, selection.before, pivot, selection.after);
    else if (join == BL_STROKE_JOIN_BEVEL)
      blunt_join(*selection.outer, *selection.inner, selection.before, pivot, selection.after);
    else
      miter_join(*selection.outer, *selection.inner, selection.before, pivot, selection.after, selection.before_tangent, selection.after_tangent, prev_is_line, curr_is_line, join);
  }

  BL_INLINE void join_to(const BLPoint& before_unit_normal, const BLPoint& pivot, const BLPoint& after_unit_normal, bool prev_is_line, bool curr_is_line) noexcept {
    if (before_unit_normal == after_unit_normal)
      return;

    JoinSideSelection selection = select_join_sides(_a_out, _b_out, pivot, before_unit_normal, after_unit_normal);
    emit_join(selection, pivot, prev_is_line, curr_is_line);
  }

  BL_INLINE void line_to_selected_segment_end(const JoinSideSelection& selection, const BLPoint& curr_pt) noexcept {
    selection.outer->line_to(curr_pt + selection.after * _radius);
    selection.inner->line_to(curr_pt - selection.after * _radius);
  }

  BL_INLINE BLResult line_to(const BLPoint& curr_pt, const PathIterator* iter = nullptr) noexcept {
    double tolerance = kStrokeLengthEpsilon * bl_max(_inv_res_scale, 1.0);
    bool teeny_line = Geometry::magnitude_squared(curr_pt - _prev_pt) <= tolerance * tolerance;

    if (!has_non_butt_caps(_options) && teeny_line)
      return BL_SUCCESS;

    if (teeny_line && (_join_completed || (iter && has_valid_tangent(*iter))))
      return BL_SUCCESS;

    BLPoint normal;
    BLPoint unit_normal;

    if (!pre_join_to(curr_pt, &normal, &unit_normal, true))
      return BL_SUCCESS;

    BL_PROPAGATE(ensure_appenders_capacity(1, 1));
    line_to_current_segment_end(curr_pt, normal);
    post_join_to(curr_pt, normal, unit_normal);
    return BL_SUCCESS;
  }

  BL_INLINE void init(StrokeType stroke_type, QuadConstruct* quad_pts, double start_t, double end_t) noexcept {
    _stroke_type = stroke_type;
    _found_tangents = false;
    _recursion_depth = 0;
    quad_pts->init(start_t, end_t);
  }

  BL_INLINE void set_quad_end_normal(const BLPoint quad[3], const BLPoint& normal_ab, const BLPoint& unit_ab, BLPoint* normal_bc, BLPoint* unit_bc) const noexcept {
    if (!set_normal_unitnormal(quad[1], quad[2], 1.0, _radius, normal_bc, unit_bc)) {
      *normal_bc = normal_ab;
      *unit_bc = unit_ab;
    }
  }

  BL_INLINE void set_conic_end_normal(const BLPoint conic[4], const BLPoint& normal_ab, const BLPoint& unit_ab, BLPoint* normal_bc, BLPoint* unit_bc) const noexcept {
    BLPoint quad[3] { conic[0], conic[1], conic[3] };
    set_quad_end_normal(quad, normal_ab, unit_ab, normal_bc, unit_bc);
  }

  BL_INLINE void set_cubic_end_normal(const BLPoint cubic[4], const BLPoint& normal_ab, const BLPoint& unit_ab, BLPoint* normal_cd, BLPoint* unit_cd) const noexcept {
    BLPoint ab = cubic[1] - cubic[0];
    BLPoint cd = cubic[3] - cubic[2];
    bool degenerate_ab = degenerate_vector(ab);
    bool degenerate_cd = degenerate_vector(cd);

    if (degenerate_ab && degenerate_cd) {
      *normal_cd = normal_ab;
      *unit_cd = unit_ab;
      return;
    }

    if (degenerate_ab) {
      ab = cubic[2] - cubic[0];
      degenerate_ab = degenerate_vector(ab);
    }
    if (degenerate_cd) {
      cd = cubic[3] - cubic[1];
      degenerate_cd = degenerate_vector(cd);
    }

    if (degenerate_ab || degenerate_cd || !set_normal_unitnormal(cd, _radius, normal_cd, unit_cd)) {
      *normal_cd = normal_ab;
      *unit_cd = unit_ab;
    }
  }

  BL_INLINE void set_ray_pts(const BLPoint& t_pt, BLPoint* dxy, BLPoint* on_pt, BLPoint* tangent) const noexcept {
    *dxy = set_length(*dxy, _radius);
    if (dxy->x == 0.0 && dxy->y == 0.0)
      dxy->reset(_radius, 0.0);

    BLPoint offset = Geometry::normal(*dxy) * double(int(_stroke_type));
    *on_pt = t_pt + offset;

    if (tangent)
      *tangent = *dxy;
  }

  BL_INLINE void quad_perp_ray(const BLPoint quad[3], double t, BLPoint* t_pt, BLPoint* on_pt, BLPoint* tangent) const noexcept {
    Geometry::QuadDerivativeCoefficients dc = Geometry::derivative_coefficients_of(Geometry::quad_ref(quad));
    BLPoint dxy = dc.a * t + dc.b;
    *t_pt = Geometry::evaluate_precise(Geometry::quad_ref(quad), t);

    if (degenerate_vector(dxy))
      dxy = quad[2] - quad[0];

    set_ray_pts(*t_pt, &dxy, on_pt, tangent);
  }

  BL_INLINE void conic_perp_ray(const BLPoint conic[4], double t, BLPoint* t_pt, BLPoint* on_pt, BLPoint* tangent) const noexcept {
    BLPoint a, b, c;
    Geometry::get_conic_derivative_coefficients(conic, a, b, c);
    BLPoint dxy = (a * t + b) * t + c;
    *t_pt = Geometry::eval_conic_precise(conic, BLPoint(t, t));

    if (degenerate_vector(dxy))
      dxy = conic[3] - conic[0];

    set_ray_pts(*t_pt, &dxy, on_pt, tangent);
  }

  BL_INLINE void cubic_perp_ray(const BLPoint cubic[4], double t, BLPoint* t_pt, BLPoint* on_pt, BLPoint* tangent) const noexcept {
    BLPoint dxy = Geometry::derivative_at(Geometry::cubic_ref(cubic), t);
    *t_pt = Geometry::evaluate_precise(Geometry::cubic_ref(cubic), t);

    if (degenerate_vector(dxy)) {
      if (t <= kStrokeJoinEpsilon) {
        dxy = cubic[2] - cubic[0];
      }
      else if (1.0 - t <= kStrokeJoinEpsilon) {
        dxy = cubic[3] - cubic[1];
      }
      else {
        BLPoint chopped[8];
        Geometry::split(Geometry::cubic_ref(cubic), Geometry::cubic_out(chopped), Geometry::cubic_out(chopped + 4), t);
        dxy = chopped[4] - chopped[3];
        if (degenerate_vector(dxy))
          dxy = chopped[4] - chopped[2];
      }

      if (degenerate_vector(dxy))
        dxy = cubic[3] - cubic[0];
    }

    set_ray_pts(*t_pt, &dxy, on_pt, tangent);
  }

  BL_INLINE void quad_ends(const BLPoint quad[3], QuadConstruct* quad_pts) const noexcept {
    if (!quad_pts->start_set) {
      BLPoint quad_start_pt;
      quad_perp_ray(quad, quad_pts->start_t, &quad_start_pt, &quad_pts->quad[0], &quad_pts->tangent_start);
      quad_pts->start_set = true;
    }

    if (!quad_pts->end_set) {
      BLPoint quad_end_pt;
      quad_perp_ray(quad, quad_pts->end_t, &quad_end_pt, &quad_pts->quad[2], &quad_pts->tangent_end);
      quad_pts->end_set = true;
    }
  }

  BL_INLINE void conic_ends(const BLPoint conic[4], QuadConstruct* quad_pts) const noexcept {
    if (!quad_pts->start_set) {
      BLPoint conic_start_pt;
      conic_perp_ray(conic, quad_pts->start_t, &conic_start_pt, &quad_pts->quad[0], &quad_pts->tangent_start);
      quad_pts->start_set = true;
    }

    if (!quad_pts->end_set) {
      BLPoint conic_end_pt;
      conic_perp_ray(conic, quad_pts->end_t, &conic_end_pt, &quad_pts->quad[2], &quad_pts->tangent_end);
      quad_pts->end_set = true;
    }
  }

  BL_INLINE void cubic_ends(const BLPoint cubic[4], QuadConstruct* quad_pts) const noexcept {
    if (!quad_pts->start_set) {
      BLPoint cubic_start_pt;
      cubic_perp_ray(cubic, quad_pts->start_t, &cubic_start_pt, &quad_pts->quad[0], &quad_pts->tangent_start);
      quad_pts->start_set = true;
    }

    if (!quad_pts->end_set) {
      BLPoint cubic_end_pt;
      cubic_perp_ray(cubic, quad_pts->end_t, &cubic_end_pt, &quad_pts->quad[2], &quad_pts->tangent_end);
      quad_pts->end_set = true;
    }
  }

  BL_INLINE ResultType intersect_ray(QuadConstruct* quad_pts, IntersectRayType intersect_ray_type) const noexcept {
    const BLPoint& start = quad_pts->quad[0];
    const BLPoint& end = quad_pts->quad[2];
    BLPoint a_len = quad_pts->tangent_start;
    BLPoint b_len = quad_pts->tangent_end;

    double denom = Geometry::cross(a_len, b_len);
    if (denom == 0.0 || !Math::is_finite(denom)) {
      quad_pts->opposite_tangents = Geometry::dot(a_len, b_len) < 0.0;
      return ResultType::kDegenerate;
    }

    quad_pts->opposite_tangents = false;
    BLPoint ab0 = start - end;
    double numer_a = Geometry::cross(b_len, ab0);
    double numer_b = Geometry::cross(a_len, ab0);

    if ((numer_a >= 0.0) == (numer_b >= 0.0)) {
      double dist1 = point_to_tangent_line_distance_sq(start, end, quad_pts->tangent_end);
      double dist2 = point_to_tangent_line_distance_sq(end, start, quad_pts->tangent_start);
      if (bl_max(dist1, dist2) <= _inv_res_scale_sq)
        return ResultType::kDegenerate;
      return ResultType::kSplit;
    }

    numer_a /= denom;
    if (numer_a > numer_a - 1.0) {
      if (intersect_ray_type == IntersectRayType::kCtrlPt)
        quad_pts->quad[1] = start + quad_pts->tangent_start * numer_a;
      return ResultType::kQuad;
    }

    quad_pts->opposite_tangents = Geometry::dot(a_len, b_len) < 0.0;
    return ResultType::kDegenerate;
  }

  static BL_INLINE int intersect_quad_ray(const BLPoint line[2], const BLPoint quad[3], double roots[2]) noexcept {
    BLPoint vec = line[1] - line[0];
    double r[3];

    for (uint32_t i = 0; i < 3; i++)
      r[i] = Geometry::cross(vec, quad[i] - line[0]);

    double a = r[2] + r[0] - 2.0 * r[1];
    double b = r[1] - r[0];
    double c = r[0];

    return int(Math::quad_roots(roots, a, 2.0 * b, c, 0.0, 1.0));
  }

  BL_INLINE bool point_in_quad_bounds(const BLPoint quad[3], const BLPoint& pt) const noexcept {
    double x_min = bl_min(bl_min(quad[0].x, quad[1].x), quad[2].x);
    double x_max = bl_max(bl_max(quad[0].x, quad[1].x), quad[2].x);
    double y_min = bl_min(bl_min(quad[0].y, quad[1].y), quad[2].y);
    double y_max = bl_max(bl_max(quad[0].y, quad[1].y), quad[2].y);

    return pt.x + _inv_res_scale >= x_min &&
           pt.x - _inv_res_scale <= x_max &&
           pt.y + _inv_res_scale >= y_min &&
           pt.y - _inv_res_scale <= y_max;
  }

  static BL_INLINE bool sharp_angle(const BLPoint quad[3]) noexcept {
    BLPoint smaller = quad[1] - quad[0];
    BLPoint larger = quad[1] - quad[2];
    double smaller_len = Geometry::magnitude_squared(smaller);
    double larger_len = Geometry::magnitude_squared(larger);

    if (smaller_len > larger_len) {
      using std::swap;
      swap(smaller, larger);
      larger_len = smaller_len;
    }

    smaller = set_length(smaller, larger_len);
    if (smaller.x == 0.0 && smaller.y == 0.0)
      return false;

    return Geometry::dot(smaller, larger) > 0.0;
  }

  BL_INLINE ResultType stroke_close_enough(const BLPoint stroke[3], const BLPoint ray[2], QuadConstruct* quad_pts) const noexcept {
    BLPoint stroke_mid = Geometry::evaluate_precise(Geometry::quad_ref(stroke), 0.5);
    if (points_within_dist(ray[0], stroke_mid, _inv_res_scale)) {
      if (sharp_angle(quad_pts->quad))
        return ResultType::kSplit;
      return ResultType::kQuad;
    }

    if (!point_in_quad_bounds(stroke, ray[0]))
      return ResultType::kSplit;

    double roots[2];
    if (intersect_quad_ray(ray, stroke, roots) != 1)
      return ResultType::kSplit;

    BLPoint quad_pt = Geometry::evaluate_precise(Geometry::quad_ref(stroke), roots[0]);
    double error = _inv_res_scale * (1.0 - bl_abs(roots[0] - 0.5) * 2.0);
    if (points_within_dist(ray[0], quad_pt, error)) {
      if (sharp_angle(quad_pts->quad))
        return ResultType::kSplit;
      return ResultType::kQuad;
    }

    return ResultType::kSplit;
  }

  BL_INLINE ResultType compare_quad_quad(const BLPoint quad[3], QuadConstruct* quad_pts) const noexcept {
    quad_ends(quad, quad_pts);

    ResultType result_type = intersect_ray(quad_pts, IntersectRayType::kCtrlPt);
    if (result_type != ResultType::kQuad)
      return result_type;

    BLPoint ray[2];
    quad_perp_ray(quad, quad_pts->mid_t, &ray[1], &ray[0], nullptr);
    return stroke_close_enough(quad_pts->quad, ray, quad_pts);
  }

  BL_INLINE ResultType compare_quad_conic(const BLPoint conic[4], QuadConstruct* quad_pts) const noexcept {
    conic_ends(conic, quad_pts);

    ResultType result_type = intersect_ray(quad_pts, IntersectRayType::kCtrlPt);
    if (result_type != ResultType::kQuad)
      return result_type;

    BLPoint ray[2];
    conic_perp_ray(conic, quad_pts->mid_t, &ray[1], &ray[0], nullptr);
    return stroke_close_enough(quad_pts->quad, ray, quad_pts);
  }

  BL_INLINE ResultType tangents_meet(const BLPoint cubic[4], QuadConstruct* quad_pts) noexcept {
    cubic_ends(cubic, quad_pts);
    return intersect_ray(quad_pts, IntersectRayType::kResultType);
  }

  BL_INLINE BLResult add_degenerate_line(const QuadConstruct* quad_pts) noexcept {
    BL_PROPAGATE(ensure_active_side_capacity(1));
    active_side().line_to(quad_pts->quad[2]);
    return BL_SUCCESS;
  }

  BL_INLINE void cubic_quad_mid(const BLPoint cubic[4], const QuadConstruct* quad_pts, BLPoint* mid) const noexcept {
    BLPoint cubic_mid_pt;
    cubic_perp_ray(cubic, quad_pts->mid_t, &cubic_mid_pt, mid, nullptr);
  }

  BL_INLINE bool cubic_mid_on_line(const BLPoint cubic[4], const QuadConstruct* quad_pts) const noexcept {
    BLPoint stroke_mid;
    cubic_quad_mid(cubic, quad_pts, &stroke_mid);
    double dist = point_to_line_distance_sq(stroke_mid, quad_pts->quad[0], quad_pts->quad[2]);
    return dist < _inv_res_scale_sq;
  }

  BL_INLINE ResultType compare_quad_cubic(const BLPoint cubic[4], QuadConstruct* quad_pts) const noexcept {
    cubic_ends(cubic, quad_pts);

    ResultType result_type = intersect_ray(quad_pts, IntersectRayType::kCtrlPt);
    if (result_type != ResultType::kQuad)
      return result_type;

    BLPoint ray[2];
    cubic_perp_ray(cubic, quad_pts->mid_t, &ray[1], &ray[0], nullptr);
    return stroke_close_enough(quad_pts->quad, ray, quad_pts);
  }

  BL_INLINE BLResult quad_stroke(const BLPoint quad[3], QuadConstruct* quad_pts) noexcept {
    ResultType result_type = compare_quad_quad(quad, quad_pts);
    if (result_type == ResultType::kQuad) {
      BL_PROPAGATE(ensure_active_side_capacity(2, 0));
      active_side().quad_to(quad_pts->quad[1], quad_pts->quad[2]);
      return BL_SUCCESS;
    }

    if (result_type == ResultType::kDegenerate)
      return add_degenerate_line(quad_pts);

    if (++_recursion_depth > kStrokeQuadRecursiveLimit)
      return add_degenerate_line(quad_pts);

    QuadConstruct half;
    if (!half.init_with_start(*quad_pts))
      return add_degenerate_line(quad_pts);
    BL_PROPAGATE(quad_stroke(quad, &half));

    if (!half.init_with_end(*quad_pts))
      return add_degenerate_line(quad_pts);
    BL_PROPAGATE(quad_stroke(quad, &half));

    _recursion_depth--;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult conic_stroke(const BLPoint conic[4], QuadConstruct* quad_pts) noexcept {
    ResultType result_type = compare_quad_conic(conic, quad_pts);
    if (result_type == ResultType::kQuad) {
      BL_PROPAGATE(ensure_active_side_capacity(2));
      active_side().quad_to(quad_pts->quad[1], quad_pts->quad[2]);
      return BL_SUCCESS;
    }

    if (result_type == ResultType::kDegenerate)
      return add_degenerate_line(quad_pts);

    if (++_recursion_depth > kStrokeConicRecursiveLimit)
      return add_degenerate_line(quad_pts);

    QuadConstruct half;
    if (!half.init_with_start(*quad_pts))
      return add_degenerate_line(quad_pts);
    BL_PROPAGATE(conic_stroke(conic, &half));

    if (!half.init_with_end(*quad_pts))
      return add_degenerate_line(quad_pts);
    BL_PROPAGATE(conic_stroke(conic, &half));

    _recursion_depth--;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult cubic_stroke(const BLPoint cubic[4], QuadConstruct* quad_pts) noexcept {
    if (!_found_tangents) {
      ResultType result_type = tangents_meet(cubic, quad_pts);
      if (result_type != ResultType::kQuad) {
        if ((result_type == ResultType::kDegenerate || points_within_dist(quad_pts->quad[0], quad_pts->quad[2], _inv_res_scale)) &&
            cubic_mid_on_line(cubic, quad_pts)) {
          return add_degenerate_line(quad_pts);
        }
      }
      else {
        _found_tangents = true;
      }
    }

    if (_found_tangents) {
      ResultType result_type = compare_quad_cubic(cubic, quad_pts);
      if (result_type == ResultType::kQuad) {
        BL_PROPAGATE(ensure_active_side_capacity(2));
        active_side().quad_to(quad_pts->quad[1], quad_pts->quad[2]);
        return BL_SUCCESS;
      }

      if (result_type == ResultType::kDegenerate && !quad_pts->opposite_tangents)
        return add_degenerate_line(quad_pts);
    }

    if (!Math::is_finite(quad_pts->quad[2].x) || !Math::is_finite(quad_pts->quad[2].y))
      return BL_SUCCESS;

    if (++_recursion_depth > (_found_tangents ? kStrokeCubicRecursiveLimit : kStrokeTangentRecursiveLimit))
      return add_degenerate_line(quad_pts);

    QuadConstruct half;
    if (!half.init_with_start(*quad_pts))
      return add_degenerate_line(quad_pts);
    BL_PROPAGATE(cubic_stroke(cubic, &half));

    if (!half.init_with_end(*quad_pts))
      return add_degenerate_line(quad_pts);
    BL_PROPAGATE(cubic_stroke(cubic, &half));

    _recursion_depth--;
    return BL_SUCCESS;
  }

  static BL_INLINE ReductionType check_quad_linear(const BLPoint quad[3], BLPoint* reduction) noexcept {
    bool degenerate_ab = degenerate_vector(quad[1] - quad[0]);
    bool degenerate_bc = degenerate_vector(quad[2] - quad[1]);

    if (degenerate_ab && degenerate_bc)
      return ReductionType::kPoint;

    if (degenerate_ab || degenerate_bc)
      return ReductionType::kLine;

    if (!quad_in_line(quad))
      return ReductionType::kQuad;

    double t = find_quad_max_curvature(quad);
    if (t == 0.0 || t == 1.0 || !Math::is_finite(t))
      return ReductionType::kLine;

    *reduction = Geometry::evaluate_precise(Geometry::quad_ref(quad), t);
    return ReductionType::kDegenerate;
  }

  static BL_INLINE ReductionType check_conic_linear(const BLPoint conic[4], BLPoint* reduction) noexcept {
    bool degenerate_ab = degenerate_vector(conic[1] - conic[0]);
    bool degenerate_bc = degenerate_vector(conic[3] - conic[1]);

    if (degenerate_ab && degenerate_bc)
      return ReductionType::kPoint;

    if (degenerate_ab || degenerate_bc)
      return ReductionType::kLine;

    if (!conic_in_line(conic))
      return ReductionType::kQuad;

    double t = find_quad_max_curvature(conic);
    if (t == 0.0 || !Math::is_finite(t))
      return ReductionType::kLine;

    *reduction = Geometry::eval_conic_precise(conic, BLPoint(t, t));
    return ReductionType::kDegenerate;
  }

  static BL_INLINE ReductionType check_cubic_linear(const BLPoint cubic[4], BLPoint reduction[3], const BLPoint** tangent_pt_ptr) noexcept {
    bool degenerate_ab = degenerate_vector(cubic[1] - cubic[0]);
    bool degenerate_bc = degenerate_vector(cubic[2] - cubic[1]);
    bool degenerate_cd = degenerate_vector(cubic[3] - cubic[2]);

    if (degenerate_ab && degenerate_bc && degenerate_cd)
      return ReductionType::kPoint;

    if (uint32_t(degenerate_ab) + uint32_t(degenerate_bc) + uint32_t(degenerate_cd) == 2u)
      return ReductionType::kLine;

    if (!cubic_in_line(cubic)) {
      *tangent_pt_ptr = degenerate_ab ? &cubic[2] : &cubic[1];
      return ReductionType::kQuad;
    }

    double t_values[3];
    size_t count = find_cubic_max_curvature(cubic, t_values);
    size_t reduction_count = 0;

    for (size_t i = 0; i < count; i++) {
      double t = t_values[i];
      if (!(t > 0.0 && t < 1.0))
        continue;

      reduction[reduction_count] = Geometry::evaluate_precise(Geometry::cubic_ref(cubic), t);
      if (reduction[reduction_count] != cubic[0] && reduction[reduction_count] != cubic[3])
        reduction_count++;
    }

    if (reduction_count == 0)
      return ReductionType::kLine;
    if (reduction_count == 1)
      return ReductionType::kDegenerate;
    if (reduction_count == 2)
      return ReductionType::kDegenerate2;
    return ReductionType::kDegenerate3;
  }

  BL_INLINE BLResult append_cusp_circle(const BLPoint& center) noexcept {
    PathAppender cusp_out;
    BL_PROPAGATE(cusp_out.begin_append(&_cusp_path, kStrokeCircleVertices, kStrokeCircleConics));

    BLPoint p0(center.x + _radius, center.y);
    BLPoint p1(center.x, center.y + _radius);
    BLPoint p2(center.x - _radius, center.y);
    BLPoint p3(center.x, center.y - _radius);

    cusp_out.move_to(p0);
    cusp_out.arc_quadrant_to(BLPoint(center.x + _radius, center.y + _radius), p1);
    cusp_out.arc_quadrant_to(BLPoint(center.x - _radius, center.y + _radius), p2);
    cusp_out.arc_quadrant_to(BLPoint(center.x - _radius, center.y - _radius), p3);
    cusp_out.arc_quadrant_to(BLPoint(center.x + _radius, center.y - _radius), p0);
    cusp_out.close();
    cusp_out.done(&_cusp_path);
    return BL_SUCCESS;
  }

  BL_INLINE BLResult quad_to(const BLPoint& pt1, const BLPoint& pt2) noexcept {
    BLPoint quad[3] { _prev_pt, pt1, pt2 };
    BLPoint reduction;

    ReductionType reduction_type = check_quad_linear(quad, &reduction);
    if (reduction_type == ReductionType::kPoint || reduction_type == ReductionType::kLine)
      return line_to(pt2);

    if (reduction_type == ReductionType::kDegenerate) {
      BL_PROPAGATE(line_to(reduction));
      _join_override = BL_STROKE_JOIN_ROUND;
      BLResult result = line_to(pt2);
      _join_override = UINT32_MAX;
      return result;
    }

    BLPoint normal_ab;
    BLPoint unit_ab;
    BLPoint normal_bc;
    BLPoint unit_bc;

    if (!pre_join_to(pt1, &normal_ab, &unit_ab, false))
      return line_to(pt2);

    QuadConstruct quad_pts;
    init(StrokeType::kOuter, &quad_pts, 0.0, 1.0);
    BL_PROPAGATE(quad_stroke(quad, &quad_pts));

    init(StrokeType::kInner, &quad_pts, 0.0, 1.0);
    BL_PROPAGATE(quad_stroke(quad, &quad_pts));

    set_quad_end_normal(quad, normal_ab, unit_ab, &normal_bc, &unit_bc);

    post_join_to(pt2, normal_bc, unit_bc);
    return BL_SUCCESS;
  }

  BL_INLINE BLResult conic_to(const BLPoint& pt1, const BLPoint& pt2, double weight) noexcept {
    BLPoint conic[4];
    conic[0] = _prev_pt;
    conic[1] = pt1;
    conic[2].reset(weight, Math::nan<double>());
    conic[3] = pt2;

    BLPoint reduction;
    ReductionType reduction_type = check_conic_linear(conic, &reduction);
    if (reduction_type == ReductionType::kPoint || reduction_type == ReductionType::kLine)
      return line_to(pt2);

    if (reduction_type == ReductionType::kDegenerate) {
      BL_PROPAGATE(line_to(reduction));
      _join_override = BL_STROKE_JOIN_ROUND;
      BLResult result = line_to(pt2);
      _join_override = UINT32_MAX;
      return result;
    }

    BLPoint normal_ab;
    BLPoint unit_ab;
    BLPoint normal_bc;
    BLPoint unit_bc;

    if (!pre_join_to(pt1, &normal_ab, &unit_ab, false))
      return line_to(pt2);

    QuadConstruct quad_pts;
    init(StrokeType::kOuter, &quad_pts, 0.0, 1.0);
    BL_PROPAGATE(conic_stroke(conic, &quad_pts));

    init(StrokeType::kInner, &quad_pts, 0.0, 1.0);
    BL_PROPAGATE(conic_stroke(conic, &quad_pts));

    set_conic_end_normal(conic, normal_ab, unit_ab, &normal_bc, &unit_bc);

    post_join_to(pt2, normal_bc, unit_bc);
    return BL_SUCCESS;
  }

  BL_INLINE BLResult cubic_to(const BLPoint& pt1, const BLPoint& pt2, const BLPoint& pt3) noexcept {
    BLPoint cubic[4] { _prev_pt, pt1, pt2, pt3 };
    BLPoint reduction[3];
    const BLPoint* tangent_pt = nullptr;

    ReductionType reduction_type = check_cubic_linear(cubic, reduction, &tangent_pt);
    if (reduction_type == ReductionType::kPoint || reduction_type == ReductionType::kLine)
      return line_to(pt3);

    if (reduction_type == ReductionType::kDegenerate ||
        reduction_type == ReductionType::kDegenerate2 ||
        reduction_type == ReductionType::kDegenerate3) {
      BL_PROPAGATE(line_to(reduction[0]));
      _join_override = BL_STROKE_JOIN_ROUND;

      BLResult result = BL_SUCCESS;
      if (reduction_type >= ReductionType::kDegenerate2)
        result |= line_to(reduction[1]);
      if (reduction_type == ReductionType::kDegenerate3)
        result |= line_to(reduction[2]);
      result |= line_to(pt3);

      _join_override = UINT32_MAX;
      return result;
    }

    BLPoint normal_ab;
    BLPoint unit_ab;
    BLPoint normal_cd;
    BLPoint unit_cd;

    if (!pre_join_to(*tangent_pt, &normal_ab, &unit_ab, false))
      return line_to(pt3);

    double t_values[2];
    size_t count = find_cubic_inflections(cubic, t_values);
    double last_t = 0.0;

    for (size_t i = 0; i <= count; i++) {
      double next_t = i < count ? t_values[i] : 1.0;
      QuadConstruct quad_pts;

      init(StrokeType::kOuter, &quad_pts, last_t, next_t);
      BL_PROPAGATE(cubic_stroke(cubic, &quad_pts));

      init(StrokeType::kInner, &quad_pts, last_t, next_t);
      BL_PROPAGATE(cubic_stroke(cubic, &quad_pts));

      last_t = next_t;
    }

    double cusp_t = find_cubic_cusp(cubic);
    if (cusp_t > 0.0) {
      BLPoint cusp_loc = Geometry::evaluate_precise(Geometry::cubic_ref(cubic), cusp_t);
      BL_PROPAGATE(append_cusp_circle(cusp_loc));
    }

    set_cubic_end_normal(cubic, normal_ab, unit_ab, &normal_cd, &unit_cd);

    post_join_to(pt3, normal_cd, unit_cd);
    return BL_SUCCESS;
  }

  BL_INLINE BLResult finish_contour(bool close) noexcept {
    _c_path->clear();

    if (_segment_count <= 0)
      return BL_SUCCESS;

    if (close) {
      BLPoint close_normal;
      BLPoint close_unit_normal;

      if (set_normal_unitnormal(_prev_pt, _first_pt, 1.0, _radius, &close_normal, &close_unit_normal)) {
        BL_PROPAGATE(ensure_appenders_capacity(2 * kStrokeMaxJoinVertices + 1,
                                               2 * kStrokeMaxJoinVertices + 1,
                                               2 * kStrokeMaxJoinConics,
                                               2 * kStrokeMaxJoinConics));

        JoinSideSelection close_selection = select_join_sides(_a_out, _b_out, _prev_pt, _prev_unit_normal, close_unit_normal);
        emit_join(close_selection, _prev_pt, _prev_is_line, true);
        line_to_selected_segment_end(close_selection, _first_pt);
        join_to(close_unit_normal, _first_pt, _first_unit_normal, true, _first_is_line);
      }
      else {
        BL_PROPAGATE(ensure_appenders_capacity(kStrokeMaxJoinVertices,
                                               kStrokeMaxJoinVertices,
                                               kStrokeMaxJoinConics,
                                               kStrokeMaxJoinConics));
        join_to(_prev_unit_normal, _prev_pt, _first_unit_normal, _prev_is_line, _first_is_line);
      }

      _a_out.close();
      _b_out.close();
      return BL_SUCCESS;
    }

    uint32_t start_cap = sanitize_stroke_cap(_options.start_cap);
    uint32_t end_cap = sanitize_stroke_cap(_options.end_cap);

    BL_PROPAGATE(_a_out.ensure(_a_path, cap_vertex_count_table[end_cap], cap_conic_count_table[end_cap]));
    BL_PROPAGATE(add_cap(_a_out, _prev_pt, _b_out.vtx[-1], end_cap));

    PathAppender c_out;
    BL_PROPAGATE(c_out.begin(_c_path, BL_MODIFY_OP_ASSIGN_GROW, cap_vertex_count_table[start_cap] + 1, cap_conic_count_table[start_cap]));
    c_out.move_to(_b_path->vertex_data()[_b_figure_offset]);
    BL_PROPAGATE(add_cap(c_out, _first_pt, _a_path->vertex_data()[_a_figure_offset], start_cap));
    c_out.done(_c_path);

    return BL_SUCCESS;
  }

  BL_INLINE BLResult stroke(BLPathStrokeSinkFunc sink, void* user_data) noexcept {
    size_t figure_start_idx = 0;
    size_t estimated_size = _iter.remaining_forward() * 3u;

    BL_PROPAGATE(_a_path->reserve(_a_path->size() + estimated_size));

    while (!_iter.at_end()) {
      const uint8_t* figure_start_cmd = _iter.cmd;
      if (BL_UNLIKELY(_iter.cmd[0] != BL_PATH_CMD_MOVE)) {
        if (_iter.cmd[0] != BL_PATH_CMD_CLOSE)
          return bl_make_error(BL_ERROR_INVALID_GEOMETRY);

        _iter++;
        figure_start_idx++;
        continue;
      }

      _a_figure_offset = _a_path->size();
      _b_figure_offset = 0;
      _cusp_path.clear();

      BL_PROPAGATE(_a_out.begin(_a_path, BL_MODIFY_OP_APPEND_GROW, _iter.remaining_forward() * 2u, _iter.remaining_forward()));
      BL_PROPAGATE(_b_out.begin(_b_path, BL_MODIFY_OP_ASSIGN_GROW, _iter.remaining_forward() * 2u, _iter.remaining_forward()));

      move_to(*_iter.vtx);
      _iter++;

      bool contour_closed = false;
      while (!_iter.at_end()) {
        BL_PROPAGATE(ensure_appenders_capacity(kStrokeMaxJoinVertices, kStrokeMaxJoinVertices, kStrokeMaxJoinConics, kStrokeMaxJoinConics));

        uint8_t cmd = _iter.cmd[0];
        if (cmd == BL_PATH_CMD_ON) {
          BLPoint p1 = _iter.vtx[0];
          _iter++;
          BL_PROPAGATE(line_to(p1, &_iter));
          continue;
        }

        if (cmd == BL_PATH_CMD_QUAD) {
          if (_iter.remaining_forward() < 2)
            return bl_make_error(BL_ERROR_INVALID_GEOMETRY);

          BLPoint p1 = _iter.vtx[0];
          BLPoint p2 = _iter.vtx[1];
          _iter += 2;
          BL_PROPAGATE(quad_to(p1, p2));
          continue;
        }

        if (cmd == BL_PATH_CMD_CONIC) {
          if (_iter.remaining_forward() < 2)
            return bl_make_error(BL_ERROR_INVALID_GEOMETRY);

          BLPoint p1 = _iter.vtx[0];
          BLPoint p2 = _iter.vtx[1];
          double w = _iter.conic_weight_data[0];
          _iter += 2;
          BL_PROPAGATE(conic_to(p1, p2, w));
          continue;
        }

        if (cmd == BL_PATH_CMD_CUBIC) {
          if (_iter.remaining_forward() < 3)
            return bl_make_error(BL_ERROR_INVALID_GEOMETRY);

          BLPoint p1 = _iter.vtx[0];
          BLPoint p2 = _iter.vtx[1];
          BLPoint p3 = _iter.vtx[2];
          _iter += 3;
          BL_PROPAGATE(cubic_to(p1, p2, p3));
          continue;
        }

        if (cmd == BL_PATH_CMD_CLOSE) {
          _iter++;
          if (has_non_butt_caps(_options) && (has_only_move_to() || is_current_contour_empty())) {
            BL_PROPAGATE(line_to(move_to_pt()));
          }
          else {
            contour_closed = true;
          }
          break;
        }

        break;
      }

      BL_PROPAGATE(finish_contour(contour_closed));

      _a_out.done(_a_path);
      _b_out.done(_b_path);

      if (!_cusp_path.is_empty())
        BL_PROPAGATE(_a_path->add_path(_cusp_path));

      size_t figure_end_idx = figure_start_idx + size_t(_iter.cmd - figure_start_cmd);
      BL_PROPAGATE(sink(_a_path, _b_path, _c_path, figure_start_idx, figure_end_idx, user_data));
      figure_start_idx = figure_end_idx;
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
  BLPathStrokeSinkFunc sink, void* user_data) noexcept {

  return PathStroker(input, options, approx, &a, &b, &c).stroke(sink, user_data);
}

} // {PathInternal}
} // {bl}
