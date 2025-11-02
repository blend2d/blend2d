#include <blend2d/blend2d.h>
#include <blend2d-testing/bench/images_data.h>
#include <blend2d-testing/demos/bl_qt_headers.h>
#include <blend2d-testing/demos/bl_qt_canvas.h>

#include <stdlib.h>
#include <vector>

class MainWindow : public QWidget {
  Q_OBJECT

public:
  QTimer _timer;
  QSlider _count_slider;
  QComboBox _renderer_select;
  QComboBox _comp_op_select;
  QCheckBox _limit_fps_check;
  QBLCanvas _canvas;

  BLRandom _random;
  std::vector<BLPoint> _coords;
  std::vector<BLPoint> _steps;
  std::vector<uint32_t> _sprite_ids;

  bool _animate = true;
  BLCompOp _comp_op = BL_COMP_OP_SRC_OVER;

  BLImage _sprites_blend2d[4];
  QImage _sprites_qt[4];

  enum ShapeType {
    kShapeRect,
    kShapeRectPath,
    kShapeRoundRect,
    kShapePolyPath,
  };

  MainWindow() : _random(0x1234) {
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

    _limit_fps_check.setText(QLatin1String("Limit FPS"));

    _count_slider.setOrientation(Qt::Horizontal);
    _count_slider.setMinimum(1);
    _count_slider.setMaximum(10000);
    _count_slider.setSliderPosition(200);

    _canvas.on_render_blend2d = std::bind(&MainWindow::on_render_blend2d, this, std::placeholders::_1);
    _canvas.on_render_qt = std::bind(&MainWindow::on_render_qt, this, std::placeholders::_1);

    connect(&_renderer_select, SIGNAL(activated(int)), SLOT(onRendererChanged(int)));
    connect(&_comp_op_select, SIGNAL(activated(int)), SLOT(onCompOpChanged(int)));
    connect(&_limit_fps_check, SIGNAL(stateChanged(int)), SLOT(onLimitFpsChanged(int)));
    connect(&_count_slider, SIGNAL(valueChanged(int)), SLOT(onCountChanged(int)));

    grid->addWidget(new QLabel("Renderer:"), 0, 0);
    grid->addWidget(&_renderer_select, 0, 1);

    grid->addWidget(new QLabel("Comp Op:"), 0, 2);
    grid->addWidget(&_comp_op_select, 0, 3);

    grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 0, 4);
    grid->addWidget(&_limit_fps_check, 0, 5, Qt::AlignRight);

    grid->addWidget(new QLabel("Count:"), 1, 0, 1, 1, Qt::AlignRight);
    grid->addWidget(&_count_slider, 1, 1, 1, 7);

    vBox->addLayout(grid);
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
    _sprites_blend2d[0].read_from_data(_resource_babelfish_png, sizeof(_resource_babelfish_png));
    _sprites_blend2d[1].read_from_data(_resource_ksplash_png  , sizeof(_resource_ksplash_png  ));
    _sprites_blend2d[2].read_from_data(_resource_ktip_png     , sizeof(_resource_ktip_png     ));
    _sprites_blend2d[3].read_from_data(_resource_firewall_png , sizeof(_resource_firewall_png ));

    for (uint32_t i = 0; i < 4; i++) {
      const BLImage& sprite = _sprites_blend2d[i];

      BLImageData sprite_data;
      sprite.get_data(&sprite_data);

      _sprites_qt[i] = QImage(
        static_cast<unsigned char*>(sprite_data.pixel_data),
        sprite_data.size.w,
        sprite_data.size.h,
        int(sprite_data.stride), QImage::Format_ARGB32_Premultiplied);
    }

    setCount(_count_slider.sliderPosition());
    _limit_fps_check.setChecked(true);
    _updateTitle();
  }

  double randomSign() noexcept { return _random.next_double() < 0.5 ? 1.0 : -1.0; }

  Q_SLOT void onToggleAnimate() { _animate = !_animate; }
  Q_SLOT void onRendererChanged(int index) { _canvas.set_renderer_type(_renderer_select.itemData(index).toInt());  }
  Q_SLOT void onCompOpChanged(int index) { _comp_op = (BLCompOp)_comp_op_select.itemData(index).toInt(); };
  Q_SLOT void onLimitFpsChanged(int value) { _timer.setInterval(value ? 1000 / 120 : 0); }
  Q_SLOT void onCountChanged(int value) { setCount(size_t(value)); }

  Q_SLOT void onTimer() {
    if (_animate) {
      double w = _canvas.image_width();
      double h = _canvas.image_height();

      size_t size = _coords.size();
      for (size_t i = 0; i < size; i++) {
        BLPoint& vertex = _coords[i];
        BLPoint& step = _steps[i];

        vertex += step;
        if (vertex.x < 0 || vertex.x >= w) {
          vertex.x -= step.x;
          if (vertex.x < 0) vertex.x = 0;
          if (vertex.x >= w) vertex.x = w - 1;
          step.x = -step.x;
        }

        if (vertex.y < 0 || vertex.y >= h) {
          vertex.y -= step.y;
          if (vertex.y < 0) vertex.y = 0;
          if (vertex.y >= h) vertex.y = h - 1;
          step.y = -step.y;
        }
      }
    }

    _canvas.update_canvas(true);
    _updateTitle();
  }

  void on_render_blend2d(BLContext& ctx) noexcept {
    ctx.set_comp_op(BL_COMP_OP_SRC_COPY);
    ctx.fill_all(bl_background_for_comp_op(_comp_op));
    ctx.set_comp_op(_comp_op);

    size_t size = _coords.size();
    int rectSize = 128;
    int halfSize = rectSize / 2;

    for (size_t i = 0; i < size; i++) {
      int x = int(_coords[i].x) - halfSize;
      int y = int(_coords[i].y) - halfSize;
      ctx.blit_image(BLPointI(x, y), _sprites_blend2d[_sprite_ids[i]]);
    }
  }

  void on_render_qt(QPainter& ctx) noexcept {
    ctx.setCompositionMode(QPainter::CompositionMode_Source);
    ctx.fillRect(0, 0, _canvas.image_width(), _canvas.image_height(), bl_rgba_to_qcolor(bl_background_for_comp_op(_comp_op)));
    ctx.setRenderHint(QPainter::Antialiasing, true);
    ctx.setCompositionMode(bl_comp_op_to_qt_composition_mode(_comp_op));

    size_t size = _coords.size();
    int rectSize = 128;
    int halfSize = rectSize / 2;

    for (size_t i = 0; i < size; i++) {
      int x = int(_coords[i].x) - halfSize;
      int y = int(_coords[i].y) - halfSize;
      ctx.drawImage(QPoint(x, y), _sprites_qt[_sprite_ids[i]]);
    }
  }

  void setCount(size_t size) {
    int w = _canvas.image_width();
    int h = _canvas.image_height();
    size_t i = _coords.size();

    if (w < 16) w = 128;
    if (h < 16) h = 128;

    _coords.resize(size);
    _steps.resize(size);
    _sprite_ids.resize(size);

    while (i < size) {
      _coords[i].reset(_random.next_double() * (w - 1),
                       _random.next_double() * (h - 1));
      _steps[i].reset((_random.next_double() * 2 + 1) * randomSign(),
                      (_random.next_double() * 2 + 1) * randomSign());
      _sprite_ids[i] = _random.next_uint32() % 4u;
      i++;
    }
  }

  void _updateTitle() {
    char buf[256];
    snprintf(buf, 256, "Sprites [%dx%d] [Count=%zu] [RenderTime=%.2fms FPS=%.1f]",
      _canvas.image_width(),
      _canvas.image_height(),
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

#include "bl_demo_sprites.moc"
