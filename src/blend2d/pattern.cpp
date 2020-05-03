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
#include "./matrix_p.h"
#include "./pattern_p.h"
#include "./runtime_p.h"
#include "./support_p.h"
#include "./variant.h"

// ============================================================================
// [Global Variables]
// ============================================================================

static BLWrap<BLInternalPatternImpl> blNullPatternImpl;

static constexpr const BLRectI blPatternNoArea(0, 0, 0, 0);

// ============================================================================
// [BLPattern - Internals]
// ============================================================================

static BL_INLINE BLInternalPatternImpl* blPatternImplNew(const BLImageCore* image, const BLRectI* area, uint32_t extendMode, uint32_t matrixType, const BLMatrix2D* matrix) noexcept {
  uint16_t memPoolData;
  BLInternalPatternImpl* impl = blRuntimeAllocImplT<BLInternalPatternImpl>(sizeof(BLInternalPatternImpl), &memPoolData);

  if (BL_UNLIKELY(!impl))
    return impl;

  blImplInit(impl, BL_IMPL_TYPE_PATTERN, BL_IMPL_TRAIT_MUTABLE, memPoolData);
  impl->image.impl = blImplIncRef(image->impl);
  impl->patternType = 0;
  impl->extendMode = uint8_t(extendMode);
  impl->matrixType = uint8_t(matrixType);
  impl->reserved[0] = 0;
  impl->matrix = *matrix;
  impl->area = *area;

  return impl;
}

// Cannot be static, called by `BLVariant` implementation.
BLResult blPatternImplDelete(BLPatternImpl* impl_) noexcept {
  BLInternalPatternImpl* impl = blInternalCast(impl_);
  blImageReset(&impl->image);

  uint8_t* implBase = reinterpret_cast<uint8_t*>(impl);
  size_t implSize = sizeof(BLInternalPatternImpl);
  uint32_t implTraits = impl->implTraits;
  uint32_t memPoolData = impl->memPoolData;

  if (implTraits & BL_IMPL_TRAIT_EXTERNAL) {
    implSize += sizeof(BLExternalImplPreface);
    implBase -= sizeof(BLExternalImplPreface);
    blImplDestroyExternal(impl);
  }

  if (implTraits & BL_IMPL_TRAIT_FOREIGN)
    return BL_SUCCESS;
  else
    return blRuntimeFreeImpl(implBase, implSize, memPoolData);
}

static BL_INLINE BLResult blPatternImplRelease(BLInternalPatternImpl* impl) noexcept {
  if (blImplDecRefAndTest(impl))
    return blPatternImplDelete(impl);
  return BL_SUCCESS;
}

static BL_NOINLINE BLResult blPatternMakeMutableCopyOf(BLPatternCore* self, BLInternalPatternImpl* impl) noexcept {
  BLInternalPatternImpl* newI = blPatternImplNew(&impl->image, &impl->area, impl->extendMode, impl->matrixType, &impl->matrix);
  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLInternalPatternImpl* oldI = blInternalCast(self->impl);
  self->impl = newI;
  return blPatternImplRelease(oldI);
}

static BL_INLINE BLResult blPatternMakeMutable(BLPatternCore* self) noexcept {
  BLInternalPatternImpl* selfI = blInternalCast(self->impl);
  if (!blImplIsMutable(selfI))
    return blPatternMakeMutableCopyOf(self, selfI);
  else
    return BL_SUCCESS;
}

// ============================================================================
// [BLPattern - Init / Destroy]
// ============================================================================

BLResult blPatternInit(BLPatternCore* self) noexcept {
  self->impl = &blNullPatternImpl;
  return BL_SUCCESS;
}

BLResult blPatternInitAs(BLPatternCore* self, const BLImageCore* image, const BLRectI* area, uint32_t extendMode, const BLMatrix2D* matrix) noexcept {
  if (!image)
    image = &BLImage::none();

  if (!area)
    area = &blPatternNoArea;
  else if (BL_UNLIKELY(!blPatternIsAreaValid(*area, image->impl->size)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (BL_UNLIKELY(extendMode >= BL_EXTEND_MODE_COMPLEX_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  uint32_t matrixType = BL_MATRIX2D_TYPE_IDENTITY;
  if (!matrix)
    matrix = &blMatrix2DIdentity;
  else
    matrixType = matrix->type();

  BLInternalPatternImpl* impl = blPatternImplNew(image, area, extendMode, matrixType, matrix);
  if (BL_UNLIKELY(!impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  self->impl = impl;
  return BL_SUCCESS;
}

BLResult blPatternDestroy(BLPatternCore* self) noexcept {
  BLInternalPatternImpl* selfI = blInternalCast(self->impl);
  self->impl = nullptr;
  return blPatternImplRelease(selfI);
}

// ============================================================================
// [BLPattern - Reset]
// ============================================================================

BLResult blPatternReset(BLPatternCore* self) noexcept {
  BLInternalPatternImpl* selfI = blInternalCast(self->impl);
  self->impl = &blNullPatternImpl;
  return blPatternImplRelease(selfI);
}

// ============================================================================
// [BLPattern - Assign / Create]
// ============================================================================

BLResult blPatternAssignMove(BLPatternCore* self, BLPatternCore* other) noexcept {
  BLInternalPatternImpl* selfI = blInternalCast(self->impl);
  BLInternalPatternImpl* otherI = blInternalCast(other->impl);

  self->impl = otherI;
  other->impl = &blNullPatternImpl;

  return blPatternImplRelease(selfI);
}

BLResult blPatternAssignWeak(BLPatternCore* self, const BLPatternCore* other) noexcept {
  BLInternalPatternImpl* selfI = blInternalCast(self->impl);
  BLInternalPatternImpl* otherI = blInternalCast(other->impl);

  self->impl = blImplIncRef(otherI);
  return blPatternImplRelease(selfI);
}

BLResult blPatternAssignDeep(BLPatternCore* self, const BLPatternCore* other) noexcept {
  BLInternalPatternImpl* selfI = blInternalCast(self->impl);
  BLInternalPatternImpl* otherI = blInternalCast(other->impl);

  if (!blImplIsMutable(selfI))
    return blPatternMakeMutableCopyOf(self, otherI);

  selfI->patternType = 0;
  selfI->extendMode = otherI->extendMode;
  selfI->matrixType = otherI->matrixType;
  selfI->matrix = otherI->matrix;
  selfI->area = otherI->area;
  return blImageAssignWeak(&selfI->image, &otherI->image);
}

BLResult blPatternCreate(BLPatternCore* self, const BLImageCore* image, const BLRectI* area, uint32_t extendMode, const BLMatrix2D* matrix) noexcept {
  if (!image)
    image = &BLImage::none();

  if (!area)
    area = &blPatternNoArea;
  else if (BL_UNLIKELY(!blPatternIsAreaValid(*area, image->impl->size)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (BL_UNLIKELY(extendMode >= BL_EXTEND_MODE_COMPLEX_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  uint32_t matrixType = BL_MATRIX2D_TYPE_IDENTITY;
  if (!matrix)
    matrix = &blMatrix2DIdentity;
  else
    matrixType = matrix->type();

  BLInternalPatternImpl* selfI = blInternalCast(self->impl);
  if (!blImplIsMutable(selfI)) {
    BLInternalPatternImpl* newI = blPatternImplNew(image, area, extendMode, matrixType, matrix);
    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    self->impl = newI;
    return blPatternImplRelease(selfI);
  }
  else {
    selfI->extendMode = uint8_t(extendMode);
    selfI->matrixType = uint8_t(matrixType);
    selfI->matrix = *matrix;
    selfI->area = *area;
    return blImageAssignWeak(&selfI->image, image);
  }
}

// ============================================================================
// [BLPattern - Properties]
// ============================================================================

BLResult blPatternSetImage(BLPatternCore* self, const BLImageCore* image, const BLRectI* area) noexcept {
  if (!image)
    image = &BLImage::none();

  if (!area)
    area = &blPatternNoArea;
  else if (!blPatternIsAreaValid(*area, image->impl->size))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BL_PROPAGATE(blPatternMakeMutable(self));
  BLInternalPatternImpl* selfI = blInternalCast(self->impl);

  selfI->area = *area;
  return blImageAssignWeak(&selfI->image, image);
}

BLResult blPatternSetArea(BLPatternCore* self, const BLRectI* area) noexcept {
  if (!area) {
    area = &blPatternNoArea;
  }
  else {
    BLImageImpl* imageImpl = self->impl->image.impl;
    if (!blPatternIsAreaValid(*area, imageImpl->size))
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  BL_PROPAGATE(blPatternMakeMutable(self));
  BLInternalPatternImpl* selfI = blInternalCast(self->impl);

  selfI->area = *area;
  return BL_SUCCESS;
}

BLResult blPatternSetExtendMode(BLPatternCore* self, uint32_t extendMode) noexcept {
  if (BL_UNLIKELY(extendMode >= BL_EXTEND_MODE_COMPLEX_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BL_PROPAGATE(blPatternMakeMutable(self));
  BLInternalPatternImpl* selfI = blInternalCast(self->impl);

  selfI->extendMode = uint8_t(extendMode);
  return BL_SUCCESS;
}

// ============================================================================
// [BLPattern - Matrix]
// ============================================================================

BLResult blPatternApplyMatrixOp(BLPatternCore* self, uint32_t opType, const void* opData) noexcept {
  if (BL_UNLIKELY(opType >= BL_MATRIX2D_OP_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLInternalPatternImpl* selfI = blInternalCast(self->impl);
  if (opType == 0 && selfI->matrixType == BL_MATRIX2D_TYPE_IDENTITY)
    return BL_SUCCESS;

  BL_PROPAGATE(blPatternMakeMutable(self));
  selfI = blInternalCast(self->impl);

  blMatrix2DApplyOp(&selfI->matrix, opType, opData);
  selfI->matrixType = uint8_t(selfI->matrix.type());

  return BL_SUCCESS;
}

// ============================================================================
// [BLPattern - Equals]
// ============================================================================

bool blPatternEquals(const BLPatternCore* a, const BLPatternCore* b) noexcept {
  const BLPatternImpl* aI = a->impl;
  const BLPatternImpl* bI = b->impl;

  if (aI == bI)
    return true;

  bool eq = (aI->patternType == bI->patternType) &
            (aI->extendMode  == bI->extendMode ) &
            (aI->matrixType  == bI->matrixType ) &
            (aI->matrix      == bI->matrix     ) &
            (aI->area        == bI->area       ) ;
  return eq && aI->image == bI->image;
}

// ============================================================================
// [BLPattern - Runtime]
// ============================================================================

void blPatternOnInit(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);

  BLInternalPatternImpl* patternI = &blNullPatternImpl;
  blInitBuiltInNull(patternI, BL_IMPL_TYPE_PATTERN, 0);
  blCallCtor(patternI->image);
  patternI->patternType = 0;
  patternI->extendMode = uint8_t(BL_EXTEND_MODE_REPEAT);
  patternI->matrix.reset();
  patternI->area.reset(0, 0, 0, 0);
  blAssignBuiltInNull(patternI);

  // Checks whether the initialization order is correct.
  BL_ASSERT(patternI->image.impl != nullptr);
}
