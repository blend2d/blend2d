// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_EDGE_BUILDER_P_H_INCLUDED
#define BLEND2D_RASTER_EDGE_BUILDER_P_H_INCLUDED

#include <blend2d/axl/algorithm_p.h>
#include <blend2d/axl/arena_allocator_p.h>
#include <blend2d/axl/commons.h>
#include <blend2d/axl/math_p.h>
#include <blend2d/core/path_p.h>
#include <blend2d/geometry/bezier_p.h>
#include <blend2d/pipeline/pipe_defs_p.h>
#include <blend2d/raster/edge_storage_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace bl::RasterEngine {

enum ClipShift : uint32_t {
  kClipShiftX0  = 0,
  kClipShiftY0  = 1,
  kClipShiftX1  = 2,
  kClipShiftY1  = 3
};

enum ClipFlags: uint32_t {
  kClipFlagNone = 0u,
  kClipFlagX0   = 1u << kClipShiftX0,
  kClipFlagY0   = 1u << kClipShiftY0,
  kClipFlagX1   = 1u << kClipShiftX1,
  kClipFlagY1   = 1u << kClipShiftY1,

  kClipFlagX0X1 = kClipFlagX0 | kClipFlagX1,
  kClipFlagY0Y1 = kClipFlagY0 | kClipFlagY1,

  kClipFlagX0Y0 = kClipFlagX0 | kClipFlagY0,
  kClipFlagX1Y0 = kClipFlagX1 | kClipFlagY0,

  kClipFlagX0Y1 = kClipFlagX0 | kClipFlagY1,
  kClipFlagX1Y1 = kClipFlagX1 | kClipFlagY1
};

static BL_INLINE uint32_t bl_clip_calc_x0_flags(const BLPoint& pt, const BLBox& box) noexcept { return (uint32_t(!(pt.x >= box.x0)) << kClipShiftX0); }
static BL_INLINE uint32_t bl_clip_calc_x1_flags(const BLPoint& pt, const BLBox& box) noexcept { return (uint32_t(!(pt.x <= box.x1)) << kClipShiftX1); }
static BL_INLINE uint32_t bl_clip_calc_y0_flags(const BLPoint& pt, const BLBox& box) noexcept { return (uint32_t(!(pt.y >= box.y0)) << kClipShiftY0); }
static BL_INLINE uint32_t bl_clip_calc_y1_flags(const BLPoint& pt, const BLBox& box) noexcept { return (uint32_t(!(pt.y <= box.y1)) << kClipShiftY1); }

static BL_INLINE uint32_t bl_clip_calc_x_flags(const BLPoint& pt, const BLBox& box) noexcept { return bl_clip_calc_x0_flags(pt, box) | bl_clip_calc_x1_flags(pt, box); }
static BL_INLINE uint32_t bl_clip_calc_y_flags(const BLPoint& pt, const BLBox& box) noexcept { return bl_clip_calc_y0_flags(pt, box) | bl_clip_calc_y1_flags(pt, box); }
static BL_INLINE uint32_t bl_clip_calc_xy_flags(const BLPoint& pt, const BLBox& box) noexcept { return bl_clip_calc_x_flags(pt, box) | bl_clip_calc_y_flags(pt, box); }

//! \name Edge Transformations
//! \{

class EdgeTransformNone {
public:
  BL_INLINE EdgeTransformNone() noexcept {}
  BL_INLINE void apply(BLPoint& dst, const BLPoint& src) noexcept { dst = src; }
};

class EdgeTransformScale {
public:
  double _sx, _sy;
  double _tx, _ty;

  BL_INLINE EdgeTransformScale(const BLMatrix2D& transform) noexcept
    : _sx(transform.m00),
      _sy(transform.m11),
      _tx(transform.m20),
      _ty(transform.m21) {}
  BL_INLINE EdgeTransformScale(const EdgeTransformScale& other) noexcept = default;

  BL_INLINE void apply(BLPoint& dst, const BLPoint& src) noexcept {
    dst.reset(Math::madd(src.x, _sx, _tx), Math::madd(src.y, _sy, _ty));
  }
};

class EdgeTransformAffine {
public:
  BLMatrix2D _transform;

  BL_INLINE EdgeTransformAffine(const BLMatrix2D& transform) noexcept
    : _transform(transform) {}
  BL_INLINE EdgeTransformAffine(const EdgeTransformAffine& other) noexcept = default;

  BL_INLINE void apply(BLPoint& dst, const BLPoint& src) noexcept {
    dst = BLPoint(Math::madd(src.y, _transform.m10, Math::madd(src.x, _transform.m00, _transform.m20)),
                  Math::madd(src.y, _transform.m11, Math::madd(src.x, _transform.m01, _transform.m21)));
  }
};

//! \}

//! \name Edge Source Data
//! \{

template<class PointType, class Transform = EdgeTransformNone>
class EdgeSourcePoly {
public:
  Transform _transform;
  const PointType* _src_ptr;
  const PointType* _src_end;

  BL_INLINE EdgeSourcePoly(const Transform& transform) noexcept
    : _transform(transform),
      _src_ptr(nullptr),
      _src_end(nullptr) {}

  BL_INLINE EdgeSourcePoly(const Transform& transform, const PointType* src_ptr, size_t count) noexcept
    : _transform(transform),
      _src_ptr(src_ptr),
      _src_end(src_ptr + count) {}

  BL_INLINE void reset(const PointType* src_ptr, size_t count) noexcept {
    _src_ptr = src_ptr;
    _src_end = src_ptr + count;
  }

  BL_INLINE bool begin(BLPoint& initial) noexcept {
    if (_src_ptr == _src_end)
      return false;

    _transform.apply(initial, BLPoint(_src_ptr[0].x, _src_ptr[0].y));
    _src_ptr++;
    return true;
  }

  BL_INLINE bool has_command() const noexcept { return _src_ptr != _src_end; }

  BL_INLINE bool is_line_to() const noexcept {
    BL_ASSERT(has_command());
    return true;
  }

  BL_INLINE constexpr bool is_quad_to() const noexcept { return false; }
  BL_INLINE constexpr bool is_cubic_to() const noexcept { return false; }
  BL_INLINE constexpr bool is_conic_to() const noexcept { return false; }
  BL_INLINE constexpr bool is_figure_end() const noexcept { return false; }

  BL_INLINE bool handle_figure_end() noexcept { return false; }

  BL_INLINE void next_line_to(BLPoint& pt1) noexcept {
    _transform.apply(pt1, BLPoint(_src_ptr[0].x, _src_ptr[0].y));
    _src_ptr++;
  }

  BL_INLINE bool maybe_next_line_to(BLPoint& pt1) noexcept {
    if (_src_ptr == _src_end)
      return false;

    next_line_to(pt1);
    return true;
  }

  BL_INLINE_CONSTEXPR void next_quad_to(BLPoint&, BLPoint&) const noexcept {}
  BL_INLINE_CONSTEXPR bool maybe_next_quad_to(BLPoint&, BLPoint&) const noexcept { return false; }

  BL_INLINE_CONSTEXPR void next_cubic_to(BLPoint&, BLPoint&, BLPoint&) const noexcept {}
  BL_INLINE_CONSTEXPR bool maybe_next_cubic_to(BLPoint&, BLPoint&, BLPoint&) const noexcept { return false; }

  BL_INLINE_CONSTEXPR void next_conic_to(BLPoint&, BLPoint&, double&) const noexcept {}
  BL_INLINE_CONSTEXPR bool maybe_next_conic_to(BLPoint&, BLPoint&, double&) const noexcept { return false; }
};

template<class Transform = EdgeTransformNone>
class EdgeSourcePath {
public:
  PathIterator _it;
  Transform _transform;

  BL_INLINE EdgeSourcePath(const Transform& transform) noexcept
    : _it{},
      _transform(transform) {}

  BL_INLINE EdgeSourcePath(const Transform& transform, PathView view) noexcept
    : _it(view),
      _transform(transform) {}

  BL_INLINE void reset() noexcept { _it.reset(); }
  BL_INLINE void reset(BLPathView view) noexcept { _it.reset(view); }

  BL_INLINE bool begin(BLPoint& initial) noexcept {
    if (!_it.has_command()) {
      return false;
    }

    if (_it.command() != BL_PATH_CMD_MOVE) {
      return false;
    }

    _transform.apply(initial, _it.vertex(0));
    _it.advance<BL_PATH_CMD_MOVE>();
    return true;
  }

  BL_INLINE bool has_command() const noexcept { return _it.has_command(); }
  BL_INLINE bool is_line_to() const noexcept { return _it.is_line_to(); }
  BL_INLINE bool is_quad_to() const noexcept { return _it.is_quad_to(); }
  BL_INLINE bool is_conic_to() const noexcept { return _it.is_conic_to(); }
  BL_INLINE bool is_cubic_to() const noexcept { return _it.is_cubic_to(); }
  BL_INLINE bool is_figure_end() const noexcept { return PathDetail::is_move_or_close_cmd(_it.command()); }

  BL_INLINE bool handle_figure_end() noexcept {
    // When figure-end is handled, it has to point to either MOVE or CLOSE.
    BL_ASSERT(_it.has_command());

    bool is_close = _it.is_close();
    _it.advance_vertex_and_control(0, size_t(is_close));
    return is_close;
  }

  BL_INLINE void next_line_to(BLPoint& pt1) noexcept {
    _transform.apply(pt1, _it.vertex(0));
    _it.advance<BL_PATH_CMD_LINE>();
  }

  BL_INLINE bool maybe_next_line_to(BLPoint& pt1) noexcept {
    if (!has_command() || !is_line_to()) {
      return false;
    }

    next_line_to(pt1);
    return true;
  }

  BL_INLINE void next_quad_to(BLPoint& pt1, BLPoint& pt2) noexcept {
    _transform.apply(pt1, _it.vertex(0));
    _transform.apply(pt2, _it.vertex(1));
    _it.advance<BL_PATH_CMD_QUAD>();
  }

  BL_INLINE bool maybe_next_quad_to(BLPoint& pt1, BLPoint& pt2) noexcept {
    if (!has_command() || !is_quad_to()) {
      return false;
    }

    next_quad_to(pt1, pt2);
    return true;
  }

  BL_INLINE void next_cubic_to(BLPoint& pt1, BLPoint& pt2, BLPoint& pt3) noexcept {
    _transform.apply(pt1, _it.vertex(0));
    _transform.apply(pt2, _it.vertex(1));
    _transform.apply(pt3, _it.vertex(2));
    _it.advance<BL_PATH_CMD_CUBIC>();
  }

  BL_INLINE bool maybe_next_cubic_to(BLPoint& pt1, BLPoint& pt2, BLPoint& pt3) noexcept {
    if (!has_command() || !is_cubic_to()) {
      return false;
    }

    next_cubic_to(pt1, pt2, pt3);
    return true;
  }

  BL_INLINE void next_conic_to(BLPoint& pt1, BLPoint& pt2, double& w) noexcept {
    _transform.apply(pt1, _it.vertex(0));
    _transform.apply(pt2, _it.vertex(1));
    w = _it.weight();
    _it.advance<BL_PATH_CMD_CONIC>();
  }

  BL_INLINE bool maybe_next_conic_to(BLPoint& pt1, BLPoint& pt2, double& w) noexcept {
    if (!has_command() || !is_conic_to()) {
      return false;
    }

    next_conic_to(pt1, pt2, w);
    return true;
  }
};

template<class PointType>
using EdgeSourcePolyScale = EdgeSourcePoly<PointType, EdgeTransformScale>;

template<class PointType>
using EdgeSourcePolyAffine = EdgeSourcePoly<PointType, EdgeTransformAffine>;

typedef EdgeSourcePath<EdgeTransformScale> EdgeSourcePathScale;
typedef EdgeSourcePath<EdgeTransformAffine> EdgeSourcePathAffine;

//! \}

//! \name Edge Flattening
//! \{

//! Base data (mostly stack) used by `FlattenMonoQuad` and `FlattenMonoCubic`.
class FlattenMonoData {
public:
  enum : size_t {
    kRecursionLimit = 32,

    kStackSizeQuad  = kRecursionLimit * 3,
    kStackSizeCubic = kRecursionLimit * 4,
    kStackSizeTotal = kStackSizeCubic
  };

  BLPoint _stack[kStackSizeTotal];
  // Per-frame weights for rational (conic) subdivision - one weight per pushed stack frame.
  double _w_stack[kRecursionLimit];
};

//! Helper to flatten a monotonic quad curve.
class FlattenMonoQuad {
public:
  FlattenMonoData& _flatten_data;
  double _tolerance_sq;
  BLPoint* _stack_ptr;
  BLPoint _p0, _p1, _p2;

  struct SplitStep {
    BL_INLINE bool is_finite() const noexcept { return Math::is_finite(value); }
    BL_INLINE const BLPoint& mid_point() const noexcept { return p012; }

    double value;
    double limit;

    BLPoint p01;
    BLPoint p12;
    BLPoint p012;
  };

  BL_INLINE explicit FlattenMonoQuad(FlattenMonoData& flatten_data, double tolerance_sq) noexcept
    : _flatten_data(flatten_data),
      _tolerance_sq(tolerance_sq) {}

  BL_INLINE void begin(const BLPoint* src, uint32_t sign_bit) noexcept {
    _stack_ptr = _flatten_data._stack;

    if (sign_bit == 0) {
      _p0 = src[0];
      _p1 = src[1];
      _p2 = src[2];
    }
    else {
      _p0 = src[2];
      _p1 = src[1];
      _p2 = src[0];
    }
  }

  BL_INLINE const BLPoint& first() const noexcept { return _p0; }
  BL_INLINE const BLPoint& last() const noexcept { return _p2; }

  BL_INLINE bool can_pop() const noexcept { return _stack_ptr != _flatten_data._stack; }
  BL_INLINE bool can_push() const noexcept { return _stack_ptr != _flatten_data._stack + FlattenMonoData::kStackSizeQuad; }

  BL_INLINE bool is_left_to_right() const noexcept { return first().x < last().x; }

  // Caused by floating point inaccuracy, we must bound the control point as we really need
  // monotonic curve that would never outbound the boundary defined by its start/end points.
  BL_INLINE void bound_left_to_right() noexcept {
    _p1.x = bl_clamp(_p1.x, _p0.x, _p2.x);
    _p1.y = bl_clamp(_p1.y, _p0.y, _p2.y);
  }

  BL_INLINE void bound_right_to_left() noexcept {
    _p1.x = bl_clamp(_p1.x, _p2.x, _p0.x);
    _p1.y = bl_clamp(_p1.y, _p0.y, _p2.y);
  }

  BL_INLINE bool is_flat(SplitStep& step) const noexcept {
    BLPoint v1 = _p1 - _p0;
    BLPoint v2 = _p2 - _p0;

    double d = Geometry::cross(v2, v1);
    double len_sq = Geometry::magnitude_squared(v2);

    step.value = d * d;
    step.limit = _tolerance_sq * len_sq;

    return step.value <= step.limit;
  }

  BL_INLINE void split(SplitStep& step) const noexcept {
    step.p01 = (_p0 + _p1) * 0.5;
    step.p12 = (_p1 + _p2) * 0.5;
    step.p012 = (step.p01 + step.p12) * 0.5;
  }

  BL_INLINE void push(const SplitStep& step) noexcept {
    // Must be checked before calling `push()`.
    BL_ASSERT(can_push());

    _stack_ptr[0].reset(step.p012);
    _stack_ptr[1].reset(step.p12);
    _stack_ptr[2].reset(_p2);
    _stack_ptr += 3;

    _p1 = step.p01;
    _p2 = step.p012;
  }

  BL_INLINE void discard_and_advance(const SplitStep& step) noexcept {
    _p0 = step.p012;
    _p1 = step.p12;
  }

  BL_INLINE void pop() noexcept {
    _stack_ptr -= 3;
    _p0 = _stack_ptr[0];
    _p1 = _stack_ptr[1];
    _p2 = _stack_ptr[2];
  }
};

//! Helper to flatten a monotonic cubic curve.
class FlattenMonoCubic {
public:
  FlattenMonoData& _flatten_data;
  double _tolerance_sq;
  BLPoint* _stack_ptr;
  BLPoint _p0, _p1, _p2, _p3;

  struct SplitStep {
    BL_INLINE bool is_finite() const noexcept { return Math::is_finite(value); }
    BL_INLINE const BLPoint& mid_point() const noexcept { return p0123; }

    double value;
    double limit;

    BLPoint p01;
    BLPoint p12;
    BLPoint p23;
    BLPoint p012;
    BLPoint p123;
    BLPoint p0123;
  };

  BL_INLINE explicit FlattenMonoCubic(FlattenMonoData& flatten_data, double tolerance_sq) noexcept
    : _flatten_data(flatten_data),
      _tolerance_sq(tolerance_sq) {}

  BL_INLINE void begin(const BLPoint* src, uint32_t sign_bit) noexcept {
    _stack_ptr = _flatten_data._stack;

    if (sign_bit == 0) {
      _p0 = src[0];
      _p1 = src[1];
      _p2 = src[2];
      _p3 = src[3];
    }
    else {
      _p0 = src[3];
      _p1 = src[2];
      _p2 = src[1];
      _p3 = src[0];
    }
  }

  BL_INLINE const BLPoint& first() const noexcept { return _p0; }
  BL_INLINE const BLPoint& last() const noexcept { return _p3; }

  BL_INLINE bool can_pop() const noexcept { return _stack_ptr != _flatten_data._stack; }
  BL_INLINE bool can_push() const noexcept { return _stack_ptr != _flatten_data._stack + FlattenMonoData::kStackSizeCubic; }

  BL_INLINE bool is_left_to_right() const noexcept { return first().x < last().x; }

  // Caused by floating point inaccuracy, we must bound the control point as we really need
  // monotonic curve that would never outbound the boundary defined by its start/end points.
  BL_INLINE void bound_left_to_right() noexcept {
    _p1.x = bl_clamp(_p1.x, _p0.x, _p3.x);
    _p1.y = bl_clamp(_p1.y, _p0.y, _p3.y);
    _p2.x = bl_clamp(_p2.x, _p0.x, _p3.x);
    _p2.y = bl_clamp(_p2.y, _p0.y, _p3.y);
  }

  BL_INLINE void bound_right_to_left() noexcept {
    _p1.x = bl_clamp(_p1.x, _p3.x, _p0.x);
    _p1.y = bl_clamp(_p1.y, _p0.y, _p3.y);
    _p2.x = bl_clamp(_p2.x, _p3.x, _p0.x);
    _p2.y = bl_clamp(_p2.y, _p0.y, _p3.y);
  }

  BL_INLINE bool is_flat(SplitStep& step) const noexcept {
    BLPoint v = _p3 - _p0;

    double d1_sq = Math::square(Geometry::cross(v, _p1 - _p0));
    double d2_sq = Math::square(Geometry::cross(v, _p2 - _p0));
    double len_sq = Geometry::magnitude_squared(v);

    step.value = bl_max(d1_sq, d2_sq);
    step.limit = _tolerance_sq * len_sq;

    return step.value <= step.limit;
  }

  BL_INLINE void split(SplitStep& step) const noexcept {
    step.p01 = (_p0 + _p1) * 0.5;
    step.p12 = (_p1 + _p2) * 0.5;
    step.p23 = (_p2 + _p3) * 0.5;
    step.p012 = (step.p01 + step.p12 ) * 0.5;
    step.p123 = (step.p12 + step.p23 ) * 0.5;
    step.p0123 = (step.p012 + step.p123) * 0.5;
  }

  BL_INLINE void push(const SplitStep& step) noexcept {
    // Must be checked before calling `push()`.
    BL_ASSERT(can_push());

    _stack_ptr[0].reset(step.p0123);
    _stack_ptr[1].reset(step.p123);
    _stack_ptr[2].reset(step.p23);
    _stack_ptr[3].reset(_p3);
    _stack_ptr += 4;

    _p1 = step.p01;
    _p2 = step.p012;
    _p3 = step.p0123;
  }

  BL_INLINE void discard_and_advance(const SplitStep& step) noexcept {
    _p0 = step.p0123;
    _p1 = step.p123;
    _p2 = step.p23;
  }

  BL_INLINE void pop() noexcept {
    _stack_ptr -= 4;
    _p0 = _stack_ptr[0];
    _p1 = _stack_ptr[1];
    _p2 = _stack_ptr[2];
    _p3 = _stack_ptr[3];
  }
};

//! Helper to flatten a monotonic quad curve.
class FlattenMonoConic {
public:
  FlattenMonoData& _flatten_data;
  double _tolerance_sq;
  BLPoint* _stack_ptr;
  double* _w_stack_ptr;
  BLPoint _p0, _p1, _p2;
  // Weight of the conic segment currently being flattened.
  double _w;

  struct SplitStep {
    BL_INLINE bool is_finite() const noexcept { return Math::is_finite(value); }
    BL_INLINE const BLPoint& mid_point() const noexcept { return p012; }

    double value;
    double limit;

    BLPoint p01;
    BLPoint p12;
    BLPoint p012;
    // Weight shared by both halves produced by subdividing this segment at its midpoint.
    double w;
  };

  BL_INLINE explicit FlattenMonoConic(FlattenMonoData& flatten_data, double tolerance_sq) noexcept
    : _flatten_data(flatten_data),
      _tolerance_sq(tolerance_sq) {}

  // Must be called before `begin()` to set the weight of the conic segment to flatten.
  BL_INLINE void set_weight(double w) noexcept { _w = w; }

  BL_INLINE void begin(const BLPoint* src, uint32_t sign_bit) noexcept {
    _stack_ptr = _flatten_data._stack;
    _w_stack_ptr = _flatten_data._w_stack;

    if (sign_bit == 0) {
      _p0 = src[0];
      _p1 = src[1];
      _p2 = src[2];
    }
    else {
      _p0 = src[2];
      _p1 = src[1];
      _p2 = src[0];
    }
  }

  BL_INLINE const BLPoint& first() const noexcept { return _p0; }
  BL_INLINE const BLPoint& last() const noexcept { return _p2; }

  BL_INLINE bool can_pop() const noexcept { return _stack_ptr != _flatten_data._stack; }
  BL_INLINE bool can_push() const noexcept { return _stack_ptr != _flatten_data._stack + FlattenMonoData::kStackSizeQuad; }

  BL_INLINE bool is_left_to_right() const noexcept { return first().x < last().x; }

  // Caused by floating point inaccuracy, we must bound the control
  // point as we really need monotonic curve that would never outbound
  // the boundary defined by its start/end points.
  BL_INLINE void bound_left_to_right() noexcept {
    _p1.x = bl_clamp(_p1.x, _p0.x, _p2.x);
    _p1.y = bl_clamp(_p1.y, _p0.y, _p2.y);
  }

  BL_INLINE void bound_right_to_left() noexcept {
    _p1.x = bl_clamp(_p1.x, _p2.x, _p0.x);
    _p1.y = bl_clamp(_p1.y, _p0.y, _p2.y);
  }

  BL_INLINE bool is_flat(SplitStep& step) const noexcept {
    // The conic lies within the convex hull of its control points, so the control-point distance from the
    // chord is a conservative upper bound on the curve's deviation - this never under-subdivides.
    BLPoint v1 = _p1 - _p0;
    BLPoint v2 = _p2 - _p0;

    double d = Geometry::cross(v2, v1);
    double len_sq = Geometry::magnitude_squared(v2);

    step.value = d * d;
    step.limit = _tolerance_sq * len_sq;

    return step.value <= step.limit;
  }

  BL_INLINE void split(SplitStep& step) const noexcept {
    // Rational de Casteljau subdivision at the midpoint. In projective space the control points are
    // `(p0, 1)`, `(w*p1, w)`, `(p2, 1)`; subdividing and projecting back yields:
    //   p01  = (p0 + w*p1) / (1 + w)
    //   p12  = (w*p1 + p2) / (1 + w)
    //   p012 = (p01 + p12) / 2
    // and both halves share the weight `sqrt((1 + w) / 2)`.
    double k = 1.0 / (1.0 + _w);

    step.p01 = (_p0 + _p1 * _w) * k;
    step.p12 = (_p1 * _w + _p2) * k;
    step.p012 = (step.p01 + step.p12) * 0.5;
    step.w = Math::sqrt((1.0 + _w) * 0.5);
  }

  BL_INLINE void push(const SplitStep& step) noexcept {
    // Must be checked before calling `push()`.
    BL_ASSERT(can_push());

    _stack_ptr[0].reset(step.p012);
    _stack_ptr[1].reset(step.p12);
    _stack_ptr[2].reset(_p2);
    _stack_ptr += 3;
    *_w_stack_ptr++ = step.w;

    _p1 = step.p01;
    _p2 = step.p012;
    _w = step.w;
  }

  BL_INLINE void discard_and_advance(const SplitStep& step) noexcept {
    _p0 = step.p012;
    _p1 = step.p12;
    _w = step.w;
  }

  BL_INLINE void pop() noexcept {
    _stack_ptr -= 3;
    _p0 = _stack_ptr[0];
    _p1 = _stack_ptr[1];
    _p2 = _stack_ptr[2];
    _w = *--_w_stack_ptr;
  }
};


//! \}

//! \name Edge Builder
//! \{

template<typename CoordT>
class EdgeBuilder {
public:
  static inline constexpr uint32_t kEdgeOffset = uint32_t(sizeof(EdgeVector<CoordT>) - sizeof(EdgePoint<CoordT>));
  static inline constexpr uint32_t kMinEdgeSize = uint32_t(sizeof(EdgeVector<CoordT>) + sizeof(EdgePoint<CoordT>));

  //! \name Edge Storage
  //! \{

  //! Arena memory used to allocate EdgeVector[].
  ArenaAllocator* _arena;
  //! Edge storage the builder adds edges to.
  EdgeStorage<int>* _storage;

  //! ClipBox already scaled to fixed-point in `double` precision.
  BLBox _clip_box_d;
  //! ClipBox already scaled to fixed-point (integral).
  BLBoxI _clip_box_i;
  //! Curve flattening tolerance
  double _flatten_tolerance_sq;

  //! \}

  //! \name Shorthands and Working Variables
  //! \{

  //! Bands (shortcut to `_storage->band_edges()`).
  EdgeList<CoordT>* _band_edges;
  //! Shift to get band_id from fixed coordinate (shortcut to `_storage->fixed_band_height_shift()`).
  uint32_t _fixed_band_height_shift;
  //! Current point in edge-vector.
  EdgePoint<CoordT>* _ptr;
  //! Last point the builder can go.
  EdgePoint<CoordT>* _end;

  //! Master sign bit, which is XORed with sign bit of a single edge or multiple edges.
  uint32_t _master_sign_bit;

  //! Current bounding box, must be flushed.
  BLBoxI _bbox_i;
  double _border_acc_x0_y0;
  double _border_acc_x0_y1;
  double _border_acc_x1_y0;
  double _border_acc_x1_y1;

  //! \}

  //! \name State
  //! \{

  //! Working state that is only used during path/poly processing.
  struct State {
    BLPoint a;
    uint32_t a_flags;
    FlattenMonoData flatten_data;
  };

  //! \}

  //! \name Appender
  //! \{

  struct Appender {
    EdgeBuilder<CoordT>& _builder;
    uint32_t _sign_bit;

    BL_INLINE Appender(EdgeBuilder& builder, uint32_t sign_bit = 0u) noexcept
      : _builder(builder),
        _sign_bit(sign_bit) {}

    BL_INLINE uint32_t sign_bit() noexcept { return _sign_bit; }
    BL_INLINE void set_sign_bit(uint32_t sign_bit) noexcept { _sign_bit = sign_bit; }

    BL_INLINE BLResult open_at(double x, double y) noexcept {
      int fx = Math::trunc_to_int(x);
      int fy = Math::trunc_to_int(y);

      BL_PROPAGATE(_builder.descending_open());
      _builder.descending_add_unsafe(fx, fy);

      return BL_SUCCESS;
    }

    BL_INLINE BLResult add_line(double x, double y) noexcept {
      int fx = Math::trunc_to_int(x);
      int fy = Math::trunc_to_int(y);

      return _builder.descending_add_checked(fx, fy, _sign_bit);
    }

    BL_INLINE BLResult close() noexcept {
      int fy0 = _builder.descending_first()->y;
      int fy1 = _builder.descending_last()->y;

      // Rare but happens, degenerated h-lines make no contribution.
      if (fy0 == fy1) {
        _builder.descending_cancel();
      }
      else {
        _builder._bbox_i.y0 = bl_min(_builder._bbox_i.y0, fy0);
        _builder._bbox_i.y1 = bl_max(_builder._bbox_i.y1, fy1);
        _builder.descending_close(_sign_bit);
      }

      return BL_SUCCESS;
    }
  };

  //! \}

  //! \name Construction & Destruction
  //! \{

  BL_INLINE EdgeBuilder(ArenaAllocator* zone, EdgeStorage<int>* storage) noexcept
    : EdgeBuilder(zone, storage, BLBox {}, 0.0) {}

  BL_INLINE EdgeBuilder(ArenaAllocator* zone, EdgeStorage<int>* storage, const BLBox& clip_box, double tolerance_sq) noexcept
    : _arena(zone),
      _storage(storage),
      _clip_box_d(clip_box),
      _clip_box_i(Math::trunc_to_int(clip_box.x0),
                  Math::trunc_to_int(clip_box.y0),
                  Math::trunc_to_int(clip_box.x1),
                  Math::trunc_to_int(clip_box.y1)),
      _flatten_tolerance_sq(tolerance_sq),
      _band_edges(nullptr),
      _fixed_band_height_shift(0),
      _ptr(nullptr),
      _end(nullptr),
      _master_sign_bit(0u),
      _bbox_i(axl::max_value<int>(), axl::max_value<int>(), axl::min_value<int>(), axl::min_value<int>()),
      _border_acc_x0_y0(clip_box.y0),
      _border_acc_x0_y1(clip_box.y0),
      _border_acc_x1_y0(clip_box.y0),
      _border_acc_x1_y1(clip_box.y0) {}

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE void set_clip_box(const BLBox& clip_box) noexcept {
    _clip_box_d.reset(clip_box);
    _clip_box_i.reset(Math::trunc_to_int(clip_box.x0),
                      Math::trunc_to_int(clip_box.y0),
                      Math::trunc_to_int(clip_box.x1),
                      Math::trunc_to_int(clip_box.y1));
  }

  BL_INLINE void set_flatten_tolerance_sq(double tolerance_sq) noexcept {
    _flatten_tolerance_sq = tolerance_sq;
  }

  BL_INLINE void merge_bounding_box() noexcept {
    Geometry::bound(_storage->_bounding_box, _bbox_i);
  }

  //! \}

  //! \name Begin & End
  //! \{

  BL_INLINE void begin() noexcept {
    _band_edges = _storage->band_edges();
    _fixed_band_height_shift = _storage->fixed_band_height_shift();
    _ptr = nullptr;
    _end = nullptr;
    _bbox_i.reset(axl::max_value<int>(), axl::max_value<int>(), axl::min_value<int>(), axl::min_value<int>());
    _border_acc_x0_y0 = _clip_box_d.y0;
    _border_acc_x0_y1 = _clip_box_d.y0;
    _border_acc_x1_y0 = _clip_box_d.y0;
    _border_acc_x1_y1 = _clip_box_d.y0;
  }

  BL_INLINE BLResult flip_master_sign() noexcept {
    _master_sign_bit ^= 1u;
    axl::swap(_border_acc_x0_y0, _border_acc_x0_y1);
    axl::swap(_border_acc_x1_y0, _border_acc_x1_y1);
    return BL_SUCCESS;
  }

  BL_INLINE BLResult done() noexcept {
    BL_PROPAGATE(flush_border_accumulators());
    reset_border_accumulators();
    merge_bounding_box();
    return BL_SUCCESS;
  }

  //! \}

  //! \name Begin + Add + End Shortcuts
  //! \{

  //! A convenience function that calls `begin()`, `add_poly()`, and `done()`.
  template<class PointType>
  BL_INLINE BLResult init_from_poly(const PointType* pts, size_t size, const BLMatrix2D& transform, BLTransformType transform_type) noexcept {
    begin();
    BL_PROPAGATE(add_poly(pts, size, transform, transform_type));
    return done();
  }

  //! A convenience function that calls `begin()`, `add_path()`, and `done()`.
  BL_INLINE BLResult init_from_path(const BLPathView& view, bool closed, const BLMatrix2D& transform, BLTransformType transform_type) noexcept {
    begin();
    BL_PROPAGATE(add_path(view, closed, transform, transform_type));
    return done();
  }

  //! \}

  //! \name Add Geometry
  //! \{

  template<class PointType>
  BL_NOINLINE BLResult _add_poly_scale(const PointType* pts, size_t size, const BLMatrix2D& transform) noexcept {
    EdgeSourcePolyScale<PointType> source(EdgeTransformScale(transform), pts, size);
    return add_from_source(source, true);
  }

  template<class PointType>
  BL_NOINLINE BLResult _add_poly_affine(const PointType* pts, size_t size, const BLMatrix2D& transform) noexcept {
    EdgeSourcePolyAffine<PointType> source(EdgeTransformAffine(transform), pts, size);
    return add_from_source(source, true);
  }

  BL_NOINLINE BLResult _add_path_scale(BLPathView view, bool closed, const BLMatrix2D& transform) noexcept {
    EdgeSourcePathScale source(EdgeTransformScale(transform), view);
    return add_from_source(source, closed);
  }

  BL_NOINLINE BLResult _add_path_affine(BLPathView view, bool closed, const BLMatrix2D& transform) noexcept {
    EdgeSourcePathAffine source(EdgeTransformAffine(transform), view);
    return add_from_source(source, closed);
  }

  template<class PointType>
  BL_INLINE BLResult add_poly(const PointType* pts, size_t size, const BLMatrix2D& transform, BLTransformType transform_type) noexcept {
    if (transform_type <= BL_TRANSFORM_TYPE_SCALE)
      return _add_poly_scale(pts, size, transform);
    else
      return _add_poly_affine(pts, size, transform);
  }

  BL_INLINE BLResult add_path(const BLPathView& view, bool closed, const BLMatrix2D& transform, BLTransformType transform_type) noexcept {
    if (transform_type <= BL_TRANSFORM_TYPE_SCALE)
      return _add_path_scale(view, closed, transform);
    else
      return _add_path_affine(view, closed, transform);
  }

  template<class Source>
  BL_INLINE BLResult add_from_source(Source& source, bool auto_close) noexcept {
    State state;
    while (source.begin(state.a)) {
      BLPoint start(state.a);
      BLPoint b;

      bool done = false;
      state.a_flags = bl_clip_calc_xy_flags(state.a, _clip_box_d);

      for (;;) {
        if (BL_LIKELY(source.has_command())) {
          if (source.is_line_to()) {
            source.next_line_to(b);
LineTo:
            BL_PROPAGATE(line_to(source, state, b));
            if (done) {
              break;
            }
          }
          else if (source.is_quad_to()) {
            BL_PROPAGATE(quad_to(source, state));
          }
          else if (source.is_cubic_to()) {
            BL_PROPAGATE(cubic_to(source, state));
          }
          else if (source.is_conic_to()) {
            BL_PROPAGATE(conic_to(source, state));
          }
          else {
            b = start;
            done = true;

            if (BL_LIKELY(source.is_figure_end())) {
              if (axl::bool_or(auto_close, source.handle_figure_end())) {
                goto LineTo;
              }
              break;
            }

            return bl_make_error(BL_ERROR_CORRUPTED_PATH_DATA);
          }
        }
        else {
          if (auto_close) {
            b = start;
            done = true;
            goto LineTo;
          }
          break;
        }
      }
    }

    return BL_SUCCESS;
  }

  BL_INLINE BLResult add_line_segment(double x0, double y0, double x1, double y1) noexcept {
    int fx0 = Math::trunc_to_int(x0);
    int fy0 = Math::trunc_to_int(y0);
    int fx1 = Math::trunc_to_int(x1);
    int fy1 = Math::trunc_to_int(y1);

    if (fy0 == fy1) {
      return BL_SUCCESS;
    }

    if (fy0 < fy1) {
      _bbox_i.y0 = bl_min(_bbox_i.y0, fy0);
      _bbox_i.y1 = bl_max(_bbox_i.y1, fy1);
      return add_closed_line(fx0, fy0, fx1, fy1, _master_sign_bit);
    }
    else {
      _bbox_i.y0 = bl_min(_bbox_i.y0, fy1);
      _bbox_i.y1 = bl_max(_bbox_i.y1, fy0);
      return add_closed_line(fx1, fy1, fx0, fy0, _master_sign_bit ^ 1u);
    }
  }

  BL_INLINE BLResult add_closed_line(CoordT x0, CoordT y0, CoordT x1, CoordT y1, uint32_t sign_bit) noexcept {
    // Must be correct, the rasterizer won't check this.
    BL_ASSERT(y0 < y1);

    EdgeVector<CoordT>* edge = static_cast<EdgeVector<CoordT>*>(_arena->alloc(kMinEdgeSize));
    if (BL_UNLIKELY(!edge))
      return bl_make_error(BL_ERROR_OUT_OF_MEMORY);

    edge->pts[0].reset(x0, y0);
    edge->pts[1].reset(x1, y1);
    edge->count_and_sign = pack_count_and_sign_bit(2, sign_bit);

    _link_edge(edge, y0);
    return BL_SUCCESS;
  }

  //! \}

  //! \name Low-Level API - Line To
  //! \{

  // Terminology:
  //
  //   'a' - Line start point.
  //   'b' - List end point.
  //
  //   'd' - Difference between 'b' and 'a'.
  //   'p' - Clipped start point.
  //   'q' - Clipped end point.

  template<class Source>
  BL_INLINE BLResult line_to(Source& source, State& state, BLPoint b) noexcept {
    BLPoint& a = state.a;
    uint32_t& a_flags = state.a_flags;

    BLPoint p, d;
    uint32_t b_flags;

    // EdgePoint coordinates.
    int fx0, fy0;
    int fx1, fy1;

    do {
      if (!a_flags) {
        // Line - Unclipped
        // ----------------

        b_flags = bl_clip_calc_xy_flags(b, _clip_box_d);
        if (!b_flags) {
          fx0 = Math::trunc_to_int(a.x);
          fy0 = Math::trunc_to_int(a.y);
          fx1 = Math::trunc_to_int(b.x);
          fy1 = Math::trunc_to_int(b.y);

          for (;;) {
            if (fy0 < fy1) {
DescendingLineBegin:
              BL_PROPAGATE(descending_open());
              descending_add_unsafe(fx0, fy0);
              descending_add_unsafe(fx1, fy1);
              _bbox_i.y0 = bl_min(_bbox_i.y0, fy0);

              // Instead of processing one vertex and swapping a/b each time
              // we process two vertices and swap only upon loop termination.
              for (;;) {
DescendingLineLoopA:
                if (!source.maybe_next_line_to(a)) {
                  descending_close();
                  _bbox_i.y1 = bl_max(_bbox_i.y1, fy1);
                  a = b;
                  return BL_SUCCESS;
                }

                b_flags = bl_clip_calc_xy_flags(a, _clip_box_d);
                if (b_flags) {
                  descending_close();
                  Internal::swap(a, b);
                  goto BeforeClipEndPoint;
                }

                fx0 = Math::trunc_to_int(a.x);
                fy0 = Math::trunc_to_int(a.y);

                if (fy0 < fy1) {
                  descending_close();
                  BL_PROPAGATE(ascending_open());
                  ascending_add_unsafe(fx1, fy1);
                  ascending_add_unsafe(fx0, fy0);
                  _bbox_i.y1 = bl_max(_bbox_i.y1, fy1);
                  goto AscendingLineLoopB;
                }
                BL_PROPAGATE(descending_add_checked(fx0, fy0));

DescendingLineLoopB:
                if (!source.maybe_next_line_to(b)) {
                  descending_close();
                  _bbox_i.y1 = bl_max(_bbox_i.y1, fy0);
                  return BL_SUCCESS;
                }

                b_flags = bl_clip_calc_xy_flags(b, _clip_box_d);
                if (b_flags) {
                  descending_close();
                  _bbox_i.y1 = bl_max(_bbox_i.y1, fy0);
                  goto BeforeClipEndPoint;
                }

                fx1 = Math::trunc_to_int(b.x);
                fy1 = Math::trunc_to_int(b.y);

                if (fy1 < fy0) {
                  descending_close();
                  BL_PROPAGATE(ascending_open());
                  ascending_add_unsafe(fx0, fy0);
                  ascending_add_unsafe(fx1, fy1);
                  _bbox_i.y1 = bl_max(_bbox_i.y1, fy0);
                  goto AscendingLineLoopA;
                }
                BL_PROPAGATE(descending_add_checked(fx1, fy1));
              }
              // [NOT REACHED HERE]
            }
            else if (fy0 > fy1) {
AscendingLineBegin:
              BL_PROPAGATE(ascending_open());
              ascending_add_unsafe(fx0, fy0);
              ascending_add_unsafe(fx1, fy1);
              _bbox_i.y1 = bl_max(_bbox_i.y1, fy0);

              // Instead of processing one vertex and swapping a/b each time
              // we process two vertices and swap only upon loop termination.
              for (;;) {
AscendingLineLoopA:
                if (!source.maybe_next_line_to(a)) {
                  ascending_close();
                  _bbox_i.y0 = bl_min(_bbox_i.y0, fy1);
                  a = b;
                  return BL_SUCCESS;
                }

                b_flags = bl_clip_calc_xy_flags(a, _clip_box_d);
                if (b_flags) {
                  ascending_close();
                  Internal::swap(a, b);
                  goto BeforeClipEndPoint;
                }

                fx0 = Math::trunc_to_int(a.x);
                fy0 = Math::trunc_to_int(a.y);

                if (fy0 > fy1) {
                  ascending_close();
                  BL_PROPAGATE(descending_open());
                  descending_add_unsafe(fx1, fy1);
                  descending_add_unsafe(fx0, fy0);
                  _bbox_i.y0 = bl_min(_bbox_i.y0, fy1);
                  goto DescendingLineLoopB;
                }
                BL_PROPAGATE(ascending_add_checked(fx0, fy0));

AscendingLineLoopB:
                if (!source.maybe_next_line_to(b)) {
                  ascending_close();
                  _bbox_i.y0 = bl_min(_bbox_i.y0, fy0);
                  return BL_SUCCESS;
                }

                b_flags = bl_clip_calc_xy_flags(b, _clip_box_d);
                if (b_flags) {
                  ascending_close();
                  _bbox_i.y0 = bl_min(_bbox_i.y0, fy0);
                  goto BeforeClipEndPoint;
                }

                fx1 = Math::trunc_to_int(b.x);
                fy1 = Math::trunc_to_int(b.y);

                if (fy1 > fy0) {
                  ascending_close();
                  BL_PROPAGATE(descending_open());
                  descending_add_unsafe(fx0, fy0);
                  descending_add_unsafe(fx1, fy1);
                  _bbox_i.y0 = bl_min(_bbox_i.y0, fy0);
                  goto DescendingLineLoopA;
                }
                BL_PROPAGATE(ascending_add_checked(fx1, fy1));
              }
            }
            else {
              a = b;
              if (!source.maybe_next_line_to(b)) {
                return BL_SUCCESS;
              }

              b_flags = bl_clip_calc_xy_flags(b, _clip_box_d);
              if (b_flags) {
                break;
              }

              fx0 = fx1;
              fy0 = fy1;
              fx1 = Math::trunc_to_int(b.x);
              fy1 = Math::trunc_to_int(b.y);
            }
          }
        }

BeforeClipEndPoint:
        p = a;
        d = b - a;
      }
      else {
        // Line - Partially or Completely Clipped
        // --------------------------------------

        double bor_y0;
        double bor_y1;

RestartClipLoop:
        if (a_flags & kClipFlagY0) {
          // Quickly skip all lines that are out of ClipBox or at its border.
          for (;;) {
            if (_clip_box_d.y0 < b.y) break;                    // xxxxxxxxxxxxxxxxxxx
            a = b;                                              // .                 .
            if (!source.maybe_next_line_to(b)) {                // .                 .
              a_flags = bl_clip_calc_x_flags(a, _clip_box_d) |  // .                 .
                        bl_clip_calc_y0_flags(a, _clip_box_d);  // .                 .
              return BL_SUCCESS;                                // .                 .
            }                                                   // ...................
          }

          // Calculate flags we haven't updated inside the loop.
          a_flags = bl_clip_calc_x_flags(a, _clip_box_d) | bl_clip_calc_y0_flags(a, _clip_box_d);
          b_flags = bl_clip_calc_x_flags(b, _clip_box_d) | bl_clip_calc_y1_flags(b, _clip_box_d);

          bor_y0 = _clip_box_d.y0;
          uint32_t common_flags = a_flags & b_flags;

          if (common_flags) {
            bor_y1 = bl_min(_clip_box_d.y1, b.y);
            if (common_flags & kClipFlagX0) {
              BL_PROPAGATE(accumulate_left_border(bor_y0, bor_y1));
            }
            else {
              BL_PROPAGATE(accumulate_right_border(bor_y0, bor_y1));
            }

            a = b;
            a_flags = b_flags;
            continue;
          }
        }
        else if (a_flags & kClipFlagY1) {
          // Quickly skip all lines that are out of ClipBox or at its border.
          for (;;) {
            if (_clip_box_d.y1 > b.y) break;                    // ...................
            a = b;                                              // .                 .
            if (!source.maybe_next_line_to(b)) {                // .                 .
              a_flags = bl_clip_calc_x_flags(a, _clip_box_d) |  // .                 .
                        bl_clip_calc_y1_flags(a, _clip_box_d);  // .                 .
              return BL_SUCCESS;                                // .                 .
            }                                                   // xxxxxxxxxxxxxxxxxxx
          }

          // Calculate flags we haven't updated inside the loop.
          a_flags = bl_clip_calc_x_flags(a, _clip_box_d) | bl_clip_calc_y1_flags(a, _clip_box_d);
          b_flags = bl_clip_calc_x_flags(b, _clip_box_d) | bl_clip_calc_y0_flags(b, _clip_box_d);

          bor_y0 = _clip_box_d.y1;
          uint32_t common_flags = a_flags & b_flags;

          if (common_flags) {
            bor_y1 = bl_max(_clip_box_d.y0, b.y);
            if (common_flags & kClipFlagX0)
              BL_PROPAGATE(accumulate_left_border(bor_y0, bor_y1));
            else
              BL_PROPAGATE(accumulate_right_border(bor_y0, bor_y1));

            a = b;
            a_flags = b_flags;
            continue;
          }
        }
        else if (a_flags & kClipFlagX0) {
          bor_y0 = bl_clamp(a.y, _clip_box_d.y0, _clip_box_d.y1);

          // Quickly skip all lines that are out of ClipBox or at its border.
          for (;;) {                                            // x..................
            if (_clip_box_d.x0 < b.x) break;                    // x                 .
            a = b;                                              // x                 .
            if (!source.maybe_next_line_to(b)) {                // x                 .
              a_flags = bl_clip_calc_y_flags(a, _clip_box_d) |  // x                 .
                        bl_clip_calc_x0_flags(a, _clip_box_d);  // x..................
              bor_y1 = bl_clamp(a.y, _clip_box_d.y0, _clip_box_d.y1);
              if (bor_y0 != bor_y1)
                BL_PROPAGATE(accumulate_left_border(bor_y0, bor_y1));
              return BL_SUCCESS;
            }
          }

          bor_y1 = bl_clamp(a.y, _clip_box_d.y0, _clip_box_d.y1);
          if (bor_y0 != bor_y1)
            BL_PROPAGATE(accumulate_left_border(bor_y0, bor_y1));

          // Calculate flags we haven't updated inside the loop.
          a_flags = bl_clip_calc_x0_flags(a, _clip_box_d) | bl_clip_calc_y_flags(a, _clip_box_d);
          b_flags = bl_clip_calc_x1_flags(b, _clip_box_d) | bl_clip_calc_y_flags(b, _clip_box_d);

          if (a_flags & b_flags)
            goto RestartClipLoop;

          bor_y0 = bor_y1;
        }
        else {
          bor_y0 = bl_clamp(a.y, _clip_box_d.y0, _clip_box_d.y1);

          // Quickly skip all lines that are out of ClipBox or at its border.
          for (;;) {                                            // ..................x
            if (_clip_box_d.x1 > b.x) break;                    // .                 x
            a = b;                                              // .                 x
            if (!source.maybe_next_line_to(b)) {                // .                 x
              a_flags = bl_clip_calc_y_flags(a, _clip_box_d) |  // .                 x
                        bl_clip_calc_x1_flags(a, _clip_box_d);  // ..................x
              bor_y1 = bl_clamp(a.y, _clip_box_d.y0, _clip_box_d.y1);
              if (bor_y0 != bor_y1)
                BL_PROPAGATE(accumulate_right_border(bor_y0, bor_y1));
              return BL_SUCCESS;
            }
          }

          bor_y1 = bl_clamp(a.y, _clip_box_d.y0, _clip_box_d.y1);
          if (bor_y0 != bor_y1)
            BL_PROPAGATE(accumulate_right_border(bor_y0, bor_y1));

          // Calculate flags we haven't updated inside the loop.
          a_flags = bl_clip_calc_x1_flags(a, _clip_box_d) | bl_clip_calc_y_flags(a, _clip_box_d);
          b_flags = bl_clip_calc_x0_flags(b, _clip_box_d) | bl_clip_calc_y_flags(b, _clip_box_d);

          if (a_flags & b_flags)
            goto RestartClipLoop;

          bor_y0 = bor_y1;
        }

        // Line - Clip Start Point
        // -----------------------

        // The start point of the line requires clipping.
        d = b - a;
        p.x = _clip_box_d.x1;
        p.y = _clip_box_d.y1;

        switch (a_flags) {
          case kClipFlagNone:
            p = a;
            break;

          case kClipFlagX0Y0:
            p.x = _clip_box_d.x0;
            [[fallthrough]];

          case kClipFlagX1Y0:
            p.y = a.y + (p.x - a.x) * d.y / d.x;
            a_flags = bl_clip_calc_y_flags(p, _clip_box_d);

            if (p.y >= _clip_box_d.y0)
              break;
            [[fallthrough]];

          case kClipFlagY0:
            p.y = _clip_box_d.y0;
            p.x = a.x + (p.y - a.y) * d.x / d.y;

            a_flags = bl_clip_calc_x_flags(p, _clip_box_d);
            break;

          case kClipFlagX0Y1:
            p.x = _clip_box_d.x0;
            [[fallthrough]];

          case kClipFlagX1Y1:
            p.y = a.y + (p.x - a.x) * d.y / d.x;
            a_flags = bl_clip_calc_y_flags(p, _clip_box_d);

            if (p.y <= _clip_box_d.y1)
              break;
            [[fallthrough]];

          case kClipFlagY1:
            p.y = _clip_box_d.y1;
            p.x = a.x + (p.y - a.y) * d.x / d.y;

            a_flags = bl_clip_calc_x_flags(p, _clip_box_d);
            break;

          case kClipFlagX0:
            p.x = _clip_box_d.x0;
            [[fallthrough]];

          case kClipFlagX1:
            p.y = a.y + (p.x - a.x) * d.y / d.x;

            a_flags = bl_clip_calc_y_flags(p, _clip_box_d);
            break;

          default:
            // Possibly caused by NaNs.
            return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
        }

        if (a_flags) {
          bor_y1 = bl_clamp(b.y, _clip_box_d.y0, _clip_box_d.y1);
          if (p.x <= _clip_box_d.x0)
            BL_PROPAGATE(accumulate_left_border(bor_y0, bor_y1));
          else if (p.x >= _clip_box_d.x1)
            BL_PROPAGATE(accumulate_right_border(bor_y0, bor_y1));

          a = b;
          a_flags = b_flags;
          continue;
        }

        bor_y1 = bl_clamp(p.y, _clip_box_d.y0, _clip_box_d.y1);
        if (bor_y0 != bor_y1) {
          if (p.x <= _clip_box_d.x0)
            BL_PROPAGATE(accumulate_left_border(bor_y0, bor_y1));
          else
            BL_PROPAGATE(accumulate_right_border(bor_y0, bor_y1));
        }

        if (!b_flags) {
          a = b;
          a_flags = 0;

          fx0 = Math::trunc_to_int(p.x);
          fy0 = Math::trunc_to_int(p.y);
          fx1 = Math::trunc_to_int(b.x);
          fy1 = Math::trunc_to_int(b.y);

          if (fy0 == fy1)
            continue;

          if (fy0 < fy1)
            goto DescendingLineBegin;
          else
            goto AscendingLineBegin;
        }
      }

      {
        // Line - Clip End Point
        // ---------------------

        BLPoint q(_clip_box_d.x1, _clip_box_d.y1);

        BL_ASSERT(b_flags != 0);
        switch (b_flags) {
          case kClipFlagX0Y0:
            q.x = _clip_box_d.x0;
            [[fallthrough]];

          case kClipFlagX1Y0:
            q.y = a.y + (q.x - a.x) * d.y / d.x;
            if (q.y >= _clip_box_d.y0)
              break;
            [[fallthrough]];

          case kClipFlagY0:
            q.y = _clip_box_d.y0;
            q.x = a.x + (q.y - a.y) * d.x / d.y;
            break;

          case kClipFlagX0Y1:
            q.x = _clip_box_d.x0;
            [[fallthrough]];

          case kClipFlagX1Y1:
            q.y = a.y + (q.x - a.x) * d.y / d.x;
            if (q.y <= _clip_box_d.y1)
              break;
            [[fallthrough]];

          case kClipFlagY1:
            q.y = _clip_box_d.y1;
            q.x = a.x + (q.y - a.y) * d.x / d.y;
            break;

          case kClipFlagX0:
            q.x = _clip_box_d.x0;
            [[fallthrough]];

          case kClipFlagX1:
            q.y = a.y + (q.x - a.x) * d.y / d.x;
            break;

          default:
            // Possibly caused by NaNs.
            return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
        }

        BL_PROPAGATE(add_line_segment(p.x, p.y, q.x, q.y));
        double clipped_by = bl_clamp(b.y, _clip_box_d.y0, _clip_box_d.y1);

        if (q.y != clipped_by) {
          if (q.x == _clip_box_d.x0)
            BL_PROPAGATE(accumulate_left_border(q.y, clipped_by));
          else
            BL_PROPAGATE(accumulate_right_border(q.y, clipped_by));
        }
      }

      a = b;
      a_flags = b_flags;
    } while (source.maybe_next_line_to(b));

    return BL_SUCCESS;
  }

  //! \}

  //! \name Low-Level API - Quad To
  //! \{

  // Terminology:
  //
  //   'p0' - Quad start point.
  //   'p1' - Quad control point.
  //   'p2' - Quad end point.

  template<class Source>
  BL_INLINE_IF_NOT_DEBUG BLResult quad_to(Source& source, State& state) noexcept {
    // 2 extrema and 1 terminating `1.0` value.
    constexpr uint32_t kMaxTCount = 2 + 1;

    BLPoint spline[kMaxTCount * 2 + 1];
    BLPoint& p0 = state.a;
    BLPoint& p1 = spline[1];
    BLPoint& p2 = spline[2];

    uint32_t& p0_flags = state.a_flags;
    source.next_quad_to(p1, p2);

    for (;;) {
      uint32_t p1_flags = bl_clip_calc_xy_flags(p1, _clip_box_d);
      uint32_t p2_flags = bl_clip_calc_xy_flags(p2, _clip_box_d);
      uint32_t common_flags = p0_flags & p1_flags & p2_flags;

      // Fast reject.
      if (common_flags) {
        uint32_t end = 0;

        if (common_flags & kClipFlagY0) {
          // CLIPPED OUT: Above top (fast).
          for (;;) {
            p0 = p2;
            end = !source.has_command() || !source.is_quad_to();
            if (end) {
              break;
            }

            source.next_quad_to(p1, p2);
            if (!((p1.y <= _clip_box_d.y0) & (p2.y <= _clip_box_d.y0))) {
              break;
            }
          }
        }
        else if (common_flags & kClipFlagY1) {
          // CLIPPED OUT: Below bottom (fast).
          for (;;) {
            p0 = p2;
            end = !source.has_command() || !source.is_quad_to();
            if (end) {
              break;
            }

            source.next_quad_to(p1, p2);
            if (!((p1.y >= _clip_box_d.y1) & (p2.y >= _clip_box_d.y1))) {
              break;
            }
          }
        }
        else {
          // CLIPPED OUT: Before left or after right (border-line required).
          double y0 = bl_clamp(p0.y, _clip_box_d.y0, _clip_box_d.y1);

          if (common_flags & kClipFlagX0) {
            for (;;) {
              p0 = p2;
              end = !source.has_command() || !source.is_quad_to();
              if (end) {
                break;
              }

              source.next_quad_to(p1, p2);
              if (!((p1.x <= _clip_box_d.x0) & (p2.x <= _clip_box_d.x0))) {
                break;
              }
            }

            double y1 = bl_clamp(p0.y, _clip_box_d.y0, _clip_box_d.y1);
            BL_PROPAGATE(accumulate_left_border(y0, y1));
          }
          else {
            for (;;) {
              p0 = p2;
              end = !source.has_command() || !source.is_quad_to();
              if (end) {
                break;
              }

              source.next_quad_to(p1, p2);
              if (!((p1.x >= _clip_box_d.x1) & (p2.x >= _clip_box_d.x1))) {
                break;
              }
            }

            double y1 = bl_clamp(p0.y, _clip_box_d.y0, _clip_box_d.y1);
            BL_PROPAGATE(accumulate_right_border(y0, y1));
          }
        }

        p0_flags = bl_clip_calc_xy_flags(p0, _clip_box_d);
        if (end) {
          return BL_SUCCESS;
        }
        continue;
      }

      spline[0] = p0;

      BLPoint* spline_ptr = spline;
      BLPoint* spline_end = Geometry::quad_split_with_options<Geometry::QuadSplitOptions::kExtremaXY>(Geometry::quad_ref(spline), spline_ptr);

      if (spline_end == spline_ptr)
        spline_end = spline_ptr + 2;

      Appender appender(*this);
      FlattenMonoQuad mono_curve(state.flatten_data, _flatten_tolerance_sq);

      uint32_t any_flags = p0_flags | p1_flags | p2_flags;
      if (any_flags) {
        // One or more quad may need clipping.
        do {
          uint32_t sign_bit = spline_ptr[0].y > spline_ptr[2].y;
          BL_PROPAGATE(
            flatten_unsafe_mono_curve<FlattenMonoQuad>(mono_curve, appender, spline_ptr, sign_bit)
          );
        } while ((spline_ptr += 2) != spline_end);

        p0 = spline_end[0];
        p0_flags = p2_flags;
      }
      else {
        // No clipping - optimized fast-path.
        do {
          uint32_t sign_bit = spline_ptr[0].y > spline_ptr[2].y;
          BL_PROPAGATE(
            flatten_safe_mono_curve<FlattenMonoQuad>(mono_curve, appender, spline_ptr, sign_bit)
          );
        } while ((spline_ptr += 2) != spline_end);

        p0 = spline_end[0];
      }

      if (!source.maybe_next_quad_to(p1, p2)) {
        return BL_SUCCESS;
      }
    }
  }

  //! \}

  //! \name Low-Level API - Cubic To
  //! \{

  // Terminology:
  //
  //   'p0' - Cubic start point.
  //   'p1' - Cubic first control point.
  //   'p2' - Cubic second control point.
  //   'p3' - Cubic end point.

  template<class Source>
  BL_INLINE_IF_NOT_DEBUG BLResult cubic_to(Source& source, State& state) noexcept {
    // 4 extrema, 2 inflections, 1 cusp, and 1 terminating `1.0` value.
    constexpr uint32_t kMaxTCount = 4 + 2 + 1 + 1;

    BLPoint spline[kMaxTCount * 3 + 1];
    BLPoint& p0 = state.a;
    BLPoint& p1 = spline[1];
    BLPoint& p2 = spline[2];
    BLPoint& p3 = spline[3];

    uint32_t& p0_flags = state.a_flags;
    source.next_cubic_to(p1, p2, p3);

    for (;;) {
      uint32_t p1_flags = bl_clip_calc_xy_flags(p1, _clip_box_d);
      uint32_t p2_flags = bl_clip_calc_xy_flags(p2, _clip_box_d);
      uint32_t p3_flags = bl_clip_calc_xy_flags(p3, _clip_box_d);
      uint32_t common_flags = p0_flags & p1_flags & p2_flags & p3_flags;

      // Fast reject.
      if (common_flags) {
        uint32_t end = 0;

        if (common_flags & kClipFlagY0) {
          // CLIPPED OUT: Above top (fast).
          for (;;) {
            p0 = p3;
            end = !source.has_command() || !source.is_cubic_to();
            if (end) {
              break;
            }

            source.next_cubic_to(p1, p2, p3);
            if (!((p1.y <= _clip_box_d.y0) & (p2.y <= _clip_box_d.y0) & (p3.y <= _clip_box_d.y0)))
              break;
          }
        }
        else if (common_flags & kClipFlagY1) {
          // CLIPPED OUT: Below bottom (fast).
          for (;;) {
            p0 = p3;
            end = !source.has_command() || !source.is_cubic_to();
            if (end) {
              break;
            }

            source.next_cubic_to(p1, p2, p3);
            if (!((p1.y >= _clip_box_d.y1) & (p2.y >= _clip_box_d.y1) & (p3.y >= _clip_box_d.y1))) {
              break;
            }
          }
        }
        else {
          // CLIPPED OUT: Before left or after right (border-line required).
          double y0 = bl_clamp(p0.y, _clip_box_d.y0, _clip_box_d.y1);

          if (common_flags & kClipFlagX0) {
            for (;;) {
              p0 = p3;
              end = !source.has_command() || !source.is_cubic_to();
              if (end) {
                break;
              }

              source.next_cubic_to(p1, p2, p3);
              if (!((p1.x <= _clip_box_d.x0) & (p2.x <= _clip_box_d.x0) & (p3.x <= _clip_box_d.x0))) {
                break;
              }
            }

            double y1 = bl_clamp(p0.y, _clip_box_d.y0, _clip_box_d.y1);
            BL_PROPAGATE(accumulate_left_border(y0, y1));
          }
          else {
            for (;;) {
              p0 = p3;
              end = !source.has_command() || !source.is_cubic_to();
              if (end) {
                break;
              }

              source.next_cubic_to(p1, p2, p3);
              if (!((p1.x >= _clip_box_d.x1) & (p2.x >= _clip_box_d.x1) & (p3.x >= _clip_box_d.x1))) {
                break;
              }
            }

            double y1 = bl_clamp(p0.y, _clip_box_d.y0, _clip_box_d.y1);
            BL_PROPAGATE(accumulate_right_border(y0, y1));
          }
        }

        p0_flags = bl_clip_calc_xy_flags(p0, _clip_box_d);
        if (end) {
          return BL_SUCCESS;
        }
        continue;
      }

      spline[0] = p0;

      BLPoint* spline_ptr = spline;
      BLPoint* spline_end = Geometry::cubic_split_with_options<Geometry::CubicSplitOptions::kExtremaXYInflectionsCusp>(Geometry::cubic_ref(spline), spline_ptr);

      if (spline_end == spline_ptr) {
        spline_end += 3;
      }

      Appender appender(*this);
      FlattenMonoCubic mono_curve(state.flatten_data, _flatten_tolerance_sq);

      uint32_t any_flags = p0_flags | p1_flags | p2_flags | p3_flags;
      if (any_flags) {
        // One or more cubic may need clipping.
        do {
          uint32_t sign_bit = spline_ptr[0].y > spline_ptr[3].y;
          BL_PROPAGATE(
            flatten_unsafe_mono_curve<FlattenMonoCubic>(mono_curve, appender, spline_ptr, sign_bit)
          );
        } while ((spline_ptr += 3) != spline_end);

        p0 = spline_end[0];
        p0_flags = p3_flags;
      }
      else {
        // No clipping - optimized fast-path.
        do {
          uint32_t sign_bit = spline_ptr[0].y > spline_ptr[3].y;
          BL_PROPAGATE(
            flatten_safe_mono_curve<FlattenMonoCubic>(mono_curve, appender, spline_ptr, sign_bit)
          );
        } while ((spline_ptr += 3) != spline_end);

        p0 = spline_end[0];
      }

      if (!source.maybe_next_cubic_to(p1, p2, p3)) {
        return BL_SUCCESS;
      }
    }
  }

  //! \}

  //! \name Low-Level API - Conic To
  //! \{

  // Terminology:
  //
  //   'p0' - Conic start point.
  //   'p1' - Conic control point.
  //   'p2' - Conic end point.

  template<class Source>
  BL_INLINE_IF_NOT_DEBUG BLResult conic_to(Source& source, State& state) noexcept {
    // 2 extrema and 1 terminating `1.0` value.
    constexpr uint32_t kMaxTCount = 2 + 1;

    BLPoint spline[kMaxTCount * 2 + 1];
    // Per-segment weights, one for each conic produced by splitting the input conic at its extrema.
    double spline_w[kMaxTCount];
    BLPoint& p0 = state.a;
    BLPoint& p1 = spline[1];
    BLPoint& p2 = spline[2];
    double w;

    uint32_t& p0_flags = state.a_flags;
    source.next_conic_to(p1, p2, w);

    for (;;) {
      uint32_t p1_flags = bl_clip_calc_xy_flags(p1, _clip_box_d);
      uint32_t p2_flags = bl_clip_calc_xy_flags(p2, _clip_box_d);
      uint32_t common_flags = p0_flags & p1_flags & p2_flags;

      // Fast reject.
      if (common_flags) {
        uint32_t end = 0;

        if (common_flags & kClipFlagY0) {
          // CLIPPED OUT: Above top (fast).
          for (;;) {
            p0 = p2;
            end = !source.has_command() || !source.is_conic_to();
            if (end) {
              break;
            }

            source.next_conic_to(p1, p2, w);
            if (!((p1.y <= _clip_box_d.y0) & (p2.y <= _clip_box_d.y0))) {
              break;
            }
          }
        }
        else if (common_flags & kClipFlagY1) {
          // CLIPPED OUT: Below bottom (fast).
          for (;;) {
            p0 = p2;
            end = !source.has_command() || !source.is_conic_to();
            if (end) {
              break;
            }

            source.next_conic_to(p1, p2, w);
            if (!((p1.y >= _clip_box_d.y1) & (p2.y >= _clip_box_d.y1))) {
              break;
            }
          }
        }
        else {
          // CLIPPED OUT: Before left or after right (border-line required).
          double y0 = bl_clamp(p0.y, _clip_box_d.y0, _clip_box_d.y1);

          if (common_flags & kClipFlagX0) {
            for (;;) {
              p0 = p2;
              end = !source.has_command() || !source.is_conic_to();
              if (end) {
                break;
              }

              source.next_conic_to(p1, p2, w);
              if (!((p1.x <= _clip_box_d.x0) & (p2.x <= _clip_box_d.x0))) {
                break;
              }
            }

            double y1 = bl_clamp(p0.y, _clip_box_d.y0, _clip_box_d.y1);
            BL_PROPAGATE(accumulate_left_border(y0, y1));
          }
          else {
            for (;;) {
              p0 = p2;
              end = !source.has_command() || !source.is_conic_to();
              if (end) {
                break;
              }

              source.next_conic_to(p1, p2, w);
              if (!((p1.x >= _clip_box_d.x1) & (p2.x >= _clip_box_d.x1))) {
                break;
              }
            }

            double y1 = bl_clamp(p0.y, _clip_box_d.y0, _clip_box_d.y1);
            BL_PROPAGATE(accumulate_right_border(y0, y1));
          }
        }

        p0_flags = bl_clip_calc_xy_flags(p0, _clip_box_d);
        if (end) {
          return BL_SUCCESS;
        }
        continue;
      }

      spline[0] = p0;
      // Weight used by the fast-path (no extrema - a single segment that keeps the original weight).
      spline_w[0] = w;

      BLPoint* spline_ptr = spline;
      double* spline_w_ptr = spline_w;
      BLPoint* spline_end = Geometry::split_conic_to_spline<Geometry::ConicSplitOptions::kExtremaXY>(spline, w, spline_ptr, spline_w_ptr);

      if (spline_end == spline_ptr) {
        spline_end = spline_ptr + 2;
      }

      Appender appender(*this);
      FlattenMonoConic mono_curve(state.flatten_data, _flatten_tolerance_sq);

      uint32_t any_flags = p0_flags | p1_flags | p2_flags;
      if (any_flags) {
        // One or more conic may need clipping.
        do {
          uint32_t sign_bit = spline_ptr[0].y > spline_ptr[2].y;
          mono_curve.set_weight(*spline_w_ptr++);
          BL_PROPAGATE(
            flatten_unsafe_mono_curve<FlattenMonoConic>(mono_curve, appender, spline_ptr, sign_bit)
          );
        } while ((spline_ptr += 2) != spline_end);

        p0 = spline_end[0];
        p0_flags = p2_flags;
      }
      else {
        // No clipping - optimized fast-path.
        do {
          uint32_t sign_bit = spline_ptr[0].y > spline_ptr[2].y;
          mono_curve.set_weight(*spline_w_ptr++);
          BL_PROPAGATE(
            flatten_safe_mono_curve<FlattenMonoConic>(mono_curve, appender, spline_ptr, sign_bit)
          );
        } while ((spline_ptr += 2) != spline_end);

        p0 = spline_end[0];
      }

      if (!source.maybe_next_conic_to(p1, p2, w)) {
        return BL_SUCCESS;
      }
    }
  }

  //! \}

  //! \name Curve Helpers
  //! \{

  template<typename MonoCurveT>
  BL_INLINE BLResult flatten_safe_mono_curve(MonoCurveT& mono_curve, Appender& appender, const BLPoint* src, uint32_t sign_bit) noexcept {
    mono_curve.begin(src, sign_bit);
    appender.set_sign_bit(sign_bit);

    if (mono_curve.is_left_to_right())
      mono_curve.bound_left_to_right();
    else
      mono_curve.bound_right_to_left();

    BL_PROPAGATE(appender.open_at(mono_curve.first().x, mono_curve.first().y));
    for (;;) {
      typename MonoCurveT::SplitStep step;
      if (!mono_curve.is_flat(step)) {
        if (mono_curve.can_push()) {
          mono_curve.split(step);
          mono_curve.push(step);
          continue;
        }
        else {
          // The curve is either invalid or the tolerance is too strict. We shouldn't get INF nor NaNs here as
          //  we know we are within the clip_box.
          BL_ASSERT(step.is_finite());
        }
      }

      BL_PROPAGATE(appender.add_line(mono_curve.last().x, mono_curve.last().y));
      if (!mono_curve.can_pop()) {
        break;
      }
      mono_curve.pop();
    }

    appender.close();
    return BL_SUCCESS;
  }

  // Clips and flattens a monotonic curve - works for both quadratics and cubics.
  //
  // The idea behind this function is to quickly subdivide to find the intersection with ClipBox. When the
  // intersection is found the intersecting line is clipped and the subdivision continues until the end of
  // the curve or until another intersection is found, which would be the end of the curve. The algorithm
  // handles all cases and accumulates border lines when necessary.
  template<typename MonoCurveT>
  BL_INLINE BLResult flatten_unsafe_mono_curve(MonoCurveT& mono_curve, Appender& appender, const BLPoint* src, uint32_t sign_bit) noexcept {
    mono_curve.begin(src, sign_bit);
    appender.set_sign_bit(sign_bit);

    double y_start = mono_curve.first().y;
    double y_end = bl_min(mono_curve.last().y, _clip_box_d.y1);

    if ((y_start >= y_end) | (y_end <= _clip_box_d.y0)) {
      return BL_SUCCESS;
    }

    // The delta must be enough to represent our fixed point.
    double kDeltaLimit = 0.00390625;
    double x_delta = bl_abs(mono_curve.first().x - mono_curve.last().x);

    uint32_t completely_out = 0;
    typename MonoCurveT::SplitStep step;

    if (x_delta <= kDeltaLimit) {
      // Straight-Line
      // -------------

      y_start = bl_max(y_start, _clip_box_d.y0);

      double x_min = bl_min(mono_curve.first().x, mono_curve.last().x);
      double x_max = bl_max(mono_curve.first().x, mono_curve.last().x);

      if (x_max <= _clip_box_d.x0) {
        BL_PROPAGATE(accumulate_left_border(y_start, y_end, sign_bit));
      }
      else if (x_min >= _clip_box_d.x1) {
        BL_PROPAGATE(accumulate_right_border(y_start, y_end, sign_bit));
      }
      else {
        BL_PROPAGATE(appender.open_at(mono_curve.first().x, y_start));
        BL_PROPAGATE(appender.add_line(mono_curve.last().x, y_end));
        BL_PROPAGATE(appender.close());
      }

      return BL_SUCCESS;
    }
    else if (mono_curve.is_left_to_right()) {
      // Left-To-Right
      // ------------>
      //
      //  ...__
      //       --._
      //           *_
      mono_curve.bound_left_to_right();

      if (y_start < _clip_box_d.y0) {
        y_start = _clip_box_d.y0;
        for (;;) {
          // CLIPPED OUT: Above ClipBox.y0
          // -----------------------------

          completely_out = (mono_curve.first().x >= _clip_box_d.x1);
          if (completely_out) {
            break;
          }

          if (!mono_curve.is_flat(step)) {
            mono_curve.split(step);

            if (step.mid_point().y <= _clip_box_d.y0) {
              mono_curve.discard_and_advance(step);
              continue;
            }

            if (mono_curve.can_push()) {
              mono_curve.push(step);
              continue;
            }
          }

          if (mono_curve.last().y > _clip_box_d.y0) {
            // The `completely_out` value will only be used if there is no
            // curve to be popped from the stack. In that case it's important
            // to be `1` as we have to accumulate the border.
            completely_out = mono_curve.last().x < _clip_box_d.x0;
            if (completely_out) {
              goto LeftToRight_BeforeX0_Pop;
            }

            double x_clipped = mono_curve.first().x + (_clip_box_d.y0 - mono_curve.first().y) * dx_div_dy(mono_curve.last() - mono_curve.first());
            if (x_clipped <= _clip_box_d.x0) {
              goto LeftToRight_BeforeX0_Clip;
            }

            completely_out = (x_clipped >= _clip_box_d.x1);
            if (completely_out) {
              break;
            }

            BL_PROPAGATE(appender.open_at(x_clipped, _clip_box_d.y0));
            goto LeftToRight_AddLine;
          }

          if (!mono_curve.can_pop()) {
            break;
          }

          mono_curve.pop();
        }
        completely_out <<= kClipShiftX1;
      }
      else if (y_start < _clip_box_d.y1) {
        if (mono_curve.first().x < _clip_box_d.x0) {
          // CLIPPED OUT: Before ClipBox.x0
          // ------------------------------

          for (;;) {
            completely_out = (mono_curve.first().y >= _clip_box_d.y1);
            if (completely_out) {
              break;
            }

            if (!mono_curve.is_flat(step)) {
              mono_curve.split(step);

              if (step.mid_point().x <= _clip_box_d.x0) {
                mono_curve.discard_and_advance(step);
                continue;
              }

              if (mono_curve.can_push()) {
                mono_curve.push(step);
                continue;
              }
            }

            if (mono_curve.last().x > _clip_box_d.x0) {
LeftToRight_BeforeX0_Clip:
              double y_clipped = mono_curve.first().y + (_clip_box_d.x0 - mono_curve.first().x) * dy_div_dx(mono_curve.last() - mono_curve.first());
              completely_out = (y_clipped >= y_end);

              if (completely_out) {
                break;
              }

              if (y_start < y_clipped) {
                BL_PROPAGATE(accumulate_left_border(y_start, y_clipped, sign_bit));
              }

              BL_PROPAGATE(appender.open_at(_clip_box_d.x0, y_clipped));
              goto LeftToRight_AddLine;
            }

            completely_out = (mono_curve.last().y >= y_end);
            if (completely_out) {
              break;
            }

LeftToRight_BeforeX0_Pop:
            if (!mono_curve.can_pop()) {
              break;
            }

            mono_curve.pop();
          }
          completely_out <<= kClipShiftX0;
        }
        else if (mono_curve.first().x < _clip_box_d.x1) {
          // VISIBLE CASE
          // ------------

          BL_PROPAGATE(appender.open_at(mono_curve.first().x, mono_curve.first().y));
          for (;;) {
            if (!mono_curve.is_flat(step)) {
              mono_curve.split(step);

              if (mono_curve.can_push()) {
                mono_curve.push(step);
                continue;
              }
            }

LeftToRight_AddLine:
            completely_out = mono_curve.last().x > _clip_box_d.x1;
            if (completely_out) {
              double y_clipped = mono_curve.first().y + (_clip_box_d.x1 - mono_curve.first().x) * dy_div_dx(mono_curve.last() - mono_curve.first());
              if (y_clipped <= y_end) {
                y_start = y_clipped;
                BL_PROPAGATE(appender.add_line(_clip_box_d.x1, y_clipped));
                break;
              }
            }

            completely_out = mono_curve.last().y >= _clip_box_d.y1;
            if (completely_out) {
              double x_clipped = bl_min(mono_curve.first().x + (_clip_box_d.y1 - mono_curve.first().y) * dx_div_dy(mono_curve.last() - mono_curve.first()), _clip_box_d.x1);
              BL_PROPAGATE(appender.add_line(x_clipped, _clip_box_d.y1));

              completely_out = 0;
              break;
            }

            BL_PROPAGATE(appender.add_line(mono_curve.last().x, mono_curve.last().y));
            if (!mono_curve.can_pop()) {
              break;
            }
            mono_curve.pop();
          }
          appender.close();
          completely_out <<= kClipShiftX1;
        }
        else {
          completely_out = kClipFlagX1;
        }
      }
      else {
        // Below bottom or invalid, ignore this part...
      }
    }
    else {
      // Right-To-Left
      // <------------
      //
      //        __...
      //    _.--
      //  _*
      mono_curve.bound_right_to_left();

      if (y_start < _clip_box_d.y0) {
        y_start = _clip_box_d.y0;
        for (;;) {
          // CLIPPED OUT: Above ClipBox.y0
          // -----------------------------

          completely_out = (mono_curve.first().x <= _clip_box_d.x0);
          if (completely_out) {
            break;
          }

          if (!mono_curve.is_flat(step)) {
            mono_curve.split(step);

            if (step.mid_point().y <= _clip_box_d.y0) {
              mono_curve.discard_and_advance(step);
              continue;
            }

            if (mono_curve.can_push()) {
              mono_curve.push(step);
              continue;
            }
          }

          if (mono_curve.last().y > _clip_box_d.y0) {
            // The `completely_out` value will only be used if there is no
            // curve to be popped from the stack. In that case it's important
            // to be `1` as we have to accumulate the border.
            completely_out = mono_curve.last().x > _clip_box_d.x1;
            if (completely_out) {
              goto RightToLeft_AfterX1_Pop;
            }

            double x_clipped = mono_curve.first().x + (_clip_box_d.y0 - mono_curve.first().y) * dx_div_dy(mono_curve.last() - mono_curve.first());
            if (x_clipped >= _clip_box_d.x1) {
              goto RightToLeft_AfterX1_Clip;
            }

            completely_out = (x_clipped <= _clip_box_d.x0);
            if (completely_out) {
              break;
            }

            BL_PROPAGATE(appender.open_at(x_clipped, _clip_box_d.y0));
            goto RightToLeft_AddLine;
          }

          if (!mono_curve.can_pop()) {
            break;
          }

          mono_curve.pop();
        }
        completely_out <<= kClipShiftX0;
      }
      else if (y_start < _clip_box_d.y1) {
        if (mono_curve.first().x > _clip_box_d.x1) {
          // CLIPPED OUT: After ClipBox.x1
          // ------------------------------

          for (;;) {
            completely_out = (mono_curve.first().y >= _clip_box_d.y1);
            if (completely_out) {
              break;
            }

            if (!mono_curve.is_flat(step)) {
              mono_curve.split(step);

              if (step.mid_point().x >= _clip_box_d.x1) {
                mono_curve.discard_and_advance(step);
                continue;
              }

              if (mono_curve.can_push()) {
                mono_curve.push(step);
                continue;
              }
            }

            if (mono_curve.last().x < _clip_box_d.x1) {
RightToLeft_AfterX1_Clip:
              double y_clipped = mono_curve.first().y + (_clip_box_d.x1 - mono_curve.first().x) * dy_div_dx(mono_curve.last() - mono_curve.first());
              completely_out = (y_clipped >= y_end);

              if (completely_out) {
                break;
              }

              if (y_start < y_clipped) {
                BL_PROPAGATE(accumulate_right_border(y_start, y_clipped, sign_bit));
              }

              BL_PROPAGATE(appender.open_at(_clip_box_d.x1, y_clipped));
              goto RightToLeft_AddLine;
            }

            completely_out = (mono_curve.last().y >= y_end);
            if (completely_out) {
              break;
            }

RightToLeft_AfterX1_Pop:
            if (!mono_curve.can_pop()) {
              break;
            }

            mono_curve.pop();
          }
          completely_out <<= kClipShiftX1;
        }
        else if (mono_curve.first().x > _clip_box_d.x0) {
          // VISIBLE CASE
          // ------------

          BL_PROPAGATE(appender.open_at(mono_curve.first().x, mono_curve.first().y));
          for (;;) {
            if (!mono_curve.is_flat(step)) {
              mono_curve.split(step);

              if (mono_curve.can_push()) {
                mono_curve.push(step);
                continue;
              }
            }

RightToLeft_AddLine:
            completely_out = mono_curve.last().x < _clip_box_d.x0;
            if (completely_out) {
              double y_clipped = Math::madd(_clip_box_d.x0 - mono_curve.first().x,
                                            dy_div_dx(mono_curve.last() - mono_curve.first()),
                                            mono_curve.first().y);
              if (y_clipped <= y_end) {
                y_start = y_clipped;
                BL_PROPAGATE(appender.add_line(_clip_box_d.x0, y_clipped));
                break;
              }
            }

            completely_out = mono_curve.last().y >= _clip_box_d.y1;
            if (completely_out) {
              double x_clipped = bl_max(
                Math::madd(_clip_box_d.y1 - mono_curve.first().y,
                           dx_div_dy(mono_curve.last() - mono_curve.first()),
                           mono_curve.first().x),
                _clip_box_d.x0
              );
              BL_PROPAGATE(appender.add_line(x_clipped, _clip_box_d.y1));

              completely_out = 0;
              break;
            }

            BL_PROPAGATE(appender.add_line(mono_curve.last().x, mono_curve.last().y));
            if (!mono_curve.can_pop()) {
              break;
            }
            mono_curve.pop();
          }
          appender.close();
          completely_out <<= kClipShiftX0;
        }
        else {
          completely_out = kClipFlagX0;
        }
      }
      else {
        // Below bottom or invalid, ignore this part...
      }
    }

    if (completely_out && y_start < y_end) {
      if (completely_out & kClipFlagX0)
        BL_PROPAGATE(accumulate_left_border(y_start, y_end, sign_bit));
      else
        BL_PROPAGATE(accumulate_right_border(y_start, y_end, sign_bit));
    }

    return BL_SUCCESS;
  }

  //! \}

  //! \name Raw Edge Building
  //! \{

  BL_INLINE bool has_space_in_edge_vector() const noexcept { return _ptr != _end; }

  BL_INLINE BLResult ascending_open() noexcept {
    BL_PROPAGATE(_arena->ensure(kMinEdgeSize));
    _ptr = _arena->end<EdgePoint<CoordT>>();
    _end = _arena->ptr<EdgeVector<CoordT>>()->pts;
    return BL_SUCCESS;
  }

  BL_INLINE void ascending_add_unsafe(CoordT x, CoordT y) noexcept {
    BL_ASSERT(has_space_in_edge_vector());
    _ptr--;
    _ptr->reset(x, y);
  }

  BL_INLINE BLResult ascending_add_checked(CoordT x, CoordT y, uint32_t sign_bit = 1) noexcept {
    if (BL_UNLIKELY(!has_space_in_edge_vector())) {
      const EdgePoint<CoordT>* last = ascending_last();
      ascending_close(sign_bit);
      BL_PROPAGATE(ascending_open());
      _ptr--;
      _ptr->reset(last->x, last->y);
    }

    _ptr--;
    _ptr->reset(x, y);
    return BL_SUCCESS;
  }

  BL_INLINE void ascending_close(uint32_t sign_bit = 1u) noexcept {
    BL_ASSUME(sign_bit <= 1u);

    EdgeVector<CoordT>* edge = reinterpret_cast<EdgeVector<CoordT>*>(reinterpret_cast<uint8_t*>(_ptr) - kEdgeOffset);
    edge->count_and_sign = pack_count_and_sign_bit((size_t)(_arena->end<EdgePoint<CoordT>>() - _ptr), sign_bit ^ _master_sign_bit);
    _arena->set_end(edge);
    _link_edge(edge, _ptr[0].y);
  }

  BL_INLINE EdgePoint<CoordT>* ascending_last() const noexcept { return _ptr; }

  BL_INLINE BLResult descending_open() noexcept {
    BL_PROPAGATE(_arena->ensure(kMinEdgeSize));
    _ptr = _arena->ptr<EdgeVector<CoordT>>()->pts;
    _end = _arena->end<EdgePoint<CoordT>>();
    return BL_SUCCESS;
  }

  BL_INLINE void descending_add_unsafe(CoordT x, CoordT y) noexcept {
    BL_ASSERT(has_space_in_edge_vector());
    _ptr->reset(x, y);
    _ptr++;
  }

  BL_INLINE BLResult descending_add_checked(CoordT x, CoordT y, uint32_t sign_bit = 0) noexcept {
    BL_ASSERT(_arena->ptr<EdgeVector<CoordT>>()->pts == _ptr || _ptr[-1].y <= y);

    if (BL_UNLIKELY(!has_space_in_edge_vector())) {
      const EdgePoint<CoordT>* last = descending_last();
      descending_close(sign_bit);
      BL_PROPAGATE(descending_open());
      _ptr->reset(last->x, last->y);
      _ptr++;
    }

    _ptr->reset(x, y);
    _ptr++;
    return BL_SUCCESS;
  }

  BL_INLINE void descending_close(uint32_t sign_bit = 0) noexcept {
    EdgeVector<CoordT>* edge = _arena->ptr<EdgeVector<CoordT>>();
    edge->count_and_sign = pack_count_and_sign_bit((size_t)(_ptr - edge->pts), sign_bit ^ _master_sign_bit);
    _arena->set_ptr(_ptr);
    _link_edge(edge, edge->pts[0].y);
  }

  BL_INLINE void descending_cancel() noexcept {
    // Nothing needed here...
  }

  BL_INLINE EdgePoint<CoordT>* descending_first() const noexcept { return _arena->ptr<EdgeVector<CoordT>>()->pts; };
  BL_INLINE EdgePoint<CoordT>* descending_last() const noexcept { return _ptr - 1; }

  BL_INLINE void _link_edge(EdgeVector<CoordT>* edge, int y0) noexcept {
    size_t band_id = size_t(unsigned(y0) >> _fixed_band_height_shift);
    BL_ASSERT(band_id < _storage->band_count());
    _band_edges[band_id].append(edge);
  }

  //! \}

  //! \name Border Accumulation
  //! \{

  BL_INLINE void reset_border_accumulators() noexcept {
    _border_acc_x0_y0 = _border_acc_x0_y1;
    _border_acc_x1_y0 = _border_acc_x1_y1;
  }

  BL_INLINE BLResult flush_border_accumulators() noexcept {
    return bl_combine_result(_emit_left_border(), _emit_right_border());
  }

  BL_INLINE BLResult accumulate_left_border(double y0, double y1) noexcept {
    if (_border_acc_x0_y1 == y0) {
      _border_acc_x0_y1 = y1;
      return BL_SUCCESS;
    }

    BL_PROPAGATE(_emit_left_border());
    _border_acc_x0_y0 = y0;
    _border_acc_x0_y1 = y1;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult accumulate_left_border(double y0, double y1, uint32_t sign_bit) noexcept {
    if (sign_bit != 0) {
      Internal::swap(y0, y1);
    }
    return accumulate_left_border(y0, y1);
  }

  BL_INLINE BLResult accumulate_right_border(double y0, double y1) noexcept {
    if (_border_acc_x1_y1 == y0) {
      _border_acc_x1_y1 = y1;
      return BL_SUCCESS;
    }

    BL_PROPAGATE(_emit_right_border());
    _border_acc_x1_y0 = y0;
    _border_acc_x1_y1 = y1;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult accumulate_right_border(double y0, double y1, uint32_t sign_bit) noexcept {
    if (sign_bit != 0) {
      Internal::swap(y0, y1);
    }
    return accumulate_right_border(y0, y1);
  }

  BL_INLINE BLResult _emit_left_border() noexcept {
    int acc_y0 = Math::trunc_to_int(_border_acc_x0_y0);
    int acc_y1 = Math::trunc_to_int(_border_acc_x0_y1);

    if (acc_y0 == acc_y1) {
      return BL_SUCCESS;
    }

    int min_y = bl_min(acc_y0, acc_y1);
    int max_y = bl_max(acc_y0, acc_y1);

    _bbox_i.y0 = bl_min(_bbox_i.y0, min_y);
    _bbox_i.y1 = bl_max(_bbox_i.y1, max_y);

    return add_closed_line(_clip_box_i.x0, min_y, _clip_box_i.x0, max_y, uint32_t(acc_y0 > acc_y1) ^ _master_sign_bit);
  }

  BL_INLINE BLResult _emit_right_border() noexcept {
    int acc_y0 = Math::trunc_to_int(_border_acc_x1_y0);
    int acc_y1 = Math::trunc_to_int(_border_acc_x1_y1);

    if (acc_y0 == acc_y1) {
      return BL_SUCCESS;
    }

    int min_y = bl_min(acc_y0, acc_y1);
    int max_y = bl_max(acc_y0, acc_y1);

    _bbox_i.y0 = bl_min(_bbox_i.y0, min_y);
    _bbox_i.y1 = bl_max(_bbox_i.y1, max_y);

    return add_closed_line(_clip_box_i.x1, min_y, _clip_box_i.x1, max_y, uint32_t(acc_y0 > acc_y1) ^ _master_sign_bit);
  }

  //! \}

  // TODO: This should go somewhere else, also the name doesn't make much sense...
  static BL_INLINE double dx_div_dy(const BLPoint& d) noexcept { return d.x / d.y; }
  static BL_INLINE double dy_div_dx(const BLPoint& d) noexcept { return d.y / d.x; }
};

//! \}

} // {bl::RasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_EDGE_BUILDER_P_H_INCLUDED
