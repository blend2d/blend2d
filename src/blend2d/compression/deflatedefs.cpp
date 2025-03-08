// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// The DEFLATE algorithm is based on stb_image <https://github.com/nothings/stb>
// released into PUBLIC DOMAIN. It was initially part of Blend2D's PNG decoder,
// but later split as it could be useful outside of PNG implementation as well.

#include "../api-build_p.h"
#include "../compression/deflatedefs_p.h"

namespace bl {
namespace Compression {
namespace Deflate {

const uint8_t kPrecodeLensPermutation[kNumPrecodeSymbols] = {
  16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

} // {Deflate}
} // {Compression}
} // {bl}
