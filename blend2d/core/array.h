// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_ARRAY_H_INCLUDED
#define BLEND2D_CORE_ARRAY_H_INCLUDED

#include <blend2d/core/object.h>
#include <blend2d/core/object_layer.h>
#include <blend2d/core/span.h>

//! \addtogroup bl_c_api
//! \{

//! \name BLArray - C API
//!
//! Array functionality is provided by \ref BLArrayCore in C API and wrapped by \ref BLArray<T> template in C++ API.
//!
//! C API users must call either generic functions with `Item` suffix or correct specialized functions in case
//! of typed arrays. For example if you create a `BLArray<uint32_t>` in C then you can only modify it through
//! functions that have either `U32` or `Item` suffix. Arrays of signed types are treated as arrays of unsigned
//! types at API level as there is no difference between them from an implementation perspective.
//!
//! \{

//! Array container [C API].
struct BLArrayCore BL_INHERITS_FROM(BLObjectCore) {
#ifdef __cplusplus
  //! \cond INTERNAL
  using CoreType = BLArrayCore;
  using ImplType = BLArrayImpl;

  using Info = bl::ObjectLayer::ContainerInfo<
    BL_OBJECT_TYPE_UNKNOWN,
    bl::ObjectLayer::ObjectMode::kAny,
    bl::ObjectLayer::ObjectLayout::kNative,
    0u, 0u, BLObjectDetail::kMetaByteIndex>;

  template<typename T>
  [[nodiscard]] BL_INLINE_NODEBUG T& dcast() noexcept { return static_cast<T&>(*this); }

  template<typename T>
  [[nodiscard]] BL_INLINE_NODEBUG const T& dcast() const noexcept { return static_cast<const T&>(*this); }

  template<typename I = BLArrayImpl>
  [[nodiscard]] BL_INLINE_NODEBUG I* _impl() const noexcept { return _d.impl<I>(); }

  //! \endcond
#else
  BLObjectDetail _d;
#endif
};

//! \cond INTERNAL

#if defined(__cplusplus)
//! Array container [C++ API Impl].
struct BLArrayImpl : public BLObjectImpl {};
#endif

//! \endcond

BL_EXTERN_C_BEGIN

BL_API BLResult BL_CDECL bl_array_init(BLArrayCore* self, BLObjectType array_type) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_init_move(BLArrayCore* self, BLArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_init_weak(BLArrayCore* self, const BLArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_destroy(BLArrayCore* self) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL bl_array_reset(BLArrayCore* self) BL_NOEXCEPT_C;

BL_API size_t BL_CDECL bl_array_get_size(const BLArrayCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL bl_array_get_capacity(const BLArrayCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL bl_array_get_item_size(BLArrayCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const void* BL_CDECL bl_array_get_data(const BLArrayCore* self) BL_NOEXCEPT_C BL_PURE;

BL_API BLResult BL_CDECL bl_array_clear(BLArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_shrink(BLArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_reserve(BLArrayCore* self, size_t n) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_reserve_for_append(BLArrayCore* self, size_t n) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_resize(BLArrayCore* self, size_t new_size, const void* fill) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_truncate(BLArrayCore* self, size_t truncated_size) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL bl_array_make_mutable(BLArrayCore* self, void** data_out) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL bl_array_assign_move(BLArrayCore* self, BLArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_assign_weak(BLArrayCore* self, const BLArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_assign_deep(BLArrayCore* self, const BLArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_assign_data(BLArrayCore* self, const void* data, size_t n) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_assign_external_data(BLArrayCore* self, void* data, size_t size, size_t capacity, BLDataAccessMode access_mode, BLDestroyExternalDataFunc destroy_func, void* user_data) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_assign_uninitialized(BLArrayCore* self, size_t n, void** data_out) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL bl_array_append_u8(BLArrayCore* self, uint8_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_append_u16(BLArrayCore* self, uint16_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_append_u32(BLArrayCore* self, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_append_u64(BLArrayCore* self, uint64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_append_f32(BLArrayCore* self, float value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_append_f64(BLArrayCore* self, double value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_append_item(BLArrayCore* self, const void* item) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_append_data(BLArrayCore* self, const void* data, size_t n) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_append_uninitialized(BLArrayCore* self, size_t n, void** data_out) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL bl_array_insert_u8(BLArrayCore* self, size_t index, uint8_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_insert_u16(BLArrayCore* self, size_t index, uint16_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_insert_u32(BLArrayCore* self, size_t index, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_insert_u64(BLArrayCore* self, size_t index, uint64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_insert_f32(BLArrayCore* self, size_t index, float value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_insert_f64(BLArrayCore* self, size_t index, double value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_insert_item(BLArrayCore* self, size_t index, const void* item) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_insert_data(BLArrayCore* self, size_t index, const void* data, size_t n) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_insert_uninitialized(BLArrayCore* self, size_t index, size_t n, void** data_out) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL bl_array_replace_u8(BLArrayCore* self, size_t index, uint8_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_replace_u16(BLArrayCore* self, size_t index, uint16_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_replace_u32(BLArrayCore* self, size_t index, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_replace_u64(BLArrayCore* self, size_t index, uint64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_replace_f32(BLArrayCore* self, size_t index, float value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_replace_f64(BLArrayCore* self, size_t index, double value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_replace_item(BLArrayCore* self, size_t index, const void* item) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_replace_data(BLArrayCore* self, size_t r_start, size_t r_end, const void* data, size_t n) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL bl_array_remove_index(BLArrayCore* self, size_t index) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_array_remove_range(BLArrayCore* self, size_t start, size_t end) BL_NOEXCEPT_C;

BL_API bool BL_CDECL bl_array_equals(const BLArrayCore* a, const BLArrayCore* b) BL_NOEXCEPT_C BL_PURE;

BL_EXTERN_C_END

//! \}

//! \}

//! \addtogroup bl_containers
//! \{

//! \cond INTERNAL
//! \name BLArray - Internals
//! \{

#ifdef __cplusplus
namespace bl::Internal {
namespace {

template<typename T, uint32_t TypeCategory>
struct ArrayTraitsByCategory {
  static constexpr const BLObjectType kObjectType = BL_OBJECT_TYPE_NULL;

  typedef T CompatibleType;
  static BL_INLINE_NODEBUG const T& pass(const T& arg) noexcept { return arg; }
};

template<typename T>
struct ArrayTraitsByCategory<T, kTypeCategoryObject> {
  static constexpr const BLObjectType kObjectType = BL_OBJECT_TYPE_ARRAY_OBJECT;

  typedef T CompatibleType;
  static BL_INLINE_NODEBUG const CompatibleType& pass(const T& arg) noexcept { return arg; }
};

template<typename T>
struct ArrayTraitsByCategory<T, kTypeCategoryPtr> {
  static constexpr const BLObjectType kObjectType =
    sizeof(T) == 4 ? BL_OBJECT_TYPE_ARRAY_UINT32 : BL_OBJECT_TYPE_ARRAY_UINT64;

  using CompatibleType = UIntByType<T>;
  static BL_INLINE_NODEBUG CompatibleType pass(const T& arg) noexcept { return (CompatibleType)arg; }
};

template<typename T>
struct ArrayTraitsByCategory<T, kTypeCategoryInt> {
  static constexpr const BLObjectType kObjectType =
    sizeof(T) == 1 && std::is_signed_v  <T> ? BL_OBJECT_TYPE_ARRAY_INT8   :
    sizeof(T) == 1 && std::is_unsigned_v<T> ? BL_OBJECT_TYPE_ARRAY_UINT8  :
    sizeof(T) == 2 && std::is_signed_v  <T> ? BL_OBJECT_TYPE_ARRAY_INT16  :
    sizeof(T) == 2 && std::is_unsigned_v<T> ? BL_OBJECT_TYPE_ARRAY_UINT16 :
    sizeof(T) == 4 && std::is_signed_v  <T> ? BL_OBJECT_TYPE_ARRAY_INT32  :
    sizeof(T) == 4 && std::is_unsigned_v<T> ? BL_OBJECT_TYPE_ARRAY_UINT32 :
    sizeof(T) == 8 && std::is_signed_v  <T> ? BL_OBJECT_TYPE_ARRAY_INT64  :
    sizeof(T) == 8 && std::is_unsigned_v<T> ? BL_OBJECT_TYPE_ARRAY_UINT64 : BL_OBJECT_TYPE_NULL;

  using CompatibleType = UIntByType<T>;
  static BL_INLINE_NODEBUG CompatibleType pass(const T& arg) noexcept { return (CompatibleType)arg; }
};

template<typename T>
struct ArrayTraitsByCategory<T, kTypeCategoryFloat> {
  static constexpr const BLObjectType kObjectType =
    sizeof(T) == 4 ? BL_OBJECT_TYPE_ARRAY_FLOAT32 :
    sizeof(T) == 8 ? BL_OBJECT_TYPE_ARRAY_FLOAT64 : BL_OBJECT_TYPE_NULL;

  typedef T CompatibleType;
  static BL_INLINE_NODEBUG CompatibleType pass(const T& arg) noexcept { return (CompatibleType)arg; }
};

template<typename T>
struct ArrayTraitsByCategory<T, kTypeCategoryStruct> {
  static constexpr const BLObjectType kObjectType =
    sizeof(T) ==  1 ? BL_OBJECT_TYPE_ARRAY_STRUCT_1  :
    sizeof(T) ==  2 ? BL_OBJECT_TYPE_ARRAY_STRUCT_2  :
    sizeof(T) ==  3 ? BL_OBJECT_TYPE_ARRAY_STRUCT_3  :
    sizeof(T) ==  4 ? BL_OBJECT_TYPE_ARRAY_STRUCT_4  :
    sizeof(T) ==  6 ? BL_OBJECT_TYPE_ARRAY_STRUCT_6  :
    sizeof(T) ==  8 ? BL_OBJECT_TYPE_ARRAY_STRUCT_8  :
    sizeof(T) == 10 ? BL_OBJECT_TYPE_ARRAY_STRUCT_10 :
    sizeof(T) == 12 ? BL_OBJECT_TYPE_ARRAY_STRUCT_12 :
    sizeof(T) == 16 ? BL_OBJECT_TYPE_ARRAY_STRUCT_16 :
    sizeof(T) == 20 ? BL_OBJECT_TYPE_ARRAY_STRUCT_20 :
    sizeof(T) == 24 ? BL_OBJECT_TYPE_ARRAY_STRUCT_24 :
    sizeof(T) == 32 ? BL_OBJECT_TYPE_ARRAY_STRUCT_32 : BL_OBJECT_TYPE_NULL;

  typedef T CompatibleType;
  static BL_INLINE_NODEBUG CompatibleType pass(const T& arg) noexcept { return (CompatibleType)arg; }
};

template<typename T>
struct ArrayTraits : public ArrayTraitsByCategory<T, TypeTraits<T>::kCategory> {};

template<typename T>
BL_INLINE_NODEBUG const T& first_in_var_args(const T& arg) noexcept { return arg; }
template<typename T, typename... Args>
BL_INLINE_NODEBUG const T& first_in_var_args(const T& arg, Args&&...) noexcept { return arg; }

template<typename T, typename Arg0>
BL_INLINE_NODEBUG void copy_to_uninitialized(T* dst, Arg0&& src) noexcept {
  bl_call_ctor(*dst, forward<Arg0>(src));
}

template<typename T, typename Arg0, typename... Args>
BL_INLINE_NODEBUG void copy_to_uninitialized(T* dst, Arg0&& arg0, Args&&... args) noexcept {
  copy_to_uninitialized(dst + 0, forward<Arg0>(arg0));
  copy_to_uninitialized(dst + 1, forward<Args>(args)...);
}

template<typename T> BL_INLINE_NODEBUG BLResult append_item(BLArrayCore* self, const T& item) noexcept { return bl_array_append_item(self, &item); }
template<> BL_INLINE_NODEBUG BLResult append_item(BLArrayCore* self, const uint8_t& item) noexcept { return bl_array_append_u8(self, item); }
template<> BL_INLINE_NODEBUG BLResult append_item(BLArrayCore* self, const uint16_t& item) noexcept { return bl_array_append_u16(self, item); }
template<> BL_INLINE_NODEBUG BLResult append_item(BLArrayCore* self, const uint32_t& item) noexcept { return bl_array_append_u32(self, item); }
template<> BL_INLINE_NODEBUG BLResult append_item(BLArrayCore* self, const uint64_t& item) noexcept { return bl_array_append_u64(self, item); }
template<> BL_INLINE_NODEBUG BLResult append_item(BLArrayCore* self, const float& item) noexcept { return bl_array_append_f32(self, item); }
template<> BL_INLINE_NODEBUG BLResult append_item(BLArrayCore* self, const double& item) noexcept { return bl_array_append_f64(self, item); }

template<typename T> BL_INLINE_NODEBUG BLResult insert_item(BLArrayCore* self, size_t index, const T& item) noexcept { return bl_array_insert_item(self, index, &item); }
template<> BL_INLINE_NODEBUG BLResult insert_item(BLArrayCore* self, size_t index, const uint8_t& item) noexcept { return bl_array_insert_u8(self, index, item); }
template<> BL_INLINE_NODEBUG BLResult insert_item(BLArrayCore* self, size_t index, const uint16_t& item) noexcept { return bl_array_insert_u16(self, index, item); }
template<> BL_INLINE_NODEBUG BLResult insert_item(BLArrayCore* self, size_t index, const uint32_t& item) noexcept { return bl_array_insert_u32(self, index, item); }
template<> BL_INLINE_NODEBUG BLResult insert_item(BLArrayCore* self, size_t index, const uint64_t& item) noexcept { return bl_array_insert_u64(self, index, item); }
template<> BL_INLINE_NODEBUG BLResult insert_item(BLArrayCore* self, size_t index, const float& item) noexcept { return bl_array_insert_f32(self, index, item); }
template<> BL_INLINE_NODEBUG BLResult insert_item(BLArrayCore* self, size_t index, const double& item) noexcept { return bl_array_insert_f64(self, index, item); }

template<typename T> BL_INLINE_NODEBUG BLResult replace_item(BLArrayCore* self, size_t index, const T& item) noexcept { return bl_array_replace_item(self, index, &item); }
template<> BL_INLINE_NODEBUG BLResult replace_item(BLArrayCore* self, size_t index, const uint8_t& item) noexcept { return bl_array_replace_u8(self, index, item); }
template<> BL_INLINE_NODEBUG BLResult replace_item(BLArrayCore* self, size_t index, const uint16_t& item) noexcept { return bl_array_replace_u16(self, index, item); }
template<> BL_INLINE_NODEBUG BLResult replace_item(BLArrayCore* self, size_t index, const uint32_t& item) noexcept { return bl_array_replace_u32(self, index, item); }
template<> BL_INLINE_NODEBUG BLResult replace_item(BLArrayCore* self, size_t index, const uint64_t& item) noexcept { return bl_array_replace_u64(self, index, item); }
template<> BL_INLINE_NODEBUG BLResult replace_item(BLArrayCore* self, size_t index, const double& item) noexcept { return bl_array_replace_f64(self, index, item); }

} // {anonymous}
} // {bl::Internal}
#endif

//! \}
//! \endcond

//! \name BLArray - C++ API
//! \{
#ifdef __cplusplus

//! Array container (template) [C++ API].
template<typename T>
class BLArray final : public BLArrayCore {
public:
  //! \cond INTERNAL
  //! \name Internals
  //! \{

  //! Array traits of `T`.
  using Traits = bl::Internal::ArrayTraits<T>;

  using Info = bl::ObjectLayer::ContainerInfo<
    Traits::kObjectType,
    (BLObjectDetail::kSSOCapacity / sizeof(T) >= 1u) ? bl::ObjectLayer::ObjectMode::kAny : bl::ObjectLayer::ObjectMode::kDyn,
    bl::ObjectLayer::ObjectLayout::kNative,
    (BLObjectDetail::kSSOCapacity / sizeof(T)), 0u, BLObjectDetail::kMetaByteIndex>;

  static_assert(Info::kType >= BL_OBJECT_TYPE_MIN_ARRAY &&
                Info::kType <= BL_OBJECT_TYPE_MAX_ARRAY,
                "BLArray<T> requires `T` to be either bool, integer, floating point, BLObject-derived type, or a trivial type of a supported size");

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  //! Creates a default constructed array.
  BL_INLINE_NODEBUG BLArray() noexcept {
    if (Info::kSSOAvailable) {
      bl::ObjectLayer::init_sso(this);
    }
    else {
      bl_array_init(this, Info::kType);
    }
  }

  //! Move constructor.
  //!
  //! \remarks The `other` array is always reset by move constructor, so it becomes a default constructed empty array.
  BL_INLINE_NODEBUG BLArray(BLArray&& other) noexcept {
    if (Info::kSSOAvailable) {
      _d = other._d;
      bl::ObjectLayer::init_sso(&other);
    }
    else {
      bl_array_init_move(this, &other);
    }
  }

  //! Copy constructor, performs weak copy of the data held by the `other` array.
  BL_INLINE_NODEBUG BLArray(const BLArray& other) noexcept {
    bl_array_init_weak(this, &other);
  }

  //! Destroys the array.
  BL_INLINE_NODEBUG ~BLArray() noexcept {
    if (bl::ObjectLayer::requires_destroy(this)) {
      bl_array_destroy(this);
    }
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  //! Tests whether the array has items. Returns `true` if the array is not empty.
  //!
  //! \note This is essentially the opposite of `is_empty()`.
  [[nodiscard]] BL_INLINE_NODEBUG explicit operator bool() const noexcept { return !is_empty(); }

  //! Move assignment.
  //!
  //! \note The `other` array is reset by move assignment, so its state after the move operation is the same as
  //! the default constructed array.
  BL_INLINE_NODEBUG BLArray& operator=(BLArray&& other) noexcept { bl_array_assign_move(this, &other); return *this; }
  //! Copy assignment, performs weak copy of the data held by the `other` array.
  BL_INLINE_NODEBUG BLArray& operator=(const BLArray& other) noexcept { bl_array_assign_weak(this, &other); return *this; }

  //! Returns true if this and `other` arrays are equal.
  [[nodiscard]] BL_INLINE_NODEBUG bool operator==(const BLArray& other) const noexcept { return equals(other); }

  //! Returns true if this and `other` arrays are not equal.
  [[nodiscard]] BL_INLINE_NODEBUG bool operator!=(const BLArray& other) const noexcept { return !equals(other); }

  //! Returns a read-only reference to the array item at the given `index`.
  //!
  //! \note This is the same as calling `at(index)`.
  [[nodiscard]] BL_INLINE_NODEBUG const T& operator[](size_t index) const noexcept { return at(index); }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Resets the array into a default constructed state by clearing its content and releasing its memory.
  //!
  //! \note This function always returns \ref BL_SUCCESS.
  BL_INLINE_NODEBUG BLResult reset() noexcept {
    BLResult result = bl_array_reset(this);

    // Reset operation always succeeds.
    BL_ASSUME(result == BL_SUCCESS);

    // Assume a default constructed BLArray<T> after reset has a zero SSO size (if the array itself is SSO).
    if (Info::kSSOAvailable) {
      BL_ASSUME(_d._sso_data[BLObjectDetail::kMetaByteIndex] == 0);
    }

    return result;
  }

  //! Swaps the content of this array with the `other` array.
  BL_INLINE_NODEBUG void swap(BLArray<T>& other) noexcept { _d.swap(other._d); }

  //! \}

  //! \name Accessors
  //! \{

  //! Tests whether the array is empty.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_empty() const noexcept { return bl::ObjectLayer::get_size(this) == 0; }
  //! Returns the size of the array (number of items).
  [[nodiscard]] BL_INLINE_NODEBUG size_t size() const noexcept { return bl::ObjectLayer::get_size(this); }
  //! Returns the capacity of the array (number of items).
  [[nodiscard]] BL_INLINE_NODEBUG size_t capacity() const noexcept { return bl::ObjectLayer::get_capacity(this); }

  //! Returns a pointer to the array data.
  [[nodiscard]] BL_INLINE_NODEBUG const T* data() const noexcept { return bl::ObjectLayer::get_data<T>(this); }

  //! Returns a pointer to the array data (iterator compatibility).
  [[nodiscard]] BL_INLINE_NODEBUG const T* begin() const noexcept { return data(); }
  //! Returns a pointer to the end of array data (iterator compatibility).
  [[nodiscard]] BL_INLINE_NODEBUG const T* end() const noexcept { return begin() + size(); }

  //! Returns the array data as `BLSpan<const T>`.
  [[nodiscard]] BL_INLINE_NODEBUG BLSpan<const T> span() const noexcept { return BLSpan<const T> { data(), size() }; }

  //! Returns a read-only reference to the array item at the given `index`.
  //!
  //! \note The index must be valid, which means it has to be less than the array length. Accessing items out of range
  //! is undefined behavior that would be caught by assertions in debug builds.
  [[nodiscard]] BL_INLINE const T& at(size_t index) const noexcept {
    BL_ASSERT(index < size());
    return data()[index];
  }

  //! Returns a read-only reference to the first item.
  //!
  //! \note The array must have at least one item otherwise calling `first()` would point to the end of the array,
  //! which is not initialized, and such reference would be invalid. Debug builds would catch this condition with
  //! an assertion.
  [[nodiscard]] BL_INLINE_NODEBUG const T& first() const noexcept { return at(0); }

  //! Returns a read-only reference to the last item.
  //!
  //! \note The array must have at least one item otherwise calling `last()`  would point to the end of the array,
  //! which is not initialized, and such reference would be invalid. Debug builds would catch this condition with
  //! an assertion.
  [[nodiscard]] BL_INLINE_NODEBUG const T& last() const noexcept { return at(size() - 1); }

  //! \}

  //! \name Data Manipulation
  //! \{

  //! Clears the content of the array.
  //!
  //! \note If the array uses a dynamically allocated memory and the instance is mutable the memory won't be released,
  //! it will be reused instead. Consider using `reset()` if you want to release the memory in such case instead.
  BL_INLINE_NODEBUG BLResult clear() noexcept { return bl_array_clear(this); }

  //! Shrinks the capacity of the array to fit its length.
  //!
  //! Some array operations like `append()` may grow the array more than necessary to make it faster when such
  //! manipulation operations are called consecutively. When you are done with modifications and you know the
  //! lifetime of the array won't be short you can use `shrink()` to fit its memory requirements to the number
  //! of items it stores, which could optimize the application's memory requirements.
  BL_INLINE_NODEBUG BLResult shrink() noexcept { return bl_array_shrink(this); }

  //! Reserves the array capacity to hold at least `min_capacity` items.
  //!
  //! \remarks The function would never try to resize the array when `min_capacity` is lesser than the array length.
  BL_INLINE_NODEBUG BLResult reserve(size_t min_capacity) noexcept { return bl_array_reserve(this, min_capacity); }

  //! Reserves the array capacity to hold the currently stored items and more items passed as `additional_size`.
  BL_INLINE_NODEBUG BLResult reserve_for_append(size_t additional_size) noexcept { return bl_array_reserve_for_append(this, additional_size); }

  //! Truncates the length of the array to maximum `n` items.
  //!
  //! If the length of the array is less than `n`n then truncation does nothing.
  BL_INLINE_NODEBUG BLResult truncate(size_t truncated_size) noexcept { return bl_array_truncate(this, truncated_size); }

  //! Resizes the array to `n` items.
  //!
  //! If `n` is greater than the array length then all new items will be initialized by `fill` item.
  BL_INLINE_NODEBUG BLResult resize(size_t n, const T& fill) noexcept { return bl_array_resize(this, n, &fill); }

  //! Makes the array mutable by possibly creating a deep copy of the data if it's either read-only or shared with
  //! another array. Stores the pointer to the beginning of mutable data in `data_out`.
  //!
  //! ```
  //! BLArray<uint8_t> a;
  //! if (a.append(0, 1, 2, 3, 4, 5, 6, 7) != BL_SUCCESS) {
  //!   // Handle error condition.
  //! }
  //!
  //! uint8_t* data;
  //! if (a.make_mutable(&data) != BL_SUCCESS) {
  //!   // Handle error condition.
  //! }
  //!
  //! // `data` is a mutable pointer to array content of 8 items.
  //! data[0] = 100;
  //!
  //! // Calling array member functions (or C API) could invalidate `data`.
  //! a.append(9); // You shouldn't use `data` afterwards.
  //! ```
  BL_INLINE_NODEBUG BLResult make_mutable(T** data_out) noexcept {
    return bl_array_make_mutable(this, (void**)data_out);
  }

  //! Move assignment, the same as `operator=`, but returns a `BLResult` instead of `this`.
  BL_INLINE_NODEBUG BLResult assign(BLArray<T>&& other) noexcept {
    return bl_array_assign_move(this, &other);
  }

  //! Copy assignment, the same as `operator=`, but returns a `BLResult` instead of `this`.
  BL_INLINE_NODEBUG BLResult assign(const BLArray<T>& other) noexcept {
    return bl_array_assign_weak(this, &other);
  }

  //! Copy assignment, but creates a deep copy of the `other` array instead of weak copy.
  BL_INLINE_NODEBUG BLResult assign_deep(const BLArray<T>& other) noexcept {
    return bl_array_assign_deep(this, &other);
  }

  //! Replaces the content of the array by items in the passed via `data` span or compatible.
  //!
  //! \remarks The implementation can handle `data` span pointing to the array's data as well, so it's possible
  //! to create a slice of the array itself.
  BL_INLINE_NODEBUG BLResult assign_data(BLSpanConvertible<const T> auto&& data) noexcept {
    return bl_array_assign_data(this, (const void*)data.data(), data.size());
  }

  //! Replaces the content of the array `items` of length `n`.
  //!
  //! \remarks The implementation can handle `items` pointing to the array's data as well, so it's possible
  //! to create a slice of the array itself.
  BL_INLINE_NODEBUG BLResult assign_data(const T* items, size_t n) noexcept {
    return bl_array_assign_data(this, (const void*)items, n);
  }

  //! Assign an external buffer to the array, which would replace the existing content.
  //!
  //! \param data External data buffer to use (cannot be NULL).
  //! \param size Size of the data buffer in items.
  //! \param capacity Capacity of the buffer, cannot be zero or smaller than `size`.
  //! \param access_mode Describes whether the data is read-only or read-write, see \ref BLDataAccessMode.
  //! \param destroy_func A function that would be called when the array is destroyed (can be null if you don't need it).
  //! \param user_data User data passed to `destroy_func`.
  BL_INLINE_NODEBUG BLResult assign_external_data(
    T* data, size_t size, size_t capacity,
    BLDataAccessMode access_mode,
    BLDestroyExternalDataFunc destroy_func = nullptr, void* user_data = nullptr
  ) noexcept {
    return bl_array_assign_external_data(this, data, size, capacity, access_mode, destroy_func, user_data);
  }

  BL_INLINE_NODEBUG BLResult assign_external_data_const(
    const T* data, size_t size, size_t capacity,
    BLDestroyExternalDataFunc destroy_func = nullptr, void* user_data = nullptr
  ) noexcept {
    return bl_array_assign_external_data(this, const_cast<T*>(data), size, capacity, BL_DATA_ACCESS_MODE_RO, destroy_func, user_data);
  }

  BL_INLINE_NODEBUG BLResult assign_external_data_mut(
    T* data, size_t size, size_t capacity,
    BLDestroyExternalDataFunc destroy_func = nullptr, void* user_data = nullptr
  ) noexcept {
    return bl_array_assign_external_data(this, data, size, capacity, BL_DATA_ACCESS_MODE_RW, destroy_func, user_data);
  }

  //! Replaces the content of the array with variadic number of items passed in `args...`.
  template<typename... Args>
  BL_INLINE_NODEBUG BLResult assign_v(Args&&... args) noexcept {
    T* dst;
    BL_PROPAGATE(bl_array_assign_uninitialized(this, sizeof...(args), (void**)&dst));
    bl::Internal::copy_to_uninitialized(dst, bl::Internal::forward<Args>(args)...);
    return BL_SUCCESS;
  }

  BL_INLINE_NODEBUG BLResult assign_uninitialized(size_t size, T** data_out) noexcept {
    return bl_array_assign_uninitialized(this, size, (void**)data_out);
  }

  //! Appends a variadic number of items items passed in `args...` to the array.
  //!
  //! \note The data in `args` cannot point to the same data that the array holds as the function that prepares the
  //! append operation has no way to know about the source (it only makes space for new data). It's an undefined
  //! behavior in such case.
  template<typename... Args>
  BL_INLINE BLResult append(Args&&... args) noexcept {
    if constexpr (sizeof...(args) == 1) {
      return bl::Internal::append_item(this, Traits::pass(bl::Internal::first_in_var_args(bl::Internal::forward<Args>(args)...)));
    }
    else {
      T* dst;
      BL_PROPAGATE(bl_array_append_uninitialized(this, sizeof...(args), (void**)&dst));
      bl::Internal::copy_to_uninitialized(dst, bl::Internal::forward<Args>(args)...);
      return BL_SUCCESS;
    }
  }

  //! Appends items to the array passed by `span`.
  //!
  //! \note The implementation guarantees that a `span` could point to the array data itself.
  BL_INLINE_NODEBUG BLResult append_data(BLSpanConvertible<const T> auto&& span) noexcept {
    return bl_array_append_data(this, (const void*)span.data(), span.size());
  }

  //! Appends items to the array passed through `data` and `size` arguments. The `size` describes the number of items in `data`.
  //!
  //! \note The implementation guarantees that `data` could point to the array data itself.
  BL_INLINE_NODEBUG BLResult append_data(const T* data, size_t size) noexcept {
    return bl_array_append_data(this, (const void*)data, size);
  }

  BL_INLINE_NODEBUG BLResult append_uninitialized(size_t size, T** data_out) noexcept {
    return bl_array_append_uninitialized(this, size, (void**)data_out);
  }

  //! Prepends a variadic number of items items passed in `args...` to the array.
  //!
  //! \note The data in `args` cannot point to the same data that the array holds as the function that prepares the
  //! prepend operation has no way to know about the source (it only makes space for new data). It's an undefined
  //! behavior in such case.
  template<typename... Args>
  BL_INLINE BLResult prepend(Args&&... args) noexcept {
    if constexpr (sizeof...(args) == 1)
      return bl::Internal::insert_item(this, 0, Traits::pass(bl::Internal::first_in_var_args(bl::Internal::forward<Args>(args)...)));
    else
      return insert(0, bl::Internal::forward<Args>(args)...);
  }

  //! Prepends items to the array of the given array `span`.
  //!
  //! \note The implementation guarantees that a `view` pointing to the array data itself would work.
  BL_INLINE_NODEBUG BLResult prepend_data(BLSpanConvertible<const T> auto&& span) noexcept {
    return bl_array_insert_data(this, 0, (const void*)span.data(), span.size());
  }

  //! Prepends `items` to the array of length `n`.
  //!
  //! \note The implementation guarantees that a `items` pointing to the array data itself would work.
  BL_INLINE_NODEBUG BLResult prepend_data(const T* data, size_t size) noexcept {
    return bl_array_insert_data(this, 0, (const void*)data, size);
  }

  //! Prepare for prepend, but don't initialize the data to be prepended.
  //!
  //! \remarks The caller is responsible for initializing the data returned in `data_out` when the call succeeds.
  BL_INLINE_NODEBUG BLResult prepend_uninitialized(size_t n, T** data_out) noexcept {
    return bl_array_insert_uninitialized(this, 0u, n, (void**)data_out);
  }

  //! Inserts a variadic number of items items passed in `args...` at the given `index`.
  //!
  //! \note The data in `args` cannot point to the same data that the array holds as the function that prepares the
  //! insert operation has no way to know about the source (it only makes space for new data). It's an undefined
  //! behavior in such case.
  template<typename... Args>
  BL_INLINE BLResult insert(size_t index, Args&&... args) noexcept {
    if constexpr (sizeof...(args) == 1) {
      return bl::Internal::insert_item(this, index, Traits::pass(bl::Internal::first_in_var_args(bl::Internal::forward<Args>(args)...)));
    }
    else {
      T* dst;
      BL_PROPAGATE(bl_array_insert_uninitialized(this, index, sizeof...(args), (void**)&dst));
      bl::Internal::copy_to_uninitialized(dst, bl::Internal::forward<Args>(args)...);
      return BL_SUCCESS;
    }
  }

  //! Inserts items to the array of the given array `view` at the given `index`.
  //!
  //! \note The implementation guarantees that a `view` pointing to the array data itself would work.
  BL_INLINE_NODEBUG BLResult insert_data(size_t index, BLSpanConvertible<const T> auto&& span) noexcept {
    return bl_array_insert_data(this, index, (const void*)span.data(), span.size());
  }

  //! Prepends `items` to the array of length `n` at the given `index`.
  //!
  //! \note The implementation guarantees that a `items` pointing to the array data itself would work.
  BL_INLINE_NODEBUG BLResult insert_data(size_t index, const T* data, size_t size) noexcept {
    return bl_array_insert_data(this, index, (const void*)data, size);
  }

  //! Prepare for insert, but don't initialize the data to be inserted.
  //!
  //! \remarks The caller is responsible for initializing the data returned in `data_out` when the call succeeds.
  BL_INLINE_NODEBUG BLResult insert_uninitialized(size_t index, size_t n, T** data_out) noexcept {
    return bl_array_insert_uninitialized(this, index, n, (void**)data_out);
  }

  //! Replaces an item at the given `index` by `item`.
  BL_INLINE_NODEBUG BLResult replace(size_t index, const T& item) noexcept {
    return bl::Internal::replace_item(this, index, Traits::pass(item));
  }

  //! Replaces the given `range` of items by the given array `span`.
  BL_INLINE_NODEBUG BLResult replace_data(const BLRange& range, BLSpanConvertible<const T> auto&& span) noexcept {
    return bl_array_replace_data(this, range.start, range.end, (const void*)span.data(), span.size());
  }

  //! Replaces the given `range` of items by `items` of length `n`.
  BL_INLINE_NODEBUG BLResult replace_data(const BLRange& range, const T* data, size_t size) noexcept {
    return bl_array_replace_data(this, range.start, range.end, data, size);
  }

  //! Removes an item at the given `index`.
  BL_INLINE_NODEBUG BLResult remove(size_t index) noexcept {
    return bl_array_remove_index(this, index);
  }

  //! Removes a `range` of items.
  BL_INLINE_NODEBUG BLResult remove(BLRange range) noexcept {
    return bl_array_remove_range(this, range.start, range.end);
  }

  //! Removes a `range` of items.
  BL_INLINE_NODEBUG BLResult remove(size_t start, size_t end) noexcept {
    return bl_array_remove_range(this, start, end);
  }

  //! \}

  //! \name Manipulation
  //! \{

  BL_INLINE_NODEBUG void _set_size_unsafe(size_t new_size) noexcept {
    BL_ASSERT(new_size <= capacity());
    bl::ObjectLayer::set_object_size(this, new_size);
  }

  BL_INLINE_NODEBUG void _set_size_from_end_ptr_unsafe(const T* end_ptr) noexcept {
    BL_ASSERT(end_ptr >= data() && end_ptr <= data() + capacity());
    bl::ObjectLayer::set_object_size(this, (size_t)(end_ptr - data()));
  }

  //! \}

  //! \name Equality & Comparison
  //! \{

  //! Returns whether the content of this array and `other` matches.
  [[nodiscard]] BL_INLINE_NODEBUG bool equals(const BLArray<T>& rhs) const noexcept {
    return bl_array_equals(this, &rhs);
  }

  //! Returns whether the content of this array and `span` matches.
  [[nodiscard]] BL_INLINE_NODEBUG bool equals(BLSpanConvertible<const T> auto&& rhs) const noexcept
    requires (!std::same_as<std::remove_cvref_t<decltype(rhs)>, BLArray<T>>)
  {
    return span() == BLSpan<const T>(rhs.data(), rhs.size());
  }

  //! \}

  //! \name Search
  //! \{

  //! Returns the first index at which a given `item` can be found in the array, or `SIZE_MAX` if not found.
  [[nodiscard]] BL_INLINE size_t index_of(const T& item) const noexcept {
    return span().index_of(item, 0u);
  }

  //! Returns the index at which a given `item` can be found in the array starting from `from_index`, or `SIZE_MAX`
  //! if not present.
  [[nodiscard]] BL_INLINE size_t index_of(const T& item, size_t from_index) const noexcept {
    return span().index_of(item, from_index);
  }

  //! Returns the last index at which a given `item` can be found in the array, or `SIZE_MAX` if not present.
  [[nodiscard]] BL_INLINE size_t last_index_of(const T& item) const noexcept {
    return span().last_index_of(item);
  }

  //! Returns the index at which a given `item` can be found in the array starting from `from_index` and ending
  //! at `0`, or `SIZE_MAX` if not present.
  [[nodiscard]] BL_INLINE size_t last_index_of(const T& item, size_t from_index) const noexcept {
    return span().last_index_of(item, from_index);
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_CORE_ARRAY_H_INCLUDED
