// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../gradient_p.h"
#include "../image_p.h"
#include "../raster/rastercontext_p.h"
#include "../raster/renderfetchdata_p.h"

namespace BLRasterEngine {

// RenderFetchData - Pattern
// =========================

static BL_INLINE bool blRasterFetchDataSetupPattern(RenderFetchData* fetchData, const StyleData* style) noexcept {
  BLImageImpl* imgI = BLImagePrivate::getImpl(&fetchData->image());
  const BLMatrix2D& m = style->adjustedMatrix;

  // Zero area means to cover the whole image.
  BLRectI area = style->imageArea;
  if (!BLGeometry::isValid(area))
    area.reset(0, 0, imgI->size.w, imgI->size.h);

  if (!fetchData->setupPatternAffine(fetchData->_extendMode, style->quality, m))
    return false;

  fetchData->_isSetup = true;
  return true;
}

void BL_CDECL blRasterFetchDataDestroyPattern(BLRasterContextImpl* ctxI, RenderFetchData* fetchData) noexcept {
  BLImagePrivate::releaseInstance(static_cast<BLImageCore*>(&fetchData->_style));
  ctxI->freeFetchData(fetchData);
}

// RenderFetchData - Gradient
// ==========================

static BL_INLINE bool blRasterFetchDataSetupGradient(RenderFetchData* fetchData, const StyleData* style) noexcept {
  BLGradientPrivateImpl* gradientI = BLGradientPrivate::getImpl(&fetchData->gradient());
  BLGradientLUT* lut = BLGradientPrivate::ensureLut32(gradientI);

  if (BL_UNLIKELY(!lut))
    return false;

  const BLMatrix2D& m = style->adjustedMatrix;
  BLPipeline::FetchType fetchType = fetchData->_data.initGradient(gradientI->gradientType, gradientI->values, gradientI->extendMode, lut, m);

  if (fetchType == BLPipeline::FetchType::kFailure)
    return false;

  fetchData->_isSetup = true;
  fetchData->_fetchType = fetchType;
  fetchData->_fetchFormat = uint8_t(style->styleFormat);
  return true;
}

void BL_CDECL blRasterFetchDataDestroyGradient(BLRasterContextImpl* ctxI, RenderFetchData* fetchData) noexcept {
  BLGradientPrivate::releaseInstance(static_cast<BLGradientCore*>(&fetchData->_style));
  ctxI->freeFetchData(fetchData);
}

// RenderFetchData - Setup
// =======================

bool blRasterFetchDataSetup(RenderFetchData* fetchData, const StyleData* style) noexcept {
  switch (fetchData->_style._d.rawType()) {
    case BL_OBJECT_TYPE_GRADIENT:
      return blRasterFetchDataSetupGradient(fetchData, style);

    case BL_OBJECT_TYPE_IMAGE:
      return blRasterFetchDataSetupPattern(fetchData, style);

    default:
      return false;
  }
}

} // {BLRasterEngine}
