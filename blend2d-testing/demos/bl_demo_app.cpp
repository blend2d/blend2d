#include <blend2d/blend2d.h>

#include <blend2d-testing/commons/image_diff.h>
#include <blend2d-testing/demos/bl_demo_utils.h>
#include <blend2d-testing/demos/bl_qt_headers.h>
#include <blend2d-testing/demos/bl_qt_canvas.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

static constexpr double kControlPointRadius = 3.5;
static constexpr double kPI = 3.14159265359;

template<typename... Args>
static QString fmt(const char* fmt_str, Args&&... args) {
  BLString s;
  s.assign_format(fmt_str, std::forward<Args>(args)...);

  return QString::fromUtf8(s.data(), s.size());
}

static BLRgba32 lerp_rgba32(BLRgba32 a, BLRgba32 b, float t) {
  float ar = float(a.r());
  float ag = float(a.g());
  float ab = float(a.b());
  float aa = float(a.a());

  float br = float(b.r());
  float bg = float(b.g());
  float bb = float(b.b());
  float ba = float(b.a());

  float cr = std::min(std::max(ar + ((br - ar) * t), 0.0f), 255.0f);
  float cg = std::min(std::max(ag + ((bg - ag) * t), 0.0f), 255.0f);
  float cb = std::min(std::max(ab + ((bb - ab) * t), 0.0f), 255.0f);
  float ca = std::min(std::max(aa + ((ba - aa) * t), 0.0f), 255.0f);

  return BLRgba32(uint8_t(cr), uint8_t(cg), uint8_t(cb), uint8_t(ca));
}

// DemoCard
// ========
//
// Base class of a single demo "page". Each demo owns its own canvas and a grid
// of controls placed above it. Derived classes populate `_controls` in their
// constructor and implement the rendering and mouse handling. The card stays
// completely unaware of how it's selected - it just exposes a name, an optional
// status string, and notifies the host when its title-relevant state changes.
class DemoCard : public QWidget {
  Q_OBJECT

public:
  QBLCanvas _canvas;
  QGridLayout* _controls {};
  // Top-level vertical layout: [controls grid][canvas]. Derived classes may
  // append an extra footer widget below the canvas (see EllipticArcDemo).
  QVBoxLayout* _box {};

  // Set by the host (AppWindow) so the card can refresh the title & status bar.
  std::function<void()> on_status_changed;

  // Last known mouse position over the canvas (in image coordinates). Valid only
  // while the cursor is actually inside the canvas (see eventFilter()).
  BLPoint _mouse_pos {};
  bool _mouse_valid = false;

  DemoCard() {
    _box = new QVBoxLayout(this);
    _box->setContentsMargins(0, 0, 0, 0);
    _box->setSpacing(0);

    _controls = new QGridLayout();
    _controls->setContentsMargins(5, 5, 5, 5);
    _controls->setSpacing(5);

    _canvas.on_render_blend2d = [this](BLContext& ctx) {
      on_render(ctx);
      // Rendering is on-demand here (no animation), so refreshing the status
      // after each paint is cheap and keeps the reported size/time accurate.
      emit_status_changed();
    };
    _canvas.on_mouse_event = [this](QMouseEvent* event) {
      double dp = double(devicePixelRatioF());
      _mouse_pos = BLPoint(double(event->position().x()) * dp, double(event->position().y()) * dp);
      _mouse_valid = true;
      on_mouse(event);
      emit_status_changed();
    };

    // Watch the canvas so the status bar can drop the position once the cursor
    // leaves it (mouse callbacks alone never report "no longer hovering").
    _canvas.installEventFilter(this);

    _box->addLayout(_controls);
    _box->addWidget(&_canvas, 1);
  }

  bool eventFilter(QObject* watched, QEvent* event) override {
    if (watched == &_canvas && event->type() == QEvent::Leave && _mouse_valid) {
      _mouse_valid = false;
      emit_status_changed();
    }
    return QWidget::eventFilter(watched, event);
  }

  void emit_status_changed() {
    if (on_status_changed) {
      on_status_changed();
    }
  }

  // Name shown in the navigation panel and the window title.
  virtual QString name() const = 0;
  // Optional extra info appended to the window title (e.g. a parameter value).
  virtual QString status() const { return QString(); }

  virtual void on_render(BLContext& ctx) = 0;
  virtual void on_mouse(QMouseEvent* event) = 0;
};

// CurvesDemo
// ==========
//
// Interactive editor of a quad, a conic, and a cubic curve. Vertices can be
// dragged, the conic weight is controlled by a slider, and the control polygon
// can be toggled on/off.
class CurvesDemo : public DemoCard {
  Q_OBJECT

public:
  static constexpr size_t kPathCount = 3;

  BLPath _paths[kPathCount];
  bl::DemoUtils::PathEditView _path_edits[kPathCount] {};
  size_t _conic_weight_index = 0;
  bool _show_ink_bounds = false;
  bool _show_control = true;

  QSlider* _weight_slider {};
  QCheckBox* _show_ink_bounds_check {};
  QCheckBox* _show_control_check {};

  CurvesDemo() {
    _weight_slider = new QSlider(Qt::Horizontal);
    _weight_slider->setMinimum(1);
    _weight_slider->setMaximum(1000);
    _weight_slider->setSliderPosition(100);

    _show_ink_bounds_check = new QCheckBox("Ink Bounds");
    _show_ink_bounds_check->setChecked(_show_ink_bounds);

    _show_control_check = new QCheckBox("Show Control Data");
    _show_control_check->setChecked(_show_control);

    _controls->addWidget(new QLabel("Conic Weight:"), 0, 0, Qt::AlignRight);
    _controls->addWidget(_weight_slider, 0, 1);
    _controls->addWidget(_show_ink_bounds_check, 0, 2);
    _controls->addWidget(_show_control_check, 0, 3);
    _controls->setColumnStretch(1, 1);

    connect(_weight_slider, &QSlider::valueChanged, this, [this](int) {
      _paths[1].set_weight_at(_conic_weight_index, conic_weight());
      _canvas.update_canvas();
    });

    connect(_show_ink_bounds_check, &QCheckBox::toggled, this, [this](bool checked) {
      _show_ink_bounds = checked;
      _canvas.update_canvas();
    });

    connect(_show_control_check, &QCheckBox::toggled, this, [this](bool checked) {
      _show_control = checked;
      _canvas.update_canvas();
    });

    init_data();
  }

  double conic_weight() const {
    return double(_weight_slider->value()) / 50.0;
  }

  void init_data() {
    _paths[0].move_to(50, 50);
    _paths[0].quad_to(150, 250, 250, 50);

    _paths[1].move_to(300, 50);
    _conic_weight_index = _paths[1].control_data_size();
    _paths[1].conic_to(400, 250, 500, 50, conic_weight());

    _paths[2].move_to(50, 300);
    _paths[2].cubic_to(500, 300, 500, 500, 50, 500);
  }

  QString name() const override {
    return QStringLiteral("Paths - Curves");
  }

  QString status() const override {
    return fmt("Weight=%6f", conic_weight());
  }

  void on_mouse(QMouseEvent* event) override {
    bool update = false;
    double dp = double(devicePixelRatioF());

    for (size_t i = 0; i < kPathCount; i++) {
      if (_path_edits[i].handle_event(_paths[i], event, dp)) {
        update = true;
      }
    }

    if (update) {
      _canvas.update_canvas();
    }
  }

  void on_render(BLContext& ctx) override {
    static const BLRgba32 fill_colors[kPathCount] = {
      BLRgba32(0xAF003FCAu),
      BLRgba32(0xAF3FCA3Fu),
      BLRgba32(0xAFCA3F00u)
    };

    static const BLRgba32 point_colors[kPathCount] = {
      BLRgba32(0xFF3FAFFFu),
      BLRgba32(0xFFAFFFAFu),
      BLRgba32(0xFFFFAF3Fu)
    };

    static const BLRgba32 bbox_colors[kPathCount] = {
      BLRgba32(0xFF007FAFu),
      BLRgba32(0xFF7FAF7Fu),
      BLRgba32(0xFFAF7F00u)
    };

    ctx.fill_all(BLRgba32(0xFF000000u));
    ctx.set_stroke_width(1.2f);

    for (size_t i = 0; i < kPathCount; i++) {
      BLPath vertex_outline;
      vertex_outline.add_vertex_outline_of_path(_paths[i]);

      BLRgba32 vertex_outline_color = BLRgba32((fill_colors[i].value & 0x00FFFFFFu) | (0x7F000000u));
      if (_show_control) {
        ctx.stroke_path(vertex_outline, vertex_outline_color);
      }

      ctx.fill_path(_paths[i], fill_colors[i]);

      if (_show_ink_bounds) {
        BLBox ink_bbox;
        _paths[i].get_ink_bounding_box(&ink_bbox);
        ctx.stroke_box(ink_bbox, bbox_colors[i]);
      }

      if (_show_control) {
        bl::DemoUtils::render_path_vertices(
          ctx, _paths[i], kControlPointRadius, _path_edits[i].closest_vertex(), point_colors[i], BLRgba32(0xFFFFFFFFu));
      }
    }
  }
};

// FitToDemo
// =========
//
// Demonstrates `BLPath::fit_to()`: one of three source paths (with aspect
// ratios 1:1, 2:1, and 1:2) is fitted into an adjustable target rectangle.
// Every fit flag is exposed - the scaling policy (Fill / Contain / Cover), the
// horizontal and vertical alignment, and the shrink-only modifier - so their
// effect on the fitted geometry can be observed live. The target rectangle is
// edited by dragging its four corner handles or its body.
class FitToDemo : public DemoCard {
  Q_OBJECT

public:
  static constexpr size_t kPathCount = 3;
  static constexpr size_t kHandleCount = 4;

  // Immutable source paths. `fit_to()` works in place, so each render fits a
  // *copy* of the selected source path and leaves these untouched.
  BLPath _paths[kPathCount];

  // Target rectangle, stored as two opposite corners so it can be resized by
  // dragging a handle; normalized into a BLRect before fitting.
  BLPoint _rect_p0 {130, 120};
  BLPoint _rect_p1 {460, 380};

  // Rectangle editing state (handles 0..3 are corners; the body is dragged
  // separately to translate the whole rectangle).
  size_t _closest_handle = SIZE_MAX;
  size_t _grabbed_handle = SIZE_MAX;
  bool _dragging_body = false;
  BLPoint _drag_anchor {};

  QComboBox* _path_select {};
  QComboBox* _scale_select {};
  QComboBox* _align_x_select {};
  QComboBox* _align_y_select {};
  QCheckBox* _shrink_only_check {};
  QCheckBox* _show_bounds_check {};

  bool _show_bounds = true;

  FitToDemo() {
    _path_select = new QComboBox();
    _path_select->addItem("Star (1:1)", QVariant(0));
    _path_select->addItem("Arrow (2:1)", QVariant(1));
    _path_select->addItem("Arrow (1:2)", QVariant(2));

    _scale_select = new QComboBox();
    _scale_select->addItem("Fill (stretch)", QVariant(int(BL_PATH_FIT_SCALE_FILL)));
    _scale_select->addItem("Contain", QVariant(int(BL_PATH_FIT_SCALE_CONTAIN)));
    _scale_select->addItem("Cover", QVariant(int(BL_PATH_FIT_SCALE_COVER)));
    _scale_select->setCurrentIndex(1);

    _align_x_select = new QComboBox();
    _align_x_select->addItem("Min (left)", QVariant(int(BL_PATH_FIT_ALIGN_X_MIN)));
    _align_x_select->addItem("Mid (center)", QVariant(int(BL_PATH_FIT_ALIGN_X_MID)));
    _align_x_select->addItem("Max (right)", QVariant(int(BL_PATH_FIT_ALIGN_X_MAX)));
    _align_x_select->setCurrentIndex(1);

    _align_y_select = new QComboBox();
    _align_y_select->addItem("Min (top)", QVariant(int(BL_PATH_FIT_ALIGN_Y_MIN)));
    _align_y_select->addItem("Mid (middle)", QVariant(int(BL_PATH_FIT_ALIGN_Y_MID)));
    _align_y_select->addItem("Max (bottom)", QVariant(int(BL_PATH_FIT_ALIGN_Y_MAX)));
    _align_y_select->setCurrentIndex(1);

    _shrink_only_check = new QCheckBox("Shrink Only");
    _show_bounds_check = new QCheckBox("Show Fitted Bounds");
    _show_bounds_check->setChecked(_show_bounds);

    _controls->addWidget(new QLabel("Path:"), 0, 0, Qt::AlignRight);
    _controls->addWidget(_path_select, 0, 1);
    _controls->addWidget(new QLabel("Scale:"), 0, 2, Qt::AlignRight);
    _controls->addWidget(_scale_select, 0, 3);
    _controls->addWidget(_shrink_only_check, 0, 5);

    _controls->addWidget(new QLabel("Align X:"), 1, 0, Qt::AlignRight);
    _controls->addWidget(_align_x_select, 1, 1);
    _controls->addWidget(new QLabel("Align Y:"), 1, 2, Qt::AlignRight);
    _controls->addWidget(_align_y_select, 1, 3);
    _controls->addWidget(_show_bounds_check, 1, 5);

    _controls->setColumnStretch(4, 1);

    connect(_path_select, QOverload<int>::of(&QComboBox::activated), this, [this](int) { _canvas.update_canvas(); });
    connect(_scale_select, QOverload<int>::of(&QComboBox::activated), this, [this](int) { _canvas.update_canvas(); });
    connect(_align_x_select, QOverload<int>::of(&QComboBox::activated), this, [this](int) { _canvas.update_canvas(); });
    connect(_align_y_select, QOverload<int>::of(&QComboBox::activated), this, [this](int) { _canvas.update_canvas(); });
    connect(_shrink_only_check, &QCheckBox::toggled, this, [this](bool) { _canvas.update_canvas(); });
    connect(_show_bounds_check, &QCheckBox::toggled, this, [this](bool checked) {
      _show_bounds = checked;
      _canvas.update_canvas();
    });

    init_data();
  }

  void init_data() {
    // 1:1 - a four-pointed compass star (bounding box 200 x 200).
    _paths[0].move_to(100, 0);
    _paths[0].line_to(130, 70);
    _paths[0].line_to(200, 100);
    _paths[0].line_to(130, 130);
    _paths[0].line_to(100, 200);
    _paths[0].line_to(70, 130);
    _paths[0].line_to(0, 100);
    _paths[0].line_to(70, 70);
    _paths[0].close();

    // 2:1 - a wide arrow pointing right (bounding box 200 x 100).
    _paths[1].move_to(0, 30);
    _paths[1].line_to(120, 30);
    _paths[1].line_to(120, 0);
    _paths[1].line_to(200, 50);
    _paths[1].line_to(120, 100);
    _paths[1].line_to(120, 70);
    _paths[1].line_to(0, 70);
    _paths[1].close();

    // 1:2 - a tall arrow pointing up (bounding box 100 x 200).
    _paths[2].move_to(35, 200);
    _paths[2].line_to(35, 80);
    _paths[2].line_to(0, 80);
    _paths[2].line_to(50, 0);
    _paths[2].line_to(100, 80);
    _paths[2].line_to(65, 80);
    _paths[2].line_to(65, 200);
    _paths[2].close();
  }

  QString name() const override {
    return QStringLiteral("Paths - FitTo");
  }

  QString status() const override {
    return QStringLiteral("Scale=%1 | AlignX=%2 | AlignY=%3%4")
      .arg(_scale_select->currentText())
      .arg(_align_x_select->currentText())
      .arg(_align_y_select->currentText())
      .arg(_shrink_only_check->isChecked() ? QStringLiteral("  ShrinkOnly") : QString());
  }

  BLPathFitFlags fit_flags() const {
    uint32_t flags = uint32_t(_scale_select->currentData().toInt())
                   | uint32_t(_align_x_select->currentData().toInt())
                   | uint32_t(_align_y_select->currentData().toInt());
    if (_shrink_only_check->isChecked())
      flags |= uint32_t(BL_PATH_FIT_SHRINK_ONLY);
    return BLPathFitFlags(flags);
  }

  // Normalized target rectangle (positive width/height) used for fitting.
  BLRect target_rect() const {
    double x0 = _rect_p0.x < _rect_p1.x ? _rect_p0.x : _rect_p1.x;
    double y0 = _rect_p0.y < _rect_p1.y ? _rect_p0.y : _rect_p1.y;
    double x1 = _rect_p0.x > _rect_p1.x ? _rect_p0.x : _rect_p1.x;
    double y1 = _rect_p0.y > _rect_p1.y ? _rect_p0.y : _rect_p1.y;
    return BLRect(x0, y0, x1 - x0, y1 - y0);
  }

  BLPoint handle_pos(size_t i) const {
    switch (i) {
      case 0:  return BLPoint(_rect_p0.x, _rect_p0.y); // top-left
      case 1:  return BLPoint(_rect_p1.x, _rect_p0.y); // top-right
      case 2:  return BLPoint(_rect_p1.x, _rect_p1.y); // bottom-right
      default: return BLPoint(_rect_p0.x, _rect_p1.y); // bottom-left
    }
  }

  void move_handle(size_t i, BLPoint p) {
    switch (i) {
      case 0:  _rect_p0.x = p.x; _rect_p0.y = p.y; break;
      case 1:  _rect_p1.x = p.x; _rect_p0.y = p.y; break;
      case 2:  _rect_p1.x = p.x; _rect_p1.y = p.y; break;
      default: _rect_p0.x = p.x; _rect_p1.y = p.y; break;
    }
  }

  size_t closest_handle(BLPoint p, double max_distance) const {
    size_t closest = SIZE_MAX;
    double closest_distance = max_distance;
    for (size_t i = 0; i < kHandleCount; i++) {
      BLPoint h = handle_pos(i);
      double d = std::hypot(h.x - p.x, h.y - p.y);
      if (d <= closest_distance) {
        closest = i;
        closest_distance = d;
      }
    }
    return closest;
  }

  bool hit_body(BLPoint p) const {
    BLRect r = target_rect();
    return p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y && p.y <= r.y + r.h;
  }

  void on_mouse(QMouseEvent* event) override {
    double dp = double(devicePixelRatioF());
    BLPoint p(double(event->position().x()) * dp, double(event->position().y()) * dp);

    if (event->type() == QEvent::MouseButtonPress) {
      if (event->button() == Qt::LeftButton) {
        size_t h = closest_handle(p, 8.0);
        if (h != SIZE_MAX) {
          _grabbed_handle = h;
        }
        else if (hit_body(p)) {
          _dragging_body = true;
          _drag_anchor = p;
        }
        _canvas.update_canvas();
      }
    }
    else if (event->type() == QEvent::MouseButtonRelease) {
      if (event->button() == Qt::LeftButton) {
        _grabbed_handle = SIZE_MAX;
        _dragging_body = false;
        _canvas.update_canvas();
      }
    }
    else if (event->type() == QEvent::MouseMove) {
      if (_grabbed_handle != SIZE_MAX) {
        move_handle(_grabbed_handle, p);
        _canvas.update_canvas();
      }
      else if (_dragging_body) {
        _rect_p0.x += p.x - _drag_anchor.x;
        _rect_p0.y += p.y - _drag_anchor.y;
        _rect_p1.x += p.x - _drag_anchor.x;
        _rect_p1.y += p.y - _drag_anchor.y;
        _drag_anchor = p;
        _canvas.update_canvas();
      }
      else {
        size_t h = closest_handle(p, 8.0);
        if (h != _closest_handle) {
          _closest_handle = h;
          _canvas.update_canvas();
        }
      }
    }
  }

  void on_render(BLContext& ctx) override {
    ctx.fill_all(BLRgba32(0xFF000000u));

    BLRect rect = target_rect();

    // Target rectangle: faint fill plus a visible outline.
    ctx.fill_rect(rect, BLRgba32(0x14FFFFFFu));
    ctx.set_stroke_width(1.5f);
    ctx.stroke_rect(rect, BLRgba32(0x66FFFFFFu));

    // Fit a copy of the selected source path into the rectangle in place.
    size_t idx = size_t(_path_select->currentIndex());
    BLPath path = _paths[idx];
    path.fit_to(rect, fit_flags());

    // The fitted path. Cover may exceed the rectangle on purpose, so nothing
    // is clipped here.
    ctx.fill_path(path, BLRgba32(0x80003FCAu));
    ctx.set_stroke_width(2.0f);
    ctx.stroke_path(path, BLRgba32(0xFF3F9FFFu));

    // Bounding box of the fitted path - shows the free margins left by Contain
    // and the overflow produced by Cover, and how alignment shifts the result.
    if (_show_bounds) {
      BLBox bbox;
      if (path.get_vertex_bounding_box(&bbox) == BL_SUCCESS) {
        ctx.set_stroke_width(1.0f);
        ctx.stroke_box(bbox, BLRgba32(0x80FFCC00u));
      }
    }

    // Rectangle corner handles.
    for (size_t i = 0; i < kHandleCount; i++) {
      BLPoint h = handle_pos(i);
      bool active = (i == _closest_handle || i == _grabbed_handle);
      ctx.fill_circle(h.x, h.y, kControlPointRadius, active ? BLRgba32(0xFF00FFFFu) : BLRgba32(0xFF007FFFu));
    }
  }
};

// HitTestDemo
// ===========
//
// Demonstrates `BLPath::hit_test()`: a single editable path is filled, and the
// fill color changes whenever the cursor is inside the shape according to the
// selected fill rule.
class HitTestDemo : public DemoCard {
  Q_OBJECT

public:
  BLPath _path;
  bl::DemoUtils::PathEditView _path_edit {};

  BLFillRule _fill_rule = BL_FILL_RULE_EVEN_ODD;
  bool _hit_test_on = false;
  bool _show_control = true;

  QComboBox* _fill_rule_select {};
  QCheckBox* _show_control_check {};

  HitTestDemo() {
    _fill_rule_select = new QComboBox();
    _fill_rule_select->addItem("Non-Zero", QVariant(int(BL_FILL_RULE_NON_ZERO)));
    _fill_rule_select->addItem("Even-Odd", QVariant(int(BL_FILL_RULE_EVEN_ODD)));
    _fill_rule_select->setCurrentIndex(1);

    _show_control_check = new QCheckBox("Show Control Data");
    _show_control_check->setChecked(_show_control);

    connect(_show_control_check, &QCheckBox::toggled, this, [this](bool checked) {
      _show_control = checked;
      _canvas.update_canvas();
    });


    _controls->addWidget(new QLabel("Fill Rule:"), 0, 0);
    _controls->addWidget(_fill_rule_select, 0, 1);
    _controls->setColumnStretch(2, 1);
    _controls->addWidget(_show_control_check, 0, 3);

    connect(_fill_rule_select, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
      _fill_rule = static_cast<BLFillRule>(_fill_rule_select->currentData().toInt());
      _canvas.update_canvas();
    });

    init_data();
  }

  void init_data() {
    _path.move_to(50, 50);
    _path.quad_to(150, 250, 250, 50);
    _path.conic_to(400, 250, 500, 50, 1.9);
    _path.cubic_to(500, 300, 400, 500, 50, 500);
    _path.close();

    _path.move_to(200, 250);
    _path.line_to(300, 250);
    _path.line_to(250, 350);
    _path.close();
  }

  QString name() const override {
    return QStringLiteral("Paths - HitTest");
  }

  QString status() const override {
    return _hit_test_on ? QStringLiteral("IN") : QStringLiteral("OUT");
  }

  void on_mouse(QMouseEvent* event) override {
    bool update = false;
    double dp = double(devicePixelRatioF());

    QPointF position(double(event->position().x()) * dp,
                     double(event->position().y()) * dp);

    if (_path_edit.handle_event(_path, event, dp)) {
      update = true;
    }

    bool hit = (_path.hit_test(BLPoint(double(position.x()), double(position.y())), _fill_rule) == BL_HIT_TEST_IN);
    if (hit != _hit_test_on) {
      _hit_test_on = hit;
      update = true;
    }

    if (update) {
      _canvas.update_canvas();
    }
  }

  void on_render(BLContext& ctx) override {
    const BLRgba32 fill_color(0xAF003FCAu);
    const BLRgba32 hit_color(0xAF004FFFu);

    const BLRgba32 point_color(0xFF3FAFFFu);
    const BLRgba32 vertex_outline_color(0x7F00AFFFu);

    ctx.fill_all(BLRgba32(0xFF000000u));

    if (_show_control) {
      BLPath vertex_outline;
      vertex_outline.add_vertex_outline_of_path(_path);
      ctx.set_stroke_width(1.5f);
      ctx.stroke_path(vertex_outline, vertex_outline_color);
    }

    ctx.set_fill_rule(_fill_rule);
    ctx.fill_path(_path, _hit_test_on ? hit_color : fill_color);

    if (_show_control) {
      bl::DemoUtils::render_path_vertices(
        ctx, _path, kControlPointRadius, _path_edit.closest_vertex(), point_color, BLRgba32(0xFFFFFFFFu));
    }
  }
};

// EllipticArcDemo
// ===============
//
// Visualizes `BLPath::elliptic_arc_to()`. The two endpoints are draggable and
// the radii, rotation angle, and the large-arc / sweep flags are configurable.
// All four flag combinations are drawn faintly, with the selected one on top,
// and the equivalent SVG arc command is shown in a footer.
class EllipticArcDemo : public DemoCard {
  Q_OBJECT

public:
  QSlider* _x_radius_slider {};
  QSlider* _y_radius_slider {};
  QSlider* _angle_slider {};
  QCheckBox* _large_arc_check {};
  QCheckBox* _sweep_arc_check {};

  BLPoint _pts[2] {};
  size_t _closest_vertex = SIZE_MAX;
  size_t _grabbed_vertex = SIZE_MAX;

  EllipticArcDemo() {
    _x_radius_slider = new QSlider(Qt::Horizontal);
    _x_radius_slider->setRange(1, 500);
    _x_radius_slider->setSliderPosition(131);

    _y_radius_slider = new QSlider(Qt::Horizontal);
    _y_radius_slider->setRange(1, 500);
    _y_radius_slider->setSliderPosition(143);

    _angle_slider = new QSlider(Qt::Horizontal);
    _angle_slider->setRange(-360, 360);
    _angle_slider->setSliderPosition(0);

    _large_arc_check = new QCheckBox("Large Arc Flag");
    _sweep_arc_check = new QCheckBox("Sweep Arc Flag");

    _controls->addWidget(new QLabel("X Radius:"), 0, 0, Qt::AlignRight);
    _controls->addWidget(_x_radius_slider, 0, 1);
    _controls->addWidget(_large_arc_check, 0, 2);

    _controls->addWidget(new QLabel("Y Radius:"), 1, 0, Qt::AlignRight);
    _controls->addWidget(_y_radius_slider, 1, 1);
    _controls->addWidget(_sweep_arc_check, 1, 2);

    _controls->addWidget(new QLabel("Angle:"), 2, 0, Qt::AlignRight);
    _controls->addWidget(_angle_slider, 2, 1, 1, 2);
    _controls->setColumnStretch(1, 1);

    connect(_x_radius_slider, &QSlider::valueChanged, this, [this](int) { _canvas.update_canvas(); });
    connect(_y_radius_slider, &QSlider::valueChanged, this, [this](int) { _canvas.update_canvas(); });
    connect(_angle_slider, &QSlider::valueChanged, this, [this](int) { _canvas.update_canvas(); });
    connect(_large_arc_check, &QCheckBox::toggled, this, [this](bool) { _canvas.update_canvas(); });
    connect(_sweep_arc_check, &QCheckBox::toggled, this, [this](bool) { _canvas.update_canvas(); });

    init_data();
  }

  void init_data() {
    _pts[0].reset(124, 180);
    _pts[1].reset(296, 284);
  }

  QString name() const override {
    return QStringLiteral("Paths - Elliptic Arc");
  }

  QString status() const override {
    double angle = (double(_angle_slider->value()) / 180.0) * kPI;
    bool large_arc_flag = _large_arc_check->isChecked();
    bool sweep_arc_flag = _sweep_arc_check->isChecked();

    BLPoint radius(_x_radius_slider->value(), _y_radius_slider->value());
    BLPoint start(_pts[0]);
    BLPoint end(_pts[1]);

    return fmt("<path d=\"M%g %g A%g %g %g %d %d %g %g\" />",
      start.x, start.y, radius.x, radius.y, angle / kPI * 180, large_arc_flag, sweep_arc_flag, end.x, end.y);
  }

  size_t closest_point(BLPoint p, double max_distance) const noexcept {
    size_t closest = SIZE_MAX;
    double closest_distance = std::numeric_limits<double>::max();
    for (size_t i = 0; i < 2; i++) {
      double d = std::hypot(_pts[i].x - p.x, _pts[i].y - p.y);
      if (d < closest_distance && d < max_distance) {
        closest = i;
        closest_distance = d;
      }
    }
    return closest;
  }

  void on_mouse(QMouseEvent* event) override {
    double dp = double(devicePixelRatioF());
    double mx = double(event->position().x()) * dp;
    double my = double(event->position().y()) * dp;

    if (event->type() == QEvent::MouseButtonPress) {
      if (event->button() == Qt::LeftButton && _closest_vertex != SIZE_MAX) {
        _grabbed_vertex = _closest_vertex;
        _canvas.update_canvas();
      }
    }
    else if (event->type() == QEvent::MouseButtonRelease) {
      if (event->button() == Qt::LeftButton && _grabbed_vertex != SIZE_MAX) {
        _grabbed_vertex = SIZE_MAX;
        _canvas.update_canvas();
      }
    }
    else if (event->type() == QEvent::MouseMove) {
      if (_grabbed_vertex == SIZE_MAX) {
        _closest_vertex = closest_point(BLPoint(mx, my), 5);
        _canvas.update_canvas();
      }
      else {
        _pts[_grabbed_vertex] = BLPoint(mx, my);
        _canvas.update_canvas();
      }
    }
  }

  void on_render(BLContext& ctx) override {
    ctx.fill_all(BLRgba32(0xFF000000u));

    BLPoint radius(_x_radius_slider->value(), _y_radius_slider->value());
    BLPoint start(_pts[0]);
    BLPoint end(_pts[1]);

    double angle = (double(_angle_slider->value()) / 180.0) * kPI;
    bool large_arc_flag = _large_arc_check->isChecked();
    bool sweep_arc_flag = _sweep_arc_check->isChecked();

    // Render all flag combinations faintly before the selected one.
    BLPath p;
    p.move_to(start); p.elliptic_arc_to(radius, angle, false, false, end);
    p.move_to(start); p.elliptic_arc_to(radius, angle, false, true, end);
    p.move_to(start); p.elliptic_arc_to(radius, angle, true, false, end);
    p.move_to(start); p.elliptic_arc_to(radius, angle, true, true, end);
    ctx.stroke_path(p, BLRgba32(0x40FFFFFFu));

    // Render the elliptic arc selected by the current flags.
    p.clear();
    p.move_to(start);
    p.elliptic_arc_to(radius, angle, large_arc_flag, sweep_arc_flag, end);
    ctx.stroke_path(p, BLRgba32(0xFFFFFFFFu));

    // Render all on-path points (the arc was split into segments).
    size_t count = p.vertex_count();
    const BLPoint* vtx = p.vertex_data();
    ctx.set_fill_style(BLRgba32(0xFF808080u));
    for (size_t i = 0; i < count; i++) {
      ctx.fill_circle(vtx[i].x, vtx[i].y, 2.0);
    }

    // Render the draggable endpoints.
    for (size_t i = 0; i < 2; i++) {
      ctx.fill_circle(
        _pts[i].x, _pts[i].y, kControlPointRadius, i == _closest_vertex ? BLRgba32(0xFF00FFFFu) : BLRgba32(0xFF007FFFu));
    }
  }
};

// StrokeDemo
// ==========
//
// Interactive playground for `BLPath::add_stroked_path()`: an editable path is
// stroked with configurable caps, joins, width, miter limit, and dash pattern.
// A set of preset buttons loads various test paths, and the resulting stroke
// geometry can be inspected on top of the original path.
class StrokeDemo : public DemoCard {
  Q_OBJECT

public:
  static constexpr size_t kMaxDashCount = 4;

  QComboBox* _cap_select {};
  QComboBox* _join_select {};
  QSlider* _width_slider {};
  QSlider* _miter_slider {};
  QGroupBox* _dash_group {};
  QSpinBox* _dash_count_spin {};
  QWidget* _dash_entry_controls[kMaxDashCount] {};
  QSlider* _dash_sliders[kMaxDashCount] {};
  QSlider* _dash_offset_slider {};
  QCheckBox* _show_control_check {};

  BLRandom _prng;
  BLPath _path;
  bl::DemoUtils::PathEditView _path_edit {};
  bool _show_control = true;
  BLStrokeOptions _stroke_options;

  StrokeDemo() {
    const int max_pb_width = 30;

    _cap_select = new QComboBox();
    _cap_select->addItem("Butt", QVariant(int(BL_STROKE_CAP_BUTT)));
    _cap_select->addItem("Square", QVariant(int(BL_STROKE_CAP_SQUARE)));
    _cap_select->addItem("Round", QVariant(int(BL_STROKE_CAP_ROUND)));
    _cap_select->addItem("Round-Rev", QVariant(int(BL_STROKE_CAP_ROUND_REV)));
    _cap_select->addItem("Triangle", QVariant(int(BL_STROKE_CAP_TRIANGLE)));
    _cap_select->addItem("Triangle-Rev", QVariant(int(BL_STROKE_CAP_TRIANGLE_REV)));

    _join_select = new QComboBox();
    _join_select->addItem("Miter-Clip", QVariant(int(BL_STROKE_JOIN_MITER_CLIP)));
    _join_select->addItem("Miter-Bevel", QVariant(int(BL_STROKE_JOIN_MITER_BEVEL)));
    _join_select->addItem("Miter-Round", QVariant(int(BL_STROKE_JOIN_MITER_ROUND)));
    _join_select->addItem("Bevel", QVariant(int(BL_STROKE_JOIN_BEVEL)));
    _join_select->addItem("Round", QVariant(int(BL_STROKE_JOIN_ROUND)));

    _width_slider = new QSlider(Qt::Horizontal);
    _width_slider->setRange(1, 400);
    _width_slider->setSliderPosition(40);

    _miter_slider = new QSlider(Qt::Horizontal);
    _miter_slider->setRange(0, 1000);
    _miter_slider->setSliderPosition(400);

    _dash_group = new QGroupBox("Dashing");
    _dash_group->setCheckable(true);
    QGridLayout* dash_layout = new QGridLayout(_dash_group);

    _dash_count_spin = new QSpinBox();
    _dash_count_spin->setRange(2, int(kMaxDashCount));
    _dash_count_spin->setValue(2);
    _dash_count_spin->setToolTip("Number of dash-array entries. Odd counts alternate dashes and gaps on each repeat.");

    _dash_offset_slider = new QSlider(Qt::Horizontal);
    _dash_offset_slider->setRange(-400, 400);
    QLabel* dash_offset_value = new QLabel("0");
    dash_offset_value->setMinimumWidth(fontMetrics().horizontalAdvance(QStringLiteral("-400")));
    dash_offset_value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QHBoxLayout* dash_header = new QHBoxLayout();
    dash_header->addWidget(new QLabel("Entries:"));
    dash_header->addWidget(_dash_count_spin);
    dash_header->addSpacing(10);
    dash_header->addWidget(new QLabel("Offset:"));
    dash_header->addWidget(_dash_offset_slider, 1);
    dash_header->addWidget(dash_offset_value);
    dash_layout->addLayout(dash_header, 0, 0, 1, 2);

    static const char* dash_labels[kMaxDashCount] = {"Dash 1:", "Gap 1:", "Dash 2:", "Gap 2:"};
    static const int dash_defaults[kMaxDashCount] = {40, 20, 10, 20};
    for (size_t i = 0; i < kMaxDashCount; i++) {
      _dash_entry_controls[i] = new QWidget();
      QHBoxLayout* entry_layout = new QHBoxLayout(_dash_entry_controls[i]);
      entry_layout->setContentsMargins(0, 0, 0, 0);

      _dash_sliders[i] = new QSlider(Qt::Horizontal);
      _dash_sliders[i]->setRange(1, 200);
      _dash_sliders[i]->setValue(dash_defaults[i]);
      QLabel* value_label = new QLabel(QString::number(dash_defaults[i]));
      value_label->setMinimumWidth(fontMetrics().horizontalAdvance(QStringLiteral("200")));
      value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

      entry_layout->addWidget(new QLabel(dash_labels[i]));
      entry_layout->addWidget(_dash_sliders[i], 1);
      entry_layout->addWidget(value_label);
      dash_layout->addWidget(_dash_entry_controls[i], 1 + int(i / 2), int(i % 2));

      connect(_dash_sliders[i], &QSlider::valueChanged, this, [this, value_label](int value) {
        value_label->setNum(value);
        update_dash_options();
      });
    }
    _dash_group->setChecked(false);

    _show_control_check = new QCheckBox("Show Control Data");
    _show_control_check->setChecked(_show_control);

    // Preset buttons: A-F load curves/lines, X-Z load degenerate paths.
    struct Preset { const char* label; void (StrokeDemo::*fn)(); };
    static const Preset presets[] = {
      {"A", &StrokeDemo::set_data_a}, {"B", &StrokeDemo::set_data_b}, {"C", &StrokeDemo::set_data_c},
      {"D", &StrokeDemo::set_data_d}, {"E", &StrokeDemo::set_data_e}, {"F", &StrokeDemo::set_data_f},
      {"X", &StrokeDemo::set_data_x}, {"Y", &StrokeDemo::set_data_y}, {"Z", &StrokeDemo::set_data_z}
    };

    QPushButton* pb[9];
    for (size_t i = 0; i < 9; i++) {
      pb[i] = new QPushButton(presets[i].label);
      pb[i]->setMaximumWidth(max_pb_width);
      auto fn = presets[i].fn;
      connect(pb[i], &QPushButton::clicked, this, [this, fn]() { (this->*fn)(); });
    }

    QPushButton* pb_random = new QPushButton("Random");
    connect(pb_random, &QPushButton::clicked, this, [this]() { set_random(); });

    _controls->addWidget(new QLabel("Stroke Caps:"), 0, 0, Qt::AlignRight);
    _controls->addWidget(_cap_select, 0, 1);
    _controls->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 0, 9);
    _controls->addWidget(new QLabel("Presets:"), 0, 2);
    _controls->addWidget(pb[0], 0, 3);
    _controls->addWidget(pb[1], 0, 4);
    _controls->addWidget(pb[2], 0, 5);
    _controls->addWidget(pb[3], 0, 6);
    _controls->addWidget(pb[4], 0, 7);
    _controls->addWidget(pb[5], 0, 8);

    _controls->addWidget(new QLabel("Stroke Join:"), 1, 0, Qt::AlignRight);
    _controls->addWidget(_join_select, 1, 1);
    _controls->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 1, 3, 1, 4);
    _controls->addWidget(pb_random, 1, 3, 1, 3);
    _controls->addWidget(pb[6], 1, 6);
    _controls->addWidget(pb[7], 1, 7);
    _controls->addWidget(pb[8], 1, 8);

    _controls->addWidget(new QLabel("Width:"), 2, 0, Qt::AlignRight);
    _controls->addWidget(_width_slider, 2, 1, 1, 10);

    _controls->addWidget(new QLabel("Miter Limit:"), 3, 0, Qt::AlignRight);
    _controls->addWidget(_miter_slider, 3, 1, 1, 10);

    _controls->addWidget(_dash_group, 4, 0, 1, 11);

    _controls->addWidget(_show_control_check, 0, 10);

    connect(_cap_select, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
      _stroke_options.set_caps(BLStrokeCap(_cap_select->itemData(index).toInt()));
      _canvas.update_canvas();
    });
    connect(_join_select, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
      _stroke_options.set_join(BLStrokeJoin(_join_select->itemData(index).toInt()));
      _canvas.update_canvas();
    });
    connect(_width_slider, &QSlider::valueChanged, this, [this](int value) {
      _stroke_options.set_width(float(value));
      _canvas.update_canvas();
    });
    connect(_miter_slider, &QSlider::valueChanged, this, [this](int value) {
      _stroke_options.set_miter_limit(float(value) / 100.0f);
      _canvas.update_canvas();
    });
    connect(_dash_group, &QGroupBox::toggled, this, [this](bool) { update_dash_options(); });
    connect(_dash_count_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { update_dash_options(); });
    connect(_dash_offset_slider, &QSlider::valueChanged, this, [this, dash_offset_value](int value) {
      dash_offset_value->setNum(value);
      update_dash_options();
    });
    connect(_show_control_check, &QCheckBox::toggled, this, [this](bool checked) {
      _show_control = checked;
      _canvas.update_canvas();
    });

    init_data();
  }

  void init_data() {
    _prng.reset(QCoreApplication::applicationPid());
    _stroke_options.set_width(_width_slider->sliderPosition());
    _stroke_options.set_miter_limit(5);
    update_dash_options();
    set_data_a();
  }

  void update_dash_options() {
    bool enabled = _dash_group->isChecked();
    size_t count = size_t(_dash_count_spin->value());
    float dash_array[kMaxDashCount];
    for (size_t i = 0; i < kMaxDashCount; i++) {
      _dash_entry_controls[i]->setEnabled(enabled && i < count);
      dash_array[i] = float(_dash_sliders[i]->value());
    }

    if (enabled) {
      _stroke_options.set_dash_array(dash_array, count);
    }
    else {
      _stroke_options.reset_dash_array();
    }
    _stroke_options.set_dash_offset(float(_dash_offset_slider->value()));
    _canvas.update_canvas();
  }

  QString name() const override {
    return QStringLiteral("Paths - Stroke");
  }

  QString status() const override {
    return fmt("Width=%d | MiterLimit=%d",
      _width_slider->value(),
      _miter_slider->value());
  }

  void on_mouse(QMouseEvent* event) override {
    if (_path_edit.handle_event(_path, event, double(devicePixelRatioF()))) {
      _canvas.update_canvas();
    }
  }

  void set_random() {
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

  void set_data_a() {
    _path.clear();
    _path.move_to(345, 333);
    _path.cubic_to(308, 3, 33, 352, 512, 244);
    _canvas.update_canvas();
  }

  void set_data_b() {
    _path.clear();
    _path.move_to(60, 177);
    _path.quad_to(144, 354, 396, 116);
    _path.quad_to(106, 184, 43.4567, 43.3091);
    _canvas.update_canvas();
  }

  void set_data_c() {
    _path.clear();
    _path.move_to(488, 45);
    _path.cubic_to(22, 331, 26, 27, 493, 338);
    _canvas.update_canvas();
  }

  void set_data_d() {
    _path.clear();
    _path.move_to(276, 152);
    _path.line_to(194.576, 54.1927);
    _path.line_to(114, 239);
    _path.line_to(526.311, 134.453);
    _canvas.update_canvas();
  }

  void set_data_e() {
    _path.clear();
    _path.move_to(161, 308);
    _path.cubic_to(237.333, 152.509, 146.849, 108.62, 467.225, 59.9782);
    _path.close();
    _canvas.update_canvas();
  }

  void set_data_f() {
    _path.clear();
    _path.add_circle(BLCircle(280, 190, 140));
    _canvas.update_canvas();
  }

  void set_data_x() {
    _path.clear();
    _path.move_to(300, 200);
    _path.quad_to(50, 200, 500, 200);
    _canvas.update_canvas();
  }

  void set_data_y() {
    _path.clear();
    _path.move_to(300, 200);
    _path.cubic_to(50, 200, 500, 200, 350, 200);
    _canvas.update_canvas();
  }

  void set_data_z() {
    _path.clear();
    _path.move_to(300, 200);
    _path.line_to(50, 200);
    _path.line_to(500, 200);
    _path.line_to(350, 200);
    _canvas.update_canvas();
  }

  void on_render(BLContext& ctx) override {
    ctx.fill_all(BLRgba32(0xFF000000u));

    BLPath s;
    s.add_stroked_path(_path, _stroke_options, bl_default_approximation_options);
    ctx.fill_path(s, BLRgba32(0x8F003FAAu));

    if (_show_control) {
      ctx.stroke_path(s, BLRgba32(0xFF0066AAu));
      bl::DemoUtils::render_path_vertices(
        ctx, s, kControlPointRadius, SIZE_MAX, BLRgba32(0x7F007FFFu), BLRgba32(0xFFFFFFFFu));
    }

    ctx.stroke_path(_path, BLRgba32(0xFFFFFFFFu));
    bl::DemoUtils::render_path_vertices(
      ctx, _path, kControlPointRadius, _path_edit.closest_vertex(), BLRgba32(0xFFFFFF00u), BLRgba32(0xFF00FFFFu));
  }
};

// CircleApproxDemo
// ================
//
// Compares the ways a circle can be built from path primitives and shows how
// closely each one matches a true circle:
//
//   - Conic : four 90-degree rational-quadratic arcs with weight cos(45deg).
//             A conic represents a circular arc *exactly*, so this is the
//             reference-quality construction.
//   - Cubic : four 90-degree cubic arcs using the classic kappa handle length
//             (4/3 * tan(pi/8)) - the approximation most libraries use.
//   - Quad  : eight 45-degree or sixteen 22.5-degree quadratic arcs.
class CircleApproxDemo : public DemoCard {
  Q_OBJECT

public:
  enum Approach : int {
    kApproachConic = 0,
    kApproachCubic = 1,
    kApproachQuad8 = 2,
    kApproachQuad16 = 3
  };

  enum SegType : int {
    kSegQuad  = 0,
    kSegConic = 1,
    kSegCubic = 2
  };

  // One primitive of the constructed circle. Endpoint is `p2` for line/quad/
  // conic and `p3` for cubic; `p1` (and `p2` for cubic) are control points and
  // `w` is the conic weight.
  struct Seg {
    int type;
    BLPoint p0, p1, p2, p3;
    double w;
  };

  QComboBox* _approach_select {};
  QComboBox* _mode_select {};
  QCheckBox* _difference_check {};
  QCheckBox* _control_check {};

  bool _show_difference = false;
  bool _show_control = true;

  CircleApproxDemo() {
    _approach_select = new QComboBox();
    _approach_select->addItem("4x conic (exact)", QVariant(int(kApproachConic)));
    _approach_select->addItem("4x cubic (90 deg)", QVariant(int(kApproachCubic)));
    _approach_select->addItem("16x quad (22.5 deg)", QVariant(int(kApproachQuad16)));
    _approach_select->addItem("8x quad (45 deg)", QVariant(int(kApproachQuad8)));

    _mode_select = new QComboBox();
    _mode_select->addItem("Fill", QVariant(0));
    _mode_select->addItem("Stroke", QVariant(1));
    _mode_select->setCurrentIndex(0);

    _difference_check = new QCheckBox("Show Difference");
    _difference_check->setChecked(_show_difference);

    _control_check = new QCheckBox("Show Control Data");
    _control_check->setChecked(_show_control);

    _controls->addWidget(new QLabel("Approach:"), 0, 0, Qt::AlignRight);
    _controls->addWidget(_approach_select, 0, 1);
    _controls->addWidget(new QLabel("Mode:"), 0, 2, Qt::AlignRight);
    _controls->addWidget(_mode_select, 0, 3);
    _controls->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 0, 4);
    _controls->addWidget(_difference_check, 0, 5);
    _controls->addWidget(_control_check, 0, 6);
    _controls->setColumnStretch(4, 1);

    connect(_approach_select, QOverload<int>::of(&QComboBox::activated), this, [this](int) { _canvas.update_canvas(); });
    connect(_mode_select, QOverload<int>::of(&QComboBox::activated), this, [this](int) { _canvas.update_canvas(); });
    connect(_difference_check, &QCheckBox::toggled, this, [this](bool checked) {
      _show_difference = checked;
      _canvas.update_canvas();
    });
    connect(_control_check, &QCheckBox::toggled, this, [this](bool checked) {
      _show_control = checked;
      _canvas.update_canvas();
    });
  }

  double radius() const {
    return std::max(double(std::min(_canvas.image_width(), _canvas.image_height())) / 2.0 - 5.0, 1.0);
  }

  BLPoint center() const {
    return BLPoint(double(_canvas.image_width() / 2), double(_canvas.image_height() / 2u));
  }

  QString name() const override {
    return QStringLiteral("Paths - Circle Approx.");
  }

  QString status() const override {
    int approach = _approach_select->currentData().toInt();
    double r = radius();
    BLPoint c = center();

    std::vector<Seg> segs = build_segments(approach, c, r);
    double err = measure_error(segs, c, r);
    double pct = r > 0.0 ? (err / r) * 100.0 : 0.0;

    return QStringLiteral("Method=%1 | Segments=%2 | R=%3 | MaxError=%4 px (%5%)")
      .arg(_approach_select->currentText())
      .arg(int(segs.size()))
      .arg(r, 0, 'f', 0)
      .arg(err, 0, 'f', 4)
      .arg(pct, 0, 'f', 4);
  }

  // Builds the primitives of a circle for the selected approach.
  std::vector<Seg> build_segments(int approach, BLPoint c, double r) const {
    std::vector<Seg> segs;
    auto pt = [&](double a) { return BLPoint(c.x + r * std::cos(a), c.y + r * std::sin(a)); };

    switch (approach) {
      case kApproachConic: {
        // A rational quadratic with weight cos(half-sweep) is an exact circular
        // arc; four 90-degree arcs give a mathematically perfect circle.
        double w = std::cos(kPI / 4.0);
        double d = r / w; // tangent-intersection distance == r * sqrt(2)
        for (int i = 0; i < 4; i++) {
          double a0 = double(i) * (kPI / 2.0);
          double am = a0 + kPI / 4.0;
          double a1 = a0 + kPI / 2.0;
          Seg s {};
          s.type = kSegConic;
          s.p0 = pt(a0);
          s.p1 = BLPoint(c.x + d * std::cos(am), c.y + d * std::sin(am));
          s.p2 = pt(a1);
          s.w = w;
          segs.push_back(s);
        }
        break;
      }

      case kApproachCubic: {
        // Four 90-degree cubic arcs with the classic kappa handle length.
        double k = (4.0 / 3.0) * (std::sqrt(2.0) - 1.0); // == 4/3 * tan(pi/8)
        for (int i = 0; i < 4; i++) {
          double a0 = double(i) * (kPI / 2.0);
          double a1 = a0 + kPI / 2.0;
          BLPoint p0 = pt(a0);
          BLPoint p3 = pt(a1);
          BLPoint t0(-std::sin(a0), std::cos(a0)); // unit tangent at start
          BLPoint t1(-std::sin(a1), std::cos(a1)); // unit tangent at end
          Seg s {};
          s.type = kSegCubic;
          s.p0 = p0;
          s.p1 = BLPoint(p0.x + k * r * t0.x, p0.y + k * r * t0.y);
          s.p2 = BLPoint(p3.x - k * r * t1.x, p3.y - k * r * t1.y);
          s.p3 = p3;
          segs.push_back(s);
        }
        break;
      }

      case kApproachQuad8: {
        // Eight 45-degree quadratic arcs. The control point is the intersection
        // of the endpoint tangents at r / cos(22.5deg) along the bisector.
        double d = r / std::cos(kPI / 8.0);
        for (int i = 0; i < 8; i++) {
          double a0 = double(i) * (kPI / 4.0);
          double am = a0 + kPI / 8.0;
          double a1 = a0 + kPI / 4.0;
          Seg s {};
          s.type = kSegQuad;
          s.p0 = pt(a0);
          s.p1 = BLPoint(c.x + d * std::cos(am), c.y + d * std::sin(am));
          s.p2 = pt(a1);
          segs.push_back(s);
        }
        break;
      }

      case kApproachQuad16: {
        // Eight 22.5-degree quadratic arcs. The control point is the intersection
        // of the endpoint tangents at r / cos(11.25deg) along the bisector.
        double d = r / std::cos(kPI / 16.0);
        for (int i = 0; i < 16; i++) {
          double a0 = double(i) * (kPI / 8.0);
          double am = a0 + kPI / 16.0;
          double a1 = a0 + kPI / 8.0;
          Seg s {};
          s.type = kSegQuad;
          s.p0 = pt(a0);
          s.p1 = BLPoint(c.x + d * std::cos(am), c.y + d * std::sin(am));
          s.p2 = pt(a1);
          segs.push_back(s);
        }
        break;
      }
    }

    return segs;
  }

  BLPath build_path(const std::vector<Seg>& segs) const {
    BLPath path;
    if (segs.empty())
      return path;

    path.move_to(segs.front().p0);
    for (const Seg& s : segs) {
      switch (s.type) {
        case kSegQuad:  path.quad_to(s.p1, s.p2); break;
        case kSegConic: path.conic_to(s.p1, s.p2, s.w); break;
        default:        path.cubic_to(s.p1, s.p2, s.p3); break;
      }
    }
    path.close();
    return path;
  }

  // Evaluates a segment at parameter `t` in [0, 1].
  static BLPoint seg_point(const Seg& s, double t) {
    double u = 1.0 - t;
    switch (s.type) {
      case kSegQuad: {
        double a = u * u, b = 2.0 * u * t, c = t * t;
        return BLPoint(a * s.p0.x + b * s.p1.x + c * s.p2.x,
                       a * s.p0.y + b * s.p1.y + c * s.p2.y);
      }

      case kSegConic: {
        double a = u * u, b = 2.0 * u * t * s.w, c = t * t;
        double inv = 1.0 / (a + b + c);
        return BLPoint((a * s.p0.x + b * s.p1.x + c * s.p2.x) * inv,
                       (a * s.p0.y + b * s.p1.y + c * s.p2.y) * inv);
      }

      default: { // kSegCubic
        double a = u * u * u, b = 3.0 * u * u * t, c = 3.0 * u * t * t, d = t * t * t;
        return BLPoint(a * s.p0.x + b * s.p1.x + c * s.p2.x + d * s.p3.x,
                       a * s.p0.y + b * s.p1.y + c * s.p2.y + d * s.p3.y);
      }
    }
  }

  // Maximum radial deviation from the ideal circle, sampled along every
  // segment. Endpoints lie exactly on the circle, so the error always peaks
  // somewhere in the interior of a segment.
  double measure_error(const std::vector<Seg>& segs, BLPoint c, double r) const {
    constexpr int kSamples = 100;
    double max_err = 0.0;
    for (const Seg& s : segs) {
      for (int i = 0; i <= kSamples; i++) {
        double t = double(i) / double(kSamples);
        BLPoint p = seg_point(s, t);
        double d = std::hypot(p.x - c.x, p.y - c.y);
        max_err = std::max(max_err, std::abs(d - r));
      }
    }
    return max_err;
  }

  void on_mouse([[maybe_unused]] QMouseEvent* event) override {}

  void on_render(BLContext& ctx) override {
    ctx.fill_all(BLRgba32(0xFF000000u));
    ctx.set_stroke_width(1.2f);

    BLRgba32 shape_color = BLRgba32(0xFFFFFFFFu);
    BLRgba32 reference_color = BLRgba32(0xFFFFFFFFu);

    int approach = _approach_select->currentData().toInt();
    bool fill_mode = (_mode_select->currentData().toInt() == 0);

    double r = radius();
    BLPoint c = center();

    std::vector<Seg> segs = build_segments(approach, c, r);
    BLPath path = build_path(segs);

    // Faint exact circle (fine sin/cos polyline) drawn underneath so that any
    // deviation of the selected construction stands out against it.
    if (_show_difference) {
      BLImage img1(_canvas.image_width(), _canvas.image_height(), BL_FORMAT_XRGB32);
      BLImage img2(_canvas.image_width(), _canvas.image_height(), BL_FORMAT_XRGB32);

      BLContext ctx1(img1);
      BLContext ctx2(img2);

      ctx1.fill_all(BLRgba32(0xFF000000));
      ctx2.fill_all(BLRgba32(0xFF000000));

      BLRgba32 color(0xFFFFFFFFu);

      if (fill_mode) {
        ctx1.fill_path(path, color);
        ctx2.fill_circle(BLCircle(c.x, c.y, r), color);
      }
      else {
        ctx1.stroke_path(path, color);
        ctx2.stroke_circle(BLCircle(c.x, c.y, r), color);
      }

      BLImage diff = ImageUtils::diff_image(img1, img2);
      ctx.blit_image(BLPointI(0, 0), diff);
    }
    else {
      if (fill_mode) {
        ctx.fill_path(path, shape_color);
      }
      else {
        ctx.stroke_path(path, shape_color);
      }
    }

    // Control polygon and on-path / control vertices.
    if (_show_control) {
      BLPath outline;
      outline.add_vertex_outline_of_path(path);
      ctx.set_stroke_width(1.2f);
      ctx.stroke_path(outline, BLRgba32(0x30FFFFFFu));
      bl::DemoUtils::render_path_vertices(ctx, path, kControlPointRadius, SIZE_MAX, BLRgba32(0x2FFFFFFFu), BLRgba32(0xFFFFFFFFu));
    }
  }
};

// ArcShapesDemo
// =============
//
// Showcases the three arc-based figures built from a single `BLArc`:
//
//   - Arc   : `BLPath::add_arc()`   - the open elliptic arc itself (no closing).
//   - Chord : `BLPath::add_chord()` - the arc closed by a straight chord between
//             its endpoints (a circular/elliptic segment).
//   - Pie   : `BLPath::add_pie()`   - the arc closed via two radii to the center
//             (a sector / pie slice).
//
// The center is fixed at the canvas center and the base radius is derived from
// the canvas size (like the Circle Approx. demo). The start angle, sweep angle,
// and the vertical/horizontal radius ratio (to turn the circle into an ellipse)
// are adjustable via sliders. A faint reference ellipse and the center / start /
// end markers make the relationship between the three figures easy to read.
class ArcShapesDemo : public DemoCard {
  Q_OBJECT

public:
  enum Shape : int {
    kShapeArc   = 0,
    kShapeChord = 1,
    kShapePie   = 2
  };

  QComboBox* _shape_select {};
  QComboBox* _mode_select {};
  QSlider* _start_slider {};
  QSlider* _sweep_slider {};
  QSlider* _ratio_slider {};
  QCheckBox* _reference_check {};
  QCheckBox* _control_check {};

  bool _show_reference = true;
  bool _show_control = true;

  static const char* shape_name(Shape shape) {
    switch (shape) {
      case kShapeArc: return "Arc";
      case kShapeChord: return "Chord";
      case kShapePie: return "Pie";
      default: return "<Unknown>";
    }
  }

  ArcShapesDemo() {
    _shape_select = new QComboBox();
    _shape_select->addItem(shape_name(kShapeArc), QVariant(int(kShapeArc)));
    _shape_select->addItem(shape_name(kShapeChord), QVariant(int(kShapeChord)));
    _shape_select->addItem(shape_name(kShapePie), QVariant(int(kShapePie)));
    _shape_select->setCurrentIndex(kShapePie);

    _mode_select = new QComboBox();
    _mode_select->addItem("Fill", QVariant(0));
    _mode_select->addItem("Stroke", QVariant(1));
    _mode_select->setCurrentIndex(0);

    _start_slider = new QSlider(Qt::Horizontal);
    _start_slider->setRange(-3600, 3600);
    _start_slider->setSliderPosition(2000);

    _sweep_slider = new QSlider(Qt::Horizontal);
    _sweep_slider->setRange(-3600, 3600);
    _sweep_slider->setSliderPosition(1400);

    // Vertical radius as a percentage of the base (horizontal) radius: 100% is a
    // circle, lower values squash it into an ellipse that still fits the canvas.
    _ratio_slider = new QSlider(Qt::Horizontal);
    _ratio_slider->setRange(100, 1000);
    _ratio_slider->setSliderPosition(1000);

    _reference_check = new QCheckBox("Show Reference Ellipse");
    _reference_check->setChecked(_show_reference);

    _control_check = new QCheckBox("Show Control Data");
    _control_check->setChecked(_show_control);

    _controls->addWidget(new QLabel("Shape:"), 0, 0, Qt::AlignRight);
    _controls->addWidget(_shape_select, 0, 1);
    _controls->addWidget(new QLabel("Mode:"), 0, 2, Qt::AlignRight);
    _controls->addWidget(_mode_select, 0, 3);
    _controls->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 0, 4);
    _controls->addWidget(_reference_check, 0, 5);
    _controls->addWidget(_control_check, 0, 6);

    _controls->addWidget(new QLabel("Start:"), 1, 0, Qt::AlignRight);
    _controls->addWidget(_start_slider, 1, 1, 1, 6);
    _controls->addWidget(new QLabel("Sweep:"), 2, 0, Qt::AlignRight);
    _controls->addWidget(_sweep_slider, 2, 1, 1, 6);
    _controls->addWidget(new QLabel("Radius Ratio:"), 3, 0, Qt::AlignRight);
    _controls->addWidget(_ratio_slider, 3, 1, 1, 6);
    _controls->setColumnStretch(4, 1);

    connect(_shape_select, QOverload<int>::of(&QComboBox::activated), this, [this](int) { _canvas.update_canvas(); });
    connect(_mode_select, QOverload<int>::of(&QComboBox::activated), this, [this](int) { _canvas.update_canvas(); });
    connect(_start_slider, &QSlider::valueChanged, this, [this](int) { _canvas.update_canvas(); });
    connect(_sweep_slider, &QSlider::valueChanged, this, [this](int) { _canvas.update_canvas(); });
    connect(_ratio_slider, &QSlider::valueChanged, this, [this](int) { _canvas.update_canvas(); });
    connect(_reference_check, &QCheckBox::toggled, this, [this](bool checked) {
      _show_reference = checked;
      _canvas.update_canvas();
    });
    connect(_control_check, &QCheckBox::toggled, this, [this](bool checked) {
      _show_control = checked;
      _canvas.update_canvas();
    });
  }

  // Base (horizontal) radius derived from the canvas size, leaving a margin for
  // the endpoint markers.
  double radius() const {
    return std::max(double(std::min(_canvas.image_width(), _canvas.image_height())) / 2.0 - 20.0, 1.0);
  }

  BLPoint center() const {
    return BLPoint(double(_canvas.image_width() / 2u), double(_canvas.image_height() / 2u));
  }

  double start_angle() const { return (double(_start_slider->value()) / 1800.0) * kPI; }
  double sweep_angle() const { return (double(_sweep_slider->value()) / 1800.0) * kPI; }
  double radius_ratio() const { return double(_ratio_slider->value()) / 1000.0; }

  BLArc arc() const {
    BLPoint c = center();
    double rx = radius();
    double ry = rx * radius_ratio();
    return BLArc(c.x, c.y, rx, ry, start_angle(), sweep_angle());
  }

  QString name() const override {
    return QStringLiteral("Paths - Arc / Chord / Pie");
  }

  QString status() const override {
    return fmt("%s | Start=%g [deg] | Sweep=%g [deg] | Ratio=%.2f | R=%.0f",
      shape_name(static_cast<Shape>(_shape_select->currentData().toInt())),
      double(_start_slider->value()) / 10.0,
      double(_sweep_slider->value()) / 10.0,
      radius_ratio(),
      radius());
  }

  static BLPath build_shape(int shape, const BLArc& a) {
    BLPath p;
    switch (shape) {
      case kShapeArc:   p.add_arc(a); break;
      case kShapeChord: p.add_chord(a); break;
      case kShapePie:   p.add_pie(a); break;
    }
    return p;
  }

  void on_mouse([[maybe_unused]] QMouseEvent* event) override {}

  void on_render(BLContext& ctx) override {
    ctx.fill_all(BLRgba32(0xFF000000u));

    BLArc a = arc();
    BLPoint c(a.cx, a.cy);

    // Faint full ellipse so the arc's placement on the ellipse is obvious.
    if (_show_reference) {
      BLPath ref;
      ref.add_ellipse(BLEllipse(a.cx, a.cy, a.rx, a.ry));
      ctx.set_stroke_width(1.0f);
      ctx.stroke_path(ref, BLRgba32(0x30FFFFFFu));
    }

    int shape = _shape_select->currentData().toInt();
    bool fill_mode = (_mode_select->currentData().toInt() == 0);

    BLPath path = build_shape(shape, a);

    // An arc is an open curve - filling it would implicitly close it with a
    // straight chord (i.e. look like a chord), so it is always stroked.
    if (shape == kShapeArc || !fill_mode) {
      ctx.set_stroke_width(2.0f);
      ctx.stroke_path(path, BLRgba32(0xFFFFFFFFu));
    }
    else {
      ctx.fill_path(path, BLRgba32(0x80003FCAu));
      ctx.set_stroke_width(1.5f);
      ctx.stroke_path(path, BLRgba32(0xFF3F9FFFu));
    }

    if (_show_control) {
      BLPath outline;
      outline.add_vertex_outline_of_path(path);
      ctx.set_stroke_width(1.0f);
      ctx.stroke_path(outline, BLRgba32(0x30FFFFFFu));
      bl::DemoUtils::render_path_vertices(
        ctx, path, kControlPointRadius, SIZE_MAX, BLRgba32(0x2FFFFFFFu), BLRgba32(0xFFFFFFFFu));
    }

    // Center, start, and end markers.
    BLPoint sp(a.cx + a.rx * std::cos(a.start), a.cy + a.ry * std::sin(a.start));
    BLPoint ep(a.cx + a.rx * std::cos(a.start + a.sweep), a.cy + a.ry * std::sin(a.start + a.sweep));

    ctx.fill_circle(c.x, c.y, kControlPointRadius, BLRgba32(0xFFFFFF00u));
    ctx.fill_circle(sp.x, sp.y, kControlPointRadius, BLRgba32(0xFF3FFF3Fu));
    ctx.fill_circle(ep.x, ep.y, kControlPointRadius, BLRgba32(0xFFFF5F3Fu));
  }
};


// AppWindow
// =========
//
// Host window: a navigation panel on the left selects which demo is shown in
// the content area on the right (a stacked view of all demo cards).
class AppWindow : public QWidget {
  Q_OBJECT

public:
  QListWidget* _nav {};
  QStackedWidget* _stack {};
  QLabel* _status_left {};
  QLabel* _status_right {};
  std::vector<DemoCard*> _cards;

  AppWindow() {
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    QWidget* body = new QWidget();
    QHBoxLayout* hBox = new QHBoxLayout(body);
    hBox->setContentsMargins(0, 0, 0, 0);
    hBox->setSpacing(0);

    // Left navigation panel: a caption header sitting above the demo list.
    QWidget* panel = new QWidget();
    panel->setObjectName("navPanel");
    panel->setFixedWidth(180);

    QVBoxLayout* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    QLabel* caption = new QLabel("Select Page");
    caption->setObjectName("navCaption");

    _nav = new QListWidget();
    _nav->setObjectName("navList");
    _nav->setFocusPolicy(Qt::NoFocus);

    panelLayout->addWidget(caption);
    panelLayout->addWidget(_nav, 1);

    panel->setStyleSheet(QStringLiteral(R"(
      #navPanel {
        background-color: #252526;
      }
      #navCaption {
        padding: 5px 6px;
        color: #FFFFFF;
        font-weight: bold;
        letter-spacing: 1px;
        background-color: #333337;
        border-bottom: 1px solid #1A1A1B;
      }
      #navList {
        border: none;
        outline: 0;
        background-color: #252526;
        color: #D4D4D4;
      }
      #navList::item {
        padding: 5px 6px;
      }
      #navList::item:hover {
        background-color: #2A2D2E;
      }
      #navList::item:selected {
        background-color: #094771;
        color: #FFFFFF;
      }
    )"));

    _stack = new QStackedWidget();

    hBox->addWidget(panel);
    hBox->addWidget(_stack, 1);

    // Bottom status bar: demo info on the left, mouse position on the right.
    QWidget* status_bar = new QWidget();
    status_bar->setObjectName("statusBar");

    QHBoxLayout* status_layout = new QHBoxLayout(status_bar);
    status_layout->setContentsMargins(0, 0, 0, 0);
    status_layout->setSpacing(0);

    _status_left = new QLabel();
    _status_left->setObjectName("statusInfo");
    _status_right = new QLabel();
    _status_right->setObjectName("statusInfo");

    status_layout->addWidget(_status_left);
    status_layout->addStretch(1);
    status_layout->addWidget(_status_right);

    status_bar->setStyleSheet(QStringLiteral(R"(
      #statusBar {
        background-color: #252526;
        border-top: 1px solid #1A1A1B;
      }
      #statusInfo {
        color: #D4D4D4;
        padding: 3px 8px;
      }
    )"));

    outer->addWidget(body, 1);
    outer->addWidget(status_bar);

    add_demo(new CurvesDemo());
    add_demo(new CircleApproxDemo());
    add_demo(new EllipticArcDemo());
    add_demo(new ArcShapesDemo());
    add_demo(new FitToDemo());
    add_demo(new HitTestDemo());
    add_demo(new StrokeDemo());

    connect(_nav, &QListWidget::currentRowChanged, this, [this](int row) {
      if (row >= 0) {
        _stack->setCurrentIndex(row);
        update_ui();
      }
    });

    _nav->setCurrentRow(0);
    update_ui();
  }

  void add_demo(DemoCard* card) {
    card->on_status_changed = [this]() { update_ui(); };
    _cards.push_back(card);
    _stack->addWidget(card);
    _nav->addItem(card->name());
  }

  DemoCard* current_card() const {
    int i = _stack->currentIndex();
    return (i >= 0 && i < int(_cards.size())) ? _cards[size_t(i)] : nullptr;
  }

  // Refresh the title (demo name only) and the status bar (canvas size, render
  // time, demo status, and the live mouse position).
  void update_ui() {
    DemoCard* card = current_card();
    if (!card) {
      return;
    }

    QString window_wh = fmt(" [%d x %d]",
      card->_canvas.image_width(),
      card->_canvas.image_height());

    QString title = QString("Blend2D - ") + card->name() + window_wh;

    if (title != windowTitle()) {
      setWindowTitle(title);
    }


    QString status = card->status();

    double render_time = card->_canvas.average_render_time();
    if (render_time > 0.0) {
      status += fmt(" | %.2f ms", render_time);
    }

    _status_left->setText(status);
    _status_right->setText(card->_mouse_valid
      ? fmt("[%d, %d]", qRound(card->_mouse_pos.x), qRound(card->_mouse_pos.y))
      : QString());
  }
};

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  AppWindow win;

  win.resize(QSize(820, 620));
  win.show();

  return app.exec();
}

#include "bl_demo_app.moc"
