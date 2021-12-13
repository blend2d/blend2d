// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_COMPRESSION_CHECKSUM_P_H_INCLUDED
#define BLEND2D_COMPRESSION_CHECKSUM_P_H_INCLUDED

#include "../api-internal_p.h"

//! \cond INTERNAL

namespace BLCompression {

BL_HIDDEN extern const uint32_t crc32_table[];

static BL_INLINE uint32_t crc32_update_byte(uint32_t hash, uint8_t b) noexcept {
  return (hash >> 8) ^ crc32_table[(hash ^ b) & 0xFFu];
}

BL_HIDDEN uint32_t crc32(const uint8_t* data, size_t size) noexcept;
BL_HIDDEN uint32_t adler32(const uint8_t* data, size_t size) noexcept;

} // {BLCompression}

//! \endcond

#endif // BLEND2D_COMPRESSION_CHECKSUM_P_H_INCLUDED
