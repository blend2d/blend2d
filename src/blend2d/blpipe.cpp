// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blcontext.h"
#include "./blmath_p.h"
#include "./blmatrix_p.h"
#include "./blpipe_p.h"
#include "./blsupport_p.h"

// ============================================================================
// [BLPipeFetchData - Init Pattern]
// ============================================================================

static BL_INLINE uint32_t blExtendXFromExtendMode(uint32_t extendMode) noexcept {
  BL_ASSERT(extendMode < BL_EXTEND_MODE_COMPLEX_COUNT);

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

static BL_INLINE uint32_t blExtendYFromExtendMode(uint32_t extendMode) noexcept {
  BL_ASSERT(extendMode < BL_EXTEND_MODE_COMPLEX_COUNT);

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

static BL_INLINE uint32_t blPipeFetchDataInitPatternTxTy(BLPipeFetchData* fetchData, uint32_t fetchBase, uint32_t extendMode, int tx, int ty, bool isFractional) noexcept {
  BLPipeFetchData::Pattern& d = fetchData->pattern;
  uint32_t extendX = blExtendXFromExtendMode(extendMode);
  uint32_t extendY = blExtendYFromExtendMode(extendMode);
  uint32_t ixIndex = 17;

  int rx = 0;
  int ry = 0;

  // If the pattern width/height is 1 all extend modes produce the same output.
  // However, it's safer to just set it to PAD as FetchPatternPart requires
  // `width` to be equal or greater than 2 if the extend mode is REPEAT or
  // REFLECT.
  if (d.src.size.w <= 1) extendX = BL_EXTEND_MODE_PAD;
  if (d.src.size.h <= 1) extendY = BL_EXTEND_MODE_PAD;

  if (extendX >= BL_EXTEND_MODE_REPEAT) {
    bool isReflect = extendX == BL_EXTEND_MODE_REFLECT;

    rx = int(d.src.size.w) << uint32_t(isReflect);
    if (unsigned(tx) >= unsigned(rx))
      tx %= rx;
    if (tx < 0)
      tx += rx;

    // In extreme cases, when `rx` is very small, fetch4()/fetch8() functions
    // may overflow `x` if they increment more than they can fix by subtracting
    // `rw` in case of overflow (and overflow happens as it's used to start
    // over). To fix this and simplify the compiled code we simply precalculate
    // these constants so they are always safe.
    ixIndex = blMin<uint32_t>(uint32_t(rx), 17);

    // Don't specialize `Repeat vs Reflect` when we are not pixel aligned.
    if (isFractional)
      extendX = 1; // TODO: Naming...
  }

  if (extendY >= BL_EXTEND_MODE_REPEAT) {
    ry = int(d.src.size.h) << uint32_t(extendY == BL_EXTEND_MODE_REFLECT);
    if (unsigned(ty) >= unsigned(ry))
      ty %= ry;
    if (ty < 0)
      ty += ry;
  }

  d.simple.tx = tx;
  d.simple.ty = ty;
  d.simple.rx = rx;
  d.simple.ry = ry;
  d.simple.ix = blModuloTable[ixIndex];

  return fetchBase + extendX;
}

uint32_t BLPipeFetchData::initPatternAxAy(uint32_t extendMode, int x, int y) noexcept {
  return blPipeFetchDataInitPatternTxTy(this, BL_PIPE_FETCH_TYPE_PATTERN_AA_PAD, extendMode, -x, -y, false);
}

uint32_t BLPipeFetchData::initPatternFxFy(uint32_t extendMode, uint32_t filter, int64_t tx64, int64_t ty64) noexcept {
  BLPipeFetchData::Pattern& d = this->pattern;

  uint32_t fetchBase = BL_PIPE_FETCH_TYPE_PATTERN_AA_PAD;
  uint32_t wx = uint32_t(tx64 & 0xFF);
  uint32_t wy = uint32_t(ty64 & 0xFF);

  int tx = -int(((tx64)) >> 8);
  int ty = -int(((ty64)) >> 8);

  // If one or both `wx` or `why` are non-zero it means that the translation
  // is fractional. In that case we must calculate weights of [x0 y0], [x1 y0],
  // [x0 y1], and [x1 y1] pixels.
  bool isFractional = (wx | wy) != 0;
  if (isFractional) {
    if (filter == BL_PATTERN_QUALITY_NEAREST) {
      tx -= (wx >= 128);
      ty -= (wy >= 128);
      isFractional = false;
    }
    else {
      d.simple.wa = ((      wy) * (      wx)      ) >> 8; // [x0 y0]
      d.simple.wb = ((      wy) * (256 - wx) + 255) >> 8; // [x1 y0]
      d.simple.wc = ((256 - wy) * (      wx)      ) >> 8; // [x0 y1]
      d.simple.wd = ((256 - wy) * (256 - wx) + 255) >> 8; // [x1 y1]

      // The FxFy fetcher must work even when one or both `wx` or `wy` are
      // zero, so we always decrement `tx` and `ty` based on the fetch type.
      if (wy == 0) {
        tx--;
        fetchBase = BL_PIPE_FETCH_TYPE_PATTERN_FX_PAD;
      }
      else if (wx == 0) {
        ty--;
        fetchBase = BL_PIPE_FETCH_TYPE_PATTERN_FY_PAD;
      }
      else {
        tx--;
        ty--;
        fetchBase = BL_PIPE_FETCH_TYPE_PATTERN_FX_FY_PAD;
      }
    }
  }

  return blPipeFetchDataInitPatternTxTy(this, fetchBase, extendMode, tx, ty, isFractional);
}

uint32_t BLPipeFetchData::initPatternAffine(uint32_t extendMode, uint32_t filter, const BLMatrix2D& m, const BLMatrix2D& mInv) noexcept {
  BLPipeFetchData::Pattern& d = this->pattern;

  // Inverted transformation matrix.
  double xx = mInv.m00;
  double xy = mInv.m01;
  double yx = mInv.m10;
  double yy = mInv.m11;

  if (isNearOne(xx) && isNearZero(xy) && isNearZero(yx) && isNearOne(yy)) {
    return initPatternFxFy(
      extendMode,
      filter,
      blFloorToInt64(-mInv.m20 * 256.0),
      blFloorToInt64(-mInv.m21 * 256.0));
  }

  uint32_t fetchType =
    filter == BL_PATTERN_QUALITY_NEAREST
      ? BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_NN_ANY
      : BL_PIPE_FETCH_TYPE_PATTERN_AFFINE_BI_ANY;

  // Pattern bounds.
  int tw = int(d.src.size.w);
  int th = int(d.src.size.h);

  uint32_t opt = blMax(tw, th) < 32767 &&
                 d.src.stride >= 0 &&
                 d.src.stride <= intptr_t(blMaxValue<int16_t>());

  // TODO: [PIPEGEN] Not implemented for bilinear yet.
  if (filter == BL_PATTERN_QUALITY_BILINEAR)
    opt = 0;

  fetchType += opt;

  // Pattern X/Y extends.
  uint32_t extendX = blExtendXFromExtendMode(extendMode);
  uint32_t extendY = blExtendYFromExtendMode(extendMode);

  // Translation.
  double tx = mInv.m20;
  double ty = mInv.m21;

  tx += 0.5 * (xx + yx);
  ty += 0.5 * (xy + yy);

  // 32x32 fixed point scale as double, equals to `pow(2, 32)`.
  double fpScale = 4294967296.0;

  // Overflow check of X/Y. When this check passes we decrement rx/ry from
  // the overflown values.
  int ox = blMaxValue<int32_t>();
  int oy = blMaxValue<int32_t>();

  // Normalization of X/Y. These values are added to the current `px` and `py`
  // when they overflow the repeat|reflect bounds.
  int rx = 0;
  int ry = 0;

  d.affine.minX = 0;
  d.affine.minY = 0;

  d.affine.maxX = int32_t(tw - 1);
  d.affine.maxY = int32_t(th - 1);

  d.affine.corX = int32_t(tw - 1);
  d.affine.corY = int32_t(th - 1);

  if (extendX != BL_EXTEND_MODE_PAD) {
    d.affine.minX = blMinValue<int32_t>();
    if (extendX == BL_EXTEND_MODE_REPEAT)
      d.affine.corX = 0;

    ox = tw;
    if (extendX == BL_EXTEND_MODE_REFLECT)
      tw *= 2;

    if (xx < 0.0) {
      xx = -xx;
      yx = -yx;
      tx = double(tw) - tx;

      if (extendX == BL_EXTEND_MODE_REPEAT) {
        ox = 0;
        d.affine.corY = d.affine.maxX;
      }
    }
    ox--;
  }

  if (extendY != BL_EXTEND_MODE_PAD) {
    d.affine.minY = blMinValue<int32_t>();
    if (extendY == BL_EXTEND_MODE_REPEAT)
      d.affine.corY = 0;

    oy = th;
    if (extendY == BL_EXTEND_MODE_REFLECT)
      th *= 2;

    if (xy < 0.0) {
      xy = -xy;
      yy = -yy;
      ty = double(th) - ty;

      if (extendY == BL_EXTEND_MODE_REPEAT) {
        oy = 0;
        d.affine.corY = d.affine.maxY;
      }
    }
    oy--;
  }

  // Keep the center of the pixel at [0.5, 0.5] if the filter is NEAREST so
  // it can properly round to the nearest pixel during the fetch phase.
  // However, if the filter is not NEAREST the `tx` and `ty` have to be
  // translated by -0.5 so the position starts at the beginning of the pixel.
  if (filter != BL_PATTERN_QUALITY_NEAREST) {
    tx -= 0.5;
    ty -= 0.5;
  }

  // Pattern boundaries converted to `double`.
  double tw_d = double(tw);
  double th_d = double(th);

  // Normalize the matrix in a way that it won't overflow the pattern more
  // than once per a single iteration. Happens when scaling part is very
  // small. Only useful for repeated / reflected cases.
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

  d.affine.xx.i64 = blFloorToInt64(xx * fpScale);
  d.affine.xy.i64 = blFloorToInt64(xy * fpScale);
  d.affine.yx.i64 = blFloorToInt64(yx * fpScale);
  d.affine.yy.i64 = blFloorToInt64(yy * fpScale);

  d.affine.tx.i64 = blFloorToInt64(tx * fpScale);
  d.affine.ty.i64 = blFloorToInt64(ty * fpScale);
  d.affine.rx.i64 = blBitShl(int64_t(rx), 32);
  d.affine.ry.i64 = blBitShl(int64_t(ry), 32);

  d.affine.ox.i32Hi = ox;
  d.affine.ox.i32Lo = blMaxValue<int32_t>();
  d.affine.oy.i32Hi = oy;
  d.affine.oy.i32Lo = blMaxValue<int32_t>();

  d.affine.tw = tw_d;
  d.affine.th = th_d;

  d.affine.xx2.u64 = d.affine.xx.u64 << 1u;
  d.affine.xy2.u64 = d.affine.xy.u64 << 1u;

  if (extendX >= BL_EXTEND_MODE_REPEAT && d.affine.xx2.u32Hi >= uint32_t(tw)) d.affine.xx2.u32Hi %= uint32_t(tw);
  if (extendY >= BL_EXTEND_MODE_REPEAT && d.affine.xy2.u32Hi >= uint32_t(th)) d.affine.xy2.u32Hi %= uint32_t(th);

  // TODO: Hardcoded for 32-bit PRGB/XRGB formats.
  if (opt) {
    d.affine.addrMul[0] = 4;
    d.affine.addrMul[1] = int16_t(d.src.stride);
  }
  else {
    d.affine.addrMul[0] = 0;
    d.affine.addrMul[1] = 0;
  }

  return fetchType;
}

// ============================================================================
// [BLPipeFetchData - Init Gradient]
// ============================================================================

static BL_INLINE uint32_t blPipeFetchDataInitLinearGradient(BLPipeFetchData* fetchData, const BLLinearGradientValues& values, uint32_t extendMode, const BLMatrix2D& m, const BLMatrix2D& mInv) noexcept {
  BLPipeFetchData::Gradient& d = fetchData->gradient;

  BLPoint p0(values.x0, values.y0);
  BLPoint p1(values.x1, values.y1);

  uint32_t lutSize = d.lut.size;
  BL_ASSERT(lutSize > 0);

  bool isPad     = extendMode == BL_EXTEND_MODE_PAD;
  bool isReflect = extendMode == BL_EXTEND_MODE_REFLECT;

  // Distance between [x0, y0] and [x1, y1], before transform.
  double ax = p1.x - p0.x;
  double ay = p1.y - p0.y;
  double dist = ax * ax + ay * ay;

  // Invert origin and move it to the center of the pixel.
  BLPoint o = BLPoint(0.5, 0.5) - m.mapPoint(p0);

  double dt = ax * mInv.m00 + ay * mInv.m01;
  double dy = ax * mInv.m10 + ay * mInv.m11;

  double scale = double(int64_t(uint64_t(lutSize) << 32)) / dist;
  double offset = o.x * dt + o.y * dy;

  dt *= scale;
  dy *= scale;
  offset *= scale;

  d.linear.dy.i64 = blFloorToInt64(dy);
  d.linear.dt.i64 = blFloorToInt64(dt);
  d.linear.dt2.u64 = d.linear.dt.u64 << 1;
  d.linear.pt[0].i64 = blFloorToInt64(offset);
  d.linear.pt[1].u64 = d.linear.pt[0].u64 + d.linear.dt.u64;

  uint32_t rorSize = isReflect ? lutSize * 2u : lutSize;
  d.linear.rep.u32Hi = isPad ? uint32_t(0xFFFFFFFFu) : uint32_t(rorSize - 1u);
  d.linear.rep.u32Lo = 0xFFFFFFFFu;
  d.linear.msk.u     = isPad ? (lutSize - 1u) * 0x00010001u : (lutSize * 2u - 1u) * 0x00010001u;

  return isPad ? BL_PIPE_FETCH_TYPE_GRADIENT_LINEAR_PAD : BL_PIPE_FETCH_TYPE_GRADIENT_LINEAR_ROR;
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
// The variables `x` and `y` increase linearly, thus we can use multiple
// differentiation to get delta (d) and delta-of-delta (dd).
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
static BL_INLINE uint32_t blPipeFetchDataInitRadialGradient(BLPipeFetchData* fetchData, const BLRadialGradientValues& values, uint32_t extendMode, const BLMatrix2D& m, const BLMatrix2D& mInv) noexcept {
  BLPipeFetchData::Gradient& d = fetchData->gradient;

  BLPoint c(values.x0, values.y0);
  BLPoint f(values.x1, values.y1);

  double r = values.r0;
  uint32_t lutSize = d.lut.size;

  BL_ASSERT(lutSize != 0);
  BL_ASSERT(extendMode < BL_EXTEND_MODE_SIMPLE_COUNT);

  BLPoint fOrig = f;
  f -= c;

  double fxfx = f.x * f.x;
  double fyfy = f.y * f.y;

  double rr = r * r;
  double dd = rr - fxfx - fyfy;

  // If the focal point is near the border we move it slightly to prevent
  // division by zero. This idea comes from AntiGrain library.
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

  d.radial.ax = ax;
  d.radial.ay = ay;
  d.radial.fx = f.x;
  d.radial.fy = f.y;

  double xx = mInv.m00;
  double xy = mInv.m01;
  double yx = mInv.m10;
  double yy = mInv.m11;

  d.radial.xx = xx;
  d.radial.xy = xy;
  d.radial.yx = yx;
  d.radial.yy = yy;
  d.radial.ox = (mInv.m20 - fOrig.x) + 0.5 * (xx + yx);
  d.radial.oy = (mInv.m21 - fOrig.y) + 0.5 * (xy + yy);

  double ax_xx = ax * xx;
  double ay_xy = ay * xy;
  double fx_xx = f.x * xx;
  double fy_xy = f.y * xy;

  d.radial.dd = ax_xx * xx + ay_xy * xy + 2.0 * (fx_xx * fy_xy);
  d.radial.bd = fx_xx + fy_xy;

  d.radial.ddx = 2.0 * (ax_xx + fy_xy * f.x);
  d.radial.ddy = 2.0 * (ay_xy + fx_xx * f.y);

  d.radial.ddd = 2.0 * d.radial.dd;
  d.radial.scale = scale;
  d.radial.maxi = (extendMode == BL_EXTEND_MODE_REFLECT) ? int(lutSize * 2 - 1) : int(lutSize - 1);

  return BL_PIPE_FETCH_TYPE_GRADIENT_RADIAL_PAD + extendMode;
}

static BL_INLINE uint32_t blPipeFetchDataInitConicalGradient(BLPipeFetchData* fetchData, const BLConicalGradientValues& values, uint32_t extendMode, const BLMatrix2D& m, const BLMatrix2D& mInv) noexcept {
  BLPipeFetchData::Gradient& d = fetchData->gradient;

  BLPoint c(values.x0, values.y0);
  double angle = values.angle;

  uint32_t lutSize = d.lut.size;
  uint32_t tableId = blBitCtz(lutSize) - 8;
  BL_ASSERT(tableId < BLCommonTable::kTableCount);

  // Invert the origin and move it to the center of the pixel.
  c = BLPoint(0.5, 0.5) - m.mapPoint(c);

  d.conical.xx = mInv.m00;
  d.conical.xy = mInv.m01;
  d.conical.yx = mInv.m10;
  d.conical.yy = mInv.m11;
  d.conical.ox = mInv.m20 + c.x * mInv.m00 + c.y * mInv.m10;
  d.conical.oy = mInv.m21 + c.x * mInv.m01 + c.y * mInv.m11;
  d.conical.consts = &blCommonTable.xmm_f_con[tableId];

  d.conical.maxi = int(lutSize - 1);

  return BL_PIPE_FETCH_TYPE_GRADIENT_CONICAL;
}

uint32_t BLPipeFetchData::initGradient(uint32_t gradientType, const void* values, uint32_t extendMode, const BLGradientLUT* lut, const BLMatrix2D& m, const BLMatrix2D& mInv) noexcept {
  // Initialize LUT.
  this->gradient.lut.data = lut->data();
  this->gradient.lut.size = uint32_t(lut->size);

  // Initialize gradient by type.
  switch (gradientType) {
    case BL_GRADIENT_TYPE_LINEAR: return blPipeFetchDataInitLinearGradient(this, *static_cast<const BLLinearGradientValues*>(values), extendMode, m, mInv);
    case BL_GRADIENT_TYPE_RADIAL: return blPipeFetchDataInitRadialGradient(this, *static_cast<const BLRadialGradientValues*>(values), extendMode, m, mInv);
    case BL_GRADIENT_TYPE_CONICAL: return blPipeFetchDataInitConicalGradient(this, *static_cast<const BLConicalGradientValues*>(values), extendMode, m, mInv);

    default:
      BL_NOT_REACHED();
  }
}
