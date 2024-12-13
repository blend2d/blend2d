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

//! \addtogroup bl_imaging
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

//! Data that describes a raster image. Used by \ref BLImage.
struct BLImageData {
  //! Pixel data, starting at the top left corner of the image.
  //!
  //! \note If the stride is negative the image data would start at the bottom.
  void* pixelData;

  //! Stride (in bytes) of image data (positive when image data starts at top-left, negative when it starts at
  //! bottom-left).
  intptr_t stride;

  //! Size of the image.
  BLSizeI size;
  //! Pixel format, see \ref BLFormat.
  uint32_t format;
  uint32_t flags;

#ifdef __cplusplus
  //! Resets the image data to represent an empty image (all members set to zeros).
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
//! \}

//! \addtogroup bl_c_api
//! \{

//! \name BLImage - C API
//! \{

//! 2D raster image [C API].
struct BLImageCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
  BL_DEFINE_OBJECT_DCAST(BLImage)
};

//! \cond INTERNAL
//! 2D raster image [C API Impl].
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
//! \endcond

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

//! \}
//! \}

//! \addtogroup bl_imaging
//! \{

//! \name BLImage - C++ API
//! \{

#ifdef __cplusplus
//! 2D raster image [C++ API].
//!
//! Raster image holds pixel data and additional information such as pixel format. The underlying image data can
//! be shared between multiple instances of \ref BLImage, which can be used by multiple threads. Atomic reference
//! counting is used to safely manage the internal reference count of the underlying image data.
//!
//! When an image is copied to another \ref BLImage instance its called a weak-copy as the underlying data is not
//! copied, but the reference count is increased instead (atomically). Since atomic operations involve a minor
//! overhead Blend2D implements also move operations, which are the most efficient operations that can be used to
//! move one instance to another.
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

  //! Creates a default constructed image, which is an empty image having pixel format equal to \ref BL_FORMAT_NONE.
  BL_INLINE_NODEBUG BLImage() noexcept { blImageInit(this); }

  //! Copy constructor creates a weak copy of `other` image by incrementing the reference count of the underlying
  //! representation.
  BL_INLINE_NODEBUG BLImage(const BLImage& other) noexcept { blImageInitWeak(this, &other); }

  //! Move constructor moves `other` image this this image and sets `other` image to a default constructed state.
  //!
  //! This is the most efficient operation as the reference count of the underlying image is not touched by a move
  //! operation.
  BL_INLINE_NODEBUG BLImage(BLImage&& other) noexcept { blImageInitMove(this, &other); }

  //! Creates a new image data of `[w, h]` size (specified in pixels) having the given pixel `format`.
  //!
  //! To create a valid image, both `w` and `h` must be greater than zero and the pixel `format` cannot be
  //! \ref BL_FORMAT_NONE.
  //!
  //! \note Since C++ cannot return values via constructors you should verify that the image was created by using
  //! either explicit `operator bool()` or verifying the image is not \ref empty().
  BL_INLINE_NODEBUG BLImage(int w, int h, BLFormat format) noexcept { blImageInitAs(this, w, h, format); }

  //! Destroys the image data held by the instance.
  //!
  //! The pixel data held by the image will only be deallocated if the reference count of the underlying representation
  //! gets decremented to zero.
  BL_INLINE_NODEBUG ~BLImage() {
    if (BLInternal::objectNeedsCleanup(_d.info.bits))
      blImageDestroy(this);
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  //! Returns whether the image holds any data, which means that both width and height are greater than zero and that
  //! the pixel format is not \ref BL_FORMAT_NONE (which would never be true if the size is non-zero).
  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return !empty(); }

  //! Copy assignment replaces the underlying data of this image with a weak-copy of the `other` image.
  BL_INLINE_NODEBUG BLImage& operator=(const BLImage& other) noexcept { blImageAssignWeak(this, &other); return *this; }
  //! Move assignment replaces the underlying data of this image with the `other` image and resets `other` image to
  //! a default constructed image. This operation is the fastest operation to change the underlying data of an image
  //! as moving doesn't involve reference counting in most cases.
  BL_INLINE_NODEBUG BLImage& operator=(BLImage&& other) noexcept { blImageAssignMove(this, &other); return *this; }

  //! Tests whether this image is equal with `other` image , see \ref equals() for more details about image equality.
  BL_NODISCARD BL_INLINE_NODEBUG bool operator==(const BLImage& other) const noexcept { return  equals(other); }
  //! Tests whether this image is not equal with `other` image , see \ref equals() for more details about image equality.
  BL_NODISCARD BL_INLINE_NODEBUG bool operator!=(const BLImage& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Resets the image to a default constructed image.
  //!
  //! A default constructed image has zero size and a pixel format equal to \ref BL_FORMAT_NONE. Such image is
  //! considered \ref empty() and holds no data that could be used by the rendering context or as a pattern.
  BL_INLINE_NODEBUG BLResult reset() noexcept { return blImageReset(this); }

  //! Swaps the underlying data with the `other` image.
  BL_INLINE_NODEBUG void swap(BLImage& other) noexcept { _d.swap(other._d); }

  //! Copy assignment replaces the underlying data of this image with `other`, see \ref operator=() for more details.
  BL_INLINE_NODEBUG BLResult assign(const BLImage& other) noexcept { return blImageAssignWeak(this, &other); }
  //! Move assignment replaces the underlying data of this image with `other` and resets `other` image to a default
  //! constructed state, see \ref operator=() for more details.
  BL_INLINE_NODEBUG BLResult assign(BLImage&& other) noexcept { return blImageAssignMove(this, &other); }

  //! Create a deep copy of the `other` image.
  BL_INLINE_NODEBUG BLResult assignDeep(const BLImage& other) noexcept { return blImageAssignDeep(this, &other); }

  //! Tests whether the image is empty (such image has no size and a pixel format is equal to \ref BL_FORMAT_NONE).
  //!
  //! \note You can use `operator bool()`, which does the reverse of \ref empty() - it checks whether the image is
  //! actually holding pixel data.
  BL_NODISCARD
  BL_INLINE_NODEBUG bool empty() const noexcept { return format() == BL_FORMAT_NONE; }

  //! Tests whether the image is equal to `other` image.
  //!
  //! Images are equal when the size, pixel format, and pixel data match. This means that this operation could be
  //! very expensive if the images are large.
  BL_NODISCARD
  BL_INLINE_NODEBUG bool equals(const BLImage& other) const noexcept { return blImageEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  //! Creates a new image of a specified width `w`, height `h`, and `format`.
  //!
  //! \note It's important to always test whether the function succeeded as allocating pixel-data can fail. If invalid
  //! arguments (invalid size or format) were passed to the function \ref BL_ERROR_INVALID_VALUE will be returned and
  //! no data will be allocated. It's also important to notice that \ref create() would not change anything if the
  //! function fails (the previous image content would be kept as is in such case).
  BL_INLINE_NODEBUG BLResult create(int w, int h, BLFormat format) noexcept {
    return blImageCreate(this, w, h, format);
  }

  //! Creates a new image from external data passed in `pixelData`.
  //!
  //! Blend2D can use pixel-data allocated outside of Blend2D, which is useful for rendering into images that can be
  //! allocated by other libraries such as AGG, Cairo, Qt, Windows API (DIBSECTION), etc.. The only thing that the user
  //! should pay extra attention to is the passed pixel `format` and `stride`.
  //!
  //! If the image data you are passing is read-only, pass \ref BL_DATA_ACCESS_READ in `accessFlags`, in that case
  //! Blend2D would never attempt to modify the passed data and would create a copy instead if such image gets modified.
  //!
  //! Additionally, if you would like to get notified about the destruction of the image (and thus Blend2D not holding
  //! the passed `pixelData` anymore, pass your own function in `destroyFunc` parameter with an optional `userData`).
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

  //! Returns image width (in pixels).
  BL_NODISCARD
  BL_INLINE_NODEBUG int width() const noexcept { return _impl()->size.w; }

  //! Returns image height (in pixels).
  BL_NODISCARD
  BL_INLINE_NODEBUG int height() const noexcept { return _impl()->size.h; }

  //! Returns image size (in pixels).
  BL_NODISCARD
  BL_INLINE_NODEBUG BLSizeI size() const noexcept { return _impl()->size; }

  //! Returns image format, see \ref BLFormat.
  //!
  //! \note When an image is \ref empty(), the pixel format returned is always \ref BL_FORMAT_NONE.
  BL_NODISCARD
  BL_INLINE_NODEBUG BLFormat format() const noexcept { return BLFormat(_impl()->format); }

  //! Returns image depth (in bits).
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

  //! Converts the image to a different pixel `format`.
  //!
  //! This operation could be lossy if the given pixel `format` has less channels than this image.
  //!
  //! \note If the image is \ref empty() the image format would not be changed. It will stay \ref BL_FORMAT_NONE
  //! and \ref BL_ERROR_NOT_INITIALIZED will be returned.
  BL_INLINE_NODEBUG BLResult convert(BLFormat format) noexcept {
    return blImageConvert(this, format);
  }

  //! \}

  //! \name Image IO
  //! \{

  //! Reads an image from a file specified by `fileName`.
  //!
  //! Image reader will automatically detect the image format by checking whether it's supported by available image
  //! codecs, which can be retrieved by \ref BLImageCodec::builtInCodecs().
  BL_INLINE_NODEBUG BLResult readFromFile(const char* fileName) noexcept {
    return blImageReadFromFile(this, fileName, nullptr);
  }

  //! Reads an image from a file specified by `fileName` by using image codecs passed via `codecs` parameter.
  //!
  //! Image reader will automatically detect the image format by checking whether it's supported by the passed image
  //! `codecs` - only codecs passed in will be considered.
  BL_INLINE_NODEBUG BLResult readFromFile(const char* fileName, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageReadFromFile(this, fileName, &codecs);
  }

  //! Reads an image from an existing byte-array starting at `data` and having `size` bytes.
  //!
  //! Image reader will automatically detect the image format by checking whether it's supported by available image
  //! codecs, which can be retrieved by \ref BLImageCodec::builtInCodecs().
  BL_INLINE_NODEBUG BLResult readFromData(const void* data, size_t size) noexcept {
    return blImageReadFromData(this, data, size, nullptr);
  }

  //! Reads an image from an existing byte-array starting at `data` and having `size` bytes by using image codecs
  //! passed via `codecs` parameter.
  //!
  //! Image reader will automatically detect the image format by checking whether it's supported by the passed image
  //! `codecs` - only codecs passed in will be considered.
  BL_INLINE_NODEBUG BLResult readFromData(const void* data, size_t size, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageReadFromData(this, data, size, &codecs);
  }

  //! Reads an image from an existing byte-buffer passed via `array`.
  //!
  //! Image reader will automatically detect the image format by checking whether it's supported by available image
  //! codecs, which can be retrieved by \ref BLImageCodec::builtInCodecs().
  BL_INLINE_NODEBUG BLResult readFromData(const BLArray<uint8_t>& array) noexcept {
    return blImageReadFromData(this, array.data(), array.size(), nullptr);
  }

  //! Reads an image from an existing byte-buffer passed via `array` by using image codecs passed via `codecs` parameter.
  //!
  //! Image reader will automatically detect the image format by checking whether it's supported by the passed image
  //! `codecs` - only codecs passed in will be considered.
  BL_INLINE_NODEBUG BLResult readFromData(const BLArray<uint8_t>& array, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageReadFromData(this, array.data(), array.size(), &codecs);
  }

  //! Reads an image from an existing byte-view passed via `view`.
  //!
  //! Image reader will automatically detect the image format by checking whether it's supported by available image
  //! codecs, which can be retrieved by \ref BLImageCodec::builtInCodecs().
  BL_INLINE_NODEBUG BLResult readFromData(const BLArrayView<uint8_t>& view) noexcept {
    return blImageReadFromData(this, view.data, view.size, nullptr);
  }

  //! Reads an image from an existing byte-view passed via `view` by using image codecs passed via `codecs` parameter.
  //!
  //! Image reader will automatically detect the image format by checking whether it's supported by the passed image
  //! `codecs` - only codecs passed in will be considered.
  BL_INLINE_NODEBUG BLResult readFromData(const BLArrayView<uint8_t>& view, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageReadFromData(this, view.data, view.size, &codecs);
  }

  //! Writes an encoded image to a file specified by `fileName`.
  //!
  //! Image writer detects the image codec by inspecting the extension of a file passed via `fileName`.
  BL_INLINE_NODEBUG BLResult writeToFile(const char* fileName) const noexcept {
    return blImageWriteToFile(this, fileName, nullptr);
  }

  //! Writes an encoded image to a file specified by `fileName` using the specified image `codec` to encode the image.
  BL_INLINE_NODEBUG BLResult writeToFile(const char* fileName, const BLImageCodec& codec) const noexcept {
    return blImageWriteToFile(this, fileName, reinterpret_cast<const BLImageCodecCore*>(&codec));
  }

  //! Writes an encoded image to a buffer `dst` using the specified image `codec` to encode the image.
  BL_INLINE_NODEBUG BLResult writeToData(BLArray<uint8_t>& dst, const BLImageCodec& codec) const noexcept {
    return blImageWriteToData(this, &dst, reinterpret_cast<const BLImageCodecCore*>(&codec));
  }

  //! \}

  //! Scales the `src` image to the specified `size` by using `filter` and writes the scaled image to `dst`.
  //!
  //! If the destination image `dst` doesn't match `size` and the source pixel format the underlying image data will
  //! be re-created.
  static BL_INLINE_NODEBUG BLResult scale(BLImage& dst, const BLImage& src, const BLSizeI& size, BLImageScaleFilter filter) noexcept {
    return blImageScale(&dst, &src, &size, filter);
  }
};

#endif
//! \}

//! \}

#endif // BLEND2D_IMAGE_H_INCLUDED
