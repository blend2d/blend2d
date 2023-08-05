// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "fonttagdata_p.h"
#include "fontvariationsettings_p.h"
#include "math_p.h"
#include "object_p.h"
#include "runtime_p.h"
#include "string_p.h"
#include "support/algorithm_p.h"
#include "support/memops_p.h"
#include "support/ptrops_p.h"

namespace BLFontVariationSettingsPrivate {

// BLFontVariationSettings - SSO Utilities
// =======================================

//! A constant that can be used to increment / decrement a size in SSO representation.
static constexpr uint32_t kSSOSizeIncrement = (1u << BL_OBJECT_INFO_A_SHIFT);

//! Number of bits that represents a variation id in SSO mode.
static constexpr uint32_t kSSOTagBitSize = 5u;

//! Mask of a single SSO tag value (id).
static constexpr uint32_t kSSOTagBitMask = (1u << kSSOTagBitSize) - 1;

static BL_INLINE BLResult initSSO(BLFontVariationSettingsCore* self, size_t size = 0) noexcept {
  self->_d.initStatic(BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS) |
                      BLObjectInfo::fromAbcp(uint32_t(size)));
  return BL_SUCCESS;
}

static BL_INLINE size_t getSSOSize(const BLFontVariationSettingsCore* self) noexcept { return self->_d.info.aField(); }
static BL_INLINE void setSSOSize(BLFontVariationSettingsCore* self, size_t size) noexcept { self->_d.info.setAField(uint32_t(size)); }

static BL_INLINE float getSSOValueAt(const BLFontVariationSettingsCore* self, size_t index) noexcept {return self->_d.f32_data[index]; }

static BL_INLINE BLResult setSSOValueAt(BLFontVariationSettingsCore* self, size_t index, float value) noexcept {
  self->_d.f32_data[index] = value;
  return BL_SUCCESS;
}

static BL_INLINE bool findSSOTag(const BLFontVariationSettingsCore* self, uint32_t id, size_t* indexOut) noexcept {
  uint32_t ssoBits = self->_d.info.bits;
  size_t size = getSSOSize(self);

  size_t i = 0;
  for (i = 0; i < size; i++, ssoBits >>= kSSOTagBitSize) {
    uint32_t ssoId = ssoBits & kSSOTagBitMask;
    if (ssoId < id)
      continue;
    *indexOut = i;
    return id == ssoId;
  }

  *indexOut = i;
  return false;
}

static bool convertItemsToSSO(BLFontVariationSettingsCore* dst, const BLFontVariationItem* items, size_t size) noexcept {
  BL_ASSERT(size <= BLFontVariationSettings::kSSOCapacity);

  initSSO(dst, size);

  uint32_t idShift = 0;
  uint32_t ssoBits = 0;
  float* ssoValues = dst->_d.f32_data;

  for (size_t i = 0; i < size; i++, idShift += kSSOTagBitSize) {
    uint32_t id = BLFontTagData::variationTagToId(items[i].tag);
    float value = items[i].value;

    if (id == BLFontTagData::kInvalidId)
      return false;

    ssoBits |= id << idShift;
    ssoValues[i] = value;
  }

  dst->_d.info.bits |= ssoBits;
  return true;
}

// BLFontVariationSettings - Impl Utilities
// ========================================

static BL_INLINE constexpr size_t getMaximumSize() noexcept {
  return BLFontTagData::kUniqueTagCount;
}

static BL_INLINE BLObjectImplSize expandImplSize(BLObjectImplSize implSize) noexcept {
  return blObjectExpandImplSize(implSize);
}

static BL_INLINE BLResult initDynamic(BLFontVariationSettingsCore* self, BLObjectImplSize implSize, size_t size = 0u) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS);
  BL_PROPAGATE(BLObjectPrivate::allocImplT<BLFontVariationSettingsImpl>(self, info, implSize));

  BLFontVariationSettingsImpl* impl = getImpl(self);
  BLFontVariationItem* items = BLPtrOps::offset<BLFontVariationItem>(impl, sizeof(BLFontVariationSettingsImpl));

  impl->data = items;
  impl->size = size;
  impl->capacity = capacityFromImplSize(implSize);

  BL_ASSERT(size <= impl->capacity);
  return BL_SUCCESS;
}

static BL_NOINLINE BLResult initDynamicFromSSO(BLFontVariationSettingsCore* self, BLObjectImplSize implSize, const BLFontVariationSettingsCore* ssoMap) noexcept {
  size_t size = getSSOSize(ssoMap);
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS);
  BL_PROPAGATE(BLObjectPrivate::allocImplT<BLFontVariationSettingsImpl>(self, info, implSize));

  BLFontVariationSettingsImpl* impl = getImpl(self);
  BLFontVariationItem* items = BLPtrOps::offset<BLFontVariationItem>(impl, sizeof(BLFontVariationSettingsImpl));

  impl->data = items;
  impl->size = size;
  impl->capacity = capacityFromImplSize(implSize);

  BL_ASSERT(size <= impl->capacity);
  uint32_t ssoBits = ssoMap->_d.info.bits;

  const float* ssoValues = ssoMap->_d.f32_data;
  for (size_t i = 0; i < size; i++, ssoBits >>= kSSOTagBitSize)
    items[i] = BLFontVariationItem{BLFontTagData::variationIdToTagTable[ssoBits & kSSOTagBitMask], ssoValues[i]};

  return BL_SUCCESS;
}

static BL_NOINLINE BLResult initDynamicFromData(BLFontVariationSettingsCore* self, BLObjectImplSize implSize, const BLFontVariationItem* src, size_t size) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS);
  BL_PROPAGATE(BLObjectPrivate::allocImplT<BLFontVariationSettingsImpl>(self, info, implSize));

  BLFontVariationSettingsImpl* impl = getImpl(self);
  BLFontVariationItem* items = BLPtrOps::offset<BLFontVariationItem>(impl, sizeof(BLFontVariationSettingsImpl));

  impl->data = items;
  impl->size = size;
  impl->capacity = capacityFromImplSize(implSize);

  BL_ASSERT(size <= impl->capacity);
  memcpy(items, src, size * sizeof(BLFontVariationItem));

  return BL_SUCCESS;
}

} // {BLFontVariationSettingsPrivate}

// BLFontVariationSettings - API - Init & Destroy
// ==============================================

BLResult blFontVariationSettingsInit(BLFontVariationSettingsCore* self) noexcept {
  using namespace BLFontVariationSettingsPrivate;
  return initSSO(self);
}

BLResult blFontVariationSettingsInitMove(BLFontVariationSettingsCore* self, BLFontVariationSettingsCore* other) noexcept {
  using namespace BLFontVariationSettingsPrivate;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontVariationSettings());

  self->_d = other->_d;
  return initSSO(other);
}

BLResult blFontVariationSettingsInitWeak(BLFontVariationSettingsCore* self, const BLFontVariationSettingsCore* other) noexcept {
  using namespace BLFontVariationSettingsPrivate;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontVariationSettings());

  self->_d = other->_d;
  return retainInstance(self);
}

BLResult blFontVariationSettingsDestroy(BLFontVariationSettingsCore* self) noexcept {
  using namespace BLFontVariationSettingsPrivate;
  BL_ASSERT(self->_d.isFontVariationSettings());

  return releaseInstance(self);
}

// BLFontVariationSettings - API - Reset & Clear
// =============================================

BLResult blFontVariationSettingsReset(BLFontVariationSettingsCore* self) noexcept {
  using namespace BLFontVariationSettingsPrivate;
  BL_ASSERT(self->_d.isFontVariationSettings());

  releaseInstance(self);
  return initSSO(self);
}

BLResult blFontVariationSettingsClear(BLFontVariationSettingsCore* self) noexcept {
  using namespace BLFontVariationSettingsPrivate;
  BL_ASSERT(self->_d.isFontVariationSettings());

  if (self->_d.sso())
    return initSSO(self);

  BLFontVariationSettingsImpl* selfI = getImpl(self);
  if (isImplMutable(selfI)) {
    selfI->size = 0;
    return BL_SUCCESS;
  }
  else {
    releaseInstance(self);
    return initSSO(self);
  }
}

// BLFontVariationSettings - API - Shrink
// ======================================

BLResult blFontVariationSettingsShrink(BLFontVariationSettingsCore* self) noexcept {
  using namespace BLFontVariationSettingsPrivate;
  BL_ASSERT(self->_d.isFontVariationSettings());

  if (self->_d.sso())
    return BL_SUCCESS;

  BLFontVariationSettingsImpl* selfI = getImpl(self);
  BLFontVariationItem* items = selfI->data;
  size_t size = selfI->size;

  BLFontVariationSettingsCore tmp;
  if (size <= BLFontVariationSettings::kSSOCapacity && convertItemsToSSO(&tmp, items, size))
    return replaceInstance(self, &tmp);

  BLObjectImplSize currentSize = implSizeFromCapacity(selfI->capacity);
  BLObjectImplSize shrunkSize = implSizeFromCapacity(selfI->size);

  if (shrunkSize + BL_OBJECT_IMPL_ALIGNMENT > currentSize)
    return BL_SUCCESS;

  BL_PROPAGATE(initDynamicFromData(&tmp, shrunkSize, items, size));
  return replaceInstance(self, &tmp);
}

// BLFontVariationSettings - API - Assign
// ======================================

BLResult blFontVariationSettingsAssignMove(BLFontVariationSettingsCore* self, BLFontVariationSettingsCore* other) noexcept {
  using namespace BLFontVariationSettingsPrivate;

  BL_ASSERT(self->_d.isFontVariationSettings());
  BL_ASSERT(other->_d.isFontVariationSettings());

  BLFontVariationSettingsCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS]._d;
  return replaceInstance(self, &tmp);
}

BLResult blFontVariationSettingsAssignWeak(BLFontVariationSettingsCore* self, const BLFontVariationSettingsCore* other) noexcept {
  using namespace BLFontVariationSettingsPrivate;

  BL_ASSERT(self->_d.isFontVariationSettings());
  BL_ASSERT(other->_d.isFontVariationSettings());

  retainInstance(other);
  return replaceInstance(self, other);
}

// BLFontVariationSettings - API - Accessors
// =========================================

size_t blFontVariationSettingsGetSize(const BLFontVariationSettingsCore* self) noexcept {
  using namespace BLFontVariationSettingsPrivate;
  BL_ASSERT(self->_d.isFontVariationSettings());

  if (self->_d.sso())
    return getSSOSize(self);
  else
    return getImpl(self)->size;
}

size_t blFontVariationSettingsGetCapacity(const BLFontVariationSettingsCore* self) noexcept {
  using namespace BLFontVariationSettingsPrivate;
  BL_ASSERT(self->_d.isFontVariationSettings());

  if (self->_d.sso())
    return BLFontVariationSettings::kSSOCapacity;
  else
    return getImpl(self)->capacity;
}

BLResult blFontVariationSettingsGetView(const BLFontVariationSettingsCore* self, BLFontVariationSettingsView* out) noexcept {
  using namespace BLFontVariationSettingsPrivate;
  BL_ASSERT(self->_d.isFontVariationSettings());

  // SSO Mode
  // --------

  if (self->_d.sso()) {
    BLFontVariationItem* items = out->ssoData;
    size_t size = getSSOSize(self);

    uint32_t ssoBits = self->_d.info.bits;
    const float* ssoValues = self->_d.f32_data;

    out->data = items;
    out->size = size;

    for (size_t i = 0; i < size; i++, ssoBits >>= kSSOTagBitSize)
      items[i] = BLFontVariationItem{BLFontTagData::variationIdToTagTable[ssoBits & kSSOTagBitMask], ssoValues[i]};

    return BL_SUCCESS;
  }

  // Dynamic Mode
  // ------------

  const BLFontVariationSettingsImpl* selfI = getImpl(self);
  out->data = selfI->data;
  out->size = selfI->size;
  return BL_SUCCESS;
}

bool blFontVariationSettingsHasValue(const BLFontVariationSettingsCore* self, BLTag variationTag) noexcept {
  using namespace BLFontVariationSettingsPrivate;
  BL_ASSERT(self->_d.isFontVariationSettings());

  // SSO Mode
  // --------

  if (self->_d.sso()) {
    uint32_t id = BLFontTagData::variationTagToId(variationTag);
    if (id == BLFontTagData::kInvalidId)
      return false;

    size_t index;
    return findSSOTag(self, id, &index);
  }

  // Dynamic Mode
  // ------------

  const BLFontVariationSettingsImpl* selfI = getImpl(self);
  const BLFontVariationItem* data = selfI->data;

  size_t size = selfI->size;
  size_t index = BLAlgorithm::lowerBound(data, selfI->size, variationTag, [](const BLFontVariationItem& item, uint32_t tag) noexcept { return item.tag < tag; });

  return index < size && data[index].tag == variationTag;
}

float blFontVariationSettingsGetValue(const BLFontVariationSettingsCore* self, BLTag variationTag) noexcept {
  using namespace BLFontVariationSettingsPrivate;
  BL_ASSERT(self->_d.isFontVariationSettings());

  // SSO Mode
  // --------

  if (self->_d.sso()) {
    uint32_t id = BLFontTagData::variationTagToId(variationTag);
    if (id == BLFontTagData::kInvalidId)
      return blNaN<float>();

    size_t index;
    if (findSSOTag(self, id, &index))
      return getSSOValueAt(self, index);
    else
      return blNaN<float>();
  }

  // Dynamic Mode
  // ------------

  const BLFontVariationSettingsImpl* selfI = getImpl(self);
  const BLFontVariationItem* data = selfI->data;

  size_t size = selfI->size;
  size_t index = BLAlgorithm::lowerBound(data, selfI->size, variationTag, [](const BLFontVariationItem& item, uint32_t tag) noexcept { return item.tag < tag; });

  if (index < size && data[index].tag == variationTag)
    return data[index].value;
  else
    return blNaN<float>();
}

BLResult blFontVariationSettingsSetValue(BLFontVariationSettingsCore* self, BLTag variationTag, float value) noexcept {
  using namespace BLFontVariationSettingsPrivate;
  BL_ASSERT(self->_d.isFontVariationSettings());

  if (BL_UNLIKELY(value > 65535u))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  // SSO Mode
  // --------

  bool canModify = true;

  if (self->_d.sso()) {
    size_t size = getSSOSize(self);

    if (value <= 1) {
      uint32_t id = BLFontTagData::variationTagToId(variationTag);
      if (id != BLFontTagData::kInvalidId) {
        size_t index;
        if (findSSOTag(self, id, &index)) {
          setSSOValueAt(self, index, value);
          return BL_SUCCESS;
        }

        if (size < BLFontVariationSettings::kSSOCapacity) {
          // Every inserted tag must be inserted in a way to make tags sorted and we know where to insert (index).
          float* ssoValues = self->_d.f32_data;
          size_t nTagsAfterIndex = size - index;
          BLMemOps::copyBackwardInlineT(ssoValues + index + 1u, ssoValues + index, nTagsAfterIndex);
          ssoValues[index] = value;

          // Update the tag and object info - updates the size (increments one), adds a new tag, and shifts all ids after `index`.
          uint32_t ssoBits = self->_d.info.bits + kSSOSizeIncrement;
          uint32_t bitIndex = uint32_t(index * kSSOTagBitSize);
          uint32_t tagsAfterIndexMask = ((1u << (nTagsAfterIndex * kSSOTagBitSize)) - 1u) << bitIndex;
          self->_d.info.bits = (ssoBits & ~tagsAfterIndexMask) | ((ssoBits & tagsAfterIndexMask) << kSSOTagBitSize) | (id << bitIndex);
          return BL_SUCCESS;
        }
      }
      else {
        if (BL_UNLIKELY(!BLFontTagData::isValidTag(variationTag)))
          return blTraceError(BL_ERROR_INVALID_VALUE);
      }
    }

    // Turn the SSO settings to dynamic settings, because some (or multiple) cases below are true:
    //   a) The `tag` doesn't have a corresponding variation id, thus it cannot be used in SSO mode.
    //   b) There is no room in SSO storage to insert another tag/value pair.
    BLObjectImplSize implSize = blObjectAlignImplSize(implSizeFromCapacity(blMax<size_t>(size + 1, 4u)));
    BLFontVariationSettingsCore tmp;

    // NOTE: This will turn the SSO settings into a dynamic settings - it's guaranteed that all further operations will succeed.
    BL_PROPAGATE(initDynamicFromSSO(&tmp, implSize, self));
    *self = tmp;
  }
  else {
    if (BL_UNLIKELY(!BLFontTagData::isValidTag(variationTag)))
      return blTraceError(BL_ERROR_INVALID_VALUE);

    canModify = isImplMutable(getImpl(self));
  }

  // Dynamic Mode
  // ------------

  BLFontVariationSettingsImpl* selfI = getImpl(self);
  BLFontVariationItem* items = selfI->data;

  size_t size = selfI->size;
  size_t index = BLAlgorithm::lowerBound(items, size, variationTag, [](const BLFontVariationItem& item, uint32_t tag) noexcept { return item.tag < tag; });

  // Overwrite the value if the `variationTag` is already in the settings.
  if (index < size && items[index].tag == variationTag) {
    if (items[index].value == value)
      return BL_SUCCESS;

    if (canModify) {
      items[index].value = value;
      return BL_SUCCESS;
    }
    else {
      BLFontVariationSettingsCore tmp;
      BL_PROPAGATE(initDynamicFromData(&tmp, implSizeFromCapacity(size), items, size));
      getImpl(&tmp)->data[index].value = value;
      return replaceInstance(self, &tmp);
    }
  }

  if (BL_UNLIKELY(!BLFontTagData::isValidTag(variationTag)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  // Insert a new variation tag if it's not in the settings.
  size_t nTagsAfterIndex = size - index;
  if (canModify && selfI->capacity > size) {
    BLMemOps::copyBackwardInlineT(items + index + 1, items + index, nTagsAfterIndex);
    items[index] = BLFontVariationItem{variationTag, value};
    selfI->size = size + 1;
    return BL_SUCCESS;
  }
  else {
    BLFontVariationSettingsCore tmp;
    BL_PROPAGATE(initDynamic(&tmp, expandImplSize(implSizeFromCapacity(size + 1)), size + 1));

    BLFontVariationItem* dst = getImpl(&tmp)->data;
    BLMemOps::copyForwardInlineT(dst, items, index);
    dst[index] = BLFontVariationItem{variationTag, value};
    BLMemOps::copyForwardInlineT(dst + index + 1, items + index, nTagsAfterIndex);

    return replaceInstance(self, &tmp);
  }
}

BLResult blFontVariationSettingsRemoveValue(BLFontVariationSettingsCore* self, BLTag variationTag) noexcept {
  using namespace BLFontVariationSettingsPrivate;
  BL_ASSERT(self->_d.isFontVariationSettings());

  // SSO Mode
  // --------

  if (self->_d.sso()) {
    uint32_t id = BLFontTagData::variationTagToId(variationTag);
    if (id == BLFontTagData::kInvalidId)
      return BL_SUCCESS;

    size_t size = getSSOSize(self);
    size_t index;

    if (!findSSOTag(self, id, &index))
      return BL_SUCCESS;

    size_t i = index;
    float* ssoValues = self->_d.f32_data;

    while (i < size) {
      ssoValues[i] = ssoValues[i + 1];
      i++;
    }

    // Clear the value that has been removed. The reason for doing this is to make sure that two settings that have
    // the same SSO data would be binary equal (there would not be garbage in data after the size in SSO storage).
    ssoValues[size - 1] = 0.0f;

    // Shift the bit data representing tags (ids) so they are in correct places  after the removal operation.
    uint32_t ssoBits = self->_d.info.bits;
    uint32_t bitIndex = uint32_t(index * kSSOTagBitSize);
    uint32_t tagsToShift = uint32_t(size - index - 1);
    uint32_t remainingKeysAfterIndexMask = ((1u << (tagsToShift * kSSOTagBitSize)) - 1u) << (bitIndex + kSSOTagBitSize);

    self->_d.info.bits = (ssoBits & ~(BL_OBJECT_INFO_A_MASK | remainingKeysAfterIndexMask | (kSSOTagBitMask << bitIndex))) |
                         ((ssoBits & remainingKeysAfterIndexMask) >> kSSOTagBitSize) |
                         (uint32_t(size - 1u) << BL_OBJECT_INFO_A_SHIFT);
    return BL_SUCCESS;
  }

  // Dynamic Mode
  // ------------

  BLFontVariationSettingsImpl* selfI = getImpl(self);
  BLFontVariationItem* items = selfI->data;

  size_t size = selfI->size;
  size_t index = BLAlgorithm::lowerBound(items, selfI->size, variationTag, [](const BLFontVariationItem& item, uint32_t tag) noexcept { return item.tag < tag; });

  if (index >= size || items[index].tag != variationTag)
    return BL_SUCCESS;

  if (isImplMutable(selfI)) {
    selfI->size = size - 1;
    BLMemOps::copyForwardInlineT(items + index, items + index + 1, size - index - 1);
    return BL_SUCCESS;
  }
  else {
    BLFontVariationSettingsCore tmp;
    BL_PROPAGATE(initDynamic(&tmp, expandImplSize(implSizeFromCapacity(size - 1)), size - 1));

    BLFontVariationItem* dst = getImpl(&tmp)->data;
    BLMemOps::copyForwardInlineT(dst, items, index);
    BLMemOps::copyForwardInlineT(dst + index, items + index + 1, size - index - 1);

    return replaceInstance(self, &tmp);
  }
}

// BLFontVariationSettings - API - Equals
// ======================================

bool blFontVariationSettingsEquals(const BLFontVariationSettingsCore* a, const BLFontVariationSettingsCore* b) noexcept {
  using namespace BLFontVariationSettingsPrivate;

  BL_ASSERT(a->_d.isFontVariationSettings());
  BL_ASSERT(b->_d.isFontVariationSettings());

  if (a->_d == b->_d)
    return true;

  if (a->_d.sso() == b->_d.sso()) {
    // Both are SSO: They must be binary equal, if not, they are not equal.
    if (a->_d.sso())
      return false;

    // Both are dynamic.
    const BLFontVariationSettingsImpl* aImpl = getImpl(a);
    const BLFontVariationSettingsImpl* bImpl = getImpl(b);

    size_t size = aImpl->size;
    if (size != bImpl->size)
      return false;

    return memcmp(aImpl->data, bImpl->data, size * sizeof(BLFontVariationItem)) == 0;
  }
  else {
    // One is SSO and one is dynamic, make `a` the SSO one.
    if (b->_d.sso())
      std::swap(a, b);

    const BLFontVariationSettingsImpl* bImpl = getImpl(b);
    size_t size = getSSOSize(a);

    if (size != bImpl->size)
      return false;

    uint32_t aBits = a->_d.info.bits;
    const float* aValues = a->_d.f32_data;
    const BLFontVariationItem* bItems = bImpl->data;

    for (size_t i = 0; i < size; i++, aBits >>= kSSOTagBitSize) {
      uint32_t aTag = BLFontTagData::variationIdToTagTable[aBits & kSSOTagBitMask];
      float aValue = aValues[i];

      if (bItems[i].tag != aTag || bItems[i].value != aValue)
        return false;
    }

    return true;
  }
}

// BLFontVariationSettings - Runtime Registration
// ==============================================

void blFontVariationSettingsRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  // Initialize BLFontVariationSettings.
  blObjectDefaults[BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS]._d.initStatic(
    BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS));
}
