// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_OBJECT_P_H_INCLUDED
#define BLEND2D_OBJECT_P_H_INCLUDED

#include "api-internal_p.h"
#include "api-impl.h"
#include "object.h"
#include "support/intops_p.h"
#include "support/ptrops_p.h"
#include "support/wrap_p.h"
#include "threading/atomic_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name BLObject - Internals - Constants
//! \{

//! Default object impl alignment that the Impl allocator honors.
static constexpr size_t BL_OBJECT_IMPL_ALIGNMENT = 16;

//! Maximum impl size: MaximumTheoreticalAddressableMemory / 2 - 4096.
//!
//! \note The reason we divide the theoretical addressable space by 2 is to never allocate anything that would
//! have a sign bit set. In addition, the sign bit then can be used as a flag in \ref BLObjectImplHeader.
static constexpr size_t BL_OBJECT_IMPL_MAX_SIZE = (SIZE_MAX / 2u) - 4096u;

//! \}

//! \name BLObject - Internals - Strong Types
//! \{

//! Strongly typed object impl size to not confuse it with regular size / capacity of containers.
BL_DEFINE_STRONG_TYPE(BLObjectImplSize, size_t)

//! \}

//! \name BLObject - Internals - Structs
//! \{

//! BLObjectImpl header, which precedes BLObjectImpl.
struct BLObjectImplHeader {
  //! \name Members
  //! \{

  //! Reference count.
  size_t refCount;

  //!   - [0]     - 'R' RefCount flag (if set, the impl data is reference countable, and refcount is not 0).
  //!   - [1]     - 'I' Immutable flag (if set, the impl data is immutable, and the refCount base is 2).
  //!   - [5:2]   - alignment offset multiplied by 4 to subtract from the impl to get the original allocated pointer.
  //!   - [MSB]   - 'X' External flag (if the impl holds external data and BLDestroyExternalDataFunc pointer + userData).
  //!
  //! \note Immutable flag can only be set when also RefCount flag is set. By design all Impls are immutable when
  //! `refCount != 1`, so Immutable flag is only useful when the Impl is RefCounted. When not refCounted, the Impl
  //! is immutable by design.
  size_t flags;

  //! \}

  //! \name
  //! \{

  enum : uint32_t {
    kRefCountedFlagShift = 0,
    kImmutableFlagShift = 1,
    kExternalFlagShift = bl::IntOps::bitSizeOf<size_t>() - 1u,
    kAlignmentMaskShift = 2
  };

  enum : size_t {
    kRefCountedFlag = size_t(0x01u) << kRefCountedFlagShift,
    kImmutableFlag = size_t(0x01u) << kImmutableFlagShift,
    kRefCountedAndImmutableFlags = kRefCountedFlag | kImmutableFlag,

    kExternalFlag = size_t(0x01u) << kExternalFlagShift,
    kAlignmentOffsetMask = size_t(0x1Fu) << kAlignmentMaskShift
  };

  //! \}

  //! \name Accessors
  //! \{

  //! Returns the number of bytes used for alignment of the impl (0, 4, 8, 12, 16, ..., 56).
  BL_INLINE_NODEBUG size_t alignmentOffset() const noexcept { return flags & kAlignmentOffsetMask; }

  //! Tests whether this object impl is reference counted.
  BL_INLINE_NODEBUG bool isRefCounted() const noexcept { return refCount != 0u; }
  //! Tests whether this object impl is immutable.
  BL_INLINE_NODEBUG bool isImmutable() const noexcept { return (flags & kRefCountedAndImmutableFlags) != kRefCountedFlag; }
  //! Tests whether this object impl holds external data.
  BL_INLINE_NODEBUG bool isExternal() const noexcept { return (flags & kExternalFlag) != 0u; }

  //! Returns the base reference count value (if the reference count goes below the object must be freed).
  //!
  //! The returned value describes a reference count of Impl that would signalize that it's not shared with any other
  //! object. The base value is always 1 for mutable Impls and 3 for immutable Impls. This function just uses some
  //! trick to make the extraction of this value as short as possible in the resulting machine code.
  //!
  //! \note Why it works this way? Typically the runtime only check the reference-count to check whether an Impl can
  //! be modified. If the reference count is not 1 the Impl cannot be modified. This makes it simple to check whether
  //! an Impl is mutable.
  BL_INLINE_NODEBUG size_t baseRefCountValue() const noexcept { return flags & kRefCountedAndImmutableFlags; }

  //! \}
};

//! Provides information necessary to release external data that Impl references.
//!
//! \note The `destroyFunc` is always non-null - if the user passes `nullptr` as a `destroyFunc` to Blend2D API it would
//! be replaced with a built-in "dummy" function that does nothing to make sure we only have a single code-path
struct BLObjectExternalInfo {
  //! Destroy callback to be called when Impl holding the external data is being destroyed.
  BLDestroyExternalDataFunc destroyFunc;
  //! Data provided by the user to identify the external data, passed to destroyFunc() as `userData`.
  void* userData;
};

//! BLObjectImpl having a virtual function table.
struct BLObjectVirtImpl : public BLObjectImpl {
  const BLObjectVirt* virt;
};

struct alignas(16) BLObjectEternalHeader {
#if BL_TARGET_ARCH_BITS == 32
  uint64_t padding;
#endif
  BLObjectImplHeader header;
};

//! Only used for storing built-in default Impls.
template<typename Impl>
struct alignas(16) BLObjectEternalImpl {
  BLObjectEternalHeader header;
  bl::Wrap<Impl> impl;
};

//! Only used for storing built-in default Impls with virtual function table.
template<typename Impl, typename Virt>
struct alignas(16) BLObjectEternalVirtualImpl {
  BLObjectEternalHeader header;
  bl::Wrap<Impl> impl;
  Virt virt;
};

//! \}

//! \name BLObject - Internals - Globals
//! \{

//! Object header used by \ref bl::ObjectInternal::isInstanceMutable() and similar functions to avoid branching in SSO case.
BL_HIDDEN extern const BLObjectImplHeader blObjectHeaderWithRefCountEq0;

//! Object header used by \ref bl::ObjectInternal::isInstanceMutable() and similar functions to avoid branching in SSO case.
BL_HIDDEN extern const BLObjectImplHeader blObjectHeaderWithRefCountEq1;

//! A table that contains default constructed objects of each object type.
BL_HIDDEN extern BLObjectCore blObjectDefaults[BL_OBJECT_TYPE_MAX_VALUE + 1];

BL_HIDDEN void BL_CDECL blObjectDestroyExternalDataDummy(void* impl, void* externalData, void* userData) noexcept;

//! \}

//! \name BLObject - Internals - Property Handling
//! \{

BL_HIDDEN BLResult BL_CDECL blObjectImplGetProperty(const BLObjectImpl* impl, const char* name, size_t nameSize, BLVarCore* valueOut) BL_NOEXCEPT_C;
BL_HIDDEN BLResult BL_CDECL blObjectImplSetProperty(BLObjectImpl* impl, const char* name, size_t nameSize, const BLVarCore* value) BL_NOEXCEPT_C;

static BL_INLINE bool blMatchProperty(const char* key, size_t keySize, const char* str) noexcept {
  size_t strSize = strlen(str);
  return keySize == strSize && memcmp(key, str, keySize) == 0;
}

//! \}

//! \name BLObject - Internals - Cast From Unknown
//! \{

//! Casts the given unknown pointer to `BLObjectCore*`.
static BL_INLINE_NODEBUG BLObjectCore* blAsObject(BLUnknown* unknown) { return static_cast<BLObjectCore*>(unknown); }
//! Casts the given unknown pointer to `BLObjectCore*` (const).
static BL_INLINE_NODEBUG const BLObjectCore* blAsObject(const BLUnknown* unknown) { return static_cast<const BLObjectCore*>(unknown); }

//! \}

BL_HIDDEN BLResult blObjectDestroyUnknownImpl(BLObjectImpl* impl, BLObjectInfo info) noexcept;

namespace bl {

//! Reference counting mode.
enum class RCMode : uint32_t {
  //! It's not known whether the Impl is reference counted (useful for "always-dynamic" objects that don't check object info).
  kMaybe,
  //! It's guaranteed that the Impl is reference counted (for example BLObjectInfo::isRefCountedObject() returned true).
  kForce
};

} // {bl}

namespace bl {
namespace ObjectInternal {

//! \name BLObject - Internals - Impl - Header
//! \{

//! Returns a pointer to the header of `impl`.
static BL_INLINE_NODEBUG BLObjectImplHeader* getImplHeader(BLObjectImpl* impl) noexcept {
  return PtrOps::deoffset<BLObjectImplHeader>(impl, sizeof(BLObjectImplHeader));
}

//! Returns a pointer to the header of `impl` (const).
static BL_INLINE_NODEBUG const BLObjectImplHeader* getImplHeader(const BLObjectImpl* impl) noexcept {
  return PtrOps::deoffset<const BLObjectImplHeader>(impl, sizeof(BLObjectImplHeader));
}

//! \}

//! \name BLObject - Internals - Impl - Alloc / Free
//! \{

static BL_INLINE void* getAllocatedPtr(BLObjectImpl* impl) noexcept {
  const BLObjectImplHeader* header = getImplHeader(impl);
  size_t offset = sizeof(BLObjectImplHeader);

  if (header->isExternal())
    offset = sizeof(BLObjectExternalInfo) + sizeof(BLObjectImplHeader);

  return PtrOps::deoffset(impl, offset + header->alignmentOffset());
}

template<typename T>
static BL_INLINE BLResult allocImplT(BLObjectCore* self, BLObjectInfo info, BLObjectImplSize implSize = BLObjectImplSize{sizeof(T)}) noexcept {
  return blObjectAllocImpl(self, info.bits, implSize.value());
}

template<typename T>
static BL_INLINE BLResult allocImplAlignedT(BLObjectCore* self, BLObjectInfo info, BLObjectImplSize implSize, size_t implAlignment) noexcept {
  return blObjectAllocImplAligned(self, info.bits, implSize.value(), implAlignment);
}

template<typename T>
static BL_INLINE BLResult allocImplExternalT(BLObjectCore* self, BLObjectInfo info, bool immutable, BLDestroyExternalDataFunc destroyFunc, void* userData) noexcept {
  return blObjectAllocImplExternal(self, info.bits, sizeof(T), immutable, destroyFunc, userData);
}

template<typename T>
static BL_INLINE BLResult allocImplExternalT(BLObjectCore* self, BLObjectInfo info, BLObjectImplSize implSize, bool immutable, BLDestroyExternalDataFunc destroyFunc, void* userData) noexcept {
  return blObjectAllocImplExternal(self, info.bits, implSize.value(), immutable, destroyFunc, userData);
}

static BL_INLINE BLResult freeImpl(BLObjectImpl* impl) noexcept {
  void* ptr = getAllocatedPtr(impl);
  free(ptr);
  return BL_SUCCESS;
}

static BL_INLINE BLResult freeVirtualImpl(BLObjectImpl* impl) noexcept {
  return static_cast<BLObjectVirtImpl*>(impl)->virt->base.destroy(static_cast<BLObjectImpl*>(impl));
}

//! \}

//! \name BLObject - Internals - Impl - External
//! \{

//! Tests whether the Impl uses external data.
static BL_INLINE bool isImplExternal(const BLObjectImpl* impl) noexcept {
  const BLObjectImplHeader* header = getImplHeader(impl);
  return header->isExternal();
}

//! Returns a pointer to the header of `impl`.
static BL_INLINE_NODEBUG BLObjectExternalInfo* getExternalInfo(BLObjectImpl* impl) noexcept {
  return PtrOps::deoffset<BLObjectExternalInfo>(impl, sizeof(BLObjectExternalInfo) + sizeof(BLObjectImplHeader));
}

//! Returns a pointer to the header of `impl` (const).
static BL_INLINE_NODEBUG const BLObjectExternalInfo* getExternalInfo(const BLObjectImpl* impl) noexcept {
  return PtrOps::deoffset<const BLObjectExternalInfo>(impl, sizeof(BLObjectExternalInfo) + sizeof(BLObjectImplHeader));
}

static BL_INLINE void initExternalDestroyFunc(BLObjectImpl* impl, BLDestroyExternalDataFunc destroyFunc, void* userData) noexcept {
  BLObjectExternalInfo* externalInfo = getExternalInfo(impl);
  externalInfo->destroyFunc = destroyFunc ? destroyFunc : blObjectDestroyExternalDataDummy;
  externalInfo->userData = userData;

}

static BL_INLINE void callExternalDestroyFunc(BLObjectImpl* impl, void* externalData) noexcept {
  BLObjectExternalInfo* externalInfo = getExternalInfo(impl);
  externalInfo->destroyFunc(impl, externalData, externalInfo->userData);
}

//! \}

//! \name BLObject - Internals - Impl - Reference Counting
//! \{

//! Tests whether the `impl` is mutable.
static BL_INLINE bool isImplMutable(const BLObjectImpl* impl) noexcept {
  const BLObjectImplHeader* header = getImplHeader(impl);
  return header->refCount == 1;
}

//! Tests whether the `impl` is reference counted.
static BL_INLINE bool isImplRefCounted(const BLObjectImpl* impl) noexcept {
  const BLObjectImplHeader* header = getImplHeader(impl);
  return header->isRefCounted();
}

//! Tests whether the `impl` reference count is the same as its initial value.
//!
//! This check essentially checks whether these is only a single remaining reference to the `impl`.
static BL_INLINE bool isImplRefCountEqualToBase(const BLObjectImpl* impl) noexcept {
  const BLObjectImplHeader* header = getImplHeader(impl);
  return header->refCount == header->baseRefCountValue();
}

//! Initializes the reference count of `impl` to its base value, considering the passed `immediate` flag.
//!
//! The base value is either 1 if the impl is mutable, or 3, if the impl is immutable.
static BL_INLINE void initRefCountToBase(BLObjectImpl* impl, bool immutable) noexcept {
  size_t riFlags = BLObjectImplHeader::kRefCountedFlag | (size_t(immutable) << BLObjectImplHeader::kImmutableFlagShift);

  BLObjectImplHeader* header = getImplHeader(impl);
  header->flags = (header->flags & ~BLObjectImplHeader::kImmutableFlag) | riFlags;
}

//! Returns a reference count of `impl`.
static BL_INLINE size_t getImplRefCount(const BLObjectImpl* impl) noexcept {
  return getImplHeader(impl)->refCount;
}

template<RCMode kRCMode>
static BL_INLINE void retainImpl(BLObjectImpl* impl, size_t n = 1u) noexcept {
  if (kRCMode == RCMode::kMaybe && !isImplRefCounted(impl))
    return;
  blAtomicFetchAddRelaxed(&getImplHeader(impl)->refCount, n);
}

template<RCMode kRCMode>
static BL_INLINE bool derefImplAndTest(BLObjectImpl* impl) noexcept {
  BLObjectImplHeader* header = getImplHeader(impl);
  size_t baseRefCount = header->baseRefCountValue();

  if (kRCMode == RCMode::kMaybe && !baseRefCount)
    return false;

  return blAtomicFetchSubStrong(&header->refCount) == baseRefCount;
}

template<RCMode kRCMode>
static BL_INLINE BLResult releaseVirtualImpl(BLObjectImpl* impl) noexcept {
  return derefImplAndTest<kRCMode>(impl) ? freeVirtualImpl(impl) : BLResult(BL_SUCCESS);
}

//! \}
//! \name BLObject - Internals - Object Utilities
//! \{

//! Tests whether an untyped object is mutable.
//!
//! \note This function supports both SSO and dynamic objects. SSO object always returns true. If you want to check
//! whether the object is dynamic and that dynamic object has a mutable impl, use `isInstanceImplMutable()` instead.
static BL_INLINE bool isInstanceMutable(const BLObjectCore* self) noexcept {
  const BLObjectImplHeader* header = self->_d.sso() ? &blObjectHeaderWithRefCountEq1 : getImplHeader(self->_d.impl);
  return header->refCount == 1u;
}

//! Tests whether an untyped object is dynamic and has a mutable Impl.
static BL_INLINE bool isInstanceDynamicAndMutable(const BLObjectCore* self) noexcept {
  const BLObjectImplHeader* header = self->_d.sso() ? &blObjectHeaderWithRefCountEq0 : getImplHeader(self->_d.impl);
  return header->refCount == 1u;
}

//! Tests whether an object that always has a dynamic Impl is mutable.
static BL_INLINE bool isDynamicInstanceMutable(const BLObjectCore* self) noexcept {
  BL_ASSERT(self->_d.isDynamicObject());

  const BLObjectImplHeader* header = getImplHeader(self->_d.impl);
  return header->refCount == 1u;
}

template<typename T>
static BL_INLINE BLResult retainInstance(const T* self, size_t n = 1) noexcept {
  if (self->_d.isRefCountedObject())
    retainImpl<RCMode::kForce>(self->_d.impl, n);
  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE BLResult releaseUnknownInstance(T* self) noexcept {
  BLObjectInfo info = self->_d.info;
  if (info.isDynamicObject() && derefImplAndTest<RCMode::kMaybe>(self->_d.impl))
    return blObjectDestroyUnknownImpl(self->_d.impl, info);
  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE BLResult releaseVirtualInstance(T* self) noexcept {
  BL_ASSERT(self->_d.isVirtualObject());
  return releaseVirtualImpl<RCMode::kMaybe>(self->_d.impl);
}

template<typename T>
static BL_INLINE BLResult replaceVirtualInstance(T* self, const T* other) noexcept {
  BL_ASSERT(self->_d.isVirtualObject());
  BL_ASSERT(other->_d.isVirtualObject());

  BLObjectImpl* impl = self->_d.impl;
  self->_d = other->_d;
  return releaseVirtualImpl<RCMode::kMaybe>(impl);
}

template<typename T>
static BL_INLINE BLResult assignVirtualInstance(T* dst, const T* src) noexcept {
  retainInstance(src);
  releaseVirtualInstance(dst);

  dst->_d = src->_d;
  return BL_SUCCESS;
}

//! \}

} // {ObjectInternal}
} // {bl}

//! \name BLObject - Internals - Reference Counting and Object Lifetime
//! \{

template<typename T>
static BL_INLINE BLResult blObjectPrivateInitMoveTagged(T* dst, T* src) noexcept {
  dst->_d = src->_d;
  src->_d = blObjectDefaults[src->_d.rawType()]._d;
  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE BLResult blObjectPrivateInitMoveUnknown(T* dst, T* src) noexcept {
  dst->_d = src->_d;
  src->_d = blObjectDefaults[dst->_d.getType()]._d;
  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE BLResult blObjectPrivateInitWeakTagged(T* dst, const T* src) noexcept {
  dst->_d = src->_d;
  return bl::ObjectInternal::retainInstance(dst);
}

template<typename T>
static BL_INLINE BLResult blObjectPrivateInitWeakUnknown(T* dst, const T* src) noexcept {
  dst->_d = src->_d;
  return bl::ObjectInternal::retainInstance(dst);
}

template<typename T>
static BL_INLINE BLResult blObjectPrivateAssignWeakUnknown(T* dst, const T* src) noexcept {
  bl::ObjectInternal::retainInstance(src);
  bl::ObjectInternal::releaseUnknownInstance(dst);

  dst->_d = src->_d;
  return BL_SUCCESS;
}

//! \}

//! \name BLObject - Internals - Expanding Utilities (Containers)
//! \{

static BL_INLINE size_t blObjectGrowImplSizeToPowerOf2(size_t x) noexcept {
  return size_t(1u) << (bl::IntOps::bitSizeOf<size_t>() - bl::IntOps::clz(x + 1u));
}

static BL_INLINE BLObjectImplSize blObjectAlignImplSize(BLObjectImplSize implSize) noexcept {
  return BLObjectImplSize(bl::IntOps::alignUp(implSize.value(), 64u));
}

static BL_INLINE BLObjectImplSize blObjectExpandImplSize(BLObjectImplSize implSize) noexcept {
  size_t n = implSize.value();

  if (n >= BL_ALLOC_GROW_LIMIT)
    n = n + (n >> 2) + (n >> 3); // Makes the capacity 37.5% greater.
  else
    n = blObjectGrowImplSizeToPowerOf2(n); // Doubles the capacity.

  // If an overflow happened during any of the computation above `blMax()` would cancel it and make it fitting.
  return BLObjectImplSize(blMax(n, implSize.value()));
}

static BLObjectImplSize blObjectExpandImplSizeWithModifyOp(BLObjectImplSize implSize, BLModifyOp modifyOp) noexcept {
  if (blModifyOpDoesGrow(modifyOp))
    return blObjectExpandImplSize(implSize);
  else
    return implSize;
}

//! \}

//! \name BLObject - Internals - Atomic Content Utilities
//! \{

//! Initializes an object to a representation suitable for using `blObjectAtomicAssignMove()` on it.
static BL_INLINE void blObjectAtomicContentInit(BLObjectCore* self) noexcept {
  self->_d.u64_data[0] = 0;
  self->_d.u64_data[1] = 0;
}

//! Tests whether the object contains a valid instance.
//!
//! Freshly initialized object by `blObjectAtomicContentInit()` returns false. When a moving into the object is still
//! in progress `false` is returned as well. When the first called `blObjectAtomicAssignMove()` finishes, `true` is
//! returned.
static BL_INLINE bool blObjectAtomicContentTest(const BLObjectCore* self) noexcept {
  return blAtomicFetchStrong(&self->_d.info.bits) > 1u;
}

//! Moves `other` to `self` atomically.
//!
//! The `self` object must have been initialized by `blObjectAtomicContentInit()` or assigned by
//! `blObjectAtomicAssignMove()` - the later case would be detected by the implementation.
//!
//! Returns `true` when the object was successfully moved, `false` otherwise.
//!
//! \note If `false` was returned it doesn't mean that `self` has been successfully initialized by other thread. It
//! means that the implementation failed to move `other` to `self`, because some other thread started moving into
//! that object first, however, it could be still moving the object when `blObjectAtomicAssignMove()` returns.
static BL_NOINLINE bool blObjectAtomicContentMove(BLObjectCore* self, BLObjectCore* other) noexcept {
  // TODO: This should use CMPXCHG16B on X86_64 when available.
  BL_ASSERT(self != other);

  // Maximum number of spins to wait for another thread in case of high contention.
  constexpr size_t kMaxSpins = 100;

  BLObjectDetail otherD = other->_d;
  uint32_t selfInfo = 0;

  if (blAtomicCompareExchange(&self->_d.info.bits, &selfInfo, 1u)) {
    // We have successfully acquired the info so we can perform the move.
    self->_d = otherD;
    blAtomicThreadFence();

    other->_d = blObjectDefaults[otherD.rawType()]._d;
    return true;
  }
  else {
    // Other thread is either moving at the moment or did already move.
    size_t spinCount = kMaxSpins;

    // Wait for a bit for another thread to finish the atomic assignment.
    for (;;) {
      if (selfInfo > 1u) {
        // `self` is now a valid object, however, `other` was not moved, so reset it.
        blObjectReset(other);
        return true;
      }

      if (--spinCount == 0)
        return false;

      selfInfo = blAtomicFetchStrong(&self->_d.info.bits);
    }
  }
}

//! \}

//! \}
//! \endcond

#endif // BLEND2D_OBJECT_P_H_INCLUDED
