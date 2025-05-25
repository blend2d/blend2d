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
#include "../support/ptrops_p.h"
#include "../support/scopedbuffer_p.h"

namespace bl::Png {

// bl::Png::Codec - Globals
// ========================

static BLObjectEternalVirtualImpl<BLPngCodecImpl, BLImageCodecVirt> pngCodec;
static BLImageCodecCore pngCodecInstance;

static BLImageDecoderVirt pngDecoderVirt;
static BLImageEncoderVirt pngEncoderVirt;

// bl::Png::Codec - Constants
// ==========================

// PNG file signature (8 bytes).
static constexpr uint8_t kPngSignature[8] = { 0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au };

// Allowed bits-per-sample per "ColorType"
static constexpr uint8_t kColorTypeBitDepthTable[7] = { 0x1Fu, 0u, 0x18u, 0x0Fu, 0x18u, 0u, 0x18u };

// Count of samples per "ColorType".
static constexpr uint8_t kColorTypeToSampleCountTable[7] = { 1u, 0u, 3u, 1u, 2u, 0u, 4u };

static constexpr uint32_t kPngSignatureSize = 8u;

static constexpr uint32_t kPngChunkHeaderSize = 8u;
static constexpr uint32_t kPngChunkCRCSize = 4u;
static constexpr uint32_t kPngChunkBaseSize = 12u;

static constexpr uint32_t kPngChunkDataSize_CgBI = 4u;
static constexpr uint32_t kPngChunkDataSize_IHDR = 13u;
static constexpr uint32_t kPngChunkDataSize_acTL = 8u;
static constexpr uint32_t kPngChunkDataSize_fcTL = 26;

// bl::Png::Codec - Utilities
// ==========================

static BL_INLINE bool checkColorTypeAndBitDepth(uint32_t colorType, uint32_t depth) noexcept {
  // TODO: [PNG] 16-BPC.
  if (depth == 16)
    return false;

  return colorType < BL_ARRAY_SIZE(kColorTypeBitDepthTable) &&
         (kColorTypeBitDepthTable[colorType] & depth) != 0 &&
         IntOps::isPowerOf2(depth);
}

static BL_INLINE void createGrayscalePalette(BLRgba32* pal, uint32_t depth) noexcept {
  static const uint32_t scaleTable[9] = { 0, 0xFF, 0x55, 0, 0x11, 0, 0, 0, 0x01 };
  BL_ASSERT(depth < BL_ARRAY_SIZE(scaleTable));

  uint32_t scale = uint32_t(scaleTable[depth]) * 0x00010101;
  uint32_t count = 1u << depth;
  uint32_t value = 0xFF000000;

  for (uint32_t i = 0; i < count; i++, value += scale)
    pal[i].value = value;
}

// bl::Png::Codec - Interlace / Deinterlace
// ========================================

// A single PNG interlace/deinterlace step related to the full image size.
struct InterlaceStep {
  uint32_t used;
  uint32_t width;
  uint32_t height;
  uint32_t bpl;

  uint32_t offset;
  uint32_t size;
};

// PNG deinterlace table data.
struct InterlaceTable {
  uint8_t xOff;
  uint8_t yOff;
  uint8_t xPow;
  uint8_t yPow;
};

// No interlacing.
static const InterlaceTable interlaceTableNone[1] = {
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
static const InterlaceTable interlaceTableAdam7[7] = {
  { 0, 0, 3, 3 },
  { 4, 0, 3, 3 },
  { 0, 4, 2, 3 },
  { 2, 0, 2, 2 },
  { 0, 2, 1, 2 },
  { 1, 0, 1, 1 },
  { 0, 1, 0, 1 }
};

static uint32_t calculateInterlaceSteps(
  InterlaceStep* dst, const InterlaceTable* table, uint32_t stepCount,
  uint32_t sampleDepth, uint32_t sampleCount,
  uint32_t w, uint32_t h) noexcept {

  // Byte-offset of each chunk.
  uint32_t offset = 0;

  for (uint32_t i = 0; i < stepCount; i++, dst++) {
    const InterlaceTable& tab = table[i];

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
    bl::OverflowFlag of{};
    offset = IntOps::addOverflow(offset, size, &of);

    if (BL_UNLIKELY(of)) {
      return 0;
    }
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

// Deinterlace a PNG image that has depth less than 8 bits. This is a bit tricky as one byte describes two or more
// pixels that can be fetched from 1st to 6th progressive images. Basically each bit depth is implemented separately
// as generic case would be very inefficient. Also, the destination image is handled pixel-by-pixel fetching data
// from all possible scanlines as necessary - this is a bit different when compared with `deinterlaceBytes()`.
template<uint32_t N>
static void deinterlaceBits(
  uint8_t* dstLine, intptr_t dstStride, const BLPixelConverter& pc,
  uint8_t* tmpLine, intptr_t tmpStride, const uint8_t* data, const InterlaceStep* steps,
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
    uint8_t* tmpData = tmpLine + (intptr_t(n) * tmpStride);
    uint32_t x = w;

    // 1-BPP
    // -----

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

          tmpData[0] = combineByte1bpp(a     , b >>  9, b >> 2, b >> 10, a >> 12, b >> 11, b >> 5, b >> 12);
          if (x <= 8) break;

          tmpData[1] = combineByte1bpp(a << 1, b >>  5, b     , b >>  6, a >> 11, b >>  7, b >> 3, b >>  8);
          if (x <= 16) break;

          tmpData[2] = combineByte1bpp(a << 2, b >> 17, b << 2, b >> 18, a >> 10, b >> 19, b >> 1, b >> 20);
          if (x <= 24) break;

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

          tmpData[0] = combineByte1bpp(a     , b >>  1, a >> 10, b >>  2, a >>  3, b >>  3, a >> 13, b >>  4);
          if (x <=  8) break;

          tmpData[1] = combineByte1bpp(a << 2, b <<  3, a >>  8, b <<  2, a >>  1, b <<  1, a >> 11, b      );
          if (x <= 16) break;

          tmpData[2] = combineByte1bpp(a << 4, b >>  9, a >>  6, b >> 10, a <<  1, b >> 11, a >>  9, b >> 12);
          if (x <= 24) break;

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

          tmpData[0] = combineByte1bpp(a     , b >> 1, a >> 1, b >> 2, a >> 2, b >> 3, a >> 3, b >> 4);

          if (x <= 8) break;
          tmpData[1] = combineByte1bpp(a << 4, b << 3, a << 3, b << 2, a << 2, b << 1, a << 1, b     );
          break;
        }
      }
    }

    // 2-BPP
    // -----

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

          tmpData[0] = combineByte2bpp(a     , b >> 10, b >> 4, b >> 12);
          if (x <=  4) break;

          tmpData[1] = combineByte2bpp(a >> 8, b >>  6, b >> 2, b >>  8);
          if (x <=  8) break;

          tmpData[2] = combineByte2bpp(a << 2, b >> 18, b     , b >> 20);
          if (x <= 12) break;

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

          tmpData[0] = combineByte2bpp(a     , b >>  2, a >> 12, b >>  4);
          if (x <=  4) break;

          tmpData[1] = combineByte2bpp(a << 2, b <<  2, a >> 10, b      );
          if (x <=  8) break;

          tmpData[2] = combineByte2bpp(a << 4, b >> 10, a >>  8, b >> 12);
          if (x <= 12) break;

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

          tmpData[0] = combineByte2bpp(a     , b >> 10, b >> 4, b >> 12);
          if (x <=  4) break;

          tmpData[1] = combineByte2bpp(a >> 8, b >>  6, b >> 2, b >>  8);
          break;
        }
      }
    }

    // 4-BPP
    // -----

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

          tmpData[0] = combineByte4bpp(a, b >> 12);
          if (x <= 2) break;

          tmpData[1] = combineByte4bpp(b, b >> 8);
          if (x <= 4) break;

          tmpData[2] = combineByte4bpp(a >> 8, b >> 20);
          if (x <= 6) break;

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
            tmpData[0] = combineByte4bpp(a, b >> 4);
            tmpData[1] = combineByte4bpp(a >> 8, b);

            b = uint32_t(*d5++);
            tmpData[2] = combineByte4bpp(a << 4, b >> 4);
            tmpData[3] = combineByte4bpp(a >> 4, b);
            tmpData += 4;

            x -= 8;
          }

          if (!x)
            break;

          a = uint32_t(*d2++);
          b = 0;

          if (x >= 3) a += (uint32_t(*d3++) << 8);
          if (x >= 2) b  = (uint32_t(*d5++)     );

          tmpData[0] = combineByte4bpp(a, b >> 4);
          if (x <= 2) break;

          tmpData[1] = combineByte4bpp(a >> 8, b);
          if (x <= 4) break;

          b = uint32_t(*d5++);
          tmpData[2] = combineByte4bpp(a << 4, b >> 4);
          if (x <= 6) break;

          tmpData[3] = combineByte4bpp(a >> 4, b);
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

            tmpData[0] = combineByte4bpp(a, b >> 4);
            tmpData[1] = combineByte4bpp(a << 4, b);
            tmpData += 2;

            x -= 4;
          }

          if (!x)
            break;

          a = uint32_t(*d4++);
          b = 0;

          if (x >= 2)
            b = uint32_t(*d5++);

          tmpData[0] = combineByte4bpp(a, b >> 4);
          if (x <= 2) break;

          tmpData[1] = combineByte4bpp(a << 4, b);
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
static BL_INLINE const uint8_t* copyBytes(uint8_t* dst, const uint8_t* src) noexcept {
  if (N == 2) {
    MemOps::writeU16a(dst, MemOps::readU16u(src));
  }
  else if (N == 4) {
    MemOps::writeU32a(dst, MemOps::readU32u(src));
  }
  else if (N == 8) {
    MemOps::writeU32a(dst + 0, MemOps::readU32u(src + 0));
    MemOps::writeU32a(dst + 4, MemOps::readU32u(src + 4));
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
static void deinterlaceBytes(
  uint8_t* dstLine, intptr_t dstStride, const BLPixelConverter& pc,
  uint8_t* tmpLine, intptr_t tmpStride, const uint8_t* data, const InterlaceStep* steps,
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
    uint8_t* tmpData = tmpLine + (intptr_t(n) * tmpStride);
    uint32_t x;

    switch (n) {
      // [05351535]
      case 0: {
        d0 += 1;
        d1 += (w >= 5);
        d3 += (w >= 3);
        d5 += (w >= 2);

        for (x = 0 * N; x < xMax; x += 8 * N) d0 = copyBytes<N>(tmpData + x, d0);
        for (x = 4 * N; x < xMax; x += 8 * N) d1 = copyBytes<N>(tmpData + x, d1);
        for (x = 2 * N; x < xMax; x += 4 * N) d3 = copyBytes<N>(tmpData + x, d3);
        for (x = 1 * N; x < xMax; x += 2 * N) d5 = copyBytes<N>(tmpData + x, d5);

        break;
      }

      // [25352535]
      case 2: {
        d2 += 1;
        d3 += (w >= 3);
        d5 += (w >= 2);

        for (x = 0 * N; x < xMax; x += 4 * N) d2 = copyBytes<N>(tmpData + x, d2);
        for (x = 2 * N; x < xMax; x += 4 * N) d3 = copyBytes<N>(tmpData + x, d3);
        for (x = 1 * N; x < xMax; x += 2 * N) d5 = copyBytes<N>(tmpData + x, d5);

        break;
      }

      // [45454545]
      case 1:
      case 3: {
        d4 += 1;
        d5 += (w >= 2);

        for (x = 0 * N; x < xMax; x += 2 * N) d4 = copyBytes<N>(tmpData + x, d4);
        for (x = 1 * N; x < xMax; x += 2 * N) d5 = copyBytes<N>(tmpData + x, d5);

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

// bl::Png::Codec - Decoder - ChunkReader
// ======================================

namespace {

class ChunkReader {
public:
  const uint8_t* ptr {};
  const uint8_t* end {};

  BL_INLINE_NODEBUG ChunkReader(const uint8_t* ptr, const uint8_t* end) noexcept
    : ptr(ptr),
      end(end) {}

  BL_INLINE_NODEBUG size_t remainingBytes() const noexcept { return PtrOps::bytesUntil(ptr, end); }

  BL_INLINE void advance(size_t size) noexcept {
    BL_ASSERT(size <= remainingBytes());
    ptr += size;
  }

  BL_INLINE void advanceChunkHeader() noexcept {
    BL_ASSERT(remainingBytes() >= kPngChunkHeaderSize);
    ptr += kPngChunkHeaderSize;
  }

  BL_INLINE void advanceChecksum() noexcept {
    BL_ASSERT(remainingBytes() >= kPngChunkCRCSize);
    ptr += kPngChunkCRCSize;
  }

  BL_INLINE_NODEBUG bool atEnd() const noexcept { return ptr == end; }
  BL_INLINE_NODEBUG bool hasChunk() const noexcept { return remainingBytes() >= kPngChunkBaseSize; }

  BL_INLINE_NODEBUG bool hasChunkWithSize(size_t size) const noexcept {
    // Always called after `hasChunk()` with the advertized size of the chunk, so we always have at least 12 bytes.
    BL_ASSERT(remainingBytes() >= kPngChunkBaseSize);

    return remainingBytes() - kPngChunkBaseSize >= size;
  }

  BL_INLINE uint32_t readChunkSize() const noexcept {
    BL_ASSERT(hasChunk());
    return MemOps::readU32uBE(ptr + 0);
  }

  BL_INLINE uint32_t readChunkTag() const noexcept {
    BL_ASSERT(hasChunk());
    return MemOps::readU32uBE(ptr + 4);
  }

  BL_INLINE uint32_t readUInt8(size_t offset) const noexcept {
    BL_ASSERT(offset + 1u <= remainingBytes());
    return MemOps::readU8(ptr + offset);
  }

  BL_INLINE uint32_t readUInt16(size_t offset) const noexcept {
    BL_ASSERT(offset + 2u <= remainingBytes());
    return MemOps::readU16uBE(ptr + offset);
  }

  BL_INLINE uint32_t readUInt32(size_t offset) const noexcept {
    BL_ASSERT(offset + 4u <= remainingBytes());
    return MemOps::readU32uBE(ptr + offset);
  }
};

} // {anonymous}

// bl::Png::Codec - Decoder - API
// ==============================

static BLResult BL_CDECL decoderRestartImpl(BLImageDecoderImpl* impl) noexcept {
  BLPngDecoderImpl* decoderI = static_cast<BLPngDecoderImpl*>(impl);

  decoderI->lastResult = BL_SUCCESS;
  decoderI->frameIndex = 0;
  decoderI->bufferIndex = 0;

  decoderI->imageInfo.reset();
  decoderI->statusFlags = DecoderStatusFlags::kNone;
  decoderI->colorType = 0;
  decoderI->sampleDepth = 0;
  decoderI->sampleCount = 0;
  decoderI->outputFormat = uint8_t(BL_FORMAT_NONE);
  decoderI->colorKey.reset();
  decoderI->paletteSize = 0;
  decoderI->firstFCTLOffset = 0;
  decoderI->prevCtrl = FCTL{};
  decoderI->frameCtrl = FCTL{};

  return BL_SUCCESS;
}

static BLResult decoderReadFCTL(BLPngDecoderImpl* decoderI, size_t chunkOffset, BLArrayView<uint8_t> chunk) noexcept {
  if (BL_UNLIKELY(chunk.size < kPngChunkDataSize_fcTL)) {
    return blTraceError(BL_ERROR_INVALID_DATA);
  }

  uint32_t n = MemOps::readU32uBE(chunk.data + 0u);
  uint32_t w = MemOps::readU32uBE(chunk.data + 4u);
  uint32_t h = MemOps::readU32uBE(chunk.data + 8u);
  uint32_t x = MemOps::readU32uBE(chunk.data + 12u);
  uint32_t y = MemOps::readU32uBE(chunk.data + 16u);
  uint32_t delayNum = MemOps::readU16uBE(chunk.data + 20u);
  uint32_t delayDen = MemOps::readU16uBE(chunk.data + 22u);
  uint32_t disposeOp = MemOps::readU8(chunk.data + 24u);
  uint32_t blendOp = MemOps::readU8(chunk.data + 25u);

  if (BL_UNLIKELY(x >= uint32_t(decoderI->imageInfo.size.w) ||
                  y >= uint32_t(decoderI->imageInfo.size.h) ||
                  w > uint32_t(decoderI->imageInfo.size.w) - x ||
                  h > uint32_t(decoderI->imageInfo.size.h) - y ||
                  disposeOp > kAPNGDisposeOpMaxValue ||
                  blendOp > kAPNGBlendOpMaxValue)) {
    return blTraceError(BL_ERROR_INVALID_DATA);
  }

  if (decoderI->firstFCTLOffset == 0) {
    decoderI->firstFCTLOffset = chunkOffset;
  }

  decoderI->prevCtrl = decoderI->frameCtrl;
  decoderI->frameCtrl.sequenceNumber = n;
  decoderI->frameCtrl.w = w;
  decoderI->frameCtrl.h = h;
  decoderI->frameCtrl.x = x;
  decoderI->frameCtrl.y = y;
  decoderI->frameCtrl.delayNum = uint16_t(delayNum);
  decoderI->frameCtrl.delayDen = uint16_t(delayDen);
  decoderI->frameCtrl.disposeOp = uint8_t(disposeOp);
  decoderI->frameCtrl.blendOp = uint8_t(blendOp);
  decoderI->addFlag(DecoderStatusFlags::kRead_fcTL);

  return BL_SUCCESS;
}

static BLResult decoderReadInfoInternal(BLPngDecoderImpl* decoderI, const uint8_t* p, size_t size) noexcept {
  const size_t kMinSize_PNG = kPngSignatureSize + kPngChunkBaseSize + kPngChunkDataSize_IHDR;
  const size_t kMinSize_CgBI = kMinSize_PNG + kPngChunkBaseSize + kPngChunkDataSize_CgBI;

  if (BL_UNLIKELY(size < kMinSize_PNG)) {
    return blTraceError(BL_ERROR_DATA_TRUNCATED);
  }

  // Check PNG signature.
  if (BL_UNLIKELY(memcmp(p, kPngSignature, kPngSignatureSize) != 0)) {
    return blTraceError(BL_ERROR_INVALID_SIGNATURE);
  }

  ChunkReader chunkReader(p + kPngSignatureSize, p + size - kPngSignatureSize);

  // Already verified by `kMinSize_PNG` check - so it must be true.
  BL_ASSERT(chunkReader.hasChunk());

  // Expect 'IHDR' or 'CgBI' chunk.
  uint32_t chunkTag = chunkReader.readChunkTag();
  uint32_t chunkSize = chunkReader.readChunkSize();

  // Read 'CgBI' Chunk (4 Bytes)
  // ---------------------------

  // Support "CgBI" aka "CoreGraphicsBrokenImage" - a violation of the PNG Spec:
  //   1. http://www.jongware.com/pngdefry.html
  //   2. http://iphonedevwiki.net/index.php/CgBI_file_format
  if (chunkTag == BL_MAKE_TAG('C', 'g', 'B', 'I')) {
    if (BL_UNLIKELY(chunkSize != kPngChunkDataSize_CgBI)) {
      return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
    }

    if (BL_UNLIKELY(size < kMinSize_CgBI)) {
      return blTraceError(BL_ERROR_DATA_TRUNCATED);
    }

    decoderI->addFlag(DecoderStatusFlags::kRead_CgBI);

    // Skip "CgBI" chunk and read the next chunk tag/size, which must be 'IHDR'.
    chunkReader.advance(kPngChunkBaseSize + kPngChunkDataSize_CgBI);

    chunkTag = chunkReader.readChunkTag();
    chunkSize = chunkReader.readChunkSize();
  }

  // Read 'IHDR' Chunk (13 Bytes)
  // ----------------------------

  if (chunkTag != BL_MAKE_TAG('I', 'H', 'D', 'R') || chunkSize != kPngChunkDataSize_IHDR) {
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
  }

  uint32_t w           = chunkReader.readUInt32(kPngChunkHeaderSize + 0u);
  uint32_t h           = chunkReader.readUInt32(kPngChunkHeaderSize + 4u);
  uint32_t sampleDepth = chunkReader.readUInt8(kPngChunkHeaderSize + 8u);
  uint32_t colorType   = chunkReader.readUInt8(kPngChunkHeaderSize + 9u);
  uint32_t compression = chunkReader.readUInt8(kPngChunkHeaderSize + 10u);
  uint32_t filter      = chunkReader.readUInt8(kPngChunkHeaderSize + 11u);
  uint32_t progressive = chunkReader.readUInt8(kPngChunkHeaderSize + 12u);

  chunkReader.advance(kPngChunkBaseSize + kPngChunkDataSize_IHDR);

  // Width/Height can't be zero or greater than `2^31 - 1`.
  if (BL_UNLIKELY(w == 0 || h == 0)) {
    return blTraceError(BL_ERROR_INVALID_DATA);
  }

  if (BL_UNLIKELY(w >= 0x80000000u || h >= 0x80000000u)) {
    return blTraceError(BL_ERROR_IMAGE_TOO_LARGE);
  }

  if (BL_UNLIKELY(!checkColorTypeAndBitDepth(colorType, sampleDepth))) {
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
  }

  // Compression and filter has to be zero, progressive can be [0, 1].
  if (BL_UNLIKELY(compression != 0 || filter != 0 || progressive >= 2)) {
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
  }

  // Setup the image information.
  decoderI->addFlag(DecoderStatusFlags::kRead_IHDR);
  decoderI->colorType = uint8_t(colorType);
  decoderI->sampleDepth = uint8_t(sampleDepth);
  decoderI->sampleCount = kColorTypeToSampleCountTable[colorType];

  decoderI->imageInfo.size.reset(int(w), int(h));
  decoderI->imageInfo.depth = uint16_t(sampleDepth * uint32_t(decoderI->sampleCount));
  decoderI->imageInfo.frameCount = 1;
  decoderI->imageInfo.flags = progressive ? BL_IMAGE_INFO_FLAG_PROGRESSIVE : BL_IMAGE_INFO_FLAG_NO_FLAGS;

  memcpy(decoderI->imageInfo.format, "PNG", 4);
  memcpy(decoderI->imageInfo.compression, "DEFLATE", 8);

  BLFormat outputFormat = colorType == kColorType2_RGB ? BL_FORMAT_XRGB32 : BL_FORMAT_PRGB32;

  decoderI->outputFormat = uint8_t(outputFormat);
  decoderI->bufferIndex = PtrOps::byteOffset(p, chunkReader.ptr);

  // Read Extra Chunks to Detect APNG
  // --------------------------------

  while (chunkReader.hasChunk()) {
    chunkTag = chunkReader.readChunkTag();
    chunkSize = chunkReader.readChunkSize();

    if (BL_UNLIKELY(!chunkReader.hasChunkWithSize(chunkSize))) {
      break;
    }

    if (chunkTag == BL_MAKE_TAG('a', 'c', 'T', 'L')) {
      // Animated PNG chunk.
      if (chunkSize != kPngChunkDataSize_acTL) {
        // Don't refuse the file, but don't mark it as APNG (we would just treat it as a regular PNG if 'acTL' is broken).
        break;
      }

      uint32_t frameCount = chunkReader.readUInt32(kPngChunkHeaderSize + 0u);
      uint32_t repeatCount = chunkReader.readUInt32(kPngChunkHeaderSize + 4u);

      if (frameCount <= 1) {
        break;
      }

      decoderI->imageInfo.frameCount = frameCount;
      decoderI->imageInfo.repeatCount = repeatCount;
      memcpy(decoderI->imageInfo.format, "APNG", 5);
      decoderI->addFlag(DecoderStatusFlags::kRead_acTL);
      break;
    }

    if ((chunkTag == BL_MAKE_TAG('I', 'H', 'D', 'R')) ||
        (chunkTag == BL_MAKE_TAG('P', 'L', 'T', 'E')) ||
        (chunkTag == BL_MAKE_TAG('I', 'D', 'A', 'T')) ||
        (chunkTag == BL_MAKE_TAG('I', 'E', 'N', 'D'))) {
      break;
    }

    chunkReader.advance(size_t(kPngChunkBaseSize) + size_t(chunkSize));
  }

  return BL_SUCCESS;
}

// Reads initial chunks and stops at the beginning of pixel data ('IDAT' and 'fdAT') or 'IEND'.
static BLResult decoderReadImportantChunks(BLPngDecoderImpl* decoderI, const uint8_t* p, size_t size) noexcept {
  // Don't read beyond the user provided buffer.
  if (BL_UNLIKELY(size < decoderI->bufferIndex)) {
    return blTraceError(BL_ERROR_INVALID_STATE);
  }

  ChunkReader chunkReader(p + decoderI->bufferIndex, p + size);
  for (;;) {
    if (BL_UNLIKELY(!chunkReader.hasChunk())) {
      return blTraceError(BL_ERROR_DATA_TRUNCATED);
    }

    uint32_t chunkTag = chunkReader.readChunkTag();
    uint32_t chunkSize = chunkReader.readChunkSize();

    if (BL_UNLIKELY(!chunkReader.hasChunkWithSize(chunkSize))) {
      return blTraceError(BL_ERROR_DATA_TRUNCATED);
    }

    if (chunkTag == BL_MAKE_TAG('P', 'L', 'T', 'E')) {
      // Read 'PLTE' Chunk (Once)
      // ------------------------

      // 1. There must not be more than one PLTE chunk.
      // 2. It must precede the first IDAT chunk (also tRNS chunk).
      // 3. Contains 1...256 RGB palette entries.
      if (decoderI->hasFlag(DecoderStatusFlags::kRead_PLTE | DecoderStatusFlags::kRead_tRNS)) {
        return blTraceError(BL_ERROR_PNG_INVALID_PLTE);
      }

      if (chunkSize == 0 || chunkSize > 768 || (chunkSize % 3) != 0) {
        return blTraceError(BL_ERROR_PNG_INVALID_PLTE);
      }

      chunkReader.advanceChunkHeader();

      uint32_t i = 0u;
      uint32_t paletteSize = chunkSize / 3;

      decoderI->addFlag(DecoderStatusFlags::kRead_PLTE);
      decoderI->paletteSize = paletteSize;

      while (i < paletteSize) {
        decoderI->paletteData[i++] = BLRgba32(chunkReader.readUInt8(0u),
                                              chunkReader.readUInt8(1u),
                                              chunkReader.readUInt8(2u));
        chunkReader.advance(3);
      }

      while (i < 256u) {
        decoderI->paletteData[i++] = BLRgba32(0x00, 0x00, 0x00, 0xFF);
      }

      chunkReader.advance(4u); // CRC32
    }
    else if (chunkTag == BL_MAKE_TAG('t', 'R', 'N', 'S')) {
      // Read 'tRNS' Chunk (Once)
      // ------------------------

      uint32_t colorType = decoderI->colorType;

      // 1. There must not be more than one 'tRNS' chunk.
      // 2. It must precede the first 'IDAT' chunk and follow a 'PLTE' chunk, if any.
      // 3. It is prohibited for color types 4 and 6.
      if (decoderI->hasFlag(DecoderStatusFlags::kRead_tRNS)) {
        return blTraceError(BL_ERROR_PNG_INVALID_TRNS);
      }

      if (colorType == kColorType4_LUMA || colorType == kColorType6_RGBA) {
        return blTraceError(BL_ERROR_PNG_INVALID_TRNS);
      }

      if (colorType == kColorType0_LUM) {
        // For color type 0 (grayscale), the tRNS chunk contains a single gray level value, stored in the format:
        //   [0..1] Gray:  2 bytes, range 0 .. (2^depth)-1
        if (chunkSize != 2u) {
          return blTraceError(BL_ERROR_PNG_INVALID_TRNS);
        }

        uint32_t gray = chunkReader.readUInt16(kPngChunkHeaderSize);
        decoderI->colorKey.reset(gray, gray, gray, 0u);
        decoderI->addFlag(DecoderStatusFlags::kHasColorKey);

        chunkReader.advance(kPngChunkBaseSize + 2u);
      }
      else if (colorType == kColorType2_RGB) {
        // For color type 2 (truecolor), the tRNS chunk contains a single RGB color value, stored in the format:
        //   [0..1] Red:   2 bytes, range 0 .. (2^depth)-1
        //   [2..3] Green: 2 bytes, range 0 .. (2^depth)-1
        //   [4..5] Blue:  2 bytes, range 0 .. (2^depth)-1
        if (chunkSize != 6u) {
          return blTraceError(BL_ERROR_PNG_INVALID_TRNS);
        }

        uint32_t r = chunkReader.readUInt16(kPngChunkHeaderSize + 0u);
        uint32_t g = chunkReader.readUInt16(kPngChunkHeaderSize + 2u);
        uint32_t b = chunkReader.readUInt16(kPngChunkHeaderSize + 4u);

        decoderI->colorKey.reset(r, g, b, 0u);
        decoderI->addFlag(DecoderStatusFlags::kHasColorKey);

        chunkReader.advance(kPngChunkBaseSize + 6u);
      }
      else {
        // For color type 3 (indexed color), the tRNS chunk contains a series of one-byte alpha values, corresponding
        // to entries in the PLTE chunk.
        BL_ASSERT(colorType == kColorType3_PAL);

        // 1. Has to follow PLTE if color type is 3.
        // 2. The tRNS chunk can contain 1...palSize alpha values, but in general it can contain less than `palSize`
        //    values, in that case the remaining alpha values are assumed to be 255.
        if (!decoderI->hasFlag(DecoderStatusFlags::kRead_PLTE) || chunkSize == 0u || chunkSize > decoderI->paletteSize) {
          return blTraceError(BL_ERROR_PNG_INVALID_TRNS);
        }

        chunkReader.advanceChunkHeader();

        for (uint32_t i = 0; i < chunkSize; i++) {
          decoderI->paletteData[i].setA(chunkReader.readUInt8(i));
        }

        chunkReader.advance(chunkSize + 4u);
      }

      decoderI->addFlag(DecoderStatusFlags::kRead_tRNS);
    }
    else if (chunkTag == BL_MAKE_TAG('I', 'H', 'D', 'R') ||
             chunkTag == BL_MAKE_TAG('I', 'D', 'A', 'T') ||
             chunkTag == BL_MAKE_TAG('I', 'E', 'N', 'D') ||
             chunkTag == BL_MAKE_TAG('f', 'c', 'T', 'L')) {
      // Stop - these will be read by a different function.
      break;
    }
    else {
      if (chunkTag == BL_MAKE_TAG('f', 'c', 'T', 'L') && decoderI->isAPNG()) {
        if (decoderI->hasFCTL()) {
          return blTraceError(BL_ERROR_INVALID_DATA);
        }

        BL_PROPAGATE(
          decoderReadFCTL(
            decoderI,
            PtrOps::byteOffset(p, chunkReader.ptr),
            BLArrayView<uint8_t>{chunkReader.ptr + kPngChunkHeaderSize, chunkSize}));
      }

      // Skip unknown or known, but unsupported chunks.
      chunkReader.advance(kPngChunkBaseSize + chunkSize);
    }
  }

  // Create a pixel converter capable of converting PNG pixel data to BLImage pixel data.
  BLFormatInfo pngFmt {};
  pngFmt.depth = decoderI->sampleDepth;

  if (BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_LE) {
    pngFmt.addFlags(BL_FORMAT_FLAG_BYTE_SWAP);
  }

  if (decoderI->colorType == kColorType0_LUM && decoderI->sampleDepth <= 8) {
    // Treat grayscale images up to 8bpp as indexed and create a dummy palette.
    createGrayscalePalette(decoderI->paletteData, decoderI->sampleDepth);

    // Handle color-key properly.
    if (decoderI->hasColorKey() && decoderI->colorKey.r() < (1u << decoderI->sampleDepth)) {
      decoderI->paletteData[decoderI->colorKey.r()] = BLRgba32(0u);
    }

    pngFmt.addFlags(BLFormatFlags(BL_FORMAT_FLAG_RGBA | BL_FORMAT_FLAG_INDEXED));
    pngFmt.palette = decoderI->paletteData;
  }
  else if (decoderI->colorType == kColorType3_PAL) {
    pngFmt.addFlags(BLFormatFlags(BL_FORMAT_FLAG_RGBA | BL_FORMAT_FLAG_INDEXED));
    pngFmt.palette = decoderI->paletteData;
  }
  else {
    pngFmt.depth *= decoderI->sampleCount;

    if (decoderI->colorType == kColorType0_LUM) {
      // TODO: [PNG] 16-BPC.
    }
    else if (decoderI->colorType == kColorType2_RGB) {
      pngFmt.addFlags(BL_FORMAT_FLAG_RGB);
      pngFmt.rSize = 8; pngFmt.rShift = 16;
      pngFmt.gSize = 8; pngFmt.gShift = 8;
      pngFmt.bSize = 8; pngFmt.bShift = 0;
    }
    else if (decoderI->colorType == kColorType4_LUMA) {
      pngFmt.addFlags(BL_FORMAT_FLAG_LUMA);
      pngFmt.rSize = 8; pngFmt.rShift = 8;
      pngFmt.gSize = 8; pngFmt.gShift = 8;
      pngFmt.bSize = 8; pngFmt.bShift = 8;
      pngFmt.aSize = 8; pngFmt.aShift = 0;
    }
    else if (decoderI->colorType == kColorType6_RGBA) {
      pngFmt.addFlags(BL_FORMAT_FLAG_RGBA);
      pngFmt.rSize = 8; pngFmt.rShift = 24;
      pngFmt.gSize = 8; pngFmt.gShift = 16;
      pngFmt.bSize = 8; pngFmt.bShift = 8;
      pngFmt.aSize = 8; pngFmt.aShift = 0;
    }

    if (decoderI->isCGBI()) {
      BLInternal::swap(pngFmt.rShift, pngFmt.bShift);
      if (pngFmt.hasFlag(BL_FORMAT_FLAG_ALPHA)) {
        pngFmt.addFlags(BL_FORMAT_FLAG_PREMULTIPLIED);
      }
    }
  }

  BL_PROPAGATE(decoderI->pixelConverter.create(blFormatInfo[decoderI->outputFormat], pngFmt,
    BLPixelConverterCreateFlags(
      BL_PIXEL_CONVERTER_CREATE_FLAG_DONT_COPY_PALETTE |
      BL_PIXEL_CONVERTER_CREATE_FLAG_ALTERABLE_PALETTE)));

  decoderI->bufferIndex = PtrOps::byteOffset(p, chunkReader.ptr);
  return BL_SUCCESS;
}

static void copyPixels(uint8_t* dstData, intptr_t dstStride, const uint8_t* srcData, intptr_t srcStride, size_t w, uint32_t h) noexcept {
  for (uint32_t i = 0; i < h; i++) {
    memcpy(dstData, srcData, w);
    dstData += dstStride;
    srcData += srcStride;
  }
}

static void zeroPixels(uint8_t* dstData, intptr_t dstStride, size_t w, uint32_t h) noexcept {
  for (uint32_t i = 0; i < h; i++) {
    memset(dstData, 0, w);
    dstData += dstStride;
  }
}

static BLResult decoderReadPixelData(BLPngDecoderImpl* decoderI, BLImage* imageOut, const uint8_t* input, size_t size) noexcept {
  // Number of bytes to overallocate so the DEFLATE decoder doesn't have to run the slow loop at the end.
  constexpr uint32_t kOutputSizeScratch = 1024u;

  // Make sure we won't initialize our chunk reader out of range.
  if (BL_UNLIKELY(size < decoderI->bufferIndex)) {
    return blTraceError(BL_ERROR_INVALID_STATE);
  }

  ChunkReader chunkReader(input + decoderI->bufferIndex, input + size);

  uint32_t x = 0u;
  uint32_t y = 0u;
  uint32_t w = uint32_t(decoderI->imageInfo.size.w);
  uint32_t h = uint32_t(decoderI->imageInfo.size.h);

  // Advance Chunks
  // --------------

  uint32_t frameTag =
    (decoderI->frameIndex == 0u)
      ? BL_MAKE_TAG('I', 'D', 'A', 'T')
      : BL_MAKE_TAG('f', 'd', 'A', 'T');

  // Process all preceding chunks, which are not 'IDAT' or 'fdAT'.
  for (;;) {
    if (BL_UNLIKELY(!chunkReader.hasChunk())) {
      return blTraceError(BL_ERROR_DATA_TRUNCATED);
    }

    uint32_t chunkTag = chunkReader.readChunkTag();
    uint32_t chunkSize = chunkReader.readChunkSize();

    if (BL_UNLIKELY(!chunkReader.hasChunkWithSize(chunkSize))) {
      return blTraceError(BL_ERROR_DATA_TRUNCATED);
    }

    if (chunkTag == frameTag) {
      // Found a frame chunk.
      break;
    }

    if (chunkTag == BL_MAKE_TAG('I', 'H', 'D', 'R')) {
      return blTraceError(BL_ERROR_PNG_MULTIPLE_IHDR);
    }

    if (chunkTag == BL_MAKE_TAG('I', 'E', 'N', 'D')) {
      return blTraceError(BL_ERROR_PNG_INVALID_IEND);
    }

    if (chunkTag == BL_MAKE_TAG('f', 'c', 'T', 'L') && decoderI->isAPNG()) {
      if (decoderI->hasFCTL()) {
        return blTraceError(BL_ERROR_INVALID_DATA);
      }
      BL_PROPAGATE(
        decoderReadFCTL(
          decoderI,
          PtrOps::byteOffset(input, chunkReader.ptr),
          BLArrayView<uint8_t>{chunkReader.ptr + 8u, chunkSize}));
    }

    chunkReader.advance(size_t(kPngChunkBaseSize) + chunkSize);
  }

  // Handle APNG Frame Window
  // ------------------------

  if (decoderI->frameIndex != 0u) {
    if (!decoderI->hasFCTL()) {
      return blTraceError(BL_ERROR_INVALID_DATA);
    }

    x = decoderI->frameCtrl.x;
    y = decoderI->frameCtrl.y;
    w = decoderI->frameCtrl.w;
    h = decoderI->frameCtrl.h;
  }

  // Decode Pixel Data (DEFLATE)
  // ---------------------------

  uint32_t sampleDepth = decoderI->sampleDepth;
  uint32_t sampleCount = decoderI->sampleCount;

  uint32_t progressive = (decoderI->imageInfo.flags & BL_IMAGE_INFO_FLAG_PROGRESSIVE) != 0;
  uint32_t stepCount = progressive ? 7 : 1;

  InterlaceStep steps[7];
  uint32_t pngPixelDataSize = calculateInterlaceSteps(steps, progressive ? interlaceTableAdam7 : interlaceTableNone, stepCount, sampleDepth, sampleCount, w, h);

  if (BL_UNLIKELY(pngPixelDataSize == 0)) {
    return blTraceError(BL_ERROR_INVALID_DATA);
  }

  BL_PROPAGATE(decoderI->deflateDecoder.init(decoderI->deflateFormat(), Compression::Deflate::DecoderOptions::kNeverReallocOutputBuffer));
  BL_PROPAGATE(decoderI->pngPixelData.clear());
  BL_PROPAGATE(decoderI->pngPixelData.reserve(size_t(pngPixelDataSize) + kOutputSizeScratch));

  // Read 'IDAT' or 'fdAT' chunks - once the first chunk is found, it's either the only chunk or there are consecutive
  // chunks of the same type. It's not allowed that the chunks are interleaved with chunks of a different chunk tag.
  {
    uint32_t chunkSize = chunkReader.readChunkSize();
    // Was already checked...
    BL_ASSERT(chunkReader.hasChunkWithSize(chunkSize));

    for (;;) {
      // Zero chunks are allowed, however, they don't contain any data, thus don't call the DEFLATE decoder with these.
      const uint8_t* chunkData = chunkReader.ptr + kPngChunkHeaderSize;
      chunkReader.advance(size_t(kPngChunkBaseSize) + chunkSize);

      if (frameTag == BL_MAKE_TAG('f', 'd', 'A', 'T')) {
        // The 'fdAT' chunk starts with 4 bytes specifying the sequence.
        if (BL_UNLIKELY(chunkSize < 4u)) {
          return blTraceError(BL_ERROR_INVALID_DATA);
        }

        chunkData += 4u;
        chunkSize -= 4u;
      }

      if (chunkSize > 0u) {
        // When the decompression is done, verify whether the decompressed data size matches the PNG pixel data size.
        BLResult result = decoderI->deflateDecoder.decode(decoderI->pngPixelData, BLDataView{chunkData, chunkSize});
        if (result == BL_SUCCESS) {
          if (decoderI->pngPixelData.size() != pngPixelDataSize) {
            return blTraceError(BL_ERROR_INVALID_DATA);
          }
          break;
        }

        // The decoder returns this error (which is not traced) in case that the input data was not enough to
        // decompress the data. It's not an error if more pixel data chunks follow.
        if (result != BL_ERROR_DATA_TRUNCATED) {
          return result;
        }
      }

      // Consecutive chunks required.
      if (BL_UNLIKELY(!chunkReader.hasChunk())) {
        return blTraceError(BL_ERROR_DATA_TRUNCATED);
      }

      chunkSize = chunkReader.readChunkSize();
      if (BL_UNLIKELY(!chunkReader.hasChunkWithSize(chunkSize))) {
        return blTraceError(BL_ERROR_DATA_TRUNCATED);
      }

      uint32_t chunkTag = chunkReader.readChunkTag();
      if (BL_UNLIKELY(chunkTag != frameTag)) {
        return blTraceError(BL_ERROR_INVALID_DATA);
      }
    }
  }

  decoderI->clearFlag(DecoderStatusFlags::kRead_fcTL);
  decoderI->bufferIndex = PtrOps::byteOffset(input, chunkReader.ptr);

  uint8_t* pngPixelPtr = const_cast<uint8_t*>(decoderI->pngPixelData.data());
  uint32_t bytesPerPixel = blMax<uint32_t>((sampleDepth * sampleCount) / 8, 1);

  // Apply Inverse Filter
  // --------------------

  // If progressive `stepCount` is 7 and `steps` array contains all windows.
  for (uint32_t i = 0; i < stepCount; i++) {
    InterlaceStep& step = steps[i];
    if (step.used) {
      BL_PROPAGATE(Ops::funcTable.inverseFilter[bytesPerPixel](pngPixelPtr + step.offset, bytesPerPixel, step.bpl, step.height));
    }
  }

  // Deinterlace & Copy/Blend
  // ------------------------

  BLImageData imageData;

  if (decoderI->frameIndex == 0u) {
    BL_PROPAGATE(imageOut->create(int(w), int(h), BLFormat(decoderI->outputFormat)));
  }
  else {
    // The animation requires that the user passes an image that has the previous content, but we only want to verify
    // its size and pixel format.
    if (BL_UNLIKELY(imageOut->size() != decoderI->imageInfo.size || imageOut->format() != BLFormat(decoderI->outputFormat))) {
      return blTraceError(BL_ERROR_INVALID_STATE);
    }
  }

  BL_PROPAGATE(imageOut->makeMutable(&imageData));

  intptr_t dstStride = imageData.stride;
  uint8_t* dstPixels = static_cast<uint8_t*>(imageData.pixelData);

  if (decoderI->frameIndex != 0u) {
    size_t bpp = imageOut->depth() / 8u;
    size_t prevAreaWidthInBytes = decoderI->prevCtrl.w * bpp;

    switch (decoderI->prevCtrl.disposeOp) {
      case kAPNGDisposeOpBackground: {
        zeroPixels(
          dstPixels + intptr_t(decoderI->prevCtrl.y) * dstStride + intptr_t(decoderI->prevCtrl.x * bpp),
          dstStride,
          prevAreaWidthInBytes,
          decoderI->prevCtrl.h);
        break;
      }

      case kAPNGDisposeOpPrevious: {
        const uint8_t* savedPixels = static_cast<const uint8_t*>(decoderI->previousPixelBuffer.get());

        copyPixels(
          dstPixels + intptr_t(decoderI->prevCtrl.y) * dstStride + intptr_t(decoderI->prevCtrl.x * bpp),
          dstStride,
          savedPixels,
          intptr_t(prevAreaWidthInBytes),
          prevAreaWidthInBytes,
          decoderI->prevCtrl.h);
        break;
      }

      default: {
        // Do nothing if the dispose op is kAPNGDisposeOpNone.
        break;
      }
    }

    dstPixels += intptr_t(y) * dstStride + intptr_t(x * bpp);

    if (decoderI->frameCtrl.disposeOp == kAPNGDisposeOpPrevious) {
      size_t copyAreaWidthInBytes = w * bpp;
      uint8_t* savedPixels = static_cast<uint8_t*>(decoderI->previousPixelBuffer.alloc(h * copyAreaWidthInBytes));

      if (BL_UNLIKELY(!savedPixels)) {
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);
      }

      copyPixels(savedPixels, intptr_t(copyAreaWidthInBytes), dstPixels, dstStride, copyAreaWidthInBytes, h);
    }

    // TODO: [APNG] kAPNGBlendOpOver is currently not supported.
    //
    // if (decoderI->frameCtrl.blendOp == kBlendOpOver) {
    // }
  }

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
    uint32_t tmpHeight = blMin<uint32_t>((h + 1u) / 2u, 4u);
    uint32_t tmpBpl = steps[6].bpl;
    uint32_t tmpSize;

    if (steps[6].height) {
      decoderI->pixelConverter.convertRect(dstPixels + dstStride, dstStride * 2, pngPixelPtr + 1u + steps[6].offset, intptr_t(tmpBpl), w, steps[6].height);
    }

    // Align `tmpBpl` so we can use aligned memory writes and reads while using it.
    tmpBpl = IntOps::alignUp(tmpBpl, 16);
    tmpSize = tmpBpl * tmpHeight;

    ScopedBuffer tmpAlloc;
    uint8_t* tmpPixelPtr;

    // Decide whether to alloc an extra buffer or to reuse 7th.
    if (steps[6].size < tmpSize + 15) {
      tmpPixelPtr = static_cast<uint8_t*>(tmpAlloc.alloc(tmpSize + 15));
    }
    else {
      tmpPixelPtr = pngPixelPtr + steps[6].offset;
    }

    if (!tmpPixelPtr) {
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
    }

    tmpPixelPtr = IntOps::alignUp(tmpPixelPtr, 16);
    switch (depth) {
      case 1 : deinterlaceBits<1>(dstPixels, dstStride, decoderI->pixelConverter, tmpPixelPtr, intptr_t(tmpBpl), pngPixelPtr, steps, w, h); break;
      case 2 : deinterlaceBits<2>(dstPixels, dstStride, decoderI->pixelConverter, tmpPixelPtr, intptr_t(tmpBpl), pngPixelPtr, steps, w, h); break;
      case 4 : deinterlaceBits<4>(dstPixels, dstStride, decoderI->pixelConverter, tmpPixelPtr, intptr_t(tmpBpl), pngPixelPtr, steps, w, h); break;
      case 8 : deinterlaceBytes<1>(dstPixels, dstStride, decoderI->pixelConverter, tmpPixelPtr, intptr_t(tmpBpl), pngPixelPtr, steps, w, h); break;
      case 16: deinterlaceBytes<2>(dstPixels, dstStride, decoderI->pixelConverter, tmpPixelPtr, intptr_t(tmpBpl), pngPixelPtr, steps, w, h); break;
      case 24: deinterlaceBytes<3>(dstPixels, dstStride, decoderI->pixelConverter, tmpPixelPtr, intptr_t(tmpBpl), pngPixelPtr, steps, w, h); break;
      case 32: deinterlaceBytes<4>(dstPixels, dstStride, decoderI->pixelConverter, tmpPixelPtr, intptr_t(tmpBpl), pngPixelPtr, steps, w, h); break;
    }
  }
  else {
    BL_ASSERT(steps[0].width == w);
    BL_ASSERT(steps[0].height == h);

    decoderI->pixelConverter.convertRect(dstPixels, dstStride, pngPixelPtr + 1, intptr_t(steps[0].bpl), w, h);
  }

  decoderI->frameIndex++;
  if (decoderI->isAPNG() && decoderI->frameIndex >= decoderI->imageInfo.frameCount) {
    // Restart the animation to create a loop.
    decoderI->frameIndex = 0;
    decoderI->bufferIndex = decoderI->firstFCTLOffset;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL decoderReadInfoImpl(BLImageDecoderImpl* impl, BLImageInfo* infoOut, const uint8_t* data, size_t size) noexcept {
  BLPngDecoderImpl* decoderI = static_cast<BLPngDecoderImpl*>(impl);
  BLResult result = decoderI->lastResult;

  if (decoderI->bufferIndex == 0u && result == BL_SUCCESS) {
    result = decoderReadInfoInternal(decoderI, data, size);
    if (result != BL_SUCCESS) {
      decoderI->lastResult = result;
    }
  }

  if (infoOut) {
    memcpy(infoOut, &decoderI->imageInfo, sizeof(BLImageInfo));
  }

  return result;
}

static BLResult BL_CDECL decoderReadFrameImpl(BLImageDecoderImpl* impl, BLImageCore* imageOut, const uint8_t* data, size_t size) noexcept {
  BLPngDecoderImpl* decoderI = static_cast<BLPngDecoderImpl*>(impl);
  BL_PROPAGATE(decoderReadInfoImpl(decoderI, nullptr, data, size));

  if (decoderI->frameIndex == 0u && decoderI->firstFCTLOffset == 0u) {
    BLResult result = decoderReadImportantChunks(decoderI, data, size);
    if (result != BL_SUCCESS) {
      decoderI->lastResult = result;
      return result;
    }
  }
  else if (!decoderI->isAPNG()) {
    return blTraceError(BL_ERROR_NO_MORE_DATA);
  }

  {
    BLResult result = decoderReadPixelData(decoderI, static_cast<BLImage*>(imageOut), data, size);
    if (result != BL_SUCCESS) {
      decoderI->lastResult = result;
      return result;
    }
    return BL_SUCCESS;
  }
}

static BLResult BL_CDECL decoderCreateImpl(BLImageDecoderCore* self) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_IMAGE_DECODER);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLPngDecoderImpl>(self, info));

  BLPngDecoderImpl* decoderI = static_cast<BLPngDecoderImpl*>(self->_d.impl);
  decoderI->ctor(&pngDecoderVirt, &pngCodecInstance);
  return decoderRestartImpl(decoderI);
}

static BLResult BL_CDECL decoderDestroyImpl(BLObjectImpl* impl) noexcept {
  BLPngDecoderImpl* decoderI = static_cast<BLPngDecoderImpl*>(impl);
  decoderI->dtor();
  return blObjectFreeImpl(decoderI);
}

// bl::Png::Codec - Encoder - OutputBuffer
// =======================================

class OutputBuffer {
public:
  uint8_t* _data = nullptr;
  uint8_t* _ptr = nullptr;
  uint8_t* _end = nullptr;

  BL_INLINE OutputBuffer() noexcept {}

  BL_INLINE OutputBuffer(uint8_t* data, size_t size) noexcept
    : _data(data),
      _ptr(data),
      _end(data + size) {}

  BL_INLINE uint8_t* ptr() const noexcept { return _ptr; }
  BL_INLINE uint8_t* end() const noexcept { return _end; }

  BL_INLINE size_t index() const noexcept { return PtrOps::byteOffset(_data, _ptr); }
  BL_INLINE size_t capacity() const noexcept { return PtrOps::byteOffset(_data, _end); }
  BL_INLINE size_t remainingSize() const noexcept { return PtrOps::bytesUntil(_ptr, _end); }

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

    MemOps::writeU16u(_ptr, value);
    _ptr += 2;
  }

  BL_INLINE void appendUInt16LE(uint16_t value) noexcept {
    BL_ASSERT(remainingSize() >= 2);

    MemOps::writeU16uLE(_ptr, value);
    _ptr += 2;
  }

  BL_INLINE void appendUInt16BE(uint16_t value) noexcept {
    BL_ASSERT(remainingSize() >= 2);

    MemOps::writeU16uBE(_ptr, value);
    _ptr += 2;
  }

  BL_INLINE void appendUInt32(uint32_t value) noexcept {
    BL_ASSERT(remainingSize() >= 4);

    MemOps::writeU32u(_ptr, value);
    _ptr += 4;
  }

  BL_INLINE void appendUInt32LE(uint32_t value) noexcept {
    BL_ASSERT(remainingSize() >= 4);

    MemOps::writeU32uLE(_ptr, value);
    _ptr += 4;
  }

  BL_INLINE void appendUInt32BE(uint32_t value) noexcept {
    BL_ASSERT(remainingSize() >= 4);

    MemOps::writeU32uBE(_ptr, value);
    _ptr += 4;
  }

  BL_INLINE void appendUInt64(uint64_t value) noexcept {
    BL_ASSERT(remainingSize() >= 4);

    MemOps::writeU64u(_ptr, value);
    _ptr += 8;
  }

  BL_INLINE void appendUInt64LE(uint64_t value) noexcept {
    BL_ASSERT(remainingSize() >= 4);

    MemOps::writeU64uLE(_ptr, value);
    _ptr += 8;
  }

  BL_INLINE void appendUInt64BE(uint64_t value) noexcept {
    BL_ASSERT(remainingSize() >= 4);

    MemOps::writeU64uBE(_ptr, value);
    _ptr += 8;
  }

  BL_INLINE void appendData(const uint8_t* data, size_t size) noexcept {
    BL_ASSERT(remainingSize() >= size);

    memcpy(_ptr, data, size);
    _ptr += size;
  }
};

// bl::Png::Codec - Encoder - ChunkWriter
// ======================================

class ChunkWriter {
public:
  uint8_t* chunkData = nullptr;

  BL_INLINE void start(OutputBuffer& output, uint32_t tag) noexcept {
    chunkData = output.ptr();
    output.appendUInt32BE(0);
    output.appendUInt32BE(tag);
  }

  BL_INLINE void done(OutputBuffer& output) noexcept {
    const uint8_t* start = chunkData + 8;
    size_t chunkLength = PtrOps::byteOffset(start, output.ptr());

    // PNG Specification: CRC is calculated on the preceding bytes in the chunk, including
    // the chunk type code and chunk data fields, but not including the length field.
    MemOps::writeU32uBE(chunkData, uint32_t(chunkLength));
    output.appendUInt32BE(Compression::Checksum::crc32(start - 4, chunkLength + 4));
  }
};

// bl::Png::Codec - Encoder - API
// ==============================

static BLResult BL_CDECL encoderRestartImpl(BLImageEncoderImpl* impl) noexcept {
  BLPngEncoderImpl* encoderI = static_cast<BLPngEncoderImpl*>(impl);

  encoderI->lastResult = BL_SUCCESS;
  encoderI->frameIndex = 0;
  encoderI->bufferIndex = 0;
  encoderI->compressionLevel = 6;

  return BL_SUCCESS;
}

static BLResult BL_CDECL encoderGetPropertyImpl(const BLObjectImpl* impl, const char* name, size_t nameSize, BLVarCore* valueOut) noexcept {
  const BLPngEncoderImpl* encoderI = static_cast<const BLPngEncoderImpl*>(impl);

  if (blMatchProperty(name, nameSize, "compression")) {
    return blVarAssignUInt64(valueOut, encoderI->compressionLevel);
  }

  return blObjectImplGetProperty(encoderI, name, nameSize, valueOut);
}

static BLResult BL_CDECL encoderSetPropertyImpl(BLObjectImpl* impl, const char* name, size_t nameSize, const BLVarCore* value) noexcept {
  BLPngEncoderImpl* encoderI = static_cast<BLPngEncoderImpl*>(impl);

  if (blMatchProperty(name, nameSize, "compression")) {
    uint64_t v;
    BL_PROPAGATE(blVarToUInt64(value, &v));
    encoderI->compressionLevel = uint8_t(blMin<uint64_t>(v, 12));
    return BL_SUCCESS;
  }

  return blObjectImplSetProperty(encoderI, name, nameSize, value);
}

static BLResult filterImageData(uint8_t* data, intptr_t stride, uint32_t bitsPerPixel, uint32_t w, uint32_t h) noexcept {
  blUnused(bitsPerPixel, w);

  for (uint32_t y = 0; y < h; y++) {
    data[0] = 0;
    data += stride;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL encoderWriteFrameImpl(BLImageEncoderImpl* impl, BLArrayCore* dst, const BLImageCore* image) noexcept {
  BLPngEncoderImpl* encoderI = static_cast<BLPngEncoderImpl*>(impl);
  BL_PROPAGATE(encoderI->lastResult);

  BLArray<uint8_t>& buf = *static_cast<BLArray<uint8_t>*>(dst);
  const BLImage& img = *static_cast<const BLImage*>(image);

  if (img.empty()) {
    return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  BLImageData imageData;
  BL_PROPAGATE(img.getData(&imageData));

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
      pngFormatInfo.flags = BLFormatFlags(BL_FORMAT_FLAG_RGBA | BL_FORMAT_FLAG_BE);
      pngFormatInfo.setSizes(8, 8, 8, 8);
      pngFormatInfo.setShifts(24, 16, 8, 0);
      pngBitDepth = 8;
      pngColorType = 6;
      break;

    case BL_FORMAT_XRGB32:
      pngFormatInfo.depth = 24;
      pngFormatInfo.flags = BLFormatFlags(BL_FORMAT_FLAG_RGB | BL_FORMAT_FLAG_BE);
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

  size_t uncompressedStride = ((w * pngFormatInfo.depth + 7) / 8u) + 1;
  size_t uncompressedDataSize = uncompressedStride * h;

  ScopedBuffer uncompressedBuffer;
  uint8_t* uncompressedData = static_cast<uint8_t*>(uncompressedBuffer.alloc(uncompressedDataSize));

  if (BL_UNLIKELY(!uncompressedData)) {
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  BL_PROPAGATE(pc.convertRect(uncompressedData + 1, intptr_t(uncompressedStride), imageData.pixelData, imageData.stride, w, h));
  BL_PROPAGATE(filterImageData(uncompressedData, intptr_t(uncompressedStride), pngFormatInfo.depth, w, h));

  // Setup a deflate encoder - higher compression levels require more space, so init it now.
  Compression::Deflate::Encoder deflateEncoder;
  BL_PROPAGATE(deflateEncoder.init(Compression::Deflate::FormatType::kZlib, encoderI->compressionLevel));

  // Create PNG file.
  size_t outputWorstCaseSize = deflateEncoder.minimumOutputBufferSize(uncompressedDataSize);

  size_t ihdrSize = kPngChunkBaseSize + kPngChunkDataSize_IHDR;
  size_t idatSize = kPngChunkBaseSize + outputWorstCaseSize;
  size_t iendSize = kPngChunkBaseSize;

  size_t reserveBytes = kPngSignatureSize + ihdrSize + idatSize + iendSize;
  uint8_t* outputData;
  BL_PROPAGATE(buf.modifyOp(BL_MODIFY_OP_APPEND_FIT, reserveBytes, &outputData));

  // Prepare output buffer and chunk writer.
  OutputBuffer output(outputData, reserveBytes);
  ChunkWriter chunk;

  // Write PNG signature.
  output.appendData(kPngSignature, kPngSignatureSize);

  // Write IHDR chunk.
  chunk.start(output, BL_MAKE_TAG('I', 'H', 'D', 'R'));
  output.appendUInt32BE(w);        // Image width.
  output.appendUInt32BE(h);        // Image height.
  output.appendByte(pngBitDepth);  // Bit depth (1, 2, 4, 8, 16).
  output.appendByte(pngColorType); // Color type (0, 2, 3, 4, 6).
  output.appendByte(0u);           // Compression method, must be zero.
  output.appendByte(0u);           // Filter method, must be zero.
  output.appendByte(0u);           // Interlace method (0 == no interlacing).
  chunk.done(output);

  // Write IDAT chunk.
  chunk.start(output, BL_MAKE_TAG('I', 'D', 'A', 'T'));
  output._ptr += deflateEncoder.compressTo(output.ptr(), output.remainingSize(), uncompressedData, uncompressedDataSize);
  chunk.done(output);

  // Write IEND chunk.
  chunk.start(output, BL_MAKE_TAG('I', 'E', 'N', 'D'));
  chunk.done(output);

  ArrayInternal::setSize(dst, PtrOps::byteOffset(buf.data(), output.ptr()));
  return BL_SUCCESS;
}

static BLResult encoderCreateImpl(BLImageEncoderCore* self) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_IMAGE_ENCODER);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLPngEncoderImpl>(self, info));

  BLPngEncoderImpl* encoderI = static_cast<BLPngEncoderImpl*>(self->_d.impl);
  encoderI->ctor(&pngEncoderVirt, &pngCodecInstance);
  return encoderRestartImpl(encoderI);
}

static BLResult BL_CDECL encoderDestroyImpl(BLObjectImpl* impl) noexcept {
  BLPngEncoderImpl* encoderI = static_cast<BLPngEncoderImpl*>(impl);
  encoderI->dtor();
  return blObjectFreeImpl(encoderI);
}

// bl::Png::Codec - Codec API
// ==========================

static BLResult BL_CDECL codecDestroyImpl(BLObjectImpl* impl) noexcept {
  // Built-in codecs are never destroyed.
  blUnused(impl);
  return BL_SUCCESS;
}

static uint32_t BL_CDECL codecInspectDataImpl(const BLImageCodecImpl* impl, const uint8_t* data, size_t size) noexcept {
  blUnused(impl);

  // Minimum PNG size and signature.
  if (size < kPngSignatureSize || memcmp(data, kPngSignature, kPngSignatureSize) != 0) {
    return 0;
  }

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

// bl::Png::Codec - Runtime Registration
// =====================================

void pngCodecOnInit(BLRuntimeContext* rt, BLArray<BLImageCodec>* codecs) noexcept {
  using namespace bl::Png;

  Ops::initFuncTable(rt);

  // Initialize PNG codec.
  pngCodec.virt.base.destroy = codecDestroyImpl;
  pngCodec.virt.base.getProperty = blObjectImplGetProperty;
  pngCodec.virt.base.setProperty = blObjectImplSetProperty;
  pngCodec.virt.inspectData = codecInspectDataImpl;
  pngCodec.virt.createDecoder = codecCreateDecoderImpl;
  pngCodec.virt.createEncoder = codecCreateEncoderImpl;

  pngCodec.impl->ctor(&pngCodec.virt);
  pngCodec.impl->features =
    BL_IMAGE_CODEC_FEATURE_READ     |
    BL_IMAGE_CODEC_FEATURE_WRITE    |
    BL_IMAGE_CODEC_FEATURE_LOSSLESS ;
  pngCodec.impl->name.dcast().assign("PNG");
  pngCodec.impl->vendor.dcast().assign("Blend2D");
  pngCodec.impl->mimeType.dcast().assign("image/png");
  pngCodec.impl->extensions.dcast().assign("png");

  pngCodecInstance._d.initDynamic(BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_IMAGE_CODEC), &pngCodec.impl);

  // Initialize PNG decoder virtual functions.
  pngDecoderVirt.base.destroy = decoderDestroyImpl;
  pngDecoderVirt.base.getProperty = blObjectImplGetProperty;
  pngDecoderVirt.base.setProperty = blObjectImplSetProperty;
  pngDecoderVirt.restart = decoderRestartImpl;
  pngDecoderVirt.readInfo = decoderReadInfoImpl;
  pngDecoderVirt.readFrame = decoderReadFrameImpl;

  // Initialize PNG encoder virtual functions.
  pngEncoderVirt.base.destroy = encoderDestroyImpl;
  pngEncoderVirt.base.getProperty = encoderGetPropertyImpl;
  pngEncoderVirt.base.setProperty = encoderSetPropertyImpl;
  pngEncoderVirt.restart = encoderRestartImpl;
  pngEncoderVirt.writeFrame = encoderWriteFrameImpl;

  codecs->append(pngCodecInstance.dcast());
}

} // {bl::Png}
