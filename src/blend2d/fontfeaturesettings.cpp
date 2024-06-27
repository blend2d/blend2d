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

namespace bl {
namespace FontFeatureSettingsInternal {

// bl::FontFeatureSettings - SSO Utilities
// =======================================

static BL_INLINE BLResult initSSO(BLFontFeatureSettingsCore* self, size_t size = 0) noexcept {
  self->_d.initStatic(BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS) | BLObjectInfo::fromAbcp(uint32_t(size)));
  self->_d.u32_data[2] = kSSOInvalidFatFeaturePattern;
  return BL_SUCCESS;
}

static BL_INLINE size_t getSSOSize(const BLFontFeatureSettingsCore* self) noexcept {
  return self->_d.info.aField();
}

static BL_INLINE void setSSOSize(BLFontFeatureSettingsCore* self, size_t size) noexcept {
  self->_d.info.setAField(uint32_t(size));
}

static BL_INLINE void addSSOBitTag(BLFontFeatureSettingsCore* self, uint32_t index, uint32_t value) noexcept {
  uint32_t bit = 1u << index;

  BL_ASSERT((self->_d.u32_data[0] & bit) == 0u);
  BL_ASSERT((self->_d.u32_data[1] & bit) == 0u);

  self->_d.u32_data[0] |= bit;
  self->_d.u32_data[1] |= value << index;
  self->_d.info.bits += 1u << BL_OBJECT_INFO_A_SHIFT;
}

static BL_INLINE void updateSSOBitValue(BLFontFeatureSettingsCore* self, uint32_t index, uint32_t value) noexcept {
  uint32_t bit = 1u << index;

  BL_ASSERT((self->_d.u32_data[0] & bit) != 0u);

  self->_d.u32_data[1] = (self->_d.u32_data[1] & ~bit) | (value << index);
}

static BL_INLINE void removeSSOBitTag(BLFontFeatureSettingsCore* self, uint32_t index) noexcept {
  uint32_t bit = 1u << index;

  BL_ASSERT(self->_d.info.aField() > 0u);
  BL_ASSERT((self->_d.u32_data[0] & bit) != 0u);

  self->_d.u32_data[0] &= ~bit;
  self->_d.u32_data[1] &= ~bit;
  self->_d.info.bits -= 1u << BL_OBJECT_INFO_A_SHIFT;
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
  self->_d.info.bits = ((self->_d.info.bits & ~kValueDataMask) + (1u << BL_OBJECT_INFO_A_SHIFT)) | (vals & kValueDataMask);
}

static BL_INLINE void updateSSOFatValue(BLFontFeatureSettingsCore* self, uint32_t index, uint32_t value) noexcept {
  BL_ASSERT(index < kSSOFatFeatureCount);
  BL_ASSERT(value <= kSSOFatFeatureValueBitMask);

  uint32_t valueOffset = index * kSSOFatFeatureValueBitCount;
  uint32_t mask = kSSOFatFeatureValueBitMask << valueOffset;

  self->_d.info.bits = (self->_d.info.bits & ~mask) | value << valueOffset;
}

static BL_INLINE void removeSSOFatTag(BLFontFeatureSettingsCore* self, uint32_t index) noexcept {
  BL_ASSERT(self->_d.info.aField() > 0u);
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
  self->_d.info.bits = ((self->_d.info.bits & ~kValueDataMask) - (1u << BL_OBJECT_INFO_A_SHIFT)) | (vals & kValueDataMask);
}

static BL_INLINE bool canInsertSSOFatTag(const BLFontFeatureSettingsCore* self) noexcept {
  uint32_t lastId = self->_d.u32_data[2] >> ((kSSOFatFeatureCount - 1u) * kSSOFatFeatureTagBitCount);
  return lastId == kSSOInvalidFatFeatureId;
}

static bool convertItemsToSSO(BLFontFeatureSettingsCore* self, const BLFontFeatureItem* items, size_t size) noexcept {
  BL_ASSERT(size <= BLFontFeatureSettings::kSSOCapacity);

  uint32_t infoBits = BLObjectInfo::packTypeWithMarker(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS) | BLObjectInfo::packAbcp(uint32_t(size));

  uint32_t bitTagIds = 0;
  uint32_t bitValues = 0;

  uint32_t fatIndex = 0;
  uint32_t fatTagIds = kSSOInvalidFatFeaturePattern;
  uint32_t fatValues = infoBits;

  for (size_t i = 0; i < size; i++) {
    uint32_t id = FontTagData::featureTagToId(items[i].tag);
    uint32_t value = items[i].value;

    if (id == FontTagData::kInvalidId)
      return false;

    FeatureInfo featureInfo = FontTagData::featureInfoTable[id];
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

  ParametrizedBitOps<BitOrder::kLSB, uint32_t>::BitIterator bitIterator(bitTagIds);
  while (bitIterator.hasNext()) {
    uint32_t bitIndex = bitIterator.next();
    uint32_t bitFeatureTagId = uint32_t(FontTagData::featureBitIdToFeatureId(bitIndex));
    while (bitFeatureTagId > fatFeatureTagId) {
      *items++ = BLFontFeatureItem{FontTagData::featureIdToTagTable[fatFeatureTagId], fatValues & kSSOFatFeatureValueBitMask};

      fatFeatureTagId = fatTagIds & kSSOFatFeatureTagBitMask;
      if (fatFeatureTagId == kSSOInvalidFatFeatureId)
        fatFeatureTagId = kDummyFatTagId;

      fatTagIds >>= kSSOFatFeatureTagBitCount;
      fatValues >>= kSSOFatFeatureValueBitCount;
    }

    *items++ = BLFontFeatureItem{FontTagData::featureIdToTagTable[bitFeatureTagId], (bitValues >> bitIndex) & 0x1u};
  }

  if (fatFeatureTagId == kDummyFatTagId)
    return;

  do {
    *items++ = BLFontFeatureItem{FontTagData::featureIdToTagTable[fatFeatureTagId], fatValues & kSSOFatFeatureValueBitMask};
    fatFeatureTagId = fatTagIds & kSSOFatFeatureTagBitMask;
    fatTagIds >>= kSSOFatFeatureTagBitCount;
    fatValues >>= kSSOFatFeatureValueBitCount;
  } while (fatFeatureTagId != kSSOInvalidFatFeatureId);
}

// bl::FontFeatureSettings - Impl Utilities
// ========================================

static BL_INLINE constexpr size_t getMaximumSize() noexcept {
  return FontTagData::kUniqueTagCount;
}

static BL_INLINE BLObjectImplSize expandImplSize(BLObjectImplSize implSize) noexcept {
  return blObjectExpandImplSize(implSize);
}

static BL_INLINE BLResult initDynamic(BLFontFeatureSettingsCore* self, BLObjectImplSize implSize, size_t size = 0u) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLFontFeatureSettingsImpl>(self, info, implSize));

  BLFontFeatureSettingsImpl* impl = getImpl(self);
  BLFontFeatureItem* items = PtrOps::offset<BLFontFeatureItem>(impl, sizeof(BLFontFeatureSettingsImpl));

  impl->data = items;
  impl->size = size;
  impl->capacity = capacityFromImplSize(implSize);

  BL_ASSERT(size <= impl->capacity);
  return BL_SUCCESS;
}

static BL_NOINLINE BLResult initDynamicFromSSO(BLFontFeatureSettingsCore* self, BLObjectImplSize implSize, const BLFontFeatureSettingsCore* ssoMap) noexcept {
  size_t size = getSSOSize(ssoMap);
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLFontFeatureSettingsImpl>(self, info, implSize));

  BLFontFeatureSettingsImpl* impl = getImpl(self);
  BLFontFeatureItem* items = PtrOps::offset<BLFontFeatureItem>(impl, sizeof(BLFontFeatureSettingsImpl));

  impl->data = items;
  impl->size = size;
  impl->capacity = capacityFromImplSize(implSize);

  BL_ASSERT(size <= impl->capacity);
  convertSSOToItems(ssoMap, items);

  return BL_SUCCESS;
}

static BL_NOINLINE BLResult initDynamicFromData(BLFontFeatureSettingsCore* self, BLObjectImplSize implSize, const BLFontFeatureItem* src, size_t size) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLFontFeatureSettingsImpl>(self, info, implSize));

  BLFontFeatureSettingsImpl* impl = getImpl(self);
  BLFontFeatureItem* items = PtrOps::offset<BLFontFeatureItem>(impl, sizeof(BLFontFeatureSettingsImpl));

  impl->data = items;
  impl->size = size;
  impl->capacity = capacityFromImplSize(implSize);

  BL_ASSERT(size <= impl->capacity);
  memcpy(items, src, size * sizeof(BLFontFeatureItem));

  return BL_SUCCESS;
}

} // {FontFeatureSettingsInternal}
} // {bl}

// bl::FontFeatureSettings - API - Init & Destroy
// ==============================================

BL_API_IMPL BLResult blFontFeatureSettingsInit(BLFontFeatureSettingsCore* self) noexcept {
  using namespace bl::FontFeatureSettingsInternal;
  return initSSO(self);
}

BL_API_IMPL BLResult blFontFeatureSettingsInitMove(BLFontFeatureSettingsCore* self, BLFontFeatureSettingsCore* other) noexcept {
  using namespace bl::FontFeatureSettingsInternal;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontFeatureSettings());

  self->_d = other->_d;
  return initSSO(other);
}

BL_API_IMPL BLResult blFontFeatureSettingsInitWeak(BLFontFeatureSettingsCore* self, const BLFontFeatureSettingsCore* other) noexcept {
  using namespace bl::FontFeatureSettingsInternal;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontFeatureSettings());

  self->_d = other->_d;
  return retainInstance(self);
}

BL_API_IMPL BLResult blFontFeatureSettingsDestroy(BLFontFeatureSettingsCore* self) noexcept {
  using namespace bl::FontFeatureSettingsInternal;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  return releaseInstance(self);
}

// bl::FontFeatureSettings - API - Reset & Clear
// =============================================

BL_API_IMPL BLResult blFontFeatureSettingsReset(BLFontFeatureSettingsCore* self) noexcept {
  using namespace bl::FontFeatureSettingsInternal;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  releaseInstance(self);
  return initSSO(self);
}

BL_API_IMPL BLResult blFontFeatureSettingsClear(BLFontFeatureSettingsCore* self) noexcept {
  using namespace bl::FontFeatureSettingsInternal;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  if (self->_d.sso())
    return initSSO(self);

  BLFontFeatureSettingsImpl* selfI = getImpl(self);
  if (isImplMutable(selfI)) {
    selfI->size = 0;
    return BL_SUCCESS;
  }
  else {
    releaseInstance(self);
    return initSSO(self);
  }
}

// bl::FontFeatureSettings - API - Shrink
// ======================================

BL_API_IMPL BLResult blFontFeatureSettingsShrink(BLFontFeatureSettingsCore* self) noexcept {
  using namespace bl::FontFeatureSettingsInternal;
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

// bl::FontFeatureSettings - API - Assign
// ======================================

BL_API_IMPL BLResult blFontFeatureSettingsAssignMove(BLFontFeatureSettingsCore* self, BLFontFeatureSettingsCore* other) noexcept {
  using namespace bl::FontFeatureSettingsInternal;

  BL_ASSERT(self->_d.isFontFeatureSettings());
  BL_ASSERT(other->_d.isFontFeatureSettings());

  BLFontFeatureSettingsCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS]._d;
  return replaceInstance(self, &tmp);
}

BL_API_IMPL BLResult blFontFeatureSettingsAssignWeak(BLFontFeatureSettingsCore* self, const BLFontFeatureSettingsCore* other) noexcept {
  using namespace bl::FontFeatureSettingsInternal;

  BL_ASSERT(self->_d.isFontFeatureSettings());
  BL_ASSERT(other->_d.isFontFeatureSettings());

  retainInstance(other);
  return replaceInstance(self, other);
}

// bl::FontFeatureSettings - API - Accessors
// =========================================

BL_API_IMPL size_t blFontFeatureSettingsGetSize(const BLFontFeatureSettingsCore* self) noexcept {
  using namespace bl::FontFeatureSettingsInternal;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  if (self->_d.sso())
    return getSSOSize(self);
  else
    return getImpl(self)->size;
}

BL_API_IMPL size_t blFontFeatureSettingsGetCapacity(const BLFontFeatureSettingsCore* self) noexcept {
  using namespace bl::FontFeatureSettingsInternal;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  if (self->_d.sso())
    return BLFontFeatureSettings::kSSOCapacity;
  else
    return getImpl(self)->capacity;
}

BL_API_IMPL BLResult blFontFeatureSettingsGetView(const BLFontFeatureSettingsCore* self, BLFontFeatureSettingsView* out) noexcept {
  using namespace bl::FontFeatureSettingsInternal;
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

BL_API_IMPL bool blFontFeatureSettingsHasValue(const BLFontFeatureSettingsCore* self, BLTag featureTag) noexcept {
  using namespace bl::FontFeatureSettingsInternal;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  // SSO Mode
  // --------

  if (self->_d.sso()) {
    uint32_t id = bl::FontTagData::featureTagToId(featureTag);
    if (id == bl::FontTagData::kInvalidId)
      return false;

    FeatureInfo featureInfo = bl::FontTagData::featureInfoTable[id];
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
  size_t index = bl::lowerBound(data, selfI->size, featureTag, [](const BLFontFeatureItem& item, uint32_t tag) noexcept { return item.tag < tag; });

  return index < size && data[index].tag == featureTag;
}

BL_API_IMPL uint32_t blFontFeatureSettingsGetValue(const BLFontFeatureSettingsCore* self, BLTag featureTag) noexcept {
  using namespace bl::FontFeatureSettingsInternal;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  if (self->_d.sso())
    return getSSOTagValue(self, featureTag);
  else
    return getDynamicTagValue(self, featureTag);
}

BL_API_IMPL BLResult blFontFeatureSettingsSetValue(BLFontFeatureSettingsCore* self, BLTag featureTag, uint32_t value) noexcept {
  using namespace bl::FontFeatureSettingsInternal;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  if (BL_UNLIKELY(value > 65535u))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  uint32_t featureId = bl::FontTagData::featureTagToId(featureTag);
  bool canModify = true;

  // SSO Mode
  // --------

  if (self->_d.sso()) {
    size_t size = getSSOSize(self);

    if (featureId != bl::FontTagData::kInvalidId) {
      FeatureInfo featureInfo = bl::FontTagData::featureInfoTable[featureId];
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
      if (BL_UNLIKELY(!bl::FontTagData::isValidTag(featureTag)))
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
    if (BL_UNLIKELY(!bl::FontTagData::isValidTag(featureTag)))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    canModify = isImplMutable(getImpl(self));
  }

  // Dynamic Mode
  // ------------

  BLFontFeatureSettingsImpl* selfI = getImpl(self);
  BLFontFeatureItem* items = selfI->data;

  size_t size = selfI->size;
  size_t index = bl::lowerBound(items, size, featureTag, [](const BLFontFeatureItem& item, uint32_t tag) noexcept { return item.tag < tag; });

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

  if (BL_UNLIKELY(!bl::FontTagData::isValidTag(featureTag)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  // Insert a new tag/value pair if `featureTag` is not in the settings.
  size_t nTagsAfterIndex = size - index;
  if (canModify && selfI->capacity > size) {
    bl::MemOps::copyBackwardInlineT(items + index + 1, items + index, nTagsAfterIndex);
    items[index] = BLFontFeatureItem{featureTag, value};
    selfI->size = size + 1;
    return BL_SUCCESS;
  }
  else {
    BLFontFeatureSettingsCore tmp;
    BL_PROPAGATE(initDynamic(&tmp, expandImplSize(implSizeFromCapacity(size + 1)), size + 1));

    BLFontFeatureItem* dst = getImpl(&tmp)->data;
    bl::MemOps::copyForwardInlineT(dst, items, index);
    dst[index] = BLFontFeatureItem{featureTag, value};
    bl::MemOps::copyForwardInlineT(dst + index + 1, items + index, nTagsAfterIndex);

    return replaceInstance(self, &tmp);
  }
}

BL_API_IMPL BLResult blFontFeatureSettingsRemoveValue(BLFontFeatureSettingsCore* self, BLTag featureTag) noexcept {
  using namespace bl::FontFeatureSettingsInternal;
  BL_ASSERT(self->_d.isFontFeatureSettings());

  // SSO Mode
  // --------

  if (self->_d.sso()) {
    uint32_t featureId = bl::FontTagData::featureTagToId(featureTag);
    if (featureId == bl::FontTagData::kInvalidId)
      return BL_SUCCESS;

    FeatureInfo featureInfo = bl::FontTagData::featureInfoTable[featureId];
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
  size_t index = bl::lowerBound(items, selfI->size, featureTag, [](const BLFontFeatureItem& item, uint32_t tag) noexcept { return item.tag < tag; });

  if (index >= size || items[index].tag != featureTag)
    return BL_SUCCESS;

  if (isImplMutable(selfI)) {
    selfI->size = size - 1;
    bl::MemOps::copyForwardInlineT(items + index, items + index + 1, size - index - 1);
    return BL_SUCCESS;
  }
  else {
    BLFontFeatureSettingsCore tmp;
    BL_PROPAGATE(initDynamic(&tmp, expandImplSize(implSizeFromCapacity(size - 1)), size - 1));

    BLFontFeatureItem* dst = getImpl(&tmp)->data;
    bl::MemOps::copyForwardInlineT(dst, items, index);
    bl::MemOps::copyForwardInlineT(dst + index, items + index + 1, size - index - 1);

    return replaceInstance(self, &tmp);
  }
}

// bl::FontFeatureSettings - API - Equals
// ======================================

BL_API_IMPL bool blFontFeatureSettingsEquals(const BLFontFeatureSettingsCore* a, const BLFontFeatureSettingsCore* b) noexcept {
  using namespace bl::FontFeatureSettingsInternal;

  BL_ASSERT(a->_d.isFontFeatureSettings());
  BL_ASSERT(b->_d.isFontFeatureSettings());

  if (a->_d == b->_d)
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
      BLInternal::swap(a, b);

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

    return a->_d == bSSO._d;
  }
}

// bl::FontFeatureSettings - Runtime Registration
// ==============================================

void blFontFeatureSettingsRtInit(BLRuntimeContext* rt) noexcept {
  using namespace bl::FontFeatureSettingsInternal;

  blUnused(rt);

  // Initialize BLFontFeatureSettings.
  initSSO(static_cast<BLFontFeatureSettingsCore*>(&blObjectDefaults[BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS]));
}
