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
#include "../pixelops/scalar_p.h"
#include "../support/memops_p.h"
#include "../support/lookuptable_p.h"

#if BL_TARGET_ARCH_BITS >= 64
  #define BL_QOI_USE_64_BIT_ARITHMETIC
#endif

namespace bl {
namespace Qoi {

// bl::Qoi::Codec - Globals
// ========================

static BLObjectEternalVirtualImpl<BLQoiCodecImpl, BLImageCodecVirt> qoiCodec;
static BLImageCodecCore qoiCodecInstance;
static BLImageDecoderVirt qoiDecoderVirt;
static BLImageEncoderVirt qoiEncoderVirt;

// bl::Qoi::Codec - Constants
// ==========================

static constexpr size_t kQoiHeaderSize = 14;
static constexpr size_t kQoiMagicSize = 4;
static constexpr size_t kQoiEndMarkerSize = 8;

static constexpr uint8_t kQoiOpIndex = 0x00; // 00xxxxxx
static constexpr uint8_t kQoiOpDiff  = 0x40; // 01xxxxxx
static constexpr uint8_t kQoiOpLuma  = 0x80; // 10xxxxxx
static constexpr uint8_t kQoiOpRun   = 0xC0; // 11xxxxxx
static constexpr uint8_t kQoiOpRgb   = 0xFE; // 11111110
static constexpr uint8_t kQoiOpRgba  = 0xFF; // 11111111

static constexpr uint32_t kQoiHashR = 3u;
static constexpr uint32_t kQoiHashG = 5u;
static constexpr uint32_t kQoiHashB = 7u;
static constexpr uint32_t kQoiHashA = 11u;
static constexpr uint32_t kQoiHashMask = 0x3Fu;

static constexpr uint8_t qoiMagic[kQoiMagicSize] = { 'q', 'o', 'i', 'f' };
static constexpr uint8_t qoiEndMarker[kQoiEndMarkerSize] = { 0, 0, 0, 0, 0, 0, 0, 1 };

// Lookup table generator that generates delta values for QOI_OP_DIFF and the first byte of QOI_OP_LUMA.
// Additionally, it provides values for a RLE run of a single pixel for a possible experimentation.
struct IndexDiffLumaTableGen {
  static constexpr uint32_t rgb(uint32_t r, uint32_t g, uint32_t b, uint32_t lumaMask) noexcept {
    return ((r & 0xFFu) << 24) |
           ((g & 0xFFu) << 16) |
           ((b & 0xFFu) <<  8) |
           ((lumaMask ) <<  0) ;
  }

  static constexpr uint32_t diff(uint32_t b0) noexcept {
    return rgb(((b0 >> 4) & 0x3u) - 2u, ((b0 >> 2) & 0x3u) - 2u, ((b0 >> 0) & 0x3u) - 2u, 0x00u);
  }

  static constexpr uint32_t luma(uint32_t b0) noexcept {
    return rgb(b0 - 40u, b0 - 32u, b0 - 40u, 0xFF);
  }

  static constexpr uint32_t value(size_t idx) noexcept {
    return idx < 64u  ? diff(uint32_t(idx      )) :
           idx < 128u ? luma(uint32_t(idx - 64u)) : 0u;
  }
};

static constexpr LookupTable<uint32_t, 129> qoiIndexDiffLumaLUT = makeLookupTable<uint32_t, 129, IndexDiffLumaTableGen>();

// bl::Qoi::Codec - Hashing
// ========================

static BL_INLINE uint32_t hashPixelAGxRBx64(uint64_t ag_rb) noexcept {
  ag_rb *= (uint64_t(kQoiHashA) << ( 8 + 2)) + (uint64_t(kQoiHashG) << (24 + 2)) +
           (uint64_t(kQoiHashR) << (40 + 2)) + (uint64_t(kQoiHashB) << (56 + 2)) ;
  return uint32_t(ag_rb >> 58);
}

static BL_INLINE uint32_t hashPixelAGxRBx32(uint32_t ag, uint32_t rb) noexcept {
  ag *= ((kQoiHashA << (0 + 2)) + (kQoiHashG << (16 + 2)));
  rb *= ((kQoiHashR << (8 + 2)) + (kQoiHashB << (24 + 2)));

  return (ag + rb) >> 26;
}

static BL_INLINE uint32_t hashPixelRGBA32(uint32_t pixel) noexcept {
#if defined(BL_QOI_USE_64_BIT_ARITHMETIC)
  return hashPixelAGxRBx64(((uint64_t(pixel) << 24) | pixel) & 0x00FF00FF00FF00FFu);
#else
  return hashPixelAGxRBx32(pixel & 0xFF00FF00u, pixel & 0x00FF00FFu);
#endif
}

static BL_INLINE uint32_t hashPixelA8(uint32_t a) noexcept {
  return (0xFFu * (kQoiHashR + kQoiHashG + kQoiHashB) + a * kQoiHashA) & kQoiHashMask;
}

// bl::Qoi::Codec - UnpackedPixel
// ==============================

#if defined(BL_QOI_USE_64_BIT_ARITHMETIC)
struct UnpackedPixel {
  uint64_t ag_rb; // Represents 0x00AA00GG00RR00BB.

  static BL_INLINE UnpackedPixel unpack(uint32_t packed) noexcept {
    return UnpackedPixel{((uint64_t(packed) << 24) | packed) & 0x00FF00FF00FF00FFu};
  }

  static BL_INLINE UnpackedPixel unpackRGBA(uint32_t r, uint32_t g, uint32_t b, uint32_t a) noexcept {
    return UnpackedPixel{(uint64_t(a) << 48) | (uint64_t(g) << 32) | (uint64_t(r) << 16) | (uint64_t(b) << 0)};
  }

  template<bool kHasAlpha>
  BL_INLINE uint32_t pack() const noexcept {
    uint32_t rgba32 = uint32_t(ag_rb >> 24) | uint32_t(ag_rb & 0xFFFFFFFFu);
    if BL_CONSTEXPR (kHasAlpha)
      return PixelOps::Scalar::cvt_prgb32_8888_from_argb32_8888(rgba32);
    else
      return rgba32 | 0xFF000000u;
  }

  BL_INLINE uint32_t hash() const noexcept { return hashPixelAGxRBx64(ag_rb); }

  BL_INLINE void add(const UnpackedPixel& other) noexcept { ag_rb += other.ag_rb; }
  BL_INLINE void addRB(uint32_t value) noexcept { ag_rb += value; }
  BL_INLINE void mask() noexcept { ag_rb &= 0x00FF00FF00FF00FFu; }

  BL_INLINE void opRGBX(uint32_t hbyte0, const UnpackedPixel& other) noexcept {
    uint64_t msk = uint64_t(hbyte0 + 1) << 48;
    ag_rb = (ag_rb & msk) | (other.ag_rb & ~msk);
  }
};
#else
struct UnpackedPixel {
  uint32_t ag; // Represents 0xAA00GG00.
  uint32_t rb; // Represents 0x00RR00BB.

  static BL_INLINE UnpackedPixel unpack(uint32_t packed) noexcept {
    return UnpackedPixel{packed & 0xFF00FF00u, packed & 0x00FF00FFu };
  }

  static BL_INLINE UnpackedPixel unpackRGBA(uint32_t r, uint32_t g, uint32_t b, uint32_t a) noexcept {
    return UnpackedPixel{
      (uint32_t(a) << 24) | (uint32_t(g) << 8), // AG
      (uint32_t(r) << 16) | (uint32_t(b) << 0)  // RB
    };
  }

  template<bool kHasAlpha>
  BL_INLINE uint32_t pack() const noexcept {
    uint32_t rgba32 = ag | rb;

    if (kHasAlpha)
      return PixelOps::Scalar::cvt_prgb32_8888_from_argb32_8888(rgba32);
    else
      return rgba32 | 0xFF000000u;
  }

  BL_INLINE uint32_t hash() const noexcept { return hashPixelAGxRBx32(ag, rb); }

  BL_INLINE void add(const UnpackedPixel& other) noexcept { ag += other.ag; rb += other.rb; }
  BL_INLINE void addRB(uint32_t value) noexcept { rb += value; }
  BL_INLINE void mask() noexcept { ag &= 0xFF00FF00u; rb &= 0x00FF00FFu; }

  BL_INLINE void opRGBX(uint32_t hbyte0, const UnpackedPixel& other) noexcept {
    uint32_t msk = uint32_t(hbyte0 + 1) << 24;
    ag = (ag & msk) | (other.ag & ~msk);
    rb = other.rb;
  }
};
#endif

// bl::Qoi::Codec - Utilities
// ==========================

static BL_INLINE uint32_t* fillRgba32(uint32_t* dst, uint32_t value, size_t count) noexcept {
  for (size_t i = 0; i < count; i++)
    dst[i] = value;
  return dst + count;
}

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

template<bool kHasAlpha>
static BL_INLINE BLResult decodeQoiData(
  uint8_t* dstRow,
  intptr_t dstStride,
  uint32_t w,
  uint32_t h,
  uint32_t packedTable[64],
  UnpackedPixel unpackedTable[64],
  const uint8_t* src,
  const uint8_t* end) noexcept {

  constexpr size_t kMinRemainingBytesOfNextChunk = kQoiEndMarkerSize + 1u;

  uint32_t* dstPtr = reinterpret_cast<uint32_t*>(dstRow);
  uint32_t* dstEnd = dstPtr + w;

  uint32_t packedPixel = 0xFF000000;
  UnpackedPixel unpackedPixel = UnpackedPixel::unpack(packedPixel);

  // Edge case: If the image starts with QOI_OP_RUN, the repeated pixel must be
  // added to the pixel table, otherwise the decoder may produce incorrect result.
  {
    uint32_t hbyte0 = src[0];

    if (hbyte0 >= kQoiOpRun && hbyte0 < kQoiOpRun + 62u) {
      uint32_t hash = unpackedPixel.hash();
      packedTable[hash] = packedPixel;
      unpackedTable[hash] = unpackedPixel;
    }
  }

  for (;;) {
    size_t remaining = (size_t)(end - src);
    if (BL_UNLIKELY(remaining < kMinRemainingBytesOfNextChunk)) {
      return blTraceError(BL_ERROR_DATA_TRUNCATED);
    }

    uint32_t hbyte0 = src[0];
    uint32_t hbyte1 = src[1];
    src++;

    if (hbyte0 < kQoiOpRun) {
      // QOI_OP_INDEX + QOI_OP_DIFF + QOI_OP_LUMA
      // ========================================

      if (hbyte0 < 64u) {
        // Handle QOI_OP_INDEX - 6-bit index to a pixel table (hbyte0 = 0b00xxxxxx).
        packedPixel = packedTable[hbyte0];
        unpackedPixel = unpackedTable[hbyte0];

        *dstPtr = packedPixel;
        if (BL_LIKELY(++dstPtr != dstEnd)) {
          if (!(hbyte1 < 64u)) {
            continue;
          }

          packedPixel = packedTable[hbyte1];
          unpackedPixel = unpackedTable[hbyte1];
          src++;

          *dstPtr = packedPixel;
          if (++dstPtr != dstEnd) {
            continue;
          }
        }
        hbyte0 = 0;
      }
      else {
        // Handle QOI_OP_DIFF and QOI_OP_LUMA chunks.
        {
          src += hbyte0 >> 7;

          uint32_t packedDelta = qoiIndexDiffLumaLUT[hbyte0 - 64u];
          hbyte1 &= packedDelta;
          packedDelta >>= 8;

          unpackedPixel.addRB((hbyte1 | (hbyte1 << 12)) & 0x000F000Fu);
          unpackedPixel.add(UnpackedPixel::unpack(packedDelta));
          unpackedPixel.mask();
        }

store_pixel:
        hbyte0 = unpackedPixel.hash();

        packedPixel = unpackedPixel.pack<kHasAlpha>();
        unpackedTable[hbyte0] = unpackedPixel;

        *dstPtr = packedPixel;
        packedTable[hbyte0] = packedPixel;

        if (++dstPtr != dstEnd)
          continue;

        hbyte0 = 0;
      }
    }
    else {
      // QOI_OP_RUN + QOI_OP_RGB + QOI_OP_RGBA
      // =====================================

      if (hbyte0 >= kQoiOpRgb) {
        // Handle both QOI_OP_RGB and QOI_OP_RGBA at the same time.
        unpackedPixel.opRGBX(hbyte0, UnpackedPixel::unpackRGBA(hbyte1, src[1], src[2], src[3]));

        // Advance by either 3 (RGB) or 4 (RGBA) bytes.
        src += hbyte0 - 251u;
        goto store_pixel;
      }
      else {
        // Run-length encoding repeats the previous pixel by `(hbyte0 & 0x3F) + 1` times (N stored with a bias of -1).
        hbyte0 = size_t(hbyte0 & 0x3Fu) + 1u;

store_rle:
        {
          size_t limit = (size_t)(dstEnd - dstPtr);
          size_t fill = blMin<size_t>(hbyte0, limit);

          hbyte0 -= uint32_t(fill);
          dstPtr = fillRgba32(dstPtr, packedPixel, fill);

          if (dstPtr != dstEnd) {
            continue;
          }
        }
      }
    }

    if (BL_UNLIKELY(--h == 0)) {
      return BL_SUCCESS;
    }

    dstRow += dstStride;
    dstPtr = reinterpret_cast<uint32_t*>(dstRow);
    dstEnd = dstPtr + w;

    // True if we are inside an unfinished QOI_OP_RUN that spans across two or more rows.
    if (hbyte0 != 0) {
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

  data += kQoiHeaderSize;
  if (data >= end)
    return blTraceError(BL_ERROR_DATA_TRUNCATED);

  BLImageData imageData;
  BL_PROPAGATE(imageOut->create(int(w), int(h), format));
  BL_PROPAGATE(imageOut->makeMutable(&imageData));

  uint8_t* dstRow = static_cast<uint8_t*>(imageData.pixelData);
  intptr_t dstStride = imageData.stride;

  uint32_t packedTable[64];
  fillRgba32(packedTable, depth == 32 ? 0u : 0xFF000000u, 64);

  UnpackedPixel unpackedTable[64] {};

  if (depth == 32)
    BL_PROPAGATE(decodeQoiData<true>(dstRow, dstStride, w, h, packedTable, unpackedTable, data, end));
  else
    BL_PROPAGATE(decodeQoiData<false>(dstRow, dstStride, w, h, packedTable, unpackedTable, data, end));

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

// QOI isn't good for compressing alpha-only images - we can optimize the encoder's performance, but not the final size.
static uint8_t* encodeQoiDataA8(uint8_t* dstData, uint32_t w, uint32_t h, const uint8_t* srcData, intptr_t srcStride) noexcept {
  // NOTE: Use an initial value which is not representable, because the encoder/decoder starts with RGB==0,
  // which would decode badly into RGBA formats (the components would be zero and thus it would not be the
  // same as when used by Blend2D, which defaults to having RGB components the same as 0xFF premultiplied).
  uint32_t pixel = 0xFFFFFFFFu;
  uint16_t pixelTable[64];

  for (size_t i = 0; i < 64; i++) {
    pixelTable[i] = 0xFFFFu;
  }

  srcStride -= intptr_t(w);
  uint32_t x = w;

  for (;;) {
    uint32_t p = *srcData++;

    // Run length encoding.
    if (p == pixel) {
      size_t n = 1;
      x--;

      for (;;) {
        uint32_t prevX = x;

        while (x) {
          p = *srcData++;
          if (p != pixel)
            break;
          x--;
        }

        n += size_t(prevX - x);

        if (x == 0 && --h != 0) {
          srcData += srcStride;
          x = w;
        }
        else {
          break;
        }
      }

      do {
        size_t run = blMin<size_t>(n, 62u);
        *dstData++ = uint8_t(run + (kQoiOpRun - 1u));
        n -= run;
      } while (n);

      if (!x) {
        return dstData;
      }
    }

    uint32_t hash = hashPixelA8(p);

    if (pixelTable[hash] == p) {
      *dstData++ = uint8_t(kQoiOpIndex | hash);
    }
    else {
      pixelTable[hash] = uint16_t(p);

      dstData[0] = kQoiOpRgba;
      dstData[1] = uint8_t(0xFFu);
      dstData[2] = uint8_t(0xFFu);
      dstData[3] = uint8_t(0xFFu);
      dstData[4] = uint8_t(p);
      dstData += 5;
    }

    pixel = p;

    if (--x != 0u)
      continue;

    if (--h == 0u)
      return dstData;

    srcData += srcStride;
    x = w;
  }
}

static uint8_t* encodeQoiDataXRGB32(uint8_t* dstData, uint32_t w, uint32_t h, const uint8_t* srcData, intptr_t srcStride) noexcept {
  BLRgba32 pixel = BLRgba32(0xFF000000u);
  uint32_t pixelTable[64] {};

  uint32_t x = w;
  srcStride -= intptr_t(w) * 4;

  for (;;) {
    BLRgba32 p = BLRgba32(MemOps::readU32a(srcData) | 0xFF000000u);
    srcData += 4;

    // Run length encoding.
    if (p == pixel) {
      size_t n = 1;
      x--;

      for (;;) {
        uint32_t prevX = x;

        while (x) {
          p = BLRgba32(MemOps::readU32a(srcData) | 0xFF000000u);
          srcData += 4;
          if (p != pixel)
            break;
          x--;
        }

        n += size_t(prevX - x);

        if (x == 0 && --h != 0) {
          srcData += srcStride;
          x = w;
        }
        else {
          break;
        }
      }

      do {
        size_t run = blMin<size_t>(n, 62u);
        *dstData++ = uint8_t(run + (kQoiOpRun - 1u));
        n -= run;
      } while (n);

      if (!x) {
        return dstData;
      }
    }

    uint32_t hash = hashPixelRGBA32(p.value);

    if (pixelTable[hash] == p.value) {
      *dstData++ = uint8_t(kQoiOpIndex | hash);
    }
    else {
      pixelTable[hash] = p.value;

      uint32_t dr = uint32_t(p.r()) - uint32_t(pixel.r());
      uint32_t dg = uint32_t(p.g()) - uint32_t(pixel.g());
      uint32_t db = uint32_t(p.b()) - uint32_t(pixel.b());

      uint32_t xr = uint32_t(dr + 2u) & 0xFFu;
      uint32_t xg = uint32_t(dg + 2u) & 0xFFu;
      uint32_t xb = uint32_t(db + 2u) & 0xFFu;

      if ((xr | xg | xb) <= 0x3u) {
        *dstData++ = uint8_t(kQoiOpDiff | (xr << 4) | (xg << 2) | xb);
      }
      else {
        uint32_t dg_r = dr - dg;
        uint32_t dg_b = db - dg;

        xr = (dg_r + 8u) & 0xFFu;
        xg = (dg  + 32u) & 0xFFu;
        xb = (dg_b + 8u) & 0xFFu;

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

    if (--x != 0u)
      continue;

    if (--h == 0u)
      return dstData;

    srcData += srcStride;
    x = w;
  }
}

static uint8_t* encodeQoiDataPRGB32(uint8_t* dstData, uint32_t w, uint32_t h, const uint8_t* srcData, intptr_t srcStride) noexcept {
  BLRgba32 pixelPM = BLRgba32(0xFF000000u);
  BLRgba32 pixelNP = BLRgba32(0xFF000000u);
  uint32_t pixelTable[64] {};

  uint32_t x = w;
  srcStride -= intptr_t(w) * 4;

  for (;;) {
    BLRgba32 pm = BLRgba32(MemOps::readU32a(srcData));
    srcData += 4;

    // Run length encoding.
    if (pm == pixelPM) {
      size_t n = 1;
      x--;

      for (;;) {
        uint32_t prevX = x;

        while (x) {
          pm = BLRgba32(MemOps::readU32a(srcData));
          srcData += 4;
          if (pm != pixelPM)
            break;
          x--;
        }

        n += size_t(prevX - x);

        if (x == 0 && --h != 0) {
          srcData += srcStride;
          x = w;
        }
        else {
          break;
        }
      }

      do {
        size_t run = blMin<size_t>(n, 62u);
        *dstData++ = uint8_t(run + (kQoiOpRun - 1u));
        n -= run;
      } while (n);

      if (!x) {
        return dstData;
      }
    }

    BLRgba32 np = BLRgba32(PixelOps::Scalar::cvt_argb32_8888_from_prgb32_8888(pm.value));
    uint32_t hash = hashPixelRGBA32(np.value);

    if (pixelTable[hash] == np.value) {
      *dstData++ = uint8_t(kQoiOpIndex | hash);
    }
    else {
      pixelTable[hash] = np.value;

      // To use delta, the previous pixel needs to have the same alpha value unfortunately.
      if (pixelNP.a() == np.a()) {
        uint32_t dr = uint32_t(np.r()) - uint32_t(pixelNP.r());
        uint32_t dg = uint32_t(np.g()) - uint32_t(pixelNP.g());
        uint32_t db = uint32_t(np.b()) - uint32_t(pixelNP.b());

        uint32_t xr = (dr + 2u) & 0xFFu;
        uint32_t xg = (dg + 2u) & 0xFFu;
        uint32_t xb = (db + 2u) & 0xFFu;

        if ((xr | xg | xb) <= 0x3u) {
          *dstData++ = uint8_t(kQoiOpDiff | (xr << 4) | (xg << 2) | xb);
        }
        else {
          uint32_t dg_r = dr - dg;
          uint32_t dg_b = db - dg;

          xr = (dg_r + 8) & 0xFFu;
          xg = (dg  + 32) & 0xFFu;
          xb = (dg_b + 8) & 0xFFu;

          if ((xr | xb) <= 0xFu && xg <= 0x3Fu) {
            dstData[0] = uint8_t(kQoiOpLuma | xg);
            dstData[1] = uint8_t((xr << 4) | xb);
            dstData += 2;
          }
          else {
            dstData[0] = kQoiOpRgb;
            dstData[1] = uint8_t(np.r());
            dstData[2] = uint8_t(np.g());
            dstData[3] = uint8_t(np.b());
            dstData += 4;
          }
        }
      }
      else {
        dstData[0] = kQoiOpRgba;
        dstData[1] = uint8_t(np.r());
        dstData[2] = uint8_t(np.g());
        dstData[3] = uint8_t(np.b());
        dstData[4] = uint8_t(np.a());
        dstData += 5;
      }
    }

    pixelPM = pm;
    pixelNP = np;

    if (--x != 0u)
      continue;

    if (--h == 0u)
      return dstData;

    srcData += srcStride;
    x = w;
  }
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
