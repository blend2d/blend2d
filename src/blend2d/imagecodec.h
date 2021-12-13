// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_IMAGECODEC_H_INCLUDED
#define BLEND2D_IMAGECODEC_H_INCLUDED

#include "array.h"
#include "object.h"
#include "string.h"

//! \addtogroup blend2d_api_imaging
//! \{

//! \name BLImageCodec - Constants
//! \{

//! Image codec feature bits.
BL_DEFINE_ENUM(BLImageCodecFeatures) {
  //! No features.
  BL_IMAGE_CODEC_NO_FEATURES = 0u,
  //! Image codec supports reading images (can create BLImageDecoder).
  BL_IMAGE_CODEC_FEATURE_READ = 0x00000001u,
  //! Image codec supports writing images (can create BLImageEncoder).
  BL_IMAGE_CODEC_FEATURE_WRITE = 0x00000002u,
  //! Image codec supports lossless compression.
  BL_IMAGE_CODEC_FEATURE_LOSSLESS = 0x00000004u,
  //! Image codec supports loosy compression.
  BL_IMAGE_CODEC_FEATURE_LOSSY = 0x00000008u,
  //! Image codec supports writing multiple frames (GIF).
  BL_IMAGE_CODEC_FEATURE_MULTI_FRAME = 0x00000010u,
  //! Image codec supports IPTC metadata.
  BL_IMAGE_CODEC_FEATURE_IPTC = 0x10000000u,
  //! Image codec supports EXIF metadata.
  BL_IMAGE_CODEC_FEATURE_EXIF = 0x20000000u,
  //! Image codec supports XMP metadata.
  BL_IMAGE_CODEC_FEATURE_XMP = 0x40000000u

  BL_FORCE_ENUM_UINT32(BL_IMAGE_CODEC_FEATURE)
};

//! \}

//! \name BLImageCodec - C API
//! \{

//! Image codec [Virtual Function Table].
struct BLImageCodecVirt BL_CLASS_INHERITS(BLObjectVirt) {
  BL_DEFINE_VIRT_BASE

  uint32_t (BL_CDECL* inspectData)(const BLImageCodecImpl* impl, const uint8_t* data, size_t size) BL_NOEXCEPT;
  BLResult (BL_CDECL* createDecoder)(const BLImageCodecImpl* impl, BLImageDecoderCore* dst) BL_NOEXCEPT;
  BLResult (BL_CDECL* createEncoder)(const BLImageCodecImpl* impl, BLImageEncoderCore* dst) BL_NOEXCEPT;
};

//! Image codec [Impl].
struct BLImageCodecImpl BL_CLASS_INHERITS(BLObjectImpl) {
  BL_HAS_TYPED_MEMBERS(BLImageCodecImpl)

  //! \name Members
  //! \{

  //! Virtual function table.
  const BLImageCodecVirt* virt;

  //! Image codec name like "PNG", "JPEG", etc...
  BL_TYPED_MEMBER(BLStringCore, BLString, name);
  //! Image codec vendor, built-in codecs use "Blend2D".
  BL_TYPED_MEMBER(BLStringCore, BLString, vendor);
  //! Mime type.
  BL_TYPED_MEMBER(BLStringCore, BLString, mimeType);
  //! Known file extensions used by this image codec separated by "|".
  BL_TYPED_MEMBER(BLStringCore, BLString, extensions);

  //! Image codec features.
  uint32_t features;

  //! \}

#ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  //! Explicit constructor that constructs this Impl.
  BL_INLINE void ctor(const BLImageCodecVirt* virt_) noexcept {
    virt = virt_;
    blCallCtor(name);
    blCallCtor(vendor);
    blCallCtor(mimeType);
    blCallCtor(extensions);
    features = 0;
  }

  //! Explicit destructor that destructs this Impl.
  BL_INLINE void dtor() noexcept {
    blCallDtor(name);
    blCallDtor(vendor);
    blCallDtor(mimeType);
    blCallDtor(extensions);
  }

  //! \}
#endif
};

//! Image codec [C API].
struct BLImageCodecCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL

#ifdef __cplusplus
  //! \name Impl Utilities
  //! \{

  //! Returns Impl of the image codec (only provided for use cases that implement BLImageCodec).
  template<typename T = BLImageCodecImpl>
  BL_INLINE T* _impl() const noexcept { return static_cast<T*>(_d.impl); }

  //! \}
#endif
};

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blImageCodecInit(BLImageCodecCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCodecInitMove(BLImageCodecCore* self, BLImageCodecCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCodecInitWeak(BLImageCodecCore* self, const BLImageCodecCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCodecInitByName(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCodecDestroy(BLImageCodecCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCodecReset(BLImageCodecCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCodecAssignMove(BLImageCodecCore* self, BLImageCodecCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCodecAssignWeak(BLImageCodecCore* self, const BLImageCodecCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCodecFindByName(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCodecFindByExtension(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCodecFindByData(BLImageCodecCore* self, const void* data, size_t size, const BLArrayCore* codecs) BL_NOEXCEPT_C;
BL_API uint32_t BL_CDECL blImageCodecInspectData(const BLImageCodecCore* self, const void* data, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCodecCreateDecoder(const BLImageCodecCore* self, BLImageDecoderCore* dst) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCodecCreateEncoder(const BLImageCodecCore* self, BLImageEncoderCore* dst) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blImageCodecArrayInitBuiltInCodecs(BLArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCodecArrayAssignBuiltInCodecs(BLArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCodecAddToBuiltIn(const BLImageCodecCore* codec) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageCodecRemoveFromBuiltIn(const BLImageCodecCore* codec) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blImageDecoderInit(BLImageDecoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageDecoderInitMove(BLImageDecoderCore* self, BLImageDecoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageDecoderInitWeak(BLImageDecoderCore* self, const BLImageDecoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageDecoderDestroy(BLImageDecoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageDecoderReset(BLImageDecoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageDecoderAssignMove(BLImageDecoderCore* self, BLImageDecoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageDecoderAssignWeak(BLImageDecoderCore* self, const BLImageDecoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageDecoderRestart(BLImageDecoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageDecoderReadInfo(BLImageDecoderCore* self, BLImageInfo* infoOut, const uint8_t* data, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageDecoderReadFrame(BLImageDecoderCore* self, BLImageCore* imageOut, const uint8_t* data, size_t size) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blImageEncoderInit(BLImageEncoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageEncoderInitMove(BLImageEncoderCore* self, BLImageEncoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageEncoderInitWeak(BLImageEncoderCore* self, const BLImageEncoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageEncoderDestroy(BLImageEncoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageEncoderReset(BLImageEncoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageEncoderAssignMove(BLImageEncoderCore* self, BLImageEncoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageEncoderAssignWeak(BLImageEncoderCore* self, const BLImageEncoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageEncoderRestart(BLImageEncoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blImageEncoderWriteFrame(BLImageEncoderCore* self, BLArrayCore* dst, const BLImageCore* image) BL_NOEXCEPT_C;

BL_END_C_DECLS
//! \}

//! \name BLImageCodec - C++ API
//! \{
#ifdef __cplusplus

//! Image codec [C++ API].
//!
//! Provides a unified interface for inspecting image data and creating image
//! decoders & encoders.
class BLImageCodec : public BLImageCodecCore {
public:
  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLImageCodec() noexcept { blImageCodecInit(this); }
  BL_INLINE BLImageCodec(BLImageCodec&& other) noexcept { blImageCodecInitMove(this, &other); }
  BL_INLINE BLImageCodec(const BLImageCodec& other) noexcept { blImageCodecInitWeak(this, &other); }
  BL_INLINE ~BLImageCodec() { blImageCodecDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return isValid(); }

  BL_INLINE BLImageCodec& operator=(const BLImageCodec& other) noexcept {
    blImageCodecAssignWeak(this, &other);
    return *this;
  }

  BL_NODISCARD BL_INLINE bool operator==(const BLImageCodec& other) const noexcept { return  equals(other); }
  BL_NODISCARD BL_INLINE bool operator!=(const BLImageCodec& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blImageCodecReset(this); }
  BL_INLINE void swap(BLImageCodecCore& other) noexcept { _d.swap(other._d); }

  BL_INLINE BLResult assign(const BLImageCodecCore& other) noexcept { return blImageCodecAssignWeak(this, &other); }

  //! Tests whether the image codec is a built-in null instance.
  BL_NODISCARD
  BL_INLINE bool isValid() const noexcept { return (_impl()->features & (BL_IMAGE_CODEC_FEATURE_READ | BL_IMAGE_CODEC_FEATURE_WRITE)) != 0; }

  BL_NODISCARD
  BL_INLINE bool equals(const BLImageCodecCore& other) const noexcept { return _d.impl == other._d.impl; }

  //! \}

  //! \name Accessors
  //! \{

  //! Returns image codec name (i.e, "PNG", "JPEG", etc...).
  BL_NODISCARD
  BL_INLINE const BLString& name() const noexcept { return _impl()->name; }

  //! Returns the image codec vendor (i.e. "Blend2D" for all built-in codecs).
  BL_NODISCARD
  BL_INLINE const BLString& vendor() const noexcept { return _impl()->vendor; }

  //! Returns a mime-type associated with the image codec's format.
  BL_NODISCARD
  BL_INLINE const BLString& mimeType() const noexcept { return _impl()->mimeType; }

  //! Returns a list of file extensions used to store image of this codec, separated by '|' character.
  BL_NODISCARD
  BL_INLINE const BLString& extensions() const noexcept { return _impl()->extensions; }

  //! Returns image codec flags, see `BLImageCodecFeatures`.
  BL_NODISCARD
  BL_INLINE BLImageCodecFeatures features() const noexcept { return (BLImageCodecFeatures)_impl()->features; }

  //! Tests whether the image codec has a flag `flag`.
  BL_NODISCARD
  BL_INLINE bool hasFeature(BLImageCodecFeatures feature) const noexcept { return (_impl()->features & feature) != 0; }

  //! \}

  //! \name Properties
  //! \{

  BL_DEFINE_OBJECT_PROPERTY_API

  //! \}

  //! \name Find Functionality
  //! \{

  BL_INLINE BLResult findByName(const char* name) noexcept {
    return blImageCodecFindByName(this, name, SIZE_MAX, nullptr);
  }

  BL_INLINE BLResult findByName(const char* name, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageCodecFindByName(this, name, SIZE_MAX, &codecs);
  }

  BL_INLINE BLResult findByName(const BLStringView& name) noexcept {
    return blImageCodecFindByName(this, name.data, name.size, nullptr);
  }

  BL_INLINE BLResult findByName(const BLStringView& name, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageCodecFindByName(this, name.data, name.size, &codecs);
  }

  BL_INLINE BLResult findByExtension(const char* name) noexcept {
    return blImageCodecFindByExtension(this, name, SIZE_MAX, nullptr);
  }

  BL_INLINE BLResult findByExtension(const char* name, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageCodecFindByExtension(this, name, SIZE_MAX, &codecs);
  }

  BL_INLINE BLResult findByExtension(const BLStringView& name) noexcept {
    return blImageCodecFindByExtension(this, name.data, name.size, nullptr);
  }

  BL_INLINE BLResult findByExtension(const BLStringView& name, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageCodecFindByExtension(this, name.data, name.size, &codecs);
  }

  BL_INLINE BLResult findByData(const void* data, size_t size) noexcept {
    return blImageCodecFindByData(this, data, size, nullptr);
  }

  BL_INLINE BLResult findByData(const void* data, size_t size, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageCodecFindByData(this, data, size, &codecs);
  }

  BL_INLINE BLResult findByData(const BLArrayView<uint8_t>& view) noexcept {
    return blImageCodecFindByData(this, view.data, view.size, nullptr);
  }

  BL_INLINE BLResult findByData(const BLArrayView<uint8_t>& view, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageCodecFindByData(this, view.data, view.size, &codecs);
  }

  BL_INLINE BLResult findByData(const BLArray<uint8_t>& buffer) noexcept {
    return blImageCodecFindByData(this, buffer.data(), buffer.size(), nullptr);
  }

  BL_INLINE BLResult findByData(const BLArray<uint8_t>& buffer, const BLArray<BLImageCodec>& codecs) noexcept {
    return blImageCodecFindByData(this, buffer.data(), buffer.size(), &codecs);
  }

  //! \}

  //! \name Codec Functionality
  //! \{

  BL_NODISCARD
  BL_INLINE uint32_t inspectData(const BLArray<uint8_t>& buffer) const noexcept { return inspectData(buffer.view()); }

  BL_NODISCARD
  BL_INLINE uint32_t inspectData(const BLArrayView<uint8_t>& view) const noexcept { return inspectData(view.data, view.size); }

  BL_NODISCARD
  BL_INLINE uint32_t inspectData(const void* data, size_t size) const noexcept { return blImageCodecInspectData(this, static_cast<const uint8_t*>(data), size); }

  BL_INLINE BLResult createDecoder(BLImageDecoderCore* dst) const noexcept { return blImageCodecCreateDecoder(this, reinterpret_cast<BLImageDecoderCore*>(dst)); }
  BL_INLINE BLResult createEncoder(BLImageEncoderCore* dst) const noexcept { return blImageCodecCreateEncoder(this, reinterpret_cast<BLImageEncoderCore*>(dst)); }

  //! \}

  //! \name Built-In Codecs
  //! \{

  static BL_INLINE BLArray<BLImageCodec> builtInCodecs() noexcept {
    BLArray<BLImageCodec> result;
    blImageCodecArrayInitBuiltInCodecs(&result);
    return result;
  }

  static BL_INLINE BLResult addToBuiltIn(const BLImageCodecCore& codec) noexcept {
    return blImageCodecAddToBuiltIn(&codec);
  }

  static BL_INLINE BLResult removeFromBuiltIn(const BLImageCodecCore& codec) noexcept {
    return blImageCodecRemoveFromBuiltIn(&codec);
  }

  //! \}
};

#endif
//! \}

//! \name BLImageCodec - C API
//! \{

//! Image decoder [Virtual Function Table].
struct BLImageDecoderVirt BL_CLASS_INHERITS(BLObjectVirt) {
  BL_DEFINE_VIRT_BASE

  BLResult (BL_CDECL* restart)(BLImageDecoderImpl* impl) BL_NOEXCEPT;
  BLResult (BL_CDECL* readInfo)(BLImageDecoderImpl* impl, BLImageInfo* infoOut, const uint8_t* data, size_t size) BL_NOEXCEPT;
  BLResult (BL_CDECL* readFrame)(BLImageDecoderImpl* impl, BLImageCore* imageOut, const uint8_t* data, size_t size) BL_NOEXCEPT;
};

//! Image decoder [Impl].
struct BLImageDecoderImpl BL_CLASS_INHERITS(BLObjectImpl) {
  BL_HAS_TYPED_MEMBERS(BLImageDecoderImpl)

  //! \name Members
  //! \{

  //! Virtual function table.
  const BLImageDecoderVirt* virt;

  //! Image codec that created this decoder.
  BL_TYPED_MEMBER(BLImageCodecCore, BLImageCodec, codec);

  //! Last faulty result (if failed).
  BLResult lastResult;

  //! Handle in case that this decoder wraps a thirt-party library.
  void* handle;
  //! Current frame index.
  uint64_t frameIndex;
  //! Position in source buffer.
  size_t bufferIndex;

  //! \}

#ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  //! Explicit constructor that constructs this Impl.
  BL_INLINE void ctor(const BLImageDecoderVirt* virt_, const BLImageCodecCore* codec_) noexcept {
    virt = virt_;
    blCallCtor(codec, *static_cast<const BLImageCodec*>(codec_));
    lastResult = BL_SUCCESS;
    handle = nullptr;
    bufferIndex = 0;
    frameIndex = 0;
  }

  //! Explicit destructor that destructs this Impl.
  BL_INLINE void dtor() noexcept {
    blCallDtor(codec);
  }

  //! \}
#endif
};

//! Image decoder [C API]
struct BLImageDecoderCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL

#ifdef __cplusplus
  //! \name Impl Utilities
  //! \{

  //! Returns Impl of the image decoder (only provided for use cases that implement BLImageDecoder).
  template<typename T = BLImageDecoderImpl>
  BL_INLINE T* _impl() const noexcept { return static_cast<T*>(_d.impl); }

  //! \}
#endif
};

//! \}

//! \name BLImageCodec - C++ API
//! \{
#ifdef __cplusplus

//! Image decoder [C++ API].
class BLImageDecoder : public BLImageDecoderCore {
public:
  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLImageDecoder() noexcept { blImageDecoderInit(this); }
  BL_INLINE BLImageDecoder(BLImageDecoder&& other) noexcept { blImageDecoderInitMove(this, &other); }
  BL_INLINE BLImageDecoder(const BLImageDecoder& other) noexcept { blImageDecoderInitWeak(this, &other); }
  BL_INLINE ~BLImageDecoder() { blImageDecoderDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return isValid(); }

  BL_INLINE BLImageDecoder& operator=(BLImageDecoder&& other) noexcept { blImageDecoderAssignMove(this, &other); return *this; }
  BL_INLINE BLImageDecoder& operator=(const BLImageDecoder& other) noexcept { blImageDecoderAssignWeak(this, &other); return *this; }

  BL_NODISCARD BL_INLINE bool operator==(const BLImageDecoder& other) const noexcept { return  equals(other); }
  BL_NODISCARD BL_INLINE bool operator!=(const BLImageDecoder& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blImageDecoderReset(this); }
  BL_INLINE void swap(BLImageDecoderCore& other) noexcept { _d.swap(other._d); }

  BL_INLINE BLResult assign(BLImageDecoderCore&& other) noexcept { return blImageDecoderAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLImageDecoderCore& other) noexcept { return blImageDecoderAssignWeak(this, &other); }

  //! Tests whether the image decoder is a built-in null instance.
  BL_NODISCARD
  BL_INLINE bool isValid() const noexcept { return _impl()->lastResult != BL_ERROR_NOT_INITIALIZED; }

  BL_NODISCARD
  BL_INLINE bool equals(const BLImageDecoderCore& other) const noexcept { return _d.impl == other._d.impl; }

  //! \}

  //! \name Accessors
  //! \{

  BL_NODISCARD
  BL_INLINE BLImageCodec& codec() const noexcept { return _impl()->codec; }

  //! Returns the last decoding result.
  BL_NODISCARD
  BL_INLINE BLResult lastResult() const noexcept { return _impl()->lastResult; }

  //! Returns the current frame index (to be decoded).
  BL_NODISCARD
  BL_INLINE uint64_t frameIndex() const noexcept { return _impl()->frameIndex; }

  //! Returns the position in source buffer.
  BL_NODISCARD
  BL_INLINE size_t bufferIndex() const noexcept { return _impl()->bufferIndex; }

  //! \}

  //! \name Properties
  //! \{

  BL_DEFINE_OBJECT_PROPERTY_API

  //! \}

  //! \name Decoder Functionality
  //! \{

  BL_INLINE BLResult restart() noexcept { return blImageDecoderRestart(this); }

  BL_INLINE BLResult readInfo(BLImageInfo& dst, const BLArray<uint8_t>& buffer) noexcept { return blImageDecoderReadInfo(this, &dst, buffer.data(), buffer.size()); }
  BL_INLINE BLResult readInfo(BLImageInfo& dst, const BLArrayView<uint8_t>& view) noexcept { return blImageDecoderReadInfo(this, &dst, view.data, view.size); }
  BL_INLINE BLResult readInfo(BLImageInfo& dst, const void* data, size_t size) noexcept { return blImageDecoderReadInfo(this, &dst, static_cast<const uint8_t*>(data), size); }

  BL_INLINE BLResult readFrame(BLImageCore& dst, const BLArray<uint8_t>& buffer) noexcept { return blImageDecoderReadFrame(this, &dst, buffer.data(), buffer.size()); }
  BL_INLINE BLResult readFrame(BLImageCore& dst, const BLArrayView<uint8_t>& view) noexcept { return blImageDecoderReadFrame(this, &dst, view.data, view.size); }
  BL_INLINE BLResult readFrame(BLImageCore& dst, const void* data, size_t size) noexcept { return blImageDecoderReadFrame(this, &dst, static_cast<const uint8_t*>(data), size); }

  //! \}
};

#endif
//! \}

//! \name BLImageCodec - C API
//! \{

//! Image encoder [Virtual Function Table].
struct BLImageEncoderVirt BL_CLASS_INHERITS(BLObjectVirt) {
  BL_DEFINE_VIRT_BASE

  BLResult (BL_CDECL* restart)(BLImageEncoderImpl* impl) BL_NOEXCEPT;
  BLResult (BL_CDECL* writeFrame)(BLImageEncoderImpl* impl, BLArrayCore* dst, const BLImageCore* image) BL_NOEXCEPT;
};

//! Image encoder [Impl].
struct BLImageEncoderImpl BL_CLASS_INHERITS(BLObjectImpl) {
  BL_HAS_TYPED_MEMBERS(BLImageEncoderImpl)

  //! \name Members
  //! \{

  //! Virtual function table.
  const BLImageEncoderVirt* virt;

  //! Image codec that created this encoder.
  BL_TYPED_MEMBER(BLImageCodecCore, BLImageCodec, codec);

  //! Last faulty result (if failed).
  BLResult lastResult;

  //! Handle in case that this encoder wraps a thirt-party library.
  void* handle;
  //! Current frame index.
  uint64_t frameIndex;
  //! Position in source buffer.
  size_t bufferIndex;

  //! \}

#ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  //! Explicit constructor that constructs this Impl.
  BL_INLINE void ctor(const BLImageEncoderVirt* virt_, const BLImageCodecCore* codec_) noexcept {
    virt = virt_;
    blCallCtor(codec, *static_cast<const BLImageCodec*>(codec_));
    lastResult = BL_SUCCESS;
    handle = nullptr;
    bufferIndex = 0;
    frameIndex = 0;
  }

  //! Explicit destructor that destructs this Impl.
  BL_INLINE void dtor() noexcept {
    blCallDtor(codec);
  }

  //! \}
#endif
};

//! Image encoder [C API].
struct BLImageEncoderCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL

#ifdef __cplusplus
  //! \name Impl Utilities
  //! \{

  //! Returns Impl of the image encoder (only provided for use cases that implement BLImageEncoder).
  template<typename T = BLImageEncoderImpl>
  BL_INLINE T* _impl() const noexcept { return static_cast<T*>(_d.impl); }

  //! \}
#endif
};

//! \}

//! \name BLImageCodec - C++ API
//! \{
#ifdef __cplusplus

//! Image encoder [C++ API].
class BLImageEncoder : public BLImageEncoderCore {
public:
  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLImageEncoder() noexcept { blImageEncoderInit(this); }
  BL_INLINE BLImageEncoder(BLImageEncoder&& other) noexcept { blImageEncoderInitMove(this, &other); }
  BL_INLINE BLImageEncoder(const BLImageEncoder& other) noexcept { blImageEncoderInitWeak(this, &other); }
  BL_INLINE ~BLImageEncoder() { blImageEncoderDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return isValid(); }

  BL_INLINE BLImageEncoder& operator=(BLImageEncoder&& other) noexcept { blImageEncoderAssignMove(this, &other); return *this; }
  BL_INLINE BLImageEncoder& operator=(const BLImageEncoder& other) noexcept { blImageEncoderAssignWeak(this, &other); return *this; }

  BL_NODISCARD BL_INLINE bool operator==(const BLImageEncoder& other) const noexcept { return  equals(other); }
  BL_NODISCARD BL_INLINE bool operator!=(const BLImageEncoder& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blImageEncoderReset(this); }
  BL_INLINE void swap(BLImageEncoderCore& other) noexcept { _d.swap(other._d); }

  BL_INLINE BLResult assign(BLImageEncoderCore&& other) noexcept { return blImageEncoderAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLImageEncoderCore& other) noexcept { return blImageEncoderAssignWeak(this, &other); }

  //! Tests whether the image encoder is a built-in null instance.
  BL_NODISCARD
  BL_INLINE bool isValid() const noexcept { return _impl()->lastResult != BL_ERROR_NOT_INITIALIZED; }

  BL_NODISCARD
  BL_INLINE bool equals(const BLImageEncoderCore& other) const noexcept { return _d.impl == other._d.impl; }

  //! \}

  //! \name Accessors
  //! \{

  BL_NODISCARD
  BL_INLINE BLImageCodec& codec() const noexcept { return _impl()->codec; }

  //! Returns the last encoding result.
  BL_NODISCARD
  BL_INLINE BLResult lastResult() const noexcept { return _impl()->lastResult; }

  //! Returns the current frame index (yet to be written).
  BL_NODISCARD
  BL_INLINE uint64_t frameIndex() const noexcept { return _impl()->frameIndex; }

  //! Returns the position in destination buffer.
  BL_NODISCARD
  BL_INLINE size_t bufferIndex() const noexcept { return _impl()->bufferIndex; }

  //! \}

  //! \name Properties
  //! \{

  BL_DEFINE_OBJECT_PROPERTY_API

  //! \}

  //! \name Encoder Functionality
  //! \{

  BL_INLINE BLResult restart() noexcept { return blImageEncoderRestart(this); }

  //! Encodes the given `image` and writes the encoded data to the destination buffer `dst`.
  BL_INLINE BLResult writeFrame(BLArray<uint8_t>& dst, const BLImageCore& image) noexcept { return blImageEncoderWriteFrame(this, &dst, &image); }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_IMAGECODEC_H_INCLUDED
