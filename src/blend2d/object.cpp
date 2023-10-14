// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "bitset_p.h"
#include "font_p.h"
#include "fontfeaturesettings_p.h"
#include "fontmanager_p.h"
#include "fontvariationsettings_p.h"
#include "gradient_p.h"
#include "image_p.h"
#include "object_p.h"
#include "path_p.h"
#include "pattern_p.h"
#include "runtime_p.h"
#include "string_p.h"
#include "var_p.h"
#include "support/intops_p.h"
#include "support/ptrops_p.h"

// bl::Object - Globals
// ====================

BLObjectCore blObjectDefaults[BL_OBJECT_TYPE_MAX_VALUE + 1];
const BLObjectImplHeader blObjectHeaderWithRefCountEq0 = { 0, 0 };
const BLObjectImplHeader blObjectHeaderWithRefCountEq1 = { 1, 0 };

void blObjectDestroyExternalDataDummy(void* impl, void* externalData, void* userData) noexcept {
  blUnused(impl, externalData, userData);
}

// bl::Object - API - Alloc & Free Impl
// ====================================

static BL_INLINE BLResult blObjectAllocImplInternal(BLObjectCore* self, uint32_t objectInfo, size_t implSize, size_t implFlags, size_t implAlignment, bool isExternal = false) noexcept {
  if (BL_UNLIKELY(implSize > BL_OBJECT_IMPL_MAX_SIZE))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  implSize = bl::IntOps::alignUp(implSize, implAlignment);

  size_t headerSize = sizeof(BLObjectImplHeader) + (isExternal ? sizeof(BLObjectExternalInfo) : size_t(0));
  size_t allocationSize = implSize + headerSize + implAlignment;

  void* ptr = malloc(allocationSize);
  if (BL_UNLIKELY(!ptr))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLObjectImpl* impl = static_cast<BLObjectImpl*>(
    bl::IntOps::alignUp(bl::PtrOps::offset(ptr, headerSize), implAlignment));
  BLObjectImplHeader* implHeader = bl::ObjectInternal::getImplHeader(impl);

  size_t alignmentOffset = size_t(uintptr_t(impl) - uintptr_t(ptr)) - headerSize;
  BL_ASSERT((alignmentOffset & ~BLObjectImplHeader::kAlignmentOffsetMask) == 0);

  implHeader->refCount = implFlags & BLObjectImplHeader::kRefCountedAndImmutableFlags;
  implHeader->flags = implFlags | alignmentOffset;

  self->_d.clearStaticData();
  self->_d.impl = impl;
  self->_d.info.bits = objectInfo | BL_OBJECT_INFO_M_FLAG | BL_OBJECT_INFO_D_FLAG | BL_OBJECT_INFO_R_FLAG;

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blObjectAllocImpl(BLObjectCore* self, uint32_t objectInfo, size_t implSize) noexcept {
  size_t flags = BLObjectImplHeader::kRefCountedFlag;
  return blObjectAllocImplInternal(self, objectInfo, implSize, flags, BL_OBJECT_IMPL_ALIGNMENT);
}

BL_API_IMPL BLResult blObjectAllocImplAligned(BLObjectCore* self, uint32_t objectInfo, size_t implSize, size_t implAlignment) noexcept {
  if (!bl::IntOps::isPowerOf2(implAlignment))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  size_t flags = BLObjectImplHeader::kRefCountedFlag;
  implAlignment = blClamp<size_t>(implAlignment, 16, 128);
  return blObjectAllocImplInternal(self, objectInfo, implSize, flags, implAlignment);
}

BL_API_IMPL BLResult blObjectAllocImplExternal(BLObjectCore* self, uint32_t objectInfo, size_t implSize, bool immutable, BLDestroyExternalDataFunc destroyFunc, void* userData) noexcept {
  size_t flags = (BLObjectImplHeader::kRefCountedFlag) |
                 (BLObjectImplHeader::kExternalFlag) |
                 (size_t(immutable) << BLObjectImplHeader::kImmutableFlagShift);

  BL_PROPAGATE(blObjectAllocImplInternal(self, objectInfo, implSize, flags, BL_OBJECT_IMPL_ALIGNMENT, true));
  bl::ObjectInternal::initExternalDestroyFunc(self->_d.impl, destroyFunc, userData);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blObjectFreeImpl(BLObjectImpl* impl) noexcept {
  return bl::ObjectInternal::freeImpl(impl);
}

BLResult blObjectDestroyUnknownImpl(BLObjectImpl* impl, BLObjectInfo info) noexcept {
  BL_ASSERT(info.isDynamicObject());

  if (info.isVirtualObject())
    return bl::ObjectInternal::freeVirtualImpl(impl);

  BLObjectType type = info.rawType();
  switch (type) {
    case BL_OBJECT_TYPE_GRADIENT:
      return bl::GradientInternal::freeImpl(static_cast<BLGradientPrivateImpl*>(impl));

    case BL_OBJECT_TYPE_PATTERN:
      return bl::PatternInternal::freeImpl(static_cast<BLPatternPrivateImpl*>(impl));

    case BL_OBJECT_TYPE_STRING:
      return bl::StringInternal::freeImpl(static_cast<BLStringImpl*>(impl));

    case BL_OBJECT_TYPE_PATH:
      return bl::PathInternal::freeImpl(static_cast<BLPathPrivateImpl*>(impl));

    case BL_OBJECT_TYPE_IMAGE:
      return bl::ImageInternal::freeImpl(static_cast<BLImagePrivateImpl*>(impl));

    case BL_OBJECT_TYPE_FONT:
      return bl::FontInternal::freeImpl(static_cast<BLFontPrivateImpl*>(impl));

    case BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS:
      return bl::FontFeatureSettingsInternal::freeImpl(static_cast<BLFontFeatureSettingsImpl*>(impl));

    case BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS:
      return bl::FontVariationSettingsInternal::freeImpl(static_cast<BLFontVariationSettingsImpl*>(impl));

    case BL_OBJECT_TYPE_ARRAY_OBJECT:
    case BL_OBJECT_TYPE_ARRAY_INT8:
    case BL_OBJECT_TYPE_ARRAY_UINT8:
    case BL_OBJECT_TYPE_ARRAY_INT16:
    case BL_OBJECT_TYPE_ARRAY_UINT16:
    case BL_OBJECT_TYPE_ARRAY_INT32:
    case BL_OBJECT_TYPE_ARRAY_UINT32:
    case BL_OBJECT_TYPE_ARRAY_INT64:
    case BL_OBJECT_TYPE_ARRAY_UINT64:
    case BL_OBJECT_TYPE_ARRAY_FLOAT32:
    case BL_OBJECT_TYPE_ARRAY_FLOAT64:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_1:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_2:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_3:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_4:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_6:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_8:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_10:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_12:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_16:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_20:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_24:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_32:
      return bl::ArrayInternal::freeImpl(static_cast<BLArrayImpl*>(impl));

    case BL_OBJECT_TYPE_BIT_SET:
      // NOTE: It's guaranteed that this BitSet is dynamic, so we don't have to correct the type.
      return bl::BitSetInternal::freeImpl(static_cast<BLBitSetImpl*>(impl));

    default:
      // TODO: This shouldn't happen.
      return bl::ObjectInternal::freeImpl(impl);
  }
}

// bl::Object - API - Construction & Destruction
// =============================================

BL_API_IMPL BLResult blObjectInitMove(BLUnknown* self, BLUnknown* other) noexcept {
  BL_ASSERT(self != other);

  return blObjectPrivateInitMoveUnknown(blAsObject(self), blAsObject(other));
}

BL_API_IMPL BLResult blObjectInitWeak(BLUnknown* self, const BLUnknown* other) noexcept {
  BL_ASSERT(self != other);

  return blObjectPrivateInitWeakUnknown(blAsObject(self), blAsObject(other));
}

// bl::Object - API - Reset
// ========================

BL_API_IMPL BLResult blObjectReset(BLUnknown* self) noexcept {
  BLObjectType type = blAsObject(self)->_d.getType();

  bl::ObjectInternal::releaseUnknownInstance(blAsObject(self));
  blAsObject(self)->_d = blObjectDefaults[type]._d;

  return BL_SUCCESS;
}

// bl::Object - API - Assign
// =========================

BL_API_IMPL BLResult blObjectAssignMove(BLUnknown* self, BLUnknown* other) noexcept {
  BLObjectType type = blAsObject(other)->_d.getType();
  BLObjectCore tmp = *blAsObject(other);

  blAsObject(other)->_d = blObjectDefaults[type]._d;
  bl::ObjectInternal::releaseUnknownInstance(blAsObject(self));

  blAsObject(self)->_d = tmp._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blObjectAssignWeak(BLUnknown* self, const BLUnknown* other) noexcept {
  return blObjectPrivateAssignWeakUnknown(blAsObject(self), blAsObject(other));
}

// bl::Object - API - Properties
// =============================

BL_API_IMPL BLResult blObjectGetProperty(const BLUnknown* self, const char* name, size_t nameSize, BLVarCore* valueOut) noexcept {
  if (nameSize == SIZE_MAX)
    nameSize = strlen(name);

  if (!blAsObject(self)->_d.isVirtualObject())
    return blTraceError(BL_ERROR_INVALID_KEY);

  const BLObjectVirtImpl* impl = static_cast<const BLObjectVirtImpl*>(blAsObject(self)->_d.impl);
  return impl->virt->base.getProperty(impl, name, nameSize, valueOut);
}

BL_API_IMPL BLResult blObjectGetPropertyBool(const BLUnknown* self, const char* name, size_t nameSize, bool* valueOut) noexcept {
  BLVarCore v;
  v._d.initNull();

  *valueOut = false;
  BL_PROPAGATE(blObjectGetProperty(self, name, nameSize, &v));

  BLResult result = blVarToBool(&v, valueOut);
  blVarDestroy(&v);
  return result;
}

BL_API_IMPL BLResult blObjectGetPropertyInt32(const BLUnknown* self, const char* name, size_t nameSize, int32_t* valueOut) noexcept {
  BLVarCore v;
  v._d.initNull();

  *valueOut = 0;
  BL_PROPAGATE(blObjectGetProperty(self, name, nameSize, &v));

  BLResult result = blVarToInt32(&v, valueOut);
  blVarDestroy(&v);
  return result;
}

BL_API_IMPL BLResult blObjectGetPropertyInt64(const BLUnknown* self, const char* name, size_t nameSize, int64_t* valueOut) noexcept {
  BLVarCore v;
  v._d.initNull();

  *valueOut = 0;
  BL_PROPAGATE(blObjectGetProperty(self, name, nameSize, &v));

  BLResult result = blVarToInt64(&v, valueOut);
  blVarDestroy(&v);
  return result;
}

BL_API_IMPL BLResult blObjectGetPropertyUInt32(const BLUnknown* self, const char* name, size_t nameSize, uint32_t* valueOut) noexcept {
  BLVarCore v;
  v._d.initNull();

  *valueOut = 0;
  BL_PROPAGATE(blObjectGetProperty(self, name, nameSize, &v));

  BLResult result = blVarToUInt32(&v, valueOut);
  blVarDestroy(&v);
  return result;
}

BL_API_IMPL BLResult blObjectGetPropertyUInt64(const BLUnknown* self, const char* name, size_t nameSize, uint64_t* valueOut) noexcept {
  BLVarCore v;
  v._d.initNull();

  *valueOut = 0;
  BL_PROPAGATE(blObjectGetProperty(self, name, nameSize, &v));

  BLResult result = blVarToUInt64(&v, valueOut);
  blVarDestroy(&v);
  return result;
}

BL_API_IMPL BLResult blObjectGetPropertyDouble(const BLUnknown* self, const char* name, size_t nameSize, double* valueOut) noexcept {
  BLVarCore v;
  v._d.initNull();

  *valueOut = 0.0;
  BL_PROPAGATE(blObjectGetProperty(self, name, nameSize, &v));

  BLResult result = blVarToDouble(&v, valueOut);
  blVarDestroy(&v);
  return result;
}

BL_API_IMPL BLResult blObjectSetProperty(BLUnknown* self, const char* name, size_t nameSize, const BLUnknown* value) noexcept {
  if (nameSize == SIZE_MAX)
    nameSize = strlen(name);

  if (!blAsObject(self)->_d.isVirtualObject())
    return blTraceError(BL_ERROR_INVALID_KEY);

  BLObjectVirtImpl* impl = static_cast<BLObjectVirtImpl*>(blAsObject(self)->_d.impl);
  return impl->virt->base.setProperty(impl, name, nameSize, static_cast<const BLVarCore*>(value));
}

BL_API_IMPL BLResult blObjectSetPropertyBool(BLUnknown* self, const char* name, size_t nameSize, bool value) noexcept {
  // NOTE: Bool value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  v._d.initBool(value);
  return blObjectSetProperty(self, name, nameSize, &v);
}

BL_API_IMPL BLResult blObjectSetPropertyInt32(BLUnknown* self, const char* name, size_t nameSize, int32_t value) noexcept {
  // NOTE: Integer value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  v._d.initInt64(value);
  return blObjectSetProperty(self, name, nameSize, &v);
}

BL_API_IMPL BLResult blObjectSetPropertyInt64(BLUnknown* self, const char* name, size_t nameSize, int64_t value) noexcept {
  // NOTE: Integer value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  v._d.initInt64(value);
  return blObjectSetProperty(self, name, nameSize, &v);
}

BL_API_IMPL BLResult blObjectSetPropertyUInt32(BLUnknown* self, const char* name, size_t nameSize, uint32_t value) noexcept {
  // NOTE: Integer value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  v._d.initUInt64(value);
  return blObjectSetProperty(self, name, nameSize, &v);
}

BL_API_IMPL BLResult blObjectSetPropertyUInt64(BLUnknown* self, const char* name, size_t nameSize, uint64_t value) noexcept {
  // NOTE: Integer value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  v._d.initUInt64(value);
  return blObjectSetProperty(self, name, nameSize, &v);
}

BL_API_IMPL BLResult blObjectSetPropertyDouble(BLUnknown* self, const char* name, size_t nameSize, double value) noexcept {
  // NOTE: Double value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  v._d.initDouble(value);
  return blObjectSetProperty(self, name, nameSize, &v);
}

BLResult blObjectImplGetProperty(const BLObjectImpl* impl, const char* name, size_t nameSize, BLVarCore* valueOut) noexcept {
  blUnused(impl, name, nameSize, valueOut);
  return blTraceError(BL_ERROR_INVALID_KEY);
}

BLResult blObjectImplSetProperty(BLObjectImpl* impl, const char* name, size_t nameSize, const BLVarCore* value) noexcept {
  blUnused(impl, name, nameSize, value);
  return blTraceError(BL_ERROR_INVALID_KEY);
}
