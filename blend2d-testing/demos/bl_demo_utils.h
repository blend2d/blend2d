#ifndef BL_DEMO_UTILS_H_INCLUDED
#define BL_DEMO_UTILS_H_INCLUDED

#include <blend2d/blend2d.h>

#include <blend2d-testing/demos/bl_qt_headers.h>

namespace bl::DemoUtils {

struct PathEditView {
  size_t _closest_vertex = SIZE_MAX;
  size_t _grabbed_vertex = SIZE_MAX;
  int _grabbed_x = 0;
  int _grabbed_y = 0;

  size_t closest_vertex() const noexcept { return _closest_vertex; }
  size_t grabbed_vertex() const noexcept { return _grabbed_vertex; }

  void reset() noexcept {
    _closest_vertex = SIZE_MAX;
    _grabbed_vertex = SIZE_MAX;
    _grabbed_x = 0;
    _grabbed_y = 0;
  }

  bool handle_event(BLPath& path, QMouseEvent* event, double device_pixel_ratio) {
    QPointF position(double(event->position().x()) * device_pixel_ratio,
                     double(event->position().y()) * device_pixel_ratio);

    if (event->type() == QEvent::MouseButtonPress) {
      if (event->button() == Qt::LeftButton) {
        if (_closest_vertex != SIZE_MAX) {
          _grabbed_vertex = _closest_vertex;
          _grabbed_x = position.x();
          _grabbed_y = position.y();
          return true;
        }
      }
    }

    if (event->type() == QEvent::MouseButtonRelease) {
      if (event->button() == Qt::LeftButton) {
        if (_grabbed_vertex != SIZE_MAX) {
          _grabbed_vertex = SIZE_MAX;
          return true;
        }
      }
    }

    if (event->type() == QEvent::MouseMove) {
      if (_grabbed_vertex == SIZE_MAX) {
        path.get_closest_vertex(BLPoint(double(position.x()), double(position.y())), 6.0, &_closest_vertex);
        return true;
      }
      else {
        if (_grabbed_vertex < path.vertex_count()) {
          double x = position.x();
          double y = position.y();
          path.set_vertex_at(_grabbed_vertex, x, y);
          return true;
        }
        else {
          reset();
        }
      }
    }

    return false;
  }
};

[[maybe_unused]]
static void render_path_vertices(
  BLContext& ctx,
  const BLPath& path,
  double r,
  size_t highlight_index,
  BLRgba32 normal_color,
  BLRgba32 highlight_color
) noexcept {
  size_t count = path.vertex_count();
  const BLPoint* vtx = path.vertex_data();

  for (size_t i = 0; i < count; i++) {
    ctx.fill_circle(vtx[i].x, vtx[i].y, r, i == highlight_index ? highlight_color : normal_color);
  }
}

} // {bl::DemoUtils}

#endif // BL_DEMO_UTILS_H_INCLUDED
