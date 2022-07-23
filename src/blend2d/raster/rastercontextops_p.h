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

namespace BLRasterEngine {

// The purpose of this file is to share as much as possible between both sync async implementations.

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

BL_HIDDEN BLResult blRasterContextBuildPolyEdges(WorkData* workData, const BLPointI* pts, size_t size, const BLMatrix2D& m, uint32_t mType) noexcept;
BL_HIDDEN BLResult blRasterContextBuildPolyEdges(WorkData* workData, const BLPoint* pts, size_t size, const BLMatrix2D& m, uint32_t mType) noexcept;
BL_HIDDEN BLResult blRasterContextBuildPathEdges(WorkData* workData, const BLPathView& pathView, const BLMatrix2D& m, uint32_t mType) noexcept;

//! Edge builder sink - acts as a base class for other sinks, but can also be used as is, for example
//! by `FillGlyphOutline()` implementation.
struct EdgeBuilderSink {
  EdgeBuilder<int>* edgeBuilder;
};

//! Passes the stroked paths to `EdgeBuilder` and flips signs where necessary. This is much better than
//! using BLPath::addStrokedPath() as no reversal of `b` path is necessary, instead we flip sign of such
//! path directly in the EdgeBuilder.
struct StrokeSink : public EdgeBuilderSink {
  const BLMatrix2D* matrix;
  uint32_t matrixType;
};

struct StrokeGlyphRunSink : public StrokeSink {
  BLPath* paths;
  const BLStrokeOptions* strokeOptions;
  const BLApproximationOptions* approximationOptions;
};

BL_HIDDEN BLResult BL_CDECL blRasterContextFillGlyphRunSinkFunc(BLPathCore* path, const void* info, void* closure_) noexcept;
BL_HIDDEN BLResult BL_CDECL blRasterContextStrokeGeometrySinkFunc(BLPath* a, BLPath* b, BLPath* c, void* closure_) noexcept;
BL_HIDDEN BLResult BL_CDECL blRasterContextStrokeGlyphRunSinkFunc(BLPathCore* path, const void* info, void* closure_) noexcept;

template<typename StateAccessor>
static BL_INLINE BLResult blRasterContextUtilFillGlyphRun(
  WorkData* workData,
  const StateAccessor& accessor,
  const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  BLMatrix2D m(accessor.finalMatrixFixed());
  m.translate(*pt);

  BLPath* path = &workData->tmpPath[3];
  path->clear();

  EdgeBuilderSink sink;
  sink.edgeBuilder = &workData->edgeBuilder;
  sink.edgeBuilder->begin();

  BLResult result = blFontGetGlyphRunOutlines(font, glyphRun, &m, path, blRasterContextFillGlyphRunSinkFunc, &sink);

  // EdgeBuilder::done() can only fail on out of memory condition.
  if (BL_LIKELY(result == BL_SUCCESS))
    result = workData->edgeBuilder.done();

  if (BL_LIKELY(result == BL_SUCCESS))
    return result;

  workData->revertEdgeBuilder();
  return workData->accumulateError(result);
}

template<typename StateAccessor>
static BL_INLINE BLResult blRasterContextUtilStrokeUnsafePath(
  WorkData* workData,
  const StateAccessor& accessor,
  const BLPath* path) noexcept {

  StrokeSink sink;
  sink.edgeBuilder = &workData->edgeBuilder;
  sink.matrix = &accessor.finalMatrixFixed();
  sink.matrixType = accessor.finalMatrixFixedType();

  BLPath* a = &workData->tmpPath[0];
  BLPath* b = &workData->tmpPath[1];
  BLPath* c = &workData->tmpPath[2];

  if (accessor.strokeOptions().transformOrder != BL_STROKE_TRANSFORM_ORDER_AFTER) {
    a->clear();
    BL_PROPAGATE(blPathAddTransformedPath(a, path, nullptr, &accessor.userMatrix()));

    path = a;
    a = &workData->tmpPath[3];

    sink.matrix = &accessor.metaMatrixFixed();
    sink.matrixType = accessor.metaMatrixFixedType();
  }

  a->clear();
  workData->edgeBuilder.begin();

  BLResult result = BLPathPrivate::strokePath(
    path->view(),
    accessor.strokeOptions(),
    accessor.approximationOptions(),
    *a, *b, *c,
    blRasterContextStrokeGeometrySinkFunc, &sink);

  // EdgeBuilder::done() can only fail on out of memory condition.
  if (BL_LIKELY(result == BL_SUCCESS))
    result = workData->edgeBuilder.done();

  if (BL_LIKELY(result == BL_SUCCESS))
    return result;

  workData->revertEdgeBuilder();
  return workData->accumulateError(result);
}

template<typename StateAccessor>
static BL_INLINE BLResult blRasterContextUtilStrokeGlyphRun(
  WorkData* workData,
  const StateAccessor& accessor,
  const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  StrokeGlyphRunSink sink;
  sink.edgeBuilder = &workData->edgeBuilder;
  sink.paths = workData->tmpPath;
  sink.strokeOptions = &accessor.strokeOptions();
  sink.approximationOptions = &accessor.approximationOptions();

  BLMatrix2D preMatrix;
  if (accessor.strokeOptions().transformOrder != BL_STROKE_TRANSFORM_ORDER_AFTER) {
    preMatrix = accessor.userMatrix();
    preMatrix.translate(*pt);
    sink.matrix = &accessor.metaMatrixFixed();
    sink.matrixType = accessor.metaMatrixFixedType();
  }
  else {
    preMatrix.resetToTranslation(*pt);
    sink.matrix = &accessor.finalMatrixFixed();
    sink.matrixType = accessor.finalMatrixFixedType();
  }

  BLPath* tmpPath = &workData->tmpPath[3];
  tmpPath->clear();
  workData->edgeBuilder.begin();

  BLResult result = blFontGetGlyphRunOutlines(font, glyphRun, &preMatrix, tmpPath, blRasterContextStrokeGlyphRunSinkFunc, &sink);

  // EdgeBuilder::done() can only fail on out of memory condition.
  if (BL_LIKELY(result == BL_SUCCESS))
    result = workData->edgeBuilder.done();

  if (BL_LIKELY(result == BL_SUCCESS))
    return result;

  workData->revertEdgeBuilder();
  return workData->accumulateError(result);
}

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERCONTEXTOPS_P_H_INCLUDED
