// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_CODEC_BLBMPCODEC_P_H
#define BLEND2D_CODEC_BLBMPCODEC_P_H

#include "../blapi-internal_p.h"
#include "../blimage_p.h"
#include "../blpixelconverter.h"
#include "../blsupport_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_codecs
//! \{

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct BLBmpFileHeader;
struct BLBmpInfoHeader;

struct BLBmpOS2InfoHeader;
struct BLBmpWinInfoHeader;

// ============================================================================
// [BLBmpCodec - Constants]
// ============================================================================

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

// ============================================================================
// [BLBmpCodec - Structs]
// ============================================================================

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
    fileSize          = blByteSwap32(fileSize);
    imageOffset       = blByteSwap32(imageOffset);
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
      headerSize      = blByteSwap32(headerSize);
      width           = blByteSwap16(width);
      height          = blByteSwap16(height);
      planes          = blByteSwap16(planes);
      bitsPerPixel    = blByteSwap16(bitsPerPixel);
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
      headerSize      = blByteSwap32(headerSize);
      width           = blByteSwap32(width);
      height          = blByteSwap32(height);
      planes          = blByteSwap16(planes);
      bitsPerPixel    = blByteSwap16(bitsPerPixel);
      compression     = blByteSwap32(compression);
      imageSize       = blByteSwap32(imageSize);
      horzResolution  = blByteSwap32(horzResolution);
      vertResolution  = blByteSwap32(vertResolution);
      colorsUsed      = blByteSwap32(colorsUsed);
      colorsImportant = blByteSwap32(colorsImportant);
      rMask           = blByteSwap32(rMask);
      gMask           = blByteSwap32(gMask);
      bMask           = blByteSwap32(bMask);
      aMask           = blByteSwap32(aMask);
      colorspace      = blByteSwap32(colorspace);
      r.x             = blByteSwap32(r.x);
      r.y             = blByteSwap32(r.y);
      r.z             = blByteSwap32(r.z);
      g.x             = blByteSwap32(g.x);
      g.y             = blByteSwap32(g.y);
      g.z             = blByteSwap32(g.z);
      b.x             = blByteSwap32(b.x);
      b.y             = blByteSwap32(b.y);
      b.z             = blByteSwap32(b.z);
      rGamma          = blByteSwap32(rGamma);
      gGamma          = blByteSwap32(gGamma);
      bGamma          = blByteSwap32(bGamma);
      intent          = blByteSwap32(intent);
      profileData     = blByteSwap32(profileData);
      profileSize     = blByteSwap32(profileSize);
    }
  };

  union {
    uint32_t headerSize;
    OS2 os2;
    Win win;
  };

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
};

// ============================================================================
// [BLBmpCodec - DecoderImpl]
// ============================================================================

struct BLBmpDecoderImpl : public BLImageDecoderImpl {
  BLImageInfo imageInfo;
  BLBmpFileHeader file;
  BLBmpInfoHeader info;
  BLFormatInfo fmt;
  uint32_t stride;
};

// ============================================================================
// [BLBmpCodec - EncoderImpl]
// ============================================================================

struct BLBmpEncoderImpl : public BLImageEncoderImpl {};

// ============================================================================
// [BLBmpCodec - Impl]
// ============================================================================

struct BLBmpCodecImpl : public BLImageCodecImpl {};

// ============================================================================
// [BLBmpCodec - Runtime Init]
// ============================================================================

BL_HIDDEN BLImageCodecImpl* blBmpCodecRtInit(BLRuntimeContext* rt) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_CODEC_BLBMPCODEC_P_H
