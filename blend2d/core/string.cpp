// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/commons.h>
#include <blend2d/axl/string_utils_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/string_p.h>
#include <blend2d/core/runtime_p.h>

#include <stdio.h>

namespace bl::StringDetail {

// String - Internal - Init
// ========================

static BL_INLINE BLResult init_dynamic(BLStringCore* self, ObjectImplSize impl_size, size_t size_uninitialized) noexcept {
  return ObjectLayer::alloc_container_impl(self, impl_size, {
    .object_type = BLString::Info::kType,
    .data_offset = ObjectLayer::data_offset_of<BLStringCore>,
    .size        = size_uninitialized,
    .capacity    = capacity_from_impl_size(impl_size)
  });
}

static BL_INLINE BLResult init_dynamic_with_data(BLStringCore* self, ObjectImplSize impl_size, size_t string_size, BLStringView data) noexcept {
  BL_PROPAGATE(ObjectLayer::alloc_container_impl_with_data(self, impl_size, {
    .object_type   = BLString::Info::kType,
    .data_offset   = ObjectLayer::data_offset_of<BLStringCore>,
    .size          = string_size,
    .capacity      = capacity_from_impl_size(impl_size),
    .src_data      = data.data(),
    .src_data_size = data.size()
  }));

  char* dst_ptr = ObjectLayer::dyn_data<char>(self);
  dst_ptr[string_size] = '\0';
  return BL_SUCCESS;
}

static BL_NOINLINE BLResult init_with_spans_and_result_size(BLStringCore* self, const BLStringView* spans, size_t count, size_t result_size) noexcept {
  char* dst_ptr;
  if (result_size <= BLString::Info::kSSOCapacity) {
    ObjectLayer::init_sso_with_size(self, result_size);
    dst_ptr = ObjectLayer::sso_data<char>(self);
  }
  else {
    if (BL_UNLIKELY(result_size > kMaxSize)) {
      ObjectLayer::init_sso(self);
      return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
    }

    BL_PROPAGATE(init_dynamic(self, impl_size_from_capacity(result_size), result_size));
    dst_ptr = ObjectLayer::dyn_data<char>(self);
  }

  size_t index = 0;
  for (size_t i = 0; i < count; i++) {
    size_t span_size = spans[i].size();
    memcpy(dst_ptr + index, spans[i].data(), spans[i].size());
    index += span_size;
  }
  dst_ptr[index] = '\0';

  BL_ASSERT(index == result_size);
  return BL_SUCCESS;
}

// String - Internal - Format
// ==========================

template<bool IsAppend>
static BLResult format_v(BLStringCore* self, const char* fmt, va_list ap) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  static constexpr size_t kBufferSize = 2048;

  char buf[kBufferSize];

  va_list ap_copy;
  va_copy(ap_copy, ap);

  // SECURITY: vsnprintf() expects null terminator to be included in the size of the buffer.
  int fmt_result = vsnprintf(buf, sizeof(buf), fmt, ap);
  if (unsigned(fmt_result) < kBufferSize) {
    va_end(ap_copy);

    if constexpr (IsAppend) {
      return bl_string_append_data(self, buf, unsigned(fmt_result));
    }
    else {
      return bl_string_assign_data(self, buf, unsigned(fmt_result));
    }
  }

  BLResult result;

  if (BL_UNLIKELY(fmt_result < 0)) {
    result = bl_make_error(BL_ERROR_INVALID_VALUE);
  }
  else {
    char* dst_ptr;
    size_t output_size = unsigned(fmt_result);

    if constexpr (IsAppend) {
      result = bl_string_append_uninitialized(self, output_size, &dst_ptr);
      if (BL_LIKELY(result == BL_SUCCESS)) {
        int fmt_result_2 = vsnprintf(dst_ptr, output_size + 1u, fmt, ap_copy);
        if (BL_UNLIKELY(fmt_result != fmt_result_2)) {
          // FAILED? Formatting may have overwritten our null terminator, so make sure it's there.
          ObjectLayer::get_data<char>(self)[ObjectLayer::get_size(self)] = '\0';
          result = bl_make_error(BL_ERROR_INVALID_VALUE);
        }
      }
    }
    else {
      // If vsnprintf() fails for whatever reason, it could write into the data supplied,
      // which means that we won't be able to honor transactional behavior if that happens.
      // So, instead of using `bl_string_assign_uninitialized()`, we create a new string
      // where we format, and then just replace the content in `self`.
      BLStringCore tmp;
      ObjectLayer::init_sso(&tmp);

      result = bl_string_assign_uninitialized(&tmp, output_size, &dst_ptr);
      if (BL_LIKELY(result == BL_SUCCESS)) {
        int fmt_result_2 = vsnprintf(dst_ptr, output_size + 1u, fmt, ap_copy);
        if (BL_UNLIKELY(fmt_result != fmt_result_2)) {
          // FAILED? Drop `tmp` and propagate...
          result = bl_make_error(BL_ERROR_INVALID_VALUE);
        }
        else if (self->_d.is_dynamic() && ObjectLayer::check_dynamic_capacity_and_mutability(self, output_size, 0u)) {
          bl_string_assign_data(self, ObjectLayer::get_data<char>(&tmp), output_size);
          ObjectLayer::release_object(&tmp);
        }
        else {
          ObjectLayer::replace_object(self, &tmp);
        }
      }
    }
  }

  va_end(ap_copy);
  return result;
}

} // {bl::StringDetail}

// String - API - Construction & Destruction
// =========================================

BL_API_IMPL BLResult bl_string_init(BLStringCore* self) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  return ObjectLayer::init_object(self);
}

BL_API_IMPL BLResult bl_string_init_move(BLStringCore* self, BLStringCore* other) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_string_init_weak(BLStringCore* self, const BLStringCore* other) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object(self, other);
}

BL_API_IMPL BLResult bl_string_init_with_data(BLStringCore* self, const char* data, size_t data_size) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  if (data_size <= BLString::Info::kSSOCapacity) {
    return ObjectLayer::init_sso_with_data(self, data_size, data, data_size);
  }

  if (BL_UNLIKELY(data_size > kMaxSize)) {
    ObjectLayer::init_sso(self);
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  return init_dynamic_with_data(self, impl_size_from_capacity(data_size), data_size, {data, data_size});
}

BL_API_IMPL BLResult bl_string_init_with_cstr(BLStringCore* self, const char* cstr) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  return bl_string_init_with_data(self, cstr, strlen(cstr));
}

BL_API_IMPL BLResult bl_string_init_with_spans(BLStringCore* self, const BLStringView* spans, size_t count) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  axl::OverflowFlag of{};
  size_t result_size = 0;
  for (size_t i = 0; i < count; i++) {
    result_size = axl::add_overflow(result_size, spans[i].size(), of);
  }

  result_size |= axl::bool_as_mask<size_t>(of);
  return init_with_spans_and_result_size(self, spans, count, result_size);
}

BL_API_IMPL BLResult bl_string_destroy(BLStringCore* self) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object(self);
}

// String - API - Common Functionality
// ===================================

BL_API_IMPL BLResult bl_string_reset(BLStringCore* self) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

// String - API - Accessors
// ========================

BL_API_IMPL const char* bl_string_get_data(const BLStringCore* self) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_data<char>(self);
}

BL_API_IMPL size_t bl_string_get_size(const BLStringCore* self) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_size(self);
}

BL_API_IMPL size_t bl_string_get_capacity(const BLStringCore* self) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_capacity(self);
}

// String - API - Clear
// ====================

BL_API_IMPL BLResult bl_string_clear(BLStringCore* self) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_ref_counted()) {
    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kRC>(self)) {
      ObjectLayer::dyn_data<char>(self)[0] = '\0';
      ObjectLayer::set_dyn_size(self, 0u);
      return BL_SUCCESS;
    }

    ObjectLayer::release_object<ObjectLayer::ObjectMode::kRC>(self);
  }

  return ObjectLayer::init_sso(self);
}

// String - API - Storage Size & Capacity
// ======================================

BL_API_IMPL BLResult bl_string_shrink(BLStringCore* self) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  // Don't shrink SSO or non-ref counted objects - these are possibly eternal.
  if (!self->_d.is_ref_counted()) {
    return BL_SUCCESS;
  }

  BLStringCore tmp;
  ObjectLayer::DataSpan<char> content = ObjectLayer::object_dyn_span<char>(self);

  if (content.size <= BLString::Info::kSSOCapacity) {
    ObjectLayer::init_sso_with_data(&tmp, content.size, content.data, content.size);
  }
  else {
    ObjectImplSize fitting_impl_size = impl_size_from_capacity(content.size);
    ObjectImplSize current_impl_size = impl_size_from_capacity(ObjectLayer::dyn_capacity(self));

    if (fitting_impl_size == current_impl_size) {
      return BL_SUCCESS;
    }
    BL_PROPAGATE(init_dynamic_with_data(&tmp, fitting_impl_size, content.size, {content.data, content.size}));
  }

  return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kRC>(self, &tmp);
}

BL_API_IMPL BLResult bl_string_reserve(BLStringCore* self, size_t capacity) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringCore tmp;
  ObjectLayer::DataSpan<char> content;

  if (self->_d.is_sso()) {
    content = ObjectLayer::object_sso_span<char>(self);
    if (ObjectLayer::check_sso_capacity(self, capacity)) {
      return BL_SUCCESS;
    }
  }
  else {
    content = ObjectLayer::object_dyn_span<char>(self);
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, capacity)) {
      return BL_SUCCESS;
    }
    capacity = axl::max(capacity, content.size);
  }

  if (BL_UNLIKELY(capacity >= kMaxSize)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  ObjectImplSize impl_size = impl_size_from_capacity(capacity);
  BL_PROPAGATE(init_dynamic_with_data(&tmp, impl_size, content.size, {content.data, content.size}));
  return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kDyn>(self, &tmp);
}

BL_API_IMPL BLResult bl_string_resize(BLStringCore* self, size_t new_size, char fill) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringCore tmp;
  ObjectLayer::DataSpan<char> content;

  if (self->_d.is_sso()) {
    content = ObjectLayer::object_sso_span<char>(self);
    if (ObjectLayer::check_sso_capacity(self, new_size)) {
      ObjectLayer::set_sso_size(self, new_size);

      // Fill either null terminators or `fill` depending on whether we truncate or append.
      //
      // NOTE: This is necessary as we have to maintain zeros after string data.
      if (new_size <= content.size) {
        fill = '\0';
      }

      size_t fill_index = axl::min(content.size, new_size);
      size_t fill_end = axl::max(content.size, new_size);

      memset(content.data + fill_index, uint8_t(fill), fill_end - fill_index);
      return BL_SUCCESS;
    }
  }
  else {
    content = ObjectLayer::object_dyn_span<char>(self);
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, new_size)) {
      ObjectLayer::set_dyn_size(self, new_size);

      content.data[new_size] = '\0';
      if (new_size > content.size) {
        memset(content.data + content.size, uint8_t(fill), new_size - content.size);
      }

      return BL_SUCCESS;
    }
  }

  size_t copy_size = axl::min(content.size, new_size);
  char* new_data;

  if (new_size <= BLString::Info::kSSOCapacity) {
    ObjectLayer::init_sso_with_data(&tmp, new_size, content.data, copy_size);
    new_data = ObjectLayer::sso_data<char>(&tmp);
  }
  else {
    if (BL_UNLIKELY(new_size > kMaxSize)) {
      return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
    }

    BL_PROPAGATE(init_dynamic_with_data(&tmp, impl_size_from_capacity(new_size), new_size, {content.data, copy_size}));
    new_data = ObjectLayer::dyn_data<char>(&tmp);
  }

  memset(new_data + copy_size, uint8_t(fill), new_size - copy_size);
  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_string_truncate(BLStringCore* self, size_t truncate_size) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  size_t result_size = axl::min(ObjectLayer::get_size(self), truncate_size);
  return bl_string_resize(self, result_size, char('\0'));
}

BL_API_IMPL BLResult bl_string_make_mutable(BLStringCore* self, char** data_out) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_sso()) {
    *data_out = ObjectLayer::sso_data<char>(self);
    return BL_SUCCESS;
  }
  else if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self)) {
    *data_out = ObjectLayer::dyn_data<char>(self);
    return BL_SUCCESS;
  }
  else {
    BLStringCore tmp;

    size_t size = ObjectLayer::dyn_size(self);
    ObjectImplSize impl_size = impl_size_from_capacity(size);
    BL_PROPAGATE(init_dynamic_with_data(&tmp, impl_size, size, {ObjectLayer::dyn_data<char>(self), size}));

    *data_out = ObjectLayer::dyn_data<char>(&tmp);
    return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kDyn>(self, &tmp);
  }
}

// String - API - Assign
// =====================

BL_API_IMPL BLResult bl_string_assign_move(BLStringCore* self, BLStringCore* other) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_string_assign_weak(BLStringCore* self, const BLStringCore* other) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

BL_API_IMPL BLResult bl_string_assign_char(BLStringCore* self, char c, size_t count) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  char* dst_ptr;
  BL_PROPAGATE(bl_string_assign_uninitialized(self, count, &dst_ptr));

  memset(dst_ptr, uint8_t(c), count);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_string_assign_data(BLStringCore* self, const char* data, size_t data_size) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringCore tmp;

  if (self->_d.is_sso()) {
    if (ObjectLayer::check_sso_capacity(self, data_size)) {
      // ALIASING: The given {data, data_size} span could overlap with the SSO storage.
      ObjectLayer::init_sso_with_data(&tmp, data_size, data, data_size);
      self->_d = tmp._d;
      return BL_SUCCESS;
    }
    else {
      // ALIASING: The given {data, data_size} span cannot alias SSO storage as it's larger than `sso_capacity`.
      BL_ASSERT(data_size > BLString::Info::kSSOCapacity);
      if (BL_UNLIKELY(data_size > kMaxSize)) {
        return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
      }
      return init_dynamic_with_data(self, impl_size_from_capacity(data_size), data_size, {data, data_size});
    }
  }
  else {
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, data_size)) {
      // ALIASING: Use memmove() in case the given data overlaps this string.
      ObjectLayer::set_dyn_size(self, data_size);
      char* dst_ptr = ObjectLayer::dyn_data<char>(self);
      memmove(dst_ptr, data, data_size);
      dst_ptr[data_size] = '\0';
      return BL_SUCCESS;
    }
    else {
      if (data_size < BLString::Info::kSSOCapacity) {
        ObjectLayer::init_sso_with_data(&tmp, data_size, data, data_size);
      }
      else {
        if (BL_UNLIKELY(data_size > kMaxSize)) {
          return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
        }
        BL_PROPAGATE(init_dynamic_with_data(&tmp, impl_size_from_capacity(data_size), data_size, {data, data_size}));
      }
      return ObjectLayer::replace_object<ObjectLayer::ObjectMode::kDyn>(self, &tmp);
    }
  }
}

BL_API_IMPL BLResult bl_string_assign_cstr(BLStringCore* self, const char* cstr) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return bl_string_assign_data(self, cstr, strlen(cstr));
}

BL_API_IMPL BLResult bl_string_assign_format(BLStringCore* self, const char* fmt, ...) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLResult result;
  va_list ap;

  va_start(ap, fmt);
  result = bl_string_assign_format_v(self, fmt, ap);
  va_end(ap);

  return result;
}

BL_API_IMPL BLResult bl_string_assign_format_v(BLStringCore* self, const char* fmt, va_list ap) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return format_v<false>(self, fmt, ap);
}

BL_API_IMPL BLResult bl_string_assign_uninitialized(BLStringCore* self, size_t data_size, char** data_out) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_sso()) {
    if (ObjectLayer::check_sso_capacity(self, data_size)) {
      ObjectLayer::init_sso_with_size(self, data_size);
      *data_out = ObjectLayer::sso_data<char>(self);
      return BL_SUCCESS;
    }
  }
  else {
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, data_size)) {
      char* dst_ptr = ObjectLayer::dyn_data<char>(self);
      ObjectLayer::set_dyn_size(self, data_size);

      *data_out = dst_ptr;
      dst_ptr[data_size] = '\0';
      return BL_SUCCESS;
    }
  }

  if (BL_UNLIKELY(data_size > kMaxSize)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  BLStringCore tmp;
  ObjectImplSize impl_size = impl_size_from_capacity(data_size);
  BL_PROPAGATE(init_dynamic(&tmp, impl_size, data_size));

  char* dst_ptr = ObjectLayer::dyn_data<char>(&tmp);
  *data_out = dst_ptr;
  dst_ptr[data_size] = '\0';
  return ObjectLayer::replace_object(self, &tmp);
}

// String - API - Append
// =====================

BL_API_IMPL BLResult bl_string_append_char(BLStringCore* self, char c, size_t count) noexcept {
  char* dst_ptr;
  BL_PROPAGATE(bl_string_append_uninitialized(self, count, &dst_ptr));

  memset(dst_ptr, uint8_t(c), count);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_string_append_data(BLStringCore* self, const char* data, size_t data_size) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLStringCore tmp;
  ObjectLayer::DataSpan<char> content;

  if (self->_d.is_sso()) {
    content = ObjectLayer::object_sso_span<char>(self);
    if (ObjectLayer::check_sso_capacity(self, data_size, content.size)) {
      // Append operation on a SSO string having a SSO result.
      //
      // ALIASING: Doesn't matter - copy operation never overlaps.
      size_t result_size = content.size + data_size;
      ObjectLayer::set_sso_size(self, result_size);

      // String SSO data is always zero padded, which guarantees null-termination after append.
      BL_ASSERT(content.data[result_size] == '\0');

      memcpy(content.data + content.size, data, data_size);
      return BL_SUCCESS;
    }
  }
  else {
    // ALIASING: Doesn't matter when there is enough capacity.
    content = ObjectLayer::object_dyn_span<char>(self);
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, data_size, content.size)) {
      size_t result_size = content.size + data_size;
      ObjectLayer::set_dyn_size(self, result_size);

      content.data[result_size] = '\0';
      memcpy(content.data + content.size, data, data_size);
      return BL_SUCCESS;
    }
  }

  size_t result_size = axl::add_saturate(content.size, data_size);
  if (BL_UNLIKELY(result_size > kMaxSize)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  ObjectImplSize impl_size = impl_size_from_capacity(result_size);

  impl_size = ObjectLayer::expand_impl_size_if(impl_size, content.size != 0u);
  BL_PROPAGATE(init_dynamic_with_data(&tmp, impl_size, result_size, {content.data, content.size}));

  char* new_data = ObjectLayer::dyn_data<char>(&tmp);
  memcpy(new_data + content.size, data, data_size);
  return ObjectLayer::replace_object(self, &tmp);
}

BL_API_IMPL BLResult bl_string_append_cstr(BLStringCore* self, const char* cstr) noexcept {
  return bl_string_append_data(self, cstr, strlen(cstr));
}

BL_API_IMPL BLResult bl_string_append_string(BLStringCore* self, const BLStringCore* other) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  ObjectLayer::DataSpan<const char> data = ObjectLayer::object_get_span<const char>(other);
  return bl_string_append_data(self, data.data, data.size);
}

BL_API_IMPL BLResult bl_string_append_format(BLStringCore* self, const char* fmt, ...) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLResult result;
  va_list ap;

  va_start(ap, fmt);
  result = bl_string_append_format_v(self, fmt, ap);
  va_end(ap);

  return result;
}

BL_API_IMPL BLResult bl_string_append_format_v(BLStringCore* self, const char* fmt, va_list ap) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return format_v<true>(self, fmt, ap);
}

BL_API_IMPL BLResult bl_string_append_uninitialized(BLStringCore* self, size_t data_size, char** data_out) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  ObjectLayer::DataSpan<char> content;
  if (self->_d.is_sso()) {
    content = ObjectLayer::object_sso_span<char>(self);
    if (ObjectLayer::check_sso_capacity(self, data_size, content.size)) {
      *data_out = content.data + content.size;
      size_t result_size = content.size + data_size;

      ObjectLayer::set_sso_size(self, result_size);
      return BL_SUCCESS;
    }
  }
  else {
    content = ObjectLayer::object_dyn_span<char>(self);
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, data_size, content.size)) {
      *data_out = content.data + content.size;
      size_t result_size = content.size + data_size;

      ObjectLayer::set_dyn_size(self, result_size);
      content.data[result_size] = '\0';
      return BL_SUCCESS;
    }
  }

  BLStringCore tmp;

  size_t result_size = axl::add_saturate(content.size, data_size);
  ObjectImplSize impl_size = impl_size_from_capacity(result_size);

  if (BL_UNLIKELY(result_size > kMaxSize)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  impl_size = ObjectLayer::expand_impl_size_if(impl_size, content.size != 0u);
  BL_PROPAGATE(init_dynamic_with_data(&tmp, impl_size, result_size, {content.data, content.size}));

  char* dst_ptr = ObjectLayer::dyn_data<char>(&tmp);
  *data_out = dst_ptr + content.size;
  dst_ptr[result_size] = '\0';
  return ObjectLayer::replace_object(self, &tmp);
}

// String - API - Insert
// =====================

BL_API_IMPL BLResult bl_string_insert_char(BLStringCore* self, size_t index, char c, size_t count) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  char* dst_ptr;
  BL_PROPAGATE(bl_string_insert_uninitialized(self, index, count, &dst_ptr));

  memset(dst_ptr, int((unsigned char)c), count);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_string_insert_data(BLStringCore* self, size_t index, const char* data, size_t data_size) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));

  ObjectLayer::DataSpan<char> content;
  size_t result_size;

  if (self->_d.is_sso()) {
    content = ObjectLayer::object_sso_span<char>(self);
    if (axl::bool_or(index >= content.size, data_size == 0u)) {
      return bl_string_append_data(self, data, data_size);
    }

    result_size = axl::add_saturate(content.size, data_size);
  }
  else {
    content = ObjectLayer::object_dyn_span<char>(self);
    if (axl::bool_or(index >= content.size, data_size == 0u)) {
      return bl_string_append_data(self, data, data_size);
    }

    result_size = axl::add_saturate(content.size, data_size);
    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, result_size)) {
      ObjectLayer::set_dyn_size(self, result_size);
      content.data[result_size] = '\0';

      return array_prepend_possibly_aliased_data_inplace(
        reinterpret_cast<uint8_t*>(content.data + index), content.size - index,
        reinterpret_cast<const uint8_t*>(data), data_size);
    }
  }

  {
    // ALIASING: The given {data, data_size} cannot alias a newly created string, so init_with_spans() is the right approach.
    BLStringCore tmp;
    BLStringView parts[3] = {
      {content.data, index},
      {data, data_size},
      {content.data + index, content.size - index}
    };

    BL_PROPAGATE(init_with_spans_and_result_size(&tmp, parts, 3u, result_size));
    return ObjectLayer::replace_object(self, &tmp);
  }
}

BL_API_IMPL BLResult bl_string_insert_cstr(BLStringCore* self, size_t index, const char* cstr) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return bl_string_insert_data(self, index, cstr, strlen(cstr));
}

BL_API_IMPL BLResult bl_string_insert_string(BLStringCore* self, size_t index, const BLStringCore* other) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  BLStringView span = other->dcast().span();
  return bl_string_insert_data(self, index, span.data(), span.size());
}

BL_API_IMPL BLResult bl_string_insert_uninitialized(BLStringCore* self, size_t index, size_t data_size, char** data_out) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  ObjectLayer::DataSpan<char> content;
  if (self->_d.is_sso()) {
    content = ObjectLayer::object_sso_span<char>(self);
    if (BL_UNLIKELY(index >= content.size)) {
      return bl_string_append_uninitialized(self, data_size, data_out);
    }

    if (ObjectLayer::check_sso_capacity(self, data_size, content.size)) {
      char* dst_ptr = content.data + index;
      *data_out = dst_ptr;

      ObjectLayer::set_sso_size(self, content.size + data_size);
      memmove(dst_ptr + data_size, dst_ptr, content.size - index);
      return BL_SUCCESS;
    }
  }
  else {
    content = ObjectLayer::object_dyn_span<char>(self);
    if (BL_UNLIKELY(index >= content.size)) {
      return bl_string_append_uninitialized(self, data_size, data_out);
    }

    if (ObjectLayer::check_dynamic_capacity_and_mutability(self, data_size, content.size)) {
      char* dst_ptr = content.data + index;
      *data_out = dst_ptr;

      ObjectLayer::set_dyn_size(self, content.size + data_size);
      memmove(dst_ptr + data_size, dst_ptr, content.size - index + 1u); // Include null terminator.
      return BL_SUCCESS;
    }
  }

  size_t result_size = axl::add_saturate(content.size, data_size);
  if (BL_UNLIKELY(result_size > kMaxSize)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  BLStringCore tmp;
  ObjectImplSize impl_size = impl_size_from_capacity(result_size);

  impl_size = ObjectLayer::expand_impl_size(impl_size);
  BL_PROPAGATE(init_dynamic_with_data(&tmp, impl_size, result_size, {content.data, index}));

  char* dst_ptr = ObjectLayer::dyn_data<char>(&tmp) + index;
  *data_out = dst_ptr;
  memcpy(dst_ptr + data_size, content.data + index, content.size - index + 1u); // Include null terminator.
  return ObjectLayer::replace_object(self, &tmp);
}

// String - API - Data Manipulation - Remove
// =========================================

BL_API_IMPL BLResult bl_string_remove_index(BLStringCore* self, size_t index) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return bl_string_remove_range(self, index, index + 1);
}

BL_API_IMPL BLResult bl_string_remove_range(BLStringCore* self, size_t start, size_t end) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  ObjectLayer::DataSpan<char> content = ObjectLayer::object_get_span<char>(self);
  size_t range_end = axl::min(end, content.size);
  size_t range_index = axl::min(start, range_end);
  size_t range_size = range_end - range_index;
  size_t result_size = content.size - range_size;

  if (!range_size) {
    return BL_SUCCESS;
  }

  if (self->_d.is_dynamic()) {
    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kDyn>(self)) {
      ObjectLayer::set_dyn_size(self, result_size);
      memmove(content.data + range_index, content.data + range_end, content.size - range_end + 1u);
      return BL_SUCCESS;
    }
  }

  BLStringCore tmp;
  BLStringView parts[] = {
    {content.data, range_index},
    {content.data + range_end, content.size - range_end}
  };
  BL_PROPAGATE(init_with_spans_and_result_size(&tmp, parts, 2u, result_size));
  return ObjectLayer::replace_object(self, &tmp);
}

// String - API - Equality / Comparison
// ====================================

BL_API_IMPL bool bl_string_equals(const BLStringCore* a, const BLStringCore* b) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  ObjectLayer::DataSpan<const char> a_content = ObjectLayer::object_get_span<const char>(a);
  ObjectLayer::DataSpan<const char> b_content = ObjectLayer::object_get_span<const char>(b);

  if (a_content.size != b_content.size) {
    return false;
  }

  return memcmp(a_content.data, b_content.data, a_content.size) == 0;
}

BL_API_IMPL bool bl_string_equals_data(const BLStringCore* self, const char* data, size_t data_size) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(data_size != SIZE_MAX);

  ObjectLayer::DataSpan<const char> content = ObjectLayer::object_get_span<const char>(self);
  return content.size == data_size && memcmp(content.data, data, data_size) == 0;
}

BL_API_IMPL bool bl_string_equals_cstr(const BLStringCore* self, const char* cstr) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  ObjectLayer::DataSpan<const char> content = ObjectLayer::object_get_span<const char>(self);

  size_t i;
  for (i = 0; i < content.size; i++) {
    if (axl::bool_or(content.data[i] != cstr[i], cstr[i] == '\0')) {
      return false;
    }
  }
  return cstr[i] == '\0';
}

BL_API_IMPL int bl_string_compare(const BLStringCore* a, const BLStringCore* b) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  ObjectLayer::DataSpan<const char> a_content = ObjectLayer::object_get_span<const char>(a);
  ObjectLayer::DataSpan<const char> b_content = ObjectLayer::object_get_span<const char>(b);

  size_t min_size = bl_min(a_content.size, b_content.size);
  int c = memcmp(a_content.data, b_content.data, min_size);

  if (c) {
    return c;
  }

  return a_content.size < b_content.size ? -1 : int(a_content.size > b_content.size);
}

BL_API_IMPL int bl_string_compare_data(const BLStringCore* self, const char* data, size_t data_size) noexcept {
  BL_USING_DETAIL(bl::StringDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(data_size != SIZE_MAX);

  ObjectLayer::DataSpan<const char> a_content = ObjectLayer::object_get_span<const char>(self);
  ObjectLayer::DataSpan<const char> b_content = {data, data_size};

  int size_cmp = a_content.size < b_content.size ? -1 : int(a_content.size > b_content.size);
  int result = memcmp(a_content.data, b_content.data, bl_min(a_content.size, b_content.size));

  return result ? result : size_cmp;
}

BL_API_IMPL int bl_string_compare_cstr(const BLStringCore* self, const char* cstr) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  ObjectLayer::DataSpan<const char> a_content = ObjectLayer::object_get_span<const char>(self);
  const char* b_content_cstr = cstr;

  // We don't know the size of `cstr`, thus we cannot use strcmp() as BLString itself can contain zeros within the
  // data.
  size_t i;
  for (i = 0; i < a_content.size; i++) {
    int a = uint8_t(a_content.data[i]);
    int b = uint8_t(b_content_cstr[i]);

    int c = a - b;

    // If we found a null terminator in 'b' it means that so far the strings were equal, but now we are at the end
    // of 'b', however, there is still some content in 'a'. This would mean that `a > b` like "abc?" > "abc".
    if (b == 0) {
      c = 1;
    }

    if (c) {
      return c;
    }
  }

  // We are at the end of 'a'. If this is also the end of 'b' then these strings are equal and we return zero. If
  // 'b' doesn't point to a null terminator then `a < b`.
  return -int(b_content_cstr[i] != 0);
}

// String - Runtime Registration
// =============================

void bl_string_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  BL_USING_DETAIL(bl::StringDetail);
  static constexpr BLObjectType object_type = BLString::Info::kType;

  ObjectLayer::init_sso(&bl_object_defaults[object_type], object_type);
}
