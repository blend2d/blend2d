// This file is part of Blend2D project <https://blend2d.com>
//
// See LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d-testing/bench/bl_bench_app.h>
#include <blend2d-testing/bench/bl_bench_backend.h>
#include <blend2d-testing/bench/shape_data.h>

#include <chrono>

namespace blbench {

// blbench::Backend - Construction & Destruction
// =================================================

Backend::Backend()
  : _name(),
    _params(),
    _duration(0),
    _rnd_coord(0x19AE0DDAE3FA7391ull),
    _rnd_color(0x94BD7A499AD10011ull),
    _rnd_extra(0x1ABD9CC9CAF0F123ull),
    _rnd_sprite_id(0) {}
Backend::~Backend() {}

// blbench::Backend - Run
// ==========================

static void BenchModule_shape_helper(Backend* mod, RenderOp op, ShapeKind shapeKind) {
  ShapeData shapeData;
  get_shape_data(shapeData, shapeKind);
  mod->render_shape(op, shapeData);
}

void Backend::run(const BenchApp& app, const BenchParams& params) {
  _params = params;

  _rnd_coord.rewind();
  _rnd_color.rewind();
  _rnd_extra.rewind();
  _rnd_sprite_id = 0;

  // Initialize the sprites.
  for (uint32_t i = 0; i < kBenchNumSprites; i++) {
    _sprites[i] = app.get_scaled_sprite(i, params.shape_size);
  }

  before_run();
  auto start = std::chrono::high_resolution_clock::now();

  switch (_params.testKind) {
    case TestKind::kFillAlignedRect   : render_rect_a(RenderOp::kFillNonZero); break;
    case TestKind::kFillSmoothRect    : render_rect_f(RenderOp::kFillNonZero); break;
    case TestKind::kFillRotatedRect   : render_rect_rotated(RenderOp::kFillNonZero); break;
    case TestKind::kFillSmoothRound   : render_round_f(RenderOp::kFillNonZero); break;
    case TestKind::kFillRotatedRound  : render_round_rotated(RenderOp::kFillNonZero); break;
    case TestKind::kFillTriangle      : render_polygon(RenderOp::kFillNonZero, 3); break;
    case TestKind::kFillPolygon10NZ   : render_polygon(RenderOp::kFillNonZero, 10); break;
    case TestKind::kFillPolygon10EO   : render_polygon(RenderOp::kFillEvenOdd, 10); break;
    case TestKind::kFillPolygon20NZ   : render_polygon(RenderOp::kFillNonZero, 20); break;
    case TestKind::kFillPolygon20EO   : render_polygon(RenderOp::kFillEvenOdd, 20); break;
    case TestKind::kFillPolygon40NZ   : render_polygon(RenderOp::kFillNonZero, 40); break;
    case TestKind::kFillPolygon40EO   : render_polygon(RenderOp::kFillEvenOdd, 40); break;
    case TestKind::kFillButterfly     : BenchModule_shape_helper(this, RenderOp::kFillNonZero, ShapeKind::kButterfly); break;
    case TestKind::kFillFish          : BenchModule_shape_helper(this, RenderOp::kFillNonZero, ShapeKind::kFish); break;
    case TestKind::kFillDragon        : BenchModule_shape_helper(this, RenderOp::kFillNonZero, ShapeKind::kDragon); break;
    case TestKind::kFillWorld         : BenchModule_shape_helper(this, RenderOp::kFillNonZero, ShapeKind::kWorld); break;

    case TestKind::kStrokeAlignedRect : render_rect_a(RenderOp::kStroke); break;
    case TestKind::kStrokeSmoothRect  : render_rect_f(RenderOp::kStroke); break;
    case TestKind::kStrokeRotatedRect : render_rect_rotated(RenderOp::kStroke); break;
    case TestKind::kStrokeSmoothRound : render_round_f(RenderOp::kStroke); break;
    case TestKind::kStrokeRotatedRound: render_round_rotated(RenderOp::kStroke); break;
    case TestKind::kStrokeTriangle    : render_polygon(RenderOp::kStroke, 3); break;
    case TestKind::kStrokePolygon10   : render_polygon(RenderOp::kStroke, 10); break;
    case TestKind::kStrokePolygon20   : render_polygon(RenderOp::kStroke, 20); break;
    case TestKind::kStrokePolygon40   : render_polygon(RenderOp::kStroke, 40); break;
    case TestKind::kStrokeButterfly   : BenchModule_shape_helper(this, RenderOp::kStroke, ShapeKind::kButterfly); break;
    case TestKind::kStrokeFish        : BenchModule_shape_helper(this, RenderOp::kStroke, ShapeKind::kFish); break;
    case TestKind::kStrokeDragon      : BenchModule_shape_helper(this, RenderOp::kStroke, ShapeKind::kDragon); break;
    case TestKind::kStrokeWorld       : BenchModule_shape_helper(this, RenderOp::kStroke, ShapeKind::kWorld); break;
  }

  flush();

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;
  _duration = uint64_t(elapsed.count() * 1000000);

  after_run();
}

void Backend::serialize_info(JSONBuilder& json) const { (void)json; }

} // {blbench}
