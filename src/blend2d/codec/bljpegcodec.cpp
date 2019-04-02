// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

// The JPEG codec is based on stb_image <https://github.com/nothings/stb>
// released into PUBLIC DOMAIN. Blend2D's JPEG codec can be distributed
// under Blend2D's ZLIB license or under STB's PUBLIC DOMAIN as well.

#include "../blapi-build_p.h"
#include "../blruntime_p.h"
#include "../blsupport_p.h"
#include "../codec/bljpegcodec_p.h"
#include "../codec/bljpeghuffman_p.h"
#include "../codec/bljpegops_p.h"

// ============================================================================
// [Global Variables]
// ============================================================================

static BLJpegCodecImpl blJpegCodecImpl;
static BLImageCodecVirt blJpegCodecVirt;
static BLImageDecoderVirt blJpegDecoderVirt;

// ============================================================================
// [Constants]
// ============================================================================

// Mapping table of zigzagged 8x8 data into a natural order.
static const uint8_t blJpegDeZigZagTable[64 + 16] = {
  0 , 1 , 8 , 16, 9 , 2 , 3 , 10,
  17, 24, 32, 25, 18, 11, 4 , 5 ,
  12, 19, 26, 33, 40, 48, 41, 34,
  27, 20, 13, 6 , 7 , 14, 21, 28,
  35, 42, 49, 56, 57, 50, 43, 36,
  29, 22, 15, 23, 30, 37, 44, 51,
  58, 59, 52, 45, 38, 31, 39, 46,
  53, 60, 61, 54, 47, 55, 62, 63,

  // These are not part of JPEG's spec, however, it's convenient as the decoder
  // doesn't have to check whether the coefficient index is out of bounds.
  63, 63, 63, 63, 63, 63, 63, 63,
  63, 63, 63, 63, 63, 63, 63, 63
};

// TIFF Header used by EXIF (LE or BE).
static const uint8_t blJpegExifLE[4] = { 0x49, 0x49, 0x2A, 0x00 };
static const uint8_t blJpegExifBE[4] = { 0x4D, 0x4D, 0x00, 0x2A };

// ============================================================================
// [BLJpegDecoderImpl - Process Marker]
// ============================================================================

BLResult blJpegDecoderImplProcessMarker(BLJpegDecoderImpl* impl, uint32_t m, const uint8_t* p, size_t remain, size_t& consumedBytes) noexcept {
  // Should be zero when passed in.
  BL_ASSERT(consumedBytes == 0);

  BLImageInfo& imageInfo = impl->imageInfo;

#define GET_PAYLOAD_SIZE(MinSize) \
  size_t size; \
  \
  do { \
    if (remain < MinSize) \
      return blTraceError(BL_ERROR_DATA_TRUNCATED); \
    \
    size = blMemReadU16uBE(p); \
    if (size < MinSize) \
      return blTraceError(BL_ERROR_INVALID_DATA); \
    \
    if (size > remain) \
      return blTraceError(BL_ERROR_DATA_TRUNCATED); \
    \
    p += 2; \
    remain = size - 2; \
  } while (false)

  // --------------------------------------------------------------------------
  // [SOF - Start of Frame]
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
  // --------------------------------------------------------------------------

  if (blJpegMarkerIsSOF(m)) {
    uint32_t sofMarker = m;

    // Forbid multiple SOF markers in a single JPEG file.
    if (impl->sofMarker)
      return blTraceError(BL_ERROR_JPEG_MULTIPLE_SOF);

    // Check if SOF type is supported.
    if (sofMarker != BL_JPEG_MARKER_SOF0 &&
        sofMarker != BL_JPEG_MARKER_SOF1 &&
        sofMarker != BL_JPEG_MARKER_SOF2)
      return blTraceError(BL_ERROR_JPEG_UNSUPPORTED_SOF);

    // 11 bytes is a minimum size of SOF describing exactly one component.
    GET_PAYLOAD_SIZE(2 + 6 + 3);

    uint32_t bpp = p[0];
    uint32_t h = blMemReadU16uBE(p + 1);
    uint32_t w = blMemReadU16uBE(p + 3);
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
      BLJpegDecoderComponent* comp = &impl->comp[i];

      // Check if the ID doesn't collide with previous components.
      uint32_t compId = p[0];
      for (j = 0; j < i; j++) {
        if (impl->comp[j].compId == compId)
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

      // Quantization ID.
      uint32_t quantId = p[2];
      if (quantId > 3)
        return blTraceError(BL_ERROR_INVALID_DATA);

      // Save to BLJpegDecoderComponent.
      // printf("COMPONENT %u %ux%u quantId=%u\n", compId, sfW, sfH, quantId);
      comp->compId  = uint8_t(compId);
      comp->sfW     = uint8_t(sfW);
      comp->sfH     = uint8_t(sfH);
      comp->quantId = uint8_t(quantId);

      // We need to know maximum horizontal and vertical sampling factor to
      // calculate the correct MCU size (WxH).
      mcuSfW = blMax(mcuSfW, sfW);
      mcuSfH = blMax(mcuSfH, sfH);
    }

    // Compute interleaved MCU info.
    uint32_t mcuPxW = mcuSfW * BL_JPEG_DCT_SIZE;
    uint32_t mcuPxH = mcuSfH * BL_JPEG_DCT_SIZE;

    uint32_t mcuCountW = (w + mcuPxW - 1) / mcuPxW;
    uint32_t mcuCountH = (h + mcuPxH - 1) / mcuPxH;
    bool isBaseline = sofMarker != BL_JPEG_MARKER_SOF2;

    for (i = 0; i < componentCount; i++) {
      BLJpegDecoderComponent* comp = &impl->comp[i];

      // Number of effective pixels (e.g. for non-interleaved MCU).
      comp->pxW = (w * uint32_t(comp->sfW) + mcuSfW - 1) / mcuSfW;
      comp->pxH = (h * uint32_t(comp->sfH) + mcuSfH - 1) / mcuSfH;

      // Allocate enough memory for all blocks even those that won't be used fully.
      comp->blW = mcuCountW * uint32_t(comp->sfW);
      comp->blH = mcuCountH * uint32_t(comp->sfH);

      comp->osW = comp->blW * BL_JPEG_DCT_SIZE;
      comp->osH = comp->blH * BL_JPEG_DCT_SIZE;

      comp->data = static_cast<uint8_t*>(impl->allocator.alloc(comp->osW * comp->osH));
      if (comp->data == nullptr)
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

      if (!isBaseline) {
        uint32_t kBlock8x8UInt16 = BL_JPEG_DCT_SIZE_2 * uint32_t(sizeof(int16_t));
        size_t coeffSize = comp->blW * comp->blH * kBlock8x8UInt16;
        int16_t* coeffData = static_cast<int16_t*>(impl->allocator.alloc(coeffSize, 16));

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
    imageInfo.planeCount = componentCount;
    imageInfo.frameCount = 1;

    if (!isBaseline)
      imageInfo.flags |= BL_IMAGE_INFO_FLAG_PROGRESSIVE;

    impl->sofMarker = sofMarker;
    impl->delayedHeight = (h == 0);
    impl->mcu.sf.w = uint8_t(mcuSfW);
    impl->mcu.sf.h = uint8_t(mcuSfH);
    impl->mcu.px.w = uint8_t(mcuPxW);
    impl->mcu.px.h = uint8_t(mcuPxH);
    impl->mcu.count.w = mcuCountW;
    impl->mcu.count.h = mcuCountH;

    consumedBytes = size;
    return BL_SUCCESS;
  }

  // --------------------------------------------------------------------------
  // [DHT - Define Huffman Table]
  //
  //        WORD - Size (Consumed by GET_PAYLOAD_...)
  //
  //   [00] BYTE - Table class `tc` and table identifier `ti`.
  //   [01] 16xB - The count of Huffman codes of size 1..16.
  //
  //   [17] .... - The one byte symbols sorted by Huffman code. The number of
  //               symbols is the sum of the 16 code counts.
  // --------------------------------------------------------------------------

  if (m == BL_JPEG_MARKER_DHT) {
    GET_PAYLOAD_SIZE(2 + 17);

    while (remain) {
      uint32_t q = *p++;
      remain--;

      uint32_t tableClass = q >> 4; // Table class.
      uint32_t tableId = q & 15; // Table id (0-3).

      // Invalid class or id.
      if (tableClass >= BL_JPEG_TABLE_COUNT || tableId > 3)
        return blTraceError(BL_ERROR_INVALID_DATA);

      size_t tableSizeInBytes;
      if (tableClass == BL_JPEG_TABLE_DC) {
        BL_PROPAGATE(blJpegDecoderBuildHuffmanDC(&impl->dcTable[tableId], p, remain, &tableSizeInBytes));
        impl->dcTableMask = uint8_t(impl->dcTableMask | blBitMask<uint32_t>(tableId));
      }
      else {
        BL_PROPAGATE(blJpegDecoderBuildHuffmanAC(&impl->acTable[tableId], p, remain, &tableSizeInBytes));
        impl->acTableMask = uint8_t(impl->acTableMask | blBitMask<uint32_t>(tableId));
      }

      p += tableSizeInBytes;
      remain -= tableSizeInBytes;
    }

    consumedBytes = size;
    return BL_SUCCESS;
  }

  // --------------------------------------------------------------------------
  // [DQT - Define Quantization Table]
  //
  //        WORD - Size (Consumed by GET_PAYLOAD_...)
  //
  //   [00] BYTE - Quantization value size `quantSz` (0-1) and table identifier
  //               `quantId`.
  //   [01] .... - 64 or 128 bytes depending on `qs`.
  // --------------------------------------------------------------------------

  if (m == BL_JPEG_MARKER_DQT) {
    GET_PAYLOAD_SIZE(2 + 65);

    while (remain >= 65) {
      uint32_t q = *p++;

      uint32_t qSize = q >> 4;
      uint32_t qId = q & 15;

      if (qSize > 1 || qId > 3)
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint16_t* qTable = impl->qTable[qId].data;
      uint32_t requiredSize = 1 + 64 * (qSize + 1);

      if (requiredSize > remain)
        break;

      if (qSize == 0) {
        for (uint32_t k = 0; k < 64; k++, p++)
          qTable[blJpegDeZigZagTable[k]] = *p;
      }
      else {
        for (uint32_t k = 0; k < 64; k++, p += 2)
          qTable[blJpegDeZigZagTable[k]] = blMemReadU16uBE(reinterpret_cast<const uint16_t*>(p));
      }

      impl->qTableMask = uint8_t(impl->qTableMask | blBitMask<uint8_t>(qId));
      remain -= requiredSize;
    }

    if (remain != 0)
      return blTraceError(BL_ERROR_INVALID_DATA);

    consumedBytes = size;
    return BL_SUCCESS;
  }

  // --------------------------------------------------------------------------
  // [DRI - Define Restart Interval]
  //
  //        WORD - Size (Consumed by GET_PAYLOAD_...)
  //
  //   [00] WORD - Restart interval.
  // --------------------------------------------------------------------------

  if (m == BL_JPEG_MARKER_DRI) {
    if (remain < 4)
      return blTraceError(BL_ERROR_DATA_TRUNCATED);

    size_t size = blMemReadU16uBE(p + 0);
    uint32_t ri = blMemReadU16uBE(p + 2);

    // DRI payload should be 4 bytes.
    if (size != 4)
      return blTraceError(BL_ERROR_INVALID_DATA);

    impl->restartInterval = ri;
    consumedBytes = size;
    return BL_SUCCESS;
  }

  // --------------------------------------------------------------------------
  // [SOS - Start of Scan]
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
  // --------------------------------------------------------------------------

  if (m == BL_JPEG_MARKER_SOS) {
    GET_PAYLOAD_SIZE(2 + 6);

    uint32_t sofMarker = impl->sofMarker;
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

    if (sofMarker == BL_JPEG_MARKER_SOF0 || sofMarker == BL_JPEG_MARKER_SOF1) {
      if (ssStart != 0 || saLowBit != 0 || saHighBit != 0)
        return blTraceError(BL_ERROR_INVALID_DATA);

      // The value should be 63, but it's zero sometimes.
      ssEnd = 63;
    }

    if (sofMarker == BL_JPEG_MARKER_SOF2) {
      if (ssStart > 63 || ssEnd > 63 || ssStart > ssEnd || saLowBit > 13 || saHighBit > 13)
        return blTraceError(BL_ERROR_INVALID_DATA);

      // AC & DC cannot be merged in a progressive JPEG.
      if (ssStart == 0 && ssEnd != 0)
        return blTraceError(BL_ERROR_INVALID_DATA);
    }

    BLJpegDecoderSOS& sos = impl->sos;
    sos.scCount   = scCount;
    sos.ssStart   = uint8_t(ssStart);
    sos.ssEnd     = uint8_t(ssEnd);
    sos.saLowBit  = uint8_t(saLowBit);
    sos.saHighBit = uint8_t(saHighBit);

    for (uint32_t i = 0; i < scCount; i++, p += 2) {
      uint32_t compId = p[0];
      uint32_t index = 0;

      while (impl->comp[index].compId != compId)
        if (++index >= componentCount)
          return blTraceError(BL_ERROR_JPEG_INVALID_SOS);

      // One huffman stream shouldn't overwrite the same component.
      if (blBitTest(scMask, index))
        return blTraceError(BL_ERROR_JPEG_INVALID_SOS);

      scMask |= blBitMask<uint32_t>(index);

      uint32_t selector = p[1];
      uint32_t acId = selector & 15;
      uint32_t dcId = selector >> 4;

      // Validate AC & DC selectors.
      if (acId > 3 || (!blBitTest(impl->acTableMask, acId) && ssEnd  > 0))
        return blTraceError(BL_ERROR_INVALID_DATA);

      if (dcId > 3 || (!blBitTest(impl->dcTableMask, dcId) && ssEnd == 0))
        return blTraceError(BL_ERROR_INVALID_DATA);

      // Link the current component to the `index` and update AC & DC selectors.
      BLJpegDecoderComponent* comp = &impl->comp[index];
      comp->dcId = uint8_t(dcId);
      comp->acId = uint8_t(acId);
      sos.scComp[i] = comp;
    }

    consumedBytes = size;
    return BL_SUCCESS;
  }

  // --------------------------------------------------------------------------
  // [APP - Application]
  // --------------------------------------------------------------------------

  if (blJpegMarkerIsAPP(m)) {
    GET_PAYLOAD_SIZE(2);

    // ------------------------------------------------------------------------
    // [APP0 - "JFIF\0"]
    // ------------------------------------------------------------------------

    if (m == BL_JPEG_MARKER_APP0 && remain >= 5 && memcmp(p, "JFIF", 5) == 0) {
      if (impl->statusFlags & BL_JPEG_DECODER_DONE_JFIF)
        return blTraceError(BL_ERROR_INVALID_DATA);

      if (remain < 14)
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint32_t jfifMajor = p[5];
      uint32_t jfifMinor = p[6];

      // Check the density unit, correct it to aspect-only if it's wrong, but
      // don't fail as of one wrong value won't make any difference anyway.
      uint32_t densityUnit = p[7];
      uint32_t xDensity = blMemReadU16uBE(p + 8);
      uint32_t yDensity = blMemReadU16uBE(p + 10);

      switch (densityUnit) {
        case BL_JPEG_DENSITY_ONLY_ASPECT:
          // TODO: [JPEG]
          break;

        case BL_JPEG_DENSITY_PIXELS_PER_IN:
          imageInfo.density.reset(double(int(xDensity)) * 39.3701,
                                  double(int(yDensity)) * 39.3701);
          break;

        case BL_JPEG_DENSITY_PIXELS_PER_CM:
          imageInfo.density.reset(double(int(xDensity * 100)),
                                  double(int(yDensity * 100)));
          break;

        default:
          densityUnit = BL_JPEG_DENSITY_ONLY_ASPECT;
          break;
      }

      uint32_t thumbW = p[12];
      uint32_t thumbH = p[13];

      impl->statusFlags |= BL_JPEG_DECODER_DONE_JFIF;
      impl->jfifMajor = jfifMajor;
      impl->jfifMinor = jfifMinor;

      if (thumbW && thumbH) {
        uint32_t thumbSize = thumbW * thumbH * 3;

        if (thumbSize + 14 < remain)
          return blTraceError(BL_ERROR_INVALID_DATA);

        BLJpegDecoderThumbnail& thumb = impl->thumb;
        thumb.format = BL_JPEG_THUMBNAIL_RGB24;
        thumb.w = uint8_t(thumbW);
        thumb.h = uint8_t(thumbH);
        thumb.index = impl->bufferIndex + 18;
        thumb.size = thumbSize;
        impl->statusFlags |= BL_JPEG_DECODER_HAS_THUMB;
      }
    }

    // ------------------------------------------------------------------------
    // [APP0 - "JFXX\0"]
    // ------------------------------------------------------------------------

    if (m == BL_JPEG_MARKER_APP0 && remain >= 5 && memcmp(p, "JFXX", 5) == 0) {
      if (impl->statusFlags & BL_JPEG_DECODER_DONE_JFXX)
        return blTraceError(BL_ERROR_INVALID_DATA);

      if (remain < 6)
        return blTraceError(BL_ERROR_INVALID_DATA);

      uint32_t format = p[5];
      uint32_t thumbW = 0;
      uint32_t thumbH = 0;
      uint32_t thumbSize = 0;

      switch (format) {
        case BL_JPEG_THUMBNAIL_JPEG:
          // Cannot overflow as the payload size is just 16-bit uint.
          thumbSize = uint32_t(remain - 6);
          break;

        case BL_JPEG_THUMBNAIL_PAL8:
          thumbW = p[6];
          thumbH = p[7];
          thumbSize = 768 + thumbW * thumbH;
          break;

        case BL_JPEG_THUMBNAIL_RGB24:
          thumbW = p[6];
          thumbH = p[7];
          thumbSize = thumbW * thumbH * 3;
          break;

        default:
          return blTraceError(BL_ERROR_INVALID_DATA);
      }

      if (thumbSize + 6 > remain)
        return blTraceError(BL_ERROR_INVALID_DATA);

      impl->thumb.format = format;
      impl->thumb.w = uint8_t(thumbW);
      impl->thumb.h = uint8_t(thumbH);
      impl->thumb.index = impl->bufferIndex + 10;
      impl->thumb.size = thumbSize;

      impl->statusFlags |= BL_JPEG_DECODER_DONE_JFXX | BL_JPEG_DECODER_HAS_THUMB;
    }

    // ------------------------------------------------------------------------
    // [APP1 - "EXIF\0\0"]
    // ------------------------------------------------------------------------
    /*
    // TODO: [JPEG] This would require some work to make this possible.
    if (m == BL_JPEG_MARKER_APP1 && remain >= 6 && memcmp(p, "Exif\0", 6) == 0) {
      // These should be only one EXIF marker in the whole JPEG image, not sure
      // what to do if there is more...
      if (!(impl->statusFlags & BL_JPEG_DECODER_DONE_EXIF)) {
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
        impl->statusFlags |= BL_JPEG_DECODER_DONE_EXIF;
      }
    }
    */

    consumedBytes = size;
    return BL_SUCCESS;
  }

  // --------------------------------------------------------------------------
  // [COM - Comment]
  // --------------------------------------------------------------------------

  if (m == BL_JPEG_MARKER_COM) {
    GET_PAYLOAD_SIZE(2);

    consumedBytes = size;
    return BL_SUCCESS;
  }

  // --------------------------------------------------------------------------
  // [EOI - End of Image]
  // --------------------------------------------------------------------------

  if (m == BL_JPEG_MARKER_EOI) {
    impl->statusFlags |= BL_JPEG_DECODER_DONE_EOI;
    return BL_SUCCESS;
  }

  // --------------------------------------------------------------------------
  // [Invalid / Unknown]
  // --------------------------------------------------------------------------

  return blTraceError(BL_ERROR_INVALID_DATA);

#undef GET_PAYLOAD_SIZE
}

// ============================================================================
// [BLJpegDecoderImpl - Process Stream]
// ============================================================================

struct BLJpegDecoderRun {
  //! Component linked with the run.
  BLJpegDecoderComponent* comp;

  //! Current data pointer (advanced during decoding).
  uint8_t* data;
  //! Dequantization table pointer.
  const BLJpegBlock<uint16_t>* qTable;

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
static BLResult blJpegDecoderImplHandleRestart(BLJpegDecoderImpl* impl, BLJpegDecoderBitStream& stream, const uint8_t* pEnd) noexcept {
  if (stream.restartCounter == 0 || --stream.restartCounter != 0)
    return BL_SUCCESS;

  // I think this shouldn't be necessary to refill the code buffer/size as all
  // bytes should have been consumed. However, since the spec is so vague, I'm
  // not sure if this is necessary, recommended, or forbidden :(
  BLJpegDecoderBitReader reader(stream);
  reader.refill();

  if (!reader.atEnd() || (size_t)(pEnd - reader.ptr) < 2 || !blJpegMarkerIsRST(reader.ptr[1]))
    return blTraceError(BL_ERROR_DECOMPRESSION_FAILED);

  // Skip the marker and flush entropy bits.
  reader.flush();
  reader.advance(2);
  reader.done(stream);

  stream.eobRun = 0;
  stream.restartCounter = impl->restartInterval;

  // Reset DC predictions.
  BLJpegDecoderComponent* comp = impl->comp;
  comp[0].dcPred = 0;
  comp[1].dcPred = 0;
  comp[2].dcPred = 0;
  comp[3].dcPred = 0;
  return BL_SUCCESS;
}

//! Decode a baseline 8x8 block.
static BLResult blJpegDecoderImplReadBaselineBlock(BLJpegDecoderImpl* impl, BLJpegDecoderBitStream& stream, BLJpegDecoderComponent* comp, int16_t* dst) noexcept {
  const BLJpegDecoderHuffmanTable* dcTable = &impl->dcTable[comp->dcId];
  const BLJpegDecoderHuffmanTable* acTable = &impl->acTable[comp->acId];

  BLJpegDecoderBitReader reader(stream);
  reader.refill();

  // --------------------------------------------------------------------------
  // [Decode DC - Maximum Bytes Consumed: 4 (unescaped)]
  // --------------------------------------------------------------------------

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

  // --------------------------------------------------------------------------
  // [Decode AC - Maximum Bytes Consumed: 4 * 63 (unescaped)]
  // --------------------------------------------------------------------------

  uint32_t k = 1;
  const int16_t* acAccel = impl->acTable[comp->acId].acAccel;

  do {
    reader.refill();

    uint32_t c = reader.peek<uint32_t>(BL_JPEG_DECODER_HUFFMAN_ACCEL_BITS);
    int32_t ac = acAccel[c];

    // Fast AC.
    if (ac) {
      s = (ac & 15);       // Size.
      k += (ac >> 4) & 15; // Skip.
      ac >>= 8;
      reader.drop(s);
      dst[blJpegDeZigZagTable[k++]] = int16_t(ac);
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
        k += ac;

        reader.refillIf32Bit();
        BL_PROPAGATE(reader.requireBits(s));

        ac = reader.readSigned(s);
        dst[blJpegDeZigZagTable[k++]] = int16_t(ac);
      }
    }
  } while (k < 64);

  reader.done(stream);
  return BL_SUCCESS;
}

//! Decode a progressive 8x8 block (AC or DC coefficients, but never both).
static BLResult blJpegDecoderImplReadProgressiveBlock(BLJpegDecoderImpl* impl, BLJpegDecoderBitStream& stream, BLJpegDecoderComponent* comp, int16_t* dst) noexcept {
  BLJpegDecoderBitReader reader(stream);
  reader.refill();

  uint32_t k     = uint32_t(impl->sos.ssStart);
  uint32_t kEnd  = uint32_t(impl->sos.ssEnd) + 1;
  uint32_t shift = impl->sos.saLowBit;

  // --------------------------------------------------------------------------
  // [Decode DC - Maximum Bytes Consumed: 4 (unescaped)]
  // --------------------------------------------------------------------------

  if (k == 0) {
    const BLJpegDecoderHuffmanTable* dcTable = &impl->dcTable[comp->dcId];
    uint32_t s;

    if (impl->sos.saHighBit == 0) {
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

      dst[0] = int16_t(blBitShl(dcPred, shift));
    }
    else {
      // Refinement scan for DC coefficient.
      BL_PROPAGATE(reader.requireBits(1));

      s = reader.readBit<uint32_t>();
      dst[0] += int16_t(s << shift);
    }

    k++;
  }

  // --------------------------------------------------------------------------
  // [Decode AC - Maximum Bytes Consumed: max(4 * 63, 8) (unescaped)]
  // --------------------------------------------------------------------------

  if (k < kEnd) {
    const BLJpegDecoderHuffmanTable* acTable = &impl->acTable[comp->acId];
    const int16_t* acAccel = impl->acTable[comp->acId].acAccel;

    if (impl->sos.saHighBit == 0) {
      // Initial scan for AC coefficients.
      if (stream.eobRun) {
        stream.eobRun--;
        return BL_SUCCESS;
      }

      do {
        // Fast AC.
        reader.refill();
        int32_t r = acAccel[reader.peek(BL_JPEG_DECODER_HUFFMAN_ACCEL_BITS)];

        if (r) {
          int32_t s = r & 15;
          k += (r >> 4) & 15;
          reader.drop(s);

          uint32_t zig = blJpegDeZigZagTable[k++];
          dst[zig] = int16_t(blBitShl(r >> 8, shift));
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
                BL_PROPAGATE(reader.requireBits(r));
                eobRun = reader.readUnsigned(r);
              }
              stream.eobRun = eobRun + (1u << r) - 1;
              break;
            }
            k += 16;
          }
          else {
            k += r;
            r = reader.readSigned(s);

            uint32_t zig = blJpegDeZigZagTable[k++];
            dst[zig] = int16_t(blBitShl(r, shift));
          }
        }
      } while (k < kEnd);
    }
    else {
      // Refinement scan for AC coefficients.
      int32_t bit = int32_t(1) << shift;
      if (stream.eobRun) {
        do {
          int16_t* p = &dst[blJpegDeZigZagTable[k++]];
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
                BL_PROPAGATE(reader.requireBits(r));
                eobRun = reader.readUnsigned(r);
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
            int16_t* p = &dst[blJpegDeZigZagTable[k++]];
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

BLResult blJpegDecoderImplProcessStream(BLJpegDecoderImpl* impl, const uint8_t* p, size_t remain, size_t& consumedBytes) noexcept {
  BLJpegDecoderSOS& sos = impl->sos;

  const uint8_t* start = p;
  const uint8_t* end = p + remain;

  // --------------------------------------------------------------------------
  // [Initialize]
  // --------------------------------------------------------------------------

  // Just needed to determine the logic.
  uint32_t sofMarker = impl->sofMarker;

  // Whether the stream is baseline or progressive. Progressive streams use
  // multiple SOS markers to progressively update the image being decoded.
  bool isBaseline = sofMarker != BL_JPEG_MARKER_SOF2;

  // If this is a basline stream then the unit-size is 1 byte, because the
  // block of coefficients is immediately IDCTed to pixel values after it is
  // decoded. However, progressive decoding cannot use this space optimization
  // as coefficients are updated progressively.
  uint32_t unitSize = isBaseline ? 1 : 2;

  // Initialize the entropy stream.
  BLJpegDecoderBitStream stream;
  stream.reset(p, end);
  stream.restartCounter = impl->restartInterval;

  uint32_t i;
  uint32_t scCount = sos.scCount;

  uint32_t mcuX = 0;
  uint32_t mcuY = 0;

  // TODO: [JPEG] This is not right, we must calculate MCU W/H every time.
  uint32_t mcuW = impl->mcu.count.w;
  uint32_t mcuH = impl->mcu.count.h;

  // A single component's decoding doesn't use interleaved MCUs.
  if (scCount == 1) {
    BLJpegDecoderComponent* comp = sos.scComp[0];
    mcuW = (comp->pxW + BL_JPEG_DCT_SIZE - 1) / BL_JPEG_DCT_SIZE;
    mcuH = (comp->pxH + BL_JPEG_DCT_SIZE - 1) / BL_JPEG_DCT_SIZE;
  }

  // printf("SOS %s MCU{%ux%u} scCount=%u\n", isBaseline ? "BASELINE" : "PROGRESSIVE", mcuW, mcuH, scCount);

  // Initialize decoder runs (each run specifies one component per scan).
  BLJpegDecoderRun runs[4];
  for (i = 0; i < scCount; i++) {
    BLJpegDecoderRun* run = &runs[i];
    BLJpegDecoderComponent* comp = sos.scComp[i];

    uint32_t sfW = scCount > 1 ? uint32_t(comp->sfW) : uint32_t(1);
    uint32_t sfH = scCount > 1 ? uint32_t(comp->sfH) : uint32_t(1);

    uint32_t count = 0;
    uint32_t offset = 0;

    if (isBaseline) {
      uint32_t stride = comp->osW * unitSize;

      for (uint32_t y = 0; y < sfH; y++) {
        for (uint32_t x = 0; x < sfW; x++) {
          run->offset[count++] = offset + x * unitSize * BL_JPEG_DCT_SIZE;
        }
        offset += stride * BL_JPEG_DCT_SIZE;
      }

      run->comp = comp;
      run->data = comp->data;
      run->qTable = &impl->qTable[comp->quantId];

      run->count = count;
      run->stride = stride;
      run->advance[0] = sfW * unitSize * BL_JPEG_DCT_SIZE;
      run->advance[1] = run->advance[0] + (sfH * BL_JPEG_DCT_SIZE - 1) * stride;
    }
    else {
      uint32_t blockSize = unitSize * BL_JPEG_DCT_SIZE_2;
      uint32_t blockStride = comp->blW * blockSize;

      for (uint32_t y = 0; y < sfH; y++) {
        for (uint32_t x = 0; x < sfW; x++) {
          run->offset[count++] = offset + x * blockSize;
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

      // printf("  COMP #%u [%ux%u] {count=%u advance[0]=%u advance[1]=%u} SOS{start=%u end=%u}\n", comp->compId, sfW, sfH, run->count, run->advance[0], run->advance[1], sos.ssStart, sos.ssEnd);
    }
  }

  // --------------------------------------------------------------------------
  // [SOF0/1 - Baseline / Extended]
  // --------------------------------------------------------------------------

  if (sofMarker == BL_JPEG_MARKER_SOF0 || sofMarker == BL_JPEG_MARKER_SOF1) {
    BLJpegBlock<int16_t> tmpBlock;

    for (;;) {
      // Increment it here so we can use `mcuX == mcuW` in the inner loop.
      mcuX++;

      // Decode all blocks required by a single MCU.
      for (i = 0; i < scCount; i++) {
        BLJpegDecoderRun* run = &runs[i];
        uint8_t* blockData = run->data;
        uint32_t blockCount = run->count;

        for (uint32_t n = 0; n < blockCount; n++) {
          tmpBlock.reset();
          BL_PROPAGATE(blJpegDecoderImplReadBaselineBlock(impl, stream, run->comp, tmpBlock.data));
          blJpegOps.idct8(blockData + run->offset[n], run->stride, tmpBlock.data, run->qTable->data);
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
      BL_PROPAGATE(blJpegDecoderImplHandleRestart(impl, stream, end));
    }
  }

  // --------------------------------------------------------------------------
  // [SOF2 - Progressive]
  // --------------------------------------------------------------------------

  else if (sofMarker == BL_JPEG_MARKER_SOF2) {
    for (;;) {
      // Increment it here so we can use `mcuX == mcuW` in the inner loop.
      mcuX++;

      // Decode all blocks required by a single MCU.
      for (i = 0; i < scCount; i++) {
        BLJpegDecoderRun* run = &runs[i];

        uint8_t* blockData = run->data;
        uint32_t blockCount = run->count;

        for (uint32_t n = 0; n < blockCount; n++) {
          BL_PROPAGATE(blJpegDecoderImplReadProgressiveBlock(impl, stream, run->comp,
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
      BL_PROPAGATE(blJpegDecoderImplHandleRestart(impl, stream, end));
    }
  }

  // --------------------------------------------------------------------------
  // [End]
  // --------------------------------------------------------------------------

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

// ============================================================================
// [BLJpegDecoderImpl - Process MCUs]
// ============================================================================

static BLResult blJpegDecoderImplProcessMCUs(BLJpegDecoderImpl* impl) noexcept {
  if (impl->sofMarker == BL_JPEG_MARKER_SOF2) {
    uint32_t componentCount = impl->imageInfo.planeCount;

    // Dequantize & IDCT.
    for (uint32_t n = 0; n < componentCount; n++) {
      BLJpegDecoderComponent& comp = impl->comp[n];

      uint32_t w = (comp.pxW + 7) >> 3;
      uint32_t h = (comp.pxH + 7) >> 3;
      const BLJpegBlock<uint16_t>* qTable = &impl->qTable[comp.quantId];

      for (uint32_t j = 0; j < h; j++) {
        for (uint32_t i = 0; i < w; i++) {
          int16_t *data = comp.coeff + 64 * (i + j * comp.blW);
          blJpegOps.idct8(comp.data + comp.osW * j * 8 + i * 8, comp.osW, data, qTable->data);
        }
      }
    }
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLJpegDecoderImpl - ConvertToRGB]
// ============================================================================

struct BLJpegDecoderUpsample {
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
  uint8_t* (BL_CDECL* upsample)(uint8_t* out, uint8_t* in0, uint8_t* in1, uint32_t w, uint32_t hs) BL_NOEXCEPT;
};

static BLResult blJpegDecoderImplConvertToRgb(BLJpegDecoderImpl* impl, BLImageData& dst) noexcept {
  uint32_t w = uint32_t(impl->imageInfo.size.w);
  uint32_t h = uint32_t(impl->imageInfo.size.h);

  BL_ASSERT(uint32_t(dst.size.w) >= w);
  BL_ASSERT(uint32_t(dst.size.h) >= h);

  uint8_t* dstLine = static_cast<uint8_t*>(dst.pixelData);
  intptr_t dstStride = dst.stride;

  BLMemBufferTmp<1024 * 3 + 16> tmpMem;

  // Allocate a line buffer that's big enough for upsampling off the edges with
  // upsample factor of 4.
  uint32_t componentCount = impl->imageInfo.planeCount;
  uint32_t lineStride = blAlignUp(w + 3, 16);
  uint8_t* lineBuffer = static_cast<uint8_t*>(tmpMem.alloc(lineStride * componentCount));

  if (BL_UNLIKELY(!lineBuffer))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLJpegDecoderUpsample upsample[4];
  uint32_t y, k;

  uint8_t* pPlane[4];
  uint8_t* pBuffer[4];

  for (k = 0; k < componentCount; k++) {
    BLJpegDecoderComponent& comp = impl->comp[k];
    BLJpegDecoderUpsample* r = &upsample[k];

    pBuffer[k] = lineBuffer + k * lineStride;

    r->hs      = int(impl->mcu.sf.w) / comp.sfW;
    r->vs      = int(impl->mcu.sf.h) / comp.sfH;
    r->ystep   = r->vs >> 1;
    r->w_lores = (w + r->hs - 1) / r->hs;
    r->ypos    = 0;
    r->line[0] = comp.data;
    r->line[1] = comp.data;

    if      (r->hs == 1 && r->vs == 1) r->upsample = blJpegOps.upsample1x1;
    else if (r->hs == 1 && r->vs == 2) r->upsample = blJpegOps.upsample1x2;
    else if (r->hs == 2 && r->vs == 1) r->upsample = blJpegOps.upsample2x1;
    else if (r->hs == 2 && r->vs == 2) r->upsample = blJpegOps.upsample2x2;
    else                               r->upsample = blJpegOps.upsampleAny;
  }

  // Now go ahead and resample.
  for (y = 0; y < h; y++, dstLine += dstStride) {
    for (k = 0; k < componentCount; k++) {
      BLJpegDecoderComponent& comp = impl->comp[k];
      BLJpegDecoderUpsample* r = &upsample[k];

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
      blJpegOps.convYCbCr8ToRGB32(dstLine, pY, pPlane[1], pPlane[2], w);
    }
    else {
      for (uint32_t x = 0; x < w; x++) {
        blMemWriteU32a(dstLine + x * 4, 0xFF000000u + uint32_t(pY[x]) * 0x010101u);
      }
    }
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLJpegDecoderImpl - Read Internal]
// ============================================================================

static BLResult blJpegDecoderImplReadInfoInternal(BLJpegDecoderImpl* impl, const uint8_t* p, size_t size) noexcept {
  // JPEG file signature is 2 bytes (0xFF, 0xD8) followed by markers, SOF
  // (start of file) marker contains 1 byte signature and at least 8 bytes of
  // data describing basic information of the image.
  if (size < 2 + 8 + 1)
    return blTraceError(BL_ERROR_DATA_TRUNCATED);

  const uint8_t* start = p;
  const uint8_t* end = p + size;

  // Check JPEG signature (SOI marker).
  if (p[0] != 0xFF || p[1] != BL_JPEG_MARKER_SOI)
    return blTraceError(BL_ERROR_INVALID_SIGNATURE);

  p += 2;
  impl->statusFlags |= BL_JPEG_DECODER_DONE_SOI;

  // Process markers until SOF.
  for (;;) {
    impl->bufferIndex = (size_t)(p - start);

    if ((size_t)(end - p) < 2)
      return blTraceError(BL_ERROR_DATA_TRUNCATED);

    if (p[0] != 0xFF)
      return blTraceError(BL_ERROR_INVALID_DATA);

    uint32_t m = p[1];
    p += 2;

    // Some files have an extra padding (0xFF) after their blocks, ignore it.
    if (m == BL_JPEG_MARKER_INVALID) {
      while (p != end && (m = p[0]) == BL_JPEG_MARKER_INVALID)
        p++;

      if (p == end)
        break;
      p++;
    }

    size_t consumedBytes = 0;
    BL_PROPAGATE(blJpegDecoderImplProcessMarker(impl, m, p, (size_t)(end - p), consumedBytes));

    BL_ASSERT(consumedBytes < (size_t)(end - p));
    p += consumedBytes;

    // Terminate after SOF has been processed, the rest is handled by `decode()`.
    if (blJpegMarkerIsSOF(m))
      break;
  }

  impl->bufferIndex = (size_t)(p - start);
  return BL_SUCCESS;
}

static BLResult blJpegDecoderImplReadFrameInternal(BLJpegDecoderImpl* impl, BLImage* imageOut, const uint8_t* p, size_t size) noexcept {
  const uint8_t* start = p;
  const uint8_t* end = p + size;

  if (size < impl->bufferIndex)
    return blTraceError(BL_ERROR_DATA_TRUNCATED);

  p += impl->bufferIndex;

  // Process markers.
  //
  // We are already after SOF, which was processed by `blJpegDecoderImplReadInfoInternal`.
  for (;;) {
    impl->bufferIndex = (size_t)(p - start);
    if ((size_t)(end - p) < 2)
      return blTraceError(BL_ERROR_DATA_TRUNCATED);

    if (p[0] != 0xFF)
      return blTraceError(BL_ERROR_INVALID_DATA);

    uint32_t m = p[1];
    p += 2;

    // Some files have an extra padding (0xFF) after their blocks, ignore it.
    if (m == BL_JPEG_MARKER_INVALID) {
      while (p != end && (m = p[0]) == BL_JPEG_MARKER_INVALID)
        p++;

      if (p == end)
        break;
      p++;
    }

    // Process the marker.
    {
      size_t consumedBytes = 0;
      BL_PROPAGATE(blJpegDecoderImplProcessMarker(impl, m, p, (size_t)(end - p), consumedBytes));

      BL_ASSERT((size_t)(end - p) >= consumedBytes);
      p += consumedBytes;
    }

    // EOI - terminate.
    if (m == BL_JPEG_MARKER_EOI)
      break;

    // SOS - process the entropy coded data-stream that follows SOS.
    if (m == BL_JPEG_MARKER_SOS) {
      size_t consumedBytes = 0;
      BL_PROPAGATE(blJpegDecoderImplProcessStream(impl, p, (size_t)(end - p), consumedBytes));

      BL_ASSERT((size_t)(end - p) >= consumedBytes);
      p += consumedBytes;
    }
  }

  // Process MCUs.
  BL_PROPAGATE(blJpegDecoderImplProcessMCUs(impl));

  // Create the final image and convert YCbCr -> RGB.
  uint32_t w = uint32_t(impl->imageInfo.size.w);
  uint32_t h = uint32_t(impl->imageInfo.size.h);
  uint32_t format = BL_FORMAT_XRGB32;
  BLImageData imageData;

  BL_PROPAGATE(imageOut->create(int(w), int(h), format));
  BL_PROPAGATE(imageOut->makeMutable(&imageData));
  BL_PROPAGATE(blJpegDecoderImplConvertToRgb(impl, imageData));

  impl->frameIndex++;
  impl->bufferIndex = (size_t)(p - start);
  return BL_SUCCESS;
}

// ============================================================================
// [BLJpegDecoder - Interface]
// ============================================================================

static BLResult BL_CDECL blJpegDecoderImplDestroy(BLJpegDecoderImpl* impl) noexcept {
  impl->allocator.reset();
  return blRuntimeFreeImpl(impl, sizeof(BLJpegDecoderImpl), impl->memPoolData);
}

static BLResult BL_CDECL blJpegDecoderImplRestart(BLJpegDecoderImpl* impl) noexcept {
  impl->lastResult = BL_SUCCESS;
  impl->frameIndex = 0;
  impl->bufferIndex = 0;

  impl->allocator.reset();
  impl->imageInfo.reset();
  impl->statusFlags = 0;
  impl->restartInterval = 0;
  impl->sofMarker = 0;
  impl->colorspace = 0;
  impl->delayedHeight = 0;
  impl->jfifMajor = 0;
  impl->jfifMinor = 0;
  impl->dcTableMask = 0;
  impl->acTableMask = 0;
  impl->qTableMask = 0;
  impl->mcu.reset();
  impl->sos.reset();
  impl->thumb.reset();
  memset(impl->comp, 0, sizeof(impl->comp));

  return BL_SUCCESS;
}

static BLResult BL_CDECL blJpegDecoderImplReadInfo(BLJpegDecoderImpl* impl, BLImageInfo* infoOut, const uint8_t* p, size_t size) noexcept {
  BLResult result = impl->lastResult;
  if (impl->bufferIndex == 0 && result == BL_SUCCESS) {
    result = blJpegDecoderImplReadInfoInternal(impl, p, size);
    if (result != BL_SUCCESS)
      impl->lastResult = result;
  }

  if (infoOut)
    memcpy(infoOut, &impl->imageInfo, sizeof(BLImageInfo));

  return result;
}

static BLResult BL_CDECL blJpegDecoderImplReadFrame(BLJpegDecoderImpl* impl, BLImage* imageOut, const uint8_t* p, size_t size) noexcept {
  BL_PROPAGATE(blJpegDecoderImplReadInfo(impl, nullptr, p, size));

  if (impl->frameIndex)
    return blTraceError(BL_ERROR_NO_MORE_DATA);

  BLResult result = blJpegDecoderImplReadFrameInternal(impl, imageOut, p, size);
  if (result != BL_SUCCESS)
    impl->lastResult = result;
  return result;
}

static BLJpegDecoderImpl* blJpegDecoderImplNew() noexcept {
  uint16_t memPoolData;
  BLJpegDecoderImpl* impl = blRuntimeAllocImplT<BLJpegDecoderImpl>(sizeof(BLJpegDecoderImpl), &memPoolData);

  if (BL_UNLIKELY(!impl))
    return nullptr;

  blImplInit(impl, BL_IMPL_TYPE_IMAGE_DECODER, BL_IMPL_TRAIT_VIRT, memPoolData);
  impl->virt = &blJpegDecoderVirt;
  impl->codec.impl = &blJpegCodecImpl;
  impl->handle = nullptr;
  new (&impl->allocator) BLScopedAllocator();
  blJpegDecoderImplRestart(impl);

  return impl;
}

// ============================================================================
// [BLJpegCodec - Interface]
// ============================================================================

static BLResult BL_CDECL blJpegCodecImplDestroy(BLJpegCodecImpl* impl) noexcept { return BL_SUCCESS; }

static uint32_t BL_CDECL blJpegCodecImplInspectData(BLJpegCodecImpl* impl, const uint8_t* data, size_t size) noexcept {
  // JPEG minimum size and signature (SOI).
  if (size < 2 || data[0] != 0xFF || data[1] != BL_JPEG_MARKER_SOI)
    return 0;

  // JPEG signature has to be followed by a marker that starts with 0xFF.
  if (size > 2 && data[2] != 0xFF)
    return 0;

  return 100;
}

static BLResult BL_CDECL blJpegCodecImplCreateDecoder(const BLImageCodecImpl* impl, BLImageDecoderCore* dst) noexcept {
  BLImageDecoderCore decoder { blJpegDecoderImplNew() };
  if (BL_UNLIKELY(!decoder.impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  return blImageDecoderAssignMove(dst, &decoder);
}

static BLResult BL_CDECL blJpegCodecImplCreateEncoder(const BLImageCodecImpl* impl, BLImageEncoderCore* dst) noexcept {
  // TODO: [JPEG] Encoder
  return blTraceError(BL_ERROR_IMAGE_ENCODER_NOT_PROVIDED);
  /*
  BLImageEncoderCore encoder { blJpegEncoderImplNew() };
  if (BL_UNLIKELY(!encoder.impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  return blImageEncoderAssignMove(dst, &encoder);
  */
}

// ============================================================================
// [BLJpegCodec - Runtime Init]
// ============================================================================

BLImageCodecImpl* blJpegCodecRtInit(BLRuntimeContext* rt) noexcept {
  // Initialize JPEG ops.
  blJpegOps.idct8             = blJpegIDCT8;
  blJpegOps.convYCbCr8ToRGB32 = blJpegRGB32FromYCbCr8;

  #ifdef BL_BUILD_OPT_SSE2
  blJpegOps.idct8             = blJpegIDCT8_SSE2;
  blJpegOps.convYCbCr8ToRGB32 = blJpegRGB32FromYCbCr8_SSE2;
  #endif

  blJpegOps.upsample1x1       = blJpegUpsample1x1;
  blJpegOps.upsample1x2       = blJpegUpsample1x2;
  blJpegOps.upsample2x1       = blJpegUpsample2x1;
  blJpegOps.upsample2x2       = blJpegUpsample2x2;
  blJpegOps.upsampleAny       = blJpegUpsampleAny;

  // Initialize JPEG decoder virtual functions.
  blAssignFunc(&blJpegDecoderVirt.destroy, blJpegDecoderImplDestroy);
  blAssignFunc(&blJpegDecoderVirt.restart, blJpegDecoderImplRestart);
  blAssignFunc(&blJpegDecoderVirt.readInfo, blJpegDecoderImplReadInfo);
  blAssignFunc(&blJpegDecoderVirt.readFrame, blJpegDecoderImplReadFrame);

  // Initialize JPEG encoder virtual functions.
  // TODO: [JPEG] Encoder
  // blAssignFunc(&blJpegEncoderVirt.destroy, blJpegEncoderDestroy);
  // blAssignFunc(&blJpegEncoderVirt.restart, blJpegEncoderRestart);
  // blAssignFunc(&blJpegEncoderVirt.writeFrame, blJpegEncoderWriteFrame);

  // Initialize JPEG codec virtual functions.
  blAssignFunc(&blJpegCodecVirt.destroy, blJpegCodecImplDestroy);
  blAssignFunc(&blJpegCodecVirt.inspectData, blJpegCodecImplInspectData);
  blAssignFunc(&blJpegCodecVirt.createDecoder, blJpegCodecImplCreateDecoder);
  blAssignFunc(&blJpegCodecVirt.createEncoder, blJpegCodecImplCreateEncoder);

  // Initialize JPEG codec built-in instance.
  BLJpegCodecImpl* codecI = &blJpegCodecImpl;

  codecI->virt = &blJpegCodecVirt;
  codecI->implType = uint8_t(BL_IMPL_TYPE_IMAGE_CODEC);
  codecI->implTraits = uint8_t(BL_IMPL_TRAIT_VIRT);

  codecI->features = BL_IMAGE_CODEC_FEATURE_READ  |
                     BL_IMAGE_CODEC_FEATURE_WRITE |
                     BL_IMAGE_CODEC_FEATURE_LOSSY ;

  codecI->name = "JPEG";
  codecI->vendor = "Blend2D";
  codecI->mimeType = "image/jpeg";
  codecI->extensions = "jpg|jpeg|jif|jfi|jfif";

  return codecI;
}
