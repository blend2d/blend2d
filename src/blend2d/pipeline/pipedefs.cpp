// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../context.h"
#include "../math_p.h"
#include "../matrix_p.h"
#include "../pipeline/pipedefs_p.h"
#include "../support/intops_p.h"
#include "../support/traits_p.h"

namespace BLPipeline {
namespace FetchUtils {

// Pipeline - FetchData - Extend Modes
// ===================================

static BL_INLINE uint32_t extendXFromExtendMode(uint32_t extendMode) noexcept {
  BL_ASSERT(extendMode <= BL_EXTEND_MODE_COMPLEX_MAX_VALUE);

  constexpr uint32_t kTable = (BL_EXTEND_MODE_PAD     <<  0) | // [pad-x     pad-y    ]
                              (BL_EXTEND_MODE_REPEAT  <<  2) | // [repeat-x  repeat-y ]
                              (BL_EXTEND_MODE_REFLECT <<  4) | // [reflect-x reflect-y]
                              (BL_EXTEND_MODE_PAD     <<  6) | // [pad-x     repeat-y ]
                              (BL_EXTEND_MODE_PAD     <<  8) | // [pad-x     reflect-y]
                              (BL_EXTEND_MODE_REPEAT  << 10) | // [repeat-x  pad-y    ]
                              (BL_EXTEND_MODE_REPEAT  << 12) | // [repeat-x  reflect-y]
                              (BL_EXTEND_MODE_REFLECT << 14) | // [reflect-x pad-y    ]
                              (BL_EXTEND_MODE_REFLECT << 16) ; // [reflect-x repeat-y ]
  return (kTable >> (extendMode * 2u)) & 0x3u;
}

static BL_INLINE uint32_t extendYFromExtendMode(uint32_t extendMode) noexcept {
  BL_ASSERT(extendMode <= BL_EXTEND_MODE_COMPLEX_MAX_VALUE);

  constexpr uint32_t kTable = (BL_EXTEND_MODE_PAD     <<  0) | // [pad-x     pad-y    ]
                              (BL_EXTEND_MODE_REPEAT  <<  2) | // [repeat-x  repeat-y ]
                              (BL_EXTEND_MODE_REFLECT <<  4) | // [reflect-x reflect-y]
                              (BL_EXTEND_MODE_REPEAT  <<  6) | // [pad-x     repeat-y ]
                              (BL_EXTEND_MODE_REFLECT <<  8) | // [pad-x     reflect-y]
                              (BL_EXTEND_MODE_PAD     << 10) | // [repeat-x  pad-y    ]
                              (BL_EXTEND_MODE_REFLECT << 12) | // [repeat-x  reflect-y]
                              (BL_EXTEND_MODE_PAD     << 14) | // [reflect-x pad-y    ]
                              (BL_EXTEND_MODE_REPEAT  << 16) ; // [reflect-x repeat-y ]
  return (kTable >> (extendMode * 2u)) & 0x3u;
}

// Pipeline - FetchData - Init Pattern
// ===================================

static BL_INLINE Signature initPatternTxTy(FetchData::Pattern& fetchData, FetchType fetchBase, uint32_t extendMode, int tx, int ty, bool isFractional) noexcept {
  uint32_t extendX = extendXFromExtendMode(extendMode);
  uint32_t extendY = extendYFromExtendMode(extendMode);
  uint32_t ixIndex = 17;

  int rx = 0;
  int ry = 0;

  // If the pattern width/height is 1 all extend modes produce the same output. However, it's safer to just set it to
  // PAD as FetchPatternPart requires `width` to be equal or greater than 2 if the extend mode is REPEAT or REFLECT.
  if (fetchData.src.size.w <= 1) extendX = BL_EXTEND_MODE_PAD;
  if (fetchData.src.size.h <= 1) extendY = BL_EXTEND_MODE_PAD;

  if (extendX >= BL_EXTEND_MODE_REPEAT) {
    bool isReflect = extendX == BL_EXTEND_MODE_REFLECT;

    rx = int(fetchData.src.size.w) << uint32_t(isReflect);
    if (unsigned(tx) >= unsigned(rx))
      tx %= rx;
    if (tx < 0)
      tx += rx;

    // In extreme cases, when `rx` is very small, fetch4()/fetch8() functions may overflow `x` if they increment more
    // than they can fix by subtracting `rw` in case of overflow (and overflow happens as it's used to start over). To
    // fix this and simplify the compiled code we simply precalculate these constants so they are always safe.
    ixIndex = blMin<uint32_t>(uint32_t(rx), 17);

    // Don't specialize `Repeat vs Reflect` when we are not pixel aligned.
    if (isFractional)
      extendX = 1; // TODO: Naming...
  }

  // Setup vExtendData initially for PADding, then refine in REPEAT|REFLECT case.
  FetchData::Pattern::VertExtendData& extData = fetchData.simple.vExtendData;
  extData.stride[0] = fetchData.src.stride;
  extData.stride[1] = 0;
  extData.yStop[0] = uint32_t(fetchData.src.size.h);
  extData.yStop[1] = 0;
  extData.yRewindOffset = 0;
  extData.pixelPtrRewindOffset = (extendY != BL_EXTEND_MODE_REPEAT ? intptr_t(0) : intptr_t(fetchData.src.size.h - 1)) * fetchData.src.stride;

  if (extendY >= BL_EXTEND_MODE_REPEAT) {
    ry = int(fetchData.src.size.h) << uint32_t(extendY == BL_EXTEND_MODE_REFLECT);
    if (unsigned(ty) >= unsigned(ry))
      ty %= ry;
    if (ty < 0)
      ty += ry;

    extData.stride[1] = (extendY == BL_EXTEND_MODE_REPEAT) ? fetchData.src.stride : -fetchData.src.stride;
    extData.yStop[1] = uint32_t(fetchData.src.size.h);
    extData.yRewindOffset = uint32_t(fetchData.src.size.h);
  }

  fetchData.simple.tx = tx;
  fetchData.simple.ty = ty;
  fetchData.simple.rx = rx;
  fetchData.simple.ry = ry;
  fetchData.simple.ix = blModuloTable[ixIndex];

  return Signature::fromFetchType(FetchType(uint32_t(fetchBase) + extendX));
}

Signature initPatternAxAy(FetchData::Pattern& fetchData, BLExtendMode extendMode, int x, int y) noexcept {
  return initPatternTxTy(fetchData, FetchType::kPatternAlignedPad, extendMode, -x, -y, false);
}

Signature initPatternFxFy(FetchData::Pattern& fetchData, BLExtendMode extendMode, BLPatternQuality quality, uint32_t bytesPerPixel, int64_t tx64, int64_t ty64) noexcept {
  blUnused(bytesPerPixel);

  FetchType fetchBase = FetchType::kPatternAlignedPad;
  uint32_t wx = uint32_t(tx64 & 0xFF);
  uint32_t wy = uint32_t(ty64 & 0xFF);

  int tx = -int(tx64 >> 8);
  int ty = -int(ty64 >> 8);

  // If one or both `wx` or `wy` are non-zero it means that the translation is fractional. In that case we must
  // calculate weights of [x0 y0], [x1 y0], [x0 y1], and [x1 y1] pixels.
  bool isFractional = (wx | wy) != 0;
  if (isFractional) {
    if (quality == BL_PATTERN_QUALITY_NEAREST) {
      tx -= (wx >= 128);
      ty -= (wy >= 128);
      isFractional = false;
    }
    else {
      fetchData.simple.wa = ((      wy) * (      wx)      ) >> 8; // [x0 y0]
      fetchData.simple.wb = ((      wy) * (256 - wx) + 255) >> 8; // [x1 y0]
      fetchData.simple.wc = ((256 - wy) * (      wx)      ) >> 8; // [x0 y1]
      fetchData.simple.wd = ((256 - wy) * (256 - wx) + 255) >> 8; // [x1 y1]

      // The FxFy fetcher must work even when one or both `wx` or `wy` are zero, so we always decrement `tx` and `ty`.
      // In addition, Fx or Fy fetcher can be replaced by FxFy if there is no Fx or Fy implementation (typically this
      // could happen if we are running portable pipeline without any optimizations).
      tx--;
      ty--;

      if (wy == 0)
        fetchBase = FetchType::kPatternFxPad;
      else if (wx == 0)
        fetchBase = FetchType::kPatternFyPad;
      else
        fetchBase = FetchType::kPatternFxFyPad;
    }
  }

  return initPatternTxTy(fetchData, fetchBase, extendMode, tx, ty, isFractional);
}

Signature initPatternAffine(FetchData::Pattern& fetchData, BLExtendMode extendMode, BLPatternQuality quality, uint32_t bytesPerPixel, const BLMatrix2D& transform) noexcept {
  // Inverted transformation matrix.
  BLMatrix2D inv;
  if (BLMatrix2D::invert(inv, transform) != BL_SUCCESS)
    return Signature::fromPendingFlag(1);

  double xx = inv.m00;
  double xy = inv.m01;
  double yx = inv.m10;
  double yy = inv.m11;

  if (isNearOne(xx) && isNearZero(xy) && isNearZero(yx) && isNearOne(yy)) {
    int64_t tx64 = blFloorToInt64(-inv.m20 * 256.0);
    int64_t ty64 = blFloorToInt64(-inv.m21 * 256.0);
    return initPatternFxFy(fetchData, extendMode, quality, bytesPerPixel, tx64, ty64);
  }

  FetchType fetchType =
    quality == BL_PATTERN_QUALITY_NEAREST
      ? FetchType::kPatternAffineNNAny
      : FetchType::kPatternAffineBIAny;

  // Pattern bounds.
  int tw = int(fetchData.src.size.w);
  int th = int(fetchData.src.size.h);

  uint32_t opt = blMax(tw, th) < 32767 &&
                 fetchData.src.stride >= 0 &&
                 fetchData.src.stride <= intptr_t(BLTraits::maxValue<int16_t>());

  // TODO: [PIPEGEN] Not implemented for bilinear yet.
  if (quality == BL_PATTERN_QUALITY_BILINEAR)
    opt = 0;

  fetchType = FetchType(uint32_t(fetchType) + opt);

  // Pattern X/Y extends.
  uint32_t extendX = extendXFromExtendMode(extendMode);
  uint32_t extendY = extendYFromExtendMode(extendMode);

  // Translation.
  double tx = inv.m20;
  double ty = inv.m21;

  tx += 0.5 * (xx + yx);
  ty += 0.5 * (xy + yy);

  // 32x32 fixed point scale as double, equals to `pow(2, 32)`.
  double fpScale = 4294967296.0;

  // Overflow check of X/Y. When this check passes we decrement rx/ry from the overflown values.
  int ox = BLTraits::maxValue<int32_t>();
  int oy = BLTraits::maxValue<int32_t>();

  // Normalization of X/Y. These values are added to the current `px` and `py` when they overflow the repeat|reflect
  // bounds.
  int rx = 0;
  int ry = 0;

  fetchData.affine.minX = 0;
  fetchData.affine.minY = 0;

  fetchData.affine.maxX = int32_t(tw - 1);
  fetchData.affine.maxY = int32_t(th - 1);

  fetchData.affine.corX = int32_t(tw - 1);
  fetchData.affine.corY = int32_t(th - 1);

  if (extendX != BL_EXTEND_MODE_PAD) {
    fetchData.affine.minX = BLTraits::minValue<int32_t>();
    if (extendX == BL_EXTEND_MODE_REPEAT)
      fetchData.affine.corX = 0;

    ox = tw;
    if (extendX == BL_EXTEND_MODE_REFLECT)
      tw *= 2;

    if (xx < 0.0) {
      xx = -xx;
      yx = -yx;
      tx = double(tw) - tx;

      if (extendX == BL_EXTEND_MODE_REPEAT) {
        ox = 0;
        fetchData.affine.corX = fetchData.affine.maxX;
        fetchData.affine.maxX = -1;
      }
    }
    ox--;
  }

  if (extendY != BL_EXTEND_MODE_PAD) {
    fetchData.affine.minY = BLTraits::minValue<int32_t>();
    if (extendY == BL_EXTEND_MODE_REPEAT)
      fetchData.affine.corY = 0;

    oy = th;
    if (extendY == BL_EXTEND_MODE_REFLECT)
      th *= 2;

    if (xy < 0.0) {
      xy = -xy;
      yy = -yy;
      ty = double(th) - ty;

      if (extendY == BL_EXTEND_MODE_REPEAT) {
        oy = 0;
        fetchData.affine.corY = fetchData.affine.maxY;
        fetchData.affine.maxY = -1;
      }
    }
    oy--;
  }

  // Keep the center of the pixel at [0.5, 0.5] if the filter is NEAREST so it can properly round to the nearest
  // pixel during the fetch phase. However, if the filter is not NEAREST the `tx` and `ty` have to be translated
  // by -0.5 so the position starts at the beginning of the pixel.
  if (quality != BL_PATTERN_QUALITY_NEAREST) {
    tx -= 0.5;
    ty -= 0.5;
  }

  // Pattern boundaries converted to `double`.
  double tw_d = double(tw);
  double th_d = double(th);

  // Normalize the matrix in a way that it won't overflow the pattern more than once per a single iteration. Happens
  // when scaling part is very small. Only useful for repeated / reflected cases.
  if (extendX == BL_EXTEND_MODE_PAD) {
    tw_d = 4294967296.0;
  }
  else {
    tx = fmod(tx, tw_d);
    rx = tw;
    if (xx >= tw_d) xx = fmod(xx, tw_d);
  }

  if (extendY == BL_EXTEND_MODE_PAD) {
    th_d = 4294967296.0;
  }
  else {
    ty = fmod(ty, th_d);
    ry = th;
    if (xy >= th_d) xy = fmod(xy, th_d);
  }

  fetchData.affine.xx.i64 = blFloorToInt64(xx * fpScale);
  fetchData.affine.xy.i64 = blFloorToInt64(xy * fpScale);
  fetchData.affine.yx.i64 = blFloorToInt64(yx * fpScale);
  fetchData.affine.yy.i64 = blFloorToInt64(yy * fpScale);

  fetchData.affine.tx.i64 = blFloorToInt64(tx * fpScale);
  fetchData.affine.ty.i64 = blFloorToInt64(ty * fpScale);
  fetchData.affine.rx.i64 = BLIntOps::shl(int64_t(rx), 32);
  fetchData.affine.ry.i64 = BLIntOps::shl(int64_t(ry), 32);

  fetchData.affine.ox.i32Hi = ox;
  fetchData.affine.ox.i32Lo = BLTraits::maxValue<int32_t>();
  fetchData.affine.oy.i32Hi = oy;
  fetchData.affine.oy.i32Lo = BLTraits::maxValue<int32_t>();

  fetchData.affine.tw = tw_d;
  fetchData.affine.th = th_d;

  fetchData.affine.xx2.u64 = fetchData.affine.xx.u64 << 1u;
  fetchData.affine.xy2.u64 = fetchData.affine.xy.u64 << 1u;

  if (extendX >= BL_EXTEND_MODE_REPEAT && fetchData.affine.xx2.u32Hi >= uint32_t(tw)) fetchData.affine.xx2.u32Hi %= uint32_t(tw);
  if (extendY >= BL_EXTEND_MODE_REPEAT && fetchData.affine.xy2.u32Hi >= uint32_t(th)) fetchData.affine.xy2.u32Hi %= uint32_t(th);

  if (opt) {
    fetchData.affine.addrMul16[0] = int16_t(bytesPerPixel);
    fetchData.affine.addrMul16[1] = int16_t(fetchData.src.stride);
  }
  else {
    fetchData.affine.addrMul32[0] = int32_t(bytesPerPixel);
    fetchData.affine.addrMul32[1] = int32_t(fetchData.src.stride);;
  }

  return Signature::fromFetchType(fetchType);
}

// FetchData - Init Gradient
// =========================

static BL_INLINE Signature initLinearGradient(FetchData::Gradient& fetchData, const BLLinearGradientValues& values, BLExtendMode extendMode, BLGradientQuality quality, const BLMatrix2D& transform) noexcept {
  BL_ASSERT(extendMode <= BL_EXTEND_MODE_SIMPLE_MAX_VALUE);
  BL_ASSERT(fetchData.lut.size > 0u);

  // Inverted transformation matrix.
  BLMatrix2D inv;
  if (BLMatrix2D::invert(inv, transform) != BL_SUCCESS)
    return Signature::fromPendingFlag(1);

  BLPoint p0(values.x0, values.y0);
  BLPoint p1(values.x1, values.y1);

  uint32_t lutSize = fetchData.lut.size;
  uint32_t maxi = extendMode == BL_EXTEND_MODE_REFLECT ? lutSize * 2u - 1u : lutSize - 1u;
  uint32_t rori = extendMode == BL_EXTEND_MODE_REFLECT ? maxi : 0u;

  // Distance between [x0, y0] and [x1, y1], before transform.
  double ax = p1.x - p0.x;
  double ay = p1.y - p0.y;
  double dist = ax * ax + ay * ay;

  // Invert origin and move it to the center of the pixel.
  BLPoint o = BLPoint(0.5, 0.5) - transform.mapPoint(p0);

  double dt = ax * inv.m00 + ay * inv.m01;
  double dy = ax * inv.m10 + ay * inv.m11;

  double scale = double(int64_t(uint64_t(lutSize) << 32)) / dist;
  double offset = o.x * dt + o.y * dy;

  dt *= scale;
  dy *= scale;
  offset *= scale;

  fetchData.linear.dy.i64 = blFloorToInt64(dy);
  fetchData.linear.dt.i64 = blFloorToInt64(dt);
  fetchData.linear.pt[0].i64 = blFloorToInt64(offset);
  fetchData.linear.pt[1].u64 = fetchData.linear.pt[0].u64 + fetchData.linear.dt.u64;

  fetchData.linear.maxi = maxi;
  fetchData.linear.rori = rori;

  FetchType fetchTypeBase =
    quality < BL_GRADIENT_QUALITY_DITHER
      ? FetchType::kGradientLinearNNPad
      : FetchType::kGradientLinearDitherPad;

  return Signature::fromFetchType(FetchType(uint32_t(fetchTypeBase) + uint32_t(extendMode != BL_EXTEND_MODE_PAD)));
}

// The radial gradient uses the following equation:
//
//    b = x * fx + y * fy
//    d = x^2 * (r^2 - fy^2) + y^2 * (r^2 - fx^2) + x*y * (2*fx*fy)
//
//    pos = ((b + sqrt(d))) * scale)
//
// Simplified to:
//
//    C1 = r^2 - fy^2
//    C2 = r^2 - fx^2
//    C3 = 2 * fx * fy
//
//    b = x*fx + y*fy
//    d = x^2 * C1 + y^2 * C2 + x*y * C3
//
//    pos = ((b + sqrt(d))) * scale)
//
// Radial gradient function can be defined as follows:
//
//    D = C1*(x^2) + C2*(y^2) + C3*(x*y)
//
// Which could be rewritten as:
//
//    D = D1 + D2 + D3
//
//    Where: D1 = C1*(x^2)
//           D2 = C2*(y^2)
//           D3 = C3*(x*y)
//
// The variables `x` and `y` increase linearly, thus we can use multiple differentiation to get delta (d) and
// delta-of-delta (dd).
//
// Deltas for `C*(x^2)` at `t`:
//
//   C*x*x: 1st delta `d`  at step `t`: C*(t^2) + 2*C*x
//   C*x*x: 2nd delta `dd` at step `t`: 2*C *t^2
//
//   ( Hint, use Mathematica DifferenceDelta[x*x*C, {x, 1, t}] )
//
// Deltas for `C*(x*y)` at `t`:
//
//   C*x*y: 1st delta `d`  at step `tx/ty`: C*x*ty + C*y*tx + C*tx*ty
//   C*x*y: 2nd delta `dd` at step `tx/ty`: 2*C * tx*ty
static BL_INLINE Signature initRadialGradient(FetchData::Gradient& fetchData, const BLRadialGradientValues& values, BLExtendMode extendMode, BLGradientQuality quality, const BLMatrix2D& transform) noexcept {
  BL_ASSERT(extendMode <= BL_EXTEND_MODE_SIMPLE_MAX_VALUE);
  BL_ASSERT(fetchData.lut.size > 0u);

  // Inverted transformation matrix.
  BLMatrix2D inv;
  if (BLMatrix2D::invert(inv, transform) != BL_SUCCESS)
    return Signature::fromPendingFlag(1);

  BLPoint c(values.x0, values.y0);
  BLPoint f(values.x1, values.y1);
  double r = values.r0;

  uint32_t lutSize = fetchData.lut.size;
  uint32_t maxi = extendMode == BL_EXTEND_MODE_REFLECT ? lutSize * 2u - 1u : lutSize - 1u;
  uint32_t rori = extendMode == BL_EXTEND_MODE_REFLECT ? maxi : 0u;

  BLPoint fOrig = f;
  f -= c;

  double fxfx = f.x * f.x;
  double fyfy = f.y * f.y;

  double rr = r * r;
  double dd = rr - fxfx - fyfy;

  // If the focal point is near the border we move it slightly to prevent division by zero. This idea comes from
  // AntiGrain library.
  if (isNearZero(dd)) {
    if (!isNearZero(f.x)) f.x += (f.x < 0.0) ? 0.5 : -0.5;
    if (!isNearZero(f.y)) f.y += (f.y < 0.0) ? 0.5 : -0.5;

    fxfx = f.x * f.x;
    fyfy = f.y * f.y;
    dd = rr - fxfx - fyfy;
  }

  double scale = double(int(lutSize)) / dd;
  double ax = rr - fyfy;
  double ay = rr - fxfx;

  fetchData.radial.ax = ax;
  fetchData.radial.ay = ay;
  fetchData.radial.fx = f.x;
  fetchData.radial.fy = f.y;

  double xx = inv.m00;
  double xy = inv.m01;
  double yx = inv.m10;
  double yy = inv.m11;

  fetchData.radial.xx = xx;
  fetchData.radial.xy = xy;
  fetchData.radial.yx = yx;
  fetchData.radial.yy = yy;
  fetchData.radial.ox = (inv.m20 - fOrig.x) + 0.5 * (xx + yx);
  fetchData.radial.oy = (inv.m21 - fOrig.y) + 0.5 * (xy + yy);

  double ax_xx = ax * xx;
  double ay_xy = ay * xy;
  double fx_xx = f.x * xx;
  double fy_xy = f.y * xy;

  fetchData.radial.dd = ax_xx * xx + ay_xy * xy + 2.0 * (fx_xx * fy_xy);
  fetchData.radial.bd = fx_xx + fy_xy;

  fetchData.radial.ddx = 2.0 * (ax_xx + fy_xy * f.x);
  fetchData.radial.ddy = 2.0 * (ay_xy + fx_xx * f.y);

  fetchData.radial.ddd = 2.0 * fetchData.radial.dd;
  fetchData.radial.scale = scale;
  fetchData.radial.maxi = maxi;
  fetchData.radial.rori = rori;

  FetchType fetchTypeBase =
    quality < BL_GRADIENT_QUALITY_DITHER
      ? FetchType::kGradientRadialNNPad
      : FetchType::kGradientRadialDitherPad;

  return Signature::fromFetchType(FetchType(uint32_t(fetchTypeBase) + uint32_t(extendMode != BL_EXTEND_MODE_PAD)));
}

static BL_INLINE Signature initConicGradient(FetchData::Gradient& fetchData, const BLConicGradientValues& values, BLExtendMode extendMode, BLGradientQuality quality, const BLMatrix2D& transform) noexcept {
  // TODO: Extend mode is not used.
  blUnused(extendMode);

  BLPoint c(values.x0, values.y0);
  double angle = values.angle;

  uint32_t lutSize = fetchData.lut.size;
  uint32_t tableId = BLIntOps::ctz(lutSize) - 8;
  BL_ASSERT(tableId < BLCommonTable::kTableCount);

  // Invert the origin and move it to the center of the pixel.
  c = BLPoint(0.5, 0.5) - transform.mapPoint(c);

  BLPoint v = transform.mapVector(BLPoint(1.0, 0.0));
  double matrixAngle = atan2(v.y, v.x);
  BLMatrix2D updatedTransform(transform);
  updatedTransform.rotate(-matrixAngle, c);
  fetchData.conic.offset = float(((angle + matrixAngle) / -(2.0 * BL_M_PI)) * lutSize) + lutSize;

  BLMatrix2D inv;
  if (BLMatrix2D::invert(inv, updatedTransform) != BL_SUCCESS)
    return Signature::fromPendingFlag(1);

  fetchData.conic.xx = inv.m00;
  fetchData.conic.yx = inv.m10;
  fetchData.conic.yy = inv.m11;
  fetchData.conic.ox = c.x * inv.m00 + c.y * inv.m10;
  fetchData.conic.oy = c.x * inv.m01 + c.y * inv.m11;
  fetchData.conic.consts = &blCommonTable.xmm_f_con[tableId];
  fetchData.conic.maxi = int(lutSize - 1);

  FetchType fetchType =
    quality < BL_GRADIENT_QUALITY_DITHER
      ? FetchType::kGradientConicNN
      : FetchType::kGradientConicDither;

  return Signature::fromFetchType(fetchType);
}

Signature initGradient(
  FetchData::Gradient& fetchData,
  BLGradientType gradientType,
  BLExtendMode extendMode,
  BLGradientQuality quality,
  const void* values, const void* lutData, uint32_t lutSize, const BLMatrix2D& transform) noexcept {

  // Initialize LUT.
  fetchData.lut.data = lutData;
  fetchData.lut.size = lutSize;

  // Initialize gradient by type.
  switch (gradientType) {
    case BL_GRADIENT_TYPE_LINEAR:
      return initLinearGradient(fetchData, *static_cast<const BLLinearGradientValues*>(values), extendMode, quality, transform);

    case BL_GRADIENT_TYPE_RADIAL:
      return initRadialGradient(fetchData, *static_cast<const BLRadialGradientValues*>(values), extendMode, quality, transform);

    case BL_GRADIENT_TYPE_CONIC:
      return initConicGradient(fetchData, *static_cast<const BLConicGradientValues*>(values), extendMode, quality, transform);

    default:
      // Should not happen, but be defensive.
      return Signature::fromPendingFlag(1);
  }
}

} // {FetchUtils}
} // {BLPipeline}
