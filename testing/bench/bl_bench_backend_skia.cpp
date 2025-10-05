// This file is part of Blend2D project <https://blend2d.com>
//
// See LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifdef BL_BENCH_ENABLE_SKIA

#include "bl_bench_app.h"
#include "bl_bench_backend.h"

#include <skia/core/SkBitmap.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkColor.h>
#include <skia/core/SkImageInfo.h>
#include <skia/core/SkPaint.h>
#include <skia/core/SkPath.h>
#include <skia/core/SkTypes.h>
#include <skia/effects/SkGradientShader.h>

namespace blbench {

static inline SkIRect to_sk_irect(const BLRectI& rect) {
  return SkIRect::MakeXYWH(rect.x, rect.y, rect.w, rect.h);
}

static inline SkRect to_sk_rect(const BLRect& rect) {
  return SkRect::MakeXYWH(SkScalar(rect.x), SkScalar(rect.y), SkScalar(rect.w), SkScalar(rect.h));
}

static uint32_t to_sk_blend_mode(BLCompOp comp_op) {
  switch (comp_op) {
    case BL_COMP_OP_SRC_OVER   : return uint32_t(SkBlendMode::kSrcOver);
    case BL_COMP_OP_SRC_COPY   : return uint32_t(SkBlendMode::kSrc);
    case BL_COMP_OP_SRC_IN     : return uint32_t(SkBlendMode::kSrcIn);
    case BL_COMP_OP_SRC_OUT    : return uint32_t(SkBlendMode::kSrcOut);
    case BL_COMP_OP_SRC_ATOP   : return uint32_t(SkBlendMode::kSrcATop);
    case BL_COMP_OP_DST_OVER   : return uint32_t(SkBlendMode::kDstOver);
    case BL_COMP_OP_DST_COPY   : return uint32_t(SkBlendMode::kDst);
    case BL_COMP_OP_DST_IN     : return uint32_t(SkBlendMode::kDstIn);
    case BL_COMP_OP_DST_OUT    : return uint32_t(SkBlendMode::kDstOut);
    case BL_COMP_OP_DST_ATOP   : return uint32_t(SkBlendMode::kDstATop);
    case BL_COMP_OP_XOR        : return uint32_t(SkBlendMode::kXor);
    case BL_COMP_OP_CLEAR      : return uint32_t(SkBlendMode::kClear);
    case BL_COMP_OP_PLUS       : return uint32_t(SkBlendMode::kPlus);
    case BL_COMP_OP_MODULATE   : return uint32_t(SkBlendMode::kModulate);
    case BL_COMP_OP_MULTIPLY   : return uint32_t(SkBlendMode::kMultiply);
    case BL_COMP_OP_SCREEN     : return uint32_t(SkBlendMode::kScreen);
    case BL_COMP_OP_OVERLAY    : return uint32_t(SkBlendMode::kOverlay);
    case BL_COMP_OP_DARKEN     : return uint32_t(SkBlendMode::kDarken);
    case BL_COMP_OP_LIGHTEN    : return uint32_t(SkBlendMode::kLighten);
    case BL_COMP_OP_COLOR_DODGE: return uint32_t(SkBlendMode::kColorDodge);
    case BL_COMP_OP_COLOR_BURN : return uint32_t(SkBlendMode::kColorBurn);
    case BL_COMP_OP_HARD_LIGHT : return uint32_t(SkBlendMode::kHardLight);
    case BL_COMP_OP_SOFT_LIGHT : return uint32_t(SkBlendMode::kSoftLight);
    case BL_COMP_OP_DIFFERENCE : return uint32_t(SkBlendMode::kDifference);
    case BL_COMP_OP_EXCLUSION  : return uint32_t(SkBlendMode::kExclusion);

    default: return 0xFFFFFFFFu;
  }
}

struct SkiaModule final : public Backend {
  SkCanvas* _sk_canvas {};
  SkBitmap _sk_surface;
  SkBitmap _sk_sprites[4];

  SkBlendMode _blend_mode {};
  SkTileMode _gradient_tile_mode {};

  SkiaModule();
  ~SkiaModule() override;

  template<typename RectT>
  sk_sp<SkShader> create_shader(StyleKind style, const RectT& rect);

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

SkiaModule::SkiaModule() {
  strcpy(_name, "Skia");
}
SkiaModule::~SkiaModule() {}

template<typename RectT>
sk_sp<SkShader> SkiaModule::create_shader(StyleKind style, const RectT& rect) {
  static const SkScalar positions3[3] = {
    SkScalar(0.0),
    SkScalar(0.5),
    SkScalar(1.0)
  };

  static const SkScalar positions4[4] = {
    SkScalar(0.0),
    SkScalar(0.33),
    SkScalar(0.66),
    SkScalar(1.0)
  };

  switch (style) {
    case StyleKind::kLinearPad:
    case StyleKind::kLinearRepeat:
    case StyleKind::kLinearReflect: {
      SkPoint pts[2] = {
        SkScalar(rect.x + rect.w * 0.2),
        SkScalar(rect.y + rect.h * 0.2),
        SkScalar(rect.x + rect.w * 0.8),
        SkScalar(rect.y + rect.h * 0.8)
      };

      SkColor colors[3] = {
        _rnd_color.next_rgba32().value,
        _rnd_color.next_rgba32().value,
        _rnd_color.next_rgba32().value
      };

      return SkGradientShader::MakeLinear(pts, colors, positions3, 3, _gradient_tile_mode);
    }

    case StyleKind::kRadialPad:
    case StyleKind::kRadialRepeat:
    case StyleKind::kRadialReflect: {
      double cx = rect.x + rect.w / 2.0;
      double cy = rect.y + rect.h / 2.0;
      double cr = (rect.w + rect.h) / 4.0;
      double fx = cx - cr / 2;
      double fy = cy - cr / 2;

      SkColor colors[3] = {
        _rnd_color.next_rgba32().value,
        _rnd_color.next_rgba32().value,
        _rnd_color.next_rgba32().value
      };

      return SkGradientShader::MakeTwoPointConical(
        SkPoint::Make(SkScalar(cx), SkScalar(cy)),
        SkScalar(cr),
        SkPoint::Make(SkScalar(fx), SkScalar(fy)),
        SkScalar(0.0),
        colors, positions3, 3, _gradient_tile_mode);
    }

    case StyleKind::kConic: {
      double cx = rect.x + rect.w / 2;
      double cy = rect.y + rect.h / 2;
      BLRgba32 c = _rnd_color.next_rgba32();

      SkColor colors[4] = {
        c.value,
        _rnd_color.next_rgba32().value,
        _rnd_color.next_rgba32().value,
        c.value
      };

      return SkGradientShader::MakeSweep(SkScalar(cx), SkScalar(cy), colors, positions4, 4);
    }

    case StyleKind::kPatternNN:
    case StyleKind::kPatternBI: {
      uint32_t sprite_id = nextSpriteId();
      SkFilterMode filter_mode = style == StyleKind::kPatternNN ? SkFilterMode::kNearest : SkFilterMode::kLinear;
      SkMatrix m = SkMatrix::Translate(SkScalar(rect.x), SkScalar(rect.y));
      return _sk_sprites[sprite_id].makeShader(SkSamplingOptions(filter_mode), m);
    }

    default: {
      return sk_sp<SkShader>{};
    }
  }
}

bool SkiaModule::supports_comp_op(BLCompOp comp_op) const {
  return to_sk_blend_mode(comp_op) != 0xFFFFFFFFu;
}

bool SkiaModule::supports_style(StyleKind style) const {
  return style == StyleKind::kSolid         ||
         style == StyleKind::kLinearPad     ||
         style == StyleKind::kLinearRepeat  ||
         style == StyleKind::kLinearReflect ||
         style == StyleKind::kRadialPad     ||
         style == StyleKind::kRadialRepeat  ||
         style == StyleKind::kRadialReflect ||
         style == StyleKind::kConic         ||
         style == StyleKind::kPatternNN     ||
         style == StyleKind::kPatternBI     ;
}

void SkiaModule::before_run() {
  int w = int(_params.screen_w);
  int h = int(_params.screen_h);
  StyleKind style = _params.style;

  // Initialize the sprites.
  for (uint32_t i = 0; i < kBenchNumSprites; i++) {
    const BLImage& sprite = _sprites[i];

    BLImageData sprite_data;
    sprite.get_data(&sprite_data);

    SkImageInfo sprite_info = SkImageInfo::Make(sprite_data.size.w, sprite_data.size.h, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
    _sk_sprites[i].installPixels(sprite_info, sprite_data.pixel_data, size_t(sprite_data.stride));
  }

  // Initialize the surface and the context.
  BLImageData surface_data;
  _surface.create(w, h, _params.format);
  _surface.make_mutable(&surface_data);

  SkImageInfo surface_info = SkImageInfo::Make(w, h, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  _sk_surface.installPixels(surface_info, surface_data.pixel_data, size_t(surface_data.stride));
  _sk_surface.erase(0x00000000, SkIRect::MakeXYWH(0, 0, w, h));

  _sk_canvas = new SkCanvas(_sk_surface);

  // Setup globals.
  _blend_mode = SkBlendMode(to_sk_blend_mode(_params.comp_op));
  _gradient_tile_mode = SkTileMode::kClamp;

  switch (style) {
    case StyleKind::kLinearPad    : _gradient_tile_mode = SkTileMode::kClamp ; break;
    case StyleKind::kLinearRepeat : _gradient_tile_mode = SkTileMode::kRepeat; break;
    case StyleKind::kLinearReflect: _gradient_tile_mode = SkTileMode::kMirror; break;
    case StyleKind::kRadialPad    : _gradient_tile_mode = SkTileMode::kClamp ; break;
    case StyleKind::kRadialRepeat : _gradient_tile_mode = SkTileMode::kRepeat; break;
    case StyleKind::kRadialReflect: _gradient_tile_mode = SkTileMode::kMirror; break;

    default:
      break;
  }
}

void SkiaModule::flush() {
  // Nothing...
}

void SkiaModule::after_run() {
  delete _sk_canvas;
  _sk_canvas = nullptr;
  _sk_surface.reset();

  for (uint32_t i = 0; i < kBenchNumSprites; i++) {
    _sk_sprites[i].reset();
  }
}

void SkiaModule::render_rect_a(RenderOp op) {
  BLSizeI bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  int wh = _params.shape_size;

  SkPaint p;
  p.setStyle(op == RenderOp::kStroke ? SkPaint::kStroke_Style : SkPaint::kFill_Style);
  p.setAntiAlias(true);
  p.setBlendMode(_blend_mode);
  p.setStrokeWidth(SkScalar(_params.stroke_width));

  if (style == StyleKind::kSolid) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRectI rect = _rnd_coord.next_rect_i(bounds, wh, wh);

      p.setColor(_rnd_color.next_rgba32().value);
      _sk_canvas->drawIRect(to_sk_irect(rect), p);
    }
  }
  else {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRectI rect = _rnd_coord.next_rect_i(bounds, wh, wh);

      p.setShader(create_shader(style, rect));
      _sk_canvas->drawIRect(to_sk_irect(rect), p);
    }
  }
}

void SkiaModule::render_rect_f(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  double wh = _params.shape_size;

  SkPaint p;
  p.setStyle(op == RenderOp::kStroke ? SkPaint::kStroke_Style : SkPaint::kFill_Style);
  p.setAntiAlias(true);
  p.setBlendMode(_blend_mode);
  p.setStrokeWidth(SkScalar(_params.stroke_width));

  if (style == StyleKind::kSolid) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRect rect = _rnd_coord.next_rect(bounds, wh, wh);

      p.setColor(_rnd_color.next_rgba32().value);
      _sk_canvas->drawRect(to_sk_rect(rect), p);
    }
  }
  else {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRect rect = _rnd_coord.next_rect(bounds, wh, wh);

      p.setShader(create_shader(style, rect));
      _sk_canvas->drawRect(to_sk_rect(rect), p);
    }
  }
}

void SkiaModule::render_rect_rotated(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double cx = double(_params.screen_w) * 0.5;
  double cy = double(_params.screen_h) * 0.5;
  double wh = _params.shape_size;
  double angle = 0.0;

  SkPaint p;
  p.setStyle(op == RenderOp::kStroke ? SkPaint::kStroke_Style : SkPaint::kFill_Style);
  p.setAntiAlias(true);
  p.setBlendMode(_blend_mode);
  p.setStrokeWidth(SkScalar(_params.stroke_width));

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
    BLRect rect = _rnd_coord.next_rect(bounds, wh, wh);

    _sk_canvas->rotate(SkRadiansToDegrees(angle), SkScalar(cx), SkScalar(cy));

    if (style == StyleKind::kSolid)
      p.setColor(_rnd_color.next_rgba32().value);
    else
      p.setShader(create_shader(style, rect));

    _sk_canvas->drawRect(to_sk_rect(rect), p);
    _sk_canvas->resetMatrix();
  }
}

void SkiaModule::render_round_f(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  double wh = _params.shape_size;

  SkPaint p;
  p.setStyle(op == RenderOp::kStroke ? SkPaint::kStroke_Style : SkPaint::kFill_Style);
  p.setAntiAlias(true);
  p.setBlendMode(_blend_mode);
  p.setStrokeWidth(SkScalar(_params.stroke_width));

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLRect rect = _rnd_coord.next_rect(bounds, wh, wh);
    double radius = _rnd_extra.next_double(4.0, 40.0);

    if (style == StyleKind::kSolid)
      p.setColor(_rnd_color.next_rgba32().value);
    else
      p.setShader(create_shader(style, rect));

    _sk_canvas->drawRoundRect(to_sk_rect(rect), SkScalar(radius), SkScalar(radius), p);
  }
}

void SkiaModule::render_round_rotated(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double cx = double(_params.screen_w) * 0.5;
  double cy = double(_params.screen_h) * 0.5;
  double wh = _params.shape_size;
  double angle = 0.0;

  SkPaint p;
  p.setStyle(op == RenderOp::kStroke ? SkPaint::kStroke_Style : SkPaint::kFill_Style);
  p.setAntiAlias(true);
  p.setBlendMode(_blend_mode);
  p.setStrokeWidth(SkScalar(_params.stroke_width));

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
    _sk_canvas->rotate(SkRadiansToDegrees(angle), SkScalar(cx), SkScalar(cy));

    BLRect rect = _rnd_coord.next_rect(bounds, wh, wh);
    double radius = _rnd_extra.next_double(4.0, 40.0);

    if (style == StyleKind::kSolid)
      p.setColor(_rnd_color.next_rgba32().value);
    else
      p.setShader(create_shader(style, rect));

    _sk_canvas->drawRoundRect(to_sk_rect(rect), SkScalar(radius), SkScalar(radius), p);
    _sk_canvas->resetMatrix();
  }
}

void SkiaModule::render_polygon(RenderOp op, uint32_t complexity) {
  static constexpr uint32_t kPointCapacity = 128;

  BLSizeI bounds(_params.screen_w - _params.shape_size,
                 _params.screen_h - _params.shape_size);
  StyleKind style = _params.style;
  double wh = double(_params.shape_size);

  if (complexity > kPointCapacity) {
    return;
  }

  SkPoint points[kPointCapacity];

  SkPaint p;
  p.setStyle(op == RenderOp::kStroke ? SkPaint::kStroke_Style : SkPaint::kFill_Style);
  p.setAntiAlias(true);
  p.setBlendMode(_blend_mode);
  p.setStrokeWidth(SkScalar(_params.stroke_width));

  // SKIA cannot draw a polygon without having a path, so we have two cases here.
  if (op != RenderOp::kStroke) {
    SkPathFillType fillType = op == RenderOp::kFillEvenOdd ? SkPathFillType::kEvenOdd : SkPathFillType::kWinding;

    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLPoint base(_rnd_coord.nextPoint(bounds));

      SkPath path;
      path.setFillType(fillType);

      double x, y;
      x = _rnd_coord.next_double(base.x, base.x + wh);
      y = _rnd_coord.next_double(base.y, base.y + wh);
      path.moveTo(SkPoint::Make(SkScalar(x), SkScalar(y)));

      for (uint32_t j = 1; j < complexity; j++) {
        x = _rnd_coord.next_double(base.x, base.x + wh);
        y = _rnd_coord.next_double(base.y, base.y + wh);
        path.lineTo(SkPoint::Make(SkScalar(x), SkScalar(y)));
      }

      if (style == StyleKind::kSolid) {
        p.setColor(_rnd_color.next_rgba32().value);
      }
      else {
        BLRect rect(base.x, base.y, wh, wh);
        p.setShader(create_shader(style, rect));
      }

      _sk_canvas->drawPath(path, p);
    }
  }
  else {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLPoint base(_rnd_coord.nextPoint(bounds));

      for (uint32_t j = 0; j < complexity; j++) {
        double x = _rnd_coord.next_double(base.x, base.x + wh);
        double y = _rnd_coord.next_double(base.y, base.y + wh);
        points[j].set(SkScalar(x), SkScalar(y));
      }

      if (style == StyleKind::kSolid) {
        p.setColor(_rnd_color.next_rgba32().value);
      }
      else {
        BLRect rect(base.x, base.y, wh, wh);
        p.setShader(create_shader(style, rect));
      }

      _sk_canvas->drawPoints(SkCanvas::kPolygon_PointMode, complexity, points, p);
    }
  }
}

void SkiaModule::render_shape(RenderOp op, ShapeData shape) {
  BLSizeI bounds(_params.screen_w - _params.shape_size,
                 _params.screen_h - _params.shape_size);
  StyleKind style = _params.style;
  double wh = double(_params.shape_size);

  SkPathFillType fillType = op == RenderOp::kFillEvenOdd ? SkPathFillType::kEvenOdd : SkPathFillType::kWinding;

  SkPath path;
  path.setFillType(fillType);

  ShapeIterator it(shape);
  while (it.has_command()) {
    if (it.is_move_to()) {
      path.moveTo(SkScalar(it.x(0) * wh), SkScalar(it.y(0) * wh));
    }
    else if (it.is_line_to()) {
      path.lineTo(SkScalar(it.x(0) * wh), SkScalar(it.y(0) * wh));
    }
    else if (it.is_quad_to()) {
      path.quadTo(
        SkScalar(it.x(0) * wh), SkScalar(it.y(0) * wh),
        SkScalar(it.x(1) * wh), SkScalar(it.y(1) * wh));
    }
    else if (it.is_cubic_to()) {
      path.cubicTo(
        SkScalar(it.x(0) * wh), SkScalar(it.y(0) * wh),
        SkScalar(it.x(1) * wh), SkScalar(it.y(1) * wh),
        SkScalar(it.x(2) * wh), SkScalar(it.y(2) * wh));
    }
    else {
      path.close();
    }
    it.next();
  }

  SkPaint p;
  p.setStyle(op == RenderOp::kStroke ? SkPaint::kStroke_Style : SkPaint::kFill_Style);
  p.setAntiAlias(true);
  p.setBlendMode(_blend_mode);
  p.setStrokeWidth(SkScalar(_params.stroke_width));

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLPoint base(_rnd_coord.nextPoint(bounds));

    _sk_canvas->translate(SkScalar(base.x), SkScalar(base.y));

    if (style == StyleKind::kSolid) {
      p.setColor(_rnd_color.next_rgba32().value);
    }
    else {
      BLRect rect(0, 0, wh, wh);
      p.setShader(create_shader(style, rect));
    }

    _sk_canvas->drawPath(path, p);
    _sk_canvas->resetMatrix();
  }
}

Backend* create_skia_backend() {
  return new SkiaModule();
}

} // {blbench}

#endif // BL_BENCH_ENABLE_SKIA
