// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_IMAGE_ENCODER_H_INCLUDED
#define BLEND2D_CORE_IMAGE_ENCODER_H_INCLUDED

#include <blend2d/core/api.h>
#if !defined(BL_BUILD_NO_CODECS)

#include <blend2d/core/image_codec.h>
#include <blend2d/core/object_layer.h>
#include <blend2d/core/string.h>

//! \addtogroup bl_c_api
//! \{

//! \name BLImageEncoder - C API
//! \{

//! \cond INTERNAL
//! Image encoder [Virtual Function Table].
struct BLImageEncoderVirt BL_INHERITS_FROM(BLObjectVirt) {
  BL_DEFINE_VIRT_BASE

  BLResult (BL_CDECL* restart)(BLImageEncoderCore* self) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* write_frame)(BLImageEncoderCore* self, BLArrayCore* dst, const BLImageCore* image) BL_NOEXCEPT_C;
};

//! Image encoder [Impl].
struct BLImageEncoderImpl BL_INHERITS_FROM(BLObjectImpl) {
  //! \name Members
  //! \{

  //! Image codec that created this encoder.
  BLImageCodecCore codec;

  //! Last faulty result (if failed).
  BLResult last_result;

  //! Handle in case that this encoder wraps a third-party library.
  void* handle;
  //! Current frame index.
  uint64_t frame_index;
  //! Position in source buffer.
  size_t buffer_index;

  //! \}

#ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  //! Explicit constructor that constructs this Impl.
  BL_INLINE void ctor(const BLImageCodecCore* codec_) noexcept {
    bl_image_codec_init_weak(&codec, codec_);
    last_result = BL_SUCCESS;
    handle = nullptr;
    buffer_index = 0;
    frame_index = 0;
  }

  //! Explicit destructor that destructs this Impl.
  BL_INLINE void dtor() noexcept {
    bl_image_codec_destroy(&codec);
  }

  //! \}
#endif
};
//! \endcond

//! Image encoder [C API].
struct BLImageEncoderCore BL_INHERITS_FROM(BLObjectCore) {
  BL_DEFINE_OBJECT_VIRT(BLImageEncoder, bl::ObjectLayer::ObjectInfo<
    BL_OBJECT_TYPE_IMAGE_ENCODER,
    bl::ObjectLayer::ObjectMode::kDyn,
    bl::ObjectLayer::ObjectLayout::kUnknown>
  )
};

BL_EXTERN_C_BEGIN

BL_API BLResult BL_CDECL bl_image_encoder_init(BLImageEncoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_encoder_init_move(BLImageEncoderCore* self, BLImageEncoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_encoder_init_weak(BLImageEncoderCore* self, const BLImageEncoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_encoder_destroy(BLImageEncoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_encoder_reset(BLImageEncoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_encoder_assign_move(BLImageEncoderCore* self, BLImageEncoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_encoder_assign_weak(BLImageEncoderCore* self, const BLImageEncoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_encoder_restart(BLImageEncoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_encoder_write_frame(BLImageEncoderCore* self, BLArrayCore* dst, const BLImageCore* image) BL_NOEXCEPT_C;

BL_EXTERN_C_END

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

  static constexpr uint8_t kDefaultMetaFlags = BLObjectDetail::kMetaFlagD;

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLImageEncoder() noexcept {
    bl_image_encoder_init(this);

    // Assume a default constructed BLImageEncoder.
    BL_ASSUME(_d._sso_data[BLObjectDetail::kMetaByteIndex] == kDefaultMetaFlags);
  }

  BL_INLINE_NODEBUG BLImageEncoder(BLImageEncoder&& other) noexcept {
    bl_image_encoder_init_move(this, &other);

    // Assume a default initialized `other`.
    BL_ASSUME(other._d._sso_data[BLObjectDetail::kMetaByteIndex] == kDefaultMetaFlags);
  }

  BL_INLINE_NODEBUG BLImageEncoder(const BLImageEncoder& other) noexcept {
    bl_image_encoder_init_weak(this, &other);
  }

  BL_INLINE_NODEBUG ~BLImageEncoder() {
    if (bl::ObjectLayer::requires_destroy(this)) {
      bl_image_encoder_destroy(this);
    }
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return is_valid(); }

  BL_INLINE_NODEBUG BLImageEncoder& operator=(BLImageEncoder&& other) noexcept { bl_image_encoder_assign_move(this, &other); return *this; }
  BL_INLINE_NODEBUG BLImageEncoder& operator=(const BLImageEncoder& other) noexcept { bl_image_encoder_assign_weak(this, &other); return *this; }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool operator==(const BLImageEncoder& other) const noexcept { return  equals(other); }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool operator!=(const BLImageEncoder& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult reset() noexcept {
    BLResult result = bl_image_encoder_reset(this);

    // Reset operation always succeeds.
    BL_ASSUME(result == BL_SUCCESS);
    // Assume a default constructed BLImageEncoder after reset.
    BL_ASSUME(_d._sso_data[BLObjectDetail::kMetaByteIndex] == kDefaultMetaFlags);

    return result;
  }

  BL_INLINE_NODEBUG void swap(BLImageEncoderCore& other) noexcept { _d.swap(other._d); }

  BL_INLINE_NODEBUG BLResult assign(BLImageEncoderCore&& other) noexcept { return bl_image_encoder_assign_move(this, &other); }
  BL_INLINE_NODEBUG BLResult assign(const BLImageEncoderCore& other) noexcept { return bl_image_encoder_assign_weak(this, &other); }

  //! Tests whether the image encoder is a built-in null instance.
  [[nodiscard]]
  BL_INLINE_NODEBUG bool is_valid() const noexcept { return _impl()->last_result != BL_ERROR_NOT_INITIALIZED; }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool equals(const BLImageEncoderCore& other) const noexcept { return _d.dyn.impl.ptr == other._d.dyn.impl.ptr; }

  //! \}

  //! \name Accessors
  //! \{

  [[nodiscard]]
  BL_INLINE_NODEBUG BLImageCodec& codec() const noexcept { return _impl()->codec.dcast(); }

  //! Returns the last encoding result.
  [[nodiscard]]
  BL_INLINE_NODEBUG BLResult last_result() const noexcept { return _impl()->last_result; }

  //! Returns the current frame index (yet to be written).
  [[nodiscard]]
  BL_INLINE_NODEBUG uint64_t frame_index() const noexcept { return _impl()->frame_index; }

  //! Returns the position in destination buffer.
  [[nodiscard]]
  BL_INLINE_NODEBUG size_t buffer_index() const noexcept { return _impl()->buffer_index; }

  //! \}

  //! \name Properties
  //! \{

  BL_DEFINE_OBJECT_PROPERTY_API

  //! \}

  //! \name Encoder Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult restart() noexcept {
    return bl_image_encoder_restart(this);
  }

  //! Encodes the given `image` and writes the encoded data to the destination buffer `dst`.
  BL_INLINE_NODEBUG BLResult write_frame(BLArray<uint8_t>& dst, const BLImageCore& image) noexcept {
    return bl_image_encoder_write_frame(this, &dst, &image);
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // !BL_BUILD_NO_CODECS
#endif // BLEND2D_CORE_IMAGE_ENCODER_H_INCLUDED
