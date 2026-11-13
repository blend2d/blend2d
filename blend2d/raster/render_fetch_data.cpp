// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#include <blend2d/raster/render_fetch_data_p.h>

namespace bl::RasterEngine {

// RasterEngine - Fetch Data Utilities
// ===================================

BLResult compute_pending_fetch_data(RenderFetchData* fetch_data) noexcept {
  // At the moment only gradients have support for pending fetch data calculation.
  BL_ASSERT(fetch_data->signature.is_gradient());

  const BLGradientCore& gradient = fetch_data->style_as<BLGradientCore>();
  GradientPrivateImpl* gradient_impl = ObjectLayer::get_impl(&gradient);

  uint32_t lut_size = fetch_data->pipeline_data.gradient.lut.size;
  BLGradientQuality quality = BLGradientQuality(fetch_data->extra.custom[0]);

  GradientLUT* lut;
  if (quality < BL_GRADIENT_QUALITY_DITHER) {
    lut = GradientDetail::ensure_lut32(&gradient, gradient_impl, lut_size);
  }
  else {
    lut = GradientDetail::ensure_lut64(&gradient, gradient_impl, lut_size);
  }

  if (BL_UNLIKELY(!lut)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  fetch_data->signature.clear_pending_bit();
  fetch_data->pipeline_data.gradient.lut.data = lut->data();

  return BL_SUCCESS;
}

} // {bl::RasterEngine}
