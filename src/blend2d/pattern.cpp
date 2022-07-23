// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "image_p.h"
#include "matrix_p.h"
#include "object_p.h"
#include "pattern_p.h"
#include "runtime_p.h"

namespace BLPatternPrivate {

// BLPattern - Globals
// ===================

static BLObjectEthernalImpl<BLPatternPrivateImpl> defaultImpl;

static constexpr const BLRectI blPatternNoArea(0, 0, 0, 0);

// BLPattern - Internals
// =====================

static BL_INLINE BLResult blPatternImplAlloc(
  BLPatternCore* self,
  const BLImageCore* image,
  const BLRectI* area,
  BLExtendMode extendMode,
  BLMatrix2DType matrixType,
  const BLMatrix2D* matrix) noexcept {

  BLPatternPrivateImpl* impl = blObjectDetailAllocImplT<BLPatternPrivateImpl>(self, BLObjectInfo::packType(BL_OBJECT_TYPE_PATTERN));
  if (BL_UNLIKELY(!impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  setExtendMode(self->_d.info, extendMode);
  setMatrixType(self->_d.info, matrixType);

  blCallCtor(impl->image.dcast(), image->dcast());
  impl->matrix = *matrix;
  impl->area = *area;

  return BL_SUCCESS;
}

BLResult freeImpl(BLPatternPrivateImpl* impl, BLObjectInfo info) noexcept {
  blImageReset(&impl->image);

  return blObjectDetailFreeImpl(impl, info.bits);
}

static BL_INLINE bool blPatternPrivateIsMutable(const BLPatternCore* self) noexcept {
  const size_t* refCountPtr = blObjectImplGetRefCountPtr(self->_d.impl);
  return *refCountPtr == 1;
}

static BL_INLINE BLResult blPatternPrivateRelease(BLPatternCore* self) noexcept {
  BLPatternPrivateImpl* impl = getImpl(self);
  BLObjectInfo info = self->_d.info;

  if (info.refCountedFlag() && blObjectImplDecRefAndTest(impl, info))
    return freeImpl(impl, info);

  return BL_SUCCESS;
}

static BL_INLINE BLResult blPatternPrivateReplace(BLPatternCore* self, const BLPatternCore* other) noexcept {
  BLPatternPrivateImpl* impl = getImpl(self);
  BLObjectInfo info = self->_d.info;

  self->_d = other->_d;

  if (info.refCountedFlag() && blObjectImplDecRefAndTest(impl, info))
    return freeImpl(impl, info);

  return BL_SUCCESS;
}

static BL_NOINLINE BLResult blPatternMakeMutableCopyOf(BLPatternCore* self, const BLPatternCore* other) noexcept {
  BLPatternPrivateImpl* otherI = getImpl(other);

  BLPatternCore newO;
  BL_PROPAGATE(blPatternImplAlloc(
    &newO,
    &otherI->image,
    &otherI->area,
    getExtendMode(self),
    getMatrixType(self),
    &otherI->matrix));

  return blPatternPrivateReplace(self, &newO);
}

static BL_INLINE BLResult blPatternMakeMutable(BLPatternCore* self) noexcept {
  if (!blPatternPrivateIsMutable(self))
    return blPatternMakeMutableCopyOf(self, self);
  else
    return BL_SUCCESS;
}

} // {BLPatternPrivate}

// BLPattern - API - Init & Destroy
// ================================

BL_API_IMPL BLResult blPatternInit(BLPatternCore* self) noexcept {
  using namespace BLPatternPrivate;

  self->_d = blObjectDefaults[BL_OBJECT_TYPE_PATTERN]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blPatternInitMove(BLPatternCore* self, BLPatternCore* other) noexcept {
  using namespace BLPatternPrivate;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isPattern());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_PATTERN]._d;

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blPatternInitWeak(BLPatternCore* self, const BLPatternCore* other) noexcept {
  using namespace BLPatternPrivate;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isPattern());

  return blObjectPrivateInitWeakTagged(self, other);
}

BL_API_IMPL BLResult blPatternInitAs(BLPatternCore* self, const BLImageCore* image, const BLRectI* area, BLExtendMode extendMode, const BLMatrix2D* matrix) noexcept {
  using namespace BLPatternPrivate;

  self->_d = blObjectDefaults[BL_OBJECT_TYPE_PATTERN]._d;

  if (!image)
    image = static_cast<BLImageCore*>(&blObjectDefaults[BL_OBJECT_TYPE_IMAGE]);

  if (!area)
    area = &blPatternNoArea;
  else if (BL_UNLIKELY(!isAreaValid(*area, image->dcast().size())))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (BL_UNLIKELY(extendMode > BL_EXTEND_MODE_COMPLEX_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLMatrix2DType matrixType = BL_MATRIX2D_TYPE_IDENTITY;
  if (!matrix)
    matrix = &BLTransformPrivate::identityTransform;
  else
    matrixType = matrix->type();

  return blPatternImplAlloc(self, image, area, extendMode, matrixType, matrix);
}

BL_API_IMPL BLResult blPatternDestroy(BLPatternCore* self) noexcept {
  using namespace BLPatternPrivate;
  BL_ASSERT(self->_d.isPattern());

  return blPatternPrivateRelease(self);
}

// BLPattern - API - Reset
// =======================

BL_API_IMPL BLResult blPatternReset(BLPatternCore* self) noexcept {
  using namespace BLPatternPrivate;
  BL_ASSERT(self->_d.isPattern());

  return blPatternPrivateReplace(self, static_cast<BLPatternCore*>(&blObjectDefaults[BL_OBJECT_TYPE_PATTERN]));
}

// BLPattern - API - Assign
// ========================

BL_API_IMPL BLResult blPatternAssignMove(BLPatternCore* self, BLPatternCore* other) noexcept {
  using namespace BLPatternPrivate;

  BL_ASSERT(self->_d.isPattern());
  BL_ASSERT(other->_d.isPattern());

  BLPatternCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_PATTERN]._d;
  return blPatternPrivateReplace(self, &tmp);
}

BL_API_IMPL BLResult blPatternAssignWeak(BLPatternCore* self, const BLPatternCore* other) noexcept {
  using namespace BLPatternPrivate;

  BL_ASSERT(self->_d.isPattern());
  BL_ASSERT(other->_d.isPattern());

  blObjectPrivateAddRefTagged(other);
  return blPatternPrivateReplace(self, other);
}

BL_API_IMPL BLResult blPatternAssignDeep(BLPatternCore* self, const BLPatternCore* other) noexcept {
  using namespace BLPatternPrivate;

  BL_ASSERT(self->_d.isPattern());
  BL_ASSERT(other->_d.isPattern());

  if (!blPatternPrivateIsMutable(self))
    return blPatternMakeMutableCopyOf(self, other);

  BLPatternPrivateImpl* selfI = getImpl(self);
  BLPatternPrivateImpl* otherI = getImpl(other);

  self->_d.info.setBField(other->_d.info.bField());
  self->_d.info.setCField(other->_d.info.cField());
  selfI->matrix = otherI->matrix;
  selfI->area = otherI->area;
  return blImageAssignWeak(&selfI->image, &otherI->image);
}

// BLPattern - API - Create
// ========================

BL_API_IMPL BLResult blPatternCreate(BLPatternCore* self, const BLImageCore* image, const BLRectI* area, BLExtendMode extendMode, const BLMatrix2D* matrix) noexcept {
  using namespace BLPatternPrivate;
  BL_ASSERT(self->_d.isPattern());

  if (!image)
    image = static_cast<BLImageCore*>(&blObjectDefaults[BL_OBJECT_TYPE_IMAGE]);

  if (!area)
    area = &blPatternNoArea;
  else if (BL_UNLIKELY(!isAreaValid(*area, image->dcast().size())))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (BL_UNLIKELY(extendMode > BL_EXTEND_MODE_COMPLEX_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLMatrix2DType matrixType = BL_MATRIX2D_TYPE_IDENTITY;
  if (!matrix)
    matrix = &BLTransformPrivate::identityTransform;
  else
    matrixType = matrix->type();

  if (!blPatternPrivateIsMutable(self)) {
    BLPatternCore newO;
    BL_PROPAGATE(blPatternImplAlloc(&newO, image, area, extendMode, matrixType, matrix));

    return blPatternPrivateReplace(self, &newO);
  }
  else {
    BLPatternPrivateImpl* selfI = getImpl(self);

    setExtendMode(self->_d.info, extendMode);
    setMatrixType(self->_d.info, matrixType);
    selfI->area = *area;
    selfI->matrix = *matrix;

    return blImageAssignWeak(&selfI->image, image);
  }
}

// BLPattern - API - Image & Area
// ==============================

BL_API_IMPL BLResult blPatternGetImage(const BLPatternCore* self, BLImageCore* image) noexcept {
  using namespace BLPatternPrivate;
  BL_ASSERT(self->_d.isPattern());

  BLPatternPrivateImpl* selfI = getImpl(self);
  return blImageAssignWeak(image, &selfI->image);
}

BL_API_IMPL BLResult blPatternSetImage(BLPatternCore* self, const BLImageCore* image, const BLRectI* area) noexcept {
  using namespace BLPatternPrivate;
  BL_ASSERT(self->_d.isPattern());

  if (!image)
    image = static_cast<BLImageCore*>(&blObjectDefaults[BL_OBJECT_TYPE_IMAGE]);

  if (!area)
    area = &blPatternNoArea;
  else if (!isAreaValid(*area, image->dcast().size()))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BL_PROPAGATE(blPatternMakeMutable(self));
  BLPatternPrivateImpl* selfI = getImpl(self);

  selfI->area = *area;
  return blImageAssignWeak(&selfI->image, image);
}

BL_API_IMPL BLResult blPatternResetImage(BLPatternCore* self) noexcept {
  using namespace BLPatternPrivate;
  BL_ASSERT(self->_d.isPattern());

  return blPatternSetImage(self, nullptr, nullptr);
}

BL_API_IMPL BLResult blPatternGetArea(const BLPatternCore* self, BLRectI* areaOut) noexcept {
  using namespace BLPatternPrivate;
  BL_ASSERT(self->_d.isPattern());

  BLPatternPrivateImpl* selfI = getImpl(self);
  *areaOut = selfI->area;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blPatternSetArea(BLPatternCore* self, const BLRectI* area) noexcept {
  using namespace BLPatternPrivate;
  BL_ASSERT(self->_d.isPattern());

  if (!area) {
    area = &blPatternNoArea;
  }
  else {
    BLPatternPrivateImpl* selfI = getImpl(self);
    BLImageImpl* imageI = BLImagePrivate::getImpl(&selfI->image);

    if (!isAreaValid(*area, imageI->size))
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  BL_PROPAGATE(blPatternMakeMutable(self));
  BLPatternPrivateImpl* selfI = getImpl(self);

  selfI->area = *area;
  return BL_SUCCESS;
}

// BLPattern - API - Extend Mode
// =============================

BL_API_IMPL BLExtendMode blPatternGetExtendMode(const BLPatternCore* self) noexcept {
  using namespace BLPatternPrivate;
  BL_ASSERT(self->_d.isPattern());

  return getExtendMode(self);
}

BL_API_IMPL BLResult blPatternSetExtendMode(BLPatternCore* self, BLExtendMode extendMode) noexcept {
  using namespace BLPatternPrivate;
  BL_ASSERT(self->_d.isPattern());

  if (BL_UNLIKELY(extendMode > BL_EXTEND_MODE_COMPLEX_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  setExtendMode(self->_d.info, extendMode);
  return BL_SUCCESS;
}

// BLPattern - API - Matrix
// ========================

BL_API_IMPL BLMatrix2DType blPatternGetMatrixType(const BLPatternCore* self) noexcept {
  using namespace BLPatternPrivate;
  BL_ASSERT(self->_d.isPattern());

  return getMatrixType(self);
}

BL_API_IMPL BLResult blPatternGetMatrix(const BLPatternCore* self, BLMatrix2D* matrixOut) noexcept {
  using namespace BLPatternPrivate;
  BL_ASSERT(self->_d.isPattern());

  if (getMatrixType(self) == BL_MATRIX2D_TYPE_IDENTITY) {
    matrixOut->reset();
  }
  else {
    BLPatternPrivateImpl* selfI = getImpl(self);
    *matrixOut = selfI->matrix;
  }

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blPatternApplyMatrixOp(BLPatternCore* self, BLMatrix2DOp opType, const void* opData) noexcept {
  using namespace BLPatternPrivate;
  BL_ASSERT(self->_d.isPattern());

  if (BL_UNLIKELY(uint32_t(opType) > BL_MATRIX2D_OP_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (opType == BL_MATRIX2D_OP_RESET && getMatrixType(self) == BL_MATRIX2D_TYPE_IDENTITY)
    return BL_SUCCESS;

  BL_PROPAGATE(blPatternMakeMutable(self));
  BLPatternPrivateImpl* selfI = getImpl(self);

  blMatrix2DApplyOp(&selfI->matrix, opType, opData);
  setMatrixType(self->_d.info, selfI->matrix.type());

  return BL_SUCCESS;
}

// BLPattern - API - Equality & Comparison
// =======================================

BL_API_IMPL bool blPatternEquals(const BLPatternCore* a, const BLPatternCore* b) noexcept {
  using namespace BLPatternPrivate;

  BL_ASSERT(a->_d.isPattern());
  BL_ASSERT(b->_d.isPattern());

  unsigned eq = unsigned(getExtendMode(a) == getExtendMode(b)) &
                unsigned(getMatrixType(a) == getMatrixType(b)) ;

  if (!eq)
    return false;

  const BLPatternPrivateImpl* aI = getImpl(a);
  const BLPatternPrivateImpl* bI = getImpl(b);

  if (aI == bI)
    return true;

  if (!(unsigned(aI->matrix == bI->matrix) & unsigned(aI->area == bI->area)))
    return false;

  return aI->image.dcast() == bI->image.dcast();
}

// BLPattern - Runtime Registration
// ================================

void blPatternRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  blCallCtor(BLPatternPrivate::defaultImpl.impl->image.dcast());
  BLPatternPrivate::defaultImpl.impl->matrix.reset();

  blObjectDefaults[BL_OBJECT_TYPE_PATTERN]._d.initDynamic(
    BL_OBJECT_TYPE_PATTERN,
    BLObjectInfo::packFields(0u, BL_EXTEND_MODE_REPEAT),
    &BLPatternPrivate::defaultImpl.impl);
}
