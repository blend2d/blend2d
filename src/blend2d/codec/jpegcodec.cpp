// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// The JPEG codec is based on stb_image <https://github.com/nothings/stb>
// released into PUBLIC DOMAIN. Blend2D's JPEG codec can be distributed
// under Blend2D's ZLIB license or under STB's PUBLIC DOMAIN as well.

#include "../api-build_p.h"
#include "../object_p.h"
#include "../runtime_p.h"
#include "../string_p.h"
#include "../codec/jpegcodec_p.h"
#include "../codec/jpeghuffman_p.h"
#include "../codec/jpegops_p.h"
#include "../support/intops_p.h"
#include "../support/memops_p.h"
#include "../support/scopedbuffer_p.h"

namespace bl::Jpeg {

// bl::Jpeg::Codec - Globals
// =========================

static BLObjectEternalVirtualImpl<BLJpegCodecImpl, BLImageCodecVirt> jpegCodec;
static BLImageCodecCore jpegCodecInstance;

static BLImageDecoderVirt jpegDecoderVirt;
/*
static BLImageEncoderVirt jpegEncoderVirt;
*/

// bl::Jpeg::Decoder - DeZigZag Table
// ==================================

// Mapping table of zigzagged 8x8 data into a natural order.
static const uint8_t decoderDeZigZagTable[64 + 16] = {
  0 , 1 , 8 , 16, 9 , 2 , 3 , 10,
  17, 24, 32, 25, 18, 11, 4 , 5 ,
  12, 19, 26, 33, 40, 48, 41, 34,
  27, 20, 13, 6 , 7 , 14, 21, 28,
  35, 42, 49, 56, 57, 50, 43, 36,
  29, 22, 15, 23, 30, 37, 44, 51,
  58, 59, 52, 45, 38, 31, 39, 46,
  53, 60, 61, 54, 47, 55, 62, 63,

  // These are not part of JPEG's spec, however, it's convenient as the decoder doesn't have to check whether
  // the coefficient index is out of bounds.
  63, 63, 63, 63, 63, 63, 63, 63,
  63, 63, 63, 63, 63, 63, 63, 63
};

// bl::Jpeg::Decoder - Process Marker
// ==================================

static BLResult decoderProcessMarker(BLJpegDecoderImpl* decoderI, uint32_t m, const uint8_t* p, size_t remain, size_t& consumedBytes) noexcept {
  // Should be zero when passed in.
  BL_ASSERT(consumedBytes == 0);

  BLImageInfo& imageInfo = decoderI->imageInfo;

#define GET_PAYLOAD_SIZE(MinSize) \
  size_t size; \
  \
  do { \
    if (remain < MinSize) \
      return blTraceError(BL_ERROR_DATA_TRUNCATED); \
    \
    size = MemOps::readU16uBE(p); \
    if (size < MinSize) \
      return blTraceError(BL_ERROR_INVALID_DATA); \
    \
    if (size > remain) \
      return blTraceError(BL_ERROR_DATA_TRUNCATED); \
    \
    p += 2; \
    remain = size - 2; \
  } while (false)

  // SOF - Start of Frame
  // --------------------
  //
  //        WORD - Size (Consumed by GET_PAYLOAD_...)
  //
  //   [00] BYTE - Precision `P`
  //   [01] WORD - Height `Y`
  //   [03] WORD - Width `X`
  //   [05] BYTE - Number of components `Nf`
  //
  //   [06] Specification of each component [0..Nf] {
  //        [00] BYTE Component identifier `id`
  //        [01] BYTE Horizontal `Hi` and vertical `Vi` sampling factor
  //        [02] BYTE Quantization table destination selector `TQi`
  //   }

  if (isMarkerSOF(m)) {
    uint32_t sofMarker = m;

    // Forbid multiple SOF markers in a single JPEG file.
    if (decoderI->sofMarker)
      return blTraceError(BL_ERROR_JPEG_MULTIPLE_SOF);

    // Check if SOF type is supported.
    if (sofMarker != kMarkerSOF0 &&
        sofMarker != kMarkerSOF1 &&
        sofMarker != kMarkerSOF2)
      return blTraceError(BL_ERROR_JPEG_UNSUPPORTED_SOF);

    // 11 bytes is a minimum size of SOF describing exactly one component.
    GET_PAYLOAD_SIZE(2 + 6 + 3);

    uint32_t bpp = p[0];
    uint32_t h = MemOps::readU16uBE(p + 1);
    uint32_t w = MemOps::readU16uBE(p + 3);
    uint32_t componentCount = p[5];

    if (size != 8 + 3 * componentCount)
      return blTraceError(BL_ERROR_JPEG_INVALID_SOF);

    // Advance header.
    p += 6;

    if (w == 0)
      return blTraceError(BL_ERROR_INVALID_DATA);

    // TODO: [JPEG] Unsupported delayed height (0).
    if (h == 0)
      return blTraceError(BL_ERROR_JPEG_UNSUPPORTED_FEATURE);

    if (w > BL_RUNTIME_MAX_IMAGE_SIZE || h > BL_RUNTIME_MAX_IMAGE_SIZE)
      return blTraceError(BL_ERROR_IMAGE_TOO_LARGE);

    // Check number of components and SOF size.
    if ((componentCount != 1 && componentCount != 3))
      return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

    // TODO: [JPEG] 16-BPC.
    if (bpp != 8)
      return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

    // Maximum horizontal/vertical sampling factor of all components.
    uint32_t mcuSfW = 1;
    uint32_t mcuSfH = 1;

    uint32_t i, j;
    for (i = 0; i < componentCount; i++, p += 3) {
      DecoderComponent* comp = &decoderI->comp[i];

      // Check if the ID doesn't collide with previous components.
      uint32_t compId = p[0];
      for (j = 0; j < i; j++) {
        if (decoderI->comp[j].compId == compId)
          return blTraceError(BL_ERROR_INVALID_DATA);
      }

      // TODO: [JPEG] Is this necessary?
      // Required by JFIF.
      if (compId != i + 1) {
        // Some version of JpegTran outputs non-JFIF-compliant files!
        if (compId != i)
          return blTraceError(BL_ERROR_INVALID_DATA);
      }

      // Horizontal/Vertical sampling factor.
      uint32_t sf = p[1];
      uint32_t sfW = sf >> 4;
      uint32_t sfH = sf & 15;

      if (sfW == 0 || sfW > 4 || sfH == 0 || sfH > 4)
        return blTraceError(BL_ERROR_INVALID_DATA);

      // Bail to 1 if there is only one component as it contributes to nothing.
      if (componentCount == 1) {
        sfW = 1;
        sfH = 1;
      }

      // Quantization ID.
      uint32_t quantId = p[2];
      if (quantId > 3)
        return blTraceError(BL_ERROR_INVALID_DATA);

      // Save to DecoderComponent.
      comp->compId  = uint8_t(compId);
      comp->sfW     = uint8_t(sfW);
      comp->sfH     = uint8_t(sfH);
      comp->quantId = uint8_t(quantId);

      // We need to know maximum horizontal and vertical sampling factor to calculate the correct MCU size (WxH).
      mcuSfW = blMax(mcuSfW, sfW);
      mcuSfH = blMax(mcuSfH, sfH);
    }

    // Compute interleaved MCU info.
    uint32_t mcuPxW = mcuSfW * kDctSize;
    uint32_t mcuPxH = mcuSfH * kDctSize;

    uint32_t mcuCountW = (w + mcuPxW - 1) / mcuPxW;
    uint32_t mcuCountH = (h + mcuPxH - 1) / mcuPxH;
    bool isBaseline = sofMarker != kMarkerSOF2;

    for (i = 0; i < componentCount; i++) {
      DecoderComponent* comp = &decoderI->comp[i];

      // Number of effective pixels (e.g. for non-interleaved MCU).
      comp->pxW = (w * uint32_t(comp->sfW) + mcuSfW - 1) / mcuSfW;
      comp->pxH = (h * uint32_t(comp->sfH) + mcuSfH - 1) / mcuSfH;

      // Allocate enough memory for all blocks even those that won't be used fully.
      comp->blW = mcuCountW * uint32_t(comp->sfW);
      comp->blH = mcuCountH * uint32_t(comp->sfH);

      comp->osW = comp->blW * kDctSize;
      comp->osH = comp->blH * kDctSize;

      comp->data = static_cast<uint8_t*>(decoderI->allocator.alloc(comp->osW * comp->osH));
      if (comp->data == nullptr)
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

      if (!isBaseline) {
        uint32_t kBlock8x8UInt16 = kDctSize2 * uint32_t(sizeof(int16_t));
        size_t coeffSize = comp->blW * comp->blH * kBlock8x8UInt16;
        int16_t* coeffData = static_cast<int16_t*>(decoderI->allocator.alloc(coeffSize, 16));

        if (coeffData == nullptr)
          return blTraceError(BL_ERROR_OUT_OF_MEMORY);

        comp->coeff = coeffData;
        memset(comp->coeff, 0, coeffSize);
      }
    }

    // Everything seems ok, store the image information.
    imageInfo.flags = 0;
    imageInfo.size.reset(int(w), int(h));
    imageInfo.depth = uint16_t(componentCount * bpp);
    imageInfo.planeCount = uint16_t(componentCount);
    imageInfo.frameCount = 1;

    if (!isBaseline)
      imageInfo.flags |= BL_IMAGE_INFO_FLAG_PROGRESSIVE;

    decoderI->sofMarker = uint8_t(sofMarker);
    decoderI->delayedHeight = (h == 0);
    decoderI->mcu.sf.w = uint8_t(mcuSfW);
    decoderI->mcu.sf.h = uint8_t(mcuSfH);
    decoderI->mcu.px.w = uint8_t(mcuPxW);
    decoderI->mcu.px.h = uint8_t(mcuPxH);
    decoderI->mcu.count.w = mcuCountW;
    decoderI->mcu.count.h = mcuCountH;

    consumedBytes = size;
    return BL_SUCCESS;
  }

  // DHT - Define Huffman Table
  // --------------------------
  //
  //        WORD - Size (Consumed by GET_PAYLOAD_...)
  //
  //   [00] BYTE - Table class `tc` and table identifier `ti`.
  //   [01] 16xB - The count of Huffman codes of size 1..16.
  //
  //   [17] .... - The one byte symbols sorted by Huffman code. The number of symbols is the sum of the 16 code counts.

  if (m == kMarkerDHT) {
    GET_PAYLOAD_SIZE(2 + 17);

    while (remain) {
      uint32_t q = *p++;
      remain--;

      uint32_t tableClass = q >> 4; // Table class.
      uint32_t tableId = q & 15; // Table id (0-3).

      // Invalid class or id.
      if (tableClass >= kTableCount || tableId > 3)
        return blTraceError(BL_ERROR_INVALID_DATA);

      size_t tableSizeInBytes;
      if (tableClass == kTableDC) {
        BL_PROPAGATE(buildHuffmanDC(&decoderI->dcTable[tableId], p, remain, &tableSizeInBytes));
        decoderI->dcTableMask = uint8_t(decoderI->dcTableMask | IntOps::lsbBitAt<uint32_t>(tableId));
      }
      else {
        BL_PROPAGATE(buildHuffmanAC(&decoderI->acTable[tableId], p, remain, &tableSizeInBytes));
        decoderI->acTableMask = uint8_t(decoderI->acTableMask | IntOps::lsbBitAt<uint32_t>(tableId));
      }

      p += tableSizeInBytes;
      remain -= tableSizeInBytes;
    }

    consumedBytes = size;
    return BL_SUCCESS;
  }

  // DQT - Define Quantization Table
  // -------------------------------
  //
  //        WORD - Size (Consumed by GET_PAYLOAD_...)
  //
  //   [00] BYTE - Quantization value size `quantSz` (0-1) and table identifier `quantId`.
  //   [01] .... - 64 or 128 bytes depending on `qs`.

  if (m == kMarkerDQT) {
    GET_PAYLOAD_SIZE(2 + 65);

    while (remain >= 65) {
      uint32_t q = *p++;

      uint32_t qSize = q >> 4;
      uint32_t qId = q & 15;

      if (qSize > 1 || qId > 3)
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint16_t* qTable = decoderI->qTable[qId].data;
      uint32_t requiredSize = 1 + 64 * (qSize + 1);

      if (requiredSize > remain)
        break;

      if (qSize == 0) {
        for (uint32_t k = 0; k < 64; k++, p++)
          qTable[decoderDeZigZagTable[k]] = *p;
      }
      else {
        for (uint32_t k = 0; k < 64; k++, p += 2)
          qTable[decoderDeZigZagTable[k]] = uint16_t(MemOps::readU16uBE(reinterpret_cast<const uint16_t*>(p)));
      }

      decoderI->qTableMask = uint8_t(decoderI->qTableMask | IntOps::lsbBitAt<uint8_t>(qId));
      remain -= requiredSize;
    }

    if (remain != 0)
      return blTraceError(BL_ERROR_INVALID_DATA);

    consumedBytes = size;
    return BL_SUCCESS;
  }

  // DRI - Define Restart Interval
  // -----------------------------
  //
  //        WORD - Size (Consumed by GET_PAYLOAD_...)
  //
  //   [00] WORD - Restart interval.

  if (m == kMarkerDRI) {
    if (remain < 4)
      return blTraceError(BL_ERROR_DATA_TRUNCATED);

    size_t size = MemOps::readU16uBE(p + 0);
    uint32_t ri = MemOps::readU16uBE(p + 2);

    // DRI payload should be 4 bytes.
    if (size != 4)
      return blTraceError(BL_ERROR_INVALID_DATA);

    decoderI->restartInterval = ri;
    consumedBytes = size;
    return BL_SUCCESS;
  }

  // SOS - Start of Scan
  // -------------------
  //
  //        WORD - Size (Consumed by GET_PAYLOAD_...)
  //
  //   [00] BYTE - Number of components in this SOS:
  //
  //   [01] Specification of each component - {
  //        [00] BYTE - Component ID
  //        [01] BYTE - DC and AC Selector
  //   }
  //
  //   [01 + NumComponents * 2]:
  //        [00] BYTE - Spectral Selection Start
  //        [01] BYTE - Spectral Selection End
  //        [02] BYTE - Successive Approximation High/Low

  if (m == kMarkerSOS) {
    GET_PAYLOAD_SIZE(2 + 6);

    uint32_t sofMarker = decoderI->sofMarker;
    uint32_t componentCount = imageInfo.planeCount;

    uint32_t scCount = *p++;
    uint32_t scMask = 0;

    if (size != 6 + scCount * 2)
      return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

    if (scCount < 1 || scCount > componentCount)
      return blTraceError(BL_ERROR_JPEG_INVALID_SOS);

    uint32_t ssStart   = uint32_t(p[scCount * 2 + 0]);
    uint32_t ssEnd     = uint32_t(p[scCount * 2 + 1]);
    uint32_t saLowBit  = uint32_t(p[scCount * 2 + 2]) & 15;
    uint32_t saHighBit = uint32_t(p[scCount * 2 + 2]) >> 4;

    if (sofMarker == kMarkerSOF0 || sofMarker == kMarkerSOF1) {
      if (ssStart != 0 || saLowBit != 0 || saHighBit != 0)
        return blTraceError(BL_ERROR_INVALID_DATA);

      // The value should be 63, but it's zero sometimes.
      ssEnd = 63;
    }

    if (sofMarker == kMarkerSOF2) {
      if (ssStart > 63 || ssEnd > 63 || ssStart > ssEnd || saLowBit > 13 || saHighBit > 13)
        return blTraceError(BL_ERROR_INVALID_DATA);

      // AC & DC cannot be merged in a progressive JPEG.
      if (ssStart == 0 && ssEnd != 0)
        return blTraceError(BL_ERROR_INVALID_DATA);
    }

    DecoderSOS& sos = decoderI->sos;
    sos.scCount   = uint8_t(scCount);
    sos.ssStart   = uint8_t(ssStart);
    sos.ssEnd     = uint8_t(ssEnd);
    sos.saLowBit  = uint8_t(saLowBit);
    sos.saHighBit = uint8_t(saHighBit);

    for (uint32_t i = 0; i < scCount; i++, p += 2) {
      uint32_t compId = p[0];
      uint32_t index = 0;

      while (decoderI->comp[index].compId != compId)
        if (++index >= componentCount)
          return blTraceError(BL_ERROR_JPEG_INVALID_SOS);

      // One huffman stream shouldn't overwrite the same component.
      if (IntOps::bitTest(scMask, index))
        return blTraceError(BL_ERROR_JPEG_INVALID_SOS);

      scMask |= IntOps::lsbBitAt<uint32_t>(index);

      uint32_t selector = p[1];
      uint32_t acId = selector & 15;
      uint32_t dcId = selector >> 4;

      // Validate AC & DC selectors.
      if (acId > 3 || (!IntOps::bitTest(decoderI->acTableMask, acId) && ssEnd  > 0))
        return blTraceError(BL_ERROR_INVALID_DATA);

      if (dcId > 3 || (!IntOps::bitTest(decoderI->dcTableMask, dcId) && ssEnd == 0))
        return blTraceError(BL_ERROR_INVALID_DATA);

      // Link the current component to the `index` and update AC & DC selectors.
      DecoderComponent* comp = &decoderI->comp[index];
      comp->dcId = uint8_t(dcId);
      comp->acId = uint8_t(acId);
      sos.scComp[i] = comp;
    }

    consumedBytes = size;
    return BL_SUCCESS;
  }

  // APP - Application
  // -----------------

  if (isMarkerAPP(m)) {
    GET_PAYLOAD_SIZE(2);

    // APP0 - "JFIF\0"
    // ---------------

    if (m == kMarkerAPP0 && remain >= 5 && memcmp(p, "JFIF", 5) == 0) {
      if (blTestFlag(decoderI->statusFlags, DecoderStatusFlags::kDoneJFIF))
        return blTraceError(BL_ERROR_INVALID_DATA);

      if (remain < 14)
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint32_t jfifMajor = p[5];
      uint32_t jfifMinor = p[6];

      // Check the density unit, correct it to aspect-only if it's wrong, but
      // don't fail as of one wrong value won't make any difference anyway.
      uint32_t densityUnit = p[7];
      uint32_t xDensity = MemOps::readU16uBE(p + 8);
      uint32_t yDensity = MemOps::readU16uBE(p + 10);

      switch (densityUnit) {
        case kDensityOnlyAspect:
          // TODO: [JPEG]
          break;

        case kDensityPixelsPerIN:
          imageInfo.density.reset(double(int(xDensity)) * 39.3701,
                                  double(int(yDensity)) * 39.3701);
          break;

        case kDensityPixelsPerCM:
          imageInfo.density.reset(double(int(xDensity * 100)),
                                  double(int(yDensity * 100)));
          break;

        default:
          break;
      }

      uint32_t thumbW = p[12];
      uint32_t thumbH = p[13];

      decoderI->statusFlags |= DecoderStatusFlags::kDoneJFIF;
      decoderI->jfifMajor = uint8_t(jfifMajor);
      decoderI->jfifMinor = uint8_t(jfifMinor);

      if (thumbW && thumbH) {
        uint32_t thumbSize = thumbW * thumbH * 3;

        if (thumbSize + 14 < remain)
          return blTraceError(BL_ERROR_INVALID_DATA);

        DecoderThumbnail& thumb = decoderI->thumb;
        thumb.format = kThumbnailRGB24;
        thumb.w = uint8_t(thumbW);
        thumb.h = uint8_t(thumbH);
        thumb.index = decoderI->bufferIndex + 18;
        thumb.size = thumbSize;
        decoderI->statusFlags |= DecoderStatusFlags::kHasThumb;
      }
    }

    // APP0 - "JFXX\0"
    // ---------------

    if (m == kMarkerAPP0 && remain >= 5 && memcmp(p, "JFXX", 5) == 0) {
      if (blTestFlag(decoderI->statusFlags, DecoderStatusFlags::kDoneJFXX))
        return blTraceError(BL_ERROR_INVALID_DATA);

      if (remain < 6)
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint32_t format = p[5];
      uint32_t thumbW = 0;
      uint32_t thumbH = 0;
      uint32_t thumbSize = 0;

      switch (format) {
        case kThumbnailJPEG:
          // Cannot overflow as the payload size is just 16-bit uint.
          thumbSize = uint32_t(remain - 6);
          break;

        case kThumbnailPAL8:
          thumbW = p[6];
          thumbH = p[7];
          thumbSize = 768 + thumbW * thumbH;
          break;

        case kThumbnailRGB24:
          thumbW = p[6];
          thumbH = p[7];
          thumbSize = thumbW * thumbH * 3;
          break;

        default:
          return blTraceError(BL_ERROR_INVALID_DATA);
      }

      if (thumbSize + 6 > remain)
        return blTraceError(BL_ERROR_INVALID_DATA);

      decoderI->thumb.format = uint8_t(format);
      decoderI->thumb.w = uint8_t(thumbW);
      decoderI->thumb.h = uint8_t(thumbH);
      decoderI->thumb.index = decoderI->bufferIndex + 10;
      decoderI->thumb.size = thumbSize;

      decoderI->statusFlags |= DecoderStatusFlags::kDoneJFXX | DecoderStatusFlags::kHasThumb;
    }

    // APP1 - "EXIF\0\0"
    // -----------------
    /*
    // TODO: [JPEG] This would require some work to make this possible.
    if (m == kMarkerAPP1 && remain >= 6 && memcmp(p, "Exif\0", 6) == 0) {
      // These should be only one EXIF marker in the whole JPEG image, not sure
      // what to do if there is more...
      if (!(decoderI->statusFlags & BL_JPEG_DECODER_DONE_EXIF)) {
        p += 6;
        remain -= 6;

        // Need at least more 8 bytes required by TIFF header.
        if (remain < 8)
          return blTraceError(kErrorExifInvalidHeader);

        // Check if the EXIF marker has a proper TIFF header.
        uint32_t byteOrder;
        uint32_t doByteSwap;

        if (memcmp(p, blJpegExifLE, 4))
          byteOrder = BL_BYTE_ORDER_LE;
        else if (memcmp(p, blJpegExifBE, 4))
          byteOrder = BL_BYTE_ORDER_BE;
        else
          return blTraceError(kErrorExifInvalidHeader);

        doByteSwap = byteOrder != BL_BYTE_ORDER_NATIVE;
        decoderI->statusFlags |= BL_JPEG_DECODER_DONE_EXIF;
      }
    }
    */

    consumedBytes = size;
    return BL_SUCCESS;
  }

  // COM - Comment
  // -------------

  if (m == kMarkerCOM) {
    GET_PAYLOAD_SIZE(2);

    consumedBytes = size;
    return BL_SUCCESS;
  }

  // EOI - End of Image
  // ------------------

  if (m == kMarkerEOI) {
    decoderI->statusFlags |= DecoderStatusFlags::kDoneEOI;
    return BL_SUCCESS;
  }

  // Invalid / Unknown
  // -----------------

  return blTraceError(BL_ERROR_INVALID_DATA);

#undef GET_PAYLOAD_SIZE
}

// bl::Jpeg::Decoder - Process Stream
// ==================================

struct DecoderRun {
  //! Component linked with the run.
  DecoderComponent* comp;

  //! Current data pointer (advanced during decoding).
  uint8_t* data;
  //! De-quantization table pointer.
  const Block<uint16_t>* qTable;

  //! Count of 8x8 blocks required by a single MCU, calculated as `sfW * sfH`.
  uint32_t count;
  //! Stride.
  uint32_t stride;
  //! Horizontal/Vertical advance per MCU.
  uint32_t advance[2];

  //! Offsets of all blocks of this component that are part of a single MCU.
  intptr_t offset[16];
};

// Called after a restart marker (RES) has been reached.
static BLResult decoderHandleRestart(BLJpegDecoderImpl* decoderI, DecoderBitStream& stream, const uint8_t* pEnd) noexcept {
  if (stream.restartCounter == 0 || --stream.restartCounter != 0)
    return BL_SUCCESS;

  // I think this shouldn't be necessary to refill the code buffer/size as all bytes should have been consumed.
  // However, since the spec is so vague, I'm not sure if this is necessary, recommended, or forbidden :(
  DecoderBitReader reader(stream);
  reader.refill();

  if (!reader.atEnd() || (size_t)(pEnd - reader.ptr) < 2 || !isMarkerRST(reader.ptr[1]))
    return blTraceError(BL_ERROR_DECOMPRESSION_FAILED);

  // Skip the marker and flush entropy bits.
  reader.flush();
  reader.advance(2);
  reader.done(stream);

  stream.eobRun = 0;
  stream.restartCounter = decoderI->restartInterval;

  // Reset DC predictions.
  DecoderComponent* comp = decoderI->comp;
  comp[0].dcPred = 0;
  comp[1].dcPred = 0;
  comp[2].dcPred = 0;
  comp[3].dcPred = 0;
  return BL_SUCCESS;
}

//! Decode a baseline 8x8 block.
static BLResult decoderReadBaselineBlock(BLJpegDecoderImpl* decoderI, DecoderBitStream& stream, DecoderComponent* comp, int16_t* dst) noexcept {
  const DecoderHuffmanTable* dcTable = &decoderI->dcTable[comp->dcId];
  const DecoderHuffmanTable* acTable = &decoderI->acTable[comp->acId];

  DecoderBitReader reader(stream);
  reader.refill();

  // Decode DC - Maximum Bytes Consumed: 4 (unescaped)
  // -------------------------------------------------

  uint32_t s;
  int32_t dcPred = comp->dcPred;
  BL_PROPAGATE(reader.readCode(s, dcTable));

  if (s) {
    reader.refillIf32Bit();
    BL_PROPAGATE(reader.requireBits(s));

    int32_t dcVal = reader.readSigned(s);
    dcPred += dcVal;
    comp->dcPred = dcPred;
  }
  dst[0] = int16_t(dcPred);

  // Decode AC - Maximum Bytes Consumed: 4 * 63 (unescaped)
  // ------------------------------------------------------

  uint32_t k = 1;
  const int16_t* acAccel = decoderI->acTable[comp->acId].acAccel;

  do {
    reader.refill();

    uint32_t c = reader.peek<uint32_t>(kHuffmanAccelBits);
    int32_t ac = acAccel[c];

    // Fast AC.
    if (ac) {
      s = (ac & 15);       // Size.
      k += (ac >> 4) & 15; // Skip.
      ac >>= 8;
      reader.drop(s);
      dst[decoderDeZigZagTable[k++]] = int16_t(ac);
    }
    else {
      BL_PROPAGATE(reader.readCode(ac, acTable));
      s = ac & 15;
      ac >>= 4;

      if (s == 0) {
        // End block.
        if (ac != 0xF)
          break;
        k += 16;
      }
      else {
        k += uint32_t(ac);

        reader.refillIf32Bit();
        BL_PROPAGATE(reader.requireBits(s));

        ac = reader.readSigned(s);
        dst[decoderDeZigZagTable[k++]] = int16_t(ac);
      }
    }
  } while (k < 64);

  reader.done(stream);
  return BL_SUCCESS;
}

//! Decode a progressive 8x8 block (AC or DC coefficients, but never both).
static BLResult decoderReadProgressiveBlock(BLJpegDecoderImpl* decoderI, DecoderBitStream& stream, DecoderComponent* comp, int16_t* dst) noexcept {
  DecoderBitReader reader(stream);
  reader.refill();

  uint32_t k     = uint32_t(decoderI->sos.ssStart);
  uint32_t kEnd  = uint32_t(decoderI->sos.ssEnd) + 1;
  uint32_t shift = decoderI->sos.saLowBit;

  // Decode DC - Maximum Bytes Consumed: 4 (unescaped)
  // -------------------------------------------------

  if (k == 0) {
    const DecoderHuffmanTable* dcTable = &decoderI->dcTable[comp->dcId];
    uint32_t s;

    if (decoderI->sos.saHighBit == 0) {
      // Initial scan for DC coefficient.
      int32_t dcPred = comp->dcPred;
      BL_PROPAGATE(reader.readCode(s, dcTable));

      if (s) {
        reader.refillIf32Bit();
        BL_PROPAGATE(reader.requireBits(s));

        int32_t dcVal = reader.readSigned(s);
        dcPred += dcVal;
        comp->dcPred = dcPred;
      }

      dst[0] = int16_t(IntOps::shl(dcPred, shift));
    }
    else {
      // Refinement scan for DC coefficient.
      BL_PROPAGATE(reader.requireBits(1));

      s = reader.readBit<uint32_t>();
      dst[0] = int16_t(int32_t(dst[0]) + int32_t(s << shift));
    }

    k++;
  }

  // Decode AC - Maximum Bytes Consumed: max(4 * 63, 8) (unescaped)
  // --------------------------------------------------------------

  if (k < kEnd) {
    const DecoderHuffmanTable* acTable = &decoderI->acTable[comp->acId];
    const int16_t* acAccel = decoderI->acTable[comp->acId].acAccel;

    if (decoderI->sos.saHighBit == 0) {
      // Initial scan for AC coefficients.
      if (stream.eobRun) {
        stream.eobRun--;
        return BL_SUCCESS;
      }

      do {
        // Fast AC.
        reader.refill();
        int32_t r = acAccel[reader.peek(kHuffmanAccelBits)];

        if (r) {
          int32_t s = r & 15;
          k += (r >> 4) & 15;
          reader.drop(uint32_t(s));

          uint32_t zig = decoderDeZigZagTable[k++];
          dst[zig] = int16_t(IntOps::shl(r >> 8, shift));
        }
        else {
          BL_PROPAGATE(reader.readCode(r, acTable));
          reader.refillIf32Bit();

          int32_t s = r & 15;
          r >>= 4;

          if (s == 0) {
            if (r < 15) {
              uint32_t eobRun = 0;
              if (r) {
                BL_PROPAGATE(reader.requireBits(uint32_t(r)));
                eobRun = reader.readUnsigned(uint32_t(r));
              }
              stream.eobRun = eobRun + (1u << r) - 1;
              break;
            }
            k += 16;
          }
          else {
            k += uint32_t(r);
            r = reader.readSigned(uint32_t(s));

            uint32_t zig = decoderDeZigZagTable[k++];
            dst[zig] = int16_t(IntOps::shl(r, shift));
          }
        }
      } while (k < kEnd);
    }
    else {
      // Refinement scan for AC coefficients.
      int32_t bit = int32_t(1) << shift;
      if (stream.eobRun) {
        do {
          int16_t* p = &dst[decoderDeZigZagTable[k++]];
          int32_t pVal = *p;

          if (pVal) {
            BL_PROPAGATE(reader.requireBits(1));
            uint32_t b = reader.readBit<uint32_t>();

            reader.refill();
            if (b && (pVal & bit) == 0)
              *p = int16_t(pVal + (pVal > 0 ? bit : -bit));
          }
        } while (k < kEnd);
        stream.eobRun--;
      }
      else {
        do {
          int32_t r, s;

          reader.refill();
          BL_PROPAGATE(reader.readCode(r, acTable));

          reader.refillIf32Bit();
          s = r & 15;
          r >>= 4;

          if (s == 0) {
            if (r < 15) {
              uint32_t eobRun = 0;
              if (r) {
                BL_PROPAGATE(reader.requireBits(uint32_t(r)));
                eobRun = reader.readUnsigned(uint32_t(r));
              }
              stream.eobRun = eobRun + (1u << r) - 1;
              r = 64; // Force end of block.
            }
            // r=15 s=0 already does the right thing (write 16 0s).
          }
          else {
            if (BL_UNLIKELY(s != 1))
              return blTraceError(BL_ERROR_DECOMPRESSION_FAILED);

            BL_PROPAGATE(reader.requireBits(1));
            uint32_t sign = reader.readBit<uint32_t>();
            s = sign ? bit : -bit;
          }

          // Advance by `r`.
          while (k < kEnd) {
            int16_t* p = &dst[decoderDeZigZagTable[k++]];
            int32_t pVal = *p;

            if (pVal) {
              uint32_t b;

              reader.refill();
              BL_PROPAGATE(reader.requireBits(1));

              b = reader.readBit<uint32_t>();
              if (b && (pVal & bit) == 0)
                *p = int16_t(pVal + (pVal > 0 ? bit : -bit));
            }
            else {
              if (r == 0) {
                *p = int16_t(s);
                break;
              }
              r--;
            }
          }
        } while (k < kEnd);
      }
    }
  }

  reader.done(stream);
  return BL_SUCCESS;
}

static BLResult decoderProcessStream(BLJpegDecoderImpl* decoderI, const uint8_t* p, size_t remain, size_t& consumedBytes) noexcept {
  DecoderSOS& sos = decoderI->sos;

  const uint8_t* start = p;
  const uint8_t* end = p + remain;

  // Initialize
  // ----------

  // Just needed to determine the logic.
  uint32_t sofMarker = decoderI->sofMarker;

  // Whether the stream is baseline or progressive. Progressive streams use multiple SOS markers to progressively
  // update the image being decoded.
  bool isBaseline = sofMarker != kMarkerSOF2;

  // If this is a baseline stream then the unit-size is 1 byte, because the block of coefficients is immediately
  // IDCTed to pixel values after it is decoded. However, progressive decoding cannot use this space optimization
  // as coefficients are updated progressively.
  uint32_t unitSize = isBaseline ? 1 : 2;

  // Initialize the entropy stream.
  DecoderBitStream stream;
  stream.reset(p, end);
  stream.restartCounter = decoderI->restartInterval;

  uint32_t i;
  uint32_t scCount = sos.scCount;

  uint32_t mcuX = 0;
  uint32_t mcuY = 0;

  // TODO: [JPEG] This is not right, we must calculate MCU W/H every time.
  uint32_t mcuW = decoderI->mcu.count.w;
  uint32_t mcuH = decoderI->mcu.count.h;

  // A single component's decoding doesn't use interleaved MCUs.
  if (scCount == 1) {
    DecoderComponent* comp = sos.scComp[0];
    mcuW = (comp->pxW + kDctSize - 1) / kDctSize;
    mcuH = (comp->pxH + kDctSize - 1) / kDctSize;
  }

  // Initialize decoder runs (each run specifies one component per scan).
  DecoderRun runs[4];
  for (i = 0; i < scCount; i++) {
    DecoderRun* run = &runs[i];
    DecoderComponent* comp = sos.scComp[i];

    uint32_t sfW = scCount > 1 ? uint32_t(comp->sfW) : uint32_t(1);
    uint32_t sfH = scCount > 1 ? uint32_t(comp->sfH) : uint32_t(1);

    uint32_t count = 0;
    uint32_t offset = 0;

    if (isBaseline) {
      uint32_t stride = comp->osW * unitSize;

      for (uint32_t y = 0; y < sfH; y++) {
        for (uint32_t x = 0; x < sfW; x++) {
          run->offset[count++] = intptr_t(offset + x * unitSize * kDctSize);
        }
        offset += stride * kDctSize;
      }

      run->comp = comp;
      run->data = comp->data;
      run->qTable = &decoderI->qTable[comp->quantId];

      run->count = count;
      run->stride = stride;
      run->advance[0] = sfW * unitSize * kDctSize;
      run->advance[1] = run->advance[0] + (sfH * kDctSize - 1) * stride;
    }
    else {
      uint32_t blockSize = unitSize * kDctSize2;
      uint32_t blockStride = comp->blW * blockSize;

      for (uint32_t y = 0; y < sfH; y++) {
        for (uint32_t x = 0; x < sfW; x++) {
          run->offset[count++] = intptr_t(offset + x * blockSize);
        }
        offset += blockStride;
      }

      run->comp = comp;
      run->data = reinterpret_cast<uint8_t*>(comp->coeff);
      run->qTable = nullptr;

      run->count = count;
      run->stride = 0;

      run->advance[0] = sfW * blockSize;
      run->advance[1] = sfH * blockStride - (mcuW - 1) * run->advance[0];
    }
  }

  // SOF0/1 - Baseline / Extended
  // ----------------------------

  if (sofMarker == kMarkerSOF0 || sofMarker == kMarkerSOF1) {
    Block<int16_t> tmpBlock;

    for (;;) {
      // Increment it here so we can use `mcuX == mcuW` in the inner loop.
      mcuX++;

      // Decode all blocks required by a single MCU.
      for (i = 0; i < scCount; i++) {
        DecoderRun* run = &runs[i];
        uint8_t* blockData = run->data;
        uint32_t blockCount = run->count;

        for (uint32_t n = 0; n < blockCount; n++) {
          tmpBlock.reset();
          BL_PROPAGATE(decoderReadBaselineBlock(decoderI, stream, run->comp, tmpBlock.data));
          opts.idct8(blockData + run->offset[n], intptr_t(run->stride), tmpBlock.data, run->qTable->data);
        }

        run->data = blockData + run->advance[mcuX == mcuW];
      }

      // Advance.
      if (mcuX == mcuW) {
        if (++mcuY == mcuH)
          break;
        mcuX = 0;
      }

      // Restart.
      BL_PROPAGATE(decoderHandleRestart(decoderI, stream, end));
    }
  }

  // SOF2 - Progressive
  // ------------------

  else if (sofMarker == kMarkerSOF2) {
    for (;;) {
      // Increment it here so we can use `mcuX == mcuW` in the inner loop.
      mcuX++;

      // Decode all blocks required by a single MCU.
      for (i = 0; i < scCount; i++) {
        DecoderRun* run = &runs[i];

        uint8_t* blockData = run->data;
        uint32_t blockCount = run->count;

        for (uint32_t n = 0; n < blockCount; n++) {
          BL_PROPAGATE(decoderReadProgressiveBlock(decoderI, stream, run->comp,
            reinterpret_cast<int16_t*>(blockData + run->offset[n])));
        }

        run->data = blockData + run->advance[mcuX == mcuW];
      }

      // Advance.
      if (mcuX == mcuW) {
        if (++mcuY == mcuH)
          break;
        mcuX = 0;
      }

      // Restart.
      BL_PROPAGATE(decoderHandleRestart(decoderI, stream, end));
    }
  }

  // End
  // ---

  else {
    BL_NOT_REACHED();
  }

  p = stream.ptr;

  // Skip zeros at the end of the entropy stream that was not consumed `refill()`
  while (p != end && p[0] == 0x00)
    p++;

  consumedBytes = (size_t)(p - start);
  return BL_SUCCESS;
}

// bl:::Jpeg::Decoder - Process MCUs
// =================================

static BLResult decoderProcessMCUs(BLJpegDecoderImpl* decoderI) noexcept {
  if (decoderI->sofMarker == kMarkerSOF2) {
    uint32_t componentCount = decoderI->imageInfo.planeCount;

    // Dequantize & IDCT.
    for (uint32_t n = 0; n < componentCount; n++) {
      DecoderComponent& comp = decoderI->comp[n];

      uint32_t w = (comp.pxW + 7) >> 3;
      uint32_t h = (comp.pxH + 7) >> 3;
      const Block<uint16_t>* qTable = &decoderI->qTable[comp.quantId];

      for (uint32_t j = 0; j < h; j++) {
        for (uint32_t i = 0; i < w; i++) {
          int16_t *data = comp.coeff + 64 * (i + j * comp.blW);
          opts.idct8(comp.data + comp.osW * j * 8 + i * 8, intptr_t(comp.osW), data, qTable->data);
        }
      }
    }
  }

  return BL_SUCCESS;
}

// bl::Jpeg::Decoder - ConvertToRGB
// ================================

struct DecoderUpsample {
  uint8_t* line[2];

  // Expansion factor in each axis.
  uint32_t hs, vs;
  // Horizontal pixels pre-expansion.
  uint32_t w_lores;
  // How far through vertical expansion we are.
  uint32_t ystep;
  // Which pre-expansion row we're on.
  uint32_t ypos;
  // Selected upsample function.
  uint8_t* (BL_CDECL* upsample)(uint8_t* out, uint8_t* in0, uint8_t* in1, uint32_t w, uint32_t hs) noexcept;
};

static BLResult decoderConvertToRGB(BLJpegDecoderImpl* decoderI, BLImageData& dst) noexcept {
  uint32_t w = uint32_t(decoderI->imageInfo.size.w);
  uint32_t h = uint32_t(decoderI->imageInfo.size.h);

  BL_ASSERT(uint32_t(dst.size.w) >= w);
  BL_ASSERT(uint32_t(dst.size.h) >= h);

  uint8_t* dstLine = static_cast<uint8_t*>(dst.pixelData);
  intptr_t dstStride = dst.stride;

  bl::ScopedBufferTmp<1024 * 3 + 16> tmpMem;

  // Allocate a line buffer that's big enough for up-sampling off the edges with up-sample factor of 4.
  uint32_t componentCount = decoderI->imageInfo.planeCount;
  BL_ASSERT(componentCount > 0u && componentCount <= 4u);

  uint32_t lineStride = IntOps::alignUp(w + 3, 16);
  uint8_t* lineBuffer = static_cast<uint8_t*>(tmpMem.alloc(lineStride * componentCount));

  if (BL_UNLIKELY(!lineBuffer))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  DecoderUpsample upsample[4];
  uint8_t* pPlane[4];
  uint8_t* pBuffer[4];

  for (uint32_t k = 0; k < componentCount; k++) {
    DecoderComponent& comp = decoderI->comp[k];
    DecoderUpsample* r = &upsample[k];

    pBuffer[k] = lineBuffer + k * lineStride;

    r->hs      = uint32_t(decoderI->mcu.sf.w / comp.sfW);
    r->vs      = uint32_t(decoderI->mcu.sf.h / comp.sfH);
    r->ystep   = r->vs >> 1;
    r->w_lores = (w + r->hs - 1) / r->hs;
    r->ypos    = 0;
    r->line[0] = comp.data;
    r->line[1] = comp.data;

    if      (r->hs == 1 && r->vs == 1) r->upsample = opts.upsample1x1;
    else if (r->hs == 1 && r->vs == 2) r->upsample = opts.upsample1x2;
    else if (r->hs == 2 && r->vs == 1) r->upsample = opts.upsample2x1;
    else if (r->hs == 2 && r->vs == 2) r->upsample = opts.upsample2x2;
    else                               r->upsample = opts.upsampleAny;
  }

  // Now go ahead and resample.
  for (uint32_t y = 0; y < h; y++, dstLine += dstStride) {
    for (uint32_t k = 0; k < componentCount; k++) {
      DecoderComponent& comp = decoderI->comp[k];
      DecoderUpsample* r = &upsample[k];

      int y_bot = r->ystep >= (r->vs >> 1);
      pPlane[k] = r->upsample(pBuffer[k], r->line[y_bot], r->line[1 - y_bot], r->w_lores, r->hs);

      if (++r->ystep >= r->vs) {
        r->ystep = 0;
        r->line[0] = r->line[1];
        if (++r->ypos < comp.pxH)
          r->line[1] += comp.osW;
      }
    }

    uint8_t* pY = pPlane[0];
    if (componentCount == 3) {
      opts.convYCbCr8ToRGB32(dstLine, pY, pPlane[1], pPlane[2], w);
    }
    else {
      for (uint32_t x = 0; x < w; x++) {
        MemOps::writeU32a(dstLine + x * 4, 0xFF000000u + uint32_t(pY[x]) * 0x010101u);
      }
    }
  }

  return BL_SUCCESS;
}

// bl::Jpeg::Decoder - Read Internal
// =================================

static BLResult decoderReadInfoImplInternal(BLJpegDecoderImpl* decoderI, const uint8_t* p, size_t size) noexcept {
  // JPEG file signature is 2 bytes (0xFF, 0xD8) followed by markers, SOF
  // (start of file) marker contains 1 byte signature and at least 8 bytes of
  // data describing basic information of the image.
  if (size < 2 + 8 + 1)
    return blTraceError(BL_ERROR_DATA_TRUNCATED);

  const uint8_t* start = p;
  const uint8_t* end = p + size;

  // Check JPEG signature (SOI marker).
  if (p[0] != 0xFF || p[1] != kMarkerSOI)
    return blTraceError(BL_ERROR_INVALID_SIGNATURE);

  memcpy(decoderI->imageInfo.format, "JPEG", 5);
  memcpy(decoderI->imageInfo.compression, "HUFFMAN", 8);

  p += 2;
  decoderI->statusFlags |= DecoderStatusFlags::kDoneSOI;

  // Process markers until SOF.
  for (;;) {
    decoderI->bufferIndex = (size_t)(p - start);

    if ((size_t)(end - p) < 2)
      return blTraceError(BL_ERROR_DATA_TRUNCATED);

    if (p[0] != 0xFF)
      return blTraceError(BL_ERROR_INVALID_DATA);

    uint32_t m = p[1];
    p += 2;

    // Some files have an extra padding (0xFF) after their blocks, ignore it.
    if (m == kMarkerInvalid) {
      while (p != end && (m = p[0]) == kMarkerInvalid)
        p++;

      if (p == end)
        break;
      p++;
    }

    size_t consumedBytes = 0;
    BL_PROPAGATE(decoderProcessMarker(decoderI, m, p, (size_t)(end - p), consumedBytes));

    BL_ASSERT(consumedBytes < (size_t)(end - p));
    p += consumedBytes;

    // Terminate after SOF has been processed, the rest is handled by `decode()`.
    if (isMarkerSOF(m)) {
      break;
    }
  }

  decoderI->bufferIndex = (size_t)(p - start);
  return BL_SUCCESS;
}

static BLResult decoderReadFrameImplInternal(BLJpegDecoderImpl* decoderI, BLImage* imageOut, const uint8_t* p, size_t size) noexcept {
  const uint8_t* start = p;
  const uint8_t* end = p + size;

  if (size < decoderI->bufferIndex)
    return blTraceError(BL_ERROR_DATA_TRUNCATED);

  p += decoderI->bufferIndex;

  // Process markers.
  //
  // We are already after SOF, which was processed by `decoderReadInfoImplInternal`.
  for (;;) {
    decoderI->bufferIndex = (size_t)(p - start);
    if ((size_t)(end - p) < 2)
      return blTraceError(BL_ERROR_DATA_TRUNCATED);

    if (p[0] != 0xFF)
      return blTraceError(BL_ERROR_INVALID_DATA);

    uint32_t m = p[1];
    p += 2;

    // Some files have an extra padding (0xFF) after their blocks, ignore it.
    if (m == kMarkerInvalid) {
      while (p != end && (m = p[0]) == kMarkerInvalid)
        p++;

      if (p == end)
        break;
      p++;
    }

    // Process the marker.
    {
      size_t consumedBytes = 0;
      BL_PROPAGATE(decoderProcessMarker(decoderI, m, p, (size_t)(end - p), consumedBytes));

      BL_ASSERT((size_t)(end - p) >= consumedBytes);
      p += consumedBytes;
    }

    // EOI - terminate.
    if (m == kMarkerEOI)
      break;

    // SOS - process the entropy coded data-stream that follows SOS.
    if (m == kMarkerSOS) {
      size_t consumedBytes = 0;
      BL_PROPAGATE(decoderProcessStream(decoderI, p, (size_t)(end - p), consumedBytes));

      BL_ASSERT((size_t)(end - p) >= consumedBytes);
      p += consumedBytes;
    }
  }

  // Process MCUs.
  BL_PROPAGATE(decoderProcessMCUs(decoderI));

  // Create the final image and convert YCbCr -> RGB.
  uint32_t w = uint32_t(decoderI->imageInfo.size.w);
  uint32_t h = uint32_t(decoderI->imageInfo.size.h);
  BLFormat format = BL_FORMAT_XRGB32;
  BLImageData imageData;

  BL_PROPAGATE(imageOut->create(int(w), int(h), format));
  BL_PROPAGATE(imageOut->makeMutable(&imageData));
  BL_PROPAGATE(decoderConvertToRGB(decoderI, imageData));

  decoderI->bufferIndex = (size_t)(p - start);
  decoderI->frameIndex++;

  return BL_SUCCESS;
}

// bl::Jpeg::Decoder - Interface
// =============================

static BLResult BL_CDECL decoderRestartImpl(BLImageDecoderImpl* impl) noexcept {
  BLJpegDecoderImpl* decoderI = static_cast<BLJpegDecoderImpl*>(impl);

  decoderI->lastResult = BL_SUCCESS;
  decoderI->frameIndex = 0;
  decoderI->bufferIndex = 0;

  decoderI->allocator.reset();
  decoderI->imageInfo.reset();
  decoderI->statusFlags = DecoderStatusFlags::kNoFlags;
  decoderI->restartInterval = 0;
  decoderI->sofMarker = 0;
  decoderI->colorspace = 0;
  decoderI->delayedHeight = 0;
  decoderI->jfifMajor = 0;
  decoderI->jfifMinor = 0;
  decoderI->dcTableMask = 0;
  decoderI->acTableMask = 0;
  decoderI->qTableMask = 0;
  decoderI->mcu.reset();
  decoderI->sos.reset();
  decoderI->thumb.reset();
  memset(decoderI->comp, 0, sizeof(decoderI->comp));

  return BL_SUCCESS;
}

static BLResult BL_CDECL decoderReadInfoImpl(BLImageDecoderImpl* impl, BLImageInfo* infoOut, const uint8_t* p, size_t size) noexcept {
  BLJpegDecoderImpl* decoderI = static_cast<BLJpegDecoderImpl*>(impl);
  BLResult result = decoderI->lastResult;

  if (decoderI->bufferIndex == 0 && result == BL_SUCCESS) {
    result = decoderReadInfoImplInternal(decoderI, p, size);
    if (result != BL_SUCCESS)
      decoderI->lastResult = result;
  }

  if (infoOut)
    memcpy(infoOut, &decoderI->imageInfo, sizeof(BLImageInfo));

  return result;
}

static BLResult BL_CDECL decoderReadFrameImpl(BLImageDecoderImpl* impl, BLImageCore* imageOut, const uint8_t* p, size_t size) noexcept {
  BLJpegDecoderImpl* decoderI = static_cast<BLJpegDecoderImpl*>(impl);
  BL_PROPAGATE(decoderReadInfoImpl(decoderI, nullptr, p, size));

  if (decoderI->frameIndex)
    return blTraceError(BL_ERROR_NO_MORE_DATA);

  BLResult result = decoderReadFrameImplInternal(decoderI, static_cast<BLImage*>(imageOut), p, size);
  if (result != BL_SUCCESS)
    decoderI->lastResult = result;
  return result;
}

static BLResult BL_CDECL blJpegDecoderImplCreate(BLImageDecoderCore* self) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_IMAGE_DECODER);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLJpegDecoderImpl>(self, info));

  BLJpegDecoderImpl* decoderI = static_cast<BLJpegDecoderImpl*>(self->_d.impl);
  decoderI->ctor(&jpegDecoderVirt, &jpegCodecInstance);
  blCallCtor(decoderI->allocator);
  return decoderRestartImpl(decoderI);
}

static BLResult BL_CDECL decoderDestroyImpl(BLObjectImpl* impl) noexcept {
  BLJpegDecoderImpl* decoderI = static_cast<BLJpegDecoderImpl*>(impl);

  decoderI->allocator.reset();
  decoderI->dtor();
  return blObjectFreeImpl(decoderI);
}

// bl::Jpeg::Codec - Interface
// ===========================

static BLResult BL_CDECL codecDestroyImpl(BLObjectImpl* impl) noexcept {
  // Built-in codecs are never destroyed.
  blUnused(impl);
  return BL_SUCCESS;
}

static uint32_t BL_CDECL codecInspectDataImpl(const BLImageCodecImpl* impl, const uint8_t* data, size_t size) noexcept {
  blUnused(impl);

  // JPEG minimum size and signature (SOI).
  if (size < 2 || data[0] != 0xFF || data[1] != kMarkerSOI)
    return 0;

  // JPEG signature has to be followed by a marker that starts with 0xFF.
  if (size > 2 && data[2] != 0xFF)
    return 0;

  return 100;
}

static BLResult BL_CDECL codecCreateDecoderImpl(const BLImageCodecImpl* impl, BLImageDecoderCore* dst) noexcept {
  blUnused(impl);

  BLImageDecoderCore tmp;
  BL_PROPAGATE(blJpegDecoderImplCreate(&tmp));
  return blImageDecoderAssignMove(dst, &tmp);
}

static BLResult BL_CDECL codecCreateEncoderImpl(const BLImageCodecImpl* impl, BLImageEncoderCore* dst) noexcept {
  blUnused(impl);
  blUnused(dst);

  return blTraceError(BL_ERROR_IMAGE_ENCODER_NOT_PROVIDED);

  // TODO: [JPEG] Encoder
  /*
  BLImageEncoderCore tmp;
  BL_PROPAGATE(blJpegEncoderImplCreate(tmp._d));
  return blImageEncoderAssignMove(dst, &tmp);
  */
}

// bl::Jpeg::Codec - Runtime Registration
// ======================================

void jpegCodecOnInit(BLRuntimeContext* rt, BLArray<BLImageCodec>* codecs) noexcept {
  using namespace bl::Jpeg;

  blUnused(rt);

  BL_DEFINE_STATIC_STRING(jpegExtensions, "jpg|jpeg|jif|jfi|jfif");

  // Initialize JPEG opts.
  opts.idct8             = idct8;
  opts.convYCbCr8ToRGB32 = rgb32_from_ycbcr8;

#ifdef BL_BUILD_OPT_SSE2
  opts.idct8             = idct8_SSE2;
  opts.convYCbCr8ToRGB32 = rgb32_from_ycbcr8_SSE2;
#endif

  opts.upsample1x1       = upsample_1x1;
  opts.upsample1x2       = upsample_1x2;
  opts.upsample2x1       = upsample_2x1;
  opts.upsample2x2       = upsample_2x2;
  opts.upsampleAny       = upsample_generic;

  // Initialize JPEG codec.
  jpegCodec.virt.base.destroy = codecDestroyImpl;
  jpegCodec.virt.base.getProperty = blObjectImplGetProperty;
  jpegCodec.virt.base.setProperty = blObjectImplSetProperty;
  jpegCodec.virt.inspectData = codecInspectDataImpl;
  jpegCodec.virt.createDecoder = codecCreateDecoderImpl;
  jpegCodec.virt.createEncoder = codecCreateEncoderImpl;

  jpegCodec.impl->ctor(&jpegCodec.virt);
  jpegCodec.impl->features =
    BL_IMAGE_CODEC_FEATURE_READ  |
    BL_IMAGE_CODEC_FEATURE_WRITE |
    BL_IMAGE_CODEC_FEATURE_LOSSY ;
  jpegCodec.impl->name.dcast().assign("JPEG");
  jpegCodec.impl->vendor.dcast().assign("Blend2D");
  jpegCodec.impl->mimeType.dcast().assign("image/jpeg");
  bl::StringInternal::initStatic(&jpegCodec.impl->extensions, jpegExtensions);

  jpegCodecInstance._d.initDynamic(BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_IMAGE_CODEC), &jpegCodec.impl);

  // Initialize JPEG decoder virtual functions.
  jpegDecoderVirt.base.destroy = decoderDestroyImpl;
  jpegDecoderVirt.base.getProperty = blObjectImplGetProperty;
  jpegDecoderVirt.base.setProperty = blObjectImplSetProperty;
  jpegDecoderVirt.restart = decoderRestartImpl;
  jpegDecoderVirt.readInfo = decoderReadInfoImpl;
  jpegDecoderVirt.readFrame = decoderReadFrameImpl;

  // Initialize JPEG encoder virtual functions.
  // TODO: [JPEG] Encoder

  codecs->append(jpegCodecInstance.dcast());
}

} // {bl::Jpeg}
