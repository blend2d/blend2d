// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

// The PNG codec is based on stb_image <https://github.com/nothings/stb>
// released into PUBLIC DOMAIN. Blend2D's PNG codec can be distributed
// under Blend2D's ZLIB license or under STB's PUBLIC DOMAIN as well.

#include "../blapi-build_p.h"
#include "../blpixelops_p.h"
#include "../blruntime_p.h"
#include "../blsupport_p.h"
#include "../codec/bldeflate_p.h"
#include "../codec/blpngcodec_p.h"
#include "../codec/blpngops_p.h"

// ============================================================================
// [Global Variables]
// ============================================================================

static BLPngCodecImpl blPngCodecImpl;
static BLImageCodecVirt blPngCodecVirt;
static BLImageDecoderVirt blPngDecoderVirt;

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
         blIsPowerOf2(depth);
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
    offset = blAddOverflow(offset, size, &of);

    if (BL_UNLIKELY(of))
      return 0;
  }

  return offset;
}

#define COMB_BYTE_1BPP(B0, B1, B2, B3, B4, B5, B6, B7) \
  uint8_t(((B0) & 0x80) + ((B1) & 0x40) + ((B2) & 0x20) + ((B3) & 0x10) + \
          ((B4) & 0x08) + ((B5) & 0x04) + ((B6) & 0x02) + ((B7) & 0x01) )

#define COMB_BYTE_2BPP(B0, B1, B2, B3) \
  uint8_t(((B0) & 0xC0) + ((B1) & 0x30) + ((B2) & 0x0C) + ((B3) & 0x03))

#define COMB_BYTE_4BPP(B0, B1) \
  uint8_t(((B0) & 0xF0) + ((B1) & 0x0F))

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

            tmpData[0] = COMB_BYTE_1BPP(a     , b >>  9, b >> 2, b >> 10, a >> 12, b >> 11, b >> 5, b >> 12);
            tmpData[1] = COMB_BYTE_1BPP(a << 1, b >>  5, b     , b >>  6, a >> 11, b >>  7, b >> 3, b >>  8);
            tmpData[2] = COMB_BYTE_1BPP(a << 2, b >> 17, b << 2, b >> 18, a >> 10, b >> 19, b >> 1, b >> 20);
            tmpData[3] = COMB_BYTE_1BPP(a << 3, b >> 13, b << 4, b >> 14, a >>  9, b >> 15, b << 1, b >> 16);
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

          tmpData[0] = COMB_BYTE_1BPP(a     , b >>  9, b >> 2, b >> 10, a >> 12, b >> 11, b >> 5, b >> 12); if (x <=  8) break;
          tmpData[1] = COMB_BYTE_1BPP(a << 1, b >>  5, b     , b >>  6, a >> 11, b >>  7, b >> 3, b >>  8); if (x <= 16) break;
          tmpData[2] = COMB_BYTE_1BPP(a << 2, b >> 17, b << 2, b >> 18, a >> 10, b >> 19, b >> 1, b >> 20); if (x <= 24) break;
          tmpData[3] = COMB_BYTE_1BPP(a << 3, b >> 13, b << 4, b >> 14, a >>  9, b >> 15, b << 1, b >> 16);

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

            tmpData[0] = COMB_BYTE_1BPP(a     , b >>  1, a >> 10, b >>  2, a >>  3, b >>  3, a >> 13, b >>  4);
            tmpData[1] = COMB_BYTE_1BPP(a << 2, b <<  3, a >>  8, b <<  2, a >>  1, b <<  1, a >> 11, b      );
            tmpData[2] = COMB_BYTE_1BPP(a << 4, b >>  9, a >>  6, b >> 10, a <<  1, b >> 11, a >>  9, b >> 12);
            tmpData[3] = COMB_BYTE_1BPP(a << 6, b >>  5, a >>  4, b >>  6, a <<  3, b >>  7, a >>  7, b >>  8);
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

          tmpData[0] = COMB_BYTE_1BPP(a     , b >>  1, a >> 10, b >>  2, a >>  3, b >>  3, a >> 13, b >>  4); if (x <=  8) break;
          tmpData[1] = COMB_BYTE_1BPP(a << 2, b <<  3, a >>  8, b <<  2, a >>  1, b <<  1, a >> 11, b      ); if (x <= 16) break;
          tmpData[2] = COMB_BYTE_1BPP(a << 4, b >>  9, a >>  6, b >> 10, a <<  1, b >> 11, a >>  9, b >> 12); if (x <= 24) break;
          tmpData[3] = COMB_BYTE_1BPP(a << 6, b >>  5, a >>  4, b >>  6, a <<  3, b >>  7, a >>  7, b >>  8);

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

            tmpData[0] = COMB_BYTE_1BPP(a     , b >> 1, a >> 1, b >> 2, a >> 2, b >> 3, a >> 3, b >> 4);
            tmpData[1] = COMB_BYTE_1BPP(a << 4, b << 3, a << 3, b << 2, a << 2, b << 1, a << 1, b     );
            tmpData += 2;

            x -= 16;
          }

          if (!x)
            break;

          a = uint32_t(*d4++);
          b = 0;

          if (x >= 2)
            b = uint32_t(*d5++);

          tmpData[0] = COMB_BYTE_1BPP(a     , b >> 1, a >> 1, b >> 2, a >> 2, b >> 3, a >> 3, b >> 4); if (x <= 8) break;
          tmpData[1] = COMB_BYTE_1BPP(a << 4, b << 3, a << 3, b << 2, a << 2, b << 1, a << 1, b     );

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

            tmpData[0] = COMB_BYTE_2BPP(a     , b >> 10, b >> 4, b >> 12);
            tmpData[1] = COMB_BYTE_2BPP(a >> 8, b >>  6, b >> 2, b >>  8);
            tmpData[2] = COMB_BYTE_2BPP(a << 2, b >> 18, b     , b >> 20);
            tmpData[3] = COMB_BYTE_2BPP(a >> 6, b >> 14, b << 2, b >> 16);
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

          tmpData[0] = COMB_BYTE_2BPP(a     , b >> 10, b >> 4, b >> 12); if (x <=  4) break;
          tmpData[1] = COMB_BYTE_2BPP(a >> 8, b >>  6, b >> 2, b >>  8); if (x <=  8) break;
          tmpData[2] = COMB_BYTE_2BPP(a << 2, b >> 18, b     , b >> 20); if (x <= 12) break;
          tmpData[3] = COMB_BYTE_2BPP(a >> 6, b >> 14, b << 2, b >> 16);

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

            tmpData[0] = COMB_BYTE_2BPP(a     , b >>  2, a >> 12, b >>  4);
            tmpData[1] = COMB_BYTE_2BPP(a << 2, b <<  2, a >> 10, b      );

            b = uint32_t(*d5++);

            tmpData[2] = COMB_BYTE_2BPP(a << 4, b >>  2, a >>  8, b >>  4);
            tmpData[3] = COMB_BYTE_2BPP(a << 6, b <<  2, a >>  6, b      );
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

          tmpData[0] = COMB_BYTE_2BPP(a     , b >>  2, a >> 12, b >>  4); if (x <=  4) break;
          tmpData[1] = COMB_BYTE_2BPP(a << 2, b <<  2, a >> 10, b      ); if (x <=  8) break;
          tmpData[2] = COMB_BYTE_2BPP(a << 4, b >> 10, a >>  8, b >> 12); if (x <= 12) break;
          tmpData[3] = COMB_BYTE_2BPP(a << 6, b >>  6, a >>  6, b >>  8);

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

            tmpData[0] = COMB_BYTE_2BPP(a     , b >> 2, a >> 2, b >> 4);
            tmpData[1] = COMB_BYTE_2BPP(a << 4, b << 2, a << 2, b     );
            tmpData += 2;

            x -= 8;
          }

          if (!x)
            break;

          a = uint32_t(*d4++);
          b = 0;

          if (x >= 2)
            b = uint32_t(*d5++);

          tmpData[0] = COMB_BYTE_2BPP(a     , b >> 10, b >> 4, b >> 12); if (x <=  4) break;
          tmpData[1] = COMB_BYTE_2BPP(a >> 8, b >>  6, b >> 2, b >>  8);

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

            tmpData[0] = COMB_BYTE_4BPP(a     , b >> 12);
            tmpData[1] = COMB_BYTE_4BPP(b     , b >>  8);
            tmpData[2] = COMB_BYTE_4BPP(a >> 8, b >> 20);
            tmpData[3] = COMB_BYTE_4BPP(b << 4, b >> 16);
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

          tmpData[0] = COMB_BYTE_4BPP(a     , b >> 12); if (x <= 2) break;
          tmpData[1] = COMB_BYTE_4BPP(b     , b >>  8); if (x <= 4) break;
          tmpData[2] = COMB_BYTE_4BPP(a >> 8, b >> 20); if (x <= 6) break;
          tmpData[3] = COMB_BYTE_4BPP(b << 4, b >> 16);

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
            tmpData[0] = COMB_BYTE_4BPP(a     , b >>  4);
            tmpData[1] = COMB_BYTE_4BPP(a >> 8, b      );

            b = uint32_t(*d5++);
            tmpData[2] = COMB_BYTE_4BPP(a << 4, b >>  4);
            tmpData[3] = COMB_BYTE_4BPP(a >> 4, b      );
            tmpData += 4;

            x -= 8;
          }

          if (!x)
            break;

          a = uint32_t(*d2++);
          b = 0;

          if (x >= 3) a += (uint32_t(*d3++) << 8);
          if (x >= 2) b  = (uint32_t(*d5++)     );
          tmpData[0] = COMB_BYTE_4BPP(a     , b >> 4); if (x <= 2) break;
          tmpData[1] = COMB_BYTE_4BPP(a >> 8, b     ); if (x <= 4) break;

          if (x >= 5) b = uint32_t(*d5++);
          tmpData[2] = COMB_BYTE_4BPP(a << 4, b >> 4); if (x <= 6) break;
          tmpData[3] = COMB_BYTE_4BPP(a >> 4, b     );

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

            tmpData[0] = COMB_BYTE_4BPP(a     , b >> 4);
            tmpData[1] = COMB_BYTE_4BPP(a << 4, b     );
            tmpData += 2;

            x -= 4;
          }

          if (!x)
            break;

          a = uint32_t(*d4++);
          b = 0;

          if (x >= 2)
            b = uint32_t(*d5++);

          tmpData[0] = COMB_BYTE_4BPP(a     , b >> 4); if (x <= 2) break;
          tmpData[1] = COMB_BYTE_4BPP(a << 4, b     );

          break;
        }
      }
    }

    // Don't change to `||`, both have to be executed!
    if ((--y == 0) | (++n == 4)) {
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
    blMemWriteU16a(dst, blMemReadU16u(src));
  }
  else if (N == 4) {
    blMemWriteU32a(dst, blMemReadU32u(src));
  }
  else if (N == 8) {
    blMemWriteU32a(dst + 0, blMemReadU32u(src + 0));
    blMemWriteU32a(dst + 4, blMemReadU32u(src + 4));
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
    if ((--y == 0) | (++n == 4)) {
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
    chunkTag = blMemReadU32uBE(p + index + 4);
    chunkSize = blMemReadU32uBE(p + index + 0);

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

static BLResult BL_CDECL blPngDecoderImplDestroy(BLPngDecoderImpl* impl) noexcept {
  return blRuntimeFreeImpl(impl, sizeof(BLPngDecoderImpl), impl->memPoolData);
}

static BLResult BL_CDECL blPngDecoderImplRestart(BLPngDecoderImpl* impl) noexcept {
  impl->lastResult = BL_SUCCESS;
  impl->frameIndex = 0;
  impl->bufferIndex = 0;

  impl->imageInfo.reset();
  impl->statusFlags = 0;
  impl->colorType = 0;
  impl->sampleDepth = 0;
  impl->sampleCount = 0;
  impl->cgbi = 0;

  return BL_SUCCESS;
}

static BLResult blPngDecoderImplReadInfoInternal(BLPngDecoderImpl* impl, const uint8_t* p, size_t size) noexcept {
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
  uint32_t chunkTag = blMemReadU32uBE(p + 4);
  uint32_t chunkSize = blMemReadU32uBE(p + 0);

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

    chunkTag = blMemReadU32uBE(p + 4);
    chunkSize = blMemReadU32uBE(p + 0);

    impl->statusFlags |= BL_PNG_DECODER_STATUS_SEEN_CgBI;
    impl->cgbi = true;
  }

  p += 8;

  // --------------------------------------------------------------------------
  // [IHDR]
  // --------------------------------------------------------------------------

  if (chunkTag != BL_MAKE_TAG('I', 'H', 'D', 'R') || chunkSize != 13)
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

  // IHDR Data [13 bytes].
  uint32_t w = blMemReadU32uBE(p + 0);
  uint32_t h = blMemReadU32uBE(p + 4);

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

  if (w >= 0x80000000 || h >= 0x80000000)
    return blTraceError(BL_ERROR_IMAGE_TOO_LARGE);

  if (!blPngCheckColorTypeAndBitDepth(colorType, sampleDepth))
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

  // Compression and filter has to be zero, progressive can be [0, 1].
  if (compression != 0 || filter != 0 || progressive >= 2)
    return blTraceError(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);

  // Setup the BLImage+PNG information.
  impl->statusFlags |= BL_PNG_DECODER_STATUS_SEEN_IHDR;
  impl->colorType = uint8_t(colorType);
  impl->sampleDepth = uint8_t(sampleDepth);
  impl->sampleCount = blPngColorTypeToSampleCountTable[colorType];

  impl->imageInfo.size.reset(int(w), int(h));
  impl->imageInfo.depth = sampleDepth * uint32_t(impl->sampleCount);
  impl->imageInfo.frameCount = 1;

  impl->bufferIndex = (size_t)(p - start);
  return BL_SUCCESS;
}

static BLResult blPngDecoderImplReadFrameInternal(BLPngDecoderImpl* impl, BLImage* imageOut, const uint8_t* p, size_t size) noexcept {
  BLResult result = BL_SUCCESS;
  const uint8_t* begin = p;
  const uint8_t* end = p + size;

  // Make sure we won't read out of range.
  if ((size_t)(end - p) < impl->bufferIndex)
    return blTraceError(BL_ERROR_INVALID_STATE);
  p += impl->bufferIndex;

  // Basic information.
  uint32_t w = uint32_t(impl->imageInfo.size.w);
  uint32_t h = uint32_t(impl->imageInfo.size.h);
  uint32_t colorType = impl->colorType;

  // Palette & ColorKey.
  BLRgba32 pal[256];
  uint32_t palSize = 0;

  BLRgba64 colorKey {};
  bool hasColorKey = false;

  // --------------------------------------------------------------------------
  // [Decode Chunks]
  // --------------------------------------------------------------------------

  uint32_t i;
  size_t idatOff = 0; // First IDAT chunk offset.
  size_t idatSize = 0; // Size of all IDAT chunks' p.

  for (;;) {
    // Chunk type, size, and CRC require 12 bytes.
    if ((size_t)(end - p) < 12)
      return blTraceError(BL_ERROR_DATA_TRUNCATED);

    uint32_t chunkTag = blMemReadU32uBE(p + 4);
    uint32_t chunkSize = blMemReadU32uBE(p + 0);

    // Make sure that we have p of the whole chunk.
    if ((size_t)(end - p) - 12 < chunkSize)
      return blTraceError(BL_ERROR_DATA_TRUNCATED);

    // Advance tag+size.
    p += 8;

    // ------------------------------------------------------------------------
    // [IHDR - Once]
    // ------------------------------------------------------------------------

    if (chunkTag == BL_MAKE_TAG('I', 'H', 'D', 'R')) {
      // Multiple IHDR chunks are not allowed.
      return blTraceError(BL_ERROR_PNG_MULTIPLE_IHDR);
    }

    // ------------------------------------------------------------------------
    // [PLTE - Once]
    // ------------------------------------------------------------------------

    else if (chunkTag == BL_MAKE_TAG('P', 'L', 'T', 'E')) {
      // 1. There must not be more than one PLTE chunk.
      // 2. It must precede the first IDAT chunk (also tRNS chunk).
      // 3. Contains 1...256 RGB palette entries.
      if ((impl->statusFlags & (BL_PNG_DECODER_STATUS_SEEN_PLTE | BL_PNG_DECODER_STATUS_SEEN_tRNS | BL_PNG_DECODER_STATUS_SEEN_IDAT)) != 0)
        return blTraceError(BL_ERROR_PNG_INVALID_PLTE);

      if (chunkSize == 0 || chunkSize > 768 || (chunkSize % 3) != 0)
        return blTraceError(BL_ERROR_PNG_INVALID_PLTE);

      palSize = chunkSize / 3;
      impl->statusFlags |= BL_PNG_DECODER_STATUS_SEEN_PLTE;

      for (i = 0; i < palSize; i++, p += 3)
        pal[i] = BLRgba32(p[0], p[1], p[2]);

      while (i < 256)
        pal[i++] = BLRgba32(0x00, 0x00, 0x00, 0xFF);
    }

    // ------------------------------------------------------------------------
    // [tRNS - Once]
    // ------------------------------------------------------------------------

    else if (chunkTag == BL_MAKE_TAG('t', 'R', 'N', 'S')) {
      // 1. There must not be more than one tRNS chunk.
      // 2. It must precede the first IDAT chunk, follow PLTE chunk, if any.
      // 3. It is prohibited for color types 4 and 6.
      if ((impl->statusFlags & (BL_PNG_DECODER_STATUS_SEEN_tRNS | BL_PNG_DECODER_STATUS_SEEN_IDAT)) != 0)
        return blTraceError(BL_ERROR_PNG_INVALID_TRNS);

      if (colorType == BL_PNG_COLOR_TYPE4_LUMA || colorType == BL_PNG_COLOR_TYPE6_RGBA)
        return blTraceError(BL_ERROR_PNG_INVALID_TRNS);

      // For color type 0 (grayscale), the tRNS chunk contains a single gray
      // level value, stored in the format:
      //   [0..1] Gray:  2 bytes, range 0 .. (2^depth)-1
      if (colorType == BL_PNG_COLOR_TYPE0_LUM) {
        if (chunkSize != 2)
          return blTraceError(BL_ERROR_PNG_INVALID_TRNS);

        uint32_t gray = blMemReadU16uBE(p);

        colorKey.reset(0, gray, gray, gray);
        hasColorKey = true;

        p += 2;
      }
      // For color type 2 (truecolor), the tRNS chunk contains a single RGB
      // color value, stored in the format:
      //   [0..1] Red:   2 bytes, range 0 .. (2^depth)-1
      //   [2..3] Green: 2 bytes, range 0 .. (2^depth)-1
      //   [4..5] Blue:  2 bytes, range 0 .. (2^depth)-1
      else if (colorType == BL_PNG_COLOR_TYPE2_RGB) {
        if (chunkSize != 6)
          return blTraceError(BL_ERROR_PNG_INVALID_TRNS);

        uint32_t r = blMemReadU16uBE(p + 0);
        uint32_t g = blMemReadU16uBE(p + 2);
        uint32_t b = blMemReadU16uBE(p + 4);

        colorKey.reset(0, r, g, b);
        hasColorKey = true;

        p += 6;
      }
      // For color type 3 (indexed color), the tRNS chunk contains a series
      // of one-byte alpha values, corresponding to entries in the PLTE chunk.
      else {
        BL_ASSERT(colorType == BL_PNG_COLOR_TYPE3_PAL);
        // 1. Has to follow PLTE if color type is 3.
        // 2. The tRNS chunk can contain 1...palSize alpha values, but in
        //    general it can contain less than `palSize` values, in that case
        //    the remaining alpha values are assumed to be 255.
        if ((impl->statusFlags & BL_PNG_DECODER_STATUS_SEEN_PLTE) == 0 || chunkSize == 0 || chunkSize > palSize)
          return blTraceError(BL_ERROR_PNG_INVALID_TRNS);

        for (i = 0; i < chunkSize; i++, p++) {
          // Premultiply now so we don't have to worry about it later.
          pal[i] = BLRgba32(bl_prgb32_8888_from_argb32_8888(pal[i].value, p[i]));
        }
      }

      impl->statusFlags |= BL_PNG_DECODER_STATUS_SEEN_tRNS;
    }

    // ------------------------------------------------------------------------
    // [IDAT - Many]
    // ------------------------------------------------------------------------

    else if (chunkTag == BL_MAKE_TAG('I', 'D', 'A', 'T')) {
      if (idatOff == 0) {
        idatOff = (size_t)(p - begin) - 8;
        impl->statusFlags |= BL_PNG_DECODER_STATUS_SEEN_IDAT;

        BLOverflowFlag of = 0;
        idatSize = blAddOverflow<size_t>(idatSize, chunkSize, &of);

        if (BL_UNLIKELY(of))
          return blTraceError(BL_ERROR_PNG_INVALID_IDAT);
      }

      p += chunkSize;
    }

    // ------------------------------------------------------------------------
    // [IEND - Once]
    // ------------------------------------------------------------------------

    else if (chunkTag == BL_MAKE_TAG('I', 'E', 'N', 'D')) {
      if (chunkSize != 0 || idatOff == 0)
        return blTraceError(BL_ERROR_PNG_INVALID_IEND);

      // Skip the CRC and break.
      p += 4;
      break;
    }

    // ------------------------------------------------------------------------
    // [Unrecognized]
    // ------------------------------------------------------------------------

    else {
      p += chunkSize;
    }

    // Skip chunk's CRC.
    p += 4;
  }

  // --------------------------------------------------------------------------
  // [Decode]
  // --------------------------------------------------------------------------

  // If we reached this point it means that the image is most probably valid.
  // The index of the first IDAT chunk is stored in `idatOff` and should
  // be non-zero.
  BL_ASSERT(idatOff != 0);

  uint32_t format = BL_FORMAT_PRGB32;
  uint32_t sampleDepth = impl->sampleDepth;
  uint32_t sampleCount = impl->sampleCount;

  uint32_t progressive = (impl->imageInfo.flags & BL_IMAGE_INFO_FLAG_PROGRESSIVE) != 0;
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

  result = Deflate::deflate(output, &rd, (Deflate::ReadFunc)blPngDecoderImplReadFunc, !impl->cgbi);
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

  // --------------------------------------------------------------------------
  // [Convert / Deinterlace]
  // --------------------------------------------------------------------------

  BLImageData imageData;
  BL_PROPAGATE(imageOut->create(w, h, format));
  BL_PROPAGATE(imageOut->makeMutable(&imageData));

  uint8_t* dstPixels = static_cast<uint8_t*>(imageData.pixelData);
  intptr_t dstStride = imageData.stride;

  BLFormatInfo pngFmt {};
  pngFmt.depth = sampleDepth;

  if (BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_BE)
    pngFmt.flags |= BL_FORMAT_FLAG_BYTE_SWAP;

  if (colorType == BL_PNG_COLOR_TYPE0_LUM && sampleDepth <= 8) {
    // Treat grayscale images up to 8bpp as indexed and create a dummy palette.
    blPngCreateGrayscalePalette(pal, sampleDepth);

    // Handle color-key properly.
    if (hasColorKey && colorKey.r < (1u << sampleDepth))
      pal[colorKey.r] = BLRgba32(0);

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
      pngFmt.rSize = 8; pngFmt.rShift = 16;
      pngFmt.gSize = 8; pngFmt.gShift = 8;
      pngFmt.bSize = 8; pngFmt.bShift = 0;
      pngFmt.aSize = 8; pngFmt.aShift = 24;
    }

    if (impl->cgbi) {
      std::swap(pngFmt.rShift, pngFmt.bShift);
      if (pngFmt.flags & BL_FORMAT_FLAG_ALPHA)
        pngFmt.flags |= BL_FORMAT_FLAG_PREMULTIPLIED;
    }
  }

  BLPixelConverter pc;
  BL_PROPAGATE(pc.create(blFormatInfo[format], pngFmt));

  if (progressive) {
    // PNG interlacing requires 7 steps, where 7th handles all even scanlines
    // (indexing from 1). This means that we can, in general, reuse the buffer
    // required by 7th step as a temporary to merge steps 1-6. To achieve this,
    // we need to:
    //
    //   1. Convert all even scanlines already ready by 7th step to `dst`. This
    //      makes the buffer ready to be reused.
    //   2. Merge pixels from steps 1-6 into that buffer.
    //   3. Convert all odd scanlines (from the reused buffer) to `dst`.
    //
    // We, in general, process 4 odd scanlines at a time, so we need the 7th
    // buffer to have enough space to hold them as well, if not, we allocate
    // an extra buffer and use it instead. This approach is good as small
    // images would probably require an extra buffer, but larger images can
    // reuse the 7th.
    BL_ASSERT(steps[6].width == w);
    BL_ASSERT(steps[6].height == h / 2); // Half of the rows, rounded down.

    uint32_t depth = sampleDepth * sampleCount;
    uint32_t tmpHeight = blMin<uint32_t>((h + 1) / 2, 4);
    uint32_t tmpBpl = steps[6].bpl;
    uint32_t tmpSize;

    if (steps[6].height)
      pc.convertRect(dstPixels + dstStride, dstStride * 2, data + 1 + steps[6].offset, tmpBpl, w, steps[6].height);

    // Align `tmpBpl` so we can use aligned memory writes and reads while using it.
    tmpBpl = blAlignUp(tmpBpl, 16);
    tmpSize = tmpBpl * tmpHeight;

    BLMemBuffer tmpAlloc;
    uint8_t* tmp;

    // Decide whether to alloc an extra buffer of to reuse 7th.
    if (steps[6].size < tmpSize + 15)
      tmp = static_cast<uint8_t*>(tmpAlloc.alloc(tmpSize + 15));
    else
      tmp = data + steps[6].offset;

    if (!tmp)
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    tmp = blAlignUp(tmp, 16);
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

  impl->bufferIndex = (size_t)(p - begin);
  return result;
}

static BLResult BL_CDECL blPngDecoderImplReadInfo(BLPngDecoderImpl* impl, BLImageInfo* infoOut, const uint8_t* data, size_t size) noexcept {
  BLResult result = impl->lastResult;
  if (impl->bufferIndex == 0 && result == BL_SUCCESS) {
    result = blPngDecoderImplReadInfoInternal(impl, data, size);
    if (result != BL_SUCCESS)
      impl->lastResult = result;
  }

  if (infoOut)
    memcpy(infoOut, &impl->imageInfo, sizeof(BLImageInfo));

  return result;
}

static BLResult BL_CDECL blPngDecoderImplReadFrame(BLPngDecoderImpl* impl, BLImage* imageOut, const uint8_t* data, size_t size) noexcept {
  BL_PROPAGATE(blPngDecoderImplReadInfo(impl, nullptr, data, size));

  if (impl->frameIndex)
    return blTraceError(BL_ERROR_NO_MORE_DATA);

  BLResult result = blPngDecoderImplReadFrameInternal(impl, imageOut, data, size);
  if (result != BL_SUCCESS)
    impl->lastResult = result;
  return result;
}

static BLPngDecoderImpl* blPngDecoderImplNew() noexcept {
  uint16_t memPoolData;
  BLPngDecoderImpl* impl = blRuntimeAllocImplT<BLPngDecoderImpl>(sizeof(BLPngDecoderImpl), &memPoolData);

  if (BL_UNLIKELY(!impl))
    return nullptr;

  blImplInit(impl, BL_IMPL_TYPE_IMAGE_DECODER, BL_IMPL_TRAIT_VIRT, memPoolData);
  impl->virt = &blPngDecoderVirt;
  impl->codec.impl = &blPngCodecImpl;
  impl->handle = nullptr;
  blPngDecoderImplRestart(impl);

  return impl;
}

// ============================================================================
// [BLPngCodec - Impl]
// ============================================================================

static BLResult BL_CDECL blPngCodecImplDestroy(BLPngCodecImpl* impl) noexcept { return BL_SUCCESS; }

static uint32_t BL_CDECL blPngCodecImplInspectData(BLPngCodecImpl* impl, const uint8_t* data, size_t size) noexcept {
  // Minimum PNG size and signature.
  if (size < 8 || memcmp(data, blPngSignature, 8) != 0)
    return 0;

  return 100;
}

static BLResult BL_CDECL blPngCodecImplCreateDecoder(const BLImageCodecImpl* impl, BLImageDecoderCore* dst) noexcept {
  BLImageDecoderCore decoder { blPngDecoderImplNew() };
  if (BL_UNLIKELY(!decoder.impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  return blImageDecoderAssignMove(dst, &decoder);
}

static BLResult BL_CDECL blPngCodecImplCreateEncoder(const BLImageCodecImpl* impl, BLImageEncoderCore* dst) noexcept {
  // TODO: [PNG] Encoder
  return blTraceError(BL_ERROR_IMAGE_ENCODER_NOT_PROVIDED);
  /*
  BLImageEncoderCore encoder { blPngEncoderImplNew() };
  if (BL_UNLIKELY(!encoder.impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  return blImageEncoderAssignMove(dst, &encoder);
  */
}

// ============================================================================
// [BLPngCodec - Runtime Init]
// ============================================================================

BLImageCodecImpl* blPngCodecRtInit(BLRuntimeContext* rt) noexcept {
  // Initialize PNG ops.
  blPngOpsRtInit(rt);

  // Initialize PNG decoder virtual functions.
  blAssignFunc(&blPngDecoderVirt.destroy, blPngDecoderImplDestroy);
  blAssignFunc(&blPngDecoderVirt.restart, blPngDecoderImplRestart);
  blAssignFunc(&blPngDecoderVirt.readInfo, blPngDecoderImplReadInfo);
  blAssignFunc(&blPngDecoderVirt.readFrame, blPngDecoderImplReadFrame);

  // Initialize PNG encoder virtual functions.
  // TODO: [PNG] Encoder
  // blAssignFunc(&blPngEncoderVirt.destroy, blPngEncoderImplDestroy);
  // blAssignFunc(&blPngEncoderVirt.restart, blPngEncoderImplRestart);
  // blAssignFunc(&blPngEncoderVirt.writeFrame, blPngEncoderImplWriteFrame);

  // Initialize PNG codec virtual functions.
  blAssignFunc(&blPngCodecVirt.destroy, blPngCodecImplDestroy);
  blAssignFunc(&blPngCodecVirt.inspectData, blPngCodecImplInspectData);
  blAssignFunc(&blPngCodecVirt.createDecoder, blPngCodecImplCreateDecoder);
  blAssignFunc(&blPngCodecVirt.createEncoder, blPngCodecImplCreateEncoder);

  // Initialize PNG codec built-in instance.
  BLPngCodecImpl* codecI = &blPngCodecImpl;

  codecI->virt = &blPngCodecVirt;
  codecI->implType = uint8_t(BL_IMPL_TYPE_IMAGE_CODEC);
  codecI->implTraits = uint8_t(BL_IMPL_TRAIT_VIRT);

  codecI->features = BL_IMAGE_CODEC_FEATURE_READ     |
                     BL_IMAGE_CODEC_FEATURE_WRITE    |
                     BL_IMAGE_CODEC_FEATURE_LOSSLESS ;

  codecI->name = "PNG";
  codecI->vendor = "Blend2D";
  codecI->mimeType = "image/png";
  codecI->extensions = "png";

  return codecI;
}
