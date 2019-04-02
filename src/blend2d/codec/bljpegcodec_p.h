// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_CODEC_BLJPEGCODEC_P_H
#define BLEND2D_CODEC_BLJPEGCODEC_P_H

#include "../blapi-internal_p.h"
#include "../blimage_p.h"
#include "../blpixelconverter.h"
#include "../blsupport_p.h"
#include "../codec/bljpeghuffman_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_codecs
//! \{

// ============================================================================
// [BLJpegCodec - Constants]
// ============================================================================

enum BLJpegConstants : uint32_t {
  BL_JPEG_DCT_SIZE              = 8,           //!< Size of JPEG's DCT block (N).
  BL_JPEG_DCT_SIZE_2            = 8 * 8        //!< Size of JPEG's DCT block squared (NxN).
};

//! JPEG markers.
enum JpegMarker : uint32_t {
  BL_JPEG_MARKER_NULL           = 0x00u,       //!< A "stuff byte" used in Huffman stream to encode 0xFF, which is otherwise used as a marker.
  BL_JPEG_MARKER_TEM            = 0x01u,       //!< Temporary use in arithmetic coding.
  BL_JPEG_MARKER_RES            = 0x02u,       //!< Reserved (first) (0x02..0xBF).
  BL_JPEG_MARKER_RES_LAST       = 0xBFu,       //!< Reserved (last).

  BL_JPEG_MARKER_SOF0           = 0xC0u,       //!< Start of Frame 0 - Baseline DCT (Huffman).
  BL_JPEG_MARKER_SOF1           = 0xC1u,       //!< Start of Frame 1 - Sequential DCT (Huffman).
  BL_JPEG_MARKER_SOF2           = 0xC2u,       //!< Start of Frame 2 - Progressive DCT (Huffman).
  BL_JPEG_MARKER_SOF3           = 0xC3u,       //!< Start of Frame 3 - Lossless (Huffman).
  BL_JPEG_MARKER_DHT            = 0xC4u,       //!< Define Huffman Table (0xC4).
  BL_JPEG_MARKER_SOF5           = 0xC5u,       //!< Start of Frame 5 - Differential Sequential DCT (Huffman).
  BL_JPEG_MARKER_SOF6           = 0xC6u,       //!< Start of Frame 6 - Differential Progressive DCT (Huffman).
  BL_JPEG_MARKER_SOF7           = 0xC7u,       //!< Start of Frame 7 - Differential Lossless (Huffman).
  BL_JPEG_MARKER_JPG            = 0xC8u,       //!< JPEG Extensions (0xC8).
  BL_JPEG_MARKER_SOF9           = 0xC9u,       //!< Start of Frame 9 - Sequential DCT (Arithmetic).
  BL_JPEG_MARKER_SOF10          = 0xCAu,       //!< Start of Frame 10 - Progressive DCT (Arithmetic).
  BL_JPEG_MARKER_SOF11          = 0xCBu,       //!< Start of Frame 11 - Lossless (Arithmetic).
  BL_JPEG_MARKER_DAC            = 0xCCu,       //!< Define Arithmetic Coding (0xCC).
  BL_JPEG_MARKER_SOF13          = 0xCDu,       //!< Start of Frame 13 - Differential Sequential DCT (Arithmetic).
  BL_JPEG_MARKER_SOF14          = 0xCEu,       //!< Start of Frame 14 - Differential Progressive DCT (Arithmetic).
  BL_JPEG_MARKER_SOF15          = 0xCFu,       //!< Start of Frame 15 - Differential Lossless (Arithmetic).

  BL_JPEG_MARKER_RST            = 0xD0u,       //!< Restart Marker (first) (0xD0..0xD7)
  BL_JPEG_MARKER_RST_LAST       = 0xD7u,       //!< Restart Marker (last)
  BL_JPEG_MARKER_SOI            = 0xD8u,       //!< Start of BLImage (0xD8).
  BL_JPEG_MARKER_EOI            = 0xD9u,       //!< End of BLImage (0xD9).
  BL_JPEG_MARKER_SOS            = 0xDAu,       //!< Start of Scan (0xDA).
  BL_JPEG_MARKER_DQT            = 0xDBu,       //!< Define Quantization Table (0xDB).
  BL_JPEG_MARKER_DNL            = 0xDCu,       //!< Define Number of Lines (0xDC).
  BL_JPEG_MARKER_DRI            = 0xDDu,       //!< Define Restart Interval (0xDD).
  BL_JPEG_MARKER_DHP            = 0xDEu,       //!< Define Hierarchical Progression (0xDE)
  BL_JPEG_MARKER_EXP            = 0xDFu,       //!< Expand Reference Component (0xDF).

  BL_JPEG_MARKER_APP            = 0xE0u,       //!< Application (first) (0xE0..0xEF).
  BL_JPEG_MARKER_APP_LAST       = 0xEFu,       //!< Application (last).

  BL_JPEG_MARKER_APP0           = 0xE0u,       //!< APP0 - JFIF, JFXX (0xE0).
  BL_JPEG_MARKER_APP1           = 0xE1u,       //!< APP1 - EXIF, XMP (0xE1).
  BL_JPEG_MARKER_APP2           = 0xE2u,       //!< APP2 - ICC (0xE2).
  BL_JPEG_MARKER_APP3           = 0xE3u,       //!< APP3 - META (0xE3).
  BL_JPEG_MARKER_APP4           = 0xE4u,       //!< APP4 (0xE4).
  BL_JPEG_MARKER_APP5           = 0xE5u,       //!< APP5 (0xE5).
  BL_JPEG_MARKER_APP6           = 0xE6u,       //!< APP6 (0xE6).
  BL_JPEG_MARKER_APP7           = 0xE7u,       //!< APP7 (0xE7).
  BL_JPEG_MARKER_APP8           = 0xE8u,       //!< APP8 (0xE8).
  BL_JPEG_MARKER_APP9           = 0xE9u,       //!< APP9 (0xE9).
  BL_JPEG_MARKER_APP10          = 0xEAu,       //!< APP10 (0xEA).
  BL_JPEG_MARKER_APP11          = 0xEBu,       //!< APP11 (0xEB).
  BL_JPEG_MARKER_APP12          = 0xECu,       //!< APP12 - Picture information (0xEC).
  BL_JPEG_MARKER_APP13          = 0xEDu,       //!< APP13 - Adobe IRB (0xED).
  BL_JPEG_MARKER_APP14          = 0xEEu,       //!< APP14 - Adobe (0xEE).
  BL_JPEG_MARKER_APP15          = 0xEFu,       //!< APP15 (0xEF).

  BL_JPEG_MARKER_EXT            = 0xF0u,       //!< JPEG Extension (first) (0xF0..0xFD).
  BL_JPEG_MARKER_EXT_LAST       = 0xFDu,       //!< JPEG Extension (last).
  BL_JPEG_MARKER_COM            = 0xFEu,       //!< Comment (0xFE).

  BL_JPEG_MARKER_INVALID        = 0xFFu        //!< Invalid (0xFF), sometimes used as padding.
};

//! JPEG colorspace type.
enum BLJpegColorspaceType : uint32_t {
  BL_JPEG_COLORSPACE_NONE       = 0,           //!< Colorspace is unknown / unspecified.
  BL_JPEG_COLORSPACE_Y          = 1,           //!< Colorspace is Y-only (grayscale).
  BL_JPEG_COLORSPACE_RGB        = 2,           //!< Colorspace is sRGB.
  BL_JPEG_COLORSPACE_YCBCR      = 3,           //!< Colorspace is YCbCr.
  BL_JPEG_COLORSPACE_CMYK       = 4,           //!< Colorspace is CMYK.
  BL_JPEG_COLORSPACE_YCCK       = 5,           //!< Colorspace is YCbCrK.
  BL_JPEG_COLORSPACE_COUNT      = 6            //!< Count of JPEG's colorspace types.
};

//! JPEG's density unit specified by APP0-JFIF marker.
enum BLJpegDensityUnit : uint32_t {
  BL_JPEG_DENSITY_ONLY_ASPECT   = 0,
  BL_JPEG_DENSITY_PIXELS_PER_IN = 1,
  BL_JPEG_DENSITY_PIXELS_PER_CM = 2,
  BL_JPEG_DENSITY_COUNT         = 3
};

//! JPEG's thumbnail format specified by APP0-JFXX marker.
enum BLJpegThumbnailFormat : uint32_t {
  BL_JPEG_THUMBNAIL_JPEG        = 0,
  BL_JPEG_THUMBNAIL_PAL8        = 1,
  BL_JPEG_THUMBNAIL_RGB24       = 2,
  BL_JPEG_THUMBNAIL_COUNT       = 3
};

//! JPEG's sampling point as specified by JFIF-APP0 marker.
enum BLJpegSamplingPoint : uint32_t {
  BL_JPEG_SAMPLING_UNKNOWN      = 0,           //!< Unknown / not specified sampling point (no JFIF-APP0 marker).
  BL_JPEG_SAMPLING_COSITTED     = 1,           //!< Co-sitted sampling point (specified by JFIF-APP0 marker).
  BL_JPEG_SAMPLING_CENTERED     = 2            //!< Centered sampling point (specified by JFIF-APP0 marker).
};

//! JPEG's table class selector (DC, AC).
enum BLJpegTableClass : uint32_t {
  BL_JPEG_TABLE_DC              = 0,           //!< DC table.
  BL_JPEG_TABLE_AC              = 1,           //!< AC table.
  BL_JPEG_TABLE_COUNT           = 2            //!< Number of tables.
};

//! JPEG decoder flags - bits of information collected from JPEG markers.
enum BLJpegDecoderStatusFlags : uint32_t {
  BL_JPEG_DECODER_DONE_SOI      = 0x00000001u,
  BL_JPEG_DECODER_DONE_EOI      = 0x00000002u,
  BL_JPEG_DECODER_DONE_JFIF     = 0x00000004u,
  BL_JPEG_DECODER_DONE_JFXX     = 0x00000008u,
  BL_JPEG_DECODER_DONE_EXIF     = 0x00000010u,
  BL_JPEG_DECODER_HAS_THUMB     = 0x80000000u
};

// ============================================================================
// [BLJpegCodec - Utilities]
// ============================================================================

//! Get whether the marker `m` is a SOF marker.
static BL_INLINE bool blJpegMarkerIsSOF(uint32_t m) noexcept { return m >= BL_JPEG_MARKER_SOF0 && m <= BL_JPEG_MARKER_SOF2; }
//! Get whether the marker `m` is an RST marker.
static BL_INLINE bool blJpegMarkerIsRST(uint32_t m) noexcept { return m >= BL_JPEG_MARKER_RST && m <= BL_JPEG_MARKER_RST_LAST; }
//! Get whether the marker `m` is an APP marker.
static BL_INLINE bool blJpegMarkerIsAPP(uint32_t m) noexcept { return m >= BL_JPEG_MARKER_APP && m <= BL_JPEG_MARKER_APP_LAST; }

// ============================================================================
// [BLJpegCodec - Structs]
// ============================================================================

template<typename T>
struct alignas(16) BLJpegBlock {
  T data[BL_JPEG_DCT_SIZE_2];

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
};

struct BLJpegMcuInfo {
  //! MCU width/height in blocks (maximum sampling factor of all components).
  struct {
    uint8_t w;
    uint8_t h;
  } sf;

  //! MCU width/height in pixels (resolution of a single MCU).
  struct {
    uint8_t w;
    uint8_t h;
  } px;

  //! Number of MCUs in horizontal/vertical direction.
  struct {
    uint32_t w;
    uint32_t h;
  } count;

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
};

// ============================================================================
// [BLJpegDecoder - Structs]
// ============================================================================

struct BLJpegDecoderComponent {
  //! Raster data.
  uint8_t* data;

  //! Component ID.
  uint8_t compId;
  //! Quantization table ID.
  uint8_t quantId;
  //! DC Huffman-Table ID.
  uint8_t dcId;
  //! AC Huffman-Table ID.
  uint8_t acId;

  //! Effective width.
  uint32_t pxW;
  //! Effective height.
  uint32_t pxH;
  //! Oversized width to match the total width requires by all MCUs.
  uint32_t osW;
  //! Oversized height to match the total height required by all MCUs.
  uint32_t osH;
  //! Number of 8x8 blocks in horizontal direction.
  uint32_t blW;
  //! Number of 8x8 blocks in vertical direction.
  uint32_t blH;

  //! Horizontal sampling factor (width).
  uint8_t sfW;
  //! Vertical sampling factor (height).
  uint8_t sfH;

  //! DC prediction (modified during decoding phase).
  int32_t dcPred;
  //! Coefficients used only by progressive JPEGs.
  int16_t* coeff;

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
};

//! Start of stream (SOS) data.
struct BLJpegDecoderSOS {
  //! Maps a stream component index into the `BLJpegDecoderComponent`.
  BLJpegDecoderComponent* scComp[4];
  //! Count of components in this stream.
  uint8_t scCount;
  //! Start of spectral selection.
  uint8_t ssStart;
  //! End of spectral selection.
  uint8_t ssEnd;
  //! Successive approximation low bit.
  uint8_t saLowBit;
  //! Successive approximation high bit.
  uint8_t saHighBit;

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
};

//! In case of RGB24 or PAL8 thumbnail data, the index points to the first
//! byte describing W, H, and then data follows. In case of an embedded JPEG
//! the `index` points to the first byte of that JPEG. So to decode the RAW
//! uncompressed PAL8/RGB24 data, skip first two bytes, that describe W/H,
//! which is already filled in this struct.
struct BLJpegDecoderThumbnail {
  //! Thumbnail format, see `BLJpegThumbnailFormat`.
  uint8_t format;
  //! Reserved
  uint8_t reserved;
  //! Thumbnail width and height (8-bit, as in JFIF spec.).
  uint8_t w, h;
  //! Index of the thumbnail data from the beginning of the stream.
  size_t index;
  //! Thumbnail data size (raw data, the JFIF headers not included here).
  size_t size;

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
};

// ============================================================================
// [BLJpegDecoder - Impl]
// ============================================================================

struct BLJpegDecoderImpl : public BLImageDecoderImpl {
  //! JPEG memory allocator (can allocate aligned blocks and keep track of them).
  BLScopedAllocator allocator;
  //! JPEG image information.
  BLImageInfo imageInfo;

  //! JPEG decoder flags, see `BLJpegDecoderStatusFlags`.
  uint32_t statusFlags;
  //! Restart interval as specified by DRI marker.
  uint32_t restartInterval;
  //! SOF marker (so we can select right decompression algorithm), initially zero.
  uint8_t sofMarker;
  //! Colorspace, see `JpegColorspace`.
  uint8_t colorspace;
  //! True if the data contains zero height (delayed height).
  uint8_t delayedHeight;
  //!< JFIF major version (if present).
  uint8_t jfifMajor;
  //!< JFIF minor version (if present).
  uint8_t jfifMinor;
  //! Mask of all defined DC tables.
  uint8_t dcTableMask;
  //! Mask of all defined AC tables.
  uint8_t acTableMask;
  //! Mask of all defined (de)quantization tables.
  uint8_t qTableMask;

  //! JPEG decoder MCU information.
  BLJpegMcuInfo mcu;
  //! JPEG decoder's current stream data (defined and overwritten by SOS markers).
  BLJpegDecoderSOS sos;
  //! JPEG decoder thumbnail data.
  BLJpegDecoderThumbnail thumb;
  //! JPEG decoder components.
  BLJpegDecoderComponent comp[4];
  //! JPEG Huffman DC tables.
  BLJpegDecoderHuffmanDCTable dcTable[4];
  //! JPEG Huffman AC tables.
  BLJpegDecoderHuffmanACTable acTable[4];
  //! JPEG quantization tables.
  BLJpegBlock<uint16_t> qTable[4];
};

// ============================================================================
// [BLJpegEncoder - Impl]
// ============================================================================

struct BLJpegEncoderImpl : public BLImageEncoderImpl {};

// ============================================================================
// [BLJpegCodec - Impl]
// ============================================================================

struct BLJpegCodecImpl : public BLImageCodecImpl {};

// ============================================================================
// [BLJpegCodec - Runtime Init]
// ============================================================================

BL_HIDDEN BLImageCodecImpl* blJpegCodecRtInit(BLRuntimeContext* rt) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_CODEC_BLJPEGCODEC_P_H
