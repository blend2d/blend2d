// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "fonttagdatainfo_p.h"

namespace BLFontTagData {

struct FeatureInfoTableGen {
  static constexpr FeatureInfo value(size_t id) noexcept {
    return FeatureInfo {
      // Enabled by default:
      uint8_t(id == uint32_t(BLFontTagData::FeatureId::kCALT) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kCLIG) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kCPSP) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kKERN) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kLIGA) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kOPBD) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kRVRN) ? 1u :
              0u),

      // User control:
      uint8_t(id == uint32_t(BLFontTagData::FeatureId::kAALT) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kAFRC) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kC2PC) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kC2SC) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kCALT) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kCASE) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kCHWS) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kCLIG) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kCPCT) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kCPSP) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kCSWH) ? 1u :
              id >= uint32_t(BLFontTagData::FeatureId::kCV01) && id <= uint32_t(BLFontTagData::FeatureId::kCV99) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kDLIG) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kDNOM) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kEXPT) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kFALT) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kFRAC) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kFWID) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kHALT) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kHIST) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kHKNA) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kHLIG) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kHNGL) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kHOJO) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kHWID) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kJALT) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kJP04) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kJP78) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kJP83) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kJP90) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kKERN) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kLFBD) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kLIGA) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kLNUM) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kMGRK) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kNALT) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kNLCK) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kONUM) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kOPBD) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kORDN) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kORNM) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kPALT) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kPCAP) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kPKNA) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kPNUM) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kPWID) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kQWID) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kRAND) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kRTBD) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kRUBY) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kSALT) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kSINF) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kSMCP) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kSMPL) ? 1u :
              id >= uint32_t(BLFontTagData::FeatureId::kSS01) && id <= uint32_t(BLFontTagData::FeatureId::kSS20) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kSUBS) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kSUPS) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kSWSH) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kTITL) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kTNAM) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kTNUM) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kTRAD) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kTWID) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kUNIC) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kVHAL) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kVKNA) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kVKRN) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kVPAL) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kVRT2) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kVRTR) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kZERO) ? 1u :
              0u),

      // Feature bit id:
      uint8_t(id == uint32_t(BLFontTagData::FeatureId::kCASE) ? 0u :
              id == uint32_t(BLFontTagData::FeatureId::kCLIG) ? 1u :
              id == uint32_t(BLFontTagData::FeatureId::kCPCT) ? 2u :
              id == uint32_t(BLFontTagData::FeatureId::kCPSP) ? 3u :
              id == uint32_t(BLFontTagData::FeatureId::kDLIG) ? 4u :
              id == uint32_t(BLFontTagData::FeatureId::kDNOM) ? 5u :
              id == uint32_t(BLFontTagData::FeatureId::kEXPT) ? 6u :
              id == uint32_t(BLFontTagData::FeatureId::kFALT) ? 7u :
              id == uint32_t(BLFontTagData::FeatureId::kFRAC) ? 8u :
              id == uint32_t(BLFontTagData::FeatureId::kFWID) ? 9u :
              id == uint32_t(BLFontTagData::FeatureId::kHALT) ? 10u :
              id == uint32_t(BLFontTagData::FeatureId::kHIST) ? 11u :
              id == uint32_t(BLFontTagData::FeatureId::kHWID) ? 12u :
              id == uint32_t(BLFontTagData::FeatureId::kJALT) ? 13u :
              id == uint32_t(BLFontTagData::FeatureId::kKERN) ? 14u :
              id == uint32_t(BLFontTagData::FeatureId::kLIGA) ? 15u :
              id == uint32_t(BLFontTagData::FeatureId::kLNUM) ? 16u :
              id == uint32_t(BLFontTagData::FeatureId::kONUM) ? 17u :
              id == uint32_t(BLFontTagData::FeatureId::kORDN) ? 18u :
              id == uint32_t(BLFontTagData::FeatureId::kPALT) ? 19u :
              id == uint32_t(BLFontTagData::FeatureId::kPCAP) ? 20u :
              id == uint32_t(BLFontTagData::FeatureId::kRUBY) ? 21u :
              id == uint32_t(BLFontTagData::FeatureId::kSMCP) ? 22u :
              id == uint32_t(BLFontTagData::FeatureId::kSUBS) ? 23u :
              id == uint32_t(BLFontTagData::FeatureId::kSUPS) ? 24u :
              id == uint32_t(BLFontTagData::FeatureId::kTITL) ? 25u :
              id == uint32_t(BLFontTagData::FeatureId::kTNAM) ? 26u :
              id == uint32_t(BLFontTagData::FeatureId::kTNUM) ? 27u :
              id == uint32_t(BLFontTagData::FeatureId::kUNIC) ? 28u :
              id == uint32_t(BLFontTagData::FeatureId::kVALT) ? 29u :
              id == uint32_t(BLFontTagData::FeatureId::kVKRN) ? 30u :
              id == uint32_t(BLFontTagData::FeatureId::kZERO) ? 31u :
              unsigned(kInvalidFeatureBitId))
    };
  }
};

BL_CONSTEXPR_TABLE(featureInfoTable, FeatureInfoTableGen, FeatureInfo, kFeatureIdCount + 1u);

// This is a reverse table that must match feature bit its defined by `featureInfoTable`.
const uint8_t featureBitIdToFeatureIdTable[32] = {
  uint8_t(FeatureId::kCASE),
  uint8_t(FeatureId::kCLIG),
  uint8_t(FeatureId::kCPCT),
  uint8_t(FeatureId::kCPSP),
  uint8_t(FeatureId::kDLIG),
  uint8_t(FeatureId::kDNOM),
  uint8_t(FeatureId::kEXPT),
  uint8_t(FeatureId::kFALT),
  uint8_t(FeatureId::kFRAC),
  uint8_t(FeatureId::kFWID),
  uint8_t(FeatureId::kHALT),
  uint8_t(FeatureId::kHIST),
  uint8_t(FeatureId::kHWID),
  uint8_t(FeatureId::kJALT),
  uint8_t(FeatureId::kKERN),
  uint8_t(FeatureId::kLIGA),
  uint8_t(FeatureId::kLNUM),
  uint8_t(FeatureId::kONUM),
  uint8_t(FeatureId::kORDN),
  uint8_t(FeatureId::kPALT),
  uint8_t(FeatureId::kPCAP),
  uint8_t(FeatureId::kRUBY),
  uint8_t(FeatureId::kSMCP),
  uint8_t(FeatureId::kSUBS),
  uint8_t(FeatureId::kSUPS),
  uint8_t(FeatureId::kTITL),
  uint8_t(FeatureId::kTNAM),
  uint8_t(FeatureId::kTNUM),
  uint8_t(FeatureId::kUNIC),
  uint8_t(FeatureId::kVALT),
  uint8_t(FeatureId::kVKRN),
  uint8_t(FeatureId::kZERO)
};

} // {BLFontTagData}
