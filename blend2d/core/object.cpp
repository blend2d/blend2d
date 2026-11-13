// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_alloc.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/coverage_set_p.h>
#include <blend2d/core/font_p.h>
#include <blend2d/core/font_feature_settings_p.h>
#include <blend2d/core/font_manager_p.h>
#include <blend2d/core/font_variation_settings_p.h>
#include <blend2d/core/glyph_buffer_p.h>
#include <blend2d/core/gradient_p.h>
#include <blend2d/core/image_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/path_p.h>
#include <blend2d/core/pattern_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/core/string_p.h>
#include <blend2d/core/stroke_options_p.h>
#include <blend2d/core/var_p.h>

// Object - Globals
// ================

BLObjectCore bl_object_defaults[BL_OBJECT_TYPE_MAX_VALUE + 1];

void bl_object_destroy_external_func_dummy(
  [[maybe_unused]] void* impl_ptr,
  [[maybe_unused]] void* external_data,
  [[maybe_unused]] void* user_data) noexcept {}

// Object - Internal - Impl - Alloc & Free
// =======================================

namespace bl::ObjectLayer {

static BL_INLINE void* init_impl_after_allocation(void* allocated_ptr, size_t header_size, size_t impl_alignment, size_t rc_base, ImplFlags impl_flags) noexcept {
  void* impl_ptr = axl::align_up(axl::offset_ptr(allocated_ptr, header_size), impl_alignment);
  size_t allocation_offset = axl::ptr_byte_diff(allocated_ptr, impl_ptr);
  init_impl_header(impl_ptr, rc_base, static_cast<uint8_t>(allocation_offset), impl_flags);
  return impl_ptr;
}

#if BL_TARGET_ARCH_BITS >= 64
static BL_INLINE_NODEBUG uint64_t object_combine_type_meta_capacity_aliased_64(BLObjectType object_type, MetaByte meta_byte, size_t capacity) noexcept {
  if constexpr (std::endian::native == std::endian::little) {
    return (uint64_t(meta_byte.v) << 56) | (uint64_t(object_type) << 48) | uint64_t(capacity);
  }
  else {
    return (uint64_t(meta_byte.v)) | (uint64_t(object_type) << 8) | (uint64_t(capacity) << 16);
  }
}

static BL_INLINE BLResult init_dynamic_container_64(BLObjectCore* self, BLObjectType object_type, MetaByte meta_byte, void* impl_ptr, void* data, size_t size, size_t capacity) noexcept {
  self->_d.dyn.data_or_virt.ptr = data;
  self->_d.dyn.impl.ptr = impl_ptr;
  self->_d.dyn.size_u64 = size;
  self->_d.dyn.capacity_u48 = object_combine_type_meta_capacity_aliased_64(object_type, meta_byte, capacity);

  return BL_SUCCESS;
}
#endif

static BL_INLINE BLResult init_dynamic_container_32(BLObjectCore* self, BLObjectType object_type, MetaByte meta_byte, void* impl_ptr, void* data, size_t size, size_t capacity) noexcept {
  self->_d.dyn.data_or_virt.ptr = data;
  self->_d.dyn.impl.ptr = impl_ptr;
#if BL_TARGET_ARCH_BITS < 64
  self->_d.dyn.data_or_virt.padding = 0u;
  self->_d.dyn.impl.padding = 0u;
#endif

  self->_d.dyn.size_u32 = uint32_t(size);
  self->_d.dyn.capacity_u32 = uint32_t(capacity);
  self->_d.dyn.payload_u32 = uint32_t(0);
  self->_d.dyn.payload_u16 = uint16_t(0);
  self->_d.dyn.type_byte = uint8_t(object_type);
  self->_d.dyn.meta_byte = uint8_t(meta_byte.v);

  return BL_SUCCESS;
}

template<size_t SizeWidth>
static BL_INLINE BLResult init_dynamic_container(BLObjectCore* self, BLObjectType object_type, MetaByte meta_byte, void* impl_ptr, void* data, size_t size, size_t capacity) noexcept {
#if BL_TARGET_ARCH_BITS >= 64
  if constexpr (SizeWidth == 64) {
    return init_dynamic_container_64(self, object_type, meta_byte, impl_ptr, data, size, capacity);
  }
  else {
    return init_dynamic_container_32(self, object_type, meta_byte, impl_ptr, data, size, capacity);
  }
#else
  static_assert(SizeWidth == 32);
  return init_dynamic_container_32(self, object_type, meta_byte, impl_ptr, data, size, capacity);
#endif
}

static BL_INLINE BLResult init_virt(BLObjectCore* self, BLObjectType object_type, MetaByte meta_byte, void* impl_ptr, const BLObjectVirt* virt) noexcept {
  self->_d.dyn.data_or_virt.ptr = const_cast<void*>(static_cast<const void*>(virt)); // It stays const, just the variable is a type erased `void*`.
  self->_d.dyn.impl.ptr = impl_ptr;

#if BL_TARGET_ARCH_BITS >= 64
  self->_d.dyn.size_u64 = 0;
#else
  self->_d.dyn.size_u32 = uint32_t(0);
  self->_d.dyn.capacity_u32 = uint32_t(0);
#endif

  self->_d.dyn.payload_u32 = uint32_t(0);
  self->_d.dyn.payload_u16 = uint16_t(0);
  self->_d.dyn.type_byte = uint8_t(object_type);
  self->_d.dyn.meta_byte = uint8_t(meta_byte.v);

  return BL_SUCCESS;
}

static BL_INLINE BLResult alloc_impl_internal(BLObjectCore* self, BLObjectType object_type, ObjectImplSize impl_size, size_t impl_alignment) noexcept {
  constexpr size_t header_size = sizeof(ImplHeader);

  size_t over_alloc = (impl_alignment <= axl::kAllocAlignment) ? size_t(0) : impl_alignment;

  size_t invalid_mask = axl::bool_as_mask<size_t>(size_t(impl_size) > kMaxImplSize);
  size_t allocation_size = (size_t(impl_size) + header_size + over_alloc) | invalid_mask;

  void* allocated_ptr = axl::mem_alloc(allocation_size);
  if (BL_UNLIKELY(!allocated_ptr)) {
    self->_d = bl_object_defaults[size_t(object_type)]._d;
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  // If `kAllocAlignment` must be correct, otherwise this won't work.
  BL_ASSERT(axl::is_aligned(allocated_ptr, axl::kAllocAlignment));

  void* impl_ptr = init_impl_after_allocation(allocated_ptr, header_size, impl_alignment, kImplRCScale, ImplFlags::kNone);
  return init_dyn(self, object_type, BLObjectDetail::kMetaFlagDR, impl_ptr, nullptr);
}

BLResult alloc_impl(BLObjectCore* self, BLObjectType object_type, ObjectImplSize impl_size) noexcept {
  return alloc_impl_internal(self, object_type, impl_size, kMinImplAlignment);
}

BLResult alloc_impl_aligned(BLObjectCore* self, BLObjectType object_type, ObjectImplSize impl_size, size_t impl_alignment) noexcept {
  using namespace bl;

  if (!axl::is_power_of_2(impl_alignment)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  impl_alignment = bl_clamp<size_t>(impl_alignment, kMinImplAlignment, kMaxImplAlignment);
  return alloc_impl_internal(self, object_type, impl_size, impl_alignment);
}

template<size_t SizeWidth, typename Params>
static BL_INLINE BLResult alloc_container_impl_internal(BLObjectCore* self, ObjectImplSize impl_size, const Params& params) noexcept {
  constexpr size_t header_size = sizeof(ImplHeader);

  size_t invalid_mask = axl::bool_as_mask<size_t>(size_t(impl_size) > kMaxImplSize);
  size_t allocation_size = size_t(impl_size) + header_size + kMinImplAlignment;
  void* allocated_ptr = axl::mem_alloc(allocation_size | invalid_mask);

  BLObjectType object_type = params.object_type;
  if (BL_UNLIKELY(!allocated_ptr)) {
    self->_d = bl_object_defaults[size_t(object_type)]._d;
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  void* impl_ptr = init_impl_after_allocation(allocated_ptr, header_size, kMinImplAlignment, kImplRCScale, ImplFlags::kNone);
  void* data_ptr = axl::offset_ptr(impl_ptr, params.data_offset);

  if constexpr (Params::is_with_data) {
    memcpy(data_ptr, params.src_data, params.src_data_size);
  }

  return init_dynamic_container<SizeWidth>(self, object_type, MetaByte{BLObjectDetail::kMetaFlagDR}, impl_ptr, data_ptr, params.size, params.capacity);
}

BLResult alloc_container_impl_32(BLObjectCore* self, ObjectImplSize impl_size, const AllocContainerImplParams& params) noexcept {
  return alloc_container_impl_internal<32>(self, impl_size, params);
}

BLResult alloc_container_impl_with_data_32(BLObjectCore* self, ObjectImplSize impl_size, const AllocContainerImplWithDataParams& params) noexcept {
  return alloc_container_impl_internal<32>(self, impl_size, params);
}

#if BL_TARGET_ARCH_BITS >= 64
BLResult alloc_container_impl_64(BLObjectCore* self, ObjectImplSize impl_size, const AllocContainerImplParams& params) noexcept {
  return alloc_container_impl_internal<64>(self, impl_size, params);
}

BLResult alloc_container_impl_with_data_64(BLObjectCore* self, ObjectImplSize impl_size, const AllocContainerImplWithDataParams& params) noexcept {
  return alloc_container_impl_internal<64>(self, impl_size, params);
}
#endif

BLResult alloc_external_impl(BLObjectCore* self, ObjectImplSize impl_size, const AllocExternalImplParams& params) noexcept {
  // Cannot happen - external Impls have no data associated with them, thus they have a relatively small size.
  BL_ASSERT(size_t(impl_size) <= kMaxImplSize);

  constexpr size_t header_size = sizeof(ImplHeader) + sizeof(ExternalHeader);

  size_t allocation_size = size_t(impl_size) + header_size + kMinImplAlignment;
  void* allocated_ptr = axl::mem_alloc(allocation_size);

  BLObjectType object_type = params.object_type;
  if (BL_UNLIKELY(!allocated_ptr)) {
    self->_d = bl_object_defaults[size_t(object_type)]._d;
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  size_t immutable_bit = size_t(params.access_mode == BL_DATA_ACCESS_MODE_RO);
  void* impl_ptr = init_impl_after_allocation(allocated_ptr, header_size, kMinImplAlignment, kImplRCScale + immutable_bit, ImplFlags::kExternalData);
  init_external_header(impl_ptr, params.destroy_func, params.user_data);

  return init_dynamic_container<sizeof(size_t) * 8>(self, object_type, MetaByte{BLObjectDetail::kMetaFlagDR}, impl_ptr, params.external_data, params.size, params.capacity);
}

BLResult alloc_virt_impl(BLObjectCore* self, ObjectImplSize impl_size, const AllocVirtImplParams& params) noexcept {
  // Cannot happen - external Impls have no data associated with them, thus they have a relatively small size.
  BL_ASSERT(size_t(impl_size) <= kMaxImplSize);
  BL_ASSERT(axl::is_power_of_2(params.impl_alignment));
  BL_ASSERT(axl::is_between(params.impl_alignment, kMinImplAlignment, kMaxImplAlignment));

  constexpr size_t header_size = sizeof(ImplHeader);

  size_t allocation_size = size_t(impl_size) + header_size + params.impl_alignment;
  void* allocated_ptr = axl::mem_alloc(allocation_size);

  BLObjectType object_type = params.object_type;
  if (BL_UNLIKELY(!allocated_ptr)) {
    self->_d = bl_object_defaults[size_t(object_type)]._d;
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  void* impl_ptr = init_impl_after_allocation(allocated_ptr, header_size, params.impl_alignment, kImplRCScale, ImplFlags::kNone);
  return init_virt(self, object_type, BLObjectDetail::kMetaFlagDR, impl_ptr, params.virt);
}

} // {bl::ObjectLayer}

// Object - API - Construction & Destruction
// =========================================

BL_API_IMPL BLResult bl_object_init_move(BLObjectCore* self, BLObjectCore* other) noexcept {
  using namespace bl;
  BL_ASSERT(self != other);

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_object_init_weak(BLObjectCore* self, const BLObjectCore* other) noexcept {
  using namespace bl;
  BL_ASSERT(self != other);

  return ObjectLayer::copy_object(self, other);
}

template<typename T>
static BL_INLINE BLResult free_object_t(BLObjectCore* self, void* impl_ptr) noexcept {
  using DispatchInfo = bl::ObjectDispatchInfo<T>;
  return DispatchInfo::free_object(static_cast<T*>(self), static_cast<DispatchInfo::ImplType*>(impl_ptr));
}

BLResult bl_object_free_dynamic(BLObjectCore* self, void* impl_ptr) noexcept {
  using namespace bl;

  BL_ASSERT(self != nullptr);
  BL_ASSERT(impl_ptr != nullptr);
  BL_ASSERT(self->_d.is_dynamic());

  BLObjectType type = self->_d.type();

  // Release BLString, BLBitArray, and BLArray<> objects.
  if (type <= BL_OBJECT_TYPE_MAX_ARRAY) {
    if (type < BL_OBJECT_TYPE_MIN_ARRAY) {
      // Everything here doesn't need any specialized deallocation, so we delegate directly to dealloc_impl.
      return ObjectLayer::dealloc_impl(impl_ptr);
    }
    else {
      return free_object_t<BLArrayCore>(self, impl_ptr);
    }
  }

  switch (type) {
    case BL_OBJECT_TYPE_GRADIENT               : return free_object_t<BLGradientCore>(self, impl_ptr);
    case BL_OBJECT_TYPE_PATTERN                : return free_object_t<BLPatternCore>(self, impl_ptr);
    case BL_OBJECT_TYPE_FONT                   : return free_object_t<BLFontCore>(self, impl_ptr);
    case BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS  : return free_object_t<BLFontFeatureSettingsCore>(self, impl_ptr);
    case BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS: return free_object_t<BLFontVariationSettingsCore>(self, impl_ptr);
    case BL_OBJECT_TYPE_GLYPH_BUFFER           : return free_object_t<BLGlyphBufferCore>(self, impl_ptr);
    case BL_OBJECT_TYPE_COVERAGE_SET           : return free_object_t<BLCoverageSetCore>(self, impl_ptr);
    case BL_OBJECT_TYPE_PATH                   : return free_object_t<BLPathCore>(self, impl_ptr);
    case BL_OBJECT_TYPE_STROKE_OPTIONS         : return free_object_t<BLStrokeOptionsCore>(self, impl_ptr);

    // TODO: [STAGING] - IN PREPARATION
    // case BL_OBJECT_TYPE_PATH_EFFECT            : return free_object_t<BLPathEffectCore>(self, impl_ptr);

    default: {
      if (type >= BL_OBJECT_TYPE_MIN_VIRTUAL) {
        // Release objects that have virtual table.
        return self->_d.virt()->base.destroy(self);
      }
      else {
        // SECURITY: Anything else means we haven't handled the object type!
        return BL_SUCCESS;
      }
    }
  }
}

BL_API_IMPL BLResult bl_object_destroy(BLObjectCore* self) noexcept {
  using namespace bl;

  ObjectLayer::MetaByte meta_byte{self->_d.meta_byte()};
  void* impl_ptr = self->_d.dyn.impl.ptr;

  if (meta_byte.is_ref_counted() && ObjectLayer::deref_and_test_impl(impl_ptr)) {
    return bl_object_free_dynamic(self, impl_ptr);
  }
  else {
    return BL_SUCCESS;
  }
}

// Object - API - Reset
// ====================

BL_API_IMPL BLResult bl_object_reset(BLObjectCore* self) noexcept {
  using namespace bl;

  BLResult result = BL_SUCCESS;
  ObjectLayer::MetaByte meta_byte{self->_d.meta_byte()};

  BLObjectType type = self->_d.type();
  void* impl_ptr = self->_d.dyn.impl.ptr;

  if (meta_byte.is_ref_counted() && ObjectLayer::deref_and_test_impl(impl_ptr)) {
    result = bl_object_free_dynamic(self, impl_ptr);
  }

  self->_d = bl_object_defaults[type]._d;
  return result;
}

// Object - API - Assign
// =====================

BL_API_IMPL BLResult bl_object_assign_move(BLObjectCore* self, BLObjectCore* other) noexcept {
  using namespace bl;
  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_object_assign_weak(BLObjectCore* self, const BLObjectCore* other) noexcept {
  using namespace bl;
  return ObjectLayer::assign_object_weak(self, other);
}

BL_API_IMPL bool bl_object_is_external(const BLObjectCore* self) noexcept {
  using namespace bl;

  if (!self->_d.is_dynamic()) {
    return false;
  }

  const ObjectLayer::ImplHeader* header = ObjectLayer::get_impl_header(self->_d.impl());
  return axl::test(header->impl_flags, ObjectLayer::ImplFlags::kExternalData);
}

// Object - API - Properties
// =========================

BL_API_IMPL BLResult bl_object_get_property(const BLObjectCore* self, const char* name, size_t name_size, BLVarCore* value_out) noexcept {
  using namespace bl;

  if (name_size == SIZE_MAX) {
    name_size = strlen(name);
  }

  if (!self->_d.is_virtual()) {
    return bl_make_error(BL_ERROR_INVALID_KEY);
  }

  return self->_d.virt<BLObjectVirt>()->base.get_property(self, name, name_size, value_out);
}

BL_API_IMPL BLResult bl_object_get_property_bool(const BLObjectCore* self, const char* name, size_t name_size, bool* value_out) noexcept {
  using namespace bl;

  BLVarCore v;
  ObjectLayer::init_null(&v);

  *value_out = false;
  BL_PROPAGATE(bl_object_get_property(self, name, name_size, &v));

  BLResult result = bl_var_to_bool(&v, value_out);
  bl_var_destroy(&v);
  return result;
}

BL_API_IMPL BLResult bl_object_get_property_int32(const BLObjectCore* self, const char* name, size_t name_size, int32_t* value_out) noexcept {
  using namespace bl;

  BLVarCore v;
  ObjectLayer::init_null(&v);

  *value_out = 0;
  BL_PROPAGATE(bl_object_get_property(self, name, name_size, &v));

  BLResult result = bl_var_to_int32(&v, value_out);
  bl_var_destroy(&v);
  return result;
}

BL_API_IMPL BLResult bl_object_get_property_int64(const BLObjectCore* self, const char* name, size_t name_size, int64_t* value_out) noexcept {
  using namespace bl;

  BLVarCore v;
  ObjectLayer::init_null(&v);

  *value_out = 0;
  BL_PROPAGATE(bl_object_get_property(self, name, name_size, &v));

  BLResult result = bl_var_to_int64(&v, value_out);
  bl_var_destroy(&v);
  return result;
}

BL_API_IMPL BLResult bl_object_get_property_uint32(const BLObjectCore* self, const char* name, size_t name_size, uint32_t* value_out) noexcept {
  using namespace bl;

  BLVarCore v;
  ObjectLayer::init_null(&v);

  *value_out = 0;
  BL_PROPAGATE(bl_object_get_property(self, name, name_size, &v));

  BLResult result = bl_var_to_uint32(&v, value_out);
  bl_var_destroy(&v);
  return result;
}

BL_API_IMPL BLResult bl_object_get_property_uint64(const BLObjectCore* self, const char* name, size_t name_size, uint64_t* value_out) noexcept {
  using namespace bl;

  BLVarCore v;
  ObjectLayer::init_null(&v);

  *value_out = 0;
  BL_PROPAGATE(bl_object_get_property(self, name, name_size, &v));

  BLResult result = bl_var_to_uint64(&v, value_out);
  bl_var_destroy(&v);
  return result;
}

BL_API_IMPL BLResult bl_object_get_property_double(const BLObjectCore* self, const char* name, size_t name_size, double* value_out) noexcept {
  using namespace bl;

  BLVarCore v;
  ObjectLayer::init_null(&v);

  *value_out = 0.0;
  BL_PROPAGATE(bl_object_get_property(self, name, name_size, &v));

  BLResult result = bl_var_to_double(&v, value_out);
  bl_var_destroy(&v);
  return result;
}

BL_API_IMPL BLResult bl_object_set_property(BLObjectCore* self, const char* name, size_t name_size, const BLObjectCore* value) noexcept {
  if (name_size == SIZE_MAX)
    name_size = strlen(name);

  if (!self->_d.is_virtual()) {
    return bl_make_error(BL_ERROR_INVALID_KEY);
  }

  return self->_d.virt<BLObjectVirt>()->base.set_property(self, name, name_size, static_cast<const BLVarCore*>(value));
}

BL_API_IMPL BLResult bl_object_set_property_bool(BLObjectCore* self, const char* name, size_t name_size, bool value) noexcept {
  using namespace bl;

  // NOTE: Bool value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  ObjectLayer::init_bool(&v, value);
  return bl_object_set_property(self, name, name_size, &v);
}

BL_API_IMPL BLResult bl_object_set_property_int32(BLObjectCore* self, const char* name, size_t name_size, int32_t value) noexcept {
  using namespace bl;

  // NOTE: Integer value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  ObjectLayer::init_int64(&v, value);
  return bl_object_set_property(self, name, name_size, &v);
}

BL_API_IMPL BLResult bl_object_set_property_int64(BLObjectCore* self, const char* name, size_t name_size, int64_t value) noexcept {
  using namespace bl;

  // NOTE: Integer value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  ObjectLayer::init_int64(&v, value);
  return bl_object_set_property(self, name, name_size, &v);
}

BL_API_IMPL BLResult bl_object_set_property_uint32(BLObjectCore* self, const char* name, size_t name_size, uint32_t value) noexcept {
  using namespace bl;

  // NOTE: Integer value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  ObjectLayer::init_uint64(&v, value);
  return bl_object_set_property(self, name, name_size, &v);
}

BL_API_IMPL BLResult bl_object_set_property_uint64(BLObjectCore* self, const char* name, size_t name_size, uint64_t value) noexcept {
  using namespace bl;

  // NOTE: Integer value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  ObjectLayer::init_uint64(&v, value);
  return bl_object_set_property(self, name, name_size, &v);
}

BL_API_IMPL BLResult bl_object_set_property_double(BLObjectCore* self, const char* name, size_t name_size, double value) noexcept {
  using namespace bl;

  // NOTE: Double value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  ObjectLayer::init_double(&v, value);
  return bl_object_set_property(self, name, name_size, &v);
}

BLResult bl_object_impl_get_property(const BLObjectCore* self, const char* name, size_t name_size, BLVarCore* value_out) noexcept {
  using namespace bl;

  axl::maybe_unused(self, name, name_size, value_out);
  return bl_make_error(BL_ERROR_INVALID_KEY);
}

BLResult bl_object_impl_set_property(BLObjectCore* self, const char* name, size_t name_size, const BLVarCore* value) noexcept {
  using namespace bl;

  axl::maybe_unused(self, name, name_size, value);
  return bl_make_error(BL_ERROR_INVALID_KEY);
}
