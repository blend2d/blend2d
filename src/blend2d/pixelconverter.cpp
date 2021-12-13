// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "api-impl.h"
#include "format_p.h"
#include "image.h"
#include "pixelconverter_p.h"
#include "runtime_p.h"
#include "tables_p.h"
#include "pixelops/scalar_p.h"
#include "support/intops_p.h"
#include "support/memops_p.h"
#include "support/ptrops_p.h"

#ifdef BL_TEST
  #include "random.h"
#endif

// PixelConverter - Globals
// ========================

const BLPixelConverterOptions blPixelConverterDefaultOptions {};

// PixelConverter - Tables
// =======================

// A table that contains shifts of native 32-bit pixel format. The only reason to have this in a table is a fact that
// a blue component is shifted by 8 (the same as green) to be at the right place, because there is no way to calculate
// the constants of component that has to stay within the low 8 bits as `scale` value is calculated by doubling the
// size until it reaches the required depth, so for example depth of 5 would scale to 10, depth 3 would scale to 9,
// and depths 1-2 would scale to 8.
static constexpr const uint8_t blPixelConverterNative32FromForeignShiftTable[] = {
  16, // [0x00FF0000] R.
  8 , // [0x0000FF00] G.
  8 , // [0x0000FF00] B (shift to right by 8 to get the desired result).
  24  // [0xFF000000] A.
};

// PixelConverter - Uninitialized
// ==============================

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

static BLResult BL_CDECL bl_convert_func_not_initialized(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcLine, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return blTraceError(BL_ERROR_NOT_INITIALIZED);
}

BL_DIAGNOSTIC_POP

// PixelConverter - Utilities
// ==========================

static BL_INLINE bool blPixelConverterIsIndexedDepth(uint32_t depth) noexcept {
  return depth == 1 || depth == 2 || depth == 4 || depth == 8;
}

static bool blPixelConverterPaletteFormatFromFormatFlags(BLFormatInfo& fi, uint32_t flags) noexcept {
  // `fi` is now ARGB32 (non-premultiplied).
  fi = blFormatInfo[BL_FORMAT_PRGB32];
  fi.flags &= ~BL_FORMAT_FLAG_PREMULTIPLIED;

  switch (flags & BL_FORMAT_FLAG_RGBA) {
    case BL_FORMAT_FLAG_ALPHA:
      return true;

    case BL_FORMAT_FLAG_RGB:
      fi.flags &= ~BL_FORMAT_FLAG_ALPHA;
      fi.sizes[3] = 0;
      fi.shifts[3] = 0;
      return true;

    case BL_FORMAT_FLAG_RGBA:
      fi.flags |= flags & BL_FORMAT_FLAG_PREMULTIPLIED;
      return true;

    default:
      return false;
  }
}

// PixelConverter - Memory Management
// ==================================

static BL_INLINE void blPixelConverterZeroInitialize(BLPixelConverterCore* self) noexcept {
  memset(self, 0, sizeof(BLPixelConverterCore));
  self->convertFunc = bl_convert_func_not_initialized;
}

static BL_INLINE void blPixelConverterAddRef(BLPixelConverterCore* self) noexcept {
  BLPixelConverterData* d = blPixelConverterGetData(self);
  if (!(d->internalFlags & BL_PIXEL_CONVERTER_INTERNAL_FLAG_DYNAMIC_DATA))
    return;

  blAtomicFetchAdd(d->refCount);
}

static void blPixelConverterRelease(BLPixelConverterCore* self) noexcept {
  BLPixelConverterData* d = blPixelConverterGetData(self);

  uint32_t flags = d->internalFlags;
  if (!(flags & BL_PIXEL_CONVERTER_INTERNAL_FLAG_DYNAMIC_DATA))
    return;

  void* dataPtr = d->dataPtr;
  if (blAtomicFetchSub(d->refCount) == 1) {
    if (flags & BL_PIXEL_CONVERTER_INTERNAL_FLAG_MULTI_STEP) {
      BLPixelConverterMultiStepContext* ctx = static_cast<BLPixelConverterMultiStepContext*>(dataPtr);
      blPixelConverterReset(&ctx->first);
      blPixelConverterReset(&ctx->second);
    }
    free(dataPtr);
  }
}

static BL_INLINE void blPixelConverterCopyRef(BLPixelConverterCore* self, const BLPixelConverterCore* other) noexcept {
  memcpy(self, other, sizeof(BLPixelConverterCore));
  blPixelConverterAddRef(self);
}

// PixelConverter - Init & Destroy
// ===============================

BLResult blPixelConverterInit(BLPixelConverterCore* self) noexcept {
  blPixelConverterZeroInitialize(self);
  return BL_SUCCESS;
}

BLResult blPixelConverterInitWeak(BLPixelConverterCore* self, const BLPixelConverterCore* other) noexcept {
  blPixelConverterCopyRef(self, other);
  return BL_SUCCESS;
}

BLResult blPixelConverterDestroy(BLPixelConverterCore* self) noexcept {
  blPixelConverterRelease(self);
  self->convertFunc = nullptr;
  return BL_SUCCESS;
}

// PixelConverter - Reset
// ======================

BLResult blPixelConverterReset(BLPixelConverterCore* self) noexcept {
  blPixelConverterRelease(self);
  blPixelConverterZeroInitialize(self);
  return BL_SUCCESS;
}

// PixelConverter - Assign
// =======================

BLResult blPixelConverterAssign(BLPixelConverterCore* self, const BLPixelConverterCore* other) noexcept {
  if (self == other)
    return BL_SUCCESS;

  blPixelConverterRelease(self);
  blPixelConverterCopyRef(self, other);
  return BL_SUCCESS;
}

// PixelConverter - Create
// =======================

BLResult blPixelConverterCreate(BLPixelConverterCore* self, const BLFormatInfo* dstInfo, const BLFormatInfo* srcInfo, BLPixelConverterCreateFlags createFlags) noexcept {
  BLFormatInfo di = *dstInfo;
  BLFormatInfo si = *srcInfo;

  BL_PROPAGATE(di.sanitize());
  BL_PROPAGATE(si.sanitize());

  // Always create a new one and then swap it if the initialization succeeded.
  BLPixelConverterCore pc {};
  BL_PROPAGATE(blPixelConverterInitInternal(&pc, di, si, createFlags));

  blPixelConverterRelease(self);
  memcpy(self, &pc, sizeof(BLPixelConverterCore));
  return BL_SUCCESS;
}

// PixelConverter - Convert
// ========================

BLResult blPixelConverterConvert(const BLPixelConverterCore* self,
  void* dstData, intptr_t dstStride,
  const void* srcData, intptr_t srcStride,
  uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  return self->convertFunc(self, static_cast<      uint8_t*>(dstData), dstStride,
                                 static_cast<const uint8_t*>(srcData), srcStride, w, h, options);
}

// PixelConverter - Pixel Access
// =============================

struct BLPixelAccess8 {
  enum : uint32_t { kSize = 1 };

  static BL_INLINE uint32_t fetchA(const void* p) noexcept { return BLMemOps::readU8(p); }
  static BL_INLINE uint32_t fetchU(const void* p) noexcept { return BLMemOps::readU8(p); }

  static BL_INLINE void storeA(void* p, uint32_t v) noexcept { BLMemOps::writeU8(p, uint16_t(v)); }
  static BL_INLINE void storeU(void* p, uint32_t v) noexcept { BLMemOps::writeU8(p, uint16_t(v)); }
};

template<uint32_t ByteOrder>
struct BLPixelAccess16 {
  enum : uint32_t { kSize = 2 };

  static BL_INLINE uint32_t fetchA(const void* p) noexcept { return BLMemOps::readU16<ByteOrder, 2>(p); }
  static BL_INLINE uint32_t fetchU(const void* p) noexcept { return BLMemOps::readU16<ByteOrder, 1>(p); }

  static BL_INLINE void storeA(void* p, uint32_t v) noexcept { BLMemOps::writeU16<ByteOrder, 2>(p, uint16_t(v)); }
  static BL_INLINE void storeU(void* p, uint32_t v) noexcept { BLMemOps::writeU16<ByteOrder, 1>(p, uint16_t(v)); }
};

template<uint32_t ByteOrder>
struct BLPixelAccess24 {
  enum : uint32_t { kSize = 3 };

  static BL_INLINE uint32_t fetchA(const void* p) noexcept { return BLMemOps::readU24u<ByteOrder>(p); }
  static BL_INLINE uint32_t fetchU(const void* p) noexcept { return BLMemOps::readU24u<ByteOrder>(p); }

  static BL_INLINE void storeA(void* p, uint32_t v) noexcept { BLMemOps::writeU24u<ByteOrder>(p, v); }
  static BL_INLINE void storeU(void* p, uint32_t v) noexcept { BLMemOps::writeU24u<ByteOrder>(p, v); }
};

template<uint32_t ByteOrder>
struct BLPixelAccess32 {
  enum : uint32_t { kSize = 4 };

  static BL_INLINE uint32_t fetchA(const void* p) noexcept { return BLMemOps::readU32<ByteOrder, 4>(p); }
  static BL_INLINE uint32_t fetchU(const void* p) noexcept { return BLMemOps::readU32<ByteOrder, 1>(p); }

  static BL_INLINE void storeA(void* p, uint32_t v) noexcept { BLMemOps::writeU32<ByteOrder, 4>(p, v); }
  static BL_INLINE void storeU(void* p, uint32_t v) noexcept { BLMemOps::writeU32<ByteOrder, 1>(p, v); }
};

// PixelConverter - Copy
// =====================

BLResult bl_convert_copy(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t bytesPerPixel = blPixelConverterGetData(self)->memCopyData.bytesPerPixel;
  const size_t byteWidth = size_t(w) * bytesPerPixel;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(byteWidth) + gap;
  srcStride -= uintptr_t(byteWidth);

  for (uint32_t y = h; y != 0; y--) {
    size_t i = byteWidth;

    if (!BLMemOps::kUnalignedMem32 && (uintptr_t)dstData == (uintptr_t)srcData) {
      while (i && ((uintptr_t)dstData) & 0x03) {
        *dstData++ = *srcData++;
        i--;
      }

      while (i >= 16) {
        uint32_t p0 = BLMemOps::readU32a(srcData +  0);
        uint32_t p1 = BLMemOps::readU32a(srcData +  4);
        uint32_t p2 = BLMemOps::readU32a(srcData +  8);
        uint32_t p3 = BLMemOps::readU32a(srcData + 12);

        BLMemOps::writeU32a(dstData +  0, p0);
        BLMemOps::writeU32a(dstData +  4, p1);
        BLMemOps::writeU32a(dstData +  8, p2);
        BLMemOps::writeU32a(dstData + 12, p3);

        dstData += 16;
        srcData += 16;
        i -= 16;
      }

      while (i >= 4) {
        BLMemOps::writeU32a(dstData, BLMemOps::readU32a(srcData));
        dstData += 4;
        srcData += 4;
        i -= 4;
      }
    }
    else {
      while (i >= 16) {
        uint32_t p0 = BLMemOps::readU32u(srcData +  0);
        uint32_t p1 = BLMemOps::readU32u(srcData +  4);
        uint32_t p2 = BLMemOps::readU32u(srcData +  8);
        uint32_t p3 = BLMemOps::readU32u(srcData + 12);

        BLMemOps::writeU32u(dstData +  0, p0);
        BLMemOps::writeU32u(dstData +  4, p1);
        BLMemOps::writeU32u(dstData +  8, p2);
        BLMemOps::writeU32u(dstData + 12, p3);

        dstData += 16;
        srcData += 16;
        i -= 16;
      }

      while (i >= 4) {
        BLMemOps::writeU32u(dstData, BLMemOps::readU32u(srcData));
        dstData += 4;
        srcData += 4;
        i -= 4;
      }
    }

    while (i) {
      *dstData++ = *srcData++;
      i--;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// PixelConverter - Copy|Or
// ========================

BLResult bl_convert_copy_or_8888(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const uint32_t fillMask = blPixelConverterGetData(self)->memCopyData.fillMask;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * 4;

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;
    if (!BLMemOps::kUnalignedMem32 && (uintptr_t)dstData == (uintptr_t)srcData) {
      while (i >= 4) {
        uint32_t p0 = BLMemOps::readU32a(srcData +  0);
        uint32_t p1 = BLMemOps::readU32a(srcData +  4);
        uint32_t p2 = BLMemOps::readU32a(srcData +  8);
        uint32_t p3 = BLMemOps::readU32a(srcData + 12);

        BLMemOps::writeU32a(dstData +  0, p0 | fillMask);
        BLMemOps::writeU32a(dstData +  4, p1 | fillMask);
        BLMemOps::writeU32a(dstData +  8, p2 | fillMask);
        BLMemOps::writeU32a(dstData + 12, p3 | fillMask);

        dstData += 16;
        srcData += 16;
        i -= 4;
      }

      while (i) {
        BLMemOps::writeU32a(dstData, BLMemOps::readU32a(srcData) | fillMask);
        dstData += 4;
        srcData += 4;
        i--;
      }
    }
    else {
      while (i >= 4) {
        uint32_t p0 = BLMemOps::readU32u(srcData +  0);
        uint32_t p1 = BLMemOps::readU32u(srcData +  4);
        uint32_t p2 = BLMemOps::readU32u(srcData +  8);
        uint32_t p3 = BLMemOps::readU32u(srcData + 12);

        BLMemOps::writeU32u(dstData +  0, p0 | fillMask);
        BLMemOps::writeU32u(dstData +  4, p1 | fillMask);
        BLMemOps::writeU32u(dstData +  8, p2 | fillMask);
        BLMemOps::writeU32u(dstData + 12, p3 | fillMask);

        dstData += 16;
        srcData += 16;
        i -= 4;
      }

      while (i) {
        BLMemOps::writeU32u(dstData, BLMemOps::readU32u(srcData) | fillMask);
        dstData += 4;
        srcData += 4;
        i--;
      }
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// PixelConverter - Premultiply & Unpremultiply
// ============================================

static BLResult BL_CDECL bl_convert_premultiply_8888(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * 4;

  const BLPixelConverterData::PremultiplyData& d = blPixelConverterGetData(self)->premultiplyData;
  const uint32_t alphaShift = d.alphaShift;
  const uint32_t alphaMask = 0xFFu << alphaShift;
  const uint32_t fillMask = d.fillMask;

  for (uint32_t y = h; y != 0; y--) {
    if (!BLMemOps::kUnalignedMem32 && BLIntOps::isAligned((uintptr_t)dstData | (uintptr_t)srcData, 4)) {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = BLMemOps::readU32a(srcData);
        uint32_t a = (pix >> alphaShift) & 0xFFu;

        pix |= alphaMask;

        uint32_t c0 = ((pix     ) & 0x00FF00FFu) * a + 0x00800080u;
        uint32_t c1 = ((pix >> 8) & 0x00FF00FFu) * a + 0x00800080u;

        c0 = (c0 + ((c0 >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;
        c1 = (c1 + ((c1 >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;

        BLMemOps::writeU32a(dstData, (c0 >> 8) | c1 | fillMask);

        dstData += 4;
        srcData += 4;
      }
    }
    else {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = BLMemOps::readU32u(srcData);
        uint32_t a = (pix >> alphaShift) & 0xFFu;

        pix |= alphaMask;

        uint32_t c0 = ((pix     ) & 0x00FF00FFu) * a + 0x00800080u;
        uint32_t c1 = ((pix >> 8) & 0x00FF00FFu) * a + 0x00800080u;

        c0 = (c0 + ((c0 >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;
        c1 = (c1 + ((c1 >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;

        BLMemOps::writeU32u(dstData, (c0 >> 8) | c1 | fillMask);

        dstData += 4;
        srcData += 4;
      }
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

template<uint32_t A_Shift>
static BLResult BL_CDECL bl_convert_unpremultiply_8888(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  blUnused(self);

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * 4;

  const uint32_t R_Shift = (A_Shift +  8u) % 32u;
  const uint32_t G_Shift = (A_Shift + 16u) % 32u;
  const uint32_t B_Shift = (A_Shift + 24u) % 32u;

  for (uint32_t y = h; y != 0; y--) {
    if (!BLMemOps::kUnalignedMem32 && BLIntOps::isAligned((uintptr_t)dstData | (uintptr_t)srcData, 4)) {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = BLMemOps::readU32a(srcData);
        uint32_t r = (pix >> R_Shift) & 0xFFu;
        uint32_t g = (pix >> G_Shift) & 0xFFu;
        uint32_t b = (pix >> B_Shift) & 0xFFu;
        uint32_t a = (pix >> A_Shift) & 0xFFu;

        BLPixelOps::Scalar::unpremultiply_rgb_8bit(r, g, b, a);
        BLMemOps::writeU32a(dstData, (r << R_Shift) | (g << G_Shift) | (b << B_Shift) | (a << A_Shift));

        dstData += 4;
        srcData += 4;
      }
    }
    else {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = BLMemOps::readU32u(srcData);
        uint32_t r = (pix >> R_Shift) & 0xFFu;
        uint32_t g = (pix >> G_Shift) & 0xFFu;
        uint32_t b = (pix >> B_Shift) & 0xFFu;
        uint32_t a = (pix >> A_Shift) & 0xFFu;

        BLPixelOps::Scalar::unpremultiply_rgb_8bit(r, g, b, a);
        BLMemOps::writeU32u(dstData, (r << R_Shift) | (g << G_Shift) | (b << B_Shift) | (a << A_Shift));

        dstData += 4;
        srcData += 4;
      }
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// PixelConverter - A8 From PRGB32/ARGB32
// ======================================

BLResult bl_convert_a8_from_8888(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) + gap;
  srcStride -= uintptr_t(w) * 4;

  const BLPixelConverterData::X8FromRgb32Data& d = blPixelConverterGetData(self)->x8FromRgb32Data;
  const size_t srcBPP = d.bytesPerPixel;
  const size_t srcAI = d.alphaShift / 8u;

  srcData += srcAI;

  for (uint32_t y = h; y != 0; y--) {
    for (uint32_t i = w; i != 0; i--) {
      dstData[0] = srcData[0];
      dstData += 1;
      srcData += srcBPP;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// PixelConverter - RGB32 From A8/L8
// =================================

BLResult bl_convert_8888_from_x8(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w);

  const BLPixelConverterData::Rgb32FromX8Data& d = blPixelConverterGetData(self)->rgb32FromX8Data;
  const uint32_t fillMask = d.fillMask;
  const uint32_t zeroMask = d.zeroMask;

  for (uint32_t y = h; y != 0; y--) {
    if (!BLMemOps::kUnalignedMem32 && BLIntOps::isAligned(dstData, 4)) {
      for (uint32_t i = w; i != 0; i--) {
        BLMemOps::writeU32a(dstData, ((uint32_t(srcData[0]) * 0x01010101u) & zeroMask) | fillMask);
        dstData += 4;
        srcData += 1;
      }
    }
    else {
      for (uint32_t i = w; i != 0; i--) {
        BLMemOps::writeU32u(dstData, ((uint32_t(srcData[0]) * 0x01010101u) & zeroMask) | fillMask);
        dstData += 4;
        srcData += 1;
      }
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// PixelConverter - Any <- Indexed1
// ================================

// Instead of doing a table lookup each time we create a XOR mask that is used to get the second color value from
// the first one. This allows to remove the lookup completely. The only requirement is that we need all zeros or
// ones depending on the source value (see the implementation, it uses signed right shift to fill these bits in).

template<typename PixelAccess>
static BLResult BL_CDECL bl_convert_any_from_indexed1(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcLine, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const uint32_t kPixelSize = PixelAccess::kSize;
  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * kPixelSize + gap;

  const BLPixelConverterData::IndexedData& d = blPixelConverterGetData(self)->indexedData;
  uint32_t c0 = PixelAccess::fetchA(d.embedded.table8 + 0 * kPixelSize);
  uint32_t cm = PixelAccess::fetchA(d.embedded.table8 + 1 * kPixelSize) ^ c0; // XOR mask.

  if (c0 == 0 && cm == (0xFFFFFFFFu >> (32 - kPixelSize))) {
    // Special case for zeros and all ones.
    for (uint32_t y = h; y != 0; y--) {
      const uint8_t* srcData = srcLine;
      uint32_t i = w;

      while (i >= 8) {
        uint32_t b0 = uint32_t(*srcData++) << 24;
        uint32_t b1 = b0 << 1;

        PixelAccess::storeU(dstData + 0 * kPixelSize, BLIntOps::sar(b0, 31)); b0 <<= 2;
        PixelAccess::storeU(dstData + 1 * kPixelSize, BLIntOps::sar(b1, 31)); b1 <<= 2;
        PixelAccess::storeU(dstData + 2 * kPixelSize, BLIntOps::sar(b0, 31)); b0 <<= 2;
        PixelAccess::storeU(dstData + 3 * kPixelSize, BLIntOps::sar(b1, 31)); b1 <<= 2;
        PixelAccess::storeU(dstData + 4 * kPixelSize, BLIntOps::sar(b0, 31)); b0 <<= 2;
        PixelAccess::storeU(dstData + 5 * kPixelSize, BLIntOps::sar(b1, 31)); b1 <<= 2;
        PixelAccess::storeU(dstData + 6 * kPixelSize, BLIntOps::sar(b0, 31));
        PixelAccess::storeU(dstData + 7 * kPixelSize, BLIntOps::sar(b1, 31));

        dstData += 8 * kPixelSize;
        i -= 8;
      }

      if (i) {
        uint32_t b0 = uint32_t(*srcData++) << 24;
        do {
          PixelAccess::storeU(dstData, BLIntOps::sar(b0, 31));
          dstData += kPixelSize;
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

        PixelAccess::storeU(dstData + 0 * kPixelSize, c0 ^ (cm & BLIntOps::sar(b0, 31))); b0 <<= 2;
        PixelAccess::storeU(dstData + 1 * kPixelSize, c0 ^ (cm & BLIntOps::sar(b1, 31))); b1 <<= 2;
        PixelAccess::storeU(dstData + 2 * kPixelSize, c0 ^ (cm & BLIntOps::sar(b0, 31))); b0 <<= 2;
        PixelAccess::storeU(dstData + 3 * kPixelSize, c0 ^ (cm & BLIntOps::sar(b1, 31))); b1 <<= 2;
        PixelAccess::storeU(dstData + 4 * kPixelSize, c0 ^ (cm & BLIntOps::sar(b0, 31))); b0 <<= 2;
        PixelAccess::storeU(dstData + 5 * kPixelSize, c0 ^ (cm & BLIntOps::sar(b1, 31))); b1 <<= 2;
        PixelAccess::storeU(dstData + 6 * kPixelSize, c0 ^ (cm & BLIntOps::sar(b0, 31))); b0 <<= 2;
        PixelAccess::storeU(dstData + 7 * kPixelSize, c0 ^ (cm & BLIntOps::sar(b1, 31))); b1 <<= 2;

        dstData += 8 * kPixelSize;
        i -= 8;
      }

      if (i) {
        uint32_t b0 = uint32_t(*srcData++) << 24;
        do {
          PixelAccess::storeU(dstData, c0 ^ (cm & BLIntOps::sar(b0, 31)));
          dstData += kPixelSize;
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

// PixelConverter - Any <- Indexed2
// ================================

template<typename PixelAccess>
static BLResult BL_CDECL bl_convert_any_from_indexed2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcLine, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const uint32_t kPixelSize = PixelAccess::kSize;
  const uint32_t kShiftToLeadingByte = (BLIntOps::bitSizeOf<uintptr_t>() - 8);
  const uint32_t kShiftToTableIndex  = (BLIntOps::bitSizeOf<uintptr_t>() - 2);

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * kPixelSize + gap;

  const BLPixelConverterData::IndexedData& d = blPixelConverterGetData(self)->indexedData;
  const uint8_t* table = d.embedded.table8;

  for (uint32_t y = h; y != 0; y--) {
    const uint8_t* srcData = srcLine;
    uint32_t i = w;

    while (i >= 4) {
      uintptr_t b0 = uintptr_t(*srcData++) << kShiftToLeadingByte;

      uint32_t p0 = PixelAccess::fetchA(table + (b0 >> kShiftToTableIndex) * kPixelSize); b0 <<= 2;
      uint32_t p1 = PixelAccess::fetchA(table + (b0 >> kShiftToTableIndex) * kPixelSize); b0 <<= 2;
      uint32_t p2 = PixelAccess::fetchA(table + (b0 >> kShiftToTableIndex) * kPixelSize); b0 <<= 2;
      uint32_t p3 = PixelAccess::fetchA(table + (b0 >> kShiftToTableIndex) * kPixelSize);

      PixelAccess::storeU(dstData + 0 * kPixelSize, p0);
      PixelAccess::storeU(dstData + 1 * kPixelSize, p1);
      PixelAccess::storeU(dstData + 2 * kPixelSize, p2);
      PixelAccess::storeU(dstData + 3 * kPixelSize, p3);

      dstData += 4 * kPixelSize;
      i -= 4;
    }

    if (i) {
      uintptr_t b0 = uintptr_t(*srcData++) << kShiftToLeadingByte;
      do {
        PixelAccess::storeU(dstData, PixelAccess::fetchA(table + (b0 >> kShiftToTableIndex) * kPixelSize));
        dstData += kPixelSize;
        b0 <<= 2;
      } while (--i);
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcLine += srcStride;
  }

  return BL_SUCCESS;
}

// PixelConverter - Any <- Indexed4
// ================================

template<typename PixelAccess>
static BLResult BL_CDECL bl_convert_any_from_indexed4(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcLine, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const uint32_t kPixelSize = PixelAccess::kSize;

  const BLPixelConverterData::IndexedData& d = blPixelConverterGetData(self)->indexedData;
  const uint8_t* table = d.embedded.table8;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * kPixelSize + gap;

  for (uint32_t y = h; y != 0; y--) {
    const uint8_t* srcData = srcLine;
    uint32_t i = w;

    while (i >= 2) {
      uintptr_t b0 = *srcData++;

      uint32_t p0 = PixelAccess::fetchA(table + (b0 >> 4) * kPixelSize);
      uint32_t p1 = PixelAccess::fetchA(table + (b0 & 15) * kPixelSize);

      PixelAccess::storeU(dstData + 0 * kPixelSize, p0);
      PixelAccess::storeU(dstData + 1 * kPixelSize, p1);

      dstData += 2 * kPixelSize;
      i -= 2;
    }

    if (i) {
      uintptr_t b0 = srcData[0];
      PixelAccess::storeU(dstData, PixelAccess::fetchA(table + (b0 >> 4) * kPixelSize));
      dstData += kPixelSize;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcLine += srcStride;
  }

  return BL_SUCCESS;
}

// PixelConverter - Any <- Indexed8
// ================================

// Special case - used when no copy of the palette is required.
static BLResult BL_CDECL bl_convert_a8_from_indexed8_pal32(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) + gap;
  srcStride -= uintptr_t(w);

  const BLPixelConverterData::IndexedData& d = blPixelConverterGetData(self)->indexedData;
  const uint32_t* table = d.dynamic.table32;

  for (uint32_t y = h; y != 0; y--) {
    for (uint32_t i = w; i != 0; i--) {
      uintptr_t b0 = *srcData++;
      *dstData++ = uint8_t(table[b0] >> 24);
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

template<typename PixelAccess>
static BLResult BL_CDECL bl_convert_any_from_indexed8(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const uint32_t kPixelSize = PixelAccess::kSize;
  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * kPixelSize + gap;
  srcStride -= uintptr_t(w);

  const BLPixelConverterData::IndexedData& d = blPixelConverterGetData(self)->indexedData;
  const uint8_t* table = d.dynamic.table8;

  for (uint32_t y = h; y != 0; y--) {
    for (uint32_t i = w; i != 0; i--) {
      uintptr_t b0 = *srcData++;
      PixelAccess::storeU(dstData, PixelAccess::fetchA(table + b0 * kPixelSize));
      dstData += kPixelSize;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// PixelConverter - ByteShuffle
// ============================

// TODO:

// PixelConverter - Native32 <- XRGB|ARGB|PRGB
// ===========================================

template<typename PixelAccess, bool AlwaysUnaligned>
static BLResult BL_CDECL bl_convert_xrgb32_from_xrgb_any(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * PixelAccess::kSize;

  const BLPixelConverterData::NativeFromForeign& d = blPixelConverterGetData(self)->nativeFromForeign;
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
    if (!AlwaysUnaligned && BLIntOps::isAligned(srcData, PixelAccess::kSize)) {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = PixelAccess::fetchA(srcData);
        uint32_t r = (((pix >> rShift) & rMask) * rScale) & 0x00FF0000u;
        uint32_t g = (((pix >> gShift) & gMask) * gScale) & 0x0000FF00u;
        uint32_t b = (((pix >> bShift) & bMask) * bScale) >> 8;

        BLMemOps::writeU32a(dstData, r | g | b | fillMask);

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

        BLMemOps::writeU32a(dstData, r | g | b | fillMask);

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

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * PixelAccess::kSize;

  const BLPixelConverterData::NativeFromForeign& d = blPixelConverterGetData(self)->nativeFromForeign;
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
    if (!AlwaysUnaligned && BLIntOps::isAligned(srcData, PixelAccess::kSize)) {
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
        BLMemOps::writeU32a(dstData, ag + rb);

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
        BLMemOps::writeU32a(dstData, ag | rb);

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

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * PixelAccess::kSize;

  const BLPixelConverterData::NativeFromForeign& d = blPixelConverterGetData(self)->nativeFromForeign;
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
    if (!AlwaysUnaligned && BLIntOps::isAligned(srcData, PixelAccess::kSize)) {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = PixelAccess::fetchA(srcData);
        uint32_t r = ((pix >> rShift) & rMask) * rScale;
        uint32_t g = ((pix >> gShift) & gMask) * gScale;
        uint32_t b = ((pix >> bShift) & bMask) * bScale;
        uint32_t a = ((pix >> aShift) & aMask) * aScale;

        uint32_t ag = (a + (g     )) & 0xFF00FF00u;
        uint32_t rb = (r + (b >> 8)) & 0x00FF00FFu;

        BLMemOps::writeU32a(dstData, ag | rb);

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

        BLMemOps::writeU32a(dstData, ag | rb);

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
static BLResult BL_CDECL bl_convert_argb32_from_prgb_any(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * 4 + gap;
  srcStride -= uintptr_t(w) * PixelAccess::kSize;

  const BLPixelConverterData::NativeFromForeign& d = blPixelConverterGetData(self)->nativeFromForeign;
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
    if (!AlwaysUnaligned && BLIntOps::isAligned(srcData, PixelAccess::kSize)) {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = PixelAccess::fetchA(srcData);
        uint32_t r = (((pix >> rShift) & rMask) * rScale) >> 16;
        uint32_t g = (((pix >> gShift) & gMask) * gScale) >> 8;
        uint32_t b = (((pix >> bShift) & bMask) * bScale) >> 8;
        uint32_t a = (((pix >> aShift) & aMask) * aScale) >> 24;

        BLPixelOps::Scalar::unpremultiply_rgb_8bit(r, g, b, a);
        BLMemOps::writeU32a(dstData, (a << 24) | (r << 16) | (g << 8) | b);

        dstData += 4;
        srcData += PixelAccess::kSize;
      }
    }
    else {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = PixelAccess::fetchU(srcData);
        uint32_t r = (((pix >> rShift) & rMask) * rScale) >> 16;
        uint32_t g = (((pix >> gShift) & gMask) * gScale) >> 8;
        uint32_t b = (((pix >> bShift) & bMask) * bScale) >> 8;
        uint32_t a = (((pix >> aShift) & aMask) * aScale) >> 24;

        BLPixelOps::Scalar::unpremultiply_rgb_8bit(r, g, b, a);
        BLMemOps::writeU32a(dstData, (a << 24) | (r << 16) | (g << 8) | b);

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

// PixelConverter - XRGB|ARGB|PRGB <- Native32
// ===========================================

template<typename PixelAccess, bool AlwaysUnaligned>
static BLResult BL_CDECL bl_convert_xrgb_any_from_xrgb32(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  if (!options)
    options = &blPixelConverterDefaultOptions;

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * PixelAccess::kSize + gap;
  srcStride -= uintptr_t(w) * 4;

  const BLPixelConverterData::ForeignFromNative& d = blPixelConverterGetData(self)->foreignFromNative;
  uint32_t rMask = d.masks[0];
  uint32_t gMask = d.masks[1];
  uint32_t bMask = d.masks[2];

  uint32_t rShift = d.shifts[0];
  uint32_t gShift = d.shifts[1];
  uint32_t bShift = d.shifts[2];

  uint32_t fillMask = d.fillMask;

  for (uint32_t y = h; y != 0; y--) {
    if (!AlwaysUnaligned && BLIntOps::isAligned(dstData, PixelAccess::kSize)) {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = BLMemOps::readU32a(srcData);

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
        uint32_t pix = BLMemOps::readU32u(srcData);

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

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * PixelAccess::kSize + gap;
  srcStride -= uintptr_t(w) * 4;

  const BLPixelConverterData::ForeignFromNative& d = blPixelConverterGetData(self)->foreignFromNative;
  uint32_t rMask = d.masks[0];
  uint32_t gMask = d.masks[1];
  uint32_t bMask = d.masks[2];
  uint32_t aMask = d.masks[3];

  uint32_t rShift = d.shifts[0];
  uint32_t gShift = d.shifts[1];
  uint32_t bShift = d.shifts[2];
  uint32_t aShift = d.shifts[3];

  const uint32_t* unpremultiplyRcp = blCommonTable.unpremultiplyRcp;

  for (uint32_t y = h; y != 0; y--) {
    if (!AlwaysUnaligned && BLIntOps::isAligned(dstData, PixelAccess::kSize)) {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = BLMemOps::readU32a(srcData);

        uint32_t a = pix >> 24;
        uint32_t rcp = unpremultiplyRcp[a];

        uint32_t r = ((((pix >> 16) & 0xFFu) * rcp + 0x8000u) >> 16) * 0x01010101u;
        uint32_t g = ((((pix >>  8) & 0xFFu) * rcp + 0x8000u) >> 16) * 0x01010101u;
        uint32_t b = ((((pix      ) & 0xFFu) * rcp + 0x8000u) >> 16) * 0x01010101u;

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
        uint32_t pix = BLMemOps::readU32u(srcData);

        uint32_t a = pix >> 24;
        uint32_t rcp = unpremultiplyRcp[a];

        uint32_t r = ((((pix >> 16) & 0xFFu) * rcp + 0x8000u) >> 16) * 0x01010101u;
        uint32_t g = ((((pix >>  8) & 0xFFu) * rcp + 0x8000u) >> 16) * 0x01010101u;
        uint32_t b = ((((pix      ) & 0xFFu) * rcp + 0x8000u) >> 16) * 0x01010101u;

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

  const size_t gap = options->gap;
  dstStride -= uintptr_t(w) * PixelAccess::kSize + gap;
  srcStride -= uintptr_t(w) * 4;

  const BLPixelConverterData::ForeignFromNative& d = blPixelConverterGetData(self)->foreignFromNative;
  uint32_t rMask = d.masks[0];
  uint32_t gMask = d.masks[1];
  uint32_t bMask = d.masks[2];
  uint32_t aMask = d.masks[3];

  uint32_t rShift = d.shifts[0];
  uint32_t gShift = d.shifts[1];
  uint32_t bShift = d.shifts[2];
  uint32_t aShift = d.shifts[3];

  for (uint32_t y = h; y != 0; y--) {
    if (!AlwaysUnaligned && BLIntOps::isAligned(dstData, PixelAccess::kSize)) {
      for (uint32_t i = w; i != 0; i--) {
        uint32_t pix = BLMemOps::readU32a(srcData);

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
        uint32_t pix = BLMemOps::readU32u(srcData);

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

// PixelConverter - Init - Utilities
// =================================

static BL_INLINE BLResult blPixelConverterInitFuncC(BLPixelConverterCore* self, BLPixelConverterFunc func, uint32_t flags = 0) noexcept {
  self->convertFunc = func;
  self->internalFlags = uint8_t(flags | BL_PIXEL_CONVERTER_INTERNAL_FLAG_INITIALIZED);
  return BL_SUCCESS;
}

static BL_INLINE BLResult blPixelConverterInitFuncOpt(BLPixelConverterCore* self, BLPixelConverterFunc func, uint32_t flags = 0) noexcept {
  self->convertFunc = func;
  self->internalFlags = uint8_t(flags | BL_PIXEL_CONVERTER_INTERNAL_FLAG_INITIALIZED |
                                        BL_PIXEL_CONVERTER_INTERNAL_FLAG_OPTIMIZED);
  return BL_SUCCESS;
}

static uint32_t blPixelConverterCalcRgbMask32(const BLFormatInfo& fmtInfo) noexcept {
  uint32_t mask = 0;
  for (uint32_t i = 0; i < 3; i++)
    if (fmtInfo.sizes[i])
      mask |= BLIntOps::nonZeroLsbMask<uint32_t>(fmtInfo.sizes[i]) << fmtInfo.shifts[i];
  return mask;
}

static uint32_t blPixelConverterCalcFillMask32(const BLFormatInfo& fmtInfo) noexcept {
  uint32_t mask = 0;
  for (uint32_t i = 0; i < 4; i++)
    if (fmtInfo.sizes[i])
      mask |= BLIntOps::nonZeroLsbMask<uint32_t>(fmtInfo.sizes[i]) << fmtInfo.shifts[i];
  return ~mask;
}

static void blPixelConverterCalcPshufbPredicate32From24(uint32_t out[4], const BLFormatInfo& dstInfo, const BLFormatInfo& srcInfo) noexcept {
  BL_ASSERT(dstInfo.depth == 32);
  BL_ASSERT(srcInfo.depth == 24);

  BL_ASSERT(dstInfo.flags & BL_FORMAT_FLAG_BYTE_ALIGNED);
  BL_ASSERT(srcInfo.flags & BL_FORMAT_FLAG_BYTE_ALIGNED);

  uint32_t rIndex = uint32_t(srcInfo.shifts[0]) / 8u;
  uint32_t gIndex = uint32_t(srcInfo.shifts[1]) / 8u;
  uint32_t bIndex = uint32_t(srcInfo.shifts[2]) / 8u;

  uint32_t predicate = 0x80808080u;
  predicate ^= (0x80u ^ rIndex) << dstInfo.shifts[0];
  predicate ^= (0x80u ^ gIndex) << dstInfo.shifts[1];
  predicate ^= (0x80u ^ bIndex) << dstInfo.shifts[2];

  uint32_t increment = (0x03u << dstInfo.shifts[0]) |
                       (0x03u << dstInfo.shifts[1]) |
                       (0x03u << dstInfo.shifts[2]) ;

  out[0] = predicate; predicate += increment;
  out[1] = predicate; predicate += increment;
  out[2] = predicate; predicate += increment;
  out[3] = predicate;
}

static void blPixelConverterCalcPshufbPredicate32From32(uint32_t out[4], const BLFormatInfo& dstInfo, const BLFormatInfo& srcInfo) noexcept {
  BL_ASSERT(dstInfo.depth == 32);
  BL_ASSERT(srcInfo.depth == 32);

  BL_ASSERT(dstInfo.flags & BL_FORMAT_FLAG_BYTE_ALIGNED);
  BL_ASSERT(srcInfo.flags & BL_FORMAT_FLAG_BYTE_ALIGNED);

  uint32_t rIndex = uint32_t(srcInfo.shifts[0]) / 8u;
  uint32_t gIndex = uint32_t(srcInfo.shifts[1]) / 8u;
  uint32_t bIndex = uint32_t(srcInfo.shifts[2]) / 8u;
  uint32_t aIndex = uint32_t(srcInfo.shifts[3]) / 8u;

  uint32_t predicate = 0x80808080u;
  predicate ^= (0x80u ^ rIndex) << dstInfo.shifts[0];
  predicate ^= (0x80u ^ gIndex) << dstInfo.shifts[1];
  predicate ^= (0x80u ^ bIndex) << dstInfo.shifts[2];

  uint32_t increment = (0x04u << dstInfo.shifts[0]) |
                       (0x04u << dstInfo.shifts[1]) |
                       (0x04u << dstInfo.shifts[2]) ;

  if (srcInfo.sizes[3] != 0 && dstInfo.sizes[3] != 0) {
    predicate ^= (0x80u ^ aIndex) << dstInfo.shifts[3];
    increment |= (0x04u         ) << dstInfo.shifts[3];
  }

  out[0] = predicate; predicate += increment;
  out[1] = predicate; predicate += increment;
  out[2] = predicate; predicate += increment;
  out[3] = predicate;
}

// PixelConverter - Init - Indexed
// ===============================

static BLResult blPixelConverterInitIndexed(BLPixelConverterCore* self, const BLFormatInfo& di, const BLFormatInfo& si, BLPixelConverterCreateFlags createFlags) noexcept {
  BLPixelConverterData::IndexedData& d = blPixelConverterGetData(self)->indexedData;

  // Bail if the source depth doesn't match any supported one.
  if (BL_UNLIKELY(!blPixelConverterIsIndexedDepth(si.depth)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLFormatInfo paletteFormatInfo;
  if (BL_UNLIKELY(!blPixelConverterPaletteFormatFromFormatFlags(paletteFormatInfo, si.flags)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  bool dontCopyPalette = (createFlags & BL_PIXEL_CONVERTER_CREATE_FLAG_DONT_COPY_PALETTE) != 0;
  bool alterablePalette = (createFlags & BL_PIXEL_CONVERTER_CREATE_FLAG_ALTERABLE_PALETTE) != 0;

  // Special case - avoid making the copy of the palette for known conversions.
  if (di.depth == 8 && si.depth == 8 && dontCopyPalette) {
    if ((di.flags & (BL_FORMAT_FLAG_RGBA | BL_FORMAT_FLAG_BYTE_ALIGNED)) == (BL_FORMAT_FLAG_ALPHA | BL_FORMAT_FLAG_BYTE_ALIGNED)) {
      d.convertFunc = bl_convert_a8_from_indexed8_pal32;
      d.internalFlags = BL_PIXEL_CONVERTER_INTERNAL_FLAG_INITIALIZED;
      d.dynamic.table = si.palette;
      return BL_SUCCESS;
    }
  }

  // We need a temporary pixel converter to convert the palette to the destination pixel format. This operation should
  // not allocate any memory as the converter will convert native pixel format (BLRgba32) into a possibly non-native
  // one although a native pixel format is used most of the time.
  BLPixelConverterCore palCvt;
  BL_PROPAGATE(blPixelConverterInitInternal(&palCvt, di, paletteFormatInfo, BL_PIXEL_CONVERTER_CREATE_NO_FLAGS));

  // If the source depth is 8 bits it means that we either use the source format's palette or make a copy of it
  // depending on `createFlags` and the destination format as well.
  void* palette = nullptr;
  uint32_t paletteSize = 1u << si.depth;
  uint32_t paletteSizeInBytes = paletteSize * (di.depth / 8u);
  uint32_t internalFlags = BL_PIXEL_CONVERTER_INTERNAL_FLAG_INITIALIZED;

  if (paletteSizeInBytes > sizeof(d.embedded.table8)) {
    if (dontCopyPalette && (palCvt.internalFlags & BL_PIXEL_CONVERTER_INTERNAL_FLAG_RAW_COPY || alterablePalette)) {
      palette = si.palette;
      d.dynamic.table = palette;
    }
    else {
      palette = malloc(paletteSizeInBytes + sizeof(size_t));
      internalFlags |= BL_PIXEL_CONVERTER_INTERNAL_FLAG_DYNAMIC_DATA;

      if (!palette) {
        blPixelConverterReset(&palCvt);
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);
      }
    }
  }
  else {
    palette = d.embedded.table8;
  }

  palCvt.convertFunc(&palCvt, static_cast<uint8_t*>(palette), 0, reinterpret_cast<const uint8_t*>(si.palette), 0, paletteSize, 0, nullptr);
  blPixelConverterReset(&palCvt);

  BLPixelConverterFunc func = nullptr;
  switch (di.depth) {
    case 8:
      switch (si.depth) {
        case 1: func = bl_convert_any_from_indexed1<BLPixelAccess8>; break;
        case 2: func = bl_convert_any_from_indexed2<BLPixelAccess8>; break;
        case 4: func = bl_convert_any_from_indexed4<BLPixelAccess8>; break;
        case 8: func = bl_convert_any_from_indexed8<BLPixelAccess8>; break;
      }
      break;

    case 16:
      switch (si.depth) {
        case 1: func = bl_convert_any_from_indexed1<BLPixelAccess16<BL_BYTE_ORDER_NATIVE>>; break;
        case 2: func = bl_convert_any_from_indexed2<BLPixelAccess16<BL_BYTE_ORDER_NATIVE>>; break;
        case 4: func = bl_convert_any_from_indexed4<BLPixelAccess16<BL_BYTE_ORDER_NATIVE>>; break;
        case 8: func = bl_convert_any_from_indexed8<BLPixelAccess16<BL_BYTE_ORDER_NATIVE>>; break;
      }
      break;

    case 24:
      switch (si.depth) {
        case 1: func = bl_convert_any_from_indexed1<BLPixelAccess24<BL_BYTE_ORDER_NATIVE>>; break;
        case 2: func = bl_convert_any_from_indexed2<BLPixelAccess24<BL_BYTE_ORDER_NATIVE>>; break;
        case 4: func = bl_convert_any_from_indexed4<BLPixelAccess24<BL_BYTE_ORDER_NATIVE>>; break;
        case 8: func = bl_convert_any_from_indexed8<BLPixelAccess24<BL_BYTE_ORDER_NATIVE>>; break;
      }
      break;

    case 32:
      switch (si.depth) {
        case 1: func = bl_convert_any_from_indexed1<BLPixelAccess32<BL_BYTE_ORDER_NATIVE>>; break;
        case 2: func = bl_convert_any_from_indexed2<BLPixelAccess32<BL_BYTE_ORDER_NATIVE>>; break;
        case 4: func = bl_convert_any_from_indexed4<BLPixelAccess32<BL_BYTE_ORDER_NATIVE>>; break;
        case 8: func = bl_convert_any_from_indexed8<BLPixelAccess32<BL_BYTE_ORDER_NATIVE>>; break;
      }
      break;
  }

  d.convertFunc = func;
  d.internalFlags = uint8_t(internalFlags);

  if (internalFlags & BL_PIXEL_CONVERTER_INTERNAL_FLAG_DYNAMIC_DATA) {
    size_t* refCount = BLPtrOps::offset<size_t>(palette, paletteSizeInBytes);
    *refCount = 1;

    d.dynamic.table = palette;
    d.dynamic.refCount = refCount;
  }

  if (!func) {
    blPixelConverterReset(self);
    return blTraceError(BL_ERROR_NOT_IMPLEMENTED);
  }

  return BL_SUCCESS;
}

// PixelConverter - Init - Simple
// ==============================

static BLResult blPixelConverterInitCopyOr8888(BLPixelConverterCore* self, const BLFormatInfo& di, const BLFormatInfo& si) noexcept {
  BLPixelConverterData::MemCopyData& d = blPixelConverterGetData(self)->memCopyData;

  d.internalFlags = BL_PIXEL_CONVERTER_INTERNAL_FLAG_INITIALIZED | BL_PIXEL_CONVERTER_INTERNAL_FLAG_RAW_COPY ;
  d.bytesPerPixel = uint8_t(di.depth / 8u);

  // Required to handle Copy32, XRGB32<-PRGB32, and PRGB32<-XRGB32 conversions.
  uint32_t commonFlags = di.flags & si.flags;
  if (!(commonFlags & BL_FORMAT_FLAG_ALPHA)) {
    if (di.flags & BL_FORMAT_FLAG_ALPHA)
      d.fillMask = 0xFFu << di.shifts[3];
    else
      d.fillMask = blPixelConverterCalcFillMask32(di);
  }

  #ifdef BL_BUILD_OPT_AVX2
  if (blRuntimeHasAVX2(&blRuntimeContext)) {
    d.convertFunc = bl_convert_copy_or_8888_avx2;
    return BL_SUCCESS;
  }
  #endif

  #ifdef BL_BUILD_OPT_SSE2
  if (blRuntimeHasSSE2(&blRuntimeContext)) {
    d.convertFunc = bl_convert_copy_or_8888_sse2;
    return BL_SUCCESS;
  }
  #endif

  d.convertFunc = bl_convert_copy_or_8888;
  return BL_SUCCESS;
}

static BLResult blPixelConverterInitPremultiply8888(BLPixelConverterCore* self, const BLFormatInfo& di, const BLFormatInfo& si) noexcept {
  BLPixelConverterData::PremultiplyData& d = blPixelConverterGetData(self)->premultiplyData;

  bool dstHasAlpha = (di.flags & BL_FORMAT_FLAG_ALPHA) != 0;
  uint32_t aShift = dstHasAlpha ? di.shifts[3] : si.shifts[3];
  uint32_t fillMask = uint32_t(dstHasAlpha ? 0 : 0xFF) << aShift;

  d.alphaShift = uint8_t(aShift);
  d.fillMask = fillMask;

  #ifdef BL_BUILD_OPT_AVX2
  if (blRuntimeHasAVX2(&blRuntimeContext)) {
    if (aShift == 0) return blPixelConverterInitFuncOpt(self, bl_convert_premultiply_8888_trailing_alpha_avx2);
    if (aShift == 24) return blPixelConverterInitFuncOpt(self, bl_convert_premultiply_8888_leading_alpha_avx2);
  }
  #endif

  #ifdef BL_BUILD_OPT_SSE2
  if (blRuntimeHasSSE2(&blRuntimeContext)) {
    if (aShift == 0) return blPixelConverterInitFuncOpt(self, bl_convert_premultiply_8888_trailing_alpha_sse2);
    if (aShift == 24) return blPixelConverterInitFuncOpt(self, bl_convert_premultiply_8888_leading_alpha_sse2);
  }
  #endif

  return blPixelConverterInitFuncC(self, bl_convert_premultiply_8888);
}

static BLResult blPixelConverterInitUnpremultiply8888(BLPixelConverterCore* self, const BLFormatInfo& di, const BLFormatInfo& si) noexcept {
  blUnused(si);

  BLPixelConverterData::PremultiplyData& d = blPixelConverterGetData(self)->premultiplyData;

  uint32_t aShift = di.shifts[3];
  d.alphaShift = uint8_t(aShift);

  #ifdef BL_BUILD_OPT_AVX2
  if (blRuntimeHasAVX2(&blRuntimeContext)) {
    if (blRuntimeContext.optimizationInfo.hasFastPmulld()) {
      if (aShift == 0) return blPixelConverterInitFuncOpt(self, bl_convert_unpremultiply_8888_trailing_alpha_pmulld_avx2);
      if (aShift == 24) return blPixelConverterInitFuncOpt(self, bl_convert_unpremultiply_8888_leading_alpha_pmulld_avx2);
    }
    else {
      if (aShift == 0) return blPixelConverterInitFuncOpt(self, bl_convert_unpremultiply_8888_trailing_alpha_float_avx2);
      if (aShift == 24) return blPixelConverterInitFuncOpt(self, bl_convert_unpremultiply_8888_leading_alpha_float_avx2);
    }
  }
  #endif

  #ifdef BL_BUILD_OPT_SSE2
  if (blRuntimeHasSSE2(&blRuntimeContext)) {
    if (aShift == 0) return blPixelConverterInitFuncOpt(self, bl_convert_unpremultiply_8888_trailing_alpha_sse2);
    if (aShift == 24) return blPixelConverterInitFuncOpt(self, bl_convert_unpremultiply_8888_leading_alpha_sse2);
  }
  #endif

  if (aShift == 0) return blPixelConverterInitFuncC(self, bl_convert_unpremultiply_8888<0>);
  if (aShift == 24) return blPixelConverterInitFuncC(self, bl_convert_unpremultiply_8888<24>);

  return BL_RESULT_NOTHING;
}

static BLResult blPixelConverterInitSimple(BLPixelConverterCore* self, const BLFormatInfo& di, const BLFormatInfo& si, BLPixelConverterCreateFlags createFlags) noexcept {
  blUnused(createFlags);

  uint32_t depth = di.depth;
  uint32_t commonFlags = di.flags & si.flags;

  const uint32_t kA = BL_FORMAT_FLAG_ALPHA;
  const uint32_t kP = BL_FORMAT_FLAG_PREMULTIPLIED;

  if (blFormatInfoHasSameRgbLayout(di, si)) {
    if (blFormatInfoHasSameAlphaLayout(di, si)) {
      // Memory copy.
      if (di.flags == si.flags) {
        // Don't copy undefined bytes in 8888 formats, it's better to set them to 0xFF.
        if (depth == 32 && !(di.flags & BL_FORMAT_FLAG_ALPHA) && (di.flags & BL_FORMAT_FLAG_UNDEFINED_BITS))
          return blPixelConverterInitCopyOr8888(self, di, si);

        BLPixelConverterData::MemCopyData& d = blPixelConverterGetData(self)->memCopyData;
        d.internalFlags = BL_PIXEL_CONVERTER_INTERNAL_FLAG_INITIALIZED |
                          BL_PIXEL_CONVERTER_INTERNAL_FLAG_RAW_COPY ;
        d.bytesPerPixel = uint8_t(di.depth / 8u);

        #ifdef BL_BUILD_OPT_AVX2
        if (blRuntimeHasAVX2(&blRuntimeContext)) {
          d.convertFunc = bl_convert_copy_avx2;
          return BL_SUCCESS;
        }
        #endif

        #ifdef BL_BUILD_OPT_SSE2
        if (blRuntimeHasSSE2(&blRuntimeContext)) {
          d.convertFunc = bl_convert_copy_sse2;
          return BL_SUCCESS;
        }
        #endif

        d.convertFunc = bl_convert_copy;
        return BL_SUCCESS;
      }

      // Premultiply / Unpremultiply.
      if (BLIntOps::bitMatch(commonFlags, BL_FORMAT_FLAG_RGBA | BL_FORMAT_FLAG_BYTE_ALIGNED) && di.flags == (si.flags ^ kP)) {
        // Premultiply / Unpremultiply: 32-bit format where the alpha is either first or last.
        if (depth == 32) {
          // If we can do any alpha index it's okay, but generally prefer only
          // AlphaFirst|AlphaLast - other layouts are very unlikely to be used.
          if (di.flags & kP)
            BL_PROPAGATE_IF_NOT_NOTHING(blPixelConverterInitPremultiply8888(self, di, si));
          else
            BL_PROPAGATE_IF_NOT_NOTHING(blPixelConverterInitUnpremultiply8888(self, di, si));
        }
      }
    }
    else if (depth == 32 && BLIntOps::bitMatch(commonFlags, BL_FORMAT_FLAG_RGB | BL_FORMAT_FLAG_BYTE_ALIGNED)) {
      // Copy:
      //   PRGB32 <- XRGB32 - Copy with or-mask.
      //   ARGB32 <- XRGB32 - Copy with or-mask.
      //   XRGB32 <- PRGB32 - Copy with or-mask.
      if ((!(di.flags & kA) && (si.flags & kP)) || (!(si.flags & kA) && (di.flags & kA)))
        return blPixelConverterInitCopyOr8888(self, di, si);

      // Premultiply:
      //   XRGB32 <- ARGB32 - Premultiply with or-mask.
      if (!(di.flags & kA) && (si.flags & kA))
        return blPixelConverterInitPremultiply8888(self, di, si);
    }
  }
  else {
    #ifdef BL_BUILD_OPT_SSSE3
    if (depth == 32 && BLIntOps::bitMatch(commonFlags, BL_FORMAT_FLAG_RGB | BL_FORMAT_FLAG_BYTE_ALIGNED)) {
      // Handle the following conversions (Shufb|Or):
      //   XRGB32 <- XRGB32 - Shuffle with or-mask
      //   ARGB32 <- XRGB32 - Shuffle with or-mask (opaque alpha)
      //   PRGB32 <- XRGB32 - Shuffle with or-mask (opaque alpha)
      //   ARGB32 <- ARGB32 - Shuffle
      //   XRGB32 <- PRGB32 - Shuffle with or-mask (no unpremultiply)
      //   PRGB32 <- PRGB32 - Shuffle
      bool sameAlpha = (di.flags & (kA | kP)) == (si.flags & (kA | kP));
      bool dstAlpha = (di.flags & kA) != 0;
      bool srcAlpha = (si.flags & kA) != 0;

      if (sameAlpha || !srcAlpha || (!dstAlpha && BLIntOps::bitMatch(si.flags, kP))) {
        BLPixelConverterData::ShufbData& d = blPixelConverterGetData(self)->shufbData;
        blPixelConverterCalcPshufbPredicate32From32(d.shufbPredicate, di, si);

        if (!(di.flags & kA))
          d.fillMask = blPixelConverterCalcFillMask32(di);
        else if (!(si.flags & kA))
          d.fillMask = 0xFFu << di.shifts[3];

        #ifdef BL_BUILD_OPT_AVX2
        if (blRuntimeHasAVX2(&blRuntimeContext))
          return blPixelConverterInitFuncOpt(self, bl_convert_copy_shufb_8888_avx2);
        #endif

        return blPixelConverterInitFuncOpt(self, bl_convert_copy_shufb_8888_ssse3);
      }

      // Handle the following conversions (Premultiply|Shufb)
      //   PRGB32 <- ARGB32 - Shuffle with premultiply
      //   XRGB32 <- ARGB32 - Shuffle with premultiply
      if (((di.flags & kP) || !(di.flags & kA)) && (si.flags & (kA | kP)) == kA) {
        uint32_t aShift = di.shifts[3];

        BLPixelConverterData::ShufbData& d = blPixelConverterGetData(self)->shufbData;
        blPixelConverterCalcPshufbPredicate32From32(d.shufbPredicate, di, si);

        #ifdef BL_BUILD_OPT_AVX2
        if (blRuntimeHasAVX2(&blRuntimeContext)) {
          if (aShift == 0) return blPixelConverterInitFuncOpt(self, bl_convert_premultiply_8888_trailing_alpha_shufb_avx2);
          if (aShift == 24) return blPixelConverterInitFuncOpt(self, bl_convert_premultiply_8888_leading_alpha_shufb_avx2);
        }
        #endif

        if (aShift == 0) return blPixelConverterInitFuncOpt(self, bl_convert_premultiply_8888_trailing_alpha_shufb_ssse3);
        if (aShift == 24) return blPixelConverterInitFuncOpt(self, bl_convert_premultiply_8888_leading_alpha_shufb_ssse3);
      }
    }
    #endif
  }

  return BL_RESULT_NOTHING;
}

// PixelConverter - Init - 8 From 8888
// ===================================

static BLResult blPixelConverterInit8From8888(BLPixelConverterCore* self, const BLFormatInfo& di, const BLFormatInfo& si, BLPixelConverterCreateFlags createFlags) noexcept {
  blUnused(createFlags);
  BLPixelConverterData::X8FromRgb32Data& d = blPixelConverterGetData(self)->x8FromRgb32Data;

  uint32_t commonFlags = di.flags & si.flags;
  if (BLIntOps::bitMatch(commonFlags, BL_FORMAT_FLAG_ALPHA | BL_FORMAT_FLAG_BYTE_ALIGNED)) {
    d.bytesPerPixel = uint8_t(si.depth / 8u);
    d.alphaShift = uint8_t(si.shifts[3]);
    return blPixelConverterInitFuncC(self, bl_convert_a8_from_8888);
  }

  return BL_RESULT_NOTHING;
}

// PixelConverter - Init - 8888 From 8
// ===================================

static BLResult blPixelConverterInit8888From8(BLPixelConverterCore* self, const BLFormatInfo& di, const BLFormatInfo& si, BLPixelConverterCreateFlags createFlags) noexcept {
  blUnused(createFlags);
  uint32_t rgbMask = blPixelConverterCalcRgbMask32(di);

  BLPixelConverterData::Rgb32FromX8Data& d = blPixelConverterGetData(self)->rgb32FromX8Data;
  d.zeroMask = 0xFFFFFFFFu;

  if (!(si.flags & BL_FORMAT_FLAG_ALPHA)) {
    // ?RGB32 <- L8.
    d.fillMask = ~rgbMask;
  }
  else if (BLIntOps::bitMatch(di.flags, BL_FORMAT_FLAG_ALPHA | BL_FORMAT_FLAG_PREMULTIPLIED)) {
    // PRGB32 <- A8 - RGB channels are set to A, alpha channel is kept.
  }
  else if (BLIntOps::bitMatch(di.flags, BL_FORMAT_FLAG_ALPHA)) {
    // ARGB32 <- A8 - RGB channels are set to 255, alpha channel is kept.
    d.fillMask = rgbMask;
  }
  else {
    // XRGB32 <- A8 - RGB channels are set to A, alpha channel is set to 255.
    d.fillMask = ~rgbMask;
  }

  #ifdef BL_BUILD_OPT_SSE2
  if (blRuntimeHasSSE2(&blRuntimeContext))
    return blPixelConverterInitFuncOpt(self, bl_convert_8888_from_x8_sse2);
  #endif

  return blPixelConverterInitFuncC(self, bl_convert_8888_from_x8);
}

// PixelConverter - Init - 8888 From 888
// =====================================

static BLResult blPixelConverterInit8888From888(BLPixelConverterCore* self, const BLFormatInfo& di, const BLFormatInfo& si, BLPixelConverterCreateFlags createFlags) noexcept {
  blUnused(self, createFlags);

  uint32_t commonFlags = di.flags & si.flags;

  // This is only possible with SSSE3 and AVX2 enabled converters.
  #ifdef BL_BUILD_OPT_SSSE3
  if (blRuntimeHasSSSE3(&blRuntimeContext)) {
    // We expect both formats to provide RGB components and to be BYTE aligned.
    if (!(commonFlags & BL_FORMAT_FLAG_RGB))
      return BL_RESULT_NOTHING;

    BLPixelConverterData::ShufbData& d = blPixelConverterGetData(self)->shufbData;
    d.fillMask = ~blPixelConverterCalcRgbMask32(di);
    blPixelConverterCalcPshufbPredicate32From24(d.shufbPredicate, di, si);

    #ifdef BL_BUILD_OPT_AVX2
    if (blRuntimeHasAVX2(&blRuntimeContext))
      return blPixelConverterInitFuncOpt(self, bl_convert_rgb32_from_rgb24_shufb_avx2);
    #endif

    return blPixelConverterInitFuncOpt(self, bl_convert_rgb32_from_rgb24_shufb_ssse3);
  }
  #endif

  return BL_RESULT_NOTHING;
}

// PixelConverter - Init - NativeFromForeign
// =========================================

static BLResult blPixelConverterInit8888FromForeign(BLPixelConverterCore* self, const BLFormatInfo& di, const BLFormatInfo& si, BLPixelConverterCreateFlags createFlags) noexcept {
  blUnused(createFlags);

  BL_ASSERT(di.depth == 32);
  BL_ASSERT(di.flags & BL_FORMAT_FLAG_BYTE_ALIGNED);

  if (di.rShift != 16 || di.gShift != 8 || di.bShift != 0)
    return BL_RESULT_NOTHING;

  BLPixelConverterData::NativeFromForeign& d = blPixelConverterGetData(self)->nativeFromForeign;

  bool isSrcRGBA           = (si.flags & BL_FORMAT_FLAG_ALPHA) != 0;
  bool isSrcPremultiplied  = (si.flags & BL_FORMAT_FLAG_PREMULTIPLIED) != 0;
  bool hasSrcHostBO        = (si.flags & BL_FORMAT_FLAG_BYTE_SWAP) == 0;

  if (di.depth == 32 && !isSrcRGBA)
    d.fillMask = 0xFF000000u;

  for (uint32_t i = 0; i < 4; i++) {
    uint32_t size = si.sizes[i];
    uint32_t shift = si.shifts[i];

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

    d.masks[i] = BLIntOps::nonZeroLsbMask<uint32_t>(size);
    d.shifts[i] = uint8_t(shift);

    // Calculate a scale constant that will be used to expand bits in case that the source contains less than 8 bits.
    // We do it by adding `size`  to the `scaledSize` until we reach the required bit-depth.
    uint32_t scale = 0x1;
    uint32_t scaledSize = size;

    while (scaledSize < 8) {
      scale = (scale << size) | 1;
      scaledSize += size;
    }

    // Shift scale in a way that it contains MSB of the mask and the right position.
    uint32_t scaledShift = blPixelConverterNative32FromForeignShiftTable[i] - (scaledSize - 8);
    scale <<= scaledShift;
    d.scale[i] = scale;
  }

  // Special case of converting LUM to RGB.
  if (si.flags & BL_FORMAT_FLAG_LUM) {
    // TODO:
  }

  // Generic conversion.
  BLPixelConverterFunc func = nullptr;
  switch (si.depth) {
    case 16:
      // TODO:
      if (isSrcPremultiplied)
        func = hasSrcHostBO ? bl_convert_prgb32_from_prgb_any<BLPixelAccess16<BL_BYTE_ORDER_NATIVE >, BLMemOps::kUnalignedMem16>
                            : bl_convert_prgb32_from_prgb_any<BLPixelAccess16<BL_BYTE_ORDER_SWAPPED>, BLMemOps::kUnalignedMem16>;
      else if (isSrcRGBA)
        func = hasSrcHostBO ? bl_convert_prgb32_from_argb_any<BLPixelAccess16<BL_BYTE_ORDER_NATIVE >, BLMemOps::kUnalignedMem16>
                            : bl_convert_prgb32_from_argb_any<BLPixelAccess16<BL_BYTE_ORDER_SWAPPED>, BLMemOps::kUnalignedMem16>;
      else
        func = hasSrcHostBO ? bl_convert_xrgb32_from_xrgb_any<BLPixelAccess16<BL_BYTE_ORDER_NATIVE >, BLMemOps::kUnalignedMem16>
                            : bl_convert_xrgb32_from_xrgb_any<BLPixelAccess16<BL_BYTE_ORDER_SWAPPED>, BLMemOps::kUnalignedMem16>;
      break;

    case 24:
      if (isSrcPremultiplied)
        func = hasSrcHostBO ? bl_convert_prgb32_from_prgb_any<BLPixelAccess24<BL_BYTE_ORDER_NATIVE >, true>
                            : bl_convert_prgb32_from_prgb_any<BLPixelAccess24<BL_BYTE_ORDER_SWAPPED>, true>;
      else if (isSrcRGBA)
        func = hasSrcHostBO ? bl_convert_prgb32_from_argb_any<BLPixelAccess24<BL_BYTE_ORDER_NATIVE >, true>
                            : bl_convert_prgb32_from_argb_any<BLPixelAccess24<BL_BYTE_ORDER_SWAPPED>, true>;
      else
        func = hasSrcHostBO ? bl_convert_xrgb32_from_xrgb_any<BLPixelAccess24<BL_BYTE_ORDER_NATIVE >, true>
                            : bl_convert_xrgb32_from_xrgb_any<BLPixelAccess24<BL_BYTE_ORDER_SWAPPED>, true>;
      break;

    case 32:
      if (isSrcPremultiplied)
        func = hasSrcHostBO ? bl_convert_prgb32_from_prgb_any<BLPixelAccess32<BL_BYTE_ORDER_NATIVE >, BLMemOps::kUnalignedMem32>
                            : bl_convert_prgb32_from_prgb_any<BLPixelAccess32<BL_BYTE_ORDER_SWAPPED>, BLMemOps::kUnalignedMem32>;
      else if (isSrcRGBA)
        func = hasSrcHostBO ? bl_convert_prgb32_from_argb_any<BLPixelAccess32<BL_BYTE_ORDER_NATIVE >, BLMemOps::kUnalignedMem32>
                            : bl_convert_prgb32_from_argb_any<BLPixelAccess32<BL_BYTE_ORDER_SWAPPED>, BLMemOps::kUnalignedMem32>;
      else
        func = hasSrcHostBO ? bl_convert_xrgb32_from_xrgb_any<BLPixelAccess32<BL_BYTE_ORDER_NATIVE >, BLMemOps::kUnalignedMem32>
                            : bl_convert_xrgb32_from_xrgb_any<BLPixelAccess32<BL_BYTE_ORDER_SWAPPED>, BLMemOps::kUnalignedMem32>;
      break;

    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  return blPixelConverterInitFuncC(self, func);
}

// PixelConverter - Init - ForeignFromNative
// =========================================

static BLResult blPixelConverterInitForeignFrom8888(BLPixelConverterCore* self, const BLFormatInfo& di, const BLFormatInfo& si, BLPixelConverterCreateFlags createFlags) noexcept {
  blUnused(si, createFlags);

  BL_ASSERT(si.depth == 32);
  BL_ASSERT(si.flags & BL_FORMAT_FLAG_BYTE_ALIGNED);

  if (si.rShift != 16 || si.gShift != 8 || si.bShift != 0)
    return BL_RESULT_NOTHING;

  if (di.flags & BL_FORMAT_FLAG_INDEXED) {
    // TODO:
    return blTraceError(BL_ERROR_NOT_IMPLEMENTED);
  }
  else {
    BLPixelConverterData::ForeignFromNative& d = blPixelConverterGetData(self)->foreignFromNative;

    bool isDstRGBA          = (di.flags & BL_FORMAT_FLAG_ALPHA) != 0;
    bool isDstPremultiplied = (di.flags & BL_FORMAT_FLAG_PREMULTIPLIED) != 0;
    bool hasDstHostBO       = (di.flags & BL_FORMAT_FLAG_BYTE_SWAP) == 0;

    for (uint32_t i = 0; i < 4; i++) {
      uint32_t mask = 0;
      uint32_t size = di.sizes[i];
      uint32_t shift = di.shifts[i];

      if (size != 0) {
        mask = BLIntOps::nonZeroLsbMask<uint32_t>(size) << shift;
        shift = 32 - size - shift;
      }

      d.masks[i] = mask;
      d.shifts[i] = uint8_t(shift);
    }

    BLPixelConverterFunc func = nullptr;
    switch (di.depth) {
      case 16:
        if (isDstPremultiplied)
          func = hasDstHostBO ? bl_convert_prgb_any_from_prgb32<BLPixelAccess16<BL_BYTE_ORDER_NATIVE>, BLMemOps::kUnalignedMem16>
                              : bl_convert_prgb_any_from_prgb32<BLPixelAccess16<BL_BYTE_ORDER_SWAPPED>, BLMemOps::kUnalignedMem16>;
        else if (isDstRGBA)
          func = hasDstHostBO ? bl_convert_argb_any_from_prgb32<BLPixelAccess16<BL_BYTE_ORDER_NATIVE>, BLMemOps::kUnalignedMem16>
                              : bl_convert_argb_any_from_prgb32<BLPixelAccess16<BL_BYTE_ORDER_SWAPPED>, BLMemOps::kUnalignedMem16>;
        else
          func = hasDstHostBO ? bl_convert_xrgb_any_from_xrgb32<BLPixelAccess16<BL_BYTE_ORDER_NATIVE>, BLMemOps::kUnalignedMem16>
                              : bl_convert_xrgb_any_from_xrgb32<BLPixelAccess16<BL_BYTE_ORDER_SWAPPED>, BLMemOps::kUnalignedMem16>;
        break;

      case 24:
        if (isDstPremultiplied)
          func = hasDstHostBO ? bl_convert_prgb_any_from_prgb32<BLPixelAccess24<BL_BYTE_ORDER_NATIVE>, true>
                              : bl_convert_prgb_any_from_prgb32<BLPixelAccess24<BL_BYTE_ORDER_SWAPPED>, true>;
        else if (isDstRGBA)
          func = hasDstHostBO ? bl_convert_argb_any_from_prgb32<BLPixelAccess24<BL_BYTE_ORDER_NATIVE>, true>
                              : bl_convert_argb_any_from_prgb32<BLPixelAccess24<BL_BYTE_ORDER_SWAPPED>, true>;
        else
          func = hasDstHostBO ? bl_convert_xrgb_any_from_xrgb32<BLPixelAccess24<BL_BYTE_ORDER_NATIVE>, true>
                              : bl_convert_xrgb_any_from_xrgb32<BLPixelAccess24<BL_BYTE_ORDER_SWAPPED>, true>;
        break;

      case 32:
        if (isDstPremultiplied)
          func = hasDstHostBO ? bl_convert_prgb_any_from_prgb32<BLPixelAccess32<BL_BYTE_ORDER_NATIVE>, BLMemOps::kUnalignedMem32>
                              : bl_convert_prgb_any_from_prgb32<BLPixelAccess32<BL_BYTE_ORDER_SWAPPED>, BLMemOps::kUnalignedMem32>;
        else if (isDstRGBA)
          func = hasDstHostBO ? bl_convert_argb_any_from_prgb32<BLPixelAccess32<BL_BYTE_ORDER_NATIVE>, BLMemOps::kUnalignedMem32>
                              : bl_convert_argb_any_from_prgb32<BLPixelAccess32<BL_BYTE_ORDER_SWAPPED>, BLMemOps::kUnalignedMem32>;
        else
          func = hasDstHostBO ? bl_convert_xrgb_any_from_xrgb32<BLPixelAccess32<BL_BYTE_ORDER_NATIVE>, BLMemOps::kUnalignedMem32>
                              : bl_convert_xrgb_any_from_xrgb32<BLPixelAccess32<BL_BYTE_ORDER_SWAPPED>, BLMemOps::kUnalignedMem32>;
        break;

      default:
        return blTraceError(BL_ERROR_INVALID_VALUE);
    }

    return blPixelConverterInitFuncC(self, func);
  }
}

// PixelConverter - Init - Multi-Step
// ==================================

static BLResult BL_CDECL bl_convert_multi_step(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  const BLPixelConverterData::MultiStepData& d = blPixelConverterGetData(self)->multiStepData;
  uint32_t intermediatePixelCount = d.intermediatePixelCount;

  // NOTE: We use uintptr_t so the buffer gets properly aligned. In general we don't need a higher alignment than
  // 32-bit or 64-bit depending on the target.
  uintptr_t intermediateStorage[BL_PIXEL_CONVERTER_MULTISTEP_BUFFER_SIZE / sizeof(uintptr_t)];
  uint8_t* intermediateData = reinterpret_cast<uint8_t*>(intermediateStorage);

  const BLPixelConverterMultiStepContext* ctx = d.ctx;
  BLPixelConverterFunc srcToIntermediate = ctx->first.convertFunc;
  BLPixelConverterFunc intermediateToDst = ctx->second.convertFunc;

  if (!options)
    options = &blPixelConverterDefaultOptions;
  BLPixelConverterOptions workOpt = *options;

  if (w > intermediatePixelCount) {
    // Process part of the scanline at a time.
    uint8_t* dstLine = dstData;
    const uint8_t* srcLine = srcData;

    int baseOriginX = workOpt.origin.x;
    uint32_t dstBytesPerPixel = d.dstBytesPerPixel;
    uint32_t srcBytesPerPixel = d.srcBytesPerPixel;

    for (uint32_t y = h; y; y--) {
      uint32_t i = w;

      workOpt.origin.x = baseOriginX;
      dstData = dstLine;
      srcData = srcLine;

      while (i) {
        uint32_t n = blMin(i, intermediatePixelCount);

        srcToIntermediate(&ctx->first, intermediateData, 0, srcData, srcStride, n, 1, nullptr);
        intermediateToDst(&ctx->second, dstData, dstStride, intermediateData, 0, n, 1, &workOpt);

        dstData += n * dstBytesPerPixel;
        srcData += n * srcBytesPerPixel;
        workOpt.origin.x += int(n);

        i -= n;
      }

      dstLine += dstStride;
      srcLine += srcStride;
      workOpt.origin.y++;
    }

    return BL_SUCCESS;
  }
  else if (h > intermediatePixelCount || w * h > intermediatePixelCount) {
    // Process at least one scanline at a time.
    for (uint32_t y = h; y; y--) {
      srcToIntermediate(&ctx->first, intermediateData, 0, srcData, srcStride, w, 1, nullptr);
      intermediateToDst(&ctx->second, dstData, dstStride, intermediateData, 0, w, 1, &workOpt);

      dstData += dstStride;
      srcData += srcStride;
      workOpt.origin.y++;
    }

    return BL_SUCCESS;
  }
  else {
    // Process all scanlines as the `intermediateBuffer` is large enough.
    intptr_t intermediateStride = intptr_t(w) * d.intermediateBytesPerPixel;
    srcToIntermediate(&ctx->first, intermediateData, intermediateStride, srcData, srcStride, w, h, nullptr);
    return intermediateToDst(&ctx->second, dstData, dstStride, intermediateData, intermediateStride, w, h, &workOpt);
  }
}

static BLResult blPixelConverterInitMultiStepInternal(BLPixelConverterCore* self, const BLFormatInfo& di, const BLFormatInfo& intermediate, const BLFormatInfo& si) noexcept {
  BLPixelConverterMultiStepContext* ctx =
    static_cast<BLPixelConverterMultiStepContext*>(malloc(sizeof(BLPixelConverterMultiStepContext)));

  if (BL_UNLIKELY(!ctx))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLResult result;
  BLPixelConverterCreateFlags customFlags = BL_PIXEL_CONVERTER_CREATE_FLAG_NO_MULTI_STEP;

  memset(ctx, 0, sizeof(*ctx));
  if ((result = blPixelConverterInitInternal(&ctx->first, intermediate, si, customFlags)) != BL_SUCCESS ||
      (result = blPixelConverterInitInternal(&ctx->first, di, intermediate, customFlags)) != BL_SUCCESS) {
    blPixelConverterReset(&ctx->first);
    blPixelConverterReset(&ctx->second);
    free(ctx);
    return result;
  }

  BLPixelConverterData::MultiStepData& d = blPixelConverterGetData(self)->multiStepData;
  d.dstBytesPerPixel = uint8_t(di.depth / 8u);
  d.srcBytesPerPixel = uint8_t(si.depth / 8u);
  d.intermediateBytesPerPixel = uint8_t(intermediate.depth / 8u);
  d.intermediatePixelCount = BL_PIXEL_CONVERTER_MULTISTEP_BUFFER_SIZE / d.intermediateBytesPerPixel;

  ctx->refCount = 1;
  d.refCount = (size_t*)&ctx->refCount;
  d.ctx = ctx;

  uint32_t internalFlags = BL_PIXEL_CONVERTER_INTERNAL_FLAG_MULTI_STEP |
                           BL_PIXEL_CONVERTER_INTERNAL_FLAG_DYNAMIC_DATA;
  return blPixelConverterInitFuncC(self, bl_convert_multi_step, internalFlags);
}

static BLResult blPixelConverterInitMultiStep(BLPixelConverterCore* self, const BLFormatInfo& di, const BLFormatInfo& si, BLPixelConverterCreateFlags createFlags) noexcept {
  blUnused(createFlags);

  // We have foreign pixel formats on both input and output. This means that we will create two converters and
  // convert through a native pixel format as otherwise it would not be possible to convert the pixels by using
  // built-in converters.

  const uint32_t kA = BL_FORMAT_FLAG_ALPHA;
  const uint32_t kP = BL_FORMAT_FLAG_PREMULTIPLIED;

  uint32_t commonFlags = di.flags & si.flags;
  if (commonFlags & BL_FORMAT_FLAG_RGB) {
    // Temporary format information.
    BLFormatInfo intermediate = blFormatInfo[BL_FORMAT_PRGB32];
    if ((di.flags & (kA | kP)) == kA)
      intermediate.flags &= ~BL_FORMAT_FLAG_PREMULTIPLIED;
    if (!(di.flags & kA) || !(si.flags & kA))
      intermediate = blFormatInfo[BL_FORMAT_XRGB32];
    return blPixelConverterInitMultiStepInternal(self, di, intermediate, si);
  }

  return BL_RESULT_NOTHING;
}

// PixelConverter - Init - Internal
// ================================

BLResult blPixelConverterInitInternal(BLPixelConverterCore* self, const BLFormatInfo& di, const BLFormatInfo& si, BLPixelConverterCreateFlags createFlags) noexcept {
  uint32_t commonFlags = di.flags & si.flags;
  // Convert - Indexed destination is not supported.
  if (di.flags & BL_FORMAT_FLAG_INDEXED)
    return blTraceError(BL_ERROR_NOT_IMPLEMENTED);

  // Convert - Any from Indexed.
  if (si.flags & BL_FORMAT_FLAG_INDEXED)
    return blPixelConverterInitIndexed(self, di, si, createFlags);

  // Convert - MemCopy | Native | ShufB | Premultiply | Unpremultiply.
  if (di.depth == si.depth)
    BL_PROPAGATE_IF_NOT_NOTHING(blPixelConverterInitSimple(self, di, si, createFlags));

  if (di.depth == 8 && si.depth == 32) {
    // Convert - A8 <- ARGB32|PRGB32.
    if (BLIntOps::bitMatch(commonFlags, BL_FORMAT_FLAG_ALPHA | BL_FORMAT_FLAG_BYTE_ALIGNED))
      BL_PROPAGATE_IF_NOT_NOTHING(blPixelConverterInit8From8888(self, di, si, createFlags));
  }

  // Convert - ?RGB32 <- A8|L8.
  if (di.depth == 32 && si.depth == 8) {
    if (BLIntOps::bitMatch(commonFlags, BL_FORMAT_FLAG_BYTE_ALIGNED) && (di.flags & BL_FORMAT_FLAG_RGB) != 0)
      BL_PROPAGATE_IF_NOT_NOTHING(blPixelConverterInit8888From8(self, di, si, createFlags));
  }

  // Convert - ?RGB32 <- RGB24.
  if (di.depth == 32 && si.depth == 24) {
    if (BLIntOps::bitMatch(commonFlags, BL_FORMAT_FLAG_BYTE_ALIGNED | BL_FORMAT_FLAG_RGB))
      BL_PROPAGATE_IF_NOT_NOTHING(blPixelConverterInit8888From888(self, di, si, createFlags));
  }

  // Convert - ?RGB32 <- Foreign.
  if (di.depth == 32 && BLIntOps::bitMatch(di.flags, BL_FORMAT_FLAG_BYTE_ALIGNED))
    BL_PROPAGATE_IF_NOT_NOTHING(blPixelConverterInit8888FromForeign(self, di, si, createFlags));

  // Convert - Foreign <- ?RGB32.
  if (si.depth == 32 && BLIntOps::bitMatch(si.flags, BL_FORMAT_FLAG_BYTE_ALIGNED))
    BL_PROPAGATE_IF_NOT_NOTHING(blPixelConverterInitForeignFrom8888(self, di, si, createFlags));

  // Convert - Foreign <- Foreign.
  if (!(createFlags & BL_PIXEL_CONVERTER_CREATE_FLAG_NO_MULTI_STEP))
    BL_PROPAGATE_IF_NOT_NOTHING(blPixelConverterInitMultiStep(self, di, si, createFlags));

  // Probably extreme case that is not implemented.
  return blTraceError(BL_ERROR_NOT_IMPLEMENTED);
}

// PixelConverter - Tests
// ======================

#ifdef BL_TEST

// XRGB32 <-> A8 Conversion Tests
// ------------------------------

static void testRgb32A8Conversions() noexcept {
  INFO("Testing ?RGB32 <-> A8 conversions");

  // Pixel formats.
  BLFormatInfo a8Format = blFormatInfo[BL_FORMAT_A8];
  BLFormatInfo xrgb32Format = blFormatInfo[BL_FORMAT_XRGB32];
  BLFormatInfo argb32Format = blFormatInfo[BL_FORMAT_PRGB32];
  BLFormatInfo prgb32Format = blFormatInfo[BL_FORMAT_PRGB32];

  argb32Format.flags &= ~BL_FORMAT_FLAG_PREMULTIPLIED;

  // Pixel buffers.
  uint8_t srcX8[256];
  uint8_t dstX8[256];
  uint32_t rgb32[256];

  uint32_t i;
  uint32_t n;

  // Prepare.
  for (i = 0; i < 256; i++)
    srcX8[i] = uint8_t(i);

  BLPixelConverter cvtXrgb32FromA8;
  BLPixelConverter cvtArgb32FromA8;
  BLPixelConverter cvtPrgb32FromA8;
  BLPixelConverter cvtA8FromPrgb32;

  EXPECT_SUCCESS(cvtXrgb32FromA8.create(xrgb32Format, a8Format));
  EXPECT_SUCCESS(cvtArgb32FromA8.create(argb32Format, a8Format));
  EXPECT_SUCCESS(cvtPrgb32FromA8.create(prgb32Format, a8Format));
  EXPECT_SUCCESS(cvtA8FromPrgb32.create(a8Format, prgb32Format));

  // This would test the conversion and also whether the SIMD implementation
  // is okay. We test for 1..256 pixels and verify all pixels from 0..255.
  for (n = 1; n < 256; n++) {
    memset(rgb32, 0, sizeof(rgb32));
    EXPECT_SUCCESS(cvtXrgb32FromA8.convertSpan(rgb32, srcX8, n));

    for (i = 0; i < 256; i++) {
      if (i < n) {
        uint32_t p0 = rgb32[i];
        uint32_t p1 = (uint32_t(srcX8[i]) * 0x01010101u) | 0xFF000000u;
        EXPECT_EQ(p0, p1).message("[%u] XRGB32<-A8 conversion error OUT[%08X] != EXP[%08X]", i, p0, p1);
      }
      else {
        EXPECT_EQ(rgb32[i], 0u).message("[%u] Detected buffer overrun after XRGB32<-A8 conversion", i);
      }
    }

    memset(rgb32, 0, sizeof(rgb32));
    EXPECT_SUCCESS(cvtArgb32FromA8.convertSpan(rgb32, srcX8, n));

    for (i = 0; i < 256; i++) {
      if (i < n) {
        uint32_t p0 = rgb32[i];
        uint32_t p1 = (uint32_t(srcX8[i]) * 0x01010101u) | 0x00FFFFFFu;
        EXPECT_EQ(p0, p1).message("[%u] ARGB32<-A8 conversion error OUT[%08X] != EXP[%08X]", i, p0, p1);
      }
      else {
        EXPECT_EQ(rgb32[i], 0u).message("[%u] Detected buffer overrun after ARGB32<-A8 conversion", i);
      }
    }

    memset(rgb32, 0, sizeof(rgb32));
    EXPECT_SUCCESS(cvtPrgb32FromA8.convertSpan(rgb32, srcX8, n));

    for (i = 0; i < 256; i++) {
      if (i < n) {
        uint32_t p0 = rgb32[i];
        uint32_t p1 = uint32_t(srcX8[i]) * 0x01010101u;
        EXPECT_EQ(p0, p1).message("[%u] PRGB32<-A8 conversion error OUT[%08X] != EXP[%08X]", i, p0, p1);
      }
      else {
        EXPECT_EQ(rgb32[i], 0u).message("[%u] Detected buffer overrun after PRGB32<-A8 conversion", i);
      }
    }
  }

  for (n = 1; n < 256; n++) {
    memset(dstX8, 0, sizeof(dstX8));
    EXPECT_SUCCESS(cvtA8FromPrgb32.convertSpan(dstX8, rgb32, n));

    for (i = 0; i < 256; i++) {
      if (i < n) {
        uint32_t p0 = srcX8[i];
        uint32_t p1 = dstX8[i];
        EXPECT_EQ(p0, p1).message("[%u] A8<-PRGB32 conversion error OUT[%02X] != EXP[%02X]", i, p0, p1);
      }
      else {
        EXPECT_EQ(dstX8[i], 0u).message("[%u] Detected buffer overrun after A8<-PRGB32 conversion", i);
      }
    }
  }
}

// XRGB32 <-> RGB24 Conversion Tests
// ---------------------------------

static void testRgb32Rgb24Conversions() noexcept {
  INFO("Testing ?RGB32 <-> RGB24 conversions");

  // Pixel formats.
  BLFormatInfo rgb32Format = blFormatInfo[BL_FORMAT_XRGB32];
  BLFormatInfo rgb24Format { 24, BL_FORMAT_FLAG_RGB | BL_FORMAT_FLAG_BE, {{ { 8, 8, 8, 0 }, { 16, 8, 0, 0 } }} };
  BLFormatInfo bgr24Format { 24, BL_FORMAT_FLAG_RGB | BL_FORMAT_FLAG_LE, {{ { 8, 8, 8, 0 }, { 16, 8, 0, 0 } }} };

  const uint8_t allZeros[4] {};

  // Pixel buffers.
  uint8_t srcRgb24[256 * 3];
  uint8_t dstRgb24[256 * 3];
  uint32_t rgb32[256];

  // Prepare.
  uint32_t i;
  uint32_t n;

  for (i = 0; i < 256 * 3; i += 3) {
    srcRgb24[i    ] = uint8_t((i + 0) & 0xFF);
    srcRgb24[i + 1] = uint8_t((i + 1) & 0xFF);
    srcRgb24[i + 2] = uint8_t((i + 2) & 0xFF);
  }

  BLPixelConverter cvtRgb32FromRgb24;
  BLPixelConverter cvtRgb32FromBgr24;
  BLPixelConverter cvtBgr24FromRgb32;
  BLPixelConverter cvtRgb24FromRgb32;

  EXPECT_SUCCESS(cvtRgb32FromRgb24.create(rgb32Format, rgb24Format));
  EXPECT_SUCCESS(cvtRgb32FromBgr24.create(rgb32Format, bgr24Format));

  EXPECT_SUCCESS(cvtRgb24FromRgb32.create(rgb24Format, rgb32Format));
  EXPECT_SUCCESS(cvtBgr24FromRgb32.create(bgr24Format, rgb32Format));

  // This would test the conversion and also whether the SIMD implementation
  // is okay. We test for 1..256 pixels and verify all pixels from 0..255.
  for (n = 1; n < 256; n++) {
    memset(rgb32, 0, sizeof(rgb32));
    EXPECT_SUCCESS(cvtRgb32FromRgb24.convertSpan(rgb32, srcRgb24, n));

    for (i = 0; i < 256; i++) {
      if (i < n) {
        uint32_t p0 = rgb32[i];
        uint32_t p1 = BLRgbaPrivate::packRgba32(srcRgb24[i * 3 + 0], srcRgb24[i * 3 + 1], srcRgb24[i * 3 + 2]);
        EXPECT_EQ(p0, p1)
          .message("[%u] RGB32<-RGB24 conversion error OUT[%08X] != EXP[%08X]", i, p0, p1);
      }
      else {
        EXPECT_EQ(rgb32[i], 0u)
          .message("[%u] Detected buffer overrun after RGB32<-RGB24 conversion", i);
      }
    }
  }

  for (n = 1; n < 256; n++) {
    memset(dstRgb24, 0, sizeof(dstRgb24));
    EXPECT_SUCCESS(cvtRgb24FromRgb32.convertSpan(dstRgb24, rgb32, n));

    for (i = 0; i < 256; i++) {
      if (i < n) {
        EXPECT_EQ(memcmp(dstRgb24 + i * 3, srcRgb24 + i * 3, 3), 0)
          .message("[%u] RGB24<-RGB32 conversion error OUT[%02X|%02X|%02X] != EXP[%02X|%02X|%02X]", i,
                   dstRgb24[i * 3 + 0], dstRgb24[i * 3 + 1], dstRgb24[i * 3 + 2],
                   srcRgb24[i * 3 + 0], srcRgb24[i * 3 + 1], srcRgb24[i * 3 + 2]);
      }
      else {
        EXPECT_EQ(memcmp(dstRgb24 + i * 3, allZeros, 3), 0)
          .message("[%u] Detected buffer overrun after RGB24<-RGB32 conversion", i);
      }
    }
  }

  for (n = 1; n < 256; n++) {
    memset(rgb32, 0, sizeof(rgb32));
    EXPECT_SUCCESS(cvtRgb32FromBgr24.convertSpan(rgb32, srcRgb24, n));

    for (i = 0; i < 256; i++) {
      if (i < n) {
        uint32_t p0 = rgb32[i];
        uint32_t p1 = BLRgbaPrivate::packRgba32(srcRgb24[i * 3 + 2], srcRgb24[i * 3 + 1], srcRgb24[i * 3 + 0]);
        EXPECT_EQ(p0, p1)
          .message("[%u] RGB32<-BGR24 conversion error OUT[%08X] != EXP[%08X]", i, p0, p1);
      }
      else {
        EXPECT_EQ(rgb32[i], 0u)
          .message("[%u] Detected buffer overrun after RGB32<-BGR24 conversion", i);
      }
    }
  }

  for (n = 1; n < 256; n++) {
    memset(dstRgb24, 0, sizeof(dstRgb24));
    EXPECT_SUCCESS(cvtBgr24FromRgb32.convertSpan(dstRgb24, rgb32, n));

    for (i = 0; i < 256; i++) {
      if (i < n) {
        EXPECT_EQ(memcmp(dstRgb24 + i * 3, srcRgb24 + i * 3, 3), 0)
          .message("[%u] BGR24<-RGB32 conversion error OUT[%02X|%02X|%02X] != EXP[%02X|%02X|%02X]", i,
                    dstRgb24[i * 3 + 0], dstRgb24[i * 3 + 1], dstRgb24[i * 3 + 2],
                    srcRgb24[i * 3 + 0], srcRgb24[i * 3 + 1], srcRgb24[i * 3 + 2]);
      }
      else {
        EXPECT_EQ(memcmp(dstRgb24 + i * 3, allZeros, 3), 0)
          .message("[%u] Detected buffer overrun after BGR24<-RGB32 conversion", i);
      }
    }
  }
}

// Premultiply / Unpremultiply Conversion Tests
// --------------------------------------------

static void testPremultiplyConversions() noexcept {
  INFO("Testing premultiply & unpremultiply conversions");

  uint32_t i;
  uint32_t n;
  uint32_t f;

  constexpr uint32_t N = 1024;

  uint32_t src[N];
  uint32_t dst[N];
  uint32_t unp[N];

  uint64_t defaultSeed = 0x1234;

  BLFormatInfo unpremultipliedFmt[4];
  BLFormatInfo premultipliedFmt[4];

  // Shifts in host byte-order.
  static const uint8_t formatShifts[4][4] = {
    { 16,  8,  0, 24 }, // 0x[AA|RR|GG|BB]
    {  0,  8, 16, 24 }, // 0x[AA|BB|GG|RR]
    { 24, 16,  8,  0 }, // 0x[RR|GG|BB|AA]
    {  8, 16, 24,  0 }  // 0x[BB|GG|RR|AA]
  };

  static const char* formatNames[4] = {
    "ARGB32",
    "ABGR32",
    "RGBA32",
    "BGRA32"
  };

  // Initialize both formats.
  for (f = 0; f < 4; f++) {
    const uint8_t* s = formatShifts[f];

    unpremultipliedFmt[f].depth = 32;
    unpremultipliedFmt[f].flags = BL_FORMAT_FLAG_RGBA;
    unpremultipliedFmt[f].setSizes(8, 8, 8, 8);
    unpremultipliedFmt[f].setShifts(s[0], s[1], s[2], s[3]);

    premultipliedFmt[f] = unpremultipliedFmt[f];
    premultipliedFmt[f].flags |= BL_FORMAT_FLAG_PREMULTIPLIED;
  }

  BLRandom r(defaultSeed);
  for (i = 0; i < N; i++) {
    src[i] = r.nextUInt32();
  }

  for (f = 0; f < 4; f++) {
    INFO("  32-bit %s format", formatNames[f]);

    bool leadingAlpha = formatShifts[f][3] == 24;
    BLPixelConverter cvt1;
    BLPixelConverter cvt2;

    EXPECT_SUCCESS(cvt1.create(premultipliedFmt[f], unpremultipliedFmt[f]));
    EXPECT_SUCCESS(cvt2.create(unpremultipliedFmt[f], premultipliedFmt[f]));

    for (n = 1; n < N; n++) {
      memset(dst, 0, sizeof(dst));
      memset(unp, 0, sizeof(unp));

      EXPECT_SUCCESS(cvt1.convertSpan(dst, src, n));
      EXPECT_SUCCESS(cvt2.convertSpan(unp, dst, n));

      for (i = 0; i < n; i++) {
        if (i < n) {
          uint32_t sp = src[i]; // Source pixel.
          uint32_t dp = dst[i]; // Premultiply(sp).
          uint32_t up = unp[i]; // Unpremultiply(dp).

          uint32_t s0 = (sp >> 24) & 0xFFu;
          uint32_t s1 = (sp >> 16) & 0xFFu;
          uint32_t s2 = (sp >>  8) & 0xFFu;
          uint32_t s3 = (sp >>  0) & 0xFFu;

          uint32_t a = leadingAlpha ? s0 : s3;
          if (leadingAlpha)
            s0 = 0xFF;
          else
            s3 = 0xFF;

          uint32_t e0 = BLPixelOps::Scalar::udiv255(s0 * a);
          uint32_t e1 = BLPixelOps::Scalar::udiv255(s1 * a);
          uint32_t e2 = BLPixelOps::Scalar::udiv255(s2 * a);
          uint32_t e3 = BLPixelOps::Scalar::udiv255(s3 * a);
          uint32_t ep = (e0 << 24) | (e1 << 16) | (e2 << 8) | e3;

          EXPECT_EQ(dp, ep)
            .message("[%u] OUT[0x%08X] != EXP[0x%08X] <- Premultiply(SRC[0x%08X])", i, dp, ep, sp);

          if (leadingAlpha)
            BLPixelOps::Scalar::unpremultiply_rgb_8bit(e1, e2, e3, e0);
          else
            BLPixelOps::Scalar::unpremultiply_rgb_8bit(e0, e1, e2, e3);

          ep = (e0 << 24) | (e1 << 16) | (e2 << 8) | e3;
          EXPECT_EQ(up, ep)
            .message("[%u] OUT[0x%08X] != EXP[0x%08X] <- Unpremultiply(DST[0x%08X])", i, up, ep, dp);
        }
        else {
          uint32_t dp = dst[i];
          EXPECT_EQ(dp, 0u)
            .message("[%u] Detected buffer overrun", i);
        }
      }
    }
  }
}

// Generic Conversion Tests
// ------------------------

template<typename T>
struct BLPixelConverterGenericTest {
  static void fillMasks(BLFormatInfo& fi) noexcept {
    fi.shifts[0] = uint8_t(T::kR ? BLIntOps::ctz(T::kR) : uint32_t(0));
    fi.shifts[1] = uint8_t(T::kG ? BLIntOps::ctz(T::kG) : uint32_t(0));
    fi.shifts[2] = uint8_t(T::kB ? BLIntOps::ctz(T::kB) : uint32_t(0));
    fi.shifts[3] = uint8_t(T::kA ? BLIntOps::ctz(T::kA) : uint32_t(0));
    fi.sizes[0] = uint8_t(T::kR ? BLIntOps::ctz(~(T::kR >> fi.shifts[0])) : uint32_t(0));
    fi.sizes[1] = uint8_t(T::kG ? BLIntOps::ctz(~(T::kG >> fi.shifts[1])) : uint32_t(0));
    fi.sizes[2] = uint8_t(T::kB ? BLIntOps::ctz(~(T::kB >> fi.shifts[2])) : uint32_t(0));
    fi.sizes[3] = uint8_t(T::kA ? BLIntOps::ctz(~(T::kA >> fi.shifts[3])) : uint32_t(0));
  }

  static void testPrgb32() noexcept {
    INFO("  %d-bit %s format", T::kDepth, T::formatString());

    BLPixelConverter from;
    BLPixelConverter back;

    BLFormatInfo fi {};
    fillMasks(fi);
    fi.depth = T::kDepth;
    fi.flags = fi.sizes[3] ? BL_FORMAT_FLAG_RGBA | BL_FORMAT_FLAG_PREMULTIPLIED : BL_FORMAT_FLAG_RGB;

    EXPECT_SUCCESS(from.create(fi, blFormatInfo[BL_FORMAT_PRGB32]))
      .message("%s: Failed to create from [%dbpp 0x%08X 0x%08X 0x%08X 0x%08X]", T::formatString(), T::kDepth, T::kR, T::kG, T::kB, T::kA);

    EXPECT_SUCCESS(back.create(blFormatInfo[BL_FORMAT_PRGB32], fi))
      .message("%s: Failed to create to [%dbpp 0x%08X 0x%08X 0x%08X 0x%08X]", T::formatString(), T::kDepth, T::kR, T::kG, T::kB, T::kA);

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
      switch (uint32_t(T::kDepth)) {
        case 8 : mid = BLMemOps::readU8(buf + i); break;
        case 16: mid = BLMemOps::readU16u(buf + i * 2u); break;
        case 24: mid = BLMemOps::readU24u(buf + i * 3u); break;
        case 32: mid = BLMemOps::readU32u(buf + i * 4u); break;
      }

      EXPECT_EQ(dst[i], src[i])
        .message("%s: [%u] Dst(%08X) <- 0x%08X <- Src(0x%08X) [%dbpp %08X|%08X|%08X|%08X]",
                 T::formatString(), i, dst[i], mid, src[i], T::kDepth, T::kA, T::kR, T::kG, T::kB);
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

static void testGenericConversions() noexcept {
  INFO("Testing generic conversions");

  BLPixelConverterGenericTest<Test_XRGB_0555>::test();
  BLPixelConverterGenericTest<Test_XBGR_0555>::test();
  BLPixelConverterGenericTest<Test_XRGB_0565>::test();
  BLPixelConverterGenericTest<Test_XBGR_0565>::test();
  BLPixelConverterGenericTest<Test_ARGB_4444>::test();
  BLPixelConverterGenericTest<Test_ABGR_4444>::test();
  BLPixelConverterGenericTest<Test_RGBA_4444>::test();
  BLPixelConverterGenericTest<Test_BGRA_4444>::test();
  BLPixelConverterGenericTest<Test_XRGB_0888>::test();
  BLPixelConverterGenericTest<Test_XBGR_0888>::test();
  BLPixelConverterGenericTest<Test_XRGB_8888>::test();
  BLPixelConverterGenericTest<Test_XBGR_8888>::test();
  BLPixelConverterGenericTest<Test_RGBX_8888>::test();
  BLPixelConverterGenericTest<Test_BGRX_8888>::test();
  BLPixelConverterGenericTest<Test_ARGB_8888>::test();
  BLPixelConverterGenericTest<Test_ABGR_8888>::test();
  BLPixelConverterGenericTest<Test_RGBA_8888>::test();
  BLPixelConverterGenericTest<Test_BGRA_8888>::test();
  BLPixelConverterGenericTest<Test_BRGA_8888>::test();
}

UNIT(pixel_converter, -7) {
  testRgb32A8Conversions();
  testRgb32Rgb24Conversions();
  testPremultiplyConversions();
  testGenericConversions();
}
#endif
