// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CODEC_PNGCODEC_P_H_INCLUDED
#define BLEND2D_CODEC_PNGCODEC_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../array_p.h"
#include "../image_p.h"
#include "../imagecodec.h"
#include "../imagedecoder.h"
#include "../imageencoder.h"
#include "../pixelconverter.h"
#include "../runtime_p.h"
#include "../compression/deflatedecoder_p.h"
#include "../support/scopedbuffer_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_codec_impl
//! \{

namespace bl::Png {

static constexpr uint32_t kColorType0_LUM          = 0u; //!< Each pixel is a grayscale sample (1/2/4/8/16-bits per sample).
static constexpr uint32_t kColorType2_RGB          = 2u; //!< Each pixel is an RGB triple (8/16-bits per sample).
static constexpr uint32_t kColorType3_PAL          = 3u; //!< Each pixel is a palette index (1/2/4/8 bits per sample).
static constexpr uint32_t kColorType4_LUMA         = 4u; //!< Each pixel is a grayscale+alpha sample (8/16-bits per sample).
static constexpr uint32_t kColorType6_RGBA         = 6u; //!< Each pixel is an RGBA quad (8/16 bits per sample).

static constexpr uint32_t kFilterTypeNone          = 0u;
static constexpr uint32_t kFilterTypeSub           = 1u;
static constexpr uint32_t kFilterTypeUp            = 2u;
static constexpr uint32_t kFilterTypeAvg           = 3u;
static constexpr uint32_t kFilterTypePaeth         = 4u;
static constexpr uint32_t kFilterTypeCount         = 5u;

static constexpr uint32_t kFilterTypeAvg0          = 5u; //!< A synthetic filter used reverse-filter implementation.

static constexpr uint32_t kAPNGDisposeOpNone       = 0u; //!< No disposal of the current frame (next frame is drawn over it).
static constexpr uint32_t kAPNGDisposeOpBackground = 1u; //!< The current frame is cleared to a transparent color.
static constexpr uint32_t kAPNGDisposeOpPrevious   = 2u; //!< The current frame is cleared to the previous frame's content.
static constexpr uint32_t kAPNGDisposeOpMaxValue   = 2u; //!< The maximum value of disposal-op.

static constexpr uint32_t kAPNGBlendOpSource       = 0u; //!< The current frame is copied to the target pixel data.
static constexpr uint32_t kAPNGBlendOpOver         = 1u; //!< The current frame is composited by SRC_OVER to the target pixel data.
static constexpr uint32_t kAPNGBlendOpMaxValue     = 1u; //!< The maximum value of blend-op.

enum class DecoderStatusFlags : uint32_t {
  //! No flags.
  kNone = 0x00000000u,

  //! PNG Header.
  kRead_IHDR = 0x00000001u,
  //! 'CgBI' chunk was already processed (if present, this is a CgBI image).
  kRead_CgBI = 0x00000002u,
  //! 'acTL' chunk was already processed (if present, this is an APNG image).
  kRead_acTL = 0x00000004u,
  //! 'PLTE' chunk was already processed (once per PNG image).
  kRead_PLTE = 0x00000010u,
  //! 'tRNS' chunk was already processed (once per PNG image).
  kRead_tRNS = 0x00000020u,
  //! 'fcTL' chunk was already processed (once per APNG frame).
  kRead_fcTL = 0x00000040u,

  //! Whether the PNG uses a color key.
  kHasColorKey = 0x00000100u,
  //! Whether the first frame is the default image as well (APNG).
  kFirstFrameIsDefaultImage = 0x00000200u
};

BL_DEFINE_ENUM_FLAGS(DecoderStatusFlags)

BL_HIDDEN void pngCodecOnInit(BLRuntimeContext* rt, BLArray<BLImageCodec>* codecs) noexcept;

//! Frame control chunk data.
struct FCTL {
  uint32_t sequenceNumber;
  uint32_t w;
  uint32_t h;
  uint32_t x;
  uint32_t y;
  uint16_t delayNum;
  uint16_t delayDen;
  uint8_t disposeOp;
  uint8_t blendOp;
  uint8_t padding[6];
};

} // {bl::Png}

struct BLPngDecoderImpl : public BLImageDecoderImpl {
  //! \name Members
  //! \{

  //! Decoder image information.
  BLImageInfo imageInfo;
  //! Decoder status flags.
  bl::Png::DecoderStatusFlags statusFlags;
  //! Color type.
  uint8_t colorType;
  //! Depth (depth per one sample).
  uint8_t sampleDepth;
  //! Number of samples (1, 2, 3, 4).
  uint8_t sampleCount;
  //! Pixel format of BLImage.
  uint8_t outputFormat;

  //! Color key.
  BLRgba64 colorKey;
  //! Palette entries.
  BLRgba32 paletteData[256];
  //! Palette size.
  uint32_t paletteSize;
  //! The current frame control chunk.
  bl::Png::FCTL prevCtrl;
  //! The current frame control chunk.
  bl::Png::FCTL frameCtrl;
  //! First 'fcTL' chunk offset in the PNG data.
  size_t firstFCTLOffset;

  //! Decoded PNG pixel data (reused in case this is APNG where each frame needs a new decode).
  BLArray<uint8_t> pngPixelData;
  //! Pixel converter used to convert PNG pixel data into a BLImage compatible format.
  BLPixelConverter pixelConverter;
  //! Deflate decoder.
  bl::Compression::Deflate::Decoder deflateDecoder;
  //! Buffer used for storing previous frame content for APNG_DISPOSE_OP_PREVIOUS case.
  bl::ScopedBuffer previousPixelBuffer;

  //! \}

  //! \name Construction & Destruction
  //! \{

  //! Explicit constructor that constructs this Impl.
  BL_INLINE void ctor(const BLImageDecoderVirt* virt_, const BLImageCodecCore* codec_) noexcept {
    BLImageDecoderImpl::ctor(virt_, codec_);
    blCallCtor(pngPixelData);
    blCallCtor(pixelConverter);
  }

  //! Explicit destructor that destructs this Impl.
  BL_INLINE void dtor() noexcept {
    blCallDtor(pixelConverter);
    blCallDtor(pngPixelData);
    BLImageDecoderImpl::dtor();
  }

  //! \}

  //! \name Accessors
  //! \{

  //! Tests whether the PNG decoder has the given `flag` set.
  BL_INLINE_NODEBUG bool hasFlag(bl::Png::DecoderStatusFlags flag) const noexcept { return blTestFlag(statusFlags, flag); }

  BL_INLINE_NODEBUG void addFlag(bl::Png::DecoderStatusFlags flag) noexcept { statusFlags |= flag; }
  BL_INLINE_NODEBUG void clearFlag(bl::Png::DecoderStatusFlags flag) noexcept { statusFlags &= ~flag; }

  //! Tests whether the image is 'APNG' (animated PNG).
  BL_INLINE_NODEBUG bool isAPNG() const noexcept { return hasFlag(bl::Png::DecoderStatusFlags::kRead_acTL); }
  //! Tests whether the image is 'CgBI' and not PNG - 'CgBI' chunk before 'IHDR' and other violations.
  BL_INLINE_NODEBUG bool isCGBI() const noexcept { return hasFlag(bl::Png::DecoderStatusFlags::kRead_CgBI); }

  //! Tests whether the image uses a color key.
  BL_INLINE_NODEBUG bool hasColorKey() const noexcept { return hasFlag(bl::Png::DecoderStatusFlags::kHasColorKey); }
  //! Tests whether the 'fcTL' chunk was already processed for the next frame.
  BL_INLINE_NODEBUG bool hasFCTL() const noexcept { return hasFlag(bl::Png::DecoderStatusFlags::kRead_fcTL); }

  // By default PNG uses a ZLIB header, however, when CgBI non-conforming image is decoded, it's a RAW DEFLATE stream.
  BL_INLINE_NODEBUG bl::Compression::Deflate::FormatType deflateFormat() const noexcept {
    return isCGBI() ? bl::Compression::Deflate::FormatType::kRaw
                    : bl::Compression::Deflate::FormatType::kZlib;
  }

  //! \}
};

struct BLPngEncoderImpl : public BLImageEncoderImpl {
  uint8_t compressionLevel;
};

struct BLPngCodecImpl : public BLImageCodecImpl {};

//! \}
//! \endcond

#endif // BLEND2D_CODEC_PNGCODEC_P_H_INCLUDED
