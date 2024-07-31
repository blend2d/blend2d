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

namespace bl {
namespace RasterEngine {
namespace JobProc {

// bl::RasterEngine - Job Processor - State Accessor
// =================================================

namespace {

class JobStateAccessor {
public:
  const RenderJob_BaseOp* job;

  BL_INLINE_NODEBUG explicit JobStateAccessor(const RenderJob_BaseOp* job) noexcept
    : job(job) {}

  BL_INLINE_NODEBUG const SharedFillState* fillState() const noexcept {
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
  BL_INLINE_NODEBUG BLTransformType finalTransformFixedType() const noexcept { return job->finalTransformFixedType(); }

  BL_INLINE BLMatrix2D finalTransformFixed(const BLPoint& originFixed) const noexcept {
    const Matrix2x2& t = fillState()->finalTransformFixed;
    return BLMatrix2D(t.m[0], t.m[1], t.m[2], t.m[3], originFixed.x, originFixed.y);
  }

  BL_INLINE_NODEBUG const BLBox& finalClipBoxFixedD() const noexcept { return fillState()->finalClipBoxFixedD; }

  // Stroke states.
  BL_INLINE_NODEBUG const BLApproximationOptions& approximationOptions() const noexcept { return baseStrokeState()->approximationOptions; }
  BL_INLINE_NODEBUG const BLStrokeOptions& strokeOptions() const noexcept { return baseStrokeState()->strokeOptions; }

  BL_INLINE_NODEBUG BLTransformType metaTransformFixedType() const noexcept { return job->metaTransformFixedType(); }

  BL_INLINE BLMatrix2D metaTransformFixed(const BLPoint& originFixed) const noexcept {
    const Matrix2x2& t = extStrokeState()->metaTransformFixed;
    return BLMatrix2D(t.m[0], t.m[1], t.m[2], t.m[3], originFixed.x, originFixed.y);
  }

  BL_INLINE BLMatrix2D userTransform() const noexcept {
    const Matrix2x2& t = extStrokeState()->userTransform;
    return BLMatrix2D(t.m[0], t.m[1], t.m[2], t.m[3], 0.0, 0.0);
  }
};

} // {anonymous}

// bl::RasterEngine - Job Processor - Utilities
// ============================================

static BL_INLINE void prepareEdgeBuilder(WorkData* workData, const SharedFillState* fillState) noexcept {
  workData->saveState();
  workData->edgeBuilder.setClipBox(fillState->finalClipBoxFixedD);
  workData->edgeBuilder.setFlattenToleranceSq(Math::square(fillState->toleranceFixedD));
}

static BL_INLINE BLPath* getGeometryAsPath(WorkData* workData, RenderJob_GeometryOp* job) noexcept {
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

static BL_INLINE void finalizeGeometryData(WorkData* workData, RenderJob_GeometryOp* job) noexcept {
  blUnused(workData);
  if (job->geometryType() == BL_GEOMETRY_TYPE_PATH)
    job->geometryData<BLPath>()->~BLPath();
}

template<typename Job>
static BL_INLINE void assignEdges(WorkData* workData, Job* job, EdgeStorage<int>* edgeStorage) noexcept {
  if (!edgeStorage->empty()) {
    RenderCommandQueue* commandQueue = job->commandQueue();
    size_t commandIndex = job->commandIndex();
    uint8_t qy0 = uint8_t((edgeStorage->boundingBox().y0) >> workData->commandQuantizationShiftFp());

    commandQueue->initQuantizedY0(commandIndex, qy0);
    commandQueue->at(commandIndex).setAnalyticEdges(edgeStorage);
    edgeStorage->resetBoundingBox();
  }
}

// bl::RasterEngine - Job Processor - Fill Geometry Job
// ====================================================

static void processFillGeometryJob(WorkData* workData, RenderJob_GeometryOp* job) noexcept {
  BLPath* path = getGeometryAsPath(workData, job);
  if (BL_UNLIKELY(!path))
    return;

  JobStateAccessor accessor(job);
  prepareEdgeBuilder(workData, accessor.fillState());
  BLResult result = addFilledPathEdges(workData, path->view(), accessor.finalTransformFixed(job->originFixed()), accessor.finalTransformFixedType());

  if (result == BL_SUCCESS) {
    assignEdges(workData, job, &workData->edgeStorage);
  }

  finalizeGeometryData(workData, job);
}

// bl::RasterEngine - Job Processor - Fill Text Job
// ================================================

static void processFillTextJob(WorkData* workData, RenderJob_TextOp* job) noexcept {
  BLResult result = BL_SUCCESS;
  uint32_t dataType = job->textDataType();

  const BLFont& font = job->_font.dcast();
  const BLPoint& originFixed = job->originFixed();
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
    JobStateAccessor accessor(job);
    prepareEdgeBuilder(workData, accessor.fillState());

    result = addFilledGlyphRunEdges(workData, accessor, originFixed, &font, glyphRun);
    if (result == BL_SUCCESS) {
      assignEdges(workData, job, &workData->edgeStorage);
    }
  }

  job->destroy();
}

// bl::RasterEngine - Job Processor - Stroke Geometry Job
// ======================================================

static void processStrokeGeometryJob(WorkData* workData, RenderJob_GeometryOp* job) noexcept {
  BLPath* path = getGeometryAsPath(workData, job);
  if (BL_UNLIKELY(!path))
    return;

  JobStateAccessor accessor(job);
  prepareEdgeBuilder(workData, accessor.fillState());

  if (addStrokedPathEdges(workData, accessor, job->originFixed(), path) == BL_SUCCESS) {
    assignEdges(workData, job, &workData->edgeStorage);
  }

  finalizeGeometryData(workData, job);
}

// bl::RasterEngine - Job Processor - Stroke Text Job
// ==================================================

static void processStrokeTextJob(WorkData* workData, RenderJob_TextOp* job) noexcept {
  BLResult result = BL_SUCCESS;
  uint32_t dataType = job->textDataType();

  const BLFont& font = job->_font.dcast();
  const BLPoint& originFixed = job->originFixed();
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
    JobStateAccessor accessor(job);
    prepareEdgeBuilder(workData, accessor.fillState());

    result = addStrokedGlyphRunEdges(workData, accessor, originFixed, &font, glyphRun);
    if (result == BL_SUCCESS) {
      assignEdges(workData, job, &workData->edgeStorage);
    }
  }

  job->destroy();
}

// bl::RasterEngine - Job Processor - Dispatch
// ===========================================

static void processJob(WorkData* workData, RenderJob* job) noexcept {
  if (job->hasJobFlag(RenderJobFlags::kComputePendingFetchData)) {
    RenderCommand& command = job->command();
    computePendingFetchData(command._source.fetchData);
  }

  switch (job->jobType()) {
    case RenderJobType::kFillGeometry:
      processFillGeometryJob(workData, static_cast<RenderJob_GeometryOp*>(job));
      break;

    case RenderJobType::kFillText:
      processFillTextJob(workData, static_cast<RenderJob_TextOp*>(job));
      break;

    case RenderJobType::kStrokeGeometry:
      processStrokeGeometryJob(workData, static_cast<RenderJob_GeometryOp*>(job));
      break;

    case RenderJobType::kStrokeText:
      processStrokeTextJob(workData, static_cast<RenderJob_TextOp*>(job));
      break;

    default:
      BL_NOT_REACHED();
  }
}

} // {JobProc}
} // {RasterEngine}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RENDERJOBPROC_P_H_INCLUDED
