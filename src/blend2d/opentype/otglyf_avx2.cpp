// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"

#ifdef BL_BUILD_OPT_AVX2
  #define getGlyphOutlines_SIMD getGlyphOutlines_AVX2
  #include "../opentype/otglyf_sse4_2.cpp"
#endif
