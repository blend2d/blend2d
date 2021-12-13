// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RENDERFETCHDATA_P_H_INCLUDED
#define BLEND2D_RASTER_RENDERFETCHDATA_P_H_INCLUDED

#include "../raster/rasterdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace BLRasterEngine {

BL_HIDDEN bool blRasterFetchDataSetup(RenderFetchData* fetchData, const StyleData* style) noexcept;
BL_HIDDEN void BL_CDECL blRasterFetchDataDestroyPattern(BLRasterContextImpl* ctxI, RenderFetchData* fetchData) noexcept;
BL_HIDDEN void BL_CDECL blRasterFetchDataDestroyGradient(BLRasterContextImpl* ctxI, RenderFetchData* fetchData) noexcept;

//! Raster context fetch data.
//!
//! Contains pipeline fetch data and additional members that are required by
//! the rendering engine for proper pipeline construction and memory management.
struct alignas(16) RenderFetchData {
  //! \name Types
  //! \{

  typedef void (BL_CDECL* DestroyFunc)(BLRasterContextImpl* ctxI, RenderFetchData* fetchData) BL_NOEXCEPT;

  //! \}

  //! \name Members
  //! \{

  //! Fetch data part, which is used by pipelines.
  BLPipeline::FetchData _data;

  //! Reference count (not atomic, never manipulated by worker threads).
  size_t _refCount;

  //! Batch id.
  uint32_t _batchId;

  //! Basic fetch data properties.
  union {
    uint32_t _packed;
    struct {
      //! True if this fetchData has been properly setup (by `setup...()` funcs).
      uint8_t _isSetup;
      //! Fetch type.
      BLPipeline::FetchType _fetchType;
      //! Fetch (source) format.
      uint8_t _fetchFormat;
      //! Extend mode.
      uint8_t _extendMode;
    };
  };

  //! Link to the external object holding the style data (BLImage or BLGradient).
  BLObjectCore _style;

  //! Releases this fetchData to the rendering context, can only be called if the reference count is decreased
  //! to zero. Don't use manually.
  DestroyFunc _destroyFunc;

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE uint8_t isSetup() const noexcept { return _isSetup; }
  BL_INLINE BLPipeline::FetchType fetchType() const noexcept { return _fetchType; }
  BL_INLINE uint8_t fetchFormat() const noexcept { return _fetchFormat; }

  BL_INLINE const BLImage& image() const noexcept { return static_cast<const BLImage&>(_style); }
  BL_INLINE const BLGradient& gradient() const noexcept { return static_cast<const BLGradient&>(_style); }

  //! \}

  //! \name Initialization
  //! \{

  BL_INLINE void initGradientSource(const BLGradientCore* gradient) noexcept {
    _batchId = 0;
    _refCount = 1;
    _packed = 0;
    _style._d = gradient->_d;
    _destroyFunc = blRasterFetchDataDestroyGradient;
  }

  BL_INLINE void initPatternSource(const BLImageCore* image, const BLRectI& area) noexcept {
    BL_ASSERT(area.x >= 0);
    BL_ASSERT(area.y >= 0);
    BL_ASSERT(area.w >= 0);
    BL_ASSERT(area.h >= 0);

    const BLImageImpl* imageI = BLImagePrivate::getImpl(image);

    _batchId = 0;
    _refCount = 1;
    _packed = 0;
    _fetchFormat = uint8_t(imageI->format);
    _style._d = image->_d;
    _destroyFunc = blRasterFetchDataDestroyPattern;

    const uint8_t* srcPixelData = static_cast<const uint8_t*>(imageI->pixelData);
    intptr_t srcStride = imageI->stride;
    uint32_t srcBytesPerPixel = blFormatInfo[imageI->format].depth / 8u;
    _data.initPatternSource(srcPixelData + uint32_t(area.y) * srcStride + uint32_t(area.x) * srcBytesPerPixel, imageI->stride, area.w, area.h);
  }

  // Initializes `fetchData` for a blit. Blits are never repeating and are
  // always 1:1 (no scaling, only pixel translation is possible).
  BL_INLINE bool setupPatternBlit(int tx, int ty) noexcept {
    _fetchType = _data.initPatternBlit(tx, ty);
    _isSetup = true;
    return true;
  }

  BL_INLINE bool setupPatternFxFy(uint32_t extendMode, uint32_t quality, int64_t txFixed, int64_t tyFixed) noexcept {
    _fetchType = _data.initPatternFxFy(extendMode, quality, image().depth() / 8, txFixed, tyFixed);
    _isSetup = true;
    return true;
  }

  BL_INLINE bool setupPatternAffine(uint32_t extendMode, uint32_t quality, const BLMatrix2D& m) noexcept {
    _fetchType = _data.initPatternAffine(extendMode, quality, image().depth() / 8, m);
    _isSetup = _fetchType != BLPipeline::FetchType::kFailure;
    return _isSetup;
  }

  //! \}

  //! \name Reference Counting
  //! \{


  BL_INLINE RenderFetchData* addRef() noexcept {
    _refCount++;
    return this;
  }

  BL_INLINE void release(BLRasterContextImpl* ctxI) noexcept {
    if (--_refCount == 0)
      _destroyFunc(ctxI, this);
  }

  //! \}
};

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RENDERFETCHDATA_P_H_INCLUDED
