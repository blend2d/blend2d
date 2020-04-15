// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_RASTERFETCHDATA_P_H
#define BLEND2D_RASTER_RASTERFETCHDATA_P_H

#include "../raster/rasterdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

class BLRasterContextImpl;
struct BLRasterFetchData;

// ============================================================================
// [BLRasterFetchData]
// ============================================================================

BL_HIDDEN bool blRasterFetchDataSetup(BLRasterFetchData* fetchData, const BLRasterContextStyleData* style) noexcept;
BL_HIDDEN void BL_CDECL blRasterFetchDataDestroyPattern(BLRasterContextImpl* ctxI, BLRasterFetchData* fetchData) noexcept;
BL_HIDDEN void BL_CDECL blRasterFetchDataDestroyGradient(BLRasterContextImpl* ctxI, BLRasterFetchData* fetchData) noexcept;

// ============================================================================
// [BLRasterFetchData]
// ============================================================================

//! Raster context fetch data.
//!
//! Contains pipeline fetch data and additional members that are required by
//! the rendering engine for proper pipeline construction and memory management.
struct alignas(16) BLRasterFetchData {
  //! \name Types
  //! \{

  typedef void (BL_CDECL* DestroyFunc)(BLRasterContextImpl* ctxI, BLRasterFetchData* fetchData) BL_NOEXCEPT;

  //! \}

  //! \name Members
  //! \{

  //! Fetch data part, which is used by pipelines.
  BLPipeFetchData _data;

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
      uint8_t _fetchType;
      //! Fetch (source) format.
      uint8_t _fetchFormat;
      //! Extend mode.
      uint8_t _extendMode;
    };
  };

  //! Link to the external data.
  union {
    //!< Source as variant.
    BLWrap<BLVariant> _variant;
    //!< Source image.
    BLWrap<BLImage> _image;
    //!< Source gradient.
    BLWrap<BLGradient> _gradient;
  };

  //! Releases this fetchData to the rendering context, can only be called
  //! if the reference count is decreased to zero. Don't use manually.
  DestroyFunc _destroyFunc;

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE uint8_t isSetup() const noexcept { return _isSetup; }
  BL_INLINE uint8_t fetchType() const noexcept { return _fetchType; }
  BL_INLINE uint8_t fetchFormat() const noexcept { return _fetchFormat; }

  //! \}

  //! \name Initialization
  //! \{

  BL_INLINE void initGradientSource(BLGradientImpl* gradientI) noexcept {
    _batchId = 0;
    _refCount = 1;
    _packed = 0;
    _gradient->impl = gradientI;
    _destroyFunc = blRasterFetchDataDestroyGradient;
  }

  BL_INLINE void initPatternSource(BLImageImpl* imageI, const BLRectI& area) noexcept {
    BL_ASSERT(area.x >= 0);
    BL_ASSERT(area.y >= 0);
    BL_ASSERT(area.w >= 0);
    BL_ASSERT(area.h >= 0);

    _batchId = 0;
    _refCount = 1;
    _packed = 0;
    _fetchFormat = uint8_t(imageI->format);
    _image->impl = imageI;
    _destroyFunc = blRasterFetchDataDestroyPattern;

    const uint8_t* srcPixelData = static_cast<const uint8_t*>(imageI->pixelData);
    intptr_t srcStride = imageI->stride;
    uint32_t srcBytesPerPixel = blFormatInfo[imageI->format].depth / 8u;
    _data.initPatternSource(srcPixelData + uint32_t(area.y) * srcStride + uint32_t(area.x) * srcBytesPerPixel, imageI->stride, area.w, area.h);
  }

  // Initializes `fetchData` for a blit. Blits are never repeating and are
  // always 1:1 (no scaling, only pixel translation is possible).
  BL_INLINE bool setupPatternBlit(int tx, int ty) noexcept {
    uint32_t fetchType = _data.initPatternBlit(tx, ty);
    _isSetup = true;
    _fetchType = uint8_t(fetchType);
    return true;
  }

  BL_INLINE bool setupPatternFxFy(uint32_t extendMode, uint32_t quality, int64_t txFixed, int64_t tyFixed) noexcept {
    uint32_t fetchType = _data.initPatternFxFy(extendMode, quality, _image->impl->depth / 8, txFixed, tyFixed);
    _isSetup = true;
    _fetchType = uint8_t(fetchType);
    return true;
  }

  BL_INLINE bool setupPatternAffine(uint32_t extendMode, uint32_t quality, const BLMatrix2D& m) noexcept {
    uint32_t fetchType = _data.initPatternAffine(extendMode, quality, _image->impl->depth / 8, m);
    _isSetup = fetchType != BL_PIPE_FETCH_TYPE_FAILURE;
    _fetchType = uint8_t(fetchType);
    return _isSetup;
  }

  //! \}

  //! \name Reference Counting
  //! \{


  BL_INLINE BLRasterFetchData* addRef() noexcept {
    _refCount++;
    return this;
  }

  BL_INLINE void release(BLRasterContextImpl* ctxI) noexcept {
    if (--_refCount == 0)
      _destroyFunc(ctxI, this);
  }

  //! \}
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERFETCHDATA_P_H
