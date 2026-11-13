// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/commons.h>
#include <blend2d/axl/lookuptable_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/core/string_p.h>
#include <blend2d/core/var.h>

namespace bl {

// Array - Internal - Null
// =======================

static constexpr ObjectLayer::EternalImpl<BLArrayImpl> null_array {};

// Array - Internal - Tables
// =========================

struct DivMulConst64 {
  uint64_t multiply;
  uint8_t hi_shift;
  uint8_t is_one;
};

static consteval DivMulConst64 div_mul_const_64(uint64_t divisor) noexcept {
  switch (divisor) {
    case  1: return DivMulConst64{0x0000000000000000u, 0u, 1u};
    case  2: return DivMulConst64{0x8000000000000000u, 0u, 0u};
    case  3: return DivMulConst64{0xAAAAAAAAAAAAAAABu, 1u, 0u};
    case  4: return DivMulConst64{0x8000000000000000u, 1u, 0u};
    case  6: return DivMulConst64{0xAAAAAAAAAAAAAAABu, 2u, 0u};
    case  8: return DivMulConst64{0x8000000000000000u, 2u, 0u};
    case 10: return DivMulConst64{0xCCCCCCCCCCCCCCCDu, 3u, 0u};
    case 12: return DivMulConst64{0xAAAAAAAAAAAAAAABu, 3u, 0u};
    case 16: return DivMulConst64{0x8000000000000000u, 3u, 0u};
    case 20: return DivMulConst64{0xCCCCCCCCCCCCCCCDu, 4u, 0u};
    case 24: return DivMulConst64{0xAAAAAAAAAAAAAAABu, 4u, 0u};
    case 32: return DivMulConst64{0x8000000000000000u, 4u, 0u};
    default: return DivMulConst64{0x0000000000000000u, 0u, 0u};
  }
}

struct ArrayItemSizeGenerator {
  static consteval uint8_t value(size_t impl_type) noexcept {
    return impl_type == BL_OBJECT_TYPE_ARRAY_INT8      ? uint8_t(1) :
           impl_type == BL_OBJECT_TYPE_ARRAY_UINT8     ? uint8_t(1) :
           impl_type == BL_OBJECT_TYPE_ARRAY_INT16     ? uint8_t(2) :
           impl_type == BL_OBJECT_TYPE_ARRAY_UINT16    ? uint8_t(2) :
           impl_type == BL_OBJECT_TYPE_ARRAY_INT32     ? uint8_t(4) :
           impl_type == BL_OBJECT_TYPE_ARRAY_UINT32    ? uint8_t(4) :
           impl_type == BL_OBJECT_TYPE_ARRAY_INT64     ? uint8_t(8) :
           impl_type == BL_OBJECT_TYPE_ARRAY_UINT64    ? uint8_t(8) :
           impl_type == BL_OBJECT_TYPE_ARRAY_FLOAT32   ? uint8_t(4) :
           impl_type == BL_OBJECT_TYPE_ARRAY_FLOAT64   ? uint8_t(8) :
           impl_type == BL_OBJECT_TYPE_ARRAY_STRUCT_1  ? uint8_t(1) :
           impl_type == BL_OBJECT_TYPE_ARRAY_STRUCT_2  ? uint8_t(2) :
           impl_type == BL_OBJECT_TYPE_ARRAY_STRUCT_3  ? uint8_t(3) :
           impl_type == BL_OBJECT_TYPE_ARRAY_STRUCT_4  ? uint8_t(4) :
           impl_type == BL_OBJECT_TYPE_ARRAY_STRUCT_6  ? uint8_t(6) :
           impl_type == BL_OBJECT_TYPE_ARRAY_STRUCT_8  ? uint8_t(8) :
           impl_type == BL_OBJECT_TYPE_ARRAY_STRUCT_10 ? uint8_t(10) :
           impl_type == BL_OBJECT_TYPE_ARRAY_STRUCT_12 ? uint8_t(12) :
           impl_type == BL_OBJECT_TYPE_ARRAY_STRUCT_16 ? uint8_t(16) :
           impl_type == BL_OBJECT_TYPE_ARRAY_STRUCT_20 ? uint8_t(20) :
           impl_type == BL_OBJECT_TYPE_ARRAY_STRUCT_24 ? uint8_t(24) :
           impl_type == BL_OBJECT_TYPE_ARRAY_STRUCT_32 ? uint8_t(32) :
           impl_type == BL_OBJECT_TYPE_ARRAY_OBJECT    ? uint8_t(sizeof(BLObjectCore)) : uint8_t(0);
  }
};

struct ArrayMaxSizeGenerator {
  static consteval size_t value(size_t object_type) noexcept {
    return ArrayItemSizeGenerator::value(object_type) == 0
      ? size_t(0)
      : (ObjectLayer::kMaxImplSize - sizeof(BLArrayImpl)) / ArrayItemSizeGenerator::value(object_type);
  }
};

struct ArraySSOCapacityGenerator {
  static consteval uint8_t value(size_t object_type) noexcept {
    uint8_t object_size = ArrayItemSizeGenerator::value(object_type);
    return object_size == 0 ? uint8_t(0) : uint8_t(BLObjectDetail::kSSOCapacity / object_size);
  }
};

static constexpr auto array_max_size_table = make_lookup_table<size_t, BL_OBJECT_TYPE_MAX_ARRAY + 1, ArrayMaxSizeGenerator>();
static constexpr auto array_item_size_table = make_lookup_table<uint8_t, BL_OBJECT_TYPE_MAX_ARRAY + 1, ArrayItemSizeGenerator>();
static constexpr auto array_sso_capacity_table = make_lookup_table<uint8_t, BL_OBJECT_TYPE_MAX_ARRAY + 1, ArraySSOCapacityGenerator>();

template<typename T>
inline constexpr size_t array_sso_capacity_of_type = BLObjectDetail::kSSOCapacity / sizeof(T);

// Array - Internal - Commons
// ==========================

// Only used as a filler
template<size_t N>
struct UInt32xN { uint32_t data[N]; };

static BL_INLINE_CONSTEXPR bool is_array_type(BLObjectType type) noexcept {
  return axl::is_between(type, BL_OBJECT_TYPE_MIN_ARRAY, BL_OBJECT_TYPE_MAX_ARRAY);
}

static BL_INLINE_CONSTEXPR bool is_array_of_objects(BLObjectType type) noexcept { return type == BL_OBJECT_TYPE_ARRAY_OBJECT; }
static BL_INLINE_CONSTEXPR size_t array_max_size_from_array_type(BLObjectType type) noexcept { return array_max_size_table[type]; }
static BL_INLINE_CONSTEXPR size_t array_item_size_from_array_type(BLObjectType type) noexcept { return array_item_size_table[type]; }
static BL_INLINE_CONSTEXPR size_t array_sso_capacity_from_array_type(BLObjectType type) noexcept { return array_sso_capacity_table[type]; }

static BL_INLINE_CONSTEXPR size_t array_capacity_from_impl_size(ObjectImplSize impl_size, size_t item_size) noexcept {
  return (size_t(impl_size) - ObjectLayer::data_offset_of<BLArrayCore>) / item_size;
}

static BL_INLINE_CONSTEXPR ObjectImplSize array_impl_size_from_capacity(size_t capacity, size_t item_size) noexcept {
  return ObjectImplSize(axl::align_up(ObjectLayer::data_offset_of<BLArrayCore> + capacity * item_size, ObjectLayer::kMinImplAlignment));
}

static BL_INLINE size_t array_get_capacity(const BLArrayCore* self) noexcept {
  size_t sso_capacity = array_sso_capacity_from_array_type(self->_d.type());
  return ObjectLayer::get_capacity_of_array(self, sso_capacity);
}

// Array - Internal - Low-Level Operations
// =======================================

static BL_NOINLINE BLResult array_fill_objects(void* dst, const void* src, size_t item_count, size_t item_size) noexcept {
  BL_ASSUME((item_size % sizeof(BLObjectCore)) == 0);

  if (item_count == 0u) {
    return BL_SUCCESS;
  }

  BLObjectCore* dst_ptr = static_cast<BLObjectCore*>(dst);
  const BLObjectCore* src_ptr = static_cast<const BLObjectCore*>(src);

  // Weak assigning a single object into an array of objects - if it's reference counted, increase the ref_count only once.
  for (size_t item_offset = 0; item_offset < item_size; item_offset += sizeof(BLObjectCore)) {
    const BLObjectCore* src_obj = axl::offset_ptr(src_ptr, item_offset);
    ObjectLayer::retain_object(src_obj, item_count);
  }

  for (size_t i = 0; i < item_count; i++) {
    for (size_t item_offset = 0; item_offset < item_size; item_offset += sizeof(BLObjectCore)) {
      BLObjectCore* dst_obj = axl::offset_ptr(dst_ptr, item_offset);
      const BLObjectCore* src_obj = axl::offset_ptr(src_ptr, item_offset);

      dst_obj->_d = src_obj->_d;
    }
    dst_ptr = axl::offset_ptr(dst_ptr, item_size);
  }

  return BL_SUCCESS;
}

static BL_NOINLINE BLResult array_fill_items(void* dst, const void* fill, size_t item_count, size_t item_size) noexcept {
  BL_ASSERT(item_size > 0u);

  if (item_count > 0u) {
    if (item_size == 1) {
      memset(dst, static_cast<const uint8_t*>(fill)[0], item_count);
    }
    else {
      axl::copy_bytes_small(dst, fill, item_size);
      axl::copy_forward(axl::offset_ptr<uint8_t>(dst, item_size), static_cast<uint8_t*>(dst), (item_count - 1u) * item_size);
    }
  }
  return BL_SUCCESS;
}

static BL_INLINE bool array_equals_content(const void* a, const void* b, size_t n_bytes, BLObjectType array_type) noexcept {
  if (is_array_of_objects(array_type)) {
    const BLObjectCore* a_obj = static_cast<const BLObjectCore*>(a);
    const BLObjectCore* b_obj = static_cast<const BLObjectCore*>(b);
    const BLObjectCore* a_end = axl::offset_ptr(a_obj, n_bytes);

    while (a_obj != a_end) {
      if (!bl_var_equals(a_obj, b_obj))
        return false;

      a_obj++;
      b_obj++;
    }

    return true;
  }
  else {
    return memcmp(a, b, n_bytes) == 0;
  }
}

// Array - Internal - Shared Utilities
// ===================================

BLResult array_prepend_possibly_aliased_data_inplace(uint8_t* dst, size_t dst_size, const uint8_t* src, size_t src_size) noexcept {
  // Move memory in-place making space for {src, src_size}.
  size_t shift_size = src_size;
  memmove(dst + shift_size, dst, dst_size);

  // Split the source data into LEADING and TRAILING spans and shift TRAILING span in a way to cancel the `memmove()`
  // operation that already happened in case the given source aliases with the destination. Only TRAILING span needs
  // adjustment.
  uint8_t* dst_end = dst + dst_size;

  // LEADING span - precedes `dst`, left as is as there is no adjustment.
  if (src < dst) {
    size_t leading_size = axl::min((size_t)(dst - src), src_size);
    memcpy(dst, src, leading_size);

    dst += leading_size;
    src += leading_size;

    src_size -= leading_size;
    if (src_size == 0) {
      return BL_SUCCESS;
    }
  }

  // TRAILING span - if src data aliases string data it has to be adjusted.
  if (src < dst_end) {
    src += shift_size;
  }

  memcpy(dst, src, src_size);
  return BL_SUCCESS;
}

BLResult array_replace_possibly_aliased_data_inplace(uint8_t* dst, size_t dst_size, const uint8_t* src, size_t src_size, size_t replace_size) noexcept {
  BL_ASSERT(dst_size >= replace_size);
  size_t tail_size = dst_size - replace_size;

  if (replace_size >= src_size) {
    memmove(dst, src, src_size);
    if (replace_size != src_size) {
      memmove(dst + src_size, dst + replace_size, tail_size);
    }
    return BL_SUCCESS;
  }
  else {
    // [dddDDDDDD..] - destination data.
    // [---        ] - data to be replaced.
    // [sssss      ] - data to be inserted.
    //
    // [     DDDDDD] - destination data, shifted right.
    // [sssssDDDDDD] - destination with source data.
    uint8_t* dst_end = dst + dst_size;
    size_t shift_size = src_size - replace_size;

    // Move memory in-place making space for {src, src_size}.
    // [ddd  DDDDDD] - destination data, shifted right.
    memmove(dst + src_size, dst + replace_size, dst_size - replace_size);

    // Split the source data into LEADING and TRAILING spans and shift TRAILING span in a way to cancel the `memmove()`
    // operation that already happened in case the given source aliases with the destination. Only TRAILING span needs
    // adjustment.

    // LEADING span - precedes `dst`, left as is as there is no adjustment.
    uint8_t* dst_adjusted = dst + src_size;
    if (src < dst_adjusted) {
      size_t leading_size = axl::min((size_t)(dst_adjusted - src), src_size);
      memmove(dst, src, leading_size);

      dst += leading_size;
      src += leading_size;

      src_size -= leading_size;
      if (src_size == 0) {
        return BL_SUCCESS;
      }
    }

    // TRAILING span - if src data aliases string data, which was moved, it has to be adjusted.
    if (src < dst_end) {
      src += shift_size;
    }

    memmove(dst, src, src_size);
    return BL_SUCCESS;
  }
}

// Array - Internal - Alloc & Free Impl
// ====================================

static BL_INLINE BLResult array_init_dynamic(BLArrayCore* self, BLObjectType array_type, ObjectImplSize impl_size, size_t size, size_t capacity) noexcept {
  return ObjectLayer::alloc_container_impl(self, impl_size, {
    .object_type = array_type,
    .data_offset = ObjectLayer::data_offset_of<BLArrayCore>,
    .size        = size,
    .capacity    = capacity
  });
}

static BL_INLINE BLResult array_init_dynamic_with_data(BLArrayCore* self, BLObjectType array_type, ObjectImplSize impl_size, size_t size, BLSpan<const uint8_t> data) noexcept {
  size_t item_size = array_item_size_from_array_type(array_type);
  return ObjectLayer::alloc_container_impl_with_data(self, impl_size, {
    .object_type    = array_type,
    .data_offset    = ObjectLayer::data_offset_of<BLArrayCore>,
    .size           = size,
    .capacity       = array_capacity_from_impl_size(impl_size, item_size),
    .src_data       = data.data(),
    .src_data_size  = data.size()
  });
}

static BL_INLINE BLResult array_init_external(
  BLArrayCore* self,
  BLObjectType array_type,
  BLDataAccessMode access_mode,
  void* external_data, size_t size, size_t capacity,
  BLDestroyExternalDataFunc destroy_func, void* user_data
) noexcept {
  ObjectImplSize impl_size = ObjectImplSize(sizeof(BLArrayImpl));
  return ObjectLayer::alloc_external_impl(self, impl_size, {
    .object_type   = array_type,
    .access_mode   = access_mode,
    .external_data = external_data,
    .size          = size,
    .capacity      = capacity,
    .destroy_func  = destroy_func,
    .user_data     = user_data
  });
}

static BL_NOINLINE BLResult array_init(BLArrayCore* self, BLObjectType array_type, size_t size, size_t capacity, uint8_t** out) noexcept {
  size_t sso_capacity = array_sso_capacity_table[array_type];
  if (capacity <= sso_capacity) {
    *out = ObjectLayer::sso_data<uint8_t>(self);
    return ObjectLayer::init_sso_with_size(self, array_type, size);
  }
  else {
    size_t item_size = array_item_size_from_array_type(array_type);
    BL_PROPAGATE(array_init_dynamic(self, array_type, array_impl_size_from_capacity(capacity, item_size), size, capacity));

    *out = ObjectLayer::dyn_data<uint8_t>(self);
    return BL_SUCCESS;
  }
}

static BL_INLINE BLResult array_realloc_dynamic(BLArrayCore* self, BLObjectType array_type, ObjectImplSize impl_size, size_t size, BLSpan<const uint8_t> data) noexcept {
  BLArrayCore tmp;
  BL_PROPAGATE(array_init_dynamic_with_data(&tmp, array_type, impl_size, size, data));

  if (is_array_of_objects(array_type)) {
    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self)) {
      ObjectLayer::set_dyn_size(self, 0u);
    }
    else {
      ObjectLayer::retain_object_array(ObjectLayer::dyn_data<BLObjectCore>(&tmp), size);
    }
  }

  return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kDyn>(self, &tmp);
}

BLResult array_free_object(BLArrayCore* self, BLArrayImpl* impl) noexcept {
  void* array_data = ObjectLayer::dyn_data<void>(self);

  BLObjectType array_type = self->_d.type();

  if (is_array_of_objects(array_type)) {
    size_t item_size = array_item_size_from_array_type(array_type);
    ObjectLayer::release_object_array(static_cast<BLObjectCore*>(array_data), (ObjectLayer::dyn_size(self) * item_size) / sizeof(BLObjectCore));
  }

  ObjectLayer::ImplHeader* impl_header = ObjectLayer::get_impl_header(impl);
  if (impl_header->is_external()) {
    ObjectLayer::call_external_destroy_func(impl, array_data);
  }

  return ObjectLayer::dealloc_impl(impl);
}

// Array - Internal - Typed Operations
// ===================================

template<typename T>
static BL_INLINE BLResult array_append_value_t(BLArrayCore* self, T value) noexcept {
  BL_ASSERT(self->_d.is_array());
  BL_ASSERT(array_item_size_from_array_type(self->_d.type()) == sizeof(T));

  constexpr size_t sso_capacity = array_sso_capacity_of_type<T>;

  if (self->_d.is_sso()) {
    ObjectLayer::DataSpan<T> content = ObjectLayer::object_sso_span<T>(self);
    if (content.size < sso_capacity) {
      ObjectLayer::set_sso_size(self, content.size + 1u);
      content.data[content.size] = value;
      return BL_SUCCESS;
    }
  }
  else {
    ObjectLayer::DataSpan<T> content = ObjectLayer::object_dyn_span<T>(self);
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, 1, content.size)) {
      ObjectLayer::set_dyn_size(self, content.size + 1u);
      content.data[content.size] = value;
      return BL_SUCCESS;
    }
  }

  return bl_array_append_item(self, &value);
}

template<typename T>
static BL_INLINE BLResult array_insert_value_t(BLArrayCore* self, size_t index, T value) noexcept {
  BL_ASSERT(self->_d.is_array());
  BL_ASSERT(array_item_size_from_array_type(self->_d.type()) == sizeof(T));

  T* dst;
  BL_PROPAGATE(bl_array_insert_uninitialized(self, index, 1, (void**)&dst));

  *dst = value;
  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE BLResult array_replace_value_t(BLArrayCore* self, size_t index, T value) noexcept {
  BL_ASSERT(self->_d.is_array());
  BL_ASSERT(array_item_size_from_array_type(self->_d.type()) == sizeof(T));

  if (self->_d.is_sso()) {
    ObjectLayer::DataSpan<T> content = ObjectLayer::object_sso_span<T>(self);
    if (BL_UNLIKELY(index >= content.size)) {
      return bl_make_error(BL_ERROR_INVALID_VALUE);
    }

    content.data[index] = value;
    return BL_SUCCESS;
  }
  else {
    ObjectLayer::DataSpan<T> content = ObjectLayer::object_dyn_span<T>(self);
    if (BL_UNLIKELY(axl::bool_or(!ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self), index >= content.size))) {
      return bl_array_replace_item(self, index, &value);
    }

    content.data[index] = value;
    return BL_SUCCESS;
  }
}

} // {bl}

// Array - API - Init & Destroy
// ============================

BL_API_IMPL BLResult bl_array_init(BLArrayCore* self, BLObjectType array_type) noexcept {
  using namespace bl;

  BLResult result = BL_SUCCESS;
  if (BL_UNLIKELY(!is_array_type(array_type))) {
    array_type = BL_OBJECT_TYPE_NULL;
    result = bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  self->_d = bl_object_defaults[array_type]._d;
  return result;
}

BL_API_IMPL BLResult bl_array_init_move(BLArrayCore* self, BLArrayCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.is_array());

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_array_init_weak(BLArrayCore* self, const BLArrayCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.is_array());

  return ObjectLayer::copy_object(self, other);
}

BL_API_IMPL BLResult bl_array_destroy(BLArrayCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  return ObjectLayer::release_object<ObjectLayer::ObjectMode::kAny>(self);
}

// Array - API - Reset
// ===================

BL_API_IMPL BLResult bl_array_reset(BLArrayCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  return ObjectLayer::replace_object(self, static_cast<const BLArrayCore*>(&bl_object_defaults[array_type]));
}

// Array - API - Accessors
// =======================

BL_API_IMPL size_t bl_array_get_size(const BLArrayCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  return ObjectLayer::get_size(self);
}

BL_API_IMPL size_t bl_array_get_capacity(const BLArrayCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t sso_capacity = array_sso_capacity_from_array_type(array_type);

  return ObjectLayer::get_capacity_of_array(self, sso_capacity);
}

BL_API_IMPL size_t bl_array_get_item_size(BLArrayCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  return array_item_size_from_array_type(self->_d.type());
}

BL_API_IMPL const void* bl_array_get_data(const BLArrayCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  return ObjectLayer::get_data<void>(self);
}

// Array - API - Data Manipulation
// ===============================

BL_API_IMPL BLResult bl_array_clear(BLArrayCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();

  if (self->_d.is_ref_counted()) {
    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kRC>(self)) {
      if (is_array_of_objects(array_type)) {
        size_t item_size = array_item_size_from_array_type(array_type);
        ObjectLayer::release_object_array(ObjectLayer::dyn_data<BLObjectCore>(self), (ObjectLayer::dyn_size(self) * item_size) / sizeof(BLObjectCore));
      }
      ObjectLayer::set_dyn_size(self, 0u);
      return BL_SUCCESS;
    }
    ObjectLayer::release_object<ObjectLayer::ObjectMode::kRC>(self);
  }

  self->_d = bl_object_defaults[array_type]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_array_shrink(BLArrayCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  // Don't shrink SSO, external, or non-ref counted arrays.
  if (!self->_d.is_ref_counted() || ObjectLayer::get_impl_header(self->_d.impl())->is_external()) {
    return BL_SUCCESS;
  }

  BLObjectType array_type = self->_d.type();
  ObjectLayer::DataSpan<uint8_t> content = ObjectLayer::object_dyn_span<uint8_t>(self);

  size_t item_size = array_item_size_from_array_type(array_type);
  size_t sso_capacity = array_sso_capacity_from_array_type(array_type);

  BLArrayCore tmp;
  if (content.size <= sso_capacity) {
    ObjectLayer::init_sso_with_data(&tmp, array_type, content.size, content.data, content.size * item_size);
    return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kRC>(self, &tmp);
  }
  else {
    size_t capacity = ObjectLayer::dyn_capacity(self);
    ObjectImplSize fitting_impl_size = array_impl_size_from_capacity(content.size, item_size);
    ObjectImplSize current_impl_size = array_impl_size_from_capacity(capacity, item_size);

    if (fitting_impl_size == current_impl_size) {
      return BL_SUCCESS;
    }

    return array_realloc_dynamic(self, array_type, fitting_impl_size, content.size, {content.data, content.size * item_size});
  }
}

BL_API_IMPL BLResult bl_array_resize(BLArrayCore* self, size_t new_size, const void* fill) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t item_size = array_item_size_from_array_type(array_type);
  size_t sso_capacity = array_sso_capacity_from_array_type(array_type);

  ObjectLayer::DataSpan<uint8_t> content;
  size_t fill_count;
  size_t byte_index;

  if (self->_d.is_sso()) {
    content = ObjectLayer::object_sso_span<uint8_t>(self);
    if (new_size <= content.size) {
      return bl_array_truncate(self, new_size);
    }

    fill_count = new_size - content.size;
    byte_index = content.size * item_size;

    if (new_size <= sso_capacity) {
      ObjectLayer::set_sso_size(self, new_size);
      return array_fill_items(content.data + byte_index, static_cast<const uint8_t*>(fill), fill_count, item_size);
    }
  }
  else {
    content = ObjectLayer::object_dyn_span<uint8_t>(self);
    if (new_size <= content.size) {
      return bl_array_truncate(self, new_size);
    }

    fill_count = new_size - content.size;
    byte_index = content.size * item_size;

    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, new_size)) {
      ObjectLayer::set_dyn_size(self, new_size);
      if (is_array_of_objects(array_type)) {
        return array_fill_objects(content.data + byte_index, fill, fill_count, item_size);
      }
      else {
        return array_fill_items(content.data + byte_index, fill, fill_count, item_size);
      }
    }
  }

  if (BL_UNLIKELY(new_size > array_max_size_table[array_type])) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  BLArrayCore tmp;
  ObjectImplSize impl_size = array_impl_size_from_capacity(new_size, item_size);
  BL_PROPAGATE(array_init_dynamic_with_data(&tmp, array_type, impl_size, new_size, {content.data, content.size * item_size}));

  if (is_array_of_objects(array_type)) {
    BLObjectCore* dst = ObjectLayer::dyn_data<BLObjectCore>(&tmp);
    ObjectLayer::retain_object_array(dst, (content.size * item_size) / sizeof(BLObjectCore));
    BL_PROPAGATE(array_fill_objects(axl::offset_ptr(dst, byte_index), fill, fill_count, item_size));
  }
  else {
    uint8_t* dst = ObjectLayer::dyn_data<uint8_t>(&tmp);
    BL_PROPAGATE(array_fill_items(dst + byte_index, static_cast<const uint8_t*>(fill), fill_count, item_size));
  }

  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_array_truncate(BLArrayCore* self, size_t truncated_size) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t item_size = array_item_size_from_array_type(array_type);

  if (self->_d.is_sso()) {
    // SSO: Make sure that the truncated content is zeroed to follow SSO requirements.
    ObjectLayer::DataSpan<uint8_t> content = ObjectLayer::object_sso_span<uint8_t>(self);
    if (truncated_size >= content.size) {
      return BL_SUCCESS;
    }

    ObjectLayer::set_sso_size(self, truncated_size);

    size_t truncate_index = truncated_size * item_size;
    size_t truncate_byte_size = (content.size * item_size) - truncate_index;

    memset(ObjectLayer::sso_data<uint8_t>(self) + truncate_index, 0, truncate_byte_size);
    return BL_SUCCESS;
  }
  else {
    ObjectLayer::DataSpan<uint8_t> content = ObjectLayer::object_dyn_span<uint8_t>(self);
    if (truncated_size >= content.size) {
      return BL_SUCCESS;
    }

    if (is_array_of_objects(array_type)) {
      // If the array has BLObjectCore content, we cannot just simply truncate the size. The objects need to be released first.
      if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self)) {
        size_t truncate_index = truncated_size * item_size;
        size_t truncate_byte_size = (content.size * item_size) - truncate_index;
        ObjectLayer::release_object_array(axl::offset_ptr<BLObjectCore>(content.data, truncate_index), truncate_byte_size / sizeof(BLObjectCore));
        ObjectLayer::set_dyn_size(self, truncated_size);
        return BL_SUCCESS;
      }
      else {
        BLArrayCore tmp;
        ObjectImplSize impl_size = array_impl_size_from_capacity(truncated_size, item_size);
        BL_PROPAGATE(array_init_dynamic_with_data(&tmp, array_type, impl_size, truncated_size, {content.data, truncated_size * item_size}));

        ObjectLayer::retain_object_array(ObjectLayer::dyn_data<BLObjectCore>(&tmp), truncated_size);
        return ObjectLayer::replace_object(self, &tmp);
      }
    }
    else {
      // Just set the array size. Since the size is stored in BLArrayCore we don't have to touch anything in the dynamically
      // allocated Impl, thus we don't even have to make it data unique in this case.
      ObjectLayer::set_dyn_size(self, truncated_size);
      return BL_SUCCESS;
    }
  }
}

BL_API_IMPL BLResult bl_array_reserve(BLArrayCore* self, size_t n) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t sso_capacity = array_sso_capacity_table[array_type];

  ObjectLayer::DataSpan<uint8_t> content;
  if (self->_d.is_sso()) {
    content = ObjectLayer::object_sso_span<uint8_t>(self);
    if (n <= sso_capacity) {
      return BL_SUCCESS;
    }
  }
  else {
    content = ObjectLayer::object_dyn_span<uint8_t>(self);
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, n)) {
      return BL_SUCCESS;
    }
  }

  size_t item_size = array_item_size_from_array_type(array_type);
  n = axl::max(n, content.size);

  if (BL_UNLIKELY(n > array_max_size_table[array_type])) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  if (n <= sso_capacity) {
    // Must be dynamic - immutable when the capacity was checked.
    BL_ASSERT(self->_d.is_dynamic());

    BLArrayCore tmp;
    ObjectLayer::init_sso_with_data(&tmp, array_type, content.size, content.data, content.size * item_size);
    return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kDyn>(self, &tmp);
  }
  else {
    ObjectImplSize impl_size = array_impl_size_from_capacity(n, item_size);
    return array_realloc_dynamic(self, array_type, impl_size, content.size, {content.data, content.size * item_size});
  }
}

BL_API_IMPL BLResult bl_array_reserve_for_append(BLArrayCore* self, size_t n) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t sso_capacity = array_sso_capacity_table[array_type];

  ObjectLayer::DataSpan<uint8_t> content;
  if (self->_d.is_sso()) {
    content = ObjectLayer::object_sso_span<uint8_t>(self);
    if (n <= sso_capacity - content.size) {
      return BL_SUCCESS;
    }
  }
  else {
    content = ObjectLayer::object_dyn_span<uint8_t>(self);
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, n, content.size)) {
      return BL_SUCCESS;
    }
  }

  size_t item_size = array_item_size_from_array_type(array_type);
  size_t max_allowed = array_max_size_table[array_type] - content.size;
  size_t min_capacity = content.size + n;

  if (BL_UNLIKELY(n >= max_allowed)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  ObjectImplSize impl_size = ObjectLayer::expand_impl_size(array_impl_size_from_capacity(min_capacity, item_size));
  return array_realloc_dynamic(self, array_type, impl_size, content.size, {content.data, content.size * item_size});
}

BL_API_IMPL BLResult bl_array_make_mutable(BLArrayCore* self, void** data_out) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  if (self->_d.is_sso()) {
    *data_out = ObjectLayer::sso_data<uint8_t>(self);
    return BL_SUCCESS;
  }

  if (ObjectLayer::is_object_mutable(self)) {
    *data_out = ObjectLayer::dyn_data<uint8_t>(self);
    return BL_SUCCESS;
  }

  // No need to optimize the slow case - let reserve handle the preparation for mutation.
  BL_PROPAGATE(bl_array_reserve(self, ObjectLayer::dyn_size(self)));

  *data_out = ObjectLayer::get_data<uint8_t>(self);
  return BL_SUCCESS;
}

// Array - API - Assign
// ====================

BL_API_IMPL BLResult bl_array_assign_move(BLArrayCore* self, BLArrayCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(self->_d.is_array());
  BL_ASSERT(self->_d.type() == other->_d.type());

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_array_assign_weak(BLArrayCore* self, const BLArrayCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(self->_d.is_array());
  BL_ASSERT(self->_d.type() == other->_d.type());

  return ObjectLayer::assign_object_weak(self, other);
}

BL_API_IMPL BLResult bl_array_assign_deep(BLArrayCore* self, const BLArrayCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(self->_d.is_array());
  BL_ASSERT(self->_d.type() == other->_d.type());

  return bl_array_assign_data(self, ObjectLayer::get_data<void>(other), ObjectLayer::get_size(other));
}

BL_API_IMPL BLResult bl_array_assign_data(BLArrayCore* self, const void* items, size_t n) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t item_size = array_item_size_from_array_type(array_type);

  ObjectLayer::DataSpan<uint8_t> content;
  size_t copy_size;

  if (self->_d.is_sso()) {
    size_t sso_capacity = array_sso_capacity_from_array_type(array_type);

    content = ObjectLayer::object_sso_span<uint8_t>(self);
    copy_size = n * item_size;

    if (n <= sso_capacity) {
      // ALIASING: Use memmove() to handle cases in which `items` aliases `content.data`.
      ObjectLayer::set_sso_size(self, n);

      memmove(content.data, items, copy_size);
      memset(content.data + copy_size, 0, BLObjectDetail::kSSOCapacity - copy_size);

      return BL_SUCCESS;
    }
  }
  else {
    content = ObjectLayer::object_dyn_span<uint8_t>(self);
    copy_size = n * item_size;

    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, n)) {
      ObjectLayer::set_dyn_size(self, n);

      if (is_array_of_objects(array_type)) {
        ObjectLayer::retain_object_array(static_cast<const BLObjectCore*>(items), copy_size / sizeof(BLObjectCore));
        ObjectLayer::release_object_array(reinterpret_cast<BLObjectCore*>(content.data), (content.size * item_size) / sizeof(BLObjectCore));
      }

      // ALIASING: Use memmove() to handle cases in which `items` aliases `content.data`.
      memmove(content.data, items, copy_size);
      return BL_SUCCESS;
    }
  }

  if (BL_UNLIKELY(n > array_max_size_table[array_type])) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  BLArrayCore tmp;
  ObjectImplSize impl_size = array_impl_size_from_capacity(n, item_size);
  BL_PROPAGATE(array_init_dynamic(&tmp, array_type, impl_size, n, array_capacity_from_impl_size(impl_size, item_size)));

  if (is_array_of_objects(array_type)) {
    ObjectLayer::copy_object_array(ObjectLayer::dyn_data<BLObjectCore>(&tmp), static_cast<const BLObjectCore*>(items), copy_size / sizeof(BLObjectCore));
  }
  else {
    memcpy(ObjectLayer::dyn_data<void>(&tmp), items, copy_size);
  }

  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_array_assign_external_data(BLArrayCore* self, void* external_data, size_t size, size_t capacity, BLDataAccessMode access_mode, BLDestroyExternalDataFunc destroy_func, void* user_data) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t item_size = array_item_size_from_array_type(array_type);

  axl::OverflowFlag of{};
  (void)axl::mul_overflow(capacity, item_size, of);

  if (BL_UNLIKELY(!capacity || capacity < size || axl::did_overflow(of))) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  BLArrayCore tmp;
  ObjectImplSize impl_size = ObjectImplSize(sizeof(BLArrayImpl));
  BL_PROPAGATE(ObjectLayer::alloc_external_impl(&tmp, impl_size, ObjectLayer::AllocExternalImplParams{
    .object_type   = array_type,
    .access_mode   = access_mode,
    .external_data = external_data,
    .size          = size,
    .capacity      = capacity,
    .destroy_func  = destroy_func,
    .user_data     = user_data
  }));

  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_array_assign_uninitialized(BLArrayCore* self, size_t n, void** data_out) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t item_size = array_item_size_from_array_type(array_type);

  if (self->_d.is_sso()) {
    size_t sso_capacity = array_sso_capacity_from_array_type(array_type);
    if (n <= sso_capacity) {
      void* dst_ptr = ObjectLayer::sso_data<void>(self);

      ObjectLayer::set_sso_size(self, n);
      *data_out = dst_ptr;

      memset(dst_ptr, 0, BLObjectDetail::kSSOCapacity);
      return BL_SUCCESS;
    }
  }
  else {
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, n)) {
      void* dst_ptr = ObjectLayer::dyn_data<void>(self);
      size_t old_size = ObjectLayer::dyn_size(self);

      ObjectLayer::set_dyn_size(self, n);
      *data_out = dst_ptr;

      if (is_array_of_objects(array_type)) {
        ObjectLayer::release_object_array(static_cast<BLObjectCore*>(dst_ptr), (old_size * item_size) / sizeof(BLObjectCore));
      }
      return BL_SUCCESS;
    }
  }

  BLArrayCore tmp;
  ObjectImplSize impl_size = array_impl_size_from_capacity(n, item_size);
  BL_PROPAGATE(array_init_dynamic(&tmp, array_type, impl_size, n, array_capacity_from_impl_size(impl_size, item_size)));

  *data_out = ObjectLayer::dyn_data<void>(&tmp);
  return ObjectLayer::replace_object(self, &tmp);
}

// Array - API - Append
// ====================

BL_API_IMPL BLResult bl_array_append_u8(BLArrayCore* self, uint8_t value) noexcept { return bl::array_append_value_t<uint8_t>(self, value); }
BL_API_IMPL BLResult bl_array_append_u16(BLArrayCore* self, uint16_t value) noexcept { return bl::array_append_value_t<uint16_t>(self, value); }
BL_API_IMPL BLResult bl_array_append_u32(BLArrayCore* self, uint32_t value) noexcept { return bl::array_append_value_t<uint32_t>(self, value); }
BL_API_IMPL BLResult bl_array_append_u64(BLArrayCore* self, uint64_t value) noexcept { return bl::array_append_value_t<uint64_t>(self, value); }
BL_API_IMPL BLResult bl_array_append_f32(BLArrayCore* self, float value) noexcept { return bl::array_append_value_t<float>(self, value); }
BL_API_IMPL BLResult bl_array_append_f64(BLArrayCore* self, double value) noexcept { return bl::array_append_value_t<double>(self, value); }

BL_API_IMPL BLResult bl_array_append_item(BLArrayCore* self, const void* item) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t item_size = array_item_size_from_array_type(array_type);

  ObjectLayer::DataSpan<uint8_t> content;
  size_t content_byte_size;

  if (self->_d.is_sso()) {
    content = ObjectLayer::object_sso_span<uint8_t>(self);
    content_byte_size = content.size * item_size;

    size_t sso_capacity = array_sso_capacity_from_array_type(array_type);
    if (content.size < sso_capacity) {
      ObjectLayer::set_sso_size(self, content.size + 1u);
      axl::copy_bytes_small(content.data + content_byte_size, item, item_size);
      return BL_SUCCESS;
    }
  }
  else {
    content = ObjectLayer::object_dyn_span<uint8_t>(self);
    content_byte_size = content.size * item_size;

    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, 1, content.size)) {
      ObjectLayer::set_dyn_size(self, content.size + 1u);
      axl::copy_bytes_small(content.data + content_byte_size, item, item_size);

      if (is_array_of_objects(array_type)) {
        ObjectLayer::retain_object_array(axl::offset_ptr<BLObjectCore>(content.data, content_byte_size), item_size / sizeof(BLObjectCore));
      }

      return BL_SUCCESS;
    }
  }

  BLArrayCore tmp;
  ObjectImplSize impl_size = array_impl_size_from_capacity(content.size + 1u, item_size);

  if (BL_UNLIKELY(size_t(impl_size) > ObjectLayer::kMaxImplSize)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  impl_size = ObjectLayer::expand_impl_size_if(impl_size, content.size != 0u);
  BL_PROPAGATE(array_init_dynamic_with_data(&tmp, array_type, impl_size, content.size + 1u, {content.data, content_byte_size}));

  void* dst_ptr = ObjectLayer::dyn_data<void>(&tmp);
  axl::copy_bytes_small(axl::offset_ptr(dst_ptr, content_byte_size), item, item_size);

  if (is_array_of_objects(array_type)) {
    size_t retain_byte_index = 0u;
    size_t retain_byte_size = content_byte_size + item_size;

    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self)) {
      retain_byte_index = content_byte_size;
      retain_byte_size = item_size;

      // Adjusting the size of `self` is done to prevent releasing the content when the object itself is released.
      ObjectLayer::set_dyn_size(self, 0);
    }

    ObjectLayer::retain_object_array(axl::offset_ptr<BLObjectCore>(dst_ptr, retain_byte_index), retain_byte_size / sizeof(BLObjectCore));
  }

  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_array_append_data(BLArrayCore* self, const void* items, size_t n) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t item_size = array_item_size_from_array_type(array_type);
  size_t items_byte_size = n * item_size;

  ObjectLayer::DataSpan<uint8_t> content;
  size_t content_byte_size;

  if (self->_d.is_sso()) {
    content = ObjectLayer::object_sso_span<uint8_t>(self);
    content_byte_size = content.size * item_size;

    size_t sso_capacity = array_sso_capacity_from_array_type(array_type);
    if (n <= sso_capacity - content.size) {
      ObjectLayer::set_sso_size(self, content.size + n);
      memcpy(content.data + content_byte_size, items, items_byte_size);
      return BL_SUCCESS;
    }
  }
  else {
    content = ObjectLayer::object_dyn_span<uint8_t>(self);
    content_byte_size = content.size * item_size;

    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, n, content.size)) {
      ObjectLayer::set_dyn_size(self, content.size + n);
      memcpy(content.data + content_byte_size, items, items_byte_size);

      if (is_array_of_objects(array_type)) {
        ObjectLayer::retain_object_array(axl::offset_ptr<BLObjectCore>(content.data, content_byte_size), items_byte_size / sizeof(BLObjectCore));
      }

      return BL_SUCCESS;
    }
  }

  size_t max_array_size = array_max_size_from_array_type(array_type);
  if (BL_UNLIKELY(n > max_array_size - content.size)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  BLArrayCore tmp;
  ObjectImplSize impl_size = array_impl_size_from_capacity(content.size + n, item_size);

  impl_size = ObjectLayer::expand_impl_size_if(impl_size, content.size != 0u);
  BL_PROPAGATE(array_init_dynamic_with_data(&tmp, array_type, impl_size, content.size + n, {content.data, content_byte_size}));

  void* dst_ptr = ObjectLayer::dyn_data<void>(&tmp);
  memcpy(axl::offset_ptr(dst_ptr, content_byte_size), items, items_byte_size);

  if (is_array_of_objects(array_type)) {
    size_t retain_byte_index = 0u;
    size_t retain_byte_size = content_byte_size + items_byte_size;

    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self)) {
      retain_byte_index = content_byte_size;
      retain_byte_size = items_byte_size;

      // Adjusting the size of `self` is done to prevent releasing the content when the object itself is released.
      ObjectLayer::set_dyn_size(self, 0);
    }

    ObjectLayer::retain_object_array(axl::offset_ptr<BLObjectCore>(dst_ptr, retain_byte_index), retain_byte_size / sizeof(BLObjectCore));
  }

  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_array_append_uninitialized(BLArrayCore* self, size_t n, void** data_out) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t item_size = array_item_size_from_array_type(array_type);

  ObjectLayer::DataSpan<uint8_t> content;
  size_t content_byte_size;

  if (self->_d.is_sso()) {
    content = ObjectLayer::object_sso_span<uint8_t>(self);
    content_byte_size = content.size * item_size;

    size_t sso_capacity = array_sso_capacity_from_array_type(array_type);
    if (n <= sso_capacity - content.size) {
      ObjectLayer::set_sso_size(self, content.size + n);
      *data_out = content.data + content_byte_size;
      return BL_SUCCESS;
    }
  }
  else {
    content = ObjectLayer::object_dyn_span<uint8_t>(self);
    content_byte_size = content.size * item_size;

    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, n, content.size)) {
      ObjectLayer::set_dyn_size(self, content.size + n);
      *data_out = content.data + content_byte_size;
      return BL_SUCCESS;
    }
  }

  size_t max_array_size = array_max_size_from_array_type(array_type);
  if (BL_UNLIKELY(n > max_array_size - content.size)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  BLArrayCore tmp;
  ObjectImplSize impl_size = array_impl_size_from_capacity(content.size + n, item_size);

  impl_size = ObjectLayer::expand_impl_size_if(impl_size, content.size != 0u);
  BL_PROPAGATE(array_init_dynamic_with_data(&tmp, array_type, impl_size, content.size + n, {content.data, content_byte_size}));

  void* dst_ptr = ObjectLayer::dyn_data<void>(&tmp);
  *data_out = axl::offset_ptr(dst_ptr, content_byte_size);

  if (is_array_of_objects(array_type)) {
    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self)) {
      // Adjusting the size of `self` is done to prevent releasing the content when the object itself is released.
      ObjectLayer::set_dyn_size(self, 0);
    }
    else {
      ObjectLayer::retain_object_array(static_cast<BLObjectCore*>(dst_ptr), content_byte_size / sizeof(BLObjectCore));
    }
  }

  return ObjectLayer::replace_object(self, &tmp);
}

// Array - API - Insert
// ====================

BL_API_IMPL BLResult bl_array_insert_u8(BLArrayCore* self, size_t index, uint8_t value) noexcept { return bl::array_insert_value_t<uint8_t>(self, index, value); }
BL_API_IMPL BLResult bl_array_insert_u16(BLArrayCore* self, size_t index, uint16_t value) noexcept { return bl::array_insert_value_t<uint16_t>(self, index, value); }
BL_API_IMPL BLResult bl_array_insert_u32(BLArrayCore* self, size_t index, uint32_t value) noexcept { return bl::array_insert_value_t<uint32_t>(self, index, value); }
BL_API_IMPL BLResult bl_array_insert_u64(BLArrayCore* self, size_t index, uint64_t value) noexcept { return bl::array_insert_value_t<uint64_t>(self, index, value); }
BL_API_IMPL BLResult bl_array_insert_f32(BLArrayCore* self, size_t index, float value) noexcept { return bl::array_insert_value_t<float>(self, index, value); }
BL_API_IMPL BLResult bl_array_insert_f64(BLArrayCore* self, size_t index, double value) noexcept { return bl::array_insert_value_t<double>(self, index, value); }
BL_API_IMPL BLResult bl_array_insert_item(BLArrayCore* self, size_t index, const void* item) noexcept { return bl_array_insert_data(self, index, item, 1); }

BL_API_IMPL BLResult bl_array_insert_data(BLArrayCore* self, size_t index, const void* items, size_t n) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t item_size = array_item_size_from_array_type(array_type);
  size_t items_byte_size = n * item_size;
  size_t insert_byte_index = index * item_size;

  ObjectLayer::DataSpan<uint8_t> content;
  size_t content_byte_size;

  if (self->_d.is_sso()) {
    content = ObjectLayer::object_sso_span<uint8_t>(self);
    content_byte_size = content.size * item_size;

    size_t sso_capacity = array_sso_capacity_from_array_type(array_type);
    if (n <= sso_capacity - content.size) {
      uint8_t* insert_ptr = content.data + insert_byte_index;
      ObjectLayer::set_sso_size(self, content.size + n);

      return array_prepend_possibly_aliased_data_inplace(
        insert_ptr, content_byte_size - insert_byte_index,
        static_cast<const uint8_t*>(items), items_byte_size);
    }
  }
  else {
    content = ObjectLayer::object_dyn_span<uint8_t>(self);
    content_byte_size = content.size * item_size;

    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, n, content.size)) {
      uint8_t* insert_ptr = content.data + insert_byte_index;
      ObjectLayer::set_dyn_size(self, content.size + n);

      if (is_array_of_objects(array_type)) {
        ObjectLayer::retain_object_array(static_cast<const BLObjectCore*>(items), items_byte_size / sizeof(BLObjectCore));
      }

      return array_prepend_possibly_aliased_data_inplace(
        insert_ptr, content_byte_size - insert_byte_index,
        static_cast<const uint8_t*>(items), items_byte_size);
    }
  }

  size_t max_array_size = array_max_size_from_array_type(array_type);
  if (BL_UNLIKELY(n > max_array_size - content.size)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  BLArrayCore tmp;
  ObjectImplSize impl_size = array_impl_size_from_capacity(content.size + n, item_size);

  impl_size = ObjectLayer::expand_impl_size_if(impl_size, content.size != 0u);
  BL_PROPAGATE(array_init_dynamic_with_data(&tmp, array_type, impl_size, content.size + n, {content.data, insert_byte_index}));

  uint8_t* insert_ptr = ObjectLayer::dyn_data<uint8_t>(&tmp) + insert_byte_index;
  memcpy(insert_ptr, items, items_byte_size);
  memcpy(insert_ptr + items_byte_size, content.data + insert_byte_index, content_byte_size - insert_byte_index);

  if (is_array_of_objects(array_type)) {
    size_t retain_byte_index = 0u;
    size_t retain_byte_size = content_byte_size + items_byte_size;

    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self)) {
      retain_byte_index = insert_byte_index;
      retain_byte_size = items_byte_size;

      // Adjusting the size of `self` is done to prevent releasing the content when the object itself is released.
      ObjectLayer::set_dyn_size(self, 0);
    }

    ObjectLayer::retain_object_array(
      axl::offset_ptr<BLObjectCore>(ObjectLayer::dyn_data<uint8_t>(&tmp), retain_byte_index), retain_byte_size / sizeof(BLObjectCore));
  }

  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_array_insert_uninitialized(BLArrayCore* self, size_t index, size_t n, void** data_out) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t item_size = array_item_size_from_array_type(array_type);
  size_t items_byte_size = n * item_size;
  size_t insert_byte_index = index * item_size;

  ObjectLayer::DataSpan<uint8_t> content;
  size_t content_byte_size;

  if (self->_d.is_sso()) {
    content = ObjectLayer::object_sso_span<uint8_t>(self);
    content_byte_size = content.size * item_size;

    size_t sso_capacity = array_sso_capacity_from_array_type(array_type);
    if (n <= sso_capacity - content.size) {
      uint8_t* insert_ptr = content.data + insert_byte_index;
      ObjectLayer::set_sso_size(self, content.size + n);

      *data_out = insert_ptr;
      memmove(insert_ptr + items_byte_size, insert_ptr, content_byte_size - insert_byte_index);
      return BL_SUCCESS;
    }
  }
  else {
    content = ObjectLayer::object_dyn_span<uint8_t>(self);
    content_byte_size = content.size * item_size;

    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, n, content.size)) {
      uint8_t* insert_ptr = content.data + insert_byte_index;
      ObjectLayer::set_dyn_size(self, content.size + n);

      *data_out = insert_ptr;
      memmove(insert_ptr + items_byte_size, insert_ptr, content_byte_size - insert_byte_index);
      return BL_SUCCESS;
    }
  }

  size_t max_array_size = array_max_size_from_array_type(array_type);
  if (BL_UNLIKELY(n > max_array_size - content.size)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  BLArrayCore tmp;
  ObjectImplSize impl_size = array_impl_size_from_capacity(content.size + n, item_size);

  impl_size = ObjectLayer::expand_impl_size(impl_size);
  BL_PROPAGATE(array_init_dynamic_with_data(&tmp, array_type, impl_size, content.size + n, {content.data, insert_byte_index}));

  uint8_t* insert_ptr = ObjectLayer::dyn_data<uint8_t>(&tmp) + insert_byte_index;
  *data_out = insert_ptr;

  memcpy(insert_ptr + items_byte_size, content.data + insert_byte_index, content_byte_size - insert_byte_index);

  if (is_array_of_objects(array_type)) {
    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self)) {
      // Adjusting the size of `self` is done to prevent releasing the content when the object itself is released.
      ObjectLayer::set_dyn_size(self, 0);
    }
    else {
      ObjectLayer::retain_object_array(reinterpret_cast<BLObjectCore*>(content.data), content_byte_size / sizeof(BLObjectCore));
    }
  }

  return ObjectLayer::replace_object(self, &tmp);
}

// Array - API - Replace
// =====================

BL_API_IMPL BLResult bl_array_replace_u8(BLArrayCore* self, size_t index, uint8_t value) noexcept { return bl::array_replace_value_t<uint8_t>(self, index, value); }
BL_API_IMPL BLResult bl_array_replace_u16(BLArrayCore* self, size_t index, uint16_t value) noexcept { return bl::array_replace_value_t<uint16_t>(self, index, value); }
BL_API_IMPL BLResult bl_array_replace_u32(BLArrayCore* self, size_t index, uint32_t value) noexcept { return bl::array_replace_value_t<uint32_t>(self, index, value); }
BL_API_IMPL BLResult bl_array_replace_u64(BLArrayCore* self, size_t index, uint64_t value) noexcept { return bl::array_replace_value_t<uint64_t>(self, index, value); }
BL_API_IMPL BLResult bl_array_replace_f32(BLArrayCore* self, size_t index, float value) noexcept { return bl::array_replace_value_t<float>(self, index, value); }
BL_API_IMPL BLResult bl_array_replace_f64(BLArrayCore* self, size_t index, double value) noexcept { return bl::array_replace_value_t<double>(self, index, value); }

BL_API_IMPL BLResult bl_array_replace_item(BLArrayCore* self, size_t index, const void* item) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t item_size = array_item_size_from_array_type(array_type);
  size_t byte_index = index * item_size;

  ObjectLayer::DataSpan<uint8_t> content;
  if (self->_d.is_sso()) {
    content = ObjectLayer::object_sso_span<uint8_t>(self);
    if (index >= content.size) {
      return bl_make_error(BL_ERROR_INVALID_VALUE);
    }

    memcpy(content.data + byte_index, item, item_size);
    return BL_SUCCESS;
  }
  else {
    content = ObjectLayer::object_dyn_span<uint8_t>(self);
    if (index >= content.size) {
      return bl_make_error(BL_ERROR_INVALID_VALUE);
    }

    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self)) {
      if (is_array_of_objects(array_type)) {
        ObjectLayer::assign_object_array(reinterpret_cast<BLObjectCore*>(content.data + byte_index),
                                         reinterpret_cast<const BLObjectCore*>(item),
                                         item_size / sizeof(BLObjectCore));
      }
      else {
        memcpy(content.data + byte_index, item, item_size);
      }

      return BL_SUCCESS;
    }
    else {
      BLArrayCore tmp;
      ObjectImplSize impl_size = array_impl_size_from_capacity(content.size, item_size);

      impl_size = ObjectLayer::expand_impl_size(impl_size);
      BL_PROPAGATE(array_init_dynamic_with_data(&tmp, array_type, impl_size, content.size, {content.data, content.size * item_size}));

      if (is_array_of_objects(array_type)) {
        BLObjectCore* dst_ptr = ObjectLayer::dyn_data<BLObjectCore>(&tmp);
        const BLObjectCore* src_ptr = reinterpret_cast<const BLObjectCore*>(item);

        ObjectLayer::retain_object_array(dst_ptr, byte_index / sizeof(BLObjectCore));
        ObjectLayer::copy_object_array(axl::offset_ptr(dst_ptr, byte_index), src_ptr, item_size / sizeof(BLObjectCore));
        ObjectLayer::retain_object_array(axl::offset_ptr(dst_ptr, byte_index + item_size), content.size - index - 1u);
      }
      else {
        uint8_t* dst_ptr = ObjectLayer::dyn_data<uint8_t>(&tmp);
        memcpy(dst_ptr + byte_index, item, item_size);
      }

      return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kDyn>(self, &tmp);
    }
  }
}

BL_API_IMPL BLResult bl_array_replace_data(BLArrayCore* self, size_t r_start, size_t r_end, const void* items, size_t n) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t item_size = array_item_size_from_array_type(array_type);

  size_t range_size;

  uint8_t* array_data;
  size_t array_size;
  size_t array_size_without_range;

  BLArrayCore tmp;
  uint8_t* tmp_data;

  if (self->_d.is_sso()) {
    array_data = ObjectLayer::sso_data<uint8_t>(self);
    array_size = ObjectLayer::sso_size(self);

    r_end = axl::min(r_end, array_size);
    r_start = axl::min(r_start, r_end);

    range_size = r_end - r_start;
    array_size_without_range = array_size - range_size;

    if (r_start == array_size) {
      return bl_array_append_data(self, items, n);
    }

    size_t capacity = array_sso_capacity_from_array_type(array_type);
    if (n > capacity - array_size_without_range) {
      // The code that implements SSO->Dynamic and Dynamic->Dynamic is identical.
      goto MakeDynamic;
    }

    // In-place replace within SSO boundary.
    //
    // ALIASING: Avoided as the data is processed in a temporary buffer.
    size_t result_size = array_size_without_range + n;
    ObjectLayer::init_sso_with_size(&tmp, array_type, result_size);

    tmp_data = ObjectLayer::sso_data<uint8_t>(&tmp);
  }
  else {
    array_data = ObjectLayer::dyn_data<uint8_t>(self);
    array_size = ObjectLayer::dyn_size(self);

    r_end = axl::min(r_end, array_size);
    r_start = axl::min(r_start, r_end);

    range_size = r_end - r_start;
    array_size_without_range = array_size - range_size;

    if (r_start == array_size) {
      return bl_array_append_data(self, items, n);
    }

    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, n, array_size - range_size)) {
      // In-place mutation.
      //
      // ALIASING: We have to account for `items` aliasing `array_data` - even the data replaced.
      if (is_array_of_objects(array_type)) {
        // Retain first
        ObjectLayer::retain_object_array(
          static_cast<const BLObjectCore*>(items), (n * item_size) / sizeof(BLObjectCore));

        ObjectLayer::release_object_array(
          reinterpret_cast<BLObjectCore*>(array_data + r_start * item_size), (range_size * item_size) / sizeof(BLObjectCore));
      }

      size_t result_size = array_size_without_range + n;
      ObjectLayer::set_dyn_size(self, result_size);

      return array_replace_possibly_aliased_data_inplace(
        array_data + r_start * item_size, (array_size - r_start) * item_size,
        static_cast<const uint8_t*>(items), n * item_size, range_size * item_size);
    }

MakeDynamic:
    size_t max_size = array_max_size_from_array_type(array_type);
    size_t result_size = array_size_without_range + n;

    if (BL_UNLIKELY(n > max_size - array_size_without_range)) {
      return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
    }

    ObjectImplSize impl_size = ObjectLayer::expand_impl_size(array_impl_size_from_capacity(result_size, item_size));
    BL_PROPAGATE(array_init_dynamic(&tmp, array_type, impl_size, result_size, array_capacity_from_impl_size(impl_size, item_size)));

    tmp_data = ObjectLayer::dyn_data<uint8_t>(&tmp);
  }

  // Replacement to a new undelying array for any combination [SSO->SSO, SSO->Dynamic, Dynamic->SSO, Dynamic->Dynamic].
  memcpy(tmp_data, array_data, r_start * item_size);
  tmp_data += r_start * item_size;

  memcpy(tmp_data, items, n * item_size);
  tmp_data += n * item_size;

  memcpy(tmp_data, array_data + r_end * item_size, (array_size - r_end) * item_size);

  if (is_array_of_objects(array_type)) {
    // Never SSO -> Use dyn_data() to avoid SSO vs DYN checks.
    ObjectLayer::retain_object_array(ObjectLayer::dyn_data<BLObjectCore>(&tmp), array_size_without_range + n);
  }

  return ObjectLayer::replace_object(self, &tmp);
}

// Array - API - Remove
// ====================

BL_API_IMPL BLResult bl_array_remove_index(BLArrayCore* self, size_t index) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  return bl_array_remove_range(self, index, index + 1u);
}

BL_API_IMPL BLResult bl_array_remove_range(BLArrayCore* self, size_t start, size_t end) noexcept {
  using namespace bl;
  BL_ASSERT(self->_d.is_array());

  BLObjectType array_type = self->_d.type();
  size_t item_size = array_item_size_from_array_type(array_type);

  if (self->_d.is_sso()) {
    ObjectLayer::DataSpan<uint8_t> content = ObjectLayer::object_sso_span<uint8_t>(self);
    end = axl::min(end, content.size);
    start = axl::min(start, end);

    size_t n = end - start;
    if (!n) {
      return BL_SUCCESS;
    }

    end *= item_size;
    start *= item_size;

    size_t result_size = content.size - n;
    size_t content_byte_size = content.size * item_size;

    n *= item_size;

    ObjectLayer::set_sso_size(self, result_size);
    axl::copy_forward(content.data + start, content.data + end, content_byte_size - end);
    axl::fill_bytes_small(content.data + result_size * item_size, n, uint8_t(0));
    return BL_SUCCESS;
  }
  else {
    ObjectLayer::DataSpan<uint8_t> content = ObjectLayer::object_dyn_span<uint8_t>(self);
    end = axl::min(end, content.size);
    start = axl::min(start, end);

    size_t n = end - start;
    if (!n) {
      return BL_SUCCESS;
    }

    end *= item_size;
    start *= item_size;

    size_t result_size = content.size - n;
    size_t content_byte_size = content.size * item_size;

    n *= item_size;

    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self)) {
      if (is_array_of_objects(array_type)) {
        ObjectLayer::release_object_array(axl::offset_ptr<BLObjectCore>(content.data, start), n / sizeof(BLObjectCore));
      }

      ObjectLayer::set_dyn_size(self, result_size);
      memmove(content.data + start, content.data + end, content_byte_size - end);
      return BL_SUCCESS;
    }

    BLArrayCore tmp;
    uint8_t* dst_ptr;

    size_t sso_capacity = array_sso_capacity_from_array_type(array_type);
    if (result_size <= sso_capacity) {
      ObjectLayer::init_sso_with_data(&tmp, array_type, result_size, content.data, start);
      dst_ptr = ObjectLayer::sso_data<uint8_t>(&tmp);
    }
    else {
      ObjectImplSize impl_size = array_impl_size_from_capacity(result_size, item_size);
      BL_PROPAGATE(array_init_dynamic_with_data(&tmp, array_type, impl_size, result_size, {content.data, start}));
      dst_ptr = ObjectLayer::dyn_data<uint8_t>(&tmp);
    }

    memcpy(dst_ptr + start, content.data + end, content_byte_size - end);

    if (is_array_of_objects(array_type)) {
      ObjectLayer::retain_object_array(reinterpret_cast<BLObjectCore*>(dst_ptr), (result_size * item_size) / sizeof(BLObjectCore));
    }

    return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kDyn>(self, &tmp);
  }
}

// Array - API - Equality & Comparison
// ===================================

BL_API_IMPL bool bl_array_equals(const BLArrayCore* a, const BLArrayCore* b) noexcept {
  using namespace bl;

  BL_ASSERT(a->_d.is_array());
  BL_ASSERT(b->_d.is_array());

  if (a->_d == b->_d)
    return true;

  BLObjectType a_array_type = a->_d.type();
  BLObjectType b_array_type = b->_d.type();

  size_t a_size = ObjectLayer::get_size(a);
  size_t b_size = ObjectLayer::get_size(b);

  if (axl::bool_or(a_array_type != b_array_type, a_size != b_size)) {
    return false;
  }

  const void* a_data = ObjectLayer::get_data<void>(a);
  const void* b_data = ObjectLayer::get_data<void>(b);

  size_t item_size = array_item_size_from_array_type(a_array_type);
  return array_equals_content(a_data, b_data, a_size * item_size, a_array_type);
}

// Array - Runtime Registration
// ============================

void bl_array_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  using namespace bl;

  for (uint32_t object_type = BL_OBJECT_TYPE_MIN_ARRAY; object_type < BL_OBJECT_TYPE_ARRAY_STRUCT_32; object_type++) {
    ObjectLayer::init_sso(&bl_object_defaults[object_type], static_cast<BLObjectType>(object_type));
  }

  auto* null_impl = const_cast<BLArrayImpl*>(&null_array.impl);
  for (uint32_t object_type = BL_OBJECT_TYPE_ARRAY_STRUCT_32; object_type <= BL_OBJECT_TYPE_MAX_ARRAY; object_type++) {
    ObjectLayer::init_dyn(&bl_object_defaults[object_type], static_cast<BLObjectType>(object_type), BLObjectDetail::kMetaFlagD, null_impl, null_impl);
  }
}
