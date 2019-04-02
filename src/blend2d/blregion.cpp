// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blarray_p.h"
#include "./blarrayops_p.h"
#include "./blgeometry_p.h"
#include "./blregion_p.h"
#include "./blruntime_p.h"
#include "./blsupport_p.h"

// ============================================================================
// [Global Variables]
// ============================================================================

static BLWrap<BLInternalRegionImpl> blNullRegionImpl;

static const BLBoxI blRegionLargestBoxI {
  blMinValue<int>(),
  blMinValue<int>(),
  blMaxValue<int>(),
  blMaxValue<int>()
};

// ============================================================================
// [BLRegion - Internal]
// ============================================================================

static constexpr size_t blRegionImplSizeOf(size_t n = 0) noexcept { return blContainerSizeOf(sizeof(BLInternalRegionImpl), sizeof(BLBoxI), n); }
static constexpr size_t blRegionCapacityOf(size_t implSize) noexcept { return blContainerCapacityOf(sizeof(BLInternalRegionImpl), sizeof(BLBoxI), implSize); }
static constexpr size_t blRegionMaximumCapacity() noexcept { return blRegionCapacityOf(SIZE_MAX); }
static BL_INLINE size_t blRegionFittingCapacity(size_t n) noexcept { return blContainerFittingCapacity(blRegionImplSizeOf(), sizeof(BLBoxI), n); }
static BL_INLINE size_t blRegionGrowingCapacity(size_t n) noexcept { return blContainerGrowingCapacity(blRegionImplSizeOf(), sizeof(BLBoxI), n, BL_ALLOC_HINT_REGION); }

static BL_INLINE void blRegionCopyData(BLBoxI* dst, const BLBoxI* src, size_t n) noexcept {
  for (size_t i = 0; i < n; i++)
    dst[i] = src[i];
}

static BL_INLINE BLBoxI blRegionCopyDataAndCalcBBox(BLBoxI* dst, const BLBoxI* src, size_t n) noexcept {
  BL_ASSUME(n > 0);

  int bBoxX0 = blMaxValue<int>();
  int bBoxX1 = blMinValue<int>();

  for (size_t i = 0; i < n; i++) {
    bBoxX0 = blMin(bBoxX0, src[i].x0);
    bBoxX1 = blMax(bBoxX1, src[i].x1);
    dst[i] = src[i];
  }

  return BLBoxI(bBoxX0, src[0].y0, bBoxX1, src[n - 1].y1);
}

static BL_INLINE BLInternalRegionImpl* blRegionImplNew(size_t n) noexcept {
  uint16_t memPoolData;
  BLInternalRegionImpl* impl = blRuntimeAllocImplT<BLInternalRegionImpl>(blRegionImplSizeOf(n), &memPoolData);

  if (BL_UNLIKELY(!impl))
    return impl;

  blImplInit(impl, BL_IMPL_TYPE_REGION, 0, memPoolData);
  impl->data = blOffsetPtr<BLBoxI>(impl, sizeof(BLInternalRegionImpl));
  impl->size = 0;
  impl->capacity = n;
  impl->reserved[0] = 0;
  impl->reserved[1] = 0;
  impl->reserved[2] = 0;
  impl->reserved[3] = 0;
  impl->boundingBox.reset();

  return impl;
}

// Cannot be static, called by `BLVariant` implementation.
BLResult blRegionImplDelete(BLRegionImpl* impl_) noexcept {
  BLInternalRegionImpl* impl = blInternalCast(impl_);

  uint8_t* implBase = reinterpret_cast<uint8_t*>(impl);
  size_t implSize = blRegionImplSizeOf(impl->capacity);
  uint32_t implTraits = impl->implTraits;
  uint32_t memPoolData = impl->memPoolData;

  if (implTraits & BL_IMPL_TRAIT_EXTERNAL) {
    implSize = blRegionImplSizeOf() + sizeof(BLExternalImplPreface);
    implBase -= sizeof(BLExternalImplPreface);
    blImplDestroyExternal(impl);
  }

  if (implTraits & BL_IMPL_TRAIT_FOREIGN)
    return BL_SUCCESS;
  else
    return blRuntimeFreeImpl(implBase, implSize, memPoolData);
}

static BL_INLINE BLResult blRegionImplRelease(BLInternalRegionImpl* impl) noexcept {
  if (blImplDecRefAndTest(impl))
    return blRegionImplDelete(impl);
  return BL_SUCCESS;
}

static BL_NOINLINE BLResult blRegionRealloc(BLRegionCore* self, size_t n) noexcept {
  BLInternalRegionImpl* oldI = blInternalCast(self->impl);
  BLInternalRegionImpl* newI = blRegionImplNew(n);

  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  size_t size = oldI->size;
  BL_ASSERT(size <= n);

  self->impl = newI;
  newI->size = size;
  newI->boundingBox = oldI->boundingBox;
  blRegionCopyData(newI->data, oldI->data, size);

  return blRegionImplRelease(oldI);
}

// ============================================================================
// [BLRegion - Matcher]
// ============================================================================

struct BLRegionXYMatcher {
  int x, y;
  BL_INLINE BLRegionXYMatcher(int x, int y) noexcept : x(x), y(y) {}
};
static BL_INLINE bool operator<(const BLBoxI& a, const BLRegionXYMatcher& b) noexcept { return (a.y1 <= b.y) || ((a.y0 <= b.y) & (a.x1 <= b.x)); }

// ============================================================================
// [BLRegion - Utilities]
// ============================================================================

static BL_INLINE const BLBoxI& blAsBox(const BLBoxI& box) noexcept { return box; }
static BL_INLINE BLBoxI blAsBox(const BLRectI& rect) noexcept { return BLBoxI(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h); }

//! Checks whether two bands (of the same size) must coalesce.
static BL_INLINE bool blRegionMustCoalesce(const BLBoxI* aBand, const BLBoxI* bBand, size_t n) noexcept {
  size_t i = 0;
  while (i < n && (aBand[i].x0 == bBand[i].x0) & (aBand[i].x1 == bBand[i].x1))
    i++;
  return i == n;
}

//! Checks whether two bands (of the same size) must coalesce.
static BL_INLINE bool blRegionMustCoalesce(const BLRectI* aBand, const BLRectI* bBand, size_t n) noexcept {
  size_t i = 0;
  while (i < n && (aBand[i].x == bBand[i].x) & (aBand[i].w == bBand[i].w))
    i++;
  return i == n;
}

static BL_INLINE void blRegionSetBandY1(BLBoxI* band, size_t n, int y1) noexcept {
  for (size_t i = 0; i < n; i++)
    band[i].y1 = y1;
}

// Get the end band of the current horizontal rectangle list.
static BL_INLINE const BLBoxI* blRegionGetEndBand(const BLBoxI* data, const BLBoxI* end) {
  const BLBoxI* cur = data;
  int y0 = data[0].y0;

  while (++cur != end && cur[0].y0 == y0)
    continue;
  return cur;
}

static BL_INLINE BLBoxI* blRegionCoalesce(BLBoxI* p, BLBoxI* curBand, int y1, size_t *prevBandSize) noexcept {
  size_t bandSize = (size_t)(p - curBand);
  if (*prevBandSize == bandSize) {
    BLBoxI* prevBand = curBand - bandSize;
    if (prevBand->y1 == curBand->y0 && blRegionMustCoalesce(prevBand, curBand, bandSize)) {
      blRegionSetBandY1(prevBand, bandSize, y1);
      return curBand;
    }
  }
  *prevBandSize = bandSize;
  return p;
}

// ============================================================================
// [BLRegion - Analysis]
// ============================================================================

static uint32_t blRegionAnalyzeBoxIArray(const BLBoxI* data, size_t size, size_t* sizeOut) {
  const BLBoxI* end = data + size;
  *sizeOut = size;

  if (data == end)
    return BL_DATA_ANALYSIS_CONFORMING;

  const BLBoxI* prevBand = data;
  uint32_t prevBandSum = 0;

  for (;;) {
    int y0 = data->y0;
    int y1 = data->y1;
    int x1 = data->x1;

    const BLBoxI* curBand = data;
    uint32_t curBandSum = unsigned(x1);

    if (BL_UNLIKELY((data->x0 >= x1) | (y0 >= y1)))
      return BL_DATA_ANALYSIS_INVALID_VALUE;

    while (++data != end) {
      if ((data->y0 != y0) | (data->y1 != y1)) {
        // Start of a next band.
        if (data->y0 >= y1)
          break;

        // Detect non-conforming box.
        if (BL_UNLIKELY(data->y0 < y1))
          goto NonConforming;
      }

      if (BL_UNLIKELY((data->x0 <= x1) | (data->x0 >= data->x1)))
        goto NonConforming;

      x1 = data->x1;
      curBandSum += unsigned(x1);
    }

    if (prevBand->y1 == y0) {
      if (BL_UNLIKELY(curBandSum == prevBandSum)) {
        size_t prevBandSize = (size_t)(curBand - prevBand);
        size_t curBandSize = (size_t)(data - curBand);

        if (prevBandSize == curBandSize && blRegionMustCoalesce(prevBand, curBand, curBandSize))
          size -= curBandSize;
      }
    }

    prevBand = curBand;
    prevBandSum = curBandSum;
  }

  *sizeOut = size;
  return BL_DATA_ANALYSIS_CONFORMING;

NonConforming:
  do {
    if (BL_UNLIKELY((data->x0 >= data->x1) | (data->y0 >= data->y1)))
      return BL_DATA_ANALYSIS_INVALID_VALUE;
  } while (++data != end);

  return BL_DATA_ANALYSIS_NON_CONFORMING;
}

static uint32_t blRegionAnalyzeRectIArray(const BLRectI* data, size_t size, size_t* sizeOut) {
  const BLRectI* end = data + size;
  *sizeOut = size;

  if (data == end)
    return BL_DATA_ANALYSIS_CONFORMING;

  const BLRectI* prevBand = data;
  uint32_t prevBandSum = 0;

  for (;;) {
    int y0 = data->y;
    int y1;
    int x1 = data->x;
    int h = data->h;

    BLOverflowFlag of = 0;
    x1 = blAddOverflow(x1, data->w, &of);
    y1 = blAddOverflow(y0, h, &of);

    if (BL_UNLIKELY(of | (data->w <= 0) | (h <= 0)))
      return BL_DATA_ANALYSIS_INVALID_VALUE;

    const BLRectI* curBand = data;
    uint32_t curBandSum = unsigned(x1);

    while (++data != end) {
      if ((data->y != y0) | (data->h != h)) {
        // Start of a next band.
        if (data->y >= y1)
          break;

        // Detect non-conforming box.
        if (data->y < y1)
          goto NonConforming;
      }

      if (BL_UNLIKELY((data->x <= x1)))
        goto NonConforming;

      x1 = blAddOverflow(data->x, data->w, &of);
      if (BL_UNLIKELY(of | (data->w <= 0)))
        return BL_DATA_ANALYSIS_INVALID_VALUE;

      curBandSum += unsigned(x1);
    }

    if (prevBand->y + prevBand->h == y0) {
      if (BL_UNLIKELY(curBandSum == prevBandSum)) {
        size_t prevBandSize = (size_t)(curBand - prevBand);
        size_t curBandSize = (size_t)(data - curBand);

        if (prevBandSize == curBandSize && blRegionMustCoalesce(prevBand, curBand, curBandSize))
          size -= curBandSize;
      }
    }

    prevBand = curBand;
    prevBandSum = curBandSum;
  }

  *sizeOut = size;
  return BL_DATA_ANALYSIS_CONFORMING;

NonConforming:
  do {
    BLOverflowFlag of = 0;
    int w = data->w;
    int h = data->h;
    blAddOverflow(data->x, w, &of);
    blAddOverflow(data->y, h, &of);
    if (BL_UNLIKELY(of | (w <= 0) | (h <= 0)))
      return BL_DATA_ANALYSIS_INVALID_VALUE;
  } while (++data != end);

  return BL_DATA_ANALYSIS_NON_CONFORMING;
}

static bool blRegionImplIsValid(const BLInternalRegionImpl* impl) noexcept {
  if (impl->capacity < impl->size)
    return false;

  const BLBoxI* data = impl->data;
  const BLBoxI& bbox = impl->boundingBox;

  size_t n = impl->size;

  // If the region is empty the bounding box must match [0, 0, 0, 0].
  if (!n)
    return bbox.x0 == 0 && bbox.y0 == 0 && bbox.x1 == 0 && bbox.y1 == 0;

  if (n == 1)
    return blIsValid(data[0]) && data[0] == bbox;

  size_t coalescedSize;
  uint32_t status = blRegionAnalyzeBoxIArray(data, n, &coalescedSize);

  return status == BL_DATA_ANALYSIS_CONFORMING && n == coalescedSize;
}

// ============================================================================
// [BLRegion - Init / Reset]
// ============================================================================

BLResult blRegionInit(BLRegionCore* self) noexcept {
  self->impl = BLRegion::none().impl;
  return BL_SUCCESS;
}

BLResult blRegionReset(BLRegionCore* self) noexcept {
  BLInternalRegionImpl* selfI = blInternalCast(self->impl);
  self->impl = &blNullRegionImpl;
  return blRegionImplRelease(selfI);
}

// ============================================================================
// [BLRegion - Storage]
// ============================================================================

BLResult blRegionClear(BLRegionCore* self) noexcept {
  BLInternalRegionImpl* selfI = blInternalCast(self->impl);

  if (!blImplIsMutable(selfI)) {
    self->impl = &blNullRegionImpl;
    return blRegionImplRelease(selfI);
  }
  else {
    selfI->size = 0;
    selfI->boundingBox.reset();
    return BL_SUCCESS;
  }
}

BLResult blRegionShrink(BLRegionCore* self) noexcept {
  BLInternalRegionImpl* selfI = blInternalCast(self->impl);
  size_t size = selfI->size;

  if (!size) {
    self->impl = &blNullRegionImpl;
    return blRegionImplRelease(selfI);
  }

  size_t capacity = blRegionFittingCapacity(size);
  if (capacity >= selfI->capacity)
    return BL_SUCCESS;

  return blRegionRealloc(self, capacity);
}

BLResult blRegionReserve(BLRegionCore* self, size_t n) noexcept {
  BLInternalRegionImpl* selfI = blInternalCast(self->impl);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  if ((n | immutableMsk) > selfI->capacity) {
    if (BL_UNLIKELY(n > blRegionMaximumCapacity()))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity = blRegionFittingCapacity(blMax(n, selfI->size));
    return blRegionRealloc(self, capacity);
  }

  return BL_SUCCESS;
}

static BLResult blRegionMakeMutableToAssign(BLRegionCore* self, size_t n) noexcept {
  BLInternalRegionImpl* selfI = blInternalCast(self->impl);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  if ((n | immutableMsk) > selfI->capacity) {
    if (BL_UNLIKELY(n > blRegionMaximumCapacity()))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity = blRegionFittingCapacity(n);
    BLInternalRegionImpl* newI = blRegionImplNew(capacity);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    self->impl = newI;
    return blRegionImplRelease(selfI);
  }

  return BL_SUCCESS;
}

static BLResult blRegionMakeMutableToAppend(BLRegionCore* self, size_t n) noexcept {
  BLInternalRegionImpl* selfI = blInternalCast(self->impl);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  // NOTE: This can never overflow in theory due to size of `BLBoxI`.
  n += selfI->size;

  if ((n | immutableMsk) > selfI->capacity) {
    if (BL_UNLIKELY(n > blRegionMaximumCapacity()))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity = blRegionFittingCapacity(n);
    return blRegionRealloc(self, capacity);
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLRegion - Assign]
// ============================================================================

static BLResult blRegionAssignValidBoxIArray(BLRegionCore* self, const BLBoxI* data, size_t n) noexcept {
  BLInternalRegionImpl* selfI = blInternalCast(self->impl);

  size_t size = selfI->size;
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  if ((n | immutableMsk) > selfI->capacity) {
    if (BL_UNLIKELY(n > blRegionMaximumCapacity()))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity = blRegionFittingCapacity(size);
    BLInternalRegionImpl* newI = blRegionImplNew(capacity);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    self->impl = newI;
    newI->size = n;
    newI->boundingBox = blRegionCopyDataAndCalcBBox(newI->data, data, n);

    return blRegionImplRelease(selfI);
  }

  if (!n)
    return blRegionClear(self);

  selfI->size = n;
  selfI->boundingBox = blRegionCopyDataAndCalcBBox(selfI->data, data, n);

  return BL_SUCCESS;
}

static BLResult blRegionAssignValidBoxIArray(BLRegionCore* self, const BLBoxI* data, size_t n, const BLBoxI* bbox) noexcept {
  BLInternalRegionImpl* selfI = blInternalCast(self->impl);

  size_t size = selfI->size;
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  if ((n | immutableMsk) > selfI->capacity) {
    if (BL_UNLIKELY(n > blRegionMaximumCapacity()))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    size_t capacity = blRegionFittingCapacity(size);
    BLInternalRegionImpl* newI = blRegionImplNew(capacity);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    self->impl = newI;
    newI->size = n;
    newI->boundingBox = *bbox;
    blRegionCopyData(newI->data, data, n);

    return blRegionImplRelease(selfI);
  }

  if (!n)
    return blRegionClear(self);

  selfI->size = n;
  selfI->boundingBox = *bbox;
  blRegionCopyData(selfI->data, data, n);

  return BL_SUCCESS;
}

template<typename T>
static BLResult blRegionAssignAlmostConformingBoxIArray(BLRegionCore* self, const T* srcData, size_t n, size_t analysisSize) noexcept {
  BL_PROPAGATE(blRegionMakeMutableToAssign(self, analysisSize));
  BLInternalRegionImpl* selfI = blInternalCast(self->impl);

  BLBoxI* dstData = selfI->data;
  size_t prevBandSize = SIZE_MAX;

  BL_ASSUME(n > 0);
  const T* srcEnd = srcData + n;

  int bBoxX0 = blMaxValue<int>();
  int bBoxX1 = blMinValue<int>();

  do {
    // First box is always appended as is.
    BL_ASSERT(dstData != selfI->data + selfI->capacity);
    dstData->reset(blAsBox(*srcData));

    int y0 = dstData[0].y0;
    int y1 = dstData[0].y1;

    // Next boxes are either merged with the previous one or appended.
    BLBoxI* curBand = dstData++;
    while (++srcData != srcEnd) {
      BLBoxI src = blAsBox(*srcData);
      if (src.y0 != y0)
        break;

      if (dstData[-1].x1 == src.x0) {
        dstData[-1].x1 = src.x1;
      }
      else {
        BL_ASSERT(dstData != selfI->data + selfI->capacity);
        dstData->reset(src.x0, y0, src.x1, y1);
        dstData++;
      }
    }

    bBoxX0 = blMin(bBoxX0, curBand[0].x0);
    bBoxX1 = blMax(bBoxX1, dstData[-1].x1);

    dstData = blRegionCoalesce(dstData, curBand, y1, &prevBandSize);
  } while (srcData != srcEnd);

  n = (size_t)(dstData - selfI->data);
  selfI->size = n;
  selfI->boundingBox.reset(bBoxX0, selfI->data[0].y0, bBoxX1, dstData[-1].y1);

  BL_ASSERT(blRegionImplIsValid(selfI));
  return BL_SUCCESS;
}

template<typename T>
static BLResult blRegionAssignNonConformingBoxIArray(BLRegionCore* self, const T* srcData, size_t n) noexcept {
  BLRegion tmp;
  BLRegionCore* regions[2] = { &tmp, self };
  size_t index = 0;

  const T* srcEnd = srcData + n;
  while (srcData != srcEnd) {
    BLBoxI src = blAsBox(*srcData);
    BL_PROPAGATE(blRegionCombineRB(regions[index ^ 1], regions[index], &src, BL_BOOLEAN_OP_OR));
    index ^= 1;
  }

  return blRegionAssignWeak(self, regions[index]);
}

BLResult blRegionAssignMove(BLRegionCore* self, BLRegionCore* other) noexcept {
  BLInternalRegionImpl* selfI = blInternalCast(self->impl);
  BLInternalRegionImpl* otherI = blInternalCast(other->impl);

  self->impl = otherI;
  other->impl = &blNullRegionImpl;

  return blRegionImplRelease(selfI);
}

BLResult blRegionAssignWeak(BLRegionCore* self, const BLRegionCore* other) noexcept {
  BLInternalRegionImpl* selfI = blInternalCast(self->impl);
  BLInternalRegionImpl* otherI = blInternalCast(other->impl);

  self->impl = blImplIncRef(otherI);
  return blRegionImplRelease(selfI);
}

BLResult blRegionAssignDeep(BLRegionCore* self, const BLRegionCore* other) noexcept {
  BLInternalRegionImpl* otherI = blInternalCast(other->impl);
  return blRegionAssignValidBoxIArray(self, otherI->data, otherI->size, &otherI->boundingBox);
}

BLResult blRegionAssignBoxI(BLRegionCore* self, const BLBoxI* src) noexcept {
  if ((src->x0 >= src->x1) | (src->y0 >= src->y1))
    return blTraceError(BL_ERROR_INVALID_VALUE);
  return blRegionAssignValidBoxIArray(self, src, 1, src);
}

BLResult blRegionAssignBoxIArray(BLRegionCore* self, const BLBoxI* data, size_t n) noexcept {
  if (!n)
    return blRegionClear(self);

  size_t analysisSize;
  uint32_t analysisStatus = blRegionAnalyzeBoxIArray(data, n, &analysisSize);

  if (analysisStatus >= BL_DATA_ANALYSIS_INVALID_VALUE)
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (analysisStatus == BL_DATA_ANALYSIS_NON_CONFORMING)
    return blRegionAssignNonConformingBoxIArray<BLBoxI>(self, data, n);

  // If `analysisSize == n` it means that the given data is conforming and
  // properly coalesced. The easiest way to assign these boxes to the
  // region is to use `blRegionAssignValidBoxIArray()` as it would also
  // handle the case in which the given `data` overlaps `self` data.
  if (analysisSize == n)
    return blRegionAssignValidBoxIArray(self, data, n);
  else
    return blRegionAssignAlmostConformingBoxIArray<BLBoxI>(self, data, n, analysisSize);
}

BLResult blRegionAssignRectI(BLRegionCore* self, const BLRectI* rect) noexcept {
  int w = rect->w;
  int h = rect->h;

  if (BL_UNLIKELY((w <= 0) | (h <= 0)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLOverflowFlag of = 0;
  int x0 = rect->x;
  int y0 = rect->y;
  int x1 = blAddOverflow(x0, w, &of);
  int y1 = blAddOverflow(y0, h, &of);

  if (BL_UNLIKELY(of))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLBoxI box(x0, y0, x1, y1);
  return blRegionAssignValidBoxIArray(self, &box, 1, &box);
}

BLResult blRegionAssignRectIArray(BLRegionCore* self, const BLRectI* data, size_t n) noexcept {
  if (!n)
    return blRegionClear(self);

  size_t analysisSize;
  uint32_t analysisStatus = blRegionAnalyzeRectIArray(data, n, &analysisSize);

  if (analysisStatus >= BL_DATA_ANALYSIS_INVALID_VALUE)
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (analysisStatus == BL_DATA_ANALYSIS_NON_CONFORMING)
    return blRegionAssignNonConformingBoxIArray<BLRectI>(self, data, n);
  else
    return blRegionAssignAlmostConformingBoxIArray<BLRectI>(self, data, n, analysisSize);
}

// ============================================================================
// [BLRegion - Append]
// ============================================================================

// Get whether it is possible to append box B after box A (or merge with it).
static BL_INLINE bool blRegionCanAppend(const BLBoxI& a, const BLBoxI& b) noexcept {
  return (a.y0 == b.y0) & (a.y1 == b.y1) & (a.x1 <= b.x0);
}

// Internal append - The DST data must be large enough to append SRC into them.
// This function handles possible cases that require coalescing. It first tries
// to append the first box in SRC with the last box in DST. This is a very
// special case used by OR and possibly non-overlapping XOR:
//
// 1) The first source rectangle immediately follows the last destination one.
// 2) The first source rectangle follows the last destination band.
//
//       1)            2)
//   ..........   ..........
//   ..DDDDDDDD   ..DDDDDDDD   D - Destination rectangle(srcData)
//   DDDDDDSSSS   DDDD   SSS
//   SSSSSSSS..   SSSSSSSS..   S - Source rectangle(srcData)
//   ..........   ..........
//
// The function must also handle several special cases:
//
//   ..........
//   DDDD  DDDD
//   DDSS  SSSS <- First Coalesce
//   SSSS  SSSS <- Second coalesce
//   ..........
static BLBoxI* blRegionAppendInternal(BLBoxI* dstStart, BLBoxI* dstData, const BLBoxI* srcData, const BLBoxI* srcEnd) noexcept {
  BLBoxI* prevBand = dstData;
  int y0 = srcData->y0;

  if (dstData != dstStart && dstData[-1].y0 == y0) {
    // This must be checked before calling this function.
    int y1 = dstData[-1].y1;
    BL_ASSERT(dstData[-1].y1 == y1);

    // BLBoxI* pMark = dstData;

    // Merge the last destination rectangle with the first source one? (Case 1).
    if (dstData[-1].x1 == srcData->x0) {
      dstData[-1].x1 = srcData->x1;
      srcData++;
    }

    // Append the remaining part of the band.
    while (srcData != srcEnd && srcData->y0 == y0)
      *dstData++ = *srcData++;

    // Find the beginning of the current band. It's called `prevBand` here
    // as it will be the previous band after we handle this special case.
    while (--prevBand != dstStart && prevBand[-1].y0 == y0)
      continue;

    // Attempt to coalesce the last two consecutive bands.
    size_t bandSize = (size_t)(dstData - prevBand);
    if (prevBand != dstStart && prevBand[-1].y1 == y0) {
      size_t beforeSize = (size_t)(prevBand - dstStart);

      // The size of previous band must be exactly same as `bandSize`.
      if (beforeSize == bandSize || (beforeSize > bandSize && prevBand[bandSize - 1].y1 != y0)) {
        if (blRegionMustCoalesce(prevBand - bandSize, prevBand, bandSize)) {
          prevBand -= bandSize;
          dstData -= bandSize;
          blRegionSetBandY1(prevBand, bandSize, y1);
        }
      }
    }

    // If the second band of source data is consecutive we have to attempt to
    // coalesce this one as well. Since we know the beginning of the previous
    // band this is a bit easier than before.
    if (srcData != srcEnd) {
      y0 = srcData->y0;
      if (y0 == y1) {
        // Append the whole band, terminate at its end.
        BLBoxI* curBand = dstData;
        y1 = srcData->y1;

        do {
          *dstData++ = *srcData++;
        } while (srcData != srcEnd && srcData->y0 == y0);

        if ((size_t)(dstData - curBand) == bandSize) {
          if (blRegionMustCoalesce(prevBand, curBand, bandSize)) {
            dstData -= bandSize;
            blRegionSetBandY1(prevBand, bandSize, y1);
          }
        }
      }
    }
  }

  // Simply append the rest of source as there is no way it would need coalescing.
  while (srcData != srcEnd)
    *dstData++ = *srcData++;

  return dstData;
}

static BLResult blRegionAppendSelf(
  BLRegionCore* dst,
  const BLBoxI* sData, size_t sSize, const BLBoxI& sBoundingBox) noexcept {

  BL_PROPAGATE(blRegionMakeMutableToAppend(dst, sSize));
  BLInternalRegionImpl* dstI = blInternalCast(dst->impl);

  BLBoxI* dstStart = dstI->data;
  BLBoxI* dstData = blRegionAppendInternal(dstStart, dstStart + dstI->size, sData, sData + sSize);

  dstI->size = (size_t)(dstData - dstStart);
  dstI->boundingBox.reset(blMin(dstI->boundingBox.x0, sBoundingBox.x1), dstStart[0].y0,
                          blMax(dstI->boundingBox.x1, sBoundingBox.x1), dstData[-1].y1);
  return BL_SUCCESS;
}

static BLResult blRegionAppendAB(
  BLRegionCore* dst,
  const BLBoxI* aData, size_t aSize, const BLBoxI& aBoundingBox,
  const BLBoxI* bData, size_t bSize, const BLBoxI& bBoundingBox) noexcept {

  // NOTE: The calculation cannot overflow due to the size of `BLBoxI`.
  size_t n = aSize + bSize;

  BL_PROPAGATE(blRegionMakeMutableToAssign(dst, n));
  BLInternalRegionImpl* dstI = blInternalCast(dst->impl);

  BLBoxI* dstStart = dstI->data;
  BLBoxI* dstData = dstStart;

  blRegionCopyData(dstData, aData, aSize);
  dstData = blRegionAppendInternal(dstStart, dstData + aSize, bData, bData + bSize);

  dstI->size = (size_t)(dstData - dstStart);
  dstI->boundingBox.reset(blMin(aBoundingBox.x0, bBoundingBox.x1), dstStart[0].y0,
                          blMax(aBoundingBox.x1, bBoundingBox.x1), dstData[-1].y1);
  return BL_SUCCESS;
}

// ============================================================================
// [BLRegion - Intersect]
// ============================================================================

static BLResult blRegionIntersectBox(BLRegionCore* dst, const BLRegionCore* src, const BLBoxI* box) noexcept {
  BLInternalRegionImpl* dstI = blInternalCast(dst->impl);
  BLInternalRegionImpl* srcI = blInternalCast(src->impl);

  size_t n = srcI->size;
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(dstI));

  BLInternalRegionImpl* oldI = nullptr;
  if ((n | immutableMsk) > dstI->capacity) {
    oldI = dstI;
    dstI = blRegionImplNew(blRegionFittingCapacity(n));

    if (BL_UNLIKELY(!dstI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    dst->impl = dstI;
  }

  BL_ASSERT(dstI->capacity >= n);

  BLBoxI* dstDataPtr = dstI->data;
  size_t prevBandSize = SIZE_MAX;

  BLBoxI* srcDataPtr = srcI->data;
  BLBoxI* srcDataEnd = srcDataPtr + n;

  int ix0 = box->x0;
  int iy0 = box->y0;
  int ix1 = box->x1;
  int iy1 = box->y1;

  int dstBBoxX0 = blMaxValue<int>();
  int dstBBoxX1 = blMinValue<int>();

  // Skip boxes which do not intersect with the clip-box.
  while (srcDataPtr->y1 <= iy0) {
    if (++srcDataPtr == srcDataEnd)
      goto Done;
  }

  // Do the intersection part.
  for (;;) {
    BL_ASSERT(srcDataPtr != srcDataEnd);

    int bandY0 = srcDataPtr->y0;
    if (bandY0 >= iy1)
      break;

    int y0;
    int y1 = 0; // Be quiet.
    BLBoxI* dstBandPtr = dstDataPtr;

    // Skip leading boxes which do not intersect with the clip-box.
    while (srcDataPtr->x1 <= ix0) {
      if (++srcDataPtr == srcDataEnd) goto Done;
      if (srcDataPtr->y0 != bandY0) goto Skip;
    }

    // Do the inner part.
    if (srcDataPtr->x0 < ix1) {
      y0 = blMax(srcDataPtr->y0, iy0);
      y1 = blMin(srcDataPtr->y1, iy1);

      // First box.
      BL_ASSERT(dstDataPtr < dstI->data + n);
      dstDataPtr->reset(blMax(srcDataPtr->x0, ix0), y0, blMin(srcDataPtr->x1, ix1), y1);
      dstDataPtr++;

      if (++srcDataPtr == srcDataEnd || srcDataPtr->y0 != bandY0)
        goto Merge;

      // Inner boxes.
      while (srcDataPtr->x1 <= ix1) {
        BL_ASSERT(dstDataPtr < dstI->data + n);
        BL_ASSERT(srcDataPtr->x0 >= ix0 && srcDataPtr->x1 <= ix1);

        dstDataPtr->reset(srcDataPtr->x0, y0, srcDataPtr->x1, y1);
        dstDataPtr++;

        if (++srcDataPtr == srcDataEnd || srcDataPtr->y0 != bandY0)
          goto Merge;
      }

      // Last box.
      if (srcDataPtr->x0 < ix1) {
        BL_ASSERT(dstDataPtr < dstI->data + n);
        BL_ASSERT(srcDataPtr->x0 >= ix0);

        dstDataPtr->reset(srcDataPtr->x0, y0, blMin(srcDataPtr->x1, ix1), y1);
        dstDataPtr++;

        if (++srcDataPtr == srcDataEnd || srcDataPtr->y0 != bandY0)
          goto Merge;
      }

      BL_ASSERT(srcDataPtr->x0 >= ix1);
    }

    // Skip trailing boxes which do not intersect with the clip-box.
    while (srcDataPtr->x0 >= ix1) {
      if (++srcDataPtr == srcDataEnd || srcDataPtr->y0 != bandY0)
        break;
    }

Merge:
    if (dstBandPtr != dstDataPtr) {
      dstBBoxX0 = blMin(dstBBoxX0, dstBandPtr[0].x0);
      dstBBoxX1 = blMax(dstBBoxX1, dstDataPtr[-1].x1);
      dstDataPtr = blRegionCoalesce(dstDataPtr, dstBandPtr, y1, &prevBandSize);
    }

Skip:
    if (srcDataPtr == srcDataEnd)
      break;
  }

Done:
  n = (size_t)(dstDataPtr - dstI->data);
  dstI->size = n;

  if (!n)
    dstI->boundingBox.reset();
  else
    dstI->boundingBox.reset(dstBBoxX0, dstI->data[0].y0, dstBBoxX1, dstI->data[n - 1].y1);

  BL_ASSERT(blRegionImplIsValid(dstI));
  return oldI ? blRegionImplRelease(oldI) : BL_SUCCESS;
}

// ============================================================================
// [BLRegion - Combine]
// ============================================================================

// A helper function used by `blRegionCombineInternal()` to reallocate the impl.
// Reallocation is not something that should happen often so it's fine that it's
// outside of the main implementation.
static BLInternalRegionImpl* blRegionCombineGrow(BLInternalRegionImpl* impl, BLBoxI** dstData, size_t n, bool fitOnly) noexcept {
  size_t size = (*dstData - impl->data);
  size_t afterSize = size + n;

  BL_ASSERT(impl->refCount == 1);
  BL_ASSERT(size <= impl->capacity);

  if (BL_UNLIKELY(afterSize > blRegionMaximumCapacity()))
    return nullptr;

  size_t capacity = fitOnly ? blRegionFittingCapacity(afterSize)
                            : blRegionGrowingCapacity(afterSize);
  BLInternalRegionImpl* newI = blRegionImplNew(capacity);

  if (BL_UNLIKELY(!newI))
    return nullptr;

  newI->size = size;
  *dstData = newI->data + size;

  blRegionCopyData(newI->data, impl->data, size);
  blRegionImplDelete(impl);

  return newI;
}

// A low-level function that performs the boolean operation of two regions,
// Box+Region or Region+Box combinations. This function does raw processing
// and doesn't special case anything, thus, all special cases must be dealed
// with before.
static BLResult blRegionCombineInternal(
  BLRegionCore* dst,
  const BLBoxI* aData, size_t aSize, const BLBoxI& aBoundingBox,
  const BLBoxI* bData, size_t bSize, const BLBoxI& bBoundingBox,
  uint32_t op, bool memOverlap) noexcept {

  // Make sure the inputs are as required.
  BL_ASSUME(op != BL_BOOLEAN_OP_COPY);
  BL_ASSUME(op < BL_BOOLEAN_OP_COUNT);
  BL_ASSUME(aSize > 0);
  BL_ASSUME(bSize > 0);

  // The resulting number of boxes after (A & B) can't be larger than (A + B) * 2.
  // However, ror other operators this value is only a hint as the output could
  // be greater than the estimation in some cases. Each combiner except AND checks
  // for availale space before each band is processed.
  //
  // NOTE: The calculation cannot overflow due to the size of `BLBoxI`.
  size_t n = 8 + (aSize + bSize) * 2;

  BLInternalRegionImpl* oldI = nullptr;
  BLInternalRegionImpl* dstI = blInternalCast(dst->impl);

  size_t overlapMsk = blBitMaskFromBool<size_t>(memOverlap);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(dstI));

  if ((n | overlapMsk | immutableMsk) > dstI->capacity) {
    if (BL_UNLIKELY(n >= blRegionMaximumCapacity()))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    oldI = dstI;
    dstI = blRegionImplNew(n);

    if (BL_UNLIKELY(!dstI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  BLBoxI* dstData = dstI->data;
  BLBoxI* dstEnd = dstData + dstI->capacity;
  size_t prevBandSize = SIZE_MAX;

  int dstBBoxX0 = blMaxValue<int>();
  int dstBBoxX1 = blMinValue<int>();

  const BLBoxI* aEnd = aData + aSize;
  const BLBoxI* bEnd = bData + bSize;

  const BLBoxI* aBandEnd = nullptr;
  const BLBoxI* bBandEnd = nullptr;

  int y0, y1;

  #define BL_REGION_ENSURE_SPACE(N, FIT_ONLY)                                 \
    do {                                                                      \
      size_t remain = (size_t)(dstEnd - dstData);                             \
      size_t needed = (size_t)(N);                                            \
                                                                              \
      if (BL_UNLIKELY(remain < needed)) {                                     \
        BLInternalRegionImpl* newI =                                          \
          blRegionCombineGrow(dstI, &dstData, needed, FIT_ONLY);              \
                                                                              \
        if (BL_UNLIKELY(!newI))                                               \
          goto OutOfMemory;                                                   \
                                                                              \
        dstI = newI;                                                          \
        dstEnd = dstI->data + dstI->capacity;                                 \
      }                                                                       \
    } while (0)

  switch (op) {
    // ------------------------------------------------------------------------
    // [Intersect]
    // ------------------------------------------------------------------------

    case BL_BOOLEAN_OP_AND: {
      int yStop = blMin(aBoundingBox.y1, bBoundingBox.y1);

      // Skip all parts which do not intersect.
      for (;;) {
        if (aData->y1 <= bData->y0) { if (++aData == aEnd) goto Done; else continue; }
        if (bData->y1 <= aData->y0) { if (++bData == bEnd) goto Done; else continue; }
        break;
      }

      BL_ASSERT(aData != aEnd);
      BL_ASSERT(bData != bEnd);

      aBandEnd = blRegionGetEndBand(aData, aEnd);
      bBandEnd = blRegionGetEndBand(bData, bEnd);

      for (;;) {
        // Vertical intersection.
        y0 = blMax(aData->y0, bData->y0);
        y1 = blMin(aData->y1, bData->y1);

        if (y0 < y1) {
          BLBoxI* dstDataBand = dstData;
          const BLBoxI* aBand = aData;
          const BLBoxI* bBand = bData;

          for (;;) {
            // Skip boxes which do not intersect.
            if (aBand->x1 <= bBand->x0) { if (++aBand == aBandEnd) break; else continue; }
            if (bBand->x1 <= aBand->x0) { if (++bBand == bBandEnd) break; else continue; }

            // Horizontal intersection.
            int x0 = blMax(aBand->x0, bBand->x0);
            int x1 = blMin(aBand->x1, bBand->x1);

            BL_ASSERT(x0 < x1);
            BL_ASSERT(dstData != dstEnd);

            dstData->reset(x0, y0, x1, y1);
            dstData++;

            // Advance.
            if (aBand->x1 == x1 && ++aBand == aBandEnd) break;
            if (bBand->x1 == x1 && ++bBand == bBandEnd) break;
          }

          // Update bounding box and coalesce.
          if (dstDataBand != dstData) {
            dstBBoxX0 = blMin(dstBBoxX0, dstDataBand[0].x0);
            dstBBoxX1 = blMax(dstBBoxX1, dstData[-1].x1);
            dstData = blRegionCoalesce(dstData, dstDataBand, y1, &prevBandSize);
          }
        }

        // Advance A.
        if (aData->y1 == y1) {
          if ((aData = aBandEnd) == aEnd || aData->y0 >= yStop) break;
          aBandEnd = blRegionGetEndBand(aData, aEnd);
        }

        // Advance B.
        if (bData->y1 == y1) {
          if ((bData = bBandEnd) == bEnd || bData->y0 >= yStop) break;
          bBandEnd = blRegionGetEndBand(bData, bEnd);
        }
      }
      break;
    }

    // ------------------------------------------------------------------------
    // [Union]
    // ------------------------------------------------------------------------

    case BL_BOOLEAN_OP_OR: {
      dstBBoxX0 = blMin(aBoundingBox.x0, bBoundingBox.x0);
      dstBBoxX1 = blMax(aBoundingBox.x1, bBoundingBox.x1);

      aBandEnd = blRegionGetEndBand(aData, aEnd);
      bBandEnd = blRegionGetEndBand(bData, bEnd);

      y0 = blMin(aData->y0, bData->y0);
      for (;;) {
        const BLBoxI* aBand = aData;
        const BLBoxI* bBand = bData;

        BL_REGION_ENSURE_SPACE((size_t)(aBandEnd - aBand) + (size_t)(bBandEnd - bBand), false);
        BLBoxI* dstDataBand = dstData;

        // Merge bands which do not intersect.
        if (bBand->y0 > y0) {
          y1 = blMin(aBand->y1, bBand->y0);
          do {
            BL_ASSERT(dstData != dstEnd);
            dstData->reset(aBand->x0, y0, aBand->x1, y1);
            dstData++;
          } while (++aBand != aBandEnd);
          goto OnUnionBandDone;
        }

        if (aBand->y0 > y0) {
          y1 = blMin(bBand->y1, aBand->y0);
          do {
            BL_ASSERT(dstData != dstEnd);
            dstData->reset(bBand->x0, y0, bBand->x1, y1);
            dstData++;
          } while (++bBand != bBandEnd);
          goto OnUnionBandDone;
        }

        // Vertical intersection of current A and B bands.
        y1 = blMin(aBand->y1, bBand->y1);
        BL_ASSERT(y0 < y1);

        do {
          int x0;
          int x1;

          if (aBand->x0 < bBand->x0) {
            x0 = aBand->x0;
            x1 = aBand->x1;
            aBand++;
          }
          else {
            x0 = bBand->x0;
            x1 = bBand->x1;
            bBand++;
          }

          for (;;) {
            bool didAdvance = false;

            while (aBand != aBandEnd && aBand->x0 <= x1) {
              x1 = blMax(x1, aBand->x1);
              aBand++;
              didAdvance = true;
            }

            while (bBand != bBandEnd && bBand->x0 <= x1) {
              x1 = blMax(x1, bBand->x1);
              bBand++;
              didAdvance = true;
            }

            if (!didAdvance)
              break;
          }

          #ifdef BL_BUILD_DEBUG
          BL_ASSERT(dstData != dstEnd);
          if (dstData != dstDataBand)
            BL_ASSERT(dstData[-1].x1 < x0);
          #endif

          dstData->reset(x0, y0, x1, y1);
          dstData++;
        } while (aBand != aBandEnd && bBand != bBandEnd);

        // Merge boxes which do not intersect.
        while (aBand != aBandEnd) {
          #ifdef BL_BUILD_DEBUG
          BL_ASSERT(dstData != dstEnd);
          if (dstData != dstDataBand)
            BL_ASSERT(dstData[-1].x1 < aBand->x0);
          #endif

          dstData->reset(aBand->x0, y0, aBand->x1, y1);
          dstData++;
          aBand++;
        }

        while (bBand != bBandEnd) {
          #ifdef BL_BUILD_DEBUG
          BL_ASSERT(dstData != dstEnd);
          if (dstData != dstDataBand)
            BL_ASSERT(dstData[-1].x1 < bBand->x0);
          #endif

          dstData->reset(bBand->x0, y0, bBand->x1, y1);
          dstData++;
          bBand++;
        }

        // Coalesce.
OnUnionBandDone:
        dstData = blRegionCoalesce(dstData, dstDataBand, y1, &prevBandSize);

        y0 = y1;

        // Advance A.
        if (aData->y1 == y1) {
          if ((aData = aBandEnd) == aEnd) break;
          aBandEnd = blRegionGetEndBand(aData, aEnd);
        }

        // Advance B.
        if (bData->y1 == y1) {
          if ((bData = bBandEnd) == bEnd) break;
          bBandEnd = blRegionGetEndBand(bData, bEnd);
        }

        y0 = blMax(y0, blMin(aData->y0, bData->y0));
      }

      if (aData != aEnd) goto OnMergeA;
      if (bData != bEnd) goto OnMergeB;
      break;
    }

    // ------------------------------------------------------------------------
    // [Xor]
    // ------------------------------------------------------------------------

    case BL_BOOLEAN_OP_XOR: {
      aBandEnd = blRegionGetEndBand(aData, aEnd);
      bBandEnd = blRegionGetEndBand(bData, bEnd);

      y0 = blMin(aData->y0, bData->y0);
      for (;;) {
        const BLBoxI* aBand = aData;
        const BLBoxI* bBand = bData;

        BL_REGION_ENSURE_SPACE(((size_t)(aBandEnd - aBand) + (size_t)(bBandEnd - bBand)) * 2, false);
        BLBoxI* dstDataBand = dstData;

        // Merge bands which do not intersect.
        if (bBand->y0 > y0) {
          y1 = blMin(aBand->y1, bBand->y0);
          do {
            BL_ASSERT(dstData != dstEnd);
            dstData->reset(aBand->x0, y0, aBand->x1, y1);
            dstData++;
          } while (++aBand != aBandEnd);
          goto OnXorBandDone;
        }

        if (aBand->y0 > y0) {
          y1 = blMin(bBand->y1, aBand->y0);
          do {
            BL_ASSERT(dstData != dstEnd);
            dstData->reset(bBand->x0, y0, bBand->x1, y1);
            dstData++;
          } while (++bBand != bBandEnd);
          goto OnXorBandDone;
        }

        // Vertical intersection of current A and B bands.
        y1 = blMin(aBand->y1, bBand->y1);
        BL_ASSERT(y0 < y1);

        {
          int pos = blMin(aBand->x0, bBand->x0);
          int x0;
          int x1;

          for (;;) {
            if (aBand->x1 <= bBand->x0) {
              x0 = blMax(aBand->x0, pos);
              x1 = aBand->x1;
              pos = x1;
              goto OnXorMerge;
            }

            if (bBand->x1 <= aBand->x0) {
              x0 = blMax(bBand->x0, pos);
              x1 = bBand->x1;
              pos = x1;
              goto OnXorMerge;
            }

            x0 = pos;
            x1 = blMax(aBand->x0, bBand->x0);
            pos = blMin(aBand->x1, bBand->x1);

            if (x0 >= x1)
              goto OnXorSkip;

OnXorMerge:
            BL_ASSERT(x0 < x1);
            if (dstData != dstDataBand && dstData[-1].x1 == x0) {
              dstData[-1].x1 = x1;
            }
            else {
              dstData->reset(x0, y0, x1, y1);
              dstData++;
            }

OnXorSkip:
            if (aBand->x1 <= pos) aBand++;
            if (bBand->x1 <= pos) bBand++;

            if (aBand == aBandEnd || bBand == bBandEnd)
              break;
            pos = blMax(pos, blMin(aBand->x0, bBand->x0));
          }

          // Merge boxes which do not intersect.
          if (aBand != aBandEnd) {
            x0 = blMax(aBand->x0, pos);
            for (;;) {
              x1 = aBand->x1;
              BL_ASSERT(x0 < x1);
              BL_ASSERT(dstData != dstEnd);

              if (dstData != dstDataBand && dstData[-1].x1 == x0) {
                dstData[-1].x1 = x1;
              }
              else {
                dstData->reset(x0, y0, x1, y1);
                dstData++;
              }

              if (++aBand == aBandEnd)
                break;
              x0 = aBand->x0;
            }
          }

          if (bBand != bBandEnd) {
            x0 = blMax(bBand->x0, pos);
            for (;;) {
              x1 = bBand->x1;
              BL_ASSERT(x0 < x1);
              BL_ASSERT(dstData != dstEnd);

              if (dstData != dstDataBand && dstData[-1].x1 == x0) {
                dstData[-1].x1 = x1;
              }
              else {
                dstData->reset(x0, y0, x1, y1);
                dstData++;
              }

              if (++bBand == bBandEnd)
                break;
              x0 = bBand->x0;
            }
          }
        }

        // Update bounding box and coalesce.
OnXorBandDone:
        if (dstDataBand != dstData) {
          dstBBoxX0 = blMin(dstBBoxX0, dstDataBand[0].x0);
          dstBBoxX1 = blMax(dstBBoxX1, dstData[-1].x1);
          dstData = blRegionCoalesce(dstData, dstDataBand, y1, &prevBandSize);
        }

        y0 = y1;

        // Advance A.
        if (aData->y1 == y1) {
          if ((aData = aBandEnd) == aEnd)
            break;
          aBandEnd = blRegionGetEndBand(aData, aEnd);
        }

        // Advance B.
        if (bData->y1 == y1) {
          if ((bData = bBandEnd) == bEnd)
            break;
          bBandEnd = blRegionGetEndBand(bData, bEnd);
        }

        y0 = blMax(y0, blMin(aData->y0, bData->y0));
      }

      if (aData != aEnd) goto OnMergeA;
      if (bData != bEnd) goto OnMergeB;
      break;
    }

    // ------------------------------------------------------------------------
    // [Subtract]
    // ------------------------------------------------------------------------

    case BL_BOOLEAN_OP_SUB: {
      aBandEnd = blRegionGetEndBand(aData, aEnd);
      bBandEnd = blRegionGetEndBand(bData, bEnd);

      y0 = blMin(aData->y0, bData->y0);
      for (;;) {
        const BLBoxI* aBand = aData;
        const BLBoxI* bBand = bData;

        BL_REGION_ENSURE_SPACE(((size_t)(aBandEnd - aBand) + (size_t)(bBandEnd - bBand)) * 2, false);
        BLBoxI* dstDataBand = dstData;

        // Merge (A) / Skip (B) bands which do not intersect.
        if (bBand->y0 > y0) {
          y1 = blMin(aBand->y1, bBand->y0);
          do {
            BL_ASSERT(dstData != dstEnd);
            dstData->reset(aBand->x0, y0, aBand->x1, y1);
            dstData++;
          } while (++aBand != aBandEnd);
          goto OnSubtractBandDone;
        }

        if (aBand->y0 > y0) {
          y1 = blMin(bBand->y1, aBand->y0);
          goto OnSubtractBandSkip;
        }

        // Vertical intersection of current A and B bands.
        y1 = blMin(aBand->y1, bBand->y1);
        BL_ASSERT(y0 < y1);

        {
          int pos = aBand->x0;
          int sub = bBand->x0;

          int x0;
          int x1;

          for (;;) {
            if (aBand->x1 <= sub) {
              x0 = pos;
              x1 = aBand->x1;
              pos = x1;

              if (x0 < x1)
                goto OnSubtractMerge;
              else
                goto OnSubtractSkip;
            }

            if (aBand->x0 >= sub) {
              pos = bBand->x1;
              goto OnSubtractSkip;
            }

            x0 = pos;
            x1 = bBand->x0;
            pos = bBand->x1;

OnSubtractMerge:
            BL_ASSERT(x0 < x1);
            dstData->reset(x0, y0, x1, y1);
            dstData++;

OnSubtractSkip:
            if (aBand->x1 <= pos) aBand++;
            if (bBand->x1 <= pos) bBand++;

            if (aBand == aBandEnd || bBand == bBandEnd)
              break;

            sub = bBand->x0;
            pos = blMax(pos, aBand->x0);
          }

          // Merge boxes (A) / Ignore boxes (B) which do not intersect.
          while (aBand != aBandEnd) {
            x0 = blMax(aBand->x0, pos);
            x1 = aBand->x1;

            if (x0 < x1) {
              BL_ASSERT(dstData != dstEnd);
              dstData->reset(x0, y0, x1, y1);
              dstData++;
            }
            aBand++;
          }
        }

        // Update bounding box and coalesce.
OnSubtractBandDone:
        if (dstDataBand != dstData) {
          dstBBoxX0 = blMin(dstBBoxX0, dstDataBand[0].x0);
          dstBBoxX1 = blMax(dstBBoxX1, dstData[-1].x1);
          dstData = blRegionCoalesce(dstData, dstDataBand, y1, &prevBandSize);
        }

OnSubtractBandSkip:
        y0 = y1;

        // Advance A.
        if (aData->y1 == y1) {
          if ((aData = aBandEnd) == aEnd)
            break;
          aBandEnd = blRegionGetEndBand(aData, aEnd);
        }

        // Advance B.
        if (bData->y1 == y1) {
          if ((bData = bBandEnd) == bEnd)
            break;
          bBandEnd = blRegionGetEndBand(bData, bEnd);
        }

        y0 = blMax(y0, blMin(aData->y0, bData->y0));
      }

      if (aData != aEnd)
        goto OnMergeA;
      break;
    }

    default:
      BL_NOT_REACHED();
  }

Done:
  n = (size_t)(dstData - dstI->data);
  dstI->size = n;

  if (!n)
    dstI->boundingBox.reset();
  else
    dstI->boundingBox.reset(dstBBoxX0, dstI->data[0].y0, dstBBoxX1, dstData[-1].y1);

  BL_ASSERT(blRegionImplIsValid(dstI));
  return oldI ? blRegionImplRelease(oldI) : BL_SUCCESS;

OutOfMemory:
  dstI->boundingBox.reset();
  dstI->size = 0;
  dst->impl = dstI;

  if (oldI) blRegionImplRelease(oldI);
  return blTraceError(BL_ERROR_OUT_OF_MEMORY);

OnMergeB:
  BL_ASSERT(aData == aEnd);

  aData = bData;
  aEnd = bEnd;
  aBandEnd = bBandEnd;

OnMergeA:
  BL_ASSERT(aData != aEnd);
  if (y0 >= aData->y1) {
    if ((aData = aBandEnd) == aEnd)
      goto Done;
    aBandEnd = blRegionGetEndBand(aData, aEnd);
  }

  y0 = blMax(y0, aData->y0);
  y1 = aData->y1;

  {
    BL_REGION_ENSURE_SPACE((size_t)(aEnd - aData), true);
    BLBoxI* dstDataBand = dstData;

    do {
      BL_ASSERT(dstData != dstEnd);
      dstData->reset(aData->x0, y0, aData->x1, y1);
      dstData++;
    } while (++aData != aBandEnd);

    dstBBoxX0 = blMin(dstBBoxX0, dstDataBand[0].x0);
    dstBBoxX1 = blMax(dstBBoxX1, dstData[-1].x1);
    dstData = blRegionCoalesce(dstData, dstDataBand, y1, &prevBandSize);
  }

  if (aData == aEnd)
    goto Done;

  if (op == BL_BOOLEAN_OP_OR) {
    // Special case for OR. The bounding box can be easily calculated
    // by using `A | B`. We don't have to do anthing else than that.
    do {
      BL_ASSERT(dstData != dstEnd);
      dstData->reset(aData->x0, aData->y0, aData->x1, aData->y1);
      dstData++;
    } while (++aData != aEnd);
  }
  else {
    do {
      BL_ASSERT(dstData != dstEnd);
      dstData->reset(aData->x0, aData->y0, aData->x1, aData->y1);
      dstData++;

      dstBBoxX0 = blMin(dstBBoxX0, aData->x0);
      dstBBoxX1 = blMax(dstBBoxX1, aData->x1);
    } while (++aData != aEnd);
  }

  goto Done;

  #undef BL_REGION_ENSURE_SPACE
}

BLResult blRegionCombine(BLRegionCore* self, const BLRegionCore* a, const BLRegionCore* b, uint32_t op) noexcept {
  const BLInternalRegionImpl* aI = blInternalCast(a->impl);
  const BLInternalRegionImpl* bI = blInternalCast(b->impl);

  if (BL_UNLIKELY(op >= BL_BOOLEAN_OP_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (BL_UNLIKELY(aI == bI)) {
    if (op == BL_BOOLEAN_OP_COPY || op == BL_BOOLEAN_OP_AND || op == BL_BOOLEAN_OP_OR)
      return blRegionAssignWeak(self, b);
    else
      return blRegionClear(self);
  }

  size_t aSize = aI->size;
  size_t bSize = bI->size;

  // Fast-paths that take advantage of box on any side.
  if (aSize <= 1) {
    BLBoxI aBox(aI->boundingBox);
    return blRegionCombineBR(self, &aBox, b, op);
  }

  if (bSize <= 1) {
    BLBoxI bBox(bI->boundingBox);
    return blRegionCombineRB(self, a, &bBox, op);
  }

  BL_ASSERT(aSize > 1 && bSize > 1);

  switch (op) {
    case BL_BOOLEAN_OP_COPY:
      return blRegionAssignWeak(self, b);

    case BL_BOOLEAN_OP_AND:
      if (!blOverlaps(aI->boundingBox, bI->boundingBox))
        return blRegionClear(self);
      break;

    case BL_BOOLEAN_OP_XOR:
      // Check whether to use UNION instead of XOR.
      if (blOverlaps(aI->boundingBox, bI->boundingBox))
        break;

      op = BL_BOOLEAN_OP_OR;
      BL_FALLTHROUGH

    case BL_BOOLEAN_OP_OR:
      // Check whether to use APPEND instead of OR. This is a special case, but
      // happens often when the region is constructed from many boxes using OR.
      if (aI->boundingBox.y1 <= bI->boundingBox.y0 || blRegionCanAppend(aI->data[aSize - 1], bI->data[0])) {
        if (self->impl == aI)
          return blRegionAppendSelf(self, bI->data, bSize, bI->boundingBox);

        if (self->impl == bI) {
          BLRegion bTmp(*blDownCast(b));
          return blRegionAppendAB(self, aI->data, aSize, aI->boundingBox, bI->data, bSize, bI->boundingBox);
        }

        return blRegionAppendAB(self, aI->data, aSize, aI->boundingBox, bI->data, bSize, bI->boundingBox);
      }

      if (bI->boundingBox.y1 <= aI->boundingBox.y0 || blRegionCanAppend(bI->data[bSize - 1], aI->data[0])) {
        if (self->impl == bI)
          return blRegionAppendSelf(self, aI->data, aSize, aI->boundingBox);

        if (self->impl == aI) {
          BLRegion aTmp(*blDownCast(a));
          return blRegionAppendAB(self, bI->data, bSize, bI->boundingBox, aI->data, aSize, aI->boundingBox);
        }

        return blRegionAppendAB(self, bI->data, bSize, bI->boundingBox, aI->data, aSize, aI->boundingBox);
      }
      break;

    case BL_BOOLEAN_OP_SUB:
      if (!blOverlaps(aI->boundingBox, bI->boundingBox))
        return blRegionAssignWeak(self, a);
      break;
  }

  return blRegionCombineInternal(self, aI->data, aSize, aI->boundingBox, bI->data, bSize, bI->boundingBox, op, self->impl == aI || self->impl == bI);
}

BLResult blRegionCombineRB(BLRegionCore* self, const BLRegionCore* a, const BLBoxI* b, uint32_t op) noexcept {
  BLInternalRegionImpl* aI = blInternalCast(a->impl);

  // Fast-path.
  if (aI->size <= 1)
    return blRegionCombineBB(self, &aI->boundingBox, b, op);

  BLBoxI bBox(*b);
  bool bIsValid = blIsValid(bBox);

  switch (op) {
    case BL_BOOLEAN_OP_COPY:
      if (!bIsValid)
        return blRegionClear(self);
      else
        return blRegionAssignBoxI(self, &bBox); // DST <- B.

    case BL_BOOLEAN_OP_AND:
      if (!bIsValid || !blOverlaps(bBox, aI->boundingBox))
        return blRegionClear(self);

      if (blSubsumes(bBox, aI->boundingBox))
        return blRegionAssignWeak(self, a);

      return blRegionIntersectBox(self, a, &bBox);

    case BL_BOOLEAN_OP_OR:
    case BL_BOOLEAN_OP_XOR:
      if (!bIsValid)
        return blRegionAssignWeak(self, a);

      if (aI->boundingBox.y1 <= bBox.y0 || blRegionCanAppend(aI->data[aI->size - 1], bBox)) {
        if (self->impl == aI)
          return blRegionAppendSelf(self, &bBox, 1, bBox);
        else
          return blRegionAppendAB(self, aI->data, aI->size, aI->boundingBox, &bBox, 1, bBox);
      }
      break;

    case BL_BOOLEAN_OP_SUB:
      if (!bIsValid || !blOverlaps(bBox, aI->boundingBox))
        return blRegionAssignWeak(self, a);
      break;

    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  return blRegionCombineInternal(self, aI->data, aI->size, aI->boundingBox, &bBox, 1, bBox, op, self->impl == aI);
}

BLResult blRegionCombineBR(BLRegionCore* self, const BLBoxI* a, const BLRegionCore* b, uint32_t op) noexcept {
  const BLInternalRegionImpl* bI = blInternalCast(b->impl);

  // Box-Box is faster than Box-Region so use this fast-path when possible.
  if (bI->size <= 1) {
    BLBoxI bBox(bI->boundingBox);
    return blRegionCombineBB(self, a, &bBox, op);
  }

  BLBoxI aBox(*a);
  bool aIsValid = blIsValid(aBox);

  switch (op) {
    case BL_BOOLEAN_OP_COPY:
      return blRegionAssignWeak(self, b);

    case BL_BOOLEAN_OP_AND:
      if (!aIsValid || !blOverlaps(aBox, bI->boundingBox))
        return blRegionClear(self);

      if (blSubsumes(aBox, bI->boundingBox))
        return blRegionAssignWeak(self, b);

      return blRegionIntersectBox(self, b, &aBox);

    case BL_BOOLEAN_OP_OR:
      if (!aIsValid)
        return blRegionAssignWeak(self, b);
      break;

    case BL_BOOLEAN_OP_XOR:
      if (!aIsValid)
        return blRegionAssignWeak(self, b);

      if (!blOverlaps(aBox, bI->boundingBox))
        op = BL_BOOLEAN_OP_OR;
      break;

    case BL_BOOLEAN_OP_SUB:
      if (!aIsValid)
        return blRegionClear(self);

      if (!blOverlaps(aBox, bI->boundingBox))
        return blRegionAssignBoxI(self, &aBox);
      break;

    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  return blRegionCombineInternal(self, &aBox, 1, aBox, bI->data, bI->size, bI->boundingBox, op, self->impl == bI);
}

BLResult blRegionCombineBB(BLRegionCore* self, const BLBoxI* a, const BLBoxI* b, uint32_t op) noexcept {
  if (BL_UNLIKELY(op >= BL_BOOLEAN_OP_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  // There are several combinations of A and B.
  //
  // Special cases:
  //
  //   1) Both boxes are invalid.
  //   2) Only one box is valid (one invalid).
  //   3) Boxes do not intersect, but they are continuous on the y-axis (In some
  //      cases this can be extended that boxes intersect, but their left and
  //      right coordinates are shared).
  //   4) Boxes do not intersect, but they are continuous on the x-axis (In some
  //      cases this can be extended that boxes intersect, but their top and
  //      bottom coordinates are shared).
  //
  // Common cases:
  //
  //   5) Boxes do not intersect and do not share any area on the y-axis.
  //   6) Boxes do not intersect, but they share any area on the y-axis.
  //   7) Boxes intersect.
  //   8) One box overlaps the other.
  //
  //   +--------------+--------------+--------------+--------------+
  //   |       1)     |       2)     |       3)     |       4)     |
  //   |              |              |              |              |
  //   |              |  +--------+  |  +--------+  |  +----+----+ |
  //   |              |  |        |  |  |        |  |  |    |    | |
  //   |              |  |        |  |  |        |  |  |    |    | |
  //   |              |  |        |  |  +--------+  |  |    |    | |
  //   |              |  |        |  |  |        |  |  |    |    | |
  //   |              |  |        |  |  |        |  |  |    |    | |
  //   |              |  |        |  |  |        |  |  |    |    | |
  //   |              |  +--------+  |  +--------+  |  +----+----+ |
  //   |              |              |              |              |
  //   +--------------+--------------+--------------+--------------+
  //   |       5)     |       6)     |       7)     |       8)     |
  //   |              |              |              |              |
  //   |  +----+      | +---+        |  +-----+     |  +--------+  |
  //   |  |    |      | |   |        |  |     |     |  |        |  |
  //   |  |    |      | |   | +----+ |  |  +--+--+  |  |  +--+  |  |
  //   |  +----+      | +---+ |    | |  |  |  |  |  |  |  |  |  |  |
  //   |              |       |    | |  +--+--+  |  |  |  +--+  |  |
  //   |      +----+  |       +----+ |     |     |  |  |        |  |
  //   |      |    |  |              |     +-----+  |  +--------+  |
  //   |      |    |  |              |              |              |
  //   |      +----+  |              |              |              |
  //   +--------------+--------------+--------------+--------------+

  BLBoxI box[4];  // Maximum number of generated boxes by any operator is 4.
  size_t n = 1;   // We assume 1 box by default, overriden when needed.

  // COPY & AND & SUB Operators
  // --------------------------

  if (op == BL_BOOLEAN_OP_COPY) {
Copy:
    if (!blIsValid(*b))
      goto Clear;

    box[0] = *b;
    goto Done;
  }

  if (op == BL_BOOLEAN_OP_AND) {
    if (!blIntersectBoxes(box[0], *a, *b))
      goto Clear; // Case 1, 2, 3, 4, 5, 6.
    else
      goto Done;  // Case 7, 8 (has intersection).
  }

  // From now we assume A to be the output in case of bailing out early.
  box[0] = *a;

  if (op == BL_BOOLEAN_OP_SUB) {
    // case 1, 2.
    if (!blIsValid(*a)) goto Clear;
    if (!blIsValid(*b)) goto Done; // Copy A.

    // Case 3, 4, 5, 6.
    // If the input boxes A and B do not intersect then the result is A.
    if (!blIntersectBoxes(box[3], *a, *b))
      goto Done;

    // Case 7, 8.

    // Top part.
    n = size_t(a->y0 < b->y0);
    box[0].reset(a->x0, a->y0, a->x1, box[3].y0);

    // Inner part.
    if (a->x0 < box[3].x0) box[n++].reset(a->x0, box[3].y0, box[3].x0, box[3].y1);
    if (box[3].x1 < a->x0) box[n++].reset(box[3].x1, box[3].y0, a->x1, box[3].y1);

    // Bottom part.
    if (a->y1 > box[3].y1) box[n++].reset(a->x0, box[3].y1, a->x1, a->y1);

    if (!n)
      goto Clear;
    else
      goto Done;
  }

  // OR & XOR Operators
  // ------------------

  // The order of boxes doesn't matter for next operators, so make the upper first.
  if (a->y0 > b->y0)
    std::swap(a, b);

  // Case 1, 2.
  if (!blIsValid(*a)) goto Copy; // Copy B (if valid).
  if (!blIsValid(*b)) goto Done; // Copy A.

  if (op == BL_BOOLEAN_OP_XOR) {
    if (blIntersectBoxes(box[3], *a, *b)) {
      // Case 7, 8.

      // Top part.
      n = size_t(a->y0 < b->y0);
      box[0].reset(a->x0, a->y0, a->x1, b->y0);

      // Inner part.
      if (a->x0 > b->x0)
        std::swap(a, b);

      if (a->x0 < box[3].x0) box[n++].reset(a->x0, box[3].y0, box[3].x0, box[3].y1);
      if (b->x1 > box[3].x1) box[n++].reset(box[3].x1, box[3].y0, b->x1, box[3].y1);

      // Bottom part.
      if (a->y1 > b->y1) std::swap(a, b);
      if (b->y1 > box[3].y1) box[n++].reset(b->x0, box[3].y1, b->x1, b->y1);

      if (!n)
        goto Clear;
      else
        goto Done;
    }
    else {
      // Case 3, 4, 5, 6.
      // If the input boxes A and B do not intersect then we fall to OR operator.
    }
  }

  // OR or non-intersecting XOR:
  {
    if (a->y1 <= b->y0) {
      // Case 3, 5.
      box[0] = *a;
      box[1] = *b;
      n = 2;

      // Coalesce (Case 3).
      if ((box[0].y1 == box[1].y0) & (box[0].x0 = box[1].x0) & (box[0].x1 == box[1].x1)) {
        box[0].y1 = box[1].y1;
        n = 1;
      }

      goto Done;
    }

    if (a->y0 == b->y0 && a->y1 == b->y1) {
      // Case 4 - with addition that rectangles can intersect.
      //        - with addition that rectangles do not need to be continuous on the x-axis.
      box[0].y0 = a->y0;
      box[0].y1 = a->y1;

      if (a->x0 > b->x0)
        std::swap(a, b);

      box[0].x0 = a->x0;
      box[0].x1 = a->x1;

      // Intersects or continuous.
      if (b->x0 <= a->x1) {
        if (b->x1 > a->x1)
          box[0].x1 = b->x1;
        n = 1;
      }
      else {
        box[1].reset(b->x0, box[0].y0, b->x1, box[0].y1);
        n = 2;
      }
    }
    else {
      // Case 6, 7, 8.
      BL_ASSERT(b->y0 < a->y1);

      // Top part.
      n = size_t(a->y0 < b->y0);
      box[0].reset(a->x0, a->y0, a->x1, b->y0);

      // Inner part.
      int iy0 = b->y0;
      int iy1 = blMin(a->y1, b->y1);

      if (a->x0 > b->x0)
        std::swap(a, b);

      int ix0 = blMax(a->x0, b->x0);
      int ix1 = blMin(a->x1, b->x1);

      if (ix0 > ix1) {
        box[n + 0].reset(a->x0, iy0, a->x1, iy1);
        box[n + 1].reset(b->x0, iy0, b->x1, iy1);
        n += 2;
      }
      else {
        BL_ASSERT(a->x1 >= ix0 && b->x0 <= ix1);

        // If the A or B subsumes the intersection area, extend also the iy1
        // and skip the bottom part (we append it).
        if (a->x0 <= ix0 && a->x1 >= ix1 && iy1 < a->y1) iy1 = a->y1;
        if (b->x0 <= ix0 && b->x1 >= ix1 && iy1 < b->y1) iy1 = b->y1;
        box[n].reset(a->x0, iy0, blMax(a->x1, b->x1), iy1);

        // Coalesce.
        if (n == 1 && box[0].x0 == box[1].x0 && box[0].x1 == box[1].x1)
          box[0].y1 = box[1].y1;
        else
          n++;
      }

      // Bottom part.
      if (a->y1 > iy1) {
        box[n].reset(a->x0, iy1, a->x1, a->y1);
        goto UnionLastCoalesce;
      }
      else if (b->y1 > iy1) {
        box[n].reset(b->x0, iy1, b->x1, b->y1);
UnionLastCoalesce:
        if (n == 1 && box[0].x0 == box[1].x0 && box[0].x1 == box[1].x1)
          box[0].y1 = box[1].y1;
        else
          n++;
      }
    }

    goto Done;
  }

Clear:
  return blRegionClear(self);

Done:
  BL_ASSUME(n > 0);
  return blRegionAssignValidBoxIArray(self, box, n);
}

// ============================================================================
// [BLRegion - Translate]
// ============================================================================

BLResult blRegionTranslate(BLRegionCore* self, const BLRegionCore* r, const BLPointI* pt) noexcept {
  BLInternalRegionImpl* dstI = blInternalCast(self->impl);
  BLInternalRegionImpl* srcI = blInternalCast(r->impl);

  int tx = pt->x;
  int ty = pt->y;

  if ((tx | ty) == 0)
    return blRegionAssignWeak(self, r);

  size_t n = srcI->size;
  if (!n)
    return blRegionClear(self);

  // If the translation cause arithmetic overflow then we first clip the input
  // region into a safe boundary which might be translated without overflow.
  BLOverflowFlag of = 0;
  BLBoxI bbox(blAddOverflow(srcI->boundingBox.x0, tx, &of),
              blAddOverflow(srcI->boundingBox.y0, ty, &of),
              blAddOverflow(srcI->boundingBox.x1, tx, &of),
              blAddOverflow(srcI->boundingBox.y1, ty, &of));
  if (BL_UNLIKELY(of))
    return blRegionTranslateAndClip(self, r, pt, &blRegionLargestBoxI);

  BLInternalRegionImpl* oldI = nullptr;
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(dstI));

  if ((n | immutableMsk) > dstI->capacity) {
    oldI = dstI;
    dstI = blRegionImplNew(blRegionFittingCapacity(n));

    if (BL_UNLIKELY(!dstI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    self->impl = dstI;
  }

  dstI->size = n;
  dstI->boundingBox = bbox;

  BLBoxI* dstData = dstI->data;
  BLBoxI* srcData = srcI->data;

  for (size_t i = 0; i < n; i++)
    dstData[i].reset(srcData[i].x0 + tx, srcData[i].y0 + ty, srcData[i].x1 + tx, srcData[i].y1 + ty);

  BL_ASSERT(blRegionImplIsValid(dstI));
  return oldI ? blRegionImplRelease(oldI) : BL_SUCCESS;
}

BLResult blRegionTranslateAndClip(BLRegionCore* self, const BLRegionCore* r, const BLPointI* pt, const BLBoxI* clipBox) noexcept {
  BLInternalRegionImpl* dstI = blInternalCast(self->impl);
  BLInternalRegionImpl* srcI = blInternalCast(r->impl);

  size_t n = srcI->size;
  if (!n || !blIsValid(*clipBox))
    return blRegionClear(self);

  int tx = pt->x;
  int ty = pt->y;

  // Use faster `blRegionIntersectBox()` in case that there is no translation.
  if ((tx | ty) == 0)
    return blRegionIntersectBox(self, r, clipBox);

  int cx0 = clipBox->x0;
  int cy0 = clipBox->y0;
  int cx1 = clipBox->x1;
  int cy1 = clipBox->y1;

  // This function is possibly called also by `blRegionTranslate()` in case that
  // the translation would overflow. We adjust the given clipBox in a way so it
  // would never overflow and then pre-translate it so we can first clip and then
  // translate safely.
  if (tx < 0) {
    cx0 = blMin(cx0, blMaxValue<int>() + tx);
    cx1 = blMin(cx1, blMaxValue<int>() + tx);
  }
  else if (tx > 0) {
    cx0 = blMax(cx0, blMinValue<int>() + tx);
    cx1 = blMax(cx1, blMinValue<int>() + tx);
  }

  if (ty < 0) {
    cy0 = blMin(cy0, blMaxValue<int>() + ty);
    cy1 = blMin(cy1, blMaxValue<int>() + ty);
  }
  else if (ty > 0) {
    cy0 = blMax(cy0, blMinValue<int>() + ty);
    cy1 = blMax(cy1, blMinValue<int>() + ty);
  }

  // Pre-translate clipBox.
  cx0 -= tx;
  cy0 -= ty;
  cx1 -= tx;
  cy1 -= ty;

  if (cx0 >= cx1 || cy0 >= cy1)
    return blRegionClear(self);

  BLBoxI* srcData = srcI->data;
  BLBoxI* srcEnd = srcData + n;

  // Skip boxes which do not intersect with `clipBox`. We do it here as we can
  // change the size requirements of the new region if we skip some boxes here.
  while (srcData->y1 <= cy0)
    if (++srcData == srcEnd)
      return blRegionClear(self);

  while (srcEnd[-1].y0 >= cy1)
    if (--srcEnd == srcData)
      return blRegionClear(self);

  n = (size_t)(srcEnd - srcData);

  // Make sure there is enough space in the destination region.
  BLInternalRegionImpl* oldI = nullptr;
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(dstI));

  if ((n | immutableMsk) > dstI->capacity) {
    oldI = dstI;
    dstI = blRegionImplNew(blRegionFittingCapacity(n));

    if (BL_UNLIKELY(!dstI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    self->impl = dstI;
  }

  BLBoxI* dstData = dstI->data;
  size_t prevBandSize = SIZE_MAX;

  int dstBBoxX0 = blMaxValue<int>();
  int dstBBoxX1 = blMinValue<int>();

  // Do the intersection part.
  for (;;) {
    BL_ASSERT(srcData != srcEnd);
    if (srcData->y0 >= cy1)
      break;

    int y0;
    int y1 = 0; // Be quiet.
    int bandY0 = srcData->y0;

    // Skip leading boxes which do not intersect with `clipBox`.
    if (srcData->x1 <= cx0) {
      do {
        if (++srcData == srcEnd)
          goto Done;
      } while (srcData->x1 <= cx0);

      if (srcData->y0 != bandY0)
        continue;
    }

    // Do the inner part.
    if (srcData->x0 < cx1) {
      BLBoxI* dstCurBand = dstData;
      y0 = blMax(srcData->y0, cy0) + ty;
      y1 = blMin(srcData->y1, cy1) + ty;

      // First box (could clip).
      BL_ASSERT(dstData != dstI->data + dstI->capacity);
      dstData->reset(blMax(srcData->x0, cx0) + tx, y0, blMin(srcData->x1, cx1) + tx, y1);
      dstData++;

      if (++srcData == srcEnd || srcData->y0 != bandY0)
        goto EndBand;

      // Inner boxes (won't clip).
      while (srcData->x1 <= cx1) {
        BL_ASSERT(dstData != dstI->data + dstI->capacity);
        BL_ASSERT(srcData->x0 >= cx0 && srcData->x1 <= cx1);

        dstData->reset(srcData->x0 + tx, y0, srcData->x1 + tx, y1);
        dstData++;

        if (++srcData == srcEnd || srcData->y0 != bandY0)
          goto EndBand;
      }

      // Last box (could clip).
      if (srcData->x0 < cx1) {
        BL_ASSERT(dstData != dstI->data + dstI->capacity);
        BL_ASSERT(srcData->x0 >= cx0);

        dstData->reset(srcData->x0 + tx, y0, blMin(srcData->x1, cx1) + tx, y1);
        dstData++;

        if (++srcData == srcEnd || srcData->y0 != bandY0)
          goto EndBand;
      }

      BL_ASSERT(srcData->x0 >= cx1);

EndBand:
      dstBBoxX0 = blMin(dstBBoxX0, dstCurBand[0].x0);
      dstBBoxX1 = blMax(dstBBoxX1, dstData[-1].x1);
      dstData = blRegionCoalesce(dstData, dstCurBand, y1, &prevBandSize);

      if (srcData == srcEnd)
        break;
    }
    else {
      // Skip trailing boxes which do not intersect with `clipBox`.
      while (srcData->x0 >= cx1)
        if (++srcData == srcEnd)
          goto Done;
    }
  }

Done:
  n = (size_t)(dstData - dstI->data);
  dstI->size = n;

  if (!n)
    dstI->boundingBox.reset();
  else
    dstI->boundingBox.reset(dstBBoxX0, dstI->data[0].y0, dstBBoxX1, dstData[-1].y1);

  BL_ASSERT(blRegionImplIsValid(dstI));
  return oldI ? blRegionImplRelease(oldI) : BL_SUCCESS;
}

// ============================================================================
// [BLRegion - Intersect]
// ============================================================================

BLResult blRegionIntersectAndClip(BLRegionCore* self, const BLRegionCore* a, const BLRegionCore* b, const BLBoxI* clipBox) noexcept {
  const BLInternalRegionImpl* aI = blInternalCast(a->impl);
  const BLInternalRegionImpl* bI = blInternalCast(b->impl);

  int cx0 = blMax(clipBox->x0, aI->boundingBox.x0, bI->boundingBox.x0);
  int cy0 = blMax(clipBox->y0, aI->boundingBox.y0, bI->boundingBox.y0);
  int cx1 = blMin(clipBox->x1, aI->boundingBox.x1, bI->boundingBox.x1);
  int cy1 = blMin(clipBox->y1, aI->boundingBox.y1, bI->boundingBox.y1);

  // This would handle empty `a` or `b`, non-intersecting regions, and invalid
  // `clipBox` as well.
  if ((cx0 >= cx1) | (cy0 >= cy1))
    return blRegionClear(self);

  size_t aSize = aI->size;
  size_t bSize = bI->size;

  if ((aSize == 1) | (bSize == 1)) {
    BLBoxI newClipBox(cx0, cy0, cx1, cy1);
    if (aSize != 1) return blRegionIntersectBox(self, a, &newClipBox);
    if (bSize != 1) return blRegionIntersectBox(self, b, &newClipBox);
    return blRegionAssignBoxI(self, &newClipBox);
  }

  // These define input regions [aData:aEnd] and [bData:bEnd].
  const BLBoxI* aData = aI->data;
  const BLBoxI* bData = bI->data;

  const BLBoxI* aEnd = aData + aSize;
  const BLBoxI* bEnd = bData + bSize;

  // Skip all parts which do not intersect.
  while (aData->y0 <= cy0) { if (++aData == aEnd) return blRegionClear(self); }
  while (bData->y0 <= cy0) { if (++bData == bEnd) return blRegionClear(self); }

  for (;;) {
    bool cont = false;

    while (aData->y1 <= bData->y0) { cont = true; if (++aData == aEnd) return blRegionClear(self); }
    while (bData->y1 <= aData->y0) { cont = true; if (++bData == bEnd) return blRegionClear(self); }

    if (cont)
      continue;

    while ((aData->x1 <= cx0) | (aData->x0 >= cx1)) { cont = true; if (++aData == aEnd) return blRegionClear(self); }
    while ((bData->x1 <= cx0) | (bData->x0 >= cx1)) { cont = true; if (++bData == bEnd) return blRegionClear(self); }

    if (!cont)
      break;
  }

  if (aData->y0 >= cy1 || bData->y0 >= cy1)
    return blRegionClear(self);

  // If we are here it means there are still some `a` and `b` boxes left.
  BL_ASSERT(aData != aEnd);
  BL_ASSERT(bData != bEnd);

  // Update aSize and bSize so the estimated size is closer to the result.
  aSize = (size_t)(aEnd - aData);
  bSize = (size_t)(bEnd - bData);

  // The maximum number of boxes this operation can generate is (A + B) * 2.
  //
  // NOTE: The calculation cannot overflow due to the size of `BLBoxI`.
  size_t n = (aSize + bSize) * 2;

  BLInternalRegionImpl* oldI = nullptr;
  BLInternalRegionImpl* dstI = blInternalCast(self->impl);

  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(dstI));
  if ((dstI == aI) | (dstI == bI) | ((n | immutableMsk) > dstI->capacity)) {
    oldI = dstI;
    dstI = blRegionImplNew(n);

    if (BL_UNLIKELY(!dstI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  BLBoxI* dstData = dstI->data;
  size_t prevBandSize = SIZE_MAX;

  const BLBoxI* aBandEnd = blRegionGetEndBand(aData, aEnd);
  const BLBoxI* bBandEnd = blRegionGetEndBand(bData, bEnd);

  int dstBBoxX0 = blMaxValue<int>();
  int dstBBoxX1 = blMinValue<int>();

  for (;;) {
    int ym = blMin(aData->y1, bData->y1);

    // Vertical intersection of current A and B bands.
    int y0 = blMax(aData->y0, blMax(bData->y0, cy0));
    int y1 = blMin(cy1, ym);

    if (y0 < y1) {
      const BLBoxI* aBand = aData;
      const BLBoxI* bBand = bData;
      BLBoxI* dstCurBand = dstData;

      for (;;) {
        // Skip boxes which do not intersect.
        if (aBand->x1 <= bBand->x0) { if (++aBand == aBandEnd) goto EndBand; else continue; }
        if (bBand->x1 <= aBand->x0) { if (++bBand == bBandEnd) goto EndBand; else continue; }

        // Horizontal intersection of current A and B boxes.
        int x0 = blMax(aBand->x0, bBand->x0, cx0);
        int xm = blMin(aBand->x1, bBand->x1);
        int x1 = blMin(cx1, xm);

        if (x0 < x1) {
          BL_ASSERT(dstData != dstI->data + dstI->capacity);
          dstData->reset(x0, y0, x1, y1);
          dstData++;
        }

        // Advance.
        if (aBand->x1 == xm && (++aBand == aBandEnd || aBand->x0 >= cx1)) break;
        if (bBand->x1 == xm && (++bBand == bBandEnd || bBand->x0 >= cx1)) break;
      }

      // Update bounding box and coalesce.
EndBand:
      if (dstCurBand != dstData) {
        dstBBoxX0 = blMin(dstBBoxX0, dstCurBand[0].x0);
        dstBBoxX1 = blMax(dstBBoxX1, dstData[-1].x1);
        dstData = blRegionCoalesce(dstData, dstCurBand, y1, &prevBandSize);
      }
    }

    // Advance A.
    if (aData->y1 == ym) {
      aData = aBandEnd;
      if (aData == aEnd || aData->y0 >= cy1)
        break;

      while (aData->x1 <= cx0 || aData->x0 >= cx1)
        if (++aData == aEnd)
          goto Done;

      aBandEnd = blRegionGetEndBand(aData, aEnd);
    }

    // Advance B.
    if (bData->y1 == ym) {
      bData = bBandEnd;
      if (bData == bEnd || bData->y0 >= cy1)
        break;

      while (bData->x1 <= cx0 || bData->x0 >= cx1)
        if (++bData == bEnd)
          goto Done;

      bBandEnd = blRegionGetEndBand(bData, bEnd);
    }
  }

Done:
  n = (size_t)(dstData - dstI->data);
  dstI->size = n;

  if (!n)
    dstI->boundingBox.reset();
  else
    dstI->boundingBox.reset(dstBBoxX0, dstI->data[0].y0, dstBBoxX1, dstData[-1].y1);

  BL_ASSERT(blRegionImplIsValid(dstI));
  return oldI ? blRegionImplRelease(oldI) : BL_SUCCESS;
}

// ============================================================================
// [BLRegion - Equals]
// ============================================================================

bool blRegionEquals(const BLRegionCore* a, const BLRegionCore* b) noexcept {
  const BLInternalRegionImpl* aI = blInternalCast(a->impl);
  const BLInternalRegionImpl* bI = blInternalCast(b->impl);

  size_t size = aI->size;

  if (aI == bI)
    return true;

  if (size != bI->size || aI->boundingBox != bI->boundingBox)
    return false;

  return memcmp(aI->data, bI->data, size * sizeof(BLBoxI)) == 0;
}

// ============================================================================
// [BLRegion - Type]
// ============================================================================

uint32_t blRegionGetType(const BLRegionCore* self) noexcept {
  return uint32_t(blMin<size_t>(self->impl->size, BL_REGION_TYPE_COMPLEX));
}

// ============================================================================
// [BLRegion - HitTest]
// ============================================================================

uint32_t blRegionHitTest(const BLRegionCore* self, const BLPointI* pt) noexcept {
  const BLInternalRegionImpl* selfI = blInternalCast(self->impl);

  const BLBoxI* data = selfI->data;
  size_t n = selfI->size;

  BLRegionXYMatcher m(pt->x, pt->y);
  if (!selfI->boundingBox.contains(m.x, m.y))
    return BL_HIT_TEST_OUT;

  // If the bounding-box check passed the size MUST be greater than zero.
  BL_ASSUME(n > 0);

  size_t i = blBinarySearchClosestFirst(data, n, m);
  return data[i].contains(m.x, m.y) ? BL_HIT_TEST_IN : BL_HIT_TEST_OUT;
}

uint32_t blRegionHitTestBoxI(const BLRegionCore* self, const BLBoxI* box) noexcept {
  const BLInternalRegionImpl* selfI = blInternalCast(self->impl);

  const BLBoxI* data = selfI->data;
  size_t n = selfI->size;

  if (!blIsValid(*box))
    return BL_HIT_TEST_INVALID;

  int bx0 = box->x0;
  int by0 = box->y0;
  int bx1 = box->x1;
  int by1 = box->y1;

  if ((bx0 >= selfI->boundingBox.x1) | (by0 >= selfI->boundingBox.y1) |
      (bx1 <= selfI->boundingBox.x0) | (by1 <= selfI->boundingBox.y0))
    return BL_HIT_TEST_OUT;

  // If the bounding-box check passed the size MUST be greater than zero.
  BL_ASSUME(n > 0);

  const BLBoxI* end = data + n;
  data += blBinarySearchClosestFirst(data, n, BLRegionXYMatcher(bx0, by0));

  // [data:end] is our new working set, there is nothing to do if it's empty.
  if (data == end)
    return BL_HIT_TEST_OUT;

  // Initially we assume that the hit-test would be BL_HIT_TEST_IN, which
  // means that all parts of the input box are within the region. When we
  // fail this condition we would try to match BL_HIT_TEST_PART and if
  // that fails we would return BL_HIT_TEST_OUT.
  if (data->y0 <= by0) {
    // 1. If the box is split into multiple region bands then each band must
    //    contain a single rectangle that fully subsumes `box` to have a
    //    full-in result.
    // 2. In addition, each band must follow the previous band, if there is
    //    a gap then the hit-test cannot return full-in.
    int ry0 = data->y0;
    for (;;) {
      int ry1 = data->y1;

      while (data->x1 <= bx0 && ++data == end)
        return ry0 > by0 ? BL_HIT_TEST_PART : BL_HIT_TEST_OUT;

      if (data->x0 >= bx1)
        return ry0 > by0 ? BL_HIT_TEST_PART : BL_HIT_TEST_OUT;

      // Now we know that the box pointed by data intersects our input box.
      // We don't know, however, whether we skipped multiple bands during
      // the loop and whether the box at `data[0]` fully subsumes our input
      // box.
      if ((data->y0 != ry0) | (data->x0 > bx0) | (data->x1 < bx1))
        return BL_HIT_TEST_PART;

      // Last important band.
      if (by1 <= ry1)
        return BL_HIT_TEST_IN;

      // Skip all remaining rectangles in this band.
      do {
        if (++data == end)
          return BL_HIT_TEST_PART;
      } while (data[0].y0 == ry0);

      // It would be a partial hit if the next band doesn't follow this band.
      ry0 = ry1;
      if (ry0 != data->y0)
        return BL_HIT_TEST_PART;
    }
  }

  // Partial hit at most.
  while (data->y0 < by1) {
    if ((data->x0 < bx1) & (data->x1 > bx0))
      return BL_HIT_TEST_PART;
    if (++data == end)
      break;
  }

  return BL_HIT_TEST_OUT;
}

// ============================================================================
// [BLRegion - Runtime Init]
// ============================================================================

void blRegionRtInit(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);

  BLInternalRegionImpl* regionI = &blNullRegionImpl;
  regionI->implType = uint8_t(BL_IMPL_TYPE_REGION);
  regionI->implTraits = uint8_t(BL_IMPL_TRAIT_NULL);
  blAssignBuiltInNull(regionI);
}
