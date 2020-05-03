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
#include "./rgba_p.h"

// ============================================================================
// [BLRgba - Unit Tests]
// ============================================================================

#ifdef BL_TEST
UNIT(rgba, -7) {
  BLRgba32 c32(0x01, 0x02, 0x03, 0xFF);
  BLRgba64 c64(0x100, 0x200, 0x300, 0xFFFF);

  EXPECT(c32.value == 0xFF010203u);
  EXPECT(c64.value == 0xFFFF010002000300u);

  EXPECT(BLRgba64(c32).value == 0xFFFF010102020303u);
  EXPECT(BLRgba32(c64).value == 0xFF010203u);
}
#endif
