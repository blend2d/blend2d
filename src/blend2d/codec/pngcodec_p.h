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

namespace bl {
namespace Png {

static constexpr uint32_t kColorType0_LUM  = 0; //!< Each pixel is a grayscale sample (1/2/4/8/16-bits per sample).
static constexpr uint32_t kColorType2_RGB  = 2; //!< Each pixel is an RGB triple (8/16-bits per sample).
static constexpr uint32_t kColorType3_PAL  = 3; //!< Each pixel is a palette index (1/2/4/8 bits per sample).
static constexpr uint32_t kColorType4_LUMA = 4; //!< Each pixel is a grayscale+alpha sample (8/16-bits per sample).
static constexpr uint32_t kColorType6_RGBA = 6; //!< Each pixel is an RGBA quad (8/16 bits per sample).

static constexpr uint32_t kFilterTypeNone  = 0;
static constexpr uint32_t kFilterTypeSub   = 1;
static constexpr uint32_t kFilterTypeUp    = 2;
static constexpr uint32_t kFilterTypeAvg   = 3;
static constexpr uint32_t kFilterTypePaeth = 4;
static constexpr uint32_t kFilterTypeCount = 5;

//! Synthetic filter used only by Blend2D's reverse-filter implementation.
static constexpr uint32_t kFilterTypeAvg0  = 5;

enum BLPngDecoderStatusFlags : uint32_t {
  BL_PNG_DECODER_STATUS_SEEN_IHDR = 0x00000001u,
  BL_PNG_DECODER_STATUS_SEEN_IDAT = 0x00000002u,
  BL_PNG_DECODER_STATUS_SEEN_IEND = 0x00000004u,
  BL_PNG_DECODER_STATUS_SEEN_PLTE = 0x00000010u,
  BL_PNG_DECODER_STATUS_SEEN_tRNS = 0x00000020u,
  BL_PNG_DECODER_STATUS_SEEN_CgBI = 0x00000040u
};

BL_HIDDEN void pngCodecOnInit(BLRuntimeContext* rt, BLArray<BLImageCodec>* codecs) noexcept;

} // {Png}
} // {bl}

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

//! \}
//! \endcond

#endif // BLEND2D_CODEC_PNGCODEC_P_H_INCLUDED
