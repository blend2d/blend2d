// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../math_p.h"
#include "../support/algorithm_p.h"

#ifdef BL_TEST
template<typename T>
static void checkArrays(const T* a, const T* b, size_t size) noexcept {
  for (size_t i = 0; i < size; i++) {
    EXPECT_EQ(a[i], b[i]).message("Mismatch at %u", unsigned(i));
  }
}

UNIT(support_algorithm, -9) {
  INFO("BLAlgorithm::lowerBound()");
  {
    static const int arr[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12 };

    EXPECT_EQ(BLAlgorithm::lowerBound(arr, 0, 0), 0u);
    EXPECT_EQ(BLAlgorithm::lowerBound(arr, BL_ARRAY_SIZE(arr), -11000), 0u);
    EXPECT_EQ(BLAlgorithm::lowerBound(arr, BL_ARRAY_SIZE(arr), 0), 0u);
    EXPECT_EQ(BLAlgorithm::lowerBound(arr, BL_ARRAY_SIZE(arr), 1), 1u);
    EXPECT_EQ(BLAlgorithm::lowerBound(arr, BL_ARRAY_SIZE(arr), 2), 2u);
    EXPECT_EQ(BLAlgorithm::lowerBound(arr, BL_ARRAY_SIZE(arr), 3), 3u);
    EXPECT_EQ(BLAlgorithm::lowerBound(arr, BL_ARRAY_SIZE(arr), 4), 4u);
    EXPECT_EQ(BLAlgorithm::lowerBound(arr, BL_ARRAY_SIZE(arr), 5), 5u);
    EXPECT_EQ(BLAlgorithm::lowerBound(arr, BL_ARRAY_SIZE(arr), 6), 6u);
    EXPECT_EQ(BLAlgorithm::lowerBound(arr, BL_ARRAY_SIZE(arr), 10), 10u);
    EXPECT_EQ(BLAlgorithm::lowerBound(arr, BL_ARRAY_SIZE(arr), 11), 11u);
    EXPECT_EQ(BLAlgorithm::lowerBound(arr, BL_ARRAY_SIZE(arr), 12), 11u);
    EXPECT_EQ(BLAlgorithm::lowerBound(arr, BL_ARRAY_SIZE(arr), 11000), BL_ARRAY_SIZE(arr));
  }

  INFO("BLAlgorithm::quickSort() - Testing qsort and isort of predefined arrays");
  {
    constexpr size_t kArraySize = 11;

    int ref_[kArraySize] = { -4, -2, -1, 0, 1, 9, 12, 13, 14, 19, 22 };
    int arr1[kArraySize] = { 0, 1, -1, 19, 22, 14, -4, 9, 12, 13, -2 };
    int arr2[kArraySize];

    memcpy(arr2, arr1, kArraySize * sizeof(int));

    BLAlgorithm::insertionSort(arr1, kArraySize);
    BLAlgorithm::quickSort(arr2, kArraySize);
    checkArrays(arr1, ref_, kArraySize);
    checkArrays(arr2, ref_, kArraySize);
  }

  INFO("BLAlgorithm::quickSort() - Testing qsort and isort of artificial arrays");
  {
    constexpr size_t kArraySize = 200;

    int arr1[kArraySize];
    int arr2[kArraySize];
    int ref_[kArraySize];

    for (size_t size = 2; size < kArraySize; size++) {
      for (size_t i = 0; i < size; i++) {
        arr1[i] = int(size - 1 - i);
        arr2[i] = int(size - 1 - i);
        ref_[i] = int(i);
      }

      BLAlgorithm::insertionSort(arr1, size);
      BLAlgorithm::quickSort(arr2, size);
      checkArrays(arr1, ref_, size);
      checkArrays(arr2, ref_, size);
    }
  }

  INFO("BLAlgorithm::quickSort() - Testing qsort and isort having unstable compare function");
  {
    constexpr size_t kArraySize = 5;

    float arr1[kArraySize] = { 1.0f, 0.0f, 3.0f, -1.0f, blNaN<float>() };
    float arr2[kArraySize] = { };

    memcpy(arr2, arr1, kArraySize * sizeof(float));

    // We don't test as it's undefined where the NaN would be.
    BLAlgorithm::insertionSort(arr1, kArraySize);
    BLAlgorithm::quickSort(arr2, kArraySize);
  }

  INFO("BLAlgorithm::binarySearch() - Testing binary search");
  {
    static const int arr[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    for (size_t size = BL_ARRAY_SIZE(arr); size > 0; size--) {
      for (size_t i = 0; i < size; i++) {
        int value = arr[i];
        EXPECT_EQ(BLAlgorithm::binarySearch(arr, size, value), i);
        EXPECT_EQ(BLAlgorithm::binarySearchClosestFirst(arr, size, value), i);
        EXPECT_EQ(BLAlgorithm::binarySearchClosestLast(arr, size, value), i);
      }
    }
  }
}
#endif
