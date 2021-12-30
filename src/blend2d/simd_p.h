// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SIMD_P_H_INCLUDED
#define BLEND2D_SIMD_P_H_INCLUDED

#include "api-internal_p.h"

// SIMD - Architecture
// ===================

#if BL_TARGET_ARCH_X86
  #include "simd_x86_p.h"
#endif

#if BL_TARGET_ARCH_ARM
  #include "simd_neon_p.h"
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

// SIMD - Loop Construction
// ========================

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
