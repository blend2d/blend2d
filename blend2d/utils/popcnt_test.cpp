// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/utils/popcnt_p.h>

namespace bl::UtilsTests {

TEST_CASE(support_intops_popcnt) {
  TEST_LOG("Testing Utils::PopCounter<uint32_t>");

  static const uint32_t bit_word_data[] = {
    0xFFFFFFFF, 0x11881111, 0x10000000, 0x08000000,
    0x00000001, 0x00000008, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFEFEFEFE, 0xCCCCCCCC, 0xBACFE1D9, 0x11100111,
    0x12DFEAAA, 0xFE1290AA, 0xF1018021, 0x00000000,
    0x23467111, 0x11111111, 0x137F137F
  };

  for (uint32_t i = 1; i <= BL_ARRAY_SIZE(bit_word_data); i++) {
    Utils::PopCounterSimple<uint32_t> simple;
    Utils::PopCounterHarleySeal<uint32_t> harley_seal;

    simple.add_array(bit_word_data, i);
    harley_seal.add_array(bit_word_data, i);

    EXPECT_EQ(simple.get(), harley_seal.get());
  }
}

} // {bl::UtilsTests}

#endif // BL_TEST
