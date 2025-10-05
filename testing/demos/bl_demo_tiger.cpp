#include "bl_qt_headers.h"
#include "bl_qt_canvas.h"
#include "bl_demo_tiger.h"

#include <stdlib.h>

#define ARRAY_SIZE(X) uint32_t(sizeof(X) / sizeof(X[0]))

static const double PI = 3.14159265359;

struct TigerPath {
  inline TigerPath()
    : fill_color(0),
      stroke_color(0),
      qt_pen(Qt::NoPen),
      fill_rule(BL_FILL_RULE_NON_ZERO),
      fill(false),
      stroke(false) {}
  inline ~TigerPath() {}

  BLPath bl_path;
  BLPath bl_stroked_path;
  BLStrokeOptions bl_stroke_options;

  BLRgba32 fill_color;
  BLRgba32 stroke_color;

  QPainterPath qt_path;
  QPainterPath qt_stroked_path;
  QPen qt_pen;
  QBrush qt_brush;

  BLFillRule fill_rule;
  bool fill;
  bool stroke;
};

struct Tiger {
  Tiger() {
    init(
      TigerData::commands, ARRAY_SIZE(TigerData::commands),
      TigerData::points, ARRAY_SIZE(TigerData::points));
  }

  ~Tiger() {
    destroy();
  }

  void init(const char* commands, int commandCount, const float* points, uint32_t pointCount) {
    size_t c = 0;
    size_t p = 0;

    float h = float(TigerData::height);

    while (c < commandCount) {
      TigerPath* tp = new TigerPath();

      // Fill params.
      switch (commands[c++]) {
        case 'N': tp->fill = false; break;
        case 'F': tp->fill = true; tp->fill_rule = BL_FILL_RULE_NON_ZERO; break;
        case 'E': tp->fill = true; tp->fill_rule = BL_FILL_RULE_EVEN_ODD; break;
      }

      // Stroke params.
      switch (commands[c++]) {
        case 'N': tp->stroke = false; break;
        case 'S': tp->stroke = true; break;
      }

      switch (commands[c++]) {
        case 'B': tp->bl_stroke_options.set_caps(BL_STROKE_CAP_BUTT); break;
        case 'R': tp->bl_stroke_options.set_caps(BL_STROKE_CAP_ROUND); break;
        case 'S': tp->bl_stroke_options.set_caps(BL_STROKE_CAP_SQUARE); break;
      }

      switch (commands[c++]) {
        case 'M': tp->bl_stroke_options.join = BL_STROKE_JOIN_MITER_BEVEL; break;
        case 'R': tp->bl_stroke_options.join = BL_STROKE_JOIN_ROUND; break;
        case 'B': tp->bl_stroke_options.join = BL_STROKE_JOIN_BEVEL; break;
      }

      tp->bl_stroke_options.miter_limit = points[p++];
      tp->bl_stroke_options.width = points[p++];

      // Stroke & Fill style.
      tp->stroke_color = BLRgba32(uint32_t(points[p + 0] * 255.0f), uint32_t(points[p + 1] * 255.0f), uint32_t(points[p + 2] * 255.0f), 255);
      tp->fill_color = BLRgba32(uint32_t(points[p + 3] * 255.0f), uint32_t(points[p + 4] * 255.0f), uint32_t(points[p + 5] * 255.0f), 255);
      p += 6;

      // Path.
      int i, count = int(points[p++]);
      for (i = 0 ; i < count; i++) {
        switch (commands[c++]) {
          case 'M':
            tp->bl_path.move_to(points[p], h - points[p + 1]);
            tp->qt_path.moveTo(points[p], h - points[p + 1]);
            p += 2;
            break;
          case 'L':
            tp->bl_path.line_to(points[p], h - points[p + 1]);
            tp->qt_path.lineTo(points[p], h - points[p + 1]);
            p += 2;
            break;
          case 'C':
            tp->bl_path.cubic_to(points[p], h - points[p + 1], points[p + 2], h - points[p + 3], points[p + 4], h - points[p + 5]);
            tp->qt_path.cubicTo(points[p], h - points[p + 1], points[p + 2], h - points[p + 3], points[p + 4], h - points[p + 5]);
            p += 6;
            break;
          case 'E':
            tp->bl_path.close();
            tp->qt_path.closeSubpath();
            break;
        }
      }

      tp->bl_path.shrink();
      tp->qt_path.setFillRule(tp->fill_rule == BL_FILL_RULE_NON_ZERO ? Qt::WindingFill : Qt::OddEvenFill);

      if (tp->fill) {
        tp->qt_brush = QBrush(bl_rgba_to_qcolor(tp->fill_color));
      }

      if (tp->stroke) {
        tp->bl_stroked_path.add_stroked_path(tp->bl_path, tp->bl_stroke_options, bl_default_approximation_options);
        tp->bl_stroked_path.shrink();

        tp->qt_pen = QPen(bl_rgba_to_qcolor(tp->stroke_color));
        tp->qt_pen.setWidthF(tp->bl_stroke_options.width);
        tp->qt_pen.setMiterLimit(tp->bl_stroke_options.miter_limit);

        Qt::PenCapStyle qtCapStyle =
          tp->bl_stroke_options.start_cap == BL_STROKE_CAP_BUTT  ? Qt::FlatCap  :
          tp->bl_stroke_options.start_cap == BL_STROKE_CAP_ROUND ? Qt::RoundCap : Qt::SquareCap;

        Qt::PenJoinStyle qtJoinStyle =
          tp->bl_stroke_options.join == BL_STROKE_JOIN_ROUND ? Qt::RoundJoin :
          tp->bl_stroke_options.join == BL_STROKE_JOIN_BEVEL ? Qt::BevelJoin : Qt::MiterJoin;

        tp->qt_pen.setCapStyle(qtCapStyle);
        tp->qt_pen.setJoinStyle(qtJoinStyle);

        QPainterPathStroker stroker;
        stroker.setWidth(tp->bl_stroke_options.width);
        stroker.setMiterLimit(tp->bl_stroke_options.miter_limit);
        stroker.setJoinStyle(qtJoinStyle);
        stroker.setCapStyle(qtCapStyle);

        tp->qt_stroked_path = stroker.createStroke(tp->qt_path);
      }

      paths.append(tp);
    }
  }

  void destroy() {
    for (size_t i = 0, count = paths.size(); i < count; i++)
      delete paths[i];
    paths.reset();
  }

  BLArray<TigerPath*> paths;
};

class MainWindow : public QWidget {
  Q_OBJECT

public:
  QTimer _timer;
  QBLCanvas _canvas;
  QComboBox _renderer_select;
  QCheckBox _limit_fps_check;
  QComboBox _caching_select;
  QSlider _slider;
  Tiger _tiger;

  bool _animate = true;
  bool _cache_stroke = false;
  bool _render_stroke = true;
  double _rot = 0.0;
  double _scale = 1.0;

  MainWindow() {
    QVBoxLayout* vBox = new QVBoxLayout();
    vBox->setContentsMargins(0, 0, 0, 0);
    vBox->setSpacing(0);

    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(5, 5, 5, 5);
    grid->setSpacing(5);

    QBLCanvas::init_renderer_select_box(&_renderer_select);
    _limit_fps_check.setText(QLatin1String("Limit FPS"));

    _caching_select.addItem("None", QVariant(int(0)));
    _caching_select.addItem("Strokes", QVariant(int(1)));

    _slider.setOrientation(Qt::Horizontal);
    _slider.setMinimum(50);
    _slider.setMaximum(20000);
    _slider.setSliderPosition(1000);

    connect(&_renderer_select, SIGNAL(currentIndexChanged(int)), SLOT(onRendererChanged(int)));
    connect(&_limit_fps_check, SIGNAL(stateChanged(int)), SLOT(onLimitFpsChanged(int)));
    connect(&_caching_select, SIGNAL(currentIndexChanged(int)), SLOT(onCachingChanged(int)));
    connect(&_slider, SIGNAL(valueChanged(int)), SLOT(onZoomChanged(int)));

    _canvas.on_render_blend2d = std::bind(&MainWindow::on_render_blend2d, this, std::placeholders::_1);
    _canvas.on_render_qt = std::bind(&MainWindow::on_render_qt, this, std::placeholders::_1);

    grid->addWidget(new QLabel("Renderer:"), 0, 0, Qt::AlignRight);
    grid->addWidget(&_renderer_select, 0, 1);

    grid->addWidget(new QLabel("Caching:"), 0, 2, Qt::AlignRight);
    grid->addWidget(&_caching_select, 0, 3);

    grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 0, 4);
    grid->addWidget(&_limit_fps_check, 0, 5);

    grid->addWidget(new QLabel("Zoom:"), 1, 0, Qt::AlignRight);
    grid->addWidget(&_slider, 1, 1, 1, 5);

    _canvas.on_render_blend2d = std::bind(&MainWindow::on_render_blend2d, this, std::placeholders::_1);
    _canvas.on_render_qt = std::bind(&MainWindow::on_render_qt, this, std::placeholders::_1);

    vBox->addLayout(grid);
    vBox->addWidget(&_canvas);
    setLayout(vBox);

    connect(&_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
    connect(new QShortcut(QKeySequence(Qt::Key_P), this), SIGNAL(activated()), SLOT(onToggleAnimate()));
    connect(new QShortcut(QKeySequence(Qt::Key_R), this), SIGNAL(activated()), SLOT(onToggleRenderer()));
    connect(new QShortcut(QKeySequence(Qt::Key_S), this), SIGNAL(activated()), SLOT(onToggleStroke()));
    connect(new QShortcut(QKeySequence(Qt::Key_Q), this), SIGNAL(activated()), SLOT(onRotatePrev()));
    connect(new QShortcut(QKeySequence(Qt::Key_W), this), SIGNAL(activated()), SLOT(onRotateNext()));

    onInit();
  }

  void showEvent(QShowEvent* event) override { _timer.start(); }
  void hideEvent(QHideEvent* event) override { _timer.stop(); }

  void onInit() {
    _updateTitle();
    _limit_fps_check.setChecked(true);
  }

  Q_SLOT void onRendererChanged(int index) { _canvas.set_renderer_type(_renderer_select.itemData(index).toInt()); }
  Q_SLOT void onLimitFpsChanged(int value) { _timer.setInterval(value ? 1000 / 120 : 0); }

  Q_SLOT void onCachingChanged(int index) { _cache_stroke = index != 0; }
  Q_SLOT void onZoomChanged(int value) { _scale = (double(value) / 1000.0); }

  Q_SLOT void onToggleAnimate() { _animate = !_animate; }
  Q_SLOT void onToggleRenderer() { _renderer_select.setCurrentIndex(_renderer_select.currentIndex() ^ 1); }
  Q_SLOT void onToggleStroke() { _render_stroke = !_render_stroke; }
  Q_SLOT void onRotatePrev() { _rot -= 0.25; }
  Q_SLOT void onRotateNext() { _rot += 0.25; }

  Q_SLOT void onTimer() {
    if (_animate) {
      _rot += 0.25;
      if (_rot >= 360) _rot -= 360.0;
    }

    _canvas.update_canvas(true);
    _updateTitle();
  }

  void on_render_blend2d(BLContext& ctx) noexcept {
    ctx.fill_all(BLRgba32(0xFF00007Fu));

    bool renderStroke = _render_stroke;
    double min_x = 17;
    double min_y = 53;
    double max_x = 562.0;
    double max_y = 613.0;
    double s = bl_min(_canvas.image_width() / (max_x - min_x), _canvas.image_height() / (max_y - min_y)) * _scale;

    BLMatrix2D transform;
    transform.reset();
    transform.rotate((_rot / 180.0) * PI, min_x + max_x / 2.0, min_y + max_y / 2.0);
    transform.post_translate(-max_x / 2, -max_y / 2);

    ctx.save();
    ctx.translate(_canvas.image_width() / 2, _canvas.image_height() / 2);
    ctx.scale(s);
    ctx.apply_transform(transform);

    for (size_t i = 0, count = _tiger.paths.size(); i < count; i++) {
      const TigerPath* tp = _tiger.paths[i];

      if (tp->fill) {
        ctx.set_fill_rule(tp->fill_rule);
        ctx.fill_path(tp->bl_path, tp->fill_color);
      }

      if (tp->stroke && renderStroke) {
        if (_cache_stroke) {
          ctx.fill_path(tp->bl_stroked_path, tp->stroke_color);
        }
        else {
          ctx.set_stroke_options(tp->bl_stroke_options);
          ctx.stroke_path(tp->bl_path, tp->stroke_color);
        }
      }
    }

    ctx.restore();
  }

  void on_render_qt(QPainter& ctx) noexcept {
    bool renderStroke = _render_stroke;

    ctx.fillRect(0, 0, _canvas.image_width(), _canvas.image_height(), QColor(0, 0, 0x7F));
    ctx.setRenderHint(QPainter::Antialiasing, true);

    double min_x = 17;
    double min_y = 53;
    double max_x = 562.0;
    double max_y = 613.0;
    double s = bl_min(_canvas.image_width() / (max_x - min_x), _canvas.image_height() / (max_y - min_y)) * _scale;

    BLMatrix2D m;
    m.reset();
    m.rotate((_rot / 180.0) * PI, min_x + max_x / 2.0, min_y + max_y / 2.0);
    m.post_translate(-max_x / 2, -max_y / 2);

    ctx.save();
    ctx.translate(_canvas.image_width() / 2, _canvas.image_height() / 2);
    ctx.scale(s, s);
    ctx.setTransform(QTransform(m.m00, m.m01, m.m10, m.m11, m.m20, m.m21), true);

    for (size_t i = 0, count = _tiger.paths.size(); i < count; i++) {
      const TigerPath* tp = _tiger.paths[i];

      if (tp->fill) {
        ctx.fillPath(tp->qt_path, tp->qt_brush);
      }

      if (tp->stroke && renderStroke) {
        if (_cache_stroke)
          ctx.fillPath(tp->qt_stroked_path, tp->qt_pen.brush());
        else
          ctx.strokePath(tp->qt_path, tp->qt_pen);
      }
    }

    ctx.restore();
  }

  void _updateTitle() {
    char buf[256];
    snprintf(buf, 256, "Tiger [%dx%d] [RenderTime=%.2fms FPS=%.1f]",
      _canvas.image_width(),
      _canvas.image_height(),
      _canvas.average_render_time(),
      _canvas.fps());

    QString title = QString::fromUtf8(buf);
    if (title != windowTitle())
      setWindowTitle(title);
  }
};

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  MainWindow win;

  win.setMinimumSize(QSize(400, 320));
  win.resize(QSize(580, 520));
  win.show();

  return app.exec();
}

#include "bl_demo_tiger.moc"
