// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#if defined(BL_TARGET_OPT_SSE4_2)

#include "../compression/checksum_p.h"
#include "../compression/checksumcrc32simdimpl_p.h"

namespace bl {
namespace Compression {
namespace Checksum {

// bl::Compression - CheckSum - CRC32 (SSE4.2 + PCLMULQDQ)
// =======================================================

uint32_t crc32Update_SSE4_2(uint32_t checksum, const uint8_t* data, size_t size) noexcept {
  return crc32Update_CLMUL128(checksum, data, size);
}

} // {Checksum}
} // {Compression}
} // {bl}

#endif // BL_TARGET_OPT_SSE4_2
