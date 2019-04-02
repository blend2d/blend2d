// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLIMAGE_H
#define BLEND2D_BLIMAGE_H

#include "./blarray.h"
#include "./blformat.h"
#include "./blgeometry.h"
#include "./blvariant.h"

//! \addtogroup blend2d_api_images
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! Image codec feature bits.
BL_DEFINE_ENUM(BLImageCodecFeatures) {
  //! Image codec supports reading images (can create BLImageDecoder).
  BL_IMAGE_CODEC_FEATURE_READ                 = 0x00000001u,
  //! Image codec supports writing images (can create BLImageEncoder).
  BL_IMAGE_CODEC_FEATURE_WRITE                = 0x00000002u,
  //! Image codec supports lossless compression.
  BL_IMAGE_CODEC_FEATURE_LOSSLESS             = 0x00000004u,
  //! Image codec supports loosy compression.
  BL_IMAGE_CODEC_FEATURE_LOSSY                = 0x00000008u,
  //! Image codec supports writing multiple frames (GIF).
  BL_IMAGE_CODEC_FEATURE_MULTI_FRAME          = 0x00000010u,
  //! Image codec supports IPTC metadata.
  BL_IMAGE_CODEC_FEATURE_IPTC                 = 0x10000000u,
  //! Image codec supports EXIF metadata.
  BL_IMAGE_CODEC_FEATURE_EXIF                 = 0x20000000u,
  //! Image codec supports XMP metadata.
  BL_IMAGE_CODEC_FEATURE_XMP                  = 0x40000000u
};

//! Flags used by `BLImageInfo`.
BL_DEFINE_ENUM(BLImageInfoFlags) {
  //! Progressive mode.
  BL_IMAGE_INFO_FLAG_PROGRESSIVE              = 0x00000001u
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
// [Typedefs]
// ============================================================================

//! A user function that can be used by `BLImage::scale()`.
typedef BLResult (BL_CDECL* BLImageScaleUserFunc)(double* dst, const double* tArray, size_t n, const void* data) BL_NOEXCEPT;

// ============================================================================
// [BLImageData]
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
// [BLImageInfo]
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
// [BLImageScaleOptions]
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
  //! Image stride.
  intptr_t stride;
  //! Non-null if the image has a writer.
  volatile void* writer;

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

  //! \name Constructors and Destructors
  //! \{

  BL_INLINE BLImage() noexcept { this->impl = none().impl; }
  BL_INLINE BLImage(BLImage&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLImage(const BLImage& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLImage(BLImageImpl* impl) noexcept { this->impl = impl; }
  BL_INLINE BLImage(int w, int h, uint32_t format) noexcept { blImageInitAs(this, w, h, format); }
  BL_INLINE ~BLImage() { blImageReset(this); }

  BL_INLINE BLImage& operator=(BLImage&& other) noexcept { blImageAssignMove(this, &other); return *this; }
  BL_INLINE BLImage& operator=(const BLImage& other) noexcept { blImageAssignWeak(this, &other); return *this; }

  //! \}

  //! \name Operator Overloads
  //! \{

  BL_INLINE bool operator==(const BLImage& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLImage& other) const noexcept { return !equals(other); }

  BL_INLINE explicit operator bool() const noexcept { return !empty(); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blImageReset(this); }
  BL_INLINE void swap(BLImage& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult assign(BLImage&& other) noexcept { return blImageAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLImage& other) noexcept { return blImageAssignWeak(this, &other); }

  //! Create a deep copy of the `other` image.
  BL_INLINE BLResult assignDeep(const BLImage& other) noexcept { return blImageAssignDeep(this, &other); }

  //! Get whether the image is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }
  //! Get whether the image is empty (has no size).
  BL_INLINE bool empty() const noexcept { return impl->format == BL_FORMAT_NONE; }

  BL_INLINE bool equals(const BLImage& other) const noexcept { return blImageEquals(this, &other); }

  //! \}

  //! \name Create Image
  //! \{

  //! Create a new image of a specified width `w`, height `h`, and `format`.
  //!
  //! NOTE: It's important to always test whether the function succeeded as
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

  //! Get image width.
  BL_INLINE int width() const noexcept { return impl->size.w; }
  //! Get image height.
  BL_INLINE int height() const noexcept { return impl->size.h; }
  //! Get image size.
  BL_INLINE const BLSizeI& size() const noexcept { return impl->size; }
  //! Get image format, see `BLFormat`.
  BL_INLINE uint32_t format() const noexcept { return impl->format; }

  BL_INLINE BLResult getData(BLImageData* dataOut) const noexcept { return blImageGetData(this, dataOut); }
  BL_INLINE BLResult makeMutable() noexcept { BLImageData unused; return blImageMakeMutable(this, &unused); }
  BL_INLINE BLResult makeMutable(BLImageData* dataOut) noexcept { return blImageMakeMutable(this, dataOut); }

  //! \}

  //! \name Image IO
  //! \{

  BL_INLINE BLResult readFromFile(const char* fileName, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageReadFromFile(this, fileName, &codecs);
  }

  BL_INLINE BLResult readFromData(const void* data, size_t size, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageReadFromData(this, data, size, &codecs);
  }

  BL_INLINE BLResult readFromData(const BLArray<uint8_t>& array, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageReadFromData(this, array.data(), array.size(), &codecs);
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

// ============================================================================
// [BLImageCodec - Core]
// ============================================================================

//! Image codec [C Interface - Virtual Function Table].
struct BLImageCodecVirt {
  BLResult (BL_CDECL* destroy)(BLImageCodecImpl* impl) BL_NOEXCEPT;
  uint32_t (BL_CDECL* inspectData)(const BLImageCodecImpl* impl, const uint8_t* data, size_t size) BL_NOEXCEPT;
  BLResult (BL_CDECL* createDecoder)(const BLImageCodecImpl* impl, BLImageDecoderCore* dst) BL_NOEXCEPT;
  BLResult (BL_CDECL* createEncoder)(const BLImageCodecImpl* impl, BLImageEncoderCore* dst) BL_NOEXCEPT;
};

//! Image codec [C Interface - Impl].
struct BLImageCodecImpl {
  //! Virtual function table.
  const BLImageCodecVirt* virt;
  //! Image codec name like "PNG", "JPEG", etc...
  const char* name;
  //! Image codec vendor, built-in codecs use "Blend2D".
  const char* vendor;

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;

  //! Image codec features.
  uint32_t features;
  //! Mime type.
  const char* mimeType;
  //! Known file extensions used by this image codec separated by "|".
  const char* extensions;
};

//! Image codec [C Interface - Core].
struct BLImageCodecCore {
  BLImageCodecImpl* impl;
};

// ============================================================================
// [BLImageCodec - C++]
// ============================================================================

#ifdef __cplusplus
//! Image codec [C++ API].
//!
//! Provides a unified interface for inspecting image data and creating image
//! decoders & encoders.
class BLImageCodec : public BLImageCodecCore {
public:
  //! \cond INTERNAL
  static constexpr const uint32_t kImplType = BL_IMPL_TYPE_IMAGE_CODEC;
  //! \endcond

  BL_INLINE BLImageCodec() noexcept { this->impl = none().impl; }
  BL_INLINE BLImageCodec(BLImageCodec&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLImageCodec(const BLImageCodec& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLImageCodec(BLImageCodecImpl* impl) noexcept { this->impl = impl; }
  BL_INLINE ~BLImageCodec() { blImageCodecReset(this); }

  BL_INLINE BLImageCodec& operator=(const BLImageCodec& other) noexcept {
    blImageCodecAssignWeak(this, &other);
    return *this;
  }

  //! Get whether the image codec is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }
  //! Get the image codec name (i.e, "PNG", "JPEG", etc...).
  BL_INLINE const char* name() const noexcept { return impl->name; }
  //! Get the image codec vendor (i.e. "Blend2D" for all built-in codecs).
  BL_INLINE const char* vendor() const noexcept { return impl->vendor; }
  //! Get a mime-type associated with the image codec's format.
  BL_INLINE const char* mimeType() const noexcept { return impl->mimeType; }
  //! Get a list of file extensions used to store image of this codec, separated by '|' character.
  BL_INLINE const char* extensions() const noexcept { return impl->extensions; }
  //! Get image codec flags, see `BLImageCodecFeatures`.
  BL_INLINE uint32_t features() const noexcept { return impl->features; }
  //! Get whether the image codec has a flag `flag`.
  BL_INLINE bool hasFeature(uint32_t feature) const noexcept { return (impl->features & feature) != 0; }

  BL_INLINE BLResult reset() noexcept { return blImageCodecReset(this); }
  BL_INLINE BLResult assign(const BLImageCodec& other) noexcept { return blImageCodecAssignWeak(this, &other); }

  BL_INLINE BLResult findByName(const BLArray<BLImageCodec>& codecs, const char* name) noexcept { return blImageCodecFindByName(this, &codecs, name); }
  BL_INLINE BLResult findByData(const BLArray<BLImageCodec>& codecs, const void* data, size_t size) noexcept { return blImageCodecFindByData(this, &codecs, data, size); }
  BL_INLINE BLResult findByData(const BLArray<BLImageCodec>& codecs, const BLArrayView<uint8_t>& view) noexcept { return blImageCodecFindByData(this, &codecs, view.data, view.size); }
  BL_INLINE BLResult findByData(const BLArray<BLImageCodec>& codecs, const BLArray<uint8_t>& buffer) noexcept { return blImageCodecFindByData(this, &codecs, buffer.data(), buffer.size()); }

  BL_INLINE uint32_t inspectData(const BLArray<uint8_t>& buffer) const noexcept { return inspectData(buffer.view()); }
  BL_INLINE uint32_t inspectData(const BLArrayView<uint8_t>& view) const noexcept { return inspectData(view.data, view.size); }
  BL_INLINE uint32_t inspectData(const void* data, size_t size) const noexcept { return blImageCodecInspectData(this, static_cast<const uint8_t*>(data), size); }

  BL_INLINE BLResult createDecoder(BLImageDecoder* dst) const noexcept { return blImageCodecCreateDecoder(this, reinterpret_cast<BLImageDecoderCore*>(dst)); }
  BL_INLINE BLResult createEncoder(BLImageEncoder* dst) const noexcept { return blImageCodecCreateEncoder(this, reinterpret_cast<BLImageEncoderCore*>(dst)); }

  static BL_INLINE const BLImageCodec& none() noexcept { return reinterpret_cast<const BLImageCodec*>(blNone)[kImplType]; }
  static BL_INLINE BLArray<BLImageCodec>& builtInCodecs() noexcept { return *static_cast<BLArray<BLImageCodec>*>(blImageCodecBuiltInCodecs()); }
};
#endif

// ============================================================================
// [BLImageDecoder - Core]
// ============================================================================

//! Image decoder [C Interface - Virtual Function Table].
struct BLImageDecoderVirt {
  BLResult (BL_CDECL* destroy)(BLImageDecoderImpl* impl) BL_NOEXCEPT;
  BLResult (BL_CDECL* restart)(BLImageDecoderImpl* impl) BL_NOEXCEPT;
  BLResult (BL_CDECL* readInfo)(BLImageDecoderImpl* impl, BLImageInfo* infoOut, const uint8_t* data, size_t size) BL_NOEXCEPT;
  BLResult (BL_CDECL* readFrame)(BLImageDecoderImpl* impl, BLImageCore* imageOut, const uint8_t* data, size_t size) BL_NOEXCEPT;
};

//! Image decoder [C Interface - Impl].
struct BLImageDecoderImpl {
  //! Virtual function table.
  const BLImageDecoderVirt* virt;
  //! Image codec that created this decoder.
  BL_TYPED_MEMBER(BLImageCodecCore, BLImageCodec, codec);
  //! Handle in case that this decoder wraps a thirt-party library.
  void* handle;

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;

  //! Last faulty result (if failed).
  BLResult lastResult;
  //! Current frame index.
  uint64_t frameIndex;
  //! Position in source buffer.
  size_t bufferIndex;

  BL_HAS_TYPED_MEMBERS(BLImageDecoderImpl)
};

//! Image decoder [C Interface - Core]
struct BLImageDecoderCore {
  BLImageDecoderImpl* impl;
};

// ============================================================================
// [BLImageDecoder - C++]
// ============================================================================

#ifdef __cplusplus
//! Image decoder [C++ API].
class BLImageDecoder : public BLImageDecoderCore {
public:
  //! \cond INTERNAL
  static constexpr const uint32_t kImplType = BL_IMPL_TYPE_IMAGE_DECODER;
  //! \endcond

  BL_INLINE BLImageDecoder() noexcept { this->impl = none().impl; }
  BL_INLINE BLImageDecoder(BLImageDecoder&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLImageDecoder(const BLImageDecoder& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLImageDecoder(BLImageDecoderImpl* impl) noexcept { this->impl = impl; }
  BL_INLINE ~BLImageDecoder() { blImageDecoderReset(this); }

  BL_INLINE BLImageDecoder& operator=(BLImageDecoder&& other) noexcept { blImageDecoderAssignMove(this, &other); return *this; }
  BL_INLINE BLImageDecoder& operator=(const BLImageDecoder& other) noexcept { blImageDecoderAssignWeak(this, &other); return *this; }

  //! Get whether the image decoder is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }
  //! Get the last decoding result.
  BL_INLINE BLResult lastResult() const noexcept { return impl->lastResult; }
  //! Get position in source buffer.
  BL_INLINE uint64_t frameIndex() const noexcept { return impl->frameIndex; }
  //! Get position in source buffer.
  BL_INLINE size_t bufferIndex() const noexcept { return impl->bufferIndex; }

  BL_INLINE BLResult reset() noexcept { return blImageDecoderReset(this); }

  BL_INLINE BLResult assign(BLImageDecoder&& other) noexcept { return blImageDecoderAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLImageDecoder& other) noexcept { return blImageDecoderAssignWeak(this, &other); }

  BL_INLINE BLResult restart() noexcept { return blImageDecoderRestart(this); }

  BL_INLINE BLResult readInfo(BLImageInfo& dst, const BLArray<uint8_t>& buffer) noexcept { return blImageDecoderReadInfo(this, &dst, buffer.data(), buffer.size()); }
  BL_INLINE BLResult readInfo(BLImageInfo& dst, const BLArrayView<uint8_t>& view) noexcept { return blImageDecoderReadInfo(this, &dst, view.data, view.size); }
  BL_INLINE BLResult readInfo(BLImageInfo& dst, const void* data, size_t size) noexcept { return blImageDecoderReadInfo(this, &dst, static_cast<const uint8_t*>(data), size); }

  BL_INLINE BLResult readFrame(BLImage& dst, const BLArray<uint8_t>& buffer) noexcept { return blImageDecoderReadFrame(this, &dst, buffer.data(), buffer.size()); }
  BL_INLINE BLResult readFrame(BLImage& dst, const BLArrayView<uint8_t>& view) noexcept { return blImageDecoderReadFrame(this, &dst, view.data, view.size); }
  BL_INLINE BLResult readFrame(BLImage& dst, const void* data, size_t size) noexcept { return blImageDecoderReadFrame(this, &dst, static_cast<const uint8_t*>(data), size); }

  static BL_INLINE const BLImageDecoder& none() noexcept { return reinterpret_cast<const BLImageDecoder*>(blNone)[kImplType]; }
};
#endif

// ============================================================================
// [BLImageEncoder - Core]
// ============================================================================

//! Image encoder [C Interface - Virtual Function Table].
struct BLImageEncoderVirt {
  BLResult (BL_CDECL* destroy)(BLImageEncoderImpl* impl) BL_NOEXCEPT;
  BLResult (BL_CDECL* restart)(BLImageEncoderImpl* impl) BL_NOEXCEPT;
  BLResult (BL_CDECL* writeFrame)(BLImageEncoderImpl* impl, BLArrayCore* dst, const BLImageCore* image) BL_NOEXCEPT;
};

//! Image encoder [C Interface - Impl].
struct BLImageEncoderImpl {
  //! Virtual function table.
  const BLImageEncoderVirt* virt;
  //! Image codec that created this encoder.
  BL_TYPED_MEMBER(BLImageCodecCore, BLImageCodec, codec);
  //! Handle in case that this encoder wraps a thirt-party library.
  void* handle;

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;

  //! Last faulty result (if failed).
  BLResult lastResult;
  //! Current frame index.
  uint64_t frameIndex;
  //! Position in source buffer.
  size_t bufferIndex;

  BL_HAS_TYPED_MEMBERS(BLImageEncoderImpl)
};

//! Image encoder [C Interface - Core].
struct BLImageEncoderCore {
  BLImageEncoderImpl* impl;
};

// ============================================================================
// [BLImageEncoder - C++]
// ============================================================================

#ifdef __cplusplus
//! Image encoder [C++ API].
class BLImageEncoder : public BLImageEncoderCore {
public:
  //! \cond INTERNAL
  static constexpr const uint32_t kImplType = BL_IMPL_TYPE_IMAGE_ENCODER;
  //! \endcond

  BL_INLINE BLImageEncoder() noexcept { this->impl = none().impl; }
  BL_INLINE BLImageEncoder(BLImageEncoder&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLImageEncoder(const BLImageEncoder& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLImageEncoder(BLImageEncoderImpl* impl) noexcept { this->impl = impl; }
  BL_INLINE ~BLImageEncoder() { blImageEncoderReset(this); }

  BL_INLINE BLImageEncoder& operator=(BLImageEncoder&& other) noexcept { blImageEncoderAssignMove(this, &other); return *this; }
  BL_INLINE BLImageEncoder& operator=(const BLImageEncoder& other) noexcept { blImageEncoderAssignWeak(this, &other); return *this; }

  //! Get whether the image encoder is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }
  //! Get the last decoding result.
  BL_INLINE BLResult lastResult() const noexcept { return impl->lastResult; }
  //! Get position in source buffer.
  BL_INLINE uint64_t frameIndex() const noexcept { return impl->frameIndex; }
  //! Get position in source buffer.
  BL_INLINE size_t bufferIndex() const noexcept { return impl->bufferIndex; }

  BL_INLINE BLResult reset() noexcept { return blImageEncoderReset(this); }

  BL_INLINE BLResult assign(BLImageEncoder&& other) noexcept { return blImageEncoderAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLImageEncoder& other) noexcept { return blImageEncoderAssignWeak(this, &other); }

  BL_INLINE BLResult restart() noexcept { return blImageEncoderRestart(this); }

  //! Encode a given image `src`.
  BL_INLINE BLResult writeFrame(BLArray<uint8_t>& dst, const BLImage& image) noexcept { return blImageEncoderWriteFrame(this, &dst, &image); }

  static BL_INLINE const BLImageEncoder& none() noexcept { return reinterpret_cast<const BLImageEncoder*>(blNone)[kImplType]; }
};
#endif

//! \}

#endif // BLEND2D_BLIMAGE_H
