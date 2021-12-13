// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_IMAGE_H_INCLUDED
#define BLEND2D_IMAGE_H_INCLUDED

#include "array.h"
#include "format.h"
#include "geometry.h"
#include "imagecodec.h"
#include "object.h"

//! \addtogroup blend2d_api_imaging
//! \{

//! \name BLImage - Constants
//! \{

//! Flags used by `BLImageInfo`.
BL_DEFINE_ENUM(BLImageInfoFlags) {
  //! No flags.
  BL_IMAGE_INFO_FLAG_NO_FLAGS = 0u,
  //! Progressive mode.
  BL_IMAGE_INFO_FLAG_PROGRESSIVE = 0x00000001u

  BL_FORCE_ENUM_UINT32(BL_IMAGE_INFO_FLAG)
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

  //! Maximum value of `BLImageScaleFilter`.
  BL_IMAGE_SCALE_FILTER_MAX_VALUE = 14

  BL_FORCE_ENUM_UINT32(BL_IMAGE_SCALE_FILTER)
};

//! \}

//! \name BLImage - Types
//! \{

//! A user function that can be used by `BLImage::scale()`.
typedef BLResult (BL_CDECL* BLImageScaleUserFunc)(double* dst, const double* tArray, size_t n, const void* data) BL_NOEXCEPT;

//! \}

//! \name BLImage - Structs
//! \{

//! Data that describes a raster image. Used by `BLImage`.
struct BLImageData {
  void* pixelData;
  intptr_t stride;
  BLSizeI size;
  uint32_t format;
  uint32_t flags;

#ifdef __cplusplus
  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
#endif
};

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

#ifdef __cplusplus
  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }
#endif
};

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
};

//! \}

//! \name BLImage - C API
//! \{

//! \cond INTERNAL
//! 2D raster image [Impl].
struct BLImageImpl {
  //! Pixel data.
  void* pixelData;
  //! Image stride.
  intptr_t stride;
  //! Image size.
  BLSizeI size;
  //! Image format.
  uint8_t format;
  //! Image flags.
  uint8_t flags;
  //! Image depth (in bits).
  uint16_t depth;
  //! Reserved for future use, must be zero.
  uint8_t reserved[4];
};
//! \endcond

//! 2D raster image [C API].
struct BLImageCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
};

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blImageInit(BLImageCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageInitMove(BLImageCore* self, BLImageCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageInitWeak(BLImageCore* self, const BLImageCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageInitAs(BLImageCore* self, int w, int h, BLFormat format) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageInitAsFromData(BLImageCore* self, int w, int h, BLFormat format, void* pixelData, intptr_t stride, BLDestroyExternalDataFunc destroyFunc, void* userData) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageDestroy(BLImageCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageReset(BLImageCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageAssignMove(BLImageCore* self, BLImageCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageAssignWeak(BLImageCore* self, const BLImageCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageAssignDeep(BLImageCore* self, const BLImageCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCreate(BLImageCore* self, int w, int h, BLFormat format) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCreateFromData(BLImageCore* self, int w, int h, BLFormat format, void* pixelData, intptr_t stride, BLDestroyExternalDataFunc destroyFunc, void* userData) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageGetData(const BLImageCore* self, BLImageData* dataOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageMakeMutable(BLImageCore* self, BLImageData* dataOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageConvert(BLImageCore* self, BLFormat format) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blImageEquals(const BLImageCore* a, const BLImageCore* b) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageScale(BLImageCore* dst, const BLImageCore* src, const BLSizeI* size, uint32_t filter, const BLImageScaleOptions* options) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageReadFromFile(BLImageCore* self, const char* fileName, const BLArrayCore* codecs) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageReadFromData(BLImageCore* self, const void* data, size_t size, const BLArrayCore* codecs) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageWriteToFile(const BLImageCore* self, const char* fileName, const BLImageCodecCore* codec) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageWriteToData(const BLImageCore* self, BLArrayCore* dst, const BLImageCodecCore* codec) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! \}

//! \name BLImage - C++ API
//! \{

#ifdef __cplusplus
//! 2D raster image [C++ API].
class BLImage : public BLImageCore {
public:
  //! \cond INTERNAL
  BL_INLINE BLImageImpl* _impl() const noexcept { return static_cast<BLImageImpl*>(_d.impl); }
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLImage() noexcept { blImageInit(this); }
  BL_INLINE BLImage(BLImage&& other) noexcept { blImageInitMove(this, &other); }
  BL_INLINE BLImage(const BLImage& other) noexcept { blImageInitWeak(this, &other); }
  BL_INLINE BLImage(int w, int h, BLFormat format) noexcept { blImageInitAs(this, w, h, format); }
  BL_INLINE ~BLImage() { blImageDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return !empty(); }

  BL_INLINE BLImage& operator=(BLImage&& other) noexcept { blImageAssignMove(this, &other); return *this; }
  BL_INLINE BLImage& operator=(const BLImage& other) noexcept { blImageAssignWeak(this, &other); return *this; }

  BL_NODISCARD BL_INLINE bool operator==(const BLImage& other) const noexcept { return  equals(other); }
  BL_NODISCARD BL_INLINE bool operator!=(const BLImage& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blImageReset(this); }

  BL_INLINE void swap(BLImage& other) noexcept { _d.swap(other._d); }

  BL_INLINE BLResult assign(BLImage&& other) noexcept { return blImageAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLImage& other) noexcept { return blImageAssignWeak(this, &other); }

  //! Create a deep copy of the `other` image.
  BL_INLINE BLResult assignDeep(const BLImage& other) noexcept { return blImageAssignDeep(this, &other); }

  //! Tests whether the image is empty (has no size).
  BL_NODISCARD
  BL_INLINE bool empty() const noexcept { return format() == BL_FORMAT_NONE; }

  BL_NODISCARD
  BL_INLINE bool equals(const BLImage& other) const noexcept { return blImageEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  //! Create a new image of a specified width `w`, height `h`, and `format`.
  //!
  //! \note It's important to always test whether the function succeeded as allocating pixel-data can fail. If invalid
  //! arguments (invalid size or format) were passed to the function a `BL_ERROR_INVALID_VALUE` result will be returned
  //! and no data will be allocated. It's also important to notice that `BLImage::create()` would not change anything
  //! if the function fails (the previous image content would be kept as is).
  BL_INLINE BLResult create(int w, int h, BLFormat format) noexcept {
    return blImageCreate(this, w, h, format);
  }

  //! Create a new image from external data.
  BL_INLINE BLResult createFromData(
    int w, int h, BLFormat format,
    void* pixelData, intptr_t stride,
    BLDestroyExternalDataFunc destroyFunc = nullptr,
    void* userData = nullptr) noexcept {

    return blImageCreateFromData(this, w, h, format, pixelData, stride, destroyFunc, userData);
  }

  //! \}

  //! \name Accessors
  //! \{

  //! Returns image width.
  BL_NODISCARD
  BL_INLINE int width() const noexcept { return _impl()->size.w; }

  //! Returns image height.
  BL_NODISCARD
  BL_INLINE int height() const noexcept { return _impl()->size.h; }

  //! Returns image size.
  BL_NODISCARD
  BL_INLINE BLSizeI size() const noexcept { return _impl()->size; }

  //! Returns image format, see `BLFormat`.
  BL_NODISCARD
  BL_INLINE BLFormat format() const noexcept { return BLFormat(_impl()->format); }

  //! Returns image depth, in bits.
  BL_NODISCARD
  BL_INLINE uint32_t depth() const noexcept { return _impl()->depth; }

  BL_INLINE BLResult getData(BLImageData* dataOut) const noexcept { return blImageGetData(this, dataOut); }
  BL_INLINE BLResult makeMutable() noexcept { BLImageData unused; return blImageMakeMutable(this, &unused); }
  BL_INLINE BLResult makeMutable(BLImageData* dataOut) noexcept { return blImageMakeMutable(this, dataOut); }

  //! \}

  //! \name Image Utilities
  //! \{

  BL_INLINE BLResult convert(BLFormat format) noexcept {
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

  BL_INLINE BLResult writeToFile(const char* fileName) noexcept {
    return blImageWriteToFile(this, fileName, nullptr);
  }

  BL_INLINE BLResult writeToFile(const char* fileName, const BLImageCodec& codec) noexcept {
    return blImageWriteToFile(this, fileName, reinterpret_cast<const BLImageCodecCore*>(&codec));
  }

  BL_INLINE BLResult writeToData(BLArray<uint8_t>& dst, const BLImageCodec& codec) noexcept {
    return blImageWriteToData(this, &dst, reinterpret_cast<const BLImageCodecCore*>(&codec));
  }

  //! \}

  static BL_INLINE BLResult scale(BLImage& dst, const BLImage& src, const BLSizeI& size, uint32_t filter, const BLImageScaleOptions* options = nullptr) noexcept {
    return blImageScale(&dst, &src, &size, filter, options);
  }
};

#endif
//! \}

//! \}

#endif // BLEND2D_IMAGE_H_INCLUDED
