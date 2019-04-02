// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blformat_p.h"
#include "./blimage.h"
#include "./blpixelconverter_p.h"
#include "./blruntime_p.h"
#include "./blsupport_p.h"
#include "./bltables_p.h"

// ============================================================================
// [BLPixelConverter - Tables]
// ============================================================================

// A table that contains shifts of native 32-bit pixel format. The only reason
// to have this in a table is a fact that a blue component is shifted by 8 (the
// same as green) to be at the right place, because there is no way to calculate
// the constants of component that has to stay within the low 8 bits as `scale`
// value is calculated by doubling the size until it reaches the required depth,
// so for example depth of 5 would scale to 10, depth 3 would scale to 9, and
// depths 1-2 would scale to 8.
static constexpr const uint8_t blPixelConverterNative32FromExternalShiftTable[] = {
  16, // [0x00FF0000] R.
  8 , // [0x0000FF00] G.
  8 , // [0x0000FF00] B (shift to right by 8 to get the desired result).
  24  // [0xFF000000] A.
};

#define F(VALUE) BL_FORMAT_FLAG_##VALUE
#define U 0 // Used only to distinguish between zero and Unused.

const BLFormatInfo blPixelConverterFormatInfo[BL_PIXEL_CONVERTER_FORMAT_COUNT] = {
  {  0, 0                                            , {{ { U, U, U, U }, {  U,  U,  U,  U } }} }, // NONE.
  { 32, F(RGBA)  | F(BYTE_ALIGNED) | F(PREMULTIPLIED), {{ { 8, 8, 8, 8 }, { 16,  8,  0, 24 } }} }, // PRGB32.
  { 32, F(RGB)   | F(BYTE_ALIGNED)                   , {{ { 8, 8, 8, U }, { 16,  8,  0,  U } }} }, // XRGB32.
  {  8, F(ALPHA) | F(BYTE_ALIGNED)                   , {{ { U, U, U, 8 }, {  U,  U,  U,  0 } }} }, // A8.
  { 32, F(RGBA)  | F(BYTE_ALIGNED)                   , {{ { 8, 8, 8, 8 }, { 16,  8,  0, 24 } }} }  // ARGB32.
};

#undef U
#undef F

// ============================================================================
// [BLPixelConverter - Globals]
// ============================================================================

const BLPixelConverterOptions blPixelConverterDefaultOptions {};

// ============================================================================
// [BLPixelConverter - Pixel Access]
// ============================================================================

template<uint32_t ByteOrder>
struct BLPixelAccess16 {
  enum : uint32_t { kSize = 2 };

  static BL_INLINE uint32_t fetchA(const void* p) noexcept { return blMemReadU16<ByteOrder, 2>(p); }
  static BL_INLINE uint32_t fetchU(const void* p) noexcept { return blMemReadU16<ByteOrder, 1>(p); }

  static BL_INLINE void storeA(void* p, uint32_t v) noexcept { blMemWriteU16<ByteOrder, 2>(p, uint16_t(v)); }
  static BL_INLINE void storeU(void* p, uint32_t v) noexcept { blMemWriteU16<ByteOrder, 1>(p, uint16_t(v)); }
};

template<uint32_t ByteOrder>
struct BLPixelAccess24 {
  enum : uint32_t { kSize = 3 };

  static BL_INLINE uint32_t fetchA(const void* p) noexcept { return blMemReadU24u<ByteOrder>(p); }
  static BL_INLINE uint32_t fetchU(const void* p) noexcept { return blMemReadU24u<ByteOrder>(p); }

  static BL_INLINE void storeA(void* p, uint32_t v) noexcept { blMemWriteU24u<ByteOrder>(p, v); }
  static BL_INLINE void storeU(void* p, uint32_t v) noexcept { blMemWriteU24u<ByteOrder>(p, v); }
};

template<uint32_t ByteOrder>
struct BLPixelAccess32 {
  enum : uint32_t { kSize = 4 };

  static BL_INLINE uint32_t fetchA(const void* p) noexcept { return blMemReadU32<ByteOrder, 4>(p); }
  static BL_INLINE uint32_t fetchU(const void* p) noexcept { return blMemReadU32<ByteOrder, 1>(p); }

  static BL_INLINE void storeA(void* p, uint32_t v) noexcept { blMemWriteU32<ByteOrder, 4>(p, v); }
  static BL_INLINE void storeU(void* p, uint32_t v) noexcept { blMemWriteU32<ByteOrder, 1>(p, v); }
};

// ============================================================================
// [BLPixelConverter - LookupTable]
// ============================================================================

static BLResult BL_CDECL bl_convert_lookup32_from_index1(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcLine, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const BLPixelConverterData::LookupTable& d = blPixelConverterGetData(self)->lookupTable;
  const size_t gap = options->gap;

  // Instead of doing a table lookup each time we create a XOR mask that is
  // used to get the second color value from the first one. This allows to
  // remove the lookup completely. The only requirement is that we need all
  // zeros or ones depending on the source value (see the implementation, it
  // uses signed right shift to fill these bits in).
  uint32_t c0 = d.table[0];
  uint32_t cm = d.table[1] ^ c0;

  dstStride -= intptr_t(w * 4 + gap);

  if (c0 == 0x00000000u && cm == 0xFFFFFFFFu) {
    // Special case for black/white palette, quite common.
    for (uint32_t y = h; y != 0; y--) {
      const uint8_t* srcData = srcLine;

      uint32_t i = w;
      while (i >= 8) {
        uint32_t b0 = uint32_t(*srcData++) << 24;
        uint32_t b1 = b0 << 1;

        blMemWriteU32a(dstData +  0, blBitSar(b0, 31)); b0 <<= 2;
        blMemWriteU32a(dstData +  4, blBitSar(b1, 31)); b1 <<= 2;
        blMemWriteU32a(dstData +  8, blBitSar(b0, 31)); b0 <<= 2;
        blMemWriteU32a(dstData + 12, blBitSar(b1, 31)); b1 <<= 2;
        blMemWriteU32a(dstData + 16, blBitSar(b0, 31)); b0 <<= 2;
        blMemWriteU32a(dstData + 20, blBitSar(b1, 31)); b1 <<= 2;
        blMemWriteU32a(dstData + 24, blBitSar(b0, 31));
        blMemWriteU32a(dstData + 28, blBitSar(b1, 31));

        dstData += 32;
        i -= 8;
      }

      if (i) {
        uint32_t b0 = uint32_t(*srcData++) << 24;
        do {
          blMemWriteU32a(dstData, blBitSar(b0, 31));

          dstData += 4;
          b0 <<= 1;
        } while (--i);
      }

      dstData = blPixelConverterFillGap(dstData, gap);
      dstData += dstStride;
      srcLine += srcStride;
    }
  }
  else {
    // Generic case for any other combination.
    for (uint32_t y = h; y != 0; y--) {
      const uint8_t* srcData = srcLine;

      uint32_t i = w;
      while (i >= 8) {
        uint32_t b0 = uint32_t(*srcData++) << 24;
        uint32_t b1 = b0 << 1;

        blMemWriteU32a(dstData +  0, c0 ^ (cm & blBitSar(b0, 31))); b0 <<= 2;
        blMemWriteU32a(dstData +  4, c0 ^ (cm & blBitSar(b1, 31))); b1 <<= 2;
        blMemWriteU32a(dstData +  8, c0 ^ (cm & blBitSar(b0, 31))); b0 <<= 2;
        blMemWriteU32a(dstData + 12, c0 ^ (cm & blBitSar(b1, 31))); b1 <<= 2;
        blMemWriteU32a(dstData + 16, c0 ^ (cm & blBitSar(b0, 31))); b0 <<= 2;
        blMemWriteU32a(dstData + 20, c0 ^ (cm & blBitSar(b1, 31))); b1 <<= 2;
        blMemWriteU32a(dstData + 24, c0 ^ (cm & blBitSar(b0, 31)));
        blMemWriteU32a(dstData + 28, c0 ^ (cm & blBitSar(b1, 31)));

        dstData += 32;
        i -= 8;
      }

      if (i) {
        uint32_t b0 = uint32_t(*srcData++) << 24;
        do {
          blMemWriteU32a(dstData, c0 ^ (cm & blBitSar(b0, 31)));

          dstData += 4;
          b0 <<= 1;
        } while (--i);
      }

      dstData = blPixelConverterFillGap(dstData, gap);
      dstData += dstStride;
      srcLine += srcStride;
    }
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL bl_convert_lookup32_from_index2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcLine, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const BLPixelConverterData::LookupTable& d = blPixelConverterGetData(self)->lookupTable;
  const uint32_t* table = d.table;
  const size_t gap = options->gap;

  dstStride -= w * 4 + gap;

  for (uint32_t y = h; y != 0; y--) {
    const uint8_t* srcData = srcLine;

    uint32_t i = w;
    while (i >= 4) {
      uint32_t b0 = uint32_t(*srcData++) << 24;

      blMemWriteU32a(dstData +  0, table[b0 >> 30]); b0 <<= 2;
      blMemWriteU32a(dstData +  4, table[b0 >> 30]); b0 <<= 2;
      blMemWriteU32a(dstData +  8, table[b0 >> 30]); b0 <<= 2;
      blMemWriteU32a(dstData + 12, table[b0 >> 30]);

      dstData += 16;
      i -= 4;
    }

    if (i) {
      uint32_t b0 = uint32_t(*srcData++) << 24;
      do {
        blMemWriteU32a(dstData, table[b0 >> 30]);

        dstData += 4;
        b0 <<= 2;
      } while (--i);
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcLine += srcStride;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL bl_convert_lookup32_from_index4(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcLine, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const BLPixelConverterData::LookupTable& d = blPixelConverterGetData(self)->lookupTable;
  const uint32_t* table = d.table;
  const size_t gap = options->gap;

  dstStride -= w * 4 + gap;

  for (uint32_t y = h; y != 0; y--) {
    const uint8_t* srcData = srcLine;

    uint32_t i = w;
    while (i >= 2) {
      uint32_t b0 = *srcData++;

      blMemWriteU32a(dstData + 0, table[b0 >> 4]);
      blMemWriteU32a(dstData + 4, table[b0 & 15]);

      dstData += 8;
      i -= 2;
    }

    if (i) {
      uint32_t b0 = srcData[0];
      blMemWriteU32a(dstData, table[b0 >> 4]);
      dstData += 4;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcLine += srcStride;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL bl_convert_lookup32_from_index8(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcLine, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const BLPixelConverterData::LookupTable& d = blPixelConverterGetData(self)->lookupTable;
  const uint32_t* table = d.table;
  const size_t gap = options->gap;

  dstStride -= w * 4 + gap;

  for (uint32_t y = h; y != 0; y--) {
    const uint8_t* srcData = srcLine;

    for (uint32_t i = w; i != 0; i--) {
      uint32_t b0 = *srcData++;
      blMemWriteU32a(dstData, table[b0]);
      dstData += 4;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcLine += srcStride;
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLPixelConverter - ByteShuffle]
// ============================================================================

// TODO:

// ============================================================================
// [BLPixelConverter - Native32 <- XRGB|ARGB|PRGB]
// ============================================================================

template<typename PixelAccess, bool AlwaysUnaligned>
static BLResult BL_CDECL bl_convert_xrgb32_from_xrgb_any(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const BLPixelConverterData::NativeFromExternal& d = blPixelConverterGetData(self)->nativeFromExternal;
  const size_t gap = options->gap;

  dstStride -= w * 4 + gap;
  srcStride -= w * PixelAccess::kSize;

  uint32_t rMask = d.masks[0];
  uint32_t gMask = d.masks[1];
  uint32_t bMask = d.masks[2];

  uint32_t rShift = d.shifts[0];
  uint32_t gShift = d.shifts[1];
  uint32_t bShift = d.shifts[2];

  uint32_t rScale = d.scale[0];
  uint32_t gScale = d.scale[1];
  uint32_t bScale = d.scale[2];

  uint32_t fillMask = d.fillMask;

  for (uint32_t y = h; y != 0; y--) {
    if (!AlwaysUnaligned && blIsAligned(srcData, PixelAccess::kSize)) {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = PixelAccess::fetchA(srcData);
        uint32_t r = (((pix >> rShift) & rMask) * rScale) & 0x00FF0000u;
        uint32_t g = (((pix >> gShift) & gMask) * gScale) & 0x0000FF00u;
        uint32_t b = (((pix >> bShift) & bMask) * bScale) >> 8;

        blMemWriteU32a(dstData, r | g | b | fillMask);

        dstData += 4;
        srcData += PixelAccess::kSize;
      }
    }
    else {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = PixelAccess::fetchU(srcData);
        uint32_t r = (((pix >> rShift) & rMask) * rScale) & 0x00FF0000u;
        uint32_t g = (((pix >> gShift) & gMask) * gScale) & 0x0000FF00u;
        uint32_t b = (((pix >> bShift) & bMask) * bScale) >> 8;

        blMemWriteU32a(dstData, r | g | b | fillMask);

        dstData += 4;
        srcData += PixelAccess::kSize;
      }
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

template<typename PixelAccess, bool AlwaysUnaligned>
static BLResult BL_CDECL bl_convert_prgb32_from_argb_any(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const BLPixelConverterData::NativeFromExternal& d = blPixelConverterGetData(self)->nativeFromExternal;
  const size_t gap = options->gap;

  dstStride -= w * 4 + gap;
  srcStride -= w * PixelAccess::kSize;

  uint32_t rMask = d.masks[0];
  uint32_t gMask = d.masks[1];
  uint32_t bMask = d.masks[2];
  uint32_t aMask = d.masks[3];

  uint32_t rShift = d.shifts[0];
  uint32_t gShift = d.shifts[1];
  uint32_t bShift = d.shifts[2];
  uint32_t aShift = d.shifts[3];

  uint32_t rScale = d.scale[0];
  uint32_t gScale = d.scale[1];
  uint32_t bScale = d.scale[2];
  uint32_t aScale = d.scale[3];

  for (uint32_t y = h; y != 0; y--) {
    if (!AlwaysUnaligned && blIsAligned(srcData, PixelAccess::kSize)) {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = PixelAccess::fetchA(srcData);
        uint32_t _a = ((((pix >> aShift) & aMask) * aScale) >> 24);
        uint32_t ag = ((((pix >> gShift) & gMask) * gScale) >>  8);
        uint32_t rb = ((((pix >> rShift) & rMask) * rScale) & 0x00FF0000u) |
                      ((((pix >> bShift) & bMask) * bScale) >>  8);

        ag |= 0x00FF0000u;
        rb *= _a;
        ag *= _a;

        rb += 0x00800080u;
        ag += 0x00800080u;

        rb = (rb + ((rb >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;
        ag = (ag + ((ag >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;

        rb >>= 8;
        blMemWriteU32a(dstData, ag + rb);

        dstData += 4;
        srcData += PixelAccess::kSize;
      }
    }
    else {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = PixelAccess::fetchU(srcData);
        uint32_t _a = ((((pix >> aShift) & aMask) * aScale) >> 24);
        uint32_t ag = ((((pix >> gShift) & gMask) * gScale) >>  8);
        uint32_t rb = ((((pix >> rShift) & rMask) * rScale) & 0x00FF0000u) |
                      ((((pix >> bShift) & bMask) * bScale) >>  8);

        ag |= 0x00FF0000u;
        rb *= _a;
        ag *= _a;

        rb += 0x00800080u;
        ag += 0x00800080u;

        rb = (rb + ((rb >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;
        ag = (ag + ((ag >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;

        rb >>= 8;
        blMemWriteU32a(dstData, ag | rb);

        dstData += 4;
        srcData += PixelAccess::kSize;
      }
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

template<typename PixelAccess, bool AlwaysUnaligned>
static BLResult BL_CDECL bl_convert_prgb32_from_prgb_any(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const BLPixelConverterData::NativeFromExternal& d = blPixelConverterGetData(self)->nativeFromExternal;
  const size_t gap = options->gap;

  dstStride -= w * 4 + gap;
  srcStride -= w * PixelAccess::kSize;

  uint32_t rMask = d.masks[0];
  uint32_t gMask = d.masks[1];
  uint32_t bMask = d.masks[2];
  uint32_t aMask = d.masks[3];

  uint32_t rShift = d.shifts[0];
  uint32_t gShift = d.shifts[1];
  uint32_t bShift = d.shifts[2];
  uint32_t aShift = d.shifts[3];

  uint32_t rScale = d.scale[0];
  uint32_t gScale = d.scale[1];
  uint32_t bScale = d.scale[2];
  uint32_t aScale = d.scale[3];

  for (uint32_t y = h; y != 0; y--) {
    if (!AlwaysUnaligned && blIsAligned(srcData, PixelAccess::kSize)) {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = PixelAccess::fetchA(srcData);
        uint32_t r = ((pix >> rShift) & rMask) * rScale;
        uint32_t g = ((pix >> gShift) & gMask) * gScale;
        uint32_t b = ((pix >> bShift) & bMask) * bScale;
        uint32_t a = ((pix >> aShift) & aMask) * aScale;

        uint32_t ag = (a + (g     )) & 0xFF00FF00u;
        uint32_t rb = (r + (b >> 8)) & 0x00FF00FFu;

        blMemWriteU32a(dstData, ag | rb);

        dstData += 4;
        srcData += PixelAccess::kSize;
      }
    }
    else {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = PixelAccess::fetchU(srcData);
        uint32_t g = ((pix >> gShift) & gMask) * gScale;
        uint32_t r = ((pix >> rShift) & rMask) * rScale;
        uint32_t b = ((pix >> bShift) & bMask) * bScale;
        uint32_t a = ((pix >> aShift) & aMask) * aScale;

        uint32_t ag = (a + (g     )) & 0xFF00FF00u;
        uint32_t rb = (r + (b >> 8)) & 0x00FF00FFu;

        blMemWriteU32a(dstData, ag | rb);

        dstData += 4;
        srcData += PixelAccess::kSize;
      }
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLPixelConverter - XRGB|ARGB|PRGB <- Native32]
// ============================================================================

template<typename PixelAccess, bool AlwaysUnaligned>
static BLResult BL_CDECL bl_convert_xrgb_any_from_xrgb32(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const BLPixelConverterData::ExternalFromNative& d = blPixelConverterGetData(self)->externalFromNative;
  const size_t gap = options->gap;

  dstStride -= w * PixelAccess::kSize + gap;
  srcStride -= w * 4;

  uint32_t rMask = d.masks[0];
  uint32_t gMask = d.masks[1];
  uint32_t bMask = d.masks[2];

  uint32_t rShift = d.shifts[0];
  uint32_t gShift = d.shifts[1];
  uint32_t bShift = d.shifts[2];

  uint32_t fillMask = d.fillMask;

  for (uint32_t y = h; y != 0; y--) {
    if (!AlwaysUnaligned && blIsAligned(dstData, PixelAccess::kSize)) {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = blMemReadU32a(srcData);

        uint32_t r = ((pix >> 16) & 0xFFu) * 0x01010101u;
        uint32_t g = ((pix >>  8) & 0xFFu) * 0x01010101u;
        uint32_t b = ((pix      ) & 0xFFu) * 0x01010101u;

        PixelAccess::storeA(dstData, ((r >> rShift) & rMask) |
                                     ((g >> gShift) & gMask) |
                                     ((b >> bShift) & bMask) | fillMask);
        dstData += PixelAccess::kSize;
        srcData += 4;
      }
    }
    else {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = blMemReadU32u(srcData);

        uint32_t r = ((pix >> 16) & 0xFFu) * 0x01010101u;
        uint32_t g = ((pix >>  8) & 0xFFu) * 0x01010101u;
        uint32_t b = ((pix      ) & 0xFFu) * 0x01010101u;

        PixelAccess::storeU(dstData, ((r >> rShift) & rMask) |
                                     ((g >> gShift) & gMask) |
                                     ((b >> bShift) & bMask) | fillMask);
        dstData += PixelAccess::kSize;
        srcData += 4;
      }
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

template<typename PixelAccess, bool AlwaysUnaligned>
static BLResult BL_CDECL bl_convert_argb_any_from_prgb32(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const BLPixelConverterData::ExternalFromNative& d = blPixelConverterGetData(self)->externalFromNative;
  const size_t gap = options->gap;

  dstStride -= w * PixelAccess::kSize + gap;
  srcStride -= w * 4;

  uint32_t rMask = d.masks[0];
  uint32_t gMask = d.masks[1];
  uint32_t bMask = d.masks[2];
  uint32_t aMask = d.masks[3];

  uint32_t rShift = d.shifts[0];
  uint32_t gShift = d.shifts[1];
  uint32_t bShift = d.shifts[2];
  uint32_t aShift = d.shifts[3];

  const uint32_t* div24bitRecip = blCommonTable.div24bit.data;

  for (uint32_t y = h; y != 0; y--) {
    if (!AlwaysUnaligned && blIsAligned(dstData, PixelAccess::kSize)) {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = blMemReadU32a(srcData);

        uint32_t a = pix >> 24;
        uint32_t recip = div24bitRecip[a];

        uint32_t r = ((((pix >> 16) & 0xFFu) * recip) >> 16) * 0x01010101u;
        uint32_t g = ((((pix >>  8) & 0xFFu) * recip) >> 16) * 0x01010101u;
        uint32_t b = ((((pix      ) & 0xFFu) * recip) >> 16) * 0x01010101u;

        a *= 0x01010101u;
        PixelAccess::storeA(dstData, ((r >> rShift) & rMask) |
                                     ((g >> gShift) & gMask) |
                                     ((b >> bShift) & bMask) |
                                     ((a >> aShift) & aMask));
        dstData += PixelAccess::kSize;
        srcData += 4;
      }
    }
    else {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = blMemReadU32u(srcData);

        uint32_t a = pix >> 24;
        uint32_t recip = div24bitRecip[a];

        uint32_t r = ((((pix >> 16) & 0xFFu) * recip) >> 16) * 0x01010101u;
        uint32_t g = ((((pix >>  8) & 0xFFu) * recip) >> 16) * 0x01010101u;
        uint32_t b = ((((pix      ) & 0xFFu) * recip) >> 16) * 0x01010101u;

        a *= 0x01010101u;
        PixelAccess::storeU(dstData, ((r >> rShift) & rMask) |
                                     ((g >> gShift) & gMask) |
                                     ((b >> bShift) & bMask) |
                                     ((a >> aShift) & aMask));
        dstData += PixelAccess::kSize;
        srcData += 4;
      }
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

template<typename PixelAccess, bool AlwaysUnaligned>
static BLResult BL_CDECL bl_convert_prgb_any_from_prgb32(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const BLPixelConverterData::ExternalFromNative& d = blPixelConverterGetData(self)->externalFromNative;
  const size_t gap = options->gap;

  dstStride -= w * PixelAccess::kSize + gap;
  srcStride -= w * 4;

  uint32_t rMask = d.masks[0];
  uint32_t gMask = d.masks[1];
  uint32_t bMask = d.masks[2];
  uint32_t aMask = d.masks[3];

  uint32_t rShift = d.shifts[0];
  uint32_t gShift = d.shifts[1];
  uint32_t bShift = d.shifts[2];
  uint32_t aShift = d.shifts[3];

  for (uint32_t y = h; y != 0; y--) {
    if (!AlwaysUnaligned && blIsAligned(dstData, PixelAccess::kSize)) {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = blMemReadU32a(srcData);

        uint32_t r = ((pix >> 16) & 0xFFu) * 0x01010101u;
        uint32_t g = ((pix >>  8) & 0xFFu) * 0x01010101u;
        uint32_t b = ((pix      ) & 0xFFu) * 0x01010101u;
        uint32_t a = ((pix >> 24)        ) * 0x01010101u;

        PixelAccess::storeA(dstData, ((r >> rShift) & rMask) |
                                     ((g >> gShift) & gMask) |
                                     ((b >> bShift) & bMask) |
                                     ((a >> aShift) & aMask));
        dstData += PixelAccess::kSize;
        srcData += 4;
      }
    }
    else {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = blMemReadU32u(srcData);

        uint32_t r = ((pix >> 16) & 0xFFu) * 0x01010101u;
        uint32_t g = ((pix >>  8) & 0xFFu) * 0x01010101u;
        uint32_t b = ((pix      ) & 0xFFu) * 0x01010101u;
        uint32_t a = ((pix >> 24)        ) * 0x01010101u;

        PixelAccess::storeU(dstData, ((r >> rShift) & rMask) |
                                     ((g >> gShift) & gMask) |
                                     ((b >> bShift) & bMask) |
                                     ((a >> aShift) & aMask));
        dstData += PixelAccess::kSize;
        srcData += 4;
      }
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLPixelConverter - Utilities]
// ============================================================================

static uint32_t blPixelConverterMatchFormat(const BLFormatInfo& fmt) noexcept {
  for (uint32_t i = 1; i < BL_PIXEL_CONVERTER_FORMAT_COUNT; i++)
    if (memcmp(&blPixelConverterFormatInfo[i], &fmt, sizeof(BLFormatInfo)) == 0)
      return i;
  return BL_PIXEL_CONVERTER_FORMAT_NONE;
}

static BLResult blPixelConverterInitInternal(BLPixelConverterCore* self, const BLFormatInfo& dstInfo, const BLFormatInfo& srcInfo) noexcept {
  // Initially the pixel converter should be initialized to all zeros. So we
  // just fill what we need to, but we don't have to zero the existing members.
  BLPixelConverterFunc func = nullptr;

  uint32_t dstFormat = blPixelConverterMatchFormat(dstInfo);
  uint32_t srcFormat = blPixelConverterMatchFormat(srcInfo);

  // --------------------------------------------------------------------------
  // [Native <- External]
  // --------------------------------------------------------------------------

  if (dstFormat != BL_PIXEL_CONVERTER_FORMAT_NONE) {
    if (srcInfo.flags & BL_FORMAT_FLAG_INDEXED) {
      switch (srcInfo.depth) {
        case 1: func = bl_convert_lookup32_from_index1; break;
        case 2: func = bl_convert_lookup32_from_index2; break;
        case 4: func = bl_convert_lookup32_from_index4; break;
        case 8: func = bl_convert_lookup32_from_index8; break;

        default:
          // We return invalid value, but the sanitizer should fail in such case.
          return blTraceError(BL_ERROR_INVALID_VALUE);
      }

      BLPixelConverterData::LookupTable& d = blPixelConverterGetData(self)->lookupTable;
      d.strategy = BL_PIXEL_CONVERTER_STRATEGY_LOOKUP_TABLE;
      d.table = reinterpret_cast<const uint32_t*>(srcInfo.palette);

      self->convertFunc = func;
      return BL_SUCCESS;
    }
    else {
      BLPixelConverterData::NativeFromExternal& d = blPixelConverterGetData(self)->nativeFromExternal;

      bool isARGB = (srcInfo.flags & BL_FORMAT_FLAG_ALPHA) != 0;
      bool isPRGB = (srcInfo.flags & BL_FORMAT_FLAG_PREMULTIPLIED) != 0;
      bool isGray = (srcInfo.flags & BL_FORMAT_FLAG_LUM) != 0;
      bool hostBO = (srcInfo.flags & BL_FORMAT_FLAG_BYTE_SWAP) == 0;

      if (dstInfo.depth == 32 && !isARGB)
        d.fillMask = 0xFF000000u;

      for (uint32_t i = 0; i < 4; i++) {
        uint32_t size = srcInfo.sizes[i];
        uint32_t shift = srcInfo.shifts[i];

        d.masks[i] = 0;
        d.shifts[i] = uint8_t(shift);
        d.scale[i] = 0;

        if (size == 0)
          continue;

        // Discard all bits that are below 8 most significant ones.
        if (size > 8) {
          shift += (size - 8);
          size = 8;
        }

        d.masks[i] = blTrailingBitMask<uint32_t>(size);
        d.shifts[i] = uint8_t(shift);

        // Calculate a scale constant that will be used to expand bits in case
        // that the source contains less than 8 bits. We do it by adding `size`
        // to the `scaledSize` until we reach the required bit-depth.
        uint32_t scale = 0x1;
        uint32_t scaledSize = size;

        while (scaledSize < 8) {
          scale = (scale << size) | 1;
          scaledSize += size;
        }

        // Shift scale in a way that it contains MSB of the mask and the right position.
        uint32_t scaledShift = blPixelConverterNative32FromExternalShiftTable[i] - (scaledSize - 8);
        scale <<= scaledShift;
        d.scale[i] = scale;
      }

      // Prefer SIMD optimized converters if possible.
      #ifdef BL_BUILD_OPT_AVX2
      if (blRuntimeHasAVX2(&blRuntimeContext) && blPixelConverterInitNativeFromXRGB_AVX2(self, dstFormat, srcInfo))
        return BL_SUCCESS;
      #endif

      #ifdef BL_BUILD_OPT_SSSE3
      if (blRuntimeHasSSSE3(&blRuntimeContext) && blPixelConverterInitNativeFromXRGB_SSSE3(self, dstFormat, srcInfo))
        return BL_SUCCESS;
      #endif

      #ifdef BL_BUILD_OPT_SSE2
      if (blRuntimeHasSSE2(&blRuntimeContext) && blPixelConverterInitNativeFromXRGB_SSE2(self, dstFormat, srcInfo))
        return BL_SUCCESS;
      #endif

      // Special case of converting LUM to RGB.
      if (srcInfo.flags & BL_FORMAT_FLAG_LUM) {
        // TODO:
      }

      // Generic conversion.
      switch (srcInfo.depth) {
        case 16:
          if (isPRGB)
            func = hostBO ? bl_convert_prgb32_from_prgb_any<BLPixelAccess16<BL_BYTE_ORDER_NATIVE>, BL_UNALIGNED_IO_16>
                          : bl_convert_prgb32_from_prgb_any<BLPixelAccess16<BL_BYTE_ORDER_SWAPPED>, BL_UNALIGNED_IO_16>;
          else if (isARGB)
            func = hostBO ? bl_convert_prgb32_from_argb_any<BLPixelAccess16<BL_BYTE_ORDER_NATIVE>, BL_UNALIGNED_IO_16>
                          : bl_convert_prgb32_from_argb_any<BLPixelAccess16<BL_BYTE_ORDER_SWAPPED>, BL_UNALIGNED_IO_16>;
          else
            func = hostBO ? bl_convert_xrgb32_from_xrgb_any<BLPixelAccess16<BL_BYTE_ORDER_NATIVE>, BL_UNALIGNED_IO_16>
                          : bl_convert_xrgb32_from_xrgb_any<BLPixelAccess16<BL_BYTE_ORDER_SWAPPED>, BL_UNALIGNED_IO_16>;
          break;

        case 24:
          if (isPRGB)
            func = hostBO ? bl_convert_prgb32_from_prgb_any<BLPixelAccess24<BL_BYTE_ORDER_NATIVE>, true>
                          : bl_convert_prgb32_from_prgb_any<BLPixelAccess24<BL_BYTE_ORDER_SWAPPED>, true>;
          else if (isARGB)
            func = hostBO ? bl_convert_prgb32_from_argb_any<BLPixelAccess24<BL_BYTE_ORDER_NATIVE>, true>
                          : bl_convert_prgb32_from_argb_any<BLPixelAccess24<BL_BYTE_ORDER_SWAPPED>, true>;
          else
            func = hostBO ? bl_convert_xrgb32_from_xrgb_any<BLPixelAccess24<BL_BYTE_ORDER_NATIVE>, true>
                          : bl_convert_xrgb32_from_xrgb_any<BLPixelAccess24<BL_BYTE_ORDER_SWAPPED>, true>;
          break;

        case 32:
          if (isPRGB)
            func = hostBO ? bl_convert_prgb32_from_prgb_any<BLPixelAccess32<BL_BYTE_ORDER_NATIVE>, BL_UNALIGNED_IO_32>
                          : bl_convert_prgb32_from_prgb_any<BLPixelAccess32<BL_BYTE_ORDER_SWAPPED>, BL_UNALIGNED_IO_32>;
          else if (isARGB)
            func = hostBO ? bl_convert_prgb32_from_argb_any<BLPixelAccess32<BL_BYTE_ORDER_NATIVE>, BL_UNALIGNED_IO_32>
                          : bl_convert_prgb32_from_argb_any<BLPixelAccess32<BL_BYTE_ORDER_SWAPPED>, BL_UNALIGNED_IO_32>;
          else
            func = hostBO ? bl_convert_xrgb32_from_xrgb_any<BLPixelAccess32<BL_BYTE_ORDER_NATIVE>, BL_UNALIGNED_IO_32>
                          : bl_convert_xrgb32_from_xrgb_any<BLPixelAccess32<BL_BYTE_ORDER_SWAPPED>, BL_UNALIGNED_IO_32>;
          break;

        default:
          return blTraceError(BL_ERROR_INVALID_VALUE);
      }

      self->convertFunc = func;
      return BL_SUCCESS;
    }
  }

  // --------------------------------------------------------------------------
  // [External <- Native]
  // --------------------------------------------------------------------------

  if (srcFormat != BL_PIXEL_CONVERTER_FORMAT_NONE) {
    if (dstInfo.flags & BL_FORMAT_FLAG_INDEXED) {
      // TODO:
      return blTraceError(BL_ERROR_NOT_IMPLEMENTED);
    }
    else {
      BLPixelConverterData::ExternalFromNative& d = blPixelConverterGetData(self)->externalFromNative;

      bool isARGB = (dstInfo.flags & BL_FORMAT_FLAG_ALPHA) != 0;
      bool isPRGB = (dstInfo.flags & BL_FORMAT_FLAG_PREMULTIPLIED) != 0;
      bool isGray = (dstInfo.flags & BL_FORMAT_FLAG_LUM) != 0;
      bool hostBO = (dstInfo.flags & BL_FORMAT_FLAG_BYTE_SWAP) == 0;

      for (uint32_t i = 0; i < 4; i++) {
        uint32_t mask = 0;
        uint32_t size = dstInfo.sizes[i];
        uint32_t shift = dstInfo.shifts[i];

        if (size != 0) {
          mask = blTrailingBitMask<uint32_t>(size) << shift;
          shift = 32 - size - shift;
        }

        d.masks[i] = mask;
        d.shifts[i] = uint8_t(shift);
      }

      switch (dstInfo.depth) {
        case 16:
          if (isPRGB)
            func = hostBO ? bl_convert_prgb_any_from_prgb32<BLPixelAccess16<BL_BYTE_ORDER_NATIVE>, BL_UNALIGNED_IO_16>
                          : bl_convert_prgb_any_from_prgb32<BLPixelAccess16<BL_BYTE_ORDER_SWAPPED>, BL_UNALIGNED_IO_16>;
          else if (isARGB)
            func = hostBO ? bl_convert_argb_any_from_prgb32<BLPixelAccess16<BL_BYTE_ORDER_NATIVE>, BL_UNALIGNED_IO_16>
                          : bl_convert_argb_any_from_prgb32<BLPixelAccess16<BL_BYTE_ORDER_SWAPPED>, BL_UNALIGNED_IO_16>;
          else
            func = hostBO ? bl_convert_xrgb_any_from_xrgb32<BLPixelAccess16<BL_BYTE_ORDER_NATIVE>, BL_UNALIGNED_IO_16>
                          : bl_convert_xrgb_any_from_xrgb32<BLPixelAccess16<BL_BYTE_ORDER_SWAPPED>, BL_UNALIGNED_IO_16>;
          break;

        case 24:
          if (isPRGB)
            func = hostBO ? bl_convert_prgb_any_from_prgb32<BLPixelAccess24<BL_BYTE_ORDER_NATIVE>, true>
                          : bl_convert_prgb_any_from_prgb32<BLPixelAccess24<BL_BYTE_ORDER_SWAPPED>, true>;
          else if (isARGB)
            func = hostBO ? bl_convert_argb_any_from_prgb32<BLPixelAccess24<BL_BYTE_ORDER_NATIVE>, true>
                          : bl_convert_argb_any_from_prgb32<BLPixelAccess24<BL_BYTE_ORDER_SWAPPED>, true>;
          else
            func = hostBO ? bl_convert_xrgb_any_from_xrgb32<BLPixelAccess24<BL_BYTE_ORDER_NATIVE>, true>
                          : bl_convert_xrgb_any_from_xrgb32<BLPixelAccess24<BL_BYTE_ORDER_SWAPPED>, true>;
          break;

        case 32:
          if (isPRGB)
            func = hostBO ? bl_convert_prgb_any_from_prgb32<BLPixelAccess32<BL_BYTE_ORDER_NATIVE>, BL_UNALIGNED_IO_32>
                          : bl_convert_prgb_any_from_prgb32<BLPixelAccess32<BL_BYTE_ORDER_SWAPPED>, BL_UNALIGNED_IO_32>;
          else if (isARGB)
            func = hostBO ? bl_convert_argb_any_from_prgb32<BLPixelAccess32<BL_BYTE_ORDER_NATIVE>, BL_UNALIGNED_IO_32>
                          : bl_convert_argb_any_from_prgb32<BLPixelAccess32<BL_BYTE_ORDER_SWAPPED>, BL_UNALIGNED_IO_32>;
          else
            func = hostBO ? bl_convert_xrgb_any_from_xrgb32<BLPixelAccess32<BL_BYTE_ORDER_NATIVE>, BL_UNALIGNED_IO_32>
                          : bl_convert_xrgb_any_from_xrgb32<BLPixelAccess32<BL_BYTE_ORDER_SWAPPED>, BL_UNALIGNED_IO_32>;
          break;

        default:
          return blTraceError(BL_ERROR_INVALID_VALUE);
      }

      self->convertFunc = func;
      return BL_SUCCESS;
    }
  }

  // --------------------------------------------------------------------------
  // [External <- External]
  // --------------------------------------------------------------------------

  // We have non-native pixel formats on input and output. This means that we
  // will create two converters and convert through a native pixel format as
  // otherwise there would be a lot of combinations that we would have to handle.

  // TODO:

  // --------------------------------------------------------------------------
  // [Invalid]
  // --------------------------------------------------------------------------

  return blTraceError(BL_ERROR_INVALID_VALUE);
}

// ============================================================================
// [BLPixelConverter - Init / Reset]
// ============================================================================

BLResult blPixelConverterInit(BLPixelConverterCore* self) noexcept {
  memset(self, 0, sizeof(BLPixelConverterCore));
  return BL_SUCCESS;
}

BLResult blPixelConverterInitWeak(BLPixelConverterCore* self, const BLPixelConverterCore* other) noexcept {
  memcpy(self, other, sizeof(BLPixelConverterCore));
  return BL_SUCCESS;
}

BLResult blPixelConverterReset(BLPixelConverterCore* self) noexcept {
  memset(self, 0, sizeof(BLPixelConverterCore));
  return BL_SUCCESS;
}

// ============================================================================
// [BLPixelConverter - Assign]
// ============================================================================

BLResult blPixelConverterAssign(BLPixelConverterCore* self, const BLPixelConverterCore* other) noexcept {
  memcpy(self, other, sizeof(BLPixelConverterCore));
  return BL_SUCCESS;
}

// ============================================================================
// [BLPixelConverter - Create]
// ============================================================================

BLResult blPixelConverterCreate(BLPixelConverterCore* self, const BLFormatInfo* dstInfo, const BLFormatInfo* srcInfo) noexcept {
  BLFormatInfo dstSanitized = *dstInfo;
  BLFormatInfo srcSanitized = *srcInfo;

  BL_PROPAGATE(dstSanitized.sanitize());
  BL_PROPAGATE(srcSanitized.sanitize());

  // Always create a new one and then swap it if the initialization succeeded.
  BLPixelConverterCore pc {};
  BL_PROPAGATE(blPixelConverterInitInternal(&pc, dstSanitized, srcSanitized));

  blPixelConverterReset(self);
  memcpy(self, &pc, sizeof(BLPixelConverterCore));
  return BL_SUCCESS;
}

// ============================================================================
// [BLPixelConverter - Convert]
// ============================================================================

BLResult blPixelConverterConvert(const BLPixelConverterCore* self,
  void* dstData, intptr_t dstStride,
  const void* srcData, intptr_t srcStride,
  uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return self->convertFunc(self,
    static_cast<      uint8_t*>(dstData), dstStride,
    static_cast<const uint8_t*>(srcData), srcStride, w, h, options);
}

// ============================================================================
// [BLPixelConverter - Unit Tests]
// ============================================================================

#ifdef BL_BUILD_TEST
template<typename T>
struct BLPixelConverterUnit {
  static void fillMasks(BLFormatInfo& fi) noexcept {
    fi.shifts[0] = T::kR ? blBitCtz(T::kR) : 0;
    fi.shifts[1] = T::kG ? blBitCtz(T::kG) : 0;
    fi.shifts[2] = T::kB ? blBitCtz(T::kB) : 0;
    fi.shifts[3] = T::kA ? blBitCtz(T::kA) : 0;
    fi.sizes[0] = T::kR ? blBitCtz(~(T::kR >> fi.shifts[0])) : 0;
    fi.sizes[1] = T::kG ? blBitCtz(~(T::kG >> fi.shifts[1])) : 0;
    fi.sizes[2] = T::kB ? blBitCtz(~(T::kB >> fi.shifts[2])) : 0;
    fi.sizes[3] = T::kA ? blBitCtz(~(T::kA >> fi.shifts[3])) : 0;
  }

  static void testPrgb32() noexcept {
    INFO("Testing %dbpp %s format", T::kDepth, T::formatString());

    BLPixelConverter from;
    BLPixelConverter back;

    BLFormatInfo fi {};
    fillMasks(fi);
    fi.depth = T::kDepth;
    fi.flags = fi.sizes[3] ? BL_FORMAT_FLAG_RGBA | BL_FORMAT_FLAG_PREMULTIPLIED : BL_FORMAT_FLAG_RGB;

    EXPECT(from.create(fi, blFormatInfo[BL_FORMAT_PRGB32]) == BL_SUCCESS, "%s: Failed to create from [%dbpp 0x%08X 0x%08X 0x%08X 0x%08X]", T::formatString(), T::kDepth, T::kR, T::kG, T::kB, T::kA);
    EXPECT(back.create(blFormatInfo[BL_FORMAT_PRGB32], fi) == BL_SUCCESS, "%s: Failed to create to [%dbpp 0x%08X 0x%08X 0x%08X 0x%08X]", T::formatString(), T::kDepth, T::kR, T::kG, T::kB, T::kA);

    enum : uint32_t { kCount = 8 };

    static const uint32_t src[kCount] = {
      0xFF000000, 0xFF0000FF, 0xFF00FF00, 0xFF00FFFF,
      0xFFFF0000, 0xFFFF00FF, 0xFFFFFF00, 0xFFFFFFFF
    };

    uint32_t dst[kCount];
    uint8_t buf[kCount * 16];

    // The test is rather basic now, we basically convert from PRGB to external
    // pixel format, then back, and then compare if the output is matching input.
    // In the future we should also check the intermediate result.
    from.convertSpan(buf, src, kCount);
    back.convertSpan(dst, buf, kCount);

    for (uint32_t i = 0; i < kCount; i++) {
      uint32_t mid = 0;
      switch (T::kDepth) {
        case 8 : mid = blMemReadU8(buf + i); break;
        case 16: mid = blMemReadU16u(buf + i * 2u); break;
        case 24: mid = blMemReadU24u(buf + i * 3u); break;
        case 32: mid = blMemReadU32u(buf + i * 4u); break;
      }

      EXPECT(dst[i] == src[i],
        "%s: Dst(%08X) <- 0x%08X <- Src(0x%08X) [%dbpp %08X|%08X|%08X|%08X]",
        T::formatString(), dst[i], mid, src[i], T::kDepth, T::kA, T::kR, T::kG, T::kB);
    }
  }

  static void test() noexcept {
    testPrgb32();
  }
};

#define BL_PIXEL_TEST(FORMAT, DEPTH, R_MASK, G_MASK, B_MASK, A_MASK)      \
  struct Test_##FORMAT {                                                  \
    static inline const char* formatString() noexcept { return #FORMAT; } \
                                                                          \
    enum : uint32_t {                                                     \
      kDepth = DEPTH,                                                     \
      kR = R_MASK,                                                        \
      kG = G_MASK,                                                        \
      kB = B_MASK,                                                        \
      kA = A_MASK                                                         \
    };                                                                    \
  }

BL_PIXEL_TEST(XRGB_0555, 16, 0x00007C00u, 0x000003E0u, 0x0000001Fu, 0x00000000u);
BL_PIXEL_TEST(XBGR_0555, 16, 0x0000001Fu, 0x000003E0u, 0x00007C00u, 0x00000000u);
BL_PIXEL_TEST(XRGB_0565, 16, 0x0000F800u, 0x000007E0u, 0x0000001Fu, 0x00000000u);
BL_PIXEL_TEST(XBGR_0565, 16, 0x0000001Fu, 0x000007E0u, 0x0000F800u, 0x00000000u);
BL_PIXEL_TEST(ARGB_4444, 16, 0x00000F00u, 0x000000F0u, 0x0000000Fu, 0x0000F000u);
BL_PIXEL_TEST(ABGR_4444, 16, 0x0000000Fu, 0x000000F0u, 0x00000F00u, 0x0000F000u);
BL_PIXEL_TEST(RGBA_4444, 16, 0x0000F000u, 0x00000F00u, 0x000000F0u, 0x0000000Fu);
BL_PIXEL_TEST(BGRA_4444, 16, 0x000000F0u, 0x00000F00u, 0x0000F000u, 0x0000000Fu);
BL_PIXEL_TEST(XRGB_0888, 24, 0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0x00000000u);
BL_PIXEL_TEST(XBGR_0888, 24, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0x00000000u);
BL_PIXEL_TEST(XRGB_8888, 32, 0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0x00000000u);
BL_PIXEL_TEST(XBGR_8888, 32, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0x00000000u);
BL_PIXEL_TEST(RGBX_8888, 32, 0xFF000000u, 0x00FF0000u, 0x0000FF00u, 0x00000000u);
BL_PIXEL_TEST(BGRX_8888, 32, 0x0000FF00u, 0x00FF0000u, 0xFF000000u, 0x00000000u);
BL_PIXEL_TEST(ARGB_8888, 32, 0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
BL_PIXEL_TEST(ABGR_8888, 32, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0xFF000000u);
BL_PIXEL_TEST(RGBA_8888, 32, 0xFF000000u, 0x00FF0000u, 0x0000FF00u, 0x000000FFu);
BL_PIXEL_TEST(BGRA_8888, 32, 0x0000FF00u, 0x00FF0000u, 0xFF000000u, 0x000000FFu);
BL_PIXEL_TEST(BRGA_8888, 32, 0x00FF0000u, 0x0000FF00u, 0xFF000000u, 0x000000FFu);

#undef BL_PIXEL_TEST

UNIT(blend2d_pixel_converter) {
  BLPixelConverterUnit<Test_XRGB_0555>::test();
  BLPixelConverterUnit<Test_XBGR_0555>::test();
  BLPixelConverterUnit<Test_XRGB_0565>::test();
  BLPixelConverterUnit<Test_XBGR_0565>::test();
  BLPixelConverterUnit<Test_ARGB_4444>::test();
  BLPixelConverterUnit<Test_ABGR_4444>::test();
  BLPixelConverterUnit<Test_RGBA_4444>::test();
  BLPixelConverterUnit<Test_BGRA_4444>::test();
  BLPixelConverterUnit<Test_XRGB_0888>::test();
  BLPixelConverterUnit<Test_XBGR_0888>::test();
  BLPixelConverterUnit<Test_XRGB_8888>::test();
  BLPixelConverterUnit<Test_XBGR_8888>::test();
  BLPixelConverterUnit<Test_RGBX_8888>::test();
  BLPixelConverterUnit<Test_BGRX_8888>::test();
  BLPixelConverterUnit<Test_ARGB_8888>::test();
  BLPixelConverterUnit<Test_ABGR_8888>::test();
  BLPixelConverterUnit<Test_RGBA_8888>::test();
  BLPixelConverterUnit<Test_BGRA_8888>::test();
  BLPixelConverterUnit<Test_BRGA_8888>::test();
}
#endif
