// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "../api-build_p.h"
#include "../gradient_p.h"
#include "../image_p.h"
#include "../raster/rastercontext_p.h"
#include "../raster/rasterfetchdata_p.h"

// ============================================================================
// [BLRasterFetchData - Pattern]
// ============================================================================

static BL_INLINE bool blRasterFetchDataSetupPattern(BLRasterFetchData* fetchData, const BLRasterContextStyleData* style) noexcept {
  BLImageImpl* imgI = fetchData->_image->impl;
  const BLMatrix2D& m = style->adjustedMatrix;

  // Zero area means to cover the whole image.
  BLRectI area = style->imageArea;
  if (!blIsValid(area))
    area.reset(0, 0, imgI->size.w, imgI->size.h);

  if (!fetchData->setupPatternAffine(fetchData->_extendMode, style->quality, m))
    return false;

  fetchData->_isSetup = true;
  return true;
}

void BL_CDECL blRasterFetchDataDestroyPattern(BLRasterContextImpl* ctxI, BLRasterFetchData* fetchData) noexcept {
  BLImageImpl* imgI = fetchData->_image->impl;
  ctxI->freeFetchData(fetchData);

  if (blImplDecRefAndTest(imgI))
    blImageImplDelete(imgI);
}

// ============================================================================
// [BLRasterFetchData - Gradient]
// ============================================================================

static BL_INLINE bool blRasterFetchDataSetupGradient(BLRasterFetchData* fetchData, const BLRasterContextStyleData* style) noexcept {
  BLGradientImpl* gradientI = fetchData->_gradient->impl;
  BLGradientLUT* lut = blGradientImplEnsureLut32(gradientI);

  if (BL_UNLIKELY(!lut))
    return false;

  const BLMatrix2D& m = style->adjustedMatrix;
  uint32_t fetchType = fetchData->_data.initGradient(gradientI->gradientType, gradientI->values, gradientI->extendMode, lut, m);

  if (fetchType == BL_PIPE_FETCH_TYPE_FAILURE)
    return false;

  fetchData->_isSetup = true;
  fetchData->_fetchType = uint8_t(fetchType);
  fetchData->_fetchFormat = uint8_t(style->styleFormat);
  return true;
}

void BL_CDECL blRasterFetchDataDestroyGradient(BLRasterContextImpl* ctxI, BLRasterFetchData* fetchData) noexcept {
  BLGradientImpl* gradientI = fetchData->_gradient->impl;
  ctxI->freeFetchData(fetchData);

  if (blImplDecRefAndTest(gradientI))
    blGradientImplDelete(gradientI);
}

// ============================================================================
// [BLRasterFetchData - Setup]
// ============================================================================

bool blRasterFetchDataSetup(BLRasterFetchData* fetchData, const BLRasterContextStyleData* style) noexcept {
  switch (fetchData->_variant->implType()) {
    case BL_IMPL_TYPE_GRADIENT:
      return blRasterFetchDataSetupGradient(fetchData, style);

    case BL_IMPL_TYPE_IMAGE:
      return blRasterFetchDataSetupPattern(fetchData, style);

    default:
      return false;
  }
}
