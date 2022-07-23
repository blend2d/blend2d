// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../format_p.h"
#include "../imagecodec.h"
#include "../object_p.h"
#include "../pixelconverter.h"
#include "../rgba.h"
#include "../runtime_p.h"
#include "../codec/bmpcodec_p.h"
#include "../support/intops_p.h"
#include "../support/memops_p.h"
#include "../support/traits_p.h"

// BLBmpCodecImpl - Globals
// ========================

static BLImageCodecCore blBmpCodecObject;
static BLObjectEthernalVirtualImpl<BLBmpCodecImpl, BLImageCodecVirt> blBmpCodec;
static BLImageDecoderVirt blBmpDecoderVirt;
static BLImageEncoderVirt blBmpEncoderVirt;

// BLBmpDecoderImpl - Utilities
// ============================

static bool blBmpCheckHeaderSize(uint32_t headerSize) noexcept {
  return headerSize == BL_BMP_HEADER_SIZE_OS2_V1 ||
         headerSize == BL_BMP_HEADER_SIZE_WIN_V1 ||
         headerSize == BL_BMP_HEADER_SIZE_WIN_V2 ||
         headerSize == BL_BMP_HEADER_SIZE_WIN_V3 ||
         headerSize == BL_BMP_HEADER_SIZE_WIN_V4 ||
         headerSize == BL_BMP_HEADER_SIZE_WIN_V5 ;
}

static bool blBmpCheckDepth(uint32_t depth) noexcept {
  return depth ==  1 ||
         depth ==  4 ||
         depth ==  8 ||
         depth == 16 ||
         depth == 24 ||
         depth == 32 ;
}

static bool blBmpCheckImageSize(const BLSizeI& size) noexcept {
  return uint32_t(size.w) <= BL_RUNTIME_MAX_IMAGE_SIZE &&
         uint32_t(size.h) <= BL_RUNTIME_MAX_IMAGE_SIZE ;
}

static bool blBmpCheckBitMasks(const uint32_t* masks, uint32_t n) noexcept {
  uint32_t combined = 0;
  for (uint32_t i = 0; i < n; i++) {
    uint32_t m = masks[i];

    // RGB masks can't be zero.
    if (m == 0 && i != 3)
      return false;

    // Mask has to have consecutive bits set, masks like 000110011 are not allowed.
    if (m != 0 && !BLIntOps::isBitMaskConsecutive(m))
      return false;

    // Mask can't overlap with other.
    if ((combined & m) != 0)
      return false;

    combined |= m;
  }
  return true;
}

// BLBmpDecoderImpl - RLE4
// =======================

static BLResult blBmpDecodeRLE4(uint8_t* dstLine, intptr_t dstStride, const uint8_t* p, size_t size, uint32_t w, uint32_t h, const BLRgba32* pal) noexcept {
  uint8_t* dstData = dstLine;
  const uint8_t* end = p + size;

  uint32_t x = 0;
  uint32_t y = 0;

  for (;;) {
    if ((size_t)(end - p) < 2)
      return blTraceError(BL_ERROR_DATA_TRUNCATED);

    uint32_t b0 = p[0];
    uint32_t b1 = p[1]; p += 2;

    if (b0 != 0) {
      // RLE_FILL (b0 = Size, b1 = Pattern).
      uint32_t c0 = pal[b1 >> 4].value;
      uint32_t c1 = pal[b1 & 15].value;

      uint32_t i = blMin<uint32_t>(b0, w - x);
      for (x += i; i >= 2; i -= 2, dstData += 8) {
        BLMemOps::writeU32a(dstData + 0, c0);
        BLMemOps::writeU32a(dstData + 4, c1);
      }

      if (i) {
        BLMemOps::writeU32a(dstData + 0, c0);
        dstData += 4;
      }
    }
    else if (b1 >= BL_BMP_RLE_CMD_COUNT) {
      // Absolute (b1 = Size).
      uint32_t i = blMin<uint32_t>(b1, w - x);
      uint32_t reqBytes = ((b1 + 3u) >> 1) & ~uint32_t(0x1);

      if ((size_t)(end - p) < reqBytes)
        return blTraceError(BL_ERROR_DATA_TRUNCATED);

      for (x += i; i >= 4; i -= 4, dstData += 16) {
        b0 = p[0];
        b1 = p[1]; p += 2;

        BLMemOps::writeU32a(dstData +  0, pal[b0 >> 4].value);
        BLMemOps::writeU32a(dstData +  4, pal[b0 & 15].value);
        BLMemOps::writeU32a(dstData +  8, pal[b1 >> 4].value);
        BLMemOps::writeU32a(dstData + 12, pal[b1 & 15].value);
      }

      if (i) {
        b0 = p[0];
        b1 = p[1]; p += 2;

        BLMemOps::writeU32a(dstData, pal[b0 >> 4].value);
        dstData += 4;

        if (--i) {
          BLMemOps::writeU32a(dstData, pal[b0 & 15].value);
          dstData += 4;

          if (--i) {
            BLMemOps::writeU32a(dstData, pal[b1 >> 4].value);
            dstData += 4;
          }
        }
      }
    }
    else {
      // RLE_SKIP (fill by a background pixel).
      uint32_t toX = x;
      uint32_t toY = y;

      if (b1 == BL_BMP_RLE_CMD_LINE) {
        toX = 0;
        toY++;
      }
      else if (b1 == BL_BMP_RLE_CMD_MOVE) {
        if ((size_t)(end - p) < 2)
          return blTraceError(BL_ERROR_DATA_TRUNCATED);

        toX += p[0];
        toY += p[1]; p += 2;

        if (toX > w || toY > h)
          return blTraceError(BL_ERROR_DECOMPRESSION_FAILED);
      }
      else {
        toX = 0;
        toY = h;
      }

      for (; y < toY; y++) {
        for (x = w - x; x; x--, dstData += 4) {
          BLMemOps::writeU32a(dstData, BL_BMP_RLE_BACKGROUND);
        }

        dstLine += dstStride;
        dstData = dstLine;
      }

      for (; x < toX; x++, dstData += 4) {
        BLMemOps::writeU32a(dstData, BL_BMP_RLE_BACKGROUND);
      }

      if (b1 == BL_BMP_RLE_CMD_STOP || y == h)
        return BL_SUCCESS;
    }
  }
}

// BLBmpDecoderImpl - RLE8
// =======================

static BLResult blBmpDecodeRLE8(uint8_t* dstLine, intptr_t dstStride, const uint8_t* p, size_t size, uint32_t w, uint32_t h, const BLRgba32* pal) noexcept {
  uint8_t* dstData = dstLine;
  const uint8_t* end = p + size;

  uint32_t x = 0;
  uint32_t y = 0;

  for (;;) {
    if ((size_t)(end - p) < 2)
      return blTraceError(BL_ERROR_DATA_TRUNCATED);

    uint32_t b0 = p[0];
    uint32_t b1 = p[1]; p += 2;

    if (b0 != 0) {
      // RLE_FILL (b0 = Size, b1 = Pattern).
      uint32_t c0 = pal[b1].value;
      uint32_t i = blMin<uint32_t>(b0, w - x);

      for (x += i; i; i--, dstData += 4) {
        BLMemOps::writeU32a(dstData, c0);
      }
    }
    else if (b1 >= BL_BMP_RLE_CMD_COUNT) {
      // Absolute (b1 = Size).
      uint32_t i = blMin<uint32_t>(b1, w - x);
      uint32_t reqBytes = ((b1 + 1) >> 1) << 1;

      if ((size_t)(end - p) < reqBytes)
        return blTraceError(BL_ERROR_DATA_TRUNCATED);

      for (x += i; i >= 2; i -= 2, dstData += 8) {
        b0 = p[0];
        b1 = p[1]; p += 2;

        BLMemOps::writeU32a(dstData + 0, pal[b0].value);
        BLMemOps::writeU32a(dstData + 4, pal[b1].value);
      }

      if (i) {
        b0 = p[0]; p += 2;

        BLMemOps::writeU32a(dstData, pal[b0].value);
        dstData += 4;
      }
    }
    else {
      // RLE_SKIP (fill by a background pixel).
      uint32_t toX = x;
      uint32_t toY = y;

      if (b1 == BL_BMP_RLE_CMD_LINE) {
        toX = 0;
        toY++;
      }
      else if (b1 == BL_BMP_RLE_CMD_MOVE) {
        if ((size_t)(end - p) < 2)
          return blTraceError(BL_ERROR_DATA_TRUNCATED);

        toX += p[0];
        toY += p[1]; p += 2;

        if (toX > w || toY > h)
          return blTraceError(BL_ERROR_DECOMPRESSION_FAILED);
      }
      else {
        toX = 0;
        toY = h;
      }

      for (; y < toY; y++) {
        for (x = w - x; x; x--, dstData += 4) {
          BLMemOps::writeU32a(dstData, BL_BMP_RLE_BACKGROUND);
        }

        dstLine += dstStride;
        dstData = dstLine;
      }

      for (; x < toX; x++, dstData += 4) {
        BLMemOps::writeU32a(dstData, BL_BMP_RLE_BACKGROUND);
      }

      if (b1 == BL_BMP_RLE_CMD_STOP || y == h)
        return BL_SUCCESS;
    }
  }
}

// BLBmpDecoderImpl - Read Internal
// ================================

static BLResult blBmpDecoderImplReadInfoInternal(BLBmpDecoderImpl* decoderI, const uint8_t* data, size_t size) noexcept {
  // Signature + BmpFile header + BmpInfo header size (18 bytes total).
  const size_t kBmpMinSize = 2 + 12 + 4;
  if (size < kBmpMinSize)
    return blTraceError(BL_ERROR_DATA_TRUNCATED);

  // Read BMP file signature.
  if (data[0] != 'B' || data[1] != 'M')
    return blTraceError(BL_ERROR_INVALID_SIGNATURE);

  const uint8_t* start = data;
  const uint8_t* end = data + size;

  // Read BMP file header.
  memcpy(&decoderI->file, data + 2, 12);
  data += 2 + 12;
  if (BL_BYTE_ORDER_NATIVE != BL_BYTE_ORDER_LE) decoderI->file.byteSwap();

  // First check if the header is supported by the decoder.
  uint32_t headerSize = BLMemOps::readU32uLE(data);
  uint32_t fileAndInfoHeaderSize = 14 + headerSize;

  if (!blBmpCheckHeaderSize(headerSize))
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

  // Read BMP info header.
  if ((size_t)(end - data) < headerSize)
    return blTraceError(BL_ERROR_DATA_TRUNCATED);

  memcpy(&decoderI->info, data, headerSize);
  data += headerSize;

  int32_t w, h;
  uint32_t depth;
  uint32_t planeCount;
  uint32_t compression = BL_BMP_COMPRESSION_RGB;
  bool rleUsed = false;

  if (headerSize == BL_BMP_HEADER_SIZE_OS2_V1) {
    // Handle OS/2 BMP.
    if (BL_BYTE_ORDER_NATIVE != BL_BYTE_ORDER_LE) decoderI->info.os2.byteSwap();

    w = decoderI->info.os2.width;
    h = decoderI->info.os2.height;
    depth = decoderI->info.os2.bitsPerPixel;
    planeCount = decoderI->info.os2.planes;

    // Convert to Windows BMP, there is no difference except the header.
    decoderI->info.win.width = w;
    decoderI->info.win.height = h;
    decoderI->info.win.planes = uint16_t(planeCount);
    decoderI->info.win.bitsPerPixel = uint16_t(depth);
    decoderI->info.win.compression = compression;
  }
  else {
    // Handle Windows BMP.
    if (BL_BYTE_ORDER_NATIVE != BL_BYTE_ORDER_LE) decoderI->info.win.byteSwap();

    w = decoderI->info.win.width;
    h = decoderI->info.win.height;
    depth = decoderI->info.win.bitsPerPixel;
    planeCount = decoderI->info.win.planes;
    compression = decoderI->info.win.compression;
  }

  // Verify whether input data is ok.
  if (h == BLTraits::minValue<int32_t>() || w <= 0)
    return blTraceError(BL_ERROR_INVALID_DATA);

  if (planeCount != 1)
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

  if (h < 0)
    h = -h;

  decoderI->imageInfo.size.reset(w, h);
  decoderI->imageInfo.depth = uint16_t(depth);
  decoderI->imageInfo.planeCount = uint16_t(planeCount);
  decoderI->imageInfo.frameCount = 1;

  // Check if the compression field is correct when depth <= 8.
  if (compression != BL_BMP_COMPRESSION_RGB) {
    if (depth <= 8) {
      rleUsed = (depth == 4 && compression == BL_BMP_COMPRESSION_RLE4) |
                (depth == 8 && compression == BL_BMP_COMPRESSION_RLE8) ;

      if (!rleUsed)
        return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
    }
  }

  if (decoderI->file.imageOffset < fileAndInfoHeaderSize)
    return blTraceError(BL_ERROR_INVALID_DATA);

  // Check if the size is valid.
  if (!blBmpCheckImageSize(decoderI->imageInfo.size))
    return blTraceError(BL_ERROR_IMAGE_TOO_LARGE);

  // Check if the depth is valid.
  if (!blBmpCheckDepth(decoderI->imageInfo.depth))
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

  // Calculate a stride aligned to 32 bits.
  BLOverflowFlag of = 0;
  uint64_t stride = (((uint64_t(w) * uint64_t(depth) + 7u) / 8u) + 3u) & ~uint32_t(3);
  uint32_t imageSize = BLIntOps::mulOverflow(uint32_t(decoderI->stride & 0xFFFFFFFFu), uint32_t(h), &of);

  if (stride >= BLTraits::maxValue<uint32_t>() || of)
    return blTraceError(BL_ERROR_INVALID_DATA);

  decoderI->stride = uint32_t(stride);

  // 1. OS/2 format doesn't specify imageSize, it's always calculated.
  // 2. BMP allows `imageSize` to be zero in case of uncompressed bitmaps.
  if (headerSize == BL_BMP_HEADER_SIZE_OS2_V1 || (decoderI->info.win.imageSize == 0 && !rleUsed))
    decoderI->info.win.imageSize = imageSize;

  // Check if the `imageSize` matches the calculated one. It's malformed if it doesn't.
  if (!rleUsed && decoderI->info.win.imageSize < imageSize)
    return blTraceError(BL_ERROR_INVALID_DATA);

  decoderI->fmt.depth = depth;
  if (depth <= 8) {
    decoderI->fmt.flags = BL_FORMAT_FLAG_RGB | BL_FORMAT_FLAG_INDEXED;
  }
  else {
    decoderI->fmt.flags = BL_FORMAT_FLAG_RGB;

    if (BL_BYTE_ORDER_NATIVE != BL_BYTE_ORDER_LE)
      decoderI->fmt.flags |= BL_FORMAT_FLAG_BYTE_SWAP;

    if (depth == 16) {
      decoderI->fmt.rSize = 5; decoderI->fmt.rShift = 10;
      decoderI->fmt.gSize = 5; decoderI->fmt.gShift = 5;
      decoderI->fmt.bSize = 5; decoderI->fmt.bShift = 0;
    }

    if (depth == 24 || depth == 32) {
      decoderI->fmt.rSize = 8; decoderI->fmt.rShift = 16;
      decoderI->fmt.gSize = 8; decoderI->fmt.gShift = 8;
      decoderI->fmt.bSize = 8; decoderI->fmt.bShift = 0;
    }
  }

  bool hasBitFields = depth > 8 && headerSize >= BL_BMP_HEADER_SIZE_WIN_V2;
  if (headerSize == BL_BMP_HEADER_SIZE_WIN_V1) {
    // Use BITFIELDS if specified.
    if (compression == BL_BMP_COMPRESSION_BIT_FIELDS || compression == BL_BMP_COMPRESSION_ALPHA_BIT_FIELDS) {
      uint32_t channels = 3 + (compression == BL_BMP_COMPRESSION_ALPHA_BIT_FIELDS);
      if (depth != 16 && depth != 32)
        return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

      if ((size_t)(end - data) < channels * 4)
        return blTraceError(BL_ERROR_DATA_TRUNCATED);

      for (uint32_t i = 0; i < channels; i++)
        decoderI->info.win.masks[i] = BLMemOps::readU32uLE(data + i * 4);

      hasBitFields = true;
      data += channels * 4;
    }
  }

  if (hasBitFields) {
    // BitFields provided by info header must be continuous and non-overlapping.
    if (!blBmpCheckBitMasks(decoderI->info.win.masks, 4))
      return blTraceError(BL_ERROR_INVALID_DATA);

    blFormatInfoAssignAbsoluteMasks(decoderI->fmt, decoderI->info.win.masks);
    if (decoderI->info.win.aMask)
      decoderI->fmt.flags |= BL_FORMAT_FLAG_ALPHA | BL_FORMAT_FLAG_PREMULTIPLIED;
  }

  decoderI->bufferIndex = (size_t)(data - start);
  return BL_SUCCESS;
}

static BLResult blBmpDecoderImplReadFrameInternal(BLBmpDecoderImpl* decoderI, BLImage* imageOut, const uint8_t* data, size_t size) noexcept {
  uint32_t result = BL_SUCCESS;
  const uint8_t* start = data;
  const uint8_t* end = data + size;

  // BLImage info.
  uint32_t w = uint32_t(decoderI->imageInfo.size.w);
  uint32_t h = uint32_t(decoderI->imageInfo.size.h);

  BLFormat format = decoderI->fmt.sizes[3] ? BL_FORMAT_PRGB32 : BL_FORMAT_XRGB32;
  uint32_t depth = decoderI->imageInfo.depth;
  uint32_t fileAndInfoHeaderSize = 14 + decoderI->info.headerSize;

  if (size < fileAndInfoHeaderSize)
    return blTraceError(BL_ERROR_DATA_TRUNCATED);

  // Palette.
  BLRgba32 pal[256];
  uint32_t palSize;

  if (depth <= 8) {
    const uint8_t* pPal = data + fileAndInfoHeaderSize;
    palSize = decoderI->file.imageOffset - fileAndInfoHeaderSize;

    uint32_t palEntitySize = decoderI->info.headerSize == BL_BMP_HEADER_SIZE_OS2_V1 ? 3 : 4;
    uint32_t palBytesTotal;

    palSize = blMin<uint32_t>(palSize / palEntitySize, 256);
    palBytesTotal = palSize * palEntitySize;

    if ((size_t)(end - pPal) < palBytesTotal)
      return blTraceError(BL_ERROR_DATA_TRUNCATED);

    // Stored as BGR|BGR (OS/2) or BGRX|BGRX (Windows).
    uint32_t i = 0;
    while (i < palSize) {
      pal[i++] = BLRgba32(pPal[2], pPal[1], pPal[0], 0xFF);
      pPal += palEntitySize;
    }

    // All remaining entries should be opaque black.
    while (i < 256) {
      pal[i++] = BLRgba32(0, 0, 0, 0xFF);
    }
  }

  // Move the cursor to the beginning of the image data and check if the whole
  // image content specified by `info.win.imageSize` is present in the buffer.
  if (size - decoderI->file.imageOffset < decoderI->info.win.imageSize)
    return blTraceError(BL_ERROR_DATA_TRUNCATED);
  data += decoderI->file.imageOffset;

  // Make sure that the destination image has the correct pixel format and size.
  result = imageOut->create(int(w), int(h), format);
  if (result != BL_SUCCESS)
    return result;

  BLImageData imageData;
  result = imageOut->makeMutable(&imageData);
  if (result != BL_SUCCESS)
    return result;

  uint8_t* dstLine = static_cast<uint8_t*>(imageData.pixelData);
  intptr_t dstStride = imageData.stride;

  // Flip vertically.
  if (decoderI->info.win.height > 0) {
    dstLine += (h - 1) * dstStride;
    dstStride = -dstStride;
  }

  // Decode.
  if (depth == 4 && decoderI->info.win.compression == BL_BMP_COMPRESSION_RLE4) {
    BL_PROPAGATE(blBmpDecodeRLE4(dstLine, dstStride, data, decoderI->info.win.imageSize, w, h, pal));
  }
  else if (depth == 8 && decoderI->info.win.compression == BL_BMP_COMPRESSION_RLE8) {
    BL_PROPAGATE(blBmpDecodeRLE8(dstLine, dstStride, data, decoderI->info.win.imageSize, w, h, pal));
  }
  else {
    BLPixelConverter pc;
    BL_PROPAGATE(pc.create(blFormatInfo[format], decoderI->fmt,
      BLPixelConverterCreateFlags(
        BL_PIXEL_CONVERTER_CREATE_FLAG_DONT_COPY_PALETTE |
        BL_PIXEL_CONVERTER_CREATE_FLAG_ALTERABLE_PALETTE)));
    pc.convertRect(dstLine, dstStride, data, decoderI->stride, w, h);
  }

  decoderI->bufferIndex = (size_t)(data - start);
  decoderI->frameIndex++;
  return BL_SUCCESS;
}

// BLBmpDecoderImpl - Interface
// ============================

static BLResult BL_CDECL blBmpDecoderImplRestart(BLImageDecoderImpl* impl) noexcept {
  BLBmpDecoderImpl* decoderI = static_cast<BLBmpDecoderImpl*>(impl);

  decoderI->lastResult = BL_SUCCESS;
  decoderI->frameIndex = 0;
  decoderI->bufferIndex = 0;
  decoderI->imageInfo.reset();
  decoderI->file.reset();
  decoderI->info.reset();
  decoderI->fmt.reset();
  decoderI->stride = 0;

  return BL_SUCCESS;
}

static BLResult BL_CDECL blBmpDecoderImplReadInfo(BLImageDecoderImpl* impl, BLImageInfo* infoOut, const uint8_t* data, size_t size) noexcept {
  BLBmpDecoderImpl* decoderI = static_cast<BLBmpDecoderImpl*>(impl);
  BLResult result = decoderI->lastResult;

  if (decoderI->bufferIndex == 0 && result == BL_SUCCESS) {
    result = blBmpDecoderImplReadInfoInternal(decoderI, data, size);
    if (result != BL_SUCCESS)
      decoderI->lastResult = result;
  }

  if (infoOut)
    memcpy(infoOut, &decoderI->imageInfo, sizeof(BLImageInfo));

  return result;
}

static BLResult BL_CDECL blBmpDecoderImplReadFrame(BLImageDecoderImpl* impl, BLImageCore* imageOut, const uint8_t* data, size_t size) noexcept {
  BLBmpDecoderImpl* decoderI = static_cast<BLBmpDecoderImpl*>(impl);
  BL_PROPAGATE(blBmpDecoderImplReadInfo(decoderI, nullptr, data, size));

  if (decoderI->frameIndex)
    return blTraceError(BL_ERROR_NO_MORE_DATA);

  BLResult result = blBmpDecoderImplReadFrameInternal(decoderI, static_cast<BLImage*>(imageOut), data, size);
  if (result != BL_SUCCESS)
    decoderI->lastResult = result;
  return result;
}

static BLResult BL_CDECL blBmpDecoderImplCreate(BLImageDecoderCore* self) noexcept {
  BLBmpDecoderImpl* decoderI = blObjectDetailAllocImplT<BLBmpDecoderImpl>(self, BLObjectInfo::packType(BL_OBJECT_TYPE_IMAGE_DECODER));

  if (BL_UNLIKELY(!decoderI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  decoderI->ctor(&blBmpDecoderVirt, &blBmpCodecObject);
  return blBmpDecoderImplRestart(decoderI);
}

static BLResult BL_CDECL blBmpDecoderImplDestroy(BLObjectImpl* impl, uint32_t info) noexcept {
  BLBmpDecoderImpl* decoderI = static_cast<BLBmpDecoderImpl*>(impl);

  decoderI->dtor();
  return blObjectDetailFreeImpl(decoderI, info);
}

// BLBmpEncoderImpl - Interface
// ============================

static BLResult BL_CDECL blBmpEncoderImplRestart(BLImageEncoderImpl* impl) noexcept {
  BLBmpEncoderImpl* encoderI = static_cast<BLBmpEncoderImpl*>(impl);
  encoderI->lastResult = BL_SUCCESS;
  encoderI->frameIndex = 0;
  encoderI->bufferIndex = 0;
  return BL_SUCCESS;
}

static BLResult BL_CDECL blBmpEncoderImplWriteFrame(BLImageEncoderImpl* impl, BLArrayCore* dst, const BLImageCore* image) noexcept {
  BLBmpEncoderImpl* encoderI = static_cast<BLBmpEncoderImpl*>(impl);
  BL_PROPAGATE(encoderI->lastResult);

  BLArray<uint8_t>& buf = *static_cast<BLArray<uint8_t>*>(dst);
  const BLImage& img = *static_cast<const BLImage*>(image);

  if (img.empty())
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLImageData imageData;
  BLResult result = img.getData(&imageData);

  if (result != BL_SUCCESS)
    return result;

  uint32_t w = uint32_t(imageData.size.w);
  uint32_t h = uint32_t(imageData.size.h);
  uint32_t format = imageData.format;

  uint32_t headerSize = BL_BMP_HEADER_SIZE_WIN_V1;
  uint32_t bpl = 0;
  uint32_t gap = 0;
  uint32_t paletteSize = 0;

  BLPixelConverter pc;
  BLBmpFileHeader file {};
  BLBmpInfoHeader info {};
  BLFormatInfo bmpFmt {};

  info.win.width = int32_t(w);
  info.win.height = int32_t(h);
  info.win.planes = 1;
  info.win.compression = BL_BMP_COMPRESSION_RGB;
  info.win.colorspace = BL_BMP_COLOR_SPACE_DD_RGB;

  switch (format) {
    case BL_FORMAT_PRGB32: {
      // NOTE: Version 3 would be okay, but not all tools can read BMPv3.
      headerSize = BL_BMP_HEADER_SIZE_WIN_V4;
      bpl = w * 4;
      bmpFmt.depth = 32;
      bmpFmt.flags = BL_FORMAT_FLAG_RGBA | BL_FORMAT_FLAG_PREMULTIPLIED;
      bmpFmt.rSize = 8; bmpFmt.rShift = 16;
      bmpFmt.gSize = 8; bmpFmt.gShift = 8;
      bmpFmt.bSize = 8; bmpFmt.bShift = 0;
      bmpFmt.aSize = 8; bmpFmt.aShift = 24;
      break;
    }

    case BL_FORMAT_XRGB32: {
      bpl = w * 3;
      gap = BLIntOps::alignUpDiff(bpl, 4);
      bmpFmt.depth = 24;
      bmpFmt.flags = BL_FORMAT_FLAG_RGB;
      bmpFmt.rSize = 8; bmpFmt.rShift = 16;
      bmpFmt.gSize = 8; bmpFmt.gShift = 8;
      bmpFmt.bSize = 8; bmpFmt.bShift = 0;
      break;
    }

    case BL_FORMAT_A8: {
      bpl = w;
      gap = BLIntOps::alignUpDiff(bpl, 4);
      bmpFmt.depth = 8;
      paletteSize = 256 * 4;
      info.win.colorsUsed = 256;
      break;
    }
  }

  uint32_t imageOffset = 2 + 12 + headerSize + paletteSize;
  uint32_t imageSize = (bpl + gap) * h;
  uint32_t fileSize = imageOffset + imageSize;

  file.fileSize = fileSize;
  file.imageOffset = imageOffset;
  info.win.headerSize = headerSize;
  info.win.bitsPerPixel = uint16_t(bmpFmt.depth);
  info.win.imageSize = imageSize;

  if (paletteSize == 0) {
    info.win.rMask = (bmpFmt.rSize ? BLIntOps::nonZeroLsbMask<uint32_t>(bmpFmt.rSize) : uint32_t(0)) << bmpFmt.rShift;
    info.win.gMask = (bmpFmt.gSize ? BLIntOps::nonZeroLsbMask<uint32_t>(bmpFmt.gSize) : uint32_t(0)) << bmpFmt.gShift;
    info.win.bMask = (bmpFmt.bSize ? BLIntOps::nonZeroLsbMask<uint32_t>(bmpFmt.bSize) : uint32_t(0)) << bmpFmt.bShift;
    info.win.aMask = (bmpFmt.aSize ? BLIntOps::nonZeroLsbMask<uint32_t>(bmpFmt.aSize) : uint32_t(0)) << bmpFmt.aShift;

    if (BL_BYTE_ORDER_NATIVE != BL_BYTE_ORDER_LE)
      bmpFmt.flags |= BL_FORMAT_FLAG_BYTE_SWAP;

    // This should never fail as only a limited subset of possibilities exist
    // that are guaranteed by the implementation.
    result = pc.create(bmpFmt, blFormatInfo[format]);
    BL_ASSERT(result == BL_SUCCESS);
  }

  uint8_t* dstData;
  BL_PROPAGATE(buf.modifyOp(BL_MODIFY_OP_ASSIGN_FIT, fileSize, &dstData));

  const uint8_t* srcData = static_cast<const uint8_t*>(imageData.pixelData);
  intptr_t srcStride = imageData.stride;

  if (BL_BYTE_ORDER_NATIVE != BL_BYTE_ORDER_LE) {
    file.byteSwap();
    info.win.byteSwap();
  }

  // Write file signature.
  memcpy(dstData, "BM", 2);
  dstData += 2;

  // Write file header.
  memcpy(dstData, &file, 12);
  dstData += 12;

  // Write info header.
  memcpy(dstData, &info, headerSize);
  dstData += headerSize;

  // Write palette and pixel data.
  if (paletteSize == 0) {
    BLPixelConverterOptions opt {};
    opt.gap = gap;
    pc.convertRect(dstData, bpl + gap, srcData + (intptr_t(h - 1) * srcStride), -srcStride, w, h, &opt);
  }
  else {
    uint32_t i;

    for (i = 0; i < 256; i++, dstData += 4) {
      dstData[0] = uint8_t(i);
      dstData[1] = uint8_t(i);
      dstData[2] = uint8_t(i);
      dstData[3] = uint8_t(0xFFu);
    }

    for (i = h; i; i--) {
      memcpy(dstData, srcData + (i - 1) * srcStride, bpl);
      dstData += bpl;
      BLMemOps::fillInlineT(dstData, uint8_t(0), gap);
      dstData += gap;
    }
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blBmpEncoderImplCreate(BLImageEncoderCore* self) noexcept {
  BLBmpEncoderImpl* encoderI = blObjectDetailAllocImplT<BLBmpEncoderImpl>(self, BLObjectInfo::packType(BL_OBJECT_TYPE_IMAGE_ENCODER));

  if (BL_UNLIKELY(!encoderI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  encoderI->ctor(&blBmpEncoderVirt, &blBmpCodecObject);
  return blBmpEncoderImplRestart(encoderI);
}

static BLResult BL_CDECL blBmpEncoderImplDestroy(BLObjectImpl* impl, uint32_t info) noexcept {
  BLBmpEncoderImpl* encoderI = static_cast<BLBmpEncoderImpl*>(impl);

  encoderI->dtor();
  return blObjectDetailFreeImpl(encoderI, info);
}

// BLBmpCodecImpl - Interface
// ==========================

static BLResult BL_CDECL blBmpCodecImplDestroy(BLObjectImpl* impl, uint32_t info) noexcept {
  // Built-in codecs are never destroyed.
  blUnused(impl, info);
  return BL_SUCCESS;
};

static uint32_t BL_CDECL blBmpCodecImplInspectData(const BLImageCodecImpl* impl, const uint8_t* data, size_t size) noexcept {
  blUnused(impl);

  // BMP minimum size and signature (BM).
  if (size < 2 || data[0] != 0x42 || data[1] != 0x4D)
    return 0;

  // Return something low as we cannot validate the header.
  if (size < 18)
    return 1;

  // Check whether `data` contains a correct BMP header.
  uint32_t headerSize = BLMemOps::readU32uLE(data + 14);
  if (!blBmpCheckHeaderSize(headerSize))
    return 0;

  return 100;
}

static BLResult BL_CDECL blBmpCodecImplCreateDecoder(const BLImageCodecImpl* impl, BLImageDecoderCore* dst) noexcept {
  blUnused(impl);

  BLImageDecoderCore tmp;
  BL_PROPAGATE(blBmpDecoderImplCreate(&tmp));
  return blImageDecoderAssignMove(dst, &tmp);
}

static BLResult BL_CDECL blBmpCodecImplCreateEncoder(const BLImageCodecImpl* impl, BLImageEncoderCore* dst) noexcept {
  blUnused(impl);

  BLImageEncoderCore tmp;
  BL_PROPAGATE(blBmpEncoderImplCreate(&tmp));
  return blImageEncoderAssignMove(dst, &tmp);
}

// BLBmpCodecImpl - Runtime Registration
// =====================================

void blBmpCodecOnInit(BLRuntimeContext* rt, BLArray<BLImageCodec>* codecs) noexcept {
  blUnused(rt);

  // Initialize BMP codec.
  blBmpCodec.virt.base.destroy = blBmpCodecImplDestroy;
  blBmpCodec.virt.base.getProperty = blObjectImplGetProperty;
  blBmpCodec.virt.base.setProperty = blObjectImplSetProperty;
  blBmpCodec.virt.inspectData = blBmpCodecImplInspectData;
  blBmpCodec.virt.createDecoder = blBmpCodecImplCreateDecoder;
  blBmpCodec.virt.createEncoder = blBmpCodecImplCreateEncoder;

  blBmpCodec.impl->ctor(&blBmpCodec.virt);
  blBmpCodec.impl->features =
    BL_IMAGE_CODEC_FEATURE_READ     |
    BL_IMAGE_CODEC_FEATURE_WRITE    |
    BL_IMAGE_CODEC_FEATURE_LOSSLESS ;
  blBmpCodec.impl->name.dcast().assign("BMP");
  blBmpCodec.impl->vendor.dcast().assign("Blend2D");
  blBmpCodec.impl->mimeType.dcast().assign("image/x-bmp");
  blBmpCodec.impl->extensions.dcast().assign("bmp|ras");

  blBmpCodecObject._d.initDynamic(
    BL_OBJECT_TYPE_IMAGE_CODEC,
    BLObjectInfo{BL_OBJECT_INFO_IMMUTABLE_FLAG},
    &blBmpCodec.impl);

  // Initialize BMP decoder virtual functions.
  blBmpDecoderVirt.base.destroy = blBmpDecoderImplDestroy;
  blBmpDecoderVirt.base.getProperty = blObjectImplGetProperty;
  blBmpDecoderVirt.base.setProperty = blObjectImplSetProperty;
  blBmpDecoderVirt.restart = blBmpDecoderImplRestart;
  blBmpDecoderVirt.readInfo = blBmpDecoderImplReadInfo;
  blBmpDecoderVirt.readFrame = blBmpDecoderImplReadFrame;

  // Initialize BMP encoder virtual functions.
  blBmpEncoderVirt.base.destroy = blBmpEncoderImplDestroy;
  blBmpEncoderVirt.base.getProperty = blObjectImplGetProperty;
  blBmpEncoderVirt.base.setProperty = blObjectImplSetProperty;
  blBmpEncoderVirt.restart = blBmpEncoderImplRestart;
  blBmpEncoderVirt.writeFrame = blBmpEncoderImplWriteFrame;

  codecs->append(blBmpCodecObject.dcast());
}
