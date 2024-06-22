// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../array_p.h"
#include "../format_p.h"
#include "../object_p.h"
#include "../rgba.h"
#include "../runtime_p.h"
#include "../codec/qoicodec_p.h"
#include "../codec/qoiops_p.h"
#include "../pixelops/scalar_p.h"
#include "../support/intops_p.h"
#include "../support/memops_p.h"
#include "../support/traits_p.h"

namespace bl {
namespace Qoi {

// bl::Qoi::Codec - Globals
// ========================

static BLObjectEternalVirtualImpl<BLQoiCodecImpl, BLImageCodecVirt> qoiCodec;
static BLImageCodecCore qoiCodecInstance;
static BLImageDecoderVirt qoiDecoderVirt;
static BLImageEncoderVirt qoiEncoderVirt;

static constexpr uint8_t qoiMagic[kQoiMagicSize] = { 'q', 'o', 'i', 'f' };
static constexpr uint8_t qoiEndMarker[kQoiEndMarkerSize] = { 0, 0, 0, 0, 0, 0, 0, 1 };

// bl::Qoi::Decoder - Read Info (Internal)
// =======================================

// struct qoi_header {
//   char magic[4];      // magic bytes "qoif"
//   uint32_t width;     // image width in pixels (BE)
//   uint32_t height;    // image height in pixels (BE)
//   uint8_t channels;   // 3 = RGB, 4 = RGBA
//   uint8_t colorspace; // 0 = sRGB with linear alpha, 1 = all channels linear
// };

static BLResult decoderReadInfoInternal(BLQoiDecoderImpl* decoderI, const uint8_t* data, size_t size) noexcept {
  if (size < kQoiHeaderSize) {
    return blTraceError(BL_ERROR_DATA_TRUNCATED);
  }

  if (memcmp(qoiMagic, data, kQoiMagicSize) != 0) {
    return blTraceError(BL_ERROR_INVALID_SIGNATURE);
  }

  uint32_t w = bl::MemOps::readU32uBE(data + 4);
  uint32_t h = bl::MemOps::readU32uBE(data + 8);

  if (w == 0 || h == 0) {
    return blTraceError(BL_ERROR_INVALID_DATA);
  }

  uint8_t channels = data[12];
  uint8_t colorspace = data[13];

  if (channels != 3u && channels != 4u) {
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
  }

  if (colorspace > 1u) {
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
  }

  if (w > BL_RUNTIME_MAX_IMAGE_SIZE || h > BL_RUNTIME_MAX_IMAGE_SIZE) {
    return blTraceError(BL_ERROR_IMAGE_TOO_LARGE);
  }

  decoderI->bufferIndex = 14u;
  decoderI->imageInfo.reset();
  decoderI->imageInfo.size.reset(int32_t(w), int32_t(h));
  decoderI->imageInfo.depth = uint16_t(channels * 8);
  decoderI->imageInfo.planeCount = 1;
  decoderI->imageInfo.frameCount = 1;

  return BL_SUCCESS;
}

static BL_INLINE uint32_t* fillRgba32(uint32_t* dst, uint32_t value, size_t count) noexcept {
  for (size_t i = 0; i < count; i++)
    dst[i] = value;
  return dst + count;
}

// Compute both DIFF and LUMA deltas and then blend between these two without branching.
static BL_INLINE uint64_t calculateDiffLumaDelta64(uint32_t hbyte0, uint32_t hbyte1) noexcept {
  // QOI_OP_DIFF chunk (0b01xxxxxx) - the bias is the same for RGB channels.
  constexpr uint64_t kDiffBiasRGB = 0x100u - 2u;
  constexpr uint64_t kDiffBiasAll = kDiffBiasRGB * 0x000100010001u;

  // QOI_OP_LUMA chunk (0b10xxxxxx) - the bias is -32 for G channel and -40 for R and B channels.
  constexpr uint64_t kLumaBiasG = (0x100u ^ 0x80u) - 32;
  constexpr uint64_t kLumaBiasRB = (0x100u ^ 0x80u) - 40;
  constexpr uint64_t kLumaBiasAll = kLumaBiasRB * 0x00010001u + (kLumaBiasG << 32);

  // Compute both DIFF and LUMA deltas and then blend between these two without branching.
  uint64_t lumaDelta = (uint32_t(hbyte1) * (1u | (1u << 12))             ) & 0x00000000000F000Fu;
  uint64_t diffDelta = (uint64_t(hbyte0) * (1u | (1u << 12) | (1u << 30))) & 0x0000000300030003u;

  lumaDelta += uint64_t(hbyte0) * 0x0000000100010001u;
  diffDelta += kDiffBiasAll;
  lumaDelta += kLumaBiasAll;

  return hbyte0 < kQoiOpLuma ? diffDelta : lumaDelta;
}

template<bool kHasAlpha>
static BL_INLINE BLResult decodeQoiData(
  uint8_t* dstRow,
  intptr_t dstStride,
  uint32_t w,
  uint32_t h,
  uint32_t pixelTableNP[64],
  uint32_t pixelTablePM[64],
  const uint8_t* src,
  const uint8_t* end) noexcept {

  constexpr size_t kMinRemainingBytesOfNextChunk = kQoiEndMarkerSize + 1u;

  uint32_t* dstPtr = reinterpret_cast<uint32_t*>(dstRow);
  uint32_t* dstEnd = dstPtr + w;
  size_t n = 0;

  BLRgba32 pixelNP = BLRgba32(0xFF000000u);
  uint32_t pixelPM = pixelNP.value;
  uint32_t hash = hashPixelRGBA32(pixelNP);

  // Edge case: If the image starts with QOI_OP_RUN, the repeated pixel must be
  // added to the pixel table, otherwise the decoder may produce incorrect result.
  {
    uint32_t header = src[0];
    if (header >= kQoiOpRun && header < kQoiOpRun + 62u) {
      if BL_CONSTEXPR (kHasAlpha) {
        pixelTableNP[hash] = pixelNP.value;
        pixelTablePM[hash] = pixelPM;
      }
      else {
        pixelTableNP[hash] = pixelNP.value;
      }
    }
  }

  for (;;) {
    size_t remaining = (size_t)(end - src);
    if (BL_UNLIKELY(remaining < kMinRemainingBytesOfNextChunk)) {
      return blTraceError(BL_ERROR_DATA_TRUNCATED);
    }

    uint32_t header = src[0];
    if (header < kQoiOpRun) {
      // QOI_OP_INDEX + QOI_OP_DIFF + QOI_OP_LUMA
      // ========================================

      if (header < 64u) {
        // Handle QOI_OP_INDEX - 6-bit index to a pixel table (header = 0b00xxxxxx).
        pixelNP = BLRgba32(pixelTableNP[header]);
        src++;

        if BL_CONSTEXPR (kHasAlpha) {
          pixelPM = pixelTablePM[header];
          *dstPtr = pixelPM;
        }
        else {
          *dstPtr = pixelNP.value;
        }

        if (++dstPtr != dstEnd)
          continue;
      }
      else {
        // Handle QOI_OP_DIFF and QOI_OP_LUMA chunks.

        // Specialize for 64-bit targets as we can significantly improve the computations if we
        // know we have a native 64-bit arithmetic. Also, we can go fully branchless in 64-bit case.
        if BL_CONSTEXPR (sizeof(uintptr_t) >= sizeof(uint64_t)) {
          uint32_t hbyte1 = src[1];
          src += header >> 6;

          uint64_t delta = calculateDiffLumaDelta64(header, hbyte1);
          uint64_t ag_rb = unpackPixelToAGxRBx64(pixelNP.value);

          ag_rb = (ag_rb + delta) & 0x00FF00FF00FF00FFu;
          hash = hashPixelAGxRBx64(ag_rb);
          pixelNP.value = packPixelFromAGxRBx64(ag_rb);
        }
        else {
          // QOI_OP_DIFF bias is 2, but we can subtract it here.
          constexpr uint32_t kDiffBias = 0x100 - 2;
          constexpr uint32_t kDiffBiasAG = kDiffBias << 8;
          constexpr uint32_t kDiffBiasRB = (kDiffBias << 16) | kDiffBias;

          uint32_t alt = (uint32_t(src[1]) * (1u | (1u << 12))) & 0x000F000Fu;
          src += header >> 6;

          uint32_t ag = (pixelNP.value & 0xFF00FF00u) + kDiffBiasAG;
          uint32_t rb = (pixelNP.value & 0x00FF00FFu) + kDiffBiasRB;

          if (header < kQoiOpLuma) {
            // Handle QOI_OP_DIFF chunk (0b01xxxxxx).
            ag += (0x00000300u & (header << 6));
            rb += (0x00030003u & ((header << 12) | header));
          }
          else {
            // Handle QOI_OP_LUMA chunk (0b10xxxxxx).
            uint32_t ag_base = header + ((256u ^ 0x80u) - (32u - 2u));
            uint32_t rb_base = ag_base - 8u;

            ag += ag_base << 8;
            rb += ((rb_base << 16) | rb_base) + alt;
          }

          ag &= 0xFF00FF00u;
          rb &= 0x00FF00FFu;

          pixelNP.value = ag + rb;
          hash = hashPixelAG_RB(ag, rb);
        }

store_pixel:
        if BL_CONSTEXPR (kHasAlpha) {
          pixelPM = PixelOps::Scalar::cvt_prgb32_8888_from_argb32_8888(pixelNP.value);
          pixelTableNP[hash] = pixelNP.value;
          pixelTablePM[hash] = pixelPM;

          *dstPtr = pixelPM;
        }
        else {
          pixelTableNP[hash] = pixelNP.value;
          *dstPtr = pixelNP.value | 0xFF000000u;
        }

        if (++dstPtr != dstEnd)
          continue;
      }
    }
    else {
      // QOI_OP_RUN + QOI_OP_RGB + QOI_OP_RGBA
      // =====================================

      if (header >= kQoiOpRgb) {
        // Handle both QOI_OP_RGB and QOI_OP_RGBA at the same time.
        BLRgba32 q = BLRgba32(src[1], src[2], src[3], src[4]);
        uint32_t msk = (header + 1) << 24;

        pixelNP.value = (pixelNP.value & msk) | (q.value & ~msk);
        hash = hashPixelRGBA32(pixelNP);

        // Advance by either 4 (RGB) or 5 (RGBA) bytes.
        src += header - 250u;
        goto store_pixel;
      }
      else {
        // Run-length encoding uses a single byte.
        src++;

        // Run-length encoding repeats the previous pixel by (header & 0x3F) + 1 times (N stored with bias of -1).
        n = size_t(header & 0x3Fu) + 1u;

store_rle:
        {
          size_t limit = (size_t)(dstEnd - dstPtr);
          size_t fill = blMin(n, limit);

          n -= fill;

          if BL_CONSTEXPR (kHasAlpha) {
            dstPtr = fillRgba32(dstPtr, pixelPM, fill);
          }
          else {
            dstPtr = fillRgba32(dstPtr, pixelNP.value | 0xFF000000u, fill);
          }

          if (dstPtr != dstEnd)
            continue;
        }
      }
    }

    if (--h == 0) {
      return BL_SUCCESS;
    }

    dstRow += dstStride;
    dstPtr = reinterpret_cast<uint32_t*>(dstRow);
    dstEnd = dstPtr + w;

    if (n != 0) {
      goto store_rle;
    }
  }
}

static BLResult decoderReadFrameInternal(BLQoiDecoderImpl* decoderI, BLImage* imageOut, const uint8_t* data, size_t size) noexcept {
  if (size < kQoiHeaderSize)
    return blTraceError(BL_ERROR_DATA_TRUNCATED);

  const uint8_t* start = data;
  const uint8_t* end = data + size;

  uint32_t w = uint32_t(decoderI->imageInfo.size.w);
  uint32_t h = uint32_t(decoderI->imageInfo.size.h);

  uint32_t depth = decoderI->imageInfo.depth;
  BLFormat format = depth == 32 ? BL_FORMAT_PRGB32 : BL_FORMAT_XRGB32;
  uint32_t rgbaMask = depth == 32 ? 0u : 0xFF000000u;

  data += kQoiHeaderSize;
  if (data >= end)
    return blTraceError(BL_ERROR_DATA_TRUNCATED);

  BLImageData imageData;
  BL_PROPAGATE(imageOut->create(int(w), int(h), format));
  BL_PROPAGATE(imageOut->makeMutable(&imageData));

  uint8_t* dstRow = static_cast<uint8_t*>(imageData.pixelData);
  intptr_t dstStride = imageData.stride;

  uint32_t pixelTableNP[64];
  uint32_t pixelTablePM[64];

  fillRgba32(pixelTableNP, rgbaMask, 64);
  fillRgba32(pixelTablePM, rgbaMask, 64);

  if (depth == 32)
    BL_PROPAGATE(decodeQoiData<true>(dstRow, dstStride, w, h, pixelTableNP, pixelTablePM, data, end));
  else
    BL_PROPAGATE(decodeQoiData<false>(dstRow, dstStride, w, h, pixelTableNP, pixelTablePM, data, end));

  decoderI->bufferIndex = (size_t)(data - start);
  decoderI->frameIndex++;

  return BL_SUCCESS;
}

// bl::Qoi::Decoder - Interface
// ============================

static BLResult BL_CDECL decoderRestartImpl(BLImageDecoderImpl* impl) noexcept {
  BLQoiDecoderImpl* decoderI = static_cast<BLQoiDecoderImpl*>(impl);

  decoderI->lastResult = BL_SUCCESS;
  decoderI->frameIndex = 0;
  decoderI->bufferIndex = 0;
  decoderI->imageInfo.reset();

  return BL_SUCCESS;
}

static BLResult BL_CDECL decoderReadInfoImpl(BLImageDecoderImpl* impl, BLImageInfo* infoOut, const uint8_t* data, size_t size) noexcept {
  BLQoiDecoderImpl* decoderI = static_cast<BLQoiDecoderImpl*>(impl);
  BLResult result = decoderI->lastResult;

  if (decoderI->bufferIndex == 0 && result == BL_SUCCESS) {
    result = decoderReadInfoInternal(decoderI, data, size);
    if (result != BL_SUCCESS)
      decoderI->lastResult = result;
  }

  if (infoOut)
    memcpy(infoOut, &decoderI->imageInfo, sizeof(BLImageInfo));

  return result;
}

static BLResult BL_CDECL decoderReadFrameImpl(BLImageDecoderImpl* impl, BLImageCore* imageOut, const uint8_t* data, size_t size) noexcept {
  BLQoiDecoderImpl* decoderI = static_cast<BLQoiDecoderImpl*>(impl);
  BL_PROPAGATE(decoderReadInfoImpl(decoderI, nullptr, data, size));

  if (decoderI->frameIndex)
    return blTraceError(BL_ERROR_NO_MORE_DATA);

  BLResult result = decoderReadFrameInternal(decoderI, static_cast<BLImage*>(imageOut), data, size);
  if (result != BL_SUCCESS)
    decoderI->lastResult = result;
  return result;
}

static BLResult BL_CDECL decoderCreateImpl(BLImageDecoderCore* self) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_IMAGE_DECODER);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLQoiDecoderImpl>(self, info));

  BLQoiDecoderImpl* decoderI = static_cast<BLQoiDecoderImpl*>(self->_d.impl);
  decoderI->ctor(&qoiDecoderVirt, &qoiCodecInstance);
  return decoderRestartImpl(decoderI);
}

static BLResult BL_CDECL decoderDestroyImpl(BLObjectImpl* impl) noexcept {
  BLQoiDecoderImpl* decoderI = static_cast<BLQoiDecoderImpl*>(impl);

  decoderI->dtor();
  return blObjectFreeImpl(decoderI);
}

// bl::Qoi::Encoder - Interface
// ============================

// QOI is no good for compressing alpha-only images.
static uint8_t* encodeQoiDataA8(uint8_t* dstData, uint32_t w, uint32_t h, const uint8_t* srcLine, intptr_t srcStride) noexcept {
  uint8_t pixel = 0xFFu;
  uint8_t pixelTable[64] {};
  uint32_t runHash = 0;

  do {
    uint32_t x = w;
    const uint8_t* srcData = srcLine;

    do {
      uint8_t p = srcData[0];
      srcData++;

      if (p == pixel) {
        if (runHash < kQoiOpRun) {
          runHash = kQoiOpRun;
          *dstData++ = uint8_t(runHash);
        }
        else {
          dstData[-1] = uint8_t(++runHash);
          if (runHash == kQoiOpRun + 62u - 1u)
            runHash = 0;
        }
      }
      else {
        runHash = hashPixelA8(p);

        if (pixelTable[runHash] == p) {
          *dstData++ = uint8_t(kQoiOpIndex | runHash);
        }
        else {
          pixelTable[runHash] = p;

          dstData[0] = kQoiOpRgba;
          dstData[1] = uint8_t(0xFFu);
          dstData[2] = uint8_t(0xFFu);
          dstData[3] = uint8_t(0xFFu);
          dstData[4] = p;
          dstData += 5;
        }

        pixel = p;
      }
    } while (--x);

    srcLine += srcStride;
  } while (--h);

  return dstData;
}

static uint8_t* encodeQoiDataXRGB32(uint8_t* dstData, uint32_t w, uint32_t h, const uint8_t* srcLine, intptr_t srcStride) noexcept {
  BLRgba32 pixel = BLRgba32(0xFF000000u);
  uint32_t pixelTable[64] {};
  uint32_t runHash = 0;

  do {
    uint32_t x = w;
    const uint8_t* srcData = srcLine;

    do {
      BLRgba32 p = BLRgba32(MemOps::readU32a(srcData) | 0xFF000000u);
      srcData += 4;

      if (p == pixel) {
        if (runHash < kQoiOpRun) {
          runHash = kQoiOpRun;
          *dstData++ = uint8_t(runHash);
        }
        else {
          dstData[-1] = uint8_t(++runHash);
          if (runHash == kQoiOpRun + 62u - 1u)
            runHash = 0;
        }
      }
      else {
        runHash = hashPixelRGB32(p);

        if (pixelTable[runHash] == p.value) {
          *dstData++ = uint8_t(kQoiOpIndex | runHash);
        }
        else {
          pixelTable[runHash] = p.value;

          int32_t dr = int32_t(p.r()) - int32_t(pixel.r());
          int32_t dg = int32_t(p.g()) - int32_t(pixel.g());
          int32_t db = int32_t(p.b()) - int32_t(pixel.b());

          uint32_t xr = uint32_t(dr + 2);
          uint32_t xg = uint32_t(dg + 2);
          uint32_t xb = uint32_t(db + 2);

          if ((xr | xg | xb) <= 0x3u) {
            *dstData++ = uint8_t(kQoiOpDiff | (xr << 4) | (xg << 2) | xb);
          }
          else {
            int32_t dg_r = dr - dg;
            int32_t dg_b = db - dg;

            xr = uint32_t(dg_r + 8);
            xg = uint32_t(dg + 32);
            xb = uint32_t(dg_b + 8);

            if ((xr | xb) <= 0xFu && xg <= 0x3F) {
              dstData[0] = uint8_t(kQoiOpLuma | xg);
              dstData[1] = uint8_t((xr << 4) | xb);
              dstData += 2;
            }
            else {
              dstData[0] = kQoiOpRgb;
              dstData[1] = uint8_t(p.r());
              dstData[2] = uint8_t(p.g());
              dstData[3] = uint8_t(p.b());
              dstData += 4;
            }
          }
        }

        pixel = p;
      }
    } while (--x);

    srcLine += srcStride;
  } while (--h);

  return dstData;
}

static uint8_t* encodeQoiDataPRGB32(uint8_t* dstData, uint32_t w, uint32_t h, const uint8_t* srcLine, intptr_t srcStride) noexcept {
  BLRgba32 pixel = BLRgba32(0xFF000000u);
  uint32_t pixelTable[64] {};
  uint32_t runHash = 0;

  do {
    uint32_t x = w;
    const uint8_t* srcData = srcLine;

    do {
      BLRgba32 p = BLRgba32(PixelOps::Scalar::cvt_argb32_8888_from_prgb32_8888(MemOps::readU32a(srcData)));
      srcData += 4;

      if (p == pixel) {
        if (runHash < kQoiOpRun) {
          runHash = kQoiOpRun;
          *dstData++ = uint8_t(runHash);
        }
        else {
          dstData[-1] = uint8_t(++runHash);
          if (runHash == kQoiOpRun + 62u - 1u)
            runHash = 0;
        }
      }
      else {
        runHash = hashPixelRGBA32(p);

        if (pixelTable[runHash] == p.value) {
          *dstData++ = uint8_t(kQoiOpIndex | runHash);
        }
        else {
          pixelTable[runHash] = p.value;

          // To use delta, the previous pixel needs to have the same alpha value unfortunately.
          if (pixel.a() == p.a()) {
            int32_t dr = int32_t(p.r()) - int32_t(pixel.r());
            int32_t dg = int32_t(p.g()) - int32_t(pixel.g());
            int32_t db = int32_t(p.b()) - int32_t(pixel.b());

            uint32_t xr = uint32_t(dr + 2);
            uint32_t xg = uint32_t(dg + 2);
            uint32_t xb = uint32_t(db + 2);

            if ((xr | xg | xb) <= 0x3u) {
              *dstData++ = uint8_t(kQoiOpDiff | (xr << 4) | (xg << 2) | xb);
            }
            else {
              int32_t dg_r = dr - dg;
              int32_t dg_b = db - dg;

              xr = uint32_t(dg_r + 8);
              xg = uint32_t(dg + 32);
              xb = uint32_t(dg_b + 8);

              if ((xr | xb) <= 0xFu && xg <= 0x3Fu) {
                dstData[0] = uint8_t(kQoiOpLuma | xg);
                dstData[1] = uint8_t((xr << 4) | xb);
                dstData += 2;
              }
              else {
                dstData[0] = kQoiOpRgb;
                dstData[1] = uint8_t(p.r());
                dstData[2] = uint8_t(p.g());
                dstData[3] = uint8_t(p.b());
                dstData += 4;
              }
            }
          }
          else {
            dstData[0] = kQoiOpRgba;
            dstData[1] = uint8_t(p.r());
            dstData[2] = uint8_t(p.g());
            dstData[3] = uint8_t(p.b());
            dstData[4] = uint8_t(p.a());
            dstData += 5;
          }
        }

        pixel = p;
      }
    } while (--x);

    srcLine += srcStride;
  } while (--h);

  return dstData;
}

static BLResult BL_CDECL encoderRestartImpl(BLImageEncoderImpl* impl) noexcept {
  BLQoiEncoderImpl* encoderI = static_cast<BLQoiEncoderImpl*>(impl);
  encoderI->lastResult = BL_SUCCESS;
  encoderI->frameIndex = 0;
  encoderI->bufferIndex = 0;
  return BL_SUCCESS;
}

static BLResult BL_CDECL encoderWriteFrameImpl(BLImageEncoderImpl* impl, BLArrayCore* dst, const BLImageCore* image) noexcept {
  BLQoiEncoderImpl* encoderI = static_cast<BLQoiEncoderImpl*>(impl);
  BL_PROPAGATE(encoderI->lastResult);

  BLArray<uint8_t>& buf = *static_cast<BLArray<uint8_t>*>(dst);
  const BLImage& img = *static_cast<const BLImage*>(image);

  if (img.empty())
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLImageData imageData;
  BL_PROPAGATE(img.getData(&imageData));

  uint32_t w = uint32_t(imageData.size.w);
  uint32_t h = uint32_t(imageData.size.h);
  uint32_t format = imageData.format;

  uint32_t channels = format == BL_FORMAT_XRGB32 ? 3u : 4u;
  uint32_t maxBytesPerEncodedPixel = channels + 1u;

  // NOTE: This should never overflow.
  uint64_t maxSize = uint64_t(w) * uint64_t(h) * uint64_t(maxBytesPerEncodedPixel) + kQoiHeaderSize + kQoiEndMarkerSize;

  if (BL_UNLIKELY(maxSize >= uint64_t(SIZE_MAX)))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  uint8_t* dstData;
  BL_PROPAGATE(buf.modifyOp(BL_MODIFY_OP_ASSIGN_FIT, size_t(maxSize), &dstData));

  memcpy(dstData, qoiMagic, kQoiMagicSize);
  MemOps::writeU32uBE(dstData + 4, w);
  MemOps::writeU32uBE(dstData + 8, h);
  dstData[12] = uint8_t(channels);
  dstData[13] = 0;
  dstData += 14;

  const uint8_t* srcLine = static_cast<const uint8_t*>(imageData.pixelData);

  switch (format) {
    case BL_FORMAT_A8:
      dstData = encodeQoiDataA8(dstData, w, h, srcLine, imageData.stride);
      break;

    case BL_FORMAT_XRGB32:
      dstData = encodeQoiDataXRGB32(dstData, w, h, srcLine, imageData.stride);
      break;

    case BL_FORMAT_PRGB32:
      dstData = encodeQoiDataPRGB32(dstData, w, h, srcLine, imageData.stride);
      break;

    default:
      ArrayInternal::setSize(dst, 0);
      return blTraceError(BL_ERROR_INVALID_STATE);
  }

  memcpy(dstData, qoiEndMarker, kQoiEndMarkerSize);
  dstData += kQoiEndMarkerSize;

  ArrayInternal::setSize(dst, (size_t)(dstData - buf.data()));
  return BL_SUCCESS;
}

static BLResult BL_CDECL encoderCreateImpl(BLImageEncoderCore* self) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_IMAGE_ENCODER);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLQoiEncoderImpl>(self, info));

  BLQoiEncoderImpl* encoderI = static_cast<BLQoiEncoderImpl*>(self->_d.impl);
  encoderI->ctor(&qoiEncoderVirt, &qoiCodecInstance);
  return encoderRestartImpl(encoderI);
}

static BLResult BL_CDECL encoderDestroyImpl(BLObjectImpl* impl) noexcept {
  BLQoiEncoderImpl* encoderI = static_cast<BLQoiEncoderImpl*>(impl);

  encoderI->dtor();
  return blObjectFreeImpl(encoderI);
}

// bl::Qoi::Codec - Interface
// ==========================

static BLResult BL_CDECL codecDestroyImpl(BLObjectImpl* impl) noexcept {
  // Built-in codecs are never destroyed.
  blUnused(impl);
  return BL_SUCCESS;
};

static uint32_t BL_CDECL codecInspectDataImpl(const BLImageCodecImpl* impl, const uint8_t* data, size_t size) noexcept {
  blUnused(impl);

  if (size == 0)
    return 0;

  size_t magicSize = blMin<size_t>(size, kQoiMagicSize);
  if (memcmp(qoiMagic, data, magicSize) != 0)
    return 0;

  if (size < 12)
    return uint32_t(magicSize);

  uint32_t w = bl::MemOps::readU32uBE(data + 4);
  uint32_t h = bl::MemOps::readU32uBE(data + 8);

  if (w == 0 || h == 0)
    return 0;

  if (size < 14)
    return uint32_t(magicSize + 1u);

  uint8_t channels = data[12];
  uint8_t colorspace = data[13];

  if (channels != 3u && channels != 4u)
    return 0;

  if (colorspace > 1u)
    return 0;

  // A valid QOI header.
  return 100;
}

static BLResult BL_CDECL codecCreateDecoderImpl(const BLImageCodecImpl* impl, BLImageDecoderCore* dst) noexcept {
  blUnused(impl);

  BLImageDecoderCore tmp;
  BL_PROPAGATE(decoderCreateImpl(&tmp));
  return blImageDecoderAssignMove(dst, &tmp);
}

static BLResult BL_CDECL codecCreateEncoderImpl(const BLImageCodecImpl* impl, BLImageEncoderCore* dst) noexcept {
  blUnused(impl);

  BLImageEncoderCore tmp;
  BL_PROPAGATE(encoderCreateImpl(&tmp));
  return blImageEncoderAssignMove(dst, &tmp);
}

// bl::Qoi::Codec - Runtime Registration
// =====================================

void qoiCodecOnInit(BLRuntimeContext* rt, BLArray<BLImageCodec>* codecs) noexcept {
  using namespace bl::Qoi;

  blUnused(rt);

  // Initialize QOI codec.
  qoiCodec.virt.base.destroy = codecDestroyImpl;
  qoiCodec.virt.base.getProperty = blObjectImplGetProperty;
  qoiCodec.virt.base.setProperty = blObjectImplSetProperty;
  qoiCodec.virt.inspectData = codecInspectDataImpl;
  qoiCodec.virt.createDecoder = codecCreateDecoderImpl;
  qoiCodec.virt.createEncoder = codecCreateEncoderImpl;

  qoiCodec.impl->ctor(&qoiCodec.virt);
  qoiCodec.impl->features = BLImageCodecFeatures(BL_IMAGE_CODEC_FEATURE_READ     |
                                                 BL_IMAGE_CODEC_FEATURE_WRITE    |
                                                 BL_IMAGE_CODEC_FEATURE_LOSSLESS);
  qoiCodec.impl->name.dcast().assign("QOI");
  qoiCodec.impl->vendor.dcast().assign("Blend2D");
  qoiCodec.impl->mimeType.dcast().assign("image/qoi");
  qoiCodec.impl->extensions.dcast().assign("qoi");

  qoiCodecInstance._d.initDynamic(BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_IMAGE_CODEC), &qoiCodec.impl);

  // Initialize QOI decoder virtual functions.
  qoiDecoderVirt.base.destroy = decoderDestroyImpl;
  qoiDecoderVirt.base.getProperty = blObjectImplGetProperty;
  qoiDecoderVirt.base.setProperty = blObjectImplSetProperty;
  qoiDecoderVirt.restart = decoderRestartImpl;
  qoiDecoderVirt.readInfo = decoderReadInfoImpl;
  qoiDecoderVirt.readFrame = decoderReadFrameImpl;

  // Initialize QOI encoder virtual functions.
  qoiEncoderVirt.base.destroy = encoderDestroyImpl;
  qoiEncoderVirt.base.getProperty = blObjectImplGetProperty;
  qoiEncoderVirt.base.setProperty = blObjectImplSetProperty;
  qoiEncoderVirt.restart = encoderRestartImpl;
  qoiEncoderVirt.writeFrame = encoderWriteFrameImpl;

  codecs->append(qoiCodecInstance.dcast());
}

} // {Qoi}
} // {bl}
