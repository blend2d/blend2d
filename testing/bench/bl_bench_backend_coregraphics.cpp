// This file is part of Blend2D project <https://blend2d.com>
//
// See LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifdef BL_BENCH_ENABLE_COREGRAPHICS

#include "bl_bench_app.h"
#include "bl_bench_backend.h"

#include <ApplicationServices/ApplicationServices.h>
#include <utility>

namespace blbench {

static uint32_t to_cg_bitmap_info(BLFormat format) noexcept {
  switch (format) {
    case BL_FORMAT_PRGB32: return kCGImageByteOrder32Little | kCGImageAlphaPremultipliedFirst;
    case BL_FORMAT_XRGB32: return kCGImageByteOrder32Little | kCGImageAlphaNoneSkipFirst;

    default: return 0;
  }
}

static CGBlendMode to_cg_blend_mode(BLCompOp comp_op) noexcept {
  switch (comp_op) {
    case BL_COMP_OP_SRC_OVER   : return kCGBlendModeNormal;
    case BL_COMP_OP_SRC_COPY   : return kCGBlendModeCopy;
    case BL_COMP_OP_SRC_IN     : return kCGBlendModeSourceIn;
    case BL_COMP_OP_SRC_OUT    : return kCGBlendModeSourceOut;
    case BL_COMP_OP_SRC_ATOP   : return kCGBlendModeSourceAtop;
    case BL_COMP_OP_DST_OVER   : return kCGBlendModeDestinationOver;
    case BL_COMP_OP_DST_IN     : return kCGBlendModeDestinationIn;
    case BL_COMP_OP_DST_OUT    : return kCGBlendModeDestinationOut;
    case BL_COMP_OP_DST_ATOP   : return kCGBlendModeDestinationAtop;
    case BL_COMP_OP_XOR        : return kCGBlendModeXOR;
    case BL_COMP_OP_CLEAR      : return kCGBlendModeClear;
    case BL_COMP_OP_PLUS       : return kCGBlendModePlusLighter;
    case BL_COMP_OP_MULTIPLY   : return kCGBlendModeMultiply;
    case BL_COMP_OP_SCREEN     : return kCGBlendModeScreen;
    case BL_COMP_OP_OVERLAY    : return kCGBlendModeOverlay;
    case BL_COMP_OP_DARKEN     : return kCGBlendModeDarken;
    case BL_COMP_OP_LIGHTEN    : return kCGBlendModeLighten;
    case BL_COMP_OP_COLOR_DODGE: return kCGBlendModeColorDodge;
    case BL_COMP_OP_COLOR_BURN : return kCGBlendModeColorBurn;
    case BL_COMP_OP_HARD_LIGHT : return kCGBlendModeHardLight;
    case BL_COMP_OP_SOFT_LIGHT : return kCGBlendModeSoftLight;
    case BL_COMP_OP_DIFFERENCE : return kCGBlendModeDifference;
    case BL_COMP_OP_EXCLUSION  : return kCGBlendModeExclusion;

    default:
      return kCGBlendModeNormal;
  }
}

template<typename RectT>
static inline CGRect to_cg_rect(const RectT& rect) noexcept {
  return CGRectMake(CGFloat(rect.x), CGFloat(rect.y), CGFloat(rect.w), CGFloat(rect.h));
}

static inline void to_cg_color_components(CGFloat components[4], BLRgba32 color) noexcept {
  constexpr CGFloat scale = CGFloat(1.0f / 255.0f);
  components[0] = CGFloat(int(color.r())) * scale;
  components[1] = CGFloat(int(color.g())) * scale;
  components[2] = CGFloat(int(color.b())) * scale;
  components[3] = CGFloat(int(color.a())) * scale;
}

static void flipImage(BLImage& img) noexcept {
  BLImageData imgData;
  img.make_mutable(&imgData);

  uint32_t h = uint32_t(imgData.size.h);
  intptr_t stride = imgData.stride;
  uint8_t* pixel_data = static_cast<uint8_t*>(imgData.pixel_data);

  for (uint32_t y = 0; y < h / 2; y++) {
    uint8_t* a = pixel_data + intptr_t(y) * stride;
    uint8_t* b = pixel_data + intptr_t(h - y - 1) * stride;

    for (size_t x = 0; x < size_t(stride); x++) {
      std::swap(a[x], b[x]);
    }

  }
}

struct CoreGraphicsModule : public Backend {
  CGImageRef _cg_sprites[kBenchNumSprites] {};

  CGColorSpaceRef _cg_colorspace {};
  CGContextRef _cg_ctx {};

  CoreGraphicsModule();
  ~CoreGraphicsModule() override;

  void serialize_info(JSONBuilder& json) const override;

  bool supports_comp_op(BLCompOp comp_op) const override;
  bool supports_style(StyleKind style) const override;

  void before_run() override;
  void flush() override;
  void after_run() override;

  CGGradientRef create_gradient(StyleKind style) noexcept;

  BL_INLINE void render_solid_path(RenderOp op) noexcept;

  template<typename RectT>
  BL_INLINE void render_solid_rect(const RectT& rect, RenderOp op) noexcept;

  template<bool kSaveGState, typename RectT>
  BL_INLINE void render_styled_path(const RectT& rect, StyleKind style, RenderOp op) noexcept;

  template<bool kSaveGState, typename RectT>
  BL_INLINE void render_styled_rect(const RectT& rect, StyleKind style, RenderOp op) noexcept;

  void render_rect_a(RenderOp op) override;
  void render_rect_f(RenderOp op) override;
  void render_rect_rotated(RenderOp op) override;
  void render_round_f(RenderOp op) override;
  void render_round_rotated(RenderOp op) override;
  void render_polygon(RenderOp op, uint32_t complexity) override;
  void render_shape(RenderOp op, ShapeData shape) override;
};

CoreGraphicsModule::CoreGraphicsModule() {
  strcpy(_name, "CoreGraphics");
}

CoreGraphicsModule::~CoreGraphicsModule() {}

void CoreGraphicsModule::serialize_info(JSONBuilder& json) const {
}

bool CoreGraphicsModule::supports_comp_op(BLCompOp comp_op) const {
  return comp_op == BL_COMP_OP_SRC_OVER    ||
         comp_op == BL_COMP_OP_SRC_COPY    ||
         comp_op == BL_COMP_OP_SRC_IN      ||
         comp_op == BL_COMP_OP_SRC_OUT     ||
         comp_op == BL_COMP_OP_SRC_ATOP    ||
         comp_op == BL_COMP_OP_DST_OVER    ||
         comp_op == BL_COMP_OP_DST_IN      ||
         comp_op == BL_COMP_OP_DST_OUT     ||
         comp_op == BL_COMP_OP_DST_ATOP    ||
         comp_op == BL_COMP_OP_XOR         ||
         comp_op == BL_COMP_OP_CLEAR       ||
         comp_op == BL_COMP_OP_PLUS        ||
         comp_op == BL_COMP_OP_MULTIPLY    ||
         comp_op == BL_COMP_OP_SCREEN      ||
         comp_op == BL_COMP_OP_OVERLAY     ||
         comp_op == BL_COMP_OP_DARKEN      ||
         comp_op == BL_COMP_OP_LIGHTEN     ||
         comp_op == BL_COMP_OP_COLOR_DODGE ||
         comp_op == BL_COMP_OP_COLOR_BURN  ||
         comp_op == BL_COMP_OP_HARD_LIGHT  ||
         comp_op == BL_COMP_OP_SOFT_LIGHT  ||
         comp_op == BL_COMP_OP_DIFFERENCE  ||
         comp_op == BL_COMP_OP_EXCLUSION   ;
}

bool CoreGraphicsModule::supports_style(StyleKind style) const {
  return style == StyleKind::kSolid     ||
         style == StyleKind::kLinearPad ||
         style == StyleKind::kRadialPad ||
         style == StyleKind::kConic     ||
         style == StyleKind::kPatternNN ||
         style == StyleKind::kPatternBI ;
}

void CoreGraphicsModule::before_run() {
  int w = int(_params.screen_w);
  int h = int(_params.screen_h);
  StyleKind style = _params.style;

  _cg_colorspace = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGBLinear);

  // Initialize the sprites.
  for (uint32_t i = 0; i < kBenchNumSprites; i++) {
    BLImage& sprite = _sprites[i];
    flipImage(sprite);

    BLImageData sprite_data;
    sprite.get_data(&sprite_data);

    BLFormat sprite_format = BLFormat(sprite_data.format);

    CGDataProviderRef dp = CGDataProviderCreateWithData(
      nullptr,
      sprite_data.pixel_data,
      size_t(sprite_data.size.h) * size_t(sprite_data.stride),
      nullptr);

    if (!dp) {
      printf("Failed to create CGDataProvider\n");
      continue;
    }

    CGImageRef cg_sprite = CGImageCreate(
      size_t(sprite_data.size.w),       // Width.
      size_t(sprite_data.size.h),       // Height.
      8,                                // Bits per component.
      32,                               // Bits per pixel.
      size_t(sprite_data.stride),       // Bytes per row.
      _cg_colorspace,                   // Colorspace.
      to_cg_bitmap_info(sprite_format), // Bitmap info.
      dp,                               // Data provider.
      nullptr,                          // Decode.
      style == StyleKind::kPatternBI,   // Should interpolate.
      kCGRenderingIntentDefault);       // Color rendering intent.

    if (!cg_sprite) {
      printf("Failed to create CGImage\n");
      continue;
    }

    _cg_sprites[i] = cg_sprite;
    CGDataProviderRelease(dp);
  }

  // Initialize the surface and the context.
  BLImageData surface_data;
  _surface.create(w, h, _params.format);
  _surface.make_mutable(&surface_data);

  _cg_ctx = CGBitmapContextCreate(
    surface_data.pixel_data,
    uint32_t(surface_data.size.w),
    uint32_t(surface_data.size.h),
    8,
    size_t(surface_data.stride),
    _cg_colorspace,
    to_cg_bitmap_info(BLFormat(surface_data.format)));

  const CGFloat transparent[4] {};

  // Setup the context.
  CGContextSetBlendMode(_cg_ctx, kCGBlendModeCopy);
  CGContextSetFillColorSpace(_cg_ctx, _cg_colorspace);
  CGContextSetStrokeColorSpace(_cg_ctx, _cg_colorspace);

  CGContextSetFillColor(_cg_ctx, transparent);
  CGContextFillRect(_cg_ctx, CGRectMake(0, 0, CGFloat(surface_data.size.w), CGFloat(surface_data.size.h)));

  CGContextSetBlendMode(_cg_ctx, to_cg_blend_mode(_params.comp_op));
  CGContextSetAllowsAntialiasing(_cg_ctx, true);

  CGContextSetLineJoin(_cg_ctx, kCGLineJoinMiter);
  CGContextSetLineWidth(_cg_ctx, CGFloat(_params.stroke_width));
}

void CoreGraphicsModule::flush() {
  CGContextSynchronize(_cg_ctx);
}

void CoreGraphicsModule::after_run() {
  CGContextRelease(_cg_ctx);
  CGColorSpaceRelease(_cg_colorspace);

  _cg_ctx = nullptr;
  _cg_colorspace = nullptr;

  // Free the sprites.
  for (uint32_t i = 0; i < kBenchNumSprites; i++) {
    if (_cg_sprites[i]) {
      CGImageRelease(_cg_sprites[i]);
      _cg_sprites[i] = nullptr;
    }
  }

  flipImage(_surface);
}

CGGradientRef CoreGraphicsModule::create_gradient(StyleKind style) noexcept {
  BLRgba32 c0 = _rnd_color.next_rgba32();
  BLRgba32 c1 = _rnd_color.next_rgba32();
  BLRgba32 c2 = _rnd_color.next_rgba32();

  switch (style) {
    case StyleKind::kLinearPad:
    case StyleKind::kLinearRepeat:
    case StyleKind::kRadialPad:
    case StyleKind::kRadialRepeat: {
      CGFloat components[12];
      to_cg_color_components(components + 0, c0);
      to_cg_color_components(components + 4, c1);
      to_cg_color_components(components + 8, c2);

      CGFloat locations[3] = {
        CGFloat(0.0),
        CGFloat(0.5),
        CGFloat(1.0)
      };

      return CGGradientCreateWithColorComponents(_cg_colorspace, components, locations, 3);
    }

    case StyleKind::kLinearReflect:
    case StyleKind::kRadialReflect: {

      CGFloat components[20];
      to_cg_color_components(components +  0, c0);
      to_cg_color_components(components +  4, c1);
      to_cg_color_components(components +  8, c2);
      to_cg_color_components(components + 12, c1);
      to_cg_color_components(components + 16, c0);

      CGFloat locations[5] = {
        CGFloat(0.0),
        CGFloat(0.25),
        CGFloat(0.5),
        CGFloat(0.75),
        CGFloat(1.0)
      };

      return CGGradientCreateWithColorComponents(_cg_colorspace, components, locations, 5);
    }

    case StyleKind::kConic: {
      BLRgba32 c0 = _rnd_color.next_rgba32();
      BLRgba32 c1 = _rnd_color.next_rgba32();
      BLRgba32 c2 = _rnd_color.next_rgba32();

      CGFloat components[16];
      to_cg_color_components(components +  0, c0);
      to_cg_color_components(components +  4, c1);
      to_cg_color_components(components +  8, c2);
      to_cg_color_components(components + 12, c0);

      CGFloat locations[4] = {
        CGFloat(0.0),
        CGFloat(0.33),
        CGFloat(0.66),
        CGFloat(1.0)
      };

      return CGGradientCreateWithColorComponents(_cg_colorspace, components, locations, 4);
    }

    default:
      return CGGradientRef{};
  }
}

BL_INLINE void CoreGraphicsModule::render_solid_path(RenderOp op) noexcept {
  CGFloat color[4];
  to_cg_color_components(color, _rnd_color.next_rgba32());

  if (op == RenderOp::kStroke) {
    CGContextSetStrokeColor(_cg_ctx, color);
    CGContextStrokePath(_cg_ctx);
  }
  else {
    CGContextSetFillColor(_cg_ctx, color);
    if (op == RenderOp::kFillNonZero)
      CGContextFillPath(_cg_ctx);
    else
      CGContextEOFillPath(_cg_ctx);
  }
}

template<typename RectT>
BL_INLINE void CoreGraphicsModule::render_solid_rect(const RectT& rect, RenderOp op) noexcept {
  CGFloat color[4];
  to_cg_color_components(color, _rnd_color.next_rgba32());

  if (op == RenderOp::kStroke) {
    CGContextSetStrokeColor(_cg_ctx, color);
    CGContextStrokeRect(_cg_ctx, to_cg_rect(rect));
  }
  else {
    CGContextSetFillColor(_cg_ctx, color);
    CGContextFillRect(_cg_ctx, to_cg_rect(rect));
  }
}

template<bool kSaveGState, typename RectT>
BL_INLINE void CoreGraphicsModule::render_styled_path(const RectT& rect, StyleKind style, RenderOp op) noexcept {
  if (kSaveGState)
    CGContextSaveGState(_cg_ctx);

  if (op == RenderOp::kStroke) {
    CGContextReplacePathWithStrokedPath(_cg_ctx);
  }

  if (op == RenderOp::kFillEvenOdd) {
    CGContextEOClip(_cg_ctx);
  }
  else {
    CGContextClip(_cg_ctx);
  }

  switch (style) {
    case StyleKind::kSolid:
      // Not reached (the caller must use render_solid_path() instead).
      break;

    case StyleKind::kLinearPad:
    case StyleKind::kLinearRepeat:
    case StyleKind::kLinearReflect: {
      CGFloat reflectScale = style == StyleKind::kLinearReflect ? 1.0 : 2.0;
      CGFloat w = rect.w * reflectScale;
      CGFloat h = rect.h * reflectScale;

      CGFloat x0 = CGFloat(rect.x) + w * CGFloat(0.2);
      CGFloat y0 = CGFloat(rect.y) + h * CGFloat(0.2);
      CGFloat x1 = CGFloat(rect.x) + w * CGFloat(0.8);
      CGFloat y1 = CGFloat(rect.y) + h * CGFloat(0.8);

      CGGradientRef gradient = create_gradient(style);
      CGGradientDrawingOptions options = kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation;
      CGContextDrawLinearGradient(_cg_ctx, gradient, CGPointMake(x0, y0), CGPointMake(x1, y1), options);
      CGGradientRelease(gradient);
      break;
    }

    case StyleKind::kRadialPad:
    case StyleKind::kRadialRepeat:
    case StyleKind::kRadialReflect: {
      CGFloat cx = CGFloat(rect.x + rect.w / 2);
      CGFloat cy = CGFloat(rect.y + rect.h / 2);
      CGFloat cr = CGFloat((rect.w + rect.h) / 4);
      CGFloat fx = CGFloat(cx - cr / 2);
      CGFloat fy = CGFloat(cy - cr / 2);

      CGGradientRef gradient = create_gradient(style);
      CGGradientDrawingOptions options = kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation;
      CGContextDrawRadialGradient(_cg_ctx, gradient, CGPointMake(cx, cy), cr, CGPointMake(fx, fy), CGFloat(0.0), options);
      CGGradientRelease(gradient);
      break;
    }

    case StyleKind::kConic: {
      double cx = rect.x + rect.w / 2;
      double cy = rect.y + rect.h / 2;
      double angle = 0.0;

      CGGradientRef gradient = create_gradient(style);
      CGContextDrawConicGradient(_cg_ctx, gradient, CGPointMake(cx, cy), CGFloat(angle));
      CGGradientRelease(gradient);
      break;
    }

    case StyleKind::kPatternNN:
    case StyleKind::kPatternBI: {
      uint32_t spriteId = nextSpriteId();

      CGContextDrawImage(_cg_ctx, to_cg_rect(rect), _cg_sprites[spriteId]);
      break;
    }
  }

  if (kSaveGState)
    CGContextRestoreGState(_cg_ctx);
}

template<bool kSaveGState, typename RectT>
BL_INLINE void CoreGraphicsModule::render_styled_rect(const RectT& rect, StyleKind style, RenderOp op) noexcept {
  CGContextAddRect(_cg_ctx, to_cg_rect(rect));
  render_styled_path<kSaveGState>(rect, style, op);
}

void CoreGraphicsModule::render_rect_a(RenderOp op) {
  BLSizeI bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  int wh = _params.shape_size;

  if (style == StyleKind::kSolid) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      render_solid_rect(_rnd_coord.next_rect_i(bounds, wh, wh), op);
    }
  }
  else if ((style == StyleKind::kPatternNN || style == StyleKind::kPatternBI) && op != RenderOp::kStroke) {
    CGFloat wh_f = CGFloat(wh);
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRectI r = _rnd_coord.next_rect_i(bounds, wh, wh);
      uint32_t spriteId = nextSpriteId();

      CGContextDrawImage(_cg_ctx, CGRectMake(CGFloat(r.x), CGFloat(r.y), wh_f, wh_f), _cg_sprites[spriteId]);
    }
  }
  else {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      render_styled_rect<true>(_rnd_coord.next_rect_i(bounds, wh, wh), style, op);
    }
  }
}

void CoreGraphicsModule::render_rect_f(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  double wh = _params.shape_size;

  if (style == StyleKind::kSolid) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      render_solid_rect(_rnd_coord.next_rect(bounds, wh, wh), op);
    }
  }
  else {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      render_styled_rect<true>(_rnd_coord.next_rect(bounds, wh, wh), style, op);
    }
  }
}

void CoreGraphicsModule::render_rect_rotated(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double cx = double(_params.screen_w) * 0.5;
  double cy = double(_params.screen_h) * 0.5;
  double wh = _params.shape_size;
  double angle = 0.0;

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
    BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));

    CGContextSaveGState(_cg_ctx);
    CGContextTranslateCTM(_cg_ctx, CGFloat(cx), CGFloat(cy));
    CGContextRotateCTM(_cg_ctx, CGFloat(angle));
    CGContextTranslateCTM(_cg_ctx, CGFloat(-cx), CGFloat(-cy));

    if (style == StyleKind::kSolid) {
      render_solid_rect(rect, op);
    }
    else {
      render_styled_rect<false>(rect, style, op);
    }

    CGContextRestoreGState(_cg_ctx);
  }
}

void CoreGraphicsModule::render_round_f(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  double wh = _params.shape_size;

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
    double radius = _rnd_extra.next_double(4.0, 40.0);

    CGPathRef path = CGPathCreateWithRoundedRect(
      CGRectMake(rect.x, rect.y, rect.w, rect.h),
      std::min(rect.w * 0.5, radius),
      std::min(rect.h * 0.5, radius),
      nullptr);

    CGContextAddPath(_cg_ctx, path);

    if (style == StyleKind::kSolid) {
      render_solid_path(op);
    }
    else {
      render_styled_path<true>(rect, style, op);
    }

    CGPathRelease(path);
  }
}

void CoreGraphicsModule::render_round_rotated(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double cx = double(_params.screen_w) * 0.5;
  double cy = double(_params.screen_h) * 0.5;
  double wh = _params.shape_size;
  double angle = 0.0;

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
    BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
    double radius = _rnd_extra.next_double(4.0, 40.0);

    CGContextSaveGState(_cg_ctx);
    CGContextTranslateCTM(_cg_ctx, CGFloat(cx), CGFloat(cy));
    CGContextRotateCTM(_cg_ctx, CGFloat(angle));
    CGContextTranslateCTM(_cg_ctx, CGFloat(-cx), CGFloat(-cy));

    CGPathRef path = CGPathCreateWithRoundedRect(
      CGRectMake(rect.x, rect.y, rect.w, rect.h),
      std::min(rect.w * 0.5, radius),
      std::min(rect.h * 0.5, radius),
      nullptr);

    CGContextAddPath(_cg_ctx, path);

    if (style == StyleKind::kSolid) {
      render_solid_path(op);
    }
    else {
      render_styled_path<false>(rect, style, op);
    }

    CGPathRelease(path);
    CGContextRestoreGState(_cg_ctx);
  }
}

void CoreGraphicsModule::render_polygon(RenderOp op, uint32_t complexity) {
  BLSizeI bounds(_params.screen_w - _params.shape_size,
                 _params.screen_h - _params.shape_size);
  StyleKind style = _params.style;
  double wh = double(_params.shape_size);

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLPoint base(_rnd_coord.nextPoint(bounds));

    double x = _rnd_coord.next_double(base.x, base.x + wh);
    double y = _rnd_coord.next_double(base.y, base.y + wh);

    CGContextMoveToPoint(_cg_ctx, CGFloat(x), CGFloat(y));
    for (uint32_t p = 1; p < complexity; p++) {
      x = _rnd_coord.next_double(base.x, base.x + wh);
      y = _rnd_coord.next_double(base.y, base.y + wh);
      CGContextAddLineToPoint(_cg_ctx, CGFloat(x), CGFloat(y));
    }
    CGContextClosePath(_cg_ctx);

    if (style == StyleKind::kSolid) {
      render_solid_path(op);
    }
    else {
      render_styled_path<true>(BLRect(x, y, wh, wh), style, op);
    }
  }
}

void CoreGraphicsModule::render_shape(RenderOp op, ShapeData shape) {
  BLSizeI bounds(_params.screen_w - _params.shape_size,
                 _params.screen_h - _params.shape_size);
  StyleKind style = _params.style;
  double wh = double(_params.shape_size);

  CGMutablePathRef path = CGPathCreateMutable();
  ShapeIterator it(shape);

  while (it.has_command()) {
    if (it.is_move_to()) {
      CGPathMoveToPoint(
        path,
        nullptr,
        it.x(0) * wh, it.y(0) * wh);
    }
    else if (it.is_line_to()) {
      CGPathAddLineToPoint(
        path,
        nullptr,
        it.x(0) * wh, it.y(0) * wh);
    }
    else if (it.is_quad_to()) {
      CGPathAddQuadCurveToPoint(
        path,
        nullptr,
        it.x(0) * wh, it.y(0) * wh,
        it.x(1) * wh, it.y(1) * wh);
    }
    else if (it.is_cubic_to()) {
      CGPathAddCurveToPoint(
        path,
        nullptr,
        it.x(0) * wh, it.y(0) * wh,
        it.x(1) * wh, it.y(1) * wh,
        it.x(2) * wh, it.y(2) * wh);
    }
    else {
      CGPathCloseSubpath(path);
    }
    it.next();
  }

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLPoint base(_rnd_coord.nextPoint(bounds));

    CGContextSaveGState(_cg_ctx);
    CGContextTranslateCTM(_cg_ctx, CGFloat(base.x), CGFloat(base.y));
    CGContextAddPath(_cg_ctx, path);

    if (style == StyleKind::kSolid) {
      render_solid_path(op);
    }
    else {
      render_styled_path<false>(BLRect(base.x, base.y, wh, wh), style, op);
    }

    CGContextRestoreGState(_cg_ctx);
  }

  CGPathRelease(path);
}

Backend* create_cg_backend() {
  return new CoreGraphicsModule();
}

} // {blbench}

#endif // BL_BENCH_ENABLE_COREGRAPHICS
