// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if !defined(BL_BUILD_NO_CODECS) && defined(BL_TEST)

#include <blend2d/core/array_p.h>
#include <blend2d/core/image_p.h>
#include <blend2d/core/image_codec.h>
#include <blend2d/core/image_decoder.h>
#include <blend2d/core/image_encoder.h>

namespace bl::ImageCodecTests {

// ImageCodec - Test Utilities
// ===========================

static const uint8_t bmp_signature[2] = { 'B', 'M' };
static const uint8_t png_signature[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
static const uint8_t jpg_signature[3] = { 0xFF, 0xD8, 0xFF };
static const uint8_t qoi_signature[4] = { 'q', 'o', 'i', 'f' };

static const uint8_t unknown_signature[] = {0xEE, 0xFE, 0xDE};

// ImageCodec - Tests
// ==================

TEST_CASE(core_image_codec_constructors) {
  BLImageCodec c1;
  EXPECT_FALSE(c1.is_valid());

  BLImageCodec c2(c1);
  EXPECT_FALSE(c2.is_valid());

  EXPECT_EQ(c1, c2);
  EXPECT_TRUE(c1.equals(c2));

  EXPECT_OK(c1.find_by_name("BMP"));
  EXPECT_OK(c2.assign(c1));
  EXPECT_TRUE(c1.is_valid());
  EXPECT_TRUE(c2.is_valid());

  BLImageCodec moved(axl::move(c1));
  EXPECT_FALSE(c1.equals(c2));
  EXPECT_TRUE(moved.equals(c2));

  BLImageDecoder decoder;
  BLImageEncoder encoder;
  EXPECT_EQ(c1.inspect_data(unknown_signature, BL_ARRAY_SIZE(unknown_signature)), 0u);
  EXPECT_EQ(c1.create_decoder(&decoder), BL_ERROR_IMAGE_DECODER_NOT_PROVIDED);
  EXPECT_EQ(c1.create_encoder(&encoder), BL_ERROR_IMAGE_ENCODER_NOT_PROVIDED);

  EXPECT_OK(c1.assign(axl::move(c2)));
  EXPECT_FALSE(c2.is_valid());
  EXPECT_EQ(c1, moved);
}

TEST_CASE(core_image_codec_find_inspect) {
  BLImageCodec codec;
  BLImageCodec bmp;
  BLImageCodec jpg;
  BLImageCodec png;
  BLImageCodec qoi;

  EXPECT_OK(bmp.find_by_name("BMP"));
  EXPECT_OK(jpg.find_by_name("JPEG"));
  EXPECT_OK(png.find_by_name("PNG"));
  EXPECT_OK(qoi.find_by_name("QOI"));

  BLArray<BLImageCodec> codec_list;
  EXPECT_EQ(codec.find_by_name("BMP", codec_list), BL_ERROR_IMAGE_NO_MATCHING_CODEC);
  EXPECT_EQ(codec.find_by_name("PNG", codec_list), BL_ERROR_IMAGE_NO_MATCHING_CODEC);
  EXPECT_EQ(codec.find_by_data(bmp_signature, BL_ARRAY_SIZE(bmp_signature), codec_list), BL_ERROR_IMAGE_NO_MATCHING_CODEC);
  EXPECT_EQ(codec.find_by_data(png_signature, BL_ARRAY_SIZE(png_signature), codec_list), BL_ERROR_IMAGE_NO_MATCHING_CODEC);
  EXPECT_EQ(codec.find_by_extension("bmp", codec_list), BL_ERROR_IMAGE_NO_MATCHING_CODEC);
  EXPECT_EQ(codec.find_by_extension("png", codec_list), BL_ERROR_IMAGE_NO_MATCHING_CODEC);

  EXPECT_OK(codec_list.append(bmp));
  EXPECT_OK(codec.find_by_name("BMP", codec_list));
  EXPECT_EQ(codec.find_by_name("PNG", codec_list), BL_ERROR_IMAGE_NO_MATCHING_CODEC);
  EXPECT_OK(codec.find_by_data(bmp_signature, BL_ARRAY_SIZE(bmp_signature), codec_list));
  EXPECT_EQ(codec.find_by_data(png_signature, BL_ARRAY_SIZE(png_signature), codec_list), BL_ERROR_IMAGE_NO_MATCHING_CODEC);
  EXPECT_OK(codec.find_by_extension("bmp", codec_list));
  EXPECT_EQ(codec.find_by_extension("png", codec_list), BL_ERROR_IMAGE_NO_MATCHING_CODEC);

  EXPECT_OK(codec_list.append(png));
  EXPECT_OK(codec.find_by_name("BMP", codec_list));
  EXPECT_OK(codec.find_by_name("PNG", codec_list));
  EXPECT_OK(codec.find_by_data(bmp_signature, BL_ARRAY_SIZE(bmp_signature), codec_list));
  EXPECT_OK(codec.find_by_data(png_signature, BL_ARRAY_SIZE(png_signature), codec_list));
  EXPECT_OK(codec.find_by_extension("bmp", codec_list));
  EXPECT_OK(codec.find_by_extension("png", codec_list));
  EXPECT_OK(codec.find_by_extension(".bmp", codec_list));
  EXPECT_OK(codec.find_by_extension(".png", codec_list));

  EXPECT_EQ(codec.find_by_name(BLStringView{"BMP", 0}, codec_list), BL_ERROR_IMAGE_NO_MATCHING_CODEC);
  EXPECT_EQ(codec.find_by_name(BLStringView{"BMP", 1}, codec_list), BL_ERROR_IMAGE_NO_MATCHING_CODEC);
  EXPECT_EQ(codec.find_by_data(nullptr, 0u, codec_list), BL_ERROR_IMAGE_NO_MATCHING_CODEC);
  EXPECT_EQ(codec.find_by_data("AAAAA", 0u, codec_list), BL_ERROR_IMAGE_NO_MATCHING_CODEC);
  EXPECT_EQ(codec.find_by_extension(nullptr, codec_list), BL_ERROR_IMAGE_NO_MATCHING_CODEC);
  EXPECT_EQ(codec.find_by_extension("AAAAA", codec_list), BL_ERROR_IMAGE_NO_MATCHING_CODEC);

  EXPECT_TRUE(bmp.is_valid());
  EXPECT_TRUE(jpg.is_valid());
  EXPECT_TRUE(png.is_valid());
  EXPECT_TRUE(qoi.is_valid());

  EXPECT_TRUE(bmp.name().equals("BMP"));
  EXPECT_TRUE(jpg.name().equals("JPEG"));
  EXPECT_TRUE(png.name().equals("PNG"));
  EXPECT_TRUE(qoi.name().equals("QOI"));

  EXPECT_TRUE(bmp.vendor().equals("Blend2D"));
  EXPECT_TRUE(jpg.vendor().equals("Blend2D"));
  EXPECT_TRUE(png.vendor().equals("Blend2D"));
  EXPECT_TRUE(qoi.vendor().equals("Blend2D"));

  EXPECT_TRUE(bmp.mime_type().equals("image/x-bmp"));
  EXPECT_TRUE(jpg.mime_type().equals("image/jpeg"));
  EXPECT_TRUE(png.mime_type().equals("image/png"));
  EXPECT_TRUE(qoi.mime_type().equals("image/qoi"));

  EXPECT_GT(bmp.inspect_data(bmp_signature, BL_ARRAY_SIZE(bmp_signature)), 0u);
  EXPECT_GT(jpg.inspect_data(jpg_signature, BL_ARRAY_SIZE(jpg_signature)), 0u);
  EXPECT_GT(png.inspect_data(png_signature, BL_ARRAY_SIZE(png_signature)), 0u);
  EXPECT_GT(qoi.inspect_data(qoi_signature, BL_ARRAY_SIZE(qoi_signature)), 0u);

  EXPECT_NE(bmp.features(), BL_IMAGE_CODEC_NO_FEATURES);
  EXPECT_NE(jpg.features(), BL_IMAGE_CODEC_NO_FEATURES);
  EXPECT_NE(png.features(), BL_IMAGE_CODEC_NO_FEATURES);
  EXPECT_NE(qoi.features(), BL_IMAGE_CODEC_NO_FEATURES);

  EXPECT_OK(codec.find_by_extension("bmp"));
  EXPECT_EQ(codec, bmp);
  EXPECT_OK(codec.reset());

  EXPECT_OK(codec.find_by_extension(".bmp"));
  EXPECT_EQ(codec, bmp);
  EXPECT_OK(codec.reset());

  EXPECT_OK(codec.find_by_extension("SomeFile.BMp"));
  EXPECT_EQ(codec, bmp);
  EXPECT_OK(codec.reset());

  EXPECT_OK(codec.find_by_extension(".jpg"));
  EXPECT_EQ(codec, jpg);
  EXPECT_OK(codec.reset());

  EXPECT_OK(codec.find_by_extension(".jpeg"));
  EXPECT_EQ(codec, jpg);
  EXPECT_OK(codec.reset());

  EXPECT_OK(codec.find_by_extension("png"));
  EXPECT_EQ(codec, png);
  EXPECT_OK(codec.reset());

  EXPECT_OK(codec.find_by_extension(".png"));
  EXPECT_EQ(codec, png);
  EXPECT_OK(codec.reset());

  EXPECT_OK(codec.find_by_extension(".qoi"));
  EXPECT_EQ(codec, qoi);
  EXPECT_OK(codec.reset());

  EXPECT_OK(codec.find_by_data(bmp_signature, BL_ARRAY_SIZE(bmp_signature)));
  EXPECT_EQ(codec, bmp);
  EXPECT_OK(codec.reset());

  EXPECT_OK(codec.find_by_data(png_signature, BL_ARRAY_SIZE(png_signature)));
  EXPECT_EQ(codec, png);
  EXPECT_OK(codec.reset());

  EXPECT_OK(codec.find_by_data(jpg_signature, BL_ARRAY_SIZE(jpg_signature)));
  EXPECT_EQ(codec, jpg);
  EXPECT_OK(codec.reset());

  EXPECT_OK(codec.find_by_data(qoi_signature, BL_ARRAY_SIZE(qoi_signature)));
  EXPECT_EQ(codec, qoi);
  EXPECT_OK(codec.reset());
}

TEST_CASE(core_image_codec_built_in) {
  BLArray<BLImageCodec> codecs = BLImageCodec::built_in_codecs();

  size_t codec_count = codecs.size();
  // At least 1 built-in codec should be always available.
  EXPECT_GT(codec_count, 0u);

  BLImageCodec removed = codecs[0];
  EXPECT_OK(BLImageCodec::remove_from_built_in(removed));
  EXPECT_EQ(BLImageCodec::remove_from_built_in(removed), BL_ERROR_NO_ENTRY);
  EXPECT_EQ(BLImageCodec::built_in_codecs().size(), codec_count - 1u);

  BLImageCodec codec;
  EXPECT_EQ(codec.find_by_name(removed.name()), BL_ERROR_IMAGE_NO_MATCHING_CODEC);

  EXPECT_OK(BLImageCodec::add_to_built_in(removed));
  EXPECT_EQ(BLImageCodec::add_to_built_in(removed), BL_ERROR_ALREADY_EXISTS);
  EXPECT_OK(codec.find_by_name(removed.name()));
  EXPECT_EQ(codec, removed);
}

TEST_CASE(core_image_codec_c_api_vs_cxx_api) {
  BLImageCodecCore bmp1;
  BLImageCodecCore bmp2;

  EXPECT_OK(bl_image_codec_init(&bmp1));
  EXPECT_OK(bl_image_codec_find_by_name_cstr(&bmp1, "BMP", nullptr));
  EXPECT_OK(bl_image_codec_init_by_name_view(&bmp2, "BMP", 3, nullptr));

  EXPECT_TRUE(bl_image_codec_equals(&bmp1, &bmp2));

  EXPECT_OK(bl_image_codec_destroy(&bmp1));
  EXPECT_OK(bl_image_codec_destroy(&bmp2));

  BLArray<BLImageCodec> codecs1;
  EXPECT_OK(bl_image_codec_array_assign_built_in_codecs(&codecs1));
  BLArray<BLImageCodec> codecs2 = BLImageCodec::built_in_codecs();
  EXPECT_EQ(codecs1, codecs2);
}

} // {bl::ImageCodecTests}

#endif // !BL_BUILD_NO_CODECS && BL_TEST
