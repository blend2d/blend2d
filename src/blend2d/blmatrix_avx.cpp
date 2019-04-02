// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#ifdef BL_TARGET_OPT_AVX

#include "./blgeometry.h"
#include "./blmatrix.h"
#include "./blruntime_p.h"
#include "./blsimd_p.h"
#include "./blsupport_p.h"

// ============================================================================
// [BLMatrix2D - MapPointDArray [AVX]]
// ============================================================================

static BLResult BL_CDECL blMatrix2DMapPointDArrayIdentity_AVX(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  BL_UNUSED(self);
  if (dst == src)
    return BL_SUCCESS;

  size_t i = size;
  while (i >= 8) {
    vstored256u(dst + 0, vloadd256u(src + 0));
    vstored256u(dst + 2, vloadd256u(src + 2));
    vstored256u(dst + 4, vloadd256u(src + 4));
    vstored256u(dst + 6, vloadd256u(src + 6));

    i -= 8;
    dst += 8;
    src += 8;
  }

  while (i >= 2) {
    vstored256u(dst, vloadd256u(src));

    i -= 2;
    dst += 2;
    src += 2;
  }

  if (i)
    vstored128u(dst, vloadd128u(src));

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArrayTranslate_AVX(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  size_t i = size;
  D256 m20_m21 = vbroadcastd256_128(&self->m20);

  while (i >= 8) {
    vstored256u(dst + 0, vaddpd(vloadd256u(src + 0), m20_m21));
    vstored256u(dst + 2, vaddpd(vloadd256u(src + 2), m20_m21));
    vstored256u(dst + 4, vaddpd(vloadd256u(src + 4), m20_m21));
    vstored256u(dst + 6, vaddpd(vloadd256u(src + 6), m20_m21));

    i -= 8;
    dst += 8;
    src += 8;
  }

  while (i >= 2) {
    vstored256u(dst, vaddpd(vloadd256u(src), m20_m21));

    i -= 2;
    dst += 2;
    src += 2;
  }

  if (i)
    vstored128u(dst, vaddpd(vloadd128u(src), vcast<D128>(m20_m21)));

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArrayScale_AVX(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  size_t i = size;
  D256 m00_m11 = vdupld128(vsetd128(self->m11, self->m00));
  D256 m20_m21 = vbroadcastd256_128(&self->m20);

  while (i >= 8) {
    vstored256u(dst + 0, vaddpd(vmulpd(vloadd256u(src + 0), m00_m11), m20_m21));
    vstored256u(dst + 2, vaddpd(vmulpd(vloadd256u(src + 2), m00_m11), m20_m21));
    vstored256u(dst + 4, vaddpd(vmulpd(vloadd256u(src + 4), m00_m11), m20_m21));
    vstored256u(dst + 6, vaddpd(vmulpd(vloadd256u(src + 6), m00_m11), m20_m21));

    i -= 8;
    dst += 8;
    src += 8;
  }

  while (i >= 2) {
    vstored256u(dst, vaddpd(vmulpd(vloadd256u(src), m00_m11), m20_m21));

    i -= 2;
    dst += 2;
    src += 2;
  }

  if (i)
    vstored128u(dst, vaddpd(vmulpd(vloadd128u(src), vcast<D128>(m00_m11)), vcast<D128>(m20_m21)));

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArraySwap_AVX(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  size_t i = size;
  D256 m01_m10 = vdupld128(vsetd128(self->m01, self->m10));
  D256 m20_m21 = vbroadcastd256_128(&self->m20);

  while (i >= 8) {
    vstored256u(dst + 0, vaddpd(vmulpd(vswapd64(vloadd256u(src + 0)), m01_m10), m20_m21));
    vstored256u(dst + 2, vaddpd(vmulpd(vswapd64(vloadd256u(src + 2)), m01_m10), m20_m21));
    vstored256u(dst + 4, vaddpd(vmulpd(vswapd64(vloadd256u(src + 4)), m01_m10), m20_m21));
    vstored256u(dst + 6, vaddpd(vmulpd(vswapd64(vloadd256u(src + 6)), m01_m10), m20_m21));

    i -= 8;
    dst += 8;
    src += 8;
  }

  while (i >= 2) {
    vstored256u(dst, vaddpd(vmulpd(vswapd64(vloadd256u(src)), m01_m10), m20_m21));

    i -= 2;
    dst += 2;
    src += 2;
  }

  if (i)
    vstored128u(dst, vaddpd(vmulpd(vswapd64(vloadd128u(src)), vcast<D128>(m01_m10)), vcast<D128>(m20_m21)));

  return BL_SUCCESS;
}

static BLResult BL_CDECL blMatrix2DMapPointDArrayAffine_AVX(const BLMatrix2D* self, BLPoint* dst, const BLPoint* src, size_t size) noexcept {
  using namespace SIMD;

  size_t i = size;
  D256 m00_m11 = vdupld128(vsetd128(self->m11, self->m00));
  D256 m10_m01 = vdupld128(vsetd128(self->m01, self->m10));
  D256 m20_m21 = vbroadcastd256_128(&self->m20);

  while (i >= 8) {
    D256 s0 = vloadd256u(src + 0);
    D256 s1 = vloadd256u(src + 2);
    D256 s2 = vloadd256u(src + 4);
    D256 s3 = vloadd256u(src + 6);

    vstored256u(dst + 0, vaddpd(vaddpd(vmulpd(s0, m00_m11), m20_m21), vmulpd(vswapd64(s0), m10_m01)));
    vstored256u(dst + 2, vaddpd(vaddpd(vmulpd(s1, m00_m11), m20_m21), vmulpd(vswapd64(s1), m10_m01)));
    vstored256u(dst + 4, vaddpd(vaddpd(vmulpd(s2, m00_m11), m20_m21), vmulpd(vswapd64(s2), m10_m01)));
    vstored256u(dst + 6, vaddpd(vaddpd(vmulpd(s3, m00_m11), m20_m21), vmulpd(vswapd64(s3), m10_m01)));

    i -= 8;
    dst += 8;
    src += 8;
  }

  while (i >= 2) {
    D256 s0 = vloadd256u(src);
    vstored256u(dst, vaddpd(vaddpd(vmulpd(s0, m00_m11), m20_m21), vmulpd(vswapd64(s0), m10_m01)));

    i -= 2;
    dst += 2;
    src += 2;
  }

  if (i) {
    D128 s0 = vloadd128u(src);
    vstored128u(dst, vaddpd(vaddpd(vmulpd(s0, vcast<D128>(m00_m11)), vcast<D128>(m20_m21)), vmulpd(vswapd64(s0), vcast<D128>(m10_m01))));
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLMatrix2D - Runtime Init [AVX]]
// ============================================================================

BL_HIDDEN void blMatrix2DRtInit_AVX(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);
  BLMapPointDArrayFunc* funcs = blMatrix2DMapPointDArrayFuncs;

  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_IDENTITY ], blMatrix2DMapPointDArrayIdentity_AVX);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_TRANSLATE], blMatrix2DMapPointDArrayTranslate_AVX);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_SCALE    ], blMatrix2DMapPointDArrayScale_AVX);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_SWAP     ], blMatrix2DMapPointDArraySwap_AVX);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_AFFINE   ], blMatrix2DMapPointDArrayAffine_AVX);
  blAssignFunc(&funcs[BL_MATRIX2D_TYPE_INVALID  ], blMatrix2DMapPointDArrayAffine_AVX);
}

#endif
