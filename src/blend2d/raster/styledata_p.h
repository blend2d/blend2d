// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_STYLEDATA_P_H_INCLUDED
#define BLEND2D_RASTER_STYLEDATA_P_H_INCLUDED

#include "../raster/rasterdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace BLRasterEngine {

union StyleSource {
  //! Solid data.
  BLPipeline::FetchData::Solid solid;
  //! Fetch data.
  RenderFetchData* fetchData;

  //! Reset all data to zero.
  BL_INLINE void reset() noexcept { solid.prgb64 = 0; }
};

//! Style data holds a copy of user-provided style with additional members that
//! allow to create a `RenderFetchData` from it. When a style is assigned to
//! the rendering context it has to calculate the style transformation matrix
//! and a few other things that could degrade the style into a solid fill.
struct StyleData {
  union {
    uint32_t packed;
    struct {
      uint8_t cmdFlags;
      //! Style type.
      uint8_t styleType;
      //! Style pixel format.
      uint8_t styleFormat;
      //! Gradient/Pattern filter.
      uint8_t quality : 4;
      //! Adjusted matrix type.
      uint8_t adjustedMatrixType : 4;
    };
  };

  //! Alpha value (0..255 or 0..65535).
  uint32_t alphaI;
  //! Source data - either solid data or pointer to `RenderFetchData`.
  StyleSource source;

  union {
    //! Solid color as non-premultiplied RGBA (float components).
    BLRgba rgba;
    //! Solid color as non-premultiplied RGBA32 (integer components).
    BLRgba32 rgba32;
    //! Solid color as non-premultiplied RGBA64 (integer components).
    BLRgba64 rgba64;
    //! Image area in case this data wraps a `BLPattern` object.
    BLRectI imageArea;
    //! Structure used to tag which data is used, whether `rgba` or `rgba32`.
    struct {
      uint32_t u32[3];
      uint32_t tag;
    } tagging;
  };

  //! Adjusted matrix.
  BLMatrix2D adjustedMatrix;

  static BL_INLINE uint32_t solidRgba32Tag() noexcept { return blBitCast<uint32_t>(blNaN<float>()) + 0u; }
  static BL_INLINE uint32_t solidRgba64Tag() noexcept { return blBitCast<uint32_t>(blNaN<float>()) + 1u; }

  BL_INLINE bool isRgba32() const noexcept { return tagging.tag == solidRgba32Tag(); }
  BL_INLINE bool isRgba64() const noexcept { return tagging.tag == solidRgba64Tag(); }

  BL_INLINE void assignRgba(const BLRgba& value) noexcept {
    rgba = value;
  }

  BL_INLINE void assignRgba32(uint32_t value) noexcept {
    rgba32.value = value;
    tagging.tag = solidRgba32Tag();
  }

  BL_INLINE void assignRgba64(uint64_t value) noexcept {
    rgba64.value = value;
    tagging.tag = solidRgba64Tag();
  }

  BL_INLINE bool hasFetchData() const noexcept {
    return (cmdFlags & BL_RASTER_COMMAND_FLAG_FETCH_DATA) != 0;
  }
};

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_STYLEDATA_P_H_INCLUDED
