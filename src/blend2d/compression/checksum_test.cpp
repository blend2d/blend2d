// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_test_p.h"
#if defined(BL_TEST)

#include "../compression/checksum_p.h"

// Compression - CheckSum - Tests
// ==============================

namespace bl {
namespace Tests {

UNIT(compression_crc32, BL_TEST_GROUP_COMPRESSION_UTILITIES) {
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("a"), 1), 0xE8B7BE43u);
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("ab"), 2), 0x9E83486Du);
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("abc"), 3), 0x352441C2u);
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("abcd"), 4), 0xED82CD11u);
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("abcde"), 5), 0x8587D865u);
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("abcdef"), 6), 0x4B8E39EFu);
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("abcdefg"), 7), 0x312A6AA6u);
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("abcdefgh"), 8), 0xAEEF2A50u);
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("abcdefghi"), 9), 0x8DA988AFu);
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("abcdefghij"), 10), 0x3981703Au);
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("abcdefghijk"), 11), 0xCE570F9Fu);
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("abcdefghijkl"), 12), 0xF6781B24u);
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("abcdefghijklm"), 13), 0xDDF46EA2u);
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("abcdefghijklmn"), 14), 0x400D9578u);
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("abcdefghijklmno"), 15), 0x519167DFu);
  EXPECT_EQ(Compression::crc32(reinterpret_cast<const uint8_t*>("abcdefghijklmnop"), 16), 0x943AC093u);
}

} // {Tests}
} // {bl}

#endif // BL_TEST
