#include <stdlib.h>
#include <cmath>
#include <vector>

#include "bl_qt_headers.h"
#include "bl_qt_canvas.h"

struct Particle {
  BLPoint p;
  BLPoint v;
  int age;
  int category;
};

class MainWindow : public QWidget {
  Q_OBJECT

public:
  QTimer _timer;
  QComboBox _renderer_select;
  QCheckBox _limit_fps_check;
  QCheckBox _colors_check_box;
  QSlider _count_slider;
  QSlider _rotation_slider;
  QBLCanvas _canvas;

  BLRandom _rnd;
  std::vector<Particle> _particles;

  bool _animate = true;
  int maxAge = 650;
  double radius_scale = 6;

  enum { kCategoryCount = 8 };
  BLRgba32 colors[kCategoryCount] = {
    BLRgba32(0xFF4F00FF),
    BLRgba32(0xFFFF004F),
    BLRgba32(0xFFFF7F00),
    BLRgba32(0xFFFF3F9F),
    BLRgba32(0xFF7F4FFF),
    BLRgba32(0xFFFF9F3F),
    BLRgba32(0xFFFFFF00),
    BLRgba32(0xFFAF3F00)
  };

  MainWindow() {
    QVBoxLayout* vBox = new QVBoxLayout();
    vBox->setContentsMargins(0, 0, 0, 0);
    vBox->setSpacing(0);

    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(5, 5, 5, 5);
    grid->setSpacing(5);

    QBLCanvas::init_renderer_select_box(&_renderer_select);
    _limit_fps_check.setText(QLatin1String("Limit FPS"));
    _colors_check_box.setText(QLatin1String("Colors"));

    _count_slider.setMinimum(0);
    _count_slider.setMaximum(5000);
    _count_slider.setValue(500);
    _count_slider.setOrientation(Qt::Horizontal);

    _rotation_slider.setMinimum(0);
    _rotation_slider.setMaximum(1000);
    _rotation_slider.setValue(100);
    _rotation_slider.setOrientation(Qt::Horizontal);

    connect(&_renderer_select, SIGNAL(activated(int)), SLOT(onRendererChanged(int)));
    connect(&_limit_fps_check, SIGNAL(stateChanged(int)), SLOT(onLimitFpsChanged(int)));

    grid->addWidget(new QLabel("Renderer:"), 0, 0);
    grid->addWidget(&_renderer_select, 0, 1);
    grid->addWidget(&_colors_check_box, 0, 2);
    grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 0, 3);
    grid->addWidget(&_limit_fps_check, 0, 4, Qt::AlignRight);

    grid->addWidget(new QLabel("Count:"), 1, 0, Qt::AlignRight);
    grid->addWidget(&_count_slider, 1, 1, 1, 5);

    grid->addWidget(new QLabel("Rotation:"), 2, 0, Qt::AlignRight);
    grid->addWidget(&_rotation_slider, 2, 1, 1, 5);

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
    _rnd.reset(1234);
    _limit_fps_check.setChecked(true);
    _updateTitle();
  }

  Q_SLOT void onToggleAnimate() { _animate = !_animate; }
  Q_SLOT void onRendererChanged(int index) { _canvas.set_renderer_type(_renderer_select.itemData(index).toInt()); }
  Q_SLOT void onLimitFpsChanged(int value) { _timer.setInterval(value ? 1000 / 120 : 0); }

  Q_SLOT void onTimer() {
    if (_animate) {
      size_t i = 0;
      size_t j = 0;
      size_t count = _particles.size();

      double rot = double(_rotation_slider.value()) * 0.02 / 1000;
      double PI = 3.14159265359;
      BLMatrix2D m = BLMatrix2D::make_rotation(rot);

      while (i < count) {
        Particle& p = _particles[i++];
        p.p += p.v;
        p.v = m.map_point(p.v);
        if (++p.age >= maxAge)
          continue;
        _particles[j++] = p;
      }
      _particles.resize(j);

      size_t maxParticles = size_t(_count_slider.value());
      size_t n = size_t(_rnd.next_double() * maxParticles / 60 + 0.95);

      for (i = 0; i < n; i++) {
        if (_particles.size() >= maxParticles)
          break;

        double angle = _rnd.next_double() * PI * 2.0;
        double speed = bl_max(_rnd.next_double() * 2.0, 0.05);
        double aSin = std::sin(angle);
        double aCos = std::cos(angle);

        Particle part;
        part.p.reset();
        part.v.reset(aCos * speed, aSin * speed);
        part.age = int(bl_min(_rnd.next_double(), 0.5) * maxAge);
        part.category = int(_rnd.next_double() * kCategoryCount);
        _particles.push_back(part);
      }
    }

    _canvas.update_canvas(true);
    _updateTitle();
  }

  void on_render_blend2d(BLContext& ctx) noexcept {
    ctx.fill_all(BLRgba32(0xFF000000u));

    double cx = _canvas.image_width() / 2;
    double cy = _canvas.image_height() / 2;

    if (_colors_check_box.isChecked()) {
      BLPath paths[kCategoryCount];

      for (Particle& part : _particles) {
        paths[part.category].add_circle(BLCircle(cx + part.p.x, cy + part.p.y, double(maxAge - part.age) / double(maxAge) * radius_scale));
      }

      ctx.set_comp_op(BL_COMP_OP_PLUS);
      for (size_t i = 0; i < kCategoryCount; i++) {
        ctx.fill_path(paths[i], colors[i]);
      }
    }
    else {
      BLPath path;
      for (Particle& part : _particles) {
        path.add_circle(BLCircle(cx + part.p.x, cy + part.p.y, double(maxAge - part.age) / double(maxAge) * radius_scale));
      }
      ctx.fill_path(path, BLRgba32(0xFFFFFFFFu));
    }
  }

  void on_render_qt(QPainter& ctx) noexcept {
    ctx.fillRect(0, 0, _canvas.image_width(), _canvas.image_height(), QColor(0, 0, 0));
    ctx.setRenderHint(QPainter::Antialiasing, true);

    double cx = _canvas.image_width() / 2;
    double cy = _canvas.image_height() / 2;

    if (_colors_check_box.isChecked()) {
      QPainterPath paths[kCategoryCount];

      for (Particle& part : _particles) {
        double r = double(maxAge - part.age) / double(maxAge) * radius_scale;
        double d = r * 2.0;
        paths[part.category].addEllipse(cx + part.p.x - r, cy + part.p.y - r, d, d);
      }

      ctx.setCompositionMode(QPainter::CompositionMode_Plus);
      for (size_t i = 0; i < kCategoryCount; i++) {
        paths[i].setFillRule(Qt::WindingFill);
        ctx.fillPath(paths[i], QBrush(bl_rgba_to_qcolor(colors[i])));
      }
    }
    else {
      QPainterPath p;
      p.setFillRule(Qt::WindingFill);

      for (Particle& part : _particles) {
        double r = double(maxAge - part.age) / double(maxAge) * radius_scale;
        double d = r * 2.0;
        p.addEllipse(cx + part.p.x - r, cy + part.p.y - r, d, d);
      }

      ctx.fillPath(p, QBrush(QColor(qRgb(255, 255, 255))));
    }
  }

  void _updateTitle() {
    char buf[256];
    snprintf(buf, 256, "Particles [%dx%d] [Count=%d] [RenderTime=%.2fms FPS=%.1f]",
      _canvas.image_width(),
      _canvas.image_height(),
      int(_particles.size()),
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

#include "bl_demo_particles.moc"
