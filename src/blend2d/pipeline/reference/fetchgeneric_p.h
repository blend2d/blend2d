// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_REFERENCE_FETCHGENERIC_P_H_INCLUDED
#define BLEND2D_PIPELINE_REFERENCE_FETCHGENERIC_P_H_INCLUDED

#include "../../pipeline/pipedefs_p.h"
#include "../../pipeline/reference/pixelgeneric_p.h"
#include "../../support/intops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_reference
//! \{

namespace BLPipeline {
namespace Reference {

static BL_INLINE_NODEBUG float msbMask(float a) noexcept { return blBitCast<float>(blBitCast<int32_t>(a) >> 31); }
static BL_INLINE_NODEBUG float bitNot(float a) noexcept { return blBitCast<float>(~blBitCast<uint32_t>(a)); }
static BL_INLINE_NODEBUG float bitAnd(float a, float b) noexcept { return blBitCast<float>(blBitCast<uint32_t>(a) & blBitCast<uint32_t>(b)); }
static BL_INLINE_NODEBUG float bitOr(float a, float b) noexcept { return blBitCast<float>(blBitCast<uint32_t>(a) | blBitCast<uint32_t>(b)); }
static BL_INLINE_NODEBUG float bitXor(float a, float b) noexcept { return blBitCast<float>(blBitCast<uint32_t>(a) ^ blBitCast<uint32_t>(b)); }

static BL_INLINE_NODEBUG double msbMask(double a) noexcept { return blBitCast<double>(blBitCast<int64_t>(a) >> 63); }
static BL_INLINE_NODEBUG double bitNot(double a) noexcept { return blBitCast<double>(~blBitCast<uint64_t>(a)); }
static BL_INLINE_NODEBUG double bitAnd(double a, double b) noexcept { return blBitCast<double>(blBitCast<uint64_t>(a) & blBitCast<uint64_t>(b)); }
static BL_INLINE_NODEBUG double bitOr(double a, double b) noexcept { return blBitCast<double>(blBitCast<uint64_t>(a) | blBitCast<uint64_t>(b)); }
static BL_INLINE_NODEBUG double bitXor(double a, double b) noexcept { return blBitCast<double>(blBitCast<uint64_t>(a) ^ blBitCast<uint64_t>(b)); }

template<typename T>
struct Vec2T {
  T x, y;

  BL_INLINE_NODEBUG Vec2T swap() const noexcept { return Vec2T{y, x}; }

  BL_INLINE_NODEBUG void reset(double x_, double y_) noexcept { x = x_; y = y_; }
  BL_INLINE_NODEBUG double hadd() const noexcept { return x + y; }
  BL_INLINE_NODEBUG double hmul() const noexcept { return x * y; }

  BL_INLINE_NODEBUG Vec2T& operator+=(T scalar) noexcept { *this = Vec2T{x + scalar, y + scalar}; return *this; }
  BL_INLINE_NODEBUG Vec2T& operator-=(T scalar) noexcept { *this = Vec2T{x - scalar, y - scalar}; return *this; }
  BL_INLINE_NODEBUG Vec2T& operator*=(T scalar) noexcept { *this = Vec2T{x * scalar, y * scalar}; return *this; }
  BL_INLINE_NODEBUG Vec2T& operator/=(T scalar) noexcept { *this = Vec2T{x / scalar, y / scalar}; return *this; }
  BL_INLINE_NODEBUG Vec2T& operator&=(T scalar) noexcept { *this = Vec2T{bitAnd(x, scalar), bitAnd(y, scalar)}; return *this; }
  BL_INLINE_NODEBUG Vec2T& operator|=(T scalar) noexcept { *this = Vec2T{bitOr(x, scalar), bitOr(y, scalar)}; return *this; }
  BL_INLINE_NODEBUG Vec2T& operator^=(T scalar) noexcept { *this = Vec2T{bitXor(x, scalar), bitXor(y, scalar)}; return *this; }

  BL_INLINE_NODEBUG Vec2T& operator+=(const Vec2T& other) noexcept { *this = Vec2T{x + other.x, y + other.y}; return *this; }
  BL_INLINE_NODEBUG Vec2T& operator-=(const Vec2T& other) noexcept { *this = Vec2T{x - other.x, y - other.y}; return *this; }
  BL_INLINE_NODEBUG Vec2T& operator*=(const Vec2T& other) noexcept { *this = Vec2T{x * other.x, y * other.y}; return *this; }
  BL_INLINE_NODEBUG Vec2T& operator/=(const Vec2T& other) noexcept { *this = Vec2T{x / other.x, y / other.y}; return *this; }
  BL_INLINE_NODEBUG Vec2T& operator&=(const Vec2T& other) noexcept { *this = Vec2T{bitAnd(x, other.x), bitAnd(y, other.y)}; return *this; }
  BL_INLINE_NODEBUG Vec2T& operator|=(const Vec2T& other) noexcept { *this = Vec2T{bitOr(x, other.x), bitOr(y, other.y)}; return *this; }
  BL_INLINE_NODEBUG Vec2T& operator^=(const Vec2T& other) noexcept { *this = Vec2T{bitXor(x, other.x), bitXor(y, other.y)}; return *this; }
};

template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator+(T a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a + b.x, a + b.y}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator-(T a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a - b.x, a - b.y}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator*(T a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a * b.x, a * b.y}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator/(T a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a / b.x, a / b.y}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator&(T a, const Vec2T<T>& b) noexcept { return Vec2T<T>{bitAnd(a, b.x), bitAnd(a, b.y)}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator|(T a, const Vec2T<T>& b) noexcept { return Vec2T<T>{bitOr(a, b.x), bitOr(a, b.y)}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator^(T a, const Vec2T<T>& b) noexcept { return Vec2T<T>{bitXor(a, b.x), bitXor(a, b.y)}; };

template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator+(const Vec2T<T>& a, T b) noexcept { return Vec2T<T>{a.x + b, a.y + b}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator-(const Vec2T<T>& a, T b) noexcept { return Vec2T<T>{a.x - b, a.y - b}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator*(const Vec2T<T>& a, T b) noexcept { return Vec2T<T>{a.x * b, a.y * b}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator/(const Vec2T<T>& a, T b) noexcept { return Vec2T<T>{a.x / b, a.y / b}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator&(const Vec2T<T>& a, T b) noexcept { return Vec2T<T>{bitAnd(a.x, b), bitAnd(a.y, b)}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator|(const Vec2T<T>& a, T b) noexcept { return Vec2T<T>{bitOr(a.x, b), bitOr(a.y, b)}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator^(const Vec2T<T>& a, T b) noexcept { return Vec2T<T>{bitXor(a.x, b), bitXor(a.y, b)}; };

template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator+(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a.x + b.x, a.y + b.y}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator-(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a.x - b.x, a.y - b.y}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator*(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a.x * b.x, a.y * b.y}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator/(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a.x / b.x, a.y / b.y}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator&(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{bitAnd(a.x, b.x), bitAnd(a.y, b.y)}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator|(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{bitOr(a.x, b.x), bitOr(a.y, b.y)}; };
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> operator^(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{bitXor(a.x, b.x), bitXor(a.y, b.y)}; };

template<typename T> static BL_INLINE_NODEBUG Vec2T<T> msbMask(Vec2T<T> a) noexcept { return Vec2T<T>{msbMask(a.x), msbMask(a.y)}; }
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> v_abs(const Vec2T<T>& v) noexcept { return Vec2T<T>{blAbs(v.x), blAbs(v.y)}; }
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> v_min(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{blMin(a.x, b.x), blMin(a.y, b.y)}; }
template<typename T> static BL_INLINE_NODEBUG Vec2T<T> v_max(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{blMax(a.x, b.x), blMax(a.y, b.y)}; }

typedef Vec2T<float> Vec2F;
typedef Vec2T<double> Vec2D;
typedef Vec2T<int32_t> Vec2I32;
typedef Vec2T<uint32_t> Vec2U32;
typedef Vec2T<int64_t> Vec2I64;
typedef Vec2T<uint64_t> Vec2U64;

// Fetch - Solid
// =============

template<typename PixelT>
struct FetchSolid {
  typedef PixelT PixelType;
  enum : uint32_t { kIsSolid = 1 };

  PixelType _src;

  BL_INLINE void _initFetch(const void* fetchData) noexcept {
    _src = PixelType::fromValue(static_cast<const FetchData::Solid*>(fetchData)->prgb32);
  }

  BL_INLINE void rectInitFetch(ContextData* ctxData, const void* fetchData, uint32_t xPos, uint32_t yPos, uint32_t rectWidth) noexcept {
    blUnused(ctxData, xPos, yPos, rectWidth);

    _initFetch(fetchData);
  }

  BL_INLINE void rectStartX(uint32_t xPos) noexcept {
    blUnused(xPos);
  }

  BL_INLINE void spanInitY(ContextData* ctxData, const void* fetchData, uint32_t yPos) noexcept {
    blUnused(ctxData, yPos);

    _initFetch(fetchData);
  }

  BL_INLINE void spanStartX(uint32_t xPos) noexcept {
    blUnused(xPos);
  }
  BL_INLINE void spanAdvanceX(uint32_t xPos, uint32_t xDiff) noexcept {
    blUnused(xPos, xDiff);
  }

  BL_INLINE void spanEndX(uint32_t xPos) noexcept {
    blUnused(xPos);
  }

  BL_INLINE void advanceY() noexcept {}

  BL_INLINE PixelType fetch() const noexcept {
    return _src;
  }
};

// Fetch - Non Solid
// =================

struct FetchNonSolid {
  enum : uint32_t { kIsSolid = 0 };
};

// Fetch - Pattern - Utilities
// ===========================

struct FetchPatternVertAAExtendCtxAny {
  const uint8_t* _pixelPtr;

  intptr_t _stride0;
  intptr_t _stride1;

  intptr_t _yStop0;
  intptr_t _yStop1;

  uintptr_t _yRewindOffset;
  intptr_t _pixelPtrRewindOffset;

  intptr_t _y;

  BL_INLINE void init(const FetchData::Pattern* pattern, uint32_t yPos) noexcept {
    _pixelPtr = static_cast<const uint8_t*>(pattern->src.pixelData);

    _stride0 = pattern->src.stride;
    _stride1 = _stride0;

    _yStop0 = intptr_t(pattern->src.size.h);
    _yStop1 = _yStop0;

    _yRewindOffset = pattern->simple.vExtendData.yRewindOffset;
    _pixelPtrRewindOffset = pattern->simple.vExtendData.pixelPtrRewindOffset;

    _y = intptr_t(yPos) + intptr_t(pattern->simple.ty);

    intptr_t ry = pattern->simple.ry;
    if (ry == 0) {
      // Vertical Extend - Pad
      // ---------------------

      intptr_t clampedY = blClamp<intptr_t>(_y, 0, _yStop0 - 1);
      _pixelPtr += clampedY * _stride0;

      if (_y != clampedY) {
        // The current Y is padded at the moment so we have to setup stride0 and yStop0. If we are
        // padded before the first scanline, then we may hit yStop0 at some point and then go non-padded
        // for a while, otherwise, if we are padded past the last scanline we would stay there forever.
        _stride0 = 0;
        _yStop0 = 0;
      }
      else {
        // The current Y is within bounds, so setup stride1 and yStop1 as we will go to the end and
        // then after the end is matched we will stay at the end (it would pad to the last scanline).
        _stride1 = 0;
        _yStop1 = 0;
      }
    }
    else {
      // Vertical Extend - Repeat or Reflect
      // -----------------------------------

      _y = BLIntOps::pmod(uint32_t(_y), uint32_t(ry));

      // If reflecting, we need few additional checks to reflect vertically. We are either reflecting now
      // (the first check) or we would be reflecting after the initial repeat (the second condition).
      if (_y >= _yStop0) {
        _pixelPtr += (_yStop0 - 1) * _stride0;
        _stride0 = -_stride0;
        _y -= _yStop0;
      }
      else if (_yStop0 != ry) {
        _stride1 = -_stride0;
      }

      _pixelPtr += _y * _stride0;
    }
  }

  BL_INLINE void advance1() noexcept {
    if (++_y == _yStop0) {
      std::swap(_yStop0, _yStop1);
      std::swap(_stride0, _stride1);

      _y -= _yRewindOffset;
      _pixelPtr -= _pixelPtrRewindOffset;
    }
    else {
      _pixelPtr += _stride0;
    }
  }

  BL_INLINE_NODEBUG const uint8_t* pixelPtr() const noexcept { return _pixelPtr; }
};

struct FetchPatternVertFyExtendCtxAny {
  const uint8_t* _pixelPtr0;
  FetchPatternVertAAExtendCtxAny _ctx;

  BL_INLINE void init(const FetchData::Pattern* pattern, uint32_t yPos) noexcept {
    _ctx.init(pattern, yPos);
    _pixelPtr0 = _ctx.pixelPtr();
    _ctx.advance1();
  }

  BL_INLINE void advance1() noexcept {
    _pixelPtr0 = _ctx.pixelPtr();
    _ctx.advance1();
  }

  BL_INLINE_NODEBUG const uint8_t* pixelPtr0() const noexcept { return _pixelPtr0; }
  BL_INLINE_NODEBUG const uint8_t* pixelPtr1() const noexcept { return _ctx.pixelPtr(); }
};

template<uint32_t kBPP>
struct FetchPatternHorzExtendCtxPad {
  intptr_t _x;
  intptr_t _tx;
  intptr_t _mx;

  BL_INLINE void _initPattern(const FetchData::Pattern* pattern) noexcept {
    _tx = intptr_t(pattern->simple.tx) * kBPP;
    _mx = intptr_t(pattern->src.size.w - 1) * kBPP;
  }

  BL_INLINE void rectInit(const FetchData::Pattern* pattern, uint32_t xPos, uint32_t rectWidth) noexcept {
    blUnused(rectWidth);
    _initPattern(pattern);

    _tx += intptr_t(xPos) * kBPP;
  }

  BL_INLINE void rectStart(uint32_t xPos) noexcept {
    blUnused(xPos);
    _x = _tx;
  }

  BL_INLINE void spanInit(const FetchData::Pattern* pattern) noexcept {
    _initPattern(pattern);
  }

  BL_INLINE void spanStart(uint32_t xPos) noexcept {
    _x = intptr_t(xPos) * kBPP + _tx;
  }

  BL_INLINE void spanAdvance(uint32_t xPos, uint32_t xDiff) noexcept {
    blUnused(xPos);
    _x += intptr_t(xDiff) * kBPP;
  }

  BL_INLINE void spanEnd(uint32_t xPos) noexcept {
    blUnused(xPos);
  }

  BL_INLINE size_t index() const noexcept {
    return size_t(blClamp<intptr_t>(_x, 0, _mx));
  }

  BL_INLINE void advance1() noexcept {
    _x += kBPP;
  }
};

template<uint32_t kBPP>
struct FetchPatternHorzExtendCtxRepeat {
  uintptr_t _x;
  uintptr_t _tx;
  uintptr_t _w;

  BL_INLINE void _initPattern(const FetchData::Pattern* pattern) noexcept {
    _w = uintptr_t(pattern->src.size.w) * kBPP;
    _tx = uintptr_t(pattern->simple.tx) * kBPP;
  }

  BL_INLINE void rectInit(const FetchData::Pattern* pattern, uint32_t xPos, uint32_t rectWidth) noexcept {
    blUnused(rectWidth);

    _initPattern(pattern);
    _tx = BLIntOps::pmod(uintptr_t(xPos) * kBPP + _tx, _w);
  }

  BL_INLINE void rectStart(uint32_t xPos) noexcept {
    blUnused(xPos);
    _x = _tx;
  }

  BL_INLINE void spanInit(const FetchData::Pattern* pattern) noexcept {
    _initPattern(pattern);
  }

  BL_INLINE void spanStart(uint32_t xPos) noexcept {
    _x = BLIntOps::pmod(uintptr_t(xPos) * kBPP + _tx, _w);
  }

  BL_INLINE void spanAdvance(uint32_t xPos, uint32_t xDiff) noexcept {
    blUnused(xPos);

    _x += xDiff * kBPP;
    if (_x >= _w)
      _x = BLIntOps::pmod(_x, _w);
  }

  BL_INLINE void spanEnd(uint32_t xPos) noexcept {
    blUnused(xPos);
  }

  BL_INLINE size_t index() const noexcept {
    return _x;
  }

  BL_INLINE void advance1() noexcept {
    _x += kBPP;
    if (_x >= _w)
      _x = 0;
  }
};

template<uint32_t kBPP>
struct FetchPatternHorzExtendCtxRoR {
  intptr_t _x;
  intptr_t _tx;
  uintptr_t _rx;
  uintptr_t _w;

  BL_INLINE void _initPattern(const FetchData::Pattern* pattern) noexcept {
    _w = uintptr_t(pattern->src.size.w);
    _rx = uintptr_t(pattern->simple.rx);
    _tx = intptr_t(pattern->simple.tx);
  }

  BL_INLINE void rectInit(const FetchData::Pattern* pattern, uint32_t xPos, uint32_t rectWidth) noexcept {
    blUnused(rectWidth);

    _initPattern(pattern);
    _tx = intptr_t(BLIntOps::pmod(uintptr_t(xPos) + _tx, _rx));
    if (uintptr_t(_tx) >= _w)
      _tx -= intptr_t(_rx);
  }

  BL_INLINE void rectStart(uint32_t xPos) noexcept {
    blUnused(xPos);
    _x = _tx;
  }

  BL_INLINE void spanInit(const FetchData::Pattern* pattern) noexcept {
    _initPattern(pattern);
  }

  BL_INLINE void spanStart(uint32_t xPos) noexcept {
    _x = BLIntOps::pmod(intptr_t(xPos) + _tx, intptr_t(_rx));
    if (_x >= intptr_t(_w))
      _x -= _rx;
  }

  BL_INLINE void spanAdvance(uint32_t xPos, uint32_t xDiff) noexcept {
    blUnused(xPos);

    _x += xDiff;
    if (uintptr_t(_x) >= _w) {
      _x = BLIntOps::pmod(uintptr_t(_x), _rx);
      if (_x >= intptr_t(_w)) {
        _x -= _rx;
      }
    }
  }

  BL_INLINE void spanEnd(uint32_t xPos) noexcept {
    blUnused(xPos);
  }

  BL_INLINE size_t index() const noexcept {
    return blMin<uintptr_t>(_x, _x ^ BLIntOps::allOnes<uintptr_t>()) * kBPP;
  }

  BL_INLINE void advance1() noexcept {
    if (++_x >= intptr_t(_w))
      _x -= _rx;
  }
};

struct FetchPatternAffineCtx {
  Vec2U64 xx_xy;
  Vec2U64 yx_yy;
  Vec2U64 tx_ty;
  Vec2U64 px_py;
  Vec2I32 ox_oy;
  Vec2I32 rx_ry;
  Vec2I32 minx_miny;
  Vec2I32 maxx_maxy;
  Vec2I32 corx_cory;
  Vec2I32 tw_th;
  // Vec2D tw_th;

  BL_INLINE void _initPattern(const FetchData::Pattern* pattern) noexcept {
    xx_xy = Vec2U64{pattern->affine.xx.u64, pattern->affine.xy.u64};
    yx_yy = Vec2U64{pattern->affine.yx.u64, pattern->affine.yy.u64};
    tx_ty = Vec2U64{pattern->affine.tx.u64, pattern->affine.ty.u64};
    ox_oy = Vec2I32{int32_t(pattern->affine.ox.u64 >> 32), int32_t(pattern->affine.oy.u64 >> 32)};
    rx_ry = Vec2I32{int32_t(pattern->affine.rx.u64 >> 32), int32_t(pattern->affine.ry.u64 >> 32)};
    minx_miny = Vec2I32{pattern->affine.minX, pattern->affine.minY};
    maxx_maxy = Vec2I32{pattern->affine.maxX, pattern->affine.maxY};
    corx_cory = Vec2I32{pattern->affine.corX, pattern->affine.corY};

    tw_th = Vec2I32{int(pattern->affine.tw), int(pattern->affine.th)};
  }

  BL_INLINE void normalizePxPy(Vec2U64& v) noexcept {
    uint32_t x = uint32_t(int32_t(v.x >> 32) % tw_th.x);
    uint32_t y = uint32_t(int32_t(v.y >> 32) % tw_th.y);

    if (int32_t(x) < 0) x += rx_ry.x;
    if (int32_t(y) < 0) y += rx_ry.y;

    if (int32_t(x) > ox_oy.x) x -= rx_ry.x;
    if (int32_t(y) > ox_oy.y) y -= rx_ry.y;

    v = Vec2U64{(uint64_t(x) << 32) | (v.x & 0xFFFFFFFF), (uint64_t(y) << 32) | (v.y & 0xFFFFFFFF)};
  }

  BL_INLINE void rectInitY(ContextData* ctxData, const FetchData::Pattern* pattern, uint32_t xPos, uint32_t yPos, uint32_t rectWidth) noexcept {
    blUnused(ctxData, rectWidth);

    _initPattern(pattern);
    tx_ty += yx_yy * uint64_t(yPos) + xx_xy * uint64_t(xPos);
  }

  BL_INLINE void rectStartX(uint32_t xPos) noexcept {
    blUnused(xPos);

    px_py = tx_ty;
    normalizePxPy(px_py);
  }

  BL_INLINE void spanInitY(ContextData* ctxData, const FetchData::Pattern* pattern, uint32_t yPos) noexcept {
    blUnused(ctxData);

    _initPattern(pattern);
    tx_ty += yx_yy * uint64_t(yPos);
  }

  BL_INLINE void spanStartX(uint32_t xPos) noexcept {
    px_py = tx_ty + xx_xy * uint64_t(xPos);
    normalizePxPy(px_py);
  }

  BL_INLINE void spanAdvanceX(uint32_t xPos, uint32_t xDiff) noexcept {
    blUnused(xDiff);

    px_py += xx_xy * uint64_t(xPos);
    normalizePxPy(px_py);
  }

  BL_INLINE void spanEndX(uint32_t xPos) noexcept {
    blUnused(xPos);
  }

  BL_INLINE void advanceY() noexcept {
    tx_ty += yx_yy;
  }

  BL_INLINE uint32_t fracX() const noexcept { return uint32_t(px_py.x & 0xFFFFFFFFu) >> 24; }
  BL_INLINE uint32_t fracY() const noexcept { return uint32_t(px_py.y & 0xFFFFFFFFu) >> 24; }

  BL_INLINE Vec2T<size_t> index(int32_t offX = 0, int32_t offY = 0) const noexcept {
    int32_t x = int32_t(px_py.x >> 32) + offX;
    int32_t y = int32_t(px_py.y >> 32) + offY;

    // Step A - Handle a possible underflow (PAD).
    x = blMax(x, minx_miny.x);
    y = blMax(y, minx_miny.y);

    // Step B - Handle a possible overflow (PAD | Bilinear overflow).
    if (x > maxx_maxy.x) x = corx_cory.x;
    if (y > maxx_maxy.y) y = corx_cory.y;

    // Step C - Handle a possible reflection (RoR).
    x = x ^ (x >> 31);
    y = y ^ (y >> 31);

    return Vec2T<size_t>{uint32_t(x), uint32_t(y)};
  }

  BL_INLINE void advanceX() noexcept {
    px_py += xx_xy;

    int32_t x = int32_t(px_py.x >> 32);
    int32_t y = int32_t(px_py.y >> 32);

    x -= (BLIntOps::bitMaskFromBool<int32_t>(x > ox_oy.x) & rx_ry.x);
    y -= (BLIntOps::bitMaskFromBool<int32_t>(y > ox_oy.y) & rx_ry.y);

    px_py = Vec2U64{(uint64_t(uint32_t(x)) << 32) | (px_py.x & 0xFFFFFFFFu),
                    (uint64_t(uint32_t(y)) << 32) | (px_py.y & 0xFFFFFFFFu)};
  }
};

// Fetch - Pattern - Aligned
// =========================

template<typename DstPixelT, BLInternalFormat kFormat>
struct FetchPatternAlignedBlit : public FetchNonSolid {
  typedef DstPixelT PixelType;

  static constexpr uint32_t kSrcBPP = FormatMetadata<kFormat>::kBPP;

  const uint8_t* pixelPtr;
  intptr_t stride;

  BL_INLINE void _initFetch(const FetchData::Pattern* pattern) noexcept {
    pixelPtr = static_cast<const uint8_t*>(pattern->src.pixelData);
    stride = pattern->src.stride;
  }

  BL_INLINE void rectInitFetch(ContextData* ctxData, const void* fetchData, uint32_t xPos, uint32_t yPos, uint32_t rectWidth) noexcept {
    blUnused(ctxData);

    const FetchData::Pattern* pattern = static_cast<const FetchData::Pattern*>(fetchData);
    _initFetch(pattern);

    uint32_t tx = uint32_t(pattern->simple.tx);
    uint32_t ty = uint32_t(pattern->simple.ty);

    stride -= intptr_t(uintptr_t(rectWidth) * kSrcBPP);
    pixelPtr += intptr_t(yPos - ty) * stride + intptr_t(xPos - tx) * kSrcBPP;
  }

  BL_INLINE void rectStartX(uint32_t xPos) noexcept {
    blUnused(xPos);
  }

  BL_INLINE void spanInitY(ContextData* ctxData, const void* fetchData, uint32_t yPos) noexcept {
    blUnused(ctxData);

    const FetchData::Pattern* pattern = static_cast<const FetchData::Pattern*>(fetchData);
    _initFetch(pattern);

    uint32_t tx = uint32_t(pattern->simple.tx);
    uint32_t ty = uint32_t(pattern->simple.ty);
    pixelPtr += intptr_t(uintptr_t(yPos - ty)) * stride - uintptr_t(tx) * kSrcBPP;
  }

  BL_INLINE void spanStartX(uint32_t xPos) noexcept {
    pixelPtr += uintptr_t(xPos) * kSrcBPP;
  }

  BL_INLINE void spanAdvanceX(uint32_t xPos, uint32_t xDiff) noexcept {
    blUnused(xPos);

    pixelPtr += uintptr_t(xDiff) * kSrcBPP;
  }

  BL_INLINE void spanEndX(uint32_t xPos) noexcept {
    pixelPtr -= uintptr_t(xPos) * kSrcBPP;
  }

  BL_INLINE void advanceY() noexcept {
    pixelPtr += stride;
  }

  BL_INLINE PixelType fetch() noexcept {
    PixelType pixel = PixelIO<PixelType, kFormat>::fetch(pixelPtr);
    pixelPtr += kSrcBPP;
    return pixel;
  }
};

template<typename DstPixelT, BLInternalFormat kFormat, typename CtxX>
struct FetchPatternAlignedAny : public FetchNonSolid {
  typedef DstPixelT PixelType;

  FetchPatternVertAAExtendCtxAny _ctxY;
  CtxX _ctxX;

  BL_INLINE void rectInitFetch(ContextData* ctxData, const void* fetchData, uint32_t xPos, uint32_t yPos, uint32_t rectWidth) noexcept {
    blUnused(ctxData);

    const FetchData::Pattern* pattern = static_cast<const FetchData::Pattern*>(fetchData);
    _ctxY.init(pattern, yPos);
    _ctxX.rectInit(pattern, xPos, rectWidth);
  }

  BL_INLINE void rectStartX(uint32_t xPos) noexcept {
    _ctxX.rectStart(xPos);
  }

  BL_INLINE void spanInitY(ContextData* ctxData, const void* fetchData, uint32_t yPos) noexcept {
    blUnused(ctxData);

    const FetchData::Pattern* pattern = static_cast<const FetchData::Pattern*>(fetchData);
    _ctxY.init(pattern, yPos);
    _ctxX.spanInit(pattern);
  }

  BL_INLINE void spanStartX(uint32_t xPos) noexcept {
    _ctxX.spanStart(xPos);
  }

  BL_INLINE void spanAdvanceX(uint32_t xPos, uint32_t xDiff) noexcept {
    _ctxX.spanAdvance(xPos, xDiff);
  }

  BL_INLINE void spanEndX(uint32_t xPos) noexcept {
    _ctxX.spanEnd(xPos);
  }

  BL_INLINE void advanceY() noexcept {
    _ctxY.advance1();
  }

  BL_INLINE PixelType fetch() noexcept {
    PixelType pixel = PixelIO<PixelType, kFormat>::fetch(_ctxY.pixelPtr() + _ctxX.index());
    _ctxX.advance1();
    return pixel;
  }
};

template<typename DstPixelT, BLInternalFormat kFormat>
struct FetchPatternAlignedPad : public FetchPatternAlignedAny<DstPixelT, kFormat, FetchPatternHorzExtendCtxPad<FormatMetadata<kFormat>::kBPP>> {};

template<typename DstPixelT, BLInternalFormat kFormat>
struct FetchPatternAlignedRepeat : public FetchPatternAlignedAny<DstPixelT, kFormat, FetchPatternHorzExtendCtxRepeat<FormatMetadata<kFormat>::kBPP>> {};

template<typename DstPixelT, BLInternalFormat kFormat>
struct FetchPatternAlignedRoR : public FetchPatternAlignedAny<DstPixelT, kFormat, FetchPatternHorzExtendCtxRoR<FormatMetadata<kFormat>::kBPP>> {};

// Fetch - Pattern - FxFy
// ======================

template<typename DstPixelT, BLInternalFormat kFormat, typename CtxX>
struct FetchPatternFxFyAny : public FetchNonSolid {
  typedef DstPixelT PixelType;

  FetchPatternVertFyExtendCtxAny _ctxY;
  CtxX _ctxX;
  typename PixelType::Unpacked _prev;

  uint32_t _wa;
  uint32_t _wb;
  uint32_t _wc;
  uint32_t _wd;

  BL_INLINE void _initPrevX() noexcept {
    size_t index = _ctxX.index();
    PixelType p0 = PixelIO<PixelType, kFormat>::fetch(_ctxY.pixelPtr0() + index);
    PixelType p1 = PixelIO<PixelType, kFormat>::fetch(_ctxY.pixelPtr1() + index);

    auto pA = p0.unpack() * Pixel::Repeat{_wa};
    auto pC = p1.unpack() * Pixel::Repeat{_wc};

    _prev = pA + pC;
    _ctxX.advance1();
  }

  BL_INLINE void _updatePrevX(const typename PixelType::Unpacked& p) noexcept {
    _prev = p;
  }

  BL_INLINE void _initFxFy(const FetchData::Pattern* pattern) noexcept {
    _wa = pattern->simple.wa;
    _wb = pattern->simple.wb;
    _wc = pattern->simple.wc;
    _wd = pattern->simple.wd;
  }

  BL_INLINE void rectInitFetch(ContextData* ctxData, const void* fetchData, uint32_t xPos, uint32_t yPos, uint32_t rectWidth) noexcept {
    blUnused(ctxData);

    const FetchData::Pattern* pattern = static_cast<const FetchData::Pattern*>(fetchData);
    _ctxY.init(pattern, yPos);
    _ctxX.rectInit(pattern, xPos, rectWidth);
    _initFxFy(pattern);
  }

  BL_INLINE void rectStartX(uint32_t xPos) noexcept {
    _ctxX.rectStart(xPos);
    _initPrevX();
  }

  BL_INLINE void spanInitY(ContextData* ctxData, const void* fetchData, uint32_t yPos) noexcept {
    blUnused(ctxData);

    const FetchData::Pattern* pattern = static_cast<const FetchData::Pattern*>(fetchData);
    _ctxY.init(pattern, yPos);
    _ctxX.spanInit(pattern);
    _initFxFy(pattern);
  }

  BL_INLINE void spanStartX(uint32_t xPos) noexcept {
    _ctxX.spanStart(xPos);
    _initPrevX();
  }

  BL_INLINE void spanAdvanceX(uint32_t xPos, uint32_t xDiff) noexcept {
    if (xDiff) {
      _ctxX.spanAdvance(xPos, xDiff - 1);
      _initPrevX();
    }
  }

  BL_INLINE void spanEndX(uint32_t xPos) noexcept {
    _ctxX.spanEnd(xPos);
  }

  BL_INLINE void advanceY() noexcept {
    _ctxY.advance1();
  }

  BL_INLINE PixelType fetch() noexcept {
    size_t index = _ctxX.index();
    _ctxX.advance1();

    auto pixel0 = PixelIO<PixelType, kFormat>::fetch(_ctxY.pixelPtr0() + index).unpack();
    auto pixel1 = PixelIO<PixelType, kFormat>::fetch(_ctxY.pixelPtr1() + index).unpack();

    auto unpacked = pixel0 * Pixel::Repeat{_wb} + pixel1 * Pixel::Repeat{_wd} + _prev;
    _updatePrevX(pixel0 * Pixel::Repeat{_wa} + pixel1 * Pixel::Repeat{_wc});

    return unpacked.div256().pack();
  }
};

template<typename DstPixelT, BLInternalFormat kFormat>
struct FetchPatternFxFyPad : public FetchPatternFxFyAny<DstPixelT, kFormat, FetchPatternHorzExtendCtxPad<FormatMetadata<kFormat>::kBPP>> {};

template<typename DstPixelT, BLInternalFormat kFormat>
struct FetchPatternFxFyRoR : public FetchPatternFxFyAny<DstPixelT, kFormat, FetchPatternHorzExtendCtxRoR<FormatMetadata<kFormat>::kBPP>> {};

// Fetch - Pattern - Affine
// ========================

template<typename DstPixelT, BLInternalFormat kFormat>
struct FetchPatternAffineNNBase : public FetchNonSolid {
  const uint8_t* _pixelData;
  intptr_t _stride;
  FetchPatternAffineCtx _ctx;

  BL_INLINE void _initAffine(const FetchData::Pattern* pattern) noexcept {
    _pixelData = pattern->src.pixelData;
    _stride = pattern->src.stride;
  }

  BL_INLINE void rectInitFetch(ContextData* ctxData, const void* fetchData, uint32_t xPos, uint32_t yPos, uint32_t rectWidth) noexcept {
    const FetchData::Pattern* pattern = static_cast<const FetchData::Pattern*>(fetchData);
    _initAffine(pattern);
    _ctx.rectInitY(ctxData, pattern, xPos, yPos, rectWidth);
  }

  BL_INLINE void rectStartX(uint32_t xPos) noexcept {
    _ctx.rectStartX(xPos);
  }

  BL_INLINE void spanInitY(ContextData* ctxData, const void* fetchData, uint32_t yPos) noexcept {
    const FetchData::Pattern* pattern = static_cast<const FetchData::Pattern*>(fetchData);
    _initAffine(pattern);
    _ctx.spanInitY(ctxData, pattern, yPos);
  }

  BL_INLINE void spanStartX(uint32_t xPos) noexcept {
    _ctx.spanStartX(xPos);
  }

  BL_INLINE void spanAdvanceX(uint32_t xPos, uint32_t xDiff) noexcept {
    _ctx.spanAdvanceX(xPos, xDiff);
  }

  BL_INLINE void spanEndX(uint32_t xPos) noexcept {
    _ctx.spanEndX(xPos);
  }

  BL_INLINE void advanceY() noexcept {
    _ctx.advanceY();
  }
};

template<typename DstPixelT, BLInternalFormat kFormat>
struct FetchPatternAffineNNAny : public FetchPatternAffineNNBase<DstPixelT, kFormat> {
  typedef DstPixelT PixelType;

  static constexpr uint32_t kSrcBPP = FormatMetadata<kFormat>::kBPP;

  using FetchPatternAffineNNBase<DstPixelT, kFormat>::_pixelData;
  using FetchPatternAffineNNBase<DstPixelT, kFormat>::_stride;
  using FetchPatternAffineNNBase<DstPixelT, kFormat>::_ctx;

  BL_INLINE PixelType fetch() noexcept {
    Vec2T<size_t> index = _ctx.index();
    _ctx.advanceX();

    const uint8_t* p = _pixelData + intptr_t(index.y) * _stride + index.x * kSrcBPP;
    return PixelIO<PixelType, kFormat>::fetch(p);
  }
};

template<typename DstPixelT, BLInternalFormat kFormat>
struct FetchPatternAffineBIAny : public FetchPatternAffineNNBase<DstPixelT, kFormat> {
  typedef DstPixelT PixelType;

  static constexpr uint32_t kSrcBPP = FormatMetadata<kFormat>::kBPP;

  using FetchPatternAffineNNBase<DstPixelT, kFormat>::_pixelData;
  using FetchPatternAffineNNBase<DstPixelT, kFormat>::_stride;
  using FetchPatternAffineNNBase<DstPixelT, kFormat>::_ctx;

  BL_INLINE PixelType fetch() noexcept {
    Vec2T<size_t> index0 = _ctx.index();
    Vec2T<size_t> index1 = _ctx.index(1, 1);

    uint32_t wx = _ctx.fracX();
    uint32_t wy = _ctx.fracY();

    _ctx.advanceX();

    uint32_t wa = ((256 - wy) * (256 - wx) + 255) >> 8; // [x0 y0]
    uint32_t wb = ((256 - wy) * (      wx)      ) >> 8; // [x1 y0]
    uint32_t wc = ((      wy) * (256 - wx) + 255) >> 8; // [x0 y1]
    uint32_t wd = ((      wy) * (      wx)      ) >> 8; // [x1 y1]

    const uint8_t* line0 = _pixelData + intptr_t(index0.y) * _stride;
    const uint8_t* line1 = _pixelData + intptr_t(index1.y) * _stride;

    auto p = PixelIO<PixelType, kFormat>::fetch(line0 + index0.x * kSrcBPP).unpack() * Pixel::Repeat{wa} +
             PixelIO<PixelType, kFormat>::fetch(line0 + index1.x * kSrcBPP).unpack() * Pixel::Repeat{wb} +
             PixelIO<PixelType, kFormat>::fetch(line1 + index0.x * kSrcBPP).unpack() * Pixel::Repeat{wc} +
             PixelIO<PixelType, kFormat>::fetch(line1 + index1.x * kSrcBPP).unpack() * Pixel::Repeat{wd} ;
    return p.div256().pack();
  }
};

// Fetch - Gradient - Base
// =======================

template<typename PixelType, BLGradientQuality kQuality>
struct FetchGradientBase;

template<typename PixelType>
struct FetchGradientBase<PixelType, BL_GRADIENT_QUALITY_NEAREST> : public FetchNonSolid {
  const void* _table;

  BL_INLINE void _initGradientBase(ContextData* ctxData, const FetchData::Gradient* gradient, uint32_t yPos) noexcept {
    blUnused(ctxData, yPos);

    _table = gradient->lut.data;
  }

  BL_INLINE void _initGradientX(uint32_t xPos) noexcept { blUnused(xPos); }
  BL_INLINE void _advanceGradientY() noexcept {}

  BL_INLINE PixelType fetchPixel(uint32_t idx) const noexcept {
    return PixelIO<PixelType, BLInternalFormat::kPRGB32>::fetch(static_cast<const uint32_t*>(_table) + idx);
  }
};

template<typename PixelType>
struct FetchGradientBase<PixelType, BL_GRADIENT_QUALITY_DITHER> : public FetchNonSolid {
  const void* _table;
  uint32_t _dmOffsetY;
  uint32_t _dmOffsetX;

  static constexpr uint32_t kAdvanceYMask = (16u * 16u * 2u) - 1u;

  BL_INLINE void _initGradientBase(ContextData* ctxData, const FetchData::Gradient* gradient, uint32_t yPos) noexcept {
    blUnused(ctxData);

    _table = gradient->lut.data;
    _dmOffsetY = ((uint32_t(ctxData->pixelOrigin.y) + yPos) & 15u) * (16u * 2u) + (ctxData->pixelOrigin.x & 15u);
  }

  BL_INLINE void _initGradientX(uint32_t xPos) noexcept {
    _dmOffsetX = (xPos & 15u);
  }

  BL_INLINE void _advanceGradientY() noexcept {
    _dmOffsetY = (_dmOffsetY + 16u * 2u) & kAdvanceYMask;
  }

  BL_INLINE PixelType fetchPixel(uint32_t idx) noexcept {
    BLRgba64 v{static_cast<const uint64_t*>(_table)[idx]};
    uint32_t dd = blCommonTable.bayerMatrix16x16[_dmOffsetY + _dmOffsetX];

    uint32_t a = v.a() >> 8;
    uint32_t r = blMin<uint32_t>((v.r() + dd) >> 8, a);
    uint32_t g = blMin<uint32_t>((v.g() + dd) >> 8, a);
    uint32_t b = blMin<uint32_t>((v.b() + dd) >> 8, a);

    _dmOffsetX = (_dmOffsetX + 1u) & 15u;
    return PixelIO<PixelType, BLInternalFormat::kPRGB32>::make(r, g, b, a);
  }
};

// Fetch - Gradient - Linear
// =========================

template<typename PixelType, BLGradientQuality kQuality, bool kIsPad>
struct FetchLinearGradient : public FetchGradientBase<PixelType, kQuality> {
  using Base = FetchGradientBase<PixelType, kQuality>;
  using Base::fetchPixel;

  uint64_t _pt;
  uint64_t _dt;
  uint64_t _py;
  uint64_t _dy;
  uint32_t _maxi;
  uint32_t _rori;

  BL_INLINE void _initFetch(ContextData* ctxData, const void* fetchData, uint32_t yPos) noexcept {
    const FetchData::Gradient* gradient = static_cast<const FetchData::Gradient*>(fetchData);
    const FetchData::Gradient::Linear& linear = gradient->linear;

    Base::_initGradientBase(ctxData, gradient, yPos);
    _pt = 0;
    _py = linear.pt[0].u64;
    _dt = linear.dt.u64;
    _dy = linear.dy.u64;
    _maxi = linear.maxi;
    _rori = linear.rori;
  }

  BL_INLINE void rectInitFetch(ContextData* ctxData, const void* fetchData, uint32_t xPos, uint32_t yPos, uint32_t rectWidth) noexcept {
    blUnused(rectWidth);

    _initFetch(ctxData, fetchData, yPos);
    _py += yPos * _dy + xPos * _dt;
  }

  BL_INLINE void rectStartX(uint32_t xPos) noexcept {
    Base::_initGradientX(xPos);
    _pt = _py;
  }

  BL_INLINE void spanInitY(ContextData* ctxData, const void* fetchData, uint32_t yPos) noexcept {
    _initFetch(ctxData, fetchData, yPos);
    _py += yPos * _dy;
  }

  BL_INLINE void spanStartX(uint32_t xPos) noexcept {
    Base::_initGradientX(xPos);
    _pt = _py + xPos * _dt;
  }

  BL_INLINE void spanAdvanceX(uint32_t xPos, uint32_t xDiff) noexcept {
    Base::_initGradientX(xPos);
    _pt += xDiff * _dt;
  }

  BL_INLINE void spanEndX(uint32_t xPos) noexcept {
    blUnused(xPos);
  }

  BL_INLINE void advanceY() noexcept {
    _py += _dy;
    Base::_advanceGradientY();
  }

  BL_INLINE PixelType fetch() noexcept {
    uint32_t idx = uint32_t(_pt >> 32);

    if (kIsPad)
      idx = uint32_t(blClamp<int32_t>(int32_t(idx), 0, int32_t(_maxi)));
    else
      idx = blMin<uint32_t>(idx & _maxi, (idx & _maxi) ^ _rori);

    _pt += _dt;
    return fetchPixel(idx);
  }
};

// Fetch - Gradient - Radial
// =========================

template<typename PixelType, BLGradientQuality kQuality, bool kIsPad>
struct FetchRadialGradient : public FetchGradientBase<PixelType, kQuality> {
  using Base = FetchGradientBase<PixelType, kQuality>;
  using Base::fetchPixel;

  Vec2D xx_xy;
  Vec2D yx_yy;
  Vec2D px_py;

  Vec2D ax_ay;
  Vec2D fx_fy;
  Vec2D da_ba;

  Vec2D d_b;
  Vec2D dd_bd;
  Vec2D ddx_ddy;

  double ddd;
  float scale_f32;
  uint32_t _maxi;
  uint32_t _rori;

  BL_INLINE void _initFetch(ContextData* ctxData, const void* fetchData, uint32_t yPos) noexcept {
    const FetchData::Gradient* gradient = static_cast<const FetchData::Gradient*>(fetchData);
    const FetchData::Gradient::Radial& radial = gradient->radial;

    Base::_initGradientBase(ctxData, gradient, yPos);
    xx_xy = Vec2D{radial.xx, radial.xy};
    yx_yy = Vec2D{radial.yx, radial.yy};
    px_py = Vec2D{radial.ox, radial.oy};
    ax_ay = Vec2D{radial.ax, radial.ay};
    fx_fy = Vec2D{radial.fx, radial.fy};
    da_ba = Vec2D{radial.dd, radial.bd};
    ddx_ddy = Vec2D{radial.ddx, radial.ddy};
    ddd = radial.ddd;
    scale_f32 = float(radial.scale);
    _maxi = radial.maxi;
    _rori = radial.rori;
  }

  BL_INLINE void rectInitFetch(ContextData* ctxData, const void* fetchData, uint32_t x, uint32_t y, uint32_t width) noexcept {
    blUnused(width);

    _initFetch(ctxData, fetchData, y);
    Vec2D pt = Vec2D{double(int32_t(x)), double(int32_t(y))};
    px_py += pt.y * yx_yy + pt.x * xx_xy;
  }

  BL_INLINE void rectStartX(uint32_t xPos) noexcept {
    Base::_initGradientX(xPos);
    precalc(px_py);
  }

  BL_INLINE void spanInitY(ContextData* ctxData, const void* fetchData, uint32_t yPos) noexcept {
    _initFetch(ctxData, fetchData, yPos);
    px_py += double(int32_t(yPos)) * yx_yy;
  }

  BL_INLINE void spanStartX(uint32_t x) noexcept {
    Base::_initGradientX(x);
    precalc(px_py + xx_xy * double(int(x)));
  }

  BL_INLINE void spanAdvanceX(uint32_t xPos, uint32_t xDiff) noexcept {
    blUnused(xDiff);

    Base::_initGradientX(xPos);
    precalc(px_py + xx_xy * double(int(xPos)));
  }

  BL_INLINE void spanEndX(uint32_t xPos) noexcept {
    blUnused(xPos);
  }

  BL_INLINE void advanceY() noexcept {
    px_py += yx_yy;
    Base::_advanceGradientY();
  }

  BL_INLINE void precalc(const Vec2D& tx_ty) noexcept {
    Vec2D tx_fx_ty_fy = tx_ty * fx_fy;
    Vec2D tx_ddx_ty_ddy = tx_ty * ddx_ddy;
    double z = tx_fx_ty_fy.hmul();

    d_b = Vec2D{(ax_ay * tx_ty * tx_ty).hadd() + z + z, tx_fx_ty_fy.hadd()};
    dd_bd = Vec2D{da_ba.x + tx_ddx_ty_ddy.x + tx_ddx_ty_ddy.y, da_ba.y};
  }

  BL_INLINE PixelType fetch() noexcept {
    float v_f32 = blSqrt(blAbs(float(d_b.x)));
    float b_f32 = float(d_b.y);

    d_b += dd_bd;
    v_f32 = (v_f32 + b_f32) * scale_f32;

    uint32_t idx = uint32_t(int(v_f32));

    if (kIsPad)
      idx = uint32_t(blClamp<int32_t>(int32_t(idx), 0, int32_t(_maxi)));
    else
      idx = blMin<uint32_t>(idx & _maxi, (idx & _maxi) ^ _rori);

    dd_bd.x += ddd;
    return fetchPixel(idx);
  }
};

template<typename PixelType, BLGradientQuality kQuality>
struct FetchRadialGradientPad : public FetchRadialGradient<PixelType, kQuality, true> {};

template<typename PixelType, BLGradientQuality kQuality>
struct FetchRadialGradientRoR : public FetchRadialGradient<PixelType, kQuality, false> {};

// Fetch - Gradient - Conic
// ========================

template<typename PixelType, BLGradientQuality kQuality>
struct FetchConicGradient : public FetchGradientBase<PixelType, kQuality> {
  using Base = FetchGradientBase<PixelType, kQuality>;
  using Base::fetchPixel;

  double xx;
  Vec2D yx_yy;
  Vec2D hx_hy;
  Vec2D px_py;

  const BLCommonTable::Conic* consts;
  float angleOffset;
  int maxi;

  BL_INLINE void _initFetch(ContextData* ctxData, const void* fetchData, uint32_t yPos) noexcept {
    const FetchData::Gradient* gradient = static_cast<const FetchData::Gradient*>(fetchData);
    const FetchData::Gradient::Conic& conic = gradient->conic;

    Base::_initGradientBase(ctxData, gradient, yPos);
    xx = conic.xx;
    yx_yy = Vec2D{conic.yx, conic.yy};
    hx_hy = Vec2D{conic.ox, conic.oy};
    consts = conic.consts;
    angleOffset = conic.offset;
    maxi = conic.maxi;
  }

  BL_INLINE void rectInitFetch(ContextData* ctxData, const void* fetchData, uint32_t x, uint32_t y, uint32_t width) noexcept {
    blUnused(width);

    _initFetch(ctxData, fetchData, y);
    Base::_initGradientX(x);

    Vec2D pt = Vec2D{double(int32_t(x)), double(int32_t(y))};
    hx_hy += pt.y * yx_yy + Vec2D{pt.x * xx, 0.0};
  }

  BL_INLINE void rectStartX(uint32_t xPos) noexcept {
    Base::_initGradientX(xPos);
    px_py = hx_hy;
  }

  BL_INLINE void spanInitY(ContextData* ctxData, const void* fetchData, uint32_t yPos) noexcept {
    _initFetch(ctxData, fetchData, yPos);
    hx_hy += double(int32_t(yPos)) * yx_yy;
  }

  BL_INLINE void spanStartX(uint32_t xPos) noexcept {
    Base::_initGradientX(xPos);
    px_py = hx_hy;
    px_py.x += double(int32_t(xPos)) * xx;
  }

  BL_INLINE void spanAdvanceX(uint32_t xPos, uint32_t xDiff) noexcept {
    blUnused(xDiff);

    Base::_initGradientX(xPos);
    px_py.x += double(int32_t(xPos)) * xx;
  }

  BL_INLINE void spanEndX(uint32_t xPos) noexcept {
    blUnused(xPos);
  }

  BL_INLINE void advanceY() noexcept {
    hx_hy += yx_yy;
    Base::_advanceGradientY();
  }

  BL_INLINE PixelType fetch() noexcept {
    Vec2F pt = Vec2F{float(px_py.x), float(px_py.y)};
    Vec2F x1 = v_abs(pt);

    px_py.x += xx;

    float x2 = blMax(x1.x, x1.y);
    float x3 = blMin(x1.x, x1.y);

    float s = bitAnd(blBitCast<float>(BLIntOps::bitMaskFromBool<uint32_t>(x1.x == x3)), consts->n_div_4[0]);
    x3 = x3 / x2;
    x2 = x3 * x3;
    pt = msbMask(pt) & Vec2F{consts->n_extra[0], consts->n_extra[1]};

    float x4 = x2 * consts->q3[0];
    x4 = x4 + consts->q2[0];
    x4 = x4 * x2 + consts->q1[0];
    x2 = x2 * x4 + consts->q0[0];

    x2 = blAbs(x2 * x3 - s);
    x2 = blAbs(x2 - pt.x);
    x2 = blAbs(x2 - pt.y) + angleOffset;

    uint32_t idx = uint32_t(int(x2)) & uint32_t(maxi);
    return fetchPixel(idx);
  }
};

} // {Reference}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_REFERENCE_FETCHGENERIC_P_H_INCLUDED
