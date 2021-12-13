// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RASTERDEFS_P_H_INCLUDED
#define BLEND2D_RASTER_RASTERDEFS_P_H_INCLUDED

#include "../compop_p.h"
#include "../context_p.h"
#include "../gradient_p.h"
#include "../image_p.h"
#include "../matrix_p.h"
#include "../object_p.h"
#include "../pattern_p.h"
#include "../path_p.h"
#include "../pipeline/pipedefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

class BLRasterContextImpl;

namespace BLRasterEngine {

struct RenderFetchData;
struct StyleData;
class WorkData;

} // {BLRasterEngine}

//! Raster command flags.
enum RenderCommandFlags : uint32_t {
  BL_RASTER_COMMAND_FLAG_FETCH_DATA = 0x01u
};

//! Indexes to a `BLRasterContextImpl::solidFormatTable`, which describes pixel
//! formats used by solid fills. There are in total 3 choices that are selected
//! based on properties of the solid color.
enum BLRasterContextSolidFormatId : uint32_t {
  BL_RASTER_CONTEXT_SOLID_FORMAT_ARGB = 0,
  BL_RASTER_CONTEXT_SOLID_FORMAT_FRGB = 1,
  BL_RASTER_CONTEXT_SOLID_FORMAT_ZERO = 2,

  BL_RASTER_CONTEXT_SOLID_FORMAT_COUNT = 3
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERDEFS_P_H_INCLUDED
