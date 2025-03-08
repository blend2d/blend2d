// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// The DEFLATE algorithm is based on stb_image <https://github.com/nothings/stb>
// released into PUBLIC DOMAIN. It was initially part of Blend2D's PNG decoder,
// but later split as it could be useful outside of PNG implementation as well.

#include "../api-build_p.h"
#if BL_TARGET_ARCH_BITS >= 64 && defined(BL_TARGET_OPT_AVX2)

#include "../compression/deflatedecoderfastimpl_p.h"

namespace bl {
namespace Compression {
namespace Deflate {
namespace Fast {

DecoderFastResult decode_AVX2(
  Decoder* ctx,
  uint8_t* dstStart,
  uint8_t* dstPtr,
  uint8_t* dstEnd,
  const uint8_t* srcPtr,
  const uint8_t* srcEnd
) noexcept {
  return decodeImpl(ctx, dstStart, dstPtr, dstEnd, srcPtr, srcEnd);
}

} // {Fast}
} // {Deflate}
} // {Compression}
} // {bl}

#endif // BL_TARGET_ARCH_BITS >= 64 && BL_BUILD_OPT_AVX2
