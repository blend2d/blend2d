// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// NOTE: In general this implementation would work when SSSE3 is available, however, we really want to use POPCNT,
// which is available from SSE4.2 (Intel) and SSE4a (AMD). So we require SSE4.2 to make sure both SSSE3 and POPCNT
// are present.

#include "../api-build_p.h"
#ifdef BL_BUILD_OPT_SSE4_2

#include "../opentype/otglyfsimdimpl_p.h"

namespace bl {
namespace OpenType {
namespace GlyfImpl {

BLResult BL_CDECL getGlyphOutlines_SSE4_2(
  const BLFontFaceImpl* faceI_,
  BLGlyphId glyphId,
  const BLMatrix2D* transform,
  BLPath* out,
  size_t* contourCountOut,
  ScopedBuffer* tmpBuffer) noexcept {

  return getGlyphOutlinesSimdImpl(faceI_, glyphId, transform, out, contourCountOut, tmpBuffer);
}

} // {GlyfImpl}
} // {OpenType}
} // {bl}

#endif // BL_BUILD_OPT_SSE4_2
