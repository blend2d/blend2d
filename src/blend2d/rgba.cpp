// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "rgba_p.h"

// BLRgba - Tests
// ==============

#ifdef BL_TEST
UNIT(rgba, -7) {
  BLRgba32 c32(0x01, 0x02, 0x03, 0xFF);
  BLRgba64 c64(0x100, 0x200, 0x300, 0xFFFF);

  EXPECT_EQ(c32.value, 0xFF010203u);
  EXPECT_EQ(c64.value, 0xFFFF010002000300u);

  EXPECT_EQ(BLRgba64(c32).value, 0xFFFF010102020303u);
  EXPECT_EQ(BLRgba32(c64).value, 0xFF010203u);
}
#endif
