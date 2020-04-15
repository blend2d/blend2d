// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_IMAGECODEC_H
#define BLEND2D_IMAGECODEC_H

#include "./array.h"
#include "./image.h"
#include "./variant.h"

//! \addtogroup blend2d_api_imaging
//! \{

// ============================================================================
// [BLImageCodec - Constants]
// ============================================================================

//! Image codec feature bits.
BL_DEFINE_ENUM(BLImageCodecFeatures) {
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
};

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

  //! Image codec name like "PNG", "JPEG", etc...
  const char* name;
  //! Image codec vendor, built-in codecs use "Blend2D".
  const char* vendor;
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

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLImageCodec() noexcept { this->impl = none().impl; }
  BL_INLINE BLImageCodec(BLImageCodec&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLImageCodec(const BLImageCodec& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLImageCodec(BLImageCodecImpl* impl) noexcept { this->impl = impl; }
  BL_INLINE ~BLImageCodec() { blImageCodecDestroy(this); }

  //! \}

  //! \name Operator Overloads
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return !isNone(); }

  BL_INLINE BLImageCodec& operator=(const BLImageCodec& other) noexcept {
    blImageCodecAssignWeak(this, &other);
    return *this;
  }

  BL_INLINE bool operator==(const BLImageCodec& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLImageCodec& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blImageCodecReset(this); }
  BL_INLINE void swap(BLImageCodec& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult assign(const BLImageCodec& other) noexcept { return blImageCodecAssignWeak(this, &other); }

  //! Tests whether the image codec is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }

  BL_INLINE bool equals(const BLImageCodec& other) const noexcept { return this->impl == other.impl; }

  //! \}

  //! \name Properties
  //! \{

  //! Returns image codec name (i.e, "PNG", "JPEG", etc...).
  BL_INLINE const char* name() const noexcept { return impl->name; }
  //! Returns the image codec vendor (i.e. "Blend2D" for all built-in codecs).
  BL_INLINE const char* vendor() const noexcept { return impl->vendor; }
  //! Returns a mime-type associated with the image codec's format.
  BL_INLINE const char* mimeType() const noexcept { return impl->mimeType; }
  //! Returns a list of file extensions used to store image of this codec, separated by '|' character.
  BL_INLINE const char* extensions() const noexcept { return impl->extensions; }
  //! Returns image codec flags, see `BLImageCodecFeatures`.
  BL_INLINE uint32_t features() const noexcept { return impl->features; }
  //! Tests whether the image codec has a flag `flag`.
  BL_INLINE bool hasFeature(uint32_t feature) const noexcept { return (impl->features & feature) != 0; }

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

  BL_INLINE uint32_t inspectData(const BLArray<uint8_t>& buffer) const noexcept { return inspectData(buffer.view()); }
  BL_INLINE uint32_t inspectData(const BLArrayView<uint8_t>& view) const noexcept { return inspectData(view.data, view.size); }
  BL_INLINE uint32_t inspectData(const void* data, size_t size) const noexcept { return blImageCodecInspectData(this, static_cast<const uint8_t*>(data), size); }

  BL_INLINE BLResult createDecoder(BLImageDecoder* dst) const noexcept { return blImageCodecCreateDecoder(this, reinterpret_cast<BLImageDecoderCore*>(dst)); }
  BL_INLINE BLResult createEncoder(BLImageEncoder* dst) const noexcept { return blImageCodecCreateEncoder(this, reinterpret_cast<BLImageEncoderCore*>(dst)); }

  //! \}

  //! \name Built-In Codecs
  //! \{

  static BL_INLINE BLArray<BLImageCodec> builtInCodecs() noexcept {
    BLArray<BLImageCodec> result(nullptr);
    blImageCodecArrayInitBuiltInCodecs(&result);
    return result;
  }

  static BL_INLINE BLResult addToBuiltIn(const BLImageCodec& codec) noexcept {
    return blImageCodecAddToBuiltIn(&codec);
  }

  static BL_INLINE BLResult removeFromBuiltIn(const BLImageCodec& codec) noexcept {
    return blImageCodecRemoveFromBuiltIn(&codec);
  }

  //! \}

  static BL_INLINE const BLImageCodec& none() noexcept { return reinterpret_cast<const BLImageCodec*>(blNone)[kImplType]; }
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

  //! Image codec that created this decoder.
  BL_TYPED_MEMBER(BLImageCodecCore, BLImageCodec, codec);
  //! Handle in case that this decoder wraps a thirt-party library.
  void* handle;
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

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLImageDecoder() noexcept { this->impl = none().impl; }
  BL_INLINE BLImageDecoder(BLImageDecoder&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLImageDecoder(const BLImageDecoder& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLImageDecoder(BLImageDecoderImpl* impl) noexcept { this->impl = impl; }
  BL_INLINE ~BLImageDecoder() { blImageDecoderDestroy(this); }

  //! \}

  //! \name Operator Overloads
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return !isNone(); }

  BL_INLINE BLImageDecoder& operator=(BLImageDecoder&& other) noexcept { blImageDecoderAssignMove(this, &other); return *this; }
  BL_INLINE BLImageDecoder& operator=(const BLImageDecoder& other) noexcept { blImageDecoderAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLImageDecoder& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLImageDecoder& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blImageDecoderReset(this); }
  BL_INLINE void swap(BLImageDecoder& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult assign(BLImageDecoder&& other) noexcept { return blImageDecoderAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLImageDecoder& other) noexcept { return blImageDecoderAssignWeak(this, &other); }

  //! Tests whether the image decoder is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }

  BL_INLINE bool equals(const BLImageDecoder& other) const noexcept { return this->impl == other.impl; }

  //! \}

  //! \name Properties
  //! \{

  //! Returns the last decoding result.
  BL_INLINE BLResult lastResult() const noexcept { return impl->lastResult; }
  //! Returns the current frame index (to be decoded).
  BL_INLINE uint64_t frameIndex() const noexcept { return impl->frameIndex; }
  //! Returns the position in source buffer.
  BL_INLINE size_t bufferIndex() const noexcept { return impl->bufferIndex; }

  //! \}

  //! \name Decoder Functionality
  //! \{

  BL_INLINE BLResult restart() noexcept { return blImageDecoderRestart(this); }

  BL_INLINE BLResult readInfo(BLImageInfo& dst, const BLArray<uint8_t>& buffer) noexcept { return blImageDecoderReadInfo(this, &dst, buffer.data(), buffer.size()); }
  BL_INLINE BLResult readInfo(BLImageInfo& dst, const BLArrayView<uint8_t>& view) noexcept { return blImageDecoderReadInfo(this, &dst, view.data, view.size); }
  BL_INLINE BLResult readInfo(BLImageInfo& dst, const void* data, size_t size) noexcept { return blImageDecoderReadInfo(this, &dst, static_cast<const uint8_t*>(data), size); }

  BL_INLINE BLResult readFrame(BLImage& dst, const BLArray<uint8_t>& buffer) noexcept { return blImageDecoderReadFrame(this, &dst, buffer.data(), buffer.size()); }
  BL_INLINE BLResult readFrame(BLImage& dst, const BLArrayView<uint8_t>& view) noexcept { return blImageDecoderReadFrame(this, &dst, view.data, view.size); }
  BL_INLINE BLResult readFrame(BLImage& dst, const void* data, size_t size) noexcept { return blImageDecoderReadFrame(this, &dst, static_cast<const uint8_t*>(data), size); }

  //! \}

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

  //! Image codec that created this encoder.
  BL_TYPED_MEMBER(BLImageCodecCore, BLImageCodec, codec);
  //! Handle in case that this encoder wraps a thirt-party library.
  void* handle;
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

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLImageEncoder() noexcept { this->impl = none().impl; }
  BL_INLINE BLImageEncoder(BLImageEncoder&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLImageEncoder(const BLImageEncoder& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLImageEncoder(BLImageEncoderImpl* impl) noexcept { this->impl = impl; }
  BL_INLINE ~BLImageEncoder() { blImageEncoderDestroy(this); }

  //! \}

  //! \name Operator Overloads
  //! \{

  BL_INLINE explicit operator bool() const noexcept { return !isNone(); }

  BL_INLINE BLImageEncoder& operator=(BLImageEncoder&& other) noexcept { blImageEncoderAssignMove(this, &other); return *this; }
  BL_INLINE BLImageEncoder& operator=(const BLImageEncoder& other) noexcept { blImageEncoderAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLImageEncoder& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLImageEncoder& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE BLResult reset() noexcept { return blImageEncoderReset(this); }
  BL_INLINE void swap(BLImageEncoder& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult assign(BLImageEncoder&& other) noexcept { return blImageEncoderAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLImageEncoder& other) noexcept { return blImageEncoderAssignWeak(this, &other); }

  //! Tests whether the image encoder is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }

  BL_INLINE bool equals(const BLImageEncoder& other) const noexcept { return this->impl == other.impl; }

  //! \}

  //! \name Properties
  //! \{

  //! Returns the last decoding result.
  BL_INLINE BLResult lastResult() const noexcept { return impl->lastResult; }
  //! Returns the current frame index (yet to be written).
  BL_INLINE uint64_t frameIndex() const noexcept { return impl->frameIndex; }
  //! Returns the position in destination buffer.
  BL_INLINE size_t bufferIndex() const noexcept { return impl->bufferIndex; }

  //! \}

  //! \name Encoder Functionality
  //! \{

  BL_INLINE BLResult restart() noexcept { return blImageEncoderRestart(this); }

  //! Encodes the given `image` and writes the encoded data to the destination buffer `dst`.
  BL_INLINE BLResult writeFrame(BLArray<uint8_t>& dst, const BLImage& image) noexcept { return blImageEncoderWriteFrame(this, &dst, &image); }

  //! \}

  static BL_INLINE const BLImageEncoder& none() noexcept { return reinterpret_cast<const BLImageEncoder*>(blNone)[kImplType]; }
};
#endif

//! \}

#endif // BLEND2D_IMAGECODEC_H
