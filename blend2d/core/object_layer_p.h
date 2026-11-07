// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_OBJECT_LAYER_P_H_INCLUDED
#define BLEND2D_CORE_OBJECT_LAYER_P_H_INCLUDED

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_alloc.h>
#include <blend2d/axl/wrap_p.h>
#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/api_impl.h>
#include <blend2d/core/object_layer.h>
#include <blend2d/threading/atomic_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name BLObject - Internals - Constants
//! \{

//! A table that contains default constructed objects of each object type.
extern BLObjectCore bl_object_defaults[BL_OBJECT_TYPE_MAX_VALUE + 1];

void BL_CDECL bl_object_destroy_external_func_dummy(BLObjectCore* object, void* external_data, void* user_data) noexcept;

BLResult bl_object_free_dynamic(BLObjectCore* self, BLObjectImpl* impl, size_t meta_byte) noexcept;

//! \}

//! \name BLObject - Internals - Property Handling
//! \{

BLResult BL_CDECL bl_object_impl_get_property(const BLObjectCore* self, const char* name, size_t name_size, BLVarCore* value_out) BL_NOEXCEPT_C;
BLResult BL_CDECL bl_object_impl_set_property(BLObjectCore* self, const char* name, size_t name_size, const BLVarCore* value) BL_NOEXCEPT_C;

template<size_t kN>
static BL_INLINE bool bl_match_property_literal(const char* key, size_t key_size, const char(&str)[kN]) noexcept {
  return key_size == sizeof(str) - 1u && memcmp(key, str, key_size) == 0;
}

//! \}

namespace bl {

//! Strongly typed size in bytes, used in places where byte-size could get confusing with array size in element units.
enum class ByteSize : size_t {};

//! Strongly typed object impl size to not confuse it with regular size / capacity of containers.
enum class ObjectImplSize : size_t {};

//! Provides essential information about each object type.
template<typename T> struct ObjectDispatchInfo;

} // {bl}


// Object Layer
// ------------

namespace bl::ObjectLayer {

// Object Layer - Constants
// ------------------------

//! Minimum object impl alignment that the Impl allocator honors.
static constexpr size_t kMinImplAlignment = 16;
//! Maximum object impl alignment that can be specified.
static constexpr size_t kMaxImplAlignment = 64;

//! Maximum impl size: MaximumTheoreticalAddressableMemory / 2 - 4096.
//!
//! In 64-bit mode, the impl size can be at most 48 bits as that's the size of the capacity field in BLObjectDetail,
//! however, in 32-bit mode the maximum capacity allowed is roughly the maximum theoretical addressable memory divided
//! by 2 and decreased by a constant.
static constexpr size_t kMaxImplSize = sizeof(size_t) > 4 ? size_t(0x0000FFFFFFFFFF00u) : (0x7FFFFFFFu - 4095u);

//! Maximum impl size of objects that store capacity as 32-bit integer.
static constexpr size_t kMaxImplSize32 = sizeof(size_t) > 4 ? 0xFFFFFF00u : (0x7FFFFFFFu - 4095u);

//! Specifies scale of Impl reference count.
//!
//! The purpose is to be able to store one bit of payload at LSB that would describe immutability.
static constexpr size_t kImplRCScale = 2u;

//! Immutability flag directly in `ImplHeader::ref_count`.
static constexpr size_t kImplRCImmutableFlag = 1u;

//! Value of an eternal reference count.
static constexpr size_t kImplRCEternal = 0;

//! The scale of impl-offset adjustment (used to align the allocated Impl and then to get the original pointer back).
static constexpr size_t kImplAdjustOffsetScale = 4u;

// Object Layer - Object Info Wrappers
// -----------------------------------

template<typename T> using object_impl_of = ObjectDispatchInfo<typename T::CoreType>::ImplType;
template<typename T> inline constexpr size_t data_offset_of = ObjectDispatchInfo<typename T::CoreType>::kImplDataOffset;

// Object Layer - Meta Byte Wrapper
// --------------------------------

//! Wrapper for a meta_byte, which is read from meta field of BLObjectCore. A different type is used to disallow
//! a confusion with any other type, especially for parameter passing in utilities provided by bl::Core namespace.
struct MetaByte {
  size_t v;

  BL_INLINE_NODEBUG bool check_flag(size_t flag) const noexcept { return (v & flag) != 0u; }
  BL_INLINE_NODEBUG bool check_multiple(size_t flags) const noexcept { return (v & flags) == flags; }

  BL_INLINE_NODEBUG bool is_sso() const noexcept { return !check_flag(BLObjectDetail::kMetaFlagD); }
  BL_INLINE_NODEBUG bool is_dynamic() const noexcept { return check_flag(BLObjectDetail::kMetaFlagD); }
  BL_INLINE_NODEBUG bool is_external() const noexcept { return check_multiple(BLObjectDetail::kMetaFlagDX); }
  BL_INLINE_NODEBUG bool is_ref_counted() const noexcept { return check_flag(BLObjectDetail::kMetaFlagR); }
};

// Object Layer - Size & Capacity Utilities
// ----------------------------------------

static BL_INLINE_NODEBUG bool size_fits_32_bits(size_t size) noexcept {
#if BL_TARGET_ARCH_BITS < 64
  axl::maybe_unused(size);
  return true;
#else
  return size <= 0xFFFFFFFFu;
#endif
}

// Object Layer - Helper Structures
// --------------------------------

//! Impl header - 16 bytes, which precedes \ref BLObjectImpl.
struct ImplHeader {
  //! Data that can be used freely by Impl, never touched by object allocator nor underlying object layer.
  uint8_t impl_data[16 - (sizeof(size_t) + sizeof(uint8_t))];
  //! Data related to object allocator - it cannot be altered.
  uint8_t allocator_data;
  //! Reference count - see \ref kImplRCScale and \ref kImplRCImmutableFlag for implementation details.
  size_t ref_count;
};

template<typename T>
inline constexpr size_t align_of_impl = axl::max<size_t>(alignof(T), kMinImplAlignment);

//! Holds information that is required to release external data that Impl references.
//!
//! \note The `destroy_func` is always non-null - if the user passes `nullptr` as a `destroy_func` to Blend2D API it would
//! be replaced with a built-in "dummy" function that does nothing to make sure we only have a single code-path
struct ExternalHeader {
  //! Destroy callback to be called when Impl holding the external data is being destroyed.
  BLDestroyExternalDataFunc destroy_func;
  //! Data provided by the user to identify the external data, passed to destroy_func() as `user_data`.
  void* user_data;
};

template<size_t N>
struct EternalPadding {
  uint8_t unused[N];
};

//! Only used for storing built-in default Impl instances.
template<typename ImplType>
struct alignas(32) EternalImpl {
  static_assert(align_of_impl<ImplType> <= 32u, "Maximum alignment of EternalImpl is 32 bytes");

  EternalPadding<32u - sizeof(ImplHeader)> padding;
  ImplHeader header;
  ImplType impl;
};

//! Only used for storing built-in default Impl instances with virtual function table.
template<typename ImplType, typename Virt>
struct alignas(32) EternalVirtImpl {
  static_assert(align_of_impl<ImplType> <= 32u, "Maximum alignment of EternalVirtImpl is 32 bytes");

  EternalPadding<32u - sizeof(ImplHeader)> padding;
  ImplHeader header;
  axl::Wrap<ImplType> impl;
  Virt virt;
};

// Object Layer - Impl - Accessors
// -------------------------------

//! Returns a pointer to the header of `impl`.
static BL_INLINE_NODEBUG ImplHeader* get_impl_header(BLObjectImpl* impl) noexcept {
  return axl::deoffset_ptr<ImplHeader>(impl, sizeof(ImplHeader));
}

//! Returns a pointer to the header of `impl` (const).
static BL_INLINE_NODEBUG const ImplHeader* get_impl_header(const BLObjectImpl* impl) noexcept {
  return axl::deoffset_ptr<const ImplHeader>(impl, sizeof(ImplHeader));
}

//! Returns a pointer to the header of `impl`.
static BL_INLINE_NODEBUG ExternalHeader* get_external_header(BLObjectImpl* impl) noexcept {
  return axl::deoffset_ptr<ExternalHeader>(impl, sizeof(ImplHeader) + sizeof(ExternalHeader));
}

// Object Layer - Impl - Commons
// -----------------------------

static BL_INLINE size_t ref_count_base_from_immutable_bit(size_t immutable_bit) noexcept {
  return kImplRCScale + immutable_bit;
}

static BL_INLINE size_t ref_count_base_from_access_mode(BLDataAccessMode access_mode) noexcept {
  return kImplRCScale + size_t(access_mode == BL_DATA_ACCESS_MODE_RO);
}

static BL_INLINE size_t impl_header_size_from_meta_byte(MetaByte meta_byte) noexcept {
  if constexpr (ObjectDetail::kMetaFlagX == sizeof(ExternalHeader)) {
    // Fast-path used in 64-bit mode, which takes the advantage of X flag placement in meta_byte.
    size_t offset = meta_byte.v & ObjectDetail::kMetaFlagX;
    return offset + sizeof(ImplHeader);
  }
  else {
    return sizeof(ImplHeader) + (meta_byte.is_external() ? sizeof(ExternalHeader) : size_t(0));
  }
}

static BL_INLINE size_t impl_offset_from_meta_byte(MetaByte meta_byte) noexcept {
  size_t alignment_field = meta_byte.v & 0x0Fu;
  return impl_header_size_from_meta_byte(meta_byte) + (alignment_field * kImplAdjustOffsetScale);
}

static BL_INLINE void* allocated_ptr_from_impl(void* impl_storage, MetaByte meta_byte) noexcept {
  return axl::deoffset_ptr(impl_storage, impl_offset_from_meta_byte(meta_byte));
}

// Object Layer - Impl - External
// ------------------------------

static BL_INLINE void init_external_header(BLObjectImpl* impl, BLDestroyExternalDataFunc destroy_func, void* user_data) noexcept {
  ExternalHeader* external_header = get_external_header(impl);
  external_header->destroy_func = destroy_func ? destroy_func : bl_object_destroy_external_func_dummy;
  external_header->user_data = user_data;
}

static BL_INLINE void call_external_destroy_func(BLObjectCore* self, BLObjectImpl* impl, void* external_data) noexcept {
  ExternalHeader* external_header = get_external_header(impl);
  external_header->destroy_func(self, external_data, external_header->user_data);
}

// Object Layer - Impl - Alloc & Dealloc
// -------------------------------------

BLResult alloc_impl(BLObjectCore* self, BLObjectType object_type, ObjectImplSize impl_size) noexcept;
BLResult alloc_impl_aligned(BLObjectCore* self, BLObjectType object_type, ObjectImplSize impl_size, size_t impl_alignment) noexcept;

struct AllocContainerImplParams {
  static constexpr bool is_external = false;
  static constexpr bool is_with_data = false;

  BLObjectType object_type;
  uint32_t data_offset;
  size_t size;
  size_t capacity;
};

struct AllocContainerImplWithDataParams {
  static constexpr bool is_external = false;
  static constexpr bool is_with_data = true;

  BLObjectType object_type;
  uint32_t data_offset;
  size_t size;
  size_t capacity;
  const void* src_data;
  size_t src_data_size;
};

BLResult alloc_container_impl_32(BLObjectCore* self, ObjectImplSize impl_size, const AllocContainerImplParams& params) noexcept;
BLResult alloc_container_impl_with_data_32(BLObjectCore* self, ObjectImplSize impl_size, const AllocContainerImplWithDataParams& params) noexcept;

#if BL_TARGET_ARCH_BITS >= 64
BLResult alloc_container_impl_64(BLObjectCore* self, ObjectImplSize impl_size, const AllocContainerImplParams& params) noexcept;
BLResult alloc_container_impl_with_data_64(BLObjectCore* self, ObjectImplSize impl_size, const AllocContainerImplWithDataParams& params) noexcept;
#endif

template<typename Object>
static BL_INLINE BLResult alloc_container_impl(Object* self, ObjectImplSize impl_size, const AllocContainerImplParams& params) noexcept {
#if BL_TARGET_ARCH_BITS >= 64
  if constexpr (Object::Info::kLayout == ObjectLayout::kNative64) {
    return alloc_container_impl_64(self, impl_size, params);
  }
  else {
    return alloc_container_impl_32(self, impl_size, params);
  }
#else
  return alloc_container_impl_32(self, impl_size, params);
#endif
}

template<typename Object>
static BL_INLINE BLResult alloc_container_impl_with_data(Object* self, ObjectImplSize impl_size, const AllocContainerImplWithDataParams& params) noexcept {
#if BL_TARGET_ARCH_BITS >= 64
  if constexpr (Object::Info::kLayout == ObjectLayout::kNative64) {
    return alloc_container_impl_with_data_64(self, impl_size, params);
  }
  else {
    return alloc_container_impl_with_data_32(self, impl_size, params);
  }
#else
  return alloc_container_impl_with_data_32(self, impl_size, params);
#endif
}

struct AllocExternalImplParams {
  static constexpr bool is_external = true;
  static constexpr bool is_with_data = false;

  BLObjectType object_type;
  BLDataAccessMode access_mode;
  void* external_data;
  size_t size;
  size_t capacity;
  BLDestroyExternalDataFunc destroy_func;
  void* user_data;
};
BLResult alloc_external_impl(BLObjectCore* self, ObjectImplSize impl_size, const AllocExternalImplParams& params) noexcept;

struct AllocVirtImplParams {
  BLObjectType object_type;
  const BLObjectVirt* virt;
  size_t impl_alignment;
};
BLResult alloc_virt_impl(BLObjectCore* self, ObjectImplSize impl_size, const AllocVirtImplParams& params) noexcept;

template<typename I, typename T>
static BL_INLINE BLResult alloc_virt_impl_t(
  T* self,
  const typename T::VirtType* virt,
  size_t impl_alignment = axl::max(kMinImplAlignment, alignof(I))
) noexcept {
  return alloc_virt_impl(self, ObjectImplSize{sizeof(I)}, AllocVirtImplParams{
    .object_type = T::Info::kType,
    .virt = virt,
    .impl_alignment = impl_alignment
  });
}

template<typename T>
static BL_INLINE BLResult alloc_impl_t(BLObjectCore* self, BLObjectType object_type, ObjectImplSize impl_size = ObjectImplSize{sizeof(T)}) noexcept {
  return alloc_impl(self, object_type, impl_size);
}

template<typename T>
static BL_INLINE BLResult alloc_impl_aligned_t(BLObjectCore* self, BLObjectType object_type, ObjectImplSize impl_size, size_t impl_alignment) noexcept {
  return alloc_impl_aligned(self, object_type, impl_size, impl_alignment);
}

static BL_INLINE BLResult dealloc_impl(void* impl_storage, MetaByte meta_byte) noexcept {
  void* ptr = allocated_ptr_from_impl(impl_storage, meta_byte);
  axl::mem_free(ptr);
  return BL_SUCCESS;
}

// Object Layer - Impl - Size Expansion (Containers)
// -------------------------------------------------

template<size_t max_size = kMaxImplSize>
static BL_INLINE ObjectImplSize expand_impl_size(ObjectImplSize impl_size) noexcept {
  size_t n = size_t(impl_size);
  size_t s = size_t(n >= BL_ALLOC_GROWTH_LIMIT_50) +
             size_t(n >= BL_ALLOC_GROWTH_LIMIT_25);
  return ObjectImplSize{axl::min(n + (n >> s) + BL_ALLOC_GROWTH_ADDEND, max_size)};
}

static BL_INLINE ObjectImplSize expand_impl_size_32(ObjectImplSize impl_size) noexcept {
  return expand_impl_size<kMaxImplSize32>(impl_size);
}

template<size_t max_size = kMaxImplSize>
static BL_INLINE ObjectImplSize expand_impl_size_if(ObjectImplSize impl_size, bool expand) noexcept {
  size_t n = size_t(impl_size);
  size_t s = size_t(n >= BL_ALLOC_GROWTH_LIMIT_50) +
             size_t(n >= BL_ALLOC_GROWTH_LIMIT_25);
  if (!expand) {
    s = axl::bit_size_of<ObjectImplSize> - 1u;
  }
  return ObjectImplSize{axl::min(n + (n >> s) + BL_ALLOC_GROWTH_ADDEND, max_size)};
}

// Object Layer - Impl - Reference Counting
// ----------------------------------------

static BL_INLINE_NODEBUG void init_impl_ref_count(BLObjectImpl* impl, size_t value) noexcept {
  return Atomic::store_relaxed(get_impl_header(impl)->ref_count, value);
}

static BL_INLINE_NODEBUG size_t get_impl_ref_count(const BLObjectImpl* impl) noexcept {
  return Atomic::fetch_relaxed(get_impl_header(impl)->ref_count);
}

static BL_INLINE_NODEBUG void retain_impl(BLObjectImpl* impl, size_t n = 1u) noexcept {
  Atomic::fetch_add_rc(get_impl_header(impl)->ref_count, n * kImplRCScale);
}

static BL_INLINE_NODEBUG bool deref_and_test_impl(BLObjectImpl* impl) noexcept {
  size_t prev_ref_count = Atomic::fetch_sub_rc(get_impl_header(impl)->ref_count, kImplRCScale);
  return prev_ref_count <= (kImplRCScale | kImplRCImmutableFlag);
}

//! Tests whether the `impl` is mutable.
static BL_INLINE_NODEBUG bool is_impl_mutable(const BLObjectImpl* impl) noexcept { return get_impl_ref_count(impl) == kImplRCScale; }

//! Tests whether the `impl` is unique.
//!
//! \remarks Unique impl doesn't mean it's mutable, it just means that it's not shared with any other object.
//! Always use \ref is_impl_mutable() to check for mutability without doing any modifications.
static BL_INLINE_NODEBUG bool is_impl_unique(const BLObjectImpl* impl) noexcept { return (get_impl_ref_count(impl) - 1u) < (kImplRCScale | kImplRCImmutableFlag); }

// Object Layer - Object - Impl
// ----------------------------

template<typename T>
static BL_INLINE_NODEBUG object_impl_of<T>* get_impl(const T* self) noexcept {
  return static_cast<object_impl_of<T>*>(self->_d.dyn.impl.ptr);
}

template<typename ImplType, typename T>
static BL_INLINE_NODEBUG ImplType* get_impl(const T* self) noexcept {
  return static_cast<ImplType*>(static_cast<object_impl_of<T>*>(self->_d.dyn.impl.ptr));
}

// Object Layer - Object - Free
// ----------------------------

template<typename T, typename ImplType>
static BL_INLINE_NODEBUG BLResult free_object(T* self, ImplType* impl, MetaByte meta_byte) noexcept {
  return ObjectDispatchInfo<typename T::CoreType>::free_object(self, impl, meta_byte);
}

// Object Layer - Object - Lifetime Management
// -------------------------------------------

//! Initializes this BLObjectDetail with object that uses dynamic storage (Impl).
template<typename Object>
BL_INLINE_NODEBUG BLResult init_dyn(Object* self, BLObjectType object_type, uint8_t meta_byte, BLObjectImpl* impl) noexcept {
  self->_d.zero_all();
  self->_d.dyn.impl.ptr = impl;
  self->_d.set_type_byte(uint8_t(object_type));
  self->_d.set_meta_byte(uint8_t(meta_byte | ObjectDetail::kMetaFlagD));
  return BL_SUCCESS;
}

//! Initializes this BLObjectDetail with object that uses dynamic storage (Impl) with additional `data` member.
template<typename Object>
BL_INLINE_NODEBUG BLResult init_dyn(Object* self, BLObjectType object_type, uint8_t meta_byte, BLObjectImpl* impl, void* data) noexcept {
  self->_d.zero_all();
  self->_d.dyn.data_or_virt.ptr = data;
  self->_d.dyn.impl.ptr = impl;
  self->_d.set_type_byte(uint8_t(object_type));
  self->_d.set_meta_byte(uint8_t(meta_byte | ObjectDetail::kMetaFlagD));
  return BL_SUCCESS;
}

//! Initializes this BLObjectDetail with object that uses dynamic storage (Impl).
template<typename Object>
BL_INLINE_NODEBUG BLResult init_virt(Object* self, BLObjectType object_type, uint8_t meta_byte, BLObjectImpl* impl, const BLObjectVirt* virt) noexcept {
  self->_d.zero_all();
  self->_d.dyn.data_or_virt.ptr = (void*)(virt);
  self->_d.dyn.impl.ptr = impl;
  self->_d.set_type_byte(uint8_t(object_type));
  self->_d.set_meta_byte(meta_byte | ObjectDetail::kMetaFlagD);
  return BL_SUCCESS;
}

//! Tests whether an object is mutable.
template<ObjectMode M, typename T>
static BL_INLINE_NODEBUG bool is_object_mutable(const T* self) noexcept {
  if constexpr (M == ObjectMode::kSSO) {
    return true;
  }
  else if constexpr (M >= ObjectMode::kDyn) {
    return is_impl_mutable(self->_d.template impl<object_impl_of<T>>());
  }
  else {
    if (self->_d.is_ref_counted()) {
      return is_impl_mutable(self->_d.template impl<object_impl_of<T>>());
    }
    else {
      return self->_d.is_sso();
    }
  }
}

template<typename T>
static BL_INLINE_NODEBUG bool is_object_mutable(const T* self) noexcept {
  return is_object_mutable<T::Info::kMode, T>(self);
}

template<ObjectMode M, typename T>
static BL_INLINE BLResult retain_object(const T* self, size_t n = 1) noexcept {
  using ImplType = object_impl_of<T>;

  if constexpr (M == ObjectMode::kSSO) {
    return BL_SUCCESS;
  }
  else if constexpr (M == ObjectMode::kRC) {
    retain_impl(self->_d.template impl<ImplType>(), n);
    return BL_SUCCESS;
  }
  else {
    if (self->_d.is_ref_counted()) {
      retain_impl(self->_d.template impl<ImplType>(), n);
    }
    return BL_SUCCESS;
  }
}

template<typename T>
static BL_INLINE_NODEBUG BLResult retain_object(const T* self, size_t n = 1) noexcept {
  return retain_object<T::Info::kMode, T>(self, n);
}

template<ObjectMode M, typename T>
static BL_INLINE BLResult release_object(T* self, MetaByte meta_byte) noexcept {
  using ImplType = object_impl_of<T>;

  if constexpr (M == ObjectMode::kSSO) {
    return BL_SUCCESS;
  }
  else if constexpr (M == ObjectMode::kRC) {
    BL_ASSERT(meta_byte.is_ref_counted());

    ImplType* impl = self->_d.template impl<ImplType>();
    if (deref_and_test_impl(impl)) {
      return free_object(self, impl, meta_byte);
    }
    return BL_SUCCESS;
  }
  else {
    if (meta_byte.is_ref_counted()) {
      ImplType* impl = self->_d.template impl<ImplType>();
      if (deref_and_test_impl(impl)) {
        return free_object(self, impl, meta_byte);
      }
    }
    return BL_SUCCESS;
  }
}

template<typename T>
static BL_INLINE_NODEBUG BLResult release_object(T* self, MetaByte meta_byte) noexcept {
  return release_object<T::Info::kMode, T>(self, meta_byte);
}

template<ObjectMode M, typename T>
static BL_INLINE_NODEBUG BLResult release_object(T* self) noexcept {
  return release_object<M, T>(self, MetaByte{self->_d.meta_byte()});
}

template<typename T>
static BL_INLINE_NODEBUG BLResult release_object(T* self) noexcept {
  return release_object<T::Info::kMode, T>(self);
}

template<typename T>
static BL_INLINE_NODEBUG BLResult init_object(T* self) noexcept
  requires (T::Info::kType != BL_OBJECT_TYPE_UNKNOWN)
{
  if constexpr (ObjectDispatchInfo<T>::kHasInitSSO) {
    ObjectDispatchInfo<T>::init_sso(self);
  }
  else {
    self->_d = bl_object_defaults[T::Info::kType]._d;
  }
  return BL_SUCCESS;
};

template<ObjectMode M, typename T>
static BL_INLINE_NODEBUG BLResult reset_object(T* self) noexcept {
  release_object<M>(self);
  return init_object(self);
}

template<typename T>
static BL_INLINE_NODEBUG BLResult reset_object(T* self) noexcept {
  return reset_object<T::Info::kMode, T>(self);
}

template<typename T>
static BL_INLINE_NODEBUG BLResult reset_after_move(T* self) noexcept {
  if constexpr (T::Info::kType == BL_OBJECT_TYPE_UNKNOWN) {
    self->_d = bl_object_defaults[self->_d.type()]._d;
    return BL_SUCCESS;
  }
  else {
    return init_object(self);
  }
};

template<ObjectMode M, typename T>
static BL_INLINE_NODEBUG BLResult replace_object(T* self, const T* other) noexcept {
  BL_ASSERT(self != other);

  release_object<M>(self);
  self->_d = other->_d;
  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE_NODEBUG BLResult replace_object(T* self, const T* other) noexcept {
  return replace_object<T::Info::kMode, T>(self, other);
}

template<typename T>
static BL_INLINE_NODEBUG BLResult assign_object_move(T* self, T* other) noexcept {
  BLObjectDetail tmp_d = other->_d;
  reset_after_move(other);

  release_object<T::Info::kMode>(self);
  self->_d = tmp_d;
  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE_NODEBUG BLResult assign_object_weak(T* self, const T* other) noexcept {
  retain_object<T::Info::kMode>(other);
  release_object<T::Info::kMode>(self);

  self->_d = other->_d;
  return BL_SUCCESS;
}

template<ObjectMode M, typename T>
static BL_INLINE_NODEBUG BLResult copy_object(T* self, const T* other) noexcept {
  BL_ASSERT(self != other);

  self->_d = other->_d;
  return retain_object<M, T>(self);
}

template<typename T>
static BL_INLINE_NODEBUG BLResult copy_object(T* self, const T* other) noexcept {
  return copy_object<T::Info::kMode>(self, other);
}

template<typename T>
static BL_INLINE_NODEBUG BLResult move_object(T* self, T* other) noexcept {
  BL_ASSERT(self != other);

  self->_d = other->_d;
  return reset_after_move(other);
}

// Object Layer - Object - Array Lifetime Management
// -------------------------------------------------

template<typename T>
static BL_INLINE_NODEBUG BLResult copy_object_array(T* dst, const T* src, size_t count) noexcept {
  for (size_t i = 0; i < count; i++) {
    dst[i]._d = src[i]._d;
    retain_object<ObjectMode::kAny>(&dst[i], 1u);
  }
  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE_NODEBUG BLResult assign_object_array(T* dst, const T* src, size_t count) noexcept {
  for (size_t i = 0; i < count; i++) {
    assign_object_weak(&dst[i], &src[i]);
  }
  return BL_SUCCESS;
}

static BL_INLINE BLResult retain_object_array(const BLObjectCore* self, size_t count) noexcept {
  for (size_t i = 0; i < count; i++) {
    retain_object<ObjectMode::kAny>(&self[i], 1u);
  }
  return BL_SUCCESS;
}

static BL_INLINE BLResult release_object_array(BLObjectCore* self, size_t count) noexcept {
  for (size_t i = 0; i < count; i++) {
    release_object<ObjectMode::kAny>(&self[i]);
  }
  return BL_SUCCESS;
}

// Object Layer - Object - Container Utilities
// -------------------------------------------

template<typename T>
struct DataSpan {
  T* data;
  size_t size;
};

template<size_t sso_capacity, typename T>
static BL_INLINE BLResult init_sso_buffer(T* dst, const T* src, size_t size) noexcept {
  size_t i;

  BL_NOUNROLL
  for (i = 0u; i < size; i++) {
    dst[i] = src[i];
  }

  BL_NOUNROLL
  while (i < sso_capacity) {
    dst[i++] = 0.0f;
  }

  return BL_SUCCESS;
}

template<typename Object>
BL_INLINE_NODEBUG void set_dyn_data(Object* self, void* data_ptr) noexcept {
  static_assert(Object::Info::kLayout == ObjectLayout::kNative32 ||
                Object::Info::kLayout == ObjectLayout::kNative64,
                "bl::ObjectLayer::set_dyn_data() cannot be used on objects that use a non-native container layout");
  self->_d.dyn.data_or_virt.ptr = data_ptr;
}

template<typename Object>
BL_INLINE_NODEBUG void set_dyn_capacity(Object* self, size_t capacity) noexcept {
  static_assert(Object::Info::kLayout != ObjectLayout::kUnknown);

  if constexpr (Object::Info::kLayout == ObjectLayout::kNative64) {
    if constexpr (std::endian::native == std::endian::little) {
      self->_d.dyn.capacity_u48 = (self->_d.dyn.capacity_u48 & 0xFFFF000000000000u) | (capacity);
    }
    else {
      self->_d.dyn.capacity_u48 = (self->_d.dyn.capacity_u48 & 0x000000000000FFFFu) | (capacity << 16);
    }
  }
  else {
    self->_d.dyn.capacity_u32 = uint32_t(capacity);
  }
}

template<typename T, typename Object>
[[nodiscard]] BL_INLINE_NODEBUG DataSpan<T> object_sso_span(Object* self) noexcept {
  return {sso_data<T>(self), sso_size(self)};
}

template<typename T, typename Object>
[[nodiscard]] BL_INLINE_NODEBUG DataSpan<T> object_dyn_span(Object* self) noexcept {
  return {dyn_data<T>(self), dyn_size(self)};
}

template<typename T, typename Object>
[[nodiscard]] BL_INLINE_NODEBUG DataSpan<T> object_get_span(Object* self) noexcept {
  return {get_data<T>(self), get_size(self)};
}

template<typename T>
[[nodiscard]] static BL_INLINE_NODEBUG bool check_sso_capacity(const T* self, size_t need, size_t used = 0) noexcept {
  axl::maybe_unused(self);

  constexpr size_t capacity = T::Info::kSSOCapacity;
  BL_ASSERT(used <= capacity);

  size_t remaining = capacity - used;
  return need <= remaining;
}

template<typename T>
[[nodiscard]] static BL_INLINE_NODEBUG bool check_dynamic_capacity_and_mutability_raw(const T* self, size_t need, size_t remaining) noexcept {
  BL_ASSERT(self->_d.is_dynamic());

  size_t immutable_mask = axl::bool_as_mask<size_t>(!is_object_mutable<ObjectMode::kDyn>(self));
  return (need | immutable_mask) <= remaining;
}

template<typename T>
[[nodiscard]] static BL_INLINE_NODEBUG bool check_dynamic_capacity_and_mutability(const T* self, size_t need, size_t used = 0u) noexcept {
  BL_ASSERT(self->_d.is_dynamic());

  size_t capacity = dyn_capacity(self);
  BL_ASSERT(used <= capacity);

  return check_dynamic_capacity_and_mutability_raw(self, need, capacity - used);
}

// Object Layer - Object - Atomic Operations
// -----------------------------------------

template<typename T>
struct alignas(32) AtomicObject { T obj; };

//! \name BLObject - Internals - Atomic Content Utilities
//! \{

static BL_INLINE size_t& object_footer_of(BLObjectDetail& d) noexcept { return reinterpret_cast<size_t*>(d._sso_data)[32u / sizeof(size_t) - 1u]; }

static constexpr size_t atomic_object_footer_unpopulated = axl::repeat_byte<size_t>(0xFFu);
static constexpr size_t atomic_object_footer_populating = axl::repeat_byte<size_t>(0xFFu - 1u);
static constexpr size_t atomic_object_populate_spin_count = 20;

//! Initializes an object into 'unpopulated' state, which means the content was initialized into a representation
//! that is not a valid object, but it's an unpopulated state in the context of `AtomicObject`. To query the state
//! or object content use \ref atomic_object_is_populated() and \ref atomic_object_get_or_populate(), respectively.
template<typename T>
static BL_INLINE void atomic_object_init_as_unpopulated(AtomicObject<T>& atomic_object) noexcept {
  memset(atomic_object.obj._d._sso_data, 0xFF, sizeof(AtomicObject<T>));
}

//! Returns true when the object was populated and contains an object in a valid state.
template<typename T>
static BL_INLINE bool atomic_object_is_populated(AtomicObject<T>& atomic_object) noexcept {
  return (Atomic::fetch_strong(object_footer_of(atomic_object.obj._d)) & atomic_object_footer_populating) != atomic_object_footer_populating;
}

//! Destroys AtomicObject.
//!
//! If the object was not populated this function does nothing. When it was populated it calls the respective release
//! handler of that object.
template<typename T>
static BL_INLINE void atomic_object_destroy_populated(AtomicObject<T>& atomic_object) noexcept {
  if (atomic_object_is_populated(atomic_object)) {
    ObjectLayer::release_object(&atomic_object.obj);
  }
}

template<typename PopulateFn, typename T>
static BL_INLINE BLResult atomic_object_get_or_populate(AtomicObject<T>& atomic_object, T& out, PopulateFn&& populate_fn) noexcept {
  if (atomic_object_is_populated(atomic_object)) {
    out._d = atomic_object.obj._d;
    return BL_SUCCESS;
  }

  populate_fn(out);

  size_t& footer = object_footer_of(atomic_object.obj._d);
  size_t state = atomic_object_footer_unpopulated;

  if (Atomic::compare_exchange(footer, state, atomic_object_footer_populating)) {
    // Changed state from Unpopulated -> Populating - so let's populate the object and be done.
    retain_object(&out);

    size_t* dst_ptr = reinterpret_cast<size_t*>(atomic_object.obj._d._sso_data);
    const size_t* src_ptr = reinterpret_cast<size_t*>(out._d._sso_data);

    for (size_t i = 0; i < sizeof(BLObjectDetail) / sizeof(size_t); i++) {
      Atomic::store_strong(dst_ptr[i], src_ptr[i]);
    }

    Atomic::thread_fence();
    return BL_SUCCESS;
  }

  size_t remaining_spins = atomic_object_populate_spin_count;
  while (state == atomic_object_footer_populating) {
    if (remaining_spins == 0u) {
      // We have exhausted the number of spins we had - we already have a populated object, so
      // let's use it instead of waiting for other thread to populate this AtomicObject.
      return BL_SUCCESS;
    }
    remaining_spins--;
    state = Atomic::fetch_strong(footer);
  }

  // The object has been populated meanwhile - we don't need `out` we have populated ourselves.
  release_object(&out);
  copy_object(&out, &atomic_object.obj);
  return BL_SUCCESS;
}

} // {bl::ObjectLayer}

//! \}

namespace bl {

// Default dispatch information of BLObjectCore - this must handle any type of object, thus delegating `free_object()` to the default C-API.
template<>
struct ObjectDispatchInfo<BLObjectCore> {
  using ImplType = BLObjectImpl;

  static constexpr bool kHasInitSSO = false;

  static BL_INLINE BLResult free_object(BLObjectCore* self, ImplType* impl, ObjectLayer::MetaByte meta_byte) noexcept {
    return bl_object_free_dynamic(self, impl, meta_byte.v);
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_OBJECT_LAYER_P_H_INCLUDED
