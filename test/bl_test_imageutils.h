// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// This file provides utility classes and functions shared between some tests.

#ifndef BLEND2D_TEST_IMAGEUTILS_H_INCLUDED
#define BLEND2D_TEST_IMAGEUTILS_H_INCLUDED

#include <blend2d.h>
#include <stdint.h>

namespace ImageUtils {

struct DiffInfo {
  uint32_t maxDiff;
  uint64_t cumulativeDiff;
};

static DiffInfo diffInfo(const BLImage& aImage, const BLImage& bImage) noexcept {
  DiffInfo info {};
  BLImageData aData;
  BLImageData bData;

  // Used in case of error (image size/format doesn't match).
  info.maxDiff = 0xFFFFFFFFu;

  if (aImage.size() != bImage.size())
    return info;

  size_t w = size_t(aImage.width());
  size_t h = size_t(aImage.height());

  if (aImage.getData(&aData) != BL_SUCCESS)
    return info;

  if (bImage.getData(&bData) != BL_SUCCESS)
    return info;

  if (aData.format != bData.format)
    return info;

  intptr_t aStride = aData.stride;
  intptr_t bStride = bData.stride;

  const uint8_t* aLine = static_cast<const uint8_t*>(aData.pixelData);
  const uint8_t* bLine = static_cast<const uint8_t*>(bData.pixelData);

  info.maxDiff = 0;

  switch (aData.format) {
    case BL_FORMAT_XRGB32:
    case BL_FORMAT_PRGB32: {
      uint32_t mask = aData.format == BL_FORMAT_XRGB32 ? 0xFF000000u : 0x0u;

      for (size_t y = 0; y < h; y++) {
        const uint32_t* aPtr = reinterpret_cast<const uint32_t*>(aLine);
        const uint32_t* bPtr = reinterpret_cast<const uint32_t*>(bLine);

        for (size_t x = 0; x < w; x++) {
          uint32_t aVal = aPtr[x] | mask;
          uint32_t bVal = bPtr[x] | mask;

          if (aVal != bVal) {
            int aDiff = blAbs(int((aVal >> 24) & 0xFF) - int((bVal >> 24) & 0xFF));
            int rDiff = blAbs(int((aVal >> 16) & 0xFF) - int((bVal >> 16) & 0xFF));
            int gDiff = blAbs(int((aVal >>  8) & 0xFF) - int((bVal >>  8) & 0xFF));
            int bDiff = blAbs(int((aVal      ) & 0xFF) - int((bVal      ) & 0xFF));
            int maxDiff = blMax(aDiff, rDiff, gDiff, bDiff);

            info.maxDiff = blMax(info.maxDiff, uint32_t(maxDiff));
            info.cumulativeDiff += maxDiff;
          }
        }

        aLine += aStride;
        bLine += bStride;
      }
      break;
    }

    case BL_FORMAT_A8: {
      for (size_t y = 0; y < h; y++) {
        const uint8_t* aPtr = aLine;
        const uint8_t* bPtr = bLine;

        for (size_t x = 0; x < w; x++) {
          uint32_t aVal = aPtr[x];
          uint32_t bVal = bPtr[x];
          uint32_t diff = uint32_t(blAbs(int(aVal) - int(bVal)));

          info.maxDiff = blMax(info.maxDiff, diff);
          info.cumulativeDiff += diff;
        }

        aLine += aStride;
        bLine += bStride;
      }
      break;
    }

    default: {
      info.maxDiff = 0xFFFFFFFFu;
      break;
    }
  }

  return info;
}

static BLImage diffImage(const BLImage& aImage, const BLImage& bImage) noexcept {
  BLImage result;
  BLImageData rData;
  BLImageData aData;
  BLImageData bData;

  if (aImage.size() != bImage.size())
    return result;

  size_t w = size_t(aImage.width());
  size_t h = size_t(aImage.height());

  if (aImage.getData(&aData) != BL_SUCCESS)
    return result;

  if (bImage.getData(&bData) != BL_SUCCESS)
    return result;

  if (aData.format != bData.format)
    return result;

  if (result.create(w, h, BL_FORMAT_XRGB32) != BL_SUCCESS)
    return result;

  if (result.getData(&rData) != BL_SUCCESS)
    return result;

  intptr_t dStride = rData.stride;
  intptr_t aStride = aData.stride;
  intptr_t bStride = bData.stride;

  uint8_t* dLine = static_cast<uint8_t*>(rData.pixelData);
  const uint8_t* aLine = static_cast<const uint8_t*>(aData.pixelData);
  const uint8_t* bLine = static_cast<const uint8_t*>(bData.pixelData);

  auto&& colorFromDiff = [](uint32_t diff) noexcept -> uint32_t {
    static constexpr uint32_t lowDiff[] = {
      0xFF000000u,
      0xFF0000A0u,
      0xFF0000C0u,
      0xFF0000FFu,
      0xFF0040A0u
    };

    if (diff <= 4u)
      return lowDiff[diff];
    else if (diff <= 16u)
      return 0xFF000000u + unsigned((diff * 16u - 1u) << 8);
    else
      return 0xFF000000u + unsigned((127u + diff / 2u) << 16);
  };

  switch (aData.format) {
    case BL_FORMAT_PRGB32:
    case BL_FORMAT_XRGB32: {
      for (size_t y = 0; y < h; y++) {
        uint32_t* dPtr = reinterpret_cast<uint32_t*>(dLine);
        const uint32_t* aPtr = reinterpret_cast<const uint32_t*>(aLine);
        const uint32_t* bPtr = reinterpret_cast<const uint32_t*>(bLine);

        for (size_t x = 0; x < w; x++) {
          uint32_t aVal = aPtr[x];
          uint32_t bVal = bPtr[x];
          uint32_t diff = uint32_t(blAbs(int(aVal) - int(bVal)));

          uint32_t color = colorFromDiff(diff);
          dPtr[x] = color;
        }

        dLine += dStride;
        aLine += aStride;
        bLine += bStride;
      }
      break;
    }

    case BL_FORMAT_A8: {
      for (size_t y = 0; y < h; y++) {
        uint32_t* dPtr = reinterpret_cast<uint32_t*>(dLine);
        const uint8_t* aPtr = aLine;
        const uint8_t* bPtr = bLine;

        for (size_t x = 0; x < w; x++) {
          uint32_t aVal = aPtr[x];
          uint32_t bVal = bPtr[x];
          int aDiff = blAbs(int((aVal >> 24) & 0xFF) - int((bVal >> 24) & 0xFF));
          int rDiff = blAbs(int((aVal >> 16) & 0xFF) - int((bVal >> 16) & 0xFF));
          int gDiff = blAbs(int((aVal >>  8) & 0xFF) - int((bVal >>  8) & 0xFF));
          int bDiff = blAbs(int((aVal      ) & 0xFF) - int((bVal      ) & 0xFF));

          uint32_t color = colorFromDiff(uint32_t(blMax(aDiff, rDiff, gDiff, bDiff)));
          dPtr[x] = color;
        }

        dLine += dStride;
        aLine += aStride;
        bLine += bStride;
      }
      break;
    }

    default: {
      result.reset();
      break;
    }
  }

  return result;
}

} // {ImageUtils}

#endif // BLEND2D_TEST_IMAGEUTILS_H_INCLUDED
