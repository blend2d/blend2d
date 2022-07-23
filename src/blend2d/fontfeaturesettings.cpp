// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "fontfeaturesettings_p.h"
#include "fonttags_p.h"
#include "object_p.h"
#include "runtime_p.h"
#include "string_p.h"
#include "support/algorithm_p.h"
#include "support/memops_p.h"
#include "support/ptrops_p.h"

namespace BLFontFeatureSettingsPrivate {

// BLFontFeatureSettings - SSO Utilities
// =====================================

//! A constant that can be used to increment / decrement a size in SSO representation.
static constexpr uint32_t kSSOSizeIncrement = (1u << BL_OBJECT_INFO_A_SHIFT);

static BL_INLINE BLResult initSSO(BLFontFeatureSettingsCore* self, size_t size = 0) noexcept {
  self->_d.initStatic(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS, BLObjectInfo::packFields(uint32_t(size)));
  return BL_SUCCESS;
}

static BL_INLINE size_t getSSOSize(const BLFontFeatureSettingsCore* self) noexcept { return self->_d.info.aField(); }
static BL_INLINE void setSSOSize(BLFontFeatureSettingsCore* self, size_t size) noexcept { self->_d.info.setAField(uint32_t(size)); }

static BL_INLINE uint32_t getSSOValueAt(const BLFontFeatureSettingsCore* self, size_t index) noexcept {return (self->_d.info.bits >> index) & 0x1u; }
static BL_INLINE void setSSOValueAt(BLFontFeatureSettingsCore* self, size_t index, uint32_t value) noexcept {
  uint32_t clearMask = 1u << index;
  uint32_t valueMask = value << index;

  self->_d.info.bits = (self->_d.info.bits & ~clearMask) | valueMask;
}

static BL_INLINE bool findSSOKey(const BLFontFeatureSettingsCore* self, uint32_t id, size_t* indexOut) noexcept {
  const uint8_t* ssoIds = self->_d.u8_data;
  size_t size = getSSOSize(self);

  size_t i = 0;
  for (i = 0; i < size; i++) {
    if (ssoIds[i] < id)
      continue;
    *indexOut = i;
    return id == ssoIds[i];
  }

  *indexOut = i;
  return false;
}

static bool convertItemsToSSO(BLFontFeatureSettingsCore* dst, const BLFontFeatureItem* items, size_t size) noexcept {
  BL_ASSERT(size <= BLFontFeatureSettings::kSSOCapacity);

  initSSO(dst, size);

  uint8_t* ssoIds = dst->_d.u8_data;
  uint32_t ssoBits = 0;

  for (size_t i = 0; i < size; i++) {
    uint32_t id = BLFontTagsPrivate::featureTagToId(items[i].tag);
    uint32_t value = items[i].value;

    if (id == BLFontTagsPrivate::kInvalidId || value > 1)
      return false;

    ssoIds[i] = uint8_t(id);
    ssoBits |= value << i;
  }

  dst->_d.info.bits |= ssoBits;
  return true;
}

// BLFontFeatureSettings - Impl Utilities
// ======================================

static BL_INLINE constexpr BLObjectImplSize implSizeFromCapacity(size_t capacity) noexcept {
  return BLObjectImplSize(sizeof(BLFontFeatureSettingsImpl) + capacity * sizeof(BLFontFeatureItem));
}

static BL_INLINE constexpr size_t capacityFromImplSize(BLObjectImplSize implSize) noexcept {
  return (implSize.value() - sizeof(BLFontFeatureSettingsImpl)) / sizeof(BLFontFeatureItem);
}

static BL_INLINE constexpr size_t getMaximumSize() noexcept {
  return BLFontTagsPrivate::kUniqueTagCount;
}

static BL_INLINE BLObjectImplSize expandImplSize(BLObjectImplSize implSize) noexcept {
  return blObjectExpandImplSize(implSize);
}

static BL_INLINE BLFontFeatureSettingsImpl* getImpl(const BLFontFeatureSettingsCore* self) noexcept {
  return static_cast<BLFontFeatureSettingsImpl*>(self->_d.impl);
}

static BL_INLINE bool isMutable(const BLFontFeatureSettingsCore* self) noexcept {
  const size_t* refCountPtr = blObjectDummyRefCount;
  if (!self->_d.sso())
    refCountPtr = blObjectImplGetRefCountPtr(self->_d.impl);
  return *refCountPtr == 1;
}

static BL_INLINE BLResult initDynamic(BLFontFeatureSettingsCore* self, BLObjectImplSize implSize, size_t size = 0u) noexcept {
  BLFontFeatureSettingsImpl* impl = blObjectDetailAllocImplT<BLFontFeatureSettingsImpl>(self,
    BLObjectInfo::packType(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS), implSize, &implSize);

  if(BL_UNLIKELY(!impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLFontFeatureItem* items = BLPtrOps::offset<BLFontFeatureItem>(impl, sizeof(BLFontFeatureSettingsImpl));
  impl->data = items;
  impl->size = size;
  impl->capacity = capacityFromImplSize(implSize);
  BL_ASSERT(size <= impl->capacity);

  return BL_SUCCESS;
}

static BL_NOINLINE BLResult initDynamicFromSSO(BLFontFeatureSettingsCore* self, BLObjectImplSize implSize, const BLFontFeatureSettingsCore* ssoMap) noexcept {
  size_t size = getSSOSize(ssoMap);

  BLFontFeatureSettingsImpl* impl = blObjectDetailAllocImplT<BLFontFeatureSettingsImpl>(self,
    BLObjectInfo::packType(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS), implSize, &implSize);

  if(BL_UNLIKELY(!impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLFontFeatureItem* items = BLPtrOps::offset<BLFontFeatureItem>(impl, sizeof(BLFontFeatureSettingsImpl));
  impl->data = items;
  impl->size = size;
  impl->capacity = capacityFromImplSize(implSize);
  BL_ASSERT(size <= impl->capacity);

  const uint8_t* ssoIds = ssoMap->_d.u8_data;
  uint32_t ssoBits = ssoMap->_d.info.bits;

  for (size_t i = 0; i < size; i++, ssoBits >>= 1)
    items[i] = BLFontFeatureItem{BLFontTagsPrivate::featureIdToTagTable[ssoIds[i]], ssoBits & 0x1u};

  return BL_SUCCESS;
}

static BL_NOINLINE BLResult initDynamicFromData(BLFontFeatureSettingsCore* self, BLObjectImplSize implSize, const BLFontFeatureItem* src, size_t size) noexcept {
  BLFontFeatureSettingsImpl* impl = blObjectDetailAllocImplT<BLFontFeatureSettingsImpl>(self,
    BLObjectInfo::packType(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS), implSize, &implSize);

  if(BL_UNLIKELY(!impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLFontFeatureItem* items = BLPtrOps::offset<BLFontFeatureItem>(impl, sizeof(BLFontFeatureSettingsImpl));
  impl->data = items;
  impl->size = size;
  impl->capacity = capacityFromImplSize(implSize);
  BL_ASSERT(size <= impl->capacity);

  memcpy(items, src, size * sizeof(BLFontFeatureItem));
  return BL_SUCCESS;
}

BLResult freeImpl(BLFontFeatureSettingsImpl* impl, BLObjectInfo info) noexcept {
  return blObjectImplFreeInline(impl, info);
}

// BLFontFeatureSettings - Instance Utilities
// ==========================================

static BL_INLINE BLResult releaseInstance(BLFontFeatureSettingsCore* self) noexcept {
  BLFontFeatureSettingsImpl* impl = getImpl(self);
  BLObjectInfo info = self->_d.info;

  if (info.isRefCountedObject() && blObjectImplDecRefAndTest(impl, info))
    return freeImpl(impl, info);

  return BL_SUCCESS;
}

static BL_INLINE BLResult replaceInstance(BLFontFeatureSettingsCore* self, const BLFontFeatureSettingsCore* other) noexcept {
  BLFontFeatureSettingsImpl* impl = getImpl(self);
  BLObjectInfo info = self->_d.info;

  self->_d = other->_d;

  if (info.isRefCountedObject() && blObjectImplDecRefAndTest(impl, info))
    return freeImpl(impl, info);

  return BL_SUCCESS;
}

} // {BLFontFeatureSettingsPrivate}

// BLFontFeatureSettings - API - Init & Destroy
// ============================================

BLResult blFontFeatureSettingsInit(BLFontFeatureSettingsCore* self) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  return initSSO(self);
}

BLResult blFontFeatureSettingsInitMove(BLFontFeatureSettingsCore* self, BLFontFeatureSettingsCore* other) noexcept {
  using namespace BLFontFeatureSettingsPrivate;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontFeatureSettings());

  self->_d = other->_d;
  return initSSO(other);
}

BLResult blFontFeatureSettingsInitWeak(BLFontFeatureSettingsCore* self, const BLFontFeatureSettingsCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontFeatureSettings());

  return blObjectPrivateInitWeakTagged(self, other);
}

BLResult blFontFeatureSettingsDestroy(BLFontFeatureSettingsCore* self) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  return releaseInstance(self);
}

// BLFontFeatureSettings - API - Reset & Clear
// ===========================================

BLResult blFontFeatureSettingsReset(BLFontFeatureSettingsCore* self) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  releaseInstance(self);
  return initSSO(self);
}

BLResult blFontFeatureSettingsClear(BLFontFeatureSettingsCore* self) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  if (self->_d.sso())
    return initSSO(self);

  if (isMutable(self)) {
    BLFontFeatureSettingsImpl* selfI = getImpl(self);
    selfI->size = 0;
    return BL_SUCCESS;
  }
  else {
    releaseInstance(self);
    return initSSO(self);
  }
}

// BLFontFeatureSettings - API - Shrink
// ====================================

BLResult blFontFeatureSettingsShrink(BLFontFeatureSettingsCore* self) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  if (self->_d.sso())
    return BL_SUCCESS;

  BLFontFeatureSettingsImpl* selfI = getImpl(self);
  BLFontFeatureItem* items = selfI->data;
  size_t size = selfI->size;

  BLFontFeatureSettingsCore tmp;
  if (size <= BLFontFeatureSettings::kSSOCapacity && convertItemsToSSO(&tmp, items, size))
    return replaceInstance(self, &tmp);

  BLObjectImplSize currentSize = implSizeFromCapacity(selfI->capacity);
  BLObjectImplSize shrunkSize = implSizeFromCapacity(selfI->size);

  if (shrunkSize + BL_OBJECT_IMPL_ALIGNMENT > currentSize)
    return BL_SUCCESS;

  BL_PROPAGATE(initDynamicFromData(&tmp, shrunkSize, items, size));
  return replaceInstance(self, &tmp);
}

// BLFontFeatureSettings - API - Assign
// ====================================

BLResult blFontFeatureSettingsAssignMove(BLFontFeatureSettingsCore* self, BLFontFeatureSettingsCore* other) noexcept {
  using namespace BLFontFeatureSettingsPrivate;

  BL_ASSERT(self->_d.isFontFeatureSettings());
  BL_ASSERT(other->_d.isFontFeatureSettings());

  BLFontFeatureSettingsCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS]._d;
  return replaceInstance(self, &tmp);
}

BLResult blFontFeatureSettingsAssignWeak(BLFontFeatureSettingsCore* self, const BLFontFeatureSettingsCore* other) noexcept {
  using namespace BLFontFeatureSettingsPrivate;

  BL_ASSERT(self->_d.isFontFeatureSettings());
  BL_ASSERT(other->_d.isFontFeatureSettings());

  blObjectPrivateAddRefTagged(other);
  return replaceInstance(self, other);
}

// BLFontFeatureSettings - API - Accessors
// =======================================

size_t blFontFeatureSettingsGetSize(const BLFontFeatureSettingsCore* self) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  if (self->_d.sso())
    return getSSOSize(self);
  else
    return getImpl(self)->size;
}

size_t blFontFeatureSettingsGetCapacity(const BLFontFeatureSettingsCore* self) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  if (self->_d.sso())
    return BLFontFeatureSettings::kSSOCapacity;
  else
    return getImpl(self)->capacity;
}

BLResult blFontFeatureSettingsGetView(const BLFontFeatureSettingsCore* self, BLFontFeatureSettingsView* out) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  // SSO Mode
  // --------

  if (self->_d.sso()) {
    BLFontFeatureItem* items = out->ssoData;
    size_t size = getSSOSize(self);

    const uint8_t* ssoIds = self->_d.u8_data;
    uint32_t ssoBits = self->_d.info.bits;

    out->data = items;
    out->size = size;

    for (size_t i = 0; i < size; i++, ssoBits >>= 1)
      items[i] = BLFontFeatureItem{BLFontTagsPrivate::featureIdToTagTable[ssoIds[i]], ssoBits & 0x1};

    return BL_SUCCESS;
  }

  // Dynamic Mode
  // ------------

  const BLFontFeatureSettingsImpl* selfI = getImpl(self);
  out->data = selfI->data;
  out->size = selfI->size;
  return BL_SUCCESS;
}

bool blFontFeatureSettingsHasKey(const BLFontFeatureSettingsCore* self, BLTag key) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  // SSO Mode
  // --------

  if (self->_d.sso()) {
    uint32_t id = BLFontTagsPrivate::featureTagToId(key);
    if (id == BLFontTagsPrivate::kInvalidId)
      return false;

    size_t index;
    return findSSOKey(self, id, &index);
  }

  // Dynamic Mode
  // ------------

  const BLFontFeatureSettingsImpl* selfI = getImpl(self);
  const BLFontFeatureItem* data = selfI->data;

  size_t size = selfI->size;
  size_t index = BLAlgorithm::lowerBound(data, selfI->size, key, [](const BLFontFeatureItem& item, uint32_t key) noexcept { return item.tag < key; });

  return index < size && data[index].tag == key;
}

uint32_t blFontFeatureSettingsGetKey(const BLFontFeatureSettingsCore* self, BLTag key) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  // SSO Mode
  // --------

  if (self->_d.sso()) {
    uint32_t id = BLFontTagsPrivate::featureTagToId(key);
    if (id == BLFontTagsPrivate::kInvalidId)
      return BL_FONT_FEATURE_INVALID_VALUE;

    size_t index;
    if (findSSOKey(self, id, &index))
      return getSSOValueAt(self, index);
    else
      return BL_FONT_FEATURE_INVALID_VALUE;
  }

  // Dynamic Mode
  // ------------

  const BLFontFeatureSettingsImpl* selfI = getImpl(self);
  const BLFontFeatureItem* data = selfI->data;

  size_t size = selfI->size;
  size_t index = BLAlgorithm::lowerBound(data, selfI->size, key, [](const BLFontFeatureItem& item, uint32_t key) noexcept { return item.tag < key; });

  if (index < size && data[index].tag == key)
    return data[index].value;
  else
    return BL_FONT_FEATURE_INVALID_VALUE;
}

BLResult blFontFeatureSettingsSetKey(BLFontFeatureSettingsCore* self, BLTag key, uint32_t value) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  if (BL_UNLIKELY(value > 65535u))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  // SSO Mode
  // --------

  bool canModify = true;

  if (self->_d.sso()) {
    size_t size = getSSOSize(self);

    if (value <= 1) {
      uint32_t id = BLFontTagsPrivate::featureTagToId(key);
      if (id != BLFontTagsPrivate::kInvalidId) {
        size_t index;
        if (findSSOKey(self, id, &index)) {
          setSSOValueAt(self, index, value);
          return BL_SUCCESS;
        }

        if (size < BLFontFeatureSettings::kSSOCapacity) {
          // Every inserted key must be inserted in a way to make keys sorted and we know where to insert (index).
          // We must move keys and values too. Since values are represented as bits only, we just need to do a shift.
          uint8_t* ssoIds = self->_d.u8_data;
          size_t nKeysAfterIndex = size - index;
          BLMemOps::copyBackwardInlineT(ssoIds + index + 1u, ssoIds + index, nKeysAfterIndex);
          ssoIds[index] = uint8_t(id);

          // Update the key and object info - updates the size (increments one), adds a new value, and shifts all bits after `index`.
          uint32_t ssoBits = self->_d.info.bits + kSSOSizeIncrement;
          uint32_t keysAfterIndexMask = ((1u << nKeysAfterIndex) - 1u) << index;
          self->_d.info.bits = (ssoBits & ~keysAfterIndexMask) | ((ssoBits & keysAfterIndexMask) << 1u) | (value << index);
          return BL_SUCCESS;
        }
      }
      else {
        if (BL_UNLIKELY(!BLFontTagsPrivate::isTagValid(key)))
          return blTraceError(BL_ERROR_INVALID_VALUE);
      }
    }

    // Turn the SSO settings to dynamic settings, because some (or multiple) cases below are true:
    //   a) The `key` doesn't have a corresponding feature id, thus it cannot be used in SSO mode.
    //   b) The `value` is not either 0 or 1.
    //   c) There is no room in SSO storage to insert another key/value pair.
    BLObjectImplSize implSize = blObjectAlignImplSize(implSizeFromCapacity(blMax<size_t>(size + 1, 4u)));
    BLFontFeatureSettingsCore tmp;

    // NOTE: This will turn the SSO settings into a dynamic settings - it's guaranteed that all further operations will succeed.
    BL_PROPAGATE(initDynamicFromSSO(&tmp, implSize, self));
    *self = tmp;
  }
  else {
    if (BL_UNLIKELY(!BLFontTagsPrivate::isTagValid(key)))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    canModify = isMutable(self);
  }

  // Dynamic Mode
  // ------------

  BLFontFeatureSettingsImpl* selfI = getImpl(self);
  BLFontFeatureItem* items = selfI->data;

  size_t size = selfI->size;
  size_t index = BLAlgorithm::lowerBound(items, size, key, [](const BLFontFeatureItem& item, uint32_t key) noexcept { return item.tag < key; });

  // Overwrite the value if the `key` is already in the settings.
  if (index < size && items[index].tag == key) {
    if (items[index].value == value)
      return BL_SUCCESS;

    if (canModify) {
      items[index].value = value;
      return BL_SUCCESS;
    }
    else {
      BLFontFeatureSettingsCore tmp;
      BL_PROPAGATE(initDynamicFromData(&tmp, implSizeFromCapacity(size), items, size));
      getImpl(&tmp)->data[index].value = value;
      return replaceInstance(self, &tmp);
    }
  }

  if (BL_UNLIKELY(!BLFontTagsPrivate::isTagValid(key)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  // Insert a new key if the `key` is not in the settings.
  size_t nKeysAfterIndex = size - index;
  if (canModify && selfI->capacity > size) {
    BLMemOps::copyBackwardInlineT(items + index + 1, items + index, nKeysAfterIndex);
    items[index] = BLFontFeatureItem{key, value};
    selfI->size = size + 1;
    return BL_SUCCESS;
  }
  else {
    BLFontFeatureSettingsCore tmp;
    BL_PROPAGATE(initDynamic(&tmp, expandImplSize(implSizeFromCapacity(size + 1)), size + 1));

    BLFontFeatureItem* dst = getImpl(&tmp)->data;
    BLMemOps::copyForwardInlineT(dst, items, index);
    dst[index] = BLFontFeatureItem{key, value};
    BLMemOps::copyForwardInlineT(dst + index + 1, items + index, nKeysAfterIndex);

    return replaceInstance(self, &tmp);
  }
}

BLResult blFontFeatureSettingsRemoveKey(BLFontFeatureSettingsCore* self, BLTag key) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  // SSO Mode
  // --------

  if (self->_d.sso()) {
    uint32_t id = BLFontTagsPrivate::featureTagToId(key);
    if (id == BLFontTagsPrivate::kInvalidId)
      return BL_SUCCESS;

    size_t size = getSSOSize(self);
    size_t index;

    if (!findSSOKey(self, id, &index))
      return BL_SUCCESS;

    size_t i = index;
    uint8_t* ssoIds = self->_d.u8_data;

    while (i < size) {
      ssoIds[i] = ssoIds[i + 1];
      i++;
    }

    // Clear the key that has been removed. The reason for doing this is to make sure that two settings that have
    // the same SSO data would be binary equal (there would not be garbage in data after the size in SSO storage).
    ssoIds[size - 1] = uint8_t(0);

    // Shift the bit data representing [0, 1] values so they are in correct places after the removal operation.
    uint32_t ssoBits = self->_d.info.bits;
    uint32_t valuesToShift = uint32_t(size - index - 1);
    uint32_t remainingKeysAfterIndexMask = ((1u << valuesToShift) - 1u) << (index + 1);

    self->_d.info.bits = (ssoBits & ~(BL_OBJECT_INFO_A_MASK | remainingKeysAfterIndexMask | (1u << index))) |
                         ((ssoBits & remainingKeysAfterIndexMask) >> 1) |
                         (uint32_t(size - 1u) << BL_OBJECT_INFO_A_SHIFT);
    return BL_SUCCESS;
  }

  // Dynamic Mode
  // ------------

  BLFontFeatureSettingsImpl* selfI = getImpl(self);
  BLFontFeatureItem* items = selfI->data;

  size_t size = selfI->size;
  size_t index = BLAlgorithm::lowerBound(items, selfI->size, key, [](const BLFontFeatureItem& item, uint32_t key) noexcept { return item.tag < key; });

  if (index >= size || items[index].tag != key)
    return BL_SUCCESS;

  if (isMutable(self)) {
    selfI->size = size - 1;
    BLMemOps::copyForwardInlineT(items + index, items + index + 1, size - index - 1);
    return BL_SUCCESS;
  }
  else {
    BLFontFeatureSettingsCore tmp;
    BL_PROPAGATE(initDynamic(&tmp, expandImplSize(implSizeFromCapacity(size - 1)), size - 1));

    BLFontFeatureItem* dst = getImpl(&tmp)->data;
    BLMemOps::copyForwardInlineT(dst, items, index);
    BLMemOps::copyForwardInlineT(dst + index, items + index + 1, size - index - 1);

    return replaceInstance(self, &tmp);
  }
}

// BLFontFeatureSettings - API - Equals
// ====================================

bool blFontFeatureSettingsEquals(const BLFontFeatureSettingsCore* a, const BLFontFeatureSettingsCore* b) noexcept {
  using namespace BLFontFeatureSettingsPrivate;

  BL_ASSERT(a->_d.isFontFeatureSettings());
  BL_ASSERT(b->_d.isFontFeatureSettings());

  if (blObjectPrivateBinaryEquals(a, b))
    return true;

  if (a->_d.sso() == b->_d.sso()) {
    // Both are SSO: They must be binary equal, if not, they are not equal.
    if (a->_d.sso())
      return false;

    // Both are dynamic.
    const BLFontFeatureSettingsImpl* aImpl = getImpl(a);
    const BLFontFeatureSettingsImpl* bImpl = getImpl(b);

    size_t size = aImpl->size;
    if (size != bImpl->size)
      return false;

    return memcmp(aImpl->data, bImpl->data, size * sizeof(BLFontFeatureItem)) == 0;
  }
  else {
    // One is SSO and one is dynamic, make `a` the SSO one.
    if (b->_d.sso())
      std::swap(a, b);

    const BLFontFeatureSettingsImpl* bImpl = getImpl(b);
    size_t size = getSSOSize(a);

    if (size != bImpl->size)
      return false;

    uint32_t aBits = a->_d.info.bits;
    const uint8_t* aIds = a->_d.u8_data;
    const BLFontFeatureItem* bItems = bImpl->data;

    for (size_t i = 0; i < size; i++, aBits >>= 1) {
      uint32_t aTag = BLFontTagsPrivate::featureIdToTagTable[aIds[i]];
      uint32_t aValue = aBits & 0x1u;

      if (bItems[i].tag != aTag || bItems[i].value != aValue)
        return false;
    }

    return true;
  }
}

// BLFontFeatureSettings - Runtime Registration
// ============================================

void blFontFeatureSettingsRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  // Initialize BLFontFeatureSettings.
  blObjectDefaults[BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS]._d.initStatic(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS, BLObjectInfo{});
}

// BLFontFeatureSettings - Tests
// =============================

#if defined(BL_TEST)
static void verifyFontFeatureSettings(const BLFontFeatureSettings& ffs) noexcept {
  BLFontFeatureSettingsView view;
  ffs.getView(&view);

  if (view.size == 0)
    return;

  uint32_t prevTag = view.data[0].tag;
  for (size_t i = 1; i < view.size; i++) {
    EXPECT_LT(prevTag, view.data[i].tag)
      .message("BLFontFeatureSettings is corrupted - previous tag 0x%08X is not less than current tag 0x%08X at [%zu]", prevTag, view.data[i].tag, i);
  }
}

UNIT(fontfeaturesettings) {
  // These are not sorted on purpose - we want BLFontFeatureSettings to sort them.
  static const uint32_t ssoTags[] = {
    BL_MAKE_TAG('c', '2', 's', 'c'),
    BL_MAKE_TAG('a', 'a', 'l', 't'),
    BL_MAKE_TAG('f', 'l', 'a', 'c'),
    BL_MAKE_TAG('c', 'l', 'i', 'g'),
    BL_MAKE_TAG('d', 'l', 'i', 'g'),
    BL_MAKE_TAG('k', 'e', 'r', 'n'),
    BL_MAKE_TAG('c', 's', 'w', 'h'),
    BL_MAKE_TAG('d', 'n', 'o', 'm'),
    BL_MAKE_TAG('c', 'v', '0', '1'),
    BL_MAKE_TAG('d', 't', 'l', 's'),
    BL_MAKE_TAG('s', 'm', 'p', 'l'),
    BL_MAKE_TAG('a', 'f', 'r', 'c')
  };

  static const uint32_t dynamicTags[] = {
    BL_MAKE_TAG('c', '2', 's', 'c'),
    BL_MAKE_TAG('s', 's', '1', '0'),
    BL_MAKE_TAG('a', 'a', 'l', 't'),
    BL_MAKE_TAG('s', 's', '1', '1'),
    BL_MAKE_TAG('f', 'l', 'a', 'c'),
    BL_MAKE_TAG('s', 's', '1', '2'),
    BL_MAKE_TAG('c', 'l', 'i', 'g'),
    BL_MAKE_TAG('s', 's', '1', '3'),
    BL_MAKE_TAG('d', 'l', 'i', 'g'),
    BL_MAKE_TAG('s', 's', '1', '4'),
    BL_MAKE_TAG('k', 'e', 'r', 'n'),
    BL_MAKE_TAG('s', 's', '1', '5'),
    BL_MAKE_TAG('c', 's', 'w', 'h'),
    BL_MAKE_TAG('s', 's', '1', '6'),
    BL_MAKE_TAG('d', 'n', 'o', 'm'),
    BL_MAKE_TAG('s', 's', '1', '7'),
    BL_MAKE_TAG('c', 'v', '0', '1'),
    BL_MAKE_TAG('s', 's', '1', '8'),
    BL_MAKE_TAG('d', 't', 'l', 's'),
    BL_MAKE_TAG('s', 's', '1', '9'),
    BL_MAKE_TAG('s', 'm', 'p', 'l'),
    BL_MAKE_TAG('s', 's', '2', '0'),
    BL_MAKE_TAG('a', 'f', 'r', 'c')
  };

  INFO("SSO representation");
  {
    BLFontFeatureSettings ffs;

    EXPECT_TRUE(ffs._d.sso());
    EXPECT_TRUE(ffs.empty());
    EXPECT_EQ(ffs.size(), 0u);
    EXPECT_EQ(ffs.capacity(), BLFontFeatureSettings::kSSOCapacity);

    // Getting an unknown key should return invalid value.
    EXPECT_EQ(ffs.getKey(BL_MAKE_TAG('-', '-', '-', '-')), BL_FONT_FEATURE_INVALID_VALUE);

    for (uint32_t i = 0; i < BL_ARRAY_SIZE(ssoTags); i++) {
      EXPECT_SUCCESS(ffs.setKey(ssoTags[i], 1u));
      EXPECT_EQ(ffs.getKey(ssoTags[i]), 1u);
      EXPECT_EQ(ffs.size(), i + 1);
      EXPECT_TRUE(ffs._d.sso());
      verifyFontFeatureSettings(ffs);
    }

    for (uint32_t i = 0; i < BL_ARRAY_SIZE(ssoTags); i++) {
      EXPECT_SUCCESS(ffs.setKey(ssoTags[i], 0u));
      EXPECT_EQ(ffs.getKey(ssoTags[i]), 0u);
      EXPECT_EQ(ffs.size(), BL_ARRAY_SIZE(ssoTags));
      EXPECT_TRUE(ffs._d.sso());
      verifyFontFeatureSettings(ffs);
    }

    for (uint32_t i = 0; i < BL_ARRAY_SIZE(ssoTags); i++) {
      EXPECT_SUCCESS(ffs.removeKey(ssoTags[i]));
      EXPECT_EQ(ffs.getKey(ssoTags[i]), BL_FONT_FEATURE_INVALID_VALUE);
      EXPECT_EQ(ffs.size(), BL_ARRAY_SIZE(ssoTags) - i - 1);
      EXPECT_TRUE(ffs._d.sso());
      verifyFontFeatureSettings(ffs);
    }
  }

  INFO("SSO border cases");
  {
    // First feature ids use R/I bits, which is used for reference counted dynamic objects. What
    // we want to test here is that this bit is not checked when destroying SSO instances.
    BLFontFeatureSettings settings;
    settings.setKey(BLFontTagsPrivate::featureIdToTagTable[0], 1);
    settings.setKey(BLFontTagsPrivate::featureIdToTagTable[1], 1);
  }

  INFO("Dynamic representation");
  {
    BLFontFeatureSettings ffs;

    EXPECT_TRUE(ffs._d.sso());
    EXPECT_TRUE(ffs.empty());
    EXPECT_EQ(ffs.size(), 0u);
    EXPECT_EQ(ffs.capacity(), BLFontFeatureSettings::kSSOCapacity);

    // Getting an unknown key should return invalid value.
    EXPECT_EQ(ffs.getKey(BL_MAKE_TAG('-', '-', '-', '-')), BL_FONT_FEATURE_INVALID_VALUE);

    for (uint32_t i = 0; i < BL_ARRAY_SIZE(dynamicTags); i++) {
      EXPECT_SUCCESS(ffs.setKey(dynamicTags[i], 1u));
      EXPECT_EQ(ffs.getKey(dynamicTags[i]), 1u);
      EXPECT_EQ(ffs.size(), i + 1);
      verifyFontFeatureSettings(ffs);
    }

    EXPECT_FALSE(ffs._d.sso());

    for (uint32_t i = 0; i < BL_ARRAY_SIZE(dynamicTags); i++) {
      EXPECT_SUCCESS(ffs.setKey(dynamicTags[i], 0u));
      EXPECT_EQ(ffs.getKey(dynamicTags[i]), 0u);
      EXPECT_EQ(ffs.size(), BL_ARRAY_SIZE(dynamicTags));
      verifyFontFeatureSettings(ffs);
    }

    EXPECT_FALSE(ffs._d.sso());

    for (uint32_t i = 0; i < BL_ARRAY_SIZE(dynamicTags); i++) {
      EXPECT_SUCCESS(ffs.removeKey(dynamicTags[i]));
      EXPECT_EQ(ffs.getKey(dynamicTags[i]), BL_FONT_FEATURE_INVALID_VALUE);
      EXPECT_EQ(ffs.size(), BL_ARRAY_SIZE(dynamicTags) - i - 1);
      verifyFontFeatureSettings(ffs);
    }

    EXPECT_FALSE(ffs._d.sso());
  }

  INFO("Equality");
  {
    BLFontFeatureSettings ffs1;
    BLFontFeatureSettings ffs2;

    for (uint32_t i = 0; i < BL_ARRAY_SIZE(ssoTags); i++) {
      EXPECT_SUCCESS(ffs1.setKey(ssoTags[i], 1u));
      EXPECT_SUCCESS(ffs2.setKey(ssoTags[BL_ARRAY_SIZE(ssoTags) - i - 1], 1u));
    }

    EXPECT_TRUE(ffs1.equals(ffs2));

    // Make ffs1 go out of SSO mode.
    EXPECT_SUCCESS(ffs1.setKey(BL_MAKE_TAG('a', 'a', 'a', 'a'), 1));
    EXPECT_SUCCESS(ffs1.removeKey(BL_MAKE_TAG('a', 'a', 'a', 'a')));
    EXPECT_TRUE(ffs1.equals(ffs2));

    // Make ffs2 go out of SSO mode.
    EXPECT_SUCCESS(ffs2.setKey(BL_MAKE_TAG('a', 'a', 'a', 'a'), 1));
    EXPECT_SUCCESS(ffs2.removeKey(BL_MAKE_TAG('a', 'a', 'a', 'a')));
    EXPECT_TRUE(ffs1.equals(ffs2));
  }

  INFO("Dynamic memory allocation strategy");
  {
    BLFontFeatureSettings ffs;
    size_t capacity = ffs.capacity();

    constexpr uint32_t kCharRange = BLFontTagsPrivate::kCharRangeInTag;
    constexpr uint32_t kNumItems = BLFontTagsPrivate::kUniqueTagCount;

    for (uint32_t i = 0; i < kNumItems; i++) {
      BLTag key = BL_MAKE_TAG(
        uint32_t(' ') + (i / (kCharRange * kCharRange * kCharRange)),
        uint32_t(' ') + (i / (kCharRange * kCharRange)) % kCharRange,
        uint32_t(' ') + (i / (kCharRange)) % kCharRange,
        uint32_t(' ') + (i % kCharRange));

      ffs.setKey(key, i & 0xFFFFu);
      if (capacity != ffs.capacity()) {
        size_t implSize = BLFontFeatureSettingsPrivate::implSizeFromCapacity(ffs.capacity()).value();
        INFO("  Capacity increased from %zu to %zu [ImplSize=%zu]\n", capacity, ffs.capacity(), implSize);
        capacity = ffs.capacity();
      }
    }

    verifyFontFeatureSettings(ffs);
  }
}
#endif
