// This file is part of Blend2D project <https://blend2d.com>
//
// See LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifdef BL_BENCH_ENABLE_QT

#include <blend2d-testing/bench/bl_bench_app.h>
#include <blend2d-testing/bench/bl_bench_backend.h>

#include <QtCore>
#include <QtGui>

namespace blbench {

static inline QColor to_qt_color(const BLRgba32& rgba) {
  return QColor(rgba.r(), rgba.g(), rgba.b(), rgba.a());
}

static uint32_t to_qt_format(BLFormat format) {
  switch (format) {
    case BL_FORMAT_PRGB32: return QImage::Format_ARGB32_Premultiplied;
    case BL_FORMAT_XRGB32: return QImage::Format_RGB32;

    default:
      return 0xFFFFFFFFu;
  }
}

static uint32_t to_qt_operator(BLCompOp comp_op) {
  switch (comp_op) {
    case BL_COMP_OP_SRC_OVER   : return QPainter::CompositionMode_SourceOver;
    case BL_COMP_OP_SRC_COPY   : return QPainter::CompositionMode_Source;
    case BL_COMP_OP_SRC_IN     : return QPainter::CompositionMode_SourceIn;
    case BL_COMP_OP_SRC_OUT    : return QPainter::CompositionMode_SourceOut;
    case BL_COMP_OP_SRC_ATOP   : return QPainter::CompositionMode_SourceAtop;
    case BL_COMP_OP_DST_OVER   : return QPainter::CompositionMode_DestinationOver;
    case BL_COMP_OP_DST_COPY   : return QPainter::CompositionMode_Destination;
    case BL_COMP_OP_DST_IN     : return QPainter::CompositionMode_DestinationIn;
    case BL_COMP_OP_DST_OUT    : return QPainter::CompositionMode_DestinationOut;
    case BL_COMP_OP_DST_ATOP   : return QPainter::CompositionMode_DestinationAtop;
    case BL_COMP_OP_XOR        : return QPainter::CompositionMode_Xor;
    case BL_COMP_OP_CLEAR      : return QPainter::CompositionMode_Clear;
    case BL_COMP_OP_PLUS       : return QPainter::CompositionMode_Plus;
    case BL_COMP_OP_MULTIPLY   : return QPainter::CompositionMode_Multiply;
    case BL_COMP_OP_SCREEN     : return QPainter::CompositionMode_Screen;
    case BL_COMP_OP_OVERLAY    : return QPainter::CompositionMode_Overlay;
    case BL_COMP_OP_DARKEN     : return QPainter::CompositionMode_Darken;
    case BL_COMP_OP_LIGHTEN    : return QPainter::CompositionMode_Lighten;
    case BL_COMP_OP_COLOR_DODGE: return QPainter::CompositionMode_ColorDodge;
    case BL_COMP_OP_COLOR_BURN : return QPainter::CompositionMode_ColorBurn;
    case BL_COMP_OP_HARD_LIGHT : return QPainter::CompositionMode_HardLight;
    case BL_COMP_OP_SOFT_LIGHT : return QPainter::CompositionMode_SoftLight;
    case BL_COMP_OP_DIFFERENCE : return QPainter::CompositionMode_Difference;
    case BL_COMP_OP_EXCLUSION  : return QPainter::CompositionMode_Exclusion;

    default:
      return 0xFFFFFFFFu;
  }
}

struct QtModule : public Backend {
  QImage* _qt_surface {};
  QImage* _qt_sprites[kBenchNumSprites] {};
  QPainter* _qt_context {};

  // Initialized by before_run().
  uint32_t _gradient_spread {};

  QtModule();
  ~QtModule() override;

  void serialize_info(JSONBuilder& json) const override;

  template<typename RectT>
  inline QBrush create_brush(StyleKind style, const RectT& rect);

  bool supports_comp_op(BLCompOp comp_op) const override;
  bool supports_style(StyleKind style) const override;

  void before_run() override;
  void flush() override;
  void after_run() override;

  void render_rect_a(RenderOp op) override;
  void render_rect_f(RenderOp op) override;
  void render_rect_rotated(RenderOp op) override;
  void render_round_f(RenderOp op) override;
  void render_round_rotated(RenderOp op) override;
  void render_polygon(RenderOp op, uint32_t complexity) override;
  void render_shape(RenderOp op, ShapeData shape) override;
};

QtModule::QtModule() {
  strcpy(_name, "Qt6");
}
QtModule::~QtModule() {}

void QtModule::serialize_info(JSONBuilder& json) const {
  json.before_record()
      .add_key("version")
      .add_string(qVersion());
}

template<typename RectT>
inline QBrush QtModule::create_brush(StyleKind style, const RectT& rect) {
  switch (style) {
    case StyleKind::kLinearPad:
    case StyleKind::kLinearRepeat:
    case StyleKind::kLinearReflect: {
      double x0 = rect.x + rect.w * 0.2;
      double y0 = rect.y + rect.h * 0.2;
      double x1 = rect.x + rect.w * 0.8;
      double y1 = rect.y + rect.h * 0.8;

      QLinearGradient g((qreal)x0, (qreal)y0, (qreal)x1, (qreal)y1);
      g.setColorAt(qreal(0.0), to_qt_color(_rnd_color.next_rgba32()));
      g.setColorAt(qreal(0.5), to_qt_color(_rnd_color.next_rgba32()));
      g.setColorAt(qreal(1.0), to_qt_color(_rnd_color.next_rgba32()));
      g.setSpread(static_cast<QGradient::Spread>(_gradient_spread));
      return QBrush(g);
    }

    case StyleKind::kRadialPad:
    case StyleKind::kRadialRepeat:
    case StyleKind::kRadialReflect: {
      double cx = rect.x + rect.w / 2;
      double cy = rect.y + rect.h / 2;
      double cr = (rect.w + rect.h) / 4;
      double fx = cx - cr / 2;
      double fy = cy - cr / 2;

      QRadialGradient g(qreal(cx), qreal(cy), qreal(cr), qreal(fx), qreal(fy), qreal(0));
      g.setColorAt(qreal(0.0), to_qt_color(_rnd_color.next_rgba32()));
      g.setColorAt(qreal(0.5), to_qt_color(_rnd_color.next_rgba32()));
      g.setColorAt(qreal(1.0), to_qt_color(_rnd_color.next_rgba32()));
      g.setSpread(static_cast<QGradient::Spread>(_gradient_spread));
      return QBrush(g);
    }

    case StyleKind::kConic: {
      double cx = rect.x + rect.w / 2;
      double cy = rect.y + rect.h / 2;
      QColor c(to_qt_color(_rnd_color.next_rgba32()));

      QConicalGradient g(qreal(cx), qreal(cy), qreal(0));
      g.setColorAt(qreal(0.00), c);
      g.setColorAt(qreal(0.33), to_qt_color(_rnd_color.next_rgba32()));
      g.setColorAt(qreal(0.66), to_qt_color(_rnd_color.next_rgba32()));
      g.setColorAt(qreal(1.00), c);
      return QBrush(g);
    }

    case StyleKind::kPatternNN:
    case StyleKind::kPatternBI:
    default: {
      QBrush brush(*_qt_sprites[nextSpriteId()]);

      // FIXME: It seems that Qt will never use subpixel filtering when drawing
      // an unscaled image. The test suite, however, expects that path to be
      // triggered. To fix this, we scale the image slightly (it should have no
      // visual impact) to prevent Qt using nearest-neighbor fast-path.
      qreal scale =  style == StyleKind::kPatternNN ? 1.0 : 1.00001;

      brush.setTransform(QTransform(scale, qreal(0), qreal(0), scale, qreal(rect.x), qreal(rect.y)));
      return brush;
    }
  }
}

bool QtModule::supports_comp_op(BLCompOp comp_op) const {
  return to_qt_operator(comp_op) != 0xFFFFFFFFu;
}

bool QtModule::supports_style(StyleKind style) const {
  return style == StyleKind::kSolid         ||
         style == StyleKind::kLinearPad     ||
         style == StyleKind::kLinearRepeat  ||
         style == StyleKind::kLinearReflect ||
         style == StyleKind::kRadialPad     ||
         style == StyleKind::kRadialRepeat  ||
         style == StyleKind::kRadialReflect ||
         style == StyleKind::kConic         ||
         style == StyleKind::kPatternNN     ||
         style == StyleKind::kPatternBI     ;
}

void QtModule::before_run() {
  int w = int(_params.screen_w);
  int h = int(_params.screen_h);
  StyleKind style = _params.style;

  // Initialize the sprites.
  for (uint32_t i = 0; i < kBenchNumSprites; i++) {
    const BLImage& sprite = _sprites[i];

    BLImageData sprite_data;
    sprite.get_data(&sprite_data);

    QImage* qt_sprite = new QImage(
      static_cast<unsigned char*>(sprite_data.pixel_data),
      sprite_data.size.w,
      sprite_data.size.h,
      int(sprite_data.stride), QImage::Format_ARGB32_Premultiplied);

    _qt_sprites[i] = qt_sprite;
  }

  // Initialize the surface and the context.
  BLImageData surface_data;
  _surface.create(w, h, _params.format);
  _surface.make_mutable(&surface_data);

  int stride = int(surface_data.stride);
  int qtFormat = to_qt_format(BLFormat(surface_data.format));

  _qt_surface = new QImage(
    (unsigned char*)surface_data.pixel_data, w, h,
    stride, static_cast<QImage::Format>(qtFormat));

  if (_qt_surface == nullptr) {
    return;
  }

  _qt_context = new QPainter(_qt_surface);

  if (_qt_context == nullptr) {
    return;
  }

  // Setup the context.
  _qt_context->setCompositionMode(QPainter::CompositionMode_Source);
  _qt_context->fillRect(0, 0, w, h, QColor(0, 0, 0, 0));

  _qt_context->setCompositionMode(
    static_cast<QPainter::CompositionMode>(
      to_qt_operator(_params.comp_op)));

  _qt_context->setRenderHint(QPainter::Antialiasing, true);
  _qt_context->setRenderHint(QPainter::SmoothPixmapTransform, _params.style != StyleKind::kPatternNN);

  // Setup globals.
  _gradient_spread = QGradient::PadSpread;

  switch (style) {
    case StyleKind::kLinearPad      : _gradient_spread = QGradient::PadSpread    ; break;
    case StyleKind::kLinearRepeat   : _gradient_spread = QGradient::RepeatSpread ; break;
    case StyleKind::kLinearReflect  : _gradient_spread = QGradient::ReflectSpread; break;
    case StyleKind::kRadialPad      : _gradient_spread = QGradient::PadSpread    ; break;
    case StyleKind::kRadialRepeat   : _gradient_spread = QGradient::RepeatSpread ; break;
    case StyleKind::kRadialReflect  : _gradient_spread = QGradient::ReflectSpread; break;

    default:
      break;
  }
}

void QtModule::flush() {
  // Nothing...
}

void QtModule::after_run() {
  // Free the surface & the context.
  delete _qt_context;
  delete _qt_surface;

  _qt_context = nullptr;
  _qt_surface = nullptr;

  // Free the sprites.
  for (uint32_t i = 0; i < kBenchNumSprites; i++) {
    delete _qt_sprites[i];
    _qt_sprites[i] = nullptr;
  }
}

void QtModule::render_rect_a(RenderOp op) {
  BLSizeI bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  int wh = _params.shape_size;

  if (op == RenderOp::kStroke)
    _qt_context->setBrush(Qt::NoBrush);

  if (style == StyleKind::kSolid) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRectI rect(_rnd_coord.next_rect_i(bounds, wh, wh));
      QColor color(to_qt_color(_rnd_color.next_rgba32()));

      if (op == RenderOp::kStroke) {
        _qt_context->setPen(color);
        _qt_context->drawRect(QRectF(rect.x, rect.y, rect.w, rect.h));
      }
      else {
        _qt_context->fillRect(QRect(rect.x, rect.y, rect.w, rect.h), color);
      }
    }
  }
  else {
    if ((style == StyleKind::kPatternNN || style == StyleKind::kPatternBI) && op != RenderOp::kStroke) {
      for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
        BLRectI rect(_rnd_coord.next_rect_i(bounds, wh, wh));
        const QImage& sprite = *_qt_sprites[nextSpriteId()];

        _qt_context->drawImage(QPoint(rect.x, rect.y), sprite);
      }
    }
    else {
      for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
        BLRectI rect(_rnd_coord.next_rect_i(bounds, wh, wh));
        QBrush brush(create_brush<BLRectI>(style, rect));

        if (op == RenderOp::kStroke) {
          QPen pen(brush, qreal(_params.stroke_width));
          pen.setJoinStyle(Qt::MiterJoin);
          _qt_context->setPen(pen);
          _qt_context->drawRect(QRectF(rect.x, rect.y, rect.w, rect.h));
        }
        else {
          _qt_context->fillRect(QRect(rect.x, rect.y, rect.w, rect.h), brush);
        }
      }
    }
  }
}

void QtModule::render_rect_f(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  double wh = _params.shape_size;

  if (op == RenderOp::kStroke)
    _qt_context->setBrush(Qt::NoBrush);

  if (style == StyleKind::kSolid) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
      QColor color(to_qt_color(_rnd_color.next_rgba32()));

      if (op == RenderOp::kStroke) {
        _qt_context->setPen(color);
        _qt_context->drawRect(QRectF(rect.x, rect.y, rect.w, rect.h));
      }
      else {
        _qt_context->fillRect(QRectF(rect.x, rect.y, rect.w, rect.h), color);
      }
    }
  }
  else {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
      QBrush brush(create_brush<BLRect>(style, rect));

      if (op == RenderOp::kStroke) {
        QPen pen(brush, qreal(_params.stroke_width));
        pen.setJoinStyle(Qt::MiterJoin);

        _qt_context->setPen(pen);
        _qt_context->drawRect(QRectF(rect.x, rect.y, rect.w, rect.h));
      }
      else {
        _qt_context->fillRect(QRectF(rect.x, rect.y, rect.w, rect.h), brush);
      }
    }
  }
}

void QtModule::render_rect_rotated(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double cx = double(_params.screen_w) * 0.5;
  double cy = double(_params.screen_h) * 0.5;
  double wh = _params.shape_size;
  double angle = 0.0;

  if (op == RenderOp::kStroke)
    _qt_context->setBrush(Qt::NoBrush);

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
    BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));

    QTransform transform;
    transform.translate(cx, cy);
    transform.rotateRadians(angle);
    transform.translate(-cx, -cy);
    _qt_context->setTransform(transform, false);

    if (style == StyleKind::kSolid) {
      QColor color(to_qt_color(_rnd_color.next_rgba32()));

      if (op == RenderOp::kStroke) {
        QPen pen(color, qreal(_params.stroke_width));
        pen.setJoinStyle(Qt::MiterJoin);

        _qt_context->setPen(pen);
        _qt_context->drawRect(QRectF(rect.x, rect.y, rect.w, rect.h));
      }
      else {
        _qt_context->fillRect(QRectF(rect.x, rect.y, rect.w, rect.h), color);
      }
    }
    else {
      QBrush brush(create_brush<BLRect>(style, rect));

      if (op == RenderOp::kStroke) {
        QPen pen(brush, qreal(_params.stroke_width));
        pen.setJoinStyle(Qt::MiterJoin);

        _qt_context->setPen(pen);
        _qt_context->drawRect(QRectF(rect.x, rect.y, rect.w, rect.h));
      }
      else {
        _qt_context->fillRect(QRectF(rect.x, rect.y, rect.w, rect.h), brush);
      }
    }

    _qt_context->resetTransform();
  }
}

void QtModule::render_round_f(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  double wh = _params.shape_size;

  if (op == RenderOp::kStroke)
    _qt_context->setBrush(Qt::NoBrush);
  else
    _qt_context->setPen(QPen(Qt::NoPen));

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
    double radius = _rnd_extra.next_double(4.0, 40.0);

    if (style == StyleKind::kSolid) {
      QColor color(to_qt_color(_rnd_color.next_rgba32()));

      if (op == RenderOp::kStroke)
        _qt_context->setPen(QPen(color, qreal(_params.stroke_width)));
      else
        _qt_context->setBrush(QBrush(color));
    }
    else {
      QBrush brush(create_brush<BLRect>(style, rect));

      if (op == RenderOp::kStroke)
        _qt_context->setPen(QPen(brush, qreal(_params.stroke_width)));
      else
        _qt_context->setBrush(brush);
    }

    _qt_context->drawRoundedRect(
      QRectF(rect.x, rect.y, rect.w, rect.h),
      std::min(rect.w * 0.5, radius),
      std::min(rect.h * 0.5, radius));
  }
}

void QtModule::render_round_rotated(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double cx = double(_params.screen_w) * 0.5;
  double cy = double(_params.screen_h) * 0.5;
  double wh = _params.shape_size;
  double angle = 0.0;

  if (op == RenderOp::kStroke)
    _qt_context->setBrush(Qt::NoBrush);
  else
    _qt_context->setPen(QPen(Qt::NoPen));

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
    BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
    double radius = _rnd_extra.next_double(4.0, 40.0);

    QTransform transform;
    transform.translate(cx, cy);
    transform.rotateRadians(angle);
    transform.translate(-cx, -cy);
    _qt_context->setTransform(transform, false);

    if (style == StyleKind::kSolid) {
      QColor color(to_qt_color(_rnd_color.next_rgba32()));
      if (op == RenderOp::kStroke)
        _qt_context->setPen(QPen(color, qreal(_params.stroke_width)));
      else
        _qt_context->setBrush(QBrush(color));
    }
    else {
      QBrush brush(create_brush<BLRect>(style, rect));
      if (op == RenderOp::kStroke)
        _qt_context->setPen(QPen(brush, qreal(_params.stroke_width)));
      else
        _qt_context->setBrush(brush);
    }

    _qt_context->drawRoundedRect(
      QRectF(rect.x, rect.y, rect.w, rect.h),
      std::min(rect.w * 0.5, radius),
      std::min(rect.h * 0.5, radius));

    _qt_context->resetTransform();
  }
}

void QtModule::render_polygon(RenderOp op, uint32_t complexity) {
  BLSizeI bounds(_params.screen_w - _params.shape_size,
                 _params.screen_h - _params.shape_size);
  StyleKind style = _params.style;
  double wh = double(_params.shape_size);

  Qt::FillRule fillRule = op == RenderOp::kFillEvenOdd ? Qt::OddEvenFill : Qt::WindingFill;

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLPoint base(_rnd_coord.nextPoint(bounds));

    double x = _rnd_coord.next_double(base.x, base.x + wh);
    double y = _rnd_coord.next_double(base.y, base.y + wh);

    QPainterPath path;
    path.setFillRule(fillRule);
    path.moveTo(x, y);

    for (uint32_t p = 1; p < complexity; p++) {
      x = _rnd_coord.next_double(base.x, base.x + wh);
      y = _rnd_coord.next_double(base.y, base.y + wh);
      path.lineTo(x, y);
    }

    if (style == StyleKind::kSolid) {
      QColor color(to_qt_color(_rnd_color.next_rgba32()));

      if (op == RenderOp::kStroke) {
        QPen pen(color, qreal(_params.stroke_width));
        pen.setJoinStyle(Qt::MiterJoin);
        _qt_context->strokePath(path, pen);
      }
      else {
        _qt_context->fillPath(path, QBrush(color));
      }
    }
    else {
      BLRect rect(base.x, base.y, wh, wh);
      QBrush brush(create_brush<BLRect>(style, rect));

      if (op == RenderOp::kStroke) {
        QPen pen(brush, qreal(_params.stroke_width));
        pen.setJoinStyle(Qt::MiterJoin);
        _qt_context->strokePath(path, pen);
      }
      else {
        _qt_context->fillPath(path, brush);
      }
    }
  }
}

void QtModule::render_shape(RenderOp op, ShapeData shape) {
  BLSizeI bounds(_params.screen_w - _params.shape_size,
                 _params.screen_h - _params.shape_size);
  StyleKind style = _params.style;
  double wh = double(_params.shape_size);

  ShapeIterator it(shape);
  QPainterPath path;
  while (it.has_command()) {
    if (it.is_move_to()) {
      path.moveTo(it.x(0) * wh, it.y(0) * wh);
    }
    else if (it.is_line_to()) {
      path.lineTo(it.x(0) * wh, it.y(0) * wh);
    }
    else if (it.is_quad_to()) {
      path.quadTo(
        it.x(0) * wh, it.y(0) * wh,
        it.x(1) * wh, it.y(1) * wh);
    }
    else if (it.is_cubic_to()) {
      path.cubicTo(
        it.x(0) * wh, it.y(0) * wh,
        it.x(1) * wh, it.y(1) * wh,
        it.x(2) * wh, it.y(2) * wh);
    }
    else {
      path.closeSubpath();
    }
    it.next();
  }

  Qt::FillRule fillRule = op == RenderOp::kFillEvenOdd ? Qt::OddEvenFill : Qt::WindingFill;

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLPoint base(_rnd_coord.nextPoint(bounds));

    _qt_context->save();
    _qt_context->translate(qreal(base.x), qreal(base.y));

    if (style == StyleKind::kSolid) {
      QColor color(to_qt_color(_rnd_color.next_rgba32()));

      if (op == RenderOp::kStroke) {
        QPen pen(color, qreal(_params.stroke_width));
        pen.setJoinStyle(Qt::MiterJoin);
        _qt_context->strokePath(path, pen);
      }
      else {
        _qt_context->fillPath(path, QBrush(color));
      }
    }
    else {
      BLRect rect(0, 0, wh, wh);
      QBrush brush(create_brush<BLRect>(style, rect));

      if (op == RenderOp::kStroke) {
        QPen pen(brush, qreal(_params.stroke_width));
        pen.setJoinStyle(Qt::MiterJoin);
        _qt_context->strokePath(path, pen);
      }
      else {
        _qt_context->fillPath(path, brush);
      }
    }

    _qt_context->restore();
  }
}

Backend* create_qt_backend() {
  return new QtModule();
}

} // {blbench}

#endif // BL_BENCH_ENABLE_QT
