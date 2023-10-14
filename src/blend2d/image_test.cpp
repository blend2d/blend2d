// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_test_p.h"
#if defined(BL_TEST)

#include "image_p.h"

// bl::Image - Tests
// =================

namespace bl {
namespace Tests {

UNIT(image, BL_TEST_GROUP_IMAGE_CONTAINERS) {
  constexpr uint32_t kSize = 256;

  INFO("Testing BLImage::create() and BLImage::makeMutable()");
  {
    BLImage img0;
    BLImage img1;

    EXPECT_SUCCESS(img0.create(kSize, kSize, BL_FORMAT_PRGB32));
    EXPECT_SUCCESS(img1.create(kSize, kSize, BL_FORMAT_PRGB32));

    EXPECT_EQ(img0.width(), int(kSize));
    EXPECT_EQ(img0.height(), int(kSize));
    EXPECT_EQ(img0.format(), BL_FORMAT_PRGB32);

    EXPECT_EQ(img1.width(), int(kSize));
    EXPECT_EQ(img1.height(), int(kSize));
    EXPECT_EQ(img1.format(), BL_FORMAT_PRGB32);

    BLImageData imgData0;
    BLImageData imgData1;

    EXPECT_SUCCESS(img0.makeMutable(&imgData0));
    EXPECT_SUCCESS(img1.makeMutable(&imgData1));

    EXPECT_EQ(imgData0.size.w, int(kSize));
    EXPECT_EQ(imgData0.size.h, int(kSize));
    EXPECT_EQ(imgData0.format, BL_FORMAT_PRGB32);

    EXPECT_EQ(imgData1.size.w, int(kSize));
    EXPECT_EQ(imgData1.size.h, int(kSize));
    EXPECT_EQ(imgData1.format, BL_FORMAT_PRGB32);

    // Direct memory manipulation.
    for (uint32_t y = 0; y < kSize; y++) {
      memset(static_cast<uint8_t*>(imgData0.pixelData) + y * imgData0.stride, int(y & 0xFF), kSize * 4);
      memset(static_cast<uint8_t*>(imgData1.pixelData) + y * imgData1.stride, int(y & 0xFF), kSize * 4);
    }

    EXPECT_TRUE(img0.equals(img1));
  }

  INFO("Testing BLImage::create() and BLImage::convert()");
  {
    BLImage img0;
    BLImage img1;

    EXPECT_SUCCESS(img0.create(kSize, kSize, BL_FORMAT_PRGB32));
    EXPECT_SUCCESS(img1.create(kSize, kSize, BL_FORMAT_A8));

    EXPECT_EQ(img0.width(), int(kSize));
    EXPECT_EQ(img0.height(), int(kSize));
    EXPECT_EQ(img0.format(), BL_FORMAT_PRGB32);

    EXPECT_EQ(img1.width(), int(kSize));
    EXPECT_EQ(img1.height(), int(kSize));
    EXPECT_EQ(img1.format(), BL_FORMAT_A8);

    BLImageData imgData0;
    BLImageData imgData1;

    EXPECT_SUCCESS(img0.makeMutable(&imgData0));
    EXPECT_SUCCESS(img1.makeMutable(&imgData1));

    for (uint32_t y = 0; y < kSize; y++) {
      uint32_t* line0 = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(imgData0.pixelData) + y * imgData0.stride);
      uint8_t* line1 = static_cast<uint8_t*>(imgData1.pixelData) + y * imgData1.stride;

      for (uint32_t x = 0; x < kSize; x++) {
        line0[x] = uint32_t((x + y) & 0xFF) << 24;
        line1[x] = uint8_t((x + y) & 0xFF);
      }
    }

    EXPECT_SUCCESS(img0.convert(BL_FORMAT_A8));
    EXPECT_TRUE(img0.equals(img1));
  }

  INFO("Testing BLImage::createFromData()");
  {
    struct ExternalDataInfo {
      void* externalData;
      size_t destroyCount;
    };

    void* externalDataPtr = malloc(kSize * kSize * 4);
    EXPECT_NE(externalDataPtr, nullptr);

    BLImage img;
    BLImageData imgData;

    // Test createFromData() without a destroy handler.
    EXPECT_SUCCESS(img.createFromData(kSize, kSize, BL_FORMAT_PRGB32, externalDataPtr, kSize * 4, BL_DATA_ACCESS_RW));
    EXPECT_EQ(img.width(), int(kSize));
    EXPECT_EQ(img.height(), int(kSize));
    EXPECT_EQ(img.format(), BL_FORMAT_PRGB32);

    EXPECT_SUCCESS(img.makeMutable(&imgData));
    EXPECT_EQ(imgData.size.w, int(kSize));
    EXPECT_EQ(imgData.size.h, int(kSize));
    EXPECT_EQ(imgData.format, BL_FORMAT_PRGB32);
    EXPECT_EQ(imgData.pixelData, externalDataPtr);

    EXPECT_SUCCESS(img.reset());

    // Test createFromData() with a destroy handler.
    ExternalDataInfo externalDataInfo {};

    auto destroyFunc = [](void* impl, void* externalData, void* userData) noexcept -> void {
      blUnused(impl);

      ExternalDataInfo* info = static_cast<ExternalDataInfo*>(userData);
      info->destroyCount++;
      info->externalData = externalData;
    };

    EXPECT_SUCCESS(img.createFromData(kSize, kSize, BL_FORMAT_PRGB32, externalDataPtr, kSize * 4, BL_DATA_ACCESS_RW, destroyFunc, &externalDataInfo));
    EXPECT_EQ(img.width(), int(kSize));
    EXPECT_EQ(img.height(), int(kSize));
    EXPECT_EQ(img.format(), BL_FORMAT_PRGB32);

    EXPECT_SUCCESS(img.makeMutable(&imgData));
    EXPECT_EQ(imgData.size.w, int(kSize));
    EXPECT_EQ(imgData.size.h, int(kSize));
    EXPECT_EQ(imgData.format, BL_FORMAT_PRGB32);
    EXPECT_EQ(imgData.pixelData, externalDataPtr);

    // Verify our info is zero initialized.
    EXPECT_EQ(externalDataInfo.externalData, nullptr);
    EXPECT_EQ(externalDataInfo.destroyCount, 0u);

    // Destroy the image, should call the destroy handler.
    EXPECT_SUCCESS(img.reset());

    // Verify the destroy handler was called once.
    EXPECT_EQ(externalDataInfo.externalData, externalDataPtr);
    EXPECT_EQ(externalDataInfo.destroyCount, 1u);

    free(externalDataPtr);
  }

  INFO("Testing BLImage reference counting");
  {
    BLImage img0;
    BLImageData imgData0;

    EXPECT_SUCCESS(img0.create(kSize, kSize, BL_FORMAT_PRGB32));
    EXPECT_SUCCESS(img0.makeMutable(&imgData0));

    for (uint32_t y = 0; y < kSize; y++) {
      uint32_t* line = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(imgData0.pixelData) + y * imgData0.stride);
      for (uint32_t x = 0; x < kSize; x++) {
        line[x] = uint32_t((x + y) & 0xFF) << 24;
      }
    }

    BLImage img1(img0);
    EXPECT_EQ(img0, img1);

    BLImageData imgData1;
    EXPECT_SUCCESS(img0.makeMutable(&imgData0));
    EXPECT_SUCCESS(img1.makeMutable(&imgData1));

    EXPECT_NE(imgData0.pixelData, imgData1.pixelData);
    EXPECT_EQ(img0, img1);
  }
}

} // {Tests}
} // {bl}

#endif // BL_TEST
