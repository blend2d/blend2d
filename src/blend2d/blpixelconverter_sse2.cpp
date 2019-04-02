// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#ifdef BL_BUILD_OPT_SSE2

#include "./blpixelconverter_p.h"
#include "./blsimd_p.h"
#include "./blsupport_p.h"

// ============================================================================
// [BLPixelConverter - PRGB32 <- XRGB32 (SSE2)]
// ============================================================================

static BLResult BL_CDECL bl_convert_prgb32_from_xrgb32_sse2(
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

  I128 fillMask = vseti128i32(d.fillMask);

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while (i >= 16) {
      I128 p0, p1, p2, p3;

      p0 = vloadi128u(srcData +  0);
      p1 = vloadi128u(srcData + 16);
      p2 = vloadi128u(srcData + 32);
      p3 = vloadi128u(srcData + 48);

      p0 = vor(p0, fillMask);
      p1 = vor(p1, fillMask);
      p2 = vor(p2, fillMask);
      p3 = vor(p3, fillMask);

      vstorei128u(dstData +  0, p0);
      vstorei128u(dstData + 16, p1);
      vstorei128u(dstData + 32, p2);
      vstorei128u(dstData + 48, p3);

      dstData += 64;
      srcData += 64;
      i -= 16;
    }

    while (i >= 4) {
      I128 p0;

      p0 = vloadi128u(srcData);
      p0 = vor(p0, fillMask);
      vstorei128u(dstData, p0);

      dstData += 16;
      srcData += 16;
      i -= 4;
    }

    while (i) {
      I128 p0;

      p0 = vloadi128_32(srcData);
      p0 = vor(p0, fillMask);
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
// [BLPixelConverter - PRGB32 <- ARGB32 (SSE2)]
// ============================================================================

static BLResult BL_CDECL bl_convert_prgb32_from_argb32_sse2(
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

  I128 zero = vzeroi128();
  I128 a255 = vseti128i64(int64_t(0x00FF000000000000));
  I128 fillMask = vseti128i32(d.fillMask);

  for (uint32_t y = h; y != 0; y--) {
    uint32_t i = w;

    while (i >= 4) {
      I128 p0, p1;
      I128 a0, a1;

      p0 = vloadi128u(srcData);

      p1 = vunpackhi8(p0, zero);
      p0 = vunpackli8(p0, zero);

      a1 = vswizi16<3, 3, 3, 3>(p1);
      p1 = vor(p1, a255);

      a0 = vswizi16<3, 3, 3, 3>(p0);
      p0 = vor(p0, a255);

      p1 = vdiv255u16(vmuli16(p1, a1));
      p0 = vdiv255u16(vmuli16(p0, a0));
      p0 = vpacki16u8(p0, p1);
      p0 = vor(p0, fillMask);
      vstorei128u(dstData, p0);

      dstData += 16;
      srcData += 16;
      i -= 4;
    }

    while (i) {
      I128 p0;
      I128 a0;

      p0 = vloadi128_32(srcData);
      p0 = vunpackli8(p0, zero);
      a0 = vswizi16<3, 3, 3, 3>(p0);
      p0 = vor(p0, a255);
      p0 = vdiv255u16(vmuli16(p0, a0));
      p0 = vpacki16u8(p0, p0);
      p0 = vor(p0, fillMask);
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
// [BLPixelConverter - Init (SSE2)]
// ============================================================================

bool blPixelConverterInitNativeFromXRGB_SSE2(BLPixelConverterCore* self, uint32_t dstFormat, const BLFormatInfo& srcInfo) noexcept {
  BLPixelConverterData::NativeFromExternal& d = blPixelConverterGetData(self)->nativeFromExternal;
  const BLFormatInfo& dstInfo = blPixelConverterFormatInfo[dstFormat];

  if (srcInfo.depth == 32) {
    // Only BYTE aligned components (8888 or X888 formats).
    if (!(srcInfo.flags & BL_FORMAT_FLAG_BYTE_ALIGNED))
      return false;

    // Only PRGB32, ARGB32, or XRGB32 formats. See SSSE3 implementation, which
    // uses PSHUFB instruction and implements optimized conversion between all
    // possible byte-aligned formats.
    if (d.shifts[1] != 16 || d.shifts[2] != 8 || d.shifts[3] != 0)
      return false;

    bool isARGB = d.shifts[0] == 24;
    bool isPremultiplied = (srcInfo.flags & BL_FORMAT_FLAG_PREMULTIPLIED) != 0;

    switch (dstFormat) {
      case BL_FORMAT_XRGB32:
      case BL_FORMAT_PRGB32:
        self->convertFunc = (isARGB && !isPremultiplied) ? bl_convert_prgb32_from_argb32_sse2
                                                         : bl_convert_prgb32_from_xrgb32_sse2;
        return true;

      default:
        break;
    }
  }

  return false;
}

#endif
