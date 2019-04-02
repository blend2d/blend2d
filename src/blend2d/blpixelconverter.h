// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLPIXELCONVERTER_H
#define BLEND2D_BLPIXELCONVERTER_H

#include "./blapi.h"
#include "./blformat.h"
#include "./blgeometry.h"

//! \addtogroup blend2d_api_images
//! \{

// ============================================================================
// [Typedefs]
// ============================================================================

//! \cond INTERNAL
//! \ingroup  blend2d_internal
//! Pixel converter function.
typedef BLResult (BL_CDECL* BLPixelConverterFunc)(
  const BLPixelConverterCore* self,
  uint8_t* dstData, intptr_t dstStride,
  const uint8_t* srcData, intptr_t srcStride,
  uint32_t w, uint32_t h, const BLPixelConverterOptions* options) BL_NOEXCEPT;
//! \endcond

// ============================================================================
// [BLPixelConverter - Options]
// ============================================================================

//! Pixel conversion options.
struct BLPixelConverterOptions {
  BLPointI origin;
  size_t gap;
};

// ============================================================================
// [BLPixelConverter - Core]
// ============================================================================

//! Pixel converter [C Interface - Core].
struct BLPixelConverterCore {
  //! Converter function.
  BLPixelConverterFunc convertFunc;

  union {
    uint8_t strategy;
    //! Internal data used by the pixel converter not exposed to users.
    uint8_t data[64];
  };
};

// ============================================================================
// [BLPixelConverter - C++]
// ============================================================================

#ifdef __cplusplus
//! Pixel converter [C++ API].
//!
//! Provides interface to convert pixels between various pixel formats. The
//! primary purpose of this class is to allow efficient conversion between
//! pixel formats used natively by Blend2D and pixel formats required by I/O.
class BLPixelConverter : public BLPixelConverterCore {
public:
  BL_INLINE BLPixelConverter() noexcept { blPixelConverterInit(this); }
  BL_INLINE BLPixelConverter(const BLPixelConverter& other) noexcept { blPixelConverterInitWeak(this, &other); }
  BL_INLINE ~BLPixelConverter() noexcept { blPixelConverterReset(this); }

  BL_INLINE BLPixelConverter& operator=(const BLPixelConverter& other) noexcept {
    blPixelConverterAssign(this, &other);
    return *this;
  }

  //! Returns `true` if the converter is initialized.
  BL_INLINE bool isInitialized() const noexcept { return strategy != 0; }

  //! Reset the pixel converter.
  BL_INLINE BLResult reset() noexcept {
    return blPixelConverterReset(this);
  }

  BL_INLINE BLResult assign(const BLPixelConverter& other) noexcept {
    return blPixelConverterAssign(this, &other);
  }

  BL_INLINE BLResult create(const BLFormatInfo& dstInfo, const BLFormatInfo& srcInfo) noexcept {
    return blPixelConverterCreate(this, &dstInfo, &srcInfo);
  }

  //! Convert a single span of pixels of `w` width.
  BL_INLINE BLResult convertSpan(void* dstData, const void* srcData, uint32_t w,
                                 const BLPixelConverterOptions* options = nullptr) const noexcept {
    return convertFunc(this, static_cast<uint8_t*>(dstData), 0, static_cast<const uint8_t*>(srcData), 0, w, 1, options);
  }

  //! Convert a rectangular area of pixels from source format to destination.
  BL_INLINE BLResult convertRect(void* dstData, intptr_t dstStride,
                                 const void* srcData, intptr_t srcStride,
                                 uint32_t w, uint32_t h, const BLPixelConverterOptions* options = nullptr) const noexcept {
    return convertFunc(this, static_cast<uint8_t*>(dstData), dstStride, static_cast<const uint8_t*>(srcData), srcStride, w, h, options);
  }
};
#endif

//! \}

#endif // BLEND2D_BLPIXELCONVERTER_H
