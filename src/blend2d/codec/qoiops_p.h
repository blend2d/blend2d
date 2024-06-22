// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CODEC_QOIOPS_P_H_INCLUDED
#define BLEND2D_CODEC_QOIOPS_P_H_INCLUDED

#include "../api-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_codec_impl
//! \{

namespace bl {
namespace Qoi {

static constexpr size_t kQoiHeaderSize = 14;
static constexpr size_t kQoiMagicSize = 4;
static constexpr size_t kQoiEndMarkerSize = 8;

static constexpr uint8_t kQoiOpIndex = 0x00; // 00xxxxxx
static constexpr uint8_t kQoiOpDiff  = 0x40; // 01xxxxxx
static constexpr uint8_t kQoiOpLuma  = 0x80; // 10xxxxxx
static constexpr uint8_t kQoiOpRun   = 0xC0; // 11xxxxxx
static constexpr uint8_t kQoiOpRgb   = 0xFE; // 11111110
static constexpr uint8_t kQoiOpRgba  = 0xFF; // 11111111

static constexpr uint32_t kQoiHashR = 3u;
static constexpr uint32_t kQoiHashG = 5u;
static constexpr uint32_t kQoiHashB = 7u;
static constexpr uint32_t kQoiHashA = 11u;
static constexpr uint32_t kQoiHashMask = 0x3Fu;

static BL_INLINE uint32_t packPixelFromAGxRBx64(uint64_t v) noexcept {
  return uint32_t(v >> 24) | uint32_t(v & 0xFFFFFFFFu);
}

static BL_INLINE uint64_t unpackPixelToAGxRBx64(uint32_t v) noexcept {
  uint32_t ag = v & 0xFF00FF00;
  uint32_t rb = v & 0x00FF00FF;

  return (uint64_t(ag) << 24) | rb;
}

static BL_INLINE uint32_t hashPixelA8(uint8_t a) noexcept {
  return (0xFFu * kQoiHashR + 0xFFu * kQoiHashG + 0xFFu * kQoiHashB + uint32_t(a) * kQoiHashA) & kQoiHashMask;
}

static BL_INLINE uint32_t hashPixelAGxRBx64(uint64_t ag_rb) noexcept {
  ag_rb *= (uint64_t(kQoiHashA) << ( 8 + 2)) + (uint64_t(kQoiHashG) << (24 + 2)) +
           (uint64_t(kQoiHashR) << (40 + 2)) + (uint64_t(kQoiHashB) << (56 + 2)) ;
  return uint32_t(ag_rb >> 58);
}

static BL_INLINE uint32_t hashPixelAGxRBx32(uint32_t ag, uint32_t rb) noexcept {
  ag *= ((kQoiHashA << (0 + 2)) + (kQoiHashG << (16 + 2)));
  rb *= ((kQoiHashR << (8 + 2)) + (kQoiHashB << (24 + 2)));

  return (ag + rb) >> 26;
}

static BL_INLINE uint32_t hashPixelAG_RB(uint32_t ag, uint32_t rb) noexcept {
#if BL_TARGET_ARCH_BITS >= 64
  return hashPixelAGxRBx64((uint64_t(ag) << 24) | rb);
#else
  return hashPixelAGxRBx32(ag, rb);
#endif
}

static BL_INLINE uint32_t hashPixelRGBA32(BLRgba32 c) noexcept {
  uint32_t ag = c.value & 0xFF00FF00;
  uint32_t rb = c.value & 0x00FF00FF;
  return hashPixelAG_RB(ag, rb);
}

static BL_INLINE uint32_t hashPixelRGB32(BLRgba32 c) noexcept {
  return hashPixelRGBA32(BLRgba32(c.value | 0xFF000000u));
}

} // {Qoi}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CODEC_QOIOPS_P_H_INCLUDED
