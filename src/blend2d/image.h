// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_IMAGE_H
#define BLEND2D_IMAGE_H

#include "./array.h"
#include "./format.h"
#include "./geometry.h"
#include "./variant.h"

//! \addtogroup blend2d_api_imaging
//! \{

// ============================================================================
// [BLImage - Constants]
// ============================================================================

//! Flags used by `BLImageInfo`.
BL_DEFINE_ENUM(BLImageInfoFlags) {
  //! Progressive mode.
  BL_IMAGE_INFO_FLAG_PROGRESSIVE = 0x00000001u
};

//! Filter type used by `BLImage::scale()`.
BL_DEFINE_ENUM(BLImageScaleFilter) {
  //! No filter or uninitialized.
  BL_IMAGE_SCALE_FILTER_NONE = 0,
  //! Nearest neighbor filter (radius 1.0).
  BL_IMAGE_SCALE_FILTER_NEAREST = 1,
  //! Bilinear filter (radius 1.0).
  BL_IMAGE_SCALE_FILTER_BILINEAR = 2,
  //! Bicubic filter (radius 2.0).
  BL_IMAGE_SCALE_FILTER_BICUBIC = 3,
  //! Bell filter (radius 1.5).
  BL_IMAGE_SCALE_FILTER_BELL = 4,
  //! Gauss filter (radius 2.0).
  BL_IMAGE_SCALE_FILTER_GAUSS = 5,
  //! Hermite filter (radius 1.0).
  BL_IMAGE_SCALE_FILTER_HERMITE = 6,
  //! Hanning filter (radius 1.0).
  BL_IMAGE_SCALE_FILTER_HANNING = 7,
  //! Catrom filter (radius 2.0).
  BL_IMAGE_SCALE_FILTER_CATROM = 8,
  //! Bessel filter (radius 3.2383).
  BL_IMAGE_SCALE_FILTER_BESSEL = 9,
  //! Sinc filter (radius 2.0, adjustable through `BLImageScaleOptions`).
  BL_IMAGE_SCALE_FILTER_SINC = 10,
  //! Lanczos filter (radius 2.0, adjustable through `BLImageScaleOptions`).
  BL_IMAGE_SCALE_FILTER_LANCZOS = 11,
  //! Blackman filter (radius 2.0, adjustable through `BLImageScaleOptions`).
  BL_IMAGE_SCALE_FILTER_BLACKMAN = 12,
  //! Mitchell filter (radius 2.0, parameters 'b' and 'c' passed through `BLImageScaleOptions`).
  BL_IMAGE_SCALE_FILTER_MITCHELL = 13,
  //! Filter using a user-function, must be passed through `BLImageScaleOptions`.
  BL_IMAGE_SCALE_FILTER_USER = 14,

  //! Count of image-scale filters.
  BL_IMAGE_SCALE_FILTER_COUNT = 15
};

// ============================================================================
// [BLImage - Typedefs]
// ============================================================================

//! A user function that can be used by `BLImage::scale()`.
typedef BLResult (BL_CDECL* BLImageScaleUserFunc)(double* dst, const double* tArray, size_t n, const void* data) BL_NOEXCEPT;

// ============================================================================
// [BLImage - Data]
// ============================================================================

//! Data that describes a raster image. Used by `BLImage`.
struct BLImageData {
  void* pixelData;
  intptr_t stride;
  BLSizeI size;
  uint32_t format;
  uint32_t flags;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLImage - Info]
// ============================================================================

//! Image information provided by image codecs.
struct BLImageInfo {
  //! Image size.
  BLSizeI size;
  //! Pixel density per one meter, can contain fractions.
  BLSize density;

  //! Image flags.
  uint32_t flags;
  //! Image depth.
  uint16_t depth;
  //! Number of planes.
  uint16_t planeCount;
  //! Number of frames (0 = unknown/unspecified).
  uint64_t frameCount;

  //! Image format (as understood by codec).
  char format[16];
  //! Image compression (as understood by codec).
  char compression[16];

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLImage - ScaleOptions]
// ============================================================================

//! Options that can used to customize image scaling.
struct BLImageScaleOptions {
  BLImageScaleUserFunc userFunc;
  void* userData;

  double radius;
  union {
    double data[3];
    struct {
      double b, c;
    } mitchell;
  };

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  BL_INLINE void resetToDefaults() noexcept {
    userFunc = nullptr;
    userData = nullptr;
    radius = 2.0;
    mitchell.b = 1.0 / 3.0;
    mitchell.c = 1.0 / 3.0;
    data[2] = 0.0;
  }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLImage - Core]
// ============================================================================

//! Image [C Interface - Impl].
struct BLImageImpl {
  //! Pixel data.
  void* pixelData;

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;

  //! Image format.
  uint8_t format;
  //! Image flags.
  uint8_t flags;
  //! Image depth (in bits).
  uint16_t depth;
  //! Image size.
  BLSizeI size;
  //! Image stride.
  intptr_t stride;
};

//! Image [C Interface - Core].
struct BLImageCore {
  BLImageImpl* impl;
};

// ============================================================================
// [BLImage - C++]
// ============================================================================

#ifdef __cplusplus
//! 2D raster image [C++ API].
class BLImage : public BLImageCore {
public:
  //! \cond INTERNAL
  static constexpr const uint32_t kImplType = BL_IMPL_TYPE_IMAGE;
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLImage() noexcept { this->impl = none().impl; }
  BL_INLINE BLImage(BLImage&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLImage(const BLImage& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLImage(BLImageImpl* impl) noexcept { this->impl = impl; }
  BL_INLINE BLImage(int w, int h, uint32_t format) noexcept { blImageInitAs(this, w, h, format); }
  BL_INLINE ~BLImage() { blImageReset(this); }

  //! \}

  //! \name Operator Overloads
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return !empty(); }

  BL_INLINE BLImage& operator=(BLImage&& other) noexcept { blImageAssignMove(this, &other); return *this; }
  BL_INLINE BLImage& operator=(const BLImage& other) noexcept { blImageAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLImage& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLImage& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blImageReset(this); }
  BL_INLINE void swap(BLImage& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult assign(BLImage&& other) noexcept { return blImageAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLImage& other) noexcept { return blImageAssignWeak(this, &other); }

  //! Create a deep copy of the `other` image.
  BL_INLINE BLResult assignDeep(const BLImage& other) noexcept { return blImageAssignDeep(this, &other); }

  //! Tests whether the image is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }
  //! Tests whether the image is empty (has no size).
  BL_INLINE bool empty() const noexcept { return impl->format == BL_FORMAT_NONE; }

  BL_INLINE bool equals(const BLImage& other) const noexcept { return blImageEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  //! Create a new image of a specified width `w`, height `h`, and `format`.
  //!
  //! \note It's important to always test whether the function succeeded as
  //! allocating pixel-data can fail. If invalid arguments (invalid size or
  //! format) were passed to the function a `BL_ERROR_INVALID_VALUE` result
  //! will be returned and no data will be allocated. It's also important
  //! to notice that `BLImage::create()` would not change anything if the
  //! function fails (the previous image content would be kept as is).
  BL_INLINE BLResult create(int w, int h, uint32_t format) noexcept {
    return blImageCreate(this, w, h, format);
  }

  //! Create a new image from external data.
  BL_INLINE BLResult createFromData(
    int w, int h, uint32_t format,
    void* pixelData, intptr_t stride,
    BLDestroyImplFunc destroyFunc = nullptr,
    void* destroyData = nullptr) noexcept { return blImageCreateFromData(this, w, h, format, pixelData, stride, destroyFunc, destroyData); }

  //! \}

  //! \name Image Data
  //! \{

  //! Returns image width.
  BL_INLINE int width() const noexcept { return impl->size.w; }
  //! Returns image height.
  BL_INLINE int height() const noexcept { return impl->size.h; }
  //! Returns image size.
  BL_INLINE const BLSizeI& size() const noexcept { return impl->size; }
  //! Returns image format, see `BLFormat`.
  BL_INLINE uint32_t format() const noexcept { return impl->format; }

  BL_INLINE BLResult getData(BLImageData* dataOut) const noexcept { return blImageGetData(this, dataOut); }
  BL_INLINE BLResult makeMutable() noexcept { BLImageData unused; return blImageMakeMutable(this, &unused); }
  BL_INLINE BLResult makeMutable(BLImageData* dataOut) noexcept { return blImageMakeMutable(this, dataOut); }

  //! \}

  //! \name Image Utilities
  //! \{

  BL_INLINE BLResult convert(uint32_t format) noexcept {
    return blImageConvert(this, format);
  }

  //! \}

  //! \name Image IO
  //! \{

  BL_INLINE BLResult readFromFile(const char* fileName) noexcept {
    return blImageReadFromFile(this, fileName, nullptr);
  }

  BL_INLINE BLResult readFromFile(const char* fileName, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageReadFromFile(this, fileName, &codecs);
  }

  BL_INLINE BLResult readFromData(const void* data, size_t size) noexcept {
    return blImageReadFromData(this, data, size, nullptr);
  }

  BL_INLINE BLResult readFromData(const void* data, size_t size, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageReadFromData(this, data, size, &codecs);
  }

  BL_INLINE BLResult readFromData(const BLArray<uint8_t>& array) noexcept {
    return blImageReadFromData(this, array.data(), array.size(), nullptr);
  }

  BL_INLINE BLResult readFromData(const BLArray<uint8_t>& array, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageReadFromData(this, array.data(), array.size(), &codecs);
  }

  BL_INLINE BLResult readFromData(const BLArrayView<uint8_t>& view) noexcept {
    return blImageReadFromData(this, view.data, view.size, nullptr);
  }

  BL_INLINE BLResult readFromData(const BLArrayView<uint8_t>& view, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageReadFromData(this, view.data, view.size, &codecs);
  }

  BL_INLINE BLResult writeToFile(const char* fileName, const BLImageCodec& codec) noexcept {
    return blImageWriteToFile(this, fileName, reinterpret_cast<const BLImageCodecCore*>(&codec));
  }

  BL_INLINE BLResult writeToData(BLArray<uint8_t>& dst, const BLImageCodec& codec) noexcept {
    return blImageWriteToData(this, &dst, reinterpret_cast<const BLImageCodecCore*>(&codec));
  }

  //! \}

  static BL_INLINE const BLImage& none() noexcept { return reinterpret_cast<const BLImage*>(blNone)[kImplType]; }

  static BL_INLINE BLResult scale(BLImage& dst, const BLImage& src, const BLSizeI& size, uint32_t filter, const BLImageScaleOptions* options = nullptr) noexcept {
    return blImageScale(&dst, &src, &size, filter, options);
  }
};
#endif

//! \}

#endif // BLEND2D_IMAGE_H
