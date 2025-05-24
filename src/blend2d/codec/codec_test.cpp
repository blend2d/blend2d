// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_test_p.h"
#if defined(BL_TEST)

#include "../array.h"
#include "../context.h"
#include "../image.h"
#include "../imagecodec.h"
#include "../imagedecoder.h"
#include "../imageencoder.h"
#include "../random.h"

#include "../../../test/bl_test_imageutils.h"

// bl::Codecs - Tests
// ==================

namespace bl::Codecs::Tests {

static void render_simple_image(BLImage& image, BLRandom& rnd, uint32_t cmd_count) noexcept {
  BLContext ctx(image);
  ctx.clearAll();

  double w = image.width();
  double h = image.height();
  double s = blMin(w, h);

  for (uint32_t i = 0; i < cmd_count; i++) {
    uint32_t shape = rnd.nextUInt32() & 0x3u;
    BLRgba32 color = BLRgba32(rnd.nextUInt32() | 0xFF000000u);

    ctx.setFillStyle(color);

    switch (shape) {
      case 0: {
        double x0 = rnd.nextDouble() * w;
        double y0 = rnd.nextDouble() * h;
        double x1 = rnd.nextDouble() * w;
        double y1 = rnd.nextDouble() * h;

        double rx = blMin(x0, x1);
        double ry = blMin(y0, y1);
        double rw = blMax(x0, x1) - rx;
        double rh = blMax(y0, y1) - ry;

        ctx.fillRect(rx, ry, rw, rh);
        break;
      }

      case 1: {
        double x0 = rnd.nextDouble() * w;
        double y0 = rnd.nextDouble() * h;
        double x1 = rnd.nextDouble() * w;
        double y1 = rnd.nextDouble() * h;
        double x2 = rnd.nextDouble() * w;
        double y2 = rnd.nextDouble() * h;

        ctx.fillTriangle(x0, y0, x1, y1, x2, y2);
        break;
      }

      case 2: {
        double cx = rnd.nextDouble() * w;
        double cy = rnd.nextDouble() * h;
        double r = rnd.nextDouble() * s;

        ctx.fillCircle(cx, cy, r);
        break;
      }

      case 3: {
        double cx = rnd.nextDouble() * w;
        double cy = rnd.nextDouble() * h;
        double r = rnd.nextDouble() * s;
        double start = rnd.nextDouble() * 3;
        double sweep = rnd.nextDouble() * 6;

        ctx.fillPie(cx, cy, r, start, sweep);
        break;
      }
    }
  }
}

struct TestOptions {
  uint32_t compressionLevel = 0xFFFFFFFFu;

  BL_INLINE bool hasCompressionLevel() const noexcept { return compressionLevel != 0xFFFFFFFFu; }
};

static void test_encoding_decoding_random_images(BLSizeI size, BLFormat fmt, BLImageCodec& codec, BLRandom& rnd, uint32_t test_count, uint32_t cmd_count, const TestOptions& testOptions) noexcept {
  for (uint32_t i = 0; i < test_count; i++) {
    BLImage image1;

    EXPECT_SUCCESS(image1.create(size.w, size.h, fmt));
    render_simple_image(image1, rnd, cmd_count);

    BLImageEncoder encoder;
    EXPECT_SUCCESS(codec.createEncoder(&encoder));

    if (testOptions.hasCompressionLevel()) {
      EXPECT_SUCCESS(encoder.setProperty("compression", BLVar(testOptions.compressionLevel)));
    }

    BLArray<uint8_t> encodedData;
    encoder.writeFrame(encodedData, image1);

    BLImageDecoder decoder;
    EXPECT_SUCCESS(codec.createDecoder(&decoder));

    BLImage image2;
    EXPECT_SUCCESS(decoder.readFrame(image2, encodedData));

    ImageUtils::DiffInfo diffInfo = ImageUtils::diffInfo(image1, image2);
    EXPECT_EQ(diffInfo.maxDiff, 0u);
  }
}

static constexpr BLSizeI image_codec_test_sizes[] = {
  { 1, 1 },
  { 1, 2 },
  { 2, 2 },
  { 3, 3 },
  { 4, 4 },
  { 5, 4 },
  { 6, 6 },
  { 1, 7 },
  { 7, 1 },
  { 11, 13 },
  { 15, 15 },
  { 16, 15 },
  { 99, 54 },
  { 132, 23 },
  { 301, 301 }
};

UNIT(image_codec_bmp, BL_TEST_GROUP_IMAGE_CODEC_ROUNDTRIP) {
  static constexpr uint32_t kCmdCount = 10;
  static constexpr uint32_t kTestCount = 100;

  for (BLSizeI imageSize : image_codec_test_sizes) {
    BLImageCodec codec;
    EXPECT_SUCCESS(codec.findByName("BMP"));

    BLRandom rnd(0x123456789ABCDEFu);
    TestOptions testOptions{};

    INFO("Testing BMP encoder & decoder with %dx%d images", imageSize.w, imageSize.h);
    test_encoding_decoding_random_images(imageSize, BL_FORMAT_XRGB32, codec, rnd, kTestCount, kCmdCount, testOptions);
    test_encoding_decoding_random_images(imageSize, BL_FORMAT_PRGB32, codec, rnd, kTestCount, kCmdCount, testOptions);
  }
}

UNIT(image_codec_png, BL_TEST_GROUP_IMAGE_CODEC_ROUNDTRIP) {
  static constexpr uint32_t kCmdCount = 10;
  static constexpr uint32_t kTestCount = 100;

  for (BLSizeI imageSize : image_codec_test_sizes) {
    INFO("Testing PNG encoder & decoder with %dx%d images", imageSize.w, imageSize.h);

    BLImageCodec codec;
    EXPECT_SUCCESS(codec.findByName("PNG"));

    BLRandom rnd(0x123456789ABCDEFu);
    for (uint32_t compressionLevel = 0; compressionLevel <= 12; compressionLevel++) {
      TestOptions testOptions{};
      testOptions.compressionLevel = compressionLevel;

      test_encoding_decoding_random_images(imageSize, BL_FORMAT_XRGB32, codec, rnd, kTestCount, kCmdCount, testOptions);
      test_encoding_decoding_random_images(imageSize, BL_FORMAT_PRGB32, codec, rnd, kTestCount, kCmdCount, testOptions);
    }
  }
}

UNIT(image_codec_qoi, BL_TEST_GROUP_IMAGE_CODEC_ROUNDTRIP) {
  static constexpr uint32_t kCmdCount = 10;
  static constexpr uint32_t kTestCount = 100;

  for (BLSizeI imageSize : image_codec_test_sizes) {
    BLImageCodec codec;
    EXPECT_SUCCESS(codec.findByName("QOI"));

    BLRandom rnd(0x123456789ABCDEFu);
    TestOptions testOptions{};

    INFO("Testing QOI encoder & decoder with %dx%d images", imageSize.w, imageSize.h);
    test_encoding_decoding_random_images(imageSize, BL_FORMAT_XRGB32, codec, rnd, kTestCount, kCmdCount, testOptions);
    test_encoding_decoding_random_images(imageSize, BL_FORMAT_PRGB32, codec, rnd, kTestCount, kCmdCount, testOptions);
  }
}

} // {bl::Codecs::Tests}

#endif // BL_TEST
