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

namespace bl {
namespace RasterEngine {

//! A small struct that precedes `Pipeline::FetchData` in `RenderFetchData` struct.
//!
//! When a pipeline signature is built, there is a lot of unknowns and in general two code paths to build SOLID
//! and NON-SOLID pipelines. However, it's just a detail and the only thing that NON-SOLID render call needs is
//! to make sure that FetchData has been properly setup. This is only problem when rendering with a default fill
//! or stroke style, because in order to make style assignment fast, some calculations are postponed up to the place
//! we would hit once we know that the style is really going to be used - in general, some properties are materialized
//! lazily.
//!
//! To make this materialization simpler, we have a little prefix before a real `Pipeline::FetchData` that contains
//! a signature (other members are here just to use the space as FetchData should be aligned to 16 bytes, so we need
//! a 16 byte prefix as well). When the signature has only a PendingFlag set, it means that the FetchData hasn't been
//! setup yet and it has to be setup before the pipeline signature can be obtained.
//!
//! \note In some cases, this header can be left uninitialized in a single-threaded rendering in case that the
//! FetchData is constructed in place and allocated statically. In general, if it doesn't survive the render call
//! (which happens in single-threaded rendering a lot) then these fields are not really needed.
struct RenderFetchDataHeader {
  //! \name Members
  //! \{

  //! Signature if the fetch data is initialized, otherwise a Signature with PendingFlag bit set (last MSB).
  Pipeline::Signature signature;
  //! Batch id.
  uint32_t batchId;
  //! Non-atomic reference count (never manipulated concurrently by multiple threads, usually the user thread only).
  uint32_t refCount;

  union {
    uint32_t packed;

    struct {
      //! Pixel format of the source (possibly resolved to FRGB/ZERO, etc).
      uint8_t format;
      //! Extra bits, which can be used by the rendering engine to store some essential information required to materialize
      //! the FetchData.
      uint8_t custom[3];
    };
  } extra;

  //! \}

  //! \name Initialization
  //! \{

  //! Initializes the fetch data header by resetting all header members and initializing the reference count to `rc`.
  BL_INLINE void initHeader(uint32_t rc, FormatExt format = FormatExt::kNone) noexcept {
    signature.reset();
    batchId = 0;
    refCount = rc;
    extra.packed = 0;
    extra.format = uint8_t(format);
  }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG bool isSolid() const noexcept { return signature.isSolid(); }

  BL_INLINE void retain(uint32_t n = 1) noexcept { refCount += n; }

  BL_INLINE_NODEBUG const void* getPipelineData() const noexcept {
    return reinterpret_cast<const uint8_t*>(this) + sizeof(RenderFetchDataHeader);
  }

  //! \}
};

BL_STATIC_ASSERT(sizeof(RenderFetchDataHeader) == 16);

//! FetchData that can only hold a solid color.
struct RenderFetchDataSolid : public RenderFetchDataHeader {
  Pipeline::FetchData::Solid pipelineData;
};

//! Raster context fetch data.
//!
//! Contains pipeline fetch data and additional members that are required by
//! the rendering engine for proper pipeline construction and memory management.
struct alignas(16) RenderFetchData : public RenderFetchDataHeader {
  //! \name Types
  //! \{

  typedef void (BL_CDECL* DestroyFunc)(BLRasterContextImpl* ctxI, RenderFetchData* fetchData) BL_NOEXCEPT;

  //! \}

  //! \name Members
  //! \{

  //! Fetch data part, which is used by pipelines.
  Pipeline::FetchData pipelineData;

  //! Link to the external object holding the style data (BLImage or BLGradient).
  BLObjectCore style;

  //! Releases this fetchData to the rendering context, can only be called if the reference count is decreased
  //! to zero. Don't use manually.
  DestroyFunc destroyFunc;

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG bool isPending() const noexcept { return signature.hasPendingFlag(); }
  BL_INLINE_NODEBUG Pipeline::FetchType fetchType() const noexcept { return signature.fetchType(); }

  template<typename T>
  BL_INLINE_NODEBUG const T& styleAs() const noexcept { return static_cast<const T&>(style); }

  BL_INLINE_NODEBUG const BLImage& image() const noexcept { return static_cast<const BLImage&>(style); }
  BL_INLINE_NODEBUG const BLPattern& pattern() const noexcept { return static_cast<const BLPattern&>(style); }
  BL_INLINE_NODEBUG const BLGradient& gradient() const noexcept { return static_cast<const BLGradient&>(style); }

  //! \}

  //! \name Initialization
  //! \{

  BL_INLINE void initStyleObject(const BLObjectCore* src) noexcept { style._d = src->_d; }
  BL_INLINE void initDestroyFunc(DestroyFunc fn) noexcept { destroyFunc = fn; }

  BL_INLINE void initStyleObjectAndDestroyFunc(const BLObjectCore* src, DestroyFunc fn) noexcept {
    initStyleObject(src);
    initDestroyFunc(fn);
  }

  BL_INLINE void initImageSource(const BLImageImpl* imageI, const BLRectI& area) noexcept {
    BL_ASSERT(area.x >= 0);
    BL_ASSERT(area.y >= 0);
    BL_ASSERT(area.w >= 0);
    BL_ASSERT(area.h >= 0);

    const uint8_t* srcPixelData = static_cast<const uint8_t*>(imageI->pixelData);
    intptr_t srcStride = imageI->stride;
    uint32_t srcBytesPerPixel = imageI->depth / 8u;
    Pipeline::FetchUtils::initImageSource(pipelineData.pattern, srcPixelData + uint32_t(area.y) * srcStride + uint32_t(area.x) * srcBytesPerPixel, imageI->stride, area.w, area.h);
  }

  // Initializes `fetchData` for a blit. Blits are never repeating and are always 1:1 (no scaling, no fractional translation).
  BL_INLINE bool setupPatternBlit(int tx, int ty) noexcept {
    signature = Pipeline::FetchUtils::initPatternBlit(pipelineData.pattern, tx, ty);
    return true;
  }

  BL_INLINE bool setupPatternFxFy(BLExtendMode extendMode, BLPatternQuality quality, uint32_t bytesPerPixel, int64_t txFixed, int64_t tyFixed) noexcept {
    signature = Pipeline::FetchUtils::initPatternFxFy(pipelineData.pattern, extendMode, quality, bytesPerPixel, txFixed, tyFixed);
    return true;
  }

  BL_INLINE bool setupPatternAffine(BLExtendMode extendMode, BLPatternQuality quality, uint32_t bytesPerPixel, const BLMatrix2D& transform) noexcept {
    signature = Pipeline::FetchUtils::initPatternAffine(pipelineData.pattern, extendMode, quality, bytesPerPixel, transform);
    return !signature.hasPendingFlag();
  }

  //! \}

  //! \name Reference Counting
  //! \{

  BL_INLINE void release(BLRasterContextImpl* ctxI) noexcept {
    if (--refCount == 0)
      destroyFunc(ctxI, this);
  }

  //! \}
};

BL_HIDDEN BLResult computePendingFetchData(RenderFetchData* fetchData) noexcept;

} // {RasterEngine}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RENDERFETCHDATA_P_H_INCLUDED
