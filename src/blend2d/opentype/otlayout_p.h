// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_OPENTYPE_OTLAYOUT_P_H_INCLUDED
#define BLEND2D_OPENTYPE_OTLAYOUT_P_H_INCLUDED

#include "../array.h"
#include "../glyphbuffer_p.h"
#include "../opentype/otcore_p.h"
#include "../support/intops_p.h"
#include "../support/ptrops_p.h"
#include "../threading/atomic_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_opentype_impl
//! \{

namespace bl {
namespace OpenType {

//! Kind of a lookup (either GPOS or GSUB).
enum class LookupKind {
  kGSUB,
  kGPOS
};

//! GSUB lookup type & format combined.
enum class GSubLookupAndFormat : uint8_t {
  kNone = 0,

  kType1Format1,
  kType1Format2,
  kType2Format1,
  kType3Format1,
  kType4Format1,
  kType5Format1,
  kType5Format2,
  kType5Format3,
  kType6Format1,
  kType6Format2,
  kType6Format3,
  kType8Format1,

  kMaxValue = kType8Format1
};

//! GPOS lookup type & format combined.
enum class GPosLookupAndFormat : uint8_t {
  kNone = 0,

  kType1Format1,
  kType1Format2,
  kType2Format1,
  kType2Format2,
  kType3Format1,
  kType4Format1,
  kType5Format1,
  kType6Format1,
  kType7Format1,
  kType7Format2,
  kType7Format3,
  kType8Format1,
  kType8Format2,
  kType8Format3,

  kMaxValue = kType8Format3
};

struct DebugSink {
  BLDebugMessageSinkFunc _sink;
  void* _userData;

  BL_INLINE void init(BLDebugMessageSinkFunc sink, void* userData) noexcept {
    _sink = sink;
    _userData = userData;
  }

  BL_INLINE bool enabled() const noexcept { return _sink != nullptr; }
  BL_INLINE void message(const BLString& s) const noexcept { _sink(s.data(), s.size(), _userData); }
};

struct GSubGPosLookupInfo {
  enum : uint32_t {
    kTypeCount = 10,
    kFormatAndIdCount = 20
  };

  //! Structure that describes a lookup of a specific LookupType of any format.
  struct TypeInfo {
    //! Number of formats.
    uint8_t formatCount;
    //! First lookup of format 1, see \ref GSubLookupAndFormat and \ref GPosLookupAndFormat.
    uint8_t typeAndFormat;
  };

  //! Structure that describes a lookup, see \ref GSubLookupAndFormat and \ref GPosLookupAndFormat.
  struct TypeFormatInfo {
    uint8_t type : 4;
    uint8_t format : 4;
    uint8_t headerSize;
  };

  //! Maximum value of LookupType (inclusive).
  uint8_t lookupMaxValue;
  //! "Extension" lookup type.
  uint8_t extensionType;

  //! Lookup type to LookupTypeAndFormat mapping.
  TypeInfo typeInfo[kTypeCount];
  //! Information of the lookup type & format, see \ref GSubLookupAndFormat and \ref GPosLookupAndFormat.
  TypeFormatInfo lookupInfo[kFormatAndIdCount];
};

//! Data stored in `OTFaceImpl` related to OpenType advanced layout features.
class LayoutData {
public:
  //! \name Structs
  //! \{

  union LookupStatusBits {
    struct {
      uint32_t analyzed;
      uint32_t valid;
    };
    uint64_t composed;

    static BL_INLINE LookupStatusBits make(uint32_t analyzed, uint32_t valid) noexcept {
      return LookupStatusBits{{analyzed, valid}};
    }

    static BL_INLINE LookupStatusBits makeComposed(uint64_t composed) noexcept {
      LookupStatusBits out;
      out.composed = composed;
      return out;
    }
  };

  struct LookupEntry {
    uint8_t type;
    uint8_t format;
    uint16_t flags;
    uint32_t offset;
  };

  struct TableRef {
    uint32_t format : 4;
    uint32_t offset : 28;

    BL_INLINE void reset(uint32_t format_, uint32_t offset_) noexcept {
      format = format_ & 0xFu;
      offset = offset_ & 0x0FFFFFFFu;
    }
  };

  struct GDef {
    TableRef glyphClassDef;
    TableRef markAttachClassDef;

    uint16_t attachListOffset;
    uint16_t ligCaretListOffset;
    uint16_t markGlyphSetsDefOffset;
    uint32_t itemVarStoreOffset;
  };

  struct GSubGPos {
    uint16_t scriptListOffset;
    uint16_t featureListOffset;
    uint16_t lookupListOffset;
    uint16_t featureCount;
    uint16_t lookupCount;
    uint16_t lookupStatusDataSize;
    uint16_t lookupStatusDataOffset;
  };

  //! \}

  //! \name Members
  //! \{

  RawTable tables[3];
  GDef gdef;
  GSubGPos kinds[2];
  LookupStatusBits* _lookupStatusBits;

  //! \}

  //! \name Construction & Destruction
  //! \{

  BL_INLINE LayoutData() noexcept
    : tables{},
      gdef{},
      kinds{},
      _lookupStatusBits(nullptr) {}

  BL_INLINE ~LayoutData() noexcept {
    if (_lookupStatusBits)
      free(_lookupStatusBits);
  }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE GSubGPos& byKind(LookupKind lookupKind) noexcept { return kinds[size_t(lookupKind)]; }
  BL_INLINE const GSubGPos& byKind(LookupKind lookupKind) const noexcept { return kinds[size_t(lookupKind)]; }

  BL_INLINE GSubGPos& gsub() noexcept { return byKind(LookupKind::kGSUB); }
  BL_INLINE GSubGPos& gpos() noexcept { return byKind(LookupKind::kGPOS); }

  BL_INLINE const GSubGPos& gsub() const noexcept { return byKind(LookupKind::kGSUB); }
  BL_INLINE const GSubGPos& gpos() const noexcept { return byKind(LookupKind::kGPOS); }

  //! \}

  //! \name Lookup Status Bits
  //! \{

  //! Allocates 4 lookup bit arrays for both GSUB/GPOS lookups each having analyzed/valid bit per lookup.
  BL_INLINE BLResult allocateLookupStatusBits() noexcept {
    uint32_t gsubLookupCount = gsub().lookupCount;
    uint32_t gposLookupCount = gpos().lookupCount;

    uint32_t gsubLookupStatusDataSize = (gsubLookupCount + 31) / 32u;
    uint32_t gposLookupStatusDataSize = (gposLookupCount + 31) / 32u;
    uint32_t totalLookupStatusDataSize = gsubLookupStatusDataSize + gposLookupStatusDataSize;

    if (!totalLookupStatusDataSize)
      return BL_SUCCESS;

    LookupStatusBits* lookupStatusBits = static_cast<LookupStatusBits*>(calloc(totalLookupStatusDataSize, sizeof(LookupStatusBits)));
    if (BL_UNLIKELY(!lookupStatusBits))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    _lookupStatusBits = lookupStatusBits;
    gsub().lookupStatusDataSize = uint16_t(gsubLookupStatusDataSize);
    gsub().lookupStatusDataOffset = uint16_t(0);
    gpos().lookupStatusDataSize = uint16_t(gposLookupStatusDataSize);
    gpos().lookupStatusDataOffset = uint16_t(gsubLookupStatusDataSize);
    return BL_SUCCESS;
  }

  //! Returns a bit array representing analyzed lookup bits of either GSUB or GPOS lookup.
  BL_INLINE LookupStatusBits* _lookupStatusBitsOf(LookupKind lookupKind) const noexcept {
    return _lookupStatusBits + kinds[size_t(lookupKind)].lookupStatusDataOffset;
  }

  BL_INLINE LookupStatusBits getLookupStatusBits(LookupKind lookupKind, uint32_t index) const noexcept {
    BL_ASSERT(index < kinds[size_t(lookupKind)].lookupStatusDataSize);
    const LookupStatusBits& bits = _lookupStatusBitsOf(lookupKind)[index];

#if BL_TARGET_ARCH_BITS >= 64
    uint64_t composed = blAtomicFetchRelaxed(&bits.composed);
    return LookupStatusBits::makeComposed(composed);
#else
    uint32_t analyzed = blAtomicFetchRelaxed(&bits.analyzed);
    uint32_t valid = blAtomicFetchRelaxed(&bits.valid);
    return LookupStatusBits::make(analyzed, valid);
#endif
  }

  //! Commit means combining the given `statusBits` with status bits already present.
  //!
  //! As validation goes on, it keeps committing the validated lookups.
  BL_INLINE LookupStatusBits commitLookupStatusBits(LookupKind lookupKind, uint32_t index, LookupStatusBits statusBits) const noexcept {
    BL_ASSERT(index < kinds[size_t(lookupKind)].lookupStatusDataSize);
    LookupStatusBits* statusData = _lookupStatusBitsOf(lookupKind);

#if BL_TARGET_ARCH_BITS >= 64
    // This is designed to make it much easier on 64-bit targets as we just issue a single write for both
    // 32-bit words.
    uint64_t existingComposed = blAtomicFetchOrStrong(&statusData[index].composed, statusBits.composed);
    return LookupStatusBits::makeComposed(statusBits.composed | existingComposed);
#else
    // Valid bits must be committed first, then analyzed bits. The reason is that these are two separate
    // atomic operations and the code always checks for analyzed bits first - once an analyzed lookup bit
    // is set, its validated lookup bit must be valid. This means that another thread can validate the
    // same lookup concurrently, but that's ok as they would reach the same result.
    uint32_t existingValid = blAtomicFetchOrStrong(&statusData[index].valid, statusBits.valid);
    uint32_t existingAnalyzed = blAtomicFetchOrStrong(&statusData[index].analyzed, statusBits.analyzed);
    return LookupStatusBits::make(statusBits.analyzed | existingAnalyzed, statusBits.valid | existingValid);
#endif
  }

  //! \}
};

namespace LayoutImpl {

BLResult calculateGSubPlan(const OTFaceImpl* faceI, const BLFontFeatureSettings& settings, BLBitArrayCore* plan) noexcept;
BLResult calculateGPosPlan(const OTFaceImpl* faceI, const BLFontFeatureSettings& settings, BLBitArrayCore* plan) noexcept;
BLResult init(OTFaceImpl* faceI, OTFaceTables& tables) noexcept;

} // {LayoutImpl}

} // {OpenType}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_OTLAYOUT_P_H_INCLUDED
