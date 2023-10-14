// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../raster/renderfetchdata_p.h"

namespace bl {
namespace RasterEngine {

// bl::RasterEngine - Fetch Data Utilities
// =======================================

BLResult computePendingFetchData(RenderFetchData* fetchData) noexcept {
  // At the moment only gradients have support for pending fetch data calculation.
  BL_ASSERT(fetchData->signature.isGradient());

  BLGradientPrivateImpl* gradientI = GradientInternal::getImpl(&fetchData->styleAs<BLGradientCore>());
  uint32_t lutSize = fetchData->pipelineData.gradient.lut.size;
  BLGradientQuality quality = BLGradientQuality(fetchData->extra.custom[0]);

  BLGradientLUT* lut;
  if (quality < BL_GRADIENT_QUALITY_DITHER)
    lut = GradientInternal::ensureLut32(gradientI, lutSize);
  else
    lut = GradientInternal::ensureLut64(gradientI, lutSize);

  if (BL_UNLIKELY(!lut))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  fetchData->signature.clearPendingBit();
  fetchData->pipelineData.gradient.lut.data = lut->data();

  return BL_SUCCESS;
}

} // {RasterEngine}
} // {bl}
