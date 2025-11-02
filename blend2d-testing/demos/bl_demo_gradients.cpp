#include <blend2d/blend2d.h>
#include <blend2d-testing/demos/bl_qt_headers.h>
#include <blend2d-testing/demos/bl_qt_canvas.h>

class MainWindow : public QWidget {
  Q_OBJECT
public:
  // Widgets.
  QComboBox _renderer_select;
  QComboBox _gradient_type_select;
  QComboBox _extend_mode_select;
  QSlider _parameter_slider1;
  QSlider _parameter_slider2;
  QLabel _parameter_label1;
  QLabel _parameter_label2;
  QSlider _stop_sliders[9];
  QCheckBox _control_check_box;
  QCheckBox _dither_check_box;
  QBLCanvas _canvas;

  // Canvas data.
  BLPoint _pts[2] {};
  BLGradientType _gradient_type = BL_GRADIENT_TYPE_LINEAR;
  BLExtendMode _gradient_extend_mode = BL_EXTEND_MODE_PAD;
  size_t _num_points = 2;
  size_t _closest_vertex = SIZE_MAX;
  size_t _grabbed_vertex = SIZE_MAX;
  int _grabbed_x = 0;
  int _grabbed_y = 0;

  MainWindow() {
    setWindowTitle(QLatin1String("Gradients"));

    QVBoxLayout* vBox = new QVBoxLayout();
    vBox->setContentsMargins(0, 0, 0, 0);
    vBox->setSpacing(0);

    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(5, 5, 5, 5);
    grid->setSpacing(5);

    QBLCanvas::init_renderer_select_box(&_renderer_select);
    connect(&_renderer_select, SIGNAL(activated(int)), SLOT(onRendererChanged(int)));

    _gradient_type_select.addItem("Linear", QVariant(int(BL_GRADIENT_TYPE_LINEAR)));
    _gradient_type_select.addItem("Radial", QVariant(int(BL_GRADIENT_TYPE_RADIAL)));
    _gradient_type_select.addItem("Conic", QVariant(int(BL_GRADIENT_TYPE_CONIC)));
    connect(&_gradient_type_select, SIGNAL(activated(int)), SLOT(onGradientTypeChanged(int)));

    _extend_mode_select.addItem("Pad", QVariant(int(BL_EXTEND_MODE_PAD)));
    _extend_mode_select.addItem("Repeat", QVariant(int(BL_EXTEND_MODE_REPEAT)));
    _extend_mode_select.addItem("Reflect", QVariant(int(BL_EXTEND_MODE_REFLECT)));
    connect(&_extend_mode_select, SIGNAL(activated(int)), SLOT(onExtendModeChanged(int)));

    _parameter_slider1.setOrientation(Qt::Horizontal);
    _parameter_slider1.setMinimum(0);
    _parameter_slider1.setMaximum(720);
    _parameter_slider1.setSliderPosition(360);
    connect(&_parameter_slider1, SIGNAL(valueChanged(int)), SLOT(onParameterChanged(int)));

    _parameter_slider2.setOrientation(Qt::Horizontal);
    _parameter_slider2.setMinimum(0);
    _parameter_slider2.setMaximum(720);
    _parameter_slider2.setSliderPosition(0);
    connect(&_parameter_slider2, SIGNAL(valueChanged(int)), SLOT(onParameterChanged(int)));

    _control_check_box.setText("Control");
    _control_check_box.setChecked(true);
    connect(&_control_check_box, SIGNAL(stateChanged(int)), SLOT(onParameterChanged(int)));

    _dither_check_box.setText("Dither");
    connect(&_dither_check_box, SIGNAL(stateChanged(int)), SLOT(onParameterChanged(int)));

    const uint32_t initialColors[3] = {
      0xFF000000,
      0xFFFF0000,
      0xFFFFFFFF
    };

    for (uint32_t stop_id = 0; stop_id < 3; stop_id++) {
      uint32_t color = initialColors[stop_id];
      for (uint32_t component = 0; component < 3; component++, color <<= 8) {
        uint32_t sliderId = stop_id * 3 + component;
        _stop_sliders[sliderId].setOrientation(Qt::Horizontal);
        _stop_sliders[sliderId].setMinimum(0);
        _stop_sliders[sliderId].setMaximum(255);
        _stop_sliders[sliderId].setSliderPosition((color >> 16) & 0xFF);

        static const char channels[] = "R:\0G:\0B:\0";
        grid->addWidget(new QLabel(QString::fromLatin1(channels + component * 3, 2)), component, stop_id * 2 + 2);
        grid->addWidget(&_stop_sliders[sliderId], component, stop_id * 2 + 3);
        connect(&_stop_sliders[sliderId], SIGNAL(valueChanged(int)), SLOT(onParameterChanged(int)));
      }
    }

    QPushButton* randomizeButton = new QPushButton("Random");
    connect(randomizeButton, SIGNAL(clicked()), SLOT(onRandomizeVertices()));

    grid->addWidget(new QLabel("Renderer:"), 0, 0, Qt::AlignRight);
    grid->addWidget(&_renderer_select, 0, 1);
    grid->addWidget(new QLabel("Gradient:"), 1, 0, Qt::AlignRight);
    grid->addWidget(&_gradient_type_select, 1, 1);
    grid->addItem(new QSpacerItem(0, 10), 0, 2);
    grid->addWidget(new QLabel("Extend Mode:"), 2, 0);
    grid->addWidget(&_extend_mode_select, 2, 1);

    grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 0, 7);
    grid->addWidget(randomizeButton, 0, 8);

    grid->addWidget(&_parameter_label1, 3, 0, Qt::AlignRight);
    grid->addWidget(&_parameter_slider1, 3, 1, 1, 8);

    grid->addWidget(&_parameter_label2, 4, 0, Qt::AlignRight);
    grid->addWidget(&_parameter_slider2, 4, 1, 1, 8);

    grid->addWidget(&_control_check_box, 1, 8);
    grid->addWidget(&_dither_check_box, 2, 8);

    vBox->addItem(grid);
    vBox->addWidget(&_canvas);
    setLayout(vBox);

    _canvas.on_render_blend2d = std::bind(&MainWindow::on_render_blend2d, this, std::placeholders::_1);
    _canvas.on_render_qt = std::bind(&MainWindow::on_render_qt, this, std::placeholders::_1);
    _canvas.on_mouse_event = std::bind(&MainWindow::on_mouse_event, this, std::placeholders::_1);

    onInit();
    updateLabels();
  }

  void updateLabels() {
    switch (_gradient_type) {
      case BL_GRADIENT_TYPE_LINEAR:
        _parameter_label1.setText("(Unused)");
        _parameter_label2.setText("(Unused)");
        break;

      case BL_GRADIENT_TYPE_RADIAL:
        _parameter_label1.setText("Center Rad");
        _parameter_label2.setText("Focal Rad");
        break;

      case BL_GRADIENT_TYPE_CONIC:
        _parameter_label1.setText("Angle");
        _parameter_label2.setText("Repeat");
        break;
    }
  }

  void keyPressEvent(QKeyEvent *event) override {}

  void onInit() {
    _pts[0].reset(350, 300);
    _pts[1].reset(200, 150);
  }

  size_t getClosestVertex(BLPoint p, double maxDistance) noexcept {
    size_t closestIndex = SIZE_MAX;
    double closestDistance = std::numeric_limits<double>::max();
    for (size_t i = 0; i < _num_points; i++) {
      double d = std::hypot(_pts[i].x - p.x, _pts[i].y - p.y);
      if (d < closestDistance && d < maxDistance) {
        closestIndex = i;
        closestDistance = d;
      }
    }
    return closestIndex;
  }

  double sliderAngle(double scale) {
    return double(_parameter_slider1.value()) / 720.0 * scale;
  }

  void on_mouse_event(QMouseEvent* event) {
    double mx = event->position().x() * devicePixelRatio();
    double my = event->position().y() * devicePixelRatio();

    if (event->type() == QEvent::MouseButtonPress) {
      if (event->button() == Qt::LeftButton) {
        if (_closest_vertex != SIZE_MAX) {
          _grabbed_vertex = _closest_vertex;
          _grabbed_x = mx;
          _grabbed_y = my;
          _canvas.update_canvas();
        }
      }
    }

    if (event->type() == QEvent::MouseButtonRelease) {
      if (event->button() == Qt::LeftButton) {
        if (_grabbed_vertex != SIZE_MAX) {
          _grabbed_vertex = SIZE_MAX;
          _canvas.update_canvas();
        }
      }
    }

    if (event->type() == QEvent::MouseMove) {
      if (_grabbed_vertex == SIZE_MAX) {
        _closest_vertex = getClosestVertex(BLPoint(mx, my), 5);
        _canvas.update_canvas();
      }
      else {
        _pts[_grabbed_vertex] = BLPoint(mx, my);
        _canvas.update_canvas();
      }
    }
  }

  Q_SLOT void onRendererChanged(int index) {
    _canvas.set_renderer_type(_renderer_select.itemData(index).toInt());
  }

  Q_SLOT void onGradientTypeChanged(int index) {
    _num_points = index == BL_GRADIENT_TYPE_CONIC ? 1 : 2;
    _gradient_type = BLGradientType(index);
    updateLabels();
    _canvas.update_canvas();
  }

  Q_SLOT void onExtendModeChanged(int index) {
    _gradient_extend_mode = (BLExtendMode)index;
    _canvas.update_canvas();
  }

  Q_SLOT void onParameterChanged(int value) {
    _canvas.update_canvas();
  }

  Q_SLOT void onRandomizeVertices() {
    _pts[0].x = (double(rand() % 65536) / 65535.0) * (_canvas.image_width()  - 1) + 0.5;
    _pts[0].y = (double(rand() % 65536) / 65535.0) * (_canvas.image_height() - 1) + 0.5;
    _pts[1].x = (double(rand() % 65536) / 65535.0) * (_canvas.image_width()  - 1) + 0.5;
    _pts[1].y = (double(rand() % 65536) / 65535.0) * (_canvas.image_height() - 1) + 0.5;
    _canvas.update_canvas();
  }

  void on_render_blend2d(BLContext& ctx) noexcept {
    if (_dither_check_box.isChecked()) {
      ctx.set_gradient_quality(BL_GRADIENT_QUALITY_DITHER);
    }

    BLGradient gradient;
    gradient.set_type(_gradient_type);
    gradient.set_extend_mode(_gradient_extend_mode);
    gradient.reset_stops();

    constexpr double offsets[] = { 0.0, 0.5, 1.0 };
    for (uint32_t stop_id = 0; stop_id < 3; stop_id++) {
      uint32_t r = uint32_t(_stop_sliders[stop_id * 3 + 0].value());
      uint32_t g = uint32_t(_stop_sliders[stop_id * 3 + 1].value());
      uint32_t b = uint32_t(_stop_sliders[stop_id * 3 + 2].value());
      gradient.add_stop(offsets[stop_id], BLRgba32(r, g, b));
    }

    if (_gradient_type == BL_GRADIENT_TYPE_LINEAR) {
      gradient.set_values(BLLinearGradientValues(_pts[0].x, _pts[0].y, _pts[1].x, _pts[1].y));
    }
    else if (_gradient_type == BL_GRADIENT_TYPE_RADIAL) {
      double s1 = double(_parameter_slider1.value());
      double s2 = double(_parameter_slider2.value());
      gradient.set_values(BLRadialGradientValues(_pts[0].x, _pts[0].y, _pts[1].x, _pts[1].y, s1, s2));
    }
    else {
      double a = sliderAngle(3.14159265358979323846 * 2.0);
      double s2 = double(_parameter_slider2.value());
      gradient.set_values(BLConicGradientValues(_pts[0].x, _pts[0].y, a, s2 / 100.0 + 1.0));
    }

    ctx.fill_all(gradient);

    if (_control_check_box.isChecked()) {
      for (size_t i = 0; i < _num_points; i++) {
        ctx.stroke_circle(_pts[i].x, _pts[i].y, 3, i == _closest_vertex ? BLRgba32(0xFF00FFFFu) : BLRgba32(0xFF007FFFu));
      }
    }
  }

  void on_render_qt(QPainter& ctx) noexcept {
    ctx.fillRect(0, 0, _canvas.image_width(), _canvas.image_height(), QColor(255, 0, 0));
    ctx.setRenderHint(QPainter::Antialiasing, true);

    QGradientStops stops;
    const double offsets[] = { 0.0, 0.5, 1.0 };

    for (uint32_t stop_id = 0; stop_id < 3; stop_id++) {
      uint32_t r = uint32_t(_stop_sliders[stop_id * 3 + 0].value());
      uint32_t g = uint32_t(_stop_sliders[stop_id * 3 + 1].value());
      uint32_t b = uint32_t(_stop_sliders[stop_id * 3 + 2].value());
      stops.append(QGradientStop(qreal(offsets[stop_id]), QColor(qRgb(r, g, b))));
    }

    QGradient::Spread spread = QGradient::PadSpread;
    if (_gradient_extend_mode == BL_EXTEND_MODE_REPEAT) spread = QGradient::RepeatSpread;
    if (_gradient_extend_mode == BL_EXTEND_MODE_REFLECT) spread = QGradient::ReflectSpread;

    switch (_gradient_type) {
      case BL_GRADIENT_TYPE_LINEAR: {
        QLinearGradient g(_pts[0].x, _pts[0].y, _pts[1].x, _pts[1].y);
        g.setStops(stops);
        g.setSpread(spread);
        ctx.fillRect(0, 0, _canvas.image_width(), _canvas.image_height(), QBrush(g));
        break;
      }

      case BL_GRADIENT_TYPE_RADIAL: {
        QRadialGradient g(
          qreal(_pts[0].x), qreal(_pts[0].y), qreal(_parameter_slider1.value()),
          qreal(_pts[1].x), qreal(_pts[1].y), qreal(_parameter_slider2.value()));
        g.setStops(stops);
        g.setSpread(spread);
        ctx.fillRect(0, 0, _canvas.image_width(), _canvas.image_height(), QBrush(g));
        break;
      }

      case BL_GRADIENT_TYPE_CONIC: {
        QConicalGradient g(_pts[0].x, _pts[0].y, sliderAngle(360.0));
        g.setSpread(spread);
        g.setStops(stops);
        ctx.fillRect(0, 0, _canvas.image_width(), _canvas.image_height(), QBrush(g));
        break;
      }
    }

    if (_control_check_box.isChecked()) {
      for (size_t i = 0; i < _num_points; i++) {
        QColor color = bl_rgba_to_qcolor(i == _closest_vertex ? BLRgba32(0xFF00FFFFu) : BLRgba32(0xFF007FFFu));
        ctx.setPen(QPen(color, 1.0f));
        ctx.setBrush(QBrush());
        ctx.drawEllipse(QPointF(_pts[i].x, _pts[i].y), 3, 3);
      }
    }
  }
};

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  MainWindow win;

  win.setMinimumSize(QSize(700, 650));
  win.show();

  return app.exec();
}

#include "bl_demo_gradients.moc"
