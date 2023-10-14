// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "imageencoder.h"
#include "object_p.h"
#include "runtime_p.h"

// bl::ImageEncoder - Globals
// ==========================

namespace bl {
namespace ImageEncoderInternal {

static BLObjectEternalVirtualImpl<BLImageEncoderImpl, BLImageEncoderVirt> defaultEncoder;

} // {ImageEncoderInternal}
} // {bl}

// bl::ImageEncoder - API - Init & Destroy
// =======================================

BL_API_IMPL BLResult blImageEncoderInit(BLImageEncoderCore* self) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE_ENCODER]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blImageEncoderInitMove(BLImageEncoderCore* self, BLImageEncoderCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isImageEncoder());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE_ENCODER]._d;

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blImageEncoderInitWeak(BLImageEncoderCore* self, const BLImageEncoderCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isImageEncoder());

  return blObjectPrivateInitWeakTagged(self, other);
}

BL_API_IMPL BLResult blImageEncoderDestroy(BLImageEncoderCore* self) noexcept {
  BL_ASSERT(self->_d.isImageEncoder());

  return bl::ObjectInternal::releaseVirtualInstance(self);
}

// bl::ImageEncoder - API - Reset
// ==============================

BL_API_IMPL BLResult blImageEncoderReset(BLImageEncoderCore* self) noexcept {
  BL_ASSERT(self->_d.isImageEncoder());

  return bl::ObjectInternal::replaceVirtualInstance(self, static_cast<BLImageEncoderCore*>(&blObjectDefaults[BL_OBJECT_TYPE_IMAGE_ENCODER]));
}

// bl::ImageEncoder - API - Assign
// ===============================

BL_API_IMPL BLResult blImageEncoderAssignMove(BLImageEncoderCore* self, BLImageEncoderCore* other) noexcept {
  BL_ASSERT(self->_d.isImageEncoder());
  BL_ASSERT(other->_d.isImageEncoder());

  BLImageEncoderCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE_ENCODER]._d;
  return bl::ObjectInternal::replaceVirtualInstance(self, &tmp);
}

BL_API_IMPL BLResult blImageEncoderAssignWeak(BLImageEncoderCore* self, const BLImageEncoderCore* other) noexcept {
  BL_ASSERT(self->_d.isImageEncoder());
  BL_ASSERT(other->_d.isImageEncoder());

  return bl::ObjectInternal::assignVirtualInstance(self, other);
}

// bl::ImageEncoder - API - Interface
// ==================================

BL_API_IMPL BLResult blImageEncoderRestart(BLImageEncoderCore* self) noexcept {
  BL_ASSERT(self->_d.isImageEncoder());
  BLImageEncoderImpl* selfI = self->_impl();

  return selfI->virt->restart(selfI);
}

BL_API_IMPL BLResult blImageEncoderWriteFrame(BLImageEncoderCore* self, BLArrayCore* dst, const BLImageCore* src) noexcept {
  BL_ASSERT(self->_d.isImageEncoder());
  BLImageEncoderImpl* selfI = self->_impl();

  return selfI->virt->writeFrame(selfI, dst, src);
}

// bl::ImageEncoder - Virtual Functions (Null)
// ===========================================

static BLResult BL_CDECL blImageEncoderImplDestroy(BLObjectImpl* impl) noexcept {
  blUnused(impl);
  return BL_SUCCESS;
}

static uint32_t BL_CDECL blImageEncoderImplRestart(BLImageEncoderImpl* impl) noexcept {
  blUnused(impl);
  return BL_ERROR_INVALID_STATE;
}

static BLResult BL_CDECL blImageEncoderImplWriteFrame(BLImageEncoderImpl* impl, BLArrayCore* dst, const BLImageCore* image) noexcept {
  blUnused(impl, dst, image);
  return BL_ERROR_INVALID_STATE;
}

// bl::ImageEncoder - Runtime Registration
// =======================================

void blImageEncoderRtInit(BLRuntimeContext* rt) noexcept {
  using namespace bl::ImageEncoderInternal;

  blUnused(rt);

  // Initialize default BLImageEncoder.
  defaultEncoder.virt.base.destroy = blImageEncoderImplDestroy;
  defaultEncoder.virt.base.getProperty = blObjectImplGetProperty;
  defaultEncoder.virt.base.setProperty = blObjectImplSetProperty;
  defaultEncoder.virt.restart = blImageEncoderImplRestart;
  defaultEncoder.virt.writeFrame = blImageEncoderImplWriteFrame;
  defaultEncoder.impl->ctor(
    &defaultEncoder.virt,
    static_cast<BLImageCodecCore*>(&blObjectDefaults[BL_OBJECT_TYPE_IMAGE_CODEC]));
  defaultEncoder.impl->lastResult = BL_ERROR_NOT_INITIALIZED;

  blObjectDefaults[BL_OBJECT_TYPE_IMAGE_ENCODER]._d.initDynamic(
    BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_IMAGE_ENCODER), &defaultEncoder.impl);
}
