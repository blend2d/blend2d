// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_test_p.h"
#if defined(BL_TEST)

#include "array_p.h"
#include "image_p.h"
#include "imagecodec.h"

// bl::ImageCodec - Tests
// ======================

namespace bl {
namespace Tests {

UNIT(image_codec_find, BL_TEST_GROUP_IMAGE_CODECS) {
  INFO("Testing BLImageCodec::findByName() and BLImageCodec::findByData()");
  {
    static const uint8_t bmpSignature[2] = { 'B', 'M' };
    static const uint8_t pngSignature[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    static const uint8_t jpgSignature[3] = { 0xFF, 0xD8, 0xFF };

    BLImageCodec codec;
    BLImageCodec bmp;
    BLImageCodec png;
    BLImageCodec jpg;

    EXPECT_SUCCESS(bmp.findByName("BMP"));
    EXPECT_SUCCESS(png.findByName("PNG"));
    EXPECT_SUCCESS(jpg.findByName("JPEG"));

    EXPECT_SUCCESS(codec.findByExtension("bmp"));
    EXPECT_EQ(codec, bmp);

    EXPECT_SUCCESS(codec.findByExtension(".bmp"));
    EXPECT_EQ(codec, bmp);

    EXPECT_SUCCESS(codec.findByExtension("SomeFile.BMp"));
    EXPECT_EQ(codec, bmp);

    EXPECT_SUCCESS(codec.findByExtension("png"));
    EXPECT_EQ(codec, png);

    EXPECT_SUCCESS(codec.findByExtension(".png"));
    EXPECT_EQ(codec, png);

    EXPECT_SUCCESS(codec.findByExtension(".jpg"));
    EXPECT_EQ(codec, jpg);

    EXPECT_SUCCESS(codec.findByExtension(".jpeg"));
    EXPECT_EQ(codec, jpg);

    EXPECT_SUCCESS(codec.findByData(bmpSignature, BL_ARRAY_SIZE(bmpSignature)));
    EXPECT_EQ(codec, bmp);

    EXPECT_SUCCESS(codec.findByData(pngSignature, BL_ARRAY_SIZE(pngSignature)));
    EXPECT_EQ(codec, png);

    EXPECT_SUCCESS(codec.findByData(jpgSignature, BL_ARRAY_SIZE(jpgSignature)));
    EXPECT_EQ(codec, jpg);
  }
}

} // {Tests}
} // {bl}

#endif // BL_TEST
