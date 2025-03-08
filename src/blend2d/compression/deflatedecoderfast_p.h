// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_COMPRESSION_DEFLATEDECODERFAST_P_H_INCLUDED
#define BLEND2D_COMPRESSION_DEFLATEDECODERFAST_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../compression/deflatedecoder_p.h"
#include "../support/intops_p.h"

//! \cond INTERNAL

namespace bl {
namespace Compression {
namespace Deflate {
namespace Fast {

// Must be powers of 2 so we can divide easily from raw buffer lengths to calculate `safeIters`.
// We definitely want extra safety here and to actually be more strict than necessary.
constexpr uint32_t kSrcBytesPerIter = 8u;
constexpr uint32_t kDstBytesPerIter = 8u;

constexpr uint32_t kDstCopyBytesPerIter = 16u;

constexpr uint32_t kSrcBytesPerIterShift = IntOps::ctzStatic(kSrcBytesPerIter);
constexpr uint32_t kDstBytesPerIterShift = IntOps::ctzStatic(kDstBytesPerIter);

// Scratch - extra size that must always be available to perform a single iteration.
constexpr uint32_t kSrcMinScratch = sizeof(BLBitWord) * 2u;
constexpr uint32_t kDstMinScratch = kMaxMatchLen + kDstCopyBytesPerIter * 2u;

// Scratch aligned to iteration count and shifted the same way as dst/src counters.
constexpr uint32_t kSrcMinScratchShifted = (kSrcMinScratch + kSrcBytesPerIter - 1) >> kSrcBytesPerIterShift;
constexpr uint32_t kDstMinScratchShifted = (kDstMinScratch + kDstBytesPerIter - 1) >> kDstBytesPerIterShift;

constexpr uint32_t kMinimumFastIterationCount = 20;

constexpr uint32_t kMinimumFastDstBuffer = kDstMinScratch + kDstBytesPerIter * kMinimumFastIterationCount;
constexpr uint32_t kMinimumFastSrcBuffer = kSrcMinScratch + kSrcBytesPerIter * kMinimumFastIterationCount;

#if BL_TARGET_ARCH_BITS >= 64

DecoderFastResult BL_CDECL decode(
  Decoder* ctx,
  uint8_t* dstStart,
  uint8_t* dstPtr,
  uint8_t* dstEnd,
  const uint8_t* srcPtr,
  const uint8_t* srcEnd
) noexcept;

#if defined(BL_BUILD_OPT_AVX2)
DecoderFastResult BL_CDECL decode_AVX2(
  Decoder* ctx,
  uint8_t* dstStart,
  uint8_t* dstPtr,
  uint8_t* dstEnd,
  const uint8_t* srcPtr,
  const uint8_t* srcEnd
) noexcept;
#endif // BL_BUILD_OPT_AVX2

#endif // BL_TARGET_ARCH_BITS == 64

} // {Fast}
} // {Deflate}
} // {Compression}
} // {bl}

//! \endcond

#endif // BLEND2D_COMPRESSION_DEFLATEDECODERFAST_P_H_INCLUDED
