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

//! \name Object - Internals - Constants
//! \{

//! Default object impl alignment, should be the same as cache line size, althought we won't go above 64.
static constexpr size_t BL_OBJECT_IMPL_ALIGNMENT = 64;

//! Shift applied to allocation adjustment in BLObjectDetail's A field.
//!
//! When the allocator allocates Impl it aligns it to `BL_OBJECT_IMPL_ALIGNMENT`, but it has to remember how to
//! deallocate (free) it. Allocation adjustment is used for that and stored in BLObjectDetail A field shifted right
//! by `BL_OBJECT_IMPL_ALLOC_ADJUST_SHIFT`.
static constexpr size_t BL_OBJECT_IMPL_ALLOC_ADJUST_SHIFT = 3;

//! Maximum impl size ~= MaximumTheoreticalAddressableMemory - 4096 - ImplAlignment - ExternalInfo - RefCount.
//!
//! \note This is just for sanity checks. We don't expect that in a real world user would be able to go anywhere
//! close to this number even if the operating system would allow the process to allocate most of the addressable
//! memory (32-bit process can theoretically allocate up to 4GB if it's running on a 64-bit operating system). The
//! thing is that even dynamically loaded libraries would eat some of the memory, so even in that case the process
//! would not be able to have all the theoretical addressable memory for user data.
static constexpr size_t BL_OBJECT_IMPL_MAX_SIZE =
  (SIZE_MAX - 4096u + 1u) - BL_OBJECT_IMPL_ALIGNMENT - sizeof(BLObjectExternalInfo) - sizeof(void*);

//! \}

//! \name Object - Internals - Strong Types
//! \{

//! Strongly typed object impl size to not confuse it with regular size / capacity of containers.
BL_DEFINE_STRONG_TYPE(BLObjectImplSize, size_t)

//! \}

//! \name Object - Internals - Structs
//! \{

// TODO [Object]: Unused. It was planned to return this instead of the Impl itself from Impl allocators.
template<typename T>
struct BLObjectImplWithInfo {
  T* impl;
  size_t info;
};

//! BLObject Impl having a virtual function table.
struct BLObjectVirtImpl : public BLObjectImpl {
  const BLObjectVirt* virt;
};

//! Only used for storing built-in default Impls.
template<typename Impl>
struct alignas(16) BLObjectEthernalImpl {
  uint8_t padding[16 - sizeof(size_t)];

  size_t refCount;
  BLWrap<Impl> impl;
};

//! Only used for storing built-in default Impls with virtual function table.
template<typename Impl, typename Virt>
struct alignas(16) BLObjectEthernalVirtualImpl {
  uint8_t padding[16 - sizeof(size_t)];

  size_t refCount;
  BLWrap<Impl> impl;
  Virt virt;
};

//! \}

//! \name Object - Internals - Globals
//! \{

//! A table that contains reference count that is used for IsMutable checks of Impls without a reference count.
BL_HIDDEN extern const size_t blObjectDummyRefCount[1];

//! A table that contains default constructed objects of each object type.
BL_HIDDEN extern BLObjectCore blObjectDefaults[BL_OBJECT_TYPE_MAX_VALUE + 1];

//! \}

//! \name Object - Internals - Property Handling
//! \{

BL_HIDDEN BLResult BL_CDECL blObjectImplGetProperty(const BLObjectImpl* impl, const char* name, size_t nameSize, BLVarCore* valueOut) BL_NOEXCEPT_C;
BL_HIDDEN BLResult BL_CDECL blObjectImplSetProperty(BLObjectImpl* impl, const char* name, size_t nameSize, const BLVarCore* value) BL_NOEXCEPT_C;

static BL_INLINE bool blMatchProperty(const char* key, size_t keySize, const char* str) noexcept {
  size_t strSize = strlen(str);
  return keySize == strSize && memcmp(key, str, keySize) == 0;
}

//! \}

//! \name Object - Internals - Cast From Unknown
//! \{

//! Casts the given unknown pointer to `BLObjectCore*`.
static BL_INLINE BLObjectCore* blAsObject(BLUnknown* unknown) { return static_cast<BLObjectCore*>(unknown); }
//! Casts the given unknown pointer to `BLObjectCore*` (const).
static BL_INLINE const BLObjectCore* blAsObject(const BLUnknown* unknown) { return static_cast<const BLObjectCore*>(unknown); }

//! \}

//! \name Object - Internals - Type Checks
//! \{

static BL_INLINE bool blObjectTypeIsNumber(BLObjectType type) noexcept {
  return type == BL_OBJECT_TYPE_INT64  ||
         type == BL_OBJECT_TYPE_UINT64 ||
         type == BL_OBJECT_TYPE_DOUBLE ;
}

//! \}

//! \name Object - Internals - Basic checks
//! \{

//! Returns a base value of a reference count of Impl. The returned value describes a reference count of Impl that
//! would signalize that it's not shared with any other object. The base value is always 1 for mutable Impls and 3
//! for immutable Impls. This function just uses some trick to make the extraction of this value as short as
//! possible in the resulting machine code.
//!
//! \note Why it works this way? Typically the runtime only check the reference-count to check whether an Impl can
//! be modified. If the reference count is not 1 the Impl cannot be modified. This makes it simple to check whether
//! an Impl is mutable.
static BL_INLINE constexpr size_t blObjectImplGetRefCountBaseFromObjectInfo(BLObjectInfo info) noexcept {
  return size_t((info.bits                   >> BL_OBJECT_INFO_RC_INIT_SHIFT) &
                (BL_OBJECT_INFO_RC_INIT_MASK >> BL_OBJECT_INFO_RC_INIT_SHIFT));
}

//! \}

//! \name Object - Internals - Allocation Adjustment
//!
//! Allocation adjustment is used to adjust and deadjust 'impl' so it's properly aligned.
//!
//! \{

//! Calculates adjustment that is stored in BLObjectDetail's header A field.
static BL_INLINE uint32_t blObjectImplCalcAllocationAdjustmentField(void* impl, void* allocatedImplPtr) noexcept {
  uint32_t adjustment = uint32_t((uintptr_t(impl) - uintptr_t(allocatedImplPtr)) >> BL_OBJECT_IMPL_ALLOC_ADJUST_SHIFT) - 1u;

  // Make sure the aField doesn't overflow its range.
  uint32_t aField = adjustment << BL_OBJECT_INFO_A_SHIFT;
  BL_ASSERT((aField & ~BL_OBJECT_INFO_A_MASK) == 0);

  return aField;
}

//! Applies allocation adjustment to get the original pointer from `impl` that can be passed to `free()`.
static BL_INLINE void* blObjectImplDeadjustImplPtr(void* impl, BLObjectInfo info) noexcept {
  uintptr_t aField = info.aField();
  return BLPtrOps::deoffset(impl, (aField + 1) << BL_OBJECT_IMPL_ALLOC_ADJUST_SHIFT);
}

//! \}

//! \name Object - Internals - External Data
//! \{

struct BLObjectExternalInfoAndData {
  BLObjectExternalInfo* info;
  void* optionalData;
};

static BL_INLINE BLObjectExternalInfoAndData blObjectDetailGetExternalInfoAndData(void* impl, void* allocatedImplPtr, BLObjectImplSize implSize) noexcept {
  constexpr size_t kRefCountSize = sizeof(size_t);
  constexpr size_t kExtInfoSize = sizeof(BLObjectExternalInfo);
  constexpr size_t kExtOptDataSize = 32;

  size_t implOffset = size_t(uintptr_t(impl) - uintptr_t(allocatedImplPtr));
  size_t spaceBeforeRefCount = implOffset - sizeof(size_t);

  intptr_t extInfoOffset;
  intptr_t optDataOffset;

  if (spaceBeforeRefCount >= kExtInfoSize + kExtOptDataSize) {
    // +-+-+-+-+-+-+-+-+
    // |64ByteCacheLine|
    // +-+-+-+-+-+-+-+-+---------------+-+
    //   |o|o|o|o|X|X|R|   Impl Data   | |
    // +-+-+-+-+-+-+-+-+---------------+-+
    // | |o|o|o|o|X|X|R|   Impl Data   |
    // +-+-+-+-+-+-+-+-+---------------+
    extInfoOffset = -intptr_t(kRefCountSize + kExtInfoSize);
    optDataOffset = -intptr_t(kRefCountSize + kExtInfoSize + kExtOptDataSize);
  }
  else if (spaceBeforeRefCount >= kExtOptDataSize) {
    // +-+-+-+-+-+-+-+-+
    // |64ByteCacheLine|
    // +-+-+-+-+-+-+-+-+---------------+-+-+-+
    //       |o|o|o|o|R|   Impl Data   |X|X| |
    //     +-+-+-+-+-+-+---------------+-+-+-+
    //     | |o|o|o|o|R|   Impl Data   |X|X|
    //     +-+-+-+-+-+-+---------------+-+-+
    extInfoOffset = intptr_t(implSize.value());
    optDataOffset = -intptr_t(kRefCountSize + kExtOptDataSize);
  }
  else if (spaceBeforeRefCount >= kExtInfoSize) {
    // +-+-+-+-+-+-+-+-+
    // |64ByteCacheLine|
    // +-+-+-+-+-+-+-+-+---------------+-+-+-+-+-+
    //           |X|X|R|   Impl Data   |o|o|o|o| |
    //         +-+-+-+-+---------------+-+-+-+-+-+
    //         | |X|X|R|   Impl Data   |o|o|o|o|
    //         +-+-+-+-+---------------+-+-+-+-+
    extInfoOffset = -intptr_t(kRefCountSize + kExtInfoSize);
    optDataOffset = intptr_t(implSize.value());
  }
  else {
    // +-+-+-+-+-+-+-+-+
    // |64ByteCacheLine|
    // +-+-+-+-+-+-+-+-+---------------+-+-+-+-+-+-+-+
    //               |R|   Impl Data   |X|X|o|o|o|o| |
    //             +-+-+---------------+-+-+-+-+-+-+-+
    //             | |R|   Impl Data   |X|X|o|o|o|o|
    //             +-+-+---------------+-+-+-+-+-+-+
    extInfoOffset = intptr_t(implSize.value());
    optDataOffset = intptr_t(implSize.value() + kExtInfoSize);
  }

  return BLObjectExternalInfoAndData {
    BLPtrOps::offset<BLObjectExternalInfo>(impl, extInfoOffset),
    BLPtrOps::offset(impl, optDataOffset)
  };
}

static BL_INLINE BLObjectExternalInfoAndData blObjectDetailGetExternalInfoAndData(void* impl, BLObjectInfo info, BLObjectImplSize implSize) noexcept {
  return blObjectDetailGetExternalInfoAndData(impl, blObjectImplDeadjustImplPtr(impl, info), implSize);
}

static BL_INLINE void blObjectDetailCallExternalDestroyFunc(void* impl, BLObjectInfo info, BLObjectImplSize implSize, void* externalData) noexcept {
  BLObjectExternalInfoAndData ext = blObjectDetailGetExternalInfoAndData(impl, info, implSize);
  ext.info->destroyFunc(impl, externalData, ext.info->userData);
}

//! \}

//! \name Object - Internals - Alloc / Free Impl
//! \{

template<typename T>
static BL_INLINE T* blObjectDetailAllocImplT(BLObjectCore* self, BLObjectInfo info, BLObjectImplSize implSize, BLObjectImplSize* implSizeOut) noexcept {
  return static_cast<T*>(blObjectDetailAllocImpl(&self->_d, info.bits, implSize.value(), implSizeOut->valuePtr()));
}

template<typename T>
static BL_INLINE T* blObjectDetailAllocImplT(BLObjectCore* self, BLObjectInfo info, BLObjectImplSize implSize = BLObjectImplSize{sizeof(T)}) noexcept {
  BLObjectImplSize dummy;
  return blObjectDetailAllocImplT<T>(self, info, implSize, &dummy);
}

template<typename T>
static BL_INLINE T* blObjectDetailAllocImplExternalT(BLObjectCore* self, BLObjectInfo info, BLObjectImplSize implSize, BLObjectExternalInfo** externalInfoOut, void** optionalDataOut) noexcept {
  return static_cast<T*>(blObjectDetailAllocImplExternal(&self->_d, info.bits, implSize.value(), externalInfoOut, optionalDataOut));
}

static BL_INLINE BLResult blObjectImplFreeInline(void* impl, BLObjectInfo info) noexcept {
  BL_ASSERT(info.isDynamicObject());

  void* allocatedImplPtr = blObjectImplDeadjustImplPtr(impl, info);
  free(allocatedImplPtr);

  return BL_SUCCESS;
}

static BL_INLINE BLResult blObjectImplFreeVirtual(void* impl, BLObjectInfo info) noexcept {
  BL_ASSERT(info.virtualFlag());
  return static_cast<BLObjectVirtImpl*>(impl)->virt->base.destroy(static_cast<BLObjectImpl*>(impl), info.bits);
}

//! \}

//! \name Object - Internals - Reference Counting Utilities
//! \{

//! Returns a pointer to the reference count of the given `impl`.
//!
//! \note The Impl must be a real object Impl and its BLObjectDetail must have the RefCount flag set to 1.
static BL_INLINE size_t* blObjectImplGetRefCountPtr(void* impl) noexcept {
  return static_cast<size_t*>(BLPtrOps::deoffset(impl, sizeof(size_t)));
}

//! \overload
static BL_INLINE const size_t* blObjectImplGetRefCountPtr(const void* impl) noexcept {
  return static_cast<const size_t*>(BLPtrOps::deoffset(impl, sizeof(size_t)));
}

//! Returns a reference count of object's `impl`.
//!
//! \note The Impl must be a real object `impl` and its BLObjectDetail must have the RefCount flag set to 1.
static BL_INLINE size_t blObjectImplGetRefCount(const void* impl) noexcept {
  return *blObjectImplGetRefCountPtr(impl);
}

//! Initializes a reference count of a freshly allocated `impl` to its initial `value`.
static BL_INLINE void blObjectImplInitRefCount(void* impl, size_t value = 1u) noexcept {
  *static_cast<volatile size_t*>(BLPtrOps::deoffset(impl, sizeof(size_t))) = value;
}

static BL_INLINE void blObjectImplAddRef(void* impl, size_t value = 1u) noexcept {
  blAtomicFetchAdd(blObjectImplGetRefCountPtr(impl), value);
}

static BL_INLINE bool blObjectImplDecRefAndTest(void* impl, BLObjectInfo info) noexcept {
  size_t rcPrev = blAtomicFetchSub(blObjectImplGetRefCountPtr(impl), 1u);
  size_t rcInit = blObjectImplGetRefCountBaseFromObjectInfo(info);

  return rcPrev == rcInit;
}

// An optimized function that checks whether the object is ref-counted and then performs blObjectImplDecRefAndTest().
static BL_INLINE bool blObjectImplDecRefAndTestIfRefCounted(void* impl, BLObjectInfo info) noexcept {
  if (!info.isRefCountedObject())
    return false;

  size_t rcPrev = blAtomicFetchSub(blObjectImplGetRefCountPtr(impl), 1u);
  size_t rcInit = (info.bits & BL_OBJECT_INFO_RC_INIT_MASK) >> BL_OBJECT_INFO_RC_INIT_SHIFT;

  return rcPrev == rcInit;
}

//! \}

//! \name Object - Internals - Reference Counting and Object Lifetime
//! \{

BL_HIDDEN void BL_CDECL blObjectDestroyExternalDataDummy(void* impl, void* externalData, void* userData) noexcept;

BL_HIDDEN BLResult blObjectDetailDestroyUnknownImpl(void* impl, BLObjectInfo info) noexcept;

template<typename T>
static BL_INLINE BLResult blObjectPrivateAddRefTagged(const T* self) noexcept {
  if (self->_d.info.refCountedFlag())
    blObjectImplAddRef(self->_d.impl);
  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE BLResult blObjectPrivateAddRefUnknown(const T* self) noexcept {
  if (self->_d.isRefCountedObject())
    blObjectImplAddRef(self->_d.impl);
  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE BLResult blObjectPrivateReleaseUnknown(T* self) noexcept {
  void* impl = self->_d.impl;
  BLObjectInfo info = self->_d.info;

  if (blObjectImplDecRefAndTestIfRefCounted(impl, info))
    return blObjectDetailDestroyUnknownImpl(impl, info);

  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE BLResult blObjectPrivateReleaseVirtual(T* self) noexcept {
  BL_ASSERT(self->_d.virtualFlag());

  void* impl = self->_d.impl;
  BLObjectInfo info = self->_d.info;

  if (info.refCountedFlag() && blObjectImplDecRefAndTest(impl, info))
    return blObjectImplFreeVirtual(impl, info);

  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE BLResult blObjectPrivateReplaceVirtual(T* self, const T* other) noexcept {
  BL_ASSERT(self->_d.virtualFlag());
  BL_ASSERT(other->_d.virtualFlag());

  void* impl = self->_d.impl;
  BLObjectInfo info = self->_d.info;

  self->_d = other->_d;

  if (info.refCountedFlag() && blObjectImplDecRefAndTest(impl, info))
    return blObjectImplFreeVirtual(impl, info);

  return BL_SUCCESS;
}

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
  return blObjectPrivateAddRefTagged(dst);
}

template<typename T>
static BL_INLINE BLResult blObjectPrivateInitWeakUnknown(T* dst, const T* src) noexcept {
  dst->_d = src->_d;
  return blObjectPrivateAddRefUnknown(dst);
}

template<typename T>
static BL_INLINE BLResult blObjectPrivateAssignWeakUnknown(T* dst, const T* src) noexcept {
  blObjectPrivateAddRefUnknown(src);
  blObjectPrivateReleaseUnknown(dst);

  dst->_d = src->_d;
  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE BLResult blObjectPrivateAssignWeakVirtual(T* dst, const T* src) noexcept {
  blObjectPrivateAddRefTagged(src);
  blObjectPrivateReleaseVirtual(dst);

  dst->_d = src->_d;
  return BL_SUCCESS;
}

//! \}

//! \name Object - Internals - Binary / Strict Equality
//! \{

//! Tests whether the given objects are binary equivalent.
//!
//! Binary equality is used by some equality implementations as a quick check. The implementation in general tries
//! to keep objects having the same content binary equivalent by clearing the unused SSO storage, but this is an
//! optimization and in general it's not guaranteed that it would be always clean.
static BL_INLINE bool blObjectPrivateBinaryEquals(const BLObjectCore* a, const BLObjectCore* b) noexcept {
  return (a->_d.u64_data[0] == b->_d.u64_data[0]) &
         (a->_d.u64_data[1] == b->_d.u64_data[1]) ;
}

//! \}

//! \name Object - Internals - Expanding Utilities (Containers)
//! \{

static BL_INLINE size_t blObjectGrowImplSizeToPowerOf2(size_t x) noexcept {
  return size_t(1u) << (BLIntOps::bitSizeOf<size_t>() - BLIntOps::clz(x + 1u));
}

static BL_INLINE BLObjectImplSize blObjectAlignImplSize(BLObjectImplSize implSize) noexcept {
  return BLObjectImplSize(BLIntOps::alignUp(implSize.value(), 64u));
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

//! \name Object - Internals - Atomic Content Utilities
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
  return blAtomicFetch(&self->_d.info.bits) > 1u;
}

//! Moves `other` to `self` atomically.
//!
//! The `self` object must have been initialized by `blObjectAtomicContentInit()` or assigned by
//! `blObjectAtomicAssignMove()` - the later case would be detected by the implementation.
//!
//! Returns `true` when the object was sucessfully moved, `false` otherwise.
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

      selfInfo = blAtomicFetch(&self->_d.info.bits);
    }
  }
}

//! \}

//! \}
//! \endcond

#endif // BLEND2D_OBJECT_P_H_INCLUDED
