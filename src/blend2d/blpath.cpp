// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blarray_p.h"
#include "./blgeometry_p.h"
#include "./blmath_p.h"
#include "./blmatrix_p.h"
#include "./blpath_p.h"
#include "./blpathstroke_p.h"
#include "./blregion_p.h"
#include "./blruntime_p.h"
#include "./blsupport_p.h"
#include "./bltables_p.h"

// ============================================================================
// [Global Variables]
// ============================================================================

static BLWrap<BLInternalPathImpl> blNullPathImpl;

// ============================================================================
// [BLApproximationOptions]
// ============================================================================

const BLApproximationOptions blDefaultApproximationOptions = blMakeDefaultApproximationOptions();

// ============================================================================
// [BLStrokeOptions - Init / Reset]
// ============================================================================

BLResult blStrokeOptionsInit(BLStrokeOptionsCore* self) noexcept {
  self->hints = 0;
  self->width = 1.0;
  self->miterLimit = 4.0;
  self->dashOffset = 0;
  blCallCtor(self->dashArray);

  return BL_SUCCESS;
}

BLResult blStrokeOptionsInitWeak(BLStrokeOptionsCore* self, const BLStrokeOptionsCore* other) noexcept {
  BLArrayImpl* dashArrayI = other->dashArray.impl;

  self->hints = other->hints;
  self->width = other->width;
  self->miterLimit = other->miterLimit;
  self->dashOffset = other->dashOffset;
  self->dashArray.impl = blImplIncRef(dashArrayI);

  return BL_SUCCESS;
}

BLResult blStrokeOptionsInitMove(BLStrokeOptionsCore* self, BLStrokeOptionsCore* other) noexcept {
  BLArrayImpl* dashArrayI = other->dashArray.impl;
  blCallCtor(other->dashArray);

  self->hints = other->hints;
  self->width = other->width;
  self->miterLimit = other->miterLimit;
  self->dashOffset = other->dashOffset;
  self->dashArray.impl = dashArrayI;

  return BL_SUCCESS;
}

BLResult blStrokeOptionsReset(BLStrokeOptionsCore* self) noexcept {
  self->hints = 0;
  self->width = 1.0;
  self->miterLimit = 4.0;
  self->dashOffset = 0;
  self->dashArray.reset();

  return BL_SUCCESS;
}

// ============================================================================
// [BLStrokeOptions - Assign]
// ============================================================================

BLResult blStrokeOptionsAssignMove(BLStrokeOptionsCore* self, BLStrokeOptionsCore* other) noexcept {
  BLArrayImpl* prevDashArrayI = self->dashArray.impl;

  self->width = other->width;
  self->miterLimit = other->miterLimit;
  self->dashOffset = other->dashOffset;
  self->dashArray.impl = other->dashArray.impl;
  self->hints = other->hints;

  blCallCtor(other->dashArray);
  return blImplDecRefAndTest(prevDashArrayI) ? blArrayImplDelete(prevDashArrayI) : BL_SUCCESS;
}

BLResult blStrokeOptionsAssignWeak(BLStrokeOptionsCore* self, const BLStrokeOptionsCore* other) noexcept {
  BLArrayImpl* prevDashArrayI = self->dashArray.impl;

  self->width = other->width;
  self->miterLimit = other->miterLimit;
  self->dashOffset = other->dashOffset;
  self->dashArray.impl = blImplIncRef(other->dashArray.impl);
  self->hints = other->hints;

  return blImplDecRefAndTest(prevDashArrayI) ? blArrayImplDelete(prevDashArrayI) : BL_SUCCESS;
}

// ============================================================================
// [BLPath - Utilities]
// ============================================================================

static BL_INLINE bool blPathRangeCheck(BLInternalPathImpl* pathI, const BLRange* range, size_t* startOut, size_t* nOut) noexcept {
  size_t start = 0;
  size_t end = pathI->size;

  if (range) {
    start = range->start;
    end = blMin(end, range->end);
  }

  *startOut = start;
  *nOut = end - start;
  return start < end;
}

static BL_INLINE void blPathCopyData(uint8_t* cmdDst, BLPoint* vtxDst, const uint8_t* cmdSrc, const BLPoint* vtxSrc, size_t n) noexcept {
  for (size_t i = 0; i < n; i++) {
    cmdDst[i] = cmdSrc[i];
    vtxDst[i] = vtxSrc[i];
  }
}

// ============================================================================
// [BLPath - Internal]
// ============================================================================

static constexpr size_t blPathImplSizeOf(size_t n = 0) noexcept {
  return blContainerSizeOf(sizeof(BLInternalPathImpl), sizeof(BLPoint) + 1, n);
}

static constexpr size_t blPathCapacityOf(size_t implSize) noexcept {
  return blContainerCapacityOf(sizeof(BLInternalPathImpl), sizeof(BLPoint) + 1, implSize);
}

static constexpr size_t blPathMaximumCapacity() noexcept {
  return blPathCapacityOf(SIZE_MAX);
}

static BL_INLINE size_t blPathFittingCapacity(size_t n) noexcept {
  return blContainerFittingCapacity(blPathImplSizeOf(), sizeof(BLPoint) + 1, n);
}

static BL_INLINE size_t blPathGrowingCapacity(size_t n) noexcept {
  return blContainerGrowingCapacity(blPathImplSizeOf(), sizeof(BLPoint) + 1, n, BL_ALLOC_HINT_PATH2D);
}

static BL_INLINE BLInternalPathImpl* blPathImplNew(size_t capacity) noexcept {
  uint16_t memPoolData;
  BLInternalPathImpl* impl = blRuntimeAllocImplT<BLInternalPathImpl>(blPathImplSizeOf(capacity), &memPoolData);

  if (BL_UNLIKELY(!impl))
    return impl;

  blImplInit(impl, BL_IMPL_TYPE_PATH2D, 0, memPoolData);
  impl->vertexData = blOffsetPtr<BLPoint>(impl, sizeof(BLInternalPathImpl));
  impl->commandData = blOffsetPtr<uint8_t>(impl->vertexData, capacity * sizeof(BLPoint));
  impl->size = 0;
  impl->flags = BL_PATH_FLAG_DIRTY;
  impl->capacity = capacity;
  impl->controlBox.reset();
  impl->boundingBox.reset();

  return impl;
}

// Cannot be static, called by `BLVariant` implementation.
BLResult blPathImplDelete(BLPathImpl* impl_) noexcept {
  BLInternalPathImpl* impl = blInternalCast(impl_);

  uint8_t* implBase = reinterpret_cast<uint8_t*>(impl);
  size_t implSize = blPathImplSizeOf(impl->capacity);
  uint32_t implTraits = impl->implTraits;
  uint32_t memPoolData = impl->memPoolData;

  if (implTraits & BL_IMPL_TRAIT_EXTERNAL) {
    implSize = blPathImplSizeOf() + sizeof(BLExternalImplPreface);
    implBase -= sizeof(BLExternalImplPreface);
    blImplDestroyExternal(impl);
  }

  if (implTraits & BL_IMPL_TRAIT_FOREIGN)
    return BL_SUCCESS;
  else
    return blRuntimeFreeImpl(implBase, implSize, memPoolData);
}

static BL_INLINE BLResult blPathImplRelease(BLInternalPathImpl* impl) noexcept {
  if (blImplDecRefAndTest(impl))
    return blPathImplDelete(impl);
  return BL_SUCCESS;
}

// Plain realloc - allocates a new path, copies its data into it, and replaces the
// impl in `self`. Flags and cached information are cleared.
static BL_NOINLINE BLResult blPathRealloc(BLPathCore* self, size_t newCapacity) noexcept {
  BLInternalPathImpl* newI = blPathImplNew(newCapacity);
  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLInternalPathImpl* oldI = blInternalCast(self->impl);
  size_t size = oldI->size;

  self->impl = newI;
  newI->size = size;
  blPathCopyData(newI->commandData, newI->vertexData, oldI->commandData, oldI->vertexData, size);

  return blPathImplRelease(oldI);
}

// Called by `blPathPrepareAdd` and some others to create a new path, copy
// a content from `self` into it, and release the current impl. The size of
// the new path will be set to `newSize` so this function should really be
// only used as an append fallback.
static BL_NOINLINE BLResult blPathReallocToAdd(BLPathCore* self, size_t newSize, uint8_t** cmdOut, BLPoint** vtxOut) noexcept {
  size_t newCapacity = blPathGrowingCapacity(newSize);
  BLInternalPathImpl* newI = blPathImplNew(newCapacity);

  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLInternalPathImpl* oldI = blInternalCast(self->impl);
  size_t oldSize = oldI->size;

  self->impl = newI;
  newI->size = newSize;
  blPathCopyData(newI->commandData, newI->vertexData, oldI->commandData, oldI->vertexData, oldSize);

  *cmdOut = newI->commandData + oldSize;
  *vtxOut = newI->vertexData + oldSize;

  return blPathImplRelease(oldI);
}

// Called when adding something to the path. Any `n` is always considered safe
// as it would be impossible that a path length would go to half `size_t`. The
// memory required by each vertex is either:
//
//   -  5 bytes (2*i16 + 1 command byte)
//   -  9 bytes (2*f32 + 1 command byte)
//   - 17 bytes (2*f64 + 1 command byte)
//
// This means that a theoretical maximum size of a path without considering its
// header would be:
//
//   `SIZE_MAX / (sizeof(vertex) + sizeof(uint8_t))
//
// which would be always smaller than SIZE_MAX / 2 so we can assume that apending
// two paths would never overflow the maximum possible capacity representable by
// `size_t` type.
static BL_INLINE BLResult blPathPrepareAdd(BLPathCore* self, size_t n, uint8_t** cmdOut, BLPoint** vtxOut) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);

  size_t size = selfI->size;
  size_t sizeAfter = size + n;
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  if ((sizeAfter | immutableMsk) > selfI->capacity)
    return blPathReallocToAdd(self, sizeAfter, cmdOut, vtxOut);

  // Likely case, appending to a path that is not shared and has the required
  // capacity. We have to clear FLAGS in addition to set the new size as
  // flags can contain bits regarding BLPathInfo that will no longer hold.
  selfI->flags = BL_PATH_FLAG_DIRTY;
  selfI->size = sizeAfter;

  *cmdOut = selfI->commandData + size;
  *vtxOut = selfI->vertexData + size;

  return BL_SUCCESS;
}

// ============================================================================
// [BLPath - Init / Reset]
// ============================================================================

BLResult blPathInit(BLPathCore* self) noexcept {
  self->impl = BLPath::none().impl;
  return BL_SUCCESS;
}

BLResult blPathReset(BLPathCore* self) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  self->impl = &blNullPathImpl;
  return blPathImplRelease(selfI);
}

// ============================================================================
// [BLPath - Storage]
// ============================================================================

size_t blPathGetSize(const BLPathCore* self) BL_NOEXCEPT_C {
  return self->impl->size;
}

size_t blPathGetCapacity(const BLPathCore* self) BL_NOEXCEPT_C {
  return self->impl->capacity;
}

const uint8_t* blPathGetCommandData(const BLPathCore* self) BL_NOEXCEPT_C {
  return self->impl->commandData;
}

const BLPoint* blPathGetVertexdData(const BLPathCore* self) BL_NOEXCEPT_C {
  return self->impl->vertexData;
}

BLResult blPathClear(BLPathCore* self) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);

  if (!blImplIsMutable(selfI)) {
    self->impl = BLPath::none().impl;
    return blPathImplRelease(selfI);
  }

  selfI->flags = 0;
  selfI->size = 0;
  return BL_SUCCESS;
}

BLResult blPathShrink(BLPathCore* self) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  size_t size = selfI->size;
  size_t capacity = selfI->capacity;

  if (!size) {
    self->impl = BLPath::none().impl;
    return blPathImplRelease(selfI);
  }

  size_t fittingCapacity = blPathFittingCapacity(size);
  if (fittingCapacity < capacity)
    BL_PROPAGATE(blPathRealloc(self, fittingCapacity));

  // Update path info as this this path may be kept alive for some time.
  uint32_t dummyFlags;
  return blPathGetInfoFlags(self, &dummyFlags);
}

BLResult blPathReserve(BLPathCore* self, size_t n) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  if ((n | immutableMsk) > selfI->capacity)
    return blPathRealloc(self, blPathFittingCapacity(blMax(n, selfI->size)));

  return BL_SUCCESS;
}

BLResult blPathModifyOp(BLPathCore* self, uint32_t op, size_t n, uint8_t** cmdDataOut, BLPoint** vtxDataOut) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);

  size_t index = (op >= BL_MODIFY_OP_APPEND_START) ? selfI->size : size_t(0);
  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));

  size_t remaining = selfI->capacity - index;
  size_t sizeAfter = index + n;

  if ((n | immutableMsk) > remaining) {
    size_t newCapacity =
      (op & BL_MODIFY_OP_GROW_MASK)
        ? blPathGrowingCapacity(sizeAfter)
        : blPathFittingCapacity(sizeAfter);

    BLInternalPathImpl* newI = blPathImplNew(newCapacity);
    if (BL_UNLIKELY(!newI)) {
      *cmdDataOut = nullptr;
      *vtxDataOut = nullptr;
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
    }

    newI->size = sizeAfter;
    *cmdDataOut = newI->commandData + index;
    *vtxDataOut = newI->vertexData + index;
    blPathCopyData(newI->commandData, newI->vertexData, selfI->commandData, selfI->vertexData, index);

    self->impl = newI;
    return blPathImplRelease(selfI);
  }

  if (n) {
    selfI->size = sizeAfter;
  }
  else if (!index) {
    blPathClear(self);
    selfI = blInternalCast(self->impl);
  }

  selfI->flags = BL_PATH_FLAG_DIRTY;
  *vtxDataOut = selfI->vertexData + index;
  *cmdDataOut = selfI->commandData + index;

  return BL_SUCCESS;
}

static BL_INLINE BLResult blPathMakeMutable(BLPathCore* self) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  if (!blImplIsMutable(selfI))
    return blPathRealloc(self, blPathFittingCapacity(selfI->size));
  return BL_SUCCESS;
}
// ============================================================================
// [BLPath - Assign]
// ============================================================================

BLResult blPathAssignMove(BLPathCore* self, BLPathCore* other) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  BLInternalPathImpl* otherI = blInternalCast(other->impl);

  self->impl = otherI;
  other->impl = &blNullPathImpl;

  return blPathImplRelease(selfI);
}

BLResult blPathAssignWeak(BLPathCore* self, const BLPathCore* other) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  BLInternalPathImpl* otherI = blInternalCast(other->impl);

  self->impl = blImplIncRef(otherI);
  return blPathImplRelease(selfI);
}

BLResult blPathAssignDeep(BLPathCore* self, const BLPathCore* other) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  BLInternalPathImpl* otherI = blInternalCast(other->impl);

  size_t size = otherI->size;
  if (!size)
    return blPathClear(self);

  size_t immutableMsk = blBitMaskFromBool<size_t>(!blImplIsMutable(selfI));
  if ((size | immutableMsk) > selfI->capacity) {
    BLInternalPathImpl* newI = blPathImplNew(blPathFittingCapacity(size));

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    newI->size = size;
    blPathCopyData(newI->commandData, newI->vertexData, otherI->commandData, otherI->vertexData, size);

    self->impl = newI;
    return blPathImplRelease(selfI);
  }

  selfI->flags = BL_PATH_FLAG_DIRTY;
  selfI->size = size;

  blPathCopyData(selfI->commandData, selfI->vertexData, otherI->commandData, otherI->vertexData, size);
  return BL_SUCCESS;
}

// ============================================================================
// [BLPath - Arcs Helpers]
// ============================================================================

static const double blArc90DegStepsTable[] = {
  BL_MATH_PI_DIV_2,
  BL_MATH_PI,
  BL_MATH_1p5_PI,
  BL_MATH_2_PI
};

static void blArcToCubicSpline(BLPathAppender& dst, BLPoint c, BLPoint r, double startAngle, double sweepAngle, uint8_t initialCmd, bool maybeRedundantLineTo = false) noexcept {
  double startSin = blSin(startAngle);
  double startCos = blCos(startAngle);

  BLMatrix2D m = BLMatrix2D::makeSinCos(startSin, startCos);
  m.postScale(r);
  m.postTranslate(c);

  if (sweepAngle < 0) {
    m.scale(1.0, -1.0);
    sweepAngle = -sweepAngle;
  }

  BLPoint v1(1.0, 0.0);
  BLPoint vc(1.0, 1.0);
  BLPoint v2;

  if (sweepAngle >= BL_MATH_2_PI - blEpsilon<double>()) {
    sweepAngle = BL_MATH_2_PI;
    v2 = v1;
  }
  else {
    if (blIsNaN(sweepAngle))
      return;

    double sweepSin = blSin(sweepAngle);
    double sweepCos = blCos(sweepAngle);
    v2 = BLPoint(sweepCos, sweepSin);
  }

  BLPoint p0 = m.mapPoint(v1);
  dst.addVertex(initialCmd, p0);

  if (maybeRedundantLineTo && dst.cmd[-1] <= BL_PATH_CMD_ON) {
    BL_ASSERT(initialCmd == BL_PATH_CMD_ON);
    double diff = blMax(blAbs(p0.x - dst.vtx[-2].x), blAbs(p0.y - dst.vtx[-2].y));

    if (diff < blEpsilon<double>())
      dst.back(1);
  }

  size_t i = 0;
  while (sweepAngle > blArc90DegStepsTable[i]) {
    v1 = blNormal(v1);
    BLPoint p1 = m.mapPoint(vc);
    BLPoint p2 = m.mapPoint(v1);
    dst.cubicTo(p0 + (p1 - p0) * BL_MATH_KAPPA, p2 + (p1 - p2) * BL_MATH_KAPPA, p2);

    // Full circle.
    if (++i == 4)
      return;

    vc = blNormal(vc);
    p0 = p2;
  }

  // Calculate the remaining control point.
  vc = v1 + v2;
  vc = 2.0 * vc / blDotProduct(vc, vc);

  // This is actually half of the remaining cos. It is required that v1 dot v2 > -1 holds
  // but we can safely assume it does (only critical for angles close to 180 degrees).
  double w = blSqrt(0.5 * blDotProduct(v1, v2) + 0.5);
  dst.conicTo(m.mapPoint(vc), m.mapPoint(v2), w);
}

// ============================================================================
// [BLPath - Info Updater]
// ============================================================================

class BLPathInfoUpdater {
public:
  uint32_t moveToCount;
  uint32_t flags;
  BLBox controlBox;
  BLBox boundingBox;

  BL_INLINE BLPathInfoUpdater() noexcept
    : moveToCount(0),
      flags(0),
      controlBox(blMaxValue<double>(), blMaxValue<double>(), blMinValue<double>(), blMinValue<double>()),
      boundingBox(blMaxValue<double>(), blMaxValue<double>(), blMinValue<double>(), blMinValue<double>()) {}

  BLResult update(const BLPathView& view, uint32_t hasPrevVertex = false) noexcept {
    const uint8_t* cmdData = view.commandData;
    const uint8_t* cmdEnd = view.commandData + view.size;
    const BLPoint* vtxData = view.vertexData;

    // Iterate over the whole path.
    while (cmdData != cmdEnd) {
      uint32_t c = cmdData[0];
      switch (c) {
        case BL_PATH_CMD_MOVE: {
          moveToCount++;
          hasPrevVertex = true;

          blBoundBoxes(boundingBox, vtxData[0]);

          cmdData++;
          vtxData++;
          break;
        }

        case BL_PATH_CMD_ON: {
          if (!hasPrevVertex)
            return blTraceError(BL_ERROR_INVALID_GEOMETRY);

          blBoundBoxes(boundingBox, vtxData[0]);

          cmdData++;
          vtxData++;
          break;
        }

        case BL_PATH_CMD_QUAD: {
          cmdData += 2;
          vtxData += 2;

          if (cmdData > cmdEnd || !hasPrevVertex)
            return blTraceError(BL_ERROR_INVALID_GEOMETRY);

          flags |= BL_PATH_FLAG_QUADS;
          hasPrevVertex = true;
          blBoundBoxes(boundingBox, vtxData[-1]);

          // Calculate tight bounding-box only when control points are outside the current one.
          const BLPoint& ctrl = vtxData[-2];

          if (!(ctrl.x >= boundingBox.x0 && ctrl.y >= boundingBox.y0 && ctrl.x <= boundingBox.x1 && ctrl.y <= boundingBox.y1)) {
            BLPoint extrema;
            blGetQuadExtremaPoint(vtxData - 3, extrema);
            blBoundBoxes(boundingBox, extrema);
            blBoundBoxes(controlBox, vtxData[-2]);
          }
          break;
        }

        case BL_PATH_CMD_CUBIC: {
          cmdData += 3;
          vtxData += 3;
          if (cmdData > cmdEnd || !hasPrevVertex)
            return blTraceError(BL_ERROR_INVALID_GEOMETRY);

          flags |= BL_PATH_FLAG_CUBICS;
          hasPrevVertex = true;
          blBoundBoxes(boundingBox, vtxData[-1]);

          // Calculate tight bounding-box only when control points are outside of the current one.
          BLPoint ctrlMin = blMin(vtxData[-3], vtxData[-2]);
          BLPoint ctrlMax = blMax(vtxData[-3], vtxData[-2]);

          if (!(ctrlMin.x >= boundingBox.x0 && ctrlMin.y >= boundingBox.y0 && ctrlMax.x <= boundingBox.x1 && ctrlMax.y <= boundingBox.y1)) {
            BLPoint extremas[2];
            blGetCubicExtremaPoints(vtxData - 4, extremas);
            blBoundBoxes(boundingBox, extremas[0]);
            blBoundBoxes(boundingBox, extremas[1]);
            blBoundBoxes(controlBox, vtxData[-3]);
            blBoundBoxes(controlBox, vtxData[-2]);
          }
          break;
        }

        case BL_PATH_CMD_CLOSE:
          hasPrevVertex = false;

          cmdData++;
          vtxData++;
          break;

        default:
          blTraceError(BL_ERROR_INVALID_GEOMETRY);
      }
    }

    controlBox.x0 = blMin(controlBox.x0, boundingBox.x0);
    controlBox.y0 = blMin(controlBox.y0, boundingBox.y0);
    controlBox.x1 = blMax(controlBox.x1, boundingBox.x1);
    controlBox.y1 = blMax(controlBox.y1, boundingBox.y1);

    if (moveToCount > 1)
      flags |= BL_PATH_FLAG_MULTIPLE;

    if (!blIsFinite(controlBox, boundingBox))
      return blTraceError(BL_ERROR_INVALID_GEOMETRY);

    return BL_SUCCESS;
  }
};

// ============================================================================
// [BLPath - Path Construction]
// ============================================================================

struct BLPathVertexCountOfGeometryTypeGen {
  static constexpr uint8_t value(size_t i) noexcept {
    return uint8_t(i == BL_GEOMETRY_TYPE_BOXI       ?  5 :
                   i == BL_GEOMETRY_TYPE_BOXD       ?  5 :
                   i == BL_GEOMETRY_TYPE_RECTI      ?  5 :
                   i == BL_GEOMETRY_TYPE_RECTD      ?  5 :
                   i == BL_GEOMETRY_TYPE_CIRCLE     ? 14 :
                   i == BL_GEOMETRY_TYPE_ELLIPSE    ? 14 :
                   i == BL_GEOMETRY_TYPE_ROUND_RECT ? 18 :
                   i == BL_GEOMETRY_TYPE_ARC        ? 13 :
                   i == BL_GEOMETRY_TYPE_CHORD      ? 20 :
                   i == BL_GEOMETRY_TYPE_PIE        ? 20 :
                   i == BL_GEOMETRY_TYPE_LINE       ?  2 :
                   i == BL_GEOMETRY_TYPE_TRIANGLE   ?  4 : 255);
  }
};

static constexpr const auto blPathVertexCountOfGeometryType =
  blLookupTable<uint8_t, BL_GEOMETRY_TYPE_COUNT, BLPathVertexCountOfGeometryTypeGen>();

static BL_INLINE BLResult blPathAddBoxInternal(BLPathCore* self, double x0, double y0, double x1, double y1, uint32_t dir) noexcept {
  uint8_t* cmdData;
  BLPoint* vtxData;
  BL_PROPAGATE(blPathPrepareAdd(self, 5, &cmdData, &vtxData));

  vtxData[0].reset(x0, y0);
  vtxData[1].reset(x1, y0);
  vtxData[2].reset(x1, y1);
  vtxData[3].reset(x0, y1);
  vtxData[4].reset(blNaN<double>(), blNaN<double>());
  cmdData[0] = BL_PATH_CMD_MOVE;
  cmdData[1] = BL_PATH_CMD_ON;
  cmdData[2] = BL_PATH_CMD_ON;
  cmdData[3] = BL_PATH_CMD_ON;
  cmdData[4] = BL_PATH_CMD_CLOSE;

  if (dir == BL_GEOMETRY_DIRECTION_CW)
    return BL_SUCCESS;

  vtxData[1].reset(x0, y1);
  vtxData[3].reset(x1, y0);
  return BL_SUCCESS;
}

BLResult blPathSetVertexAt(BLPathCore* self, size_t index, uint32_t cmd, double x, double y) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  size_t size = selfI->size;

  if (BL_UNLIKELY(index >= size))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BL_PROPAGATE(blPathMakeMutable(self));
  selfI = blInternalCast(self->impl);

  uint32_t oldCmd = selfI->commandData[index];
  if (cmd == BL_PATH_CMD_PRESERVE) cmd = oldCmd;

  // NOTE: We don't check `cmd` as we don't care of the value. Invalid commands
  // must always be handled by all Blend2D functions anyway so let it fail at
  // some other place if the given `cmd` is invalid.
  selfI->flags = BL_PATH_FLAG_DIRTY;
  selfI->commandData[index] = cmd & 0xFFu;
  selfI->vertexData[index].reset(x, y);

  return BL_SUCCESS;
}

BLResult blPathMoveTo(BLPathCore* self, double x0, double y0) noexcept {
  uint8_t* cmdData;
  BLPoint* vtxData;
  BL_PROPAGATE(blPathPrepareAdd(self, 1, &cmdData, &vtxData));

  vtxData[0].reset(x0, y0);
  cmdData[0] = BL_PATH_CMD_MOVE;

  return BL_SUCCESS;
}

BLResult blPathLineTo(BLPathCore* self, double x1, double y1) noexcept {
  uint8_t* cmdData;
  BLPoint* vtxData;
  BL_PROPAGATE(blPathPrepareAdd(self, 1, &cmdData, &vtxData));

  vtxData[0].reset(x1, y1);
  cmdData[0] = BL_PATH_CMD_ON;

  return BL_SUCCESS;
}

BLResult blPathPolyTo(BLPathCore* self, const BLPoint* poly, size_t count) noexcept {
  uint8_t* cmdData;
  BLPoint* vtxData;
  BL_PROPAGATE(blPathPrepareAdd(self, count, &cmdData, &vtxData));

  for (size_t i = 0; i < count; i++) {
    vtxData[i] = poly[i];
    cmdData[i] = BL_PATH_CMD_ON;
  }

  return BL_SUCCESS;
}

BLResult blPathQuadTo(BLPathCore* self, double x1, double y1, double x2, double y2) noexcept {
  uint8_t* cmdData;
  BLPoint* vtxData;
  BL_PROPAGATE(blPathPrepareAdd(self, 2, &cmdData, &vtxData));

  vtxData[0].reset(x1, y1);
  vtxData[1].reset(x2, y2);

  cmdData[0] = BL_PATH_CMD_QUAD;
  cmdData[1] = BL_PATH_CMD_ON;

  return BL_SUCCESS;
}

BLResult blPathCubicTo(BLPathCore* self, double x1, double y1, double x2, double y2, double x3, double y3) noexcept {
  uint8_t* cmdData;
  BLPoint* vtxData;
  BL_PROPAGATE(blPathPrepareAdd(self, 3, &cmdData, &vtxData));

  vtxData[0].reset(x1, y1);
  vtxData[1].reset(x2, y2);
  vtxData[2].reset(x3, y3);

  cmdData[0] = BL_PATH_CMD_CUBIC;
  cmdData[1] = BL_PATH_CMD_CUBIC;
  cmdData[2] = BL_PATH_CMD_ON;

  return BL_SUCCESS;
}

BLResult blPathSmoothQuadTo(BLPathCore* self, double x2, double y2) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  size_t size = selfI->size;

  if (BL_UNLIKELY(!size || selfI->commandData[size - 1u] >= BL_PATH_CMD_CLOSE))
    return blTraceError(BL_ERROR_NO_MATCHING_VERTEX);

  uint8_t* cmdData;
  BLPoint* vtxData;
  BL_PROPAGATE(blPathPrepareAdd(self, 2, &cmdData, &vtxData));

  double x1 = vtxData[-1].x;
  double y1 = vtxData[-1].y;

  if (size >= 2 && cmdData[-2] == BL_PATH_CMD_QUAD) {
    x1 += x1 - vtxData[-2].x;
    y1 += y1 - vtxData[-2].y;
  }

  vtxData[0].reset(x1, y1);
  vtxData[1].reset(x2, y2);

  cmdData[0] = BL_PATH_CMD_QUAD;
  cmdData[1] = BL_PATH_CMD_ON;

  return BL_SUCCESS;
}

BLResult blPathSmoothCubicTo(BLPathCore* self, double x2, double y2, double x3, double y3) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  size_t size = selfI->size;

  if (BL_UNLIKELY(!size || selfI->commandData[size - 1u] >= BL_PATH_CMD_CLOSE))
    return blTraceError(BL_ERROR_NO_MATCHING_VERTEX);

  uint8_t* cmdData;
  BLPoint* vtxData;
  BL_PROPAGATE(blPathPrepareAdd(self, 2, &cmdData, &vtxData));

  double x1 = vtxData[-1].x;
  double y1 = vtxData[-1].y;

  if (size >= 2 && cmdData[-2] == BL_PATH_CMD_CUBIC) {
    x1 += x1 - vtxData[-2].x;
    y1 += y1 - vtxData[-2].y;
  }

  vtxData[0].reset(x1, y1);
  vtxData[1].reset(x2, y2);
  vtxData[2].reset(x3, y3);

  cmdData[0] = BL_PATH_CMD_CUBIC;
  cmdData[1] = BL_PATH_CMD_CUBIC;
  cmdData[2] = BL_PATH_CMD_ON;

  return BL_SUCCESS;
}

BLResult blPathArcTo(BLPathCore* self, double x, double y, double rx, double ry, double start, double sweep, bool forceMoveTo) noexcept {
  BLPathAppender dst;

  uint8_t initialCmd = BL_PATH_CMD_MOVE;
  bool maybeRedundantLineTo = false;

  if (!forceMoveTo) {
    BLInternalPathImpl* selfI = blInternalCast(self->impl);
    size_t size = selfI->size;

    if (size != 0 && selfI->commandData[size - 1] <= BL_PATH_CMD_ON) {
      initialCmd = BL_PATH_CMD_ON;
      maybeRedundantLineTo = true;
    }
  }

  BL_PROPAGATE(dst.beginAppend(self, 13));
  blArcToCubicSpline(dst, BLPoint(x, y), BLPoint(rx, ry), start, sweep, initialCmd, maybeRedundantLineTo);

  dst.done(self);
  return BL_SUCCESS;
}

BLResult blPathArcQuadrantTo(BLPathCore* self, double x1, double y1, double x2, double y2) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  size_t size = selfI->size;

  if (BL_UNLIKELY(!size || selfI->commandData[size - 1u] >= BL_PATH_CMD_CLOSE))
    return blTraceError(BL_ERROR_NO_MATCHING_VERTEX);

  uint8_t* cmdData;
  BLPoint* vtxData;
  BL_PROPAGATE(blPathPrepareAdd(self, 3, &cmdData, &vtxData));

  BLPoint p0 = vtxData[-1];
  BLPoint p1(x1, y1);
  BLPoint p2(x2, y2);

  vtxData[0].reset(p0 + (p1 - p0) * BL_MATH_KAPPA);
  vtxData[1].reset(p2 + (p1 - p2) * BL_MATH_KAPPA);
  vtxData[2].reset(p2);

  cmdData[0] = BL_PATH_CMD_CUBIC;
  cmdData[1] = BL_PATH_CMD_CUBIC;
  cmdData[2] = BL_PATH_CMD_ON;

  return BL_SUCCESS;
}

BLResult blPathEllipticArcTo(BLPathCore* self, double rx, double ry, double xAxisRotation, bool largeArcFlag, bool sweepFlag, double x1, double y1) noexcept {
  BLPathImpl* selfI = self->impl;
  size_t size = selfI->size;

  if (!size || selfI->commandData[size - 1u] > BL_PATH_CMD_ON)
    return BL_ERROR_NO_MATCHING_VERTEX;

  BLPoint p0 = selfI->vertexData[size - 1u]; // Start point.
  BLPoint p1(x1, y1);                        // End point.

  // Special case - out of range radii.
  //   - See https://www.w3.org/TR/SVG/implnote.html#ArcCorrectionOutOfRangeRadii
  rx = blAbs(rx);
  ry = blAbs(ry);

  // Special case - out of range parameters:
  //   - See https://www.w3.org/TR/SVG/paths.html#ArcOutOfRangeParameters
  if (p0 == p1)
    return BL_SUCCESS;

  if ((!(rx > blEpsilon<double>())) | (!(ry > blEpsilon<double>())))
    return blPathLineTo(self, p1.x, p1.y);

  // Calculate sin/cos for reuse.
  double sin = blSin(xAxisRotation);
  double cos = blCos(xAxisRotation);

  // Inverse rotation to align the ellipse.
  BLMatrix2D m = BLMatrix2D::makeSinCos(-sin, cos);

  // Vector from center (transformed midpoint).
  BLPoint v = m.mapPoint((p0 - p1) * 0.5);

  // If scale > 1 the ellipse will need to be rescaled.
  double scale = blSquare(v.x) / blSquare(rx) +
                 blSquare(v.y) / blSquare(ry) ;
  if (scale > 1.0) {
    scale = blSqrt(scale);
    rx *= scale;
    ry *= scale;
  }

  // Prepend scale.
  m.postScale(1.0 / rx, 1.0 / ry);

  // Calculate unit coordinates.
  BLPoint pp0 = m.mapPoint(p0);
  BLPoint pp1 = m.mapPoint(p1);

  // New vector from center (unit midpoint).
  v = (pp1 - pp0) * 0.5;
  BLPoint pc = pp0 + v;

  // If lenght^2 >= 1 the point is already the center.
  double len2 = blLengthSq(v);
  if (len2 < 1.0) {
    v = blSqrt(1.0 / len2 - 1.0) * blNormal(v);

    if (largeArcFlag != sweepFlag)
      pc += v;
    else
      pc -= v;
  }

  // Both vectors are unit vectors.
  BLPoint v1 = pp0 - pc;
  BLPoint v2 = pp1 - pc;

  // Set up the final transformation matrix.
  m.resetToSinCos(v1.y, v1.x);
  m.postTranslate(pc);
  m.postScale(rx, ry);
  blMatrix2DMultiply(m, m, BLMatrix2D::makeSinCos(sin, cos));

  // We have sin = v1.Cross(v2) / (v1.Length * v2.Length)
  // with length of 'v1' and 'v2' both 1 (unit vectors).
  sin = blCrossProduct(v1, v2);

  // Accordingly cos = v1.Dot(v2) / (v1.Length * v2.Length)
  // to get the angle between 'v1' and 'v2'.
  cos = blDotProduct(v1, v2);

  // So the sweep angle is Atan2(y, x) = Atan2(sin, cos)
  // https://stackoverflow.com/a/16544330
  double sweepAngle = blAtan2(sin, cos);
  if (sweepFlag) {
    // Correct the angle if necessary.
    if (sweepAngle < 0) {
      sweepAngle += BL_MATH_2_PI;
    }

    // |  v1.X  v1.Y  0 |   | v2.X |   | v1.X * v2.X + v1.Y * v2.Y |
    // | -v1.Y  v1.X  0 | * | v2.Y | = | v1.X * v2.Y - v1.Y * v2.X |
    // |  0     0     1 |   | 1    |   | 1                         |
    v2.reset(cos, sin);
  }
  else {
    if (sweepAngle > 0) {
      sweepAngle -= BL_MATH_2_PI;
    }

    // Flip Y.
    m.scale(1.0, -1.0);

    v2.reset(cos, -sin);
    sweepAngle = blAbs(sweepAngle);
  }

  // First quadrant (start and control point).
  v1.reset(1, 0);
  v.reset(1, 1);

  // The the number of 90deg segments we are gonna need. If `i == 1` it means
  // we need one 90deg segment and one smaller segment handled after the loop.
  size_t i = 3;
  if (sweepAngle < BL_MATH_1p5_PI   + BL_MATH_ANGLE_EPSILON) i = 2;
  if (sweepAngle < BL_MATH_PI       + BL_MATH_ANGLE_EPSILON) i = 1;
  if (sweepAngle < BL_MATH_PI_DIV_2 + BL_MATH_ANGLE_EPSILON) i = 0;

  BLPathAppender appender;
  BL_PROPAGATE(appender.begin(self, BL_MODIFY_OP_APPEND_GROW, (i + 1) * 3));

  // Process 90 degree segments.
  while (i) {
    v1 = blNormal(v1);

    // Transformed points of the arc segment.
    pp0 = m.mapPoint(v);
    pp1 = m.mapPoint(v1);
    appender.arcQuadrantTo(pp0, pp1);

    v = blNormal(v);
    i--;
  }

  // Calculate the remaining control point.
  v = v1 + v2;
  v = 2.0 * v / blDotProduct(v, v);

  // Final arc segment.
  pp0 = m.mapPoint(v);
  pp1 = p1;

  // This is actually half of the remaining cos. It is required that v1 dot v2 > -1 holds
  // but we can safely assume it (only critical for angles close to 180 degrees).
  cos = blSqrt(0.5 * (1.0 + blDotProduct(v1, v2)));
  appender.conicTo(pp0, pp1, cos);
  appender.done(self);

  return BL_SUCCESS;
}

BLResult blPathClose(BLPathCore* self) noexcept {
  uint8_t* cmdData;
  BLPoint* vtxData;
  BL_PROPAGATE(blPathPrepareAdd(self, 1, &cmdData, &vtxData));

  vtxData[0].reset(blNaN<double>(), blNaN<double>());
  cmdData[0] = BL_PATH_CMD_CLOSE;

  return BL_SUCCESS;
}

BLResult blPathAddBoxI(BLPathCore* self, const BLBoxI* box, uint32_t dir) noexcept {
  return blPathAddBoxInternal(self, double(box->x0), double(box->y0), double(box->x1), double(box->y1), dir);
}

BLResult blPathAddBoxD(BLPathCore* self, const BLBox* box, uint32_t dir) noexcept {
  return blPathAddBoxInternal(self, box->x0, box->y0, box->x1, box->y1, dir);
}

BLResult blPathAddRectI(BLPathCore* self, const BLRectI* rect, uint32_t dir) noexcept {
  double x0 = double(rect->x);
  double y0 = double(rect->y);
  double x1 = double(rect->w) + x0;
  double y1 = double(rect->h) + y0;
  return blPathAddBoxInternal(self, x0, y0, x1, y1, dir);
}

BLResult blPathAddRectD(BLPathCore* self, const BLRect* rect, uint32_t dir) noexcept {
  double x0 = rect->x;
  double y0 = rect->y;
  double x1 = rect->w + x0;
  double y1 = rect->h + y0;
  return blPathAddBoxInternal(self, x0, y0, x1, y1, dir);
}

static BLResult blPathJoinFigure(BLPathAppender& dst, BLPathIterator src) noexcept {
  if (src.atEnd())
    return BL_SUCCESS;

  bool isClosed = dst.cmd[-1] == BL_PATH_CMD_CLOSE;
  uint8_t initialCmd = uint8_t(isClosed ? BL_PATH_CMD_MOVE : BL_PATH_CMD_ON);

  // Initial vertex (either MOVE or ON). If the initial vertex matches the
  // the last vertex in `dst` we won't emit it as it would be unnecessary.
  if (dst.vtx[-1] != src.vtx[0] || initialCmd == BL_PATH_CMD_MOVE)
    dst.addVertex(initialCmd, src.vtx[0]);

  // Iterate the figure.
  while (!(++src).atEnd())
    dst.addVertex(src.cmd[0], src.vtx[0]);

  return BL_SUCCESS;
}

static BLResult blPathJoinReversedFigure(BLPathAppender& dst, BLPathIterator src) noexcept {
  if (src.atEnd())
    return BL_SUCCESS;

  src.reverse();
  src--;

  bool isClosed = dst.cmd[-1] == BL_PATH_CMD_CLOSE;
  uint8_t initialCmd = uint8_t(isClosed ? BL_PATH_CMD_MOVE : BL_PATH_CMD_ON);
  uint8_t cmd = src.cmd[1];

  // Initial MOVE means the whole figure consists of just a single MOVE.
  if (cmd == BL_PATH_CMD_MOVE) {
    dst.addVertex(initialCmd, src.vtx[1]);
    return BL_SUCCESS;
  }

  // Get whether the figure is closed.
  BL_ASSERT(cmd == BL_PATH_CMD_CLOSE || cmd == BL_PATH_CMD_ON);
  bool hasClose = (cmd == BL_PATH_CMD_CLOSE);

  if (hasClose) {
    // Make sure the next command is ON.
    if (src.atEnd()) {
      dst.close();
      return BL_SUCCESS;
    }

    // We just encountered CLOSE followed by ON (reversed).
    BL_ASSERT(src.cmd[0] == BL_PATH_CMD_ON);
    src--;
  }

  // Initial vertex (either MOVE or ON). If the initial vertex matches the
  // the last vertex in `dst` we won't emit it as it would be unnecessary.
  if (dst.vtx[-1] != src.vtx[1] || initialCmd == BL_PATH_CMD_MOVE)
    dst.addVertex(initialCmd, src.vtx[1]);

  // Iterate the figure.
  if (!src.atEnd()) {
    do {
      dst.addVertex(src.cmd[0], src.vtx[0]);
      src--;
    } while (!src.atEnd());
    // Fix the last vertex to not be MOVE.
    dst.cmd[-1] = BL_PATH_CMD_ON;
  }

  // Emit CLOSE if the figure is closed.
  if (hasClose)
    dst.close();
  return BL_SUCCESS;
}

// If the function succeeds then the number of vertices written to destination
// equals `n`. If the function fails you should not rely on the output data.
//
// The algorithm reverses the path, but not the implicit line assumed in case
// of CLOSE command. This means that for example a sequence like:
//
//   [0,0] [0,1] [1,0] [1,1] [CLOSE]
//
// Would be reversed to:
//
//   [1,1] [1,0] [0,1] [0,0] [CLOSE]
//
// Which is what other libraries do as well.
static BLResult blPathCopyDataReversed(BLPathAppender& dst, BLPathIterator src, uint32_t reverseMode) noexcept {
  for (;;) {
    BLPathIterator next;
    if (reverseMode != BL_PATH_REVERSE_MODE_COMPLETE) {
      // This mode is more complicated as we have to scan the path forward
      // and find the end of each figure so we can then go again backward.
      const uint8_t* p = src.cmd;
      if (p == src.end)
        return BL_SUCCESS;

      uint8_t cmd = p[0];
      if (cmd != BL_PATH_CMD_MOVE)
        return blTraceError(BL_ERROR_INVALID_GEOMETRY);

      while (++p != src.end) {
        // Terminate on MOVE command, but don't consume it.
        if (p[0] == BL_PATH_CMD_MOVE)
          break;

        // Terminate on CLOSE command and consume it as it's part of the figure.
        if (p[0] == BL_PATH_CMD_CLOSE) {
          p++;
          break;
        }
      }

      size_t figureSize = (size_t)(p - src.cmd);

      next.reset(src.cmd + figureSize, src.vtx + figureSize, src.remainingForward() - figureSize);
      src.end = src.cmd + figureSize;
    }

    src.reverse();
    while (!src.atEnd()) {
      uint8_t cmd = src.cmd[0];
      src--;

      // Initial MOVE means the whole figure consists of just a single MOVE.
      if (cmd == BL_PATH_CMD_MOVE) {
        dst.addVertex(cmd, src.vtx[1]);
        continue;
      }

      // Only relevant to non-ON commands
      bool hasClose = (cmd == BL_PATH_CMD_CLOSE);
      if (cmd != BL_PATH_CMD_ON) {
        // A figure cannot end with anything else than MOVE|ON|CLOSE.
        if (!hasClose)
          return blTraceError(BL_ERROR_INVALID_GEOMETRY);

        // Make sure the next command is ON, continue otherwise.
        if (src.atEnd() || src.cmd[0] != BL_PATH_CMD_ON) {
          dst.addVertex(BL_PATH_CMD_CLOSE, src.vtx[1]);
          continue;
        }
        src--;
      }

      // Each figure starts with MOVE.
      dst.moveTo(src.vtx[1]);

      // Iterate the figure.
      while (!src.atEnd()) {
        cmd = src.cmd[0];
        if (cmd == BL_PATH_CMD_MOVE) {
          dst.addVertex(BL_PATH_CMD_ON, src.vtx[0]);
          src--;
          break;
        }

        if (cmd == BL_PATH_CMD_CLOSE)
          break;

        dst.addVertex(src.cmd[0], src.vtx[0]);
        src--;
      }

      // Emit CLOSE if the figure is closed.
      if (hasClose)
        dst.close();
    }

    if (reverseMode == BL_PATH_REVERSE_MODE_COMPLETE)
      return BL_SUCCESS;
    src = next;
  }
}

BLResult blPathAddGeometry(BLPathCore* self, uint32_t geometryType, const void* geometryData, const BLMatrix2D* m, uint32_t dir) noexcept {
  if (BL_UNLIKELY(geometryType >= BL_GEOMETRY_TYPE_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  size_t n = blPathVertexCountOfGeometryType[geometryType];
  if (n == 255) {
    switch (geometryType) {
      // We don't expect this often so that's why we pessimistically check it here...
      case BL_GEOMETRY_TYPE_NONE:
        return BL_SUCCESS;

      case BL_GEOMETRY_TYPE_POLYLINED:
      case BL_GEOMETRY_TYPE_POLYLINEI:
        n = static_cast<const BLArrayView<void>*>(geometryData)->size;
        if (!n)
          return BL_SUCCESS;
        break;

      case BL_GEOMETRY_TYPE_POLYGOND:
      case BL_GEOMETRY_TYPE_POLYGONI:
        n = static_cast<const BLArrayView<void>*>(geometryData)->size;
        if (!n)
          return BL_SUCCESS;
        n++;
        break;

      case BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXD:
      case BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXI:
      case BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTD:
      case BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTI: {
        n = static_cast<const BLArrayView<void>*>(geometryData)->size;
        if (!n)
          return BL_SUCCESS;

        n = blUMulSaturate<size_t>(n, 5);
        break;
      }

      case BL_GEOMETRY_TYPE_PATH: {
        const BLPath* other = static_cast<const BLPath*>(geometryData);
        n = other->size();
        if (!n)
          return BL_SUCCESS;

        if (dir == BL_GEOMETRY_DIRECTION_CW) {
          if (m)
            return blPathAddTransformedPath(self, other, nullptr, m);
          else
            return blPathAddPath(self, other, nullptr);
        }
        break;
      }

      case BL_GEOMETRY_TYPE_REGION: {
        n = static_cast<const BLRegion*>(geometryData)->size();
        if (!n)
          return BL_SUCCESS;

        n = blUMulSaturate<size_t>(n, 5);
        break;
      }

      // Should never be reached as we filtered all border cases already...
      default:
        return blTraceError(BL_ERROR_INVALID_VALUE);
    }
  }

  // Should never be zero if we went here.
  BL_ASSERT(n != 0);
  size_t initialSize = self->impl->size;

  BLPathAppender appender;
  BL_PROPAGATE(appender.beginAppend(self, n));

  // For adding 'BLBox', 'BLBoxI', 'BLRect', 'BLRectI', and `BLRoundRect`.
  double x0, y0;
  double x1, y1;

  switch (geometryType) {
    case BL_GEOMETRY_TYPE_BOXI:
      x0 = double(static_cast<const BLBoxI*>(geometryData)->x0);
      y0 = double(static_cast<const BLBoxI*>(geometryData)->y0);
      x1 = double(static_cast<const BLBoxI*>(geometryData)->x1);
      y1 = double(static_cast<const BLBoxI*>(geometryData)->y1);
      goto AddBoxD;

    case BL_GEOMETRY_TYPE_BOXD:
      x0 = static_cast<const BLBox*>(geometryData)->x0;
      y0 = static_cast<const BLBox*>(geometryData)->y0;
      x1 = static_cast<const BLBox*>(geometryData)->x1;
      y1 = static_cast<const BLBox*>(geometryData)->y1;
      goto AddBoxD;

    case BL_GEOMETRY_TYPE_RECTI:
      x0 = double(static_cast<const BLRectI*>(geometryData)->x);
      y0 = double(static_cast<const BLRectI*>(geometryData)->y);
      x1 = double(static_cast<const BLRectI*>(geometryData)->w) + x0;
      y1 = double(static_cast<const BLRectI*>(geometryData)->h) + y0;
      goto AddBoxD;

    case BL_GEOMETRY_TYPE_RECTD:
      x0 = static_cast<const BLRect*>(geometryData)->x;
      y0 = static_cast<const BLRect*>(geometryData)->y;
      x1 = static_cast<const BLRect*>(geometryData)->w + x0;
      y1 = static_cast<const BLRect*>(geometryData)->h + y0;

AddBoxD:
      appender.addBox(x0, y0, x1, y1, dir);
      break;

    case BL_GEOMETRY_TYPE_CIRCLE:
    case BL_GEOMETRY_TYPE_ELLIPSE: {
      double rx, kx;
      double ry, ky;

      if (geometryType == BL_GEOMETRY_TYPE_CIRCLE) {
        const BLCircle* circle = static_cast<const BLCircle*>(geometryData);
        x0 = circle->cx;
        y0 = circle->cy;
        rx = circle->r;
        ry = blAbs(rx);
      }
      else {
        const BLEllipse* ellipse = static_cast<const BLEllipse*>(geometryData);
        x0 = ellipse->cx;
        y0 = ellipse->cy;
        rx = ellipse->rx;
        ry = ellipse->ry;
      }

      if (dir != BL_GEOMETRY_DIRECTION_CW)
        ry = -ry;

      kx = rx * BL_MATH_KAPPA;
      ky = ry * BL_MATH_KAPPA;

      appender.moveTo(x0 + rx, y0);
      appender.cubicTo(x0 + rx, y0 + ky, x0 + kx, y0 + ry, x0     , y0 + ry);
      appender.cubicTo(x0 - kx, y0 + ry, x0 - rx, y0 + ky, x0 - rx, y0     );
      appender.cubicTo(x0 - rx, y0 - ky, x0 - kx, y0 - ry, x0     , y0 - ry);
      appender.cubicTo(x0 + kx, y0 - ry, x0 + rx, y0 - ky, x0 + rx, y0     );
      appender.close();
      break;
    }

    case BL_GEOMETRY_TYPE_ROUND_RECT: {
      const BLRoundRect* round = static_cast<const BLRoundRect*>(geometryData);

      x0 = round->x;
      y0 = round->y;
      x1 = round->x + round->w;
      y1 = round->y + round->h;

      double wHalf = round->w * 0.5;
      double hHalf = round->h * 0.5;

      double rx = blMin(blAbs(round->rx), wHalf);
      double ry = blMin(blAbs(round->ry), hHalf);

      // Degrade to box if rx/ry are degenerate.
      if (BL_UNLIKELY(!(rx > blEpsilon<double>() && ry > blEpsilon<double>())))
        goto AddBoxD;

      double kx = rx * (1.0 - BL_MATH_KAPPA);
      double ky = ry * (1.0 - BL_MATH_KAPPA);

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        appender.moveTo(x0 + rx, y0);
        appender.lineTo(x1 - rx, y0);
        appender.cubicTo(x1 - kx, y0, x1, y0 + ky, x1, y0 + ry);
        appender.lineTo(x1, y1 - ry);
        appender.cubicTo(x1, y1 - ky, x1 - kx, y1, x1 - rx, y1);
        appender.lineTo(x0 + rx, y1);
        appender.cubicTo(x0 + kx, y1, x0, y1 - ky, x0, y1 - ry);
        appender.lineTo(x0, y0 + ry);
        appender.cubicTo(x0, y0 + ky, x0 + kx, y0, x0 + rx, y0);
        appender.close();
      }
      else {
        appender.moveTo(x0 + rx, y0);
        appender.cubicTo(x0 + kx, y0, x0, y0 + ky, x0, y0 + ry);
        appender.lineTo(x0, y1 - ry);
        appender.cubicTo(x0, y1 - ky, x0 + kx, y1, x0 + rx, y1);
        appender.lineTo(x1 - rx, y1);
        appender.cubicTo(x1 - kx, y1, x1, y1 - ky, x1, y1 - ry);
        appender.lineTo(x1, y0 + ry);
        appender.cubicTo(x1, y0 + ky, x1 - kx, y0, x1 - rx, y0);
        appender.close();
      }
      break;
    }

    case BL_GEOMETRY_TYPE_LINE: {
      const BLPoint* src = static_cast<const BLPoint*>(geometryData);
      size_t first = dir != BL_GEOMETRY_DIRECTION_CW;

      appender.moveTo(src[first]);
      appender.lineTo(src[first ^ 1]);
      break;
    }

    case BL_GEOMETRY_TYPE_ARC: {
      const BLArc* arc = static_cast<const BLArc*>(geometryData);

      BLPoint c = arc->center;
      BLPoint r = arc->radius;
      double start = arc->start;
      double sweep = arc->sweep;

      if (dir != BL_GEOMETRY_DIRECTION_CW)
        sweep = -sweep;

      blArcToCubicSpline(appender, c, r, start, sweep, BL_PATH_CMD_MOVE);
      break;
    }

    case BL_GEOMETRY_TYPE_CHORD:
    case BL_GEOMETRY_TYPE_PIE: {
      const BLArc* arc = static_cast<const BLArc*>(geometryData);

      BLPoint c = arc->center;
      BLPoint r = arc->radius;
      double start = arc->start;
      double sweep = arc->sweep;

      if (dir != BL_GEOMETRY_DIRECTION_CW)
        sweep = -sweep;

      uint8_t arcInitialCmd = BL_PATH_CMD_MOVE;
      if (geometryType == BL_GEOMETRY_TYPE_PIE) {
        appender.moveTo(c);
        arcInitialCmd = BL_PATH_CMD_ON;
      }

      blArcToCubicSpline(appender, c, r, start, sweep, arcInitialCmd);
      appender.close();
      break;
    }

    case BL_GEOMETRY_TYPE_TRIANGLE: {
      const BLPoint* src = static_cast<const BLPoint*>(geometryData);
      size_t cw = dir == BL_GEOMETRY_DIRECTION_CW ? 0 : 2;

      appender.moveTo(src[0 + cw]);
      appender.lineTo(src[1]);
      appender.lineTo(src[2 - cw]);
      appender.close();
      break;
    }

    case BL_GEOMETRY_TYPE_POLYLINEI: {
      const BLArrayView<BLPointI>* array = static_cast<const BLArrayView<BLPointI>*>(geometryData);
      const BLPointI* src = array->data;

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = n; i; i--)
          appender.lineTo(*src++);
      }
      else {
        src += n - 1;
        for (size_t i = n; i; i--)
          appender.lineTo(*src--);
      }

      appender.cmd[-intptr_t(n)] = BL_PATH_CMD_MOVE;
      break;
    }

    case BL_GEOMETRY_TYPE_POLYLINED: {
      const BLArrayView<BLPoint>* array = static_cast<const BLArrayView<BLPoint>*>(geometryData);
      const BLPoint* src = array->data;

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = n; i; i--)
          appender.lineTo(*src++);
      }
      else {
        src += n - 1;
        for (size_t i = n; i; i--)
          appender.lineTo(*src--);
      }

      appender.cmd[-intptr_t(n)] = BL_PATH_CMD_MOVE;
      break;
    }

    case BL_GEOMETRY_TYPE_POLYGONI: {
      const BLArrayView<BLPointI>* array = static_cast<const BLArrayView<BLPointI>*>(geometryData);
      const BLPointI* src = array->data;

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = n - 1; i; i--)
          appender.lineTo(*src++);
      }
      else {
        src += n - 1;
        for (size_t i = n - 1; i; i--)
          appender.lineTo(*src--);
      }

      appender.close();
      appender.cmd[-intptr_t(n)] = BL_PATH_CMD_MOVE;
      break;
    }

    case BL_GEOMETRY_TYPE_POLYGOND: {
      const BLArrayView<BLPoint>* array = static_cast<const BLArrayView<BLPoint>*>(geometryData);
      const BLPoint* src = array->data;

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = n - 1; i; i--)
          appender.lineTo(*src++);
      }
      else {
        src += n - 1;
        for (size_t i = n - 1; i; i--)
          appender.lineTo(*src--);
      }

      appender.close();
      appender.cmd[-intptr_t(n)] = BL_PATH_CMD_MOVE;
      break;
    }

    case BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXI: {
      const BLArrayView<BLBoxI>* array = static_cast<const BLArrayView<BLBoxI>*>(geometryData);
      const BLBoxI* src = array->data;

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = n; i != 0; i--, src++) {
          if (!blIsValid(*src))
            continue;
          appender.addBoxCW(src->x0, src->y0, src->x1, src->y1);
        }
      }
      else {
        src += n - 1;
        for (size_t i = n; i != 0; i--, src--) {
          if (!blIsValid(*src))
            continue;
          appender.addBoxCCW(src->x0, src->y0, src->x1, src->y1);
        }
      }
      break;
    }

    case BL_GEOMETRY_TYPE_ARRAY_VIEW_BOXD: {
      const BLArrayView<BLBox>* array = static_cast<const BLArrayView<BLBox>*>(geometryData);
      const BLBox* src = array->data;

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = n; i != 0; i--, src++) {
          if (!blIsValid(*src))
            continue;
          appender.addBoxCW(src->x0, src->y0, src->x1, src->y1);
        }
      }
      else {
        src += n - 1;
        for (size_t i = n; i != 0; i--, src--) {
          if (!blIsValid(*src))
            continue;
          appender.addBoxCCW(src->x0, src->y0, src->x1, src->y1);
        }
      }
      break;
    }

    case BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTI: {
      const BLArrayView<BLRectI>* array = static_cast<const BLArrayView<BLRectI>*>(geometryData);
      const BLRectI* src = array->data;

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = n; i != 0; i--, src++) {
          if (!blIsValid(*src))
            continue;

          x0 = double(src->x);
          y0 = double(src->y);
          x1 = double(src->w) + x0;
          y1 = double(src->h) + y0;
          appender.addBoxCW(x0, y0, x1, y1);
        }
      }
      else {
        src += n - 1;
        for (size_t i = n; i != 0; i--, src--) {
          if (!blIsValid(*src))
            continue;

          x0 = double(src->x);
          y0 = double(src->y);
          x1 = double(src->w) + x0;
          y1 = double(src->h) + y0;
          appender.addBoxCCW(x0, y0, x1, y1);
        }
      }
      break;
    }

    case BL_GEOMETRY_TYPE_ARRAY_VIEW_RECTD: {
      const BLArrayView<BLRect>* array = static_cast<const BLArrayView<BLRect>*>(geometryData);
      const BLRect* src = array->data;

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = n; i != 0; i--, src++) {
          if (!blIsValid(*src))
            continue;

          x0 = src->x;
          y0 = src->y;
          x1 = src->w + x0;
          y1 = src->h + y0;
          appender.addBoxCW(x0, y0, x1, y1);
        }
      }
      else {
        src += n - 1;
        for (size_t i = n; i != 0; i--, src--) {
          if (!blIsValid(*src))
            continue;

          x0 = src->x;
          y0 = src->y;
          x1 = src->w + x0;
          y1 = src->h + y0;
          appender.addBoxCCW(x0, y0, x1, y1);
        }
      }
      break;
    }

    case BL_GEOMETRY_TYPE_PATH: {
      // Only for appending path in reverse order, otherwise we use a better approach.
      BL_ASSERT(dir != BL_GEOMETRY_DIRECTION_CW);

      const BLInternalPathImpl* otherI = blInternalCast(static_cast<const BLPath*>(geometryData)->impl);
      BLResult result = blPathCopyDataReversed(appender, BLPathIterator(otherI->view), BL_PATH_REVERSE_MODE_COMPLETE);

      if (result != BL_SUCCESS) {
        self->impl->size = initialSize;
        return result;
      }
      break;
    }

    case BL_GEOMETRY_TYPE_REGION: {
      const BLRegion* region = static_cast<const BLRegion*>(geometryData);
      const BLBoxI* src = region->data();

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = n; i != 0; i--, src++)
          appender.addBoxCW(src->x0, src->y0, src->x1, src->y1);
      }
      else {
        src += n - 1;
        for (size_t i = n; i != 0; i--, src--)
          appender.addBoxCCW(src->x0, src->y0, src->x1, src->y1);
      }
      break;
    }

    default:
      // This is not possible considering even bad input as we have filtered this already.
      BL_NOT_REACHED();
  }

  appender.done(self);
  if (!m)
    return BL_SUCCESS;

  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  BLPoint* vtxData = selfI->vertexData + initialSize;
  return blMatrix2DMapPointDArray(m, vtxData, vtxData, selfI->size - initialSize);
}

BLResult blPathAddPath(BLPathCore* self, const BLPathCore* other, const BLRange* range) noexcept {
  BLInternalPathImpl* otherI = blInternalCast(other->impl);
  size_t start, n;

  if (!blPathRangeCheck(otherI, range, &start, &n))
    return BL_SUCCESS;

  uint8_t* cmdData;
  BLPoint* vtxData;

  // Maybe `self` and `other` are the same, so get the `other` impl.
  BL_PROPAGATE(blPathPrepareAdd(self, n, &cmdData, &vtxData));
  otherI = blInternalCast(other->impl);

  blPathCopyData(cmdData, vtxData, otherI->commandData + start, otherI->vertexData + start, n);
  return BL_SUCCESS;
}

BLResult blPathAddTranslatedPath(BLPathCore* self, const BLPathCore* other, const BLRange* range, const BLPoint* p) noexcept {
  BLMatrix2D m = BLMatrix2D::makeTranslation(*p);
  return blPathAddTransformedPathWithType(self, other, range, &m, BL_MATRIX2D_TYPE_TRANSLATE);
}

BLResult blPathAddTransformedPath(BLPathCore* self, const BLPathCore* other, const BLRange* range, const BLMatrix2D* m) noexcept {
  BLInternalPathImpl* otherI = blInternalCast(other->impl);
  size_t start, n;

  if (!blPathRangeCheck(otherI, range, &start, &n))
    return BL_SUCCESS;

  uint8_t* cmdData;
  BLPoint* vtxData;

  // Maybe `self` and `other` were the same, so get the `other` impl again.
  BL_PROPAGATE(blPathPrepareAdd(self, n, &cmdData, &vtxData));
  otherI = blInternalCast(other->impl);

  // Only check the matrix type if we reach the limit as the check costs some cycles.
  uint32_t mType = (n >= BL_MATRIX_TYPE_MINIMUM_SIZE) ? m->type() : BL_MATRIX2D_TYPE_AFFINE;

  memcpy(cmdData, otherI->commandData + start, n);
  return blMatrix2DMapPointDArrayFuncs[mType](m, vtxData, otherI->vertexData + start, n);
}

BLResult blPathAddTransformedPathWithType(BLPathCore* self, const BLPathCore* other, const BLRange* range, const BLMatrix2D* m, uint32_t mType) noexcept {
  BLInternalPathImpl* otherI = blInternalCast(other->impl);
  size_t start, n;

  if (!blPathRangeCheck(otherI, range, &start, &n))
    return BL_SUCCESS;

  uint8_t* cmdData;
  BLPoint* vtxData;

  // Maybe `self` and `other` were the same, so get the `other` impl again.
  BL_PROPAGATE(blPathPrepareAdd(self, n, &cmdData, &vtxData));
  otherI = blInternalCast(other->impl);

  memcpy(cmdData, otherI->commandData + start, n);
  return blMatrix2DMapPointDArrayFuncs[mType](m, vtxData, otherI->vertexData + start, n);
}

BLResult blPathAddReversedPath(BLPathCore* self, const BLPathCore* other, const BLRange* range, uint32_t reverseMode) noexcept {
  if (BL_UNLIKELY(reverseMode >= BL_PATH_REVERSE_MODE_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLInternalPathImpl* otherI = blInternalCast(other->impl);
  size_t start, n;

  if (!blPathRangeCheck(otherI, range, &start, &n))
    return BL_SUCCESS;

  size_t initialSize = self->impl->size;

  BLPathAppender dst;
  BL_PROPAGATE(dst.beginAppend(self, n));

  // Maybe `self` and `other` were the same, so get the `other` impl again.
  otherI = blInternalCast(other->impl);
  BLPathIterator src(otherI->commandData + start, otherI->vertexData + start, n);

  BLResult result = blPathCopyDataReversed(dst, src, reverseMode);
  dst.done(self);

  // Don't keep anything if reversal failed.
  if (result != BL_SUCCESS)
    self->impl->size = initialSize;
  return result;
}

// ============================================================================
// [BLPath - Stroke]
// ============================================================================

static BLResult blPathAddStrokedPathSink(BLPath* a, BLPath* b, BLPath* c, void* closure) noexcept {
  BLPathAppender dst;
  BL_PROPAGATE(dst.begin(a, BL_MODIFY_OP_APPEND_GROW, b->size() + c->size()));

  BLResult result = blPathJoinReversedFigure(dst, BLPathIterator(b->view()));
  result |= blPathJoinFigure(dst, BLPathIterator(c->view()));

  dst.done(a);
  return result;
}

BLResult blPathAddStrokedPath(BLPathCore* self, const BLPathCore* other, const BLRange* range, const BLStrokeOptionsCore* options, const BLApproximationOptions* approx) noexcept {
  BLInternalPathImpl* otherI = blInternalCast(other->impl);
  size_t start, n;

  if (!blPathRangeCheck(otherI, range, &start, &n))
    return BL_SUCCESS;

  if (!approx)
    approx = &blDefaultApproximationOptions;

  BLPathView input { otherI->commandData + start, otherI->vertexData + start, n };
  BLPath bPath;
  BLPath cPath;

  if (self == other) {
    // Border case, we don't want anything to happen to the `other` path during
    // processing. And since stroking may need to reallocate the output path it
    // would be unsafe.
    BLPath tmp(blDownCast(*other));
    return blPathStrokeInternal(input, blDownCast(*options), *approx, blDownCast(self), &bPath, &cPath, blPathAddStrokedPathSink, nullptr);
  }
  else {
    return blPathStrokeInternal(input, blDownCast(*options), *approx, blDownCast(self), &bPath, &cPath, blPathAddStrokedPathSink, nullptr);
  }
}

// ============================================================================
// [BLPath - Path Transformations]
// ============================================================================

BLResult blPathTranslate(BLPathCore* self, const BLRange* range, const BLPoint* p) noexcept {
  BLMatrix2D m = BLMatrix2D::makeTranslation(*p);
  return blPathTransformWithType(self, range, &m, BL_MATRIX2D_TYPE_TRANSLATE);
}

BLResult blPathTransform(BLPathCore* self, const BLRange* range, const BLMatrix2D* m) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  size_t start, n;

  if (!blPathRangeCheck(selfI, range, &start, &n))
    return BL_SUCCESS;

  BL_PROPAGATE(blPathMakeMutable(self));
  selfI = blInternalCast(self->impl);

  // Only check the matrix type if we reach the limit as the check costs some cycles.
  uint32_t mType = (n >= BL_MATRIX_TYPE_MINIMUM_SIZE) ? m->type() : BL_MATRIX2D_TYPE_AFFINE;

  BLPoint* vtxData = selfI->vertexData + start;
  return blMatrix2DMapPointDArrayFuncs[mType](m, vtxData, vtxData, n);
}

BLResult blPathFitTo(BLPathCore* self, const BLRange* range, const BLRect* rect, uint32_t fitFlags) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  size_t start, n;

  if (!blPathRangeCheck(selfI, range, &start, &n))
    return BL_SUCCESS;

  if (!blIsFinite(*rect) || rect->w <= 0.0 || rect->h <= 0.0)
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLPathInfoUpdater updater;
  BL_PROPAGATE(updater.update(BLPathView { selfI->commandData + start, selfI->vertexData + start, n }, true));

  // TODO: Honor `fitFlags`.

  const BLBox& bBox = updater.boundingBox;

  double bx = bBox.x0;
  double by = bBox.y0;
  double bw = bBox.x1 - bBox.x0;
  double bh = bBox.y1 - bBox.y0;

  double tx = rect->x;
  double ty = rect->y;
  double sx = rect->w / bw;
  double sy = rect->h / bh;

  tx -= bx * sx;
  ty -= by * sy;

  BLMatrix2D m(sx, 0.0, 0.0, sy, tx, ty);
  return blPathTransformWithType(self, range, &m, BL_MATRIX2D_TYPE_SCALE);
}

BLResult blPathTransformWithType(BLPathCore* self, const BLRange* range, const BLMatrix2D* m, uint32_t mType) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  size_t start, n;

  if (!blPathRangeCheck(selfI, range, &start, &n))
    return BL_SUCCESS;

  BL_PROPAGATE(blPathMakeMutable(self));
  selfI = blInternalCast(self->impl);

  BLPoint* vtxData = selfI->vertexData + start;
  return blMatrix2DMapPointDArrayFuncs[mType](m, vtxData, vtxData, n);
}

// ============================================================================
// [BLPath - Equals]
// ============================================================================

bool blPathEquals(const BLPathCore* a, const BLPathCore* b) noexcept {
  const BLInternalPathImpl* aI = blInternalCast(a->impl);
  const BLInternalPathImpl* bI = blInternalCast(b->impl);

  if (aI == bI)
    return true;

  size_t size = aI->size;
  if (size != bI->size)
    return false;

  return memcmp(aI->commandData, bI->commandData, size * sizeof(uint8_t)) == 0 &&
         memcmp(aI->vertexData , bI->vertexData , size * sizeof(BLPoint)) == 0;
}

// ============================================================================
// [BLPath - Path Info]
// ============================================================================

static BL_NOINLINE BLResult blPathUpdateInfoInternal(BLInternalPathImpl* selfI) noexcept {
  // Special-case. The path info is valid, but the path is invalid. We handle
  // it here to simplify `blPathEnsureInfo()` and to make it a bit shorter.
  if (selfI->flags & BL_PATH_FLAG_INVALID)
    return blTraceError(BL_ERROR_INVALID_GEOMETRY);

  BLPathInfoUpdater updater;
  BLResult result = updater.update(selfI->view);

  // Path is invalid.
  if (result != BL_SUCCESS) {
    selfI->flags = updater.flags | BL_PATH_FLAG_INVALID;
    selfI->controlBox.reset();
    selfI->boundingBox.reset();
    return result;
  }

  // Path is empty.
  if (!(updater.boundingBox.x0 <= updater.boundingBox.x1 &&
        updater.boundingBox.y0 <= updater.boundingBox.y1)) {
    selfI->flags = updater.flags | BL_PATH_FLAG_EMPTY;
    selfI->controlBox.reset();
    selfI->boundingBox.reset();
    return BL_SUCCESS;
  }

  // Path is valid.
  selfI->flags = updater.flags;
  selfI->controlBox = updater.controlBox;
  selfI->boundingBox = updater.boundingBox;
  return BL_SUCCESS;
}

static BL_INLINE BLResult blPathEnsureInfo(BLInternalPathImpl* selfI) noexcept {
  if (selfI->flags & (BL_PATH_FLAG_INVALID | BL_PATH_FLAG_DIRTY))
    return blPathUpdateInfoInternal(selfI);

  return BL_SUCCESS;
}

BLResult blPathGetInfoFlags(const BLPathCore* self, uint32_t* flagsOut) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  BLResult result = blPathEnsureInfo(selfI);

  *flagsOut = selfI->flags;
  return result;
}

// ============================================================================
// [BLPath - BoundingBox]
// ============================================================================

BLResult blPathGetControlBox(const BLPathCore* self, BLBox* boxOut) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  BLResult result = blPathEnsureInfo(selfI);

  *boxOut = selfI->controlBox;
  return result;
}

BLResult blPathGetBoundingBox(const BLPathCore* self, BLBox* boxOut) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  BLResult result = blPathEnsureInfo(selfI);

  *boxOut = selfI->boundingBox;
  return result;
}

// ============================================================================
// [BLPath - Subpath Range]
// ============================================================================

BLResult blPathGetFigureRange(const BLPathCore* self, size_t index, BLRange* rangeOut) noexcept {
  const BLInternalPathImpl* selfI = blInternalCast(self->impl);

  const uint8_t* cmdData = selfI->commandData;
  size_t size = selfI->size;

  if (index >= size) {
    rangeOut->reset(0, 0);
    return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  // Find end of the sub-path.
  size_t end = index + 1;
  while (end < size) {
    uint32_t cmd = cmdData[end];
    if (cmd == BL_PATH_CMD_MOVE)
      break;

    end++;
    if (cmd == BL_PATH_CMD_CLOSE)
      break;
  }

  // Find start of the sub-path.
  if (cmdData[index] != BL_PATH_CMD_MOVE) {
    while (index > 0) {
      uint32_t cmd = cmdData[index - 1];

      if (cmd == BL_PATH_CMD_CLOSE)
        break;

      index--;
      if (cmd == BL_PATH_CMD_MOVE)
        break;
    }
  }

  rangeOut->reset(index, end);
  return BL_SUCCESS;
}

// ============================================================================
// [BLPath - Vertex Queries]
// ============================================================================

BLResult blPathGetLastVertex(const BLPathCore* self, BLPoint* vtxOut) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  size_t index = selfI->size;

  vtxOut->reset();
  if (BL_UNLIKELY(!index))
    return blTraceError(BL_ERROR_NO_MATCHING_VERTEX);

  const uint8_t* cmdData = selfI->commandData;
  uint32_t cmd = cmdData[--index];

  if (cmd != BL_PATH_CMD_CLOSE) {
    *vtxOut = selfI->vertexData[index];
    return BL_SUCCESS;
  }

  for (;;) {
    if (index == 0)
      return blTraceError(BL_ERROR_NO_MATCHING_VERTEX);

    cmd = cmdData[--index];
    if (cmd == BL_PATH_CMD_CLOSE)
      return blTraceError(BL_ERROR_NO_MATCHING_VERTEX);

    if (cmd == BL_PATH_CMD_MOVE)
      break;
  }

  *vtxOut = selfI->vertexData[index];
  return BL_SUCCESS;
}

BLResult blPathGetClosestVertex(const BLPathCore* self, const BLPoint* p, double maxDistance, size_t* indexOut, double* distanceOut) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  size_t size = selfI->size;

  *indexOut = SIZE_MAX;
  *distanceOut = blNaN<double>();

  if (BL_UNLIKELY(!size))
    return blTraceError(BL_ERROR_NO_MATCHING_VERTEX);

  const uint8_t* cmdData = selfI->commandData;
  const BLPoint* vtxData = selfI->vertexData;

  size_t bestIndex = SIZE_MAX;
  double bestDistance = blInf<double>();
  double bestDistanceSq = blInf<double>();

  BLPoint pt(*p);
  bool hasMaxDistance = maxDistance > 0.0 && maxDistance < blInf<double>();

  if (hasMaxDistance) {
    bestDistance = maxDistance;
    bestDistanceSq = blSquare(bestDistance);

    // This code-path can be used to skip the whole path if the given point is
    // too far. We need 'maxDistance' to be specified and also bounding-box to
    // be available.
    if (blPathEnsureInfo(selfI) != BL_SUCCESS) {
      // If the given point is outside of the path bounding-box extended by
      // `maxDistance` then there is no matching vertex to possibly return.
      const BLBox& bBox = selfI->controlBox;
      if (!(pt.x >= bBox.x0 - bestDistance && pt.y >= bBox.y0 - bestDistance &&
            pt.x <= bBox.x1 + bestDistance && pt.y <= bBox.y1 + bestDistance))
        return blTraceError(BL_ERROR_NO_MATCHING_VERTEX);
    }
  }

  for (size_t i = 0; i < size; i++) {
    if (cmdData[i] != BL_PATH_CMD_CLOSE) {
      double d = blSquare(vtxData[i].x - pt.x) +
                 blSquare(vtxData[i].y - pt.y);

      if (d < bestDistanceSq) {
        bestIndex = i;
        bestDistanceSq = d;
      }
    }
  }

  if (bestIndex == SIZE_MAX)
    bestDistance = blNaN<double>();
  else
    bestDistance = blSqrt(bestDistanceSq);

  *indexOut = bestIndex;
  *distanceOut = bestDistance;

  return BL_SUCCESS;;
}

// ============================================================================
// [BLPath - Hit Test]
// ============================================================================

uint32_t blPathHitTest(const BLPathCore* self, const BLPoint* p, uint32_t fillRule) noexcept {
  BLInternalPathImpl* selfI = blInternalCast(self->impl);
  size_t i = selfI->size;

  if (!i)
    return BL_HIT_TEST_OUT;

  const uint8_t* cmdData = selfI->commandData;
  const BLPoint* vtxData = selfI->vertexData;

  BLPoint start;
  bool hasMoveTo = false;

  BLPoint pt(*p);

  double x0, y0;
  double x1, y1;

  intptr_t windingNumber = 0;
  BLPoint ptBuffer[8];

  do {
    switch (cmdData[0]) {
      case BL_PATH_CMD_MOVE: {
        if (hasMoveTo) {
          x0 = vtxData[-1].x;
          y0 = vtxData[-1].y;
          x1 = start.x;
          y1 = start.y;

          hasMoveTo = false;
          goto OnLine;
        }

        start = vtxData[0];

        cmdData++;
        vtxData++;
        i--;

        hasMoveTo = true;
        break;
      }

      case BL_PATH_CMD_ON: {
        if (BL_UNLIKELY(!hasMoveTo))
          return BL_HIT_TEST_INVALID;

        x0 = vtxData[-1].x;
        y0 = vtxData[-1].y;
        x1 = vtxData[0].x;
        y1 = vtxData[0].y;

        cmdData++;
        vtxData++;
        i--;

OnLine:
        {
          double dx = x1 - x0;
          double dy = y1 - y0;

          if (dy > 0.0) {
            if (pt.y >= y0 && pt.y < y1) {
              double ix = x0 + (pt.y - y0) * dx / dy;
              windingNumber += (pt.x >= ix);
            }
          }
          else if (dy < 0.0) {
            if (pt.y >= y1 && pt.y < y0) {
              double ix = x0 + (pt.y - y0) * dx / dy;
              windingNumber -= (pt.x >= ix);
            }
          }
        }
        break;
      }

      case BL_PATH_CMD_QUAD: {
        BL_ASSERT(hasMoveTo);
        BL_ASSERT(i >= 2);

        const BLPoint* p = vtxData - 1;
        if (BL_UNLIKELY(!hasMoveTo))
          return BL_HIT_TEST_INVALID;

        double minY = blMin(p[0].y, p[1].y, p[2].y);
        double maxY = blMax(p[0].y, p[1].y, p[2].y);

        cmdData += 2;
        vtxData += 2;
        i -= 2;

        if (pt.y >= minY && pt.y <= maxY) {
          bool degenerate = isNear(p[0].y, p[1].y) && isNear(p[1].y, p[2].y);

          if (degenerate) {
            x0 = p[0].x;
            y0 = p[0].y;
            x1 = p[2].x;
            y1 = p[2].y;
            goto OnLine;
          }

          // Subdivide curve to curve-spline separated at Y-extrama.
          BLPoint* left = (BLPoint*)ptBuffer;
          BLPoint* rght = (BLPoint*)ptBuffer + 3;

          double tArray[2];
          tArray[0] = (p[0].y - p[1].y) / (p[0].y - 2.0 * p[1].y + p[2].y);

          size_t tLength = tArray[0] > 0.0 && tArray[0] < 1.0;
          tArray[tLength++] = 1.0;

          rght[0] = p[0];
          rght[1] = p[1];
          rght[2] = p[2];

          double tCut = 0.0;
          for (size_t tIndex = 0; tIndex < tLength; tIndex++) {
            double tVal = tArray[tIndex];
            if (tVal == tCut) continue;

            if (tVal == 1.0) {
              left[0] = rght[0];
              left[1] = rght[1];
              left[2] = rght[2];
            }
            else {
              blSplitQuad(rght, left, rght, tCut == 0.0 ? tVal : (tVal - tCut) / (1.0 - tCut));
            }

            minY = blMin(left[0].y, left[2].y);
            maxY = blMax(left[0].y, left[2].y);

            if (pt.y >= minY && pt.y < maxY) {
              int dir = 0;
              if (left[0].y < left[2].y)
                dir = 1;
              else if (left[0].y > left[2].y)
                dir = -1;

              // It should be only possible to have none or one solution.
              double ti[2];
              double ix;

              BLPoint a, b, c;
              blGetQuadCoefficients(left, a, b, c);

              // { At^2 + Bt + C } -> { t(At + B) + C }
              if (blQuadRoots(ti, a.y, b.y, c.y - pt.y, BL_MATH_AFTER_0, BL_MATH_BEFORE_1) >= 1)
                ix = ti[0] * (a.x * ti[0] + b.x) + c.x;
              else if (pt.y - minY < maxY - pt.y)
                ix = p[0].x;
              else
                ix = p[2].x;

              if (pt.x >= ix)
                windingNumber += dir;
            }

            tCut = tVal;
          }
        }
        break;
      }

      case BL_PATH_CMD_CUBIC: {
        BL_ASSERT(hasMoveTo);
        BL_ASSERT(i >= 3);

        const BLPoint* p = vtxData - 1;
        if (BL_UNLIKELY(!hasMoveTo))
          return BL_HIT_TEST_INVALID;

        double minY = blMin(p[0].y, p[1].y, p[2].y, p[3].y);
        double maxY = blMax(p[0].y, p[1].y, p[2].y, p[3].y);

        cmdData += 3;
        vtxData += 3;
        i -= 3;

        if (pt.y >= minY && pt.y <= maxY) {
          bool degenerate = isNear(p[0].y, p[1].y) &&
                            isNear(p[1].y, p[2].y) &&
                            isNear(p[2].y, p[3].y) ;

          if (degenerate) {
            x0 = p[0].x;
            y0 = p[0].y;
            x1 = p[3].x;
            y1 = p[3].y;
            goto OnLine;
          }

          // Subdivide curve to curve-spline separated at Y-extrama.
          BLPoint* left = (BLPoint*)ptBuffer;
          BLPoint* rght = (BLPoint*)ptBuffer + 4;

          double tArray[3];
          size_t tLength = blQuadRoots(
            tArray,
            3.0 * (-p[0].y + 3.0 * (p[1].y - p[2].y) + p[3].y),
            6.0 * ( p[0].y - 2.0 * (p[1].y + p[2].y)         ),
            3.0 * (-p[0].y +       (p[1].y         )         ),
            BL_MATH_AFTER_0,
            BL_MATH_BEFORE_1);
          tArray[tLength++] = 1.0;

          rght[0] = p[0];
          rght[1] = p[1];
          rght[2] = p[2];
          rght[3] = p[3];

          double tCut = 0.0;
          for (size_t tIndex = 0; tIndex < tLength; tIndex++) {
            double tVal = tArray[tIndex];
            if (tVal == tCut) continue;

            if (tVal == 1.0) {
              left[0] = rght[0];
              left[1] = rght[1];
              left[2] = rght[2];
              left[3] = rght[3];
            }
            else {
              blSplitCubic(rght, rght, left, tCut == 0.0 ? tVal : (tVal - tCut) / (1.0 - tCut));
            }

            minY = blMin(left[0].y, left[3].y);
            maxY = blMax(left[0].y, left[3].y);

            if (pt.y >= minY && pt.y < maxY) {
              int dir = 0;
              if (left[0].y < left[3].y)
                dir = 1;
              else if (left[0].y > left[3].y)
                dir = -1;

              // It should be only possible to have zero/one solution.
              double ti[3];
              double ix;

              BLPoint a, b, c, d;
              blGetCubicCoefficients(left, a, b, c, d);

              // { At^3 + Bt^2 + Ct + D } -> { ((At + B)t + C)t + D }
              if (blCubicRoots(ti, a.y, b.y, c.y, d.y - pt.y, BL_MATH_AFTER_0, BL_MATH_BEFORE_1) >= 1)
                ix = ((a.x * ti[0] + b.x) * ti[0] + c.x) * ti[0] + d.x;
              else if (pt.y - minY < maxY - pt.y)
                ix = p[0].x;
              else
                ix = p[3].x;

              if (pt.x >= ix)
                windingNumber += dir;
            }

            tCut = tVal;
          }
        }
        break;
      }

      case BL_PATH_CMD_CLOSE: {
        if (hasMoveTo) {
          x0 = vtxData[-1].x;
          y0 = vtxData[-1].y;
          x1 = start.x;
          y1 = start.y;

          hasMoveTo = false;
          goto OnLine;
        }

        cmdData++;
        vtxData++;

        i--;
        break;
      }

      default:
        return BL_HIT_TEST_INVALID;
    }
  } while (i);

  // Close the path.
  if (hasMoveTo) {
    x0 = vtxData[-1].x;
    y0 = vtxData[-1].y;
    x1 = start.x;
    y1 = start.y;

    hasMoveTo = false;
    goto OnLine;
  }

  if (fillRule == BL_FILL_RULE_EVEN_ODD)
    windingNumber &= 1;
  return windingNumber != 0;
}

// ============================================================================
// [BLPath - Runtime Init]
// ============================================================================

void blPathRtInit(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);

  BLInternalPathImpl* pathI = &blNullPathImpl;
  pathI->implType = uint8_t(BL_IMPL_TYPE_PATH2D);
  pathI->implTraits = uint8_t(BL_IMPL_TRAIT_NULL);
  pathI->flags = BL_PATH_FLAG_EMPTY;
  blAssignBuiltInNull(pathI);
}
