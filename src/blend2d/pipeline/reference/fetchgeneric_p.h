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

static BL_INLINE float msbMask(float a) noexcept { return blBitCast<float>(blBitCast<int32_t>(a) >> 31); }
static BL_INLINE float bitNot(float a) noexcept { return blBitCast<float>(~blBitCast<uint32_t>(a)); }
static BL_INLINE float bitAnd(float a, float b) noexcept { return blBitCast<float>(blBitCast<uint32_t>(a) & blBitCast<uint32_t>(b)); }
static BL_INLINE float bitOr(float a, float b) noexcept { return blBitCast<float>(blBitCast<uint32_t>(a) | blBitCast<uint32_t>(b)); }
static BL_INLINE float bitXor(float a, float b) noexcept { return blBitCast<float>(blBitCast<uint32_t>(a) ^ blBitCast<uint32_t>(b)); }

static BL_INLINE double msbMask(double a) noexcept { return blBitCast<double>(blBitCast<int64_t>(a) >> 63); }
static BL_INLINE double bitNot(double a) noexcept { return blBitCast<double>(~blBitCast<uint64_t>(a)); }
static BL_INLINE double bitAnd(double a, double b) noexcept { return blBitCast<double>(blBitCast<uint64_t>(a) & blBitCast<uint64_t>(b)); }
static BL_INLINE double bitOr(double a, double b) noexcept { return blBitCast<double>(blBitCast<uint64_t>(a) | blBitCast<uint64_t>(b)); }
static BL_INLINE double bitXor(double a, double b) noexcept { return blBitCast<double>(blBitCast<uint64_t>(a) ^ blBitCast<uint64_t>(b)); }

template<typename T>
struct Vec2T {
  T x, y;

  BL_INLINE Vec2T swap() const noexcept { return Vec2T{y, x}; }

  BL_INLINE void reset(double x_, double y_) noexcept { x = x_; y = y_; }
  BL_INLINE double hadd() const noexcept { return x + y; }
  BL_INLINE double hmul() const noexcept { return x * y; }

  BL_INLINE Vec2T& operator+=(T scalar) noexcept { *this = Vec2T{x + scalar, y + scalar}; return *this; }
  BL_INLINE Vec2T& operator-=(T scalar) noexcept { *this = Vec2T{x - scalar, y - scalar}; return *this; }
  BL_INLINE Vec2T& operator*=(T scalar) noexcept { *this = Vec2T{x * scalar, y * scalar}; return *this; }
  BL_INLINE Vec2T& operator/=(T scalar) noexcept { *this = Vec2T{x / scalar, y / scalar}; return *this; }
  BL_INLINE Vec2T& operator&=(T scalar) noexcept { *this = Vec2T{bitAnd(x, scalar), bitAnd(y, scalar)}; return *this; }
  BL_INLINE Vec2T& operator|=(T scalar) noexcept { *this = Vec2T{bitOr(x, scalar), bitOr(y, scalar)}; return *this; }
  BL_INLINE Vec2T& operator^=(T scalar) noexcept { *this = Vec2T{bitXor(x, scalar), bitXor(y, scalar)}; return *this; }

  BL_INLINE Vec2T& operator+=(const Vec2T& other) noexcept { *this = Vec2T{x + other.x, y + other.y}; return *this; }
  BL_INLINE Vec2T& operator-=(const Vec2T& other) noexcept { *this = Vec2T{x - other.x, y - other.y}; return *this; }
  BL_INLINE Vec2T& operator*=(const Vec2T& other) noexcept { *this = Vec2T{x * other.x, y * other.y}; return *this; }
  BL_INLINE Vec2T& operator/=(const Vec2T& other) noexcept { *this = Vec2T{x / other.x, y / other.y}; return *this; }
  BL_INLINE Vec2T& operator&=(const Vec2T& other) noexcept { *this = Vec2T{bitAnd(x, other.x), bitAnd(y, other.y)}; return *this; }
  BL_INLINE Vec2T& operator|=(const Vec2T& other) noexcept { *this = Vec2T{bitOr(x, other.x), bitOr(y, other.y)}; return *this; }
  BL_INLINE Vec2T& operator^=(const Vec2T& other) noexcept { *this = Vec2T{bitXor(x, other.x), bitXor(y, other.y)}; return *this; }
};

template<typename T> static BL_INLINE Vec2T<T> operator+(T a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a + b.x, a + b.y}; };
template<typename T> static BL_INLINE Vec2T<T> operator-(T a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a - b.x, a - b.y}; };
template<typename T> static BL_INLINE Vec2T<T> operator*(T a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a * b.x, a * b.y}; };
template<typename T> static BL_INLINE Vec2T<T> operator/(T a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a / b.x, a / b.y}; };
template<typename T> static BL_INLINE Vec2T<T> operator&(T a, const Vec2T<T>& b) noexcept { return Vec2T<T>{bitAnd(a, b.x), bitAnd(a, b.y)}; };
template<typename T> static BL_INLINE Vec2T<T> operator|(T a, const Vec2T<T>& b) noexcept { return Vec2T<T>{bitOr(a, b.x), bitOr(a, b.y)}; };
template<typename T> static BL_INLINE Vec2T<T> operator^(T a, const Vec2T<T>& b) noexcept { return Vec2T<T>{bitXor(a, b.x), bitXor(a, b.y)}; };

template<typename T> static BL_INLINE Vec2T<T> operator+(const Vec2T<T>& a, T b) noexcept { return Vec2T<T>{a.x + b, a.y + b}; };
template<typename T> static BL_INLINE Vec2T<T> operator-(const Vec2T<T>& a, T b) noexcept { return Vec2T<T>{a.x - b, a.y - b}; };
template<typename T> static BL_INLINE Vec2T<T> operator*(const Vec2T<T>& a, T b) noexcept { return Vec2T<T>{a.x * b, a.y * b}; };
template<typename T> static BL_INLINE Vec2T<T> operator/(const Vec2T<T>& a, T b) noexcept { return Vec2T<T>{a.x / b, a.y / b}; };
template<typename T> static BL_INLINE Vec2T<T> operator&(const Vec2T<T>& a, T b) noexcept { return Vec2T<T>{bitAnd(a.x, b), bitAnd(a.y, b)}; };
template<typename T> static BL_INLINE Vec2T<T> operator|(const Vec2T<T>& a, T b) noexcept { return Vec2T<T>{bitOr(a.x, b), bitOr(a.y, b)}; };
template<typename T> static BL_INLINE Vec2T<T> operator^(const Vec2T<T>& a, T b) noexcept { return Vec2T<T>{bitXor(a.x, b), bitXor(a.y, b)}; };

template<typename T> static BL_INLINE Vec2T<T> operator+(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a.x + b.x, a.y + b.y}; };
template<typename T> static BL_INLINE Vec2T<T> operator-(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a.x - b.x, a.y - b.y}; };
template<typename T> static BL_INLINE Vec2T<T> operator*(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a.x * b.x, a.y * b.y}; };
template<typename T> static BL_INLINE Vec2T<T> operator/(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{a.x / b.x, a.y / b.y}; };
template<typename T> static BL_INLINE Vec2T<T> operator&(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{bitAnd(a.x, b.x), bitAnd(a.y, b.y)}; };
template<typename T> static BL_INLINE Vec2T<T> operator|(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{bitOr(a.x, b.x), bitOr(a.y, b.y)}; };
template<typename T> static BL_INLINE Vec2T<T> operator^(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{bitXor(a.x, b.x), bitXor(a.y, b.y)}; };

template<typename T> static BL_INLINE Vec2T<T> msbMask(Vec2T<T> a) noexcept { return Vec2T<T>{msbMask(a.x), msbMask(a.y)}; }
template<typename T> static BL_INLINE Vec2T<T> v_abs(const Vec2T<T>& v) noexcept { return Vec2T<T>{blAbs(v.x), blAbs(v.y)}; }
template<typename T> static BL_INLINE Vec2T<T> v_min(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{blMin(a.x, b.x), blMin(a.y, b.y)}; }
template<typename T> static BL_INLINE Vec2T<T> v_max(const Vec2T<T>& a, const Vec2T<T>& b) noexcept { return Vec2T<T>{blMax(a.x, b.x), blMax(a.y, b.y)}; }

typedef Vec2T<float> Vec2F;
typedef Vec2T<double> Vec2D;

template<typename PixelT>
struct FetchSolid {
  typedef PixelT PixelType;
  enum : uint32_t { kIsSolid = 1 };

  PixelType src;

  BL_INLINE void _initFetch(const void* fetchData) noexcept {
    src = PixelType::fromValue(static_cast<const FetchData::Solid*>(fetchData)->prgb32);
  }

  BL_INLINE void rectInitFetch(const void* fetchData, uint32_t xPos, uint32_t yPos, uint32_t rectWidth) noexcept {
    _initFetch(fetchData);
    blUnused(xPos, yPos, rectWidth);
  }

  BL_INLINE void rectStartX(uint32_t xPos) noexcept { blUnused(xPos); }

  BL_INLINE void spanInitY(const void* fetchData, uint32_t yPos) noexcept {
    _initFetch(fetchData);
    blUnused(yPos);
  }

  BL_INLINE void spanStartX(uint32_t xPos) noexcept { blUnused(xPos); }
  BL_INLINE void spanAdvanceX(uint32_t xPos, uint32_t xDiff) noexcept { blUnused(xPos, xDiff); }
  BL_INLINE void spanEndX(uint32_t xPos) noexcept { blUnused(xPos); }

  BL_INLINE void advanceY() noexcept {}

  BL_INLINE PixelType fetch() const noexcept { return src; }
};

struct FetchNonSolid {
  enum : uint32_t { kIsSolid = 0 };
};

struct FetchGradientBase : public FetchNonSolid {
  const void* _table;
};

template<typename PixelT, bool kIsPad>
struct FetchLinearGradient : public FetchGradientBase {
  typedef PixelT PixelType;

  uint64_t _pt;
  uint64_t _dt;
  uint64_t _py;
  uint64_t _dy;
  uint32_t _maxi;
  uint32_t _rori;

  BL_INLINE void _initFetch(const void* fetchData) noexcept {
    const FetchData::Gradient* gradient = static_cast<const FetchData::Gradient*>(fetchData);
    _table = gradient->lut.data;

    const FetchData::Gradient::Linear& linear = gradient->linear;
    _pt = 0;
    _py = linear.pt[0].u64;
    _dt = linear.dt.u64;
    _dy = linear.dy.u64;
    _maxi = linear.maxi;
    _rori = linear.rori;
  }

  BL_INLINE void rectInitFetch(const void* fetchData, uint32_t xPos, uint32_t yPos, uint32_t rectWidth) noexcept {
    blUnused(rectWidth);

    _initFetch(fetchData);
    _py += yPos * _dy + xPos * _dt;
  }

  BL_INLINE void rectStartX(uint32_t xPos) noexcept {
    blUnused(xPos);
    _pt = _py;
  }

  BL_INLINE void spanInitY(const void* fetchData, uint32_t yPos) noexcept {
    _initFetch(fetchData);
    _py += yPos * _dy;
  }

  BL_INLINE void spanStartX(uint32_t xPos) noexcept {
    _pt = _py + xPos * _dt;
  }

  BL_INLINE void spanAdvanceX(uint32_t xPos, uint32_t xDiff) noexcept {
    blUnused(xPos);
    _pt += xDiff * _dt;
  }

  BL_INLINE void spanEndX(uint32_t xPos) noexcept {
    blUnused(xPos);
  }

  BL_INLINE void advanceY() noexcept {
    _py += _dy;
  }

  BL_INLINE PixelType fetch() noexcept {
    uint32_t idx = uint32_t(_pt >> 32);

    if (kIsPad)
      idx = uint32_t(blClamp<int32_t>(int32_t(idx), 0, int32_t(_maxi)));
    else
      idx = blMin<uint32_t>(idx & _maxi, (idx & _maxi) ^ _rori);

    _pt += _dt;
    return PixelIO<PixelType, BLInternalFormat::kPRGB32>::fetch(static_cast<const uint32_t*>(_table) + idx);
  }
};

template<typename PixelT, bool kIsPad>
struct FetchRadialGradient : public FetchGradientBase {
  typedef PixelT PixelType;

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

  BL_INLINE void _initFetch(const void* fetchData) noexcept {
    const FetchData::Gradient* gradient = static_cast<const FetchData::Gradient*>(fetchData);
    _table = gradient->lut.data;

    const FetchData::Gradient::Radial& radial = gradient->radial;
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

  BL_INLINE void rectInitFetch(const void* fetchData, uint32_t x, uint32_t y, uint32_t width) noexcept {
    blUnused(width);

    _initFetch(fetchData);
    Vec2D pt = Vec2D{double(int32_t(x)), double(int32_t(y))};
    px_py += pt.y * yx_yy + pt.x * xx_xy;
  }

  BL_INLINE void rectStartX(uint32_t xPos) noexcept {
    blUnused(xPos);
    precalc(px_py);
  }

  BL_INLINE void spanInitY(const void* fetchData, uint32_t yPos) noexcept {
    _initFetch(fetchData);
    px_py += double(int32_t(yPos)) * yx_yy;
  }

  BL_INLINE void spanStartX(uint32_t x) noexcept {
    precalc(px_py + xx_xy * double(int(x)));
  }

  BL_INLINE void spanAdvanceX(uint32_t xPos, uint32_t xDiff) noexcept {
    blUnused(xDiff);
    precalc(px_py + xx_xy * double(int(xPos)));
  }

  BL_INLINE void spanEndX(uint32_t xPos) noexcept {
    blUnused(xPos);
  }

  BL_INLINE void advanceY() noexcept {
    px_py += yx_yy;
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
    return PixelIO<PixelType, BLInternalFormat::kPRGB32>::fetch(static_cast<const uint32_t*>(_table) + idx);
  }
};

template<typename PixelT>
struct FetchRadialGradientPad : public FetchRadialGradient<PixelT, true> {};

template<typename PixelT>
struct FetchRadialGradientRoR : public FetchRadialGradient<PixelT, false> {};

template<typename PixelT>
struct FetchConicalGradient : public FetchGradientBase {
  typedef PixelT PixelType;

  Vec2D xx_xy;
  Vec2D yx_yy;

  Vec2D hx_hy;
  Vec2D px_py;

  const BLCommonTable::Conical* consts;
  int maxi;

  BL_INLINE void _initFetch(const void* fetchData) noexcept {
    const FetchData::Gradient* gradient = static_cast<const FetchData::Gradient*>(fetchData);
    _table = gradient->lut.data;

    const FetchData::Gradient::Conical& conical = gradient->conical;
    xx_xy = Vec2D{conical.xx, conical.xy};
    yx_yy = Vec2D{conical.yx, conical.yy};
    hx_hy = Vec2D{conical.ox, conical.oy};
    consts = conical.consts;
    maxi = conical.maxi;
  }

  BL_INLINE void rectInitFetch(const void* fetchData, uint32_t x, uint32_t y, uint32_t width) noexcept {
    blUnused(width);

    _initFetch(fetchData);
    Vec2D pt = Vec2D{double(int32_t(x)), double(int32_t(y))};
    hx_hy = hx_hy + pt.y * yx_yy + pt.x * xx_xy;
  }

  BL_INLINE void rectStartX(uint32_t xPos) noexcept {
    blUnused(xPos);
    px_py = hx_hy;
  }

  BL_INLINE void spanInitY(const void* fetchData, uint32_t yPos) noexcept {
    _initFetch(fetchData);
    hx_hy += double(int32_t(yPos)) * yx_yy;
  }

  BL_INLINE void spanStartX(uint32_t xPos) noexcept {
    px_py = hx_hy + double(int32_t(xPos)) * xx_xy;
  }

  BL_INLINE void spanAdvanceX(uint32_t xPos, uint32_t xDiff) noexcept {
    blUnused(xDiff);
    px_py = hx_hy + double(int32_t(xPos)) * xx_xy;
  }

  BL_INLINE void spanEndX(uint32_t xPos) noexcept {
    blUnused(xPos);
  }

  BL_INLINE void advanceY() noexcept {
    hx_hy += yx_yy;
  }

  BL_INLINE PixelType fetch() noexcept {
    Vec2F pt = Vec2F{float(px_py.x), float(px_py.y)};
    Vec2F x1 = v_abs(pt);

    px_py += xx_xy;

    float x2 = blMax(x1.x, x1.y, 1e-20f);
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
    x2 = blAbs(x2 - pt.y);

    uint32_t idx = uint32_t(int(x2)) & uint32_t(maxi);
    return PixelIO<PixelType, BLInternalFormat::kPRGB32>::fetch(static_cast<const uint32_t*>(_table) + idx);
  }
};

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

  BL_INLINE void rectInitFetch(const void* fetchData, uint32_t xPos, uint32_t yPos, uint32_t rectWidth) noexcept {
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

  BL_INLINE void spanInitY(const void* fetchData, uint32_t yPos) noexcept {
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

template<typename DstPixelT, BLInternalFormat kFormat, FetchType kFetchType>
struct FetchPatternAligned : public FetchNonSolid {
  typedef DstPixelT PixelType;

  static constexpr uint32_t kSrcBPP = FormatMetadata<kFormat>::kBPP;

  const uint8_t* pixelPtr;
  intptr_t stride;
  intptr_t w;
  intptr_t h;
  intptr_t x;
  intptr_t y;

  BL_INLINE void _initFetch(const FetchData::Pattern* pattern) noexcept {
    pixelPtr = static_cast<const uint8_t*>(pattern->src.pixelData);
    stride = pattern->src.stride;
    w = pattern->src.size.w;
    h = pattern->src.size.h;
  }

  BL_INLINE void rectInitFetch(const void* fetchData, uint32_t xPos, uint32_t yPos, uint32_t rectWidth) noexcept {
    const FetchData::Pattern* pattern = static_cast<const FetchData::Pattern*>(fetchData);

    _initFetch(pattern);

    intptr_t tx = pattern->simple.tx;
    intptr_t ty = pattern->simple.ty;

    x = intptr_t(xPos) - tx;
    y = intptr_t(yPos) - ty;

    stride -= intptr_t(uintptr_t(rectWidth) * kSrcBPP);
    pixelPtr += intptr_t(yPos - ty) * stride + intptr_t(xPos - tx) * kSrcBPP;
  }

  BL_INLINE void rectStartX(uint32_t xPos) noexcept {
    blUnused(xPos);
  }

  BL_INLINE void spanInitY(const void* fetchData, uint32_t yPos) noexcept {
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

} // {Reference}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_REFERENCE_FETCHGENERIC_P_H_INCLUDED
