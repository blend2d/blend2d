// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#ifdef BL_BUILD_OPT_AVX2

#include "./blpixelconverter_p.h"
#include "./blsimd_p.h"
#include "./blsupport_p.h"

// ============================================================================
// [BLPixelConverter - PRGB32 <- XRGB32 (AVX2)]
// ============================================================================

static BLResult BL_CDECL bl_convert_prgb32_from_xrgb32_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  using namespace SIMD;
  if (!options)
    options = &blPixelConverterDefaultOptions;

  const BLPixelConverterData::NativeFromExternal& d = blPixelConverterGetData(self)->nativeFromExternal;
  size_t gap = options->gap;

  dstStride -= (w * 4) + gap;
  srcStride -= (w * 4);

  I256 fillMask = vseti256i32(d.fillMask);
  I256 predicate = vdupli128(vloadi128u(d.simdData));

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while (i >= 32) {
      I256 p0, p1, p2, p3;

      p0 = vloadi256u(srcData +  0);
      p1 = vloadi256u(srcData + 32);
      p2 = vloadi256u(srcData + 64);
      p3 = vloadi256u(srcData + 96);

      p0 = vpshufb(p0, predicate);
      p1 = vpshufb(p1, predicate);
      p2 = vpshufb(p2, predicate);
      p3 = vpshufb(p3, predicate);

      p0 = vor(p0, fillMask);
      p1 = vor(p1, fillMask);
      p2 = vor(p2, fillMask);
      p3 = vor(p3, fillMask);

      vstorei256u(dstData +  0, p0);
      vstorei256u(dstData + 32, p1);
      vstorei256u(dstData + 64, p2);
      vstorei256u(dstData + 96, p3);

      dstData += 128;
      srcData += 128;
      i -= 32;
    }

    while (i >= 8) {
      I256 p0;

      p0 = vloadi256u(srcData);
      p0 = vpshufb(p0, predicate);
      p0 = vor(p0, fillMask);
      vstorei256u(dstData, p0);

      dstData += 32;
      srcData += 32;
      i -= 8;
    }

    while (i) {
      I128 p0;

      p0 = vloadi128_32(srcData);
      p0 = vpshufb(p0, vcast<I128>(predicate));
      p0 = vor(p0, vcast<I128>(fillMask));
      vstorei32(dstData, p0);

      dstData += 4;
      srcData += 4;
      i--;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLPixelConverter - PRGB32 <- ARGB32 (AVX2)]
// ============================================================================

static BLResult BL_CDECL bl_convert_prgb32_from_argb32_avx2(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride, uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept {

  using namespace SIMD;
  if (!options)
    options = &blPixelConverterDefaultOptions;

  const BLPixelConverterData::NativeFromExternal& d = blPixelConverterGetData(self)->nativeFromExternal;
  size_t gap = options->gap;

  dstStride -= (w * 4) + gap;
  srcStride -= (w * 4);

  I256 a255 = vseti256i64(int64_t(0x00FF000000000000));
  I256 fillMask = vseti256i32(d.fillMask);
  I256 predicate = vdupli128(vloadi128u(d.simdData));

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while (i >= 16) {
      I256 p0, p1, p2, p3;

      p0 = vloadi256u(srcData +  0);
      p2 = vloadi256u(srcData + 32);

      I256 zero = vzeroi256();
      p0 = vpshufb(p0, predicate);
      p2 = vpshufb(p2, predicate);

      p1 = vunpackhi8(p0, zero);
      p0 = vunpackli8(p0, zero);
      p3 = vunpackhi8(p2, zero);
      p2 = vunpackli8(p2, zero);

      p0 = vmuli16(vor(p0, a255), vswizi16<3, 3, 3, 3>(p0));
      p1 = vmuli16(vor(p1, a255), vswizi16<3, 3, 3, 3>(p1));
      p2 = vmuli16(vor(p2, a255), vswizi16<3, 3, 3, 3>(p2));
      p3 = vmuli16(vor(p3, a255), vswizi16<3, 3, 3, 3>(p3));

      p0 = vdiv255u16(p0);
      p1 = vdiv255u16(p1);
      p2 = vdiv255u16(p2);
      p3 = vdiv255u16(p3);

      p0 = vpacki16u8(p0, p1);
      p2 = vpacki16u8(p2, p3);

      p0 = vor(p0, fillMask);
      p2 = vor(p2, fillMask);

      vstorei256u(dstData +  0, p0);
      vstorei256u(dstData + 32, p2);

      dstData += 64;
      srcData += 64;
      i -= 16;
    }

    while (i >= 4) {
      I128 p0, p1;
      I128 a0, a1;

      p0 = vloadi128u(srcData);
      I128 zero = vzeroi128();
      p0 = vpshufb(p0, vcast<I128>(predicate));

      p1 = vunpackhi8(p0, zero);
      p0 = vunpackli8(p0, zero);

      a1 = vswizi16<3, 3, 3, 3>(p1);
      p1 = vor(p1, vcast<I128>(a255));

      a0 = vswizi16<3, 3, 3, 3>(p0);
      p0 = vor(p0, vcast<I128>(a255));

      p1 = vdiv255u16(vmuli16(p1, a1));
      p0 = vdiv255u16(vmuli16(p0, a0));
      p0 = vpacki16u8(p0, p1);
      p0 = vor(p0, vcast<I128>(fillMask));
      vstorei128u(dstData, p0);

      dstData += 16;
      srcData += 16;
      i -= 4;
    }

    while (i) {
      I128 p0;
      I128 a0;

      p0 = vloadi128_32(srcData);
      I128 zero = vzeroi128();

      p0 = vpshufb(p0, vcast<I128>(predicate));
      p0 = vunpackli8(p0, zero);
      a0 = vswizi16<3, 3, 3, 3>(p0);
      p0 = vor(p0, vcast<I128>(a255));
      p0 = vdiv255u16(vmuli16(p0, a0));
      p0 = vpacki16u8(p0, p0);
      p0 = vor(p0, vcast<I128>(fillMask));
      vstorei32(dstData, p0);

      dstData += 4;
      srcData += 4;
      i--;
    }

    dstData = blPixelConverterFillGap(dstData, gap);
    dstData += dstStride;
    srcData += srcStride;
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLPixelConverter - Init (AVX2)]
// ============================================================================

static BL_INLINE uint32_t blPixelConverterMakePshufbPredicate32(const BLPixelConverterData::NativeFromExternal& d) noexcept {
  uint32_t rIndex = uint32_t(d.shifts[0]) >> 3;
  uint32_t gIndex = uint32_t(d.shifts[1]) >> 3;
  uint32_t bIndex = uint32_t(d.shifts[2]) >> 3;
  uint32_t aIndex = uint32_t(d.shifts[3]) >> 3;

  return (rIndex << 16) | (gIndex << 8) | (bIndex << 0) | (aIndex << 24);
}

bool blPixelConverterInitNativeFromXRGB_AVX2(BLPixelConverterCore* self, uint32_t dstFormat, const BLFormatInfo& srcInfo) noexcept {
  BLPixelConverterData::NativeFromExternal& d = blPixelConverterGetData(self)->nativeFromExternal;
  const BLFormatInfo& dstInfo = blPixelConverterFormatInfo[dstFormat];

  if (srcInfo.depth == 32) {
    // Only BYTE aligned components (8888 or X888 formats).
    if (!(srcInfo.flags & BL_FORMAT_FLAG_BYTE_ALIGNED))
      return false;

    bool isARGB = (srcInfo.flags & BL_FORMAT_FLAG_ALPHA) != 0;
    bool isPremultiplied = (srcInfo.flags & BL_FORMAT_FLAG_PREMULTIPLIED) != 0;

    switch (dstFormat) {
      case BL_FORMAT_XRGB32:
      case BL_FORMAT_PRGB32:
        d.simdData[0] = blPixelConverterMakePshufbPredicate32(d);
        d.simdData[1] = d.simdData[0] + 0x04040404u;
        d.simdData[2] = d.simdData[0] + 0x08080808u;
        d.simdData[3] = d.simdData[0] + 0x0C0C0C0Cu;

        self->convertFunc = (isARGB && !isPremultiplied) ? bl_convert_prgb32_from_argb32_avx2
                                                         : bl_convert_prgb32_from_xrgb32_avx2;
        return true;

      default:
        return false;
    }
  }

  return false;
}

#endif
