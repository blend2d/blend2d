#include <stdlib.h>
#include <cmath>
#include <vector>

#include "bl_qt_headers.h"
#include "bl_qt_canvas.h"

struct Bubble {
  BLPoint p;
  double r;
  double x;
  double a;
  double b;
  double c;
  BLRgba32 colors[2];
};

static BL_INLINE BLRgba32 random_rgba32(BLRandom& rnd) noexcept {
  return BLRgba32(rnd.next_uint32() | 0x55000000u);
}

class MainWindow : public QWidget {
  Q_OBJECT

public:
  QTimer _timer;
  QComboBox _renderer_select;
  QCheckBox _limit_fps_check;
  QSlider _count_slider;
  QSlider _parameter_slider;
  QBLCanvas _canvas;

  BLRandom _rnd;
  std::vector<Bubble> _bubbles;

  bool _animate = true;

  MainWindow() {
    QVBoxLayout* vBox = new QVBoxLayout();
    vBox->setContentsMargins(0, 0, 0, 0);
    vBox->setSpacing(0);

    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(5, 5, 5, 5);
    grid->setSpacing(5);

    QBLCanvas::init_renderer_select_box(&_renderer_select);
    _limit_fps_check.setText(QLatin1String("Limit FPS"));

    _count_slider.setMinimum(1);
    _count_slider.setMaximum(5000);
    _count_slider.setValue(100);
    _count_slider.setOrientation(Qt::Horizontal);

    _parameter_slider.setMinimum(0);
    _parameter_slider.setMaximum(1000);
    _parameter_slider.setValue(100);
    _parameter_slider.setOrientation(Qt::Horizontal);

    connect(&_renderer_select, SIGNAL(activated(int)), SLOT(onRendererChanged(int)));
    connect(&_limit_fps_check, SIGNAL(stateChanged(int)), SLOT(onLimitFpsChanged(int)));

    grid->addWidget(new QLabel("Renderer:"), 0, 0);
    grid->addWidget(&_renderer_select, 0, 1);
    grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 0, 3);
    grid->addWidget(&_limit_fps_check, 0, 4, Qt::AlignRight);

    grid->addWidget(new QLabel("Count:"), 1, 0, Qt::AlignRight);
    grid->addWidget(&_count_slider, 1, 1, 1, 5);

    grid->addWidget(new QLabel("Param:"), 2, 0, Qt::AlignRight);
    grid->addWidget(&_parameter_slider, 2, 1, 1, 5);

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
    _rnd.reset(0x123456789ABCDEF);
    _limit_fps_check.setChecked(true);
    _updateTitle();
  }

  Q_SLOT void onToggleAnimate() { _animate = !_animate; }
  Q_SLOT void onRendererChanged(int index) { _canvas.set_renderer_type(_renderer_select.itemData(index).toInt()); }
  Q_SLOT void onLimitFpsChanged(int value) { _timer.setInterval(value ? 1000 / 120 : 0); }

  Q_SLOT void onTimer() {
    size_t count = unsigned(_count_slider.value());

    double w = _canvas.image_width();
    double h = _canvas.image_height();

    if (_bubbles.size() > count) {
      _bubbles.resize(count);
    }

    double param = double(_parameter_slider.value()) * 0.0001;

    auto newBubble = [](double w, double h, double param, BLRandom& rnd) noexcept -> Bubble {
      Bubble bubble {};

      double r = rnd.next_double() * 20.0 + 5.0;

      bubble.p = BLPoint(rnd.next_double() * w, h + r);
      bubble.r = r;
      bubble.x = bubble.p.x;
      bubble.a = rnd.next_double() * 3.0 + 1.0;
      bubble.b = 0.0;
      bubble.c = rnd.next_double() * (param * 0.4 + 0.001) + param * 0.02;
      bubble.colors[0] = random_rgba32(rnd);
      bubble.colors[1] = BLRgba32(0u);

      return bubble;
    };

    if (_animate) {
      constexpr double PI = 3.14159265359;
      constexpr uint32_t kAddLimit = 10;

      uint32_t added = 0;
      while (_bubbles.size() < count && added < kAddLimit) {
        Bubble bubble = newBubble(w, h, param, _rnd);
        _bubbles.push_back(bubble);
        added++;
      }

      for (Bubble& bubble : _bubbles) {
        bubble.b += bubble.c;

        if (bubble.b > 2.0) {
          bubble.b -= 2.0;
        }

        bubble.p.x = bubble.x + sin(bubble.b * PI) * 20.0;
        bubble.p.y -= bubble.a;

        if (bubble.p.y < -bubble.r) {
          bubble = newBubble(w, h, param, _rnd);
        }
      }
    }

    _canvas.update_canvas(true);
    _updateTitle();
  }

  void on_render_blend2d(BLContext& ctx) noexcept {
    ctx.fill_all(BLRgba32(0xFF000000u));
    ctx.set_comp_op(BL_COMP_OP_PLUS);

    BLGradient g;
    g.set_type(BL_GRADIENT_TYPE_RADIAL);

    double h = _canvas.image_height();

    for (const Bubble& bubble : _bubbles) {
      double r = bubble.r;
      double f = -r * 0.5 + r * (bubble.p.y / h);

      BLGradientStop stops[2] = {
        BLGradientStop(0.0, bubble.colors[0]),
        BLGradientStop(1.0, bubble.colors[1])
      };

      g.set_values(BLRadialGradientValues(bubble.p.x, bubble.p.y, bubble.p.x, bubble.p.y + f, r));
      g.assign_stops(stops, 2);
      ctx.fill_rect(BLRectI(int(bubble.p.x - r - 1.0), int(bubble.p.y - r - 1.0), int(r * 2.0) + 2, int(r * 2.0) + 2), g);
    }
  }

  void on_render_qt(QPainter& ctx) noexcept {
    ctx.fillRect(0, 0, _canvas.image_width(), _canvas.image_height(), QColor(0, 0, 0));
    ctx.setRenderHint(QPainter::Antialiasing, true);
    ctx.setCompositionMode(QPainter::CompositionMode_Plus);
    ctx.setPen(Qt::NoPen);

    double h = _canvas.image_height();

    for (const Bubble& bubble : _bubbles) {
      double r = bubble.r;
      double f = -r * 0.5 + r * (bubble.p.y / h);

      QRadialGradient g(bubble.p.x, bubble.p.y, r, bubble.p.x, bubble.p.y + f);
      g.setInterpolationMode(QGradient::InterpolationMode::ComponentInterpolation);
      g.setColorAt(0.0f, bl_rgba_to_qcolor(bubble.colors[0]));
      g.setColorAt(1.0f, bl_rgba_to_qcolor(bubble.colors[1]));
      ctx.fillRect(QRect(int(bubble.p.x - r - 1.0), int(bubble.p.y - r - 1.0), int(r * 2.0) + 2, int(r * 2.0) + 2), QBrush(g));
    }
  }

  void _updateTitle() {
    char buf[256];
    snprintf(buf, 256, "Bubbles [%dx%d] [Count=%d] [RenderTime=%.2fms FPS=%.1f]",
      _canvas.image_width(),
      _canvas.image_height(),
      int(_bubbles.size()),
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

#include "bl_demo_bubbles.moc"
