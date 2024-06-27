// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "object_p.h"
#include "string_p.h"
#include "runtime_p.h"
#include "support/intops_p.h"
#include "support/memops_p.h"
#include "support/stringops_p.h"

#include <stdio.h>

namespace bl {
namespace StringInternal {

// bl::String - Private - Preconditions
// ====================================

static_assert(((BL_OBJECT_TYPE_STRING << BL_OBJECT_INFO_TYPE_SHIFT) & 0xFFFFu) == 0,
              "BL_OBJECT_TYPE_STRING must be a value that would not use any bits in the two lowest bytes in the "
              "object info, which can be used by BLString on little endian targets to store 13th and 14th byte.");

// bl::String - Private - Commons
// ==============================

static BL_INLINE constexpr size_t getMaximumSize() noexcept {
  return capacityFromImplSize(BLObjectImplSize(BL_OBJECT_IMPL_MAX_SIZE));
}

static BL_INLINE BLObjectImplSize expandImplSize(BLObjectImplSize implSize) noexcept {
  return blObjectExpandImplSize(implSize);
}

static BLObjectImplSize expandImplSizeWithModifyOp(BLObjectImplSize implSize, BLModifyOp modifyOp) noexcept {
  return blObjectExpandImplSizeWithModifyOp(implSize, modifyOp);
}

static BL_INLINE void setSSOSize(BLStringCore* self, size_t newSize) noexcept {
  self->_d.info.setAField(uint32_t(newSize) ^ BLString::kSSOCapacity);
}

static BL_INLINE void setSize(BLStringCore* self, size_t newSize) noexcept {
  BL_ASSERT(newSize <= getCapacity(self));
  if (self->_d.sso())
    setSSOSize(self, newSize);
  else
    getImpl(self)->size = newSize;
}

static BL_INLINE void clearSSOData(BLStringCore* self) noexcept {
  memset(self->_d.char_data, 0, blMax<size_t>(BLString::kSSOCapacity, BLObjectDetail::kStaticDataSize));
}

// bl::String - Private - Alloc & Free Impl
// ========================================

static BL_INLINE BLResult initSSO(BLStringCore* self, size_t size = 0u) noexcept {
  self->_d.initStatic(BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_STRING) |
                      BLObjectInfo::fromAbcp(uint32_t(size) ^ BLString::kSSOCapacity));
  return BL_SUCCESS;
}

static BL_INLINE BLResult initDynamic(BLStringCore* self, BLObjectImplSize implSize, size_t size = 0u) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_STRING);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLStringImpl>(self, info, implSize));

  BLStringImpl* impl = getImpl(self);
  impl->capacity = capacityFromImplSize(implSize);
  impl->size = size;
  impl->data()[size] = '\0';
  return BL_SUCCESS;
}

static BL_NOINLINE BLResult initString(BLStringCore* self, size_t size, size_t capacity, char** out) noexcept {
  BL_ASSERT(capacity >= size);

  if (capacity <= BLString::kSSOCapacity) {
    initSSO(self, size);
    *out = self->_d.char_data;
    return BL_SUCCESS;
  }
  else {
    BL_PROPAGATE(initDynamic(self, implSizeFromCapacity(size), size));
    *out = getImpl(self)->data();
    return BL_SUCCESS;
  }
}

static BL_NOINLINE BLResult initStringAndCopy(BLStringCore* self, size_t capacity, const char* str, size_t size) noexcept {
  BL_ASSERT(capacity >= size);
  BL_ASSERT(size != SIZE_MAX);

  char* dst;
  BL_PROPAGATE(initString(self, size, capacity, &dst));

  memcpy(dst, str, size);
  return BL_SUCCESS;
}

// bl::String - Private - Manipulation
// ===================================

static BLResult modifyAndCopy(BLStringCore* self, BLModifyOp op, const char* str, size_t n) noexcept {
  UnpackedData u = unpackData(self);
  size_t index = blModifyOpIsAppend(op) ? u.size : size_t(0);
  size_t sizeAfter = IntOps::uaddSaturate(index, n);
  size_t immutableMsk = IntOps::bitMaskFromBool<size_t>(!isInstanceMutable(self));

  if ((sizeAfter | immutableMsk) > u.capacity) {
    if (BL_UNLIKELY(sizeAfter > getMaximumSize()))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    // Use a temporary object to avoid possible overlaps with both 'self' and 'str'.
    BLStringCore newO;
    char* dst = nullptr;

    if (sizeAfter <= BLString::kSSOCapacity && !blModifyOpDoesGrow(op)) {
      initSSO(self, sizeAfter);
      dst = self->_d.char_data;
    }
    else {
      BLObjectImplSize implSize = expandImplSizeWithModifyOp(implSizeFromCapacity(sizeAfter), op);
      BL_PROPAGATE(initDynamic(&newO, implSize, sizeAfter));

      dst = getImpl(&newO)->data();
    }

    memcpy(dst, u.data, index);
    memcpy(dst + index, str, n);

    return replaceInstance(self, &newO);
  }

  memmove(u.data + index, str, n);
  u.data[sizeAfter] = '\0';

  if (self->_d.sso()) {
    setSSOSize(self, sizeAfter);
    if (blModifyOpIsAssign(op))
      MemOps::fillInlineT(u.data + sizeAfter, char(0), BLString::kSSOCapacity - sizeAfter);
    return BL_SUCCESS;
  }
  else {
    getImpl(self)->size = sizeAfter;
    return BL_SUCCESS;
  }
}

static BLResult insertAndCopy(BLStringCore* self, size_t index, const char* str, size_t n) noexcept {
  UnpackedData u = unpackData(self);
  size_t endIndex = index + n;
  size_t sizeAfter = IntOps::uaddSaturate(u.size, n);
  size_t immutableMsk = IntOps::bitMaskFromBool<size_t>(!isInstanceMutable(self));

  if ((sizeAfter | immutableMsk) > u.capacity) {
    if (BL_UNLIKELY(sizeAfter > getMaximumSize()))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    BLStringCore newO;
    BLObjectImplSize implSize = expandImplSize(implSizeFromCapacity(sizeAfter));
    BL_PROPAGATE(initDynamic(&newO, implSize, sizeAfter));

    char* dst = getImpl(&newO)->data();
    memcpy(dst, u.data, index);
    memcpy(dst + endIndex, u.data +  index, u.size - index);
    memcpy(dst + index, str, n);

    return replaceInstance(self, &newO);
  }
  else {
    setSize(self, sizeAfter);

    char* dst = u.data;
    char* dstEnd = dst + u.size;

    // The destination would point into the first byte that will be modified. So for example if the
    // data is `[ABCDEF]` and we are inserting at index 1 then the `dst` would point to `[BCDEF]`.
    dst += index;
    dstEnd += n;

    // Move the memory in-place making space for items to insert. For example if the destination points
    // to [ABCDEF] and we want to insert 4 items we  would get [____ABCDEF].
    //
    // NOTE: +1 includes a NULL terminator.
    memmove(dst + n, dst, u.size - index + 1);

    // Split the [str:strEnd] into LEAD and TRAIL slices and shift TRAIL slice in a way to cancel the `memmove()` if
    // `str` overlaps `dst`. In practice if there is an overlap the [str:strEnd] source should be within [dst:dstEnd]
    // as it doesn't make sense to insert something which is outside of the current valid area.
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

    // Leading area precedes `dst` - nothing changed in here and if this is the whole area then there was no overlap
    // that we would have to deal with.
    size_t nLeadBytes = 0;
    if (str < dst) {
      nLeadBytes = blMin<size_t>((size_t)(dst - str), n);
      memcpy(dst, str, nLeadBytes);

      dst += nLeadBytes;
      str += nLeadBytes;
    }

    // Trailing area - we either shift none or all of it.
    if (str < dstEnd)
      str += n; // Shift source in case of overlap.

    memcpy(dst, str, n - nLeadBytes);
    return BL_SUCCESS;
  }
}

} // {StringInternal}
} // {bl}

// bl::String - API - Construction & Destruction
// =============================================

BL_API_IMPL BLResult blStringInit(BLStringCore* self) noexcept {
  using namespace bl::StringInternal;

  initSSO(self);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blStringInitMove(BLStringCore* self, BLStringCore* other) noexcept {
  using namespace bl::StringInternal;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isString());

  self->_d = other->_d;
  initSSO(other);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blStringInitWeak(BLStringCore* self, const BLStringCore* other) noexcept {
  using namespace bl::StringInternal;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isString());

  self->_d = other->_d;
  return retainInstance(self);
}

BL_API_IMPL BLResult blStringInitWithData(BLStringCore* self, const char* str, size_t size) noexcept {
  using namespace bl::StringInternal;

  if (size == SIZE_MAX)
    size = strlen(str);

  BLResult result = initStringAndCopy(self, size, str, size);
  if (result != BL_SUCCESS)
    initSSO(self);
  return result;
}

BL_API_IMPL BLResult blStringDestroy(BLStringCore* self) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  return releaseInstance(self);
}

// bl::String - API - Common Functionality
// =======================================

BL_API_IMPL BLResult blStringReset(BLStringCore* self) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  releaseInstance(self);
  initSSO(self);

  return BL_SUCCESS;
}

// bl::String - API - Accessors
// ============================

BL_API_IMPL const char* blStringGetData(const BLStringCore* self) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  return getData(self);
}

BL_API_IMPL size_t blStringGetSize(const BLStringCore* self) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  return getSize(self);
}

BL_API_IMPL size_t blStringGetCapacity(const BLStringCore* self) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  return getCapacity(self);
}

// bl::String - API - Data Manipulation - Storage Management
// =========================================================

BL_API_IMPL BLResult blStringClear(BLStringCore* self) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  if (self->_d.sso()) {
    size_t size = getSSOSize(self);

    if (size) {
      clearSSOData(self);
      setSSOSize(self, 0);
    }

    return BL_SUCCESS;
  }
  else {
    BLStringImpl* selfI = getImpl(self);

    if (!isImplMutable(selfI)) {
      releaseInstance(self);
      initSSO(self);

      return BL_SUCCESS;
    }

    if (selfI->size) {
      selfI->size = 0;
      selfI->data()[0] = '\0';
    }

    return BL_SUCCESS;
  }
}

BL_API_IMPL BLResult blStringShrink(BLStringCore* self) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  if (self->_d.sso())
    return BL_SUCCESS;

  BLStringImpl* selfI = getImpl(self);
  if (!bl::ObjectInternal::isImplRefCounted(selfI))
    return BL_SUCCESS;

  const char* data = selfI->data();
  size_t size = selfI->size;

  if (size <= BLString::kSSOCapacity || size + BL_OBJECT_IMPL_ALIGNMENT <= selfI->capacity) {
    // Use static storage if the string is small enough to hold the data. Only try to reduce the capacity if the string
    // is dynamic and reallocating the storage would save at least a single cache line, otherwise we would end up most
    // likely with a similar size of the Impl.
    BLStringCore tmp;
    BL_PROPAGATE(initStringAndCopy(&tmp, size, data, size));
    return replaceInstance(self, &tmp);
  }

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blStringReserve(BLStringCore* self, size_t n) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  UnpackedData u = unpackData(self);
  size_t immutableMask = bl::IntOps::bitMaskFromBool<size_t>(!isInstanceMutable(self));

  if ((n | immutableMask) <= u.capacity)
    return BL_SUCCESS;

  BLStringCore newO;
  BL_PROPAGATE(initDynamic(&newO, implSizeFromCapacity(blMax(u.size, n)), u.size));

  char* dst = getImpl(&newO)->data();
  memcpy(dst, u.data, u.size);
  return replaceInstance(self, &newO);
}

BL_API_IMPL BLResult blStringResize(BLStringCore* self, size_t n, char fill) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  UnpackedData u = unpackData(self);
  if (n <= u.size) {
    if (n == u.size)
      return BL_SUCCESS;

    // If `n` is lesser than the current `size` it's a truncation.
    if (!isInstanceMutable(self)) {
      BLStringCore newO;
      BL_PROPAGATE(initStringAndCopy(&newO, n, u.data, n));
      return replaceInstance(self, &newO);
    }
    else {
      if (self->_d.sso()) {
        // Clears all unused bytes in the SSO storage.
        bl::MemOps::fillInlineT<char>(u.data + n, '\0', u.size - n);
        setSSOSize(self, n);
        return BL_SUCCESS;
      }
      else {
        BLStringImpl* impl = getImpl(self);
        impl->size = n;
        impl->data()[n] = '\0';
        return BL_SUCCESS;
      }
    }
  }
  else {
    n -= u.size;
    char* dst;
    BL_PROPAGATE(blStringModifyOp(self, BL_MODIFY_OP_APPEND_FIT, n, &dst));

    memset(dst, int((unsigned char)fill), n);
    return BL_SUCCESS;
  }
}

// bl::String - API - Data Manipulation - Modify Operations
// ========================================================

BL_API_IMPL BLResult blStringMakeMutable(BLStringCore* self, char** dataOut) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  if (self->_d.sso()) {
    *dataOut = self->_d.char_data;
    return BL_SUCCESS;
  }

  BLStringImpl* selfI = getImpl(self);
  if (isImplMutable(selfI)) {
    *dataOut = selfI->data();
    return BL_SUCCESS;
  }

  // Temporarily store it here as we need to create a new instance on 'self' to be able to return `dataOut` ptr.
  BLStringCore tmp = *self;
  size_t size = selfI->size;
  BL_PROPAGATE(initStringAndCopy(self, size, selfI->data(), size));

  *dataOut = getData(self);
  return releaseInstance(&tmp);
}

BL_API_IMPL BLResult blStringModifyOp(BLStringCore* self, BLModifyOp op, size_t n, char** dataOut) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  UnpackedData u = unpackData(self);
  size_t index = blModifyOpIsAppend(op) ? u.size : size_t(0);
  size_t sizeAfter = bl::IntOps::uaddSaturate(index, n);
  size_t immutableMsk = bl::IntOps::bitMaskFromBool<size_t>(!isInstanceMutable(self));

  if ((sizeAfter | immutableMsk) > u.capacity) {
    BLStringCore tmp = *self;
    char* dst = nullptr;
    const char* src = getData(&tmp);

    if (sizeAfter <= BLString::kSSOCapacity && !blModifyOpDoesGrow(op)) {
      initSSO(self, sizeAfter);
      dst = self->_d.char_data;
    }
    else {
      *dataOut = nullptr;

      if (BL_UNLIKELY(sizeAfter > getMaximumSize()))
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

      BLObjectImplSize implSize = expandImplSizeWithModifyOp(implSizeFromCapacity(sizeAfter), op);
      BL_PROPAGATE(initDynamic(self, implSize, sizeAfter));

      dst = getImpl(self)->data();
    }

    *dataOut = dst + index;
    memcpy(dst, src, index);
    dst[sizeAfter] = '\0';

    return releaseInstance(&tmp);
  }

  *dataOut = u.data + index;
  u.data[sizeAfter] = '\0';

  if (self->_d.sso()) {
    setSSOSize(self, sizeAfter);
    if (blModifyOpIsAssign(op))
      clearSSOData(self);
    return BL_SUCCESS;
  }
  else {
    getImpl(self)->size = sizeAfter;
    return BL_SUCCESS;
  }
}

BL_API_IMPL BLResult blStringInsertOp(BLStringCore* self, size_t index, size_t n, char** dataOut) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  UnpackedData u = unpackData(self);
  size_t sizeAfter = bl::IntOps::uaddSaturate(u.size, n);
  size_t immutableMsk = bl::IntOps::bitMaskFromBool<size_t>(!isInstanceMutable(self));

  if ((sizeAfter | immutableMsk) > u.capacity) {
    *dataOut = nullptr;

    if (BL_UNLIKELY(sizeAfter > getMaximumSize()))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    BLStringCore newO;
    BLObjectImplSize implSize = expandImplSize(implSizeFromCapacity(sizeAfter));
    BL_PROPAGATE(initDynamic(&newO, implSize, sizeAfter));

    char* dst = getImpl(&newO)->data();
    memcpy(dst, u.data, index);
    memcpy(dst + index + n, u.data + index, u.size - index);

    *dataOut = dst + index;
    return replaceInstance(self, &newO);
  }
  else {
    setSize(self, sizeAfter);
    memmove(u.data + index + n, u.data + index, u.size - index);
    u.data[sizeAfter] = '\0';

    *dataOut = u.data + index;
    return BL_SUCCESS;
  }
}

// bl::String - API - Data Manipulation - Assignment
// =================================================

BL_API_IMPL BLResult blStringAssignMove(BLStringCore* self, BLStringCore* other) noexcept {
  using namespace bl::StringInternal;

  BL_ASSERT(self->_d.isString());
  BL_ASSERT(other->_d.isString());

  BLStringCore tmp = *other;
  initSSO(other);
  return replaceInstance(self, &tmp);
}

BL_API_IMPL BLResult blStringAssignWeak(BLStringCore* self, const BLStringCore* other) noexcept {
  using namespace bl::StringInternal;

  BL_ASSERT(self->_d.isString());
  BL_ASSERT(other->_d.isString());

  bl::ObjectInternal::retainInstance(other);
  return replaceInstance(self, other);
}

BL_API_IMPL BLResult blStringAssignDeep(BLStringCore* self, const BLStringCore* other) noexcept {
  using namespace bl::StringInternal;

  BL_ASSERT(self->_d.isString());
  BL_ASSERT(other->_d.isString());

  return modifyAndCopy(self, BL_MODIFY_OP_ASSIGN_FIT, getData(other), getSize(other));
}

BL_API_IMPL BLResult blStringAssignData(BLStringCore* self, const char* str, size_t n) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  if (n == SIZE_MAX)
    n = strlen(str);

  return modifyAndCopy(self, BL_MODIFY_OP_ASSIGN_FIT, str, n);
}

// bl::String - API - Data Manipulation - ApplyOp
// ==============================================

BL_API_IMPL BLResult blStringApplyOpChar(BLStringCore* self, BLModifyOp op, char c, size_t n) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  char* dst;
  BL_PROPAGATE(blStringModifyOp(self, op, n, &dst));

  memset(dst, int((unsigned char)c), n);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blStringApplyOpData(BLStringCore* self, BLModifyOp op, const char* str, size_t n) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  if (n == SIZE_MAX)
    n = strlen(str);

  return modifyAndCopy(self, op, str, n);
}

BL_API_IMPL BLResult blStringApplyOpString(BLStringCore* self, BLModifyOp op, const BLStringCore* other) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  return modifyAndCopy(self, op, getData(other), getSize(other));
}

BL_API_IMPL BLResult blStringApplyOpFormatV(BLStringCore* self, BLModifyOp op, const char* fmt, va_list ap) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  UnpackedData u = unpackData(self);
  size_t index = blModifyOpIsAppend(op) ? u.size : size_t(0);
  size_t remaining = u.capacity - index;
  size_t mutableMsk = bl::IntOps::bitMaskFromBool<size_t>(isInstanceMutable(self));

  char buf[1024];
  int fmtResult;
  size_t outputSize;

  va_list apCopy;
  va_copy(apCopy, ap);

  if ((remaining & mutableMsk) >= 64) {
    // vsnprintf() expects null terminator to be included in the size of the buffer.
    char* dst = u.data;
    fmtResult = vsnprintf(dst + index, remaining + 1, fmt, ap);

    if (BL_UNLIKELY(fmtResult < 0))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    outputSize = size_t(unsigned(fmtResult));
    if (BL_LIKELY(outputSize <= remaining)) {
      // `vsnprintf` must write a null terminator, verify it's true.
      BL_ASSERT(dst[index + outputSize] == '\0');

      setSize(self, index + outputSize);
      return BL_SUCCESS;
    }
  }
  else {
    fmtResult = vsnprintf(buf, BL_ARRAY_SIZE(buf), fmt, ap);
    if (BL_UNLIKELY(fmtResult < 0))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    // If the `outputSize` is less than our buffer size then we are fine and the formatted text is already in the
    // buffer. Since `vsnprintf` doesn't include null-terminator in the returned size we cannot use '<=' as that
    // would mean that the last character written by `vsnprintf` was truncated.
    outputSize = size_t(unsigned(fmtResult));
    if (BL_LIKELY(outputSize < BL_ARRAY_SIZE(buf)))
      return blStringApplyOpData(self, op, buf, outputSize);
  }

  // If we are here it means that the string is either not large enough to hold the formatted text or it's not
  // mutable. In both cases we have to allocate a new buffer and call `vsnprintf` again.
  size_t sizeAfter = bl::IntOps::uaddSaturate(index, outputSize);
  if (BL_UNLIKELY(sizeAfter > getMaximumSize()))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLStringCore newO;
  BLObjectImplSize implSize = expandImplSizeWithModifyOp(implSizeFromCapacity(sizeAfter), op);
  BL_PROPAGATE(initDynamic(&newO, implSize, sizeAfter));

  char* dst = getImpl(&newO)->data();

  // This should always match. If it doesn't then it means that some other thread must have changed some value where
  // `apCopy` points and it caused `vsnprintf` to format a different string. If this happens we fail as there is no
  // reason to try again...
  fmtResult = vsnprintf(dst + index, outputSize + 1, fmt, apCopy);

  if (BL_UNLIKELY(size_t(unsigned(fmtResult)) != outputSize)) {
    releaseInstance(&newO);
    return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  memcpy(dst, u.data, index);
  return replaceInstance(self, &newO);
}

BL_API_IMPL BLResult blStringApplyOpFormat(BLStringCore* self, BLModifyOp op, const char* fmt, ...) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  BLResult result;
  va_list ap;

  va_start(ap, fmt);
  result = blStringApplyOpFormatV(self, op, fmt, ap);
  va_end(ap);

  return result;
}

// bl::String - API - Data Manipulation - Insert
// =============================================

BL_API_IMPL BLResult blStringInsertChar(BLStringCore* self, size_t index, char c, size_t n) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  char* dst;
  BL_PROPAGATE(blStringInsertOp(self, index, n, &dst));

  memset(dst, int((unsigned char)c), n);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blStringInsertData(BLStringCore* self, size_t index, const char* str, size_t n) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  if (n == SIZE_MAX)
    n = strlen(str);

  return insertAndCopy(self, index, str, n);
}

BL_API_IMPL BLResult blStringInsertString(BLStringCore* self, size_t index, const BLStringCore* other) noexcept {
  using namespace bl::StringInternal;

  BL_ASSERT(self->_d.isString());
  BL_ASSERT(other->_d.isString());

  if (self != other) {
    return insertAndCopy(self, index, getData(other), getSize(other));
  }
  else {
    BLStringCore copy(*other);
    return insertAndCopy(self, index, getData(&copy), getSize(&copy));
  }
}

// bl::String - API - Data Manipulation - Remove
// =============================================

BL_API_IMPL BLResult blStringRemoveIndex(BLStringCore* self, size_t index) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  return blStringRemoveRange(self, index, index + 1);
}

BL_API_IMPL BLResult blStringRemoveRange(BLStringCore* self, size_t rStart, size_t rEnd) noexcept {
  BL_ASSERT(self->_d.isString());
  using namespace bl::StringInternal;

  size_t size = getSize(self);
  size_t end = blMin(rEnd, size);
  size_t index = blMin(rStart, end);

  size_t n = end - index;
  size_t sizeAfter = size - n;

  if (!n)
    return BL_SUCCESS;

  if (self->_d.sso()) {
    char* data = self->_d.char_data;
    bl::MemOps::copySmall(data + index, data + index + n, size - end);
    bl::MemOps::fillSmallT(data + sizeAfter, char(0), BLString::kSSOCapacity - sizeAfter);

    setSSOSize(self, sizeAfter);
    return BL_SUCCESS;
  }

  BLStringImpl* selfI = getImpl(self);
  if (isImplMutable(selfI)) {
    // Copy one more byte that acts as a NULL terminator.
    char* data = selfI->data();
    memmove(data + index, data + index + n, size - end + 1);

    selfI->size = sizeAfter;
    return BL_SUCCESS;
  }

  BLStringCore tmp = *self;
  char* dst;
  BL_PROPAGATE(initString(self, sizeAfter, sizeAfter, &dst));

  const char* src = getData(&tmp);
  memcpy(dst, src, index);
  memcpy(dst + index, src + end, size - end);

  return releaseInstance(&tmp);
}

// bl::String - API - Equality / Comparison
// ========================================

BL_API_IMPL bool blStringEquals(const BLStringCore* a, const BLStringCore* b) noexcept {
  using namespace bl::StringInternal;

  BL_ASSERT(a->_d.isString());
  BL_ASSERT(b->_d.isString());

  UnpackedData au = unpackData(a);
  UnpackedData bu = unpackData(b);

  if (au.size != bu.size)
    return false;

  return memcmp(au.data, bu.data, au.size) == 0;
}

BL_API_IMPL bool blStringEqualsData(const BLStringCore* self, const char* str, size_t n) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  const char* aData = getData(self);
  const char* bData = str;

  size_t size = getSize(self);
  if (n == SIZE_MAX) {
    // Null terminated, we don't know the size of `str`.
    size_t i;
    for (i = 0; i < size; i++)
      if ((aData[i] != bData[i]) | (bData[i] == 0))
        return false;
    return bData[i] == 0;
  }
  else {
    if (size != n)
      return false;
    return memcmp(aData, bData, size) == 0;
  }
}

BL_API_IMPL int blStringCompare(const BLStringCore* a, const BLStringCore* b) noexcept {
  using namespace bl::StringInternal;

  BL_ASSERT(a->_d.isString());
  BL_ASSERT(b->_d.isString());

  UnpackedData au = unpackData(a);
  UnpackedData bu = unpackData(b);

  size_t minSize = blMin(au.size, bu.size);
  int c = memcmp(au.data, bu.data, minSize);

  if (c)
    return c;

  return au.size < bu.size ? -1 : int(au.size > bu.size);
}

BL_API_IMPL int blStringCompareData(const BLStringCore* self, const char* str, size_t n) noexcept {
  using namespace bl::StringInternal;
  BL_ASSERT(self->_d.isString());

  UnpackedData u = unpackData(self);
  size_t aSize = u.size;

  const char* aData = u.data;
  const char* bData = str;

  if (n == SIZE_MAX) {
    // Null terminated: We don't know the size of `str`, thus we cannot use strcmp() as BLString content can be
    // arbitrary, so strcmp() won't work if the string holds zeros (aka null terminators).
    size_t i;

    for (i = 0; i < aSize; i++) {
      int a = uint8_t(aData[i]);
      int b = uint8_t(bData[i]);

      int c = a - b;

      // If we found a null terminator in 'b' it means that so far the strings were equal, but now we are at the end
      // of 'b', however, there is still some content in 'a'. This would mean that `a > b` like "abc?" > "abc".
      if (b == 0)
        c = 1;

      if (c)
        return c;
    }

    // We are at the end of 'a'. If this is also the end of 'b' then these strings are equal and we return zero. If
    // 'b' doesn't point to a null terminator then `a < b`.
    return -int(bData[i] != 0);
  }
  else {
    size_t bSize = n;
    size_t minSize = blMin(aSize, bSize);

    int c = memcmp(aData, bData, minSize);
    if (c)
      return c;

    return aSize < bSize ? -1 : int(aSize > bSize);
  }
}

// bl::String - Runtime Registration
// =================================

void blStringRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);
  bl::StringInternal::initSSO(static_cast<BLStringCore*>(&blObjectDefaults[BL_OBJECT_TYPE_STRING]));
}
