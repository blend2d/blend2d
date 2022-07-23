// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RENDERJOBPROC_P_H_INCLUDED
#define BLEND2D_RASTER_RENDERJOBPROC_P_H_INCLUDED

#include "../path_p.h"
#include "../pathstroke_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rendercommand_p.h"
#include "../raster/rastercontextops_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/renderjob_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace BLRasterEngine {

class RenderJobStateAccessor {
public:
  const RenderJob_BaseOp* job;

  explicit BL_INLINE RenderJobStateAccessor(const RenderJob_BaseOp* job) noexcept
    : job(job) {}

  BL_INLINE const SharedFillState* fillState() const noexcept {
    return job->fillState();
  }

  BL_INLINE const SharedBaseStrokeState* baseStrokeState() const noexcept {
    BL_ASSERT(job->strokeState());
    return job->strokeState();
  }

  BL_INLINE const SharedExtendedStrokeState* extStrokeState() const noexcept {
    BL_ASSERT(strokeOptions().transformOrder != BL_STROKE_TRANSFORM_ORDER_AFTER);
    return static_cast<const SharedExtendedStrokeState*>(job->strokeState());
  }

  // Fill states.
  BL_INLINE uint32_t finalMatrixFixedType() const noexcept { return job->finalMatrixFixedType(); }
  BL_INLINE const BLMatrix2D& finalMatrixFixed() const noexcept { return fillState()->finalMatrixFixed; }
  BL_INLINE const BLBox& finalClipBoxFixedD() const noexcept { return fillState()->finalClipBoxFixedD; }

  // Stroke states.
  BL_INLINE const BLApproximationOptions& approximationOptions() const noexcept { return baseStrokeState()->approximationOptions; }
  BL_INLINE const BLStrokeOptions& strokeOptions() const noexcept { return baseStrokeState()->strokeOptions; }

  BL_INLINE uint32_t metaMatrixFixedType() const noexcept { return job->metaMatrixFixedType(); }
  BL_INLINE const BLMatrix2D& metaMatrixFixed() const noexcept { return extStrokeState()->metaMatrixFixed; }
  BL_INLINE const BLMatrix2D& userMatrix() const noexcept { return extStrokeState()->userMatrix; }
};

static BL_INLINE void blRasterJobPrepareEdgeBuilder(WorkData* workData, const SharedFillState* fillState) noexcept {
  workData->saveState();
  workData->edgeBuilder.setClipBox(fillState->finalClipBoxFixedD);
  workData->edgeBuilder.setFlattenToleranceSq(blSquare(fillState->toleranceFixedD));
}

static BL_INLINE BLPath* blRasterJobGetGeometryAsPath(WorkData* workData, RenderJob_GeometryOp* job) noexcept {
  BLPath* path = nullptr;
  BLGeometryType geometryType = job->geometryType();

  if (geometryType == BL_GEOMETRY_TYPE_PATH)
    return job->geometryData<BLPath>();

  path = &workData->tmpPath[3];
  path->clear();
  BLResult result = path->addGeometry(geometryType, job->geometryData<void>());

  if (result != BL_SUCCESS) {
    workData->accumulateError(result);
    return nullptr;
  }

  return path;
}

static BL_INLINE void blRasterJobFinalizeGeometryData(WorkData* workData, RenderJob_GeometryOp* job) noexcept {
  blUnused(workData);
  if (job->geometryType() == BL_GEOMETRY_TYPE_PATH)
    job->geometryData<BLPath>()->~BLPath();
}

static void blRasterJobProcAsync_FillGeometry(WorkData* workData, RenderJob_GeometryOp* job) noexcept {
  BLPath* path = blRasterJobGetGeometryAsPath(workData, job);
  if (BL_UNLIKELY(!path))
    return;

  RenderJobStateAccessor accessor(job);
  blRasterJobPrepareEdgeBuilder(workData, accessor.fillState());
  BLResult result = blRasterContextBuildPathEdges(workData, path->view(), accessor.finalMatrixFixed(), accessor.finalMatrixFixedType());

  if (result == BL_SUCCESS) {
    EdgeStorage<int>* edgeStorage = &workData->edgeStorage;
    RenderCommand* commandData = job->command();

    if (!edgeStorage->empty()) {
      commandData->setEdgesAsync(edgeStorage);
      edgeStorage->resetBoundingBox();
    }
  }

  blRasterJobFinalizeGeometryData(workData, job);
}

static void blRasterJobProcAsync_FillText(WorkData* workData, RenderJob_TextOp* job) noexcept {
  BLResult result = BL_SUCCESS;
  uint32_t dataType = job->textDataType();

  const BLPoint& pt = job->_pt;
  const BLFont& font = job->_font.dcast();

  const BLGlyphRun* glyphRun = nullptr;

  if (dataType != RenderJob::kTextDataGlyphRun) {
    BLGlyphBuffer* glyphBuffer;

    if (dataType != RenderJob::kTextDataGlyphBuffer) {
      glyphBuffer = &workData->glyphBuffer;
      glyphBuffer->setText(job->textData(), job->textSize(), (BLTextEncoding)dataType);
    }
    else {
      glyphBuffer = &job->_glyphBuffer.dcast();
    }

    result = font.shape(*glyphBuffer);
    glyphRun = &glyphBuffer->glyphRun();
  }
  else {
    glyphRun = &job->_glyphRun;
  }

  if (result == BL_SUCCESS) {
    RenderJobStateAccessor accessor(job);
    blRasterJobPrepareEdgeBuilder(workData, accessor.fillState());
    result = blRasterContextUtilFillGlyphRun(workData, accessor, &pt, &font, glyphRun);
  }

  if (result == BL_SUCCESS) {
    EdgeStorage<int>* edgeStorage = &workData->edgeStorage;
    RenderCommand* commandData = job->command();

    if (!edgeStorage->empty()) {
      commandData->setEdgesAsync(edgeStorage);
      edgeStorage->resetBoundingBox();
    }
  }

  job->destroy();
}

static void blRasterJobProcAsync_StrokeGeometry(WorkData* workData, RenderJob_GeometryOp* job) noexcept {
  BLPath* path = blRasterJobGetGeometryAsPath(workData, job);
  if (BL_UNLIKELY(!path))
    return;

  RenderJobStateAccessor accessor(job);
  blRasterJobPrepareEdgeBuilder(workData, accessor.fillState());

  if (blRasterContextUtilStrokeUnsafePath(workData, accessor, path) == BL_SUCCESS) {
    EdgeStorage<int>* edgeStorage = &workData->edgeStorage;
    RenderCommand* commandData = job->command();

    if (!edgeStorage->empty()) {
      commandData->setEdgesAsync(edgeStorage);
      edgeStorage->resetBoundingBox();
    }
  }

  blRasterJobFinalizeGeometryData(workData, job);
}

static void blRasterJobProcAsync_StrokeText(WorkData* workData, RenderJob_TextOp* job) noexcept {
  BLResult result = BL_SUCCESS;
  uint32_t dataType = job->textDataType();

  const BLPoint& pt = job->_pt;
  const BLFont& font = job->_font.dcast();

  const BLGlyphRun* glyphRun = nullptr;

  if (dataType != RenderJob::kTextDataGlyphRun) {
    BLGlyphBuffer* glyphBuffer;

    if (dataType != RenderJob::kTextDataGlyphBuffer) {
      glyphBuffer = &workData->glyphBuffer;
      glyphBuffer->setText(job->textData(), job->textSize(), (BLTextEncoding)dataType);
    }
    else {
      glyphBuffer = &job->_glyphBuffer.dcast();
    }

    result = font.shape(*glyphBuffer);
    glyphRun = &glyphBuffer->glyphRun();
  }
  else {
    glyphRun = &job->_glyphRun;
  }

  if (result == BL_SUCCESS) {
    RenderJobStateAccessor accessor(job);
    blRasterJobPrepareEdgeBuilder(workData, accessor.fillState());
    result = blRasterContextUtilStrokeGlyphRun(workData, accessor, &pt, &font, glyphRun);
  }

  if (result == BL_SUCCESS) {
    EdgeStorage<int>* edgeStorage = &workData->edgeStorage;
    RenderCommand* commandData = job->command();

    if (!edgeStorage->empty()) {
      commandData->setEdgesAsync(edgeStorage);
      edgeStorage->resetBoundingBox();
    }
  }

  job->destroy();
}

static BL_NOINLINE void blRasterJobProcAsync(WorkData* workData, RenderJob* job) noexcept {
  switch (job->jobType()) {
    case RenderJob::kTypeFillGeometry:
      blRasterJobProcAsync_FillGeometry(workData, static_cast<RenderJob_GeometryOp*>(job));
      break;

    case RenderJob::kTypeFillText:
      blRasterJobProcAsync_FillText(workData, static_cast<RenderJob_TextOp*>(job));
      break;

    case RenderJob::kTypeStrokeGeometry:
      blRasterJobProcAsync_StrokeGeometry(workData, static_cast<RenderJob_GeometryOp*>(job));
      break;

    case RenderJob::kTypeStrokeText:
      blRasterJobProcAsync_StrokeText(workData, static_cast<RenderJob_TextOp*>(job));
      break;

    default:
      BL_NOT_REACHED();
  }
}

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RENDERJOBPROC_P_H_INCLUDED
