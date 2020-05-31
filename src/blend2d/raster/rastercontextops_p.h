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

#ifndef BLEND2D_RASTER_RASTERCONTEXTOPS_P_H_INCLUDED
#define BLEND2D_RASTER_RASTERCONTEXTOPS_P_H_INCLUDED

#include "../path_p.h"
#include "../pathstroke_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/rasterworkdata_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// The purpose of this file is to share as much as possible between both sync
// async implementations.

// ============================================================================
// [BLRasterContext - Geometry Utilities]
// ============================================================================

// TODO: [Rendering Context] SIMD idea: shift left by 24 and check whether all bytes are zero.
static BL_INLINE bool blRasterIsBoxAligned24x8(const BLBoxI& box) noexcept {
  if (blRuntimeIs32Bit()) {
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

// ============================================================================
// [BLRasterContext - Edge Building Utilities]
// ============================================================================

BL_HIDDEN BLResult blRasterContextBuildPolyEdges(BLRasterWorkData* workData, const BLPointI* pts, size_t size, const BLMatrix2D& m, uint32_t mType) noexcept;
BL_HIDDEN BLResult blRasterContextBuildPolyEdges(BLRasterWorkData* workData, const BLPoint* pts, size_t size, const BLMatrix2D& m, uint32_t mType) noexcept;
BL_HIDDEN BLResult blRasterContextBuildPathEdges(BLRasterWorkData* workData, const BLPathView& pathView, const BLMatrix2D& m, uint32_t mType) noexcept;

// ============================================================================
// [BLRasterContext - Sinks & Sink Utilities]
// ============================================================================

//! Edge builder sink - acts as a base class for other sinks, but can also be
//! used as is, for example by `FillGlyphOutline()` implementation.
struct BLRasterContextEdgeBuilderSink {
  BLEdgeBuilder<int>* edgeBuilder;
};

//! Passes the stroked paths to `BLEdgeBuilder` and flips signs where necessary.
//! This is much better than using BLPath::addStrokedPath() as no reversal
//! of `b` path is necessary, instead we flip sign of such path directly in the
//! EdgeBuilder.
struct BLRasterContextStrokeSink : public BLRasterContextEdgeBuilderSink {
  const BLMatrix2D* matrix;
  uint32_t matrixType;
};

struct BLRasterContextStrokeGlyphRunSink : public BLRasterContextStrokeSink {
  BLPath* paths;
  const BLStrokeOptions* strokeOptions;
  const BLApproximationOptions* approximationOptions;
};

BL_HIDDEN BLResult BL_CDECL blRasterContextFillGlyphRunSinkFunc(BLPathCore* path, const void* info, void* closure_) noexcept;
BL_HIDDEN BLResult BL_CDECL blRasterContextStrokeGeometrySinkFunc(BLPath* a, BLPath* b, BLPath* c, void* closure_) noexcept;
BL_HIDDEN BLResult BL_CDECL blRasterContextStrokeGlyphRunSinkFunc(BLPathCore* path, const void* info, void* closure_) noexcept;

// ============================================================================
// [BLRasterContext - Fill GlyphRun Utilities]
// ============================================================================

template<typename StateAccessor>
static BL_INLINE BLResult blRasterContextUtilFillGlyphRun(
  BLRasterWorkData* workData,
  const StateAccessor& accessor,
  const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  BLMatrix2D m(accessor.finalMatrixFixed());
  m.translate(*pt);

  BLPath* path = &workData->tmpPath[3];
  path->clear();

  BLRasterContextEdgeBuilderSink sink;
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

// ============================================================================
// [BLRasterContext - Stroke Path Utilities]
// ============================================================================

template<typename StateAccessor>
static BL_INLINE BLResult blRasterContextUtilStrokeUnsafePath(
  BLRasterWorkData* workData,
  const StateAccessor& accessor,
  const BLPath* path) noexcept {

  BLRasterContextStrokeSink sink;
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

  BLResult result = blPathStrokeInternal(
    path->view(),
    accessor.strokeOptions(),
    accessor.approximationOptions(),
    a, b, c,
    blRasterContextStrokeGeometrySinkFunc, &sink);

  // EdgeBuilder::done() can only fail on out of memory condition.
  if (BL_LIKELY(result == BL_SUCCESS))
    result = workData->edgeBuilder.done();

  if (BL_LIKELY(result == BL_SUCCESS))
    return result;

  workData->revertEdgeBuilder();
  return workData->accumulateError(result);
}

// ============================================================================
// [BLRasterContext - Stroke GlyphRun Utilities]
// ============================================================================

template<typename StateAccessor>
static BL_INLINE BLResult blRasterContextUtilStrokeGlyphRun(
  BLRasterWorkData* workData,
  const StateAccessor& accessor,
  const BLPoint* pt, const BLFontCore* font, const BLGlyphRun* glyphRun) noexcept {

  BLRasterContextStrokeGlyphRunSink sink;
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

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERCONTEXTOPS_P_H_INCLUDED
