// This file is part of Blend2D project <https://blend2d.com>
//
// See LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifdef BL_BENCH_ENABLE_CAIRO

#include "bl_bench_app.h"
#include "bl_bench_backend.h"

#include <algorithm>
#include <cairo.h>

namespace blbench {

static inline double u8_to_unit(int x) {
  constexpr double kDiv255 = 1.0 / 255.0;
  return double(x) * kDiv255;
}

static uint32_t to_cairo_format(uint32_t format) {
  switch (format) {
    case BL_FORMAT_PRGB32: return CAIRO_FORMAT_ARGB32;
    case BL_FORMAT_XRGB32: return CAIRO_FORMAT_RGB24;
    case BL_FORMAT_A8    : return CAIRO_FORMAT_A8;
    default:
      return 0xFFFFFFFFu;
  }
}

static uint32_t to_cairo_operator(uint32_t comp_op) {
  switch (comp_op) {
    case BL_COMP_OP_SRC_OVER   : return CAIRO_OPERATOR_OVER;
    case BL_COMP_OP_SRC_COPY   : return CAIRO_OPERATOR_SOURCE;
    case BL_COMP_OP_SRC_IN     : return CAIRO_OPERATOR_IN;
    case BL_COMP_OP_SRC_OUT    : return CAIRO_OPERATOR_OUT;
    case BL_COMP_OP_SRC_ATOP   : return CAIRO_OPERATOR_ATOP;
    case BL_COMP_OP_DST_OVER   : return CAIRO_OPERATOR_DEST_OVER;
    case BL_COMP_OP_DST_COPY   : return CAIRO_OPERATOR_DEST;
    case BL_COMP_OP_DST_IN     : return CAIRO_OPERATOR_DEST_IN;
    case BL_COMP_OP_DST_OUT    : return CAIRO_OPERATOR_DEST_OUT;
    case BL_COMP_OP_DST_ATOP   : return CAIRO_OPERATOR_DEST_ATOP;
    case BL_COMP_OP_XOR        : return CAIRO_OPERATOR_XOR;
    case BL_COMP_OP_CLEAR      : return CAIRO_OPERATOR_CLEAR;
    case BL_COMP_OP_PLUS       : return CAIRO_OPERATOR_ADD;
    case BL_COMP_OP_MULTIPLY   : return CAIRO_OPERATOR_MULTIPLY;
    case BL_COMP_OP_SCREEN     : return CAIRO_OPERATOR_SCREEN;
    case BL_COMP_OP_OVERLAY    : return CAIRO_OPERATOR_OVERLAY;
    case BL_COMP_OP_DARKEN     : return CAIRO_OPERATOR_DARKEN;
    case BL_COMP_OP_LIGHTEN    : return CAIRO_OPERATOR_LIGHTEN;
    case BL_COMP_OP_COLOR_DODGE: return CAIRO_OPERATOR_COLOR_DODGE;
    case BL_COMP_OP_COLOR_BURN : return CAIRO_OPERATOR_COLOR_BURN;
    case BL_COMP_OP_HARD_LIGHT : return CAIRO_OPERATOR_HARD_LIGHT;
    case BL_COMP_OP_SOFT_LIGHT : return CAIRO_OPERATOR_SOFT_LIGHT;
    case BL_COMP_OP_DIFFERENCE : return CAIRO_OPERATOR_DIFFERENCE;
    case BL_COMP_OP_EXCLUSION  : return CAIRO_OPERATOR_EXCLUSION;

    default:
      return 0xFFFFFFFFu;
  }
}

static void round_rect(cairo_t* ctx, const BLRect& rect, double radius) {
  double rw2 = rect.w * 0.5;
  double rh2 = rect.h * 0.5;

  double rx = std::min(bl_abs(radius), rw2);
  double ry = std::min(bl_abs(radius), rh2);

  double kappa_inv = 1 - 0.551915024494;
  double kx = rx * kappa_inv;
  double ky = ry * kappa_inv;

  double x0 = rect.x;
  double y0 = rect.y;
  double x1 = rect.x + rect.w;
  double y1 = rect.y + rect.h;

  cairo_move_to(ctx, x0 + rx, y0);
  cairo_line_to(ctx, x1 - rx, y0);
  cairo_curve_to(ctx, x1 - kx, y0, x1, y0 + ky, x1, y0 + ry);

  cairo_line_to(ctx, x1, y1 - ry);
  cairo_curve_to(ctx, x1, y1 - ky, x1 - kx, y1, x1 - rx, y1);

  cairo_line_to(ctx, x0 + rx, y1);
  cairo_curve_to(ctx, x0 + kx, y1, x0, y1 - ky, x0, y1 - ry);

  cairo_line_to(ctx, x0, y0 + ry);
  cairo_curve_to(ctx, x0, y0 + ky, x0 + kx, y0, x0 + rx, y0);

  cairo_close_path(ctx);
}

struct CairoModule : public Backend {
  cairo_surface_t* _cairo_surface {};
  cairo_surface_t* _cairo_sprites[kBenchNumSprites] {};
  cairo_t* _cairo_ctx {};

  // Initialized by before_run().
  uint32_t _pattern_extend {};
  uint32_t _pattern_filter {};

  CairoModule();
  ~CairoModule() override;

  void serialize_info(JSONBuilder& json) const override;

  template<typename RectT>
  void setup_style(StyleKind style, const RectT& rect);

  bool supports_comp_op(BLCompOp comp_op) const override;
  bool supports_style(StyleKind style) const override;

  void before_run() override;
  void flush() override;
  void after_run() override;

  void render_rect_a(RenderOp op) override;
  void render_rect_f(RenderOp op) override;
  void render_rect_rotated(RenderOp op) override;
  void render_round_f(RenderOp op) override;
  void render_round_rotated(RenderOp op) override;
  void render_polygon(RenderOp op, uint32_t complexity) override;
  void render_shape(RenderOp op, ShapeData shape) override;
};

CairoModule::CairoModule() {
  strcpy(_name, "Cairo");
}
CairoModule::~CairoModule() {}

void CairoModule::serialize_info(JSONBuilder& json) const {
  json.before_record()
      .add_key("version")
      .add_string(cairo_version_string());
}

template<typename RectT>
void CairoModule::setup_style(StyleKind style, const RectT& rect) {
  switch (style) {
    case StyleKind::kSolid: {
      BLRgba32 c(_rnd_color.next_rgba32());
      cairo_set_source_rgba(_cairo_ctx, u8_to_unit(c.r()), u8_to_unit(c.g()), u8_to_unit(c.b()), u8_to_unit(c.a()));
      return;
    }

    case StyleKind::kLinearPad:
    case StyleKind::kLinearRepeat:
    case StyleKind::kLinearReflect:
    case StyleKind::kRadialPad:
    case StyleKind::kRadialRepeat:
    case StyleKind::kRadialReflect: {
      double x = rect.x;
      double y = rect.y;

      BLRgba32 c0(_rnd_color.next_rgba32());
      BLRgba32 c1(_rnd_color.next_rgba32());
      BLRgba32 c2(_rnd_color.next_rgba32());

      cairo_pattern_t* pattern {};
      if (style < StyleKind::kRadialPad) {
        // Linear gradient.
        double x0 = rect.x + rect.w * 0.2;
        double y0 = rect.y + rect.h * 0.2;
        double x1 = rect.x + rect.w * 0.8;
        double y1 = rect.y + rect.h * 0.8;
        pattern = cairo_pattern_create_linear(x0, y0, x1, y1);

        cairo_pattern_add_color_stop_rgba(pattern, 0.0, u8_to_unit(c0.r()), u8_to_unit(c0.g()), u8_to_unit(c0.b()), u8_to_unit(c0.a()));
        cairo_pattern_add_color_stop_rgba(pattern, 0.5, u8_to_unit(c1.r()), u8_to_unit(c1.g()), u8_to_unit(c1.b()), u8_to_unit(c1.a()));
        cairo_pattern_add_color_stop_rgba(pattern, 1.0, u8_to_unit(c2.r()), u8_to_unit(c2.g()), u8_to_unit(c2.b()), u8_to_unit(c2.a()));
      }
      else {
        // Radial gradient.
        x += double(rect.w) / 2.0;
        y += double(rect.h) / 2.0;

        double r = double(rect.w + rect.h) / 4.0;
        pattern = cairo_pattern_create_radial(x, y, r, x - r / 2, y - r / 2, 0.0);

        // Color stops in Cairo's radial gradient are reverse to Blend/Qt.
        cairo_pattern_add_color_stop_rgba(pattern, 0.0, u8_to_unit(c2.r()), u8_to_unit(c2.g()), u8_to_unit(c2.b()), u8_to_unit(c2.a()));
        cairo_pattern_add_color_stop_rgba(pattern, 0.5, u8_to_unit(c1.r()), u8_to_unit(c1.g()), u8_to_unit(c1.b()), u8_to_unit(c1.a()));
        cairo_pattern_add_color_stop_rgba(pattern, 1.0, u8_to_unit(c0.r()), u8_to_unit(c0.g()), u8_to_unit(c0.b()), u8_to_unit(c0.a()));
      }

      cairo_pattern_set_extend(pattern, cairo_extend_t(_pattern_extend));
      cairo_set_source(_cairo_ctx, pattern);
      cairo_pattern_destroy(pattern);
      return;
    }

    case StyleKind::kPatternNN:
    case StyleKind::kPatternBI: {
      // Matrix associated with cairo_pattern_t is inverse to Blend/Qt.
      cairo_matrix_t matrix;
      cairo_matrix_init_translate(&matrix, -rect.x, -rect.y);

      cairo_pattern_t* pattern = cairo_pattern_create_for_surface(_cairo_sprites[nextSpriteId()]);
      cairo_pattern_set_matrix(pattern, &matrix);
      cairo_pattern_set_extend(pattern, cairo_extend_t(_pattern_extend));
      cairo_pattern_set_filter(pattern, cairo_filter_t(_pattern_filter));

      cairo_set_source(_cairo_ctx, pattern);
      cairo_pattern_destroy(pattern);
      return;
    }

    default: {
      return;
    }
  }
}

bool CairoModule::supports_comp_op(BLCompOp comp_op) const {
  return to_cairo_operator(comp_op) != 0xFFFFFFFFu;
}

bool CairoModule::supports_style(StyleKind style) const {
  return style == StyleKind::kSolid         ||
         style == StyleKind::kLinearPad     ||
         style == StyleKind::kLinearRepeat  ||
         style == StyleKind::kLinearReflect ||
         style == StyleKind::kRadialPad     ||
         style == StyleKind::kRadialRepeat  ||
         style == StyleKind::kRadialReflect ||
         style == StyleKind::kPatternNN     ||
         style == StyleKind::kPatternBI     ;
}

void CairoModule::before_run() {
  int w = int(_params.screen_w);
  int h = int(_params.screen_h);
  StyleKind style = _params.style;

  // Initialize the sprites.
  for (uint32_t i = 0; i < kBenchNumSprites; i++) {
    const BLImage& sprite = _sprites[i];

    BLImageData sprite_data;
    sprite.get_data(&sprite_data);

    int stride = int(sprite_data.stride);
    int format = to_cairo_format(sprite_data.format);
    unsigned char* pixels = static_cast<unsigned char*>(sprite_data.pixel_data);

    cairo_surface_t* cairo_sprite = cairo_image_surface_create_for_data(
      pixels, cairo_format_t(format), sprite_data.size.w, sprite_data.size.h, stride);

    _cairo_sprites[i] = cairo_sprite;
  }

  // Initialize the surface and the context.
  {
    BLImageData surface_data;
    _surface.create(w, h, _params.format);
    _surface.make_mutable(&surface_data);

    int stride = int(surface_data.stride);
    int format = to_cairo_format(surface_data.format);
    unsigned char* pixels = (unsigned char*)surface_data.pixel_data;

    _cairo_surface = cairo_image_surface_create_for_data(
      pixels, cairo_format_t(format), w, h, stride);

    if (_cairo_surface == nullptr) {
      return;
    }

    _cairo_ctx = cairo_create(_cairo_surface);
    if (_cairo_ctx == nullptr) {
      return;
    }
  }

  // Setup the context.
  cairo_set_operator(_cairo_ctx, CAIRO_OPERATOR_CLEAR);
  cairo_rectangle(_cairo_ctx, 0, 0, w, h);
  cairo_fill(_cairo_ctx);

  cairo_set_operator(_cairo_ctx, cairo_operator_t(to_cairo_operator(_params.comp_op)));
  cairo_set_line_width(_cairo_ctx, _params.stroke_width);

  // Setup globals.
  _pattern_extend = CAIRO_EXTEND_REPEAT;
  _pattern_filter = CAIRO_FILTER_NEAREST;

  switch (style) {
    case StyleKind::kSolid          : break;
    case StyleKind::kLinearPad      : _pattern_extend = CAIRO_EXTEND_PAD     ; break;
    case StyleKind::kLinearRepeat   : _pattern_extend = CAIRO_EXTEND_REPEAT  ; break;
    case StyleKind::kLinearReflect  : _pattern_extend = CAIRO_EXTEND_REFLECT ; break;
    case StyleKind::kRadialPad      : _pattern_extend = CAIRO_EXTEND_PAD     ; break;
    case StyleKind::kRadialRepeat   : _pattern_extend = CAIRO_EXTEND_REPEAT  ; break;
    case StyleKind::kRadialReflect  : _pattern_extend = CAIRO_EXTEND_REFLECT ; break;
    case StyleKind::kPatternNN      : _pattern_filter = CAIRO_FILTER_NEAREST ; break;
    case StyleKind::kPatternBI      : _pattern_filter = CAIRO_FILTER_BILINEAR; break;

    // These are not supported.
    case StyleKind::kConic:
      break;
  }
}

void CairoModule::flush() {
  // Nothing...
}

void CairoModule::after_run() {
  // Free the surface & the context.
  cairo_destroy(_cairo_ctx);
  cairo_surface_destroy(_cairo_surface);

  _cairo_ctx = nullptr;
  _cairo_surface = nullptr;

  // Free the sprites.
  for (uint32_t i = 0; i < kBenchNumSprites; i++) {
    cairo_surface_destroy(_cairo_sprites[i]);
    _cairo_sprites[i] = nullptr;
  }
}

void CairoModule::render_rect_a(RenderOp op) {
  BLSizeI bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  int wh = _params.shape_size;

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLRectI rect(_rnd_coord.next_rect_i(bounds, wh, wh));
    setup_style<BLRectI>(style, rect);

    if (op == RenderOp::kStroke) {
      cairo_rectangle(_cairo_ctx, rect.x, rect.y, rect.w, rect.h);
      cairo_stroke(_cairo_ctx);
    }
    else {
      cairo_rectangle(_cairo_ctx, rect.x, rect.y, rect.w, rect.h);
      cairo_fill(_cairo_ctx);
    }
  }
}

void CairoModule::render_rect_f(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double wh = _params.shape_size;

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));

    setup_style<BLRect>(style, rect);
    cairo_rectangle(_cairo_ctx, rect.x, rect.y, rect.w, rect.h);

    if (op == RenderOp::kStroke) {
      cairo_stroke(_cairo_ctx);
    }
    else {
      cairo_fill(_cairo_ctx);
    }
  }
}

void CairoModule::render_rect_rotated(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double cx = double(_params.screen_w) * 0.5;
  double cy = double(_params.screen_h) * 0.5;
  double wh = _params.shape_size;
  double angle = 0.0;

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
    BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));

    cairo_translate(_cairo_ctx, cx, cy);
    cairo_rotate(_cairo_ctx, angle);
    cairo_translate(_cairo_ctx, -cx, -cy);

    setup_style<BLRect>(style, rect);
    cairo_rectangle(_cairo_ctx, rect.x, rect.y, rect.w, rect.h);

    if (op == RenderOp::kStroke) {
      cairo_stroke(_cairo_ctx);
    }
    else {
      cairo_fill(_cairo_ctx);
    }

    cairo_identity_matrix(_cairo_ctx);
  }
}

void CairoModule::render_round_f(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double wh = _params.shape_size;

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
    double radius = _rnd_extra.next_double(4.0, 40.0);

    setup_style<BLRect>(style, rect);
    round_rect(_cairo_ctx, rect, radius);

    if (op == RenderOp::kStroke) {
      cairo_stroke(_cairo_ctx);
    }
    else {
      cairo_fill(_cairo_ctx);
    }
  }
}

void CairoModule::render_round_rotated(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double cx = double(_params.screen_w) * 0.5;
  double cy = double(_params.screen_h) * 0.5;
  double wh = _params.shape_size;
  double angle = 0.0;

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
    BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
    double radius = _rnd_extra.next_double(4.0, 40.0);

    cairo_translate(_cairo_ctx, cx, cy);
    cairo_rotate(_cairo_ctx, angle);
    cairo_translate(_cairo_ctx, -cx, -cy);

    setup_style<BLRect>(style, rect);
    round_rect(_cairo_ctx, rect, radius);

    if (op == RenderOp::kStroke) {
      cairo_stroke(_cairo_ctx);
    }
    else {
      cairo_fill(_cairo_ctx);
    }

    cairo_identity_matrix(_cairo_ctx);
  }
}

void CairoModule::render_polygon(RenderOp op, uint32_t complexity) {
  BLSizeI bounds(_params.screen_w - _params.shape_size,
                 _params.screen_h - _params.shape_size);
  StyleKind style = _params.style;
  double wh = double(_params.shape_size);

  cairo_set_fill_rule(_cairo_ctx, op == RenderOp::kFillEvenOdd ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLPoint base(_rnd_coord.nextPoint(bounds));

    double x = _rnd_coord.next_double(base.x, base.x + wh);
    double y = _rnd_coord.next_double(base.y, base.y + wh);

    cairo_move_to(_cairo_ctx, x, y);
    for (uint32_t p = 1; p < complexity; p++) {
      x = _rnd_coord.next_double(base.x, base.x + wh);
      y = _rnd_coord.next_double(base.y, base.y + wh);
      cairo_line_to(_cairo_ctx, x, y);
    }
    setup_style<BLRect>(style, BLRect(base.x, base.y, wh, wh));

    if (op == RenderOp::kStroke) {
      cairo_stroke(_cairo_ctx);
    }
    else {
      cairo_fill(_cairo_ctx);
    }
  }
}

void CairoModule::render_shape(RenderOp op, ShapeData shape) {
  BLSizeI bounds(_params.screen_w - _params.shape_size,
                 _params.screen_h - _params.shape_size);
  StyleKind style = _params.style;
  double wh = double(_params.shape_size);

  ShapeIterator it(shape);
  while (it.has_command()) {
    if (it.is_move_to()) {
      cairo_move_to(_cairo_ctx, it.x(0) * wh, it.y(0) * wh);
    }
    else if (it.is_line_to()) {
      cairo_line_to(_cairo_ctx, it.x(0) * wh, it.y(0) * wh);
    }
    else if (it.is_quad_to()) {
      double x0 = it.x(-1) * wh;
      double y0 = it.y(-1) * wh;
      double x1 = it.x(0) * wh;
      double y1 = it.y(0) * wh;
      double x2 = it.x(1) * wh;
      double y2 = it.y(1) * wh;

      cairo_curve_to(_cairo_ctx,
        (2.0 / 3.0) * x1 + (1.0 / 3.0) * x0, (2.0 / 3.0) * y1 + (1.0 / 3.0) * y0,
        (2.0 / 3.0) * x1 + (1.0 / 3.0) * x2, (2.0 / 3.0) * y1 + (1.0 / 3.0) * y2,
        y1, y2);
    }
    else if (it.is_cubic_to()) {
      cairo_curve_to(_cairo_ctx,
        it.x(0) * wh, it.y(0) * wh,
        it.x(1) * wh, it.y(1) * wh,
        it.x(2) * wh, it.y(2) * wh);
    }
    else {
      cairo_close_path(_cairo_ctx);
    }
    it.next();
  }

  // No idea who invented this, but you need a `cairo_t` to create a `cairo_path_t`.
  cairo_path_t* path = cairo_copy_path(_cairo_ctx);
  cairo_new_path(_cairo_ctx);

  cairo_set_fill_rule(_cairo_ctx, op == RenderOp::kFillEvenOdd ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    cairo_save(_cairo_ctx);

    BLPoint base(_rnd_coord.nextPoint(bounds));
    setup_style<BLRect>(style, BLRect(base.x, base.y, wh, wh));

    cairo_translate(_cairo_ctx, base.x, base.y);
    cairo_append_path(_cairo_ctx, path);

    if (op == RenderOp::kStroke) {
      cairo_stroke(_cairo_ctx);
    }
    else {
      cairo_fill(_cairo_ctx);
    }

    cairo_restore(_cairo_ctx);
  }

  cairo_path_destroy(path);
}

Backend* create_cairo_backend() {
  return new CairoModule();
}
} // {blbench}

#endif // BL_BENCH_ENABLE_CAIRO
