// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "math_p.h"
#include "format_p.h"
#include "gradient_p.h"
#include "rgba_p.h"
#include "runtime_p.h"
#include "tables_p.h"
#include "pixelops/funcs_p.h"
#include "support/algorithm_p.h"
#include "support/intops_p.h"
#include "support/ptrops_p.h"
#include "threading/atomic_p.h"

namespace BLGradientPrivate {

// BLGradient - Globals
// ====================

static BLObjectEthernalImpl<BLGradientPrivateImpl> defaultImpl;

static constexpr const double noValues[BL_GRADIENT_VALUE_MAX_VALUE + 1] = { 0.0 };
static constexpr const BLMatrix2D noMatrix(1.0, 0.0, 0.0, 1.0, 0.0, 0.0);

// BLGradient - Tables
// ===================

struct BLGradientValueCountTableGen {
  static constexpr uint8_t value(size_t i) noexcept {
    return i == BL_GRADIENT_TYPE_LINEAR  ? uint8_t(sizeof(BLLinearGradientValues ) / sizeof(double)) :
           i == BL_GRADIENT_TYPE_RADIAL  ? uint8_t(sizeof(BLRadialGradientValues ) / sizeof(double)) :
           i == BL_GRADIENT_TYPE_CONICAL ? uint8_t(sizeof(BLConicalGradientValues) / sizeof(double)) : uint8_t(0);
  }
};

static constexpr const auto valueCountTable =
  blMakeLookupTable<uint8_t, BL_GRADIENT_TYPE_MAX_VALUE + 1, BLGradientValueCountTableGen>();

// BLGradient - Internals & Utilities
// ==================================

static BL_INLINE constexpr BLObjectImplSize implSizeFromCapacity(size_t n) noexcept {
  return BLObjectImplSize(sizeof(BLGradientPrivateImpl) + n * sizeof(BLGradientStop));
}

static BL_INLINE constexpr size_t capacityFromImplSize(BLObjectImplSize implSize) noexcept {
  return (implSize.value() - sizeof(BLGradientPrivateImpl)) / sizeof(BLGradientStop);
}

static BL_INLINE bool isMutable(const BLGradientCore* self) noexcept {
  const size_t* refCountPtr = blObjectImplGetRefCountPtr(self->_d.impl);
  return *refCountPtr == 1;
}

static BL_INLINE size_t getSize(const BLGradientCore* self) noexcept { return getImpl(self)->size; }
static BL_INLINE size_t getCapacity(const BLGradientCore* self) noexcept { return getImpl(self)->capacity; }
static BL_INLINE BLGradientStop* getStops(const BLGradientCore* self) noexcept { return getImpl(self)->stops; }

static constexpr size_t BL_GRADIENT_IMPL_INITIAL_SIZE = BLIntOps::alignUp(implSizeFromCapacity(2).value(), BL_OBJECT_IMPL_ALIGNMENT);

// BLGradient - Analysis
// =====================

static BL_INLINE uint32_t analyzeStopArray(const BLGradientStop* stops, size_t n) noexcept {
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

// BLGradient - Stop Matcher
// =========================

struct GradientStopMatcher {
  double offset;
  BL_INLINE GradientStopMatcher(double offset) noexcept : offset(offset) {}
};
static BL_INLINE bool operator==(const BLGradientStop& a, const GradientStopMatcher& b) noexcept { return a.offset == b.offset; }
static BL_INLINE bool operator<=(const BLGradientStop& a, const GradientStopMatcher& b) noexcept { return a.offset <= b.offset; }

// BLGradient - AltStop
// ====================

// Alternative representation of `BLGradientStop` that is used to sort unknown stop array that is either unsorted or
// may contain more than 2 stops that have the same offset. The `index` member is actually an index to the original
// stop array.
struct GradientStopAlt {
  double offset;
  union {
    intptr_t index;
    uint64_t rgba;
  };
};

BL_STATIC_ASSERT(sizeof(GradientStopAlt) == sizeof(BLGradientStop));

// BLGradient - Utilities
// ======================

static BL_INLINE void initValues(double* dst, const double* src, size_t n) noexcept {
  size_t i;

  BL_NOUNROLL
  for (i = 0; i < n; i++)
    dst[i] = src[i];

  BL_NOUNROLL
  while (i <= BL_GRADIENT_VALUE_MAX_VALUE)
    dst[i++] = 0.0;
}

static BL_INLINE void moveStops(BLGradientStop* dst, const BLGradientStop* src, size_t n) noexcept {
  memmove(dst, src, n * sizeof(BLGradientStop));
}

static BL_INLINE size_t copyStops(BLGradientStop* dst, const BLGradientStop* src, size_t n) noexcept {
  for (size_t i = 0; i < n; i++)
    dst[i] = src[i];
  return n;
}

static BL_NOINLINE size_t copyUnsafeStops(BLGradientStop* dst, const BLGradientStop* src, size_t n, uint32_t analysis) noexcept {
  BL_ASSERT(analysis == BL_DATA_ANALYSIS_CONFORMING ||
            analysis == BL_DATA_ANALYSIS_NON_CONFORMING);

  if (analysis == BL_DATA_ANALYSIS_CONFORMING)
    return copyStops(dst, src, n);

  size_t i;

  // First copy source stops into the destination and index them.
  GradientStopAlt* stops = reinterpret_cast<GradientStopAlt*>(dst);
  for (i = 0; i < n; i++) {
    stops[i].offset = src[i].offset;
    stops[i].index = intptr_t(i);
  }

  // Now sort the stops and use both `offset` and `index` as a comparator. After the sort is done we will have
  // preserved the order of all stops that have the same `offset`.
  BLAlgorithm::quickSort(stops, n, [](const GradientStopAlt& a, const GradientStopAlt& b) noexcept -> intptr_t {
    intptr_t result = 0;
    if (a.offset < b.offset) result = -1;
    if (a.offset > b.offset) result = 1;
    return result ? result : a.index - b.index;
  });

  // Now assign rgba value to the stop and remove all duplicates. If there are 3 or more consecutive stops we
  // remove all except the first/second to make sharp transitions possible.
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

static BL_INLINE BLGradientLUT* copyMaybeNullLUT(BLGradientLUT* lut) noexcept {
  return lut ? lut->incRef() : nullptr;
}

// Cache invalidation means to remove the cached lut tables from `impl`. Since modification always means to either
// create a copy of it or to modify a unique instance (not shared) it also means that we don't have to worry about
// atomic operations here.
static BL_INLINE BLResult invalidateLUTCache(BLGradientPrivateImpl* impl) noexcept {
  BLGradientLUT* lut32 = impl->lut32;
  if (lut32) {
    impl->lut32 = nullptr;
    lut32->release();
  }

  impl->info32.packed = 0;
  return BL_SUCCESS;
}

BLGradientInfo ensureInfo32(BLGradientPrivateImpl* impl) noexcept {
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
        if (value > 0x00FFFFFFFFFFFFFFu)
          flags |= FLAG_ALPHA_NOT_ZERO;
        prev = value;
      }

      // If all alpha values are zero then we consider this to be without transition, because the whole transition
      // would result in transparent black.
      if (!(flags & FLAG_ALPHA_NOT_ZERO))
        flags &= ~FLAG_TRANSITION;

      if (!(flags & FLAG_TRANSITION)) {
        // Minimal LUT size for no transition. The engine should always convert such style into solid fill, so such
        // LUT should never be used by the renderer.
        lutSize = 256;
      }
      else {
        // TODO: This is kinda adhoc, it would be much better if we base the calculation on both stops and their
        // offsets and estimate how big the ideal table should be.
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
      info.format = uint8_t(flags & FLAG_ALPHA_NOT_ONE ? BLInternalFormat::kPRGB32 : BLInternalFormat::kFRGB32);
      info.lutSize = uint16_t(lutSize);

      // Update the info. It doesn't have to be atomic.
      impl->info32.packed = info.packed;
    }
  }


  return info;
}

BLGradientLUT* ensureLut32(BLGradientPrivateImpl* impl) noexcept {
  BLGradientLUT* lut = impl->lut32;
  if (lut)
    return lut;

  BLGradientInfo info = ensureInfo32(impl);
  const BLGradientStop* stops = impl->stops;
  uint32_t lutSize = info.lutSize;

  if (!lutSize)
    return nullptr;

  lut = BLGradientLUT::alloc(lutSize, 4);
  if (BL_UNLIKELY(!lut))
    return nullptr;

  BLPixelOps::funcs.interpolate_prgb32(lut->data<uint32_t>(), lutSize, stops, impl->size);

  // We must drop this LUT if another thread created it meanwhile.
  BLGradientLUT* expected = nullptr;
  if (!blAtomicCompareExchange(&impl->lut32, &expected, lut)) {
    BL_ASSERT(expected != nullptr);
    BLGradientLUT::destroy(lut);
    lut = expected;
  }

  return lut;
}

// BLGradient - Alloc & Free Impl
// ==============================

static BLGradientPrivateImpl* allocImpl(
  BLGradientCore* self,
  BLObjectImplSize implSize,
  BLGradientType type,
  const void* values,
  BLExtendMode extendMode,
  BLMatrix2DType mType,
  const BLMatrix2D* m) noexcept {

  BL_ASSERT(type <= BL_GRADIENT_TYPE_MAX_VALUE);
  BL_ASSERT(mType <= BL_MATRIX2D_TYPE_MAX_VALUE);
  BL_ASSERT(extendMode <= BL_EXTEND_MODE_SIMPLE_MAX_VALUE);

  BLGradientPrivateImpl* impl = blObjectDetailAllocImplT<BLGradientPrivateImpl>(self,
    BLObjectInfo::packType(BL_OBJECT_TYPE_GRADIENT),
    implSize,
    &implSize);

  if (BL_UNLIKELY(!impl))
    return nullptr;

  impl->stops = BLPtrOps::offset<BLGradientStop>(impl, sizeof(BLGradientPrivateImpl));
  impl->size = 0;
  impl->capacity = capacityFromImplSize(implSize);
  impl->gradientType = uint8_t(type);
  impl->extendMode = uint8_t(extendMode);
  impl->matrixType = uint8_t(mType);
  impl->reserved[0] = 0;
  impl->matrix = *m;
  initValues(impl->values, static_cast<const double*>(values), valueCountTable[type]);
  impl->lut32 = nullptr;
  impl->info32.packed = 0;

  return impl;
}

BLResult freeImpl(BLGradientPrivateImpl* impl, BLObjectInfo info) noexcept {
  invalidateLUTCache(impl);
  return blObjectImplFreeInline(impl, info);
}

// BLGradient - Deep Copy & Mutation
// =================================

static BL_NOINLINE BLResult deepCopy(BLGradientCore* self, const BLGradientCore* other, bool copyCache) noexcept {
  const BLGradientPrivateImpl* otherI = getImpl(other);

  BLGradientCore newO;
  BLGradientPrivateImpl* newI = allocImpl(&newO,
    implSizeFromCapacity(otherI->capacity),
    (BLGradientType)otherI->gradientType,
    otherI->values,
    (BLExtendMode)otherI->extendMode,
    (BLMatrix2DType)otherI->matrixType, &otherI->matrix);

  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  newI->size = copyStops(newI->stops, otherI->stops, otherI->size);

  if (copyCache) {
    newI->lut32 = copyMaybeNullLUT(otherI->lut32);
    newI->info32.packed = otherI->info32.packed;
  }

  return replaceInstance(self, &newO);
}

static BL_INLINE BLResult makeMutable(BLGradientCore* self, bool copyCache) noexcept {
  // NOTE: `copyCache` should be a constant so its handling should have zero cost.
  if (!isMutable(self))
    return deepCopy(self, self, copyCache);

  if (!copyCache)
    return invalidateLUTCache(getImpl(self));

  return BL_SUCCESS;
}

} // {BLGradientPrivate}

// BLGradient - API - Init & Destroy
// =================================

BL_API_IMPL BLResult blGradientInit(BLGradientCore* self) noexcept {
  using namespace BLGradientPrivate;

  self->_d = blObjectDefaults[BL_OBJECT_TYPE_GRADIENT]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blGradientInitMove(BLGradientCore* self, BLGradientCore* other) noexcept {
  using namespace BLGradientPrivate;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isGradient());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_GRADIENT]._d;

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blGradientInitWeak(BLGradientCore* self, const BLGradientCore* other) noexcept {
  using namespace BLGradientPrivate;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isGradient());

  return blObjectPrivateInitWeakTagged(self, other);
}

BL_API_IMPL BLResult blGradientInitAs(BLGradientCore* self, BLGradientType type, const void* values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D* m) noexcept {
  using namespace BLGradientPrivate;

  self->_d = blObjectDefaults[BL_OBJECT_TYPE_GRADIENT]._d;
  return blGradientCreate(self, type, values, extendMode, stops, n, m);
}

BL_API_IMPL BLResult blGradientDestroy(BLGradientCore* self) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  return releaseInstance(self);
}

// BLGradient - API - Reset
// ========================

BL_API_IMPL BLResult blGradientReset(BLGradientCore* self) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  return replaceInstance(self, static_cast<BLGradientCore*>(&blObjectDefaults[BL_OBJECT_TYPE_GRADIENT]));
}

// BLGradient - API - Assign
// =========================

BL_API_IMPL BLResult blGradientAssignMove(BLGradientCore* self, BLGradientCore* other) noexcept {
  using namespace BLGradientPrivate;

  BL_ASSERT(self->_d.isGradient());
  BL_ASSERT(other->_d.isGradient());

  BLGradientCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_GRADIENT]._d;
  return replaceInstance(self, &tmp);
}

BL_API_IMPL BLResult blGradientAssignWeak(BLGradientCore* self, const BLGradientCore* other) noexcept {
  using namespace BLGradientPrivate;

  BL_ASSERT(self->_d.isGradient());
  BL_ASSERT(other->_d.isGradient());

  blObjectPrivateAddRefTagged(other);
  return replaceInstance(self, other);
}

BL_API_IMPL BLResult blGradientCreate(BLGradientCore* self, BLGradientType type, const void* values, BLExtendMode extendMode, const BLGradientStop* stops, size_t n, const BLMatrix2D* m) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  if (BL_UNLIKELY((uint32_t(type) > BL_GRADIENT_TYPE_MAX_VALUE) | (uint32_t(extendMode) > BL_EXTEND_MODE_SIMPLE_MAX_VALUE)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (!values)
    values = noValues;

  BLMatrix2DType mType = BL_MATRIX2D_TYPE_IDENTITY;
  if (!m)
    m = &noMatrix;
  else
    mType = m->type();

  uint32_t analysis = BL_DATA_ANALYSIS_CONFORMING;
  if (n) {
    if (BL_UNLIKELY(stops == nullptr))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    analysis = analyzeStopArray(stops, n);
    if (BL_UNLIKELY(analysis >= BL_DATA_ANALYSIS_INVALID_VALUE))
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  size_t immutableMsk = BLIntOps::bitMaskFromBool<size_t>(!isMutable(self));
  if ((n | immutableMsk) > getCapacity(self)) {
    BLObjectImplSize implSize = blMax(implSizeFromCapacity(n), BLObjectImplSize(BL_GRADIENT_IMPL_INITIAL_SIZE));

    BLGradientCore newO;
    BLGradientPrivateImpl* newI = allocImpl(&newO, implSize, type, values, extendMode, mType, m);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    newI->size = copyUnsafeStops(newI->stops, stops, n, analysis);
    return replaceInstance(self, &newO);
  }
  else {
    BLGradientPrivateImpl* selfI = getImpl(self);

    selfI->gradientType = uint8_t(type);
    selfI->extendMode = uint8_t(extendMode);
    selfI->matrixType = uint8_t(mType);
    selfI->matrix.reset(*m);

    initValues(selfI->values, static_cast<const double*>(values), valueCountTable[type]);
    selfI->size = copyUnsafeStops(selfI->stops, stops, n, analysis);

    return invalidateLUTCache(selfI);
  }
}

// BLGradient - API - Storage
// ==========================

BL_API_IMPL BLResult blGradientShrink(BLGradientCore* self) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  BLGradientPrivateImpl* selfI = getImpl(self);
  BLObjectImplSize currentSize = implSizeFromCapacity(selfI->capacity);
  BLObjectImplSize fittingSize = implSizeFromCapacity(selfI->size);

  if (currentSize - fittingSize < BL_OBJECT_IMPL_ALIGNMENT)
    return BL_SUCCESS;

  BLGradientCore newO;
  BLGradientPrivateImpl* newI = allocImpl(&newO,
    fittingSize,
    (BLGradientType)selfI->gradientType,
    selfI->values,
    (BLExtendMode)selfI->extendMode,
    (BLMatrix2DType)selfI->matrixType,
    &selfI->matrix);

  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  newI->size = copyStops(newI->stops, selfI->stops, selfI->size);
  newI->lut32 = copyMaybeNullLUT(selfI->lut32);

  return replaceInstance(self, &newO);
}

BL_API_IMPL BLResult blGradientReserve(BLGradientCore* self, size_t n) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  BLGradientPrivateImpl* selfI = getImpl(self);
  size_t immutableMsk = BLIntOps::bitMaskFromBool<size_t>(!isMutable(self));

  if ((n | immutableMsk) > selfI->capacity) {
    BLGradientCore newO;

    BLObjectImplSize implSize = blMax(implSizeFromCapacity(n), BLObjectImplSize(BL_GRADIENT_IMPL_INITIAL_SIZE));
    BLGradientPrivateImpl* newI = allocImpl(&newO,
      implSize,
      (BLGradientType)selfI->gradientType,
      selfI->values,
      (BLExtendMode)selfI->extendMode,
      (BLMatrix2DType)selfI->matrixType,
      &selfI->matrix);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    newI->size = copyStops(newI->stops, selfI->stops, selfI->size);
    newI->lut32 = copyMaybeNullLUT(selfI->lut32);

    return replaceInstance(self, &newO);
  }
  else {
    return BL_SUCCESS;
  }
}

// BLGradient - API - Accessors
// ============================

BL_API_IMPL BLGradientType blGradientGetType(const BLGradientCore* self) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  const BLGradientPrivateImpl* selfI = getImpl(self);
  return (BLGradientType)selfI->gradientType;
}

BL_API_IMPL BLResult blGradientSetType(BLGradientCore* self, BLGradientType type) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  if (BL_UNLIKELY(uint32_t(type) > BL_GRADIENT_TYPE_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BL_PROPAGATE(makeMutable(self, true));

  BLGradientPrivateImpl* selfI = getImpl(self);
  selfI->gradientType = uint8_t(type);

  return BL_SUCCESS;
}

BL_API_IMPL BLExtendMode blGradientGetExtendMode(const BLGradientCore* self) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  const BLGradientPrivateImpl* selfI = getImpl(self);
  return (BLExtendMode)selfI->extendMode;
}

BL_API_IMPL BLResult blGradientSetExtendMode(BLGradientCore* self, BLExtendMode extendMode) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  if (BL_UNLIKELY(extendMode > BL_EXTEND_MODE_SIMPLE_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BL_PROPAGATE(makeMutable(self, true));

  BLGradientPrivateImpl* selfI = getImpl(self);
  selfI->extendMode = uint8_t(extendMode);

  return BL_SUCCESS;
}

BL_API_IMPL double blGradientGetValue(const BLGradientCore* self, size_t index) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  if (BL_UNLIKELY(index > BL_GRADIENT_VALUE_MAX_VALUE))
    return blNaN<double>();

  const BLGradientPrivateImpl* selfI = getImpl(self);
  return selfI->values[index];
}

BL_API_IMPL BLResult blGradientSetValue(BLGradientCore* self, size_t index, double value) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  if (BL_UNLIKELY(index > BL_GRADIENT_VALUE_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BL_PROPAGATE(makeMutable(self, true));

  BLGradientPrivateImpl* selfI = getImpl(self);
  selfI->values[index] = value;

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blGradientSetValues(BLGradientCore* self, size_t index, const double* values, size_t valueCount) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  if (BL_UNLIKELY(index > BL_GRADIENT_VALUE_MAX_VALUE || valueCount > BL_GRADIENT_VALUE_MAX_VALUE + 1 - index))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (BL_UNLIKELY(!valueCount))
    return BL_SUCCESS;

  BL_PROPAGATE(makeMutable(self, true));

  BLGradientPrivateImpl* selfI = getImpl(self);
  double* dst = selfI->values + index;

  for (size_t i = 0; i < valueCount; i++)
    dst[i] = values[i];

  return BL_SUCCESS;
}

// BLGradient - API - Stops
// ========================

BL_API_IMPL size_t blGradientGetSize(const BLGradientCore* self) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  return getSize(self);
}

BL_API_IMPL size_t blGradientGetCapacity(const BLGradientCore* self) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  return getCapacity(self);
}

BL_API_IMPL const BLGradientStop* blGradientGetStops(const BLGradientCore* self) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  return getStops(self);
}

BL_API_IMPL BLResult blGradientResetStops(BLGradientCore* self) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  if (!getSize(self))
    return BL_SUCCESS;

  BLGradientPrivateImpl* selfI = getImpl(self);
  if (!isMutable(self)) {
    BLGradientCore newO;

    BLGradientPrivateImpl* newI = allocImpl(&newO,
      BLObjectImplSize(BL_GRADIENT_IMPL_INITIAL_SIZE),
      (BLGradientType)selfI->gradientType,
      selfI->values,
      (BLExtendMode)selfI->extendMode,
      (BLMatrix2DType)selfI->matrixType, &selfI->matrix);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    return replaceInstance(self, &newO);
  }
  else {
    selfI->size = 0;
    return invalidateLUTCache(selfI);
  }
}

BL_API_IMPL BLResult blGradientAssignStops(BLGradientCore* self, const BLGradientStop* stops, size_t n) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  if (n == 0)
    return blGradientResetStops(self);

  BLGradientPrivateImpl* selfI = getImpl(self);
  size_t immutableMsk = BLIntOps::bitMaskFromBool<size_t>(!isMutable(self));
  uint32_t analysis = analyzeStopArray(stops, n);

  if (BL_UNLIKELY(analysis >= BL_DATA_ANALYSIS_INVALID_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if ((n | immutableMsk) > selfI->capacity) {
    BLGradientCore newO;

    BLObjectImplSize implSize = blMax(implSizeFromCapacity(n), BLObjectImplSize(BL_GRADIENT_IMPL_INITIAL_SIZE));
    BLGradientPrivateImpl* newI = allocImpl(
      &newO,
      implSize,
      (BLGradientType)selfI->gradientType,
      selfI->values,
      (BLExtendMode)selfI->extendMode,
      (BLMatrix2DType)selfI->matrixType,
      &selfI->matrix);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    newI->size = copyUnsafeStops(newI->stops, stops, n, analysis);
    return replaceInstance(self, &newO);
  }
  else {
    selfI->size = copyUnsafeStops(selfI->stops, stops, n, analysis);
    return invalidateLUTCache(selfI);
  }
}

BL_API_IMPL BLResult blGradientAddStopRgba32(BLGradientCore* self, double offset, uint32_t rgba32) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  return blGradientAddStopRgba64(self, offset, BLRgbaPrivate::rgba64FromRgba32(rgba32));
}

BL_API_IMPL BLResult blGradientAddStopRgba64(BLGradientCore* self, double offset, uint64_t rgba64) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  if (BL_UNLIKELY(!(offset >= 0.0 && offset <= 1.0)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLGradientPrivateImpl* selfI = getImpl(self);
  BLGradientStop* stops = selfI->stops;

  size_t i = 0;
  size_t n = selfI->size;

  if (n && offset >= stops[0].offset) {
    i = BLAlgorithm::binarySearchClosestLast(stops, n, GradientStopMatcher(offset));

    // If there are two stops that have the same offset then we would replace the second one. This is supported
    // and it would make a sharp transition.
    if (i > 0 && stops[i - 1].offset == offset)
      return blGradientReplaceStopRgba64(self, i, offset, rgba64);

    // Insert a new stop after `i`.
    i++;
  }

  // If we are here it means that we are going to insert a stop at `i`. All other cases were handled at this point
  // so focus on generic insert, which could be just a special case of append operation, but we don't really care.
  size_t immutableMsk = BLIntOps::bitMaskFromBool<size_t>(!isMutable(self));

  if ((n | immutableMsk) >= selfI->capacity) {
    BLGradientCore newO;

    BLObjectImplSize implSize = blObjectExpandImplSize(implSizeFromCapacity(n + 1));
    BLGradientPrivateImpl* newI = allocImpl(&newO,
      implSize,
      (BLGradientType)selfI->gradientType,
      selfI->values,
      (BLExtendMode)selfI->extendMode,
      (BLMatrix2DType)selfI->matrixType, &selfI->matrix);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    BLGradientStop* newStops = newI->stops;
    copyStops(newStops, stops, i);

    newStops[i].reset(offset, BLRgba64(rgba64));
    copyStops(newStops + i + 1, stops + i, n - i);

    newI->size = n + 1;
    return replaceInstance(self, &newO);
  }
  else {
    moveStops(stops + i + 1, stops + i, n - i);
    stops[i].reset(offset, BLRgba64(rgba64));

    selfI->size = n + 1;
    return invalidateLUTCache(selfI);
  }
}

BL_API_IMPL BLResult blGradientRemoveStop(BLGradientCore* self, size_t index) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  return blGradientRemoveStopsByIndex(self, index, index + 1);
}

BL_API_IMPL BLResult blGradientRemoveStopByOffset(BLGradientCore* self, double offset, uint32_t all) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  if (BL_UNLIKELY(!(offset >= 0.0 && offset <= 1.0)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  size_t size = getSize(self);
  const BLGradientStop* stops = getStops(self);

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
      return blGradientRemoveStopsByIndex(self, a, b);
    }
  }

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blGradientRemoveStopsByIndex(BLGradientCore* self, size_t rStart, size_t rEnd) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  size_t size = getSize(self);

  size_t index = rStart;
  size_t end = blMin(rEnd, size);

  if (BL_UNLIKELY(index > size || end < index))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (BL_UNLIKELY(index == end))
    return BL_SUCCESS;

  BLGradientPrivateImpl* selfI = getImpl(self);
  BLGradientStop* stops = selfI->stops;

  size_t removedCount = end - index;
  size_t shiftedCount = size - end;
  size_t afterCount = size - removedCount;

  if (!isMutable(self)) {
    BLGradientCore newO;

    BLGradientPrivateImpl* newI = allocImpl(&newO,
      implSizeFromCapacity(afterCount),
      (BLGradientType)selfI->gradientType,
      selfI->values,
      (BLExtendMode)selfI->extendMode,
      (BLMatrix2DType)selfI->matrixType,
      &selfI->matrix);

    BLGradientStop* newStops = newI->stops;
    copyStops(newStops, stops, index);
    copyStops(newStops + index, stops + end, shiftedCount);

    return replaceInstance(self, &newO);
  }
  else {
    moveStops(stops + index, stops + end, shiftedCount);
    selfI->size = afterCount;
    return invalidateLUTCache(selfI);
  }
}

BL_API_IMPL BLResult blGradientRemoveStopsByOffset(BLGradientCore* self, double offsetMin, double offsetMax) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  if (BL_UNLIKELY(offsetMax < offsetMin))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (!getSize(self))
    return BL_SUCCESS;

  BLGradientPrivateImpl* selfI = getImpl(self);
  const BLGradientStop* stops = selfI->stops;

  size_t size = selfI->size;
  size_t a, b;

  for (a = 0; a < size && stops[a].offset <  offsetMin; a++) continue;
  for (b = a; b < size && stops[b].offset <= offsetMax; b++) continue;

  if (a >= b)
    return BL_SUCCESS;

  return blGradientRemoveStopsByIndex(self, a, b);
}

BL_API_IMPL BLResult blGradientReplaceStopRgba32(BLGradientCore* self, size_t index, double offset, uint32_t rgba32) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  return blGradientReplaceStopRgba64(self, index, offset, BLRgbaPrivate::rgba64FromRgba32(rgba32));
}

BL_API_IMPL BLResult blGradientReplaceStopRgba64(BLGradientCore* self, size_t index, double offset, uint64_t rgba64) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  if (BL_UNLIKELY(index >= getSize(self)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BL_PROPAGATE(makeMutable(self, false));

  BLGradientPrivateImpl* selfI = getImpl(self);
  BLGradientStop* stops = selfI->stops;

  if (stops[index].offset == offset) {
    stops[index].rgba.value = rgba64;
    return BL_SUCCESS;
  }
  else {
    BL_PROPAGATE(blGradientRemoveStop(self, index));
    return blGradientAddStopRgba64(self, offset, rgba64);
  }
}

BL_API_IMPL size_t blGradientIndexOfStop(const BLGradientCore* self, double offset) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  const BLGradientPrivateImpl* selfI = getImpl(self);
  const BLGradientStop* stops = selfI->stops;

  size_t n = selfI->size;
  if (!n)
    return SIZE_MAX;

  size_t i = BLAlgorithm::binarySearch(stops, n, GradientStopMatcher(offset));
  if (i == SIZE_MAX)
    return SIZE_MAX;

  if (i > 0 && stops[i - 1].offset == offset)
    i--;

  return i;
}

// BLGradient - API - Matrix
// =========================

BL_API_IMPL BLResult blGradientApplyMatrixOp(BLGradientCore* self, BLMatrix2DOp opType, const void* opData) noexcept {
  using namespace BLGradientPrivate;
  BL_ASSERT(self->_d.isGradient());

  if (BL_UNLIKELY(uint32_t(opType) > BL_MATRIX2D_OP_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLGradientPrivateImpl* selfI = getImpl(self);
  if (opType == 0 && selfI->matrixType == BL_MATRIX2D_TYPE_IDENTITY)
    return BL_SUCCESS;

  BL_PROPAGATE(makeMutable(self, true));
  selfI = getImpl(self);

  blMatrix2DApplyOp(&selfI->matrix, opType, opData);
  selfI->matrixType = uint8_t(selfI->matrix.type());

  return BL_SUCCESS;
}

// BLGradient - API - Equals
// =========================

BL_API_IMPL bool blGradientEquals(const BLGradientCore* a, const BLGradientCore* b) noexcept {
  using namespace BLGradientPrivate;

  BL_ASSERT(a->_d.isGradient());
  BL_ASSERT(b->_d.isGradient());

  const BLGradientPrivateImpl* aI = getImpl(a);
  const BLGradientPrivateImpl* bI = getImpl(b);

  if (aI == bI)
    return true;

  size_t size = aI->size;
  bool eq = (aI->gradientType == bI->gradientType) &
            (aI->extendMode   == bI->extendMode  ) &
            (aI->matrixType   == bI->matrixType  ) &
            (aI->matrix       == bI->matrix      ) &
            (size             == bI->size        ) ;
  return eq && memcmp(aI->stops, bI->stops, size * sizeof(BLGradientStop)) == 0;
}

// BLGradient - Runtime Registration
// =================================

void blGradientRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  BLGradientPrivate::defaultImpl.impl->matrix.reset();
  blObjectDefaults[BL_OBJECT_TYPE_GRADIENT]._d.initDynamic(
    BL_OBJECT_TYPE_GRADIENT,
    BLObjectInfo{BL_OBJECT_INFO_IMMUTABLE_FLAG},
    &BLGradientPrivate::defaultImpl.impl);
}

// BLGradient - Tests
// ==================

#if defined(BL_TEST)
UNIT(gradient) {
  INFO("Dynamic memory allocation strategy");
  {
    BLGradient g;
    size_t kNumItems = 10000000;
    size_t capacity = g.capacity();

    for (size_t i = 0; i < kNumItems; i++) {
      g.addStop(double(i) / double(kNumItems), BLRgba32(0xFFFFFFFF));

      if (capacity != g.capacity()) {
        size_t implSize = BLGradientPrivate::implSizeFromCapacity(g.capacity()).value();
        INFO("  Capacity increased from %zu to %zu [ImplSize=%zu]\n", capacity, g.capacity(), implSize);

        capacity = g.capacity();
      }
    }
  }

  INFO("Gradient - Linear values");
  {
    BLGradient g(BLLinearGradientValues(0.0, 0.5, 1.0, 1.5));

    EXPECT_EQ(g.type(), BL_GRADIENT_TYPE_LINEAR);
    EXPECT_EQ(g.x0(), 0.0);
    EXPECT_EQ(g.y0(), 0.5);
    EXPECT_EQ(g.x1(), 1.0);
    EXPECT_EQ(g.y1(), 1.5);

    g.setX0(0.15);
    g.setY0(0.85);
    g.setX1(0.75);
    g.setY1(0.25);

    EXPECT_EQ(g.x0(), 0.15);
    EXPECT_EQ(g.y0(), 0.85);
    EXPECT_EQ(g.x1(), 0.75);
    EXPECT_EQ(g.y1(), 0.25);
  }

  INFO("Gradient - Radial values");
  {
    BLGradient g(BLRadialGradientValues(1.0, 1.5, 0.0, 0.5, 500.0));

    EXPECT_EQ(g.type(), BL_GRADIENT_TYPE_RADIAL);
    EXPECT_EQ(g.x0(), 1.0);
    EXPECT_EQ(g.y0(), 1.5);
    EXPECT_EQ(g.x1(), 0.0);
    EXPECT_EQ(g.y1(), 0.5);
    EXPECT_EQ(g.r0(), 500.0);

    g.setR0(150.0);
    EXPECT_EQ(g.r0(), 150.0);
  }

  INFO("Gradient - Conical values");
  {
    BLGradient g(BLConicalGradientValues(1.0, 1.5, 0.1));

    EXPECT_EQ(g.type(), BL_GRADIENT_TYPE_CONICAL);
    EXPECT_EQ(g.x0(), 1.0);
    EXPECT_EQ(g.y0(), 1.5);
    EXPECT_EQ(g.angle(), 0.1);
  }

  INFO("Gradient - Stops");
  {
    BLGradient g;

    g.addStop(0.0, BLRgba32(0x00000000u));
    EXPECT_EQ(g.size(), 1u);
    EXPECT_EQ(g.stopAt(0).rgba.value, 0x0000000000000000u);

    g.addStop(1.0, BLRgba32(0xFF000000u));
    EXPECT_EQ(g.size(), 2u);
    EXPECT_EQ(g.stopAt(1).rgba.value, 0xFFFF000000000000u);

    g.addStop(0.5, BLRgba32(0xFFFF0000u));
    EXPECT_EQ(g.size(), 3u);
    EXPECT_EQ(g.stopAt(1).rgba.value, 0xFFFFFFFF00000000u);

    g.addStop(0.5, BLRgba32(0xFFFFFF00u));
    EXPECT_EQ(g.size(), 4u);
    EXPECT_EQ(g.stopAt(2).rgba.value, 0xFFFFFFFFFFFF0000u);

    g.removeStopByOffset(0.5, true);
    EXPECT_EQ(g.size(), 2u);
    EXPECT_EQ(g.stopAt(0).rgba.value, 0x0000000000000000u);
    EXPECT_EQ(g.stopAt(1).rgba.value, 0xFFFF000000000000u);

    g.addStop(0.5, BLRgba32(0x80000000u));
    EXPECT_EQ(g.size(), 3u);
    EXPECT_EQ(g.stopAt(1).rgba.value, 0x8080000000000000u);

    // Check whether copy-on-write works as expected.
    BLGradient copy(g);
    EXPECT_EQ(copy.size(), 3u);

    g.addStop(0.5, BLRgba32(0xCC000000u));
    EXPECT_EQ(copy.size(), 3u);
    EXPECT_EQ(g.size(), 4u);
    EXPECT_EQ(g.stopAt(0).rgba.value, 0x0000000000000000u);
    EXPECT_EQ(g.stopAt(1).rgba.value, 0x8080000000000000u);
    EXPECT_EQ(g.stopAt(2).rgba.value, 0xCCCC000000000000u);
    EXPECT_EQ(g.stopAt(3).rgba.value, 0xFFFF000000000000u);

    g.resetStops();
    EXPECT_EQ(g.size(), 0u);
  }
}
#endif
