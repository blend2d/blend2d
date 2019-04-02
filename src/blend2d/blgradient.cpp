// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blarray_p.h"
#include "./blarrayops_p.h"
#include "./blmath_p.h"
#include "./blformat_p.h"
#include "./blgradient_p.h"
#include "./blpixelops_p.h"
#include "./blrgba_p.h"
#include "./blruntime_p.h"
#include "./blsupport_p.h"
#include "./bltables_p.h"

// ============================================================================
// [Global Variables]
// ============================================================================

static BLWrap<BLInternalGradientImpl> blNullGradientImpl;

static constexpr const double blGradientNoValues[BL_GRADIENT_VALUE_COUNT] = { 0.0 };
static constexpr const BLMatrix2D blGradientNoMatrix(1.0, 0.0, 0.0, 1.0, 0.0, 0.0);

BLGradientOps blGradientOps;

// ============================================================================
// [BLGradient - Capacity]
// ============================================================================

static constexpr size_t blGradientImplSizeOf(size_t n = 0) noexcept {
  return sizeof(BLInternalGradientImpl) + n * sizeof(BLGradientStop);
}

static constexpr size_t blGradientCapacityOf(size_t implSize) noexcept {
  return (implSize - blGradientImplSizeOf()) / sizeof(BLGradientStop);
}

static constexpr size_t blGradientInitialCapacity() noexcept {
  return blGradientCapacityOf(BL_ALLOC_HINT_GRADIENT);
}

static BL_INLINE size_t blGradientFittingCapacity(size_t n) noexcept {
  return blContainerFittingCapacity(blGradientImplSizeOf(), sizeof(BLGradientStop), n);
}

static BL_INLINE size_t blGradientGrowingCapacity(size_t n) noexcept {
  return blContainerGrowingCapacity(blGradientImplSizeOf(), sizeof(BLGradientStop), n, BL_ALLOC_HINT_GRADIENT);
}

// ============================================================================
// [BLGradient - Tables]
// ============================================================================

struct BLGradientValueCountTableGen {
  static constexpr uint8_t value(size_t i) noexcept {
    return i == BL_GRADIENT_TYPE_LINEAR  ? uint8_t(sizeof(BLLinearGradientValues ) / sizeof(double)) :
           i == BL_GRADIENT_TYPE_RADIAL  ? uint8_t(sizeof(BLRadialGradientValues ) / sizeof(double)) :
           i == BL_GRADIENT_TYPE_CONICAL ? uint8_t(sizeof(BLConicalGradientValues) / sizeof(double)) : uint8_t(0);
  }
};

static constexpr const auto blGradientValueCountTable =
  blLookupTable<uint8_t, BL_GRADIENT_TYPE_COUNT, BLGradientValueCountTableGen>();

// ============================================================================
// [BLGradient - Analysis]
// ============================================================================

static BL_INLINE uint32_t blGradientAnalyzeStopArray(const BLGradientStop* stops, size_t n) noexcept {
  uint32_t result = BL_DATA_ANALYSIS_CONFORMING;
  uint32_t wasSame = false;
  double prev = -1.0;

  for (size_t i = 0; i < n; i++) {
    double offset = stops[i].offset;
    if (!((offset >= 0.0) & (offset <= 1.0)))
      return BL_DATA_ANALYSIS_INVALID_VALUE;

    uint32_t isSame = (offset == prev);
    result |= (offset < prev);
    result |= isSame & wasSame;

    wasSame = isSame;
    prev = offset;
  }

  return result;
}

// ============================================================================
// [BLGradient - Matcher]
// ============================================================================

struct BLGradientStopMatcher {
  double offset;
  BL_INLINE BLGradientStopMatcher(double offset) noexcept : offset(offset) {}
};
static BL_INLINE bool operator==(const BLGradientStop& a, const BLGradientStopMatcher& b) noexcept { return a.offset == b.offset; }
static BL_INLINE bool operator<=(const BLGradientStop& a, const BLGradientStopMatcher& b) noexcept { return a.offset <= b.offset; }

// ============================================================================
// [BLGradient - AltStop]
// ============================================================================

// Alternative representation of `BLGradientStop` that is used to sort unknown
// stop array that is either unsorted or may contain more than 2 stops that have
// the same offset. The `index` member is actually an index to the original stop
// array.
struct BLGradientStopAlt {
  double offset;
  union {
    intptr_t index;
    uint64_t rgba;
  };
};

static_assert(sizeof(BLGradientStopAlt) == sizeof(BLGradientStop),
              "'BLGradientStopAlt' must have exactly the same as 'BLGradientStop'");

// ============================================================================
// [BLGradient - Utilities]
// ============================================================================

static BL_INLINE void blGradientCopyValues(double* dst, const double* src, size_t n) noexcept {
  size_t i;
  for (i = 0; i < n; i++)
    dst[i] = src[i];

  while (i < BL_GRADIENT_VALUE_COUNT)
    dst[i++] = 0.0;
}

static BL_INLINE void blGradientMoveStops(BLGradientStop* dst, const BLGradientStop* src, size_t n) noexcept {
  memmove(dst, src, n * sizeof(BLGradientStop));
}

static BL_INLINE size_t blGradientCopyStops(BLGradientStop* dst, const BLGradientStop* src, size_t n) noexcept {
  for (size_t i = 0; i < n; i++)
    dst[i] = src[i];
  return n;
}

static BL_NOINLINE size_t blGradientCopyUnsafeStops(BLGradientStop* dst, const BLGradientStop* src, size_t n, uint32_t analysis) noexcept {
  BL_ASSERT(analysis == BL_DATA_ANALYSIS_CONFORMING ||
            analysis == BL_DATA_ANALYSIS_NON_CONFORMING);

  if (analysis == BL_DATA_ANALYSIS_CONFORMING)
    return blGradientCopyStops(dst, src, n);

  size_t i;

  // First copy source stops into the destination and index them.
  BLGradientStopAlt* stops = reinterpret_cast<BLGradientStopAlt*>(dst);
  for (i = 0; i < n; i++) {
    stops[i].offset = src[i].offset;
    stops[i].index = intptr_t(i);
  }

  // Now sort the stops and use both `offset` and `index` as a comparator. After
  // the sort is done we will have preserved the order of all stops that have
  // the same `offset`.
  blQuickSort(stops, n, [](const BLGradientStopAlt& a, const BLGradientStopAlt& b) noexcept -> intptr_t {
    intptr_t result = 0;
    if (a.offset < b.offset) result = -1;
    if (a.offset > b.offset) result = 1;
    return result ? result : a.index - b.index;
  });

  // Now assign rgba value to the stop and remove all duplicates. If there are
  // 3 or more consecutive stops we remove all except the first/second to make
  // sharp transitions possible.
  size_t j = 0;
  double prev1 = -1.0; // Dummy, cannot be within [0..1] range.
  double prev2 = -1.0;

  for (i = 0; i < n - 1; i++) {
    double offset = stops[i].offset;
    BLRgba64 rgba = src[size_t(stops[i].index)].rgba;

    j -= size_t((prev1 == prev2) & (prev2 == offset));
    stops[j].offset = offset;
    stops[j].rgba = rgba.value;

    j++;
    prev1 = prev2;
    prev2 = offset;
  }

  // Returns the final number of stops kept. Could be the same as `n` or less.
  return j;
}

static BL_INLINE BLGradientLUT* blGradientCopyMaybeNullLUT(BLGradientLUT* lut) noexcept {
  return lut ? lut->incRef() : nullptr;
}

// Cache invalidation means to remove the cached lut tables from `impl`.
// Since modification always means to either create a copy of it or to modify
// a unique instance (not shared) it also means that we don't have to worry
// about atomic operations here.
static BL_INLINE BLResult blGradientInvalidateCache(BLInternalGradientImpl* impl) noexcept {
  BLGradientLUT* lut32 = impl->lut32;
  if (lut32) {
    impl->lut32 = nullptr;
    lut32->release();
  }

  impl->info32.packed = 0;
  return BL_SUCCESS;
}

BLGradientInfo blGradientImplEnsureInfo32(BLGradientImpl* impl_) noexcept {
  BLInternalGradientImpl* impl = blInternalCast(impl_);
  BLGradientInfo info;

  info.packed = impl->info32.packed;

  constexpr uint32_t FLAG_ALPHA_NOT_ONE  = 0x1; // Has alpha that is not 1.0.
  constexpr uint32_t FLAG_ALPHA_NOT_ZERO = 0x2; // Has alpha that is not 0.0.
  constexpr uint32_t FLAG_TRANSITION     = 0x4; // Has transition.

  if (info.packed == 0) {
    const BLGradientStop* stops = impl->stops;
    size_t stopCount = impl->size;

    if (stopCount != 0) {
      uint32_t flags = 0;
      uint64_t prev = stops[0].rgba.value & 0xFF00FF00FF00FF00u;
      uint32_t lutSize = 0;

      if (prev < 0xFF00000000000000u)
        flags |= FLAG_ALPHA_NOT_ONE;

      if (prev > 0x00FFFFFFFFFFFFFFu)
        flags |= FLAG_ALPHA_NOT_ZERO;

      for (size_t i = 1; i < stopCount; i++) {
        uint64_t value = stops[i].rgba.value & 0xFF00FF00FF00FF00u;
        if (value == prev)
          continue;

        flags |= FLAG_TRANSITION;
        if (value < 0xFF00000000000000u)
          flags |= FLAG_ALPHA_NOT_ONE;
        if (prev > 0x00FFFFFFFFFFFFFFu)
          flags |= FLAG_ALPHA_NOT_ZERO;
        prev = value;
      }

      // If all alpha values are zero then we consider this to be without transition,
      // because the whole transition would result in transparent black.
      if (!(flags & FLAG_ALPHA_NOT_ZERO))
        flags &= ~FLAG_TRANSITION;

      if (!(flags & FLAG_TRANSITION)) {
        // Minimal LUT size for no transition. The engine should always convert such
        // style into solid fill, so such LUT should never be used by the renderer.
        lutSize = 256;
      }
      else {
        // TODO: This is kinda adhoc, it would be much better if we base the calculation
        // on both stops and their offsets and estimate how big the ideal table should be.
        switch (stopCount) {
          case 1: {
            lutSize = 256;
            break;
          }

          case 2: {
            // 2 stops at endpoints only require 256 entries, more stops will use 512.
            double delta = stops[1].offset - stops[0].offset;
            lutSize = (delta >= 0.998) ? 256 : 512;
            break;
          }

          case 3: {
            lutSize = (stops[0].offset <= 0.002 && stops[1].offset == 0.5 && stops[2].offset >= 0.998) ? 512 : 1024;
            break;
          }

          default: {
            lutSize = 1024;
            break;
          }
        }
      }

      info.solid = uint8_t(flags & FLAG_TRANSITION ? 0 : 1);
      info.format = uint8_t(flags & FLAG_ALPHA_NOT_ONE) ? uint8_t(BL_FORMAT_PRGB32) : uint8_t(BL_FORMAT_FRGB32);
      info.lutSize = uint16_t(lutSize);

      // Update the info. It doesn't have to be atomic.
      impl->info32.packed = info.packed;
    }
  }


  return info;
}

BLGradientLUT* blGradientImplEnsureLut32(BLGradientImpl* impl_) noexcept {
  BLInternalGradientImpl* impl = blInternalCast(impl_);
  BLGradientLUT* lut = impl->lut32;

  if (lut)
    return lut;

  BLGradientInfo info = blGradientImplEnsureInfo32(impl);
  const BLGradientStop* stops = impl->stops;
  uint32_t lutSize = info.lutSize;

  if (!lutSize)
    return nullptr;

  lut = BLGradientLUT::alloc(lutSize, 4);
  if (BL_UNLIKELY(!lut))
    return nullptr;

  blGradientOps.interpolate32(lut->data<uint32_t>(), lutSize, stops, impl->size);

  // We must drop this LUT if another thread created it meanwhile.
  BLGradientLUT* expected = nullptr;
  if (!std::atomic_compare_exchange_strong((std::atomic<BLGradientLUT*>*)&impl->lut32, &expected, lut)) {
    BL_ASSERT(expected != nullptr);
    BLGradientLUT::destroy(lut);
    lut = expected;
  }

  return lut;
}

// ============================================================================
// [BLGradient - Internals]
// ============================================================================

static BLInternalGradientImpl* blGradientImplNew(size_t capacity, uint32_t type, const void* values, uint32_t extendMode, uint32_t mType, const BLMatrix2D* m) noexcept {
  BL_ASSERT(type < BL_GRADIENT_TYPE_COUNT);
  BL_ASSERT(mType < BL_MATRIX2D_TYPE_COUNT);
  BL_ASSERT(extendMode < BL_EXTEND_MODE_SIMPLE_COUNT);

  uint16_t memPoolData;
  BLInternalGradientImpl* impl = blRuntimeAllocImplT<BLInternalGradientImpl>(blGradientImplSizeOf(capacity), &memPoolData);

  if (BL_UNLIKELY(!impl))
    return impl;

  blImplInit(impl, BL_IMPL_TYPE_GRADIENT, 0, memPoolData);
  impl->stops = blOffsetPtr<BLGradientStop>(impl, sizeof(BLInternalGradientImpl));
  impl->size = 0;
  impl->capacity = capacity;
  impl->gradientType = uint8_t(type);
  impl->extendMode = uint8_t(extendMode);
  impl->matrixType = uint8_t(mType);
  impl->reserved[0] = 0;
  impl->matrix = *m;
  blGradientCopyValues(impl->values, static_cast<const double*>(values), blGradientValueCountTable[type]);
  impl->lut32 = nullptr;
  impl->info32.packed = 0;

  return impl;
}

// Cannot be static, called by `BLVariant` implementation.
BLResult blGradientImplDelete(BLGradientImpl* impl_) noexcept {
  BLInternalGradientImpl* impl = blInternalCast(impl_);
  blGradientInvalidateCache(impl);

  uint8_t* implBase = reinterpret_cast<uint8_t*>(impl);
  size_t implSize = blGradientImplSizeOf(impl->capacity);
  uint32_t implTraits = impl->implTraits;
  uint32_t memPoolData = impl->memPoolData;

  if (implTraits & BL_IMPL_TRAIT_EXTERNAL) {
    implSize = blGradientImplSizeOf() + sizeof(BLExternalImplPreface);
    implBase -= sizeof(BLExternalImplPreface);
    blImplDestroyExternal(impl);
  }

  if (implTraits & BL_IMPL_TRAIT_FOREIGN)
    return BL_SUCCESS;
  else
    return blRuntimeFreeImpl(implBase, implSize, memPoolData);
}

static BLResult BL_CDECL blGradientImplRelease(BLGradientImpl* impl) noexcept {
  if (blAtomicFetchDecRef(&impl->refCount) != 1)
    return BL_SUCCESS;

  return blGradientImplDelete(blInternalCast(impl));
}

static BL_NOINLINE BLResult blGradientDeepCopy(BLGradientCore* self, const BLInternalGradientImpl* impl, bool copyCache) noexcept {
  BLInternalGradientImpl* newI =
    blGradientImplNew(
      impl->capacity,
      impl->gradientType, impl->values, impl->extendMode,
      impl->matrixType, &impl->matrix);

  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  newI->size = blGradientCopyStops(newI->stops, impl->stops, impl->size);
  if (copyCache) {
    newI->lut32 = blGradientCopyMaybeNullLUT(impl->lut32);
    newI->info32.packed = impl->info32.packed;
  }

  BLInternalGradientImpl* oldI = blInternalCast(self->impl);
  self->impl = newI;
  return blGradientImplRelease(oldI);
}

static BL_INLINE BLResult blGradientMakeMutable(BLGradientCore* self, bool copyCache) noexcept {
  BLInternalGradientImpl* selfI = blInternalCast(self->impl);

  // NOTE: `copyCache` should be a constant so its handling should have zero cost.
  if (!blImplIsMutable(selfI))
    return blGradientDeepCopy(self, selfI, copyCache);

  if (!copyCache)
    return blGradientInvalidateCache(selfI);

  return BL_SUCCESS;
}

// ============================================================================
// [BLGradient - Init / Reset]
// ============================================================================

BLResult blGradientInit(BLGradientCore* self) noexcept {
  self->impl = &blNullGradientImpl;
  return BL_SUCCESS;
}

BLResult blGradientInitAs(BLGradientCore* self, uint32_t type, const void* values, uint32_t extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D* m) noexcept {
  self->impl = &blNullGradientImpl;
  if (BL_UNLIKELY((type >= BL_GRADIENT_TYPE_COUNT) | (extendMode >= BL_EXTEND_MODE_SIMPLE_COUNT)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (!values)
    values = blGradientNoValues;

  uint32_t mType = BL_MATRIX2D_TYPE_IDENTITY;
  if (!m)
    m = &blGradientNoMatrix;
  else
    mType = m->type();

  uint32_t analysis = BL_DATA_ANALYSIS_CONFORMING;
  if (n) {
    if (BL_UNLIKELY(stops == nullptr))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    analysis = blGradientAnalyzeStopArray(stops, n);
    if (BL_UNLIKELY(analysis >= BL_DATA_ANALYSIS_INVALID_VALUE))
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  size_t newCapacity = blGradientFittingCapacity(blMax(n, blGradientInitialCapacity()));
  BLInternalGradientImpl* impl = blGradientImplNew(newCapacity, type, values, extendMode, mType, m);

  if (BL_UNLIKELY(!impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  impl->size = blGradientCopyUnsafeStops(impl->stops, stops, n, analysis);
  self->impl = impl;

  return BL_SUCCESS;
}

BLResult blGradientReset(BLGradientCore* self) noexcept {
  BLInternalGradientImpl* selfI = blInternalCast(self->impl);
  self->impl = &blNullGradientImpl;
  return blGradientImplRelease(selfI);
}

// ============================================================================
// [BLGradient - Assign]
// ============================================================================

BLResult blGradientAssignMove(BLGradientCore* self, BLGradientCore* other) noexcept {
  BLInternalGradientImpl* selfI = blInternalCast(self->impl);
  BLInternalGradientImpl* otherI = blInternalCast(other->impl);

  self->impl = otherI;
  other->impl = &blNullGradientImpl;

  return blGradientImplRelease(selfI);
}

BLResult blGradientAssignWeak(BLGradientCore* self, const BLGradientCore* other) noexcept {
  BLInternalGradientImpl* selfI = blInternalCast(self->impl);
  BLInternalGradientImpl* otherI = blInternalCast(other->impl);

  self->impl = blImplIncRef(otherI);
  return blGradientImplRelease(selfI);
}

BLResult blGradientCreate(BLGradientCore* self, uint32_t type, const void* values, uint32_t extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D* m) noexcept {
  if (BL_UNLIKELY((type >= BL_GRADIENT_TYPE_COUNT) | (extendMode >= BL_EXTEND_MODE_SIMPLE_COUNT)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (!values)
    values = blGradientNoValues;

  uint32_t mType = BL_MATRIX2D_TYPE_IDENTITY;
  if (!m)
    m = &blGradientNoMatrix;
  else
    mType = m->type();

  uint32_t analysis = BL_DATA_ANALYSIS_CONFORMING;
  if (n) {
    if (BL_UNLIKELY(stops == nullptr))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    analysis = blGradientAnalyzeStopArray(stops, n);
    if (BL_UNLIKELY(analysis >= BL_DATA_ANALYSIS_INVALID_VALUE))
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  BLInternalGradientImpl* impl = blInternalCast(self->impl);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(impl));

  if ((n | immutableMsk) > impl->capacity) {
    size_t newCapacity = blGradientFittingCapacity(blMax(n, blGradientInitialCapacity()));
    BLInternalGradientImpl* newI = blGradientImplNew(newCapacity, type, values, extendMode, mType, m);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    newI->size = blGradientCopyUnsafeStops(newI->stops, stops, n, analysis);
    self->impl = newI;

    return blGradientImplRelease(impl);
  }
  else {
    impl->gradientType = uint8_t(type);
    impl->extendMode = uint8_t(extendMode);
    impl->matrixType = uint8_t(mType);
    impl->matrix.reset(*m);

    blGradientCopyValues(impl->values, static_cast<const double*>(values), blGradientValueCountTable[type]);
    impl->size = blGradientCopyUnsafeStops(impl->stops, stops, n, analysis);

    return blGradientInvalidateCache(impl);
  }
}

// ============================================================================
// [BLGradient - Storage]
// ============================================================================

BLResult blGradientShrink(BLGradientCore* self) noexcept {
  BLInternalGradientImpl* selfI = blInternalCast(self->impl);
  size_t size = selfI->size;
  size_t fittingCapacity = blGradientFittingCapacity(size);

  if (fittingCapacity >= selfI->capacity)
    return BL_SUCCESS;

  BLInternalGradientImpl* newI =
    blGradientImplNew(
      fittingCapacity,
      selfI->gradientType, selfI->values, selfI->extendMode,
      selfI->matrixType, &selfI->matrix);

  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  newI->size = blGradientCopyStops(newI->stops, selfI->stops, selfI->size);
  newI->lut32 = blGradientCopyMaybeNullLUT(selfI->lut32);
  self->impl = newI;

  return blGradientImplRelease(selfI);
}

BLResult blGradientReserve(BLGradientCore* self, size_t n) noexcept {
  BLInternalGradientImpl* selfI = blInternalCast(self->impl);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  if ((n | immutableMsk) > selfI->capacity) {
    size_t newCapacity = blGradientFittingCapacity(blMax(n, selfI->size));
    BLInternalGradientImpl* newI =
      blGradientImplNew(
        newCapacity,
        selfI->gradientType, selfI->values, selfI->extendMode,
        selfI->matrixType, &selfI->matrix);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    newI->size = blGradientCopyStops(newI->stops, selfI->stops, selfI->size);
    newI->lut32 = blGradientCopyMaybeNullLUT(selfI->lut32);
    self->impl = newI;

    return blGradientImplRelease(selfI);
  }
  else {
    return BL_SUCCESS;
  }
}

// ============================================================================
// [BLGradient - Properties]
// ============================================================================

uint32_t blGradientGetType(const BLGradientCore* self) noexcept {
  return self->impl->gradientType;
}

BLResult blGradientSetType(BLGradientCore* self, uint32_t type) noexcept {
  if (BL_UNLIKELY(type >= BL_GRADIENT_TYPE_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BL_PROPAGATE(blGradientMakeMutable(self, true));
  BLInternalGradientImpl* selfI = blInternalCast(self->impl);

  selfI->gradientType = uint8_t(type);
  return BL_SUCCESS;
}

double blGradientGetValue(const BLGradientCore* self, size_t index) noexcept {
  if (BL_UNLIKELY(index >= BL_GRADIENT_VALUE_COUNT))
    return blNaN<double>();
  else
    return self->impl->values[index];
}

BLResult blGradientSetValue(BLGradientCore* self, size_t index, double value) noexcept {
  if (BL_UNLIKELY(index >= BL_GRADIENT_VALUE_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BL_PROPAGATE(blGradientMakeMutable(self, true));
  BLInternalGradientImpl* selfI = blInternalCast(self->impl);

  selfI->values[index] = value;
  return BL_SUCCESS;
}

BLResult blGradientSetValues(BLGradientCore* self, size_t index, const double* values, size_t valueCount) noexcept {
  if (BL_UNLIKELY(index >= BL_GRADIENT_VALUE_COUNT || valueCount > BL_GRADIENT_VALUE_COUNT - index))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (BL_UNLIKELY(!valueCount))
    return BL_SUCCESS;

  BL_PROPAGATE(blGradientMakeMutable(self, true));
  BLInternalGradientImpl* selfI = blInternalCast(self->impl);

  double* dst = selfI->values + index;
  for (size_t i = 0; i < valueCount; i++)
    dst[i] = values[i];

  return BL_SUCCESS;
}

uint32_t blGradientGetExtendMode(BLGradientCore* self) noexcept {
  return self->impl->extendMode;
}

BLResult blGradientSetExtendMode(BLGradientCore* self, uint32_t extendMode) noexcept {
  if (BL_UNLIKELY(extendMode >= BL_EXTEND_MODE_SIMPLE_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BL_PROPAGATE(blGradientMakeMutable(self, true));
  BLInternalGradientImpl* selfI = blInternalCast(self->impl);

  selfI->extendMode = uint8_t(extendMode);
  return BL_SUCCESS;
}

// ============================================================================
// [BLGradient - Stops]
// ============================================================================

const BLGradientStop* blGradientGetStops(const BLGradientCore* self) noexcept {
  return self->impl->stops;
}

size_t blGradientGetSize(const BLGradientCore* self) noexcept {
  return self->impl->size;
}

size_t blGradientGetCapacity(const BLGradientCore* self) noexcept {
  return self->impl->capacity;
}

BLResult blGradientResetStops(BLGradientCore* self) noexcept {
  BLInternalGradientImpl* selfI = blInternalCast(self->impl);
  size_t size = selfI->size;

  if (!size)
    return BL_SUCCESS;

  if (!blImplIsMutable(selfI)) {
    BLInternalGradientImpl* newI =
      blGradientImplNew(
        blGradientFittingCapacity(4),
        selfI->gradientType, selfI->values, selfI->extendMode,
        selfI->matrixType, &selfI->matrix);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    self->impl = newI;
    return blGradientImplRelease(selfI);
  }
  else {
    selfI->size = 0;
    return blGradientInvalidateCache(selfI);
  }
}

BLResult blGradientAssignStops(BLGradientCore* self, const BLGradientStop* stops, size_t n) noexcept {
  if (n == 0)
    return blGradientResetStops(self);

  BLInternalGradientImpl* selfI = blInternalCast(self->impl);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));
  uint32_t analysis = blGradientAnalyzeStopArray(stops, n);

  if (BL_UNLIKELY(analysis >= BL_DATA_ANALYSIS_INVALID_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if ((n | immutableMsk) > selfI->capacity) {
    size_t newCapacity = blGradientFittingCapacity(n);
    BLInternalGradientImpl* newI = blGradientImplNew(
      newCapacity,
      selfI->gradientType, selfI->values, selfI->extendMode,
      selfI->matrixType, &selfI->matrix);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    newI->size = blGradientCopyUnsafeStops(newI->stops, stops, n, analysis);
    self->impl = newI;

    return blGradientImplRelease(selfI);
  }
  else {
    selfI->size = blGradientCopyUnsafeStops(selfI->stops, stops, n, analysis);
    return blGradientInvalidateCache(selfI);
  }
}

BLResult blGradientAddStopRgba32(BLGradientCore* self, double offset, uint32_t rgba32) noexcept {
  return blGradientAddStopRgba64(self, offset, blRgba64FromRgba32(rgba32));
}

BLResult blGradientAddStopRgba64(BLGradientCore* self, double offset, uint64_t rgba64) noexcept {
  if (BL_UNLIKELY(!(offset >= 0.0 && offset <= 1.0)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLInternalGradientImpl* selfI = blInternalCast(self->impl);
  BLGradientStop* stops = selfI->stops;

  size_t i = 0;
  size_t n = selfI->size;

  if (n && offset >= stops[0].offset) {
    i = blBinarySearchClosestLast(stops, n, BLGradientStopMatcher(offset));

    // If there are two stops that have the same offset then we would replace
    // the second one. This is supported and it would make a sharp transition.
    if (i > 0 && stops[i - 1].offset == offset)
      return blGradientReplaceStopRgba64(self, i, offset, rgba64);

    // Insert a new stop after `i`.
    i++;
  }

  // If we are here it means that we are going to insert a stop at `i`. All
  // other cases were handled at this point so focus on generic insert, which
  // could be just a special case of append operation, but we don't really care.
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  if ((n | immutableMsk) >= selfI->capacity) {
    size_t newCapacity = blGradientGrowingCapacity(n + 1);
    BLInternalGradientImpl* newI =
      blGradientImplNew(
        newCapacity,
        selfI->gradientType, selfI->values, selfI->extendMode,
        selfI->matrixType, &selfI->matrix);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    BLGradientStop* newStops = newI->stops;

    blGradientCopyStops(newStops, stops, i);
    newStops[i].reset(offset, BLRgba64(rgba64));
    blGradientCopyStops(newStops + i + 1, stops + i, n - i);

    newI->size = n + 1;
    self->impl = newI;

    return blGradientImplRelease(selfI);
  }
  else {
    blGradientMoveStops(stops + i + 1, stops + i, n - i);
    stops[i].reset(offset, BLRgba64(rgba64));

    selfI->size = n + 1;
    return blGradientInvalidateCache(selfI);
  }
}

BLResult blGradientRemoveStop(BLGradientCore* self, size_t index) noexcept {
  BLRange range(index, index + 1);
  return blGradientRemoveStops(self, &range);
}

BLResult blGradientRemoveStopByOffset(BLGradientCore* self, double offset, uint32_t all) noexcept {
  if (BL_UNLIKELY(!(offset >= 0.0 && offset <= 1.0)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLInternalGradientImpl* selfI = blInternalCast(self->impl);
  const BLGradientStop* stops = selfI->stops;
  size_t size = selfI->size;

  for (size_t a = 0; a < size; a++) {
    if (stops[a].offset > offset)
      break;

    if (stops[a].offset == offset) {
      size_t b = a + 1;

      if (all) {
        while (b < size) {
          if (stops[b].offset != offset)
            break;
          b++;
        }
      }

      BLRange range(a, b);
      return blGradientRemoveStops(self, &range);
    }
  }

  return BL_SUCCESS;
}

BLResult blGradientRemoveStops(BLGradientCore* self, const BLRange* range) noexcept {
  BLInternalGradientImpl* selfI = blInternalCast(self->impl);
  size_t size = selfI->size;

  size_t index = range->start;
  size_t end = blMin(range->end, size);

  if (BL_UNLIKELY(index > size || end < index))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (BL_UNLIKELY(index == end))
    return BL_SUCCESS;

  BLGradientStop* stops = selfI->stops;
  size_t removedCount = end - index;
  size_t shiftedCount = size - end;
  size_t afterCount = size - removedCount;

  if (!blImplIsMutable(selfI)) {
    BLInternalGradientImpl* newI =
      blGradientImplNew(
        blGradientFittingCapacity(afterCount),
        selfI->gradientType, selfI->values, selfI->extendMode,
        selfI->matrixType, &selfI->matrix);

    BLGradientStop* newStops = newI->stops;
    blGradientCopyStops(newStops, stops, index);
    blGradientCopyStops(newStops + index, stops + end, shiftedCount);

    self->impl = newI;
    return blGradientImplRelease(selfI);
  }
  else {
    blGradientMoveStops(stops + index, stops + end, shiftedCount);
    selfI->size = afterCount;
    return blGradientInvalidateCache(selfI);
  }
}

BLResult blGradientRemoveStopsFromTo(BLGradientCore* self, double offsetMin, double offsetMax) noexcept {
  if (BL_UNLIKELY(offsetMax < offsetMin))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLInternalGradientImpl* selfI = blInternalCast(self->impl);
  const BLGradientStop* stops = selfI->stops;
  size_t size = selfI->size;

  if (!size)
    return BL_SUCCESS;

  size_t a, b;
  for (a = 0; a < size && stops[a].offset <  offsetMin; a++) continue;
  for (b = a; b < size && stops[b].offset <= offsetMax; b++) continue;

  if (a >= b)
    return BL_SUCCESS;

  BLRange range(a, b);
  return blGradientRemoveStops(self, &range);
}

BLResult blGradientReplaceStopRgba32(BLGradientCore* self, size_t index, double offset, uint32_t rgba32) noexcept {
  return blGradientReplaceStopRgba64(self, index, offset, blRgba64FromRgba32(rgba32));
}

BLResult blGradientReplaceStopRgba64(BLGradientCore* self, size_t index, double offset, uint64_t rgba64) noexcept {
  BLInternalGradientImpl* selfI = blInternalCast(self->impl);
  size_t size = selfI->size;

  if (BL_UNLIKELY(index >= size))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BL_PROPAGATE(blGradientMakeMutable(self, false));
  selfI = blInternalCast(self->impl);

  BLGradientStop* stops = selfI->stops;
  if (stops[index].offset == offset) {
    stops[index].rgba.value = rgba64;
    return BL_SUCCESS;
  }
  else {
    // Since we made this gradient mutable this cannot fail.
    BLResult result = blGradientRemoveStop(self, index);
    BL_ASSERT(result == BL_SUCCESS);

    return blGradientAddStopRgba64(self, offset, rgba64);
  }
}

size_t blGradientIndexOfStop(const BLGradientCore* self, double offset) noexcept {
  const BLInternalGradientImpl* selfI = blInternalCast(self->impl);
  const BLGradientStop* stops = selfI->stops;

  size_t n = selfI->size;
  if (!n)
    return SIZE_MAX;

  size_t i = blBinarySearch(stops, n, BLGradientStopMatcher(offset));
  if (i == SIZE_MAX)
    return SIZE_MAX;

  if (i > 0 && stops[i - 1].offset == offset)
    i--;
  return i;
}

// ============================================================================
// [BLGradient - Matrix]
// ============================================================================

BLResult blGradientApplyMatrixOp(BLGradientCore* self, uint32_t opType, const void* opData) noexcept {
  if (BL_UNLIKELY(opType >= BL_MATRIX2D_OP_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLInternalGradientImpl* selfI = blInternalCast(self->impl);
  if (opType == 0 && selfI->matrixType == BL_MATRIX2D_TYPE_IDENTITY)
    return BL_SUCCESS;

  BL_PROPAGATE(blGradientMakeMutable(self, true));
  selfI = blInternalCast(self->impl);

  blMatrix2DApplyOp(&selfI->matrix, opType, opData);
  selfI->matrixType = uint8_t(selfI->matrix.type());

  return BL_SUCCESS;
}

// ============================================================================
// [BLGradient - Equals]
// ============================================================================

bool blGradientEquals(const BLGradientCore* a, const BLGradientCore* b) noexcept {
  const BLGradientImpl* aI = a->impl;
  const BLGradientImpl* bI = b->impl;

  if (aI == bI)
    return true;

  size_t size = aI->size;
  bool eq = ((aI->gradientType != bI->gradientType) |
             (aI->extendMode   != bI->extendMode  ) |
             (aI->matrixType   != bI->matrixType  ) |
             (aI->matrix       != bI->matrix      ) |
             (size             != bI->size        ));
  return eq && memcmp(aI->stops, bI->stops, size * sizeof(BLGradientStop)) == 0;
}

// ============================================================================
// [BLGradient - Interpolate32]
// ============================================================================

static void BL_CDECL blGradientInterpolate32(uint32_t* dPtr, uint32_t dSize, const BLGradientStop* sPtr, size_t sSize) noexcept {
  BL_ASSERT(dPtr != nullptr);
  BL_ASSERT(dSize > 0);

  BL_ASSERT(sPtr != nullptr);
  BL_ASSERT(sSize > 0);

  uint32_t* dSpanPtr = dPtr;
  uint32_t i = dSize;

  uint32_t c0 = blRgba32FromRgba64(sPtr[0].rgba.value);
  uint32_t c1 = c0;

  uint32_t p0 = 0;
  uint32_t p1;

  size_t sIndex = 0;
  double fWidth = double(int32_t(--dSize) << 8);

  uint32_t cp = bl_prgb32_8888_from_argb32_8888(c0);
  uint32_t cpFirst = cp;

  if (sSize == 1)
    goto SolidLoop;

  do {
    c1 = blRgba32FromRgba64(sPtr[sIndex].rgba.value);
    p1 = uint32_t(blRoundToInt(sPtr[sIndex].offset * fWidth));

    dSpanPtr = dPtr + (p0 >> 8);
    i = ((p1 >> 8) - (p0 >> 8));

    if (i == 0)
      c0 = c1;

    p0 = p1;
    i++;

SolidInit:
    cp = bl_prgb32_8888_from_argb32_8888(c0);
    if (c0 == c1) {
SolidLoop:
      do {
        dSpanPtr[0] = cp;
        dSpanPtr++;
      } while (--i);
    }
    else {
      dSpanPtr[0] = cp;
      dSpanPtr++;

      if (--i) {
        const uint32_t kShift = 23;
        const uint32_t kMask = 0xFFu << kShift;

        uint32_t rPos = (c0 <<  7) & kMask;
        uint32_t gPos = (c0 << 15) & kMask;
        uint32_t bPos = (c0 << 23) & kMask;

        uint32_t rInc = (c1 <<  7) & kMask;
        uint32_t gInc = (c1 << 15) & kMask;
        uint32_t bInc = (c1 << 23) & kMask;

        rInc = uint32_t(int32_t(rInc - rPos) / int32_t(i));
        gInc = uint32_t(int32_t(gInc - gPos) / int32_t(i));
        bInc = uint32_t(int32_t(bInc - bPos) / int32_t(i));

        rPos += 1u << (kShift - 1);
        gPos += 1u << (kShift - 1);
        bPos += 1u << (kShift - 1);

        if (blRgba32IsFullyOpaque(c0 & c1)) {
          // Both fully opaque, no need to premultiply.
          do {
            rPos += rInc;
            gPos += gInc;
            bPos += bInc;

            cp = 0xFF000000u + ((rPos & kMask) >>  7) +
                               ((gPos & kMask) >> 15) +
                               ((bPos & kMask) >> 23) ;

            dSpanPtr[0] = cp;
            dSpanPtr++;
          } while (--i);
        }
        else {
          // One or both having alpha, have to be premultiplied.
          uint32_t aPos = (c0 >> 1) & kMask;
          uint32_t aInc = (c1 >> 1) & kMask;

          aInc = uint32_t(int32_t(aInc - aPos) / int32_t(i));
          aPos += 1u << (kShift - 1);

          do {
            uint32_t _a, _g;

            aPos += aInc;
            rPos += rInc;
            gPos += gInc;
            bPos += bInc;

            cp = ((bPos & kMask) >> 23) +
                 ((rPos & kMask) >>  7);
            _a = ((aPos & kMask) >> 23);
            _g = ((gPos & kMask) >> 15);

            cp *= _a;
            _g *= _a;
            _a <<= 24;

            cp += 0x00800080u;
            _g += 0x00008000u;

            cp = (cp + ((cp >> 8) & 0x00FF00FFu));
            _g = (_g + ((_g >> 8) & 0x0000FF00u));

            cp &= 0xFF00FF00u;
            _g &= 0x00FF0000u;

            cp += _g;
            cp >>= 8;
            cp += _a;

            dSpanPtr[0] = cp;
            dSpanPtr++;
          } while (--i);
        }
      }

      c0 = c1;
    }
  } while (++sIndex < sSize);

  // The last stop doesn't have to end at 1.0, in such case the remaining space
  // is filled by the last color stop (premultiplied). We jump to the main loop
  // instead of filling the buffer here.
  i = uint32_t((size_t)((dPtr + dSize + 1) - dSpanPtr));
  if (i != 0)
    goto SolidInit;

  // The first pixel has to be always set to the first stop's color. The main
  // loop always honors the last color value of the stop colliding with the
  // previous offset index - for example if multiple stops have the same offset
  // [0.0] the first pixel will be the last stop's color. This is easier to fix
  // here as we don't need extra conditions in the main loop.
  dPtr[0] = cpFirst;
}

// ============================================================================
// [BLGradient - Runtime Init]
// ============================================================================

void blGradientRtInit(BLRuntimeContext* rt) noexcept {
  // Initialize gradient ops.
  blGradientOps.interpolate32 = blGradientInterpolate32;

  #ifdef BL_BUILD_OPT_SSE2
  if (blRuntimeHasSSE2(rt)) {
    blGradientOps.interpolate32 = blGradientInterpolate32_SSE2;
  }
  #endif

  #ifdef BL_BUILD_OPT_AVX2
  if (blRuntimeHasAVX2(rt)) {
    blGradientOps.interpolate32 = blGradientInterpolate32_AVX2;
  }
  #endif

  // Initialize gradient built-in instance.
  BLInternalGradientImpl* gradientI = &blNullGradientImpl;
  gradientI->implType = uint8_t(BL_IMPL_TYPE_GRADIENT);
  gradientI->implTraits = uint8_t(BL_IMPL_TRAIT_NULL);
  blAssignBuiltInNull(gradientI);
}

// ============================================================================
// [BLGradient - Unit Tests]
// ============================================================================

#if defined(BL_BUILD_TEST)
UNIT(blend2d_gradient) {
  INFO("BLGradient - Linear values");
  {
    BLGradient g(BLLinearGradientValues(0.0, 0.5, 1.0, 1.5));

    EXPECT(g.type() == BL_GRADIENT_TYPE_LINEAR);
    EXPECT(g.x0() == 0.0);
    EXPECT(g.y0() == 0.5);
    EXPECT(g.x1() == 1.0);
    EXPECT(g.y1() == 1.5);

    g.setX0(0.15);
    g.setY0(0.85);
    g.setX1(0.75);
    g.setY1(0.25);

    EXPECT(g.x0() == 0.15);
    EXPECT(g.y0() == 0.85);
    EXPECT(g.x1() == 0.75);
    EXPECT(g.y1() == 0.25);
  }

  INFO("BLGradient - Radial values");
  {
    BLGradient g(BLRadialGradientValues(1.0, 1.5, 0.0, 0.5, 500.0));

    EXPECT(g.type() == BL_GRADIENT_TYPE_RADIAL);
    EXPECT(g.x0() == 1.0);
    EXPECT(g.y0() == 1.5);
    EXPECT(g.x1() == 0.0);
    EXPECT(g.y1() == 0.5);
    EXPECT(g.r0() == 500.0);

    g.setR0(150.0);
    EXPECT(g.r0() == 150.0);
  }

  INFO("BLGradient - Conical values");
  {
    BLGradient g(BLConicalGradientValues(1.0, 1.5, 0.1));

    EXPECT(g.type() == BL_GRADIENT_TYPE_CONICAL);
    EXPECT(g.x0() == 1.0);
    EXPECT(g.y0() == 1.5);
    EXPECT(g.angle() == 0.1);
  }

  INFO("BLGradient - Stops");
  {
    BLGradient g;

    g.addStop(0.0, BLRgba32(0x00000000u));
    EXPECT(g.size() == 1);
    EXPECT(g.stopAt(0).rgba.value == 0x0000000000000000u);

    g.addStop(1.0, BLRgba32(0xFF000000u));
    EXPECT(g.size() == 2);
    EXPECT(g.stopAt(1).rgba.value == 0xFFFF000000000000u);

    g.addStop(0.5, BLRgba32(0xFFFF0000u));
    EXPECT(g.size() == 3);
    EXPECT(g.stopAt(1).rgba.value == 0xFFFFFFFF00000000u);

    g.addStop(0.5, BLRgba32(0xFFFFFF00u));
    EXPECT(g.size() == 4);
    EXPECT(g.stopAt(2).rgba.value == 0xFFFFFFFFFFFF0000u);

    g.removeStopByOffset(0.5, true);
    EXPECT(g.size() == 2);
    EXPECT(g.stopAt(0).rgba.value == 0x0000000000000000u);
    EXPECT(g.stopAt(1).rgba.value == 0xFFFF000000000000u);

    g.addStop(0.5, BLRgba32(0x80000000u));
    EXPECT(g.size() == 3);
    EXPECT(g.stopAt(1).rgba.value == 0x8080000000000000u);

    // Check whether copy-on-write works as expected.
    BLGradient copy(g);
    EXPECT(copy.size() == 3);

    g.addStop(0.5, BLRgba32(0xCC000000u));
    EXPECT(copy.size() == 3);
    EXPECT(g.size() == 4);
    EXPECT(g.stopAt(0).rgba.value == 0x0000000000000000u);
    EXPECT(g.stopAt(1).rgba.value == 0x8080000000000000u);
    EXPECT(g.stopAt(2).rgba.value == 0xCCCC000000000000u);
    EXPECT(g.stopAt(3).rgba.value == 0xFFFF000000000000u);

    g.resetStops();
    EXPECT(g.size() == 0);
  }
}
#endif
