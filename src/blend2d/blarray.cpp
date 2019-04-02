// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blarray.h"
#include "./blarray_p.h"
#include "./blruntime_p.h"
#include "./blsupport_p.h"
#include "./bltables_p.h"
#include "./blvariant_p.h"

// ============================================================================
// [BLArray - Global]
// ============================================================================

enum : uint32_t {
  BL_IMPL_TYPE_ARRAY_FIRST = BL_IMPL_TYPE_ARRAY_VAR,
  BL_IMPL_TYPE_ARRAY_LAST = BL_IMPL_TYPE_ARRAY_STRUCT_32
};

static BLWrap<BLArrayImpl> blNullArrayImpl[BL_IMPL_TYPE_ARRAY_LAST + 1];
static const uint8_t blNullArrayBuffer[64] = { 0 };

// ============================================================================
// [BLArray - Capacity]
// ============================================================================

static constexpr size_t blArrayImplSizeOf() noexcept {
  return sizeof(BLArrayImpl);
}

static constexpr size_t blArrayImplSizeOf(size_t itemSize, size_t n) noexcept {
  return blContainerSizeOf(sizeof(BLArrayImpl), itemSize, n);
}

static constexpr size_t blArrayCapacityOf(size_t itemSize, size_t implSize) noexcept {
  return blContainerCapacityOf(blArrayImplSizeOf(), itemSize, implSize);
}

static constexpr size_t blArrayInitialCapacity(size_t itemSize) noexcept {
  return blArrayCapacityOf(itemSize, BL_ALLOC_HINT_ARRAY);
}

static BL_INLINE size_t blArrayFittingCapacity(size_t itemSize, size_t n) noexcept {
  return blContainerFittingCapacity(blArrayImplSizeOf(), itemSize, n);
}

static BL_INLINE size_t blArrayGrowingCapacity(size_t itemSize, size_t n) noexcept {
  return blContainerGrowingCapacity(blArrayImplSizeOf(), itemSize, n, BL_ALLOC_HINT_ARRAY);
}

// ============================================================================
// [BLArray - Tables]
// ============================================================================

struct BLArrayItemSizeGen {
  static constexpr uint8_t value(size_t implType) noexcept {
    return implType == BL_IMPL_TYPE_ARRAY_VAR       ? uint8_t(sizeof(void*)) :
           implType == BL_IMPL_TYPE_ARRAY_I8        ? uint8_t(1) :
           implType == BL_IMPL_TYPE_ARRAY_U8        ? uint8_t(1) :
           implType == BL_IMPL_TYPE_ARRAY_I16       ? uint8_t(2) :
           implType == BL_IMPL_TYPE_ARRAY_U16       ? uint8_t(2) :
           implType == BL_IMPL_TYPE_ARRAY_I32       ? uint8_t(4) :
           implType == BL_IMPL_TYPE_ARRAY_U32       ? uint8_t(4) :
           implType == BL_IMPL_TYPE_ARRAY_I64       ? uint8_t(8) :
           implType == BL_IMPL_TYPE_ARRAY_U64       ? uint8_t(8) :
           implType == BL_IMPL_TYPE_ARRAY_F32       ? uint8_t(4) :
           implType == BL_IMPL_TYPE_ARRAY_F64       ? uint8_t(8) :
           implType == BL_IMPL_TYPE_ARRAY_STRUCT_1  ? uint8_t(1) :
           implType == BL_IMPL_TYPE_ARRAY_STRUCT_2  ? uint8_t(2) :
           implType == BL_IMPL_TYPE_ARRAY_STRUCT_3  ? uint8_t(3) :
           implType == BL_IMPL_TYPE_ARRAY_STRUCT_4  ? uint8_t(4) :
           implType == BL_IMPL_TYPE_ARRAY_STRUCT_6  ? uint8_t(6) :
           implType == BL_IMPL_TYPE_ARRAY_STRUCT_8  ? uint8_t(8) :
           implType == BL_IMPL_TYPE_ARRAY_STRUCT_10 ? uint8_t(10) :
           implType == BL_IMPL_TYPE_ARRAY_STRUCT_12 ? uint8_t(12) :
           implType == BL_IMPL_TYPE_ARRAY_STRUCT_16 ? uint8_t(16) :
           implType == BL_IMPL_TYPE_ARRAY_STRUCT_20 ? uint8_t(20) :
           implType == BL_IMPL_TYPE_ARRAY_STRUCT_24 ? uint8_t(24) :
           implType == BL_IMPL_TYPE_ARRAY_STRUCT_32 ? uint8_t(32) : uint8_t(0);
  }
};

struct BLArrayMaximumCapacityGen {
  static constexpr size_t value(size_t implType) noexcept {
    return BLArrayItemSizeGen::value(implType) == 0
      ? size_t(0)
      : blArrayCapacityOf(BLArrayItemSizeGen::value(implType), SIZE_MAX);
  }
};

static constexpr const auto blArrayItemSizeTable = blLookupTable<uint8_t, BL_IMPL_TYPE_COUNT, BLArrayItemSizeGen>();
static constexpr const auto blArrayMaximumCapacityTable = blLookupTable<size_t, BL_IMPL_TYPE_COUNT, BLArrayMaximumCapacityGen>();

// ============================================================================
// [BLArray - Dispatch Funcs]
// ============================================================================

static constexpr bool blArrayDispatchTypeByImplType(uint32_t implType) noexcept {
  return implType == BL_IMPL_TYPE_ARRAY_VAR;
}

static constexpr bool blIsVarArrayImplType(uint32_t implType) noexcept {
  return implType == BL_IMPL_TYPE_ARRAY_VAR;
}

// Only used as a filler
template<size_t N>
struct BLUInt32xN { uint32_t data[N]; };

template<typename T>
static BL_INLINE void blArrayFillPattern(T* dst, T src, size_t n) noexcept {
  for (size_t i = 0; i < n; i++)
    dst[i] = src;
}

static BLResult BL_CDECL blArrayDestroySimpleData(void* dst, size_t nBytes) noexcept {
  BL_UNUSED(dst);
  BL_UNUSED(nBytes);
  return BL_SUCCESS;
}

static void* BL_CDECL blArrayCopyVariantData(void* dst_, const void* src_, size_t nBytes) noexcept {
  BL_ASSUME((nBytes % sizeof(BLVariant)) == 0);

  BLVariant* dst = static_cast<BLVariant*>(dst_);
  const BLVariant* src = static_cast<const BLVariant*>(src_);

  void* end = blOffsetPtr(dst, nBytes);
  while (dst != end) {
    dst->impl = blImplIncRef(src->impl);

    dst = blOffsetPtr(dst, sizeof(BLVariant));
    src = blOffsetPtr(dst, sizeof(BLVariant));
  }

  return dst_;
}

static void* BL_CDECL blArrayReplaceVariantData(void* dst_, const void* src_, size_t nBytes) noexcept {
  BL_ASSUME((nBytes % sizeof(BLVariant)) == 0);

  BLVariant* dst = static_cast<BLVariant*>(dst_);
  const BLVariant* src = static_cast<const BLVariant*>(src_);

  void* end = blOffsetPtr(dst, nBytes);
  while (dst != end) {
    BLVariantImpl* replacedImpl = dst->impl;
    dst->impl = blImplIncRef(src->impl);
    blVariantImplRelease(replacedImpl);

    dst = blOffsetPtr(dst, sizeof(BLVariant));
    src = blOffsetPtr(dst, sizeof(BLVariant));
  }

  return dst_;
}

static BLResult BL_CDECL blArrayDestroyVariantData(void* data, size_t nBytes) noexcept {
  BL_ASSUME((nBytes % sizeof(BLVariant)) == 0);
  for (size_t i = 0; i < nBytes; i += sizeof(BLVariant))
    blVariantImplRelease(blOffsetPtr<BLVariant>(data, i)->impl);
  return BL_SUCCESS;
}

struct BLArrayFuncs {
  typedef void* (BL_CDECL* CopyData)(void* dst, const void* src, size_t nBytes) BL_NOEXCEPT;
  typedef void* (BL_CDECL* ReplaceData)(void* dst, const void* src, size_t nBytes) BL_NOEXCEPT;
  typedef BLResult (BL_CDECL* DestroyData)(void* dst, size_t nBytes) BL_NOEXCEPT;

  CopyData copyData;
  ReplaceData replaceData;
  DestroyData destroyData;
};

static const BLArrayFuncs blArrayFuncs[2] = {
  // DispatchType #0: Arrays that store simple data.
  {
    (BLArrayFuncs::CopyData)memcpy,
    (BLArrayFuncs::ReplaceData)memcpy,
    blArrayDestroySimpleData
  },

  // DispatchType #1:Arrays that store BLVariant or { BLVariant, ... } items.
  {
    blArrayCopyVariantData,
    blArrayReplaceVariantData,
    blArrayDestroyVariantData
  }
};

static const BLArrayFuncs& blArrayFuncsByDispatchType(uint32_t dispatchType) noexcept {
  BL_ASSERT(dispatchType < BL_ARRAY_SIZE(blArrayFuncs));
  return blArrayFuncs[dispatchType];
}

// ============================================================================
// [BLArray - Internals]
// ============================================================================

static BL_INLINE BLArrayImpl* blArrayImplNew(uint32_t implType, size_t capacity) noexcept {
  uint32_t itemSize = blArrayItemSizeTable[implType];
  uint16_t memPoolData;
  BLArrayImpl* impl = blRuntimeAllocImplT<BLArrayImpl>(blArrayImplSizeOf(itemSize, capacity), &memPoolData);

  if (BL_UNLIKELY(!impl))
    return impl;

  blImplInit(impl, implType, 0, memPoolData);
  impl->data = blOffsetPtr<void>(impl, sizeof(BLArrayImpl));
  impl->size = 0;
  impl->capacity = capacity;
  impl->itemSize = uint8_t(itemSize);
  impl->dispatchType = uint8_t(blArrayDispatchTypeByImplType(implType));
  impl->reserved[0] = 0;
  impl->reserved[1] = 0;

  return impl;
}

// Cannot be static, called by `BLVariant` implementation.
BLResult blArrayImplDelete(BLArrayImpl* impl) noexcept {
  const BLArrayFuncs& funcs = blArrayFuncsByDispatchType(impl->dispatchType);
  funcs.destroyData(impl->data, impl->size * impl->itemSize);

  uint8_t* implBase = reinterpret_cast<uint8_t*>(impl);
  size_t implSize = blArrayImplSizeOf(impl->implType, impl->capacity);
  uint32_t implTraits = impl->implTraits;
  uint32_t memPoolData = impl->memPoolData;

  if (implTraits & BL_IMPL_TRAIT_EXTERNAL) {
    implSize = blArrayImplSizeOf() + sizeof(BLExternalImplPreface);
    implBase -= sizeof(BLExternalImplPreface);
    blImplDestroyExternal(impl);
  }

  if (implTraits & BL_IMPL_TRAIT_FOREIGN)
    return BL_SUCCESS;
  else
    return blRuntimeFreeImpl(implBase, implSize, memPoolData);
}

static BL_NOINLINE BLResult blArrayRealloc(BLArrayCore* self, size_t capacity) noexcept {
  BLArrayImpl* oldI = self->impl;
  BLArrayImpl* newI = blArrayImplNew(oldI->implType, capacity);

  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BL_ASSERT(newI->itemSize == oldI->itemSize);

  size_t size = oldI->size;
  size_t itemSize = oldI->itemSize;

  self->impl = newI;
  newI->size = size;

  if (oldI->refCount == 1) {
    // Zero the old size and fall through to memcpy(). This is much better as
    // we don't have to IncRef/DecRef the same BLVariant[].
    oldI->size = 0;
    memcpy(newI->data, oldI->data, size * itemSize);
    return blArrayImplRelease(oldI);
  }
  else {
    const BLArrayFuncs& funcs = blArrayFuncsByDispatchType(oldI->dispatchType);
    funcs.copyData(newI->data, oldI->data, size * itemSize);
    return blArrayImplRelease(oldI);
  }
}

// ============================================================================
// [BLArray - Init / Reset]
// ============================================================================

BLResult blArrayInit(BLArrayCore* self, uint32_t arrayTypeId) noexcept {
  if (BL_UNLIKELY(arrayTypeId >= BL_IMPL_TYPE_COUNT || !blArrayItemSizeTable[arrayTypeId])) {
    self->impl = &blNullArrayImpl[0];
    return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  self->impl = &blNullArrayImpl[arrayTypeId];
  return BL_SUCCESS;
}

BLResult blArrayReset(BLArrayCore* self) noexcept {
  BLArrayImpl* selfI = self->impl;
  self->impl = &blNullArrayImpl[selfI->implType];

  if (blImplDecRefAndTest(selfI))
    return blArrayImplDelete(selfI);
  return BL_SUCCESS;
}

// ============================================================================
// [BLArray - Storage]
// ============================================================================

size_t blArrayGetSize(const BLArrayCore* self) noexcept {
  return self->impl->size;
}

size_t blArrayGetCapacity(const BLArrayCore* self) noexcept {
  return self->impl->capacity;
}

const void* blArrayGetData(const BLArrayCore* self) noexcept {
  return self->impl->data;
}

BLResult blArrayClear(BLArrayCore* self) noexcept {
  BLArrayImpl* selfI = self->impl;
  size_t size = selfI->size;
  if (!size)
    return BL_SUCCESS;

  if (!blImplIsMutable(selfI)) {
    self->impl = &blNullArrayImpl[selfI->implType];
    return blArrayImplRelease(selfI);
  }

  selfI->size = 0;

  const BLArrayFuncs& funcs = blArrayFuncsByDispatchType(selfI->dispatchType);
  return funcs.destroyData(selfI->data, size * selfI->itemSize);
}

BLResult blArrayShrink(BLArrayCore* self) noexcept {
  BLArrayImpl* selfI = self->impl;

  size_t size = selfI->size;
  if (size == 0) {
    self->impl = &blNullArrayImpl[selfI->implType];
    return blArrayImplRelease(selfI);
  }

  size_t capacity = blArrayFittingCapacity(selfI->itemSize, size);
  if (capacity < selfI->capacity)
    BL_PROPAGATE(blArrayRealloc(self, capacity));

  return BL_SUCCESS;
}

BLResult blArrayResize(BLArrayCore* self, size_t n, const void* fill) noexcept {
  BLArrayImpl* selfI = self->impl;
  size_t size = selfI->size;
  size_t itemSize = selfI->itemSize;

  // If `n` is smaller than the current `size` then this is a truncation. We
  // only have to cover the BLVariant[] case, which means to destroy all
  // variants beyond `n`.
  const BLArrayFuncs& funcs = blArrayFuncsByDispatchType(selfI->dispatchType);
  if (n <= size) {
    if (!blImplIsMutable(selfI)) {
      if (n == size)
        return BL_SUCCESS;

      size_t capacity = blArrayFittingCapacity(itemSize, n);
      BLArrayImpl* newI = blArrayImplNew(selfI->implType, capacity);

      if (BL_UNLIKELY(!newI))
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

      newI->size = n;
      self->impl = newI;
      funcs.copyData(newI->data, selfI->data, n * itemSize);
      return blArrayImplRelease(selfI);
    }
    else {
      selfI->size = n;
      return funcs.destroyData(selfI->data, (size - n) * itemSize);
    }
  }

  // `n` becames the number of items to add to the array.
  n -= size;

  void* dst;
  BL_PROPAGATE(blArrayModifyOp(self, BL_MODIFY_OP_APPEND_FIT, n, &dst));

  if (BL_UNLIKELY(!fill)) {
    memset(dst, 0, n * itemSize);
    return BL_SUCCESS;
  }

  if (selfI->dispatchType) {
    BLVariant* dstPtr = static_cast<BLVariant*>(dst);
    const BLVariant* fillPtr = static_cast<const BLVariant*>(fill);

    size_t tupleSize = itemSize / sizeof(BLVariant);
    size_t i, j;

    for (j = 0; j < tupleSize; j++) {
      BLVariantImpl* impl = fillPtr[j].impl;
      if (impl->refCount != 0)
        blAtomicFetchIncRef(&impl->refCount, n);
    }

    for (i = 0; i < n; i++) {
      for (j = 0; j < tupleSize; j++) {
        dstPtr->impl = fillPtr[j].impl;
        dstPtr++;
      }
    }
    return BL_SUCCESS;
  }

  switch (itemSize) {
    case  1: blArrayFillPattern(static_cast<uint8_t      *>(dst), *static_cast<const uint8_t      *>(fill), n); break;
    case  2: blArrayFillPattern(static_cast<uint16_t     *>(dst), *static_cast<const uint16_t     *>(fill), n); break;
    case  4: blArrayFillPattern(static_cast<uint32_t     *>(dst), *static_cast<const uint32_t     *>(fill), n); break;
    case  8: blArrayFillPattern(static_cast<BLUInt32xN<2>*>(dst), *static_cast<const BLUInt32xN<2>*>(fill), n); break;
    case 12: blArrayFillPattern(static_cast<BLUInt32xN<3>*>(dst), *static_cast<const BLUInt32xN<3>*>(fill), n); break;
    case 16: blArrayFillPattern(static_cast<BLUInt32xN<4>*>(dst), *static_cast<const BLUInt32xN<4>*>(fill), n); break;

    default: {
      uint32_t* dst32 = static_cast<uint32_t*>(dst);
      const uint32_t* src32 = static_cast<const uint32_t*>(fill);
      size_t itemSizeDiv4 = itemSize / 4;

      for (size_t i = 0; i < n; i++)
        for (size_t j = 0; j < itemSizeDiv4; j++)
          *dst32++ = src32[j];
      break;
    }
  }

  return BL_SUCCESS;
}

BLResult blArrayMakeMutable(BLArrayCore* self, void** dataOut) noexcept {
  BLArrayImpl* selfI = self->impl;

  if (!blImplIsMutable(selfI)) {
    size_t size = selfI->size;
    size_t capacity = blArrayFittingCapacity(selfI->itemSize, blMax(size, blArrayInitialCapacity(selfI->itemSize)));

    BL_PROPAGATE(blArrayRealloc(self, capacity));
    selfI = self->impl;
  }

  *dataOut = selfI->data;
  return BL_SUCCESS;
}

BLResult blArrayReserve(BLArrayCore* self, size_t n) noexcept {
  BLArrayImpl* selfI = self->impl;
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  if ((n | immutableMsk) > selfI->capacity) {
    if (BL_UNLIKELY(n > blArrayMaximumCapacityTable[selfI->implType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity = blArrayFittingCapacity(selfI->itemSize, blMax(n, selfI->size));
    return blArrayRealloc(self, capacity);
  }

  return BL_SUCCESS;
}

BLResult blArrayModifyOp(BLArrayCore* self, uint32_t op, size_t n, void** dataOut) noexcept {
  BLArrayImpl* selfI = self->impl;
  size_t size = selfI->size;
  size_t itemSize = selfI->itemSize;

  size_t index = (op >= BL_MODIFY_OP_APPEND_START) ? size : size_t(0);
  size_t sizeAfter = blUAddSaturate(size, n);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));
  const BLArrayFuncs& funcs = blArrayFuncsByDispatchType(selfI->dispatchType);

  if ((sizeAfter | immutableMsk) > selfI->capacity) {
    if (BL_UNLIKELY(sizeAfter > blArrayMaximumCapacityTable[selfI->implType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity =
      (op & BL_MODIFY_OP_GROW_MASK)
        ? blArrayGrowingCapacity(itemSize, sizeAfter)
        : blArrayFittingCapacity(itemSize, sizeAfter);

    BLArrayImpl* newI = blArrayImplNew(selfI->implType, capacity);
    if (BL_UNLIKELY(!newI)) {
      *dataOut = nullptr;
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
    }

    self->impl = newI;
    newI->size = sizeAfter;

    uint8_t* dst = newI->dataAs<uint8_t>();
    *dataOut = dst + index * itemSize;

    if (immutableMsk) {
      funcs.copyData(dst, selfI->data, index * itemSize);
      return blArrayImplRelease(selfI);
    }
    else {
      // Fallthrough to memcpy as it's faster than IncRef/DecRef.
      selfI->size = 0;
      memcpy(dst, selfI->data, index * itemSize);
      return blArrayImplRelease(selfI);
    }
  }
  else {
    uint8_t* data = selfI->dataAs<uint8_t>();
    selfI->size = sizeAfter;

    *dataOut = data + index * itemSize;
    return funcs.destroyData(data, size * itemSize);
  }
}

BLResult blArrayInsertOp(BLArrayCore* self, size_t index, size_t n, void** dataOut) noexcept {
  BLArrayImpl* selfI = self->impl;
  size_t size = selfI->size;
  size_t itemSize = selfI->itemSize;

  size_t sizeAfter = blUAddSaturate(size, n);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  if ((sizeAfter | immutableMsk) > selfI->capacity) {
    if (BL_UNLIKELY(sizeAfter > blArrayMaximumCapacityTable[selfI->implType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity = blArrayGrowingCapacity(itemSize, sizeAfter);
    BLArrayImpl* newI = blArrayImplNew(selfI->implType, capacity);

    if (BL_UNLIKELY(!newI)) {
      *dataOut = nullptr;
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
    }

    self->impl = newI;
    newI->size = sizeAfter;

    uint8_t* dst = newI->dataAs<uint8_t>();
    *dataOut = dst + index * itemSize;

    // NOTE: The same trick as used everywhere - if this is a mutable BLVariant[]
    // array we just set its size to zero and use `memcpy()` as it's much faster
    // than going through IncRef/DecRef of BLVariant[].
    BLArrayFuncs::CopyData copyData = blArrayFuncsByDispatchType(selfI->dispatchType).copyData;
    if (!immutableMsk) {
      selfI->size = 0;
      copyData = (BLArrayFuncs::CopyData)memcpy;
    }

    const uint8_t* src = selfI->dataAs<uint8_t>();
    copyData(dst, src, index * itemSize);
    copyData(dst + (index + n) * itemSize, src + index * itemSize, (size - index) * itemSize);
    return blArrayImplRelease(selfI);
  }
  else {
    selfI->size = sizeAfter;
    uint8_t* data = selfI->dataAs<uint8_t>();

    *dataOut = data + index * itemSize;
    memmove(data + (index + n) * itemSize, data + index * itemSize, (size - index) * itemSize);

    return BL_SUCCESS;
  }
}

// ============================================================================
// [BLArray - Assign]
// ============================================================================

BLResult blArrayAssignMove(BLArrayCore* self, BLArrayCore* other) noexcept {
  BLArrayImpl* selfI = self->impl;
  BLArrayImpl* otherI = other->impl;

  self->impl = otherI;
  other->impl = &blNullArrayImpl[otherI->implType];

  return blArrayImplRelease(selfI);
}

BLResult blArrayAssignWeak(BLArrayCore* self, const BLArrayCore* other) noexcept {
  BLArrayImpl* selfI = self->impl;
  BLArrayImpl* otherI = other->impl;

  self->impl = blImplIncRef(otherI);
  return blArrayImplRelease(selfI);
}

BLResult blArrayAssignDeep(BLArrayCore* self, const BLArrayCore* other) noexcept {
  BLArrayImpl* otherI = other->impl;
  return blArrayAssignView(self, otherI->data, otherI->size);
}

BLResult blArrayAssignView(BLArrayCore* self, const void* items, size_t n) noexcept {
  BLArrayImpl* selfI = self->impl;
  size_t size = selfI->size;
  size_t itemSize = selfI->itemSize;

  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));
  const BLArrayFuncs& funcs = blArrayFuncsByDispatchType(selfI->dispatchType);

  if ((n | immutableMsk) > selfI->capacity) {
    if (BL_UNLIKELY(n > blArrayMaximumCapacityTable[selfI->implType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity = blArrayFittingCapacity(itemSize, size);
    BLArrayImpl* newI = blArrayImplNew(selfI->implType, capacity);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    newI->size = n;
    self->impl = newI;

    funcs.copyData(newI->data, items, n * itemSize);
    return blArrayImplRelease(selfI);
  }

  if (!n)
    return blArrayClear(self);
  selfI->size = n;

  if (blIsVarArrayImplType(selfI->implType)) {
    size_t replaceSize = blMin(size, n);

    uint8_t* dst = selfI->dataAs<uint8_t>();
    const uint8_t* src = static_cast<const uint8_t*>(items);

    funcs.replaceData(dst, src, replaceSize * itemSize);
    return funcs.destroyData(dst + replaceSize * itemSize, (size - replaceSize) * itemSize);
  }
  else {
    // Memory move is required in case of overlap between `data` and `items`.
    memmove(selfI->data, items, n * itemSize);
    return BL_SUCCESS;
  }
}

// ============================================================================
// [BLArray - Append]
// ============================================================================

template<typename T>
static BL_INLINE BLResult blArrayAppendTypeT(BLArrayCore* self, T value) noexcept {
  BLArrayImpl* selfI = self->impl;
  BL_ASSERT(selfI->itemSize == sizeof(T));

  size_t size = selfI->size + 1;
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  // No enough capacity or not mutable - don't inline as this is an expensive
  // case anyway.
  if (BL_UNLIKELY((size | immutableMsk) > selfI->capacity))
    return blArrayAppendItem(self, &value);

  T* dst = selfI->dataAs<T>() + size - 1;
  selfI->size = size;

  *dst = value;
  return BL_SUCCESS;
}

BLResult blArrayAppendU8(BLArrayCore* self, uint8_t value) noexcept { return blArrayAppendTypeT<uint8_t>(self, value); }
BLResult blArrayAppendU16(BLArrayCore* self, uint16_t value) noexcept { return blArrayAppendTypeT<uint16_t>(self, value); }
BLResult blArrayAppendU32(BLArrayCore* self, uint32_t value) noexcept { return blArrayAppendTypeT<uint32_t>(self, value); }
BLResult blArrayAppendU64(BLArrayCore* self, uint64_t value) noexcept { return blArrayAppendTypeT<uint64_t>(self, value); }
BLResult blArrayAppendF32(BLArrayCore* self, float value) noexcept { return blArrayAppendTypeT<float>(self, value); }
BLResult blArrayAppendF64(BLArrayCore* self, double value) noexcept { return blArrayAppendTypeT<double>(self, value); }

BLResult blArrayAppendItem(BLArrayCore* self, const void* item) noexcept {
  BLArrayImpl* selfI = self->impl;
  size_t size = selfI->size;
  size_t itemSize = selfI->itemSize;

  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));
  BLArrayFuncs::CopyData copyData = blArrayFuncsByDispatchType(selfI->dispatchType).copyData;

  if (BL_UNLIKELY((size | immutableMsk) >= selfI->capacity)) {
    if (BL_UNLIKELY(size >= blArrayMaximumCapacityTable[selfI->implType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity = blArrayGrowingCapacity(itemSize, size + 1);
    BLArrayImpl* newI = blArrayImplNew(selfI->implType, capacity);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    self->impl = newI;
    newI->size = size + 1;

    uint8_t* dst = newI->dataAs<uint8_t>();
    const uint8_t* src = selfI->dataAs<uint8_t>();

    // NOTE: The same trick as used everywhere - if this is a mutable BLVariant[]
    // array we just set its size to zero and use `memcpy()` as it's much faster
    // than going through IncRef/DecRef of BLVariant[].
    copyData(dst + size * itemSize, item, itemSize);
    if (!immutableMsk) {
      selfI->size = 0;
      copyData = (BLArrayFuncs::CopyData)memcpy;
    }

    copyData(dst, src, size * itemSize);
    return blArrayImplRelease(selfI);
  }
  else {
    void* dst = selfI->dataAs<uint8_t>() + size * itemSize;
    selfI->size = size + 1;

    copyData(dst, item, itemSize);
    return BL_SUCCESS;
  }
}

BLResult blArrayAppendView(BLArrayCore* self, const void* items, size_t n) noexcept {
  BLArrayImpl* selfI = self->impl;
  size_t size = selfI->size;
  size_t itemSize = selfI->itemSize;

  size_t sizeAfter = blUAddSaturate(size, n);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));
  const BLArrayFuncs& funcs = blArrayFuncsByDispatchType(selfI->dispatchType);

  if (BL_UNLIKELY((sizeAfter | immutableMsk) > selfI->capacity)) {
    if (BL_UNLIKELY(sizeAfter >= blArrayMaximumCapacityTable[selfI->implType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity = blArrayGrowingCapacity(itemSize, size + 1);
    BLArrayImpl* newI = blArrayImplNew(selfI->implType, capacity);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    self->impl = newI;
    newI->size = sizeAfter;

    uint8_t* dst = newI->dataAs<uint8_t>();
    const uint8_t* src = selfI->dataAs<uint8_t>();

    if (!immutableMsk) {
      selfI->size = 0;
      memcpy(dst, src, size * itemSize);
    }
    else {
      funcs.copyData(dst, src, size * itemSize);
    }

    funcs.copyData(dst + size * itemSize, items, n * itemSize);
    return blArrayImplRelease(selfI);
  }
  else {
    uint8_t* data = selfI->dataAs<uint8_t>();
    selfI->size = sizeAfter;

    funcs.copyData(data + size * itemSize, items, n * itemSize);
    return BL_SUCCESS;
  }
}

// ============================================================================
// [BLArray - Insert]
// ============================================================================

template<typename T>
static BL_INLINE BLResult blArrayInsertSimple(BLArrayCore* self, size_t index, T value) noexcept {
  BL_ASSERT(self->impl->itemSize == sizeof(T));

  T* dst;
  BL_PROPAGATE(blArrayInsertOp(self, index, 1, (void**)&dst));

  *dst = value;
  return BL_SUCCESS;
}

BLResult blArrayInsertU8(BLArrayCore* self, size_t index, uint8_t value) noexcept { return blArrayInsertSimple<uint8_t>(self, index, value); }
BLResult blArrayInsertU16(BLArrayCore* self, size_t index, uint16_t value) noexcept { return blArrayInsertSimple<uint16_t>(self, index, value); }
BLResult blArrayInsertU32(BLArrayCore* self, size_t index, uint32_t value) noexcept { return blArrayInsertSimple<uint32_t>(self, index, value); }
BLResult blArrayInsertU64(BLArrayCore* self, size_t index, uint64_t value) noexcept { return blArrayInsertSimple<uint64_t>(self, index, value); }
BLResult blArrayInsertF32(BLArrayCore* self, size_t index, float value) noexcept { return blArrayInsertSimple<float>(self, index, value); }
BLResult blArrayInsertF64(BLArrayCore* self, size_t index, double value) noexcept { return blArrayInsertSimple<double>(self, index, value); }

BLResult blArrayInsertItem(BLArrayCore* self, size_t index, const void* item) noexcept {
  return blArrayInsertView(self, index, item, 1);
}

BLResult blArrayInsertView(BLArrayCore* self, size_t index, const void* items, size_t n) noexcept {
  BLArrayImpl* selfI = self->impl;
  size_t size = selfI->size;
  size_t itemSize = selfI->itemSize;

  size_t endIndex = index + n;
  size_t sizeAfter = blUAddSaturate(size, n);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));
  const BLArrayFuncs& funcs = blArrayFuncsByDispatchType(selfI->dispatchType);

  if (BL_UNLIKELY((sizeAfter | immutableMsk) > selfI->capacity)) {
    if (BL_UNLIKELY(sizeAfter > blArrayMaximumCapacityTable[selfI->implType]))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity = blArrayGrowingCapacity(itemSize, sizeAfter);
    BLArrayImpl* newI = blArrayImplNew(selfI->implType, capacity);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    uint8_t* dst = newI->dataAs<uint8_t>();
    const uint8_t* src = selfI->dataAs<uint8_t>();

    self->impl = newI;
    newI->size = sizeAfter;

    BLArrayFuncs::CopyData rawCopyData = (BLArrayFuncs::CopyData)memcpy;
    if (!immutableMsk)
      selfI->size = 0;
    else
      rawCopyData = funcs.copyData;

    rawCopyData(dst, src, index * itemSize);
    rawCopyData(dst + endIndex * itemSize, src + index * itemSize, (size - endIndex) * itemSize);
    funcs.copyData(dst + index * itemSize, items, n * itemSize);

    return blArrayImplRelease(selfI);
  }
  else {
    size_t nInBytes = n * itemSize;
    selfI->size = sizeAfter;

    uint8_t* dst = selfI->dataAs<uint8_t>();
    uint8_t* dstEnd = dst + size * itemSize;

    const uint8_t* src = static_cast<const uint8_t*>(items);

    // The destination would point into the first byte that will be modified.
    // So for example if the data is `[ABCDEF]` and we are inserting at index
    // 1 then the `dst` would point to `[BCDEF]`.
    dst += index * itemSize;
    dstEnd += nInBytes;

    // Move the memory in-place making space for items to insert. For example
    // if the destination points to [ABCDEF] and we want to insert 4 items we
    // would get [____ABCDEF].
    memmove(dst + nInBytes, dst, (size - endIndex) * itemSize);

    // Split the [src:srcEnd] into LEAD and TRAIL slices and shift TRAIL slice
    // in a way to cancel the `memmove()` if `src` overlaps `dst`. In practice
    // if there is an overlap the [src:srcEnd] source should be within [dst:dstEnd]
    // as it doesn't make sense to insert something which is outside of the current
    // valid area.
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
    // [abcdBCDEFGHdefgh]

    // Leading area precedes `dst` - nothing changed in here and if this is
    // the whole are then there was no overlap that we would have to deal with.
    size_t nLeadBytes = 0;
    if (src < dst) {
      nLeadBytes = blMin<size_t>((size_t)(dst - src), nInBytes);

      funcs.copyData(dst, src, nLeadBytes);
      dst += nLeadBytes;
      src += nLeadBytes;
    }

    // Trailing area - we either shift none or all of it.
    if (src < dstEnd)
      src += nInBytes; // Shift source in case of overlap.

    funcs.copyData(dst, src, nInBytes - nLeadBytes);
    return BL_SUCCESS;
  }
}

// ============================================================================
// [BLArray - Replace]
// ============================================================================

template<typename T>
static BL_INLINE BLResult blArrayReplaceSimple(BLArrayCore* self, size_t index, T value) noexcept {
  BLArrayImpl* selfI = self->impl;
  BL_ASSERT(selfI->itemSize == sizeof(T));

  size_t size = selfI->size;
  if (BL_UNLIKELY(index >= size))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  // No mutable - don't inline as this is an expensive case anyway.
  if (BL_UNLIKELY(!blImplIsMutable(selfI)))
    return blArrayReplaceItem(self, index, &value);

  T* data = selfI->dataAs<T>();
  data[index] = value;
  return BL_SUCCESS;
}

BLResult blArrayReplaceU8(BLArrayCore* self, size_t index, uint8_t value) noexcept { return blArrayReplaceSimple<uint8_t>(self, index, value); }
BLResult blArrayReplaceU16(BLArrayCore* self, size_t index, uint16_t value) noexcept { return blArrayReplaceSimple<uint16_t>(self, index, value); }
BLResult blArrayReplaceU32(BLArrayCore* self, size_t index, uint32_t value) noexcept { return blArrayReplaceSimple<uint32_t>(self, index, value); }
BLResult blArrayReplaceU64(BLArrayCore* self, size_t index, uint64_t value) noexcept { return blArrayReplaceSimple<uint64_t>(self, index, value); }
BLResult blArrayReplaceF32(BLArrayCore* self, size_t index, float value) noexcept { return blArrayReplaceSimple<float>(self, index, value); }
BLResult blArrayReplaceF64(BLArrayCore* self, size_t index, double value) noexcept { return blArrayReplaceSimple<double>(self, index, value); }

BLResult blArrayReplaceItem(BLArrayCore* self, size_t index, const void* item) noexcept {
  BLArrayImpl* selfI = self->impl;
  size_t size = selfI->size;
  size_t itemSize = selfI->itemSize;

  if (BL_UNLIKELY(index >= size))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  const BLArrayFuncs& funcs = blArrayFuncsByDispatchType(selfI->dispatchType);
  if (!blImplIsMutable(selfI)) {
    size_t capacity = blArrayFittingCapacity(itemSize, size);
    BLArrayImpl* newI = blArrayImplNew(selfI->implType, capacity);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    uint8_t* dst = newI->dataAs<uint8_t>();
    const uint8_t* src = selfI->dataAs<uint8_t>();

    funcs.copyData(dst, src, index * itemSize);
    dst += index * itemSize;
    src += index * itemSize;

    funcs.copyData(dst, item, itemSize);
    dst += itemSize;
    src += itemSize;
    funcs.copyData(dst, src, (size - (index + 1)) * itemSize);

    newI->size = size;
    self->impl = newI;
    return blArrayImplRelease(selfI);
  }
  else {
    void* data = selfI->dataAs<uint8_t>() + index * itemSize;

    if (blIsVarArrayImplType(selfI->implType)) {
      BLVariantImpl* oldI = static_cast<BLVariant*>(data)->impl;
      static_cast<BLVariant*>(data)->impl = blImplIncRef(static_cast<const BLVariant*>(item)->impl);
      return blVariantImplRelease(oldI);
    }
    else {
      blMemCopyInline(data, item, itemSize);
      return BL_SUCCESS;
    }
  }
}

BLResult blArrayReplaceView(BLArrayCore* self, const BLRange* range, const void* items, size_t n) noexcept {
  BLArrayImpl* selfI = self->impl;
  if (BL_UNLIKELY(!range))
    return blArrayAssignView(self, items, n);

  size_t size = selfI->size;
  size_t end = blMin(range->end, size);
  size_t index = blMin(range->start, end);
  size_t rangeSize = end - index;

  if (!rangeSize)
    return blArrayInsertView(self, index, items, n);

  size_t itemSize = selfI->itemSize;
  size_t tailSize = size - end;
  size_t sizeAfter = size - rangeSize + n;
  const BLArrayFuncs& funcs = blArrayFuncsByDispatchType(selfI->dispatchType);

  if (blImplIsMutable(selfI)) {
    // 0           |<-Start   End->|          | <- Size
    // ^***********^***************^**********^
    // | Unchanged |  Replacement  | TailSize |
    //
    // <  Less     |+++++++| <- MidEnd
    // == Equal    |+++++++++++++++| <- MidEnd
    // >  Greater  |++++++++++++++++++++++| <- MidEnd
    uint8_t* data = selfI->dataAs<uint8_t>();
    const uint8_t* itemsPtr = static_cast<const uint8_t*>(items);
    const uint8_t* itemsEnd = itemsPtr + itemSize * n;

    if (BL_LIKELY(itemsPtr >= data + size * n || itemsEnd <= data)) {
      // Non-overlaping case (the expected one).
      if (rangeSize == n) {
        funcs.replaceData(data + index * itemSize, items, n * itemSize);
      }
      else {
        funcs.destroyData(data + index * itemSize, rangeSize * itemSize);
        memmove(data + (index + rangeSize) * itemSize, data + end * itemSize, tailSize * itemSize);
        funcs.copyData(data + index * itemSize, items, n * itemSize);
        selfI->size = sizeAfter;
      }
      return BL_SUCCESS;
    }
  }

  // Array is either immmutable or the `items` overlap.
  size_t capacity = blArrayFittingCapacity(itemSize, sizeAfter);
  BLArrayImpl* newI = blArrayImplNew(selfI->implType, capacity);

  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  uint8_t* dst = newI->dataAs<uint8_t>();
  const uint8_t* src = selfI->dataAs<uint8_t>();

  funcs.copyData(dst, src, index * itemSize);
  dst += index * itemSize;
  src += (index + rangeSize) * itemSize;

  funcs.copyData(dst, items, n * itemSize);
  dst += n * itemSize;
  funcs.copyData(dst, src, tailSize * itemSize);

  newI->size = size;
  self->impl = newI;
  return blArrayImplRelease(selfI);
}

// ============================================================================
// [BLArray - Remove]
// ============================================================================

BLResult blArrayRemoveIndex(BLArrayCore* self, size_t index) noexcept {
  BLRange range(index, index + 1);
  return blArrayRemoveRange(self, &range);
}

BLResult blArrayRemoveRange(BLArrayCore* self, const BLRange* range) noexcept {
  if (BL_UNLIKELY(!range))
    return blArrayClear(self);

  BLArrayImpl* selfI = self->impl;
  size_t size = selfI->size;
  size_t itemSize = selfI->itemSize;

  size_t end = blMin(range->end, size);
  size_t index = blMin(range->start, end);
  size_t n = end - index;

  if (!n)
    return BL_SUCCESS;

  size_t sizeAfter = size - n;
  const BLArrayFuncs& funcs = blArrayFuncsByDispatchType(selfI->dispatchType);

  if (BL_UNLIKELY(!blImplIsMutable(selfI))) {
    size_t capacity = blArrayFittingCapacity(itemSize, sizeAfter);
    BLArrayImpl* newI = blArrayImplNew(selfI->implType, capacity);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    newI->size = sizeAfter;
    self->impl = newI;

    uint8_t* dst = newI->dataAs<uint8_t>();
    const uint8_t* src = selfI->dataAs<uint8_t>();

    funcs.copyData(dst, src, index * itemSize);
    funcs.copyData(dst + index * itemSize, src + end * itemSize, (size - end) * itemSize);

    return blArrayImplRelease(selfI);
  }
  else {
    uint8_t* data = selfI->dataAs<uint8_t>() + index * itemSize;
    selfI->size = sizeAfter;

    funcs.destroyData(data, n * itemSize);
    memmove(data, data + n * itemSize, (size - end) * itemSize);

    return BL_SUCCESS;
  }
}

// ===========================================================================
// [BLArray - Equals]
// ===========================================================================

bool blArrayEquals(const BLArrayCore* a, const BLArrayCore* b) noexcept {
  const BLArrayImpl* aI = a->impl;
  const BLArrayImpl* bI = b->impl;

  size_t size = aI->size;
  size_t itemSize = aI->itemSize;

  if ((aI->implType != bI->implType) | (size != bI->size))
    return false;

  if (aI->data == bI->data)
    return true;

  if (aI->dispatchType == 0)
    return memcmp(aI->data, bI->data, size * itemSize) == 0;

  const uint8_t* aPtr = aI->dataAs<uint8_t>();
  const uint8_t* bPtr = bI->dataAs<uint8_t>();
  const uint8_t* aEnd = aPtr + size * itemSize;

  while (aPtr != aEnd) {
    if (!reinterpret_cast<const BLVariant*>(a)->equals(*reinterpret_cast<const BLVariant*>(b)))
      return false;

    aPtr += itemSize;
    bPtr += itemSize;
  }

  return true;
}

// ===========================================================================
// [BLArray - Runtime Init]
// ===========================================================================

void blArrayRtInit(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);

  for (uint32_t implType = BL_IMPL_TYPE_ARRAY_FIRST; implType <= BL_IMPL_TYPE_ARRAY_LAST; implType++) {
    BLArrayImpl* arrayI = &blNullArrayImpl[implType];

    arrayI->implType = uint8_t(implType);
    arrayI->implTraits = uint8_t(BL_IMPL_TRAIT_NULL);
    arrayI->itemSize = uint8_t(blArrayItemSizeTable[implType]);
    arrayI->dispatchType = uint8_t(blArrayDispatchTypeByImplType(implType));
    arrayI->data = const_cast<uint8_t*>(blNullArrayBuffer);

    blAssignBuiltInNull(arrayI);
  }
}

// ============================================================================
// [BLArray - Unit Tests]
// ============================================================================

#if defined(BL_BUILD_TEST)
UNIT(blend2d_array) {
  BLArray<int> a;

  EXPECT(a.size() == 0);

  // [42]
  EXPECT(a.append(42) == BL_SUCCESS);
  EXPECT(a.size() == 1);
  EXPECT(a[0] == 42);

  // [42, 1, 2, 3]
  EXPECT(a.append(1, 2, 3) == BL_SUCCESS);
  EXPECT(a.size() == 4);
  EXPECT(a[0] == 42);
  EXPECT(a[1] == 1);
  EXPECT(a[2] == 2);
  EXPECT(a[3] == 3);

  // [10, 42, 1, 2, 3]
  EXPECT(a.prepend(10) == BL_SUCCESS);
  EXPECT(a.size() == 5);
  EXPECT(a[0] == 10);
  EXPECT(a[1] == 42);
  EXPECT(a[2] == 1);
  EXPECT(a[3] == 2);
  EXPECT(a[4] == 3);
  EXPECT(a.indexOf(4) == SIZE_MAX);
  EXPECT(a.indexOf(3) == 4);
  EXPECT(a.lastIndexOf(4) == SIZE_MAX);
  EXPECT(a.lastIndexOf(10) == 0);

  BLArray<int> b;
  EXPECT(b.append(10, 42, 1, 2, 3) == BL_SUCCESS);
  EXPECT(a.equals(b));
  EXPECT(b.append(99) == BL_SUCCESS);
  EXPECT(!a.equals(b));

  // [10, 3]
  EXPECT(a.remove(BLRange(1, 4)) == BL_SUCCESS);
  EXPECT(a.size() == 2);
  EXPECT(a[0] == 10);
  EXPECT(a[1] == 3);

  // [10, 33, 3]
  EXPECT(a.insert(1, 33) == BL_SUCCESS);
  EXPECT(a.size() == 3);
  EXPECT(a[0] == 10);
  EXPECT(a[1] == 33);
  EXPECT(a[2] == 3);

  // [10, 33, 3, 999, 1010, 2293]
  EXPECT(a.insert(2, 999, 1010, 2293) == BL_SUCCESS);
  EXPECT(a.size() == 6);
  EXPECT(a[0] == 10);
  EXPECT(a[1] == 33);
  EXPECT(a[2] == 999);
  EXPECT(a[3] == 1010);
  EXPECT(a[4] == 2293);
  EXPECT(a[5] == 3);
}
#endif
