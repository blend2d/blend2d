// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if !defined(BL_BUILD_NO_CODECS) && defined(BL_TEST)

#include <blend2d/axl/mem_ops.h>
#include <blend2d/core/array.h>
#include <blend2d/core/context.h>
#include <blend2d/core/image.h>
#include <blend2d/core/image_codec.h>
#include <blend2d/core/image_decoder.h>
#include <blend2d/core/image_encoder.h>
#include <blend2d/core/random.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/core/string.h>
#include <blend2d/codec/qoi_ops_p.h>

#include <blend2d-testing/commons/image_diff.h>

// Codecs - Tests
// ==============

namespace bl::Codecs::Tests {

static void render_simple_image(BLImage& image, BLRandom& rnd, uint32_t cmd_count) noexcept {
  BLContext ctx(image);
  ctx.clear_all();

  double w = image.width();
  double h = image.height();
  double s = bl_min(w, h);

  for (uint32_t i = 0; i < cmd_count; i++) {
    uint32_t shape = rnd.next_uint32() & 0x3u;
    BLRgba32 color = BLRgba32(rnd.next_uint32() | 0xFF000000u);

    ctx.set_fill_style(color);

    switch (shape) {
      case 0: {
        double x0 = rnd.next_double() * w;
        double y0 = rnd.next_double() * h;
        double x1 = rnd.next_double() * w;
        double y1 = rnd.next_double() * h;

        double rx = bl_min(x0, x1);
        double ry = bl_min(y0, y1);
        double rw = bl_max(x0, x1) - rx;
        double rh = bl_max(y0, y1) - ry;

        ctx.fill_rect(rx, ry, rw, rh);
        break;
      }

      case 1: {
        double x0 = rnd.next_double() * w;
        double y0 = rnd.next_double() * h;
        double x1 = rnd.next_double() * w;
        double y1 = rnd.next_double() * h;
        double x2 = rnd.next_double() * w;
        double y2 = rnd.next_double() * h;

        ctx.fill_triangle(x0, y0, x1, y1, x2, y2);
        break;
      }

      case 2: {
        double cx = rnd.next_double() * w;
        double cy = rnd.next_double() * h;
        double r = rnd.next_double() * s;

        ctx.fill_circle(cx, cy, r);
        break;
      }

      case 3: {
        double cx = rnd.next_double() * w;
        double cy = rnd.next_double() * h;
        double r = rnd.next_double() * s;
        double start = rnd.next_double() * 3;
        double sweep = rnd.next_double() * 6;

        ctx.fill_pie(cx, cy, r, start, sweep);
        break;
      }
    }
  }
}

struct TestOptions {
  uint32_t compression_level = 0xFFFFFFFFu;

  BL_INLINE bool has_compression_level() const noexcept { return compression_level != 0xFFFFFFFFu; }
};

static void log_roundtrip_failure_data(const BLImage& image, const BLArray<uint8_t>& encoded_data) noexcept {
  // Keep small failing cases reproducible without depending on filesystem access in CI.
  constexpr size_t kMaxDumpSize = 4096u;
  if (encoded_data.size() <= kMaxDumpSize) {
    BLString hex;
    for (uint8_t byte : encoded_data) {
      hex.append_format("%02X", uint32_t(byte));
    }
    TEST_LOG("EncodedData=%s", hex.data());
  }

  BLImageData data;
  if (image.get_data(&data) == BL_SUCCESS) {
    size_t row_size = size_t(data.size.w) * (data.format == BL_FORMAT_A8 ? 1u : 4u);
    if (row_size && size_t(data.size.h) <= kMaxDumpSize / row_size) {
      BLString hex;
      const uint8_t* row = static_cast<const uint8_t*>(data.pixel_data);
      for (int y = 0; y < data.size.h; y++, row += data.stride) {
        for (size_t x = 0; x < row_size; x++) {
          hex.append_format("%02X", uint32_t(row[x]));
        }
      }
      TEST_LOG("SourcePixels=%s", hex.data());
    }
  }
}

static void test_encoding_decoding_random_images(BLSizeI size, BLFormat fmt, BLImageCodec& codec, BLRandom& rnd, uint32_t test_count, uint32_t cmd_count, const TestOptions& test_options) noexcept {
  for (uint32_t i = 0; i < test_count; i++) {
    BLImage image1;

    EXPECT_OK(image1.create(size.w, size.h, fmt));
    render_simple_image(image1, rnd, cmd_count);

    BLImageEncoder encoder;
    EXPECT_OK(codec.create_encoder(&encoder));

    if (test_options.has_compression_level()) {
      EXPECT_OK(encoder.set_property("compression", BLVar(test_options.compression_level)));
    }

    BLArray<uint8_t> encoded_data;
    EXPECT_OK(encoder.write_frame(encoded_data, image1)).message(
      "Codec=%s Size=%dx%d Format=%u Compression=%u Iteration=%u",
      codec.name().data(), size.w, size.h, uint32_t(fmt), test_options.compression_level, i);

    BLImageDecoder decoder;
    EXPECT_OK(codec.create_decoder(&decoder));

    BLImage image2;
    EXPECT_OK(decoder.read_frame(image2, encoded_data)).message(
      "Codec=%s Size=%dx%d Format=%u Compression=%u Iteration=%u EncodedSize=%zu",
      codec.name().data(), size.w, size.h, uint32_t(fmt), test_options.compression_level, i, encoded_data.size());

    ImageUtils::DiffInfo diff_info = ImageUtils::diff_info(image1, image2);
    if (diff_info.max_diff != 0u) {
      log_roundtrip_failure_data(image1, encoded_data);
    }
    EXPECT_EQ(diff_info.max_diff, 0u).message(
      "Codec=%s Size=%dx%d Format=%u Compression=%u Iteration=%u EncodedSize=%zu",
      codec.name().data(), size.w, size.h, uint32_t(fmt), test_options.compression_level, i, encoded_data.size());
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
  { 301, 299 }
};

TEST_CASE(codec_bmp_roundtrip) {
  static constexpr uint32_t kCmdCount = 10;
  static constexpr uint32_t kTestCount = 64;

  BLImageCodec codec;
  EXPECT_OK(codec.find_by_name("BMP"));

  for (BLSizeI image_size : image_codec_test_sizes) {
    BLString param;
    param.append_format("%dx%d", image_size.w, image_size.h);

    if (test_runner.test_variation(param.data(), param.size())) {
      BLRandom rnd(0x123456789ABCDEFu ^ (uint64_t(image_size.w) << 13) ^ (uint64_t(image_size.h) << 29));
      TestOptions test_options{};

      test_encoding_decoding_random_images(image_size, BL_FORMAT_XRGB32, codec, rnd, kTestCount, kCmdCount, test_options);
      test_encoding_decoding_random_images(image_size, BL_FORMAT_PRGB32, codec, rnd, kTestCount, kCmdCount, test_options);
    }
  }
}

TEST_CASE(codec_png_roundtrip) {
  static constexpr uint32_t kCmdCount = 10;
  static constexpr uint32_t kTestCount = 32;

  BLImageCodec codec;
  EXPECT_OK(codec.find_by_name("PNG"));

  for (BLSizeI image_size : image_codec_test_sizes) {
    BLString param;
    param.append_format("%dx%d", image_size.w, image_size.h);

    if (test_runner.test_variation(param.data(), param.size())) {
      for (uint32_t compression_level = 0; compression_level <= 12; compression_level++) {
        BLRandom rnd(0x123456789ABCDEFu ^ (uint64_t(image_size.w) << 13) ^ (uint64_t(image_size.h) << 29));
        TestOptions test_options{.compression_level = compression_level};

        test_encoding_decoding_random_images(image_size, BL_FORMAT_XRGB32, codec, rnd, kTestCount, kCmdCount, test_options);
        test_encoding_decoding_random_images(image_size, BL_FORMAT_PRGB32, codec, rnd, kTestCount, kCmdCount, test_options);
      }
    }
  }
}

TEST_CASE(codec_qoi_roundtrip) {
  static constexpr uint32_t kCmdCount = 10;
  static constexpr uint32_t kTestCount = 64;

  BLImageCodec codec;
  EXPECT_OK(codec.find_by_name("QOI"));

  for (BLSizeI image_size : image_codec_test_sizes) {
    BLString param;
    param.append_format("%dx%d", image_size.w, image_size.h);

    if (test_runner.test_variation(param.data(), param.size())) {
      BLRandom rnd(0x123456789ABCDEFu ^ (uint64_t(image_size.w) << 13) ^ (uint64_t(image_size.h) << 29));
      TestOptions test_options{};

      test_encoding_decoding_random_images(image_size, BL_FORMAT_XRGB32, codec, rnd, kTestCount, kCmdCount, test_options);
      test_encoding_decoding_random_images(image_size, BL_FORMAT_PRGB32, codec, rnd, kTestCount, kCmdCount, test_options);
    }
  }
}

} // {bl::Codecs::Tests}

#endif // !BL_BUILD_NO_CODECS && BL_TEST
