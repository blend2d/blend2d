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

#ifndef BLEND2D_SIMD_P_H_INCLUDED
#define BLEND2D_SIMD_P_H_INCLUDED

#include "./api-internal_p.h"

// ============================================================================
// [SIMD - Architecture]
// ============================================================================

#if BL_TARGET_ARCH_X86
  #include "./simd_x86_p.h"
#endif

#ifndef BL_TARGET_SIMD_I
  #define BL_TARGET_SIMD_I 0
#endif

#ifndef BL_TARGET_SIMD_F
  #define BL_TARGET_SIMD_F 0
#endif

#ifndef BL_TARGET_SIMD_D
  #define BL_TARGET_SIMD_D 0
#endif

// ============================================================================
// [SIMD - Loop Helpers]
// ============================================================================

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! Define a blit that processes 4 (32-bit) pixels at a time in main loop.
#define BL_SIMD_LOOP_32x4_INIT()                                              \
  size_t miniLoopCnt;                                                         \
  size_t mainLoopCnt;

#define BL_SIMD_LOOP_32x4_MINI_BEGIN(LOOP, DST, COUNT)                        \
  miniLoopCnt = blMin(size_t((uintptr_t(0) - ((uintptr_t)(DST) / 4)) & 0x3),  \
                      size_t(COUNT));                                         \
  mainLoopCnt = size_t(COUNT) - miniLoopCnt;                                  \
  if (!miniLoopCnt) goto On##LOOP##_MiniSkip;                                 \
                                                                              \
On##LOOP##_MiniBegin:                                                         \
  do {

#define BL_SIMD_LOOP_32x4_MINI_END(LOOP)                                      \
  } while (--miniLoopCnt);                                                    \
                                                                              \
On##LOOP##_MiniSkip:                                                          \
  miniLoopCnt = mainLoopCnt & 3;                                              \
  mainLoopCnt /= 4;                                                           \
  if (!mainLoopCnt) goto On##LOOP##_MainSkip;

#define BL_SIMD_LOOP_32x4_MAIN_BEGIN(LOOP)                                    \
  do {

#define BL_SIMD_LOOP_32x4_MAIN_END(LOOP)                                      \
  } while (--mainLoopCnt);                                                    \
                                                                              \
On##LOOP##_MainSkip:                                                          \
  if (miniLoopCnt) goto On##LOOP##_MiniBegin;

//! \}
//! \endcond

#endif // BLEND2D_SIMD_P_H_INCLUDED
