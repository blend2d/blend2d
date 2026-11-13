// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_OBJECT_LAYER_H_INCLUDED
#define BLEND2D_CORE_OBJECT_LAYER_H_INCLUDED

#include <blend2d/core/object.h>
#include <blend2d/core/span.h>

//! \addtogroup bl_object
//! \{

#ifdef __cplusplus
namespace bl::ObjectLayer {
namespace {

// Object Layer - SSO Value
// ========================

template<typename T, typename Object>
[[nodiscard]] BL_INLINE_NODEBUG const T& sso_value(const Object* self) noexcept {
  return *reinterpret_cast<const T*>(self->_d._sso_data);
}

template<typename T, typename Object>
[[nodiscard]] BL_INLINE_NODEBUG T& sso_value_mut(Object* self) noexcept {
  return *reinterpret_cast<T*>(self->_d._sso_data);
}

// Object Layer - Container Utils
// ==============================

template<typename T, typename Object>
[[nodiscard]] BL_INLINE_NODEBUG std::add_const_t<T>* sso_data(const Object* self) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown && Object::Info::kSSOAvailable)
{
  return reinterpret_cast<std::add_const_t<T>*>(self->_d._sso_data + Object::Info::kSSODataOffset);
}

template<typename T, typename Object>
[[nodiscard]] BL_INLINE_NODEBUG T* sso_data(Object* self) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown  && Object::Info::kSSOAvailable)
{
  return reinterpret_cast<T*>(self->_d._sso_data + Object::Info::kSSODataOffset);
}

template<typename T, typename Object>
[[nodiscard]] BL_INLINE_NODEBUG std::add_const_t<T>* dyn_data(const Object* self) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown )
{
  if constexpr (Object::Info::kLayout == ObjectLayout::kCompact32) {
    return reinterpret_cast<std::add_const_t<T>*>(self->_d.dyn.impl.ptr);
  }
  else {
    return reinterpret_cast<std::add_const_t<T>*>(self->_d.dyn.data_or_virt.ptr);
  }
}

template<typename T, typename Object>
[[nodiscard]] BL_INLINE_NODEBUG T* dyn_data(Object* self) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown)
{
  if constexpr (Object::Info::kLayout == ObjectLayout::kCompact32) {
    return reinterpret_cast<T*>(self->_d.dyn.impl.ptr);
  }
  else {
    return reinterpret_cast<T*>(self->_d.dyn.data_or_virt.ptr);
  }
}

template<typename T, typename Object>
[[nodiscard]] BL_INLINE_NODEBUG std::add_const_t<T>* get_data(const Object* self) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown)
{
  if constexpr (Object::Info::kSSOAvailable) {
    const void* sso = sso_data<void>(self);
    const void* dyn = dyn_data<void>(self);
    return reinterpret_cast<std::add_const_t<T>*>(self->_d.is_sso() ? sso : dyn);
  }
  else {
    return dyn_data<T>(self);
  }
}

template<typename T, typename Object>
[[nodiscard]] BL_INLINE_NODEBUG T* get_data(Object* self) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown)
{
  if constexpr (Object::Info::kSSOAvailable) {
    void* sso = sso_data<void>(self);
    void* dyn = dyn_data<void>(self);
    return reinterpret_cast<T*>(self->_d.is_sso() ? sso : dyn);
  }
  else {
    return dyn_data<T>(self);
  }
}

template<typename Object>
[[nodiscard]] BL_INLINE_NODEBUG size_t sso_size(const Object* self) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown && Object::Info::kSSOAvailable)
{
  return self->_d._sso_data[Object::Info::kSSOSizeByte];
}

template<typename Object>
[[nodiscard]] BL_INLINE_NODEBUG size_t dyn_size(const Object* self) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown)
{
  if constexpr (Object::Info::kLayout == ObjectLayout::kNative64) {
    return self->_d.dyn.size_u64;
  }
  else if constexpr (Object::Info::kLayout == ObjectLayout::kNative32 ||
                     Object::Info::kLayout == ObjectLayout::kCompact32) {
    return self->_d.dyn.size_u32;
  }
  else if constexpr (Object::Info::kLayout == ObjectLayout::kPath) {
    // Path has 2 size components (vertex data size and control data size) - a dynamic "byte-size" is the sum of both.
    return self->_d.dyn.size_u32 + self->_d.dyn.payload_u32;
  }
}

template<typename Object>
[[nodiscard]] BL_INLINE_NODEBUG size_t get_size(const Object* self) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown)
{
  if constexpr (Object::Info::kSSOAvailable) {
    size_t sso = sso_size(self);
    size_t dyn = dyn_size(self);
    return self->_d.is_sso() ? sso : dyn;
  }
  else {
    return dyn_size(self);
  }
}

template<typename Object>
BL_INLINE_NODEBUG void set_sso_size(Object* self, size_t new_size) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown)
{
  self->_d._sso_data[Object::Info::kSSOSizeByte] = uint8_t(new_size);
}

template<typename Object>
BL_INLINE_NODEBUG void set_dyn_size(Object* self, size_t new_size) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown)
{
  if constexpr (Object::Info::kLayout == ObjectLayout::kNative64) {
    self->_d.dyn.size_u64 = new_size;
  }
  else {
    self->_d.dyn.size_u32 = uint32_t(new_size);
  }
}

template<typename Object>
BL_INLINE_NODEBUG void set_object_size(Object* self, size_t new_size) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown)
{
  if constexpr (Object::Info::kSSOAvailable) {
    if (self->_d.is_sso()) {
      set_sso_size(self, new_size);
    }
    else {
      set_dyn_size(self, new_size);
    }
  }
  else {
    set_dyn_size(self, new_size);
  }
}

template<typename Object>
[[nodiscard]] BL_INLINE_NODEBUG size_t sso_capacity(const Object* self) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown)
{
  (void)self;
  return Object::Info::kSSOCapacity;
}

template<typename Object>
[[nodiscard]] BL_INLINE_NODEBUG size_t dyn_capacity(const Object* self) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown)
{
  if constexpr (Object::Info::kLayout == ObjectLayout::kNative64) {
    if constexpr (std::endian::native == std::endian::little) {
      return size_t((self->_d.dyn.capacity_u48 << 16) >> 16);
    }
    else {
      return size_t(self->_d.dyn.capacity_u48 >> 16);
    }
  }
  else {
    return size_t(self->_d.dyn.capacity_u32);
  }
}

template<typename Object>
[[nodiscard]] BL_INLINE_NODEBUG size_t get_capacity(const Object* self) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown)
{
  if constexpr (Object::Info::kSSOAvailable) {
    size_t sso = sso_capacity(self);
    size_t dyn = dyn_capacity(self);
    return self->_d.is_sso() ? sso : dyn;
  }
  else {
    return dyn_capacity(self);
  }
}


template<typename Object>
[[nodiscard]] BL_INLINE_NODEBUG size_t get_capacity_of_array(const Object* self, size_t sso_capacity) noexcept
  requires (Object::Info::kLayout != ObjectLayout::kUnknown)
{
  size_t dyn = dyn_capacity(self);
  return self->_d.is_sso() ? sso_capacity : dyn;
}

// Object Layer - Fields
// =====================

template<typename T, size_t ByteIndex, uint32_t Mask>
struct ByteField {
  using Type = T;
  static constexpr size_t kByteIndex = ByteIndex;
  static constexpr uint32_t kMask = Mask;
  static constexpr uint32_t kShift = std::countr_zero(kMask);
};

template<typename T, uint32_t Mask>
struct PayloadField {
  using Type = T;
  static constexpr uint32_t kMask = Mask;
  static constexpr uint32_t kShift = std::countr_zero(kMask);
};

template<typename Field>
[[nodiscard]] BL_INLINE_CONSTEXPR typename Field::Type get_byte_field(const BLObjectCore* self) noexcept {
  using T = Field::Type;

  constexpr uint32_t byte_index = Field::kByteIndex;

  uint32_t value = self->_d._sso_data[byte_index];

  if constexpr (Field::kMask == 0xFFu) {
    return T(value);
  }
  else {
    return T((value >> Field::kShift) & (Field::kMask >> Field::kShift));
  }
}

template<typename Field>
BL_INLINE_CONSTEXPR void set_byte_field(BLObjectCore* self, typename Field::Type value) noexcept {
  constexpr uint32_t byte_index = Field::kByteIndex;

  if constexpr (Field::kMask == 0xFFu) {
    self->_d._sso_data[byte_index] = uint8_t(value);
  }
  else {
    uint32_t prev_byte = self->_d._sso_data[byte_index];
    self->_d._sso_data[byte_index] = uint8_t((prev_byte & uint8_t(~Field::kMask)) | (uint32_t(value) << Field::kShift));
  }
}

[[nodiscard]] BL_INLINE_NODEBUG uint32_t get_payload_u16(const BLObjectCore* self) noexcept {
  return self->_d.dyn.payload_u16;
}

BL_INLINE_NODEBUG BLResult set_payload_u16(BLObjectCore* self, uint32_t payload) noexcept {
  self->_d.dyn.payload_u16 = uint16_t(payload);
  return BL_SUCCESS;
}

template<typename Field>
[[nodiscard]] BL_INLINE_NODEBUG typename Field::Type get_payload_field(const BLObjectCore* self) noexcept {
  using T = Field::Type;
  return T((get_payload_u16(self) >> Field::kShift) & (Field::kMask >> Field::kShift));
}

template<typename Field>
BL_INLINE_NODEBUG BLResult set_payload_field(BLObjectCore* self, typename Field::Type value) noexcept {
  if constexpr (Field::kMask == 0xFFFFu) {
    return set_payload_u16(self, uint16_t(value));
  }
  else {
    return set_payload_u16(self, (get_payload_u16(self) & static_cast<uint16_t>(~Field::kMask)) | (uint32_t(value) << Field::kShift));
  }
}

// Object Layer - Init
// ===================

//! Initializes this Object to represent `object_type` that uses SSO storage.
template<typename Object>
BL_INLINE_NODEBUG BLResult init_sso(Object* self, BLObjectType object_type) noexcept {
  self->_d.zero_all();
  self->_d._sso_data[ObjectDetail::kTypeByteIndex] = uint8_t(object_type);
  return BL_SUCCESS;
}

//! Initializes this Object to represent the same object type that uses SSO storage.
template<typename Object>
BL_INLINE_NODEBUG BLResult init_sso(Object* self) noexcept {
  return init_sso(self, Object::Info::kType);
}

//! Initializes this BLObjectDetail to represent an object that uses static storage and sets it SSO size to `size_byte`.
template<typename Object>
BL_INLINE_NODEBUG BLResult init_sso_with_size(Object* self, BLObjectType object_type, size_t size) noexcept {
  init_sso(self, object_type);
  self->_d._sso_data[Object::Info::kSSOSizeByte] = uint8_t(size);
  return BL_SUCCESS;
}

//! Initializes this BLObjectDetail to represent an object that uses static storage and sets it SSO size to `size_byte`.
template<typename Object>
BL_INLINE_NODEBUG BLResult init_sso_with_size(Object* self, size_t size) noexcept {
  return init_sso_with_size(self, Object::Info::kType, size);
}

//! Initializes this BLObjectDetail to represent an object that uses static storage and copies the given `data` to `_sso_data`.
template<typename Object>
BL_INLINE_NODEBUG BLResult init_sso_with_data(Object* self, BLObjectType object_type, size_t size, const void* data, size_t data_byte_size) noexcept {
  init_sso_with_size(self, object_type, size);

  BL_ASSUME(data_byte_size <= ObjectDetail::kSSOCapacity);
  memcpy(self->_d._sso_data, data, data_byte_size);
  return BL_SUCCESS;
}

//! Initializes this BLObjectDetail to represent an object that uses static storage and copies the given `data` to `_sso_data`.
template<typename T, typename Object>
BL_INLINE_NODEBUG BLResult init_sso_with_data(Object* self, BLObjectType object_type, size_t size, BLSpan<T> data) noexcept {
  return init_sso_with_data(self, object_type, size, data.data(), data.size() * sizeof(T));
}

template<typename Object>
BL_INLINE_NODEBUG BLResult init_sso_with_data(Object* self, size_t size, const void* data, size_t data_byte_size) noexcept {
  return init_sso_with_data(self, Object::Info::kType, size, data, data_byte_size);
}

template<typename T, typename Object>
BL_INLINE_NODEBUG BLResult init_sso_with_data(Object* self, size_t size, BLSpan<T> data) noexcept {
  return init_sso_with_data(self, Object::Info::kType, size, data.data(), data.size() * sizeof(T));
}

template<typename T, typename Object>
BL_INLINE_NODEBUG BLResult init_sso_with_value(Object* self, BLObjectType object_type, T value) noexcept {
  init_sso(self, object_type);
  sso_value_mut<T>(self) = value;
  return BL_SUCCESS;
}

template<typename T, typename Object>
BL_INLINE_NODEBUG BLResult append_sso_data(Object* self, BLSpan<const T> data) noexcept {
  BL_ASSERT(self->_d.is_sso());

  size_t self_size = sso_size(self);
  size_t data_size = data.size();
  BL_ASSERT(BLObjectDetail::kSSOCapacity - self_size * sizeof(T) >= data_size * sizeof(T));

  T* dst = sso_data<T>(self);
  memcpy(dst + self_size, data.data(), data_size * sizeof(T));

  size_t result_size = self_size + data_size;
  set_sso_size(self, result_size);

  return BL_SUCCESS;
}

template<typename Object>
BL_INLINE_NODEBUG BLResult init_null(Object* self) noexcept {
  return init_sso(self, BL_OBJECT_TYPE_NULL);
}

template<typename Object>
BL_INLINE_NODEBUG BLResult init_bool(Object* self, bool value) noexcept { return init_sso_with_value(self, BL_OBJECT_TYPE_BOOL, uint64_t(value)); }

template<typename Object>
BL_INLINE_NODEBUG BLResult init_int64(Object* self, int64_t value) noexcept { return init_sso_with_value(self, BL_OBJECT_TYPE_INT64, value); }

template<typename Object>
BL_INLINE_NODEBUG BLResult init_uint64(Object* self, uint64_t value) noexcept { return init_sso_with_value(self, BL_OBJECT_TYPE_UINT64, value); }

template<typename Object>
BL_INLINE_NODEBUG BLResult init_double(Object* self, double value) noexcept { return init_sso_with_value(self, BL_OBJECT_TYPE_FLOAT64, value); }

template<typename Object>
BL_INLINE_NODEBUG BLResult init_rgba(Object* self, const BLRgba& rgba) noexcept { return init_sso_with_value(self, BL_OBJECT_TYPE_RGBA, rgba); }

template<typename Object>
BL_INLINE_NODEBUG BLResult init_rgba32(Object* self, uint32_t rgba32) noexcept { return init_sso_with_value(self, BL_OBJECT_TYPE_RGBA32, rgba32); }

template<typename Object>
BL_INLINE_NODEBUG BLResult init_rgba64(Object* self, uint64_t rgba64) noexcept { return init_sso_with_value(self, BL_OBJECT_TYPE_RGBA64, rgba64); }

//! Internal helper function that can be used to optimize out calling a function that would only
//! need to be called in case that the object is dynamic and reference counted. At the moment it's
//! used to determine whether a type inherited from BLObject requires to call a C-API destructor.
#if defined(__GNUC__)
template<typename Object>
[[nodiscard]] BL_INLINE_NODEBUG bool is_comptime_sso(const Object* self) noexcept {
  if BL_BUILTIN_CONSTANT_P(self->_d._sso_data[ObjectDetail::kMetaByteIndex]) {
    return (self->_d._sso_data[ObjectDetail::kMetaByteIndex] & ObjectDetail::kMetaFlagD) == 0u;
  }

  return false;
}

template<typename Object>
[[nodiscard]] BL_INLINE_NODEBUG bool requires_destroy(const Object* self) noexcept {
  if BL_BUILTIN_CONSTANT_P(self->_d._sso_data[ObjectDetail::kMetaByteIndex]) {
    return (self->_d._sso_data[ObjectDetail::kMetaByteIndex] & ObjectDetail::kMetaFlagR) != 0u;
  }
  else {
    return true;
  }
}
#else
template<typename Object>
[[nodiscard]] BL_INLINE_NODEBUG bool is_comptime_sso([[maybe_unused]] const Object* self) noexcept {
  return false;
}

template<typename Object>
[[nodiscard]] BL_INLINE_CONSTEXPR bool requires_destroy(const Object*) noexcept {
  return true;
}
#endif

// Object Layer - Debug
// ====================

template<typename Object>
[[nodiscard]] BL_INLINE_NODEBUG bool is_object_valid(const Object* self) noexcept {
  static_assert(Object::Info::kType != BL_OBJECT_TYPE_UNKNOWN,
                "Cannot validate object with unstable object type such as 'BLObjectCore' or 'BLArrayCore'");
  return self->_d.type() == Object::Info::kType;
}

} // {anonymous}
} // {bl::ObjectLayer}
#endif

//! \}

#endif // BLEND2D_CORE_OBJECT_LAYER_H_INCLUDED
