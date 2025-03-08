// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CODEC_BMPCODEC_P_H_INCLUDED
#define BLEND2D_CODEC_BMPCODEC_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../image_p.h"
#include "../imagecodec.h"
#include "../imagedecoder.h"
#include "../imageencoder.h"
#include "../pixelconverter.h"
#include "../runtime_p.h"
#include "../support/intops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_codec_impl
//! \{

namespace bl {
namespace Bmp {

static constexpr uint32_t kHeaderSizeOS2_V1 = 12;
static constexpr uint32_t kHeaderSizeWIN_V1 = 40;
static constexpr uint32_t kHeaderSizeWIN_V2 = 52;
static constexpr uint32_t kHeaderSizeWIN_V3 = 56;
static constexpr uint32_t kHeaderSizeWIN_V4 = 108;
static constexpr uint32_t kHeaderSizeWIN_V5 = 12;

static constexpr uint32_t kCompressionRGB = 0;
static constexpr uint32_t kCompressionRLE8 = 1;
static constexpr uint32_t kCompressionRLE4 = 2;
static constexpr uint32_t kCompressionBitFields = 3;
static constexpr uint32_t kCompressionJPEG = 4;
static constexpr uint32_t kCompressionPNG = 5;
static constexpr uint32_t kCompressionAlphaBitFields = 6;
static constexpr uint32_t kCompressionCMYK = 11;
static constexpr uint32_t kCompressionCMYK_RLE8 = 12;
static constexpr uint32_t kCompressionCMYK_RLE4 = 1;
static constexpr uint32_t kCompressionValueCount = 13;

static constexpr uint32_t kColorSpaceCalibratedRGB = 0;
static constexpr uint32_t kColorSpaceDD_RGB = 1;
static constexpr uint32_t kColorSpaceDD_CMYK = 2;

static constexpr uint32_t kRleLine = 0;
static constexpr uint32_t kRleStop = 1;
static constexpr uint32_t kRleMove = 2;
static constexpr uint32_t kRleCount = 3;

// Specification describes that skipped pixels contain background color, transparent for us.
static constexpr uint32_t kRleBackground = 0u;

//! Bitmap file signature [2 Bytes].
struct BmpFileSignature {
  //! Bitmap signature data - "BM".
  uint8_t data[2];
};

//! Bitmap File Header [12 Bytes] (we don't count signature here, it's separate).
struct BmpFileHeader {
  //! Bitmap file size in bytes.
  uint32_t fileSize;
  //! Reserved, should be zero.
  uint32_t reserved;
  //! Offset to image data (54, 124, ...).
  uint32_t imageOffset;

  BL_INLINE void reset() noexcept { *this = BmpFileHeader{}; }

  BL_INLINE void byteSwap() {
    fileSize          = IntOps::byteSwap32(fileSize);
    imageOffset       = IntOps::byteSwap32(imageOffset);
  }
};

//! All bitmap headers in one union.
struct BmpInfoHeader {
  //! Helper that contains XYZ (colorspace).
  struct XYZ {
    uint32_t x, y, z;
  };

  //! Bitmap OS/2 Header [12 Bytes].
  struct OS2 {
    //! Header size (40, 52).
    uint32_t headerSize;
    //! Bitmap width (16-bit value).
    int16_t width;
    //! Bitmap height (16-bit value).
    int16_t height;
    //! Number of color planes (always 1).
    uint16_t planes;
    //! Bits per pixel (1, 4, 8 or 24).
    uint16_t bitsPerPixel;

    BL_INLINE void byteSwap() noexcept {
      headerSize      = IntOps::byteSwap32(headerSize);
      width           = IntOps::byteSwap16(width);
      height          = IntOps::byteSwap16(height);
      planes          = IntOps::byteSwap16(planes);
      bitsPerPixel    = IntOps::byteSwap16(bitsPerPixel);
    }
  };

  //! Windows Info Header [40..124 Bytes].
  struct Win {
    // Version 1.

    //! Header size (40, 52, 56, 108, 124).
    uint32_t headerSize;
    //! Bitmap width.
    int32_t width;
    //! Bitmap height.
    int32_t height;
    //! Count of planes, always 1.
    uint16_t planes;
    //! Bits per pixel (1, 4, 8, 16, 24 or 32).
    uint16_t bitsPerPixel;
    //! Compression methods used.
    uint32_t compression;
    //! Image data size (in bytes).
    uint32_t imageSize;
    //! Horizontal resolution in pixels per meter.
    uint32_t horzResolution;
    //! Vertical resolution in pixels per meter.
    uint32_t vertResolution;
    //! Number of colors in the image.
    uint32_t colorsUsed;
    //! Minimum number of important colors.
    uint32_t colorsImportant;

    // Version 2 and 3.

    union {
      uint32_t masks[4];
      struct {
        //! Mask identifying bits of red component.
        uint32_t rMask;
        //! Mask identifying bits of green component.
        uint32_t gMask;
        //! Mask identifying bits of blue component.
        uint32_t bMask;
        //! Mask identifying bits of alpha component [Version 3+ only].
        uint32_t aMask;
      };
    };

    // Version 4.

    //! Color space type.
    uint32_t colorspace;
    //! Coordinates of red endpoint.
    XYZ r;
    //! Coordinates of green endpoint.
    XYZ g;
    //! Coordinates of blue endpoint.
    XYZ b;
    //! Gamma red coordinate scale value.
    uint32_t rGamma;
    //! Gamma green coordinate scale value.
    uint32_t gGamma;
    //! Gamma blue coordinate scale value.
    uint32_t bGamma;

    // Version 5.

    //! Rendering intent for bitmap.
    uint32_t intent;
    //! ProfileData offset (in bytes), from the beginning of `BmpInfoHeader::Win`.
    uint32_t profileData;
    //! Size, in bytes, of embedded profile data.
    uint32_t profileSize;
    //! Reserved, should be zero.
    uint32_t reserved;

    BL_INLINE void byteSwap() noexcept {
      headerSize      = IntOps::byteSwap32(headerSize);
      width           = IntOps::byteSwap32(width);
      height          = IntOps::byteSwap32(height);
      planes          = IntOps::byteSwap16(planes);
      bitsPerPixel    = IntOps::byteSwap16(bitsPerPixel);
      compression     = IntOps::byteSwap32(compression);
      imageSize       = IntOps::byteSwap32(imageSize);
      horzResolution  = IntOps::byteSwap32(horzResolution);
      vertResolution  = IntOps::byteSwap32(vertResolution);
      colorsUsed      = IntOps::byteSwap32(colorsUsed);
      colorsImportant = IntOps::byteSwap32(colorsImportant);
      rMask           = IntOps::byteSwap32(rMask);
      gMask           = IntOps::byteSwap32(gMask);
      bMask           = IntOps::byteSwap32(bMask);
      aMask           = IntOps::byteSwap32(aMask);
      colorspace      = IntOps::byteSwap32(colorspace);
      r.x             = IntOps::byteSwap32(r.x);
      r.y             = IntOps::byteSwap32(r.y);
      r.z             = IntOps::byteSwap32(r.z);
      g.x             = IntOps::byteSwap32(g.x);
      g.y             = IntOps::byteSwap32(g.y);
      g.z             = IntOps::byteSwap32(g.z);
      b.x             = IntOps::byteSwap32(b.x);
      b.y             = IntOps::byteSwap32(b.y);
      b.z             = IntOps::byteSwap32(b.z);
      rGamma          = IntOps::byteSwap32(rGamma);
      gGamma          = IntOps::byteSwap32(gGamma);
      bGamma          = IntOps::byteSwap32(bGamma);
      intent          = IntOps::byteSwap32(intent);
      profileData     = IntOps::byteSwap32(profileData);
      profileSize     = IntOps::byteSwap32(profileSize);
    }
  };

  union {
    uint32_t headerSize;
    OS2 os2;
    Win win;
  };

  BL_INLINE void reset() noexcept { *this = BmpInfoHeader{}; }
};

BL_HIDDEN void bmpCodecOnInit(BLRuntimeContext* rt, BLArray<BLImageCodec>* codecs) noexcept;

} // {Bmp}
} // {bl}

struct BLBmpDecoderImpl : public BLImageDecoderImpl {
  BLImageInfo imageInfo;
  bl::Bmp::BmpFileHeader file;
  bl::Bmp::BmpInfoHeader info;
  BLFormatInfo fmt;
  uint32_t stride;
};

struct BLBmpEncoderImpl : public BLImageEncoderImpl {};

struct BLBmpCodecImpl : public BLImageCodecImpl {};

//! \}
//! \endcond

#endif // BLEND2D_CODEC_BMPCODEC_P_H_INCLUDED
