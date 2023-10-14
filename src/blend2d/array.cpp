// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "object_p.h"
#include "runtime_p.h"
#include "string_p.h"
#include "var_p.h"
#include "support/lookuptable_p.h"
#include "support/memops_p.h"
#include "support/ptrops_p.h"

namespace bl {
namespace ArrayInternal {

// bl::Array - Private - Tables
// ============================

struct ItemSizeGen {
  static constexpr uint8_t value(size_t implType) noexcept {
    return implType == BL_OBJECT_TYPE_ARRAY_OBJECT    ? uint8_t(sizeof(BLObjectCore)) :
           implType == BL_OBJECT_TYPE_ARRAY_INT8      ? uint8_t(1) :
           implType == BL_OBJECT_TYPE_ARRAY_UINT8     ? uint8_t(1) :
           implType == BL_OBJECT_TYPE_ARRAY_INT16     ? uint8_t(2) :
           implType == BL_OBJECT_TYPE_ARRAY_UINT16    ? uint8_t(2) :
           implType == BL_OBJECT_TYPE_ARRAY_INT32     ? uint8_t(4) :
           implType == BL_OBJECT_TYPE_ARRAY_UINT32    ? uint8_t(4) :
           implType == BL_OBJECT_TYPE_ARRAY_INT64     ? uint8_t(8) :
           implType == BL_OBJECT_TYPE_ARRAY_UINT64    ? uint8_t(8) :
           implType == BL_OBJECT_TYPE_ARRAY_FLOAT32   ? uint8_t(4) :
           implType == BL_OBJECT_TYPE_ARRAY_FLOAT64   ? uint8_t(8) :
           implType == BL_OBJECT_TYPE_ARRAY_STRUCT_1  ? uint8_t(1) :
           implType == BL_OBJECT_TYPE_ARRAY_STRUCT_2  ? uint8_t(2) :
           implType == BL_OBJECT_TYPE_ARRAY_STRUCT_3  ? uint8_t(3) :
           implType == BL_OBJECT_TYPE_ARRAY_STRUCT_4  ? uint8_t(4) :
           implType == BL_OBJECT_TYPE_ARRAY_STRUCT_6  ? uint8_t(6) :
           implType == BL_OBJECT_TYPE_ARRAY_STRUCT_8  ? uint8_t(8) :
           implType == BL_OBJECT_TYPE_ARRAY_STRUCT_10 ? uint8_t(10) :
           implType == BL_OBJECT_TYPE_ARRAY_STRUCT_12 ? uint8_t(12) :
           implType == BL_OBJECT_TYPE_ARRAY_STRUCT_16 ? uint8_t(16) :
           implType == BL_OBJECT_TYPE_ARRAY_STRUCT_20 ? uint8_t(20) :
           implType == BL_OBJECT_TYPE_ARRAY_STRUCT_24 ? uint8_t(24) :
           implType == BL_OBJECT_TYPE_ARRAY_STRUCT_32 ? uint8_t(32) : uint8_t(0);
  }
};

struct SSOCapacityGen {
  static constexpr uint8_t value(size_t objectType) noexcept {
    return ItemSizeGen::value(objectType) == 0
      ? uint8_t(0)
      : uint8_t(BLObjectDetail::kStaticDataSize / ItemSizeGen::value(objectType));
  }
};

struct MaximumCapacityGen {
  static constexpr size_t value(size_t objectType) noexcept {
    return ItemSizeGen::value(objectType) == 0
      ? size_t(0)
      : (BL_OBJECT_IMPL_MAX_SIZE - sizeof(BLArrayImpl)) / ItemSizeGen::value(objectType);
  }
};

static constexpr const auto itemSizeTable = makeLookupTable<uint8_t, BL_OBJECT_TYPE_MAX_VALUE + 1, ItemSizeGen>();
static constexpr const auto ssoCapacityTable = makeLookupTable<uint8_t, BL_OBJECT_TYPE_MAX_VALUE + 1, SSOCapacityGen>();
static constexpr const auto maximumCapacityTable = makeLookupTable<size_t, BL_OBJECT_TYPE_MAX_VALUE + 1, MaximumCapacityGen>();

// bl::Array - Private - Commons
// =============================

// Only used as a filler
template<size_t N>
struct UInt32xN { uint32_t data[N]; };

static BL_INLINE constexpr bool isArrayTypeValid(BLObjectType arrayType) noexcept {
  return arrayType >= BL_OBJECT_TYPE_MIN_ARRAY &&
         arrayType <= BL_OBJECT_TYPE_MAX_ARRAY;
}

static BL_INLINE constexpr bool isArrayTypeObjectBased(BLObjectType arrayType) noexcept {
  return arrayType == BL_OBJECT_TYPE_ARRAY_OBJECT;
}

static BL_INLINE size_t itemSizeFromArrayType(BLObjectType arrayType) noexcept {
  return itemSizeTable[arrayType];
}

static BL_INLINE constexpr size_t capacityFromImplSize(BLObjectImplSize implSize, size_t itemSize) noexcept {
  return (implSize.value() - sizeof(BLArrayImpl)) / itemSize;
}

static BL_INLINE constexpr BLObjectImplSize implSizeFromCapacity(size_t capacity, size_t itemSize) noexcept {
  return BLObjectImplSize(sizeof(BLArrayImpl) + capacity * itemSize);
}

static BL_INLINE BLObjectImplSize expandImplSize(BLObjectImplSize implSize) noexcept {
  return blObjectExpandImplSize(implSize);
}

static BLObjectImplSize expandImplSizeWithModifyOp(BLObjectImplSize implSize, BLModifyOp modifyOp) noexcept {
  return blObjectExpandImplSizeWithModifyOp(implSize, modifyOp);
}

// bl::Array - Private - Low-Level Operations
// ==========================================

static BL_NOINLINE void initContentObjects(void* dst_, const void* src_, size_t nBytes) noexcept {
  BL_ASSUME((nBytes % sizeof(BLObjectCore)) == 0);

  BLObjectCore* dst = static_cast<BLObjectCore*>(dst_);
  BLObjectCore* end = PtrOps::offset(dst, nBytes);
  const BLObjectCore* src = static_cast<const BLObjectCore*>(src_);

  while (dst != end) {
    blObjectPrivateInitWeakUnknown(dst, src);

    dst++;
    src++;
  }
}

static BL_INLINE void initContentByType(void* dst, const void* src, size_t nBytes, BLObjectType arrayType) noexcept {
  if (isArrayTypeObjectBased(arrayType))
    initContentObjects(dst, src, nBytes);
  else
    memcpy(dst, src, nBytes);
}

static BL_NOINLINE void assignContentObjects(void* dst_, const void* src_, size_t nBytes) noexcept {
  BL_ASSUME((nBytes % sizeof(BLObjectCore)) == 0);

  BLObjectCore* dst = static_cast<BLObjectCore*>(dst_);
  BLObjectCore* end = PtrOps::offset(dst, nBytes);
  const BLObjectCore* src = static_cast<const BLObjectCore*>(src_);

  while (dst != end) {
    blObjectPrivateAssignWeakUnknown(dst, src);
    dst++;
    src++;
  }
}

static BL_INLINE void assignContentByType(void* dst, const void* src, size_t nBytes, BLObjectType arrayType) noexcept {
  if (isArrayTypeObjectBased(arrayType))
    assignContentObjects(dst, src, nBytes);
  else
    memcpy(dst, src, nBytes);
}

static BL_NOINLINE void releaseContentObjects(void* data, size_t nBytes) noexcept {
  BL_ASSUME((nBytes % sizeof(BLObjectCore)) == 0);

  for (size_t i = 0; i < nBytes; i += sizeof(BLObjectCore))
    ObjectInternal::releaseUnknownInstance(PtrOps::offset<BLObjectCore>(data, i));
}

static BL_INLINE void releaseContentByType(void* data, size_t nBytes, BLObjectType arrayType) noexcept {
  if (isArrayTypeObjectBased(arrayType))
    releaseContentObjects(data, nBytes);
}

static BL_INLINE void fillContentObjects(void* dst, size_t n, const void* src, size_t itemSize) noexcept {
  // NOTE: This is the best we can do. We can increase the reference count of each item in the item / tuple
  // (in case the array stores pair/tuple of objects) and then just copy the content of BLObjectDetail to the
  // destination.
  BLObjectCore* dstObj = static_cast<BLObjectCore*>(dst);
  const BLObjectCore* srcObj = static_cast<const BLObjectCore*>(src);

  size_t tupleSize = itemSize / sizeof(BLObjectCore);
  BL_ASSERT(tupleSize > 0);

  size_t i, j;
  for (j = 0; j < tupleSize; j++) {
    ObjectInternal::retainInstance(&srcObj[j], n);
  }

  for (i = 0; i < n; i++) {
    for (j = 0; j < tupleSize; j++) {
      dstObj->_d = srcObj[j]._d;
      dstObj++;
    }
  }
}

static BL_INLINE void fillContentSimple(void* dst, size_t n, const void* src, size_t itemSize) noexcept {
  switch (itemSize) {
    case  1: MemOps::fillInlineT(static_cast<uint8_t    *>(dst), *static_cast<const uint8_t    *>(src), n); break;
    case  2: MemOps::fillInlineT(static_cast<uint16_t   *>(dst), *static_cast<const uint16_t   *>(src), n); break;
    case  4: MemOps::fillInlineT(static_cast<uint32_t   *>(dst), *static_cast<const uint32_t   *>(src), n); break;
    case  8: MemOps::fillInlineT(static_cast<UInt32xN<2>*>(dst), *static_cast<const UInt32xN<2>*>(src), n); break;
    case 12: MemOps::fillInlineT(static_cast<UInt32xN<3>*>(dst), *static_cast<const UInt32xN<3>*>(src), n); break;
    case 16: MemOps::fillInlineT(static_cast<UInt32xN<4>*>(dst), *static_cast<const UInt32xN<4>*>(src), n); break;

    default: {
      for (size_t i = 0; i < n; i++) {
        memcpy(dst, src, itemSize);
        dst = PtrOps::offset(dst, itemSize);
      }
      break;
    }
  }
}

static BL_INLINE bool equalsContent(const void* a, const void* b, size_t nBytes, BLObjectType arrayType) noexcept {
  if (isArrayTypeObjectBased(arrayType)) {
    const BLObjectCore* aObj = static_cast<const BLObjectCore*>(a);
    const BLObjectCore* bObj = static_cast<const BLObjectCore*>(b);
    const BLObjectCore* aEnd = PtrOps::offset(aObj, nBytes);

    while (aObj != aEnd) {
      if (!blVarEquals(aObj, bObj))
        return false;

      aObj++;
      bObj++;
    }

    return true;
  }
  else {
    return memcmp(a, b, nBytes) == 0;
  }
}

// bl::Array - Private - Alloc & Free Impl
// =======================================

static BL_INLINE BLResult initStatic(BLArrayCore* self, BLObjectType arrayType, size_t size = 0u) noexcept {
  self->_d = blObjectDefaults[arrayType]._d;
  // We know the size is default Impl is always zero, so make this faster than `BLObjectInfo::setAField()`.
  self->_d.info.bits |= uint32_t(size) << BL_OBJECT_INFO_A_SHIFT;

  return BL_SUCCESS;
}

static BL_INLINE BLResult initDynamic(BLArrayCore* self, BLObjectType arrayType, size_t size, BLObjectImplSize implSize) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(arrayType);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLArrayImpl>(self, info, implSize));

  BLArrayImpl* impl = getImpl(self);
  uint8_t* data = PtrOps::offset<uint8_t>(impl, sizeof(BLArrayImpl));
  size_t itemSize = itemSizeTable[arrayType];

  impl->capacity = capacityFromImplSize(implSize, itemSize);
  impl->size = size;
  impl->data = data;
  return BL_SUCCESS;
}

static BL_INLINE BLResult initExternal(
  BLArrayCore* self, BLObjectType arrayType,
  void* externalData, size_t size, size_t capacity, BLDataAccessFlags accessFlags,
  BLDestroyExternalDataFunc destroyFunc, void* userData) noexcept {

  BLObjectImplSize implSize(sizeof(BLArrayImpl));
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(arrayType);

  bool immutable = !(accessFlags & BL_DATA_ACCESS_WRITE);
  BL_PROPAGATE(ObjectInternal::allocImplExternalT<BLArrayImpl>(self, info, implSize, immutable, destroyFunc, userData));

  BLArrayImpl* impl = getImpl(self);
  impl->data = externalData;
  impl->size = size;
  impl->capacity = capacity;
  return BL_SUCCESS;
}

static BL_NOINLINE BLResult initArray(BLArrayCore* self, BLObjectType arrayType, size_t size, size_t capacity, uint8_t** out) noexcept {
  size_t ssoCapacity = ssoCapacityTable[arrayType];
  if (capacity <= ssoCapacity) {
    initStatic(self, arrayType, size);
    *out = self->_d.u8_data;
    return BL_SUCCESS;
  }
  else {
    BL_PROPAGATE(initDynamic(self, arrayType, size, implSizeFromCapacity(capacity, itemSizeFromArrayType(arrayType))));
    *out = getImpl(self)->dataAs<uint8_t>();
    return BL_SUCCESS;
  }
}

static BL_NOINLINE BLResult reallocToDynamic(BLArrayCore* self, BLObjectType arrayType, BLObjectImplSize implSize) noexcept {
  BL_ASSERT(self->_d.rawType() == arrayType);

  size_t size = getSize(self);
  size_t itemSize = itemSizeFromArrayType(arrayType);

  BLArrayCore newO;
  BL_PROPAGATE(initDynamic(&newO, arrayType, size, implSize));

  uint8_t* dst = getImpl(&newO)->dataAs<uint8_t>();
  if (isInstanceDynamicAndMutable(self)) {
    BLArrayImpl* tmpI = getImpl(self);
    memcpy(dst, tmpI->data, size * itemSize);
    tmpI->size = 0;
  }
  else {
    initContentByType(dst, getData(self), size * itemSize, arrayType);
  }

  return replaceInstance(self, &newO);
}

BLResult freeImpl(BLArrayImpl* impl) noexcept {
  if (ObjectInternal::isImplExternal(impl))
    ObjectInternal::callExternalDestroyFunc(impl, impl->data);
  return ObjectInternal::freeImpl(impl);
}

// bl::Array - Private - Typed Operations
// ======================================

template<typename T>
static BL_INLINE BLResult appendTypeT(BLArrayCore* self, T value) noexcept {
  BL_ASSERT(self->_d.isArray());
  BL_ASSERT(itemSizeFromArrayType(self->_d.rawType()) == sizeof(T));

  if (self->_d.sso()) {
    size_t size = self->_d.aField();
    size_t capacity = self->_d.bField();

    BL_ASSERT(size <= capacity);
    if (size == capacity)
      return blArrayAppendItem(self, &value);

    T* data = self->_d.dataAs<T>() + size;
    self->_d.info.setAField(uint32_t(size + 1));

    *data = value;
    return BL_SUCCESS;
  }
  else {
    BLArrayImpl* selfI = getImpl(self);

    size_t size = selfI->size;
    size_t capacity = selfI->capacity;
    size_t immutableMsk = IntOps::bitMaskFromBool<size_t>(!isImplMutable(selfI));

    // Not enough capacity or not mutable - bail to the generic implementation.
    if ((size | immutableMsk) >= capacity)
      return blArrayAppendItem(self, &value);

    T* dst = selfI->dataAs<T>() + size;
    selfI->size = size + 1;

    *dst = value;
    return BL_SUCCESS;
  }
}

template<typename T>
static BL_INLINE BLResult insertTypeT(BLArrayCore* self, size_t index, T value) noexcept {
  BL_ASSERT(self->_d.isArray());
  BL_ASSERT(itemSizeFromArrayType(self->_d.rawType()) == sizeof(T));

  T* dst;
  BL_PROPAGATE(blArrayInsertOp(self, index, 1, (void**)&dst));

  *dst = value;
  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE BLResult replaceTypeT(BLArrayCore* self, size_t index, T value) noexcept {
  BL_ASSERT(self->_d.isArray());
  BL_ASSERT(itemSizeFromArrayType(self->_d.rawType()) == sizeof(T));

  if (!self->_d.sso()) {
    BLArrayImpl* selfI = getImpl(self);
    size_t size = selfI->size;

    if (BL_UNLIKELY(index >= size))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    // Not mutable - don't inline as this is an expensive case anyway.
    if (!isImplMutable(selfI))
      return blArrayReplaceItem(self, index, &value);

    T* data = selfI->dataAs<T>();
    data[index] = value;

    return BL_SUCCESS;
  }
  else {
    size_t size = self->_d.aField();
    if (BL_UNLIKELY(index >= size))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    T* data = self->_d.dataAs<T>();
    data[index] = value;

    return BL_SUCCESS;
  }
}

} // {ArrayInternal}
} // {bl}

// bl::Array - API - Init & Destroy
// ================================

BL_API_IMPL BLResult blArrayInit(BLArrayCore* self, BLObjectType arrayType) noexcept {
  using namespace bl::ArrayInternal;

  BLResult result = BL_SUCCESS;
  if (BL_UNLIKELY(!isArrayTypeValid(arrayType))) {
    arrayType = BL_OBJECT_TYPE_NULL;
    result = blTraceError(BL_ERROR_INVALID_VALUE);
  }

  initStatic(self, arrayType);
  return result;
}

BL_API_IMPL BLResult blArrayInitMove(BLArrayCore* self, BLArrayCore* other) noexcept {
  using namespace bl::ArrayInternal;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isArray());

  self->_d = other->_d;
  return initStatic(other, other->_d.rawType());
}

BL_API_IMPL BLResult blArrayInitWeak(BLArrayCore* self, const BLArrayCore* other) noexcept {
  using namespace bl::ArrayInternal;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isArray());

  self->_d = other->_d;
  return retainInstance(self);
}

BL_API_IMPL BLResult blArrayDestroy(BLArrayCore* self) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  return releaseInstance(self);
}

// bl::Array - API - Reset
// =======================

BL_API_IMPL BLResult blArrayReset(BLArrayCore* self) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  BLObjectType arrayType = self->_d.rawType();
  return replaceInstance(self, static_cast<const BLArrayCore*>(&blObjectDefaults[arrayType]));
}

// bl::Array - API - Accessors
// ===========================

BL_API_IMPL size_t blArrayGetSize(const BLArrayCore* self) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  return getSize(self);
}

BL_API_IMPL size_t blArrayGetCapacity(const BLArrayCore* self) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  return getCapacity(self);
}

BL_API_IMPL size_t blArrayGetItemSize(BLArrayCore* self) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  return itemSizeFromArrayType(self->_d.rawType());
}

BL_API_IMPL const void* blArrayGetData(const BLArrayCore* self) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  return getData(self);
}

// bl::Array - API - Data Manipulation
// ===================================

BL_API_IMPL BLResult blArrayClear(BLArrayCore* self) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  if (self->_d.sso()) {
    size_t size = self->_d.aField();
    if (size) {
      self->_d.clearStaticData();
      self->_d.info.setAField(0);
    }
    return BL_SUCCESS;
  }
  else {
    BLArrayImpl* selfI = getImpl(self);
    BLObjectType arrayType = self->_d.rawType();

    if (!isImplMutable(selfI)) {
      releaseInstance(self);
      return initStatic(self, arrayType);
    }

    size_t size = selfI->size;
    if (!size)
      return BL_SUCCESS;

    size_t itemSize = itemSizeTable[arrayType];
    releaseContentByType(selfI->data, size * itemSize, arrayType);

    selfI->size = 0;
    return BL_SUCCESS;
  }
}

BL_API_IMPL BLResult blArrayShrink(BLArrayCore* self) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  BLObjectType arrayType = self->_d.rawType();

  size_t itemSize = itemSizeFromArrayType(arrayType);
  uint32_t ssoCapacity = ssoCapacityTable[arrayType];

  // 1. Try to move the content to static storage, if possible.
  if (u.size <= ssoCapacity) {
    if (self->_d.sso())
      return BL_SUCCESS;

    BLArrayCore newO;
    newO._d.initStatic(BLObjectInfo::fromTypeWithMarker(arrayType) |
                       BLObjectInfo::fromAbcp(uint32_t(u.size), ssoCapacity));
    memcpy(newO._d.u8_data, u.data, u.size * itemSize);
    return replaceInstance(self, &newO);
  }

  // 2. Don't touch arrays that hold external data.
  if (bl::ObjectInternal::isImplExternal(self->_d.impl))
    return BL_SUCCESS;

  // 2. Only reallocate if we can save at least a single cache line.
  BLObjectImplSize fittingImplSize = implSizeFromCapacity(u.size, itemSize);
  BLObjectImplSize currentImplSize = implSizeFromCapacity(u.capacity, itemSize);

  if (currentImplSize - fittingImplSize >= BL_OBJECT_IMPL_ALIGNMENT)
    return reallocToDynamic(self, arrayType, fittingImplSize);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blArrayResize(BLArrayCore* self, size_t n, const void* fill) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);

  // If `n` is smaller than the current `size` then this is a truncation. We only have
  // to cover the BLObjectCore[] case, which means to destroy all variants beyond `n`.
  if (n <= u.size) {
    if (!isInstanceMutable(self)) {
      if (n == u.size)
        return BL_SUCCESS;

      BLArrayCore newO;
      uint8_t* dst;
      BL_PROPAGATE(initArray(&newO, arrayType, n, n, &dst));

      initContentByType(dst, u.data, n * itemSize, arrayType);
      return replaceInstance(self, &newO);
    }
    else {
      setSize(self, n);
      releaseContentByType(u.data, (u.size - n) * itemSize, arrayType);
      return BL_SUCCESS;
    }
  }

  // `n` becames the number of items to add to the array.
  n -= u.size;

  void* dst;
  BL_PROPAGATE(blArrayModifyOp(self, BL_MODIFY_OP_APPEND_FIT, n, &dst));

  if (!fill)
    memset(dst, 0, n * itemSize);
  else if (isArrayTypeObjectBased(arrayType))
    fillContentObjects(dst, n, fill, itemSize);
  else
    fillContentSimple(dst, n, fill, itemSize);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blArrayReserve(BLArrayCore* self, size_t n) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  size_t immutableMsk = bl::IntOps::bitMaskFromBool<size_t>(!isInstanceMutable(self));

  if ((n | immutableMsk) <= u.capacity)
    return BL_SUCCESS;

  BLObjectType arrayType = self->_d.rawType();
  if (BL_UNLIKELY(n > maximumCapacityTable[arrayType]))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  size_t ssoCapacity = ssoCapacityTable[arrayType];
  size_t itemSize = itemSizeFromArrayType(arrayType);
  n = blMax(n, u.size);

  if (n <= ssoCapacity) {
    BLArrayCore newO;
    initStatic(&newO, arrayType, u.size);

    uint8_t* dst = newO._d.u8_data;
    bl::MemOps::copyForwardInlineT(dst, getImpl(self)->dataAs<uint8_t>(), u.size * itemSize);
    return replaceInstance(self, &newO);
  }
  else {
    return reallocToDynamic(self, arrayType, implSizeFromCapacity(n, itemSize));
  }
}

BL_API_IMPL BLResult blArrayMakeMutable(BLArrayCore* self, void** dataOut) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  if (self->_d.sso()) {
    *dataOut = self->_d.u8_data;
    return BL_SUCCESS;
  }

  BLArrayImpl* selfI = getImpl(self);
  if (isImplMutable(selfI)) {
    *dataOut = selfI->data;
    return BL_SUCCESS;
  }

  BLObjectType arrayType = self->_d.rawType();
  size_t size = selfI->size;
  size_t itemSize = itemSizeFromArrayType(arrayType);

  BLArrayCore tmp = *self;
  uint8_t* dst;

  BL_PROPAGATE(initArray(self, arrayType, size, size, &dst));
  initContentByType(dst, selfI->data, size * itemSize, arrayType);
  releaseInstance(&tmp);

  *dataOut = dst;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blArrayModifyOp(BLArrayCore* self, BLModifyOp op, size_t n, void** dataOut) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeTable[arrayType];

  UnpackedData u;

  size_t index;
  size_t sizeAfter;

  if (self->_d.sso()) {
    u.data = self->_d.u8_data;
    u.size = self->_d.aField();
    u.capacity = self->_d.bField();

    if (blModifyOpIsAssign(op)) {
      index = 0;
      sizeAfter = n;

      if (sizeAfter <= u.capacity) {
        self->_d.info.setAField(uint32_t(sizeAfter));
        self->_d.clearStaticData();

        *dataOut = self->_d.u8_data;
        return BL_SUCCESS;
      }
    }
    else {
      bl::OverflowFlag of{};
      index = u.size;
      sizeAfter = bl::IntOps::addOverflow(u.size, n, &of);

      if (BL_UNLIKELY(of))
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

      if (sizeAfter <= u.capacity) {
        self->_d.info.setAField(uint32_t(sizeAfter));

        *dataOut = self->_d.u8_data + index * itemSize;
        return BL_SUCCESS;
      }
    }
  }
  else {
    BLArrayImpl* selfI = getImpl(self);
    size_t immutableMsk = bl::IntOps::bitMaskFromBool<size_t>(!isImplMutable(selfI));

    u.data = selfI->dataAs<uint8_t>();
    u.size = selfI->size;
    u.capacity = selfI->capacity;

    if (blModifyOpIsAssign(op)) {
      index = 0;
      sizeAfter = n;

      if ((sizeAfter | immutableMsk) <= u.capacity) {
        selfI->size = sizeAfter;
        releaseContentByType(u.data, u.size * itemSize, arrayType);

        *dataOut = u.data;
        return BL_SUCCESS;
      }
    }
    else {
      bl::OverflowFlag of{};
      index = u.size;
      sizeAfter = bl::IntOps::addOverflow(u.size, n, &of);

      if (BL_UNLIKELY(of))
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

      if ((sizeAfter | immutableMsk) <= u.capacity) {
        selfI->size = sizeAfter;

        *dataOut = u.data + index * itemSize;
        return BL_SUCCESS;
      }
    }
  }

  // The container is either immutable or doesn't have the capacity required.
  BLArrayCore newO;
  size_t ssoCapacity = ssoCapacityTable[arrayType];

  if (sizeAfter <= ssoCapacity) {
    // The new content fits in static storage, which implies that the current content must be dynamic.
    BL_ASSERT(!self->_d.sso());

    newO._d.initStatic(BLObjectInfo::fromTypeWithMarker(arrayType) |
                       BLObjectInfo::fromAbcp(uint32_t(sizeAfter), uint32_t(ssoCapacity)));
    bl::MemOps::copyForwardInlineT(newO._d.u8_data, u.data, index * itemSize);

    *dataOut = self->_d.u8_data + index * itemSize;
    return replaceInstance(self, &newO);
  }
  else {
    BLObjectImplSize implSize = expandImplSizeWithModifyOp(implSizeFromCapacity(sizeAfter, itemSize), op);
    BL_PROPAGATE(initDynamic(&newO, arrayType, sizeAfter, implSize));

    uint8_t* dst = getImpl(&newO)->dataAs<uint8_t>();
    if (self->_d.isDynamicObject() && bl::ObjectInternal::isImplMutable(self->_d.impl)) {
      // Use memcpy() instead of weak copying if the original data is gonna be destroyed.
      memcpy(dst, u.data, index * itemSize);
      // We have to patch the source Impl as releaseInstance() as we have moved the content.
      getImpl(self)->size = 0;
    }
    else {
      initContentByType(dst, u.data, index * itemSize, arrayType);
    }

    *dataOut = dst + index * itemSize;
    return replaceInstance(self, &newO);
  }
}

BL_API_IMPL BLResult blArrayInsertOp(BLArrayCore* self, size_t index, size_t n, void** dataOut) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);

  size_t sizeAfter = bl::IntOps::uaddSaturate(u.size, n);
  size_t immutableMsk = bl::IntOps::bitMaskFromBool<size_t>(!isInstanceMutable(self));

  if ((sizeAfter | immutableMsk) > u.capacity) {
    if (BL_UNLIKELY(sizeAfter > maximumCapacityTable[arrayType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    BLArrayCore tmp = *self;

    uint8_t* dst = nullptr;
    const uint8_t* src = getData<uint8_t>(&tmp);

    size_t ssoCapacity = ssoCapacityTable[arrayType];
    if (sizeAfter <= ssoCapacity) {
      initStatic(self, arrayType, sizeAfter);
      dst = self->_d.u8_data;
    }
    else {
      BLObjectImplSize implSize = expandImplSize(implSizeFromCapacity(sizeAfter, itemSize));
      *dataOut = nullptr;

      BL_PROPAGATE(initDynamic(self, arrayType, sizeAfter, implSize));
      dst = getImpl(self)->dataAs<uint8_t>();
    }

    if (!immutableMsk) {
      // Move if `tmp` will be destroyed.
      memcpy(dst, src, index * itemSize);
      memcpy(dst + (index + n) * itemSize, src + index * itemSize, (u.size - index) * itemSize);
      setSize(&tmp, 0);
    }
    else {
      initContentByType(dst, src, index * itemSize, arrayType);
      initContentByType(dst + (index + n) * itemSize, src + index * itemSize, (u.size - index) * itemSize, arrayType);
    }

    *dataOut = dst + index * itemSize;
    return releaseInstance(&tmp);
  }
  else {
    setSize(self, sizeAfter);
    memmove(u.data + (index + n) * itemSize, u.data + index * itemSize, (u.size - index) * itemSize);

    *dataOut = u.data + index * itemSize;
    return BL_SUCCESS;
  }
}

// bl::Array - API - Data Manipulation - Assignment
// ================================================

BL_API_IMPL BLResult blArrayAssignMove(BLArrayCore* self, BLArrayCore* other) noexcept {
  using namespace bl::ArrayInternal;

  BL_ASSERT(self->_d.isArray());
  BL_ASSERT(other->_d.isArray());
  BL_ASSERT(self->_d.rawType() == other->_d.rawType());

  BLObjectType arrayType = self->_d.rawType();
  BLArrayCore tmp = *other;

  initStatic(other, arrayType);
  return replaceInstance(self, &tmp);
}

BL_API_IMPL BLResult blArrayAssignWeak(BLArrayCore* self, const BLArrayCore* other) noexcept {
  using namespace bl::ArrayInternal;

  BL_ASSERT(self->_d.isArray());
  BL_ASSERT(other->_d.isArray());
  BL_ASSERT(self->_d.rawType() == other->_d.rawType());

  retainInstance(other);
  return replaceInstance(self, other);
}

BL_API_IMPL BLResult blArrayAssignDeep(BLArrayCore* self, const BLArrayCore* other) noexcept {
  using namespace bl::ArrayInternal;

  BL_ASSERT(self->_d.isArray());
  BL_ASSERT(other->_d.isArray());
  BL_ASSERT(self->_d.rawType() == other->_d.rawType());

  return blArrayAssignData(self, getData(other), getSize(other));
}

BL_API_IMPL BLResult blArrayAssignData(BLArrayCore* self, const void* items, size_t n) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);
  size_t immutableMsk = bl::IntOps::bitMaskFromBool<size_t>(!isInstanceMutable(self));

  if ((n | immutableMsk) > u.capacity) {
    if (BL_UNLIKELY(n > maximumCapacityTable[arrayType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    BLObjectImplSize implSize = implSizeFromCapacity(u.size, itemSize);
    BLArrayCore newO;
    BL_PROPAGATE(initDynamic(&newO, arrayType, n, implSize));

    uint8_t* dst = getImpl(&newO)->dataAs<uint8_t>();
    initContentByType(dst, items, n * itemSize, arrayType);
    return replaceInstance(self, &newO);
  }

  if (!n)
    return blArrayClear(self);

  setSize(self, n);

  if (isArrayTypeObjectBased(arrayType)) {
    size_t replaceSize = blMin(u.size, n);
    const uint8_t* src = static_cast<const uint8_t*>(items);

    assignContentObjects(u.data, src, replaceSize * itemSize);
    releaseContentObjects(u.data + replaceSize * itemSize, (u.size - replaceSize) * itemSize);

    return BL_SUCCESS;
  }
  else {
    // Memory move is required in case of overlap between `data` and `items`.
    memmove(u.data, items, n * itemSize);
    return BL_SUCCESS;
  }
}

BL_API_IMPL BLResult blArrayAssignExternalData(BLArrayCore* self, void* externalData, size_t size, size_t capacity, BLDataAccessFlags accessFlags, BLDestroyExternalDataFunc destroyFunc, void* userData) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);

  bl::OverflowFlag of{};
  bl::IntOps::mulOverflow(capacity, itemSize, &of);

  if (BL_UNLIKELY(!capacity || capacity < size || !blDataAccessFlagsIsValid(accessFlags) || of))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLArrayCore newO;
  BL_PROPAGATE(initExternal(&newO, arrayType, externalData, size, capacity, accessFlags, destroyFunc, userData));

  return replaceInstance(self, &newO);
}

// bl::Array - API - Data Manipulation - Append
// ============================================

BL_API_IMPL BLResult blArrayAppendU8(BLArrayCore* self, uint8_t value) noexcept { return bl::ArrayInternal::appendTypeT<uint8_t>(self, value); }
BL_API_IMPL BLResult blArrayAppendU16(BLArrayCore* self, uint16_t value) noexcept { return bl::ArrayInternal::appendTypeT<uint16_t>(self, value); }
BL_API_IMPL BLResult blArrayAppendU32(BLArrayCore* self, uint32_t value) noexcept { return bl::ArrayInternal::appendTypeT<uint32_t>(self, value); }
BL_API_IMPL BLResult blArrayAppendU64(BLArrayCore* self, uint64_t value) noexcept { return bl::ArrayInternal::appendTypeT<uint64_t>(self, value); }
BL_API_IMPL BLResult blArrayAppendF32(BLArrayCore* self, float value) noexcept { return bl::ArrayInternal::appendTypeT<float>(self, value); }
BL_API_IMPL BLResult blArrayAppendF64(BLArrayCore* self, double value) noexcept { return bl::ArrayInternal::appendTypeT<double>(self, value); }

BL_API_IMPL BLResult blArrayAppendItem(BLArrayCore* self, const void* item) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);
  size_t immutableMsk = bl::IntOps::bitMaskFromBool<size_t>(!isInstanceMutable(self));

  if (BL_UNLIKELY((u.size | immutableMsk) >= u.capacity)) {
    if (BL_UNLIKELY(u.size >= maximumCapacityTable[arrayType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    BLArrayCore newO;
    BLObjectImplSize implSize = expandImplSize(implSizeFromCapacity(u.size + 1u, itemSize));
    BL_PROPAGATE(initDynamic(&newO, arrayType, u.size + 1, implSize));

    // Copy the existing data to a new place / move if the data will be destroyed.
    uint8_t* dst = getImpl(&newO)->dataAs<uint8_t>();
    if (!immutableMsk) {
      setSize(self, 0);
      memcpy(dst, u.data, u.size * itemSize);
    }
    else {
      initContentByType(dst, u.data, u.size * itemSize, arrayType);
    }

    initContentByType(dst + u.size * itemSize, item, itemSize, arrayType);
    return replaceInstance(self, &newO);
  }
  else {
    initContentByType(u.data + u.size * itemSize, item, itemSize, arrayType);
    setSize(self, u.size + 1);

    return BL_SUCCESS;
  }
}

BL_API_IMPL BLResult blArrayAppendData(BLArrayCore* self, const void* items, size_t n) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);
  size_t immutableMsk = bl::IntOps::bitMaskFromBool<size_t>(!isInstanceMutable(self));

  size_t sizeAfter = bl::IntOps::uaddSaturate(u.size, n);

  if (BL_UNLIKELY((sizeAfter | immutableMsk) > u.capacity)) {
    if (BL_UNLIKELY(sizeAfter > maximumCapacityTable[arrayType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    BLArrayCore newO;
    BLObjectImplSize implSize = expandImplSize(implSizeFromCapacity(sizeAfter, itemSize));
    BL_PROPAGATE(initDynamic(&newO, arrayType, sizeAfter, implSize));

    // Copy the existing data to a new place / move if the data will be destroyed.
    uint8_t* dst = getImpl(&newO)->dataAs<uint8_t>();
    if (!immutableMsk) {
      setSize(self, 0);
      memcpy(dst, u.data, u.size * itemSize);
    }
    else {
      initContentByType(dst, u.data, u.size * itemSize, arrayType);
    }

    initContentByType(dst + u.size * itemSize, items, n * itemSize, arrayType);
    return replaceInstance(self, &newO);
  }
  else {
    initContentByType(u.data + u.size * itemSize, items, n * itemSize, arrayType);
    setSize(self, sizeAfter);

    return BL_SUCCESS;
  }
}

// bl::Array - API - Data Manipulation - Insert
// ============================================

BL_API_IMPL BLResult blArrayInsertU8(BLArrayCore* self, size_t index, uint8_t value) noexcept { return bl::ArrayInternal::insertTypeT<uint8_t>(self, index, value); }
BL_API_IMPL BLResult blArrayInsertU16(BLArrayCore* self, size_t index, uint16_t value) noexcept { return bl::ArrayInternal::insertTypeT<uint16_t>(self, index, value); }
BL_API_IMPL BLResult blArrayInsertU32(BLArrayCore* self, size_t index, uint32_t value) noexcept { return bl::ArrayInternal::insertTypeT<uint32_t>(self, index, value); }
BL_API_IMPL BLResult blArrayInsertU64(BLArrayCore* self, size_t index, uint64_t value) noexcept { return bl::ArrayInternal::insertTypeT<uint64_t>(self, index, value); }
BL_API_IMPL BLResult blArrayInsertF32(BLArrayCore* self, size_t index, float value) noexcept { return bl::ArrayInternal::insertTypeT<float>(self, index, value); }
BL_API_IMPL BLResult blArrayInsertF64(BLArrayCore* self, size_t index, double value) noexcept { return bl::ArrayInternal::insertTypeT<double>(self, index, value); }
BL_API_IMPL BLResult blArrayInsertItem(BLArrayCore* self, size_t index, const void* item) noexcept { return blArrayInsertData(self, index, item, 1); }

BL_API_IMPL BLResult blArrayInsertData(BLArrayCore* self, size_t index, const void* items, size_t n) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);
  size_t immutableMsk = bl::IntOps::bitMaskFromBool<size_t>(!isInstanceMutable(self));

  size_t endIndex = index + n;
  size_t sizeAfter = bl::IntOps::uaddSaturate(u.size, n);

  if ((sizeAfter | immutableMsk) > u.capacity) {
    if (BL_UNLIKELY(sizeAfter > maximumCapacityTable[arrayType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    BLObjectImplSize implSize = expandImplSize(implSizeFromCapacity(sizeAfter, itemSize));
    BLArrayCore newO;
    BL_PROPAGATE(initDynamic(&newO, arrayType, sizeAfter, implSize));

    uint8_t* dst = getImpl(&newO)->dataAs<uint8_t>();
    if (!immutableMsk) {
      setSize(self, 0);
      memcpy(dst, u.data, index * itemSize);
      memcpy(dst + endIndex * itemSize, u.data + index * itemSize, (u.size - index) * itemSize);
    }
    else {
      initContentByType(dst, u.data, index * itemSize, arrayType);
      initContentByType(dst + endIndex * itemSize, u.data + index * itemSize, (u.size - index) * itemSize, arrayType);
    }

    initContentByType(dst + index * itemSize, items, n * itemSize, arrayType);
    return replaceInstance(self, &newO);
  }
  else {
    size_t nInBytes = n * itemSize;

    uint8_t* dst = u.data;
    uint8_t* dstEnd = dst + u.size * itemSize;
    const uint8_t* src = static_cast<const uint8_t*>(items);

    // The destination would point into the first byte that will be modified. So for example if the
    // data is `[ABCDEF]` and we are inserting at index 1 then the `dst` would point to `[BCDEF]`.
    dst += index * itemSize;
    dstEnd += nInBytes;

    // Move the memory in-place making space for items to insert. For example if the destination points
    // to [ABCDEF] and we want to insert 4 items we would get [____ABCDEF].
    memmove(dst + nInBytes, dst, (u.size - index) * itemSize);

    // Split the [src:srcEnd] into LEAD and TRAIL slices and shift TRAIL slice in a way to cancel the
    // `memmove()` if `src` overlaps `dst`. In practice if there is an overlap the [src:srcEnd] source
    // should be within [dst:dstEnd] as it doesn't make sense to insert something which is outside of
    // the current valid area.
    //
    // This illustrates how the input is divided into leading and traling data.
    //
    //   BCDEFGH    <- Insert This
    // [abcdefghi]
    //      ^       <- Here
    //
    // [abcd_______efgh]
    //              <- memmove()
    //
    //      |-|     <- Copy leading data
    // [abcdBCD____efgh]
    //
    //         |--| <- Copy shifted trailing data.
    // [abcdBCDEFGHefgh]

    // Leading area precedes `dst` - nothing changed in here and if this is the whole area then there
    // was no overlap that we would have to deal with.
    size_t nLeadBytes = 0;
    if (src < dst) {
      nLeadBytes = blMin<size_t>((size_t)(dst - src), nInBytes);
      initContentByType(dst, src, nLeadBytes, arrayType);

      dst += nLeadBytes;
      src += nLeadBytes;
    }

    // Trailing area - we either shift none or all of it.
    if (src < dstEnd)
      src += nInBytes; // Shift source in case of overlap.

    initContentByType(dst, src, nInBytes - nLeadBytes, arrayType);
    setSize(self, sizeAfter);

    return BL_SUCCESS;
  }
}

// bl::Array - API - Data Manipulation - Replace
// =============================================

BL_API_IMPL BLResult blArrayReplaceU8(BLArrayCore* self, size_t index, uint8_t value) noexcept { return bl::ArrayInternal::replaceTypeT<uint8_t>(self, index, value); }
BL_API_IMPL BLResult blArrayReplaceU16(BLArrayCore* self, size_t index, uint16_t value) noexcept { return bl::ArrayInternal::replaceTypeT<uint16_t>(self, index, value); }
BL_API_IMPL BLResult blArrayReplaceU32(BLArrayCore* self, size_t index, uint32_t value) noexcept { return bl::ArrayInternal::replaceTypeT<uint32_t>(self, index, value); }
BL_API_IMPL BLResult blArrayReplaceU64(BLArrayCore* self, size_t index, uint64_t value) noexcept { return bl::ArrayInternal::replaceTypeT<uint64_t>(self, index, value); }
BL_API_IMPL BLResult blArrayReplaceF32(BLArrayCore* self, size_t index, float value) noexcept { return bl::ArrayInternal::replaceTypeT<float>(self, index, value); }
BL_API_IMPL BLResult blArrayReplaceF64(BLArrayCore* self, size_t index, double value) noexcept { return bl::ArrayInternal::replaceTypeT<double>(self, index, value); }

BL_API_IMPL BLResult blArrayReplaceItem(BLArrayCore* self, size_t index, const void* item) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  if (BL_UNLIKELY(index >= u.size))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);

  if (!isInstanceMutable(self)) {
    BLArrayCore newO;
    BLObjectImplSize implSize = implSizeFromCapacity(u.size, itemSize);
    BL_PROPAGATE(initDynamic(&newO, arrayType, u.size, implSize));

    uint8_t* dst = getImpl(&newO)->dataAs<uint8_t>();
    const uint8_t* src = u.data;

    initContentByType(dst, src, index * itemSize, arrayType);
    dst += index * itemSize;
    src += index * itemSize;

    initContentByType(dst, item, itemSize, arrayType);
    dst += itemSize;
    src += itemSize;

    initContentByType(dst, src, (u.size - index - 1) * itemSize, arrayType);
    return replaceInstance(self, &newO);
  }
  else {
    assignContentByType(u.data + index * itemSize, item, itemSize, arrayType);
    return BL_SUCCESS;
  }
}

BL_API_IMPL BLResult blArrayReplaceData(BLArrayCore* self, size_t rStart, size_t rEnd, const void* items, size_t n) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  size_t end = blMin(rEnd, u.size);
  size_t index = blMin(rStart, end);
  size_t rangeSize = end - index;

  if (!rangeSize)
    return blArrayInsertData(self, index, items, n);

  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);

  size_t tailSize = u.size - end;
  size_t sizeAfter = u.size - rangeSize + n;

  if (isInstanceMutable(self)) {
    // 0           |<-Start   End->|          | <- Size
    // ^***********^***************^**********^
    // | Unchanged |  Replacement  | TailSize |
    //
    // <  Less     |+++++++| <- MidEnd
    // == Equal    |+++++++++++++++| <- MidEnd
    // >  Greater  |++++++++++++++++++++++| <- MidEnd
    const uint8_t* itemsPtr = static_cast<const uint8_t*>(items);
    const uint8_t* itemsEnd = itemsPtr + itemSize * n;

    if (BL_LIKELY(itemsPtr >= u.data + u.size * n || itemsEnd <= u.data)) {
      // Non-overlaping case (the expected one).
      if (rangeSize == n) {
        assignContentByType(u.data + index * itemSize, items, n * itemSize, arrayType);
      }
      else {
        releaseContentByType(u.data + index * itemSize, rangeSize * itemSize, arrayType);
        memmove(u.data + (index + rangeSize) * itemSize, u.data + end * itemSize, tailSize * itemSize);
        initContentByType(u.data + index * itemSize, items, n * itemSize, arrayType);
        setSize(self, sizeAfter);
      }
      return BL_SUCCESS;
    }
  }

  // Array is either immmutable or its data overlaps with `items`.
  BLArrayCore newO;
  uint8_t* dst;
  BL_PROPAGATE(initArray(&newO, arrayType, sizeAfter, sizeAfter, &dst));

  const uint8_t* src = u.data;
  initContentByType(dst, src, index * itemSize, arrayType);

  dst += index * itemSize;
  src += (index + rangeSize) * itemSize;

  initContentByType(dst, items, n * itemSize, arrayType);
  dst += n * itemSize;

  initContentByType(dst, src, tailSize * itemSize, arrayType);
  return replaceInstance(self, &newO);
}

// bl::Array - API - Data Manipulation - Remove
// ============================================

BL_API_IMPL BLResult blArrayRemoveIndex(BLArrayCore* self, size_t index) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  return blArrayRemoveRange(self, index, index + 1);
}

BL_API_IMPL BLResult blArrayRemoveRange(BLArrayCore* self, size_t rStart, size_t rEnd) noexcept {
  using namespace bl::ArrayInternal;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  size_t end = blMin(rEnd, u.size);
  size_t index = blMin(rStart, end);

  size_t n = end - index;
  size_t sizeAfter = u.size - n;

  if (!n)
    return BL_SUCCESS;

  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);

  if (self->_d.sso()) {
    size_t ssoCapacity = self->_d.bField();

    bl::MemOps::copySmall(u.data + index * itemSize, u.data + (index + n) * itemSize, (u.size - end) * itemSize);
    bl::MemOps::fillSmallT(u.data + sizeAfter * itemSize, uint8_t(0), (ssoCapacity - sizeAfter) * itemSize);

    self->_d.info.setAField(uint32_t(sizeAfter));
    return BL_SUCCESS;
  }

  BLArrayImpl* selfI = getImpl(self);
  if (!isImplMutable(selfI)) {
    BLArrayCore newO;
    uint8_t* dst;
    BL_PROPAGATE(initArray(&newO, arrayType, sizeAfter, sizeAfter, &dst));

    initContentByType(dst, u.data, index * itemSize, arrayType);
    initContentByType(dst + index * itemSize, u.data + end * itemSize, (u.size - end) * itemSize, arrayType);

    return replaceInstance(self, &newO);
  }
  else {
    uint8_t* data = getData<uint8_t>(self) + index * itemSize;

    releaseContentByType(data, n * itemSize, arrayType);
    memmove(data, data + n * itemSize, (u.size - end) * itemSize);

    selfI->size = sizeAfter;
    return BL_SUCCESS;
  }
}

// bl::Array - API - Equality & Comparison
// =======================================

BL_API_IMPL bool blArrayEquals(const BLArrayCore* a, const BLArrayCore* b) noexcept {
  using namespace bl::ArrayInternal;

  BL_ASSERT(a->_d.isArray());
  BL_ASSERT(b->_d.isArray());

  if (a->_d == b->_d)
    return true;

  // NOTE: This should never happen. Mixing array types is not supported.
  BLObjectType arrayType = a->_d.rawType();
  BL_ASSERT(arrayType == b->_d.rawType());

  // However, if it happens, we want the comparison to return false in release builds.
  if (arrayType != b->_d.rawType())
    return false;

  UnpackedData au = unpack(a);
  UnpackedData bu = unpack(b);

  if (au.size != bu.size)
    return false;

  size_t itemSize = itemSizeFromArrayType(arrayType);
  return equalsContent(au.data, bu.data, au.size * itemSize, arrayType);
}

// bl::Array - Runtime Registration
// ================================

void blArrayRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  for (uint32_t objectType = BL_OBJECT_TYPE_MIN_ARRAY; objectType <= BL_OBJECT_TYPE_MAX_ARRAY; objectType++) {
    blObjectDefaults[objectType]._d.initStatic(
      BLObjectInfo::fromTypeWithMarker(BLObjectType(objectType)) |
      BLObjectInfo::fromAbcp(0, bl::ArrayInternal::ssoCapacityTable[objectType]));
  }
}
