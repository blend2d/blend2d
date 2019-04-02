// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blimagescale_p.h"
#include "./blmath_p.h"
#include "./blformat_p.h"
#include "./blgeometry_p.h"
#include "./blrgba_p.h"
#include "./blruntime_p.h"
#include "./blsupport_p.h"

// ============================================================================
// [BLImageScale - Global Variables]
// ============================================================================

static constexpr const BLImageScaleOptions blImageScaleOptionsNone = {
  nullptr,   // UserFunc.
  nullptr,   // UserData.
  2.0,       // Radius.
  {{
    1.0 / 3.0, // Mitchell B.
    1.0 / 3.0, // Mitchell C.
    0.0        // Reserved.
  }}
};

// ============================================================================
// [BLImageScale - Ops]
// ============================================================================

struct BLImageScaleOps {
  BLResult (BL_CDECL* weights)(BLImageScaleContext::Data* d, uint32_t dir, BLImageScaleUserFunc func, const void* data) BL_NOEXCEPT;
  void (BL_CDECL* horz[BL_FORMAT_COUNT])(const BLImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) BL_NOEXCEPT;
  void (BL_CDECL* vert[BL_FORMAT_COUNT])(const BLImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) BL_NOEXCEPT;
};
static BLImageScaleOps blImageScaleOps;

// ============================================================================
// [BLImageScale - BuiltInParams]
// ============================================================================

// Data needed by some functions that take additional parameters.
struct BLImageScaleBuiltInParams {
  double radius;

  struct Mitchell {
    double p0, p2, p3;
    double q0, q1, q2, q3;
  } mitchell;

  BL_INLINE void initMitchell(double b, double c) noexcept {
    constexpr double k1Div3 = 1.0 / 3.0;
    constexpr double k1Div6 = 1.0 / 6.0;
    constexpr double k4Div3 = 4.0 / 3.0;

    mitchell.p0 =  1.0 - k1Div3 * b;
    mitchell.p2 = -3.0 + 2.0    * b + c;
    mitchell.p3 =  2.0 - 1.5    * b - c;

    mitchell.q0 =  k4Div3       * b + c * 4.0;
    mitchell.q1 = -2.0          * b - c * 8.0;
    mitchell.q2 =                 b + c * 5.0;
    mitchell.q3 = -k1Div6       * b - c;
  }
};

// ============================================================================
// [BLImageScale - Utilities]
// ============================================================================

// Calculates a Bessel function of first kind of order `n`.
//
// Adapted for use in AGG library by Andy Wilk <castor.vulgaris@gmail.com>
static BL_INLINE double blBessel(double x, int n) noexcept {
  double d = 1e-6;
  double b0 = 0.0;
  double b1 = 0.0;

  if (blAbs(x) <= d)
    return n != 0 ? 0.0 : 1.0;

  // Set up a starting order for recurrence.
  int m1 = blAbs(x) > 5.0 ? int(blAbs(1.4 * x + 60.0 / x)) : int(blAbs(x) + 6);
  int m2 = blMax(int(blAbs(x)) / 4 + 2 + n, m1);

  for (;;) {
    double c2 = blEpsilon<double>();
    double c3 = 0.0;
    double c4 = 0.0;

    int m8 = m2 & 1;
    for (int i = 1, iEnd = m2 - 1; i < iEnd; i++) {
      double c6 = 2 * (m2 - i) * c2 / x - c3;
      c3 = c2;
      c2 = c6;

      if (m2 - i - 1 == n)
        b1 = c6;

      m8 ^= 1;
      if (m8)
        c4 += c6 * 2.0;
    }

    double c6 = 2.0 * c2 / x - c3;
    if (n == 0)
      b1 = c6;

    c4 += c6;
    b1 /= c4;

    if (blAbs(b1 - b0) < d)
      return b1;

    b0 = b1;
    m2 += 3;
  }
}

static BL_INLINE double blSinXDivX(double x) noexcept {
  return blSin(x) / x;
}

static BL_INLINE double blLanczos(double x, double y) noexcept {
  return blSinXDivX(x) * blSinXDivX(y);
}

static BL_INLINE double blBlackman(double x, double y) noexcept {
  return blSinXDivX(x) * (0.42 + 0.5 * blCos(y) + 0.08 * blCos(y * 2.0));
}

// ============================================================================
// [BLImageScale - Functions]
// ============================================================================

static BLResult BL_CDECL blImageScaleNearestFunc(double* dst, const double* tArray, size_t n, const void* data) noexcept {
  BL_UNUSED(data);

  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t <= 0.5 ? 1.0 : 0.0;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blImageScaleBilinearFunc(double* dst, const double* tArray, size_t n, const void* data) noexcept {
  BL_UNUSED(data);

  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t < 1.0 ? 1.0 - t : 0.0;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blImageScaleBicubicFunc(double* dst, const double* tArray, size_t n, const void* data) noexcept {
  BL_UNUSED(data);
  constexpr double k2Div3 = 2.0 / 3.0;

  // 0.5t^3 - t^2 + 2/3 == (0.5t - 1.0) t^2 + 2/3
  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t < 1.0 ? (t * 0.5 - 1.0) * blSquare(t) + k2Div3 :
             t < 2.0 ? blPow3(2.0 - t) / 6.0 : 0.0;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blImageScaleBellFunc(double* dst, const double* tArray, size_t n, const void* data) noexcept {
  BL_UNUSED(data);

  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t < 0.5 ? 0.75 - blSquare(t) :
             t < 1.5 ? 0.50 * blSquare(t - 1.5) : 0.0;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blImageScaleGaussFunc(double* dst, const double* tArray, size_t n, const void* data) noexcept {
  BL_UNUSED(data);
  constexpr double x = 0.7978845608; // sqrt(2 / PI);

  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t <= 2.0 ? exp(blSquare(t) * -2.0) * x : 0.0;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blImageScaleHermiteFunc(double* dst, const double* tArray, size_t n, const void* data) noexcept {
  BL_UNUSED(data);

  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t < 1.0 ? (2.0 * t - 3.0) * blSquare(t) + 1.0 : 0.0;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blImageScaleHanningFunc(double* dst, const double* tArray, size_t n, const void* data) noexcept {
  BL_UNUSED(data);

  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t <= 1.0 ? 0.5 + 0.5 * blCos(t * BL_MATH_PI) : 0.0;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blImageScaleCatromFunc(double* dst, const double* tArray, size_t n, const void* data) noexcept {
  BL_UNUSED(data);

  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t < 1.0 ? 0.5 * (2.0 + t * t * (t * 3.0 - 5.0)) :
             t < 2.0 ? 0.5 * (4.0 + t * (t * (5.0 - t) - 8.0)) : 0.0;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blImageScaleBesselFunc(double* dst, const double* tArray, size_t n, const void* data) noexcept {
  BL_UNUSED(data);
  constexpr double x = BL_MATH_PI * 0.25;

  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t == 0.0 ? x : t <= 3.2383 ? blBessel(t * BL_MATH_PI, 1) / (2.0 * t) : 0.0;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blImageScaleSincFunc(double* dst, const double* tArray, size_t n, const void* data) noexcept {
  const double r = static_cast<const BLImageScaleBuiltInParams*>(data)->radius;

  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t == 0.0 ? 1.0 : t <= r ? blSinXDivX(t * BL_MATH_PI) : 0.0;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blImageScaleLanczosFunc(double* dst, const double* tArray, size_t n, const void* data) noexcept {
  const double r = static_cast<const BLImageScaleBuiltInParams*>(data)->radius;
  const double x = BL_MATH_PI;
  const double y = BL_MATH_PI / r;

  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t == 0.0 ? 1.0 : t <= r ? blLanczos(t * x, t * y) : 0.0;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blImageScaleBlackmanFunc(double* dst, const double* tArray, size_t n, const void* data) noexcept {
  const double r = static_cast<const BLImageScaleBuiltInParams*>(data)->radius;
  const double x = BL_MATH_PI;
  const double y = BL_MATH_PI / r;

  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t == 0.0 ? 1.0 : t <= r ? blBlackman(t * x, t * y) : 0.0;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blImageScaleMitchellFunc(double* dst, const double* tArray, size_t n, const void* data) noexcept {
  const BLImageScaleBuiltInParams::Mitchell& p = static_cast<const BLImageScaleBuiltInParams*>(data)->mitchell;

  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t < 1.0 ? p.p0 + blSquare(t) * (p.p2 + t * p.p3) :
             t < 2.0 ? p.q0 + t         * (p.q1 + t * (p.q2 + t * p.q3)) : 0.0;
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLImageScale - Weights]
// ============================================================================

static BLResult BL_CDECL blImageScaleWeights(BLImageScaleContext::Data* d, uint32_t dir, BLImageScaleUserFunc userFunc, const void* userData) noexcept {
  int32_t* weightList = d->weightList[dir];
  BLImageScaleContext::Record* recordList = d->recordList[dir];

  int dstSize = d->dstSize[dir];
  int srcSize = d->srcSize[dir];
  int kernelSize = d->kernelSize[dir];

  double radius = d->radius[dir];
  double factor = d->factor[dir];
  double scale = d->scale[dir];
  int32_t isUnbound = 0;

  BLMemBufferTmp<512> wMem;
  double* wData = static_cast<double*>(wMem.alloc(kernelSize * sizeof(double)));

  if (BL_UNLIKELY(!wData))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  for (int i = 0; i < dstSize; i++) {
    double wPos = (double(i) + 0.5) / scale - 0.5;
    double wSum = 0.0;

    int left = int(wPos - radius);
    int right = left + kernelSize;
    int wIndex;

    // Calculate all weights for the destination pixel.
    wPos -= left;
    for (wIndex = 0; wIndex < kernelSize; wIndex++, wPos -= 1.0) {
      wData[wIndex] = blAbs(wPos * factor);
    }

    // User function can fail.
    BL_PROPAGATE(userFunc(wData, wData, kernelSize, userData));

    // Remove padded pixels from left and right.
    wIndex = 0;
    while (left < 0) {
      double w = wData[wIndex];
      wData[++wIndex] += w;
      left++;
    }

    int wCount = kernelSize;
    while (right > srcSize) {
      BL_ASSERT(wCount > 0);
      double w = wData[--wCount];
      wData[wCount - 1] += w;
      right--;
    }

    recordList[i].pos = 0;
    recordList[i].count = 0;

    if (wIndex < wCount) {
      // Sum all weights.
      int j;

      for (j = wIndex; j < wCount; j++) {
        double w = wData[j];
        wSum += w;
      }

      int iStrongest = 0;
      int32_t iSum = 0;
      int32_t iMax = 0;

      double wScale = 65535 / wSum;
      for (j = wIndex; j < wCount; j++) {
        int32_t w = int32_t(wData[j] * wScale) >> 8;

        // Remove zero weight from the beginning of the list.
        if (w == 0 && wIndex == j) {
          wIndex++;
          left++;
          continue;
        }

        weightList[j - wIndex] = w;
        iSum += w;
        isUnbound |= w;

        if (iMax < w) {
          iMax = w;
          iStrongest = j - wIndex;
        }
      }

      // Normalize the strongest weight so the sum matches `0x100`.
      if (iSum != 0x100)
        weightList[iStrongest] += int32_t(0x100) - iSum;

      // `wCount` is now absolute size of weights in `weightList`.
      wCount -= wIndex;

      // Remove all zero weights from the end of the weight array.
      while (wCount > 0 && weightList[wCount - 1] == 0)
        wCount--;

      if (wCount) {
        BL_ASSERT(left >= 0);
        recordList[i].pos = left;
        recordList[i].count = wCount;
      }
    }

    weightList += kernelSize;
  }

  d->isUnbound[dir] = isUnbound < 0;
  return BL_SUCCESS;
}

// ============================================================================
// [BLImageScale - Horz]
// ============================================================================

static void BL_CDECL blImageScaleHorzPrgb32(const BLImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) noexcept {
  int dw = d->dstSize[0];
  int sh = d->srcSize[1];
  int kernelSize = d->kernelSize[0];

  if (!d->isUnbound[BLImageScaleContext::kDirHorz]) {
    for (int y = 0; y < sh; y++) {
      const BLImageScaleContext::Record* recordList = d->recordList[BLImageScaleContext::kDirHorz];
      const int32_t* weightList = d->weightList[BLImageScaleContext::kDirHorz];

      uint8_t* dp = dstLine;

      for (int x = 0; x < dw; x++) {
        const uint8_t* sp = srcLine + recordList->pos * 4;
        const int32_t* wp = weightList;

        uint32_t cr_cb = 0x00800080;
        uint32_t ca_cg = 0x00800080;

        for (int i = recordList->count; i; i--) {
          uint32_t p0 = reinterpret_cast<const uint32_t*>(sp)[0];
          uint32_t w0 = wp[0];

          ca_cg += ((p0 >> 8) & 0x00FF00FF) * w0;
          cr_cb += ((p0     ) & 0x00FF00FF) * w0;

          sp += 4;
          wp += 1;
        }
        reinterpret_cast<uint32_t*>(dp)[0] = (ca_cg & 0xFF00FF00) + ((cr_cb & 0xFF00FF00) >> 8);

        recordList += 1;
        weightList += kernelSize;

        dp += 4;
      }

      dstLine += dstStride;
      srcLine += srcStride;
    }
  }
  else {
    for (int y = 0; y < sh; y++) {
      const BLImageScaleContext::Record* recordList = d->recordList[BLImageScaleContext::kDirHorz];
      const int32_t* weightList = d->weightList[BLImageScaleContext::kDirHorz];

      uint8_t* dp = dstLine;

      for (int x = 0; x < dw; x++) {
        const uint8_t* sp = srcLine + recordList->pos * 4;
        const int32_t* wp = weightList;

        int32_t ca = 0x80;
        int32_t cr = 0x80;
        int32_t cg = 0x80;
        int32_t cb = 0x80;

        for (int i = recordList->count; i; i--) {
          uint32_t p0 = reinterpret_cast<const uint32_t*>(sp)[0];
          int32_t w0 = wp[0];

          ca += int32_t((p0 >> 24)       ) * w0;
          cr += int32_t((p0 >> 16) & 0xFF) * w0;
          cg += int32_t((p0 >>  8) & 0xFF) * w0;
          cb += int32_t((p0      ) & 0xFF) * w0;

          sp += 4;
          wp += 1;
        }

        ca = blClamp<int32_t>(ca >> 8, 0, 255);
        cr = blClamp<int32_t>(cr >> 8, 0, ca);
        cg = blClamp<int32_t>(cg >> 8, 0, ca);
        cb = blClamp<int32_t>(cb >> 8, 0, ca);
        reinterpret_cast<uint32_t*>(dp)[0] = blRgba32Pack(cr, cg, cb, ca);

        recordList += 1;
        weightList += kernelSize;

        dp += 4;
      }

      dstLine += dstStride;
      srcLine += srcStride;
    }
  }
}

static void BL_CDECL blImageScaleHorzXrgb32(const BLImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) noexcept {
  int dw = d->dstSize[0];
  int sh = d->srcSize[1];
  int kernelSize = d->kernelSize[0];

  if (!d->isUnbound[BLImageScaleContext::kDirHorz]) {
    for (int y = 0; y < sh; y++) {
      const BLImageScaleContext::Record* recordList = d->recordList[BLImageScaleContext::kDirHorz];
      const int32_t* weightList = d->weightList[BLImageScaleContext::kDirHorz];

      uint8_t* dp = dstLine;

      for (int x = 0; x < dw; x++) {
        const uint8_t* sp = srcLine + recordList->pos * 4;
        const int32_t* wp = weightList;

        uint32_t cx_cg = 0x00008000;
        uint32_t cr_cb = 0x00800080;

        for (int i = recordList->count; i; i--) {
          uint32_t p0 = reinterpret_cast<const uint32_t*>(sp)[0];
          uint32_t w0 = wp[0];

          cx_cg += (p0 & 0x0000FF00) * w0;
          cr_cb += (p0 & 0x00FF00FF) * w0;

          sp += 4;
          wp += 1;
        }

        reinterpret_cast<uint32_t*>(dp)[0] = 0xFF000000 + (((cx_cg & 0x00FF0000) | (cr_cb & 0xFF00FF00)) >> 8);

        recordList += 1;
        weightList += kernelSize;

        dp += 4;
      }

      dstLine += dstStride;
      srcLine += srcStride;
    }
  }
  else {
    for (int y = 0; y < sh; y++) {
      const BLImageScaleContext::Record* recordList = d->recordList[BLImageScaleContext::kDirHorz];
      const int32_t* weightList = d->weightList[BLImageScaleContext::kDirHorz];

      uint8_t* dp = dstLine;

      for (int x = 0; x < dw; x++) {
        const uint8_t* sp = srcLine + recordList->pos * 4;
        const int32_t* wp = weightList;

        int32_t cr = 0x80;
        int32_t cg = 0x80;
        int32_t cb = 0x80;

        for (int i = recordList->count; i; i--) {
          uint32_t p0 = reinterpret_cast<const uint32_t*>(sp)[0];
          int32_t w0 = wp[0];

          cr += int32_t((p0 >> 16) & 0xFF) * w0;
          cg += int32_t((p0 >>  8) & 0xFF) * w0;
          cb += int32_t((p0      ) & 0xFF) * w0;

          sp += 4;
          wp += 1;
        }

        cr = blClamp<int32_t>(cr >> 8, 0, 255);
        cg = blClamp<int32_t>(cg >> 8, 0, 255);
        cb = blClamp<int32_t>(cb >> 8, 0, 255);
        reinterpret_cast<uint32_t*>(dp)[0] = blRgba32Pack(cr, cg, cb, 0xFF);

        recordList += 1;
        weightList += kernelSize;

        dp += 4;
      }

      dstLine += dstStride;
      srcLine += srcStride;
    }
  }
}

static void BL_CDECL blImageScaleHorzA8(const BLImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) noexcept {
  int dw = d->dstSize[0];
  int sh = d->srcSize[1];
  int kernelSize = d->kernelSize[0];

  if (!d->isUnbound[BLImageScaleContext::kDirHorz]) {
    for (int y = 0; y < sh; y++) {
      const BLImageScaleContext::Record* recordList = d->recordList[BLImageScaleContext::kDirHorz];
      const int32_t* weightList = d->weightList[BLImageScaleContext::kDirHorz];

      uint8_t* dp = dstLine;

      for (int x = 0; x < dw; x++) {
        const uint8_t* sp = srcLine + recordList->pos * 1;
        const int32_t* wp = weightList;

        uint32_t ca = 0x80;

        for (int i = recordList->count; i; i--) {
          uint32_t p0 = sp[0];
          uint32_t w0 = wp[0];

          ca += p0 * w0;

          sp += 1;
          wp += 1;
        }

        dp[0] = uint8_t(ca >> 8);

        recordList += 1;
        weightList += kernelSize;

        dp += 1;
      }

      dstLine += dstStride;
      srcLine += srcStride;
    }
  }
  else {
    for (int y = 0; y < sh; y++) {
      const BLImageScaleContext::Record* recordList = d->recordList[BLImageScaleContext::kDirHorz];
      const int32_t* weightList = d->weightList[BLImageScaleContext::kDirHorz];

      uint8_t* dp = dstLine;

      for (int x = 0; x < dw; x++) {
        const uint8_t* sp = srcLine + recordList->pos * 1;
        const int32_t* wp = weightList;

        int32_t ca = 0x80;

        for (int i = recordList->count; i; i--) {
          uint32_t p0 = sp[0];
          int32_t w0 = wp[0];

          ca += (int32_t)p0 * w0;

          sp += 1;
          wp += 1;
        }

        dp[0] = blClampToByte(ca >> 8);

        recordList += 1;
        weightList += kernelSize;

        dp += 1;
      }

      dstLine += dstStride;
      srcLine += srcStride;
    }
  }
}

// ============================================================================
// [BLImageScale - Vert]
// ============================================================================

static void BL_CDECL blImageScaleVertPrgb32(const BLImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) noexcept {
  int dw = d->dstSize[0];
  int dh = d->dstSize[1];
  int kernelSize = d->kernelSize[BLImageScaleContext::kDirVert];

  const BLImageScaleContext::Record* recordList = d->recordList[BLImageScaleContext::kDirVert];
  const int32_t* weightList = d->weightList[BLImageScaleContext::kDirVert];

  if (!d->isUnbound[BLImageScaleContext::kDirVert]) {
    for (int y = 0; y < dh; y++) {
      const uint8_t* srcData = srcLine + intptr_t(recordList->pos) * srcStride;
      uint8_t* dp = dstLine;

      int count = recordList->count;
      for (int x = 0; x < dw; x++) {
        const uint8_t* sp = srcData;
        const int32_t* wp = weightList;

        uint32_t cr_cb = 0x00800080;
        uint32_t ca_cg = 0x00800080;

        for (int i = count; i; i--) {
          uint32_t p0 = reinterpret_cast<const uint32_t*>(sp)[0];
          uint32_t w0 = wp[0];

          ca_cg += ((p0 >> 8) & 0x00FF00FF) * w0;
          cr_cb += ((p0     ) & 0x00FF00FF) * w0;

          sp += srcStride;
          wp += 1;
        }

        reinterpret_cast<uint32_t*>(dp)[0] = (ca_cg & 0xFF00FF00) + ((cr_cb & 0xFF00FF00) >> 8);

        dp += 4;
        srcData += 4;
      }

      recordList += 1;
      weightList += kernelSize;

      dstLine += dstStride;
    }
  }
  else {
    for (int y = 0; y < dh; y++) {
      const uint8_t* srcData = srcLine + intptr_t(recordList->pos) * srcStride;
      uint8_t* dp = dstLine;

      int count = recordList->count;
      for (int x = 0; x < dw; x++) {
        const uint8_t* sp = srcData;
        const int32_t* wp = weightList;

        int32_t ca = 0x80;
        int32_t cr = 0x80;
        int32_t cg = 0x80;
        int32_t cb = 0x80;

        for (int i = count; i; i--) {
          uint32_t p0 = reinterpret_cast<const uint32_t*>(sp)[0];
          int32_t w0 = wp[0];

          ca += int32_t((p0 >> 24)       ) * w0;
          cr += int32_t((p0 >> 16) & 0xFF) * w0;
          cg += int32_t((p0 >>  8) & 0xFF) * w0;
          cb += int32_t((p0      ) & 0xFF) * w0;

          sp += srcStride;
          wp += 1;
        }

        ca = blClamp<int32_t>(ca >> 8, 0, 255);
        cr = blClamp<int32_t>(cr >> 8, 0, ca);
        cg = blClamp<int32_t>(cg >> 8, 0, ca);
        cb = blClamp<int32_t>(cb >> 8, 0, ca);
        reinterpret_cast<uint32_t*>(dp)[0] = blRgba32Pack(cr, cg, cb, ca);

        dp += 4;
        srcData += 4;
      }

      recordList += 1;
      weightList += kernelSize;

      dstLine += dstStride;
    }
  }
}

static void BL_CDECL blImageScaleVertXrgb32(const BLImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) noexcept {
  int dw = d->dstSize[0];
  int dh = d->dstSize[1];
  int kernelSize = d->kernelSize[BLImageScaleContext::kDirVert];

  const BLImageScaleContext::Record* recordList = d->recordList[BLImageScaleContext::kDirVert];
  const int32_t* weightList = d->weightList[BLImageScaleContext::kDirVert];

  if (!d->isUnbound[BLImageScaleContext::kDirVert]) {
    for (int y = 0; y < dh; y++) {
      const uint8_t* srcData = srcLine + intptr_t(recordList->pos) * srcStride;
      uint8_t* dp = dstLine;

      int count = recordList->count;
      for (int x = 0; x < dw; x++) {
        const uint8_t* sp = srcData;
        const int32_t* wp = weightList;

        uint32_t cx_cg = 0x00008000;
        uint32_t cr_cb = 0x00800080;

        for (int i = count; i; i--) {
          uint32_t p0 = reinterpret_cast<const uint32_t*>(sp)[0];
          uint32_t w0 = wp[0];

          cx_cg += (p0 & 0x0000FF00) * w0;
          cr_cb += (p0 & 0x00FF00FF) * w0;

          sp += srcStride;
          wp += 1;
        }

        reinterpret_cast<uint32_t*>(dp)[0] = 0xFF000000 + (((cx_cg & 0x00FF0000) | (cr_cb & 0xFF00FF00)) >> 8);

        dp += 4;
        srcData += 4;
      }

      recordList += 1;
      weightList += kernelSize;

      dstLine += dstStride;
    }
  }
  else {
    for (int y = 0; y < dh; y++) {
      const uint8_t* srcData = srcLine + intptr_t(recordList->pos) * srcStride;
      uint8_t* dp = dstLine;

      int count = recordList->count;
      for (int x = 0; x < dw; x++) {
        const uint8_t* sp = srcData;
        const int32_t* wp = weightList;

        int32_t cr = 0x80;
        int32_t cg = 0x80;
        int32_t cb = 0x80;

        for (int i = count; i; i--) {
          uint32_t p0 = reinterpret_cast<const uint32_t*>(sp)[0];
          int32_t w0 = wp[0];

          cr += int32_t((p0 >> 16) & 0xFF) * w0;
          cg += int32_t((p0 >>  8) & 0xFF) * w0;
          cb += int32_t((p0      ) & 0xFF) * w0;

          sp += srcStride;
          wp += 1;
        }

        cr = blClamp<int32_t>(cr >> 8, 0, 255);
        cg = blClamp<int32_t>(cg >> 8, 0, 255);
        cb = blClamp<int32_t>(cb >> 8, 0, 255);
        reinterpret_cast<uint32_t*>(dp)[0] = blRgba32Pack(cr, cg, cb, 0xFF);

        dp += 4;
        srcData += 4;
      }

      recordList += 1;
      weightList += kernelSize;

      dstLine += dstStride;
    }
  }
}

static void BL_CDECL blImageScaleVertBytes(const BLImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride, uint32_t wScale) noexcept {
  int dw = d->dstSize[0] * wScale;
  int dh = d->dstSize[1];
  int kernelSize = d->kernelSize[BLImageScaleContext::kDirVert];

  const BLImageScaleContext::Record* recordList = d->recordList[BLImageScaleContext::kDirVert];
  const int32_t* weightList = d->weightList[BLImageScaleContext::kDirVert];

  if (!d->isUnbound[BLImageScaleContext::kDirVert]) {
    for (int y = 0; y < dh; y++) {
      const uint8_t* srcData = srcLine + intptr_t(recordList->pos) * srcStride;
      uint8_t* dp = dstLine;

      int x = dw;
      int i = 0;
      int count = recordList->count;

      if (((intptr_t)dp & 0x7) == 0)
        goto BoundLarge;
      i = 8 - int((intptr_t)dp & 0x7);

BoundSmall:
      x -= i;
      do {
        const uint8_t* sp = srcData;
        const int32_t* wp = weightList;

        uint32_t c0 = 0x80;

        for (int j = count; j; j--) {
          uint32_t p0 = sp[0];
          uint32_t w0 = wp[0];

          c0 += p0 * w0;

          sp += srcStride;
          wp += 1;
        }

        dp[0] = (uint8_t)(c0 >> 8);

        dp += 1;
        srcData += 1;
      } while (--i);

BoundLarge:
      while (x >= 8) {
        const uint8_t* sp = srcData;
        const int32_t* wp = weightList;

        uint32_t c0 = 0x00800080;
        uint32_t c1 = 0x00800080;
        uint32_t c2 = 0x00800080;
        uint32_t c3 = 0x00800080;

        for (int j = count; j; j--) {
          uint32_t p0 = reinterpret_cast<const uint32_t*>(sp)[0];
          uint32_t p1 = reinterpret_cast<const uint32_t*>(sp)[1];
          uint32_t w0 = wp[0];

          c0 += ((p0     ) & 0x00FF00FF) * w0;
          c1 += ((p0 >> 8) & 0x00FF00FF) * w0;
          c2 += ((p1     ) & 0x00FF00FF) * w0;
          c3 += ((p1 >> 8) & 0x00FF00FF) * w0;

          sp += srcStride;
          wp += 1;
        }

        reinterpret_cast<uint32_t*>(dp)[0] = ((c0 & 0xFF00FF00) >> 8) + (c1 & 0xFF00FF00);
        reinterpret_cast<uint32_t*>(dp)[1] = ((c2 & 0xFF00FF00) >> 8) + (c3 & 0xFF00FF00);

        dp += 8;
        srcData += 8;

        x -= 8;
      }

      i = x;
      if (i != 0)
        goto BoundSmall;

      recordList += 1;
      weightList += kernelSize;

      dstLine += dstStride;
    }
  }
  else {
    for (int y = 0; y < dh; y++) {
      const uint8_t* srcData = srcLine + intptr_t(recordList->pos) * srcStride;
      uint8_t* dp = dstLine;

      int x = dw;
      int i = 0;
      int count = recordList->count;

      if (((size_t)dp & 0x3) == 0)
        goto UnboundLarge;
      i = 4 - int((intptr_t)dp & 0x3);

UnboundSmall:
      x -= i;
      do {
        const uint8_t* sp = srcData;
        const int32_t* wp = weightList;

        int32_t c0 = 0x80;

        for (int j = count; j; j--) {
          uint32_t p0 = sp[0];
          int32_t w0 = wp[0];

          c0 += int32_t(p0) * w0;

          sp += srcStride;
          wp += 1;
        }

        dp[0] = (uint8_t)(uint32_t)blClamp<int32_t>(c0 >> 8, 0, 255);

        dp += 1;
        srcData += 1;
      } while (--i);

UnboundLarge:
      while (x >= 4) {
        const uint8_t* sp = srcData;
        const int32_t* wp = weightList;

        int32_t c0 = 0x80;
        int32_t c1 = 0x80;
        int32_t c2 = 0x80;
        int32_t c3 = 0x80;

        for (int j = count; j; j--) {
          uint32_t p0 = reinterpret_cast<const uint32_t*>(sp)[0];
          uint32_t w0 = wp[0];

          c0 += ((p0      ) & 0xFF) * w0;
          c1 += ((p0 >>  8) & 0xFF) * w0;
          c2 += ((p0 >> 16) & 0xFF) * w0;
          c3 += ((p0 >> 24)       ) * w0;

          sp += srcStride;
          wp += 1;
        }

        reinterpret_cast<uint32_t*>(dp)[0] =
          (blClamp<int32_t>(c0 >> 8, 0, 255)      ) +
          (blClamp<int32_t>(c1 >> 8, 0, 255) <<  8) +
          (blClamp<int32_t>(c2 >> 8, 0, 255) << 16) +
          (blClamp<int32_t>(c3 >> 8, 0, 255) << 24) ;

        dp += 4;
        srcData += 4;

        x -= 4;
      }

      i = x;
      if (i != 0)
        goto UnboundSmall;

      recordList += 1;
      weightList += kernelSize;

      dstLine += dstStride;
    }
  }
}

static void BL_CDECL blImageScaleVertA8(const BLImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) noexcept {
  blImageScaleVertBytes(d, dstLine, dstStride, srcLine, srcStride, 1);
}

// ============================================================================
// [BLImageScale - Reset]
// ============================================================================

BLResult BLImageScaleContext::reset() noexcept {
  if (data != nullptr)
    free(data);

  data = nullptr;
  return BL_SUCCESS;
}

// ============================================================================
// [BLImageScale - Interface]
// ============================================================================

BLResult BLImageScaleContext::create(const BLSizeI& to, const BLSizeI& from, uint32_t filter, const BLImageScaleOptions* options) noexcept {
  if (!options)
    options = &blImageScaleOptionsNone;

  BLImageScaleBuiltInParams p;
  BLImageScaleUserFunc userFunc;
  const void* userData = &p;

  // --------------------------------------------------------------------------
  // [Setup Parameters]
  // --------------------------------------------------------------------------

  if (!blIsValid(to) || !blIsValid(from))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  switch (filter) {
    case BL_IMAGE_SCALE_FILTER_NEAREST : userFunc = blImageScaleNearestFunc ; p.radius = 1.0; break;
    case BL_IMAGE_SCALE_FILTER_BILINEAR: userFunc = blImageScaleBilinearFunc; p.radius = 1.0; break;
    case BL_IMAGE_SCALE_FILTER_BICUBIC : userFunc = blImageScaleBicubicFunc ; p.radius = 2.0; break;
    case BL_IMAGE_SCALE_FILTER_BELL    : userFunc = blImageScaleBellFunc    ; p.radius = 1.5; break;
    case BL_IMAGE_SCALE_FILTER_GAUSS   : userFunc = blImageScaleGaussFunc   ; p.radius = 2.0; break;
    case BL_IMAGE_SCALE_FILTER_HERMITE : userFunc = blImageScaleHermiteFunc ; p.radius = 1.0; break;
    case BL_IMAGE_SCALE_FILTER_HANNING : userFunc = blImageScaleHanningFunc ; p.radius = 1.0; break;
    case BL_IMAGE_SCALE_FILTER_CATROM  : userFunc = blImageScaleCatromFunc  ; p.radius = 2.0; break;
    case BL_IMAGE_SCALE_FILTER_BESSEL  : userFunc = blImageScaleBesselFunc  ; p.radius = 3.2383; break;

    case BL_IMAGE_SCALE_FILTER_SINC    : userFunc = blImageScaleSincFunc    ; p.radius = options->radius; break;
    case BL_IMAGE_SCALE_FILTER_LANCZOS : userFunc = blImageScaleLanczosFunc ; p.radius = options->radius; break;
    case BL_IMAGE_SCALE_FILTER_BLACKMAN: userFunc = blImageScaleBlackmanFunc; p.radius = options->radius; break;

    case BL_IMAGE_SCALE_FILTER_MITCHELL: {
      p.radius = 2.0;
      if (!blIsFinite(options->mitchell.b) || !blIsFinite(options->mitchell.c))
        return blTraceError(BL_ERROR_INVALID_VALUE);

      p.initMitchell(options->mitchell.b, options->mitchell.c);
      userFunc = blImageScaleMitchellFunc;
      break;
    }

    case BL_IMAGE_SCALE_FILTER_USER: {
      userFunc = options->userFunc;
      userData = options->userData;

      if (!userFunc)
        return blTraceError(BL_ERROR_INVALID_VALUE);
      break;
    }

    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  if (!(p.radius >= 1.0 && p.radius <= 16.0))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  // --------------------------------------------------------------------------
  // [Setup Weights]
  // --------------------------------------------------------------------------

  double scale[2];
  double factor[2];
  double radius[2];
  int kernelSize[2];
  int isUnbound[2];

  scale[0] = double(to.w) / double(from.w);
  scale[1] = double(to.h) / double(from.h);

  factor[0] = 1.0;
  factor[1] = 1.0;

  radius[0] = p.radius;
  radius[1] = p.radius;

  if (scale[0] < 1.0) { factor[0] = scale[0]; radius[0] = p.radius / scale[0]; }
  if (scale[1] < 1.0) { factor[1] = scale[1]; radius[1] = p.radius / scale[1]; }

  kernelSize[0] = blCeilToInt(1.0 + 2.0 * radius[0]);
  kernelSize[1] = blCeilToInt(1.0 + 2.0 * radius[1]);

  isUnbound[0] = false;
  isUnbound[1] = false;

  size_t wWeightDataSize = size_t(to.w) * kernelSize[0] * sizeof(int32_t);
  size_t hWeightDataSize = size_t(to.h) * kernelSize[1] * sizeof(int32_t);
  size_t wRecordDataSize = size_t(to.w) * sizeof(Record);
  size_t hRecordDataSize = size_t(to.h) * sizeof(Record);
  size_t dataSize = sizeof(Data) + wWeightDataSize + hWeightDataSize + wRecordDataSize + hRecordDataSize;

  if (this->data)
    free(this->data);

  this->data = static_cast<Data*>(malloc(dataSize));
  if (BL_UNLIKELY(!this->data))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  // Init data.
  Data* d = this->data;
  d->dstSize[0] = to.w;
  d->dstSize[1] = to.h;
  d->srcSize[0] = from.w;
  d->srcSize[1] = from.h;
  d->kernelSize[0] = kernelSize[0];
  d->kernelSize[1] = kernelSize[1];
  d->isUnbound[0] = isUnbound[0];
  d->isUnbound[1] = isUnbound[1];

  d->scale[0] = scale[0];
  d->scale[1] = scale[1];
  d->factor[0] = factor[0];
  d->factor[1] = factor[1];
  d->radius[0] = radius[0];
  d->radius[1] = radius[1];

  // Distribute the memory buffer.
  uint8_t* dataPtr = blOffsetPtr<uint8_t>(d, sizeof(Data));

  d->weightList[kDirHorz] = reinterpret_cast<int32_t*>(dataPtr); dataPtr += wWeightDataSize;
  d->weightList[kDirVert] = reinterpret_cast<int32_t*>(dataPtr); dataPtr += hWeightDataSize;
  d->recordList[kDirHorz] = reinterpret_cast<Record*>(dataPtr); dataPtr += wRecordDataSize;
  d->recordList[kDirVert] = reinterpret_cast<Record*>(dataPtr);

  // Built-in filters will probably never fail, however, custom filters can and
  // it wouldn't be safe to just continue.
  BL_PROPAGATE(blImageScaleOps.weights(d, kDirHorz, userFunc, userData));
  BL_PROPAGATE(blImageScaleOps.weights(d, kDirVert, userFunc, userData));

  return BL_SUCCESS;
}

BLResult BLImageScaleContext::processHorzData(uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride, uint32_t format) const noexcept {
  BL_ASSERT(isInitialized());
  blImageScaleOps.horz[format](this->data, dstLine, dstStride, srcLine, srcStride);
  return BL_SUCCESS;
}

BLResult BLImageScaleContext::processVertData(uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride, uint32_t format) const noexcept {
  BL_ASSERT(isInitialized());
  blImageScaleOps.vert[format](this->data, dstLine, dstStride, srcLine, srcStride);
  return BL_SUCCESS;
}

// ============================================================================
// [BLImageScale - Runtime Init]
// ============================================================================

void blImageScalerRtInit(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);

  blImageScaleOps.weights = blImageScaleWeights;

  blImageScaleOps.horz[BL_FORMAT_PRGB32] = blImageScaleHorzPrgb32;
  blImageScaleOps.horz[BL_FORMAT_XRGB32] = blImageScaleHorzXrgb32;
  blImageScaleOps.horz[BL_FORMAT_A8    ] = blImageScaleHorzA8;

  blImageScaleOps.vert[BL_FORMAT_PRGB32] = blImageScaleVertPrgb32;
  blImageScaleOps.vert[BL_FORMAT_XRGB32] = blImageScaleVertXrgb32;
  blImageScaleOps.vert[BL_FORMAT_A8    ] = blImageScaleVertA8;
}
