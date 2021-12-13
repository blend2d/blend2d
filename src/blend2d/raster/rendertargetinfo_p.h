// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RENDERTARGETINFO_P_H_INCLUDED
#define BLEND2D_RASTER_RENDERTARGETINFO_P_H_INCLUDED

#include "../raster/rasterdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace BLRasterEngine {

//! Rendering target information
//! Describes precision used for pixel blending and fixed point calculations of
//! a target pixel format.
struct RenderTargetInfo {
  //! Type of a pixel component.
  enum PixelComponentType : uint8_t {
    kPixelComponentUInt8 = 0,
    kPixelComponentUInt16 = 1,
    kPixelComponentFloat32 = 2,
    kPixelComponentCount = 3
  };

  //! Pixel component type, see \ref PixelComponentType.
  uint8_t pixelComponentType;
  //! Reserved for future use.
  uint8_t reserved;
  //! Full alpha value (255 or 65535).
  uint16_t fullAlphaI;
  //! Fixed point shift (able to multiply / divide by fpScale).
  int fpShiftI;
  //! Fixed point scale as int (either 256 or 65536).
  int fpScaleI;
  //! Fixed point mask calculated as `fpScaleI - 1`.
  int fpMaskI;

  //! Full alpha (255 or 65535) stored as `double`.
  double fullAlphaD;
  //! Fixed point scale as double (either 256.0 or 65536.0).
  double fpScaleD;
};

extern const RenderTargetInfo renderTargetInfoByComponentType[RenderTargetInfo::kPixelComponentCount];

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RENDERTARGETINFO_P_H_INCLUDED
