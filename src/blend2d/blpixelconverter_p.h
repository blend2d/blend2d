// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLPIXELCONVERTER_P_H
#define BLEND2D_BLPIXELCONVERTER_P_H

#include "./blapi-internal_p.h"
#include "./blpixelconverter.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! Native pixel formats for pixel converter.
enum BLPixelConverterFormat : uint32_t {
  BL_PIXEL_CONVERTER_FORMAT_NONE   = 0,
  BL_PIXEL_CONVERTER_FORMAT_PRGB32 = BL_FORMAT_PRGB32,
  BL_PIXEL_CONVERTER_FORMAT_XRGB32 = BL_FORMAT_XRGB32,
  BL_PIXEL_CONVERTER_FORMAT_A8     = BL_FORMAT_A8,
  BL_PIXEL_CONVERTER_FORMAT_ARGB32 = BL_FORMAT_COUNT + 0,

  BL_PIXEL_CONVERTER_FORMAT_COUNT  = BL_FORMAT_COUNT + 1
};

//! Strategy used by BLPixelConverter.
enum BLPixelConverterStrategy : uint32_t {
  //! None - this means that the converter is not initialized.
  BL_PIXEL_CONVERTER_STRATEGY_NONE,
  BL_PIXEL_CONVERTER_STRATEGY_LOOKUP_TABLE,
  BL_PIXEL_CONVERTER_STRATEGY_SHUFFLE_BYTE,
  BL_PIXEL_CONVERTER_STRATEGY_XRGB32_FROM_XRGB_ANY,
  BL_PIXEL_CONVERTER_STRATEGY_PRGB32_FROM_ARGB_ANY,
  BL_PIXEL_CONVERTER_STRATEGY_PRGB32_FROM_PRGB_ANY
};

// ============================================================================
// [BLPixelConverter - Globals]
// ============================================================================

BL_HIDDEN extern const BLFormatInfo blPixelConverterFormatInfo[BL_PIXEL_CONVERTER_FORMAT_COUNT];
BL_HIDDEN extern const BLPixelConverterOptions blPixelConverterDefaultOptions;

//! Internal data used by `BLPixelConverter`.
struct BLPixelConverterData {
  //! Data used by lookup table strategy.
  struct LookupTable {
    uint8_t strategy;
    uint8_t reserved[sizeof(void*) - 3];
    const uint32_t* table;
  };

  //! Data used by shuffle bytes strategy.
  struct ShuffleByte {
    uint8_t strategy;
    uint8_t reserved[3];
  };

  //! Data used by strategies for converting ANY pixel format to native XRGB/PRGB.
  struct NativeFromExternal {
    uint8_t strategy;
    uint8_t reserved[3];
    uint32_t fillMask;
    uint8_t shifts[4];
    uint32_t masks[4];
    uint32_t scale[4];
    uint32_t simdData[4];
  };

  struct ExternalFromNative {
    uint8_t strategy;
    uint8_t reserved[3];
    uint32_t fillMask;
    uint8_t shifts[4];
    uint32_t masks[4];
    uint32_t simdData[4];
  };

  union {
    uint8_t strategy;
    LookupTable lookupTable;
    ShuffleByte shuffleByte;
    NativeFromExternal nativeFromExternal;
    ExternalFromNative externalFromNative;
  };
};

static_assert(sizeof(BLPixelConverterData) <= sizeof(BLPixelConverterCore) - sizeof(void*),
              "BLPixelConverterData cannot be longer than BLPixelConverterCore::data");

static BL_INLINE BLPixelConverterData* blPixelConverterGetData(BLPixelConverterCore* self) noexcept {
  return reinterpret_cast<BLPixelConverterData*>(self->data);
}

static BL_INLINE const BLPixelConverterData* blPixelConverterGetData(const BLPixelConverterCore* self) noexcept {
  return reinterpret_cast<const BLPixelConverterData*>(self->data);
}

static BL_INLINE uint8_t* blPixelConverterFillGap(uint8_t* data, size_t size) noexcept {
  uint8_t* end = data + size;
  while (data != end)
    *data++ = 0;
  return data;
}

// ============================================================================
// [BLPixelConverter - Create]
// ============================================================================

#ifdef BL_BUILD_OPT_SSE2
BL_HIDDEN bool blPixelConverterInitNativeFromXRGB_SSE2(BLPixelConverterCore* self, uint32_t dstFormat, const BLFormatInfo& srcInfo) noexcept;
#endif

#ifdef BL_BUILD_OPT_SSSE3
BL_HIDDEN bool blPixelConverterInitNativeFromXRGB_SSSE3(BLPixelConverterCore* self, uint32_t dstFormat, const BLFormatInfo& srcInfo) noexcept;
#endif

#ifdef BL_BUILD_OPT_AVX2
BL_HIDDEN bool blPixelConverterInitNativeFromXRGB_AVX2(BLPixelConverterCore* self, uint32_t dstFormat, const BLFormatInfo& srcInfo) noexcept;
#endif

//! \}
//! \endcond

#endif // BLEND2D_BLPIXELCONVERTER_P_H
