// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/math_p.h>
#include <blend2d/geometry/bezier_p.h>
#include <blend2d/geometry/dasher_p.h>

namespace bl::Geometry {
namespace {

class Dasher {
public:
  BLSpan<const float> _array;
  double _tolerance;
  double _angle_param;
  size_t _start_index = 0;
  double _start_remaining = Math::inf<double>();
  bool _start_on = true;

  size_t _index = 0;
  double _remaining = 0.0;
  bool _on = true;
  bool _hold_first = false;
  bool _figure_output = false;

  BLPath& _output;
  BLPath _dash;
  BLPath _first;

  BL_INLINE Dasher(const BLStrokeOptions& options, const BLApproximationOptions& approx, BLPath& output) noexcept
    : _array(options.dash_array()),
      _tolerance(double(approx.simplify_tolerance)),
      _angle_param(double(approx.offset_parameter)),
      _output(output) {}

  BLResult init(double offset) noexcept {
    if (!Math::is_finite(offset) || !Math::is_finite(_tolerance) || !(_tolerance > 0.0) ||
        !Math::is_finite(_angle_param) || !(_angle_param > 0.0)) {
      return bl_make_error(BL_ERROR_INVALID_VALUE);
    }

    if (_array.is_empty()) {
      return BL_SUCCESS;
    }

    double sum = 0.0;
    for (float value : _array) {
      if (!Math::is_finite(value) || value < 0.0f) {
        return bl_make_error(BL_ERROR_INVALID_VALUE);
      }
      sum += double(value);
    }

    if (!(sum > 0.0)) {
      return bl_make_error(BL_ERROR_INVALID_VALUE);
    }

    // An odd-sized array reverses the on/off phase each time it wraps.
    double period = (_array.size() & 1u) ? sum * 2.0 : sum;
    offset = std::fmod(offset, period);
    if (offset < 0.0) {
      offset += period;
    }

    _start_remaining = double(_array[0]);
    while (offset > 0.0 && offset >= _start_remaining) {
      offset -= _start_remaining;
      if (++_start_index == _array.size()) {
        _start_index = 0;
      }
      _start_on = !_start_on;
      _start_remaining = double(_array[_start_index]);
    }
    _start_remaining -= offset;

    // A zero-length initial gap ends at the seam itself, so the following dash starts there.
    if (!_start_on && _start_remaining == 0.0) {
      if (++_start_index == _array.size()) {
        _start_index = 0;
      }
      _start_on = true;
      _start_remaining = double(_array[_start_index]);
    }
    return BL_SUCCESS;
  }

  void advance() noexcept {
    if (++_index == _array.size()) {
      _index = 0;
    }
    _on = !_on;
    _remaining = double(_array[_index]);
  }

  BLResult finish_dash() noexcept {
    if (_dash.is_empty()) {
      return BL_SUCCESS;
    }

    if (_hold_first) {
      // A closed figure's first dash may have to join its final dash at the seam.
      _first.swap(_dash);
      _hold_first = false;
    }
    else {
      BL_PROPAGATE(_output.add_path(_dash));
      _figure_output = true;
      BL_PROPAGATE(_dash.clear());
    }
    return BL_SUCCESS;
  }

  BLResult prepare(BLPoint p) noexcept {
    while (_remaining == 0.0) {
      if (_on) {
        // Zero-length on entries still describe a dash (visible with non-butt caps).
        if (_dash.is_empty()) {
          BL_PROPAGATE(_dash.move_to(p));
          BL_PROPAGATE(_dash.line_to(p));
        }
        BL_PROPAGATE(finish_dash());
      }

      advance();
    }

    if (_on && _dash.is_empty()) {
      BL_PROPAGATE(_dash.move_to(p));
    }
    return BL_SUCCESS;
  }

  BLResult line(BLPoint p0, BLPoint p1) noexcept {
    double remaining_length = length(p0, p1);
    if (!Math::is_finite(remaining_length)) {
      return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
    }

    // A degenerate segment consumes no length, but can still produce a capped point.
    do {
      BL_PROPAGATE(prepare(p0));
      double step = bl_min(remaining_length, _remaining);
      BLPoint p = step == remaining_length ? p1 : lerp(p0, p1, step / remaining_length);
      if (step > 0.0 && (remaining_length - step == remaining_length || p == p0)) {
        return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
      }

      if (_on) {
        BL_PROPAGATE(_dash.line_to(p));
      }

      _remaining -= step;
      remaining_length -= step;
      p0 = p;
    } while (remaining_length > 0.0);

    return BL_SUCCESS;
  }

  BLResult quad(QuadRef curve) noexcept {
    double t = quad_parameter_at_angle(curve, _angle_param);
    if (t >= 1.0) {
      return quad_simple(curve);
    }

    Quad q(curve.vtx);
    for (;;) {
      if (!(t > 0.0 && t < 1.0)) {
        return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
      }

      Quad before, after;
      quad_split(q, before, after, t);
      if (cross(q[1] - q[0], q[2] - q[1]) == 0.0) {
        // The angle step of a collinear reversal is its stationary point. Keep both endpoint
        // tangents exactly zero so rounding cannot introduce another tiny reversal after the split.
        before[1] = before[2];
        after[1] = after[0];
      }
      if (after[0] == q[0] && after[1] == q[1]) {
        return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
      }
      BL_PROPAGATE(quad_simple(before));

      q = after;
      t = quad_parameter_at_angle(q, _angle_param);
      if (t >= 1.0) {
        return quad_simple(q);
      }
    }
  }

  // All subsequent dash splits preserve the angle bound established by quad().
  BLResult quad_simple(QuadRef curve) noexcept {
    Quad q(curve.vtx);
    double remaining_length = quad_length(q);

    for (;;) {
      if (!Math::is_finite(remaining_length)) {
        return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
      }
      BL_PROPAGATE(prepare(q[0]));

      if (remaining_length <= _remaining) {
        if (_on) {
          BL_PROPAGATE(_dash.quad_to(q[1], q[2]));
        }
        _remaining -= remaining_length;
        return BL_SUCCESS;
      }

      double t = quad_parameter_at_length(q, _remaining);
      if (!(t > 0.0 && t < 1.0)) {
        return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
      }

      Quad before, after;
      quad_split(q, before, after, t);
      double after_length = quad_length(after);
      if (!(after_length < remaining_length)) {
        return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
      }
      if (_on) {
        BL_PROPAGATE(_dash.quad_to(before[1], before[2]));
      }

      _remaining = 0.0;
      q = after;
      remaining_length = after_length;
    }
  }

  BLResult finish_figure(bool closed) noexcept {
    if (closed && _hold_first && !_dash.is_empty()) {
      // No dash boundary was crossed: keep a solid closed figure closed.
      BL_PROPAGATE(_dash.close());
      _hold_first = false;
    }
    else if (closed && _on && !_dash.is_empty() && !_first.is_empty()) {
      // Continue the final dash with the saved first dash, omitting its MOVE.
      PathAppender appender;
      BL_PROPAGATE(appender.begin_append(&_dash, _first.byte_size()));
      appender.join_path(_first.view());
      appender.done(&_dash);
      BL_PROPAGATE(_first.clear());
    }

    BL_PROPAGATE(finish_dash());
    if (!_first.is_empty()) {
      BL_PROPAGATE(_output.add_path(_first));
      _figure_output = true;
      BL_PROPAGATE(_first.clear());
    }
    return BL_SUCCESS;
  }

  BLResult figure(BLPathView input, bool closed) noexcept {
    _index = _start_index;
    _remaining = _start_remaining;
    _on = _start_on;
    _hold_first = closed && _on;
    _figure_output = false;

    PathIterator it(input);
    BLPoint start = it.vertex(0);
    BLPoint p = start;
    it.advance_cmd<BL_PATH_CMD_MOVE>();

    auto dash_quad = [&](QuadRef q) noexcept -> BLResult { return quad(q); };
    while (it.has_command()) {
      switch (it.command()) {
        case BL_PATH_CMD_LINE: {
          BLPoint end = it.vertex(0);
          BL_PROPAGATE(line(p, end));
          p = end;
          it.advance_cmd<BL_PATH_CMD_LINE>();
          break;
        }

        case BL_PATH_CMD_QUAD: {
          Quad q(p, it.vertex(0), it.vertex(1));
          BL_PROPAGATE(quad(q));
          p = q[2];
          it.advance_cmd<BL_PATH_CMD_QUAD>();
          break;
        }

        case BL_PATH_CMD_CONIC: {
          Conic c(p, it.vertex(0), it.vertex(1), it.weight());
          BL_PROPAGATE(approximate_conic_with_quads(conic_ref(c), _tolerance, dash_quad));
          p = c[2];
          it.advance_cmd<BL_PATH_CMD_CONIC>();
          break;
        }

        case BL_PATH_CMD_CUBIC: {
          Cubic c(p, it.vertex(0), it.vertex(1), it.vertex(2));
          BL_PROPAGATE(approximate_cubic_with_quads(c, _tolerance, dash_quad));
          p = c[3];
          it.advance_cmd<BL_PATH_CMD_CUBIC>();
          break;
        }

        default: {
          BL_ASSERT(it.is_close());
          if (p != start) {
            BL_PROPAGATE(line(p, start));
          }
          it.advance_cmd<BL_PATH_CMD_CLOSE>();
          break;
        }
      }
    }

    BL_PROPAGATE(finish_figure(closed));

    // Include zero-length on entries exactly at an open figure's endpoint. Positive-length entries
    // starting there have no geometry to emit, and closed figures already handle this point at the seam.
    if (!closed && input.vertex_count > 1u) {
      while (_remaining == 0.0) {
        advance();
        if (_on && _remaining == 0.0) {
          BL_PROPAGATE(_dash.move_to(p));
          BL_PROPAGATE(_dash.line_to(p));
          BL_PROPAGATE(finish_dash());
        }
      }
    }
    return BL_SUCCESS;
  }

  BLResult process(BLPathView input, BLPathDashSinkFunc sink, void* user_data) noexcept {
    if (!input.control_data_size) {
      return input.vertex_count ? bl_make_error(BL_ERROR_CORRUPTED_PATH_DATA) : BL_SUCCESS;
    }

    PathIterator it(input);
    size_t vertex_count = input.vertex_count;
    size_t control_size = input.control_data_size;

    while (control_size) {
      if (!it.is_move()) {
        return bl_make_error(BL_ERROR_CORRUPTED_PATH_DATA);
      }

      PathIterator::Location begin = it.location();
      size_t figure_vertex_count = vertex_count;
      size_t figure_control_size = control_size;
      bool closed = false;

      // Find and validate a complete figure before emitting it. Knowing whether it is closed lets us
      // retain its first dash only when needed to join across the seam.
      do {
        BLPathCmd cmd = it.command();
        size_t n_vertices;
        size_t n_control = 1;
        switch (cmd) {
          case BL_PATH_CMD_MOVE:
          case BL_PATH_CMD_LINE: n_vertices = 1; break;
          case BL_PATH_CMD_QUAD: n_vertices = 2; break;
          case BL_PATH_CMD_CUBIC: n_vertices = 3; break;
          case BL_PATH_CMD_CONIC:
            n_vertices = 2;
            n_control = PathAdvance<BL_PATH_CMD_CONIC>::kControlSize;
            break;
          case BL_PATH_CMD_CLOSE: n_vertices = 0; closed = true; break;
          default: return bl_make_error(BL_ERROR_CORRUPTED_PATH_DATA);
        }

        if (n_vertices > vertex_count || n_control > control_size) {
          return bl_make_error(BL_ERROR_CORRUPTED_PATH_DATA);
        }
        if (cmd == BL_PATH_CMD_CONIC && it.location().control_rev[-ptrdiff_t(n_control - 1u)] != BL_PATH_CMD_CONIC) {
          return bl_make_error(BL_ERROR_CORRUPTED_PATH_DATA);
        }
        for (size_t i = 0; i < n_vertices; i++) {
          if (!Math::is_finite(it.vertex(i))) {
            return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
          }
        }
        if (cmd == BL_PATH_CMD_CONIC && (!Math::is_finite(it.weight()) || !(it.weight() > 0.0))) {
          return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
        }

        vertex_count -= n_vertices;
        control_size -= n_control;
        it.advance(n_vertices, n_control);
      } while (control_size && !closed && !it.is_move());

      BLPathView figure_view{begin.vertex_data, begin.control_rev + 1,
                            figure_vertex_count - vertex_count, figure_control_size - control_size};
      BL_PROPAGATE(figure(figure_view, closed));
      if (_figure_output && sink) {
        BL_PROPAGATE(sink(&_output, user_data));
      }
    }

    return vertex_count ? bl_make_error(BL_ERROR_CORRUPTED_PATH_DATA) : BL_SUCCESS;
  }
};

} // {anonymous}

BL_HIDDEN BLResult dash_path(
  const BLPathView& input_view,
  const BLStrokeOptions& options,
  const BLApproximationOptions& approx,
  BLPath& output_path,
  BLPathDashSinkFunc sink, void* user_data
) noexcept {
  Dasher dasher(options, approx, output_path);
  BL_PROPAGATE(dasher.init(double(options.dash_offset())));

  // Keep aliased input alive if appending or a sink clearing the output detaches its storage.
  BLPath input_storage;
  if (input_view.vertex_count &&
      uintptr_t(input_view.vertex_data) >= uintptr_t(output_path.vertex_data()) &&
      uintptr_t(input_view.vertex_data) < uintptr_t(output_path.vertex_data() + output_path.vertex_count())) {
    input_storage = output_path;
  }
  return dasher.process(input_view, sink, user_data);
}

} // {bl::Geometry}
