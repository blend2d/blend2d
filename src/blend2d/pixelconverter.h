// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_PIXELCONVERTER_H
#define BLEND2D_PIXELCONVERTER_H

#include "./api.h"
#include "./format.h"
#include "./geometry.h"

//! \addtogroup blend2d_api_imaging
//! \{

// ============================================================================
// [BLPixelConverterFunc]
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
// [BLPixelConverterCreateFlags]
// ============================================================================

//! Flags used by `BLPixelConverter::create()` function.
BL_DEFINE_ENUM(BLPixelConverterCreateFlags) {
  //! Specifies that the source palette in `BLFormatInfo` doesn't have to by
  //! copied by `BLPixelConverter`. The caller must ensure that the palette
  //! would stay valid until the pixel converter is destroyed.
  BL_PIXEL_CONVERTER_CREATE_FLAG_DONT_COPY_PALETTE = 0x00000001u,

  //! Specifies that the source palette in `BLFormatInfo` is alterable and
  //! the pixel converter can modify it when preparing the conversion. The
  //! modification can be irreversible so only use this flag when you are sure
  //! that the palette passed to `BLPixelConverter::create()` won't be needed
  //! outside of pixel conversion.
  //!
  //! \note The flag `BL_PIXEL_CONVERTER_CREATE_FLAG_DONT_COPY_PALETTE` must be
  //! set as well, otherwise this flag would be ignored.
  BL_PIXEL_CONVERTER_CREATE_FLAG_ALTERABLE_PALETTE = 0x00000002u,

  //! When there is no built-in conversion betweeb the given pixel formats it's
  //! possible to use an intermediate format that is used during conversion. In
  //! such case the base pixel converter creates two more converters that are
  //! then used internally.
  //!
  //! This option disables such feature - creating a pixel converter would fail
  //! with `BL_ERROR_NOT_IMPLEMENTED` error if direct conversion is not possible.
  BL_PIXEL_CONVERTER_CREATE_FLAG_NO_MULTI_STEP = 0x00000004u
};

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
  union {
    struct {
      //! Converter function.
      BLPixelConverterFunc convertFunc;
      //! Internal flags used by the converter - non-zero value means initialized.
      uint8_t internalFlags;
    };

    //! Internal data not exposed to users, aligned to sizeof(void*).
    uint8_t data[80];
  };
};

// ============================================================================
// [BLPixelConverter - C++]
// ============================================================================

#ifdef __cplusplus
//! Pixel converter [C++ API].
//!
//! Provides an interface to convert pixels between various pixel formats. The
//! primary purpose of this class is to allow efficient conversion between
//! pixel formats used natively by Blend2D and pixel formats used elsewhere,
//! for example image codecs or native framebuffers.
//!
//! \note A default-initialized converter has a valid conversion function that
//! would return \ref BL_ERROR_NOT_INITIALIZED if invoked. Use `isInitialized()`
//! member function to test whether the pixel converter was properly initialized.
class BLPixelConverter : public BLPixelConverterCore {
public:
  //! Creates a new default-initialized pixel converter.
  BL_INLINE BLPixelConverter() noexcept {
    blPixelConverterInit(this);
  }

  //! Creates a copy of the `other` converter.
  //!
  //! If the `other` converter has dynamically allocated resources they will
  //! be properly managed (reference counting). Only very specific converters
  //! require such resources so this operation should be considered very cheap.
  BL_INLINE BLPixelConverter(const BLPixelConverter& other) noexcept {
    blPixelConverterInitWeak(this, &other);
  }

  //! Destroys the pixel-converter and releases all resources allocated by it.
  BL_INLINE ~BLPixelConverter() noexcept {
    blPixelConverterReset(this);
  }

  BL_INLINE BLPixelConverter& operator=(const BLPixelConverter& other) noexcept {
    blPixelConverterAssign(this, &other);
    return *this;
  }

  //! Returns `true` if the converter is initialized.
  BL_INLINE bool isInitialized() const noexcept {
    // Internal flags are non-zero when the pixel converter is initialized.
    return internalFlags != 0;
  }

  //! Reset the pixel converter.
  BL_INLINE BLResult reset() noexcept {
    return blPixelConverterReset(this);
  }

  //! Assigns the `other` pixel converter into this one.
  BL_INLINE BLResult assign(const BLPixelConverter& other) noexcept {
    return blPixelConverterAssign(this, &other);
  }

  //! Creates a new pixel converter that will convert pixels described by
  //! `srcInfo` into pixels described by `dstInfo`.
  //!
  //! Use `createFlags` to further specify the parameters of the conversion,
  //! see \ref BLPixelConverterCreateFlags for more details.
  //!
  //! \note Destination and source format informattion must be valid,
  //! otherwise `BL_ERROR_INVALID_VALUE` would be returned.
  BL_INLINE BLResult create(const BLFormatInfo& dstInfo, const BLFormatInfo& srcInfo, uint32_t createFlags = 0) noexcept {
    return blPixelConverterCreate(this, &dstInfo, &srcInfo, createFlags);
  }

  //! Converts a single span of pixels of `w` width.
  BL_INLINE BLResult convertSpan(void* dstData, const void* srcData, uint32_t w,
                                 const BLPixelConverterOptions* options = nullptr) const noexcept {
    return convertFunc(this, static_cast<uint8_t*>(dstData), 0, static_cast<const uint8_t*>(srcData), 0, w, 1, options);
  }

  //! Converts a rectangular area of pixels from source format to destination.
  BL_INLINE BLResult convertRect(void* dstData, intptr_t dstStride,
                                 const void* srcData, intptr_t srcStride,
                                 uint32_t w, uint32_t h, const BLPixelConverterOptions* options = nullptr) const noexcept {
    return convertFunc(this, static_cast<uint8_t*>(dstData), dstStride, static_cast<const uint8_t*>(srcData), srcStride, w, h, options);
  }
};
#endif

//! \}

#endif // BLEND2D_PIXELCONVERTER_H
