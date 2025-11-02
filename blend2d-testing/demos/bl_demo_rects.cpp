#include <blend2d/blend2d.h>
#include <blend2d-testing/demos/bl_qt_headers.h>
#include <blend2d-testing/demos/bl_qt_canvas.h>

#include <stdlib.h>
#include <vector>

class MainWindow : public QWidget {
  Q_OBJECT

public:
  QTimer _timer;
  QSlider _size_slider;
  QSlider _count_slider;
  QComboBox _renderer_select;
  QComboBox _comp_op_select;
  QComboBox _shape_type_select;
  QCheckBox _limit_fps_check;
  QBLCanvas _canvas;

  BLRandom _random;
  bool _animate = true;
  std::vector<BLPoint> _coords;
  std::vector<BLPoint> _steps;
  std::vector<BLRgba32> _colors;
  BLCompOp _comp_op = BL_COMP_OP_SRC_OVER;
  uint32_t _shape_type = 0;
  double _rect_size = 64.0;

  enum ShapeType {
    kShapeRectA,
    kShapeRectU,
    kShapeRectPath,
    kShapeRoundRect,
    kShapePolyPath,
  };

  MainWindow() : _random(0x123456789ABCDEF) {
    QVBoxLayout* vBox = new QVBoxLayout();
    vBox->setContentsMargins(0, 0, 0, 0);
    vBox->setSpacing(0);

    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(5, 5, 5, 5);
    grid->setSpacing(5);

    QBLCanvas::init_renderer_select_box(&_renderer_select);
    _comp_op_select.addItem("SrcOver", QVariant(int(BL_COMP_OP_SRC_OVER)));
    _comp_op_select.addItem("SrcCopy", QVariant(int(BL_COMP_OP_SRC_COPY)));
    _comp_op_select.addItem("SrcAtop", QVariant(int(BL_COMP_OP_SRC_ATOP)));
    _comp_op_select.addItem("DstAtop", QVariant(int(BL_COMP_OP_DST_ATOP)));
    _comp_op_select.addItem("Xor", QVariant(int(BL_COMP_OP_XOR)));
    _comp_op_select.addItem("Plus", QVariant(int(BL_COMP_OP_PLUS)));
    _comp_op_select.addItem("Multiply", QVariant(int(BL_COMP_OP_MULTIPLY)));
    _comp_op_select.addItem("Screen", QVariant(int(BL_COMP_OP_SCREEN)));
    _comp_op_select.addItem("Overlay", QVariant(int(BL_COMP_OP_OVERLAY)));
    _comp_op_select.addItem("Darken", QVariant(int(BL_COMP_OP_DARKEN)));
    _comp_op_select.addItem("Lighten", QVariant(int(BL_COMP_OP_LIGHTEN)));
    _comp_op_select.addItem("Color Dodge", QVariant(int(BL_COMP_OP_COLOR_DODGE)));
    _comp_op_select.addItem("Color Burn", QVariant(int(BL_COMP_OP_COLOR_BURN)));
    _comp_op_select.addItem("Hard Light", QVariant(int(BL_COMP_OP_HARD_LIGHT)));
    _comp_op_select.addItem("Soft Light", QVariant(int(BL_COMP_OP_SOFT_LIGHT)));
    _comp_op_select.addItem("Difference", QVariant(int(BL_COMP_OP_DIFFERENCE)));
    _comp_op_select.addItem("Exclusion", QVariant(int(BL_COMP_OP_EXCLUSION)));

    _shape_type_select.addItem("RectA", QVariant(int(kShapeRectA)));
    _shape_type_select.addItem("RectU", QVariant(int(kShapeRectU)));
    _shape_type_select.addItem("RectPath", QVariant(int(kShapeRectPath)));
    _shape_type_select.addItem("RoundRect", QVariant(int(kShapeRoundRect)));
    _shape_type_select.addItem("Polygon", QVariant(int(kShapePolyPath)));

    _limit_fps_check.setText(QLatin1String("Limit FPS"));

    _size_slider.setOrientation(Qt::Horizontal);
    _size_slider.setMinimum(1);
    _size_slider.setMaximum(128);
    _size_slider.setSliderPosition(64);

    _count_slider.setOrientation(Qt::Horizontal);
    _count_slider.setMinimum(1);
    _count_slider.setMaximum(20000);
    _count_slider.setSliderPosition(200);

    _canvas.on_render_blend2d = std::bind(&MainWindow::on_render_blend2d, this, std::placeholders::_1);
    _canvas.on_render_qt = std::bind(&MainWindow::on_render_qt, this, std::placeholders::_1);

    connect(&_renderer_select, SIGNAL(activated(int)), SLOT(onRendererChanged(int)));
    connect(&_comp_op_select, SIGNAL(activated(int)), SLOT(onCompOpChanged(int)));
    connect(&_shape_type_select, SIGNAL(activated(int)), SLOT(onShapeTypeChanged(int)));
    connect(&_limit_fps_check, SIGNAL(stateChanged(int)), SLOT(onLimitFpsChanged(int)));
    connect(&_size_slider, SIGNAL(valueChanged(int)), SLOT(onSizeChanged(int)));
    connect(&_count_slider, SIGNAL(valueChanged(int)), SLOT(onCountChanged(int)));

    grid->addWidget(new QLabel("Renderer:"), 0, 0);
    grid->addWidget(&_renderer_select, 0, 1);

    grid->addWidget(new QLabel("Comp Op:"), 0, 2);
    grid->addWidget(&_comp_op_select, 0, 3);

    grid->addWidget(new QLabel("Shape:"), 0, 4);
    grid->addWidget(&_shape_type_select, 0, 5);

    grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 0, 6);
    grid->addWidget(&_limit_fps_check, 0, 7, Qt::AlignRight);

    grid->addWidget(new QLabel("Count:"), 1, 0, 1, 1, Qt::AlignRight);
    grid->addWidget(&_count_slider, 1, 1, 1, 7);

    grid->addWidget(new QLabel("Size:"), 2, 0, 1, 1, Qt::AlignRight);
    grid->addWidget(&_size_slider, 2, 1, 1, 7);

    vBox->addLayout(grid);
    vBox->addWidget(&_canvas);
    setLayout(vBox);

    connect(&_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
    connect(new QShortcut(QKeySequence(Qt::Key_P), this), SIGNAL(activated()), SLOT(onToggleAnimate()));
    connect(new QShortcut(QKeySequence(Qt::Key_S), this), SIGNAL(activated()), SLOT(onStep()));

    onInit();
  }

  void showEvent(QShowEvent* event) override { _timer.start(); }
  void hideEvent(QHideEvent* event) override { _timer.stop(); }
  void keyPressEvent(QKeyEvent* event) override {}

  void onInit() {
    setCount(_count_slider.sliderPosition());
    _limit_fps_check.setChecked(true);
    _updateTitle();
  }

  double randomSign() noexcept { return _random.next_double() < 0.5 ? 1.0 : -1.0; }
  BLRgba32 randomColor() noexcept { return BLRgba32(_random.next_uint32()); }

  Q_SLOT void onToggleAnimate() { _animate = !_animate; }
  Q_SLOT void onStep() { step(); }

  Q_SLOT void onRendererChanged(int index) { _canvas.set_renderer_type(_renderer_select.itemData(index).toInt());  }
  Q_SLOT void onCompOpChanged(int index) { _comp_op = (BLCompOp)_comp_op_select.itemData(index).toInt(); };
  Q_SLOT void onShapeTypeChanged(int index) { _shape_type = _shape_type_select.itemData(index).toInt(); };
  Q_SLOT void onLimitFpsChanged(int value) { _timer.setInterval(value ? 1000 / 120 : 0); }
  Q_SLOT void onSizeChanged(int value) { _rect_size = value; }
  Q_SLOT void onCountChanged(int value) { setCount(size_t(value)); }

  Q_SLOT void onTimer() {
    if (_animate) {
      step();
    }

    _canvas.update_canvas(true);
    _updateTitle();
  }

  void step() noexcept {
    double w = _canvas.image_width();
    double h = _canvas.image_height();

    size_t size = _coords.size();
    for (size_t i = 0; i < size; i++) {
      BLPoint& vertex = _coords[i];
      BLPoint& step = _steps[i];

      vertex += step;
      if (vertex.x <= 0.0 || vertex.x >= w) {
        step.x = -step.x;
        vertex.x = bl_min(vertex.x + step.x, w);
      }

      if (vertex.y <= 0.0 || vertex.y >= h) {
        step.y = -step.y;
        vertex.y = bl_min(vertex.y + step.y, h);
      }
    }
  }

  void on_render_blend2d(BLContext& ctx) noexcept {
    ctx.set_comp_op(BL_COMP_OP_SRC_COPY);
    ctx.fill_all(bl_background_for_comp_op(_comp_op));
    ctx.set_comp_op(_comp_op);

    size_t i;
    size_t size = _coords.size();

    double rectSize = _rect_size;
    double halfSize = _rect_size * 0.5;

    switch (_shape_type) {
      case kShapeRectA: {
        int rectSizeI = int(_rect_size);
        for (i = 0; i < size; i++) {
          int x = int(_coords[i].x - halfSize);
          int y = int(_coords[i].y - halfSize);
          ctx.fill_rect(BLRectI(x, y, rectSizeI, rectSizeI), _colors[i]);
        }
        break;
      }

      case kShapeRectU: {
        for (i = 0; i < size; i++) {
          double x = _coords[i].x - halfSize;
          double y = _coords[i].y - halfSize;
          ctx.fill_rect(x, y, rectSize, rectSize, _colors[i]);
        }
        break;
      }

      case kShapeRectPath: {
        for (i = 0; i < size; i++) {
          double x = _coords[i].x - halfSize;
          double y = _coords[i].y - halfSize;

          BLPath path;
          path.add_rect(x, y, rectSize, rectSize);
          ctx.fill_path(path, _colors[i]);
        }
        break;
      }

      case kShapePolyPath: {
        for (i = 0; i < size; i++) {
          double x = _coords[i].x - halfSize;
          double y = _coords[i].y - halfSize;

          BLPath path;
          path.move_to(x + rectSize / 2, y);
          path.line_to(x + rectSize, y + rectSize / 3);
          path.line_to(x + rectSize - rectSize / 3, y + rectSize);
          path.line_to(x + rectSize / 3, y + rectSize);
          path.line_to(x, y + rectSize / 3);
          ctx.fill_path(path, _colors[i]);
        }
        break;
      }

      case kShapeRoundRect: {
        for (i = 0; i < size; i++) {
          double x = _coords[i].x - halfSize;
          double y = _coords[i].y - halfSize;

          ctx.fill_round_rect(BLRoundRect(x, y, rectSize, rectSize, 10), _colors[i]);
        }
        break;
      }
    }
  }

  void on_render_qt(QPainter& ctx) noexcept {
    ctx.setCompositionMode(QPainter::CompositionMode_Source);
    ctx.fillRect(0, 0, _canvas.image_width(), _canvas.image_height(), bl_rgba_to_qcolor(bl_background_for_comp_op(_comp_op)));
    ctx.setRenderHint(QPainter::Antialiasing, true);
    ctx.setCompositionMode(bl_comp_op_to_qt_composition_mode(_comp_op));

    size_t i;
    size_t size = _coords.size();

    double rectSize = _rect_size;
    double halfSize = _rect_size * 0.5;

    switch (_shape_type) {
      case kShapeRectA: {
        int rectSizeI = int(_rect_size);
        for (i = 0; i < size; i++) {
          int x = int(_coords[i].x - halfSize);
          int y = int(_coords[i].y - halfSize);
          ctx.fillRect(QRect(x, y, rectSizeI, rectSizeI), bl_rgba_to_qcolor(_colors[i]));
        }
        break;
      }

      case kShapeRectU: {
        for (i = 0; i < size; i++) {
          double x = _coords[i].x - halfSize;
          double y = _coords[i].y - halfSize;
          ctx.fillRect(QRectF(_coords[i].x - halfSize, _coords[i].y - halfSize, rectSize, rectSize), bl_rgba_to_qcolor(_colors[i]));
        }
        break;
      }

      case kShapeRectPath: {
        for (i = 0; i < size; i++) {
          double x = _coords[i].x - halfSize;
          double y = _coords[i].y - halfSize;

          QPainterPath path;
          path.addRect(x, y, rectSize, rectSize);
          ctx.fillPath(path, bl_rgba_to_qcolor(_colors[i]));
        }
        break;
      }

      case kShapePolyPath: {
        for (i = 0; i < size; i++) {
          double x = _coords[i].x - halfSize;
          double y = _coords[i].y - halfSize;

          QPainterPath path;
          path.moveTo(x + rectSize / 2, y);
          path.lineTo(x + rectSize, y + rectSize / 3);
          path.lineTo(x + rectSize - rectSize / 3, y + rectSize);
          path.lineTo(x + rectSize / 3, y + rectSize);
          path.lineTo(x, y + rectSize / 3);
          ctx.fillPath(path, bl_rgba_to_qcolor(_colors[i]));
        }
        break;
      }

      case kShapeRoundRect: {
        for (i = 0; i < size; i++) {
          double x = _coords[i].x - halfSize;
          double y = _coords[i].y - halfSize;

          QPainterPath path;
          path.addRoundedRect(QRectF(x, y, rectSize, rectSize), 10, 10);
          ctx.fillPath(path, bl_rgba_to_qcolor(_colors[i]));
        }
        break;
      }
    }
  }

  void setCount(size_t size) {
    double w = _canvas.image_width();
    double h = _canvas.image_height();
    size_t i = _coords.size();

    _coords.resize(size);
    _steps.resize(size);
    _colors.resize(size);

    while (i < size) {
      _coords[i].reset(_random.next_double() * w,
                       _random.next_double() * h);
      _steps[i].reset((_random.next_double() * 0.5 + 0.04) * randomSign(),
                      (_random.next_double() * 0.5 + 0.04) * randomSign());
      _colors[i].reset(randomColor());
      i++;
    }
  }

  void _updateTitle() {
    char buf[256];
    snprintf(buf, 256, "Rects [%dx%d] [Size=%d Count=%zu] [RenderTime=%.2fms FPS=%.1f]",
      _canvas.image_width(),
      _canvas.image_height(),
      int(_rect_size),
      _coords.size(),
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

#include "bl_demo_rects.moc"
