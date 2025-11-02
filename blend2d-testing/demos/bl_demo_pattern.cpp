#include <blend2d/blend2d.h>
#include <blend2d-testing/bench/images_data.h>
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
  QComboBox _extend_mode_select;
  QCheckBox _limit_fps_check;
  QCheckBox _bilinear_check_box;
  QCheckBox _fill_path_check_box;
  QSlider _frac_x;
  QSlider _frac_y;
  QSlider _angle;
  QSlider _scale;
  QBLCanvas _canvas;
  BLImage _sprites_blend2d[4];
  QImage _sprites_qt[4];

  MainWindow() {
    QVBoxLayout* vBox = new QVBoxLayout();
    vBox->setContentsMargins(0, 0, 0, 0);
    vBox->setSpacing(0);

    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(5, 5, 5, 5);
    grid->setSpacing(5);

    QBLCanvas::init_renderer_select_box(&_renderer_select);
    _limit_fps_check.setText(QLatin1String("Limit FPS"));
    _bilinear_check_box.setText(QLatin1String("Bilinear"));
    _fill_path_check_box.setText(QLatin1String("Fill Path"));

    static const char* extendModeNames[] = {
      "PAD",
      "REPEAT",
      "REFLECT",
      "PAD-X PAD-Y",
      "PAD-X REPEAT-Y",
      "PAD-X REFLECT-Y",
      "REPEAT-X PAD-Y",
      "REPEAT-X REPEAT-Y",
      "REPEAT-X REFLECT-Y",
      "REFLECT-X PAD-Y",
      "REFLECT-X REPEAT-Y",
      "REFLECT-X REFLECT-Y"
    };

    static const BLExtendMode extendModeValues[] = {
      BL_EXTEND_MODE_PAD,
      BL_EXTEND_MODE_REPEAT,
      BL_EXTEND_MODE_REFLECT,
      BL_EXTEND_MODE_PAD_X_PAD_Y,
      BL_EXTEND_MODE_PAD_X_REPEAT_Y,
      BL_EXTEND_MODE_PAD_X_REFLECT_Y,
      BL_EXTEND_MODE_REPEAT_X_PAD_Y,
      BL_EXTEND_MODE_REPEAT_X_REPEAT_Y,
      BL_EXTEND_MODE_REPEAT_X_REFLECT_Y,
      BL_EXTEND_MODE_REFLECT_X_PAD_Y,
      BL_EXTEND_MODE_REFLECT_X_REPEAT_Y,
      BL_EXTEND_MODE_REFLECT_X_REFLECT_Y
    };

    for (uint32_t i = 0; i < 12; i++) {
      QString s = extendModeNames[i];
      _extend_mode_select.addItem(s, QVariant(int(extendModeValues[i])));
    }
    _extend_mode_select.setCurrentIndex(1);

    _frac_x.setMinimum(0);
    _frac_x.setMaximum(255);
    _frac_x.setValue(0);
    _frac_x.setOrientation(Qt::Horizontal);

    _frac_y.setMinimum(0);
    _frac_y.setMaximum(255);
    _frac_y.setValue(0);
    _frac_y.setOrientation(Qt::Horizontal);

    _angle.setMinimum(0);
    _angle.setMaximum(3600);
    _angle.setValue(0);
    _angle.setOrientation(Qt::Horizontal);

    _scale.setMinimum(0);
    _scale.setMaximum(1000);
    _scale.setValue(0);
    _scale.setOrientation(Qt::Horizontal);

    connect(&_renderer_select, SIGNAL(activated(int)), SLOT(onRendererChanged(int)));
    connect(&_limit_fps_check, SIGNAL(stateChanged(int)), SLOT(onLimitFpsChanged(int)));
    connect(&_bilinear_check_box, SIGNAL(valueChanged(int)), SLOT(onSliderChanged(int)));
    connect(&_fill_path_check_box, SIGNAL(valueChanged(int)), SLOT(onSliderChanged(int)));
    connect(&_extend_mode_select, SIGNAL(activated(int)), SLOT(onSliderChanged(int)));
    connect(&_frac_x, SIGNAL(valueChanged(int)), SLOT(onSliderChanged(int)));
    connect(&_frac_y, SIGNAL(valueChanged(int)), SLOT(onSliderChanged(int)));
    connect(&_angle, SIGNAL(valueChanged(int)), SLOT(onSliderChanged(int)));
    connect(&_scale, SIGNAL(valueChanged(int)), SLOT(onSliderChanged(int)));

    grid->addWidget(new QLabel("Renderer:"), 0, 0);
    grid->addWidget(&_renderer_select, 0, 1);

    grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 0, 4);
    grid->addWidget(&_limit_fps_check, 0, 5);

    grid->addWidget(new QLabel("Extend Mode:"), 1, 0);
    grid->addWidget(&_extend_mode_select, 1, 1);

    grid->addWidget(new QLabel("Fx Offset:"), 0, 2);
    grid->addWidget(&_frac_x, 0, 3, 1, 2);

    grid->addItem(new QSpacerItem(1, 0, QSizePolicy::Expanding), 0, 4);
    grid->addWidget(&_bilinear_check_box, 1, 5);

    grid->addItem(new QSpacerItem(2, 0, QSizePolicy::Expanding), 0, 4);
    grid->addWidget(&_fill_path_check_box, 2, 5);

    grid->addWidget(new QLabel("Fy Offset:"), 1, 2);
    grid->addWidget(&_frac_y, 1, 3, 1, 2);

    grid->addWidget(new QLabel("Angle:"), 2, 0);
    grid->addWidget(&_angle, 2, 1, 1, 4);

    grid->addWidget(new QLabel("Scale:"), 3, 0);
    grid->addWidget(&_scale, 3, 1, 1, 4);

    _canvas.on_render_blend2d = std::bind(&MainWindow::on_render_blend2d, this, std::placeholders::_1);
    _canvas.on_render_qt = std::bind(&MainWindow::on_render_qt, this, std::placeholders::_1);

    vBox->addItem(grid);
    vBox->addWidget(&_canvas);
    setLayout(vBox);

    connect(&_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
    onInit();
  }

  void showEvent(QShowEvent* event) override { _timer.start(); }
  void hideEvent(QHideEvent* event) override { _timer.stop(); }
  void keyPressEvent(QKeyEvent* event) override {}

  void onInit() {
    _limit_fps_check.setChecked(true);
    _bilinear_check_box.setChecked(true);

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

    _updateTitle();
  }

  Q_SLOT void onRendererChanged(int index) { _canvas.set_renderer_type(_renderer_select.itemData(index).toInt()); }
  Q_SLOT void onLimitFpsChanged(int value) { _timer.setInterval(value ? 1000 / 120 : 0); }

  Q_SLOT void onSliderChanged(int value) { _canvas.update_canvas(true); }

  Q_SLOT void onTimer() {
    _canvas.update_canvas(true);
    _updateTitle();
  }

  inline double tx() const { return 256.0 + double(_frac_x.value()) / 256.0; }
  inline double ty() const { return 256.0 + double(_frac_y.value()) / 256.0; }
  inline double angle_in_radians() const { return (double(_angle.value()) / (3600.0 / 2)) * M_PI; }
  inline double scale() const { return double(_scale.value() + 100) / 100.0; }

  void on_render_blend2d(BLContext& ctx) noexcept {
    int rx = _canvas.image_width() / 2;
    int ry = _canvas.image_height() / 2;

    BLPattern pattern(_sprites_blend2d[0], BLExtendMode(_extend_mode_select.currentData().toInt()));
    pattern.rotate(angle_in_radians(), rx, ry);
    pattern.translate(tx(), ty());
    pattern.scale(scale());

    ctx.set_pattern_quality(
      _bilinear_check_box.isChecked()
        ? BL_PATTERN_QUALITY_BILINEAR
        : BL_PATTERN_QUALITY_NEAREST);

    ctx.set_comp_op(BL_COMP_OP_SRC_COPY);

    if (_fill_path_check_box.isChecked()) {
      ctx.clear_all();
      ctx.fill_circle(rx, ry, bl_min(rx, ry), pattern);
    }
    else {
      ctx.fill_all(pattern);
    }
  }

  void on_render_qt(QPainter& ctx) noexcept {
    int rx = _canvas.image_width() / 2;
    int ry = _canvas.image_height() / 2;

    QTransform tr;
    tr.translate(rx, ry);
    tr.rotateRadians(angle_in_radians());
    tr.translate(-rx + tx(), -ry + ty());
    tr.scale(scale(), scale());

    QBrush brush(_sprites_qt[0]);
    brush.setTransform(tr);

    ctx.setRenderHint(QPainter::SmoothPixmapTransform, _bilinear_check_box.isChecked());
    ctx.setRenderHint(QPainter::Antialiasing, true);
    ctx.setCompositionMode(QPainter::CompositionMode_Source);

    if (_fill_path_check_box.isChecked()) {
      double r = bl_min(rx, ry);
      ctx.fillRect(QRect(0, 0, _canvas.image_width(), _canvas.image_height()), QColor(0, 0, 0, 0));
      ctx.setBrush(brush);
      ctx.setPen(Qt::NoPen);
      ctx.drawEllipse(QPointF(qreal(rx), qreal(ry)), qreal(r), qreal(r));
    }
    else {
      ctx.fillRect(QRect(0, 0, _canvas.image_width(), _canvas.image_height()), brush);
    }
  }

  void _updateTitle() {
    char buf[256];

    snprintf(buf, 256, "Patterns [%dx%d] [RenderTime=%.2fms FPS=%.1f]",
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

  win.setMinimumSize(QSize(20 + (128 + 10) * 4 + 20, 20 + (128 + 10) * 4 + 20));
  win.resize(QSize(580, 520));
  win.show();

  return app.exec();
}

#include "bl_demo_pattern.moc"
