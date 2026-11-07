// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CODEC_QOI_CODEC_P_H_INCLUDED
#define BLEND2D_CODEC_QOI_CODEC_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/image_p.h>
#include <blend2d/core/image_codec.h>
#include <blend2d/core/image_decoder.h>
#include <blend2d/core/image_encoder.h>
#include <blend2d/core/runtime_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_codec_impl
//! \{

namespace bl::Qoi {

BL_HIDDEN void qoi_codec_on_init(BLRuntimeContext* rt, BLArray<BLImageCodec>* codecs) noexcept;

struct QoiCodecImpl : public BLImageCodecImpl {};
struct QoiDecoderImpl : public BLImageDecoderImpl { BLImageInfo image_info; };
struct QoiEncoderImpl : public BLImageEncoderImpl {};

} // {bl::Qoi}

//! \}
//! \endcond

#endif // BLEND2D_CODEC_QOI_CODEC_P_H_INCLUDED
