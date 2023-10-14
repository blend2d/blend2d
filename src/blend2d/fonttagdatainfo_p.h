// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTTAGDATAINFO_P_H_INCLUDED
#define BLEND2D_FONTTAGDATAINFO_P_H_INCLUDED

#include "api-internal_p.h"
#include "fonttagdataids_p.h"
#include "support/lookuptable_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \namespace bl::FontTagData
//! Namespace that provides private information regarding font features.

namespace bl {
namespace FontTagData {

static constexpr uint32_t kInvalidFeatureBitId = 63;

struct FeatureInfo {
  uint8_t enabledByDefault : 1;
  uint8_t userControl : 1;
  uint8_t bitId : 6;

  BL_INLINE bool hasBitId() const noexcept { return bitId != kInvalidFeatureBitId; };
};

extern const LookupTable<FeatureInfo, kFeatureIdCount + 1u> featureInfoTable;

extern const uint8_t featureBitIdToFeatureIdTable[32];

static BL_INLINE FeatureId featureBitIdToFeatureId(uint32_t bitId) noexcept {
  BL_ASSERT(bitId < 32u);
  return FeatureId(featureBitIdToFeatureIdTable[bitId]);
}

static BL_INLINE uint32_t featureIdToFeatureBitId(FeatureId featureId) noexcept {
  BL_ASSERT(uint32_t(featureId) < kFeatureIdCount);
  return featureInfoTable[size_t(featureId)].bitId;
}

} // {FontTagData}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_FONTTAGDATAINFO_P_H_INCLUDED
