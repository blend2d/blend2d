#include <blend2d/blend2d.h>
#include <blend2d-testing/demos/bl_qt_headers.h>
#include <blend2d-testing/demos/bl_qt_canvas.h>

#include <stdlib.h>
#include <cmath>
#include <vector>

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
  QComboBox _comp_op_select;
  QCheckBox _limit_fps_check;
  QCheckBox _colors_check_box;
  QSlider _count_slider;
  QSlider _radius_slider;
  QSlider _rotation_slider;
  QBLCanvas _canvas;

  BLRandom _rnd;
  std::vector<Particle> _particles;

  bool _animate = true;
  int _max_age = 650;

  static inline constexpr uint32_t kCategoryCount = 10;

  BLRgba32 colors[kCategoryCount] = {
    BLRgba32(0xFF4F00FF),
    BLRgba32(0xFF9F00DF),
    BLRgba32(0xFFFF004F),
    BLRgba32(0xFFFF7F00),
    BLRgba32(0xFFFF3F9F),
    BLRgba32(0xFF7F4FFF),
    BLRgba32(0xFFFF9F3F),
    BLRgba32(0xFFFFFF00),
    BLRgba32(0xFFAF3F00),
    BLRgba32(0xFFFF1F00)
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

    _comp_op_select.addItem("Plus", QVariant(int(BL_COMP_OP_PLUS)));
    _comp_op_select.addItem("Screen", QVariant(int(BL_COMP_OP_SCREEN)));
    _comp_op_select.addItem("Lighten", QVariant(int(BL_COMP_OP_LIGHTEN)));
    _comp_op_select.addItem("Src Atop", QVariant(int(BL_COMP_OP_SRC_ATOP)));
    _comp_op_select.addItem("Hard Light", QVariant(int(BL_COMP_OP_HARD_LIGHT)));
    _comp_op_select.addItem("Difference", QVariant(int(BL_COMP_OP_DIFFERENCE)));
    _comp_op_select.addItem("Exclusion", QVariant(int(BL_COMP_OP_EXCLUSION)));
    _comp_op_select.setCurrentIndex(0);

    _count_slider.setMinimum(0);
    _count_slider.setMaximum(5000);
    _count_slider.setValue(500);
    _count_slider.setOrientation(Qt::Horizontal);

    _radius_slider.setMinimum(10);
    _radius_slider.setMaximum(100);
    _radius_slider.setValue(60);
    _radius_slider.setOrientation(Qt::Horizontal);

    _rotation_slider.setMinimum(0);
    _rotation_slider.setMaximum(1000);
    _rotation_slider.setValue(100);
    _rotation_slider.setOrientation(Qt::Horizontal);

    connect(&_renderer_select, SIGNAL(activated(int)), SLOT(on_renderer_changed(int)));
    connect(&_limit_fps_check, SIGNAL(stateChanged(int)), SLOT(on_limit_fps_changed(int)));

    grid->addWidget(new QLabel("Renderer:"), 0, 0);
    grid->addWidget(&_renderer_select, 0, 1);

    grid->addWidget(new QLabel("Comp Op:"), 0, 2);
    grid->addWidget(&_comp_op_select, 0, 3);

    grid->addWidget(&_colors_check_box, 0, 4);
    grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 0, 5);
    grid->addWidget(&_limit_fps_check, 0, 6, Qt::AlignRight);

    grid->addWidget(new QLabel("Count:"), 1, 0, Qt::AlignRight);
    grid->addWidget(&_count_slider, 1, 1, 1, 7);

    grid->addWidget(new QLabel("Max Radius:"), 2, 0, Qt::AlignRight);
    grid->addWidget(&_radius_slider, 2, 1, 1, 7);

    grid->addWidget(new QLabel("Rotation:"), 3, 0, Qt::AlignRight);
    grid->addWidget(&_rotation_slider, 3, 1, 1, 7);

    _canvas.on_render_blend2d = std::bind(&MainWindow::on_render_blend2d, this, std::placeholders::_1);
    _canvas.on_render_qt = std::bind(&MainWindow::on_render_qt, this, std::placeholders::_1);

    vBox->addItem(grid);
    vBox->addWidget(&_canvas);
    setLayout(vBox);

    connect(&_timer, SIGNAL(timeout()), this, SLOT(on_timer()));
    connect(new QShortcut(QKeySequence(Qt::Key_P), this), SIGNAL(activated()), SLOT(on_toggle_animate()));

    init_data();
  }

  void showEvent(QShowEvent* event) override { _timer.start(); }
  void hideEvent(QHideEvent* event) override { _timer.stop(); }
  void keyPressEvent(QKeyEvent* event) override {}

  void init_data() {
    _rnd.reset(1234);
    _limit_fps_check.setChecked(true);
    _update_window_title();
  }

  double radius_scale() const { return double(_radius_slider.value()) / 10.0; }

  Q_SLOT void on_toggle_animate() { _animate = !_animate; }
  Q_SLOT void on_renderer_changed(int index) { _canvas.set_renderer_type(_renderer_select.itemData(index).toInt()); }
  Q_SLOT void on_limit_fps_changed(int value) { _timer.setInterval(value ? 1000 / 120 : 0); }

  Q_SLOT void on_timer() {
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
        if (++p.age >= _max_age)
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
        double as = std::sin(angle);
        double ac = std::cos(angle);

        Particle part;
        part.p.reset();
        part.v.reset(ac * speed, as * speed);
        part.age = int(bl_min(_rnd.next_double(), 0.5) * _max_age);
        part.category = int(_rnd.next_double() * kCategoryCount);
        _particles.push_back(part);
      }
    }

    _canvas.update_canvas(true);
    _update_window_title();
  }

  void on_render_blend2d(BLContext& ctx) noexcept {
    ctx.fill_all(BLRgba32(0xFF000000u));

    BLCompOp comp_op = BLCompOp(_comp_op_select.currentData().toInt());

    double rs = radius_scale();
    double cx = _canvas.image_width() / 2;
    double cy = _canvas.image_height() / 2;

    if (_colors_check_box.isChecked()) {
      BLPath paths[kCategoryCount];

      for (Particle& part : _particles) {
        paths[part.category].add_circle(BLCircle(cx + part.p.x, cy + part.p.y, double(_max_age - part.age) / double(_max_age) * rs));
      }

      ctx.set_comp_op(comp_op);
      for (size_t i = 0; i < kCategoryCount; i++) {
        ctx.fill_path(paths[i], colors[i]);
      }
    }
    else {
      BLPath path;
      for (Particle& part : _particles) {
        path.add_circle(BLCircle(cx + part.p.x, cy + part.p.y, double(_max_age - part.age) / double(_max_age) * rs));
      }
      ctx.fill_path(path, BLRgba32(0xFFFFFFFFu));
    }
  }

  void on_render_qt(QPainter& ctx) noexcept {
    ctx.fillRect(0, 0, _canvas.image_width(), _canvas.image_height(), QColor(0, 0, 0));
    ctx.setRenderHint(QPainter::Antialiasing, true);

    QPainter::CompositionMode comp_op = bl_comp_op_to_qt_composition_mode(BLCompOp(_comp_op_select.currentData().toInt()));

    double rs = radius_scale();
    double cx = _canvas.image_width() / 2;
    double cy = _canvas.image_height() / 2;

    if (_colors_check_box.isChecked()) {
      QPainterPath paths[kCategoryCount];

      for (Particle& part : _particles) {
        double r = double(_max_age - part.age) / double(_max_age) * rs;
        double d = r * 2.0;
        paths[part.category].addEllipse(cx + part.p.x - r, cy + part.p.y - r, d, d);
      }

      ctx.setCompositionMode(comp_op);
      for (size_t i = 0; i < kCategoryCount; i++) {
        paths[i].setFillRule(Qt::WindingFill);
        ctx.fillPath(paths[i], QBrush(bl_rgba_to_qcolor(colors[i])));
      }
    }
    else {
      QPainterPath p;
      p.setFillRule(Qt::WindingFill);

      for (Particle& part : _particles) {
        double r = double(_max_age - part.age) / double(_max_age) * rs;
        double d = r * 2.0;
        p.addEllipse(cx + part.p.x - r, cy + part.p.y - r, d, d);
      }

      ctx.fillPath(p, QBrush(QColor(qRgb(255, 255, 255))));
    }
  }

  void _update_window_title() {
    char buf[256];
    snprintf(buf, 256, "bl_demo_particles [%dx%d] [N=%zu] [RenderTime=%.2fms FPS=%.1f]",
      _canvas.image_width(),
      _canvas.image_height(),
      _particles.size(),
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
