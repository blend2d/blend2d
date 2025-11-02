// This file is part of Blend2D project <https://blend2d.com>
//
// See LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifdef BL_BENCH_ENABLE_JUCE

#include <blend2d-testing/bench/bl_bench_app.h>
#include <blend2d-testing/bench/bl_bench_backend.h>

#include <juce_graphics/juce_graphics.h>

namespace blbench {

static inline juce::Image::PixelFormat to_juce_format(BLFormat format) noexcept {
  switch (format) {
    case BL_FORMAT_PRGB32:
      return juce::Image::ARGB;

    case BL_FORMAT_XRGB32:
      return juce::Image::RGB;

    case BL_FORMAT_A8:
      return juce::Image::SingleChannel;

    default:
      return juce::Image::UnknownFormat;
  }
}

static inline BLFormat to_blend2d_format(juce::Image::PixelFormat format) noexcept {
  switch (format) {
    case juce::Image::ARGB:
      return BL_FORMAT_PRGB32;

    case juce::Image::RGB:
      return BL_FORMAT_XRGB32;

    case juce::Image::SingleChannel:
      return BL_FORMAT_A8;

    default:
      return BL_FORMAT_NONE;
  }
}

static void convert_blend2d_image_to_juce_image(juce::Image& dst, BLImage& src, const juce::ImageType& imageType) {
  BLImageData src_data;
  src.get_data(&src_data);

  BLFormat format = BLFormat(src_data.format);
  uint32_t w = uint32_t(src_data.size.w);
  uint32_t h = uint32_t(src_data.size.h);
  size_t row_size = size_t(w) * (bl_format_info[format].depth / 8);

  if (dst.getWidth() != int(w) && dst.getHeight() != h || dst.getFormat() != to_juce_format(format)) {
    dst = juce::Image(to_juce_format(format), int(w), int(h), false, imageType);
  }

  juce::Image::BitmapData dst_data(dst, juce::Image::BitmapData::readWrite);

  if (format == BL_FORMAT_XRGB32) {
    for (uint32_t y = 0; y < h; y++) {
      uint8_t* dst_line = dst_data.data + intptr_t(y) * dst_data.lineStride;
      const uint8_t* src_line = static_cast<const uint8_t*>(src_data.pixel_data) + intptr_t(y) * src_data.stride;

      for (uint32_t x = 0; x < w; x++) {
        dst_line[x * 3 + 0] = src_line[x * 4 + 2];
        dst_line[x * 3 + 1] = src_line[x * 4 + 1];
        dst_line[x * 3 + 2] = src_line[x * 4 + 0];
      }
    }
  }
  else {
    for (uint32_t y = 0; y < h; y++) {
      memcpy(
        dst_data.data + intptr_t(y) * dst_data.lineStride,
        static_cast<const uint8_t*>(src_data.pixel_data) + intptr_t(y) * src_data.stride,
        row_size);
    }
  }
}

static void convert_juce_image_to_blend2d_image(BLImage& dst, juce::Image& src) {
  juce::Image::BitmapData src_data(src, juce::Image::BitmapData::readOnly);
  BLFormat format = to_blend2d_format(src_data.pixelFormat);
  uint32_t w = uint32_t(src_data.width);
  uint32_t h = uint32_t(src_data.height);
  size_t row_size = size_t(w) * (bl_format_info[format].depth / 8);

  BLImageData dst_data;
  dst.create(int(w), int(h), format);
  dst.make_mutable(&dst_data);

  if (format == BL_FORMAT_XRGB32) {
    for (uint32_t y = 0; y < h; y++) {
      uint8_t* dst_line = static_cast<uint8_t*>(dst_data.pixel_data) + intptr_t(y) * dst_data.stride;
      const uint8_t* src_line = src_data.data + intptr_t(y) * src_data.lineStride;

      for (uint32_t x = 0; x < w; x++) {
        dst_line[x * 4 + 0] = src_line[x * 3 + 2];
        dst_line[x * 4 + 1] = src_line[x * 3 + 1];
        dst_line[x * 4 + 2] = src_line[x * 3 + 0];
        dst_line[x * 4 + 3] = uint8_t(0xFF);
      }
    }
  }
  else {
    for (uint32_t y = 0; y < h; y++) {
      memcpy(
        static_cast<uint8_t*>(dst_data.pixel_data) + intptr_t(y) * dst_data.stride,
        src_data.data + intptr_t(y) * src_data.lineStride,
        row_size);
    }
  }
}

static inline juce::Colour toJuceColor(BLRgba32 rgba) noexcept {
  return juce::Colour(
    uint8_t(rgba.r()),
    uint8_t(rgba.g()),
    uint8_t(rgba.b()),
    uint8_t(rgba.a()));
}

struct JuceModule : public Backend {
  juce::SoftwareImageType _juce_image_type;
  juce::PathStrokeType _juce_stroke_type;
  float _line_thickness {};
  uint32_t _opaque_bits {};

  juce::Image _juce_surface;
  juce::Image _juce_sprites[kBenchNumSprites];
  juce::Image _juce_sprites_opaque[kBenchNumSprites];
  juce::Graphics* _juce_context {};

  JuceModule();
  ~JuceModule() override;

  void serialize_info(JSONBuilder& json) const override;

  bool supports_comp_op(BLCompOp comp_op) const override;
  bool supports_style(StyleKind style) const override;

  void before_run() override;
  void flush() override;
  void after_run() override;

  template<typename RectT>
  inline void setup_style(const RectT& rect, StyleKind style);

  void render_rect_a(RenderOp op) override;
  void render_rect_f(RenderOp op) override;
  void render_rect_rotated(RenderOp op) override;
  void render_round_f(RenderOp op) override;
  void render_round_rotated(RenderOp op) override;
  void render_polygon(RenderOp op, uint32_t complexity) override;
  void render_shape(RenderOp op, ShapeData shape) override;
};

JuceModule::JuceModule()
  : _juce_stroke_type(1.0f) {
  strcpy(_name, "JUCE");
}

JuceModule::~JuceModule() {}

void JuceModule::serialize_info(JSONBuilder& json) const {
#if defined(JUCE_VERSION)
  uint32_t major_version = (JUCE_VERSION >> 16);
  uint32_t minor_version = (JUCE_VERSION >> 8) & 0xFF;
  uint32_t patch_version = (JUCE_VERSION >> 0) & 0xFF;

  json.before_record()
      .add_key("version")
      .add_stringf("%u.%u.%u", major_version, minor_version, patch_version);
#endif // JUCE_VERSION
}

bool JuceModule::supports_comp_op(BLCompOp comp_op) const {
  return comp_op == BL_COMP_OP_SRC_OVER ||
         comp_op == BL_COMP_OP_SRC_COPY ;
}

bool JuceModule::supports_style(StyleKind style) const {
  return style == StyleKind::kSolid     ||
         style == StyleKind::kLinearPad ||
         style == StyleKind::kRadialPad ||
         style == StyleKind::kPatternNN ||
         style == StyleKind::kPatternBI ;
}

void JuceModule::before_run() {
  int w = int(_params.screen_w);
  int h = int(_params.screen_h);
  StyleKind style = _params.style;

  _opaque_bits = _params.comp_op == BL_COMP_OP_SRC_COPY ? 0xFF000000u : 0x00000000u;
  _line_thickness = float(_params.stroke_width);
  _juce_stroke_type.setEndStyle(juce::PathStrokeType::butt);
  _juce_stroke_type.setJointStyle(juce::PathStrokeType::mitered);
  _juce_stroke_type.setStrokeThickness(_line_thickness);

  for (uint32_t i = 0; i < kBenchNumSprites; i++) {
    BLImage opaque(_sprites[i]);
    opaque.convert(BL_FORMAT_XRGB32);

    convert_blend2d_image_to_juce_image(_juce_sprites[i], _sprites[i], _juce_image_type);
    convert_blend2d_image_to_juce_image(_juce_sprites_opaque[i], opaque, _juce_image_type);
  }

  // NOTE: There seems to be no way we can use a user provided pixel buffer in JUCE, so let's create two
  // separate buffers so we can copy to our main `_surface` in `after_run()`. The `after_run()` function
  // is excluded from rendering time so there is no penalty for JUCE.
  _juce_surface = juce::Image(to_juce_format(_params.format), w, h, false, _juce_image_type);
  _juce_surface.clear(juce::Rectangle<int>(0, 0, w, h));
  _juce_context = new juce::Graphics(_juce_surface);

  if (_params.style == StyleKind::kPatternBI) {
    _juce_context->setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
  }
  else {
    _juce_context->setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
  }
}

void JuceModule::flush() {
}

void JuceModule::after_run() {
  if (_juce_context) {
    delete _juce_context;
    _juce_context = nullptr;
  }

  convert_juce_image_to_blend2d_image(_surface, _juce_surface);
}

template<typename RectT>
inline void JuceModule::setup_style(const RectT& rect, StyleKind style) {
  switch (style) {
    case StyleKind::kLinearPad:
    case StyleKind::kLinearRepeat:
    case StyleKind::kLinearReflect: {
      BLRgba32 c0 = _rnd_color.next_rgba32(_opaque_bits);
      BLRgba32 c1 = _rnd_color.next_rgba32(_opaque_bits);
      BLRgba32 c2 = _rnd_color.next_rgba32(_opaque_bits);

      float x0 = float(rect.x) + rect.w * float(0.2);
      float y0 = float(rect.y) + rect.h * float(0.2);
      float x1 = float(rect.x) + rect.w * float(0.8);
      float y1 = float(rect.y) + rect.h * float(0.8);

      juce::ColourGradient gradient(toJuceColor(c0), juce::Point<float>(x0, y0), toJuceColor(c2), juce::Point<float>(x1, y1), false);
      gradient.addColour(0.5, toJuceColor(c1));
      _juce_context->setGradientFill(gradient);
      break;
    }

    case StyleKind::kRadialPad:
    case StyleKind::kRadialRepeat:
    case StyleKind::kRadialReflect: {
      BLRgba32 c0 = _rnd_color.next_rgba32(_opaque_bits);
      BLRgba32 c1 = _rnd_color.next_rgba32(_opaque_bits);
      BLRgba32 c2 = _rnd_color.next_rgba32(_opaque_bits);

      float cx = float(rect.x + rect.w / 2);
      float cy = float(rect.y + rect.h / 2);
      float cr = float((rect.w + rect.h) / 4);
      float fx = float(cx - cr / 2);
      float fy = float(cy - cr / 2);

      juce::ColourGradient gradient(toJuceColor(c0), juce::Point<float>(cx, cy), toJuceColor(c2), juce::Point<float>(cx - cr, cy - cr), true);
      gradient.addColour(0.5, toJuceColor(c1));
      _juce_context->setGradientFill(gradient);
      break;
    }

    case StyleKind::kPatternNN:
    case StyleKind::kPatternBI: {
      juce::AffineTransform transform = juce::AffineTransform::translation(float(rect.x), float(rect.y));
      if (_params.comp_op == BL_COMP_OP_SRC_OVER)
        _juce_context->setFillType(juce::FillType(_juce_sprites[nextSpriteId()], transform));
      else
        _juce_context->setFillType(juce::FillType(_juce_sprites_opaque[nextSpriteId()], transform));
      break;
    }

    default:
      return;
  }
}

void JuceModule::render_rect_a(RenderOp op) {
  BLSizeI bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  int wh = _params.shape_size;

  if (style == StyleKind::kSolid) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRectI r = _rnd_coord.next_rect_i(bounds, wh, wh);
      _juce_context->setColour(toJuceColor(_rnd_color.next_rgba32(_opaque_bits)));

      if (op == RenderOp::kStroke)
        _juce_context->drawRect(juce::Rectangle<int>(r.x, r.y, r.w, r.h), _line_thickness);
      else
        _juce_context->fillRect(juce::Rectangle<int>(r.x, r.y, r.w, r.h));
    }
  }
  else if ((style == StyleKind::kPatternNN || style == StyleKind::kPatternBI) && op != RenderOp::kStroke) {
    if (_params.comp_op == BL_COMP_OP_SRC_OVER) {
      for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
        BLRectI rect(_rnd_coord.next_rect_i(bounds, wh, wh));
        _juce_context->drawImageAt(_juce_sprites[nextSpriteId()], rect.x, rect.y);
      }
    }
    else {
      for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
        BLRectI rect(_rnd_coord.next_rect_i(bounds, wh, wh));
        _juce_context->drawImageAt(_juce_sprites_opaque[nextSpriteId()], rect.x, rect.y);
      }
    }
  }
  else {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRectI r = _rnd_coord.next_rect_i(bounds, wh, wh);
      setup_style(r, style);

      if (op == RenderOp::kStroke)
        _juce_context->drawRect(juce::Rectangle<int>(r.x, r.y, r.w, r.h), _line_thickness);
      else
        _juce_context->fillRect(juce::Rectangle<int>(r.x, r.y, r.w, r.h));
    }
  }
}

void JuceModule::render_rect_f(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  double wh = _params.shape_size;

  if (style == StyleKind::kSolid) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRect r = _rnd_coord.next_rect(bounds, wh, wh);
      _juce_context->setColour(toJuceColor(_rnd_color.next_rgba32(_opaque_bits)));

      if (op == RenderOp::kStroke)
        _juce_context->drawRect(juce::Rectangle<float>(float(r.x), float(r.y), float(r.w), float(r.h)), _line_thickness);
      else
        _juce_context->fillRect(juce::Rectangle<float>(float(r.x), float(r.y), float(r.w), float(r.h)));
    }
  }
  else {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRect r = _rnd_coord.next_rect(bounds, wh, wh);
      setup_style(r, style);

      if (op == RenderOp::kStroke)
        _juce_context->drawRect(juce::Rectangle<float>(float(r.x), float(r.y), float(r.w), float(r.h)), _line_thickness);
      else
        _juce_context->fillRect(juce::Rectangle<float>(float(r.x), float(r.y), float(r.w), float(r.h)));
    }
  }
}

void JuceModule::render_rect_rotated(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double cx = double(_params.screen_w) * 0.5;
  double cy = double(_params.screen_h) * 0.5;
  double wh = _params.shape_size;
  double angle = 0.0;

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
    BLRect r(_rnd_coord.next_rect(bounds, wh, wh));
    juce::AffineTransform tr = juce::AffineTransform::rotation(float(angle), float(cx), float(cy));

    _juce_context->saveState();
    _juce_context->addTransform(tr);

    if (style == StyleKind::kSolid) {
      _juce_context->setColour(toJuceColor(_rnd_color.next_rgba32(_opaque_bits)));
    }
    else {
      setup_style(r, style);
    }

    if (op == RenderOp::kStroke)
      _juce_context->drawRect(juce::Rectangle<float>(float(r.x), float(r.y), float(r.w), float(r.h)), _line_thickness);
    else
      _juce_context->fillRect(juce::Rectangle<float>(float(r.x), float(r.y), float(r.w), float(r.h)));

    _juce_context->restoreState();
  }
}

void JuceModule::render_round_f(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  double wh = _params.shape_size;

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLRect r(_rnd_coord.next_rect(bounds, wh, wh));
    float radius = float(_rnd_extra.next_double(4.0, 40.0));

    if (style == StyleKind::kSolid) {
      _juce_context->setColour(toJuceColor(_rnd_color.next_rgba32(_opaque_bits)));
    }
    else {
      setup_style(r, style);
    }

    if (op == RenderOp::kStroke)
      _juce_context->drawRoundedRectangle(float(r.x), float(r.y), float(r.w), float(r.h), radius, _line_thickness);
    else
      _juce_context->fillRoundedRectangle(float(r.x), float(r.y), float(r.w), float(r.h), radius);
  }
}

void JuceModule::render_round_rotated(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double cx = double(_params.screen_w) * 0.5;
  double cy = double(_params.screen_h) * 0.5;
  double wh = _params.shape_size;
  double angle = 0.0;

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
    BLRect r(_rnd_coord.next_rect(bounds, wh, wh));
    float radius = float(_rnd_extra.next_double(4.0, 40.0));
    juce::AffineTransform tr = juce::AffineTransform::rotation(float(angle), float(cx), float(cy));

    _juce_context->saveState();
    _juce_context->addTransform(tr);

    if (style == StyleKind::kSolid) {
      _juce_context->setColour(toJuceColor(_rnd_color.next_rgba32(_opaque_bits)));
    }
    else {
      setup_style(r, style);
    }

    if (op == RenderOp::kStroke)
      _juce_context->drawRoundedRectangle(float(r.x), float(r.y), float(r.w), float(r.h), radius, _line_thickness);
    else
      _juce_context->fillRoundedRectangle(float(r.x), float(r.y), float(r.w), float(r.h), radius);

    _juce_context->restoreState();
  }
}

void JuceModule::render_polygon(RenderOp op, uint32_t complexity) {
  BLSizeI bounds(_params.screen_w - _params.shape_size,
                 _params.screen_h - _params.shape_size);
  StyleKind style = _params.style;
  double wh = double(_params.shape_size);

  juce::Path path;
  path.setUsingNonZeroWinding(op != RenderOp::kFillEvenOdd);

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLPoint base(_rnd_coord.nextPoint(bounds));

    double x = _rnd_coord.next_double(base.x, base.x + wh);
    double y = _rnd_coord.next_double(base.y, base.y + wh);

    path.clear();
    path.startNewSubPath(float(x), float(y));

    for (uint32_t p = 1; p < complexity; p++) {
      x = _rnd_coord.next_double(base.x, base.x + wh);
      y = _rnd_coord.next_double(base.y, base.y + wh);
      path.lineTo(float(x), float(y));
    }

    path.closeSubPath();

    if (style == StyleKind::kSolid) {
      _juce_context->setColour(toJuceColor(_rnd_color.next_rgba32(_opaque_bits)));
    }
    else {
      setup_style(BLRect(x, y, wh, wh), style);
    }

    if (op == RenderOp::kStroke)
      _juce_context->strokePath(path, _juce_stroke_type);
    else
      _juce_context->fillPath(path);
  }
}

void JuceModule::render_shape(RenderOp op, ShapeData shape) {
  BLSizeI bounds(_params.screen_w - _params.shape_size,
                 _params.screen_h - _params.shape_size);
  StyleKind style = _params.style;
  double wh = double(_params.shape_size);

  juce::Path path;
  path.setUsingNonZeroWinding(op != RenderOp::kFillEvenOdd);

  ShapeIterator it(shape);
  while (it.has_command()) {
    if (it.is_move_to()) {
      path.startNewSubPath(
        float(it.x(0) * wh), float(it.y(0) * wh));
    }
    else if (it.is_line_to()) {
      path.lineTo(
        float(it.x(0) * wh), float(it.y(0) * wh));
    }
    else if (it.is_quad_to()) {
      path.quadraticTo(
        float(it.x(0) * wh), float(it.y(0) * wh),
        float(it.x(1) * wh), float(it.y(1) * wh));
    }
    else if (it.is_cubic_to()) {
      path.cubicTo(
        float(it.x(0) * wh), float(it.y(0) * wh),
        float(it.x(1) * wh), float(it.y(1) * wh),
        float(it.x(2) * wh), float(it.y(2) * wh));
    }
    else {
      path.closeSubPath();
    }
    it.next();
  }

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLPoint base(_rnd_coord.nextPoint(bounds));
    juce::AffineTransform transform = juce::AffineTransform::translation(float(base.x), float(base.y));

    if (style == StyleKind::kSolid) {
      _juce_context->setColour(toJuceColor(_rnd_color.next_rgba32(_opaque_bits)));
    }
    else {
      setup_style(BLRect(base.x, base.y, wh, wh), style);
    }

    if (op == RenderOp::kStroke)
      _juce_context->strokePath(path, _juce_stroke_type, transform);
    else
      _juce_context->fillPath(path, transform);
  }
}

Backend* create_juce_backend() {
  return new JuceModule();
}

} // {blbench}

#endif // BL_BENCH_ENABLE_JUCE
