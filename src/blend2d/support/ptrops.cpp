// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../support/ptrops_p.h"

#ifdef BL_TEST
UNIT(support_ptrops, -10) {
  INFO("BLPtrOps - Offset / Deoffset");
  {
    uint32_t array[16] = { 0 };

    EXPECT_EQ(BLPtrOps::offset(array, 4), array + 1);
    EXPECT_EQ(BLPtrOps::deoffset(array + 1, 4), array);
  }
}
#endif
