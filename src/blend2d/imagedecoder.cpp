// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "imagedecoder.h"
#include "object_p.h"
#include "runtime_p.h"

// BLImageDecoder - Globals
// ========================

namespace BLImageDecoderPrivate {

static BLObjectEthernalVirtualImpl<BLImageDecoderImpl, BLImageDecoderVirt> defaultDecoder;

} // {BLImageDecoderPrivate}

// BLImageDecoder - API - Init & Destroy
// =====================================

BL_API_IMPL BLResult blImageDecoderInit(BLImageDecoderCore* self) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE_DECODER]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blImageDecoderInitMove(BLImageDecoderCore* self, BLImageDecoderCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isImageDecoder());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE_DECODER]._d;

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blImageDecoderInitWeak(BLImageDecoderCore* self, const BLImageDecoderCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isImageDecoder());

  return blObjectPrivateInitWeakTagged(self, other);
}

BL_API_IMPL BLResult blImageDecoderDestroy(BLImageDecoderCore* self) noexcept {
  return blObjectPrivateReleaseVirtual(self);
}

// BLImageDecoder - API - Reset
// ============================

BL_API_IMPL BLResult blImageDecoderReset(BLImageDecoderCore* self) noexcept {
  BL_ASSERT(self->_d.isImageDecoder());

  return blObjectPrivateReplaceVirtual(self, static_cast<BLImageDecoderCore*>(&blObjectDefaults[BL_OBJECT_TYPE_IMAGE_DECODER]));
}

// BLImageDecoder - API - Assign
// =============================

BL_API_IMPL BLResult blImageDecoderAssignMove(BLImageDecoderCore* self, BLImageDecoderCore* other) noexcept {
  BL_ASSERT(self->_d.isImageDecoder());
  BL_ASSERT(other->_d.isImageDecoder());

  BLImageDecoderCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE_DECODER]._d;
  return blObjectPrivateReplaceVirtual(self, &tmp);
}

BL_API_IMPL BLResult blImageDecoderAssignWeak(BLImageDecoderCore* self, const BLImageDecoderCore* other) noexcept {
  BL_ASSERT(self->_d.isImageDecoder());
  BL_ASSERT(other->_d.isImageDecoder());

  return blObjectPrivateAssignWeakVirtual(self, other);
}

// BLImageDecoder - API - Interface
// ================================

BL_API_IMPL BLResult blImageDecoderRestart(BLImageDecoderCore* self) noexcept {
  BL_ASSERT(self->_d.isImageDecoder());
  BLImageDecoderImpl* selfI = self->_impl();

  return selfI->virt->restart(selfI);
}

BL_API_IMPL BLResult blImageDecoderReadInfo(BLImageDecoderCore* self, BLImageInfo* infoOut, const uint8_t* data, size_t size) noexcept {
  BL_ASSERT(self->_d.isImageDecoder());
  BLImageDecoderImpl* selfI = self->_impl();

  return selfI->virt->readInfo(selfI, infoOut, data, size);
}

BL_API_IMPL BLResult blImageDecoderReadFrame(BLImageDecoderCore* self, BLImageCore* imageOut, const uint8_t* data, size_t size) noexcept {
  BL_ASSERT(self->_d.isImageDecoder());
  BLImageDecoderImpl* selfI = self->_impl();

  return selfI->virt->readFrame(selfI, imageOut, data, size);
}

// BLImageDecoder - Virtual Functions (Null)
// =========================================

static BLResult BL_CDECL blImageDecoderImplDestroy(BLObjectImpl* impl, uint32_t info) noexcept {
  blUnused(impl, info);
  return BL_SUCCESS;
}

static uint32_t BL_CDECL blImageDecoderImplRestart(BLImageDecoderImpl* impl) noexcept {
  blUnused(impl);
  return BL_ERROR_INVALID_STATE;
}

static BLResult BL_CDECL blImageDecoderImplReadInfo(BLImageDecoderImpl* impl, BLImageInfo* infoOut, const uint8_t* data, size_t size) noexcept {
  blUnused(impl, infoOut, data, size);
  return BL_ERROR_INVALID_STATE;
}

static BLResult BL_CDECL blImageDecoderImplReadFrame(BLImageDecoderImpl* impl, BLImageCore* imageOut, const uint8_t* data, size_t size) noexcept {
  blUnused(impl, imageOut, data, size);
  return BL_ERROR_INVALID_STATE;
}

// BLImageDecoder - Runtime Registration
// =====================================

void blImageDecoderRtInit(BLRuntimeContext* rt) noexcept {
  using namespace BLImageDecoderPrivate;

  blUnused(rt);

  // Initialize default BLImageDecoder.
  defaultDecoder.virt.base.destroy = blImageDecoderImplDestroy;
  defaultDecoder.virt.base.getProperty = blObjectImplGetProperty;
  defaultDecoder.virt.base.setProperty = blObjectImplSetProperty;
  defaultDecoder.virt.restart = blImageDecoderImplRestart;
  defaultDecoder.virt.readInfo = blImageDecoderImplReadInfo;
  defaultDecoder.virt.readFrame = blImageDecoderImplReadFrame;
  defaultDecoder.impl->ctor(
    &defaultDecoder.virt,
    static_cast<BLImageCodecCore*>(&blObjectDefaults[BL_OBJECT_TYPE_IMAGE_CODEC]));
  defaultDecoder.impl->lastResult = BL_ERROR_NOT_INITIALIZED;

  blObjectDefaults[BL_OBJECT_TYPE_IMAGE_DECODER]._d.initDynamic(
    BL_OBJECT_TYPE_IMAGE_DECODER,
    BLObjectInfo{BL_OBJECT_INFO_IMMUTABLE_FLAG},
    &defaultDecoder.impl);
}
