// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "object_p.h"
#include "runtime_p.h"
#include "string_p.h"
#include "tables_p.h"
#include "var_p.h"
#include "support/memops_p.h"
#include "support/ptrops_p.h"

namespace BLArrayPrivate {

// BLArray - Private - Tables
// ==========================

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

static constexpr const auto itemSizeTable = blMakeLookupTable<uint8_t, BL_OBJECT_TYPE_MAX_VALUE + 1, ItemSizeGen>();
static constexpr const auto ssoCapacityTable = blMakeLookupTable<uint8_t, BL_OBJECT_TYPE_MAX_VALUE + 1, SSOCapacityGen>();
static constexpr const auto maximumCapacityTable = blMakeLookupTable<size_t, BL_OBJECT_TYPE_MAX_VALUE + 1, MaximumCapacityGen>();

// BLArray - Private - Commons
// ===========================

// Only used as a filler
template<size_t N>
struct UInt32xN { uint32_t data[N]; };

static BL_INLINE constexpr bool isArrayTypeValid(BLObjectType arrayType) noexcept {
  return arrayType >= BL_OBJECT_TYPE_ARRAY_FIRST &&
         arrayType <= BL_OBJECT_TYPE_ARRAY_LAST;
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

// BLArray - Private - Low-Level Operations
// ========================================

static BL_NOINLINE void initContentObjects(void* dst_, const void* src_, size_t nBytes) noexcept {
  BL_ASSUME((nBytes % sizeof(BLObjectCore)) == 0);

  BLObjectCore* dst = static_cast<BLObjectCore*>(dst_);
  BLObjectCore* end = BLPtrOps::offset(dst, nBytes);
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
  BLObjectCore* end = BLPtrOps::offset(dst, nBytes);
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
    blObjectPrivateReleaseUnknown(BLPtrOps::offset<BLObjectCore>(data, i));
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
    const BLObjectCore& obj = srcObj[j];
    if (obj._d.isRefCountedObject())
      blObjectImplAddRef(obj._d.impl, n);
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
    case  1: BLMemOps::fillInlineT(static_cast<uint8_t    *>(dst), *static_cast<const uint8_t    *>(src), n); break;
    case  2: BLMemOps::fillInlineT(static_cast<uint16_t   *>(dst), *static_cast<const uint16_t   *>(src), n); break;
    case  4: BLMemOps::fillInlineT(static_cast<uint32_t   *>(dst), *static_cast<const uint32_t   *>(src), n); break;
    case  8: BLMemOps::fillInlineT(static_cast<UInt32xN<2>*>(dst), *static_cast<const UInt32xN<2>*>(src), n); break;
    case 12: BLMemOps::fillInlineT(static_cast<UInt32xN<3>*>(dst), *static_cast<const UInt32xN<3>*>(src), n); break;
    case 16: BLMemOps::fillInlineT(static_cast<UInt32xN<4>*>(dst), *static_cast<const UInt32xN<4>*>(src), n); break;

    default: {
      for (size_t i = 0; i < n; i++) {
        memcpy(dst, src, itemSize);
        dst = BLPtrOps::offset(dst, itemSize);
      }
      break;
    }
  }
}

static BL_INLINE bool equalsContent(const void* a, const void* b, size_t nBytes, BLObjectType arrayType) noexcept {
  if (isArrayTypeObjectBased(arrayType)) {
    const BLObjectCore* aObj = static_cast<const BLObjectCore*>(a);
    const BLObjectCore* bObj = static_cast<const BLObjectCore*>(b);
    const BLObjectCore* aEnd = BLPtrOps::offset(aObj, nBytes);

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

// BLArray - Private - Alloc & Free Impl
// =====================================

static BL_INLINE uint8_t* initStatic(BLArrayCore* self, BLObjectType arrayType, size_t size = 0u) noexcept {
  self->_d = blObjectDefaults[arrayType]._d;
  // We know the size is default Impl is always zero, so make this faster than `BLObjectInfo::setAField()`.
  self->_d.info.bits |= uint32_t(size) << BL_OBJECT_INFO_A_SHIFT;
  return self->_d.u8_data;
}

static BL_INLINE uint8_t* initDynamic(BLArrayCore* self, BLObjectType arrayType, size_t size, BLObjectImplSize implSize) noexcept {
  BLArrayImpl* impl = blObjectDetailAllocImplT<BLArrayImpl>(self, BLObjectInfo::packType(arrayType), implSize, &implSize);

  if(BL_UNLIKELY(!impl))
    return nullptr;

  uint8_t* data = BLPtrOps::offset<uint8_t>(impl, sizeof(BLArrayImpl));
  size_t itemSize = itemSizeTable[arrayType];

  impl->capacity = capacityFromImplSize(implSize, itemSize);
  impl->size = size;
  impl->data = data;

  return data;
}

static BL_INLINE BLResult initExternal(
  BLArrayCore* self, BLObjectType arrayType,
  void* externalData, size_t size, size_t capacity, BLDataAccessFlags accessFlags,
  BLDestroyExternalDataFunc destroyFunc, void* userData) noexcept {

  BLObjectImplSize implSize(sizeof(BLArrayImpl));
  BLObjectInfo info = BLObjectInfo::packType(arrayType);

  if (!(accessFlags & BL_DATA_ACCESS_WRITE))
    info |= BL_OBJECT_INFO_IMMUTABLE_FLAG;

  BLObjectExternalInfo* externalInfo;
  void* externalOptData;

  BLArrayImpl* impl = blObjectDetailAllocImplExternalT<BLArrayImpl>(self, info, implSize, &externalInfo, &externalOptData);
  if (BL_UNLIKELY(!impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  externalInfo->destroyFunc = destroyFunc ? destroyFunc : blObjectDestroyExternalDataDummy;
  externalInfo->userData = userData;

  impl->data = externalData;
  impl->size = size;
  impl->capacity = capacity;

  return BL_SUCCESS;
}

static BL_NOINLINE uint8_t* initArray(BLArrayCore* self, BLObjectType arrayType, size_t size, size_t capacity) noexcept {
  size_t ssoCapacity = ssoCapacityTable[arrayType];
  if (capacity <= ssoCapacity)
    return initStatic(self, arrayType, size);
  else
    return initDynamic(self, arrayType, size, implSizeFromCapacity(capacity, itemSizeFromArrayType(arrayType)));
}

static BL_NOINLINE BLResult reallocToDynamic(BLArrayCore* self, BLObjectType arrayType, BLObjectImplSize implSize) noexcept {
  BL_ASSERT(self->_d.rawType() == arrayType);

  size_t size = getSize(self);
  size_t itemSize = itemSizeFromArrayType(arrayType);

  BLArrayCore newO;
  uint8_t* dst = initDynamic(&newO, arrayType, size, implSize);

  if (BL_UNLIKELY(!dst))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  if (self->_d.refCountedFlag() && blObjectImplGetRefCount(self->_d.impl) == 1) {
    BLArrayImpl* tmpI = getImpl(self);
    memcpy(dst, tmpI->data, size * itemSize);
    tmpI->size = 0;
  }
  else {
    initContentByType(dst, getData(self), size * itemSize, arrayType);
  }

  return replaceInstance(self, &newO);
}

BLResult freeImpl(BLArrayImpl* impl, BLObjectInfo info) noexcept {
  if (info.xFlag())
    blObjectDetailCallExternalDestroyFunc(impl, info, BLObjectImplSize(sizeof(BLArrayImpl)), impl->data);

  return blObjectImplFreeInline(impl, info);
}

// BLArray - Private - Typed Operations
// ====================================

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
    size_t immutableMsk = BLIntOps::bitMaskFromBool<size_t>(!isMutable(self));

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
    if (!isMutable(self))
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

} // {BLArrayPrivate}

// BLArray - API - Init & Destroy
// ==============================

BL_API_IMPL BLResult blArrayInit(BLArrayCore* self, BLObjectType arrayType) noexcept {
  using namespace BLArrayPrivate;

  BLResult result = BL_SUCCESS;
  if (BL_UNLIKELY(!isArrayTypeValid(arrayType))) {
    arrayType = BL_OBJECT_TYPE_NULL;
    result = blTraceError(BL_ERROR_INVALID_VALUE);
  }

  initStatic(self, arrayType);
  return result;
}

BL_API_IMPL BLResult blArrayInitMove(BLArrayCore* self, BLArrayCore* other) noexcept {
  using namespace BLArrayPrivate;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isArray());

  self->_d = other->_d;
  initStatic(other, other->_d.rawType());

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blArrayInitWeak(BLArrayCore* self, const BLArrayCore* other) noexcept {
  using namespace BLArrayPrivate;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isArray());

  return blObjectPrivateInitWeakTagged(self, other);
}

BL_API_IMPL BLResult blArrayDestroy(BLArrayCore* self) noexcept {
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  return releaseInstance(self);
}

// BLArray - API - Reset
// =====================

BL_API_IMPL BLResult blArrayReset(BLArrayCore* self) noexcept {
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  BLObjectType arrayType = self->_d.rawType();
  return replaceInstance(self, static_cast<const BLArrayCore*>(&blObjectDefaults[arrayType]));
}

// BLArray - API - Accessors
// =========================

BL_API_IMPL size_t blArrayGetSize(const BLArrayCore* self) noexcept {
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  return getSize(self);
}

BL_API_IMPL size_t blArrayGetCapacity(const BLArrayCore* self) noexcept {
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  return getCapacity(self);
}

BL_API_IMPL size_t blArrayGetItemSize(BLArrayCore* self) noexcept {
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  return itemSizeFromArrayType(self->_d.rawType());
}

BL_API_IMPL const void* blArrayGetData(const BLArrayCore* self) noexcept {
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  return getData(self);
}

// BLArray - API - Data Manipulation
// =================================

BL_API_IMPL BLResult blArrayClear(BLArrayCore* self) noexcept {
  using namespace BLArrayPrivate;
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

    if (!isMutable(self)) {
      releaseInstance(self);
      initStatic(self, arrayType);
      return BL_SUCCESS;
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
  using namespace BLArrayPrivate;
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
    newO._d.initStatic(arrayType, BLObjectInfo::packFields(uint32_t(u.size), ssoCapacity));
    memcpy(newO._d.u8_data, u.data, u.size * itemSize);
    return replaceInstance(self, &newO);
  }

  // 2. Don't touch arrays that hold external data.
  if (self->_d.xFlag())
    return BL_SUCCESS;

  // 2. Only reallocate if we can save at least a single cache line.
  BLObjectImplSize fittingImplSize = implSizeFromCapacity(u.size, itemSize);
  BLObjectImplSize currentImplSize = implSizeFromCapacity(u.capacity, itemSize);

  if (currentImplSize - fittingImplSize >= BL_OBJECT_IMPL_ALIGNMENT)
    return reallocToDynamic(self, arrayType, fittingImplSize);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blArrayResize(BLArrayCore* self, size_t n, const void* fill) noexcept {
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);

  // If `n` is smaller than the current `size` then this is a truncation. We only have
  // to  cover the BLObjectCore[] case, which means to destroy all variants beyond `n`.
  if (n <= u.size) {
    if (!isMutable(self)) {
      if (n == u.size)
        return BL_SUCCESS;

      BLArrayCore newO;
      uint8_t* dst = initArray(&newO, arrayType, n, n);

      if (BL_UNLIKELY(!dst))
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

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
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  size_t immutableMsk = BLIntOps::bitMaskFromBool<size_t>(!isMutable(self));

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
    uint8_t* dst = initStatic(&newO, arrayType, u.size);
    BLMemOps::copyForwardInlineT(dst, getImpl(self)->dataAs<uint8_t>(), u.size * itemSize);
    return replaceInstance(self, &newO);
  }
  else {
    return reallocToDynamic(self, arrayType, implSizeFromCapacity(n, itemSize));
  }
}

BL_API_IMPL BLResult blArrayMakeMutable(BLArrayCore* self, void** dataOut) noexcept {
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  if (self->_d.sso()) {
    *dataOut = self->_d.u8_data;
    return BL_SUCCESS;
  }

  BLArrayImpl* selfI = getImpl(self);
  if (isMutable(self)) {
    *dataOut = selfI->data;
    return BL_SUCCESS;
  }

  BLObjectType arrayType = self->_d.rawType();
  size_t size = selfI->size;
  size_t itemSize = itemSizeFromArrayType(arrayType);

  BLArrayCore tmp = *self;
  uint8_t* dst = initArray(self, arrayType, size, size);

  if (BL_UNLIKELY(!dst))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  initContentByType(dst, selfI->data, size * itemSize, arrayType);
  releaseInstance(&tmp);

  *dataOut = dst;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blArrayModifyOp(BLArrayCore* self, BLModifyOp op, size_t n, void** dataOut) noexcept {
  using namespace BLArrayPrivate;
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
      BLOverflowFlag of = 0;
      index = u.size;
      sizeAfter = BLIntOps::addOverflow(u.size, n, &of);

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
    size_t immutableMsk = BLIntOps::bitMaskFromBool<size_t>(!isMutable(self));

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
      BLOverflowFlag of = 0;
      index = u.size;
      sizeAfter = BLIntOps::addOverflow(u.size, n, &of);

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

    newO._d.initStatic(arrayType, BLObjectInfo::packFields(uint32_t(sizeAfter), uint32_t(ssoCapacity)));
    BLMemOps::copyForwardInlineT(newO._d.u8_data, u.data, index * itemSize);

    *dataOut = self->_d.u8_data + index * itemSize;
    return replaceInstance(self, &newO);
  }
  else {
    BLObjectImplSize implSize = expandImplSizeWithModifyOp(implSizeFromCapacity(sizeAfter, itemSize), op);
    uint8_t* dst = initDynamic(&newO, arrayType, sizeAfter, implSize);

    if (BL_UNLIKELY(!dst))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    if (self->_d.refCountedFlag() && blObjectImplGetRefCount(self->_d.impl) == 1) {
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
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);

  size_t sizeAfter = BLIntOps::uaddSaturate(u.size, n);
  size_t immutableMsk = BLIntOps::bitMaskFromBool<size_t>(!isMutable(self));

  if ((sizeAfter | immutableMsk) > u.capacity) {
    if (BL_UNLIKELY(sizeAfter > maximumCapacityTable[arrayType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    BLArrayCore tmp = *self;

    uint8_t* dst = nullptr;
    const uint8_t* src = getData<uint8_t>(&tmp);

    size_t ssoCapacity = ssoCapacityTable[arrayType];
    if (sizeAfter <= ssoCapacity) {
      dst = initStatic(self, arrayType, sizeAfter);
    }
    else {
      BLObjectImplSize implSize = expandImplSize(implSizeFromCapacity(sizeAfter, itemSize));
      dst = initDynamic(self, arrayType, sizeAfter, implSize);
      if (BL_UNLIKELY(!dst)) {
        *dataOut = nullptr;
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);
      }
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

// BLArray - API - Data Manipulation - Assignment
// ==============================================

BL_API_IMPL BLResult blArrayAssignMove(BLArrayCore* self, BLArrayCore* other) noexcept {
  using namespace BLArrayPrivate;

  BL_ASSERT(self->_d.isArray());
  BL_ASSERT(other->_d.isArray());
  BL_ASSERT(self->_d.rawType() == other->_d.rawType());

  BLObjectType arrayType = self->_d.rawType();
  BLArrayCore tmp = *other;

  initStatic(other, arrayType);
  return replaceInstance(self, &tmp);
}

BL_API_IMPL BLResult blArrayAssignWeak(BLArrayCore* self, const BLArrayCore* other) noexcept {
  using namespace BLArrayPrivate;

  BL_ASSERT(self->_d.isArray());
  BL_ASSERT(other->_d.isArray());
  BL_ASSERT(self->_d.rawType() == other->_d.rawType());

  blObjectPrivateAddRefTagged(other);
  return replaceInstance(self, other);
}

BL_API_IMPL BLResult blArrayAssignDeep(BLArrayCore* self, const BLArrayCore* other) noexcept {
  using namespace BLArrayPrivate;

  BL_ASSERT(self->_d.isArray());
  BL_ASSERT(other->_d.isArray());
  BL_ASSERT(self->_d.rawType() == other->_d.rawType());

  return blArrayAssignData(self, getData(other), getSize(other));
}

BL_API_IMPL BLResult blArrayAssignData(BLArrayCore* self, const void* items, size_t n) noexcept {
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);
  size_t immutableMsk = BLIntOps::bitMaskFromBool<size_t>(!isMutable(self));

  if ((n | immutableMsk) > u.capacity) {
    if (BL_UNLIKELY(n > maximumCapacityTable[arrayType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    BLObjectImplSize implSize = implSizeFromCapacity(u.size, itemSize);
    BLArrayCore newO;

    uint8_t* dst = initDynamic(&newO, arrayType, n, implSize);
    if (BL_UNLIKELY(!dst))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

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
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);

  BLOverflowFlag of = 0;
  BLIntOps::mulOverflow(capacity, itemSize, &of);

  if (BL_UNLIKELY(!capacity || capacity < size || !blDataAccessFlagsIsValid(accessFlags) || of))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLArrayCore newO;
  BL_PROPAGATE(initExternal(&newO, arrayType, externalData, size, capacity, accessFlags, destroyFunc, userData));

  return replaceInstance(self, &newO);
}

// BLArray - API - Data Manipulation - Append
// ==========================================

BL_API_IMPL BLResult blArrayAppendU8(BLArrayCore* self, uint8_t value) noexcept { return BLArrayPrivate::appendTypeT<uint8_t>(self, value); }
BL_API_IMPL BLResult blArrayAppendU16(BLArrayCore* self, uint16_t value) noexcept { return BLArrayPrivate::appendTypeT<uint16_t>(self, value); }
BL_API_IMPL BLResult blArrayAppendU32(BLArrayCore* self, uint32_t value) noexcept { return BLArrayPrivate::appendTypeT<uint32_t>(self, value); }
BL_API_IMPL BLResult blArrayAppendU64(BLArrayCore* self, uint64_t value) noexcept { return BLArrayPrivate::appendTypeT<uint64_t>(self, value); }
BL_API_IMPL BLResult blArrayAppendF32(BLArrayCore* self, float value) noexcept { return BLArrayPrivate::appendTypeT<float>(self, value); }
BL_API_IMPL BLResult blArrayAppendF64(BLArrayCore* self, double value) noexcept { return BLArrayPrivate::appendTypeT<double>(self, value); }

BL_API_IMPL BLResult blArrayAppendItem(BLArrayCore* self, const void* item) noexcept {
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);
  size_t immutableMsk = BLIntOps::bitMaskFromBool<size_t>(!isMutable(self));

  if (BL_UNLIKELY((u.size | immutableMsk) >= u.capacity)) {
    if (BL_UNLIKELY(u.size >= maximumCapacityTable[arrayType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    BLArrayCore newO;
    BLObjectImplSize implSize = expandImplSize(implSizeFromCapacity(u.size + 1u, itemSize));

    uint8_t* dst = initDynamic(&newO, arrayType, u.size + 1, implSize);
    if (BL_UNLIKELY(!dst))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    // Copy the existing data to a new place / move if the data will be destroyed.
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
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);
  size_t immutableMsk = BLIntOps::bitMaskFromBool<size_t>(!isMutable(self));

  size_t sizeAfter = BLIntOps::uaddSaturate(u.size, n);

  if (BL_UNLIKELY((sizeAfter | immutableMsk) > u.capacity)) {
    if (BL_UNLIKELY(sizeAfter > maximumCapacityTable[arrayType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    BLArrayCore newO;
    BLObjectImplSize implSize = expandImplSize(implSizeFromCapacity(sizeAfter, itemSize));

    uint8_t* dst = initDynamic(&newO, arrayType, sizeAfter, implSize);
    if (BL_UNLIKELY(!dst))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    // Copy the existing data to a new place / move if the data will be destroyed.
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

// BLArray - API - Data Manipulation - Insert
// ==========================================

BL_API_IMPL BLResult blArrayInsertU8(BLArrayCore* self, size_t index, uint8_t value) noexcept { return BLArrayPrivate::insertTypeT<uint8_t>(self, index, value); }
BL_API_IMPL BLResult blArrayInsertU16(BLArrayCore* self, size_t index, uint16_t value) noexcept { return BLArrayPrivate::insertTypeT<uint16_t>(self, index, value); }
BL_API_IMPL BLResult blArrayInsertU32(BLArrayCore* self, size_t index, uint32_t value) noexcept { return BLArrayPrivate::insertTypeT<uint32_t>(self, index, value); }
BL_API_IMPL BLResult blArrayInsertU64(BLArrayCore* self, size_t index, uint64_t value) noexcept { return BLArrayPrivate::insertTypeT<uint64_t>(self, index, value); }
BL_API_IMPL BLResult blArrayInsertF32(BLArrayCore* self, size_t index, float value) noexcept { return BLArrayPrivate::insertTypeT<float>(self, index, value); }
BL_API_IMPL BLResult blArrayInsertF64(BLArrayCore* self, size_t index, double value) noexcept { return BLArrayPrivate::insertTypeT<double>(self, index, value); }
BL_API_IMPL BLResult blArrayInsertItem(BLArrayCore* self, size_t index, const void* item) noexcept { return blArrayInsertData(self, index, item, 1); }

BL_API_IMPL BLResult blArrayInsertData(BLArrayCore* self, size_t index, const void* items, size_t n) noexcept {
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);
  size_t immutableMsk = BLIntOps::bitMaskFromBool<size_t>(!isMutable(self));

  size_t endIndex = index + n;
  size_t sizeAfter = BLIntOps::uaddSaturate(u.size, n);

  if ((sizeAfter | immutableMsk) > u.capacity) {
    if (BL_UNLIKELY(sizeAfter > maximumCapacityTable[arrayType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    BLObjectImplSize implSize = expandImplSize(implSizeFromCapacity(sizeAfter, itemSize));
    BLArrayCore newO;

    uint8_t* dst = initDynamic(&newO, arrayType, sizeAfter, implSize);
    if (BL_UNLIKELY(!dst))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

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

// BLArray - API - Data Manipulation - Replace
// ===========================================

BL_API_IMPL BLResult blArrayReplaceU8(BLArrayCore* self, size_t index, uint8_t value) noexcept { return BLArrayPrivate::replaceTypeT<uint8_t>(self, index, value); }
BL_API_IMPL BLResult blArrayReplaceU16(BLArrayCore* self, size_t index, uint16_t value) noexcept { return BLArrayPrivate::replaceTypeT<uint16_t>(self, index, value); }
BL_API_IMPL BLResult blArrayReplaceU32(BLArrayCore* self, size_t index, uint32_t value) noexcept { return BLArrayPrivate::replaceTypeT<uint32_t>(self, index, value); }
BL_API_IMPL BLResult blArrayReplaceU64(BLArrayCore* self, size_t index, uint64_t value) noexcept { return BLArrayPrivate::replaceTypeT<uint64_t>(self, index, value); }
BL_API_IMPL BLResult blArrayReplaceF32(BLArrayCore* self, size_t index, float value) noexcept { return BLArrayPrivate::replaceTypeT<float>(self, index, value); }
BL_API_IMPL BLResult blArrayReplaceF64(BLArrayCore* self, size_t index, double value) noexcept { return BLArrayPrivate::replaceTypeT<double>(self, index, value); }

BL_API_IMPL BLResult blArrayReplaceItem(BLArrayCore* self, size_t index, const void* item) noexcept {
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  UnpackedData u = unpack(self);
  if (BL_UNLIKELY(index >= u.size))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLObjectType arrayType = self->_d.rawType();
  size_t itemSize = itemSizeFromArrayType(arrayType);

  if (!isMutable(self)) {
    BLArrayCore newO;
    BLObjectImplSize implSize = implSizeFromCapacity(u.size, itemSize);

    uint8_t* dst = initDynamic(&newO, arrayType, u.size, implSize);
    const uint8_t* src = u.data;

    if (BL_UNLIKELY(!dst))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

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
  using namespace BLArrayPrivate;
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

  if (isMutable(self)) {
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

  uint8_t* dst = initArray(&newO, arrayType, sizeAfter, sizeAfter);
  const uint8_t* src = u.data;

  if (BL_UNLIKELY(!dst))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  initContentByType(dst, src, index * itemSize, arrayType);
  dst += index * itemSize;
  src += (index + rangeSize) * itemSize;

  initContentByType(dst, items, n * itemSize, arrayType);
  dst += n * itemSize;

  initContentByType(dst, src, tailSize * itemSize, arrayType);
  return replaceInstance(self, &newO);
}

// BLArray - API - Data Manipulation - Remove
// ==========================================

BL_API_IMPL BLResult blArrayRemoveIndex(BLArrayCore* self, size_t index) noexcept {
  using namespace BLArrayPrivate;
  BL_ASSERT(self->_d.isArray());

  return blArrayRemoveRange(self, index, index + 1);
}

BL_API_IMPL BLResult blArrayRemoveRange(BLArrayCore* self, size_t rStart, size_t rEnd) noexcept {
  using namespace BLArrayPrivate;
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

    BLMemOps::copySmall(u.data + index * itemSize, u.data + (index + n) * itemSize, (u.size - end) * itemSize);
    BLMemOps::fillSmallT(u.data + sizeAfter * itemSize, uint8_t(0), (ssoCapacity - sizeAfter) * itemSize);

    self->_d.info.setAField(uint32_t(sizeAfter));
    return BL_SUCCESS;
  }
  else if (!isMutable(self)) {
    BLArrayCore newO;
    uint8_t* dst = initArray(&newO, arrayType, sizeAfter, sizeAfter);

    if (BL_UNLIKELY(!dst))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    initContentByType(dst, u.data, index * itemSize, arrayType);
    initContentByType(dst + index * itemSize, u.data + end * itemSize, (u.size - end) * itemSize, arrayType);

    return replaceInstance(self, &newO);
  }
  else {
    uint8_t* data = getData<uint8_t>(self) + index * itemSize;

    releaseContentByType(data, n * itemSize, arrayType);
    memmove(data, data + n * itemSize, (u.size - end) * itemSize);

    setSize(self, sizeAfter);
    return BL_SUCCESS;
  }
}

// BLArray - API - Equality & Comparison
// =====================================

BL_API_IMPL bool blArrayEquals(const BLArrayCore* a, const BLArrayCore* b) noexcept {
  using namespace BLArrayPrivate;

  BL_ASSERT(a->_d.isArray());
  BL_ASSERT(b->_d.isArray());

  if (blObjectPrivateBinaryEquals(a, b))
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

// BLArray - Runtime Registration
// ==============================

void blArrayRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  for (uint32_t objectType = BL_OBJECT_TYPE_ARRAY_FIRST; objectType <= BL_OBJECT_TYPE_ARRAY_LAST; objectType++) {
    blObjectDefaults[objectType]._d.initStatic(
      BLObjectType(objectType),
      BLObjectInfo::packFields(0, BLArrayPrivate::ssoCapacityTable[objectType]));
  }
}

// BLArray - Tests
// ===============

#if defined(BL_TEST)
UNIT(array) {
  INFO("Basic functionality - BLArray<int>");
  {
    BLArray<int> a;
    EXPECT_EQ(a.size(), 0u);
    EXPECT_GT(a.capacity(), 0u);
    EXPECT_TRUE(a._d.sso());

    // [42]
    EXPECT_SUCCESS(a.append(42));
    EXPECT_EQ(a.size(), 1u);
    EXPECT_EQ(a[0], 42);
    EXPECT_TRUE(a._d.sso());

    // [42, 1, 2, 3]
    EXPECT_SUCCESS(a.append(1, 2, 3));
    EXPECT_EQ(a.size(), 4u);
    EXPECT_GE(a.capacity(), 4u);
    EXPECT_EQ(a[0], 42);
    EXPECT_EQ(a[1], 1);
    EXPECT_EQ(a[2], 2);
    EXPECT_EQ(a[3], 3);
    EXPECT_FALSE(a._d.sso());

    // [10, 42, 1, 2, 3]
    EXPECT_SUCCESS(a.prepend(10));
    EXPECT_EQ(a.size(), 5u);
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[1], 42);
    EXPECT_EQ(a[2], 1);
    EXPECT_EQ(a[3], 2);
    EXPECT_EQ(a[4], 3);
    EXPECT_EQ(a.indexOf(4), SIZE_MAX);
    EXPECT_EQ(a.indexOf(3), 4u);
    EXPECT_EQ(a.lastIndexOf(4), SIZE_MAX);
    EXPECT_EQ(a.lastIndexOf(10), 0u);

    BLArray<int> b;
    EXPECT_SUCCESS(b.append(10, 42, 1, 2, 3));
    EXPECT_TRUE(a.equals(b));
    EXPECT_SUCCESS(b.append(99));
    EXPECT_FALSE(a.equals(b));

    // [10, 3]
    EXPECT_SUCCESS(a.remove(BLRange{1, 4}));
    EXPECT_EQ(a.size(), 2u);
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[1], 3);

    // [10, 33, 3]
    EXPECT_SUCCESS(a.insert(1, 33));
    EXPECT_EQ(a.size(), 3u);
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[1], 33);
    EXPECT_EQ(a[2], 3);

    // [10, 33, 3, 999, 1010, 2293]
    EXPECT_SUCCESS(a.insert(2, 999, 1010, 2293));
    EXPECT_EQ(a.size(), 6u);
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[1], 33);
    EXPECT_EQ(a[2], 999);
    EXPECT_EQ(a[3], 1010);
    EXPECT_EQ(a[4], 2293);
    EXPECT_EQ(a[5], 3);

    EXPECT_SUCCESS(a.insert(6, 1));
    EXPECT_EQ(a[6], 1);

    EXPECT_SUCCESS(a.clear());
    EXPECT_SUCCESS(a.insert(0, 1));
    EXPECT_SUCCESS(a.insert(1, 2));
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(a[1], 2);
  }

  INFO("Basic functionality - BLArray<uint64_t>");
  {
    BLArray<uint64_t> a;

    EXPECT_EQ(a.size(), 0u);
    EXPECT_GT(a.capacity(), 0u);
    EXPECT_TRUE(a._d.sso());

    for (size_t i = 0; i < 1000; i++)
      EXPECT_SUCCESS(a.append(i));

    // NOTE: AppendItem must work, but it's never called by C++ API (C++ API would call blArrayAppendU64 instead).
    for (uint64_t i = 0; i < 1000; i++)
      EXPECT_SUCCESS(blArrayAppendItem(&a, &i));

    EXPECT_EQ(a.size(), 2000u);
    for (size_t i = 0; i < 2000; i++)
      EXPECT_EQ(a[i], i % 1000u);
  }

  INFO("Basic functionality - C API");
  {
    BLArrayCore a;
    BLArray<uint64_t> b;

    EXPECT_SUCCESS(blArrayInit(&a, BL_OBJECT_TYPE_ARRAY_UINT64));
    EXPECT_EQ(blArrayGetSize(&a), b.size());
    EXPECT_EQ(blArrayGetCapacity(&a), b.capacity());

    const uint64_t items[] = { 1, 2, 3, 4, 5 };
    EXPECT_SUCCESS(blArrayAppendData(&a, &items, BL_ARRAY_SIZE(items)));
    EXPECT_EQ(blArrayGetSize(&a), 5u);

    for (size_t i = 0; i < BL_ARRAY_SIZE(items); i++)
      EXPECT_EQ(static_cast<const uint64_t*>(blArrayGetData(&a))[i], items[i]);

    EXPECT_SUCCESS(blArrayInsertData(&a, 1, &items, BL_ARRAY_SIZE(items)));
    const uint64_t itemsAfterInsertion[] = { 1, 1, 2, 3, 4, 5, 2, 3, 4, 5 };
    for (size_t i = 0; i < BL_ARRAY_SIZE(itemsAfterInsertion); i++)
      EXPECT_EQ(static_cast<const uint64_t*>(blArrayGetData(&a))[i], itemsAfterInsertion[i]);

    EXPECT_SUCCESS(blArrayDestroy(&a));
  }

  INFO("External array");
  {
    BLArray<int> a;
    int externalData[4] = { 0 };

    EXPECT_SUCCESS(a.assignExternalData(externalData, 0, 4, BL_DATA_ACCESS_RW));
    EXPECT_EQ(a.data(), externalData);

    EXPECT_SUCCESS(a.append(42));
    EXPECT_EQ(externalData[0], 42);

    EXPECT_SUCCESS(a.append(1, 2, 3));
    EXPECT_EQ(externalData[3], 3);

    // Appending more items the external array can hold must reallocate it.
    EXPECT_SUCCESS(a.append(4));
    EXPECT_NE(a.data(), externalData);
    EXPECT_EQ(a[0], 42);
    EXPECT_EQ(a[1], 1);
    EXPECT_EQ(a[2], 2);
    EXPECT_EQ(a[3], 3);
    EXPECT_EQ(a[4], 4);
  }

  INFO("String array");
  {
    BLArray<BLString> a;
    EXPECT_EQ(a.size(), 0u);

    a.append(BLString("Hello"));
    EXPECT_EQ(a.size(), 1u);
    EXPECT_TRUE(a[0].equals("Hello"));

    a.insert(0, BLString("Blend2D"));
    EXPECT_EQ(a.size(), 2u);
    EXPECT_TRUE(a[0].equals("Blend2D"));
    EXPECT_TRUE(a[1].equals("Hello"));

    a.insert(2, BLString("World!"));
    EXPECT_EQ(a.size(), 3u);
    EXPECT_TRUE(a[0].equals("Blend2D"));
    EXPECT_TRUE(a[1].equals("Hello"));
    EXPECT_TRUE(a[2].equals("World!"));

    a.insertData(1, a.view());
    EXPECT_EQ(a.size(), 6u);
    EXPECT_TRUE(a[0].equals("Blend2D"));
    EXPECT_TRUE(a[1].equals("Blend2D"));
    EXPECT_TRUE(a[2].equals("Hello"));
    EXPECT_TRUE(a[3].equals("World!"));
    EXPECT_TRUE(a[4].equals("Hello"));
    EXPECT_TRUE(a[5].equals("World!"));
  }
}
#endif
