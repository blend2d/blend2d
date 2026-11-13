// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if !defined(BL_BUILD_NO_CODECS) && defined(BL_TEST)

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_ops.h>
#include <blend2d/codec/png_codec_p.h>
#include <blend2d/compression/checksum_p.h>

namespace bl::Tests {

TEST_CASE(codec_png_minimum_image) {
  // Minimum PNG file 33 bytes long, with valid IHDR.
  constexpr size_t kDataSize = 33;

  uint8_t png_data[kDataSize] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, // PNG signature.
    0x00, 0x00, 0x00, 0x0D,                         // IHDR data size.
    'I' , 'H' , 'D' , 'R' ,                         // IHDR tag.
    0x00, 0x00, 0x00, 0x10,                         // Width.
    0x00, 0x00, 0x00, 0x20,                         // Height.
    0x08, 0x06, 0x00, 0x00,                         // Depth, ColorType, Compression, Filter.
    0x00,                                           // Interlace.
    0x00, 0x00, 0x00, 0x00                          // CRC32.
  };
  axl::storeu_u32_be(png_data + 29, Compression::Checksum::crc32(png_data + 12, 17));

  BLImageCodec codec;
  EXPECT_OK(codec.find_by_name("PNG"));

  BLImageDecoder decoder;
  EXPECT_OK(codec.create_decoder(&decoder));

  BLImageInfo image_info;
  EXPECT_OK(decoder.read_info(image_info, png_data, kDataSize));
  EXPECT_EQ(image_info.size.w, 16);
  EXPECT_EQ(image_info.size.h, 32);
  EXPECT_EQ(image_info.depth, 32u);
  EXPECT_EQ(image_info.plane_count, 0u);
  EXPECT_EQ(image_info.frame_count, 1u);
  EXPECT_EQ(strcmp(image_info.format, "PNG"), 0);
  EXPECT_EQ(strcmp(image_info.compression, "DEFLATE"), 0);

  BLImage image;
  EXPECT_NE(decoder.read_frame(image, png_data, kDataSize), BL_SUCCESS);
}

} // {bl::Tests}

#endif
