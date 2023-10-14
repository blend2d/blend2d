// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RASTERCONTEXTOPS_P_H_INCLUDED
#define BLEND2D_RASTER_RASTERCONTEXTOPS_P_H_INCLUDED

#include "../path_p.h"
#include "../pathstroke_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/workdata_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace bl {
namespace RasterEngine {

// The purpose of this file is to share as much as possible between both sync and async implementations.

// TODO: [Rendering Context] SIMD idea: shift left by 24 and check whether all bytes are zero.
static BL_INLINE bool isBoxAligned24x8(const BLBoxI& box) noexcept {
  if (BL_TARGET_ARCH_BITS < 64) {
    return ((box.x0 | box.y0 | box.x1 | box.y1) & 0xFF) == 0;
  }
  else {
    // Compilers like this one more than the previous code in 64-bit mode.
    uint64_t a;
    uint64_t b;

    memcpy(&a, &box.x0, sizeof(uint64_t));
    memcpy(&b, &box.x1, sizeof(uint64_t));

    return ((a | b) & 0x000000FF000000FF) == 0;
  }
}

BL_HIDDEN BLResult addFilledPolygonEdges(WorkData* workData, const BLPointI* pts, size_t size, const BLMatrix2D& transform, BLTransformType transformType) noexcept;
BL_HIDDEN BLResult addFilledPolygonEdges(WorkData* workData, const BLPoint* pts, size_t size, const BLMatrix2D& transform, BLTransformType transformType) noexcept;
BL_HIDDEN BLResult addFilledPathEdges(WorkData* workData, const BLPathView& pathView, const BLMatrix2D& transform, BLTransformType transformType) noexcept;

//! Edge builder sink - acts as a base class for other sinks, but can also be used as is, for example
//! by `addFilledGlyphRunEdges()` implementation.
struct EdgeBuilderSink {
  EdgeBuilder<int>* edgeBuilder;
};

//! Passes the stroked paths to `EdgeBuilder` and flips signs where necessary. This is much better than
//! using BLPath::addStrokedPath() as no reversal of `b` path is necessary, instead we flip sign of such
//! path directly in the EdgeBuilder.
struct StrokeSink : public EdgeBuilderSink {
  const BLMatrix2D* transform;
  BLTransformType transformType;
};

struct StrokeGlyphRunSink : public StrokeSink {
  BLPath* paths;
  const BLStrokeOptions* strokeOptions;
  const BLApproximationOptions* approximationOptions;
};

BL_HIDDEN BLResult BL_CDECL fillGlyphRunSink(BLPathCore* path, const void* info, void* userData) noexcept;
BL_HIDDEN BLResult BL_CDECL strokeGeometrySink(BLPathCore* a, BLPathCore* b, BLPathCore* c, size_t figureStart, size_t figureEnd, void* userData) noexcept;
BL_HIDDEN BLResult BL_CDECL strokeGlyphRunSink(BLPathCore* path, const void* info, void* userData) noexcept;

template<typename StateAccessor>
static BL_INLINE BLResult addFilledGlyphRunEdges(
  WorkData* workData,
  const StateAccessor& accessor,
  const BLPoint& originFixed, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  BLMatrix2D transform(accessor.finalTransformFixed(originFixed));
  BLPath* path = &workData->tmpPath[3];
  path->clear();

  EdgeBuilderSink sink;
  sink.edgeBuilder = &workData->edgeBuilder;
  sink.edgeBuilder->begin();

  BLResult result = blFontGetGlyphRunOutlines(font, glyphRun, &transform, path, fillGlyphRunSink, &sink);

  // EdgeBuilder::done() can only fail on out of memory condition.
  if (BL_LIKELY(result == BL_SUCCESS)) {
    result = workData->edgeBuilder.done();
    if (BL_LIKELY(result == BL_SUCCESS))
      return result;
  }

  workData->revertEdgeBuilder();
  return workData->accumulateError(result);
}

template<typename StateAccessor>
static BL_INLINE BLResult addStrokedPathEdges(
  WorkData* workData,
  const StateAccessor& accessor,
  const BLPoint& originFixed, const BLPath* path) noexcept {

  StrokeSink sink;
  BLMatrix2D transform = accessor.finalTransformFixed(originFixed);

  sink.edgeBuilder = &workData->edgeBuilder;
  sink.transform = &transform;
  sink.transformType = accessor.finalTransformFixedType();

  BLPath* a = &workData->tmpPath[0];
  BLPath* b = &workData->tmpPath[1];
  BLPath* c = &workData->tmpPath[2];

  if (accessor.strokeOptions().transformOrder != BL_STROKE_TRANSFORM_ORDER_AFTER) {
    a->clear();
    BL_PROPAGATE(a->addPath(*path, accessor.userTransform()));

    path = a;
    a = &workData->tmpPath[3];

    transform = accessor.metaTransformFixed(originFixed);
    sink.transformType = accessor.metaTransformFixedType();
  }

  a->clear();
  workData->edgeBuilder.begin();

  BLResult result = PathInternal::strokePath(
    path->view(),
    accessor.strokeOptions(),
    accessor.approximationOptions(),
    *a, *b, *c,
    strokeGeometrySink, &sink);

  // EdgeBuilder::done() can only fail on out of memory condition.
  if (BL_LIKELY(result == BL_SUCCESS)) {
    result = workData->edgeBuilder.done();
    if (BL_LIKELY(result == BL_SUCCESS)) {
      return result;
    }
  }

  workData->revertEdgeBuilder();
  return workData->accumulateError(result);
}

template<typename StateAccessor>
static BL_INLINE BLResult addStrokedGlyphRunEdges(
  WorkData* workData,
  const StateAccessor& accessor,
  const BLPoint& originFixed, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  StrokeGlyphRunSink sink;
  sink.edgeBuilder = &workData->edgeBuilder;
  sink.paths = workData->tmpPath;
  sink.strokeOptions = &accessor.strokeOptions();
  sink.approximationOptions = &accessor.approximationOptions();

  BLMatrix2D glyphRunTransform;
  BLMatrix2D finalTransformFixed;

  if (accessor.strokeOptions().transformOrder == BL_STROKE_TRANSFORM_ORDER_AFTER) {
    glyphRunTransform.reset();
    finalTransformFixed = accessor.finalTransformFixed(originFixed);

    sink.transform = &finalTransformFixed;
    sink.transformType = accessor.finalTransformFixedType();
  }
  else {
    glyphRunTransform = accessor.userTransform();
    finalTransformFixed = accessor.metaTransformFixed(originFixed);

    sink.transform = &finalTransformFixed;
    sink.transformType = accessor.metaTransformFixedType();
  }

  BLPath* tmpPath = &workData->tmpPath[3];
  tmpPath->clear();
  workData->edgeBuilder.begin();

  // EdgeBuilder::done() can only fail on out of memory condition.
  BLResult result = blFontGetGlyphRunOutlines(font, glyphRun, &glyphRunTransform, tmpPath, strokeGlyphRunSink, &sink);
  if (BL_LIKELY(result == BL_SUCCESS)) {
    result = workData->edgeBuilder.done();
    if (BL_LIKELY(result == BL_SUCCESS)) {
      return result;
    }
  }

  workData->revertEdgeBuilder();
  return workData->accumulateError(result);
}

} // {RasterEngine}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERCONTEXTOPS_P_H_INCLUDED
