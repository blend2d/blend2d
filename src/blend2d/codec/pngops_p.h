// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CODEC_PNGOPS_P_H_INCLUDED
#define BLEND2D_CODEC_PNGOPS_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../codec/pngcodec_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_codec_impl
//! \{

namespace bl {
namespace Png {

// bl::Png::Opts - Utilities
// =========================

// Unsigned division by 3 translated into a multiplication and shift. The range of `x` is [0, 255], inclusive.
// This means that we need at most 16 bits to have the result. The SIMD implementation exploits this approach
// by using PMULHUW instruction that will multiply and shift by 16 bits right (the constant is adjusted for that).
static BL_INLINE uint32_t udiv3(uint32_t x) noexcept { return (x * 0xABu) >> 9; }

//! Returns a simplified filter of the first PNG row, because no prior row exists at that point.
//! This is the only function that can replace AVG filter with `AVG0`.
static BL_INLINE uint32_t simplifyFilterOfFirstRow(uint32_t filter) noexcept {
  uint32_t kReplacement = (BL_PNG_FILTER_TYPE_NONE <<  0) | // None  -> None
                          (BL_PNG_FILTER_TYPE_SUB  <<  4) | // Sub   -> Sub
                          (BL_PNG_FILTER_TYPE_NONE <<  8) | // Up    -> None
                          (BL_PNG_FILTER_TYPE_AVG0 << 12) | // Avg   -> Avg0 (Special-Case)
                          (BL_PNG_FILTER_TYPE_SUB  << 16) ; // Paeth -> Sub
  return (kReplacement >> (filter * 4)) & 0xF;
}

// Performs PNG sum filter and casts to byte.
static BL_INLINE uint8_t applySumFilter(uint32_t a, uint32_t b) noexcept { return uint8_t((a + b) & 0xFF); }

// Performs PNG average filter.
static BL_INLINE uint32_t applyAvgFilter(uint32_t a, uint32_t b) noexcept { return (a + b) >> 1; }

// This is an optimized implementation of PNG's Paeth reference filter. This
// optimization originally comes from my previous implementation where I tried
// to simplify it to be more SIMD friendly. One interesting property of Paeth
// filter is:
//
//   Paeth(a, b, c) == Paeth(b, a, c);
//
// Actually what the filter needs is a minimum and maximum of `a` and `b`, so
// I based the implementation on getting those first. If you know `min(a, b)`
// and `max(a, b)` you can divide the interval to be checked against `c`. This
// requires division by 3, which is available above as `udiv3()`.
//
// The previous implementation looked like:
//
// ```
// uint32_t Paeth(uint32_t a, uint32_t b, uint32_t c) noexcept {
//   uint32_t minAB = min(a, b);
//   uint32_t maxAB = max(a, b);
//   uint32_t divAB = udiv3(maxAB - minAB);
//
//   if (c <= minAB + divAB) return maxAB;
//   if (c >= maxAB - divAB) return minAB;
//
//   return c;
// }
// ```
//
// Although it's not bad I tried to exploit more the idea of SIMD and masking.
// The following code basically removes the need of any comparison, it relies
// on bit shifting and performs an arithmetic (not logical) shift of signs
// produced by `divAB + minAB` and `divAB - maxAB`, which are then used to mask
// out `minAB` and `maxAB`. The `minAB` and `maxAB` can be negative after `c`
// is subtracted, which will basically remove the original `c` if one of the
// two additions is unmasked. The code can unmask either zero or one addition,
// but it never unmasks both.
static BL_INLINE uint32_t blPngPaethFilter(uint32_t a, uint32_t b, uint32_t c) noexcept {
  uint32_t minAB = blMin(a, b) - c;
  uint32_t maxAB = blMax(a, b) - c;
  uint32_t divAB = udiv3(maxAB - minAB);

  return c + (maxAB & ~uint32_t(int32_t(divAB + minAB) >> 31)) +
             (minAB & ~uint32_t(int32_t(divAB - maxAB) >> 31)) ;
}

// bl::Png::Opts - Dispatch
// ========================

//! Optimized PNG functions.
struct FuncOpts {
  BLResult (BL_CDECL* inverseFilter)(uint8_t* p, uint32_t bpp, uint32_t bpl, uint32_t h) BL_NOEXCEPT;
};
extern FuncOpts opts;

// bl::Png::Opts - Baseline
// ========================

BL_HIDDEN BLResult BL_CDECL inverseFilterImpl(uint8_t* p, uint32_t bpp, uint32_t bpl, uint32_t h) noexcept;

// bl::Png::Opts - SSE2
// ====================

#ifdef BL_BUILD_OPT_SSE2
BL_HIDDEN BLResult BL_CDECL inverseFilterImpl_SSE2(uint8_t* p, uint32_t bpp, uint32_t bpl, uint32_t h) noexcept;
#endif

} // {Png}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CODEC_PNGOPS_P_H_INCLUDED
