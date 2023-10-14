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

namespace bl {
namespace RasterEngine {

// bl::RasterEngine - Edge Building Utilities
// ==========================================

template<typename PointType>
static BL_INLINE BLResult blRasterContextBuildPolyEdgesT(
  WorkData* workData,
  const PointType* pts, size_t size, const BLMatrix2D& transform, BLTransformType transformType) noexcept {

  BLResult result = workData->edgeBuilder.initFromPoly(pts, size, transform, transformType);
  if (BL_LIKELY(result == BL_SUCCESS))
    return result;

  workData->revertEdgeBuilder();
  return workData->accumulateError(result);
}

BLResult addFilledPolygonEdges(WorkData* workData, const BLPointI* pts, size_t size, const BLMatrix2D& transform, BLTransformType transformType) noexcept {
  return blRasterContextBuildPolyEdgesT(workData, pts, size, transform, transformType);
}

BLResult addFilledPolygonEdges(WorkData* workData, const BLPoint* pts, size_t size, const BLMatrix2D& transform, BLTransformType transformType) noexcept {
  return blRasterContextBuildPolyEdgesT(workData, pts, size, transform, transformType);
}

BLResult addFilledPathEdges(WorkData* workData, const BLPathView& pathView, const BLMatrix2D& transform, BLTransformType transformType) noexcept {
  BLResult result = workData->edgeBuilder.initFromPath(pathView, true, transform, transformType);
  if (BL_LIKELY(result == BL_SUCCESS))
    return result;

  workData->revertEdgeBuilder();
  return workData->accumulateError(result);
}

// bl::RasterEngine - Sinks & Sink Utilities
// =========================================

BLResult fillGlyphRunSink(BLPathCore* path, const void* info, void* userData) noexcept {
  blUnused(info);

  EdgeBuilderSink* sink = static_cast<EdgeBuilderSink*>(userData);
  EdgeBuilder<int>* edgeBuilder = sink->edgeBuilder;

  BL_PROPAGATE(edgeBuilder->addPath(path->dcast().view(), true, TransformInternal::identityTransform, BL_TRANSFORM_TYPE_IDENTITY));
  return path->dcast().clear();
}

BLResult strokeGeometrySink(BLPathCore* a, BLPathCore* b, BLPathCore* c, size_t figureStart, size_t figureEnd, void* userData) noexcept {
  blUnused(figureStart, figureEnd);

  StrokeSink* self = static_cast<StrokeSink*>(userData);
  EdgeBuilder<int>* edgeBuilder = self->edgeBuilder;

  BL_PROPAGATE(edgeBuilder->addPath(a->dcast().view(), false, *self->transform, self->transformType));
  BL_PROPAGATE(edgeBuilder->addReversePathFromStrokeSink(b->dcast().view(), *self->transform, self->transformType));

  if (!c->dcast().empty())
    BL_PROPAGATE(edgeBuilder->addPath(c->dcast().view(), false, *self->transform, self->transformType));

  return a->dcast().clear();
}

BLResult strokeGlyphRunSink(BLPathCore* path, const void* info, void* userData) noexcept {
  blUnused(info);

  StrokeGlyphRunSink* sink = static_cast<StrokeGlyphRunSink*>(userData);
  BLPath& a = sink->paths[0];
  BLPath& b = sink->paths[1];
  BLPath& c = sink->paths[2];

  a.clear();
  BLResult localResult = PathInternal::strokePath(
    path->dcast().view(),
    *sink->strokeOptions,
    *sink->approximationOptions,
    a, b, c,
    strokeGeometrySink, sink);

  // We must clear the input path, because glyph outlines are appended to it and we just just consumed its content.
  // If we haven't cleared it we would process the same data that we have already processed the next time.
  blPathClear(path);

  return localResult;
}

} // {RasterEngine}
} // {bl}
