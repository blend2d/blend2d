// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#ifdef BL_TARGET_OPT_SSE2

#include "./blgeometry.h"
#include "./blmatrix.h"
#include "./blruntime_p.h"
#include "./blsimd_p.h"
#include "./blsupport_p.h"

// ============================================================================
// [BLMatrix2D - MapPointDArray [SSE2]]
// ============================================================================

static BLResult BL_CDECL blMatrix2DMapPointDArrayIdentity_SSE2(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  BL_UNUSED(self);
  if (dst == src)
    return BL_SUCCESS;

  size_t i = size;
  if (blIsAligned(((uintptr_t)dst | (uintptr_t)src), 16)) {
    while (i >= 4) {
      D128 s0 = vloadd128a(src + 0);
      D128 s1 = vloadd128a(src + 1);
      D128 s2 = vloadd128a(src + 2);
      D128 s3 = vloadd128a(src + 3);

      vstored128a(dst + 0, s0);
      vstored128a(dst + 1, s1);
      vstored128a(dst + 2, s2);
      vstored128a(dst + 3, s3);

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      vstored128a(dst, vloadd128a(src));
      i--;
      dst++;
      src++;
    }
  }
  else {
    while (i) {
      D128 sx = vloadd128_64(&src->x);
      D128 sy = vloadd128_64(&src->y);

      vstored64(&dst->x, sx);
      vstored64(&dst->y, sy);

      i--;
      dst++;
      src++;
    }
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArrayTranslate_SSE2(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  size_t i = size;
  D128 m20_m21 = vloadd128u(&self->m20);

  if (blIsAligned(((uintptr_t)dst | (uintptr_t)src), 16)) {
    while (i >= 4) {
      D128 s0 = vloadd128a(src + 0);
      D128 s1 = vloadd128a(src + 1);
      D128 s2 = vloadd128a(src + 2);
      D128 s3 = vloadd128a(src + 3);

      vstored128a(dst + 0, vaddpd(s0, m20_m21));
      vstored128a(dst + 1, vaddpd(s1, m20_m21));
      vstored128a(dst + 2, vaddpd(s2, m20_m21));
      vstored128a(dst + 3, vaddpd(s3, m20_m21));

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      D128 s0 = vloadd128a(src);
      vstored128a(dst, vaddpd(s0, m20_m21));

      i--;
      dst++;
      src++;
    }
  }
  else {
    while (i >= 4) {
      D128 s0 = vloadd128u(src + 0);
      D128 s1 = vloadd128u(src + 1);
      D128 s2 = vloadd128u(src + 2);
      D128 s3 = vloadd128u(src + 3);

      vstored128u(dst + 0, vaddpd(s0, m20_m21));
      vstored128u(dst + 1, vaddpd(s1, m20_m21));
      vstored128u(dst + 2, vaddpd(s2, m20_m21));
      vstored128u(dst + 3, vaddpd(s3, m20_m21));

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      D128 s0 = vloadd128u(src + 0);
      vstored128u(dst + 0, vaddpd(s0, m20_m21));

      i--;
      dst++;
      src++;
    }
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArrayScale_SSE2(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  size_t i = size;
  D128 m00_m11 = vsetd128(self->m11, self->m00);
  D128 m20_m21 = vloadd128u(&self->m20);

  if (blIsAligned(((uintptr_t)dst | (uintptr_t)src), 16)) {
    while (i >= 4) {
      D128 s0 = vloadd128a(src + 0);
      D128 s1 = vloadd128a(src + 1);
      D128 s2 = vloadd128a(src + 2);
      D128 s3 = vloadd128a(src + 3);

      vstored128a(dst + 0, vaddpd(vmulpd(s0, m00_m11), m20_m21));
      vstored128a(dst + 1, vaddpd(vmulpd(s1, m00_m11), m20_m21));
      vstored128a(dst + 2, vaddpd(vmulpd(s2, m00_m11), m20_m21));
      vstored128a(dst + 3, vaddpd(vmulpd(s3, m00_m11), m20_m21));

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      D128 s0 = vloadd128a(src);
      vstored128a(dst, vaddpd(vmulpd(s0, m00_m11), m20_m21));

      i--;
      dst++;
      src++;
    }
  }
  else {
    while (i >= 4) {
      D128 s0 = vloadd128u(src + 0);
      D128 s1 = vloadd128u(src + 1);
      D128 s2 = vloadd128u(src + 2);
      D128 s3 = vloadd128u(src + 3);

      vstored128u(dst + 0, vaddpd(vmulpd(s0, m00_m11), m20_m21));
      vstored128u(dst + 1, vaddpd(vmulpd(s1, m00_m11), m20_m21));
      vstored128u(dst + 2, vaddpd(vmulpd(s2, m00_m11), m20_m21));
      vstored128u(dst + 3, vaddpd(vmulpd(s3, m00_m11), m20_m21));

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      D128 s0 = vloadd128u(src);
      vstored128u(dst, vaddpd(vmulpd(s0, m00_m11), m20_m21));

      i--;
      dst++;
      src++;
    }
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArraySwap_SSE2(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  D128 m01_m10 = vsetd128(self->m01, self->m10);
  D128 m20_m21 = vloadd128u(&self->m20);

  size_t i = size;
  if (blIsAligned(((uintptr_t)dst | (uintptr_t)src), 16)) {
    while (i >= 4) {
      D128 s0 = vloadd128a(src + 0);
      D128 s1 = vloadd128a(src + 1);
      D128 s2 = vloadd128a(src + 2);
      D128 s3 = vloadd128a(src + 3);

      s0 = vmulpd(vswapd64(s0), m01_m10);
      s1 = vmulpd(vswapd64(s1), m01_m10);
      s2 = vmulpd(vswapd64(s2), m01_m10);
      s3 = vmulpd(vswapd64(s3), m01_m10);

      vstored128a(dst + 0, vaddpd(s0, m20_m21));
      vstored128a(dst + 1, vaddpd(s1, m20_m21));
      vstored128a(dst + 2, vaddpd(s2, m20_m21));
      vstored128a(dst + 3, vaddpd(s3, m20_m21));

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      D128 s0 = vloadd128a(src);
      s0 = vmulpd(vswapd64(s0), m01_m10);
      vstored128a(dst, vaddpd(s0, m20_m21));

      i--;
      dst++;
      src++;
    }
  }
  else {
    while (i >= 4) {
      D128 s0 = vloadd128u(src + 0);
      D128 s1 = vloadd128u(src + 1);
      D128 s2 = vloadd128u(src + 2);
      D128 s3 = vloadd128u(src + 3);

      s0 = vmulpd(vswapd64(s0), m01_m10);
      s1 = vmulpd(vswapd64(s1), m01_m10);
      s2 = vmulpd(vswapd64(s2), m01_m10);
      s3 = vmulpd(vswapd64(s3), m01_m10);

      vstored128u(dst + 0, vaddpd(s0, m20_m21));
      vstored128u(dst + 1, vaddpd(s1, m20_m21));
      vstored128u(dst + 2, vaddpd(s2, m20_m21));
      vstored128u(dst + 3, vaddpd(s3, m20_m21));

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      D128 s0 = vloadd128u(src);
      s0 = vmulpd(vswapd64(s0), m01_m10);
      vstored128u(dst, vaddpd(s0, m20_m21));

      i--;
      dst++;
      src++;
    }
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArrayAffine_SSE2(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  size_t i = size;
  D128 m00_m11 = vsetd128(self->m11, self->m00);
  D128 m10_m01 = vsetd128(self->m01, self->m10);
  D128 m20_m21 = vloadd128u(&self->m20);

  if (blIsAligned(((uintptr_t)dst | (uintptr_t)src), 16)) {
    while (i >= 4) {
      D128 s0 = vloadd128a(src + 0);
      D128 s1 = vloadd128a(src + 1);
      D128 s2 = vloadd128a(src + 2);
      D128 s3 = vloadd128a(src + 3);

      D128 r0 = vswapd64(s0);
      D128 r1 = vswapd64(s1);
      D128 r2 = vswapd64(s2);
      D128 r3 = vswapd64(s3);

      s0 = vmulpd(s0, m00_m11);
      s1 = vmulpd(s1, m00_m11);
      s2 = vmulpd(s2, m00_m11);
      s3 = vmulpd(s3, m00_m11);

      s0 = vaddpd(vaddpd(s0, m20_m21), vmulpd(r0, m10_m01));
      s1 = vaddpd(vaddpd(s1, m20_m21), vmulpd(r1, m10_m01));
      s2 = vaddpd(vaddpd(s2, m20_m21), vmulpd(r2, m10_m01));
      s3 = vaddpd(vaddpd(s3, m20_m21), vmulpd(r3, m10_m01));

      vstored128a(dst + 0, s0);
      vstored128a(dst + 1, s1);
      vstored128a(dst + 2, s2);
      vstored128a(dst + 3, s3);

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      D128 s0 = vloadd128a(src);
      D128 r0 = vswapd64(s0);

      s0 = vmulpd(s0, m00_m11);
      s0 = vaddpd(vaddpd(s0, m20_m21), vmulpd(r0, m10_m01));

      vstored128a(dst, s0);

      i--;
      dst++;
      src++;
    }
  }
  else {
    while (i >= 4) {
      D128 s0 = vloadd128u(src + 0);
      D128 s1 = vloadd128u(src + 1);
      D128 s2 = vloadd128u(src + 2);
      D128 s3 = vloadd128u(src + 3);

      D128 r0 = vswapd64(s0);
      D128 r1 = vswapd64(s1);
      D128 r2 = vswapd64(s2);
      D128 r3 = vswapd64(s3);

      s0 = vmulpd(s0, m00_m11);
      s1 = vmulpd(s1, m00_m11);
      s2 = vmulpd(s2, m00_m11);
      s3 = vmulpd(s3, m00_m11);

      s0 = vaddpd(vaddpd(s0, m20_m21), vmulpd(r0, m10_m01));
      s1 = vaddpd(vaddpd(s1, m20_m21), vmulpd(r1, m10_m01));
      s2 = vaddpd(vaddpd(s2, m20_m21), vmulpd(r2, m10_m01));
      s3 = vaddpd(vaddpd(s3, m20_m21), vmulpd(r3, m10_m01));

      vstored128u(dst + 0, s0);
      vstored128u(dst + 1, s1);
      vstored128u(dst + 2, s2);
      vstored128u(dst + 3, s3);

      i -= 4;
      dst += 4;
      src += 4;
    }

    while (i) {
      D128 s0 = vloadd128u(src);
      D128 r0 = vswapd64(s0);

      s0 = vmulpd(s0, m00_m11);
      s0 = vaddpd(vaddpd(s0, m20_m21), vmulpd(r0, m10_m01));

      vstored128u(dst, s0);

      i--;
      dst++;
      src++;
    }
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLMatrix2D - Runtime Init [SSE2]]
// ============================================================================

BL_HIDDEN void blMatrix2DRtInit_SSE2(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);
  BLMapPointDArrayFunc* funcs = blMatrix2DMapPointDArrayFuncs;

  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_IDENTITY ], blMatrix2DMapPointDArrayIdentity_SSE2);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_TRANSLATE], blMatrix2DMapPointDArrayTranslate_SSE2);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_SCALE    ], blMatrix2DMapPointDArrayScale_SSE2);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_SWAP     ], blMatrix2DMapPointDArraySwap_SSE2);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_AFFINE   ], blMatrix2DMapPointDArrayAffine_SSE2);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_INVALID  ], blMatrix2DMapPointDArrayAffine_SSE2);
}

#endif
