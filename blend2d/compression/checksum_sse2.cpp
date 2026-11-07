// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#if defined(BL_TARGET_OPT_SSE2)

#include <blend2d/compression/checksum_p.h>
#include <blend2d/compression/checksum_adler32_simd_impl_p.h>

namespace bl::Compression::Checksum {

// Compression - CheckSum - ADLER32 (SSE2)
// =======================================

uint32_t adler32_update_sse2(uint32_t checksum, const uint8_t* data, size_t size) noexcept {
  return adler32_update_simd(checksum, data, size);
}

} // {bl::Compression::Checksum}

#endif // BL_TARGET_OPT_SSE2
