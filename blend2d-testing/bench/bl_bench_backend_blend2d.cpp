// This file is part of Blend2D project <https://blend2d.com>
//
// See LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/blend2d.h>
#include <blend2d-testing/bench/bl_bench_app.h>
#include <blend2d-testing/bench/bl_bench_backend.h>

#include <stdio.h>

namespace blbench {

class Blend2DModule : public Backend {
public:

  BLContext _context;
  uint32_t _threadCount;
  uint32_t _cpu_features;

  // Initialized by before_run().
  BLGradientType _gradient_type;
  BLExtendMode _gradient_extend;

  // Construction & Destruction
  // --------------------------

  explicit Blend2DModule(uint32_t thread_count = 0, uint32_t cpu_features = 0);
  ~Blend2DModule() override;

  // Interface
  // ---------

  void serialize_info(JSONBuilder& json) const override;

  bool supports_comp_op(BLCompOp comp_op) const override;
  bool supports_style(StyleKind style) const override;

  void before_run() override;
  void flush() override;
  void after_run() override;

  template<typename RectT>
  inline const BLVar& setup_style(const RectT& rect, StyleKind style, BLGradient& gradient, BLPattern& pattern);

  void render_rect_a(RenderOp op) override;
  void render_rect_f(RenderOp op) override;
  void render_rect_rotated(RenderOp op) override;
  void render_round_f(RenderOp op) override;
  void render_round_rotated(RenderOp op) override;
  void render_polygon(RenderOp op, uint32_t complexity) override;
  void render_shape(RenderOp op, ShapeData shape) override;
};

Blend2DModule::Blend2DModule(uint32_t thread_count, uint32_t cpu_features) {
  _threadCount = thread_count;
  _cpu_features = cpu_features;

  const char* feature = nullptr;

  if (_cpu_features == 0xFFFFFFFFu) {
    feature = "[NO JIT]";
  }
  else {
    if (_cpu_features & BL_RUNTIME_CPU_FEATURE_X86_SSE2  ) feature = "[SSE2]";
    if (_cpu_features & BL_RUNTIME_CPU_FEATURE_X86_SSE3  ) feature = "[SSE3]";
    if (_cpu_features & BL_RUNTIME_CPU_FEATURE_X86_SSSE3 ) feature = "[SSSE3]";
    if (_cpu_features & BL_RUNTIME_CPU_FEATURE_X86_SSE4_1) feature = "[SSE4.1]";
    if (_cpu_features & BL_RUNTIME_CPU_FEATURE_X86_SSE4_2) feature = "[SSE4.2]";
    if (_cpu_features & BL_RUNTIME_CPU_FEATURE_X86_AVX   ) feature = "[AVX]";
    if (_cpu_features & BL_RUNTIME_CPU_FEATURE_X86_AVX2  ) feature = "[AVX2]";
    if (_cpu_features & BL_RUNTIME_CPU_FEATURE_X86_AVX512) feature = "[AVX512]";
  }

  if (!_threadCount)
    snprintf(_name, sizeof(_name), "Blend2D ST%s%s", feature ? " " : "", feature ? feature : "");
  else
    snprintf(_name, sizeof(_name), "Blend2D %uT%s%s", _threadCount, feature ? " " : "", feature ? feature : "");
}

Blend2DModule::~Blend2DModule() {}

void Blend2DModule::serialize_info(JSONBuilder& json) const {
  BLRuntimeBuildInfo build_info;
  BLRuntime::query_build_info(&build_info);

  json.before_record()
      .add_key("version")
      .add_stringf("%u.%u.%u", build_info.major_version, build_info.minor_version, build_info.patch_version);
}

template<typename RectT>
inline const BLVar& Blend2DModule::setup_style(const RectT& rect, StyleKind style, BLGradient& gradient, BLPattern& pattern) {
  if (style <= StyleKind::kConic) {
    BLRgba32 c0(_rnd_color.next_rgba32());
    BLRgba32 c1(_rnd_color.next_rgba32());
    BLRgba32 c2(_rnd_color.next_rgba32());

    switch (style) {
      case StyleKind::kLinearPad:
      case StyleKind::kLinearRepeat:
      case StyleKind::kLinearReflect: {
        BLLinearGradientValues values {};
        values.x0 = rect.x + rect.w * 0.2;
        values.y0 = rect.y + rect.h * 0.2;
        values.x1 = rect.x + rect.w * 0.8;
        values.y1 = rect.y + rect.h * 0.8;

        gradient.set_values(values);
        gradient.reset_stops();
        gradient.add_stop(0.0, c0);
        gradient.add_stop(0.5, c1);
        gradient.add_stop(1.0, c2);
        break;
      }

      case StyleKind::kRadialPad:
      case StyleKind::kRadialRepeat:
      case StyleKind::kRadialReflect: {
        BLRadialGradientValues values {};
        values.x0 = rect.x + (rect.w / 2);
        values.y0 = rect.y + (rect.h / 2);
        values.r0 = (rect.w + rect.h) / 4;
        values.x1 = values.x0 - values.r0 / 2.0;
        values.y1 = values.y0 - values.r0 / 2.0;

        gradient.set_values(values);
        gradient.reset_stops();
        gradient.add_stop(0.0, c0);
        gradient.add_stop(0.5, c1);
        gradient.add_stop(1.0, c2);
        break;
      }

      default: {
        BLConicGradientValues values {};
        values.x0 = rect.x + (rect.w / 2);
        values.y0 = rect.y + (rect.h / 2);
        values.angle = 0;
        values.repeat = 1;

        gradient.set_values(values);
        gradient.reset_stops();

        gradient.add_stop(0.00, c0);
        gradient.add_stop(0.33, c1);
        gradient.add_stop(0.66, c2);
        gradient.add_stop(1.00, c0);
        break;
      }
    }
    return reinterpret_cast<const BLVar&>(gradient);
  }
  else {
    pattern.create(_sprites[nextSpriteId()], BL_EXTEND_MODE_REPEAT, BLMatrix2D::make_translation(rect.x, rect.y));
    return reinterpret_cast<const BLVar&>(pattern);
  }
}

bool Blend2DModule::supports_comp_op(BLCompOp comp_op) const {
  // This backend supports all composition operators.
  (void)comp_op;

  return true;
}

bool Blend2DModule::supports_style(StyleKind style) const {
  // This backend supports all styles.
  (void)style;

  return true;
}

void Blend2DModule::before_run() {
  int w = int(_params.screen_w);
  int h = int(_params.screen_h);
  StyleKind style = _params.style;

  BLContextCreateInfo create_info {};
  create_info.thread_count = _threadCount;

  if (_cpu_features == 0xFFFFFFFFu) {
    create_info.flags = BL_CONTEXT_CREATE_FLAG_DISABLE_JIT;
  }
  else  if (_cpu_features) {
    create_info.flags = BL_CONTEXT_CREATE_FLAG_ISOLATED_JIT_RUNTIME |
                        BL_CONTEXT_CREATE_FLAG_OVERRIDE_CPU_FEATURES;
    create_info.cpu_features = _cpu_features;
  }

  _surface.create(w, h, _params.format);
  _context.begin(_surface, &create_info);

  _context.set_comp_op(BL_COMP_OP_SRC_COPY);
  _context.fill_all(BLRgba32(0x00000000));

  _context.set_comp_op(_params.comp_op);
  _context.set_stroke_width(_params.stroke_width);

  _context.set_pattern_quality(
    _params.style == StyleKind::kPatternNN
      ? BL_PATTERN_QUALITY_NEAREST
      : BL_PATTERN_QUALITY_BILINEAR);

  // Setup globals.
  _gradient_type = BL_GRADIENT_TYPE_LINEAR;
  _gradient_extend = BL_EXTEND_MODE_PAD;

  switch (style) {
    case StyleKind::kLinearPad    : _gradient_type = BL_GRADIENT_TYPE_LINEAR; _gradient_extend = BL_EXTEND_MODE_PAD    ; break;
    case StyleKind::kLinearRepeat : _gradient_type = BL_GRADIENT_TYPE_LINEAR; _gradient_extend = BL_EXTEND_MODE_REPEAT ; break;
    case StyleKind::kLinearReflect: _gradient_type = BL_GRADIENT_TYPE_LINEAR; _gradient_extend = BL_EXTEND_MODE_REFLECT; break;
    case StyleKind::kRadialPad    : _gradient_type = BL_GRADIENT_TYPE_RADIAL; _gradient_extend = BL_EXTEND_MODE_PAD    ; break;
    case StyleKind::kRadialRepeat : _gradient_type = BL_GRADIENT_TYPE_RADIAL; _gradient_extend = BL_EXTEND_MODE_REPEAT ; break;
    case StyleKind::kRadialReflect: _gradient_type = BL_GRADIENT_TYPE_RADIAL; _gradient_extend = BL_EXTEND_MODE_REFLECT; break;
    case StyleKind::kConic        : _gradient_type = BL_GRADIENT_TYPE_CONIC; break;

    default:
      break;
  }

  _context.flush(BL_CONTEXT_FLUSH_SYNC);
}

void Blend2DModule::flush() {
  _context.flush(BL_CONTEXT_FLUSH_SYNC);
}

void Blend2DModule::after_run() {
  _context.end();
}

void Blend2DModule::render_rect_a(RenderOp op) {
  BLSizeI bounds(int(_params.screen_w), int(_params.screen_h));
  StyleKind style = _params.style;
  int wh = int(_params.shape_size);

  if (style == StyleKind::kSolid) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRectI rect(_rnd_coord.next_rect_i(bounds, wh, wh));
      BLRgba32 color(_rnd_color.next_rgba32());

      if (op == RenderOp::kStroke)
        _context.stroke_rect(BLRect(rect.x, rect.y, rect.w, rect.h), color);
      else
        _context.fill_rect(rect, color);
    }
  }
  else if ((style == StyleKind::kPatternNN || style == StyleKind::kPatternBI) && op != RenderOp::kStroke) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRectI rect(_rnd_coord.next_rect_i(bounds, wh, wh));
      _context.blit_image(BLPointI(rect.x, rect.y), _sprites[nextSpriteId()]);
    }
  }
  else {
    BLPattern pattern;
    BLGradient gradient(_gradient_type);
    gradient.set_extend_mode(_gradient_extend);

    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRectI rect(_rnd_coord.next_rect_i(bounds, wh, wh));
      const auto& obj = setup_style(rect, style, gradient, pattern);

      if (op == RenderOp::kStroke)
        _context.stroke_rect(BLRect(rect.x, rect.y, rect.w, rect.h), obj);
      else
        _context.fill_rect(rect, obj);
    }
  }
}

void Blend2DModule::render_rect_f(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  double wh = double(_params.shape_size);

  if (style == StyleKind::kSolid) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
      BLRgba32 color(_rnd_color.next_rgba32());

      if (op == RenderOp::kStroke)
        _context.stroke_rect(rect, color);
      else
        _context.fill_rect(rect, color);
    }
  }
  else if ((style == StyleKind::kPatternNN || style == StyleKind::kPatternBI) && op != RenderOp::kStroke) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
      _context.blit_image(BLPoint(rect.x, rect.y), _sprites[nextSpriteId()]);
    }
  }
  else {
    BLPattern pattern;
    BLGradient gradient(_gradient_type);
    gradient.set_extend_mode(_gradient_extend);

    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
      const auto& obj = setup_style(rect, style, gradient, pattern);

      if (op == RenderOp::kStroke)
        _context.stroke_rect(rect, obj);
      else
        _context.fill_rect(rect, obj);
    }
  }
}

void Blend2DModule::render_rect_rotated(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double cx = double(_params.screen_w) * 0.5;
  double cy = double(_params.screen_h) * 0.5;
  double wh = double(_params.shape_size);
  double angle = 0.0;

  if (style == StyleKind::kSolid) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
      BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
      BLRgba32 color(_rnd_color.next_rgba32());

      _context.rotate(angle, BLPoint(cx, cy));

      if (op == RenderOp::kStroke)
        _context.stroke_rect(rect, color);
      else
        _context.fill_rect(rect, color);

      _context.reset_transform();
    }
  }
  else if ((style == StyleKind::kPatternNN || style == StyleKind::kPatternBI) && op != RenderOp::kStroke) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
      BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));

      _context.save();
      _context.rotate(angle, BLPoint(cx, cy));
      _context.blit_image(BLPoint(rect.x, rect.y), _sprites[nextSpriteId()]);
      _context.restore();
    }
  }
  else {
    BLPattern pattern;
    BLGradient gradient(_gradient_type);
    gradient.set_extend_mode(_gradient_extend);

    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
      BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
      const auto& obj = setup_style(rect, style, gradient, pattern);

      _context.save();
      _context.rotate(angle, BLPoint(cx, cy));

      if (op == RenderOp::kStroke)
        _context.stroke_rect(rect, obj);
      else
        _context.fill_rect(rect, obj);

      _context.restore();
    }
  }
}

void Blend2DModule::render_round_f(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  double wh = double(_params.shape_size);

  if (style == StyleKind::kSolid) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      double radius = _rnd_extra.next_double(4.0, 40.0);
      BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
      BLRoundRect round(rect, radius);

      BLRgba32 color(_rnd_color.next_rgba32());
      if (op == RenderOp::kStroke)
        _context.stroke_round_rect(round, color);
      else
        _context.fill_round_rect(round, color);
    }
  }
  else {
    BLPattern pattern;
    BLGradient gradient(_gradient_type);
    gradient.set_extend_mode(_gradient_extend);

    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      double radius = _rnd_extra.next_double(4.0, 40.0);
      BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
      BLRoundRect round(rect, radius);

      const auto& obj = setup_style(rect, style, gradient, pattern);
      if (op == RenderOp::kStroke)
        _context.stroke_round_rect(round, obj);
      else
        _context.fill_round_rect(round, obj);
    }
  }
}

void Blend2DModule::render_round_rotated(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double cx = double(_params.screen_w) * 0.5;
  double cy = double(_params.screen_h) * 0.5;
  double wh = double(_params.shape_size);
  double angle = 0.0;

  if (style == StyleKind::kSolid) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
      double radius = _rnd_extra.next_double(4.0, 40.0);
      BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
      BLRoundRect round(rect, radius);

      _context.rotate(angle, BLPoint(cx, cy));
      BLRgba32 color(_rnd_color.next_rgba32());

      if (op == RenderOp::kStroke)
        _context.stroke_round_rect(round, color);
      else
        _context.fill_round_rect(round, color);

      _context.reset_transform();
    }
  }
  else {
    BLPattern pattern;
    BLGradient gradient(_gradient_type);
    gradient.set_extend_mode(_gradient_extend);

    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
      double radius = _rnd_extra.next_double(4.0, 40.0);
      BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
      BLRoundRect round(rect, radius);

      const auto& obj = setup_style(rect, style, gradient, pattern);

      _context.save();
      _context.rotate(angle, BLPoint(cx, cy));

      if (op == RenderOp::kStroke)
        _context.stroke_round_rect(round, obj);
      else
        _context.fill_round_rect(round, obj);

      _context.restore();
    }
  }
}

void Blend2DModule::render_polygon(RenderOp op, uint32_t complexity) {
  static constexpr uint32_t kPointCapacity = 128;

  if (complexity > kPointCapacity)
    return;

  BLSizeI bounds(
    int(_params.screen_w - _params.shape_size),
    int(_params.screen_h - _params.shape_size));
  StyleKind style = _params.style;
  double wh = double(_params.shape_size);

  BLPoint points[kPointCapacity];
  BLPattern pattern;
  BLGradient gradient(_gradient_type);

  _context.set_fill_rule(op == RenderOp::kFillEvenOdd ? BL_FILL_RULE_EVEN_ODD : BL_FILL_RULE_NON_ZERO);
  gradient.set_extend_mode(_gradient_extend);

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLPoint base(_rnd_coord.nextPoint(bounds));

    for (uint32_t p = 0; p < complexity; p++) {
      double x = _rnd_coord.next_double(base.x, base.x + wh);
      double y = _rnd_coord.next_double(base.y, base.y + wh);
      points[p].reset(x, y);
    }

    if (style == StyleKind::kSolid) {
      BLRgba32 color = _rnd_color.next_rgba32();
      if (op == RenderOp::kStroke)
        _context.stroke_polygon(points, complexity, color);
      else
        _context.fill_polygon(points, complexity, color);
    }
    else {
      BLRect rect(base.x, base.y, wh, wh);
      const auto& obj = setup_style(rect, style, gradient, pattern);

      if (op == RenderOp::kStroke)
        _context.stroke_polygon(points, complexity, obj);
      else
        _context.fill_polygon(points, complexity, obj);
    }
  }
}

void Blend2DModule::render_shape(RenderOp op, ShapeData shape) {
  BLSizeI bounds(
    int(_params.screen_w - _params.shape_size),
    int(_params.screen_h - _params.shape_size));
  StyleKind style = _params.style;
  double wh = double(_params.shape_size);

  BLPath path;
  ShapeIterator it(shape);

  while (it.has_command()) {
    if (it.is_move_to()) {
      path.move_to(it.vertex(0));
    }
    else if (it.is_line_to()) {
      path.line_to(it.vertex(0));
    }
    else if (it.is_quad_to()) {
      path.quad_to(it.vertex(0), it.vertex(1));
    }
    else if (it.is_cubic_to()) {
      path.cubic_to(it.vertex(0), it.vertex(1), it.vertex(2));
    }
    else {
      path.close();
    }
    it.next();
  }

  path.transform(BLMatrix2D::make_scaling(wh, wh));

  BLPattern pattern;
  BLGradient gradient(_gradient_type);

  _context.set_fill_rule(op == RenderOp::kFillEvenOdd ? BL_FILL_RULE_EVEN_ODD : BL_FILL_RULE_NON_ZERO);
  gradient.set_extend_mode(_gradient_extend);

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLPoint base(_rnd_coord.nextPoint(bounds));

    if (style == StyleKind::kSolid) {
      BLRgba32 color = _rnd_color.next_rgba32();
      if (op == RenderOp::kStroke)
        _context.stroke_path(base, path, color);
      else
        _context.fill_path(base, path, color);
    }
    else {
      BLRect rect(base.x, base.y, wh, wh);
      const auto& obj = setup_style(rect, style, gradient, pattern);

      if (op == RenderOp::kStroke)
        _context.stroke_path(base, path, obj);
      else
        _context.fill_path(base, path, obj);
    }
  }
}

Backend* create_blend2d_backend(uint32_t thread_count, uint32_t cpu_features) {
  return new Blend2DModule(thread_count, cpu_features);
}

} // {blbench}
