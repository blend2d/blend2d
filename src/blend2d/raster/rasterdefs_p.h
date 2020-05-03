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

#ifndef BLEND2D_RASTER_RASTERDEFS_P_H_INCLUDED
#define BLEND2D_RASTER_RASTERDEFS_P_H_INCLUDED

#include "../compop_p.h"
#include "../context_p.h"
#include "../gradient_p.h"
#include "../matrix_p.h"
#include "../pattern_p.h"
#include "../path_p.h"
#include "../pipedefs_p.h"
#include "../variant_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

class BLRasterContextImpl;
struct BLRasterFetchData;

class BLRasterWorkData;

// ============================================================================
// [Constants]
// ============================================================================

//! Raster command flags.
enum BLRasterCommandFlags : uint32_t {
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

enum BLRasterContextFormatPrecision {
  BL_RASTER_CONTEXT_FORMAT_PRECISION_8BPC = 0,
  BL_RASTER_CONTEXT_FORMAT_PRECISION_16BPC = 1,
  BL_RASTER_CONTEXT_FORMAT_PRECISION_FLOAT = 2
};

// ============================================================================
// [BLRasterContextPrecisionInfo]
// ============================================================================

//! Describes precision used for pixel blending and fixed point calculations of
//! a target pixel format.
struct BLRasterContextPrecisionInfo {
  //! Format type, see \ref BLRasterContextFormatType.
  //!
  //! \note Don't confuse this with `BLFormat`!
  uint8_t precision;
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


//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERDEFS_P_H_INCLUDED
