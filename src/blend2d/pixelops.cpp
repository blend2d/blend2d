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
#include "./pixelops_p.h"
#include "./support_p.h"

// ============================================================================
// [BLPixelUtils - Unit Test]
// ============================================================================

#ifdef BL_TEST
static void test_pixelops_xrgb32_0888_from_xrgb16_0555() {
  INFO("Testing xrgb32_0888_from_xrgb16_0555");
  uint32_t c = 0;

  for (;;) {
    uint32_t r = ((c >> 10) & 0x1F) << 3;
    uint32_t g = ((c >>  5) & 0x1F) << 3;
    uint32_t b = ((c      ) & 0x1F) << 3;

    uint32_t result   = BLPixelOps::xrgb32_0888_from_xrgb16_0555(c);
    uint32_t expected = blRgba32Pack(r + (r >> 5), g + (g >> 5), b + (b >> 5), 0xFF);

    EXPECT(result == expected,
      "xrgb32_0888_from_xrgb16_0555() - %08X -> %08X (Expected %08X)", c, result, expected);

    if (c == 0xFFFF)
      break;
    c++;
  }
}

static void test_pixelops_xrgb32_0888_from_xrgb16_0565() {
  INFO("Testing xrgb32_0888_from_xrgb16_0565");
  uint32_t c = 0;

  for (;;) {
    uint32_t r = ((c >> 11) & 0x1F) << 3;
    uint32_t g = ((c >>  5) & 0x3F) << 2;
    uint32_t b = ((c      ) & 0x1F) << 3;

    uint32_t result   = BLPixelOps::xrgb32_0888_from_xrgb16_0565(c);
    uint32_t expected = blRgba32Pack(r + (r >> 5), g + (g >> 6), b + (b >> 5), 0xFF);

    EXPECT(result == expected,
      "xrgb32_0888_from_xrgb16_0555() - %08X -> %08X (Expected %08X)", c, result, expected);

    if (c == 0xFFFF)
      break;
    c++;
  }
}

static void test_pixelops_argb32_8888_from_argb16_4444() {
  INFO("Testing argb32_8888_from_argb16_4444");
  uint32_t c = 0;

  for (;;) {
    uint32_t a = ((c >> 12) & 0xF) * 0x11;
    uint32_t r = ((c >>  8) & 0xF) * 0x11;
    uint32_t g = ((c >>  4) & 0xF) * 0x11;
    uint32_t b = ((c      ) & 0xF) * 0x11;

    uint32_t result   = BLPixelOps::argb32_8888_from_argb16_4444(c);
    uint32_t expected = blRgba32Pack(r, g, b, a);

    EXPECT(result == expected,
      "argb32_8888_from_argb16_4444() - %08X -> %08X (Expected %08X)", c, result, expected);

    if (c == 0xFFFF)
      break;
    c++;
  }
}

static void test_pixelops_prgb32_8888_from_argb32_8888() {
  INFO("Testing prgb32_8888_from_argb32_8888");

  uint32_t i;
  uint32_t c = 0;

  for (i = 0; i < 10000000; i++) {
    uint32_t a = (c >> 24) & 0xFF;
    uint32_t r = (c >> 16) & 0xFF;
    uint32_t g = (c >>  8) & 0xFF;
    uint32_t b = (c      ) & 0xFF;

    uint32_t result = BLPixelOps::prgb32_8888_from_argb32_8888(c);
    uint32_t expected = blRgba32Pack(blUdiv255(r * a), blUdiv255(g * a), blUdiv255(b * a), a);

    EXPECT(result == expected,
      "prgb32_8888_from_argb32_8888() - %08X -> %08X (Expected %08X)", c, result, expected);

    c += 7919;
  }
}

UNIT(pixelops, -9) {
  test_pixelops_xrgb32_0888_from_xrgb16_0555();
  test_pixelops_xrgb32_0888_from_xrgb16_0565();
  test_pixelops_argb32_8888_from_argb16_4444();
  test_pixelops_prgb32_8888_from_argb32_8888();
}
#endif
