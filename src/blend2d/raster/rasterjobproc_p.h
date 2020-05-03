// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_RASTER_RASTERJOBPROC_P_H_INCLUDED
#define BLEND2D_RASTER_RASTERJOBPROC_P_H_INCLUDED

#include "../path_p.h"
#include "../pathstroke_p.h"
#include "../region_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rastercommand_p.h"
#include "../raster/rastercontextops_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/rasterjob_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [blRasterJobProcAsync - State Accessor]
// ============================================================================

class BLRasterContextJobStateAccessor {
public:
  const BLRasterJobData_BaseOp* job;

  explicit BL_INLINE BLRasterContextJobStateAccessor(const BLRasterJobData_BaseOp* job) noexcept
    : job(job) {}

  BL_INLINE const BLRasterSharedFillState* fillState() const noexcept {
    return job->fillState();
  }

  BL_INLINE const BLRasterSharedBaseStrokeState* baseStrokeState() const noexcept {
    BL_ASSERT(job->strokeState());
    return job->strokeState();
  }

  BL_INLINE const BLRasterSharedExtendedStrokeState* extStrokeState() const noexcept {
    BL_ASSERT(strokeOptions().transformOrder != BL_STROKE_TRANSFORM_ORDER_AFTER);
    return static_cast<const BLRasterSharedExtendedStrokeState*>(job->strokeState());
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

// ============================================================================
// [blRasterJobProcAsync - Common Utilities]
// ============================================================================

static BL_INLINE void blRasterJobPrepareEdgeBuilder(BLRasterWorkData* workData, const BLRasterSharedFillState* fillState) noexcept {
  workData->saveState();
  workData->edgeBuilder.setClipBox(fillState->finalClipBoxFixedD);
  workData->edgeBuilder.setFlattenToleranceSq(blSquare(fillState->toleranceFixedD));
}

// ============================================================================
// [blRasterJobProcAsync - Fill/Stroke Utilities]
// ============================================================================

static BL_INLINE BLPath* blRasterJobGetGeometryAsPath(BLRasterWorkData* workData, BLRasterJobData_GeometryOp* job) noexcept {
  BLPath* path = nullptr;
  uint32_t geometryType = job->geometryType();

  if (geometryType == BL_GEOMETRY_TYPE_PATH)
    return job->geometryData<BLPath>();

  path = &workData->tmpPath[3];
  path->clear();
  BLResult result = path->addGeometry(geometryType, job->geometryData<void>());

  // BLRegion is the only container except BLPath that requires a cleanup.
  if (geometryType == BL_GEOMETRY_TYPE_REGION)
    job->geometryData<BLRegion>()->~BLRegion();

  if (result != BL_SUCCESS) {
    workData->accumulateError(result);
    return nullptr;
  }

  return path;
}

static BL_INLINE void blRasterJobFinalizeGeometryData(BLRasterWorkData* workData, BLRasterJobData_GeometryOp* job) noexcept {
  BL_UNUSED(workData);
  if (job->geometryType() == BL_GEOMETRY_TYPE_PATH)
    job->geometryData<BLPath>()->~BLPath();
}

// ============================================================================
// [blRasterJobProcAsync - Fill Geometry]
// ============================================================================

static void blRasterJobProcAsync_FillGeometry(BLRasterWorkData* workData, BLRasterJobData_GeometryOp* job) noexcept {
  BLPath* path = blRasterJobGetGeometryAsPath(workData, job);
  if (BL_UNLIKELY(!path))
    return;

  BLRasterContextJobStateAccessor accessor(job);
  blRasterJobPrepareEdgeBuilder(workData, accessor.fillState());
  BLResult result = blRasterContextBuildPathEdges(workData, path->view(), accessor.finalMatrixFixed(), accessor.finalMatrixFixedType());

  if (result == BL_SUCCESS) {
    BLEdgeStorage<int>* edgeStorage = &workData->edgeStorage;
    BLRasterCommand* commandData = job->commandData();

    if (!edgeStorage->empty()) {
      commandData->setEdgesAsync(edgeStorage);
      edgeStorage->resetBoundingBox();
    }
  }

  blRasterJobFinalizeGeometryData(workData, job);
}

// ============================================================================
// [blRasterJobProcAsync - Fill Text]
// ============================================================================

static void blRasterJobProcAsync_FillText(BLRasterWorkData* workData, BLRasterJobData_TextOp* job) noexcept {
  BLResult result = BL_SUCCESS;
  uint32_t dataType = job->textDataType();

  const BLPoint* pt = &job->_pt;
  const BLFont* font = blDownCast(&job->_font);

  const BLGlyphRun* glyphRun = nullptr;

  if (dataType != BL_RASTER_JOB_TEXT_DATA_TYPE_GLYPH_RUN) {
    BLGlyphBuffer* glyphBuffer;

    if (dataType != BL_RASTER_JOB_TEXT_DATA_TYPE_GLYPH_BUFFER) {
      glyphBuffer = &workData->glyphBuffer;
      glyphBuffer->setText(job->textData(), job->textSize(), dataType);
    }
    else {
      glyphBuffer = blDownCast(&job->_glyphBuffer);
    }

    result = font->shape(*glyphBuffer);
    glyphRun = &glyphBuffer->glyphRun();
  }
  else {
    glyphRun = &job->_glyphRun;
  }

  if (result == BL_SUCCESS) {
    BLRasterContextJobStateAccessor accessor(job);
    blRasterJobPrepareEdgeBuilder(workData, accessor.fillState());
    result = blRasterContextUtilFillGlyphRun(workData, accessor, pt, font, glyphRun);
  }

  if (result == BL_SUCCESS) {
    BLEdgeStorage<int>* edgeStorage = &workData->edgeStorage;
    BLRasterCommand* commandData = job->commandData();

    if (!edgeStorage->empty()) {
      commandData->setEdgesAsync(edgeStorage);
      edgeStorage->resetBoundingBox();
    }
  }

  job->destroy();
}

// ============================================================================
// [blRasterJobProcAsync - Stroke Geometry]
// ============================================================================

static void blRasterJobProcAsync_StrokeGeometry(BLRasterWorkData* workData, BLRasterJobData_GeometryOp* job) noexcept {
  BLPath* path = blRasterJobGetGeometryAsPath(workData, job);
  if (BL_UNLIKELY(!path))
    return;

  BLRasterContextJobStateAccessor accessor(job);
  blRasterJobPrepareEdgeBuilder(workData, accessor.fillState());

  if (blRasterContextUtilStrokeUnsafePath(workData, accessor, path) == BL_SUCCESS) {
    BLEdgeStorage<int>* edgeStorage = &workData->edgeStorage;
    BLRasterCommand* commandData = job->commandData();

    if (!edgeStorage->empty()) {
      commandData->setEdgesAsync(edgeStorage);
      edgeStorage->resetBoundingBox();
    }
  }

  blRasterJobFinalizeGeometryData(workData, job);
}

// ============================================================================
// [blRasterJobProcAsync - Stroke Text]
// ============================================================================

static void blRasterJobProcAsync_StrokeText(BLRasterWorkData* workData, BLRasterJobData_TextOp* job) noexcept {
  BLResult result = BL_SUCCESS;
  uint32_t dataType = job->textDataType();

  const BLPoint* pt = &job->_pt;
  const BLFont* font = blDownCast(&job->_font);

  const BLGlyphRun* glyphRun = nullptr;

  if (dataType != BL_RASTER_JOB_TEXT_DATA_TYPE_GLYPH_RUN) {
    BLGlyphBuffer* glyphBuffer;

    if (dataType != BL_RASTER_JOB_TEXT_DATA_TYPE_GLYPH_BUFFER) {
      glyphBuffer = &workData->glyphBuffer;
      glyphBuffer->setText(job->textData(), job->textSize(), dataType);
    }
    else {
      glyphBuffer = blDownCast(&job->_glyphBuffer);
    }

    result = font->shape(*glyphBuffer);
    glyphRun = &glyphBuffer->glyphRun();
  }
  else {
    glyphRun = &job->_glyphRun;
  }

  if (result == BL_SUCCESS) {
    BLRasterContextJobStateAccessor accessor(job);
    blRasterJobPrepareEdgeBuilder(workData, accessor.fillState());
    result = blRasterContextUtilStrokeGlyphRun(workData, accessor, pt, font, glyphRun);
  }

  if (result == BL_SUCCESS) {
    BLEdgeStorage<int>* edgeStorage = &workData->edgeStorage;
    BLRasterCommand* commandData = job->commandData();

    if (!edgeStorage->empty()) {
      commandData->setEdgesAsync(edgeStorage);
      edgeStorage->resetBoundingBox();
    }
  }

  job->destroy();
}
// ============================================================================
// [blRasterJobProcAsync]
// ============================================================================

static BL_NOINLINE void blRasterJobProcAsync(BLRasterWorkData* workData, BLRasterJobData* job) noexcept {
  switch (job->jobType()) {
    case BL_RASTER_JOB_TYPE_FILL_GEOMETRY:
      blRasterJobProcAsync_FillGeometry(workData, static_cast<BLRasterJobData_GeometryOp*>(job));
      break;

    case BL_RASTER_JOB_TYPE_FILL_TEXT:
      blRasterJobProcAsync_FillText(workData, static_cast<BLRasterJobData_TextOp*>(job));
      break;

    case BL_RASTER_JOB_TYPE_STROKE_GEOMETRY:
      blRasterJobProcAsync_StrokeGeometry(workData, static_cast<BLRasterJobData_GeometryOp*>(job));
      break;

    case BL_RASTER_JOB_TYPE_STROKE_TEXT:
      blRasterJobProcAsync_StrokeText(workData, static_cast<BLRasterJobData_TextOp*>(job));
      break;

    default:
      BL_NOT_REACHED();
  }
}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERJOBPROC_P_H_INCLUDED
