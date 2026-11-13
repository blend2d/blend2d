// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CODEC_QOI_OPS_P_H_INCLUDED
#define BLEND2D_CODEC_QOI_OPS_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/codec/qoi_codec_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_codec_impl
//! \{

namespace bl::Qoi {

// Codecs - Qoi - Constants
// ========================

static constexpr size_t kQoiHeaderSize = 14;
static constexpr size_t kQoiMagicSize = 4;
static constexpr size_t kQoiEndMarkerSize = 8;

static constexpr uint8_t kQoiOpIndex = 0x00; // 00xxxxxx
static constexpr uint8_t kQoiOpDiff  = 0x40; // 01xxxxxx
static constexpr uint8_t kQoiOpLuma  = 0x80; // 10xxxxxx
static constexpr uint8_t kQoiOpRun   = 0xC0; // 11xxxxxx
static constexpr uint8_t kQoiOpRGB   = 0xFE; // 11111110
static constexpr uint8_t kQoiOpRGBA  = 0xFF; // 11111111

static constexpr uint32_t kQoiHashMask = 0x3Fu;
static constexpr uint32_t kQoiHashShiftR = 3u;
static constexpr uint32_t kQoiHashShiftG = 5u;
static constexpr uint32_t kQoiHashShiftB = 7u;
static constexpr uint32_t kQoiHashShiftA = 11u;

static constexpr uint32_t kMaxRunLen = 62;

// Codecs - Qoi - Hashing
// ======================

static BL_INLINE uint32_t qoi_hash_pixel_ag_rb_x64(uint64_t ag_rb) noexcept {
  ag_rb *= (uint64_t(kQoiHashShiftA) << ( 8 + 2)) + (uint64_t(kQoiHashShiftG) << (24 + 2)) +
           (uint64_t(kQoiHashShiftR) << (40 + 2)) + (uint64_t(kQoiHashShiftB) << (56 + 2)) ;
  return uint32_t(ag_rb >> 58);
}

static BL_INLINE uint32_t qoi_hash_pixel_ag_rb_x32(uint32_t ag, uint32_t rb) noexcept {
  ag *= ((kQoiHashShiftA << (0 + 2)) + (kQoiHashShiftG << (16 + 2)));
  rb *= ((kQoiHashShiftR << (8 + 2)) + (kQoiHashShiftB << (24 + 2)));

  return (ag + rb) >> 26;
}

static BL_INLINE uint32_t qoi_hash_pixel_rgba32(uint32_t pixel) noexcept {
#if BL_TARGET_ARCH_BITS >= 64
  return qoi_hash_pixel_ag_rb_x64(((uint64_t(pixel) << 24) | pixel) & 0x00FF00FF00FF00FFu);
#else
  return qoi_hash_pixel_ag_rb_x32(pixel & 0xFF00FF00u, pixel & 0x00FF00FFu);
#endif
}

static BL_INLINE uint32_t qoi_hash_pixel_a8(uint32_t a) noexcept {
  return (0xFFu * (kQoiHashShiftR + kQoiHashShiftG + kQoiHashShiftB) + a * kQoiHashShiftA) & kQoiHashMask;
}

// Codecs - Qoi - Decoder - Ops
// ============================

BLResult qoi_decode_data(uint8_t* dst_row, intptr_t dst_stride, uint32_t w, uint32_t h, const uint8_t* src, const uint8_t* end, bool has_alpha) noexcept;

// Codecs - Qoi - Encoder - Ops
// ============================

uint8_t* qoi_encode_data_a8(uint8_t* BL_RESTRICT dst_data, uint32_t w, uint32_t h, const uint8_t* BL_RESTRICT src_data, intptr_t src_stride) noexcept;
uint8_t* qoi_encode_data_xrgb32(uint8_t* BL_RESTRICT dst_data, uint32_t w, uint32_t h, const uint8_t* BL_RESTRICT src_data, intptr_t src_stride) noexcept;
uint8_t* qoi_encode_data_prgb32(uint8_t* BL_RESTRICT dst_data, uint32_t w, uint32_t h, const uint8_t* BL_RESTRICT src_data, intptr_t src_stride) noexcept;

} // {bl::Qoi}

//! \}
//! \endcond

#endif // BLEND2D_CODEC_QOI_OPS_P_H_INCLUDED
