// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTFEATURESETTINGS_P_H_INCLUDED
#define BLEND2D_FONTFEATURESETTINGS_P_H_INCLUDED

#include "api-internal_p.h"
#include "fontfeaturesettings.h"
#include "fonttagdata_p.h"
#include "object_p.h"
#include "support/algorithm_p.h"
#include "support/bitops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {
namespace FontFeatureSettingsInternal {

//! \name BLFontFeatureSettings - Internals - Common Functionality (Container)
//! \{

static BL_INLINE constexpr BLObjectImplSize implSizeFromCapacity(size_t capacity) noexcept {
  return BLObjectImplSize(sizeof(BLFontFeatureSettingsImpl) + capacity * sizeof(BLFontFeatureItem));
}

static BL_INLINE constexpr size_t capacityFromImplSize(BLObjectImplSize implSize) noexcept {
  return (implSize.value() - sizeof(BLFontFeatureSettingsImpl)) / sizeof(BLFontFeatureItem);
}

//! \}

//! \name BLFontFeatureSettings - Internals - Common Functionality (Impl)
//! \{

static BL_INLINE bool isImplMutable(BLFontFeatureSettingsImpl* impl) noexcept {
  return ObjectInternal::isImplMutable(impl);
}

static BL_INLINE BLResult freeImpl(BLFontFeatureSettingsImpl* impl) noexcept {
  return ObjectInternal::freeImpl(impl);
}

template<RCMode kRCMode>
static BL_INLINE BLResult releaseImpl(BLFontFeatureSettingsImpl* impl) noexcept {
  return ObjectInternal::derefImplAndTest<kRCMode>(impl) ? freeImpl(impl) : BLResult(BL_SUCCESS);
}

//! \}

//! \name BLFontFeatureSettings - Internals - Common Functionality (Instance)
//! \{

static BL_INLINE BLFontFeatureSettingsImpl* getImpl(const BLFontFeatureSettingsCore* self) noexcept {
  return static_cast<BLFontFeatureSettingsImpl*>(self->_d.impl);
}

static BL_INLINE BLResult retainInstance(const BLFontFeatureSettingsCore* self, size_t n = 1) noexcept {
  return ObjectInternal::retainInstance(self, n);
}

static BL_INLINE BLResult releaseInstance(BLFontFeatureSettingsCore* self) noexcept {
  return self->_d.info.isRefCountedObject() ? releaseImpl<RCMode::kForce>(getImpl(self)) : BLResult(BL_SUCCESS);
}

static BL_INLINE BLResult replaceInstance(BLFontFeatureSettingsCore* self, const BLFontFeatureSettingsCore* other) noexcept {
  BLFontFeatureSettingsImpl* impl = getImpl(self);
  BLObjectInfo info = self->_d.info;

  self->_d = other->_d;
  return info.isRefCountedObject() ? releaseImpl<RCMode::kForce>(impl) : BLResult(BL_SUCCESS);
}

//! \}

//! \name BLFontFeatureSettings - Internals - SSO Fat Representation
//! \{

using FontTagData::FeatureInfo;
using FatBitOps = ParametrizedBitOps<BitOrder::kLSB, uint32_t>;

static constexpr uint32_t kSSOFatFeatureCount = 4;
static constexpr uint32_t kSSOFatFeatureTagBitCount = 8;
static constexpr uint32_t kSSOFatFeatureTagBitMask = (1u << kSSOFatFeatureTagBitCount) - 1u;
static constexpr uint32_t kSSOFatFeatureValueBitCount = 4;
static constexpr uint32_t kSSOFatFeatureValueBitMask = (1u << kSSOFatFeatureValueBitCount) - 1u;

// 'zero' is used by SSO, thus it can never be used in fat feature data.
static constexpr uint32_t kSSOInvalidFatFeatureId = 0xFFu;
// 32-bit pattern that is used to initialize SSO storage.
static constexpr uint32_t kSSOInvalidFatFeaturePattern = 0xFFFFFFFFu;

static BL_INLINE bool hasSSOBitTag(const BLFontFeatureSettingsCore* self, uint32_t index) noexcept {
  return (self->_d.u32_data[0] >> index) & 0x1u;
}

static BL_INLINE uint32_t getSSOBitValue(const BLFontFeatureSettingsCore* self, uint32_t index) noexcept {
  return (self->_d.u32_data[1] >> index) & 0x1u;
}

static BL_INLINE uint32_t getSSOFatValue(const BLFontFeatureSettingsCore* self, uint32_t index) noexcept {
  return (self->_d.info.bits >> (index * kSSOFatFeatureValueBitCount)) & kSSOFatFeatureValueBitMask;
}

static BL_INLINE bool findSSOFatTag(const BLFontFeatureSettingsCore* self, uint32_t featureId, uint32_t* indexOut) noexcept {
  uint32_t tags = self->_d.u32_data[2];

  for (uint32_t index = 0; index < kSSOFatFeatureCount; index++, tags >>= kSSOFatFeatureTagBitCount) {
    uint32_t id = tags & kSSOFatFeatureTagBitMask;
    if (id == kSSOInvalidFatFeatureId || id >= featureId) {
      *indexOut = index;
      return id == featureId;
    }
  }

  *indexOut = kSSOFatFeatureCount;
  return false;
}

static BL_INLINE uint32_t getSSOTagValue(const BLFontFeatureSettingsCore* self, BLTag featureTag, uint32_t notFoundValue = BL_FONT_FEATURE_INVALID_VALUE) noexcept {
  BL_ASSERT(self->_d.sso());

  uint32_t featureId = FontTagData::featureTagToId(featureTag);
  if (featureId == FontTagData::kInvalidId)
    return notFoundValue;

  FontTagData::FeatureInfo featureInfo = FontTagData::featureInfoTable[featureId];
  if (featureInfo.hasBitId()) {
    uint32_t featureBitId = featureInfo.bitId;
    if (!hasSSOBitTag(self, featureBitId))
      return notFoundValue;

    return getSSOBitValue(self, featureBitId);
  }
  else {
    uint32_t index;
    if (!findSSOFatTag(self, featureId, &index))
      return notFoundValue;

    return getSSOFatValue(self, index);
  }
}

static BL_INLINE uint32_t getDynamicTagValue(const BLFontFeatureSettingsCore* self, BLTag featureTag, uint32_t notFoundValue = BL_FONT_FEATURE_INVALID_VALUE) noexcept {
  const BLFontFeatureSettingsImpl* selfI = getImpl(self);
  const BLFontFeatureItem* data = selfI->data;

  size_t size = selfI->size;
  size_t index = lowerBound(data, selfI->size, featureTag, [](const BLFontFeatureItem& item, uint32_t tag) noexcept { return item.tag < tag; });

  if (index < size && data[index].tag == featureTag)
    return data[index].value;
  else
    return notFoundValue;
}

template<bool kSSO>
static BL_INLINE uint32_t getTagValue(const BLFontFeatureSettingsCore* self, BLTag featureTag, uint32_t notFoundValue = BL_FONT_FEATURE_INVALID_VALUE) noexcept {
  return kSSO ? getSSOTagValue(self, featureTag, notFoundValue) : getDynamicTagValue(self, featureTag, notFoundValue);
}

template<bool kSSO>
static BL_INLINE bool isFeatureEnabledForPlan(const BLFontFeatureSettingsCore* self, BLTag featureTag) noexcept {
  uint32_t featureId = FontTagData::featureTagToId(featureTag);
  uint32_t featureInfoIndex = blMin<uint32_t>(featureId, FontTagData::kFeatureIdCount);
  const FontTagData::FeatureInfo& featureInfo = FontTagData::featureInfoTable[featureInfoIndex];

  return getTagValue<kSSO>(self, featureTag, featureInfo.enabledByDefault) > 0u;
}

} // {FontFeatureSettingsInternal}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_FONTFEATURESETTINGS_P_H_INCLUDED
