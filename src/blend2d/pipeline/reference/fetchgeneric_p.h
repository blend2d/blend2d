// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_REFERENCE_FETCHGENERIC_P_H_INCLUDED
#define BLEND2D_PIPELINE_REFERENCE_FETCHGENERIC_P_H_INCLUDED

#include "../../pipeline/pipedefs_p.h"
#include "../../pipeline/reference/pixelgeneric_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_reference
//! \{

namespace BLPipeline {
namespace Reference {

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

template<typename PixelT>
class FetchSolid {
public:
  typedef PixelT PixelType;
  enum : uint32_t { kIsSolid = 1 };

  PixelType src;

  BL_INLINE FetchSolid(const void* fetchData) noexcept {
    src = PixelType::fromValue(static_cast<const FetchData::Solid*>(fetchData)->prgb32);
  }

  BL_INLINE void initRectY(uint32_t x, uint32_t y, uint32_t width) noexcept {}
  BL_INLINE void beginRectX(uint32_t x) noexcept {}

  BL_INLINE void initSpanY(uint32_t y) noexcept {}
  BL_INLINE void beginSpanX(uint32_t x) noexcept {}
  BL_INLINE void advanceSpanX(uint32_t x, uint32_t diff) noexcept {}
  BL_INLINE void endSpanX(uint32_t x) noexcept {}

  BL_INLINE void advanceY() noexcept {}

  BL_INLINE PixelType fetch() const noexcept { return src; }
};

BL_DIAGNOSTIC_POP

template<typename PixelT, bool IsPad>
class FetchLinearGradient {
public:
  typedef PixelT PixelType;
  enum : uint32_t { kIsSolid = 0 };

  const void* _table;
  uint64_t _pt;
  uint64_t _dt;
  uint64_t _py;
  uint64_t _dy;
  uint64_t _rep;
  uint32_t _msk;

  BL_INLINE FetchLinearGradient(const void* fetchData) noexcept {
    const FetchData::Gradient* gradient = static_cast<const FetchData::Gradient*>(fetchData);

    _table = gradient->lut.data;
    _pt = 0;
    _py = gradient->linear.pt[0].u64;
    _dt = gradient->linear.dt.u64;
    _dy = gradient->linear.dy.u64;
    _rep = (uint64_t(gradient->linear.rep.u) << 32) | 0xFFFFFFFFu;
    _msk = gradient->linear.msk.u & 0x0000FFFFu;
  }

  BL_INLINE void initRectY(uint32_t x, uint32_t y, uint32_t width) noexcept {
    blUnused(width);
    _py += y * _dy + x * _dt;
  }

  BL_INLINE void beginRectX(uint32_t x) noexcept {
    blUnused(x);
    _pt = _py;
  }

  BL_INLINE void initSpanY(uint32_t y) noexcept {
    _py += y * _dy;
  }

  BL_INLINE void beginSpanX(uint32_t x) noexcept {
    _pt = _py + x * _dt;
  }

  BL_INLINE void advanceSpanX(uint32_t x, uint32_t diff) noexcept {
    blUnused(x);
    _pt += diff * _dt;
  }

  BL_INLINE void endSpanX(uint32_t x) noexcept {
    blUnused(x);
  }

  BL_INLINE void advanceY() noexcept {
    _py += _dy;
  }

  BL_INLINE PixelType fetch() noexcept {
    uint32_t idx;
    if (IsPad) {
      int32_t pt = int32_t(_pt >> 32);
      idx = uint32_t(blClamp<int32_t>(pt, 0, int32_t(_msk)));
    }
    else {
      _pt &= _rep;
      uint32_t pt = uint32_t(_pt >> 32);
      idx = blMin<uint32_t>(pt ^ _msk, pt);
    }

    _pt += _dt;
    return PixelType::fromValue(static_cast<const uint32_t*>(_table)[idx]);
  }
};

} // {Reference}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_REFERENCE_FETCHGENERIC_P_H_INCLUDED
