#include <blend2d.h>

#include "bl_qt_headers.h"
#include "bl_qt_canvas.h"

class MainWindow : public QWidget {
  Q_OBJECT
public:
  // Widgets.
  QComboBox _cap_type_select;
  QComboBox _join_type_select;
  QSlider _width_slider;
  QSlider _miter_limit_slider;
  QBLCanvas _canvas;

  // Canvas data.
  BLRandom _prng;
  BLPath _path;
  bool _show_control = true;
  size_t _closest_vertex = SIZE_MAX;
  size_t _grabbed_vertex = SIZE_MAX;
  int _grabbed_x = 0;
  int _grabbed_y = 0;
  BLStrokeOptions _stroke_options;

  MainWindow() {
    int max_pb_width = 30;

    setWindowTitle(QLatin1String("Stroke Sample"));

    QVBoxLayout* vBox = new QVBoxLayout();
    vBox->setContentsMargins(0, 0, 0, 0);
    vBox->setSpacing(0);

    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(5, 5, 5, 5);
    grid->setSpacing(5);

    QPushButton* pb_a = new QPushButton("A");
    QPushButton* pb_b = new QPushButton("B");
    QPushButton* pb_c = new QPushButton("C");
    QPushButton* pb_d = new QPushButton("D");
    QPushButton* pb_e = new QPushButton("E");
    QPushButton* pb_f = new QPushButton("F");
    QPushButton* pb_x = new QPushButton("X");
    QPushButton* pb_y = new QPushButton("Y");
    QPushButton* pb_z = new QPushButton("Z");
    QPushButton* pb_random = new QPushButton("Random");
    QPushButton* pb_dump = new QPushButton(QString::fromLatin1("Dump"));

    pb_a->setMaximumWidth(max_pb_width);
    pb_b->setMaximumWidth(max_pb_width);
    pb_c->setMaximumWidth(max_pb_width);
    pb_d->setMaximumWidth(max_pb_width);
    pb_e->setMaximumWidth(max_pb_width);
    pb_f->setMaximumWidth(max_pb_width);
    pb_x->setMaximumWidth(max_pb_width);
    pb_y->setMaximumWidth(max_pb_width);
    pb_z->setMaximumWidth(max_pb_width);

    _cap_type_select.addItem("Butt", QVariant(int(BL_STROKE_CAP_BUTT)));
    _cap_type_select.addItem("Square", QVariant(int(BL_STROKE_CAP_SQUARE)));
    _cap_type_select.addItem("Round", QVariant(int(BL_STROKE_CAP_ROUND)));
    _cap_type_select.addItem("Round-Rev", QVariant(int(BL_STROKE_CAP_ROUND_REV)));
    _cap_type_select.addItem("Triangle", QVariant(int(BL_STROKE_CAP_TRIANGLE)));
    _cap_type_select.addItem("Triangle-Rev", QVariant(int(BL_STROKE_CAP_TRIANGLE_REV)));

    _join_type_select.addItem("Miter-Clip", QVariant(int(BL_STROKE_JOIN_MITER_CLIP)));
    _join_type_select.addItem("Miter-Bevel", QVariant(int(BL_STROKE_JOIN_MITER_BEVEL)));
    _join_type_select.addItem("Miter-Round", QVariant(int(BL_STROKE_JOIN_MITER_ROUND)));
    _join_type_select.addItem("Bevel", QVariant(int(BL_STROKE_JOIN_BEVEL)));
    _join_type_select.addItem("Round", QVariant(int(BL_STROKE_JOIN_ROUND)));

    connect(pb_a, SIGNAL(clicked()), SLOT(onSetDataA()));
    connect(pb_b, SIGNAL(clicked()), SLOT(onSetDataB()));
    connect(pb_c, SIGNAL(clicked()), SLOT(onSetDataC()));
    connect(pb_d, SIGNAL(clicked()), SLOT(onSetDataD()));
    connect(pb_e, SIGNAL(clicked()), SLOT(onSetDataE()));
    connect(pb_f, SIGNAL(clicked()), SLOT(onSetDataF()));
    connect(pb_x, SIGNAL(clicked()), SLOT(onSetDataX()));
    connect(pb_y, SIGNAL(clicked()), SLOT(onSetDataY()));
    connect(pb_z, SIGNAL(clicked()), SLOT(onSetDataZ()));
    connect(pb_dump, SIGNAL(clicked()), SLOT(onDumpPath()));
    connect(pb_random, SIGNAL(clicked()), SLOT(onSetRandom()));
    connect(&_cap_type_select, SIGNAL(activated(int)), SLOT(onCapTypeUpdate(int)));
    connect(&_join_type_select, SIGNAL(activated(int)), SLOT(onJoinTypeUpdate(int)));

    _width_slider.setOrientation(Qt::Horizontal);
    _width_slider.setMinimum(1);
    _width_slider.setMaximum(400);
    _width_slider.setSliderPosition(40);
    connect(&_width_slider, SIGNAL(valueChanged(int)), SLOT(onWidthChanged(int)));

    _miter_limit_slider.setOrientation(Qt::Horizontal);
    _miter_limit_slider.setMinimum(0);
    _miter_limit_slider.setMaximum(1000);
    _miter_limit_slider.setSliderPosition(400);
    connect(&_miter_limit_slider, SIGNAL(valueChanged(int)), SLOT(onMiterLimitChanged(int)));

    _canvas.on_render_blend2d = std::bind(&MainWindow::onRender, this, std::placeholders::_1);
    _canvas.on_mouse_event = std::bind(&MainWindow::on_mouse_event, this, std::placeholders::_1);

    grid->addWidget(new QLabel("Stroke Caps:"), 0, 0, Qt::AlignRight);
    grid->addWidget(&_cap_type_select, 0, 1);
    grid->addWidget(pb_random, 0, 2);
    grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 0, 3);
    grid->addWidget(pb_a, 0, 4);
    grid->addWidget(pb_b, 0, 5);
    grid->addWidget(pb_c, 0, 6);
    grid->addWidget(pb_d, 0, 7);
    grid->addWidget(pb_e, 0, 8);
    grid->addWidget(pb_f, 0, 9);

    grid->addWidget(new QLabel("Stroke Join:"), 1, 0, Qt::AlignRight);
    grid->addWidget(&_join_type_select, 1, 1);
    grid->addWidget(pb_dump, 1, 2);
    grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 1, 3, 1, 4);
    grid->addWidget(pb_x, 1, 7);
    grid->addWidget(pb_y, 1, 8);
    grid->addWidget(pb_z, 1, 9);

    grid->addWidget(new QLabel("Width:"), 2, 0, Qt::AlignRight);
    grid->addWidget(&_width_slider, 2, 1, 1, 10);

    grid->addWidget(new QLabel("Miter Limit:"), 3, 0, Qt::AlignRight);
    grid->addWidget(&_miter_limit_slider, 3, 1, 1, 10);

    vBox->addLayout(grid);
    vBox->addWidget(&_canvas);
    setLayout(vBox);
    onInit();
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Z) {
      _show_control = !_show_control;
      _canvas.update_canvas();
    }
  }

  void onInit() {
    _prng.reset(QCoreApplication::applicationPid());
    _stroke_options.width = _width_slider.sliderPosition();
    _stroke_options.miter_limit = 5;

    onSetDataA();
  }

  void on_mouse_event(QMouseEvent* event) {
    QPointF position(event->position().x() * devicePixelRatio(),
                     event->position().y() * devicePixelRatio());

    if (event->type() == QEvent::MouseButtonPress) {
      if (event->button() == Qt::LeftButton) {
        if (_closest_vertex != SIZE_MAX) {
          _grabbed_vertex = _closest_vertex;
          _grabbed_x = position.x();
          _grabbed_y = position.y();
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
        _path.get_closest_vertex(BLPoint(double(position.x()), double(position.y())), 5, &_closest_vertex);
        _canvas.update_canvas();
      }
      else {
        double x = position.x();
        double y = position.y();
        _path.set_vertex_at(_grabbed_vertex, BL_PATH_CMD_PRESERVE, BLPoint(x, y));
        _canvas.update_canvas();
      }
    }
  }

  Q_SLOT void onSetRandom() {
    double min_x = 25;
    double min_y = 25;
    double max_x = double(_canvas.image_width()) - min_x;
    double max_y = double(_canvas.image_height()) - min_y;

    auto rx = [&]() { return _prng.next_double() * (max_x - min_x) + min_x; };
    auto ry = [&]() { return _prng.next_double() * (max_y - min_y) + min_y; };

    _path.clear();
    _path.move_to(rx(), ry());

    double cmd = _prng.next_double();
    if (cmd < 0.33) {
      _path.line_to(rx(), ry());
      _path.line_to(rx(), ry());
      _path.line_to(rx(), ry());
    }
    else if (cmd < 0.66) {
      _path.quad_to(rx(), ry(), rx(), ry());
      _path.quad_to(rx(), ry(), rx(), ry());
    }
    else {
      _path.cubic_to(rx(), ry(), rx(), ry(), rx(), ry());
    }

    if (_prng.next_double() < 0.5)
      _path.close();

    _canvas.update_canvas();
  }

  Q_SLOT void onSetDataA() {
    _path.clear();
    _path.move_to(345, 333);
    _path.cubic_to(308, 3, 33, 352, 512, 244);
    _canvas.update_canvas();
  }

  Q_SLOT void onSetDataB() {
    _path.clear();
    _path.move_to(60, 177);
    _path.quad_to(144, 354, 396, 116);
    _path.quad_to(106, 184, 43.4567, 43.3091);
    _canvas.update_canvas();
  }

  Q_SLOT void onSetDataC() {
    _path.clear();
    _path.move_to(488, 45);
    _path.cubic_to(22, 331, 26, 27, 493, 338);
    _canvas.update_canvas();
  }

  Q_SLOT void onSetDataD() {
    _path.clear();
    _path.move_to(276, 152);
    _path.line_to(194.576, 54.1927);
    _path.line_to(114, 239);
    _path.line_to(526.311, 134.453);
    _canvas.update_canvas();
  }

  Q_SLOT void onSetDataE() {
    _path.clear();
    _path.move_to(161, 308);
    _path.cubic_to(237.333, 152.509, 146.849, 108.62, 467.225, 59.9782);
    _path.close();
    _canvas.update_canvas();
  }

  Q_SLOT void onSetDataF() {
    _path.clear();
    _path.add_circle(BLCircle(280, 190, 140));
    _canvas.update_canvas();
  }

  Q_SLOT void onSetDataX() {
    _path.clear();
    _path.move_to(300, 200);
    _path.quad_to(50, 200, 500, 200);
    _canvas.update_canvas();
  }

  Q_SLOT void onSetDataY() {
    _path.clear();
    _path.move_to(300, 200);
    _path.cubic_to(50, 200, 500, 200, 350, 200);
    _canvas.update_canvas();
  }

  Q_SLOT void onSetDataZ() {
    _path.clear();
    _path.move_to(300, 200);
    _path.line_to(50, 200);
    _path.line_to(500, 200);
    _path.line_to(350, 200);
    _canvas.update_canvas();
  }

  Q_SLOT void onDumpPath() {
    size_t count = _path.size();
    const BLPoint* vtx = _path.vertex_data();
    const uint8_t* cmd = _path.command_data();

    size_t i = 0;
    while (i < count) {
      switch (cmd[i]) {
        case BL_PATH_CMD_MOVE:
          printf("p.moveTo(%g, %g);\n", vtx[i].x, vtx[i].y);
          i++;
          break;
        case BL_PATH_CMD_ON:
          printf("p.lineTo(%g, %g);\n", vtx[i].x, vtx[i].y);
          i++;
          break;
        case BL_PATH_CMD_QUAD:
          printf("p.quadTo(%g, %g, %g, %g);\n", vtx[i].x, vtx[i].y, vtx[i+1].x, vtx[i+1].y);
          i += 2;
          break;
        case BL_PATH_CMD_CUBIC:
          printf("p.cubicTo(%g, %g, %g, %g, %g, %g);\n", vtx[i].x, vtx[i].y, vtx[i+1].x, vtx[i+1].y, vtx[i+2].x, vtx[i+2].y);
          i += 3;
          break;
        case BL_PATH_CMD_CLOSE:
          printf("p.close();\n");
          i++;
          break;
      }
    }
  }

  Q_SLOT void onCapTypeUpdate(int index) {
    _stroke_options.start_cap = uint8_t(_cap_type_select.itemData(index).toInt());
    _stroke_options.end_cap = _stroke_options.start_cap;
    _canvas.update_canvas();
  }

  Q_SLOT void onJoinTypeUpdate(int index) {
    _stroke_options.join = uint8_t(_join_type_select.itemData(index).toInt());
    _canvas.update_canvas();
  }

  Q_SLOT void onWidthChanged(int value) {
    _stroke_options.width = double(value);
    _canvas.update_canvas();
  }

  Q_SLOT void onMiterLimitChanged(int value) {
    _stroke_options.miter_limit = double(value) / 100.0;
    _canvas.update_canvas();
  }

  void onRender(BLContext& ctx) {
    ctx.fill_all(BLRgba32(0xFF000000u));

    BLPath s;
    s.add_stroked_path(_path, _stroke_options, bl_default_approximation_options);
    ctx.fill_path(s, BLRgba32(0x8F003FAAu));

    if (_show_control) {
      ctx.stroke_path(s, BLRgba32(0xFF0066AAu));
      render_path_points(ctx, s, SIZE_MAX, BLRgba32(0x7F007FFFu), BLRgba32(0xFFFFFFFFu));
    }

    ctx.stroke_path(_path, BLRgba32(0xFFFFFFFFu));
    render_path_points(ctx, _path, _closest_vertex, BLRgba32(0xFFFFFFFF), BLRgba32(0xFF00FFFFu));
  }

  void render_path_points(BLContext& ctx, const BLPath& path, size_t highlight, BLRgba32 normalColor, BLRgba32 highlightColor) noexcept {
    size_t count = path.size();
    const BLPoint* vtx = path.vertex_data();

    for (size_t i = 0; i < count; i++) {
      if (!std::isfinite(vtx[i].x))
        continue;
      ctx.fill_circle(vtx[i].x, vtx[i].y, 2.5, i == highlight ? highlightColor : normalColor);
    }
  }
};

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  MainWindow win;

  win.resize(QSize(580, 520));
  win.show();

  return app.exec();
}

#include "bl_demo_stroke.moc"
