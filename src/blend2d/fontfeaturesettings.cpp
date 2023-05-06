// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "fontfeaturesettings_p.h"
#include "fonttagdata_p.h"
#include "object_p.h"
#include "runtime_p.h"
#include "string_p.h"
#include "support/algorithm_p.h"
#include "support/bitops_p.h"
#include "support/memops_p.h"
#include "support/ptrops_p.h"

namespace BLFontFeatureSettingsPrivate {

// BLFontFeatureSettings - SSO Utilities
// =====================================

static BL_INLINE BLResult initSSO(BLFontFeatureSettingsCore* self, size_t size = 0) noexcept {
  self->_d.initStatic(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS, BLObjectInfo::packYzcpFields(uint32_t(size)));
  self->_d.u32_data[2] = kSSOInvalidFatFeaturePattern;
  return BL_SUCCESS;
}

static BL_INLINE size_t getSSOSize(const BLFontFeatureSettingsCore* self) noexcept {
  return self->_d.info.yField();
}

static BL_INLINE void setSSOSize(BLFontFeatureSettingsCore* self, size_t size) noexcept {
  self->_d.info.setYField(uint32_t(size));
}

static BL_INLINE void addSSOBitTag(BLFontFeatureSettingsCore* self, uint32_t index, uint32_t value) noexcept {
  uint32_t bit = 1u << index;

  BL_ASSERT((self->_d.u32_data[0] & bit) == 0u);
  BL_ASSERT((self->_d.u32_data[1] & bit) == 0u);

  self->_d.u32_data[0] |= bit;
  self->_d.u32_data[1] |= value << index;
  self->_d.info.bits += 1u << BL_OBJECT_INFO_Y_SHIFT;
}

static BL_INLINE void updateSSOBitValue(BLFontFeatureSettingsCore* self, uint32_t index, uint32_t value) noexcept {
  uint32_t bit = 1u << index;

  BL_ASSERT((self->_d.u32_data[0] & bit) != 0u);

  self->_d.u32_data[1] = (self->_d.u32_data[1] & ~bit) | (value << index);
}

static BL_INLINE void removeSSOBitTag(BLFontFeatureSettingsCore* self, uint32_t index) noexcept {
  uint32_t bit = 1u << index;

  BL_ASSERT(self->_d.info.yField() > 0u);
  BL_ASSERT((self->_d.u32_data[0] & bit) != 0u);

  self->_d.u32_data[0] &= ~bit;
  self->_d.u32_data[1] &= ~bit;
  self->_d.info.bits -= 1u << BL_OBJECT_INFO_Y_SHIFT;
}

static BL_INLINE void addSSOFatTag(BLFontFeatureSettingsCore* self, uint32_t index, uint32_t featureId, uint32_t value) noexcept {
  BL_ASSERT(index < kSSOFatFeatureCount);
  BL_ASSERT(featureId < kSSOInvalidFatFeatureId);
  BL_ASSERT(value <= kSSOFatFeatureValueBitMask);

  constexpr uint32_t kValueDataMask = (1u << (kSSOFatFeatureCount * kSSOFatFeatureValueBitCount)) - 1u;

  uint32_t tagOffset = index * kSSOFatFeatureTagBitCount;
  uint32_t valOffset = index * kSSOFatFeatureValueBitCount;

  uint32_t tags = self->_d.u32_data[2];
  uint32_t vals = self->_d.info.bits & kValueDataMask;

  uint32_t tagsLsbMask = ((1u << tagOffset) - 1u);
  uint32_t valsLsbMask = ((1u << valOffset) - 1u);

  tags = (tags & tagsLsbMask) | ((tags & ~tagsLsbMask) << kSSOFatFeatureTagBitCount) | (featureId << tagOffset);
  vals = (vals & valsLsbMask) | ((vals & ~valsLsbMask) << kSSOFatFeatureValueBitCount) | (value << valOffset);

  self->_d.u32_data[2] = tags;
  self->_d.info.bits = ((self->_d.info.bits & ~kValueDataMask) + (1u << BL_OBJECT_INFO_Y_SHIFT)) | (vals & kValueDataMask);
}

static BL_INLINE void updateSSOFatValue(BLFontFeatureSettingsCore* self, uint32_t index, uint32_t value) noexcept {
  BL_ASSERT(index < kSSOFatFeatureCount);
  BL_ASSERT(value <= kSSOFatFeatureValueBitMask);

  uint32_t valueOffset = index * kSSOFatFeatureValueBitCount;
  uint32_t mask = kSSOFatFeatureValueBitMask << valueOffset;

  self->_d.info.bits = (self->_d.info.bits & ~mask) | value << valueOffset;
}

static BL_INLINE void removeSSOFatTag(BLFontFeatureSettingsCore* self, uint32_t index) noexcept {
  BL_ASSERT(self->_d.info.yField() > 0u);
  BL_ASSERT(index < kSSOFatFeatureCount);

  constexpr uint32_t kValueDataMask = (1u << (kSSOFatFeatureCount * kSSOFatFeatureValueBitCount)) - 1u;

  uint32_t tagOffset = index * kSSOFatFeatureTagBitCount;
  uint32_t valOffset = index * kSSOFatFeatureValueBitCount;

  uint32_t tags = self->_d.u32_data[2];
  uint32_t vals = self->_d.info.bits & kValueDataMask;

  uint32_t tagsLsbMask = ((1u << tagOffset) - 1u);
  uint32_t valsLsbMask = ((1u << valOffset) - 1u);

  tags = (tags & tagsLsbMask) | ((tags >> kSSOFatFeatureTagBitCount) & ~tagsLsbMask) | (kSSOInvalidFatFeatureId << ((kSSOFatFeatureCount - 1) * kSSOFatFeatureTagBitCount));
  vals = (vals & valsLsbMask) | ((vals >> kSSOFatFeatureValueBitCount) & ~tagsLsbMask);

  self->_d.u32_data[2] = tags;
  self->_d.info.bits = ((self->_d.info.bits & ~kValueDataMask) - (1u << BL_OBJECT_INFO_Y_SHIFT)) | (vals & kValueDataMask);
}

static BL_INLINE bool canInsertSSOFatTag(const BLFontFeatureSettingsCore* self) noexcept {
  uint32_t lastId = self->_d.u32_data[2] >> ((kSSOFatFeatureCount - 1u) * kSSOFatFeatureTagBitCount);
  return lastId == kSSOInvalidFatFeatureId;
}

static bool convertItemsToSSO(BLFontFeatureSettingsCore* self, const BLFontFeatureItem* items, size_t size) noexcept {
  BL_ASSERT(size <= BLFontFeatureSettings::kSSOCapacity);

  uint32_t infoBits = (BLObjectInfo::packType(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS) | BLObjectInfo::packYzcpFields(uint32_t(size))).bits | BL_OBJECT_INFO_MARKER_FLAG;

  uint32_t bitTagIds = 0;
  uint32_t bitValues = 0;

  uint32_t fatIndex = 0;
  uint32_t fatTagIds = kSSOInvalidFatFeaturePattern;
  uint32_t fatValues = infoBits;

  for (size_t i = 0; i < size; i++) {
    uint32_t id = BLFontTagData::featureTagToId(items[i].tag);
    uint32_t value = items[i].value;

    if (id == BLFontTagData::kInvalidId)
      return false;

    FeatureInfo featureInfo = BLFontTagData::featureInfoTable[id];
    if (featureInfo.hasBitId()) {
      if (value > 1u)
        return false;

      uint32_t bitId = featureInfo.bitId;
      bitTagIds |= uint32_t(1) << bitId;
      bitValues |= uint32_t(value) << bitId;
    }
    else {
      if (value > kSSOFatFeatureValueBitMask || fatIndex >= kSSOFatFeatureCount)
        return false;

      fatTagIds ^= (id ^ kSSOInvalidFatFeatureId) << (fatIndex * kSSOFatFeatureTagBitCount);
      fatValues |= value << (fatIndex * kSSOFatFeatureValueBitCount);
      fatIndex++;
    }
  }

  self->_d.u32_data[0] = bitTagIds;
  self->_d.u32_data[1] = bitValues;
  self->_d.u32_data[2] = fatTagIds;
  self->_d.u32_data[3] = fatValues;

  return true;
}

static void convertSSOToItems(const BLFontFeatureSettingsCore* self, BLFontFeatureItem* items) noexcept {
  constexpr uint32_t kDummyFatTagId = 0xFFFFFFFFu;

  uint32_t bitTagIds = self->_d.u32_data[0];
  uint32_t bitValues = self->_d.u32_data[1];
  uint32_t fatTagIds = self->_d.u32_data[2];
  uint32_t fatValues = self->_d.info.bits;
  uint32_t fatFeatureTagId = fatTagIds & kSSOFatFeatureTagBitMask;

  // Marks the end of fat tags (since we have removed one we don't have to check for the end, this is the end).
  fatTagIds >>= kSSOFatFeatureTagBitCount;
  fatTagIds |= kSSOInvalidFatFeatureId << ((kSSOFatFeatureCount - 1u) * kSSOFatFeatureTagBitCount);

  if (fatFeatureTagId == kSSOInvalidFatFeatureId)
    fatFeatureTagId = kDummyFatTagId;

  BLParametrizedBitOps<BLBitOrder::kLSB, uint32_t>::BitIterator bitIterator(bitTagIds);
  while (bitIterator.hasNext()) {
    uint32_t bitIndex = bitIterator.next();
    uint32_t bitFeatureTagId = uint32_t(BLFontTagData::featureBitIdToFeatureId(bitIndex));
    while (bitFeatureTagId > fatFeatureTagId) {
      *items++ = BLFontFeatureItem{BLFontTagData::featureIdToTagTable[fatFeatureTagId], fatValues & kSSOFatFeatureValueBitMask};

      fatFeatureTagId = fatTagIds & kSSOFatFeatureTagBitMask;
      if (fatFeatureTagId == kSSOInvalidFatFeatureId)
        fatFeatureTagId = kDummyFatTagId;

      fatTagIds >>= kSSOFatFeatureTagBitCount;
      fatValues >>= kSSOFatFeatureValueBitCount;
    }

    *items++ = BLFontFeatureItem{BLFontTagData::featureIdToTagTable[bitFeatureTagId], (bitValues >> bitIndex) & 0x1u};
  }

  if (fatFeatureTagId == kDummyFatTagId)
    return;

  do {
    *items++ = BLFontFeatureItem{BLFontTagData::featureIdToTagTable[fatFeatureTagId], fatValues & kSSOFatFeatureValueBitMask};
    fatFeatureTagId = fatTagIds & kSSOFatFeatureTagBitMask;
    fatTagIds >>= kSSOFatFeatureTagBitCount;
    fatValues >>= kSSOFatFeatureValueBitCount;
  } while (fatFeatureTagId != kSSOInvalidFatFeatureId);
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
  return BLFontTagData::kUniqueTagCount;
}

static BL_INLINE BLObjectImplSize expandImplSize(BLObjectImplSize implSize) noexcept {
  return blObjectExpandImplSize(implSize);
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

  convertSSOToItems(ssoMap, items);
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

  blObjectPrivateAddRefIfRCObject(other);
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

    out->data = items;
    out->size = size;

    if (!size)
      return BL_SUCCESS;

    convertSSOToItems(self, items);
    return BL_SUCCESS;
  }

  // Dynamic Mode
  // ------------

  const BLFontFeatureSettingsImpl* selfI = getImpl(self);
  out->data = selfI->data;
  out->size = selfI->size;
  return BL_SUCCESS;
}

bool blFontFeatureSettingsHasValue(const BLFontFeatureSettingsCore* self, BLTag featureTag) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  // SSO Mode
  // --------

  if (self->_d.sso()) {
    uint32_t id = BLFontTagData::featureTagToId(featureTag);
    if (id == BLFontTagData::kInvalidId)
      return false;

    FeatureInfo featureInfo = BLFontTagData::featureInfoTable[id];
    if (featureInfo.hasBitId()) {
      return hasSSOBitTag(self, featureInfo.bitId);
    }
    else {
      uint32_t dummyIndex;
      return findSSOFatTag(self, featureInfo.bitId, &dummyIndex);
    }
  }

  // Dynamic Mode
  // ------------

  const BLFontFeatureSettingsImpl* selfI = getImpl(self);
  const BLFontFeatureItem* data = selfI->data;

  size_t size = selfI->size;
  size_t index = BLAlgorithm::lowerBound(data, selfI->size, featureTag, [](const BLFontFeatureItem& item, uint32_t tag) noexcept { return item.tag < tag; });

  return index < size && data[index].tag == featureTag;
}

uint32_t blFontFeatureSettingsGetValue(const BLFontFeatureSettingsCore* self, BLTag featureTag) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  if (self->_d.sso())
    return getSSOTagValue(self, featureTag);
  else
    return getDynamicTagValue(self, featureTag);
}

BLResult blFontFeatureSettingsSetValue(BLFontFeatureSettingsCore* self, BLTag featureTag, uint32_t value) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  if (BL_UNLIKELY(value > 65535u))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  uint32_t featureId = BLFontTagData::featureTagToId(featureTag);
  bool canModify = true;

  // SSO Mode
  // --------

  if (self->_d.sso()) {
    size_t size = getSSOSize(self);

    if (featureId != BLFontTagData::kInvalidId) {
      FeatureInfo featureInfo = BLFontTagData::featureInfoTable[featureId];
      if (featureInfo.hasBitId()) {
        if (value > 1u)
          return blTraceError(BL_ERROR_INVALID_VALUE);

        uint32_t featureBitId = featureInfo.bitId;
        if (hasSSOBitTag(self, featureBitId)) {
          updateSSOBitValue(self, featureBitId, value);
          return BL_SUCCESS;
        }
        else {
          addSSOBitTag(self, featureBitId, value);
          return BL_SUCCESS;
        }
      }
      else if (value <= kSSOFatFeatureTagBitMask) {
        uint32_t index;
        if (findSSOFatTag(self, featureId, &index)) {
          updateSSOFatValue(self, index, value);
          return BL_SUCCESS;
        }
        else if (canInsertSSOFatTag(self)) {
          addSSOFatTag(self, index, featureId, value);
          return BL_SUCCESS;
        }
      }
    }
    else {
      if (BL_UNLIKELY(!BLFontTagData::isValidTag(featureTag)))
        return blTraceError(BL_ERROR_INVALID_VALUE);
    }

    // Turn the SSO settings to dynamic settings, because some (or multiple) cases below are true:
    //   a) The `featureTag` doesn't have a corresponding feature id, thus it cannot be used in SSO mode.
    //   b) The `value` is not either 0 or 1.
    //   c) There is no room in SSO storage to insert another tag/value pair.
    BLObjectImplSize implSize = blObjectAlignImplSize(implSizeFromCapacity(blMax<size_t>(size + 1, 4u)));
    BLFontFeatureSettingsCore tmp;

    // NOTE: This will turn the SSO settings into dynamic settings - it's guaranteed that all further operations will succeed.
    BL_PROPAGATE(initDynamicFromSSO(&tmp, implSize, self));
    *self = tmp;
  }
  else {
    if (BL_UNLIKELY(!BLFontTagData::isValidTag(featureTag)))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    canModify = isMutable(self);
  }

  // Dynamic Mode
  // ------------

  BLFontFeatureSettingsImpl* selfI = getImpl(self);
  BLFontFeatureItem* items = selfI->data;

  size_t size = selfI->size;
  size_t index = BLAlgorithm::lowerBound(items, size, featureTag, [](const BLFontFeatureItem& item, uint32_t tag) noexcept { return item.tag < tag; });

  // Overwrite the value if `featureTag` is already in the settings.
  if (index < size && items[index].tag == featureTag) {
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

  if (BL_UNLIKELY(!BLFontTagData::isValidTag(featureTag)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  // Insert a new tag/value pair if `featureTag` is not in the settings.
  size_t nTagsAfterIndex = size - index;
  if (canModify && selfI->capacity > size) {
    BLMemOps::copyBackwardInlineT(items + index + 1, items + index, nTagsAfterIndex);
    items[index] = BLFontFeatureItem{featureTag, value};
    selfI->size = size + 1;
    return BL_SUCCESS;
  }
  else {
    BLFontFeatureSettingsCore tmp;
    BL_PROPAGATE(initDynamic(&tmp, expandImplSize(implSizeFromCapacity(size + 1)), size + 1));

    BLFontFeatureItem* dst = getImpl(&tmp)->data;
    BLMemOps::copyForwardInlineT(dst, items, index);
    dst[index] = BLFontFeatureItem{featureTag, value};
    BLMemOps::copyForwardInlineT(dst + index + 1, items + index, nTagsAfterIndex);

    return replaceInstance(self, &tmp);
  }
}

BLResult blFontFeatureSettingsRemoveValue(BLFontFeatureSettingsCore* self, BLTag featureTag) noexcept {
  using namespace BLFontFeatureSettingsPrivate;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  // SSO Mode
  // --------

  if (self->_d.sso()) {
    uint32_t featureId = BLFontTagData::featureTagToId(featureTag);
    if (featureId == BLFontTagData::kInvalidId)
      return BL_SUCCESS;

    FeatureInfo featureInfo = BLFontTagData::featureInfoTable[featureId];
    if (featureInfo.hasBitId()) {
      uint32_t featureBitId = featureInfo.bitId;
      if (!hasSSOBitTag(self, featureBitId))
        return BL_SUCCESS;

      removeSSOBitTag(self, featureBitId);
      return BL_SUCCESS;
    }
    else {
      uint32_t index;
      if (!findSSOFatTag(self, featureId, &index))
        return BL_SUCCESS;

      removeSSOFatTag(self, index);
      return BL_SUCCESS;
    }
  }

  // Dynamic Mode
  // ------------

  BLFontFeatureSettingsImpl* selfI = getImpl(self);
  BLFontFeatureItem* items = selfI->data;

  size_t size = selfI->size;
  size_t index = BLAlgorithm::lowerBound(items, selfI->size, featureTag, [](const BLFontFeatureItem& item, uint32_t tag) noexcept { return item.tag < tag; });

  if (index >= size || items[index].tag != featureTag)
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

    // NOTE: Since SSO representation is not that trivial, just try to convert B impl to SSO representation
    // and then try binary equality of two SSO instances. If B is not convertible, then A and B are not equal.
    BLFontFeatureSettingsCore bSSO;
    const BLFontFeatureItem* bItems = bImpl->data;

    BL_ASSERT(size <= BLFontFeatureSettings::kSSOCapacity);
    if (!convertItemsToSSO(&bSSO, bItems, size))
      return false;

    return blObjectPrivateBinaryEquals(a, &bSSO);
  }
}

// BLFontFeatureSettings - Runtime Registration
// ============================================

void blFontFeatureSettingsRtInit(BLRuntimeContext* rt) noexcept {
  using namespace BLFontFeatureSettingsPrivate;

  blUnused(rt);

  // Initialize BLFontFeatureSettings.
  initSSO(static_cast<BLFontFeatureSettingsCore*>(&blObjectDefaults[BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS]));
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
    prevTag = view.data[i].tag;
  }
}

/*
static void printFontFeatureSettings(const BLFontFeatureSettings& ffs) noexcept {
  BLFontFeatureSettingsView view;
  ffs.getView(&view);

  printf("FeatureSettings [size=%zu]:\n", view.size);

  if (view.size == 0)
    return;

  for (size_t i = 0; i < view.size; i++) {
    printf("  %c%c%c%c=%u\n", (view.data[i].tag >> 24) & 0xFF, (view.data[i].tag >> 16) & 0xFF, (view.data[i].tag >> 8) & 0xFF, view.data[i].tag & 0xFF, unsigned(view.data[i].value));
  }
}
*/

UNIT(fontfeaturesettings, BL_TEST_GROUP_TEXT_CONTAINERS) {
  // These are not sorted on purpose to test whether BLFontFeatureSettings would sort them during insertion.
  static const uint32_t fatTags[] = {
    BL_MAKE_TAG('r', 'a', 'n', 'd'),
    BL_MAKE_TAG('a', 'a', 'l', 't'),
    BL_MAKE_TAG('s', 's', '0', '9'),
    BL_MAKE_TAG('s', 's', '0', '4')
  };

  INFO("SSO initial state");
  {
    BLFontFeatureSettings ffs;

    EXPECT_TRUE(ffs._d.sso());
    EXPECT_TRUE(ffs.empty());
    EXPECT_EQ(ffs.size(), 0u);
    EXPECT_EQ(ffs.capacity(), BLFontFeatureSettings::kSSOCapacity);

    // SSO mode should present all available features as invalid (unassigned).
    for (uint32_t featureId = 0; featureId < BLFontTagData::kFeatureIdCount; featureId++) {
      BLTag featureTag = BLFontTagData::featureIdToTagTable[featureId];
      EXPECT_EQ(ffs.getValue(featureTag), BL_FONT_FEATURE_INVALID_VALUE);
    }

    // Trying to get an unknown tag should fail.
    EXPECT_EQ(ffs.getValue(BL_MAKE_TAG('-', '-', '-', '-')), BL_FONT_FEATURE_INVALID_VALUE);
    EXPECT_EQ(ffs.getValue(BL_MAKE_TAG('a', 'a', 'a', 'a')), BL_FONT_FEATURE_INVALID_VALUE);
    EXPECT_EQ(ffs.getValue(BL_MAKE_TAG('z', 'z', 'z', 'z')), BL_FONT_FEATURE_INVALID_VALUE);
  }

  INFO("SSO bit tag/value storage");
  {
    BLFontFeatureSettings ffs;

    // SSO storage must allow to store ALL font features that have bit mapping.
    uint32_t numTags = 0;
    for (uint32_t featureId = 0; featureId < BLFontTagData::kFeatureIdCount; featureId++) {
      if (BLFontTagData::featureInfoTable[featureId].hasBitId()) {
        numTags++;
        BLTag featureTag = BLFontTagData::featureIdToTagTable[featureId];

        EXPECT_SUCCESS(ffs.setValue(featureTag, 1u));
        EXPECT_EQ(ffs.getValue(featureTag), 1u);
        EXPECT_EQ(ffs.size(), numTags);
        EXPECT_TRUE(ffs._d.sso());

        verifyFontFeatureSettings(ffs);
      }
    }

    // Set all features to zero (disabled, but still present in the mapping).
    for (uint32_t featureId = 0; featureId < BLFontTagData::kFeatureIdCount; featureId++) {
      if (BLFontTagData::featureInfoTable[featureId].hasBitId()) {
        BLTag featureTag = BLFontTagData::featureIdToTagTable[featureId];

        EXPECT_SUCCESS(ffs.setValue(featureTag, 0u));
        EXPECT_EQ(ffs.getValue(featureTag), 0u);
        EXPECT_EQ(ffs.size(), numTags);
        EXPECT_TRUE(ffs._d.sso());
        verifyFontFeatureSettings(ffs);
      }
    }

    // Remove all features.
    for (uint32_t featureId = 0; featureId < BLFontTagData::kFeatureIdCount; featureId++) {
      if (BLFontTagData::featureInfoTable[featureId].hasBitId()) {
        numTags--;
        BLTag featureTag = BLFontTagData::featureIdToTagTable[featureId];

        EXPECT_SUCCESS(ffs.removeValue(featureTag));
        EXPECT_EQ(ffs.getValue(featureTag), BL_FONT_FEATURE_INVALID_VALUE);
        EXPECT_EQ(ffs.size(), numTags);
        EXPECT_TRUE(ffs._d.sso());

        verifyFontFeatureSettings(ffs);
      }
    }

    EXPECT_TRUE(ffs.empty());
    EXPECT_EQ(ffs, BLFontFeatureSettings());
  }

  INFO("SSO bit tag/value storage limitations");
  {
    BLFontFeatureSettings ffs;

    // Trying to set any other value than 0-1 with bit tags fails.
    for (uint32_t featureId = 0; featureId < BLFontTagData::kFeatureIdCount; featureId++) {
      if (BLFontTagData::featureInfoTable[featureId].hasBitId()) {
        BLTag featureTag = BLFontTagData::featureIdToTagTable[featureId];
        EXPECT_EQ(ffs.setValue(featureTag, 2u), BL_ERROR_INVALID_VALUE);
      }
    }

    EXPECT_TRUE(ffs.empty());
  }

  INFO("SSO bit tag/value storage + fat tag/value storage");
  {
    BLFontFeatureSettings ffs;
    uint32_t numTags = 0;

    // Add fat tag/value data.
    for (uint32_t i = 0; i < BL_ARRAY_SIZE(fatTags); i++) {
      numTags++;

      EXPECT_SUCCESS(ffs.setValue(fatTags[i], 15u));
      EXPECT_EQ(ffs.getValue(fatTags[i]), 15u);
      EXPECT_EQ(ffs.size(), numTags);
      EXPECT_TRUE(ffs._d.sso());

      verifyFontFeatureSettings(ffs);

      // Verify that changing a fat tag's value is working properly (it's bit twiddling).
      EXPECT_SUCCESS(ffs.setValue(fatTags[i], 1u));
      EXPECT_EQ(ffs.getValue(fatTags[i]), 1u);
      EXPECT_EQ(ffs.size(), numTags);
      EXPECT_TRUE(ffs._d.sso());

      verifyFontFeatureSettings(ffs);
    }

    // Add bit tag/value data.
    for (uint32_t featureId = 0; featureId < BLFontTagData::kFeatureIdCount; featureId++) {
      if (BLFontTagData::featureInfoTable[featureId].hasBitId()) {
        numTags++;
        BLTag featureTag = BLFontTagData::featureIdToTagTable[featureId];

        EXPECT_SUCCESS(ffs.setValue(featureTag, 1u));
        EXPECT_EQ(ffs.getValue(featureTag), 1u);
        EXPECT_EQ(ffs.size(), numTags);
        EXPECT_TRUE(ffs._d.sso());

        verifyFontFeatureSettings(ffs);
      }
    }

    // Remove fat tag/value data.
    for (uint32_t i = 0; i < BL_ARRAY_SIZE(fatTags); i++) {
      numTags--;

      EXPECT_SUCCESS(ffs.removeValue(fatTags[i]));
      EXPECT_EQ(ffs.size(), numTags);
      EXPECT_TRUE(ffs._d.sso());

      verifyFontFeatureSettings(ffs);
    }

    // Remove bit tag/value data.
    for (uint32_t featureId = 0; featureId < BLFontTagData::kFeatureIdCount; featureId++) {
      if (BLFontTagData::featureInfoTable[featureId].hasBitId()) {
        numTags--;
        BLTag featureTag = BLFontTagData::featureIdToTagTable[featureId];

        EXPECT_SUCCESS(ffs.removeValue(featureTag));
        EXPECT_EQ(ffs.size(), numTags);
        EXPECT_TRUE(ffs._d.sso());

        verifyFontFeatureSettings(ffs);
      }
    }

    EXPECT_TRUE(ffs.empty());
    EXPECT_EQ(ffs, BLFontFeatureSettings());
  }

  INFO("SSO tag/value equality");
  {
    BLFontFeatureSettings ffsA;
    BLFontFeatureSettings ffsB;

    // Assign bit tag/value data.
    for (uint32_t i = 0; i < BLFontTagData::kFeatureIdCount; i++) {
      uint32_t featureId = i;
      if (BLFontTagData::featureInfoTable[featureId].hasBitId()) {
        BLTag featureTag = BLFontTagData::featureIdToTagTable[featureId];
        EXPECT_SUCCESS(ffsA.setValue(featureTag, 1u));
        verifyFontFeatureSettings(ffsA);
      }
    }

    for (uint32_t i = 0; i < BLFontTagData::kFeatureIdCount; i++) {
      uint32_t featureId = BLFontTagData::kFeatureIdCount - 1 - i;
      if (BLFontTagData::featureInfoTable[featureId].hasBitId()) {
        BLTag featureTag = BLFontTagData::featureIdToTagTable[featureId];
        EXPECT_SUCCESS(ffsB.setValue(featureTag, 1u));
        verifyFontFeatureSettings(ffsB);
      }
    }

    EXPECT_EQ(ffsA, ffsB);

    // Assign fat tag/value data.
    for (uint32_t i = 0; i < BL_ARRAY_SIZE(fatTags); i++) {
      EXPECT_SUCCESS(ffsA.setValue(fatTags[i], i));
      verifyFontFeatureSettings(ffsA);
    }

    for (uint32_t iRev = 0; iRev < BL_ARRAY_SIZE(fatTags); iRev++) {
      uint32_t i = BL_ARRAY_SIZE(fatTags) - 1 - iRev;
      EXPECT_SUCCESS(ffsB.setValue(fatTags[i], i));
      verifyFontFeatureSettings(ffsB);
    }

    EXPECT_EQ(ffsA, ffsB);

    // Remove fat tag/value data.
    for (uint32_t i = 0; i < BL_ARRAY_SIZE(fatTags); i++) {
      EXPECT_SUCCESS(ffsA.removeValue(fatTags[i]));
      verifyFontFeatureSettings(ffsA);
    }

    for (uint32_t iRev = 0; iRev < BL_ARRAY_SIZE(fatTags); iRev++) {
      uint32_t i = BL_ARRAY_SIZE(fatTags) - 1 - iRev;
      EXPECT_SUCCESS(ffsB.removeValue(fatTags[i]));
      verifyFontFeatureSettings(ffsB);
    }

    EXPECT_EQ(ffsA, ffsB);
  }

  INFO("Dynamic representation");
  {
    BLFontFeatureSettings ffs;

    for (uint32_t i = 0; i < BLFontTagData::kFeatureIdCount; i++) {
      uint32_t featureId = BLFontTagData::kFeatureIdCount - 1 - i;
      BLTag featureTag = BLFontTagData::featureIdToTagTable[featureId];
      EXPECT_SUCCESS(ffs.setValue(featureTag, 1u));
      EXPECT_EQ(ffs.getValue(featureTag), 1u);
      EXPECT_EQ(ffs.size(), i + 1u);
      verifyFontFeatureSettings(ffs);
    }

    EXPECT_FALSE(ffs._d.sso());

    for (uint32_t i = 0; i < BLFontTagData::kFeatureIdCount; i++) {
      uint32_t featureId = BLFontTagData::kFeatureIdCount - 1 - i;
      BLTag featureTag = BLFontTagData::featureIdToTagTable[featureId];

      if (!BLFontTagData::featureInfoTable[featureId].hasBitId()) {
        EXPECT_SUCCESS(ffs.setValue(featureTag, 65535u));
        EXPECT_EQ(ffs.getValue(featureTag), 65535u);
      }
      else {
        EXPECT_SUCCESS(ffs.setValue(featureTag, 0u));
        EXPECT_EQ(ffs.getValue(featureTag), 0u);
      }

      verifyFontFeatureSettings(ffs);
    }

    EXPECT_FALSE(ffs._d.sso());

    for (uint32_t i = 0; i < BLFontTagData::kFeatureIdCount; i++) {
      uint32_t featureId = i;
      BLTag featureTag = BLFontTagData::featureIdToTagTable[featureId];

      EXPECT_SUCCESS(ffs.removeValue(featureTag));
      EXPECT_EQ(ffs.getValue(featureTag), BL_FONT_FEATURE_INVALID_VALUE);

      verifyFontFeatureSettings(ffs);
    }

    EXPECT_TRUE(ffs.empty());
    EXPECT_EQ(ffs.size(), 0u);
    EXPECT_FALSE(ffs._d.sso());

  }

  INFO("Dynamic tag/value equality");
  {
    BLFontFeatureSettings ffs1;
    BLFontFeatureSettings ffs2;

    for (uint32_t i = 0; i < BLFontTagData::kFeatureIdCount; i++) {
      EXPECT_SUCCESS(ffs1.setValue(BLFontTagData::featureIdToTagTable[i], 1u));
      EXPECT_SUCCESS(ffs2.setValue(BLFontTagData::featureIdToTagTable[BLFontTagData::kFeatureIdCount - 1u - i], 1u));

      verifyFontFeatureSettings(ffs1);
      verifyFontFeatureSettings(ffs2);
    }

    EXPECT_EQ(ffs1, ffs2);
  }

  INFO("Dynamic tag/value vs SSO tag/value equality");
  {
    BLFontFeatureSettings ffs1;
    BLFontFeatureSettings ffs2;

    for (uint32_t i = 0; i < BLFontTagData::kFeatureIdCount; i++) {
      if (BLFontTagData::featureInfoTable[i].hasBitId()) {
        EXPECT_SUCCESS(ffs1.setValue(BLFontTagData::featureIdToTagTable[i], 1u));
        EXPECT_SUCCESS(ffs2.setValue(BLFontTagData::featureIdToTagTable[i], 1u));

        verifyFontFeatureSettings(ffs1);
        verifyFontFeatureSettings(ffs2);
      }
    }

    EXPECT_EQ(ffs1, ffs2);

    // Make ffs1 go out of SSO mode.
    EXPECT_SUCCESS(ffs1.setValue(BL_MAKE_TAG('a', 'a', 'a', 'a'), 1000));
    EXPECT_SUCCESS(ffs1.removeValue(BL_MAKE_TAG('a', 'a', 'a', 'a')));
    EXPECT_EQ(ffs1, ffs2);
    EXPECT_EQ(ffs2, ffs1);

    // Make ffs2 go out of SSO mode.
    EXPECT_SUCCESS(ffs2.setValue(BL_MAKE_TAG('a', 'a', 'a', 'a'), 1000));
    EXPECT_SUCCESS(ffs2.removeValue(BL_MAKE_TAG('a', 'a', 'a', 'a')));
    EXPECT_EQ(ffs1, ffs2);
    EXPECT_EQ(ffs2, ffs1);
  }

  INFO("Dynamic memory allocation strategy");
  {
    BLFontFeatureSettings ffs;
    size_t capacity = ffs.capacity();

    constexpr uint32_t kCharRange = BLFontTagData::kCharRangeInTag;
    constexpr uint32_t kNumItems = BLFontTagData::kUniqueTagCount / 100;

    for (uint32_t i = 0; i < kNumItems; i++) {
      BLTag tag = BL_MAKE_TAG(
        uint32_t(' ') + (i / (kCharRange * kCharRange * kCharRange)),
        uint32_t(' ') + (i / (kCharRange * kCharRange)) % kCharRange,
        uint32_t(' ') + (i / (kCharRange)) % kCharRange,
        uint32_t(' ') + (i % kCharRange));

      ffs.setValue(tag, i & 0xFFFFu);
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
