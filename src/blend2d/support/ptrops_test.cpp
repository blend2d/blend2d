// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_test_p.h"
#if defined(BL_TEST)

#include "../support/ptrops_p.h"

namespace BLPtrOpsTests {

UNIT(support_ptrops, BL_TEST_GROUP_SUPPORT_UTILITIES) {
  INFO("BLPtrOps - Offset / Deoffset");
  {
    uint32_t array[16] = { 0 };

    EXPECT_EQ(BLPtrOps::offset(array, 4), array + 1);
    EXPECT_EQ(BLPtrOps::deoffset(array + 1, 4), array);

    EXPECT_TRUE(BLPtrOps::bothAligned((void*)(uintptr_t)0x0, (void*)(uintptr_t)0x4, 4));
    EXPECT_FALSE(BLPtrOps::bothAligned((void*)(uintptr_t)0x1, (void*)(uintptr_t)0x4, 4));
    EXPECT_FALSE(BLPtrOps::bothAligned((void*)(uintptr_t)0x1, (void*)(uintptr_t)0x5, 4));
    EXPECT_TRUE(BLPtrOps::bothAligned((void*)(uintptr_t)0x10, (void*)(uintptr_t)0x20, 16));
    EXPECT_FALSE(BLPtrOps::bothAligned((void*)(uintptr_t)0x1, (void*)(uintptr_t)0x5, 16));

    EXPECT_TRUE(BLPtrOps::haveEqualAlignment((void*)(uintptr_t)0x1, (void*)(uintptr_t)0x5, 4));
    EXPECT_TRUE(BLPtrOps::haveEqualAlignment((void*)(uintptr_t)0x1, (void*)(uintptr_t)0x11, 16));
    EXPECT_FALSE(BLPtrOps::haveEqualAlignment((void*)(uintptr_t)0x1, (void*)(uintptr_t)0x12, 16));
  }
}

} // {BLPtrOpsTests}

#endif // BL_TEST
