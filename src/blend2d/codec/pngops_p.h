// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_CODEC_PNGOPS_P_H_INCLUDED
#define BLEND2D_CODEC_PNGOPS_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../codec/pngcodec_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_codecs
//! \{

// ============================================================================
// [Global Variables]
// ============================================================================

//! Optimized PNG functions.
struct BLPngOps {
  BLResult (BL_CDECL* inverseFilter)(uint8_t* p, uint32_t bpp, uint32_t bpl, uint32_t h) BL_NOEXCEPT;
};
extern BLPngOps blPngOps;

// ============================================================================
// [BLPngOps - Utilities]
// ============================================================================

//! Returns a replacement filter for the first PNG row, because no prior row
//! exists at that point. This is the only function that can replace AVG
//! filter with `BL_PNG_FILTER_TYPE_AVG0`.
static BL_INLINE uint32_t blPngFirstRowFilterReplacement(uint32_t filter) noexcept {
  uint32_t kReplacement = (BL_PNG_FILTER_TYPE_NONE <<  0) | // None  -> None
                          (BL_PNG_FILTER_TYPE_SUB  <<  4) | // Sub   -> Sub
                          (BL_PNG_FILTER_TYPE_NONE <<  8) | // Up    -> None
                          (BL_PNG_FILTER_TYPE_AVG0 << 12) | // Avg   -> Avg0 (Special-Case)
                          (BL_PNG_FILTER_TYPE_SUB  << 16) ; // Paeth -> Sub
  return (kReplacement >> (filter * 4)) & 0xF;
}

// Performs PNG sum filter and casts to byte.
static BL_INLINE uint8_t blPngSumFilter(uint32_t a, uint32_t b) noexcept { return uint8_t((a + b) & 0xFF); }

// Performs PNG average filter.
static BL_INLINE uint32_t blPngAvgFilter(uint32_t a, uint32_t b) noexcept { return (a + b) >> 1; }

// Unsigned division by 3 translated into a multiplication and shift. The range
// of `x` is [0, 255], inclusive. This means that we need at most 16 bits to
// have the result. In SIMD this is exploited by using PMULHUW instruction that
// will multiply and shift by 16 bits right (the constant is adjusted for that).
static BL_INLINE uint32_t blPngUdiv3(uint32_t x) noexcept { return (x * 0xABu) >> 9; }

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
// requires division by 3, which is available above as `blPngUdiv3()`.
//
// The previous implementation looked like:
//
// ```
// uint32_t Paeth(uint32_t a, uint32_t b, uint32_t c) noexcept {
//   uint32_t minAB = min(a, b);
//   uint32_t maxAB = max(a, b);
//   uint32_t divAB = blPngUdiv3(maxAB - minAB);
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
  uint32_t divAB = blPngUdiv3(maxAB - minAB);

  return c + (maxAB & ~uint32_t(int32_t(divAB + minAB) >> 31)) +
             (minAB & ~uint32_t(int32_t(divAB - maxAB) >> 31)) ;
}

// ============================================================================
// [BLPngOps - Base]
// ============================================================================

BL_HIDDEN BLResult BL_CDECL blPngInverseFilter(uint8_t* p, uint32_t bpp, uint32_t bpl, uint32_t h) noexcept;

// ============================================================================
// [BLPngOps - SSE2]
// ============================================================================

#ifdef BL_BUILD_OPT_SSE2
BL_HIDDEN BLResult BL_CDECL blPngInverseFilter_SSE2(uint8_t* p, uint32_t bpp, uint32_t bpl, uint32_t h) noexcept;
#endif

// ============================================================================
// [BLPngOps - Runtime]
// ============================================================================

BL_HIDDEN void blPngOpsOnInit(BLRuntimeContext* rt) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_CODEC_PNGOPS_P_H_INCLUDED
