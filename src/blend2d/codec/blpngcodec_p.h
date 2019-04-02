// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_CODEC_BLPNGCODEC_P_H
#define BLEND2D_CODEC_BLPNGCODEC_P_H

#include "../blapi-internal_p.h"
#include "../blimage_p.h"
#include "../blpixelconverter.h"
#include "../blsupport_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_codecs
//! \{

// ============================================================================
// [BLPngDecoderStatusFlags]
// ============================================================================

enum BLPngDecoderStatusFlags : uint32_t {
  BL_PNG_DECODER_STATUS_SEEN_IHDR = 0x00000001u,
  BL_PNG_DECODER_STATUS_SEEN_IDAT = 0x00000002u,
  BL_PNG_DECODER_STATUS_SEEN_IEND = 0x00000004u,
  BL_PNG_DECODER_STATUS_SEEN_PLTE = 0x00000010u,
  BL_PNG_DECODER_STATUS_SEEN_tRNS = 0x00000020u,
  BL_PNG_DECODER_STATUS_SEEN_CgBI = 0x00000040u
};

// ============================================================================
// [BLPngColorType]
// ============================================================================

enum BLPngColorType : uint32_t {
  //! Each pixel is a grayscale sample (1/2/4/8/16-bits per sample).
  BL_PNG_COLOR_TYPE0_LUM  = 0,
  //! Each pixel is an RGB triple (8/16-bits per sample).
  BL_PNG_COLOR_TYPE2_RGB  = 2,
  //! Each pixel is a palette index (1/2/4/8 bits per sample).
  BL_PNG_COLOR_TYPE3_PAL  = 3,
  //! Each pixel is a grayscale+alpha sample (8/16-bits per sample).
  BL_PNG_COLOR_TYPE4_LUMA = 4,
  //! Each pixel is an RGBA quad (8/16 bits per sample).
  BL_PNG_COLOR_TYPE6_RGBA = 6
};

// ============================================================================
// [BLPngFilterType]
// ============================================================================

enum PngFilterType : uint32_t {
  BL_PNG_FILTER_TYPE_NONE  = 0,
  BL_PNG_FILTER_TYPE_SUB   = 1,
  BL_PNG_FILTER_TYPE_UP    = 2,
  BL_PNG_FILTER_TYPE_AVG   = 3,
  BL_PNG_FILTER_TYPE_PAETH = 4,
  BL_PNG_FILTER_TYPE_COUNT = 5,

  //! Synthetic filter used only by Blend2D's reverse-filter implementation.
  BL_PNG_FILTER_TYPE_AVG0  = 5
};

// ============================================================================
// [BLPngCodec - DecoderImpl]
// ============================================================================

struct BLPngDecoderImpl : public BLImageDecoderImpl {
  //! Decoder image information.
  BLImageInfo imageInfo;
  //! Decoder status flags.
  uint32_t statusFlags;
  //! Color type.
  uint8_t colorType;
  //! Depth (depth per one sample).
  uint8_t sampleDepth;
  //! Number of samples (1, 2, 3, 4).
  uint8_t sampleCount;
  //! Contains "CgBI" chunk before "IHDR" and other violations.
  uint8_t cgbi;
};

// ============================================================================
// [BLPngCodec - EncoderImpl]
// ============================================================================

struct BLPngEncoderImpl : public BLImageEncoderImpl {};

// ============================================================================
// [BLPngCodec - Impl]
// ============================================================================

struct BLPngCodecImpl : public BLImageCodecImpl {};

// ============================================================================
// [BLPngCodec - Runtime Init]
// ============================================================================

BL_HIDDEN BLImageCodecImpl* blPngCodecRtInit(BLRuntimeContext* rt) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_CODEC_BLPNGCODEC_P_H
