// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SIMD_SIMD_P_H_INCLUDED
#define BLEND2D_SIMD_SIMD_P_H_INCLUDED

#include "../api-internal_p.h"

// SIMD - Architecture
// ===================

#if BL_TARGET_ARCH_X86
  #include "../simd/simdx86_p.h"
#elif BL_TARGET_ARCH_ARM && (defined(__ARM_NEON) || defined(_M_ARM) || defined(_M_ARM64))
  #include "../simd/simdarm_p.h"
#else
  #define BL_TARGET_SIMD_I 0
  #define BL_TARGET_SIMD_F 0
  #define BL_TARGET_SIMD_D 0
#endif

#endif // BLEND2D_SIMD_SIMD_P_H_INCLUDED
