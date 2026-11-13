// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_OPENTYPE_OT_LAYOUT_P_H_INCLUDED
#define BLEND2D_OPENTYPE_OT_LAYOUT_P_H_INCLUDED

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_alloc.h>
#include <blend2d/core/array.h>
#include <blend2d/core/glyph_buffer_p.h>
#include <blend2d/opentype/ot_core_p.h>
#include <blend2d/threading/atomic_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_opentype_impl
//! \{

namespace bl::OpenType {

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
  void* _user_data;

  BL_INLINE void init(BLDebugMessageSinkFunc sink, void* user_data) noexcept {
    _sink = sink;
    _user_data = user_data;
  }

  BL_INLINE bool enabled() const noexcept { return _sink != nullptr; }
  BL_INLINE void message(const BLString& s) const noexcept { _sink(s.data(), s.size(), _user_data); }
};

struct GSubGPosLookupInfo {
  enum : uint32_t {
    kTypeCount = 10,
    kFormatAndIdCount = 20
  };

  //! Structure that describes a lookup of a specific LookupType of any format.
  struct TypeInfo {
    //! Number of formats.
    uint8_t format_count;
    //! First lookup of format 1, see \ref GSubLookupAndFormat and \ref GPosLookupAndFormat.
    uint8_t type_and_format;
  };

  //! Structure that describes a lookup, see \ref GSubLookupAndFormat and \ref GPosLookupAndFormat.
  struct TypeFormatInfo {
    uint8_t type : 4;
    uint8_t format : 4;
    uint8_t header_size;
  };

  //! Maximum value of LookupType (inclusive).
  uint8_t lookup_max_value;
  //! "Extension" lookup type.
  uint8_t extension_type;

  //! Lookup type to LookupTypeAndFormat mapping.
  TypeInfo type_info[kTypeCount];
  //! Information of the lookup type & format, see \ref GSubLookupAndFormat and \ref GPosLookupAndFormat.
  TypeFormatInfo lookup_info[kFormatAndIdCount];
};

//! Data stored in `OTFaceImpl` related to OpenType advanced layout features.
class LayoutData {
public:
  //! \name Structs
  //! \{

  struct LookupStatusBits {
    BLBitWord analyzed;
    BLBitWord valid;
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
    TableRef glyph_class_def;
    TableRef mark_attach_class_def;

    uint16_t attach_list_offset;
    uint16_t lig_caret_list_offset;
    uint16_t mark_glyph_sets_def_offset;
    uint32_t item_var_store_offset;
  };

  struct GSubGPos {
    uint16_t script_list_offset;
    uint16_t feature_list_offset;
    uint16_t lookup_list_offset;
    uint16_t feature_count;
    uint16_t lookup_count;
    uint16_t lookup_status_data_size;
    uint16_t lookup_status_data_offset;
  };

  //! \}

  //! \name Members
  //! \{

  RawTable tables[3];
  GDef gdef;
  GSubGPos kinds[2];
  LookupStatusBits* _lookup_status_bits;

  //! \}

  //! \name Construction & Destruction
  //! \{

  BL_INLINE LayoutData() noexcept
    : tables{},
      gdef{},
      kinds{},
      _lookup_status_bits(nullptr) {}

  BL_INLINE ~LayoutData() noexcept {
    if (_lookup_status_bits) {
      axl::mem_free(_lookup_status_bits);
    }
  }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE GSubGPos& by_kind(LookupKind lookup_kind) noexcept { return kinds[size_t(lookup_kind)]; }
  BL_INLINE const GSubGPos& by_kind(LookupKind lookup_kind) const noexcept { return kinds[size_t(lookup_kind)]; }

  BL_INLINE GSubGPos& gsub() noexcept { return by_kind(LookupKind::kGSUB); }
  BL_INLINE GSubGPos& gpos() noexcept { return by_kind(LookupKind::kGPOS); }

  BL_INLINE const GSubGPos& gsub() const noexcept { return by_kind(LookupKind::kGSUB); }
  BL_INLINE const GSubGPos& gpos() const noexcept { return by_kind(LookupKind::kGPOS); }

  //! \}

  //! \name Lookup Status Bits
  //! \{

  //! Allocates 4 lookup bit arrays for both GSUB/GPOS lookups each having analyzed/valid bit per lookup.
  BL_INLINE BLResult allocate_lookup_status_bits() noexcept {
    uint32_t gsub_lookup_count = gsub().lookup_count;
    uint32_t gpos_lookup_count = gpos().lookup_count;

    uint32_t gsub_lookup_status_data_size = (gsub_lookup_count + 31) / 32u;
    uint32_t gpos_lookup_status_data_size = (gpos_lookup_count + 31) / 32u;
    uint32_t total_lookup_status_data_size = gsub_lookup_status_data_size + gpos_lookup_status_data_size;

    if (!total_lookup_status_data_size)
      return BL_SUCCESS;

    LookupStatusBits* lookup_status_bits = axl::mem_alloc_zeroed<LookupStatusBits>(total_lookup_status_data_size * sizeof(LookupStatusBits));
    if (BL_UNLIKELY(!lookup_status_bits))
      return bl_make_error(BL_ERROR_OUT_OF_MEMORY);

    _lookup_status_bits = lookup_status_bits;
    gsub().lookup_status_data_size = uint16_t(gsub_lookup_status_data_size);
    gsub().lookup_status_data_offset = uint16_t(0);
    gpos().lookup_status_data_size = uint16_t(gpos_lookup_status_data_size);
    gpos().lookup_status_data_offset = uint16_t(gsub_lookup_status_data_size);
    return BL_SUCCESS;
  }

  //! Returns a bit array representing analyzed lookup bits of either GSUB or GPOS lookup.
  BL_INLINE LookupStatusBits* _lookup_status_bits_of(LookupKind lookup_kind) const noexcept {
    return _lookup_status_bits + kinds[size_t(lookup_kind)].lookup_status_data_offset;
  }

  BL_INLINE LookupStatusBits get_lookup_status_bits(LookupKind lookup_kind, size_t index) const noexcept {
    BL_ASSERT(index < kinds[size_t(lookup_kind)].lookup_status_data_size);
    const LookupStatusBits& bits = _lookup_status_bits_of(lookup_kind)[index];

    BLBitWord analyzed = Atomic::fetch_relaxed(bits.analyzed);
    BLBitWord valid = Atomic::fetch_relaxed(bits.valid);
    return LookupStatusBits{analyzed, valid};
  }

  //! Commit means combining the given `status_bits` with status bits already present.
  //!
  //! As validation goes on, it keeps committing the validated lookups.
  BL_INLINE LookupStatusBits commit_lookup_status_bits(LookupKind lookup_kind, size_t index, LookupStatusBits status_bits) const noexcept {
    BL_ASSERT(index < kinds[size_t(lookup_kind)].lookup_status_data_size);
    LookupStatusBits* status_data = _lookup_status_bits_of(lookup_kind);

    // Valid bits must be committed first, then analyzed bits. The reason is that these are two separate
    // atomic operations and the code always checks for analyzed bits first - once an analyzed lookup bit
    // is set, its validated lookup bit must be valid. This means that another thread can validate the
    // same lookup concurrently, but that's ok as they would reach the same result.
    BLBitWord existing_valid = Atomic::fetch_or_acq_rel(status_data[index].valid, status_bits.valid);
    BLBitWord existing_analyzed = Atomic::fetch_or_acq_rel(status_data[index].analyzed, status_bits.analyzed);
    return LookupStatusBits{status_bits.analyzed | existing_analyzed, status_bits.valid | existing_valid};
  }

  //! \}
};

namespace LayoutImpl {

BLResult calculate_gsub_plan(const OTFaceImpl* ot_face_impl, const BLFontFeatureSettings& settings, BLBitArrayCore* plan) noexcept;
BLResult calculate_gpos_plan(const OTFaceImpl* ot_face_impl, const BLFontFeatureSettings& settings, BLBitArrayCore* plan) noexcept;
BLResult init(OTFaceImpl* ot_face_impl, OTFaceTables& tables) noexcept;

} // {LayoutImpl}

} // {bl::OpenType}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_OT_LAYOUT_P_H_INCLUDED
