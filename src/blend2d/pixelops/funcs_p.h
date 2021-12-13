// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIXELOPS_FUNCS_P_H_INCLUDED
#define BLEND2D_PIXELOPS_FUNCS_P_H_INCLUDED

#include "../api-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLPixelOps {

struct Funcs {
  void (BL_CDECL* interpolate_prgb32)(uint32_t* dst, uint32_t dstSize, const BLGradientStop* stops, size_t stopCount) BL_NOEXCEPT;
};

extern Funcs funcs;

} // {BLPixelOps}

//! \}
//! \endcond

#endif // BLEND2D_PIXELOPS_FUNCS_P_H_INCLUDED
