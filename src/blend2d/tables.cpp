// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "tables_p.h"

#ifdef BL_TEST
#include "math_p.h"
#include "support/intops_p.h"
#endif

// BLBitCountOfByteTable
// =====================

struct BLBitCountOfByteTableGen {
  static constexpr uint8_t value(size_t i) noexcept {
    return uint8_t( ((i >> 0) & 1) + ((i >> 1) & 1) + ((i >> 2) & 1) + ((i >> 3) & 1) +
                    ((i >> 4) & 1) + ((i >> 5) & 1) + ((i >> 6) & 1) + ((i >> 7) & 1) );
  }
};

static constexpr const BLLookupTable<uint8_t, 256> blBitCountByteTable_
  = blMakeLookupTable<uint8_t, 256, BLBitCountOfByteTableGen>();

const BLLookupTable<uint8_t, 256> blBitCountByteTable = blBitCountByteTable_;

// BLModuloTable
// =============

#define INV()  {{ 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0   , 0   , 0   , 0   , 0   , 0   , 0    }}
#define DEF(N) {{ 1%N, 2%N, 3%N, 4%N, 5%N, 6%N, 7%N, 8%N, 9%N, 10%N, 11%N, 12%N, 13%N, 14%N, 15%N, 16%N }}

const BLModuloTable blModuloTable[18] = {
  INV(  ), DEF( 1), DEF( 2), DEF( 3),
  DEF( 4), DEF( 5), DEF( 6), DEF( 7),
  DEF( 8), DEF( 9), DEF(10), DEF(11),
  DEF(12), DEF(13), DEF(14), DEF(15),
  DEF(16), DEF(17)
};

#undef DEF
#undef INV

// BLCommonTable
// =============

// NOTE: We must go through `blCommonTable_` as it's the only way to convince MSVC to emit constexpr. If this step
// is missing it will emit initialization code for this const data, which is exactly what we don't want. Also, we
// cannot just add `constexpr` to the real `blCommonTable` as MSVC would complain about different storage type.
static constexpr const BLCommonTable blCommonTable_;
const BLCommonTable blCommonTable = blCommonTable_;

// BLCommonTable - Tests
// =====================

#ifdef BL_TEST
static BL_INLINE uint32_t unpremultiplyAsFloatOp(uint32_t c, uint32_t a) noexcept {
  float cf = float(c);
  float af = blMax(float(a), 0.0001f);

  return uint32_t(blRoundToInt((cf / af) * 255.0f));
}

UNIT(tables, -10) {
#if BL_TARGET_ARCH_X86
  // Make sure that the 256-bit constants are properly aligned.
  INFO("Testing 'blCommonTable' alignment (X86 specific)");
  EXPECT_TRUE(BLIntOps::isAligned(&blCommonTable, 32));
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
#endif
