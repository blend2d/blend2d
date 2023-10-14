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

namespace bl {
namespace PatternInternal {

// bl::Pattern - Globals
// =====================

static BLObjectEternalImpl<BLPatternPrivateImpl> defaultImpl;

// bl::Pattern - Internals
// =======================

static BL_INLINE BLResult allocImpl(BLPatternCore* self, const BLImageCore* image, const BLRectI& area, BLExtendMode extendMode, const BLMatrix2D* transform, BLTransformType transformType) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_PATTERN);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLPatternPrivateImpl>(self, info));

  setExtendMode(self, extendMode);
  setTransformType(self, transformType);

  BLPatternPrivateImpl* impl = getImpl(self);
  blCallCtor(impl->image.dcast(), image->dcast());
  impl->transform = *transform;
  impl->area = area;

  return BL_SUCCESS;
}

BLResult freeImpl(BLPatternPrivateImpl* impl) noexcept {
  blImageDestroy(&impl->image);
  return ObjectInternal::freeImpl(impl);
}

static BL_NOINLINE BLResult makeMutableCopyOf(BLPatternCore* self, const BLPatternCore* other) noexcept {
  BLPatternPrivateImpl* otherI = getImpl(other);

  BLPatternCore newO;
  BL_PROPAGATE(allocImpl(&newO, &otherI->image, otherI->area, getExtendMode(self), &otherI->transform, getTransformType(self)));

  return replaceInstance(self, &newO);
}

static BL_INLINE BLResult makeMutable(BLPatternCore* self) noexcept {
  if (!isImplMutable(getImpl(self)))
    return makeMutableCopyOf(self, self);
  else
    return BL_SUCCESS;
}

} // {PatternInternal}
} // {bl}

// bl::Pattern - API - Init & Destroy
// ==================================

BL_API_IMPL BLResult blPatternInit(BLPatternCore* self) noexcept {
  using namespace bl::PatternInternal;

  self->_d = blObjectDefaults[BL_OBJECT_TYPE_PATTERN]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blPatternInitMove(BLPatternCore* self, BLPatternCore* other) noexcept {
  using namespace bl::PatternInternal;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isPattern());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_PATTERN]._d;

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blPatternInitWeak(BLPatternCore* self, const BLPatternCore* other) noexcept {
  using namespace bl::PatternInternal;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isPattern());

  self->_d = other->_d;
  return retainInstance(self);
}

BL_API_IMPL BLResult blPatternInitAs(BLPatternCore* self, const BLImageCore* image, const BLRectI* area, BLExtendMode extendMode, const BLMatrix2D* transform) noexcept {
  using namespace bl::PatternInternal;

  self->_d = blObjectDefaults[BL_OBJECT_TYPE_PATTERN]._d;

  if (!image)
    image = static_cast<BLImageCore*>(&blObjectDefaults[BL_OBJECT_TYPE_IMAGE]);

  BLImageImpl* imageI = bl::ImageInternal::getImpl(image);
  BLRectI imageArea(0, 0, imageI->size.w, imageI->size.h);

  if (BL_UNLIKELY(extendMode > BL_EXTEND_MODE_COMPLEX_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (!area)
    area = &imageArea;
  else if (*area != imageArea && !isAreaValid(*area, imageI->size))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLTransformType transformType = BL_TRANSFORM_TYPE_IDENTITY;
  if (!transform)
    transform = &bl::TransformInternal::identityTransform;
  else
    transformType = transform->type();

  return allocImpl(self, image, *area, extendMode, transform, transformType);
}

BL_API_IMPL BLResult blPatternDestroy(BLPatternCore* self) noexcept {
  using namespace bl::PatternInternal;
  BL_ASSERT(self->_d.isPattern());

  return releaseInstance(self);
}

// bl::Pattern - API - Reset
// =========================

BL_API_IMPL BLResult blPatternReset(BLPatternCore* self) noexcept {
  using namespace bl::PatternInternal;
  BL_ASSERT(self->_d.isPattern());

  return replaceInstance(self, static_cast<BLPatternCore*>(&blObjectDefaults[BL_OBJECT_TYPE_PATTERN]));
}

// bl::Pattern - API - Assign
// ==========================

BL_API_IMPL BLResult blPatternAssignMove(BLPatternCore* self, BLPatternCore* other) noexcept {
  using namespace bl::PatternInternal;

  BL_ASSERT(self->_d.isPattern());
  BL_ASSERT(other->_d.isPattern());

  BLPatternCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_PATTERN]._d;
  return replaceInstance(self, &tmp);
}

BL_API_IMPL BLResult blPatternAssignWeak(BLPatternCore* self, const BLPatternCore* other) noexcept {
  using namespace bl::PatternInternal;

  BL_ASSERT(self->_d.isPattern());
  BL_ASSERT(other->_d.isPattern());

  retainInstance(other);
  return replaceInstance(self, other);
}

BL_API_IMPL BLResult blPatternAssignDeep(BLPatternCore* self, const BLPatternCore* other) noexcept {
  using namespace bl::PatternInternal;

  BL_ASSERT(self->_d.isPattern());
  BL_ASSERT(other->_d.isPattern());

  if (!isInstanceMutable(self))
    return makeMutableCopyOf(self, other);

  BLPatternPrivateImpl* selfI = getImpl(self);
  BLPatternPrivateImpl* otherI = getImpl(other);

  self->_d.info.setBField(other->_d.info.bField());
  self->_d.info.setCField(other->_d.info.cField());
  selfI->transform = otherI->transform;
  selfI->area = otherI->area;
  return blImageAssignWeak(&selfI->image, &otherI->image);
}

// bl::Pattern - API - Create
// ==========================

BL_API_IMPL BLResult blPatternCreate(BLPatternCore* self, const BLImageCore* image, const BLRectI* area, BLExtendMode extendMode, const BLMatrix2D* transform) noexcept {
  using namespace bl::PatternInternal;
  BL_ASSERT(self->_d.isPattern());

  if (!image)
    image = static_cast<BLImageCore*>(&blObjectDefaults[BL_OBJECT_TYPE_IMAGE]);

  BLImageImpl* imageI = bl::ImageInternal::getImpl(image);
  BLRectI imageArea(0, 0, imageI->size.w, imageI->size.h);

  if (BL_UNLIKELY(extendMode > BL_EXTEND_MODE_COMPLEX_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (!area)
    area = &imageArea;
  else if (*area != imageArea && !isAreaValid(*area, imageI->size))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLTransformType transformType = BL_TRANSFORM_TYPE_IDENTITY;
  if (!transform)
    transform = &bl::TransformInternal::identityTransform;
  else
    transformType = transform->type();

  if (!isInstanceMutable(self)) {
    BLPatternCore newO;
    BL_PROPAGATE(allocImpl(&newO, image, *area, extendMode, transform, transformType));

    return replaceInstance(self, &newO);
  }
  else {
    BLPatternPrivateImpl* selfI = getImpl(self);

    setExtendMode(self, extendMode);
    setTransformType(self, transformType);
    selfI->area = *area;
    selfI->transform = *transform;

    return blImageAssignWeak(&selfI->image, image);
  }
}

// bl::Pattern - API - Image & Area
// ================================

BL_API_IMPL BLResult blPatternGetImage(const BLPatternCore* self, BLImageCore* image) noexcept {
  using namespace bl::PatternInternal;
  BL_ASSERT(self->_d.isPattern());

  BLPatternPrivateImpl* selfI = getImpl(self);
  return blImageAssignWeak(image, &selfI->image);
}

BL_API_IMPL BLResult blPatternSetImage(BLPatternCore* self, const BLImageCore* image, const BLRectI* area) noexcept {
  using namespace bl::PatternInternal;
  BL_ASSERT(self->_d.isPattern());

  if (!image)
    image = static_cast<BLImageCore*>(&blObjectDefaults[BL_OBJECT_TYPE_IMAGE]);

  BLImageImpl* imageI = bl::ImageInternal::getImpl(image);
  BLRectI imageArea(0, 0, imageI->size.w, imageI->size.h);

  if (!area)
    area = &imageArea;
  else if (*area != imageArea && !isAreaValid(*area, image->dcast().size()))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BL_PROPAGATE(makeMutable(self));
  BLPatternPrivateImpl* selfI = getImpl(self);

  selfI->area = *area;
  return blImageAssignWeak(&selfI->image, image);
}

BL_API_IMPL BLResult blPatternResetImage(BLPatternCore* self) noexcept {
  using namespace bl::PatternInternal;
  BL_ASSERT(self->_d.isPattern());

  return blPatternSetImage(self, nullptr, nullptr);
}

BL_API_IMPL BLResult blPatternGetArea(const BLPatternCore* self, BLRectI* areaOut) noexcept {
  using namespace bl::PatternInternal;
  BL_ASSERT(self->_d.isPattern());

  BLPatternPrivateImpl* selfI = getImpl(self);
  *areaOut = selfI->area;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blPatternSetArea(BLPatternCore* self, const BLRectI* area) noexcept {
  using namespace bl::PatternInternal;
  BL_ASSERT(self->_d.isPattern());

  BLPatternPrivateImpl* selfI = getImpl(self);
  BLImageImpl* imageI = bl::ImageInternal::getImpl(&selfI->image);

  if (BL_UNLIKELY(!isAreaValid(*area, imageI->size)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BL_PROPAGATE(makeMutable(self));
  selfI = getImpl(self);
  selfI->area = *area;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blPatternResetArea(BLPatternCore* self) noexcept {
  using namespace bl::PatternInternal;
  BL_ASSERT(self->_d.isPattern());

  BLPatternPrivateImpl* selfI = getImpl(self);
  BLSizeI size = bl::ImageInternal::getImpl(&selfI->image)->size;

  if (selfI->area == BLRectI(0, 0, size.w, size.h))
    return BL_SUCCESS;

  BL_PROPAGATE(makeMutable(self));
  selfI = getImpl(self);
  selfI->area.reset(0, 0, size.w, size.h);
  return BL_SUCCESS;
}

// bl::Pattern - API - Extend Mode
// ===============================

BL_API_IMPL BLExtendMode blPatternGetExtendMode(const BLPatternCore* self) noexcept {
  using namespace bl::PatternInternal;
  BL_ASSERT(self->_d.isPattern());

  return getExtendMode(self);
}

BL_API_IMPL BLResult blPatternSetExtendMode(BLPatternCore* self, BLExtendMode extendMode) noexcept {
  using namespace bl::PatternInternal;
  BL_ASSERT(self->_d.isPattern());

  if (BL_UNLIKELY(extendMode > BL_EXTEND_MODE_COMPLEX_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  setExtendMode(self, extendMode);
  return BL_SUCCESS;
}

// bl::Pattern - API - Transform
// =============================

BL_API_IMPL BLResult blPatternGetTransform(const BLPatternCore* self, BLMatrix2D* transformOut) noexcept {
  using namespace bl::PatternInternal;
  BL_ASSERT(self->_d.isPattern());

  if (getTransformType(self) == BL_TRANSFORM_TYPE_IDENTITY) {
    transformOut->reset();
  }
  else {
    BLPatternPrivateImpl* selfI = getImpl(self);
    *transformOut = selfI->transform;
  }

  return BL_SUCCESS;
}

BL_API_IMPL BLTransformType blPatternGetTransformType(const BLPatternCore* self) noexcept {
  using namespace bl::PatternInternal;
  BL_ASSERT(self->_d.isPattern());

  return getTransformType(self);
}

BL_API_IMPL BLResult blPatternApplyTransformOp(BLPatternCore* self, BLTransformOp opType, const void* opData) noexcept {
  using namespace bl::PatternInternal;
  BL_ASSERT(self->_d.isPattern());

  if (BL_UNLIKELY(uint32_t(opType) > BL_TRANSFORM_OP_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (opType == BL_TRANSFORM_OP_RESET && getTransformType(self) == BL_TRANSFORM_TYPE_IDENTITY)
    return BL_SUCCESS;

  BL_PROPAGATE(makeMutable(self));
  BLPatternPrivateImpl* selfI = getImpl(self);

  blMatrix2DApplyOp(&selfI->transform, opType, opData);
  setTransformType(self, selfI->transform.type());

  return BL_SUCCESS;
}

// bl::Pattern - API - Equality & Comparison
// =========================================

BL_API_IMPL bool blPatternEquals(const BLPatternCore* a, const BLPatternCore* b) noexcept {
  using namespace bl::PatternInternal;

  BL_ASSERT(a->_d.isPattern());
  BL_ASSERT(b->_d.isPattern());

  unsigned eq = unsigned(getExtendMode(a) == getExtendMode(b)) &
                unsigned(getTransformType(a) == getTransformType(b)) ;

  if (!eq)
    return false;

  const BLPatternPrivateImpl* aI = getImpl(a);
  const BLPatternPrivateImpl* bI = getImpl(b);

  if (aI == bI)
    return true;

  if (!(unsigned(aI->transform == bI->transform) & unsigned(aI->area == bI->area)))
    return false;

  return aI->image.dcast() == bI->image.dcast();
}

// bl::Pattern - Runtime Registration
// ==================================

void blPatternRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  blCallCtor(bl::PatternInternal::defaultImpl.impl->image.dcast());
  bl::PatternInternal::defaultImpl.impl->transform.reset();

  blObjectDefaults[BL_OBJECT_TYPE_PATTERN]._d.initDynamic(
    BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_PATTERN) | BLObjectInfo::fromAbcp(0u, BL_EXTEND_MODE_REPEAT),
    &bl::PatternInternal::defaultImpl.impl);
}
