// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../gradient_p.h"
#include "../pixelops/scalar_p.h"
#include "../support/math_p.h"

namespace bl {
namespace PixelOps {
namespace Interpolation {

// bl::PixelOps - Interpolate PRGB32
// =================================

void BL_CDECL interpolate_prgb32(uint32_t* dPtr, uint32_t dSize, const BLGradientStop* sPtr, size_t sSize) noexcept {
  BL_ASSERT(dPtr != nullptr);
  BL_ASSERT(dSize > 0);

  BL_ASSERT(sPtr != nullptr);
  BL_ASSERT(sSize > 0);

  uint32_t* dSpanPtr = dPtr;
  uint32_t i = dSize;

  uint32_t c0 = RgbaInternal::rgba32FromRgba64(sPtr[0].rgba.value);
  uint32_t c1 = c0;

  uint32_t p0 = 0;
  uint32_t p1;

  size_t sIndex = 0;
  double fWidth = double(int32_t(--dSize) << 8);

  uint32_t cp = Scalar::cvt_prgb32_8888_from_argb32_8888(c0);
  uint32_t cpFirst = cp;

  if (sSize == 1)
    goto SolidLoop;

  do {
    c1 = RgbaInternal::rgba32FromRgba64(sPtr[sIndex].rgba.value);
    p1 = uint32_t(Math::roundToInt(sPtr[sIndex].offset * fWidth));

    dSpanPtr = dPtr + (p0 >> 8);
    i = ((p1 >> 8) - (p0 >> 8));

    if (i == 0)
      c0 = c1;

    p0 = p1;
    i++;

SolidInit:
    cp = Scalar::cvt_prgb32_8888_from_argb32_8888(c0);
    if (c0 == c1) {
SolidLoop:
      do {
        dSpanPtr[0] = cp;
        dSpanPtr++;
      } while (--i);
    }
    else {
      dSpanPtr[0] = cp;
      dSpanPtr++;

      if (--i) {
        const uint32_t kShift = 23;
        const uint32_t kMask = 0xFFu << kShift;

        uint32_t rPos = (c0 <<  7) & kMask;
        uint32_t gPos = (c0 << 15) & kMask;
        uint32_t bPos = (c0 << 23) & kMask;

        uint32_t rInc = (c1 <<  7) & kMask;
        uint32_t gInc = (c1 << 15) & kMask;
        uint32_t bInc = (c1 << 23) & kMask;

        rInc = uint32_t(int32_t(rInc - rPos) / int32_t(i));
        gInc = uint32_t(int32_t(gInc - gPos) / int32_t(i));
        bInc = uint32_t(int32_t(bInc - bPos) / int32_t(i));

        rPos += 1u << (kShift - 1);
        gPos += 1u << (kShift - 1);
        bPos += 1u << (kShift - 1);

        if (RgbaInternal::isRgba32FullyOpaque(c0 & c1)) {
          // Both fully opaque, no need to premultiply.
          do {
            rPos += rInc;
            gPos += gInc;
            bPos += bInc;

            cp = 0xFF000000u + ((rPos & kMask) >>  7) +
                               ((gPos & kMask) >> 15) +
                               ((bPos & kMask) >> 23) ;

            dSpanPtr[0] = cp;
            dSpanPtr++;
          } while (--i);
        }
        else {
          // One or both having alpha, have to be premultiplied.
          uint32_t aPos = (c0 >> 1) & kMask;
          uint32_t aInc = (c1 >> 1) & kMask;

          aInc = uint32_t(int32_t(aInc - aPos) / int32_t(i));
          aPos += 1u << (kShift - 1);

          do {
            aPos += aInc;
            rPos += rInc;
            gPos += gInc;
            bPos += bInc;

            // `cp` contains red/blue components combined.
            uint32_t ca, cg;
            cp = ((bPos & kMask) >> 23) +
                 ((rPos & kMask) >>  7);
            ca = ((aPos & kMask) >> 23);
            cg = ((gPos & kMask) >> 23);

            cp = (cp * ca) + 0x00800080u;
            cg = (cg * ca) + 0x00000080u;
            ca <<= 24;

            cp = ((cp + ((cp & 0xFF00FF00u) >> 8)) & 0xFF00FF00u) >> 8;
            cg = ((cg + ((cg & 0xFF00FF00u) >> 8)) & 0xFF00FF00u);

            cp += cg + ca;

            dSpanPtr[0] = cp;
            dSpanPtr++;
          } while (--i);
        }
      }

      c0 = c1;
    }
  } while (++sIndex < sSize);

  // The last stop doesn't have to end at 1.0, in such case the remaining space is filled by the last color stop
  // (premultiplied). We jump to the main loop instead of filling the buffer here.
  i = uint32_t((size_t)((dPtr + dSize + 1) - dSpanPtr));
  if (i != 0)
    goto SolidInit;

  // The first pixel has to be always set to the first stop's color. The main loop always honors the last color
  // value of the stop colliding with the previous offset index - for example if multiple stops have the same offset
  // [0.0] the first pixel will be the last stop's color. This is easier to fix here as we don't need extra conditions
  // in the main loop.
  dPtr[0] = cpFirst;
}

// bl::PixelOps - Interpolate PRGB64
// =================================

void BL_CDECL interpolate_prgb64(uint64_t* dPtr, uint32_t dSize, const BLGradientStop* sPtr, size_t sSize) noexcept {
  BL_ASSERT(dPtr != nullptr);
  BL_ASSERT(dSize > 0);

  BL_ASSERT(sPtr != nullptr);
  BL_ASSERT(sSize > 0);

  uint64_t* dSpanPtr = dPtr;
  uint32_t i = dSize;

  uint64_t c0 = sPtr[0].rgba.value;
  uint64_t c1 = c0;

  uint32_t p0 = 0;
  uint32_t p1;

  size_t sIndex = 0;
  double fWidth = double(int32_t(--dSize) << 8);

  uint64_t cp = Scalar::cvt_prgb64_8888_from_argb64_8888(c0);
  uint64_t cpFirst = cp;

  if (sSize == 1)
    goto SolidLoop;

  do {
    c1 = sPtr[sIndex].rgba.value;
    p1 = uint32_t(Math::roundToInt(sPtr[sIndex].offset * fWidth));

    dSpanPtr = dPtr + (p0 >> 8);
    i = ((p1 >> 8) - (p0 >> 8));

    if (i == 0)
      c0 = c1;

    p0 = p1;
    i++;

SolidInit:
    cp = Scalar::cvt_prgb64_8888_from_argb64_8888(c0);
    if (c0 == c1) {
SolidLoop:
      do {
        dSpanPtr[0] = cp;
        dSpanPtr++;
      } while (--i);
    }
    else {
      dSpanPtr[0] = cp;
      dSpanPtr++;

      if (--i) {
        const uint32_t kShift = 15;
        const uint32_t kMask = 0xFFFFu << kShift;

        uint32_t rPos = uint32_t((c0 >> (32 - kShift)) & kMask);
        uint32_t gPos = uint32_t((c0 >> (16 - kShift)) & kMask);
        uint32_t bPos = uint32_t((c0 << (0  + kShift)) & kMask);

        uint32_t rInc = uint32_t((c1 >> (32 - kShift)) & kMask);
        uint32_t gInc = uint32_t((c1 >> (16 - kShift)) & kMask);
        uint32_t bInc = uint32_t((c1 << (0  + kShift)) & kMask);

        rInc = uint32_t(int32_t(rInc - rPos) / int32_t(i));
        gInc = uint32_t(int32_t(gInc - gPos) / int32_t(i));
        bInc = uint32_t(int32_t(bInc - bPos) / int32_t(i));

        rPos += 1u << (kShift - 1);
        gPos += 1u << (kShift - 1);
        bPos += 1u << (kShift - 1);

        if (RgbaInternal::isRgba64FullyOpaque(c0 & c1)) {
          // Both fully opaque, no need to premultiply.
          do {
            rPos += rInc;
            gPos += gInc;
            bPos += bInc;

            cp = (uint64_t(rPos & kMask) << (32 - kShift)) |
                 (uint64_t(gPos & kMask) << (16 - kShift)) |
                 (uint64_t(bPos & kMask) >> (0  + kShift)) | 0xFFFF000000000000u;

            dSpanPtr[0] = cp;
            dSpanPtr++;
          } while (--i);
        }
        else {
          // One or both having alpha, have to be premultiplied.
          uint32_t aPos = uint32_t((c0 >> (48 - kShift)) & kMask);
          uint32_t aInc = uint32_t((c1 >> (48 - kShift)) & kMask);

          aInc = uint32_t(int32_t(aInc - aPos) / int32_t(i));
          aPos += 1u << (kShift - 1);

          do {
            aPos += aInc;
            rPos += rInc;
            gPos += gInc;
            bPos += bInc;

            uint32_t ca = aPos >> kShift;
            uint32_t cr = Scalar::udiv65535((rPos >> kShift) * ca);
            uint32_t cg = Scalar::udiv65535((gPos >> kShift) * ca);
            uint32_t cb = Scalar::udiv65535((bPos >> kShift) * ca);

            cp = RgbaInternal::packRgba64(cr, cg, cb, ca);
            dSpanPtr[0] = cp;
            dSpanPtr++;
          } while (--i);
        }
      }

      c0 = c1;
    }
  } while (++sIndex < sSize);

  // The last stop doesn't have to end at 1.0, in such case the remaining space is filled by the last color stop
  // (premultiplied). We jump to the main loop instead of filling the buffer here.
  i = uint32_t((size_t)((dPtr + dSize + 1) - dSpanPtr));
  if (i != 0)
    goto SolidInit;

  // The first pixel has to be always set to the first stop's color. The main loop always honors the last color
  // value of the stop colliding with the previous offset index - for example if multiple stops have the same offset
  // [0.0] the first pixel will be the last stop's color. This is easier to fix here as we don't need extra conditions
  // in the main loop.
  dPtr[0] = cpFirst;
}

} // {Interpolation}
} // {PixelOps}
} // {bl}
