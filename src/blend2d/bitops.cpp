// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#include "./api-build_p.h"
#include "./bitops_p.h"
#include "./support_p.h"

// ============================================================================
// [BLSupport - Unit Tests]
// ============================================================================

#ifdef BL_TEST
static void testBitArrayOps() {
  uint32_t bits[3];

  INFO("BLParametrizedBitOps<BL_BIT_ORDER_LSB>::bitArrayFill");
  memset(bits, 0, sizeof(bits));
  BLLSBBitOps<uint32_t>::bitArrayFill(bits, 1, 94);
  EXPECT(bits[0] == 0xFFFFFFFEu);
  EXPECT(bits[1] == 0xFFFFFFFFu);
  EXPECT(bits[2] == 0x7FFFFFFFu);

  INFO("BLParametrizedBitOps<BL_BIT_ORDER_MSB>::bitArrayFill");
  memset(bits, 0, sizeof(bits));
  BLMSBBitOps<uint32_t>::bitArrayFill(bits, 1, 94);
  EXPECT(bits[0] == 0x7FFFFFFFu);
  EXPECT(bits[1] == 0xFFFFFFFFu);
  EXPECT(bits[2] == 0xFFFFFFFEu);
}

static void testBitIterator() {
  INFO("BLParametrizedBitOps<BL_BIT_ORDER_LSB>::BitIterator<uint32_t>");
  BLLSBBitOps<uint32_t>::BitIterator lsbIt(0x40000010u);

  EXPECT(lsbIt.hasNext());
  EXPECT(lsbIt.next() == 4);
  EXPECT(lsbIt.hasNext());
  EXPECT(lsbIt.next() == 30);
  EXPECT(!lsbIt.hasNext());

  INFO("BLParametrizedBitOps<BL_BIT_ORDER_MSB>::BitIterator<uint32_t>");
  BLMSBBitOps<uint32_t>::BitIterator msbIt(0x40000010u);

  EXPECT(msbIt.hasNext());
  EXPECT(msbIt.next() == 1);
  EXPECT(msbIt.hasNext());
  EXPECT(msbIt.next() == 27);
  EXPECT(!msbIt.hasNext());
}

static void testBitVectorIterator() {
  static const uint32_t lsbBits[] = { 0x00000001u, 0x80000000u };
  static const uint32_t msbBits[] = { 0x00000001u, 0x80000000u };

  INFO("BLParametrizedBitOps<BL_BIT_ORDER_LSB>::BitVectorIterator<uint32_t>");
  BLLSBBitOps<uint32_t>::BitVectorIterator lsbIt(lsbBits, BL_ARRAY_SIZE(lsbBits));

  EXPECT(lsbIt.hasNext());
  EXPECT(lsbIt.next() == 0);
  EXPECT(lsbIt.hasNext());
  EXPECT(lsbIt.next() == 63);
  EXPECT(!lsbIt.hasNext());

  INFO("BLParametrizedBitOps<BL_BIT_ORDER_MSB>::BitVectorIterator<uint32_t>");
  BLMSBBitOps<uint32_t>::BitVectorIterator msbIt(msbBits, BL_ARRAY_SIZE(msbBits));

  EXPECT(msbIt.hasNext());
  EXPECT(msbIt.next() == 31);
  EXPECT(msbIt.hasNext());
  EXPECT(msbIt.next() == 32);
  EXPECT(!msbIt.hasNext());
}

static void testBitVectorFlipIterator() {
  static const uint32_t lsbBits[] = { 0xFFFFFFF0u, 0x00FFFFFFu };
  static const uint32_t msbBits[] = { 0x0FFFFFFFu, 0xFFFFFF00u };

  INFO("BLParametrizedBitOps<BL_BIT_ORDER_LSB>::BitVectorFlipIterator<uint32_t>");
  BLLSBBitOps<uint32_t>::BitVectorFlipIterator lsbIt(lsbBits, BL_ARRAY_SIZE(lsbBits));
  EXPECT(lsbIt.hasNext());
  EXPECT(lsbIt.nextAndFlip() == 4);
  EXPECT(lsbIt.hasNext());
  EXPECT(lsbIt.nextAndFlip() == 56);
  EXPECT(!lsbIt.hasNext());

  INFO("BLParametrizedBitOps<BL_BIT_ORDER_MSB>::BitVectorFlipIterator<uint32_t>");
  BLMSBBitOps<uint32_t>::BitVectorFlipIterator msbIt(msbBits, BL_ARRAY_SIZE(msbBits));
  EXPECT(msbIt.hasNext());
  EXPECT(msbIt.nextAndFlip() == 4);
  EXPECT(msbIt.hasNext());
  EXPECT(msbIt.nextAndFlip() == 56);
  EXPECT(!msbIt.hasNext());
}

UNIT(bitops, -9) {
  testBitArrayOps();
  testBitIterator();
  testBitVectorIterator();
  testBitVectorFlipIterator();
}
#endif
