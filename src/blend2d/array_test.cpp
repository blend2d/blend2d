// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_test_p.h"
#if defined(BL_TEST)

#include "array_p.h"
#include "object_p.h"
#include "string_p.h"
#include "var_p.h"

// bl::Array - Tests
// =================

namespace bl::Tests {

UNIT(array, BL_TEST_GROUP_CORE_CONTAINERS) {
  INFO("Basic functionality - BLArray<int>");
  {
    BLArray<int> a;
    EXPECT_EQ(a.size(), 0u);
    EXPECT_GT(a.capacity(), 0u);
    EXPECT_TRUE(a._d.sso());

    // [42]
    EXPECT_SUCCESS(a.append(42));
    EXPECT_EQ(a.size(), 1u);
    EXPECT_EQ(a[0], 42);
    EXPECT_TRUE(a._d.sso());

    // [42, 1, 2, 3]
    EXPECT_SUCCESS(a.append(1, 2, 3));
    EXPECT_EQ(a.size(), 4u);
    EXPECT_GE(a.capacity(), 4u);
    EXPECT_EQ(a[0], 42);
    EXPECT_EQ(a[1], 1);
    EXPECT_EQ(a[2], 2);
    EXPECT_EQ(a[3], 3);
    EXPECT_FALSE(a._d.sso());

    // [10, 42, 1, 2, 3]
    EXPECT_SUCCESS(a.prepend(10));
    EXPECT_EQ(a.size(), 5u);
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[1], 42);
    EXPECT_EQ(a[2], 1);
    EXPECT_EQ(a[3], 2);
    EXPECT_EQ(a[4], 3);
    EXPECT_EQ(a.indexOf(4), SIZE_MAX);
    EXPECT_EQ(a.indexOf(3), 4u);
    EXPECT_EQ(a.lastIndexOf(4), SIZE_MAX);
    EXPECT_EQ(a.lastIndexOf(10), 0u);

    BLArray<int> b;
    EXPECT_SUCCESS(b.append(10, 42, 1, 2, 3));
    EXPECT_TRUE(a.equals(b));
    EXPECT_SUCCESS(b.append(99));
    EXPECT_FALSE(a.equals(b));

    // [10, 3]
    EXPECT_SUCCESS(a.remove(BLRange{1, 4}));
    EXPECT_EQ(a.size(), 2u);
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[1], 3);

    // [10, 33, 3]
    EXPECT_SUCCESS(a.insert(1, 33));
    EXPECT_EQ(a.size(), 3u);
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[1], 33);
    EXPECT_EQ(a[2], 3);

    // [10, 33, 3, 999, 1010, 2293]
    EXPECT_SUCCESS(a.insert(2, 999, 1010, 2293));
    EXPECT_EQ(a.size(), 6u);
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[1], 33);
    EXPECT_EQ(a[2], 999);
    EXPECT_EQ(a[3], 1010);
    EXPECT_EQ(a[4], 2293);
    EXPECT_EQ(a[5], 3);

    EXPECT_SUCCESS(a.insert(6, 1));
    EXPECT_EQ(a[6], 1);

    EXPECT_SUCCESS(a.clear());
    EXPECT_SUCCESS(a.insert(0, 1));
    EXPECT_SUCCESS(a.insert(1, 2));
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(a[1], 2);
  }

  INFO("Basic functionality - BLArray<uint8_t>");
  {
    auto accumulate = [](const BLArray<uint8_t>& array) noexcept -> uint32_t {
      uint32_t acc = 0;
      for (auto v : array.view())
        acc += v;
      return acc;
    };

    BLArray<uint8_t> a;
    EXPECT_TRUE(a.empty());

    for (size_t i = 0; i < 256; i++) {
      EXPECT_SUCCESS(a.append(uint8_t(i & 0xFF)));
      EXPECT_EQ(a.size(), i + 1);
    }
    EXPECT_EQ(accumulate(a), uint32_t(255 * 128));

    BLArray<uint8_t> b;
    for (uint32_t i = 0; i < 256; i++) {
      b.appendData(a.view());
    }
    EXPECT_EQ(accumulate(b), uint32_t(255 * 128 * 256));

    a.reset();
    EXPECT_TRUE(a.empty());
    EXPECT_TRUE(a._d.sso());

    for (size_t i = 0; i < 256; i += 2) {
      a.append(uint8_t(i & 0xFFu), uint8_t((i + 1) & 0xFFu));
    }
    EXPECT_EQ(accumulate(a), uint32_t(255 * 128));

    for (size_t i = 0; i < 256 * 255; i += 2) {
      a.append(uint8_t(i & 0xFFu), uint8_t((i + 1) & 0xFFu));
    }
    EXPECT_EQ(accumulate(b), uint32_t(255 * 128 * 256));
  }

  INFO("Basic functionality - BLArray<uint64_t>");
  {
    BLArray<uint64_t> a;

    EXPECT_EQ(a.size(), 0u);
    EXPECT_GT(a.capacity(), 0u);
    EXPECT_TRUE(a._d.sso());

    for (size_t i = 0; i < 1000; i++)
      EXPECT_SUCCESS(a.append(i));

    // NOTE: AppendItem must work, but it's never called by C++ API (C++ API would call blArrayAppendU64 instead).
    for (uint64_t i = 0; i < 1000; i++)
      EXPECT_SUCCESS(blArrayAppendItem(&a, &i));

    EXPECT_EQ(a.size(), 2000u);
    for (size_t i = 0; i < 2000; i++)
      EXPECT_EQ(a[i], i % 1000u);
  }

  INFO("Basic functionality - C API");
  {
    BLArrayCore a;
    BLArray<uint64_t> b;

    EXPECT_SUCCESS(blArrayInit(&a, BL_OBJECT_TYPE_ARRAY_UINT64));
    EXPECT_EQ(blArrayGetSize(&a), b.size());
    EXPECT_EQ(blArrayGetCapacity(&a), b.capacity());

    const uint64_t items[] = { 1, 2, 3, 4, 5 };
    EXPECT_SUCCESS(blArrayAppendData(&a, &items, BL_ARRAY_SIZE(items)));
    EXPECT_EQ(blArrayGetSize(&a), 5u);

    for (size_t i = 0; i < BL_ARRAY_SIZE(items); i++)
      EXPECT_EQ(static_cast<const uint64_t*>(blArrayGetData(&a))[i], items[i]);

    EXPECT_SUCCESS(blArrayInsertData(&a, 1, &items, BL_ARRAY_SIZE(items)));
    const uint64_t itemsAfterInsertion[] = { 1, 1, 2, 3, 4, 5, 2, 3, 4, 5 };
    for (size_t i = 0; i < BL_ARRAY_SIZE(itemsAfterInsertion); i++)
      EXPECT_EQ(static_cast<const uint64_t*>(blArrayGetData(&a))[i], itemsAfterInsertion[i]);

    EXPECT_SUCCESS(blArrayDestroy(&a));
  }

  INFO("External array");
  {
    BLArray<int> a;
    int externalData[4] = { 0 };

    EXPECT_SUCCESS(a.assignExternalData(externalData, 0, 4, BL_DATA_ACCESS_RW));
    EXPECT_EQ(a.data(), externalData);

    EXPECT_SUCCESS(a.append(42));
    EXPECT_EQ(externalData[0], 42);

    EXPECT_SUCCESS(a.append(1, 2, 3));
    EXPECT_EQ(externalData[3], 3);

    // Appending more items the external array can hold must reallocate it.
    EXPECT_SUCCESS(a.append(4));
    EXPECT_NE(a.data(), externalData);
    EXPECT_EQ(a[0], 42);
    EXPECT_EQ(a[1], 1);
    EXPECT_EQ(a[2], 2);
    EXPECT_EQ(a[3], 3);
    EXPECT_EQ(a[4], 4);
  }

  INFO("String array");
  {
    BLArray<BLString> a;
    EXPECT_EQ(a.size(), 0u);

    a.append(BLString("Hello"));
    EXPECT_EQ(a.size(), 1u);
    EXPECT_TRUE(a[0].equals("Hello"));

    a.insert(0, BLString("Blend2D"));
    EXPECT_EQ(a.size(), 2u);
    EXPECT_TRUE(a[0].equals("Blend2D"));
    EXPECT_TRUE(a[1].equals("Hello"));

    a.insert(2, BLString("World!"));
    EXPECT_EQ(a.size(), 3u);
    EXPECT_TRUE(a[0].equals("Blend2D"));
    EXPECT_TRUE(a[1].equals("Hello"));
    EXPECT_TRUE(a[2].equals("World!"));

    a.insertData(1, a.view());
    EXPECT_EQ(a.size(), 6u);
    EXPECT_TRUE(a[0].equals("Blend2D"));
    EXPECT_TRUE(a[1].equals("Blend2D"));
    EXPECT_TRUE(a[2].equals("Hello"));
    EXPECT_TRUE(a[3].equals("World!"));
    EXPECT_TRUE(a[4].equals("Hello"));
    EXPECT_TRUE(a[5].equals("World!"));
  }
}

} // {bl::Tests}

#endif // BL_TEST
