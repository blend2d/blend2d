// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_IMAGEENCODER_H_INCLUDED
#define BLEND2D_IMAGEENCODER_H_INCLUDED

#include "imagecodec.h"
#include "object.h"
#include "string.h"

//! \addtogroup bl_c_api
//! \{

//! \name BLImageEncoder - C API
//! \{

//! Image encoder [C API].
struct BLImageEncoderCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
  BL_DEFINE_OBJECT_DCAST(BLImageEncoder)

#ifdef __cplusplus
  //! \name Impl Utilities
  //! \{

  //! Returns Impl of the image encoder (only provided for use cases that implement BLImageEncoder).
  template<typename T = BLImageEncoderImpl>
  [[nodiscard]]
  BL_INLINE_NODEBUG T* _impl() const noexcept { return static_cast<T*>(_d.impl); }

  //! \}
#endif
};

//! \cond INTERNAL
//! Image encoder [Virtual Function Table].
struct BLImageEncoderVirt BL_CLASS_INHERITS(BLObjectVirt) {
  BL_DEFINE_VIRT_BASE

  BLResult (BL_CDECL* restart)(BLImageEncoderImpl* impl) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* writeFrame)(BLImageEncoderImpl* impl, BLArrayCore* dst, const BLImageCore* image) BL_NOEXCEPT_C;
};

//! Image encoder [Impl].
struct BLImageEncoderImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! \name Members
  //! \{

  //! Virtual function table.
  const BLImageEncoderVirt* virt;

  //! Image codec that created this encoder.
  BLImageCodecCore codec;

  //! Last faulty result (if failed).
  BLResult lastResult;

  //! Handle in case that this encoder wraps a third-party library.
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
    blImageCodecInitWeak(&codec, codec_);
    lastResult = BL_SUCCESS;
    handle = nullptr;
    bufferIndex = 0;
    frameIndex = 0;
  }

  //! Explicit destructor that destructs this Impl.
  BL_INLINE void dtor() noexcept {
    blImageCodecDestroy(&codec);
  }

  //! \}
#endif
};
//! \endcond

BL_BEGIN_C_DECLS

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
//! \}

//! \addtogroup bl_imaging
//! \{

//! \name BLImageCodec - C++ API
//! \{
#ifdef __cplusplus

//! Image encoder [C++ API].
class BLImageEncoder final : public BLImageEncoderCore {
public:
  //! \cond INTERNAL
  //! \name Internals
  //! \{

  //! Object info values of a default constructed BLImageEncoder.
  static inline constexpr uint32_t kDefaultSignature =
    BLObjectInfo::packTypeWithMarker(BL_OBJECT_TYPE_IMAGE_ENCODER) | BL_OBJECT_INFO_D_FLAG;

  //! Returns Impl of the image codec (only provided for use cases that implement BLImageCodec).
  template<typename T = BLImageEncoderImpl>
  [[nodiscard]]
  BL_INLINE_NODEBUG T* _impl() const noexcept { return static_cast<T*>(_d.impl); }

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLImageEncoder() noexcept {
    blImageEncoderInit(this);

    // Assume a default constructed BLImageEncoder.
    BL_ASSUME(_d.info.bits == kDefaultSignature);
  }

  BL_INLINE_NODEBUG BLImageEncoder(BLImageEncoder&& other) noexcept {
    blImageEncoderInitMove(this, &other);

    // Assume a default initialized `other`.
    BL_ASSUME(other._d.info.bits == kDefaultSignature);
  }

  BL_INLINE_NODEBUG BLImageEncoder(const BLImageEncoder& other) noexcept {
    blImageEncoderInitWeak(this, &other);
  }

  BL_INLINE_NODEBUG ~BLImageEncoder() {
    if (BLInternal::objectNeedsCleanup(_d.info.bits)) {
      blImageEncoderDestroy(this);
    }
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return isValid(); }

  BL_INLINE_NODEBUG BLImageEncoder& operator=(BLImageEncoder&& other) noexcept { blImageEncoderAssignMove(this, &other); return *this; }
  BL_INLINE_NODEBUG BLImageEncoder& operator=(const BLImageEncoder& other) noexcept { blImageEncoderAssignWeak(this, &other); return *this; }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool operator==(const BLImageEncoder& other) const noexcept { return  equals(other); }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool operator!=(const BLImageEncoder& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult reset() noexcept {
    BLResult result = blImageEncoderReset(this);

    // Reset operation always succeeds.
    BL_ASSUME(result == BL_SUCCESS);
    // Assume a default constructed BLImageEncoder after reset.
    BL_ASSUME(_d.info.bits == kDefaultSignature);

    return result;
  }

  BL_INLINE_NODEBUG void swap(BLImageEncoderCore& other) noexcept { _d.swap(other._d); }

  BL_INLINE_NODEBUG BLResult assign(BLImageEncoderCore&& other) noexcept { return blImageEncoderAssignMove(this, &other); }
  BL_INLINE_NODEBUG BLResult assign(const BLImageEncoderCore& other) noexcept { return blImageEncoderAssignWeak(this, &other); }

  //! Tests whether the image encoder is a built-in null instance.
  [[nodiscard]]
  BL_INLINE_NODEBUG bool isValid() const noexcept { return _impl()->lastResult != BL_ERROR_NOT_INITIALIZED; }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool equals(const BLImageEncoderCore& other) const noexcept { return _d.impl == other._d.impl; }

  //! \}

  //! \name Accessors
  //! \{

  [[nodiscard]]
  BL_INLINE_NODEBUG BLImageCodec& codec() const noexcept { return _impl()->codec.dcast(); }

  //! Returns the last encoding result.
  [[nodiscard]]
  BL_INLINE_NODEBUG BLResult lastResult() const noexcept { return _impl()->lastResult; }

  //! Returns the current frame index (yet to be written).
  [[nodiscard]]
  BL_INLINE_NODEBUG uint64_t frameIndex() const noexcept { return _impl()->frameIndex; }

  //! Returns the position in destination buffer.
  [[nodiscard]]
  BL_INLINE_NODEBUG size_t bufferIndex() const noexcept { return _impl()->bufferIndex; }

  //! \}

  //! \name Properties
  //! \{

  BL_DEFINE_OBJECT_PROPERTY_API

  //! \}

  //! \name Encoder Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult restart() noexcept { return blImageEncoderRestart(this); }

  //! Encodes the given `image` and writes the encoded data to the destination buffer `dst`.
  BL_INLINE_NODEBUG BLResult writeFrame(BLArray<uint8_t>& dst, const BLImageCore& image) noexcept { return blImageEncoderWriteFrame(this, &dst, &image); }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_IMAGEENCODER_H_INCLUDED
