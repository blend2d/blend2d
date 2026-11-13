// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_IMAGE_DECODER_H_INCLUDED
#define BLEND2D_CORE_IMAGE_DECODER_H_INCLUDED

#include <blend2d/core/api.h>
#if !defined(BL_BUILD_NO_CODECS)

#include <blend2d/core/image_codec.h>
#include <blend2d/core/object_layer.h>
#include <blend2d/core/span.h>
#include <blend2d/core/string.h>

//! \addtogroup bl_c_api
//! \{

//! \name BLImageDecoder - C API
//! \{

//! \cond INTERNAL
//! Image decoder [C API Virtual Function Table].
struct BLImageDecoderVirt BL_INHERITS_FROM(BLObjectVirt) {
  BL_DEFINE_VIRT_BASE

  BLResult (BL_CDECL* restart)(BLImageDecoderCore* self) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* read_info)(BLImageDecoderCore* self, BLImageInfo* info_out, const uint8_t* data, size_t size) BL_NOEXCEPT_C;
  BLResult (BL_CDECL* read_frame)(BLImageDecoderCore* self, BLImageCore* image_out, const uint8_t* data, size_t size) BL_NOEXCEPT_C;
};

//! Image decoder [C API Impl].
struct BLImageDecoderImpl {
  //! \name Members
  //! \{

  //! Image codec that created this decoder.
  BLImageCodecCore codec;

  //! Last faulty result (if failed).
  BLResult last_result;

  //! Handle in case that this decoder wraps a third-party library.
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
    bl_call_ctor(codec.dcast(), codec_->dcast());
    last_result = BL_SUCCESS;
    handle = nullptr;
    buffer_index = 0;
    frame_index = 0;
  }

  //! Explicit destructor that destructs this Impl.
  BL_INLINE void dtor() noexcept {
    bl_call_dtor(codec.dcast());
  }

  //! \}
#endif
};
//! \endcond

//! Image decoder [C API]
struct BLImageDecoderCore BL_INHERITS_FROM(BLObjectCore) {
  BL_DEFINE_OBJECT_VIRT(BLImageDecoder, bl::ObjectLayer::ObjectInfo<
    BL_OBJECT_TYPE_IMAGE_DECODER,
    bl::ObjectLayer::ObjectMode::kDyn,
    bl::ObjectLayer::ObjectLayout::kUnknown>
  )
};

BL_EXTERN_C_BEGIN

BL_API BLResult BL_CDECL bl_image_decoder_init(BLImageDecoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_decoder_init_move(BLImageDecoderCore* self, BLImageDecoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_decoder_init_weak(BLImageDecoderCore* self, const BLImageDecoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_decoder_destroy(BLImageDecoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_decoder_reset(BLImageDecoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_decoder_assign_move(BLImageDecoderCore* self, BLImageDecoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_decoder_assign_weak(BLImageDecoderCore* self, const BLImageDecoderCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_decoder_restart(BLImageDecoderCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_decoder_read_info(BLImageDecoderCore* self, BLImageInfo* info_out, const uint8_t* data, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_image_decoder_read_frame(BLImageDecoderCore* self, BLImageCore* image_out, const uint8_t* data, size_t size) BL_NOEXCEPT_C;

BL_EXTERN_C_END

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

  static constexpr uint8_t kDefaultMetaFlags = BLObjectDetail::kMetaFlagD;

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLImageDecoder() noexcept {
    bl_image_decoder_init(this);

    // Assume a default constructed BLImageDecoder.
    BL_ASSUME(_d._sso_data[BLObjectDetail::kMetaByteIndex] == kDefaultMetaFlags);
  }

  BL_INLINE_NODEBUG BLImageDecoder(BLImageDecoder&& other) noexcept {
    bl_image_decoder_init_move(this, &other);

    // Assume a default initialized `other`.
    BL_ASSUME(other._d._sso_data[BLObjectDetail::kMetaByteIndex] == kDefaultMetaFlags);
  }

  BL_INLINE_NODEBUG BLImageDecoder(const BLImageDecoder& other) noexcept {
    bl_image_decoder_init_weak(this, &other);
  }

  BL_INLINE_NODEBUG ~BLImageDecoder() {
    if (bl::ObjectLayer::requires_destroy(this)) {
      bl_image_decoder_destroy(this);
    }
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return is_valid(); }

  BL_INLINE_NODEBUG BLImageDecoder& operator=(BLImageDecoder&& other) noexcept { bl_image_decoder_assign_move(this, &other); return *this; }
  BL_INLINE_NODEBUG BLImageDecoder& operator=(const BLImageDecoder& other) noexcept { bl_image_decoder_assign_weak(this, &other); return *this; }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool operator==(const BLImageDecoder& other) const noexcept { return  equals(other); }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool operator!=(const BLImageDecoder& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult reset() noexcept {
    BLResult result = bl_image_decoder_reset(this);

    // Reset operation always succeeds.
    BL_ASSUME(result == BL_SUCCESS);
    // Assume a default constructed BLImageDecoder after reset.
    BL_ASSUME(_d._sso_data[BLObjectDetail::kMetaByteIndex] == kDefaultMetaFlags);

    return result;
  }

  BL_INLINE_NODEBUG void swap(BLImageDecoderCore& other) noexcept { _d.swap(other._d); }

  BL_INLINE_NODEBUG BLResult assign(BLImageDecoderCore&& other) noexcept { return bl_image_decoder_assign_move(this, &other); }
  BL_INLINE_NODEBUG BLResult assign(const BLImageDecoderCore& other) noexcept { return bl_image_decoder_assign_weak(this, &other); }

  //! Tests whether the image decoder is a built-in null instance.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_valid() const noexcept { return _impl()->last_result != BL_ERROR_NOT_INITIALIZED; }

  [[nodiscard]] BL_INLINE_NODEBUG bool equals(const BLImageDecoderCore& other) const noexcept { return _d.dyn.impl.ptr == other._d.dyn.impl.ptr; }

  //! \}

  //! \name Accessors
  //! \{

  [[nodiscard]] BL_INLINE_NODEBUG BLImageCodec& codec() const noexcept { return _impl()->codec.dcast(); }

  //! Returns the last decoding result.
  [[nodiscard]] BL_INLINE_NODEBUG BLResult last_result() const noexcept { return _impl()->last_result; }

  //! Returns the current frame index (to be decoded).
  [[nodiscard]] BL_INLINE_NODEBUG uint64_t frame_index() const noexcept { return _impl()->frame_index; }

  //! Returns the position in source buffer.
  [[nodiscard]] BL_INLINE_NODEBUG size_t buffer_index() const noexcept { return _impl()->buffer_index; }

  //! \}

  //! \name Properties
  //! \{

  BL_DEFINE_OBJECT_PROPERTY_API

  //! \}

  //! \name Decoder Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult restart() noexcept { return bl_image_decoder_restart(this); }

  BL_INLINE_NODEBUG BLResult read_info(BLImageInfo& dst, const void* data, size_t size) noexcept {
    return bl_image_decoder_read_info(this, &dst, static_cast<const uint8_t*>(data), size);
  }

  BL_INLINE_NODEBUG BLResult read_info(BLImageInfo& dst, BLSpanConvertible<const uint8_t> auto&& data) noexcept {
    return bl_image_decoder_read_info(this, &dst, data.data(), data.size());
  }

  BL_INLINE_NODEBUG BLResult read_frame(BLImageCore& dst, const void* data, size_t size) noexcept {
    return bl_image_decoder_read_frame(this, &dst, static_cast<const uint8_t*>(data), size);
  }

  BL_INLINE_NODEBUG BLResult read_frame(BLImageCore& dst, BLSpanConvertible<const uint8_t> auto&&  data) noexcept {
    return bl_image_decoder_read_frame(this, &dst, data.data(), data.size());
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // !BL_BUILD_NO_CODECS
#endif // BLEND2D_CORE_IMAGE_DECODER_H_INCLUDED
