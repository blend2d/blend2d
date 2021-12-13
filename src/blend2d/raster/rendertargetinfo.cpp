
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

// RasterEngine - RenderTargetInfo - Table
// =======================================

const RenderTargetInfo renderTargetInfoByComponentType[RenderTargetInfo::kPixelComponentCount] = {
  #define ROW(PixelComponentType, FpBits, FullAlpha) { \
    RenderTargetInfo::PixelComponentType,              \
    0,                                                 \
                                                       \
    uint16_t(FullAlpha),                               \
    int(FpBits),                                       \
    int(1 << FpBits),                                  \
    int((1 << FpBits) - 1),                            \
                                                       \
    double(FullAlpha),                                 \
    double(1 << FpBits)                                \
  }

  ROW(kPixelComponentUInt8, 8, 255),
  ROW(kPixelComponentUInt16, 16, 65535),
  ROW(kPixelComponentFloat32, 16, 1.0),

  #undef ROW
};

} // {BLRasterEngine}
