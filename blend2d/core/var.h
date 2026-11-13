// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_VAR_H_INCLUDED
#define BLEND2D_CORE_VAR_H_INCLUDED

#include <blend2d/core/object_layer.h>
#include <blend2d/core/rgba.h>

//! \addtogroup bl_c_api
//! \{

//! \name BLVar - C API
//! \{

#ifdef __cplusplus
using BLVarImpl = BLObjectImpl;
#endif

//! Variant [C API].
struct BLVarCore BL_INHERITS_FROM(BLObjectCore) {
  BL_DEFINE_OBJECT(BLVar, bl::ObjectLayer::ObjectInfo<
    BL_OBJECT_TYPE_UNKNOWN,
    bl::ObjectLayer::ObjectMode::kAny,
    bl::ObjectLayer::ObjectLayout::kUnknown>
  )
};

BL_EXTERN_C_BEGIN

BL_API BLResult BL_CDECL bl_var_init_type(BLObjectCore* self, BLObjectType type) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_init_null(BLObjectCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_init_bool(BLObjectCore* self, bool value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_init_int32(BLObjectCore* self, int32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_init_int64(BLObjectCore* self, int64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_init_uint32(BLObjectCore* self, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_init_uint64(BLObjectCore* self, uint64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_init_double(BLObjectCore* self, double value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_init_rgba(BLObjectCore* self, const BLRgba* rgba) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_init_rgba32(BLObjectCore* self, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_init_rgba64(BLObjectCore* self, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_init_move(BLObjectCore* self, BLObjectCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_init_weak(BLObjectCore* self, const BLObjectCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_destroy(BLObjectCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_reset(BLObjectCore* self) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL bl_var_assign_null(BLObjectCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_assign_bool(BLObjectCore* self, bool value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_assign_int32(BLObjectCore* self, int32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_assign_int64(BLObjectCore* self, int64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_assign_uint32(BLObjectCore* self, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_assign_uint64(BLObjectCore* self, uint64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_assign_double(BLObjectCore* self, double value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_assign_rgba(BLObjectCore* self, const BLRgba* rgba) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_assign_rgba32(BLObjectCore* self, uint32_t rgba32) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_assign_rgba64(BLObjectCore* self, uint64_t rgba64) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_assign_move(BLObjectCore* self, BLObjectCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_assign_weak(BLObjectCore* self, const BLObjectCore* other) BL_NOEXCEPT_C;

BL_API BLObjectType BL_CDECL bl_var_get_type(const BLObjectCore* self) BL_NOEXCEPT_C BL_PURE;

BL_API BLResult BL_CDECL bl_var_to_bool(const BLObjectCore* self, bool* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_to_int32(const BLObjectCore* self, int32_t* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_to_int64(const BLObjectCore* self, int64_t* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_to_uint32(const BLObjectCore* self, uint32_t* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_to_uint64(const BLObjectCore* self, uint64_t* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_to_double(const BLObjectCore* self, double* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_to_rgba(const BLObjectCore* self, BLRgba* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_to_rgba32(const BLObjectCore* self, uint32_t* out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_var_to_rgba64(const BLObjectCore* self, uint64_t* out) BL_NOEXCEPT_C;

BL_API bool BL_CDECL bl_var_equals(const BLObjectCore* a, const BLObjectCore* b) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL bl_var_equals_null(const BLObjectCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL bl_var_equals_bool(const BLObjectCore* self, bool value) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL bl_var_equals_int64(const BLObjectCore* self, int64_t value) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL bl_var_equals_uint64(const BLObjectCore* self, uint64_t value) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL bl_var_equals_double(const BLObjectCore* self, double value) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL bl_var_equals_rgba(const BLObjectCore* self, const BLRgba* rgba) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL bl_var_equals_rgba32(const BLObjectCore* self, uint32_t rgba32) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL bl_var_equals_rgba64(const BLObjectCore* self, uint64_t rgba64) BL_NOEXCEPT_C BL_PURE;

BL_API bool BL_CDECL bl_var_strict_equals(const BLObjectCore* a, const BLObjectCore* b) BL_NOEXCEPT_C BL_PURE;

BL_EXTERN_C_END

//! \}
//! \}

//! \addtogroup bl_globals
//! \{

//! \name BLVar - C++ API
//! \{

#ifdef __cplusplus

//! \cond INTERNAL
namespace bl::Internal {
namespace {

//! Helper class that provides static functions that are called by `BLVar` member functions. There are
//! several specializations for primitive types like int/float and also for special types like `BLRgba`.
template<typename T, uint32_t Category>
struct VarOps;

template<typename T>
struct VarOps<T, kTypeCategoryBool> {
  static BL_INLINE_NODEBUG void init(BLVarCore* self, const T& value) noexcept {
    bl::ObjectLayer::init_bool(self, value);
  }

  static BL_INLINE_NODEBUG BLResult assign(BLVarCore* self, const T& value) noexcept {
    return bl_var_assign_bool(self, value);
  }

  static BL_INLINE_NODEBUG bool equals(const BLVarCore* self, const T& value) noexcept {
    return bl_var_equals_bool(self, value);
  }
};

template<typename T>
struct VarOps<T, kTypeCategoryInt> {
  static BL_INLINE void init(BLVarCore* self, const T& value) noexcept {
    if constexpr (std::is_signed_v<T>)
      bl::ObjectLayer::init_int64(self, int64_t(value));
    else
      bl::ObjectLayer::init_uint64(self, uint64_t(value));
  }

  static BL_INLINE BLResult assign(BLVarCore* self, const T& value) noexcept {
    if constexpr (sizeof(T) <= 4 && std::is_signed_v<T>)
      return bl_var_assign_int32(self, int32_t(value));
    else if constexpr (sizeof(T) <= 4)
      return bl_var_assign_uint32(self, uint32_t(value));
    else if constexpr (std::is_signed_v<T>)
      return bl_var_assign_int64(self, int64_t(value));
    else
      return bl_var_assign_uint64(self, uint64_t(value));
  }

  static BL_INLINE bool equals(const BLVarCore* self, const T& value) noexcept {
    if constexpr (std::is_signed_v<T>)
      return bl_var_equals_int64(self, int64_t(value));
    else
      return bl_var_equals_uint64(self, uint64_t(value));
  }
};

template<typename T>
struct VarOps<T, kTypeCategoryFloat> {
  static BL_INLINE_NODEBUG void init(BLVarCore* self, const T& value) noexcept {
    bl::ObjectLayer::init_double(self, double(value));
  }

  static BL_INLINE_NODEBUG BLResult assign(BLVarCore* self, const T& value) noexcept {
    return bl_var_assign_double(self, double(value));
  }

  static BL_INLINE_NODEBUG bool equals(const BLVarCore* self, const T& value) noexcept {
    return bl_var_equals_double(self, double(value));
  }
};

template<typename T>
struct VarOps<T, kTypeCategoryObject> {
  static BL_INLINE_NODEBUG void init(BLVarCore* self, T&& value) noexcept { bl_var_init_move(self, &value); }
  static BL_INLINE_NODEBUG void init(BLVarCore* self, const T& value) noexcept { bl_var_init_weak(self, &value); }

  static BL_INLINE_NODEBUG BLResult assign(BLVarCore* self, T&& value) noexcept { return bl_var_assign_move(self, &value); }
  static BL_INLINE_NODEBUG BLResult assign(BLVarCore* self, const T& value) noexcept { return bl_var_assign_weak(self, &value); }

  static BL_INLINE_NODEBUG bool equals(const BLVarCore* self, const T& value) noexcept { return bl_var_equals(self, &value); }
};

template<>
struct VarOps<BLRgba, kTypeCategoryStruct> {
  static BL_INLINE void init(BLVarCore* self, const BLRgba& value) noexcept { bl::ObjectLayer::init_sso_with_value<BLRgba>(self, BL_OBJECT_TYPE_RGBA, value); }
  static BL_INLINE_NODEBUG BLResult assign(BLVarCore* self, const BLRgba& value) noexcept { return bl_var_assign_rgba(self, &value); }
  static BL_INLINE_NODEBUG bool equals(const BLVarCore* self, const BLRgba& value) noexcept { return bl_var_equals_rgba(self, &value); }
};

template<>
struct VarOps<BLRgba32, kTypeCategoryStruct> {
  static BL_INLINE void init(BLVarCore* self, const BLRgba32& rgba32) noexcept { bl::ObjectLayer::init_sso_with_value<uint32_t>(self, BL_OBJECT_TYPE_RGBA32, rgba32.value); }
  static BL_INLINE_NODEBUG BLResult assign(BLVarCore* self, const BLRgba32 rgba32) noexcept { return bl_var_assign_rgba32(self, rgba32.value); }
  static BL_INLINE_NODEBUG bool equals(const BLVarCore* self, const BLRgba32& rgba32) noexcept { return bl_var_equals_rgba32(self, rgba32.value); }
};

template<>
struct VarOps<BLRgba64, kTypeCategoryStruct> {
  static BL_INLINE void init(BLVarCore* self, const BLRgba64& rgba64) noexcept { bl::ObjectLayer::init_sso_with_value<uint64_t>(self, BL_OBJECT_TYPE_RGBA64, rgba64.value); }
  static BL_INLINE_NODEBUG BLResult assign(BLVarCore* self, const BLRgba64 rgba64) noexcept { return bl_var_assign_rgba64(self, rgba64.value); }
  static BL_INLINE_NODEBUG bool equals(const BLVarCore* self, const BLRgba64& rgba64) noexcept { return bl_var_equals_rgba64(self, rgba64.value); }
};

template<typename T>
struct VarCastImpl {
  static BL_INLINE_NODEBUG T& cast(BLVarCore* self) noexcept { return *static_cast<T*>(static_cast<BLObjectCore*>(self)); }
  static BL_INLINE_NODEBUG const T& cast(const BLVarCore* self) noexcept { return *static_cast<const T*>(static_cast<const BLObjectCore*>(self)); }
};

template<>
struct VarCastImpl<BLRgba> {
  static BL_INLINE_NODEBUG BLRgba& cast(BLVarCore* self) noexcept { return bl::ObjectLayer::sso_value_mut<BLRgba>(self); }
  static BL_INLINE_NODEBUG const BLRgba& cast(const BLVarCore* self) noexcept { return bl::ObjectLayer::sso_value<BLRgba>(self); }
};

template<>
struct VarCastImpl<BLRgba32> {
  static BL_INLINE_NODEBUG BLRgba32& cast(BLVarCore* self) noexcept { return bl::ObjectLayer::sso_value_mut<BLRgba32>(self); }
  static BL_INLINE_NODEBUG const BLRgba32& cast(const BLVarCore* self) noexcept { return bl::ObjectLayer::sso_value<BLRgba32>(self); }
};

template<>
struct VarCastImpl<BLRgba64> {
  static BL_INLINE_NODEBUG BLRgba64& cast(BLVarCore* self) noexcept { return bl::ObjectLayer::sso_value_mut<BLRgba64>(self); }
  static BL_INLINE_NODEBUG const BLRgba64& cast(const BLVarCore* self) noexcept { return bl::ObjectLayer::sso_value<BLRgba64>(self); }
};

} // {anonymous}
} // {bl::Internal}
//! \endcond

//! Variant [C++ API].
class BLVar final : public BLVarCore {
public:
  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLVar() noexcept { bl::ObjectLayer::init_null(this); }

  BL_INLINE_NODEBUG BLVar(BLVar&& other) noexcept {
    _d = other._d;
    bl::ObjectLayer::init_null(&other);
  }

  BL_INLINE_NODEBUG BLVar(const BLVar& other) noexcept { bl_var_init_weak(this, &other); }

  template<typename T>
  BL_INLINE_NODEBUG explicit BLVar(T&& value) noexcept {
    using DecayT = std::decay_t<T>;
    bl::Internal::VarOps<DecayT, bl::Internal::TypeTraits<DecayT>::kCategory>::init(this, bl::Internal::forward<T>(value));
  }

  BL_INLINE_NODEBUG ~BLVar() noexcept {
    if (bl::ObjectLayer::requires_destroy(this)) {
      bl_var_destroy(this);
    }
  }

  //! \}

  //! \name Static Construction
  //! \{

  static BL_INLINE_NODEBUG BLVar null() noexcept { return BLVar(); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  template<typename T>
  BL_INLINE_NODEBUG BLVar& operator=(T&& value) noexcept { assign(bl::Internal::forward<T>(value)); return *this; }

  template<typename T>
  BL_INLINE_NODEBUG bool operator==(const T& other) const noexcept { return equals(other); }

  template<typename T>
  BL_INLINE_NODEBUG bool operator!=(const T& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult reset() noexcept { return bl_var_reset(this); }
  BL_INLINE_NODEBUG void swap(BLVarCore& other) noexcept { _d.swap(other._d); }

  //! \}

  //! \name Type Accessors
  //! \{

  //! Returns the type of the underlying object.
  [[nodiscard]]
  BL_INLINE_NODEBUG BLObjectType type() const noexcept { return _d.type(); }

  //! Tests whether this `BLVar` instance represents a `BLArray<T>` storing any supported type.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_array() const noexcept { return _d.is_array(); }
  //! Tests whether this `BLVar` instance represents `BLBitArray`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_bit_array() const noexcept { return _d.is_bit_array(); }
  //! Tests whether this `BLVar` instance represents a boxed `bool` value.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_bool() const noexcept { return _d.is_bool(); }
  //! Tests whether this `BLVar` instance represents `BLContext`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_context() const noexcept { return _d.is_context(); }
  //! Tests whether this `BLVar` instance represents a boxed `double` value.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_float64() const noexcept { return _d.is_float64(); }
  //! Tests whether this `BLVar` instance represents `BLFont`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_font() const noexcept { return _d.is_font(); }
  //! Tests whether this `BLVar` instance represents `BLFontData`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_font_data() const noexcept { return _d.is_font_data(); }
  //! Tests whether this `BLVar` instance represents `BLFontFace`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_font_face() const noexcept { return _d.is_font_face(); }
  //! Tests whether this `BLVar` instance represents `BLFontManager`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_font_manager() const noexcept { return _d.is_font_manager(); }
  //! Tests whether this `BLVar` instance represents `BLGlyphBuffer`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_glyph_buffer() const noexcept { return _d.is_glyph_buffer(); }
  //! Tests whether this `BLVar` instance represents `BLGradient`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_gradient() const noexcept { return _d.is_gradient(); }
  //! Tests whether this `BLVar` instance represents `BLImage`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_image() const noexcept { return _d.is_image(); }
  //! Tests whether this `BLVar` instance represents `BLImageCodec`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_image_codec() const noexcept { return _d.is_image_codec(); }
  //! Tests whether this `BLVar` instance represents `BLImageDecoder`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_image_decoder() const noexcept { return _d.is_image_decoder(); }
  //! Tests whether this `BLVar` instance represents `BLImageEncoder`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_image_encoder() const noexcept { return _d.is_image_encoder(); }
  //! Tests whether this `BLVar` instance represents a boxed `int64_t` value.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_int64() const noexcept { return _d.is_int64(); }
  //! Tests whether this `BLVar` instance represents a null value.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_null() const noexcept { return _d.is_null(); }
  //! Tests whether this `BLVar` instance represents `BLPath`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_path() const noexcept { return _d.is_path(); }
  //! Tests whether this `BLVar` instance represents `BLPattern.`
  [[nodiscard]] BL_INLINE_NODEBUG bool is_pattern() const noexcept { return _d.is_pattern(); }
  //! Tests whether this `BLVar` instance represents `BLString`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_string() const noexcept { return _d.is_string(); }
  //! Tests whether this `BLVar` instance represents boxed `BLRgba32`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_rgba32() const noexcept { return _d.is_rgba32(); }
  //! Tests whether this `BLVar` instance represents boxed `BLRgba64`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_rgba64() const noexcept { return _d.is_rgba64(); }
  //! Tests whether this `BLVar` instance represents boxed `BLRgba`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_rgba() const noexcept { return _d.is_rgba(); }
  //! Tests whether this `BLVar` instance represents a boxed `uint64_t` value.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_uint64() const noexcept { return _d.is_uint64(); }
  //! Tests whether this `BLVar` instance represents `BLCoverageSet`.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_coverage_set() const noexcept { return _d.is_coverage_set(); }

  //! Tests whether this `BLVar` instance represents a boxed int64_t` or `uint64_t` value.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_integer() const noexcept { return _d.is_type_between(BL_OBJECT_TYPE_INT64, BL_OBJECT_TYPE_UINT64); }

  //! Tests whether this `BLVar` instance is a style that can be used with the rendering context.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_style() const noexcept { return _d.is_style(); }

  //! Converts this value to `bool` and stores the result in `out`.
  BL_INLINE_NODEBUG BLResult to_bool(bool* out) const noexcept { return bl_var_to_bool(this, out); }
  //! Converts this value to `int32_t` and stores the result in `out`.
  BL_INLINE_NODEBUG BLResult to_int32(int32_t* out) const noexcept { return bl_var_to_int32(this, out); }
  //! Converts this value to `int64_t` and stores the result in `out`.
  BL_INLINE_NODEBUG BLResult to_int64(int64_t* out) const noexcept { return bl_var_to_int64(this, out); }
  //! Converts this value to `uint32_t` and stores the result in `out`.
  BL_INLINE_NODEBUG BLResult to_uint32(uint32_t* out) const noexcept { return bl_var_to_uint32(this, out); }
  //! Converts this value to `uint64_t` and stores the result in `out`.
  BL_INLINE_NODEBUG BLResult to_uint64(uint64_t* out) const noexcept { return bl_var_to_uint64(this, out); }
  //! Converts this value to `double` precision floating point and stores the result in `out`.
  BL_INLINE_NODEBUG BLResult to_double(double* out) const noexcept { return bl_var_to_double(this, out); }
  //! Converts this value to `BLRgba` and stores the result in `out`.
  BL_INLINE_NODEBUG BLResult to_rgba(BLRgba* out) const noexcept { return bl_var_to_rgba(this, out); }
  //! Converts this value to `BLRgba32` and stores the result in `out`.
  BL_INLINE_NODEBUG BLResult to_rgba32(BLRgba32* out) const noexcept { return bl_var_to_rgba32(this, &out->value); }
  //! Converts this value to `BLRgba64` and stores the result in `out`.
  BL_INLINE_NODEBUG BLResult to_rgba64(BLRgba64* out) const noexcept { return bl_var_to_rgba64(this, &out->value); }

  //! \}

  //! \name Properties
  //! \{

  BL_DEFINE_OBJECT_PROPERTY_API

  //! \}

  //! \name Casts
  //! \{

  //! Casts this \ref BLVar instance to `T&`.
  template<typename T>
  BL_INLINE_NODEBUG T& as() noexcept { return bl::Internal::VarCastImpl<T>::cast(this); }

  //! Casts this \ref BLVar instance to `const T&`.
  template<typename T>
  BL_INLINE_NODEBUG const T& as() const noexcept { return bl::Internal::VarCastImpl<T>::cast(this); }

  //! \}

  //! \name Assignment
  //! \{

  template<typename T>
  BL_INLINE_NODEBUG BLResult assign(T&& value) noexcept {
    using DecayT = std::decay_t<T>;
    return bl::Internal::VarOps<DecayT, bl::Internal::TypeTraits<DecayT>::kCategory>::assign(this, bl::Internal::forward<T>(value));
  }

  //! \}

  //! \name Equality & Comparison
  //! \{

  template<typename T>
  [[nodiscard]]
  BL_INLINE_NODEBUG bool equals(const T& value) const noexcept {
    using DecayT = std::decay_t<T>;
    return bl::Internal::VarOps<DecayT, bl::Internal::TypeTraits<DecayT>::kCategory>::equals(this, value);
  }

  [[nodiscard]]
  BL_INLINE_NODEBUG bool strict_equals(const BLVarCore& other) const noexcept { return bl_var_strict_equals(this, &other); }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_CORE_VAR_H_INCLUDED
