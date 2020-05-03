// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#include "./api-build_p.h"
#include "./array_p.h"
#include "./string_p.h"
#include "./runtime_p.h"

// ============================================================================
// [Global Variables]
// ============================================================================

static BLWrap<BLStringImpl> blNullStringImpl;
static const char blNullStringData[1] = "";

// ============================================================================
// [BLString - Forward Declarations]
// ============================================================================

static BLResult blStringModifyAndCopy(BLStringCore* self, uint32_t op, const char* str, size_t n) noexcept;

// ============================================================================
// [BLString - Internal]
// ============================================================================

static constexpr size_t blStringImplSizeOf(size_t n = 0) noexcept {
  return blContainerSizeOf(sizeof(BLStringImpl) + 1, 1, n);
}

static constexpr size_t blStringCapacityOf(size_t implSize) noexcept {
  return blContainerCapacityOf(blStringImplSizeOf(), 1, implSize);
}

static constexpr size_t blStringMaximumCapacity() noexcept {
  return blStringCapacityOf(SIZE_MAX);
}

static BL_INLINE size_t blStringFittingCapacity(size_t n) noexcept {
  return blContainerFittingCapacity(blStringImplSizeOf(), 1, n);
}

static BL_INLINE size_t blStringGrowingCapacity(size_t n) noexcept {
  return blContainerGrowingCapacity(blStringImplSizeOf(), 1, n, BL_ALLOC_HINT_STRING);
}

static BL_INLINE BLStringImpl* blStringImplNew(size_t n) noexcept {
  uint16_t memPoolData;
  BLStringImpl* impl = blRuntimeAllocImplT<BLStringImpl>(blStringImplSizeOf(n), &memPoolData);

  if (BL_UNLIKELY(!impl))
    return impl;

  blImplInit(impl, BL_IMPL_TYPE_STRING, BL_IMPL_TRAIT_MUTABLE, memPoolData);
  impl->capacity = n;
  impl->reserved = 0;
  impl->data = blOffsetPtr<char>(impl, sizeof(BLStringImpl));
  impl->size = 0;
  impl->data[0] = '\0';

  return impl;
}

// Cannot be static, called by `BLVariant` implementation.
BLResult blStringImplDelete(BLStringImpl* impl) noexcept {
  uint8_t* implBase = reinterpret_cast<uint8_t*>(impl);
  size_t implSize = blStringImplSizeOf(impl->capacity);
  uint32_t implTraits = impl->implTraits;
  uint32_t memPoolData = impl->memPoolData;

  if (implTraits & BL_IMPL_TRAIT_EXTERNAL) {
    implSize = blStringImplSizeOf() + sizeof(BLExternalImplPreface);
    implBase -= sizeof(BLExternalImplPreface);
    blImplDestroyExternal(impl);
  }

  if (implTraits & BL_IMPL_TRAIT_FOREIGN)
    return BL_SUCCESS;
  else
    return blRuntimeFreeImpl(implBase, implSize, memPoolData);
}

static BL_INLINE BLResult blStringImplRelease(BLStringImpl* impl) noexcept {
  if (blImplDecRefAndTest(impl))
    return blStringImplDelete(impl);
  return BL_SUCCESS;
}

static BL_NOINLINE BLResult blStringRealloc(BLStringCore* self, size_t n) noexcept {
  BLStringImpl* oldI = self->impl;
  BLStringImpl* newI = blStringImplNew(n);

  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  size_t size = oldI->size;
  BL_ASSERT(size <= n);

  self->impl = newI;
  newI->size = size;

  char* dst = newI->data;
  memcpy(dst, oldI->data, size);
  dst[size] = '\0';

  return blStringImplRelease(oldI);
}

// ============================================================================
// [BLString - Init / Destroy]
// ============================================================================

BLResult blStringInit(BLStringCore* self) noexcept {
  self->impl = &blNullStringImpl;
  return BL_SUCCESS;
}

BLResult blStringInitWithData(BLStringCore* self, const char* str, size_t size) noexcept {
  if (size == SIZE_MAX)
    size = strlen(str);

  self->impl = &blNullStringImpl;
  if (size == 0)
    return BL_SUCCESS;

  return blStringModifyAndCopy(self, BL_MODIFY_OP_ASSIGN_FIT, str, size);
}

BLResult blStringDestroy(BLStringCore* self) noexcept {
  BLStringImpl* selfI = self->impl;
  self->impl = nullptr;
  return blStringImplRelease(selfI);
}

// ============================================================================
// [BLString - Reset]
// ============================================================================

BLResult blStringReset(BLStringCore* self) noexcept {
  BLStringImpl* selfI = self->impl;
  self->impl = &blNullStringImpl;
  return blStringImplRelease(selfI);
}

// ============================================================================
// [BLString - Storage]
// ============================================================================

size_t blStringGetSize(const BLStringCore* self) noexcept {
  return self->impl->size;
}

size_t blStringGetCapacity(const BLStringCore* self) BL_NOEXCEPT_C {
  return self->impl->capacity;
}

const char* blStringGetData(const BLStringCore* self) BL_NOEXCEPT_C {
  return self->impl->data;
}

BLResult blStringClear(BLStringCore* self) noexcept {
  BLStringImpl* selfI = self->impl;

  if (!blImplIsMutable(selfI)) {
    self->impl = &blNullStringImpl;
    return blStringImplRelease(selfI);
  }
  else {
    selfI->size = 0;
    selfI->data[0] = '\0';
    return BL_SUCCESS;
  }
}

BLResult blStringShrink(BLStringCore* self) noexcept {
  BLStringImpl* selfI = self->impl;
  size_t size = selfI->size;

  if (!size) {
    self->impl = &blNullStringImpl;
    return blStringImplRelease(selfI);
  }

  size_t capacity = blStringFittingCapacity(size);
  if (capacity >= selfI->capacity)
    return BL_SUCCESS;

  return blStringRealloc(self, capacity);
}

BLResult blStringReserve(BLStringCore* self, size_t n) noexcept {
  BLStringImpl* selfI = self->impl;
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  if ((n | immutableMsk) > selfI->capacity) {
    if (BL_UNLIKELY(n > blStringMaximumCapacity()))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity = blStringFittingCapacity(blMax(n, selfI->size));
    return blStringRealloc(self, capacity);
  }

  return BL_SUCCESS;
}

BLResult blStringResize(BLStringCore* self, size_t n, char fill) noexcept {
  BLStringImpl* selfI = self->impl;
  size_t size = selfI->size;

  // If `n` is smaller than the current `size` then this is a truncation.
  if (n <= size) {
    if (!blImplIsMutable(selfI)) {
      if (n == size)
        return BL_SUCCESS;

      size_t capacity = blStringFittingCapacity(n);
      BLStringImpl* newI = blStringImplNew(capacity);

      if (BL_UNLIKELY(!newI))
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

      newI->size = n;
      self->impl = newI;

      char* dst = newI->data;
      char* src = selfI->data;

      memcpy(dst, src, n);
      dst[n] = '\0';

      return blStringImplRelease(selfI);
    }
    else {
      char* data = selfI->data;

      selfI->size = n;
      data[n] = '\0';
      return BL_SUCCESS;
    }
  }
  else {
    n -= size;
    char* dst;
    BL_PROPAGATE(blStringModifyOp(self, BL_MODIFY_OP_APPEND_FIT, n, &dst));

    memset(dst, int((unsigned char)fill), n);
    return BL_SUCCESS;
  }
}

// ============================================================================
// [BLString - Op]
// ============================================================================

BLResult blStringMakeMutable(BLStringCore* self, char** dataOut) noexcept {
  BLStringImpl* selfI = self->impl;

  if (!blImplIsMutable(selfI)) {
    size_t size = selfI->size;
    size_t capacity = blMax(blStringFittingCapacity(size),
                            blStringCapacityOf(BL_ALLOC_HINT_ARRAY));

    BL_PROPAGATE(blStringRealloc(self, capacity));
    selfI = self->impl;
  }

  *dataOut = selfI->data;
  return BL_SUCCESS;
}

BLResult blStringModifyOp(BLStringCore* self, uint32_t op, size_t n, char** dataOut) noexcept {
  BLStringImpl* selfI = self->impl;

  size_t size = selfI->size;
  size_t index = (op >= BL_MODIFY_OP_APPEND_START) ? size : size_t(0);
  size_t sizeAfter = blUAddSaturate(index, n);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  if ((sizeAfter | immutableMsk) > selfI->capacity) {
    if (BL_UNLIKELY(sizeAfter > blStringMaximumCapacity()))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity =
      (op & BL_MODIFY_OP_GROW_MASK)
        ? blStringGrowingCapacity(sizeAfter)
        : blStringFittingCapacity(sizeAfter);

    BLStringImpl* newI = blStringImplNew(capacity);
    if (BL_UNLIKELY(!newI)) {
      *dataOut = nullptr;
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
    }

    self->impl = newI;
    newI->size = sizeAfter;

    char* dst = newI->data;
    char* src = selfI->data;

    *dataOut = dst + index;
    memcpy(dst, src, index);
    dst[sizeAfter] = '\0';

    return blStringImplRelease(selfI);
  }
  else {
    char* data = selfI->data;

    *dataOut = data + index;
    selfI->size = sizeAfter;

    data[sizeAfter] = '\0';
    return BL_SUCCESS;
  }
}

static BLResult blStringModifyAndCopy(BLStringCore* self, uint32_t op, const char* str, size_t n) noexcept {
  BLStringImpl* selfI = self->impl;

  size_t size = selfI->size;
  size_t index = (op >= BL_MODIFY_OP_APPEND_START) ? size : size_t(0);
  size_t sizeAfter = blUAddSaturate(index, n);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  if ((sizeAfter | immutableMsk) > selfI->capacity) {
    if (BL_UNLIKELY(sizeAfter > blStringMaximumCapacity()))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity =
      (op & BL_MODIFY_OP_GROW_MASK)
        ? blStringGrowingCapacity(sizeAfter)
        : blStringFittingCapacity(sizeAfter);

    BLStringImpl* newI = blStringImplNew(capacity);
    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    self->impl = newI;
    newI->size = sizeAfter;

    char* dst = newI->data;
    char* src = selfI->data;

    memcpy(dst, src, index);
    memcpy(dst + index, str, n);
    dst[sizeAfter] = '\0';

    return blStringImplRelease(selfI);
  }
  else {
    char* data = selfI->data;

    selfI->size = sizeAfter;
    memmove(data + index, str, n);

    data[sizeAfter] = '\0';
    return BL_SUCCESS;
  }
}

BLResult blStringInsertOp(BLStringCore* self, size_t index, size_t n, char** dataOut) noexcept {
  BLStringImpl* selfI = self->impl;

  size_t size = selfI->size;
  size_t sizeAfter = blUAddSaturate(size, n);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  if ((sizeAfter | immutableMsk) > selfI->capacity) {
    if (BL_UNLIKELY(sizeAfter > blStringMaximumCapacity()))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity = blStringGrowingCapacity(sizeAfter);
    BLStringImpl* newI = blStringImplNew(capacity);

    if (BL_UNLIKELY(!newI)) {
      *dataOut = nullptr;
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
    }

    self->impl = newI;
    newI->size = sizeAfter;

    char* dst = newI->data;
    char* src = selfI->data;

    *dataOut = dst + index;
    memcpy(dst, src, index);
    memcpy(dst + index + n, src+ index, size - index);
    dst[sizeAfter] = '\0';

    return blStringImplRelease(selfI);
  }
  else {
    char* data = selfI->data;

    selfI->size = sizeAfter;
    memmove(data + index + n, data + index, size - index);

    data[sizeAfter] = '\0';
    return BL_SUCCESS;
  }
}

static BLResult blStringInsertAndCopy(BLStringCore* self, size_t index, const char* str, size_t n) noexcept {
  BLStringImpl* selfI = self->impl;

  size_t size = selfI->size;
  size_t sizeAfter = blUAddSaturate(size, n);

  size_t endIndex = index + n;
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  if ((sizeAfter | immutableMsk) > selfI->capacity) {
    if (BL_UNLIKELY(sizeAfter > blStringMaximumCapacity()))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity = blStringGrowingCapacity(sizeAfter);
    BLStringImpl* newI = blStringImplNew(capacity);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    char* dst = newI->data;
    char* src = selfI->data;

    // NOTE: +1 includes a NULL terminator.
    memcpy(dst, src, index);
    memcpy(dst + endIndex, src +  index, size - index + 1);

    self->impl = newI;
    newI->size = sizeAfter;

    memcpy(dst + index, str, n);

    return blStringImplRelease(selfI);
  }
  else {
    selfI->size = sizeAfter;

    char* dst = selfI->data;
    char* dstEnd = dst + size;

    // The destination would point into the first byte that will be modified.
    // So for example if the data is `[ABCDEF]` and we are inserting at index
    // 1 then the `dst` would point to `[BCDEF]`.
    dst += index;
    dstEnd += n;

    // Move the memory in-place making space for items to insert. For example
    // if the destination points to [ABCDEF] and we want to insert 4 items we
    // would get [____ABCDEF].
    //
    // NOTE: +1 includes a NULL terminator.
    memmove(dst + n, dst, size - index + 1);

    // Split the [str:strEnd] into LEAD and TRAIL slices and shift TRAIL slice
    // in a way to cancel the `memmove()` if `str` overlaps `dst`. In practice
    // if there is an overlap the [str:strEnd] source should be within [dst:dstEnd]
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
    // [abcdBCDEFGHefgh]

    // Leading area precedes `dst` - nothing changed in here and if this is
    // the whole area then there was no overlap that we would have to deal with.
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

// ============================================================================
// [BLString - Assign]
// ============================================================================

BLResult blStringAssignMove(BLStringCore* self, BLStringCore* other) noexcept {
  BLStringImpl* selfI = self->impl;
  BLStringImpl* otherI = other->impl;

  self->impl = otherI;
  other->impl = &blNullStringImpl;

  return blStringImplRelease(selfI);
}

BLResult blStringAssignWeak(BLStringCore* self, const BLStringCore* other) noexcept {
  BLStringImpl* selfI = self->impl;
  BLStringImpl* otherI = other->impl;

  self->impl = blImplIncRef(otherI);
  return blStringImplRelease(selfI);
}

BLResult blStringAssignDeep(BLStringCore* self, const BLStringCore* other) noexcept {
  const BLStringImpl* otherI = other->impl;
  return blStringModifyAndCopy(self, BL_MODIFY_OP_ASSIGN_FIT, otherI->data, otherI->size);
}

BLResult blStringAssignData(BLStringCore* self, const char* str, size_t n) noexcept {
  if (n == SIZE_MAX)
    n = strlen(str);
  return blStringModifyAndCopy(self, BL_MODIFY_OP_ASSIGN_FIT, str, n);
}

// ============================================================================
// [BLString - Apply]
// ============================================================================

BLResult blStringApplyOpChar(BLStringCore* self, uint32_t op, char c, size_t n) noexcept {
  char* dst;
  BL_PROPAGATE(blStringModifyOp(self, op, n, &dst));

  memset(dst, int((unsigned char)c), n);
  return BL_SUCCESS;
}

BLResult blStringApplyOpData(BLStringCore* self, uint32_t op, const char* str, size_t n) noexcept {
  if (n == SIZE_MAX)
    n = strlen(str);
  return blStringModifyAndCopy(self, op, str, n);
}

BLResult blStringApplyOpString(BLStringCore* self, uint32_t op, const BLStringCore* other) noexcept {
  BLStringImpl* otherI = other->impl;
  return blStringModifyAndCopy(self, op, otherI->data, otherI->size);
}

BLResult blStringApplyOpFormatV(BLStringCore* self, uint32_t op, const char* fmt, va_list ap) noexcept {
  BLStringImpl* selfI = self->impl;

  size_t index = (op >= BL_MODIFY_OP_APPEND_START) ? selfI->size : size_t(0);
  size_t remaining = selfI->capacity - index;
  size_t mutableMsk = blBitMaskFromBool<size_t>(blImplIsMutable(selfI));

  char buf[1024];
  int fmtResult;
  size_t outputSize;

  if ((remaining & mutableMsk) >= 64) {
    // We include null terminator in buffer size as this is what 'vsnprintf' expects.
    // BLString always reserves one byte for null terminator so this is perfectly safe.
    fmtResult = vsnprintf(selfI->data + index, remaining + 1, fmt, ap);
    if (BL_UNLIKELY(fmtResult < 0))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    outputSize = size_t(unsigned(fmtResult));
    if (BL_LIKELY(outputSize <= remaining)) {
      // `vsnprintf` must write a null terminator, verify it's true.
      BL_ASSERT(selfI->data[index + outputSize] == '\0');

      selfI->size = index + outputSize;
      return BL_SUCCESS;
    }
  }
  else {
    fmtResult = vsnprintf(buf, BL_ARRAY_SIZE(buf), fmt, ap);
    if (BL_UNLIKELY(fmtResult < 0))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    // If the `outputSize` is less than our buffer size then we are fine and
    // the formatted text is already in the buffer. Since `vsnprintf` doesn't
    // include null-terminator in the returned size we cannot use '<=' as that
    // would mean that the last character written by `vsnprintf` was truncated.
    outputSize = size_t(fmtResult);
    if (BL_LIKELY(outputSize < BL_ARRAY_SIZE(buf)))
      return blStringApplyOpData(self, op, buf, outputSize);
  }

  // If we are here it means that the string is either not large enough to hold
  // the formatted text or it's not mutable. In both cases we have to allocate
  // a new buffer and call `vsnprintf` again.
  size_t sizeAfter = blUAddSaturate(index, outputSize);
  if (BL_UNLIKELY(sizeAfter > blStringMaximumCapacity()))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  size_t capacity =
    (op & BL_MODIFY_OP_GROW_MASK)
      ? blStringGrowingCapacity(sizeAfter)
      : blStringFittingCapacity(sizeAfter);

  BLStringImpl* newI = blStringImplNew(capacity);
  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  char* dst = newI->data;
  fmtResult = vsnprintf(dst + index, remaining + 1, fmt, ap);

  // This should always match. If it doesn't then it means that some other thread
  // must have changed some value where `ap` points and it caused `vsnprintf` to
  // format a different string. If this happens we fail as there is no reason to
  // try again...
  if (BL_UNLIKELY(size_t(unsigned(fmtResult)) != outputSize)) {
    blStringImplDelete(newI);
    return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  self->impl = newI;
  newI->size = sizeAfter;

  memcpy(dst, selfI->data, index);
  BL_ASSERT(dst[sizeAfter] == '\0');

  return blStringImplRelease(selfI);
}

BLResult blStringApplyOpFormat(BLStringCore* self, uint32_t op, const char* fmt, ...) noexcept {
  BLResult result;
  va_list ap;

  va_start(ap, fmt);
  result = blStringApplyOpFormatV(self, op, fmt, ap);
  va_end(ap);

  return result;
}

// ============================================================================
// [BLString - Insert]
// ============================================================================

BLResult blStringInsertChar(BLStringCore* self, size_t index, char c, size_t n) noexcept {
  char* dst;
  BL_PROPAGATE(blStringInsertOp(self, index, n, &dst));

  memset(dst, int((unsigned char)c), n);
  return BL_SUCCESS;
}

BLResult blStringInsertData(BLStringCore* self, size_t index, const char* str, size_t n) noexcept {
  if (n == SIZE_MAX)
    n = strlen(str);
  return blStringInsertAndCopy(self, index, str, n);
}

BLResult blStringInsertString(BLStringCore* self, size_t index, const BLStringCore* other) noexcept {
  BLStringImpl* otherI = other->impl;
  return blStringInsertAndCopy(self, index, otherI->data, otherI->size);
}

// ============================================================================
// [BLString - Remove]
// ============================================================================

BLResult blStringRemoveRange(BLStringCore* self, size_t rStart, size_t rEnd) noexcept {
  BLStringImpl* selfI = self->impl;

  size_t size = selfI->size;
  size_t end = blMin(rEnd, size);
  size_t index = blMin(rStart, end);

  size_t n = end - index;
  if (!n)
    return BL_SUCCESS;

  if (!blImplIsMutable(selfI)) {
    size_t capacity = blStringFittingCapacity(size - n);
    BLStringImpl* newI = blStringImplNew(capacity);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    newI->size = size - n;
    self->impl = newI;

    char* dst = newI->data;
    char* src = selfI->data;

    memcpy(dst, src, index);
    memcpy(dst + index, src + end, size - end);

    return blStringImplRelease(selfI);
  }
  else {
    char* data = selfI->data;

    // NOTE: We copy one more byte that acts as a null-terminator.
    selfI->size = size - n;
    memmove(data + index, data + index + n, size - end + 1);

    return BL_SUCCESS;
  }
}

// ============================================================================
// [BLString - Equality / Comparison]
// ============================================================================

bool blStringEquals(const BLStringCore* self, const BLStringCore* other) noexcept {
  const BLStringImpl* selfI = self->impl;
  const BLStringImpl* otherI = other->impl;

  size_t size = selfI->size;
  if (size != otherI->size)
    return false;

  return memcmp(selfI->data, otherI->data, size) == 0;
}

bool blStringEqualsData(const BLStringCore* self, const char* str, size_t n) noexcept {
  BLStringImpl* selfI = self->impl;
  size_t size = selfI->size;

  const char* aData = selfI->data;
  const char* bData = str;

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

int blStringCompare(const BLStringCore* self, const BLStringCore* other) noexcept {
  const BLStringImpl* selfI = self->impl;
  const BLStringImpl* otherI = other->impl;

  size_t aSize = selfI->size;
  size_t bSize = otherI->size;
  size_t minSize = blMin(aSize, bSize);

  int c = memcmp(selfI->data, otherI->data, minSize);
  if (c)
    return c;

  return aSize < bSize ? -1 : int(aSize > bSize);
}

int blStringCompareData(const BLStringCore* self, const char* str, size_t n) noexcept {
  const BLStringImpl* selfI = self->impl;
  size_t aSize = selfI->size;

  const char* aData = selfI->data;
  const char* bData = str;

  if (n == SIZE_MAX) {
    // Null terminated, we don't know the size of `str`. We cannot use strcmp as it's
    // allowed to have zeros (or null terminators) in BLString data as it can act as
    // a byte-vector and not string.
    size_t i;
    for (i = 0; i < aSize; i++) {
      int a = uint8_t(aData[i]);
      int b = uint8_t(bData[i]);

      int c = a - b;

      // If we found a null terminator in 'b' it means that so far the strings were
      // equal, but now we are at the end of 'b', however, there is still some content
      // in 'a'. This would mean that `a > b` like "abc?" > "abc".
      if (b == 0)
        c = 1;

      if (c)
        return c;
    }

    // We are at the end of 'a'. If this is also the end of 'b' then these strings are
    // equal and we return zero. If 'b' doesn't point to a null terminator then `a < b`.
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

// ============================================================================
// [BLString - Runtime]
// ============================================================================

void blStringOnInit(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);

  BLStringImpl* stringI = &blNullStringImpl;
  blInitBuiltInNull(stringI, BL_IMPL_TYPE_STRING, 0);
  stringI->data = const_cast<char*>(blNullStringData);
  blAssignBuiltInNull(stringI);
}

// ============================================================================
// [BLString - Unit Tests]
// ============================================================================

#if defined(BL_TEST)
UNIT(string) {
  BLString s;

  INFO("Assignment and comparison");
  EXPECT(s.assign('b') == BL_SUCCESS);
  EXPECT(s.size() == 1);
  EXPECT(s.data()[0] == 'b');
  EXPECT(s.data()[1] == '\0');
  EXPECT(s.equals("b"   ) == true);
  EXPECT(s.equals("b", 1) == true);
  EXPECT(s.compare("a"    ) > 0);
  EXPECT(s.compare("a" , 1) > 0);
  EXPECT(s.compare("a?"   ) > 0);
  EXPECT(s.compare("a?", 2) > 0);
  EXPECT(s.compare("b"    ) == 0);
  EXPECT(s.compare("b" , 1) == 0);
  EXPECT(s.compare("b?"   ) < 0);
  EXPECT(s.compare("b?", 2) < 0);
  EXPECT(s.compare("c"    ) < 0);
  EXPECT(s.compare("c",  1) < 0);
  EXPECT(s.compare("c?"   ) < 0);
  EXPECT(s.compare("c?", 2) < 0);

  EXPECT(s.assign('b', 4) == BL_SUCCESS);
  EXPECT(s.size() == 4);
  EXPECT(s.data()[0] == 'b');
  EXPECT(s.data()[1] == 'b');
  EXPECT(s.data()[2] == 'b');
  EXPECT(s.data()[3] == 'b');
  EXPECT(s.data()[4] == '\0');
  EXPECT(s.equals("bbbb"   ) == true);
  EXPECT(s.equals("bbbb", 4) == true);
  EXPECT(s.compare("bbbb"   ) == 0);
  EXPECT(s.compare("bbbb", 4) == 0);
  EXPECT(s.compare("bbba"   ) > 0);
  EXPECT(s.compare("bbba", 4) > 0);
  EXPECT(s.compare("bbbc"   ) < 0);
  EXPECT(s.compare("bbbc", 4) < 0);

  EXPECT(s.assign("abc") == BL_SUCCESS);
  EXPECT(s.size() == 3);
  EXPECT(s.data()[0] == 'a');
  EXPECT(s.data()[1] == 'b');
  EXPECT(s.data()[2] == 'c');
  EXPECT(s.data()[3] == '\0');
  EXPECT(s.equals("abc") == true);
  EXPECT(s.equals("abc", 3) == true);

  INFO("String manipulation");
  EXPECT(s.append("xyz") == BL_SUCCESS);
  EXPECT(s.equals("abcxyz") == true);

  EXPECT(s.insert(2, s.view()) == BL_SUCCESS);
  EXPECT(s.equals("ababcxyzcxyz"));

  EXPECT(s.remove(BLRange(1, 11)) == BL_SUCCESS);
  EXPECT(s.equals("az"));

  EXPECT(s.insert(1, s.view()) == BL_SUCCESS);
  EXPECT(s.equals("aazz"));

  EXPECT(s.insert(1, "xxx") == BL_SUCCESS);
  EXPECT(s.equals("axxxazz"));

  EXPECT(s.remove(BLRange(4, 6)) == BL_SUCCESS);
  EXPECT(s.equals("axxxz"));

  BLString x(s);
  EXPECT(s.insert(3, "INSERTED") == BL_SUCCESS);
  EXPECT(s.equals("axxINSERTEDxz"));

  x = s;
  EXPECT(s.remove(BLRange(1, 11)) == BL_SUCCESS);
  EXPECT(s.equals("axz"));

  EXPECT(s.insert(3, "APPENDED") == BL_SUCCESS);
  EXPECT(s.equals("axzAPPENDED"));

  EXPECT(s.reserve(1024) == BL_SUCCESS);
  EXPECT(s.capacity() >= 1024);
  EXPECT(s.shrink() == BL_SUCCESS);
  EXPECT(s.capacity() < 1024);

  INFO("String Formatting");
  EXPECT(s.assignFormat("%d", 1000) == BL_SUCCESS);
  EXPECT(s.equals("1000"));

  INFO("String Search");
  EXPECT(s.assign("abcdefghijklmnop-ponmlkjihgfedcba") == BL_SUCCESS);
  EXPECT(s.indexOf('a') == 0);
  EXPECT(s.indexOf('a', 1) == 32);
  EXPECT(s.indexOf('b') == 1);
  EXPECT(s.indexOf('b', 1) == 1);
  EXPECT(s.indexOf('b', 2) == 31);
  EXPECT(s.lastIndexOf('b') == 31);
  EXPECT(s.lastIndexOf('b', 30) == 1);
  EXPECT(s.indexOf('z') == SIZE_MAX);
  EXPECT(s.indexOf('z', SIZE_MAX) == SIZE_MAX);
  EXPECT(s.lastIndexOf('z') == SIZE_MAX);
  EXPECT(s.lastIndexOf('z', 0) == SIZE_MAX);
  EXPECT(s.lastIndexOf('z', SIZE_MAX) == SIZE_MAX);
}
#endif
