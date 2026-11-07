// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_COVERAGE_SET_P_H_INCLUDED
#define BLEND2D_CORE_COVERAGE_SET_P_H_INCLUDED

#include <blend2d/axl/commons.h>
#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/coverage_set.h>
#include <blend2d/core/object_layer_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl::CoverageSetDetail {

enum class PageIndex : size_t {};

//! Whole Unicode code-point space is `[0, kMaxSize)` (1114112 characters).
//!
//! Character equal or larger than `kMaxSize` cannot be stored in the bit-set.
static constexpr uint32_t kMaxSize = 0x110000u;
//! A size of a single page that is used by the container to manage either master-planes or sub-planes.
static constexpr uint32_t kPageSize = 128u;

//! A master-plane covers `kMPBitCount` consecutive code-points (32768).
static constexpr uint32_t kMPBitCount = 0x8000u;
//! A bit-mask derived from \ref kMPBitCount.
static constexpr uint32_t kMPBitMask = kMPBitCount - 1u;
//! Shift to convert a code-point to a master-plane index.
static constexpr uint32_t kMPBitShift = axl::ctz_const(kMPBitCount);
//! Number of master-planes required to cover the whole Unicode space (34).
static constexpr uint32_t kMPCount = kMaxSize / kMPBitCount;

//! A sub-plane covers 1024 consecutive code-points.
static constexpr uint32_t kSPBitCount = kPageSize * 8u;
//! A bit-mask derived from \ref kSPBitCount.
static constexpr uint32_t kSPBitMask = kSPBitCount - 1u;

//! Shift to convert a code-point to a sub-plane index.
static constexpr uint32_t kSPShift = axl::ctz_const(kSPBitCount);
//! Shift to convert a code-point to a sub-plane mask.
static constexpr uint32_t kSPMask = (kMPBitCount >> kSPShift) - 1u;
//! Number of sub-planes for each master-plane (32).
static constexpr uint32_t kSPPerMP = kMPBitCount / kSPBitCount;

static constexpr uint32_t kBWCount = kSPBitCount / axl::bit_size_of<BLBitWord>;
static constexpr uint32_t kBWMask = kBWCount - 1u;

//! Maximum number of pages the container can possibly use (1122):
//!  - 1 page for each master-plane (kMPCount)
//!  - 32 sub-plane pages for each master-plane (kSPPerMP * kMPCount)
static constexpr uint32_t kMaxPageCapacity = kMPCount + kSPPerMP * kMPCount;

//! Master-plane or sub-plane start of a special page_index. Use `page_index >= kPageSpecial` to check for a special page.
static constexpr uint16_t kPageSpecial = 0x0800u;
//! Master-plane or sub-plane special page index that describes a zero plane (all bits are 0 in the whole master/sub-plane).
static constexpr uint16_t kPageZero = kPageSpecial + 0u;
//! Master-plane or sub-plane special page index that describes a full plane (all bits are 1 in the whole master/sub-plane).
static constexpr uint16_t kPageFull = kPageSpecial + 1u;

//! Unassigned free slot offset.
static constexpr uint16_t kUnassignedPage = 0xFFFFu;

static bool BL_INLINE_NODEBUG is_page_special(PageIndex page_index) noexcept { return (static_cast<size_t>(page_index) & kPageSpecial) != 0u; }
static bool BL_INLINE_NODEBUG is_page_zero(PageIndex page_index) noexcept { return static_cast<size_t>(page_index) == kPageZero; }
static bool BL_INLINE_NODEBUG is_page_full(PageIndex page_index) noexcept { return static_cast<size_t>(page_index) == kPageFull; }

using Layout = BLCoverageSet::Layout;

struct MasterPlane {
  //! Page index and HIGH 4 bits of each sub-plane cardinality value (0xFFF0 = Page offset, 0x000F = HI cardinality bits).
  uint16_t sp_page_hi[kSPPerMP];
  //! LOW 8 bits of each sub-plane cardinality value.
  uint8_t sp_card_lo[kSPPerMP];
  //! Bit-mask of dense planes in this master-plane.
  uint32_t sp_dense_mask;
  //! Bit-mask of filled planes in this master-plane - cannot collide with `sp_dense_mask` as filled planes are not
  //! dense - they use special offset instead and occupy no page.
  uint32_t sp_filled_mask;
  //! Master-plane cardinality - sum of cardinalities of each sub-plane.
  uint32_t mp_cardinality;
  //! Unused bytes at the moment.
  uint8_t unused[20];
};

struct SubPlane {
  BLBitWord bits[kBWCount];
};

struct FreePlane {
  uint16_t next_page;
  uint16_t reserved[64 - 1];
};

} // {bl::CoverageSetDetail}

namespace bl {

template<>
struct ObjectDispatchInfo<BLCoverageSetCore> {
  using ImplType = BLCoverageSetImpl;

  static constexpr bool kHasInitSSO = false;
  static constexpr size_t kImplDataOffset = 0;

  static BL_INLINE BLResult free_object(BLCoverageSetCore* self, ImplType* impl, ObjectLayer::MetaByte meta_byte) noexcept {
    axl::maybe_unused(self);
    return dealloc_impl(impl, meta_byte);
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_COVERAGE_SET_P_H_INCLUDED
