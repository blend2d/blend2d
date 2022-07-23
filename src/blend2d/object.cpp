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

// BLObject - Globals
// ==================

BLObjectCore blObjectDefaults[BL_OBJECT_TYPE_MAX_VALUE + 1];
const size_t blObjectDummyRefCount[1] = { 1 };

// BLObject - API - Alloc & Free Impl
// ==================================

void* blObjectDetailAllocImpl(BLObjectDetail* d, uint32_t info, size_t implSize, size_t* implSizeOut) noexcept {
  if (implSize > SIZE_MAX - BL_OBJECT_IMPL_ALIGNMENT - sizeof(void*))
    return nullptr;

  implSize = BLIntOps::alignUp(implSize, sizeof(void*));

  size_t allocatedImplSize = implSize + BL_OBJECT_IMPL_ALIGNMENT;
  void* allocatedImplPtr = malloc(allocatedImplSize);

  if (BL_UNLIKELY(!allocatedImplPtr))
    return nullptr;

  void* impl = BLIntOps::alignUp(BLPtrOps::offset(allocatedImplPtr, 1), BL_OBJECT_IMPL_ALIGNMENT);
  void* allocatedImplEnd = BLPtrOps::offset(allocatedImplPtr, allocatedImplSize);

  // Make sure the resulting impl size is not less than the requested impl size.
  size_t realImplSize = uintptr_t(allocatedImplEnd) - uintptr_t(impl);
  BL_ASSERT(realImplSize >= implSize);

  // Allocation adjustment is stored in 'a' field.
  uint32_t aFieldBits = blObjectImplCalcAllocationAdjustmentField(impl, allocatedImplPtr);

  // Initialize the newly created Impl and return.
  info |= BL_OBJECT_INFO_MARKER_FLAG | BL_OBJECT_INFO_REF_COUNTED_FLAG | BL_OBJECT_INFO_DYNAMIC_FLAG | aFieldBits;
  blObjectImplInitRefCount(impl, blObjectImplGetRefCountBaseFromObjectInfo(BLObjectInfo{info}));

  d->clearStaticData();
  d->impl = impl;
  d->info.bits = info;

  *implSizeOut = realImplSize;
  return impl;
}

// This function allocates both Impl and BLObjectExternalInfo header, which it places either before the reference
// count of after the Impl data depending on the alignment of the pointer returned by the system allocator. The
// reference count should always be on a separate cache line if the cache line is relatively small (64 bytes). The
// following illustrates where external data and additional ExternalOptData is placed considering 64-bit machine:
//
// +-+-+-+-+-+-+-+-+
// |64ByteCacheLine|
// +-+-+-+-+-+-+-+-+---------------+-+-+-+-+-+-+-+
//               |R|   Impl Data   |X|X|o|o|o|o| |
//             +-+-+---------------+-+-+-+-+-+-+-+
//             | |R|   Impl Data   |X|X|o|o|o|o|
//           +-+-+-+---------------+-+-+-+-+-+-+
//           |X|X|R|   Impl Data   |o|o|o|o| |
//         +-+-+-+-+---------------+-+-+-+-+-+
//         | |X|X|R|   Impl Data   |o|o|o|o|
//       +-+-+-+-+-+---------------+-+-+-+-+
//       |o|o|o|o|R|   Impl Data   |X|X| |
//     +-+-+-+-+-+-+---------------+-+-+-+
//     | |o|o|o|o|R|   Impl Data   |X|X|
//   +-+-+-+-+-+-+-+---------------+-+-+
//   |o|o|o|o|X|X|R|   Impl Data   | |
// +-+-+-+-+-+-+-+-+---------------+-+
// | |o|o|o|o|X|X|R|   Impl Data   |
// +-+-+-+-+-+-+-+-+---------------+
//
// Where:
//   - 'R' - Placement of a reference counter - always immediately before Impl
//   - 'X' - Placement of BLObjectExternalInfo - either preceding RefCount of immediately after Impl data.
//   - 'o' - Optional content (max 32bytes) that can be used by Impl to store additional content to complement
//           data stored in BLObjectExternalInfo.
//   - ' ' - Always unused.
//
// NOTE: 32-bit machine layout is very similar, but sizes would be 4 bytes instead of 8 - giving it more space
// for optional external data ('o'), however, it's forbidden to use more than 32 bytes by the implementation.
void* blObjectDetailAllocImplExternal(BLObjectDetail* d, uint32_t info, size_t implSize, BLObjectExternalInfo** externalInfoOut, void** externalOptDataOut) noexcept {
  if (implSize > SIZE_MAX - BL_OBJECT_IMPL_ALIGNMENT - sizeof(void*))
    return nullptr;

  implSize = BLIntOps::alignUp(implSize, sizeof(void*));

  size_t allocatedImplSize = implSize + BL_OBJECT_IMPL_ALIGNMENT;
  void* allocatedImplPtr = malloc(allocatedImplSize);

  if (BL_UNLIKELY(!allocatedImplPtr))
    return nullptr;

  void* impl = BLIntOps::alignUp(BLPtrOps::offset(allocatedImplPtr, 1), BL_OBJECT_IMPL_ALIGNMENT);
  uint32_t adj = blObjectImplCalcAllocationAdjustmentField(impl, allocatedImplPtr);
  BLObjectExternalInfoAndData ext = blObjectDetailGetExternalInfoAndData(impl, allocatedImplPtr, BLObjectImplSize(implSize));

  // Initialize the newly created Impl and return.
  info |= BL_OBJECT_INFO_MARKER_FLAG | BL_OBJECT_INFO_REF_COUNTED_FLAG | BL_OBJECT_INFO_DYNAMIC_FLAG | BL_OBJECT_INFO_X_FLAG | adj;

  size_t initialRefCountValue = blObjectImplGetRefCountBaseFromObjectInfo(BLObjectInfo{info});
  blObjectImplInitRefCount(impl, initialRefCountValue);

  d->clearStaticData();
  d->impl = impl;
  d->info.bits = info;

  *externalInfoOut = ext.info;
  *externalOptDataOut = ext.optionalData;

  return impl;
}

BLResult blObjectDetailFreeImpl(void* impl, uint32_t info) noexcept {
  return blObjectImplFreeInline(impl, BLObjectInfo{info});
}

void blObjectDestroyExternalDataDummy(void* impl, void* externalData, void* userData) noexcept {
  blUnused(impl, externalData, userData);
}

BLResult blObjectDetailDestroyUnknownImpl(void* impl, BLObjectInfo info) noexcept {
  BL_ASSERT(info.isDynamicObject());

  if (info.virtualFlag())
    return blObjectImplFreeVirtual(impl, info);

  BLObjectType type = info.rawType();
  switch (type) {
    case BL_OBJECT_TYPE_GRADIENT:
      return BLGradientPrivate::freeImpl(static_cast<BLGradientPrivateImpl*>(impl), info);

    case BL_OBJECT_TYPE_PATTERN:
      return BLPatternPrivate::freeImpl(static_cast<BLPatternPrivateImpl*>(impl), info);

    case BL_OBJECT_TYPE_STRING:
      return BLStringPrivate::freeImpl(static_cast<BLStringImpl*>(impl), info);

    case BL_OBJECT_TYPE_PATH:
      return BLPathPrivate::freeImpl(static_cast<BLPathPrivateImpl*>(impl), info);

    case BL_OBJECT_TYPE_IMAGE:
      return BLImagePrivate::freeImpl(static_cast<BLImagePrivateImpl*>(impl), info);

    case BL_OBJECT_TYPE_FONT:
      return blFontImplFree(static_cast<BLFontPrivateImpl*>(impl), info);

    case BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS:
      return BLFontFeatureSettingsPrivate::freeImpl(static_cast<BLFontFeatureSettingsImpl*>(impl), info);

    case BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS:
      return BLFontVariationSettingsPrivate::freeImpl(static_cast<BLFontVariationSettingsImpl*>(impl), info);

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
      return BLArrayPrivate::freeImpl(static_cast<BLArrayImpl*>(impl), info);

    case BL_OBJECT_TYPE_BIT_SET:
      // NOTE: It's guaranteed that this BitSet is dynamic, so we don't have to correct the type.
      return BLBitSetPrivate::freeImpl(static_cast<BLBitSetImpl*>(impl), info);

    default:
      // TODO: This shouldn't happen.
      return blObjectImplFreeInline(impl, info);
  }
}

// BLObject - API - Construction & Destruction
// ===========================================

BLResult blObjectInitMove(BLUnknown* self, BLUnknown* other) noexcept {
  BL_ASSERT(self != other);

  return blObjectPrivateInitMoveUnknown(blAsObject(self), blAsObject(other));
}

BLResult blObjectInitWeak(BLUnknown* self, const BLUnknown* other) noexcept {
  BL_ASSERT(self != other);

  return blObjectPrivateInitWeakUnknown(blAsObject(self), blAsObject(other));
}

// BLObject - API - Reset
// ======================

BLResult blObjectReset(BLUnknown* self) noexcept {
  BLObjectType type = blAsObject(self)->_d.getType();

  blObjectPrivateReleaseUnknown(blAsObject(self));
  blAsObject(self)->_d = blObjectDefaults[type]._d;

  return BL_SUCCESS;
}

// BLObject - API - Assign
// =======================

BLResult blObjectAssignMove(BLUnknown* self, BLUnknown* other) noexcept {
  BLObjectType type = blAsObject(other)->_d.getType();
  BLObjectCore tmp = *blAsObject(other);

  blAsObject(other)->_d = blObjectDefaults[type]._d;
  blObjectPrivateReleaseUnknown(blAsObject(self));

  blAsObject(self)->_d = tmp._d;
  return BL_SUCCESS;
}

BLResult blObjectAssignWeak(BLUnknown* self, const BLUnknown* other) noexcept {
  return blObjectPrivateAssignWeakUnknown(blAsObject(self), blAsObject(other));
}

// BLObject - API - Properties
// ===========================

BLResult blObjectGetProperty(const BLUnknown* self, const char* name, size_t nameSize, BLVarCore* valueOut) noexcept {
  if (nameSize == SIZE_MAX)
    nameSize = strlen(name);

  if (!blAsObject(self)->_d.isVirtualObject())
    return blTraceError(BL_ERROR_INVALID_KEY);

  const BLObjectVirtImpl* impl = static_cast<const BLObjectVirtImpl*>(blAsObject(self)->_d.impl);
  return impl->virt->base.getProperty(impl, name, nameSize, valueOut);
}

BLResult blObjectGetPropertyBool(const BLUnknown* self, const char* name, size_t nameSize, bool* valueOut) noexcept {
  BLVarCore v;
  v._d.initNull();

  *valueOut = false;
  BL_PROPAGATE(blObjectGetProperty(self, name, nameSize, &v));

  BLResult result = blVarToBool(&v, valueOut);
  blVarDestroy(&v);
  return result;
}

BLResult blObjectGetPropertyInt32(const BLUnknown* self, const char* name, size_t nameSize, int32_t* valueOut) noexcept {
  BLVarCore v;
  v._d.initNull();

  *valueOut = 0;
  BL_PROPAGATE(blObjectGetProperty(self, name, nameSize, &v));

  BLResult result = blVarToInt32(&v, valueOut);
  blVarDestroy(&v);
  return result;
}

BLResult blObjectGetPropertyInt64(const BLUnknown* self, const char* name, size_t nameSize, int64_t* valueOut) noexcept {
  BLVarCore v;
  v._d.initNull();

  *valueOut = 0;
  BL_PROPAGATE(blObjectGetProperty(self, name, nameSize, &v));

  BLResult result = blVarToInt64(&v, valueOut);
  blVarDestroy(&v);
  return result;
}

BLResult blObjectGetPropertyUInt32(const BLUnknown* self, const char* name, size_t nameSize, uint32_t* valueOut) noexcept {
  BLVarCore v;
  v._d.initNull();

  *valueOut = 0;
  BL_PROPAGATE(blObjectGetProperty(self, name, nameSize, &v));

  BLResult result = blVarToUInt32(&v, valueOut);
  blVarDestroy(&v);
  return result;
}

BLResult blObjectGetPropertyUInt64(const BLUnknown* self, const char* name, size_t nameSize, uint64_t* valueOut) noexcept {
  BLVarCore v;
  v._d.initNull();

  *valueOut = 0;
  BL_PROPAGATE(blObjectGetProperty(self, name, nameSize, &v));

  BLResult result = blVarToUInt64(&v, valueOut);
  blVarDestroy(&v);
  return result;
}

BLResult blObjectGetPropertyDouble(const BLUnknown* self, const char* name, size_t nameSize, double* valueOut) noexcept {
  BLVarCore v;
  v._d.initNull();

  *valueOut = 0.0;
  BL_PROPAGATE(blObjectGetProperty(self, name, nameSize, &v));

  BLResult result = blVarToDouble(&v, valueOut);
  blVarDestroy(&v);
  return result;
}

BLResult blObjectSetProperty(BLUnknown* self, const char* name, size_t nameSize, const BLUnknown* value) noexcept {
  if (nameSize == SIZE_MAX)
    nameSize = strlen(name);

  if (!blAsObject(self)->_d.isVirtualObject())
    return blTraceError(BL_ERROR_INVALID_KEY);

  BLObjectVirtImpl* impl = static_cast<BLObjectVirtImpl*>(blAsObject(self)->_d.impl);
  return impl->virt->base.setProperty(impl, name, nameSize, static_cast<const BLVarCore*>(value));
}

BLResult blObjectSetPropertyBool(BLUnknown* self, const char* name, size_t nameSize, bool value) noexcept {
  // NOTE: Bool value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  v._d.initBool(value);
  return blObjectSetProperty(self, name, nameSize, &v);
}

BLResult blObjectSetPropertyInt32(BLUnknown* self, const char* name, size_t nameSize, int32_t value) noexcept {
  // NOTE: Integer value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  v._d.initInt64(value);
  return blObjectSetProperty(self, name, nameSize, &v);
}

BLResult blObjectSetPropertyInt64(BLUnknown* self, const char* name, size_t nameSize, int64_t value) noexcept {
  // NOTE: Integer value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  v._d.initInt64(value);
  return blObjectSetProperty(self, name, nameSize, &v);
}

BLResult blObjectSetPropertyUInt32(BLUnknown* self, const char* name, size_t nameSize, uint32_t value) noexcept {
  // NOTE: Integer value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  v._d.initUInt64(value);
  return blObjectSetProperty(self, name, nameSize, &v);
}

BLResult blObjectSetPropertyUInt64(BLUnknown* self, const char* name, size_t nameSize, uint64_t value) noexcept {
  // NOTE: Integer value is always in SSO mode, no need to call BLVarCore destructor.
  BLVarCore v;
  v._d.initUInt64(value);
  return blObjectSetProperty(self, name, nameSize, &v);
}

BLResult blObjectSetPropertyDouble(BLUnknown* self, const char* name, size_t nameSize, double value) noexcept {
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
