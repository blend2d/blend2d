#include <stdlib.h>

#include "bl_qt_headers.h"
#include "bl_qt_canvas.h"

class MainWindow : public QWidget {
  Q_OBJECT

public:
  QTimer _timer;
  QBLCanvas _canvas;
  QComboBox _renderer_select;
  QComboBox _style_select;
  QCheckBox _limit_fps_check;

  bool _animate = true;
  double _time {};
  int _count {};

  enum class StyleId {
    kSolid,
    kLinear,
    kRadial,
    kConic
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

    _style_select.addItem("Solid Color", QVariant(int(StyleId::kSolid)));
    _style_select.addItem("Linear Gradient", QVariant(int(StyleId::kLinear)));
    _style_select.addItem("Radial Gradient", QVariant(int(StyleId::kRadial)));
    _style_select.addItem("Conic Gradient", QVariant(int(StyleId::kConic)));
    _style_select.setCurrentIndex(1);

    connect(&_renderer_select, SIGNAL(activated(int)), SLOT(onRendererChanged(int)));
    connect(&_limit_fps_check, SIGNAL(stateChanged(int)), SLOT(onLimitFpsChanged(int)));

    grid->addWidget(new QLabel("Renderer:"), 0, 0);
    grid->addWidget(&_renderer_select, 0, 1);

    grid->addWidget(new QLabel("Style:"), 0, 2);
    grid->addWidget(&_style_select, 0, 3);

    grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 0, 4);
    grid->addWidget(&_limit_fps_check, 0, 5, Qt::AlignRight);

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
    _time = 0;
    _count = 0;
    _limit_fps_check.setChecked(true);
    _updateTitle();
  }

  Q_SLOT void onToggleAnimate() { _animate = !_animate; }
  Q_SLOT void onRendererChanged(int index) { _canvas.set_renderer_type(_renderer_select.itemData(index).toInt()); }
  Q_SLOT void onLimitFpsChanged(int value) { _timer.setInterval(value ? 1000 / 120 : 0); }

  Q_SLOT void onTimer() {
    if (_animate) {
      _time += 2.0;
    }

    _updateTitle();
    _canvas.update_canvas(true);
  }

  inline StyleId getStyleId() const noexcept { return StyleId(_style_select.currentData().toInt()); }

  void on_render_blend2d(BLContext& ctx) noexcept {
    ctx.fill_all(BLRgba32(0xFF000000));

    StyleId styleId = getStyleId();
    double kMarginSize = 7;
    double kSquareSize = 45;
    double kFullSize = kSquareSize + kMarginSize * 2.0;
    double kHalfSize = kFullSize / 2.0;

    int w = (_canvas.image_width() + kFullSize - 1) / kFullSize;
    int h = (_canvas.image_height() + kFullSize - 1) / kFullSize;

    int count = w * h;
    _count = count;

    double ix = 0;
    double iy = 0;

    double start = 0;
    double now = _time;
    double PI = 3.14159265359;

    BLGradient gr;

    switch (styleId) {
      case StyleId::kSolid:
        break;
      case StyleId::kLinear:
        gr.set_type(BL_GRADIENT_TYPE_LINEAR);
        gr.set_values(BLLinearGradientValues(0, kMarginSize, 0, kMarginSize + kSquareSize));
        break;
      case StyleId::kRadial:
        gr.set_type(BL_GRADIENT_TYPE_RADIAL);
        gr.set_values(BLRadialGradientValues(kHalfSize, kHalfSize, kHalfSize, kHalfSize - 15, kHalfSize));
        break;
      case StyleId::kConic:
        gr.set_type(BL_GRADIENT_TYPE_CONIC);
        gr.set_values(BLConicGradientValues(kHalfSize, kHalfSize, M_PI / -2.0, 1.0));
        break;
    }

    for (int i = 0; i < count; i++) {
      double x = ix * kFullSize;
      double y = iy * kFullSize;

      double dur = (now - start) + (i * 50);
      double pos = fmod(dur, 3000.0) / 3000.0;
      double bouncePos = bl_abs(pos * 2 - 1);
      double r = (bouncePos * 50 + 50) / 100;
      double b = ((1 - bouncePos) * 50) / 100;

      double rotation = pos * (PI * 2);
      double radius = bouncePos * 25;

      ctx.rotate(rotation, x + kHalfSize, y + kHalfSize);
      ctx.translate(x, y);

      BLRoundRect round_rect(kMarginSize, kMarginSize, kSquareSize, kSquareSize, radius, radius);

      switch (styleId) {
        case StyleId::kSolid: {
          ctx.fill_round_rect(round_rect, BLRgba32(int(r * 255), 0, int(b * 255)));
          break;
        }

        case StyleId::kLinear:
        case StyleId::kRadial: {
          gr.reset_stops();
          gr.add_stop(0, BLRgba32(0xFFFF7F00u));
          gr.add_stop(1, BLRgba32(int(r * 255), 0, int(b * 255)));
          ctx.fill_round_rect(round_rect, gr);
          break;
        }

        case StyleId::kConic: {
          gr.reset_stops();
          gr.add_stop(0.0, BLRgba32(0xFFFF7F00u));
          gr.add_stop(0.5, BLRgba32(int(r * 255), 0, int(b * 255)));
          gr.add_stop(1.0, BLRgba32(0xFFFF7F00u));
          ctx.fill_round_rect(round_rect, gr);
          break;
        }
      }

      ctx.reset_transform();

      if (++ix >= w) { ix = 0; iy++; }
    }
  }

  void on_render_qt(QPainter& ctx) noexcept {
    ctx.fillRect(0, 0, _canvas.image_width(), _canvas.image_height(), QColor(0, 0, 0));

    ctx.setRenderHint(QPainter::Antialiasing, true);
    ctx.setPen(Qt::NoPen);

    StyleId styleId = getStyleId();
    double kMarginSize = 7;
    double kSquareSize = 45;
    double kFullSize = kSquareSize + kMarginSize * 2.0;
    double kHalfSize = kFullSize / 2.0;

    int w = (_canvas.image_width() + kFullSize - 1) / kFullSize;
    int h = (_canvas.image_height() + kFullSize - 1) / kFullSize;

    int count = w * h;
    _count = count;

    double ix = 0;
    double iy = 0;

    double start = 0;
    double now = _time;

    for (int i = 0; i < count; i++) {
      double x = ix * kFullSize;
      double y = iy * kFullSize;

      double dur = (now - start) + (i * 50);
      double pos = fmod(dur, 3000.0) / 3000.0;
      double bouncePos = bl_abs(pos * 2 - 1);
      double r = (bouncePos * 50 + 50) / 100;
      double b = ((1 - bouncePos) * 50) / 100;

      double rotation = pos * 360;
      double radius = bouncePos * 25;

      QTransform m;
      m.translate(x + kHalfSize, y + kHalfSize);
      m.rotate(rotation);
      m.translate(-x - kHalfSize, -y - kHalfSize);

      ctx.save();
      ctx.setTransform(m);
      ctx.translate(x, y);

      switch (styleId) {
        case StyleId::kSolid: {
          ctx.setBrush(QBrush(QColor(qRgb(r * 255, 0, int(b * 255)))));
          break;
        }

        case StyleId::kLinear: {
          QLinearGradient gr(0, kMarginSize, 0, kMarginSize + kSquareSize);
          gr.setColorAt(0, QColor(qRgb(255, 127, 0)));
          gr.setColorAt(1, QColor(qRgb(r * 255, 0, int(b * 255))));
          ctx.setBrush(QBrush(gr));
          break;
        }

        case StyleId::kRadial: {
          QRadialGradient gr(kHalfSize, kHalfSize, kHalfSize, kHalfSize, kHalfSize - 15);
          gr.setColorAt(0, QColor(qRgb(255, 127, 0)));
          gr.setColorAt(1, QColor(qRgb(r * 255, 0, int(b * 255))));
          ctx.setBrush(QBrush(gr));
          break;
        }

        case StyleId::kConic: {
          QConicalGradient gr(kHalfSize, kHalfSize, 270);
          gr.setColorAt(0.0, QColor(qRgb(r * 255, 0, int(b * 255))));
          gr.setColorAt(0.5, QColor(qRgb(255, 127, 0)));
          gr.setColorAt(1.0, QColor(qRgb(r * 255, 0, int(b * 255))));
          ctx.setBrush(QBrush(gr));
          break;
        }
      }

      ctx.drawRoundedRect(kMarginSize, kMarginSize, kSquareSize, kSquareSize, radius, radius);
      ctx.restore();

      if (++ix >= w) { ix = 0; iy++; }
    }
  }

  void _updateTitle() {
    char buf[256];
    snprintf(buf, 256, "Bounces [%dx%d] [Count=%d] [RenderTime=%.2fms FPS=%.1f]",
      _canvas.image_width(),
      _canvas.image_height(),
      _count,
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

#include "bl_demo_bounces.moc"
