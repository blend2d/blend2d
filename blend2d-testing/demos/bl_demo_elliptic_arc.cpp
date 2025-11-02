#include <blend2d/blend2d.h>
#include <blend2d-testing/demos/bl_qt_headers.h>
#include <blend2d-testing/demos/bl_qt_canvas.h>

class MainWindow : public QWidget {
  Q_OBJECT
public:
  // Widgets.
  QSlider _x_radius_slider;
  QSlider _y_radius_slider;
  QSlider _angle_slider;
  QCheckBox _large_arc_flag;
  QCheckBox _sweep_arc_flag;
  QLabel _bottom_text;
  QBLCanvas _canvas;

  // Canvas data.
  BLGradient _gradient;
  BLPoint _pts[2] {};
  size_t _num_points = 2;
  size_t _closest_vertex = SIZE_MAX;
  size_t _grabbed_vertex = SIZE_MAX;
  int _grabbed_x = 0;
  int _grabbed_y = 0;

  MainWindow() {
    setWindowTitle(QLatin1String("Elliptic Arcs"));

    QVBoxLayout* vBox = new QVBoxLayout();
    vBox->setContentsMargins(0, 0, 0, 0);
    vBox->setSpacing(0);

    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(5, 5, 5, 5);
    grid->setSpacing(5);

    _x_radius_slider.setOrientation(Qt::Horizontal);
    _x_radius_slider.setMinimum(1);
    _x_radius_slider.setMaximum(500);
    _x_radius_slider.setSliderPosition(131);

    _y_radius_slider.setOrientation(Qt::Horizontal);
    _y_radius_slider.setMinimum(1);
    _y_radius_slider.setMaximum(500);
    _y_radius_slider.setSliderPosition(143);

    _angle_slider.setOrientation(Qt::Horizontal);
    _angle_slider.setMinimum(-360);
    _angle_slider.setMaximum(360);
    _angle_slider.setSliderPosition(0);

    _large_arc_flag.setText(QLatin1String("Large Arc Flag"));
    _sweep_arc_flag.setText(QLatin1String("Sweep Arc Flag"));

    _bottom_text.setTextInteractionFlags(Qt::TextSelectableByMouse);
    _bottom_text.setMargin(5);

    connect(&_x_radius_slider, SIGNAL(valueChanged(int)), SLOT(onParameterChanged(int)));
    connect(&_y_radius_slider, SIGNAL(valueChanged(int)), SLOT(onParameterChanged(int)));
    connect(&_angle_slider, SIGNAL(valueChanged(int)), SLOT(onParameterChanged(int)));
    connect(&_large_arc_flag, SIGNAL(stateChanged(int)), SLOT(onParameterChanged(int)));
    connect(&_sweep_arc_flag, SIGNAL(stateChanged(int)), SLOT(onParameterChanged(int)));

    _canvas.on_render_blend2d = std::bind(&MainWindow::onRender, this, std::placeholders::_1);
    _canvas.on_mouse_event = std::bind(&MainWindow::on_mouse_event, this, std::placeholders::_1);

    grid->addWidget(new QLabel("X Radius:"), 0, 0, Qt::AlignRight);
    grid->addWidget(&_x_radius_slider, 0, 1);
    grid->addWidget(&_large_arc_flag, 0, 2);

    grid->addWidget(new QLabel("Y Radius:"), 1, 0, Qt::AlignRight);
    grid->addWidget(&_y_radius_slider, 1, 1);
    grid->addWidget(&_sweep_arc_flag, 1, 2);

    grid->setSpacing(5);
    grid->addWidget(new QLabel("Angle:"), 2, 0, Qt::AlignRight);
    grid->addWidget(&_angle_slider, 2, 1, 1, 2);

    vBox->addItem(grid);
    vBox->addWidget(&_canvas);
    vBox->addWidget(&_bottom_text);
    setLayout(vBox);

    onInit();
  }

  void keyPressEvent(QKeyEvent *event) override {}

  void onInit() {
    _pts[0].reset(124, 180);
    _pts[1].reset(296, 284);
    _gradient.add_stop(0.0, BLRgba32(0xFF000000u));
    _gradient.add_stop(1.0, BLRgba32(0xFFFFFFFFu));
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

  void on_mouse_event(QMouseEvent* event) {
    double mx = event->position().x();
    double my = event->position().y();

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

  Q_SLOT void onParameterChanged(int value) {
    _canvas.update_canvas();
  }

  void onRender(BLContext& ctx) {
    ctx.fill_all(BLRgba32(0xFF000000u));

    BLPoint radius(_x_radius_slider.value(), _y_radius_slider.value());
    BLPoint start(_pts[0]);
    BLPoint end(_pts[1]);

    double PI = 3.14159265359;
    double angle = (double(_angle_slider.value()) / 180.0) * PI;

    bool largeArcFlag = _large_arc_flag.isChecked();
    bool sweepArcFlag = _sweep_arc_flag.isChecked();

    // Render all arcs before rendering the one that is selected.
    BLPath p;
    p.move_to(start);
    p.elliptic_arc_to(radius, angle, false, false, end);
    p.move_to(start);
    p.elliptic_arc_to(radius, angle, false, true, end);
    p.move_to(start);
    p.elliptic_arc_to(radius, angle, true, false, end);
    p.move_to(start);
    p.elliptic_arc_to(radius, angle, true, true, end);
    ctx.stroke_path(p, BLRgba32(0x40FFFFFFu));

    // Render elliptic arc based on the given parameters.
    p.clear();
    p.move_to(start);
    p.elliptic_arc_to(radius, angle, largeArcFlag, sweepArcFlag, end);
    ctx.stroke_path(p, BLRgba32(0xFFFFFFFFu));

    // Render all points of the path (as the arc was split into segments).
    renderPathPoints(ctx, p, BLRgba32(0xFF808080));

    // Render the rest of the UI (draggable points).
    for (size_t i = 0; i < _num_points; i++) {
      ctx.fill_circle(_pts[i].x, _pts[i].y, 2.5, i == _closest_vertex ? BLRgba32(0xFF00FFFFu) : BLRgba32(0xFF007FFFu));
    }

    char buf[256];
    snprintf(buf, 256, "<path d=\"M%g %g A%g %g %g %d %d %g %g\" />", start.x, start.y, radius.x, radius.y, angle / PI * 180, largeArcFlag, sweepArcFlag, end.x, end.y);
    _bottom_text.setText(QString::fromUtf8(buf));
  }

  void renderPathPoints(BLContext& ctx, const BLPath& path, BLRgba32 color) noexcept {
    size_t count = path.size();
    const BLPoint* vtx = path.vertex_data();

    ctx.set_fill_style(color);
    for (size_t i = 0; i < count; i++) {
      if (!std::isfinite(vtx[i].x))
        continue;
      ctx.fill_circle(vtx[i].x, vtx[i].y, 2.0);
    }
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

#include "bl_demo_elliptic_arc.moc"
