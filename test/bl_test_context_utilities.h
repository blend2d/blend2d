// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// This file provides utility classes and functions shared between some tests.

#ifndef BLEND2D_TEST_FUZZ_UTILITIES_H_INCLUDED
#define BLEND2D_TEST_FUZZ_UTILITIES_H_INCLUDED

#include <blend2d.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  kSrcOver,
  kSrcCopy,

  kRandom,
  kAll,
  kUnknown = 0xFFFFFFFFu
};

enum class OpacityOp : uint32_t {
  kOpaque,
  kSemi,
  kTransparent,

  kRandom,
  kAll,
  kUnknown
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
  kMaxValue = kRandom,
  kUnknown = 0xFFFFFFFFu
};

enum class StyleOp : uint32_t {
  kExplicit,
  kImplicit,

  kRandom,
  kUnknown
};

namespace StringUtils {

static bool strieq(const char* a, const char* b) {
  size_t aLen = strlen(a);
  size_t bLen = strlen(b);

  if (aLen != bLen)
    return false;

  for (size_t i = 0; i < aLen; i++) {
    unsigned ac = (unsigned char)a[i];
    unsigned bc = (unsigned char)b[i];

    if (ac >= 'A' && ac <= 'Z') ac += 'A' - 'a';
    if (bc >= 'A' && bc <= 'Z') bc += 'A' - 'a';

    if (ac != bc)
      return false;
  }

  return true;
}

static const char* boolToString(bool value) {
  return value ? "true" : "false";
}

static const char* cpuX86FeatureToString(BLRuntimeCpuFeatures feature) {
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

static const char* formatToString(BLFormat format) {
  switch (format) {
    case BL_FORMAT_NONE  : return "none";
    case BL_FORMAT_PRGB32: return "prgb32";
    case BL_FORMAT_XRGB32: return "xrgb32";
    case BL_FORMAT_A8    : return "a8";

    default:
      return "unknown";
  }
}

static const char* styleIdToString(StyleId styleId) {
  switch (styleId) {
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

    default:
      return "unknown";
  }
}

static const char* styleOpToString(StyleOp styleOp) {
  switch (styleOp) {
    case StyleOp::kExplicit             : return "explicit";
    case StyleOp::kImplicit             : return "implicit";
    case StyleOp::kRandom               : return "random";

    default:
      return "unknown";
  }
}

static const char* compOpToString(CompOp compOp) {
  switch (compOp) {
    case CompOp::kSrcOver               : return "src-over";
    case CompOp::kSrcCopy               : return "src-copy";
    case CompOp::kRandom                : return "random";
    case CompOp::kAll                   : return "all";

    default:
      return "unknown";
  }
}

static const char* opacityOpToString(OpacityOp opacity) {
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

static const char* commandIdToString(CommandId command) {
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

static BLFormat parseFormat(const char* s) {
  for (uint32_t i = 0; i <= uint32_t(BL_FORMAT_MAX_VALUE); i++)
    if (strieq(s, formatToString(BLFormat(i))))
      return BLFormat(i);
  return BL_FORMAT_NONE;
}

static StyleId parseStyleId(const char* s) {
  for (uint32_t i = 0; i <= uint32_t(StyleId::kMaxValue); i++)
    if (strieq(s, styleIdToString(StyleId(i))))
      return StyleId(i);
  return StyleId::kUnknown;
}

static StyleOp parseStyleOp(const char* s) {
  for (uint32_t i = 0; i <= uint32_t(StyleOp::kRandom); i++)
    if (strieq(s, styleOpToString(StyleOp(i))))
      return StyleOp(i);
  return StyleOp::kUnknown;
}

static CompOp parseCompOp(const char* s) {
  for (uint32_t i = 0; i <= uint32_t(CompOp::kAll); i++)
    if (strieq(s, compOpToString(CompOp(i))))
      return CompOp(i);
  return CompOp::kUnknown;
}

static OpacityOp parseOpacityOp(const char* s) {
  for (uint32_t i = 0; i <= uint32_t(OpacityOp::kAll); i++)
    if (strieq(s, opacityOpToString(OpacityOp(i))))
      return OpacityOp(i);
  return OpacityOp::kUnknown;
}

static CommandId parseCommandId(const char* s) {
  for (uint32_t i = 0; i <= uint32_t(CommandId::kMaxValue); i++)
    if (strieq(s, commandIdToString(CommandId(i))))
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

  inline Verbosity setVerbosity(Verbosity value) {
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
    printf(fmt, std::forward<Args>(args)...);
    fflush(stdout);
  }

  template<typename... Args>
  inline void debug(const char* fmt, Args&&... args) {
    if (_verbosity <= Verbosity::Debug)
      print(fmt, std::forward<Args>(args)...);
  }

  template<typename... Args>
  inline void info(const char* fmt, Args&&... args) {
    if (_verbosity <= Verbosity::Info)
      print(fmt, std::forward<Args>(args)...);
  }
};

struct TestOptions {
  uint32_t width {};
  uint32_t height {};
  BLFormat format {};
  uint32_t count {};
  uint32_t threadCount {};
  uint32_t seed {};
  CompOp compOp = CompOp::kSrcOver;
  OpacityOp opacityOp = OpacityOp::kOpaque;
  StyleId styleId = StyleId::kSolid;
  StyleOp styleOp = StyleOp::kRandom;
  CommandId command = CommandId::kAll;
  const char* font {};
  uint32_t fontSize {};
  uint32_t faceIndex {};

  bool quiet {};
  bool flushSync {};
  bool storeImages {};
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
  inline void setMode(Mode mode) { _mode = mode; }

  inline const BLBox& bounds() const { return _bounds; }
  inline void setBounds(const BLBox& bounds) {
    _bounds = bounds;
    _size.reset(_bounds.x1 - _bounds.x0, _bounds.y1 - _bounds.y0);
  }

  inline void seed(uint64_t value) { _rnd.reset(value); }

  inline CompOp nextCompOp() { return CompOp(_rnd.nextUInt32() % uint32_t(CompOp::kRandom)); }
  inline BLExtendMode nextPatternExtend() { return BLExtendMode(_rnd.nextUInt32() % (BL_EXTEND_MODE_MAX_VALUE + 1u)); }
  inline BLExtendMode nextGradientExtend() { return BLExtendMode(_rnd.nextUInt32() % (BL_EXTEND_MODE_SIMPLE_MAX_VALUE + 1u)); }

  inline uint32_t nextUInt32() { return _rnd.nextUInt32(); }
  inline uint64_t nextUInt64() { return _rnd.nextUInt64(); }
  inline double nextDouble() { return _rnd.nextDouble(); }

  inline BLRgba32 nextRgb32() { return BLRgba32(_rnd.nextUInt32() | 0xFF000000u); }
  inline BLRgba32 nextRgba32() { return BLRgba32(_rnd.nextUInt32()); }

  inline int nextXCoordI() { return int((_rnd.nextDouble() * _size.w) + _bounds.x0); }
  inline int nextYCoordI() { return int((_rnd.nextDouble() * _size.h) + _bounds.y0); }

  inline double nextXCoordD() { return (_rnd.nextDouble() * _size.w) + _bounds.x0; }
  inline double nextYCoordD() { return (_rnd.nextDouble() * _size.h) + _bounds.y0; }

  inline BLPoint nextPointD() { return BLPoint(nextXCoordD(), nextYCoordD()); }
  inline BLPointI nextPointI() { return BLPointI(nextXCoordI(), nextYCoordI()); }

  inline BLBox nextBoxD() {
    double x0 = nextXCoordD();
    double y0 = nextYCoordD();
    double x1 = nextXCoordD();
    double y1 = nextYCoordD();
    return BLBox(blMin(x0, x1), blMin(y0, y1), blMax(x0, x1), blMax(y0, y1));
  }

  inline BLBoxI nextBoxI() {
    int x0 = nextXCoordI();
    int y0 = nextYCoordI();
    int x1 = nextXCoordI();
    int y1 = nextYCoordI();

    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);

    if (x0 == x1) x1++;
    if (y0 == y1) y1++;

    return BLBoxI(x0, y0, x1, y1);
  }

  inline BLRectI nextRectI() {
    BLBoxI box = nextBoxI();
    return BLRectI(box.x0, box.y0, box.x1 - box.x0, box.y1 - box.y0);
  }

  inline BLRect nextRectD() {
    BLBox box = nextBoxD();
    return BLRect(box.x0, box.y0, box.x1 - box.x0, box.y1 - box.y0);
  }

  inline BLTriangle nextTriangle() {
    BLTriangle out;
    out.x0 = nextXCoordD();
    out.y0 = nextYCoordD();
    out.x1 = nextXCoordD();
    out.y1 = nextYCoordD();
    out.x2 = nextXCoordD();
    out.y2 = nextYCoordD();
    return out;
  }
};

class ContextTester {
public:
  static constexpr uint32_t kTextureCount = 8;

  enum class Op { kFill, kStroke };

  RandomDataGenerator _rnd;
  BLRandom _rndSync;
  BLRandom _rndCompOp;
  BLRandom _rndOpacityOp;
  BLRandom _rndOpacityValue;
  BLRandom _rndStyleOp;
  const char* _prefix {};
  BLImage _img;
  BLContext _ctx;
  CompOp _compOp {};
  OpacityOp _opacityOp {};
  StyleId _styleId {};
  StyleOp _styleOp {};
  bool _flushSync {};

  BLImage _textures[kTextureCount];
  BLFontData _fontData;

  ContextTester(const char* prefix)
    : _rndSync(0u),
      _prefix(prefix),
      _flushSync(false) {}

  BLResult init(int w, int h, BLFormat format, const BLContextCreateInfo& cci) {
    BL_PROPAGATE(_img.create(w, h, format));
    BL_PROPAGATE(_ctx.begin(_img, cci));

    double oob = 30;

    _rnd.setBounds(BLBox(0.0 - oob, 0.0 - oob, w + oob, h + oob));
    _ctx.clearAll();
    _ctx.setFillStyle(BLRgba32(0xFFFFFFFF));

    for (uint32_t i = 0; i < kTextureCount; i++) {
      BL_PROPAGATE(initTexture(i));
    }

    return BL_SUCCESS;
  }

  BLResult initTexture(uint32_t id) noexcept {
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

    // Disable JIT here as we may be testing it in the future.
    // If there is a bug in JIT we want to find it by tests,
    // and not to face it here...
    BLContextCreateInfo cci {};
    cci.flags = BL_CONTEXT_CREATE_FLAG_DISABLE_JIT;

    BLContext ctx;

    BL_PROPAGATE(ctx.begin(_textures[id], cci));
    ctx.clearAll();

    double s = double(size);
    double half = s * 0.5;

    ctx.fillCircle(half, half, half * 1.00, BLRgba32(0xFFFFFFFF));
    ctx.fillCircle(half + half * 0.33, half, half * 0.66, BLRgba32(0xFFFF0000));
    ctx.fillCircle(half, half, half * 0.33, BLRgba32(0xFF0000FF));

    return BL_SUCCESS;
  }

  inline void seed(uint32_t seed) { _rnd.seed(seed); }
  inline void setOptions(CompOp compOp, OpacityOp opacityOp, StyleId styleId, StyleOp styleOp) {
    _compOp = compOp;
    _opacityOp = opacityOp;
    _styleId = styleId;
    _styleOp = styleOp;
  }

  inline void setFontData(const BLFontData& fontData) { _fontData = fontData; }
  inline void setFlushSync(bool value) { _flushSync = value; }

  const char* prefix() const { return _prefix; }
  inline const BLImage& image() const { return _img; }

  void reset() {
    _ctx.reset();
    _img.reset();
  }

  void started(const char* testName) {
    _rndSync.reset(0xA29CF911A3B729AFu);
    _rndCompOp.reset(0xBF4D32C15432343Fu);
    _rndOpacityOp.reset(0xFA4DF28C54880133u);
    _rndOpacityValue.reset(0xF987FCABB3434DDDu);
    _rndStyleOp.reset(0x23BF4E98B4F3AABDu);
  }

  void finished(const char* testName) {
    _ctx.flush(BL_CONTEXT_FLUSH_SYNC);
  }

  inline void recordIteration(size_t n) {
    if (_flushSync && _rndSync.nextUInt32() > 0xF0000000u)
      _ctx.flush(BL_CONTEXT_FLUSH_SYNC);
  }

  inline StyleId nextStyleId() {
    StyleId styleId = _styleId;
    if (styleId >= StyleId::kRandom)
      styleId = StyleId(_rnd.nextUInt32() % uint32_t(StyleId::kRandom));
    return styleId;
  }

  inline StyleOp nextStyleOp() {
    if (_styleOp == StyleOp::kRandom)
      return StyleOp(_rndStyleOp.nextUInt32() % uint32_t(StyleOp::kRandom));
    else
      return _styleOp;
  }

  void setupCommonOptions(BLContext& ctx) {
    if (_compOp == CompOp::kRandom) {
      ctx.setCompOp(BLCompOp(_rndCompOp.nextUInt32() % uint32_t(CompOp::kRandom)));
    }

    if (_opacityOp == OpacityOp::kRandom || _opacityOp == OpacityOp::kSemi) {
      OpacityOp op = _opacityOp;
      if (op == OpacityOp::kRandom) {
        op = OpacityOp(_rndOpacityOp.nextUInt32() % uint32_t(OpacityOp::kRandom));
      }

      double alpha = 0.0;
      switch (op) {
        case OpacityOp::kOpaque     : alpha = 1.0; break;
        case OpacityOp::kSemi       : alpha = _rndOpacityValue.nextDouble(); break;
        case OpacityOp::kTransparent: alpha = 0.0; break;
        default:
          break;
      }

      _ctx.setGlobalAlpha(alpha);
    }
  }

  void setupStyleOptions(BLContext& ctx, StyleId styleId) {
    switch (styleId) {
      case StyleId::kGradientLinear:
      case StyleId::kGradientRadial:
      case StyleId::kGradientConic:
        ctx.setGradientQuality(BL_GRADIENT_QUALITY_NEAREST);
        break;

      case StyleId::kGradientLinearDither:
      case StyleId::kGradientRadialDither:
      case StyleId::kGradientConicDither:
        ctx.setGradientQuality(BL_GRADIENT_QUALITY_DITHER);
        break;

      case StyleId::kPatternAligned:
      case StyleId::kPatternAffineNearest:
        ctx.setPatternQuality(BL_PATTERN_QUALITY_NEAREST);
        break;

      case StyleId::kPatternFx:
      case StyleId::kPatternFy:
      case StyleId::kPatternFxFy:
      case StyleId::kPatternAffineBilinear:
        ctx.setPatternQuality(BL_PATTERN_QUALITY_BILINEAR);
        break;

      default:
        break;
    }
  }

  BLVar materializeStyle(StyleId styleId) {
    static constexpr double kPI = 3.14159265358979323846;

    switch (styleId) {
      default:
      case StyleId::kSolid: {
        return BLVar(_rnd.nextRgba32());
      }

      case StyleId::kSolidOpaque: {
        return BLVar(_rnd.nextRgb32());
      }

      case StyleId::kGradientLinear:
      case StyleId::kGradientLinearDither: {
        BLPoint pt0 = _rnd.nextPointD();
        BLPoint pt1 = _rnd.nextPointD();

        BLGradient gradient(BLLinearGradientValues(pt0.x, pt0.y, pt1.x, pt1.y));
        gradient.addStop(0.0, _rnd.nextRgba32());
        gradient.addStop(0.5, _rnd.nextRgba32());
        gradient.addStop(1.0, _rnd.nextRgba32());
        gradient.setExtendMode(_rnd.nextGradientExtend());
        return BLVar(std::move(gradient));
      }

      case StyleId::kGradientRadial:
      case StyleId::kGradientRadialDither: {
        // NOTE: It's tricky with radial gradients as FMA and non-FMA implementations will have a different output.
        // So, we quantize input coordinates to integers to minimize the damage, although we cannot avoid it even
        // in this case.
        double rad = std::floor(_rnd.nextDouble() * 500 + 20);
        double dist = std::floor(_rnd.nextDouble() * (rad - 10));

        double angle = _rnd.nextDouble() * kPI;
        double as = std::sin(angle);
        double ac = std::cos(angle);

        BLPoint pt0 = _rnd.nextPointI();
        BLPoint pt1 = BLPoint(std::floor(-as * dist), std::floor(ac * dist)) + pt0;

        BLGradient gradient(BLRadialGradientValues(pt0.x, pt0.y, pt1.x, pt1.y, rad));
        BLRgba32 c = _rnd.nextRgba32();
        gradient.addStop(0.0, c);
        gradient.addStop(0.5, _rnd.nextRgba32());
        gradient.addStop(1.0, c);
        gradient.setExtendMode(_rnd.nextGradientExtend());
        return BLVar(std::move(gradient));
      }

      case StyleId::kGradientConic:
      case StyleId::kGradientConicDither: {
        BLPoint pt0 = _rnd.nextPointI();
        double angle = _rnd.nextDouble() * kPI;

        BLGradient gradient(BLConicGradientValues(pt0.x, pt0.y, angle));
        gradient.addStop(0.0 , _rnd.nextRgba32());
        gradient.addStop(0.33, _rnd.nextRgba32());
        gradient.addStop(0.66, _rnd.nextRgba32());
        gradient.addStop(1.0 , _rnd.nextRgba32());
        return BLVar(std::move(gradient));
      }

      case StyleId::kPatternAligned:
      case StyleId::kPatternFx:
      case StyleId::kPatternFy:
      case StyleId::kPatternFxFy: {
        static constexpr double kFracMin = 0.004;
        static constexpr double kFracMax = 0.994;

        uint32_t textureId = _rnd.nextUInt32() % kTextureCount;
        BLExtendMode extendMode = BLExtendMode(_rnd.nextUInt32() % (BL_EXTEND_MODE_MAX_VALUE + 1));

        BLPattern pattern(_textures[textureId], extendMode);
        pattern.translate(std::floor(_rnd.nextDouble() * double(_rnd._size.w + 200) - 100.0),
                          std::floor(_rnd.nextDouble() * double(_rnd._size.h + 200) - 100.0));

        if (styleId == StyleId::kPatternFx || styleId == StyleId::kPatternFxFy) {
          pattern.translate(blClamp(_rnd.nextDouble(), kFracMin, kFracMax), 0.0);
        }

        if (styleId == StyleId::kPatternFy || styleId == StyleId::kPatternFxFy) {
          pattern.translate(0.0, blClamp(_rnd.nextDouble(), kFracMin, kFracMax));
        }

        return BLVar(std::move(pattern));
      }

      case StyleId::kPatternAffineNearest:
      case StyleId::kPatternAffineBilinear: {
        uint32_t textureId = _rnd.nextUInt32() % kTextureCount;
        BLExtendMode extendMode = BLExtendMode(_rnd.nextUInt32() % (BL_EXTEND_MODE_MAX_VALUE + 1));

        BLPattern pattern(_textures[textureId]);
        pattern.setExtendMode(extendMode);
        pattern.rotate(_rnd.nextDouble() * (kPI * 2.0));
        pattern.translate(_rnd.nextDouble() * 300, _rnd.nextDouble() * 300);
        pattern.scale((_rnd.nextDouble() + 0.2) * 2.4);
        return BLVar(std::move(pattern));
      }
    }
  }

  void clear() { _ctx.clearAll(); }

  void render(CommandId commandId, size_t n, const TestOptions& options) {
    const char* testName = StringUtils::commandIdToString(commandId);
    started(testName);

    if (_compOp != CompOp::kRandom) {
      _ctx.setCompOp(BLCompOp(_compOp));
    }

    if (_opacityOp != OpacityOp::kRandom) {
      _ctx.setGlobalAlpha(_opacityOp == OpacityOp::kOpaque ? 1.0 : 0.0);
    }

    switch (commandId) {
      case CommandId::kFillRectI:
        renderRectI<Op::kFill>(n);
        break;

      case CommandId::kFillRectD:
        renderRectD<Op::kFill>(n);
        break;

      case CommandId::kFillMultipleRects:
        renderMultipleRects<Op::kFill>(n);
        break;

      case CommandId::kFillRound:
        renderRoundedRect<Op::kFill>(n);
        break;

      case CommandId::kFillTriangle:
        renderTriangle<Op::kFill>(n);
        break;

      case CommandId::kFillPoly10:
        renderPoly10<Op::kFill>(n);
        break;

      case CommandId::kFillPathQuad:
        renderPathQuads<Op::kFill>(n);
        break;

      case CommandId::kFillPathCubic:
        renderPathCubics<Op::kFill>(n);
        break;

      case CommandId::kFillText:
        renderText<Op::kFill>(n, options.faceIndex, float(int(options.fontSize)));
        break;

      case CommandId::kStrokeRectI:
        renderRectI<Op::kStroke>(n);
        break;

      case CommandId::kStrokeRectD:
        renderRectD<Op::kStroke>(n);
        break;

      case CommandId::kStrokeMultipleRects:
        renderMultipleRects<Op::kStroke>(n);
        break;

      case CommandId::kStrokeRound:
        renderRoundedRect<Op::kStroke>(n);
        break;

      case CommandId::kStrokeTriangle:
        renderTriangle<Op::kStroke>(n);
        break;

      case CommandId::kStrokePoly10:
        renderPoly10<Op::kStroke>(n);
        break;

      case CommandId::kStrokePathQuad:
        renderPathQuads<Op::kStroke>(n);
        break;

      case CommandId::kStrokePathCubic:
        renderPathCubics<Op::kStroke>(n);
        break;

      case CommandId::kStrokeText:
        renderText<Op::kStroke>(n, options.faceIndex, float(int(options.fontSize)));
        break;

      default:
        break;
    }

    finished(testName);
  }

  template<Op kOp>
  void renderPath(const BLPath& path, StyleId styleId) {
    BLVar style = materializeStyle(styleId);

    if (nextStyleOp() == StyleOp::kExplicit) {
      if (kOp == Op::kFill)
        _ctx.fillPath(path, style);
      else
        _ctx.strokePath(path, style);
    }
    else {
      if (kOp == Op::kFill) {
        _ctx.setFillStyle(style);
        _ctx.fillPath(path);
      }
      else {
        _ctx.setStrokeStyle(style);
        _ctx.strokePath(path);
      }
    }
  }
  template<Op kOp>
  void renderRectI(size_t n) {
    for (size_t i = 0; i < n; i++) {
      StyleId styleId = nextStyleId();

      setupCommonOptions(_ctx);
      setupStyleOptions(_ctx, styleId);

      BLRectI rect = _rnd.nextRectI();
      BLVar style = materializeStyle(styleId);

      if (nextStyleOp() == StyleOp::kExplicit) {
        if (kOp == Op::kFill)
          _ctx.fillRect(rect, style);
        else
          _ctx.strokeRect(rect, style);
      }
      else {
        if (kOp == Op::kFill) {
          _ctx.setFillStyle(style);
          _ctx.fillRect(rect);
        }
        else {
          _ctx.setStrokeStyle(style);
          _ctx.strokeRect(rect);
        }
      }
      recordIteration(i);
    }
  }

  template<Op kOp>
  void renderRectD(size_t n) {
    for (size_t i = 0; i < n; i++) {
      StyleId styleId = nextStyleId();

      setupCommonOptions(_ctx);
      setupStyleOptions(_ctx, styleId);

      BLRect rect = _rnd.nextRectD();
      BLVar style = materializeStyle(styleId);

      if (nextStyleOp() == StyleOp::kExplicit) {
        if (kOp == Op::kFill)
          _ctx.fillRect(rect, style);
        else
          _ctx.strokeRect(rect, style);
      }
      else {
        if (kOp == Op::kFill) {
          _ctx.setFillStyle(style);
          _ctx.fillRect(rect, style);
        }
        else {
          _ctx.setStrokeStyle(style);
          _ctx.strokeRect(rect, style);
        }
      }

      recordIteration(i);
    }
  }

  template<Op kOp>
  void renderMultipleRects(size_t n) {
    for (size_t i = 0; i < n; i++) {
      StyleId styleId = nextStyleId();

      setupCommonOptions(_ctx);
      setupStyleOptions(_ctx, styleId);

      BLPath path;
      path.addRect(_rnd.nextRectD());
      path.addRect(_rnd.nextRectD());

      renderPath<kOp>(path, styleId);
      recordIteration(i);
    }
  }

  template<Op kOp>
  void renderRoundedRect(size_t n) {
    for (size_t i = 0; i < n; i++) {
      StyleId styleId = nextStyleId();

      setupCommonOptions(_ctx);
      setupStyleOptions(_ctx, styleId);

      BLRect rect = _rnd.nextRectD();
      BLPoint r = _rnd.nextPointD();

      BLVar style = materializeStyle(styleId);

      if (nextStyleOp() == StyleOp::kExplicit) {
        if (kOp == Op::kFill)
          _ctx.fillRoundRect(rect.w, rect.y, rect.w, rect.h, r.x, r.y, style);
        else
          _ctx.strokeRoundRect(rect.w, rect.y, rect.w, rect.h, r.x, r.y, style);
      }
      else {
        if (kOp == Op::kFill) {
          _ctx.setFillStyle(style);
          _ctx.fillRoundRect(rect.w, rect.y, rect.w, rect.h, r.x, r.y);
        }
        else {
          _ctx.setStrokeStyle(style);
          _ctx.strokeRoundRect(rect.w, rect.y, rect.w, rect.h, r.x, r.y);
        }
      }

      recordIteration(i);
    }
  }

  template<Op kOp>
  void renderTriangle(size_t n) {
    for (size_t i = 0; i < n; i++) {
      StyleId styleId = nextStyleId();

      setupCommonOptions(_ctx);
      setupStyleOptions(_ctx, styleId);

      BLTriangle t = _rnd.nextTriangle();
      BLVar style = materializeStyle(styleId);

      if (nextStyleOp() == StyleOp::kExplicit) {
        if (kOp == Op::kFill)
          _ctx.fillTriangle(t, style);
        else
          _ctx.strokeTriangle(t, style);
      }
      else {
        if (kOp == Op::kFill) {
          _ctx.setFillStyle(style);
          _ctx.fillTriangle(t);
        }
        else {
          _ctx.setStrokeStyle(style);
          _ctx.strokeTriangle(t);
        }
      }

      recordIteration(i);
    }
  }

  template<Op kOp>
  void renderPoly10(size_t n) {
    constexpr uint32_t kPointCount = 10;
    BLPoint pt[kPointCount];

    BLString s;

    for (size_t i = 0; i < n; i++) {
      StyleId styleId = nextStyleId();

      setupCommonOptions(_ctx);
      setupStyleOptions(_ctx, styleId);

      for (uint32_t j = 0; j < kPointCount; j++)
        pt[j] = _rnd.nextPointD();

      BLVar style = materializeStyle(styleId);

      if (nextStyleOp() == StyleOp::kExplicit) {
        if (kOp == Op::kFill)
          _ctx.fillPolygon(pt, kPointCount, style);
        else
          _ctx.strokePolygon(pt, kPointCount, style);
      }
      else {
        if (kOp == Op::kFill) {
          _ctx.setFillStyle(style);
          _ctx.fillPolygon(pt, kPointCount);
        }
        else {
          _ctx.setStrokeStyle(style);
          _ctx.strokePolygon(pt, kPointCount);
        }
      }
      recordIteration(i);
    }
  }

  template<Op kOp>
  void renderPathQuads(size_t n) {
    for (size_t i = 0; i < n; i++) {
      StyleId styleId = nextStyleId();

      setupCommonOptions(_ctx);
      setupStyleOptions(_ctx, styleId);

      BLPath path;
      path.moveTo(_rnd.nextPointD());
      path.quadTo(_rnd.nextPointD(), _rnd.nextPointD());

      renderPath<kOp>(path, styleId);
      recordIteration(i);
    }
  }

  template<Op kOp>
  void renderPathCubics(size_t n) {
    for (size_t i = 0; i < n; i++) {
      StyleId styleId = nextStyleId();

      setupCommonOptions(_ctx);
      setupStyleOptions(_ctx, styleId);

      BLPath path;
      path.moveTo(_rnd.nextPointD());
      path.cubicTo(_rnd.nextPointD(), _rnd.nextPointD(), _rnd.nextPointD());

      renderPath<kOp>(path, styleId);
      recordIteration(i);
    }
  }

  template<Op kOp>
  void renderText(size_t n, uint32_t faceIndex, float fontSize) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234567890!@#$%^&*()_{}:;<>?|";

    for (size_t i = 0; i < n; i++) {
      StyleId styleId = nextStyleId();

      setupCommonOptions(_ctx);
      setupStyleOptions(_ctx, styleId);

      BLFontFace face;
      face.createFromData(_fontData, faceIndex);

      BLFont font;
      font.createFromFace(face, fontSize);

      // We want to render at least two text runs so there is a chance that text processing
      // and rendering happens in parallel in case the rendering context uses multi-threading.
      uint32_t rnd0 = _rnd.nextUInt32();
      uint32_t rnd1 = _rnd.nextUInt32();

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

      BLPoint pt0 = _rnd.nextPointD();
      BLPoint pt1 = _rnd.nextPointD();
      BLVar style = materializeStyle(styleId);

      if (nextStyleOp() == StyleOp::kExplicit) {
        if (kOp == Op::kFill) {
          _ctx.fillUtf8Text(pt0, font, BLStringView{str0, 4}, style);
          _ctx.fillUtf8Text(pt1, font, BLStringView{str1, 4}, style);
        }
        else {
          _ctx.strokeUtf8Text(pt0, font, BLStringView{str0, 4}, style);
          _ctx.strokeUtf8Text(pt1, font, BLStringView{str1, 4}, style);
        }
      }
      else {
        if (kOp == Op::kFill) {
          _ctx.setFillStyle(style);
          _ctx.fillUtf8Text(pt0, font, BLStringView{str0, 4});
          _ctx.fillUtf8Text(pt1, font, BLStringView{str1, 4});
        }
        else {
          _ctx.setStrokeStyle(style);
          _ctx.strokeUtf8Text(pt0, font, BLStringView{str0, 4});
          _ctx.strokeUtf8Text(pt1, font, BLStringView{str1, 4});
        }
      }

      recordIteration(i);
    }
  }
};

} // {ContextTests}

#endif // BLEND2D_TEST_FUZZ_UTILITIES_H_INCLUDED
