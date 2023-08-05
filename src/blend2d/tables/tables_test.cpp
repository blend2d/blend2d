// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_test_p.h"
#if defined(BL_TEST)

#include "../math_p.h"
#include "../tables/tables_p.h"
#include "../support/intops_p.h"

// BLCommonTable - Tests
// =====================

namespace BLTablesTests {

static BL_INLINE uint32_t unpremultiplyAsFloatOp(uint32_t c, uint32_t a) noexcept {
  float cf = float(c);
  float af = blMax(float(a), 0.0001f);

  return uint32_t(blRoundToInt((cf / af) * 255.0f));
}

UNIT(tables, BL_TEST_GROUP_CORE_UTILITIES) {
#if BL_TARGET_ARCH_X86
  // Make sure that the 256-bit constants are properly aligned.
  INFO("Testing 'blCommonTable' alignment (X86 specific)");
  EXPECT_TRUE(BLIntOps::isAligned(&blCommonTable, 64));
#endif

  INFO("Testing 'blCommonTable.unpremultiplyRcp' correctness");
  {
    for (uint32_t a = 0; a < 256; a++) {
      for (uint32_t c = 0; c <= a; c++) {
        uint32_t u0 = (c * blCommonTable.unpremultiplyRcp[a] + 0x8000u) >> 16;
        uint32_t u1 = unpremultiplyAsFloatOp(c, a);

        EXPECT_EQ(u0, u1).message("Value[0x%02X] != Expected[0x%02X] [C=%u, A=%u]", u0, u1, c, a);
      }
    }
  }

#if BL_TARGET_ARCH_X86
  INFO("Testing 'blCommonTable.unpremultiplyPmaddwd[Rcp|Rnd]' correctness (X86 specific)");
  {
    for (uint32_t a = 0; a < 256; a++) {
      for (uint32_t c = 0; c <= a; c++) {
        uint32_t c0 = c;
        uint32_t c1 = c << 6;

        uint32_t r0 = blCommonTable.unpremultiplyPmaddwdRcp[a] & 0xFFFF;
        uint32_t r1 = blCommonTable.unpremultiplyPmaddwdRcp[a] >> 16;
        uint32_t rnd = blCommonTable.unpremultiplyPmaddwdRnd[a];

        uint32_t u0 = (c0 * r0 + c1 * r1 + rnd) >> 13;
        uint32_t u1 = unpremultiplyAsFloatOp(c, a);

        EXPECT_EQ(u0, u1).message("Value[0x%02X] != Expected[0x%02X] [C=%u, A=%u]", u0, u1, c, a);
      }
    }
  }
#endif
}

} // {BLTablesTests}

#endif // BL_TEST
