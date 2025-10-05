// This file is part of Blend2D project <https://blend2d.com>
//
// See LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BL_BENCH_BACKEND_H
#define BL_BENCH_BACKEND_H

#include "bl_bench_backend.h"
#include "jsonbuilder.h"
#include "shape_data.h"

#include <blend2d.h>

namespace blbench {

struct BenchApp;

// blbench - Constants
// ===================

enum class BackendKind : uint32_t {
  kBlend2D,
  kAGG,
  kCairo,
  kQt,
  kSkia,
  kJUCE,
  kCoreGraphics,

  kMaxValue = kCoreGraphics
};

enum class TestKind : uint32_t {
  kFillAlignedRect,
  kFillSmoothRect,
  kFillRotatedRect,
  kFillSmoothRound,
  kFillRotatedRound,
  kFillTriangle,
  kFillPolygon10NZ,
  kFillPolygon10EO,
  kFillPolygon20NZ,
  kFillPolygon20EO,
  kFillPolygon40NZ,
  kFillPolygon40EO,
  kFillButterfly,
  kFillFish,
  kFillDragon,
  kFillWorld,

  kStrokeAlignedRect,
  kStrokeSmoothRect,
  kStrokeRotatedRect,
  kStrokeSmoothRound,
  kStrokeRotatedRound,
  kStrokeTriangle,
  kStrokePolygon10,
  kStrokePolygon20,
  kStrokePolygon40,
  kStrokeButterfly,
  kStrokeFish,
  kStrokeDragon,
  kStrokeWorld,

  kMaxValue = kStrokeWorld
};

enum class StyleKind : uint32_t {
  kSolid,
  kLinearPad,
  kLinearRepeat,
  kLinearReflect,
  kRadialPad,
  kRadialRepeat,
  kRadialReflect,
  kConic,
  kPatternNN,
  kPatternBI,

  kMaxValue = kPatternBI
};

enum class RenderOp : uint32_t {
  kFillNonZero,
  kFillEvenOdd,
  kStroke
};

static constexpr uint32_t kBackendKindCount = uint32_t(BackendKind::kMaxValue) + 1;
static constexpr uint32_t kTestKindCount = uint32_t(TestKind::kMaxValue) + 1;
static constexpr uint32_t kStyleKindCount = uint32_t(StyleKind::kMaxValue) + 1;
static constexpr uint32_t kBenchNumSprites = 4;
static constexpr uint32_t kBenchShapeSizeCount = 6;

// blbench::BenchParams
// ====================

struct BenchParams {
  uint32_t screen_w;
  uint32_t screen_h;

  BLFormat format;
  uint32_t quantity;

  TestKind testKind;
  StyleKind style;
  BLCompOp comp_op;
  uint32_t shape_size;

  double stroke_width;
};

// blbench::BenchRandom
// ====================

struct BenchRandom {
  inline BenchRandom(uint64_t seed)
    : _prng(seed),
      _initial(seed) {}

  BLRandom _prng;
  BLRandom _initial;

  inline void rewind() { _prng = _initial; }

  inline int next_int() {
    return int(_prng.next_uint32() & 0x7FFFFFFFu);
  }

  inline int next_int(int a, int b) {
    return int(next_double(double(a), double(b)));
  }

  inline double next_double() {
    return _prng.next_double();
  }

  inline double next_double(double a, double b) {
    return a + _prng.next_double() * (b - a);
  }

  inline BLPoint nextPoint(const BLSizeI& bounds) {
    double x = next_double(0.0, double(bounds.w));
    double y = next_double(0.0, double(bounds.h));
    return BLPoint(x, y);
  }

  inline BLPointI nextIntPoint(const BLSizeI& bounds) {
    int x = next_int(0, bounds.w);
    int y = next_int(0, bounds.h);
    return BLPointI(x, y);
  }

  inline void next_rect_t(BLRect& out, const BLSize& bounds, double w, double h) {
    double x = next_double(0.0, bounds.w - w);
    double y = next_double(0.0, bounds.h - h);
    out.reset(x, y, w, h);
  }

  inline void next_rect_t(BLRectI& out, const BLSizeI& bounds, int w, int h) {
    int x = next_int(0, bounds.w - w);
    int y = next_int(0, bounds.h - h);
    out.reset(x, y, w, h);
  }

  inline BLRect next_rect(const BLSize& bounds, double w, double h) {
    double x = next_double(0.0, bounds.w - w);
    double y = next_double(0.0, bounds.h - h);
    return BLRect(x, y, w, h);
  }

  inline BLRectI next_rect_i(const BLSizeI& bounds, int w, int h) {
    int x = next_int(0, bounds.w - w);
    int y = next_int(0, bounds.h - h);
    return BLRectI(x, y, w, h);
  }

  inline BLRgba32 next_rgb32() {
    return BLRgba32(_prng.next_uint32() | 0xFF000000u);
  }

  inline BLRgba32 next_rgba32() {
    return BLRgba32(_prng.next_uint32());
  }

  inline BLRgba32 next_rgba32(uint32_t mask) {
    return BLRgba32(_prng.next_uint32() | mask);
  }
};

// blbench::Backend
// ====================

struct Backend {
  //! Module name.
  char _name[64] {};
  //! Current parameters.
  BenchParams _params {};
  //! Current duration.
  uint64_t _duration {};

  //! Random number generator for coordinates (points or rectangles).
  BenchRandom _rnd_coord;
  //! Random number generator for colors.
  BenchRandom _rnd_color;
  //! Random number generator for extras (radius).
  BenchRandom _rnd_extra;
  //! Random number generator for sprites.
  uint32_t _rnd_sprite_id {};

  //! Blend surface (used by all modules).
  BLImage _surface;
  //! Sprites.
  BLImage _sprites[kBenchNumSprites];

  Backend();
  virtual ~Backend();

  void run(const BenchApp& app, const BenchParams& params);

  inline const char* name() const { return _name; }

  inline uint32_t nextSpriteId() {
    uint32_t i = _rnd_sprite_id;
    if (++_rnd_sprite_id >= kBenchNumSprites)
      _rnd_sprite_id = 0;
    return i;
  };

  virtual void serialize_info(JSONBuilder& json) const;

  virtual bool supports_comp_op(BLCompOp comp_op) const = 0;
  virtual bool supports_style(StyleKind style) const = 0;

  virtual void before_run() = 0;
  virtual void flush() = 0;
  virtual void after_run() = 0;

  virtual void render_rect_a(RenderOp op) = 0;
  virtual void render_rect_f(RenderOp op) = 0;
  virtual void render_rect_rotated(RenderOp op) = 0;
  virtual void render_round_f(RenderOp op) = 0;
  virtual void render_round_rotated(RenderOp op) = 0;
  virtual void render_polygon(RenderOp op, uint32_t complexity) = 0;
  virtual void render_shape(RenderOp op, ShapeData shape) = 0;
};

Backend* create_blend2d_backend(uint32_t thread_count = 0, uint32_t cpu_features = 0);

#if defined(BL_BENCH_ENABLE_AGG)
Backend* create_agg_backend();
#endif // BL_BENCH_ENABLE_AGG

#if defined(BL_BENCH_ENABLE_CAIRO)
Backend* create_cairo_backend();
#endif // BL_BENCH_ENABLE_CAIRO

#if defined(BL_BENCH_ENABLE_QT)
Backend* create_qt_backend();
#endif // BL_BENCH_ENABLE_QT

#if defined(BL_BENCH_ENABLE_SKIA)
Backend* create_skia_backend();
#endif // BL_BENCH_ENABLE_SKIA

#if defined(BL_BENCH_ENABLE_COREGRAPHICS)
Backend* create_cg_backend();
#endif // BL_BENCH_ENABLE_COREGRAPHICS

#if defined(BL_BENCH_ENABLE_JUCE)
Backend* create_juce_backend();
#endif // BL_BENCH_ENABLE_JUCE


} // {blbench}

#endif // BL_BENCH_BACKEND_H
