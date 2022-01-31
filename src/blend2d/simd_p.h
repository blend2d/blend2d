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

namespace SIMD {

template<typename W, size_t N>
struct VecArray {
  typedef typename W::Type Type;
  Type data[N];

  BL_INLINE Type& operator[](size_t index) noexcept { return data[index]; }
  BL_INLINE const Type& operator[](size_t index) const noexcept { return data[index]; }
};

#if BL_TARGET_SIMD_I >= 128
struct VecWrap128I { typedef Vec128I Type; };
typedef VecArray<VecWrap128I, 2> Vec128Ix2;
#endif

#if BL_TARGET_SIMD_I >= 256
struct VecWrap256I { typedef Vec256I Type; };
typedef VecArray<VecWrap256I, 2> Vec256Ix2;
#endif

#if BL_TARGET_SIMD_F >= 128
struct VecWrap128F { typedef Vec128F Type; };
typedef VecArray<VecWrap128F, 2> Vec128Fx2;
#endif

#if BL_TARGET_SIMD_F >= 256
struct VecWrap256F { typedef Vec256F Type; };
typedef VecArray<VecWrap256F, 2> Vec256Fx2;
#endif

#if BL_TARGET_SIMD_D >= 128
struct VecWrap128D { typedef Vec128D Type; };
typedef VecArray<VecWrap128D, 2> Vec128Dx2;
#endif

#if BL_TARGET_SIMD_D >= 256
struct VecWrap256D { typedef Vec256D Type; };
typedef VecArray<VecWrap256D, 2> Vec256Dx2;
#endif

} // {SIMD}

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
