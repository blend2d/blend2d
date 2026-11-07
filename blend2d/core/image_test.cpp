// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/axl/mem_alloc.h>
#include <blend2d/core/image_p.h>

// Image - Tests
// =============

namespace bl::ImageTests {

constexpr uint32_t kTestImageSize = 256;

TEST_CASE(core_image_create_modify) {
  BLImage img0;
  BLImage img1;

  EXPECT_OK(img0.create(kTestImageSize, kTestImageSize, BL_FORMAT_PRGB32));
  EXPECT_OK(img1.create(kTestImageSize, kTestImageSize, BL_FORMAT_PRGB32));

  EXPECT_EQ(img0.width(), int(kTestImageSize));
  EXPECT_EQ(img0.height(), int(kTestImageSize));
  EXPECT_EQ(img0.format(), BL_FORMAT_PRGB32);

  EXPECT_EQ(img1.width(), int(kTestImageSize));
  EXPECT_EQ(img1.height(), int(kTestImageSize));
  EXPECT_EQ(img1.format(), BL_FORMAT_PRGB32);

  {
    BLImageData img_data0;
    BLImageData img_data1;

    EXPECT_OK(img0.make_mutable(&img_data0));
    EXPECT_OK(img1.make_mutable(&img_data1));

    EXPECT_EQ(img_data0.size.w, int(kTestImageSize));
    EXPECT_EQ(img_data0.size.h, int(kTestImageSize));
    EXPECT_EQ(img_data0.format, BL_FORMAT_PRGB32);

    EXPECT_EQ(img_data1.size.w, int(kTestImageSize));
    EXPECT_EQ(img_data1.size.h, int(kTestImageSize));
    EXPECT_EQ(img_data1.format, BL_FORMAT_PRGB32);

    // Direct memory manipulation.
    for (size_t y = 0; y < kTestImageSize; y++) {
      memset(static_cast<uint8_t*>(img_data0.pixel_data) + intptr_t(y) * img_data0.stride, int(y & 0xFF), kTestImageSize * 4);
      memset(static_cast<uint8_t*>(img_data1.pixel_data) + intptr_t(y) * img_data1.stride, int(y & 0xFF), kTestImageSize * 4);
    }
  }
  EXPECT_EQ(img0, img1);

  {
    BLImageData img_data0;
    EXPECT_OK(img0.make_mutable(&img_data0));

    for (size_t y = 0; y < kTestImageSize; y++) {
      uint32_t* line = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(img_data0.pixel_data) + intptr_t(y) * img_data0.stride);
      for (uint32_t x = 0; x < kTestImageSize; x++) {
        line[x] = uint32_t((x + y) & 0xFF) << 24;
      }
    }
  }
  img1 = img0;
  EXPECT_EQ(img0, img1);

  {
    BLImageData img_data0;
    BLImageData img_data1;

    EXPECT_OK(img0.make_mutable(&img_data0));
    EXPECT_OK(img1.make_mutable(&img_data1));

    EXPECT_NE(img_data0.pixel_data, img_data1.pixel_data);
    EXPECT_EQ(img0, img1);
  }
}

TEST_CASE(core_image_create_from_data) {
  struct ExternalDataInfo {
    void* external_data;
    size_t destroy_count;
  };

  void* external_data_ptr = axl::mem_alloc(kTestImageSize * kTestImageSize * 4);
  EXPECT_NE(external_data_ptr, nullptr);

  BLImage img;
  BLImageData img_data;

  // Test create_from_data() without a destroy handler.
  EXPECT_OK(img.create_from_data(kTestImageSize, kTestImageSize, BL_FORMAT_PRGB32, external_data_ptr, kTestImageSize * 4, BL_DATA_ACCESS_MODE_RW));
  EXPECT_EQ(img.width(), int(kTestImageSize));
  EXPECT_EQ(img.height(), int(kTestImageSize));
  EXPECT_EQ(img.format(), BL_FORMAT_PRGB32);

  EXPECT_OK(img.make_mutable(&img_data));
  EXPECT_EQ(img_data.size.w, int(kTestImageSize));
  EXPECT_EQ(img_data.size.h, int(kTestImageSize));
  EXPECT_EQ(img_data.format, BL_FORMAT_PRGB32);
  EXPECT_EQ(img_data.pixel_data, external_data_ptr);

  EXPECT_OK(img.reset());

  // Test create_from_data() with a destroy handler.
  ExternalDataInfo external_data_info {};

  auto destroy_func = [](BLObjectCore* object, void* external_data, void* user_data) noexcept -> void {
    axl::maybe_unused(object);
    ExternalDataInfo* info = static_cast<ExternalDataInfo*>(user_data);
    info->destroy_count++;
    info->external_data = external_data;
  };

  EXPECT_OK(img.create_from_data_mut(kTestImageSize, kTestImageSize, BL_FORMAT_PRGB32, external_data_ptr, kTestImageSize * 4, destroy_func, &external_data_info));
  EXPECT_EQ(img.width(), int(kTestImageSize));
  EXPECT_EQ(img.height(), int(kTestImageSize));
  EXPECT_EQ(img.format(), BL_FORMAT_PRGB32);

  EXPECT_OK(img.make_mutable(&img_data));
  EXPECT_EQ(img_data.size.w, int(kTestImageSize));
  EXPECT_EQ(img_data.size.h, int(kTestImageSize));
  EXPECT_EQ(img_data.format, BL_FORMAT_PRGB32);
  EXPECT_EQ(img_data.pixel_data, external_data_ptr);

  // Verify our info is zero initialized.
  EXPECT_EQ(external_data_info.external_data, nullptr);
  EXPECT_EQ(external_data_info.destroy_count, 0u);

  // Destroy the image, should call the destroy handler.
  EXPECT_OK(img.reset());

  // Verify the destroy handler was called once.
  EXPECT_EQ(external_data_info.external_data, external_data_ptr);
  EXPECT_EQ(external_data_info.destroy_count, 1u);

  axl::mem_free(external_data_ptr);
}

TEST_CASE(core_image_convert) {
  BLImage img0;
  BLImage img1;

  EXPECT_OK(img0.create(kTestImageSize, kTestImageSize, BL_FORMAT_PRGB32));
  EXPECT_OK(img1.create(kTestImageSize, kTestImageSize, BL_FORMAT_A8));

  EXPECT_EQ(img0.width(), int(kTestImageSize));
  EXPECT_EQ(img0.height(), int(kTestImageSize));
  EXPECT_EQ(img0.format(), BL_FORMAT_PRGB32);

  EXPECT_EQ(img1.width(), int(kTestImageSize));
  EXPECT_EQ(img1.height(), int(kTestImageSize));
  EXPECT_EQ(img1.format(), BL_FORMAT_A8);

  BLImageData img_data0;
  BLImageData img_data1;

  EXPECT_OK(img0.make_mutable(&img_data0));
  EXPECT_OK(img1.make_mutable(&img_data1));

  for (size_t y = 0; y < kTestImageSize; y++) {
    uint32_t* line0 = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(img_data0.pixel_data) + intptr_t(y) * img_data0.stride);
    uint8_t* line1 = static_cast<uint8_t*>(img_data1.pixel_data) + intptr_t(y) * img_data1.stride;

    for (uint32_t x = 0; x < kTestImageSize; x++) {
      line0[x] = uint32_t((x + y) & 0xFF) << 24;
      line1[x] = uint8_t((x + y) & 0xFF);
    }
  }

  EXPECT_OK(img0.convert(BL_FORMAT_A8));
  EXPECT_TRUE(img0.equals(img1));
}

} // {bl::ImageTests}

#endif // BL_TEST
