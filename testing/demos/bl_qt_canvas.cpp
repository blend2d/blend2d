#include "bl_qt_headers.h"
#include "bl_qt_canvas.h"

#include <chrono>

QBLCanvas::QBLCanvas()
  : _renderer_type(RendererBlend2D),
    _dirty(true),
    _fps(0),
    _frame_count(0) {
  _elapsed_timer.start();
  setMouseTracking(true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QBLCanvas::~QBLCanvas() {}

void QBLCanvas::resizeEvent(QResizeEvent* event) {
  _resize_canvas();
}

void QBLCanvas::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  if (_dirty)
    _render_canvas();
  painter.drawImage(QPoint(0, 0), qt_image);
}

void QBLCanvas::mousePressEvent(QMouseEvent* event) {
  if (on_mouse_event)
    on_mouse_event(event);
}

void QBLCanvas::mouseReleaseEvent(QMouseEvent* event) {
  if (on_mouse_event)
    on_mouse_event(event);
}

void QBLCanvas::mouseMoveEvent(QMouseEvent* event) {
  if (on_mouse_event)
    on_mouse_event(event);
}

void QBLCanvas::set_renderer_type(uint32_t renderer_type) {
  _renderer_type = renderer_type;
  update_canvas();
}

void QBLCanvas::update_canvas(bool force) {
  if (force)
    _render_canvas();
  else
    _dirty = true;
  repaint(0, 0, image_size().w, image_size().h);
}

void QBLCanvas::_resize_canvas() {
  int w = width();
  int h = height();

  float s = devicePixelRatio();
  int sw = int(float(w) * s);
  int sh = int(float(h) * s);

  if (qt_image.width() == sw && qt_image.height() == sh)
    return;

  QImage::Format qimage_format = QImage::Format_ARGB32_Premultiplied;

  qt_image = QImage(sw, sh, qimage_format);
  qt_image.setDevicePixelRatio(s);

  unsigned char* qimage_bits = qt_image.bits();
  qsizetype qimage_stride = qt_image.bytesPerLine();

  qt_image_non_scaling = QImage(qimage_bits, sw, sh, qimage_stride, qimage_format);
  bl_image.create_from_data(sw, sh, BL_FORMAT_PRGB32, qimage_bits, intptr_t(qimage_stride));

  update_canvas(false);
}

void QBLCanvas::_render_canvas() {
  auto startTime = std::chrono::high_resolution_clock::now();

  if (_renderer_type == RendererQt) {
    if (on_render_qt) {
      QPainter ctx(&qt_image_non_scaling);
      on_render_qt(ctx);
    }
  }
  else {
    if (on_render_blend2d) {
      // In Blend2D case the non-zero _renderer_type specifies the number of threads.
      BLContextCreateInfo create_info {};
      create_info.thread_count = _renderer_type;

      BLContext ctx(bl_image, create_info);
      on_render_blend2d(ctx);
    }
  }

  auto endTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> duration = endTime - startTime;

  _render_time_pos = (_render_time_pos + 1) & 31;
  _render_time[_render_time_pos] = duration.count();
  _rendered_frames++;

  _dirty = false;
  _after_render();
}

void QBLCanvas::_after_render() {
  uint64_t t = _elapsed_timer.elapsed();

  _frame_count++;
  if (t >= 1000) {
    _fps = _frame_count / double(t) * 1000.0;
    _frame_count = 0;
    _elapsed_timer.start();
  }
}

double QBLCanvas::last_render_time() const {
  return _rendered_frames > 0 ? _render_time[_render_time_pos] : 0.0;
}

double QBLCanvas::average_render_time() const {
  double sum = 0.0;
  size_t count = _rendered_frames < 32 ? _rendered_frames : size_t(32);

  for (size_t i = 0; i < count; i++) {
    sum += _render_time[i];
  }

  return (sum * 1000.0) / double(count);
}

void QBLCanvas::init_renderer_select_box(QComboBox* dst, bool blend2d_only) {
  static const uint32_t renderer_types[] = {
    RendererQt,
    RendererBlend2D,
    RendererBlend2D_1t,
    RendererBlend2D_2t,
    RendererBlend2D_4t,
    RendererBlend2D_8t,
    RendererBlend2D_12t,
    RendererBlend2D_16t
  };

  for (const auto& renderer_type : renderer_types) {
    if (renderer_type == RendererQt && blend2d_only)
      continue;
    QString s = renderer_type_to_string(renderer_type);
    dst->addItem(s, QVariant(int(renderer_type)));
  }

  dst->setCurrentIndex(blend2d_only ? 0 : 1);
}

QString QBLCanvas::renderer_type_to_string(uint32_t renderer_type) {
  char buffer[32];
  switch (renderer_type) {
    case RendererQt:
      return QLatin1String("Qt");

    default:
      if (renderer_type > 32)
        return QString();

      if (renderer_type == 0)
        return QLatin1String("Blend2D");

      snprintf(buffer, sizeof(buffer), "Blend2D %uT", renderer_type);
      return QLatin1String(buffer);
  }
}

#include "moc_bl_qt_canvas.cpp"
