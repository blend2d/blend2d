// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLSIMD_P_H
#define BLEND2D_BLSIMD_P_H

#include "./blapi-internal_p.h"

// ============================================================================
// [SIMD - Architecture]
// ============================================================================

#if BL_TARGET_ARCH_X86
  #include "./blsimd_x86_p.h"
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
  if (miniLoopCnt) goto On##LOOP##_MiniBegin;                                 \
                                                                              \
On##LOOP##_End:                                                               \
  (void)0;

//! \}
//! \endcond

#endif // BLEND2D_BLSIMD_P_H
