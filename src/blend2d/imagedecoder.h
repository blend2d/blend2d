// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_IMAGEDECODER_H_INCLUDED
#define BLEND2D_IMAGEDECODER_H_INCLUDED

#include "imagecodec.h"
#include "object.h"
#include "string.h"

//! \addtogroup bl_c_api
//! \{

//! \name BLImageDecoder - C API
//! \{

//! Image decoder [C API]
struct BLImageDecoderCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
  BL_DEFINE_OBJECT_DCAST(BLImageDecoder)

#ifdef __cplusplus
  //! \name Impl Utilities
  //! \{

  //! Returns Impl of the image decoder (only provided for use cases that implement BLImageDecoder).
  template<typename T = BLImageDecoderImpl>
  [[nodiscard]]
  BL_INLINE_NODEBUG T* _impl() const noexcept { return static_cast<T*>(_d.impl); }

  //! \}
#endif
};

//! \cond INTERNAL
//! Image decoder [C API Virtual Function Table].
struct BLImageDecoderVirt BL_CLASS_INHERITS(BLObjectVirt) {
  BL_DEFINE_VIRT_BASE

  BLResult (BL_CDECL* restart)(BLImageDecoderImpl* impl) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* readInfo)(BLImageDecoderImpl* impl, BLImageInfo* infoOut, const uint8_t* data, size_t size) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* readFrame)(BLImageDecoderImpl* impl, BLImageCore* imageOut, const uint8_t* data, size_t size) BL_NOEXCEPT_C;
};

//! Image decoder [C API Impl].
struct BLImageDecoderImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! \name Members
  //! \{

  //! Virtual function table.
  const BLImageDecoderVirt* virt;

  //! Image codec that created this decoder.
  BLImageCodecCore codec;

  //! Last faulty result (if failed).
  BLResult lastResult;

  //! Handle in case that this decoder wraps a third-party library.
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
    blCallCtor(codec.dcast(), codec_->dcast());
    lastResult = BL_SUCCESS;
    handle = nullptr;
    bufferIndex = 0;
    frameIndex = 0;
  }

  //! Explicit destructor that destructs this Impl.
  BL_INLINE void dtor() noexcept {
    blCallDtor(codec.dcast());
  }

  //! \}
#endif
};
//! \endcond

BL_BEGIN_C_DECLS

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

BL_END_C_DECLS

//! \}
//! \}

//! \addtogroup bl_imaging
//! \{

//! \name BLImageDecoder - C++ API
//! \{
#ifdef __cplusplus

//! Image decoder [C++ API].
class BLImageDecoder final : public BLImageDecoderCore {
public:
  //! \cond INTERNAL
  //! \name Internals
  //! \{

  //! Object info values of a default constructed BLImageDecoder.
  static inline constexpr uint32_t kDefaultSignature =
    BLObjectInfo::packTypeWithMarker(BL_OBJECT_TYPE_IMAGE_DECODER) | BL_OBJECT_INFO_D_FLAG;

  //! Returns Impl of the image codec (only provided for use cases that implement BLImageCodec).
  template<typename T = BLImageDecoderImpl>
  [[nodiscard]]
  BL_INLINE_NODEBUG T* _impl() const noexcept { return static_cast<T*>(_d.impl); }

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLImageDecoder() noexcept {
    blImageDecoderInit(this);

    // Assume a default constructed BLImageDecoder.
    BL_ASSUME(_d.info.bits == kDefaultSignature);
  }

  BL_INLINE_NODEBUG BLImageDecoder(BLImageDecoder&& other) noexcept {
    blImageDecoderInitMove(this, &other);

    // Assume a default initialized `other`.
    BL_ASSUME(other._d.info.bits == kDefaultSignature);
  }

  BL_INLINE_NODEBUG BLImageDecoder(const BLImageDecoder& other) noexcept {
    blImageDecoderInitWeak(this, &other);
  }

  BL_INLINE_NODEBUG ~BLImageDecoder() {
    if (BLInternal::objectNeedsCleanup(_d.info.bits)) {
      blImageDecoderDestroy(this);
    }
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return isValid(); }

  BL_INLINE_NODEBUG BLImageDecoder& operator=(BLImageDecoder&& other) noexcept { blImageDecoderAssignMove(this, &other); return *this; }
  BL_INLINE_NODEBUG BLImageDecoder& operator=(const BLImageDecoder& other) noexcept { blImageDecoderAssignWeak(this, &other); return *this; }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool operator==(const BLImageDecoder& other) const noexcept { return  equals(other); }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool operator!=(const BLImageDecoder& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult reset() noexcept {
    BLResult result = blImageDecoderReset(this);

    // Reset operation always succeeds.
    BL_ASSUME(result == BL_SUCCESS);
    // Assume a default constructed BLImageDecoder after reset.
    BL_ASSUME(_d.info.bits == kDefaultSignature);

    return result;
  }

  BL_INLINE_NODEBUG void swap(BLImageDecoderCore& other) noexcept { _d.swap(other._d); }

  BL_INLINE_NODEBUG BLResult assign(BLImageDecoderCore&& other) noexcept { return blImageDecoderAssignMove(this, &other); }
  BL_INLINE_NODEBUG BLResult assign(const BLImageDecoderCore& other) noexcept { return blImageDecoderAssignWeak(this, &other); }

  //! Tests whether the image decoder is a built-in null instance.
  [[nodiscard]]
  BL_INLINE_NODEBUG bool isValid() const noexcept { return _impl()->lastResult != BL_ERROR_NOT_INITIALIZED; }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool equals(const BLImageDecoderCore& other) const noexcept { return _d.impl == other._d.impl; }

  //! \}

  //! \name Accessors
  //! \{

  [[nodiscard]]
  BL_INLINE_NODEBUG BLImageCodec& codec() const noexcept { return _impl()->codec.dcast(); }

  //! Returns the last decoding result.
  [[nodiscard]]
  BL_INLINE_NODEBUG BLResult lastResult() const noexcept { return _impl()->lastResult; }

  //! Returns the current frame index (to be decoded).
  [[nodiscard]]
  BL_INLINE_NODEBUG uint64_t frameIndex() const noexcept { return _impl()->frameIndex; }

  //! Returns the position in source buffer.
  [[nodiscard]]
  BL_INLINE_NODEBUG size_t bufferIndex() const noexcept { return _impl()->bufferIndex; }

  //! \}

  //! \name Properties
  //! \{

  BL_DEFINE_OBJECT_PROPERTY_API

  //! \}

  //! \name Decoder Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult restart() noexcept { return blImageDecoderRestart(this); }

  BL_INLINE_NODEBUG BLResult readInfo(BLImageInfo& dst, const BLArray<uint8_t>& buffer) noexcept { return blImageDecoderReadInfo(this, &dst, buffer.data(), buffer.size()); }
  BL_INLINE_NODEBUG BLResult readInfo(BLImageInfo& dst, const BLArrayView<uint8_t>& view) noexcept { return blImageDecoderReadInfo(this, &dst, view.data, view.size); }
  BL_INLINE_NODEBUG BLResult readInfo(BLImageInfo& dst, const void* data, size_t size) noexcept { return blImageDecoderReadInfo(this, &dst, static_cast<const uint8_t*>(data), size); }

  BL_INLINE_NODEBUG BLResult readFrame(BLImageCore& dst, const BLArray<uint8_t>& buffer) noexcept { return blImageDecoderReadFrame(this, &dst, buffer.data(), buffer.size()); }
  BL_INLINE_NODEBUG BLResult readFrame(BLImageCore& dst, const BLArrayView<uint8_t>& view) noexcept { return blImageDecoderReadFrame(this, &dst, view.data, view.size); }
  BL_INLINE_NODEBUG BLResult readFrame(BLImageCore& dst, const void* data, size_t size) noexcept { return blImageDecoderReadFrame(this, &dst, static_cast<const uint8_t*>(data), size); }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_IMAGEDECODER_H_INCLUDED
