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
  //! Lanczos filter (radius 2.0).
  BL_IMAGE_SCALE_FILTER_LANCZOS = 4,

  //! Maximum value of `BLImageScaleFilter`.
  BL_IMAGE_SCALE_FILTER_MAX_VALUE = 4

  BL_FORCE_ENUM_UINT32(BL_IMAGE_SCALE_FILTER)
};

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
  BL_INLINE void reset() noexcept { *this = BLImageData{}; }
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
  BL_INLINE void reset() noexcept { *this = BLImageInfo{}; }
#endif
};

//! \}

//! \name BLImage - C API
//! \{

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blImageInit(BLImageCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageInitMove(BLImageCore* self, BLImageCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageInitWeak(BLImageCore* self, const BLImageCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageInitAs(BLImageCore* self, int w, int h, BLFormat format) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageInitAsFromData(BLImageCore* self, int w, int h, BLFormat format, void* pixelData, intptr_t stride, BLDataAccessFlags accessFlags, BLDestroyExternalDataFunc destroyFunc, void* userData) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageDestroy(BLImageCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageReset(BLImageCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageAssignMove(BLImageCore* self, BLImageCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageAssignWeak(BLImageCore* self, const BLImageCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageAssignDeep(BLImageCore* self, const BLImageCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCreate(BLImageCore* self, int w, int h, BLFormat format) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCreateFromData(BLImageCore* self, int w, int h, BLFormat format, void* pixelData, intptr_t stride, BLDataAccessFlags accessFlags, BLDestroyExternalDataFunc destroyFunc, void* userData) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageGetData(const BLImageCore* self, BLImageData* dataOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageMakeMutable(BLImageCore* self, BLImageData* dataOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageConvert(BLImageCore* self, BLFormat format) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blImageEquals(const BLImageCore* a, const BLImageCore* b) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageScale(BLImageCore* dst, const BLImageCore* src, const BLSizeI* size, BLImageScaleFilter filter) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageReadFromFile(BLImageCore* self, const char* fileName, const BLArrayCore* codecs) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageReadFromData(BLImageCore* self, const void* data, size_t size, const BLArrayCore* codecs) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageWriteToFile(const BLImageCore* self, const char* fileName, const BLImageCodecCore* codec) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageWriteToData(const BLImageCore* self, BLArrayCore* dst, const BLImageCodecCore* codec) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! 2D raster image [C API].
struct BLImageCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
  BL_DEFINE_OBJECT_DCAST(BLImage)
};

//! \}

//! \cond INTERNAL
//! \name BLImage - Internals
//! \{

//! 2D raster image [Impl].
struct BLImageImpl BL_CLASS_INHERITS(BLObjectImpl) {
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

//! \}
//! \endcond

//! \name BLImage - C++ API
//! \{

#ifdef __cplusplus
//! 2D raster image [C++ API].
class BLImage final : public BLImageCore {
public:
  //! \cond INTERNAL
  //! \name Internals
  //! \{

  BL_INLINE_NODEBUG BLImageImpl* _impl() const noexcept { return static_cast<BLImageImpl*>(_d.impl); }

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLImage() noexcept { blImageInit(this); }
  BL_INLINE_NODEBUG BLImage(BLImage&& other) noexcept { blImageInitMove(this, &other); }
  BL_INLINE_NODEBUG BLImage(const BLImage& other) noexcept { blImageInitWeak(this, &other); }
  BL_INLINE_NODEBUG BLImage(int w, int h, BLFormat format) noexcept { blImageInitAs(this, w, h, format); }

  BL_INLINE_NODEBUG ~BLImage() {
    if (BLInternal::objectNeedsCleanup(_d.info.bits))
      blImageDestroy(this);
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return !empty(); }

  BL_INLINE_NODEBUG BLImage& operator=(BLImage&& other) noexcept { blImageAssignMove(this, &other); return *this; }
  BL_INLINE_NODEBUG BLImage& operator=(const BLImage& other) noexcept { blImageAssignWeak(this, &other); return *this; }

  BL_NODISCARD BL_INLINE_NODEBUG bool operator==(const BLImage& other) const noexcept { return  equals(other); }
  BL_NODISCARD BL_INLINE_NODEBUG bool operator!=(const BLImage& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult reset() noexcept { return blImageReset(this); }

  BL_INLINE_NODEBUG void swap(BLImage& other) noexcept { _d.swap(other._d); }

  BL_INLINE_NODEBUG BLResult assign(BLImage&& other) noexcept { return blImageAssignMove(this, &other); }
  BL_INLINE_NODEBUG BLResult assign(const BLImage& other) noexcept { return blImageAssignWeak(this, &other); }

  //! Create a deep copy of the `other` image.
  BL_INLINE_NODEBUG BLResult assignDeep(const BLImage& other) noexcept { return blImageAssignDeep(this, &other); }

  //! Tests whether the image is empty (has no size).
  BL_NODISCARD
  BL_INLINE_NODEBUG bool empty() const noexcept { return format() == BL_FORMAT_NONE; }

  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLImage& other) const noexcept { return blImageEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  //! Create a new image of a specified width `w`, height `h`, and `format`.
  //!
  //! \note It's important to always test whether the function succeeded as allocating pixel-data can fail. If invalid
  //! arguments (invalid size or format) were passed to the function a `BL_ERROR_INVALID_VALUE` result will be returned
  //! and no data will be allocated. It's also important to notice that `BLImage::create()` would not change anything
  //! if the function fails (the previous image content would be kept as is).
  BL_INLINE_NODEBUG BLResult create(int w, int h, BLFormat format) noexcept {
    return blImageCreate(this, w, h, format);
  }

  //! Create a new image from external data.
  BL_INLINE_NODEBUG BLResult createFromData(
      int w, int h, BLFormat format,
      void* pixelData, intptr_t stride,
      BLDataAccessFlags accessFlags = BL_DATA_ACCESS_RW,
      BLDestroyExternalDataFunc destroyFunc = nullptr, void* userData = nullptr) noexcept {
    return blImageCreateFromData(this, w, h, format, pixelData, stride, accessFlags, destroyFunc, userData);
  }

  //! \}

  //! \name Accessors
  //! \{

  //! Returns image width.
  BL_NODISCARD
  BL_INLINE_NODEBUG int width() const noexcept { return _impl()->size.w; }

  //! Returns image height.
  BL_NODISCARD
  BL_INLINE_NODEBUG int height() const noexcept { return _impl()->size.h; }

  //! Returns image size.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLSizeI size() const noexcept { return _impl()->size; }

  //! Returns image format, see `BLFormat`.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLFormat format() const noexcept { return BLFormat(_impl()->format); }

  //! Returns image depth, in bits.
  BL_NODISCARD
  BL_INLINE_NODEBUG uint32_t depth() const noexcept { return _impl()->depth; }

  //! Returns immutable in `dataOut`, which contains pixel pointer, stride, and other image properties like size and
  //! pixel format.
  //!
  //! \note Although the data is filled in \ref BLImageData, which holds a non-const `pixelData` pointer, the data is
  //! immutable. If you intend to modify the data, use \ref makeMutable() function instead, which would copy the image
  //! data if it's shared with another BLImage instance.
  BL_INLINE_NODEBUG BLResult getData(BLImageData* dataOut) const noexcept { return blImageGetData(this, dataOut); }

  //! Makes the image data mutable and returns them in `dataOut`.
  BL_INLINE_NODEBUG BLResult makeMutable(BLImageData* dataOut) noexcept { return blImageMakeMutable(this, dataOut); }

  //! \}

  //! \name Image Utilities
  //! \{

  BL_INLINE_NODEBUG BLResult convert(BLFormat format) noexcept {
    return blImageConvert(this, format);
  }

  //! \}

  //! \name Image IO
  //! \{

  BL_INLINE_NODEBUG BLResult readFromFile(const char* fileName) noexcept {
    return blImageReadFromFile(this, fileName, nullptr);
  }

  BL_INLINE_NODEBUG BLResult readFromFile(const char* fileName, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageReadFromFile(this, fileName, &codecs);
  }

  BL_INLINE_NODEBUG BLResult readFromData(const void* data, size_t size) noexcept {
    return blImageReadFromData(this, data, size, nullptr);
  }

  BL_INLINE_NODEBUG BLResult readFromData(const void* data, size_t size, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageReadFromData(this, data, size, &codecs);
  }

  BL_INLINE_NODEBUG BLResult readFromData(const BLArray<uint8_t>& array) noexcept {
    return blImageReadFromData(this, array.data(), array.size(), nullptr);
  }

  BL_INLINE_NODEBUG BLResult readFromData(const BLArray<uint8_t>& array, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageReadFromData(this, array.data(), array.size(), &codecs);
  }

  BL_INLINE_NODEBUG BLResult readFromData(const BLArrayView<uint8_t>& view) noexcept {
    return blImageReadFromData(this, view.data, view.size, nullptr);
  }

  BL_INLINE_NODEBUG BLResult readFromData(const BLArrayView<uint8_t>& view, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageReadFromData(this, view.data, view.size, &codecs);
  }

  BL_INLINE_NODEBUG BLResult writeToFile(const char* fileName) const noexcept {
    return blImageWriteToFile(this, fileName, nullptr);
  }

  BL_INLINE_NODEBUG BLResult writeToFile(const char* fileName, const BLImageCodec& codec) const noexcept {
    return blImageWriteToFile(this, fileName, reinterpret_cast<const BLImageCodecCore*>(&codec));
  }

  BL_INLINE_NODEBUG BLResult writeToData(BLArray<uint8_t>& dst, const BLImageCodec& codec) const noexcept {
    return blImageWriteToData(this, &dst, reinterpret_cast<const BLImageCodecCore*>(&codec));
  }

  //! \}

  static BL_INLINE_NODEBUG BLResult scale(BLImage& dst, const BLImage& src, const BLSizeI& size, BLImageScaleFilter filter) noexcept {
    return blImageScale(&dst, &src, &size, filter);
  }
};

#endif
//! \}

//! \}

#endif // BLEND2D_IMAGE_H_INCLUDED
