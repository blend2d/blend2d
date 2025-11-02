// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// This file provides utility classes and functions shared between some tests.

#ifndef BLEND2D_TEST_CONTEXT_UTILITIES_H_INCLUDED
#define BLEND2D_TEST_CONTEXT_UTILITIES_H_INCLUDED

#include <blend2d/blend2d.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

namespace ContextTests {

enum class CommandId : uint32_t {
  kFillRectI = 0,
  kFillRectD,
  kFillMultipleRects,
  kFillRound,
  kFillTriangle,
  kFillPoly10,
  kFillPathQuad,
  kFillPathCubic,
  kFillText,
  kStrokeRectI,
  kStrokeRectD,
  kStrokeMultipleRects,
  kStrokeRound,
  kStrokeTriangle,
  kStrokePoly10,
  kStrokePathQuad,
  kStrokePathCubic,
  kStrokeText,
  kAll,

  kMaxValue = kAll,
  kUnknown = 0xFFFFFFFFu
};

enum class CompOp : uint32_t {
  kSrcOver = BL_COMP_OP_SRC_OVER,
  kSrcCopy = BL_COMP_OP_SRC_COPY,

  kRandom,
  kAll,

  kMaxValue = kAll,
  kUnknown = 0xFFFFFFFFu
};

enum class OpacityOp : uint32_t {
  kOpaque,
  kSemi,
  kTransparent,

  kRandom,
  kAll,

  kMaxValue = kAll,
  kUnknown = 0xFFFFFFFFu
};

enum class StyleId : uint32_t {
  kSolid = 0,
  kSolidOpaque,
  kGradientLinear,
  kGradientLinearDither,
  kGradientRadial,
  kGradientRadialDither,
  kGradientConic,
  kGradientConicDither,
  kPatternAligned,
  kPatternFx,
  kPatternFy,
  kPatternFxFy,
  kPatternAffineNearest,
  kPatternAffineBilinear,

  kRandom,
  kRandomStable,
  kRandomUnstable,

  kAll,
  kAllStable,
  kAllUnstable,

  kMaxValue = kAllUnstable,
  kUnknown = 0xFFFFFFFFu
};

enum class StyleOp : uint32_t {
  kExplicit,
  kImplicit,

  kRandom,
  kAll,

  kMaxValue = kAll,
  kUnknown = 0xFFFFFFFFu
};

static inline bool is_random_style(StyleId style_id) noexcept {
  return style_id >= StyleId::kRandom && style_id <= StyleId::kRandomUnstable;
}

static inline uint32_t maximum_pixel_difference_of(StyleId style_id) noexcept {
  switch (style_id) {
    // These use FMA, thus Portable VS JIT implementation could differ.
    case StyleId::kGradientRadial:
    case StyleId::kGradientRadialDither:
    case StyleId::kGradientConic:
    case StyleId::kGradientConicDither:
    case StyleId::kRandom:
    case StyleId::kRandomUnstable:
      return 2;

    default:
      return 0;
  }
}

namespace StringUtils {

[[maybe_unused]]
static bool strieq(const char* a, const char* b) {
  size_t a_len = strlen(a);
  size_t b_len = strlen(b);

  if (a_len != b_len)
    return false;

  for (size_t i = 0; i < a_len; i++) {
    unsigned ac = (unsigned char)a[i];
    unsigned bc = (unsigned char)b[i];

    if (ac >= 'a' && ac <= 'z') ac -= 'a' - 'A';
    if (bc >= 'a' && bc <= 'z') bc -= 'a' - 'A';

    if (ac != bc)
      return false;
  }

  return true;
}

[[maybe_unused]]
static const char* bool_to_string(bool value) {
  return value ? "true" : "false";
}

[[maybe_unused]]
static const char* cpu_x86_feature_to_string(BLRuntimeCpuFeatures feature) {
  switch (feature) {
    case BL_RUNTIME_CPU_FEATURE_X86_SSE2    : return "sse2";
    case BL_RUNTIME_CPU_FEATURE_X86_SSE3    : return "sse3";
    case BL_RUNTIME_CPU_FEATURE_X86_SSSE3   : return "ssse3";
    case BL_RUNTIME_CPU_FEATURE_X86_SSE4_1  : return "sse4.1";
    case BL_RUNTIME_CPU_FEATURE_X86_SSE4_2  : return "sse4.2";
    case BL_RUNTIME_CPU_FEATURE_X86_AVX     : return "avx";
    case BL_RUNTIME_CPU_FEATURE_X86_AVX2    : return "avx2";
    case BL_RUNTIME_CPU_FEATURE_X86_AVX512  : return "avx512";

    default:
      return "unknown";
  }
}

[[maybe_unused]]
static const char* format_to_string(BLFormat format) {
  switch (format) {
    case BL_FORMAT_NONE  : return "none";
    case BL_FORMAT_PRGB32: return "prgb32";
    case BL_FORMAT_XRGB32: return "xrgb32";
    case BL_FORMAT_A8    : return "a8";

    default:
      return "unknown";
  }
}

[[maybe_unused]]
static const char* style_id_to_string(StyleId style_id) {
  switch (style_id) {
    case StyleId::kSolid                : return "solid";
    case StyleId::kSolidOpaque          : return "solid-opaque";
    case StyleId::kGradientLinear       : return "gradient-linear";
    case StyleId::kGradientLinearDither : return "gradient-linear-dither";
    case StyleId::kGradientRadial       : return "gradient-radial";
    case StyleId::kGradientRadialDither : return "gradient-radial-dither";
    case StyleId::kGradientConic        : return "gradient-conic";
    case StyleId::kGradientConicDither  : return "gradient-conic-dither";
    case StyleId::kPatternAligned       : return "pattern-aligned";
    case StyleId::kPatternFx            : return "pattern-fx";
    case StyleId::kPatternFy            : return "pattern-fy";
    case StyleId::kPatternFxFy          : return "pattern-fx-fy";
    case StyleId::kPatternAffineNearest : return "pattern-affine-nearest";
    case StyleId::kPatternAffineBilinear: return "pattern-affine-bilinear";
    case StyleId::kRandom               : return "random";
    case StyleId::kRandomStable         : return "random-stable";
    case StyleId::kRandomUnstable       : return "random-unstable";
    case StyleId::kAll                  : return "all";
    case StyleId::kAllStable            : return "all-stable";
    case StyleId::kAllUnstable          : return "all-unstable";

    default:
      return "unknown";
  }
}

[[maybe_unused]]
static const char* style_op_to_string(StyleOp style_op) {
  switch (style_op) {
    case StyleOp::kExplicit             : return "explicit";
    case StyleOp::kImplicit             : return "implicit";
    case StyleOp::kRandom               : return "random";

    default:
      return "unknown";
  }
}

[[maybe_unused]]
static const char* comp_op_to_string(CompOp comp_op) {
  switch (comp_op) {
    case CompOp::kSrcOver               : return "src-over";
    case CompOp::kSrcCopy               : return "src-copy";
    case CompOp::kRandom                : return "random";
    case CompOp::kAll                   : return "all";

    default:
      return "unknown";
  }
}

[[maybe_unused]]
static const char* opacity_op_to_string(OpacityOp opacity) {
  switch (opacity) {
    case OpacityOp::kOpaque             : return "opaque";
    case OpacityOp::kSemi               : return "semi";
    case OpacityOp::kTransparent        : return "transparent";
    case OpacityOp::kRandom             : return "random";
    case OpacityOp::kAll                : return "all";

    default:
      return "unknown";
  }
}

[[maybe_unused]]
static const char* command_id_to_string(CommandId command) {
  switch (command) {
    case CommandId::kFillRectI          : return "fill-rect-i";
    case CommandId::kFillRectD          : return "fill-rect-d";
    case CommandId::kFillMultipleRects  : return "fill-multiple-rects";
    case CommandId::kFillRound          : return "fill-round";
    case CommandId::kFillTriangle       : return "fill-triangle";
    case CommandId::kFillPoly10         : return "fill-poly-10";
    case CommandId::kFillPathQuad       : return "fill-path-quad";
    case CommandId::kFillPathCubic      : return "fill-path-cubic";
    case CommandId::kFillText           : return "fill-text";
    case CommandId::kStrokeRectI        : return "stroke-rect-i";
    case CommandId::kStrokeRectD        : return "stroke-rect-d";
    case CommandId::kStrokeMultipleRects: return "stroke-multiple-rects";
    case CommandId::kStrokeRound        : return "stroke-round";
    case CommandId::kStrokeTriangle     : return "stroke-triangle";
    case CommandId::kStrokePoly10       : return "stroke-poly-10";
    case CommandId::kStrokePathQuad     : return "stroke-path-quad";
    case CommandId::kStrokePathCubic    : return "stroke-path-cubic";
    case CommandId::kStrokeText         : return "stroke-text";
    case CommandId::kAll                : return "all";

    default:
      return "unknown";
  }
}

[[maybe_unused]]
static BLFormat parse_format(const char* s) {
  for (uint32_t i = 0; i <= uint32_t(BL_FORMAT_MAX_VALUE); i++)
    if (strieq(s, format_to_string(BLFormat(i))))
      return BLFormat(i);
  return BL_FORMAT_NONE;
}

[[maybe_unused]]
static StyleId parse_style_id(const char* s) {
  for (uint32_t i = 0; i <= uint32_t(StyleId::kMaxValue); i++)
    if (strieq(s, style_id_to_string(StyleId(i))))
      return StyleId(i);
  return StyleId::kUnknown;
}

[[maybe_unused]]
static StyleOp parse_style_op(const char* s) {
  for (uint32_t i = 0; i <= uint32_t(StyleOp::kMaxValue); i++)
    if (strieq(s, style_op_to_string(StyleOp(i))))
      return StyleOp(i);
  return StyleOp::kUnknown;
}

[[maybe_unused]]
static CompOp parse_comp_op(const char* s) {
  for (uint32_t i = 0; i <= uint32_t(CompOp::kMaxValue); i++)
    if (strieq(s, comp_op_to_string(CompOp(i))))
      return CompOp(i);
  return CompOp::kUnknown;
}

[[maybe_unused]]
static OpacityOp parse_opacity_op(const char* s) {
  for (uint32_t i = 0; i <= uint32_t(OpacityOp::kMaxValue); i++)
    if (strieq(s, opacity_op_to_string(OpacityOp(i))))
      return OpacityOp(i);
  return OpacityOp::kUnknown;
}

[[maybe_unused]]
static CommandId parse_command_id(const char* s) {
  for (uint32_t i = 0; i <= uint32_t(CommandId::kMaxValue); i++)
    if (strieq(s, command_id_to_string(CommandId(i))))
      return CommandId(i);
  return CommandId::kUnknown;
}

} // {StringUtils}

class Logger {
public:
  enum class Verbosity : uint32_t {
    Debug,
    Info,
    Silent
  };

  Verbosity _verbosity;

  inline Logger(Verbosity verbosity)
    : _verbosity(verbosity) {}

  inline Verbosity verbosity() const { return _verbosity; }

  inline Verbosity set_verbosity(Verbosity value) {
    Verbosity prev = _verbosity;
    _verbosity = value;
    return prev;
  }

  inline void print(const char* fmt) {
    puts(fmt);
    fflush(stdout);
  }

  template<typename... Args>
  inline void print(const char* fmt, Args&&... args) {
    printf(fmt, BLInternal::forward<Args>(args)...);
    fflush(stdout);
  }

  template<typename... Args>
  inline void debug(const char* fmt, Args&&... args) {
    if (_verbosity <= Verbosity::Debug)
      print(fmt, BLInternal::forward<Args>(args)...);
  }

  template<typename... Args>
  inline void info(const char* fmt, Args&&... args) {
    if (_verbosity <= Verbosity::Info)
      print(fmt, BLInternal::forward<Args>(args)...);
  }
};

struct TestCases {
  //! List of pixel formats to test.
  std::vector<BLFormat> format_ids;
  //! List of commands to test.
  std::vector<CommandId> command_ids;
  //! List of styles test.
  std::vector<StyleId> style_ids;
  //! List of styles operations to test (implicit, explicit, random).
  std::vector<StyleOp> style_ops;
  //! List of composition operators to test (or that should be randomized in random case).
  std::vector<CompOp> comp_ops;
  //! List of opacity operators to test (or that should be randomized in random case).
  std::vector<OpacityOp> opacity_ops;
};

struct TestOptions {
  uint32_t width {};
  uint32_t height {};
  BLFormat format {};
  uint32_t count {};
  uint32_t thread_count {};
  uint32_t seed {};
  CompOp comp_op = CompOp::kSrcOver;
  OpacityOp opacity_op = OpacityOp::kOpaque;
  StyleId style_id = StyleId::kSolid;
  StyleOp style_op = StyleOp::kRandom;
  CommandId command = CommandId::kAll;
  const char* font {};
  uint32_t font_size {};
  uint32_t face_index {};

  bool quiet {};
  bool flush_sync {};
  bool store_images {};
};

class RandomDataGenerator {
public:
  enum class Mode : uint32_t {
    InBounds = 0
  };

  BLRandom _rnd;
  Mode _mode;
  BLBox _bounds;
  BLSize _size;

  RandomDataGenerator()
    : _rnd(0x123456789ABCDEFu),
      _mode(Mode::InBounds),
      _bounds(),
      _size() {}

  inline Mode mode() const { return _mode; }
  inline void set_mode(Mode mode) { _mode = mode; }

  inline const BLBox& bounds() const { return _bounds; }
  inline void set_bounds(const BLBox& bounds) {
    _bounds = bounds;
    _size.reset(_bounds.x1 - _bounds.x0, _bounds.y1 - _bounds.y0);
  }

  inline void seed(uint64_t value) { _rnd.reset(value); }

  inline CompOp next_comp_op() { return CompOp(_rnd.next_uint32() % uint32_t(CompOp::kRandom)); }
  inline BLExtendMode next_pattern_extend() { return BLExtendMode(_rnd.next_uint32() % (BL_EXTEND_MODE_MAX_VALUE + 1u)); }
  inline BLExtendMode next_gradient_extend() { return BLExtendMode(_rnd.next_uint32() % (BL_EXTEND_MODE_SIMPLE_MAX_VALUE + 1u)); }

  inline uint32_t next_uint32() { return _rnd.next_uint32(); }
  inline uint64_t next_uint64() { return _rnd.next_uint64(); }
  inline double next_double() { return _rnd.next_double(); }

  inline BLRgba32 next_rgb32() { return BLRgba32(_rnd.next_uint32() | 0xFF000000u); }
  inline BLRgba32 next_rgba32() { return BLRgba32(_rnd.next_uint32()); }

  inline int next_x_coord_i() { return int((_rnd.next_double() * _size.w) + _bounds.x0); }
  inline int next_y_coord_i() { return int((_rnd.next_double() * _size.h) + _bounds.y0); }

  inline double next_x_coord_d() { return (_rnd.next_double() * _size.w) + _bounds.x0; }
  inline double next_y_coord_d() { return (_rnd.next_double() * _size.h) + _bounds.y0; }

  inline BLPoint next_point_d() { return BLPoint(next_x_coord_d(), next_y_coord_d()); }
  inline BLPointI next_point_i() { return BLPointI(next_x_coord_i(), next_y_coord_i()); }

  inline BLBox next_box_d() {
    double x0 = next_x_coord_d();
    double y0 = next_y_coord_d();
    double x1 = next_x_coord_d();
    double y1 = next_y_coord_d();
    return BLBox(bl_min(x0, x1), bl_min(y0, y1), bl_max(x0, x1), bl_max(y0, y1));
  }

  inline BLBoxI next_box_i() {
    int x0 = next_x_coord_i();
    int y0 = next_y_coord_i();
    int x1 = next_x_coord_i();
    int y1 = next_y_coord_i();

    if (x0 > x1) BLInternal::swap(x0, x1);
    if (y0 > y1) BLInternal::swap(y0, y1);

    if (x0 == x1) x1++;
    if (y0 == y1) y1++;

    return BLBoxI(x0, y0, x1, y1);
  }

  inline BLRectI next_rect_i() {
    BLBoxI box = next_box_i();
    return BLRectI(box.x0, box.y0, box.x1 - box.x0, box.y1 - box.y0);
  }

  inline BLRect next_rect_d() {
    BLBox box = next_box_d();
    return BLRect(box.x0, box.y0, box.x1 - box.x0, box.y1 - box.y0);
  }

  inline BLTriangle next_triangle() {
    BLTriangle out;
    out.x0 = next_x_coord_d();
    out.y0 = next_y_coord_d();
    out.x1 = next_x_coord_d();
    out.y1 = next_y_coord_d();
    out.x2 = next_x_coord_d();
    out.y2 = next_y_coord_d();
    return out;
  }
};

class ContextTester {
public:
  static inline constexpr uint32_t kTextureCount = 8;

  enum class Op { kFill, kStroke };

  const TestCases& _test_cases;

  RandomDataGenerator _rnd;
  BLRandom _rnd_sync;
  BLRandom _rnd_comp_op;
  BLRandom _rnd_opacity_op;
  BLRandom _rnd_opacity_value;
  BLRandom _rnd_style_op;
  const char* _prefix {};
  BLImage _img;
  BLContext _ctx;
  CompOp _comp_op {};
  OpacityOp _opacity_op {};
  StyleId _style_id {};
  StyleOp _style_op {};
  bool _flush_sync {};

  BLImage _textures[kTextureCount];
  BLFontData _font_data;

  ContextTester(const TestCases& test_cases, const char* prefix)
    : _test_cases(test_cases),
      _rnd_sync(0u),
      _prefix(prefix),
      _flush_sync(false) {}

  BLResult init(int w, int h, BLFormat format, const BLContextCreateInfo& cci) {
    BL_PROPAGATE(_img.create(w, h, format));
    BL_PROPAGATE(_ctx.begin(_img, cci));

    double oob = 30;

    _rnd.set_bounds(BLBox(0.0 - oob, 0.0 - oob, w + oob, h + oob));
    _ctx.clear_all();
    _ctx.set_fill_style(BLRgba32(0xFFFFFFFF));

    for (uint32_t i = 0; i < kTextureCount; i++) {
      BL_PROPAGATE(init_texture(i));
    }

    return BL_SUCCESS;
  }

  BLResult init_texture(uint32_t id) noexcept {
    static constexpr int sizes[kTextureCount] = {
      17,
      19,
      47,
      63,
      121,
      345,
      417,
      512
    };

    static constexpr BLFormat formats[kTextureCount] = {
      BL_FORMAT_PRGB32,
      BL_FORMAT_A8,
      BL_FORMAT_PRGB32,
      BL_FORMAT_PRGB32,
      BL_FORMAT_PRGB32,
      BL_FORMAT_A8,
      BL_FORMAT_PRGB32,
      BL_FORMAT_PRGB32
    };

    int size = sizes[id];
    BLFormat format = formats[id];

    BL_PROPAGATE(_textures[id].create(size, size, format));

    // Disable JIT here as we may be testing it in the future. If there is
    // a bug in JIT we want to find it by tests, and not to face it here...
    BLContextCreateInfo cci {};
    cci.flags = BL_CONTEXT_CREATE_FLAG_DISABLE_JIT;

    BLContext ctx;

    BL_PROPAGATE(ctx.begin(_textures[id], cci));
    ctx.clear_all();

    double s = double(size);
    double half = s * 0.5;

    ctx.fill_circle(half, half, half * 1.00, BLRgba32(0xFFFFFFFF));
    ctx.fill_circle(half + half * 0.33, half, half * 0.66, BLRgba32(0xFFFF0000));
    ctx.fill_circle(half, half, half * 0.33, BLRgba32(0xFF0000FF));

    return BL_SUCCESS;
  }

  inline void seed(uint32_t seed) { _rnd.seed(seed); }
  inline void set_options(CompOp comp_op, OpacityOp opacity_op, StyleId style_id, StyleOp style_op) {
    _comp_op = comp_op;
    _opacity_op = opacity_op;
    _style_id = style_id;
    _style_op = style_op;
  }

  inline void set_font_data(const BLFontData& font_data) { _font_data = font_data; }
  inline void set_flush_sync(bool value) { _flush_sync = value; }

  const char* prefix() const { return _prefix; }
  inline const BLImage& image() const { return _img; }

  void reset() {
    _ctx.reset();
    _img.reset();
  }

  void started([[maybe_unused]] const char* test_name) {
    _rnd_sync.reset(0xA29CF911A3B729AFu);
    _rnd_comp_op.reset(0xBF4D32C15432343Fu);
    _rnd_opacity_op.reset(0xFA4DF28C54880133u);
    _rnd_opacity_value.reset(0xF987FCABB3434DDDu);
    _rnd_style_op.reset(0x23BF4E98B4F3AABDu);
  }

  void finished([[maybe_unused]] const char* test_name) {
    _ctx.flush(BL_CONTEXT_FLUSH_SYNC);
  }

  inline void record_iteration([[maybe_unused]] size_t n) {
    if (_flush_sync && _rnd_sync.next_uint32() > 0xF0000000u) {
      _ctx.flush(BL_CONTEXT_FLUSH_SYNC);
    }
  }

  inline StyleId next_style_id() {
    StyleId style_id = _style_id;
    if (is_random_style(style_id)) {
      style_id = _test_cases.style_ids[_rnd.next_uint32() % _test_cases.style_ids.size()];
    }
    return style_id;
  }

  inline StyleOp next_style_op() {
    if (_style_op == StyleOp::kRandom)
      return _test_cases.style_ops[_rnd_style_op.next_uint32() % _test_cases.style_ops.size()];
    else
      return _style_op;
  }

  void setup_common_options(BLContext& ctx) {
    if (_comp_op == CompOp::kRandom) {
      ctx.set_comp_op(BLCompOp(_test_cases.comp_ops[_rnd_comp_op.next_uint32() % _test_cases.comp_ops.size()]));
    }

    if (_opacity_op == OpacityOp::kRandom || _opacity_op == OpacityOp::kSemi) {
      OpacityOp op = _opacity_op;
      if (op == OpacityOp::kRandom) {
        op = _test_cases.opacity_ops[_rnd_opacity_op.next_uint32() % _test_cases.opacity_ops.size()];
      }

      double alpha = 0.0;
      switch (op) {
        case OpacityOp::kOpaque     : alpha = 1.0; break;
        case OpacityOp::kSemi       : alpha = _rnd_opacity_value.next_double(); break;
        case OpacityOp::kTransparent: alpha = 0.0; break;
        default:
          break;
      }

      _ctx.set_global_alpha(alpha);
    }
  }

  void setup_style_options(BLContext& ctx, StyleId style_id) {
    switch (style_id) {
      case StyleId::kGradientLinear:
      case StyleId::kGradientRadial:
      case StyleId::kGradientConic:
        ctx.set_gradient_quality(BL_GRADIENT_QUALITY_NEAREST);
        break;

      case StyleId::kGradientLinearDither:
      case StyleId::kGradientRadialDither:
      case StyleId::kGradientConicDither:
        ctx.set_gradient_quality(BL_GRADIENT_QUALITY_DITHER);
        break;

      case StyleId::kPatternAligned:
      case StyleId::kPatternAffineNearest:
        ctx.set_pattern_quality(BL_PATTERN_QUALITY_NEAREST);
        break;

      case StyleId::kPatternFx:
      case StyleId::kPatternFy:
      case StyleId::kPatternFxFy:
      case StyleId::kPatternAffineBilinear:
        ctx.set_pattern_quality(BL_PATTERN_QUALITY_BILINEAR);
        break;

      default:
        break;
    }
  }

  BLVar materialize_style(StyleId style_id) {
    static constexpr double kPI = 3.14159265358979323846;

    switch (style_id) {
      default:
      case StyleId::kSolid: {
        return BLVar(_rnd.next_rgba32());
      }

      case StyleId::kSolidOpaque: {
        return BLVar(_rnd.next_rgb32());
      }

      case StyleId::kGradientLinear:
      case StyleId::kGradientLinearDither: {
        BLPoint pt0 = _rnd.next_point_d();
        BLPoint pt1 = _rnd.next_point_d();

        BLGradient gradient(BLLinearGradientValues(pt0.x, pt0.y, pt1.x, pt1.y));
        gradient.add_stop(0.0, _rnd.next_rgba32());
        gradient.add_stop(0.5, _rnd.next_rgba32());
        gradient.add_stop(1.0, _rnd.next_rgba32());
        gradient.set_extend_mode(_rnd.next_gradient_extend());
        return BLVar(BLInternal::move(gradient));
      }

      case StyleId::kGradientRadial:
      case StyleId::kGradientRadialDither: {
        // NOTE: It's tricky with radial gradients as FMA and non-FMA implementations will have a different output.
        // So, we quantize input coordinates to integers to minimize the damage, although we cannot avoid it even
        // in this case.
        double rad = floor(_rnd.next_double() * 500 + 20);
        double dist = floor(_rnd.next_double() * (rad - 10));

        double angle = _rnd.next_double() * kPI;
        double as = sin(angle);
        double ac = cos(angle);

        BLPoint pt0 = _rnd.next_point_i();
        BLPoint pt1 = BLPoint(floor(-as * dist), floor(ac * dist)) + pt0;

        BLGradient gradient(BLRadialGradientValues(pt0.x, pt0.y, pt1.x, pt1.y, rad));
        BLRgba32 c = _rnd.next_rgba32();
        gradient.add_stop(0.0, c);
        gradient.add_stop(0.5, _rnd.next_rgba32());
        gradient.add_stop(1.0, c);
        gradient.set_extend_mode(_rnd.next_gradient_extend());
        return BLVar(BLInternal::move(gradient));
      }

      case StyleId::kGradientConic:
      case StyleId::kGradientConicDither: {
        BLPoint pt0 = _rnd.next_point_i();
        double angle = _rnd.next_double() * kPI;

        BLGradient gradient(BLConicGradientValues(pt0.x, pt0.y, angle));
        gradient.add_stop(0.0 , _rnd.next_rgba32());
        gradient.add_stop(0.33, _rnd.next_rgba32());
        gradient.add_stop(0.66, _rnd.next_rgba32());
        gradient.add_stop(1.0 , _rnd.next_rgba32());
        return BLVar(BLInternal::move(gradient));
      }

      case StyleId::kPatternAligned:
      case StyleId::kPatternFx:
      case StyleId::kPatternFy:
      case StyleId::kPatternFxFy: {
        static constexpr double kFracMin = 0.004;
        static constexpr double kFracMax = 0.994;

        uint32_t texture_id = _rnd.next_uint32() % kTextureCount;
        BLExtendMode extend_mode = BLExtendMode(_rnd.next_uint32() % (BL_EXTEND_MODE_MAX_VALUE + 1));

        BLPattern pattern(_textures[texture_id], extend_mode);
        pattern.translate(floor(_rnd.next_double() * double(_rnd._size.w + 200) - 100.0),
                          floor(_rnd.next_double() * double(_rnd._size.h + 200) - 100.0));

        if (style_id == StyleId::kPatternFx || style_id == StyleId::kPatternFxFy) {
          pattern.translate(bl_clamp(_rnd.next_double(), kFracMin, kFracMax), 0.0);
        }

        if (style_id == StyleId::kPatternFy || style_id == StyleId::kPatternFxFy) {
          pattern.translate(0.0, bl_clamp(_rnd.next_double(), kFracMin, kFracMax));
        }

        return BLVar(BLInternal::move(pattern));
      }

      case StyleId::kPatternAffineNearest:
      case StyleId::kPatternAffineBilinear: {
        uint32_t texture_id = _rnd.next_uint32() % kTextureCount;
        BLExtendMode extend_mode = BLExtendMode(_rnd.next_uint32() % (BL_EXTEND_MODE_MAX_VALUE + 1));

        BLPattern pattern(_textures[texture_id]);
        pattern.set_extend_mode(extend_mode);
        pattern.rotate(_rnd.next_double() * (kPI * 2.0));
        pattern.translate(_rnd.next_double() * 300, _rnd.next_double() * 300);
        pattern.scale((_rnd.next_double() + 0.2) * 2.4);
        return BLVar(BLInternal::move(pattern));
      }
    }
  }

  void clear() { _ctx.clear_all(); }

  void render(CommandId command_id, size_t n, const TestOptions& options) {
    const char* test_name = StringUtils::command_id_to_string(command_id);
    started(test_name);

    if (_comp_op != CompOp::kRandom) {
      _ctx.set_comp_op(BLCompOp(_comp_op));
    }

    if (_opacity_op != OpacityOp::kRandom) {
      _ctx.set_global_alpha(_opacity_op == OpacityOp::kOpaque ? 1.0 : 0.0);
    }

    switch (command_id) {
      case CommandId::kFillRectI:
        render_rect_i<Op::kFill>(n);
        break;

      case CommandId::kFillRectD:
        render_rect_d<Op::kFill>(n);
        break;

      case CommandId::kFillMultipleRects:
        render_multiple_rects<Op::kFill>(n);
        break;

      case CommandId::kFillRound:
        render_rounded_rect<Op::kFill>(n);
        break;

      case CommandId::kFillTriangle:
        render_triangle<Op::kFill>(n);
        break;

      case CommandId::kFillPoly10:
        render_poly_10<Op::kFill>(n);
        break;

      case CommandId::kFillPathQuad:
        render_path_quads<Op::kFill>(n);
        break;

      case CommandId::kFillPathCubic:
        render_path_cubics<Op::kFill>(n);
        break;

      case CommandId::kFillText:
        render_text<Op::kFill>(n, options.face_index, float(int(options.font_size)));
        break;

      case CommandId::kStrokeRectI:
        render_rect_i<Op::kStroke>(n);
        break;

      case CommandId::kStrokeRectD:
        render_rect_d<Op::kStroke>(n);
        break;

      case CommandId::kStrokeMultipleRects:
        render_multiple_rects<Op::kStroke>(n);
        break;

      case CommandId::kStrokeRound:
        render_rounded_rect<Op::kStroke>(n);
        break;

      case CommandId::kStrokeTriangle:
        render_triangle<Op::kStroke>(n);
        break;

      case CommandId::kStrokePoly10:
        render_poly_10<Op::kStroke>(n);
        break;

      case CommandId::kStrokePathQuad:
        render_path_quads<Op::kStroke>(n);
        break;

      case CommandId::kStrokePathCubic:
        render_path_cubics<Op::kStroke>(n);
        break;

      case CommandId::kStrokeText:
        render_text<Op::kStroke>(n, options.face_index, float(int(options.font_size)));
        break;

      default:
        break;
    }

    finished(test_name);
  }

  template<Op kOp>
  void render_path(const BLPath& path, StyleId style_id) {
    BLVar style = materialize_style(style_id);

    if (next_style_op() == StyleOp::kExplicit) {
      if constexpr (kOp == Op::kFill) {
        _ctx.fill_path(path, style);
      }
      else {
        _ctx.stroke_path(path, style);
      }
    }
    else {
      if constexpr (kOp == Op::kFill) {
        _ctx.set_fill_style(style);
        _ctx.fill_path(path);
      }
      else {
        _ctx.set_stroke_style(style);
        _ctx.stroke_path(path);
      }
    }
  }
  template<Op kOp>
  void render_rect_i(size_t n) {
    for (size_t i = 0; i < n; i++) {
      StyleId style_id = next_style_id();

      setup_common_options(_ctx);
      setup_style_options(_ctx, style_id);

      BLRectI rect = _rnd.next_rect_i();
      BLVar style = materialize_style(style_id);

      if (next_style_op() == StyleOp::kExplicit) {
        if constexpr (kOp == Op::kFill) {
          _ctx.fill_rect(rect, style);
        }
        else {
          _ctx.stroke_rect(rect, style);
        }
      }
      else {
        if constexpr (kOp == Op::kFill) {
          _ctx.set_fill_style(style);
          _ctx.fill_rect(rect);
        }
        else {
          _ctx.set_stroke_style(style);
          _ctx.stroke_rect(rect);
        }
      }
      record_iteration(i);
    }
  }

  template<Op kOp>
  void render_rect_d(size_t n) {
    for (size_t i = 0; i < n; i++) {
      StyleId style_id = next_style_id();

      setup_common_options(_ctx);
      setup_style_options(_ctx, style_id);

      BLRect rect = _rnd.next_rect_d();
      BLVar style = materialize_style(style_id);

      if (next_style_op() == StyleOp::kExplicit) {
        if constexpr (kOp == Op::kFill) {
          _ctx.fill_rect(rect, style);
        }
        else {
          _ctx.stroke_rect(rect, style);
        }
      }
      else {
        if constexpr (kOp == Op::kFill) {
          _ctx.set_fill_style(style);
          _ctx.fill_rect(rect, style);
        }
        else {
          _ctx.set_stroke_style(style);
          _ctx.stroke_rect(rect, style);
        }
      }

      record_iteration(i);
    }
  }

  template<Op kOp>
  void render_multiple_rects(size_t n) {
    for (size_t i = 0; i < n; i++) {
      StyleId style_id = next_style_id();

      setup_common_options(_ctx);
      setup_style_options(_ctx, style_id);

      BLPath path;
      path.add_rect(_rnd.next_rect_d());
      path.add_rect(_rnd.next_rect_d());

      render_path<kOp>(path, style_id);
      record_iteration(i);
    }
  }

  template<Op kOp>
  void render_rounded_rect(size_t n) {
    for (size_t i = 0; i < n; i++) {
      StyleId style_id = next_style_id();

      setup_common_options(_ctx);
      setup_style_options(_ctx, style_id);

      BLRect rect = _rnd.next_rect_d();
      BLPoint r = _rnd.next_point_d();

      BLVar style = materialize_style(style_id);

      if (next_style_op() == StyleOp::kExplicit) {
        if constexpr (kOp == Op::kFill) {
          _ctx.fill_round_rect(rect.w, rect.y, rect.w, rect.h, r.x, r.y, style);
        }
        else {
          _ctx.stroke_round_rect(rect.w, rect.y, rect.w, rect.h, r.x, r.y, style);
        }
      }
      else {
        if constexpr (kOp == Op::kFill) {
          _ctx.set_fill_style(style);
          _ctx.fill_round_rect(rect.w, rect.y, rect.w, rect.h, r.x, r.y);
        }
        else {
          _ctx.set_stroke_style(style);
          _ctx.stroke_round_rect(rect.w, rect.y, rect.w, rect.h, r.x, r.y);
        }
      }

      record_iteration(i);
    }
  }

  template<Op kOp>
  void render_triangle(size_t n) {
    for (size_t i = 0; i < n; i++) {
      StyleId style_id = next_style_id();

      setup_common_options(_ctx);
      setup_style_options(_ctx, style_id);

      BLTriangle t = _rnd.next_triangle();
      BLVar style = materialize_style(style_id);

      if (next_style_op() == StyleOp::kExplicit) {
        if constexpr (kOp == Op::kFill) {
          _ctx.fill_triangle(t, style);
        }
        else {
          _ctx.stroke_triangle(t, style);
        }
      }
      else {
        if constexpr (kOp == Op::kFill) {
          _ctx.set_fill_style(style);
          _ctx.fill_triangle(t);
        }
        else {
          _ctx.set_stroke_style(style);
          _ctx.stroke_triangle(t);
        }
      }

      record_iteration(i);
    }
  }

  template<Op kOp>
  void render_poly_10(size_t n) {
    constexpr uint32_t kPointCount = 10;
    BLPoint pt[kPointCount];

    BLString s;

    for (size_t i = 0; i < n; i++) {
      StyleId style_id = next_style_id();

      setup_common_options(_ctx);
      setup_style_options(_ctx, style_id);

      for (uint32_t j = 0; j < kPointCount; j++)
        pt[j] = _rnd.next_point_d();

      BLVar style = materialize_style(style_id);

      if (next_style_op() == StyleOp::kExplicit) {
        if constexpr (kOp == Op::kFill) {
          _ctx.fill_polygon(pt, kPointCount, style);
        }
        else {
          _ctx.stroke_polygon(pt, kPointCount, style);
        }
      }
      else {
        if constexpr (kOp == Op::kFill) {
          _ctx.set_fill_style(style);
          _ctx.fill_polygon(pt, kPointCount);
        }
        else {
          _ctx.set_stroke_style(style);
          _ctx.stroke_polygon(pt, kPointCount);
        }
      }
      record_iteration(i);
    }
  }

  template<Op kOp>
  void render_path_quads(size_t n) {
    for (size_t i = 0; i < n; i++) {
      StyleId style_id = next_style_id();

      setup_common_options(_ctx);
      setup_style_options(_ctx, style_id);

      BLPath path;
      path.move_to(_rnd.next_point_d());
      path.quad_to(_rnd.next_point_d(), _rnd.next_point_d());

      render_path<kOp>(path, style_id);
      record_iteration(i);
    }
  }

  template<Op kOp>
  void render_path_cubics(size_t n) {
    for (size_t i = 0; i < n; i++) {
      StyleId style_id = next_style_id();

      setup_common_options(_ctx);
      setup_style_options(_ctx, style_id);

      BLPath path;
      path.move_to(_rnd.next_point_d());
      path.cubic_to(_rnd.next_point_d(), _rnd.next_point_d(), _rnd.next_point_d());

      render_path<kOp>(path, style_id);
      record_iteration(i);
    }
  }

  template<Op kOp>
  void render_text(size_t n, uint32_t face_index, float font_size) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890!@#$%^&*()_{}:;<>?|";

    for (size_t i = 0; i < n; i++) {
      StyleId style_id = next_style_id();

      setup_common_options(_ctx);
      setup_style_options(_ctx, style_id);

      BLFontFace face;
      face.create_from_data(_font_data, face_index);

      BLFont font;
      font.create_from_face(face, font_size);

      // We want to render at least two text runs so there is a chance that text processing
      // and rendering happens in parallel in case the rendering context uses multi-threading.
      uint32_t rnd0 = _rnd.next_uint32();
      uint32_t rnd1 = _rnd.next_uint32();

      char str0[5] {};
      str0[0] = alphabet[((rnd0 >>  0) & 0xFF) % (sizeof(alphabet) - 1u)];
      str0[1] = alphabet[((rnd0 >>  8) & 0xFF) % (sizeof(alphabet) - 1u)];
      str0[2] = alphabet[((rnd0 >> 16) & 0xFF) % (sizeof(alphabet) - 1u)];
      str0[3] = alphabet[((rnd0 >> 24) & 0xFF) % (sizeof(alphabet) - 1u)];

      char str1[5] {};
      str1[0] = alphabet[((rnd1 >>  0) & 0xFF) % (sizeof(alphabet) - 1u)];
      str1[1] = alphabet[((rnd1 >>  8) & 0xFF) % (sizeof(alphabet) - 1u)];
      str1[2] = alphabet[((rnd1 >> 16) & 0xFF) % (sizeof(alphabet) - 1u)];
      str1[3] = alphabet[((rnd1 >> 24) & 0xFF) % (sizeof(alphabet) - 1u)];

      BLPoint pt0 = _rnd.next_point_d();
      BLPoint pt1 = _rnd.next_point_d();
      BLVar style = materialize_style(style_id);

      if (next_style_op() == StyleOp::kExplicit) {
        if constexpr (kOp == Op::kFill) {
          _ctx.fill_utf8_text(pt0, font, BLStringView{str0, 4}, style);
          _ctx.fill_utf8_text(pt1, font, BLStringView{str1, 4}, style);
        }
        else {
          _ctx.stroke_utf8_text(pt0, font, BLStringView{str0, 4}, style);
          _ctx.stroke_utf8_text(pt1, font, BLStringView{str1, 4}, style);
        }
      }
      else {
        if constexpr (kOp == Op::kFill) {
          _ctx.set_fill_style(style);
          _ctx.fill_utf8_text(pt0, font, BLStringView{str0, 4});
          _ctx.fill_utf8_text(pt1, font, BLStringView{str1, 4});
        }
        else {
          _ctx.set_stroke_style(style);
          _ctx.stroke_utf8_text(pt0, font, BLStringView{str0, 4});
          _ctx.stroke_utf8_text(pt1, font, BLStringView{str1, 4});
        }
      }

      record_iteration(i);
    }
  }
};

} // {ContextTests}

#endif // BLEND2D_TEST_CONTEXT_UTILITIES_H_INCLUDED
