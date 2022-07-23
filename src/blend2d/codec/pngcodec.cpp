// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// The PNG codec is based on stb_image <https://github.com/nothings/stb>
// released into PUBLIC DOMAIN. Blend2D's PNG codec can be distributed
// under Blend2D's ZLIB license or under STB's PUBLIC DOMAIN as well.

#include "../api-build_p.h"
#include "../array_p.h"
#include "../format.h"
#include "../imagecodec.h"
#include "../object_p.h"
#include "../runtime_p.h"
#include "../var_p.h"
#include "../codec/pngcodec_p.h"
#include "../codec/pngops_p.h"
#include "../compression/checksum_p.h"
#include "../compression/deflatedecoder_p.h"
#include "../compression/deflateencoder_p.h"
#include "../pixelops/scalar_p.h"
#include "../support/intops_p.h"
#include "../support/memops_p.h"
#include "../support/scopedbuffer_p.h"

// ============================================================================
// [Global Variables]
// ============================================================================

static BLImageCodecCore blPngCodecObject;
static BLObjectEthernalVirtualImpl<BLPngCodecImpl, BLImageCodecVirt> blPngCodec;
static BLImageDecoderVirt blPngDecoderVirt;
static BLImageEncoderVirt blPngEncoderVirt;

// ============================================================================
// [BLPngCodec - Constants]
// ============================================================================

// PNG file signature (8 bytes).
static const uint8_t blPngSignature[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

// Allowed bits-per-sample per "ColorType"
static const uint8_t blPngColorTypeBitDepthsTable[7] = { 0x1F, 0, 0x18, 0x0F, 0x18, 0, 0x18 };

// Count of samples per "ColorType".
static const uint8_t blPngColorTypeToSampleCountTable[7] = { 0x01, 0, 0x03, 0x01, 0x02, 0, 0x04 };

// ============================================================================
// [BLPngCodec - Utilities]
// ============================================================================

static BL_INLINE bool blPngCheckColorTypeAndBitDepth(uint32_t colorType, uint32_t depth) noexcept {
  // TODO: [PNG] 16-BPC.
  if (depth == 16)
    return false;

  return colorType < BL_ARRAY_SIZE(blPngColorTypeBitDepthsTable) &&
         (blPngColorTypeBitDepthsTable[colorType] & depth) != 0 &&
         BLIntOps::isPowerOf2(depth);
}

static BL_INLINE void blPngCreateGrayscalePalette(BLRgba32* pal, uint32_t depth) noexcept {
  static const uint32_t scaleTable[9] = { 0, 0xFF, 0x55, 0, 0x11, 0, 0, 0, 0x01 };
  BL_ASSERT(depth < BL_ARRAY_SIZE(scaleTable));

  uint32_t scale = uint32_t(scaleTable[depth]) * 0x00010101;
  uint32_t count = 1u << depth;
  uint32_t value = 0xFF000000;

  for (uint32_t i = 0; i < count; i++, value += scale)
    pal[i].value = value;
}

// ============================================================================
// [BLPngCodec - Interlace / Deinterlace]
// ============================================================================

// A single PNG interlace/deinterlace step related to the full image size.
struct BLPngInterlaceStep {
  uint32_t used;
  uint32_t width;
  uint32_t height;
  uint32_t bpl;

  uint32_t offset;
  uint32_t size;
};

// PNG deinterlace table data.
struct BLPngInterlaceTable {
  uint8_t xOff;
  uint8_t yOff;
  uint8_t xPow;
  uint8_t yPow;
};

// No interlacing.
static const BLPngInterlaceTable blPngInterlaceTableNone[1] = {
  { 0, 0, 0, 0 }
};

// Passes start from zero to stay compatible with interlacing tables, however,
// this representation is not visually compatible with PNG spec, where passes
// are indexed from `1` (that's the only difference).
//
//        8x8 block
//   +-----------------+
//   | 0 5 3 5 1 5 3 5 |
//   | 6 6 6 6 6 6 6 6 |
//   | 4 5 4 5 4 5 4 5 |
//   | 6 6 6 6 6 6 6 6 |
//   | 2 5 3 5 2 5 3 5 |
//   | 6 6 6 6 6 6 6 6 |
//   | 4 5 4 5 4 5 4 5 |
//   | 6 6 6 6 6 6 6 6 |
//   +-----------------+
static const BLPngInterlaceTable blPngInterlaceTableAdam7[7] = {
  { 0, 0, 3, 3 },
  { 4, 0, 3, 3 },
  { 0, 4, 2, 3 },
  { 2, 0, 2, 2 },
  { 0, 2, 1, 2 },
  { 1, 0, 1, 1 },
  { 0, 1, 0, 1 }
};

static uint32_t blPngCalculateInterlaceSteps(
  BLPngInterlaceStep* dst, const BLPngInterlaceTable* table, uint32_t stepCount,
  uint32_t sampleDepth, uint32_t sampleCount,
  uint32_t w, uint32_t h) noexcept {

  // Byte-offset of each chunk.
  uint32_t offset = 0;

  for (uint32_t i = 0; i < stepCount; i++, dst++) {
    const BLPngInterlaceTable& tab = table[i];

    uint32_t sx = 1 << tab.xPow;
    uint32_t sy = 1 << tab.yPow;
    uint32_t sw = (w + sx - tab.xOff - 1) >> tab.xPow;
    uint32_t sh = (h + sy - tab.yOff - 1) >> tab.yPow;

    // If the reference image contains fewer than five columns or fewer than
    // five rows, some passes will be empty, decoders must handle this case.
    uint32_t used = sw != 0 && sh != 0;

    // NOTE: No need to check for overflow at this point as we have already
    // calculated the total BPL of the whole image, and since interlacing is
    // splitting it into multiple images, it can't overflow the base size.
    uint32_t bpl = ((sw * sampleDepth + 7) / 8) * sampleCount + 1;
    uint32_t size = used ? bpl * sh : uint32_t(0);

    dst->used = used;
    dst->width = sw;
    dst->height = sh;
    dst->bpl = bpl;

    dst->offset = offset;
    dst->size = size;

    // Here we should be safe...
    BLOverflowFlag of = 0;
    offset = BLIntOps::addOverflow(offset, size, &of);

    if (BL_UNLIKELY(of))
      return 0;
  }

  return offset;
}

static BL_INLINE uint8_t combineByte1bpp(uint32_t b0, uint32_t b1, uint32_t b2, uint32_t b3, uint32_t b4, uint32_t b5, uint32_t b6, uint32_t b7) noexcept {
  return uint8_t(((b0) & 0x80) | ((b1) & 0x40) | ((b2) & 0x20) | ((b3) & 0x10) | ((b4) & 0x08) | ((b5) & 0x04) | ((b6) & 0x02) | ((b7) & 0x01));
}

static BL_INLINE uint8_t combineByte2bpp(uint32_t b0, uint32_t b1, uint32_t b2, uint32_t b3) noexcept {
  return uint8_t(((b0) & 0xC0) + ((b1) & 0x30) + ((b2) & 0x0C) + ((b3) & 0x03));
}

static BL_INLINE uint8_t combineByte4bpp(uint32_t b0, uint32_t b1) noexcept {
  return uint8_t(((b0) & 0xF0) + ((b1) & 0x0F));
}

// Deinterlace a PNG image that has depth less than 8 bits. This is a bit
// tricky as one byte describes two or more pixels that can be fetched from
// 1st to 6th progressive images. Basically each bit depth is implemented
// separatery as generic case would be very inefficient. Also, the destination
// image is handled pixel-by-pixel fetching data from all possible scanlines
// as necessary - this is a bit different compared with `blPngDeinterlaceBytes()`.
template<uint32_t N>
static void blPngDeinterlaceBits(
  uint8_t* dstLine, intptr_t dstStride, const BLPixelConverter& pc,
  uint8_t* tmpLine, intptr_t tmpStride, const uint8_t* data, const BLPngInterlaceStep* steps,
  uint32_t w, uint32_t h) noexcept {

  const uint8_t* d0 = data + steps[0].offset;
  const uint8_t* d1 = data + steps[1].offset;
  const uint8_t* d2 = data + steps[2].offset;
  const uint8_t* d3 = data + steps[3].offset;
  const uint8_t* d4 = data + steps[4].offset;
  const uint8_t* d5 = data + steps[5].offset;

  BL_ASSERT(h != 0);

  // We store only to odd scanlines.
  uint32_t y = (h + 1) / 2;
  uint32_t n = 0;

  for (;;) {
    uint8_t* tmpData = tmpLine + n * tmpStride;
    uint32_t x = w;

    // ------------------------------------------------------------------------
    // [1-BPP]
    // ------------------------------------------------------------------------

    if (N == 1) {
      switch (n) {
        // [a b b b a b b b]
        // [0 5 3 5 1 5 3 5]
        case 0: {
          uint32_t a = 0, b;

          d0 += 1;
          d1 += (x >= 5);
          d3 += (x >= 3);
          d5 += (x >= 2);

          while (x >= 32) {
            // Fetched every second iteration.
            if (!(a & 0x80000000))
              a = (uint32_t(*d0++)) + (uint32_t(*d1++) << 8) + 0x08000000u;

            b = (uint32_t(*d3++)      ) +
                (uint32_t(d5[0]) <<  8) +
                (uint32_t(d5[1]) << 16) ;
            d5 += 2;

            tmpData[0] = combineByte1bpp(a     , b >>  9, b >> 2, b >> 10, a >> 12, b >> 11, b >> 5, b >> 12);
            tmpData[1] = combineByte1bpp(a << 1, b >>  5, b     , b >>  6, a >> 11, b >>  7, b >> 3, b >>  8);
            tmpData[2] = combineByte1bpp(a << 2, b >> 17, b << 2, b >> 18, a >> 10, b >> 19, b >> 1, b >> 20);
            tmpData[3] = combineByte1bpp(a << 3, b >> 13, b << 4, b >> 14, a >>  9, b >> 15, b << 1, b >> 16);
            tmpData += 4;

            a <<= 4;
            x -= 32;
          }

          if (!x)
            break;

          if (!(a & 0x80000000)) {
            a = uint32_t(*d0++);
            if (x >= 5)
              a += uint32_t(*d1++) << 8;
          }

          b = 0;
          if (x >=  3) b  = uint32_t(*d3++);
          if (x >=  2) b += uint32_t(*d5++) <<  8;
          if (x >= 18) b += uint32_t(*d5++) << 16;

          tmpData[0] = combineByte1bpp(a     , b >>  9, b >> 2, b >> 10, a >> 12, b >> 11, b >> 5, b >> 12); if (x <=  8) break;
          tmpData[1] = combineByte1bpp(a << 1, b >>  5, b     , b >>  6, a >> 11, b >>  7, b >> 3, b >>  8); if (x <= 16) break;
          tmpData[2] = combineByte1bpp(a << 2, b >> 17, b << 2, b >> 18, a >> 10, b >> 19, b >> 1, b >> 20); if (x <= 24) break;
          tmpData[3] = combineByte1bpp(a << 3, b >> 13, b << 4, b >> 14, a >>  9, b >> 15, b << 1, b >> 16);

          break;
        }

        // [a b a b a b a b]
        // [2 5 3 5 2 5 3 5]
        case 2: {
          uint32_t a, b;

          d2 += 1;
          d3 += (x >= 3);
          d5 += (x >= 2);

          while (x >= 32) {
            a = uint32_t(*d2++) + (uint32_t(*d3++) << 8);
            b = uint32_t(d5[0]) + (uint32_t(d5[1]) << 8);
            d5 += 2;

            tmpData[0] = combineByte1bpp(a     , b >>  1, a >> 10, b >>  2, a >>  3, b >>  3, a >> 13, b >>  4);
            tmpData[1] = combineByte1bpp(a << 2, b <<  3, a >>  8, b <<  2, a >>  1, b <<  1, a >> 11, b      );
            tmpData[2] = combineByte1bpp(a << 4, b >>  9, a >>  6, b >> 10, a <<  1, b >> 11, a >>  9, b >> 12);
            tmpData[3] = combineByte1bpp(a << 6, b >>  5, a >>  4, b >>  6, a <<  3, b >>  7, a >>  7, b >>  8);
            tmpData += 4;

            x -= 32;
          }

          if (!x)
            break;

          a = uint32_t(*d2++);
          b = 0;

          if (x >=  3) a += uint32_t(*d3++) << 8;
          if (x >=  2) b  = uint32_t(*d5++);
          if (x >= 18) b += uint32_t(*d5++) << 8;

          tmpData[0] = combineByte1bpp(a     , b >>  1, a >> 10, b >>  2, a >>  3, b >>  3, a >> 13, b >>  4); if (x <=  8) break;
          tmpData[1] = combineByte1bpp(a << 2, b <<  3, a >>  8, b <<  2, a >>  1, b <<  1, a >> 11, b      ); if (x <= 16) break;
          tmpData[2] = combineByte1bpp(a << 4, b >>  9, a >>  6, b >> 10, a <<  1, b >> 11, a >>  9, b >> 12); if (x <= 24) break;
          tmpData[3] = combineByte1bpp(a << 6, b >>  5, a >>  4, b >>  6, a <<  3, b >>  7, a >>  7, b >>  8);

          break;
        }

        // [a b a b a b a b]
        // [4 5 4 5 4 5 4 5]
        case 1:
        case 3: {
          uint32_t a, b;

          d4 += 1;
          d5 += (x >= 2);

          while (x >= 16) {
            a = uint32_t(*d4++);
            b = uint32_t(*d5++);

            tmpData[0] = combineByte1bpp(a     , b >> 1, a >> 1, b >> 2, a >> 2, b >> 3, a >> 3, b >> 4);
            tmpData[1] = combineByte1bpp(a << 4, b << 3, a << 3, b << 2, a << 2, b << 1, a << 1, b     );
            tmpData += 2;

            x -= 16;
          }

          if (!x)
            break;

          a = uint32_t(*d4++);
          b = 0;

          if (x >= 2)
            b = uint32_t(*d5++);

          tmpData[0] = combineByte1bpp(a     , b >> 1, a >> 1, b >> 2, a >> 2, b >> 3, a >> 3, b >> 4); if (x <= 8) break;
          tmpData[1] = combineByte1bpp(a << 4, b << 3, a << 3, b << 2, a << 2, b << 1, a << 1, b     );

          break;
        }
      }
    }

    // ------------------------------------------------------------------------
    // [2-BPP]
    // ------------------------------------------------------------------------

    else if (N == 2) {
      switch (n) {
        // [aa bb bb bb][aa bb bb bb]
        // [00 55 33 55][11 55 33 55]
        case 0: {
          uint32_t a = 0, b;

          d0 += 1;
          d1 += (x >= 5);
          d3 += (x >= 3);
          d5 += (x >= 2);

          while (x >= 16) {
            // Fetched every second iteration.
            if (!(a & 0x80000000))
              a = (uint32_t(*d0++)     ) +
                  (uint32_t(*d1++) << 8) + 0x08000000;

            b = (uint32_t(*d3++)      ) +
                (uint32_t(d5[0]) <<  8) +
                (uint32_t(d5[1]) << 16) ;
            d5 += 2;

            tmpData[0] = combineByte2bpp(a     , b >> 10, b >> 4, b >> 12);
            tmpData[1] = combineByte2bpp(a >> 8, b >>  6, b >> 2, b >>  8);
            tmpData[2] = combineByte2bpp(a << 2, b >> 18, b     , b >> 20);
            tmpData[3] = combineByte2bpp(a >> 6, b >> 14, b << 2, b >> 16);
            tmpData += 4;

            a <<= 4;
            x -= 16;
          }

          if (!x)
            break;

          if (!(a & 0x80000000)) {
            a = (uint32_t(*d0++));
            if (x >= 5)
              a += (uint32_t(*d1++) << 8);
          }

          b = 0;
          if (x >=  3) b  = (uint32_t(*d3++)      );
          if (x >=  2) b += (uint32_t(*d5++) <<  8);
          if (x >= 10) b += (uint32_t(*d5++) << 16);

          tmpData[0] = combineByte2bpp(a     , b >> 10, b >> 4, b >> 12); if (x <=  4) break;
          tmpData[1] = combineByte2bpp(a >> 8, b >>  6, b >> 2, b >>  8); if (x <=  8) break;
          tmpData[2] = combineByte2bpp(a << 2, b >> 18, b     , b >> 20); if (x <= 12) break;
          tmpData[3] = combineByte2bpp(a >> 6, b >> 14, b << 2, b >> 16);

          break;
        }

        // [aa bb aa bb][aa bb aa bb]
        // [22 55 33 55][22 55 33 55]
        case 2: {
          uint32_t a, b;

          d2 += 1;
          d3 += (x >= 3);
          d5 += (x >= 2);

          while (x >= 16) {
            a = uint32_t(*d2++) + (uint32_t(*d3++) << 8);
            b = uint32_t(*d5++);

            tmpData[0] = combineByte2bpp(a     , b >>  2, a >> 12, b >>  4);
            tmpData[1] = combineByte2bpp(a << 2, b <<  2, a >> 10, b      );

            b = uint32_t(*d5++);

            tmpData[2] = combineByte2bpp(a << 4, b >>  2, a >>  8, b >>  4);
            tmpData[3] = combineByte2bpp(a << 6, b <<  2, a >>  6, b      );
            tmpData += 4;

            x -= 16;
          }

          if (!x)
            break;

          a = uint32_t(*d2++);
          b = 0;

          if (x >=  3) a  = (uint32_t(*d3++) << 8);
          if (x >=  2) b  = (uint32_t(*d5++)     );
          if (x >= 10) b += (uint32_t(*d5++) << 8);

          tmpData[0] = combineByte2bpp(a     , b >>  2, a >> 12, b >>  4); if (x <=  4) break;
          tmpData[1] = combineByte2bpp(a << 2, b <<  2, a >> 10, b      ); if (x <=  8) break;
          tmpData[2] = combineByte2bpp(a << 4, b >> 10, a >>  8, b >> 12); if (x <= 12) break;
          tmpData[3] = combineByte2bpp(a << 6, b >>  6, a >>  6, b >>  8);

          break;
        }

        // [aa bb aa bb][aa bb aa bb]
        // [44 55 44 55][44 55 44 55]
        case 1:
        case 3: {
          uint32_t a, b;

          d4 += 1;
          d5 += (x >= 2);

          while (x >= 8) {
            a = uint32_t(*d4++);
            b = uint32_t(*d5++);

            tmpData[0] = combineByte2bpp(a     , b >> 2, a >> 2, b >> 4);
            tmpData[1] = combineByte2bpp(a << 4, b << 2, a << 2, b     );
            tmpData += 2;

            x -= 8;
          }

          if (!x)
            break;

          a = uint32_t(*d4++);
          b = 0;

          if (x >= 2)
            b = uint32_t(*d5++);

          tmpData[0] = combineByte2bpp(a     , b >> 10, b >> 4, b >> 12); if (x <=  4) break;
          tmpData[1] = combineByte2bpp(a >> 8, b >>  6, b >> 2, b >>  8);

          break;
        }
      }
    }

    // ------------------------------------------------------------------------
    // [4-BPP]
    // ------------------------------------------------------------------------

    else if (N == 4) {
      switch (n) {
        // [aaaa bbbb][bbbb bbbb][aaaa bbbb][bbbb bbbb]
        // [0000 5555][3333 5555][1111 5555][3333 5555]
        case 0: {
          uint32_t a = 0, b;

          d0 += 1;
          d1 += (x >= 5);
          d3 += (x >= 3);
          d5 += (x >= 2);

          while (x >= 8) {
            // Fetched every second iteration.
            if (!(a & 0x80000000))
              a = (uint32_t(*d0++)      ) +
                  (uint32_t(*d1++) <<  8) + 0x08000000;

            b = (uint32_t(*d3++)      ) +
                (uint32_t(d5[0]) <<  8) +
                (uint32_t(d5[1]) << 16) ;
            d5 += 2;

            tmpData[0] = combineByte4bpp(a     , b >> 12);
            tmpData[1] = combineByte4bpp(b     , b >>  8);
            tmpData[2] = combineByte4bpp(a >> 8, b >> 20);
            tmpData[3] = combineByte4bpp(b << 4, b >> 16);
            tmpData += 4;

            a <<= 4;
            x -= 8;
          }

          if (!x)
            break;

          if (!(a & 0x80000000)) {
            a = (uint32_t(*d0++));
            if (x >= 5)
              a += (uint32_t(*d1++) << 8);
          }

          b = 0;
          if (x >= 3) b  = (uint32_t(*d3++)      );
          if (x >= 2) b += (uint32_t(*d5++) <<  8);
          if (x >= 6) b += (uint32_t(*d5++) << 16);

          tmpData[0] = combineByte4bpp(a     , b >> 12); if (x <= 2) break;
          tmpData[1] = combineByte4bpp(b     , b >>  8); if (x <= 4) break;
          tmpData[2] = combineByte4bpp(a >> 8, b >> 20); if (x <= 6) break;
          tmpData[3] = combineByte4bpp(b << 4, b >> 16);

          break;
        }

        // [aaaa bbbb][aaaa bbbb][aaaa bbbb][aaaa bbbb]
        // [2222 5555][3333 5555][2222 5555][3333 5555]
        case 2: {
          uint32_t a, b;

          d2 += 1;
          d3 += (x >= 3);
          d5 += (x >= 2);

          while (x >= 8) {
            a = uint32_t(*d2++) + (uint32_t(*d3++) << 8);
            b = uint32_t(*d5++);
            tmpData[0] = combineByte4bpp(a     , b >>  4);
            tmpData[1] = combineByte4bpp(a >> 8, b      );

            b = uint32_t(*d5++);
            tmpData[2] = combineByte4bpp(a << 4, b >>  4);
            tmpData[3] = combineByte4bpp(a >> 4, b      );
            tmpData += 4;

            x -= 8;
          }

          if (!x)
            break;

          a = uint32_t(*d2++);
          b = 0;

          if (x >= 3) a += (uint32_t(*d3++) << 8);
          if (x >= 2) b  = (uint32_t(*d5++)     );
          tmpData[0] = combineByte4bpp(a     , b >> 4); if (x <= 2) break;
          tmpData[1] = combineByte4bpp(a >> 8, b     ); if (x <= 4) break;

          if (x >= 5) b = uint32_t(*d5++);
          tmpData[2] = combineByte4bpp(a << 4, b >> 4); if (x <= 6) break;
          tmpData[3] = combineByte4bpp(a >> 4, b     );

          break;
        }

        // [aaaa bbbb aaaa bbbb][aaaa bbbb aaaa bbbb]
        // [4444 5555 4444 5555][4444 5555 4444 5555]
        case 1:
        case 3: {
          uint32_t a, b;

          d4 += 1;
          d5 += (x >= 2);

          while (x >= 4) {
            a = uint32_t(*d4++);
            b = uint32_t(*d5++);

            tmpData[0] = combineByte4bpp(a     , b >> 4);
            tmpData[1] = combineByte4bpp(a << 4, b     );
            tmpData += 2;

            x -= 4;
          }

          if (!x)
            break;

          a = uint32_t(*d4++);
          b = 0;

          if (x >= 2)
            b = uint32_t(*d5++);

          tmpData[0] = combineByte4bpp(a     , b >> 4); if (x <= 2) break;
          tmpData[1] = combineByte4bpp(a << 4, b     );

          break;
        }
      }
    }

    // Don't change to `||`, both have to be executed!
    if (uint32_t(--y == 0) | uint32_t(++n == 4)) {
      pc.convertRect(dstLine, dstStride * 2, tmpLine, tmpStride, w, n);
      dstLine += dstStride * 8;

      if (y == 0)
        break;
      n = 0;
    }
  }
}

// Copy `N` bytes from unaligned `src` into aligned `dst`. Allows us to handle
// some special cases if the CPU supports unaligned reads/writes from/to memory.
template<uint32_t N>
static BL_INLINE const uint8_t* blPngCopyBytes(uint8_t* dst, const uint8_t* src) noexcept {
  if (N == 2) {
    BLMemOps::writeU16a(dst, BLMemOps::readU16u(src));
  }
  else if (N == 4) {
    BLMemOps::writeU32a(dst, BLMemOps::readU32u(src));
  }
  else if (N == 8) {
    BLMemOps::writeU32a(dst + 0, BLMemOps::readU32u(src + 0));
    BLMemOps::writeU32a(dst + 4, BLMemOps::readU32u(src + 4));
  }
  else {
    if (N >= 1) dst[0] = src[0];
    if (N >= 2) dst[1] = src[1];
    if (N >= 3) dst[2] = src[2];
    if (N >= 4) dst[3] = src[3];
    if (N >= 5) dst[4] = src[4];
    if (N >= 6) dst[5] = src[5];
    if (N >= 7) dst[6] = src[6];
    if (N >= 8) dst[7] = src[7];
  }
  return src + N;
}

template<uint32_t N>
static void blPngDeinterlaceBytes(
  uint8_t* dstLine, intptr_t dstStride, const BLPixelConverter& pc,
  uint8_t* tmpLine, intptr_t tmpStride, const uint8_t* data, const BLPngInterlaceStep* steps,
  uint32_t w, uint32_t h) noexcept {

  const uint8_t* d0 = data + steps[0].offset;
  const uint8_t* d1 = data + steps[1].offset;
  const uint8_t* d2 = data + steps[2].offset;
  const uint8_t* d3 = data + steps[3].offset;
  const uint8_t* d4 = data + steps[4].offset;
  const uint8_t* d5 = data + steps[5].offset;

  BL_ASSERT(h != 0);

  // We store only to odd scanlines.
  uint32_t y = (h + 1) / 2;
  uint32_t n = 0;
  uint32_t xMax = w * N;

  for (;;) {
    uint8_t* tmpData = tmpLine + n * tmpStride;
    uint32_t x;

    switch (n) {
      // [05351535]
      case 0: {
        d0 += 1;
        d1 += (w >= 5);
        d3 += (w >= 3);
        d5 += (w >= 2);

        for (x = 0 * N; x < xMax; x += 8 * N) d0 = blPngCopyBytes<N>(tmpData + x, d0);
        for (x = 4 * N; x < xMax; x += 8 * N) d1 = blPngCopyBytes<N>(tmpData + x, d1);
        for (x = 2 * N; x < xMax; x += 4 * N) d3 = blPngCopyBytes<N>(tmpData + x, d3);
        for (x = 1 * N; x < xMax; x += 2 * N) d5 = blPngCopyBytes<N>(tmpData + x, d5);

        break;
      }

      // [25352535]
      case 2: {
        d2 += 1;
        d3 += (w >= 3);
        d5 += (w >= 2);

        for (x = 0 * N; x < xMax; x += 4 * N) d2 = blPngCopyBytes<N>(tmpData + x, d2);
        for (x = 2 * N; x < xMax; x += 4 * N) d3 = blPngCopyBytes<N>(tmpData + x, d3);
        for (x = 1 * N; x < xMax; x += 2 * N) d5 = blPngCopyBytes<N>(tmpData + x, d5);

        break;
      }

      // [45454545]
      case 1:
      case 3: {
        d4 += 1;
        d5 += (w >= 2);

        for (x = 0 * N; x < xMax; x += 2 * N) d4 = blPngCopyBytes<N>(tmpData + x, d4);
        for (x = 1 * N; x < xMax; x += 2 * N) d5 = blPngCopyBytes<N>(tmpData + x, d5);

        break;
      }
    }

    // Don't change to `||`, both have to be executed!
    if (uint32_t(--y == 0) | uint32_t(++n == 4)) {
      pc.convertRect(dstLine, dstStride * 2, tmpLine, tmpStride, w, n);
      dstLine += dstStride * 8;

      if (y == 0)
        break;
      n = 0;
    }
  }
}

// ============================================================================
// [BLPngCodec - DecoderImpl]
// ============================================================================

struct BLPngDecoderReadData {
  const uint8_t* p;
  size_t index;
};

static bool BL_CDECL blPngDecoderImplReadFunc(BLPngDecoderReadData* rd, const uint8_t** pData, const uint8_t** end) noexcept {
  const uint8_t* p = rd->p;
  size_t index = rd->index;

  // Ignore any repeated calls if we failed once. We have to do it here as
  // the Deflate context doesn't do that and can repeatedly call `readData`.
  if (p == nullptr)
    return false;

  uint32_t chunkTag;
  uint32_t chunkSize;

  // The spec doesn't forbid zero-size IDAT chunks so the implementation must
  // handle them as well.
  do {
    chunkTag = BLMemOps::readU32uBE(p + index + 4);
    chunkSize = BLMemOps::readU32uBE(p + index + 0);

    // IDAT's have to be consecutive, if terminated it means that there is no
    // more data to be consumed by deflate.
    if (chunkTag != BL_MAKE_TAG('I', 'D', 'A', 'T')) {
      rd->p = nullptr;
      return false;
    }

    index += 12 + chunkSize;
  } while (chunkSize == 0);

  p += index - chunkSize - 4;
  rd->index = index;

  *pData = p;
  *end = p + chunkSize;
  return true;
}

static BLResult BL_CDECL blPngDecoderImplRestart(BLImageDecoderImpl* impl) noexcept {
  BLPngDecoderImpl* decoderI = static_cast<BLPngDecoderImpl*>(impl);

  decoderI->lastResult = BL_SUCCESS;
  decoderI->frameIndex = 0;
  decoderI->bufferIndex = 0;

  decoderI->imageInfo.reset();
  decoderI->statusFlags = 0;
  decoderI->colorType = 0;
  decoderI->sampleDepth = 0;
  decoderI->sampleCount = 0;
  decoderI->cgbi = 0;

  return BL_SUCCESS;
}

static BLResult blPngDecoderImplReadInfoInternal(BLPngDecoderImpl* decoderI, const uint8_t* p, size_t size) noexcept {
  // Signature (8 bytes), IHDR tag (8 bytes), IHDR data (13 bytes), and IHDR CRC (4 bytes).
  const size_t kMinSize = 8 + 8 + 13 + 4;
  const size_t kTagSize_CgBI = 16;

  // Signature (8 bytes), IHDR tag (8 bytes), IHDR data (13 bytes), IHDR CRC (4 bytes).
  if (size < kMinSize)
    return blTraceError(BL_ERROR_DATA_TRUNCATED);

  const uint8_t* start = p;
  // Check PNG signature.
  if (memcmp(p, blPngSignature, 8) != 0)
    return blTraceError(BL_ERROR_INVALID_SIGNATURE);
  p += 8;

  // Expect IHDR or CgBI chunk.
  uint32_t chunkTag = BLMemOps::readU32uBE(p + 4);
  uint32_t chunkSize = BLMemOps::readU32uBE(p + 0);

  // --------------------------------------------------------------------------
  // [CgBI]
  // --------------------------------------------------------------------------

  // Support "CgBI" aka "CoreGraphicsBrokenImage" - a violation of the PNG Spec:
  //   1. http://www.jongware.com/pngdefry.html
  //   2. http://iphonedevwiki.net/index.php/CgBI_file_format
  if (chunkTag == BL_MAKE_TAG('C', 'g', 'B', 'I')) {
    if (chunkSize != 4)
      return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

    if (size < kMinSize + kTagSize_CgBI)
      return blTraceError(BL_ERROR_DATA_TRUNCATED);

    // Skip "CgBI" chunk, data, and CRC.
    p += 12 + chunkSize;

    chunkTag = BLMemOps::readU32uBE(p + 4);
    chunkSize = BLMemOps::readU32uBE(p + 0);

    decoderI->statusFlags |= BL_PNG_DECODER_STATUS_SEEN_CgBI;
    decoderI->cgbi = true;
  }

  p += 8;

  // --------------------------------------------------------------------------
  // [IHDR]
  // --------------------------------------------------------------------------

  if (chunkTag != BL_MAKE_TAG('I', 'H', 'D', 'R') || chunkSize != 13)
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

  // IHDR Data [13 bytes].
  uint32_t w = BLMemOps::readU32uBE(p + 0);
  uint32_t h = BLMemOps::readU32uBE(p + 4);

  uint32_t sampleDepth = p[8];
  uint32_t colorType   = p[9];
  uint32_t compression = p[10];
  uint32_t filter      = p[11];
  uint32_t progressive = p[12];

  p += 13;

  // Ignore CRC.
  p += 4;

  // Width/Height can't be zero or greater than `2^31 - 1`.
  if (w == 0 || h == 0)
    return blTraceError(BL_ERROR_INVALID_DATA);

  if (w >= 0x80000000u || h >= 0x80000000u)
    return blTraceError(BL_ERROR_IMAGE_TOO_LARGE);

  if (!blPngCheckColorTypeAndBitDepth(colorType, sampleDepth))
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

  // Compression and filter has to be zero, progressive can be [0, 1].
  if (compression != 0 || filter != 0 || progressive >= 2)
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

  // Setup the BLImage+PNG information.
  decoderI->statusFlags |= BL_PNG_DECODER_STATUS_SEEN_IHDR;
  decoderI->colorType = uint8_t(colorType);
  decoderI->sampleDepth = uint8_t(sampleDepth);
  decoderI->sampleCount = blPngColorTypeToSampleCountTable[colorType];

  decoderI->imageInfo.size.reset(int(w), int(h));
  decoderI->imageInfo.depth = uint16_t(sampleDepth * uint32_t(decoderI->sampleCount));
  decoderI->imageInfo.frameCount = 1;

  decoderI->bufferIndex = (size_t)(p - start);
  return BL_SUCCESS;
}

static BLResult blPngDecoderImplReadFrameInternal(BLPngDecoderImpl* decoderI, BLImage* imageOut, const uint8_t* p, size_t size) noexcept {
  BLResult result = BL_SUCCESS;
  const uint8_t* begin = p;
  const uint8_t* end = p + size;

  // Make sure we won't read out of range.
  if ((size_t)(end - p) < decoderI->bufferIndex)
    return blTraceError(BL_ERROR_INVALID_STATE);
  p += decoderI->bufferIndex;

  // Basic information.
  uint32_t w = uint32_t(decoderI->imageInfo.size.w);
  uint32_t h = uint32_t(decoderI->imageInfo.size.h);
  uint32_t colorType = decoderI->colorType;

  // Palette & ColorKey.
  BLRgba32 pal[256];
  uint32_t palSize = 0;

  BLRgba64 colorKey {};
  bool hasColorKey = false;

  // Decode Chunks
  // -------------

  uint32_t i;
  size_t idatOff = 0;  // First IDAT chunk offset.
  size_t idatSize = 0; // Size of all IDAT chunks' p.

  for (;;) {
    // Chunk type, size, and CRC require 12 bytes.
    if ((size_t)(end - p) < 12)
      return blTraceError(BL_ERROR_DATA_TRUNCATED);

    uint32_t chunkTag = BLMemOps::readU32uBE(p + 4);
    uint32_t chunkSize = BLMemOps::readU32uBE(p + 0);

    // Make sure that we have p of the whole chunk.
    if ((size_t)(end - p) - 12 < chunkSize)
      return blTraceError(BL_ERROR_DATA_TRUNCATED);

    // Advance tag+size.
    p += 8;

    // IHDR - Once
    // -----------

    if (chunkTag == BL_MAKE_TAG('I', 'H', 'D', 'R')) {
      // Multiple IHDR chunks are not allowed.
      return blTraceError(BL_ERROR_PNG_MULTIPLE_IHDR);
    }

    // PLTE - Once
    // -----------

    else if (chunkTag == BL_MAKE_TAG('P', 'L', 'T', 'E')) {
      // 1. There must not be more than one PLTE chunk.
      // 2. It must precede the first IDAT chunk (also tRNS chunk).
      // 3. Contains 1...256 RGB palette entries.
      if ((decoderI->statusFlags & (BL_PNG_DECODER_STATUS_SEEN_PLTE | BL_PNG_DECODER_STATUS_SEEN_tRNS | BL_PNG_DECODER_STATUS_SEEN_IDAT)) != 0)
        return blTraceError(BL_ERROR_PNG_INVALID_PLTE);

      if (chunkSize == 0 || chunkSize > 768 || (chunkSize % 3) != 0)
        return blTraceError(BL_ERROR_PNG_INVALID_PLTE);

      palSize = chunkSize / 3;
      decoderI->statusFlags |= BL_PNG_DECODER_STATUS_SEEN_PLTE;

      for (i = 0; i < palSize; i++, p += 3)
        pal[i] = BLRgba32(p[0], p[1], p[2]);

      while (i < 256)
        pal[i++] = BLRgba32(0x00, 0x00, 0x00, 0xFF);
    }

    // tRNS - Once
    // -----------

    else if (chunkTag == BL_MAKE_TAG('t', 'R', 'N', 'S')) {
      // 1. There must not be more than one tRNS chunk.
      // 2. It must precede the first IDAT chunk, follow PLTE chunk, if any.
      // 3. It is prohibited for color types 4 and 6.
      if ((decoderI->statusFlags & (BL_PNG_DECODER_STATUS_SEEN_tRNS | BL_PNG_DECODER_STATUS_SEEN_IDAT)) != 0)
        return blTraceError(BL_ERROR_PNG_INVALID_TRNS);

      if (colorType == BL_PNG_COLOR_TYPE4_LUMA || colorType == BL_PNG_COLOR_TYPE6_RGBA)
        return blTraceError(BL_ERROR_PNG_INVALID_TRNS);

      if (colorType == BL_PNG_COLOR_TYPE0_LUM) {
        // For color type 0 (grayscale), the tRNS chunk contains a single gray level value, stored in the format:
        //   [0..1] Gray:  2 bytes, range 0 .. (2^depth)-1
        if (chunkSize != 2)
          return blTraceError(BL_ERROR_PNG_INVALID_TRNS);

        uint32_t gray = BLMemOps::readU16uBE(p);

        colorKey.reset(gray, gray, gray, 0);
        hasColorKey = true;

        p += 2;
      }
      else if (colorType == BL_PNG_COLOR_TYPE2_RGB) {
        // For color type 2 (truecolor), the tRNS chunk contains a single RGB color value, stored in the format:
        //   [0..1] Red:   2 bytes, range 0 .. (2^depth)-1
        //   [2..3] Green: 2 bytes, range 0 .. (2^depth)-1
        //   [4..5] Blue:  2 bytes, range 0 .. (2^depth)-1
        if (chunkSize != 6)
          return blTraceError(BL_ERROR_PNG_INVALID_TRNS);

        uint32_t r = BLMemOps::readU16uBE(p + 0);
        uint32_t g = BLMemOps::readU16uBE(p + 2);
        uint32_t b = BLMemOps::readU16uBE(p + 4);

        colorKey.reset(r, g, b, 0);
        hasColorKey = true;

        p += 6;
      }
      else {
        // For color type 3 (indexed color), the tRNS chunk contains a series of one-byte alpha values, corresponding
        // to entries in the PLTE chunk.
        BL_ASSERT(colorType == BL_PNG_COLOR_TYPE3_PAL);
        // 1. Has to follow PLTE if color type is 3.
        // 2. The tRNS chunk can contain 1...palSize alpha values, but in general it can contain less than `palSize`
        //    values, in that case the remaining alpha values are assumed to be 255.
        if ((decoderI->statusFlags & BL_PNG_DECODER_STATUS_SEEN_PLTE) == 0 || chunkSize == 0 || chunkSize > palSize)
          return blTraceError(BL_ERROR_PNG_INVALID_TRNS);

        for (i = 0; i < chunkSize; i++, p++)
          pal[i].setA(p[i]);
      }

      decoderI->statusFlags |= BL_PNG_DECODER_STATUS_SEEN_tRNS;
    }

    // IDAT - Many
    // -----------

    else if (chunkTag == BL_MAKE_TAG('I', 'D', 'A', 'T')) {
      if (idatOff == 0) {
        idatOff = (size_t)(p - begin) - 8;
        decoderI->statusFlags |= BL_PNG_DECODER_STATUS_SEEN_IDAT;

        BLOverflowFlag of = 0;
        idatSize = BLIntOps::addOverflow<size_t>(idatSize, chunkSize, &of);

        if (BL_UNLIKELY(of))
          return blTraceError(BL_ERROR_PNG_INVALID_IDAT);
      }

      p += chunkSize;
    }

    // IEND - Once
    // -----------

    else if (chunkTag == BL_MAKE_TAG('I', 'E', 'N', 'D')) {
      if (chunkSize != 0 || idatOff == 0)
        return blTraceError(BL_ERROR_PNG_INVALID_IEND);

      // Skip the CRC and break.
      p += 4;
      break;
    }

    // Unrecognized
    // ------------

    else {
      p += chunkSize;
    }

    // Skip chunk's CRC.
    p += 4;
  }

  // Decode
  // ------

  // If we reached this point it means that the image is most probably valid. The index of the first IDAT chunk
  // is stored in `idatOff` and should be non-zero.
  BL_ASSERT(idatOff != 0);

  BLFormat format = BL_FORMAT_PRGB32;
  uint32_t sampleDepth = decoderI->sampleDepth;
  uint32_t sampleCount = decoderI->sampleCount;

  uint32_t progressive = (decoderI->imageInfo.flags & BL_IMAGE_INFO_FLAG_PROGRESSIVE) != 0;
  uint32_t stepCount = progressive ? 7 : 1;

  BLPngInterlaceStep steps[7];
  uint32_t outputSize = blPngCalculateInterlaceSteps(steps,
    progressive ? blPngInterlaceTableAdam7 : blPngInterlaceTableNone,
    stepCount, sampleDepth, sampleCount, w, h);

  if (outputSize == 0)
    return blTraceError(BL_ERROR_INVALID_DATA);

  BLArray<uint8_t> output;
  BL_PROPAGATE(output.reserve(outputSize));

  BLPngDecoderReadData rd;
  rd.p = begin;
  rd.index = idatOff;

  result = BLCompression::Deflate::deflate(output, &rd, (BLCompression::Deflate::ReadFunc)blPngDecoderImplReadFunc, !decoderI->cgbi);
  if (result != BL_SUCCESS)
    return result;

  uint8_t* data = const_cast<uint8_t*>(output.data());
  uint32_t bytesPerPixel = blMax<uint32_t>((sampleDepth * sampleCount) / 8, 1);

  // If progressive `stepCount` is 7 and `steps` contains all windows.
  for (i = 0; i < stepCount; i++) {
    BLPngInterlaceStep& step = steps[i];
    if (!step.used)
      continue;
    BL_PROPAGATE(blPngOps.inverseFilter(data + step.offset, bytesPerPixel, step.bpl, step.height));
  }

  // Convert / Deinterlace
  // ---------------------

  BLImageData imageData;
  BL_PROPAGATE(imageOut->create(int(w), int(h), format));
  BL_PROPAGATE(imageOut->makeMutable(&imageData));

  uint8_t* dstPixels = static_cast<uint8_t*>(imageData.pixelData);
  intptr_t dstStride = imageData.stride;

  BLFormatInfo pngFmt {};
  pngFmt.depth = sampleDepth;

  if (BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_LE)
    pngFmt.flags |= BL_FORMAT_FLAG_BYTE_SWAP;

  if (colorType == BL_PNG_COLOR_TYPE0_LUM && sampleDepth <= 8) {
    // Treat grayscale images up to 8bpp as indexed and create a dummy palette.
    blPngCreateGrayscalePalette(pal, sampleDepth);

    // Handle color-key properly.
    if (hasColorKey && colorKey.r() < (1u << sampleDepth))
      pal[colorKey.r()] = BLRgba32(0);

    pngFmt.flags |= BL_FORMAT_FLAG_RGBA | BL_FORMAT_FLAG_INDEXED;
    pngFmt.palette = pal;
  }
  else if (colorType == BL_PNG_COLOR_TYPE3_PAL) {
    pngFmt.flags |= BL_FORMAT_FLAG_RGBA | BL_FORMAT_FLAG_INDEXED;
    pngFmt.palette = pal;
  }
  else {
    pngFmt.depth *= sampleCount;

    if (colorType == BL_PNG_COLOR_TYPE0_LUM) {
      // TODO: [PNG] 16-BPC.
    }
    else if (colorType == BL_PNG_COLOR_TYPE2_RGB) {
      pngFmt.flags |= BL_FORMAT_FLAG_RGB;
      pngFmt.rSize = 8; pngFmt.rShift = 16;
      pngFmt.gSize = 8; pngFmt.gShift = 8;
      pngFmt.bSize = 8; pngFmt.bShift = 0;
    }
    else if (colorType == BL_PNG_COLOR_TYPE4_LUMA) {
      pngFmt.flags |= BL_FORMAT_FLAG_LUMA;
      pngFmt.rSize = 8; pngFmt.rShift = 8;
      pngFmt.gSize = 8; pngFmt.gShift = 8;
      pngFmt.bSize = 8; pngFmt.bShift = 8;
      pngFmt.aSize = 8; pngFmt.aShift = 0;
    }
    else if (colorType == BL_PNG_COLOR_TYPE6_RGBA) {
      pngFmt.flags |= BL_FORMAT_FLAG_RGBA;
      pngFmt.rSize = 8; pngFmt.rShift = 24;
      pngFmt.gSize = 8; pngFmt.gShift = 16;
      pngFmt.bSize = 8; pngFmt.bShift = 8;
      pngFmt.aSize = 8; pngFmt.aShift = 0;
    }

    if (decoderI->cgbi) {
      std::swap(pngFmt.rShift, pngFmt.bShift);
      if (pngFmt.flags & BL_FORMAT_FLAG_ALPHA)
        pngFmt.flags |= BL_FORMAT_FLAG_PREMULTIPLIED;
    }
  }

  BLPixelConverter pc;
  BL_PROPAGATE(pc.create(blFormatInfo[format], pngFmt,
    BLPixelConverterCreateFlags(
      BL_PIXEL_CONVERTER_CREATE_FLAG_DONT_COPY_PALETTE |
      BL_PIXEL_CONVERTER_CREATE_FLAG_ALTERABLE_PALETTE)));

  if (progressive) {
    // PNG interlacing requires 7 steps, where 7th handles all even scanlines (indexing from 1). This means that we
    // can, in general, reuse the buffer required by 7th step as a temporary to merge steps 1-6. To achieve this,
    // we need to:
    //
    //   1. Convert all even scanlines already ready by 7th step to `dst`. This makes the buffer ready to be reused.
    //   2. Merge pixels from steps 1-6 into that buffer.
    //   3. Convert all odd scanlines (from the reused buffer) to `dst`.
    //
    // We, in general, process 4 odd scanlines at a time, so we need the 7th buffer to have enough space to hold them
    // as well, if not, we allocate an extra buffer and use it instead. This approach is good as small images would
    // probably require the extra buffer, but larger images can reuse the 7th.
    BL_ASSERT(steps[6].width == w);
    BL_ASSERT(steps[6].height == h / 2); // Half of the rows, rounded down.

    uint32_t depth = sampleDepth * sampleCount;
    uint32_t tmpHeight = blMin<uint32_t>((h + 1) / 2, 4);
    uint32_t tmpBpl = steps[6].bpl;
    uint32_t tmpSize;

    if (steps[6].height)
      pc.convertRect(dstPixels + dstStride, dstStride * 2, data + 1 + steps[6].offset, tmpBpl, w, steps[6].height);

    // Align `tmpBpl` so we can use aligned memory writes and reads while using it.
    tmpBpl = BLIntOps::alignUp(tmpBpl, 16);
    tmpSize = tmpBpl * tmpHeight;

    BLScopedBuffer tmpAlloc;
    uint8_t* tmp;

    // Decide whether to alloc an extra buffer of to reuse 7th.
    if (steps[6].size < tmpSize + 15)
      tmp = static_cast<uint8_t*>(tmpAlloc.alloc(tmpSize + 15));
    else
      tmp = data + steps[6].offset;

    if (!tmp)
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    tmp = BLIntOps::alignUp(tmp, 16);
    switch (depth) {
      case 1 : blPngDeinterlaceBits<1>(dstPixels, dstStride, pc, tmp, tmpBpl, data, steps, w, h); break;
      case 2 : blPngDeinterlaceBits<2>(dstPixels, dstStride, pc, tmp, tmpBpl, data, steps, w, h); break;
      case 4 : blPngDeinterlaceBits<4>(dstPixels, dstStride, pc, tmp, tmpBpl, data, steps, w, h); break;
      case 8 : blPngDeinterlaceBytes<1>(dstPixels, dstStride, pc, tmp, tmpBpl, data, steps, w, h); break;
      case 16: blPngDeinterlaceBytes<2>(dstPixels, dstStride, pc, tmp, tmpBpl, data, steps, w, h); break;
      case 24: blPngDeinterlaceBytes<3>(dstPixels, dstStride, pc, tmp, tmpBpl, data, steps, w, h); break;
      case 32: blPngDeinterlaceBytes<4>(dstPixels, dstStride, pc, tmp, tmpBpl, data, steps, w, h); break;
    }
  }
  else {
    BL_ASSERT(steps[0].width == w);
    BL_ASSERT(steps[0].height == h);

    pc.convertRect(dstPixels, dstStride, data + 1, steps[0].bpl, w, h);
  }

  decoderI->bufferIndex = (size_t)(p - begin);
  return result;
}

static BLResult BL_CDECL blPngDecoderImplReadInfo(BLImageDecoderImpl* impl, BLImageInfo* infoOut, const uint8_t* data, size_t size) noexcept {
  BLPngDecoderImpl* decoderI = static_cast<BLPngDecoderImpl*>(impl);
  BLResult result = decoderI->lastResult;

  if (decoderI->bufferIndex == 0 && result == BL_SUCCESS) {
    result = blPngDecoderImplReadInfoInternal(decoderI, data, size);
    if (result != BL_SUCCESS)
      decoderI->lastResult = result;
  }

  if (infoOut)
    memcpy(infoOut, &decoderI->imageInfo, sizeof(BLImageInfo));

  return result;
}

static BLResult BL_CDECL blPngDecoderImplReadFrame(BLImageDecoderImpl* impl, BLImageCore* imageOut, const uint8_t* data, size_t size) noexcept {
  BLPngDecoderImpl* decoderI = static_cast<BLPngDecoderImpl*>(impl);
  BL_PROPAGATE(blPngDecoderImplReadInfo(decoderI, nullptr, data, size));

  if (decoderI->frameIndex)
    return blTraceError(BL_ERROR_NO_MORE_DATA);

  BLResult result = blPngDecoderImplReadFrameInternal(decoderI, static_cast<BLImage*>(imageOut), data, size);
  if (result != BL_SUCCESS)
    decoderI->lastResult = result;
  return result;
}

static BLResult BL_CDECL blPngDecoderImplCreate(BLImageDecoderCore* self) noexcept {
  BLPngDecoderImpl* decoderI = blObjectDetailAllocImplT<BLPngDecoderImpl>(self, BLObjectInfo::packType(BL_OBJECT_TYPE_IMAGE_DECODER));

  if (BL_UNLIKELY(!decoderI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  decoderI->ctor(&blPngDecoderVirt, &blPngCodecObject);
  return blPngDecoderImplRestart(decoderI);
}

static BLResult BL_CDECL blPngDecoderImplDestroy(BLObjectImpl* impl, uint32_t info) noexcept {
  BLPngDecoderImpl* decoderI = static_cast<BLPngDecoderImpl*>(impl);
  decoderI->dtor();
  return blObjectDetailFreeImpl(decoderI, info);
}

// ============================================================================
// [BLPngEncoder]
// ============================================================================

class BLOutputBuffer {
public:
  uint8_t* _data = nullptr;
  uint8_t* _ptr = nullptr;
  uint8_t* _end = nullptr;

  BL_INLINE BLOutputBuffer() noexcept {}

  BL_INLINE BLOutputBuffer(uint8_t* data, size_t size) noexcept
    : _data(data),
      _ptr(data),
      _end(data + size) {}

  BL_INLINE uint8_t* ptr() const noexcept { return _ptr; }
  BL_INLINE uint8_t* end() const noexcept { return _end; }

  BL_INLINE size_t index() const noexcept { return (size_t)(_ptr - _data); }
  BL_INLINE size_t capacity() const noexcept { return (size_t)(_end - _data); }
  BL_INLINE size_t remainingSize() const noexcept { return (size_t)(_end - _ptr); }

  BL_INLINE void reset() noexcept {
    _data = nullptr;
    _ptr = nullptr;
    _end = nullptr;
  }

  BL_INLINE void reset(uint8_t* data, size_t size) noexcept {
    _data = data;
    _ptr = data;
    _end = data + size;
  }

  BL_INLINE void appendByte(uint8_t value) noexcept {
    BL_ASSERT(remainingSize() >= 1);
    *_ptr++ = value;
  }

  BL_INLINE void appendUInt16(uint16_t value) noexcept {
    BL_ASSERT(remainingSize() >= 2);
    BLMemOps::writeU16u(_ptr, value);
    _ptr += 2;
  }

  BL_INLINE void appendUInt16LE(uint16_t value) noexcept {
    BL_ASSERT(remainingSize() >= 2);
    BLMemOps::writeU16uLE(_ptr, value);
    _ptr += 2;
  }

  BL_INLINE void appendUInt16BE(uint16_t value) noexcept {
    BL_ASSERT(remainingSize() >= 2);
    BLMemOps::writeU16uBE(_ptr, value);
    _ptr += 2;
  }

  BL_INLINE void appendUInt32(uint32_t value) noexcept {
    BL_ASSERT(remainingSize() >= 4);
    BLMemOps::writeU32u(_ptr, value);
    _ptr += 4;
  }

  BL_INLINE void appendUInt32LE(uint32_t value) noexcept {
    BL_ASSERT(remainingSize() >= 4);
    BLMemOps::writeU32uLE(_ptr, value);
    _ptr += 4;
  }

  BL_INLINE void appendUInt32BE(uint32_t value) noexcept {
    BL_ASSERT(remainingSize() >= 4);
    BLMemOps::writeU32uBE(_ptr, value);
    _ptr += 4;
  }

  BL_INLINE void appendUInt64(uint64_t value) noexcept {
    BL_ASSERT(remainingSize() >= 4);
    BLMemOps::writeU64u(_ptr, value);
    _ptr += 8;
  }

  BL_INLINE void appendUInt64LE(uint64_t value) noexcept {
    BL_ASSERT(remainingSize() >= 4);
    BLMemOps::writeU64uLE(_ptr, value);
    _ptr += 8;
  }

  BL_INLINE void appendUInt64BE(uint64_t value) noexcept {
    BL_ASSERT(remainingSize() >= 4);
    BLMemOps::writeU64uBE(_ptr, value);
    _ptr += 8;
  }

  BL_INLINE void appendData(const uint8_t* data, size_t size) noexcept {
    BL_ASSERT(remainingSize() >= size);
    memcpy(_ptr, data, size);
    _ptr += size;
  }
};

static BLResult BL_CDECL blPngEncoderImplRestart(BLImageEncoderImpl* impl) noexcept {
  BLPngEncoderImpl* encoderI = static_cast<BLPngEncoderImpl*>(impl);

  encoderI->lastResult = BL_SUCCESS;
  encoderI->frameIndex = 0;
  encoderI->bufferIndex = 0;
  encoderI->compressionLevel = 5;

  return BL_SUCCESS;
}

static BLResult BL_CDECL blPngEncoderImplGetProperty(const BLObjectImpl* impl, const char* name, size_t nameSize, BLVarCore* valueOut) noexcept {
  const BLPngEncoderImpl* encoderI = static_cast<const BLPngEncoderImpl*>(impl);

  if (blMatchProperty(name, nameSize, "compression")) {
    return blVarAssignUInt64(valueOut, encoderI->compressionLevel);
  }

  return blObjectImplGetProperty(encoderI, name, nameSize, valueOut);
}

static BLResult BL_CDECL blPngEncoderImplSetProperty(BLObjectImpl* impl, const char* name, size_t nameSize, const BLVarCore* value) noexcept {
  BLPngEncoderImpl* encoderI = static_cast<BLPngEncoderImpl*>(impl);

  if (blMatchProperty(name, nameSize, "compression")) {
    uint64_t v;
    BL_PROPAGATE(blVarToUInt64(value, &v));
    encoderI->compressionLevel = uint8_t(blMin<uint64_t>(v, 12));
    return BL_SUCCESS;
  }

  return blObjectImplSetProperty(encoderI, name, nameSize, value);
}

class ChunkWriter {
public:
  uint8_t* chunkData = nullptr;

  BL_INLINE void start(BLOutputBuffer& output, uint32_t tag) noexcept {
    chunkData = output.ptr();
    output.appendUInt32BE(0);
    output.appendUInt32BE(tag);
  }

  BL_INLINE void done(BLOutputBuffer& output) noexcept {
    const uint8_t* start = chunkData + 8;
    size_t chunkLength = (size_t)(output.ptr() - start);
    BLMemOps::writeU32uBE(chunkData, uint32_t(chunkLength));

    // PNG Specification: CRC is calculated on the preceding bytes in the chunk, including
    // the chunk type code and chunk data fields, but not including the length field.
    output.appendUInt32BE(BLCompression::crc32(start - 4, chunkLength + 4));
  }
};

static BLResult filterImageData(uint8_t* data, intptr_t stride, uint32_t bitsPerPixel, uint32_t w, uint32_t h) noexcept {
  blUnused(bitsPerPixel, w);

  for (uint32_t y = 0; y < h; y++) {
    data[0] = 0;
    data += stride;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blPngEncoderImplWriteFrame(BLImageEncoderImpl* impl, BLArrayCore* dst, const BLImageCore* image) noexcept {
  BLPngEncoderImpl* encoderI = static_cast<BLPngEncoderImpl*>(impl);
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

  // Setup target PNG format and other information.
  BLFormatInfo pngFormatInfo {};
  uint8_t pngBitDepth = 0;
  uint8_t pngColorType = 0;

  switch (format) {
    case BL_FORMAT_PRGB32:
      pngFormatInfo.depth = 32;
      pngFormatInfo.flags = BL_FORMAT_FLAG_RGBA | BL_FORMAT_FLAG_BE;
      pngFormatInfo.setSizes(8, 8, 8, 8);
      pngFormatInfo.setShifts(24, 16, 8, 0);
      pngBitDepth = 8;
      pngColorType = 6;
      break;

    case BL_FORMAT_XRGB32:
      pngFormatInfo.depth = 24;
      pngFormatInfo.flags = BL_FORMAT_FLAG_RGB | BL_FORMAT_FLAG_BE;
      pngFormatInfo.setSizes(8, 8, 8, 0);
      pngFormatInfo.setShifts(16, 8, 0, 0);
      pngBitDepth = 8;
      pngColorType = 2;
      break;

    case BL_FORMAT_A8:
      pngFormatInfo.depth = 8;
      pngFormatInfo.flags = BL_FORMAT_FLAG_ALPHA;
      pngFormatInfo.setSizes(0, 0, 0, 8);
      pngFormatInfo.setShifts(0, 0, 0, 0);
      pngBitDepth = 8;
      pngColorType = 0;
      break;
  }

  // Setup pixel converter and convert the input image to PNG representation.
  BLPixelConverter pc;
  BL_PROPAGATE(pc.create(pngFormatInfo, blFormatInfo[format]));

  uint32_t uncompressedStride = ((w * pngFormatInfo.depth + 7) / 8u) + 1;
  size_t uncompressedDataSize = uncompressedStride * h;

  BLScopedBuffer uncompressedBuffer;
  uint8_t* uncompressedData = static_cast<uint8_t*>(uncompressedBuffer.alloc(uncompressedDataSize));

  if (BL_UNLIKELY(!uncompressedData))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BL_PROPAGATE(pc.convertRect(uncompressedData + 1, uncompressedStride, imageData.pixelData, imageData.stride, w, h));
  BL_PROPAGATE(filterImageData(uncompressedData, uncompressedStride, pngFormatInfo.depth, w, h));

  // Setup a deflate encoder - higher compression levels require more space, so init it now.
  BLCompression::Deflate::Encoder deflateEncoder;
  BL_PROPAGATE(deflateEncoder.init(BLCompression::Deflate::kFormatZlib, encoderI->compressionLevel));

  // Create PNG file.
  size_t outputWorstCaseSize = deflateEncoder.minimumOutputBufferSize(uncompressedDataSize);

  size_t signatureSize = 8;
  size_t ihdrSize = 12 + 13;
  size_t idatSize = 12 + outputWorstCaseSize;
  size_t iendSize = 12;

  size_t reserveBytes = signatureSize + ihdrSize + idatSize + iendSize;
  uint8_t* outputData;
  BL_PROPAGATE(buf.modifyOp(BL_MODIFY_OP_APPEND_FIT, reserveBytes, &outputData));

  // Prepare output buffer and chunk writer.
  BLOutputBuffer output(outputData, reserveBytes);
  ChunkWriter chunk;

  // Write PNG signature.
  output.appendData(blPngSignature, signatureSize);

  // Write IHDR chunk.
  chunk.start(output, BL_MAKE_TAG('I', 'H', 'D', 'R'));
  output.appendUInt32BE(w);        // Image width.
  output.appendUInt32BE(h);        // Image height.
  output.appendByte(pngBitDepth);  // Bit depth (1, 2, 4, 8, 16).
  output.appendByte(pngColorType); // Color type (0, 2, 3, 4, 6).
  output.appendByte(0);            // Compression method, must be zero.
  output.appendByte(0);            // Filter method, must be zero.
  output.appendByte(0);            // Interlace method (0 == no interlacing).
  chunk.done(output);

  // Write IDAT chunk.
  chunk.start(output, BL_MAKE_TAG('I', 'D', 'A', 'T'));
  output._ptr += deflateEncoder.compress(output.ptr(), output.remainingSize(), uncompressedData, uncompressedDataSize);
  chunk.done(output);

  // Write IEND chunk.
  chunk.start(output, BL_MAKE_TAG('I', 'E', 'N', 'D'));
  chunk.done(output);

  BLArrayPrivate::setSize(dst, (size_t)(output.ptr() - buf.data()));
  return BL_SUCCESS;
}

static BLResult blPngEncoderImplCreate(BLImageEncoderCore* self) noexcept {
  BLPngEncoderImpl* encoderI = blObjectDetailAllocImplT<BLPngEncoderImpl>(self, BLObjectInfo::packType(BL_OBJECT_TYPE_IMAGE_ENCODER));

  if (BL_UNLIKELY(!encoderI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  encoderI->ctor(&blPngEncoderVirt, &blPngCodecObject);
  return blPngEncoderImplRestart(encoderI);
}

static BLResult BL_CDECL blPngEncoderImplDestroy(BLObjectImpl* impl, uint32_t info) noexcept {
  BLPngEncoderImpl* encoderI = static_cast<BLPngEncoderImpl*>(impl);
  encoderI->dtor();
  return blObjectDetailFreeImpl(encoderI, info);
}

// ============================================================================
// [BLPngCodec - Impl]
// ============================================================================

static BLResult BL_CDECL blPngCodecImplDestroy(BLObjectImpl* impl, uint32_t info) noexcept {
  // Built-in codecs are never destroyed.
  blUnused(impl, info);
  return BL_SUCCESS;
}

static uint32_t BL_CDECL blPngCodecImplInspectData(const BLImageCodecImpl* impl, const uint8_t* data, size_t size) noexcept {
  blUnused(impl);

  // Minimum PNG size and signature.
  if (size < 8 || memcmp(data, blPngSignature, 8) != 0)
    return 0;

  return 100;
}

static BLResult BL_CDECL blPngCodecImplCreateDecoder(const BLImageCodecImpl* impl, BLImageDecoderCore* dst) noexcept {
  blUnused(impl);

  BLImageDecoderCore tmp;
  BL_PROPAGATE(blPngDecoderImplCreate(&tmp));
  return blImageDecoderAssignMove(dst, &tmp);
}

static BLResult BL_CDECL blPngCodecImplCreateEncoder(const BLImageCodecImpl* impl, BLImageEncoderCore* dst) noexcept {
  blUnused(impl);

  BLImageEncoderCore tmp;
  BL_PROPAGATE(blPngEncoderImplCreate(&tmp));
  return blImageEncoderAssignMove(dst, &tmp);
}

// ============================================================================
// [BLPngCodec - Runtime]
// ============================================================================

void blPngCodecOnInit(BLRuntimeContext* rt, BLArray<BLImageCodec>* codecs) noexcept {
  // Initialize PNG ops.
  blPngOpsOnInit(rt);

  // Initialize PNG codec.
  blPngCodec.virt.base.destroy = blPngCodecImplDestroy;
  blPngCodec.virt.base.getProperty = blObjectImplGetProperty;
  blPngCodec.virt.base.setProperty = blObjectImplSetProperty;
  blPngCodec.virt.inspectData = blPngCodecImplInspectData;
  blPngCodec.virt.createDecoder = blPngCodecImplCreateDecoder;
  blPngCodec.virt.createEncoder = blPngCodecImplCreateEncoder;

  blPngCodec.impl->ctor(&blPngCodec.virt);
  blPngCodec.impl->features =
    BL_IMAGE_CODEC_FEATURE_READ     |
    BL_IMAGE_CODEC_FEATURE_WRITE    |
    BL_IMAGE_CODEC_FEATURE_LOSSLESS ;
  blPngCodec.impl->name.dcast().assign("PNG");
  blPngCodec.impl->vendor.dcast().assign("Blend2D");
  blPngCodec.impl->mimeType.dcast().assign("image/png");
  blPngCodec.impl->extensions.dcast().assign("png");

  blPngCodecObject._d.initDynamic(BL_OBJECT_TYPE_IMAGE_CODEC, BLObjectInfo{BL_OBJECT_INFO_IMMUTABLE_FLAG}, &blPngCodec.impl);

  // Initialize PNG decoder virtual functions.
  blPngDecoderVirt.base.destroy = blPngDecoderImplDestroy;
  blPngDecoderVirt.base.getProperty = blObjectImplGetProperty;
  blPngDecoderVirt.base.setProperty = blObjectImplSetProperty;
  blPngDecoderVirt.restart = blPngDecoderImplRestart;
  blPngDecoderVirt.readInfo = blPngDecoderImplReadInfo;
  blPngDecoderVirt.readFrame = blPngDecoderImplReadFrame;

  // Initialize PNG encoder virtual functions.
  blPngEncoderVirt.base.destroy = blPngEncoderImplDestroy;
  blPngEncoderVirt.base.getProperty = blPngEncoderImplGetProperty;
  blPngEncoderVirt.base.setProperty = blPngEncoderImplSetProperty;
  blPngEncoderVirt.restart = blPngEncoderImplRestart;
  blPngEncoderVirt.writeFrame = blPngEncoderImplWriteFrame;

  codecs->append(blPngCodecObject.dcast());
}
