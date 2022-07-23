// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CODEC_PNGCODEC_P_H_INCLUDED
#define BLEND2D_CODEC_PNGCODEC_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../image_p.h"
#include "../imagecodec.h"
#include "../imagedecoder.h"
#include "../imageencoder.h"
#include "../pixelconverter.h"
#include "../runtime_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_codec_impl
//! \{

enum BLPngDecoderStatusFlags : uint32_t {
  BL_PNG_DECODER_STATUS_SEEN_IHDR = 0x00000001u,
  BL_PNG_DECODER_STATUS_SEEN_IDAT = 0x00000002u,
  BL_PNG_DECODER_STATUS_SEEN_IEND = 0x00000004u,
  BL_PNG_DECODER_STATUS_SEEN_PLTE = 0x00000010u,
  BL_PNG_DECODER_STATUS_SEEN_tRNS = 0x00000020u,
  BL_PNG_DECODER_STATUS_SEEN_CgBI = 0x00000040u
};

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

struct BLPngEncoderImpl : public BLImageEncoderImpl {
  uint8_t compressionLevel;
};

struct BLPngCodecImpl : public BLImageCodecImpl {};

BL_HIDDEN void blPngCodecOnInit(BLRuntimeContext* rt, BLArray<BLImageCodec>* codecs) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_CODEC_PNGCODEC_P_H_INCLUDED
