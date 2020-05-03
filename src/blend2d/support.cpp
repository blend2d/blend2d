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
#include "./support_p.h"

// ============================================================================
// [BLSupport - Unit Tests]
// ============================================================================

#ifdef BL_TEST
template<typename T>
BL_INLINE bool testIsConsecutiveBitMask(T x) {
  if (x == 0)
    return false;

  T m = x;
  while ((m & 0x1) == 0)
    m >>= 1;
  return ((m + 1) & m) == 0;
}

static void testAlignment() noexcept {
  INFO("blIsAligned()");
  EXPECT(blIsAligned<size_t>(0xFFFF,  4) == false);
  EXPECT(blIsAligned<size_t>(0xFFF4,  4) == true);
  EXPECT(blIsAligned<size_t>(0xFFF8,  8) == true);
  EXPECT(blIsAligned<size_t>(0xFFF0, 16) == true);

  INFO("blAlignUp()");
  EXPECT(blAlignUp<size_t>(0xFFFF,  4) == 0x10000);
  EXPECT(blAlignUp<size_t>(0xFFF4,  4) == 0x0FFF4);
  EXPECT(blAlignUp<size_t>(0xFFF8,  8) == 0x0FFF8);
  EXPECT(blAlignUp<size_t>(0xFFF0, 16) == 0x0FFF0);
  EXPECT(blAlignUp<size_t>(0xFFF0, 32) == 0x10000);

  INFO("blAlignUpDiff()");
  EXPECT(blAlignUpDiff<size_t>(0xFFFF,  4) == 1);
  EXPECT(blAlignUpDiff<size_t>(0xFFF4,  4) == 0);
  EXPECT(blAlignUpDiff<size_t>(0xFFF8,  8) == 0);
  EXPECT(blAlignUpDiff<size_t>(0xFFF0, 16) == 0);
  EXPECT(blAlignUpDiff<size_t>(0xFFF0, 32) == 16);

  INFO("blAlignUpPowerOf2()");
  EXPECT(blAlignUpPowerOf2<size_t>(0x0000) == 0x00000);
  EXPECT(blAlignUpPowerOf2<size_t>(0xFFFF) == 0x10000);
  EXPECT(blAlignUpPowerOf2<size_t>(0xF123) == 0x10000);
  EXPECT(blAlignUpPowerOf2<size_t>(0x0F00) == 0x01000);
  EXPECT(blAlignUpPowerOf2<size_t>(0x0100) == 0x00100);
  EXPECT(blAlignUpPowerOf2<size_t>(0x1001) == 0x02000);
}

static void testBitUtils() noexcept {
  uint32_t i;

  INFO("blBitShl() / blBitShr()");
  EXPECT(blBitShl<int32_t >(0x00001111 , 16) == 0x11110000 );
  EXPECT(blBitShl<uint32_t>(0x00001111 , 16) == 0x11110000 );
  EXPECT(blBitShr<int32_t >(0x11110000u, 16) == 0x00001111u);
  EXPECT(blBitShr<uint32_t>(0x11110000u, 16) == 0x00001111u);
  EXPECT(blBitSar<uint32_t>(0xFFFF0000u, 16) == 0xFFFFFFFFu);

  INFO("blBitRol() / blBitRor()");
  EXPECT(blBitRol<int32_t >(0x00100000 , 16) == 0x00000010 );
  EXPECT(blBitRol<uint32_t>(0x00100000u, 16) == 0x00000010u);
  EXPECT(blBitRor<int32_t >(0x00001000 , 16) == 0x10000000 );
  EXPECT(blBitRor<uint32_t>(0x00001000u, 16) == 0x10000000u);

  INFO("blBitClz()");
  EXPECT(blBitClz<uint32_t>(1u) == 31);
  EXPECT(blBitClz<uint32_t>(2u) == 30);
  EXPECT(blBitClz<uint32_t>(3u) == 30);
  EXPECT(blBitClz<uint32_t>(0x80000000u) == 0);
  EXPECT(blBitClz<uint32_t>(0x88888888u) == 0);
  EXPECT(blBitClz<uint32_t>(0x11111111u) == 3);
  EXPECT(blBitClz<uint32_t>(0x12345678u) == 3);
  EXPECT(blBitClzStatic<uint32_t>(1u) == 31);
  EXPECT(blBitClzStatic<uint32_t>(2u) == 30);
  EXPECT(blBitClzStatic<uint32_t>(3u) == 30);
  EXPECT(blBitClzStatic<uint32_t>(0x80000000u) == 0);
  EXPECT(blBitClzStatic<uint32_t>(0x88888888u) == 0);
  EXPECT(blBitClzStatic<uint32_t>(0x11111111u) == 3);
  EXPECT(blBitClzStatic<uint32_t>(0x12345678u) == 3);

  for (i = 0; i < 32; i++) {
    EXPECT(blBitClz(uint32_t(1) << i) == 31 - i);
    EXPECT(blBitClz(uint32_t(0xFFFFFFFFu) >> i) == i);
  }

  INFO("blBitCtz()");
  EXPECT(blBitCtz<uint32_t>(1u) == 0);
  EXPECT(blBitCtz<uint32_t>(2u) == 1);
  EXPECT(blBitCtz<uint32_t>(3u) == 0);
  EXPECT(blBitCtz<uint32_t>(0x80000000u) == 31);
  EXPECT(blBitCtz<uint32_t>(0x88888888u) == 3);
  EXPECT(blBitCtz<uint32_t>(0x11111111u) == 0);
  EXPECT(blBitCtz<uint32_t>(0x12345678u) == 3);
  EXPECT(blBitCtzStatic<uint32_t>(1u) == 0);
  EXPECT(blBitCtzStatic<uint32_t>(2u) == 1);
  EXPECT(blBitCtzStatic<uint32_t>(3u) == 0);
  EXPECT(blBitCtzStatic<uint32_t>(0x80000000u) == 31);
  EXPECT(blBitCtzStatic<uint32_t>(0x88888888u) == 3);
  EXPECT(blBitCtzStatic<uint32_t>(0x11111111u) == 0);
  EXPECT(blBitCtzStatic<uint32_t>(0x12345678u) == 3);

  for (i = 0; i < 32; i++) {
    EXPECT(blBitCtz(uint32_t(1) << i) == i);
    EXPECT(blBitCtz(uint32_t(0xFFFFFFFFu) << i) == i);
  }

  INFO("blIsPowerOf2()");
  for (i = 0; i < 64; i++) {
    EXPECT(blIsPowerOf2(uint64_t(1) << i) == true);
    EXPECT(blIsPowerOf2((uint64_t(1) << i) ^ 0x001101) == false);
  }

  INFO("blIsBitMaskConsecutive()");
  i = 0;
  for (;;) {
    bool result = blIsBitMaskConsecutive(i);
    bool expect = testIsConsecutiveBitMask(i);
    EXPECT(result == expect);

    if (i == 0xFFFFu)
      break;
    i++;
  }
}

static void testIntUtils() noexcept {
  uint32_t i;

  INFO("blByteSwap()");
  EXPECT(blByteSwap16(int16_t(0x0102)) == int16_t(0x0201));
  EXPECT(blByteSwap16(uint16_t(0x0102)) == uint16_t(0x0201));
  EXPECT(blByteSwap24(int32_t(0x00010203)) == int32_t(0x00030201));
  EXPECT(blByteSwap24(uint32_t(0x00010203)) == uint32_t(0x00030201));
  EXPECT(blByteSwap32(int32_t(0x01020304)) == int32_t(0x04030201));
  EXPECT(blByteSwap32(uint32_t(0x01020304)) == uint32_t(0x04030201));
  EXPECT(blByteSwap64(uint64_t(0x0102030405060708)) == uint64_t(0x0807060504030201));

  INFO("blClamp()");
  EXPECT(blClamp<int>(-1, 100, 1000) == 100);
  EXPECT(blClamp<int>(99, 100, 1000) == 100);
  EXPECT(blClamp<int>(1044, 100, 1000) == 1000);
  EXPECT(blClamp<double>(-1.0, 100.0, 1000.0) == 100.0);
  EXPECT(blClamp<double>(99.0, 100.0, 1000.0) == 100.0);
  EXPECT(blClamp<double>(1044.0, 100.0, 1000.0) == 1000.0);

  INFO("blClampToByte()");
  EXPECT(blClampToByte(-1) == 0);
  EXPECT(blClampToByte(42) == 42);
  EXPECT(blClampToByte(255) == 0xFF);
  EXPECT(blClampToByte(256) == 0xFF);
  EXPECT(blClampToByte(0x7FFFFFFF) == 0xFF);
  EXPECT(blClampToByte(0x7FFFFFFFu) == 0xFF);
  EXPECT(blClampToByte(0xFFFFFFFFu) == 0xFF);

  INFO("blClampToWord()");
  EXPECT(blClampToWord(-1) == 0);
  EXPECT(blClampToWord(42) == 42);
  EXPECT(blClampToWord(0xFFFF) == 0xFFFF);
  EXPECT(blClampToWord(0x10000) == 0xFFFF);
  EXPECT(blClampToWord(0x10000u) == 0xFFFF);
  EXPECT(blClampToWord(0x7FFFFFFF) == 0xFFFF);
  EXPECT(blClampToWord(0x7FFFFFFFu) == 0xFFFF);
  EXPECT(blClampToWord(0xFFFFFFFFu) == 0xFFFF);

  INFO("blUdiv255()");
  for (i = 0; i < 255 * 255; i++) {
    uint32_t result = blUdiv255(i);
    uint32_t j = i + 128;

    // This version doesn't overflow 16 bits.
    uint32_t expected = (j + (j >> 8)) >> 8;

    EXPECT(result == expected, "blUdiv255(%u) -> %u (Expected %u)", i, result, expected);
  }
}

static void testSafeArith() noexcept {
  INFO("blAddOverflow()");
  BLOverflowFlag of = 0;

  EXPECT(blAddOverflow<int32_t>(0, 0, &of) == 0 && !of);
  EXPECT(blAddOverflow<int32_t>(0, 1, &of) == 1 && !of);
  EXPECT(blAddOverflow<int32_t>(1, 0, &of) == 1 && !of);

  EXPECT(blAddOverflow<int32_t>(2147483647, 0, &of) == 2147483647 && !of);
  EXPECT(blAddOverflow<int32_t>(0, 2147483647, &of) == 2147483647 && !of);
  EXPECT(blAddOverflow<int32_t>(2147483647, -1, &of) == 2147483646 && !of);
  EXPECT(blAddOverflow<int32_t>(-1, 2147483647, &of) == 2147483646 && !of);

  EXPECT(blAddOverflow<int32_t>(-2147483647, 0, &of) == -2147483647 && !of);
  EXPECT(blAddOverflow<int32_t>(0, -2147483647, &of) == -2147483647 && !of);
  EXPECT(blAddOverflow<int32_t>(-2147483647, -1, &of) == -2147483647 - 1 && !of);
  EXPECT(blAddOverflow<int32_t>(-1, -2147483647, &of) == -2147483647 - 1 && !of);

  (void)blAddOverflow<int32_t>(2147483647, 1, &of); EXPECT(of); of = 0;
  (void)blAddOverflow<int32_t>(1, 2147483647, &of); EXPECT(of); of = 0;
  (void)blAddOverflow<int32_t>(-2147483647, -2, &of); EXPECT(of); of = 0;
  (void)blAddOverflow<int32_t>(-2, -2147483647, &of); EXPECT(of); of = 0;

  EXPECT(blAddOverflow<uint32_t>(0u, 0u, &of) == 0 && !of);
  EXPECT(blAddOverflow<uint32_t>(0u, 1u, &of) == 1 && !of);
  EXPECT(blAddOverflow<uint32_t>(1u, 0u, &of) == 1 && !of);

  EXPECT(blAddOverflow<uint32_t>(2147483647u, 1u, &of) == 2147483648u && !of);
  EXPECT(blAddOverflow<uint32_t>(1u, 2147483647u, &of) == 2147483648u && !of);
  EXPECT(blAddOverflow<uint32_t>(0xFFFFFFFFu, 0u, &of) == 0xFFFFFFFFu && !of);
  EXPECT(blAddOverflow<uint32_t>(0u, 0xFFFFFFFFu, &of) == 0xFFFFFFFFu && !of);

  (void)blAddOverflow<uint32_t>(0xFFFFFFFFu, 1u, &of); EXPECT(of); of = 0;
  (void)blAddOverflow<uint32_t>(1u, 0xFFFFFFFFu, &of); EXPECT(of); of = 0;

  (void)blAddOverflow<uint32_t>(0x80000000u, 0xFFFFFFFFu, &of); EXPECT(of); of = 0;
  (void)blAddOverflow<uint32_t>(0xFFFFFFFFu, 0x80000000u, &of); EXPECT(of); of = 0;
  (void)blAddOverflow<uint32_t>(0xFFFFFFFFu, 0xFFFFFFFFu, &of); EXPECT(of); of = 0;

  INFO("blSubOverflow()");
  EXPECT(blSubOverflow<int32_t>(0, 0, &of) ==  0 && !of);
  EXPECT(blSubOverflow<int32_t>(0, 1, &of) == -1 && !of);
  EXPECT(blSubOverflow<int32_t>(1, 0, &of) ==  1 && !of);
  EXPECT(blSubOverflow<int32_t>(0, -1, &of) ==  1 && !of);
  EXPECT(blSubOverflow<int32_t>(-1, 0, &of) == -1 && !of);

  EXPECT(blSubOverflow<int32_t>(2147483647, 1, &of) == 2147483646 && !of);
  EXPECT(blSubOverflow<int32_t>(2147483647, 2147483647, &of) == 0 && !of);
  EXPECT(blSubOverflow<int32_t>(-2147483647, 1, &of) == -2147483647 - 1 && !of);
  EXPECT(blSubOverflow<int32_t>(-2147483647, -1, &of) == -2147483646 && !of);
  EXPECT(blSubOverflow<int32_t>(-2147483647 - 0, -2147483647 - 0, &of) == 0 && !of);
  EXPECT(blSubOverflow<int32_t>(-2147483647 - 1, -2147483647 - 1, &of) == 0 && !of);

  (void)blSubOverflow<int32_t>(-2, 2147483647, &of); EXPECT(of); of = 0;
  (void)blSubOverflow<int32_t>(-2147483647, 2, &of); EXPECT(of); of = 0;

  (void)blSubOverflow<int32_t>(-2147483647    , 2147483647, &of); EXPECT(of); of = 0;
  (void)blSubOverflow<int32_t>(-2147483647 - 1, 2147483647, &of); EXPECT(of); of = 0;

  (void)blSubOverflow<int32_t>(2147483647, -2147483647 - 0, &of); EXPECT(of); of = 0;
  (void)blSubOverflow<int32_t>(2147483647, -2147483647 - 1, &of); EXPECT(of); of = 0;

  EXPECT(blSubOverflow<uint32_t>(0, 0, &of) ==  0 && !of);
  EXPECT(blSubOverflow<uint32_t>(1, 0, &of) ==  1 && !of);

  EXPECT(blSubOverflow<uint32_t>(0xFFFFFFFFu, 0u, &of) == 0xFFFFFFFFu && !of);
  EXPECT(blSubOverflow<uint32_t>(0xFFFFFFFFu, 0xFFFFFFFFu, &of) == 0u && !of);

  (void)blSubOverflow<uint32_t>(0u, 1u, &of); EXPECT(of); of = 0;
  (void)blSubOverflow<uint32_t>(1u, 2u, &of); EXPECT(of); of = 0;

  (void)blSubOverflow<uint32_t>(0u, 0xFFFFFFFFu, &of); EXPECT(of); of = 0;
  (void)blSubOverflow<uint32_t>(1u, 0xFFFFFFFFu, &of); EXPECT(of); of = 0;

  (void)blSubOverflow<uint32_t>(0u, 0x7FFFFFFFu, &of); EXPECT(of); of = 0;
  (void)blSubOverflow<uint32_t>(1u, 0x7FFFFFFFu, &of); EXPECT(of); of = 0;

  (void)blSubOverflow<uint32_t>(0x7FFFFFFEu, 0x7FFFFFFFu, &of); EXPECT(of); of = 0;
  (void)blSubOverflow<uint32_t>(0xFFFFFFFEu, 0xFFFFFFFFu, &of); EXPECT(of); of = 0;

  INFO("blMulOverflow()");
  EXPECT(blMulOverflow<int32_t>(0, 0, &of) == 0 && !of);
  EXPECT(blMulOverflow<int32_t>(0, 1, &of) == 0 && !of);
  EXPECT(blMulOverflow<int32_t>(1, 0, &of) == 0 && !of);

  EXPECT(blMulOverflow<int32_t>( 1,  1, &of) ==  1 && !of);
  EXPECT(blMulOverflow<int32_t>( 1, -1, &of) == -1 && !of);
  EXPECT(blMulOverflow<int32_t>(-1,  1, &of) == -1 && !of);
  EXPECT(blMulOverflow<int32_t>(-1, -1, &of) ==  1 && !of);

  EXPECT(blMulOverflow<int32_t>( 32768,  65535, &of) ==  2147450880 && !of);
  EXPECT(blMulOverflow<int32_t>( 32768, -65535, &of) == -2147450880 && !of);
  EXPECT(blMulOverflow<int32_t>(-32768,  65535, &of) == -2147450880 && !of);
  EXPECT(blMulOverflow<int32_t>(-32768, -65535, &of) ==  2147450880 && !of);

  EXPECT(blMulOverflow<int32_t>(2147483647, 1, &of) == 2147483647 && !of);
  EXPECT(blMulOverflow<int32_t>(1, 2147483647, &of) == 2147483647 && !of);

  EXPECT(blMulOverflow<int32_t>(-2147483647 - 1, 1, &of) == -2147483647 - 1 && !of);
  EXPECT(blMulOverflow<int32_t>(1, -2147483647 - 1, &of) == -2147483647 - 1 && !of);

  (void)blMulOverflow<int32_t>( 65535,  65535, &of); EXPECT(of); of = 0;
  (void)blMulOverflow<int32_t>( 65535, -65535, &of); EXPECT(of); of = 0;
  (void)blMulOverflow<int32_t>(-65535,  65535, &of); EXPECT(of); of = 0;
  (void)blMulOverflow<int32_t>(-65535, -65535, &of); EXPECT(of); of = 0;

  (void)blMulOverflow<int32_t>( 2147483647    ,  2147483647    , &of); EXPECT(of); of = 0;
  (void)blMulOverflow<int32_t>( 2147483647    , -2147483647 - 1, &of); EXPECT(of); of = 0;
  (void)blMulOverflow<int32_t>(-2147483647 - 1,  2147483647    , &of); EXPECT(of); of = 0;
  (void)blMulOverflow<int32_t>(-2147483647 - 1, -2147483647 - 1, &of); EXPECT(of); of = 0;

  EXPECT(blMulOverflow<uint32_t>(0, 0, &of) == 0 && !of);
  EXPECT(blMulOverflow<uint32_t>(0, 1, &of) == 0 && !of);
  EXPECT(blMulOverflow<uint32_t>(1, 0, &of) == 0 && !of);
  EXPECT(blMulOverflow<uint32_t>(1, 1, &of) == 1 && !of);

  EXPECT(blMulOverflow<uint32_t>(0x10000000u, 15, &of) == 0xF0000000u && !of);
  EXPECT(blMulOverflow<uint32_t>(15, 0x10000000u, &of) == 0xF0000000u && !of);

  EXPECT(blMulOverflow<uint32_t>(0xFFFFFFFFu, 1, &of) == 0xFFFFFFFFu && !of);
  EXPECT(blMulOverflow<uint32_t>(1, 0xFFFFFFFFu, &of) == 0xFFFFFFFFu && !of);

  (void)blMulOverflow<uint32_t>(0xFFFFFFFFu, 2, &of); EXPECT(of); of = 0;
  (void)blMulOverflow<uint32_t>(2, 0xFFFFFFFFu, &of); EXPECT(of); of = 0;

  (void)blMulOverflow<uint32_t>(0x80000000u, 2, &of); EXPECT(of); of = 0;
  (void)blMulOverflow<uint32_t>(2, 0x80000000u, &of); EXPECT(of); of = 0;

  EXPECT(blMulOverflow<int64_t>(0, 0, &of) == 0 && !of);
  EXPECT(blMulOverflow<int64_t>(0, 1, &of) == 0 && !of);
  EXPECT(blMulOverflow<int64_t>(1, 0, &of) == 0 && !of);

  EXPECT(blMulOverflow<int64_t>( 1,  1, &of) ==  1 && !of);
  EXPECT(blMulOverflow<int64_t>( 1, -1, &of) == -1 && !of);
  EXPECT(blMulOverflow<int64_t>(-1,  1, &of) == -1 && !of);
  EXPECT(blMulOverflow<int64_t>(-1, -1, &of) ==  1 && !of);

  EXPECT(blMulOverflow<int64_t>( 32768,  65535, &of) ==  2147450880 && !of);
  EXPECT(blMulOverflow<int64_t>( 32768, -65535, &of) == -2147450880 && !of);
  EXPECT(blMulOverflow<int64_t>(-32768,  65535, &of) == -2147450880 && !of);
  EXPECT(blMulOverflow<int64_t>(-32768, -65535, &of) ==  2147450880 && !of);

  EXPECT(blMulOverflow<int64_t>(2147483647, 1, &of) == 2147483647 && !of);
  EXPECT(blMulOverflow<int64_t>(1, 2147483647, &of) == 2147483647 && !of);

  EXPECT(blMulOverflow<int64_t>(-2147483647 - 1, 1, &of) == -2147483647 - 1 && !of);
  EXPECT(blMulOverflow<int64_t>(1, -2147483647 - 1, &of) == -2147483647 - 1 && !of);

  EXPECT(blMulOverflow<int64_t>( 65535,  65535, &of) ==  int64_t(4294836225) && !of);
  EXPECT(blMulOverflow<int64_t>( 65535, -65535, &of) == -int64_t(4294836225) && !of);
  EXPECT(blMulOverflow<int64_t>(-65535,  65535, &of) == -int64_t(4294836225) && !of);
  EXPECT(blMulOverflow<int64_t>(-65535, -65535, &of) ==  int64_t(4294836225) && !of);

  EXPECT(blMulOverflow<int64_t>( 2147483647    ,  2147483647    , &of) ==  int64_t(4611686014132420609) && !of);
  EXPECT(blMulOverflow<int64_t>( 2147483647    , -2147483647 - 1, &of) == -int64_t(4611686016279904256) && !of);
  EXPECT(blMulOverflow<int64_t>(-2147483647 - 1,  2147483647    , &of) == -int64_t(4611686016279904256) && !of);
  EXPECT(blMulOverflow<int64_t>(-2147483647 - 1, -2147483647 - 1, &of) ==  int64_t(4611686018427387904) && !of);

  EXPECT(blMulOverflow<int64_t>(int64_t(0x7FFFFFFFFFFFFFFF), int64_t(1), &of) == int64_t(0x7FFFFFFFFFFFFFFF) && !of);
  EXPECT(blMulOverflow<int64_t>(int64_t(1), int64_t(0x7FFFFFFFFFFFFFFF), &of) == int64_t(0x7FFFFFFFFFFFFFFF) && !of);

  (void)blMulOverflow<int64_t>(int64_t(0x7FFFFFFFFFFFFFFF), int64_t(2), &of); EXPECT(of); of = 0;
  (void)blMulOverflow<int64_t>(int64_t(2), int64_t(0x7FFFFFFFFFFFFFFF), &of); EXPECT(of); of = 0;

  (void)blMulOverflow<int64_t>(int64_t( 0x7FFFFFFFFFFFFFFF), int64_t( 0x7FFFFFFFFFFFFFFF), &of); EXPECT(of); of = 0;
  (void)blMulOverflow<int64_t>(int64_t( 0x7FFFFFFFFFFFFFFF), int64_t(-0x7FFFFFFFFFFFFFFF), &of); EXPECT(of); of = 0;
  (void)blMulOverflow<int64_t>(int64_t(-0x7FFFFFFFFFFFFFFF), int64_t( 0x7FFFFFFFFFFFFFFF), &of); EXPECT(of); of = 0;
  (void)blMulOverflow<int64_t>(int64_t(-0x7FFFFFFFFFFFFFFF), int64_t(-0x7FFFFFFFFFFFFFFF), &of); EXPECT(of); of = 0;

  EXPECT(blMulOverflow<uint64_t>(0, 0, &of) == 0 && !of);
  EXPECT(blMulOverflow<uint64_t>(0, 1, &of) == 0 && !of);
  EXPECT(blMulOverflow<uint64_t>(1, 0, &of) == 0 && !of);
  EXPECT(blMulOverflow<uint64_t>(1, 1, &of) == 1 && !of);

  EXPECT(blMulOverflow<uint64_t>(0x1000000000000000u, 15, &of) == 0xF000000000000000u && !of);
  EXPECT(blMulOverflow<uint64_t>(15, 0x1000000000000000u, &of) == 0xF000000000000000u && !of);

  EXPECT(blMulOverflow<uint64_t>(0xFFFFFFFFFFFFFFFFu, 1, &of) == 0xFFFFFFFFFFFFFFFFu && !of);
  EXPECT(blMulOverflow<uint64_t>(1, 0xFFFFFFFFFFFFFFFFu, &of) == 0xFFFFFFFFFFFFFFFFu && !of);

  (void)blMulOverflow<uint64_t>(0xFFFFFFFFFFFFFFFFu, 2, &of); EXPECT(of); of = 0;
  (void)blMulOverflow<uint64_t>(2, 0xFFFFFFFFFFFFFFFFu, &of); EXPECT(of); of = 0;

  (void)blMulOverflow<uint64_t>(0x8000000000000000u, 2, &of); EXPECT(of); of = 0;
  (void)blMulOverflow<uint64_t>(2, 0x8000000000000000u, &of); EXPECT(of); of = 0;
}

static void testReadWrite() noexcept {
  INFO("blMemRead() / blMemWrite()");
  {
    uint8_t arr[32] = { 0 };

    blMemWriteU16uBE(arr + 1, 0x0102u);
    blMemWriteU16uBE(arr + 3, 0x0304u);
    EXPECT(blMemReadU32uBE(arr + 1) == 0x01020304u);
    EXPECT(blMemReadU32uLE(arr + 1) == 0x04030201u);
    EXPECT(blMemReadU32uBE(arr + 2) == 0x02030400u);
    EXPECT(blMemReadU32uLE(arr + 2) == 0x00040302u);

    blMemWriteU32uLE(arr + 5, 0x05060708u);
    EXPECT(blMemReadU64uBE(arr + 1) == 0x0102030408070605u);
    EXPECT(blMemReadU64uLE(arr + 1) == 0x0506070804030201u);

    blMemWriteU64uLE(arr + 7, 0x1122334455667788u);
    EXPECT(blMemReadU32uBE(arr + 8) == 0x77665544u);
  }
}

UNIT(support, -10) {
  testAlignment();
  testBitUtils();
  testIntUtils();
  testSafeArith();
  testReadWrite();
}
#endif
