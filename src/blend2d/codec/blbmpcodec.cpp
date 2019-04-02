// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../blformat_p.h"
#include "../blpixelconverter.h"
#include "../blrgba.h"
#include "../blruntime_p.h"
#include "../codec/blbmpcodec_p.h"

// ============================================================================
// [Global Variables]
// ============================================================================

static BLBmpCodecImpl blBmpCodecImpl;
static BLImageCodecVirt blBmpCodecVirt;
static BLImageDecoderVirt blBmpDecoderVirt;
static BLImageEncoderVirt blBmpEncoderVirt;

// ============================================================================
// [BLBmpDecoder - Utilities]
// ============================================================================

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
    if (m != 0 && !blIsBitMaskConsecutive(m))
      return false;

    // Mask can't overlap with other.
    if ((combined & m) != 0)
      return false;

    combined |= m;
  }
  return true;
}

// ============================================================================
// [BLBmpDecoder - RLE4 / RLE8]
// ============================================================================

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
        blMemWriteU32a(dstData + 0, c0);
        blMemWriteU32a(dstData + 4, c1);
      }

      if (i) {
        blMemWriteU32a(dstData + 0, c0);
        dstData += 4;
      }
    }
    else if (b1 >= BL_BMP_RLE_CMD_COUNT) {
      // Absolute (b1 = Size).
      uint32_t i = blMin<uint32_t>(b1, w - x);
      uint32_t reqBytes = ((b1 + 3) >> 1) & ~0x1;

      if ((size_t)(end - p) < reqBytes)
        return blTraceError(BL_ERROR_DATA_TRUNCATED);

      for (x += i; i >= 4; i -= 4, dstData += 16) {
        b0 = p[0];
        b1 = p[1]; p += 2;

        blMemWriteU32a(dstData +  0, pal[b0 >> 4].value);
        blMemWriteU32a(dstData +  4, pal[b0 & 15].value);
        blMemWriteU32a(dstData +  8, pal[b1 >> 4].value);
        blMemWriteU32a(dstData + 12, pal[b1 & 15].value);
      }

      if (i) {
        b0 = p[0];
        b1 = p[1]; p += 2;

        blMemWriteU32a(dstData, pal[b0 >> 4].value);
        dstData += 4;

        if (--i) {
          blMemWriteU32a(dstData, pal[b0 & 15].value);
          dstData += 4;

          if (--i) {
            blMemWriteU32a(dstData, pal[b1 >> 4].value);
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
          blMemWriteU32a(dstData, BL_BMP_RLE_BACKGROUND);
        }

        dstLine += dstStride;
        dstData = dstLine;
      }

      for (; x < toX; x++, dstData += 4) {
        blMemWriteU32a(dstData, BL_BMP_RLE_BACKGROUND);
      }

      if (b1 == BL_BMP_RLE_CMD_STOP || y == h)
        return BL_SUCCESS;
    }
  }
}

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
        blMemWriteU32a(dstData, c0);
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

        blMemWriteU32a(dstData + 0, pal[b0].value);
        blMemWriteU32a(dstData + 4, pal[b1].value);
      }

      if (i) {
        b0 = p[0]; p += 2;

        blMemWriteU32a(dstData, pal[b0].value);
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
          blMemWriteU32a(dstData, BL_BMP_RLE_BACKGROUND);
        }

        dstLine += dstStride;
        dstData = dstLine;
      }

      for (; x < toX; x++, dstData += 4) {
        blMemWriteU32a(dstData, BL_BMP_RLE_BACKGROUND);
      }

      if (b1 == BL_BMP_RLE_CMD_STOP || y == h)
        return BL_SUCCESS;
    }
  }
}

// ============================================================================
// [BLBmpDecoder - Read Internal]
// ============================================================================

static BLResult blBmpDecoderImplReadInfoInternal(BLBmpDecoderImpl* impl, const uint8_t* data, size_t size) noexcept {
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
  memcpy(&impl->file, data + 2, 12);
  data += 2 + 12;
  if (BL_BYTE_ORDER_NATIVE != BL_BYTE_ORDER_LE) impl->file.byteSwap();

  // First check if the header is supported by the decoder.
  uint32_t headerSize = blMemReadU32uLE(data);
  uint32_t fileAndInfoHeaderSize = 14 + headerSize;

  if (!blBmpCheckHeaderSize(headerSize))
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

  // Read BMP info header.
  if ((size_t)(end - data) < headerSize)
    return blTraceError(BL_ERROR_DATA_TRUNCATED);

  memcpy(&impl->info, data, headerSize);
  data += headerSize;

  int32_t w, h;
  uint32_t depth;
  uint32_t planeCount;
  uint32_t compression = BL_BMP_COMPRESSION_RGB;
  bool rleUsed = false;

  if (headerSize == BL_BMP_HEADER_SIZE_OS2_V1) {
    // Handle OS/2 BMP.
    if (BL_BYTE_ORDER_NATIVE != BL_BYTE_ORDER_LE) impl->info.os2.byteSwap();

    w = impl->info.os2.width;
    h = impl->info.os2.height;
    depth = impl->info.os2.bitsPerPixel;
    planeCount = impl->info.os2.planes;

    // Convert to Windows BMP, there is no difference except the header.
    impl->info.win.width = uint32_t(w);
    impl->info.win.height = uint32_t(h);
    impl->info.win.planes = uint16_t(planeCount);
    impl->info.win.bitsPerPixel = uint16_t(depth);
    impl->info.win.compression = compression;
  }
  else {
    // Handle Windows BMP.
    if (BL_BYTE_ORDER_NATIVE != BL_BYTE_ORDER_LE) impl->info.win.byteSwap();

    w = impl->info.win.width;
    h = impl->info.win.height;
    depth = impl->info.win.bitsPerPixel;
    planeCount = impl->info.win.planes;
    compression = impl->info.win.compression;
  }

  // Verify whether input data is ok.
  if (h == blMinValue<int32_t>() || w <= 0)
    return blTraceError(BL_ERROR_INVALID_DATA);

  if (planeCount != 1)
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

  if (h < 0)
    h = -h;

  impl->imageInfo.size.reset(w, h);
  impl->imageInfo.depth = uint16_t(depth);
  impl->imageInfo.planeCount = uint16_t(planeCount);
  impl->imageInfo.frameCount = 1;

  // Check if the compression field is correct when depth <= 8.
  if (compression != BL_BMP_COMPRESSION_RGB) {
    if (depth <= 8) {
      rleUsed = (depth == 4 && compression == BL_BMP_COMPRESSION_RLE4) |
                (depth == 8 && compression == BL_BMP_COMPRESSION_RLE8) ;

      if (!rleUsed)
        return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
    }
  }

  if (impl->file.imageOffset < fileAndInfoHeaderSize)
    return blTraceError(BL_ERROR_INVALID_DATA);

  // Check if the size is valid.
  if (!blBmpCheckImageSize(impl->imageInfo.size))
    return blTraceError(BL_ERROR_IMAGE_TOO_LARGE);

  // Check if the depth is valid.
  if (!blBmpCheckDepth(impl->imageInfo.depth))
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

  // Calculate a stride aligned to 32 bits.
  BLOverflowFlag of = 0;
  uint64_t stride = (((uint64_t(w) * uint64_t(depth) + 7) / 8) + 3) & ~3;
  uint32_t imageSize = blMulOverflow(uint32_t(impl->stride & 0xFFFFFFFFu), uint32_t(h), &of);

  if (stride >= blMaxValue<uint32_t>() || of)
    return blTraceError(BL_ERROR_INVALID_DATA);

  impl->stride = uint32_t(stride);

  // 1. OS/2 format doesn't specify imageSize, it's always calculated.
  // 2. BMP allows `imageSize` to be zero in case of uncompressed bitmaps.
  if (headerSize == BL_BMP_HEADER_SIZE_OS2_V1 || (impl->info.win.imageSize == 0 && !rleUsed))
    impl->info.win.imageSize = imageSize;

  // Check if the `imageSize` matches the calculated one. It's malformed if it doesn't.
  if (!rleUsed && impl->info.win.imageSize < imageSize)
    return blTraceError(BL_ERROR_INVALID_DATA);

  impl->fmt.depth = depth;
  if (depth <= 8) {
    impl->fmt.flags = BL_FORMAT_FLAG_RGB | BL_FORMAT_FLAG_INDEXED;
  }
  else {
    impl->fmt.flags = BL_FORMAT_FLAG_RGB;

    if (BL_BYTE_ORDER_NATIVE != BL_BYTE_ORDER_LE)
      impl->fmt.flags |= BL_FORMAT_FLAG_BYTE_SWAP;

    if (depth == 16) {
      impl->fmt.rSize = 5; impl->fmt.rShift = 10;
      impl->fmt.gSize = 5; impl->fmt.gShift = 5;
      impl->fmt.bSize = 5; impl->fmt.bShift = 0;
    }

    if (depth == 24 || depth == 32) {
      impl->fmt.rSize = 8; impl->fmt.rShift = 16;
      impl->fmt.gSize = 8; impl->fmt.gShift = 8;
      impl->fmt.bSize = 8; impl->fmt.bShift = 0;
    }
  }

  bool hasBitFields = depth > 8 && headerSize >= BL_BMP_HEADER_SIZE_WIN_V2;
  if (headerSize == BL_BMP_HEADER_SIZE_WIN_V1) {
    // Use BITFIELDS if specified.
    uint32_t compression = impl->info.win.compression;

    if (compression == BL_BMP_COMPRESSION_BIT_FIELDS || compression == BL_BMP_COMPRESSION_ALPHA_BIT_FIELDS) {
      uint32_t channels = 3 + (compression == BL_BMP_COMPRESSION_ALPHA_BIT_FIELDS);
      if (depth != 16 && depth != 32)
        return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

      if ((size_t)(end - data) < channels * 4)
        return blTraceError(BL_ERROR_DATA_TRUNCATED);

      for (uint32_t i = 0; i < channels; i++)
        impl->info.win.masks[i] = blMemReadU32uLE(data + i * 4);

      hasBitFields = true;
      data += channels * 4;
    }
  }

  if (hasBitFields) {
    // BitFields provided by info header must be continuous and non-overlapping.
    if (!blBmpCheckBitMasks(impl->info.win.masks, 4))
      return blTraceError(BL_ERROR_INVALID_DATA);

    blFormatInfoAssignAbsoluteMasks(impl->fmt, impl->info.win.masks);
    if (impl->info.win.aMask)
      impl->fmt.flags |= BL_FORMAT_FLAG_ALPHA | BL_FORMAT_FLAG_PREMULTIPLIED;
  }

  impl->bufferIndex = (size_t)(data - start);
  return BL_SUCCESS;
}

static BLResult blBmpDecoderImplReadFrameInternal(BLBmpDecoderImpl* impl, BLImage* imageOut, const uint8_t* data, size_t size) noexcept {
  uint32_t result = BL_SUCCESS;
  const uint8_t* start = data;
  const uint8_t* end = data + size;

  // BLImage info.
  uint32_t w = uint32_t(impl->imageInfo.size.w);
  uint32_t h = uint32_t(impl->imageInfo.size.h);

  uint32_t depth = impl->imageInfo.depth;
  uint32_t format = impl->fmt.sizes[3] ? BL_FORMAT_PRGB32 : BL_FORMAT_XRGB32;
  uint32_t fileAndInfoHeaderSize = 14 + impl->info.headerSize;

  if (size < fileAndInfoHeaderSize)
    return blTraceError(BL_ERROR_DATA_TRUNCATED);

  // Palette.
  BLRgba32 pal[256];
  uint32_t palSize;

  if (depth <= 8) {
    const uint8_t* pPal = data + fileAndInfoHeaderSize;
    palSize = impl->file.imageOffset - fileAndInfoHeaderSize;

    uint32_t palEntitySize = impl->info.headerSize == BL_BMP_HEADER_SIZE_OS2_V1 ? 3 : 4;
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
  if (size - impl->file.imageOffset < impl->info.win.imageSize)
    return blTraceError(BL_ERROR_DATA_TRUNCATED);
  data += impl->file.imageOffset;

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
  if (impl->info.win.height > 0) {
    dstLine += (h - 1) * dstStride;
    dstStride = -dstStride;
  }

  // Decode.
  if (depth == 4 && impl->info.win.compression == BL_BMP_COMPRESSION_RLE4) {
    BL_PROPAGATE(blBmpDecodeRLE4(dstLine, dstStride, data, impl->info.win.imageSize, w, h, pal));
  }
  else if (depth == 8 && impl->info.win.compression == BL_BMP_COMPRESSION_RLE8) {
    BL_PROPAGATE(blBmpDecodeRLE8(dstLine, dstStride, data, impl->info.win.imageSize, w, h, pal));
  }
  else {
    BLPixelConverter pc;
    BL_PROPAGATE(pc.create(blFormatInfo[format], impl->fmt));
    pc.convertRect(dstLine, dstStride, data, impl->stride, w, h);
  }

  impl->bufferIndex = (size_t)(data - start);
  impl->frameIndex++;
  return BL_SUCCESS;
}

// ============================================================================
// [BLBmpDecoder - Interface]
// ============================================================================

static BLResult BL_CDECL blBmpDecoderImplDestroy(BLBmpDecoderImpl* impl) noexcept {
  return blRuntimeFreeImpl(impl, sizeof(BLBmpDecoderImpl), impl->memPoolData);
}

static BLResult BL_CDECL blBmpDecoderImplRestart(BLBmpDecoderImpl* impl) noexcept {
  impl->lastResult = BL_SUCCESS;
  impl->frameIndex = 0;
  impl->bufferIndex = 0;
  impl->imageInfo.reset();
  impl->file.reset();
  impl->info.reset();
  impl->fmt.reset();
  impl->stride = 0;
  return BL_SUCCESS;
}

static BLResult BL_CDECL blBmpDecoderImplReadInfo(BLBmpDecoderImpl* impl, BLImageInfo* infoOut, const uint8_t* data, size_t size) noexcept {
  BLResult result = impl->lastResult;
  if (impl->bufferIndex == 0 && result == BL_SUCCESS) {
    result = blBmpDecoderImplReadInfoInternal(impl, data, size);
    if (result != BL_SUCCESS)
      impl->lastResult = result;
  }

  if (infoOut)
    memcpy(infoOut, &impl->imageInfo, sizeof(BLImageInfo));

  return result;
}

static BLResult BL_CDECL blBmpDecoderImplReadFrame(BLBmpDecoderImpl* impl, BLImage* imageOut, const uint8_t* data, size_t size) noexcept {
  BL_PROPAGATE(blBmpDecoderImplReadInfo(impl, nullptr, data, size));

  if (impl->frameIndex)
    return blTraceError(BL_ERROR_NO_MORE_DATA);

  BLResult result = blBmpDecoderImplReadFrameInternal(impl, imageOut, data, size);
  if (result != BL_SUCCESS)
    impl->lastResult = result;
  return result;
}

static BLBmpDecoderImpl* blBmpDecoderImplNew() noexcept {
  uint16_t memPoolData;
  BLBmpDecoderImpl* impl = blRuntimeAllocImplT<BLBmpDecoderImpl>(sizeof(BLBmpDecoderImpl), &memPoolData);

  if (BL_UNLIKELY(!impl))
    return nullptr;

  blImplInit(impl, BL_IMPL_TYPE_IMAGE_DECODER, BL_IMPL_TRAIT_VIRT, memPoolData);
  impl->virt = &blBmpDecoderVirt;
  impl->codec.impl = &blBmpCodecImpl;
  impl->handle = nullptr;
  blBmpDecoderImplRestart(impl);

  return impl;
}

// ============================================================================
// [BLBmpEncoder - Interface]
// ============================================================================

static BLResult BL_CDECL blBmpEncoderImplDestroy(BLBmpEncoderImpl* impl) noexcept {
  return blRuntimeFreeImpl(impl, sizeof(BLBmpEncoderImpl), impl->memPoolData);
}

static BLResult BL_CDECL blBmpEncoderImplRestart(BLBmpEncoderImpl* impl) noexcept {
  impl->lastResult = BL_SUCCESS;
  impl->frameIndex = 0;
  impl->bufferIndex = 0;
  return BL_SUCCESS;
}

static BLResult BL_CDECL blBmpEncoderImplWriteFrame(BLBmpEncoderImpl* impl, BLArray<uint8_t>* dst, const BLImage* image) noexcept {
  BL_PROPAGATE(impl->lastResult);

  if (image->empty())
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLImageData imageData;
  BLResult result = image->getData(&imageData);

  if (result != BL_SUCCESS)
    return result;

  uint32_t w = uint32_t(imageData.size.w);
  uint32_t h = uint32_t(imageData.size.h);
  uint32_t format = imageData.format;

  uint32_t headerSize = BL_BMP_HEADER_SIZE_WIN_V1;
  uint32_t bpl = 0;
  uint32_t gap = 0;

  BLBmpFileHeader file {};
  BLBmpInfoHeader info {};

  info.win.width = w;
  info.win.height = h;
  info.win.planes = 1;
  info.win.compression = BL_BMP_COMPRESSION_RGB;
  info.win.colorspace = BL_BMP_COLOR_SPACE_DD_RGB;

  BLFormatInfo bmpFmt {};

  switch (format) {
    case BL_FORMAT_PRGB32: {
      headerSize = BL_BMP_HEADER_SIZE_WIN_V3;
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
      gap = (4 - (bpl & 3)) & 3;
      bmpFmt.depth = 24;
      bmpFmt.flags = BL_FORMAT_FLAG_RGB;
      bmpFmt.rSize = 8; bmpFmt.rShift = 16;
      bmpFmt.gSize = 8; bmpFmt.gShift = 8;
      bmpFmt.bSize = 8; bmpFmt.bShift = 0;
      break;
    }

    case BL_FORMAT_A8: {
      // TODO: [BMP]
      break;
    }
  }

  uint32_t imageOffset = 2 + 12 + headerSize;
  uint32_t imageSize = (bpl + gap) * h;
  uint32_t fileSize = imageOffset + imageSize;

  file.fileSize = fileSize;
  file.imageOffset = imageOffset;
  info.win.headerSize = headerSize;
  info.win.bitsPerPixel = bmpFmt.depth;
  info.win.imageSize = imageSize;
  info.win.rMask = blTrailingBitMask<uint32_t>(bmpFmt.rSize) << bmpFmt.rShift;
  info.win.gMask = blTrailingBitMask<uint32_t>(bmpFmt.gSize) << bmpFmt.gShift;
  info.win.bMask = blTrailingBitMask<uint32_t>(bmpFmt.bSize) << bmpFmt.bShift;
  info.win.aMask = blTrailingBitMask<uint32_t>(bmpFmt.aSize) << bmpFmt.aShift;

  if (BL_BYTE_ORDER_NATIVE != BL_BYTE_ORDER_LE) {
    file.byteSwap();
    info.win.byteSwap();
    bmpFmt.flags |= BL_FORMAT_FLAG_BYTE_SWAP;
  }

  BLPixelConverter pc;
  result = pc.create(bmpFmt, blFormatInfo[format]);

  // This should never fail as there is only a limited subset of possibilities that are always implemented.
  BL_ASSERT(result == BL_SUCCESS);

  uint8_t* dstData;
  BL_PROPAGATE(dst->modifyOp(BL_MODIFY_OP_ASSIGN_FIT, fileSize, &dstData));

  const uint8_t* srcData = static_cast<const uint8_t*>(imageData.pixelData);
  intptr_t srcStride = imageData.stride;

  // Write file signature.
  memcpy(dstData, "BM", 2);
  dstData += 2;

  // Write file header.
  memcpy(dstData, &file, 12);
  dstData += 12;

  // Write info header.
  memcpy(dstData, &info, headerSize);
  dstData += headerSize;

  // Write pixel data.
  BLPixelConverterOptions opt {};
  opt.gap = gap;
  pc.convertRect(dstData, bpl, srcData + (intptr_t(h - 1) * srcStride), -srcStride, w, h, &opt);
  return BL_SUCCESS;
}

static BLBmpEncoderImpl* blBmpEncoderImplNew() noexcept {
  uint16_t memPoolData;
  BLBmpEncoderImpl* impl = blRuntimeAllocImplT<BLBmpEncoderImpl>(sizeof(BLBmpEncoderImpl), &memPoolData);

  if (BL_UNLIKELY(!impl))
    return nullptr;

  blImplInit(impl, BL_IMPL_TYPE_IMAGE_ENCODER, BL_IMPL_TRAIT_VIRT, memPoolData);
  impl->virt = &blBmpEncoderVirt;
  impl->codec.impl = &blBmpCodecImpl;
  impl->handle = nullptr;
  blBmpEncoderImplRestart(impl);

  return impl;
}

// ============================================================================
// [BLBmpCodec - Interface]
// ============================================================================

static BLResult BL_CDECL blBmpCodecImplDestroy(BLBmpCodecImpl* impl) noexcept { return BL_SUCCESS; };

static uint32_t BL_CDECL blBmpCodecImplInspectData(BLBmpCodecImpl* impl, const uint8_t* data, size_t size) noexcept {
  // BMP minimum size and signature (BM).
  if (size < 2 || data[0] != 0x42 || data[1] != 0x4D)
    return 0;

  // Return something low as we cannot validate the header.
  if (size < 18)
    return 1;

  // Check whether `data` contains a correct BMP header.
  uint32_t headerSize = blMemReadU32uLE(data + 14);
  if (!blBmpCheckHeaderSize(headerSize))
    return 0;

  return 100;
}

static BLResult BL_CDECL blBmpCodecImplCreateDecoder(const BLImageCodecImpl* impl, BLImageDecoderCore* dst) noexcept {
  BLImageDecoderCore decoder { blBmpDecoderImplNew() };
  if (BL_UNLIKELY(!decoder.impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  return blImageDecoderAssignMove(dst, &decoder);
}

static BLResult BL_CDECL blBmpCodecImplCreateEncoder(const BLImageCodecImpl* impl, BLImageEncoderCore* dst) noexcept {
  BLImageEncoderCore encoder { blBmpEncoderImplNew() };
  if (BL_UNLIKELY(!encoder.impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  return blImageEncoderAssignMove(dst, &encoder);
}

// ============================================================================
// [BLBmpCodec - Runtime Init]
// ============================================================================

BLImageCodecImpl* blBmpCodecRtInit(BLRuntimeContext* rt) noexcept {
  // Initialize BMP decoder virtual functions.
  blAssignFunc(&blBmpDecoderVirt.destroy, blBmpDecoderImplDestroy);
  blAssignFunc(&blBmpDecoderVirt.restart, blBmpDecoderImplRestart);
  blAssignFunc(&blBmpDecoderVirt.readInfo, blBmpDecoderImplReadInfo);
  blAssignFunc(&blBmpDecoderVirt.readFrame, blBmpDecoderImplReadFrame);

  // Initialize BMP encoder virtual functions.
  blAssignFunc(&blBmpEncoderVirt.destroy, blBmpEncoderImplDestroy);
  blAssignFunc(&blBmpEncoderVirt.restart, blBmpEncoderImplRestart);
  blAssignFunc(&blBmpEncoderVirt.writeFrame, blBmpEncoderImplWriteFrame);

  // Initialize BMP codec virtual functions.
  blAssignFunc(&blBmpCodecVirt.destroy, blBmpCodecImplDestroy);
  blAssignFunc(&blBmpCodecVirt.inspectData, blBmpCodecImplInspectData);
  blAssignFunc(&blBmpCodecVirt.createDecoder, blBmpCodecImplCreateDecoder);
  blAssignFunc(&blBmpCodecVirt.createEncoder, blBmpCodecImplCreateEncoder);

  // Initialize BMP codec built-in instance.
  BLBmpCodecImpl* codecI = &blBmpCodecImpl;

  codecI->virt = &blBmpCodecVirt;
  codecI->implType = uint8_t(BL_IMPL_TYPE_IMAGE_CODEC);
  codecI->implTraits = uint8_t(BL_IMPL_TRAIT_VIRT);

  codecI->features = BL_IMAGE_CODEC_FEATURE_READ     |
                     BL_IMAGE_CODEC_FEATURE_WRITE    |
                     BL_IMAGE_CODEC_FEATURE_LOSSLESS ;

  codecI->name = "BMP";
  codecI->vendor = "Blend2D";
  codecI->mimeType = "image/x-bmp";
  codecI->extensions = "bmp|ras";

  return codecI;
}
