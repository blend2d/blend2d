// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTFEATURESETTINGS_P_H_INCLUDED
#define BLEND2D_FONTFEATURESETTINGS_P_H_INCLUDED

#include "api-internal_p.h"
#include "fontfeaturesettings.h"
#include "fonttagdata_p.h"
#include "support/algorithm_p.h"
#include "support/bitops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLFontFeatureSettingsPrivate {

using BLFontTagData::FeatureInfo;
using FatBitOps = BLParametrizedBitOps<BLBitOrder::kLSB, uint32_t>;

static constexpr uint32_t kSSOFatFeatureCount = 4;
static constexpr uint32_t kSSOFatFeatureTagBitCount = 8;
static constexpr uint32_t kSSOFatFeatureTagBitMask = (1u << kSSOFatFeatureTagBitCount) - 1u;;
static constexpr uint32_t kSSOFatFeatureValueBitCount = 4;
static constexpr uint32_t kSSOFatFeatureValueBitMask = (1u << kSSOFatFeatureValueBitCount) - 1u;

// 'zero' is used by SSO, thus it can never be used in fat feature data.
static constexpr uint32_t kSSOInvalidFatFeatureId = 0xFFu;
// 32-bit pattern that is used to initialize SSO storage.
static constexpr uint32_t kSSOInvalidFatFeaturePattern = 0xFFFFFFFFu;

static BL_INLINE BLFontFeatureSettingsImpl* getImpl(const BLFontFeatureSettingsCore* self) noexcept {
  return static_cast<BLFontFeatureSettingsImpl*>(self->_d.impl);
}

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

  uint32_t featureId = BLFontTagData::featureTagToId(featureTag);
  if (featureId == BLFontTagData::kInvalidId)
    return notFoundValue;

  BLFontTagData::FeatureInfo featureInfo = BLFontTagData::featureInfoTable[featureId];
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
  size_t index = BLAlgorithm::lowerBound(data, selfI->size, featureTag, [](const BLFontFeatureItem& item, uint32_t tag) noexcept { return item.tag < tag; });

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
  uint32_t featureId = BLFontTagData::featureTagToId(featureTag);
  uint32_t featureInfoIndex = blMin<uint32_t>(featureId, BLFontTagData::kFeatureIdCount);
  const BLFontTagData::FeatureInfo& featureInfo = BLFontTagData::featureInfoTable[featureInfoIndex];

  return getTagValue<kSSO>(self, featureTag, featureInfo.enabledByDefault) > 0u;
}

BLResult freeImpl(BLFontFeatureSettingsImpl* impl, BLObjectInfo info) noexcept;

} // {BLFontFeatureSettingsPrivate}

//! \}
//! \endcond

#endif // BLEND2D_FONTFEATURESETTINGS_P_H_INCLUDED
