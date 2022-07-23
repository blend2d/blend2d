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
#include "../support/intops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_codec_impl
//! \{

struct BLBmpFileHeader;
struct BLBmpInfoHeader;

struct BLBmpOS2InfoHeader;
struct BLBmpWinInfoHeader;

enum BLBmpHeaderSize : uint32_t {
  BL_BMP_HEADER_SIZE_OS2_V1           = 12,
  BL_BMP_HEADER_SIZE_WIN_V1           = 40,
  BL_BMP_HEADER_SIZE_WIN_V2           = 52,
  BL_BMP_HEADER_SIZE_WIN_V3           = 56,
  BL_BMP_HEADER_SIZE_WIN_V4           = 108,
  BL_BMP_HEADER_SIZE_WIN_V5           = 124
};

enum BLBmpCompression : uint32_t {
  BL_BMP_COMPRESSION_RGB              = 0,
  BL_BMP_COMPRESSION_RLE8             = 1,
  BL_BMP_COMPRESSION_RLE4             = 2,
  BL_BMP_COMPRESSION_BIT_FIELDS       = 3,
  BL_BMP_COMPRESSION_JPEG             = 4,
  BL_BMP_COMPRESSION_PNG              = 5,
  BL_BMP_COMPRESSION_ALPHA_BIT_FIELDS = 6,
  BL_BMP_COMPRESSION_CMYK             = 11,
  BL_BMP_COMPRESSION_CMYK_RLE8        = 12,
  BL_BMP_COMPRESSION_CMYK_RLE4        = 13
};

enum BLBmpColorSpace : uint32_t {
  BL_BMP_COLOR_SPACE_CALIBRATED_RGB   = 0,
  BL_BMP_COLOR_SPACE_DD_RGB           = 1,
  BL_BMP_COLOR_SPACE_DD_CMYK          = 2
};

enum BLBmpRLECmd : uint32_t {
  BL_BMP_RLE_CMD_LINE                 = 0,
  BL_BMP_RLE_CMD_STOP                 = 1,
  BL_BMP_RLE_CMD_MOVE                 = 2,
  BL_BMP_RLE_CMD_COUNT                = 3
};

enum : uint32_t {
  // Spec says that skipped pixels contain background color, transparent for us.
  BL_BMP_RLE_BACKGROUND               = 0x00000000u
};

//! Bitmap file signature [2 Bytes].
struct BLBmpFileSignature {
  //! Bitmap signature data - "BM".
  uint8_t data[2];
};

//! Bitmap File Header [12 Bytes] (we don't count signature here, it's separate).
struct BLBmpFileHeader {
  //! Bitmap file size in bytes.
  uint32_t fileSize;
  //! Reserved, should be zero.
  uint32_t reserved;
  //! Offset to image data (54, 124, ...).
  uint32_t imageOffset;

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  BL_INLINE void byteSwap() {
    fileSize          = BLIntOps::byteSwap32(fileSize);
    imageOffset       = BLIntOps::byteSwap32(imageOffset);
  }
};

//! All bitmap headers in one union.
struct BLBmpInfoHeader {
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
      headerSize      = BLIntOps::byteSwap32(headerSize);
      width           = BLIntOps::byteSwap16(width);
      height          = BLIntOps::byteSwap16(height);
      planes          = BLIntOps::byteSwap16(planes);
      bitsPerPixel    = BLIntOps::byteSwap16(bitsPerPixel);
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
    //! Compression methods used, see `BLBmpCompression`.
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
    //! ProfileData offset (in bytes), from the beginning of `BLBmpWinInfoHeader`.
    uint32_t profileData;
    //! Size, in bytes, of embedded profile data.
    uint32_t profileSize;
    //! Reserved, should be zero.
    uint32_t reserved;

    BL_INLINE void byteSwap() noexcept {
      headerSize      = BLIntOps::byteSwap32(headerSize);
      width           = BLIntOps::byteSwap32(width);
      height          = BLIntOps::byteSwap32(height);
      planes          = BLIntOps::byteSwap16(planes);
      bitsPerPixel    = BLIntOps::byteSwap16(bitsPerPixel);
      compression     = BLIntOps::byteSwap32(compression);
      imageSize       = BLIntOps::byteSwap32(imageSize);
      horzResolution  = BLIntOps::byteSwap32(horzResolution);
      vertResolution  = BLIntOps::byteSwap32(vertResolution);
      colorsUsed      = BLIntOps::byteSwap32(colorsUsed);
      colorsImportant = BLIntOps::byteSwap32(colorsImportant);
      rMask           = BLIntOps::byteSwap32(rMask);
      gMask           = BLIntOps::byteSwap32(gMask);
      bMask           = BLIntOps::byteSwap32(bMask);
      aMask           = BLIntOps::byteSwap32(aMask);
      colorspace      = BLIntOps::byteSwap32(colorspace);
      r.x             = BLIntOps::byteSwap32(r.x);
      r.y             = BLIntOps::byteSwap32(r.y);
      r.z             = BLIntOps::byteSwap32(r.z);
      g.x             = BLIntOps::byteSwap32(g.x);
      g.y             = BLIntOps::byteSwap32(g.y);
      g.z             = BLIntOps::byteSwap32(g.z);
      b.x             = BLIntOps::byteSwap32(b.x);
      b.y             = BLIntOps::byteSwap32(b.y);
      b.z             = BLIntOps::byteSwap32(b.z);
      rGamma          = BLIntOps::byteSwap32(rGamma);
      gGamma          = BLIntOps::byteSwap32(gGamma);
      bGamma          = BLIntOps::byteSwap32(bGamma);
      intent          = BLIntOps::byteSwap32(intent);
      profileData     = BLIntOps::byteSwap32(profileData);
      profileSize     = BLIntOps::byteSwap32(profileSize);
    }
  };

  union {
    uint32_t headerSize;
    OS2 os2;
    Win win;
  };

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
};

struct BLBmpDecoderImpl : public BLImageDecoderImpl {
  BLImageInfo imageInfo;
  BLBmpFileHeader file;
  BLBmpInfoHeader info;
  BLFormatInfo fmt;
  uint32_t stride;
};

struct BLBmpEncoderImpl : public BLImageEncoderImpl {};

struct BLBmpCodecImpl : public BLImageCodecImpl {};

BL_HIDDEN void blBmpCodecOnInit(BLRuntimeContext* rt, BLArray<BLImageCodec>* codecs) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_CODEC_BMPCODEC_P_H_INCLUDED
