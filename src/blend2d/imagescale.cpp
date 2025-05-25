// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "imagescale_p.h"
#include "format_p.h"
#include "geometry_p.h"
#include "rgba_p.h"
#include "runtime_p.h"
#include "support/math_p.h"
#include "support/memops_p.h"
#include "support/ptrops_p.h"
#include "support/scopedbuffer_p.h"

namespace bl {

typedef void (BL_CDECL* ImageScaleFilterFunc)(double* dst, const double* tArray, size_t n) noexcept;

// bl::ImageScale - Ops
// ====================

struct ImageScaleOps {
  BLResult (BL_CDECL* weights)(ImageScaleContext::Data* d, uint32_t dir, ImageScaleFilterFunc filterFunc) noexcept;
  void (BL_CDECL* horz[BL_FORMAT_MAX_VALUE + 1])(const ImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) noexcept;
  void (BL_CDECL* vert[BL_FORMAT_MAX_VALUE + 1])(const ImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) noexcept;
};
static ImageScaleOps imageScaleOps;

// bl::ImageScale - Filter Implementations
// =======================================

static void BL_CDECL imageScaleNearestFilter(double* dst, const double* tArray, size_t n) noexcept {
  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t <= 0.5 ? 1.0 : 0.0;
  }
}

static void BL_CDECL imageScaleBilinearFilter(double* dst, const double* tArray, size_t n) noexcept {
  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t < 1.0 ? 1.0 - t : 0.0;
  }
}

static void BL_CDECL imageScaleBicubicFilter(double* dst, const double* tArray, size_t n) noexcept {
  constexpr double k2Div3 = 2.0 / 3.0;

  // 0.5t^3 - t^2 + 2/3 == (0.5t - 1.0) t^2 + 2/3
  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t < 1.0 ? (t * 0.5 - 1.0) * Math::square(t) + k2Div3 :
             t < 2.0 ? Math::cube(2.0 - t) / 6.0 : 0.0;
  }
}

static BL_INLINE double lanczos(double x, double y) noexcept {
  double sin_x = Math::sin(x);
  double sin_y = Math::sin(y);

  return (sin_x * sin_y) / (x * y);
}

static void BL_CDECL imageScaleLanczosFilter(double* dst, const double* tArray, size_t n) noexcept {
  constexpr double r = 2.0;
  constexpr double x = Math::kPI;
  constexpr double y = Math::kPI_DIV_2;

  for (size_t i = 0; i < n; i++) {
    double t = tArray[i];
    dst[i] = t == 0.0 ? 1.0 : t <= r ? lanczos(t * x, t * y) : 0.0;
  }
}

// bl::ImageScale - Weights
// ========================

static BLResult BL_CDECL imageScaleWeights(ImageScaleContext::Data* d, uint32_t dir, ImageScaleFilterFunc filter) noexcept {
  int32_t* weightList = d->weightList[dir];
  ImageScaleContext::Record* recordList = d->recordList[dir];

  int dstSize = d->dstSize[dir];
  int srcSize = d->srcSize[dir];
  int kernelSize = d->kernelSize[dir];

  double radius = d->radius[dir];
  double factor = d->factor[dir];
  double scale = d->scale[dir];
  int32_t isUnbound = 0;

  bl::ScopedBufferTmp<512> wMem;
  double* wData = static_cast<double*>(wMem.alloc(unsigned(kernelSize) * sizeof(double)));

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

    filter(wData, wData, unsigned(kernelSize));

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
        recordList[i].pos = uint32_t(left);
        recordList[i].count = uint32_t(wCount);
      }
    }

    weightList += kernelSize;
  }

  d->isUnbound[dir] = isUnbound < 0;
  return BL_SUCCESS;
}

// bl::ImageScale - Horz
// =====================

static void BL_CDECL imageScaleHorzPrgb32(const ImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) noexcept {
  uint32_t dw = uint32_t(d->dstSize[0]);
  uint32_t sh = uint32_t(d->srcSize[1]);
  uint32_t kernelSize = uint32_t(d->kernelSize[0]);

  if (!d->isUnbound[ImageScaleContext::kDirHorz]) {
    for (uint32_t y = 0; y < sh; y++) {
      const ImageScaleContext::Record* recordList = d->recordList[ImageScaleContext::kDirHorz];
      const int32_t* weightList = d->weightList[ImageScaleContext::kDirHorz];

      uint8_t* dp = dstLine;

      for (uint32_t x = 0; x < dw; x++) {
        const uint8_t* sp = srcLine + recordList->pos * 4;
        const int32_t* wp = weightList;

        uint32_t cr_cb = 0x00800080u;
        uint32_t ca_cg = 0x00800080u;

        for (uint32_t i = recordList->count; i; i--) {
          uint32_t p0 = MemOps::readU32a(sp);
          uint32_t w0 = unsigned(wp[0]);

          ca_cg += ((p0 >> 8) & 0x00FF00FFu) * w0;
          cr_cb += ((p0     ) & 0x00FF00FFu) * w0;

          sp += 4;
          wp += 1;
        }

        MemOps::writeU32a(dp, (ca_cg & 0xFF00FF00u) + ((cr_cb & 0xFF00FF00u) >> 8));
        dp += 4;

        recordList += 1;
        weightList += kernelSize;
      }

      dstLine += dstStride;
      srcLine += srcStride;
    }
  }
  else {
    for (uint32_t y = 0; y < sh; y++) {
      const ImageScaleContext::Record* recordList = d->recordList[ImageScaleContext::kDirHorz];
      const int32_t* weightList = d->weightList[ImageScaleContext::kDirHorz];

      uint8_t* dp = dstLine;

      for (uint32_t x = 0; x < dw; x++) {
        const uint8_t* sp = srcLine + recordList->pos * 4;
        const int32_t* wp = weightList;

        int32_t ca = 0x80;
        int32_t cr = 0x80;
        int32_t cg = 0x80;
        int32_t cb = 0x80;

        for (uint32_t i = recordList->count; i; i--) {
          uint32_t p0 = MemOps::readU32a(sp);
          int32_t w0 = wp[0];

          ca += int32_t((p0 >> 24)        ) * w0;
          cr += int32_t((p0 >> 16) & 0xFFu) * w0;
          cg += int32_t((p0 >>  8) & 0xFFu) * w0;
          cb += int32_t((p0      ) & 0xFFu) * w0;

          sp += 4;
          wp += 1;
        }

        ca = blClamp<int32_t>(ca >> 8, 0, 255);
        cr = blClamp<int32_t>(cr >> 8, 0, ca);
        cg = blClamp<int32_t>(cg >> 8, 0, ca);
        cb = blClamp<int32_t>(cb >> 8, 0, ca);

        MemOps::writeU32a(dp, RgbaInternal::packRgba32(uint32_t(cr), uint32_t(cg), uint32_t(cb), uint32_t(ca)));
        dp += 4;

        recordList += 1;
        weightList += kernelSize;
      }

      dstLine += dstStride;
      srcLine += srcStride;
    }
  }
}

static void BL_CDECL imageScaleHorzXrgb32(const ImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) noexcept {
  uint32_t dw = uint32_t(d->dstSize[0]);
  uint32_t sh = uint32_t(d->srcSize[1]);
  uint32_t kernelSize = uint32_t(d->kernelSize[0]);

  if (!d->isUnbound[ImageScaleContext::kDirHorz]) {
    for (uint32_t y = 0; y < sh; y++) {
      const ImageScaleContext::Record* recordList = d->recordList[ImageScaleContext::kDirHorz];
      const int32_t* weightList = d->weightList[ImageScaleContext::kDirHorz];

      uint8_t* dp = dstLine;

      for (uint32_t x = 0; x < dw; x++) {
        const uint8_t* sp = srcLine + recordList->pos * 4;
        const int32_t* wp = weightList;

        uint32_t cx_cg = 0x00008000u;
        uint32_t cr_cb = 0x00800080u;

        for (uint32_t i = recordList->count; i; i--) {
          uint32_t p0 = MemOps::readU32a(sp);
          uint32_t w0 = unsigned(wp[0]);

          cx_cg += (p0 & 0x0000FF00u) * w0;
          cr_cb += (p0 & 0x00FF00FFu) * w0;

          sp += 4;
          wp += 1;
        }

        MemOps::writeU32a(dp, 0xFF000000u + (((cx_cg & 0x00FF0000u) | (cr_cb & 0xFF00FF00u)) >> 8));
        dp += 4;

        recordList += 1;
        weightList += kernelSize;
      }

      dstLine += dstStride;
      srcLine += srcStride;
    }
  }
  else {
    for (uint32_t y = 0; y < sh; y++) {
      const ImageScaleContext::Record* recordList = d->recordList[ImageScaleContext::kDirHorz];
      const int32_t* weightList = d->weightList[ImageScaleContext::kDirHorz];

      uint8_t* dp = dstLine;

      for (uint32_t x = 0; x < dw; x++) {
        const uint8_t* sp = srcLine + recordList->pos * 4;
        const int32_t* wp = weightList;

        int32_t cr = 0x80;
        int32_t cg = 0x80;
        int32_t cb = 0x80;

        for (uint32_t i = recordList->count; i; i--) {
          uint32_t p0 = MemOps::readU32a(sp);
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

        MemOps::writeU32a(dp, RgbaInternal::packRgba32(uint32_t(cr), uint32_t(cg), uint32_t(cb), 0xFFu));
        dp += 4;

        recordList += 1;
        weightList += kernelSize;
      }

      dstLine += dstStride;
      srcLine += srcStride;
    }
  }
}

static void BL_CDECL imageScaleHorzA8(const ImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) noexcept {
  uint32_t dw = uint32_t(d->dstSize[0]);
  uint32_t sh = uint32_t(d->srcSize[1]);
  uint32_t kernelSize = uint32_t(d->kernelSize[0]);

  if (!d->isUnbound[ImageScaleContext::kDirHorz]) {
    for (uint32_t y = 0; y < sh; y++) {
      const ImageScaleContext::Record* recordList = d->recordList[ImageScaleContext::kDirHorz];
      const int32_t* weightList = d->weightList[ImageScaleContext::kDirHorz];

      uint8_t* dp = dstLine;

      for (uint32_t x = 0; x < dw; x++) {
        const uint8_t* sp = srcLine + recordList->pos * 1;
        const int32_t* wp = weightList;

        uint32_t ca = 0x80;

        for (uint32_t i = recordList->count; i; i--) {
          uint32_t p0 = sp[0];
          uint32_t w0 = unsigned(wp[0]);

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
    for (uint32_t y = 0; y < sh; y++) {
      const ImageScaleContext::Record* recordList = d->recordList[ImageScaleContext::kDirHorz];
      const int32_t* weightList = d->weightList[ImageScaleContext::kDirHorz];

      uint8_t* dp = dstLine;

      for (uint32_t x = 0; x < dw; x++) {
        const uint8_t* sp = srcLine + recordList->pos * 1;
        const int32_t* wp = weightList;

        int32_t ca = 0x80;

        for (uint32_t i = recordList->count; i; i--) {
          uint32_t p0 = sp[0];
          int32_t w0 = wp[0];

          ca += (int32_t)p0 * w0;

          sp += 1;
          wp += 1;
        }

        dp[0] = IntOps::clampToByte(ca >> 8);

        recordList += 1;
        weightList += kernelSize;

        dp += 1;
      }

      dstLine += dstStride;
      srcLine += srcStride;
    }
  }
}

// bl::ImageScale - Vert
// =====================

static void BL_CDECL imageScaleVertPrgb32(const ImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) noexcept {
  uint32_t dw = uint32_t(d->dstSize[0]);
  uint32_t dh = uint32_t(d->dstSize[1]);
  uint32_t kernelSize = uint32_t(d->kernelSize[ImageScaleContext::kDirVert]);

  const ImageScaleContext::Record* recordList = d->recordList[ImageScaleContext::kDirVert];
  const int32_t* weightList = d->weightList[ImageScaleContext::kDirVert];

  if (!d->isUnbound[ImageScaleContext::kDirVert]) {
    for (uint32_t y = 0; y < dh; y++) {
      const uint8_t* srcData = srcLine + intptr_t(recordList->pos) * srcStride;
      uint8_t* dp = dstLine;

      uint32_t count = recordList->count;
      for (uint32_t x = 0; x < dw; x++) {
        const uint8_t* sp = srcData;
        const int32_t* wp = weightList;

        uint32_t cr_cb = 0x00800080;
        uint32_t ca_cg = 0x00800080;

        for (uint32_t i = count; i; i--) {
          uint32_t p0 = MemOps::readU32a(sp);
          uint32_t w0 = unsigned(wp[0]);

          ca_cg += ((p0 >> 8) & 0x00FF00FF) * w0;
          cr_cb += ((p0     ) & 0x00FF00FF) * w0;

          sp += srcStride;
          wp += 1;
        }

        MemOps::writeU32a(dp, (ca_cg & 0xFF00FF00) + ((cr_cb & 0xFF00FF00) >> 8));
        dp += 4;
        srcData += 4;
      }

      recordList += 1;
      weightList += kernelSize;

      dstLine += dstStride;
    }
  }
  else {
    for (uint32_t y = 0; y < dh; y++) {
      const uint8_t* srcData = srcLine + intptr_t(recordList->pos) * srcStride;
      uint8_t* dp = dstLine;

      uint32_t count = recordList->count;
      for (uint32_t x = 0; x < dw; x++) {
        const uint8_t* sp = srcData;
        const int32_t* wp = weightList;

        int32_t ca = 0x80;
        int32_t cr = 0x80;
        int32_t cg = 0x80;
        int32_t cb = 0x80;

        for (uint32_t i = count; i; i--) {
          uint32_t p0 = MemOps::readU32a(sp);
          int32_t w0 = wp[0];

          ca += int32_t((p0 >> 24)        ) * w0;
          cr += int32_t((p0 >> 16) & 0xFFu) * w0;
          cg += int32_t((p0 >>  8) & 0xFFu) * w0;
          cb += int32_t((p0      ) & 0xFFu) * w0;

          sp += srcStride;
          wp += 1;
        }

        ca = blClamp<int32_t>(ca >> 8, 0, 255);
        cr = blClamp<int32_t>(cr >> 8, 0, ca);
        cg = blClamp<int32_t>(cg >> 8, 0, ca);
        cb = blClamp<int32_t>(cb >> 8, 0, ca);

        MemOps::writeU32a(dp, RgbaInternal::packRgba32(uint32_t(cr), uint32_t(cg), uint32_t(cb), uint32_t(ca)));
        dp += 4;
        srcData += 4;
      }

      recordList += 1;
      weightList += kernelSize;

      dstLine += dstStride;
    }
  }
}

static void BL_CDECL imageScaleVertXrgb32(const ImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) noexcept {
  uint32_t dw = uint32_t(d->dstSize[0]);
  uint32_t dh = uint32_t(d->dstSize[1]);
  uint32_t kernelSize = uint32_t(d->kernelSize[ImageScaleContext::kDirVert]);

  const ImageScaleContext::Record* recordList = d->recordList[ImageScaleContext::kDirVert];
  const int32_t* weightList = d->weightList[ImageScaleContext::kDirVert];

  if (!d->isUnbound[ImageScaleContext::kDirVert]) {
    for (uint32_t y = 0; y < dh; y++) {
      const uint8_t* srcData = srcLine + intptr_t(recordList->pos) * srcStride;
      uint8_t* dp = dstLine;

      uint32_t count = recordList->count;
      for (uint32_t x = 0; x < dw; x++) {
        const uint8_t* sp = srcData;
        const int32_t* wp = weightList;

        uint32_t cx_cg = 0x00008000u;
        uint32_t cr_cb = 0x00800080u;

        for (uint32_t i = count; i; i--) {
          uint32_t p0 = MemOps::readU32a(sp);
          uint32_t w0 = unsigned(wp[0]);

          cx_cg += (p0 & 0x0000FF00u) * w0;
          cr_cb += (p0 & 0x00FF00FFu) * w0;

          sp += srcStride;
          wp += 1;
        }

        MemOps::writeU32a(dp, 0xFF000000u + (((cx_cg & 0x00FF0000u) | (cr_cb & 0xFF00FF00u)) >> 8));
        dp += 4;
        srcData += 4;
      }

      recordList += 1;
      weightList += kernelSize;

      dstLine += dstStride;
    }
  }
  else {
    for (uint32_t y = 0; y < dh; y++) {
      const uint8_t* srcData = srcLine + intptr_t(recordList->pos) * srcStride;
      uint8_t* dp = dstLine;

      uint32_t count = recordList->count;
      for (uint32_t x = 0; x < dw; x++) {
        const uint8_t* sp = srcData;
        const int32_t* wp = weightList;

        int32_t cr = 0x80;
        int32_t cg = 0x80;
        int32_t cb = 0x80;

        for (uint32_t i = count; i; i--) {
          uint32_t p0 = MemOps::readU32a(sp);
          int32_t w0 = wp[0];

          cr += int32_t((p0 >> 16) & 0xFFu) * w0;
          cg += int32_t((p0 >>  8) & 0xFFu) * w0;
          cb += int32_t((p0      ) & 0xFFu) * w0;

          sp += srcStride;
          wp += 1;
        }

        cr = blClamp<int32_t>(cr >> 8, 0, 255);
        cg = blClamp<int32_t>(cg >> 8, 0, 255);
        cb = blClamp<int32_t>(cb >> 8, 0, 255);

        MemOps::writeU32a(dp, RgbaInternal::packRgba32(uint32_t(cr), uint32_t(cg), uint32_t(cb), 0xFFu));
        dp += 4;
        srcData += 4;
      }

      recordList += 1;
      weightList += kernelSize;

      dstLine += dstStride;
    }
  }
}

static void BL_CDECL blImageScaleVertBytes(const ImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride, uint32_t wScale) noexcept {
  uint32_t dw = uint32_t(d->dstSize[0]) * wScale;
  uint32_t dh = uint32_t(d->dstSize[1]);
  uint32_t kernelSize = uint32_t(d->kernelSize[ImageScaleContext::kDirVert]);

  const ImageScaleContext::Record* recordList = d->recordList[ImageScaleContext::kDirVert];
  const int32_t* weightList = d->weightList[ImageScaleContext::kDirVert];

  if (!d->isUnbound[ImageScaleContext::kDirVert]) {
    for (uint32_t y = 0; y < dh; y++) {
      const uint8_t* srcData = srcLine + intptr_t(recordList->pos) * srcStride;
      uint8_t* dp = dstLine;

      uint32_t x = dw;
      uint32_t i = 0;
      uint32_t count = recordList->count;

      if (((intptr_t)dp & 0x7) == 0)
        goto BoundLarge;
      i = 8u - uint32_t((uintptr_t)dp & 0x7u);

BoundSmall:
      x -= i;
      do {
        const uint8_t* sp = srcData;
        const int32_t* wp = weightList;

        uint32_t c0 = 0x80;

        for (uint32_t j = count; j; j--) {
          uint32_t p0 = sp[0];
          uint32_t w0 = unsigned(wp[0]);

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

        uint32_t c0 = 0x00800080u;
        uint32_t c1 = 0x00800080u;
        uint32_t c2 = 0x00800080u;
        uint32_t c3 = 0x00800080u;

        for (uint32_t j = count; j; j--) {
          uint32_t p0 = MemOps::readU32a(sp + 0u);
          uint32_t p1 = MemOps::readU32a(sp + 4u);
          uint32_t w0 = unsigned(wp[0]);

          c0 += ((p0     ) & 0x00FF00FFu) * w0;
          c1 += ((p0 >> 8) & 0x00FF00FFu) * w0;
          c2 += ((p1     ) & 0x00FF00FFu) * w0;
          c3 += ((p1 >> 8) & 0x00FF00FFu) * w0;

          sp += srcStride;
          wp += 1;
        }

        MemOps::writeU32a(dp + 0u, ((c0 & 0xFF00FF00u) >> 8) + (c1 & 0xFF00FF00u));
        MemOps::writeU32a(dp + 4u, ((c2 & 0xFF00FF00u) >> 8) + (c3 & 0xFF00FF00u));

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
    for (uint32_t y = 0; y < dh; y++) {
      const uint8_t* srcData = srcLine + intptr_t(recordList->pos) * srcStride;
      uint8_t* dp = dstLine;

      uint32_t x = dw;
      uint32_t i = 0;
      uint32_t count = recordList->count;

      if (((size_t)dp & 0x3) == 0)
        goto UnboundLarge;
      i = 4u - uint32_t((uintptr_t)dp & 0x3u);

UnboundSmall:
      x -= i;
      do {
        const uint8_t* sp = srcData;
        const int32_t* wp = weightList;

        int32_t c0 = 0x80;

        for (uint32_t j = count; j; j--) {
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

        for (uint32_t j = count; j; j--) {
          uint32_t p0 = MemOps::readU32a(sp);
          uint32_t w0 = unsigned(wp[0]);

          c0 += ((p0      ) & 0xFF) * w0;
          c1 += ((p0 >>  8) & 0xFF) * w0;
          c2 += ((p0 >> 16) & 0xFF) * w0;
          c3 += ((p0 >> 24)       ) * w0;

          sp += srcStride;
          wp += 1;
        }

        MemOps::writeU32a(dp, uint32_t(blClamp<int32_t>(c0 >> 8, 0, 255)      ) |
                           uint32_t(blClamp<int32_t>(c1 >> 8, 0, 255) <<  8) |
                           uint32_t(blClamp<int32_t>(c2 >> 8, 0, 255) << 16) |
                           uint32_t(blClamp<int32_t>(c3 >> 8, 0, 255) << 24));
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

static void BL_CDECL imageScaleVertA8(const ImageScaleContext::Data* d, uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride) noexcept {
  blImageScaleVertBytes(d, dstLine, dstStride, srcLine, srcStride, 1);
}

// bl::ImageScaleContext - Reset
// =============================

BLResult ImageScaleContext::reset() noexcept {
  free(data);
  data = nullptr;
  return BL_SUCCESS;
}

// bl::ImageScaleContext - Create
// ==============================

BLResult ImageScaleContext::create(const BLSizeI& to, const BLSizeI& from, uint32_t filter) noexcept {
  ImageScaleFilterFunc filterFunc;
  double r = 0.0;

  // Setup Parameters
  // ----------------

  if (!Geometry::isValid(to) || !Geometry::isValid(from))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  switch (filter) {
    case BL_IMAGE_SCALE_FILTER_NEAREST : filterFunc = imageScaleNearestFilter ; r = 1.0; break;
    case BL_IMAGE_SCALE_FILTER_BILINEAR: filterFunc = imageScaleBilinearFilter; r = 1.0; break;
    case BL_IMAGE_SCALE_FILTER_BICUBIC : filterFunc = imageScaleBicubicFilter ; r = 2.0; break;
    case BL_IMAGE_SCALE_FILTER_LANCZOS : filterFunc = imageScaleLanczosFilter ; r = 2.0; break;

    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  // Setup Weights
  // -------------

  double scale[2];
  double factor[2];
  double radius[2];
  int kernelSize[2];
  int isUnbound[2];

  scale[0] = double(to.w) / double(from.w);
  scale[1] = double(to.h) / double(from.h);

  factor[0] = 1.0;
  factor[1] = 1.0;

  radius[0] = r;
  radius[1] = r;

  if (scale[0] < 1.0) { factor[0] = scale[0]; radius[0] = r / scale[0]; }
  if (scale[1] < 1.0) { factor[1] = scale[1]; radius[1] = r / scale[1]; }

  kernelSize[0] = Math::ceilToInt(1.0 + 2.0 * radius[0]);
  kernelSize[1] = Math::ceilToInt(1.0 + 2.0 * radius[1]);

  isUnbound[0] = false;
  isUnbound[1] = false;

  size_t wWeightDataSize = size_t(to.w) * unsigned(kernelSize[0]) * sizeof(int32_t);
  size_t hWeightDataSize = size_t(to.h) * unsigned(kernelSize[1]) * sizeof(int32_t);
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
  uint8_t* dataPtr = PtrOps::offset<uint8_t>(d, sizeof(Data));

  d->weightList[kDirHorz] = reinterpret_cast<int32_t*>(dataPtr); dataPtr += wWeightDataSize;
  d->weightList[kDirVert] = reinterpret_cast<int32_t*>(dataPtr); dataPtr += hWeightDataSize;
  d->recordList[kDirHorz] = reinterpret_cast<Record*>(dataPtr); dataPtr += wRecordDataSize;
  d->recordList[kDirVert] = reinterpret_cast<Record*>(dataPtr);

  // Built-in filters will probably never fail, however, custom filters can and
  // it wouldn't be safe to just continue.
  imageScaleOps.weights(d, kDirHorz, filterFunc);
  imageScaleOps.weights(d, kDirVert, filterFunc);

  return BL_SUCCESS;
}

// bl::ImageScale - Process
// ========================

BLResult ImageScaleContext::processHorzData(uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride, uint32_t format) const noexcept {
  BL_ASSERT(isInitialized());
  imageScaleOps.horz[format](this->data, dstLine, dstStride, srcLine, srcStride);
  return BL_SUCCESS;
}

BLResult ImageScaleContext::processVertData(uint8_t* dstLine, intptr_t dstStride, const uint8_t* srcLine, intptr_t srcStride, uint32_t format) const noexcept {
  BL_ASSERT(isInitialized());
  imageScaleOps.vert[format](this->data, dstLine, dstStride, srcLine, srcStride);
  return BL_SUCCESS;
}

} // {bl}

// bl::ImageScale - Runtime Registration
// =====================================

void blImageScaleRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  bl::imageScaleOps.weights = bl::imageScaleWeights;

  bl::imageScaleOps.horz[BL_FORMAT_PRGB32] = bl::imageScaleHorzPrgb32;
  bl::imageScaleOps.horz[BL_FORMAT_XRGB32] = bl::imageScaleHorzXrgb32;
  bl::imageScaleOps.horz[BL_FORMAT_A8    ] = bl::imageScaleHorzA8;

  bl::imageScaleOps.vert[BL_FORMAT_PRGB32] = bl::imageScaleVertPrgb32;
  bl::imageScaleOps.vert[BL_FORMAT_XRGB32] = bl::imageScaleVertXrgb32;
  bl::imageScaleOps.vert[BL_FORMAT_A8    ] = bl::imageScaleVertA8;
}
