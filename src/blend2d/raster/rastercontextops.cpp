// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../path_p.h"
#include "../pathstroke_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rastercontextops_p.h"
#include "../raster/workdata_p.h"

namespace BLRasterEngine {

// RasterEngine - Edge Building Utilities
// ======================================

template<typename PointType>
static BL_INLINE BLResult blRasterContextBuildPolyEdgesT(
  WorkData* workData,
  const PointType* pts, size_t size, const BLMatrix2D& m, uint32_t mType) noexcept {

  BLResult result = workData->edgeBuilder.initFromPoly(pts, size, m, mType);
  if (BL_LIKELY(result == BL_SUCCESS))
    return result;

  workData->revertEdgeBuilder();
  return workData->accumulateError(result);
}

BLResult blRasterContextBuildPolyEdges(WorkData* workData, const BLPointI* pts, size_t size, const BLMatrix2D& m, uint32_t mType) noexcept {
  return blRasterContextBuildPolyEdgesT(workData, pts, size, m, mType);
}

BLResult blRasterContextBuildPolyEdges(WorkData* workData, const BLPoint* pts, size_t size, const BLMatrix2D& m, uint32_t mType) noexcept {
  return blRasterContextBuildPolyEdgesT(workData, pts, size, m, mType);
}

BLResult blRasterContextBuildPathEdges(WorkData* workData, const BLPathView& pathView, const BLMatrix2D& m, uint32_t mType) noexcept {
  BLResult result = workData->edgeBuilder.initFromPath(pathView, true, m, mType);
  if (BL_LIKELY(result == BL_SUCCESS))
    return result;

  workData->revertEdgeBuilder();
  return workData->accumulateError(result);
}

// RasterEngine - Sinks & Sink Utilities
// =====================================

BLResult blRasterContextFillGlyphRunSinkFunc(BLPathCore* path, const void* info, void* closure_) noexcept {
  blUnused(info);

  EdgeBuilderSink* sink = static_cast<EdgeBuilderSink*>(closure_);
  EdgeBuilder<int>* edgeBuilder = sink->edgeBuilder;

  BL_PROPAGATE(edgeBuilder->addPath(path->dcast().view(), true, BLTransformPrivate::identityTransform, BL_MATRIX2D_TYPE_IDENTITY));
  return path->dcast().clear();
}

BLResult blRasterContextStrokeGeometrySinkFunc(BLPath* a, BLPath* b, BLPath* c, void* closure_) noexcept {
  StrokeSink* self = static_cast<StrokeSink*>(closure_);
  EdgeBuilder<int>* edgeBuilder = self->edgeBuilder;

  BL_PROPAGATE(edgeBuilder->addPath(a->view(), false, *self->matrix, self->matrixType));
  BL_PROPAGATE(edgeBuilder->addReversePathFromStrokeSink(b->view(), *self->matrix, self->matrixType));

  if (!c->empty())
    BL_PROPAGATE(edgeBuilder->addPath(c->view(), false, *self->matrix, self->matrixType));

  return a->clear();
}

BLResult blRasterContextStrokeGlyphRunSinkFunc(BLPathCore* path, const void* info, void* closure_) noexcept {
  blUnused(info);

  StrokeGlyphRunSink* sink = static_cast<StrokeGlyphRunSink*>(closure_);
  BLPath* a = &sink->paths[0];
  BLPath* b = &sink->paths[1];
  BLPath* c = &sink->paths[2];

  a->clear();
  BLResult localResult = BLPathPrivate::strokePath(
    path->dcast().view(),
    *sink->strokeOptions,
    *sink->approximationOptions,
    *a, *b, *c,
    blRasterContextStrokeGeometrySinkFunc, sink);

  // We must clear the input path, because glyph outlines are appended to it and we just just consumed its content.
  // If we haven't cleared it we would process the same data that we have already processed the next time.
  blPathClear(path);

  return localResult;
}

} // {BLRasterEngine}
