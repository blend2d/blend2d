// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_STYLEDATA_P_H_INCLUDED
#define BLEND2D_RASTER_STYLEDATA_P_H_INCLUDED

#include "../raster/rasterdefs_p.h"
#include "../raster/renderfetchdata_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace bl {
namespace RasterEngine {

//! Style data holds a copy of user-provided style with additional members that allow to create a `RenderFetchData`
//! from it. When a style is assigned to the rendering context it has to calculate the style transformation matrix
//! and a few other things that could degrade the style into a solid fill.
struct StyleData {
  //! \name Members
  //! \{

  //! Pointer to a fetch data - it points to either a separate `RenderFetchData` data or to `&solid` data in this
  //! struct. Use `hasImplicitFetchData()` to check whether fetch data points to external fetch data or to &solid.
  RenderFetchDataHeader* fetchData;

  struct SolidData : public RenderFetchDataSolid {
    //! The original color passed to setStyle() API.
    union {
      //! Solid color as passed to frontend (non-premultiplied RGBA float components).
      BLRgba rgba;
      //! Solid color as passed to frontend (non-premultiplied RGBA32 integer components).
      BLRgba32 rgba32;
      //! Solid color as passed to frontend (non-premultiplied RGBA64 integer components).
      BLRgba64 rgba64;
    } original;
  };

  struct NonSolidData {
    //! Style transformation matrix combined with the rendering context transformation matrix.
    BLMatrix2D adjustedTransform;
  };

  union {
    SolidData solid;
    NonSolidData nonSolid;
  };

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG void makeFetchDataImplicit() noexcept { fetchData = &solid; }
  BL_INLINE_NODEBUG bool hasImplicitFetchData() const noexcept { return fetchData == static_cast<const void*>(&solid); }

  BL_INLINE_NODEBUG bool hasFetchData() const noexcept { return !hasImplicitFetchData(); }
  BL_INLINE_NODEBUG RenderFetchData* getRenderFetchData() const noexcept { return static_cast<RenderFetchData*>(fetchData); }

  //! \}

  //! \name Memory Operations
  //! \{

  BL_INLINE void swap(StyleData& other) noexcept {
    bool thisImplicit = hasImplicitFetchData();
    bool otherImplicit = other.hasImplicitFetchData();

    BLInternal::swap(*this, other);

    if (thisImplicit)
      other.makeFetchDataImplicit();

    if (otherImplicit)
      makeFetchDataImplicit();
  }

  BL_INLINE void copyFrom(const StyleData& other) noexcept {
    memcpy(this, &other, sizeof(StyleData));
    if (other.hasImplicitFetchData())
      makeFetchDataImplicit();
  }

  //! \}
};

} // {RasterEngine}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_STYLEDATA_P_H_INCLUDED
