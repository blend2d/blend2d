// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIXELCONVERTER_P_H_INCLUDED
#define BLEND2D_PIXELCONVERTER_P_H_INCLUDED

#include "api-internal_p.h"
#include "pixelconverter.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! Internal flags used by `BLPixelConverterData::internalFlags`.
enum BLPixelConverterInternalFlags : uint8_t {
  //! The pixel converter is initialized.
  BL_PIXEL_CONVERTER_INTERNAL_FLAG_INITIALIZED = 0x01u,

  //! Set when the conversions is using CPU-specific optimizations.
  BL_PIXEL_CONVERTER_INTERNAL_FLAG_OPTIMIZED = 0x02u,

  //! Set when the destination and source formats match.
  BL_PIXEL_CONVERTER_INTERNAL_FLAG_RAW_COPY = 0x04u,

  //! Set when the pixel converter is a multi-step converter.
  BL_PIXEL_CONVERTER_INTERNAL_FLAG_MULTI_STEP = 0x40u,

  //! The pixel converter contains data in `dataPtr` that is dynamic and must be freed. To allow reference-counting
  //! it also contains a pointer to `refCount`, which was allocated together with `dataPtr`. Since `refCount` is part
  //! of `dataPtr` it's freed with it.
  BL_PIXEL_CONVERTER_INTERNAL_FLAG_DYNAMIC_DATA = 0x80u
};

BL_HIDDEN extern const BLPixelConverterOptions blPixelConverterDefaultOptions;

//! Internal initialized that accepts already sanitized `di` and `si` info.
BL_HIDDEN BLResult blPixelConverterInitInternal(
  BLPixelConverterCore* self,
  const BLFormatInfo& di, const BLFormatInfo& si, BLPixelConverterCreateFlags createFlags) noexcept;

// Number of bytes used by the intermediate buffer. This number is adjustable, but it's not a good idea to increase
// it so much as when it gets close to a page size the C++ compiler would have to generate stack probes so the stack
// doesn't run out. We don't want such probes in the conversion function.
static constexpr uint32_t BL_PIXEL_CONVERTER_MULTISTEP_BUFFER_SIZE = 2048 + 1024;

// TODO: Implement multi-step converter.
struct BLPixelConverterMultiStepContext {
  volatile size_t refCount;
  BLPixelConverterCore first;
  BLPixelConverterCore second;
};

//! Internal data mapped to `BLPixelConverter::data`.
struct BLPixelConverterData {
  struct MultiStepData {
    BLPixelConverterFunc convertFunc;
    uint8_t internalFlags;
    uint8_t dstBytesPerPixel;
    uint8_t srcBytesPerPixel;
    uint8_t intermediateBytesPerPixel;
    uint32_t intermediatePixelCount;

    BLPixelConverterMultiStepContext* ctx;
    size_t* refCount;
  };

  //! Data used to convert an indexed format to a non-indexed format.
  struct IndexedData {
    BLPixelConverterFunc convertFunc;
    uint8_t internalFlags;
    uint8_t reserved[3];
    uint32_t alphaMask;

    struct DynamicData {
      union {
        void* table;
        uint8_t* table8;
        uint16_t* table16;
        uint32_t* table32;
      };
      size_t* refCount;
    };

    union EmbeddedData {
      uint8_t table8[64];
      uint16_t table16[32];
      uint32_t table32[16];
    };

    union {
      DynamicData dynamic;
      EmbeddedData embedded;
    };
  };

  //! Data used to make a raw copy of pixels.
  //!
  //! Used by 'copy' and 'copy_or' converters.
  struct MemCopyData {
    BLPixelConverterFunc convertFunc;
    uint8_t internalFlags;
    uint8_t bytesPerPixel;            // Only used by generic implementations.
    uint8_t reserved[2];              // Alignment only.
    uint32_t fillMask;                // Only used by copy-or implementations.
  };

  //! A8 From ARGB32/PRGB32 data
  struct X8FromRgb32Data {
    BLPixelConverterFunc convertFunc;
    uint8_t internalFlags;
    uint8_t bytesPerPixel;
    uint8_t alphaShift;
    uint8_t reserved[2];
  };

  //! RGB32 from A8/L8 data.
  //!
  //! Can be used to convert both A8 to RGB32 or L8 (greyscale) to RGB32 - the
  //! only thing needed is to specify proper `andMask` and `fillMask`.
  struct Rgb32FromX8Data {
    BLPixelConverterFunc convertFunc;
    uint8_t internalFlags;
    uint8_t reserved[3];              // Alignment only.
    uint32_t fillMask;                // Destination fill-mask (to fill alpha/undefined bits).
    uint32_t zeroMask;                // Destination zero-mask (to clear RGB channels).
  };

  //! Data used by byte shuffles.
  struct ShufbData {
    BLPixelConverterFunc convertFunc;
    uint8_t internalFlags;
    uint8_t reserved[3];
    uint32_t fillMask;
    uint32_t shufbPredicate[4];
  };

  struct PremultiplyData {
    BLPixelConverterFunc convertFunc;
    uint8_t internalFlags;
    uint8_t alphaShift;               // Not always used.
    uint8_t reserved[2];              // Alignment only.
    uint32_t fillMask;                // Destination fill-mask (to fill alpha/undefined bits).
    uint32_t shufbPredicate[4];       // Shuffle predicate for implementations using PSHUFB.
  };

  //! Data used to convert ANY pixel format to native XRGB/PRGB.
  struct NativeFromForeign {
    BLPixelConverterFunc convertFunc;
    uint8_t internalFlags;
    uint8_t reserved[3];
    uint32_t fillMask;
    uint32_t shufbPredicate[4];

    uint8_t shifts[4];
    uint32_t masks[4];
    uint32_t scale[4];
  };

  struct ForeignFromNative {
    BLPixelConverterFunc convertFunc;
    uint8_t internalFlags;
    uint8_t reserved[3];
    uint32_t fillMask;
    uint32_t shufbPredicate[4];

    uint8_t shifts[4];
    uint32_t masks[4];
  };

  union {
    struct {
      BLPixelConverterFunc convertFunc;
      uint8_t internalFlags;
      uint8_t reserved[7];

      void* dataPtr;
      size_t* refCount;
    };

    MultiStepData multiStepData;
    IndexedData indexedData;
    MemCopyData memCopyData;
    X8FromRgb32Data x8FromRgb32Data;
    Rgb32FromX8Data rgb32FromX8Data;
    ShufbData shufbData;
    PremultiplyData premultiplyData;
    NativeFromForeign nativeFromForeign;
    ForeignFromNative foreignFromNative;
  };
};

BL_STATIC_ASSERT(sizeof(BLPixelConverterData) <= sizeof(BLPixelConverterCore));

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

// All functions that can be used as a fallback by optimized converters must be defined here, in addition to all
// optimized functions that are dispatched in `blpixelconverter.cpp`.

#define BL_DECLARE_CONVERTER_BASE(NAME)                                       \
  BL_HIDDEN BLResult BL_CDECL NAME(                                           \
    const BLPixelConverterCore* self,                                         \
    uint8_t* dstData, intptr_t dstStride,                                     \
    const uint8_t* srcData, intptr_t srcStride,                               \
    uint32_t w, uint32_t h, const BLPixelConverterOptions* options) noexcept;

#ifdef BL_BUILD_OPT_SSE2
#define BL_DECLARE_CONVERTER_SSE2(NAME) BL_DECLARE_CONVERTER_BASE(NAME)
#else
#define BL_DECLARE_CONVERTER_SSE2(NAME)
#endif

#ifdef BL_BUILD_OPT_SSSE3
#define BL_DECLARE_CONVERTER_SSSE3(NAME) BL_DECLARE_CONVERTER_BASE(NAME)
#else
#define BL_DECLARE_CONVERTER_SSSE3(NAME)
#endif

#ifdef BL_BUILD_OPT_AVX2
#define BL_DECLARE_CONVERTER_AVX2(NAME) BL_DECLARE_CONVERTER_BASE(NAME)
#else
#define BL_DECLARE_CONVERTER_AVX2(NAME)
#endif

BL_DECLARE_CONVERTER_BASE(bl_convert_copy)
BL_DECLARE_CONVERTER_SSE2(bl_convert_copy_sse2)
BL_DECLARE_CONVERTER_AVX2(bl_convert_copy_avx2)

BL_DECLARE_CONVERTER_BASE(bl_convert_copy_or_8888)
BL_DECLARE_CONVERTER_SSE2(bl_convert_copy_or_8888_sse2)
BL_DECLARE_CONVERTER_AVX2(bl_convert_copy_or_8888_avx2)

BL_DECLARE_CONVERTER_SSSE3(bl_convert_copy_shufb_8888_ssse3)
BL_DECLARE_CONVERTER_AVX2(bl_convert_copy_shufb_8888_avx2)

BL_DECLARE_CONVERTER_BASE(bl_convert_a8_from_8888)

BL_DECLARE_CONVERTER_BASE(bl_convert_8888_from_x8)
BL_DECLARE_CONVERTER_SSE2(bl_convert_8888_from_x8_sse2)

BL_DECLARE_CONVERTER_SSSE3(bl_convert_rgb32_from_rgb24_shufb_ssse3)
BL_DECLARE_CONVERTER_AVX2(bl_convert_rgb32_from_rgb24_shufb_avx2)

BL_DECLARE_CONVERTER_SSE2(bl_convert_premultiply_8888_leading_alpha_sse2)
BL_DECLARE_CONVERTER_SSE2(bl_convert_premultiply_8888_trailing_alpha_sse2)

BL_DECLARE_CONVERTER_AVX2(bl_convert_premultiply_8888_leading_alpha_avx2)
BL_DECLARE_CONVERTER_AVX2(bl_convert_premultiply_8888_trailing_alpha_avx2)

BL_DECLARE_CONVERTER_SSSE3(bl_convert_premultiply_8888_leading_alpha_shufb_ssse3)
BL_DECLARE_CONVERTER_SSSE3(bl_convert_premultiply_8888_trailing_alpha_shufb_ssse3)

BL_DECLARE_CONVERTER_AVX2(bl_convert_premultiply_8888_leading_alpha_shufb_avx2)
BL_DECLARE_CONVERTER_AVX2(bl_convert_premultiply_8888_trailing_alpha_shufb_avx2)

BL_DECLARE_CONVERTER_SSE2(bl_convert_unpremultiply_8888_leading_alpha_sse2)
BL_DECLARE_CONVERTER_SSE2(bl_convert_unpremultiply_8888_trailing_alpha_sse2)

BL_DECLARE_CONVERTER_AVX2(bl_convert_unpremultiply_8888_leading_alpha_pmulld_avx2)
BL_DECLARE_CONVERTER_AVX2(bl_convert_unpremultiply_8888_trailing_alpha_pmulld_avx2)

BL_DECLARE_CONVERTER_AVX2(bl_convert_unpremultiply_8888_leading_alpha_float_avx2)
BL_DECLARE_CONVERTER_AVX2(bl_convert_unpremultiply_8888_trailing_alpha_float_avx2)

#undef BL_DECLARE_CONVERTER_AVX2
#undef BL_DECLARE_CONVERTER_SSSE3
#undef BL_DECLARE_CONVERTER_SSE2
#undef BL_DECLARE_CONVERTER_BASE

//! \}
//! \endcond

#endif // BLEND2D_PIXELCONVERTER_P_H_INCLUDED
