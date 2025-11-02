#include <blend2d/blend2d.h>
#include <blend2d-testing/demos/bl_qt_headers.h>
#include <blend2d-testing/demos/bl_qt_canvas.h>

#include <stdlib.h>
#include <cmath>
#include <vector>

class MainWindow : public QWidget {
  Q_OBJECT

public:
  QTimer _timer;
  QComboBox _renderer_select;
  QCheckBox _limit_fps_check;
  QSlider _count_slider;
  QBLCanvas _canvas;

  bool _animate = true;
  double _angle {};
  int _count {};

  MainWindow() {
    QVBoxLayout* vBox = new QVBoxLayout();
    vBox->setContentsMargins(0, 0, 0, 0);
    vBox->setSpacing(0);

    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(5, 5, 5, 5);
    grid->setSpacing(5);

    QBLCanvas::init_renderer_select_box(&_renderer_select);
    _limit_fps_check.setText(QLatin1String("Limit FPS"));

    _count_slider.setMinimum(100);
    _count_slider.setMaximum(2000);
    _count_slider.setValue(500);
    _count_slider.setOrientation(Qt::Horizontal);

    connect(&_renderer_select, SIGNAL(activated(int)), SLOT(onRendererChanged(int)));
    connect(&_limit_fps_check, SIGNAL(stateChanged(int)), SLOT(onLimitFpsChanged(int)));

    grid->addWidget(new QLabel("Renderer:"), 0, 0);
    grid->addWidget(&_renderer_select, 0, 1);

    grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 0, 2);
    grid->addWidget(&_limit_fps_check, 0, 3, Qt::AlignRight);

    grid->addWidget(new QLabel("Count:"), 1, 0, Qt::AlignRight);
    grid->addWidget(&_count_slider, 1, 1, 1, 4);

    _canvas.on_render_blend2d = std::bind(&MainWindow::on_render_blend2d, this, std::placeholders::_1);
    _canvas.on_render_qt = std::bind(&MainWindow::on_render_qt, this, std::placeholders::_1);

    vBox->addItem(grid);
    vBox->addWidget(&_canvas);
    setLayout(vBox);

    connect(&_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
    connect(new QShortcut(QKeySequence(Qt::Key_P), this), SIGNAL(activated()), SLOT(onToggleAnimate()));

    onInit();
  }

  void showEvent(QShowEvent* event) override { _timer.start(); }
  void hideEvent(QHideEvent* event) override { _timer.stop(); }
  void keyPressEvent(QKeyEvent* event) override {}

  void onInit() {
    _angle = 0;
    _count = 0;
    _limit_fps_check.setChecked(true);
    _updateTitle();
  }

  Q_SLOT void onToggleAnimate() { _animate = !_animate; }
  Q_SLOT void onRendererChanged(int index) { _canvas.set_renderer_type(_renderer_select.itemData(index).toInt()); }
  Q_SLOT void onLimitFpsChanged(int value) { _timer.setInterval(value ? 1000 / 120 : 0); }

  Q_SLOT void onTimer() {
    if (_animate) {
      _angle += 0.05;
      if (_angle >= 360)
        _angle -= 360;
    }

    _canvas.update_canvas(true);
    _updateTitle();
  }

  // The idea is based on:
  //   https://github.com/fogleman/gg/blob/master/examples/spiral.go

  void on_render_blend2d(BLContext& ctx) noexcept {
    ctx.fill_all(BLRgba32(0xFF000000u));

    BLPath p;

    int count = _count_slider.value();
    double PI = 3.14159265359;

    double cx = _canvas.image_width() / 2;
    double cy = _canvas.image_height() / 2;
    double maxDist = 1000.0;
    double baseAngle = _angle / 180.0 * PI;

    for (int i = 0; i < count; i++) {
      double t = double(i) * 1.01 / 1000;
      double d = t * 1000.0 * 0.4 + 10;
      double a = baseAngle + t * PI * 2 * 20;
      double x = cx + std::cos(a) * d;
      double y = cy + std::sin(a) * d;
      double r = std::min(t * 8 + 0.5, 10.0);
      p.add_circle(BLCircle(x, y, r));
    }

    ctx.fill_path(p, BLRgba32(0xFFFFFFFFu));
  }

  void on_render_qt(QPainter& ctx) noexcept {
    ctx.fillRect(0, 0, _canvas.image_width(), _canvas.image_height(), QColor(0, 0, 0));
    ctx.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath p;
    QBrush brush(QColor(qRgb(255, 255, 255)));

    int count = _count_slider.value();
    double PI = 3.14159265359;

    double cx = _canvas.image_width() / 2;
    double cy = _canvas.image_height() / 2;
    double baseAngle = _angle / 180.0 * PI;

    for (int i = 0; i < count; i++) {
      double t = double(i) * 1.01 / 1000;
      double d = t * 1000.0 * 0.4 + 10;
      double a = baseAngle + t * PI * 2 * 20;
      double x = cx + std::cos(a) * d;
      double y = cy + std::sin(a) * d;
      double r = std::min(t * 8 + 0.5, 10.0);
      p.addEllipse(x - r, y - r, r * 2.0, r * 2.0);
    }

    ctx.fillPath(p, brush);
  }

  void _updateTitle() {
    char buf[256];
    snprintf(buf, 256, "Circles [%dx%d] [Count=%d] [RenderTime=%.2fms FPS=%.1f]",
      _canvas.image_width(),
      _canvas.image_height(),
      _count_slider.value(),
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

#include "bl_demo_circles.moc"
